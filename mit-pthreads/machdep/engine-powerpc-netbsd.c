/* ==== machdep.c ============================================================
 * Copyright (c) 1993, 1994 Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Machine dependent functions for NetBSD/PowerPC (1.5+)
 *
 *     1.00 93/08/04 proven
 *      -Started coding this file.
 *
 *     2001/01/10 briggs
 *     -Modified to make it go with NetBSD/PowerPC
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
 * machdep_pthread_start()
 */
void machdep_pthread_start(void)
{
       context_switch_done();
       pthread_sched_resume ();

       /* XXXMLG
        * This is EXTREMELY bogus, but it seems that this function is called
        * with the pthread kernel locked.  If this happens, __errno() will
        * return the wrong address until after the first context switch.
        *
        * Clearly there is a leak of pthread_kernel somewhere, but until
        * it is found, we force a context switch here, just before calling
        * the thread start routine.  When we return from pthread_yield
        * the kernel will be unlocked.
        */
       pthread_yield();

    /* Run current threads start routine with argument */
    pthread_exit(pthread_run->machdep_data.start_routine
      (pthread_run->machdep_data.start_argument));

    /* should never reach here */
    PANIC();
}

/* ==========================================================================
 * __machdep_pthread_create()
 */
void __machdep_pthread_create(struct machdep_pthread *machdep_pthread,
  void *(* start_routine)(void *), void *start_argument,
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
    /* state is sigmask, then r8-r31 where r11 is the LR
     * So, istate[3] is r10, which is the SP
     * So, istate[4] is r11, which is the LR
     * So, istate[5] is r12, which is the CR
     */
    machdep_pthread->machdep_istate[4] = (long)machdep_pthread_start;
    machdep_pthread->machdep_istate[5] = 0;

    /* PowerPC stack starts high and builds down, and needs to be 16-byte
       aligned. */
    machdep_pthread->machdep_istate[3] =
	((long) machdep_pthread->machdep_stack + stack_size) & ~0xf;
}

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_state(void)
{
  return( _setjmp(pthread_run->machdep_data.machdep_istate) );
}

void machdep_restore_state(void)
{
  _longjmp(pthread_run->machdep_data.machdep_istate, 1);
}

void machdep_save_float_state (struct pthread *pthread)
{
  __machdep_save_fp_state(pthread->machdep_data.machdep_fstate);
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

void *machdep_pthread_cleanup(struct machdep_pthread *machdep_pthread);
void machdep_pthread_start(void);

/* ==========================================================================
 * __machdep_stack_free()
 */
void
__machdep_stack_free(void * stack)
{
    free(stack);
}

/* ==========================================================================
 * __machdep_stack_alloc()
 */
void *
__machdep_stack_alloc(size_t size)
{
    return(malloc(size));
}

/* ==========================================================================
 * machdep_sys_creat()
 */
int
machdep_sys_creat(char * path, int mode)
{  
        return(machdep_sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, mode));
}  

/* ==========================================================================
 * machdep_sys_wait3()
 */ 
int
machdep_sys_wait3(int * b, int c, int *d)
{   
        return(machdep_sys_wait4(0, b, c, d));
}  

/* ==========================================================================
 * machdep_sys_waitpid()
 */  
int
machdep_sys_waitpid(int a, int * b, int c)
{
        return(machdep_sys_wait4(a, b, c, NULL));
}

/* ==========================================================================
 * machdep_sys_getdtablesize()
 */
int
machdep_sys_getdtablesize(void)
{
        return(sysconf(_SC_OPEN_MAX));
}

/* ==========================================================================
 * machdep_sys_lseek()
 */
off_t
machdep_sys_lseek(int fd, off_t offset, int whence)
{
       return(__syscall((quad_t)SYS_lseek, fd, 0, offset, whence));
}

int
machdep_sys_ftruncate( int fd, off_t length)
{
       quad_t q;
       int rv;

       q = __syscall((quad_t)SYS_ftruncate, fd,0, length);
       if( /* LINTED constant */ sizeof( quad_t ) == sizeof( register_t ) ||
           /* LINTED constant */ BYTE_ORDER == LITTLE_ENDIAN )
               rv = (int)q;
       else
               rv = (int)((u_quad_t)q >> 32);

       return rv;
}


/* ==========================================================================
 * machdep_sys_getdirentries()
 */
int
machdep_sys_getdirentries(int fd, char * buf, int len, int * seek)
{
        return(machdep_sys_getdents(fd, buf, len));
} 
