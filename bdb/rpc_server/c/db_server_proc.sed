/^\/\* BEGIN __env_cachesize_proc/,/^\/\* END __env_cachesize_proc/c\
/* BEGIN __env_cachesize_proc */\
/*\
\ * PUBLIC: void __env_cachesize_proc __P((long, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, __env_cachesize_reply *));\
\ */\
void\
__env_cachesize_proc(dbenvcl_id, gbytes, bytes,\
\	\	ncache, replyp)\
\	long dbenvcl_id;\
\	u_int32_t gbytes;\
\	u_int32_t bytes;\
\	u_int32_t ncache;\
\	__env_cachesize_reply *replyp;\
/* END __env_cachesize_proc */
/^\/\* BEGIN __env_close_proc/,/^\/\* END __env_close_proc/c\
/* BEGIN __env_close_proc */\
/*\
\ * PUBLIC: void __env_close_proc __P((long, u_int32_t, __env_close_reply *));\
\ */\
void\
__env_close_proc(dbenvcl_id, flags, replyp)\
\	long dbenvcl_id;\
\	u_int32_t flags;\
\	__env_close_reply *replyp;\
/* END __env_close_proc */
/^\/\* BEGIN __env_create_proc/,/^\/\* END __env_create_proc/c\
/* BEGIN __env_create_proc */\
/*\
\ * PUBLIC: void __env_create_proc __P((u_int32_t, __env_create_reply *));\
\ */\
void\
__env_create_proc(timeout, replyp)\
\	u_int32_t timeout;\
\	__env_create_reply *replyp;\
/* END __env_create_proc */
/^\/\* BEGIN __env_dbremove_proc/,/^\/\* END __env_dbremove_proc/c\
/* BEGIN __env_dbremove_proc */\
/*\
\ * PUBLIC: void __env_dbremove_proc __P((long, long, char *, char *, u_int32_t,\
\ * PUBLIC:      __env_dbremove_reply *));\
\ */\
void\
__env_dbremove_proc(dbenvcl_id, txnpcl_id, name,\
\	\	subdb, flags, replyp)\
\	long dbenvcl_id;\
\	long txnpcl_id;\
\	char *name;\
\	char *subdb;\
\	u_int32_t flags;\
\	__env_dbremove_reply *replyp;\
/* END __env_dbremove_proc */
/^\/\* BEGIN __env_dbrename_proc/,/^\/\* END __env_dbrename_proc/c\
/* BEGIN __env_dbrename_proc */\
/*\
\ * PUBLIC: void __env_dbrename_proc __P((long, long, char *, char *, char *,\
\ * PUBLIC:      u_int32_t, __env_dbrename_reply *));\
\ */\
void\
__env_dbrename_proc(dbenvcl_id, txnpcl_id, name,\
\	\	subdb, newname, flags, replyp)\
\	long dbenvcl_id;\
\	long txnpcl_id;\
\	char *name;\
\	char *subdb;\
\	char *newname;\
\	u_int32_t flags;\
\	__env_dbrename_reply *replyp;\
/* END __env_dbrename_proc */
/^\/\* BEGIN __env_encrypt_proc/,/^\/\* END __env_encrypt_proc/c\
/* BEGIN __env_encrypt_proc */\
/*\
\ * PUBLIC: void __env_encrypt_proc __P((long, char *, u_int32_t,\
\ * PUBLIC:      __env_encrypt_reply *));\
\ */\
void\
__env_encrypt_proc(dbenvcl_id, passwd, flags, replyp)\
\	long dbenvcl_id;\
\	char *passwd;\
\	u_int32_t flags;\
\	__env_encrypt_reply *replyp;\
/* END __env_encrypt_proc */
/^\/\* BEGIN __env_flags_proc/,/^\/\* END __env_flags_proc/c\
/* BEGIN __env_flags_proc */\
/*\
\ * PUBLIC: void __env_flags_proc __P((long, u_int32_t, u_int32_t,\
\ * PUBLIC:      __env_flags_reply *));\
\ */\
void\
__env_flags_proc(dbenvcl_id, flags, onoff, replyp)\
\	long dbenvcl_id;\
\	u_int32_t flags;\
\	u_int32_t onoff;\
\	__env_flags_reply *replyp;\
/* END __env_flags_proc */
/^\/\* BEGIN __env_open_proc/,/^\/\* END __env_open_proc/c\
/* BEGIN __env_open_proc */\
/*\
\ * PUBLIC: void __env_open_proc __P((long, char *, u_int32_t, u_int32_t,\
\ * PUBLIC:      __env_open_reply *));\
\ */\
void\
__env_open_proc(dbenvcl_id, home, flags,\
\	\	mode, replyp)\
\	long dbenvcl_id;\
\	char *home;\
\	u_int32_t flags;\
\	u_int32_t mode;\
\	__env_open_reply *replyp;\
/* END __env_open_proc */
/^\/\* BEGIN __env_remove_proc/,/^\/\* END __env_remove_proc/c\
/* BEGIN __env_remove_proc */\
/*\
\ * PUBLIC: void __env_remove_proc __P((long, char *, u_int32_t,\
\ * PUBLIC:      __env_remove_reply *));\
\ */\
void\
__env_remove_proc(dbenvcl_id, home, flags, replyp)\
\	long dbenvcl_id;\
\	char *home;\
\	u_int32_t flags;\
\	__env_remove_reply *replyp;\
/* END __env_remove_proc */
/^\/\* BEGIN __txn_abort_proc/,/^\/\* END __txn_abort_proc/c\
/* BEGIN __txn_abort_proc */\
/*\
\ * PUBLIC: void __txn_abort_proc __P((long, __txn_abort_reply *));\
\ */\
void\
__txn_abort_proc(txnpcl_id, replyp)\
\	long txnpcl_id;\
\	__txn_abort_reply *replyp;\
/* END __txn_abort_proc */
/^\/\* BEGIN __txn_begin_proc/,/^\/\* END __txn_begin_proc/c\
/* BEGIN __txn_begin_proc */\
/*\
\ * PUBLIC: void __txn_begin_proc __P((long, long, u_int32_t,\
\ * PUBLIC:      __txn_begin_reply *));\
\ */\
void\
__txn_begin_proc(dbenvcl_id, parentcl_id,\
\	\	flags, replyp)\
\	long dbenvcl_id;\
\	long parentcl_id;\
\	u_int32_t flags;\
\	__txn_begin_reply *replyp;\
/* END __txn_begin_proc */
/^\/\* BEGIN __txn_commit_proc/,/^\/\* END __txn_commit_proc/c\
/* BEGIN __txn_commit_proc */\
/*\
\ * PUBLIC: void __txn_commit_proc __P((long, u_int32_t,\
\ * PUBLIC:      __txn_commit_reply *));\
\ */\
void\
__txn_commit_proc(txnpcl_id, flags, replyp)\
\	long txnpcl_id;\
\	u_int32_t flags;\
\	__txn_commit_reply *replyp;\
/* END __txn_commit_proc */
/^\/\* BEGIN __txn_discard_proc/,/^\/\* END __txn_discard_proc/c\
/* BEGIN __txn_discard_proc */\
/*\
\ * PUBLIC: void __txn_discard_proc __P((long, u_int32_t,\
\ * PUBLIC:      __txn_discard_reply *));\
\ */\
void\
__txn_discard_proc(txnpcl_id, flags, replyp)\
\	long txnpcl_id;\
\	u_int32_t flags;\
\	__txn_discard_reply *replyp;\
/* END __txn_discard_proc */
/^\/\* BEGIN __txn_prepare_proc/,/^\/\* END __txn_prepare_proc/c\
/* BEGIN __txn_prepare_proc */\
/*\
\ * PUBLIC: void __txn_prepare_proc __P((long, u_int8_t *,\
\ * PUBLIC:      __txn_prepare_reply *));\
\ */\
void\
__txn_prepare_proc(txnpcl_id, gid, replyp)\
\	long txnpcl_id;\
\	u_int8_t *gid;\
\	__txn_prepare_reply *replyp;\
/* END __txn_prepare_proc */
/^\/\* BEGIN __txn_recover_proc/,/^\/\* END __txn_recover_proc/c\
/* BEGIN __txn_recover_proc */\
/*\
\ * PUBLIC: void __txn_recover_proc __P((long, u_int32_t, u_int32_t,\
\ * PUBLIC:      __txn_recover_reply *, int *));\
\ */\
void\
__txn_recover_proc(dbenvcl_id, count,\
\	\	flags, replyp, freep)\
\	long dbenvcl_id;\
\	u_int32_t count;\
\	u_int32_t flags;\
\	__txn_recover_reply *replyp;\
\	int * freep;\
/* END __txn_recover_proc */
/^\/\* BEGIN __db_associate_proc/,/^\/\* END __db_associate_proc/c\
/* BEGIN __db_associate_proc */\
/*\
\ * PUBLIC: void __db_associate_proc __P((long, long, long, u_int32_t,\
\ * PUBLIC:      __db_associate_reply *));\
\ */\
void\
__db_associate_proc(dbpcl_id, txnpcl_id, sdbpcl_id,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	long sdbpcl_id;\
\	u_int32_t flags;\
\	__db_associate_reply *replyp;\
/* END __db_associate_proc */
/^\/\* BEGIN __db_bt_maxkey_proc/,/^\/\* END __db_bt_maxkey_proc/c\
/* BEGIN __db_bt_maxkey_proc */\
/*\
\ * PUBLIC: void __db_bt_maxkey_proc __P((long, u_int32_t,\
\ * PUBLIC:      __db_bt_maxkey_reply *));\
\ */\
void\
__db_bt_maxkey_proc(dbpcl_id, maxkey, replyp)\
\	long dbpcl_id;\
\	u_int32_t maxkey;\
\	__db_bt_maxkey_reply *replyp;\
/* END __db_bt_maxkey_proc */
/^\/\* BEGIN __db_bt_minkey_proc/,/^\/\* END __db_bt_minkey_proc/c\
/* BEGIN __db_bt_minkey_proc */\
/*\
\ * PUBLIC: void __db_bt_minkey_proc __P((long, u_int32_t,\
\ * PUBLIC:      __db_bt_minkey_reply *));\
\ */\
void\
__db_bt_minkey_proc(dbpcl_id, minkey, replyp)\
\	long dbpcl_id;\
\	u_int32_t minkey;\
\	__db_bt_minkey_reply *replyp;\
/* END __db_bt_minkey_proc */
/^\/\* BEGIN __db_close_proc/,/^\/\* END __db_close_proc/c\
/* BEGIN __db_close_proc */\
/*\
\ * PUBLIC: void __db_close_proc __P((long, u_int32_t, __db_close_reply *));\
\ */\
void\
__db_close_proc(dbpcl_id, flags, replyp)\
\	long dbpcl_id;\
\	u_int32_t flags;\
\	__db_close_reply *replyp;\
/* END __db_close_proc */
/^\/\* BEGIN __db_create_proc/,/^\/\* END __db_create_proc/c\
/* BEGIN __db_create_proc */\
/*\
\ * PUBLIC: void __db_create_proc __P((long, u_int32_t, __db_create_reply *));\
\ */\
void\
__db_create_proc(dbenvcl_id, flags, replyp)\
\	long dbenvcl_id;\
\	u_int32_t flags;\
\	__db_create_reply *replyp;\
/* END __db_create_proc */
/^\/\* BEGIN __db_del_proc/,/^\/\* END __db_del_proc/c\
/* BEGIN __db_del_proc */\
/*\
\ * PUBLIC: void __db_del_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, void *, u_int32_t, u_int32_t, __db_del_reply *));\
\ */\
void\
__db_del_proc(dbpcl_id, txnpcl_id, keydlen,\
\	\	keydoff, keyulen, keyflags, keydata,\
\	\	keysize, flags, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyulen;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t flags;\
\	__db_del_reply *replyp;\
/* END __db_del_proc */
/^\/\* BEGIN __db_encrypt_proc/,/^\/\* END __db_encrypt_proc/c\
/* BEGIN __db_encrypt_proc */\
/*\
\ * PUBLIC: void __db_encrypt_proc __P((long, char *, u_int32_t,\
\ * PUBLIC:      __db_encrypt_reply *));\
\ */\
void\
__db_encrypt_proc(dbpcl_id, passwd, flags, replyp)\
\	long dbpcl_id;\
\	char *passwd;\
\	u_int32_t flags;\
\	__db_encrypt_reply *replyp;\
/* END __db_encrypt_proc */
/^\/\* BEGIN __db_extentsize_proc/,/^\/\* END __db_extentsize_proc/c\
/* BEGIN __db_extentsize_proc */\
/*\
\ * PUBLIC: void __db_extentsize_proc __P((long, u_int32_t,\
\ * PUBLIC:      __db_extentsize_reply *));\
\ */\
void\
__db_extentsize_proc(dbpcl_id, extentsize, replyp)\
\	long dbpcl_id;\
\	u_int32_t extentsize;\
\	__db_extentsize_reply *replyp;\
/* END __db_extentsize_proc */
/^\/\* BEGIN __db_flags_proc/,/^\/\* END __db_flags_proc/c\
/* BEGIN __db_flags_proc */\
/*\
\ * PUBLIC: void __db_flags_proc __P((long, u_int32_t, __db_flags_reply *));\
\ */\
void\
__db_flags_proc(dbpcl_id, flags, replyp)\
\	long dbpcl_id;\
\	u_int32_t flags;\
\	__db_flags_reply *replyp;\
/* END __db_flags_proc */
/^\/\* BEGIN __db_get_proc/,/^\/\* END __db_get_proc/c\
/* BEGIN __db_get_proc */\
/*\
\ * PUBLIC: void __db_get_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *,\
\ * PUBLIC:      u_int32_t, u_int32_t, __db_get_reply *, int *));\
\ */\
void\
__db_get_proc(dbpcl_id, txnpcl_id, keydlen,\
\	\	keydoff, keyulen, keyflags, keydata,\
\	\	keysize, datadlen, datadoff, dataulen,\
\	\	dataflags, datadata, datasize, flags, replyp, freep)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyulen;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataulen;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__db_get_reply *replyp;\
\	int * freep;\
/* END __db_get_proc */
/^\/\* BEGIN __db_h_ffactor_proc/,/^\/\* END __db_h_ffactor_proc/c\
/* BEGIN __db_h_ffactor_proc */\
/*\
\ * PUBLIC: void __db_h_ffactor_proc __P((long, u_int32_t,\
\ * PUBLIC:      __db_h_ffactor_reply *));\
\ */\
void\
__db_h_ffactor_proc(dbpcl_id, ffactor, replyp)\
\	long dbpcl_id;\
\	u_int32_t ffactor;\
\	__db_h_ffactor_reply *replyp;\
/* END __db_h_ffactor_proc */
/^\/\* BEGIN __db_h_nelem_proc/,/^\/\* END __db_h_nelem_proc/c\
/* BEGIN __db_h_nelem_proc */\
/*\
\ * PUBLIC: void __db_h_nelem_proc __P((long, u_int32_t,\
\ * PUBLIC:      __db_h_nelem_reply *));\
\ */\
void\
__db_h_nelem_proc(dbpcl_id, nelem, replyp)\
\	long dbpcl_id;\
\	u_int32_t nelem;\
\	__db_h_nelem_reply *replyp;\
/* END __db_h_nelem_proc */
/^\/\* BEGIN __db_key_range_proc/,/^\/\* END __db_key_range_proc/c\
/* BEGIN __db_key_range_proc */\
/*\
\ * PUBLIC: void __db_key_range_proc __P((long, long, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, __db_key_range_reply *));\
\ */\
void\
__db_key_range_proc(dbpcl_id, txnpcl_id, keydlen,\
\	\	keydoff, keyulen, keyflags, keydata,\
\	\	keysize, flags, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyulen;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t flags;\
\	__db_key_range_reply *replyp;\
/* END __db_key_range_proc */
/^\/\* BEGIN __db_lorder_proc/,/^\/\* END __db_lorder_proc/c\
/* BEGIN __db_lorder_proc */\
/*\
\ * PUBLIC: void __db_lorder_proc __P((long, u_int32_t, __db_lorder_reply *));\
\ */\
void\
__db_lorder_proc(dbpcl_id, lorder, replyp)\
\	long dbpcl_id;\
\	u_int32_t lorder;\
\	__db_lorder_reply *replyp;\
/* END __db_lorder_proc */
/^\/\* BEGIN __db_open_proc/,/^\/\* END __db_open_proc/c\
/* BEGIN __db_open_proc */\
/*\
\ * PUBLIC: void __db_open_proc __P((long, long, char *, char *, u_int32_t,\
\ * PUBLIC:      u_int32_t, u_int32_t, __db_open_reply *));\
\ */\
void\
__db_open_proc(dbpcl_id, txnpcl_id, name,\
\	\	subdb, type, flags, mode, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	char *name;\
\	char *subdb;\
\	u_int32_t type;\
\	u_int32_t flags;\
\	u_int32_t mode;\
\	__db_open_reply *replyp;\
/* END __db_open_proc */
/^\/\* BEGIN __db_pagesize_proc/,/^\/\* END __db_pagesize_proc/c\
/* BEGIN __db_pagesize_proc */\
/*\
\ * PUBLIC: void __db_pagesize_proc __P((long, u_int32_t,\
\ * PUBLIC:      __db_pagesize_reply *));\
\ */\
void\
__db_pagesize_proc(dbpcl_id, pagesize, replyp)\
\	long dbpcl_id;\
\	u_int32_t pagesize;\
\	__db_pagesize_reply *replyp;\
/* END __db_pagesize_proc */
/^\/\* BEGIN __db_pget_proc/,/^\/\* END __db_pget_proc/c\
/* BEGIN __db_pget_proc */\
/*\
\ * PUBLIC: void __db_pget_proc __P((long, long, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *,\
\ * PUBLIC:      u_int32_t, u_int32_t, __db_pget_reply *, int *));\
\ */\
void\
__db_pget_proc(dbpcl_id, txnpcl_id, skeydlen,\
\	\	skeydoff, skeyulen, skeyflags, skeydata,\
\	\	skeysize, pkeydlen, pkeydoff, pkeyulen,\
\	\	pkeyflags, pkeydata, pkeysize, datadlen,\
\	\	datadoff, dataulen, dataflags, datadata,\
\	\	datasize, flags, replyp, freep)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t skeydlen;\
\	u_int32_t skeydoff;\
\	u_int32_t skeyulen;\
\	u_int32_t skeyflags;\
\	void *skeydata;\
\	u_int32_t skeysize;\
\	u_int32_t pkeydlen;\
\	u_int32_t pkeydoff;\
\	u_int32_t pkeyulen;\
\	u_int32_t pkeyflags;\
\	void *pkeydata;\
\	u_int32_t pkeysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataulen;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__db_pget_reply *replyp;\
\	int * freep;\
/* END __db_pget_proc */
/^\/\* BEGIN __db_put_proc/,/^\/\* END __db_put_proc/c\
/* BEGIN __db_put_proc */\
/*\
\ * PUBLIC: void __db_put_proc __P((long, long, u_int32_t, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *,\
\ * PUBLIC:      u_int32_t, u_int32_t, __db_put_reply *, int *));\
\ */\
void\
__db_put_proc(dbpcl_id, txnpcl_id, keydlen,\
\	\	keydoff, keyulen, keyflags, keydata,\
\	\	keysize, datadlen, datadoff, dataulen,\
\	\	dataflags, datadata, datasize, flags, replyp, freep)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyulen;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataulen;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__db_put_reply *replyp;\
\	int * freep;\
/* END __db_put_proc */
/^\/\* BEGIN __db_re_delim_proc/,/^\/\* END __db_re_delim_proc/c\
/* BEGIN __db_re_delim_proc */\
/*\
\ * PUBLIC: void __db_re_delim_proc __P((long, u_int32_t,\
\ * PUBLIC:      __db_re_delim_reply *));\
\ */\
void\
__db_re_delim_proc(dbpcl_id, delim, replyp)\
\	long dbpcl_id;\
\	u_int32_t delim;\
\	__db_re_delim_reply *replyp;\
/* END __db_re_delim_proc */
/^\/\* BEGIN __db_re_len_proc/,/^\/\* END __db_re_len_proc/c\
/* BEGIN __db_re_len_proc */\
/*\
\ * PUBLIC: void __db_re_len_proc __P((long, u_int32_t, __db_re_len_reply *));\
\ */\
void\
__db_re_len_proc(dbpcl_id, len, replyp)\
\	long dbpcl_id;\
\	u_int32_t len;\
\	__db_re_len_reply *replyp;\
/* END __db_re_len_proc */
/^\/\* BEGIN __db_re_pad_proc/,/^\/\* END __db_re_pad_proc/c\
/* BEGIN __db_re_pad_proc */\
/*\
\ * PUBLIC: void __db_re_pad_proc __P((long, u_int32_t, __db_re_pad_reply *));\
\ */\
void\
__db_re_pad_proc(dbpcl_id, pad, replyp)\
\	long dbpcl_id;\
\	u_int32_t pad;\
\	__db_re_pad_reply *replyp;\
/* END __db_re_pad_proc */
/^\/\* BEGIN __db_remove_proc/,/^\/\* END __db_remove_proc/c\
/* BEGIN __db_remove_proc */\
/*\
\ * PUBLIC: void __db_remove_proc __P((long, char *, char *, u_int32_t,\
\ * PUBLIC:      __db_remove_reply *));\
\ */\
void\
__db_remove_proc(dbpcl_id, name, subdb,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	char *name;\
\	char *subdb;\
\	u_int32_t flags;\
\	__db_remove_reply *replyp;\
/* END __db_remove_proc */
/^\/\* BEGIN __db_rename_proc/,/^\/\* END __db_rename_proc/c\
/* BEGIN __db_rename_proc */\
/*\
\ * PUBLIC: void __db_rename_proc __P((long, char *, char *, char *, u_int32_t,\
\ * PUBLIC:      __db_rename_reply *));\
\ */\
void\
__db_rename_proc(dbpcl_id, name, subdb,\
\	\	newname, flags, replyp)\
\	long dbpcl_id;\
\	char *name;\
\	char *subdb;\
\	char *newname;\
\	u_int32_t flags;\
\	__db_rename_reply *replyp;\
/* END __db_rename_proc */
/^\/\* BEGIN __db_stat_proc/,/^\/\* END __db_stat_proc/c\
/* BEGIN __db_stat_proc */\
/*\
\ * PUBLIC: void __db_stat_proc __P((long, u_int32_t, __db_stat_reply *,\
\ * PUBLIC:      int *));\
\ */\
void\
__db_stat_proc(dbpcl_id, flags, replyp, freep)\
\	long dbpcl_id;\
\	u_int32_t flags;\
\	__db_stat_reply *replyp;\
\	int * freep;\
/* END __db_stat_proc */
/^\/\* BEGIN __db_sync_proc/,/^\/\* END __db_sync_proc/c\
/* BEGIN __db_sync_proc */\
/*\
\ * PUBLIC: void __db_sync_proc __P((long, u_int32_t, __db_sync_reply *));\
\ */\
void\
__db_sync_proc(dbpcl_id, flags, replyp)\
\	long dbpcl_id;\
\	u_int32_t flags;\
\	__db_sync_reply *replyp;\
/* END __db_sync_proc */
/^\/\* BEGIN __db_truncate_proc/,/^\/\* END __db_truncate_proc/c\
/* BEGIN __db_truncate_proc */\
/*\
\ * PUBLIC: void __db_truncate_proc __P((long, long, u_int32_t,\
\ * PUBLIC:      __db_truncate_reply *));\
\ */\
void\
__db_truncate_proc(dbpcl_id, txnpcl_id,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t flags;\
\	__db_truncate_reply *replyp;\
/* END __db_truncate_proc */
/^\/\* BEGIN __db_cursor_proc/,/^\/\* END __db_cursor_proc/c\
/* BEGIN __db_cursor_proc */\
/*\
\ * PUBLIC: void __db_cursor_proc __P((long, long, u_int32_t,\
\ * PUBLIC:      __db_cursor_reply *));\
\ */\
void\
__db_cursor_proc(dbpcl_id, txnpcl_id,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	long txnpcl_id;\
\	u_int32_t flags;\
\	__db_cursor_reply *replyp;\
/* END __db_cursor_proc */
/^\/\* BEGIN __db_join_proc/,/^\/\* END __db_join_proc/c\
/* BEGIN __db_join_proc */\
/*\
\ * PUBLIC: void __db_join_proc __P((long, u_int32_t *, u_int32_t, u_int32_t,\
\ * PUBLIC:      __db_join_reply *));\
\ */\
void\
__db_join_proc(dbpcl_id, curs, curslen,\
\	\	flags, replyp)\
\	long dbpcl_id;\
\	u_int32_t * curs;\
\	u_int32_t curslen;\
\	u_int32_t flags;\
\	__db_join_reply *replyp;\
/* END __db_join_proc */
/^\/\* BEGIN __dbc_close_proc/,/^\/\* END __dbc_close_proc/c\
/* BEGIN __dbc_close_proc */\
/*\
\ * PUBLIC: void __dbc_close_proc __P((long, __dbc_close_reply *));\
\ */\
void\
__dbc_close_proc(dbccl_id, replyp)\
\	long dbccl_id;\
\	__dbc_close_reply *replyp;\
/* END __dbc_close_proc */
/^\/\* BEGIN __dbc_count_proc/,/^\/\* END __dbc_count_proc/c\
/* BEGIN __dbc_count_proc */\
/*\
\ * PUBLIC: void __dbc_count_proc __P((long, u_int32_t, __dbc_count_reply *));\
\ */\
void\
__dbc_count_proc(dbccl_id, flags, replyp)\
\	long dbccl_id;\
\	u_int32_t flags;\
\	__dbc_count_reply *replyp;\
/* END __dbc_count_proc */
/^\/\* BEGIN __dbc_del_proc/,/^\/\* END __dbc_del_proc/c\
/* BEGIN __dbc_del_proc */\
/*\
\ * PUBLIC: void __dbc_del_proc __P((long, u_int32_t, __dbc_del_reply *));\
\ */\
void\
__dbc_del_proc(dbccl_id, flags, replyp)\
\	long dbccl_id;\
\	u_int32_t flags;\
\	__dbc_del_reply *replyp;\
/* END __dbc_del_proc */
/^\/\* BEGIN __dbc_dup_proc/,/^\/\* END __dbc_dup_proc/c\
/* BEGIN __dbc_dup_proc */\
/*\
\ * PUBLIC: void __dbc_dup_proc __P((long, u_int32_t, __dbc_dup_reply *));\
\ */\
void\
__dbc_dup_proc(dbccl_id, flags, replyp)\
\	long dbccl_id;\
\	u_int32_t flags;\
\	__dbc_dup_reply *replyp;\
/* END __dbc_dup_proc */
/^\/\* BEGIN __dbc_get_proc/,/^\/\* END __dbc_get_proc/c\
/* BEGIN __dbc_get_proc */\
/*\
\ * PUBLIC: void __dbc_get_proc __P((long, u_int32_t, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *,\
\ * PUBLIC:      u_int32_t, u_int32_t, __dbc_get_reply *, int *));\
\ */\
void\
__dbc_get_proc(dbccl_id, keydlen, keydoff,\
\	\	keyulen, keyflags, keydata, keysize,\
\	\	datadlen, datadoff, dataulen, dataflags,\
\	\	datadata, datasize, flags, replyp, freep)\
\	long dbccl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyulen;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataulen;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__dbc_get_reply *replyp;\
\	int * freep;\
/* END __dbc_get_proc */
/^\/\* BEGIN __dbc_pget_proc/,/^\/\* END __dbc_pget_proc/c\
/* BEGIN __dbc_pget_proc */\
/*\
\ * PUBLIC: void __dbc_pget_proc __P((long, u_int32_t, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *,\
\ * PUBLIC:      u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *, u_int32_t,\
\ * PUBLIC:      u_int32_t, __dbc_pget_reply *, int *));\
\ */\
void\
__dbc_pget_proc(dbccl_id, skeydlen, skeydoff,\
\	\	skeyulen, skeyflags, skeydata, skeysize,\
\	\	pkeydlen, pkeydoff, pkeyulen, pkeyflags,\
\	\	pkeydata, pkeysize, datadlen, datadoff,\
\	\	dataulen, dataflags, datadata, datasize,\
\	\	flags, replyp, freep)\
\	long dbccl_id;\
\	u_int32_t skeydlen;\
\	u_int32_t skeydoff;\
\	u_int32_t skeyulen;\
\	u_int32_t skeyflags;\
\	void *skeydata;\
\	u_int32_t skeysize;\
\	u_int32_t pkeydlen;\
\	u_int32_t pkeydoff;\
\	u_int32_t pkeyulen;\
\	u_int32_t pkeyflags;\
\	void *pkeydata;\
\	u_int32_t pkeysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataulen;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__dbc_pget_reply *replyp;\
\	int * freep;\
/* END __dbc_pget_proc */
/^\/\* BEGIN __dbc_put_proc/,/^\/\* END __dbc_put_proc/c\
/* BEGIN __dbc_put_proc */\
/*\
\ * PUBLIC: void __dbc_put_proc __P((long, u_int32_t, u_int32_t, u_int32_t,\
\ * PUBLIC:      u_int32_t, void *, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, void *,\
\ * PUBLIC:      u_int32_t, u_int32_t, __dbc_put_reply *, int *));\
\ */\
void\
__dbc_put_proc(dbccl_id, keydlen, keydoff,\
\	\	keyulen, keyflags, keydata, keysize,\
\	\	datadlen, datadoff, dataulen, dataflags,\
\	\	datadata, datasize, flags, replyp, freep)\
\	long dbccl_id;\
\	u_int32_t keydlen;\
\	u_int32_t keydoff;\
\	u_int32_t keyulen;\
\	u_int32_t keyflags;\
\	void *keydata;\
\	u_int32_t keysize;\
\	u_int32_t datadlen;\
\	u_int32_t datadoff;\
\	u_int32_t dataulen;\
\	u_int32_t dataflags;\
\	void *datadata;\
\	u_int32_t datasize;\
\	u_int32_t flags;\
\	__dbc_put_reply *replyp;\
\	int * freep;\
/* END __dbc_put_proc */
