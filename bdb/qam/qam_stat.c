/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam_stat.c,v 11.32 2002/05/11 13:40:11 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/qam.h"

/*
 * __qam_stat --
 *	Gather/print the qam statistics
 *
 * PUBLIC: int __qam_stat __P((DB *, void *, u_int32_t));
 */
int
__qam_stat(dbp, spp, flags)
	DB *dbp;
	void *spp;
	u_int32_t flags;
{
	DBC *dbc;
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

	PANIC_CHECK(dbp->dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->stat");

	LOCK_INIT(lock);
	mpf = dbp->mpf;
	sp = NULL;
	t = dbp->q_internal;

	/* Check for invalid flags. */
	if ((ret = __db_statchk(dbp, flags)) != 0)
		return (ret);

	if (spp == NULL)
		return (0);

	/* Acquire a cursor. */
	if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, NULL, "qam_stat", NULL, NULL, flags);

	/* Allocate and clear the structure. */
	if ((ret = __os_umalloc(dbp->dbenv, sizeof(*sp), &sp)) != 0)
		goto err;
	memset(sp, 0, sizeof(*sp));

	re_len = ((QUEUE *)dbp->q_internal)->re_len;

	/* Determine the last page of the database. */
	if ((ret = __db_lget(dbc,
	    0, t->q_meta, DB_LOCK_READ, 0, &lock)) != 0)
		goto err;
	if ((ret = mpf->get(mpf, &t->q_meta, 0, (PAGE **)&meta)) != 0)
		goto err;

	if (flags == DB_FAST_STAT || flags == DB_CACHED_COUNTS) {
		sp->qs_nkeys = meta->dbmeta.key_count;
		sp->qs_ndata = meta->dbmeta.record_count;
		goto meta_only;
	}

	first = QAM_RECNO_PAGE(dbp, meta->first_recno);
	last = QAM_RECNO_PAGE(dbp, meta->cur_recno);

	if ((ret = mpf->put(mpf, meta, 0)) != 0)
		goto err;
	(void)__LPUT(dbc, lock);

	pgno = first;
	if (first > last)
		stop = QAM_RECNO_PAGE(dbp, UINT32_T_MAX);
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
			pgno += pg_ext - ((pgno - 1) % pg_ext) - 1;
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

		if ((ret = __qam_fput(dbp, pgno, h, 0)) != 0)
			goto err;
		(void)__LPUT(dbc, lock);
	}

	(void)__LPUT(dbc, lock);
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
	if ((ret = mpf->get(mpf, &t->q_meta, 0, (PAGE **)&meta)) != 0)
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
	if ((ret = mpf->put(mpf,
	    meta, F_ISSET(dbp, DB_AM_RDONLY) ? 0 : DB_MPOOL_DIRTY)) != 0)
		goto err;
	(void)__LPUT(dbc, lock);

	*(DB_QUEUE_STAT **)spp = sp;
	ret = 0;

	if (0) {
err:		if (sp != NULL)
			__os_ufree(dbp->dbenv, sp);
	}

	(void)__LPUT(dbc, lock);

	if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}
