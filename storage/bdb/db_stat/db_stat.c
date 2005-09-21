/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_stat.c,v 11.158 2004/07/15 18:26:48 ubell Exp $
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2004\nSleepycat Software Inc.  All rights reserved.\n";
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
#include "dbinc/db_page.h"
#include "dbinc/txn.h"

typedef enum { T_NOTSET,
    T_DB, T_ENV, T_LOCK, T_LOG, T_MPOOL, T_REP, T_TXN } test_t;

int	 db_init __P((DB_ENV *, char *, test_t, u_int32_t, int *));
int	 main __P((int, char *[]));
int	 usage __P((void));
int	 version_check __P((const char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	const char *progname = "db_stat";
	DB_ENV	*dbenv;
	DB_BTREE_STAT *sp;
	DB *alt_dbp, *dbp;
	test_t ttype;
	u_int32_t cache, env_flags, fast, flags;
	int ch, exitval;
	int nflag, private, resize, ret;
	char *db, *home, *p, *passwd, *subdb;

	if ((ret = version_check(progname)) != 0)
		return (ret);

	dbenv = NULL;
	dbp = NULL;
	ttype = T_NOTSET;
	cache = MEGABYTE;
	exitval = fast = flags = nflag = private = 0;
	db = home = passwd = subdb = NULL;
	env_flags = 0;

	while ((ch = getopt(argc, argv, "C:cd:Eefh:L:lM:mNP:R:rs:tVZ")) != EOF)
		switch (ch) {
		case 'C': case 'c':
			if (ttype != T_NOTSET && ttype != T_LOCK)
				goto argcombo;
			ttype = T_LOCK;
			if (ch != 'c')
				for (p = optarg; *p; ++p)
					switch (*p) {
					case 'A':
						LF_SET(DB_STAT_ALL);
						break;
					case 'c':
						LF_SET(DB_STAT_LOCK_CONF);
						break;
					case 'l':
						LF_SET(DB_STAT_LOCK_LOCKERS);
						break;
					case 'm': /* Backward compatible. */
						break;
					case 'o':
						LF_SET(DB_STAT_LOCK_OBJECTS);
						break;
					case 'p':
						LF_SET(DB_STAT_LOCK_PARAMS);
						break;
					default:
						return (usage());
					}
			break;
		case 'd':
			if (ttype != T_NOTSET && ttype != T_DB)
				goto argcombo;
			ttype = T_DB;
			db = optarg;
			break;
		case 'E': case 'e':
			if (ttype != T_NOTSET && ttype != T_ENV)
				goto argcombo;
			ttype = T_ENV;
			LF_SET(DB_STAT_SUBSYSTEM);
			if (ch == 'E')
				LF_SET(DB_STAT_ALL);
			break;
		case 'f':
			fast = DB_FAST_STAT;
			break;
		case 'h':
			home = optarg;
			break;
		case 'L': case 'l':
			if (ttype != T_NOTSET && ttype != T_LOG)
				goto argcombo;
			ttype = T_LOG;
			if (ch != 'l')
				for (p = optarg; *p; ++p)
					switch (*p) {
					case 'A':
						LF_SET(DB_STAT_ALL);
						break;
					default:
						return (usage());
					}
			break;
		case 'M': case 'm':
			if (ttype != T_NOTSET && ttype != T_MPOOL)
				goto argcombo;
			ttype = T_MPOOL;
			if (ch != 'm')
				for (p = optarg; *p; ++p)
					switch (*p) {
					case 'A':
						LF_SET(DB_STAT_ALL);
						break;
					case 'h':
						LF_SET(DB_STAT_MEMP_HASH);
						break;
					case 'm': /* Backward compatible. */
						break;
					default:
						return (usage());
					}
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
		case 'R': case 'r':
			if (ttype != T_NOTSET && ttype != T_REP)
				goto argcombo;
			ttype = T_REP;
			if (ch != 'r')
				for (p = optarg; *p; ++p)
					switch (*p) {
					case 'A':
						LF_SET(DB_STAT_ALL);
						break;
					default:
						return (usage());
					}
			break;
		case 's':
			if (ttype != T_NOTSET && ttype != T_DB)
				goto argcombo;
			ttype = T_DB;
			subdb = optarg;
			break;
		case 't':
			if (ttype != T_NOTSET) {
argcombo:			fprintf(stderr,
				    "%s: illegal option combination\n",
				    progname);
				return (EXIT_FAILURE);
			}
			ttype = T_TXN;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case 'Z':
			LF_SET(DB_STAT_CLEAR);
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	switch (ttype) {
	case T_DB:
		if (db == NULL)
			return (usage());
		break;
	case T_NOTSET:
		return (usage());
		/* NOTREACHED */
	case T_ENV:
	case T_LOCK:
	case T_LOG:
	case T_MPOOL:
	case T_REP:
	case T_TXN:
		if (fast != 0)
			return (usage());
		break;
	}

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
retry:	if ((ret = db_env_create(&dbenv, env_flags)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		goto err;
	}

	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, progname);

	if (nflag) {
		if ((ret = dbenv->set_flags(dbenv, DB_NOLOCKING, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOLOCKING");
			goto err;
		}
		if ((ret = dbenv->set_flags(dbenv, DB_NOPANIC, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOPANIC");
			goto err;
		}
	}

	if (passwd != NULL &&
	    (ret = dbenv->set_encrypt(dbenv, passwd, DB_ENCRYPT_AES)) != 0) {
		dbenv->err(dbenv, ret, "set_passwd");
		goto err;
	}

	/* Initialize the environment. */
	if (db_init(dbenv, home, ttype, cache, &private) != 0)
		goto err;

	switch (ttype) {
	case T_DB:
		if (flags != 0)
			return (usage());

		/* Create the DB object and open the file. */
		if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
			dbenv->err(dbenv, ret, "db_create");
			goto err;
		}

		if ((ret = dbp->open(dbp,
		    NULL, db, subdb, DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
			dbenv->err(dbenv, ret, "DB->open: %s", db);
			goto err;
		}

		/* Check if cache is too small for this DB's pagesize. */
		if (private) {
			if ((ret = __db_util_cache(dbp, &cache, &resize)) != 0)
				goto err;
			if (resize) {
				(void)dbp->close(dbp, DB_NOSYNC);
				dbp = NULL;

				(void)dbenv->close(dbenv, 0);
				dbenv = NULL;
				goto retry;
			}
		}

		/*
		 * See if we can open this db read/write to update counts.
		 * If its a master-db then we cannot.  So check to see,
		 * if its btree then it might be.
		 */
		if (subdb == NULL && dbp->type == DB_BTREE &&
		    (ret = dbp->stat(dbp, NULL, &sp, DB_FAST_STAT)) != 0) {
			dbenv->err(dbenv, ret, "DB->stat");
			goto err;
		}

		if (subdb != NULL ||
		    dbp->type != DB_BTREE ||
		    (sp->bt_metaflags & BTM_SUBDB) == 0) {
			if ((ret = db_create(&alt_dbp, dbenv, 0)) != 0) {
				dbenv->err(dbenv, ret, "db_create");
				goto err;
			}
			if ((ret = dbp->open(alt_dbp, NULL,
			    db, subdb, DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
				if (subdb == NULL)
					dbenv->err(dbenv,
					   ret, "DB->open: %s", db);
				else
					dbenv->err(dbenv,
					   ret, "DB->open: %s:%s", db, subdb);
				(void)alt_dbp->close(alt_dbp, DB_NOSYNC);
				goto err;
			}

			(void)dbp->close(dbp, DB_NOSYNC);
			dbp = alt_dbp;
		}

		if (dbp->stat_print(dbp, flags))
			goto err;
		break;
	case T_ENV:
		if (dbenv->stat_print(dbenv, flags))
			goto err;
		break;
	case T_LOCK:
		if (dbenv->lock_stat_print(dbenv, flags))
			goto err;
		break;
	case T_LOG:
		if (dbenv->log_stat_print(dbenv, flags))
			goto err;
		break;
	case T_MPOOL:
		if (dbenv->memp_stat_print(dbenv, flags))
			goto err;
		break;
	case T_REP:
		if (dbenv->rep_stat_print(dbenv, flags))
			goto err;
		break;
	case T_TXN:
		if (dbenv->txn_stat_print(dbenv, flags))
			goto err;
		break;
	case T_NOTSET:
		dbenv->errx(dbenv, "Unknown statistics flag");
		goto err;
	}

	if (0) {
err:		exitval = 1;
	}
	if (dbp != NULL && (ret = dbp->close(dbp, DB_NOSYNC)) != 0) {
		exitval = 1;
		dbenv->err(dbenv, ret, "close");
	}
	if (dbenv != NULL && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	if (passwd != NULL)
		free(passwd);

	/* Resend any caught signal. */
	__db_util_sigresend();

	return (exitval == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * db_init --
 *	Initialize the environment.
 */
int
db_init(dbenv, home, ttype, cache, is_private)
	DB_ENV *dbenv;
	char *home;
	test_t ttype;
	u_int32_t cache;
	int *is_private;
{
	u_int32_t oflags;
	int ret;

	/*
	 * If our environment open fails, and we're trying to look at a
	 * shared region, it's a hard failure.
	 *
	 * We will probably just drop core if the environment we join does
	 * not include a memory pool.  This is probably acceptable; trying
	 * to use an existing environment that does not contain a memory
	 * pool to look at a database can be safely construed as operator
	 * error, I think.
	 */
	*is_private = 0;
	if ((ret =
	    dbenv->open(dbenv, home, DB_JOINENV | DB_USE_ENVIRON, 0)) == 0)
		return (0);
	if (ret == DB_VERSION_MISMATCH)
		goto err;
	if (ttype != T_DB && ttype != T_LOG) {
		dbenv->err(dbenv, ret, "DB_ENV->open%s%s",
		    home == NULL ? "" : ": ", home == NULL ? "" : home);
		return (1);
	}

	/*
	 * We're looking at a database or set of log files and no environment
	 * exists.  Create one, but make it private so no files are actually
	 * created.  Declare a reasonably large cache so that we don't fail
	 * when reporting statistics on large databases.
	 *
	 * An environment is required to look at databases because we may be
	 * trying to look at databases in directories other than the current
	 * one.
	 */
	if ((ret = dbenv->set_cachesize(dbenv, 0, cache, 1)) != 0) {
		dbenv->err(dbenv, ret, "set_cachesize");
		return (1);
	}
	*is_private = 1;
	oflags = DB_CREATE | DB_PRIVATE | DB_USE_ENVIRON;
	if (ttype == T_DB)
		oflags |= DB_INIT_MPOOL;
	if (ttype == T_LOG)
		oflags |= DB_INIT_LOG;
	if (ttype == T_REP)
		oflags |= DB_INIT_REP;
	if ((ret = dbenv->open(dbenv, home, oflags, 0)) == 0)
		return (0);

	/* An environment is required. */
err:	dbenv->err(dbenv, ret, "DB_ENV->open");
	return (1);
}

int
usage()
{
	fprintf(stderr, "usage: db_stat %s\n",
	    "-d file [-fN] [-h home] [-P password] [-s database]");
	fprintf(stderr, "usage: db_stat %s\n\t%s\n",
	    "[-cEelmNrtVZ] [-C Aclop]",
	    "[-h home] [-L A] [-M A] [-P password] [-R A]");
	return (EXIT_FAILURE);
}

int
version_check(progname)
	const char *progname;
{
	int v_major, v_minor, v_patch;

	/* Make sure we're loaded with the right version of the DB library. */
	(void)db_version(&v_major, &v_minor, &v_patch);
	if (v_major != DB_VERSION_MAJOR || v_minor != DB_VERSION_MINOR) {
		fprintf(stderr,
	"%s: version %d.%d doesn't match library version %d.%d\n",
		    progname, DB_VERSION_MAJOR, DB_VERSION_MINOR,
		    v_major, v_minor);
		return (EXIT_FAILURE);
	}
	return (0);
}
