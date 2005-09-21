/* Do not edit: automatically built by gen_rpc.awk. */
#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>

#include <string.h>
#endif

#include "db_server.h"

#include "db_int.h"
#include "dbinc/txn.h"
#include "dbinc_auto/rpc_client_ext.h"

static int __dbcl_noserver __P((DB_ENV *));

static int
__dbcl_noserver(dbenv)
	DB_ENV *dbenv;
{
	__db_err(dbenv, "No server environment");
	return (DB_NOSERVER);
}

static int __dbcl_rpc_illegal __P((DB_ENV *, char *));

static int
__dbcl_rpc_illegal(dbenv, name)
	DB_ENV *dbenv;
	char *name;
{
	__db_err(dbenv, "%s method unsupported in RPC environments", name);
	return (DB_OPNOTSUP);
}

/*
 * PUBLIC: int __dbcl_env_alloc __P((DB_ENV *, void *(*)(size_t),
 * PUBLIC:      void *(*)(void *, size_t), void (*)(void *)));
 */
int
__dbcl_env_alloc(dbenv, func0, func1, func2)
	DB_ENV * dbenv;
	void *(*func0) __P((size_t));
	void *(*func1) __P((void *, size_t));
	void (*func2) __P((void *));
{
	COMPQUIET(func0, 0);
	COMPQUIET(func1, 0);
	COMPQUIET(func2, 0);
	return (__dbcl_rpc_illegal(dbenv, "env_alloc"));
}

/*
 * PUBLIC: int __dbcl_set_app_dispatch __P((DB_ENV *, int (*)(DB_ENV *, DBT *,
 * PUBLIC:      DB_LSN *, db_recops)));
 */
int
__dbcl_set_app_dispatch(dbenv, func0)
	DB_ENV * dbenv;
	int (*func0) __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
{
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_app_dispatch"));
}

/*
 * PUBLIC: int __dbcl_env_get_cachesize __P((DB_ENV *, u_int32_t *,
 * PUBLIC:      u_int32_t *, int *));
 */
int
__dbcl_env_get_cachesize(dbenv, gbytesp, bytesp, ncachep)
	DB_ENV * dbenv;
	u_int32_t * gbytesp;
	u_int32_t * bytesp;
	int * ncachep;
{
	CLIENT *cl;
	__env_get_cachesize_msg msg;
	__env_get_cachesize_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_cachesize_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (gbytesp != NULL)
		*gbytesp = replyp->gbytes;
	if (bytesp != NULL)
		*bytesp = replyp->bytes;
	if (ncachep != NULL)
		*ncachep = replyp->ncache;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_cachesize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_cachesize __P((DB_ENV *, u_int32_t, u_int32_t, int));
 */
int
__dbcl_env_cachesize(dbenv, gbytes, bytes, ncache)
	DB_ENV * dbenv;
	u_int32_t gbytes;
	u_int32_t bytes;
	int ncache;
{
	CLIENT *cl;
	__env_cachesize_msg msg;
	__env_cachesize_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	msg.gbytes = gbytes;
	msg.bytes = bytes;
	msg.ncache = ncache;

	replyp = __db_env_cachesize_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_cachesize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_close __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_env_close(dbenv, flags)
	DB_ENV * dbenv;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_close_msg msg;
	__env_close_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	msg.flags = flags;

	replyp = __db_env_close_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_close_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_create __P((DB_ENV *, long));
 */
int
__dbcl_env_create(dbenv, timeout)
	DB_ENV * dbenv;
	long timeout;
{
	CLIENT *cl;
	__env_create_msg msg;
	__env_create_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.timeout = timeout;

	replyp = __db_env_create_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_create_ret(dbenv, timeout, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_create_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_get_data_dirs __P((DB_ENV *, const char ***));
 */
int
__dbcl_get_data_dirs(dbenv, dirpp)
	DB_ENV * dbenv;
	const char *** dirpp;
{
	COMPQUIET(dirpp, NULL);
	return (__dbcl_rpc_illegal(dbenv, "get_data_dirs"));
}

/*
 * PUBLIC: int __dbcl_set_data_dir __P((DB_ENV *, const char *));
 */
int
__dbcl_set_data_dir(dbenv, dir)
	DB_ENV * dbenv;
	const char * dir;
{
	COMPQUIET(dir, NULL);
	return (__dbcl_rpc_illegal(dbenv, "set_data_dir"));
}

/*
 * PUBLIC: int __dbcl_env_dbremove __P((DB_ENV *, DB_TXN *, const char *,
 * PUBLIC:      const char *, u_int32_t));
 */
int
__dbcl_env_dbremove(dbenv, txnp, name, subdb, flags)
	DB_ENV * dbenv;
	DB_TXN * txnp;
	const char * name;
	const char * subdb;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_dbremove_msg msg;
	__env_dbremove_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	msg.flags = flags;

	replyp = __db_env_dbremove_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_dbremove_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_dbrename __P((DB_ENV *, DB_TXN *, const char *,
 * PUBLIC:      const char *, const char *, u_int32_t));
 */
int
__dbcl_env_dbrename(dbenv, txnp, name, subdb, newname, flags)
	DB_ENV * dbenv;
	DB_TXN * txnp;
	const char * name;
	const char * subdb;
	const char * newname;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_dbrename_msg msg;
	__env_dbrename_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	if (newname == NULL)
		msg.newname = "";
	else
		msg.newname = (char *)newname;
	msg.flags = flags;

	replyp = __db_env_dbrename_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_dbrename_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_get_encrypt_flags __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_env_get_encrypt_flags(dbenv, flagsp)
	DB_ENV * dbenv;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__env_get_encrypt_flags_msg msg;
	__env_get_encrypt_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_encrypt_flags_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_encrypt_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_encrypt __P((DB_ENV *, const char *, u_int32_t));
 */
int
__dbcl_env_encrypt(dbenv, passwd, flags)
	DB_ENV * dbenv;
	const char * passwd;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_encrypt_msg msg;
	__env_encrypt_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	if (passwd == NULL)
		msg.passwd = "";
	else
		msg.passwd = (char *)passwd;
	msg.flags = flags;

	replyp = __db_env_encrypt_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_encrypt_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_set_feedback __P((DB_ENV *, void (*)(DB_ENV *, int,
 * PUBLIC:      int)));
 */
int
__dbcl_env_set_feedback(dbenv, func0)
	DB_ENV * dbenv;
	void (*func0) __P((DB_ENV *, int, int));
{
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "env_set_feedback"));
}

/*
 * PUBLIC: int __dbcl_env_get_flags __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_env_get_flags(dbenv, flagsp)
	DB_ENV * dbenv;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__env_get_flags_msg msg;
	__env_get_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_flags_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_flags __P((DB_ENV *, u_int32_t, int));
 */
int
__dbcl_env_flags(dbenv, flags, onoff)
	DB_ENV * dbenv;
	u_int32_t flags;
	int onoff;
{
	CLIENT *cl;
	__env_flags_msg msg;
	__env_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	msg.flags = flags;
	msg.onoff = onoff;

	replyp = __db_env_flags_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_get_lg_bsize __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_lg_bsize(dbenv, bsizep)
	DB_ENV * dbenv;
	u_int32_t * bsizep;
{
	COMPQUIET(bsizep, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lg_bsize"));
}

/*
 * PUBLIC: int __dbcl_set_lg_bsize __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_lg_bsize(dbenv, bsize)
	DB_ENV * dbenv;
	u_int32_t bsize;
{
	COMPQUIET(bsize, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lg_bsize"));
}

/*
 * PUBLIC: int __dbcl_get_lg_dir __P((DB_ENV *, const char * *));
 */
int
__dbcl_get_lg_dir(dbenv, dirp)
	DB_ENV * dbenv;
	const char * * dirp;
{
	COMPQUIET(dirp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lg_dir"));
}

/*
 * PUBLIC: int __dbcl_set_lg_dir __P((DB_ENV *, const char *));
 */
int
__dbcl_set_lg_dir(dbenv, dir)
	DB_ENV * dbenv;
	const char * dir;
{
	COMPQUIET(dir, NULL);
	return (__dbcl_rpc_illegal(dbenv, "set_lg_dir"));
}

/*
 * PUBLIC: int __dbcl_get_lg_max __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_lg_max(dbenv, maxp)
	DB_ENV * dbenv;
	u_int32_t * maxp;
{
	COMPQUIET(maxp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lg_max"));
}

/*
 * PUBLIC: int __dbcl_set_lg_max __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_lg_max(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lg_max"));
}

/*
 * PUBLIC: int __dbcl_get_lg_regionmax __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_lg_regionmax(dbenv, maxp)
	DB_ENV * dbenv;
	u_int32_t * maxp;
{
	COMPQUIET(maxp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lg_regionmax"));
}

/*
 * PUBLIC: int __dbcl_set_lg_regionmax __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_lg_regionmax(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lg_regionmax"));
}

/*
 * PUBLIC: int __dbcl_get_lk_conflicts __P((DB_ENV *, const u_int8_t **,
 * PUBLIC:      int *));
 */
int
__dbcl_get_lk_conflicts(dbenv, conflicts, modes)
	DB_ENV * dbenv;
	const u_int8_t ** conflicts;
	int * modes;
{
	COMPQUIET(conflicts, 0);
	COMPQUIET(modes, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lk_conflicts"));
}

/*
 * PUBLIC: int __dbcl_set_lk_conflict __P((DB_ENV *, u_int8_t *, int));
 */
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

/*
 * PUBLIC: int __dbcl_get_lk_detect __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_lk_detect(dbenv, detectp)
	DB_ENV * dbenv;
	u_int32_t * detectp;
{
	COMPQUIET(detectp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lk_detect"));
}

/*
 * PUBLIC: int __dbcl_set_lk_detect __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_lk_detect(dbenv, detect)
	DB_ENV * dbenv;
	u_int32_t detect;
{
	COMPQUIET(detect, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_detect"));
}

/*
 * PUBLIC: int __dbcl_set_lk_max __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_lk_max(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_max"));
}

/*
 * PUBLIC: int __dbcl_get_lk_max_locks __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_lk_max_locks(dbenv, maxp)
	DB_ENV * dbenv;
	u_int32_t * maxp;
{
	COMPQUIET(maxp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lk_max_locks"));
}

/*
 * PUBLIC: int __dbcl_set_lk_max_locks __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_lk_max_locks(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_max_locks"));
}

/*
 * PUBLIC: int __dbcl_get_lk_max_lockers __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_lk_max_lockers(dbenv, maxp)
	DB_ENV * dbenv;
	u_int32_t * maxp;
{
	COMPQUIET(maxp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lk_max_lockers"));
}

/*
 * PUBLIC: int __dbcl_set_lk_max_lockers __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_lk_max_lockers(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_max_lockers"));
}

/*
 * PUBLIC: int __dbcl_get_lk_max_objects __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_lk_max_objects(dbenv, maxp)
	DB_ENV * dbenv;
	u_int32_t * maxp;
{
	COMPQUIET(maxp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_lk_max_objects"));
}

/*
 * PUBLIC: int __dbcl_set_lk_max_objects __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_lk_max_objects(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_lk_max_objects"));
}

/*
 * PUBLIC: int __dbcl_get_mp_max_openfd __P((DB_ENV *, int *));
 */
int
__dbcl_get_mp_max_openfd(dbenv, nopenp)
	DB_ENV * dbenv;
	int * nopenp;
{
	COMPQUIET(nopenp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_mp_max_openfd"));
}

/*
 * PUBLIC: int __dbcl_set_mp_max_openfd __P((DB_ENV *, int));
 */
int
__dbcl_set_mp_max_openfd(dbenv, nopen)
	DB_ENV * dbenv;
	int nopen;
{
	COMPQUIET(nopen, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_mp_max_openfd"));
}

/*
 * PUBLIC: int __dbcl_get_mp_max_write __P((DB_ENV *, int *, int *));
 */
int
__dbcl_get_mp_max_write(dbenv, nwritep, nsleepp)
	DB_ENV * dbenv;
	int * nwritep;
	int * nsleepp;
{
	COMPQUIET(nwritep, 0);
	COMPQUIET(nsleepp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_mp_max_write"));
}

/*
 * PUBLIC: int __dbcl_set_mp_max_write __P((DB_ENV *, int, int));
 */
int
__dbcl_set_mp_max_write(dbenv, nwrite, nsleep)
	DB_ENV * dbenv;
	int nwrite;
	int nsleep;
{
	COMPQUIET(nwrite, 0);
	COMPQUIET(nsleep, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_mp_max_write"));
}

/*
 * PUBLIC: int __dbcl_get_mp_mmapsize __P((DB_ENV *, size_t *));
 */
int
__dbcl_get_mp_mmapsize(dbenv, mmapsizep)
	DB_ENV * dbenv;
	size_t * mmapsizep;
{
	COMPQUIET(mmapsizep, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_mp_mmapsize"));
}

/*
 * PUBLIC: int __dbcl_set_mp_mmapsize __P((DB_ENV *, size_t));
 */
int
__dbcl_set_mp_mmapsize(dbenv, mmapsize)
	DB_ENV * dbenv;
	size_t mmapsize;
{
	COMPQUIET(mmapsize, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_mp_mmapsize"));
}

/*
 * PUBLIC: int __dbcl_env_get_home __P((DB_ENV *, const char * *));
 */
int
__dbcl_env_get_home(dbenv, homep)
	DB_ENV * dbenv;
	const char * * homep;
{
	CLIENT *cl;
	__env_get_home_msg msg;
	__env_get_home_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_home_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (homep != NULL)
		*homep = replyp->home;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_home_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_get_open_flags __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_env_get_open_flags(dbenv, flagsp)
	DB_ENV * dbenv;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__env_get_open_flags_msg msg;
	__env_get_open_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_open_flags_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_open_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_open __P((DB_ENV *, const char *, u_int32_t, int));
 */
int
__dbcl_env_open(dbenv, home, flags, mode)
	DB_ENV * dbenv;
	const char * home;
	u_int32_t flags;
	int mode;
{
	CLIENT *cl;
	__env_open_msg msg;
	__env_open_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	if (home == NULL)
		msg.home = "";
	else
		msg.home = (char *)home;
	msg.flags = flags;
	msg.mode = mode;

	replyp = __db_env_open_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_open_ret(dbenv, home, flags, mode, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_open_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_paniccall __P((DB_ENV *, void (*)(DB_ENV *, int)));
 */
int
__dbcl_env_paniccall(dbenv, func0)
	DB_ENV * dbenv;
	void (*func0) __P((DB_ENV *, int));
{
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "env_paniccall"));
}

/*
 * PUBLIC: int __dbcl_env_remove __P((DB_ENV *, const char *, u_int32_t));
 */
int
__dbcl_env_remove(dbenv, home, flags)
	DB_ENV * dbenv;
	const char * home;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_remove_msg msg;
	__env_remove_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	if (home == NULL)
		msg.home = "";
	else
		msg.home = (char *)home;
	msg.flags = flags;

	replyp = __db_env_remove_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_remove_ret(dbenv, home, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_remove_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_get_shm_key __P((DB_ENV *, long *));
 */
int
__dbcl_get_shm_key(dbenv, shm_keyp)
	DB_ENV * dbenv;
	long * shm_keyp;
{
	COMPQUIET(shm_keyp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_shm_key"));
}

/*
 * PUBLIC: int __dbcl_set_shm_key __P((DB_ENV *, long));
 */
int
__dbcl_set_shm_key(dbenv, shm_key)
	DB_ENV * dbenv;
	long shm_key;
{
	COMPQUIET(shm_key, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_shm_key"));
}

/*
 * PUBLIC: int __dbcl_get_tas_spins __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_tas_spins(dbenv, tas_spinsp)
	DB_ENV * dbenv;
	u_int32_t * tas_spinsp;
{
	COMPQUIET(tas_spinsp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_tas_spins"));
}

/*
 * PUBLIC: int __dbcl_set_tas_spins __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_tas_spins(dbenv, tas_spins)
	DB_ENV * dbenv;
	u_int32_t tas_spins;
{
	COMPQUIET(tas_spins, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_tas_spins"));
}

/*
 * PUBLIC: int __dbcl_get_timeout __P((DB_ENV *, u_int32_t *, u_int32_t));
 */
int
__dbcl_get_timeout(dbenv, timeoutp, flags)
	DB_ENV * dbenv;
	u_int32_t * timeoutp;
	u_int32_t flags;
{
	COMPQUIET(timeoutp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_timeout"));
}

/*
 * PUBLIC: int __dbcl_set_timeout __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__dbcl_set_timeout(dbenv, timeout, flags)
	DB_ENV * dbenv;
	u_int32_t timeout;
	u_int32_t flags;
{
	COMPQUIET(timeout, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_timeout"));
}

/*
 * PUBLIC: int __dbcl_get_tmp_dir __P((DB_ENV *, const char * *));
 */
int
__dbcl_get_tmp_dir(dbenv, dirp)
	DB_ENV * dbenv;
	const char * * dirp;
{
	COMPQUIET(dirp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_tmp_dir"));
}

/*
 * PUBLIC: int __dbcl_set_tmp_dir __P((DB_ENV *, const char *));
 */
int
__dbcl_set_tmp_dir(dbenv, dir)
	DB_ENV * dbenv;
	const char * dir;
{
	COMPQUIET(dir, NULL);
	return (__dbcl_rpc_illegal(dbenv, "set_tmp_dir"));
}

/*
 * PUBLIC: int __dbcl_get_tx_max __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_get_tx_max(dbenv, maxp)
	DB_ENV * dbenv;
	u_int32_t * maxp;
{
	COMPQUIET(maxp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_tx_max"));
}

/*
 * PUBLIC: int __dbcl_set_tx_max __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_set_tx_max(dbenv, max)
	DB_ENV * dbenv;
	u_int32_t max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_tx_max"));
}

/*
 * PUBLIC: int __dbcl_get_tx_timestamp __P((DB_ENV *, time_t *));
 */
int
__dbcl_get_tx_timestamp(dbenv, maxp)
	DB_ENV * dbenv;
	time_t * maxp;
{
	COMPQUIET(maxp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_tx_timestamp"));
}

/*
 * PUBLIC: int __dbcl_set_tx_timestamp __P((DB_ENV *, time_t *));
 */
int
__dbcl_set_tx_timestamp(dbenv, max)
	DB_ENV * dbenv;
	time_t * max;
{
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "set_tx_timestamp"));
}

/*
 * PUBLIC: int __dbcl_get_verbose __P((DB_ENV *, u_int32_t, int *));
 */
int
__dbcl_get_verbose(dbenv, which, onoffp)
	DB_ENV * dbenv;
	u_int32_t which;
	int * onoffp;
{
	COMPQUIET(which, 0);
	COMPQUIET(onoffp, 0);
	return (__dbcl_rpc_illegal(dbenv, "get_verbose"));
}

/*
 * PUBLIC: int __dbcl_set_verbose __P((DB_ENV *, u_int32_t, int));
 */
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

/*
 * PUBLIC: int __dbcl_txn_abort __P((DB_TXN *));
 */
int
__dbcl_txn_abort(txnp)
	DB_TXN * txnp;
{
	CLIENT *cl;
	__txn_abort_msg msg;
	__txn_abort_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;

	replyp = __db_txn_abort_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_txn_abort_ret(txnp, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_abort_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_begin __P((DB_ENV *, DB_TXN *, DB_TXN **,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_txn_begin(dbenv, parent, txnpp, flags)
	DB_ENV * dbenv;
	DB_TXN * parent;
	DB_TXN ** txnpp;
	u_int32_t flags;
{
	CLIENT *cl;
	__txn_begin_msg msg;
	__txn_begin_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	if (parent == NULL)
		msg.parentcl_id = 0;
	else
		msg.parentcl_id = parent->txnid;
	msg.flags = flags;

	replyp = __db_txn_begin_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_txn_begin_ret(dbenv, parent, txnpp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_begin_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_checkpoint __P((DB_ENV *, u_int32_t, u_int32_t,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_txn_checkpoint(dbenv, kbyte, min, flags)
	DB_ENV * dbenv;
	u_int32_t kbyte;
	u_int32_t min;
	u_int32_t flags;
{
	COMPQUIET(kbyte, 0);
	COMPQUIET(min, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "txn_checkpoint"));
}

/*
 * PUBLIC: int __dbcl_txn_commit __P((DB_TXN *, u_int32_t));
 */
int
__dbcl_txn_commit(txnp, flags)
	DB_TXN * txnp;
	u_int32_t flags;
{
	CLIENT *cl;
	__txn_commit_msg msg;
	__txn_commit_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = flags;

	replyp = __db_txn_commit_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_txn_commit_ret(txnp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_commit_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_discard __P((DB_TXN *, u_int32_t));
 */
int
__dbcl_txn_discard(txnp, flags)
	DB_TXN * txnp;
	u_int32_t flags;
{
	CLIENT *cl;
	__txn_discard_msg msg;
	__txn_discard_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = flags;

	replyp = __db_txn_discard_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_txn_discard_ret(txnp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_discard_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_prepare __P((DB_TXN *, u_int8_t *));
 */
int
__dbcl_txn_prepare(txnp, gid)
	DB_TXN * txnp;
	u_int8_t * gid;
{
	CLIENT *cl;
	__txn_prepare_msg msg;
	__txn_prepare_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	memcpy(msg.gid, gid, 128);

	replyp = __db_txn_prepare_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_prepare_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_recover __P((DB_ENV *, DB_PREPLIST *, long, long *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_txn_recover(dbenv, preplist, count, retp, flags)
	DB_ENV * dbenv;
	DB_PREPLIST * preplist;
	long count;
	long * retp;
	u_int32_t flags;
{
	CLIENT *cl;
	__txn_recover_msg msg;
	__txn_recover_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	msg.count = count;
	msg.flags = flags;

	replyp = __db_txn_recover_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_txn_recover_ret(dbenv, preplist, count, retp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_recover_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_stat __P((DB_ENV *, DB_TXN_STAT **, u_int32_t));
 */
int
__dbcl_txn_stat(dbenv, statp, flags)
	DB_ENV * dbenv;
	DB_TXN_STAT ** statp;
	u_int32_t flags;
{
	COMPQUIET(statp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "txn_stat"));
}

/*
 * PUBLIC: int __dbcl_txn_timeout __P((DB_TXN *, u_int32_t, u_int32_t));
 */
int
__dbcl_txn_timeout(txnp, timeout, flags)
	DB_TXN * txnp;
	u_int32_t timeout;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = txnp->mgrp->dbenv;
	COMPQUIET(timeout, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "txn_timeout"));
}

/*
 * PUBLIC: int __dbcl_rep_elect __P((DB_ENV *, int, int, int, u_int32_t, int *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_rep_elect(dbenv, nsites, nvotes, pri, timeout, idp, flags)
	DB_ENV * dbenv;
	int nsites;
	int nvotes;
	int pri;
	u_int32_t timeout;
	int * idp;
	u_int32_t flags;
{
	COMPQUIET(nsites, 0);
	COMPQUIET(nvotes, 0);
	COMPQUIET(pri, 0);
	COMPQUIET(timeout, 0);
	COMPQUIET(idp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "rep_elect"));
}

/*
 * PUBLIC: int __dbcl_rep_flush __P((DB_ENV *));
 */
int
__dbcl_rep_flush(dbenv)
	DB_ENV * dbenv;
{
	return (__dbcl_rpc_illegal(dbenv, "rep_flush"));
}

/*
 * PUBLIC: int __dbcl_rep_process_message __P((DB_ENV *, DBT *, DBT *, int *,
 * PUBLIC:      DB_LSN *));
 */
int
__dbcl_rep_process_message(dbenv, rec, control, idp, ret_lsnp)
	DB_ENV * dbenv;
	DBT * rec;
	DBT * control;
	int * idp;
	DB_LSN * ret_lsnp;
{
	COMPQUIET(rec, NULL);
	COMPQUIET(control, NULL);
	COMPQUIET(idp, 0);
	COMPQUIET(ret_lsnp, 0);
	return (__dbcl_rpc_illegal(dbenv, "rep_process_message"));
}

/*
 * PUBLIC: int __dbcl_rep_get_limit __P((DB_ENV *, u_int32_t *, u_int32_t *));
 */
int
__dbcl_rep_get_limit(dbenv, mbytesp, bytesp)
	DB_ENV * dbenv;
	u_int32_t * mbytesp;
	u_int32_t * bytesp;
{
	COMPQUIET(mbytesp, 0);
	COMPQUIET(bytesp, 0);
	return (__dbcl_rpc_illegal(dbenv, "rep_get_limit"));
}

/*
 * PUBLIC: int __dbcl_rep_set_limit __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__dbcl_rep_set_limit(dbenv, mbytes, bytes)
	DB_ENV * dbenv;
	u_int32_t mbytes;
	u_int32_t bytes;
{
	COMPQUIET(mbytes, 0);
	COMPQUIET(bytes, 0);
	return (__dbcl_rpc_illegal(dbenv, "rep_set_limit"));
}

/*
 * PUBLIC: int __dbcl_rep_set_request __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__dbcl_rep_set_request(dbenv, min, max)
	DB_ENV * dbenv;
	u_int32_t min;
	u_int32_t max;
{
	COMPQUIET(min, 0);
	COMPQUIET(max, 0);
	return (__dbcl_rpc_illegal(dbenv, "rep_set_request"));
}

/*
 * PUBLIC: int __dbcl_rep_set_rep_transport __P((DB_ENV *, int,
 * PUBLIC:      int (*)(DB_ENV *, const DBT *, const DBT *, const DB_LSN *, int, u_int32_t)));
 */
int
__dbcl_rep_set_rep_transport(dbenv, id, func0)
	DB_ENV * dbenv;
	int id;
	int (*func0) __P((DB_ENV *, const DBT *, const DBT *, const DB_LSN *, int, u_int32_t));
{
	COMPQUIET(id, 0);
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "rep_set_rep_transport"));
}

/*
 * PUBLIC: int __dbcl_rep_start __P((DB_ENV *, DBT *, u_int32_t));
 */
int
__dbcl_rep_start(dbenv, cdata, flags)
	DB_ENV * dbenv;
	DBT * cdata;
	u_int32_t flags;
{
	COMPQUIET(cdata, NULL);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "rep_start"));
}

/*
 * PUBLIC: int __dbcl_rep_stat __P((DB_ENV *, DB_REP_STAT **, u_int32_t));
 */
int
__dbcl_rep_stat(dbenv, statp, flags)
	DB_ENV * dbenv;
	DB_REP_STAT ** statp;
	u_int32_t flags;
{
	COMPQUIET(statp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "rep_stat"));
}

/*
 * PUBLIC: int __dbcl_db_alloc __P((DB *, void *(*)(size_t), void *(*)(void *,
 * PUBLIC:      size_t), void (*)(void *)));
 */
int
__dbcl_db_alloc(dbp, func0, func1, func2)
	DB * dbp;
	void *(*func0) __P((size_t));
	void *(*func1) __P((void *, size_t));
	void (*func2) __P((void *));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	COMPQUIET(func1, 0);
	COMPQUIET(func2, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_alloc"));
}

/*
 * PUBLIC: int __dbcl_db_associate __P((DB *, DB_TXN *, DB *, int (*)(DB *,
 * PUBLIC:      const DBT *, const DBT *, DBT *), u_int32_t));
 */
int
__dbcl_db_associate(dbp, txnp, sdbp, func0, flags)
	DB * dbp;
	DB_TXN * txnp;
	DB * sdbp;
	int (*func0) __P((DB *, const DBT *, const DBT *, DBT *));
	u_int32_t flags;
{
	CLIENT *cl;
	__db_associate_msg msg;
	__db_associate_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (func0 != NULL) {
		__db_err(dbenv, "User functions not supported in RPC");
		return (EINVAL);
	}
	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	if (sdbp == NULL)
		msg.sdbpcl_id = 0;
	else
		msg.sdbpcl_id = sdbp->cl_id;
	msg.flags = flags;

	replyp = __db_db_associate_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_associate_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_bt_compare __P((DB *, int (*)(DB *, const DBT *,
 * PUBLIC:      const DBT *)));
 */
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

/*
 * PUBLIC: int __dbcl_db_bt_maxkey __P((DB *, u_int32_t));
 */
int
__dbcl_db_bt_maxkey(dbp, maxkey)
	DB * dbp;
	u_int32_t maxkey;
{
	CLIENT *cl;
	__db_bt_maxkey_msg msg;
	__db_bt_maxkey_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.maxkey = maxkey;

	replyp = __db_db_bt_maxkey_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_bt_maxkey_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_bt_minkey __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_bt_minkey(dbp, minkeyp)
	DB * dbp;
	u_int32_t * minkeyp;
{
	CLIENT *cl;
	__db_get_bt_minkey_msg msg;
	__db_get_bt_minkey_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_bt_minkey_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (minkeyp != NULL)
		*minkeyp = replyp->minkey;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_bt_minkey_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_bt_minkey __P((DB *, u_int32_t));
 */
int
__dbcl_db_bt_minkey(dbp, minkey)
	DB * dbp;
	u_int32_t minkey;
{
	CLIENT *cl;
	__db_bt_minkey_msg msg;
	__db_bt_minkey_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.minkey = minkey;

	replyp = __db_db_bt_minkey_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_bt_minkey_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_bt_prefix __P((DB *, size_t(*)(DB *, const DBT *,
 * PUBLIC:      const DBT *)));
 */
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

/*
 * PUBLIC: int __dbcl_db_set_append_recno __P((DB *, int (*)(DB *, DBT *,
 * PUBLIC:      db_recno_t)));
 */
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

/*
 * PUBLIC: int __dbcl_db_get_cachesize __P((DB *, u_int32_t *, u_int32_t *,
 * PUBLIC:      int *));
 */
int
__dbcl_db_get_cachesize(dbp, gbytesp, bytesp, ncachep)
	DB * dbp;
	u_int32_t * gbytesp;
	u_int32_t * bytesp;
	int * ncachep;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(gbytesp, 0);
	COMPQUIET(bytesp, 0);
	COMPQUIET(ncachep, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_get_cachesize"));
}

/*
 * PUBLIC: int __dbcl_db_cachesize __P((DB *, u_int32_t, u_int32_t, int));
 */
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

/*
 * PUBLIC: int __dbcl_db_close __P((DB *, u_int32_t));
 */
int
__dbcl_db_close(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_close_msg msg;
	__db_close_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.flags = flags;

	replyp = __db_db_close_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_close_ret(dbp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_close_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_create __P((DB *, DB_ENV *, u_int32_t));
 */
int
__dbcl_db_create(dbp, dbenv, flags)
	DB * dbp;
	DB_ENV * dbenv;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_create_msg msg;
	__db_create_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbenv == NULL)
		msg.dbenvcl_id = 0;
	else
		msg.dbenvcl_id = dbenv->cl_id;
	msg.flags = flags;

	replyp = __db_db_create_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_create_ret(dbp, dbenv, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_create_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_del __P((DB *, DB_TXN *, DBT *, u_int32_t));
 */
int
__dbcl_db_del(dbp, txnp, key, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_del_msg msg;
	__db_del_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.flags = flags;

	replyp = __db_db_del_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_del_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_dup_compare __P((DB *, int (*)(DB *, const DBT *,
 * PUBLIC:      const DBT *)));
 */
int
__dbcl_db_dup_compare(dbp, func0)
	DB * dbp;
	int (*func0) __P((DB *, const DBT *, const DBT *));
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(func0, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_dup_compare"));
}

/*
 * PUBLIC: int __dbcl_db_get_encrypt_flags __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_encrypt_flags(dbp, flagsp)
	DB * dbp;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__db_get_encrypt_flags_msg msg;
	__db_get_encrypt_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_encrypt_flags_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_encrypt_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_encrypt __P((DB *, const char *, u_int32_t));
 */
int
__dbcl_db_encrypt(dbp, passwd, flags)
	DB * dbp;
	const char * passwd;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_encrypt_msg msg;
	__db_encrypt_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (passwd == NULL)
		msg.passwd = "";
	else
		msg.passwd = (char *)passwd;
	msg.flags = flags;

	replyp = __db_db_encrypt_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_encrypt_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_extentsize __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_extentsize(dbp, extentsizep)
	DB * dbp;
	u_int32_t * extentsizep;
{
	CLIENT *cl;
	__db_get_extentsize_msg msg;
	__db_get_extentsize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_extentsize_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (extentsizep != NULL)
		*extentsizep = replyp->extentsize;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_extentsize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_extentsize __P((DB *, u_int32_t));
 */
int
__dbcl_db_extentsize(dbp, extentsize)
	DB * dbp;
	u_int32_t extentsize;
{
	CLIENT *cl;
	__db_extentsize_msg msg;
	__db_extentsize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.extentsize = extentsize;

	replyp = __db_db_extentsize_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_extentsize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_fd __P((DB *, int *));
 */
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

/*
 * PUBLIC: int __dbcl_db_feedback __P((DB *, void (*)(DB *, int, int)));
 */
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

/*
 * PUBLIC: int __dbcl_db_get_flags __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_flags(dbp, flagsp)
	DB * dbp;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__db_get_flags_msg msg;
	__db_get_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_flags_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_flags __P((DB *, u_int32_t));
 */
int
__dbcl_db_flags(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_flags_msg msg;
	__db_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.flags = flags;

	replyp = __db_db_flags_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_db_get(dbp, txnp, key, data, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_get_msg msg;
	__db_get_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = flags;

	replyp = __db_db_get_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_get_ret(dbp, txnp, key, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_name __P((DB *, const char * *, const char * *));
 */
int
__dbcl_db_get_name(dbp, filenamep, dbnamep)
	DB * dbp;
	const char * * filenamep;
	const char * * dbnamep;
{
	CLIENT *cl;
	__db_get_name_msg msg;
	__db_get_name_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_name_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (filenamep != NULL)
		*filenamep = replyp->filename;
	if (dbnamep != NULL)
		*dbnamep = replyp->dbname;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_name_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_open_flags __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_open_flags(dbp, flagsp)
	DB * dbp;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__db_get_open_flags_msg msg;
	__db_get_open_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_open_flags_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_open_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_h_ffactor __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_h_ffactor(dbp, ffactorp)
	DB * dbp;
	u_int32_t * ffactorp;
{
	CLIENT *cl;
	__db_get_h_ffactor_msg msg;
	__db_get_h_ffactor_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_h_ffactor_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (ffactorp != NULL)
		*ffactorp = replyp->ffactor;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_h_ffactor_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_h_ffactor __P((DB *, u_int32_t));
 */
int
__dbcl_db_h_ffactor(dbp, ffactor)
	DB * dbp;
	u_int32_t ffactor;
{
	CLIENT *cl;
	__db_h_ffactor_msg msg;
	__db_h_ffactor_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.ffactor = ffactor;

	replyp = __db_db_h_ffactor_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_h_ffactor_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_h_hash __P((DB *, u_int32_t(*)(DB *, const void *,
 * PUBLIC:      u_int32_t)));
 */
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

/*
 * PUBLIC: int __dbcl_db_get_h_nelem __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_h_nelem(dbp, nelemp)
	DB * dbp;
	u_int32_t * nelemp;
{
	CLIENT *cl;
	__db_get_h_nelem_msg msg;
	__db_get_h_nelem_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_h_nelem_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (nelemp != NULL)
		*nelemp = replyp->nelem;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_h_nelem_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_h_nelem __P((DB *, u_int32_t));
 */
int
__dbcl_db_h_nelem(dbp, nelem)
	DB * dbp;
	u_int32_t nelem;
{
	CLIENT *cl;
	__db_h_nelem_msg msg;
	__db_h_nelem_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.nelem = nelem;

	replyp = __db_db_h_nelem_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_h_nelem_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_key_range __P((DB *, DB_TXN *, DBT *, DB_KEY_RANGE *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_key_range(dbp, txnp, key, range, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DB_KEY_RANGE * range;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_key_range_msg msg;
	__db_key_range_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.flags = flags;

	replyp = __db_db_key_range_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_key_range_ret(dbp, txnp, key, range, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_key_range_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_lorder __P((DB *, int *));
 */
int
__dbcl_db_get_lorder(dbp, lorderp)
	DB * dbp;
	int * lorderp;
{
	CLIENT *cl;
	__db_get_lorder_msg msg;
	__db_get_lorder_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_lorder_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (lorderp != NULL)
		*lorderp = replyp->lorder;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_lorder_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_lorder __P((DB *, int));
 */
int
__dbcl_db_lorder(dbp, lorder)
	DB * dbp;
	int lorder;
{
	CLIENT *cl;
	__db_lorder_msg msg;
	__db_lorder_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.lorder = lorder;

	replyp = __db_db_lorder_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_lorder_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_open __P((DB *, DB_TXN *, const char *, const char *,
 * PUBLIC:      DBTYPE, u_int32_t, int));
 */
int
__dbcl_db_open(dbp, txnp, name, subdb, type, flags, mode)
	DB * dbp;
	DB_TXN * txnp;
	const char * name;
	const char * subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	CLIENT *cl;
	__db_open_msg msg;
	__db_open_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	msg.type = type;
	msg.flags = flags;
	msg.mode = mode;

	replyp = __db_db_open_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_open_ret(dbp, txnp, name, subdb, type, flags, mode, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_open_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_pagesize __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_pagesize(dbp, pagesizep)
	DB * dbp;
	u_int32_t * pagesizep;
{
	CLIENT *cl;
	__db_get_pagesize_msg msg;
	__db_get_pagesize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_pagesize_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (pagesizep != NULL)
		*pagesizep = replyp->pagesize;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_pagesize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_pagesize __P((DB *, u_int32_t));
 */
int
__dbcl_db_pagesize(dbp, pagesize)
	DB * dbp;
	u_int32_t pagesize;
{
	CLIENT *cl;
	__db_pagesize_msg msg;
	__db_pagesize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.pagesize = pagesize;

	replyp = __db_db_pagesize_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_pagesize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_panic __P((DB *, void (*)(DB_ENV *, int)));
 */
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

/*
 * PUBLIC: int __dbcl_db_pget __P((DB *, DB_TXN *, DBT *, DBT *, DBT *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_pget(dbp, txnp, skey, pkey, data, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * skey;
	DBT * pkey;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_pget_msg msg;
	__db_pget_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.skeydlen = skey->dlen;
	msg.skeydoff = skey->doff;
	msg.skeyulen = skey->ulen;
	msg.skeyflags = skey->flags;
	msg.skeydata.skeydata_val = skey->data;
	msg.skeydata.skeydata_len = skey->size;
	msg.pkeydlen = pkey->dlen;
	msg.pkeydoff = pkey->doff;
	msg.pkeyulen = pkey->ulen;
	msg.pkeyflags = pkey->flags;
	msg.pkeydata.pkeydata_val = pkey->data;
	msg.pkeydata.pkeydata_len = pkey->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = flags;

	replyp = __db_db_pget_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_pget_ret(dbp, txnp, skey, pkey, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_pget_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_db_put(dbp, txnp, key, data, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_put_msg msg;
	__db_put_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = flags;

	replyp = __db_db_put_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_put_ret(dbp, txnp, key, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_put_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_re_delim __P((DB *, int *));
 */
int
__dbcl_db_get_re_delim(dbp, delimp)
	DB * dbp;
	int * delimp;
{
	CLIENT *cl;
	__db_get_re_delim_msg msg;
	__db_get_re_delim_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_re_delim_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (delimp != NULL)
		*delimp = replyp->delim;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_re_delim_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_re_delim __P((DB *, int));
 */
int
__dbcl_db_re_delim(dbp, delim)
	DB * dbp;
	int delim;
{
	CLIENT *cl;
	__db_re_delim_msg msg;
	__db_re_delim_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.delim = delim;

	replyp = __db_db_re_delim_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_re_delim_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_re_len __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_re_len(dbp, lenp)
	DB * dbp;
	u_int32_t * lenp;
{
	CLIENT *cl;
	__db_get_re_len_msg msg;
	__db_get_re_len_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_re_len_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (lenp != NULL)
		*lenp = replyp->len;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_re_len_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_re_len __P((DB *, u_int32_t));
 */
int
__dbcl_db_re_len(dbp, len)
	DB * dbp;
	u_int32_t len;
{
	CLIENT *cl;
	__db_re_len_msg msg;
	__db_re_len_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.len = len;

	replyp = __db_db_re_len_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_re_len_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_re_pad __P((DB *, int));
 */
int
__dbcl_db_re_pad(dbp, pad)
	DB * dbp;
	int pad;
{
	CLIENT *cl;
	__db_re_pad_msg msg;
	__db_re_pad_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.pad = pad;

	replyp = __db_db_re_pad_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_re_pad_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_re_pad __P((DB *, int *));
 */
int
__dbcl_db_get_re_pad(dbp, padp)
	DB * dbp;
	int * padp;
{
	CLIENT *cl;
	__db_get_re_pad_msg msg;
	__db_get_re_pad_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_re_pad_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (padp != NULL)
		*padp = replyp->pad;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_re_pad_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_re_source __P((DB *, const char * *));
 */
int
__dbcl_db_get_re_source(dbp, re_sourcep)
	DB * dbp;
	const char * * re_sourcep;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(re_sourcep, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_get_re_source"));
}

/*
 * PUBLIC: int __dbcl_db_re_source __P((DB *, const char *));
 */
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

/*
 * PUBLIC: int __dbcl_db_remove __P((DB *, const char *, const char *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_remove(dbp, name, subdb, flags)
	DB * dbp;
	const char * name;
	const char * subdb;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_remove_msg msg;
	__db_remove_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	msg.flags = flags;

	replyp = __db_db_remove_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_remove_ret(dbp, name, subdb, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_remove_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_rename __P((DB *, const char *, const char *,
 * PUBLIC:      const char *, u_int32_t));
 */
int
__dbcl_db_rename(dbp, name, subdb, newname, flags)
	DB * dbp;
	const char * name;
	const char * subdb;
	const char * newname;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_rename_msg msg;
	__db_rename_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	if (newname == NULL)
		msg.newname = "";
	else
		msg.newname = (char *)newname;
	msg.flags = flags;

	replyp = __db_db_rename_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_rename_ret(dbp, name, subdb, newname, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_rename_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_stat __P((DB *, DB_TXN *, void *, u_int32_t));
 */
int
__dbcl_db_stat(dbp, txnp, sp, flags)
	DB * dbp;
	DB_TXN * txnp;
	void * sp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_stat_msg msg;
	__db_stat_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = flags;

	replyp = __db_db_stat_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_stat_ret(dbp, txnp, sp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_stat_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_sync __P((DB *, u_int32_t));
 */
int
__dbcl_db_sync(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_sync_msg msg;
	__db_sync_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.flags = flags;

	replyp = __db_db_sync_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_sync_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_truncate __P((DB *, DB_TXN *, u_int32_t  *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_truncate(dbp, txnp, countp, flags)
	DB * dbp;
	DB_TXN * txnp;
	u_int32_t  * countp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_truncate_msg msg;
	__db_truncate_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = flags;

	replyp = __db_db_truncate_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_truncate_ret(dbp, txnp, countp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_truncate_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_upgrade __P((DB *, const char *, u_int32_t));
 */
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

/*
 * PUBLIC: int __dbcl_db_verify __P((DB *, const char *, const char *, FILE *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_verify(dbp, fname, subdb, outfile, flags)
	DB * dbp;
	const char * fname;
	const char * subdb;
	FILE * outfile;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;
	COMPQUIET(fname, NULL);
	COMPQUIET(subdb, NULL);
	COMPQUIET(outfile, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "db_verify"));
}

/*
 * PUBLIC: int __dbcl_db_cursor __P((DB *, DB_TXN *, DBC **, u_int32_t));
 */
int
__dbcl_db_cursor(dbp, txnp, dbcpp, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBC ** dbcpp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_cursor_msg msg;
	__db_cursor_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = flags;

	replyp = __db_db_cursor_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_cursor_ret(dbp, txnp, dbcpp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_cursor_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_join __P((DB *, DBC **, DBC **, u_int32_t));
 */
int
__dbcl_db_join(dbp, curs, dbcp, flags)
	DB * dbp;
	DBC ** curs;
	DBC ** dbcp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_join_msg msg;
	__db_join_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;
	DBC ** cursp;
	int cursi;
	u_int32_t * cursq;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	for (cursi = 0, cursp = curs; *cursp != 0;  cursi++, cursp++)
		;
	msg.curs.curs_len = cursi;
	if ((ret = __os_calloc(dbenv,
	    msg.curs.curs_len, sizeof(u_int32_t), &msg.curs.curs_val)) != 0)
		return (ret);
	for (cursq = msg.curs.curs_val, cursp = curs; cursi--; cursq++, cursp++)
		*cursq = (*cursp)->cl_id;
	msg.flags = flags;

	replyp = __db_db_join_4003(&msg, cl);
	__os_free(dbenv, msg.curs.curs_val);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_join_ret(dbp, curs, dbcp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_join_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_close __P((DBC *));
 */
int
__dbcl_dbc_close(dbc)
	DBC * dbc;
{
	CLIENT *cl;
	__dbc_close_msg msg;
	__dbc_close_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;

	replyp = __db_dbc_close_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_close_ret(dbc, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_close_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_count __P((DBC *, db_recno_t *, u_int32_t));
 */
int
__dbcl_dbc_count(dbc, countp, flags)
	DBC * dbc;
	db_recno_t * countp;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_count_msg msg;
	__dbc_count_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.flags = flags;

	replyp = __db_dbc_count_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_count_ret(dbc, countp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_count_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_del __P((DBC *, u_int32_t));
 */
int
__dbcl_dbc_del(dbc, flags)
	DBC * dbc;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_del_msg msg;
	__dbc_del_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.flags = flags;

	replyp = __db_dbc_del_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_del_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_dup __P((DBC *, DBC **, u_int32_t));
 */
int
__dbcl_dbc_dup(dbc, dbcp, flags)
	DBC * dbc;
	DBC ** dbcp;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_dup_msg msg;
	__dbc_dup_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.flags = flags;

	replyp = __db_dbc_dup_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_dup_ret(dbc, dbcp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_dup_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_get __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_dbc_get(dbc, key, data, flags)
	DBC * dbc;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_get_msg msg;
	__dbc_get_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = flags;

	replyp = __db_dbc_get_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_get_ret(dbc, key, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_get_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_pget __P((DBC *, DBT *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_dbc_pget(dbc, skey, pkey, data, flags)
	DBC * dbc;
	DBT * skey;
	DBT * pkey;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_pget_msg msg;
	__dbc_pget_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.skeydlen = skey->dlen;
	msg.skeydoff = skey->doff;
	msg.skeyulen = skey->ulen;
	msg.skeyflags = skey->flags;
	msg.skeydata.skeydata_val = skey->data;
	msg.skeydata.skeydata_len = skey->size;
	msg.pkeydlen = pkey->dlen;
	msg.pkeydoff = pkey->doff;
	msg.pkeyulen = pkey->ulen;
	msg.pkeyflags = pkey->flags;
	msg.pkeydata.pkeydata_val = pkey->data;
	msg.pkeydata.pkeydata_len = pkey->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = flags;

	replyp = __db_dbc_pget_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_pget_ret(dbc, skey, pkey, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_pget_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_put __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_dbc_put(dbc, key, data, flags)
	DBC * dbc;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_put_msg msg;
	__dbc_put_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = flags;

	replyp = __db_dbc_put_4003(&msg, cl);
	if (replyp == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_put_ret(dbc, key, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_put_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_lock_detect __P((DB_ENV *, u_int32_t, u_int32_t, int *));
 */
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

/*
 * PUBLIC: int __dbcl_lock_get __P((DB_ENV *, u_int32_t, u_int32_t,
 * PUBLIC:      const DBT *, db_lockmode_t, DB_LOCK *));
 */
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

/*
 * PUBLIC: int __dbcl_lock_id __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_lock_id(dbenv, idp)
	DB_ENV * dbenv;
	u_int32_t * idp;
{
	COMPQUIET(idp, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_id"));
}

/*
 * PUBLIC: int __dbcl_lock_id_free __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_lock_id_free(dbenv, id)
	DB_ENV * dbenv;
	u_int32_t id;
{
	COMPQUIET(id, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_id_free"));
}

/*
 * PUBLIC: int __dbcl_lock_put __P((DB_ENV *, DB_LOCK *));
 */
int
__dbcl_lock_put(dbenv, lock)
	DB_ENV * dbenv;
	DB_LOCK * lock;
{
	COMPQUIET(lock, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_put"));
}

/*
 * PUBLIC: int __dbcl_lock_stat __P((DB_ENV *, DB_LOCK_STAT **, u_int32_t));
 */
int
__dbcl_lock_stat(dbenv, statp, flags)
	DB_ENV * dbenv;
	DB_LOCK_STAT ** statp;
	u_int32_t flags;
{
	COMPQUIET(statp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "lock_stat"));
}

/*
 * PUBLIC: int __dbcl_lock_vec __P((DB_ENV *, u_int32_t, u_int32_t,
 * PUBLIC:      DB_LOCKREQ *, int, DB_LOCKREQ **));
 */
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

/*
 * PUBLIC: int __dbcl_log_archive __P((DB_ENV *, char ***, u_int32_t));
 */
int
__dbcl_log_archive(dbenv, listp, flags)
	DB_ENV * dbenv;
	char *** listp;
	u_int32_t flags;
{
	COMPQUIET(listp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_archive"));
}

/*
 * PUBLIC: int __dbcl_log_cursor __P((DB_ENV *, DB_LOGC **, u_int32_t));
 */
int
__dbcl_log_cursor(dbenv, logcp, flags)
	DB_ENV * dbenv;
	DB_LOGC ** logcp;
	u_int32_t flags;
{
	COMPQUIET(logcp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_cursor"));
}

/*
 * PUBLIC: int __dbcl_log_file __P((DB_ENV *, const DB_LSN *, char *, size_t));
 */
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

/*
 * PUBLIC: int __dbcl_log_flush __P((DB_ENV *, const DB_LSN *));
 */
int
__dbcl_log_flush(dbenv, lsn)
	DB_ENV * dbenv;
	const DB_LSN * lsn;
{
	COMPQUIET(lsn, NULL);
	return (__dbcl_rpc_illegal(dbenv, "log_flush"));
}

/*
 * PUBLIC: int __dbcl_log_put __P((DB_ENV *, DB_LSN *, const DBT *,
 * PUBLIC:      u_int32_t));
 */
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

/*
 * PUBLIC: int __dbcl_log_stat __P((DB_ENV *, DB_LOG_STAT **, u_int32_t));
 */
int
__dbcl_log_stat(dbenv, statp, flags)
	DB_ENV * dbenv;
	DB_LOG_STAT ** statp;
	u_int32_t flags;
{
	COMPQUIET(statp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "log_stat"));
}

/*
 * PUBLIC: int __dbcl_memp_register __P((DB_ENV *, int, int (*)(DB_ENV *,
 * PUBLIC:      db_pgno_t, void *, DBT *), int (*)(DB_ENV *, db_pgno_t, void *, DBT *)));
 */
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

/*
 * PUBLIC: int __dbcl_memp_stat __P((DB_ENV *, DB_MPOOL_STAT **,
 * PUBLIC:      DB_MPOOL_FSTAT ***, u_int32_t));
 */
int
__dbcl_memp_stat(dbenv, gstatp, fstatp, flags)
	DB_ENV * dbenv;
	DB_MPOOL_STAT ** gstatp;
	DB_MPOOL_FSTAT *** fstatp;
	u_int32_t flags;
{
	COMPQUIET(gstatp, 0);
	COMPQUIET(fstatp, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_stat"));
}

/*
 * PUBLIC: int __dbcl_memp_sync __P((DB_ENV *, DB_LSN *));
 */
int
__dbcl_memp_sync(dbenv, lsn)
	DB_ENV * dbenv;
	DB_LSN * lsn;
{
	COMPQUIET(lsn, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_sync"));
}

/*
 * PUBLIC: int __dbcl_memp_trickle __P((DB_ENV *, int, int *));
 */
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

/*
 * PUBLIC: int __dbcl_memp_fget __P((DB_MPOOLFILE *, u_int32_t *, u_int32_t,
 * PUBLIC:      void *));
 */
int
__dbcl_memp_fget(dbmfp, pgnoaddr, flags, addrp)
	DB_MPOOLFILE * dbmfp;
	u_int32_t * pgnoaddr;
	u_int32_t flags;
	void * addrp;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(pgnoaddr, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(addrp, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_fget"));
}

/*
 * PUBLIC: int __dbcl_memp_fopen __P((DB_MPOOLFILE *, const char *, u_int32_t,
 * PUBLIC:      int, size_t));
 */
int
__dbcl_memp_fopen(dbmfp, path, flags, mode, pagesize)
	DB_MPOOLFILE * dbmfp;
	const char * path;
	u_int32_t flags;
	int mode;
	size_t pagesize;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(path, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(mode, 0);
	COMPQUIET(pagesize, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_fopen"));
}

/*
 * PUBLIC: int __dbcl_memp_fput __P((DB_MPOOLFILE *, void *, u_int32_t));
 */
int
__dbcl_memp_fput(dbmfp, pgaddr, flags)
	DB_MPOOLFILE * dbmfp;
	void * pgaddr;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(pgaddr, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_fput"));
}

/*
 * PUBLIC: int __dbcl_memp_fset __P((DB_MPOOLFILE *, void *, u_int32_t));
 */
int
__dbcl_memp_fset(dbmfp, pgaddr, flags)
	DB_MPOOLFILE * dbmfp;
	void * pgaddr;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(pgaddr, 0);
	COMPQUIET(flags, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_fset"));
}

/*
 * PUBLIC: int __dbcl_memp_get_clear_len __P((DB_MPOOLFILE *, u_int32_t *));
 */
int
__dbcl_memp_get_clear_len(dbmfp, clear_lenp)
	DB_MPOOLFILE * dbmfp;
	u_int32_t * clear_lenp;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(clear_lenp, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_get_clear_len"));
}

/*
 * PUBLIC: int __dbcl_memp_set_clear_len __P((DB_MPOOLFILE *, u_int32_t));
 */
int
__dbcl_memp_set_clear_len(dbmfp, clear_len)
	DB_MPOOLFILE * dbmfp;
	u_int32_t clear_len;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(clear_len, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_set_clear_len"));
}

/*
 * PUBLIC: int __dbcl_memp_get_fileid __P((DB_MPOOLFILE *, u_int8_t *));
 */
int
__dbcl_memp_get_fileid(dbmfp, fileid)
	DB_MPOOLFILE * dbmfp;
	u_int8_t * fileid;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(fileid, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_get_fileid"));
}

/*
 * PUBLIC: int __dbcl_memp_set_fileid __P((DB_MPOOLFILE *, u_int8_t *));
 */
int
__dbcl_memp_set_fileid(dbmfp, fileid)
	DB_MPOOLFILE * dbmfp;
	u_int8_t * fileid;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(fileid, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_set_fileid"));
}

/*
 * PUBLIC: int __dbcl_memp_get_flags __P((DB_MPOOLFILE *, u_int32_t *));
 */
int
__dbcl_memp_get_flags(dbmfp, flagsp)
	DB_MPOOLFILE * dbmfp;
	u_int32_t * flagsp;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(flagsp, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_get_flags"));
}

/*
 * PUBLIC: int __dbcl_memp_set_flags __P((DB_MPOOLFILE *, u_int32_t, int));
 */
int
__dbcl_memp_set_flags(dbmfp, flags, onoff)
	DB_MPOOLFILE * dbmfp;
	u_int32_t flags;
	int onoff;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(flags, 0);
	COMPQUIET(onoff, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_set_flags"));
}

/*
 * PUBLIC: int __dbcl_memp_get_ftype __P((DB_MPOOLFILE *, int *));
 */
int
__dbcl_memp_get_ftype(dbmfp, ftype)
	DB_MPOOLFILE * dbmfp;
	int * ftype;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(ftype, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_get_ftype"));
}

/*
 * PUBLIC: int __dbcl_memp_set_ftype __P((DB_MPOOLFILE *, int));
 */
int
__dbcl_memp_set_ftype(dbmfp, ftype)
	DB_MPOOLFILE * dbmfp;
	int ftype;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(ftype, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_set_ftype"));
}

/*
 * PUBLIC: int __dbcl_memp_get_lsn_offset __P((DB_MPOOLFILE *, int32_t *));
 */
int
__dbcl_memp_get_lsn_offset(dbmfp, lsn_offsetp)
	DB_MPOOLFILE * dbmfp;
	int32_t * lsn_offsetp;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(lsn_offsetp, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_get_lsn_offset"));
}

/*
 * PUBLIC: int __dbcl_memp_set_lsn_offset __P((DB_MPOOLFILE *, int32_t));
 */
int
__dbcl_memp_set_lsn_offset(dbmfp, lsn_offset)
	DB_MPOOLFILE * dbmfp;
	int32_t lsn_offset;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(lsn_offset, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_set_lsn_offset"));
}

/*
 * PUBLIC: int __dbcl_memp_get_maxsize __P((DB_MPOOLFILE *, u_int32_t *,
 * PUBLIC:      u_int32_t *));
 */
int
__dbcl_memp_get_maxsize(dbmfp, gbytesp, bytesp)
	DB_MPOOLFILE * dbmfp;
	u_int32_t * gbytesp;
	u_int32_t * bytesp;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(gbytesp, 0);
	COMPQUIET(bytesp, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_get_maxsize"));
}

/*
 * PUBLIC: int __dbcl_memp_set_maxsize __P((DB_MPOOLFILE *, u_int32_t,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_memp_set_maxsize(dbmfp, gbytes, bytes)
	DB_MPOOLFILE * dbmfp;
	u_int32_t gbytes;
	u_int32_t bytes;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(gbytes, 0);
	COMPQUIET(bytes, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_set_maxsize"));
}

/*
 * PUBLIC: int __dbcl_memp_get_pgcookie __P((DB_MPOOLFILE *, DBT *));
 */
int
__dbcl_memp_get_pgcookie(dbmfp, pgcookie)
	DB_MPOOLFILE * dbmfp;
	DBT * pgcookie;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(pgcookie, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_get_pgcookie"));
}

/*
 * PUBLIC: int __dbcl_memp_set_pgcookie __P((DB_MPOOLFILE *, DBT *));
 */
int
__dbcl_memp_set_pgcookie(dbmfp, pgcookie)
	DB_MPOOLFILE * dbmfp;
	DBT * pgcookie;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(pgcookie, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_set_pgcookie"));
}

/*
 * PUBLIC: int __dbcl_memp_get_priority __P((DB_MPOOLFILE *,
 * PUBLIC:      DB_CACHE_PRIORITY *));
 */
int
__dbcl_memp_get_priority(dbmfp, priorityp)
	DB_MPOOLFILE * dbmfp;
	DB_CACHE_PRIORITY * priorityp;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(priorityp, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_get_priority"));
}

/*
 * PUBLIC: int __dbcl_memp_set_priority __P((DB_MPOOLFILE *,
 * PUBLIC:      DB_CACHE_PRIORITY));
 */
int
__dbcl_memp_set_priority(dbmfp, priority)
	DB_MPOOLFILE * dbmfp;
	DB_CACHE_PRIORITY priority;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	COMPQUIET(priority, 0);
	return (__dbcl_rpc_illegal(dbenv, "memp_set_priority"));
}

/*
 * PUBLIC: int __dbcl_memp_fsync __P((DB_MPOOLFILE *));
 */
int
__dbcl_memp_fsync(dbmfp)
	DB_MPOOLFILE * dbmfp;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbenv;
	return (__dbcl_rpc_illegal(dbenv, "memp_fsync"));
}

