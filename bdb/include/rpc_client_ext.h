/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_rpc_client_ext_h_
#define	_rpc_client_ext_h_
#if defined(__cplusplus)
extern "C" {
#endif
int __dbcl_envserver __P((DB_ENV *, char *, long, long, u_int32_t));
int __dbcl_refresh __P((DB_ENV *));
int __dbcl_txn_close __P((DB_ENV *));
void __dbcl_txn_end __P((DB_TXN *));
int __dbcl_c_destroy __P((DBC *));
void __dbcl_c_refresh __P((DBC *));
int __dbcl_c_setup __P((long, DB *, DBC **));
int __dbcl_retcopy __P((DB_ENV *, DBT *, void *, u_int32_t));
int __dbcl_dbclose_common __P((DB *));
#if defined(__cplusplus)
}
#endif
#endif /* _rpc_client_ext_h_ */
