/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2000\nSleepycat Software Inc.  All rights reserved.\n";
static const char revid[] =
    "$Id: db_stat.c,v 11.42 2001/01/18 18:36:59 bostic Exp $";
#endif

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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "lock.h"
#include "mp.h"

#define	PCT(f, t, pgsize)						\
    ((t) == 0 ? 0 :							\
    (((double)(((t) * (pgsize)) - (f)) / ((t) * (pgsize))) * 100))

typedef enum { T_NOTSET, T_DB, T_ENV, T_LOCK, T_LOG, T_MPOOL, T_TXN } test_t;

int	 argcheck __P((char *, const char *));
int	 btree_stats __P((DB_ENV *, DB *, DB_BTREE_STAT *));
int	 db_init __P((char *, test_t));
void	 dl __P((const char *, u_long));
void	 dl_bytes __P((const char *, u_long, u_long, u_long));
int	 env_stats __P((DB_ENV *));
int	 hash_stats __P((DB_ENV *, DB *));
int	 lock_ok __P((char *));
int	 lock_stats __P((DB_ENV *));
int	 log_stats __P((DB_ENV *));
int	 main __P((int, char *[]));
int	 mpool_ok __P((char *));
int	 mpool_stats __P((DB_ENV *));
void	 prflags __P((u_int32_t, const FN *));
int	 queue_stats __P((DB_ENV *, DB *));
int	 txn_compare __P((const void *, const void *));
int	 txn_stats __P((DB_ENV *));
void	 usage __P((void));
void	 version_check __P((void));

DB_ENV	*dbenv;
char	*internal;
const char
	*progname = "db_stat";				/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_BTREE_STAT *sp;
	DB *alt_dbp, *dbp;
	test_t ttype;
	int ch, checked, d_close, e_close, exitval, nflag, ret;
	char *db, *home, *subdb;

	version_check();

	dbp = NULL;
	ttype = T_NOTSET;
	nflag = 0;
	d_close = e_close = exitval = 0;
	db = home = subdb = NULL;
	while ((ch = getopt(argc, argv, "C:cd:eh:lM:mNs:tV")) != EOF)
		switch (ch) {
		case 'C':
			ttype = T_LOCK;
			if (!argcheck(internal = optarg, "Acflmo"))
				usage();
			break;
		case 'c':
			ttype = T_LOCK;
			break;
		case 'd':
			db = optarg;
			ttype = T_DB;
			break;
		case 'e':
			ttype = T_ENV;
			break;
		case 'h':
			home = optarg;
			break;
		case 'l':
			ttype = T_LOG;
			break;
		case 'M':
			ttype = T_MPOOL;
			if (!argcheck(internal = optarg, "Ahlm"))
				usage();
			break;
		case 'm':
			ttype = T_MPOOL;
			break;
		case 'N':
			nflag = 1;
			if ((ret = db_env_set_panicstate(0)) != 0) {
				fprintf(stderr,
				    "%s: db_env_set_panicstate: %s\n",
				    progname, db_strerror(ret));
				return (1);
			}
			break;
		case 's':
			subdb = optarg;
			ttype = T_DB;
			break;
		case 't':
			ttype = T_TXN;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			exit(0);
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch (ttype) {
	case T_DB:
		if (db == NULL)
			usage();
		break;
	case T_NOTSET:
		usage();
		/* NOTREACHED */
	default:
		break;
	}

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		goto shutdown;
	}
	e_close = 1;

	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, progname);

	if (nflag && (ret = dbenv->set_mutexlocks(dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "set_mutexlocks");
		goto shutdown;
	}

	/* Initialize the environment. */
	if (db_init(home, ttype) != 0)
		goto shutdown;

	switch (ttype) {
	case T_DB:
		/* Create the DB object and open the file. */
		if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
			dbenv->err(dbenv, ret, "db_create");
			goto shutdown;
		}

		if ((ret =
		    dbp->open(dbp, db, subdb, DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
			dbp->err(dbp, ret, "open: %s", db);
			goto shutdown;
		}

		/*
		 * See if we can open this db read/write to update counts.
		 * If its a master-db then we cannot.  So check to see,
		 * if its btree then it might be.
		 */
		checked = 0;
		if (subdb == NULL && dbp->type == DB_BTREE) {
			if ((ret = dbp->stat(dbp, &sp, NULL, 0)) != 0) {
				dbp->err(dbp, ret, "dbp->stat");
				return (1);
			}
			checked = 1;
		}

		if (subdb != NULL ||
		    dbp->type != DB_BTREE ||
		    (sp->bt_metaflags & BTM_SUBDB) == 0) {
			if ((ret = db_create(&alt_dbp, dbenv, 0)) != 0) {
				dbenv->err(dbenv, ret, "db_create");
				goto shutdown;
			}
			if ((ret = dbp->open(alt_dbp,
			    db, subdb, DB_UNKNOWN, 0, 0)) == 0) {
				(void)dbp->close(dbp, 0);
				dbp = alt_dbp;
			}
			/* Need to run again to update counts */
			checked = 0;
		}

		d_close = 1;
		switch (dbp->type) {
		case DB_BTREE:
		case DB_RECNO:
			if (btree_stats(dbenv, dbp, checked == 1 ? sp : NULL))
				goto shutdown;
			break;
		case DB_HASH:
			if (hash_stats(dbenv, dbp))
				goto shutdown;
			break;
		case DB_QUEUE:
			if (queue_stats(dbenv, dbp))
				goto shutdown;
			break;
		case DB_UNKNOWN:
			abort();		/* Impossible. */
			/* NOTREACHED */
		}
		break;
	case T_ENV:
		if (env_stats(dbenv))
			exitval = 1;
		break;
	case T_LOCK:
		if (lock_stats(dbenv))
			exitval = 1;
		break;
	case T_LOG:
		if (log_stats(dbenv))
			exitval = 1;
		break;
	case T_MPOOL:
		if (mpool_stats(dbenv))
			exitval = 1;
		break;
	case T_TXN:
		if (txn_stats(dbenv))
			exitval = 1;
		break;
	case T_NOTSET:
		abort();			/* Impossible. */
		/* NOTREACHED */
	}

	if (0) {
shutdown:	exitval = 1;
	}
	if (d_close && (ret = dbp->close(dbp, 0)) != 0) {
		exitval = 1;
		dbp->err(dbp, ret, "close");
	}
	if (e_close && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	/* Resend any caught signal. */
	__db_util_sigresend();

	return (exitval);
}

/*
 * env_stats --
 *	Display environment statistics.
 */
int
env_stats(dbenvp)
	DB_ENV *dbenvp;
{
	REGENV renv;
	REGION *rp, regs[1024];
	int n, ret;
	const char *lable;

	n = sizeof(regs) / sizeof(regs[0]);
	if ((ret = __db_e_stat(dbenvp, &renv, regs, &n)) != 0)  {
		dbenvp->err(dbenvp, ret, "__db_e_stat");
		return (1);
	}

	printf("%d.%d.%d\tEnvironment version.\n",
	    renv.majver, renv.minver, renv.patch);
	printf("%lx\tMagic number.\n", (u_long)renv.magic);
	printf("%d\tPanic value.\n", renv.panic);

	/* Adjust the reference count for us... */
	printf("%d\tReferences.\n", renv.refcnt - 1);

	dl("Locks granted without waiting.\n",
	    (u_long)renv.mutex.mutex_set_nowait);
	dl("Locks granted after waiting.\n",
	    (u_long)renv.mutex.mutex_set_wait);

	while (n > 0) {
		printf("%s\n", DB_LINE);
		rp = &regs[--n];
		switch (rp->type) {
		case REGION_TYPE_ENV:
			lable = "Environment";
			break;
		case REGION_TYPE_LOCK:
			lable = "Lock";
			break;
		case REGION_TYPE_LOG:
			lable = "Log";
			break;
		case REGION_TYPE_MPOOL:
			lable = "Mpool";
			break;
		case REGION_TYPE_MUTEX:
			lable = "Mutex";
			break;
		case REGION_TYPE_TXN:
			lable = "Txn";
			break;
		case INVALID_REGION_TYPE:
		default:
			lable = "Invalid";
			break;
		}
		printf("%s Region: %d.\n", lable, rp->id);
		dl_bytes("Size", (u_long)0, (u_long)0, (u_long)rp->size);
		printf("%ld\tSegment ID.\n", rp->segid);
		dl("Locks granted without waiting.\n",
		    (u_long)rp->mutex.mutex_set_nowait);
		dl("Locks granted after waiting.\n",
		    (u_long)rp->mutex.mutex_set_wait);
	}

	return (0);
}

/*
 * btree_stats --
 *	Display btree/recno statistics.
 */
int
btree_stats(dbenvp, dbp, msp)
	DB_ENV *dbenvp;
	DB *dbp;
	DB_BTREE_STAT *msp;
{
	static const FN fn[] = {
		{ BTM_DUP,	"duplicates" },
		{ BTM_FIXEDLEN,	"fixed-length" },
		{ BTM_RECNO,	"recno" },
		{ BTM_RECNUM,	"record-numbers" },
		{ BTM_RENUMBER,	"renumber" },
		{ BTM_SUBDB,	"multiple-databases" },
		{ 0,		NULL }
	};
	DB_BTREE_STAT *sp;
	int ret;

	COMPQUIET(dbenvp, NULL);

	if (msp != NULL)
		sp = msp;
	else if ((ret = dbp->stat(dbp, &sp, NULL, 0)) != 0) {
		dbp->err(dbp, ret, "dbp->stat");
		return (1);
	}

	printf("%lx\tBtree magic number.\n", (u_long)sp->bt_magic);
	printf("%lu\tBtree version number.\n", (u_long)sp->bt_version);
	prflags(sp->bt_metaflags, fn);
	if (dbp->type == DB_BTREE) {
#ifdef NOT_IMPLEMENTED
		dl("Maximum keys per-page.\n", (u_long)sp->bt_maxkey);
#endif
		dl("Minimum keys per-page.\n", (u_long)sp->bt_minkey);
	}
	if (dbp->type == DB_RECNO) {
		dl("Fixed-length record size.\n", (u_long)sp->bt_re_len);
		if (isprint(sp->bt_re_pad) && !isspace(sp->bt_re_pad))
			printf("%c\tFixed-length record pad.\n",
			    (int)sp->bt_re_pad);
		else
			printf("0x%x\tFixed-length record pad.\n",
			    (int)sp->bt_re_pad);
	}
	dl("Underlying database page size.\n", (u_long)sp->bt_pagesize);
	dl("Number of levels in the tree.\n", (u_long)sp->bt_levels);
	dl(dbp->type == DB_BTREE ?
	    "Number of unique keys in the tree.\n" :
	    "Number of records in the tree.\n", (u_long)sp->bt_nkeys);
	dl("Number of data items in the tree.\n", (u_long)sp->bt_ndata);

	dl("Number of tree internal pages.\n", (u_long)sp->bt_int_pg);
	dl("Number of bytes free in tree internal pages",
	    (u_long)sp->bt_int_pgfree);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->bt_int_pgfree, sp->bt_int_pg, sp->bt_pagesize));

	dl("Number of tree leaf pages.\n", (u_long)sp->bt_leaf_pg);
	dl("Number of bytes free in tree leaf pages",
	    (u_long)sp->bt_leaf_pgfree);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->bt_leaf_pgfree, sp->bt_leaf_pg, sp->bt_pagesize));

	dl("Number of tree duplicate pages.\n", (u_long)sp->bt_dup_pg);
	dl("Number of bytes free in tree duplicate pages",
	    (u_long)sp->bt_dup_pgfree);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->bt_dup_pgfree, sp->bt_dup_pg, sp->bt_pagesize));

	dl("Number of tree overflow pages.\n", (u_long)sp->bt_over_pg);
	dl("Number of bytes free in tree overflow pages",
	    (u_long)sp->bt_over_pgfree);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->bt_over_pgfree, sp->bt_over_pg, sp->bt_pagesize));

	dl("Number of pages on the free list.\n", (u_long)sp->bt_free);

	return (0);
}

/*
 * hash_stats --
 *	Display hash statistics.
 */
int
hash_stats(dbenvp, dbp)
	DB_ENV *dbenvp;
	DB *dbp;
{
	static const FN fn[] = {
		{ DB_HASH_DUP,	"duplicates" },
		{ DB_HASH_SUBDB,"multiple-databases" },
		{ 0,		NULL }
	};
	DB_HASH_STAT *sp;
	int ret;

	COMPQUIET(dbenvp, NULL);

	if ((ret = dbp->stat(dbp, &sp, NULL, 0)) != 0) {
		dbp->err(dbp, ret, "dbp->stat");
		return (1);
	}

	printf("%lx\tHash magic number.\n", (u_long)sp->hash_magic);
	printf("%lu\tHash version number.\n", (u_long)sp->hash_version);
	prflags(sp->hash_metaflags, fn);
	dl("Underlying database page size.\n", (u_long)sp->hash_pagesize);
	dl("Number of keys in the database.\n", (u_long)sp->hash_nkeys);
	dl("Number of data items in the database.\n", (u_long)sp->hash_ndata);

	dl("Number of hash buckets.\n", (u_long)sp->hash_buckets);
	dl("Number of bytes free on bucket pages", (u_long)sp->hash_bfree);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->hash_bfree, sp->hash_buckets, sp->hash_pagesize));

	dl("Number of overflow pages.\n", (u_long)sp->hash_bigpages);
	dl("Number of bytes free in overflow pages",
	    (u_long)sp->hash_big_bfree);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->hash_big_bfree, sp->hash_bigpages, sp->hash_pagesize));

	dl("Number of bucket overflow pages.\n", (u_long)sp->hash_overflows);
	dl("Number of bytes free in bucket overflow pages",
	    (u_long)sp->hash_ovfl_free);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->hash_ovfl_free, sp->hash_overflows, sp->hash_pagesize));

	dl("Number of duplicate pages.\n", (u_long)sp->hash_dup);
	dl("Number of bytes free in duplicate pages",
	    (u_long)sp->hash_dup_free);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->hash_dup_free, sp->hash_dup, sp->hash_pagesize));

	dl("Number of pages on the free list.\n", (u_long)sp->hash_free);

	return (0);
}

/*
 * queue_stats --
 *	Display queue statistics.
 */
int
queue_stats(dbenvp, dbp)
	DB_ENV *dbenvp;
	DB *dbp;
{
	DB_QUEUE_STAT *sp;
	int ret;

	COMPQUIET(dbenvp, NULL);

	if ((ret = dbp->stat(dbp, &sp, NULL, 0)) != 0) {
		dbp->err(dbp, ret, "dbp->stat");
		return (1);
	}

	printf("%lx\tQueue magic number.\n", (u_long)sp->qs_magic);
	printf("%lu\tQueue version number.\n", (u_long)sp->qs_version);
	dl("Fixed-length record size.\n", (u_long)sp->qs_re_len);
	if (isprint(sp->qs_re_pad) && !isspace(sp->qs_re_pad))
		printf("%c\tFixed-length record pad.\n", (int)sp->qs_re_pad);
	else
		printf("0x%x\tFixed-length record pad.\n", (int)sp->qs_re_pad);
	dl("Underlying database page size.\n", (u_long)sp->qs_pagesize);
	dl("Number of records in the database.\n", (u_long)sp->qs_nkeys);
	dl("Number of database pages.\n", (u_long)sp->qs_pages);
	dl("Number of bytes free in database pages", (u_long)sp->qs_pgfree);
	printf(" (%.0f%% ff).\n",
	    PCT(sp->qs_pgfree, sp->qs_pages, sp->qs_pagesize));
	printf("%lu\tFirst undeleted record.\n", (u_long)sp->qs_first_recno);
	printf(
	    "%lu\tLast allocated record number.\n", (u_long)sp->qs_cur_recno);

	return (0);
}

/*
 * lock_stats --
 *	Display lock statistics.
 */
int
lock_stats(dbenvp)
	DB_ENV *dbenvp;
{
	DB_LOCK_STAT *sp;
	int ret;

	if (internal != NULL) {
		__lock_dump_region(dbenvp, internal, stdout);
		return (0);
	}

	if ((ret = lock_stat(dbenvp, &sp, NULL)) != 0) {
		dbenvp->err(dbenvp, ret, NULL);
		return (1);
	}

	dl("Last allocated locker ID.\n", (u_long)sp->st_lastid);
	dl("Number of lock modes.\n", (u_long)sp->st_nmodes);
	dl("Maximum number of locks possible.\n", (u_long)sp->st_maxlocks);
	dl("Maximum number of lockers possible.\n", (u_long)sp->st_maxlockers);
	dl("Maximum number of objects possible.\n", (u_long)sp->st_maxobjects);
	dl("Current locks.\n", (u_long)sp->st_nlocks);
	dl("Maximum number of locks so far.\n", (u_long)sp->st_maxnlocks);
	dl("Current number of lockers.\n", (u_long)sp->st_nlockers);
	dl("Maximum number  lockers so far.\n", (u_long)sp->st_maxnlockers);
	dl("Current number lock objects.\n", (u_long)sp->st_nobjects);
	dl("Maximum number of lock objects so far.\n",
	    (u_long)sp->st_maxnobjects);
	dl("Number of lock requests.\n", (u_long)sp->st_nrequests);
	dl("Number of lock releases.\n", (u_long)sp->st_nreleases);
	dl("Number of lock requests that would have waited.\n",
	    (u_long)sp->st_nnowaits);
	dl("Number of lock conflicts.\n", (u_long)sp->st_nconflicts);
	dl("Number of deadlocks.\n", (u_long)sp->st_ndeadlocks);
	dl_bytes("Lock region size",
	    (u_long)0, (u_long)0, (u_long)sp->st_regsize);
	dl("The number of region locks granted without waiting.\n",
	    (u_long)sp->st_region_nowait);
	dl("The number of region locks granted after waiting.\n",
	    (u_long)sp->st_region_wait);

	return (0);
}

/*
 * log_stats --
 *	Display log statistics.
 */
int
log_stats(dbenvp)
	DB_ENV *dbenvp;
{
	DB_LOG_STAT *sp;
	int ret;

	if ((ret = log_stat(dbenvp, &sp, NULL)) != 0) {
		dbenvp->err(dbenvp, ret, NULL);
		return (1);
	}

	printf("%lx\tLog magic number.\n", (u_long)sp->st_magic);
	printf("%lu\tLog version number.\n", (u_long)sp->st_version);
	dl_bytes("Log region size",
	    (u_long)0, (u_long)0, (u_long)sp->st_regsize);
	dl_bytes("Log record cache size",
	    (u_long)0, (u_long)0, (u_long)sp->st_lg_bsize);
	printf("%#o\tLog file mode.\n", sp->st_mode);
	if (sp->st_lg_max % MEGABYTE == 0)
		printf("%luMb\tLog file size.\n",
		    (u_long)sp->st_lg_max / MEGABYTE);
	else if (sp->st_lg_max % 1024 == 0)
		printf("%luKb\tLog file size.\n", (u_long)sp->st_lg_max / 1024);
	else
		printf("%lu\tLog file size.\n", (u_long)sp->st_lg_max);
	dl_bytes("Log bytes written",
	    (u_long)0, (u_long)sp->st_w_mbytes, (u_long)sp->st_w_bytes);
	dl_bytes("Log bytes written since last checkpoint",
	    (u_long)0, (u_long)sp->st_wc_mbytes, (u_long)sp->st_wc_bytes);
	dl("Total log file writes.\n", (u_long)sp->st_wcount);
	dl("Total log file write due to overflow.\n",
	    (u_long)sp->st_wcount_fill);
	dl("Total log file flushes.\n", (u_long)sp->st_scount);
	printf("%lu\tCurrent log file number.\n", (u_long)sp->st_cur_file);
	printf("%lu\tCurrent log file offset.\n", (u_long)sp->st_cur_offset);
	dl("The number of region locks granted without waiting.\n",
	    (u_long)sp->st_region_nowait);
	dl("The number of region locks granted after waiting.\n",
	    (u_long)sp->st_region_wait);

	return (0);
}

/*
 * mpool_stats --
 *	Display mpool statistics.
 */
int
mpool_stats(dbenvp)
	DB_ENV *dbenvp;
{
	DB_MPOOL_FSTAT **fsp;
	DB_MPOOL_STAT *gsp;
	int ret;

	if (internal != NULL) {
		__memp_dump_region(dbenvp, internal, stdout);
		return (1);
	}

	if ((ret = memp_stat(dbenvp, &gsp, &fsp, NULL)) != 0) {
		dbenvp->err(dbenvp, ret, NULL);
		return (1);
	}

	dl_bytes("Total cache size",
	    (u_long)gsp->st_gbytes, (u_long)0, (u_long)gsp->st_bytes);
	dl("Number of caches.\n", (u_long)gsp->st_ncache);
	dl("Pool individual cache size.\n", (u_long)gsp->st_regsize);
	dl("Requested pages found in the cache", (u_long)gsp->st_cache_hit);
	if (gsp->st_cache_hit + gsp->st_cache_miss != 0)
		printf(" (%.0f%%)", ((double)gsp->st_cache_hit /
		    (gsp->st_cache_hit + gsp->st_cache_miss)) * 100);
	printf(".\n");
	dl("Requested pages mapped into the process' address space.\n",
	    (u_long)gsp->st_map);
	dl("Requested pages not found in the cache.\n",
	    (u_long)gsp->st_cache_miss);
	dl("Pages created in the cache.\n", (u_long)gsp->st_page_create);
	dl("Pages read into the cache.\n", (u_long)gsp->st_page_in);
	dl("Pages written from the cache to the backing file.\n",
	    (u_long)gsp->st_page_out);
	dl("Clean pages forced from the cache.\n",
	    (u_long)gsp->st_ro_evict);
	dl("Dirty pages forced from the cache.\n",
	    (u_long)gsp->st_rw_evict);
	dl("Dirty buffers written by trickle-sync thread.\n",
	    (u_long)gsp->st_page_trickle);
	dl("Current clean buffer count.\n",
	    (u_long)gsp->st_page_clean);
	dl("Current dirty buffer count.\n",
	    (u_long)gsp->st_page_dirty);
	dl("Number of hash buckets used for page location.\n",
	    (u_long)gsp->st_hash_buckets);
	dl("Total number of times hash chains searched for a page.\n",
	    (u_long)gsp->st_hash_searches);
	dl("The longest hash chain searched for a page.\n",
	    (u_long)gsp->st_hash_longest);
	dl("Total number of hash buckets examined for page location.\n",
	    (u_long)gsp->st_hash_examined);
	dl("The number of region locks granted without waiting.\n",
	    (u_long)gsp->st_region_nowait);
	dl("The number of region locks granted after waiting.\n",
	    (u_long)gsp->st_region_wait);

	for (; fsp != NULL && *fsp != NULL; ++fsp) {
		printf("%s\n", DB_LINE);
		printf("Pool File: %s\n", (*fsp)->file_name);
		dl("Page size.\n", (u_long)(*fsp)->st_pagesize);
		dl("Requested pages found in the cache",
		    (u_long)(*fsp)->st_cache_hit);
		if ((*fsp)->st_cache_hit + (*fsp)->st_cache_miss != 0)
			printf(" (%.0f%%)", ((double)(*fsp)->st_cache_hit /
			    ((*fsp)->st_cache_hit + (*fsp)->st_cache_miss)) *
			    100);
		printf(".\n");
		dl("Requested pages mapped into the process' address space.\n",
		    (u_long)(*fsp)->st_map);
		dl("Requested pages not found in the cache.\n",
		    (u_long)(*fsp)->st_cache_miss);
		dl("Pages created in the cache.\n",
		    (u_long)(*fsp)->st_page_create);
		dl("Pages read into the cache.\n",
		    (u_long)(*fsp)->st_page_in);
		dl("Pages written from the cache to the backing file.\n",
		    (u_long)(*fsp)->st_page_out);
	}

	return (0);
}

/*
 * txn_stats --
 *	Display transaction statistics.
 */
int
txn_stats(dbenvp)
	DB_ENV *dbenvp;
{
	DB_TXN_STAT *sp;
	u_int32_t i;
	int ret;
	const char *p;

	if ((ret = txn_stat(dbenvp, &sp, NULL)) != 0) {
		dbenvp->err(dbenvp, ret, NULL);
		return (1);
	}

	p = sp->st_last_ckp.file == 0 ?
	    "No checkpoint LSN." : "File/offset for last checkpoint LSN.";
	printf("%lu/%lu\t%s\n",
	    (u_long)sp->st_last_ckp.file, (u_long)sp->st_last_ckp.offset, p);
	p = sp->st_pending_ckp.file == 0 ?
	    "No pending checkpoint LSN." :
	    "File/offset for last pending checkpoint LSN.";
	printf("%lu/%lu\t%s\n",
	    (u_long)sp->st_pending_ckp.file,
	    (u_long)sp->st_pending_ckp.offset, p);
	if (sp->st_time_ckp == 0)
		printf("0\tNo checkpoint timestamp.\n");
	else
		printf("%.24s\tCheckpoint timestamp.\n",
		    ctime(&sp->st_time_ckp));
	printf("%lx\tLast transaction ID allocated.\n",
	    (u_long)sp->st_last_txnid);
	dl("Maximum number of active transactions possible.\n",
	    (u_long)sp->st_maxtxns);
	dl("Active transactions.\n", (u_long)sp->st_nactive);
	dl("Maximum active transactions.\n", (u_long)sp->st_maxnactive);
	dl("Number of transactions begun.\n", (u_long)sp->st_nbegins);
	dl("Number of transactions aborted.\n", (u_long)sp->st_naborts);
	dl("Number of transactions committed.\n", (u_long)sp->st_ncommits);
	dl_bytes("Transaction region size",
	    (u_long)0, (u_long)0, (u_long)sp->st_regsize);
	dl("The number of region locks granted without waiting.\n",
	    (u_long)sp->st_region_nowait);
	dl("The number of region locks granted after waiting.\n",
	    (u_long)sp->st_region_wait);
	qsort(sp->st_txnarray,
	    sp->st_nactive, sizeof(sp->st_txnarray[0]), txn_compare);
	for (i = 0; i < sp->st_nactive; ++i)
		printf("\tid: %lx; initial LSN file/offest %lu/%lu\n",
		    (u_long)sp->st_txnarray[i].txnid,
		    (u_long)sp->st_txnarray[i].lsn.file,
		    (u_long)sp->st_txnarray[i].lsn.offset);

	return (0);
}

int
txn_compare(a1, b1)
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

/*
 * dl --
 *	Display a big value.
 */
void
dl(msg, value)
	const char *msg;
	u_long value;
{
	/*
	 * Two formats: if less than 10 million, display as the number, if
	 * greater than 10 million display as ###M.
	 */
	if (value < 10000000)
		printf("%lu\t%s", value, msg);
	else
		printf("%luM\t%s", value / 1000000, msg);
}

/*
 * dl_bytes --
 *	Display a big number of bytes.
 */
void
dl_bytes(msg, gbytes, mbytes, bytes)
	const char *msg;
	u_long gbytes, mbytes, bytes;
{
	const char *sep;
	u_long sbytes;
	int showbytes;

	sbytes = bytes;
	while (bytes > MEGABYTE) {
		++mbytes;
		bytes -= MEGABYTE;
	}
	while (mbytes > GIGABYTE / MEGABYTE) {
		++gbytes;
		--mbytes;
	}

	sep = "";
	showbytes = 0;
	if (gbytes > 0) {
		printf("%luGB", gbytes);
		sep = " ";
		showbytes = 1;
	}
	if (mbytes > 0) {
		printf("%s%luMB", sep, mbytes);
		sep = " ";
		showbytes = 1;
	}
	if (bytes > 1024) {
		printf("%s%luKB", sep, bytes / 1024);
		bytes %= 1024;
		sep = " ";
		showbytes = 1;
	}
	if (bytes > 0)
		printf("%s%luB", sep, bytes);
	printf("\t%s", msg);
	if (showbytes)
		printf(" (%lu bytes)", sbytes);
	printf(".\n");
}

/*
 * prflags --
 *	Print out flag values.
 */
void
prflags(flags, fnp)
	u_int32_t flags;
	const FN *fnp;
{
	const char *sep;

	sep = "\t";
	printf("Flags:");
	for (; fnp->mask != 0; ++fnp)
		if (fnp->mask & flags) {
			printf("%s%s", sep, fnp->name);
			sep = ", ";
		}
	printf("\n");
}

/*
 * db_init --
 *	Initialize the environment.
 */
int
db_init(home, ttype)
	char *home;
	test_t ttype;
{
	int ret;

	/*
	 * If our environment open fails, and we're trying to look at a
	 * shared region, it's a hard failure.
	 */
	if ((ret = dbenv->open(dbenv,
	    home, DB_JOINENV | DB_USE_ENVIRON, 0)) == 0)
		return (0);
	if (ttype != T_DB) {
		dbenv->err(dbenv, ret, "DBENV->open%s%s",
		    home == NULL ? "" : ": ", home == NULL ? "" : home);
		return (1);
	}

	/*
	 * We're trying to look at a database.
	 *
	 * An environment is required because we may be trying to look at
	 * databases in directories other than the current one.  We could
	 * avoid using an environment iff the -h option wasn't specified,
	 * but that seems like more work than it's worth.
	 *
	 *
	 * No environment exists.  Create one, but make it private so that
	 * no files are actually created.
	 *
	 * Note that we will probably just drop core if the environment
	 * we joined above does not include a memory pool.  This is probably
	 * acceptable;  trying to use an existing shared environment that
	 * does not contain a memory pool to look at a database can
	 * be safely construed as operator error, I think.
	 */
	if ((ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON, 0)) == 0)
		return (0);

	/* An environment is required. */
	dbenv->err(dbenv, ret, "open");
	return (1);
}

/*
 * argcheck --
 *	Return if argument flags are okay.
 */
int
argcheck(arg, ok_args)
	char *arg;
	const char *ok_args;
{
	for (; *arg != '\0'; ++arg)
		if (strchr(ok_args, *arg) == NULL)
			return (0);
	return (1);
}

void
usage()
{
	fprintf(stderr, "usage: db_stat %s\n",
	    "[-celmNtV] [-C Acflmo] [-d file [-s file]] [-h home] [-M Ahlm]");
	exit (1);
}

void
version_check()
{
	int v_major, v_minor, v_patch;

	/* Make sure we're loaded with the right version of the DB library. */
	(void)db_version(&v_major, &v_minor, &v_patch);
	if (v_major != DB_VERSION_MAJOR ||
	    v_minor != DB_VERSION_MINOR || v_patch != DB_VERSION_PATCH) {
		fprintf(stderr,
	"%s: version %d.%d.%d doesn't match library version %d.%d.%d\n",
		    progname, DB_VERSION_MAJOR, DB_VERSION_MINOR,
		    DB_VERSION_PATCH, v_major, v_minor, v_patch);
		exit (1);
	}
}
