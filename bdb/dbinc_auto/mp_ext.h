/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_mp_ext_h_
#define	_mp_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __memp_alloc __P((DB_MPOOL *, REGINFO *, MPOOLFILE *, size_t, roff_t *, void *));
#ifdef DIAGNOSTIC
void __memp_check_order __P((DB_MPOOL_HASH *));
#endif
int __memp_bhwrite __P((DB_MPOOL *, DB_MPOOL_HASH *, MPOOLFILE *, BH *, int));
int __memp_pgread __P((DB_MPOOLFILE *, DB_MUTEX *, BH *, int));
int __memp_pg __P((DB_MPOOLFILE *, BH *, int));
void __memp_bhfree __P((DB_MPOOL *, DB_MPOOL_HASH *, BH *, int));
int __memp_fget __P((DB_MPOOLFILE *, db_pgno_t *, u_int32_t, void *));
int __memp_fcreate __P((DB_ENV *, DB_MPOOLFILE **, u_int32_t));
int __memp_fopen_int __P((DB_MPOOLFILE *, MPOOLFILE *, const char *, u_int32_t, int, size_t));
int __memp_fclose_int __P((DB_MPOOLFILE *, u_int32_t));
int __memp_mf_discard __P((DB_MPOOL *, MPOOLFILE *));
char * __memp_fn __P((DB_MPOOLFILE *));
char * __memp_fns __P((DB_MPOOL *, MPOOLFILE *));
int __memp_fput __P((DB_MPOOLFILE *, void *, u_int32_t));
int __memp_fset __P((DB_MPOOLFILE *, void *, u_int32_t));
void __memp_dbenv_create __P((DB_ENV *));
int __memp_open __P((DB_ENV *));
int __memp_dbenv_refresh __P((DB_ENV *));
void __mpool_region_destroy __P((DB_ENV *, REGINFO *));
int  __memp_nameop __P((DB_ENV *, u_int8_t *, const char *, const char *, const char *));
int __memp_register __P((DB_ENV *, int, int (*)(DB_ENV *, db_pgno_t, void *, DBT *), int (*)(DB_ENV *, db_pgno_t, void *, DBT *)));
int __memp_stat __P((DB_ENV *, DB_MPOOL_STAT **, DB_MPOOL_FSTAT ***, u_int32_t));
int __memp_dump_region __P((DB_ENV *, char *, FILE *));
void __memp_stat_hash __P((REGINFO *, MPOOL *, u_int32_t *));
int __memp_sync __P((DB_ENV *, DB_LSN *));
int __memp_fsync __P((DB_MPOOLFILE *));
int __mp_xxx_fh __P((DB_MPOOLFILE *, DB_FH **));
int __memp_sync_int __P((DB_ENV *, DB_MPOOLFILE *, int, db_sync_op, int *));
int __memp_trickle __P((DB_ENV *, int, int *));

#if defined(__cplusplus)
}
#endif
#endif /* !_mp_ext_h_ */
