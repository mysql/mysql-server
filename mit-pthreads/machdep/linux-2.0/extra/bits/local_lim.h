/* Minimum guaranteed maximum values for system limits.  Linux version.

/* The kernel header pollutes the namespace with the NR_OPEN symbol.
   Remove this after including the header if necessary.  */

#ifndef NR_OPEN
# define __undef_NR_OPEN
#endif

#include <linux/limits.h>

#ifdef __undef_NR_OPEN
# undef NR_OPEN
# undef __undef_NR_OPEN
#endif
