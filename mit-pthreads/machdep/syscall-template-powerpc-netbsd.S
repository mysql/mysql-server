#include <machine/asm.h>
#define COMPAT_43
#include <sys/syscall.h>

#ifdef SYS___sigsuspend14
#define SYS_sigsuspend SYS___sigsuspend14
#endif

#ifdef SYS___sigaction14
#define SYS_sigaction SYS___sigaction14
#endif

#ifdef SYS___sigprocmask14
#define SYS_sigprocmask SYS___sigprocmask14
#endif

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

#define SYSCALL(x) \
	ENTRY(machdep_sys_ ## x)	\
	li      0, SYS_ ## x 	;	\
	sc			;	\
	bnslr			;	\
	b       PIC_PLT(_C_LABEL(machdep_cerror))

#define XSYSCALL(x)	SYSCALL(x) ; blr

	XSYSCALL(SYSCALL_NAME)
