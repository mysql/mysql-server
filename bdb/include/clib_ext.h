/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_clib_ext_h_
#define	_clib_ext_h_
#if defined(__cplusplus)
extern "C" {
#endif
#ifndef HAVE_GETCWD
char *getcwd __P((char *, size_t));
#endif
#ifndef HAVE_GETOPT
int getopt __P((int, char * const *, const char *));
#endif
#ifndef HAVE_MEMCMP
int memcmp __P((const void *, const void *, size_t));
#endif
#ifndef HAVE_MEMCPY
void *memcpy __P((void *, const void *, size_t));
#endif
#ifndef HAVE_MEMMOVE
void *memmove __P((void *, const void *, size_t));
#endif
#ifndef HAVE_RAISE
int raise __P((int));
#endif
#ifndef HAVE_SNPRINTF
int snprintf __P((char *, size_t, const char *, ...));
#endif
int strcasecmp __P((const char *, const char *));
#ifndef HAVE_STRERROR
char *strerror __P((int));
#endif
#ifndef HAVE_VSNPRINTF
int vsnprintf();
#endif
#if defined(__cplusplus)
}
#endif
#endif /* _clib_ext_h_ */
