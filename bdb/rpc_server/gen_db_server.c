/* Do not edit: automatically built by gen_rpc.awk. */
#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>

#include <errno.h>
#include <string.h>
#endif
#include "db_server.h"

#include "db_int.h"
#include "db_server_int.h"
#include "rpc_server_ext.h"

#include "gen_server_ext.h"

__env_cachesize_reply *
__db_env_cachesize_1(req)
	__env_cachesize_msg *req;
{
	static __env_cachesize_reply reply; /* must be static */

	__env_cachesize_1_proc(req->dbenvcl_id,
	    req->gbytes,
	    req->bytes,
	    req->ncache,
	    &reply);

	return (&reply);
}

__env_close_reply *
__db_env_close_1(req)
	__env_close_msg *req;
{
	static __env_close_reply reply; /* must be static */

	__env_close_1_proc(req->dbenvcl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__env_create_reply *
__db_env_create_1(req)
	__env_create_msg *req;
{
	static __env_create_reply reply; /* must be static */

	__env_create_1_proc(req->timeout,
	    &reply);

	return (&reply);
}

__env_flags_reply *
__db_env_flags_1(req)
	__env_flags_msg *req;
{
	static __env_flags_reply reply; /* must be static */

	__env_flags_1_proc(req->dbenvcl_id,
	    req->flags,
	    req->onoff,
	    &reply);

	return (&reply);
}

__env_open_reply *
__db_env_open_1(req)
	__env_open_msg *req;
{
	static __env_open_reply reply; /* must be static */

	__env_open_1_proc(req->dbenvcl_id,
	    (*req->home == '\0') ? NULL : req->home,
	    req->flags,
	    req->mode,
	    &reply);

	return (&reply);
}

__env_remove_reply *
__db_env_remove_1(req)
	__env_remove_msg *req;
{
	static __env_remove_reply reply; /* must be static */

	__env_remove_1_proc(req->dbenvcl_id,
	    (*req->home == '\0') ? NULL : req->home,
	    req->flags,
	    &reply);

	return (&reply);
}

__txn_abort_reply *
__db_txn_abort_1(req)
	__txn_abort_msg *req;
{
	static __txn_abort_reply reply; /* must be static */

	__txn_abort_1_proc(req->txnpcl_id,
	    &reply);

	return (&reply);
}

__txn_begin_reply *
__db_txn_begin_1(req)
	__txn_begin_msg *req;
{
	static __txn_begin_reply reply; /* must be static */

	__txn_begin_1_proc(req->envpcl_id,
	    req->parentcl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__txn_commit_reply *
__db_txn_commit_1(req)
	__txn_commit_msg *req;
{
	static __txn_commit_reply reply; /* must be static */

	__txn_commit_1_proc(req->txnpcl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__db_bt_maxkey_reply *
__db_db_bt_maxkey_1(req)
	__db_bt_maxkey_msg *req;
{
	static __db_bt_maxkey_reply reply; /* must be static */

	__db_bt_maxkey_1_proc(req->dbpcl_id,
	    req->maxkey,
	    &reply);

	return (&reply);
}

__db_bt_minkey_reply *
__db_db_bt_minkey_1(req)
	__db_bt_minkey_msg *req;
{
	static __db_bt_minkey_reply reply; /* must be static */

	__db_bt_minkey_1_proc(req->dbpcl_id,
	    req->minkey,
	    &reply);

	return (&reply);
}

__db_close_reply *
__db_db_close_1(req)
	__db_close_msg *req;
{
	static __db_close_reply reply; /* must be static */

	__db_close_1_proc(req->dbpcl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__db_create_reply *
__db_db_create_1(req)
	__db_create_msg *req;
{
	static __db_create_reply reply; /* must be static */

	__db_create_1_proc(req->flags,
	    req->envpcl_id,
	    &reply);

	return (&reply);
}

__db_del_reply *
__db_db_del_1(req)
	__db_del_msg *req;
{
	static __db_del_reply reply; /* must be static */

	__db_del_1_proc(req->dbpcl_id,
	    req->txnpcl_id,
	    req->keydlen,
	    req->keydoff,
	    req->keyflags,
	    req->keydata.keydata_val,
	    req->keydata.keydata_len,
	    req->flags,
	    &reply);

	return (&reply);
}

__db_extentsize_reply *
__db_db_extentsize_1(req)
	__db_extentsize_msg *req;
{
	static __db_extentsize_reply reply; /* must be static */

	__db_extentsize_1_proc(req->dbpcl_id,
	    req->extentsize,
	    &reply);

	return (&reply);
}

__db_flags_reply *
__db_db_flags_1(req)
	__db_flags_msg *req;
{
	static __db_flags_reply reply; /* must be static */

	__db_flags_1_proc(req->dbpcl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__db_get_reply *
__db_db_get_1(req)
	__db_get_msg *req;
{
	static __db_get_reply reply; /* must be static */
	static int __db_get_free = 0; /* must be static */

	if (__db_get_free)
		xdr_free((xdrproc_t)xdr___db_get_reply, (void *)&reply);
	__db_get_free = 0;

	/* Reinitialize allocated fields */
	reply.keydata.keydata_val = NULL;
	reply.datadata.datadata_val = NULL;

	__db_get_1_proc(req->dbpcl_id,
	    req->txnpcl_id,
	    req->keydlen,
	    req->keydoff,
	    req->keyflags,
	    req->keydata.keydata_val,
	    req->keydata.keydata_len,
	    req->datadlen,
	    req->datadoff,
	    req->dataflags,
	    req->datadata.datadata_val,
	    req->datadata.datadata_len,
	    req->flags,
	    &reply,
	    &__db_get_free);
	return (&reply);
}

__db_h_ffactor_reply *
__db_db_h_ffactor_1(req)
	__db_h_ffactor_msg *req;
{
	static __db_h_ffactor_reply reply; /* must be static */

	__db_h_ffactor_1_proc(req->dbpcl_id,
	    req->ffactor,
	    &reply);

	return (&reply);
}

__db_h_nelem_reply *
__db_db_h_nelem_1(req)
	__db_h_nelem_msg *req;
{
	static __db_h_nelem_reply reply; /* must be static */

	__db_h_nelem_1_proc(req->dbpcl_id,
	    req->nelem,
	    &reply);

	return (&reply);
}

__db_key_range_reply *
__db_db_key_range_1(req)
	__db_key_range_msg *req;
{
	static __db_key_range_reply reply; /* must be static */

	__db_key_range_1_proc(req->dbpcl_id,
	    req->txnpcl_id,
	    req->keydlen,
	    req->keydoff,
	    req->keyflags,
	    req->keydata.keydata_val,
	    req->keydata.keydata_len,
	    req->flags,
	    &reply);

	return (&reply);
}

__db_lorder_reply *
__db_db_lorder_1(req)
	__db_lorder_msg *req;
{
	static __db_lorder_reply reply; /* must be static */

	__db_lorder_1_proc(req->dbpcl_id,
	    req->lorder,
	    &reply);

	return (&reply);
}

__db_open_reply *
__db_db_open_1(req)
	__db_open_msg *req;
{
	static __db_open_reply reply; /* must be static */

	__db_open_1_proc(req->dbpcl_id,
	    (*req->name == '\0') ? NULL : req->name,
	    (*req->subdb == '\0') ? NULL : req->subdb,
	    req->type,
	    req->flags,
	    req->mode,
	    &reply);

	return (&reply);
}

__db_pagesize_reply *
__db_db_pagesize_1(req)
	__db_pagesize_msg *req;
{
	static __db_pagesize_reply reply; /* must be static */

	__db_pagesize_1_proc(req->dbpcl_id,
	    req->pagesize,
	    &reply);

	return (&reply);
}

__db_put_reply *
__db_db_put_1(req)
	__db_put_msg *req;
{
	static __db_put_reply reply; /* must be static */
	static int __db_put_free = 0; /* must be static */

	if (__db_put_free)
		xdr_free((xdrproc_t)xdr___db_put_reply, (void *)&reply);
	__db_put_free = 0;

	/* Reinitialize allocated fields */
	reply.keydata.keydata_val = NULL;

	__db_put_1_proc(req->dbpcl_id,
	    req->txnpcl_id,
	    req->keydlen,
	    req->keydoff,
	    req->keyflags,
	    req->keydata.keydata_val,
	    req->keydata.keydata_len,
	    req->datadlen,
	    req->datadoff,
	    req->dataflags,
	    req->datadata.datadata_val,
	    req->datadata.datadata_len,
	    req->flags,
	    &reply,
	    &__db_put_free);
	return (&reply);
}

__db_re_delim_reply *
__db_db_re_delim_1(req)
	__db_re_delim_msg *req;
{
	static __db_re_delim_reply reply; /* must be static */

	__db_re_delim_1_proc(req->dbpcl_id,
	    req->delim,
	    &reply);

	return (&reply);
}

__db_re_len_reply *
__db_db_re_len_1(req)
	__db_re_len_msg *req;
{
	static __db_re_len_reply reply; /* must be static */

	__db_re_len_1_proc(req->dbpcl_id,
	    req->len,
	    &reply);

	return (&reply);
}

__db_re_pad_reply *
__db_db_re_pad_1(req)
	__db_re_pad_msg *req;
{
	static __db_re_pad_reply reply; /* must be static */

	__db_re_pad_1_proc(req->dbpcl_id,
	    req->pad,
	    &reply);

	return (&reply);
}

__db_remove_reply *
__db_db_remove_1(req)
	__db_remove_msg *req;
{
	static __db_remove_reply reply; /* must be static */

	__db_remove_1_proc(req->dbpcl_id,
	    (*req->name == '\0') ? NULL : req->name,
	    (*req->subdb == '\0') ? NULL : req->subdb,
	    req->flags,
	    &reply);

	return (&reply);
}

__db_rename_reply *
__db_db_rename_1(req)
	__db_rename_msg *req;
{
	static __db_rename_reply reply; /* must be static */

	__db_rename_1_proc(req->dbpcl_id,
	    (*req->name == '\0') ? NULL : req->name,
	    (*req->subdb == '\0') ? NULL : req->subdb,
	    (*req->newname == '\0') ? NULL : req->newname,
	    req->flags,
	    &reply);

	return (&reply);
}

__db_stat_reply *
__db_db_stat_1(req)
	__db_stat_msg *req;
{
	static __db_stat_reply reply; /* must be static */
	static int __db_stat_free = 0; /* must be static */

	if (__db_stat_free)
		xdr_free((xdrproc_t)xdr___db_stat_reply, (void *)&reply);
	__db_stat_free = 0;

	/* Reinitialize allocated fields */
	reply.statslist = NULL;

	__db_stat_1_proc(req->dbpcl_id,
	    req->flags,
	    &reply,
	    &__db_stat_free);
	return (&reply);
}

__db_swapped_reply *
__db_db_swapped_1(req)
	__db_swapped_msg *req;
{
	static __db_swapped_reply reply; /* must be static */

	__db_swapped_1_proc(req->dbpcl_id,
	    &reply);

	return (&reply);
}

__db_sync_reply *
__db_db_sync_1(req)
	__db_sync_msg *req;
{
	static __db_sync_reply reply; /* must be static */

	__db_sync_1_proc(req->dbpcl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__db_cursor_reply *
__db_db_cursor_1(req)
	__db_cursor_msg *req;
{
	static __db_cursor_reply reply; /* must be static */

	__db_cursor_1_proc(req->dbpcl_id,
	    req->txnpcl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

int __db_db_join_curslist __P((__db_join_curslist *, u_int32_t **));
void __db_db_join_cursfree __P((u_int32_t *));

__db_join_reply *
__db_db_join_1(req)
	__db_join_msg *req;
{
	u_int32_t *__db_curslist;
	int ret;
	static __db_join_reply reply; /* must be static */

	if ((ret = __db_db_join_curslist(req->curslist, &__db_curslist)) != 0)
		goto out;

	__db_join_1_proc(req->dbpcl_id,
	    __db_curslist,
	    req->flags,
	    &reply);

	__db_db_join_cursfree(__db_curslist);

out:
	return (&reply);
}

int
__db_db_join_curslist(locp, ppp)
	__db_join_curslist *locp;
	u_int32_t **ppp;
{
	u_int32_t *pp;
	int cnt, ret, size;
	__db_join_curslist *nl;

	for (cnt = 0, nl = locp; nl != NULL; cnt++, nl = nl->next)
		;

	if (cnt == 0) {
		*ppp = NULL;
		return (0);
	}
	size = sizeof(*pp) * (cnt + 1);
	if ((ret = __os_malloc(NULL, size, NULL, ppp)) != 0)
		return (ret);
	memset(*ppp, 0, size);
	for (pp = *ppp, nl = locp; nl != NULL; nl = nl->next, pp++) {
		*pp = *(u_int32_t *)nl->ent.ent_val;
	}
	return (0);
}

void
__db_db_join_cursfree(pp)
	u_int32_t *pp;
{
	size_t size;
	u_int32_t *p;

	if (pp == NULL)
		return;
	size = sizeof(*p);
	for (p = pp; *p != 0; p++) {
		size += sizeof(*p);
	}
	__os_free(pp, size);
}

__dbc_close_reply *
__db_dbc_close_1(req)
	__dbc_close_msg *req;
{
	static __dbc_close_reply reply; /* must be static */

	__dbc_close_1_proc(req->dbccl_id,
	    &reply);

	return (&reply);
}

__dbc_count_reply *
__db_dbc_count_1(req)
	__dbc_count_msg *req;
{
	static __dbc_count_reply reply; /* must be static */

	__dbc_count_1_proc(req->dbccl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__dbc_del_reply *
__db_dbc_del_1(req)
	__dbc_del_msg *req;
{
	static __dbc_del_reply reply; /* must be static */

	__dbc_del_1_proc(req->dbccl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__dbc_dup_reply *
__db_dbc_dup_1(req)
	__dbc_dup_msg *req;
{
	static __dbc_dup_reply reply; /* must be static */

	__dbc_dup_1_proc(req->dbccl_id,
	    req->flags,
	    &reply);

	return (&reply);
}

__dbc_get_reply *
__db_dbc_get_1(req)
	__dbc_get_msg *req;
{
	static __dbc_get_reply reply; /* must be static */
	static int __dbc_get_free = 0; /* must be static */

	if (__dbc_get_free)
		xdr_free((xdrproc_t)xdr___dbc_get_reply, (void *)&reply);
	__dbc_get_free = 0;

	/* Reinitialize allocated fields */
	reply.keydata.keydata_val = NULL;
	reply.datadata.datadata_val = NULL;

	__dbc_get_1_proc(req->dbccl_id,
	    req->keydlen,
	    req->keydoff,
	    req->keyflags,
	    req->keydata.keydata_val,
	    req->keydata.keydata_len,
	    req->datadlen,
	    req->datadoff,
	    req->dataflags,
	    req->datadata.datadata_val,
	    req->datadata.datadata_len,
	    req->flags,
	    &reply,
	    &__dbc_get_free);
	return (&reply);
}

__dbc_put_reply *
__db_dbc_put_1(req)
	__dbc_put_msg *req;
{
	static __dbc_put_reply reply; /* must be static */
	static int __dbc_put_free = 0; /* must be static */

	if (__dbc_put_free)
		xdr_free((xdrproc_t)xdr___dbc_put_reply, (void *)&reply);
	__dbc_put_free = 0;

	/* Reinitialize allocated fields */
	reply.keydata.keydata_val = NULL;

	__dbc_put_1_proc(req->dbccl_id,
	    req->keydlen,
	    req->keydoff,
	    req->keyflags,
	    req->keydata.keydata_val,
	    req->keydata.keydata_len,
	    req->datadlen,
	    req->datadoff,
	    req->dataflags,
	    req->datadata.datadata_val,
	    req->datadata.datadata_len,
	    req->flags,
	    &reply,
	    &__dbc_put_free);
	return (&reply);
}

