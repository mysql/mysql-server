  
#ifndef _FPOS_T 
#define _FPOS_T
typedef pthread_fpos_t fpos_t;      /* Must match off_t <sys/types.h> */
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef pthread_size_t  size_t;
#endif

#define HAVE_SYS_ERRLIST_WITHOUT_CONST

