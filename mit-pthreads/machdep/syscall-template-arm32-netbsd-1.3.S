#include <machine/asm.h>
#include <sys/syscall.h>

#ifdef SYS___sigsuspend14
#define SYS_sigsuspend SYS___sigsuspend14
#endif
#ifdef SYS___sigprocmask14
#define SYS_sigprocmask SYS___sigprocmask14
#endif

#ifdef __STDC__

#define	SYSCALL(x)					\
	.globl _C_LABEL(machdep_sys_##x);		\
							\
_C_LABEL(machdep_sys_##x):;				\
							\
	swi	SYS_##x;				\
	bcs	1b;					\
	mov	r15, r14;							

#else

#define	SYSCALL(x)					\
	.globl _C_LABEL(_machdep_sys_/**/x);		\
							\
_C_LABEL(machdep_sys_/**/x):;				\
							\
	swi	SYS_/**/x;				\
	bcs	1b;					\
	mov	r15, r14;							

#endif


/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	0


/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value.
 */

1:
	rsb	r0, r0, #0x00000000
	mvn	r1, #0x00000000
	mov	r15, r14

#define XSYSCALL(NAME)	SYSCALL(NAME)

XSYSCALL(SYSCALL_NAME)
