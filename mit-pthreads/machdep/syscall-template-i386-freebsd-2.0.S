#include <sys/syscall.h>

#ifdef __STDC__

#define SYS(X)		SYS_##X
#ifdef __ELF__
#define NAME(X)		machdep_sys_##X
#else
#define NAME(X)		_machdep_sys_##X
#endif

#else

#define SYS(X)		SYS_/**/X
#ifdef __ELF__
#define NAME(X)		machdep_sys_/**/X
#else
#define NAME(X)		_machdep_sys_/**/X
#endif

#endif

#ifdef __ELF__
#define END(X)		5: ; .type NAME(X),@function ; .size NAME(X),5b - NAME(X)
#define KERNCALL	int $0x80
#else
#define END(X)
#define KERNCALL	.byte 0x9a; .long 0; .word 7;
#endif

#define	SYSCALL(x)					\
	.globl NAME(x);					\
							\
NAME(x):;						\
	movl $(SYS(x)), %eax;				\
	KERNCALL;					\
	jb	1b;					\
	ret;						\
	END(x)

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
