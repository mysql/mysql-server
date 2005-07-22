/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: db_server_proc.c,v 1.106 2004/09/22 17:30:12 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>

#include <string.h>
#endif

#include "db_server.h"

#include "db_int.h"
#include "dbinc/db_server_int.h"
#include "dbinc_auto/rpc_server_ext.h"

/*
 * PUBLIC: void __env_get_cachesize_proc __P((long,
 * PUBLIC:      __env_get_cachesize_reply *));
 */
void
__env_get_cachesize_proc(dbenvcl_id, replyp)
	long dbenvcl_id;
	__env_get_cachesize_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_cachesize(dbenv, &replyp->gbytes,
	    &replyp->bytes, (int *)&replyp->ncache);
}

/*
 * PUBLIC: void __env_cachesize_proc __P((long, u_int32_t, u_int32_t,
 * PUBLIC:      u_int32_t, __env_cachesize_reply *));
 */
void
__env_cachesize_proc(dbenvcl_id, gbytes, bytes, ncache, replyp)
	long dbenvcl_id;
	u_int32_t gbytes;
	u_int32_t bytes;
	u_int32_t ncache;
	__env_cachesize_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp;
	int ret;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	ret = dbenv->set_cachesize(dbenv, gbytes, bytes, ncache);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __env_close_proc __P((long, u_int32_t, __env_close_reply *));
 */
void
__env_close_proc(dbenvcl_id, flags, replyp)
	long dbenvcl_id;
	u_int32_t flags;
	__env_close_reply *replyp;
{
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	replyp->status = __dbenv_close_int(dbenvcl_id, flags, 0);
	return;
}

/*
 * PUBLIC: void __env_create_proc __P((u_int32_t, __env_create_reply *));
 */
void
__env_create_proc(timeout, replyp)
	u_int32_t timeout;
	__env_create_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *ctp;
	int ret;

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

/*
 * PUBLIC: void __env_dbremove_proc __P((long, long, char *, char *, u_int32_t,
 * PUBLIC:      __env_dbremove_reply *));
 */
void
__env_dbremove_proc(dbenvcl_id, txnpcl_id, name, subdb, flags, replyp)
	long dbenvcl_id;
	long txnpcl_id;
	char *name;
	char *subdb;
	u_int32_t flags;
	__env_dbremove_reply *replyp;
{
	int ret;
	DB_ENV * dbenv;
	ct_entry *dbenv_ctp;
	DB_TXN * txnp;
	ct_entry *txnp_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	ret = dbenv->dbremove(dbenv, txnp, name, subdb, flags);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __env_dbrename_proc __P((long, long, char *, char *, char *,
 * PUBLIC:      u_int32_t, __env_dbrename_reply *));
 */
void
__env_dbrename_proc(dbenvcl_id, txnpcl_id, name, subdb, newname, flags, replyp)
	long dbenvcl_id;
	long txnpcl_id;
	char *name;
	char *subdb;
	char *newname;
	u_int32_t flags;
	__env_dbrename_reply *replyp;
{
	int ret;
	DB_ENV * dbenv;
	ct_entry *dbenv_ctp;
	DB_TXN * txnp;
	ct_entry *txnp_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	ret = dbenv->dbrename(dbenv, txnp, name, subdb, newname, flags);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __env_get_encrypt_flags_proc __P((long,
 * PUBLIC:      __env_get_encrypt_flags_reply *));
 */
void
__env_get_encrypt_flags_proc(dbenvcl_id, replyp)
	long dbenvcl_id;
	__env_get_encrypt_flags_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_encrypt_flags(dbenv, &replyp->flags);
}

/*
 * PUBLIC: void __env_encrypt_proc __P((long, char *, u_int32_t,
 * PUBLIC:      __env_encrypt_reply *));
 */
void
__env_encrypt_proc(dbenvcl_id, passwd, flags, replyp)
	long dbenvcl_id;
	char *passwd;
	u_int32_t flags;
	__env_encrypt_reply *replyp;
{
	int ret;
	DB_ENV * dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	ret = dbenv->set_encrypt(dbenv, passwd, flags);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __env_get_flags_proc __P((long, __env_get_flags_reply *));
 */
void
__env_get_flags_proc(dbenvcl_id, replyp)
	long dbenvcl_id;
	__env_get_flags_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_flags(dbenv, &replyp->flags);
}

/*
 * PUBLIC: void __env_flags_proc __P((long, u_int32_t, u_int32_t,
 * PUBLIC:      __env_flags_reply *));
 */
void
__env_flags_proc(dbenvcl_id, flags, onoff, replyp)
	long dbenvcl_id;
	u_int32_t flags;
	u_int32_t onoff;
	__env_flags_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp;
	int ret;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	ret = dbenv->set_flags(dbenv, flags, onoff);
	if (onoff)
		dbenv_ctp->ct_envdp.onflags = flags;
	else
		dbenv_ctp->ct_envdp.offflags = flags;

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __env_get_home_proc __P((long, __env_get_home_reply *));
 */
void
__env_get_home_proc(dbenvcl_id, replyp)
	long dbenvcl_id;
	__env_get_home_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_home(dbenv,
	    (const char **)&replyp->home);
}

/*
 * PUBLIC: void __env_get_open_flags_proc __P((long,
 * PUBLIC:      __env_get_open_flags_reply *));
 */
void
__env_get_open_flags_proc(dbenvcl_id, replyp)
	long dbenvcl_id;
	__env_get_open_flags_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	replyp->status = dbenv->get_open_flags(dbenv, &replyp->flags);
}

/*
 * PUBLIC: void __env_open_proc __P((long, char *, u_int32_t, u_int32_t,
 * PUBLIC:      __env_open_reply *));
 */
void
__env_open_proc(dbenvcl_id, home, flags, mode, replyp)
	long dbenvcl_id;
	char *home;
	u_int32_t flags;
	u_int32_t mode;
	__env_open_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp, *new_ctp;
	u_int32_t newflags, shareflags;
	int ret;
	home_entry *fullhome;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;
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
	    (ret = dbenv->set_lk_detect(dbenv, DB_LOCK_DEFAULT)) != 0)
		goto out;

	if (__dbsrv_verbose) {
		dbenv->set_errfile(dbenv, stderr);
		dbenv->set_errpfx(dbenv, fullhome->home);
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
		ret = dbenv->open(dbenv, fullhome->home, newflags, mode);
		dbenv_ctp->ct_envdp.home = fullhome;
		dbenv_ctp->ct_envdp.envflags = shareflags;
	}
out:	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __env_remove_proc __P((long, char *, u_int32_t,
 * PUBLIC:      __env_remove_reply *));
 */
void
__env_remove_proc(dbenvcl_id, home, flags, replyp)
	long dbenvcl_id;
	char *home;
	u_int32_t flags;
	__env_remove_reply *replyp;
{
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp;
	int ret;
	home_entry *fullhome;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	fullhome = get_fullhome(home);
	if (fullhome == NULL) {
		replyp->status = DB_NOSERVER_HOME;
		return;
	}

	ret = dbenv->remove(dbenv, fullhome->home, flags);
	__dbdel_ctp(dbenv_ctp);
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __txn_abort_proc __P((long, __txn_abort_reply *));
 */
void
__txn_abort_proc(txnpcl_id, replyp)
	long txnpcl_id;
	__txn_abort_reply *replyp;
{
	DB_TXN *txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DB_TXN *)txnp_ctp->ct_anyp;

	ret = txnp->abort(txnp);
	__dbdel_ctp(txnp_ctp);
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __txn_begin_proc __P((long, long, u_int32_t,
 * PUBLIC:      __txn_begin_reply *));
 */
void
__txn_begin_proc(dbenvcl_id, parentcl_id, flags, replyp)
	long dbenvcl_id;
	long parentcl_id;
	u_int32_t flags;
	__txn_begin_reply *replyp;
{
	DB_ENV *dbenv;
	DB_TXN *parent, *txnp;
	ct_entry *ctp, *dbenv_ctp, *parent_ctp;
	int ret;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;
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

	/*
	 * Need to set DB_TXN_NOWAIT or the RPC server may deadlock
	 * itself and no one can break the lock.
	 */
	ret = dbenv->txn_begin(dbenv, parent, &txnp, flags | DB_TXN_NOWAIT);
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

/*
 * PUBLIC: void __txn_commit_proc __P((long, u_int32_t,
 * PUBLIC:      __txn_commit_reply *));
 */
void
__txn_commit_proc(txnpcl_id, flags, replyp)
	long txnpcl_id;
	u_int32_t flags;
	__txn_commit_reply *replyp;
{
	DB_TXN *txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DB_TXN *)txnp_ctp->ct_anyp;

	ret = txnp->commit(txnp, flags);
	__dbdel_ctp(txnp_ctp);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __txn_discard_proc __P((long, u_int32_t,
 * PUBLIC:      __txn_discard_reply *));
 */
void
__txn_discard_proc(txnpcl_id, flags, replyp)
	long txnpcl_id;
	u_int32_t flags;
	__txn_discard_reply *replyp;
{
	DB_TXN *txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DB_TXN *)txnp_ctp->ct_anyp;

	ret = txnp->discard(txnp, flags);
	__dbdel_ctp(txnp_ctp);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __txn_prepare_proc __P((long, u_int8_t *,
 * PUBLIC:      __txn_prepare_reply *));
 */
void
__txn_prepare_proc(txnpcl_id, gid, replyp)
	long txnpcl_id;
	u_int8_t *gid;
	__txn_prepare_reply *replyp;
{
	DB_TXN *txnp;
	ct_entry *txnp_ctp;
	int ret;

	ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
	txnp = (DB_TXN *)txnp_ctp->ct_anyp;

	ret = txnp->prepare(txnp, gid);
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __txn_recover_proc __P((long, u_int32_t, u_int32_t,
 * PUBLIC:      __txn_recover_reply *, int *));
 */
void
__txn_recover_proc(dbenvcl_id, count, flags, replyp, freep)
	long dbenvcl_id;
	u_int32_t count;
	u_int32_t flags;
	__txn_recover_reply *replyp;
	int * freep;
{
	DB_ENV *dbenv;
	DB_PREPLIST *dbprep, *p;
	ct_entry *dbenv_ctp, *ctp;
	long erri, i, retcount;
	u_int32_t *txnidp;
	int ret;
	u_int8_t *gid;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;
	dbprep = NULL;
	*freep = 0;

	if ((ret =
	    __os_malloc(dbenv, count * sizeof(DB_PREPLIST), &dbprep)) != 0)
		goto out;
	if ((ret =
	    dbenv->txn_recover(dbenv, dbprep, count, &retcount, flags)) != 0)
		goto out;
	/*
	 * If there is nothing, success, but it's easy.
	 */
	replyp->retcount = retcount;
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
	if ((ret = __os_calloc(dbenv, retcount, sizeof(u_int32_t),
	    &replyp->txn.txn_val)) != 0)
		goto out;
	replyp->txn.txn_len = retcount * sizeof(u_int32_t);
	if ((ret = __os_calloc(dbenv, retcount, DB_XIDDATASIZE,
	    &replyp->gid.gid_val)) != 0) {
		__os_free(dbenv, replyp->txn.txn_val);
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
		__os_free(dbenv, dbprep);
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
	__os_free(dbenv, replyp->txn.txn_val);
	__os_free(dbenv, replyp->gid.gid_val);
	__os_free(dbenv, dbprep);
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_bt_maxkey_proc __P((long, u_int32_t,
 * PUBLIC:      __db_bt_maxkey_reply *));
 */
void
__db_bt_maxkey_proc(dbpcl_id, maxkey, replyp)
	long dbpcl_id;
	u_int32_t maxkey;
	__db_bt_maxkey_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_bt_maxkey(dbp, maxkey);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_associate_proc __P((long, long, long, u_int32_t,
 * PUBLIC:      __db_associate_reply *));
 */
void
__db_associate_proc(dbpcl_id, txnpcl_id, sdbpcl_id, flags, replyp)
	long dbpcl_id;
	long txnpcl_id;
	long sdbpcl_id;
	u_int32_t flags;
	__db_associate_reply *replyp;
{
	DB *dbp, *sdbp;
	DB_TXN *txnp;
	ct_entry *dbp_ctp, *sdbp_ctp, *txnp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	ACTIVATE_CTP(sdbp_ctp, sdbpcl_id, CT_DB);
	sdbp = (DB *)sdbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	/*
	 * We do not support DB_CREATE for associate or the callbacks
	 * implemented in the Java and JE RPC servers.   Users can only
	 * access secondary indices on a read-only basis, so whatever they
	 * are looking for needs to be there already.
	 */
#ifdef CONFIG_TEST
	if (LF_ISSET(DB_RPC2ND_MASK | DB_CREATE))
#else
	if (LF_ISSET(DB_CREATE))
#endif
		ret = EINVAL;
	else
		ret = dbp->associate(dbp, txnp, sdbp, NULL, flags);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_bt_minkey_proc __P((long,
 * PUBLIC:      __db_get_bt_minkey_reply *));
 */
void
__db_get_bt_minkey_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_bt_minkey_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_bt_minkey(dbp, &replyp->minkey);
}

/*
 * PUBLIC: void __db_bt_minkey_proc __P((long, u_int32_t,
 * PUBLIC:      __db_bt_minkey_reply *));
 */
void
__db_bt_minkey_proc(dbpcl_id, minkey, replyp)
	long dbpcl_id;
	u_int32_t minkey;
	__db_bt_minkey_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_bt_minkey(dbp, minkey);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_close_proc __P((long, u_int32_t, __db_close_reply *));
 */
void
__db_close_proc(dbpcl_id, flags, replyp)
	long dbpcl_id;
	u_int32_t flags;
	__db_close_reply *replyp;
{
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	replyp->status = __db_close_int(dbpcl_id, flags);
	return;
}

/*
 * PUBLIC: void __db_create_proc __P((long, u_int32_t, __db_create_reply *));
 */
void
__db_create_proc(dbenvcl_id, flags, replyp)
	long dbenvcl_id;
	u_int32_t flags;
	__db_create_reply *replyp;
{
	DB *dbp;
	DB_ENV *dbenv;
	ct_entry *dbenv_ctp, *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbenv_ctp, dbenvcl_id, CT_ENV);
	dbenv = (DB_ENV *)dbenv_ctp->ct_anyp;

	dbp_ctp = new_ct_ent(&replyp->status);
	if (dbp_ctp == NULL)
		return ;
	/*
	 * We actually require env's for databases.  The client should
	 * have caught it, but just in case.
	 */
	DB_ASSERT(dbenv != NULL);
	if ((ret = db_create(&dbp, dbenv, flags)) == 0) {
		dbp_ctp->ct_dbp = dbp;
		dbp_ctp->ct_type = CT_DB;
		dbp_ctp->ct_parent = dbenv_ctp;
		dbp_ctp->ct_envparent = dbenv_ctp;
		replyp->dbcl_id = dbp_ctp->ct_id;
	} else
		__dbclear_ctp(dbp_ctp);
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_del_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:      u_int32_t, void *, u_int32_t, u_int32_t, __db_del_reply *));
 */
void
__db_del_proc(dbpcl_id, txnpcl_id, keydlen, keydoff, keyulen, keyflags,
    keydata, keysize, flags, replyp)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyulen;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t flags;
	__db_del_reply *replyp;
{
	DB *dbp;
	DBT key;
	DB_TXN *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int ret;

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
	key.ulen = keyulen;
	key.doff = keydoff;
	key.flags = keyflags;
	key.size = keysize;
	key.data = keydata;

	ret = dbp->del(dbp, txnp, &key, flags);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_encrypt_flags_proc __P((long,
 * PUBLIC:      __db_get_encrypt_flags_reply *));
 */
void
__db_get_encrypt_flags_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_encrypt_flags_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_encrypt_flags(dbp, &replyp->flags);
}

/*
 * PUBLIC: void __db_encrypt_proc __P((long, char *, u_int32_t,
 * PUBLIC:      __db_encrypt_reply *));
 */
void
__db_encrypt_proc(dbpcl_id, passwd, flags, replyp)
	long dbpcl_id;
	char *passwd;
	u_int32_t flags;
	__db_encrypt_reply *replyp;
{
	int ret;
	DB * dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_encrypt(dbp, passwd, flags);
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_extentsize_proc __P((long,
 * PUBLIC:      __db_get_extentsize_reply *));
 */
void
__db_get_extentsize_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_extentsize_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_q_extentsize(dbp, &replyp->extentsize);
}

/*
 * PUBLIC: void __db_extentsize_proc __P((long, u_int32_t,
 * PUBLIC:      __db_extentsize_reply *));
 */
void
__db_extentsize_proc(dbpcl_id, extentsize, replyp)
	long dbpcl_id;
	u_int32_t extentsize;
	__db_extentsize_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_q_extentsize(dbp, extentsize);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_flags_proc __P((long, __db_get_flags_reply *));
 */
void
__db_get_flags_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_flags_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_flags(dbp, &replyp->flags);
}

/*
 * PUBLIC: void __db_flags_proc __P((long, u_int32_t, __db_flags_reply *));
 */
void
__db_flags_proc(dbpcl_id, flags, replyp)
	long dbpcl_id;
	u_int32_t flags;
	__db_flags_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_flags(dbp, flags);
	dbp_ctp->ct_dbdp.setflags |= flags;

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, __db_get_reply *,
 * PUBLIC:     int *));
 */
void
__db_get_proc(dbpcl_id, txnpcl_id, keydlen, keydoff, keyulen, keyflags,
    keydata, keysize, datadlen, datadoff, dataulen, dataflags, datadata,
    datasize, flags, replyp, freep)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyulen;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataulen;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__db_get_reply *replyp;
	int * freep;
{
	DB *dbp;
	DBT key, data;
	DB_TXN *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int key_alloc, bulk_alloc, ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	*freep = 0;
	bulk_alloc = 0;
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
	key.ulen = keyulen;
	key.data = keydata;

	data.dlen = datadlen;
	data.doff = datadoff;
	data.ulen = dataulen;
	/*
	 * Ignore memory related flags on server.
	 */
	data.size = datasize;
	data.data = datadata;
	if (flags & DB_MULTIPLE) {
		if (data.data == 0) {
			ret = __os_umalloc(dbp->dbenv,
			    data.ulen, &data.data);
			if (ret != 0)
				goto err;
			bulk_alloc = 1;
		}
		data.flags |= DB_DBT_USERMEM;
	} else
		data.flags |= DB_DBT_MALLOC;
	if (dataflags & DB_DBT_PARTIAL)
		data.flags |= DB_DBT_PARTIAL;

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
			ret = __os_umalloc(dbp->dbenv,
			    key.size, &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_ufree(dbp->dbenv, key.data);
				__os_ufree(dbp->dbenv, data.data);
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
			ret = __os_umalloc(dbp->dbenv,
			     data.size, &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_ufree(dbp->dbenv, key.data);
				__os_ufree(dbp->dbenv, data.data);
				if (key_alloc)
					__os_ufree(dbp->dbenv,
					    replyp->keydata.keydata_val);
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
		if (bulk_alloc)
			__os_ufree(dbp->dbenv, data.data);
	}
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_h_ffactor_proc __P((long,
 * PUBLIC:      __db_get_h_ffactor_reply *));
 */
void
__db_get_h_ffactor_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_h_ffactor_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_h_ffactor(dbp, &replyp->ffactor);
}

/*
 * PUBLIC: void __db_h_ffactor_proc __P((long, u_int32_t,
 * PUBLIC:      __db_h_ffactor_reply *));
 */
void
__db_h_ffactor_proc(dbpcl_id, ffactor, replyp)
	long dbpcl_id;
	u_int32_t ffactor;
	__db_h_ffactor_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_h_ffactor(dbp, ffactor);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_h_nelem_proc __P((long, __db_get_h_nelem_reply *));
 */
void
__db_get_h_nelem_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_h_nelem_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_h_nelem(dbp, &replyp->nelem);
}

/*
 * PUBLIC: void __db_h_nelem_proc __P((long, u_int32_t,
 * PUBLIC:      __db_h_nelem_reply *));
 */
void
__db_h_nelem_proc(dbpcl_id, nelem, replyp)
	long dbpcl_id;
	u_int32_t nelem;
	__db_h_nelem_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_h_nelem(dbp, nelem);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_key_range_proc __P((long, long, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t,
 * PUBLIC:     __db_key_range_reply *));
 */
void
__db_key_range_proc(dbpcl_id, txnpcl_id, keydlen, keydoff, keyulen,
    keyflags, keydata, keysize, flags, replyp)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyulen;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t flags;
	__db_key_range_reply *replyp;
{
	DB *dbp;
	DBT key;
	DB_KEY_RANGE range;
	DB_TXN *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int ret;

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
	key.ulen = keyulen;
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

/*
 * PUBLIC: void __db_get_lorder_proc __P((long, __db_get_lorder_reply *));
 */
void
__db_get_lorder_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_lorder_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_lorder(dbp, (int *)&replyp->lorder);
}

/*
 * PUBLIC: void __db_lorder_proc __P((long, u_int32_t, __db_lorder_reply *));
 */
void
__db_lorder_proc(dbpcl_id, lorder, replyp)
	long dbpcl_id;
	u_int32_t lorder;
	__db_lorder_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_lorder(dbp, lorder);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_name_proc __P((long, __db_get_name_reply *));
 */
void
__db_get_name_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_name_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_dbname(dbp,
	    (const char **)&replyp->filename, (const char **)&replyp->dbname);
}

/*
 * PUBLIC: void __db_get_open_flags_proc __P((long,
 * PUBLIC:      __db_get_open_flags_reply *));
 */
void
__db_get_open_flags_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_open_flags_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_open_flags(dbp, &replyp->flags);
}

/*
 * PUBLIC: void __db_open_proc __P((long, long, char *, char *, u_int32_t,
 * PUBLIC:      u_int32_t, u_int32_t, __db_open_reply *));
 */
void
__db_open_proc(dbpcl_id, txnpcl_id, name, subdb, type, flags, mode, replyp)
	long dbpcl_id;
	long txnpcl_id;
	char *name;
	char *subdb;
	u_int32_t type;
	u_int32_t flags;
	u_int32_t mode;
	__db_open_reply *replyp;
{
	DB *dbp;
	DB_TXN *txnp;
	DBTYPE dbtype;
	ct_entry *dbp_ctp, *new_ctp, *txnp_ctp;
	int isswapped, ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	replyp->dbcl_id = dbpcl_id;
	if ((new_ctp = __dbsrv_sharedb(dbp_ctp, name, subdb, type, flags))
	    != NULL) {
		/*
		 * We can share, clean up old ID, set new one.
		 */
		if (__dbsrv_verbose)
			printf("Sharing db ID %ld\n", new_ctp->ct_id);
		replyp->dbcl_id = new_ctp->ct_id;
		ret = __db_close_int(dbpcl_id, 0);
		goto out;
	}
	ret = dbp->open(dbp, txnp, name, subdb, (DBTYPE)type, flags, mode);
	if (ret == 0) {
		(void)dbp->get_type(dbp, &dbtype);
		replyp->type = dbtype;
		/*
		 * We need to determine the byte order of the database
		 * and send it back to the client.  Determine it by
		 * the server's native order and the swapped value of
		 * the DB itself.
		 */
		(void)dbp->get_byteswapped(dbp, &isswapped);
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
		else if ((ret = __os_strdup(dbp->dbenv, name,
		    &dbp_ctp->ct_dbdp.db)) != 0)
			goto out;
		if (subdb == NULL)
			dbp_ctp->ct_dbdp.subdb = NULL;
		else if ((ret = __os_strdup(dbp->dbenv, subdb,
		    &dbp_ctp->ct_dbdp.subdb)) != 0)
			goto out;
	}
out:
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_pagesize_proc __P((long, __db_get_pagesize_reply *));
 */
void
__db_get_pagesize_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_pagesize_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_pagesize(dbp, &replyp->pagesize);
}

/*
 * PUBLIC: void __db_pagesize_proc __P((long, u_int32_t,
 * PUBLIC:      __db_pagesize_reply *));
 */
void
__db_pagesize_proc(dbpcl_id, pagesize, replyp)
	long dbpcl_id;
	u_int32_t pagesize;
	__db_pagesize_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_pagesize(dbp, pagesize);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_pget_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, __db_pget_reply *,
 * PUBLIC:     int *));
 */
void
__db_pget_proc(dbpcl_id, txnpcl_id, skeydlen, skeydoff, skeyulen,
    skeyflags, skeydata, skeysize, pkeydlen, pkeydoff, pkeyulen, pkeyflags,
    pkeydata, pkeysize, datadlen, datadoff, dataulen, dataflags, datadata,
    datasize, flags, replyp, freep)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t skeydlen;
	u_int32_t skeydoff;
	u_int32_t skeyulen;
	u_int32_t skeyflags;
	void *skeydata;
	u_int32_t skeysize;
	u_int32_t pkeydlen;
	u_int32_t pkeydoff;
	u_int32_t pkeyulen;
	u_int32_t pkeyflags;
	void *pkeydata;
	u_int32_t pkeysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataulen;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__db_pget_reply *replyp;
	int * freep;
{
	DB *dbp;
	DBT skey, pkey, data;
	DB_TXN *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int key_alloc, ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	*freep = 0;
	memset(&skey, 0, sizeof(skey));
	memset(&pkey, 0, sizeof(pkey));
	memset(&data, 0, sizeof(data));

	/*
	 * Ignore memory related flags on server.
	 */
	/* Set up key and data DBT */
	skey.flags = DB_DBT_MALLOC;
	skey.dlen = skeydlen;
	skey.ulen = skeyulen;
	skey.doff = skeydoff;
	if (skeyflags & DB_DBT_PARTIAL)
		skey.flags |= DB_DBT_PARTIAL;
	skey.size = skeysize;
	skey.data = skeydata;

	pkey.flags = DB_DBT_MALLOC;
	pkey.dlen = pkeydlen;
	pkey.ulen = pkeyulen;
	pkey.doff = pkeydoff;
	if (pkeyflags & DB_DBT_PARTIAL)
		pkey.flags |= DB_DBT_PARTIAL;
	pkey.size = pkeysize;
	pkey.data = pkeydata;

	data.flags = DB_DBT_MALLOC;
	data.dlen = datadlen;
	data.ulen = dataulen;
	data.doff = datadoff;
	if (dataflags & DB_DBT_PARTIAL)
		data.flags |= DB_DBT_PARTIAL;
	data.size = datasize;
	data.data = datadata;

	/* Got all our stuff, now do the get */
	ret = dbp->pget(dbp, txnp, &skey, &pkey, &data, flags);
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
		if (skey.data == skeydata) {
			ret = __os_umalloc(dbp->dbenv,
			    skey.size, &replyp->skeydata.skeydata_val);
			if (ret != 0) {
				__os_ufree(dbp->dbenv, skey.data);
				__os_ufree(dbp->dbenv, pkey.data);
				__os_ufree(dbp->dbenv, data.data);
				goto err;
			}
			key_alloc = 1;
			memcpy(replyp->skeydata.skeydata_val, skey.data,
			    skey.size);
		} else
			replyp->skeydata.skeydata_val = skey.data;

		replyp->skeydata.skeydata_len = skey.size;

		/*
		 * Primary key
		 */
		if (pkey.data == pkeydata) {
			ret = __os_umalloc(dbp->dbenv,
			     pkey.size, &replyp->pkeydata.pkeydata_val);
			if (ret != 0) {
				__os_ufree(dbp->dbenv, skey.data);
				__os_ufree(dbp->dbenv, pkey.data);
				__os_ufree(dbp->dbenv, data.data);
				if (key_alloc)
					__os_ufree(dbp->dbenv,
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
			memcpy(replyp->pkeydata.pkeydata_val, pkey.data,
			    pkey.size);
		} else
			replyp->pkeydata.pkeydata_val = pkey.data;
		replyp->pkeydata.pkeydata_len = pkey.size;

		/*
		 * Data
		 */
		if (data.data == datadata) {
			ret = __os_umalloc(dbp->dbenv,
			     data.size, &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_ufree(dbp->dbenv, skey.data);
				__os_ufree(dbp->dbenv, pkey.data);
				__os_ufree(dbp->dbenv, data.data);
				/*
				 * If key_alloc is 1, just skey needs to be
				 * freed, if key_alloc is 2, both skey and pkey
				 * need to be freed.
				 */
				if (key_alloc--)
					__os_ufree(dbp->dbenv,
					    replyp->skeydata.skeydata_val);
				if (key_alloc)
					__os_ufree(dbp->dbenv,
					    replyp->pkeydata.pkeydata_val);
				goto err;
			}
			memcpy(replyp->datadata.datadata_val, data.data,
			    data.size);
		} else
			replyp->datadata.datadata_val = data.data;
		replyp->datadata.datadata_len = data.size;
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

/*
 * PUBLIC: void __db_put_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, __db_put_reply *,
 * PUBLIC:     int *));
 */
void
__db_put_proc(dbpcl_id, txnpcl_id, keydlen, keydoff, keyulen, keyflags,
    keydata, keysize, datadlen, datadoff, dataulen, dataflags, datadata,
    datasize, flags, replyp, freep)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyulen;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataulen;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__db_put_reply *replyp;
	int * freep;
{
	DB *dbp;
	DBT key, data;
	DB_TXN *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	int ret;

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
	key.ulen = keyulen;
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
	data.ulen = dataulen;
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
			ret = __os_umalloc(dbp->dbenv,
			    key.size, &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_ufree(dbp->dbenv, key.data);
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

/*
 * PUBLIC: void __db_get_re_delim_proc __P((long, __db_get_re_delim_reply *));
 */
void
__db_get_re_delim_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_re_delim_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_re_delim(dbp, (int *)&replyp->delim);
}

/*
 * PUBLIC: void __db_re_delim_proc __P((long, u_int32_t,
 * PUBLIC:      __db_re_delim_reply *));
 */
void
__db_re_delim_proc(dbpcl_id, delim, replyp)
	long dbpcl_id;
	u_int32_t delim;
	__db_re_delim_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_delim(dbp, delim);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_re_len_proc __P((long, __db_get_re_len_reply *));
 */
void
__db_get_re_len_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_re_len_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_re_len(dbp, &replyp->len);
}

/*
 * PUBLIC: void __db_re_len_proc __P((long, u_int32_t, __db_re_len_reply *));
 */
void
__db_re_len_proc(dbpcl_id, len, replyp)
	long dbpcl_id;
	u_int32_t len;
	__db_re_len_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_len(dbp, len);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_get_re_pad_proc __P((long, __db_get_re_pad_reply *));
 */
void
__db_get_re_pad_proc(dbpcl_id, replyp)
	long dbpcl_id;
	__db_get_re_pad_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	replyp->status = dbp->get_re_pad(dbp, (int *)&replyp->pad);
}

/*
 * PUBLIC: void __db_re_pad_proc __P((long, u_int32_t, __db_re_pad_reply *));
 */
void
__db_re_pad_proc(dbpcl_id, pad, replyp)
	long dbpcl_id;
	u_int32_t pad;
	__db_re_pad_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->set_re_pad(dbp, pad);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_remove_proc __P((long, char *, char *, u_int32_t,
 * PUBLIC:      __db_remove_reply *));
 */
void
__db_remove_proc(dbpcl_id, name, subdb, flags, replyp)
	long dbpcl_id;
	char *name;
	char *subdb;
	u_int32_t flags;
	__db_remove_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->remove(dbp, name, subdb, flags);
	__dbdel_ctp(dbp_ctp);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_rename_proc __P((long, char *, char *, char *, u_int32_t,
 * PUBLIC:      __db_rename_reply *));
 */
void
__db_rename_proc(dbpcl_id, name, subdb, newname, flags, replyp)
	long dbpcl_id;
	char *name;
	char *subdb;
	char *newname;
	u_int32_t flags;
	__db_rename_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->rename(dbp, name, subdb, newname, flags);
	__dbdel_ctp(dbp_ctp);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_stat_proc __P((long, long, u_int32_t, __db_stat_reply *,
 * PUBLIC:      int *));
 */
void
__db_stat_proc(dbpcl_id, txnpcl_id, flags, replyp, freep)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t flags;
	__db_stat_reply *replyp;
	int * freep;
{
	DB *dbp;
	DB_TXN *txnp;
	DBTYPE type;
	ct_entry *dbp_ctp, *txnp_ctp;
	u_int32_t *q, *p, *retsp;
	int i, len, ret;
	void *sp;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	ret = dbp->stat(dbp, txnp, &sp, flags);
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
	(void)dbp->get_type(dbp, &type);
	if (type == DB_HASH)
		len = sizeof(DB_HASH_STAT);
	else if (type == DB_QUEUE)
		len = sizeof(DB_QUEUE_STAT);
	else            /* BTREE or RECNO are same stats */
		len = sizeof(DB_BTREE_STAT);
	replyp->stats.stats_len = len / sizeof(u_int32_t);

	if ((ret = __os_umalloc(dbp->dbenv, len * replyp->stats.stats_len,
	    &retsp)) != 0)
		goto out;
	for (i = 0, q = retsp, p = sp; i < len;
	    i++, q++, p++)
		*q = *p;
	replyp->stats.stats_val = retsp;
	__os_ufree(dbp->dbenv, sp);
	if (ret == 0)
		*freep = 1;
out:
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_sync_proc __P((long, u_int32_t, __db_sync_reply *));
 */
void
__db_sync_proc(dbpcl_id, flags, replyp)
	long dbpcl_id;
	u_int32_t flags;
	__db_sync_reply *replyp;
{
	DB *dbp;
	ct_entry *dbp_ctp;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	ret = dbp->sync(dbp, flags);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __db_truncate_proc __P((long, long, u_int32_t,
 * PUBLIC:      __db_truncate_reply *));
 */
void
__db_truncate_proc(dbpcl_id, txnpcl_id, flags, replyp)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t flags;
	__db_truncate_reply *replyp;
{
	DB *dbp;
	DB_TXN *txnp;
	ct_entry *dbp_ctp, *txnp_ctp;
	u_int32_t count;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;
	if (txnpcl_id != 0) {
		ACTIVATE_CTP(txnp_ctp, txnpcl_id, CT_TXN);
		txnp = (DB_TXN *)txnp_ctp->ct_anyp;
	} else
		txnp = NULL;

	ret = dbp->truncate(dbp, txnp, &count, flags);
	replyp->status = ret;
	if (ret == 0)
		replyp->count = count;
	return;
}

/*
 * PUBLIC: void __db_cursor_proc __P((long, long, u_int32_t,
 * PUBLIC:      __db_cursor_reply *));
 */
void
__db_cursor_proc(dbpcl_id, txnpcl_id, flags, replyp)
	long dbpcl_id;
	long txnpcl_id;
	u_int32_t flags;
	__db_cursor_reply *replyp;
{
	DB *dbp;
	DBC *dbc;
	DB_TXN *txnp;
	ct_entry *dbc_ctp, *env_ctp, *dbp_ctp, *txnp_ctp;
	int ret;

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

/*
 * PUBLIC: void __db_join_proc __P((long, u_int32_t *, u_int32_t, u_int32_t,
 * PUBLIC:      __db_join_reply *));
 */
void
__db_join_proc(dbpcl_id, curs, curslen, flags, replyp)
	long dbpcl_id;
	u_int32_t * curs;
	u_int32_t curslen;
	u_int32_t flags;
	__db_join_reply *replyp;
{
	DB *dbp;
	DBC **jcurs, **c;
	DBC *dbc;
	ct_entry *dbc_ctp, *ctp, *dbp_ctp;
	size_t size;
	u_int32_t *cl, i;
	int ret;

	ACTIVATE_CTP(dbp_ctp, dbpcl_id, CT_DB);
	dbp = (DB *)dbp_ctp->ct_anyp;

	dbc_ctp = new_ct_ent(&replyp->status);
	if (dbc_ctp == NULL)
		return;

	size = (curslen + 1) * sizeof(DBC *);
	if ((ret = __os_calloc(dbp->dbenv,
	    curslen + 1, sizeof(DBC *), &jcurs)) != 0) {
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
		for (cl = curs; *cl != 0; cl++) {
			ctp = get_tableent(*cl);
			ctp->ct_type = CT_CURSOR;
			ctp->ct_activep = ctp->ct_origp;
		}
	}

	replyp->status = ret;
out:
	__os_free(dbp->dbenv, jcurs);
	return;
}

/*
 * PUBLIC: void __dbc_close_proc __P((long, __dbc_close_reply *));
 */
void
__dbc_close_proc(dbccl_id, replyp)
	long dbccl_id;
	__dbc_close_reply *replyp;
{
	ct_entry *dbc_ctp;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	replyp->status = __dbc_close_int(dbc_ctp);
	return;
}

/*
 * PUBLIC: void __dbc_count_proc __P((long, u_int32_t, __dbc_count_reply *));
 */
void
__dbc_count_proc(dbccl_id, flags, replyp)
	long dbccl_id;
	u_int32_t flags;
	__dbc_count_reply *replyp;
{
	DBC *dbc;
	ct_entry *dbc_ctp;
	db_recno_t num;
	int ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;

	ret = dbc->c_count(dbc, &num, flags);
	replyp->status = ret;
	if (ret == 0)
		replyp->dupcount = num;
	return;
}

/*
 * PUBLIC: void __dbc_del_proc __P((long, u_int32_t, __dbc_del_reply *));
 */
void
__dbc_del_proc(dbccl_id, flags, replyp)
	long dbccl_id;
	u_int32_t flags;
	__dbc_del_reply *replyp;
{
	DBC *dbc;
	ct_entry *dbc_ctp;
	int ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;

	ret = dbc->c_del(dbc, flags);

	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __dbc_dup_proc __P((long, u_int32_t, __dbc_dup_reply *));
 */
void
__dbc_dup_proc(dbccl_id, flags, replyp)
	long dbccl_id;
	u_int32_t flags;
	__dbc_dup_reply *replyp;
{
	DBC *dbc, *newdbc;
	ct_entry *dbc_ctp, *new_ctp;
	int ret;

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

/*
 * PUBLIC: void __dbc_get_proc __P((long, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, __dbc_get_reply *,
 * PUBLIC:     int *));
 */
void
__dbc_get_proc(dbccl_id, keydlen, keydoff, keyulen, keyflags, keydata,
    keysize, datadlen, datadoff, dataulen, dataflags, datadata, datasize,
    flags, replyp, freep)
	long dbccl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyulen;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataulen;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__dbc_get_reply *replyp;
	int * freep;
{
	DBC *dbc;
	DBT key, data;
	DB_ENV *dbenv;
	ct_entry *dbc_ctp;
	int key_alloc, bulk_alloc, ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;
	dbenv = dbc->dbp->dbenv;

	*freep = 0;
	bulk_alloc = 0;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Set up key and data DBT */
	key.dlen = keydlen;
	key.ulen = keyulen;
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
	data.ulen = dataulen;
	data.doff = datadoff;
	data.size = datasize;
	data.data = datadata;
	if (flags & DB_MULTIPLE || flags & DB_MULTIPLE_KEY) {
		if (data.data == 0) {
			ret = __os_umalloc(dbenv, data.ulen, &data.data);
			if (ret != 0)
				goto err;
			bulk_alloc = 1;
		}
		data.flags |= DB_DBT_USERMEM;
	} else
		data.flags |= DB_DBT_MALLOC;
	if (dataflags & DB_DBT_PARTIAL)
		data.flags |= DB_DBT_PARTIAL;

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
			ret = __os_umalloc(dbenv, key.size,
			    &replyp->keydata.keydata_val);
			if (ret != 0) {
				__os_ufree(dbenv, key.data);
				__os_ufree(dbenv, data.data);
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
			ret = __os_umalloc(dbenv, data.size,
			    &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_ufree(dbenv, key.data);
				__os_ufree(dbenv, data.data);
				if (key_alloc)
					__os_ufree(
					    dbenv, replyp->keydata.keydata_val);
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
		if (bulk_alloc)
			__os_ufree(dbenv, data.data);
	}
	replyp->status = ret;
	return;
}

/*
 * PUBLIC: void __dbc_pget_proc __P((long, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, __dbc_pget_reply *,
 * PUBLIC:     int *));
 */
void
__dbc_pget_proc(dbccl_id, skeydlen, skeydoff, skeyulen, skeyflags,
    skeydata, skeysize, pkeydlen, pkeydoff, pkeyulen, pkeyflags, pkeydata,
    pkeysize, datadlen, datadoff, dataulen, dataflags, datadata, datasize,
    flags, replyp, freep)
	long dbccl_id;
	u_int32_t skeydlen;
	u_int32_t skeydoff;
	u_int32_t skeyulen;
	u_int32_t skeyflags;
	void *skeydata;
	u_int32_t skeysize;
	u_int32_t pkeydlen;
	u_int32_t pkeydoff;
	u_int32_t pkeyulen;
	u_int32_t pkeyflags;
	void *pkeydata;
	u_int32_t pkeysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataulen;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__dbc_pget_reply *replyp;
	int * freep;
{
	DBC *dbc;
	DBT skey, pkey, data;
	DB_ENV *dbenv;
	ct_entry *dbc_ctp;
	int key_alloc, ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;
	dbenv = dbc->dbp->dbenv;

	*freep = 0;
	memset(&skey, 0, sizeof(skey));
	memset(&pkey, 0, sizeof(pkey));
	memset(&data, 0, sizeof(data));

	/*
	 * Ignore memory related flags on server.
	 */
	/* Set up key and data DBT */
	skey.flags = DB_DBT_MALLOC;
	skey.dlen = skeydlen;
	skey.ulen = skeyulen;
	skey.doff = skeydoff;
	if (skeyflags & DB_DBT_PARTIAL)
		skey.flags |= DB_DBT_PARTIAL;
	skey.size = skeysize;
	skey.data = skeydata;

	pkey.flags = DB_DBT_MALLOC;
	pkey.dlen = pkeydlen;
	pkey.ulen = pkeyulen;
	pkey.doff = pkeydoff;
	if (pkeyflags & DB_DBT_PARTIAL)
		pkey.flags |= DB_DBT_PARTIAL;
	pkey.size = pkeysize;
	pkey.data = pkeydata;

	data.flags = DB_DBT_MALLOC;
	data.dlen = datadlen;
	data.ulen = dataulen;
	data.doff = datadoff;
	if (dataflags & DB_DBT_PARTIAL)
		data.flags |= DB_DBT_PARTIAL;
	data.size = datasize;
	data.data = datadata;

	/* Got all our stuff, now do the get */
	ret = dbc->c_pget(dbc, &skey, &pkey, &data, flags);
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
		if (skey.data == skeydata) {
			ret = __os_umalloc(dbenv,
			    skey.size, &replyp->skeydata.skeydata_val);
			if (ret != 0) {
				__os_ufree(dbenv, skey.data);
				__os_ufree(dbenv, pkey.data);
				__os_ufree(dbenv, data.data);
				goto err;
			}
			key_alloc = 1;
			memcpy(replyp->skeydata.skeydata_val, skey.data,
			    skey.size);
		} else
			replyp->skeydata.skeydata_val = skey.data;
		replyp->skeydata.skeydata_len = skey.size;

		/*
		 * Primary key
		 */
		if (pkey.data == pkeydata) {
			ret = __os_umalloc(dbenv,
			     pkey.size, &replyp->pkeydata.pkeydata_val);
			if (ret != 0) {
				__os_ufree(dbenv, skey.data);
				__os_ufree(dbenv, pkey.data);
				__os_ufree(dbenv, data.data);
				if (key_alloc)
					__os_ufree(dbenv,
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
			memcpy(replyp->pkeydata.pkeydata_val, pkey.data,
			    pkey.size);
		} else
			replyp->pkeydata.pkeydata_val = pkey.data;
		replyp->pkeydata.pkeydata_len = pkey.size;

		/*
		 * Data
		 */
		if (data.data == datadata) {
			ret = __os_umalloc(dbenv,
			     data.size, &replyp->datadata.datadata_val);
			if (ret != 0) {
				__os_ufree(dbenv, skey.data);
				__os_ufree(dbenv, pkey.data);
				__os_ufree(dbenv, data.data);
				/*
				 * If key_alloc is 1, just skey needs to be
				 * freed, if key_alloc is 2, both skey and pkey
				 * need to be freed.
				 */
				if (key_alloc--)
					__os_ufree(dbenv,
					    replyp->skeydata.skeydata_val);
				if (key_alloc)
					__os_ufree(dbenv,
					    replyp->pkeydata.pkeydata_val);
				goto err;
			}
			memcpy(replyp->datadata.datadata_val, data.data,
			    data.size);
		} else
			replyp->datadata.datadata_val = data.data;
		replyp->datadata.datadata_len = data.size;
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

/*
 * PUBLIC: void __dbc_put_proc __P((long, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     u_int32_t, void *, u_int32_t, u_int32_t, __dbc_put_reply *,
 * PUBLIC:     int *));
 */
void
__dbc_put_proc(dbccl_id, keydlen, keydoff, keyulen, keyflags, keydata,
    keysize, datadlen, datadoff, dataulen, dataflags, datadata, datasize,
    flags, replyp, freep)
	long dbccl_id;
	u_int32_t keydlen;
	u_int32_t keydoff;
	u_int32_t keyulen;
	u_int32_t keyflags;
	void *keydata;
	u_int32_t keysize;
	u_int32_t datadlen;
	u_int32_t datadoff;
	u_int32_t dataulen;
	u_int32_t dataflags;
	void *datadata;
	u_int32_t datasize;
	u_int32_t flags;
	__dbc_put_reply *replyp;
	int * freep;
{
	DB *dbp;
	DBC *dbc;
	DBT key, data;
	ct_entry *dbc_ctp;
	int ret;

	ACTIVATE_CTP(dbc_ctp, dbccl_id, CT_CURSOR);
	dbc = (DBC *)dbc_ctp->ct_anyp;
	dbp = (DB *)dbc_ctp->ct_parent->ct_anyp;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Set up key and data DBT */
	key.dlen = keydlen;
	key.ulen = keyulen;
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
	data.ulen = dataulen;
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
