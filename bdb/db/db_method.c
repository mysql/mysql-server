/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_method.c,v 11.36 2000/12/21 09:17:04 krinsky Exp $";
#endif /* not lint */

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
#include "db_page.h"
#include "db_am.h"
#include "btree.h"
#include "hash.h"
#include "qam.h"
#include "xa.h"
#include "xa_ext.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int  __db_get_byteswapped __P((DB *));
static DBTYPE
	    __db_get_type __P((DB *));
static int  __db_init __P((DB *, u_int32_t));
static int  __db_key_range
		__P((DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t));
static int  __db_set_append_recno __P((DB *, int (*)(DB *, DBT *, db_recno_t)));
static int  __db_set_cachesize __P((DB *, u_int32_t, u_int32_t, int));
static int  __db_set_dup_compare
		__P((DB *, int (*)(DB *, const DBT *, const DBT *)));
static void __db_set_errcall __P((DB *, void (*)(const char *, char *)));
static void __db_set_errfile __P((DB *, FILE *));
static int  __db_set_feedback __P((DB *, void (*)(DB *, int, int)));
static int  __db_set_flags __P((DB *, u_int32_t));
static int  __db_set_lorder __P((DB *, int));
static int  __db_set_malloc __P((DB *, void *(*)(size_t)));
static int  __db_set_pagesize __P((DB *, u_int32_t));
static int  __db_set_realloc __P((DB *, void *(*)(void *, size_t)));
static void __db_set_errpfx __P((DB *, const char *));
static int  __db_set_paniccall __P((DB *, void (*)(DB_ENV *, int)));
static void __dbh_err __P((DB *, int, const char *, ...));
static void __dbh_errx __P((DB *, const char *, ...));

/*
 * db_create --
 *	DB constructor.
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
	if (dbenv != NULL && dbenv->cl_handle != NULL)
		ret = __dbcl_init(dbp, dbenv, flags);
	else
#endif
		ret = __db_init(dbp, flags);
	if (ret != 0) {
		__os_free(dbp, sizeof(*dbp));
		return (ret);
	}

	/* If we don't have an environment yet, allocate a local one. */
	if (dbenv == NULL) {
		if ((ret = db_env_create(&dbenv, 0)) != 0) {
			__os_free(dbp, sizeof(*dbp));
			return (ret);
		}
		dbenv->dblocal_ref = 0;
		F_SET(dbenv, DB_ENV_DBLOCAL);
	}
	if (F_ISSET(dbenv, DB_ENV_DBLOCAL))
		++dbenv->dblocal_ref;

	dbp->dbenv = dbenv;

	*dbpp = dbp;
	return (0);
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

	dbp->log_fileid = DB_LOGFILEID_INVALID;

	TAILQ_INIT(&dbp->free_queue);
	TAILQ_INIT(&dbp->active_queue);
	TAILQ_INIT(&dbp->join_queue);

	FLD_SET(dbp->am_ok,
	    DB_OK_BTREE | DB_OK_HASH | DB_OK_QUEUE | DB_OK_RECNO);

	dbp->close = __db_close;
	dbp->cursor = __db_cursor;
	dbp->del = NULL;		/* !!! Must be set by access method. */
	dbp->err = __dbh_err;
	dbp->errx = __dbh_errx;
	dbp->fd = __db_fd;
	dbp->get = __db_get;
	dbp->get_byteswapped = __db_get_byteswapped;
	dbp->get_type = __db_get_type;
	dbp->join = __db_join;
	dbp->key_range = __db_key_range;
	dbp->open = __db_open;
	dbp->put = __db_put;
	dbp->remove = __db_remove;
	dbp->rename = __db_rename;
	dbp->set_append_recno = __db_set_append_recno;
	dbp->set_cachesize = __db_set_cachesize;
	dbp->set_dup_compare = __db_set_dup_compare;
	dbp->set_errcall = __db_set_errcall;
	dbp->set_errfile = __db_set_errfile;
	dbp->set_errpfx = __db_set_errpfx;
	dbp->set_feedback = __db_set_feedback;
	dbp->set_flags = __db_set_flags;
	dbp->set_lorder = __db_set_lorder;
	dbp->set_malloc = __db_set_malloc;
	dbp->set_pagesize = __db_set_pagesize;
	dbp->set_paniccall = __db_set_paniccall;
	dbp->set_realloc = __db_set_realloc;
	dbp->stat = NULL;		/* !!! Must be set by access method. */
	dbp->sync = __db_sync;
	dbp->upgrade = __db_upgrade;
	dbp->verify = __db_verify;
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
#ifdef __STDC__
__dbh_err(DB *dbp, int error, const char *fmt, ...)
#else
__dbh_err(dbp, error, fmt, va_alist)
	DB *dbp;
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
	__db_real_err(dbp->dbenv, error, 1, 1, fmt, ap);

	va_end(ap);
}

/*
 * __dbh_errx --
 *	Error message.
 */
static void
#ifdef __STDC__
__dbh_errx(DB *dbp, const char *fmt, ...)
#else
__dbh_errx(dbp, fmt, va_alist)
	DB *dbp;
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
	__db_real_err(dbp->dbenv, 0, 0, 1, fmt, ap);

	va_end(ap);
}

/*
 * __db_get_byteswapped --
 *	Return if database requires byte swapping.
 */
static int
__db_get_byteswapped(dbp)
	DB *dbp;
{
	DB_ILLEGAL_BEFORE_OPEN(dbp, "get_byteswapped");

	return (F_ISSET(dbp, DB_AM_SWAP) ? 1 : 0);
}

/*
 * __db_get_type --
 *	Return type of underlying database.
 */
static DBTYPE
__db_get_type(dbp)
	DB *dbp;
{
	DB_ILLEGAL_BEFORE_OPEN(dbp, "get_type");

	return (dbp->type);
}

/*
 * __db_key_range --
 *	Return proportion of keys above and below given key.
 */
static int
__db_key_range(dbp, txn, key, kr, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	DB_KEY_RANGE *kr;
	u_int32_t flags;
{
	COMPQUIET(txn, NULL);
	COMPQUIET(key, NULL);
	COMPQUIET(kr, NULL);
	COMPQUIET(flags, 0);

	DB_ILLEGAL_BEFORE_OPEN(dbp, "key_range");
	DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE);

	return (EINVAL);
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
	DB_ILLEGAL_AFTER_OPEN(dbp, "set_append_recno");
	DB_ILLEGAL_METHOD(dbp, DB_OK_QUEUE | DB_OK_RECNO);

	dbp->db_append_recno = func;

	return (0);
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
	DB_ILLEGAL_IN_ENV(dbp, "set_cachesize");
	DB_ILLEGAL_AFTER_OPEN(dbp, "set_cachesize");

	return (dbp->dbenv->set_cachesize(
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
	DB_ILLEGAL_AFTER_OPEN(dbp, "dup_compare");
	DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE | DB_OK_HASH);

	dbp->dup_compare = func;

	return (0);
}

static void
__db_set_errcall(dbp, errcall)
	DB *dbp;
	void (*errcall) __P((const char *, char *));
{
	dbp->dbenv->set_errcall(dbp->dbenv, errcall);
}

static void
__db_set_errfile(dbp, errfile)
	DB *dbp;
	FILE *errfile;
{
	dbp->dbenv->set_errfile(dbp->dbenv, errfile);
}

static void
__db_set_errpfx(dbp, errpfx)
	DB *dbp;
	const char *errpfx;
{
	dbp->dbenv->set_errpfx(dbp->dbenv, errpfx);
}

static int
__db_set_feedback(dbp, feedback)
	DB *dbp;
	void (*feedback) __P((DB *, int, int));
{
	dbp->db_feedback = feedback;
	return (0);
}

static int
__db_set_flags(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	int ret;

	/*
	 * !!!
	 * The hash access method only takes two flags: DB_DUP and DB_DUPSORT.
	 * The Btree access method uses them for the same purposes, and so we
	 * resolve them there.
	 *
	 * The queue access method takes no flags.
	 */
	if ((ret = __bam_set_flags(dbp, &flags)) != 0)
		return (ret);
	if ((ret = __ram_set_flags(dbp, &flags)) != 0)
		return (ret);

	return (flags == 0 ? 0 : __db_ferr(dbp->dbenv, "DB->set_flags", 0));
}

static int
__db_set_lorder(dbp, db_lorder)
	DB *dbp;
	int db_lorder;
{
	int ret;

	DB_ILLEGAL_AFTER_OPEN(dbp, "set_lorder");

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
__db_set_malloc(dbp, func)
	DB *dbp;
	void *(*func) __P((size_t));
{
	DB_ILLEGAL_AFTER_OPEN(dbp, "set_malloc");

	dbp->db_malloc = func;
	return (0);
}

static int
__db_set_pagesize(dbp, db_pagesize)
	DB *dbp;
	u_int32_t db_pagesize;
{
	DB_ILLEGAL_AFTER_OPEN(dbp, "set_pagesize");

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
	if ((u_int32_t)1 << __db_log2(db_pagesize) != db_pagesize) {
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
__db_set_realloc(dbp, func)
	DB *dbp;
	void *(*func) __P((void *, size_t));
{
	DB_ILLEGAL_AFTER_OPEN(dbp, "set_realloc");

	dbp->db_realloc = func;
	return (0);
}

static int
__db_set_paniccall(dbp, paniccall)
	DB *dbp;
	void (*paniccall) __P((DB_ENV *, int));
{
	return (dbp->dbenv->set_paniccall(dbp->dbenv, paniccall));
}

#ifdef HAVE_RPC
/*
 * __dbcl_init --
 *	Initialize a DB structure on the server.
 *
 * PUBLIC: #ifdef HAVE_RPC
 * PUBLIC: int __dbcl_init __P((DB *, DB_ENV *, u_int32_t));
 * PUBLIC: #endif
 */
int
__dbcl_init(dbp, dbenv, flags)
	DB *dbp;
	DB_ENV *dbenv;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_create_reply *replyp;
	__db_create_msg req;
	int ret;

	TAILQ_INIT(&dbp->free_queue);
	TAILQ_INIT(&dbp->active_queue);
	/* !!!
	 * Note that we don't need to initialize the join_queue;  it's
	 * not used in RPC clients.  See the comment in __dbcl_db_join_ret().
	 */

	dbp->close = __dbcl_db_close;
	dbp->cursor = __dbcl_db_cursor;
	dbp->del = __dbcl_db_del;
	dbp->err = __dbh_err;
	dbp->errx = __dbh_errx;
	dbp->fd = __dbcl_db_fd;
	dbp->get = __dbcl_db_get;
	dbp->get_byteswapped = __dbcl_db_swapped;
	dbp->get_type = __db_get_type;
	dbp->join = __dbcl_db_join;
	dbp->key_range = __dbcl_db_key_range;
	dbp->open = __dbcl_db_open;
	dbp->put = __dbcl_db_put;
	dbp->remove = __dbcl_db_remove;
	dbp->rename = __dbcl_db_rename;
	dbp->set_append_recno = __dbcl_db_set_append_recno;
	dbp->set_cachesize = __dbcl_db_cachesize;
	dbp->set_dup_compare = NULL;
	dbp->set_errcall = __db_set_errcall;
	dbp->set_errfile = __db_set_errfile;
	dbp->set_errpfx = __db_set_errpfx;
	dbp->set_feedback = __dbcl_db_feedback;
	dbp->set_flags = __dbcl_db_flags;
	dbp->set_lorder = __dbcl_db_lorder;
	dbp->set_malloc = __dbcl_db_malloc;
	dbp->set_pagesize = __dbcl_db_pagesize;
	dbp->set_paniccall = __dbcl_db_panic;
	dbp->set_q_extentsize = __dbcl_db_extentsize;
	dbp->set_realloc = __dbcl_db_realloc;
	dbp->stat = __dbcl_db_stat;
	dbp->sync = __dbcl_db_sync;
	dbp->upgrade = __dbcl_db_upgrade;

	/*
	 * Set all the method specific functions to client funcs as well.
	 */
	dbp->set_bt_compare = __dbcl_db_bt_compare;
	dbp->set_bt_maxkey = __dbcl_db_bt_maxkey;
	dbp->set_bt_minkey = __dbcl_db_bt_minkey;
	dbp->set_bt_prefix = __dbcl_db_bt_prefix;
	dbp->set_h_ffactor = __dbcl_db_h_ffactor;
	dbp->set_h_hash = __dbcl_db_h_hash;
	dbp->set_h_nelem = __dbcl_db_h_nelem;
	dbp->set_re_delim = __dbcl_db_re_delim;
	dbp->set_re_len = __dbcl_db_re_len;
	dbp->set_re_pad = __dbcl_db_re_pad;
	dbp->set_re_source = __dbcl_db_re_source;
/*
	dbp->set_q_extentsize = __dbcl_db_q_extentsize;
*/

	cl = (CLIENT *)dbenv->cl_handle;
	req.flags = flags;
	req.envpcl_id = dbenv->cl_id;

	/*
	 * CALL THE SERVER
	 */
	replyp = __db_db_create_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		return (DB_NOSERVER);
	}

	if ((ret = replyp->status) != 0)
		return (ret);

	dbp->cl_id = replyp->dbpcl_id;
	return (0);
}
#endif
