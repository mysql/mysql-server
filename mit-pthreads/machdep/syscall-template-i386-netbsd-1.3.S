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
	movl $(SYS_##x), %eax;				\
	int $0x80;					\
	jb	1b;					\
	ret;							

#else

#define	_SYSCALL(x)					\
	.globl _C_LABEL(machdep_sys_/**/x);		\
							\
_C_LABEL(machdep_sys_/**/x):;				\
							\
	movl $(SYS_/**/x), %eax;			\
	int $0x80;					\
	jb	1b;					\
	ret;							

#endif


/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	2


/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value.
 */

1:
	neg		%eax
	ret

#define XSYSCALL(NAME)	SYSCALL(NAME)

XSYSCALL(SYSCALL_NAME)
