#include <sys/syscall.h>

#ifdef __STDC__

#define	SYSCALL(x)					\
	.globl _machdep_sys_##x##;		\
									\
_machdep_sys_##x:##;				\
									\
	movl $(SYS_##x##), %eax;		\
	.byte 0x9a; .long 0; .word 7;	\
	jb	1b;							\
	ret;							

#else

#define	SYSCALL(x)					\
	.globl _machdep_sys_/**/x;		\
									\
_machdep_sys_/**/x:;				\
									\
	movl $(SYS_/**/x), %eax;		\
	.byte 0x9a; .long 0; .word 7;	\
	jb	1b;							\
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
