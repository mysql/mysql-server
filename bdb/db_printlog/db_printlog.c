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
    "$Id: db_printlog.c,v 11.23 2001/01/18 18:36:58 bostic Exp $";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"
#include "db_am.h"
#include "hash.h"
#include "log.h"
#include "qam.h"
#include "txn.h"

int	main __P((int, char *[]));
void	usage __P((void));
void	version_check __P((void));

DB_ENV	*dbenv;
const char
	*progname = "db_printlog";			/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DBT data;
	DB_LSN key;
	int ch, e_close, exitval, nflag, ret;
	char *home;

	version_check();

	e_close = exitval = 0;
	nflag = 0;
	home = NULL;
	while ((ch = getopt(argc, argv, "h:NV")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
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
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			exit(0);
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

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

	/*
	 * An environment is required, but as all we're doing is reading log
	 * files, we create one if it doesn't already exist.  If we create
	 * it, create it private so it automatically goes away when we're done.
	 */
	if ((ret = dbenv->open(dbenv, home,
	    DB_JOINENV | DB_USE_ENVIRON, 0)) != 0 &&
	    (ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_LOG | DB_PRIVATE | DB_USE_ENVIRON, 0)) != 0) {
		dbenv->err(dbenv, ret, "open");
		goto shutdown;
	}

	/* Initialize print callbacks. */
	if ((ret = __bam_init_print(dbenv)) != 0 ||
	    (ret = __crdel_init_print(dbenv)) != 0 ||
	    (ret = __db_init_print(dbenv)) != 0 ||
	    (ret = __qam_init_print(dbenv)) != 0 ||
	    (ret = __ham_init_print(dbenv)) != 0 ||
	    (ret = __log_init_print(dbenv)) != 0 ||
	    (ret = __txn_init_print(dbenv)) != 0) {
		dbenv->err(dbenv, ret, "callback: initialization");
		goto shutdown;
	}

	memset(&data, 0, sizeof(data));
	while (!__db_util_interrupted()) {
		if ((ret = log_get(dbenv, &key, &data, DB_NEXT)) != 0) {
			if (ret == DB_NOTFOUND)
				break;
			dbenv->err(dbenv, ret, "log_get");
			goto shutdown;
		}

		/*
		 * XXX
		 * We use DB_TXN_ABORT as our op because that's the only op
		 * that calls the underlying recovery function without any
		 * consideration as to the contents of the transaction list.
		 */
		ret = __db_dispatch(dbenv, &data, &key, DB_TXN_ABORT, NULL);

		/*
		 * XXX
		 * Just in case the underlying routines don't flush.
		 */
		(void)fflush(stdout);

		if (ret != 0) {
			dbenv->err(dbenv, ret, "tx: dispatch");
			goto shutdown;
		}
	}

	if (0) {
shutdown:	exitval = 1;
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

void
usage()
{
	fprintf(stderr, "usage: db_printlog [-NV] [-h home]\n");
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
