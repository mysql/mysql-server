/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_env_ext_h_
#define	_env_ext_h_
#if defined(__cplusplus)
extern "C" {
#endif
void __db_shalloc_init __P((void *, size_t));
int __db_shalloc_size __P((size_t, size_t));
int __db_shalloc __P((void *, size_t, size_t, void *));
void __db_shalloc_free __P((void *, void *));
size_t __db_shalloc_count __P((void *));
size_t __db_shsizeof __P((void *));
void __db_shalloc_dump __P((void *, FILE *));
int __db_tablesize __P((u_int32_t));
void __db_hashinit __P((void *, u_int32_t));
int  __dbenv_init __P((DB_ENV *));
int __db_mi_env __P((DB_ENV *, const char *));
int __db_mi_open __P((DB_ENV *, const char *, int));
int __db_env_config __P((DB_ENV *, int));
int __dbenv_open __P((DB_ENV *, const char *, u_int32_t, int));
int __dbenv_remove __P((DB_ENV *, const char *, u_int32_t));
int __dbenv_close __P((DB_ENV *, u_int32_t));
int __db_appname __P((DB_ENV *, APPNAME,
   const char *, const char *, u_int32_t, DB_FH *, char **));
int __db_apprec __P((DB_ENV *, u_int32_t));
int __db_e_attach __P((DB_ENV *, u_int32_t *));
int __db_e_detach __P((DB_ENV *, int));
int __db_e_remove __P((DB_ENV *, int));
int __db_e_stat __P((DB_ENV *, REGENV *, REGION *, int *));
int __db_r_attach __P((DB_ENV *, REGINFO *, size_t));
int __db_r_detach __P((DB_ENV *, REGINFO *, int));
#if defined(__cplusplus)
}
#endif
#endif /* _env_ext_h_ */
