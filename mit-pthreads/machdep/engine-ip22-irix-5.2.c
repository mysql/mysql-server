/* ==== machdep.c ============================================================
 * Copyright (c) 1995 Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Machine dependent functions for IRIX-5.2 on the IP22
 *
 *	1.00 95/04/26 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif
 
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

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
	int i;

    machdep_pthread->start_routine = start_routine;
    machdep_pthread->start_argument = start_argument;

    machdep_pthread->machdep_timer.it_value.tv_sec = 0;
    machdep_pthread->machdep_timer.it_interval.tv_sec = 0;
    machdep_pthread->machdep_timer.it_interval.tv_usec = 0;
    machdep_pthread->machdep_timer.it_value.tv_usec = nsec / 1000;

	if (setjmp(machdep_pthread->machdep_state)) {
		machdep_pthread_start();
	}

    /*
     * Set up new stact frame so that it looks like it
     * returned from a longjmp() to the beginning of
     * machdep_pthread_start().
     */

   	/* IP22 stack starts high and builds down. */
    machdep_pthread->machdep_state[JB_SP] =
	  (int)machdep_pthread->machdep_stack + stack_size - 1024; 
	machdep_pthread->machdep_state[JB_SP] &= ~7;

	memcpy((void *)machdep_pthread->machdep_state[JB_SP], 
		   (char *)(((int)&i) - 24), 32);

}

/* ==========================================================================
 * machdep_sys_dup2()
 */
machdep_sys_dup2(int a, int b)
{
    machdep_sys_close(b);
    machdep_sys_fcntl(a, F_DUPFD, b);
}

/* ==========================================================================
 * machdep_sys_wait3()
 */
machdep_sys_wait3(int * b, int c, int * d)
{
        return(machdep_sys_waitsys(0, b, c, d));
}
 
/* ==========================================================================
 * machdep_sys_waitpid()
 */
machdep_sys_waitpid(int a, int * b, int c)
{
        return(machdep_sys_waitsys(a, b, c, NULL));
}  

struct stat;

/* ==========================================================================
 * _fxstat()
 */
int _fxstat(int __ver, int fd, struct stat *buf)
{
    int ret;

    if ((ret = fd_lock(fd, FD_READ, NULL)) == OK) {
        if ((ret = machdep_sys_fstat(fd_table[fd]->fd.i, buf)) < OK) {
        	SET_ERRNO(-ret);
    	}
        fd_unlock(fd, FD_READ);
    }
    return(ret);
}

/* ==========================================================================
 * _lxstat()
 */
int _lxstat(int __ver, const char * path, struct stat * buf)
{
    int ret;

    if ((ret = machdep_sys_lstat(path, buf)) < OK) {
        SET_ERRNO(-ret);
    }
    return(ret);

}

/* ==========================================================================
 * _xstat()
 */
int _xstat(int __ver, const char * path, struct stat * buf)
{
    int ret;

    if ((ret = machdep_sys_stat(path, buf)) < OK) {
        SET_ERRNO(-ret);
    }
    return(ret);

}

/* ==========================================================================
 * getdtablesize()
 */
machdep_sys_getdtablesize()
{
    return(sysconf(_SC_OPEN_MAX));
}

/* ==========================================================================
 * machdep_sys_getdirentries()
 */
int machdep_sys_getdirentries(int fd, char * buf, int len, int * seek)
{  
	int i;

	i = machdep_sys_getdents(fd, buf, len);
	return i;
}
