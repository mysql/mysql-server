/* ==== machdep.c ============================================================
 * Copyright (c) 1995 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote
 *      products derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Description : Machine dependent functions for NetBSD on i386
 *
 *	1.00 93/08/04 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "engine-i386-freebsd-2.0.c,v 1.1 1995/03/01 01:21:20 proven Exp";
#endif

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_state(void)
{
    return(machdep_sys_setjmp(pthread_run->machdep_data.machdep_state));
}

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_float_state(struct pthread * pthread)
{
	char * fdata = (char *)pthread->machdep_data.machdep_float_state;

	__asm__ ("fsave %0"::"m" (*fdata));
}

/* ==========================================================================
 * machdep_restore_state()
 */
void machdep_restore_state(void)
{
    machdep_sys_longjmp(pthread_run->machdep_data.machdep_state, 1);
}

/* ==========================================================================
 * machdep_restore_float_state()
 */
int machdep_restore_float_state(void)
{
	char * fdata = (char *)pthread_run->machdep_data.machdep_float_state;

	__asm__ ("frstor %0"::"m" (*fdata));
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

    machdep_sys_setjmp(machdep_pthread->machdep_state);
    /*
     * Set up new stact frame so that it looks like it
     * returned from a longjmp() to the beginning of
     * machdep_pthread_start().
     */
    machdep_pthread->machdep_state[0] = (int)machdep_pthread_start;

    /* Stack starts high and builds down. */
    machdep_pthread->machdep_state[2] =
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
