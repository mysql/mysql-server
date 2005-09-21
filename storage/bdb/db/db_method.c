/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_method.c,v 11.116 2004/10/11 18:22:05 bostic Exp $
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

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"
#include "dbinc/qam.h"
#include "dbinc/xa.h"
#include "dbinc_auto/xa_ext.h"

#ifdef HAVE_RPC
#include "dbinc_auto/rpc_client_ext.h"
#endif

static int  __db_get_byteswapped __P((DB *, int *));
static int  __db_get_dbname __P((DB *, const char **, const char **));
static DB_ENV *__db_get_env __P((DB *));
static int  __db_get_transactional __P((DB *));
static int  __db_get_type __P((DB *, DBTYPE *dbtype));
static int  __db_init __P((DB *, u_int32_t));
static int  __db_set_alloc __P((DB *, void *(*)(size_t),
		void *(*)(void *, size_t), void (*)(void *)));
static int  __db_set_append_recno __P((DB *, int (*)(DB *, DBT *, db_recno_t)));
static int  __db_get_cachesize __P((DB *, u_int32_t *, u_int32_t *, int *));
static int  __db_set_cachesize __P((DB *, u_int32_t, u_int32_t, int));
static int  __db_set_dup_compare
		__P((DB *, int (*)(DB *, const DBT *, const DBT *)));
static int  __db_get_encrypt_flags __P((DB *, u_int32_t *));
static int  __db_set_encrypt __P((DB *, const char *, u_int32_t));
static int  __db_set_feedback __P((DB *, void (*)(DB *, int, int)));
static void __db_map_flags __P((DB *, u_int32_t *, u_int32_t *));
static int  __db_get_pagesize __P((DB *, u_int32_t *));
static int  __db_set_paniccall __P((DB *, void (*)(DB_ENV *, int)));
static void __db_set_errcall
	      __P((DB *, void (*)(const DB_ENV *, const char *, const char *)));
static void __db_get_errfile __P((DB *, FILE **));
static void __db_set_errfile __P((DB *, FILE *));
static void __db_get_errpfx __P((DB *, const char **));
static void __db_set_errpfx __P((DB *, const char *));
static void __db_set_msgcall
	      __P((DB *, void (*)(const DB_ENV *, const char *)));
static void __db_get_msgfile __P((DB *, FILE **));
static void __db_set_msgfile __P((DB *, FILE *));
static void __dbh_err __P((DB *, int, const char *, ...));
static void __dbh_errx __P((DB *, const char *, ...));

#ifdef HAVE_RPC
static int  __dbcl_init __P((DB *, DB_ENV *, u_int32_t));
#endif

/*
 * db_create --
 *	DB constructor.
 *
 * EXTERN: int db_create __P((DB **, DB_ENV *, u_int32_t));
 */
int
db_create(dbpp, dbenv, flags)
	DB **dbpp;
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB *dbp;
	int ret;

	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
		break;
	case DB_REP_CREATE:
		break;
	case DB_XA_CREATE:
		if (dbenv != NULL) {
			__db_err(dbenv,
		"XA applications may not specify an environment to db_create");
			return (EINVAL);
		}

		/*
		 * If it's an XA database, open it within the XA environment,
		 * taken from the global list of environments.  (When the XA
		 * transaction manager called our xa_start() routine the
		 * "current" environment was moved to the start of the list.
		 */
		dbenv = TAILQ_FIRST(&DB_GLOBAL(db_envq));
		break;
	default:
		return (__db_ferr(dbenv, "db_create", 0));
	}

	/* Allocate the DB. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(*dbp), &dbp)) != 0)
		return (ret);
#ifdef HAVE_RPC
	if (dbenv != NULL && RPC_ON(dbenv))
		ret = __dbcl_init(dbp, dbenv, flags);
	else
#endif
		ret = __db_init(dbp, flags);
	if (ret != 0)
		goto err;

	/* If we don't have an environment yet, allocate a local one. */
	if (dbenv == NULL) {
		if ((ret = db_env_create(&dbenv, 0)) != 0)
			goto err;
		F_SET(dbenv, DB_ENV_DBLOCAL);
	}
	dbp->dbenv = dbenv;
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	++dbenv->db_ref;
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	/*
	 * Set the replication timestamp; it's 0 if we're not in a replicated
	 * environment.
	 */
	dbp->timestamp =
	    (F_ISSET(dbenv, DB_ENV_DBLOCAL) || !REP_ON(dbenv)) ? 0 :
	    ((REGENV *)((REGINFO *)dbenv->reginfo)->primary)->rep_timestamp;

	/* If not RPC, open a backing DB_MPOOLFILE handle in the memory pool. */
#ifdef HAVE_RPC
	if (!RPC_ON(dbenv))
#endif
		if ((ret = __memp_fcreate(dbenv, &dbp->mpf)) != 0)
			goto err;

	dbp->type = DB_UNKNOWN;

	*dbpp = dbp;
	return (0);

err:	if (dbp->mpf != NULL)
		(void)__memp_fclose(dbp->mpf, 0);
	if (dbenv != NULL && F_ISSET(dbenv, DB_ENV_DBLOCAL))
		(void)__dbenv_close(dbenv, 0);
	__os_free(dbenv, dbp);
	*dbpp = NULL;
	return (ret);
}

/*
 * __db_init --
 *	Initialize a DB structure.
 */
static int
__db_init(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	int ret;

	dbp->lid = DB_LOCK_INVALIDID;
	LOCK_INIT(dbp->handle_lock);

	TAILQ_INIT(&dbp->free_queue);
	TAILQ_INIT(&dbp->active_queue);
	TAILQ_INIT(&dbp->join_queue);
	LIST_INIT(&dbp->s_secondaries);

	FLD_SET(dbp->am_ok,
	    DB_OK_BTREE | DB_OK_HASH | DB_OK_QUEUE | DB_OK_RECNO);

	dbp->associate = __db_associate_pp;
	dbp->close = __db_close_pp;
	dbp->cursor = __db_cursor_pp;
	dbp->del = __db_del_pp;
	dbp->dump = __db_dump_pp;
	dbp->err = __dbh_err;
	dbp->errx = __dbh_errx;
	dbp->fd = __db_fd_pp;
	dbp->get = __db_get_pp;
	dbp->get_byteswapped = __db_get_byteswapped;
	dbp->get_dbname = __db_get_dbname;
	dbp->get_env = __db_get_env;
	dbp->get_open_flags = __db_get_open_flags;
	dbp->get_transactional = __db_get_transactional;
	dbp->get_type = __db_get_type;
	dbp->join = __db_join_pp;
	dbp->key_range = __db_key_range_pp;
	dbp->open = __db_open_pp;
	dbp->pget = __db_pget_pp;
	dbp->put = __db_put_pp;
	dbp->remove = __db_remove_pp;
	dbp->rename = __db_rename_pp;
	dbp->truncate = __db_truncate_pp;
	dbp->set_alloc = __db_set_alloc;
	dbp->set_append_recno = __db_set_append_recno;
	dbp->get_cachesize = __db_get_cachesize;
	dbp->set_cachesize = __db_set_cachesize;
	dbp->set_dup_compare = __db_set_dup_compare;
	dbp->get_encrypt_flags = __db_get_encrypt_flags;
	dbp->set_encrypt = __db_set_encrypt;
	dbp->set_errcall = __db_set_errcall;
	dbp->get_errfile = __db_get_errfile;
	dbp->set_errfile = __db_set_errfile;
	dbp->get_errpfx = __db_get_errpfx;
	dbp->set_errpfx = __db_set_errpfx;
	dbp->set_feedback = __db_set_feedback;
	dbp->get_flags = __db_get_flags;
	dbp->set_flags = __db_set_flags;
	dbp->get_lorder = __db_get_lorder;
	dbp->set_lorder = __db_set_lorder;
	dbp->set_msgcall = __db_set_msgcall;
	dbp->get_msgfile = __db_get_msgfile;
	dbp->set_msgfile = __db_set_msgfile;
	dbp->get_pagesize = __db_get_pagesize;
	dbp->set_pagesize = __db_set_pagesize;
	dbp->set_paniccall = __db_set_paniccall;
	dbp->stat = __db_stat_pp;
	dbp->stat_print = __db_stat_print_pp;
	dbp->sync = __db_sync_pp;
	dbp->upgrade = __db_upgrade_pp;
	dbp->verify = __db_verify_pp;

					/* Access method specific. */
	if ((ret = __bam_db_create(dbp)) != 0)
		return (ret);
	if ((ret = __ham_db_create(dbp)) != 0)
		return (ret);
	if ((ret = __qam_db_create(dbp)) != 0)
		return (ret);

	/*
	 * XA specific: must be last, as we replace methods set by the
	 * access methods.
	 */
	if (LF_ISSET(DB_XA_CREATE) && (ret = __db_xa_create(dbp)) != 0)
		return (ret);

	if (LF_ISSET(DB_REP_CREATE))
		F_SET(dbp, DB_AM_REPLICATION);

	return (0);
}

/*
 * __dbh_am_chk --
 *	Error if an unreasonable method is called.
 *
 * PUBLIC: int __dbh_am_chk __P((DB *, u_int32_t));
 */
int
__dbh_am_chk(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	/*
	 * We start out allowing any access methods to be called, and as the
	 * application calls the methods the options become restricted.  The
	 * idea is to quit as soon as an illegal method combination is called.
	 */
	if ((LF_ISSET(DB_OK_BTREE) && FLD_ISSET(dbp->am_ok, DB_OK_BTREE)) ||
	    (LF_ISSET(DB_OK_HASH) && FLD_ISSET(dbp->am_ok, DB_OK_HASH)) ||
	    (LF_ISSET(DB_OK_QUEUE) && FLD_ISSET(dbp->am_ok, DB_OK_QUEUE)) ||
	    (LF_ISSET(DB_OK_RECNO) && FLD_ISSET(dbp->am_ok, DB_OK_RECNO))) {
		FLD_CLR(dbp->am_ok, ~flags);
		return (0);
	}

	__db_err(dbp->dbenv,
    "call implies an access method which is inconsistent with previous calls");
	return (EINVAL);
}

/*
 * __dbh_err --
 *	Error message, including the standard error string.
 */
static void
#ifdef STDC_HEADERS
__dbh_err(DB *dbp, int error, const char *fmt, ...)
#else
__dbh_err(dbp, error, fmt, va_alist)
	DB *dbp;
	int error;
	const char *fmt;
	va_dcl
#endif
{
	DB_REAL_ERR(dbp->dbenv, error, 1, 1, fmt);
}

/*
 * __dbh_errx --
 *	Error message.
 */
static void
#ifdef STDC_HEADERS
__dbh_errx(DB *dbp, const char *fmt, ...)
#else
__dbh_errx(dbp, fmt, va_alist)
	DB *dbp;
	const char *fmt;
	va_dcl
#endif
{
	DB_REAL_ERR(dbp->dbenv, 0, 0, 1, fmt);
}

/*
 * __db_get_byteswapped --
 *	Return if database requires byte swapping.
 */
static int
__db_get_byteswapped(dbp, isswapped)
	DB *dbp;
	int *isswapped;
{
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->get_byteswapped");

	*isswapped = F_ISSET(dbp, DB_AM_SWAP) ? 1 : 0;
	return (0);
}

/*
 * __db_get_dbname --
 *	Get the name of the database as passed to DB->open.
 */
static int
__db_get_dbname(dbp, fnamep, dnamep)
	DB *dbp;
	const char **fnamep, **dnamep;
{
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->get_dbname");

	if (fnamep != NULL)
		*fnamep = dbp->fname;
	if (dnamep != NULL)
		*dnamep = dbp->dname;
	return (0);
}

/*
 * __db_get_env --
 *	Get the DB_ENV handle that was passed to db_create.
 */
static DB_ENV *
__db_get_env(dbp)
	DB *dbp;
{
	return (dbp->dbenv);
}

/*
 * get_transactional --
 *	Get whether this database was created in a transaction.
 */
static int
__db_get_transactional(dbp)
	DB *dbp;
{
	return (F_ISSET(dbp, DB_AM_TXN) ? 1 : 0);
}

/*
 * __db_get_type --
 *	Return type of underlying database.
 */
static int
__db_get_type(dbp, dbtype)
	DB *dbp;
	DBTYPE *dbtype;
{
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->get_type");

	*dbtype = dbp->type;
	return (0);
}

/*
 * __db_set_append_recno --
 *	Set record number append routine.
 */
static int
__db_set_append_recno(dbp, func)
	DB *dbp;
	int (*func) __P((DB *, DBT *, db_recno_t));
{
	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_append_recno");
	DB_ILLEGAL_METHOD(dbp, DB_OK_QUEUE | DB_OK_RECNO);

	dbp->db_append_recno = func;

	return (0);
}

/*
 * __db_get_cachesize --
 *	Get underlying cache size.
 */
static int
__db_get_cachesize(dbp, cache_gbytesp, cache_bytesp, ncachep)
	DB *dbp;
	u_int32_t *cache_gbytesp, *cache_bytesp;
	int *ncachep;
{
	DB_ILLEGAL_IN_ENV(dbp, "DB->get_cachesize");

	return (__memp_get_cachesize(dbp->dbenv,
	    cache_gbytesp, cache_bytesp, ncachep));
}

/*
 * __db_set_cachesize --
 *	Set underlying cache size.
 */
static int
__db_set_cachesize(dbp, cache_gbytes, cache_bytes, ncache)
	DB *dbp;
	u_int32_t cache_gbytes, cache_bytes;
	int ncache;
{
	DB_ILLEGAL_IN_ENV(dbp, "DB->set_cachesize");
	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_cachesize");

	return (__memp_set_cachesize(
	    dbp->dbenv, cache_gbytes, cache_bytes, ncache));
}

/*
 * __db_set_dup_compare --
 *	Set duplicate comparison routine.
 */
static int
__db_set_dup_compare(dbp, func)
	DB *dbp;
	int (*func) __P((DB *, const DBT *, const DBT *));
{
	int ret;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->dup_compare");
	DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE | DB_OK_HASH);

	if ((ret = __db_set_flags(dbp, DB_DUPSORT)) != 0)
		return (ret);

	dbp->dup_compare = func;

	return (0);
}

/*
 * __db_get_encrypt_flags --
 */
static int
__db_get_encrypt_flags(dbp, flagsp)
	DB *dbp;
	u_int32_t *flagsp;
{
	DB_ILLEGAL_IN_ENV(dbp, "DB->get_encrypt_flags");

	return (__dbenv_get_encrypt_flags(dbp->dbenv, flagsp));
}

/*
 * __db_set_encrypt --
 *	Set database passwd.
 */
static int
__db_set_encrypt(dbp, passwd, flags)
	DB *dbp;
	const char *passwd;
	u_int32_t flags;
{
	DB_CIPHER *db_cipher;
	int ret;

	DB_ILLEGAL_IN_ENV(dbp, "DB->set_encrypt");
	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_encrypt");

	if ((ret = __dbenv_set_encrypt(dbp->dbenv, passwd, flags)) != 0)
		return (ret);

	/*
	 * In a real env, this gets initialized with the region.  In a local
	 * env, we must do it here.
	 */
	db_cipher = (DB_CIPHER *)dbp->dbenv->crypto_handle;
	if (!F_ISSET(db_cipher, CIPHER_ANY) &&
	    (ret = db_cipher->init(dbp->dbenv, db_cipher)) != 0)
		return (ret);

	return (__db_set_flags(dbp, DB_ENCRYPT));
}

static void
__db_set_errcall(dbp, errcall)
	DB *dbp;
	void (*errcall) __P((const DB_ENV *, const char *, const char *));
{
	__dbenv_set_errcall(dbp->dbenv, errcall);
}

static void
__db_get_errfile(dbp, errfilep)
	DB *dbp;
	FILE **errfilep;
{
	__dbenv_get_errfile(dbp->dbenv, errfilep);
}

static void
__db_set_errfile(dbp, errfile)
	DB *dbp;
	FILE *errfile;
{
	__dbenv_set_errfile(dbp->dbenv, errfile);
}

static void
__db_get_errpfx(dbp, errpfxp)
	DB *dbp;
	const char **errpfxp;
{
	__dbenv_get_errpfx(dbp->dbenv, errpfxp);
}

static void
__db_set_errpfx(dbp, errpfx)
	DB *dbp;
	const char *errpfx;
{
	__dbenv_set_errpfx(dbp->dbenv, errpfx);
}

static int
__db_set_feedback(dbp, feedback)
	DB *dbp;
	void (*feedback) __P((DB *, int, int));
{
	dbp->db_feedback = feedback;
	return (0);
}

/*
 * __db_map_flags --
 *	Maps between public and internal flag values.
 *      This function doesn't check for validity, so it can't fail.
 */
static void
__db_map_flags(dbp, inflagsp, outflagsp)
	DB *dbp;
	u_int32_t *inflagsp, *outflagsp;
{
	COMPQUIET(dbp, NULL);

	if (FLD_ISSET(*inflagsp, DB_CHKSUM)) {
		FLD_SET(*outflagsp, DB_AM_CHKSUM);
		FLD_CLR(*inflagsp, DB_CHKSUM);
	}
	if (FLD_ISSET(*inflagsp, DB_ENCRYPT)) {
		FLD_SET(*outflagsp, DB_AM_ENCRYPT | DB_AM_CHKSUM);
		FLD_CLR(*inflagsp, DB_ENCRYPT);
	}
	if (FLD_ISSET(*inflagsp, DB_TXN_NOT_DURABLE)) {
		FLD_SET(*outflagsp, DB_AM_NOT_DURABLE);
		FLD_CLR(*inflagsp, DB_TXN_NOT_DURABLE);
	}
}

/*
 * __db_get_flags --
 *	The DB->get_flags method.
 *
 * PUBLIC: int __db_get_flags __P((DB *, u_int32_t *));
 */
int
__db_get_flags(dbp, flagsp)
	DB *dbp;
	u_int32_t *flagsp;
{
	static const u_int32_t db_flags[] = {
		DB_CHKSUM,
		DB_DUP,
		DB_DUPSORT,
		DB_ENCRYPT,
		DB_INORDER,
		DB_RECNUM,
		DB_RENUMBER,
		DB_REVSPLITOFF,
		DB_SNAPSHOT,
		DB_TXN_NOT_DURABLE,
		0
	};
	u_int32_t f, flags, mapped_flag;
	int i;

	flags = 0;
	for (i = 0; (f = db_flags[i]) != 0; i++) {
		mapped_flag = 0;
		__db_map_flags(dbp, &f, &mapped_flag);
		__bam_map_flags(dbp, &f, &mapped_flag);
		__ram_map_flags(dbp, &f, &mapped_flag);
#ifdef HAVE_QUEUE
		__qam_map_flags(dbp, &f, &mapped_flag);
#endif
		DB_ASSERT(f == 0);
		if (F_ISSET(dbp, mapped_flag) == mapped_flag)
			LF_SET(db_flags[i]);
	}

	*flagsp = flags;
	return (0);
}

/*
 * __db_set_flags --
 *	DB->set_flags.
 *
 * PUBLIC: int  __db_set_flags __P((DB *, u_int32_t));
 */
int
__db_set_flags(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbp->dbenv;

	if (LF_ISSET(DB_ENCRYPT) && !CRYPTO_ON(dbenv)) {
		__db_err(dbenv,
		    "Database environment not configured for encryption");
		return (EINVAL);
	}
	if (LF_ISSET(DB_TXN_NOT_DURABLE))
		ENV_REQUIRES_CONFIG(dbenv,
		    dbenv->tx_handle, "DB_NOT_DURABLE", DB_INIT_TXN);

	__db_map_flags(dbp, &flags, &dbp->flags);

	if ((ret = __bam_set_flags(dbp, &flags)) != 0)
		return (ret);
	if ((ret = __ram_set_flags(dbp, &flags)) != 0)
		return (ret);
#ifdef HAVE_QUEUE
	if ((ret = __qam_set_flags(dbp, &flags)) != 0)
		return (ret);
#endif

	return (flags == 0 ? 0 : __db_ferr(dbenv, "DB->set_flags", 0));
}

/*
 * __db_get_lorder --
 *	Get whether lorder is swapped or not.
 *
 * PUBLIC: int  __db_get_lorder __P((DB *, int *));
 */
int
__db_get_lorder(dbp, db_lorderp)
	DB *dbp;
	int *db_lorderp;
{
	int ret;

	/* Flag if the specified byte order requires swapping. */
	switch (ret = __db_byteorder(dbp->dbenv, 1234)) {
	case 0:
		*db_lorderp = F_ISSET(dbp, DB_AM_SWAP) ? 4321 : 1234;
		break;
	case DB_SWAPBYTES:
		*db_lorderp = F_ISSET(dbp, DB_AM_SWAP) ? 1234 : 4321;
		break;
	default:
		return (ret);
		/* NOTREACHED */
	}

	return (0);
}

/*
 * __db_set_lorder --
 *	Set whether lorder is swapped or not.
 *
 * PUBLIC: int  __db_set_lorder __P((DB *, int));
 */
int
__db_set_lorder(dbp, db_lorder)
	DB *dbp;
	int db_lorder;
{
	int ret;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_lorder");

	/* Flag if the specified byte order requires swapping. */
	switch (ret = __db_byteorder(dbp->dbenv, db_lorder)) {
	case 0:
		F_CLR(dbp, DB_AM_SWAP);
		break;
	case DB_SWAPBYTES:
		F_SET(dbp, DB_AM_SWAP);
		break;
	default:
		return (ret);
		/* NOTREACHED */
	}
	return (0);
}

static int
__db_set_alloc(dbp, mal_func, real_func, free_func)
	DB *dbp;
	void *(*mal_func) __P((size_t));
	void *(*real_func) __P((void *, size_t));
	void (*free_func) __P((void *));
{
	DB_ILLEGAL_IN_ENV(dbp, "DB->set_alloc");
	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_alloc");

	return (__dbenv_set_alloc(dbp->dbenv, mal_func, real_func, free_func));
}

static void
__db_set_msgcall(dbp, msgcall)
	DB *dbp;
	void (*msgcall) __P((const DB_ENV *, const char *));
{
	__dbenv_set_msgcall(dbp->dbenv, msgcall);
}

static void
__db_get_msgfile(dbp, msgfilep)
	DB *dbp;
	FILE **msgfilep;
{
	__dbenv_get_msgfile(dbp->dbenv, msgfilep);
}

static void
__db_set_msgfile(dbp, msgfile)
	DB *dbp;
	FILE *msgfile;
{
	__dbenv_set_msgfile(dbp->dbenv, msgfile);
}

static int
__db_get_pagesize(dbp, db_pagesizep)
	DB *dbp;
	u_int32_t *db_pagesizep;
{
	*db_pagesizep = dbp->pgsize;
	return (0);
}

/*
 * __db_set_pagesize --
 *	DB->set_pagesize
 *
 * PUBLIC: int  __db_set_pagesize __P((DB *, u_int32_t));
 */
int
__db_set_pagesize(dbp, db_pagesize)
	DB *dbp;
	u_int32_t db_pagesize;
{
	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_pagesize");

	if (db_pagesize < DB_MIN_PGSIZE) {
		__db_err(dbp->dbenv, "page sizes may not be smaller than %lu",
		    (u_long)DB_MIN_PGSIZE);
		return (EINVAL);
	}
	if (db_pagesize > DB_MAX_PGSIZE) {
		__db_err(dbp->dbenv, "page sizes may not be larger than %lu",
		    (u_long)DB_MAX_PGSIZE);
		return (EINVAL);
	}

	/*
	 * We don't want anything that's not a power-of-2, as we rely on that
	 * for alignment of various types on the pages.
	 */
	if (!POWER_OF_TWO(db_pagesize)) {
		__db_err(dbp->dbenv, "page sizes must be a power-of-2");
		return (EINVAL);
	}

	/*
	 * XXX
	 * Should we be checking for a page size that's not a multiple of 512,
	 * so that we never try and write less than a disk sector?
	 */
	dbp->pgsize = db_pagesize;

	return (0);
}

static int
__db_set_paniccall(dbp, paniccall)
	DB *dbp;
	void (*paniccall) __P((DB_ENV *, int));
{
	return (__dbenv_set_paniccall(dbp->dbenv, paniccall));
}

#ifdef HAVE_RPC
/*
 * __dbcl_init --
 *	Initialize a DB structure on the server.
 */
static int
__dbcl_init(dbp, dbenv, flags)
	DB *dbp;
	DB_ENV *dbenv;
	u_int32_t flags;
{
	TAILQ_INIT(&dbp->free_queue);
	TAILQ_INIT(&dbp->active_queue);
	/* !!!
	 * Note that we don't need to initialize the join_queue;  it's
	 * not used in RPC clients.  See the comment in __dbcl_db_join_ret().
	 */

	dbp->associate = __dbcl_db_associate;
	dbp->close = __dbcl_db_close;
	dbp->cursor = __dbcl_db_cursor;
	dbp->del = __dbcl_db_del;
	dbp->err = __dbh_err;
	dbp->errx = __dbh_errx;
	dbp->fd = __dbcl_db_fd;
	dbp->get = __dbcl_db_get;
	dbp->get_byteswapped = __db_get_byteswapped;
	dbp->get_transactional = __db_get_transactional;
	dbp->get_type = __db_get_type;
	dbp->join = __dbcl_db_join;
	dbp->key_range = __dbcl_db_key_range;
	dbp->get_dbname = __dbcl_db_get_name;
	dbp->get_open_flags = __dbcl_db_get_open_flags;
	dbp->open = __dbcl_db_open_wrap;
	dbp->pget = __dbcl_db_pget;
	dbp->put = __dbcl_db_put;
	dbp->remove = __dbcl_db_remove;
	dbp->rename = __dbcl_db_rename;
	dbp->set_alloc = __dbcl_db_alloc;
	dbp->set_append_recno = __dbcl_db_set_append_recno;
	dbp->get_cachesize = __dbcl_db_get_cachesize;
	dbp->set_cachesize = __dbcl_db_cachesize;
	dbp->set_dup_compare = __dbcl_db_dup_compare;
	dbp->get_encrypt_flags = __dbcl_db_get_encrypt_flags;
	dbp->set_encrypt = __dbcl_db_encrypt;
	dbp->set_errcall = __db_set_errcall;
	dbp->get_errfile = __db_get_errfile;
	dbp->set_errfile = __db_set_errfile;
	dbp->get_errpfx = __db_get_errpfx;
	dbp->set_errpfx = __db_set_errpfx;
	dbp->set_feedback = __dbcl_db_feedback;
	dbp->get_flags = __dbcl_db_get_flags;
	dbp->set_flags = __dbcl_db_flags;
	dbp->get_lorder = __dbcl_db_get_lorder;
	dbp->set_lorder = __dbcl_db_lorder;
	dbp->get_pagesize = __dbcl_db_get_pagesize;
	dbp->set_pagesize = __dbcl_db_pagesize;
	dbp->set_paniccall = __dbcl_db_panic;
	dbp->stat = __dbcl_db_stat;
	dbp->sync = __dbcl_db_sync;
	dbp->truncate = __dbcl_db_truncate;
	dbp->upgrade = __dbcl_db_upgrade;
	dbp->verify = __dbcl_db_verify;

	/*
	 * Set all the method specific functions to client funcs as well.
	 */
	dbp->set_bt_compare = __dbcl_db_bt_compare;
	dbp->set_bt_maxkey = __dbcl_db_bt_maxkey;
	dbp->get_bt_minkey = __dbcl_db_get_bt_minkey;
	dbp->set_bt_minkey = __dbcl_db_bt_minkey;
	dbp->set_bt_prefix = __dbcl_db_bt_prefix;
	dbp->get_h_ffactor = __dbcl_db_get_h_ffactor;
	dbp->set_h_ffactor = __dbcl_db_h_ffactor;
	dbp->set_h_hash = __dbcl_db_h_hash;
	dbp->get_h_nelem = __dbcl_db_get_h_nelem;
	dbp->set_h_nelem = __dbcl_db_h_nelem;
	dbp->get_q_extentsize = __dbcl_db_get_extentsize;
	dbp->set_q_extentsize = __dbcl_db_extentsize;
	dbp->get_re_delim = __dbcl_db_get_re_delim;
	dbp->set_re_delim = __dbcl_db_re_delim;
	dbp->get_re_len = __dbcl_db_get_re_len;
	dbp->set_re_len = __dbcl_db_re_len;
	dbp->get_re_pad = __dbcl_db_get_re_pad;
	dbp->set_re_pad = __dbcl_db_re_pad;
	dbp->get_re_source = __dbcl_db_get_re_source;
	dbp->set_re_source = __dbcl_db_re_source;

	return (__dbcl_db_create(dbp, dbenv, flags));
}
#endif
