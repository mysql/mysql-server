/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_rpc_server_ext_h_
#define	_rpc_server_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

void __env_cachesize_proc __P((long, u_int32_t, u_int32_t, u_int32_t, __env_cachesize_reply *));
void __env_close_proc __P((long, u_int32_t, __env_close_reply *));
void __env_create_proc __P((u_int32_t, __env_create_reply *));
void __env_dbremove_proc __P((long, long, char *, char *, u_int32_t, __env_dbremove_reply *));
void __env_dbrename_proc __P((long, long, char *, char *, char *, u_int32_t, __env_dbrename_reply *));
void __env_encrypt_proc __P((long, char *, u_int32_t, __env_encrypt_reply *));
void __env_flags_proc __P((long, u_int32_t, u_int32_t, __env_flags_reply *));
void __env_open_proc __P((long, char *, u_int32_t, u_int32_t, __env_open_reply *));
void __env_remove_proc __P((long, char *, u_int32_t, __env_remove_reply *));
void __txn_abort_proc __P((long, __txn_abort_reply *));
void __txn_begin_proc __P((long, long, u_int32_t, __txn_begin_reply *));
void __txn_commit_proc __P((long, u_int32_t, __txn_commit_reply *));
void __txn_discard_proc __P((long, u_int32_t, __txn_discard_reply *));
void __txn_prepare_proc __P((long, u_int8_t *, __txn_prepare_reply *));
void __txn_recover_proc __P((long, u_int32_t, u_int32_t, __txn_recover_reply *, int *));
void __db_bt_maxkey_proc __P((long, u_int32_t, __db_bt_maxkey_reply *));
void __db_associate_proc __P((long, long, long, u_int32_t, __db_associate_reply *));
void __db_bt_minkey_proc __P((long, u_int32_t, __db_bt_minkey_reply *));
void __db_close_proc __P((long, u_int32_t, __db_close_reply *));
void __db_create_proc __P((long, u_int32_t, __db_create_reply *));
void __db_del_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __db_del_reply *));
void __db_encrypt_proc __P((long, char *, u_int32_t, __db_encrypt_reply *));
void __db_extentsize_proc __P((long, u_int32_t, __db_extentsize_reply *));
void __db_flags_proc __P((long, u_int32_t, __db_flags_reply *));
void __db_get_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __db_get_reply *, int *));
void __db_h_ffactor_proc __P((long, u_int32_t, __db_h_ffactor_reply *));
void __db_h_nelem_proc __P((long, u_int32_t, __db_h_nelem_reply *));
void __db_key_range_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __db_key_range_reply *));
void __db_lorder_proc __P((long, u_int32_t, __db_lorder_reply *));
void __db_open_proc __P((long, long, char *, char *, u_int32_t, u_int32_t, u_int32_t, __db_open_reply *));
void __db_pagesize_proc __P((long, u_int32_t, __db_pagesize_reply *));
void __db_pget_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __db_pget_reply *, int *));
void __db_put_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __db_put_reply *, int *));
void __db_re_delim_proc __P((long, u_int32_t, __db_re_delim_reply *));
void __db_re_len_proc __P((long, u_int32_t, __db_re_len_reply *));
void __db_re_pad_proc __P((long, u_int32_t, __db_re_pad_reply *));
void __db_remove_proc __P((long, char *, char *, u_int32_t, __db_remove_reply *));
void __db_rename_proc __P((long, char *, char *, char *, u_int32_t, __db_rename_reply *));
void __db_stat_proc __P((long, u_int32_t, __db_stat_reply *, int *));
void __db_sync_proc __P((long, u_int32_t, __db_sync_reply *));
void __db_truncate_proc __P((long, long, u_int32_t, __db_truncate_reply *));
void __db_cursor_proc __P((long, long, u_int32_t, __db_cursor_reply *));
void __db_join_proc __P((long, u_int32_t *, u_int32_t, u_int32_t, __db_join_reply *));
void __dbc_close_proc __P((long, __dbc_close_reply *));
void __dbc_count_proc __P((long, u_int32_t, __dbc_count_reply *));
void __dbc_del_proc __P((long, u_int32_t, __dbc_del_reply *));
void __dbc_dup_proc __P((long, u_int32_t, __dbc_dup_reply *));
void __dbc_get_proc __P((long, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __dbc_get_reply *, int *));
void __dbc_pget_proc __P((long, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __dbc_pget_reply *, int *));
void __dbc_put_proc __P((long, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __dbc_put_reply *, int *));
void __dbsrv_settimeout __P((ct_entry *, u_int32_t));
void __dbsrv_timeout __P((int));
void __dbclear_ctp __P((ct_entry *));
void __dbdel_ctp __P((ct_entry *));
ct_entry *new_ct_ent __P((int *));
ct_entry *get_tableent __P((long));
ct_entry *__dbsrv_sharedb __P((ct_entry *, const char *, const char *, DBTYPE, u_int32_t));
ct_entry *__dbsrv_shareenv __P((ct_entry *, home_entry *, u_int32_t));
void __dbsrv_active __P((ct_entry *));
int __db_close_int __P((long, u_int32_t));
int __dbc_close_int __P((ct_entry *));
int __dbenv_close_int __P((long, u_int32_t, int));
home_entry *get_home __P((char *));
__env_cachesize_reply *__db_env_cachesize_4001  __P((__env_cachesize_msg *, struct svc_req *));
__env_close_reply *__db_env_close_4001 __P((__env_close_msg *, struct svc_req *));
__env_create_reply *__db_env_create_4001 __P((__env_create_msg *, struct svc_req *));
__env_dbremove_reply *__db_env_dbremove_4001  __P((__env_dbremove_msg *, struct svc_req *));
__env_dbrename_reply *__db_env_dbrename_4001  __P((__env_dbrename_msg *, struct svc_req *));
__env_encrypt_reply *__db_env_encrypt_4001  __P((__env_encrypt_msg *, struct svc_req *));
__env_flags_reply *__db_env_flags_4001 __P((__env_flags_msg *, struct svc_req *));
__env_open_reply *__db_env_open_4001 __P((__env_open_msg *, struct svc_req *));
__env_remove_reply *__db_env_remove_4001 __P((__env_remove_msg *, struct svc_req *));
__txn_abort_reply *__db_txn_abort_4001 __P((__txn_abort_msg *, struct svc_req *));
__txn_begin_reply *__db_txn_begin_4001 __P((__txn_begin_msg *, struct svc_req *));
__txn_commit_reply *__db_txn_commit_4001 __P((__txn_commit_msg *, struct svc_req *));
__txn_discard_reply *__db_txn_discard_4001  __P((__txn_discard_msg *, struct svc_req *));
__txn_prepare_reply *__db_txn_prepare_4001  __P((__txn_prepare_msg *, struct svc_req *));
__txn_recover_reply *__db_txn_recover_4001  __P((__txn_recover_msg *, struct svc_req *));
__db_associate_reply *__db_db_associate_4001  __P((__db_associate_msg *, struct svc_req *));
__db_bt_maxkey_reply *__db_db_bt_maxkey_4001  __P((__db_bt_maxkey_msg *, struct svc_req *));
__db_bt_minkey_reply *__db_db_bt_minkey_4001  __P((__db_bt_minkey_msg *, struct svc_req *));
__db_close_reply *__db_db_close_4001 __P((__db_close_msg *, struct svc_req *));
__db_create_reply *__db_db_create_4001 __P((__db_create_msg *, struct svc_req *));
__db_del_reply *__db_db_del_4001 __P((__db_del_msg *, struct svc_req *));
__db_encrypt_reply *__db_db_encrypt_4001 __P((__db_encrypt_msg *, struct svc_req *));
__db_extentsize_reply *__db_db_extentsize_4001  __P((__db_extentsize_msg *, struct svc_req *));
__db_flags_reply *__db_db_flags_4001 __P((__db_flags_msg *, struct svc_req *));
__db_get_reply *__db_db_get_4001 __P((__db_get_msg *, struct svc_req *));
__db_h_ffactor_reply *__db_db_h_ffactor_4001  __P((__db_h_ffactor_msg *, struct svc_req *));
__db_h_nelem_reply *__db_db_h_nelem_4001 __P((__db_h_nelem_msg *, struct svc_req *));
__db_key_range_reply *__db_db_key_range_4001  __P((__db_key_range_msg *, struct svc_req *));
__db_lorder_reply *__db_db_lorder_4001 __P((__db_lorder_msg *, struct svc_req *));
__db_open_reply *__db_db_open_4001 __P((__db_open_msg *, struct svc_req *));
__db_pagesize_reply *__db_db_pagesize_4001  __P((__db_pagesize_msg *, struct svc_req *));
__db_pget_reply *__db_db_pget_4001 __P((__db_pget_msg *, struct svc_req *));
__db_put_reply *__db_db_put_4001 __P((__db_put_msg *, struct svc_req *));
__db_re_delim_reply *__db_db_re_delim_4001  __P((__db_re_delim_msg *, struct svc_req *));
__db_re_len_reply *__db_db_re_len_4001 __P((__db_re_len_msg *, struct svc_req *));
__db_re_pad_reply *__db_db_re_pad_4001 __P((__db_re_pad_msg *, struct svc_req *));
__db_remove_reply *__db_db_remove_4001 __P((__db_remove_msg *, struct svc_req *));
__db_rename_reply *__db_db_rename_4001 __P((__db_rename_msg *, struct svc_req *));
__db_stat_reply *__db_db_stat_4001 __P((__db_stat_msg *, struct svc_req *));
__db_sync_reply *__db_db_sync_4001 __P((__db_sync_msg *, struct svc_req *));
__db_truncate_reply *__db_db_truncate_4001  __P((__db_truncate_msg *, struct svc_req *));
__db_cursor_reply *__db_db_cursor_4001 __P((__db_cursor_msg *, struct svc_req *));
__db_join_reply *__db_db_join_4001 __P((__db_join_msg *, struct svc_req *));
__dbc_close_reply *__db_dbc_close_4001 __P((__dbc_close_msg *, struct svc_req *));
__dbc_count_reply *__db_dbc_count_4001 __P((__dbc_count_msg *, struct svc_req *));
__dbc_del_reply *__db_dbc_del_4001 __P((__dbc_del_msg *, struct svc_req *));
__dbc_dup_reply *__db_dbc_dup_4001 __P((__dbc_dup_msg *, struct svc_req *));
__dbc_get_reply *__db_dbc_get_4001 __P((__dbc_get_msg *, struct svc_req *));
__dbc_pget_reply *__db_dbc_pget_4001 __P((__dbc_pget_msg *, struct svc_req *));
__dbc_put_reply *__db_dbc_put_4001 __P((__dbc_put_msg *, struct svc_req *));

#if defined(__cplusplus)
}
#endif
#endif /* !_rpc_server_ext_h_ */
