/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: env_stat.c,v 1.21 2004/10/29 17:37:23 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

#ifdef HAVE_STATISTICS
static int  __dbenv_print_all __P((DB_ENV *, u_int32_t));
static int  __dbenv_print_stats __P((DB_ENV *, u_int32_t));
static int  __dbenv_stat_print __P((DB_ENV *, u_int32_t));
static const char *__reg_type __P((reg_type_t));

/*
 * __dbenv_stat_print_pp --
 *	DB_ENV->stat_print pre/post processor.
 *
 * PUBLIC: int __dbenv_stat_print_pp __P((DB_ENV *, u_int32_t));
 */
int
__dbenv_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->stat_print");

	if ((ret = __db_fchk(dbenv, "DB_ENV->stat_print",
	    flags, DB_STAT_ALL | DB_STAT_CLEAR | DB_STAT_SUBSYSTEM)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __dbenv_stat_print(dbenv, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __dbenv_stat_print --
 *	DB_ENV->stat_print method.
 */
static int
__dbenv_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB *dbp;
	int ret;

	if ((ret = __dbenv_print_stats(dbenv, flags)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL) &&
	    (ret = __dbenv_print_all(dbenv, flags)) != 0)
		return (ret);

	if (!LF_ISSET(DB_STAT_SUBSYSTEM))
		return (0);

	/* The subsystems don't know anything about DB_STAT_SUBSYSTEM. */
	LF_CLR(DB_STAT_SUBSYSTEM);

	if (LOGGING_ON(dbenv)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		if ((ret = __log_stat_print(dbenv, flags)) != 0)
			return (ret);
	}

	if (LOCKING_ON(dbenv)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		if ((ret = __lock_stat_print(dbenv, flags)) != 0)
			return (ret);
	}

	if (MPOOL_ON(dbenv)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		if ((ret = __memp_stat_print(dbenv, flags)) != 0)
			return (ret);
	}

	if (REP_ON(dbenv)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		if ((ret = __rep_stat_print(dbenv, flags)) != 0)
			return (ret);
	}

	if (TXN_ON(dbenv)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		if ((ret = __txn_stat_print(dbenv, flags)) != 0)
			return (ret);
	}

	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (dbp = LIST_FIRST(&dbenv->dblist);
	    dbp != NULL; dbp = LIST_NEXT(dbp, dblistlinks)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "%s%s%s\tDatabase name",
		    dbp->fname, dbp->dname == NULL ? "" : "/",
		    dbp->dname == NULL ? "" : dbp->dname);
		if ((ret = __db_stat_print(dbp, flags)) != 0)
			break;
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	return (ret);
}

/*
 * __dbenv_print_stats --
 *	Display the default environment statistics.
 *
 */
static int
__dbenv_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	REGENV *renv;
	REGINFO *infop;

	infop = dbenv->reginfo;
	renv = infop->primary;

	if (LF_ISSET(DB_STAT_ALL)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Default database environment information:");
	}
	__db_msg(dbenv, "%d.%d.%d\tEnvironment version",
	    renv->majver, renv->minver, renv->patch);
	STAT_HEX("Magic number", renv->magic);
	STAT_LONG("Panic value", renv->envpanic);
	STAT_LONG("References", renv->refcnt);

	__db_print_mutex(dbenv, NULL, &renv->mutex,
	    "The number of region locks that required waiting", flags);

	return (0);
}

/*
 * __dbenv_print_all --
 *	Display the debugging environment statistics.
 */
static int
__dbenv_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ DB_ENV_AUTO_COMMIT,		"DB_ENV_AUTO_COMMIT" },
		{ DB_ENV_CDB,			"DB_ENV_CDB" },
		{ DB_ENV_CDB_ALLDB,		"DB_ENV_CDB_ALLDB" },
		{ DB_ENV_CREATE,		"DB_ENV_CREATE" },
		{ DB_ENV_DBLOCAL,		"DB_ENV_DBLOCAL" },
		{ DB_ENV_DIRECT_DB,		"DB_ENV_DIRECT_DB" },
		{ DB_ENV_DIRECT_LOG,		"DB_ENV_DIRECT_LOG" },
		{ DB_ENV_DSYNC_LOG,		"DB_ENV_DSYNC_LOG" },
		{ DB_ENV_FATAL,			"DB_ENV_FATAL" },
		{ DB_ENV_LOCKDOWN,		"DB_ENV_LOCKDOWN" },
		{ DB_ENV_LOG_AUTOREMOVE,	"DB_ENV_LOG_AUTOREMOVE" },
		{ DB_ENV_LOG_INMEMORY,		"DB_ENV_LOG_INMEMORY" },
		{ DB_ENV_NOLOCKING,		"DB_ENV_NOLOCKING" },
		{ DB_ENV_NOMMAP,		"DB_ENV_NOMMAP" },
		{ DB_ENV_NOPANIC,		"DB_ENV_NOPANIC" },
		{ DB_ENV_OPEN_CALLED,		"DB_ENV_OPEN_CALLED" },
		{ DB_ENV_OVERWRITE,		"DB_ENV_OVERWRITE" },
		{ DB_ENV_PRIVATE,		"DB_ENV_PRIVATE" },
		{ DB_ENV_REGION_INIT,		"DB_ENV_REGION_INIT" },
		{ DB_ENV_RPCCLIENT,		"DB_ENV_RPCCLIENT" },
		{ DB_ENV_RPCCLIENT_GIVEN,	"DB_ENV_RPCCLIENT_GIVEN" },
		{ DB_ENV_SYSTEM_MEM,		"DB_ENV_SYSTEM_MEM" },
		{ DB_ENV_THREAD,		"DB_ENV_THREAD" },
		{ DB_ENV_TIME_NOTGRANTED,	"DB_ENV_TIME_NOTGRANTED" },
		{ DB_ENV_TXN_NOSYNC,		"DB_ENV_TXN_NOSYNC" },
		{ DB_ENV_TXN_WRITE_NOSYNC,	"DB_ENV_TXN_WRITE_NOSYNC" },
		{ DB_ENV_YIELDCPU,		"DB_ENV_YIELDCPU" },
		{ 0,				NULL }
	};
	static const FN ofn[] = {
		{ DB_CREATE,			"DB_CREATE" },
		{ DB_CXX_NO_EXCEPTIONS,	"DB_CXX_NO_EXCEPTIONS" },
		{ DB_FORCE,			"DB_FORCE" },
		{ DB_INIT_CDB,			"DB_INIT_CDB" },
		{ DB_INIT_LOCK,		"DB_INIT_LOCK" },
		{ DB_INIT_LOG,			"DB_INIT_LOG" },
		{ DB_INIT_MPOOL,		"DB_INIT_MPOOL" },
		{ DB_INIT_REP,			"DB_INIT_REP" },
		{ DB_INIT_TXN,			"DB_INIT_TXN" },
		{ DB_JOINENV,			"DB_JOINENV" },
		{ DB_LOCKDOWN,			"DB_LOCKDOWN" },
		{ DB_NOMMAP,			"DB_NOMMAP" },
		{ DB_PRIVATE,			"DB_PRIVATE" },
		{ DB_RDONLY,			"DB_RDONLY" },
		{ DB_RECOVER,			"DB_RECOVER" },
		{ DB_RECOVER_FATAL,		"DB_RECOVER_FATAL" },
		{ DB_SYSTEM_MEM,		"DB_SYSTEM_MEM" },
		{ DB_THREAD,			"DB_THREAD" },
		{ DB_TRUNCATE,			"DB_TRUNCATE" },
		{ DB_TXN_NOSYNC,		"DB_TXN_NOSYNC" },
		{ DB_USE_ENVIRON,		"DB_USE_ENVIRON" },
		{ DB_USE_ENVIRON_ROOT,		"DB_USE_ENVIRON_ROOT" },
		{ 0,				NULL }
	};
	static const FN vfn[] = {
		{ DB_VERB_DEADLOCK,		"DB_VERB_DEADLOCK" },
		{ DB_VERB_RECOVERY,		"DB_VERB_RECOVERY" },
		{ DB_VERB_REPLICATION,		"DB_VERB_REPLICATION" },
		{ DB_VERB_WAITSFOR,		"DB_VERB_WAITSFOR" },
		{ 0,				NULL }
	};
	DB_MSGBUF mb;
	REGENV *renv;
	REGINFO *infop;
	REGION *rp, regs[1024];
	size_t n;
	char **p;

	infop = dbenv->reginfo;
	renv = infop->primary;
	DB_MSGBUF_INIT(&mb);

	/*
	 * Lock the database environment while we get copies of the region
	 * information.
	 */
	MUTEX_LOCK(dbenv, &infop->rp->mutex);

	for (n = 0, rp = SH_LIST_FIRST(&renv->regionq, __db_region);
	    n < sizeof(regs) / sizeof(regs[0]) && rp != NULL;
	    ++n, rp = SH_LIST_NEXT(rp, q, __db_region)) {
		regs[n] = *rp;
		if (LF_ISSET(DB_STAT_CLEAR))
			MUTEX_CLEAR(&rp->mutex);
	}
	if (n > 0)
		--n;
	MUTEX_UNLOCK(dbenv, &infop->rp->mutex);

	if (LF_ISSET(DB_STAT_ALL)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Per region database environment information:");
	}
	while (n > 0) {
		rp = &regs[--n];
		__db_msg(dbenv, "%s Region:", __reg_type(rp->type));
		STAT_LONG("Region ID", rp->id);
		STAT_LONG("Segment ID", rp->segid);
		__db_dlbytes(dbenv,
		    "Size", (u_long)0, (u_long)0, (u_long)rp->size);
		__db_print_mutex(dbenv, NULL, &rp->mutex,
		    "The number of region locks that required waiting", flags);
	}

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_ENV handle information:");
	STAT_ISSET("Errfile", dbenv->db_errfile);
	STAT_STRING("Errpfx", dbenv->db_errpfx);
	STAT_ISSET("Errcall", dbenv->db_errcall);
	STAT_ISSET("Feedback", dbenv->db_feedback);
	STAT_ISSET("Panic", dbenv->db_paniccall);
	STAT_ISSET("Malloc", dbenv->db_malloc);
	STAT_ISSET("Realloc", dbenv->db_realloc);
	STAT_ISSET("Free", dbenv->db_free);
	__db_prflags(dbenv, NULL, dbenv->verbose, vfn, NULL, "\tVerbose flags");

	STAT_ISSET("App private", dbenv->app_private);
	STAT_ISSET("App dispatch", dbenv->app_dispatch);
	STAT_STRING("Home", dbenv->db_home);
	STAT_STRING("Log dir", dbenv->db_log_dir);
	STAT_STRING("Tmp dir", dbenv->db_tmp_dir);
	if (dbenv->db_data_dir == NULL)
		STAT_ISSET("Data dir", dbenv->db_data_dir);
	else {
		for (p = dbenv->db_data_dir; *p != NULL; ++p)
			__db_msgadd(dbenv, &mb, "%s\tData dir", *p);
		DB_MSGBUF_FLUSH(dbenv, &mb);
	}
	STAT_FMT("Mode", "%#o", int, dbenv->db_mode);
	__db_prflags(dbenv, NULL, dbenv->open_flags, ofn, NULL, "\tOpen flags");
	STAT_ISSET("Lockfhp", dbenv->lockfhp);
	STAT_ISSET("Rec tab", dbenv->recover_dtab);
	STAT_ULONG("Rec tab slots", dbenv->recover_dtab_size);
	STAT_ISSET("RPC client", dbenv->cl_handle);
	STAT_LONG("RPC client ID", dbenv->cl_id);
	STAT_LONG("DB ref count", dbenv->db_ref);
	STAT_LONG("Shared mem key", dbenv->shm_key);
	STAT_ULONG("test-and-set spin configuration", dbenv->tas_spins);
	__db_print_mutex(
	    dbenv, NULL, dbenv->dblist_mutexp, "DB handle mutex", flags);

	STAT_ISSET("api1 internal", dbenv->api1_internal);
	STAT_ISSET("api2 internal", dbenv->api2_internal);
	STAT_ISSET("password", dbenv->passwd);
	STAT_ISSET("crypto handle", dbenv->crypto_handle);
	__db_print_mutex(dbenv, NULL, dbenv->mt_mutexp, "MT mutex", flags);

	__db_prflags(dbenv, NULL, dbenv->flags, fn, NULL, "\tFlags");

	return (0);
}

/*
 * __db_print_fh --
 *	Print out a file handle.
 *
 * PUBLIC: void __db_print_fh __P((DB_ENV *, DB_FH *, u_int32_t));
 */
void
__db_print_fh(dbenv, fh, flags)
	DB_ENV *dbenv;
	DB_FH *fh;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ DB_FH_NOSYNC,	"DB_FH_NOSYNC" },
		{ DB_FH_OPENED,	"DB_FH_OPENED" },
		{ DB_FH_UNLINK,	"DB_FH_UNLINK" },
		{ 0,		NULL }
	};

	__db_print_mutex(dbenv, NULL, fh->mutexp, "file-handle.mutex", flags);

	STAT_LONG("file-handle.reference count", fh->ref);
	STAT_LONG("file-handle.file descriptor", fh->fd);
	STAT_STRING("file-handle.file name", fh->name);

	STAT_ULONG("file-handle.page number", fh->pgno);
	STAT_ULONG("file-handle.page size", fh->pgsize);
	STAT_ULONG("file-handle.page offset", fh->offset);

	__db_prflags(dbenv, NULL, fh->flags, fn, NULL, "\tfile-handle.flags");
}

/*
 * __db_print_fileid --
 *	Print out a file ID.
 *
 * PUBLIC: void __db_print_fileid __P((DB_ENV *, u_int8_t *, const char *));
 */
void
__db_print_fileid(dbenv, id, suffix)
	DB_ENV *dbenv;
	u_int8_t *id;
	const char *suffix;
{
	DB_MSGBUF mb;
	int i;

	DB_MSGBUF_INIT(&mb);
	for (i = 0; i < DB_FILE_ID_LEN; ++i, ++id) {
		__db_msgadd(dbenv, &mb, "%x", (u_int)*id);
		if (i < DB_FILE_ID_LEN - 1)
			__db_msgadd(dbenv, &mb, " ");
	}
	if (suffix != NULL)
		__db_msgadd(dbenv, &mb, "%s", suffix);
	DB_MSGBUF_FLUSH(dbenv, &mb);
}

/*
 * __db_print_mutex --
 *	Print out mutex statistics.
 *
 * PUBLIC: void __db_print_mutex
 * PUBLIC:    __P((DB_ENV *, DB_MSGBUF *, DB_MUTEX *, const char *, u_int32_t));
 */
void
__db_print_mutex(dbenv, mbp, mutex, suffix, flags)
	DB_ENV *dbenv;
	DB_MSGBUF *mbp;
	DB_MUTEX *mutex;
	const char *suffix;
	u_int32_t flags;
{
	DB_MSGBUF mb;
	u_long value;
	int standalone;

	/* If we don't have a mutex, point that out and return. */
	if (mutex == NULL) {
		STAT_ISSET(suffix, mutex);
		return;
	}

	if (mbp == NULL) {
		DB_MSGBUF_INIT(&mb);
		mbp = &mb;
		standalone = 1;
	} else
		standalone = 0;

	/*
	 * !!!
	 * We may not hold the mutex lock -- that's OK, we're only reading
	 * the statistics.
	 */
	if ((value = mutex->mutex_set_wait) < 10000000)
		__db_msgadd(dbenv, mbp, "%lu", value);
	else
		__db_msgadd(dbenv, mbp, "%luM", value / 1000000);

	/*
	 * If standalone, append the mutex percent and the locker information
	 * after the suffix line.  Otherwise, append it after the counter.
	 *
	 * The setting of "suffix" tracks "standalone" -- if standalone, expect
	 * a suffix and prefix it with a <tab>, otherwise, it's optional.  This
	 * isn't a design, it's just the semantics we happen to need right now.
	 */
	if (standalone) {
		if (suffix == NULL)			/* Defense. */
			suffix = "";

		__db_msgadd(dbenv, &mb, "\t%s (%d%%", suffix,
		    DB_PCT(mutex->mutex_set_wait,
		    mutex->mutex_set_wait + mutex->mutex_set_nowait));
#ifdef DIAGNOSTIC
#ifdef HAVE_MUTEX_THREADS
		if (mutex->locked != 0)
			__db_msgadd(dbenv, &mb, "/%lu", (u_long)mutex->locked);
#else
		if (mutex->pid != 0)
			__db_msgadd(dbenv, &mb, "/%lu", (u_long)mutex->pid);
#endif
#endif
		__db_msgadd(dbenv, &mb, ")");

		DB_MSGBUF_FLUSH(dbenv, mbp);
	} else {
		__db_msgadd(dbenv, mbp, "/%d%%", DB_PCT(mutex->mutex_set_wait,
		    mutex->mutex_set_wait + mutex->mutex_set_nowait));
#ifdef DIAGNOSTIC
#ifdef HAVE_MUTEX_THREADS
		if (mutex->locked != 0)
			__db_msgadd(dbenv, &mb, "/%lu", (u_long)mutex->locked);
#else
		if (mutex->pid != 0)
			__db_msgadd(dbenv, &mb, "/%lu", (u_long)mutex->pid);
#endif
#endif
		if (suffix != NULL)
			__db_msgadd(dbenv, mbp, "%s", suffix);
	}

	if (LF_ISSET(DB_STAT_CLEAR))
		MUTEX_CLEAR(mutex);
}

/*
 * __db_dl --
 *	Display a big value.
 *
 * PUBLIC: void __db_dl __P((DB_ENV *, const char *, u_long));
 */
void
__db_dl(dbenv, msg, value)
	DB_ENV *dbenv;
	const char *msg;
	u_long value;
{
	/*
	 * Two formats: if less than 10 million, display as the number, if
	 * greater than 10 million display as ###M.
	 */
	if (value < 10000000)
		__db_msg(dbenv, "%lu\t%s", value, msg);
	else
		__db_msg(dbenv, "%luM\t%s (%lu)", value / 1000000, msg, value);
}

/*
 * __db_dl_pct --
 *	Display a big value, and related percentage.
 *
 * PUBLIC: void __db_dl_pct
 * PUBLIC:          __P((DB_ENV *, const char *, u_long, int, const char *));
 */
void
__db_dl_pct(dbenv, msg, value, pct, tag)
	DB_ENV *dbenv;
	const char *msg, *tag;
	u_long value;
	int pct;
{
	DB_MSGBUF mb;

	DB_MSGBUF_INIT(&mb);

	/*
	 * Two formats: if less than 10 million, display as the number, if
	 * greater than 10 million display as ###M.
	 */
	if (value < 10000000)
		__db_msgadd(dbenv, &mb, "%lu\t%s", value, msg);
	else
		__db_msgadd(dbenv, &mb, "%luM\t%s", value / 1000000, msg);
	if (tag == NULL)
		__db_msgadd(dbenv, &mb, " (%d%%)", pct);
	else
		__db_msgadd(dbenv, &mb, " (%d%% %s)", pct, tag);

	DB_MSGBUF_FLUSH(dbenv, &mb);
}

/*
 * __db_dlbytes --
 *	Display a big number of bytes.
 *
 * PUBLIC: void __db_dlbytes
 * PUBLIC:     __P((DB_ENV *, const char *, u_long, u_long, u_long));
 */
void
__db_dlbytes(dbenv, msg, gbytes, mbytes, bytes)
	DB_ENV *dbenv;
	const char *msg;
	u_long gbytes, mbytes, bytes;
{
	DB_MSGBUF mb;
	const char *sep;

	DB_MSGBUF_INIT(&mb);

	/* Normalize the values. */
	while (bytes >= MEGABYTE) {
		++mbytes;
		bytes -= MEGABYTE;
	}
	while (mbytes >= GIGABYTE / MEGABYTE) {
		++gbytes;
		mbytes -= GIGABYTE / MEGABYTE;
	}

	if (gbytes == 0 && mbytes == 0 && bytes == 0)
		__db_msgadd(dbenv, &mb, "0");
	else {
		sep = "";
		if (gbytes > 0) {
			__db_msgadd(dbenv, &mb, "%luGB", gbytes);
			sep = " ";
		}
		if (mbytes > 0) {
			__db_msgadd(dbenv, &mb, "%s%luMB", sep, mbytes);
			sep = " ";
		}
		if (bytes >= 1024) {
			__db_msgadd(dbenv, &mb, "%s%luKB", sep, bytes / 1024);
			bytes %= 1024;
			sep = " ";
		}
		if (bytes > 0)
			__db_msgadd(dbenv, &mb, "%s%luB", sep, bytes);
	}

	__db_msgadd(dbenv, &mb, "\t%s", msg);

	DB_MSGBUF_FLUSH(dbenv, &mb);
}

/*
 * __db_print_reginfo --
 *	Print out underlying shared region information.
 *
 * PUBLIC: void __db_print_reginfo __P((DB_ENV *, REGINFO *, const char *));
 */
void
__db_print_reginfo(dbenv, infop, s)
	DB_ENV *dbenv;
	REGINFO *infop;
	const char *s;
{
	static const FN fn[] = {
		{ REGION_CREATE,	"REGION_CREATE" },
		{ REGION_CREATE_OK,	"REGION_CREATE_OK" },
		{ REGION_JOIN_OK,	"REGION_JOIN_OK" },
		{ 0,			NULL }
	};

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "%s REGINFO information:",  s);
	STAT_STRING("Region type", __reg_type(infop->type));
	STAT_ULONG("Region ID", infop->id);
	STAT_STRING("Region name", infop->name);
	STAT_HEX("Original region address", infop->addr_orig);
	STAT_HEX("Region address", infop->addr);
	STAT_HEX("Region primary address", infop->primary);
	STAT_ULONG("Region maximum allocation", infop->max_alloc);
	STAT_ULONG("Region allocated", infop->max_alloc);

	__db_prflags(dbenv, NULL, infop->flags, fn, NULL, "\tRegion flags");
}

/*
 * __reg_type --
 *	Return the region type string.
 */
static const char *
__reg_type(t)
	reg_type_t t;
{
	switch (t) {
	case REGION_TYPE_ENV:
		return ("Environment");
	case REGION_TYPE_LOCK:
		return ("Lock");
	case REGION_TYPE_LOG:
		return ("Log");
	case REGION_TYPE_MPOOL:
		return ("Mpool");
	case REGION_TYPE_MUTEX:
		return ("Mutex");
	case REGION_TYPE_TXN:
		return ("Transaction");
	case INVALID_REGION_TYPE:
		return ("Invalid");
	}
	return ("Unknown");
}

#else /* !HAVE_STATISTICS */

/*
 * __db_stat_not_built --
 *	Common error routine when library not built with statistics.
 *
 * PUBLIC: int __db_stat_not_built __P((DB_ENV *));
 */
int
__db_stat_not_built(dbenv)
	DB_ENV *dbenv;
{
	__db_err(dbenv, "Library build did not include statistics support");
	return (DB_OPNOTSUP);
}

int
__dbenv_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif
