/* Do not edit: automatically built by gen_rpc.awk. */
__env_cachesize_reply * __db_env_cachesize_1 __P((__env_cachesize_msg *));
void __env_cachesize_1_proc __P((long, u_int32_t, u_int32_t,
	u_int32_t, __env_cachesize_reply *));
__env_close_reply * __db_env_close_1 __P((__env_close_msg *));
void __env_close_1_proc __P((long, u_int32_t, __env_close_reply *));
__env_create_reply * __db_env_create_1 __P((__env_create_msg *));
void __env_create_1_proc __P((u_int32_t, __env_create_reply *));
__env_flags_reply * __db_env_flags_1 __P((__env_flags_msg *));
void __env_flags_1_proc __P((long, u_int32_t, u_int32_t, __env_flags_reply *));
__env_open_reply * __db_env_open_1 __P((__env_open_msg *));
void __env_open_1_proc __P((long, char *, u_int32_t,
	u_int32_t, __env_open_reply *));
__env_remove_reply * __db_env_remove_1 __P((__env_remove_msg *));
void __env_remove_1_proc __P((long, char *, u_int32_t, __env_remove_reply *));
__txn_abort_reply * __db_txn_abort_1 __P((__txn_abort_msg *));
void __txn_abort_1_proc __P((long, __txn_abort_reply *));
__txn_begin_reply * __db_txn_begin_1 __P((__txn_begin_msg *));
void __txn_begin_1_proc __P((long, long,
	u_int32_t, __txn_begin_reply *));
__txn_commit_reply * __db_txn_commit_1 __P((__txn_commit_msg *));
void __txn_commit_1_proc __P((long, u_int32_t, __txn_commit_reply *));
__db_bt_maxkey_reply * __db_db_bt_maxkey_1 __P((__db_bt_maxkey_msg *));
void __db_bt_maxkey_1_proc __P((long, u_int32_t, __db_bt_maxkey_reply *));
__db_bt_minkey_reply * __db_db_bt_minkey_1 __P((__db_bt_minkey_msg *));
void __db_bt_minkey_1_proc __P((long, u_int32_t, __db_bt_minkey_reply *));
__db_close_reply * __db_db_close_1 __P((__db_close_msg *));
void __db_close_1_proc __P((long, u_int32_t, __db_close_reply *));
__db_create_reply * __db_db_create_1 __P((__db_create_msg *));
void __db_create_1_proc __P((u_int32_t, long, __db_create_reply *));
__db_del_reply * __db_db_del_1 __P((__db_del_msg *));
void __db_del_1_proc __P((long, long, u_int32_t,
	u_int32_t, u_int32_t, void *, u_int32_t,
	u_int32_t, __db_del_reply *));
__db_extentsize_reply * __db_db_extentsize_1 __P((__db_extentsize_msg *));
void __db_extentsize_1_proc __P((long, u_int32_t, __db_extentsize_reply *));
__db_flags_reply * __db_db_flags_1 __P((__db_flags_msg *));
void __db_flags_1_proc __P((long, u_int32_t, __db_flags_reply *));
__db_get_reply * __db_db_get_1 __P((__db_get_msg *));
void __db_get_1_proc __P((long, long, u_int32_t,
	u_int32_t, u_int32_t, void *, u_int32_t,
	u_int32_t, u_int32_t, u_int32_t, void *,
	u_int32_t, u_int32_t, __db_get_reply *, int *));
__db_h_ffactor_reply * __db_db_h_ffactor_1 __P((__db_h_ffactor_msg *));
void __db_h_ffactor_1_proc __P((long, u_int32_t, __db_h_ffactor_reply *));
__db_h_nelem_reply * __db_db_h_nelem_1 __P((__db_h_nelem_msg *));
void __db_h_nelem_1_proc __P((long, u_int32_t, __db_h_nelem_reply *));
__db_key_range_reply * __db_db_key_range_1 __P((__db_key_range_msg *));
void __db_key_range_1_proc __P((long, long, u_int32_t,
	u_int32_t, u_int32_t, void *, u_int32_t,
	u_int32_t, __db_key_range_reply *));
__db_lorder_reply * __db_db_lorder_1 __P((__db_lorder_msg *));
void __db_lorder_1_proc __P((long, u_int32_t, __db_lorder_reply *));
__db_open_reply * __db_db_open_1 __P((__db_open_msg *));
void __db_open_1_proc __P((long, char *, char *,
	u_int32_t, u_int32_t, u_int32_t, __db_open_reply *));
__db_pagesize_reply * __db_db_pagesize_1 __P((__db_pagesize_msg *));
void __db_pagesize_1_proc __P((long, u_int32_t, __db_pagesize_reply *));
__db_put_reply * __db_db_put_1 __P((__db_put_msg *));
void __db_put_1_proc __P((long, long, u_int32_t,
	u_int32_t, u_int32_t, void *, u_int32_t,
	u_int32_t, u_int32_t, u_int32_t, void *,
	u_int32_t, u_int32_t, __db_put_reply *, int *));
__db_re_delim_reply * __db_db_re_delim_1 __P((__db_re_delim_msg *));
void __db_re_delim_1_proc __P((long, u_int32_t, __db_re_delim_reply *));
__db_re_len_reply * __db_db_re_len_1 __P((__db_re_len_msg *));
void __db_re_len_1_proc __P((long, u_int32_t, __db_re_len_reply *));
__db_re_pad_reply * __db_db_re_pad_1 __P((__db_re_pad_msg *));
void __db_re_pad_1_proc __P((long, u_int32_t, __db_re_pad_reply *));
__db_remove_reply * __db_db_remove_1 __P((__db_remove_msg *));
void __db_remove_1_proc __P((long, char *, char *,
	u_int32_t, __db_remove_reply *));
__db_rename_reply * __db_db_rename_1 __P((__db_rename_msg *));
void __db_rename_1_proc __P((long, char *, char *,
	char *, u_int32_t, __db_rename_reply *));
__db_stat_reply * __db_db_stat_1 __P((__db_stat_msg *));
void __db_stat_1_proc __P((long,
	u_int32_t, __db_stat_reply *, int *));
__db_swapped_reply * __db_db_swapped_1 __P((__db_swapped_msg *));
void __db_swapped_1_proc __P((long, __db_swapped_reply *));
__db_sync_reply * __db_db_sync_1 __P((__db_sync_msg *));
void __db_sync_1_proc __P((long, u_int32_t, __db_sync_reply *));
__db_cursor_reply * __db_db_cursor_1 __P((__db_cursor_msg *));
void __db_cursor_1_proc __P((long, long,
	u_int32_t, __db_cursor_reply *));
__db_join_reply * __db_db_join_1 __P((__db_join_msg *));
void __db_join_1_proc __P((long, u_int32_t *,
	u_int32_t, __db_join_reply *));
__dbc_close_reply * __db_dbc_close_1 __P((__dbc_close_msg *));
void __dbc_close_1_proc __P((long, __dbc_close_reply *));
__dbc_count_reply * __db_dbc_count_1 __P((__dbc_count_msg *));
void __dbc_count_1_proc __P((long, u_int32_t, __dbc_count_reply *));
__dbc_del_reply * __db_dbc_del_1 __P((__dbc_del_msg *));
void __dbc_del_1_proc __P((long, u_int32_t, __dbc_del_reply *));
__dbc_dup_reply * __db_dbc_dup_1 __P((__dbc_dup_msg *));
void __dbc_dup_1_proc __P((long, u_int32_t, __dbc_dup_reply *));
__dbc_get_reply * __db_dbc_get_1 __P((__dbc_get_msg *));
void __dbc_get_1_proc __P((long, u_int32_t, u_int32_t,
	u_int32_t, void *, u_int32_t, u_int32_t,
	u_int32_t, u_int32_t, void *, u_int32_t,
	u_int32_t, __dbc_get_reply *, int *));
__dbc_put_reply * __db_dbc_put_1 __P((__dbc_put_msg *));
void __dbc_put_1_proc __P((long, u_int32_t, u_int32_t,
	u_int32_t, void *, u_int32_t, u_int32_t,
	u_int32_t, u_int32_t, void *, u_int32_t,
	u_int32_t, __dbc_put_reply *, int *));
