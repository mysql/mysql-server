/* ==== machdep.c ============================================================
 * Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Machine dependent functions for decstation with r2000/r3000
 *
 *  1.00 93/07/21 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

/*
 * The r2000/r3000 processors do not have a test and set instruction, so 
 * the semaphore TEST_AND_SET macro is linked very closely to the interrupt
 * handelling of the pthreads package.
 */

/* ==========================================================================
 * semaphore_test_and_set()
 *
 * SEMAPHORE_TEST_AND_SET prevents interrupts, tests the lock and then
 * turns interrupts back on, checking to see if any interrupts have occured
 * between the prevent and resume.
 */
int semaphore_test_and_set(semaphore *lock)
{
	int rval;

/* None of this should be necessary
	sig_prevent();
	if (!(rval = (*lock))) {
		*lock = SEMAPHORE_SET;
	}
	sig_check_and_resume();
	return(rval);
*/
}

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_state(void)
{
	return(setjmp(pthread_run->machdep_data.machdep_state));
}

/* ==========================================================================
 * machdep_save_float_state()
 */
void machdep_save_float_state(struct pthread * pthread)
{
	return;
}

/* ==========================================================================
 * fake_longjmp()
 */
void fake_longjmp(jmp_buf env)
{
	asm("li $5,1; sw $5, 20($4); li $2,103; syscall"); 
}

/* ==========================================================================
 * machdep_restore_state()
 *
 * When I redo machdep_save_state, I'll put the asm in machdep_save_state()
 * and machdep_restore_state() and I won't have to do an additional function
 * call.
 */
void machdep_restore_state(void)
{
	fake_longjmp(pthread_run->machdep_data.machdep_state);
	/* longjmp(pthread_run->machdep_data.machdep_state, 1); */
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
	pthread_sched_resume();

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

	setjmp(machdep_pthread->machdep_state);
	/*
	 * Set up new stact frame so that it looks like it
	 * returned from a longjmp() to the beginning of
	 * machdep_pthread_start().
	 */
	machdep_pthread->machdep_state[JB_RA] = (int)machdep_pthread_start;
	machdep_pthread->machdep_state[JB_PC] = (int)machdep_pthread_start;

	/* Stack starts high and builds down. */
	machdep_pthread->machdep_state[JB_SP] =
	  (int)machdep_pthread->machdep_stack + stack_size;

	/* This is the real global pointer */
	/* machdep_pthread->machdep_state[JB_GP] = 0; */
}
		
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

