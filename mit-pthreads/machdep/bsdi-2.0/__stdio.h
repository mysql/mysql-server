#include <machine/ansi.h>
#ifdef  _BSD_SIZE_T_
typedef _BSD_SIZE_T_    size_t;
#undef  _BSD_SIZE_T_
#endif

typedef pthread_fpos_t fpos_t;      /* Must match off_t <sys/types.h> */
