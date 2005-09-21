/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: log_stat.c,v 11.149 2004/10/15 16:59:42 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"

#ifdef HAVE_STATISTICS
static int __log_print_all __P((DB_ENV *, u_int32_t));
static int __log_print_stats __P((DB_ENV *, u_int32_t));
static int __log_stat __P((DB_ENV *, DB_LOG_STAT **, u_int32_t));

/*
 * __log_stat_pp --
 *	DB_ENV->log_stat pre/post processing.
 *
 * PUBLIC: int __log_stat_pp __P((DB_ENV *, DB_LOG_STAT **, u_int32_t));
 */
int
__log_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOG_STAT **statp;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lg_handle, "DB_ENV->log_stat", DB_INIT_LOG);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->log_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __log_stat(dbenv, statp, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __log_stat --
 *	DB_ENV->log_stat.
 */
static int
__log_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOG_STAT **statp;
	u_int32_t flags;
{
	DB_LOG *dblp;
	DB_LOG_STAT *stats;
	LOG *lp;
	int ret;

	*statp = NULL;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	if ((ret = __os_umalloc(dbenv, sizeof(DB_LOG_STAT), &stats)) != 0)
		return (ret);

	/* Copy out the global statistics. */
	R_LOCK(dbenv, &dblp->reginfo);
	*stats = lp->stat;
	if (LF_ISSET(DB_STAT_CLEAR))
		memset(&lp->stat, 0, sizeof(lp->stat));

	stats->st_magic = lp->persist.magic;
	stats->st_version = lp->persist.version;
	stats->st_mode = (int)lp->persist.mode;
	stats->st_lg_bsize = lp->buffer_size;
	stats->st_lg_size = lp->log_nsize;

	stats->st_region_wait = dblp->reginfo.rp->mutex.mutex_set_wait;
	stats->st_region_nowait = dblp->reginfo.rp->mutex.mutex_set_nowait;
	if (LF_ISSET(DB_STAT_CLEAR))
		MUTEX_CLEAR(&dblp->reginfo.rp->mutex);
	stats->st_regsize = dblp->reginfo.rp->size;

	stats->st_cur_file = lp->lsn.file;
	stats->st_cur_offset = lp->lsn.offset;
	stats->st_disk_file = lp->s_lsn.file;
	stats->st_disk_offset = lp->s_lsn.offset;

	R_UNLOCK(dbenv, &dblp->reginfo);

	*statp = stats;
	return (0);
}

/*
 * __log_stat_print_pp --
 *	DB_ENV->log_stat_print pre/post processing.
 *
 * PUBLIC: int __log_stat_print_pp __P((DB_ENV *, u_int32_t));
 */
int
__log_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lg_handle, "DB_ENV->log_stat_print", DB_INIT_LOG);

	if ((ret = __db_fchk(dbenv, "DB_ENV->log_stat_print",
	    flags, DB_STAT_ALL | DB_STAT_CLEAR)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __log_stat_print(dbenv, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __log_stat_print --
 *	DB_ENV->log_stat_print method.
 *
 * PUBLIC: int __log_stat_print __P((DB_ENV *, u_int32_t));
 */
int
__log_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	u_int32_t orig_flags;
	int ret;

	orig_flags = flags;
	LF_CLR(DB_STAT_CLEAR);
	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		ret = __log_print_stats(dbenv, orig_flags);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL) &&
	    (ret = __log_print_all(dbenv, orig_flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __log_print_stats --
 *	Display default log region statistics.
 */
static int
__log_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_LOG_STAT *sp;
	int ret;

	if ((ret = __log_stat(dbenv, &sp, flags)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL))
		__db_msg(dbenv, "Default logging region information:");
	STAT_HEX("Log magic number", sp->st_magic);
	STAT_ULONG("Log version number", sp->st_version);
	__db_dlbytes(dbenv, "Log record cache size",
	    (u_long)0, (u_long)0, (u_long)sp->st_lg_bsize);
	__db_msg(dbenv, "%#o\tLog file mode", sp->st_mode);
	if (sp->st_lg_size % MEGABYTE == 0)
		__db_msg(dbenv, "%luMb\tCurrent log file size",
		    (u_long)sp->st_lg_size / MEGABYTE);
	else if (sp->st_lg_size % 1024 == 0)
		__db_msg(dbenv, "%luKb\tCurrent log file size",
		    (u_long)sp->st_lg_size / 1024);
	else
		__db_msg(dbenv, "%lu\tCurrent log file size",
		    (u_long)sp->st_lg_size);
	__db_dlbytes(dbenv, "Log bytes written",
	    (u_long)0, (u_long)sp->st_w_mbytes, (u_long)sp->st_w_bytes);
	__db_dlbytes(dbenv, "Log bytes written since last checkpoint",
	    (u_long)0, (u_long)sp->st_wc_mbytes, (u_long)sp->st_wc_bytes);
	__db_dl(dbenv, "Total log file writes", (u_long)sp->st_wcount);
	__db_dl(dbenv, "Total log file write due to overflow",
	    (u_long)sp->st_wcount_fill);
	__db_dl(dbenv, "Total log file flushes", (u_long)sp->st_scount);
	STAT_ULONG("Current log file number", sp->st_cur_file);
	STAT_ULONG("Current log file offset", sp->st_cur_offset);
	STAT_ULONG("On-disk log file number", sp->st_disk_file);
	STAT_ULONG("On-disk log file offset", sp->st_disk_offset);

	__db_dl(dbenv,
	    "Maximum commits in a log flush", (u_long)sp->st_maxcommitperflush);
	__db_dl(dbenv,
	    "Minimum commits in a log flush", (u_long)sp->st_mincommitperflush);

	__db_dlbytes(dbenv, "Log region size",
	    (u_long)0, (u_long)0, (u_long)sp->st_regsize);
	__db_dl_pct(dbenv,
	    "The number of region locks that required waiting",
	    (u_long)sp->st_region_wait, DB_PCT(sp->st_region_wait,
	    sp->st_region_wait + sp->st_region_nowait), NULL);

	__os_ufree(dbenv, sp);

	return (0);
}

/*
 * __log_print_all --
 *	Display debugging log region statistics.
 */
static int
__log_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ DBLOG_RECOVER,	"DBLOG_RECOVER" },
		{ DBLOG_FORCE_OPEN,	"DBLOG_FORCE_OPEN" },
		{ 0,			NULL }
	};
	DB_LOG *dblp;
	DB_MUTEX *flush_mutexp;
	LOG *lp;

	dblp = dbenv->lg_handle;
	lp = (LOG *)dblp->reginfo.primary;

	R_LOCK(dbenv, &dblp->reginfo);

	__db_print_reginfo(dbenv, &dblp->reginfo, "Log");

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_LOG handle information:");

	__db_print_mutex(
	    dbenv, NULL, dblp->mutexp, "DB_LOG handle mutex", flags);
	STAT_ULONG("Log file name", dblp->lfname);
	if (dblp->lfhp == NULL)
		STAT_ISSET("Log file handle", dblp->lfhp);
	else
		__db_print_fh(dbenv, dblp->lfhp, flags);
	__db_prflags(dbenv, NULL, dblp->flags, fn, NULL, "\tFlags");

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "LOG handle information:");

	__db_print_mutex(
	    dbenv, NULL, &lp->fq_mutex, "file name list mutex", flags);

	STAT_HEX("persist.magic", lp->persist.magic);
	STAT_ULONG("persist.version", lp->persist.version);
	__db_dlbytes(dbenv,
	    "persist.log_size", (u_long)0, (u_long)0, lp->persist.log_size);
	STAT_FMT("persist.mode", "%#lo", u_long, lp->persist.mode);
	STAT_LSN("current file offset LSN", &lp->lsn);
	STAT_LSN("first buffer byte LSN", &lp->lsn);
	STAT_ULONG("current buffer offset", lp->b_off);
	STAT_ULONG("current file write offset", lp->w_off);
	STAT_ULONG("length of last record", lp->len);
	STAT_LONG("log flush in progress", lp->in_flush);

	flush_mutexp = R_ADDR(&dblp->reginfo, lp->flush_mutex_off);
	__db_print_mutex(dbenv, NULL, flush_mutexp, "Log flush mutex", flags);

	STAT_LSN("last sync LSN", &lp->s_lsn);

	/*
	 * Don't display the replication fields here, they're displayed as part
	 * of the replication statistics.
	 */

	STAT_LSN("cached checkpoint LSN", &lp->cached_ckp_lsn);

	__db_dlbytes(dbenv,
	    "log buffer size", (u_long)0, (u_long)0, lp->buffer_size);
	__db_dlbytes(dbenv,
	    "log file size", (u_long)0, (u_long)0, lp->log_size);
	__db_dlbytes(dbenv,
	    "next log file size", (u_long)0, (u_long)0, lp->log_nsize);

	STAT_ULONG("transactions waiting to commit", lp->ncommit);
	STAT_LSN("LSN of first commit", &lp->t_lsn);

	__dbreg_print_dblist(dbenv, flags);
	R_UNLOCK(dbenv, &dblp->reginfo);

	return (0);
}

#else /* !HAVE_STATISTICS */

int
__log_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOG_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}

int
__log_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif
