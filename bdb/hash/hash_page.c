/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
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
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: hash_page.c,v 11.46 2001/01/11 18:19:51 bostic Exp $";
#endif /* not lint */

/*
 * PACKAGE:  hashing
 *
 * DESCRIPTION:
 *      Page manipulation for hashing package.
 *
 * ROUTINES:
 *
 * External
 *      __get_page
 *      __add_ovflpage
 *      __overflow_page
 * Internal
 *      open_temp
 */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "hash.h"
#include "lock.h"
#include "txn.h"

/*
 * PUBLIC: int __ham_item __P((DBC *, db_lockmode_t, db_pgno_t *));
 */
int
__ham_item(dbc, mode, pgnop)
	DBC *dbc;
	db_lockmode_t mode;
	db_pgno_t *pgnop;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	db_pgno_t next_pgno;
	int ret;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;

	if (F_ISSET(hcp, H_DELETED)) {
		__db_err(dbp->dbenv, "Attempt to return a deleted item");
		return (EINVAL);
	}
	F_CLR(hcp, H_OK | H_NOMORE);

	/* Check if we need to get a page for this cursor. */
	if ((ret = __ham_get_cpage(dbc, mode)) != 0)
		return (ret);

recheck:
	/* Check if we are looking for space in which to insert an item. */
	if (hcp->seek_size && hcp->seek_found_page == PGNO_INVALID
	    && hcp->seek_size < P_FREESPACE(hcp->page))
		hcp->seek_found_page = hcp->pgno;

	/* Check for off-page duplicates. */
	if (hcp->indx < NUM_ENT(hcp->page) &&
	    HPAGE_TYPE(hcp->page, H_DATAINDEX(hcp->indx)) == H_OFFDUP) {
		memcpy(pgnop,
		    HOFFDUP_PGNO(H_PAIRDATA(hcp->page, hcp->indx)),
		    sizeof(db_pgno_t));
		F_SET(hcp, H_OK);
		return (0);
	}

	/* Check if we need to go on to the next page. */
	if (F_ISSET(hcp, H_ISDUP))
		/*
		 * ISDUP is set, and offset is at the beginning of the datum.
		 * We need to grab the length of the datum, then set the datum
		 * pointer to be the beginning of the datum.
		 */
		memcpy(&hcp->dup_len,
		    HKEYDATA_DATA(H_PAIRDATA(hcp->page, hcp->indx)) +
		    hcp->dup_off, sizeof(db_indx_t));

	if (hcp->indx >= (db_indx_t)NUM_ENT(hcp->page)) {
		/* Fetch next page. */
		if (NEXT_PGNO(hcp->page) == PGNO_INVALID) {
			F_SET(hcp, H_NOMORE);
			return (DB_NOTFOUND);
		}
		next_pgno = NEXT_PGNO(hcp->page);
		hcp->indx = 0;
		if ((ret = __ham_next_cpage(dbc, next_pgno, 0)) != 0)
			return (ret);
		goto recheck;
	}

	F_SET(hcp, H_OK);
	return (0);
}

/*
 * PUBLIC: int __ham_item_reset __P((DBC *));
 */
int
__ham_item_reset(dbc)
	DBC *dbc;
{
	HASH_CURSOR *hcp;
	DB *dbp;
	int ret;

	ret = 0;
	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;
	if (hcp->page != NULL)
		ret = memp_fput(dbp->mpf, hcp->page, 0);

	__ham_item_init(dbc);
	return (ret);
}

/*
 * PUBLIC: void __ham_item_init __P((DBC *));
 */
void
__ham_item_init(dbc)
	DBC *dbc;
{
	HASH_CURSOR *hcp;

	hcp = (HASH_CURSOR *)dbc->internal;
	/*
	 * If this cursor still holds any locks, we must
	 * release them if we are not running with transactions.
	 */
	if (hcp->lock.off != LOCK_INVALID && dbc->txn == NULL)
	    (void)lock_put(dbc->dbp->dbenv, &hcp->lock);

	/*
	 * The following fields must *not* be initialized here
	 * because they may have meaning across inits.
	 *	hlock, hdr, split_buf, stats
	 */
	hcp->bucket = BUCKET_INVALID;
	hcp->lbucket = BUCKET_INVALID;
	hcp->lock.off = LOCK_INVALID;
	hcp->lock_mode = DB_LOCK_NG;
	hcp->dup_off = 0;
	hcp->dup_len = 0;
	hcp->dup_tlen = 0;
	hcp->seek_size = 0;
	hcp->seek_found_page = PGNO_INVALID;
	hcp->flags = 0;

	hcp->pgno = PGNO_INVALID;
	hcp->indx = NDX_INVALID;
	hcp->page = NULL;
}

/*
 * Returns the last item in a bucket.
 *
 * PUBLIC: int __ham_item_last __P((DBC *, db_lockmode_t, db_pgno_t *));
 */
int
__ham_item_last(dbc, mode, pgnop)
	DBC *dbc;
	db_lockmode_t mode;
	db_pgno_t *pgnop;
{
	HASH_CURSOR *hcp;
	int ret;

	hcp = (HASH_CURSOR *)dbc->internal;
	if ((ret = __ham_item_reset(dbc)) != 0)
		return (ret);

	hcp->bucket = hcp->hdr->max_bucket;
	hcp->pgno = BUCKET_TO_PAGE(hcp, hcp->bucket);
	F_SET(hcp, H_OK);
	return (__ham_item_prev(dbc, mode, pgnop));
}

/*
 * PUBLIC: int __ham_item_first __P((DBC *, db_lockmode_t, db_pgno_t *));
 */
int
__ham_item_first(dbc, mode, pgnop)
	DBC *dbc;
	db_lockmode_t mode;
	db_pgno_t *pgnop;
{
	HASH_CURSOR *hcp;
	int ret;

	hcp = (HASH_CURSOR *)dbc->internal;
	if ((ret = __ham_item_reset(dbc)) != 0)
		return (ret);
	F_SET(hcp, H_OK);
	hcp->bucket = 0;
	hcp->pgno = BUCKET_TO_PAGE(hcp, hcp->bucket);
	return (__ham_item_next(dbc, mode, pgnop));
}

/*
 * __ham_item_prev --
 *	Returns a pointer to key/data pair on a page.  In the case of
 *	bigkeys, just returns the page number and index of the bigkey
 *	pointer pair.
 *
 * PUBLIC: int __ham_item_prev __P((DBC *, db_lockmode_t, db_pgno_t *));
 */
int
__ham_item_prev(dbc, mode, pgnop)
	DBC *dbc;
	db_lockmode_t mode;
	db_pgno_t *pgnop;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	db_pgno_t next_pgno;
	int ret;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;
	/*
	 * There are 5 cases for backing up in a hash file.
	 * Case 1: In the middle of a page, no duplicates, just dec the index.
	 * Case 2: In the middle of a duplicate set, back up one.
	 * Case 3: At the beginning of a duplicate set, get out of set and
	 *	back up to next key.
	 * Case 4: At the beginning of a page; go to previous page.
	 * Case 5: At the beginning of a bucket; go to prev bucket.
	 */
	F_CLR(hcp, H_OK | H_NOMORE | H_DELETED);

	if ((ret = __ham_get_cpage(dbc, mode)) != 0)
		return (ret);

	/*
	 * First handle the duplicates.  Either you'll get the key here
	 * or you'll exit the duplicate set and drop into the code below
	 * to handle backing up through keys.
	 */
	if (!F_ISSET(hcp, H_NEXT_NODUP) && F_ISSET(hcp, H_ISDUP)) {
		if (HPAGE_TYPE(hcp->page, H_DATAINDEX(hcp->indx)) == H_OFFDUP) {
			memcpy(pgnop,
			    HOFFDUP_PGNO(H_PAIRDATA(hcp->page, hcp->indx)),
			    sizeof(db_pgno_t));
			F_SET(hcp, H_OK);
			return (0);
		}

		/* Duplicates are on-page. */
		if (hcp->dup_off != 0) {
			memcpy(&hcp->dup_len, HKEYDATA_DATA(
			    H_PAIRDATA(hcp->page, hcp->indx))
			    + hcp->dup_off - sizeof(db_indx_t),
			    sizeof(db_indx_t));
			hcp->dup_off -=
			    DUP_SIZE(hcp->dup_len);
			return (__ham_item(dbc, mode, pgnop));
		}
	}

	/*
	 * If we get here, we are not in a duplicate set, and just need
	 * to back up the cursor.  There are still three cases:
	 * midpage, beginning of page, beginning of bucket.
	 */

	if (F_ISSET(hcp, H_DUPONLY)) {
		F_CLR(hcp, H_OK);
		F_SET(hcp, H_NOMORE);
		return (0);
	} else
		/*
		 * We are no longer in a dup set;  flag this so the dup code
		 * will reinitialize should we stumble upon another one.
		 */
		F_CLR(hcp, H_ISDUP);

	if (hcp->indx == 0) {		/* Beginning of page. */
		hcp->pgno = PREV_PGNO(hcp->page);
		if (hcp->pgno == PGNO_INVALID) {
			/* Beginning of bucket. */
			F_SET(hcp, H_NOMORE);
			return (DB_NOTFOUND);
		} else if ((ret =
		    __ham_next_cpage(dbc, hcp->pgno, 0)) != 0)
			return (ret);
		else
			hcp->indx = NUM_ENT(hcp->page);
	}

	/*
	 * Either we've got the cursor set up to be decremented, or we
	 * have to find the end of a bucket.
	 */
	if (hcp->indx == NDX_INVALID) {
		DB_ASSERT(hcp->page != NULL);

		hcp->indx = NUM_ENT(hcp->page);
		for (next_pgno = NEXT_PGNO(hcp->page);
		    next_pgno != PGNO_INVALID;
		    next_pgno = NEXT_PGNO(hcp->page)) {
			if ((ret = __ham_next_cpage(dbc, next_pgno, 0)) != 0)
				return (ret);
			hcp->indx = NUM_ENT(hcp->page);
		}

		if (hcp->indx == 0) {
			/* Bucket was empty. */
			F_SET(hcp, H_NOMORE);
			return (DB_NOTFOUND);
		}
	}

	hcp->indx -= 2;

	return (__ham_item(dbc, mode, pgnop));
}

/*
 * Sets the cursor to the next key/data pair on a page.
 *
 * PUBLIC: int __ham_item_next __P((DBC *, db_lockmode_t, db_pgno_t *));
 */
int
__ham_item_next(dbc, mode, pgnop)
	DBC *dbc;
	db_lockmode_t mode;
	db_pgno_t *pgnop;
{
	HASH_CURSOR *hcp;
	int ret;

	hcp = (HASH_CURSOR *)dbc->internal;

	if ((ret = __ham_get_cpage(dbc, mode)) != 0)
		return (ret);

	/*
	 * Deleted on-page duplicates are a weird case. If we delete the last
	 * one, then our cursor is at the very end of a duplicate set and
	 * we actually need to go on to the next key.
	 */
	if (F_ISSET(hcp, H_DELETED)) {
		if (hcp->indx != NDX_INVALID &&
		    F_ISSET(hcp, H_ISDUP) &&
		    HPAGE_TYPE(hcp->page, H_DATAINDEX(hcp->indx))
			== H_DUPLICATE && hcp->dup_tlen == hcp->dup_off) {
			if (F_ISSET(hcp, H_DUPONLY)) {
				F_CLR(hcp, H_OK);
				F_SET(hcp, H_NOMORE);
				return (0);
			} else {
				F_CLR(hcp, H_ISDUP);
				hcp->indx += 2;
			}
		} else if (!F_ISSET(hcp, H_ISDUP) && F_ISSET(hcp, H_DUPONLY)) {
			F_CLR(hcp, H_OK);
			F_SET(hcp, H_NOMORE);
			return (0);
		} else if (F_ISSET(hcp, H_ISDUP) &&
		    F_ISSET(hcp, H_NEXT_NODUP)) {
			F_CLR(hcp, H_ISDUP);
			hcp->indx += 2;
		}
		F_CLR(hcp, H_DELETED);
	} else if (hcp->indx == NDX_INVALID) {
		hcp->indx = 0;
		F_CLR(hcp, H_ISDUP);
	} else if (F_ISSET(hcp, H_NEXT_NODUP)) {
		hcp->indx += 2;
		F_CLR(hcp, H_ISDUP);
	} else if (F_ISSET(hcp, H_ISDUP) && hcp->dup_tlen != 0) {
		if (hcp->dup_off + DUP_SIZE(hcp->dup_len) >=
		    hcp->dup_tlen && F_ISSET(hcp, H_DUPONLY)) {
			F_CLR(hcp, H_OK);
			F_SET(hcp, H_NOMORE);
			return (0);
		}
		hcp->dup_off += DUP_SIZE(hcp->dup_len);
		if (hcp->dup_off >= hcp->dup_tlen) {
			F_CLR(hcp, H_ISDUP);
			hcp->indx += 2;
		}
	} else if (F_ISSET(hcp, H_DUPONLY)) {
		F_CLR(hcp, H_OK);
		F_SET(hcp, H_NOMORE);
		return (0);
	} else {
		hcp->indx += 2;
		F_CLR(hcp, H_ISDUP);
	}

	return (__ham_item(dbc, mode, pgnop));
}

/*
 * PUBLIC: void __ham_putitem __P((PAGE *p, const DBT *, int));
 *
 * This is a little bit sleazy in that we're overloading the meaning
 * of the H_OFFPAGE type here.  When we recover deletes, we have the
 * entire entry instead of having only the DBT, so we'll pass type
 * H_OFFPAGE to mean, "copy the whole entry" as opposed to constructing
 * an H_KEYDATA around it.
 */
void
__ham_putitem(p, dbt, type)
	PAGE *p;
	const DBT *dbt;
	int type;
{
	u_int16_t n, off;

	n = NUM_ENT(p);

	/* Put the item element on the page. */
	if (type == H_OFFPAGE) {
		off = HOFFSET(p) - dbt->size;
		HOFFSET(p) = p->inp[n] = off;
		memcpy(P_ENTRY(p, n), dbt->data, dbt->size);
	} else {
		off = HOFFSET(p) - HKEYDATA_SIZE(dbt->size);
		HOFFSET(p) = p->inp[n] = off;
		PUT_HKEYDATA(P_ENTRY(p, n), dbt->data, dbt->size, type);
	}

	/* Adjust page info. */
	NUM_ENT(p) += 1;
}

/*
 * PUBLIC: void __ham_reputpair
 * PUBLIC:    __P((PAGE *p, u_int32_t, u_int32_t, const DBT *, const DBT *));
 *
 * This is a special case to restore a key/data pair to its original
 * location during recovery.  We are guaranteed that the pair fits
 * on the page and is not the last pair on the page (because if it's
 * the last pair, the normal insert works).
 */
void
__ham_reputpair(p, psize, ndx, key, data)
	PAGE *p;
	u_int32_t psize, ndx;
	const DBT *key, *data;
{
	db_indx_t i, movebytes, newbytes;
	u_int8_t *from;

	/* First shuffle the existing items up on the page.  */
	movebytes =
	    (ndx == 0 ? psize : p->inp[H_DATAINDEX(ndx - 2)]) - HOFFSET(p);
	newbytes = key->size + data->size;
	from = (u_int8_t *)p + HOFFSET(p);
	memmove(from - newbytes, from, movebytes);

	/*
	 * Adjust the indices and move them up 2 spaces. Note that we
	 * have to check the exit condition inside the loop just in case
	 * we are dealing with index 0 (db_indx_t's are unsigned).
	 */
	for (i = NUM_ENT(p) - 1; ; i-- ) {
		p->inp[i + 2] = p->inp[i] - newbytes;
		if (i == H_KEYINDEX(ndx))
			break;
	}

	/* Put the key and data on the page. */
	p->inp[H_KEYINDEX(ndx)] =
	    (ndx == 0 ? psize : p->inp[H_DATAINDEX(ndx - 2)]) - key->size;
	p->inp[H_DATAINDEX(ndx)] = p->inp[H_KEYINDEX(ndx)] - data->size;
	memcpy(P_ENTRY(p, H_KEYINDEX(ndx)), key->data, key->size);
	memcpy(P_ENTRY(p, H_DATAINDEX(ndx)), data->data, data->size);

	/* Adjust page info. */
	HOFFSET(p) -= newbytes;
	NUM_ENT(p) += 2;
}

/*
 * PUBLIC: int __ham_del_pair __P((DBC *, int));
 */
int
__ham_del_pair(dbc, reclaim_page)
	DBC *dbc;
	int reclaim_page;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	DBT data_dbt, key_dbt;
	DB_ENV *dbenv;
	DB_LSN new_lsn, *n_lsn, tmp_lsn;
	PAGE *n_pagep, *nn_pagep, *p, *p_pagep;
	db_indx_t ndx;
	db_pgno_t chg_pgno, pgno, tmp_pgno;
	int ret, t_ret;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;

	dbenv = dbp->dbenv;
	ndx = hcp->indx;

	n_pagep = p_pagep = nn_pagep = NULL;

	if (hcp->page == NULL && (ret = memp_fget(dbp->mpf,
	    &hcp->pgno, DB_MPOOL_CREATE, &hcp->page)) != 0)
		return (ret);
	p = hcp->page;

	/*
	 * We optimize for the normal case which is when neither the key nor
	 * the data are large.  In this case, we write a single log record
	 * and do the delete.  If either is large, we'll call __big_delete
	 * to remove the big item and then update the page to remove the
	 * entry referring to the big item.
	 */
	ret = 0;
	if (HPAGE_PTYPE(H_PAIRKEY(p, ndx)) == H_OFFPAGE) {
		memcpy(&pgno, HOFFPAGE_PGNO(P_ENTRY(p, H_KEYINDEX(ndx))),
		    sizeof(db_pgno_t));
		ret = __db_doff(dbc, pgno);
	}

	if (ret == 0)
		switch (HPAGE_PTYPE(H_PAIRDATA(p, ndx))) {
		case H_OFFPAGE:
			memcpy(&pgno,
			    HOFFPAGE_PGNO(P_ENTRY(p, H_DATAINDEX(ndx))),
			    sizeof(db_pgno_t));
			ret = __db_doff(dbc, pgno);
			break;
		case H_OFFDUP:
		case H_DUPLICATE:
			/*
			 * If we delete a pair that is/was a duplicate, then
			 * we had better clear the flag so that we update the
			 * cursor appropriately.
			 */
			F_CLR(hcp, H_ISDUP);
			break;
		}

	if (ret)
		return (ret);

	/* Now log the delete off this page. */
	if (DB_LOGGING(dbc)) {
		key_dbt.data = P_ENTRY(p, H_KEYINDEX(ndx));
		key_dbt.size = LEN_HITEM(p, dbp->pgsize, H_KEYINDEX(ndx));
		data_dbt.data = P_ENTRY(p, H_DATAINDEX(ndx));
		data_dbt.size = LEN_HITEM(p, dbp->pgsize, H_DATAINDEX(ndx));

		if ((ret = __ham_insdel_log(dbenv,
		    dbc->txn, &new_lsn, 0, DELPAIR,
		    dbp->log_fileid, PGNO(p), (u_int32_t)ndx,
		    &LSN(p), &key_dbt, &data_dbt)) != 0)
			return (ret);

		/* Move lsn onto page. */
		LSN(p) = new_lsn;
	}

	/* Do the delete. */
	__ham_dpair(dbp, p, ndx);

	/*
	 * Mark item deleted so that we don't try to return it, and
	 * so that we update the cursor correctly on the next call
	 * to next.
	 */
	F_SET(hcp, H_DELETED);
	F_CLR(hcp, H_OK);

	/*
	 * Update cursors that are on the page where the delete happend.
	 */
	if ((ret = __ham_c_update(dbc, 0, 0, 0)) != 0)
		return (ret);

	/*
	 * If we are locking, we will not maintain this, because it is
	 * a hot spot.
	 *
	 * XXX
	 * Perhaps we can retain incremental numbers and apply them later.
	 */
	if (!STD_LOCKING(dbc))
		--hcp->hdr->nelem;

	/*
	 * If we need to reclaim the page, then check if the page is empty.
	 * There are two cases.  If it's empty and it's not the first page
	 * in the bucket (i.e., the bucket page) then we can simply remove
	 * it. If it is the first chain in the bucket, then we need to copy
	 * the second page into it and remove the second page.
	 * If its the only page in the bucket we leave it alone.
	 */
	if (!reclaim_page ||
	    NUM_ENT(p) != 0 ||
	    (PREV_PGNO(p) == PGNO_INVALID && NEXT_PGNO(p) == PGNO_INVALID))
		return (memp_fset(dbp->mpf, p, DB_MPOOL_DIRTY));

	if (PREV_PGNO(p) == PGNO_INVALID) {
		/*
		 * First page in chain is empty and we know that there
		 * are more pages in the chain.
		 */
		if ((ret =
		    memp_fget(dbp->mpf, &NEXT_PGNO(p), 0, &n_pagep)) != 0)
			return (ret);

		if (NEXT_PGNO(n_pagep) != PGNO_INVALID &&
		    (ret = memp_fget(dbp->mpf, &NEXT_PGNO(n_pagep), 0,
		    &nn_pagep)) != 0)
			goto err;

		if (DB_LOGGING(dbc)) {
			key_dbt.data = n_pagep;
			key_dbt.size = dbp->pgsize;
			if ((ret = __ham_copypage_log(dbenv,
			    dbc->txn, &new_lsn, 0, dbp->log_fileid, PGNO(p),
			    &LSN(p), PGNO(n_pagep), &LSN(n_pagep),
			    NEXT_PGNO(n_pagep),
			    nn_pagep == NULL ? NULL : &LSN(nn_pagep),
			    &key_dbt)) != 0)
				goto err;

			/* Move lsn onto page. */
			LSN(p) = new_lsn;	/* Structure assignment. */
			LSN(n_pagep) = new_lsn;
			if (NEXT_PGNO(n_pagep) != PGNO_INVALID)
				LSN(nn_pagep) = new_lsn;
		}
		if (nn_pagep != NULL) {
			PREV_PGNO(nn_pagep) = PGNO(p);
			if ((ret = memp_fput(dbp->mpf,
			    nn_pagep, DB_MPOOL_DIRTY)) != 0) {
				nn_pagep = NULL;
				goto err;
			}
		}

		tmp_pgno = PGNO(p);
		tmp_lsn = LSN(p);
		memcpy(p, n_pagep, dbp->pgsize);
		PGNO(p) = tmp_pgno;
		LSN(p) = tmp_lsn;
		PREV_PGNO(p) = PGNO_INVALID;

		/*
		 * Update cursors to reflect the fact that records
		 * on the second page have moved to the first page.
		 */
		if ((ret = __ham_c_chgpg(dbc,
		    PGNO(n_pagep), NDX_INVALID, PGNO(p), NDX_INVALID)) != 0)
			return (ret);

		/*
		 * Update the cursor to reflect its new position.
		 */
		hcp->indx = 0;
		hcp->pgno = PGNO(p);
		if ((ret = memp_fset(dbp->mpf, p, DB_MPOOL_DIRTY)) != 0 ||
		    (ret = __db_free(dbc, n_pagep)) != 0)
			return (ret);
	} else {
		if ((ret =
		    memp_fget(dbp->mpf, &PREV_PGNO(p), 0, &p_pagep)) != 0)
			goto err;

		if (NEXT_PGNO(p) != PGNO_INVALID) {
			if ((ret = memp_fget(dbp->mpf,
			    &NEXT_PGNO(p), 0, &n_pagep)) != 0)
				goto err;
			n_lsn = &LSN(n_pagep);
		} else {
			n_pagep = NULL;
			n_lsn = NULL;
		}

		NEXT_PGNO(p_pagep) = NEXT_PGNO(p);
		if (n_pagep != NULL)
			PREV_PGNO(n_pagep) = PGNO(p_pagep);

		if (DB_LOGGING(dbc)) {
			if ((ret = __ham_newpage_log(dbenv,
			    dbc->txn, &new_lsn, 0, DELOVFL,
			    dbp->log_fileid, PREV_PGNO(p), &LSN(p_pagep),
			    PGNO(p), &LSN(p), NEXT_PGNO(p), n_lsn)) != 0)
				goto err;

			/* Move lsn onto page. */
			LSN(p_pagep) = new_lsn;	/* Structure assignment. */
			if (n_pagep)
				LSN(n_pagep) = new_lsn;
			LSN(p) = new_lsn;
		}
		if (NEXT_PGNO(p) == PGNO_INVALID) {
			/*
			 * There is no next page; put the cursor on the
			 * previous page as if we'd deleted the last item
			 * on that page; index greater than number of
			 * valid entries and H_DELETED set.
			 */
			hcp->pgno = PGNO(p_pagep);
			hcp->indx = NUM_ENT(p_pagep);
			F_SET(hcp, H_DELETED);
		} else {
			hcp->pgno = NEXT_PGNO(p);
			hcp->indx = 0;
		}

		/*
		 * Since we are about to delete the cursor page and we have
		 * just moved the cursor, we need to make sure that the
		 * old page pointer isn't left hanging around in the cursor.
		 */
		hcp->page = NULL;
		chg_pgno = PGNO(p);
		ret = __db_free(dbc, p);
		if ((t_ret = memp_fput(dbp->mpf, p_pagep, DB_MPOOL_DIRTY)) != 0
		    && ret == 0)
			ret = t_ret;
		if (n_pagep != NULL && (t_ret = memp_fput(dbp->mpf,
		    n_pagep, DB_MPOOL_DIRTY)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			return (ret);
		ret = __ham_c_chgpg(dbc,
		    chg_pgno, 0, hcp->pgno, hcp->indx);
	}
	return (ret);

err:	/* Clean up any pages. */
	if (n_pagep != NULL)
		(void)memp_fput(dbp->mpf, n_pagep, 0);
	if (nn_pagep != NULL)
		(void)memp_fput(dbp->mpf, nn_pagep, 0);
	if (p_pagep != NULL)
		(void)memp_fput(dbp->mpf, p_pagep, 0);
	return (ret);
}

/*
 * __ham_replpair --
 *	Given the key data indicated by the cursor, replace part/all of it
 *	according to the fields in the dbt.
 *
 * PUBLIC: int __ham_replpair __P((DBC *, DBT *, u_int32_t));
 */
int
__ham_replpair(dbc, dbt, make_dup)
	DBC *dbc;
	DBT *dbt;
	u_int32_t make_dup;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	DBT old_dbt, tdata, tmp;
	DB_LSN	new_lsn;
	int32_t change;			/* XXX: Possible overflow. */
	u_int32_t dup, len, memsize;
	int is_big, ret, type;
	u_int8_t *beg, *dest, *end, *hk, *src;
	void *memp;

	/*
	 * Big item replacements are handled in generic code.
	 * Items that fit on the current page fall into 4 classes.
	 * 1. On-page element, same size
	 * 2. On-page element, new is bigger (fits)
	 * 3. On-page element, new is bigger (does not fit)
	 * 4. On-page element, old is bigger
	 * Numbers 1, 2, and 4 are essentially the same (and should
	 * be the common case).  We handle case 3 as a delete and
	 * add.
	 */
	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;

	/*
	 * We need to compute the number of bytes that we are adding or
	 * removing from the entry.  Normally, we can simply substract
	 * the number of bytes we are replacing (dbt->dlen) from the
	 * number of bytes we are inserting (dbt->size).  However, if
	 * we are doing a partial put off the end of a record, then this
	 * formula doesn't work, because we are essentially adding
	 * new bytes.
	 */
	change = dbt->size - dbt->dlen;

	hk = H_PAIRDATA(hcp->page, hcp->indx);
	is_big = HPAGE_PTYPE(hk) == H_OFFPAGE;

	if (is_big)
		memcpy(&len, HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
	else
		len = LEN_HKEYDATA(hcp->page,
		    dbp->pgsize, H_DATAINDEX(hcp->indx));

	if (dbt->doff + dbt->dlen > len)
		change += dbt->doff + dbt->dlen - len;

	if (change > (int32_t)P_FREESPACE(hcp->page) || is_big) {
		/*
		 * Case 3 -- two subcases.
		 * A. This is not really a partial operation, but an overwrite.
		 *    Simple del and add works.
		 * B. This is a partial and we need to construct the data that
		 *    we are really inserting (yuck).
		 * In both cases, we need to grab the key off the page (in
		 * some cases we could do this outside of this routine; for
		 * cleanliness we do it here.  If you happen to be on a big
		 * key, this could be a performance hit).
		 */
		memset(&tmp, 0, sizeof(tmp));
		if ((ret =
		    __db_ret(dbp, hcp->page, H_KEYINDEX(hcp->indx),
		    &tmp, &dbc->rkey.data, &dbc->rkey.ulen)) != 0)
			return (ret);

		/* Preserve duplicate info. */
		dup = F_ISSET(hcp, H_ISDUP);
		if (dbt->doff == 0 && dbt->dlen == len) {
			ret = __ham_del_pair(dbc, 0);
			if (ret == 0)
			    ret = __ham_add_el(dbc,
				&tmp, dbt, dup ? H_DUPLICATE : H_KEYDATA);
		} else {					/* Case B */
			type = HPAGE_PTYPE(hk) != H_OFFPAGE ?
			    HPAGE_PTYPE(hk) : H_KEYDATA;
			memset(&tdata, 0, sizeof(tdata));
			memp = NULL;
			memsize = 0;
			if ((ret = __db_ret(dbp, hcp->page,
			    H_DATAINDEX(hcp->indx), &tdata, &memp, &memsize))
			    != 0)
				goto err;

			/* Now we can delete the item. */
			if ((ret = __ham_del_pair(dbc, 0)) != 0) {
				__os_free(memp, memsize);
				goto err;
			}

			/* Now shift old data around to make room for new. */
			if (change > 0) {
				 if ((ret = __os_realloc(dbp->dbenv,
				     tdata.size + change,
				     NULL, &tdata.data)) != 0)
					return (ret);
				memp = tdata.data;
				memsize = tdata.size + change;
				memset((u_int8_t *)tdata.data + tdata.size,
				    0, change);
			}
			end = (u_int8_t *)tdata.data + tdata.size;

			src = (u_int8_t *)tdata.data + dbt->doff + dbt->dlen;
			if (src < end && tdata.size > dbt->doff + dbt->dlen) {
				len = tdata.size - dbt->doff - dbt->dlen;
				dest = src + change;
				memmove(dest, src, len);
			}
			memcpy((u_int8_t *)tdata.data + dbt->doff,
			    dbt->data, dbt->size);
			tdata.size += change;

			/* Now add the pair. */
			ret = __ham_add_el(dbc, &tmp, &tdata, type);
			__os_free(memp, memsize);
		}
		F_SET(hcp, dup);
err:		return (ret);
	}

	/*
	 * Set up pointer into existing data. Do it before the log
	 * message so we can use it inside of the log setup.
	 */
	beg = HKEYDATA_DATA(H_PAIRDATA(hcp->page, hcp->indx));
	beg += dbt->doff;

	/*
	 * If we are going to have to move bytes at all, figure out
	 * all the parameters here.  Then log the call before moving
	 * anything around.
	 */
	if (DB_LOGGING(dbc)) {
		old_dbt.data = beg;
		old_dbt.size = dbt->dlen;
		if ((ret = __ham_replace_log(dbp->dbenv,
		    dbc->txn, &new_lsn, 0, dbp->log_fileid, PGNO(hcp->page),
		    (u_int32_t)H_DATAINDEX(hcp->indx), &LSN(hcp->page),
		    (u_int32_t)dbt->doff, &old_dbt, dbt, make_dup)) != 0)
			return (ret);

		LSN(hcp->page) = new_lsn;	/* Structure assignment. */
	}

	__ham_onpage_replace(hcp->page, dbp->pgsize,
	    (u_int32_t)H_DATAINDEX(hcp->indx), (int32_t)dbt->doff, change, dbt);

	return (0);
}

/*
 * Replace data on a page with new data, possibly growing or shrinking what's
 * there.  This is called on two different occasions. On one (from replpair)
 * we are interested in changing only the data.  On the other (from recovery)
 * we are replacing the entire data (header and all) with a new element.  In
 * the latter case, the off argument is negative.
 * pagep: the page that we're changing
 * ndx: page index of the element that is growing/shrinking.
 * off: Offset at which we are beginning the replacement.
 * change: the number of bytes (+ or -) that the element is growing/shrinking.
 * dbt: the new data that gets written at beg.
 * PUBLIC: void __ham_onpage_replace __P((PAGE *, size_t, u_int32_t, int32_t,
 * PUBLIC:     int32_t,  DBT *));
 */
void
__ham_onpage_replace(pagep, pgsize, ndx, off, change, dbt)
	PAGE *pagep;
	size_t pgsize;
	u_int32_t ndx;
	int32_t off;
	int32_t change;
	DBT *dbt;
{
	db_indx_t i;
	int32_t len;
	u_int8_t *src, *dest;
	int zero_me;

	if (change != 0) {
		zero_me = 0;
		src = (u_int8_t *)(pagep) + HOFFSET(pagep);
		if (off < 0)
			len = pagep->inp[ndx] - HOFFSET(pagep);
		else if ((u_int32_t)off >= LEN_HKEYDATA(pagep, pgsize, ndx)) {
			len = HKEYDATA_DATA(P_ENTRY(pagep, ndx)) +
			    LEN_HKEYDATA(pagep, pgsize, ndx) - src;
			zero_me = 1;
		} else
			len = (HKEYDATA_DATA(P_ENTRY(pagep, ndx)) + off) - src;
		dest = src - change;
		memmove(dest, src, len);
		if (zero_me)
			memset(dest + len, 0, change);

		/* Now update the indices. */
		for (i = ndx; i < NUM_ENT(pagep); i++)
			pagep->inp[i] -= change;
		HOFFSET(pagep) -= change;
	}
	if (off >= 0)
		memcpy(HKEYDATA_DATA(P_ENTRY(pagep, ndx)) + off,
		    dbt->data, dbt->size);
	else
		memcpy(P_ENTRY(pagep, ndx), dbt->data, dbt->size);
}

/*
 * PUBLIC: int __ham_split_page __P((DBC *, u_int32_t, u_int32_t));
 */
int
__ham_split_page(dbc, obucket, nbucket)
	DBC *dbc;
	u_int32_t obucket, nbucket;
{
	DB *dbp;
	DBC **carray;
	HASH_CURSOR *hcp, *cp;
	DBT key, page_dbt;
	DB_ENV *dbenv;
	DB_LSN new_lsn;
	PAGE **pp, *old_pagep, *temp_pagep, *new_pagep;
	db_indx_t n;
	db_pgno_t bucket_pgno, npgno, next_pgno;
	u_int32_t big_len, len;
	int found, i, ret, t_ret;
	void *big_buf;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;
	dbenv = dbp->dbenv;
	temp_pagep = old_pagep = new_pagep = NULL;

	if ((ret = __ham_get_clist(dbp, obucket, NDX_INVALID, &carray)) != 0)
		return (ret);

	bucket_pgno = BUCKET_TO_PAGE(hcp, obucket);
	if ((ret = memp_fget(dbp->mpf,
	    &bucket_pgno, DB_MPOOL_CREATE, &old_pagep)) != 0)
		goto err;

	/* Properly initialize the new bucket page. */
	npgno = BUCKET_TO_PAGE(hcp, nbucket);
	if ((ret = memp_fget(dbp->mpf,
	    &npgno, DB_MPOOL_CREATE, &new_pagep)) != 0)
		goto err;
	P_INIT(new_pagep,
	    dbp->pgsize, npgno, PGNO_INVALID, PGNO_INVALID, 0, P_HASH);

	temp_pagep = hcp->split_buf;
	memcpy(temp_pagep, old_pagep, dbp->pgsize);

	if (DB_LOGGING(dbc)) {
		page_dbt.size = dbp->pgsize;
		page_dbt.data = old_pagep;
		if ((ret = __ham_splitdata_log(dbenv,
		    dbc->txn, &new_lsn, 0, dbp->log_fileid, SPLITOLD,
		    PGNO(old_pagep), &page_dbt, &LSN(old_pagep))) != 0)
			goto err;
	}

	P_INIT(old_pagep, dbp->pgsize, PGNO(old_pagep), PGNO_INVALID,
	    PGNO_INVALID, 0, P_HASH);

	if (DB_LOGGING(dbc))
		LSN(old_pagep) = new_lsn;	/* Structure assignment. */

	big_len = 0;
	big_buf = NULL;
	key.flags = 0;
	while (temp_pagep != NULL) {
		for (n = 0; n < (db_indx_t)NUM_ENT(temp_pagep); n += 2) {
			if ((ret =
			    __db_ret(dbp, temp_pagep, H_KEYINDEX(n),
			    &key, &big_buf, &big_len)) != 0)
				goto err;

			if (__ham_call_hash(dbc, key.data, key.size)
			    == obucket)
				pp = &old_pagep;
			else
				pp = &new_pagep;

			/*
			 * Figure out how many bytes we need on the new
			 * page to store the key/data pair.
			 */

			len = LEN_HITEM(temp_pagep, dbp->pgsize,
			    H_DATAINDEX(n)) +
			    LEN_HITEM(temp_pagep, dbp->pgsize,
			    H_KEYINDEX(n)) +
			    2 * sizeof(db_indx_t);

			if (P_FREESPACE(*pp) < len) {
				if (DB_LOGGING(dbc)) {
					page_dbt.size = dbp->pgsize;
					page_dbt.data = *pp;
					if ((ret = __ham_splitdata_log(
					    dbenv, dbc->txn,
					    &new_lsn, 0, dbp->log_fileid,
					    SPLITNEW, PGNO(*pp), &page_dbt,
					    &LSN(*pp))) != 0)
						goto err;
					LSN(*pp) = new_lsn;
				}
				if ((ret =
				    __ham_add_ovflpage(dbc, *pp, 1, pp)) != 0)
					goto err;
			}

			/* Check if we need to update a cursor. */
			if (carray != NULL) {
				found = 0;
				for (i = 0; carray[i] != NULL; i++) {
					cp =
					    (HASH_CURSOR *)carray[i]->internal;
					if (cp->pgno == PGNO(temp_pagep)
					    && cp->indx == n) {
						cp->pgno = PGNO(*pp);
						cp->indx = NUM_ENT(*pp);
						found = 1;
					}
				}
				if (found && DB_LOGGING(dbc)
				    && IS_SUBTRANSACTION(dbc->txn)) {
					if ((ret =
					    __ham_chgpg_log(dbp->dbenv,
					    dbc->txn, &new_lsn, 0,
					    dbp->log_fileid,
					    DB_HAM_SPLIT, PGNO(temp_pagep),
					    PGNO(*pp), n, NUM_ENT(*pp))) != 0)
						goto err;
				}
			}
			__ham_copy_item(dbp->pgsize,
			    temp_pagep, H_KEYINDEX(n), *pp);
			__ham_copy_item(dbp->pgsize,
			    temp_pagep, H_DATAINDEX(n), *pp);
		}
		next_pgno = NEXT_PGNO(temp_pagep);

		/* Clear temp_page; if it's a link overflow page, free it. */
		if (PGNO(temp_pagep) != bucket_pgno && (ret =
		    __db_free(dbc, temp_pagep)) != 0) {
			temp_pagep = NULL;
			goto err;
		}

		if (next_pgno == PGNO_INVALID)
			temp_pagep = NULL;
		else if ((ret = memp_fget(dbp->mpf,
		    &next_pgno, DB_MPOOL_CREATE, &temp_pagep)) != 0)
			goto err;

		if (temp_pagep != NULL && DB_LOGGING(dbc)) {
			page_dbt.size = dbp->pgsize;
			page_dbt.data = temp_pagep;
			if ((ret = __ham_splitdata_log(dbenv,
			    dbc->txn, &new_lsn, 0, dbp->log_fileid,
			    SPLITOLD, PGNO(temp_pagep),
			    &page_dbt, &LSN(temp_pagep))) != 0)
				goto err;
			LSN(temp_pagep) = new_lsn;
		}
	}
	if (big_buf != NULL)
		__os_free(big_buf, big_len);

	/*
	 * If the original bucket spanned multiple pages, then we've got
	 * a pointer to a page that used to be on the bucket chain.  It
	 * should be deleted.
	 */
	if (temp_pagep != NULL && PGNO(temp_pagep) != bucket_pgno &&
	    (ret = __db_free(dbc, temp_pagep)) != 0) {
		temp_pagep = NULL;
		goto err;
	}

	/*
	 * Write new buckets out.
	 */
	if (DB_LOGGING(dbc)) {
		page_dbt.size = dbp->pgsize;
		page_dbt.data = old_pagep;
		if ((ret = __ham_splitdata_log(dbenv, dbc->txn, &new_lsn, 0,
		    dbp->log_fileid, SPLITNEW, PGNO(old_pagep), &page_dbt,
		    &LSN(old_pagep))) != 0)
			goto err;
		LSN(old_pagep) = new_lsn;

		page_dbt.data = new_pagep;
		if ((ret = __ham_splitdata_log(dbenv, dbc->txn, &new_lsn, 0,
		    dbp->log_fileid, SPLITNEW, PGNO(new_pagep), &page_dbt,
		    &LSN(new_pagep))) != 0)
			goto err;
		LSN(new_pagep) = new_lsn;
	}
	ret = memp_fput(dbp->mpf, old_pagep, DB_MPOOL_DIRTY);
	if ((t_ret = memp_fput(dbp->mpf, new_pagep, DB_MPOOL_DIRTY)) != 0
	    && ret == 0)
		ret = t_ret;

	if (0) {
err:		if (old_pagep != NULL)
			(void)memp_fput(dbp->mpf, old_pagep, DB_MPOOL_DIRTY);
		if (new_pagep != NULL)
			(void)memp_fput(dbp->mpf, new_pagep, DB_MPOOL_DIRTY);
		if (temp_pagep != NULL && PGNO(temp_pagep) != bucket_pgno)
			(void)memp_fput(dbp->mpf, temp_pagep, DB_MPOOL_DIRTY);
	}
	if (carray != NULL)		/* We never knew its size. */
		__os_free(carray, 0);
	return (ret);
}

/*
 * Add the given pair to the page.  The page in question may already be
 * held (i.e. it was already gotten).  If it is, then the page is passed
 * in via the pagep parameter.  On return, pagep will contain the page
 * to which we just added something.  This allows us to link overflow
 * pages and return the new page having correctly put the last page.
 *
 * PUBLIC: int __ham_add_el __P((DBC *, const DBT *, const DBT *, int));
 */
int
__ham_add_el(dbc, key, val, type)
	DBC *dbc;
	const DBT *key, *val;
	int type;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	const DBT *pkey, *pdata;
	DBT key_dbt, data_dbt;
	DB_LSN new_lsn;
	HOFFPAGE doff, koff;
	db_pgno_t next_pgno, pgno;
	u_int32_t data_size, key_size, pairsize, rectype;
	int do_expand, is_keybig, is_databig, ret;
	int key_type, data_type;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;
	do_expand = 0;

	pgno = hcp->seek_found_page != PGNO_INVALID ?  hcp->seek_found_page :
	    hcp->pgno;
	if (hcp->page == NULL && (ret = memp_fget(dbp->mpf, &pgno,
	    DB_MPOOL_CREATE, &hcp->page)) != 0)
		return (ret);

	key_size = HKEYDATA_PSIZE(key->size);
	data_size = HKEYDATA_PSIZE(val->size);
	is_keybig = ISBIG(hcp, key->size);
	is_databig = ISBIG(hcp, val->size);
	if (is_keybig)
		key_size = HOFFPAGE_PSIZE;
	if (is_databig)
		data_size = HOFFPAGE_PSIZE;

	pairsize = key_size + data_size;

	/* Advance to first page in chain with room for item. */
	while (H_NUMPAIRS(hcp->page) && NEXT_PGNO(hcp->page) != PGNO_INVALID) {
		/*
		 * This may not be the end of the chain, but the pair may fit
		 * anyway.  Check if it's a bigpair that fits or a regular
		 * pair that fits.
		 */
		if (P_FREESPACE(hcp->page) >= pairsize)
			break;
		next_pgno = NEXT_PGNO(hcp->page);
		if ((ret =
		    __ham_next_cpage(dbc, next_pgno, 0)) != 0)
			return (ret);
	}

	/*
	 * Check if we need to allocate a new page.
	 */
	if (P_FREESPACE(hcp->page) < pairsize) {
		do_expand = 1;
		if ((ret = __ham_add_ovflpage(dbc,
		    (PAGE *)hcp->page, 1, (PAGE **)&hcp->page)) !=  0)
			return (ret);
		hcp->pgno = PGNO(hcp->page);
	}

	/*
	 * Update cursor.
	 */
	hcp->indx = NUM_ENT(hcp->page);
	F_CLR(hcp, H_DELETED);
	if (is_keybig) {
		koff.type = H_OFFPAGE;
		UMRW_SET(koff.unused[0]);
		UMRW_SET(koff.unused[1]);
		UMRW_SET(koff.unused[2]);
		if ((ret = __db_poff(dbc, key, &koff.pgno)) != 0)
			return (ret);
		koff.tlen = key->size;
		key_dbt.data = &koff;
		key_dbt.size = sizeof(koff);
		pkey = &key_dbt;
		key_type = H_OFFPAGE;
	} else {
		pkey = key;
		key_type = H_KEYDATA;
	}

	if (is_databig) {
		doff.type = H_OFFPAGE;
		UMRW_SET(doff.unused[0]);
		UMRW_SET(doff.unused[1]);
		UMRW_SET(doff.unused[2]);
		if ((ret = __db_poff(dbc, val, &doff.pgno)) != 0)
			return (ret);
		doff.tlen = val->size;
		data_dbt.data = &doff;
		data_dbt.size = sizeof(doff);
		pdata = &data_dbt;
		data_type = H_OFFPAGE;
	} else {
		pdata = val;
		data_type = type;
	}

	if (DB_LOGGING(dbc)) {
		rectype = PUTPAIR;
		if (is_databig)
			rectype |= PAIR_DATAMASK;
		if (is_keybig)
			rectype |= PAIR_KEYMASK;
		if (type == H_DUPLICATE)
			rectype |= PAIR_DUPMASK;

		if ((ret = __ham_insdel_log(dbp->dbenv, dbc->txn, &new_lsn, 0,
		    rectype, dbp->log_fileid, PGNO(hcp->page),
		    (u_int32_t)NUM_ENT(hcp->page), &LSN(hcp->page), pkey,
		    pdata)) != 0)
			return (ret);

		/* Move lsn onto page. */
		LSN(hcp->page) = new_lsn;	/* Structure assignment. */
	}

	__ham_putitem(hcp->page, pkey, key_type);
	__ham_putitem(hcp->page, pdata, data_type);

	/*
	 * For splits, we are going to update item_info's page number
	 * field, so that we can easily return to the same page the
	 * next time we come in here.  For other operations, this shouldn't
	 * matter, since odds are this is the last thing that happens before
	 * we return to the user program.
	 */
	hcp->pgno = PGNO(hcp->page);

	/*
	 * XXX
	 * Maybe keep incremental numbers here.
	 */
	if (!STD_LOCKING(dbc))
		hcp->hdr->nelem++;

	if (do_expand || (hcp->hdr->ffactor != 0 &&
	    (u_int32_t)H_NUMPAIRS(hcp->page) > hcp->hdr->ffactor))
		F_SET(hcp, H_EXPAND);
	return (0);
}

/*
 * Special __putitem call used in splitting -- copies one entry to
 * another.  Works for all types of hash entries (H_OFFPAGE, H_KEYDATA,
 * H_DUPLICATE, H_OFFDUP).  Since we log splits at a high level, we
 * do not need to do any logging here.
 *
 * PUBLIC: void __ham_copy_item __P((size_t, PAGE *, u_int32_t, PAGE *));
 */
void
__ham_copy_item(pgsize, src_page, src_ndx, dest_page)
	size_t pgsize;
	PAGE *src_page;
	u_int32_t src_ndx;
	PAGE *dest_page;
{
	u_int32_t len;
	void *src, *dest;

	/*
	 * Copy the key and data entries onto this new page.
	 */
	src = P_ENTRY(src_page, src_ndx);

	/* Set up space on dest. */
	len = LEN_HITEM(src_page, pgsize, src_ndx);
	HOFFSET(dest_page) -= len;
	dest_page->inp[NUM_ENT(dest_page)] = HOFFSET(dest_page);
	dest = P_ENTRY(dest_page, NUM_ENT(dest_page));
	NUM_ENT(dest_page)++;

	memcpy(dest, src, len);
}

/*
 *
 * Returns:
 *      pointer on success
 *      NULL on error
 *
 * PUBLIC: int __ham_add_ovflpage __P((DBC *, PAGE *, int, PAGE **));
 */
int
__ham_add_ovflpage(dbc, pagep, release, pp)
	DBC *dbc;
	PAGE *pagep;
	int release;
	PAGE **pp;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	DB_LSN new_lsn;
	PAGE *new_pagep;
	int ret;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;

	if ((ret = __db_new(dbc, P_HASH, &new_pagep)) != 0)
		return (ret);

	if (DB_LOGGING(dbc)) {
		if ((ret = __ham_newpage_log(dbp->dbenv, dbc->txn, &new_lsn, 0,
		    PUTOVFL, dbp->log_fileid, PGNO(pagep), &LSN(pagep),
		    PGNO(new_pagep), &LSN(new_pagep), PGNO_INVALID, NULL)) != 0)
			return (ret);

		/* Move lsn onto page. */
		LSN(pagep) = LSN(new_pagep) = new_lsn;
	}
	NEXT_PGNO(pagep) = PGNO(new_pagep);
	PREV_PGNO(new_pagep) = PGNO(pagep);

	if (release)
		ret = memp_fput(dbp->mpf, pagep, DB_MPOOL_DIRTY);

	*pp = new_pagep;
	return (ret);
}

/*
 * PUBLIC: int __ham_get_cpage __P((DBC *, db_lockmode_t));
 */
int
__ham_get_cpage(dbc, mode)
	DBC *dbc;
	db_lockmode_t mode;
{
	DB *dbp;
	DB_LOCK tmp_lock;
	HASH_CURSOR *hcp;
	int ret;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;
	ret = 0;

	/*
	 * There are four cases with respect to buckets and locks.
	 * 1. If there is no lock held, then if we are locking, we should
	 *    get the lock.
	 * 2. If there is a lock held, it's for the current bucket, and it's
	 *    for the right mode, we don't need to do anything.
	 * 3. If there is a lock held for the current bucket but it's not
	 *    strong enough, we need to upgrade.
	 * 4. If there is a lock, but it's for a different bucket, then we need
	 *    to release the existing lock and get a new lock.
	 */
	tmp_lock.off = LOCK_INVALID;
	if (STD_LOCKING(dbc)) {
		if (hcp->lock.off != LOCK_INVALID &&
		    hcp->lbucket != hcp->bucket) {		/* Case 4 */
			if (dbc->txn == NULL &&
			    (ret = lock_put(dbp->dbenv, &hcp->lock)) != 0)
				return (ret);
			hcp->lock.off = LOCK_INVALID;
		}
		if ((hcp->lock.off != LOCK_INVALID &&
		    (hcp->lock_mode == DB_LOCK_READ &&
		    mode == DB_LOCK_WRITE))) {
			/* Case 3. */
			tmp_lock = hcp->lock;
			hcp->lock.off = LOCK_INVALID;
		}

		/* Acquire the lock. */
		if (hcp->lock.off == LOCK_INVALID)
			/* Cases 1, 3, and 4. */
			if ((ret = __ham_lock_bucket(dbc, mode)) != 0)
				return (ret);

		if (ret == 0) {
			hcp->lock_mode = mode;
			hcp->lbucket = hcp->bucket;
			if (tmp_lock.off != LOCK_INVALID)
				/* Case 3: release the original lock. */
				ret = lock_put(dbp->dbenv, &tmp_lock);
		} else if (tmp_lock.off != LOCK_INVALID)
			hcp->lock = tmp_lock;
	}

	if (ret == 0 && hcp->page == NULL) {
		if (hcp->pgno == PGNO_INVALID)
			hcp->pgno = BUCKET_TO_PAGE(hcp, hcp->bucket);
		if ((ret = memp_fget(dbp->mpf,
		    &hcp->pgno, DB_MPOOL_CREATE, &hcp->page)) != 0)
			return (ret);
	}

	return (0);
}

/*
 * Get a new page at the cursor, putting the last page if necessary.
 * If the flag is set to H_ISDUP, then we are talking about the
 * duplicate page, not the main page.
 *
 * PUBLIC: int __ham_next_cpage __P((DBC *, db_pgno_t, int));
 */
int
__ham_next_cpage(dbc, pgno, dirty)
	DBC *dbc;
	db_pgno_t pgno;
	int dirty;
{
	DB *dbp;
	HASH_CURSOR *hcp;
	PAGE *p;
	int ret;

	dbp = dbc->dbp;
	hcp = (HASH_CURSOR *)dbc->internal;

	if (hcp->page != NULL && (ret = memp_fput(dbp->mpf,
	    hcp->page, dirty ? DB_MPOOL_DIRTY : 0)) != 0)
		return (ret);

	if ((ret = memp_fget(dbp->mpf, &pgno, DB_MPOOL_CREATE, &p)) != 0)
		return (ret);

	hcp->page = p;
	hcp->pgno = pgno;
	hcp->indx = 0;

	return (0);
}

/*
 * __ham_lock_bucket --
 *	Get the lock on a particular bucket.
 *
 * PUBLIC: int __ham_lock_bucket __P((DBC *, db_lockmode_t));
 */
int
__ham_lock_bucket(dbc, mode)
	DBC *dbc;
	db_lockmode_t mode;
{
	HASH_CURSOR *hcp;
	u_int32_t flags;
	int gotmeta, ret;

	hcp = (HASH_CURSOR *)dbc->internal;
	gotmeta = hcp->hdr == NULL ? 1 : 0;
	if (gotmeta)
		if ((ret = __ham_get_meta(dbc)) != 0)
			return (ret);
	dbc->lock.pgno = BUCKET_TO_PAGE(hcp, hcp->bucket);
	if (gotmeta)
		if ((ret = __ham_release_meta(dbc)) != 0)
			return (ret);

	flags = 0;
	if (DB_NONBLOCK(dbc))
		LF_SET(DB_LOCK_NOWAIT);

	ret = lock_get(dbc->dbp->dbenv,
		    dbc->locker, flags, &dbc->lock_dbt, mode, &hcp->lock);

	hcp->lock_mode = mode;
	return (ret);
}

/*
 * __ham_dpair --
 *	Delete a pair on a page, paying no attention to what the pair
 *	represents.  The caller is responsible for freeing up duplicates
 *	or offpage entries that might be referenced by this pair.
 *
 * PUBLIC: void __ham_dpair __P((DB *, PAGE *, u_int32_t));
 */
void
__ham_dpair(dbp, p, indx)
	DB *dbp;
	PAGE *p;
	u_int32_t indx;
{
	db_indx_t delta, n;
	u_int8_t *dest, *src;

	/*
	 * Compute "delta", the amount we have to shift all of the
	 * offsets.  To find the delta, we just need to calculate
	 * the size of the pair of elements we are removing.
	 */
	delta = H_PAIRSIZE(p, dbp->pgsize, indx);

	/*
	 * The hard case: we want to remove something other than
	 * the last item on the page.  We need to shift data and
	 * offsets down.
	 */
	if ((db_indx_t)indx != NUM_ENT(p) - 2) {
		/*
		 * Move the data: src is the first occupied byte on
		 * the page. (Length is delta.)
		 */
		src = (u_int8_t *)p + HOFFSET(p);

		/*
		 * Destination is delta bytes beyond src.  This might
		 * be an overlapping copy, so we have to use memmove.
		 */
		dest = src + delta;
		memmove(dest, src, p->inp[H_DATAINDEX(indx)] - HOFFSET(p));
	}

	/* Adjust page metadata. */
	HOFFSET(p) = HOFFSET(p) + delta;
	NUM_ENT(p) = NUM_ENT(p) - 2;

	/* Adjust the offsets. */
	for (n = (db_indx_t)indx; n < (db_indx_t)(NUM_ENT(p)); n++)
		p->inp[n] = p->inp[n + 2] + delta;

}
