/* ==== machdep.c ============================================================
 * Copyright (c) 1993, 1994 Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Machine dependent functions for SunOS-4.1.3 on sparc
 *
 *	1.00 93/08/04 proven
 *      -Started coding this file.
 *
 *	98/10/22 bad
 *	-update for fat sigset_t in NetBSD 1.3H
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif
 
#include "config.h"
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_state(void)
{
	/* Save register windows onto stackframe */
	__asm__ ("ta 3");

    return(setjmp(pthread_run->machdep_data.machdep_state));
}

/* ==========================================================================
 * machdep_restore_state()
 */
void machdep_restore_state(void)
{
    longjmp(pthread_run->machdep_data.machdep_state, 1);
}
/* ==========================================================================
 * machdep_save_float_state()
 */
void machdep_save_float_state(struct pthread * pthread)
{
	return;
}

/* ==========================================================================
 * machdep_restore_float_state()
 */
void machdep_restore_float_state(void)
{
	return;
}

/* ==========================================================================
 * machdep_set_thread_timer()
 */
void machdep_set_thread_timer(struct machdep_pthread *machdep_pthread)
{
    if (setitimer(ITIMER_VIRTUAL, &(machdep_pthread->machdep_timer), NULL)) {
        PANIC();
    }
}

/* ==========================================================================
 * machdep_unset_thread_timer()
 */
void machdep_unset_thread_timer(struct machdep_pthread *machdep_pthread)
{
    struct itimerval zeroval = { { 0, 0 }, { 0, 0} };

    if (setitimer(ITIMER_VIRTUAL, &zeroval, NULL)) {
        PANIC();
    }
}

/* ==========================================================================
 * machdep_pthread_cleanup()
 */
void *machdep_pthread_cleanup(struct machdep_pthread *machdep_pthread)
{
    return(machdep_pthread->machdep_stack);
}

/* ==========================================================================
 * machdep_pthread_start()
 */
void machdep_pthread_start(void)
{
	context_switch_done();
	pthread_sched_resume ();

    /* Run current threads start routine with argument */
    pthread_exit(pthread_run->machdep_data.start_routine
      (pthread_run->machdep_data.start_argument));

    /* should never reach here */
    PANIC();
}

/* ==========================================================================
 * __machdep_stack_free()
 */
void __machdep_stack_free(void * stack)
{       
    free(stack);
}
 
/* ==========================================================================
 * __machdep_stack_alloc()
 */ 
void * __machdep_stack_alloc(size_t size)
{   
    void * stack;
    
    return(malloc(size));
}     
    
/* ==========================================================================
 * __machdep_pthread_create()
 */
void __machdep_pthread_create(struct machdep_pthread *machdep_pthread,
  void *(* start_routine)(), void *start_argument, 
  long stack_size, long nsec, long flags)
{
    machdep_pthread->start_routine = start_routine;
    machdep_pthread->start_argument = start_argument;

    machdep_pthread->machdep_timer.it_value.tv_sec = 0;
    machdep_pthread->machdep_timer.it_interval.tv_sec = 0;
    machdep_pthread->machdep_timer.it_interval.tv_usec = 0;
    machdep_pthread->machdep_timer.it_value.tv_usec = nsec / 1000;

	/* Save register windows onto stackframe */
	__asm__ ("ta 3");

	setjmp(machdep_pthread->machdep_state);
    /*
     * Set up new stact frame so that it looks like it
     * returned from a longjmp() to the beginning of
     * machdep_pthread_start().
     */
    machdep_pthread->machdep_state[3] = (int)machdep_pthread_start;
    machdep_pthread->machdep_state[4] = (int)machdep_pthread_start;

   	/* Sparc stack starts high and builds down. */
    machdep_pthread->machdep_state[2] =
	  (int)machdep_pthread->machdep_stack + stack_size - 1024; 
	machdep_pthread->machdep_state[2] &= ~7;

}

#if defined(HAVE_SYSCALL_GETDENTS)
/* ==========================================================================
 * machdep_sys_getdirentries()
 *
 * Always use getdents in place of getdirentries if possible --proven
 */
int machdep_sys_getdirentries(int fd, char * buf, int len, int * seek)
{
	return(machdep_sys_getdents(fd, buf, len));
}
#endif

/* ==========================================================================
 * machdep_sys_wait3()
 */
machdep_sys_wait3(int * b, int c, int * d)
{
        return(machdep_sys_wait4(0, b, c, d));
}

/* ==========================================================================
 * machdep_sys_waitpid()
 */
machdep_sys_waitpid(int pid, int * statusp, int options)
{
	if (pid == -1)
		pid = 0;
	else if (pid == 0)
		pid = - getpgrp ();
	return machdep_sys_wait4 (pid, statusp, options, NULL);
}

#if !defined(HAVE_SYSCALL_SIGPROCMASK) 
#if 0
/* ==========================================================================
 * machdep_sys_sigprocmask()
 * This isn't a real implementation; we can make the assumption that the
 * pthreads library is not using oset, and that it is always blocking or
 * unblocking all signals at once.
 */
int machdep_sys_sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
    switch(how) {
      case SIG_BLOCK:
	sigblock(*set);
	break;
      case SIG_UNBLOCK:
	sigsetmask(~*set);
	break;
      case SIG_SETMASK:
	sigsetmask(*set);
	break;
      default:
	return -EINVAL;
    }
    return(OK);
}

/* ==========================================================================
 * sigaction()
 *
 * Temporary until I do machdep_sys_sigaction()
 */
int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
	return(sigvec(sig, (struct sigvec *)act, (struct sigvec *)oldact));
}
#endif
#endif

#if !defined(HAVE_SYSCALL_GETDTABLESIZE) 
/* ==========================================================================
 * machdep_sys_getdtablesize()
 */
machdep_sys_getdtablesize()
{
        return(sysconf(_SC_OPEN_MAX));
} 
#endif
