#include <machine/asm.h>
#include <sys/syscall.h>

#ifdef SYS___sigsuspend14
#define SYS_sigsuspend SYS___sigsuspend14
#endif
#ifdef SYS___sigprocmask14
#define SYS_sigprocmask SYS___sigprocmask14
#endif


#ifdef __STDC__
#define IMM	#
#define SYSCALL(x)      .even; \
			ENTRY(machdep_sys_ ## x); \
                        movl IMM SYS_ ## x,d0; \
			trap IMM 0; \
			jcs err; \
			 rts
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

#define XSYSCALL(NAME)	SYSCALL(NAME)

XSYSCALL(SYSCALL_NAME)
