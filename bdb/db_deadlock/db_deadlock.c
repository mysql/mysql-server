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
    "$Id: db_deadlock.c,v 11.19 2001/01/18 18:36:57 bostic Exp $";
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "clib_ext.h"

int	 main __P((int, char *[]));
void	 usage __P((void));
void	 version_check __P((void));

DB_ENV  *dbenv;
const char
	*progname = "db_deadlock";			/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	u_int32_t atype;
	time_t now;
	long usecs;
	u_int32_t flags;
	int ch, e_close, exitval, ret, verbose;
	char *home, *logfile;

	version_check();

	atype = DB_LOCK_DEFAULT;
	home = logfile = NULL;
	usecs = 0;
	flags = 0;
	e_close = exitval = verbose = 0;
	while ((ch = getopt(argc, argv, "a:h:L:t:Vvw")) != EOF)
		switch (ch) {
		case 'a':
			switch (optarg[0]) {
			case 'o':
				atype = DB_LOCK_OLDEST;
				break;
			case 'y':
				atype = DB_LOCK_YOUNGEST;
				break;
			default:
				usage();
				/* NOTREACHED */
			}
			if (optarg[1] != '\0')
				usage();
			break;
		case 'h':
			home = optarg;
			break;
		case 'L':
			logfile = optarg;
			break;
		case 't':
			(void)__db_getlong(NULL,
			    progname, optarg, 1, LONG_MAX, &usecs);
			usecs *= 1000000;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			exit(0);
		case 'v':
			verbose = 1;
			break;
		case 'w':
			LF_SET(DB_LOCK_CONFLICT);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (usecs == 0 && !LF_ISSET(DB_LOCK_CONFLICT)) {
		fprintf(stderr,
		    "%s: at least one of -t and -w must be specified\n",
		    progname);
		exit(1);
	}

	/*
	 * We detect every 100ms (100000 us) when we're running in
	 * DB_LOCK_CONFLICT mode.
	 */
	if (usecs == 0)
		usecs = 100000;

	/* Handle possible interruptions. */
	__db_util_siginit();

	/* Log our process ID. */
	if (logfile != NULL && __db_util_logset(progname, logfile))
		goto shutdown;

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

	if (verbose) {
		(void)dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK, 1);
		(void)dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR, 1);
	}

	/* An environment is required. */
	if ((ret = dbenv->open(dbenv, home,
	    DB_JOINENV | DB_USE_ENVIRON, 0)) != 0) {
		dbenv->err(dbenv, ret, "open");
		goto shutdown;
	}

	while (!__db_util_interrupted()) {
		if (verbose) {
			(void)time(&now);
			dbenv->errx(dbenv, "running at %.24s", ctime(&now));
		}

		if ((ret = lock_detect(dbenv, flags, atype, NULL)) != 0) {
			dbenv->err(dbenv, ret, "lock_detect");
			goto shutdown;
		}

		/* Make a pass every "usecs" usecs. */
		(void)__os_sleep(dbenv, 0, usecs);
	}

	if (0) {
shutdown:	exitval = 1;
	}

	/* Clean up the logfile. */
	if (logfile != NULL)
		remove(logfile);

	/* Clean up the environment. */
	if (e_close && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	/* Resend any caught signal. */
	__db_util_sigresend();

	return (exitval);
}

void
usage()
{
	(void)fprintf(stderr,
    "usage: db_deadlock [-Vvw] [-a o | y] [-h home] [-L file] [-t sec]\n");
	exit(1);
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
