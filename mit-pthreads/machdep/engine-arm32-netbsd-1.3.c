/* ==== machdep.c ============================================================
 * Copyright (c) 1993, 1994 Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Machine dependent functions for NetBSD on arm32
 *
 *	1.00 93/08/04 proven
 *      -Started coding this file.
 *
 *	98/10/22 bad
 *	-adapt from i386 version
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

#if defined(_JB_REG_R13)
#define REG_LR	_JB_REG_R14
#define REG_SP	_JB_REG_R13
#else
#define REG_LR	JMPBUF_REG_R14
#define REG_SP	JMPBUF_REG_R13
#endif

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_state(void)
{
    return(_setjmp(pthread_run->machdep_data.machdep_state));
}

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_float_state(struct pthread * pthread)
{
	return;
}

/* ==========================================================================
 * machdep_restore_state()
 */
void machdep_restore_state(void)
{
    _longjmp(pthread_run->machdep_data.machdep_state, 1);
}

/* ==========================================================================
 * machdep_restore_float_state()
 */
int machdep_restore_float_state(void)
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
    struct itimerval zeroval = { { 0, 0 }, { 0, 0 } };
	int ret;

	if (machdep_pthread) {
    	ret = setitimer(ITIMER_VIRTUAL, &zeroval, 
		  &(machdep_pthread->machdep_timer));
	} else {
    	ret = setitimer(ITIMER_VIRTUAL, &zeroval, NULL); 
    }

	if (ret) {
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

    _setjmp(machdep_pthread->machdep_state);
    /*
     * Set up new stact frame so that it looks like it
     * returned from a longjmp() to the beginning of
     * machdep_pthread_start().
     */
    machdep_pthread->machdep_state[REG_LR] = (int)machdep_pthread_start;

    /* Stack starts high and builds down. */
    machdep_pthread->machdep_state[REG_SP] =
      (int)machdep_pthread->machdep_stack + stack_size;
}

/* ==========================================================================
 * machdep_sys_creat()
 */
machdep_sys_creat(char * path, int mode)
{
        return(machdep_sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, mode));
}
 
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
machdep_sys_waitpid(int a, int * b, int c)
{
        return(machdep_sys_wait4(a, b, c, NULL));
}  

/* ==========================================================================
 * machdep_sys_getdtablesize()
 */
machdep_sys_getdtablesize()
{
        return(sysconf(_SC_OPEN_MAX));
}  

/* ==========================================================================
 * machdep_sys_getdirentries()
 */
machdep_sys_getdirentries(int fd, char * buf, int len, int * seek)
{
        return(machdep_sys_getdents(fd, buf, len));
}  
