/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TpcbExample.cpp,v 11.14 2000/10/27 20:32:01 dda Exp $
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

#include <iostream.h>
#include <iomanip.h>
#include <db_cxx.h>

typedef enum { ACCOUNT, BRANCH, TELLER } FTYPE;

void errExit(int err, const char *);  // show err as errno and exit

void	  invarg(int, char *);
u_int32_t random_id(FTYPE, u_int32_t, u_int32_t, u_int32_t);
u_int32_t random_int(u_int32_t, u_int32_t);
static void	  usage(void);

int verbose;
char *progname = "TpcbExample";                            // Program name.

class TpcbExample : public DbEnv
{
public:
	void populate(int, int, int, int);
	void run(int, int, int, int);
	int txn(Db *, Db *, Db *, Db *,
		int, int, int);
	void populateHistory(Db *, int, u_int32_t, u_int32_t, u_int32_t);
	void populateTable(Db *, u_int32_t, u_int32_t, int, char *);

	// Note: the constructor creates a DbEnv(), which is
	// not fully initialized until the DbEnv::open() method
	// is called.
	//
	TpcbExample(const char *home, int cachesize,
		    int initializing, int flags);

private:
	static const char FileName[];

	// no need for copy and assignment
	TpcbExample(const TpcbExample &);
	void operator = (const TpcbExample &);
};

//
// This program implements a basic TPC/B driver program.  To create the
// TPC/B database, run with the -i (init) flag.  The number of records
// with which to populate the account, history, branch, and teller tables
// is specified by the a, s, b, and t flags respectively.  To run a TPC/B
// test, use the n flag to indicate a number of transactions to run (note
// that you can run many of these processes in parallel to simulate a
// multiuser test run).
//
#define	TELLERS_PER_BRANCH      100
#define	ACCOUNTS_PER_TELLER     1000
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

#if !defined(VALID_SCALING) && !defined(TINY)
#define	ACCOUNTS	  100000
#define	BRANCHES	      10
#define	TELLERS		     100
#define	HISTORY		  259200
#endif

#define	HISTORY_LEN	    100
#define	RECLEN		    100
#define	BEGID		1000000

struct Defrec {
	u_int32_t   id;
	u_int32_t   balance;
	u_int8_t    pad[RECLEN - sizeof(u_int32_t) - sizeof(u_int32_t)];
};

struct Histrec {
	u_int32_t   aid;
	u_int32_t   bid;
	u_int32_t   tid;
	u_int32_t   amount;
	u_int8_t    pad[RECLEN - 4 * sizeof(u_int32_t)];
};

int
main(int argc, char *argv[])
{
	unsigned long seed;
	int accounts, branches, tellers, history;
	int iflag, mpool, ntxns, txn_no_sync;
	char *home, *endarg;

	home = "TESTDIR";
	accounts = branches = history = tellers = 0;
	txn_no_sync = 0;
	mpool = ntxns = 0;
	verbose = 0;
	iflag = 0;
	seed = (unsigned long)getpid();

	for (int i = 1; i < argc; ++i) {

		if (strcmp(argv[i], "-a") == 0) {
			// Number of account records
			if ((accounts = atoi(argv[++i])) <= 0)
				invarg('a', argv[i]);
		}
		else if (strcmp(argv[i], "-b") == 0) {
			// Number of branch records
			if ((branches = atoi(argv[++i])) <= 0)
				invarg('b', argv[i]);
		}
		else if (strcmp(argv[i], "-c") == 0) {
			// Cachesize in bytes
			if ((mpool = atoi(argv[++i])) <= 0)
				invarg('c', argv[i]);
		}
		else if (strcmp(argv[i], "-f") == 0) {
			// Fast mode: no txn sync.
			txn_no_sync = 1;
		}
		else if (strcmp(argv[i], "-h") == 0) {
			// DB  home.
			home = argv[++i];
		}
		else if (strcmp(argv[i], "-i") == 0) {
			// Initialize the test.
			iflag = 1;
		}
		else if (strcmp(argv[i], "-n") == 0) {
			// Number of transactions
			if ((ntxns = atoi(argv[++i])) <= 0)
				invarg('n', argv[i]);
		}
		else if (strcmp(argv[i], "-S") == 0) {
			// Random number seed.
			seed = strtoul(argv[++i], &endarg, 0);
			if (*endarg != '\0')
				invarg('S', argv[i]);
		}
		else if (strcmp(argv[i], "-s") == 0) {
			// Number of history records
			if ((history = atoi(argv[++i])) <= 0)
				invarg('s', argv[i]);
		}
		else if (strcmp(argv[i], "-t") == 0) {
			// Number of teller records
			if ((tellers = atoi(argv[++i])) <= 0)
				invarg('t', argv[i]);
		}
		else if (strcmp(argv[i], "-v") == 0) {
			// Verbose option.
			verbose = 1;
		}
		else {
			usage();
		}
	}

	srand((unsigned int)seed);

	accounts = accounts == 0 ? ACCOUNTS : accounts;
	branches = branches == 0 ? BRANCHES : branches;
	tellers = tellers == 0 ? TELLERS : tellers;
	history = history == 0 ? HISTORY : history;

	if (verbose)
		cout << (long)accounts << " Accounts "
		     << (long)branches << " Branches "
		     << (long)tellers << " Tellers "
		     << (long)history << " History\n";

	try {
		// Initialize the database environment.
		// Must be done in within a try block, unless you
		// change the error model in the environment options.
		//
		TpcbExample app(home, mpool, iflag, txn_no_sync ? DB_TXN_NOSYNC : 0);

		if (iflag) {
			if (ntxns != 0)
				usage();
			app.populate(accounts, branches, history, tellers);
		}
		else {
			if (ntxns == 0)
				usage();
			app.run(ntxns, accounts, branches, tellers);
		}

		app.close(0);
		return 0;
	}
	catch (DbException &dbe) {
		cerr << "TpcbExample: " << dbe.what() << "\n";
		return 1;
	}
}

void
invarg(int arg, char *str)
{
	cerr << "TpcbExample: invalid argument for -"
	     << (char)arg << ": " << str << "\n";
	exit(1);
}

static void
usage()
{
	cerr << "usage: TpcbExample [-fiv] [-a accounts] [-b branches]\n"
	     << "                   [-c cachesize] [-h home] [-n transactions ]\n"
	     << "                   [-S seed] [-s history] [-t tellers]\n";
	exit(1);
}

TpcbExample::TpcbExample(const char *home, int cachesize,
			 int initializing, int flags)
:	DbEnv(0)
{
	u_int32_t local_flags;

	set_error_stream(&cerr);
	set_errpfx("TpcbExample");
	(void)set_cachesize(0, cachesize == 0 ?
			    4 * 1024 * 1024 : (u_int32_t)cachesize, 0);

	local_flags = flags | DB_CREATE | DB_INIT_MPOOL;
	if (!initializing)
		local_flags |= DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
	open(home, local_flags, 0);
}

//
// Initialize the database to the specified number of accounts, branches,
// history records, and tellers.
//
void
TpcbExample::populate(int accounts, int branches, int history, int tellers)
{
	Db *dbp;

	int err;
	u_int32_t balance, idnum;
	u_int32_t end_anum, end_bnum, end_tnum;
	u_int32_t start_anum, start_bnum, start_tnum;

	idnum = BEGID;
	balance = 500000;

	dbp = new Db(this, 0);
	dbp->set_h_nelem((unsigned int)accounts);

	if ((err = dbp->open("account", NULL, DB_HASH,
			     DB_CREATE | DB_TRUNCATE, 0644)) != 0) {
		errExit(err, "Open of account file failed");
	}

	start_anum = idnum;
	populateTable(dbp, idnum, balance, accounts, "account");
	idnum += accounts;
	end_anum = idnum - 1;
	if ((err = dbp->close(0)) != 0) {
		errExit(err, "Account file close failed");
	}
	delete dbp;
	if (verbose)
		cout << "Populated accounts: "
		     << (long)start_anum << " - " << (long)end_anum << "\n";

	dbp = new Db(this, 0);
	//
	// Since the number of branches is very small, we want to use very
	// small pages and only 1 key per page.  This is the poor-man's way
	// of getting key locking instead of page locking.
	//
	dbp->set_h_ffactor(1);
	dbp->set_h_nelem((unsigned int)branches);
	dbp->set_pagesize(512);

	if ((err = dbp->open("branch", NULL, DB_HASH,
			     DB_CREATE | DB_TRUNCATE, 0644)) != 0) {
		errExit(err, "Branch file create failed");
	}
	start_bnum = idnum;
	populateTable(dbp, idnum, balance, branches, "branch");
	idnum += branches;
	end_bnum = idnum - 1;
	if ((err = dbp->close(0)) != 0) {
		errExit(err, "Close of branch file failed");
	}
	delete dbp;

	if (verbose)
		cout << "Populated branches: "
		     << (long)start_bnum << " - " << (long)end_bnum << "\n";

	dbp = new Db(this, 0);
	//
	// In the case of tellers, we also want small pages, but we'll let
	// the fill factor dynamically adjust itself.
	//
	dbp->set_h_ffactor(0);
	dbp->set_h_nelem((unsigned int)tellers);
	dbp->set_pagesize(512);

	if ((err = dbp->open("teller", NULL, DB_HASH,
			     DB_CREATE | DB_TRUNCATE, 0644)) != 0) {
		errExit(err, "Teller file create failed");
	}

	start_tnum = idnum;
	populateTable(dbp, idnum, balance, tellers, "teller");
	idnum += tellers;
	end_tnum = idnum - 1;
	if ((err = dbp->close(0)) != 0) {
		errExit(err, "Close of teller file failed");
	}
	delete dbp;
	if (verbose)
		cout << "Populated tellers: "
		     << (long)start_tnum << " - " << (long)end_tnum << "\n";

	dbp = new Db(this, 0);
	dbp->set_re_len(HISTORY_LEN);
	if ((err = dbp->open("history", NULL, DB_RECNO,
			     DB_CREATE | DB_TRUNCATE, 0644)) != 0) {
		errExit(err, "Create of history file failed");
	}

	populateHistory(dbp, history, accounts, branches, tellers);
	if ((err = dbp->close(0)) != 0) {
		errExit(err, "Close of history file failed");
	}
	delete dbp;
}

void
TpcbExample::populateTable(Db *dbp,
			   u_int32_t start_id, u_int32_t balance,
			   int nrecs, char *msg)
{
	Defrec drec;
	memset(&drec.pad[0], 1, sizeof(drec.pad));

	Dbt kdbt(&drec.id, sizeof(u_int32_t));
	Dbt ddbt(&drec, sizeof(drec));

	for (int i = 0; i < nrecs; i++) {
		drec.id = start_id + (u_int32_t)i;
		drec.balance = balance;
		int err;
		if ((err =
		     dbp->put(NULL, &kdbt, &ddbt, DB_NOOVERWRITE)) != 0) {
			cerr << "Failure initializing " << msg << " file: "
			     << strerror(err) << "\n";
			exit(1);
		}
	}
}

void
TpcbExample::populateHistory(Db *dbp, int nrecs,
		     u_int32_t accounts, u_int32_t branches, u_int32_t tellers)
{
	Histrec hrec;
	memset(&hrec.pad[0], 1, sizeof(hrec.pad));
	hrec.amount = 10;
	db_recno_t key;

	Dbt kdbt(&key, sizeof(u_int32_t));
	Dbt ddbt(&hrec, sizeof(hrec));

	for (int i = 1; i <= nrecs; i++) {
		hrec.aid = random_id(ACCOUNT, accounts, branches, tellers);
		hrec.bid = random_id(BRANCH, accounts, branches, tellers);
		hrec.tid = random_id(TELLER, accounts, branches, tellers);

		int err;
		key = (db_recno_t)i;
		if ((err = dbp->put(NULL, &kdbt, &ddbt, DB_APPEND)) != 0) {
			errExit(err, "Failure initializing history file");
		}
	}
}

u_int32_t
random_int(u_int32_t lo, u_int32_t hi)
{
	u_int32_t ret;
	int t;

	t = rand();
	ret = (u_int32_t)(((double)t / ((double)(RAND_MAX) + 1)) *
			  (hi - lo + 1));
	ret += lo;
	return (ret);
}

u_int32_t
random_id(FTYPE type, u_int32_t accounts, u_int32_t branches, u_int32_t tellers)
{
	u_int32_t min, max, num;

	max = min = BEGID;
	num = accounts;
	switch(type) {
	case TELLER:
		min += branches;
		num = tellers;
		// Fallthrough
	case BRANCH:
		if (type == BRANCH)
			num = branches;
		min += accounts;
		// Fallthrough
	case ACCOUNT:
		max = min + num - 1;
	}
	return (random_int(min, max));
}

void
TpcbExample::run(int n, int accounts, int branches, int tellers)
{
	Db *adb, *bdb, *hdb, *tdb;
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

	//
	// Open the database files.
	//

	int err;
	adb = new Db(this, 0);
	if ((err = adb->open("account", NULL, DB_UNKNOWN, 0, 0)) != 0)
		errExit(err, "Open of account file failed");

	bdb = new Db(this, 0);
	if ((err = bdb->open("branch", NULL, DB_UNKNOWN, 0, 0)) != 0)
		errExit(err, "Open of branch file failed");

	tdb = new Db(this, 0);
	if ((err = tdb->open("teller", NULL, DB_UNKNOWN, 0, 0)) != 0)
		errExit(err, "Open of teller file failed");

	hdb = new Db(this, 0);
	if ((err = hdb->open("history", NULL, DB_UNKNOWN, 0, 0)) != 0)
		errExit(err, "Open of history file failed");

	txns = failed = ifailed = 0;
	starttime = time(NULL);
	lasttime = starttime;
	while (n-- > 0) {
		txns++;
		ret = txn(adb, bdb, tdb, hdb, accounts, branches, tellers);
		if (ret != 0) {
			failed++;
			ifailed++;
		}
		if (n % 5000 == 0) {
			curtime = time(NULL);
			gtps = (double)(txns - failed) / (curtime - starttime);
			itps = (double)(5000 - ifailed) / (curtime - lasttime);

			// We use printf because it provides much simpler
			// formatting than iostreams.
			//
			printf("[%d] %d txns %d failed ", (int)pid,
			    txns, failed);
			printf("%6.2f TPS (gross) %6.2f TPS (interval)\n",
			   gtps, itps);
			lasttime = curtime;
			ifailed = 0;
		}
	}

	(void)adb->close(0);
	(void)bdb->close(0);
	(void)tdb->close(0);
	(void)hdb->close(0);

	cout << (long)txns << " transactions begun "
	     << (long)failed << " failed\n";
}

//
// XXX Figure out the appropriate way to pick out IDs.
//
int
TpcbExample::txn(Db *adb, Db *bdb, Db *tdb, Db *hdb,
		 int accounts, int branches, int tellers)
{
	Dbc *acurs = NULL;
	Dbc *bcurs = NULL;
	Dbc *tcurs = NULL;
	DbTxn *t = NULL;

	db_recno_t key;
	Defrec rec;
	Histrec hrec;
	int account, branch, teller;

	Dbt d_dbt;
	Dbt d_histdbt;
	Dbt k_dbt;
	Dbt k_histdbt(&key, sizeof(key));

	//
	// XXX We could move a lot of this into the driver to make this
	// faster.
	//
	account = random_id(ACCOUNT, accounts, branches, tellers);
	branch = random_id(BRANCH, accounts, branches, tellers);
	teller = random_id(TELLER, accounts, branches, tellers);

	k_dbt.set_size(sizeof(int));

	d_dbt.set_flags(DB_DBT_USERMEM);
	d_dbt.set_data(&rec);
	d_dbt.set_ulen(sizeof(rec));

	hrec.aid = account;
	hrec.bid = branch;
	hrec.tid = teller;
	hrec.amount = 10;
	// Request 0 bytes since we're just positioning.
	d_histdbt.set_flags(DB_DBT_PARTIAL);

	// START TIMING
	if (txn_begin(NULL, &t, 0) != 0)
		goto err;

	if (adb->cursor(t, &acurs, 0) != 0 ||
	    bdb->cursor(t, &bcurs, 0) != 0 ||
	    tdb->cursor(t, &tcurs, 0) != 0)
		goto err;

	// Account record
	k_dbt.set_data(&account);
	if (acurs->get(&k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (acurs->put(&k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	// Branch record
	k_dbt.set_data(&branch);
	if (bcurs->get(&k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (bcurs->put(&k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	// Teller record
	k_dbt.set_data(&teller);
	if (tcurs->get(&k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (tcurs->put(&k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	// History record
	d_histdbt.set_flags(0);
	d_histdbt.set_data(&hrec);
	d_histdbt.set_ulen(sizeof(hrec));
	if (hdb->put(t, &k_histdbt, &d_histdbt, DB_APPEND) != 0)
		goto err;

	if (acurs->close() != 0 || bcurs->close() != 0 ||
	    tcurs->close() != 0)
		goto err;

	if (t->commit(0) != 0)
		goto err;

	// END TIMING
	return (0);

err:
	if (acurs != NULL)
		(void)acurs->close();
	if (bcurs != NULL)
		(void)bcurs->close();
	if (tcurs != NULL)
		(void)tcurs->close();
	if (t != NULL)
		(void)t->abort();

	if (verbose)
		cout << "Transaction A=" << (long)account
		     << " B=" << (long)branch
		     << " T=" << (long)teller << " failed\n";
	return (-1);
}

void errExit(int err, const char *s)
{
	cerr << progname << ": ";
	if (s != NULL) {
		cerr << s << ": ";
	}
	cerr << strerror(err) << "\n";
	exit(1);
}
