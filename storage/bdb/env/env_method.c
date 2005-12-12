/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: env_method.c,v 12.19 2005/11/10 17:12:17 bostic Exp $
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

static void __env_err __P((const DB_ENV *, int, const char *, ...));
static void __env_errx __P((const DB_ENV *, const char *, ...));
static int  __env_get_data_dirs __P((DB_ENV *, const char ***));
static int  __env_get_flags __P((DB_ENV *, u_int32_t *));
static int  __env_get_home __P((DB_ENV *, const char **));
static int  __env_get_shm_key __P((DB_ENV *, long *));
static int  __env_get_tmp_dir __P((DB_ENV *, const char **));
static int  __env_get_verbose __P((DB_ENV *, u_int32_t, int *));
static int  __env_init __P((DB_ENV *));
static void __env_map_flags __P((DB_ENV *, u_int32_t *, u_int32_t *));
static int  __env_set_app_dispatch
		__P((DB_ENV *, int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops)));
static int  __env_set_feedback __P((DB_ENV *, void (*)(DB_ENV *, int, int)));
static int  __env_set_isalive __P((DB_ENV *, int (*)(DB_ENV *,
		pid_t, db_threadid_t)));
static int  __env_set_thread_id __P((DB_ENV *, void (*)(DB_ENV *,
		pid_t *, db_threadid_t *)));
static int  __env_set_thread_id_string __P((DB_ENV *,
		char * (*)(DB_ENV *, pid_t, db_threadid_t, char *)));
static int  __env_set_thread_count __P((DB_ENV *, u_int32_t));
static int  __env_set_rpc_server
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
	if ((ret = __env_init(dbenv)) != 0) {
		__os_free(NULL, dbenv);
		return (ret);
	}

	*dbenvpp = dbenv;
	return (0);
}

/*
 * __env_init --
 *	Initialize a DB_ENV structure.
 */
static int
__env_init(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * !!!
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 *
	 * Initialize the method handles.
	 */
	/* DB_ENV PUBLIC HANDLE LIST BEGIN */
	dbenv->close = __env_close_pp;
	dbenv->dbremove = __env_dbremove_pp;
	dbenv->dbrename = __env_dbrename_pp;
	dbenv->err = __env_err;
	dbenv->errx = __env_errx;
	dbenv->failchk = __env_failchk_pp;
	dbenv->fileid_reset = __env_fileid_reset_pp;
	dbenv->get_cachesize = __memp_get_cachesize;
	dbenv->get_data_dirs = __env_get_data_dirs;
	dbenv->get_encrypt_flags = __env_get_encrypt_flags;
	dbenv->get_errfile = __env_get_errfile;
	dbenv->get_errpfx = __env_get_errpfx;
	dbenv->get_flags = __env_get_flags;
	dbenv->get_home = __env_get_home;
	dbenv->get_lg_bsize = __log_get_lg_bsize;
	dbenv->get_lg_dir = __log_get_lg_dir;
	dbenv->get_lg_filemode = __log_get_lg_filemode;
	dbenv->get_lg_max = __log_get_lg_max;
	dbenv->get_lg_regionmax = __log_get_lg_regionmax;
	dbenv->get_lk_conflicts = __lock_get_lk_conflicts;
	dbenv->get_lk_detect = __lock_get_lk_detect;
	dbenv->get_lk_max_lockers = __lock_get_lk_max_lockers;
	dbenv->get_lk_max_locks = __lock_get_lk_max_locks;
	dbenv->get_lk_max_objects = __lock_get_lk_max_objects;
	dbenv->get_mp_max_openfd = __memp_get_mp_max_openfd;
	dbenv->get_mp_max_write = __memp_get_mp_max_write;
	dbenv->get_mp_mmapsize = __memp_get_mp_mmapsize;
	dbenv->get_msgfile = __env_get_msgfile;
	dbenv->get_open_flags = __env_get_open_flags;
	dbenv->get_rep_limit = __rep_get_limit;
	dbenv->get_shm_key = __env_get_shm_key;
	dbenv->get_timeout = __lock_get_env_timeout;
	dbenv->get_tmp_dir = __env_get_tmp_dir;
	dbenv->get_tx_max = __txn_get_tx_max;
	dbenv->get_tx_timestamp = __txn_get_tx_timestamp;
	dbenv->get_verbose = __env_get_verbose;
	dbenv->is_bigendian = __db_isbigendian;
	dbenv->lock_detect = __lock_detect_pp;
	dbenv->lock_get = __lock_get_pp;
	dbenv->lock_id = __lock_id_pp;
	dbenv->lock_id_free = __lock_id_free_pp;
	dbenv->lock_put = __lock_put_pp;
	dbenv->lock_stat = __lock_stat_pp;
	dbenv->lock_stat_print = __lock_stat_print_pp;
	dbenv->lock_vec = __lock_vec_pp;
	dbenv->log_archive = __log_archive_pp;
	dbenv->log_cursor = __log_cursor_pp;
	dbenv->log_file = __log_file_pp;
	dbenv->log_flush = __log_flush_pp;
	dbenv->log_printf = __log_printf_capi;
	dbenv->log_put = __log_put_pp;
	dbenv->log_stat = __log_stat_pp;
	dbenv->log_stat_print = __log_stat_print_pp;
	dbenv->lsn_reset = __env_lsn_reset_pp;
	dbenv->memp_fcreate = __memp_fcreate_pp;
	dbenv->memp_register = __memp_register_pp;
	dbenv->memp_stat = __memp_stat_pp;
	dbenv->memp_stat_print = __memp_stat_print_pp;
	dbenv->memp_sync = __memp_sync_pp;
	dbenv->memp_trickle = __memp_trickle_pp;
	dbenv->mutex_alloc = __mutex_alloc_pp;
	dbenv->mutex_free = __mutex_free_pp;
	dbenv->mutex_get_align = __mutex_get_align;
	dbenv->mutex_get_increment = __mutex_get_increment;
	dbenv->mutex_get_max = __mutex_get_max;
	dbenv->mutex_get_tas_spins = __mutex_get_tas_spins;
	dbenv->mutex_lock = __mutex_lock_pp;
	dbenv->mutex_set_align = __mutex_set_align;
	dbenv->mutex_set_increment = __mutex_set_increment;
	dbenv->mutex_set_max = __mutex_set_max;
	dbenv->mutex_set_tas_spins = __mutex_set_tas_spins;
	dbenv->mutex_stat = __mutex_stat;
	dbenv->mutex_stat_print = __mutex_stat_print;
	dbenv->mutex_unlock = __mutex_unlock_pp;
	dbenv->open = __env_open_pp;
	dbenv->remove = __env_remove;
	dbenv->rep_elect = __rep_elect;
	dbenv->rep_flush = __rep_flush;
	dbenv->rep_get_config = __rep_get_config;
	dbenv->rep_process_message = __rep_process_message;
	dbenv->rep_set_config = __rep_set_config;
	dbenv->rep_start = __rep_start;
	dbenv->rep_stat = __rep_stat_pp;
	dbenv->rep_stat_print = __rep_stat_print_pp;
	dbenv->rep_sync = __rep_sync;
	dbenv->set_alloc = __env_set_alloc;
	dbenv->set_app_dispatch = __env_set_app_dispatch;
	dbenv->set_cachesize = __memp_set_cachesize;
	dbenv->set_data_dir = __env_set_data_dir;
	dbenv->set_encrypt = __env_set_encrypt;
	dbenv->set_errcall = __env_set_errcall;
	dbenv->set_errfile = __env_set_errfile;
	dbenv->set_errpfx = __env_set_errpfx;
	dbenv->set_feedback = __env_set_feedback;
	dbenv->set_flags = __env_set_flags;
	dbenv->set_intermediate_dir = __env_set_intermediate_dir;
	dbenv->set_isalive = __env_set_isalive;
	dbenv->set_lg_bsize = __log_set_lg_bsize;
	dbenv->set_lg_dir = __log_set_lg_dir;
	dbenv->set_lg_filemode = __log_set_lg_filemode;
	dbenv->set_lg_max = __log_set_lg_max;
	dbenv->set_lg_regionmax = __log_set_lg_regionmax;
	dbenv->set_lk_conflicts = __lock_set_lk_conflicts;
	dbenv->set_lk_detect = __lock_set_lk_detect;
	dbenv->set_lk_max = __lock_set_lk_max;
	dbenv->set_lk_max_lockers = __lock_set_lk_max_lockers;
	dbenv->set_lk_max_locks = __lock_set_lk_max_locks;
	dbenv->set_lk_max_objects = __lock_set_lk_max_objects;
	dbenv->set_mp_max_openfd = __memp_set_mp_max_openfd;
	dbenv->set_mp_max_write = __memp_set_mp_max_write;
	dbenv->set_mp_mmapsize = __memp_set_mp_mmapsize;
	dbenv->set_msgcall = __env_set_msgcall;
	dbenv->set_msgfile = __env_set_msgfile;
	dbenv->set_paniccall = __env_set_paniccall;
	dbenv->set_rep_limit = __rep_set_limit;
	dbenv->set_rep_request = __rep_set_request;
	dbenv->set_rep_transport = __rep_set_rep_transport;
	dbenv->set_rpc_server = __env_set_rpc_server;
	dbenv->set_shm_key = __env_set_shm_key;
	dbenv->set_thread_count = __env_set_thread_count;
	dbenv->set_thread_id = __env_set_thread_id;
	dbenv->set_thread_id_string = __env_set_thread_id_string;
	dbenv->set_timeout = __lock_set_env_timeout;
	dbenv->set_tmp_dir = __env_set_tmp_dir;
	dbenv->set_tx_max = __txn_set_tx_max;
	dbenv->set_tx_timestamp = __txn_set_tx_timestamp;
	dbenv->set_verbose = __env_set_verbose;
	dbenv->stat_print = __env_stat_print_pp;
	dbenv->txn_begin = __txn_begin_pp;
	dbenv->txn_checkpoint = __txn_checkpoint_pp;
	dbenv->txn_recover = __txn_recover_pp;
	dbenv->txn_stat = __txn_stat_pp;
	dbenv->txn_stat_print = __txn_stat_print_pp;
	/* DB_ENV PUBLIC HANDLE LIST END */

	/* DB_ENV PRIVATE HANDLE LIST BEGIN */
	dbenv->prdbt = __db_prdbt;
	/* DB_ENV PRIVATE HANDLE LIST END */

	__os_id(NULL, &dbenv->pid_cache, NULL);
	dbenv->thread_id = __os_id;
	dbenv->thread_id_string = __env_thread_id_string;
	dbenv->db_ref = 0;
	dbenv->shm_key = INVALID_REGION_SEGID;

	__lock_dbenv_create(dbenv);	/* Subsystem specific. */
	__log_dbenv_create(dbenv);
	__memp_dbenv_create(dbenv);
	__txn_dbenv_create(dbenv);

#ifdef HAVE_RPC
	/*
	 * RPC specific: must be last, as we replace methods set by the
	 * access methods.
	 */
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		__dbcl_dbenv_init(dbenv);
		/*
		 * !!!
		 * We wrap the DB_ENV->open and close methods for RPC, and
		 * the rpc.src file can't handle that.
		 */
		dbenv->open = __dbcl_env_open_wrap;
		dbenv->close = __dbcl_env_close_wrap;
	}
#endif

	return (0);
}

/*
 * __env_err --
 *	Error message, including the standard error string.
 */
static void
#ifdef STDC_HEADERS
__env_err(const DB_ENV *dbenv, int error, const char *fmt, ...)
#else
__env_err(dbenv, error, fmt, va_alist)
	const DB_ENV *dbenv;
	int error;
	const char *fmt;
	va_dcl
#endif
{
	DB_REAL_ERR(dbenv, error, 1, 1, fmt);
}

/*
 * __env_errx --
 *	Error message.
 */
static void
#ifdef STDC_HEADERS
__env_errx(const DB_ENV *dbenv, const char *fmt, ...)
#else
__env_errx(dbenv, fmt, va_alist)
	const DB_ENV *dbenv;
	const char *fmt;
	va_dcl
#endif
{
	DB_REAL_ERR(dbenv, 0, 0, 1, fmt);
}

static int
__env_get_home(dbenv, homep)
	DB_ENV *dbenv;
	const char **homep;
{
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->get_home");
	*homep = dbenv->db_home;
	return (0);
}

/*
 * __env_set_alloc --
 *	{DB_ENV,DB}->set_alloc.
 *
 * PUBLIC: int  __env_set_alloc __P((DB_ENV *, void *(*)(size_t),
 * PUBLIC:          void *(*)(void *, size_t), void (*)(void *)));
 */
int
__env_set_alloc(dbenv, mal_func, real_func, free_func)
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
 * __env_set_app_dispatch --
 *	Set the transaction abort recover function.
 */
static int
__env_set_app_dispatch(dbenv, app_dispatch)
	DB_ENV *dbenv;
	int (*app_dispatch) __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_app_dispatch");

	dbenv->app_dispatch = app_dispatch;
	return (0);
}

/*
 * __env_get_encrypt_flags --
 *	{DB_ENV,DB}->get_encrypt_flags.
 *
 * PUBLIC: int __env_get_encrypt_flags __P((DB_ENV *, u_int32_t *));
 */
int
__env_get_encrypt_flags(dbenv, flagsp)
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
 * __env_set_encrypt --
 *	DB_ENV->set_encrypt.
 *
 * PUBLIC: int __env_set_encrypt __P((DB_ENV *, const char *, u_int32_t));
 */
int
__env_set_encrypt(dbenv, passwd, flags)
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
__env_map_flags(dbenv, inflagsp, outflagsp)
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
	if (FLD_ISSET(*inflagsp, DB_DSYNC_DB)) {
		FLD_SET(*outflagsp, DB_ENV_DSYNC_DB);
		FLD_CLR(*inflagsp, DB_DSYNC_DB);
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
__env_get_flags(dbenv, flagsp)
	DB_ENV *dbenv;
	u_int32_t *flagsp;
{
	static const u_int32_t env_flags[] = {
		DB_AUTO_COMMIT,
		DB_CDB_ALLDB,
		DB_DIRECT_DB,
		DB_DIRECT_LOG,
		DB_DSYNC_DB,
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
		__env_map_flags(dbenv, &f, &mapped_flag);
		DB_ASSERT(f == 0);
		if (F_ISSET(dbenv, mapped_flag) == mapped_flag)
			LF_SET(env_flags[i]);
	}

	/* Some flags are persisted in the regions. */
	if (dbenv->reginfo != NULL &&
	    ((REGENV *)((REGINFO *)dbenv->reginfo)->primary)->panic != 0) {
		LF_SET(DB_PANIC_ENVIRONMENT);
	}
	__log_get_flags(dbenv, &flags);

	*flagsp = flags;
	return (0);
}

/*
 * __env_set_flags --
 *	DB_ENV->set_flags.
 *
 * PUBLIC: int  __env_set_flags __P((DB_ENV *, u_int32_t, int));
 */
int
__env_set_flags(dbenv, flags, on)
	DB_ENV *dbenv;
	u_int32_t flags;
	int on;
{
	u_int32_t mapped_flags;
	int ret;

#define	OK_FLAGS							\
	(DB_AUTO_COMMIT | DB_CDB_ALLDB | DB_DIRECT_DB | DB_DIRECT_LOG |	\
	    DB_DSYNC_DB | DB_DSYNC_LOG | DB_LOG_AUTOREMOVE |		\
	    DB_LOG_INMEMORY | DB_NOLOCKING | DB_NOMMAP | DB_NOPANIC |	\
	    DB_OVERWRITE | DB_PANIC_ENVIRONMENT | DB_REGION_INIT |	\
	    DB_TIME_NOTGRANTED | DB_TXN_NOSYNC | DB_TXN_WRITE_NOSYNC |	\
	    DB_YIELDCPU)

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
		if (on) {
			__db_err(dbenv, "Environment panic set");
			(void)__db_panic(dbenv, EACCES);
		} else
			__db_panic_set(dbenv, 0);
	}
	if (LF_ISSET(DB_REGION_INIT))
		ENV_ILLEGAL_AFTER_OPEN(dbenv,
		    "DB_ENV->set_flags: DB_REGION_INIT");
	if (LF_ISSET(DB_LOG_INMEMORY))
		ENV_ILLEGAL_AFTER_OPEN(dbenv,
		    "DB_ENV->set_flags: DB_LOG_INMEMORY");

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
	__env_map_flags(dbenv, &flags, &mapped_flags);
	if (on)
		F_SET(dbenv, mapped_flags);
	else
		F_CLR(dbenv, mapped_flags);

	return (0);
}

static int
__env_get_data_dirs(dbenv, dirpp)
	DB_ENV *dbenv;
	const char ***dirpp;
{
	*dirpp = (const char **)dbenv->db_data_dir;
	return (0);
}

/*
 * __env_set_data_dir --
 *	DB_ENV->set_data_dir.
 *
 * PUBLIC: int  __env_set_data_dir __P((DB_ENV *, const char *));
 */
int
__env_set_data_dir(dbenv, dir)
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
 * __env_set_intermediate_dir --
 *	DB_ENV->set_intermediate_dir.
 *
 * !!!
 * Undocumented routine allowing applications to configure Berkeley DB to
 * create intermediate directories.
 *
 * PUBLIC: int  __env_set_intermediate_dir __P((DB_ENV *, int, u_int32_t));
 */
int
__env_set_intermediate_dir(dbenv, mode, flags)
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
 * __env_set_errcall --
 *	{DB_ENV,DB}->set_errcall.
 *
 * PUBLIC: void __env_set_errcall __P((DB_ENV *,
 * PUBLIC:		void (*)(const DB_ENV *, const char *, const char *)));
 */
void
__env_set_errcall(dbenv, errcall)
	DB_ENV *dbenv;
	void (*errcall) __P((const DB_ENV *, const char *, const char *));
{
	dbenv->db_errcall = errcall;
}

/*
 * __env_get_errfile --
 *	{DB_ENV,DB}->get_errfile.
 *
 * PUBLIC: void __env_get_errfile __P((DB_ENV *, FILE **));
 */
void
__env_get_errfile(dbenv, errfilep)
	DB_ENV *dbenv;
	FILE **errfilep;
{
	*errfilep = dbenv->db_errfile;
}

/*
 * __env_set_errfile --
 *	{DB_ENV,DB}->set_errfile.
 *
 * PUBLIC: void __env_set_errfile __P((DB_ENV *, FILE *));
 */
void
__env_set_errfile(dbenv, errfile)
	DB_ENV *dbenv;
	FILE *errfile;
{
	dbenv->db_errfile = errfile;
}

/*
 * __env_get_errpfx --
 *	{DB_ENV,DB}->get_errpfx.
 *
 * PUBLIC: void __env_get_errpfx __P((DB_ENV *, const char **));
 */
void
__env_get_errpfx(dbenv, errpfxp)
	DB_ENV *dbenv;
	const char **errpfxp;
{
	*errpfxp = dbenv->db_errpfx;
}

/*
 * __env_set_errpfx --
 *	{DB_ENV,DB}->set_errpfx.
 *
 * PUBLIC: void __env_set_errpfx __P((DB_ENV *, const char *));
 */
void
__env_set_errpfx(dbenv, errpfx)
	DB_ENV *dbenv;
	const char *errpfx;
{
	dbenv->db_errpfx = errpfx;
}

static int
__env_set_feedback(dbenv, feedback)
	DB_ENV *dbenv;
	void (*feedback) __P((DB_ENV *, int, int));
{
	dbenv->db_feedback = feedback;
	return (0);
}

/*
 * __env_set_thread_id --
 *	DB_ENV->set_thread_id
 */
static int
__env_set_thread_id(dbenv, id)
	DB_ENV *dbenv;
	void (*id) __P((DB_ENV *, pid_t *, db_threadid_t *));
{
	dbenv->thread_id = id;
	return (0);
}

/*
 * __env_set_threadid_string --
 *	DB_ENV->set_threadid_string
 */
static int
__env_set_thread_id_string(dbenv, thread_id_string)
	DB_ENV *dbenv;
	char *(*thread_id_string) __P((DB_ENV *, pid_t, db_threadid_t, char *));
{
	dbenv->thread_id_string = thread_id_string;
	return (0);
}

/*
 * __env_set_isalive --
 *	DB_ENV->set_isalive
 */
static int
__env_set_isalive(dbenv, is_alive)
	DB_ENV *dbenv;
	int (*is_alive) __P((DB_ENV *, pid_t, db_threadid_t));
{
	if (F_ISSET((dbenv), DB_ENV_OPEN_CALLED) &&
	    dbenv->thr_nbucket == 0) {
		__db_err(dbenv,
		    "is_alive method specified but no thread region allocated");
		return (EINVAL);
	}
	dbenv->is_alive = is_alive;
	return (0);
}

/*
 * __env_set_thread_count --
 *	DB_ENV->set_thread_count
 */
static int
__env_set_thread_count(dbenv, count)
	DB_ENV *dbenv;
	u_int32_t count;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_thread_count");
	dbenv->thr_max = count;

	/*
	 * Set the number of buckets to be 1/8th the number of
	 * proposed threads control blocks.  This is rather
	 * arbitrary.
	 */
	dbenv->thr_nbucket = count / 8;
	return (0);
}

/*
 * __env_set_msgcall --
 *	{DB_ENV,DB}->set_msgcall.
 *
 * PUBLIC: void __env_set_msgcall
 * PUBLIC:     __P((DB_ENV *, void (*)(const DB_ENV *, const char *)));
 */
void
__env_set_msgcall(dbenv, msgcall)
	DB_ENV *dbenv;
	void (*msgcall) __P((const DB_ENV *, const char *));
{
	dbenv->db_msgcall = msgcall;
}

/*
 * __env_get_msgfile --
 *	{DB_ENV,DB}->get_msgfile.
 *
 * PUBLIC: void __env_get_msgfile __P((DB_ENV *, FILE **));
 */
void
__env_get_msgfile(dbenv, msgfilep)
	DB_ENV *dbenv;
	FILE **msgfilep;
{
	*msgfilep = dbenv->db_msgfile;
}

/*
 * __env_set_msgfile --
 *	{DB_ENV,DB}->set_msgfile.
 *
 * PUBLIC: void __env_set_msgfile __P((DB_ENV *, FILE *));
 */
void
__env_set_msgfile(dbenv, msgfile)
	DB_ENV *dbenv;
	FILE *msgfile;
{
	dbenv->db_msgfile = msgfile;
}

/*
 * __env_set_paniccall --
 *	{DB_ENV,DB}->set_paniccall.
 *
 * PUBLIC: int  __env_set_paniccall __P((DB_ENV *, void (*)(DB_ENV *, int)));
 */
int
__env_set_paniccall(dbenv, paniccall)
	DB_ENV *dbenv;
	void (*paniccall) __P((DB_ENV *, int));
{
	dbenv->db_paniccall = paniccall;
	return (0);
}

static int
__env_get_shm_key(dbenv, shm_keyp)
	DB_ENV *dbenv;
	long *shm_keyp;			/* !!!: really a key_t *. */
{
	*shm_keyp = dbenv->shm_key;
	return (0);
}

/*
 * __env_set_shm_key --
 *	DB_ENV->set_shm_key.
 *
 * PUBLIC: int  __env_set_shm_key __P((DB_ENV *, long));
 */
int
__env_set_shm_key(dbenv, shm_key)
	DB_ENV *dbenv;
	long shm_key;			/* !!!: really a key_t. */
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_shm_key");

	dbenv->shm_key = shm_key;
	return (0);
}

static int
__env_get_tmp_dir(dbenv, dirp)
	DB_ENV *dbenv;
	const char **dirp;
{
	*dirp = dbenv->db_tmp_dir;
	return (0);
}

/*
 * __env_set_tmp_dir --
 *	DB_ENV->set_tmp_dir.
 *
 * PUBLIC: int  __env_set_tmp_dir __P((DB_ENV *, const char *));
 */
int
__env_set_tmp_dir(dbenv, dir)
	DB_ENV *dbenv;
	const char *dir;
{
	if (dbenv->db_tmp_dir != NULL)
		__os_free(dbenv, dbenv->db_tmp_dir);
	return (__os_strdup(dbenv, dir, &dbenv->db_tmp_dir));
}

static int
__env_get_verbose(dbenv, which, onoffp)
	DB_ENV *dbenv;
	u_int32_t which;
	int *onoffp;
{
	switch (which) {
	case DB_VERB_DEADLOCK:
	case DB_VERB_RECOVERY:
	case DB_VERB_REGISTER:
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
 * __env_set_verbose --
 *	DB_ENV->set_verbose.
 *
 * PUBLIC: int  __env_set_verbose __P((DB_ENV *, u_int32_t, int));
 */
int
__env_set_verbose(dbenv, which, on)
	DB_ENV *dbenv;
	u_int32_t which;
	int on;
{
	switch (which) {
	case DB_VERB_DEADLOCK:
	case DB_VERB_RECOVERY:
	case DB_VERB_REGISTER:
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
__env_set_rpc_server(dbenv, cl, host, tsec, ssec, flags)
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

	__db_err(dbenv, "Berkeley DB was not configured for RPC support");
	return (DB_OPNOTSUP);
}
