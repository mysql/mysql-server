#include <machine/asm.h>
#define COMPAT_43
#include <sys/syscall.h>
#ifndef __CONCAT
#include <sys/cdefs.h>
#endif
#define        CONCAT          __CONCAT

#undef SYSCALL

/* Kernel syscall interface:
   Input:
       0   - system call number
       3-8 - arguments, as in C
   Output:
       so - (summary overflow) clear iff successful

   This macro is similar to SYSCALL in asm.h, but not completely.
   There's room for optimization, if we assume this will continue to
   be assembled as one file.

   This macro expansions does not include the return instruction.
   If there's no other work to be done, use something like:
       SYSCALL(foo) ; ret
   If there is other work to do (in fork, maybe?), do it after the
   SYSCALL invocation.  */

ENTRY(machdep_cerror)
	mflr    0		# Save LR in 0
	stwu	1,-16(1)	# allocate new stack frame
	stw	0,20(1)		# Stash 0 in stack
	stw	31,8(1)		# Stash 31 in stack (since it's callee-saved
	mr	31,3		# and we stash return there)
	bl	PIC_PLT(_C_LABEL(__errno))
	stw	31,0(3)		# *errno() = err
	lwz	0,20(1)		# Restore LR from stack to 0
	neg	3,31		# return -errno to 3
	lwz	31,8(1)		# Restore 31 from stack
	mtlr	0
	la	1,16(1)		# Restore stack frame
	li	4,-1		# Put -1 in r4 for those syscalls that return
	blr			# two values

/* The fork system call is special...  */
ENTRY(machdep_sys_fork)
	li	0, SYS_fork
	sc
	bso	PIC_PLT(_C_LABEL(machdep_cerror))
	addi  4,4,-1
	blr

/* The pipe system call is special... */
ENTRY(machdep_sys_pipe)
	mr	5,3
	li	0,SYS_pipe
	sc
	bso	PIC_PLT(_C_LABEL(machdep_cerror))
	stw	3,0(5)		# Success, store fds
	stw	4,4(5)
	li	3,0
	blr			# And return 0

#ifndef SYS___sigsuspend14
/* The sigsuspend system call is special... */
ENTRY(machdep_sys_sigsuspend)
	lwz	3,0(3)
	li	0,SYS_compat_13_sigsuspend13
	sc
	b	PIC_PLT(_C_LABEL(machdep_cerror))
#endif /* SYS_sigsuspend14 */

#ifndef SYS___sigprocmask14
/* The sigprocmask system call is special... */
ENTRY(machdep_sys_sigprocmask)
	or.	4,4,4		# Set == NULL ?
	li	6,1		# how = SIG_BLOCK
	beq	Ldoit
	lwz	4,0(4)		# if not, replace it in r4 with #set
	mr	6,3
Ldoit:	mr	3,6		# ... using sigprocmask(SIG_BLOCK)
	li	0,SYS_compat_13_sigprocmask13
	sc
	bso	PIC_PLT(_C_LABEL(machdep_cerror))
	or.	5,5,5		# Check to see if oset requested
	beq	Ldone		# if oset != NULL
	stw	3,0(5)		#   *oset = oldmask
Ldone:
	li	3,0		# return 0
	blr
#endif /* SYS_sigprocmask14 */

/* More stuff ... */

/* For fstat() we actually syscall fstat13. */
ENTRY(machdep_sys_fstat)
	li	0, SYS___fstat13
	sc
	bnslr
	b	PIC_PLT(_C_LABEL(machdep_cerror))

/* Do we need to save the entire floating point state?  I think so... */
ENTRY(__machdep_save_fp_state)
	stwu	 1,-8(1)
	stw	 3,4(1)
	stfd	 0,0(3)
	stfdu	 1,8(3)
	stfdu	 2,8(3)
	stfdu	 3,8(3)
	stfdu	 4,8(3)
	stfdu	 5,8(3)
	stfdu	 6,8(3)
	stfdu	 7,8(3)
	stfdu	 8,8(3)
	stfdu	 9,8(3)
	stfdu	10,8(3)
	stfdu	11,8(3)
	stfdu	12,8(3)
	stfdu	13,8(3)
	stfdu	14,8(3)
	stfdu	15,8(3)
	stfdu	16,8(3)
	stfdu	17,8(3)
	stfdu	18,8(3)
	stfdu	19,8(3)
	stfdu	20,8(3)
	stfdu	21,8(3)
	stfdu	22,8(3)
	stfdu	23,8(3)
	stfdu	24,8(3)
	stfdu	25,8(3)
	stfdu	26,8(3)
	stfdu	27,8(3)
	stfdu	28,8(3)
	stfdu	29,8(3)
	stfdu	30,8(3)
	stfdu	31,8(3)
	mffs	0
	stfdu	0,8(3)
	lwz	3,4(1)
	lwz	1,0(1)
	blr

ENTRY(__machdep_restore_fp_state)
	stwu	 1,-12(1)
	stw	 3,4(1)
	stw	 4,8(1)
	mr	 4,3
	lfdu	 1,8(3)
	lfdu	 2,8(3)
	lfdu	 3,8(3)
	lfdu	 4,8(3)
	lfdu	 5,8(3)
	lfdu	 6,8(3)
	lfdu	 7,8(3)
	lfdu	 8,8(3)
	lfdu	 9,8(3)
	lfdu	10,8(3)
	lfdu	11,8(3)
	lfdu	12,8(3)
	lfdu	13,8(3)
	lfdu	14,8(3)
	lfdu	15,8(3)
	lfdu	16,8(3)
	lfdu	17,8(3)
	lfdu	18,8(3)
	lfdu	19,8(3)
	lfdu	20,8(3)
	lfdu	21,8(3)
	lfdu	22,8(3)
	lfdu	23,8(3)
	lfdu	24,8(3)
	lfdu	25,8(3)
	lfdu	26,8(3)
	lfdu	27,8(3)
	lfdu	28,8(3)
	lfdu	29,8(3)
	lfdu	30,8(3)
	lfdu	31,8(3)
	lfdu	0,8(3)
	mtfsf	127,0
	lfd	0,0(4)
	lwz	3,4(1)
	lwz	4,8(1)
	lwz	1,0(1)
	blr
