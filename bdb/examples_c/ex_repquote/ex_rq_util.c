/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_rq_util.c,v 1.20 2002/08/06 05:39:04 bostic Exp $
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

static int connect_site __P((DB_ENV *, machtab_t *, const char *,
   repsite_t *, int *, int *));
void * elect_thread __P((void *));

typedef struct {
	DB_ENV *dbenv;
	machtab_t *machtab;
} elect_args;

typedef struct {
	DB_ENV *dbenv;
	const char *progname;
	const char *home;
	int fd;
	u_int32_t eid;
	machtab_t *tab;
} hm_loop_args;

/*
 * This is a generic message handling loop that is used both by the
 * master to accept messages from a client as well as by clients
 * to communicate with other clients.
 */
void *
hm_loop(args)
	void *args;
{
	DB_ENV *dbenv;
	DBT rec, control;
	const char *c, *home, *progname;
	int fd, eid, n, newm;
	int open, pri, r, ret, t_ret, tmpid;
	elect_args *ea;
	hm_loop_args *ha;
	machtab_t *tab;
	pthread_t elect_thr;
	repsite_t self;
	u_int32_t timeout;
	void *status;

	ea = NULL;

	ha = (hm_loop_args *)args;
	dbenv = ha->dbenv;
	fd = ha->fd;
	home = ha->home;
	eid = ha->eid;
	progname = ha->progname;
	tab = ha->tab;
	free(ha);

	memset(&rec, 0, sizeof(DBT));
	memset(&control, 0, sizeof(DBT));

	for (ret = 0; ret == 0;) {
		if ((ret = get_next_message(fd, &rec, &control)) != 0) {
			/*
			 * Close this connection; if it's the master call
			 * for an election.
			 */
			close(fd);
			if ((ret = machtab_rem(tab, eid, 1)) != 0)
				break;

			/*
			 * If I'm the master, I just lost a client and this
			 * thread is done.
			 */
			if (master_eid == SELF_EID)
				break;

			/*
			 * If I was talking with the master and the master
			 * went away, I need to call an election; else I'm
			 * done.
			 */
			if (master_eid != eid)
				break;

			master_eid = DB_EID_INVALID;
			machtab_parm(tab, &n, &pri, &timeout);
			if ((ret = dbenv->rep_elect(dbenv,
			    n, pri, timeout, &newm)) != 0)
				continue;

			/*
			 * Regardless of the results, the site I was talking
			 * to is gone, so I have nothing to do but exit.
			 */
			if (newm == SELF_EID && (ret =
			    dbenv->rep_start(dbenv, NULL, DB_REP_MASTER)) == 0)
				ret = domaster(dbenv, progname);
			break;
		}

		tmpid = eid;
		switch(r = dbenv->rep_process_message(dbenv,
		    &control, &rec, &tmpid)) {
		case DB_REP_NEWSITE:
			/*
			 * Check if we got sent connect information and if we
			 * did, if this is me or if we already have a
			 * connection to this new site.  If we don't,
			 * establish a new one.
			 */

			/* No connect info. */
			if (rec.size == 0)
				break;

			/* It's me, do nothing. */
			if (strncmp(myaddr, rec.data, rec.size) == 0)
				break;

			self.host = (char *)rec.data;
			self.host = strtok(self.host, ":");
			if ((c = strtok(NULL, ":")) == NULL) {
				dbenv->errx(dbenv, "Bad host specification");
				goto out;
			}
			self.port = atoi(c);

			/*
			 * We try to connect to the new site.  If we can't,
			 * we treat it as an error since we know that the site
			 * should be up if we got a message from it (even
			 * indirectly).
			 */
			if ((ret = connect_site(dbenv,
			    tab, progname, &self, &open, &eid)) != 0)
				goto out;
			break;
		case DB_REP_HOLDELECTION:
			if (master_eid == SELF_EID)
				break;
			/* Make sure that previous election has finished. */
			if (ea != NULL) {
				(void)pthread_join(elect_thr, &status);
				ea = NULL;
			}
			if ((ea = calloc(sizeof(elect_args), 1)) == NULL) {
				ret = errno;
				goto out;
			}
			ea->dbenv = dbenv;
			ea->machtab = tab;
			ret = pthread_create(&elect_thr,
			    NULL, elect_thread, (void *)ea);
			break;
		case DB_REP_NEWMASTER:
			/* Check if it's us. */
			master_eid = tmpid;
			if (tmpid == SELF_EID) {
				if ((ret = dbenv->rep_start(dbenv,
				    NULL, DB_REP_MASTER)) != 0)
					goto out;
				ret = domaster(dbenv, progname);
			}
			break;
		case 0:
			break;
		default:
			dbenv->err(dbenv, r, "DB_ENV->rep_process_message");
			break;
		}
	}

out:	if ((t_ret = machtab_rem(tab, eid, 1)) != 0 && ret == 0)
		ret = t_ret;

	/* Don't close the environment before any children exit. */
	if (ea != NULL)
		(void)pthread_join(elect_thr, &status);

	return ((void *)ret);
}

/*
 * This is a generic thread that spawns a thread to listen for connections
 * on a socket and then spawns off child threads to handle each new
 * connection.
 */
void *
connect_thread(args)
	void *args;
{
	DB_ENV *dbenv;
	const char *home, *progname;
	int fd, i, eid, ns, port, ret;
	hm_loop_args *ha;
	connect_args *cargs;
	machtab_t *machtab;
#define	MAX_THREADS 25
	pthread_t hm_thrs[MAX_THREADS];
	pthread_attr_t attr;

	ha = NULL;
	cargs = (connect_args *)args;
	dbenv = cargs->dbenv;
	home = cargs->home;
	progname = cargs->progname;
	machtab = cargs->machtab;
	port = cargs->port;

	if ((ret = pthread_attr_init(&attr)) != 0)
		return ((void *)EXIT_FAILURE);

	if ((ret =
	    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
		goto err;

	/*
	 * Loop forever, accepting connections from new machines,
	 * and forking off a thread to handle each.
	 */
	if ((fd = listen_socket_init(progname, port)) < 0) {
		ret = errno;
		goto err;
	}

	for (i = 0; i < MAX_THREADS; i++) {
		if ((ns = listen_socket_accept(machtab,
		    progname, fd, &eid)) < 0) {
			ret = errno;
			goto err;
		}
		if ((ha = calloc(sizeof(hm_loop_args), 1)) == NULL)
			goto err;
		ha->progname = progname;
		ha->home = home;
		ha->fd = ns;
		ha->eid = eid;
		ha->tab = machtab;
		ha->dbenv = dbenv;
		if ((ret = pthread_create(&hm_thrs[i++], &attr,
		    hm_loop, (void *)ha)) != 0)
			goto err;
		ha = NULL;
	}

	/* If we fell out, we ended up with too many threads. */
	dbenv->errx(dbenv, "Too many threads");
	ret = ENOMEM;

err:	pthread_attr_destroy(&attr);
	return (ret == 0 ? (void *)EXIT_SUCCESS : (void *)EXIT_FAILURE);
}

/*
 * Open a connection to everyone that we've been told about.  If we
 * cannot open some connections, keep trying.
 */
void *
connect_all(args)
	void *args;
{
	DB_ENV *dbenv;
	all_args *aa;
	const char *home, *progname;
	hm_loop_args *ha;
	int failed, i, eid, nsites, open, ret, *success;
	machtab_t *machtab;
	repsite_t *sites;

	ha = NULL;
	aa = (all_args *)args;
	dbenv = aa->dbenv;
	progname = aa->progname;
	home = aa->home;
	machtab = aa->machtab;
	nsites = aa->nsites;
	sites = aa->sites;

	ret = 0;

	/* Some implementations of calloc are sad about alloc'ing 0 things. */
	if ((success = calloc(nsites > 0 ? nsites : 1, sizeof(int))) == NULL) {
		dbenv->err(dbenv, errno, "connect_all");
		ret = 1;
		goto err;
	}

	for (failed = nsites; failed > 0;) {
		for (i = 0; i < nsites; i++) {
			if (success[i])
				continue;

			ret = connect_site(dbenv, machtab,
			    progname, &sites[i], &open, &eid);

			/*
			 * If we couldn't make the connection, this isn't
			 * fatal to the loop, but we have nothing further
			 * to do on this machine at the moment.
			 */
			if (ret == DB_REP_UNAVAIL)
				continue;

			if (ret != 0)
				goto err;

			failed--;
			success[i] = 1;

			/* If the connection is already open, we're done. */
			if (ret == 0 && open == 1)
				continue;

		}
		sleep(1);
	}

err:	free(success);
	return (ret ? (void *)EXIT_FAILURE : (void *)EXIT_SUCCESS);
}

int
connect_site(dbenv, machtab, progname, site, is_open, eidp)
	DB_ENV *dbenv;
	machtab_t *machtab;
	const char *progname;
	repsite_t *site;
	int *is_open;
	int *eidp;
{
	int ret, s;
	hm_loop_args *ha;
	pthread_t hm_thr;

	if ((s = get_connected_socket(machtab, progname,
	    site->host, site->port, is_open, eidp)) < 0)
		return (DB_REP_UNAVAIL);

	if (*is_open)
		return (0);

	if ((ha = calloc(sizeof(hm_loop_args), 1)) == NULL) {
		ret = errno;
		goto err;
	}

	ha->progname = progname;
	ha->fd = s;
	ha->eid = *eidp;
	ha->tab = machtab;
	ha->dbenv = dbenv;

	if ((ret = pthread_create(&hm_thr, NULL,
	    hm_loop, (void *)ha)) != 0) {
		dbenv->err(dbenv, ret, "connect site");
		goto err1;
	}

	return (0);

err1:	free(ha);
err:
	return (ret);
}

/*
 * We need to spawn off a new thread in which to hold an election in
 * case we are the only thread listening on for messages.
 */
void *
elect_thread(args)
	void *args;
{
	DB_ENV *dbenv;
	elect_args *eargs;
	int n, ret, pri;
	machtab_t *machtab;
	u_int32_t timeout;

	eargs = (elect_args *)args;
	dbenv = eargs->dbenv;
	machtab = eargs->machtab;
	free(eargs);

	machtab_parm(machtab, &n, &pri, &timeout);
	while ((ret =
	    dbenv->rep_elect(dbenv, n, pri, timeout, &master_eid)) != 0)
		sleep(2);

	/* Check if it's us. */
	if (master_eid == SELF_EID)
		ret = dbenv->rep_start(dbenv, NULL, DB_REP_MASTER);

	return ((void *)(ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE));
}
