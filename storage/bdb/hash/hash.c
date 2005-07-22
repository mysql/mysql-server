/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994
 *	Margo Seltzer.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: hash.c,v 11.200 2004/10/14 18:11:36 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"

static int  __ham_bulk __P((DBC *, DBT *, u_int32_t));
static int  __ham_c_close __P((DBC *, db_pgno_t, int *));
static int  __ham_c_del __P((DBC *));
static int  __ham_c_destroy __P((DBC *));
static int  __ham_c_get __P((DBC *, DBT *, DBT *, u_int32_t, db_pgno_t *));
static int  __ham_c_put __P((DBC *, DBT *, DBT *, u_int32_t, db_pgno_t *));
static int  __ham_c_writelock __P((DBC *));
static int  __ham_dup_return __P((DBC *, DBT *, u_int32_t));
static int  __ham_expand_table __P((DBC *));
static int  __ham_lookup __P((DBC *,
		const DBT *, u_int32_t, db_lockmode_t, db_pgno_t *));
static int  __ham_overwrite __P((DBC *, DBT *, u_int32_t));

/*
 * __ham_quick_delete --
 *	When performing a DB->del operation that does not involve secondary
 *	indices and is not removing an off-page duplicate tree, we can
 *	speed things up substantially by removing the entire duplicate
 *	set, if any is present, in one operation, rather than by conjuring
 *	up and deleting each of the items individually.  (All are stored
 *	in one big HKEYDATA structure.)  We don't bother to distinguish
 *	on-page duplicate sets from single, non-dup items;  they're deleted
 *	in exactly the same way.
 *
 *	This function is called by __db_delete when the appropriate
 *	conditions are met, and it performs the delete in the optimized way.
 *
 *	The cursor should be set to the first item in the duplicate
 *	set, or to the sole key/data pair when the key does not have a
 *	duplicate set, before the function is called.
 *
 * PUBLIC: int __ham_quick_delete __P((DBC *));
 */
int
__ham_quick_delete(dbc)
	DBC *dbc;
{
	int ret, t_ret;

	if ((ret = __ham_get_meta(dbc)) != 0)
		return (ret);

	/* Assert that we're not using secondary indices. */
	DB_ASSERT(!F_ISSET(dbc->dbp, DB_AM_SECONDARY));
	/*
	 * We should assert that we're not a primary either, but that
	 * would require grabbing the dbp's mutex, so we don't bother.
	 */

	/* Assert that we're set, but not to an off-page duplicate. */
	DB_ASSERT(IS_INITIALIZED(dbc));
	DB_ASSERT(((HASH_CURSOR *)dbc->internal)->opd == NULL);

	if ((ret = __ham_c_writelock(dbc)) == 0)
		ret = __ham_del_pair(dbc, 1);

	if ((t_ret = __ham_release_meta(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/* ****************** CURSORS ********************************** */
/*
 * __ham_c_init --
 *	Initialize the hash-specific portion of a cursor.
 *
 * PUBLIC: int __ham_c_init __P((DBC *));
 */
int
__ham_c_init(dbc)
	DBC *dbc;
{
	DB_ENV *dbenv;
	HASH_CURSOR *new_curs;
	int ret;

	dbenv = dbc->dbp->dbenv;
	if ((ret = __os_calloc(dbenv,
	    1, sizeof(struct cursor_t), &new_curs)) != 0)
		return (ret);
	if ((ret = __os_malloc(dbenv,
	    dbc->dbp->pgsize, &new_curs->split_buf)) != 0) {
		__os_free(dbenv, new_curs);
		return (ret);
	}

	dbc->internal = (DBC_INTERNAL *) new_curs;
	dbc->c_close = __db_c_close;
	dbc->c_count = __db_c_count_pp;
	dbc->c_del = __db_c_del_pp;
	dbc->c_dup = __db_c_dup_pp;
	dbc->c_get = __db_c_get_pp;
	dbc->c_pget = __db_c_pget_pp;
	dbc->c_put = __db_c_put_pp;
	dbc->c_am_bulk = __ham_bulk;
	dbc->c_am_close = __ham_c_close;
	dbc->c_am_del = __ham_c_del;
	dbc->c_am_destroy = __ham_c_destroy;
	dbc->c_am_get = __ham_c_get;
	dbc->c_am_put = __ham_c_put;
	dbc->c_am_writelock = __ham_c_writelock;

	return (__ham_item_init(dbc));
}

/*
 * __ham_c_close --
 *	Close down the cursor from a single use.
 */
static int
__ham_c_close(dbc, root_pgno, rmroot)
	DBC *dbc;
	db_pgno_t root_pgno;
	int *rmroot;
{
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *hcp;
	HKEYDATA *dp;
	db_lockmode_t lock_mode;
	int doroot, gotmeta, ret, t_ret;
	u_int32_t dirty;

	COMPQUIET(rmroot, 0);
	mpf = dbc->dbp->mpf;
	dirty = 0;
	doroot = gotmeta = ret = 0;
	hcp = (HASH_CURSOR *) dbc->internal;

	/* Check for off page dups. */
	if (dbc->internal->opd != NULL) {
		if ((ret = __ham_get_meta(dbc)) != 0)
			goto done;
		gotmeta = 1;
		lock_mode = DB_LOCK_READ;

		/* To support dirty reads we must reget the write lock. */
		if (F_ISSET(dbc->dbp, DB_AM_DIRTY) &&
		     F_ISSET((BTREE_CURSOR *)
		     dbc->internal->opd->internal, C_DELETED))
			lock_mode = DB_LOCK_WRITE;

		if ((ret = __ham_get_cpage(dbc, lock_mode)) != 0)
			goto out;
		dp = (HKEYDATA *)H_PAIRDATA(dbc->dbp, hcp->page, hcp->indx);

		/* If it's not a dup we aborted before we changed it. */
		if (HPAGE_PTYPE(dp) == H_OFFDUP)
			memcpy(&root_pgno,
			    HOFFPAGE_PGNO(dp), sizeof(db_pgno_t));
		else
			root_pgno = PGNO_INVALID;

		if ((ret =
		    hcp->opd->c_am_close(hcp->opd, root_pgno, &doroot)) != 0)
			goto out;
		if (doroot != 0) {
			if ((ret = __ham_del_pair(dbc, 1)) != 0)
				goto out;
			dirty = DB_MPOOL_DIRTY;
		}
	}

out:	if (hcp->page != NULL && (t_ret =
	    __memp_fput(mpf, hcp->page, dirty)) != 0 && ret == 0)
		ret = t_ret;
	if (gotmeta != 0 && (t_ret = __ham_release_meta(dbc)) != 0 && ret == 0)
		ret = t_ret;

done:	if ((t_ret = __ham_item_init(dbc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __ham_c_destroy --
 *	Cleanup the access method private part of a cursor.
 */
static int
__ham_c_destroy(dbc)
	DBC *dbc;
{
	HASH_CURSOR *hcp;

	hcp = (HASH_CURSOR *)dbc->internal;
	if (hcp->split_buf != NULL)
		__os_free(dbc->dbp->dbenv, hcp->split_buf);
	__os_free(dbc->dbp->dbenv, hcp);

	return (0);
}

/*
 * __ham_c_count --
 *	Return a count of on-page duplicates.
 *
 * PUBLIC: int __ham_c_count __P((DBC *, db_recno_t *));
 */
int
__ham_c_count(dbc, recnop)
	DBC *dbc;
	db_recno_t *recnop;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *hcp;
	db_indx_t len;
	db_recno_t recno;
	int ret, t_ret;
	u_int8_t *p, *pend;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	hcp = (HASH_CURSOR *)dbc->internal;

	recno = 0;

	if ((ret = __ham_get_cpage(dbc, DB_LOCK_READ)) != 0)
		return (ret);
	if (hcp->indx >= NUM_ENT(hcp->page)) {
		*recnop = 0;
		goto err;
	}

	switch (HPAGE_PTYPE(H_PAIRDATA(dbp, hcp->page, hcp->indx))) {
	case H_KEYDATA:
	case H_OFFPAGE:
		recno = 1;
		break;
	case H_DUPLICATE:
		p = HKEYDATA_DATA(H_PAIRDATA(dbp, hcp->page, hcp->indx));
		pend = p +
		    LEN_HDATA(dbp, hcp->page, dbp->pgsize, hcp->indx);
		for (; p < pend; recno++) {
			/* p may be odd, so copy rather than just dereffing */
			memcpy(&len, p, sizeof(db_indx_t));
			p += 2 * sizeof(db_indx_t) + len;
		}

		break;
	default:
		ret = __db_pgfmt(dbp->dbenv, hcp->pgno);
		goto err;
	}

	*recnop = recno;

err:	if ((t_ret = __memp_fput(mpf, hcp->page, 0)) != 0 && ret == 0)
		ret = t_ret;
	hcp->page = NULL;
	return (ret);
}

static int
__ham_c_del(dbc)
	DBC *dbc;
{
	DB *dbp;
	DBT repldbt;
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *hcp;
	int ret, t_ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	hcp = (HASH_CURSOR *)dbc->internal;

	if (F_ISSET(hcp, H_DELETED))
		return (DB_NOTFOUND);

	if ((ret = __ham_get_meta(dbc)) != 0)
		goto out;

	if ((ret = __ham_get_cpage(dbc, DB_LOCK_WRITE)) != 0)
		goto out;

	/* Off-page duplicates. */
	if (HPAGE_TYPE(dbp, hcp->page, H_DATAINDEX(hcp->indx)) == H_OFFDUP)
		goto out;

	if (F_ISSET(hcp, H_ISDUP)) { /* On-page duplicate. */
		if (hcp->dup_off == 0 &&
		    DUP_SIZE(hcp->dup_len) == LEN_HDATA(dbp, hcp->page,
		    hcp->hdr->dbmeta.pagesize, hcp->indx))
			ret = __ham_del_pair(dbc, 1);
		else {
			repldbt.flags = 0;
			F_SET(&repldbt, DB_DBT_PARTIAL);
			repldbt.doff = hcp->dup_off;
			repldbt.dlen = DUP_SIZE(hcp->dup_len);
			repldbt.size = 0;
			repldbt.data = HKEYDATA_DATA(H_PAIRDATA(dbp, hcp->page,
			    hcp->indx));
			if ((ret = __ham_replpair(dbc, &repldbt, 0)) == 0) {
				hcp->dup_tlen -= DUP_SIZE(hcp->dup_len);
				F_SET(hcp, H_DELETED);
				ret = __ham_c_update(dbc,
				    DUP_SIZE(hcp->dup_len), 0, 1);
			}
		}

	} else /* Not a duplicate */
		ret = __ham_del_pair(dbc, 1);

out:	if (hcp->page != NULL) {
		if ((t_ret = __memp_fput(mpf,
		    hcp->page, ret == 0 ? DB_MPOOL_DIRTY : 0)) != 0 && ret == 0)
			ret = t_ret;
		hcp->page = NULL;
	}
	if ((t_ret = __ham_release_meta(dbc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __ham_c_dup --
 *	Duplicate a hash cursor, such that the new one holds appropriate
 *	locks for the position of the original.
 *
 * PUBLIC: int __ham_c_dup __P((DBC *, DBC *));
 */
int
__ham_c_dup(orig_dbc, new_dbc)
	DBC *orig_dbc, *new_dbc;
{
	HASH_CURSOR *orig, *new;
	int ret;

	orig = (HASH_CURSOR *)orig_dbc->internal;
	new = (HASH_CURSOR *)new_dbc->internal;

	new->bucket = orig->bucket;
	new->lbucket = orig->lbucket;
	new->dup_off = orig->dup_off;
	new->dup_len = orig->dup_len;
	new->dup_tlen = orig->dup_tlen;

	if (F_ISSET(orig, H_DELETED))
		F_SET(new, H_DELETED);
	if (F_ISSET(orig, H_ISDUP))
		F_SET(new, H_ISDUP);

	/*
	 * If the old cursor held a lock and we're not in transactions, get one
	 * for the new one.   The reason that we don't need a new lock if we're
	 * in a transaction is because we already hold a lock and will continue
	 * to do so until commit, so there is no point in re-acquiring it. We
	 * don't know if the old lock was a read or write lock, but it doesn't
	 * matter. We'll get a read lock.  We know that this locker already
	 * holds a lock of the correct type, so if we need a write lock and
	 * request it, we know that we'll get it.
	 */
	if (orig_dbc->txn == NULL && LOCK_ISSET(orig->lock))
		if ((ret = __ham_lock_bucket(new_dbc, DB_LOCK_READ)) != 0)
			return (ret);

	return (0);
}

static int
__ham_c_get(dbc, key, data, flags, pgnop)
	DBC *dbc;
	DBT *key;
	DBT *data;
	u_int32_t flags;
	db_pgno_t *pgnop;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *hcp;
	db_lockmode_t lock_type;
	int get_key, ret, t_ret;

	hcp = (HASH_CURSOR *)dbc->internal;
	dbp = dbc->dbp;
	mpf = dbp->mpf;

	/* Clear OR'd in additional bits so we can check for flag equality. */
	if (F_ISSET(dbc, DBC_RMW))
		lock_type = DB_LOCK_WRITE;
	else
		lock_type = DB_LOCK_READ;

	if ((ret = __ham_get_meta(dbc)) != 0)
		return (ret);
	hcp->seek_size = 0;

	ret = 0;
	get_key = 1;
	switch (flags) {
	case DB_PREV_NODUP:
		F_SET(hcp, H_NEXT_NODUP);
		/* FALLTHROUGH */
	case DB_PREV:
		if (IS_INITIALIZED(dbc)) {
			ret = __ham_item_prev(dbc, lock_type, pgnop);
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:
		ret = __ham_item_last(dbc, lock_type, pgnop);
		break;
	case DB_NEXT_NODUP:
		F_SET(hcp, H_NEXT_NODUP);
		/* FALLTHROUGH */
	case DB_NEXT:
		if (IS_INITIALIZED(dbc)) {
			ret = __ham_item_next(dbc, lock_type, pgnop);
			break;
		}
		/* FALLTHROUGH */
	case DB_FIRST:
		ret = __ham_item_first(dbc, lock_type, pgnop);
		break;
	case DB_NEXT_DUP:
		/* cgetchk has already determined that the cursor is set. */
		F_SET(hcp, H_DUPONLY);
		ret = __ham_item_next(dbc, lock_type, pgnop);
		break;
	case DB_SET:
	case DB_SET_RANGE:
	case DB_GET_BOTH:
	case DB_GET_BOTH_RANGE:
		ret = __ham_lookup(dbc, key, 0, lock_type, pgnop);
		get_key = 0;
		break;
	case DB_GET_BOTHC:
		F_SET(hcp, H_DUPONLY);

		ret = __ham_item_next(dbc, lock_type, pgnop);
		get_key = 0;
		break;
	case DB_CURRENT:
		/* cgetchk has already determined that the cursor is set. */
		if (F_ISSET(hcp, H_DELETED)) {
			ret = DB_KEYEMPTY;
			goto err;
		}

		ret = __ham_item(dbc, lock_type, pgnop);
		break;
	default:
		ret = __db_unknown_flag(dbp->dbenv, "__ham_c_get", flags);
		break;
	}

	/*
	 * Must always enter this loop to do error handling and
	 * check for big key/data pair.
	 */
	for (;;) {
		if (ret != 0 && ret != DB_NOTFOUND)
			goto err;
		else if (F_ISSET(hcp, H_OK)) {
			if (*pgnop == PGNO_INVALID)
				ret = __ham_dup_return(dbc, data, flags);
			break;
		} else if (!F_ISSET(hcp, H_NOMORE)) {
			__db_err(dbp->dbenv,
			    "H_NOMORE returned to __ham_c_get");
			ret = EINVAL;
			break;
		}

		/*
		 * Ran out of entries in a bucket; change buckets.
		 */
		switch (flags) {
			case DB_LAST:
			case DB_PREV:
			case DB_PREV_NODUP:
				ret = __memp_fput(mpf, hcp->page, 0);
				hcp->page = NULL;
				if (hcp->bucket == 0) {
					ret = DB_NOTFOUND;
					hcp->pgno = PGNO_INVALID;
					goto err;
				}
				F_CLR(hcp, H_ISDUP);
				hcp->bucket--;
				hcp->indx = NDX_INVALID;
				hcp->pgno = BUCKET_TO_PAGE(hcp, hcp->bucket);
				if (ret == 0)
					ret = __ham_item_prev(dbc,
					    lock_type, pgnop);
				break;
			case DB_FIRST:
			case DB_NEXT:
			case DB_NEXT_NODUP:
				ret = __memp_fput(mpf, hcp->page, 0);
				hcp->page = NULL;
				hcp->indx = NDX_INVALID;
				hcp->bucket++;
				F_CLR(hcp, H_ISDUP);
				hcp->pgno = BUCKET_TO_PAGE(hcp, hcp->bucket);
				if (hcp->bucket > hcp->hdr->max_bucket) {
					ret = DB_NOTFOUND;
					hcp->pgno = PGNO_INVALID;
					goto err;
				}
				if (ret == 0)
					ret = __ham_item_next(dbc,
					    lock_type, pgnop);
				break;
			case DB_GET_BOTH:
			case DB_GET_BOTHC:
			case DB_GET_BOTH_RANGE:
			case DB_NEXT_DUP:
			case DB_SET:
			case DB_SET_RANGE:
				/* Key not found. */
				ret = DB_NOTFOUND;
				goto err;
			case DB_CURRENT:
				/*
				 * This should only happen if you are doing
				 * deletes and reading with concurrent threads
				 * and not doing proper locking.  We return
				 * the same error code as we would if the
				 * cursor were deleted.
				 */
				ret = DB_KEYEMPTY;
				goto err;
			default:
				DB_ASSERT(0);
		}
	}

	if (get_key == 0)
		F_SET(key, DB_DBT_ISSET);

err:	if ((t_ret = __ham_release_meta(dbc)) != 0 && ret == 0)
		ret = t_ret;

	F_CLR(hcp, H_DUPONLY);
	F_CLR(hcp, H_NEXT_NODUP);

	return (ret);
}

/*
 * __ham_bulk -- Return bulk data from a hash table.
 */
static int
__ham_bulk(dbc, data, flags)
	DBC *dbc;
	DBT *data;
	u_int32_t flags;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *cp;
	PAGE *pg;
	db_indx_t dup_len, dup_off, dup_tlen, indx, *inp;
	db_lockmode_t lock_mode;
	db_pgno_t pgno;
	int32_t *endp, *offp, *saveoff;
	u_int32_t key_off, key_size, pagesize, size, space;
	u_int8_t *dbuf, *dp, *hk, *np, *tmp;
	int is_dup, is_key;
	int need_pg, next_key, no_dup, ret, t_ret;

	ret = 0;
	key_off = 0;
	dup_len = dup_off = dup_tlen = 0;
	size = 0;
	dbp = dbc->dbp;
	pagesize = dbp->pgsize;
	mpf = dbp->mpf;
	cp = (HASH_CURSOR *)dbc->internal;
	is_key = LF_ISSET(DB_MULTIPLE_KEY) ? 1 : 0;
	next_key = is_key && LF_ISSET(DB_OPFLAGS_MASK) != DB_NEXT_DUP;
	no_dup = LF_ISSET(DB_OPFLAGS_MASK) == DB_NEXT_NODUP;
	dbuf = data->data;
	np = dp = dbuf;

	/* Keep track of space that is left.  There is an termination entry */
	space = data->ulen;
	space -= sizeof(*offp);

	/* Build the offset/size table from the end up. */
	endp = (int32_t *) ((u_int8_t *)dbuf + data->ulen);
	endp--;
	offp = endp;

	key_size = 0;
	lock_mode = F_ISSET(dbc, DBC_RMW) ? DB_LOCK_WRITE: DB_LOCK_READ;

next_pg:
	need_pg = 1;
	indx = cp->indx;
	pg = cp->page;
	inp = P_INP(dbp, pg);

	do {
		if (is_key) {
			hk = H_PAIRKEY(dbp, pg, indx);
			if (HPAGE_PTYPE(hk) == H_OFFPAGE) {
				memcpy(&key_size,
				    HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
				memcpy(&pgno,
				    HOFFPAGE_PGNO(hk), sizeof(db_pgno_t));
				size = key_size;
				if (key_size > space)
					goto get_key_space;
				if ((ret = __bam_bulk_overflow(
				    dbc, key_size, pgno, np)) != 0)
					return (ret);
				space -= key_size;
				key_off = (u_int32_t)(np - dbuf);
				np += key_size;
			} else {
				if (need_pg) {
					dp = np;
					size = pagesize - HOFFSET(pg);
					if (space < size) {
get_key_space:
						if (offp == endp) {
							data->size = (u_int32_t)
							    DB_ALIGN(size +
							    pagesize, 1024);
							return
							    (DB_BUFFER_SMALL);
						}
						goto back_up;
					}
					memcpy(dp,
					   (u_int8_t *)pg + HOFFSET(pg), size);
					need_pg = 0;
					space -= size;
					np += size;
				}
				key_size = LEN_HKEY(dbp, pg, pagesize, indx);
				key_off = ((inp[indx] - HOFFSET(pg)) +
				    (u_int32_t)(dp - dbuf)) +
				    SSZA(HKEYDATA, data);
			}
		}

		hk = H_PAIRDATA(dbp, pg, indx);
		switch (HPAGE_PTYPE(hk)) {
		case H_DUPLICATE:
		case H_KEYDATA:
			if (need_pg) {
				dp = np;
				size = pagesize - HOFFSET(pg);
				if (space < size) {
back_up:
					if (indx != 0) {
						indx -= 2;
						/* XXX
						 * It's not clear that this is
						 * the right way to fix this,
						 * but here goes.
						 * If we are backing up onto a
						 * duplicate, then we need to
						 * position ourselves at the
						 * end of the duplicate set.
						 * We probably need to make
						 * this work for H_OFFDUP too.
						 * It might be worth making a
						 * dummy cursor and calling
						 * __ham_item_prev.
						 */
						tmp = H_PAIRDATA(dbp, pg, indx);
						if (HPAGE_PTYPE(tmp) ==
						    H_DUPLICATE) {
							dup_off = dup_tlen =
							    LEN_HDATA(dbp, pg,
							    pagesize, indx + 1);
							memcpy(&dup_len,
							    HKEYDATA_DATA(tmp),
							    sizeof(db_indx_t));
						} else  {
							is_dup = 0;
							dup_len = 0;
							dup_off = 0;
							dup_tlen = 0;
							F_CLR(cp, H_ISDUP);
						}
						goto get_space;
					}
					/* indx == 0 */
					cp->dup_len = dup_len;
					cp->dup_off = dup_off;
					cp->dup_tlen = dup_tlen;
					if ((ret = __ham_item_prev(dbc,
					    lock_mode, &pgno)) != 0) {
						if (ret != DB_NOTFOUND)
							return (ret);
						if ((ret = __memp_fput(mpf,
						    cp->page, 0)) != 0)
							return (ret);
						cp->page = NULL;
						if (cp->bucket == 0) {
							cp->indx = indx =
							    NDX_INVALID;
							goto get_space;
						}
						if ((ret =
						    __ham_get_meta(dbc)) != 0)
							return (ret);

						cp->bucket--;
						cp->pgno = BUCKET_TO_PAGE(cp,
						    cp->bucket);
						cp->indx = NDX_INVALID;
						if ((ret = __ham_release_meta(
						    dbc)) != 0)
							return (ret);
						if ((ret = __ham_item_prev(dbc,
						    lock_mode, &pgno)) != 0)
							return (ret);
					}
					indx = cp->indx;
get_space:
					/*
					 * See if we put any data in the buffer.
					 */
					if (offp >= endp ||
					    F_ISSET(dbc, DBC_TRANSIENT)) {
						data->size = (u_int32_t)
						    DB_ALIGN(size +
						    data->ulen - space, 1024);
						return (DB_BUFFER_SMALL);
					}
					/*
					 * Don't continue;  we're all out
					 * of space, even though we're
					 * returning success.
					 */
					next_key = 0;
					break;
				}
				memcpy(dp, (u_int8_t *)pg + HOFFSET(pg), size);
				need_pg = 0;
				space -= size;
				np += size;
			}

			/*
			 * We're about to crack the offset(s) and length(s)
			 * out of an H_KEYDATA or H_DUPLICATE item.
			 * There are three cases:
			 *   1. We were moved into a duplicate set by
			 *	the standard hash cursor code.  Respect
			 *	the dup_off and dup_tlen we were given.
			 *   2. We stumbled upon a duplicate set while
			 *	walking the page on our own.  We need to
			 *	recognize it as a dup and set dup_off and
			 *	dup_tlen.
			 *   3. The current item is not a dup.
			 */
			if (F_ISSET(cp, H_ISDUP)) {
				/* Case 1 */
				is_dup = 1;
				dup_len = cp->dup_len;
				dup_off = cp->dup_off;
				dup_tlen = cp->dup_tlen;
			} else if (HPAGE_PTYPE(hk) == H_DUPLICATE) {
				/* Case 2 */
				is_dup = 1;
				/*
				 * If we run out of memory and bail,
				 * make sure the fact we're in a dup set
				 * isn't ignored later.
				 */
				F_SET(cp, H_ISDUP);
				dup_off = 0;
				memcpy(&dup_len,
				    HKEYDATA_DATA(hk), sizeof(db_indx_t));
				dup_tlen = LEN_HDATA(dbp, pg, pagesize, indx);
			} else {
				/* Case 3 */
				is_dup = 0;
				dup_len = 0;
				dup_off = 0;
				dup_tlen = 0;
			}

			do {
				space -= (is_key ? 4 : 2) * sizeof(*offp);
				size += (is_key ? 4 : 2) * sizeof(*offp);
				/*
				 * Since space is an unsigned, if we happen
				 * to wrap, then this comparison will turn out
				 * to be true.  XXX Wouldn't it be better to
				 * simply check above that space is greater than
				 * the value we're about to subtract???
				 */
				if (space > data->ulen) {
					if (!is_dup || dup_off == 0)
						goto back_up;
					dup_off -= (db_indx_t)
					    DUP_SIZE((u_int32_t)offp[1]);
					goto get_space;
				}
				if (is_key) {
					*offp-- = (int32_t)key_off;
					*offp-- = (int32_t)key_size;
				}
				if (is_dup) {
					*offp-- = (int32_t)(
					    ((inp[indx + 1] - HOFFSET(pg)) +
					    dp - dbuf) + SSZA(HKEYDATA, data) +
					    dup_off + sizeof(db_indx_t));
					memcpy(&dup_len,
					    HKEYDATA_DATA(hk) + dup_off,
					    sizeof(db_indx_t));
					dup_off += DUP_SIZE(dup_len);
					*offp-- = dup_len;
				} else {
					*offp-- = (int32_t)(
					    ((inp[indx + 1] - HOFFSET(pg)) +
					    dp - dbuf) + SSZA(HKEYDATA, data));
					*offp-- = LEN_HDATA(dbp, pg,
					    pagesize, indx);
				}
			} while (is_dup && dup_off < dup_tlen && no_dup == 0);
			F_CLR(cp, H_ISDUP);
			break;
		case H_OFFDUP:
			memcpy(&pgno, HOFFPAGE_PGNO(hk), sizeof(db_pgno_t));
			space -= 2 * sizeof(*offp);
			if (space > data->ulen)
				goto back_up;

			if (is_key) {
				space -= 2 * sizeof(*offp);
				if (space > data->ulen)
					goto back_up;
				*offp-- = (int32_t)key_off;
				*offp-- = (int32_t)key_size;
			}
			saveoff = offp;
			if ((ret = __bam_bulk_duplicates(dbc,
			    pgno, dbuf, is_key ? offp + 2 : NULL,
			    &offp, &np, &space, no_dup)) != 0) {
				if (ret == DB_BUFFER_SMALL) {
					size = space;
					space = 0;
					if (is_key && saveoff == offp) {
						offp += 2;
						goto back_up;
					}
					goto get_space;
				}
				return (ret);
			}
			break;
		case H_OFFPAGE:
			space -= (is_key ? 4 : 2) * sizeof(*offp);
			if (space > data->ulen)
				goto back_up;

			memcpy(&size, HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
			memcpy(&pgno, HOFFPAGE_PGNO(hk), sizeof(db_pgno_t));
			if (size > space)
				goto back_up;

			if ((ret =
			    __bam_bulk_overflow(dbc, size, pgno, np)) != 0)
				return (ret);

			if (is_key) {
				*offp-- = (int32_t)key_off;
				*offp-- = (int32_t)key_size;
			}

			*offp-- = (int32_t)(np - dbuf);
			*offp-- = (int32_t)size;

			np += size;
			space -= size;
			break;
		default:
			/* Do nothing. */
			break;
		}
	} while (next_key && (indx += 2) < NUM_ENT(pg));

	cp->indx = indx;
	cp->dup_len = dup_len;
	cp->dup_off = dup_off;
	cp->dup_tlen = dup_tlen;

	/* If we are off the page then try to the next page. */
	if (ret == 0 && next_key && indx >= NUM_ENT(pg)) {
		if ((ret = __ham_item_next(dbc, lock_mode, &pgno)) == 0)
			goto next_pg;
		if (ret != DB_NOTFOUND)
			return (ret);
		if ((ret = __memp_fput(dbc->dbp->mpf, cp->page, 0)) != 0)
			return (ret);
		cp->page = NULL;
		if ((ret = __ham_get_meta(dbc)) != 0)
			return (ret);

		cp->bucket++;
		if (cp->bucket > cp->hdr->max_bucket) {
			/*
			 * Restore cursor to its previous state.  We're past
			 * the last item in the last bucket, so the next
			 * DBC->c_get(DB_NEXT) will return DB_NOTFOUND.
			 */
			cp->bucket--;
			ret = DB_NOTFOUND;
		} else {
			/*
			 * Start on the next bucket.
			 *
			 * Note that if this new bucket happens to be empty,
			 * but there's another non-empty bucket after it,
			 * we'll return early.  This is a rare case, and we
			 * don't guarantee any particular number of keys
			 * returned on each call, so just let the next call
			 * to bulk get move forward by yet another bucket.
			 */
			cp->pgno = BUCKET_TO_PAGE(cp, cp->bucket);
			cp->indx = NDX_INVALID;
			F_CLR(cp, H_ISDUP);
			ret = __ham_item_next(dbc, lock_mode, &pgno);
		}

		if ((t_ret = __ham_release_meta(dbc)) != 0)
			return (t_ret);
		if (ret == 0)
			goto next_pg;
		if (ret != DB_NOTFOUND)
			return (ret);
	}
	*offp = -1;
	return (0);
}

static int
__ham_c_put(dbc, key, data, flags, pgnop)
	DBC *dbc;
	DBT *key;
	DBT *data;
	u_int32_t flags;
	db_pgno_t *pgnop;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	DBT tmp_val, *myval;
	HASH_CURSOR *hcp;
	u_int32_t nbytes;
	int ret, t_ret;

	/*
	 * The compiler doesn't realize that we only use this when ret is
	 * equal to 0 and that if ret is equal to 0, that we must have set
	 * myval.  So, we initialize it here to shut the compiler up.
	 */
	COMPQUIET(myval, NULL);

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	hcp = (HASH_CURSOR *)dbc->internal;

	if (F_ISSET(hcp, H_DELETED) &&
	    flags != DB_KEYFIRST && flags != DB_KEYLAST)
		return (DB_NOTFOUND);

	if ((ret = __ham_get_meta(dbc)) != 0)
		goto err1;

	switch (flags) {
	case DB_KEYLAST:
	case DB_KEYFIRST:
	case DB_NODUPDATA:
		nbytes = (ISBIG(hcp, key->size) ? HOFFPAGE_PSIZE :
		    HKEYDATA_PSIZE(key->size)) +
		    (ISBIG(hcp, data->size) ? HOFFPAGE_PSIZE :
		    HKEYDATA_PSIZE(data->size));
		if ((ret = __ham_lookup(dbc,
		    key, nbytes, DB_LOCK_WRITE, pgnop)) == DB_NOTFOUND) {
			ret = 0;
			if (hcp->seek_found_page != PGNO_INVALID &&
			    hcp->seek_found_page != hcp->pgno) {
				if ((ret = __memp_fput(mpf, hcp->page, 0)) != 0)
					goto err2;
				hcp->page = NULL;
				hcp->pgno = hcp->seek_found_page;
				hcp->indx = NDX_INVALID;
			}

			if (F_ISSET(data, DB_DBT_PARTIAL) && data->doff != 0) {
				/*
				 * A partial put, but the key does not exist
				 * and we are not beginning the write at 0.
				 * We must create a data item padded up to doff
				 * and then write the new bytes represented by
				 * val.
				 */
				if ((ret = __ham_init_dbt(dbp->dbenv, &tmp_val,
				    data->size + data->doff,
				    &dbc->my_rdata.data,
				    &dbc->my_rdata.ulen)) == 0) {
					memset(tmp_val.data, 0, data->doff);
					memcpy((u_int8_t *)tmp_val.data +
					    data->doff, data->data, data->size);
					myval = &tmp_val;
				}
			} else
				myval = (DBT *)data;

			if (ret == 0)
				ret = __ham_add_el(dbc, key, myval, H_KEYDATA);
			goto done;
		}
		break;
	case DB_BEFORE:
	case DB_AFTER:
	case DB_CURRENT:
		ret = __ham_item(dbc, DB_LOCK_WRITE, pgnop);
		break;
	default:
		ret = __db_unknown_flag(dbp->dbenv, "__ham_c_put", flags);
		break;
	}

	if (*pgnop == PGNO_INVALID && ret == 0) {
		if (flags == DB_CURRENT ||
		    ((flags == DB_KEYFIRST ||
		    flags == DB_KEYLAST || flags == DB_NODUPDATA) &&
		    !(F_ISSET(dbp, DB_AM_DUP) || F_ISSET(key, DB_DBT_DUPOK))))
			ret = __ham_overwrite(dbc, data, flags);
		else
			ret = __ham_add_dup(dbc, data, flags, pgnop);
	}

done:	if (hcp->page != NULL) {
		if ((t_ret = __memp_fput(mpf,
		    hcp->page, DB_MPOOL_DIRTY)) != 0 && ret == 0)
			ret = t_ret;
		if (t_ret == 0)
			hcp->page = NULL;
	}

	if (ret == 0 && F_ISSET(hcp, H_EXPAND)) {
		ret = __ham_expand_table(dbc);
		F_CLR(hcp, H_EXPAND);
	}

err2:	if ((t_ret = __ham_release_meta(dbc)) != 0 && ret == 0)
		ret = t_ret;

err1:	return (ret);
}

/********************************* UTILITIES ************************/

/*
 * __ham_expand_table --
 */
static int
__ham_expand_table(dbc)
	DBC *dbc;
{
	DB *dbp;
	DB_LOCK metalock;
	DB_LSN lsn;
	DB_MPOOLFILE *mpf;
	DBMETA *mmeta;
	HASH_CURSOR *hcp;
	PAGE *h;
	db_pgno_t pgno, mpgno;
	u_int32_t dirty_meta, logn, newalloc, new_bucket, old_bucket;
	int got_meta, new_double, ret, t_ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	hcp = (HASH_CURSOR *)dbc->internal;
	if ((ret = __ham_dirty_meta(dbc)) != 0)
		return (ret);

	LOCK_INIT(metalock);
	mmeta = (DBMETA *) hcp->hdr;
	mpgno = mmeta->pgno;
	h = NULL;
	dirty_meta = newalloc = 0;
	got_meta = 0;

	/*
	 * If the split point is about to increase, make sure that we
	 * have enough extra pages.  The calculation here is weird.
	 * We'd like to do this after we've upped max_bucket, but it's
	 * too late then because we've logged the meta-data split.  What
	 * we'll do between then and now is increment max bucket and then
	 * see what the log of one greater than that is; here we have to
	 * look at the log of max + 2.  VERY NASTY STUFF.
	 *
	 * We figure out what we need to do, then we log it, then request
	 * the pages from mpool.  We don't want to fail after extending
	 * the file.
	 *
	 * If the page we are about to split into has already been allocated,
	 * then we simply need to get it to get its LSN.  If it hasn't yet
	 * been allocated, then we know it's LSN (0,0).
	 */

	new_bucket = hcp->hdr->max_bucket + 1;
	old_bucket = new_bucket & hcp->hdr->low_mask;

	new_double = hcp->hdr->max_bucket == hcp->hdr->high_mask;
	logn = __db_log2(new_bucket);

	if (!new_double || hcp->hdr->spares[logn + 1] != PGNO_INVALID) {
		/* Page exists; get it so we can get its LSN */
		pgno = BUCKET_TO_PAGE(hcp, new_bucket);
		if ((ret =
		    __memp_fget(mpf, &pgno, DB_MPOOL_CREATE, &h)) != 0)
			goto err;
		lsn = h->lsn;
	} else {
		/* Get the master meta-data page to do allocation. */
		if (F_ISSET(dbp, DB_AM_SUBDB)) {
			mpgno = PGNO_BASE_MD;
			if ((ret = __db_lget(dbc,
			   0, mpgno, DB_LOCK_WRITE, 0, &metalock)) != 0)
				goto err;
			if ((ret = __memp_fget(mpf, &mpgno, 0, &mmeta)) != 0)
				goto err;
			got_meta = 1;
		}
		pgno = mmeta->last_pgno + 1;
		ZERO_LSN(lsn);
		newalloc = 1;
	}

	/* Log the meta-data split first. */
	if (DBC_LOGGING(dbc)) {
		/*
		 * We always log the page number of the first page of
		 * the allocation group.  However, the LSN that we log
		 * is either the LSN on the first page (if we did not
		 * do the actual allocation here) or the LSN on the last
		 * page of the unit (if we did do the allocation here).
		 */
		if ((ret = __ham_metagroup_log(dbp, dbc->txn,
		    &lsn, 0, hcp->hdr->max_bucket, mpgno, &mmeta->lsn,
		    hcp->hdr->dbmeta.pgno, &hcp->hdr->dbmeta.lsn,
		    pgno, &lsn, newalloc, mmeta->last_pgno)) != 0)
			goto err;
	} else
		LSN_NOT_LOGGED(lsn);

	hcp->hdr->dbmeta.lsn = lsn;

	if (new_double && hcp->hdr->spares[logn + 1] == PGNO_INVALID) {
		/*
		 * We need to begin a new doubling and we have not allocated
		 * any pages yet.  Read the last page in and initialize it to
		 * make the allocation contiguous.  The pgno we calculated
		 * above is the first page allocated. The entry in spares is
		 * that page number minus any buckets already allocated (it
		 * simplifies bucket to page transaction).  After we've set
		 * that, we calculate the last pgno.
		 */

		hcp->hdr->spares[logn + 1] = pgno - new_bucket;
		pgno += hcp->hdr->max_bucket;

		if ((ret = __memp_fget(mpf, &pgno, DB_MPOOL_CREATE, &h)) != 0)
			goto err;

		mmeta->last_pgno = pgno;
		mmeta->lsn = lsn;
		dirty_meta = DB_MPOOL_DIRTY;

		P_INIT(h, dbp->pgsize,
		    pgno, PGNO_INVALID, PGNO_INVALID, 0, P_HASH);
	}

	/* Write out whatever page we ended up modifying. */
	h->lsn = lsn;
	if ((ret = __memp_fput(mpf, h, DB_MPOOL_DIRTY)) != 0)
		goto err;
	h = NULL;

	/*
	 * Update the meta-data page of this hash database.
	 */
	hcp->hdr->max_bucket = new_bucket;
	if (new_double) {
		hcp->hdr->low_mask = hcp->hdr->high_mask;
		hcp->hdr->high_mask = new_bucket | hcp->hdr->low_mask;
	}

	/* Relocate records to the new bucket */
	ret = __ham_split_page(dbc, old_bucket, new_bucket);

err:	if (got_meta)
		if ((t_ret =
		    __memp_fput(mpf, mmeta, dirty_meta)) != 0 && ret == 0)
			ret = t_ret;
	if ((t_ret = __TLPUT(dbc, metalock)) != 0 && ret == 0)
			ret = t_ret;
	if (h != NULL)
		if ((t_ret = __memp_fput(mpf, h, 0)) != 0 && ret == 0)
			ret = t_ret;

	return (ret);
}

/*
 * PUBLIC: u_int32_t __ham_call_hash __P((DBC *, u_int8_t *, u_int32_t));
 */
u_int32_t
__ham_call_hash(dbc, k, len)
	DBC *dbc;
	u_int8_t *k;
	u_int32_t len;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	HASH *hashp;
	u_int32_t n, bucket;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;
	hashp = dbp->h_internal;

	n = (u_int32_t)(hashp->h_hash(dbp, k, len));

	bucket = n & hcp->hdr->high_mask;
	if (bucket > hcp->hdr->max_bucket)
		bucket = bucket & hcp->hdr->low_mask;
	return (bucket);
}

/*
 * Check for duplicates, and call __db_ret appropriately.  Release
 * everything held by the cursor.
 */
static int
__ham_dup_return(dbc, val, flags)
	DBC *dbc;
	DBT *val;
	u_int32_t flags;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	PAGE *pp;
	DBT *myval, tmp_val;
	db_indx_t ndx;
	db_pgno_t pgno;
	u_int32_t off, tlen;
	u_int8_t *hk, type;
	int cmp, ret;
	db_indx_t len;

	/* Check for duplicate and return the first one. */
	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;
	ndx = H_DATAINDEX(hcp->indx);
	type = HPAGE_TYPE(dbp, hcp->page, ndx);
	pp = hcp->page;
	myval = val;

	/*
	 * There are 4 cases:
	 * 1. We are not in duplicate, simply return; the upper layer
	 *    will do the right thing.
	 * 2. We are looking at keys and stumbled onto a duplicate.
	 * 3. We are in the middle of a duplicate set. (ISDUP set)
	 * 4. We need to check for particular data match.
	 */

	/* We should never get here with off-page dups. */
	DB_ASSERT(type != H_OFFDUP);

	/* Case 1 */
	if (type != H_DUPLICATE && flags != DB_GET_BOTH &&
	    flags != DB_GET_BOTHC && flags != DB_GET_BOTH_RANGE)
		return (0);

	/*
	 * Here we check for the case where we just stumbled onto a
	 * duplicate.  In this case, we do initialization and then
	 * let the normal duplicate code handle it. (Case 2)
	 */
	if (!F_ISSET(hcp, H_ISDUP) && type == H_DUPLICATE) {
		F_SET(hcp, H_ISDUP);
		hcp->dup_tlen = LEN_HDATA(dbp, hcp->page,
		    hcp->hdr->dbmeta.pagesize, hcp->indx);
		hk = H_PAIRDATA(dbp, hcp->page, hcp->indx);
		if (flags == DB_LAST ||
		    flags == DB_PREV || flags == DB_PREV_NODUP) {
			hcp->dup_off = 0;
			do {
				memcpy(&len,
				    HKEYDATA_DATA(hk) + hcp->dup_off,
				    sizeof(db_indx_t));
				hcp->dup_off += DUP_SIZE(len);
			} while (hcp->dup_off < hcp->dup_tlen);
			hcp->dup_off -= DUP_SIZE(len);
		} else {
			memcpy(&len,
			    HKEYDATA_DATA(hk), sizeof(db_indx_t));
			hcp->dup_off = 0;
		}
		hcp->dup_len = len;
	}

	/*
	 * If we are retrieving a specific key/data pair, then we
	 * may need to adjust the cursor before returning data.
	 * Case 4
	 */
	if (flags == DB_GET_BOTH ||
	    flags == DB_GET_BOTHC || flags == DB_GET_BOTH_RANGE) {
		if (F_ISSET(hcp, H_ISDUP)) {
			/*
			 * If we're doing a join, search forward from the
			 * current position, not the beginning of the dup set.
			 */
			if (flags == DB_GET_BOTHC)
				F_SET(hcp, H_CONTINUE);

			__ham_dsearch(dbc, val, &off, &cmp, flags);

			/*
			 * This flag is set nowhere else and is safe to
			 * clear unconditionally.
			 */
			F_CLR(hcp, H_CONTINUE);
			hcp->dup_off = off;
		} else {
			hk = H_PAIRDATA(dbp, hcp->page, hcp->indx);
			if (((HKEYDATA *)hk)->type == H_OFFPAGE) {
				memcpy(&tlen,
				    HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
				memcpy(&pgno,
				    HOFFPAGE_PGNO(hk), sizeof(db_pgno_t));
				if ((ret = __db_moff(dbp, val,
				    pgno, tlen, dbp->dup_compare, &cmp)) != 0)
					return (ret);
			} else {
				/*
				 * We do not zero tmp_val since the comparison
				 * routines may only look at data and size.
				 */
				tmp_val.data = HKEYDATA_DATA(hk);
				tmp_val.size = LEN_HDATA(dbp, hcp->page,
				    dbp->pgsize, hcp->indx);
				cmp = dbp->dup_compare == NULL ?
				    __bam_defcmp(dbp, &tmp_val, val) :
				    dbp->dup_compare(dbp, &tmp_val, val);
			}
		}

		if (cmp != 0)
			return (DB_NOTFOUND);
	}

	/*
	 * If we're doing a bulk get, we don't want to actually return
	 * the data:  __ham_bulk will take care of cracking out the
	 * duplicates appropriately.
	 *
	 * The rest of this function calculates partial offsets and
	 * handles the actual __db_ret, so just return if
	 * DB_MULTIPLE(_KEY) is set.
	 */
	if (F_ISSET(dbc, DBC_MULTIPLE | DBC_MULTIPLE_KEY))
		return (0);

	/*
	 * Now, everything is initialized, grab a duplicate if
	 * necessary.
	 */
	if (F_ISSET(hcp, H_ISDUP)) {	/* Case 3 */
		/*
		 * Copy the DBT in case we are retrieving into user
		 * memory and we need the parameters for it.  If the
		 * user requested a partial, then we need to adjust
		 * the user's parameters to get the partial of the
		 * duplicate which is itself a partial.
		 */
		memcpy(&tmp_val, val, sizeof(*val));
		if (F_ISSET(&tmp_val, DB_DBT_PARTIAL)) {
			/*
			 * Take the user's length unless it would go
			 * beyond the end of the duplicate.
			 */
			if (tmp_val.doff + hcp->dup_off > hcp->dup_len)
				tmp_val.dlen = 0;
			else if (tmp_val.dlen + tmp_val.doff >
			    hcp->dup_len)
				tmp_val.dlen =
				    hcp->dup_len - tmp_val.doff;

			/*
			 * Calculate the new offset.
			 */
			tmp_val.doff += hcp->dup_off;
		} else {
			F_SET(&tmp_val, DB_DBT_PARTIAL);
			tmp_val.dlen = hcp->dup_len;
			tmp_val.doff = hcp->dup_off + sizeof(db_indx_t);
		}
		myval = &tmp_val;
	}

	/*
	 * Finally, if we had a duplicate, pp, ndx, and myval should be
	 * set appropriately.
	 */
	if ((ret = __db_ret(dbp, pp, ndx, myval, &dbc->rdata->data,
	    &dbc->rdata->ulen)) != 0)
		return (ret);

	/*
	 * In case we sent a temporary off to db_ret, set the real
	 * return values.
	 */
	val->data = myval->data;
	val->size = myval->size;

	F_SET(val, DB_DBT_ISSET);

	return (0);
}

static int
__ham_overwrite(dbc, nval, flags)
	DBC *dbc;
	DBT *nval;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	HASH_CURSOR *hcp;
	DBT *myval, tmp_val, tmp_val2;
	void *newrec;
	u_int8_t *hk, *p;
	u_int32_t len, nondup_size;
	db_indx_t newsize;
	int ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;
	hcp = (HASH_CURSOR *)dbc->internal;
	if (F_ISSET(hcp, H_ISDUP)) {
		/*
		 * This is an overwrite of a duplicate. We should never
		 * be off-page at this point.
		 */
		DB_ASSERT(hcp->opd == NULL);
		/* On page dups */
		if (F_ISSET(nval, DB_DBT_PARTIAL)) {
			/*
			 * We're going to have to get the current item, then
			 * construct the record, do any padding and do a
			 * replace.
			 */
			memset(&tmp_val, 0, sizeof(tmp_val));
			if ((ret =
			    __ham_dup_return(dbc, &tmp_val, DB_CURRENT)) != 0)
				return (ret);

			/* Figure out new size. */
			nondup_size = tmp_val.size;
			newsize = nondup_size;

			/*
			 * Three cases:
			 * 1. strictly append (may need to allocate space
			 *	for pad bytes; really gross).
			 * 2. overwrite some and append.
			 * 3. strictly overwrite.
			 */
			if (nval->doff > nondup_size)
				newsize +=
				    ((nval->doff - nondup_size) + nval->size);
			else if (nval->doff + nval->dlen > nondup_size)
				newsize += nval->size -
				    (nondup_size - nval->doff);
			else
				newsize += nval->size - nval->dlen;

			/*
			 * Make sure that the new size doesn't put us over
			 * the onpage duplicate size in which case we need
			 * to convert to off-page duplicates.
			 */
			if (ISBIG(hcp,
			    (hcp->dup_tlen - nondup_size) + newsize)) {
				if ((ret = __ham_dup_convert(dbc)) != 0)
					return (ret);
				return (hcp->opd->c_am_put(hcp->opd,
				    NULL, nval, flags, NULL));
			}

			if ((ret = __os_malloc(dbp->dbenv,
			    DUP_SIZE(newsize), &newrec)) != 0)
				return (ret);
			memset(&tmp_val2, 0, sizeof(tmp_val2));
			F_SET(&tmp_val2, DB_DBT_PARTIAL);

			/* Construct the record. */
			p = newrec;
			/* Initial size. */
			memcpy(p, &newsize, sizeof(db_indx_t));
			p += sizeof(db_indx_t);

			/* First part of original record. */
			len = nval->doff > tmp_val.size
			    ? tmp_val.size : nval->doff;
			memcpy(p, tmp_val.data, len);
			p += len;

			if (nval->doff > tmp_val.size) {
				/* Padding */
				memset(p, 0, nval->doff - tmp_val.size);
				p += nval->doff - tmp_val.size;
			}

			/* New bytes */
			memcpy(p, nval->data, nval->size);
			p += nval->size;

			/* End of original record (if there is any) */
			if (nval->doff + nval->dlen < tmp_val.size) {
				len = (tmp_val.size - nval->doff) - nval->dlen;
				memcpy(p, (u_int8_t *)tmp_val.data +
				    nval->doff + nval->dlen, len);
				p += len;
			}

			/* Final size. */
			memcpy(p, &newsize, sizeof(db_indx_t));

			/*
			 * Make sure that the caller isn't corrupting
			 * the sort order.
			 */
			if (dbp->dup_compare != NULL) {
				tmp_val2.data =
				    (u_int8_t *)newrec + sizeof(db_indx_t);
				tmp_val2.size = newsize;
				if (dbp->dup_compare(
				    dbp, &tmp_val, &tmp_val2) != 0) {
					__os_free(dbenv, newrec);
					return (__db_duperr(dbp, flags));
				}
			}

			tmp_val2.data = newrec;
			tmp_val2.size = DUP_SIZE(newsize);
			tmp_val2.doff = hcp->dup_off;
			tmp_val2.dlen = DUP_SIZE(hcp->dup_len);

			ret = __ham_replpair(dbc, &tmp_val2, 0);
			__os_free(dbenv, newrec);

			/* Update cursor */
			if (ret != 0)
				return (ret);

			if (newsize > nondup_size)
				hcp->dup_tlen += (newsize - nondup_size);
			else
				hcp->dup_tlen -= (nondup_size - newsize);
			hcp->dup_len = newsize;
			return (0);
		} else {
			/* Check whether we need to convert to off page. */
			if (ISBIG(hcp,
			    (hcp->dup_tlen - hcp->dup_len) + nval->size)) {
				if ((ret = __ham_dup_convert(dbc)) != 0)
					return (ret);
				return (hcp->opd->c_am_put(hcp->opd,
				    NULL, nval, flags, NULL));
			}

			/* Make sure we maintain sort order. */
			if (dbp->dup_compare != NULL) {
				tmp_val2.data =
				    HKEYDATA_DATA(H_PAIRDATA(dbp, hcp->page,
				    hcp->indx)) + hcp->dup_off +
				    sizeof(db_indx_t);
				tmp_val2.size = hcp->dup_len;
				if (dbp->dup_compare(
				    dbp, nval, &tmp_val2) != 0) {
					__db_err(dbenv,
			"Existing data sorts differently from put data");
					return (EINVAL);
				}
			}
			/* Overwriting a complete duplicate. */
			if ((ret =
			    __ham_make_dup(dbp->dbenv, nval, &tmp_val,
			    &dbc->my_rdata.data, &dbc->my_rdata.ulen)) != 0)
				return (ret);
			/* Now fix what we are replacing. */
			tmp_val.doff = hcp->dup_off;
			tmp_val.dlen = DUP_SIZE(hcp->dup_len);

			/* Update cursor */
			if (nval->size > hcp->dup_len)
				hcp->dup_tlen += (nval->size - hcp->dup_len);
			else
				hcp->dup_tlen -= (hcp->dup_len - nval->size);
			hcp->dup_len = (db_indx_t)nval->size;
		}
		myval = &tmp_val;
	} else if (!F_ISSET(nval, DB_DBT_PARTIAL)) {
		/* Put/overwrite */
		memcpy(&tmp_val, nval, sizeof(*nval));
		F_SET(&tmp_val, DB_DBT_PARTIAL);
		tmp_val.doff = 0;
		hk = H_PAIRDATA(dbp, hcp->page, hcp->indx);
		if (HPAGE_PTYPE(hk) == H_OFFPAGE)
			memcpy(&tmp_val.dlen,
			    HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
		else
			tmp_val.dlen = LEN_HDATA(dbp, hcp->page,
			    hcp->hdr->dbmeta.pagesize, hcp->indx);
		myval = &tmp_val;
	} else
		/* Regular partial put */
		myval = nval;

	return (__ham_replpair(dbc, myval, 0));
}

/*
 * Given a key and a cursor, sets the cursor to the page/ndx on which
 * the key resides.  If the key is found, the cursor H_OK flag is set
 * and the pagep, bndx, pgno (dpagep, dndx, dpgno) fields are set.
 * If the key is not found, the H_OK flag is not set.  If the sought
 * field is non-0, the pagep, bndx, pgno (dpagep, dndx, dpgno) fields
 * are set indicating where an add might take place.  If it is 0,
 * non of the cursor pointer field are valid.
 */
static int
__ham_lookup(dbc, key, sought, mode, pgnop)
	DBC *dbc;
	const DBT *key;
	u_int32_t sought;
	db_lockmode_t mode;
	db_pgno_t *pgnop;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	db_pgno_t pgno;
	u_int32_t tlen;
	int match, ret;
	u_int8_t *hk, *dk;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;
	/*
	 * Set up cursor so that we're looking for space to add an item
	 * as we cycle through the pages looking for the key.
	 */
	if ((ret = __ham_item_reset(dbc)) != 0)
		return (ret);
	hcp->seek_size = sought;

	hcp->bucket = __ham_call_hash(dbc, (u_int8_t *)key->data, key->size);
	hcp->pgno = BUCKET_TO_PAGE(hcp, hcp->bucket);

	for (;;) {
		*pgnop = PGNO_INVALID;
		if ((ret = __ham_item_next(dbc, mode, pgnop)) != 0)
			return (ret);

		if (F_ISSET(hcp, H_NOMORE))
			break;

		hk = H_PAIRKEY(dbp, hcp->page, hcp->indx);
		switch (HPAGE_PTYPE(hk)) {
		case H_OFFPAGE:
			memcpy(&tlen, HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
			if (tlen == key->size) {
				memcpy(&pgno,
				    HOFFPAGE_PGNO(hk), sizeof(db_pgno_t));
				if ((ret = __db_moff(dbp,
				    key, pgno, tlen, NULL, &match)) != 0)
					return (ret);
				if (match == 0)
					goto found_key;
			}
			break;
		case H_KEYDATA:
			if (key->size ==
			    LEN_HKEY(dbp, hcp->page, dbp->pgsize, hcp->indx) &&
			    memcmp(key->data,
			    HKEYDATA_DATA(hk), key->size) == 0) {
				/* Found the key, check for data type. */
found_key:			F_SET(hcp, H_OK);
				dk = H_PAIRDATA(dbp, hcp->page, hcp->indx);
				if (HPAGE_PTYPE(dk) == H_OFFDUP)
					memcpy(pgnop, HOFFDUP_PGNO(dk),
					    sizeof(db_pgno_t));
				return (0);
			}
			break;
		case H_DUPLICATE:
		case H_OFFDUP:
			/*
			 * These are errors because keys are never
			 * duplicated, only data items are.
			 */
			return (__db_pgfmt(dbp->dbenv, PGNO(hcp->page)));
		default:
			return (__db_pgfmt(dbp->dbenv, PGNO(hcp->page)));
		}
	}

	/*
	 * Item was not found.
	 */

	if (sought != 0)
		return (ret);

	return (ret);
}

/*
 * __ham_init_dbt --
 *	Initialize a dbt using some possibly already allocated storage
 *	for items.
 *
 * PUBLIC: int __ham_init_dbt __P((DB_ENV *,
 * PUBLIC:     DBT *, u_int32_t, void **, u_int32_t *));
 */
int
__ham_init_dbt(dbenv, dbt, size, bufp, sizep)
	DB_ENV *dbenv;
	DBT *dbt;
	u_int32_t size;
	void **bufp;
	u_int32_t *sizep;
{
	int ret;

	memset(dbt, 0, sizeof(*dbt));
	if (*sizep < size) {
		if ((ret = __os_realloc(dbenv, size, bufp)) != 0) {
			*sizep = 0;
			return (ret);
		}
		*sizep = size;
	}
	dbt->data = *bufp;
	dbt->size = size;
	return (0);
}

/*
 * Adjust the cursor after an insert or delete.  The cursor passed is
 * the one that was operated upon; we just need to check any of the
 * others.
 *
 * len indicates the length of the item added/deleted
 * add indicates if the item indicated by the cursor has just been
 * added (add == 1) or deleted (add == 0).
 * dup indicates if the addition occurred into a duplicate set.
 *
 * PUBLIC: int __ham_c_update
 * PUBLIC:    __P((DBC *, u_int32_t, int, int));
 */
int
__ham_c_update(dbc, len, add, is_dup)
	DBC *dbc;
	u_int32_t len;
	int add, is_dup;
{
	DB *dbp, *ldbp;
	DBC *cp;
	DB_ENV *dbenv;
	DB_LSN lsn;
	DB_TXN *my_txn;
	HASH_CURSOR *hcp, *lcp;
	int found, ret;
	u_int32_t order;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;
	hcp = (HASH_CURSOR *)dbc->internal;

	/*
	 * Adjustment will only be logged if this is a subtransaction.
	 * Only subtransactions can abort and effect their parent
	 * transactions cursors.
	 */

	my_txn = IS_SUBTRANSACTION(dbc->txn) ? dbc->txn : NULL;
	found = 0;

	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);

	/*
	 * Calculate the order of this deleted record.
	 * This will be one greater than any cursor that is pointing
	 * at this record and already marked as deleted.
	 */
	order = 0;
	if (!add) {
		order = 1;
		for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
		    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
		    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
			MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
			for (cp = TAILQ_FIRST(&ldbp->active_queue); cp != NULL;
			    cp = TAILQ_NEXT(cp, links)) {
				if (cp == dbc || cp->dbtype != DB_HASH)
					continue;
				lcp = (HASH_CURSOR *)cp->internal;
				if (F_ISSET(lcp, H_DELETED) &&
				    hcp->pgno == lcp->pgno &&
				    hcp->indx == lcp->indx &&
				    order <= lcp->order &&
				    (!is_dup || hcp->dup_off == lcp->dup_off))
					order = lcp->order + 1;
			}
			MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
		}
		hcp->order = order;
	}

	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (cp = TAILQ_FIRST(&ldbp->active_queue); cp != NULL;
		    cp = TAILQ_NEXT(cp, links)) {
			if (cp == dbc || cp->dbtype != DB_HASH)
				continue;

			lcp = (HASH_CURSOR *)cp->internal;

			if (lcp->pgno != hcp->pgno || lcp->indx == NDX_INVALID)
				continue;

			if (my_txn != NULL && cp->txn != my_txn)
				found = 1;

			if (!is_dup) {
				if (add) {
					/*
					 * This routine is not called to add
					 * non-dup records which are always put
					 * at the end.  It is only called from
					 * recovery in this case and the
					 * cursor will be marked deleted.
					 * We are "undeleting" so unmark all
					 * cursors with the same order.
					 */
					if (lcp->indx == hcp->indx &&
					    F_ISSET(lcp, H_DELETED)) {
						if (lcp->order == hcp->order)
							F_CLR(lcp, H_DELETED);
						else if (lcp->order >
						    hcp->order) {

						/*
						 * If we've moved this cursor's
						 * index, split its order
						 * number--i.e., decrement it by
						 * enough so that the lowest
						 * cursor moved has order 1.
						 * cp_arg->order is the split
						 * point, so decrement by one
						 * less than that.
						 */
							lcp->order -=
							    (hcp->order - 1);
							lcp->indx += 2;
						}
					} else if (lcp->indx >= hcp->indx)
						lcp->indx += 2;

				} else {
					if (lcp->indx > hcp->indx) {
						lcp->indx -= 2;
						if (lcp->indx == hcp->indx &&
						    F_ISSET(lcp, H_DELETED))
							lcp->order += order;
					} else if (lcp->indx == hcp->indx &&
					    !F_ISSET(lcp, H_DELETED)) {
						F_SET(lcp, H_DELETED);
						F_CLR(lcp, H_ISDUP);
						lcp->order = order;
					}
				}
			} else if (lcp->indx == hcp->indx) {
				/*
				 * Handle duplicates.  This routine is
				 * only called for on page dups.
				 * Off page dups are handled by btree/rtree
				 * code.
				 */
				if (add) {
					lcp->dup_tlen += len;
					if (lcp->dup_off == hcp->dup_off &&
					    F_ISSET(hcp, H_DELETED) &&
					    F_ISSET(lcp, H_DELETED)) {
						/* Abort of a delete. */
						if (lcp->order == hcp->order)
							F_CLR(lcp, H_DELETED);
						else if (lcp->order >
						    hcp->order) {
							lcp->order -=
							    (hcp->order -1);
							lcp->dup_off += len;
						}
					} else if (lcp->dup_off >= hcp->dup_off)
						lcp->dup_off += len;
				} else {
					lcp->dup_tlen -= len;
					if (lcp->dup_off > hcp->dup_off) {
						lcp->dup_off -= len;
						if (lcp->dup_off ==
						    hcp->dup_off &&
						    F_ISSET(lcp, H_DELETED))
							lcp->order += order;
					} else if (lcp->dup_off ==
					    hcp->dup_off &&
					    !F_ISSET(lcp, H_DELETED)) {
						F_SET(lcp, H_DELETED);
						lcp->order = order;
					}
				}
			}
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	if (found != 0 && DBC_LOGGING(dbc)) {
		if ((ret = __ham_curadj_log(dbp, my_txn, &lsn, 0, hcp->pgno,
		    hcp->indx, len, hcp->dup_off, add, is_dup, order)) != 0)
			return (ret);
	}

	return (0);
}

/*
 * __ham_get_clist --
 *
 * Get a list of cursors either on a particular bucket or on a particular
 * page and index combination.  The former is so that we can update
 * cursors on a split.  The latter is so we can update cursors when we
 * move items off page.
 *
 * PUBLIC: int __ham_get_clist __P((DB *, db_pgno_t, u_int32_t, DBC ***));
 */
int
__ham_get_clist(dbp, pgno, indx, listp)
	DB *dbp;
	db_pgno_t pgno;
	u_int32_t indx;
	DBC ***listp;
{
	DB *ldbp;
	DBC *cp;
	DB_ENV *dbenv;
	u_int nalloc, nused;
	int ret;

	/*
	 * Assume that finding anything is the exception, so optimize for
	 * the case where there aren't any.
	 */
	nalloc = nused = 0;
	*listp = NULL;
	dbenv = dbp->dbenv;

	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (cp = TAILQ_FIRST(&ldbp->active_queue); cp != NULL;
		    cp = TAILQ_NEXT(cp, links))
			/*
			 * We match if cp->pgno matches the specified
			 * pgno, and if either the cp->indx matches
			 * or we weren't given an index.
			 */
			if (cp->internal->pgno == pgno &&
			    (indx == NDX_INVALID ||
			    cp->internal->indx == indx)) {
				if (nused >= nalloc) {
					nalloc += 10;
					if ((ret = __os_realloc(dbp->dbenv,
					    nalloc * sizeof(HASH_CURSOR *),
					    listp)) != 0)
						goto err;
				}
				(*listp)[nused++] = cp;
			}

		MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	if (listp != NULL) {
		if (nused >= nalloc) {
			nalloc++;
			if ((ret = __os_realloc(dbp->dbenv,
			    nalloc * sizeof(HASH_CURSOR *), listp)) != 0)
				return (ret);
		}
		(*listp)[nused] = NULL;
	}
	return (0);
err:
	MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);
	return (ret);
}

static int
__ham_c_writelock(dbc)
	DBC *dbc;
{
	DB_LOCK tmp_lock;
	HASH_CURSOR *hcp;
	int ret;

	/*
	 * All we need do is acquire the lock and let the off-page
	 * dup tree do its thing.
	 */
	if (!STD_LOCKING(dbc))
		return (0);

	hcp = (HASH_CURSOR *)dbc->internal;
	ret = 0;
	if ((!LOCK_ISSET(hcp->lock) || hcp->lock_mode != DB_LOCK_WRITE)) {
		tmp_lock = hcp->lock;
		if ((ret = __ham_lock_bucket(dbc, DB_LOCK_WRITE)) == 0 &&
		    tmp_lock.mode != DB_LOCK_WWRITE)
			ret = __LPUT(dbc, tmp_lock);
	}
	return (ret);
}
