/* This is intended to eventually find /usr/include/sys/cdefs.h
 * if it's inside the ifdef then it won't work if this file is 
 * found in the include files path more than once.
 *
 * include_next is a GNU C extension, we might eventually want
 * to have our own cdefs in here simply to avoid GNU C dependencies
 * (though there are already enough in the asm stuff anyways)
 * [gsstark:19950419.0307EST]
 */
#include_next <sys/cdefs.h>

#ifndef _PTHREAD_SYS_CDEFS_H_
#define _PTHREAD_SYS_CDEFS_H_

#ifndef __NORETURN
#define __NORETURN
#endif /* __NORETURN not defined.  */

#if !defined(__cplusplus)
#define __CAN_DO_EXTERN_INLINE
#endif

#endif /* _PTHREAD_SYS_CDEFS_H_ */
