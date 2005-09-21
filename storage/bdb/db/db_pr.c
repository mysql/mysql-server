/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_pr.c,v 11.121 2004/10/28 14:48:43 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/mp.h"
#include "dbinc/qam.h"
#include "dbinc/db_verify.h"

/*
 * __db_loadme --
 *	A nice place to put a breakpoint.
 *
 * PUBLIC: void __db_loadme __P((void));
 */
void
__db_loadme()
{
	u_int32_t id;

	__os_id(&id);
}

#ifdef HAVE_STATISTICS
static int	 __db_bmeta __P((DB *, BTMETA *, u_int32_t));
static int	 __db_hmeta __P((DB *, HMETA *, u_int32_t));
static void	 __db_meta __P((DB *, DBMETA *, FN const *, u_int32_t));
static const char *__db_pagetype_to_string __P((u_int32_t));
static void	 __db_prdb __P((DB *, u_int32_t));
static void	 __db_proff __P((DB_ENV *, DB_MSGBUF *, void *));
static int	 __db_prtree __P((DB *, u_int32_t));
static int	 __db_qmeta __P((DB *, QMETA *, u_int32_t));

/*
 * __db_dumptree --
 *	Dump the tree to a file.
 *
 * PUBLIC: int __db_dumptree __P((DB *, char *, char *));
 */
int
__db_dumptree(dbp, op, name)
	DB *dbp;
	char *op, *name;
{
	DB_ENV *dbenv;
	FILE *fp, *orig_fp;
	u_int32_t flags;
	int ret;

	dbenv = dbp->dbenv;

	for (flags = 0; *op != '\0'; ++op)
		switch (*op) {
		case 'a':
			LF_SET(DB_PR_PAGE);
			break;
		case 'h':
			break;
		case 'r':
			LF_SET(DB_PR_RECOVERYTEST);
			break;
		default:
			return (EINVAL);
		}

	if (name != NULL) {
		if ((fp = fopen(name, "w")) == NULL)
			return (__os_get_errno());

		orig_fp = dbenv->db_msgfile;
		dbenv->db_msgfile = fp;
	} else
		fp = orig_fp = NULL;

	__db_prdb(dbp, flags);

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));

	ret = __db_prtree(dbp, flags);

	if (fp != NULL) {
		(void)fclose(fp);
		dbenv->db_msgfile = orig_fp;
	}

	return (ret);
}

static const FN __db_flags_fn[] = {
	{ DB_AM_CHKSUM,		"checksumming" },
	{ DB_AM_CL_WRITER,	"client replica writer" },
	{ DB_AM_COMPENSATE,	"created by compensating transaction" },
	{ DB_AM_CREATED,	"database created" },
	{ DB_AM_CREATED_MSTR,	"encompassing file created" },
	{ DB_AM_DBM_ERROR,	"dbm/ndbm error" },
	{ DB_AM_DELIMITER,	"variable length" },
	{ DB_AM_DIRTY,		"dirty reads" },
	{ DB_AM_DISCARD,	"discard cached pages" },
	{ DB_AM_DUP,		"duplicates" },
	{ DB_AM_DUPSORT,	"sorted duplicates" },
	{ DB_AM_ENCRYPT,	"encrypted" },
	{ DB_AM_FIXEDLEN,	"fixed-length records" },
	{ DB_AM_INMEM,		"in-memory" },
	{ DB_AM_IN_RENAME,	"file is being renamed" },
	{ DB_AM_NOT_DURABLE,	"changes not logged" },
	{ DB_AM_OPEN_CALLED,	"open called" },
	{ DB_AM_PAD,		"pad value" },
	{ DB_AM_PGDEF,		"default page size" },
	{ DB_AM_RDONLY,		"read-only" },
	{ DB_AM_RECNUM,		"Btree record numbers" },
	{ DB_AM_RECOVER,	"opened for recovery" },
	{ DB_AM_RENUMBER,	"renumber" },
	{ DB_AM_REPLICATION,	"replication file" },
	{ DB_AM_REVSPLITOFF,	"no reverse splits" },
	{ DB_AM_SECONDARY,	"secondary" },
	{ DB_AM_SNAPSHOT,	"load on open" },
	{ DB_AM_SUBDB,		"subdatabases" },
	{ DB_AM_SWAP,		"needswap" },
	{ DB_AM_TXN,		"transactional" },
	{ DB_AM_VERIFYING,	"verifier" },
	{ 0,			NULL }
};

/*
 * __db_get_flags_fn --
 *	Return the __db_flags_fn array.
 *
 * PUBLIC: const FN * __db_get_flags_fn __P((void));
 */
const FN *
__db_get_flags_fn()
{
	return (__db_flags_fn);
}

/*
 * __db_prdb --
 *	Print out the DB structure information.
 */
static void
__db_prdb(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_MSGBUF mb;
	DB_ENV *dbenv;
	BTREE *bt;
	HASH *h;
	QUEUE *q;

	dbenv = dbp->dbenv;

	DB_MSGBUF_INIT(&mb);
	__db_msg(dbenv, "In-memory DB structure:");
	__db_msgadd(dbenv, &mb, "%s: %#lx",
	    __db_dbtype_to_string(dbp->type), (u_long)dbp->flags);
	__db_prflags(dbenv, &mb, dbp->flags, __db_flags_fn, " (", ")");
	DB_MSGBUF_FLUSH(dbenv, &mb);

	switch (dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		bt = dbp->bt_internal;
		__db_msg(dbenv, "bt_meta: %lu bt_root: %lu",
		    (u_long)bt->bt_meta, (u_long)bt->bt_root);
		__db_msg(dbenv, "bt_maxkey: %lu bt_minkey: %lu",
		    (u_long)bt->bt_maxkey, (u_long)bt->bt_minkey);
		if (!LF_ISSET(DB_PR_RECOVERYTEST))
			__db_msg(dbenv, "bt_compare: %#lx bt_prefix: %#lx",
			    P_TO_ULONG(bt->bt_compare),
			    P_TO_ULONG(bt->bt_prefix));
		__db_msg(dbenv, "bt_lpgno: %lu", (u_long)bt->bt_lpgno);
		if (dbp->type == DB_RECNO) {
			__db_msg(dbenv,
		    "re_pad: %#lx re_delim: %#lx re_len: %lu re_source: %s",
			    (u_long)bt->re_pad, (u_long)bt->re_delim,
			    (u_long)bt->re_len,
			    bt->re_source == NULL ? "" : bt->re_source);
			__db_msg(dbenv,
			    "re_modified: %d re_eof: %d re_last: %lu",
			    bt->re_modified, bt->re_eof, (u_long)bt->re_last);
		}
		break;
	case DB_HASH:
		h = dbp->h_internal;
		__db_msg(dbenv, "meta_pgno: %lu", (u_long)h->meta_pgno);
		__db_msg(dbenv, "h_ffactor: %lu", (u_long)h->h_ffactor);
		__db_msg(dbenv, "h_nelem: %lu", (u_long)h->h_nelem);
		if (!LF_ISSET(DB_PR_RECOVERYTEST))
			__db_msg(dbenv, "h_hash: %#lx", P_TO_ULONG(h->h_hash));
		break;
	case DB_QUEUE:
		q = dbp->q_internal;
		__db_msg(dbenv, "q_meta: %lu", (u_long)q->q_meta);
		__db_msg(dbenv, "q_root: %lu", (u_long)q->q_root);
		__db_msg(dbenv, "re_pad: %#lx re_len: %lu",
		    (u_long)q->re_pad, (u_long)q->re_len);
		__db_msg(dbenv, "rec_page: %lu", (u_long)q->rec_page);
		__db_msg(dbenv, "page_ext: %lu", (u_long)q->page_ext);
		break;
	case DB_UNKNOWN:
	default:
		break;
	}
}

/*
 * __db_prtree --
 *	Print out the entire tree.
 */
static int
__db_prtree(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_MPOOLFILE *mpf;
	PAGE *h;
	db_pgno_t i, last;
	int ret;

	mpf = dbp->mpf;

	if (dbp->type == DB_QUEUE)
		return (__db_prqueue(dbp, flags));

	/*
	 * Find out the page number of the last page in the database, then
	 * dump each page.
	 */
	__memp_last_pgno(mpf, &last);
	for (i = 0; i <= last; ++i) {
		if ((ret = __memp_fget(mpf, &i, 0, &h)) != 0)
			return (ret);
		(void)__db_prpage(dbp, h, flags);
		if ((ret = __memp_fput(mpf, h, 0)) != 0)
			return (ret);
	}

	return (0);
}

/*
 * __db_meta --
 *	Print out common metadata information.
 */
static void
__db_meta(dbp, dbmeta, fn, flags)
	DB *dbp;
	DBMETA *dbmeta;
	FN const *fn;
	u_int32_t flags;
{
	DB_MSGBUF mb;
	DB_ENV *dbenv;
	DB_MPOOLFILE *mpf;
	PAGE *h;
	db_pgno_t pgno;
	u_int8_t *p;
	int cnt, ret;
	const char *sep;

	dbenv = dbp->dbenv;
	mpf = dbp->mpf;
	DB_MSGBUF_INIT(&mb);

	__db_msg(dbenv, "\tmagic: %#lx", (u_long)dbmeta->magic);
	__db_msg(dbenv, "\tversion: %lu", (u_long)dbmeta->version);
	__db_msg(dbenv, "\tpagesize: %lu", (u_long)dbmeta->pagesize);
	__db_msg(dbenv, "\ttype: %lu", (u_long)dbmeta->type);
	__db_msg(dbenv, "\tkeys: %lu\trecords: %lu",
	    (u_long)dbmeta->key_count, (u_long)dbmeta->record_count);

	/*
	 * If we're doing recovery testing, don't display the free list,
	 * it may have changed and that makes the dump diff not work.
	 */
	if (!LF_ISSET(DB_PR_RECOVERYTEST)) {
		__db_msgadd(
		    dbenv, &mb, "\tfree list: %lu", (u_long)dbmeta->free);
		for (pgno = dbmeta->free,
		    cnt = 0, sep = ", "; pgno != PGNO_INVALID;) {
			if ((ret = __memp_fget(mpf, &pgno, 0, &h)) != 0) {
				DB_MSGBUF_FLUSH(dbenv, &mb);
				__db_msg(dbenv,
			    "Unable to retrieve free-list page: %lu: %s",
				    (u_long)pgno, db_strerror(ret));
				break;
			}
			pgno = h->next_pgno;
			(void)__memp_fput(mpf, h, 0);
			__db_msgadd(dbenv, &mb, "%s%lu", sep, (u_long)pgno);
			if (++cnt % 10 == 0) {
				DB_MSGBUF_FLUSH(dbenv, &mb);
				cnt = 0;
				sep = "\t";
			} else
				sep = ", ";
		}
		DB_MSGBUF_FLUSH(dbenv, &mb);
		__db_msg(dbenv, "\tlast_pgno: %lu", (u_long)dbmeta->last_pgno);
	}

	if (fn != NULL) {
		DB_MSGBUF_FLUSH(dbenv, &mb);
		__db_msgadd(dbenv, &mb, "\tflags: %#lx", (u_long)dbmeta->flags);
		__db_prflags(dbenv, &mb, dbmeta->flags, fn, " (", ")");
	}

	DB_MSGBUF_FLUSH(dbenv, &mb);
	__db_msgadd(dbenv, &mb, "\tuid: ");
	for (p = (u_int8_t *)dbmeta->uid,
	    cnt = 0; cnt < DB_FILE_ID_LEN; ++cnt) {
		__db_msgadd(dbenv, &mb, "%x", *p++);
		if (cnt < DB_FILE_ID_LEN - 1)
			__db_msgadd(dbenv, &mb, " ");
	}
	DB_MSGBUF_FLUSH(dbenv, &mb);
}

/*
 * __db_bmeta --
 *	Print out the btree meta-data page.
 */
static int
__db_bmeta(dbp, h, flags)
	DB *dbp;
	BTMETA *h;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ BTM_DUP,	"duplicates" },
		{ BTM_RECNO,	"recno" },
		{ BTM_RECNUM,	"btree:recnum" },
		{ BTM_FIXEDLEN,	"recno:fixed-length" },
		{ BTM_RENUMBER,	"recno:renumber" },
		{ BTM_SUBDB,	"multiple-databases" },
		{ BTM_DUPSORT,	"sorted duplicates" },
		{ 0,		NULL }
	};
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;

	__db_meta(dbp, (DBMETA *)h, fn, flags);

	__db_msg(dbenv, "\tmaxkey: %lu minkey: %lu",
	    (u_long)h->maxkey, (u_long)h->minkey);
	if (dbp->type == DB_RECNO)
		__db_msg(dbenv, "\tre_len: %#lx re_pad: %#lx",
		    (u_long)h->re_len, (u_long)h->re_pad);
	__db_msg(dbenv, "\troot: %lu", (u_long)h->root);

	return (0);
}

/*
 * __db_hmeta --
 *	Print out the hash meta-data page.
 */
static int
__db_hmeta(dbp, h, flags)
	DB *dbp;
	HMETA *h;
	u_int32_t flags;
{
	DB_MSGBUF mb;
	static const FN fn[] = {
		{ DB_HASH_DUP,		"duplicates" },
		{ DB_HASH_SUBDB,	"multiple-databases" },
		{ DB_HASH_DUPSORT,	"sorted duplicates" },
		{ 0,			NULL }
	};
	DB_ENV *dbenv;
	int i;

	dbenv = dbp->dbenv;
	DB_MSGBUF_INIT(&mb);

	__db_meta(dbp, (DBMETA *)h, fn, flags);

	__db_msg(dbenv, "\tmax_bucket: %lu", (u_long)h->max_bucket);
	__db_msg(dbenv, "\thigh_mask: %#lx", (u_long)h->high_mask);
	__db_msg(dbenv, "\tlow_mask:  %#lx", (u_long)h->low_mask);
	__db_msg(dbenv, "\tffactor: %lu", (u_long)h->ffactor);
	__db_msg(dbenv, "\tnelem: %lu", (u_long)h->nelem);
	__db_msg(dbenv, "\th_charkey: %#lx", (u_long)h->h_charkey);
	__db_msgadd(dbenv, &mb, "\tspare points: ");
	for (i = 0; i < NCACHED; i++)
		__db_msgadd(dbenv, &mb, "%lu ", (u_long)h->spares[i]);
	DB_MSGBUF_FLUSH(dbenv, &mb);

	return (0);
}

/*
 * __db_qmeta --
 *	Print out the queue meta-data page.
 */
static int
__db_qmeta(dbp, h, flags)
	DB *dbp;
	QMETA *h;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;

	__db_meta(dbp, (DBMETA *)h, NULL, flags);

	__db_msg(dbenv, "\tfirst_recno: %lu", (u_long)h->first_recno);
	__db_msg(dbenv, "\tcur_recno: %lu", (u_long)h->cur_recno);
	__db_msg(dbenv, "\tre_len: %#lx re_pad: %lu",
	    (u_long)h->re_len, (u_long)h->re_pad);
	__db_msg(dbenv, "\trec_page: %lu", (u_long)h->rec_page);
	__db_msg(dbenv, "\tpage_ext: %lu", (u_long)h->page_ext);

	return (0);
}

/*
 * __db_prnpage
 *	-- Print out a specific page.
 *
 * PUBLIC: int __db_prnpage __P((DB *, db_pgno_t));
 */
int
__db_prnpage(dbp, pgno)
	DB *dbp;
	db_pgno_t pgno;
{
	DB_MPOOLFILE *mpf;
	PAGE *h;
	int ret, t_ret;

	mpf = dbp->mpf;

	if ((ret = __memp_fget(mpf, &pgno, 0, &h)) != 0)
		return (ret);

	ret = __db_prpage(dbp, h, DB_PR_PAGE);

	if ((t_ret = __memp_fput(mpf, h, 0)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_prpage
 *	-- Print out a page.
 *
 * PUBLIC: int __db_prpage __P((DB *, PAGE *, u_int32_t));
 */
int
__db_prpage(dbp, h, flags)
	DB *dbp;
	PAGE *h;
	u_int32_t flags;
{
	BINTERNAL *bi;
	BKEYDATA *bk;
	DB_ENV *dbenv;
	DB_MSGBUF mb;
	HOFFPAGE a_hkd;
	QAMDATA *qp, *qep;
	RINTERNAL *ri;
	db_indx_t dlen, len, i, *inp;
	db_pgno_t pgno;
	db_recno_t recno;
	u_int32_t pagesize, qlen;
	u_int8_t *ep, *hk, *p;
	int deleted, ret;
	const char *s;
	void *sp;

	dbenv = dbp->dbenv;
	DB_MSGBUF_INIT(&mb);

	/*
	 * If we're doing recovery testing and this page is P_INVALID,
	 * assume it's a page that's on the free list, and don't display it.
	 */
	if (LF_ISSET(DB_PR_RECOVERYTEST) && TYPE(h) == P_INVALID)
		return (0);

	if ((s = __db_pagetype_to_string(TYPE(h))) == NULL) {
		__db_msg(dbenv, "ILLEGAL PAGE TYPE: page: %lu type: %lu",
		    (u_long)h->pgno, (u_long)TYPE(h));
		return (1);
	}

	/*
	 * !!!
	 * Find out the page size.  We don't want to do it the "right" way,
	 * by reading the value from the meta-data page, that's going to be
	 * slow.  Reach down into the mpool region.
	 */
	pagesize = (u_int32_t)dbp->mpf->mfp->stat.st_pagesize;

	/* Page number, page type. */
	__db_msgadd(dbenv, &mb, "page %lu: %s level: %lu",
	    (u_long)h->pgno, s, (u_long)h->level);

	/* Record count. */
	if (TYPE(h) == P_IBTREE ||
	    TYPE(h) == P_IRECNO || (TYPE(h) == P_LRECNO &&
	    h->pgno == ((BTREE *)dbp->bt_internal)->bt_root))
		__db_msgadd(dbenv, &mb, " records: %lu", (u_long)RE_NREC(h));

	/* LSN. */
	if (!LF_ISSET(DB_PR_RECOVERYTEST))
		__db_msgadd(dbenv, &mb, " (lsn.file: %lu lsn.offset: %lu)",
		    (u_long)LSN(h).file, (u_long)LSN(h).offset);
	DB_MSGBUF_FLUSH(dbenv, &mb);

	switch (TYPE(h)) {
	case P_BTREEMETA:
		return (__db_bmeta(dbp, (BTMETA *)h, flags));
	case P_HASHMETA:
		return (__db_hmeta(dbp, (HMETA *)h, flags));
	case P_QAMMETA:
		return (__db_qmeta(dbp, (QMETA *)h, flags));
	case P_QAMDATA:				/* Should be meta->start. */
		if (!LF_ISSET(DB_PR_PAGE))
			return (0);

		qlen = ((QUEUE *)dbp->q_internal)->re_len;
		recno = (h->pgno - 1) * QAM_RECNO_PER_PAGE(dbp) + 1;
		i = 0;
		qep = (QAMDATA *)((u_int8_t *)h + pagesize - qlen);
		for (qp = QAM_GET_RECORD(dbp, h, i); qp < qep;
		    recno++, i++, qp = QAM_GET_RECORD(dbp, h, i)) {
			if (!F_ISSET(qp, QAM_SET))
				continue;

			__db_msgadd(dbenv, &mb, "%s",
			    F_ISSET(qp, QAM_VALID) ? "\t" : "       D");
			__db_msgadd(dbenv, &mb, "[%03lu] %4lu ", (u_long)recno,
			    (u_long)((u_int8_t *)qp - (u_int8_t *)h));
			__db_pr(dbenv, &mb, qp->data, qlen);
		}
		return (0);
	default:
		break;
	}

	/* LSN. */
	if (LF_ISSET(DB_PR_RECOVERYTEST))
		__db_msg(dbenv, " (lsn.file: %lu lsn.offset: %lu)",
		    (u_long)LSN(h).file, (u_long)LSN(h).offset);

	s = "\t";
	if (TYPE(h) != P_IBTREE && TYPE(h) != P_IRECNO) {
		__db_msgadd(dbenv, &mb, "%sprev: %4lu next: %4lu",
		    s, (u_long)PREV_PGNO(h), (u_long)NEXT_PGNO(h));
		s = " ";
	}
	if (TYPE(h) == P_OVERFLOW) {
		__db_msgadd(dbenv, &mb,
		    "%sref cnt: %4lu ", s, (u_long)OV_REF(h));
		__db_pr(dbenv, &mb, (u_int8_t *)h + P_OVERHEAD(dbp), OV_LEN(h));
		return (0);
	}
	__db_msgadd(dbenv, &mb, "%sentries: %4lu", s, (u_long)NUM_ENT(h));
	__db_msgadd(dbenv, &mb, " offset: %4lu", (u_long)HOFFSET(h));
	DB_MSGBUF_FLUSH(dbenv, &mb);

	if (TYPE(h) == P_INVALID || !LF_ISSET(DB_PR_PAGE))
		return (0);

	ret = 0;
	inp = P_INP(dbp, h);
	for (i = 0; i < NUM_ENT(h); i++) {
		if ((uintptr_t)(P_ENTRY(dbp, h, i) - (u_int8_t *)h) <
		    (uintptr_t)(P_OVERHEAD(dbp)) ||
		    (size_t)(P_ENTRY(dbp, h, i) - (u_int8_t *)h) >= pagesize) {
			__db_msg(dbenv,
			    "ILLEGAL PAGE OFFSET: indx: %lu of %lu",
			    (u_long)i, (u_long)inp[i]);
			ret = EINVAL;
			continue;
		}
		deleted = 0;
		switch (TYPE(h)) {
		case P_HASH:
		case P_IBTREE:
		case P_IRECNO:
			sp = P_ENTRY(dbp, h, i);
			break;
		case P_LBTREE:
			sp = P_ENTRY(dbp, h, i);
			deleted = i % 2 == 0 &&
			    B_DISSET(GET_BKEYDATA(dbp, h, i + O_INDX)->type);
			break;
		case P_LDUP:
		case P_LRECNO:
			sp = P_ENTRY(dbp, h, i);
			deleted = B_DISSET(GET_BKEYDATA(dbp, h, i)->type);
			break;
		default:
			goto type_err;
		}
		__db_msgadd(dbenv, &mb, "%s", deleted ? "       D" : "\t");
		__db_msgadd(
		    dbenv, &mb, "[%03lu] %4lu ", (u_long)i, (u_long)inp[i]);
		switch (TYPE(h)) {
		case P_HASH:
			hk = sp;
			switch (HPAGE_PTYPE(hk)) {
			case H_OFFDUP:
				memcpy(&pgno,
				    HOFFDUP_PGNO(hk), sizeof(db_pgno_t));
				__db_msgadd(dbenv, &mb,
				    "%4lu [offpage dups]", (u_long)pgno);
				DB_MSGBUF_FLUSH(dbenv, &mb);
				break;
			case H_DUPLICATE:
				/*
				 * If this is the first item on a page, then
				 * we cannot figure out how long it is, so
				 * we only print the first one in the duplicate
				 * set.
				 */
				if (i != 0)
					len = LEN_HKEYDATA(dbp, h, 0, i);
				else
					len = 1;

				__db_msgadd(dbenv, &mb, "Duplicates:");
				DB_MSGBUF_FLUSH(dbenv, &mb);
				for (p = HKEYDATA_DATA(hk),
				    ep = p + len; p < ep;) {
					memcpy(&dlen, p, sizeof(db_indx_t));
					p += sizeof(db_indx_t);
					__db_msgadd(dbenv, &mb, "\t\t");
					__db_pr(dbenv, &mb, p, dlen);
					p += sizeof(db_indx_t) + dlen;
				}
				break;
			case H_KEYDATA:
				__db_pr(dbenv, &mb, HKEYDATA_DATA(hk),
				    LEN_HKEYDATA(dbp, h, i == 0 ?
				    pagesize : 0, i));
				break;
			case H_OFFPAGE:
				memcpy(&a_hkd, hk, HOFFPAGE_SIZE);
				__db_msgadd(dbenv, &mb,
				    "overflow: total len: %4lu page: %4lu",
				    (u_long)a_hkd.tlen, (u_long)a_hkd.pgno);
				DB_MSGBUF_FLUSH(dbenv, &mb);
				break;
			default:
				DB_MSGBUF_FLUSH(dbenv, &mb);
				__db_msg(dbenv, "ILLEGAL HASH PAGE TYPE: %lu",
				    (u_long)HPAGE_PTYPE(hk));
				ret = EINVAL;
				break;
			}
			break;
		case P_IBTREE:
			bi = sp;
			__db_msgadd(dbenv, &mb,
			    "count: %4lu pgno: %4lu type: %4lu",
			    (u_long)bi->nrecs, (u_long)bi->pgno,
			    (u_long)bi->type);
			switch (B_TYPE(bi->type)) {
			case B_KEYDATA:
				__db_pr(dbenv, &mb, bi->data, bi->len);
				break;
			case B_DUPLICATE:
			case B_OVERFLOW:
				__db_proff(dbenv, &mb, bi->data);
				break;
			default:
				DB_MSGBUF_FLUSH(dbenv, &mb);
				__db_msg(dbenv, "ILLEGAL BINTERNAL TYPE: %lu",
				    (u_long)B_TYPE(bi->type));
				ret = EINVAL;
				break;
			}
			break;
		case P_IRECNO:
			ri = sp;
			__db_msgadd(dbenv, &mb, "entries %4lu pgno %4lu",
			    (u_long)ri->nrecs, (u_long)ri->pgno);
			DB_MSGBUF_FLUSH(dbenv, &mb);
			break;
		case P_LBTREE:
		case P_LDUP:
		case P_LRECNO:
			bk = sp;
			switch (B_TYPE(bk->type)) {
			case B_KEYDATA:
				__db_pr(dbenv, &mb, bk->data, bk->len);
				break;
			case B_DUPLICATE:
			case B_OVERFLOW:
				__db_proff(dbenv, &mb, bk);
				break;
			default:
				DB_MSGBUF_FLUSH(dbenv, &mb);
				__db_msg(dbenv,
			    "ILLEGAL DUPLICATE/LBTREE/LRECNO TYPE: %lu",
				    (u_long)B_TYPE(bk->type));
				ret = EINVAL;
				break;
			}
			break;
		default:
type_err:		DB_MSGBUF_FLUSH(dbenv, &mb);
			__db_msg(dbenv,
			    "ILLEGAL PAGE TYPE: %lu", (u_long)TYPE(h));
			ret = EINVAL;
			continue;
		}
	}
	return (ret);
}

/*
 * __db_pr --
 *	Print out a data element.
 *
 * PUBLIC: void __db_pr __P((DB_ENV *, DB_MSGBUF *, u_int8_t *, u_int32_t));
 */
void
__db_pr(dbenv, mbp, p, len)
	DB_ENV *dbenv;
	DB_MSGBUF *mbp;
	u_int8_t *p;
	u_int32_t len;
{
	u_int32_t i;

	__db_msgadd(dbenv, mbp, "len: %3lu", (u_long)len);
	if (len != 0) {
		__db_msgadd(dbenv, mbp, " data: ");
		for (i = len <= 20 ? len : 20; i > 0; --i, ++p) {
			if (isprint((int)*p) || *p == '\n')
				__db_msgadd(dbenv, mbp, "%c", *p);
			else
				__db_msgadd(dbenv, mbp, "%#.2x", (u_int)*p);
		}
		if (len > 20)
			__db_msgadd(dbenv, mbp, "...");
	}
	DB_MSGBUF_FLUSH(dbenv, mbp);
}

/*
 * __db_proff --
 *	Print out an off-page element.
 */
static void
__db_proff(dbenv, mbp, vp)
	DB_ENV *dbenv;
	DB_MSGBUF *mbp;
	void *vp;
{
	BOVERFLOW *bo;

	bo = vp;
	switch (B_TYPE(bo->type)) {
	case B_OVERFLOW:
		__db_msgadd(dbenv, mbp, "overflow: total len: %4lu page: %4lu",
		    (u_long)bo->tlen, (u_long)bo->pgno);
		break;
	case B_DUPLICATE:
		__db_msgadd(
		    dbenv, mbp, "duplicate: page: %4lu", (u_long)bo->pgno);
		break;
	default:
		/* NOTREACHED */
		break;
	}
	DB_MSGBUF_FLUSH(dbenv, mbp);
}

/*
 * __db_prflags --
 *	Print out flags values.
 *
 * PUBLIC: void __db_prflags __P((DB_ENV *, DB_MSGBUF *,
 * PUBLIC:     u_int32_t, const FN *, const char *, const char *));
 */
void
__db_prflags(dbenv, mbp, flags, fn, prefix, suffix)
	DB_ENV *dbenv;
	DB_MSGBUF *mbp;
	u_int32_t flags;
	FN const *fn;
	const char *prefix, *suffix;
{
	DB_MSGBUF mb;
	const FN *fnp;
	int found, standalone;
	const char *sep;

	/*
	 * If it's a standalone message, output the suffix (which will be the
	 * label), regardless of whether we found anything or not, and flush
	 * the line.
	 */
	if (mbp == NULL) {
		standalone = 1;
		mbp = &mb;
		DB_MSGBUF_INIT(mbp);
	} else
		standalone = 0;

	sep = prefix == NULL ? "" : prefix;
	for (found = 0, fnp = fn; fnp->mask != 0; ++fnp)
		if (LF_ISSET(fnp->mask)) {
			__db_msgadd(dbenv, mbp, "%s%s", sep, fnp->name);
			sep = ", ";
			found = 1;
		}

	if ((standalone || found) && suffix != NULL)
		__db_msgadd(dbenv, mbp, "%s", suffix);
	if (standalone)
		DB_MSGBUF_FLUSH(dbenv, mbp);
}

/*
 * __db_lockmode_to_string --
 *	Return the name of the lock mode.
 *
 * PUBLIC: const char * __db_lockmode_to_string __P((db_lockmode_t));
 */
const char *
__db_lockmode_to_string(mode)
	db_lockmode_t mode;
{
	switch (mode) {
	case DB_LOCK_NG:
		return ("Not granted");
	case DB_LOCK_READ:
		return ("Shared/read");
	case DB_LOCK_WRITE:
		return ("Exclusive/write");
	case DB_LOCK_WAIT:
		return ("Wait for event");
	case DB_LOCK_IWRITE:
		return ("Intent exclusive/write");
	case DB_LOCK_IREAD:
		return ("Intent shared/read");
	case DB_LOCK_IWR:
		return ("Intent to read/write");
	case DB_LOCK_DIRTY:
		return ("Dirty read");
	case DB_LOCK_WWRITE:
		return ("Was written");
	default:
		break;
	}
	return ("UNKNOWN LOCK MODE");
}

/*
 * __db_pagetype_to_string --
 *	Return the name of the specified page type.
 */
static const char *
__db_pagetype_to_string(type)
	u_int32_t type;
{
	char *s;

	s = NULL;
	switch (type) {
	case P_BTREEMETA:
		s = "btree metadata";
		break;
	case P_LDUP:
		s = "duplicate";
		break;
	case P_HASH:
		s = "hash";
		break;
	case P_HASHMETA:
		s = "hash metadata";
		break;
	case P_IBTREE:
		s = "btree internal";
		break;
	case P_INVALID:
		s = "invalid";
		break;
	case P_IRECNO:
		s = "recno internal";
		break;
	case P_LBTREE:
		s = "btree leaf";
		break;
	case P_LRECNO:
		s = "recno leaf";
		break;
	case P_OVERFLOW:
		s = "overflow";
		break;
	case P_QAMMETA:
		s = "queue metadata";
		break;
	case P_QAMDATA:
		s = "queue";
		break;
	default:
		/* Just return a NULL. */
		break;
	}
	return (s);
}

#else /* !HAVE_STATISTICS */

/*
 * __db_dumptree --
 *	Dump the tree to a file.
 *
 * PUBLIC: int __db_dumptree __P((DB *, char *, char *));
 */
int
__db_dumptree(dbp, op, name)
	DB *dbp;
	char *op, *name;
{
	COMPQUIET(op, NULL);
	COMPQUIET(name, NULL);

	return (__db_stat_not_built(dbp->dbenv));
}

/*
 * __db_get_flags_fn --
 *	Return the __db_flags_fn array.
 *
 * PUBLIC: const FN * __db_get_flags_fn __P((void));
 */
const FN *
__db_get_flags_fn()
{
	static const FN __db_flags_fn[] = {
		{ 0,	NULL }
	};

	/*
	 * !!!
	 * The Tcl API uses this interface, stub it off.
	 */
	return (__db_flags_fn);
}
#endif

/*
 * __db_dump_pp --
 *	DB->dump pre/post processing.
 *
 * PUBLIC: int __db_dump_pp __P((DB *, const char *,
 * PUBLIC:     int (*)(void *, const void *), void *, int, int));
 */
int
__db_dump_pp(dbp, subname, callback, handle, pflag, keyflag)
	DB *dbp;
	const char *subname;
	int (*callback) __P((void *, const void *));
	void *handle;
	int pflag, keyflag;
{
	DB_ENV *dbenv;
	int handle_check, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->dump");

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, 1)) != 0)
		return (ret);

	ret = __db_dump(dbp, subname, callback, handle, pflag, keyflag);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (0);
}

/*
 * __db_dump --
 *	DB->dump.
 *
 * PUBLIC: int __db_dump __P((DB *, const char *,
 * PUBLIC:     int (*)(void *, const void *), void *, int, int));
 */
int
__db_dump(dbp, subname, callback, handle, pflag, keyflag)
	DB *dbp;
	const char *subname;
	int (*callback) __P((void *, const void *));
	void *handle;
	int pflag, keyflag;
{
	DB_ENV *dbenv;
	DBC *dbcp;
	DBT key, data;
	DBT keyret, dataret;
	db_recno_t recno;
	int is_recno, ret, t_ret;
	void *pointer;

	dbenv = dbp->dbenv;

	if ((ret = __db_prheader(
	    dbp, subname, pflag, keyflag, handle, callback, NULL, 0)) != 0)
		return (ret);

	/*
	 * Get a cursor and step through the database, printing out each
	 * key/data pair.
	 */
	if ((ret = __db_cursor(dbp, NULL, &dbcp, 0)) != 0)
		return (ret);

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	if ((ret = __os_malloc(dbenv, 1024 * 1024, &data.data)) != 0)
		goto err;
	data.ulen = 1024 * 1024;
	data.flags = DB_DBT_USERMEM;
	is_recno = (dbp->type == DB_RECNO || dbp->type == DB_QUEUE);
	keyflag = is_recno ? keyflag : 1;
	if (is_recno) {
		keyret.data = &recno;
		keyret.size = sizeof(recno);
	}

retry: while ((ret =
	    __db_c_get(dbcp, &key, &data, DB_NEXT | DB_MULTIPLE_KEY)) == 0) {
		DB_MULTIPLE_INIT(pointer, &data);
		for (;;) {
			if (is_recno)
				DB_MULTIPLE_RECNO_NEXT(pointer, &data,
				    recno, dataret.data, dataret.size);
			else
				DB_MULTIPLE_KEY_NEXT(pointer,
				    &data, keyret.data,
				    keyret.size, dataret.data, dataret.size);

			if (dataret.data == NULL)
				break;

			if ((keyflag &&
			    (ret = __db_prdbt(&keyret, pflag, " ",
			    handle, callback, is_recno)) != 0) ||
			    (ret = __db_prdbt(&dataret, pflag, " ",
			    handle, callback, 0)) != 0)
				goto err;
		}
	}
	if (ret == DB_BUFFER_SMALL) {
		data.size = (u_int32_t)DB_ALIGN(data.size, 1024);
		if ((ret = __os_realloc(dbenv, data.size, &data.data)) != 0)
			goto err;
		data.ulen = data.size;
		goto retry;
	}

	(void)__db_prfooter(handle, callback);

err:	if ((t_ret = __db_c_close(dbcp)) != 0 && ret == 0)
		ret = t_ret;
	if (data.data != NULL)
		__os_free(dbenv, data.data);

	return (ret);
}

/*
 * __db_prdbt --
 *	Print out a DBT data element.
 *
 * PUBLIC: int __db_prdbt __P((DBT *, int, const char *, void *,
 * PUBLIC:     int (*)(void *, const void *), int));
 */
int
__db_prdbt(dbtp, checkprint, prefix, handle, callback, is_recno)
	DBT *dbtp;
	int checkprint;
	const char *prefix;
	void *handle;
	int (*callback) __P((void *, const void *));
	int is_recno;
{
	static const u_char hex[] = "0123456789abcdef";
	db_recno_t recno;
	size_t len;
	int ret;
#define	DBTBUFLEN	100
	u_int8_t *p, *hp;
	char buf[DBTBUFLEN], hbuf[DBTBUFLEN];

	/*
	 * !!!
	 * This routine is the routine that dumps out items in the format
	 * used by db_dump(1) and db_load(1).  This means that the format
	 * cannot change.
	 */
	if (prefix != NULL && (ret = callback(handle, prefix)) != 0)
		return (ret);
	if (is_recno) {
		/*
		 * We're printing a record number, and this has to be done
		 * in a platform-independent way.  So we use the numeral in
		 * straight ASCII.
		 */
		(void)__ua_memcpy(&recno, dbtp->data, sizeof(recno));
		snprintf(buf, DBTBUFLEN, "%lu", (u_long)recno);

		/* If we're printing data as hex, print keys as hex too. */
		if (!checkprint) {
			for (len = strlen(buf), p = (u_int8_t *)buf,
			    hp = (u_int8_t *)hbuf; len-- > 0; ++p) {
				*hp++ = hex[(u_int8_t)(*p & 0xf0) >> 4];
				*hp++ = hex[*p & 0x0f];
			}
			*hp = '\0';
			ret = callback(handle, hbuf);
		} else
			ret = callback(handle, buf);

		if (ret != 0)
			return (ret);
	} else if (checkprint) {
		for (len = dbtp->size, p = dbtp->data; len--; ++p)
			if (isprint((int)*p)) {
				if (*p == '\\' &&
				    (ret = callback(handle, "\\")) != 0)
					return (ret);
				snprintf(buf, DBTBUFLEN, "%c", *p);
				if ((ret = callback(handle, buf)) != 0)
					return (ret);
			} else {
				snprintf(buf, DBTBUFLEN, "\\%c%c",
				    hex[(u_int8_t)(*p & 0xf0) >> 4],
				    hex[*p & 0x0f]);
				if ((ret = callback(handle, buf)) != 0)
					return (ret);
			}
	} else
		for (len = dbtp->size, p = dbtp->data; len--; ++p) {
			snprintf(buf, DBTBUFLEN, "%c%c",
			    hex[(u_int8_t)(*p & 0xf0) >> 4],
			    hex[*p & 0x0f]);
			if ((ret = callback(handle, buf)) != 0)
				return (ret);
		}

	return (callback(handle, "\n"));
}

/*
 * __db_prheader --
 *	Write out header information in the format expected by db_load.
 *
 * PUBLIC: int	__db_prheader __P((DB *, const char *, int, int, void *,
 * PUBLIC:     int (*)(void *, const void *), VRFY_DBINFO *, db_pgno_t));
 */
int
__db_prheader(dbp, subname, pflag, keyflag, handle, callback, vdp, meta_pgno)
	DB *dbp;
	const char *subname;
	int pflag, keyflag;
	void *handle;
	int (*callback) __P((void *, const void *));
	VRFY_DBINFO *vdp;
	db_pgno_t meta_pgno;
{
	DBT dbt;
	DB_ENV *dbenv;
	DBTYPE dbtype;
	VRFY_PAGEINFO *pip;
	u_int32_t flags, tmp_u_int32;
	size_t buflen;
	char *buf;
	int using_vdp, ret, t_ret, tmp_int;

	ret = 0;
	buf = NULL;
	COMPQUIET(buflen, 0);

	/*
	 * If dbp is NULL, then pip is guaranteed to be non-NULL; we only ever
	 * call __db_prheader with a NULL dbp from one case inside __db_prdbt,
	 * and this is a special subdatabase for "lost" items.  In this case
	 * we have a vdp (from which we'll get a pip).  In all other cases, we
	 * will have a non-NULL dbp (and vdp may or may not be NULL depending
	 * on whether we're salvaging).
	 */
	DB_ASSERT(dbp != NULL || vdp != NULL);

	if (dbp == NULL)
		dbenv = NULL;
	else
		dbenv = dbp->dbenv;

	/*
	 * If we've been passed a verifier statistics object, use that;  we're
	 * being called in a context where dbp->stat is unsafe.
	 *
	 * Also, the verifier may set the pflag on a per-salvage basis.  If so,
	 * respect that.
	 */
	if (vdp != NULL) {
		if ((ret = __db_vrfy_getpageinfo(vdp, meta_pgno, &pip)) != 0)
			return (ret);

		if (F_ISSET(vdp, SALVAGE_PRINTABLE))
			pflag = 1;
		using_vdp = 1;
	} else {
		pip = NULL;
		using_vdp = 0;
	}

	/*
	 * If dbp is NULL, make it a btree.  Otherwise, set dbtype to whatever
	 * appropriate type for the specified meta page, or the type of the dbp.
	 */
	if (dbp == NULL)
		dbtype = DB_BTREE;
	else if (using_vdp)
		switch (pip->type) {
		case P_BTREEMETA:
			if (F_ISSET(pip, VRFY_IS_RECNO))
				dbtype = DB_RECNO;
			else
				dbtype = DB_BTREE;
			break;
		case P_HASHMETA:
			dbtype = DB_HASH;
			break;
		case P_QAMMETA:
			dbtype = DB_QUEUE;
			break;
		default:
			/*
			 * If the meta page is of a bogus type, it's because
			 * we have a badly corrupt database.  (We must be in
			 * the verifier for pip to be non-NULL.) Pretend we're
			 * a Btree and salvage what we can.
			 */
			DB_ASSERT(F_ISSET(dbp, DB_AM_VERIFYING));
			dbtype = DB_BTREE;
			break;
		}
	else
		dbtype = dbp->type;

	if ((ret = callback(handle, "VERSION=3\n")) != 0)
		goto err;
	if (pflag) {
		if ((ret = callback(handle, "format=print\n")) != 0)
			goto err;
	} else if ((ret = callback(handle, "format=bytevalue\n")) != 0)
		goto err;

	/*
	 * 64 bytes is long enough, as a minimum bound, for any of the
	 * fields besides subname.  Subname uses __db_prdbt and therefore
	 * does not need buffer space here.
	 */
	buflen = 64;
	if ((ret = __os_malloc(dbenv, buflen, &buf)) != 0)
		goto err;
	if (subname != NULL) {
		snprintf(buf, buflen, "database=");
		if ((ret = callback(handle, buf)) != 0)
			goto err;
		memset(&dbt, 0, sizeof(dbt));
		dbt.data = (char *)subname;
		dbt.size = (u_int32_t)strlen(subname);
		if ((ret = __db_prdbt(&dbt, 1, NULL, handle, callback, 0)) != 0)
			goto err;
	}
	switch (dbtype) {
	case DB_BTREE:
		if ((ret = callback(handle, "type=btree\n")) != 0)
			goto err;
		if (using_vdp) {
			if (F_ISSET(pip, VRFY_HAS_RECNUMS))
				if ((ret =
				    callback(handle, "recnum=1\n")) != 0)
					goto err;
			if (pip->bt_maxkey != 0) {
				snprintf(buf, buflen,
				    "bt_maxkey=%lu\n", (u_long)pip->bt_maxkey);
				if ((ret = callback(handle, buf)) != 0)
					goto err;
			}
			if (pip->bt_minkey != 0 &&
			    pip->bt_minkey != DEFMINKEYPAGE) {
				snprintf(buf, buflen,
				    "bt_minkey=%lu\n", (u_long)pip->bt_minkey);
				if ((ret = callback(handle, buf)) != 0)
					goto err;
			}
			break;
		}

		if ((ret = __db_get_flags(dbp, &flags)) != 0) {
			__db_err(dbenv, "DB->get_flags: %s", db_strerror(ret));
			goto err;
		}
		if (F_ISSET(dbp, DB_AM_RECNUM))
			if ((ret = callback(handle, "recnum=1\n")) != 0)
				goto err;
		if ((ret = __bam_get_bt_minkey(dbp, &tmp_u_int32)) != 0) {
			__db_err(dbenv,
			    "DB->get_bt_minkey: %s", db_strerror(ret));
			goto err;
		}
		if (tmp_u_int32 != 0 && tmp_u_int32 != DEFMINKEYPAGE) {
			snprintf(buf, buflen,
			    "bt_minkey=%lu\n", (u_long)tmp_u_int32);
			if ((ret = callback(handle, buf)) != 0)
				goto err;
		}
		break;
	case DB_HASH:
#ifdef HAVE_HASH
		if ((ret = callback(handle, "type=hash\n")) != 0)
			goto err;
		if (using_vdp) {
			if (pip->h_ffactor != 0) {
				snprintf(buf, buflen,
				    "h_ffactor=%lu\n", (u_long)pip->h_ffactor);
				if ((ret = callback(handle, buf)) != 0)
					goto err;
			}
			if (pip->h_nelem != 0) {
				snprintf(buf, buflen,
				    "h_nelem=%lu\n", (u_long)pip->h_nelem);
				if ((ret = callback(handle, buf)) != 0)
					goto err;
			}
			break;
		}
		if ((ret = __ham_get_h_ffactor(dbp, &tmp_u_int32)) != 0) {
			__db_err(dbenv,
			    "DB->get_h_ffactor: %s", db_strerror(ret));
			goto err;
		}
		if (tmp_u_int32 != 0) {
			snprintf(buf, buflen,
			    "h_ffactor=%lu\n", (u_long)tmp_u_int32);
			if ((ret = callback(handle, buf)) != 0)
				goto err;
		}
		if ((ret = __ham_get_h_nelem(dbp, &tmp_u_int32)) != 0) {
			__db_err(dbenv,
			    "DB->get_h_nelem: %s", db_strerror(ret));
			goto err;
		}
		if (tmp_u_int32 != 0) {
			snprintf(buf, buflen,
			    "h_nelem=%lu\n", (u_long)tmp_u_int32);
			if ((ret = callback(handle, buf)) != 0)
				goto err;
		}
		break;
#else
		ret = __db_no_hash_am(dbenv);
		goto err;
#endif
	case DB_QUEUE:
#ifdef HAVE_QUEUE
		if ((ret = callback(handle, "type=queue\n")) != 0)
			goto err;
		if (vdp != NULL) {
			snprintf(buf,
			    buflen, "re_len=%lu\n", (u_long)vdp->re_len);
			if ((ret = callback(handle, buf)) != 0)
				goto err;
			break;
		}
		if ((ret = __ram_get_re_len(dbp, &tmp_u_int32)) != 0) {
			__db_err(dbenv,
			    "DB->get_re_len: %s", db_strerror(ret));
			goto err;
		}
		snprintf(buf, buflen, "re_len=%lu\n", (u_long)tmp_u_int32);
		if ((ret = callback(handle, buf)) != 0)
			goto err;
		if ((ret = __ram_get_re_pad(dbp, &tmp_int)) != 0) {
			__db_err(dbenv,
			    "DB->get_re_pad: %s", db_strerror(ret));
			goto err;
		}
		if (tmp_int != 0 && tmp_int != ' ') {
			snprintf(buf, buflen, "re_pad=%#x\n", tmp_int);
			if ((ret = callback(handle, buf)) != 0)
				goto err;
		}
		if ((ret = __qam_get_extentsize(dbp, &tmp_u_int32)) != 0) {
			__db_err(dbenv,
			    "DB->get_q_extentsize: %s", db_strerror(ret));
			goto err;
		}
		if (tmp_u_int32 != 0) {
			snprintf(buf, buflen,
			    "extentsize=%lu\n", (u_long)tmp_u_int32);
			if ((ret = callback(handle, buf)) != 0)
				goto err;
		}
		break;
#else
		ret = __db_no_queue_am(dbenv);
		goto err;
#endif
	case DB_RECNO:
		if ((ret = callback(handle, "type=recno\n")) != 0)
			goto err;
		if (using_vdp) {
			if (F_ISSET(pip, VRFY_IS_RRECNO))
				if ((ret =
				    callback(handle, "renumber=1\n")) != 0)
					goto err;
			if (pip->re_len > 0) {
				snprintf(buf, buflen,
				    "re_len=%lu\n", (u_long)pip->re_len);
				if ((ret = callback(handle, buf)) != 0)
					goto err;
			}
			break;
		}
		if (F_ISSET(dbp, DB_AM_RENUMBER))
			if ((ret = callback(handle, "renumber=1\n")) != 0)
				goto err;
		if (F_ISSET(dbp, DB_AM_FIXEDLEN)) {
			if ((ret = __ram_get_re_len(dbp, &tmp_u_int32)) != 0) {
				__db_err(dbenv,
				    "DB->get_re_len: %s", db_strerror(ret));
				goto err;
			}
			snprintf(buf, buflen,
			    "re_len=%lu\n", (u_long)tmp_u_int32);
			if ((ret = callback(handle, buf)) != 0)
				goto err;

			if ((ret = __ram_get_re_pad(dbp, &tmp_int)) != 0) {
				__db_err(dbenv,
				    "DB->get_re_pad: %s", db_strerror(ret));
				goto err;
			}
			if (tmp_int != 0 && tmp_int != ' ') {
				snprintf(buf,
				    buflen, "re_pad=%#x\n", (u_int)tmp_int);
				if ((ret = callback(handle, buf)) != 0)
					goto err;
			}
		}
		break;
	case DB_UNKNOWN:
		DB_ASSERT(0);			/* Impossible. */
		__db_err(dbenv,
		    "Unknown or unsupported DB type in __db_prheader");
		ret = EINVAL;
		goto err;
	}

	if (using_vdp) {
		if (F_ISSET(pip, VRFY_HAS_DUPS))
			if ((ret = callback(handle, "duplicates=1\n")) != 0)
				goto err;
		if (F_ISSET(pip, VRFY_HAS_DUPSORT))
			if ((ret = callback(handle, "dupsort=1\n")) != 0)
				goto err;
		/* We should handle page size. XXX */
	} else {
		if (F_ISSET(dbp, DB_AM_CHKSUM))
			if ((ret = callback(handle, "chksum=1\n")) != 0)
				goto err;
		if (F_ISSET(dbp, DB_AM_DUP))
			if ((ret = callback(handle, "duplicates=1\n")) != 0)
				goto err;
		if (F_ISSET(dbp, DB_AM_DUPSORT))
			if ((ret = callback(handle, "dupsort=1\n")) != 0)
				goto err;
		if (!F_ISSET(dbp, DB_AM_PGDEF)) {
			snprintf(buf, buflen,
			    "db_pagesize=%lu\n", (u_long)dbp->pgsize);
			if ((ret = callback(handle, buf)) != 0)
				goto err;
		}
	}

	if (keyflag && (ret = callback(handle, "keys=1\n")) != 0)
		goto err;

	ret = callback(handle, "HEADER=END\n");

err:	if (using_vdp &&
	    (t_ret = __db_vrfy_putpageinfo(dbenv, vdp, pip)) != 0 && ret == 0)
		ret = t_ret;
	if (buf != NULL)
		__os_free(dbenv, buf);

	return (ret);
}

/*
 * __db_prfooter --
 *	Print the footer that marks the end of a DB dump.  This is trivial,
 *	but for consistency's sake we don't want to put its literal contents
 *	in multiple places.
 *
 * PUBLIC: int __db_prfooter __P((void *, int (*)(void *, const void *)));
 */
int
__db_prfooter(handle, callback)
	void *handle;
	int (*callback) __P((void *, const void *));
{
	return (callback(handle, "DATA=END\n"));
}

/*
 * __db_pr_callback --
 *	Callback function for using pr_* functions from C.
 *
 * PUBLIC: int  __db_pr_callback __P((void *, const void *));
 */
int
__db_pr_callback(handle, str_arg)
	void *handle;
	const void *str_arg;
{
	char *str;
	FILE *f;

	str = (char *)str_arg;
	f = (FILE *)handle;

	if (fprintf(f, "%s", str) != (int)strlen(str))
		return (EIO);

	return (0);
}

/*
 * __db_dbtype_to_string --
 *	Return the name of the database type.
 *
 * PUBLIC: const char * __db_dbtype_to_string __P((DBTYPE));
 */
const char *
__db_dbtype_to_string(type)
	DBTYPE type;
{
	switch (type) {
	case DB_BTREE:
		return ("btree");
	case DB_HASH:
		return ("hash");
	case DB_RECNO:
		return ("recno");
	case DB_QUEUE:
		return ("queue");
	case DB_UNKNOWN:
	default:
		break;
	}
	return ("UNKNOWN TYPE");
}
