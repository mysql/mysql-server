/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: lock_stat.c,v 11.4 2000/12/08 20:15:31 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#endif

#ifdef	HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_shash.h"
#include "lock.h"

#ifdef	HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static void __lock_dump_locker __P((DB_LOCKTAB *, DB_LOCKER *, FILE *));
static void __lock_dump_object __P((DB_LOCKTAB *, DB_LOCKOBJ *, FILE *));
static const char *
	    __lock_dump_status __P((db_status_t));

/*
 * lock_stat --
 *	Return LOCK statistics.
 */
int
lock_stat(dbenv, statp, db_malloc)
	DB_ENV *dbenv;
	DB_LOCK_STAT **statp;
	void *(*db_malloc) __P((size_t));
{
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	DB_LOCK_STAT *stats;
	int ret;

#ifdef	HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_lock_stat(dbenv, statp, db_malloc));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lk_handle, DB_INIT_LOCK);

	*statp = NULL;

	lt = dbenv->lk_handle;

	if ((ret = __os_malloc(dbenv, sizeof(*stats), db_malloc, &stats)) != 0)
		return (ret);

	/* Copy out the global statistics. */
	R_LOCK(dbenv, &lt->reginfo);

	region = lt->reginfo.primary;
	stats->st_lastid = region->id;
	stats->st_maxlocks = region->maxlocks;
	stats->st_maxlockers = region->maxlockers;
	stats->st_maxobjects = region->maxobjects;
	stats->st_nmodes = region->nmodes;
	stats->st_nlockers = region->nlockers;
	stats->st_maxnlockers = region->maxnlockers;
	stats->st_nobjects = region->nobjects;
	stats->st_maxnobjects = region->maxnobjects;
	stats->st_nlocks = region->nlocks;
	stats->st_maxnlocks = region->maxnlocks;
	stats->st_nconflicts = region->nconflicts;
	stats->st_nrequests = region->nrequests;
	stats->st_nreleases = region->nreleases;
	stats->st_nnowaits = region->nnowaits;
	stats->st_ndeadlocks = region->ndeadlocks;

	stats->st_region_wait = lt->reginfo.rp->mutex.mutex_set_wait;
	stats->st_region_nowait = lt->reginfo.rp->mutex.mutex_set_nowait;
	stats->st_regsize = lt->reginfo.rp->size;

	R_UNLOCK(dbenv, &lt->reginfo);

	*statp = stats;
	return (0);
}

#define	LOCK_DUMP_CONF		0x001		/* Conflict matrix. */
#define	LOCK_DUMP_FREE		0x002		/* Display lock free list. */
#define	LOCK_DUMP_LOCKERS	0x004		/* Display lockers. */
#define	LOCK_DUMP_MEM		0x008		/* Display region memory. */
#define	LOCK_DUMP_OBJECTS	0x010		/* Display objects. */
#define	LOCK_DUMP_ALL		0x01f		/* Display all. */

/*
 * __lock_dump_region --
 *
 * PUBLIC: void __lock_dump_region __P((DB_ENV *, char *, FILE *));
 */
void
__lock_dump_region(dbenv, area, fp)
	DB_ENV *dbenv;
	char *area;
	FILE *fp;
{
	struct __db_lock *lp;
	DB_LOCKER *lip;
	DB_LOCKOBJ *op;
	DB_LOCKREGION *lrp;
	DB_LOCKTAB *lt;
	u_int32_t flags, i, j;
	int label;

	/* Make it easy to call from the debugger. */
	if (fp == NULL)
		fp = stderr;

	for (flags = 0; *area != '\0'; ++area)
		switch (*area) {
		case 'A':
			LF_SET(LOCK_DUMP_ALL);
			break;
		case 'c':
			LF_SET(LOCK_DUMP_CONF);
			break;
		case 'f':
			LF_SET(LOCK_DUMP_FREE);
			break;
		case 'l':
			LF_SET(LOCK_DUMP_LOCKERS);
			break;
		case 'm':
			LF_SET(LOCK_DUMP_MEM);
			break;
		case 'o':
			LF_SET(LOCK_DUMP_OBJECTS);
			break;
		}

	lt = dbenv->lk_handle;
	lrp = lt->reginfo.primary;
	LOCKREGION(dbenv, lt);

	fprintf(fp, "%s\nLock region parameters\n", DB_LINE);
	fprintf(fp, "%s: %lu, %s: %lu, %s: %lu, %s: %lu, %s: %lu, %s: %lu, %s: %lu\n",
	    "locker table size", (u_long)lrp->locker_t_size,
	    "object table size", (u_long)lrp->object_t_size,
	    "obj_off", (u_long)lrp->obj_off,
	    "osynch_off", (u_long)lrp->osynch_off,
	    "locker_off", (u_long)lrp->locker_off,
	    "lsynch_off", (u_long)lrp->lsynch_off,
	    "need_dd", (u_long)lrp->need_dd);

	if (LF_ISSET(LOCK_DUMP_CONF)) {
		fprintf(fp, "\n%s\nConflict matrix\n", DB_LINE);
		for (i = 0; i < lrp->nmodes; i++) {
			for (j = 0; j < lrp->nmodes; j++)
				fprintf(fp, "%lu\t",
				    (u_long)lt->conflicts[i * lrp->nmodes + j]);
			fprintf(fp, "\n");
		}
	}

	if (LF_ISSET(LOCK_DUMP_LOCKERS)) {
		fprintf(fp, "%s\nLocker hash buckets\n", DB_LINE);
		for (i = 0; i < lrp->locker_t_size; i++) {
			label = 1;
			for (lip =
			    SH_TAILQ_FIRST(&lt->locker_tab[i], __db_locker);
			    lip != NULL;
			    lip = SH_TAILQ_NEXT(lip, links, __db_locker)) {
				if (label) {
					fprintf(fp, "Bucket %lu:\n", (u_long)i);
					label = 0;
				}
				__lock_dump_locker(lt, lip, fp);
			}
		}
	}

	if (LF_ISSET(LOCK_DUMP_OBJECTS)) {
		fprintf(fp, "%s\nObject hash buckets\n", DB_LINE);
		for (i = 0; i < lrp->object_t_size; i++) {
			label = 1;
			for (op = SH_TAILQ_FIRST(&lt->obj_tab[i], __db_lockobj);
			    op != NULL;
			    op = SH_TAILQ_NEXT(op, links, __db_lockobj)) {
				if (label) {
					fprintf(fp, "Bucket %lu:\n", (u_long)i);
					label = 0;
				}
				__lock_dump_object(lt, op, fp);
			}
		}
	}

	if (LF_ISSET(LOCK_DUMP_FREE)) {
		fprintf(fp, "%s\nLock free list\n", DB_LINE);
		for (lp = SH_TAILQ_FIRST(&lrp->free_locks, __db_lock);
		    lp != NULL;
		    lp = SH_TAILQ_NEXT(lp, links, __db_lock))
			fprintf(fp, "0x%lx: %lu\t%lu\t%s\t0x%lx\n", (u_long)lp,
			    (u_long)lp->holder, (u_long)lp->mode,
			    __lock_dump_status(lp->status), (u_long)lp->obj);

		fprintf(fp, "%s\nObject free list\n", DB_LINE);
		for (op = SH_TAILQ_FIRST(&lrp->free_objs, __db_lockobj);
		    op != NULL;
		    op = SH_TAILQ_NEXT(op, links, __db_lockobj))
			fprintf(fp, "0x%lx\n", (u_long)op);

		fprintf(fp, "%s\nLocker free list\n", DB_LINE);
		for (lip = SH_TAILQ_FIRST(&lrp->free_lockers, __db_locker);
		    lip != NULL;
		    lip = SH_TAILQ_NEXT(lip, links, __db_locker))
			fprintf(fp, "0x%lx\n", (u_long)lip);
	}

	if (LF_ISSET(LOCK_DUMP_MEM))
		__db_shalloc_dump(lt->reginfo.addr, fp);

	UNLOCKREGION(dbenv, lt);
}

static void
__lock_dump_locker(lt, lip, fp)
	DB_LOCKTAB *lt;
	DB_LOCKER *lip;
	FILE *fp;
{
	struct __db_lock *lp;

	fprintf(fp, "L %lx [%ld]", (u_long)lip->id, (long)lip->dd_id);
	fprintf(fp, " %s ", F_ISSET(lip, DB_LOCKER_DELETED) ? "(D)" : "   ");

	if ((lp = SH_LIST_FIRST(&lip->heldby, __db_lock)) == NULL)
		fprintf(fp, "\n");
	else
		for (; lp != NULL;
		    lp = SH_LIST_NEXT(lp, locker_links, __db_lock))
			__lock_printlock(lt, lp, 1);
}

static void
__lock_dump_object(lt, op, fp)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *op;
	FILE *fp;
{
	struct __db_lock *lp;
	u_int32_t j;
	u_int8_t *ptr;
	u_int ch;

	ptr = SH_DBT_PTR(&op->lockobj);
	for (j = 0; j < op->lockobj.size; ptr++, j++) {
		ch = *ptr;
		fprintf(fp, isprint(ch) ? "%c" : "\\%o", ch);
	}
	fprintf(fp, "\n");

	fprintf(fp, "H:");
	for (lp =
	    SH_TAILQ_FIRST(&op->holders, __db_lock);
	    lp != NULL;
	    lp = SH_TAILQ_NEXT(lp, links, __db_lock))
		__lock_printlock(lt, lp, 1);
	lp = SH_TAILQ_FIRST(&op->waiters, __db_lock);
	if (lp != NULL) {
		fprintf(fp, "\nW:");
		for (; lp != NULL; lp = SH_TAILQ_NEXT(lp, links, __db_lock))
			__lock_printlock(lt, lp, 1);
	}
}

static const char *
__lock_dump_status(status)
	db_status_t status;
{
	switch (status) {
	case DB_LSTAT_ABORTED:
		return ("aborted");
	case DB_LSTAT_ERR:
		return ("err");
	case DB_LSTAT_FREE:
		return ("free");
	case DB_LSTAT_HELD:
		return ("held");
	case DB_LSTAT_NOGRANT:
		return ("nogrant");
	case DB_LSTAT_PENDING:
		return ("pending");
	case DB_LSTAT_WAITING:
		return ("waiting");
	}
	return ("unknown status");
}
