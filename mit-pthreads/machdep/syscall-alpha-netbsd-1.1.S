#include <machine/asm.h>
#define	CHMK()		call_pal 0x83
#define COMPAT_43
#include <sys/syscall.h>
#ifndef __CONCAT
#include <sys/cdefs.h>
#endif
#define	CONCAT		__CONCAT

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
	.globl	CONCAT(machdep_sys_,x)			;\
	.ent	CONCAT(machdep_sys_,x), 0		;\
CONCAT(machdep_sys_,x):					;\
	.frame	sp,0,ra					;\
	ldiq	v0, CONCAT(SYS_,x)			;\
	CHMK()						;\
	beq	a3, CONCAT(Lsys_noerr_,x)		;\
	br	gp, CONCAT(Lsys_err_,x)			;\
CONCAT(Lsys_err_,x):					;\
	/* Load gp so we can find cerror to jump to.  */;\
	ldgp	gp, 0(gp)				;\
	jmp	zero, machdep_cerror			;\
CONCAT(Lsys_noerr_,x):

#define XSYSCALL(x)	SYSCALL(x) ; RET ; .end CONCAT(machdep_sys_,x)

	.globl	machdep_cerror 
machdep_cerror:
	br	t0, Lmachdep_cerror_setgp
Lmachdep_cerror_setgp:
	ldgp	gp, 0(t0)
	stl	v0, errno
#if 0
	ldiq	v0, -1
#else
	subq	zero, v0, v0
#endif
	RET

/* The fork system call is special...  */
SYSCALL(fork)
	cmovne	a4, 0, v0
	RET
	.end	machdep_sys_fork

/* The pipe system call is special... */
SYSCALL(pipe)
	stl	v0, 0(a0)
	stl	a4, 4(a0)
	mov	zero, v0
	RET
	.end	machdep_sys_pipe

/* The sigsuspend system call is special... */
	.align 4
	.globl machdep_sys_sigsuspend
	.ent machdep_sys_sigsuspend, 0
machdep_sys_sigsuspend:
	ldl	a0, 0(a0)		/* pass *mask instead of mask */
	ldiq	v0, SYS_sigsuspend
	CHMK()
	mov	zero, v0		/* shouldn't need; just in case... */
	RET
	.end 	machdep_sys_sigsuspend

/* The sigprocmask system call is special... */
	.align 4
	.globl machdep_sys_sigprocmask
	.ent machdep_sys_sigprocmask, 0
machdep_sys_sigprocmask:
	mov	a2, a5			/* safe */
	cmoveq	a1, 1, a0		/* if set == NULL, how = SIG_BLOCK */
	beq	a1, Ldoit		/* and set = 0, and do it. */
	ldl	a1, 0(a1)		/* load the set from *set */
Ldoit:	ldiq	v0, SYS_sigprocmask
	CHMK()
	beq	a5, Lret		/* if they don't want old mask, done */	
	stl	v0, 0(a5)		/* otherwise, give it to them. */
Lret:	mov	zero, v0
	RET
	.end	machdep_sys_sigprocmask

/* More stuff ... */
	.align 4
	.global __machdep_save_int_state
	.ent __machdep_save_int_state, 0
__machdep_save_int_state:
	.frame	sp, 16, ra
	ldgp	gp, 0(t12)
	lda	sp, -16(sp)
	stq	ra, 0(sp)

	/* save integer registers */
	stq	ra, ( 0 * 8)(a0)		/* return address */
	stq	s0, ( 1 * 8)(a0)		/* callee-saved registers */
	stq	s1, ( 2 * 8)(a0)
	stq	s2, ( 3 * 8)(a0)
	stq	s3, ( 4 * 8)(a0)
	stq	s4, ( 5 * 8)(a0)
	stq	s5, ( 6 * 8)(a0)
	stq	s6, ( 7 * 8)(a0)
	stq	sp, ( 9 * 8)(a0)
	stq	ra, ( 8 * 8)(a0)		/* RA on return */
	stq	pv, (10 * 8)(a0)		/* and PV; we restore it */

	mov	zero, v0
	lda	sp, 16(sp)
	RET
	.end __machdep_save_int_state

	.align 4
	.global __machdep_restore_int_state
	.ent __machdep_restore_int_state, 0
__machdep_restore_int_state:
	.frame	sp, 16, ra
	ldgp	gp, 0(t12)
	lda	sp, -16(sp)
	stq	ra, 0(sp)

	/* restore integer registers */
	ldq	t0, ( 0 * 8)(a0)		/* return address */
	ldq	s0, ( 1 * 8)(a0)		/* callee-saved registers */
	ldq	s1, ( 2 * 8)(a0)
	ldq	s2, ( 3 * 8)(a0)
	ldq	s3, ( 4 * 8)(a0)
	ldq	s4, ( 5 * 8)(a0)
	ldq	s5, ( 6 * 8)(a0)
	ldq	s6, ( 7 * 8)(a0)
	ldq	ra, ( 8 * 8)(a0)		/* RA after return */
	ldq	sp, ( 9 * 8)(a0)
	ldq	pv, (10 * 8)(a0)		/* and PV; we restore it */

	ldiq    v0, 1
	ret     zero,(t0),1
	.end __machdep_restore_int_state

	.align 4
	.global __machdep_save_fp_state
	.ent __machdep_save_fp_state, 0
__machdep_save_fp_state:
	.frame	sp, 16, ra
	ldgp	gp, 0(t12)
	lda	sp, -16(sp)
	stq	ra, 0(sp)

	/* save FP registers */
	stt	fs0, (0 * 8)(a0)		/* callee-saved registers */
	stt	fs1, (1 * 8)(a0)
	stt	fs2, (2 * 8)(a0)
	stt	fs3, (3 * 8)(a0)
	stt	fs4, (4 * 8)(a0)
	stt	fs5, (5 * 8)(a0)
	stt	fs6, (6 * 8)(a0)
	stt	fs7, (7 * 8)(a0)
	mf_fpcr ft0 				/* and FP control reg */
	stt	ft0, (8 * 8)(a0)

	lda	sp, 16(sp)
	RET
	.end __machdep_save_fp_state

	.align 4
	.global __machdep_restore_fp_state
	.ent __machdep_restore_fp_state, 0
__machdep_restore_fp_state:
	.frame	sp, 16, ra
	ldgp	gp, 0(t12)
	lda	sp, -16(sp)
	stq	ra, 0(sp)

	/* restore FP registers */
	ldt	fs0, (0 * 8)(a0)		/* callee-saved registers */
	ldt	fs1, (1 * 8)(a0)
	ldt	fs2, (2 * 8)(a0)
	ldt	fs3, (3 * 8)(a0)
	ldt	fs4, (4 * 8)(a0)
	ldt	fs5, (5 * 8)(a0)
	ldt	fs6, (6 * 8)(a0)
	ldt	fs7, (7 * 8)(a0)
	ldt	ft0, (8 * 8)(a0)
	mt_fpcr ft0 				/* and FP control reg */

	lda	sp, 16(sp)
	RET
	.end __machdep_restore_fp_state
