/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_dbclient.c,v 1.12 2000/10/26 14:13:05 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <db.h>

#define	DATABASE_HOME	"database"

#define	DATABASE	"access.db"

int	db_clientrun __P((DB_ENV *, char *));
int	ex_dbclient_run __P((char *, FILE *, char *, char *));
#ifdef HAVE_VXWORKS
int	ex_dbclient __P((char *));
#define	ERROR_RETURN	ERROR
#define	VXSHM_KEY	10
#else
int	main __P((int, char *[]));
#define	ERROR_RETURN	1
#endif

/*
 * An example of a program creating/configuring a Berkeley DB environment.
 */
#ifndef HAVE_VXWORKS
int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *home;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s hostname\n",argv[0]);
		exit(1);
	}
	/*
	 * All of the shared database files live in DATABASE_HOME, but
	 * data files will live in CONFIG_DATA_DIR.
	 */
	home = DATABASE_HOME;

	if ((ret = ex_dbclient_run(home, stderr, argv[1], argv[0])) != 0)
		return (ret);

	return (0);
}
#endif

int
ex_dbclient(host)
	char *host;
{
	char *home;
	char *progname = "ex_dbclient";		/* Program name. */
	int ret;

	/*
	 * All of the shared database files live in DATABASE_HOME, but
	 * data files will live in CONFIG_DATA_DIR.
	 */
	home = DATABASE_HOME;

	if ((ret = ex_dbclient_run(home, stderr, host, progname)) != 0)
		return (ret);

	return (0);
}

int
ex_dbclient_run(home, errfp, host, progname)
	char *home, *host, *progname;
	FILE *errfp;
{
	DB_ENV *dbenv;
	int ret, retry;

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, DB_CLIENT)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
#ifdef HAVE_VXWORKS
	if ((ret = dbenv->set_shm_key(dbenv, VXSHM_KEY)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
#endif
	retry = 0;
retry:
	while (retry < 5) {
		/*
		 * Set the server host we are talking to.
		 */
		if ((ret =
		    dbenv->set_server(dbenv, host, 10000, 10000, 0)) != 0) {
			fprintf(stderr, "Try %d: DBENV->set_server: %s\n",
			    retry, db_strerror(ret));
			retry++;
			if ((ret = __os_sleep(dbenv, 15, 0)) != 0)
				return (ret);
		} else
			break;
	}

	if (retry >= 5) {
		fprintf(stderr, "DBENV->set_server: %s\n", db_strerror(ret));
		dbenv->close(dbenv, 0);
		return (ERROR_RETURN);
	}
	/*
	 * We want to specify the shared memory buffer pool cachesize,
	 * but everything else is the default.
	 */
	if ((ret = dbenv->set_cachesize(dbenv, 0, 64 * 1024, 0)) != 0) {
		dbenv->err(dbenv, ret, "set_cachesize");
		dbenv->close(dbenv, 0);
		return (ERROR_RETURN);
	}
	/*
	 * We have multiple processes reading/writing these files, so
	 * we need concurrency control and a shared buffer pool, but
	 * not logging or transactions.
	 */
	if ((ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN, 0)) != 0) {
		dbenv->err(dbenv, ret, "environment open: %s", home);
		dbenv->close(dbenv, 0);
		if (ret == DB_NOSERVER)
			goto retry;
		return (ERROR_RETURN);
	}

	ret = db_clientrun(dbenv, progname);
	printf("db_clientrun returned %d\n", ret);
	if (ret == DB_NOSERVER)
		goto retry;

	/* Close the handle. */
	if ((ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr, "DBENV->close: %s\n", db_strerror(ret));
		return (ERROR_RETURN);
	}
	return (0);
}

int
db_clientrun(dbenv, progname)
	DB_ENV *dbenv;
	char *progname;
{
	DB *dbp;
	DBT key, data;
	u_int32_t len;
	int ret;
	char *p, *t, buf[1024], rbuf[1024];

	/* Remove the previous database. */

	/* Create and initialize database object, open the database. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_create: %s\n", progname, db_strerror(ret));
		return (ret);
	}
	if ((ret = dbp->set_pagesize(dbp, 1024)) != 0) {
		dbp->err(dbp, ret, "set_pagesize");
		goto err1;
	}
	if ((ret =
	    dbp->open(dbp, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "%s: open", DATABASE);
		goto err1;
	}

	/*
	 * Insert records into the database, where the key is the user
	 * input and the data is the user input in reverse order.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	for (;;) {
		printf("input> ");
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		if ((len = strlen(buf)) <= 1)
			continue;
		for (t = rbuf, p = buf + (len - 2); p >= buf;)
			*t++ = *p--;
		*t++ = '\0';

		key.data = buf;
		data.data = rbuf;
		data.size = key.size = len - 1;

		switch (ret =
		    dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE)) {
		case 0:
			break;
		default:
			dbp->err(dbp, ret, "DB->put");
			if (ret != DB_KEYEXIST)
				goto err1;
			break;
		}
		memset(&data, 0, sizeof(DBT));
		switch (ret = dbp->get(dbp, NULL, &key, &data, 0)) {
		case 0:
			printf("%.*s : %.*s\n",
			    (int)key.size, (char *)key.data,
			    (int)data.size, (char *)data.data);
			break;
		default:
			dbp->err(dbp, ret, "DB->get");
			break;
		}
	}
	if ((ret = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr,
		    "%s: DB->close: %s\n", progname, db_strerror(ret));
		return (1);
	}
	return (0);

err1:	(void)dbp->close(dbp, 0);
	return (ret);
}
