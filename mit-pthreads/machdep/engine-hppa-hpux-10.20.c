/* ==== machdep.c ============================================================
 * Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Machine dependent functions for HP-UX 9.03 on hppa
 *
 *	1.00 93/12/14 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>

volatile int setupStack = 0;

/* ==========================================================================
  * machdep_save_state()
 */
int machdep_save_state(void)
{
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
void machdep_restore_float_state()
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
void machdep_pthread_start(jmp_buf j)
{
  setjmp(j);
  if( setupStack )
    return;

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
    jmp_buf tmp_jmp_buf;
   
    machdep_pthread->start_routine = start_routine;
    machdep_pthread->start_argument = start_argument;

    machdep_pthread->machdep_timer.it_value.tv_sec = 0;
    machdep_pthread->machdep_timer.it_interval.tv_sec = 0;
    machdep_pthread->machdep_timer.it_interval.tv_usec = 0;
    machdep_pthread->machdep_timer.it_value.tv_usec = nsec / 1000;

    /*
     * Set up new stack frame so that it looks like it
     * returned from a longjmp() to the beginning of
     * machdep_pthread_start().
     */
    setjmp(machdep_pthread->machdep_state);

    /* get the stack frame from the real machdep_pthread_start */
    setupStack = 1;
/*    machdep_pthread_start(machdep_pthread->machdep_state); */
    machdep_pthread_start(tmp_jmp_buf);
    setupStack = 0;
    
    /* copy over the interesting part of the frame */
    ((int *)machdep_pthread->machdep_state)[44] = ((int *)tmp_jmp_buf)[44];
        
    /* Stack starts low and builds up, but needs two start frames */
    ((int *)machdep_pthread->machdep_state)[1] =
      (int)machdep_pthread->machdep_stack + (64 * 2);
}

int machdep_sys_getdtablesize()
{
	return sysconf(_SC_OPEN_MAX);
}

void sig_check_and_resume()
{
 	return;
}

void ___exit(int status)
{
	exit(status);
        PANIC();
}
