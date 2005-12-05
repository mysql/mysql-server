/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_stat.c,v 12.10 2005/11/01 00:44:28 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/mutex_int.h"

#ifdef HAVE_STATISTICS
static int __mutex_print_all __P((DB_ENV *, u_int32_t));
static const char *__mutex_print_id __P((int));
static int __mutex_print_stats __P((DB_ENV *, u_int32_t));
static void __mutex_print_summary __P((DB_ENV *));

/*
 * __mutex_stat --
 *	DB_ENV->mutex_stat.
 *
 * PUBLIC: int __mutex_stat __P((DB_ENV *, DB_MUTEX_STAT **, u_int32_t));
 */
int
__mutex_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_MUTEX_STAT **statp;
	u_int32_t flags;
{
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	DB_MUTEX_STAT *stats;
	int ret;

	PANIC_CHECK(dbenv);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->mutex_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	*statp = NULL;
	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;

	if ((ret = __os_umalloc(dbenv, sizeof(DB_MUTEX_STAT), &stats)) != 0)
		return (ret);

	MUTEX_SYSTEM_LOCK(dbenv);

	/*
	 * Most fields are maintained in the underlying region structure.
	 * Region size and region mutex are not.
	 */
	*stats = mtxregion->stat;
	stats->st_regsize = mtxmgr->reginfo.rp->size;
	__mutex_set_wait_info(dbenv, mtxregion->mtx_region,
	    &stats->st_region_wait, &stats->st_region_nowait);
	if (LF_ISSET(DB_STAT_CLEAR))
		__mutex_clear(dbenv, mtxregion->mtx_region);

	MUTEX_SYSTEM_UNLOCK(dbenv);

	*statp = stats;
	return (0);
}

/*
 * __mutex_stat_print
 *	DB_ENV->mutex_stat_print method.
 *
 * PUBLIC: int __mutex_stat_print __P((DB_ENV *, u_int32_t));
 */
int
__mutex_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	u_int32_t orig_flags;
	int ret;

	PANIC_CHECK(dbenv);

	if ((ret = __db_fchk(dbenv, "DB_ENV->mutex_stat_print",
	    flags, DB_STAT_ALL | DB_STAT_CLEAR)) != 0)
		return (ret);

	orig_flags = flags;
	LF_CLR(DB_STAT_CLEAR);
	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		ret = __mutex_print_stats(dbenv, orig_flags);
		__mutex_print_summary(dbenv);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL))
		ret = __mutex_print_all(dbenv, orig_flags);

	return (0);
}

static void
__mutex_print_summary(dbenv)
	DB_ENV *dbenv;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	db_mutex_t i;
	u_int32_t counts[MTX_MAX_ENTRY + 2];
	int alloc_id;

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	memset(counts, 0, sizeof(counts));

	for (i = 1; i <= mtxregion->stat.st_mutex_cnt; ++i, ++mutexp) {
		mutexp = MUTEXP_SET(i);

		if (!F_ISSET(mutexp, DB_MUTEX_ALLOCATED))
			counts[0]++;
		else if (mutexp->alloc_id > MTX_MAX_ENTRY)
			counts[MTX_MAX_ENTRY + 1]++;
		else
			counts[mutexp->alloc_id]++;
	}
	__db_msg(dbenv, "Mutex counts");
	__db_msg(dbenv, "%d\tUnallocated", counts[0]);
	for (alloc_id = 1; alloc_id <= MTX_TXN_REGION + 1; alloc_id++)
		if (counts[alloc_id] != 0)
			__db_msg(dbenv, "%lu\t%s",
			    (u_long)counts[alloc_id],
			    __mutex_print_id(alloc_id));

}

/*
 * __mutex_print_stats --
 *	Display default mutex region statistics.
 */
static int
__mutex_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_MUTEX_STAT *sp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	REGINFO *infop;
	THREAD_INFO *thread;
	int ret;

	if ((ret = __mutex_stat(dbenv, &sp, LF_ISSET(DB_STAT_CLEAR))) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL))
		__db_msg(dbenv, "Default mutex region information:");

	__db_dlbytes(dbenv, "Mutex region size",
	    (u_long)0, (u_long)0, (u_long)sp->st_regsize);
	__db_dl_pct(dbenv,
	    "The number of region locks that required waiting",
	    (u_long)sp->st_region_wait, DB_PCT(sp->st_region_wait,
	    sp->st_region_wait + sp->st_region_nowait), NULL);
	STAT_ULONG("Mutex alignment", sp->st_mutex_align);
	STAT_ULONG("Mutex test-and-set spins", sp->st_mutex_tas_spins);
	STAT_ULONG("Mutex total count", sp->st_mutex_cnt);
	STAT_ULONG("Mutex free count", sp->st_mutex_free);
	STAT_ULONG("Mutex in-use count", sp->st_mutex_inuse);
	STAT_ULONG("Mutex maximum in-use count", sp->st_mutex_inuse_max);

	__os_ufree(dbenv, sp);

	/*
	 * Dump out the info we have on thread tracking, we do it here only
	 * because we share the region.
	 */
	if (dbenv->thr_hashtab != NULL) {
		mtxmgr = dbenv->mutex_handle;
		mtxregion = mtxmgr->reginfo.primary;
		infop = &mtxmgr->reginfo;
		thread = R_ADDR(infop, mtxregion->thread_off);
		STAT_ULONG("Thread blocks allocated", thread->thr_count);
		STAT_ULONG("Thread allocation threshold", thread->thr_max);
		STAT_ULONG("Thread hash buckets", thread->thr_nbucket);
	}

	return (0);
}

/*
 * __mutex_print_all --
 *	Display debugging mutex region statistics.
 */
static int
__mutex_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ DB_MUTEX_ALLOCATED,		"alloc" },
		{ DB_MUTEX_LOGICAL_LOCK,	"logical" },
		{ DB_MUTEX_SELF_BLOCK,		"self-block" },
		{ DB_MUTEX_THREAD,		"thread" },
		{ 0,			NULL }
	};
	DB_MSGBUF mb, *mbp;
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	db_mutex_t i;

	DB_MSGBUF_INIT(&mb);
	mbp = &mb;

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;

	__db_print_reginfo(dbenv, &mtxmgr->reginfo, "Mutex");
	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));

	__db_msg(dbenv, "DB_MUTEXREGION structure:");
	__mutex_print_debug_single(dbenv,
	    "DB_MUTEXREGION region mutex", mtxregion->mtx_region, flags);
	STAT_ULONG("Size of the aligned mutex", mtxregion->mutex_size);
	STAT_ULONG("Next free mutex", mtxregion->mutex_next);

	/*
	 * The OOB mutex (MUTEX_INVALID) is 0, skip it.
	 *
	 * We're not holding the mutex region lock, so we're racing threads of
	 * control allocating mutexes.  That's OK, it just means we display or
	 * clear statistics while mutexes are moving.
	 */
	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "mutex\twait/nowait, pct wait, holder, flags");
	for (i = 1; i <= mtxregion->stat.st_mutex_cnt; ++i, ++mutexp) {
		mutexp = MUTEXP_SET(i);

		if (!F_ISSET(mutexp, DB_MUTEX_ALLOCATED))
			continue;

		__db_msgadd(dbenv, mbp, "%5lu\t", (u_long)i);

		__mutex_print_debug_stats(dbenv, mbp, i, flags);

		if (mutexp->alloc_id != 0)
			__db_msgadd(dbenv,
			    mbp, ", %s", __mutex_print_id(mutexp->alloc_id));

		__db_prflags(dbenv, mbp, mutexp->flags, fn, " (", ")");

		DB_MSGBUF_FLUSH(dbenv, mbp);
	}

	return (0);
}

/*
 * __mutex_print_debug_single --
 *	Print mutex internal debugging statistics for a single mutex on a
 *	single output line.
 *
 * PUBLIC: void __mutex_print_debug_single
 * PUBLIC:          __P((DB_ENV *, const char *, db_mutex_t, u_int32_t));
 */
void
__mutex_print_debug_single(dbenv, tag, mutex, flags)
	DB_ENV *dbenv;
	const char *tag;
	db_mutex_t mutex;
	u_int32_t flags;
{
	DB_MSGBUF mb, *mbp;

	DB_MSGBUF_INIT(&mb);
	mbp = &mb;

	__db_msgadd(dbenv, mbp, "%lu\t%s ", (u_long)mutex, tag);
	__mutex_print_debug_stats(dbenv, mbp, mutex, flags);
	DB_MSGBUF_FLUSH(dbenv, mbp);
}

/*
 * __mutex_print_debug_stats --
 *	Print mutex internal debugging statistics, that is, the statistics
 *	in the [] square brackets.
 *
 * PUBLIC: void __mutex_print_debug_stats
 * PUBLIC:          __P((DB_ENV *, DB_MSGBUF *, db_mutex_t, u_int32_t));
 */
void
__mutex_print_debug_stats(dbenv, mbp, mutex, flags)
	DB_ENV *dbenv;
	DB_MSGBUF *mbp;
	db_mutex_t mutex;
	u_int32_t flags;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	u_long value;
	char buf[DB_THREADID_STRLEN];

	if (mutex == MUTEX_INVALID) {
		__db_msgadd(dbenv, mbp, "[!Set]");
		return;
	}

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

	__db_msgadd(dbenv, mbp, "[");
	if ((value = mutexp->mutex_set_wait) < 10000000)
		__db_msgadd(dbenv, mbp, "%lu", value);
	else
		__db_msgadd(dbenv, mbp, "%luM", value / 1000000);
	if ((value = mutexp->mutex_set_nowait) < 10000000)
		__db_msgadd(dbenv, mbp, "/%lu", value);
	else
		__db_msgadd(dbenv, mbp, "/%luM", value / 1000000);

	__db_msgadd(dbenv, mbp, " %d%%",
	    DB_PCT(mutexp->mutex_set_wait,
	    mutexp->mutex_set_wait + mutexp->mutex_set_nowait));

	if (F_ISSET(mutexp, DB_MUTEX_LOCKED))
		__db_msgadd(dbenv, mbp, " %s]",
		    dbenv->thread_id_string(dbenv,
		    mutexp->pid, mutexp->tid, buf));
	else
		__db_msgadd(dbenv, mbp, " !Own]");

	if (LF_ISSET(DB_STAT_CLEAR))
		__mutex_clear(dbenv, mutex);
}

static const char *
__mutex_print_id(alloc_id)
	int alloc_id;
{
	switch (alloc_id) {
	case MTX_APPLICATION:		return ("application allocated");
	case MTX_DB_HANDLE:		return ("db handle");
	case MTX_ENV_DBLIST:		return ("env dblist");
	case MTX_ENV_REGION:		return ("env region");
	case MTX_LOCK_REGION:		return ("lock region");
	case MTX_LOGICAL_LOCK:		return ("logical lock");
	case MTX_LOG_FILENAME:		return ("log filename");
	case MTX_LOG_FLUSH:		return ("log flush");
	case MTX_LOG_HANDLE:		return ("log handle");
	case MTX_LOG_REGION:		return ("log region");
	case MTX_MPOOLFILE_HANDLE:	return ("mpoolfile handle");
	case MTX_MPOOL_BUFFER:		return ("mpool buffer");
	case MTX_MPOOL_FH:		return ("mpool filehandle");
	case MTX_MPOOL_HANDLE:		return ("mpool handle");
	case MTX_MPOOL_HASH_BUCKET:	return ("mpool hash bucket");
	case MTX_MPOOL_REGION:		return ("mpool region");
	case MTX_REP_DATABASE:		return ("replication database");
	case MTX_REP_REGION:		return ("replication region");
	case MTX_SEQUENCE:		return ("sequence");
	case MTX_TWISTER:		return ("twister");
	case MTX_TXN_ACTIVE:		return ("txn active list");
	case MTX_TXN_COMMIT:		return ("txn commit");
	case MTX_TXN_REGION:		return ("txn region");
	default:			return ("unknown mutex type");
	}
	/* NOTREACHED */
}

/*
 * __mutex_set_wait_info --
 *	Return mutex statistics.
 *
 * PUBLIC: void __mutex_set_wait_info
 * PUBLIC:	__P((DB_ENV *, db_mutex_t, u_int32_t *, u_int32_t *));
 */
void
__mutex_set_wait_info(dbenv, mutex, waitp, nowaitp)
	DB_ENV *dbenv;
	db_mutex_t mutex;
	u_int32_t *waitp, *nowaitp;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

	*waitp = mutexp->mutex_set_wait;
	*nowaitp = mutexp->mutex_set_nowait;
}

/*
 * __mutex_clear --
 *	Clear mutex statistics.
 *
 * PUBLIC: void __mutex_clear __P((DB_ENV *, db_mutex_t));
 */
void
__mutex_clear(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

	mutexp->mutex_set_wait = mutexp->mutex_set_nowait = 0;
}

#else /* !HAVE_STATISTICS */

int
__mutex_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_MUTEX_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}

int
__mutex_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif
