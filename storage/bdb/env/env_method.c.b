/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: env_method.c,v 11.87 2002/08/29 14:22:21 margo Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_RPC
#include <rpc/rpc.h>
#endif

#include <string.h>
#endif

/*
 * This is the file that initializes the global array.  Do it this way because
 * people keep changing one without changing the other.  Having declaration and
 * initialization in one file will hopefully fix that.
 */
#define	DB_INITIALIZE_DB_GLOBALS	1

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/hmac.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/rep.h"
#include "dbinc/txn.h"

#ifdef HAVE_RPC
#include "dbinc_auto/db_server.h"
#include "dbinc_auto/rpc_client_ext.h"
#endif

static void __dbenv_err __P((const DB_ENV *, int, const char *, ...));
static void __dbenv_errx __P((const DB_ENV *, const char *, ...));
static int  __dbenv_init __P((DB_ENV *));
static int  __dbenv_set_alloc __P((DB_ENV *, void *(*)(size_t),
    void *(*)(void *, size_t), void (*)(void *)));
static int  __dbenv_set_app_dispatch __P((DB_ENV *,
    int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops)));
static int  __dbenv_set_data_dir __P((DB_ENV *, const char *));
static int  __dbenv_set_encrypt __P((DB_ENV *, const char *, u_int32_t));
static void __dbenv_set_errcall __P((DB_ENV *, void (*)(const char *, char *)));
static void __dbenv_set_errfile __P((DB_ENV *, FILE *));
static void __dbenv_set_errpfx __P((DB_ENV *, const char *));
static int  __dbenv_set_feedback __P((DB_ENV *, void (*)(DB_ENV *, int, int)));
static int  __dbenv_set_flags __P((DB_ENV *, u_int32_t, int));
static void __dbenv_set_noticecall __P((DB_ENV *, void (*)(DB_ENV *, db_notices)));
static int  __dbenv_set_paniccall __P((DB_ENV *, void (*)(DB_ENV *, int)));
static int  __dbenv_set_rpc_server_noclnt
    __P((DB_ENV *, void *, const char *, long, long, u_int32_t));
static int  __dbenv_set_shm_key __P((DB_ENV *, long));
static int  __dbenv_set_tas_spins __P((DB_ENV *, u_int32_t));
static int  __dbenv_set_tmp_dir __P((DB_ENV *, const char *));
static int  __dbenv_set_verbose __P((DB_ENV *, u_int32_t, int));

/*
 * db_env_create --
 *	DB_ENV constructor.
 *
 * EXTERN: int db_env_create __P((DB_ENV **, u_int32_t));
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
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 *
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
		__os_free(NULL, dbenv);
		return (ret);
	}

	*dbenvpp = dbenv;
	return (0);
}

/*
 * __dbenv_init --
 *	Initialize a DB_ENV structure.
 */
static int
__dbenv_init(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * !!!
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 *
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
		dbenv->dbremove = __dbcl_env_dbremove;
		dbenv->dbrename = __dbcl_env_dbrename;
		dbenv->open = __dbcl_env_open_wrap;
		dbenv->remove = __dbcl_env_remove;
		dbenv->set_alloc = __dbcl_env_alloc;
		dbenv->set_app_dispatch = __dbcl_set_app_dispatch;
		dbenv->set_data_dir = __dbcl_set_data_dir;
		dbenv->set_encrypt = __dbcl_env_encrypt;
		dbenv->set_feedback = __dbcl_env_set_feedback;
		dbenv->set_flags = __dbcl_env_flags;
		dbenv->set_noticecall = __dbcl_env_noticecall;
		dbenv->set_paniccall = __dbcl_env_paniccall;
		dbenv->set_rpc_server = __dbcl_envrpcserver;
		dbenv->set_shm_key = __dbcl_set_shm_key;
		dbenv->set_tas_spins = __dbcl_set_tas_spins;
		dbenv->set_timeout = __dbcl_set_timeout;
		dbenv->set_tmp_dir = __dbcl_set_tmp_dir;
		dbenv->set_verbose = __dbcl_set_verbose;
	} else {
#endif
		dbenv->close = __dbenv_close;
		dbenv->dbremove = __dbenv_dbremove;
		dbenv->dbrename = __dbenv_dbrename;
		dbenv->open = __dbenv_open;
		dbenv->remove = __dbenv_remove;
		dbenv->set_alloc = __dbenv_set_alloc;
		dbenv->set_app_dispatch = __dbenv_set_app_dispatch;
		dbenv->set_data_dir = __dbenv_set_data_dir;
		dbenv->set_encrypt = __dbenv_set_encrypt;
		dbenv->set_feedback = __dbenv_set_feedback;
		dbenv->set_flags = __dbenv_set_flags;
		dbenv->set_noticecall = __dbcl_env_noticecall;
		dbenv->set_paniccall = __dbenv_set_paniccall;
		dbenv->set_rpc_server = __dbenv_set_rpc_server_noclnt;
		dbenv->set_shm_key = __dbenv_set_shm_key;
		dbenv->set_tas_spins = __dbenv_set_tas_spins;
		dbenv->set_tmp_dir = __dbenv_set_tmp_dir;
		dbenv->set_verbose = __dbenv_set_verbose;
#ifdef	HAVE_RPC
	}
#endif
	dbenv->shm_key = INVALID_REGION_SEGID;
	dbenv->db_ref = 0;

	__log_dbenv_create(dbenv);		/* Subsystem specific. */
	__lock_dbenv_create(dbenv);
	__memp_dbenv_create(dbenv);
	__rep_dbenv_create(dbenv);
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
	DB_REAL_ERR(dbenv, error, 1, 1, fmt);
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
	DB_REAL_ERR(dbenv, 0, 0, 1, fmt);
}

static int
__dbenv_set_alloc(dbenv, mal_func, real_func, free_func)
	DB_ENV *dbenv;
	void *(*mal_func) __P((size_t));
	void *(*real_func) __P((void *, size_t));
	void (*free_func) __P((void *));
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_alloc");

	dbenv->db_malloc = mal_func;
	dbenv->db_realloc = real_func;
	dbenv->db_free = free_func;
	return (0);
}

/*
 * __dbenv_set_app_dispatch --
 *	Set the transaction abort recover function.
 */
static int
__dbenv_set_app_dispatch(dbenv, app_dispatch)
	DB_ENV *dbenv;
	int (*app_dispatch) __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_app_dispatch");

	dbenv->app_dispatch = app_dispatch;
	return (0);
}

static int
__dbenv_set_encrypt(dbenv, passwd, flags)
	DB_ENV *dbenv;
	const char *passwd;
	u_int32_t flags;
{
#ifdef HAVE_CRYPTO
	DB_CIPHER *db_cipher;
	int ret;

	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_encrypt");
#define	OK_CRYPTO_FLAGS	(DB_ENCRYPT_AES)

	if (flags != 0 && LF_ISSET(~OK_CRYPTO_FLAGS))
		return (__db_ferr(dbenv, "DB_ENV->set_encrypt", 0));

	if (passwd == NULL || strlen(passwd) == 0) {
		__db_err(dbenv, "Empty password specified to set_encrypt");
		return (EINVAL);
	}
	if (!CRYPTO_ON(dbenv)) {
		if ((ret = __os_calloc(dbenv, 1, sizeof(DB_CIPHER), &db_cipher))
		    != 0)
			goto err;
		dbenv->crypto_handle = db_cipher;
	} else
		db_cipher = (DB_CIPHER *)dbenv->crypto_handle;

	if (dbenv->passwd != NULL)
		__os_free(dbenv, dbenv->passwd);
	if ((ret = __os_strdup(dbenv, passwd, &dbenv->passwd)) != 0) {
		__os_free(dbenv, db_cipher);
		goto err;
	}
	/*
	 * We're going to need this often enough to keep around
	 */
	dbenv->passwd_len = strlen(dbenv->passwd) + 1;
	/*
	 * The MAC key is for checksumming, and is separate from
	 * the algorithm.  So initialize it here, even if they
	 * are using CIPHER_ANY.
	 */
	__db_derive_mac((u_int8_t *)dbenv->passwd,
	    dbenv->passwd_len, db_cipher->mac_key);
	switch (flags) {
	case 0:
		F_SET(db_cipher, CIPHER_ANY);
		break;
	case DB_ENCRYPT_AES:
		if ((ret = __crypto_algsetup(dbenv, db_cipher, CIPHER_AES, 0))
		    != 0)
			goto err1;
		break;
	}
	return (0);

err1:
	__os_free(dbenv, dbenv->passwd);
	__os_free(dbenv, db_cipher);
	dbenv->crypto_handle = NULL;
err:
	return (ret);
#else
	COMPQUIET(dbenv, NULL);
	COMPQUIET(passwd, NULL);
	COMPQUIET(flags, 0);

	return (__db_eopnotsup(dbenv));
#endif
}

static int
__dbenv_set_flags(dbenv, flags, onoff)
	DB_ENV *dbenv;
	u_int32_t flags;
	int onoff;
{
#define	OK_FLAGS							\
	(DB_AUTO_COMMIT | DB_CDB_ALLDB | DB_DIRECT_DB | DB_DIRECT_LOG |	\
	    DB_NOLOCKING | DB_NOMMAP | DB_NOPANIC | DB_OVERWRITE |	\
	    DB_PANIC_ENVIRONMENT | DB_REGION_INIT | DB_TXN_NOSYNC |	\
	    DB_TXN_WRITE_NOSYNC | DB_YIELDCPU)

	if (LF_ISSET(~OK_FLAGS))
		return (__db_ferr(dbenv, "DB_ENV->set_flags", 0));
	if (onoff && LF_ISSET(DB_TXN_WRITE_NOSYNC) && LF_ISSET(DB_TXN_NOSYNC))
		return (__db_ferr(dbenv, "DB_ENV->set_flags", 1));

	if (LF_ISSET(DB_AUTO_COMMIT)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_AUTO_COMMIT);
		else
			F_CLR(dbenv, DB_ENV_AUTO_COMMIT);
	}
	if (LF_ISSET(DB_CDB_ALLDB)) {
		ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_flags: DB_CDB_ALLDB");
		if (onoff)
			F_SET(dbenv, DB_ENV_CDB_ALLDB);
		else
			F_CLR(dbenv, DB_ENV_CDB_ALLDB);
	}
	if (LF_ISSET(DB_DIRECT_DB)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_DIRECT_DB);
		else
			F_CLR(dbenv, DB_ENV_DIRECT_DB);
	}
	if (LF_ISSET(DB_DIRECT_LOG)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_DIRECT_LOG);
		else
			F_CLR(dbenv, DB_ENV_DIRECT_LOG);
	}
	if (LF_ISSET(DB_NOLOCKING)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_NOLOCKING);
		else
			F_CLR(dbenv, DB_ENV_NOLOCKING);
	}
	if (LF_ISSET(DB_NOMMAP)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_NOMMAP);
		else
			F_CLR(dbenv, DB_ENV_NOMMAP);
	}
	if (LF_ISSET(DB_NOPANIC)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_NOPANIC);
		else
			F_CLR(dbenv, DB_ENV_NOPANIC);
	}
	if (LF_ISSET(DB_OVERWRITE)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_OVERWRITE);
		else
			F_CLR(dbenv, DB_ENV_OVERWRITE);
	}
	if (LF_ISSET(DB_PANIC_ENVIRONMENT)) {
		ENV_ILLEGAL_BEFORE_OPEN(dbenv,
		    "set_flags: DB_PANIC_ENVIRONMENT");
		PANIC_SET(dbenv, onoff);
	}
	if (LF_ISSET(DB_REGION_INIT)) {
		ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_flags: DB_REGION_INIT");
		if (onoff)
			F_SET(dbenv, DB_ENV_REGION_INIT);
		else
			F_CLR(dbenv, DB_ENV_REGION_INIT);
	}
	if (LF_ISSET(DB_TXN_NOSYNC)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_TXN_NOSYNC);
		else
			F_CLR(dbenv, DB_ENV_TXN_NOSYNC);
	}
	if (LF_ISSET(DB_TXN_WRITE_NOSYNC)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_TXN_WRITE_NOSYNC);
		else
			F_CLR(dbenv, DB_ENV_TXN_WRITE_NOSYNC);
	}
	if (LF_ISSET(DB_YIELDCPU)) {
		if (onoff)
			F_SET(dbenv, DB_ENV_YIELDCPU);
		else
			F_CLR(dbenv, DB_ENV_YIELDCPU);
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
		    &dbenv->db_data_dir)) != 0)
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
__dbenv_set_paniccall(dbenv, paniccall)
	DB_ENV *dbenv;
	void (*paniccall) __P((DB_ENV *, int));
{
	dbenv->db_paniccall = paniccall;
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
__dbenv_set_tas_spins(dbenv, tas_spins)
	DB_ENV *dbenv;
	u_int32_t tas_spins;
{
	dbenv->tas_spins = tas_spins;
	return (0);
}

static int
__dbenv_set_tmp_dir(dbenv, dir)
	DB_ENV *dbenv;
	const char *dir;
{
	if (dbenv->db_tmp_dir != NULL)
		__os_free(dbenv, dbenv->db_tmp_dir);
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
	case DB_VERB_REPLICATION:
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
	__db_err(dbenv, "%s: method not permitted in shared environment", name);
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
	__db_err(dbenv, "%s: method not permitted %s open",
	    name, after ? "after" : "before");
	return (EINVAL);
}

/*
 * __db_env_config --
 *	Method or function called without required configuration.
 *
 * PUBLIC: int __db_env_config __P((DB_ENV *, char *, u_int32_t));
 */
int
__db_env_config(dbenv, i, flags)
	DB_ENV *dbenv;
	char *i;
	u_int32_t flags;
{
	char *sub;

	switch (flags) {
	case DB_INIT_LOCK:
		sub = "locking";
		break;
	case DB_INIT_LOG:
		sub = "logging";
		break;
	case DB_INIT_MPOOL:
		sub = "memory pool";
		break;
	case DB_INIT_TXN:
		sub = "transaction";
		break;
	default:
		sub = "<unspecified>";
		break;
	}
	__db_err(dbenv,
    "%s interface requires an environment configured for the %s subsystem",
	    i, sub);
	return (EINVAL);
}

static int
__dbenv_set_rpc_server_noclnt(dbenv, cl, host, tsec, ssec, flags)
	DB_ENV *dbenv;
	void *cl;
	const char *host;
	long tsec, ssec;
	u_int32_t flags;
{
	COMPQUIET(host, NULL);
	COMPQUIET(cl, NULL);
	COMPQUIET(tsec, 0);
	COMPQUIET(ssec, 0);
	COMPQUIET(flags, 0);

	__db_err(dbenv,
	    "set_rpc_server method not permitted in non-RPC environment");
	return (__db_eopnotsup(dbenv));
}
