/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_rpc_server_ext_h_
#define	_rpc_server_ext_h_
#if defined(__cplusplus)
extern "C" {
#endif
void __db_stats_freelist __P((__db_stat_statsreplist **));
void __dbsrv_settimeout __P((ct_entry *, u_int32_t));
void __dbsrv_timeout __P((int));
void __dbclear_ctp __P((ct_entry *));
void __dbdel_ctp __P((ct_entry *));
ct_entry *new_ct_ent __P((u_int32_t *));
ct_entry *get_tableent __P((long));
void __dbsrv_active __P((ct_entry *));
int __dbc_close_int __P((ct_entry *));
int __dbenv_close_int __P((long, int));
char *get_home __P((char *));
#if defined(__cplusplus)
}
#endif
#endif /* _rpc_server_ext_h_ */
