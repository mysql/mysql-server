/* Do not edit: automatically built by gen_rpc.awk. */
#include "db_config.h"

#ifdef HAVE_RPC
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>

#include <errno.h>
#include <string.h>
#endif
#include "db_server.h"

#include "db_int.h"
#include "db_page.h"
#include "db_ext.h"
#include "mp.h"
#include "rpc_client_ext.h"
#include "txn.h"

#include "gen_client_ext.h"

int
__dbcl_env_cachesize(dbenv, gbytes, bytes, ncache)
	DB_ENV * dbenv;
	u_int32_t gbytes;
	u_int32_t bytes;
	int ncache;
{
	CLIENT *cl;
	__env_cachesize_msg req;
	static __env_cachesize_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___env_cachesize_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		req.dbenvcl_id = 0;
	else
		req.dbenvcl_id = dbenv->cl_id;
	req.gbytes = gbytes;
	req.bytes = bytes;
	req.ncache = ncache;

	replyp = __db_env_cachesize_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_env_close(dbenv, flags)
	DB_ENV * dbenv;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_close_msg req;
	static __env_close_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___env_close_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		req.dbenvcl_id = 0;
	else
		req.dbenvcl_id = dbenv->cl_id;
	req.flags = flags;

	replyp = __db_env_close_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_env_close_ret(dbenv, flags, replyp));
out:
	return (ret);
}

int
__dbcl_rpc_illegal(dbenv, name)
	DB_ENV *dbenv;
	char *name;
{
	__db_err(dbenv,
	    "%s method meaningless in RPC environment", name);
	return (__db_eopnotsup(dbenv));
}

int
__dbcl_set_data_dir(dbenv, dir)
	DB_ENV * dbenv;
	const char * dir;
{
	COMPQUIET(dir, NULL);
	return (__dbcl_rpc_illegal(dbenv, "set_data_dir"));
}

int
__dbcl_env_set_feedback(dbenv, func0)
	DB_ENV * dbenv;
	void (*func0) __P((DB_ENV *, int, int));
{
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "env_set_feedback"));
}

int
__dbcl_env_flags(dbenv, flags, onoff)
	DB_ENV * dbenv;
	u_int32_t flags;
	int onoff;
{
	CLIENT *cl;
	__env_flags_msg req;
	static __env_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___env_flags_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		req.dbenvcl_id = 0;
	else
		req.dbenvcl_id = dbenv->cl_id;
	req.flags = flags;
	req.onoff = onoff;

	replyp = __db_env_flags_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_set_lg_bsize(dbenv, bsize)
	DB_ENV * dbenv;
	u_int32_t bsize;
{
	COMPQUIET(bsize, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lg_bsize"));
}

int
__dbcl_set_lg_dir(dbenv, dir)
	DB_ENV * dbenv;
	const char * dir;
{
	COMPQUIET(dir, NULL);
	return (__dbcl_rpc_illegal(dbenv, "set_lg_dir"));
}

int
__dbcl_set_lg_max(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lg_max"));
}

int
__dbcl_set_lk_conflict(dbenv, conflicts, modes)
	DB_ENV * dbenv;
	u_int8_t * conflicts;
	int modes;
{
	COMPQUIET(conflicts, 0);
	COMPQUIET(modes, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_conflict"));
}

int
__dbcl_set_lk_detect(dbenv, detect)
	DB_ENV * dbenv;
	u_int32_t detect;
{
	COMPQUIET(detect, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_detect"));
}

int
__dbcl_set_lk_max(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_max"));
}

int
__dbcl_set_lk_max_locks(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_max_locks"));
}

int
__dbcl_set_lk_max_lockers(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_max_lockers"));
}

int
__dbcl_set_lk_max_objects(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_max_objects"));
}

int
__dbcl_set_mp_mmapsize(dbenv, mmapsize)
	DB_ENV * dbenv;
	size_t mmapsize;
{
	COMPQUIET(mmapsize, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_mp_mmapsize"));
}

int
__dbcl_set_mutex_locks(dbenv, do_lock)
	DB_ENV * dbenv;
	int do_lock;
{
	COMPQUIET(do_lock, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_mutex_locks"));
}

int
__dbcl_env_open(dbenv, home, flags, mode)
	DB_ENV * dbenv;
	const char * home;
	u_int32_t flags;
	int mode;
{
	CLIENT *cl;
	__env_open_msg req;
	static __env_open_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___env_open_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		req.dbenvcl_id = 0;
	else
		req.dbenvcl_id = dbenv->cl_id;
	if (home == NULL)
		req.home = "";
	else
		req.home = (char *)home;
	req.flags = flags;
	req.mode = mode;

	replyp = __db_env_open_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_env_open_ret(dbenv, home, flags, mode, replyp));
out:
	return (ret);
}

int
__dbcl_env_paniccall(dbenv, func0)
	DB_ENV * dbenv;
	void (*func0) __P((DB_ENV *, int));
{
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "env_paniccall"));
}

int
__dbcl_set_recovery_init(dbenv, func0)
	DB_ENV * dbenv;
	int (*func0) __P((DB_ENV *));
{
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_recovery_init"));
}

int
__dbcl_env_remove(dbenv, home, flags)
	DB_ENV * dbenv;
	const char * home;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_remove_msg req;
	static __env_remove_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___env_remove_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		req.dbenvcl_id = 0;
	else
		req.dbenvcl_id = dbenv->cl_id;
	if (home == NULL)
		req.home = "";
	else
		req.home = (char *)home;
	req.flags = flags;

	replyp = __db_env_remove_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_env_remove_ret(dbenv, home, flags, replyp));
out:
	return (ret);
}

int
__dbcl_set_shm_key(dbenv, shm_key)
	DB_ENV * dbenv;
	long shm_key;
{
	COMPQUIET(shm_key, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_shm_key"));
}

int
__dbcl_set_tmp_dir(dbenv, dir)
	DB_ENV * dbenv;
	const char * dir;
{
	COMPQUIET(dir, NULL);
	return (__dbcl_rpc_illegal(dbenv, "set_tmp_dir"));
}

int
__dbcl_set_tx_recover(dbenv, func0)
	DB_ENV * dbenv;
	int (*func0) __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
{
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_tx_recover"));
}

int
__dbcl_set_tx_max(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_tx_max"));
}

int
__dbcl_set_tx_timestamp(dbenv, max)
	DB_ENV * dbenv;
	time_t * max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_tx_timestamp"));
}

int
__dbcl_set_verbose(dbenv, which, onoff)
	DB_ENV * dbenv;
	u_int32_t which;
	int onoff;
{
	COMPQUIET(which, 0);
	COMPQUIET(onoff, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_verbose"));
}

int
__dbcl_txn_abort(txnp)
	DB_TXN * txnp;
{
	CLIENT *cl;
	__txn_abort_msg req;
	static __txn_abort_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___txn_abort_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		req.txnpcl_id = 0;
	else
		req.txnpcl_id = txnp->txnid;

	replyp = __db_txn_abort_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_txn_abort_ret(txnp, replyp));
out:
	return (ret);
}

int
__dbcl_txn_begin(envp, parent, txnpp, flags)
	DB_ENV * envp;
	DB_TXN * parent;
	DB_TXN ** txnpp;
	u_int32_t flags;
{
	CLIENT *cl;
	__txn_begin_msg req;
	static __txn_begin_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (envp == NULL || envp->cl_handle == NULL) {
		__db_err(envp, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___txn_begin_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)envp->cl_handle;

	if (envp == NULL)
		req.envpcl_id = 0;
	else
		req.envpcl_id = envp->cl_id;
	if (parent == NULL)
		req.parentcl_id = 0;
	else
		req.parentcl_id = parent->txnid;
	req.flags = flags;

	replyp = __db_txn_begin_1(&req, cl);
	if (replyp == NULL) {
		__db_err(envp, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_txn_begin_ret(envp, parent, txnpp, flags, replyp));
out:
	return (ret);
}

int
__dbcl_txn_checkpoint(dbenv, kbyte, min)
	DB_ENV * dbenv;
	u_int32_t kbyte;
	u_int32_t min;
{
	COMPQUIET(kbyte, 0);
	COMPQUIET(min, 0);
	return (__dbcl_rpc_illegal(dbenv, "txn_checkpoint"));
}

int
__dbcl_txn_commit(txnp, flags)
	DB_TXN * txnp;
	u_int32_t flags;
{
	CLIENT *cl;
	__txn_commit_msg req;
	static __txn_commit_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___txn_commit_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		req.txnpcl_id = 0;
	else
		req.txnpcl_id = txnp->txnid;
	req.flags = flags;

	replyp = __db_txn_commit_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_txn_commit_ret(txnp, flags, replyp));
out:
	return (ret);
}

int
__dbcl_txn_prepare(txnp)
	DB_TXN * txnp;
{
	DB_ENV *dbenv;

	dbenv = txnp->mgrp->dbenv;
	return (__dbcl_rpc_illegal(dbenv, "txn_prepare"));
}

int
__dbcl_txn_stat(dbenv, statp, func0)
	DB_ENV * dbenv;
	DB_TXN_STAT ** statp;
	void *(*func0) __P((size_t));
{
	COMPQUIET(statp, 0);
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "txn_stat"));
}

int
__dbcl_db_bt_compare(dbp, func0)
	DB * dbp;
	int (*func0) __P((DB *, const DBT *, const DBT *));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_bt_compare"));
}

int
__dbcl_db_bt_maxkey(dbp, maxkey)
	DB * dbp;
	u_int32_t maxkey;
{
	CLIENT *cl;
	__db_bt_maxkey_msg req;
	static __db_bt_maxkey_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_bt_maxkey_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.maxkey = maxkey;

	replyp = __db_db_bt_maxkey_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_bt_minkey(dbp, minkey)
	DB * dbp;
	u_int32_t minkey;
{
	CLIENT *cl;
	__db_bt_minkey_msg req;
	static __db_bt_minkey_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_bt_minkey_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.minkey = minkey;

	replyp = __db_db_bt_minkey_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_bt_prefix(dbp, func0)
	DB * dbp;
	size_t (*func0) __P((DB *, const DBT *, const DBT *));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_bt_prefix"));
}

int
__dbcl_db_set_append_recno(dbp, func0)
	DB * dbp;
	int (*func0) __P((DB *, DBT *, db_recno_t));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_set_append_recno"));
}

int
__dbcl_db_cachesize(dbp, gbytes, bytes, ncache)
	DB * dbp;
	u_int32_t gbytes;
	u_int32_t bytes;
	int ncache;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(gbytes, 0);
	COMPQUIET(bytes, 0);
	COMPQUIET(ncache, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_cachesize"));
}

int
__dbcl_db_close(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_close_msg req;
	static __db_close_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_close_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.flags = flags;

	replyp = __db_db_close_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_close_ret(dbp, flags, replyp));
out:
	return (ret);
}

int
__dbcl_db_del(dbp, txnp, key, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_del_msg req;
	static __db_del_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_del_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		req.txnpcl_id = 0;
	else
		req.txnpcl_id = txnp->txnid;
	req.keydlen = key->dlen;
	req.keydoff = key->doff;
	req.keyflags = key->flags;
	req.keydata.keydata_val = key->data;
	req.keydata.keydata_len = key->size;
	req.flags = flags;

	replyp = __db_db_del_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_extentsize(dbp, extentsize)
	DB * dbp;
	u_int32_t extentsize;
{
	CLIENT *cl;
	__db_extentsize_msg req;
	static __db_extentsize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_extentsize_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.extentsize = extentsize;

	replyp = __db_db_extentsize_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_fd(dbp, fdp)
	DB * dbp;
	int * fdp;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(fdp, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_fd"));
}

int
__dbcl_db_feedback(dbp, func0)
	DB * dbp;
	void (*func0) __P((DB *, int, int));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_feedback"));
}

int
__dbcl_db_flags(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_flags_msg req;
	static __db_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_flags_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.flags = flags;

	replyp = __db_db_flags_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_get(dbp, txnp, key, data, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_get_msg req;
	static __db_get_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_get_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		req.txnpcl_id = 0;
	else
		req.txnpcl_id = txnp->txnid;
	req.keydlen = key->dlen;
	req.keydoff = key->doff;
	req.keyflags = key->flags;
	req.keydata.keydata_val = key->data;
	req.keydata.keydata_len = key->size;
	req.datadlen = data->dlen;
	req.datadoff = data->doff;
	req.dataflags = data->flags;
	req.datadata.datadata_val = data->data;
	req.datadata.datadata_len = data->size;
	req.flags = flags;

	replyp = __db_db_get_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_get_ret(dbp, txnp, key, data, flags, replyp));
out:
	return (ret);
}

int
__dbcl_db_h_ffactor(dbp, ffactor)
	DB * dbp;
	u_int32_t ffactor;
{
	CLIENT *cl;
	__db_h_ffactor_msg req;
	static __db_h_ffactor_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_h_ffactor_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.ffactor = ffactor;

	replyp = __db_db_h_ffactor_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_h_hash(dbp, func0)
	DB * dbp;
	u_int32_t (*func0) __P((DB *, const void *, u_int32_t));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_h_hash"));
}

int
__dbcl_db_h_nelem(dbp, nelem)
	DB * dbp;
	u_int32_t nelem;
{
	CLIENT *cl;
	__db_h_nelem_msg req;
	static __db_h_nelem_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_h_nelem_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.nelem = nelem;

	replyp = __db_db_h_nelem_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_key_range(dbp, txnp, key, range, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DB_KEY_RANGE * range;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_key_range_msg req;
	static __db_key_range_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_key_range_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		req.txnpcl_id = 0;
	else
		req.txnpcl_id = txnp->txnid;
	req.keydlen = key->dlen;
	req.keydoff = key->doff;
	req.keyflags = key->flags;
	req.keydata.keydata_val = key->data;
	req.keydata.keydata_len = key->size;
	req.flags = flags;

	replyp = __db_db_key_range_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_key_range_ret(dbp, txnp, key, range, flags, replyp));
out:
	return (ret);
}

int
__dbcl_db_lorder(dbp, lorder)
	DB * dbp;
	int lorder;
{
	CLIENT *cl;
	__db_lorder_msg req;
	static __db_lorder_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_lorder_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.lorder = lorder;

	replyp = __db_db_lorder_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_malloc(dbp, func0)
	DB * dbp;
	void *(*func0) __P((size_t));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_malloc"));
}

int
__dbcl_db_open(dbp, name, subdb, type, flags, mode)
	DB * dbp;
	const char * name;
	const char * subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	CLIENT *cl;
	__db_open_msg req;
	static __db_open_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_open_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if (name == NULL)
		req.name = "";
	else
		req.name = (char *)name;
	if (subdb == NULL)
		req.subdb = "";
	else
		req.subdb = (char *)subdb;
	req.type = type;
	req.flags = flags;
	req.mode = mode;

	replyp = __db_db_open_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_open_ret(dbp, name, subdb, type, flags, mode, replyp));
out:
	return (ret);
}

int
__dbcl_db_pagesize(dbp, pagesize)
	DB * dbp;
	u_int32_t pagesize;
{
	CLIENT *cl;
	__db_pagesize_msg req;
	static __db_pagesize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_pagesize_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.pagesize = pagesize;

	replyp = __db_db_pagesize_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_panic(dbp, func0)
	DB * dbp;
	void (*func0) __P((DB_ENV *, int));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_panic"));
}

int
__dbcl_db_put(dbp, txnp, key, data, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_put_msg req;
	static __db_put_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_put_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		req.txnpcl_id = 0;
	else
		req.txnpcl_id = txnp->txnid;
	req.keydlen = key->dlen;
	req.keydoff = key->doff;
	req.keyflags = key->flags;
	req.keydata.keydata_val = key->data;
	req.keydata.keydata_len = key->size;
	req.datadlen = data->dlen;
	req.datadoff = data->doff;
	req.dataflags = data->flags;
	req.datadata.datadata_val = data->data;
	req.datadata.datadata_len = data->size;
	req.flags = flags;

	replyp = __db_db_put_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_put_ret(dbp, txnp, key, data, flags, replyp));
out:
	return (ret);
}

int
__dbcl_db_realloc(dbp, func0)
	DB * dbp;
	void *(*func0) __P((void *, size_t));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_realloc"));
}

int
__dbcl_db_re_delim(dbp, delim)
	DB * dbp;
	int delim;
{
	CLIENT *cl;
	__db_re_delim_msg req;
	static __db_re_delim_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_re_delim_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.delim = delim;

	replyp = __db_db_re_delim_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_re_len(dbp, len)
	DB * dbp;
	u_int32_t len;
{
	CLIENT *cl;
	__db_re_len_msg req;
	static __db_re_len_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_re_len_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.len = len;

	replyp = __db_db_re_len_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_re_pad(dbp, pad)
	DB * dbp;
	int pad;
{
	CLIENT *cl;
	__db_re_pad_msg req;
	static __db_re_pad_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_re_pad_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.pad = pad;

	replyp = __db_db_re_pad_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_re_source(dbp, re_source)
	DB * dbp;
	const char * re_source;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(re_source, NULL);
	return (__dbcl_rpc_illegal(dbenv, "db_re_source"));
}

int
__dbcl_db_remove(dbp, name, subdb, flags)
	DB * dbp;
	const char * name;
	const char * subdb;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_remove_msg req;
	static __db_remove_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_remove_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if (name == NULL)
		req.name = "";
	else
		req.name = (char *)name;
	if (subdb == NULL)
		req.subdb = "";
	else
		req.subdb = (char *)subdb;
	req.flags = flags;

	replyp = __db_db_remove_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_remove_ret(dbp, name, subdb, flags, replyp));
out:
	return (ret);
}

int
__dbcl_db_rename(dbp, name, subdb, newname, flags)
	DB * dbp;
	const char * name;
	const char * subdb;
	const char * newname;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_rename_msg req;
	static __db_rename_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_rename_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if (name == NULL)
		req.name = "";
	else
		req.name = (char *)name;
	if (subdb == NULL)
		req.subdb = "";
	else
		req.subdb = (char *)subdb;
	if (newname == NULL)
		req.newname = "";
	else
		req.newname = (char *)newname;
	req.flags = flags;

	replyp = __db_db_rename_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_rename_ret(dbp, name, subdb, newname, flags, replyp));
out:
	return (ret);
}

int
__dbcl_db_stat(dbp, sp, func0, flags)
	DB * dbp;
	void * sp;
	void *(*func0) __P((size_t));
	u_int32_t flags;
{
	CLIENT *cl;
	__db_stat_msg req;
	static __db_stat_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_stat_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (func0 != NULL) {
		__db_err(sp, "User functions not supported in RPC.");
		return (EINVAL);
	}
	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.flags = flags;

	replyp = __db_db_stat_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_stat_ret(dbp, sp, func0, flags, replyp));
out:
	return (ret);
}

int
__dbcl_db_swapped(dbp)
	DB * dbp;
{
	CLIENT *cl;
	__db_swapped_msg req;
	static __db_swapped_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_swapped_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;

	replyp = __db_db_swapped_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_sync(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_sync_msg req;
	static __db_sync_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_sync_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	req.flags = flags;

	replyp = __db_db_sync_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_db_upgrade(dbp, fname, flags)
	DB * dbp;
	const char * fname;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(fname, NULL);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_upgrade"));
}

int
__dbcl_db_cursor(dbp, txnp, dbcpp, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBC ** dbcpp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_cursor_msg req;
	static __db_cursor_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_cursor_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		req.txnpcl_id = 0;
	else
		req.txnpcl_id = txnp->txnid;
	req.flags = flags;

	replyp = __db_db_cursor_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_db_cursor_ret(dbp, txnp, dbcpp, flags, replyp));
out:
	return (ret);
}

static int __dbcl_db_join_curslist __P((__db_join_curslist **, DBC **));
static void __dbcl_db_join_cursfree __P((__db_join_curslist **));
int
__dbcl_db_join(dbp, curs, dbcp, flags)
	DB * dbp;
	DBC ** curs;
	DBC ** dbcp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_join_msg req;
	static __db_join_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___db_join_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		req.dbpcl_id = 0;
	else
		req.dbpcl_id = dbp->cl_id;
	if ((ret = __dbcl_db_join_curslist(&req.curslist, curs)) != 0)
		goto out;
	req.flags = flags;

	replyp = __db_db_join_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	__dbcl_db_join_cursfree(&req.curslist);
	return (__dbcl_db_join_ret(dbp, curs, dbcp, flags, replyp));
out:
	__dbcl_db_join_cursfree(&req.curslist);
	return (ret);
}

int
__dbcl_db_join_curslist(locp, pp)
	__db_join_curslist **locp;
	DBC ** pp;
{
	DBC ** p;
	u_int32_t *q;
	int ret;
	__db_join_curslist *nl, **nlp;

	*locp = NULL;
	if (pp == NULL)
		return (0);
	nlp = locp;
	for (p = pp; *p != 0; p++) {
		if ((ret = __os_malloc(NULL, sizeof(*nl), NULL, nlp)) != 0)
			goto out;
		nl = *nlp;
		nl->next = NULL;
		nl->ent.ent_val = NULL;
		nl->ent.ent_len = 0;
		if ((ret = __os_malloc(NULL, sizeof(u_int32_t), NULL, &nl->ent.ent_val)) != 0)
			goto out;
		q = (u_int32_t *)nl->ent.ent_val;
		*q = (*p)->cl_id;
		nl->ent.ent_len = sizeof(u_int32_t);
		nlp = &nl->next;
	}
	return (0);
out:
	__dbcl_db_join_cursfree(locp);
	return (ret);
}

void
__dbcl_db_join_cursfree(locp)
	__db_join_curslist **locp;
{
	__db_join_curslist *nl, *nl1;

	if (locp == NULL)
		return;
	for (nl = *locp; nl != NULL; nl = nl1) {
		nl1 = nl->next;
		if (nl->ent.ent_val)
			__os_free(nl->ent.ent_val, nl->ent.ent_len);
		__os_free(nl, sizeof(*nl));
	}
}

int
__dbcl_dbc_close(dbc)
	DBC * dbc;
{
	CLIENT *cl;
	__dbc_close_msg req;
	static __dbc_close_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___dbc_close_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		req.dbccl_id = 0;
	else
		req.dbccl_id = dbc->cl_id;

	replyp = __db_dbc_close_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_dbc_close_ret(dbc, replyp));
out:
	return (ret);
}

int
__dbcl_dbc_count(dbc, countp, flags)
	DBC * dbc;
	db_recno_t * countp;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_count_msg req;
	static __dbc_count_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___dbc_count_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		req.dbccl_id = 0;
	else
		req.dbccl_id = dbc->cl_id;
	req.flags = flags;

	replyp = __db_dbc_count_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_dbc_count_ret(dbc, countp, flags, replyp));
out:
	return (ret);
}

int
__dbcl_dbc_del(dbc, flags)
	DBC * dbc;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_del_msg req;
	static __dbc_del_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___dbc_del_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		req.dbccl_id = 0;
	else
		req.dbccl_id = dbc->cl_id;
	req.flags = flags;

	replyp = __db_dbc_del_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	return (ret);
}

int
__dbcl_dbc_dup(dbc, dbcp, flags)
	DBC * dbc;
	DBC ** dbcp;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_dup_msg req;
	static __dbc_dup_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___dbc_dup_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		req.dbccl_id = 0;
	else
		req.dbccl_id = dbc->cl_id;
	req.flags = flags;

	replyp = __db_dbc_dup_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_dbc_dup_ret(dbc, dbcp, flags, replyp));
out:
	return (ret);
}

int
__dbcl_dbc_get(dbc, key, data, flags)
	DBC * dbc;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_get_msg req;
	static __dbc_get_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___dbc_get_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		req.dbccl_id = 0;
	else
		req.dbccl_id = dbc->cl_id;
	req.keydlen = key->dlen;
	req.keydoff = key->doff;
	req.keyflags = key->flags;
	req.keydata.keydata_val = key->data;
	req.keydata.keydata_len = key->size;
	req.datadlen = data->dlen;
	req.datadoff = data->doff;
	req.dataflags = data->flags;
	req.datadata.datadata_val = data->data;
	req.datadata.datadata_len = data->size;
	req.flags = flags;

	replyp = __db_dbc_get_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_dbc_get_ret(dbc, key, data, flags, replyp));
out:
	return (ret);
}

int
__dbcl_dbc_put(dbc, key, data, flags)
	DBC * dbc;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_put_msg req;
	static __dbc_put_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = NULL;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || dbenv->cl_handle == NULL) {
		__db_err(dbenv, "No server environment.");
		return (DB_NOSERVER);
	}

	if (replyp != NULL) {
		xdr_free((xdrproc_t)xdr___dbc_put_reply, (void *)replyp);
		replyp = NULL;
	}
	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		req.dbccl_id = 0;
	else
		req.dbccl_id = dbc->cl_id;
	req.keydlen = key->dlen;
	req.keydoff = key->doff;
	req.keyflags = key->flags;
	req.keydata.keydata_val = key->data;
	req.keydata.keydata_len = key->size;
	req.datadlen = data->dlen;
	req.datadoff = data->doff;
	req.dataflags = data->flags;
	req.datadata.datadata_val = data->data;
	req.datadata.datadata_len = data->size;
	req.flags = flags;

	replyp = __db_dbc_put_1(&req, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	return (__dbcl_dbc_put_ret(dbc, key, data, flags, replyp));
out:
	return (ret);
}

int
__dbcl_lock_detect(dbenv, flags, atype, aborted)
	DB_ENV * dbenv;
	u_int32_t flags;
	u_int32_t atype;
	int * aborted;
{
	COMPQUIET(flags, 0);
	COMPQUIET(atype, 0);
	COMPQUIET(aborted, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_detect"));
}

int
__dbcl_lock_get(dbenv, locker, flags, obj, mode, lock)
	DB_ENV * dbenv;
	u_int32_t locker;
	u_int32_t flags;
	const DBT * obj;
	db_lockmode_t mode;
	DB_LOCK * lock;
{
	COMPQUIET(locker, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(obj, NULL);
	COMPQUIET(mode, 0);
	COMPQUIET(lock, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_get"));
}

int
__dbcl_lock_id(dbenv, idp)
	DB_ENV * dbenv;
	u_int32_t * idp;
{
	COMPQUIET(idp, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_id"));
}

int
__dbcl_lock_put(dbenv, lock)
	DB_ENV * dbenv;
	DB_LOCK * lock;
{
	COMPQUIET(lock, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_put"));
}

int
__dbcl_lock_stat(dbenv, statp, func0)
	DB_ENV * dbenv;
	DB_LOCK_STAT ** statp;
	void *(*func0) __P((size_t));
{
	COMPQUIET(statp, 0);
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_stat"));
}

int
__dbcl_lock_vec(dbenv, locker, flags, list, nlist, elistp)
	DB_ENV * dbenv;
	u_int32_t locker;
	u_int32_t flags;
	DB_LOCKREQ * list;
	int nlist;
	DB_LOCKREQ ** elistp;
{
	COMPQUIET(locker, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(list, 0);
	COMPQUIET(nlist, 0);
	COMPQUIET(elistp, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_vec"));
}

int
__dbcl_log_archive(dbenv, listp, flags, func0)
	DB_ENV * dbenv;
	char *** listp;
	u_int32_t flags;
	void *(*func0) __P((size_t));
{
	COMPQUIET(listp, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_archive"));
}

int
__dbcl_log_file(dbenv, lsn, namep, len)
	DB_ENV * dbenv;
	const DB_LSN * lsn;
	char * namep;
	size_t len;
{
	COMPQUIET(lsn, NULL);
	COMPQUIET(namep, NULL);
	COMPQUIET(len, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_file"));
}

int
__dbcl_log_flush(dbenv, lsn)
	DB_ENV * dbenv;
	const DB_LSN * lsn;
{
	COMPQUIET(lsn, NULL);
	return (__dbcl_rpc_illegal(dbenv, "log_flush"));
}

int
__dbcl_log_get(dbenv, lsn, data, flags)
	DB_ENV * dbenv;
	DB_LSN * lsn;
	DBT * data;
	u_int32_t flags;
{
	COMPQUIET(lsn, 0);
	COMPQUIET(data, NULL);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_get"));
}

int
__dbcl_log_put(dbenv, lsn, data, flags)
	DB_ENV * dbenv;
	DB_LSN * lsn;
	const DBT * data;
	u_int32_t flags;
{
	COMPQUIET(lsn, 0);
	COMPQUIET(data, NULL);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_put"));
}

int
__dbcl_log_register(dbenv, dbp, namep)
	DB_ENV * dbenv;
	DB * dbp;
	const char * namep;
{
	COMPQUIET(dbp, 0);
	COMPQUIET(namep, NULL);
	return (__dbcl_rpc_illegal(dbenv, "log_register"));
}

int
__dbcl_log_stat(dbenv, statp, func0)
	DB_ENV * dbenv;
	DB_LOG_STAT ** statp;
	void *(*func0) __P((size_t));
{
	COMPQUIET(statp, 0);
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_stat"));
}

int
__dbcl_log_unregister(dbenv, dbp)
	DB_ENV * dbenv;
	DB * dbp;
{
	COMPQUIET(dbp, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_unregister"));
}

int
__dbcl_memp_fclose(mpf)
	DB_MPOOLFILE * mpf;
{
	DB_ENV *dbenv;

	dbenv = mpf->dbmp->dbenv;
	return (__dbcl_rpc_illegal(dbenv, "memp_fclose"));
}

int
__dbcl_memp_fget(mpf, pgno, flags, pagep)
	DB_MPOOLFILE * mpf;
	db_pgno_t * pgno;
	u_int32_t flags;
	void ** pagep;
{
	DB_ENV *dbenv;

	dbenv = mpf->dbmp->dbenv;
	COMPQUIET(pgno, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(pagep, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_fget"));
}

int
__dbcl_memp_fopen(dbenv, file, flags, mode, pagesize, finfop, mpf)
	DB_ENV * dbenv;
	const char * file;
	u_int32_t flags;
	int mode;
	size_t pagesize;
	DB_MPOOL_FINFO * finfop;
	DB_MPOOLFILE ** mpf;
{
	COMPQUIET(file, NULL);
	COMPQUIET(flags, 0);
	COMPQUIET(mode, 0);
	COMPQUIET(pagesize, 0);
	COMPQUIET(finfop, 0);
	COMPQUIET(mpf, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_fopen"));
}

int
__dbcl_memp_fput(mpf, pgaddr, flags)
	DB_MPOOLFILE * mpf;
	void * pgaddr;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = mpf->dbmp->dbenv;
	COMPQUIET(pgaddr, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_fput"));
}

int
__dbcl_memp_fset(mpf, pgaddr, flags)
	DB_MPOOLFILE * mpf;
	void * pgaddr;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = mpf->dbmp->dbenv;
	COMPQUIET(pgaddr, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_fset"));
}

int
__dbcl_memp_fsync(mpf)
	DB_MPOOLFILE * mpf;
{
	DB_ENV *dbenv;

	dbenv = mpf->dbmp->dbenv;
	return (__dbcl_rpc_illegal(dbenv, "memp_fsync"));
}

int
__dbcl_memp_register(dbenv, ftype, func0, func1)
	DB_ENV * dbenv;
	int ftype;
	int (*func0) __P((DB_ENV *, db_pgno_t, void *, DBT *));
	int (*func1) __P((DB_ENV *, db_pgno_t, void *, DBT *));
{
	COMPQUIET(ftype, 0);
	COMPQUIET(func0, 0);
	COMPQUIET(func1, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_register"));
}

int
__dbcl_memp_stat(dbenv, gstatp, fstatp, func0)
	DB_ENV * dbenv;
	DB_MPOOL_STAT ** gstatp;
	DB_MPOOL_FSTAT *** fstatp;
	void *(*func0) __P((size_t));
{
	COMPQUIET(gstatp, 0);
	COMPQUIET(fstatp, 0);
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_stat"));
}

int
__dbcl_memp_sync(dbenv, lsn)
	DB_ENV * dbenv;
	DB_LSN * lsn;
{
	COMPQUIET(lsn, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_sync"));
}

int
__dbcl_memp_trickle(dbenv, pct, nwrotep)
	DB_ENV * dbenv;
	int pct;
	int * nwrotep;
{
	COMPQUIET(pct, 0);
	COMPQUIET(nwrotep, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_trickle"));
}

#endif /* HAVE_RPC */
