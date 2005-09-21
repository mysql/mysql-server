/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: qam_stat.c,v 11.47 2004/09/22 16:29:47 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"
#include "dbinc/qam.h"

#ifdef HAVE_STATISTICS
/*
 * __qam_stat --
 *	Gather/print the qam statistics
 *
 * PUBLIC: int __qam_stat __P((DBC *, void *, u_int32_t));
 */
int
__qam_stat(dbc, spp, flags)
	DBC *dbc;
	void *spp;
	u_int32_t flags;
{
	DB *dbp;
	DB_LOCK lock;
	DB_MPOOLFILE *mpf;
	DB_QUEUE_STAT *sp;
	PAGE *h;
	QAMDATA *qp, *ep;
	QMETA *meta;
	QUEUE *t;
	db_indx_t indx;
	db_pgno_t first, last, pgno, pg_ext, stop;
	u_int32_t re_len;
	int ret, t_ret;

	dbp = dbc->dbp;

	LOCK_INIT(lock);
	mpf = dbp->mpf;
	sp = NULL;
	t = dbp->q_internal;

	if (spp == NULL)
		return (0);

	/* Allocate and clear the structure. */
	if ((ret = __os_umalloc(dbp->dbenv, sizeof(*sp), &sp)) != 0)
		goto err;
	memset(sp, 0, sizeof(*sp));

	re_len = ((QUEUE *)dbp->q_internal)->re_len;

	/* Determine the last page of the database. */
	if ((ret = __db_lget(dbc, 0, t->q_meta, DB_LOCK_READ, 0, &lock)) != 0)
		goto err;
	if ((ret = __memp_fget(mpf, &t->q_meta, 0, &meta)) != 0)
		goto err;

	if (flags == DB_FAST_STAT || flags == DB_CACHED_COUNTS) {
		sp->qs_nkeys = meta->dbmeta.key_count;
		sp->qs_ndata = meta->dbmeta.record_count;
		goto meta_only;
	}

	first = QAM_RECNO_PAGE(dbp, meta->first_recno);
	last = QAM_RECNO_PAGE(dbp, meta->cur_recno);

	ret = __memp_fput(mpf, meta, 0);
	if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		goto err;

	pgno = first;
	if (first > last)
		stop = QAM_RECNO_PAGE(dbp, UINT32_MAX);
	else
		stop = last;

	/* Dump each page. */
	pg_ext = ((QUEUE *)dbp->q_internal)->page_ext;
begin:
	/* Walk through the pages and count. */
	for (; pgno <= stop; ++pgno) {
		if ((ret =
		    __db_lget(dbc, 0, pgno, DB_LOCK_READ, 0, &lock)) != 0)
			goto err;
		ret = __qam_fget(dbp, &pgno, 0, &h);
		if (ret == ENOENT) {
			pgno += pg_ext - 1;
			continue;
		}
		if (ret == DB_PAGE_NOTFOUND) {
			if (pg_ext == 0) {
				if (pgno != stop && first != last)
					goto err;
				ret = 0;
				break;
			}
			pgno += (pg_ext - ((pgno - 1) % pg_ext)) - 1;
			continue;
		}
		if (ret != 0)
			goto err;

		++sp->qs_pages;

		ep = (QAMDATA *)((u_int8_t *)h + dbp->pgsize - re_len);
		for (indx = 0, qp = QAM_GET_RECORD(dbp, h, indx);
		    qp <= ep;
		    ++indx,  qp = QAM_GET_RECORD(dbp, h, indx)) {
			if (F_ISSET(qp, QAM_VALID))
				sp->qs_ndata++;
			else
				sp->qs_pgfree += re_len;
		}

		ret = __qam_fput(dbp, pgno, h, 0);
		if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			goto err;
	}

	if ((ret = __LPUT(dbc, lock)) != 0)
		goto err;
	if (first > last) {
		pgno = 1;
		stop = last;
		first = last;
		goto begin;
	}

	/* Get the meta-data page. */
	if ((ret = __db_lget(dbc,
	    0, t->q_meta, F_ISSET(dbp, DB_AM_RDONLY) ?
	    DB_LOCK_READ : DB_LOCK_WRITE, 0, &lock)) != 0)
		goto err;
	if ((ret = __memp_fget(mpf, &t->q_meta, 0, &meta)) != 0)
		goto err;

	if (!F_ISSET(dbp, DB_AM_RDONLY))
		meta->dbmeta.key_count =
		    meta->dbmeta.record_count = sp->qs_ndata;
	sp->qs_nkeys = sp->qs_ndata;

meta_only:
	/* Get the metadata fields. */
	sp->qs_magic = meta->dbmeta.magic;
	sp->qs_version = meta->dbmeta.version;
	sp->qs_metaflags = meta->dbmeta.flags;
	sp->qs_pagesize = meta->dbmeta.pagesize;
	sp->qs_extentsize = meta->page_ext;
	sp->qs_re_len = meta->re_len;
	sp->qs_re_pad = meta->re_pad;
	sp->qs_first_recno = meta->first_recno;
	sp->qs_cur_recno = meta->cur_recno;

	/* Discard the meta-data page. */
	ret = __memp_fput(mpf,
	    meta, F_ISSET(dbp, DB_AM_RDONLY) ? 0 : DB_MPOOL_DIRTY);
	if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		goto err;

	*(DB_QUEUE_STAT **)spp = sp;

	if (0) {
err:		if (sp != NULL)
			__os_ufree(dbp->dbenv, sp);
	}

	if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __qam_stat_print --
 *	Display queue statistics.
 *
 * PUBLIC: int __qam_stat_print __P((DBC *, u_int32_t));
 */
int
__qam_stat_print(dbc, flags)
	DBC *dbc;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_QUEUE_STAT *sp;
	int ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	if ((ret = __qam_stat(dbc, &sp, 0)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Default Queue database information:");
	}
	__db_msg(dbenv, "%lx\tQueue magic number", (u_long)sp->qs_magic);
	__db_msg(dbenv, "%lu\tQueue version number", (u_long)sp->qs_version);
	__db_dl(dbenv, "Fixed-length record size", (u_long)sp->qs_re_len);
	__db_msg(dbenv, "%#x\tFixed-length record pad", (int)sp->qs_re_pad);
	__db_dl(dbenv,
	    "Underlying database page size", (u_long)sp->qs_pagesize);
	__db_dl(dbenv,
	    "Underlying database extent size", (u_long)sp->qs_extentsize);
	__db_dl(dbenv,
	    "Number of records in the database", (u_long)sp->qs_nkeys);
	__db_dl(dbenv, "Number of database pages", (u_long)sp->qs_pages);
	__db_dl_pct(dbenv,
	    "Number of bytes free in database pages",
	    (u_long)sp->qs_pgfree,
	    DB_PCT_PG(sp->qs_pgfree, sp->qs_pages, sp->qs_pagesize), "ff");
	__db_msg(dbenv,
	    "%lu\tFirst undeleted record", (u_long)sp->qs_first_recno);
	__db_msg(dbenv,
	    "%lu\tNext available record number", (u_long)sp->qs_cur_recno);

	__os_ufree(dbenv, sp);

	return (0);
}

#else /* !HAVE_STATISTICS */

int
__qam_stat(dbc, spp, flags)
	DBC *dbc;
	void *spp;
	u_int32_t flags;
{
	COMPQUIET(spp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbc->dbp->dbenv));
}
#endif
