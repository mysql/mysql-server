
#ifndef _SIZE_T 
#define _SIZE_T
typedef pthread_size_t  size_t;
#endif

/* Hack for configuration with libgcc 2.2 */
#ifndef pthread_fpos_t
#define pthread_fpos_t long
#endif

typedef pthread_fpos_t fpos_t;
