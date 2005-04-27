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
    "$Id: db_verify.c,v 1.38 2002/08/08 03:51:38 bostic Exp $";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

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
	const char *progname = "db_verify";
	DB *dbp, *dbp1;
	DB_ENV *dbenv;
	u_int32_t cache;
	int ch, d_close, e_close, exitval, nflag, oflag, private;
	int quiet, resize, ret, t_ret;
	char *home, *passwd;

	if ((ret = version_check(progname)) != 0)
		return (ret);

	dbenv = NULL;
	cache = MEGABYTE;
	d_close = e_close = exitval = nflag = oflag = quiet = 0;
	home = passwd = NULL;
	while ((ch = getopt(argc, argv, "h:NoP:qV")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
			break;
		case 'N':
			nflag = 1;
			break;
		case 'P':
			passwd = strdup(optarg);
			memset(optarg, 0, strlen(optarg));
			if (passwd == NULL) {
				fprintf(stderr, "%s: strdup: %s\n",
				    progname, strerror(errno));
				return (EXIT_FAILURE);
			}
			break;
		case 'o':
			oflag = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	if (argc <= 0)
		return (usage());

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
retry:	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		goto shutdown;
	}
	e_close = 1;

	if (!quiet) {
		dbenv->set_errfile(dbenv, stderr);
		dbenv->set_errpfx(dbenv, progname);
	}

	if (nflag) {
		if ((ret = dbenv->set_flags(dbenv, DB_NOLOCKING, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOLOCKING");
			goto shutdown;
		}
		if ((ret = dbenv->set_flags(dbenv, DB_NOPANIC, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOPANIC");
			goto shutdown;
		}
	}

	if (passwd != NULL &&
	    (ret = dbenv->set_encrypt(dbenv, passwd, DB_ENCRYPT_AES)) != 0) {
		dbenv->err(dbenv, ret, "set_passwd");
		goto shutdown;
	}
	/*
	 * Attach to an mpool if it exists, but if that fails, attach to a
	 * private region.  In the latter case, declare a reasonably large
	 * cache so that we don't fail when verifying large databases.
	 */
	private = 0;
	if ((ret =
	    dbenv->open(dbenv, home, DB_INIT_MPOOL | DB_USE_ENVIRON, 0)) != 0) {
		if ((ret = dbenv->set_cachesize(dbenv, 0, cache, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_cachesize");
			goto shutdown;
		}
		private = 1;
		if ((ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON, 0)) != 0) {
			dbenv->err(dbenv, ret, "open");
			goto shutdown;
		}
	}

	for (; !__db_util_interrupted() && argv[0] != NULL; ++argv) {
		if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
			dbenv->err(dbenv, ret, "%s: db_create", progname);
			goto shutdown;
		}
		d_close = 1;

		/*
		 * We create a 2nd dbp to this database to get its pagesize
		 * because the dbp we're using for verify cannot be opened.
		 */
		if (private) {
			if ((ret = db_create(&dbp1, dbenv, 0)) != 0) {
				dbenv->err(
				    dbenv, ret, "%s: db_create", progname);
				goto shutdown;
			}

			if ((ret = dbp1->open(dbp1, NULL,
			    argv[0], NULL, DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
				dbenv->err(dbenv, ret, "DB->open: %s", argv[0]);
				(void)dbp1->close(dbp1, 0);
				goto shutdown;
			}
			/*
			 * If we get here, we can check the cache/page.
			 * !!!
			 * If we have to retry with an env with a larger
			 * cache, we jump out of this loop.  However, we
			 * will still be working on the same argv when we
			 * get back into the for-loop.
			 */
			ret = __db_util_cache(dbenv, dbp1, &cache, &resize);
			(void)dbp1->close(dbp1, 0);
			if (ret != 0)
				goto shutdown;

			if (resize) {
				(void)dbp->close(dbp, 0);
				d_close = 0;

				(void)dbenv->close(dbenv, 0);
				e_close = 0;
				goto retry;
			}
		}
		if ((ret = dbp->verify(dbp,
		    argv[0], NULL, NULL, oflag ? DB_NOORDERCHK : 0)) != 0)
			dbp->err(dbp, ret, "DB->verify: %s", argv[0]);
		if ((t_ret = dbp->close(dbp, 0)) != 0 && ret == 0) {
			dbenv->err(dbenv, ret, "DB->close: %s", argv[0]);
			ret = t_ret;
		}
		d_close = 0;
		if (ret != 0)
			goto shutdown;
	}

	if (0) {
shutdown:	exitval = 1;
	}

	if (d_close && (ret = dbp->close(dbp, 0)) != 0) {
		exitval = 1;
		dbenv->err(dbenv, ret, "close");
	}
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
	fprintf(stderr, "%s\n",
	    "usage: db_verify [-NoqV] [-h home] [-P password] db_file ...");
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
