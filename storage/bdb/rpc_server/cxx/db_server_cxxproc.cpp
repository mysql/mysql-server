/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: db_server_cxxproc.cpp,v 1.23 2004/09/22 17:30:12 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>

#include <string.h>
#endif

#include "db_server.h"

#include "db_int.h"
#include "db_cxx.h"

extern "C" {
#include "dbinc/db_server_int.h"
#include "dbinc_auto/rpc_server_ext.h"
}

extern "C" void
__env_get_cachesize_proc(
	long dbenvcl_id,
	__env_get_cachesize_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_cachesize(&replyp->gbytes,
	    &replyp->bytes, (int *)&replyp->ncache);
}

extern "C" void
__env_cachesize_proc(
	long dbenvcl_id,
	u_int32_t gbytes,
	u_int32_t bytes,
	u_int32_t ncache,
	__env_cachesize_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;
	int ret;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	ret = dbenv->set_cachesize(gbytes, bytes, ncache);

	replyp->status = ret;
	return;
}

extern "C" void
__env_close_proc(
	long dbenvcl_id,
	u_int32_t flags,
	__env_close_reply *replyp)
{
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	replyp->status = __dbenv_close_int(dbenvcl_id, flags, 0);
	return;
}

extern "C" void
__env_create_proc(
	u_int32_t timeout,
	__env_create_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *ctp;

	ctp = new_ct_ent(&replyp->status);
	if (ctp == NULL)
		return;

	dbenv = new DbEnv(DB_CXX_NO_EXCEPTIONS);
	ctp->ct_envp = dbenv;
	ctp->ct_type = CT_ENV;
	ctp->ct_parent = NULL;
	ctp->ct_envparent = ctp;
	__dbsrv_settimeout(ctp, timeout);
	__dbsrv_active(ctp);
	replyp->envcl_id = ctp->ct_id;

	replyp->status = 0;
	return;
}

extern "C" void
__env_dbremove_proc(
	long dbenvcl_id,
	long txnpcl_id,
	char *name,
	char *subdb,
	u_int32_t flags,
	__env_dbremove_reply *replyp)
{
	int ret;
	DbEnv *dbenv;
	DbTxn *txnp;
	ct_entry *dbenv_ctp, *txnp_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	ret = dbenv->dbremove(txnp, name, subdb, flags);

	replyp->status = ret;
	return;
}

void
__env_dbrename_proc(
	long dbenvcl_id,
	long txnpcl_id,
	char *name,
	char *subdb,
	char *newname,
	u_int32_t flags,
	__env_dbrename_reply *replyp)
{
	int ret;
	DbEnv *dbenv;
	DbTxn *txnp;
	ct_entry *dbenv_ctp, *txnp_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	ret = dbenv->dbrename(txnp, name, subdb, newname, flags);

	replyp->status = ret;
	return;
}

extern "C" void
__env_get_encrypt_flags_proc(
	long dbenvcl_id,
	__env_get_encrypt_flags_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_encrypt_flags(&replyp->flags);
}

extern "C" void
__env_encrypt_proc(
	long dbenvcl_id,
	char *passwd,
	u_int32_t flags,
	__env_encrypt_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;
	int ret;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	ret = dbenv->set_encrypt(passwd, flags);

	replyp->status = ret;
	return;
}

extern "C" void
__env_get_flags_proc(
	long dbenvcl_id,
	__env_get_flags_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_flags(&replyp->flags);
}

extern "C" void
__env_flags_proc(
	long dbenvcl_id,
	u_int32_t flags,
	u_int32_t onoff,
	__env_flags_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;
	int ret;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	ret = dbenv->set_flags(flags, onoff);
	if (onoff)
		dbenv_ctp->ct_envdp.onflags = flags;
	else
		dbenv_ctp->ct_envdp.offflags = flags;

	replyp->status = ret;
	return;
}

extern "C" void
__env_get_home_proc(
	long dbenvcl_id,
	__env_get_home_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_home((const char **)&replyp->home);
}

extern "C" void
__env_get_open_flags_proc(
	long dbenvcl_id,
	__env_get_open_flags_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_open_flags(&replyp->flags);
}

extern "C" void
__env_open_proc(
	long dbenvcl_id,
	char *home,
	u_int32_t flags,
	u_int32_t mode,
	__env_open_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp, *new_ctp;
	u_int32_t newflags, shareflags;
	int ret;
	home_entry *fullhome;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;
	fullhome = get_fullhome(home);
	if (fullhome == NULL) {
		ret = DB_NOSERVER_HOME;
		goto out;
	}

	/*
	 * If they are using locking do deadlock detection for them,
	 * internally.
	 */
	if ((flags & DB_INIT_LOCK) &&
	    (ret = dbenv->set_lk_detect(DB_LOCK_DEFAULT)) != 0)
		goto out;

	if (__dbsrv_verbose) {
		dbenv->set_errfile(stderr);
		dbenv->set_errpfx(fullhome->home);
	}

	/*
	 * Mask off flags we ignore
	 */
	newflags = (flags & ~DB_SERVER_FLAGMASK);
	shareflags = (newflags & DB_SERVER_ENVFLAGS);
	/*
	 * Check now whether we can share a handle for this env.
	 */
	replyp->envcl_id = dbenvcl_id;
	if ((new_ctp = __dbsrv_shareenv(dbenv_ctp, fullhome, shareflags))
	    != NULL) {
		/*
		 * We can share, clean up old  ID, set new one.
		 */
		if (__dbsrv_verbose)
			printf("Sharing env ID %ld\n", new_ctp->ct_id);
		replyp->envcl_id = new_ctp->ct_id;
		ret = __dbenv_close_int(dbenvcl_id, 0, 0);
	} else {
		ret = dbenv->open(fullhome->home, newflags, mode);
		dbenv_ctp->ct_envdp.home = fullhome;
		dbenv_ctp->ct_envdp.envflags = shareflags;
	}
out:	replyp->status = ret;
	return;
}

extern "C" void
__env_remove_proc(
	long dbenvcl_id,
	char *home,
	u_int32_t flags,
	__env_remove_reply *replyp)
{
	DbEnv *dbenv;
	ct_entry *dbenv_ctp;
	int ret;
	home_entry *fullhome;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;
	fullhome = get_fullhome(home);
	if (fullhome == NULL) {
		replyp->status = DB_NOSERVER_HOME;
		return;
	}

	ret = dbenv->remove(fullhome->home, flags);
	__dbdel_ctp(dbenv_ctp);
	replyp->status = ret;
	return;
}

extern "C" void
__txn_abort_proc(
	long txnpcl_id,
	__txn_abort_reply *replyp)
{
	DbTxn *txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DbTxn *)txnp_ctp->ct_anyp;

	ret = txnp->abort();
	__dbdel_ctp(txnp_ctp);
	replyp->status = ret;
	return;
}

extern "C" void
__txn_begin_proc(
	long dbenvcl_id,
	long parentcl_id,
	u_int32_t flags,
	__txn_begin_reply *replyp)
{
	DbEnv *dbenv;
	DbTxn *parent, *txnp;
	ct_entry *ctp, *dbenv_ctp, *parent_ctp;
	int ret;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;
	parent_ctp = NULL;

	ctp = new_ct_ent(&replyp->status);
	if (ctp == NULL)
		return;

	if (parentcl_id != 0) {
		ACTIVATE_CTP(parent_ctp, parentcl_id, CT_TXN);
		parent = (DbTxn *)parent_ctp->ct_anyp;
		ctp->ct_activep = parent_ctp->ct_activep;
	} else
		parent = NULL;

	ret = dbenv->txn_begin(parent, &txnp, flags | DB_TXN_NOWAIT);
	if (ret == 0) {
		ctp->ct_txnp = txnp;
		ctp->ct_type = CT_TXN;
		ctp->ct_parent = parent_ctp;
		ctp->ct_envparent = dbenv_ctp;
		replyp->txnidcl_id = ctp->ct_id;
		__dbsrv_settimeout(ctp, dbenv_ctp->ct_timeout);
		__dbsrv_active(ctp);
	} else
		__dbclear_ctp(ctp);

	replyp->status = ret;
	return;
}

extern "C" void
__txn_commit_proc(
	long txnpcl_id,
	u_int32_t flags,
	__txn_commit_reply *replyp)
{
	DbTxn *txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DbTxn *)txnp_ctp->ct_anyp;

	ret = txnp->commit(flags);
	__dbdel_ctp(txnp_ctp);

	replyp->status = ret;
	return;
}

extern "C" void
__txn_discard_proc(
	long txnpcl_id,
	u_int32_t flags,
	__txn_discard_reply *replyp)
{
	DbTxn *txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DbTxn *)txnp_ctp->ct_anyp;

	ret = txnp->discard(flags);
	__dbdel_ctp(txnp_ctp);

	replyp->status = ret;
	return;
}

extern "C" void
__txn_prepare_proc(
	long txnpcl_id,
	u_int8_t *gid,
	__txn_prepare_reply *replyp)
{
	DbTxn *txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DbTxn *)txnp_ctp->ct_anyp;

	ret = txnp->prepare(gid);
	replyp->status = ret;
	return;
}

extern "C" void
__txn_recover_proc(
	long dbenvcl_id,
	u_int32_t count,
	u_int32_t flags,
	__txn_recover_reply *replyp,
	int * freep)
{
	DbEnv *dbenv;
	DbPreplist *dbprep, *p;
	ct_entry *dbenv_ctp, *ctp;
	long erri, i, retcount;
	u_int32_t *txnidp;
	int ret;
	char *gid;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;
	*freep = 0;

	if ((ret = __os_malloc(
	    dbenv->get_DB_ENV(), count * sizeof(DbPreplist), &dbprep)) != 0)
		goto out;
	if ((ret = dbenv->txn_recover(dbprep, count, &retcount, flags)) != 0)
		goto out;
	/*
	 * If there is nothing, success, but it's easy.
	 */
	replyp->retcount = retcount; // TODO: fix C++ txn_recover
	if (retcount == 0) {
		replyp->txn.txn_val = NULL;
		replyp->txn.txn_len = 0;
		replyp->gid.gid_val = NULL;
		replyp->gid.gid_len = 0;
	}

	/*
	 * We have our txn list.  Now we need to allocate the space for
	 * the txn ID array and the GID array and set them up.
	 */
	if ((ret = __os_calloc(dbenv->get_DB_ENV(), retcount, sizeof(u_int32_t),
	    &replyp->txn.txn_val)) != 0)
		goto out;
	replyp->txn.txn_len = retcount * sizeof(u_int32_t);
	if ((ret = __os_calloc(dbenv->get_DB_ENV(), retcount, DB_XIDDATASIZE,
	    &replyp->gid.gid_val)) != 0) {
		__os_free(dbenv->get_DB_ENV(), replyp->txn.txn_val);
		goto out;
	}
	replyp->gid.gid_len = retcount * DB_XIDDATASIZE;

	/*
	 * Now walk through our results, creating parallel arrays
	 * to send back.  For each entry we need to create a new
	 * txn ctp and then fill in the array info.
	 */
	i = 0;
	p = dbprep;
	gid = replyp->gid.gid_val;
	txnidp = replyp->txn.txn_val;
	while (i++ < retcount) {
		ctp = new_ct_ent(&ret);
		if (ret != 0) {
			i--;
			goto out2;
		}
		ctp->ct_txnp = p->txn;
		ctp->ct_type = CT_TXN;
		ctp->ct_parent = NULL;
		ctp->ct_envparent = dbenv_ctp;
		__dbsrv_settimeout(ctp, dbenv_ctp->ct_timeout);
		__dbsrv_active(ctp);

		*txnidp = ctp->ct_id;
		memcpy(gid, p->gid, DB_XIDDATASIZE);

		p++;
		txnidp++;
		gid += DB_XIDDATASIZE;
	}
	/*
	 * If we get here, we have success and we have to set freep
	 * so it'll get properly freed next time.
	 */
	*freep = 1;
out:
	if (dbprep != NULL)
		__os_free(dbenv->get_DB_ENV(), dbprep);
	replyp->status = ret;
	return;
out2:
	/*
	 * We had an error in the middle of creating our new txn
	 * ct entries.  We have to unwind all that we have done.  Ugh.
	 */
	for (txnidp = replyp->txn.txn_val, erri = 0;
	    erri < i; erri++, txnidp++) {
		ctp = get_tableent(*txnidp);
		__dbclear_ctp(ctp);
	}
	__os_free(dbenv->get_DB_ENV(), replyp->txn.txn_val);
	__os_free(dbenv->get_DB_ENV(), replyp->gid.gid_val);
	__os_free(dbenv->get_DB_ENV(), dbprep);
	replyp->status = ret;
	return;
}

extern "C" void
__db_bt_maxkey_proc(
	long dbpcl_id,
	u_int32_t maxkey,
	__db_bt_maxkey_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_bt_maxkey(maxkey);

	replyp->status = ret;
	return;
}

extern "C" void
__db_associate_proc(
	long dbpcl_id,
	long txnpcl_id,
	long sdbpcl_id,
	u_int32_t flags,
	__db_associate_reply *replyp)
{
	Db *dbp, *sdbp;
	DbTxn *txnp;
	ct_entry *dbp_ctp, *sdbp_ctp, *txnp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	ACTIVATE_CTP(sdbp_ctp, sdbpcl_id, CT_DB);
	sdbp = (Db *)sdbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	/*
	 * We do not support DB_CREATE for associate or the callbacks
	 * implemented in the Java and JE RPC servers.   Users can only
	 * access secondary indices on a read-only basis, so whatever they
	 * are looking for needs to be there already.
	 */
	if (LF_ISSET(DB_RPC2ND_MASK | DB_CREATE))
		ret = EINVAL;
	else
		ret = dbp->associate(txnp, sdbp, NULL, flags);

	replyp->status = ret;
	return;
}

extern "C" void
__db_get_bt_minkey_proc(
	long dbpcl_id,
	__db_get_bt_minkey_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_bt_minkey(&replyp->minkey);
}

extern "C" void
__db_bt_minkey_proc(
	long dbpcl_id,
	u_int32_t minkey,
	__db_bt_minkey_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_bt_minkey(minkey);

	replyp->status = ret;
	return;
}

extern "C" void
__db_close_proc(
	long dbpcl_id,
	u_int32_t flags,
	__db_close_reply *replyp)
{
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	replyp->status = __db_close_int(dbpcl_id, flags);
	return;
}

extern "C" void
__db_create_proc(
	long dbenvcl_id,
	u_int32_t flags,
	__db_create_reply *replyp)
{
	Db *dbp;
	DbEnv *dbenv;
	ct_entry *dbenv_ctp, *dbp_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DbEnv *)dbenv_ctp->ct_anyp;

	dbp_ctp = new_ct_ent(&replyp->status);
	if (dbp_ctp == NULL)
		return ;
	/*
	 * We actually require env's for databases.  The client should
	 * have caught it, but just in case.
	 */
	DB_ASSERT(dbenv != NULL);
	dbp = new Db(dbenv, flags);
	dbp_ctp->ct_dbp = dbp;
	dbp_ctp->ct_type = CT_DB;
	dbp_ctp->ct_parent = dbenv_ctp;
	dbp_ctp->ct_envparent = dbenv_ctp;
	replyp->dbcl_id = dbp_ctp->ct_id;
	replyp->status = 0;
	return;
}

extern "C" void
__db_del_proc(
	long dbpcl_id,
	long txnpcl_id,
	u_int32_t keydlen,
	u_int32_t keydoff,
	u_int32_t keyulen,
	u_int32_t keyflags,
	void *keydata,
	u_int32_t keysize,
	u_int32_t flags,
	__db_del_reply *replyp)
{
	Db *dbp;
	DbTxn *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	/* Set up key */
	Dbt key(keydata, keysize);
	key.set_dlen(keydlen);
	key.set_ulen(keyulen);
	key.set_doff(keydoff);
	key.set_flags(keyflags);

	ret = dbp->del(txnp, &key, flags);

	replyp->status = ret;
	return;
}

extern "C" void
__db_get_encrypt_flags_proc(
	long dbpcl_id,
	__db_get_encrypt_flags_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_encrypt_flags(&replyp->flags);
}

extern "C" void
__db_encrypt_proc(
	long dbpcl_id,
	char *passwd,
	u_int32_t flags,
	__db_encrypt_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_encrypt(passwd, flags);
	replyp->status = ret;
	return;
}

extern "C" void
__db_get_extentsize_proc(
	long dbpcl_id,
	__db_get_extentsize_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_q_extentsize(&replyp->extentsize);
}

extern "C" void
__db_extentsize_proc(
	long dbpcl_id,
	u_int32_t extentsize,
	__db_extentsize_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_q_extentsize(extentsize);

	replyp->status = ret;
	return;
}

extern "C" void
__db_get_flags_proc(
	long dbpcl_id,
	__db_get_flags_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_flags(&replyp->flags);
}

extern "C" void
__db_flags_proc(
	long dbpcl_id,
	u_int32_t flags,
	__db_flags_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_flags(flags);
	dbp_ctp->ct_dbdp.setflags = flags;

	replyp->status = ret;
	return;
}

extern "C" void
__db_get_proc(
	long dbpcl_id,
	long txnpcl_id,
	u_int32_t keydlen,
	u_int32_t keydoff,
	u_int32_t keyulen,
	u_int32_t keyflags,
	void *keydata,
	u_int32_t keysize,
	u_int32_t datadlen,
	u_int32_t datadoff,
	u_int32_t dataulen,
	u_int32_t dataflags,
	void *datadata,
	u_int32_t datasize,
	u_int32_t flags,
	__db_get_reply *replyp,
	int * freep)
{
	Db *dbp;
	DbTxn *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int key_alloc, bulk_alloc, ret;
	void *tmpdata;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	*freep = 0;
	bulk_alloc = 0;

	/* Set up key and data */
	Dbt key(keydata, keysize);
	key.set_dlen(keydlen);
	key.set_ulen(keyulen);
	key.set_doff(keydoff);
	/*
	 * Ignore memory related flags on server.
	 */
	key.set_flags(DB_DBT_MALLOC | (keyflags & DB_DBT_PARTIAL));

	Dbt data(datadata, datasize);
	data.set_dlen(datadlen);
	data.set_ulen(dataulen);
	data.set_doff(datadoff);
	/*
	 * Ignore memory related flags on server.
	 */
	dataflags &= DB_DBT_PARTIAL;
	if (flags & DB_MULTIPLE) {
		if (data.get_data() == 0) {
			ret = __os_umalloc(dbp->get_DB()->dbenv,
			    dataulen, &tmpdata);
			if (ret != 0)
				goto err;
			data.set_data(tmpdata);
			bulk_alloc = 1;
		}
		dataflags |= DB_DBT_USERMEM;
	} else
		dataflags |= DB_DBT_MALLOC;
	data.set_flags(dataflags);

	/* Got all our stuff, now do the get */
	ret = dbp->get(txnp, &key, &data, flags);
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
		if (key.get_data() == keydata) {
			ret = __os_umalloc(dbp->get_DB()->dbenv,
			    key.get_size(), &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_ufree(
				    dbp->get_DB()->dbenv, key.get_data());
				__os_ufree(
				    dbp->get_DB()->dbenv, data.get_data());
				goto err;
			}
			key_alloc = 1;
			memcpy(replyp->keydata.keydata_val,
			    key.get_data(), key.get_size());
		} else
			replyp->keydata.keydata_val = (char *)key.get_data();

		replyp->keydata.keydata_len = key.get_size();

		/*
		 * Data
		 */
		if (data.get_data() == datadata) {
			ret = __os_umalloc(dbp->get_DB()->dbenv,
			     data.get_size(), &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_ufree(
				    dbp->get_DB()->dbenv, key.get_data());
				__os_ufree(
				    dbp->get_DB()->dbenv, data.get_data());
				if (key_alloc)
					__os_ufree(dbp->get_DB()->dbenv,
					    replyp->keydata.keydata_val);
				goto err;
			}
			memcpy(replyp->datadata.datadata_val, data.get_data(),
			    data.get_size());
		} else
			replyp->datadata.datadata_val = (char *)data.get_data();
		replyp->datadata.datadata_len = data.get_size();
	} else {
err:		replyp->keydata.keydata_val = NULL;
		replyp->keydata.keydata_len = 0;
		replyp->datadata.datadata_val = NULL;
		replyp->datadata.datadata_len = 0;
		*freep = 0;
		if (bulk_alloc)
			__os_ufree(dbp->get_DB()->dbenv, data.get_data());
	}
	replyp->status = ret;
	return;
}

extern "C" void
__db_get_h_ffactor_proc(
	long dbpcl_id,
	__db_get_h_ffactor_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_h_ffactor(&replyp->ffactor);
}

extern "C" void
__db_h_ffactor_proc(
	long dbpcl_id,
	u_int32_t ffactor,
	__db_h_ffactor_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_h_ffactor(ffactor);

	replyp->status = ret;
	return;
}

extern "C" void
__db_get_h_nelem_proc(
	long dbpcl_id,
	__db_get_h_nelem_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_h_nelem(&replyp->nelem);
}

extern "C" void
__db_h_nelem_proc(
	long dbpcl_id,
	u_int32_t nelem,
	__db_h_nelem_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_h_nelem(nelem);

	replyp->status = ret;
	return;
}

extern "C" void
__db_key_range_proc(
	long dbpcl_id,
	long txnpcl_id,
	u_int32_t keydlen,
	u_int32_t keydoff,
	u_int32_t keyulen,
	u_int32_t keyflags,
	void *keydata,
	u_int32_t keysize,
	u_int32_t flags,
	__db_key_range_reply *replyp)
{
	Db *dbp;
	DB_KEY_RANGE range;
	DbTxn *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	/* Set up key */
	Dbt key(keydata, keysize);
	key.set_dlen(keydlen);
	key.set_ulen(keyulen);
	key.set_doff(keydoff);
	key.set_flags(keyflags);

	ret = dbp->key_range(txnp, &key, &range, flags);

	replyp->status = ret;
	replyp->less = range.less;
	replyp->equal = range.equal;
	replyp->greater = range.greater;
	return;
}

extern "C" void
__db_get_lorder_proc(
	long dbpcl_id,
	__db_get_lorder_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_lorder((int *)&replyp->lorder);
}

extern "C" void
__db_lorder_proc(
	long dbpcl_id,
	u_int32_t lorder,
	__db_lorder_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_lorder(lorder);

	replyp->status = ret;
	return;
}

extern "C" void
__db_get_name_proc(
	long dbpcl_id,
	__db_get_name_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_dbname(
	    (const char **)&replyp->filename, (const char **)&replyp->dbname);
}

extern "C" void
__db_get_open_flags_proc(
	long dbpcl_id,
	__db_get_open_flags_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_open_flags(&replyp->flags);
}

extern "C" void
__db_open_proc(
	long dbpcl_id,
	long txnpcl_id,
	char *name,
	char *subdb,
	u_int32_t type,
	u_int32_t flags,
	u_int32_t mode,
	__db_open_reply *replyp)
{
	Db *dbp;
	DbTxn *txnp;
	DBTYPE dbtype;
	ct_entry *dbp_ctp, *new_ctp, *txnp_ctp;
	int isswapped, ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	replyp->dbcl_id = dbpcl_id;
	if ((new_ctp = __dbsrv_sharedb(
	    dbp_ctp, name, subdb, (DBTYPE)type, flags)) != NULL) {
		/*
		 * We can share, clean up old ID, set new one.
		 */
		if (__dbsrv_verbose)
			printf("Sharing db ID %ld\n", new_ctp->ct_id);
		replyp->dbcl_id = new_ctp->ct_id;
		ret = __db_close_int(dbpcl_id, 0);
		goto out;
	}
	ret = dbp->open(txnp, name, subdb, (DBTYPE)type, flags, mode);
	if (ret == 0) {
		(void)dbp->get_type(&dbtype);
		replyp->type = dbtype;
		/*
		 * We need to determine the byte order of the database
		 * and send it back to the client.  Determine it by
		 * the server's native order and the swapped value of
		 * the DB itself.
		 */
		(void)dbp->get_byteswapped(&isswapped);
		if (__db_byteorder(NULL, 1234) == 0) {
			if (isswapped == 0)
				replyp->lorder = 1234;
			else
				replyp->lorder = 4321;
		} else {
			if (isswapped == 0)
				replyp->lorder = 4321;
			else
				replyp->lorder = 1234;
		}
		dbp_ctp->ct_dbdp.type = dbtype;
		dbp_ctp->ct_dbdp.dbflags = LF_ISSET(DB_SERVER_DBFLAGS);
		if (name == NULL)
			dbp_ctp->ct_dbdp.db = NULL;
		else if ((ret = __os_strdup(dbp->get_DB()->dbenv, name,
		    &dbp_ctp->ct_dbdp.db)) != 0)
			goto out;
		if (subdb == NULL)
			dbp_ctp->ct_dbdp.subdb = NULL;
		else if ((ret = __os_strdup(dbp->get_DB()->dbenv, subdb,
		    &dbp_ctp->ct_dbdp.subdb)) != 0)
			goto out;
	}
out:
	replyp->status = ret;
	return;
}

extern "C" void
__db_get_pagesize_proc(
	long dbpcl_id,
	__db_get_pagesize_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_pagesize(&replyp->pagesize);
}

extern "C" void
__db_pagesize_proc(
	long dbpcl_id,
	u_int32_t pagesize,
	__db_pagesize_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_pagesize(pagesize);

	replyp->status = ret;
	return;
}

extern "C" void
__db_pget_proc(
	long dbpcl_id,
	long txnpcl_id,
	u_int32_t skeydlen,
	u_int32_t skeydoff,
	u_int32_t skeyulen,
	u_int32_t skeyflags,
	void *skeydata,
	u_int32_t skeysize,
	u_int32_t pkeydlen,
	u_int32_t pkeydoff,
	u_int32_t pkeyulen,
	u_int32_t pkeyflags,
	void *pkeydata,
	u_int32_t pkeysize,
	u_int32_t datadlen,
	u_int32_t datadoff,
	u_int32_t dataulen,
	u_int32_t dataflags,
	void *datadata,
	u_int32_t datasize,
	u_int32_t flags,
	__db_pget_reply *replyp,
	int * freep)
{
	Db *dbp;
	DbTxn *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int key_alloc, ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	*freep = 0;

	/*
	 * Ignore memory related flags on server.
	 */
	/* Set up key and data */
	Dbt skey(skeydata, skeysize);
	skey.set_dlen(skeydlen);
	skey.set_ulen(skeyulen);
	skey.set_doff(skeydoff);
	skey.set_flags(DB_DBT_MALLOC | (skeyflags & DB_DBT_PARTIAL));

	Dbt pkey(pkeydata, pkeysize);
	pkey.set_dlen(pkeydlen);
	pkey.set_ulen(pkeyulen);
	pkey.set_doff(pkeydoff);
	pkey.set_flags(DB_DBT_MALLOC | (pkeyflags & DB_DBT_PARTIAL));

	Dbt data(datadata, datasize);
	data.set_dlen(datadlen);
	data.set_ulen(dataulen);
	data.set_doff(datadoff);
	data.set_flags(DB_DBT_MALLOC | (dataflags & DB_DBT_PARTIAL));

	/* Got all our stuff, now do the get */
	ret = dbp->pget(txnp, &skey, &pkey, &data, flags);
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
		if (skey.get_data() == skeydata) {
			ret = __os_umalloc(dbp->get_DB()->dbenv,
			    skey.get_size(), &replyp->skeydata.skeydata_val);
			if (ret != 0) {
				__os_ufree(
				    dbp->get_DB()->dbenv, skey.get_data());
				__os_ufree(
				    dbp->get_DB()->dbenv, pkey.get_data());
				__os_ufree(
				    dbp->get_DB()->dbenv, data.get_data());
				goto err;
			}
			key_alloc = 1;
			memcpy(replyp->skeydata.skeydata_val, skey.get_data(),
			    skey.get_size());
		} else
			replyp->skeydata.skeydata_val = (char *)skey.get_data();

		replyp->skeydata.skeydata_len = skey.get_size();

		/*
		 * Primary key
		 */
		if (pkey.get_data() == pkeydata) {
			ret = __os_umalloc(dbp->get_DB()->dbenv,
			     pkey.get_size(), &replyp->pkeydata.pkeydata_val);
			if (ret != 0) {
				__os_ufree(
				    dbp->get_DB()->dbenv, skey.get_data());
				__os_ufree(
				    dbp->get_DB()->dbenv, pkey.get_data());
				__os_ufree(
				    dbp->get_DB()->dbenv, data.get_data());
				if (key_alloc)
					__os_ufree(dbp->get_DB()->dbenv,
					    replyp->skeydata.skeydata_val);
				goto err;
			}
			/*
			 * We can set it to 2, because they cannot send the
			 * pkey over without sending the skey over too.
			 * So if they did send a pkey, they must have sent
			 * the skey as well.
			 */
			key_alloc = 2;
			memcpy(replyp->pkeydata.pkeydata_val, pkey.get_data(),
			    pkey.get_size());
		} else
			replyp->pkeydata.pkeydata_val = (char *)pkey.get_data();
		replyp->pkeydata.pkeydata_len = pkey.get_size();

		/*
		 * Data
		 */
		if (data.get_data() == datadata) {
			ret = __os_umalloc(dbp->get_DB()->dbenv,
			     data.get_size(), &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_ufree(
				    dbp->get_DB()->dbenv, skey.get_data());
				__os_ufree(
				    dbp->get_DB()->dbenv, pkey.get_data());
				__os_ufree(
				    dbp->get_DB()->dbenv, data.get_data());
				/*
				 * If key_alloc is 1, just skey needs to be
				 * freed, if key_alloc is 2, both skey and pkey
				 * need to be freed.
				 */
				if (key_alloc--)
					__os_ufree(dbp->get_DB()->dbenv,
					    replyp->skeydata.skeydata_val);
				if (key_alloc)
					__os_ufree(dbp->get_DB()->dbenv,
					    replyp->pkeydata.pkeydata_val);
				goto err;
			}
			memcpy(replyp->datadata.datadata_val, data.get_data(),
			    data.get_size());
		} else
			replyp->datadata.datadata_val = (char *)data.get_data();
		replyp->datadata.datadata_len = data.get_size();
	} else {
err:		replyp->skeydata.skeydata_val = NULL;
		replyp->skeydata.skeydata_len = 0;
		replyp->pkeydata.pkeydata_val = NULL;
		replyp->pkeydata.pkeydata_len = 0;
		replyp->datadata.datadata_val = NULL;
		replyp->datadata.datadata_len = 0;
		*freep = 0;
	}
	replyp->status = ret;
	return;
}

extern "C" void
__db_put_proc(
	long dbpcl_id,
	long txnpcl_id,
	u_int32_t keydlen,
	u_int32_t keydoff,
	u_int32_t keyulen,
	u_int32_t keyflags,
	void *keydata,
	u_int32_t keysize,
	u_int32_t datadlen,
	u_int32_t datadoff,
	u_int32_t dataulen,
	u_int32_t dataflags,
	void *datadata,
	u_int32_t datasize,
	u_int32_t flags,
	__db_put_reply *replyp,
	int * freep)
{
	Db *dbp;
	DbTxn *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	*freep = 0;

	/* Set up key and data */
	Dbt key(keydata, keysize);
	key.set_dlen(keydlen);
	key.set_ulen(keyulen);
	key.set_doff(keydoff);
	key.set_flags(DB_DBT_MALLOC | (keyflags & DB_DBT_PARTIAL));

	Dbt data(datadata, datasize);
	data.set_dlen(datadlen);
	data.set_ulen(dataulen);
	data.set_doff(datadoff);
	data.set_flags(dataflags);

	/* Got all our stuff, now do the put */
	ret = dbp->put(txnp, &key, &data, flags);
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
		if (key.get_data() == keydata) {
			ret = __os_umalloc(dbp->get_DB()->dbenv,
			    key.get_size(), &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_ufree(
				    dbp->get_DB()->dbenv, key.get_data());
				goto err;
			}
			memcpy(replyp->keydata.keydata_val,
			    key.get_data(), key.get_size());
		} else
			replyp->keydata.keydata_val = (char *)key.get_data();

		replyp->keydata.keydata_len = key.get_size();
	} else {
err:		replyp->keydata.keydata_val = NULL;
		replyp->keydata.keydata_len = 0;
		*freep = 0;
	}
	replyp->status = ret;
	return;
}

extern "C" void
__db_get_re_delim_proc(
	long dbpcl_id,
	__db_get_re_delim_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_re_delim((int *)&replyp->delim);
}

extern "C" void
__db_re_delim_proc(
	long dbpcl_id,
	u_int32_t delim,
	__db_re_delim_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_delim(delim);

	replyp->status = ret;
	return;
}

extern "C" void
__db_get_re_len_proc(
	long dbpcl_id,
	__db_get_re_len_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_re_len(&replyp->len);
}

extern "C" void
__db_re_len_proc(
	long dbpcl_id,
	u_int32_t len,
	__db_re_len_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_len(len);

	replyp->status = ret;
	return;
}

void
__db_get_re_pad_proc(
	long dbpcl_id,
	__db_get_re_pad_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_re_pad((int *)&replyp->pad);
}

extern "C" void
__db_re_pad_proc(
	long dbpcl_id,
	u_int32_t pad,
	__db_re_pad_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_pad(pad);

	replyp->status = ret;
	return;
}

extern "C" void
__db_remove_proc(
	long dbpcl_id,
	char *name,
	char *subdb,
	u_int32_t flags,
	__db_remove_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->remove(name, subdb, flags);
	__dbdel_ctp(dbp_ctp);

	replyp->status = ret;
	return;
}

extern "C" void
__db_rename_proc(
	long dbpcl_id,
	char *name,
	char *subdb,
	char *newname,
	u_int32_t flags,
	__db_rename_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->rename(name, subdb, newname, flags);
	__dbdel_ctp(dbp_ctp);

	replyp->status = ret;
	return;
}

extern "C" void
__db_stat_proc(
	long dbpcl_id,
	long txnpcl_id,
	u_int32_t flags,
	__db_stat_reply *replyp,
	int * freep)
{
	Db *dbp;
	DbTxn *txnp;
	DBTYPE type;
	ct_entry *dbp_ctp, *txnp_ctp;
	u_int32_t *q, *p, *retsp;
	int i, len, ret;
	void *sp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	ret = dbp->stat(txnp, &sp, flags);
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
	(void)dbp->get_type(&type);
	if (type == DB_HASH)
		len = sizeof(DB_HASH_STAT);
	else if (type == DB_QUEUE)
		len = sizeof(DB_QUEUE_STAT);
	else            /* BTREE or RECNO are same stats */
		len = sizeof(DB_BTREE_STAT);
	replyp->stats.stats_len = len / sizeof(u_int32_t);

	if ((ret = __os_umalloc(dbp->get_DB()->dbenv,
	    len * replyp->stats.stats_len, &retsp)) != 0)
		goto out;
	for (i = 0, q = retsp, p = (u_int32_t *)sp; i < len;
	    i++, q++, p++)
		*q = *p;
	replyp->stats.stats_val = retsp;
	__os_ufree(dbp->get_DB()->dbenv, sp);
	if (ret == 0)
		*freep = 1;
out:
	replyp->status = ret;
	return;
}

extern "C" void
__db_sync_proc(
	long dbpcl_id,
	u_int32_t flags,
	__db_sync_reply *replyp)
{
	Db *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	ret = dbp->sync(flags);

	replyp->status = ret;
	return;
}

extern "C" void
__db_truncate_proc(
	long dbpcl_id,
	long txnpcl_id,
	u_int32_t flags,
	__db_truncate_reply *replyp)
{
	Db *dbp;
	DbTxn *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	u_int32_t count;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	ret = dbp->truncate(txnp, &count, flags);
	replyp->status = ret;
	if (ret == 0)
		replyp->count = count;
	return;
}

extern "C" void
__db_cursor_proc(
	long dbpcl_id,
	long txnpcl_id,
	u_int32_t flags,
	__db_cursor_reply *replyp)
{
	Db *dbp;
	Dbc *dbc;
	DbTxn *txnp;
	ct_entry *dbc_ctp, *env_ctp, *dbp_ctp, *txnp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;
	dbc_ctp = new_ct_ent(&replyp->status);
	if (dbc_ctp == NULL)
		return;

	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DbTxn *)txnp_ctp->ct_anyp;
		dbc_ctp->ct_activep = txnp_ctp->ct_activep;
	} else
		txnp = NULL;

	if ((ret = dbp->cursor(txnp, &dbc, flags)) == 0) {
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

extern "C" void
__db_join_proc(
	long dbpcl_id,
	u_int32_t *curs,
	u_int32_t curslen,
	u_int32_t flags,
	__db_join_reply *replyp)
{
	Db *dbp;
	Dbc **jcurs, **c;
	Dbc *dbc;
	ct_entry *dbc_ctp, *ctp, *dbp_ctp;
	size_t size;
	u_int32_t *cl, i;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (Db *)dbp_ctp->ct_anyp;

	dbc_ctp = new_ct_ent(&replyp->status);
	if (dbc_ctp == NULL)
		return;

	size = (curslen + 1) * sizeof(Dbc *);
	if ((ret = __os_calloc(dbp->get_DB()->dbenv,
	    curslen + 1, sizeof(Dbc *), &jcurs)) != 0) {
		replyp->status = ret;
		__dbclear_ctp(dbc_ctp);
		return;
	}
	/*
	 * If our curslist has a parent txn, we need to use it too
	 * for the activity timeout.  All cursors must be part of
	 * the same transaction, so just check the first.
	 */
	ctp = get_tableent(*curs);
	DB_ASSERT(ctp->ct_type == CT_CURSOR);
	/*
	 * If we are using a transaction, set the join activity timer
	 * to point to the parent transaction.
	 */
	if (ctp->ct_activep != &ctp->ct_active)
		dbc_ctp->ct_activep = ctp->ct_activep;
	for (i = 0, cl = curs, c = jcurs; i < curslen; i++, cl++, c++) {
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
	if ((ret = dbp->join(jcurs, &dbc, flags)) == 0) {
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
		for (cl = curs; *cl != 0; cl++) {
			ctp = get_tableent(*cl);
			ctp->ct_type = CT_CURSOR;
			ctp->ct_activep = ctp->ct_origp;
		}
	}

	replyp->status = ret;
out:
	__os_free(dbp->get_DB()->dbenv, jcurs);
	return;
}

extern "C" void
__dbc_close_proc(
	long dbccl_id,
	__dbc_close_reply *replyp)
{
	ct_entry *dbc_ctp;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	replyp->status = __dbc_close_int(dbc_ctp);
	return;
}

extern "C" void
__dbc_count_proc(
	long dbccl_id,
	u_int32_t flags,
	__dbc_count_reply *replyp)
{
	Dbc *dbc;
	ct_entry *dbc_ctp;
	db_recno_t num;
	int ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (Dbc *)dbc_ctp->ct_anyp;

	ret = dbc->count(&num, flags);
	replyp->status = ret;
	if (ret == 0)
		replyp->dupcount = num;
	return;
}

extern "C" void
__dbc_del_proc(
	long dbccl_id,
	u_int32_t flags,
	__dbc_del_reply *replyp)
{
	Dbc *dbc;
	ct_entry *dbc_ctp;
	int ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (Dbc *)dbc_ctp->ct_anyp;

	ret = dbc->del(flags);

	replyp->status = ret;
	return;
}

extern "C" void
__dbc_dup_proc(
	long dbccl_id,
	u_int32_t flags,
	__dbc_dup_reply *replyp)
{
	Dbc *dbc, *newdbc;
	ct_entry *dbc_ctp, *new_ctp;
	int ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (Dbc *)dbc_ctp->ct_anyp;

	new_ctp = new_ct_ent(&replyp->status);
	if (new_ctp == NULL)
		return;

	if ((ret = dbc->dup(&newdbc, flags)) == 0) {
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

extern "C" void
__dbc_get_proc(
	long dbccl_id,
	u_int32_t keydlen,
	u_int32_t keydoff,
	u_int32_t keyulen,
	u_int32_t keyflags,
	void *keydata,
	u_int32_t keysize,
	u_int32_t datadlen,
	u_int32_t datadoff,
	u_int32_t dataulen,
	u_int32_t dataflags,
	void *datadata,
	u_int32_t datasize,
	u_int32_t flags,
	__dbc_get_reply *replyp,
	int * freep)
{
	Dbc *dbc;
	DbEnv *dbenv;
	ct_entry *dbc_ctp;
	int key_alloc, bulk_alloc, ret;
	void *tmpdata;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (Dbc *)dbc_ctp->ct_anyp;
	dbenv = DbEnv::get_DbEnv(((DBC *)dbc)->dbp->dbenv);

	*freep = 0;
	bulk_alloc = 0;

	/* Set up key and data */
	Dbt key(keydata, keysize);
	key.set_dlen(keydlen);
	key.set_ulen(keyulen);
	key.set_doff(keydoff);
	key.set_flags(DB_DBT_MALLOC | (keyflags & DB_DBT_PARTIAL));

	Dbt data(datadata, datasize);
	data.set_dlen(datadlen);
	data.set_ulen(dataulen);
	data.set_doff(datadoff);
	dataflags &= DB_DBT_PARTIAL;
	if (flags & DB_MULTIPLE || flags & DB_MULTIPLE_KEY) {
		if (data.get_data() == NULL) {
			ret = __os_umalloc(dbenv->get_DB_ENV(),
			    data.get_ulen(), &tmpdata);
			if (ret != 0)
				goto err;
			data.set_data(tmpdata);
			bulk_alloc = 1;
		}
		dataflags |= DB_DBT_USERMEM;
	} else
		dataflags |= DB_DBT_MALLOC;
	data.set_flags(dataflags);

	/* Got all our stuff, now do the get */
	ret = dbc->get(&key, &data, flags);

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
		if (key.get_data() == keydata) {
			ret = __os_umalloc(dbenv->get_DB_ENV(), key.get_size(),
			    &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_ufree(dbenv->get_DB_ENV(), key.get_data());
				__os_ufree(
				    dbenv->get_DB_ENV(), data.get_data());
				goto err;
			}
			key_alloc = 1;
			memcpy(replyp->keydata.keydata_val,
			    key.get_data(), key.get_size());
		} else
			replyp->keydata.keydata_val = (char *)key.get_data();

		replyp->keydata.keydata_len = key.get_size();

		/*
		 * Data
		 */
		if (data.get_data() == datadata) {
			ret = __os_umalloc(dbenv->get_DB_ENV(), data.get_size(),
			    &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_ufree(dbenv->get_DB_ENV(), key.get_data());
				__os_ufree(
				    dbenv->get_DB_ENV(), data.get_data());
				if (key_alloc)
					__os_ufree(dbenv->get_DB_ENV(),
					    replyp->keydata.keydata_val);
				goto err;
			}
			memcpy(replyp->datadata.datadata_val, data.get_data(),
			    data.get_size());
		} else
			replyp->datadata.datadata_val = (char *)data.get_data();
		replyp->datadata.datadata_len = data.get_size();
	} else {
err:		replyp->keydata.keydata_val = NULL;
		replyp->keydata.keydata_len = 0;
		replyp->datadata.datadata_val = NULL;
		replyp->datadata.datadata_len = 0;
		*freep = 0;
		if (bulk_alloc)
			__os_ufree(dbenv->get_DB_ENV(), data.get_data());
	}
	replyp->status = ret;
	return;
}

extern "C" void
__dbc_pget_proc(
	long dbccl_id,
	u_int32_t skeydlen,
	u_int32_t skeydoff,
	u_int32_t skeyulen,
	u_int32_t skeyflags,
	void *skeydata,
	u_int32_t skeysize,
	u_int32_t pkeydlen,
	u_int32_t pkeydoff,
	u_int32_t pkeyulen,
	u_int32_t pkeyflags,
	void *pkeydata,
	u_int32_t pkeysize,
	u_int32_t datadlen,
	u_int32_t datadoff,
	u_int32_t dataulen,
	u_int32_t dataflags,
	void *datadata,
	u_int32_t datasize,
	u_int32_t flags,
	__dbc_pget_reply *replyp,
	int * freep)
{
	Dbc *dbc;
	DbEnv *dbenv;
	ct_entry *dbc_ctp;
	int key_alloc, ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (Dbc *)dbc_ctp->ct_anyp;
	dbenv = DbEnv::get_DbEnv(((DBC *)dbc)->dbp->dbenv);

	*freep = 0;

	/*
	 * Ignore memory related flags on server.
	 */
	/* Set up key and data */
	Dbt skey(skeydata, skeysize);
	skey.set_dlen(skeydlen);
	skey.set_ulen(skeyulen);
	skey.set_doff(skeydoff);
	skey.set_flags(DB_DBT_MALLOC | (skeyflags & DB_DBT_PARTIAL));

	Dbt pkey(pkeydata, pkeysize);
	pkey.set_dlen(pkeydlen);
	pkey.set_ulen(pkeyulen);
	pkey.set_doff(pkeydoff);
	pkey.set_flags(DB_DBT_MALLOC | (pkeyflags & DB_DBT_PARTIAL));

	Dbt data(datadata, datasize);
	data.set_dlen(datadlen);
	data.set_ulen(dataulen);
	data.set_doff(datadoff);
	data.set_flags(DB_DBT_MALLOC | (dataflags & DB_DBT_PARTIAL));

	/* Got all our stuff, now do the get */
	ret = dbc->pget(&skey, &pkey, &data, flags);
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
		if (skey.get_data() == skeydata) {
			ret = __os_umalloc(dbenv->get_DB_ENV(),
			    skey.get_size(), &replyp->skeydata.skeydata_val);
			if (ret != 0) {
				__os_ufree(
				    dbenv->get_DB_ENV(), skey.get_data());
				__os_ufree(
				    dbenv->get_DB_ENV(), pkey.get_data());
				__os_ufree(
				    dbenv->get_DB_ENV(), data.get_data());
				goto err;
			}
			key_alloc = 1;
			memcpy(replyp->skeydata.skeydata_val, skey.get_data(),
			    skey.get_size());
		} else
			replyp->skeydata.skeydata_val = (char *)skey.get_data();
		replyp->skeydata.skeydata_len = skey.get_size();

		/*
		 * Primary key
		 */
		if (pkey.get_data() == pkeydata) {
			ret = __os_umalloc(dbenv->get_DB_ENV(),
			     pkey.get_size(), &replyp->pkeydata.pkeydata_val);
			if (ret != 0) {
				__os_ufree(
				    dbenv->get_DB_ENV(), skey.get_data());
				__os_ufree(
				    dbenv->get_DB_ENV(), pkey.get_data());
				__os_ufree(
				    dbenv->get_DB_ENV(), data.get_data());
				if (key_alloc)
					__os_ufree(dbenv->get_DB_ENV(),
					    replyp->skeydata.skeydata_val);
				goto err;
			}
			/*
			 * We can set it to 2, because they cannot send the
			 * pkey over without sending the skey over too.
			 * So if they did send a pkey, they must have sent
			 * the skey as well.
			 */
			key_alloc = 2;
			memcpy(replyp->pkeydata.pkeydata_val, pkey.get_data(),
			    pkey.get_size());
		} else
			replyp->pkeydata.pkeydata_val = (char *)pkey.get_data();
		replyp->pkeydata.pkeydata_len = pkey.get_size();

		/*
		 * Data
		 */
		if (data.get_data() == datadata) {
			ret = __os_umalloc(dbenv->get_DB_ENV(),
			     data.get_size(), &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_ufree(
				    dbenv->get_DB_ENV(), skey.get_data());
				__os_ufree(
				    dbenv->get_DB_ENV(), pkey.get_data());
				__os_ufree(
				    dbenv->get_DB_ENV(), data.get_data());
				/*
				 * If key_alloc is 1, just skey needs to be
				 * freed, if key_alloc is 2, both skey and pkey
				 * need to be freed.
				 */
				if (key_alloc--)
					__os_ufree(dbenv->get_DB_ENV(),
					    replyp->skeydata.skeydata_val);
				if (key_alloc)
					__os_ufree(dbenv->get_DB_ENV(),
					    replyp->pkeydata.pkeydata_val);
				goto err;
			}
			memcpy(replyp->datadata.datadata_val, data.get_data(),
			    data.get_size());
		} else
			replyp->datadata.datadata_val = (char *)data.get_data();
		replyp->datadata.datadata_len = data.get_size();
	} else {
err:		replyp->skeydata.skeydata_val = NULL;
		replyp->skeydata.skeydata_len = 0;
		replyp->pkeydata.pkeydata_val = NULL;
		replyp->pkeydata.pkeydata_len = 0;
		replyp->datadata.datadata_val = NULL;
		replyp->datadata.datadata_len = 0;
		*freep = 0;
	}
	replyp->status = ret;
	return;
}

extern "C" void
__dbc_put_proc(
	long dbccl_id,
	u_int32_t keydlen,
	u_int32_t keydoff,
	u_int32_t keyulen,
	u_int32_t keyflags,
	void *keydata,
	u_int32_t keysize,
	u_int32_t datadlen,
	u_int32_t datadoff,
	u_int32_t dataulen,
	u_int32_t dataflags,
	void *datadata,
	u_int32_t datasize,
	u_int32_t flags,
	__dbc_put_reply *replyp,
	int * freep)
{
	Db *dbp;
	Dbc *dbc;
	ct_entry *dbc_ctp;
	int ret;
	DBTYPE dbtype;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (Dbc *)dbc_ctp->ct_anyp;
	dbp = (Db *)dbc_ctp->ct_parent->ct_anyp;

	/* Set up key and data */
	Dbt key(keydata, keysize);
	key.set_dlen(keydlen);
	key.set_ulen(keyulen);
	key.set_doff(keydoff);
	/*
	 * Ignore memory related flags on server.
	 */
	key.set_flags(DB_DBT_MALLOC | (keyflags & DB_DBT_PARTIAL));

	Dbt data(datadata, datasize);
	data.set_dlen(datadlen);
	data.set_ulen(dataulen);
	data.set_doff(datadoff);
	data.set_flags(dataflags);

	/* Got all our stuff, now do the put */
	ret = dbc->put(&key, &data, flags);

	*freep = 0;
	replyp->keydata.keydata_val = NULL;
	replyp->keydata.keydata_len = 0;
	if (ret == 0 && (flags == DB_AFTER || flags == DB_BEFORE)) {
		ret = dbp->get_type(&dbtype);
		if (ret == 0 && dbtype == DB_RECNO) {
			/*
			 * We need to xdr_free whatever we are returning, next
			 * time.
			 */
			replyp->keydata.keydata_val = (char *)key.get_data();
			replyp->keydata.keydata_len = key.get_size();
		}
	}
	replyp->status = ret;
	return;
}
