/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: env_failchk.c,v 12.17 2005/11/07 14:51:52 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mutex_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/hash.h"			/* Needed for call to __ham_func5. */
#include "dbinc/lock.h"
#include "dbinc/txn.h"

static int __env_in_api __P((DB_ENV *));

/*
 * __env_failchk_pp --
 *	DB_ENV->failchk pre/post processing.
 *
 * PUBLIC: int __env_failchk_pp __P((DB_ENV *, u_int32_t));
 */
int
__env_failchk_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->failchk");

	/*
	 * DB_ENV->failchk requires self and is-alive functions.  We
	 * have a default self function, but no is-alive function.
	 */
	if (!ALIVE_ON(dbenv)) {
		__db_err(dbenv,
	"DB_ENV->failchk requires DB_ENV->is_alive be configured");
		return (EINVAL);
	}

	if (flags != 0)
		return (__db_ferr(dbenv, "DB_ENV->failchk", 0));

	ENV_ENTER(dbenv, ip);

	/*
	 * We check for dead threads in the API first as this would be likely
	 * to hang other things we try later, like locks and transactions.
	 */
	if ((ret = __env_in_api(dbenv)) != 0)
		goto err;

	if (LOCKING_ON(dbenv) && (ret = __lock_failchk(dbenv)) != 0)
		goto err;

	if (TXN_ON(dbenv) && (ret = __txn_failchk(dbenv)) != 0)
		goto err;

err:	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __env_thread_init --
 *	Initialize the thread control block table.
 *
 * PUBLIC: int __env_thread_init __P((DB_ENV *, int));
 */
int
__env_thread_init(dbenv, created)
	DB_ENV *dbenv;
	int created;
{
	DB_HASHTAB *htab;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	REGINFO *infop;
	THREAD_INFO *thread;
	int ret;

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	infop = &mtxmgr->reginfo;

	if (mtxregion->thread_off == INVALID_ROFF) {
		if (dbenv->thr_nbucket == 0) {
			dbenv->thr_hashtab = NULL;
			if (ALIVE_ON(dbenv)) {
				__db_err(dbenv,
		"is_alive method specified but no thread region allocated");
				return (EINVAL);
			}
			return (0);
		}
		
		if (!created) {
			__db_err(dbenv,
		"thread table must be allocated at environment create time");
			return (EINVAL);
		}

		if ((ret = __db_shalloc(infop,
		     sizeof(THREAD_INFO), 0, &thread)) != 0) {
			__db_err(dbenv,
			     "cannot allocate a thread status block");
			return (ret);
		}
		memset(thread, 0, sizeof(*thread));
		mtxregion->thread_off = R_OFFSET(infop, thread);
		thread->thr_nbucket = __db_tablesize(dbenv->thr_nbucket);
		if ((ret = __db_shalloc(infop,
		     thread->thr_nbucket * sizeof(DB_HASHTAB), 0, &htab)) != 0)
			return (ret);
		thread->thr_hashoff = R_OFFSET(infop, htab);
		__db_hashinit(htab, thread->thr_nbucket);
		thread->thr_max = dbenv->thr_max;
	} else {
		thread = R_ADDR(infop, mtxregion->thread_off);
		htab = R_ADDR(infop, thread->thr_hashoff);
	}

	dbenv->thr_hashtab = htab;
	dbenv->thr_nbucket = thread->thr_nbucket;
	dbenv->thr_max = thread->thr_max;
	return (0);
}

/*
 * __env_in_api --
 *	Look for threads which died in the api and complain.
 */
static int
__env_in_api(dbenv)
	DB_ENV *dbenv;
{
	DB_HASHTAB *htab;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	DB_THREAD_INFO *ip;
	REGINFO *infop;
	THREAD_INFO *thread;
	u_int32_t i;

	if ((htab = dbenv->thr_hashtab) == NULL)
		return (EINVAL);

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	infop = &mtxmgr->reginfo;
	thread = R_ADDR(infop, mtxregion->thread_off);

	for (i = 0; i < dbenv->thr_nbucket; i++)
		SH_TAILQ_FOREACH(ip, &htab[i], dbth_links, __db_thread_info) {
			if (ip->dbth_state == THREAD_SLOT_NOT_IN_USE ||
			    (ip->dbth_state == THREAD_OUT &&
			    thread->thr_count <  thread->thr_max))
				continue;
			if (dbenv->is_alive(dbenv, ip->dbth_pid, ip->dbth_tid))
				continue;
			if (ip->dbth_state == THREAD_OUT) {
				ip->dbth_state = THREAD_SLOT_NOT_IN_USE;
				continue;
			}
			return (__db_failed(dbenv,
			     "Thread died in Berkeley DB library",
			     ip->dbth_pid, ip->dbth_tid));
		}

	return (0);
}

struct __db_threadid {
	pid_t pid;
	db_threadid_t tid;
};

static int __thread_id_cmp __P((struct __db_threadid *, DB_THREAD_INFO *));
static int __thread_state_cmp __P((DB_THREAD_STATE, DB_THREAD_INFO *));

static
int __thread_id_cmp(id, ip)
	struct __db_threadid *id;
	DB_THREAD_INFO *ip;
{
#ifdef HAVE_INTEGRAL_THREAD_TYPE
	return (id->pid == ip->dbth_pid && id->tid == ip->dbth_tid);
#else
	if (memcmp(&id->pid, &ip->dbth_pid, sizeof(id->pid)) != 0)
		return (0);
	if (memcmp(&id->tid, &ip->dbth_tid, sizeof(id->tid)) != 0)
		return (0);
	return (1);
#endif
}

static
int __thread_state_cmp(state, ip)
	DB_THREAD_STATE state;
	DB_THREAD_INFO *ip;
{
	return (ip->dbth_state == state);
}

/*
 * PUBLIC: int __env_set_state __P((DB_ENV *,
 * PUBLIC:      DB_THREAD_INFO **, DB_THREAD_STATE));
 */
int
__env_set_state(dbenv, ipp, state)
	DB_ENV *dbenv;
	DB_THREAD_INFO **ipp;
	DB_THREAD_STATE state;
{
	DB_HASHTAB *htab;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	DB_THREAD_INFO *ip;
	struct __db_threadid id;
	REGINFO *infop;
	THREAD_INFO *thread;
	int ret;
	u_int32_t indx;

	htab = (DB_HASHTAB *)dbenv->thr_hashtab;

	dbenv->thread_id(dbenv, &id.pid, &id.tid);

	/*
	 * Hashing of thread ids.  This is simple but could be replaced with
	 * something more expensive if needed.
	 */
#ifdef HAVE_INTEGRAL_THREAD_TYPE
	/*
	 * A thread ID may be a pointer, so explicitly cast to a pointer of
	 * the appropriate size before doing the bitwise XOR.
	 */
	indx = (u_int32_t)((uintptr_t)id.pid ^ (uintptr_t)id.tid);
#else
	indx = __ham_func5(NULL, &id.tid, sizeof(id.tid));
#endif
	indx %= dbenv->thr_nbucket;
	HASHLOOKUP(htab, indx,
	     __db_thread_info, dbth_links, &id, ip, __thread_id_cmp);
#ifdef DIAGNOSTIC
	if (state == THREAD_DIAGNOSTIC) {
		*ipp = ip;
		return (0);
	}
#endif

	ret = 0;
	if (ip == NULL) {
		mtxmgr = dbenv->mutex_handle;
		mtxregion = mtxmgr->reginfo.primary;
		infop = &mtxmgr->reginfo;
		thread = R_ADDR(infop, mtxregion->thread_off);
		MUTEX_SYSTEM_LOCK(dbenv);

		/*
		 * If we are passed the specified max, try to reclaim one from
		 * our queue.  If failcheck has marked the slot not in use, we
		 * can take it, otherwise we must call is_alive before freeing
		 * it.
		 */
		if (thread->thr_count >= thread->thr_max) {
			HASHLOOKUP(htab, indx, __db_thread_info,
			    dbth_links, THREAD_OUT, ip, __thread_state_cmp);
			while (ip != NULL &&
			    ip->dbth_state != THREAD_SLOT_NOT_IN_USE &&
			    (ip->dbth_state != THREAD_OUT || !ALIVE_ON(dbenv) ||
			    dbenv->is_alive(dbenv,
			    ip->dbth_pid, ip->dbth_tid))) {
				ip = SH_TAILQ_NEXT(ip,
				    dbth_links, __db_thread_info);
			}
			if (ip != NULL)
				goto init;
		}

		thread->thr_count++;
		if ((ret = __db_shalloc(infop,
		     sizeof(DB_THREAD_INFO), 0, &ip)) == 0) {
			memset(ip, 0, sizeof(*ip));
			/*
			 * This assumes we can link atomically since we do
			 * no locking here.  We never use the backpointer
			 * so we only need to be able to write an offset
			 * atomically.
			 */
			HASHINSERT(htab,
			    indx, __db_thread_info, dbth_links, ip);
init:			ip->dbth_pid = id.pid;
			ip->dbth_tid = id.tid;
			ip->dbth_state = state;
		}
		MUTEX_SYSTEM_UNLOCK(dbenv);
	} else
		ip->dbth_state = state;
	*ipp = ip;

	return (ret);
}

/*
 * __env_thread_id_string --
 *	Convert a thread id to a string.
 *
 * PUBLIC: char *__env_thread_id_string
 * PUBLIC:     __P((DB_ENV *, pid_t, db_threadid_t, char *));
 */
char *
__env_thread_id_string(dbenv, pid, tid, buf)
	DB_ENV *dbenv;
	pid_t pid;
	db_threadid_t tid;
	char *buf;
{
#ifdef HAVE_INTEGRAL_THREAD_TYPE
#ifdef UINT64_FMT
	char fmt[10];

	snprintf(fmt, sizeof(fmt), "%s/%s", UINT64_FMT, UINT64_FMT);
	snprintf(buf,
	    DB_THREADID_STRLEN, fmt, (u_int64_t)pid, (u_int64_t)(uintptr_t)tid);
#else
	snprintf(buf, DB_THREADID_STRLEN, "%lu/%lu", (u_long)pid, (u_long)tid);
#endif
#else
#ifdef UINT64_FMT
	char fmt[10];

	snprintf(fmt, sizeof(fmt), "%s/TID", UINT64_FMT);
	snprintf(buf, DB_THREADID_STRLEN, fmt, (u_int64_t)pid);
#else
	snprintf(buf, DB_THREADID_STRLEN, "%lu/TID", (u_long)pid);
#endif
#endif
	COMPQUIET(dbenv, NULL);
	COMPQUIET(*(u_int8_t *)&tid, 0);

	return (buf);
}
