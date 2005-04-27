/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2002\nSleepycat Software Inc.  All rights reserved.\n";
static const char revid[] =
    "$Id: db_deadlock.c,v 11.38 2002/08/08 03:50:32 bostic Exp $";
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
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

int main __P((int, char *[]));
int usage __P((void));
int version_check __P((const char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	const char *progname = "db_deadlock";
	DB_ENV  *dbenv;
	u_int32_t atype;
	time_t now;
	long secs, usecs;
	int ch, e_close, exitval, ret, verbose;
	char *home, *logfile, *str;

	if ((ret = version_check(progname)) != 0)
		return (ret);

	atype = DB_LOCK_DEFAULT;
	home = logfile = NULL;
	secs = usecs = 0;
	e_close = exitval = verbose = 0;
	while ((ch = getopt(argc, argv, "a:h:L:t:Vvw")) != EOF)
		switch (ch) {
		case 'a':
			switch (optarg[0]) {
			case 'e':
				atype = DB_LOCK_EXPIRE;
				break;
			case 'm':
				atype = DB_LOCK_MAXLOCKS;
				break;
			case 'n':
				atype = DB_LOCK_MINLOCKS;
				break;
			case 'o':
				atype = DB_LOCK_OLDEST;
				break;
			case 'w':
				atype = DB_LOCK_MINWRITE;
				break;
			case 'y':
				atype = DB_LOCK_YOUNGEST;
				break;
			default:
				return (usage());
				/* NOTREACHED */
			}
			if (optarg[1] != '\0')
				return (usage());
			break;
		case 'h':
			home = optarg;
			break;
		case 'L':
			logfile = optarg;
			break;
		case 't':
			if ((str = strchr(optarg, '.')) != NULL) {
				*str++ = '\0';
				if (*str != '\0' && __db_getlong(
				    NULL, progname, str, 0, LONG_MAX, &usecs))
					return (EXIT_FAILURE);
			}
			if (*optarg != '\0' && __db_getlong(
			    NULL, progname, optarg, 0, LONG_MAX, &secs))
				return (EXIT_FAILURE);
			if (secs == 0 && usecs == 0)
				return (usage());

			break;

		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case 'v':
			verbose = 1;
			break;
		case 'w':			/* Undocumented. */
			/* Detect every 100ms (100000 us) when polling. */
			secs = 0;
			usecs = 100000;
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		return (usage());

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

		if ((ret = dbenv->lock_detect(dbenv, 0, atype, NULL)) != 0) {
			dbenv->err(dbenv, ret, "DB_ENV->lock_detect");
			goto shutdown;
		}

		/* Make a pass every "secs" secs and "usecs" usecs. */
		if (secs == 0 && usecs == 0)
			break;
		(void)__os_sleep(dbenv, secs, usecs);
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

	return (exitval == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

int
usage()
{
	(void)fprintf(stderr, "%s\n\t%s\n",
	    "usage: db_deadlock [-Vv]",
	    "[-a e | m | n | o | w | y] [-h home] [-L file] [-t sec.usec]");
	return (EXIT_FAILURE);
}

int
version_check(progname)
	const char *progname;
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
		return (EXIT_FAILURE);
	}
	return (0);
}
