
#include <machine/ansi.h>
#ifdef  _BSD_SIZE_T_
typedef _BSD_SIZE_T_    size_t;
#undef  _BSD_SIZE_T_
#endif

/* Non-standard NetBSD string routines. */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
__BEGIN_DECLS
int      bcmp __P_((const void *, const void *, size_t));
void     bcopy __P_((const void *, void *, size_t));
void     bzero __P_((void *, size_t));
char    *index __P_((const char *, int));
char    *rindex __P_((const char *, int));
char    *strdup __P_((const char *));
void     strmode __P_((int, char *));
char    *strsep __P_((char **, const char *));
__END_DECLS
#endif

