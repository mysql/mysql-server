
#if ! defined(_SIZE_T_)
#define _SIZE_T_
typedef pthread_size_t  size_t;
#endif

/* Non-standard Ultrix string routines. */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
__BEGIN_DECLS
int	 bcmp __P_((const void *, const void *, size_t));
void	 bcopy __P_((const void *, void *, size_t));
void	 bzero __P_((void *, size_t));
char	*index __P_((const char *, int));
char	*rindex __P_((const char *, int));
__END_DECLS
#endif 

