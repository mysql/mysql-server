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
#include <stdlib.h>

/* These would be defined in setjmp.h, if _POSIX_SOURCE and _XOPEN_SOURCE
   were both undefined.  But we've already included it, and lost the
   opportunity.  */
#define JB_PC	 2
#define JB_RA	30
#define JB_PV	31
#define JB_SP	34

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_state(void)
{
  return setjmp (pthread_run->machdep_data.machdep_state);
}

/* ==========================================================================
 * machdep_restore_state()
 */
extern void machdep_restore_from_setjmp (jmp_buf, long);
void machdep_restore_state(void)
{
  machdep_restore_from_setjmp (pthread_run->machdep_data.machdep_state, 1);
}

void machdep_save_float_state (void) { }
void machdep_restore_float_state (void) { }

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

    setjmp(machdep_pthread->machdep_state);

    /* Set up new stack frame so that it looks like it returned from a
       longjmp() to the beginning of machdep_pthread_start().  */
    machdep_pthread->machdep_state[JB_RA] = 0;
    machdep_pthread->machdep_state[JB_PC] = (long)machdep_pthread_start;
    machdep_pthread->machdep_state[JB_PV] = (long)machdep_pthread_start;

    /* Alpha stack starts high and builds down. */
    {
      long stk_addr = (long) machdep_pthread->machdep_stack;
      stk_addr += stack_size - 1024;
      stk_addr &= ~15;
      machdep_pthread->machdep_state[JB_SP] = stk_addr;
    }
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
machdep_sys_waitpid(int pid, int * statusp, int options)
{
  return machdep_sys_wait4 (pid, statusp, options, NULL);
}

/* These are found in flsbuf.o in the Alpha libc.  I don't know what
   they're for, precisely.  */
static xxx;
_bufsync (p)
     char *p;
{
  long a1 = *(long *)(p+48);
  long t0 = *(long *)(p+8);
  long v0 = a1 - t0;
  long t1, t2;

  abort ();

  v0 += xxx;
  if (v0 < 0)
    {
      *(char**)(p + 8) = p;
      return v0;
    }
  t1 = *(int*)p;
  t2 = v0 - t1;
  if (t2 < 0)
    *(int*)p = (int) v0;
  return v0;
}

_findbuf () { abort (); }
_wrtchk () { abort (); }
_xflsbuf () { abort (); }
_flsbuf () { abort (); }

void __xxx_never_called () {
  /* Force other stuff to get dragged in.  */
  _cleanup ();
  fflush (NULL);
  fclose (NULL);
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
