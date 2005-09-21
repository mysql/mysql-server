/* Do not edit: automatically built by gen_rpc.awk. */

struct __env_get_cachesize_msg {
	unsigned int dbenvcl_id;
};

struct __env_get_cachesize_reply {
	/* num return vars: 3 */
	int status;
	unsigned int gbytes;
	unsigned int bytes;
	unsigned int ncache;
};

struct __env_cachesize_msg {
	unsigned int dbenvcl_id;
	unsigned int gbytes;
	unsigned int bytes;
	unsigned int ncache;
};

struct __env_cachesize_reply {
	/* num return vars: 0 */
	int status;
};

struct __env_close_msg {
	unsigned int dbenvcl_id;
	unsigned int flags;
};

struct __env_close_reply {
	/* num return vars: 0 */
	int status;
};

struct __env_create_msg {
	unsigned int timeout;
};

struct __env_create_reply {
	/* num return vars: 1 */
	int status;
	unsigned int envcl_id;
};

struct __env_dbremove_msg {
	unsigned int dbenvcl_id;
	unsigned int txnpcl_id;
	string name<>;
	string subdb<>;
	unsigned int flags;
};

struct __env_dbremove_reply {
	/* num return vars: 0 */
	int status;
};

struct __env_dbrename_msg {
	unsigned int dbenvcl_id;
	unsigned int txnpcl_id;
	string name<>;
	string subdb<>;
	string newname<>;
	unsigned int flags;
};

struct __env_dbrename_reply {
	/* num return vars: 0 */
	int status;
};

struct __env_get_encrypt_flags_msg {
	unsigned int dbenvcl_id;
};

struct __env_get_encrypt_flags_reply {
	/* num return vars: 1 */
	int status;
	unsigned int flags;
};

struct __env_encrypt_msg {
	unsigned int dbenvcl_id;
	string passwd<>;
	unsigned int flags;
};

struct __env_encrypt_reply {
	/* num return vars: 0 */
	int status;
};

struct __env_get_flags_msg {
	unsigned int dbenvcl_id;
};

struct __env_get_flags_reply {
	/* num return vars: 1 */
	int status;
	unsigned int flags;
};

struct __env_flags_msg {
	unsigned int dbenvcl_id;
	unsigned int flags;
	unsigned int onoff;
};

struct __env_flags_reply {
	/* num return vars: 0 */
	int status;
};

struct __env_get_home_msg {
	unsigned int dbenvcl_id;
};

struct __env_get_home_reply {
	/* num return vars: 1 */
	int status;
	string home<>;
};

struct __env_get_open_flags_msg {
	unsigned int dbenvcl_id;
};

struct __env_get_open_flags_reply {
	/* num return vars: 1 */
	int status;
	unsigned int flags;
};

struct __env_open_msg {
	unsigned int dbenvcl_id;
	string home<>;
	unsigned int flags;
	unsigned int mode;
};

struct __env_open_reply {
	/* num return vars: 1 */
	int status;
	unsigned int envcl_id;
};

struct __env_remove_msg {
	unsigned int dbenvcl_id;
	string home<>;
	unsigned int flags;
};

struct __env_remove_reply {
	/* num return vars: 0 */
	int status;
};

struct __txn_abort_msg {
	unsigned int txnpcl_id;
};

struct __txn_abort_reply {
	/* num return vars: 0 */
	int status;
};

struct __txn_begin_msg {
	unsigned int dbenvcl_id;
	unsigned int parentcl_id;
	unsigned int flags;
};

struct __txn_begin_reply {
	/* num return vars: 1 */
	int status;
	unsigned int txnidcl_id;
};

struct __txn_commit_msg {
	unsigned int txnpcl_id;
	unsigned int flags;
};

struct __txn_commit_reply {
	/* num return vars: 0 */
	int status;
};

struct __txn_discard_msg {
	unsigned int txnpcl_id;
	unsigned int flags;
};

struct __txn_discard_reply {
	/* num return vars: 0 */
	int status;
};

struct __txn_prepare_msg {
	unsigned int txnpcl_id;
	opaque gid[128];
};

struct __txn_prepare_reply {
	/* num return vars: 0 */
	int status;
};

struct __txn_recover_msg {
	unsigned int dbenvcl_id;
	unsigned int count;
	unsigned int flags;
};

struct __txn_recover_reply {
	/* num return vars: 3 */
	int status;
	unsigned int txn<>;
	opaque gid<>;
	unsigned int retcount;
};

struct __db_associate_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int sdbpcl_id;
	unsigned int flags;
};

struct __db_associate_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_bt_maxkey_msg {
	unsigned int dbpcl_id;
	unsigned int maxkey;
};

struct __db_bt_maxkey_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_get_bt_minkey_msg {
	unsigned int dbpcl_id;
};

struct __db_get_bt_minkey_reply {
	/* num return vars: 1 */
	int status;
	unsigned int minkey;
};

struct __db_bt_minkey_msg {
	unsigned int dbpcl_id;
	unsigned int minkey;
};

struct __db_bt_minkey_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_close_msg {
	unsigned int dbpcl_id;
	unsigned int flags;
};

struct __db_close_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_create_msg {
	unsigned int dbenvcl_id;
	unsigned int flags;
};

struct __db_create_reply {
	/* num return vars: 1 */
	int status;
	unsigned int dbcl_id;
};

struct __db_del_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int keydlen;
	unsigned int keydoff;
	unsigned int keyulen;
	unsigned int keyflags;
	opaque keydata<>;
	unsigned int flags;
};

struct __db_del_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_get_encrypt_flags_msg {
	unsigned int dbpcl_id;
};

struct __db_get_encrypt_flags_reply {
	/* num return vars: 1 */
	int status;
	unsigned int flags;
};

struct __db_encrypt_msg {
	unsigned int dbpcl_id;
	string passwd<>;
	unsigned int flags;
};

struct __db_encrypt_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_get_extentsize_msg {
	unsigned int dbpcl_id;
};

struct __db_get_extentsize_reply {
	/* num return vars: 1 */
	int status;
	unsigned int extentsize;
};

struct __db_extentsize_msg {
	unsigned int dbpcl_id;
	unsigned int extentsize;
};

struct __db_extentsize_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_get_flags_msg {
	unsigned int dbpcl_id;
};

struct __db_get_flags_reply {
	/* num return vars: 1 */
	int status;
	unsigned int flags;
};

struct __db_flags_msg {
	unsigned int dbpcl_id;
	unsigned int flags;
};

struct __db_flags_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_get_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int keydlen;
	unsigned int keydoff;
	unsigned int keyulen;
	unsigned int keyflags;
	opaque keydata<>;
	unsigned int datadlen;
	unsigned int datadoff;
	unsigned int dataulen;
	unsigned int dataflags;
	opaque datadata<>;
	unsigned int flags;
};

struct __db_get_reply {
	/* num return vars: 2 */
	int status;
	opaque keydata<>;
	opaque datadata<>;
};

struct __db_get_name_msg {
	unsigned int dbpcl_id;
};

struct __db_get_name_reply {
	/* num return vars: 2 */
	int status;
	string filename<>;
	string dbname<>;
};

struct __db_get_open_flags_msg {
	unsigned int dbpcl_id;
};

struct __db_get_open_flags_reply {
	/* num return vars: 1 */
	int status;
	unsigned int flags;
};

struct __db_get_h_ffactor_msg {
	unsigned int dbpcl_id;
};

struct __db_get_h_ffactor_reply {
	/* num return vars: 1 */
	int status;
	unsigned int ffactor;
};

struct __db_h_ffactor_msg {
	unsigned int dbpcl_id;
	unsigned int ffactor;
};

struct __db_h_ffactor_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_get_h_nelem_msg {
	unsigned int dbpcl_id;
};

struct __db_get_h_nelem_reply {
	/* num return vars: 1 */
	int status;
	unsigned int nelem;
};

struct __db_h_nelem_msg {
	unsigned int dbpcl_id;
	unsigned int nelem;
};

struct __db_h_nelem_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_key_range_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int keydlen;
	unsigned int keydoff;
	unsigned int keyulen;
	unsigned int keyflags;
	opaque keydata<>;
	unsigned int flags;
};

struct __db_key_range_reply {
	/* num return vars: 3 */
	int status;
	double less;
	double equal;
	double greater;
};

struct __db_get_lorder_msg {
	unsigned int dbpcl_id;
};

struct __db_get_lorder_reply {
	/* num return vars: 1 */
	int status;
	unsigned int lorder;
};

struct __db_lorder_msg {
	unsigned int dbpcl_id;
	unsigned int lorder;
};

struct __db_lorder_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_open_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	string name<>;
	string subdb<>;
	unsigned int type;
	unsigned int flags;
	unsigned int mode;
};

struct __db_open_reply {
	/* num return vars: 3 */
	int status;
	unsigned int dbcl_id;
	unsigned int type;
	unsigned int lorder;
};

struct __db_get_pagesize_msg {
	unsigned int dbpcl_id;
};

struct __db_get_pagesize_reply {
	/* num return vars: 1 */
	int status;
	unsigned int pagesize;
};

struct __db_pagesize_msg {
	unsigned int dbpcl_id;
	unsigned int pagesize;
};

struct __db_pagesize_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_pget_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int skeydlen;
	unsigned int skeydoff;
	unsigned int skeyulen;
	unsigned int skeyflags;
	opaque skeydata<>;
	unsigned int pkeydlen;
	unsigned int pkeydoff;
	unsigned int pkeyulen;
	unsigned int pkeyflags;
	opaque pkeydata<>;
	unsigned int datadlen;
	unsigned int datadoff;
	unsigned int dataulen;
	unsigned int dataflags;
	opaque datadata<>;
	unsigned int flags;
};

struct __db_pget_reply {
	/* num return vars: 3 */
	int status;
	opaque skeydata<>;
	opaque pkeydata<>;
	opaque datadata<>;
};

struct __db_put_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int keydlen;
	unsigned int keydoff;
	unsigned int keyulen;
	unsigned int keyflags;
	opaque keydata<>;
	unsigned int datadlen;
	unsigned int datadoff;
	unsigned int dataulen;
	unsigned int dataflags;
	opaque datadata<>;
	unsigned int flags;
};

struct __db_put_reply {
	/* num return vars: 1 */
	int status;
	opaque keydata<>;
};

struct __db_get_re_delim_msg {
	unsigned int dbpcl_id;
};

struct __db_get_re_delim_reply {
	/* num return vars: 1 */
	int status;
	unsigned int delim;
};

struct __db_re_delim_msg {
	unsigned int dbpcl_id;
	unsigned int delim;
};

struct __db_re_delim_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_get_re_len_msg {
	unsigned int dbpcl_id;
};

struct __db_get_re_len_reply {
	/* num return vars: 1 */
	int status;
	unsigned int len;
};

struct __db_re_len_msg {
	unsigned int dbpcl_id;
	unsigned int len;
};

struct __db_re_len_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_re_pad_msg {
	unsigned int dbpcl_id;
	unsigned int pad;
};

struct __db_re_pad_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_get_re_pad_msg {
	unsigned int dbpcl_id;
};

struct __db_get_re_pad_reply {
	/* num return vars: 1 */
	int status;
	unsigned int pad;
};

struct __db_remove_msg {
	unsigned int dbpcl_id;
	string name<>;
	string subdb<>;
	unsigned int flags;
};

struct __db_remove_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_rename_msg {
	unsigned int dbpcl_id;
	string name<>;
	string subdb<>;
	string newname<>;
	unsigned int flags;
};

struct __db_rename_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_stat_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int flags;
};

struct __db_stat_reply {
	/* num return vars: 1 */
	int status;
	unsigned int stats<>;
};

struct __db_sync_msg {
	unsigned int dbpcl_id;
	unsigned int flags;
};

struct __db_sync_reply {
	/* num return vars: 0 */
	int status;
};

struct __db_truncate_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int flags;
};

struct __db_truncate_reply {
	/* num return vars: 1 */
	int status;
	unsigned int count;
};

struct __db_cursor_msg {
	unsigned int dbpcl_id;
	unsigned int txnpcl_id;
	unsigned int flags;
};

struct __db_cursor_reply {
	/* num return vars: 1 */
	int status;
	unsigned int dbcidcl_id;
};

struct __db_join_msg {
	unsigned int dbpcl_id;
	unsigned int curs<>;
	unsigned int flags;
};

struct __db_join_reply {
	/* num return vars: 1 */
	int status;
	unsigned int dbcidcl_id;
};

struct __dbc_close_msg {
	unsigned int dbccl_id;
};

struct __dbc_close_reply {
	/* num return vars: 0 */
	int status;
};

struct __dbc_count_msg {
	unsigned int dbccl_id;
	unsigned int flags;
};

struct __dbc_count_reply {
	/* num return vars: 1 */
	int status;
	unsigned int dupcount;
};

struct __dbc_del_msg {
	unsigned int dbccl_id;
	unsigned int flags;
};

struct __dbc_del_reply {
	/* num return vars: 0 */
	int status;
};

struct __dbc_dup_msg {
	unsigned int dbccl_id;
	unsigned int flags;
};

struct __dbc_dup_reply {
	/* num return vars: 1 */
	int status;
	unsigned int dbcidcl_id;
};

struct __dbc_get_msg {
	unsigned int dbccl_id;
	unsigned int keydlen;
	unsigned int keydoff;
	unsigned int keyulen;
	unsigned int keyflags;
	opaque keydata<>;
	unsigned int datadlen;
	unsigned int datadoff;
	unsigned int dataulen;
	unsigned int dataflags;
	opaque datadata<>;
	unsigned int flags;
};

struct __dbc_get_reply {
	/* num return vars: 2 */
	int status;
	opaque keydata<>;
	opaque datadata<>;
};

struct __dbc_pget_msg {
	unsigned int dbccl_id;
	unsigned int skeydlen;
	unsigned int skeydoff;
	unsigned int skeyulen;
	unsigned int skeyflags;
	opaque skeydata<>;
	unsigned int pkeydlen;
	unsigned int pkeydoff;
	unsigned int pkeyulen;
	unsigned int pkeyflags;
	opaque pkeydata<>;
	unsigned int datadlen;
	unsigned int datadoff;
	unsigned int dataulen;
	unsigned int dataflags;
	opaque datadata<>;
	unsigned int flags;
};

struct __dbc_pget_reply {
	/* num return vars: 3 */
	int status;
	opaque skeydata<>;
	opaque pkeydata<>;
	opaque datadata<>;
};

struct __dbc_put_msg {
	unsigned int dbccl_id;
	unsigned int keydlen;
	unsigned int keydoff;
	unsigned int keyulen;
	unsigned int keyflags;
	opaque keydata<>;
	unsigned int datadlen;
	unsigned int datadoff;
	unsigned int dataulen;
	unsigned int dataflags;
	opaque datadata<>;
	unsigned int flags;
};

struct __dbc_put_reply {
	/* num return vars: 1 */
	int status;
	opaque keydata<>;
};
program DB_RPC_SERVERPROG {
	version DB_RPC_SERVERVERS {
		__env_get_cachesize_reply __DB_env_get_cachesize(__env_get_cachesize_msg) = 1;
		__env_cachesize_reply __DB_env_cachesize(__env_cachesize_msg) = 2;
		__env_close_reply __DB_env_close(__env_close_msg) = 3;
		__env_create_reply __DB_env_create(__env_create_msg) = 4;
		__env_dbremove_reply __DB_env_dbremove(__env_dbremove_msg) = 5;
		__env_dbrename_reply __DB_env_dbrename(__env_dbrename_msg) = 6;
		__env_get_encrypt_flags_reply __DB_env_get_encrypt_flags(__env_get_encrypt_flags_msg) = 7;
		__env_encrypt_reply __DB_env_encrypt(__env_encrypt_msg) = 8;
		__env_get_flags_reply __DB_env_get_flags(__env_get_flags_msg) = 9;
		__env_flags_reply __DB_env_flags(__env_flags_msg) = 10;
		__env_get_home_reply __DB_env_get_home(__env_get_home_msg) = 11;
		__env_get_open_flags_reply __DB_env_get_open_flags(__env_get_open_flags_msg) = 12;
		__env_open_reply __DB_env_open(__env_open_msg) = 13;
		__env_remove_reply __DB_env_remove(__env_remove_msg) = 14;
		__txn_abort_reply __DB_txn_abort(__txn_abort_msg) = 15;
		__txn_begin_reply __DB_txn_begin(__txn_begin_msg) = 16;
		__txn_commit_reply __DB_txn_commit(__txn_commit_msg) = 17;
		__txn_discard_reply __DB_txn_discard(__txn_discard_msg) = 18;
		__txn_prepare_reply __DB_txn_prepare(__txn_prepare_msg) = 19;
		__txn_recover_reply __DB_txn_recover(__txn_recover_msg) = 20;
		__db_associate_reply __DB_db_associate(__db_associate_msg) = 21;
		__db_bt_maxkey_reply __DB_db_bt_maxkey(__db_bt_maxkey_msg) = 22;
		__db_get_bt_minkey_reply __DB_db_get_bt_minkey(__db_get_bt_minkey_msg) = 23;
		__db_bt_minkey_reply __DB_db_bt_minkey(__db_bt_minkey_msg) = 24;
		__db_close_reply __DB_db_close(__db_close_msg) = 25;
		__db_create_reply __DB_db_create(__db_create_msg) = 26;
		__db_del_reply __DB_db_del(__db_del_msg) = 27;
		__db_get_encrypt_flags_reply __DB_db_get_encrypt_flags(__db_get_encrypt_flags_msg) = 28;
		__db_encrypt_reply __DB_db_encrypt(__db_encrypt_msg) = 29;
		__db_get_extentsize_reply __DB_db_get_extentsize(__db_get_extentsize_msg) = 30;
		__db_extentsize_reply __DB_db_extentsize(__db_extentsize_msg) = 31;
		__db_get_flags_reply __DB_db_get_flags(__db_get_flags_msg) = 32;
		__db_flags_reply __DB_db_flags(__db_flags_msg) = 33;
		__db_get_reply __DB_db_get(__db_get_msg) = 34;
		__db_get_name_reply __DB_db_get_name(__db_get_name_msg) = 35;
		__db_get_open_flags_reply __DB_db_get_open_flags(__db_get_open_flags_msg) = 36;
		__db_get_h_ffactor_reply __DB_db_get_h_ffactor(__db_get_h_ffactor_msg) = 37;
		__db_h_ffactor_reply __DB_db_h_ffactor(__db_h_ffactor_msg) = 38;
		__db_get_h_nelem_reply __DB_db_get_h_nelem(__db_get_h_nelem_msg) = 39;
		__db_h_nelem_reply __DB_db_h_nelem(__db_h_nelem_msg) = 40;
		__db_key_range_reply __DB_db_key_range(__db_key_range_msg) = 41;
		__db_get_lorder_reply __DB_db_get_lorder(__db_get_lorder_msg) = 42;
		__db_lorder_reply __DB_db_lorder(__db_lorder_msg) = 43;
		__db_open_reply __DB_db_open(__db_open_msg) = 44;
		__db_get_pagesize_reply __DB_db_get_pagesize(__db_get_pagesize_msg) = 45;
		__db_pagesize_reply __DB_db_pagesize(__db_pagesize_msg) = 46;
		__db_pget_reply __DB_db_pget(__db_pget_msg) = 47;
		__db_put_reply __DB_db_put(__db_put_msg) = 48;
		__db_get_re_delim_reply __DB_db_get_re_delim(__db_get_re_delim_msg) = 49;
		__db_re_delim_reply __DB_db_re_delim(__db_re_delim_msg) = 50;
		__db_get_re_len_reply __DB_db_get_re_len(__db_get_re_len_msg) = 51;
		__db_re_len_reply __DB_db_re_len(__db_re_len_msg) = 52;
		__db_re_pad_reply __DB_db_re_pad(__db_re_pad_msg) = 53;
		__db_get_re_pad_reply __DB_db_get_re_pad(__db_get_re_pad_msg) = 54;
		__db_remove_reply __DB_db_remove(__db_remove_msg) = 55;
		__db_rename_reply __DB_db_rename(__db_rename_msg) = 56;
		__db_stat_reply __DB_db_stat(__db_stat_msg) = 57;
		__db_sync_reply __DB_db_sync(__db_sync_msg) = 58;
		__db_truncate_reply __DB_db_truncate(__db_truncate_msg) = 59;
		__db_cursor_reply __DB_db_cursor(__db_cursor_msg) = 60;
		__db_join_reply __DB_db_join(__db_join_msg) = 61;
		__dbc_close_reply __DB_dbc_close(__dbc_close_msg) = 62;
		__dbc_count_reply __DB_dbc_count(__dbc_count_msg) = 63;
		__dbc_del_reply __DB_dbc_del(__dbc_del_msg) = 64;
		__dbc_dup_reply __DB_dbc_dup(__dbc_dup_msg) = 65;
		__dbc_get_reply __DB_dbc_get(__dbc_get_msg) = 66;
		__dbc_pget_reply __DB_dbc_pget(__dbc_pget_msg) = 67;
		__dbc_put_reply __DB_dbc_put(__dbc_put_msg) = 68;
	} = 4003;
} = 351457;
