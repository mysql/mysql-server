/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: hash_stat.c,v 11.48 2002/08/06 06:11:28 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"

static int __ham_stat_callback __P((DB *, PAGE *, void *, int *));

/*
 * __ham_stat --
 *	Gather/print the hash statistics
 *
 * PUBLIC: int __ham_stat __P((DB *, void *, u_int32_t));
 */
int
__ham_stat(dbp, spp, flags)
	DB *dbp;
	void *spp;
	u_int32_t flags;
{
	DBC *dbc;
	DB_ENV *dbenv;
	DB_HASH_STAT *sp;
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *hcp;
	PAGE *h;
	db_pgno_t pgno;
	int ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->stat");

	mpf = dbp->mpf;
	sp = NULL;

	/* Check for invalid flags. */
	if ((ret = __db_statchk(dbp, flags)) != 0)
		return (ret);

	if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0)
		return (ret);
	hcp = (HASH_CURSOR *)dbc->internal;

	if ((ret = __ham_get_meta(dbc)) != 0)
		goto err;

	/* Allocate and clear the structure. */
	if ((ret = __os_umalloc(dbenv, sizeof(*sp), &sp)) != 0)
		goto err;
	memset(sp, 0, sizeof(*sp));
	/* Copy the fields that we have. */
	sp->hash_nkeys = hcp->hdr->dbmeta.key_count;
	sp->hash_ndata = hcp->hdr->dbmeta.record_count;
	sp->hash_pagesize = dbp->pgsize;
	sp->hash_buckets = hcp->hdr->max_bucket + 1;
	sp->hash_magic = hcp->hdr->dbmeta.magic;
	sp->hash_version = hcp->hdr->dbmeta.version;
	sp->hash_metaflags = hcp->hdr->dbmeta.flags;
	sp->hash_ffactor = hcp->hdr->ffactor;

	if (flags == DB_FAST_STAT || flags == DB_CACHED_COUNTS)
		goto done;

	/* Walk the free list, counting pages. */
	for (sp->hash_free = 0, pgno = hcp->hdr->dbmeta.free;
	    pgno != PGNO_INVALID;) {
		++sp->hash_free;

		if ((ret = mpf->get(mpf, &pgno, 0, &h)) != 0)
			goto err;

		pgno = h->next_pgno;
		(void)mpf->put(mpf, h, 0);
	}

	/* Now traverse the rest of the table. */
	sp->hash_nkeys = 0;
	sp->hash_ndata = 0;
	if ((ret = __ham_traverse(dbc,
	    DB_LOCK_READ, __ham_stat_callback, sp, 0)) != 0)
		goto err;

	if (!F_ISSET(dbp, DB_AM_RDONLY)) {
		if ((ret = __ham_dirty_meta(dbc)) != 0)
			goto err;
		hcp->hdr->dbmeta.key_count = sp->hash_nkeys;
		hcp->hdr->dbmeta.record_count = sp->hash_ndata;
	}

done:
	if ((ret = __ham_release_meta(dbc)) != 0)
		goto err;
	if ((ret = dbc->c_close(dbc)) != 0)
		goto err;

	*(DB_HASH_STAT **)spp = sp;
	return (0);

err:	if (sp != NULL)
		__os_ufree(dbenv, sp);
	if (hcp->hdr != NULL)
		(void)__ham_release_meta(dbc);
	(void)dbc->c_close(dbc);
	return (ret);

}

/*
 * __ham_traverse
 *	 Traverse an entire hash table.  We use the callback so that we
 * can use this both for stat collection and for deallocation.
 *
 * PUBLIC: int __ham_traverse __P((DBC *, db_lockmode_t,
 * PUBLIC:     int (*)(DB *, PAGE *, void *, int *), void *, int));
 */
int
__ham_traverse(dbc, mode, callback, cookie, look_past_max)
	DBC *dbc;
	db_lockmode_t mode;
	int (*callback) __P((DB *, PAGE *, void *, int *));
	void *cookie;
	int look_past_max;
{
	DB *dbp;
	DBC *opd;
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *hcp;
	HKEYDATA *hk;
	db_pgno_t pgno, opgno;
	int did_put, i, ret, t_ret;
	u_int32_t bucket, spares_entry;

	dbp = dbc->dbp;
	opd = NULL;
	mpf = dbp->mpf;
	hcp = (HASH_CURSOR *)dbc->internal;
	ret = 0;

	/*
	 * In a perfect world, we could simply read each page in the file
	 * and look at its page type to tally the information necessary.
	 * Unfortunately, the bucket locking that hash tables do to make
	 * locking easy, makes this a pain in the butt.  We have to traverse
	 * duplicate, overflow and big pages from the bucket so that we
	 * don't access anything that isn't properly locked.
	 *
	 */
	for (bucket = 0;; bucket++) {
		/*
		 * We put the loop exit condition check here, because
		 * it made for a really vile extended ?: that made SCO's
		 * compiler drop core.
		 *
		 * If look_past_max is not set, we can stop at max_bucket;
		 * if it is set, we need to include pages that are part of
		 * the current doubling but beyond the highest bucket we've
		 * split into, as well as pages from a "future" doubling
		 * that may have been created within an aborted
		 * transaction.  To do this, keep looping (and incrementing
		 * bucket) until the corresponding spares array entries
		 * cease to be defined.
		 */
		if (look_past_max) {
			spares_entry = __db_log2(bucket + 1);
			if (spares_entry >= NCACHED ||
			    hcp->hdr->spares[spares_entry] == 0)
				break;
		} else {
			if (bucket > hcp->hdr->max_bucket)
				break;
		}

		hcp->bucket = bucket;
		hcp->pgno = pgno = BUCKET_TO_PAGE(hcp, bucket);
		for (ret = __ham_get_cpage(dbc, mode); ret == 0;
		    ret = __ham_next_cpage(dbc, pgno, 0)) {

			/*
			 * If we are cleaning up pages past the max_bucket,
			 * then they may be on the free list and have their
			 * next pointers set, but the should be ignored.  In
			 * fact, we really ought to just skip anybody who is
			 * not a valid page.
			 */
			if (TYPE(hcp->page) == P_INVALID)
				break;
			pgno = NEXT_PGNO(hcp->page);

			/*
			 * Go through each item on the page checking for
			 * duplicates (in which case we have to count the
			 * duplicate pages) or big key/data items (in which
			 * case we have to count those pages).
			 */
			for (i = 0; i < NUM_ENT(hcp->page); i++) {
				hk = (HKEYDATA *)P_ENTRY(dbp, hcp->page, i);
				switch (HPAGE_PTYPE(hk)) {
				case H_OFFDUP:
					memcpy(&opgno, HOFFDUP_PGNO(hk),
					    sizeof(db_pgno_t));
					if ((ret = __db_c_newopd(dbc,
					    opgno, NULL, &opd)) != 0)
						return (ret);
					if ((ret = __bam_traverse(opd,
					    DB_LOCK_READ, opgno,
					    callback, cookie))
					    != 0)
						goto err;
					if ((ret = opd->c_close(opd)) != 0)
						return (ret);
					opd = NULL;
					break;
				case H_OFFPAGE:
					/*
					 * We are about to get a big page
					 * which will use the same spot that
					 * the current page uses, so we need
					 * to restore the current page before
					 * looking at it again.
					 */
					memcpy(&opgno, HOFFPAGE_PGNO(hk),
					    sizeof(db_pgno_t));
					if ((ret = __db_traverse_big(dbp,
					    opgno, callback, cookie)) != 0)
						goto err;
					break;
				case H_KEYDATA:
					break;
				}
			}

			/* Call the callback on main pages. */
			if ((ret = callback(dbp,
			    hcp->page, cookie, &did_put)) != 0)
				goto err;

			if (did_put)
				hcp->page = NULL;
			if (pgno == PGNO_INVALID)
				break;
		}
		if (ret != 0)
			goto err;

		if (STD_LOCKING(dbc))
			(void)dbp->dbenv->lock_put(dbp->dbenv, &hcp->lock);

		if (hcp->page != NULL) {
			if ((ret = mpf->put(mpf, hcp->page, 0)) != 0)
				return (ret);
			hcp->page = NULL;
		}

	}
err:	if (opd != NULL &&
	    (t_ret = opd->c_close(opd)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

static int
__ham_stat_callback(dbp, pagep, cookie, putp)
	DB *dbp;
	PAGE *pagep;
	void *cookie;
	int *putp;
{
	DB_HASH_STAT *sp;
	DB_BTREE_STAT bstat;
	db_indx_t indx, len, off, tlen, top;
	u_int8_t *hk;
	int ret;

	*putp = 0;
	sp = cookie;

	switch (pagep->type) {
	case P_INVALID:
		/*
		 * Hash pages may be wholly zeroed;  this is not a bug.
		 * Obviously such pages have no data, so we can just proceed.
		 */
		break;
	case P_HASH:
		/*
		 * We count the buckets and the overflow pages
		 * separately and tally their bytes separately
		 * as well.  We need to figure out if this page
		 * is a bucket.
		 */
		if (PREV_PGNO(pagep) == PGNO_INVALID)
			sp->hash_bfree += P_FREESPACE(dbp, pagep);
		else {
			sp->hash_overflows++;
			sp->hash_ovfl_free += P_FREESPACE(dbp, pagep);
		}
		top = NUM_ENT(pagep);
		/* Correct for on-page duplicates and deleted items. */
		for (indx = 0; indx < top; indx += P_INDX) {
			switch (*H_PAIRDATA(dbp, pagep, indx)) {
			case H_OFFDUP:
			case H_OFFPAGE:
				break;
			case H_KEYDATA:
				sp->hash_ndata++;
				break;
			case H_DUPLICATE:
				tlen = LEN_HDATA(dbp, pagep, 0, indx);
				hk = H_PAIRDATA(dbp, pagep, indx);
				for (off = 0; off < tlen;
				    off += len + 2 * sizeof (db_indx_t)) {
					sp->hash_ndata++;
					memcpy(&len,
					    HKEYDATA_DATA(hk)
					    + off, sizeof(db_indx_t));
				}
			}
		}
		sp->hash_nkeys += H_NUMPAIRS(pagep);
		break;
	case P_IBTREE:
	case P_IRECNO:
	case P_LBTREE:
	case P_LRECNO:
	case P_LDUP:
		/*
		 * These are all btree pages; get a correct
		 * cookie and call them.  Then add appropriate
		 * fields into our stat structure.
		 */
		memset(&bstat, 0, sizeof(bstat));
		bstat.bt_dup_pgfree = 0;
		bstat.bt_int_pgfree = 0;
		bstat.bt_leaf_pgfree = 0;
		bstat.bt_ndata = 0;
		if ((ret = __bam_stat_callback(dbp, pagep, &bstat, putp)) != 0)
			return (ret);
		sp->hash_dup++;
		sp->hash_dup_free += bstat.bt_leaf_pgfree +
		    bstat.bt_dup_pgfree + bstat.bt_int_pgfree;
		sp->hash_ndata += bstat.bt_ndata;
		break;
	case P_OVERFLOW:
		sp->hash_bigpages++;
		sp->hash_big_bfree += P_OVFLSPACE(dbp, dbp->pgsize, pagep);
		break;
	default:
		return (__db_pgfmt(dbp->dbenv, pagep->pgno));
	}

	return (0);
}
