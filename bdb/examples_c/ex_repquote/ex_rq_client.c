/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_rq_client.c,v 1.29 2002/01/23 15:33:19 bostic Exp $
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

#include "ex_repquote.h"

static void *check_loop __P((void *));
static void *display_loop __P((void *));
static int print_stocks __P((DBC *));

typedef struct {
	const char *progname;
	DB_ENV *dbenv;
} disploop_args;

typedef struct {
	DB_ENV *dbenv;
	machtab_t *machtab;
} checkloop_args;

int
doclient(dbenv, progname, machtab)
	DB_ENV *dbenv;
	const char *progname;
	machtab_t *machtab;
{
	checkloop_args cargs;
	disploop_args dargs;
	pthread_t check_thr, disp_thr;
	void *cstatus, *dstatus;
	int rval, s;

	rval = EXIT_SUCCESS;
	s = -1;

	memset(&dargs, 0, sizeof(dargs));
	dstatus = (void *)EXIT_FAILURE;

	dargs.progname = progname;
	dargs.dbenv = dbenv;
	if (pthread_create(&disp_thr, NULL, display_loop, (void *)&dargs)) {
		dbenv->err(dbenv, errno, "display_loop pthread_create failed");
		goto err;
	}

	cargs.dbenv = dbenv;
	cargs.machtab = machtab;
	if (pthread_create(&check_thr, NULL, check_loop, (void *)&cargs)) {
		dbenv->err(dbenv, errno, "check_thread pthread_create failed");
		goto err;
	}
	if (pthread_join(disp_thr, &dstatus) ||
	    pthread_join(check_thr, &cstatus)) {
		dbenv->err(dbenv, errno, "pthread_join failed");
		goto err;
	}

	if (0) {
err:		rval = EXIT_FAILURE;
	}
	return (rval);
}

/*
 * Our only job is to check that the master is valid and if it's not
 * for an extended period, to trigger an election.  We do two phases.
 * If we do not have a master, first we send out a request for a master
 * to identify itself (that would be a call to rep_start).  If that fails,
 * we trigger an election.
 */
static void *
check_loop(args)
	void *args;
{
	DB_ENV *dbenv;
	DBT dbt;
	checkloop_args *cargs;
	int count, n, pri;
	machtab_t *machtab;
	u_int32_t timeout;

	cargs = (checkloop_args *)args;
	dbenv = cargs->dbenv;
	machtab = cargs->machtab;

#define	IDLE_INTERVAL	1

	count = 0;
	while (master_eid == DB_EID_INVALID) {
		/*
		 * Call either rep_start or rep_elect depending on if
		 * count is 0 or 1.
		 */

		if (count == 0) {
			memset(&dbt, 0, sizeof(dbt));
			dbt.data = myaddr;
			dbt.size = strlen(myaddr) + 1;
			(void)dbenv->rep_start(dbenv, &dbt, DB_REP_CLIENT);
			count = 1;
		} else {
			machtab_parm(machtab, &n, &pri, &timeout);
			(void)dbenv->rep_elect(dbenv,
			    n, pri, timeout, &master_eid);
			count = 0;
		}
		sleep(IDLE_INTERVAL);
	}

	return ((void *)EXIT_SUCCESS);
}

static void *
display_loop(args)
	void *args;
{
	DB *dbp;
	DB_ENV *dbenv;
	DBC *dbc;
	const char *progname;
	disploop_args *dargs;
	int ret, rval;

	dargs = (disploop_args *)args;
	progname = dargs->progname;
	dbenv = dargs->dbenv;

	dbc = NULL;
	dbp = NULL;

	for (;;) {
		/* If we become master, shut this loop off. */
		if (master_eid == SELF_EID)
			break;

		if (dbp == NULL) {
			if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
				dbenv->err(dbenv, ret, "db_create");
				return ((void *)EXIT_FAILURE);
			}

			if ((ret = dbp->open(dbp, NULL,
			    DATABASE, NULL, DB_BTREE, DB_RDONLY, 0)) != 0) {
				if (ret == ENOENT) {
					printf(
				    "No stock database yet available.\n");
					if ((ret = dbp->close(dbp, 0)) != 0) {
						dbenv->err(dbenv,
						    ret, "DB->close");
						goto err;
					}
					dbp = NULL;
					sleep(SLEEPTIME);
					continue;
				}
				dbenv->err(dbenv, ret, "DB->open");
				goto err;
			}
		}

		if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0) {
			dbenv->err(dbenv, ret, "DB->cursor");
			goto err;
		}

		if ((ret = print_stocks(dbc)) != 0) {
			dbenv->err(dbenv, ret, "database traversal failed");
			goto err;
		}

		if ((ret = dbc->c_close(dbc)) != 0) {
			dbenv->err(dbenv, ret, "DB->close");
			goto err;
		}

		dbc = NULL;

		sleep(SLEEPTIME);
	}

	rval = EXIT_SUCCESS;

	if (0) {
err:		rval = EXIT_FAILURE;
	}

	if (dbc != NULL && (ret = dbc->c_close(dbc)) != 0) {
		dbenv->err(dbenv, ret, "DB->close");
		rval = EXIT_FAILURE;
	}

	if (dbp != NULL && (ret = dbp->close(dbp, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->close");
		return ((void *)EXIT_FAILURE);
	}

	return ((void *)rval);
}

static int
print_stocks(dbc)
	DBC *dbc;
{
	DBT key, data;
#define	MAXKEYSIZE	10
#define	MAXDATASIZE	20
	char keybuf[MAXKEYSIZE + 1], databuf[MAXDATASIZE + 1];
	int ret;
	u_int32_t keysize, datasize;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	printf("\tSymbol\tPrice\n");
	printf("\t======\t=====\n");

	for (ret = dbc->c_get(dbc, &key, &data, DB_FIRST);
	    ret == 0;
	    ret = dbc->c_get(dbc, &key, &data, DB_NEXT)) {
		keysize = key.size > MAXKEYSIZE ? MAXKEYSIZE : key.size;
		memcpy(keybuf, key.data, keysize);
		keybuf[keysize] = '\0';

		datasize = data.size >= MAXDATASIZE ? MAXDATASIZE : data.size;
		memcpy(databuf, data.data, datasize);
		databuf[datasize] = '\0';

		printf("\t%s\t%s\n", keybuf, databuf);
	}
	printf("\n");
	return (ret == DB_NOTFOUND ? 0 : ret);
}
