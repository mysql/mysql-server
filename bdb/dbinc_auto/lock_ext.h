/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_lock_ext_h_
#define	_lock_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __lock_id __P((DB_ENV *, u_int32_t *));
int __lock_id_free __P((DB_ENV *, u_int32_t));
int __lock_vec __P((DB_ENV *, u_int32_t, u_int32_t, DB_LOCKREQ *, int, DB_LOCKREQ **));
int __lock_get __P((DB_ENV *, u_int32_t, u_int32_t, const DBT *, db_lockmode_t, DB_LOCK *));
int  __lock_put __P((DB_ENV *, DB_LOCK *));
int __lock_downgrade __P((DB_ENV *, DB_LOCK *, db_lockmode_t, u_int32_t));
int __lock_addfamilylocker __P((DB_ENV *, u_int32_t, u_int32_t));
int __lock_freefamilylocker  __P((DB_LOCKTAB *, u_int32_t));
int __lock_set_timeout __P(( DB_ENV *, u_int32_t, db_timeout_t, u_int32_t));
int __lock_inherit_timeout __P(( DB_ENV *, u_int32_t, u_int32_t));
int __lock_getlocker __P((DB_LOCKTAB *, u_int32_t, u_int32_t, int, DB_LOCKER **));
int __lock_promote __P((DB_LOCKTAB *, DB_LOCKOBJ *, u_int32_t));
int __lock_expired __P((DB_ENV *, db_timeval_t *, db_timeval_t *));
int __lock_detect __P((DB_ENV *, u_int32_t, u_int32_t, int *));
void __lock_dbenv_create __P((DB_ENV *));
void __lock_dbenv_close __P((DB_ENV *));
int __lock_open __P((DB_ENV *));
int __lock_dbenv_refresh __P((DB_ENV *));
void __lock_region_destroy __P((DB_ENV *, REGINFO *));
int __lock_id_set __P((DB_ENV *, u_int32_t, u_int32_t));
int __lock_stat __P((DB_ENV *, DB_LOCK_STAT **, u_int32_t));
int __lock_dump_region __P((DB_ENV *, char *, FILE *));
void __lock_printlock __P((DB_LOCKTAB *, struct __db_lock *, int));
int __lock_cmp __P((const DBT *, DB_LOCKOBJ *));
int __lock_locker_cmp __P((u_int32_t, DB_LOCKER *));
u_int32_t __lock_ohash __P((const DBT *));
u_int32_t __lock_lhash __P((DB_LOCKOBJ *));
u_int32_t __lock_locker_hash __P((u_int32_t));

#if defined(__cplusplus)
}
#endif
#endif /* !_lock_ext_h_ */
