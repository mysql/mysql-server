/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_env.c,v 11.18 2000/10/27 20:32:00 dda Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include <db.h>

#ifdef macintosh
#define	DATABASE_HOME	":database"
#define	CONFIG_DATA_DIR	":database"
#else
#ifdef DB_WIN32
#define	DATABASE_HOME	"\\tmp\\database"
#define	CONFIG_DATA_DIR	"\\database\\files"
#else
#ifdef HAVE_VXWORKS
#define	DATABASE_HOME	"/ata0/vxtmp/database"
#define	CONFIG_DATA_DIR	"/vxtmp/vxtmp/database/files"
#else
#define	DATABASE_HOME	"/tmp/database"
#define	CONFIG_DATA_DIR	"/database/files"
#endif
#endif
#endif

int	db_setup __P((char *, char *, FILE *, char *));
int	db_teardown __P((char *, char *, FILE *, char *));
#ifdef HAVE_VXWORKS
int	ex_env __P((void));
#define	ERROR_RETURN	ERROR
#define	VXSHM_KEY	11
#else
int	main __P((void));
#define	ERROR_RETURN	1
#endif

/*
 * An example of a program creating/configuring a Berkeley DB environment.
 */
int
#ifdef HAVE_VXWORKS
ex_env()
#else
main()
#endif
{
	int ret;
	char *data_dir, *home;
	char *progname = "ex_env";		/* Program name. */

	/*
	 * All of the shared database files live in DATABASE_HOME, but
	 * data files will live in CONFIG_DATA_DIR.
	 */
	home = DATABASE_HOME;
	data_dir = CONFIG_DATA_DIR;

	printf("Setup env\n");
	if ((ret = db_setup(home, data_dir, stderr, progname)) != 0)
		return (ret);

	printf("Teardown env\n");
	if ((ret = db_teardown(home, data_dir, stderr, progname)) != 0)
		return (ret);

	return (0);
}

int
db_setup(home, data_dir, errfp, progname)
	char *home, *data_dir, *progname;
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
		return (ERROR_RETURN);
	}
	dbenv->set_errfile(dbenv, errfp);
	dbenv->set_errpfx(dbenv, progname);

#ifdef HAVE_VXWORKS
	/* VxWorks needs to specify a base segment ID. */
	if ((ret = dbenv->set_shm_key(dbenv, VXSHM_KEY)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
#endif

	/*
	 * We want to specify the shared memory buffer pool cachesize,
	 * but everything else is the default.
	 */
	if ((ret = dbenv->set_cachesize(dbenv, 0, 64 * 1024, 0)) != 0) {
		dbenv->err(dbenv, ret, "set_cachesize");
		dbenv->close(dbenv, 0);
		return (ERROR_RETURN);
	}

	/* Databases are in a subdirectory. */
	(void)dbenv->set_data_dir(dbenv, data_dir);

	/* Open the environment with full transactional support. */
	if ((ret = dbenv->open(dbenv, home,
    DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN,
	    0)) != 0) {
		dbenv->err(dbenv, ret, "environment open: %s", home);
		dbenv->close(dbenv, 0);
		return (ERROR_RETURN);
	}

	/* Do something interesting... */

	/* Close the handle. */
	if ((ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr, "DBENV->close: %s\n", db_strerror(ret));
		return (ERROR_RETURN);
	}
	return (0);
}

int
db_teardown(home, data_dir, errfp, progname)
	char *home, *data_dir, *progname;
	FILE *errfp;
{
	DB_ENV *dbenv;
	int ret;

	/* Remove the shared database regions. */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
	dbenv->set_errfile(dbenv, errfp);
	dbenv->set_errpfx(dbenv, progname);
#ifdef HAVE_VXWORKS
	if ((ret = dbenv->set_shm_key(dbenv, VXSHM_KEY)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
#endif

	(void)dbenv->set_data_dir(dbenv, data_dir);
	if ((ret = dbenv->remove(dbenv, home, 0)) != 0) {
		fprintf(stderr, "DBENV->remove: %s\n", db_strerror(ret));
		return (ERROR_RETURN);
	}
	return (0);
}
