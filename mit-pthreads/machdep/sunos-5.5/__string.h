
#ifndef _SIZE_T
#define _SIZE_T
typedef pthread_size_t  size_t;
#endif

/* Non-standard NetBSD string routines. */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
__BEGIN_DECLS
char	*strdup __P_((const char *));
__END_DECLS
#endif 
