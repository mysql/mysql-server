#include <asm.h>
#include <regdef.h>
#define COMPAT_43
#include <syscall.h>

#undef SYSCALL

/* Kernel syscall interface:
   Input:
	v0 - system call number
	a* - arguments, as in C
   Output:
	a3 - zero iff successful
	v0 - errno value on failure, else result

   This macro is similar to SYSCALL in asm.h, but not completely.
   There's room for optimization, if we assume this will continue to
   be assembled as one file.

   This macro expansions does not include the return instruction.
   If there's no other work to be done, use something like:
	SYSCALL(foo) ; ret
   If there is other work to do (in fork, maybe?), do it after the
   SYSCALL invocation.  */

#define SYSCALL(x) \
	.align	4					;\
	.globl	machdep_sys_##x				;\
	.ent	machdep_sys_##x, 0			;\
machdep_sys_##x:					;\
	.frame	sp,0,ra					;\
	ldiq	v0, SYS_##x				;\
	CHMK()						;\
	beq	a3, 2f					;\
	br	gp, 1f					;\
1:							;\
	/* Load gp so we can find cerror to jump to.  */;\
	ldgp	gp, 0(gp)				;\
	jmp	zero, machdep_cerror				;\
2:

#define SIMPLE_SYSCALL(x)	SYSCALL(x) ; ret ; .end machdep_sys_##x

#define XSYSCALL(x)	SIMPLE_SYSCALL(x)

XSYSCALL(SYSCALL_NAME)
