#include <sys/errno.h>
#include <sys/syscall.h>

#ifdef _SCO_ELF

#define NAME(X) 	machdep_sys_##X
#define END(X)		1: ; .type NAME(X),@function ; .size NAME(X),1b - NAME(X)
#define GETBX(X) \
	push %ebx; \
	call 1f; \
1: \
	popl %ebx;
#else

#define NAME(X)		_machdep_sys_##X
#define END(X)

#endif

#ifdef __STDC__

#define	SYSCALL(x)					\
	.globl NAME(x);		\
									\
NAME(x):;				\
	movl $(SYS_##x##), %eax;		\
	.byte 0x9a; .long 0; .word 7;	\
	jae	1f;							\
	cmp	$(ERESTART), %eax; \
	je	NAME(x); \
	neg	%eax; \
1: \
	ret;							\
	END(x)

#else

#define	SYSCALL(x)					\
	.globl _machdep_sys_/**/x;		\
									\
machdep_sys_/**/x:;				\
									\
	movl $(SYS_/**/x), %eax;		\
	.byte 0x9a; .long 0; .word 7;	\
	jae	1f;							\
	neg	%eax;
1: \
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

#define XSYSCALL(NAME)	SYSCALL(NAME)

XSYSCALL(SYSCALL_NAME)
