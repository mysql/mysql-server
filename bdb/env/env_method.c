/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: env_method.c,v 11.31 2000/11/30 00:58:35 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#ifdef HAVE_RPC
#include "db_server.h"
#endif

/*
 * This is the file that initializes the global array.  Do it this way because
 * people keep changing one without changing the other.  Having declaration and
 * initialization in one file will hopefully fix that.
 */
#define	DB_INITIALIZE_DB_GLOBALS	1

#include "db_int.h"
#include "db_shash.h"
#include "db_page.h"
#include "db_am.h"
#include "lock.h"
#include "log.h"
#include "mp.h"
#include "txn.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static void __dbenv_err __P((const DB_ENV *, int, const char *, ...));
static void __dbenv_errx __P((const DB_ENV *, const char *, ...));
static int  __dbenv_set_data_dir __P((DB_ENV *, const char *));
static void __dbenv_set_errcall __P((DB_ENV *, void (*)(const char *, char *)));
static void __dbenv_set_errfile __P((DB_ENV *, FILE *));
static void __dbenv_set_errpfx __P((DB_ENV *, const char *));
static int  __dbenv_set_feedback __P((DB_ENV *, void (*)(DB_ENV *, int, int)));
static int  __dbenv_set_flags __P((DB_ENV *, u_int32_t, int));
static int  __dbenv_set_mutexlocks __P((DB_ENV *, int));
static void __dbenv_set_noticecall
    __P((DB_ENV *, void (*)(DB_ENV *, db_notices)));
static int  __dbenv_set_paniccall __P((DB_ENV *, void (*)(DB_ENV *, int)));
static int  __dbenv_set_recovery_init __P((DB_ENV *, int (*)(DB_ENV *)));
static int  __dbenv_set_server_noclnt
    __P((DB_ENV *, char *, long, long, u_int32_t));
static int  __dbenv_set_shm_key __P((DB_ENV *, long));
static int  __dbenv_set_tmp_dir __P((DB_ENV *, const char *));
static int  __dbenv_set_verbose __P((DB_ENV *, u_int32_t, int));

/*
 * db_env_create --
 *	DB_ENV constructor.
 */
int
db_env_create(dbenvpp, flags)
	DB_ENV **dbenvpp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret;

	/*
	 * !!!
	 * We can't call the flags-checking routines, we don't have an
	 * environment yet.
	 */
	if (flags != 0 && flags != DB_CLIENT)
		return (EINVAL);

	if ((ret = __os_calloc(NULL, 1, sizeof(*dbenv), &dbenv)) != 0)
		return (ret);

#ifdef HAVE_RPC
	if (LF_ISSET(DB_CLIENT))
		F_SET(dbenv, DB_ENV_RPCCLIENT);
#endif
	ret = __dbenv_init(dbenv);

	if (ret != 0) {
		__os_free(dbenv, sizeof(*dbenv));
		return (ret);
	}

	*dbenvpp = dbenv;
	return (0);
}

/*
 * __dbenv_init --
 *	Initialize a DB_ENV structure.
 *
 * PUBLIC: int  __dbenv_init __P((DB_ENV *));
 */
int
__dbenv_init(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * Set up methods that are the same in both normal and RPC
	 */
	dbenv->err = __dbenv_err;
	dbenv->errx = __dbenv_errx;
	dbenv->set_errcall = __dbenv_set_errcall;
	dbenv->set_errfile = __dbenv_set_errfile;
	dbenv->set_errpfx = __dbenv_set_errpfx;

#ifdef	HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->close = __dbcl_env_close;
		dbenv->open = __dbcl_env_open;
		dbenv->remove = __dbcl_env_remove;
		dbenv->set_data_dir = __dbcl_set_data_dir;
		dbenv->set_feedback = __dbcl_env_set_feedback;
		dbenv->set_flags = __dbcl_env_flags;
		dbenv->set_mutexlocks = __dbcl_set_mutex_locks;
		dbenv->set_noticecall = __dbcl_env_noticecall;
		dbenv->set_paniccall = __dbcl_env_paniccall;
		dbenv->set_recovery_init = __dbcl_set_recovery_init;
		dbenv->set_server = __dbcl_envserver;
		dbenv->set_shm_key = __dbcl_set_shm_key;
		dbenv->set_tmp_dir = __dbcl_set_tmp_dir;
		dbenv->set_verbose = __dbcl_set_verbose;
	} else {
#endif
		dbenv->close = __dbenv_close;
		dbenv->open = __dbenv_open;
		dbenv->remove = __dbenv_remove;
		dbenv->set_data_dir = __dbenv_set_data_dir;
		dbenv->set_feedback = __dbenv_set_feedback;
		dbenv->set_flags = __dbenv_set_flags;
		dbenv->set_mutexlocks = __dbenv_set_mutexlocks;
		dbenv->set_noticecall = __dbenv_set_noticecall;
		dbenv->set_paniccall = __dbenv_set_paniccall;
		dbenv->set_recovery_init = __dbenv_set_recovery_init;
		dbenv->set_server = __dbenv_set_server_noclnt;
		dbenv->set_shm_key = __dbenv_set_shm_key;
		dbenv->set_tmp_dir = __dbenv_set_tmp_dir;
		dbenv->set_verbose = __dbenv_set_verbose;
#ifdef	HAVE_RPC
	}
#endif
	dbenv->shm_key = INVALID_REGION_SEGID;
	dbenv->db_mutexlocks = 1;

	__log_dbenv_create(dbenv);		/* Subsystem specific. */
	__lock_dbenv_create(dbenv);
	__memp_dbenv_create(dbenv);
	__txn_dbenv_create(dbenv);

	return (0);
}

/*
 * __dbenv_err --
 *	Error message, including the standard error string.
 */
static void
#ifdef __STDC__
__dbenv_err(const DB_ENV *dbenv, int error, const char *fmt, ...)
#else
__dbenv_err(dbenv, error, fmt, va_alist)
	const DB_ENV *dbenv;
	int error;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	__db_real_err(dbenv, error, 1, 1, fmt, ap);

	va_end(ap);
}

/*
 * __dbenv_errx --
 *	Error message.
 */
static void
#ifdef __STDC__
__dbenv_errx(const DB_ENV *dbenv, const char *fmt, ...)
#else
__dbenv_errx(dbenv, fmt, va_alist)
	const DB_ENV *dbenv;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	__db_real_err(dbenv, 0, 0, 1, fmt, ap);

	va_end(ap);
}

static int
__dbenv_set_flags(dbenv, flags, onoff)
	DB_ENV *dbenv;
	u_int32_t flags;
	int onoff;
{
#define	OK_FLAGS	(DB_CDB_ALLDB | DB_NOMMAP | DB_TXN_NOSYNC)

	if (LF_ISSET(~OK_FLAGS))
		return (__db_ferr(dbenv, "DBENV->set_flags", 0));

	if (LF_ISSET(DB_CDB_ALLDB)) {
		ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_flags: DB_CDB_ALLDB");
		if (onoff)
			F_SET(dbenv, DB_ENV_CDB_ALLDB);
		else
			F_CLR(dbenv, DB_ENV_CDB_ALLDB);
	}
	if (LF_ISSET(DB_NOMMAP)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_NOMMAP);
		else
			F_CLR(dbenv, DB_ENV_NOMMAP);
	}
	if (LF_ISSET(DB_TXN_NOSYNC)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_TXN_NOSYNC);
		else
			F_CLR(dbenv, DB_ENV_TXN_NOSYNC);
	}
	return (0);
}

static int
__dbenv_set_data_dir(dbenv, dir)
	DB_ENV *dbenv;
	const char *dir;
{
	int ret;

#define	DATA_INIT_CNT	20			/* Start with 20 data slots. */
	if (dbenv->db_data_dir == NULL) {
		if ((ret = __os_calloc(dbenv, DATA_INIT_CNT,
		    sizeof(char **), &dbenv->db_data_dir)) != 0)
			return (ret);
		dbenv->data_cnt = DATA_INIT_CNT;
	} else if (dbenv->data_next == dbenv->data_cnt - 1) {
		dbenv->data_cnt *= 2;
		if ((ret = __os_realloc(dbenv,
		    dbenv->data_cnt * sizeof(char **),
		    NULL, &dbenv->db_data_dir)) != 0)
			return (ret);
	}
	return (__os_strdup(dbenv,
	    dir, &dbenv->db_data_dir[dbenv->data_next++]));
}

static void
__dbenv_set_errcall(dbenv, errcall)
	DB_ENV *dbenv;
	void (*errcall) __P((const char *, char *));
{
	dbenv->db_errcall = errcall;
}

static void
__dbenv_set_errfile(dbenv, errfile)
	DB_ENV *dbenv;
	FILE *errfile;
{
	dbenv->db_errfile = errfile;
}

static void
__dbenv_set_errpfx(dbenv, errpfx)
	DB_ENV *dbenv;
	const char *errpfx;
{
	dbenv->db_errpfx = errpfx;
}

static int
__dbenv_set_feedback(dbenv, feedback)
	DB_ENV *dbenv;
	void (*feedback) __P((DB_ENV *, int, int));
{
	dbenv->db_feedback = feedback;
	return (0);
}

static void
__dbenv_set_noticecall(dbenv, noticecall)
	DB_ENV *dbenv;
	void (*noticecall) __P((DB_ENV *, db_notices));
{
	dbenv->db_noticecall = noticecall;
}

static int
__dbenv_set_mutexlocks(dbenv, onoff)
	DB_ENV *dbenv;
	int onoff;
{
	dbenv->db_mutexlocks = onoff;
	return (0);
}

static int
__dbenv_set_paniccall(dbenv, paniccall)
	DB_ENV *dbenv;
	void (*paniccall) __P((DB_ENV *, int));
{
	dbenv->db_paniccall = paniccall;
	return (0);
}

static int
__dbenv_set_recovery_init(dbenv, recovery_init)
	DB_ENV *dbenv;
	int (*recovery_init) __P((DB_ENV *));
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_recovery_init");

	dbenv->db_recovery_init = recovery_init;

	return (0);
}

static int
__dbenv_set_shm_key(dbenv, shm_key)
	DB_ENV *dbenv;
	long shm_key;			/* !!!: really a key_t. */
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_shm_key");

	dbenv->shm_key = shm_key;
	return (0);
}

static int
__dbenv_set_tmp_dir(dbenv, dir)
	DB_ENV *dbenv;
	const char *dir;
{
	if (dbenv->db_tmp_dir != NULL)
		__os_freestr(dbenv->db_tmp_dir);
	return (__os_strdup(dbenv, dir, &dbenv->db_tmp_dir));
}

static int
__dbenv_set_verbose(dbenv, which, onoff)
	DB_ENV *dbenv;
	u_int32_t which;
	int onoff;
{
	switch (which) {
	case DB_VERB_CHKPOINT:
	case DB_VERB_DEADLOCK:
	case DB_VERB_RECOVERY:
	case DB_VERB_WAITSFOR:
		if (onoff)
			FLD_SET(dbenv->verbose, which);
		else
			FLD_CLR(dbenv->verbose, which);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * __db_mi_env --
 *	Method illegally called with public environment.
 *
 * PUBLIC: int __db_mi_env __P((DB_ENV *, const char *));
 */
int
__db_mi_env(dbenv, name)
	DB_ENV *dbenv;
	const char *name;
{
	__db_err(dbenv, "%s: method meaningless in shared environment", name);
	return (EINVAL);
}

/*
 * __db_mi_open --
 *	Method illegally called after open.
 *
 * PUBLIC: int __db_mi_open __P((DB_ENV *, const char *, int));
 */
int
__db_mi_open(dbenv, name, after)
	DB_ENV *dbenv;
	const char *name;
	int after;
{
	__db_err(dbenv,
	    "%s: method meaningless %s open", name, after ? "after" : "before");
	return (EINVAL);
}

/*
 * __db_env_config --
 *	Method or function called without subsystem being configured.
 *
 * PUBLIC: int __db_env_config __P((DB_ENV *, int));
 */
int
__db_env_config(dbenv, subsystem)
	DB_ENV *dbenv;
	int subsystem;
{
	const char *name;

	switch (subsystem) {
	case DB_INIT_LOCK:
		name = "lock";
		break;
	case DB_INIT_LOG:
		name = "log";
		break;
	case DB_INIT_MPOOL:
		name = "mpool";
		break;
	case DB_INIT_TXN:
		name = "txn";
		break;
	default:
		name = "unknown";
		break;
	}
	__db_err(dbenv,
    "%s interface called with environment not configured for that subsystem",
	    name);
	return (EINVAL);
}

static int
__dbenv_set_server_noclnt(dbenv, host, tsec, ssec, flags)
	DB_ENV *dbenv;
	char *host;
	long tsec, ssec;
	u_int32_t flags;
{
	COMPQUIET(host, NULL);
	COMPQUIET(tsec, 0);
	COMPQUIET(ssec, 0);
	COMPQUIET(flags, 0);

	__db_err(dbenv, "set_server method meaningless in non-RPC enviroment");
	return (__db_eopnotsup(dbenv));
}
