/* ==== machdep.c ============================================================
 * Copyright (c) 1993, 1994 Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Machine dependent functions for NetBSD/Alpha 1.1(+)
 *
 *	1.00 93/08/04 proven
 *      -Started coding this file.
 *
 *	95/04/22 cgd
 *	-Modified to make it go with NetBSD/Alpha
 */

#ifndef lint
static const char rcsid[] = "engine-alpha-osf1.c,v 1.4.4.1 1995/12/13 05:41:37 proven Exp";
#endif
 
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_state(void)
{
  return __machdep_save_int_state(pthread_run->machdep_data.machdep_istate);
}

void machdep_restore_state(void)
{
  __machdep_restore_int_state(pthread_run->machdep_data.machdep_istate);
}

void machdep_save_float_state (void)
{
  __machdep_save_fp_state(pthread_run->machdep_data.machdep_fstate);
}

void machdep_restore_float_state (void)
{
  __machdep_restore_fp_state(pthread_run->machdep_data.machdep_fstate);
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

    /* Set up new stack frame so that it looks like it returned from a
       longjmp() to the beginning of machdep_pthread_start().  */
    machdep_pthread->machdep_istate[8/*ISTATE_RA*/] = 0;
    machdep_pthread->machdep_istate[0/*ISTATE_PC*/] = (long)machdep_pthread_start;
    machdep_pthread->machdep_istate[10/*ISTATE_PV*/] = (long)machdep_pthread_start;

    /* Alpha stack starts high and builds down. */
    {
      long stk_addr = (long) machdep_pthread->machdep_stack;
      stk_addr += stack_size - 1024;
      stk_addr &= ~15;
      machdep_pthread->machdep_istate[9/*ISTATE_SP*/] = stk_addr;
    }
}

int safe_store (loc, new)
     int *loc;
     int new;
{
  int locked, old;
  asm ("mb" : : : "memory");
  do {
    asm ("ldl_l %0,%1" : "=r" (old) : "m" (*loc));
    asm ("stl_c %0,%1" : "=r" (locked), "=m" (*loc) : "0" (new));
  } while (!locked);
  asm ("mb" : : : "memory");
  return old;
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
 * machdep_sys_lseek()
 */
off_t machdep_sys_lseek(int fd, off_t offset, int whence)
{
	extern off_t __syscall();

	return(__syscall((quad_t)SYS_lseek, fd, 0, offset, whence));
}
