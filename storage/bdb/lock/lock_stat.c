/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: lock_stat.c,v 11.32 2002/08/14 20:08:51 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <string.h>
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

#include <ctype.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_page.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/db_am.h"

static void __lock_dump_locker __P((DB_LOCKTAB *, DB_LOCKER *, FILE *));
static void __lock_dump_object __P((DB_LOCKTAB *, DB_LOCKOBJ *, FILE *));
static void __lock_printheader __P((void));

/*
 * __lock_stat --
 *	Return LOCK statistics.
 *
 * PUBLIC: int __lock_stat __P((DB_ENV *, DB_LOCK_STAT **, u_int32_t));
 */
int
__lock_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOCK_STAT **statp;
	u_int32_t flags;
{
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	DB_LOCK_STAT *stats, tmp;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_ENV->lock_stat", DB_INIT_LOCK);

	*statp = NULL;
	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->lock_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	lt = dbenv->lk_handle;

	if ((ret = __os_umalloc(dbenv, sizeof(*stats), &stats)) != 0)
		return (ret);

	/* Copy out the global statistics. */
	R_LOCK(dbenv, &lt->reginfo);

	region = lt->reginfo.primary;
	memcpy(stats, &region->stat, sizeof(*stats));
	stats->st_locktimeout = region->lk_timeout;
	stats->st_txntimeout = region->tx_timeout;

	stats->st_region_wait = lt->reginfo.rp->mutex.mutex_set_wait;
	stats->st_region_nowait = lt->reginfo.rp->mutex.mutex_set_nowait;
	stats->st_regsize = lt->reginfo.rp->size;
	if (LF_ISSET(DB_STAT_CLEAR)) {
		tmp = region->stat;
		memset(&region->stat, 0, sizeof(region->stat));
		lt->reginfo.rp->mutex.mutex_set_wait = 0;
		lt->reginfo.rp->mutex.mutex_set_nowait = 0;

		region->stat.st_id = tmp.st_id;
		region->stat.st_cur_maxid = tmp.st_cur_maxid;
		region->stat.st_maxlocks = tmp.st_maxlocks;
		region->stat.st_maxlockers = tmp.st_maxlockers;
		region->stat.st_maxobjects = tmp.st_maxobjects;
		region->stat.st_nlocks =
		    region->stat.st_maxnlocks = tmp.st_nlocks;
		region->stat.st_nlockers =
		    region->stat.st_maxnlockers = tmp.st_nlockers;
		region->stat.st_nobjects =
		    region->stat.st_maxnobjects = tmp.st_nobjects;
		region->stat.st_nmodes = tmp.st_nmodes;
	}

	R_UNLOCK(dbenv, &lt->reginfo);

	*statp = stats;
	return (0);
}

#define	LOCK_DUMP_CONF		0x001		/* Conflict matrix. */
#define	LOCK_DUMP_LOCKERS	0x002		/* Display lockers. */
#define	LOCK_DUMP_MEM		0x004		/* Display region memory. */
#define	LOCK_DUMP_OBJECTS	0x008		/* Display objects. */
#define	LOCK_DUMP_PARAMS	0x010		/* Display params. */
#define	LOCK_DUMP_ALL				/* All */		\
	(LOCK_DUMP_CONF | LOCK_DUMP_LOCKERS | LOCK_DUMP_MEM |		\
	LOCK_DUMP_OBJECTS | LOCK_DUMP_PARAMS)

/*
 * __lock_dump_region --
 *
 * PUBLIC: int __lock_dump_region __P((DB_ENV *, char *, FILE *));
 */
int
__lock_dump_region(dbenv, area, fp)
	DB_ENV *dbenv;
	char *area;
	FILE *fp;
{
	DB_LOCKER *lip;
	DB_LOCKOBJ *op;
	DB_LOCKREGION *lrp;
	DB_LOCKTAB *lt;
	u_int32_t flags, i, j;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "lock_dump_region", DB_INIT_LOCK);

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
		case 'l':
			LF_SET(LOCK_DUMP_LOCKERS);
			break;
		case 'm':
			LF_SET(LOCK_DUMP_MEM);
			break;
		case 'o':
			LF_SET(LOCK_DUMP_OBJECTS);
			break;
		case 'p':
			LF_SET(LOCK_DUMP_PARAMS);
			break;
		}

	lt = dbenv->lk_handle;
	lrp = lt->reginfo.primary;
	LOCKREGION(dbenv, lt);

	if (LF_ISSET(LOCK_DUMP_PARAMS)) {
		fprintf(fp, "%s\nLock region parameters\n", DB_LINE);
		fprintf(fp,
	    "%s: %lu, %s: %lu, %s: %lu,\n%s: %lu, %s: %lu, %s: %lu, %s: %lu\n",
		    "locker table size", (u_long)lrp->locker_t_size,
		    "object table size", (u_long)lrp->object_t_size,
		    "obj_off", (u_long)lrp->obj_off,
		    "osynch_off", (u_long)lrp->osynch_off,
		    "locker_off", (u_long)lrp->locker_off,
		    "lsynch_off", (u_long)lrp->lsynch_off,
		    "need_dd", (u_long)lrp->need_dd);
	}

	if (LF_ISSET(LOCK_DUMP_CONF)) {
		fprintf(fp, "\n%s\nConflict matrix\n", DB_LINE);
		for (i = 0; i < lrp->stat.st_nmodes; i++) {
			for (j = 0; j < lrp->stat.st_nmodes; j++)
				fprintf(fp, "%lu\t", (u_long)
				    lt->conflicts[i * lrp->stat.st_nmodes + j]);
			fprintf(fp, "\n");
		}
	}

	if (LF_ISSET(LOCK_DUMP_LOCKERS)) {
		fprintf(fp, "%s\nLocks grouped by lockers\n", DB_LINE);
		__lock_printheader();
		for (i = 0; i < lrp->locker_t_size; i++)
			for (lip =
			    SH_TAILQ_FIRST(&lt->locker_tab[i], __db_locker);
			    lip != NULL;
			    lip = SH_TAILQ_NEXT(lip, links, __db_locker)) {
				__lock_dump_locker(lt, lip, fp);
			}
	}

	if (LF_ISSET(LOCK_DUMP_OBJECTS)) {
		fprintf(fp, "%s\nLocks grouped by object\n", DB_LINE);
		__lock_printheader();
		for (i = 0; i < lrp->object_t_size; i++) {
			for (op = SH_TAILQ_FIRST(&lt->obj_tab[i], __db_lockobj);
			    op != NULL;
			    op = SH_TAILQ_NEXT(op, links, __db_lockobj))
				__lock_dump_object(lt, op, fp);
		}
	}

	if (LF_ISSET(LOCK_DUMP_MEM))
		__db_shalloc_dump(lt->reginfo.addr, fp);

	UNLOCKREGION(dbenv, lt);

	return (0);
}

static void
__lock_dump_locker(lt, lip, fp)
	DB_LOCKTAB *lt;
	DB_LOCKER *lip;
	FILE *fp;
{
	struct __db_lock *lp;
	time_t s;
	char buf[64];

	fprintf(fp, "%8lx dd=%2ld locks held %-4d write locks %-4d",
	    (u_long)lip->id, (long)lip->dd_id, lip->nlocks, lip->nwrites);
	fprintf(fp, " %s ", F_ISSET(lip, DB_LOCKER_DELETED) ? "(D)" : "   ");
	if (LOCK_TIME_ISVALID(&lip->tx_expire)) {
		s = lip->tx_expire.tv_sec;
		strftime(buf, sizeof(buf), "%m-%d-%H:%M:%S", localtime(&s));
		fprintf(fp,
		    " expires %s.%lu", buf, (u_long)lip->tx_expire.tv_usec);
	}
	if (F_ISSET(lip, DB_LOCKER_TIMEOUT))
		fprintf(fp, " lk timeout %u", lip->lk_timeout);
	if (LOCK_TIME_ISVALID(&lip->lk_expire)) {
		s = lip->lk_expire.tv_sec;
		strftime(buf, sizeof(buf), "%m-%d-%H:%M:%S", localtime(&s));
		fprintf(fp,
		    " lk expires %s.%lu", buf, (u_long)lip->lk_expire.tv_usec);
	}
	fprintf(fp, "\n");

	lp = SH_LIST_FIRST(&lip->heldby, __db_lock);
	if (lp != NULL) {
		for (; lp != NULL;
		    lp = SH_LIST_NEXT(lp, locker_links, __db_lock))
			__lock_printlock(lt, lp, 1);
		fprintf(fp, "\n");
	}
}

static void
__lock_dump_object(lt, op, fp)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *op;
	FILE *fp;
{
	struct __db_lock *lp;

	for (lp =
	    SH_TAILQ_FIRST(&op->holders, __db_lock);
	    lp != NULL;
	    lp = SH_TAILQ_NEXT(lp, links, __db_lock))
		__lock_printlock(lt, lp, 1);
	for (lp =
	    SH_TAILQ_FIRST(&op->waiters, __db_lock);
	    lp != NULL;
	    lp = SH_TAILQ_NEXT(lp, links, __db_lock))
		__lock_printlock(lt, lp, 1);

	fprintf(fp, "\n");
}

/*
 * __lock_printheader --
 */
static void
__lock_printheader()
{
	printf("%-8s  %-6s  %-6s  %-10s  %s\n",
	    "Locker", "Mode",
	    "Count", "Status", "----------- Object ----------");
}

/*
 * __lock_printlock --
 *
 * PUBLIC: void __lock_printlock __P((DB_LOCKTAB *, struct __db_lock *, int));
 */
void
__lock_printlock(lt, lp, ispgno)
	DB_LOCKTAB *lt;
	struct __db_lock *lp;
	int ispgno;
{
	DB_LOCKOBJ *lockobj;
	db_pgno_t pgno;
	u_int32_t *fidp, type;
	u_int8_t *ptr;
	char *namep;
	const char *mode, *status;

	switch (lp->mode) {
	case DB_LOCK_DIRTY:
		mode = "DIRTY_READ";
		break;
	case DB_LOCK_IREAD:
		mode = "IREAD";
		break;
	case DB_LOCK_IWR:
		mode = "IWR";
		break;
	case DB_LOCK_IWRITE:
		mode = "IWRITE";
		break;
	case DB_LOCK_NG:
		mode = "NG";
		break;
	case DB_LOCK_READ:
		mode = "READ";
		break;
	case DB_LOCK_WRITE:
		mode = "WRITE";
		break;
	case DB_LOCK_WWRITE:
		mode = "WAS_WRITE";
		break;
	case DB_LOCK_WAIT:
		mode = "WAIT";
		break;
	default:
		mode = "UNKNOWN";
		break;
	}
	switch (lp->status) {
	case DB_LSTAT_ABORTED:
		status = "ABORT";
		break;
	case DB_LSTAT_ERR:
		status = "ERROR";
		break;
	case DB_LSTAT_FREE:
		status = "FREE";
		break;
	case DB_LSTAT_HELD:
		status = "HELD";
		break;
	case DB_LSTAT_WAITING:
		status = "WAIT";
		break;
	case DB_LSTAT_PENDING:
		status = "PENDING";
		break;
	case DB_LSTAT_EXPIRED:
		status = "EXPIRED";
		break;
	default:
		status = "UNKNOWN";
		break;
	}
	printf("%8lx  %-6s  %6lu  %-10s  ",
	    (u_long)lp->holder, mode, (u_long)lp->refcount, status);

	lockobj = (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);
	ptr = SH_DBT_PTR(&lockobj->lockobj);
	if (ispgno && lockobj->lockobj.size == sizeof(struct __db_ilock)) {
		/* Assume this is a DBT lock. */
		memcpy(&pgno, ptr, sizeof(db_pgno_t));
		fidp = (u_int32_t *)(ptr + sizeof(db_pgno_t));
		type = *(u_int32_t *)(ptr + sizeof(db_pgno_t) + DB_FILE_ID_LEN);
		if (__dbreg_get_name(lt->dbenv, (u_int8_t *)fidp, &namep) != 0)
			namep = NULL;
		if (namep == NULL)
			printf("(%lx %lx %lx %lx %lx)",
			(u_long)fidp[0], (u_long)fidp[1], (u_long)fidp[2],
			(u_long)fidp[3], (u_long)fidp[4]);
		else
			printf("%-20s", namep);
		printf("%-7s  %lu\n",
			type == DB_PAGE_LOCK ? "page" :
			type == DB_RECORD_LOCK ? "record" : "handle",
			(u_long)pgno);
	} else {
		printf("0x%lx ", (u_long)R_OFFSET(&lt->reginfo, lockobj));
		__db_pr(ptr, lockobj->lockobj.size, stdout);
		printf("\n");
	}
}
