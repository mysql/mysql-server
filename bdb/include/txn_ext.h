/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_txn_ext_h_
#define	_txn_ext_h_
#if defined(__cplusplus)
extern "C" {
#endif
int __txn_xa_begin __P((DB_ENV *, DB_TXN *));
int __txn_end __P((DB_TXN *, int));
int __txn_activekids __P((DB_ENV *, u_int32_t, DB_TXN *));
int __txn_regop_recover
   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_xa_regop_recover
   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_ckp_recover
__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_child_recover
   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
void __txn_dbenv_create __P((DB_ENV *));
int __txn_open __P((DB_ENV *));
int __txn_close __P((DB_ENV *));
#if defined(__cplusplus)
}
#endif
#endif /* _txn_ext_h_ */
