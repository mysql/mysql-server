/* $Header$ */

#ifndef	pstack_pstack_h_
#define	pstack_pstack_h_

#include	"pstacktrace.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Install the stack-trace-on-SEGV handler....
 */
extern int
pstack_install_segv_action(	const char*	path_format);
#ifdef __cplusplus
}
#endif

#endif /* pstack_pstack_h_ */

