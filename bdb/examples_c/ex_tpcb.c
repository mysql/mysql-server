/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_tpcb.c,v 11.21 2000/10/27 20:32:00 dda Exp $
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef DB_WIN32
#include <sys/types.h>
#include <sys/timeb.h>
#endif

#include <db.h>

typedef enum { ACCOUNT, BRANCH, TELLER } FTYPE;

DB_ENV	 *db_init __P((char *, char *, int, int, int));
int	  hpopulate __P((DB *, int, int, int, int));
int	  populate __P((DB *, u_int32_t, u_int32_t, int, char *));
u_int32_t random_id __P((FTYPE, int, int, int));
u_int32_t random_int __P((u_int32_t, u_int32_t));
int	  tp_populate __P((DB_ENV *, int, int, int, int, int));
int	  tp_run __P((DB_ENV *, int, int, int, int, int));
int	  tp_txn __P((DB_ENV *, DB *, DB *, DB *, DB *, int, int, int, int));

#ifdef HAVE_VXWORKS
#define	ERROR_RETURN	ERROR
#define	HOME	"/vxtmp/vxtmp/TESTDIR"
#define	VXSHM_KEY	13
int	  ex_tpcb_init __P(());
int	  ex_tpcb __P(());
#else
#define	ERROR_RETURN	1
void	  invarg __P((char *, int, char *));
int	  main __P((int, char *[]));
void	  usage __P((char *));
#endif

/*
 * This program implements a basic TPC/B driver program.  To create the
 * TPC/B database, run with the -i (init) flag.  The number of records
 * with which to populate the account, history, branch, and teller tables
 * is specified by the a, s, b, and t flags respectively.  To run a TPC/B
 * test, use the n flag to indicate a number of transactions to run (note
 * that you can run many of these processes in parallel to simulate a
 * multiuser test run).
 */
#define	TELLERS_PER_BRANCH	10
#define	ACCOUNTS_PER_TELLER	10000
#define	HISTORY_PER_BRANCH	2592000

/*
 * The default configuration that adheres to TPCB scaling rules requires
 * nearly 3 GB of space.  To avoid requiring that much space for testing,
 * we set the parameters much lower.  If you want to run a valid 10 TPS
 * configuration, define VALID_SCALING.
 */
#ifdef	VALID_SCALING
#define	ACCOUNTS	 1000000
#define	BRANCHES	      10
#define	TELLERS		     100
#define	HISTORY		25920000
#endif

#ifdef	TINY
#define	ACCOUNTS	    1000
#define	BRANCHES	      10
#define	TELLERS		     100
#define	HISTORY		   10000
#endif

#ifdef	VERY_TINY
#define	ACCOUNTS	     500
#define	BRANCHES	      10
#define	TELLERS		      50
#define	HISTORY		    5000
#endif

#if !defined(VALID_SCALING) && !defined(TINY) && !defined(VERY_TINY)
#define	ACCOUNTS	  100000
#define	BRANCHES	      10
#define	TELLERS		     100
#define	HISTORY		  259200
#endif

#define	HISTORY_LEN	    100
#define	RECLEN		    100
#define	BEGID		1000000

typedef struct _defrec {
	u_int32_t	id;
	u_int32_t	balance;
	u_int8_t	pad[RECLEN - sizeof(u_int32_t) - sizeof(u_int32_t)];
} defrec;

typedef struct _histrec {
	u_int32_t	aid;
	u_int32_t	bid;
	u_int32_t	tid;
	u_int32_t	amount;
	u_int8_t	pad[RECLEN - 4 * sizeof(u_int32_t)];
} histrec;

#ifdef HAVE_VXWORKS
int
ex_tpcb_init()
{
	DB_ENV *dbenv;
	int accounts, branches, ret, seed, t_ret, tellers, history, verbose;
	char *home;
	char *progname = "ex_tpcb_init";		/* Program name. */

	verbose = 1;
	if ((dbenv = db_init(HOME, progname, 0, 1, 0)) == NULL)
		return (ERROR_RETURN);

	accounts = ACCOUNTS;
	branches = BRANCHES;
	tellers = TELLERS;
	history = HISTORY;

	if ((ret = tp_populate(dbenv, accounts, branches, history, tellers,
	    verbose)) != OK)
		fprintf(stderr, "%s: %s\n", progname, db_strerror(ret));
	if ((t_ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr, "%s: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}

	return (ret == 0 ? t_ret : ret);
}

int
ex_tpcb()
{
	DB_ENV *dbenv;
	int accounts, branches, seed, tellers, history;
	int ch, mpool, ntxns, ret, t_ret, txn_no_sync, verbose;
	char *progname = "ex_tpcb";		/* Program name. */

	accounts = ACCOUNTS;
	branches = BRANCHES;
	tellers = TELLERS;
	history = HISTORY;

	txn_no_sync = 0;
	mpool = 0;
	ntxns = 20;
	verbose = 1;
	seed = (int)((u_int)getpid() | time(NULL));

	srand((u_int)seed);

	/* Initialize the database environment. */
	if ((dbenv = db_init(HOME, progname, mpool, 0,
	    txn_no_sync ? DB_TXN_NOSYNC : 0)) == NULL)
		return (ERROR_RETURN);

	if (verbose)
		printf("%ld Accounts, %ld Branches, %ld Tellers, %ld History\n",
		    (long)accounts, (long)branches,
		    (long)tellers, (long)history);

	if ((ret = tp_run(dbenv, ntxns, accounts, branches, tellers, verbose))
	    != OK)
		fprintf(stderr, "tp_run failed\n");

	if ((t_ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr, "%s: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
	return (ret == 0 ? t_ret : ret);
}
#else
int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV *dbenv;
	int accounts, branches, seed, tellers, history;
	int ch, iflag, mpool, ntxns, ret, txn_no_sync, verbose;
	char *home, *progname;

	home = "TESTDIR";
	progname = "ex_tpcb";
	accounts = branches = history = tellers = 0;
	txn_no_sync = 0;
	mpool = ntxns = 0;
	verbose = 0;
	iflag = 0;
	seed = (int)((u_int)getpid() | time(NULL));
	while ((ch = getopt(argc, argv, "a:b:c:fh:in:S:s:t:v")) != EOF)
		switch (ch) {
		case 'a':			/* Number of account records */
			if ((accounts = atoi(optarg)) <= 0)
				invarg(progname, ch, optarg);
			break;
		case 'b':			/* Number of branch records */
			if ((branches = atoi(optarg)) <= 0)
				invarg(progname, ch, optarg);
			break;
		case 'c':			/* Cachesize in bytes */
			if ((mpool = atoi(optarg)) <= 0)
				invarg(progname, ch, optarg);
			break;
		case 'f':			/* Fast mode: no txn sync. */
			txn_no_sync = 1;
			break;
		case 'h':			/* DB  home. */
			home = optarg;
			break;
		case 'i':			/* Initialize the test. */
			iflag = 1;
			break;
		case 'n':			/* Number of transactions */
			if ((ntxns = atoi(optarg)) <= 0)
				invarg(progname, ch, optarg);
			break;
		case 'S':			/* Random number seed. */
			if ((seed = atoi(optarg)) <= 0)
				invarg(progname, ch, optarg);
			break;
		case 's':			/* Number of history records */
			if ((history = atoi(optarg)) <= 0)
				invarg(progname, ch, optarg);
			break;
		case 't':			/* Number of teller records */
			if ((tellers = atoi(optarg)) <= 0)
				invarg(progname, ch, optarg);
			break;
		case 'v':			/* Verbose option. */
			verbose = 1;
			break;
		case '?':
		default:
			usage(progname);
		}
	argc -= optind;
	argv += optind;

	srand((u_int)seed);

	/* Initialize the database environment. */
	if ((dbenv = db_init(home,
	    progname, mpool, iflag, txn_no_sync ? DB_TXN_NOSYNC : 0)) == NULL)
		return (1);

	accounts = accounts == 0 ? ACCOUNTS : accounts;
	branches = branches == 0 ? BRANCHES : branches;
	tellers = tellers == 0 ? TELLERS : tellers;
	history = history == 0 ? HISTORY : history;

	if (verbose)
		printf("%ld Accounts, %ld Branches, %ld Tellers, %ld History\n",
		    (long)accounts, (long)branches,
		    (long)tellers, (long)history);

	if (iflag) {
		if (ntxns != 0)
			usage(progname);
		tp_populate(dbenv,
		    accounts, branches, history, tellers, verbose);
	} else {
		if (ntxns == 0)
			usage(progname);
		tp_run(dbenv, ntxns, accounts, branches, tellers, verbose);
	}

	if ((ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr, "%s: dbenv->close failed: %s\n",
		    progname, db_strerror(ret));
		return (1);
	}

	return (0);
}

void
invarg(progname, arg, str)
	char *progname;
	int arg;
	char *str;
{
	(void)fprintf(stderr,
	    "%s: invalid argument for -%c: %s\n", progname, arg, str);
	exit (1);
}

void
usage(progname)
	char *progname;
{
	char *a1, *a2;

	a1 = "[-fv] [-a accounts] [-b branches]\n";
	a2 = "\t[-c cache_size] [-h home] [-S seed] [-s history] [-t tellers]";
	(void)fprintf(stderr, "usage: %s -i %s %s\n", progname, a1, a2);
	(void)fprintf(stderr,
	    "       %s -n transactions %s %s\n", progname, a1, a2);
	exit(1);
}
#endif

/*
 * db_init --
 *	Initialize the environment.
 */
DB_ENV *
db_init(home, prefix, cachesize, initializing, flags)
	char *home, *prefix;
	int cachesize, initializing, flags;
{
	DB_ENV *dbenv;
	u_int32_t local_flags;
	int ret;

	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_env_create");
		return (NULL);
	}
	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, prefix);
#ifdef HAVE_VXWORKS
	if ((ret = dbenv->set_shm_key(dbenv, VXSHM_KEY)) != 0) {
		dbenv->err(dbenv, ret, "set_shm_key");
		return (NULL);
	}
#endif
	(void)dbenv->set_cachesize(dbenv, 0,
	    cachesize == 0 ? 4 * 1024 * 1024 : (u_int32_t)cachesize, 0);

	local_flags = flags | DB_CREATE | (initializing ? DB_INIT_MPOOL :
	    DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL);
	if ((ret = dbenv->open(dbenv, home, local_flags, 0)) != 0) {
		dbenv->err(dbenv, ret, "DBENV->open: %s", home);
		(void)dbenv->close(dbenv, 0);
		return (NULL);
	}
	return (dbenv);
}

/*
 * Initialize the database to the specified number of accounts, branches,
 * history records, and tellers.
 */
int
tp_populate(env, accounts, branches, history, tellers, verbose)
	DB_ENV *env;
	int accounts, branches, history, tellers, verbose;
{
	DB *dbp;
	char dbname[100];
	u_int32_t balance, idnum, oflags;
	u_int32_t end_anum, end_bnum, end_tnum;
	u_int32_t start_anum, start_bnum, start_tnum;
	int ret;

	idnum = BEGID;
	balance = 500000;
#ifdef HAVE_VXWORKS
	oflags = DB_CREATE;
#else
	oflags = DB_CREATE | DB_TRUNCATE;
#endif

	if ((ret = db_create(&dbp, env, 0)) != 0) {
		env->err(env, ret, "db_create");
		return (ERROR_RETURN);
	}
	(void)dbp->set_h_nelem(dbp, (u_int32_t)accounts);

	snprintf(dbname, sizeof(dbname), "account");
	if ((ret = dbp->open(dbp, dbname, NULL,
	    DB_HASH, oflags, 0644)) != 0) {
		env->err(env, ret, "DB->open: account");
		return (ERROR_RETURN);
	}

	start_anum = idnum;
	populate(dbp, idnum, balance, accounts, "account");
	idnum += accounts;
	end_anum = idnum - 1;
	if ((ret = dbp->close(dbp, 0)) != 0) {
		env->err(env, ret, "DB->close: account");
		return (ERROR_RETURN);
	}
	if (verbose)
		printf("Populated accounts: %ld - %ld\n",
		    (long)start_anum, (long)end_anum);

	/*
	 * Since the number of branches is very small, we want to use very
	 * small pages and only 1 key per page, i.e., key-locking instead
	 * of page locking.
	 */
	if ((ret = db_create(&dbp, env, 0)) != 0) {
		env->err(env, ret, "db_create");
		return (ERROR_RETURN);
	}
	(void)dbp->set_h_ffactor(dbp, 1);
	(void)dbp->set_h_nelem(dbp, (u_int32_t)branches);
	(void)dbp->set_pagesize(dbp, 512);
	snprintf(dbname, sizeof(dbname), "branch");
	if ((ret = dbp->open(dbp, dbname, NULL,
	    DB_HASH, oflags, 0644)) != 0) {
		env->err(env, ret, "DB->open: branch");
		return (ERROR_RETURN);
	}
	start_bnum = idnum;
	populate(dbp, idnum, balance, branches, "branch");
	idnum += branches;
	end_bnum = idnum - 1;
	if ((ret = dbp->close(dbp, 0)) != 0) {
		env->err(env, ret, "DB->close: branch");
		return (ERROR_RETURN);
	}
	if (verbose)
		printf("Populated branches: %ld - %ld\n",
		    (long)start_bnum, (long)end_bnum);

	/*
	 * In the case of tellers, we also want small pages, but we'll let
	 * the fill factor dynamically adjust itself.
	 */
	if ((ret = db_create(&dbp, env, 0)) != 0) {
		env->err(env, ret, "db_create");
		return (ERROR_RETURN);
	}
	(void)dbp->set_h_ffactor(dbp, 0);
	(void)dbp->set_h_nelem(dbp, (u_int32_t)tellers);
	(void)dbp->set_pagesize(dbp, 512);
	snprintf(dbname, sizeof(dbname), "teller");
	if ((ret = dbp->open(dbp, dbname, NULL,
	    DB_HASH, oflags, 0644)) != 0) {
		env->err(env, ret, "DB->open: teller");
		return (ERROR_RETURN);
	}

	start_tnum = idnum;
	populate(dbp, idnum, balance, tellers, "teller");
	idnum += tellers;
	end_tnum = idnum - 1;
	if ((ret = dbp->close(dbp, 0)) != 0) {
		env->err(env, ret, "DB->close: teller");
		return (ERROR_RETURN);
	}
	if (verbose)
		printf("Populated tellers: %ld - %ld\n",
		    (long)start_tnum, (long)end_tnum);

	if ((ret = db_create(&dbp, env, 0)) != 0) {
		env->err(env, ret, "db_create");
		return (ERROR_RETURN);
	}
	(void)dbp->set_re_len(dbp, HISTORY_LEN);
	snprintf(dbname, sizeof(dbname), "history");
	if ((ret = dbp->open(dbp, dbname, NULL,
	    DB_RECNO, oflags, 0644)) != 0) {
		env->err(env, ret, "DB->open: history");
		return (ERROR_RETURN);
	}

	hpopulate(dbp, history, accounts, branches, tellers);
	if ((ret = dbp->close(dbp, 0)) != 0) {
		env->err(env, ret, "DB->close: history");
		return (ERROR_RETURN);
	}
	return (0);
}

int
populate(dbp, start_id, balance, nrecs, msg)
	DB *dbp;
	u_int32_t start_id, balance;
	int nrecs;
	char *msg;
{
	DBT kdbt, ddbt;
	defrec drec;
	int i, ret;

	kdbt.flags = 0;
	kdbt.data = &drec.id;
	kdbt.size = sizeof(u_int32_t);
	ddbt.flags = 0;
	ddbt.data = &drec;
	ddbt.size = sizeof(drec);
	memset(&drec.pad[0], 1, sizeof(drec.pad));

	for (i = 0; i < nrecs; i++) {
		drec.id = start_id + (u_int32_t)i;
		drec.balance = balance;
		if ((ret =
		    (dbp->put)(dbp, NULL, &kdbt, &ddbt, DB_NOOVERWRITE)) != 0) {
			dbp->err(dbp,
			    ret, "Failure initializing %s file\n", msg);
			return (ERROR_RETURN);
		}
	}
	return (0);
}

int
hpopulate(dbp, history, accounts, branches, tellers)
	DB *dbp;
	int history, accounts, branches, tellers;
{
	DBT kdbt, ddbt;
	histrec hrec;
	db_recno_t key;
	int i, ret;

	memset(&kdbt, 0, sizeof(kdbt));
	memset(&ddbt, 0, sizeof(ddbt));
	ddbt.data = &hrec;
	ddbt.size = sizeof(hrec);
	kdbt.data = &key;
	kdbt.size = sizeof(key);
	memset(&hrec.pad[0], 1, sizeof(hrec.pad));
	hrec.amount = 10;

	for (i = 1; i <= history; i++) {
		hrec.aid = random_id(ACCOUNT, accounts, branches, tellers);
		hrec.bid = random_id(BRANCH, accounts, branches, tellers);
		hrec.tid = random_id(TELLER, accounts, branches, tellers);
		if ((ret = dbp->put(dbp, NULL, &kdbt, &ddbt, DB_APPEND)) != 0) {
			dbp->err(dbp, ret, "dbp->put");
			return (ERROR_RETURN);
		}
	}
	return (0);
}

u_int32_t
random_int(lo, hi)
	u_int32_t lo, hi;
{
	u_int32_t ret;
	int t;

#ifndef RAND_MAX
#define	RAND_MAX	0x7fffffff
#endif
	t = rand();
	ret = (u_int32_t)(((double)t / ((double)(RAND_MAX) + 1)) *
	    (hi - lo + 1));
	ret += lo;
	return (ret);
}

u_int32_t
random_id(type, accounts, branches, tellers)
	FTYPE type;
	int accounts, branches, tellers;
{
	u_int32_t min, max, num;

	max = min = BEGID;
	num = accounts;
	switch(type) {
	case TELLER:
		min += branches;
		num = tellers;
		/* FALLTHROUGH */
	case BRANCH:
		if (type == BRANCH)
			num = branches;
		min += accounts;
		/* FALLTHROUGH */
	case ACCOUNT:
		max = min + num - 1;
	}
	return (random_int(min, max));
}

int
tp_run(dbenv, n, accounts, branches, tellers, verbose)
	DB_ENV *dbenv;
	int n, accounts, branches, tellers, verbose;
{
	DB *adb, *bdb, *hdb, *tdb;
	char dbname[100];
	double gtps, itps;
	int failed, ifailed, ret, txns;
	time_t starttime, curtime, lasttime;
#ifndef DB_WIN32
	pid_t pid;

	pid = getpid();
#else
	int pid;

	pid = 0;
#endif

	/*
	 * Open the database files.
	 */
	if ((ret = db_create(&adb, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		return (ERROR_RETURN);
	}
	snprintf(dbname, sizeof(dbname), "account");
	if ((ret = adb->open(adb, dbname, NULL, DB_UNKNOWN, 0, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->open: account");
		return (ERROR_RETURN);
	}

	if ((ret = db_create(&bdb, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		return (ERROR_RETURN);
	}
	snprintf(dbname, sizeof(dbname), "branch");
	if ((ret = bdb->open(bdb, dbname, NULL, DB_UNKNOWN, 0, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->open: branch");
		return (ERROR_RETURN);
	}

	if ((ret = db_create(&tdb, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		return (ERROR_RETURN);
	}
	snprintf(dbname, sizeof(dbname), "teller");
	if ((ret = tdb->open(tdb, dbname, NULL, DB_UNKNOWN, 0, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->open: teller");
		return (ERROR_RETURN);
	}

	if ((ret = db_create(&hdb, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		return (ERROR_RETURN);
	}
	snprintf(dbname, sizeof(dbname), "history");
	if ((ret = hdb->open(hdb, dbname, NULL, DB_UNKNOWN, 0, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->open: history");
		return (ERROR_RETURN);
	}

	txns = failed = ifailed = 0;
	starttime = time(NULL);
	lasttime = starttime;
	while (n-- > 0) {
		txns++;
		ret = tp_txn(dbenv, adb, bdb, tdb, hdb,
		    accounts, branches, tellers, verbose);
		if (ret != 0) {
			failed++;
			ifailed++;
		}
		if (n % 5000 == 0) {
			curtime = time(NULL);
			gtps = (double)(txns - failed) / (curtime - starttime);
			itps = (double)(5000 - ifailed) / (curtime - lasttime);
			printf("[%d] %d txns %d failed ", (int)pid,
			    txns, failed);
			printf("%6.2f TPS (gross) %6.2f TPS (interval)\n",
			   gtps, itps);
			lasttime = curtime;
			ifailed = 0;
		}
	}

	(void)adb->close(adb, 0);
	(void)bdb->close(bdb, 0);
	(void)tdb->close(tdb, 0);
	(void)hdb->close(hdb, 0);

	printf("%ld transactions begun %ld failed\n", (long)txns, (long)failed);
	return (0);
}

/*
 * XXX Figure out the appropriate way to pick out IDs.
 */
int
tp_txn(dbenv, adb, bdb, tdb, hdb, accounts, branches, tellers, verbose)
	DB_ENV *dbenv;
	DB *adb, *bdb, *tdb, *hdb;
	int accounts, branches, tellers, verbose;
{
	DBC *acurs, *bcurs, *tcurs;
	DBT d_dbt, d_histdbt, k_dbt, k_histdbt;
	DB_TXN *t;
	db_recno_t key;
	defrec rec;
	histrec hrec;
	int account, branch, teller;

	t = NULL;
	acurs = bcurs = tcurs = NULL;

	/*
	 * XXX We could move a lot of this into the driver to make this
	 * faster.
	 */
	account = random_id(ACCOUNT, accounts, branches, tellers);
	branch = random_id(BRANCH, accounts, branches, tellers);
	teller = random_id(TELLER, accounts, branches, tellers);

	memset(&d_histdbt, 0, sizeof(d_histdbt));

	memset(&k_histdbt, 0, sizeof(k_histdbt));
	k_histdbt.data = &key;
	k_histdbt.size = sizeof(key);

	memset(&k_dbt, 0, sizeof(k_dbt));
	k_dbt.size = sizeof(int);

	memset(&d_dbt, 0, sizeof(d_dbt));
	d_dbt.flags = DB_DBT_USERMEM;
	d_dbt.data = &rec;
	d_dbt.ulen = sizeof(rec);

	hrec.aid = account;
	hrec.bid = branch;
	hrec.tid = teller;
	hrec.amount = 10;
	/* Request 0 bytes since we're just positioning. */
	d_histdbt.flags = DB_DBT_PARTIAL;

	/* START TIMING */
	if (txn_begin(dbenv, NULL, &t, 0) != 0)
		goto err;

	if (adb->cursor(adb, t, &acurs, 0) != 0 ||
	    bdb->cursor(bdb, t, &bcurs, 0) != 0 ||
	    tdb->cursor(tdb, t, &tcurs, 0) != 0)
		goto err;

	/* Account record */
	k_dbt.data = &account;
	if (acurs->c_get(acurs, &k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (acurs->c_put(acurs, &k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	/* Branch record */
	k_dbt.data = &branch;
	if (bcurs->c_get(bcurs, &k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (bcurs->c_put(bcurs, &k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	/* Teller record */
	k_dbt.data = &teller;
	if (tcurs->c_get(tcurs, &k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (tcurs->c_put(tcurs, &k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	/* History record */
	d_histdbt.flags = 0;
	d_histdbt.data = &hrec;
	d_histdbt.ulen = sizeof(hrec);
	if (hdb->put(hdb, t, &k_histdbt, &d_histdbt, DB_APPEND) != 0)
		goto err;

	if (acurs->c_close(acurs) != 0 || bcurs->c_close(bcurs) != 0 ||
	    tcurs->c_close(tcurs) != 0)
		goto err;

	if (txn_commit(t, 0) != 0)
		goto err;

	/* END TIMING */
	return (0);

err:	if (acurs != NULL)
		(void)acurs->c_close(acurs);
	if (bcurs != NULL)
		(void)bcurs->c_close(bcurs);
	if (tcurs != NULL)
		(void)tcurs->c_close(tcurs);
	if (t != NULL)
		(void)txn_abort(t);

	if (verbose)
		printf("Transaction A=%ld B=%ld T=%ld failed\n",
		    (long)account, (long)branch, (long)teller);
	return (-1);
}
