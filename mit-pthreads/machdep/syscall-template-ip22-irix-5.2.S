#include <sys.s>
#include <sys/regdef.h>
/* #include <sys/asm.h> */

/*
 	Kernel syscall interface:
   	Input:
		v0		syscall	number
   	Output:

   	This macro is similar to SYSCALL in sys/syscall.h, but not completely.
   	There's room for optimization, if we assume this will continue to
   	be assembled as one file.

	Compile with -DPIC for pic code.
*/

#ifdef PIC
#define PICOPT  .option pic2
#else
#define PICOPT
#endif


#define YSYSCALL(x) 					\
	PICOPT;								\
	.globl 		machdep_sys_##x;		\
	.ent		machdep_sys_##x, 0;		\
machdep_sys_##x:;						\
	.frame  sp,0,ra;					\
	.set	noreorder;					\
	li	v0, SYS_##x;					\
	syscall;							\
	bne a3, zero, 1b;					\
	nop;								\
	j ra;								\
	nop;								\
	.end 		machdep_sys_##x

/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value.
 */

1:
	subu v0,zero,v0;
	j ra;

#define XSYSCALL(x)		YSYSCALL(x)

XSYSCALL(SYSCALL_NAME)	
