/* Do not edit: automatically built by gen_rpc.awk. */
#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc_auto/db_server.h"
#include "dbinc/db_server_int.h"
#include "dbinc_auto/rpc_server_ext.h"

/*
 * PUBLIC: __env_cachesize_reply *__db_env_cachesize_4001 
 * PUBLIC:     __P((__env_cachesize_msg *, struct svc_req *));
 */
__env_cachesize_reply *
__db_env_cachesize_4001(msg, req)
	__env_cachesize_msg *msg;
	struct svc_req *req;
{
	static __env_cachesize_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_cachesize_proc(msg->dbenvcl_id,
	    msg->gbytes,
	    msg->bytes,
	    msg->ncache,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __env_close_reply *__db_env_close_4001 __P((__env_close_msg *,
 * PUBLIC:      struct svc_req *));
 */
__env_close_reply *
__db_env_close_4001(msg, req)
	__env_close_msg *msg;
	struct svc_req *req;
{
	static __env_close_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_close_proc(msg->dbenvcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __env_create_reply *__db_env_create_4001 __P((__env_create_msg *,
 * PUBLIC:      struct svc_req *));
 */
__env_create_reply *
__db_env_create_4001(msg, req)
	__env_create_msg *msg;
	struct svc_req *req;
{
	static __env_create_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_create_proc(msg->timeout,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __env_dbremove_reply *__db_env_dbremove_4001 
 * PUBLIC:     __P((__env_dbremove_msg *, struct svc_req *));
 */
__env_dbremove_reply *
__db_env_dbremove_4001(msg, req)
	__env_dbremove_msg *msg;
	struct svc_req *req;
{
	static __env_dbremove_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_dbremove_proc(msg->dbenvcl_id,
	    msg->txnpcl_id,
	    (*msg->name == '\0') ? NULL : msg->name,
	    (*msg->subdb == '\0') ? NULL : msg->subdb,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __env_dbrename_reply *__db_env_dbrename_4001 
 * PUBLIC:     __P((__env_dbrename_msg *, struct svc_req *));
 */
__env_dbrename_reply *
__db_env_dbrename_4001(msg, req)
	__env_dbrename_msg *msg;
	struct svc_req *req;
{
	static __env_dbrename_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_dbrename_proc(msg->dbenvcl_id,
	    msg->txnpcl_id,
	    (*msg->name == '\0') ? NULL : msg->name,
	    (*msg->subdb == '\0') ? NULL : msg->subdb,
	    (*msg->newname == '\0') ? NULL : msg->newname,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __env_encrypt_reply *__db_env_encrypt_4001 
 * PUBLIC:     __P((__env_encrypt_msg *, struct svc_req *));
 */
__env_encrypt_reply *
__db_env_encrypt_4001(msg, req)
	__env_encrypt_msg *msg;
	struct svc_req *req;
{
	static __env_encrypt_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_encrypt_proc(msg->dbenvcl_id,
	    (*msg->passwd == '\0') ? NULL : msg->passwd,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __env_flags_reply *__db_env_flags_4001 __P((__env_flags_msg *,
 * PUBLIC:      struct svc_req *));
 */
__env_flags_reply *
__db_env_flags_4001(msg, req)
	__env_flags_msg *msg;
	struct svc_req *req;
{
	static __env_flags_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_flags_proc(msg->dbenvcl_id,
	    msg->flags,
	    msg->onoff,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __env_open_reply *__db_env_open_4001 __P((__env_open_msg *,
 * PUBLIC:      struct svc_req *));
 */
__env_open_reply *
__db_env_open_4001(msg, req)
	__env_open_msg *msg;
	struct svc_req *req;
{
	static __env_open_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_open_proc(msg->dbenvcl_id,
	    (*msg->home == '\0') ? NULL : msg->home,
	    msg->flags,
	    msg->mode,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __env_remove_reply *__db_env_remove_4001 __P((__env_remove_msg *,
 * PUBLIC:      struct svc_req *));
 */
__env_remove_reply *
__db_env_remove_4001(msg, req)
	__env_remove_msg *msg;
	struct svc_req *req;
{
	static __env_remove_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__env_remove_proc(msg->dbenvcl_id,
	    (*msg->home == '\0') ? NULL : msg->home,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __txn_abort_reply *__db_txn_abort_4001 __P((__txn_abort_msg *,
 * PUBLIC:      struct svc_req *));
 */
__txn_abort_reply *
__db_txn_abort_4001(msg, req)
	__txn_abort_msg *msg;
	struct svc_req *req;
{
	static __txn_abort_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__txn_abort_proc(msg->txnpcl_id,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __txn_begin_reply *__db_txn_begin_4001 __P((__txn_begin_msg *,
 * PUBLIC:      struct svc_req *));
 */
__txn_begin_reply *
__db_txn_begin_4001(msg, req)
	__txn_begin_msg *msg;
	struct svc_req *req;
{
	static __txn_begin_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__txn_begin_proc(msg->dbenvcl_id,
	    msg->parentcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __txn_commit_reply *__db_txn_commit_4001 __P((__txn_commit_msg *,
 * PUBLIC:      struct svc_req *));
 */
__txn_commit_reply *
__db_txn_commit_4001(msg, req)
	__txn_commit_msg *msg;
	struct svc_req *req;
{
	static __txn_commit_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__txn_commit_proc(msg->txnpcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __txn_discard_reply *__db_txn_discard_4001 
 * PUBLIC:     __P((__txn_discard_msg *, struct svc_req *));
 */
__txn_discard_reply *
__db_txn_discard_4001(msg, req)
	__txn_discard_msg *msg;
	struct svc_req *req;
{
	static __txn_discard_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__txn_discard_proc(msg->txnpcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __txn_prepare_reply *__db_txn_prepare_4001 
 * PUBLIC:     __P((__txn_prepare_msg *, struct svc_req *));
 */
__txn_prepare_reply *
__db_txn_prepare_4001(msg, req)
	__txn_prepare_msg *msg;
	struct svc_req *req;
{
	static __txn_prepare_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__txn_prepare_proc(msg->txnpcl_id,
	    msg->gid,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __txn_recover_reply *__db_txn_recover_4001 
 * PUBLIC:     __P((__txn_recover_msg *, struct svc_req *));
 */
__txn_recover_reply *
__db_txn_recover_4001(msg, req)
	__txn_recover_msg *msg;
	struct svc_req *req;
{
	static __txn_recover_reply reply; /* must be static */
	static int __txn_recover_free = 0; /* must be static */

	COMPQUIET(req, NULL);
	if (__txn_recover_free)
		xdr_free((xdrproc_t)xdr___txn_recover_reply, (void *)&reply);
	__txn_recover_free = 0;

	/* Reinitialize allocated fields */
	reply.txn.txn_val = NULL;
	reply.gid.gid_val = NULL;

	__txn_recover_proc(msg->dbenvcl_id,
	    msg->count,
	    msg->flags,
	    &reply,
	    &__txn_recover_free);
	return (&reply);
}

/*
 * PUBLIC: __db_associate_reply *__db_db_associate_4001 
 * PUBLIC:     __P((__db_associate_msg *, struct svc_req *));
 */
__db_associate_reply *
__db_db_associate_4001(msg, req)
	__db_associate_msg *msg;
	struct svc_req *req;
{
	static __db_associate_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_associate_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    msg->sdbpcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_bt_maxkey_reply *__db_db_bt_maxkey_4001 
 * PUBLIC:     __P((__db_bt_maxkey_msg *, struct svc_req *));
 */
__db_bt_maxkey_reply *
__db_db_bt_maxkey_4001(msg, req)
	__db_bt_maxkey_msg *msg;
	struct svc_req *req;
{
	static __db_bt_maxkey_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_bt_maxkey_proc(msg->dbpcl_id,
	    msg->maxkey,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_bt_minkey_reply *__db_db_bt_minkey_4001 
 * PUBLIC:     __P((__db_bt_minkey_msg *, struct svc_req *));
 */
__db_bt_minkey_reply *
__db_db_bt_minkey_4001(msg, req)
	__db_bt_minkey_msg *msg;
	struct svc_req *req;
{
	static __db_bt_minkey_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_bt_minkey_proc(msg->dbpcl_id,
	    msg->minkey,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_close_reply *__db_db_close_4001 __P((__db_close_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_close_reply *
__db_db_close_4001(msg, req)
	__db_close_msg *msg;
	struct svc_req *req;
{
	static __db_close_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_close_proc(msg->dbpcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_create_reply *__db_db_create_4001 __P((__db_create_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_create_reply *
__db_db_create_4001(msg, req)
	__db_create_msg *msg;
	struct svc_req *req;
{
	static __db_create_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_create_proc(msg->dbenvcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_del_reply *__db_db_del_4001 __P((__db_del_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_del_reply *
__db_db_del_4001(msg, req)
	__db_del_msg *msg;
	struct svc_req *req;
{
	static __db_del_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_del_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    msg->keydlen,
	    msg->keydoff,
	    msg->keyulen,
	    msg->keyflags,
	    msg->keydata.keydata_val,
	    msg->keydata.keydata_len,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_encrypt_reply *__db_db_encrypt_4001 __P((__db_encrypt_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_encrypt_reply *
__db_db_encrypt_4001(msg, req)
	__db_encrypt_msg *msg;
	struct svc_req *req;
{
	static __db_encrypt_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_encrypt_proc(msg->dbpcl_id,
	    (*msg->passwd == '\0') ? NULL : msg->passwd,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_extentsize_reply *__db_db_extentsize_4001 
 * PUBLIC:     __P((__db_extentsize_msg *, struct svc_req *));
 */
__db_extentsize_reply *
__db_db_extentsize_4001(msg, req)
	__db_extentsize_msg *msg;
	struct svc_req *req;
{
	static __db_extentsize_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_extentsize_proc(msg->dbpcl_id,
	    msg->extentsize,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_flags_reply *__db_db_flags_4001 __P((__db_flags_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_flags_reply *
__db_db_flags_4001(msg, req)
	__db_flags_msg *msg;
	struct svc_req *req;
{
	static __db_flags_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_flags_proc(msg->dbpcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_get_reply *__db_db_get_4001 __P((__db_get_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_get_reply *
__db_db_get_4001(msg, req)
	__db_get_msg *msg;
	struct svc_req *req;
{
	static __db_get_reply reply; /* must be static */
	static int __db_get_free = 0; /* must be static */

	COMPQUIET(req, NULL);
	if (__db_get_free)
		xdr_free((xdrproc_t)xdr___db_get_reply, (void *)&reply);
	__db_get_free = 0;

	/* Reinitialize allocated fields */
	reply.keydata.keydata_val = NULL;
	reply.datadata.datadata_val = NULL;

	__db_get_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    msg->keydlen,
	    msg->keydoff,
	    msg->keyulen,
	    msg->keyflags,
	    msg->keydata.keydata_val,
	    msg->keydata.keydata_len,
	    msg->datadlen,
	    msg->datadoff,
	    msg->dataulen,
	    msg->dataflags,
	    msg->datadata.datadata_val,
	    msg->datadata.datadata_len,
	    msg->flags,
	    &reply,
	    &__db_get_free);
	return (&reply);
}

/*
 * PUBLIC: __db_h_ffactor_reply *__db_db_h_ffactor_4001 
 * PUBLIC:     __P((__db_h_ffactor_msg *, struct svc_req *));
 */
__db_h_ffactor_reply *
__db_db_h_ffactor_4001(msg, req)
	__db_h_ffactor_msg *msg;
	struct svc_req *req;
{
	static __db_h_ffactor_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_h_ffactor_proc(msg->dbpcl_id,
	    msg->ffactor,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_h_nelem_reply *__db_db_h_nelem_4001 __P((__db_h_nelem_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_h_nelem_reply *
__db_db_h_nelem_4001(msg, req)
	__db_h_nelem_msg *msg;
	struct svc_req *req;
{
	static __db_h_nelem_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_h_nelem_proc(msg->dbpcl_id,
	    msg->nelem,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_key_range_reply *__db_db_key_range_4001 
 * PUBLIC:     __P((__db_key_range_msg *, struct svc_req *));
 */
__db_key_range_reply *
__db_db_key_range_4001(msg, req)
	__db_key_range_msg *msg;
	struct svc_req *req;
{
	static __db_key_range_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_key_range_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    msg->keydlen,
	    msg->keydoff,
	    msg->keyulen,
	    msg->keyflags,
	    msg->keydata.keydata_val,
	    msg->keydata.keydata_len,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_lorder_reply *__db_db_lorder_4001 __P((__db_lorder_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_lorder_reply *
__db_db_lorder_4001(msg, req)
	__db_lorder_msg *msg;
	struct svc_req *req;
{
	static __db_lorder_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_lorder_proc(msg->dbpcl_id,
	    msg->lorder,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_open_reply *__db_db_open_4001 __P((__db_open_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_open_reply *
__db_db_open_4001(msg, req)
	__db_open_msg *msg;
	struct svc_req *req;
{
	static __db_open_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_open_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    (*msg->name == '\0') ? NULL : msg->name,
	    (*msg->subdb == '\0') ? NULL : msg->subdb,
	    msg->type,
	    msg->flags,
	    msg->mode,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_pagesize_reply *__db_db_pagesize_4001 
 * PUBLIC:     __P((__db_pagesize_msg *, struct svc_req *));
 */
__db_pagesize_reply *
__db_db_pagesize_4001(msg, req)
	__db_pagesize_msg *msg;
	struct svc_req *req;
{
	static __db_pagesize_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_pagesize_proc(msg->dbpcl_id,
	    msg->pagesize,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_pget_reply *__db_db_pget_4001 __P((__db_pget_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_pget_reply *
__db_db_pget_4001(msg, req)
	__db_pget_msg *msg;
	struct svc_req *req;
{
	static __db_pget_reply reply; /* must be static */
	static int __db_pget_free = 0; /* must be static */

	COMPQUIET(req, NULL);
	if (__db_pget_free)
		xdr_free((xdrproc_t)xdr___db_pget_reply, (void *)&reply);
	__db_pget_free = 0;

	/* Reinitialize allocated fields */
	reply.skeydata.skeydata_val = NULL;
	reply.pkeydata.pkeydata_val = NULL;
	reply.datadata.datadata_val = NULL;

	__db_pget_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    msg->skeydlen,
	    msg->skeydoff,
	    msg->skeyulen,
	    msg->skeyflags,
	    msg->skeydata.skeydata_val,
	    msg->skeydata.skeydata_len,
	    msg->pkeydlen,
	    msg->pkeydoff,
	    msg->pkeyulen,
	    msg->pkeyflags,
	    msg->pkeydata.pkeydata_val,
	    msg->pkeydata.pkeydata_len,
	    msg->datadlen,
	    msg->datadoff,
	    msg->dataulen,
	    msg->dataflags,
	    msg->datadata.datadata_val,
	    msg->datadata.datadata_len,
	    msg->flags,
	    &reply,
	    &__db_pget_free);
	return (&reply);
}

/*
 * PUBLIC: __db_put_reply *__db_db_put_4001 __P((__db_put_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_put_reply *
__db_db_put_4001(msg, req)
	__db_put_msg *msg;
	struct svc_req *req;
{
	static __db_put_reply reply; /* must be static */
	static int __db_put_free = 0; /* must be static */

	COMPQUIET(req, NULL);
	if (__db_put_free)
		xdr_free((xdrproc_t)xdr___db_put_reply, (void *)&reply);
	__db_put_free = 0;

	/* Reinitialize allocated fields */
	reply.keydata.keydata_val = NULL;

	__db_put_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    msg->keydlen,
	    msg->keydoff,
	    msg->keyulen,
	    msg->keyflags,
	    msg->keydata.keydata_val,
	    msg->keydata.keydata_len,
	    msg->datadlen,
	    msg->datadoff,
	    msg->dataulen,
	    msg->dataflags,
	    msg->datadata.datadata_val,
	    msg->datadata.datadata_len,
	    msg->flags,
	    &reply,
	    &__db_put_free);
	return (&reply);
}

/*
 * PUBLIC: __db_re_delim_reply *__db_db_re_delim_4001 
 * PUBLIC:     __P((__db_re_delim_msg *, struct svc_req *));
 */
__db_re_delim_reply *
__db_db_re_delim_4001(msg, req)
	__db_re_delim_msg *msg;
	struct svc_req *req;
{
	static __db_re_delim_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_re_delim_proc(msg->dbpcl_id,
	    msg->delim,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_re_len_reply *__db_db_re_len_4001 __P((__db_re_len_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_re_len_reply *
__db_db_re_len_4001(msg, req)
	__db_re_len_msg *msg;
	struct svc_req *req;
{
	static __db_re_len_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_re_len_proc(msg->dbpcl_id,
	    msg->len,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_re_pad_reply *__db_db_re_pad_4001 __P((__db_re_pad_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_re_pad_reply *
__db_db_re_pad_4001(msg, req)
	__db_re_pad_msg *msg;
	struct svc_req *req;
{
	static __db_re_pad_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_re_pad_proc(msg->dbpcl_id,
	    msg->pad,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_remove_reply *__db_db_remove_4001 __P((__db_remove_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_remove_reply *
__db_db_remove_4001(msg, req)
	__db_remove_msg *msg;
	struct svc_req *req;
{
	static __db_remove_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_remove_proc(msg->dbpcl_id,
	    (*msg->name == '\0') ? NULL : msg->name,
	    (*msg->subdb == '\0') ? NULL : msg->subdb,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_rename_reply *__db_db_rename_4001 __P((__db_rename_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_rename_reply *
__db_db_rename_4001(msg, req)
	__db_rename_msg *msg;
	struct svc_req *req;
{
	static __db_rename_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_rename_proc(msg->dbpcl_id,
	    (*msg->name == '\0') ? NULL : msg->name,
	    (*msg->subdb == '\0') ? NULL : msg->subdb,
	    (*msg->newname == '\0') ? NULL : msg->newname,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_stat_reply *__db_db_stat_4001 __P((__db_stat_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_stat_reply *
__db_db_stat_4001(msg, req)
	__db_stat_msg *msg;
	struct svc_req *req;
{
	static __db_stat_reply reply; /* must be static */
	static int __db_stat_free = 0; /* must be static */

	COMPQUIET(req, NULL);
	if (__db_stat_free)
		xdr_free((xdrproc_t)xdr___db_stat_reply, (void *)&reply);
	__db_stat_free = 0;

	/* Reinitialize allocated fields */
	reply.stats.stats_val = NULL;

	__db_stat_proc(msg->dbpcl_id,
	    msg->flags,
	    &reply,
	    &__db_stat_free);
	return (&reply);
}

/*
 * PUBLIC: __db_sync_reply *__db_db_sync_4001 __P((__db_sync_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_sync_reply *
__db_db_sync_4001(msg, req)
	__db_sync_msg *msg;
	struct svc_req *req;
{
	static __db_sync_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_sync_proc(msg->dbpcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_truncate_reply *__db_db_truncate_4001 
 * PUBLIC:     __P((__db_truncate_msg *, struct svc_req *));
 */
__db_truncate_reply *
__db_db_truncate_4001(msg, req)
	__db_truncate_msg *msg;
	struct svc_req *req;
{
	static __db_truncate_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_truncate_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_cursor_reply *__db_db_cursor_4001 __P((__db_cursor_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_cursor_reply *
__db_db_cursor_4001(msg, req)
	__db_cursor_msg *msg;
	struct svc_req *req;
{
	static __db_cursor_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_cursor_proc(msg->dbpcl_id,
	    msg->txnpcl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __db_join_reply *__db_db_join_4001 __P((__db_join_msg *,
 * PUBLIC:      struct svc_req *));
 */
__db_join_reply *
__db_db_join_4001(msg, req)
	__db_join_msg *msg;
	struct svc_req *req;
{
	static __db_join_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__db_join_proc(msg->dbpcl_id,
	    msg->curs.curs_val,
	    msg->curs.curs_len,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __dbc_close_reply *__db_dbc_close_4001 __P((__dbc_close_msg *,
 * PUBLIC:      struct svc_req *));
 */
__dbc_close_reply *
__db_dbc_close_4001(msg, req)
	__dbc_close_msg *msg;
	struct svc_req *req;
{
	static __dbc_close_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__dbc_close_proc(msg->dbccl_id,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __dbc_count_reply *__db_dbc_count_4001 __P((__dbc_count_msg *,
 * PUBLIC:      struct svc_req *));
 */
__dbc_count_reply *
__db_dbc_count_4001(msg, req)
	__dbc_count_msg *msg;
	struct svc_req *req;
{
	static __dbc_count_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__dbc_count_proc(msg->dbccl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __dbc_del_reply *__db_dbc_del_4001 __P((__dbc_del_msg *,
 * PUBLIC:      struct svc_req *));
 */
__dbc_del_reply *
__db_dbc_del_4001(msg, req)
	__dbc_del_msg *msg;
	struct svc_req *req;
{
	static __dbc_del_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__dbc_del_proc(msg->dbccl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __dbc_dup_reply *__db_dbc_dup_4001 __P((__dbc_dup_msg *,
 * PUBLIC:      struct svc_req *));
 */
__dbc_dup_reply *
__db_dbc_dup_4001(msg, req)
	__dbc_dup_msg *msg;
	struct svc_req *req;
{
	static __dbc_dup_reply reply; /* must be static */
	COMPQUIET(req, NULL);

	__dbc_dup_proc(msg->dbccl_id,
	    msg->flags,
	    &reply);

	return (&reply);
}

/*
 * PUBLIC: __dbc_get_reply *__db_dbc_get_4001 __P((__dbc_get_msg *,
 * PUBLIC:      struct svc_req *));
 */
__dbc_get_reply *
__db_dbc_get_4001(msg, req)
	__dbc_get_msg *msg;
	struct svc_req *req;
{
	static __dbc_get_reply reply; /* must be static */
	static int __dbc_get_free = 0; /* must be static */

	COMPQUIET(req, NULL);
	if (__dbc_get_free)
		xdr_free((xdrproc_t)xdr___dbc_get_reply, (void *)&reply);
	__dbc_get_free = 0;

	/* Reinitialize allocated fields */
	reply.keydata.keydata_val = NULL;
	reply.datadata.datadata_val = NULL;

	__dbc_get_proc(msg->dbccl_id,
	    msg->keydlen,
	    msg->keydoff,
	    msg->keyulen,
	    msg->keyflags,
	    msg->keydata.keydata_val,
	    msg->keydata.keydata_len,
	    msg->datadlen,
	    msg->datadoff,
	    msg->dataulen,
	    msg->dataflags,
	    msg->datadata.datadata_val,
	    msg->datadata.datadata_len,
	    msg->flags,
	    &reply,
	    &__dbc_get_free);
	return (&reply);
}

/*
 * PUBLIC: __dbc_pget_reply *__db_dbc_pget_4001 __P((__dbc_pget_msg *,
 * PUBLIC:      struct svc_req *));
 */
__dbc_pget_reply *
__db_dbc_pget_4001(msg, req)
	__dbc_pget_msg *msg;
	struct svc_req *req;
{
	static __dbc_pget_reply reply; /* must be static */
	static int __dbc_pget_free = 0; /* must be static */

	COMPQUIET(req, NULL);
	if (__dbc_pget_free)
		xdr_free((xdrproc_t)xdr___dbc_pget_reply, (void *)&reply);
	__dbc_pget_free = 0;

	/* Reinitialize allocated fields */
	reply.skeydata.skeydata_val = NULL;
	reply.pkeydata.pkeydata_val = NULL;
	reply.datadata.datadata_val = NULL;

	__dbc_pget_proc(msg->dbccl_id,
	    msg->skeydlen,
	    msg->skeydoff,
	    msg->skeyulen,
	    msg->skeyflags,
	    msg->skeydata.skeydata_val,
	    msg->skeydata.skeydata_len,
	    msg->pkeydlen,
	    msg->pkeydoff,
	    msg->pkeyulen,
	    msg->pkeyflags,
	    msg->pkeydata.pkeydata_val,
	    msg->pkeydata.pkeydata_len,
	    msg->datadlen,
	    msg->datadoff,
	    msg->dataulen,
	    msg->dataflags,
	    msg->datadata.datadata_val,
	    msg->datadata.datadata_len,
	    msg->flags,
	    &reply,
	    &__dbc_pget_free);
	return (&reply);
}

/*
 * PUBLIC: __dbc_put_reply *__db_dbc_put_4001 __P((__dbc_put_msg *,
 * PUBLIC:      struct svc_req *));
 */
__dbc_put_reply *
__db_dbc_put_4001(msg, req)
	__dbc_put_msg *msg;
	struct svc_req *req;
{
	static __dbc_put_reply reply; /* must be static */
	static int __dbc_put_free = 0; /* must be static */

	COMPQUIET(req, NULL);
	if (__dbc_put_free)
		xdr_free((xdrproc_t)xdr___dbc_put_reply, (void *)&reply);
	__dbc_put_free = 0;

	/* Reinitialize allocated fields */
	reply.keydata.keydata_val = NULL;

	__dbc_put_proc(msg->dbccl_id,
	    msg->keydlen,
	    msg->keydoff,
	    msg->keyulen,
	    msg->keyflags,
	    msg->keydata.keydata_val,
	    msg->keydata.keydata_len,
	    msg->datadlen,
	    msg->datadoff,
	    msg->dataulen,
	    msg->dataflags,
	    msg->datadata.datadata_val,
	    msg->datadata.datadata_len,
	    msg->flags,
	    &reply,
	    &__dbc_put_free);
	return (&reply);
}

