/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
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
static const char revid[] = "$Id: bt_put.c,v 11.46 2001/01/17 18:48:46 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"

static int __bam_dup_convert __P((DBC *, PAGE *, u_int32_t));
static int __bam_ovput
	       __P((DBC *, u_int32_t, db_pgno_t, PAGE *, u_int32_t, DBT *));

/*
 * __bam_iitem --
 *	Insert an item into the tree.
 *
 * PUBLIC: int __bam_iitem __P((DBC *, DBT *, DBT *, u_int32_t, u_int32_t));
 */
int
__bam_iitem(dbc, key, data, op, flags)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t op, flags;
{
	BKEYDATA *bk, bk_tmp;
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	DBT bk_hdr, tdbt;
	PAGE *h;
	db_indx_t indx;
	u_int32_t data_size, have_bytes, need_bytes, needed;
	int cmp, bigkey, bigdata, dupadjust, padrec, replace, ret, was_deleted;

	COMPQUIET(bk, NULL);

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	t = dbp->bt_internal;
	h = cp->page;
	indx = cp->indx;
	dupadjust = replace = was_deleted = 0;

	/*
	 * Fixed-length records with partial puts: it's an error to specify
	 * anything other simple overwrite.
	 */
	if (F_ISSET(dbp, DB_RE_FIXEDLEN) &&
	    F_ISSET(data, DB_DBT_PARTIAL) && data->dlen != data->size) {
		data_size = data->size;
		goto len_err;
	}

	/*
	 * Figure out how much space the data will take, including if it's a
	 * partial record.
	 *
	 * Fixed-length records: it's an error to specify a record that's
	 * longer than the fixed-length, and we never require less than
	 * the fixed-length record size.
	 */
	data_size = F_ISSET(data, DB_DBT_PARTIAL) ?
	    __bam_partsize(op, data, h, indx) : data->size;
	padrec = 0;
	if (F_ISSET(dbp, DB_RE_FIXEDLEN)) {
		if (data_size > t->re_len) {
len_err:		__db_err(dbp->dbenv,
			    "Length improper for fixed length record %lu",
			    (u_long)data_size);
			return (EINVAL);
		}
		if (data_size < t->re_len) {
			padrec = 1;
			data_size = t->re_len;
		}
	}

	/*
	 * Handle partial puts or short fixed-length records: build the
	 * real record.
	 */
	if (padrec || F_ISSET(data, DB_DBT_PARTIAL)) {
		tdbt = *data;
		if ((ret =
		    __bam_build(dbc, op, &tdbt, h, indx, data_size)) != 0)
			return (ret);
		data = &tdbt;
	}

	/*
	 * If the user has specified a duplicate comparison function, return
	 * an error if DB_CURRENT was specified and the replacement data
	 * doesn't compare equal to the current data.  This stops apps from
	 * screwing up the duplicate sort order.  We have to do this after
	 * we build the real record so that we're comparing the real items.
	 */
	if (op == DB_CURRENT && dbp->dup_compare != NULL) {
		if ((ret = __bam_cmp(dbp, data, h,
		     indx + (TYPE(h) == P_LBTREE ? O_INDX : 0),
		     dbp->dup_compare, &cmp)) != 0)
			return (ret);
		if (cmp != 0) {
			__db_err(dbp->dbenv,
			    "Current data differs from put data");
			return (EINVAL);
		}
	}

	/*
	 * If the key or data item won't fit on a page, we'll have to store
	 * them on overflow pages.
	 */
	needed = 0;
	bigdata = data_size > cp->ovflsize;
	switch (op) {
	case DB_KEYFIRST:
		/* We're adding a new key and data pair. */
		bigkey = key->size > cp->ovflsize;
		if (bigkey)
			needed += BOVERFLOW_PSIZE;
		else
			needed += BKEYDATA_PSIZE(key->size);
		if (bigdata)
			needed += BOVERFLOW_PSIZE;
		else
			needed += BKEYDATA_PSIZE(data_size);
		break;
	case DB_AFTER:
	case DB_BEFORE:
	case DB_CURRENT:
		/*
		 * We're either overwriting the data item of a key/data pair
		 * or we're creating a new on-page duplicate and only adding
		 * a data item.
		 *
		 * !!!
		 * We're not currently correcting for space reclaimed from
		 * already deleted items, but I don't think it's worth the
		 * complexity.
		 */
		bigkey = 0;
		if (op == DB_CURRENT) {
			bk = GET_BKEYDATA(h,
			    indx + (TYPE(h) == P_LBTREE ? O_INDX : 0));
			if (B_TYPE(bk->type) == B_KEYDATA)
				have_bytes = BKEYDATA_PSIZE(bk->len);
			else
				have_bytes = BOVERFLOW_PSIZE;
			need_bytes = 0;
		} else {
			have_bytes = 0;
			need_bytes = sizeof(db_indx_t);
		}
		if (bigdata)
			need_bytes += BOVERFLOW_PSIZE;
		else
			need_bytes += BKEYDATA_PSIZE(data_size);

		if (have_bytes < need_bytes)
			needed += need_bytes - have_bytes;
		break;
	default:
		return (__db_unknown_flag(dbp->dbenv, "__bam_iitem", op));
	}

	/*
	 * If there's not enough room, or the user has put a ceiling on the
	 * number of keys permitted in the page, split the page.
	 *
	 * XXX
	 * The t->bt_maxkey test here may be insufficient -- do we have to
	 * check in the btree split code, so we don't undo it there!?!?
	 */
	if (P_FREESPACE(h) < needed ||
	    (t->bt_maxkey != 0 && NUM_ENT(h) > t->bt_maxkey))
		return (DB_NEEDSPLIT);

	/*
	 * The code breaks it up into five cases:
	 *
	 * 1. Insert a new key/data pair.
	 * 2. Append a new data item (a new duplicate).
	 * 3. Insert a new data item (a new duplicate).
	 * 4. Delete and re-add the data item (overflow item).
	 * 5. Overwrite the data item.
	 */
	switch (op) {
	case DB_KEYFIRST:		/* 1. Insert a new key/data pair. */
		if (bigkey) {
			if ((ret = __bam_ovput(dbc,
			    B_OVERFLOW, PGNO_INVALID, h, indx, key)) != 0)
				return (ret);
		} else
			if ((ret = __db_pitem(dbc, h, indx,
			    BKEYDATA_SIZE(key->size), NULL, key)) != 0)
				return (ret);

		if ((ret = __bam_ca_di(dbc, PGNO(h), indx, 1)) != 0)
			return (ret);
		++indx;
		break;
	case DB_AFTER:			/* 2. Append a new data item. */
		if (TYPE(h) == P_LBTREE) {
			/* Copy the key for the duplicate and adjust cursors. */
			if ((ret =
			    __bam_adjindx(dbc, h, indx + P_INDX, indx, 1)) != 0)
				return (ret);
			if ((ret =
			    __bam_ca_di(dbc, PGNO(h), indx + P_INDX, 1)) != 0)
				return (ret);

			indx += 3;
			dupadjust = 1;

			cp->indx += 2;
		} else {
			++indx;
			cp->indx += 1;
		}
		break;
	case DB_BEFORE:			/* 3. Insert a new data item. */
		if (TYPE(h) == P_LBTREE) {
			/* Copy the key for the duplicate and adjust cursors. */
			if ((ret = __bam_adjindx(dbc, h, indx, indx, 1)) != 0)
				return (ret);
			if ((ret = __bam_ca_di(dbc, PGNO(h), indx, 1)) != 0)
				return (ret);

			++indx;
			dupadjust = 1;
		}
		break;
	case DB_CURRENT:
		 /*
		  * Clear the cursor's deleted flag.  The problem is that if
		  * we deadlock or fail while deleting the overflow item or
		  * replacing the non-overflow item, a subsequent cursor close
		  * will try and remove the item because the cursor's delete
		  * flag is set
		  */
		(void)__bam_ca_delete(dbp, PGNO(h), indx, 0);

		if (TYPE(h) == P_LBTREE) {
			++indx;
			dupadjust = 1;

			/*
			 * In a Btree deleted records aren't counted (deleted
			 * records are counted in a Recno because all accesses
			 * are based on record number).  If it's a Btree and
			 * it's a DB_CURRENT operation overwriting a previously
			 * deleted record, increment the record count.
			 */
			was_deleted = B_DISSET(bk->type);
		}

		/*
		 * 4. Delete and re-add the data item.
		 *
		 * If we're changing the type of the on-page structure, or we
		 * are referencing offpage items, we have to delete and then
		 * re-add the item.  We do not do any cursor adjustments here
		 * because we're going to immediately re-add the item into the
		 * same slot.
		 */
		if (bigdata || B_TYPE(bk->type) != B_KEYDATA) {
			if ((ret = __bam_ditem(dbc, h, indx)) != 0)
				return (ret);
			break;
		}

		/* 5. Overwrite the data item. */
		replace = 1;
		break;
	default:
		return (__db_unknown_flag(dbp->dbenv, "__bam_iitem", op));
	}

	/* Add the data. */
	if (bigdata) {
		if ((ret = __bam_ovput(dbc,
		    B_OVERFLOW, PGNO_INVALID, h, indx, data)) != 0)
			return (ret);
	} else {
		if (LF_ISSET(BI_DELETED)) {
			B_TSET(bk_tmp.type, B_KEYDATA, 1);
			bk_tmp.len = data->size;
			bk_hdr.data = &bk_tmp;
			bk_hdr.size = SSZA(BKEYDATA, data);
			ret = __db_pitem(dbc, h, indx,
			    BKEYDATA_SIZE(data->size), &bk_hdr, data);
		} else if (replace)
			ret = __bam_ritem(dbc, h, indx, data);
		else
			ret = __db_pitem(dbc, h, indx,
			    BKEYDATA_SIZE(data->size), NULL, data);
		if (ret != 0)
			return (ret);
	}
	if ((ret = memp_fset(dbp->mpf, h, DB_MPOOL_DIRTY)) != 0)
		return (ret);

	/*
	 * Re-position the cursors if necessary and reset the current cursor
	 * to point to the new item.
	 */
	if (op != DB_CURRENT) {
		if ((ret = __bam_ca_di(dbc, PGNO(h), indx, 1)) != 0)
			return (ret);
		cp->indx = TYPE(h) == P_LBTREE ? indx - O_INDX : indx;
	}

	/*
	 * If we've changed the record count, update the tree.  There's no
	 * need to adjust the count if the operation not performed on the
	 * current record or when the current record was previously deleted.
	 */
	if (F_ISSET(cp, C_RECNUM) && (op != DB_CURRENT || was_deleted))
		if ((ret = __bam_adjust(dbc, 1)) != 0)
			return (ret);

	/*
	 * If a Btree leaf page is at least 50% full and we may have added or
	 * modified a duplicate data item, see if the set of duplicates takes
	 * up at least 25% of the space on the page.  If it does, move it onto
	 * its own page.
	 */
	if (dupadjust && P_FREESPACE(h) <= dbp->pgsize / 2) {
		if ((ret = __bam_dup_convert(dbc, h, indx - O_INDX)) != 0)
			return (ret);
	}

	/* If we've modified a recno file, set the flag. */
	if (dbc->dbtype == DB_RECNO)
		t->re_modified = 1;

	return (ret);
}

/*
 * __bam_partsize --
 *	Figure out how much space a partial data item is in total.
 *
 * PUBLIC: u_int32_t __bam_partsize __P((u_int32_t, DBT *, PAGE *, u_int32_t));
 */
u_int32_t
__bam_partsize(op, data, h, indx)
	u_int32_t op, indx;
	DBT *data;
	PAGE *h;
{
	BKEYDATA *bk;
	u_int32_t nbytes;

	/*
	 * If the record doesn't already exist, it's simply the data we're
	 * provided.
	 */
	if (op != DB_CURRENT)
		return (data->doff + data->size);

	/*
	 * Otherwise, it's the data provided plus any already existing data
	 * that we're not replacing.
	 */
	bk = GET_BKEYDATA(h, indx + (TYPE(h) == P_LBTREE ? O_INDX : 0));
	nbytes =
	    B_TYPE(bk->type) == B_OVERFLOW ? ((BOVERFLOW *)bk)->tlen : bk->len;

	/*
	 * There are really two cases here:
	 *
	 * Case 1: We are replacing some bytes that do not exist (i.e., they
	 * are past the end of the record).  In this case the number of bytes
	 * we are replacing is irrelevant and all we care about is how many
	 * bytes we are going to add from offset.  So, the new record length
	 * is going to be the size of the new bytes (size) plus wherever those
	 * new bytes begin (doff).
	 *
	 * Case 2: All the bytes we are replacing exist.  Therefore, the new
	 * size is the oldsize (nbytes) minus the bytes we are replacing (dlen)
	 * plus the bytes we are adding (size).
	 */
	if (nbytes < data->doff + data->dlen)		/* Case 1 */
		return (data->doff + data->size);

	return (nbytes + data->size - data->dlen);	/* Case 2 */
}

/*
 * __bam_build --
 *	Build the real record for a partial put, or short fixed-length record.
 *
 * PUBLIC: int __bam_build __P((DBC *, u_int32_t,
 * PUBLIC:     DBT *, PAGE *, u_int32_t, u_int32_t));
 */
int
__bam_build(dbc, op, dbt, h, indx, nbytes)
	DBC *dbc;
	u_int32_t op, indx, nbytes;
	DBT *dbt;
	PAGE *h;
{
	BKEYDATA *bk, tbk;
	BOVERFLOW *bo;
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	DBT copy;
	u_int32_t len, tlen;
	u_int8_t *p;
	int ret;

	COMPQUIET(bo, NULL);

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *) dbc->internal;
	t = dbp->bt_internal;

	/* We use the record data return memory, it's only a short-term use. */
	if (dbc->rdata.ulen < nbytes) {
		if ((ret = __os_realloc(dbp->dbenv,
		    nbytes, NULL, &dbc->rdata.data)) != 0) {
			dbc->rdata.ulen = 0;
			dbc->rdata.data = NULL;
			return (ret);
		}
		dbc->rdata.ulen = nbytes;
	}

	/*
	 * We use nul or pad bytes for any part of the record that isn't
	 * specified; get it over with.
	 */
	memset(dbc->rdata.data,
	   F_ISSET(dbp, DB_RE_FIXEDLEN) ? t->re_pad : 0, nbytes);

	/*
	 * In the next clauses, we need to do three things: a) set p to point
	 * to the place at which to copy the user's data, b) set tlen to the
	 * total length of the record, not including the bytes contributed by
	 * the user, and c) copy any valid data from an existing record.  If
	 * it's not a partial put (this code is called for both partial puts
	 * and fixed-length record padding) or it's a new key, we can cut to
	 * the chase.
	 */
	if (!F_ISSET(dbt, DB_DBT_PARTIAL) || op != DB_CURRENT) {
		p = (u_int8_t *)dbc->rdata.data + dbt->doff;
		tlen = dbt->doff;
		goto user_copy;
	}

	/* Find the current record. */
	if (indx < NUM_ENT(h)) {
		bk = GET_BKEYDATA(h, indx + (TYPE(h) == P_LBTREE ? O_INDX : 0));
		bo = (BOVERFLOW *)bk;
	} else {
		bk = &tbk;
		B_TSET(bk->type, B_KEYDATA, 0);
		bk->len = 0;
	}
	if (B_TYPE(bk->type) == B_OVERFLOW) {
		/*
		 * In the case of an overflow record, we shift things around
		 * in the current record rather than allocate a separate copy.
		 */
		memset(&copy, 0, sizeof(copy));
		if ((ret = __db_goff(dbp, &copy, bo->tlen,
		    bo->pgno, &dbc->rdata.data, &dbc->rdata.ulen)) != 0)
			return (ret);

		/* Skip any leading data from the original record. */
		tlen = dbt->doff;
		p = (u_int8_t *)dbc->rdata.data + dbt->doff;

		/*
		 * Copy in any trailing data from the original record.
		 *
		 * If the original record was larger than the original offset
		 * plus the bytes being deleted, there is trailing data in the
		 * original record we need to preserve.  If we aren't deleting
		 * the same number of bytes as we're inserting, copy it up or
		 * down, into place.
		 *
		 * Use memmove(), the regions may overlap.
		 */
		if (bo->tlen > dbt->doff + dbt->dlen) {
			len = bo->tlen - (dbt->doff + dbt->dlen);
			if (dbt->dlen != dbt->size)
				memmove(p + dbt->size, p + dbt->dlen, len);
			tlen += len;
		}
	} else {
		/* Copy in any leading data from the original record. */
		memcpy(dbc->rdata.data,
		    bk->data, dbt->doff > bk->len ? bk->len : dbt->doff);
		tlen = dbt->doff;
		p = (u_int8_t *)dbc->rdata.data + dbt->doff;

		/* Copy in any trailing data from the original record. */
		len = dbt->doff + dbt->dlen;
		if (bk->len > len) {
			memcpy(p + dbt->size, bk->data + len, bk->len - len);
			tlen += bk->len - len;
		}
	}

user_copy:
	/*
	 * Copy in the application provided data -- p and tlen must have been
	 * initialized above.
	 */
	memcpy(p, dbt->data, dbt->size);
	tlen += dbt->size;

	/* Set the DBT to reference our new record. */
	dbc->rdata.size = F_ISSET(dbp, DB_RE_FIXEDLEN) ? t->re_len : tlen;
	dbc->rdata.dlen = 0;
	dbc->rdata.doff = 0;
	dbc->rdata.flags = 0;
	*dbt = dbc->rdata;
	return (0);
}

/*
 * __bam_ritem --
 *	Replace an item on a page.
 *
 * PUBLIC: int __bam_ritem __P((DBC *, PAGE *, u_int32_t, DBT *));
 */
int
__bam_ritem(dbc, h, indx, data)
	DBC *dbc;
	PAGE *h;
	u_int32_t indx;
	DBT *data;
{
	BKEYDATA *bk;
	DB *dbp;
	DBT orig, repl;
	db_indx_t cnt, lo, ln, min, off, prefix, suffix;
	int32_t nbytes;
	int ret;
	u_int8_t *p, *t;

	dbp = dbc->dbp;

	/*
	 * Replace a single item onto a page.  The logic figuring out where
	 * to insert and whether it fits is handled in the caller.  All we do
	 * here is manage the page shuffling.
	 */
	bk = GET_BKEYDATA(h, indx);

	/* Log the change. */
	if (DB_LOGGING(dbc)) {
		/*
		 * We might as well check to see if the two data items share
		 * a common prefix and suffix -- it can save us a lot of log
		 * message if they're large.
		 */
		min = data->size < bk->len ? data->size : bk->len;
		for (prefix = 0,
		    p = bk->data, t = data->data;
		    prefix < min && *p == *t; ++prefix, ++p, ++t)
			;

		min -= prefix;
		for (suffix = 0,
		    p = (u_int8_t *)bk->data + bk->len - 1,
		    t = (u_int8_t *)data->data + data->size - 1;
		    suffix < min && *p == *t; ++suffix, --p, --t)
			;

		/* We only log the parts of the keys that have changed. */
		orig.data = (u_int8_t *)bk->data + prefix;
		orig.size = bk->len - (prefix + suffix);
		repl.data = (u_int8_t *)data->data + prefix;
		repl.size = data->size - (prefix + suffix);
		if ((ret = __bam_repl_log(dbp->dbenv, dbc->txn,
		    &LSN(h), 0, dbp->log_fileid, PGNO(h), &LSN(h),
		    (u_int32_t)indx, (u_int32_t)B_DISSET(bk->type),
		    &orig, &repl, (u_int32_t)prefix, (u_int32_t)suffix)) != 0)
			return (ret);
	}

	/*
	 * Set references to the first in-use byte on the page and the
	 * first byte of the item being replaced.
	 */
	p = (u_int8_t *)h + HOFFSET(h);
	t = (u_int8_t *)bk;

	/*
	 * If the entry is growing in size, shift the beginning of the data
	 * part of the page down.  If the entry is shrinking in size, shift
	 * the beginning of the data part of the page up.  Use memmove(3),
	 * the regions overlap.
	 */
	lo = BKEYDATA_SIZE(bk->len);
	ln = BKEYDATA_SIZE(data->size);
	if (lo != ln) {
		nbytes = lo - ln;		/* Signed difference. */
		if (p == t)			/* First index is fast. */
			h->inp[indx] += nbytes;
		else {				/* Else, shift the page. */
			memmove(p + nbytes, p, t - p);

			/* Adjust the indices' offsets. */
			off = h->inp[indx];
			for (cnt = 0; cnt < NUM_ENT(h); ++cnt)
				if (h->inp[cnt] <= off)
					h->inp[cnt] += nbytes;
		}

		/* Clean up the page and adjust the item's reference. */
		HOFFSET(h) += nbytes;
		t += nbytes;
	}

	/* Copy the new item onto the page. */
	bk = (BKEYDATA *)t;
	B_TSET(bk->type, B_KEYDATA, 0);
	bk->len = data->size;
	memcpy(bk->data, data->data, data->size);

	return (0);
}

/*
 * __bam_dup_convert --
 *	Check to see if the duplicate set at indx should have its own page.
 *	If it should, create it.
 */
static int
__bam_dup_convert(dbc, h, indx)
	DBC *dbc;
	PAGE *h;
	u_int32_t indx;
{
	BTREE_CURSOR *cp;
	BKEYDATA *bk;
	DB *dbp;
	DBT hdr;
	PAGE *dp;
	db_indx_t cnt, cpindx, dindx, first, sz;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * Count the duplicate records and calculate how much room they're
	 * using on the page.
	 */
	while (indx > 0 && h->inp[indx] == h->inp[indx - P_INDX])
		indx -= P_INDX;
	for (cnt = 0, sz = 0, first = indx;; ++cnt, indx += P_INDX) {
		if (indx >= NUM_ENT(h) || h->inp[first] != h->inp[indx])
			break;
		bk = GET_BKEYDATA(h, indx);
		sz += B_TYPE(bk->type) == B_KEYDATA ?
		    BKEYDATA_PSIZE(bk->len) : BOVERFLOW_PSIZE;
		bk = GET_BKEYDATA(h, indx + O_INDX);
		sz += B_TYPE(bk->type) == B_KEYDATA ?
		    BKEYDATA_PSIZE(bk->len) : BOVERFLOW_PSIZE;
	}

	/*
	 * We have to do these checks when the user is replacing the cursor's
	 * data item -- if the application replaces a duplicate item with a
	 * larger data item, it can increase the amount of space used by the
	 * duplicates, requiring this check.  But that means we may have done
	 * this check when it wasn't a duplicate item after all.
	 */
	if (cnt == 1)
		return (0);

	/*
	 * If this set of duplicates is using more than 25% of the page, move
	 * them off.  The choice of 25% is a WAG, but the value must be small
	 * enough that we can always split a page without putting duplicates
	 * on two different pages.
	 */
	if (sz < dbp->pgsize / 4)
		return (0);

	/* Get a new page. */
	if ((ret = __db_new(dbc,
	    dbp->dup_compare == NULL ? P_LRECNO : P_LDUP, &dp)) != 0)
		return (ret);
	P_INIT(dp, dbp->pgsize, dp->pgno,
	    PGNO_INVALID, PGNO_INVALID, LEAFLEVEL, TYPE(dp));

	/*
	 * Move this set of duplicates off the page.  First points to the first
	 * key of the first duplicate key/data pair, cnt is the number of pairs
	 * we're dealing with.
	 */
	memset(&hdr, 0, sizeof(hdr));
	dindx = first;
	indx = first;
	cpindx = 0;
	do {
		/* Move cursors referencing the old entry to the new entry. */
		if ((ret = __bam_ca_dup(dbc, first,
		    PGNO(h), indx, PGNO(dp), cpindx)) != 0)
			goto err;

		/*
		 * Copy the entry to the new page.  If the off-duplicate page
		 * If the off-duplicate page is a Btree page (i.e. dup_compare
		 * will be non-NULL, we use Btree pages for sorted dups,
		 * and Recno pages for unsorted dups), move all entries
		 * normally, even deleted ones.  If it's a Recno page,
		 * deleted entries are discarded (if the deleted entry is
		 * overflow, then free up those pages).
		 */
		bk = GET_BKEYDATA(h, dindx + 1);
		hdr.data = bk;
		hdr.size = B_TYPE(bk->type) == B_KEYDATA ?
		    BKEYDATA_SIZE(bk->len) : BOVERFLOW_SIZE;
		if (dbp->dup_compare == NULL && B_DISSET(bk->type)) {
			/*
			 * Unsorted dups, i.e. recno page, and we have
			 * a deleted entry, don't move it, but if it was
			 * an overflow entry, we need to free those pages.
			 */
			if (B_TYPE(bk->type) == B_OVERFLOW &&
			    (ret = __db_doff(dbc,
			    (GET_BOVERFLOW(h, dindx + 1))->pgno)) != 0)
				goto err;
		} else {
			if ((ret = __db_pitem(
			    dbc, dp, cpindx, hdr.size, &hdr, NULL)) != 0)
				goto err;
			++cpindx;
		}
		/* Delete all but the last reference to the key. */
		if (cnt != 1) {
			if ((ret = __bam_adjindx(dbc,
			    h, dindx, first + 1, 0)) != 0)
				goto err;
		} else
			dindx++;

		/* Delete the data item. */
		if ((ret = __db_ditem(dbc, h, dindx, hdr.size)) != 0)
			goto err;
		indx += P_INDX;
	} while (--cnt);

	/* Put in a new data item that points to the duplicates page. */
	if ((ret = __bam_ovput(dbc,
	     B_DUPLICATE, dp->pgno, h, first + 1, NULL)) != 0)
		goto err;

	/* Adjust cursors for all the above movments. */
	if ((ret = __bam_ca_di(dbc,
	    PGNO(h), first + P_INDX, first + P_INDX - indx)) != 0)
		goto err;

	return (memp_fput(dbp->mpf, dp, DB_MPOOL_DIRTY));

err:	(void)__db_free(dbc, dp);
	return (ret);
}

/*
 * __bam_ovput --
 *	Build an item for an off-page duplicates page or overflow page and
 *	insert it on the page.
 */
static int
__bam_ovput(dbc, type, pgno, h, indx, item)
	DBC *dbc;
	u_int32_t type, indx;
	db_pgno_t pgno;
	PAGE *h;
	DBT *item;
{
	BOVERFLOW bo;
	DBT hdr;
	int ret;

	UMRW_SET(bo.unused1);
	B_TSET(bo.type, type, 0);
	UMRW_SET(bo.unused2);

	/*
	 * If we're creating an overflow item, do so and acquire the page
	 * number for it.  If we're creating an off-page duplicates tree,
	 * we are giving the page number as an argument.
	 */
	if (type == B_OVERFLOW) {
		if ((ret = __db_poff(dbc, item, &bo.pgno)) != 0)
			return (ret);
		bo.tlen = item->size;
	} else {
		bo.pgno = pgno;
		bo.tlen = 0;
	}

	/* Store the new record on the page. */
	memset(&hdr, 0, sizeof(hdr));
	hdr.data = &bo;
	hdr.size = BOVERFLOW_SIZE;
	return (__db_pitem(dbc, h, indx, BOVERFLOW_SIZE, &hdr, NULL));
}
