/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *      Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifdef HAVE_RPC
#ifndef lint
static const char revid[] = "$Id: db_server_proc.c,v 1.48 2001/01/06 16:08:01 sue Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>

#include <string.h>
#endif
#include "db_server.h"

#include "db_int.h"
#include "db_server_int.h"
#include "rpc_server_ext.h"

static int __db_stats_list __P((DB_ENV *,
	      __db_stat_statsreplist **, u_int32_t *, int));

/* BEGIN __env_cachesize_1_proc */
void
__env_cachesize_1_proc(dbenvcl_id, gbytes, bytes,
		ncache, replyp)
	long dbenvcl_id;
	u_int32_t gbytes;
	u_int32_t bytes;
	u_int32_t ncache;
	__env_cachesize_reply *replyp;
/* END __env_cachesize_1_proc */
{
	int ret;
	DB_ENV * dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	ret = dbenv->set_cachesize(dbenv, gbytes, bytes, ncache);

	replyp->status = ret;
	return;
}

/* BEGIN __env_close_1_proc */
void
__env_close_1_proc(dbenvcl_id, flags, replyp)
	long dbenvcl_id;
	u_int32_t flags;
	__env_close_reply *replyp;
/* END __env_close_1_proc */
{
	replyp->status = __dbenv_close_int(dbenvcl_id, flags);
	return;
}

/* BEGIN __env_create_1_proc */
void
__env_create_1_proc(timeout, replyp)
	u_int32_t timeout;
	__env_create_reply *replyp;
/* END __env_create_1_proc */
{
	int ret;
	DB_ENV *dbenv;
	ct_entry *ctp;

	ctp = new_ct_ent(&replyp->status);
	if (ctp == NULL)
		return;
	if ((ret = db_env_create(&dbenv, 0)) == 0) {
		ctp->ct_envp = dbenv;
		ctp->ct_type = CT_ENV;
		ctp->ct_parent = NULL;
		ctp->ct_envparent = ctp;
		__dbsrv_settimeout(ctp, timeout);
		__dbsrv_active(ctp);
		replyp->envcl_id = ctp->ct_id;
	} else
		__dbclear_ctp(ctp);

	replyp->status = ret;
	return;
}

/* BEGIN __env_flags_1_proc */
void
__env_flags_1_proc(dbenvcl_id, flags, onoff, replyp)
	long dbenvcl_id;
	u_int32_t flags;
	u_int32_t onoff;
	__env_flags_reply *replyp;
/* END __env_flags_1_proc */
{
	int ret;
	DB_ENV * dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	ret = dbenv->set_flags(dbenv, flags, onoff);

	replyp->status = ret;
	return;
}
/* BEGIN __env_open_1_proc */
void
__env_open_1_proc(dbenvcl_id, home, flags,
		mode, replyp)
	long dbenvcl_id;
	char *home;
	u_int32_t flags;
	u_int32_t mode;
	__env_open_reply *replyp;
/* END __env_open_1_proc */
{
	int ret;
	DB_ENV * dbenv;
	ct_entry *dbenv_ctp;
	char *fullhome;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;
	fullhome = get_home(home);
	if (fullhome == NULL) {
		replyp->status = DB_NOSERVER_HOME;
		return;
	}

	ret = dbenv->open(dbenv, fullhome, flags, mode);
	replyp->status = ret;
	return;
}

/* BEGIN __env_remove_1_proc */
void
__env_remove_1_proc(dbenvcl_id, home, flags, replyp)
	long dbenvcl_id;
	char *home;
	u_int32_t flags;
	__env_remove_reply *replyp;
/* END __env_remove_1_proc */
{
	int ret;
	DB_ENV * dbenv;
	ct_entry *dbenv_ctp;
	char *fullhome;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;
	fullhome = get_home(home);
	if (fullhome == NULL) {
		replyp->status = DB_NOSERVER_HOME;
		return;
	}

	ret = dbenv->remove(dbenv, fullhome, flags);
	__dbdel_ctp(dbenv_ctp);
	replyp->status = ret;
	return;
}

/* BEGIN __txn_abort_1_proc */
void
__txn_abort_1_proc(txnpcl_id, replyp)
	long txnpcl_id;
	__txn_abort_reply *replyp;
/* END __txn_abort_1_proc */
{
	DB_TXN * txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DB_TXN *)txnp_ctp->ct_anyp;

	ret =  txn_abort(txnp);
	__dbdel_ctp(txnp_ctp);
	replyp->status = ret;
	return;
}

/* BEGIN __txn_begin_1_proc */
void
__txn_begin_1_proc(envpcl_id, parentcl_id,
		flags, replyp)
	long envpcl_id;
	long parentcl_id;
	u_int32_t flags;
	__txn_begin_reply *replyp;
/* END __txn_begin_1_proc */
{
	int ret;
	DB_ENV * envp;
	ct_entry *envp_ctp;
	DB_TXN * parent;
	ct_entry *parent_ctp;
	DB_TXN *txnp;
	ct_entry *ctp;

	ACTIVATE_CTP(envp_ctp, envpcl_id, CT_ENV);
	envp = (DB_ENV *)envp_ctp->ct_anyp;
	parent_ctp = NULL;

	ctp = new_ct_ent(&replyp->status);
	if (ctp == NULL)
		return;

	if (parentcl_id != 0) {
		ACTIVATE_CTP(parent_ctp, parentcl_id, CT_TXN);
		parent = (DB_TXN *)parent_ctp->ct_anyp;
		ctp->ct_activep = parent_ctp->ct_activep;
	} else
		parent = NULL;

	ret = txn_begin(envp, parent, &txnp, flags);
	if (ret == 0) {
		ctp->ct_txnp = txnp;
		ctp->ct_type = CT_TXN;
		ctp->ct_parent = parent_ctp;
		ctp->ct_envparent = envp_ctp;
		replyp->txnidcl_id = ctp->ct_id;
		__dbsrv_settimeout(ctp, envp_ctp->ct_timeout);
		__dbsrv_active(ctp);
	} else
		__dbclear_ctp(ctp);

	replyp->status = ret;
	return;
}

/* BEGIN __txn_commit_1_proc */
void
__txn_commit_1_proc(txnpcl_id, flags, replyp)
	long txnpcl_id;
	u_int32_t flags;
	__txn_commit_reply *replyp;
/* END __txn_commit_1_proc */
{
	int ret;
	DB_TXN * txnp;
	ct_entry *txnp_ctp;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DB_TXN *)txnp_ctp->ct_anyp;

	ret = txn_commit(txnp, flags);
	__dbdel_ctp(txnp_ctp);

	replyp->status = ret;
	return;
}

/* BEGIN __db_bt_maxkey_1_proc */
void
__db_bt_maxkey_1_proc(dbpcl_id, maxkey, replyp)
	long dbpcl_id;
	u_int32_t maxkey;
	__db_bt_maxkey_reply *replyp;
/* END __db_bt_maxkey_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_bt_maxkey(dbp, maxkey);

	replyp->status = ret;
	return;
}

/* BEGIN __db_bt_minkey_1_proc */
void
__db_bt_minkey_1_proc(dbpcl_id, minkey, replyp)
	long dbpcl_id;
	u_int32_t minkey;
	__db_bt_minkey_reply *replyp;
/* END __db_bt_minkey_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_bt_minkey(dbp, minkey);

	replyp->status = ret;
	return;
}

/* BEGIN __db_close_1_proc */
void
__db_close_1_proc(dbpcl_id, flags, replyp)
	long dbpcl_id;
	u_int32_t flags;
	__db_close_reply *replyp;
/* END __db_close_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->close(dbp, flags);
	__dbdel_ctp(dbp_ctp);

	replyp-> status= ret;
	return;
}

/* BEGIN __db_create_1_proc */
void
__db_create_1_proc(flags, envpcl_id, replyp)
	u_int32_t flags;
	long envpcl_id;
	__db_create_reply *replyp;
/* END __db_create_1_proc */
{
	int ret;
	DB_ENV * envp;
	DB *dbp;
	ct_entry *envp_ctp, *dbp_ctp;

	ACTIVATE_CTP(envp_ctp, envpcl_id, CT_ENV);
	envp = (DB_ENV *)envp_ctp->ct_anyp;

	dbp_ctp = new_ct_ent(&replyp->status);
	if (dbp_ctp == NULL)
		return ;
	/*
	 * We actually require env's for databases.  The client should
	 * have caught it, but just in case.
	 */
	DB_ASSERT(envp != NULL);
	if ((ret = db_create(&dbp, envp, flags)) == 0) {
		dbp_ctp->ct_dbp = dbp;
		dbp_ctp->ct_type = CT_DB;
		dbp_ctp->ct_parent = envp_ctp;
		dbp_ctp->ct_envparent = envp_ctp;
		replyp->dbpcl_id = dbp_ctp->ct_id;
	} else
		__dbclear_ctp(dbp_ctp);
	replyp->status = ret;
	return;
}

/* BEGIN __db_del_1_proc */
void
__db_del_1_proc(dbpcl_id, txnpcl_id, keydlen,
		keydoff, keyflags, keydata, keysize,
		flags, replyp)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t flags;
	__db_del_reply *replyp;
/* END __db_del_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;
	DB_TXN * txnp;
	ct_entry *txnp_ctp;
	DBT key;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	memset(&key, 0, sizeof(key));

	/* Set up key DBT */
	key.dlen = keydlen;
	key.doff = keydoff;
	key.flags = keyflags;
	key.size = keysize;
	key.data = keydata;

	ret = dbp->del(dbp, txnp, &key, flags);

	replyp->status = ret;
	return;
}

/* BEGIN __db_extentsize_1_proc */
void
__db_extentsize_1_proc(dbpcl_id, extentsize, replyp)
	long dbpcl_id;
	u_int32_t extentsize;
	__db_extentsize_reply *replyp;
/* END __db_extentsize_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_q_extentsize(dbp, extentsize);

	replyp->status = ret;
	return;
}

/* BEGIN __db_flags_1_proc */
void
__db_flags_1_proc(dbpcl_id, flags, replyp)
	long dbpcl_id;
	u_int32_t flags;
	__db_flags_reply *replyp;
/* END __db_flags_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_flags(dbp, flags);

	replyp->status = ret;
	return;
}

/* BEGIN __db_get_1_proc */
void
__db_get_1_proc(dbpcl_id, txnpcl_id, keydlen,
		keydoff, keyflags, keydata, keysize,
		datadlen, datadoff, dataflags, datadata,
		datasize, flags, replyp, freep)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__db_get_reply *replyp;
	int * freep;
/* END __db_get_1_proc */
{
	int key_alloc, ret;
	DB * dbp;
	ct_entry *dbp_ctp;
	DB_TXN * txnp;
	ct_entry *txnp_ctp;
	DBT key, data;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	*freep = 0;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Set up key and data DBT */
	key.dlen = keydlen;
	key.doff = keydoff;
	/*
	 * Ignore memory related flags on server.
	 */
	key.flags = DB_DBT_MALLOC;
	if (keyflags & DB_DBT_PARTIAL)
		key.flags |= DB_DBT_PARTIAL;
	key.size = keysize;
	key.data = keydata;

	data.dlen = datadlen;
	data.doff = datadoff;
	/*
	 * Ignore memory related flags on server.
	 */
	data.flags = DB_DBT_MALLOC;
	if (dataflags & DB_DBT_PARTIAL)
		data.flags |= DB_DBT_PARTIAL;
	data.size = datasize;
	data.data = datadata;

	/* Got all our stuff, now do the get */
	ret = dbp->get(dbp, txnp, &key, &data, flags);
	/*
	 * Otherwise just status.
	 */
	if (ret == 0) {
		/*
		 * XXX
		 * We need to xdr_free whatever we are returning, next time.
		 * However, DB does not allocate a new key if one was given
		 * and we'd be free'ing up space allocated in the request.
		 * So, allocate a new key/data pointer if it is the same one
		 * as in the request.
		 */
		*freep = 1;
		/*
		 * Key
		 */
		key_alloc = 0;
		if (key.data == keydata) {
			ret = __os_malloc(dbp->dbenv,
			    key.size, NULL, &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_free(key.data, key.size);
				__os_free(data.data, data.size);
				goto err;
			}
			key_alloc = 1;
			memcpy(replyp->keydata.keydata_val, key.data, key.size);
		} else
			replyp->keydata.keydata_val = key.data;

		replyp->keydata.keydata_len = key.size;

		/*
		 * Data
		 */
		if (data.data == datadata) {
			ret = __os_malloc(dbp->dbenv,
			     data.size, NULL, &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_free(key.data, key.size);
				__os_free(data.data, data.size);
				if (key_alloc)
					__os_free(replyp->keydata.keydata_val,
					    key.size);
				goto err;
			}
			memcpy(replyp->datadata.datadata_val, data.data,
			    data.size);
		} else
			replyp->datadata.datadata_val = data.data;
		replyp->datadata.datadata_len = data.size;
	} else {
err:		replyp->keydata.keydata_val = NULL;
		replyp->keydata.keydata_len = 0;
		replyp->datadata.datadata_val = NULL;
		replyp->datadata.datadata_len = 0;
		*freep = 0;
	}
	replyp->status = ret;
	return;
}

/* BEGIN __db_h_ffactor_1_proc */
void
__db_h_ffactor_1_proc(dbpcl_id, ffactor, replyp)
	long dbpcl_id;
	u_int32_t ffactor;
	__db_h_ffactor_reply *replyp;
/* END __db_h_ffactor_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_h_ffactor(dbp, ffactor);

	replyp->status = ret;
	return;
}

/* BEGIN __db_h_nelem_1_proc */
void
__db_h_nelem_1_proc(dbpcl_id, nelem, replyp)
	long dbpcl_id;
	u_int32_t nelem;
	__db_h_nelem_reply *replyp;
/* END __db_h_nelem_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_h_nelem(dbp, nelem);

	replyp->status = ret;
	return;
}

/* BEGIN __db_key_range_1_proc */
void
__db_key_range_1_proc(dbpcl_id, txnpcl_id, keydlen,
		keydoff, keyflags, keydata, keysize,
		flags, replyp)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t flags;
	__db_key_range_reply *replyp;
/* END __db_key_range_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;
	DB_TXN * txnp;
	ct_entry *txnp_ctp;
	DBT key;
	DB_KEY_RANGE range;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	memset(&key, 0, sizeof(key));
	/* Set up key and data DBT */
	key.dlen = keydlen;
	key.doff = keydoff;
	key.size = keysize;
	key.data = keydata;
	key.flags = keyflags;

	ret = dbp->key_range(dbp, txnp, &key, &range, flags);

	replyp->status = ret;
	replyp->less = range.less;
	replyp->equal = range.equal;
	replyp->greater = range.greater;
	return;
}

/* BEGIN __db_lorder_1_proc */
void
__db_lorder_1_proc(dbpcl_id, lorder, replyp)
	long dbpcl_id;
	u_int32_t lorder;
	__db_lorder_reply *replyp;
/* END __db_lorder_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_lorder(dbp, lorder);

	replyp->status = ret;
	return;
}

/* BEGIN __dbopen_1_proc */
void
__db_open_1_proc(dbpcl_id, name, subdb,
		type, flags, mode, replyp)
	long dbpcl_id;
	char *name;
	char *subdb;
	u_int32_t type;
	u_int32_t flags;
	u_int32_t mode;
	__db_open_reply *replyp;
/* END __db_open_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->open(dbp, name, subdb, (DBTYPE)type, flags, mode);
	if (ret == 0) {
		replyp->type = (int) dbp->get_type(dbp);
		/* XXX
		 * Tcl needs to peek at dbp->flags for DB_AM_DUP.  Send
		 * this dbp's flags back.
		 */
		replyp->dbflags = (int) dbp->flags;
	}
	replyp->status = ret;
	return;
}

/* BEGIN __db_pagesize_1_proc */
void
__db_pagesize_1_proc(dbpcl_id, pagesize, replyp)
	long dbpcl_id;
	u_int32_t pagesize;
	__db_pagesize_reply *replyp;
/* END __db_pagesize_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_pagesize(dbp, pagesize);

	replyp->status = ret;
	return;
}

/* BEGIN __db_put_1_proc */
void
__db_put_1_proc(dbpcl_id, txnpcl_id, keydlen,
		keydoff, keyflags, keydata, keysize,
		datadlen, datadoff, dataflags, datadata,
		datasize, flags, replyp, freep)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__db_put_reply *replyp;
	int * freep;
/* END __db_put_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;
	DB_TXN * txnp;
	ct_entry *txnp_ctp;
	DBT key, data;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	*freep = 0;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Set up key and data DBT */
	key.dlen = keydlen;
	key.doff = keydoff;
	/*
	 * Ignore memory related flags on server.
	 */
	key.flags = DB_DBT_MALLOC;
	if (keyflags & DB_DBT_PARTIAL)
		key.flags |= DB_DBT_PARTIAL;
	key.size = keysize;
	key.data = keydata;

	data.dlen = datadlen;
	data.doff = datadoff;
	data.flags = dataflags;
	data.size = datasize;
	data.data = datadata;

	/* Got all our stuff, now do the put */
	ret = dbp->put(dbp, txnp, &key, &data, flags);
	/*
	 * If the client did a DB_APPEND, set up key in reply.
	 * Otherwise just status.
	 */
	if (ret == 0 && (flags == DB_APPEND)) {
		/*
		 * XXX
		 * We need to xdr_free whatever we are returning, next time.
		 * However, DB does not allocate a new key if one was given
		 * and we'd be free'ing up space allocated in the request.
		 * So, allocate a new key/data pointer if it is the same one
		 * as in the request.
		 */
		*freep = 1;
		/*
		 * Key
		 */
		if (key.data == keydata) {
			ret = __os_malloc(dbp->dbenv,
			    key.size, NULL, &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_free(key.data, key.size);
				goto err;
			}
			memcpy(replyp->keydata.keydata_val, key.data, key.size);
		} else
			replyp->keydata.keydata_val = key.data;

		replyp->keydata.keydata_len = key.size;
	} else {
err:		replyp->keydata.keydata_val = NULL;
		replyp->keydata.keydata_len = 0;
		*freep = 0;
	}
	replyp->status = ret;
	return;
}

/* BEGIN __db_re_delim_1_proc */
void
__db_re_delim_1_proc(dbpcl_id, delim, replyp)
	long dbpcl_id;
	u_int32_t delim;
	__db_re_delim_reply *replyp;
/* END __db_re_delim_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_delim(dbp, delim);

	replyp->status = ret;
	return;
}

/* BEGIN __db_re_len_1_proc */
void
__db_re_len_1_proc(dbpcl_id, len, replyp)
	long dbpcl_id;
	u_int32_t len;
	__db_re_len_reply *replyp;
/* END __db_re_len_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_len(dbp, len);

	replyp->status = ret;
	return;
}

/* BEGIN __db_re_pad_1_proc */
void
__db_re_pad_1_proc(dbpcl_id, pad, replyp)
	long dbpcl_id;
	u_int32_t pad;
	__db_re_pad_reply *replyp;
/* END __db_re_pad_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_pad(dbp, pad);

	replyp->status = ret;
	return;
}

/* BEGIN __db_remove_1_proc */
void
__db_remove_1_proc(dbpcl_id, name, subdb,
		flags, replyp)
	long dbpcl_id;
	char *name;
	char *subdb;
	u_int32_t flags;
	__db_remove_reply *replyp;
/* END __db_remove_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->remove(dbp, name, subdb, flags);
	__dbdel_ctp(dbp_ctp);

	replyp->status = ret;
	return;
}

/* BEGIN __db_rename_1_proc */
void
__db_rename_1_proc(dbpcl_id, name, subdb,
		newname, flags, replyp)
	long dbpcl_id;
	char *name;
	char *subdb;
	char *newname;
	u_int32_t flags;
	__db_rename_reply *replyp;
/* END __db_rename_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->rename(dbp, name, subdb, newname, flags);
	__dbdel_ctp(dbp_ctp);

	replyp->status = ret;
	return;
}

/* BEGIN __db_stat_1_proc */
void
__db_stat_1_proc(dbpcl_id,
		flags, replyp, freep)
	long dbpcl_id;
	u_int32_t flags;
	__db_stat_reply *replyp;
	int * freep;
/* END __db_stat_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;
	DBTYPE type;
	void *sp;
	int len;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->stat(dbp, &sp, NULL, flags);
	replyp->status = ret;
	if (ret != 0)
		return;
	/*
	 * We get here, we have success.  Allocate an array so that
	 * we can use the list generator.  Generate the reply, free
	 * up the space.
	 */
	/*
	 * XXX This assumes that all elements of all stat structures
	 * are u_int32_t fields.  They are, currently.
	 */
	type = dbp->get_type(dbp);
	if (type == DB_HASH)
		len = sizeof(DB_HASH_STAT) / sizeof(u_int32_t);
	else if (type == DB_QUEUE)
		len = sizeof(DB_QUEUE_STAT) / sizeof(u_int32_t);
	else            /* BTREE or RECNO are same stats */
		len = sizeof(DB_BTREE_STAT) / sizeof(u_int32_t);
	/*
	 * Set up our list of stats.
	 */
	ret = __db_stats_list(dbp->dbenv,
	    &replyp->statslist, (u_int32_t*)sp, len);

	__os_free(sp, 0);
	if (ret == 0)
		*freep = 1;
	replyp->status = ret;
	return;
}

int
__db_stats_list(dbenv, locp, pp, len)
	DB_ENV *dbenv;
	__db_stat_statsreplist **locp;
	u_int32_t *pp;
	int len;
{
	u_int32_t *p, *q;
	int i, ret;
	__db_stat_statsreplist *nl, **nlp;

	nlp = locp;
	for (i = 0; i < len; i++) {
		p = pp+i;
		if ((ret = __os_malloc(dbenv, sizeof(*nl), NULL, nlp)) != 0)
			goto out;
		nl = *nlp;
		nl->next = NULL;
		if ((ret = __os_malloc(dbenv,
		    sizeof(u_int32_t), NULL, &nl->ent.ent_val)) != 0)
			goto out;
		q = (u_int32_t *)nl->ent.ent_val;
		*q = *p;
		nl->ent.ent_len = sizeof(u_int32_t);
		nlp = &nl->next;
	}
	return (0);
out:
	__db_stats_freelist(locp);
	return (ret);
}

/*
 * PUBLIC: void __db_stats_freelist __P((__db_stat_statsreplist **));
 */
void
__db_stats_freelist(locp)
	__db_stat_statsreplist **locp;
{
	__db_stat_statsreplist *nl, *nl1;

	for (nl = *locp; nl != NULL; nl = nl1) {
		nl1 = nl->next;
		if (nl->ent.ent_val)
			__os_free(nl->ent.ent_val, nl->ent.ent_len);
		__os_free(nl, sizeof(*nl));
	}
	*locp = NULL;
}

/* BEGIN __db_swapped_1_proc */
void
__db_swapped_1_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_swapped_reply *replyp;
/* END __db_swapped_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->get_byteswapped(dbp);

	replyp->status = ret;
	return;
}

/* BEGIN __db_sync_1_proc */
void
__db_sync_1_proc(dbpcl_id, flags, replyp)
	long dbpcl_id;
	u_int32_t flags;
	__db_sync_reply *replyp;
/* END __db_sync_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->sync(dbp, flags);

	replyp->status = ret;
	return;
}

/* BEGIN __db_cursor_1_proc */
void
__db_cursor_1_proc(dbpcl_id, txnpcl_id,
		flags, replyp)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t flags;
	__db_cursor_reply *replyp;
/* END __db_cursor_1_proc */
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;
	DB_TXN * txnp;
	ct_entry *txnp_ctp;
	DBC *dbc;
	ct_entry *dbc_ctp, *env_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	dbc_ctp = new_ct_ent(&replyp->status);
	if (dbc_ctp == NULL)
		return;

	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
		dbc_ctp->ct_activep = txnp_ctp->ct_activep;
	} else
		txnp = NULL;

	if ((ret = dbp->cursor(dbp, txnp, &dbc, flags)) == 0) {
		dbc_ctp->ct_dbc = dbc;
		dbc_ctp->ct_type = CT_CURSOR;
		dbc_ctp->ct_parent = dbp_ctp;
		env_ctp = dbp_ctp->ct_envparent;
		dbc_ctp->ct_envparent = env_ctp;
		__dbsrv_settimeout(dbc_ctp, env_ctp->ct_timeout);
		__dbsrv_active(dbc_ctp);
		replyp->dbcidcl_id = dbc_ctp->ct_id;
	} else
		__dbclear_ctp(dbc_ctp);

	replyp->status = ret;
	return;
}

/* BEGIN __db_join_1_proc */
void
__db_join_1_proc(dbpcl_id, curslist,
		flags, replyp)
	long dbpcl_id;
	u_int32_t * curslist;
	u_int32_t flags;
	__db_join_reply *replyp;
/* END __db_join_1_proc */
{
	DB * dbp;
	ct_entry *dbp_ctp;
	DBC *dbc;
	DBC **jcurs, **c;
	ct_entry *dbc_ctp, *ctp;
	size_t size;
	int ret;
	u_int32_t *cl;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	dbc_ctp = new_ct_ent(&replyp->status);
	if (dbc_ctp == NULL)
		return;

	for (size = sizeof(DBC *), cl = curslist; *cl != 0; size += sizeof(DBC *), cl++)
		;
	if ((ret = __os_malloc(dbp->dbenv, size, NULL, &jcurs)) != 0) {
		replyp->status = ret;
		__dbclear_ctp(dbc_ctp);
		return;
	}
	/*
	 * If our curslist has a parent txn, we need to use it too
	 * for the activity timeout.  All cursors must be part of
	 * the same transaction, so just check the first.
	 */
	ctp = get_tableent(*curslist);
	DB_ASSERT(ctp->ct_type == CT_CURSOR);
	/*
	 * If we are using a transaction, set the join activity timer
	 * to point to the parent transaction.
	 */
	if (ctp->ct_activep != &ctp->ct_active)
		dbc_ctp->ct_activep = ctp->ct_activep;
	for (cl = curslist, c = jcurs; *cl != 0; cl++, c++) {
		ctp = get_tableent(*cl);
		if (ctp == NULL) {
			replyp->status = DB_NOSERVER_ID;
			goto out;
		}
		/*
		 * If we are using a txn, the join cursor points to the
		 * transaction timeout.  If we are not using a transaction,
		 * then all the curslist cursors must point to the join
		 * cursor's timeout so that we do not timeout any of the
		 * curlist cursors while the join cursor is active.
		 * Change the type of the curslist ctps to CT_JOIN so that
		 * we know they are part of a join list and we can distinguish
		 * them and later restore them when the join cursor is closed.
		 */
		DB_ASSERT(ctp->ct_type == CT_CURSOR);
		ctp->ct_type |= CT_JOIN;
		ctp->ct_origp = ctp->ct_activep;
		/*
		 * Setting this to the ct_active field of the dbc_ctp is
		 * really just a way to distinguish which join dbc this
		 * cursor is part of.  The ct_activep of this cursor is
		 * not used at all during its lifetime as part of a join
		 * cursor.
		 */
		ctp->ct_activep = &dbc_ctp->ct_active;
		*c = ctp->ct_dbc;
	}
	*c = NULL;
	if ((ret = dbp->join(dbp, jcurs, &dbc, flags)) == 0) {
		dbc_ctp->ct_dbc = dbc;
		dbc_ctp->ct_type = (CT_JOINCUR | CT_CURSOR);
		dbc_ctp->ct_parent = dbp_ctp;
		dbc_ctp->ct_envparent = dbp_ctp->ct_envparent;
		__dbsrv_settimeout(dbc_ctp, dbp_ctp->ct_envparent->ct_timeout);
		__dbsrv_active(dbc_ctp);
		replyp->dbcidcl_id = dbc_ctp->ct_id;
	} else {
		__dbclear_ctp(dbc_ctp);
		/*
		 * If we get an error, undo what we did above to any cursors.
		 */
		for (cl = curslist; *cl != 0; cl++) {
			ctp = get_tableent(*cl);
			ctp->ct_type = CT_CURSOR;
			ctp->ct_activep = ctp->ct_origp;
		}
	}

	replyp->status = ret;
out:
	__os_free(jcurs, size);
	return;
}

/* BEGIN __dbc_close_1_proc */
void
__dbc_close_1_proc(dbccl_id, replyp)
	long dbccl_id;
	__dbc_close_reply *replyp;
/* END __dbc_close_1_proc */
{
	ct_entry *dbc_ctp;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	replyp->status = __dbc_close_int(dbc_ctp);
	return;
}

/* BEGIN __dbc_count_1_proc */
void
__dbc_count_1_proc(dbccl_id, flags, replyp)
	long dbccl_id;
	u_int32_t flags;
	__dbc_count_reply *replyp;
/* END __dbc_count_1_proc */
{
	int ret;
	DBC * dbc;
	ct_entry *dbc_ctp;
	db_recno_t num;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;

	ret = dbc->c_count(dbc, &num, flags);
	replyp->status = ret;
	if (ret == 0)
		replyp->dupcount = num;
	return;
}

/* BEGIN __dbc_del_1_proc */
void
__dbc_del_1_proc(dbccl_id, flags, replyp)
	long dbccl_id;
	u_int32_t flags;
	__dbc_del_reply *replyp;
/* END __dbc_del_1_proc */
{
	int ret;
	DBC * dbc;
	ct_entry *dbc_ctp;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;

	ret = dbc->c_del(dbc, flags);

	replyp->status = ret;
	return;
}

/* BEGIN __dbc_dup_1_proc */
void
__dbc_dup_1_proc(dbccl_id, flags, replyp)
	long dbccl_id;
	u_int32_t flags;
	__dbc_dup_reply *replyp;
/* END __dbc_dup_1_proc */
{
	int ret;
	DBC * dbc;
	ct_entry *dbc_ctp;
	DBC *newdbc;
	ct_entry *new_ctp;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;

	new_ctp = new_ct_ent(&replyp->status);
	if (new_ctp == NULL)
		return;

	if ((ret = dbc->c_dup(dbc, &newdbc, flags)) == 0) {
		new_ctp->ct_dbc = newdbc;
		new_ctp->ct_type = CT_CURSOR;
		new_ctp->ct_parent = dbc_ctp->ct_parent;
		new_ctp->ct_envparent = dbc_ctp->ct_envparent;
		/*
		 * If our cursor has a parent txn, we need to use it too.
		 */
		if (dbc_ctp->ct_activep != &dbc_ctp->ct_active)
			new_ctp->ct_activep = dbc_ctp->ct_activep;
		__dbsrv_settimeout(new_ctp, dbc_ctp->ct_timeout);
		__dbsrv_active(new_ctp);
		replyp->dbcidcl_id = new_ctp->ct_id;
	} else
		__dbclear_ctp(new_ctp);

	replyp->status = ret;
	return;
}

/* BEGIN __dbc_get_1_proc */
void
__dbc_get_1_proc(dbccl_id, keydlen, keydoff,
		keyflags, keydata, keysize, datadlen,
		datadoff, dataflags, datadata, datasize,
		flags, replyp, freep)
	long dbccl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__dbc_get_reply *replyp;
	int * freep;
/* END __dbc_get_1_proc */
{
	DB_ENV *dbenv;
	DBC *dbc;
	DBT key, data;
	ct_entry *dbc_ctp;
	int key_alloc, ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;
	dbenv = dbc->dbp->dbenv;

	*freep = 0;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Set up key and data DBT */
	key.dlen = keydlen;
	key.doff = keydoff;
	/*
	 * Ignore memory related flags on server.
	 */
	key.flags = DB_DBT_MALLOC;
	if (keyflags & DB_DBT_PARTIAL)
		key.flags |= DB_DBT_PARTIAL;
	key.size = keysize;
	key.data = keydata;

	data.dlen = datadlen;
	data.doff = datadoff;
	data.flags = DB_DBT_MALLOC;
	if (dataflags & DB_DBT_PARTIAL)
		data.flags |= DB_DBT_PARTIAL;
	data.size = datasize;
	data.data = datadata;

	/* Got all our stuff, now do the get */
	ret = dbc->c_get(dbc, &key, &data, flags);

	/*
	 * Otherwise just status.
	 */
	if (ret == 0) {
		/*
		 * XXX
		 * We need to xdr_free whatever we are returning, next time.
		 * However, DB does not allocate a new key if one was given
		 * and we'd be free'ing up space allocated in the request.
		 * So, allocate a new key/data pointer if it is the same one
		 * as in the request.
		 */
		*freep = 1;
		/*
		 * Key
		 */
		key_alloc = 0;
		if (key.data == keydata) {
			ret = __os_malloc(dbenv, key.size, NULL,
			    &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_free(key.data, key.size);
				__os_free(data.data, data.size);
				goto err;
			}
			key_alloc = 1;
			memcpy(replyp->keydata.keydata_val, key.data, key.size);
		} else
			replyp->keydata.keydata_val = key.data;

		replyp->keydata.keydata_len = key.size;

		/*
		 * Data
		 */
		if (data.data == datadata) {
			ret = __os_malloc(dbenv, data.size, NULL,
			    &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_free(key.data, key.size);
				__os_free(data.data, data.size);
				if (key_alloc)
					__os_free(replyp->keydata.keydata_val,
					    key.size);
				goto err;
			}
			memcpy(replyp->datadata.datadata_val, data.data,
			    data.size);
		} else
			replyp->datadata.datadata_val = data.data;
		replyp->datadata.datadata_len = data.size;
	} else {
err:		replyp->keydata.keydata_val = NULL;
		replyp->keydata.keydata_len = 0;
		replyp->datadata.datadata_val = NULL;
		replyp->datadata.datadata_len = 0;
		*freep = 0;
	}
	replyp->status = ret;
	return;
}

/* BEGIN __dbc_put_1_proc */
void
__dbc_put_1_proc(dbccl_id, keydlen, keydoff,
		keyflags, keydata, keysize, datadlen,
		datadoff, dataflags, datadata, datasize,
		flags, replyp, freep)
	long dbccl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__dbc_put_reply *replyp;
	int * freep;
/* END __dbc_put_1_proc */
{
	int ret;
	DBC * dbc;
	DB *dbp;
	ct_entry *dbc_ctp;
	DBT key, data;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;
	dbp = (DB *)dbc_ctp->ct_parent->ct_anyp;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Set up key and data DBT */
	key.dlen = keydlen;
	key.doff = keydoff;
	/*
	 * Ignore memory related flags on server.
	 */
	key.flags = 0;
	if (keyflags & DB_DBT_PARTIAL)
		key.flags |= DB_DBT_PARTIAL;
	key.size = keysize;
	key.data = keydata;

	data.dlen = datadlen;
	data.doff = datadoff;
	data.flags = dataflags;
	data.size = datasize;
	data.data = datadata;

	/* Got all our stuff, now do the put */
	ret = dbc->c_put(dbc, &key, &data, flags);

	*freep = 0;
	if (ret == 0 && (flags == DB_AFTER || flags == DB_BEFORE) &&
	    dbp->type == DB_RECNO) {
		/*
		 * We need to xdr_free whatever we are returning, next time.
		 */
		replyp->keydata.keydata_val = key.data;
		replyp->keydata.keydata_len = key.size;
	} else {
		replyp->keydata.keydata_val = NULL;
		replyp->keydata.keydata_len = 0;
	}
	replyp->status = ret;
	return;
}
#endif /* HAVE_RPC */
