/* ==== pthread_cancel.c ====================================================
 * Copyright (c) 1996 by Larry V. Streepy, Jr.
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
 *  This product includes software developed by Larry V. Streepy, Jr.
 * 4. The name of Larry V. Streepy, Jr. may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Larry V. Streepy, Jr. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Larry V. Streepy, Jr. BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : pthread_cancel operations
 *
 * 27 Sep 1996 - Larry V. Streepy, Jr.
 *      - Initial coding
 */
#ifndef lint  
static const char rcsid[] = "$Id:";
#endif
#include <pthread.h>
#include <errno.h>
static void possiblyMakeRunnable( pthread_t pthread );

/*----------------------------------------------------------------------
 * Function:	pthread_cancel
 * Purpose:		Allows a thread to request that it or another thread
 *				terminate execution
 * Args:
 *		thread	= thread to mark as cancelled
 * Returns:
 *		int	0 = ok, -1 = some error (see errno)
 * Notes:
 *		The thread is simply marked as CANCELLED, it is up to the cancelled
 *		thread to decide how to handle it.
 *----------------------------------------------------------------------*/  int  
pthread_cancel( pthread_t pthread )
{
  int rtn = 0;				/* Assume all ok */
  pthread_sched_prevent();
  /* Ensure they gave us a legal pthread pointer */
  if( ! __pthread_is_valid( pthread ) ) {
    rtn = ESRCH;					/* No such thread */
  } else if( pthread->state == PS_UNALLOCED || pthread->state == PS_DEAD ) {
    /* The standard doesn't call these out as errors, so return 0 */
    rtn = 0;
  } else {
    SET_PF_CANCELLED(pthread);		/* Set the flag */
    /* If the thread is in the right state, then stick it on the
     * run queue so it will get a chance to process the cancel.
     */
    if( pthread != pthread_run ) {
      possiblyMakeRunnable( pthread );
    }
  }
  pthread_sched_resume();
  if( rtn == 0 )
    pthread_testcancel();		/* See if we cancelled ourself */
  return rtn;
}

/*----------------------------------------------------------------------
 * Function:	pthread_setcancelstate
 * Purpose:	Set the current thread's cancellability state
 * Args:
 *		state	= PTHREAD_CANCEL_DISABLE or PTHREAD_CANCEL_ENABLE
 *		oldstate= pointer to holder for old state or NULL (*MODIFIED*)
 * Returns:
 *		int	0        = ok
 *			EINVAL   = state is neither of the legal states
 * Notes:
 *		This has to be async-cancel safe, so we prevent scheduling in
 *		here
 *----------------------------------------------------------------------*/

int  
pthread_setcancelstate( int newstate, int *oldstate )
{
  int ostate = TEST_PF_CANCEL_STATE(pthread_run);
  int rtn = 0;
  pthread_sched_prevent();
  if( newstate == PTHREAD_CANCEL_ENABLE ||
      newstate == PTHREAD_CANCEL_DISABLE ) {
    SET_PF_CANCEL_STATE(pthread_run, newstate);
    if( oldstate != NULL )
      *oldstate = ostate;
  } else {				/* Invalid new state */
    rtn = EINVAL;
  }
  pthread_sched_resume();
  if( rtn == 0 ) {
    /* Test to see if we have a pending cancel to handle */
    pthread_testcancel();
  }
  return rtn;
}

/*----------------------------------------------------------------------
 * Function:	pthread_setcanceltype
 * Purpose:	Set the current thread's cancellability type
 * Args:
 *		type	= PTHREAD_CANCEL_DEFERRED or PTHREAD_CANCEL_ASYNCHRONOUS
 *		oldtype	= pointer to holder for old type or NULL (*MODIFIED*)
 * Returns:
 *		int	0        = ok
 *			EINVAL   = type is neither of the legal states
 * Notes:
 *		This has to be async-cancel safe, so we prevent scheduling in
 *		 here
 *----------------------------------------------------------------------*/

int  
pthread_setcanceltype( int newtype, int *oldtype )
{
  int otype = TEST_PF_CANCEL_TYPE(pthread_run);
  int rtn = 0;
  pthread_sched_prevent();
  if( newtype == PTHREAD_CANCEL_DEFERRED ||
      newtype == PTHREAD_CANCEL_ASYNCHRONOUS) {
    SET_PF_CANCEL_TYPE(pthread_run, newtype);
    if( oldtype != NULL )
      *oldtype = otype;
  } else {				/* Invalid new type */
    rtn = EINVAL;
  }
  pthread_sched_resume();
  if( rtn == 0 ) {
    /* Test to see if we have a pending cancel to handle */
    pthread_testcancel();
  }
  return rtn;
}

/*----------------------------------------------------------------------
 * Function:	pthread_testcancel
 * Purpose:	Requests delivery of a pending cancel to the current thread
 * Args:	void
 * Returns:	void
 * Notes:
 *		If the current thread has been cancelled, this function will not
 *		return and the threads exit processing will be initiated.
 *----------------------------------------------------------------------*/

void
pthread_testcancel( void )
{
  if(  TEST_PF_CANCEL_STATE(pthread_run) == PTHREAD_CANCEL_DISABLE ) {
    return;				/* Can't be cancelled */
  }
  /* Ensure that we aren't in the process of exiting already */
  if( TEST_PF_RUNNING_TO_CANCEL(pthread_run) )
    return;

  /* See if we have been cancelled */
  if( TEST_PF_CANCELLED(pthread_run) ) {
    /* Set this flag to avoid recursively calling pthread_exit */
    SET_PF_RUNNING_TO_CANCEL(pthread_run);
    pthread_exit( PTHREAD_CANCELLED );	/* Easy - just call pthread_exit */
  }
  return;				/* Not cancelled */
}

/*----------------------------------------------------------------------
 * Function:	pthread_cancel_internal
 * Purpose:	An internal routine to begin the cancel processing
 * Args:	freelocks = do we need to free locks before exiting
 * Returns:	void
 * Notes:
 *		This routine is called from pthread_resched_resume
 *		prior to a context switch, and after a thread has resumed.
 *
 *		The kernel must *NOT* be locked on entry here
 *----------------------------------------------------------------------*/

void  
pthread_cancel_internal( int freelocks )
{
  pthread_sched_prevent();		/* gotta stay focused */
  /* Since we can be called from pthread_resched_resume(), our
   * state is currently not PS_RUNNING.  Since we side stepped
   * the actually blocking, we need to be removed from the queue
   * and marked as running.
   */
  if( pthread_run->state != PS_RUNNING ) {
    if( pthread_run->queue == NULL ) {
      PANIC();				/* Must be on a queue */
    }
    /* We MUST NOT put the thread on the prio_queue here.  It
     * is already running (although it's state has changed) and if we
     * put it on the run queue, it will get resumed after it is dead
     * and we end up with a nice panic.
     */
    pthread_queue_remove(pthread_run->queue, pthread_run);
    pthread_run->state = PS_RUNNING;	/* we are running */
  }
  /* Set this flag to avoid recursively calling pthread_exit */
  SET_PF_RUNNING_TO_CANCEL(pthread_run);
  /* Free up any locks we hold if told to. */
  if( freelocks ) {
    fd_unlock_for_cancel();
  }
  pthread_sched_resume();
  pthread_exit( PTHREAD_CANCELLED );	/* Easy - just call pthread_exit */
}

/*----------------------------------------------------------------------
 * Function:	possiblyMakeRunnable
 * Purpose:	Make a thread runnable so it can be cancelled if state allows
 * Args:
 *		pthread	= thread to process
 * Returns:
 * Notes:
 *----------------------------------------------------------------------*/

static void  
possiblyMakeRunnable( pthread_t pthread )
{
  if( ! TEST_PTHREAD_IS_CANCELLABLE(pthread) )
    return;				/* Not currently cancellable */
  /* If the thread is currently runnable, then we just let things
   * take their course when it is next resumed.
   */
  if( pthread->state == PS_RUNNING )
    return;				/* will happen at context switch */
  /* If the thread is sleeping, the it isn't on a queue. */
  if( pthread->state == PS_SLEEP_WAIT ) {
    sleep_cancel( pthread );                /* Remove from sleep list */
  } else {
    /* Otherwise, we need to take it off the queue and make it runnable */
    if( pthread->queue == NULL ) {
      PANIC();				/* Must be on a queue */
    }
    pthread_queue_remove(pthread->queue, pthread);
  }
  /* And make it runnable */
  pthread_prio_queue_enq(pthread_current_prio_queue, pthread);
  pthread->old_state = pthread->state;
  pthread->state = PS_RUNNING;
}
