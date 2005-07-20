/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: txn_stat.c,v 11.37 2004/10/15 16:59:44 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

#ifdef HAVE_STATISTICS
static int  __txn_compare __P((const void *, const void *));
static int  __txn_print_all __P((DB_ENV *, u_int32_t));
static int  __txn_print_stats __P((DB_ENV *, u_int32_t));
static int  __txn_stat __P((DB_ENV *, DB_TXN_STAT **, u_int32_t));
static void __txn_xid_stats __P((DB_ENV *, DB_MSGBUF *, DB_TXN_ACTIVE *));

/*
 * __txn_stat_pp --
 *	DB_ENV->txn_stat pre/post processing.
 *
 * PUBLIC: int __txn_stat_pp __P((DB_ENV *, DB_TXN_STAT **, u_int32_t));
 */
int
__txn_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_TXN_STAT **statp;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->tx_handle, "DB_ENV->txn_stat", DB_INIT_TXN);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->txn_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __txn_stat(dbenv, statp, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __txn_stat --
 *	DB_ENV->txn_stat.
 */
static int
__txn_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_TXN_STAT **statp;
	u_int32_t flags;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	DB_TXN_STAT *stats;
	TXN_DETAIL *txnp;
	size_t nbytes;
	u_int32_t maxtxn, ndx;
	int ret;

	*statp = NULL;
	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	/*
	 * Allocate for the maximum active transactions -- the DB_TXN_ACTIVE
	 * struct is small and the maximum number of active transactions is
	 * not going to be that large.  Don't have to lock anything to look
	 * at the region's maximum active transactions value, it's read-only
	 * and never changes after the region is created.
	 *
	 * The maximum active transactions isn't a hard limit, so allocate
	 * some extra room, and don't walk off the end.
	 */
	maxtxn = region->maxtxns + (region->maxtxns / 10) + 10;
	nbytes = sizeof(DB_TXN_STAT) + sizeof(DB_TXN_ACTIVE) * maxtxn;
	if ((ret = __os_umalloc(dbenv, nbytes, &stats)) != 0)
		return (ret);

	R_LOCK(dbenv, &mgr->reginfo);
	memcpy(stats, &region->stat, sizeof(*stats));
	stats->st_last_txnid = region->last_txnid;
	stats->st_last_ckp = region->last_ckp;
	stats->st_time_ckp = region->time_ckp;
	stats->st_txnarray = (DB_TXN_ACTIVE *)&stats[1];

	for (ndx = 0,
	    txnp = SH_TAILQ_FIRST(&region->active_txn, __txn_detail);
	    txnp != NULL && ndx < maxtxn;
	    txnp = SH_TAILQ_NEXT(txnp, links, __txn_detail), ++ndx) {
		stats->st_txnarray[ndx].txnid = txnp->txnid;
		if (txnp->parent == INVALID_ROFF)
			stats->st_txnarray[ndx].parentid = TXN_INVALID;
		else
			stats->st_txnarray[ndx].parentid =
			    ((TXN_DETAIL *)R_ADDR(&mgr->reginfo,
			    txnp->parent))->txnid;
		stats->st_txnarray[ndx].lsn = txnp->begin_lsn;
		if ((stats->st_txnarray[ndx].xa_status = txnp->xa_status) != 0)
			memcpy(stats->st_txnarray[ndx].xid,
			    txnp->xid, DB_XIDDATASIZE);
	}

	stats->st_region_wait = mgr->reginfo.rp->mutex.mutex_set_wait;
	stats->st_region_nowait = mgr->reginfo.rp->mutex.mutex_set_nowait;
	stats->st_regsize = mgr->reginfo.rp->size;
	if (LF_ISSET(DB_STAT_CLEAR)) {
		mgr->reginfo.rp->mutex.mutex_set_wait = 0;
		mgr->reginfo.rp->mutex.mutex_set_nowait = 0;
		memset(&region->stat, 0, sizeof(region->stat));
		region->stat.st_maxtxns = region->maxtxns;
		region->stat.st_maxnactive =
		    region->stat.st_nactive = stats->st_nactive;
	}

	R_UNLOCK(dbenv, &mgr->reginfo);

	*statp = stats;
	return (0);
}

/*
 * __txn_stat_print_pp --
 *	DB_ENV->txn_stat_print pre/post processing.
 *
 * PUBLIC: int __txn_stat_print_pp __P((DB_ENV *, u_int32_t));
 */
int
__txn_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->tx_handle, "DB_ENV->txn_stat_print", DB_INIT_TXN);

	if ((ret = __db_fchk(dbenv, "DB_ENV->txn_stat",
	    flags, DB_STAT_ALL | DB_STAT_CLEAR)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __txn_stat_print(dbenv, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __txn_stat_print
 *	DB_ENV->txn_stat_print method.
 *
 * PUBLIC: int  __txn_stat_print __P((DB_ENV *, u_int32_t));
 */
int
__txn_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	u_int32_t orig_flags;
	int ret;

	orig_flags = flags;
	LF_CLR(DB_STAT_CLEAR);
	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		ret = __txn_print_stats(dbenv, orig_flags);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL) &&
	    (ret = __txn_print_all(dbenv, orig_flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __txn_print_stats --
 *	Display default transaction region statistics.
 */
static int
__txn_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_MSGBUF mb;
	DB_TXN_STAT *sp;
	u_int32_t i;
	int ret;

	if ((ret = __txn_stat(dbenv, &sp, flags)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL))
		__db_msg(dbenv, "Default transaction region information:");
	__db_msg(dbenv, "%lu/%lu\t%s",
	    (u_long)sp->st_last_ckp.file, (u_long)sp->st_last_ckp.offset,
	    sp->st_last_ckp.file == 0 ?
	    "No checkpoint LSN" : "File/offset for last checkpoint LSN");
	if (sp->st_time_ckp == 0)
		__db_msg(dbenv, "0\tNo checkpoint timestamp");
	else
		__db_msg(dbenv, "%.24s\tCheckpoint timestamp",
		    ctime(&sp->st_time_ckp));
	__db_msg(dbenv, "%#lx\tLast transaction ID allocated",
	    (u_long)sp->st_last_txnid);
	__db_dl(dbenv, "Maximum number of active transactions configured",
	    (u_long)sp->st_maxtxns);
	__db_dl(dbenv, "Active transactions", (u_long)sp->st_nactive);
	__db_dl(dbenv,
	    "Maximum active transactions", (u_long)sp->st_maxnactive);
	__db_dl(dbenv,
	    "Number of transactions begun", (u_long)sp->st_nbegins);
	__db_dl(dbenv,
	    "Number of transactions aborted", (u_long)sp->st_naborts);
	__db_dl(dbenv,
	    "Number of transactions committed", (u_long)sp->st_ncommits);
	__db_dl(dbenv,
	    "Number of transactions restored", (u_long)sp->st_nrestores);

	__db_dlbytes(dbenv, "Transaction region size",
	    (u_long)0, (u_long)0, (u_long)sp->st_regsize);
	__db_dl_pct(dbenv,
	    "The number of region locks that required waiting",
	    (u_long)sp->st_region_wait, DB_PCT(sp->st_region_wait,
	    sp->st_region_wait + sp->st_region_nowait), NULL);

	qsort(sp->st_txnarray,
	    sp->st_nactive, sizeof(sp->st_txnarray[0]), __txn_compare);
	__db_msg(dbenv, "List of active transactions:");
	DB_MSGBUF_INIT(&mb);
	for (i = 0; i < sp->st_nactive; ++i) {
		__db_msgadd(dbenv,
		    &mb, "\tID: %lx; begin LSN: file/offset %lu/%lu",
		    (u_long)sp->st_txnarray[i].txnid,
		    (u_long)sp->st_txnarray[i].lsn.file,
		    (u_long)sp->st_txnarray[i].lsn.offset);
		if (sp->st_txnarray[i].parentid != 0)
			__db_msgadd(dbenv, &mb, "; parent: %lx",
			    (u_long)sp->st_txnarray[i].parentid);
		if (sp->st_txnarray[i].xa_status != 0)
			__txn_xid_stats(dbenv, &mb, &sp->st_txnarray[i]);
		DB_MSGBUF_FLUSH(dbenv, &mb);
	}

	__os_ufree(dbenv, sp);

	return (0);
}

/*
 * __txn_print_all --
 *	Display debugging transaction region statistics.
 */
static int
__txn_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ TXN_IN_RECOVERY,	"TXN_IN_RECOVERY" },
		{ 0,			NULL }
	};
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	R_LOCK(dbenv, &mgr->reginfo);

	__db_print_reginfo(dbenv, &mgr->reginfo, "Transaction");

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_TXNMGR handle information:");

	__db_print_mutex(dbenv, NULL, mgr->mutexp, "DB_TXNMGR mutex", flags);
	__db_dl(dbenv,
	    "Number of transactions discarded", (u_long)mgr->n_discards);

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_TXNREGION handle information:");
	STAT_ULONG("Maximum number of active txns", region->maxtxns);
	STAT_HEX("Last transaction ID allocated", region->last_txnid);
	STAT_HEX("Current maximum unused ID", region->cur_maxid);

	STAT_LSN("Last checkpoint LSN", &region->last_ckp);
	__db_msg(dbenv,
	    "%.24s\tLast checkpoint timestamp",
	    region->time_ckp == 0 ? "0" : ctime(&region->time_ckp));

	__db_prflags(dbenv, NULL, region->flags, fn, NULL, "\tFlags");

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "XA information:");
	STAT_LONG("XA RMID", dbenv->xa_rmid);
	/*
	 * XXX
	 * Display list of XA transactions in the DB_ENV handle.
	 */

	R_UNLOCK(dbenv, &mgr->reginfo);

	return (0);
}

static void
__txn_xid_stats(dbenv, mbp, txnp)
	DB_ENV *dbenv;
	DB_MSGBUF *mbp;
	DB_TXN_ACTIVE *txnp;
{
	u_int32_t v, *xp;
	u_int i;
	int cnt;
	const char *s;

	switch (txnp->xa_status) {
	case TXN_XA_ABORTED:
		s = "ABORTED";
		break;
	case TXN_XA_DEADLOCKED:
		s = "DEADLOCKED";
		break;
	case TXN_XA_ENDED:
		s = "ENDED";
		break;
	case TXN_XA_PREPARED:
		s = "PREPARED";
		break;
	case TXN_XA_STARTED:
		s = "STARTED";
		break;
	case TXN_XA_SUSPENDED:
		s = "SUSPENDED";
		break;
	default:
		s = "UNKNOWN STATE";
		__db_err(dbenv,
		    "XA: unknown state: %lu", (u_long)txnp->xa_status);
		break;
	}
	__db_msgadd(dbenv, mbp, "\tXA: %s; XID:\n\t\t", s == NULL ? "" : s);
	for (cnt = 0, xp = (u_int32_t *)txnp->xid,
	    i = 0; i < DB_XIDDATASIZE; i += sizeof(u_int32_t)) {
		memcpy(&v, xp++, sizeof(u_int32_t));
		__db_msgadd(dbenv, mbp, "%#x ", v);
		if (++cnt == 4) {
			DB_MSGBUF_FLUSH(dbenv, mbp);
			__db_msgadd(dbenv, mbp, "\t\t");
			cnt = 0;
		}
	}
}

static int
__txn_compare(a1, b1)
	const void *a1, *b1;
{
	const DB_TXN_ACTIVE *a, *b;

	a = a1;
	b = b1;

	if (a->txnid > b->txnid)
		return (1);
	if (a->txnid < b->txnid)
		return (-1);
	return (0);
}

#else /* !HAVE_STATISTICS */

int
__txn_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_TXN_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}

int
__txn_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif
