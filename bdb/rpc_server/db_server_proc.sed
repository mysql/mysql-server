/^\/\* BEGIN __env_cachesize_1_proc/,/^\/\* END __env_cachesize_1_proc/c\
/* BEGIN __env_cachesize_1_proc */\
void\
__env_cachesize_1_proc(dbenvcl_id, gbytes, bytes,\
\	\	ncache, replyp)\
\	long dbenvcl_id;\
\	u_int32_t gbytes;\
\	u_int32_t bytes;\
\	u_int32_t ncache;\
\	__env_cachesize_reply *replyp;\
/* END __env_cachesize_1_proc */
/^\/\* BEGIN __env_close_1_proc/,/^\/\* END __env_close_1_proc/c\
/* BEGIN __env_close_1_proc */\
void\
__env_close_1_proc(dbenvcl_id, flags, replyp)\
\	long dbenvcl_id;\
\	u_int32_t flags;\
\	__env_close_reply *replyp;\
/* END __env_close_1_proc */
/^\/\* BEGIN __env_create_1_proc/,/^\/\* END __env_create_1_proc/c\
/* BEGIN __env_create_1_proc */\
void\
__env_create_1_proc(timeout, replyp)\
\	u_int32_t timeout;\
\	__env_create_reply *replyp;\
/* END __env_create_1_proc */
/^\/\* BEGIN __env_flags_1_proc/,/^\/\* END __env_flags_1_proc/c\
/* BEGIN __env_flags_1_proc */\
void\
__env_flags_1_proc(dbenvcl_id, flags, onoff, replyp)\
\	long dbenvcl_id;\
\	u_int32_t flags;\
\	u_int32_t onoff;\
\	__env_flags_reply *replyp;\
/* END __env_flags_1_proc */
/^\/\* BEGIN __env_open_1_proc/,/^\/\* END __env_open_1_proc/c\
/* BEGIN __env_open_1_proc */\
void\
__env_open_1_proc(dbenvcl_id, home, flags,\
\	\	mode, replyp)\
\	long dbenvcl_id;\
\	char *home;\
\	u_int32_t flags;\
\	u_int32_t mode;\
\	__env_open_reply *replyp;\
/* END __env_open_1_proc */
/^\/\* BEGIN __env_remove_1_proc/,/^\/\* END __env_remove_1_proc/c\
/* BEGIN __env_remove_1_proc */\
void\
__env_remove_1_proc(dbenvcl_id, home, flags, replyp)\
\	long dbenvcl_id;\
\	char *home;\
\	u_int32_t flags;\
\	__env_remove_reply *replyp;\
/* END __env_remove_1_proc */
/^\/\* BEGIN __txn_abort_1_proc/,/^\/\* END __txn_abort_1_proc/c\
/* BEGIN __txn_abort_1_proc */\
void\
__txn_abort_1_proc(txnpcl_id, replyp)\
\	long txnpcl_id;\
\	__txn_abort_reply *replyp;\
/* END __txn_abort_1_proc */
/^\/\* BEGIN __txn_begin_1_proc/,/^\/\* END __txn_begin_1_proc/c\
/* BEGIN __txn_begin_1_proc */\
void\
__txn_begin_1_proc(envpcl_id, parentcl_id,\
\	\	flags, replyp)\
\	long envpcl_id;\
\	long parentcl_id;\
\	u_int32_t flags;\
\	__txn_begin_reply *replyp;\
/* END __txn_begin_1_proc */
/^\/\* BEGIN __txn_commit_1_proc/,/^\/\* END __txn_commit_1_proc/c\
/* BEGIN __txn_commit_1_proc */\
void\
__txn_commit_1_proc(txnpcl_id, flags, replyp)\
\	long txnpcl_id;\
\	u_int32_t flags;\
\	__txn_commit_reply *replyp;\
/* END __txn_commit_1_proc */
/^\/\* BEGIN __db_bt_maxkey_1_proc/,/^\/\* END __db_bt_maxkey_1_proc/c\
/* BEGIN __db_bt_maxkey_1_proc */\
void\
__db_bt_maxkey_1_proc(dbpcl_id, maxkey, replyp)\
\	long dbpcl_id;\
\	u_int32_t maxkey;\
\	__db_bt_maxkey_reply *replyp;\
/* END __db_bt_maxkey_1_proc */
/^\/\* BEGIN __db_bt_minkey_1_proc/,/^\/\* END __db_bt_minkey_1_proc/c\
/* BEGIN __db_bt_minkey_1_proc */\
void\
__db_bt_minkey_1_proc(dbpcl_id, minkey, replyp)\
\	long dbpcl_id;\
\	u_int32_t minkey;\
\	__db_bt_minkey_reply *replyp;\
/* END __db_bt_minkey_1_proc */
/^\/\* BEGIN __db_close_1_proc/,/^\/\* END __db_close_1_proc/c\
/* BEGIN __db_close_1_proc */\
void\
__db_close_1_proc(dbpcl_id, flags, replyp)\
\	long dbpcl_id;\
\	u_int32_t flags;\
\	__db_close_reply *replyp;\
/* END __db_close_1_proc */
/^\/\* BEGIN __db_create_1_proc/,/^\/\* END __db_create_1_proc/c\
/* BEGIN __db_create_1_proc */\
void\
__db_create_1_proc(flags, envpcl_id, replyp)\
\	u_int32_t flags;\
\	long envpcl_id;\
\	__db_create_reply *replyp;\
/* END __db_create_1_proc */
/^\/\* BEGIN __db_del_1_proc/,/^\/\* END __db_del_1_proc/c\
/* BEGIN __db_del_1_proc */\
void\
__db_del_1_proc(dbpcl_id, txnpcl_id, keydlen,\
\	\	keydoff, keyflags, keydata, keysize,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t flags;\
\	__db_del_reply *replyp;\
/* END __db_del_1_proc */
/^\/\* BEGIN __db_extentsize_1_proc/,/^\/\* END __db_extentsize_1_proc/c\
/* BEGIN __db_extentsize_1_proc */\
void\
__db_extentsize_1_proc(dbpcl_id, extentsize, replyp)\
\	long dbpcl_id;\
\	u_int32_t extentsize;\
\	__db_extentsize_reply *replyp;\
/* END __db_extentsize_1_proc */
/^\/\* BEGIN __db_flags_1_proc/,/^\/\* END __db_flags_1_proc/c\
/* BEGIN __db_flags_1_proc */\
void\
__db_flags_1_proc(dbpcl_id, flags, replyp)\
\	long dbpcl_id;\
\	u_int32_t flags;\
\	__db_flags_reply *replyp;\
/* END __db_flags_1_proc */
/^\/\* BEGIN __db_get_1_proc/,/^\/\* END __db_get_1_proc/c\
/* BEGIN __db_get_1_proc */\
void\
__db_get_1_proc(dbpcl_id, txnpcl_id, keydlen,\
\	\	keydoff, keyflags, keydata, keysize,\
\	\	datadlen, datadoff, dataflags, datadata,\
\	\	datasize, flags, replyp, freep)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__db_get_reply *replyp;\
\	int * freep;\
/* END __db_get_1_proc */
/^\/\* BEGIN __db_h_ffactor_1_proc/,/^\/\* END __db_h_ffactor_1_proc/c\
/* BEGIN __db_h_ffactor_1_proc */\
void\
__db_h_ffactor_1_proc(dbpcl_id, ffactor, replyp)\
\	long dbpcl_id;\
\	u_int32_t ffactor;\
\	__db_h_ffactor_reply *replyp;\
/* END __db_h_ffactor_1_proc */
/^\/\* BEGIN __db_h_nelem_1_proc/,/^\/\* END __db_h_nelem_1_proc/c\
/* BEGIN __db_h_nelem_1_proc */\
void\
__db_h_nelem_1_proc(dbpcl_id, nelem, replyp)\
\	long dbpcl_id;\
\	u_int32_t nelem;\
\	__db_h_nelem_reply *replyp;\
/* END __db_h_nelem_1_proc */
/^\/\* BEGIN __db_key_range_1_proc/,/^\/\* END __db_key_range_1_proc/c\
/* BEGIN __db_key_range_1_proc */\
void\
__db_key_range_1_proc(dbpcl_id, txnpcl_id, keydlen,\
\	\	keydoff, keyflags, keydata, keysize,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t flags;\
\	__db_key_range_reply *replyp;\
/* END __db_key_range_1_proc */
/^\/\* BEGIN __db_lorder_1_proc/,/^\/\* END __db_lorder_1_proc/c\
/* BEGIN __db_lorder_1_proc */\
void\
__db_lorder_1_proc(dbpcl_id, lorder, replyp)\
\	long dbpcl_id;\
\	u_int32_t lorder;\
\	__db_lorder_reply *replyp;\
/* END __db_lorder_1_proc */
/^\/\* BEGIN __db_open_1_proc/,/^\/\* END __db_open_1_proc/c\
/* BEGIN __db_open_1_proc */\
void\
__db_open_1_proc(dbpcl_id, name, subdb,\
\	\	type, flags, mode, replyp)\
\	long dbpcl_id;\
\	char *name;\
\	char *subdb;\
\	u_int32_t type;\
\	u_int32_t flags;\
\	u_int32_t mode;\
\	__db_open_reply *replyp;\
/* END __db_open_1_proc */
/^\/\* BEGIN __db_pagesize_1_proc/,/^\/\* END __db_pagesize_1_proc/c\
/* BEGIN __db_pagesize_1_proc */\
void\
__db_pagesize_1_proc(dbpcl_id, pagesize, replyp)\
\	long dbpcl_id;\
\	u_int32_t pagesize;\
\	__db_pagesize_reply *replyp;\
/* END __db_pagesize_1_proc */
/^\/\* BEGIN __db_put_1_proc/,/^\/\* END __db_put_1_proc/c\
/* BEGIN __db_put_1_proc */\
void\
__db_put_1_proc(dbpcl_id, txnpcl_id, keydlen,\
\	\	keydoff, keyflags, keydata, keysize,\
\	\	datadlen, datadoff, dataflags, datadata,\
\	\	datasize, flags, replyp, freep)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__db_put_reply *replyp;\
\	int * freep;\
/* END __db_put_1_proc */
/^\/\* BEGIN __db_re_delim_1_proc/,/^\/\* END __db_re_delim_1_proc/c\
/* BEGIN __db_re_delim_1_proc */\
void\
__db_re_delim_1_proc(dbpcl_id, delim, replyp)\
\	long dbpcl_id;\
\	u_int32_t delim;\
\	__db_re_delim_reply *replyp;\
/* END __db_re_delim_1_proc */
/^\/\* BEGIN __db_re_len_1_proc/,/^\/\* END __db_re_len_1_proc/c\
/* BEGIN __db_re_len_1_proc */\
void\
__db_re_len_1_proc(dbpcl_id, len, replyp)\
\	long dbpcl_id;\
\	u_int32_t len;\
\	__db_re_len_reply *replyp;\
/* END __db_re_len_1_proc */
/^\/\* BEGIN __db_re_pad_1_proc/,/^\/\* END __db_re_pad_1_proc/c\
/* BEGIN __db_re_pad_1_proc */\
void\
__db_re_pad_1_proc(dbpcl_id, pad, replyp)\
\	long dbpcl_id;\
\	u_int32_t pad;\
\	__db_re_pad_reply *replyp;\
/* END __db_re_pad_1_proc */
/^\/\* BEGIN __db_remove_1_proc/,/^\/\* END __db_remove_1_proc/c\
/* BEGIN __db_remove_1_proc */\
void\
__db_remove_1_proc(dbpcl_id, name, subdb,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	char *name;\
\	char *subdb;\
\	u_int32_t flags;\
\	__db_remove_reply *replyp;\
/* END __db_remove_1_proc */
/^\/\* BEGIN __db_rename_1_proc/,/^\/\* END __db_rename_1_proc/c\
/* BEGIN __db_rename_1_proc */\
void\
__db_rename_1_proc(dbpcl_id, name, subdb,\
\	\	newname, flags, replyp)\
\	long dbpcl_id;\
\	char *name;\
\	char *subdb;\
\	char *newname;\
\	u_int32_t flags;\
\	__db_rename_reply *replyp;\
/* END __db_rename_1_proc */
/^\/\* BEGIN __db_stat_1_proc/,/^\/\* END __db_stat_1_proc/c\
/* BEGIN __db_stat_1_proc */\
void\
__db_stat_1_proc(dbpcl_id,\
\	\	flags, replyp, freep)\
\	long dbpcl_id;\
\	u_int32_t flags;\
\	__db_stat_reply *replyp;\
\	int * freep;\
/* END __db_stat_1_proc */
/^\/\* BEGIN __db_swapped_1_proc/,/^\/\* END __db_swapped_1_proc/c\
/* BEGIN __db_swapped_1_proc */\
void\
__db_swapped_1_proc(dbpcl_id, replyp)\
\	long dbpcl_id;\
\	__db_swapped_reply *replyp;\
/* END __db_swapped_1_proc */
/^\/\* BEGIN __db_sync_1_proc/,/^\/\* END __db_sync_1_proc/c\
/* BEGIN __db_sync_1_proc */\
void\
__db_sync_1_proc(dbpcl_id, flags, replyp)\
\	long dbpcl_id;\
\	u_int32_t flags;\
\	__db_sync_reply *replyp;\
/* END __db_sync_1_proc */
/^\/\* BEGIN __db_cursor_1_proc/,/^\/\* END __db_cursor_1_proc/c\
/* BEGIN __db_cursor_1_proc */\
void\
__db_cursor_1_proc(dbpcl_id, txnpcl_id,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t flags;\
\	__db_cursor_reply *replyp;\
/* END __db_cursor_1_proc */
/^\/\* BEGIN __db_join_1_proc/,/^\/\* END __db_join_1_proc/c\
/* BEGIN __db_join_1_proc */\
void\
__db_join_1_proc(dbpcl_id, curslist,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	u_int32_t * curslist;\
\	u_int32_t flags;\
\	__db_join_reply *replyp;\
/* END __db_join_1_proc */
/^\/\* BEGIN __dbc_close_1_proc/,/^\/\* END __dbc_close_1_proc/c\
/* BEGIN __dbc_close_1_proc */\
void\
__dbc_close_1_proc(dbccl_id, replyp)\
\	long dbccl_id;\
\	__dbc_close_reply *replyp;\
/* END __dbc_close_1_proc */
/^\/\* BEGIN __dbc_count_1_proc/,/^\/\* END __dbc_count_1_proc/c\
/* BEGIN __dbc_count_1_proc */\
void\
__dbc_count_1_proc(dbccl_id, flags, replyp)\
\	long dbccl_id;\
\	u_int32_t flags;\
\	__dbc_count_reply *replyp;\
/* END __dbc_count_1_proc */
/^\/\* BEGIN __dbc_del_1_proc/,/^\/\* END __dbc_del_1_proc/c\
/* BEGIN __dbc_del_1_proc */\
void\
__dbc_del_1_proc(dbccl_id, flags, replyp)\
\	long dbccl_id;\
\	u_int32_t flags;\
\	__dbc_del_reply *replyp;\
/* END __dbc_del_1_proc */
/^\/\* BEGIN __dbc_dup_1_proc/,/^\/\* END __dbc_dup_1_proc/c\
/* BEGIN __dbc_dup_1_proc */\
void\
__dbc_dup_1_proc(dbccl_id, flags, replyp)\
\	long dbccl_id;\
\	u_int32_t flags;\
\	__dbc_dup_reply *replyp;\
/* END __dbc_dup_1_proc */
/^\/\* BEGIN __dbc_get_1_proc/,/^\/\* END __dbc_get_1_proc/c\
/* BEGIN __dbc_get_1_proc */\
void\
__dbc_get_1_proc(dbccl_id, keydlen, keydoff,\
\	\	keyflags, keydata, keysize, datadlen,\
\	\	datadoff, dataflags, datadata, datasize,\
\	\	flags, replyp, freep)\
\	long dbccl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__dbc_get_reply *replyp;\
\	int * freep;\
/* END __dbc_get_1_proc */
/^\/\* BEGIN __dbc_put_1_proc/,/^\/\* END __dbc_put_1_proc/c\
/* BEGIN __dbc_put_1_proc */\
void\
__dbc_put_1_proc(dbccl_id, keydlen, keydoff,\
\	\	keyflags, keydata, keysize, datadlen,\
\	\	datadoff, dataflags, datadata, datasize,\
\	\	flags, replyp, freep)\
\	long dbccl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__dbc_put_reply *replyp;\
\	int * freep;\
/* END __dbc_put_1_proc */
