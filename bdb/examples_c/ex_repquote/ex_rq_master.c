/*-
 * #include <pthread.h>
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_rq_master.c,v 1.22 2002/08/06 05:39:03 bostic Exp $
 */

#include <sys/types.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

#include "ex_repquote.h"

static void *master_loop __P((void *));

#define	BUFSIZE 1024

int
domaster(dbenv, progname)
	DB_ENV *dbenv;
	const char *progname;
{
	int ret, t_ret;
	pthread_t interface_thr;
	pthread_attr_t attr;

	COMPQUIET(progname, NULL);

	/* Spawn off a thread to handle the basic master interface. */
	if ((ret = pthread_attr_init(&attr)) != 0 &&
	    (ret = pthread_attr_setdetachstate(&attr,
	    PTHREAD_CREATE_DETACHED)) != 0)
		goto err;

	if ((ret = pthread_create(&interface_thr,
	    &attr, master_loop, (void *)dbenv)) != 0)
		goto err;

err:	if ((t_ret = pthread_attr_destroy(&attr)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

static void *
master_loop(dbenvv)
	void *dbenvv;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_TXN *txn;
	DBT key, data;
	char buf[BUFSIZE], *rbuf;
	int ret;

	dbp = NULL;
	txn = NULL;

	dbenv = (DB_ENV *)dbenvv;
	/*
	 * Check if the database exists and if it verifies cleanly.
	 * If it does, run with it; else recreate it and go.  Note
	 * that we have to verify outside of the environment.
	 */
#ifdef NOTDEF
	if ((ret = db_create(&dbp, NULL, 0)) != 0)
		return (ret);
	if ((ret = dbp->verify(dbp, DATABASE, NULL, NULL, 0)) != 0) {
		if ((ret = dbp->remove(dbp, DATABASE, NULL, 0)) != 0 &&
		    ret != DB_NOTFOUND && ret != ENOENT)
			return (ret);
#endif
		if ((ret = db_create(&dbp, dbenv, 0)) != 0)
			return ((void *)ret);

		if ((ret = dbenv->txn_begin(dbenv, NULL, &txn, 0)) != 0)
			goto err;
		if ((ret = dbp->open(dbp, txn, DATABASE,
		    NULL, DB_BTREE, DB_CREATE /* | DB_THREAD */, 0)) != 0)
			goto err;
		ret = txn->commit(txn, 0);
		txn = NULL;
		if (ret != 0) {
			dbp = NULL;
			goto err;
		}

#ifdef NOTDEF
	} else {
		/* Reopen in the environment. */
		if ((ret = dbp->close(dbp, 0)) != 0)
			return (ret);
		if ((ret = db_create(&dbp, dbenv, 0)) != 0)
			return (ret);
		if ((ret = dbp->open(dbp,
		    DATABASE, NULL, DB_UNKNOWN, DB_THREAD, 0)) != 0)
			goto err;
	}
#endif
	/*
	 * XXX
	 * It would probably be kind of cool to do this in Tcl and
	 * have a nice GUI.  It would also be cool to be independently
	 * wealthy.
	 */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	for (;;) {
		printf("QUOTESERVER> ");
		fflush(stdout);

		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		(void)strtok(&buf[0], " \t\n");
		rbuf = strtok(NULL, " \t\n");
		if (rbuf == NULL || rbuf[0] == '\0') {
			if (strncmp(buf, "exit", 4) == 0 ||
			    strncmp(buf, "quit", 4) == 0)
				break;
			dbenv->errx(dbenv, "Format: TICKER VALUE");
			continue;
		}

		key.data = buf;
		key.size = strlen(buf);

		data.data = rbuf;
		data.size = strlen(rbuf);

		if ((ret = dbenv->txn_begin(dbenv, NULL, &txn, 0)) != 0)
			goto err;
		switch (ret =
		    dbp->put(dbp, txn, &key, &data, 0)) {
		case 0:
			break;
		default:
			dbp->err(dbp, ret, "DB->put");
			if (ret != DB_KEYEXIST)
				goto err;
			break;
		}
		ret = txn->commit(txn, 0);
		txn = NULL;
		if (ret != 0)
			goto err;
	}

err:	if (txn != NULL)
		(void)txn->abort(txn);

	if (dbp != NULL)
		(void)dbp->close(dbp, DB_NOSYNC);

	return ((void *)ret);
}
