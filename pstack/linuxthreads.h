/* $Header$ */

/*
 * LinuxThreads specific stuff.
 */

#ifndef	pstack_linuxthreads_h_
#define	pstack_linuxthreads_h_

#include	<pthread.h>
#include	"pstacktrace.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Tell other threads to dump stacks...
 */
int
linuxthreads_notify_others(	const int	signotify);

#ifdef __cplusplus
}
#endif

#endif /* pstack_linuxthreads_h_ */

