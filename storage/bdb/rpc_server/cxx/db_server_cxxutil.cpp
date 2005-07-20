/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: db_server_cxxutil.cpp,v 1.17 2004/09/22 17:30:13 bostic Exp $
 */

#include "db_config.h"

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

#include <rpc/rpc.h>

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_server.h"

#include "db_int.h"
#include "db_cxx.h"
#include "dbinc_auto/clib_ext.h"

extern "C" {
#include "dbinc/db_server_int.h"
#include "dbinc_auto/rpc_server_ext.h"
#include "dbinc_auto/common_ext.h"

extern int __dbsrv_main	 __P((void));
}

static int add_home __P((char *));
static int add_passwd __P((char *));
static int env_recover __P((char *));
static void __dbclear_child __P((ct_entry *));

static LIST_HEAD(cthead, ct_entry) __dbsrv_head;
static LIST_HEAD(homehead, home_entry) __dbsrv_home;
static long __dbsrv_defto = DB_SERVER_TIMEOUT;
static long __dbsrv_maxto = DB_SERVER_MAXTIMEOUT;
static long __dbsrv_idleto = DB_SERVER_IDLETIMEOUT;
static char *logfile = NULL;
static char *prog;

static void usage __P((char *));
static void version_check __P((void));

int __dbsrv_verbose = 0;

int
main(
	int argc,
	char **argv)
{
	extern char *optarg;
	CLIENT *cl;
	int ch, ret;
	char *passwd;

	prog = argv[0];

	version_check();

	/*
	 * Check whether another server is running or not.  There
	 * is a race condition where two servers could be racing to
	 * register with the portmapper.  The goal of this check is to
	 * forbid running additional servers (like those started from
	 * the test suite) if the user is already running one.
	 *
	 * XXX
	 * This does not solve nor prevent two servers from being
	 * started at the same time and running recovery at the same
	 * time on the same environments.
	 */
	if ((cl = clnt_create("localhost",
	    DB_RPC_SERVERPROG, DB_RPC_SERVERVERS, "tcp")) != NULL) {
		fprintf(stderr,
		    "%s: Berkeley DB RPC server already running.\n", prog);
		clnt_destroy(cl);
		return (EXIT_FAILURE);
	}

	LIST_INIT(&__dbsrv_home);
	while ((ch = getopt(argc, argv, "h:I:L:P:t:T:Vv")) != EOF)
		switch (ch) {
		case 'h':
			(void)add_home(optarg);
			break;
		case 'I':
			if (__db_getlong(NULL, prog,
			    optarg, 1, LONG_MAX, &__dbsrv_idleto))
				return (EXIT_FAILURE);
			break;
		case 'L':
			logfile = optarg;
			break;
		case 'P':
			passwd = strdup(optarg);
			memset(optarg, 0, strlen(optarg));
			if (passwd == NULL) {
				fprintf(stderr, "%s: strdup: %s\n",
				    prog, strerror(errno));
				return (EXIT_FAILURE);
			}
			if ((ret = add_passwd(passwd)) != 0) {
				fprintf(stderr, "%s: strdup: %s\n",
				    prog, strerror(ret));
				return (EXIT_FAILURE);
			}
			break;
		case 't':
			if (__db_getlong(NULL, prog,
			    optarg, 1, LONG_MAX, &__dbsrv_defto))
				return (EXIT_FAILURE);
			break;
		case 'T':
			if (__db_getlong(NULL, prog,
			    optarg, 1, LONG_MAX, &__dbsrv_maxto))
				return (EXIT_FAILURE);
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case 'v':
			__dbsrv_verbose = 1;
			break;
		default:
			usage(prog);
		}
	/*
	 * Check default timeout against maximum timeout
	 */
	if (__dbsrv_defto > __dbsrv_maxto)
		__dbsrv_defto = __dbsrv_maxto;

	/*
	 * Check default timeout against idle timeout
	 * It would be bad to timeout environments sooner than txns.
	 */
	if (__dbsrv_defto > __dbsrv_idleto)
		fprintf(stderr,
	    "%s: WARNING: Idle timeout %ld is less than resource timeout %ld\n",
		    prog, __dbsrv_idleto, __dbsrv_defto);

	LIST_INIT(&__dbsrv_head);

	/*
	 * If a client crashes during an RPC, our reply to it
	 * generates a SIGPIPE.  Ignore SIGPIPE so we don't exit unnecessarily.
	 */
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

	if (logfile != NULL && __db_util_logset("berkeley_db_svc", logfile))
		return (EXIT_FAILURE);

	/*
	 * Now that we are ready to start, run recovery on all the
	 * environments specified.
	 */
	if (env_recover(prog) != 0)
		return (EXIT_FAILURE);

	/*
	 * We've done our setup, now call the generated server loop
	 */
	if (__dbsrv_verbose)
		printf("%s:  Ready to receive requests\n", prog);
	__dbsrv_main();

	/* NOTREACHED */
	abort();
}

static void
usage(char *prog)
{
	fprintf(stderr, "usage: %s %s\n\t%s\n", prog,
	    "[-Vv] [-h home] [-P passwd]",
	    "[-I idletimeout] [-L logfile] [-t def_timeout] [-T maxtimeout]");
	exit(EXIT_FAILURE);
}

static void
version_check()
{
	int v_major, v_minor, v_patch;

	/* Make sure we're loaded with the right version of the DB library. */
	(void)db_version(&v_major, &v_minor, &v_patch);
	if (v_major != DB_VERSION_MAJOR ||
	    v_minor != DB_VERSION_MINOR || v_patch != DB_VERSION_PATCH) {
		fprintf(stderr,
	"%s: version %d.%d.%d doesn't match library version %d.%d.%d\n",
		    prog, DB_VERSION_MAJOR, DB_VERSION_MINOR,
		    DB_VERSION_PATCH, v_major, v_minor, v_patch);
		exit(EXIT_FAILURE);
	}
}

extern "C" void
__dbsrv_settimeout(
	ct_entry *ctp,
	u_int32_t to)
{
	if (to > (u_int32_t)__dbsrv_maxto)
		ctp->ct_timeout = __dbsrv_maxto;
	else if (to <= 0)
		ctp->ct_timeout = __dbsrv_defto;
	else
		ctp->ct_timeout = to;
}

extern "C" void
__dbsrv_timeout(int force)
{
	static long to_hint = -1;
	time_t t;
	long to;
	ct_entry *ctp, *nextctp;

	if ((t = time(NULL)) == -1)
		return;

	/*
	 * Check hint.  If hint is further in the future
	 * than now, no work to do.
	 */
	if (!force && to_hint > 0 && t < to_hint)
		return;
	to_hint = -1;
	/*
	 * Timeout transactions or cursors holding DB resources.
	 * Do this before timing out envs to properly release resources.
	 *
	 * !!!
	 * We can just loop through this list looking for cursors and txns.
	 * We do not need to verify txn and cursor relationships at this
	 * point because we maintain the list in LIFO order *and* we
	 * maintain activity in the ultimate txn parent of any cursor
	 * so either everything in a txn is timing out, or nothing.
	 * So, since we are LIFO, we will correctly close/abort all the
	 * appropriate handles, in the correct order.
	 */
	for (ctp = LIST_FIRST(&__dbsrv_head); ctp != NULL; ctp = nextctp) {
		nextctp = LIST_NEXT(ctp, entries);
		switch (ctp->ct_type) {
		case CT_TXN:
			to = *(ctp->ct_activep) + ctp->ct_timeout;
			/* TIMEOUT */
			if (to < t) {
				if (__dbsrv_verbose)
					printf("Timing out txn id %ld\n",
					    ctp->ct_id);
				(void)((DbTxn *)ctp->ct_anyp)->abort();
				__dbdel_ctp(ctp);
				/*
				 * If we timed out an txn, we may have closed
				 * all sorts of ctp's.
				 * So start over with a guaranteed good ctp.
				 */
				nextctp = LIST_FIRST(&__dbsrv_head);
			} else if ((to_hint > 0 && to_hint > to) ||
			    to_hint == -1)
				to_hint = to;
			break;
		case CT_CURSOR:
		case (CT_JOINCUR | CT_CURSOR):
			to = *(ctp->ct_activep) + ctp->ct_timeout;
			/* TIMEOUT */
			if (to < t) {
				if (__dbsrv_verbose)
					printf("Timing out cursor %ld\n",
					    ctp->ct_id);
				(void)__dbc_close_int(ctp);
				/*
				 * Start over with a guaranteed good ctp.
				 */
				nextctp = LIST_FIRST(&__dbsrv_head);
			} else if ((to_hint > 0 && to_hint > to) ||
			    to_hint == -1)
				to_hint = to;
			break;
		default:
			break;
		}
	}
	/*
	 * Timeout idle handles.
	 * If we are forcing a timeout, we'll close all env handles.
	 */
	for (ctp = LIST_FIRST(&__dbsrv_head); ctp != NULL; ctp = nextctp) {
		nextctp = LIST_NEXT(ctp, entries);
		if (ctp->ct_type != CT_ENV)
			continue;
		to = *(ctp->ct_activep) + ctp->ct_idle;
		/* TIMEOUT */
		if (to < t || force) {
			if (__dbsrv_verbose)
				printf("Timing out env id %ld\n", ctp->ct_id);
			(void)__dbenv_close_int(ctp->ct_id, 0, 1);
			/*
			 * If we timed out an env, we may have closed
			 * all sorts of ctp's (maybe even all of them.
			 * So start over with a guaranteed good ctp.
			 */
			nextctp = LIST_FIRST(&__dbsrv_head);
		}
	}
}

/*
 * RECURSIVE FUNCTION.  We need to clear/free any number of levels of nested
 * layers.
 */
static void
__dbclear_child(ct_entry *parent)
{
	ct_entry *ctp, *nextctp;

	for (ctp = LIST_FIRST(&__dbsrv_head); ctp != NULL;
	    ctp = nextctp) {
		nextctp = LIST_NEXT(ctp, entries);
		if (ctp->ct_type == 0)
			continue;
		if (ctp->ct_parent == parent) {
			__dbclear_child(ctp);
			/*
			 * Need to do this here because le_next may
			 * have changed with the recursive call and we
			 * don't want to point to a removed entry.
			 */
			nextctp = LIST_NEXT(ctp, entries);
			__dbclear_ctp(ctp);
		}
	}
}

extern "C" void
__dbclear_ctp(ct_entry *ctp)
{
	LIST_REMOVE(ctp, entries);
	__os_free(NULL, ctp);
}

extern "C" void
__dbdel_ctp(ct_entry *parent)
{
	__dbclear_child(parent);
	__dbclear_ctp(parent);
}

extern "C" ct_entry *
new_ct_ent(int *errp)
{
	time_t t;
	ct_entry *ctp, *octp;
	int ret;

	if ((ret = __os_malloc(NULL, sizeof(ct_entry), &ctp)) != 0) {
		*errp = ret;
		return (NULL);
	}
	memset(ctp, 0, sizeof(ct_entry));
	/*
	 * Get the time as ID.  We may service more than one request per
	 * second however.  If we are, then increment id value until we
	 * find an unused one.  We insert entries in LRU fashion at the
	 * head of the list.  So, if the first entry doesn't match, then
	 * we know for certain that we can use our entry.
	 */
	if ((t = time(NULL)) == -1) {
		*errp = __os_get_errno();
		__os_free(NULL, ctp);
		return (NULL);
	}
	octp = LIST_FIRST(&__dbsrv_head);
	if (octp != NULL && octp->ct_id >= t)
		t = octp->ct_id + 1;
	ctp->ct_id = t;
	ctp->ct_idle = __dbsrv_idleto;
	ctp->ct_activep = &ctp->ct_active;
	ctp->ct_origp = NULL;
	ctp->ct_refcount = 1;

	LIST_INSERT_HEAD(&__dbsrv_head, ctp, entries);
	return (ctp);
}

extern "C" ct_entry *
get_tableent(long id)
{
	ct_entry *ctp;

	for (ctp = LIST_FIRST(&__dbsrv_head); ctp != NULL;
	    ctp = LIST_NEXT(ctp, entries))
		if (ctp->ct_id == id)
			return (ctp);
	return (NULL);
}

extern "C" ct_entry *
__dbsrv_sharedb(ct_entry *db_ctp,
    const char *name, const char *subdb, DBTYPE type, u_int32_t flags)
{
	ct_entry *ctp;

	/*
	 * Check if we can share a db handle.  Criteria for sharing are:
	 * If any of the non-sharable flags are set, we cannot share.
	 * Must be a db ctp, obviously.
	 * Must share the same env parent.
	 * Must be the same type, or current one DB_UNKNOWN.
	 * Must be same byteorder, or current one must not care.
	 * All flags must match.
	 * Must be same name, but don't share in-memory databases.
	 * Must be same subdb name.
	 */
	if (flags & DB_SERVER_DBNOSHARE)
		return (NULL);
	for (ctp = LIST_FIRST(&__dbsrv_head); ctp != NULL;
	    ctp = LIST_NEXT(ctp, entries)) {
		/*
		 * Skip ourselves.
		 */
		if (ctp == db_ctp)
			continue;
		if (ctp->ct_type != CT_DB)
			continue;
		if (ctp->ct_envparent != db_ctp->ct_envparent)
			continue;
		if (type != DB_UNKNOWN && ctp->ct_dbdp.type != type)
			continue;
		if (ctp->ct_dbdp.dbflags != LF_ISSET(DB_SERVER_DBFLAGS))
			continue;
		if (db_ctp->ct_dbdp.setflags != 0 &&
		    ctp->ct_dbdp.setflags != db_ctp->ct_dbdp.setflags)
			continue;
		if (name == NULL || ctp->ct_dbdp.db == NULL ||
		    strcmp(name, ctp->ct_dbdp.db) != 0)
			continue;
		if (subdb != ctp->ct_dbdp.subdb &&
		    (subdb == NULL || ctp->ct_dbdp.subdb == NULL ||
		    strcmp(subdb, ctp->ct_dbdp.subdb) != 0))
			continue;
		/*
		 * If we get here, then we match.
		 */
		ctp->ct_refcount++;
		return (ctp);
	}

	return (NULL);
}

extern "C" ct_entry *
__dbsrv_shareenv(ct_entry *env_ctp, home_entry *home, u_int32_t flags)
{
	ct_entry *ctp;

	/*
	 * Check if we can share an env.  Criteria for sharing are:
	 * Must be an env ctp, obviously.
	 * Must share the same home env.
	 * All flags must match.
	 */
	for (ctp = LIST_FIRST(&__dbsrv_head); ctp != NULL;
	    ctp = LIST_NEXT(ctp, entries)) {
		/*
		 * Skip ourselves.
		 */
		if (ctp == env_ctp)
			continue;
		if (ctp->ct_type != CT_ENV)
			continue;
		if (ctp->ct_envdp.home != home)
			continue;
		if (ctp->ct_envdp.envflags != flags)
			continue;
		if (ctp->ct_envdp.onflags != env_ctp->ct_envdp.onflags)
			continue;
		if (ctp->ct_envdp.offflags != env_ctp->ct_envdp.offflags)
			continue;
		/*
		 * If we get here, then we match.  The only thing left to
		 * check is the timeout.  Since the server timeout set by
		 * the client is a hint, for sharing we'll give them the
		 * benefit of the doubt and grant them the longer timeout.
		 */
		if (ctp->ct_timeout < env_ctp->ct_timeout)
			ctp->ct_timeout = env_ctp->ct_timeout;
		ctp->ct_refcount++;
		return (ctp);
	}

	return (NULL);
}

extern "C" void
__dbsrv_active(ct_entry *ctp)
{
	time_t t;
	ct_entry *envctp;

	if (ctp == NULL)
		return;
	if ((t = time(NULL)) == -1)
		return;
	*(ctp->ct_activep) = t;
	if ((envctp = ctp->ct_envparent) == NULL)
		return;
	*(envctp->ct_activep) = t;
	return;
}

extern "C" int
__db_close_int(long id, u_int32_t flags)
{
	Db *dbp;
	int ret;
	ct_entry *ctp;

	ret = 0;
	ctp = get_tableent(id);
	if (ctp == NULL)
		return (DB_NOSERVER_ID);
	DB_ASSERT(ctp->ct_type == CT_DB);
	if (__dbsrv_verbose && ctp->ct_refcount != 1)
		printf("Deref'ing dbp id %ld, refcount %d\n",
		    id, ctp->ct_refcount);
	if (--ctp->ct_refcount != 0)
		return (ret);
	dbp = ctp->ct_dbp;
	if (__dbsrv_verbose)
		printf("Closing dbp id %ld\n", id);

	ret = dbp->close(flags);
	__dbdel_ctp(ctp);
	return (ret);
}

extern "C" int
__dbc_close_int(ct_entry *dbc_ctp)
{
	Dbc *dbc;
	int ret;
	ct_entry *ctp;

	dbc = (Dbc *)dbc_ctp->ct_anyp;

	ret = dbc->close();
	/*
	 * If this cursor is a join cursor then we need to fix up the
	 * cursors that it was joined from so that they are independent again.
	 */
	if (dbc_ctp->ct_type & CT_JOINCUR)
		for (ctp = LIST_FIRST(&__dbsrv_head); ctp != NULL;
		    ctp = LIST_NEXT(ctp, entries)) {
			/*
			 * Test if it is a join cursor, and if it is part
			 * of this one.
			 */
			if ((ctp->ct_type & CT_JOIN) &&
			    ctp->ct_activep == &dbc_ctp->ct_active) {
				ctp->ct_type &= ~CT_JOIN;
				ctp->ct_activep = ctp->ct_origp;
				__dbsrv_active(ctp);
			}
		}
	__dbclear_ctp(dbc_ctp);
	return (ret);

}

extern "C" int
__dbenv_close_int(long id, u_int32_t flags, int force)
{
	DbEnv *dbenv;
	int ret;
	ct_entry *ctp, *dbctp, *nextctp;

	ret = 0;
	ctp = get_tableent(id);
	if (ctp == NULL)
		return (DB_NOSERVER_ID);
	DB_ASSERT(ctp->ct_type == CT_ENV);
	if (__dbsrv_verbose && ctp->ct_refcount != 1)
		printf("Deref'ing env id %ld, refcount %d\n",
		    id, ctp->ct_refcount);
	/*
	 * If we are timing out, we need to force the close, no matter
	 * what the refcount.
	 */
	if (--ctp->ct_refcount != 0 && !force)
		return (ret);
	dbenv = ctp->ct_envp;
	if (__dbsrv_verbose)
		printf("Closing env id %ld\n", id);

	/*
	 * If we're timing out an env, we want to close all of its
	 * database handles as well.  All of the txns and cursors
	 * must have been timed out prior to timing out the env.
	 */
	if (force)
		for (dbctp = LIST_FIRST(&__dbsrv_head);
		    dbctp != NULL; dbctp = nextctp) {
			nextctp = LIST_NEXT(dbctp, entries);
			if (dbctp->ct_type != CT_DB)
				continue;
			if (dbctp->ct_envparent != ctp)
				continue;
			/*
			 * We found a DB handle that is part of this
			 * environment.  Close it.
			 */
			__db_close_int(dbctp->ct_id, 0);
			/*
			 * If we timed out a dbp, we may have removed
			 * multiple ctp entries.  Start over with a
			 * guaranteed good ctp.
			 */
			nextctp = LIST_FIRST(&__dbsrv_head);
		}

	ret = dbenv->close(flags);
	__dbdel_ctp(ctp);
	return (ret);
}

static int
add_home(char *home)
{
	home_entry *hp, *homep;
	int ret;

	if ((ret = __os_malloc(NULL, sizeof(home_entry), &hp)) != 0)
		return (ret);
	if ((ret = __os_malloc(NULL, strlen(home)+1, &hp->home)) != 0)
		return (ret);
	memcpy(hp->home, home, strlen(home)+1);
	hp->dir = home;
	hp->passwd = NULL;
	/*
	 * This loop is to remove any trailing path separators,
	 * to assure hp->name points to the last component.
	 */
	hp->name = __db_rpath(home);
	if (hp->name != NULL) {
		*(hp->name) = '\0';
		hp->name++;
	} else
		hp->name = home;
	while (*(hp->name) == '\0') {
		hp->name = __db_rpath(home);
		*(hp->name) = '\0';
		hp->name++;
	}
	/*
	 * Now we have successfully added it.  Make sure there are no
	 * identical names.
	 */
	for (homep = LIST_FIRST(&__dbsrv_home); homep != NULL;
	    homep = LIST_NEXT(homep, entries))
		if (strcmp(homep->name, hp->name) == 0) {
			printf("Already added home name %s, at directory %s\n",
			    hp->name, homep->dir);
			return (-1);
		}
	LIST_INSERT_HEAD(&__dbsrv_home, hp, entries);
	if (__dbsrv_verbose)
		printf("Added home %s in dir %s\n", hp->name, hp->dir);
	return (0);
}

static int
add_passwd(char *passwd)
{
	home_entry *hp;

	/*
	 * We add the passwd to the last given home dir.  If there
	 * isn't a home dir, or the most recent one already has a
	 * passwd, then there is a user error.
	 */
	hp = LIST_FIRST(&__dbsrv_home);
	if (hp == NULL || hp->passwd != NULL)
		return (EINVAL);
	/*
	 * We've already strdup'ed the passwd above, so we don't need
	 * to malloc new space, just point to it.
	 */
	hp->passwd = passwd;
	return (0);
}

extern "C" home_entry *
get_fullhome(char *name)
{
	home_entry *hp;

	if (name == NULL)
		return (NULL);

	for (hp = LIST_FIRST(&__dbsrv_home); hp != NULL;
	    hp = LIST_NEXT(hp, entries))
		if (strcmp(name, hp->name) == 0)
			return (hp);
	return (NULL);
}

static int
env_recover(char *progname)
{
	DbEnv *dbenv;
	home_entry *hp;
	u_int32_t flags;
	int exitval, ret;

	for (hp = LIST_FIRST(&__dbsrv_home); hp != NULL;
	    hp = LIST_NEXT(hp, entries)) {
		exitval = 0;
		dbenv = new DbEnv(DB_CXX_NO_EXCEPTIONS);
		if (__dbsrv_verbose == 1)
			(void)dbenv->set_verbose(DB_VERB_RECOVERY, 1);
		dbenv->set_errfile(stderr);
		dbenv->set_errpfx(progname);
		if (hp->passwd != NULL)
			(void)dbenv->set_encrypt(hp->passwd, DB_ENCRYPT_AES);

		/*
		 * Initialize the env with DB_RECOVER.  That is all we
		 * have to do to run recovery.
		 */
		if (__dbsrv_verbose)
			printf("Running recovery on %s\n", hp->home);
		flags = DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL |
		    DB_INIT_TXN | DB_USE_ENVIRON | DB_RECOVER;
		if ((ret = dbenv->open(hp->home, flags, 0)) != 0) {
			dbenv->err(ret, "DbEnv->open");
			goto error;
		}

		if (0) {
error:			exitval = 1;
		}
		if ((ret = dbenv->close(0)) != 0) {
			exitval = 1;
			fprintf(stderr, "%s: dbenv->close: %s\n",
			    progname, db_strerror(ret));
		}
		if (exitval)
			return (exitval);
	}
	return (0);
}
