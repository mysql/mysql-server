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
    "$Id: db_checkpoint.c,v 11.25 2001/01/18 18:36:57 bostic Exp $";
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
#include "db_page.h"
#include "btree.h"
#include "hash.h"
#include "qam.h"
#include "common_ext.h"
#include "clib_ext.h"

char	*check __P((DB_ENV *, long, long));
int	 main __P((int, char *[]));
void	 usage __P((void));
void	 version_check __P((void));

DB_ENV	*dbenv;
const char
	*progname = "db_checkpoint";		/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	time_t now;
	long argval;
	u_int32_t flags, kbytes, minutes, seconds;
	int ch, e_close, exitval, once, ret, verbose;
	char *home, *logfile;

	version_check();

	/*
	 * !!!
	 * Don't allow a fully unsigned 32-bit number, some compilers get
	 * upset and require it to be specified in hexadecimal and so on.
	 */
#define	MAX_UINT32_T	2147483647

	kbytes = minutes = 0;
	e_close = exitval = once = verbose = 0;
	flags = 0;
	home = logfile = NULL;
	while ((ch = getopt(argc, argv, "1h:k:L:p:Vv")) != EOF)
		switch (ch) {
		case '1':
			once = 1;
			flags = DB_FORCE;
			break;
		case 'h':
			home = optarg;
			break;
		case 'k':
			(void)__db_getlong(NULL, progname,
			    optarg, 1, (long)MAX_UINT32_T, &argval);
			kbytes = argval;
			break;
		case 'L':
			logfile = optarg;
			break;
		case 'p':
			(void)__db_getlong(NULL, progname,
			    optarg, 1, (long)MAX_UINT32_T, &argval);
			minutes = argval;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			exit(0);
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
			goto shutdown;
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (once == 0 && kbytes == 0 && minutes == 0) {
		(void)fprintf(stderr,
		    "%s: at least one of -1, -k and -p must be specified\n",
		    progname);
		exit (1);
	}

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

	/* Initialize the environment. */
	if ((ret = dbenv->open(dbenv,
	    home, DB_JOINENV | DB_USE_ENVIRON, 0)) != 0) {
		dbenv->err(dbenv, ret, "open");
		goto shutdown;
	}

	/* Register the standard pgin/pgout functions, in case we do I/O. */
	if ((ret =
	    memp_register(dbenv, DB_FTYPE_SET, __db_pgin, __db_pgout)) != 0) {
		dbenv->err(dbenv, ret,
		    "failed to register access method functions");
		goto shutdown;
	}

	/*
	 * If we have only a time delay, then we'll sleep the right amount
	 * to wake up when a checkpoint is necessary.  If we have a "kbytes"
	 * field set, then we'll check every 30 seconds.
	 */
	seconds = kbytes != 0 ? 30 : minutes * 60;
	while (!__db_util_interrupted()) {
		if (verbose) {
			(void)time(&now);
			dbenv->errx(dbenv, "checkpoint: %s", ctime(&now));
		}

		ret = txn_checkpoint(dbenv, kbytes, minutes, flags);
		while (ret == DB_INCOMPLETE) {
			if (verbose)
				dbenv->errx(dbenv,
				    "checkpoint did not finish, retrying\n");
			(void)__os_sleep(dbenv, 2, 0);
			ret = txn_checkpoint(dbenv, 0, 0, flags);
		}
		if (ret != 0) {
			dbenv->err(dbenv, ret, "txn_checkpoint");
			goto shutdown;
		}

		if (once)
			break;

		(void)__os_sleep(dbenv, seconds, 0);
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
    "usage: db_checkpoint [-1Vv] [-h home] [-k kbytes] [-L file] [-p min]\n");
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
