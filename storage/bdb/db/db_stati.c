/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_stati.c,v 11.123 2004/07/19 16:40:51 bostic Exp $
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

#include <string.h>
#endif

#include "db_int.h"

#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/qam.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"

#ifdef HAVE_STATISTICS
static int __db_print_all __P((DB *, u_int32_t));
static int __db_print_citem __P((DBC *));
static int __db_print_cursor __P((DB *));
static int __db_print_stats __P((DB *, u_int32_t));
static int __db_stat_arg __P((DB *, u_int32_t));

/*
 * __db_stat_pp --
 *	DB->stat pre/post processing.
 *
 * PUBLIC: int __db_stat_pp __P((DB *, DB_TXN *, void *, u_int32_t));
 */
int
__db_stat_pp(dbp, txn, spp, flags)
	DB *dbp;
	DB_TXN *txn;
	void *spp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbp->dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->stat");

	if ((ret = __db_stat_arg(dbp, flags)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, 0)) != 0)
		return (ret);

	ret = __db_stat(dbp, txn, spp, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_stat --
 *	DB->stat.
 *
 * PUBLIC: int __db_stat __P((DB *, DB_TXN *, void *, u_int32_t));
 */
int
__db_stat(dbp, txn, spp, flags)
	DB *dbp;
	DB_TXN *txn;
	void *spp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBC *dbc;
	int ret, t_ret;

	dbenv = dbp->dbenv;

	/* Acquire a cursor. */
	if ((ret = __db_cursor(dbp, txn,
	     &dbc, LF_ISSET(DB_DEGREE_2 | DB_DIRTY_READ))) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, NULL, "DB->stat", NULL, NULL, flags);
	LF_CLR(DB_DEGREE_2 | DB_DIRTY_READ);

	switch (dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		ret = __bam_stat(dbc, spp, flags);
		break;
	case DB_HASH:
		ret = __ham_stat(dbc, spp, flags);
		break;
	case DB_QUEUE:
		ret = __qam_stat(dbc, spp, flags);
		break;
	case DB_UNKNOWN:
	default:
		ret = (__db_unknown_type(dbenv, "DB->stat", dbp->type));
		break;
	}

	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_stat_arg --
 *	Check DB->stat arguments.
 */
static int
__db_stat_arg(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;

	/* Check for invalid function flags. */
	LF_CLR(DB_DEGREE_2 | DB_DIRTY_READ);
	switch (flags) {
	case 0:
	case DB_FAST_STAT:
	case DB_CACHED_COUNTS:		/* Deprecated and undocumented. */
		break;
	case DB_RECORDCOUNT:		/* Deprecated and undocumented. */
		if (dbp->type == DB_RECNO)
			break;
		if (dbp->type == DB_BTREE && F_ISSET(dbp, DB_AM_RECNUM))
			break;
		/* FALLTHROUGH */
	default:
		return (__db_ferr(dbenv, "DB->stat", 0));
	}

	return (0);
}

/*
 * __db_stat_print_pp --
 *	DB->stat_print pre/post processing.
 *
 * PUBLIC: int __db_stat_print_pp __P((DB *, u_int32_t));
 */
int
__db_stat_print_pp(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->stat");

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline.
	 */
	if ((ret = __db_fchk(dbenv, "DB->stat_print",
	    flags, DB_STAT_ALL | DB_STAT_CLEAR)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, 0)) != 0)
		return (ret);

	ret = __db_stat_print(dbp, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_stat_print --
 *	DB->stat_print.
 *
 * PUBLIC: int __db_stat_print __P((DB *, u_int32_t));
 */
int
__db_stat_print(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	int ret;

	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		ret = __db_print_stats(dbp, flags);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL) && (ret = __db_print_all(dbp, flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __db_print_stats --
 *	Display default DB handle statistics.
 */
static int
__db_print_stats(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DBC *dbc;
	DB_ENV *dbenv;
	int ret, t_ret;

	dbenv = dbp->dbenv;

	/* Acquire a cursor. */
	if ((ret = __db_cursor(dbp, NULL, &dbc, 0)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, NULL, "DB->stat_print", NULL, NULL, 0);

	switch (dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		ret = __bam_stat_print(dbc, flags);
		break;
	case DB_HASH:
		ret = __ham_stat_print(dbc, flags);
		break;
	case DB_QUEUE:
		ret = __qam_stat_print(dbc, flags);
		break;
	case DB_UNKNOWN:
	default:
		ret = (__db_unknown_type(dbenv, "DB->stat_print", dbp->type));
		break;
	}

	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_print_all --
 *	Display debugging DB handle statistics.
 */
static int
__db_print_all(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ DB_AM_CHKSUM,		"DB_AM_CHKSUM" },
		{ DB_AM_CL_WRITER,	"DB_AM_CL_WRITER" },
		{ DB_AM_COMPENSATE,	"DB_AM_COMPENSATE" },
		{ DB_AM_CREATED,	"DB_AM_CREATED" },
		{ DB_AM_CREATED_MSTR,	"DB_AM_CREATED_MSTR" },
		{ DB_AM_DBM_ERROR,	"DB_AM_DBM_ERROR" },
		{ DB_AM_DELIMITER,	"DB_AM_DELIMITER" },
		{ DB_AM_DIRTY,		"DB_AM_DIRTY" },
		{ DB_AM_DISCARD,	"DB_AM_DISCARD" },
		{ DB_AM_DUP,		"DB_AM_DUP" },
		{ DB_AM_DUPSORT,	"DB_AM_DUPSORT" },
		{ DB_AM_ENCRYPT,	"DB_AM_ENCRYPT" },
		{ DB_AM_FIXEDLEN,	"DB_AM_FIXEDLEN" },
		{ DB_AM_INMEM,		"DB_AM_INMEM" },
		{ DB_AM_IN_RENAME,	"DB_AM_IN_RENAME" },
		{ DB_AM_NOT_DURABLE,	"DB_AM_NOT_DURABLE" },
		{ DB_AM_OPEN_CALLED,	"DB_AM_OPEN_CALLED" },
		{ DB_AM_PAD,		"DB_AM_PAD" },
		{ DB_AM_PGDEF,		"DB_AM_PGDEF" },
		{ DB_AM_RDONLY,		"DB_AM_RDONLY" },
		{ DB_AM_RECNUM,		"DB_AM_RECNUM" },
		{ DB_AM_RECOVER,	"DB_AM_RECOVER" },
		{ DB_AM_RENUMBER,	"DB_AM_RENUMBER" },
		{ DB_AM_REPLICATION,	"DB_AM_REPLICATION" },
		{ DB_AM_REVSPLITOFF,	"DB_AM_REVSPLITOFF" },
		{ DB_AM_SECONDARY,	"DB_AM_SECONDARY" },
		{ DB_AM_SNAPSHOT,	"DB_AM_SNAPSHOT" },
		{ DB_AM_SUBDB,		"DB_AM_SUBDB" },
		{ DB_AM_SWAP,		"DB_AM_SWAP" },
		{ DB_AM_TXN,		"DB_AM_TXN" },
		{ DB_AM_VERIFYING,	"DB_AM_VERIFYING" },
		{ 0,			NULL }
	};
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB handle information:");
	STAT_ULONG("Page size", dbp->pgsize);
	STAT_ISSET("Append recno", dbp->db_append_recno);
	STAT_ISSET("Feedback", dbp->db_feedback);
	STAT_ISSET("Dup compare", dbp->dup_compare);
	STAT_ISSET("App private", dbp->app_private);
	STAT_ISSET("DbEnv", dbp->dbenv);
	STAT_STRING("Type", __db_dbtype_to_string(dbp->type));

	__db_print_mutex(dbenv, NULL, dbp->mutexp, "Thread mutex", flags);

	STAT_STRING("File", dbp->fname);
	STAT_STRING("Database", dbp->dname);
	STAT_HEX("Open flags", dbp->open_flags);

	__db_print_fileid(dbenv, dbp->fileid, "\tFile ID");

	STAT_ULONG("Cursor adjust ID", dbp->adj_fileid);
	STAT_ULONG("Meta pgno", dbp->meta_pgno);
	STAT_ULONG("Locker ID", dbp->lid);
	STAT_ULONG("Handle lock", dbp->cur_lid);
	STAT_ULONG("Associate lock", dbp->associate_lid);
	STAT_ULONG("RPC remote ID", dbp->cl_id);

	__db_msg(dbenv,
	    "%.24s\tReplication handle timestamp",
	    dbp->timestamp == 0 ? "0" : ctime(&dbp->timestamp));

	STAT_ISSET("Secondary callback", dbp->s_callback);
	STAT_ISSET("Primary handle", dbp->s_primary);

	STAT_ISSET("api internal", dbp->api_internal);
	STAT_ISSET("Btree/Recno internal", dbp->bt_internal);
	STAT_ISSET("Hash internal", dbp->h_internal);
	STAT_ISSET("Queue internal", dbp->q_internal);
	STAT_ISSET("XA internal", dbp->xa_internal);

	__db_prflags(dbenv, NULL, dbp->flags, fn, NULL, "\tFlags");

	if (dbp->log_filename == NULL)
		STAT_ISSET("File naming information", dbp->log_filename);
	else
		__dbreg_print_fname(dbenv, dbp->log_filename);

	(void)__db_print_cursor(dbp);

	return (0);
}

/*
 * __db_print_cursor --
 *	Display the cursor active and free queues.
 */
static int
__db_print_cursor(dbp)
	DB *dbp;
{
	DB_ENV *dbenv;
	DBC *dbc;
	int ret, t_ret;

	dbenv = dbp->dbenv;

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB handle cursors:");

	ret = 0;
	MUTEX_THREAD_LOCK(dbp->dbenv, dbp->mutexp);
	__db_msg(dbenv, "Active queue:");
	for (dbc = TAILQ_FIRST(&dbp->active_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links))
		if ((t_ret = __db_print_citem(dbc)) != 0 && ret == 0)
			ret = t_ret;
	__db_msg(dbenv, "Join queue:");
	for (dbc = TAILQ_FIRST(&dbp->join_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links))
		if ((t_ret = __db_print_citem(dbc)) != 0 && ret == 0)
			ret = t_ret;
	__db_msg(dbenv, "Free queue:");
	for (dbc = TAILQ_FIRST(&dbp->free_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links))
		if ((t_ret = __db_print_citem(dbc)) != 0 && ret == 0)
			ret = t_ret;
	MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);

	return (ret);
}

static
int __db_print_citem(dbc)
	DBC *dbc;
{
	static const FN fn[] = {
		{ DBC_ACTIVE,		"DBC_ACTIVE" },
		{ DBC_COMPENSATE,	"DBC_COMPENSATE" },
		{ DBC_DEGREE_2,		"DBC_DEGREE_2" },
		{ DBC_DIRTY_READ,	"DBC_DIRTY_READ" },
		{ DBC_OPD,		"DBC_OPD" },
		{ DBC_RECOVER,		"DBC_RECOVER" },
		{ DBC_RMW,		"DBC_RMW" },
		{ DBC_TRANSIENT,	"DBC_TRANSIENT" },
		{ DBC_WRITECURSOR,	"DBC_WRITECURSOR" },
		{ DBC_WRITER,		"DBC_WRITER" },
		{ DBC_MULTIPLE,		"DBC_MULTIPLE" },
		{ DBC_MULTIPLE_KEY,	"DBC_MULTIPLE_KEY" },
		{ DBC_OWN_LID,		"DBC_OWN_LID" },
		{ 0,			NULL }
	};
	DB *dbp;
	DBC_INTERNAL *cp;
	DB_ENV *dbenv;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;
	cp = dbc->internal;

	STAT_HEX("DBC", dbc);
	STAT_HEX("Associated dbp", dbc->dbp);
	STAT_HEX("Associated txn", dbc->txn);
	STAT_HEX("Internal", cp);
	STAT_HEX("Default locker ID", dbc->lid);
	STAT_HEX("Locker", dbc->locker);
	STAT_STRING("Type", __db_dbtype_to_string(dbc->dbtype));

	STAT_HEX("Off-page duplicate cursor", cp->opd);
	STAT_HEX("Referenced page", cp->page);
	STAT_ULONG("Root", cp->root);
	STAT_ULONG("Page number", cp->pgno);
	STAT_ULONG("Page index", cp->indx);
	STAT_STRING("Lock mode", __db_lockmode_to_string(cp->lock_mode));
	__db_prflags(dbenv, NULL, dbc->flags, fn, NULL, "\tFlags");

	switch (dbc->dbtype) {
	case DB_BTREE:
	case DB_RECNO:
		__bam_print_cursor(dbc);
		break;
	case DB_HASH:
		__ham_print_cursor(dbc);
		break;
	case DB_UNKNOWN:
		DB_ASSERT(dbp->type != DB_UNKNOWN);
		/* FALLTHROUGH */
	case DB_QUEUE:
	default:
		break;
	}
	return (0);
}

#else /* !HAVE_STATISTICS */

int
__db_stat_pp(dbp, txn, spp, flags)
	DB *dbp;
	DB_TXN *txn;
	void *spp;
	u_int32_t flags;
{
	COMPQUIET(spp, NULL);
	COMPQUIET(txn, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbp->dbenv));
}

int
__db_stat_print_pp(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbp->dbenv));
}
#endif
