
#ifndef lint
    .text
    .asciz "$Id$";
#endif

#include <machine/asm.h>
#include <sys/syscall.h>

#ifdef __STDC__
#define IMM	#
#define SYSCALL(x)      .even; ENTRY(machdep_sys_ ## x); \
                        movl IMM SYS_ ## x,d0; trap IMM 0; jcs err; rts
#else /* !__STDC__ */
#define SYSCALL(x)      .even; ENTRY(machdep_sys_/**/x); \
                        movl #SYS_/**/x,d0; trap #0; jcs err; rts
#endif /* !__STDC__ */

/*
 * Initial asm stuff for all functions.
 */
        .text
        .even


/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value.
 */

err:
        negl            d0
        rts

/* ==========================================================================
 * machdep_sys_pipe
 */
	.even
ENTRY(machdep_sys_pipe);
	movl #SYS_pipe,d0
	trap #0
	jcs 	err
	movl	sp@(4),a0
	movl	d0,a0@+
	movl	d1,a0@
	clrl	d0
	rts

#ifndef SYS___sigsuspend14
	.even
ENTRY(machdep_sys_sigsuspend)
	movl	sp@(4),a0
	movl	a0@,sp@(4)
	movl	#SYS_compat_13_sigsuspend13,d0
	trap	#0
	jcs	err
	clrl	d0
	rts
#endif

#ifndef SYS___sigprocmask14
	.even
ENTRY(machdep_sys_sigprocmask)
	tstl	sp@(8)
	jne	gotptr
/*	movl	#0,sp@(8)	/* null mask pointer; block empty set */
	movl	#1,sp@(4)	
	jra	doit
gotptr:
	movl	sp@(8),a0
	movl	a0@,sp@(8)
doit:
	movl	#SYS_compat_13_sigprocmask13,d0
	trap	#0
	jcs	err
	tstl	sp@(12)
	jeq	out
	movl	sp@(12),a0
	movl	d0,a0@
out:
	clrl	d0
	rts
#endif
