/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_os_ext_h_
#define	_os_ext_h_
#if defined(__cplusplus)
extern "C" {
#endif
int __os_abspath __P((const char *));
int __os_strdup __P((DB_ENV *, const char *, void *));
int __os_calloc __P((DB_ENV *, size_t, size_t, void *));
int __os_malloc __P((DB_ENV *, size_t, void *(*)(size_t), void *));
int __os_realloc __P((DB_ENV *,
    size_t, void *(*)(void *, size_t), void *));
void __os_free __P((void *, size_t));
void __os_freestr __P((void *));
void *__ua_memcpy __P((void *, const void *, size_t));
int __os_dirlist __P((DB_ENV *, const char *, char ***, int *));
void __os_dirfree __P((char **, int));
int __os_get_errno __P((void));
void __os_set_errno __P((int));
int __os_fileid __P((DB_ENV *, const char *, int, u_int8_t *));
int __os_finit __P((DB_ENV *, DB_FH *, size_t, int));
int __os_fpinit __P((DB_ENV *, DB_FH *, db_pgno_t, int, int));
int __os_fsync __P((DB_ENV *, DB_FH *));
int __os_openhandle __P((DB_ENV *, const char *, int, int, DB_FH *));
int __os_closehandle __P((DB_FH *));
int __os_r_sysattach __P((DB_ENV *, REGINFO *, REGION *));
int __os_r_sysdetach __P((DB_ENV *, REGINFO *, int));
int __os_mapfile __P((DB_ENV *,
    char *, DB_FH *, size_t, int, void **));
int __os_unmapfile __P((DB_ENV *, void *, size_t));
u_int32_t __db_oflags __P((int));
int __db_omode __P((const char *));
int __os_open __P((DB_ENV *, const char *, u_int32_t, int, DB_FH *));
int __os_shmname __P((DB_ENV *, const char *, char **));
int __os_r_attach __P((DB_ENV *, REGINFO *, REGION *));
int __os_r_detach __P((DB_ENV *, REGINFO *, int));
int __os_rename __P((DB_ENV *, const char *, const char *));
int __os_isroot __P((void));
char *__db_rpath __P((const char *));
int __os_io __P((DB_ENV *, DB_IO *, int, size_t *));
int __os_read __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
int __os_write __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
int __os_seek __P((DB_ENV *,
     DB_FH *, size_t, db_pgno_t, u_int32_t, int, DB_OS_SEEK));
int __os_sleep __P((DB_ENV *, u_long, u_long));
int __os_spin __P((void));
void __os_yield __P((DB_ENV*, u_long));
int __os_exists __P((const char *, int *));
int __os_ioinfo __P((DB_ENV *, const char *,
   DB_FH *, u_int32_t *, u_int32_t *, u_int32_t *));
int __os_tmpdir __P((DB_ENV *, u_int32_t));
int __os_unlink __P((DB_ENV *, const char *));
int __os_region_unlink __P((DB_ENV *, const char *));
#if defined(DB_WIN32)
int __os_win32_errno __P((void));
#endif
int __os_fpinit __P((DB_ENV *, DB_FH *, db_pgno_t, int, int));
int __os_is_winnt __P((void));
#if defined(__cplusplus)
}
#endif
#endif /* _os_ext_h_ */
