/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_rq_main.c,v 1.23 2002/08/06 05:39:03 bostic Exp $
 */

#include <sys/types.h>
#include <pthread.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

#include "ex_repquote.h"

/*
 * Process globals (we could put these in the machtab I suppose.
 */
int master_eid;
char *myaddr;

static int env_init __P((const char *, const char *, DB_ENV **, machtab_t *,
    u_int32_t));
static void usage __P((const char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV *dbenv;
	DBT local;
	enum { MASTER, CLIENT, UNKNOWN } whoami;
	all_args aa;
	connect_args ca;
	machtab_t *machtab;
	pthread_t all_thr, conn_thr;
	repsite_t site, *sitep, self, *selfp;
	struct sigaction sigact;
	int maxsites, nsites, ret, priority, totalsites;
	char *c, ch;
	const char *home, *progname;
	void *astatus, *cstatus;

	master_eid = DB_EID_INVALID;

	dbenv = NULL;
	whoami = UNKNOWN;
	machtab = NULL;
	selfp = sitep = NULL;
	maxsites = nsites = ret = totalsites = 0;
	priority = 100;
	home = "TESTDIR";
	progname = "ex_repquote";

	while ((ch = getopt(argc, argv, "Ch:Mm:n:o:p:")) != EOF)
		switch (ch) {
		case 'M':
			whoami = MASTER;
			master_eid = SELF_EID;
			break;
		case 'C':
			whoami = CLIENT;
			break;
		case 'h':
			home = optarg;
			break;
		case 'm':
			if ((myaddr = strdup(optarg)) == NULL) {
				fprintf(stderr,
				    "System error %s\n", strerror(errno));
				goto err;
			}
			self.host = optarg;
			self.host = strtok(self.host, ":");
			if ((c = strtok(NULL, ":")) == NULL) {
				fprintf(stderr, "Bad host specification.\n");
				goto err;
			}
			self.port = atoi(c);
			selfp = &self;
			break;
		case 'n':
			totalsites = atoi(optarg);
			break;
		case 'o':
			site.host = optarg;
			site.host = strtok(site.host, ":");
			if ((c = strtok(NULL, ":")) == NULL) {
				fprintf(stderr, "Bad host specification.\n");
				goto err;
			}
			site.port = atoi(c);
			if (sitep == NULL || nsites >= maxsites) {
				maxsites = maxsites == 0 ? 10 : 2 * maxsites;
				if ((sitep = realloc(sitep,
				    maxsites * sizeof(repsite_t))) == NULL) {
					fprintf(stderr, "System error %s\n",
					    strerror(errno));
					goto err;
				}
			}
			sitep[nsites++] = site;
			break;
		case 'p':
			priority = atoi(optarg);
			break;
		case '?':
		default:
			usage(progname);
		}

	/* Error check command line. */
	if (whoami == UNKNOWN) {
		fprintf(stderr, "Must specify -M or -C.\n");
		goto err;
	}

	if (selfp == NULL)
		usage(progname);

	if (home == NULL)
		usage(progname);

	/*
	 * Turn off SIGPIPE so that we don't kill processes when they
	 * happen to lose a connection at the wrong time.
	 */
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = SIG_IGN;
	if ((ret = sigaction(SIGPIPE, &sigact, NULL)) != 0) {
		fprintf(stderr,
		    "Unable to turn off SIGPIPE: %s\n", strerror(ret));
		goto err;
	}

	/*
	 * We are hardcoding priorities here that all clients have the
	 * same priority except for a designated master who gets a higher
	 * priority.
	 */
	if ((ret =
	    machtab_init(&machtab, priority, totalsites)) != 0)
		goto err;

	/*
	 * We can know open our environment, although we're not ready to
	 * begin replicating.  However, we want to have a dbenv around
	 * so that we can send it into any of our message handlers.
	 */
	if ((ret = env_init(progname, home, &dbenv, machtab, DB_RECOVER)) != 0)
		goto err;

	/*
	 * Now sets up comm infrastructure.  There are two phases.  First,
	 * we open our port for listening for incoming connections.  Then
	 * we attempt to connect to every host we know about.
	 */

	ca.dbenv = dbenv;
	ca.home = home;
	ca.progname = progname;
	ca.machtab = machtab;
	ca.port = selfp->port;
	if ((ret = pthread_create(&conn_thr, NULL, connect_thread, &ca)) != 0)
		goto err;

	aa.dbenv = dbenv;
	aa.progname = progname;
	aa.home = home;
	aa.machtab = machtab;
	aa.sites = sitep;
	aa.nsites = nsites;
	if ((ret = pthread_create(&all_thr, NULL, connect_all, &aa)) != 0)
		goto err;

	/*
	 * We have now got the entire communication infrastructure set up.
	 * It's time to declare ourselves to be a client or master.
	 */
	if (whoami == MASTER) {
		if ((ret = dbenv->rep_start(dbenv, NULL, DB_REP_MASTER)) != 0) {
			dbenv->err(dbenv, ret, "dbenv->rep_start failed");
			goto err;
		}
		if ((ret = domaster(dbenv, progname)) != 0) {
			dbenv->err(dbenv, ret, "Master failed");
			goto err;
		}
	} else {
		memset(&local, 0, sizeof(local));
		local.data = myaddr;
		local.size = strlen(myaddr) + 1;
		if ((ret =
		    dbenv->rep_start(dbenv, &local, DB_REP_CLIENT)) != 0) {
			dbenv->err(dbenv, ret, "dbenv->rep_start failed");
			goto err;
		}
		/* Sleep to give ourselves a minute to find a master. */
		sleep(5);
		if ((ret = doclient(dbenv, progname, machtab)) != 0) {
			dbenv->err(dbenv, ret, "Client failed");
			goto err;
		}

	}

	/* Wait on the connection threads. */
	if (pthread_join(all_thr, &astatus) || pthread_join(conn_thr, &cstatus))
		ret = errno;
	if (ret == 0 &&
	    ((int)astatus != EXIT_SUCCESS || (int)cstatus != EXIT_SUCCESS))
		ret = -1;

err:	if (machtab != NULL)
		free(machtab);
	if (dbenv != NULL)
		(void)dbenv->close(dbenv, 0);
	return (ret);
}

/*
 * In this application, we specify all communication via the command line.
 * In a real application, we would expect that information about the other
 * sites in the system would be maintained in some sort of configuration
 * file.  The critical part of this interface is that we assume at startup
 * that we can find out 1) what host/port we wish to listen on for connections,
 * 2) a (possibly empty) list of other sites we should attempt to connect to.
 * 3) whether we are a master or client (if we don't know, we should come up
 * as a client and see if there is a master out there) and 4) what our
 * Berkeley DB home environment is.
 *
 * These pieces of information are expressed by the following flags.
 * -m host:port (required; m stands for me)
 * -o host:port (optional; o stands for other; any number of these may be
 *	specified)
 * -[MC] M for master/C for client
 * -h home directory
 * -n nsites (optional; number of sites in replication group; defaults to 0
 *	in which case we try to dynamically compute the number of sites in
 *	the replication group.)
 * -p priority (optional: defaults to 100)
 */
static void
usage(progname)
	const char *progname;
{
	fprintf(stderr, "usage: %s ", progname);
	fprintf(stderr, "[-CM][-h home][-o host:port][-m host:port]%s",
	    "[-n nsites][-p priority]\n");
	exit(EXIT_FAILURE);
}

/* Open and configure an environment.  */
int
env_init(progname, home, dbenvp, machtab, flags)
	const char *progname, *home;
	DB_ENV **dbenvp;
	machtab_t *machtab;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret;
	char *prefix;

	if ((prefix = malloc(strlen(progname) + 2)) == NULL) {
		fprintf(stderr,
		    "%s: System error: %s\n", progname, strerror(errno));
		return (errno);
	}
	sprintf(prefix, "%s:", progname);

	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr, "%s: env create failed: %s\n",
		    progname, db_strerror(ret));
		return (ret);
	}
	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, prefix);
	/* (void)dbenv->set_verbose(dbenv, DB_VERB_REPLICATION, 1); */
	(void)dbenv->set_cachesize(dbenv, 0, CACHESIZE, 0);
	/* (void)dbenv->set_flags(dbenv, DB_TXN_NOSYNC, 1); */

	dbenv->app_private = machtab;
	(void)dbenv->set_rep_transport(dbenv, SELF_EID, quote_send);

	flags |= DB_CREATE | DB_THREAD |
	    DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN;

	ret = dbenv->open(dbenv, home, flags, 0);

	*dbenvp = dbenv;
	return (ret);
}
