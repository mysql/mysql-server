/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: env_method.c,v 11.136 2004/10/11 18:47:50 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_RPC
#include <rpc/rpc.h>
#endif

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
#include "dbinc/crypto.h"
#include "dbinc/hmac.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

#ifdef HAVE_RPC
#include "dbinc_auto/rpc_client_ext.h"
#endif

static void __dbenv_err __P((const DB_ENV *, int, const char *, ...));
static void __dbenv_errx __P((const DB_ENV *, const char *, ...));
static int  __dbenv_get_data_dirs __P((DB_ENV *, const char ***));
static int  __dbenv_get_flags __P((DB_ENV *, u_int32_t *));
static int  __dbenv_get_home __P((DB_ENV *, const char **));
static int  __dbenv_get_shm_key __P((DB_ENV *, long *));
static int  __dbenv_get_tas_spins __P((DB_ENV *, u_int32_t *));
static int  __dbenv_get_tmp_dir __P((DB_ENV *, const char **));
static int  __dbenv_get_verbose __P((DB_ENV *, u_int32_t, int *));
static int  __dbenv_init __P((DB_ENV *));
static void __dbenv_map_flags __P((DB_ENV *, u_int32_t *, u_int32_t *));
static int  __dbenv_set_app_dispatch
		__P((DB_ENV *, int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops)));
static int  __dbenv_set_feedback __P((DB_ENV *, void (*)(DB_ENV *, int, int)));
static int  __dbenv_set_rpc_server_noclnt
		__P((DB_ENV *, void *, const char *, long, long, u_int32_t));

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
	if (flags != 0 && !LF_ISSET(DB_RPCCLIENT))
		return (EINVAL);
	if ((ret = __os_calloc(NULL, 1, sizeof(*dbenv), &dbenv)) != 0)
		return (ret);

#ifdef HAVE_RPC
	if (LF_ISSET(DB_RPCCLIENT))
		F_SET(dbenv, DB_ENV_RPCCLIENT);
#endif
	if ((ret = __dbenv_init(dbenv)) != 0) {
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
	dbenv->get_errfile = __dbenv_get_errfile;
	dbenv->set_errfile = __dbenv_set_errfile;
	dbenv->get_errpfx = __dbenv_get_errpfx;
	dbenv->set_errpfx = __dbenv_set_errpfx;
	dbenv->set_msgcall = __dbenv_set_msgcall;
	dbenv->get_msgfile = __dbenv_get_msgfile;
	dbenv->set_msgfile = __dbenv_set_msgfile;

#ifdef	HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->close = __dbcl_env_close_wrap;
		dbenv->dbremove = __dbcl_env_dbremove;
		dbenv->dbrename = __dbcl_env_dbrename;
		dbenv->get_home = __dbcl_env_get_home;
		dbenv->get_open_flags = __dbcl_env_get_open_flags;
		dbenv->open = __dbcl_env_open_wrap;
		dbenv->remove = __dbcl_env_remove;
		dbenv->stat_print = NULL;

		dbenv->fileid_reset = NULL;
		dbenv->is_bigendian = NULL;
		dbenv->lsn_reset = NULL;
		dbenv->prdbt = NULL;

		dbenv->set_alloc = __dbcl_env_alloc;
		dbenv->set_app_dispatch = __dbcl_set_app_dispatch;
		dbenv->get_data_dirs = __dbcl_get_data_dirs;
		dbenv->set_data_dir = __dbcl_set_data_dir;
		dbenv->get_encrypt_flags = __dbcl_env_get_encrypt_flags;
		dbenv->set_encrypt = __dbcl_env_encrypt;
		dbenv->set_feedback = __dbcl_env_set_feedback;
		dbenv->get_flags = __dbcl_env_get_flags;
		dbenv->set_flags = __dbcl_env_flags;
		dbenv->set_noticecall = __dbcl_env_noticecall;
		dbenv->set_paniccall = __dbcl_env_paniccall;
		dbenv->set_rpc_server = __dbcl_envrpcserver;
		dbenv->get_shm_key = __dbcl_get_shm_key;
		dbenv->set_shm_key = __dbcl_set_shm_key;
		dbenv->get_tas_spins = __dbcl_get_tas_spins;
		dbenv->set_tas_spins = __dbcl_set_tas_spins;
		dbenv->get_timeout = __dbcl_get_timeout;
		dbenv->set_timeout = __dbcl_set_timeout;
		dbenv->get_tmp_dir = __dbcl_get_tmp_dir;
		dbenv->set_tmp_dir = __dbcl_set_tmp_dir;
		dbenv->get_verbose = __dbcl_get_verbose;
		dbenv->set_verbose = __dbcl_set_verbose;
	} else {
#endif
		dbenv->close = __dbenv_close_pp;
		dbenv->dbremove = __dbenv_dbremove_pp;
		dbenv->dbrename = __dbenv_dbrename_pp;
		dbenv->open = __dbenv_open;
		dbenv->remove = __dbenv_remove;
		dbenv->stat_print = __dbenv_stat_print_pp;

		dbenv->fileid_reset = __db_fileid_reset;
		dbenv->is_bigendian = __db_isbigendian;
		dbenv->lsn_reset = __db_lsn_reset;
		dbenv->prdbt = __db_prdbt;

		dbenv->get_home = __dbenv_get_home;
		dbenv->get_open_flags = __dbenv_get_open_flags;
		dbenv->set_alloc = __dbenv_set_alloc;
		dbenv->set_app_dispatch = __dbenv_set_app_dispatch;
		dbenv->get_data_dirs = __dbenv_get_data_dirs;
		dbenv->set_data_dir = __dbenv_set_data_dir;
		dbenv->get_encrypt_flags = __dbenv_get_encrypt_flags;
		dbenv->set_encrypt = __dbenv_set_encrypt;
		dbenv->set_feedback = __dbenv_set_feedback;
		dbenv->get_flags = __dbenv_get_flags;
		dbenv->set_flags = __dbenv_set_flags;
		dbenv->set_intermediate_dir = __dbenv_set_intermediate_dir;
		dbenv->set_noticecall = __dbenv_set_noticecall;
		dbenv->set_paniccall = __dbenv_set_paniccall;
		dbenv->set_rpc_server = __dbenv_set_rpc_server_noclnt;
		dbenv->get_shm_key = __dbenv_get_shm_key;
		dbenv->set_shm_key = __dbenv_set_shm_key;
		dbenv->get_tas_spins = __dbenv_get_tas_spins;
		dbenv->set_tas_spins = __dbenv_set_tas_spins;
		dbenv->get_tmp_dir = __dbenv_get_tmp_dir;
		dbenv->set_tmp_dir = __dbenv_set_tmp_dir;
		dbenv->get_verbose = __dbenv_get_verbose;
		dbenv->set_verbose = __dbenv_set_verbose;
#ifdef	HAVE_RPC
	}
#endif
	dbenv->shm_key = INVALID_REGION_SEGID;
	dbenv->db_ref = 0;

	__os_spin(dbenv);

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
#ifdef STDC_HEADERS
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
#ifdef STDC_HEADERS
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
__dbenv_get_home(dbenv, homep)
	DB_ENV *dbenv;
	const char **homep;
{
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->get_home");
	*homep = dbenv->db_home;
	return (0);
}

/*
 * __dbenv_set_alloc --
 *	{DB_ENV,DB}->set_alloc.
 *
 * PUBLIC: int  __dbenv_set_alloc __P((DB_ENV *, void *(*)(size_t),
 * PUBLIC:          void *(*)(void *, size_t), void (*)(void *)));
 */
int
__dbenv_set_alloc(dbenv, mal_func, real_func, free_func)
	DB_ENV *dbenv;
	void *(*mal_func) __P((size_t));
	void *(*real_func) __P((void *, size_t));
	void (*free_func) __P((void *));
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_alloc");

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
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_app_dispatch");

	dbenv->app_dispatch = app_dispatch;
	return (0);
}

/*
 * __dbenv_get_encrypt_flags --
 *	{DB_ENV,DB}->get_encrypt_flags.
 *
 * PUBLIC: int __dbenv_get_encrypt_flags __P((DB_ENV *, u_int32_t *));
 */
int
__dbenv_get_encrypt_flags(dbenv, flagsp)
	DB_ENV *dbenv;
	u_int32_t *flagsp;
{
#ifdef HAVE_CRYPTO
	DB_CIPHER *db_cipher;

	db_cipher = dbenv->crypto_handle;
	if (db_cipher != NULL && db_cipher->alg == CIPHER_AES)
		*flagsp = DB_ENCRYPT_AES;
	else
		*flagsp = 0;
	return (0);
#else
	COMPQUIET(flagsp, 0);
	__db_err(dbenv,
	    "library build did not include support for cryptography");
	return (DB_OPNOTSUP);
#endif
}

/*
 * __dbenv_set_encrypt --
 *	DB_ENV->set_encrypt.
 *
 * PUBLIC: int __dbenv_set_encrypt __P((DB_ENV *, const char *, u_int32_t));
 */
int
__dbenv_set_encrypt(dbenv, passwd, flags)
	DB_ENV *dbenv;
	const char *passwd;
	u_int32_t flags;
{
#ifdef HAVE_CRYPTO
	DB_CIPHER *db_cipher;
	int ret;

	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_encrypt");
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
	default:				/* Impossible. */
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
	COMPQUIET(passwd, NULL);
	COMPQUIET(flags, 0);

	__db_err(dbenv,
	    "library build did not include support for cryptography");
	return (DB_OPNOTSUP);
#endif
}

static void
__dbenv_map_flags(dbenv, inflagsp, outflagsp)
	DB_ENV *dbenv;
	u_int32_t *inflagsp, *outflagsp;
{
	COMPQUIET(dbenv, NULL);

	if (FLD_ISSET(*inflagsp, DB_AUTO_COMMIT)) {
		FLD_SET(*outflagsp, DB_ENV_AUTO_COMMIT);
		FLD_CLR(*inflagsp, DB_AUTO_COMMIT);
	}
	if (FLD_ISSET(*inflagsp, DB_CDB_ALLDB)) {
		FLD_SET(*outflagsp, DB_ENV_CDB_ALLDB);
		FLD_CLR(*inflagsp, DB_CDB_ALLDB);
	}
	if (FLD_ISSET(*inflagsp, DB_DIRECT_DB)) {
		FLD_SET(*outflagsp, DB_ENV_DIRECT_DB);
		FLD_CLR(*inflagsp, DB_DIRECT_DB);
	}
	if (FLD_ISSET(*inflagsp, DB_DIRECT_LOG)) {
		FLD_SET(*outflagsp, DB_ENV_DIRECT_LOG);
		FLD_CLR(*inflagsp, DB_DIRECT_LOG);
	}
	if (FLD_ISSET(*inflagsp, DB_DSYNC_LOG)) {
		FLD_SET(*outflagsp, DB_ENV_DSYNC_LOG);
		FLD_CLR(*inflagsp, DB_DSYNC_LOG);
	}
	if (FLD_ISSET(*inflagsp, DB_LOG_AUTOREMOVE)) {
		FLD_SET(*outflagsp, DB_ENV_LOG_AUTOREMOVE);
		FLD_CLR(*inflagsp, DB_LOG_AUTOREMOVE);
	}
	if (FLD_ISSET(*inflagsp, DB_LOG_INMEMORY)) {
		FLD_SET(*outflagsp, DB_ENV_LOG_INMEMORY);
		FLD_CLR(*inflagsp, DB_LOG_INMEMORY);
	}
	if (FLD_ISSET(*inflagsp, DB_NOLOCKING)) {
		FLD_SET(*outflagsp, DB_ENV_NOLOCKING);
		FLD_CLR(*inflagsp, DB_NOLOCKING);
	}
	if (FLD_ISSET(*inflagsp, DB_NOMMAP)) {
		FLD_SET(*outflagsp, DB_ENV_NOMMAP);
		FLD_CLR(*inflagsp, DB_NOMMAP);
	}
	if (FLD_ISSET(*inflagsp, DB_NOPANIC)) {
		FLD_SET(*outflagsp, DB_ENV_NOPANIC);
		FLD_CLR(*inflagsp, DB_NOPANIC);
	}
	if (FLD_ISSET(*inflagsp, DB_OVERWRITE)) {
		FLD_SET(*outflagsp, DB_ENV_OVERWRITE);
		FLD_CLR(*inflagsp, DB_OVERWRITE);
	}
	if (FLD_ISSET(*inflagsp, DB_REGION_INIT)) {
		FLD_SET(*outflagsp, DB_ENV_REGION_INIT);
		FLD_CLR(*inflagsp, DB_REGION_INIT);
	}
	if (FLD_ISSET(*inflagsp, DB_TIME_NOTGRANTED)) {
		FLD_SET(*outflagsp, DB_ENV_TIME_NOTGRANTED);
		FLD_CLR(*inflagsp, DB_TIME_NOTGRANTED);
	}
	if (FLD_ISSET(*inflagsp, DB_TXN_NOSYNC)) {
		FLD_SET(*outflagsp, DB_ENV_TXN_NOSYNC);
		FLD_CLR(*inflagsp, DB_TXN_NOSYNC);
	}
	if (FLD_ISSET(*inflagsp, DB_TXN_WRITE_NOSYNC)) {
		FLD_SET(*outflagsp, DB_ENV_TXN_WRITE_NOSYNC);
		FLD_CLR(*inflagsp, DB_TXN_WRITE_NOSYNC);
	}
	if (FLD_ISSET(*inflagsp, DB_YIELDCPU)) {
		FLD_SET(*outflagsp, DB_ENV_YIELDCPU);
		FLD_CLR(*inflagsp, DB_YIELDCPU);
	}
}

static int
__dbenv_get_flags(dbenv, flagsp)
	DB_ENV *dbenv;
	u_int32_t *flagsp;
{
	static const u_int32_t env_flags[] = {
		DB_AUTO_COMMIT,
		DB_CDB_ALLDB,
		DB_DIRECT_DB,
		DB_DIRECT_LOG,
		DB_DSYNC_LOG,
		DB_LOG_AUTOREMOVE,
		DB_LOG_INMEMORY,
		DB_NOLOCKING,
		DB_NOMMAP,
		DB_NOPANIC,
		DB_OVERWRITE,
		DB_REGION_INIT,
		DB_TIME_NOTGRANTED,
		DB_TXN_NOSYNC,
		DB_TXN_WRITE_NOSYNC,
		DB_YIELDCPU,
		0
	};
	u_int32_t f, flags, mapped_flag;
	int i;

	flags = 0;
	for (i = 0; (f = env_flags[i]) != 0; i++) {
		mapped_flag = 0;
		__dbenv_map_flags(dbenv, &f, &mapped_flag);
		DB_ASSERT(f == 0);
		if (F_ISSET(dbenv, mapped_flag) == mapped_flag)
			LF_SET(env_flags[i]);
	}

	/* Some flags are persisted in the regions. */
	if (dbenv->reginfo != NULL &&
	    ((REGENV *)((REGINFO *)dbenv->reginfo)->primary)->envpanic != 0) {
		LF_SET(DB_PANIC_ENVIRONMENT);
	}
	__log_get_flags(dbenv, &flags);

	*flagsp = flags;
	return (0);
}

/*
 * __dbenv_set_flags --
 *	DB_ENV->set_flags.
 *
 * PUBLIC: int  __dbenv_set_flags __P((DB_ENV *, u_int32_t, int));
 */
int
__dbenv_set_flags(dbenv, flags, on)
	DB_ENV *dbenv;
	u_int32_t flags;
	int on;
{
	u_int32_t mapped_flags;
	int ret;

#define	OK_FLAGS							\
	(DB_AUTO_COMMIT | DB_CDB_ALLDB | DB_DIRECT_DB | DB_DIRECT_LOG |	\
	    DB_DSYNC_LOG | DB_LOG_AUTOREMOVE | DB_LOG_INMEMORY | \
	    DB_NOLOCKING | DB_NOMMAP | DB_NOPANIC | DB_OVERWRITE | \
	    DB_PANIC_ENVIRONMENT | DB_REGION_INIT | DB_TIME_NOTGRANTED | \
	    DB_TXN_NOSYNC | DB_TXN_WRITE_NOSYNC | DB_YIELDCPU)

	if (LF_ISSET(~OK_FLAGS))
		return (__db_ferr(dbenv, "DB_ENV->set_flags", 0));
	if (on) {
		if ((ret = __db_fcchk(dbenv, "DB_ENV->set_flags",
		    flags, DB_LOG_INMEMORY, DB_TXN_NOSYNC)) != 0)
			return (ret);
		if ((ret = __db_fcchk(dbenv, "DB_ENV->set_flags",
		    flags, DB_LOG_INMEMORY, DB_TXN_WRITE_NOSYNC)) != 0)
			return (ret);
		if ((ret = __db_fcchk(dbenv, "DB_ENV->set_flags",
		    flags, DB_TXN_NOSYNC, DB_TXN_WRITE_NOSYNC)) != 0)
			return (ret);
		if (LF_ISSET(DB_DIRECT_DB |
		    DB_DIRECT_LOG) && __os_have_direct() == 0) {
			__db_err(dbenv,
	"DB_ENV->set_flags: direct I/O either not configured or not supported");
			return (EINVAL);
		}
	}

	if (LF_ISSET(DB_CDB_ALLDB))
		ENV_ILLEGAL_AFTER_OPEN(dbenv,
		    "DB_ENV->set_flags: DB_CDB_ALLDB");
	if (LF_ISSET(DB_PANIC_ENVIRONMENT)) {
		ENV_ILLEGAL_BEFORE_OPEN(dbenv,
		    "DB_ENV->set_flags: DB_PANIC_ENVIRONMENT");
		PANIC_SET(dbenv, on);
	}
	if (LF_ISSET(DB_REGION_INIT))
		ENV_ILLEGAL_AFTER_OPEN(dbenv,
		    "DB_ENV->set_flags: DB_REGION_INIT");

	/*
	 * DB_LOG_INMEMORY, DB_TXN_NOSYNC and DB_TXN_WRITE_NOSYNC are
	 * mutually incompatible.  If we're setting one of them, clear all
	 * current settings.
	 */
	if (LF_ISSET(
	    DB_LOG_INMEMORY | DB_TXN_NOSYNC | DB_TXN_WRITE_NOSYNC))
		F_CLR(dbenv,
		    DB_ENV_LOG_INMEMORY |
		    DB_ENV_TXN_NOSYNC | DB_ENV_TXN_WRITE_NOSYNC);

	/* Some flags are persisted in the regions. */
	__log_set_flags(dbenv, flags, on);

	mapped_flags = 0;
	__dbenv_map_flags(dbenv, &flags, &mapped_flags);
	if (on)
		F_SET(dbenv, mapped_flags);
	else
		F_CLR(dbenv, mapped_flags);

	return (0);
}

static int
__dbenv_get_data_dirs(dbenv, dirpp)
	DB_ENV *dbenv;
	const char ***dirpp;
{
	*dirpp = (const char **)dbenv->db_data_dir;
	return (0);
}

/*
 * __dbenv_set_data_dir --
 *	DB_ENV->set_data_dir.
 *
 * PUBLIC: int  __dbenv_set_data_dir __P((DB_ENV *, const char *));
 */
int
__dbenv_set_data_dir(dbenv, dir)
	DB_ENV *dbenv;
	const char *dir;
{
	int ret;

	/*
	 * The array is NULL-terminated so it can be returned by get_data_dirs
	 * without a length.
	 */

#define	DATA_INIT_CNT	20			/* Start with 20 data slots. */
	if (dbenv->db_data_dir == NULL) {
		if ((ret = __os_calloc(dbenv, DATA_INIT_CNT,
		    sizeof(char **), &dbenv->db_data_dir)) != 0)
			return (ret);
		dbenv->data_cnt = DATA_INIT_CNT;
	} else if (dbenv->data_next == dbenv->data_cnt - 2) {
		dbenv->data_cnt *= 2;
		if ((ret = __os_realloc(dbenv,
		    (u_int)dbenv->data_cnt * sizeof(char **),
		    &dbenv->db_data_dir)) != 0)
			return (ret);
	}

	ret = __os_strdup(dbenv,
	    dir, &dbenv->db_data_dir[dbenv->data_next++]);
	dbenv->db_data_dir[dbenv->data_next] = NULL;
	return (ret);
}

/*
 * __dbenv_set_intermediate_dir --
 *	DB_ENV->set_intermediate_dir.
 *
 * !!!
 * Undocumented routine allowing applications to configure Berkeley DB to
 * create intermediate directories.
 *
 * PUBLIC: int  __dbenv_set_intermediate_dir __P((DB_ENV *, int, u_int32_t));
 */
int
__dbenv_set_intermediate_dir(dbenv, mode, flags)
	DB_ENV *dbenv;
	int mode;
	u_int32_t flags;
{
	if (flags != 0)
		return (__db_ferr(dbenv, "DB_ENV->set_intermediate_dir", 0));
	if (mode == 0) {
		__db_err(dbenv,
		    "DB_ENV->set_intermediate_dir: mode may not be set to 0");
		return (EINVAL);
	}

	dbenv->dir_mode = mode;
	return (0);
}

/*
 * __dbenv_set_errcall --
 *	{DB_ENV,DB}->set_errcall.
 *
 * PUBLIC: void __dbenv_set_errcall __P((DB_ENV *,
 * PUBLIC:		void (*)(const DB_ENV *, const char *, const char *)));
 */
void
__dbenv_set_errcall(dbenv, errcall)
	DB_ENV *dbenv;
	void (*errcall) __P((const DB_ENV *, const char *, const char *));
{
	dbenv->db_errcall = errcall;
}

/*
 * __dbenv_get_errfile --
 *	{DB_ENV,DB}->get_errfile.
 *
 * PUBLIC: void __dbenv_get_errfile __P((DB_ENV *, FILE **));
 */
void
__dbenv_get_errfile(dbenv, errfilep)
	DB_ENV *dbenv;
	FILE **errfilep;
{
	*errfilep = dbenv->db_errfile;
}

/*
 * __dbenv_set_errfile --
 *	{DB_ENV,DB}->set_errfile.
 *
 * PUBLIC: void __dbenv_set_errfile __P((DB_ENV *, FILE *));
 */
void
__dbenv_set_errfile(dbenv, errfile)
	DB_ENV *dbenv;
	FILE *errfile;
{
	dbenv->db_errfile = errfile;
}

/*
 * __dbenv_get_errpfx --
 *	{DB_ENV,DB}->get_errpfx.
 *
 * PUBLIC: void __dbenv_get_errpfx __P((DB_ENV *, const char **));
 */
void
__dbenv_get_errpfx(dbenv, errpfxp)
	DB_ENV *dbenv;
	const char **errpfxp;
{
	*errpfxp = dbenv->db_errpfx;
}

/*
 * __dbenv_set_errpfx --
 *	{DB_ENV,DB}->set_errpfx.
 *
 * PUBLIC: void __dbenv_set_errpfx __P((DB_ENV *, const char *));
 */
void
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

/*
 * __dbenv_set_msgcall --
 *	{DB_ENV,DB}->set_msgcall.
 *
 * PUBLIC: void __dbenv_set_msgcall
 * PUBLIC:     __P((DB_ENV *, void (*)(const DB_ENV *, const char *)));
 */
void
__dbenv_set_msgcall(dbenv, msgcall)
	DB_ENV *dbenv;
	void (*msgcall) __P((const DB_ENV *, const char *));
{
	dbenv->db_msgcall = msgcall;
}

/*
 * __dbenv_get_msgfile --
 *	{DB_ENV,DB}->get_msgfile.
 *
 * PUBLIC: void __dbenv_get_msgfile __P((DB_ENV *, FILE **));
 */
void
__dbenv_get_msgfile(dbenv, msgfilep)
	DB_ENV *dbenv;
	FILE **msgfilep;
{
	*msgfilep = dbenv->db_msgfile;
}

/*
 * __dbenv_set_msgfile --
 *	{DB_ENV,DB}->set_msgfile.
 *
 * PUBLIC: void __dbenv_set_msgfile __P((DB_ENV *, FILE *));
 */
void
__dbenv_set_msgfile(dbenv, msgfile)
	DB_ENV *dbenv;
	FILE *msgfile;
{
	dbenv->db_msgfile = msgfile;
}

/*
 * __dbenv_set_noticecall --
 *	{DB_ENV,DB}->set_noticecall.
 *
 * PUBLIC: int  __dbenv_set_noticecall __P((DB_ENV *, void (*)(DB_ENV *, int)));
 */
int
__dbenv_set_noticecall(dbenv, noticecall)
	DB_ENV *dbenv;
	void (*noticecall) __P((DB_ENV *, int));
{
	dbenv->db_noticecall = noticecall;
	return (0);
}

/*
 * __dbenv_set_paniccall --
 *	{DB_ENV,DB}->set_paniccall.
 *
 * PUBLIC: int  __dbenv_set_paniccall __P((DB_ENV *, void (*)(DB_ENV *, int)));
 */
int
__dbenv_set_paniccall(dbenv, paniccall)
	DB_ENV *dbenv;
	void (*paniccall) __P((DB_ENV *, int));
{
	dbenv->db_paniccall = paniccall;
	return (0);
}

static int
__dbenv_get_shm_key(dbenv, shm_keyp)
	DB_ENV *dbenv;
	long *shm_keyp;			/* !!!: really a key_t *. */
{
	*shm_keyp = dbenv->shm_key;
	return (0);
}

/*
 * __dbenv_set_shm_key --
 *	DB_ENV->set_shm_key.
 *
 * PUBLIC: int  __dbenv_set_shm_key __P((DB_ENV *, long));
 */
int
__dbenv_set_shm_key(dbenv, shm_key)
	DB_ENV *dbenv;
	long shm_key;			/* !!!: really a key_t. */
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_shm_key");

	dbenv->shm_key = shm_key;
	return (0);
}

static int
__dbenv_get_tas_spins(dbenv, tas_spinsp)
	DB_ENV *dbenv;
	u_int32_t *tas_spinsp;
{
	*tas_spinsp = dbenv->tas_spins;
	return (0);
}

/*
 * __dbenv_set_tas_spins --
 *	DB_ENV->set_tas_spins.
 *
 * PUBLIC: int  __dbenv_set_tas_spins __P((DB_ENV *, u_int32_t));
 */
int
__dbenv_set_tas_spins(dbenv, tas_spins)
	DB_ENV *dbenv;
	u_int32_t tas_spins;
{
	dbenv->tas_spins = tas_spins;
	return (0);
}

static int
__dbenv_get_tmp_dir(dbenv, dirp)
	DB_ENV *dbenv;
	const char **dirp;
{
	*dirp = dbenv->db_tmp_dir;
	return (0);
}

/*
 * __dbenv_set_tmp_dir --
 *	DB_ENV->set_tmp_dir.
 *
 * PUBLIC: int  __dbenv_set_tmp_dir __P((DB_ENV *, const char *));
 */
int
__dbenv_set_tmp_dir(dbenv, dir)
	DB_ENV *dbenv;
	const char *dir;
{
	if (dbenv->db_tmp_dir != NULL)
		__os_free(dbenv, dbenv->db_tmp_dir);
	return (__os_strdup(dbenv, dir, &dbenv->db_tmp_dir));
}

static int
__dbenv_get_verbose(dbenv, which, onoffp)
	DB_ENV *dbenv;
	u_int32_t which;
	int *onoffp;
{
	switch (which) {
	case DB_VERB_DEADLOCK:
	case DB_VERB_RECOVERY:
	case DB_VERB_REPLICATION:
	case DB_VERB_WAITSFOR:
		*onoffp = FLD_ISSET(dbenv->verbose, which) ? 1 : 0;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * __dbenv_set_verbose --
 *	DB_ENV->set_verbose.
 *
 * PUBLIC: int  __dbenv_set_verbose __P((DB_ENV *, u_int32_t, int));
 */
int
__dbenv_set_verbose(dbenv, which, on)
	DB_ENV *dbenv;
	u_int32_t which;
	int on;
{
	switch (which) {
	case DB_VERB_DEADLOCK:
	case DB_VERB_RECOVERY:
	case DB_VERB_REPLICATION:
	case DB_VERB_WAITSFOR:
		if (on)
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
	__db_err(dbenv, "%s: method not permitted when environment specified",
	    name);
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
	__db_err(dbenv, "%s: method not permitted %s handle's open method",
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
	case DB_INIT_REP:
		sub = "replication";
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
	return (DB_OPNOTSUP);
}
