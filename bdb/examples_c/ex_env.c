/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_env.c,v 11.27 2002/08/15 14:34:11 bostic Exp $
 */

#include <sys/types.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <db.h>

#ifdef macintosh
#define	DATABASE_HOME	":database"
#define	CONFIG_DATA_DIR	":database"
#else
#ifdef DB_WIN32
#define	DATABASE_HOME	"\\tmp\\database"
#define	CONFIG_DATA_DIR	"\\database\\files"
#else
#define	DATABASE_HOME	"/tmp/database"
#define	CONFIG_DATA_DIR	"/database/files"
#endif
#endif

int	db_setup __P((const char *, const char *, FILE *, const char *));
int	db_teardown __P((const char *, const char *, FILE *, const char *));
int	main __P((void));

/*
 * An example of a program creating/configuring a Berkeley DB environment.
 */
int
main()
{
	const char *data_dir, *home;
	const char *progname = "ex_env";		/* Program name. */

	/*
	 * All of the shared database files live in DATABASE_HOME, but
	 * data files will live in CONFIG_DATA_DIR.
	 */
	home = DATABASE_HOME;
	data_dir = CONFIG_DATA_DIR;

	printf("Setup env\n");
	if (db_setup(home, data_dir, stderr, progname) != 0)
		return (EXIT_FAILURE);

	printf("Teardown env\n");
	if (db_teardown(home, data_dir, stderr, progname) != 0)
		return (EXIT_FAILURE);

	return (EXIT_SUCCESS);
}

int
db_setup(home, data_dir, errfp, progname)
	const char *home, *data_dir, *progname;
	FILE *errfp;
{
	DB_ENV *dbenv;
	int ret;

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (1);
	}
	dbenv->set_errfile(dbenv, errfp);
	dbenv->set_errpfx(dbenv, progname);

	/*
	 * We want to specify the shared memory buffer pool cachesize,
	 * but everything else is the default.
	 */
	if ((ret = dbenv->set_cachesize(dbenv, 0, 64 * 1024, 0)) != 0) {
		dbenv->err(dbenv, ret, "set_cachesize");
		dbenv->close(dbenv, 0);
		return (1);
	}

	/* Databases are in a subdirectory. */
	(void)dbenv->set_data_dir(dbenv, data_dir);

	/* Open the environment with full transactional support. */
	if ((ret = dbenv->open(dbenv, home,
    DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN,
	    0)) != 0) {
		dbenv->err(dbenv, ret, "environment open: %s", home);
		dbenv->close(dbenv, 0);
		return (1);
	}

	/* Do something interesting... */

	/* Close the handle. */
	if ((ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr, "DB_ENV->close: %s\n", db_strerror(ret));
		return (1);
	}
	return (0);
}

int
db_teardown(home, data_dir, errfp, progname)
	const char *home, *data_dir, *progname;
	FILE *errfp;
{
	DB_ENV *dbenv;
	int ret;

	/* Remove the shared database regions. */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (1);
	}
	dbenv->set_errfile(dbenv, errfp);
	dbenv->set_errpfx(dbenv, progname);

	(void)dbenv->set_data_dir(dbenv, data_dir);
	if ((ret = dbenv->remove(dbenv, home, 0)) != 0) {
		fprintf(stderr, "DB_ENV->remove: %s\n", db_strerror(ret));
		return (1);
	}
	return (0);
}
