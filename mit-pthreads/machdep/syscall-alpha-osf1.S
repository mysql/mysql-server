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

#define XSYSCALL(x)	SYSCALL(x) ; ret ; .end machdep_sys_##x

	.globl	machdep_cerror 
machdep_cerror:
	br	t0, 1f
1:
	ldgp	gp, 0(t0)
	stl	v0, errno
#if 0
	ldiq	v0, -1
#else
	subq	zero, v0, v0
#endif
	ret

/* The fork system call is special...  */
SYSCALL(fork)
	cmovne	a4, 0, v0
	ret
	.end	machdep_sys_fork

/* So is the sigsuspend system call */
	.align 4
	.globl machdep_sys_sigsuspend
	.ent machdep_sys_sigsuspend, 0
machdep_sys_sigsuspend:
	.frame	sp,0,ra					

	bis		a0, a0, a1
	ldq		a0, 0(a1)
	ldiq	v0, SYS_sigsuspend
	CHMK()
	ret
	.end 	machdep_sys_sigsuspend

/* More stuff ... */
	.align 4
	.globl machdep_restore_from_setjmp
	.ent machdep_restore_from_setjmp, 0
machdep_restore_from_setjmp:
	.frame	sp, 16, ra
	ldgp	gp, 0(t12)
	lda	sp, -16(sp)
	stq	ra, 0(sp)
	ldq	v0, 280(a0)
	subq	v0, 0x00000000acedbade, t0
	bne	t0, botch
	cmoveq	a1, 0x1, a1
	stq	a1, 32(a0)
	ldiq	v0, 0x67
	call_pal 0x83
botch:
	/* This should cause the program to crash.  Eventually, fix it
	   up to print a message first.  */
	jsr	abort
	.end machdep_restore_from_setjmp
