/* ==== machdep.c ============================================================
 * Copyright (c) 1993, 1994 Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Machine dependent functions for SunOS-4.1.3 on sparc
 *
 *	1.00 93/08/04 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif
 
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/procset.h>
#include <sys/systeminfo.h>
#include <poll.h>

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
 * machdep_pthread_create()
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

	if (setjmp(machdep_pthread->machdep_state)) {
		machdep_pthread_start();
	}

    /*
     * Set up new stact frame so that it looks like it
     * returned from a longjmp() to the beginning of
     * machdep_pthread_start().
     */

   	/* Sparc stack starts high and builds down. */
    machdep_pthread->machdep_state[1] =
	  (int)machdep_pthread->machdep_stack + stack_size - 1024; 
	machdep_pthread->machdep_state[1] &= ~7;

}

/* ==========================================================================
 * machdep_sys_getdirentries()
 */
int machdep_sys_getdirentries(int fd, char * buf, int len, int * seek)
{
	return(machdep_sys_getdents(fd, buf, len));
}

/* ==========================================================================
 * machdep_sys_wait3()
 */
machdep_sys_wait3(int * b, int c, int * d)
{
	return(-ENOSYS);
    /*    return(machdep_sys_wait4(0, b, c, d)); */
}

/* ==========================================================================
 * machdep_sys_waitpid()
 */
machdep_sys_waitpid(int a, int * b, int c)
{
	idtype_t id;

	switch (a) {
	case -1:
		id = P_ALL;
		break;
	case 0:
		a = machdep_sys_pgrpsys(0);
		id = P_PGID;
		break;
	default:
		if (a < 0) {
			id = P_PGID;
			a = -a;
		} else {
			id = P_PID;
		}
		break;
	}
		
	return(machdep_sys_waitsys(id, a, b, c));
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
 * machdep_sys_ftruncate()
 */
machdep_sys_ftruncate(int a, off_t b)
{
	flock_t c;
	
	c.l_len = 0;
	c.l_start = b;
	c.l_whence	= 0;
	return(machdep_sys_fcntl(a, F_FREESP, c));
}
  
/* ==========================================================================
 * machdep_sys_select()
 * Recoded to be quicker by Monty
 */
static fd_set bogus_fds;			/* Always zero, never changed */

machdep_sys_select(int nfds, fd_set *readfds, fd_set *writefds, 
		   fd_set *exceptfds, struct timeval *timeout)
{
  struct pollfd fds[64],*ptr;
  int i, fds_count, time_out, found;

  /* Make sure each arg has a valid pointer */
  if ((readfds == NULL) || (writefds == NULL) || (exceptfds == NULL)) {
    if (exceptfds == NULL) {
      exceptfds = &bogus_fds;
    }
    if (writefds == NULL) {
      writefds = &bogus_fds;
    }
    if (readfds == NULL) {
      readfds = &bogus_fds;
    }
  }

  ptr=fds;
  for (i = 0 ; i < nfds; i++)
  {
    if (FD_ISSET(i, readfds))
    {
      if (FD_ISSET(i, writefds))
	ptr->events= POLLIN | POLLOUT;
      else
	ptr->events= POLLIN;
      (ptr++)->fd=i;
    }
    else if (FD_ISSET(i, writefds))
    {
      ptr->events=POLLOUT;
      (ptr++)->fd=i;
    }
  }
  FD_ZERO(readfds);
  FD_ZERO(writefds);
  FD_ZERO(exceptfds);
  time_out = timeout->tv_usec / 1000 + timeout->tv_sec * 1000;
  fds_count=(int) (ptr-fds);
  while ((found = machdep_sys_poll(fds, fds_count, time_out)) <= 0)
  {
    if (found != -ERESTART)		/* Try again if restartable */
      return(found);			/* Usually 0 ; Cant read or write */
  }
		
  while (ptr-- != fds)
  {
    if (ptr->revents & POLLIN)
      FD_SET(ptr->fd, readfds);
    if (ptr->revents & POLLOUT)
      FD_SET(ptr->fd,writefds);
  }
  return(found);
}

/* ==========================================================================
 * machdep_sys_getdtablesize()
 */
machdep_sys_getdtablesize()
{
	return(sysconf(_SC_OPEN_MAX));
}

/* ==========================================================================
 * getpagesize()
 */
getpagesize()
{
	return(sysconf(_SC_PAGESIZE));
}

/* ==========================================================================
 * gethostname()
 */
int gethostname(char * name, int namelen)
{
	if (sysinfo(SI_HOSTNAME, name, namelen) == NOTOK) {
		return(NOTOK);
	} else {
		return(OK);
	}
}

/* ==========================================================================
 * machdep_sys_sigaction()
 *
 * This is VERY temporary.
 */
int machdep_sys_sigaction(int a, void * b, void * c)
{
	return(sigaction(a, b, c));
}
