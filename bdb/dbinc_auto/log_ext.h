/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_log_ext_h_
#define	_log_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __log_open __P((DB_ENV *));
int __log_find __P((DB_LOG *, int, u_int32_t *, logfile_validity *));
int __log_valid __P((DB_LOG *, u_int32_t, int, logfile_validity *));
int __log_dbenv_refresh __P((DB_ENV *));
int __log_stat __P((DB_ENV *, DB_LOG_STAT **, u_int32_t));
void __log_get_cached_ckp_lsn __P((DB_ENV *, DB_LSN *));
void __log_region_destroy __P((DB_ENV *, REGINFO *));
int __log_vtruncate __P((DB_ENV *, DB_LSN *, DB_LSN *));
int __log_is_outdated __P((DB_ENV *dbenv, u_int32_t fnum, int *outdatedp));
int __log_archive __P((DB_ENV *, char **[], u_int32_t));
int __log_cursor __P((DB_ENV *, DB_LOGC **, u_int32_t));
void __log_dbenv_create __P((DB_ENV *));
int __log_put __P((DB_ENV *, DB_LSN *, const DBT *, u_int32_t));
void __log_txn_lsn __P((DB_ENV *, DB_LSN *, u_int32_t *, u_int32_t *));
int __log_newfile __P((DB_LOG *, DB_LSN *));
int __log_flush __P((DB_ENV *, const DB_LSN *));
int __log_file __P((DB_ENV *, const DB_LSN *, char *, size_t));
int __log_name __P((DB_LOG *, u_int32_t, char **, DB_FH *, u_int32_t));
int __log_rep_put __P((DB_ENV *, DB_LSN *, const DBT *));

#if defined(__cplusplus)
}
#endif
#endif /* !_log_ext_h_ */
