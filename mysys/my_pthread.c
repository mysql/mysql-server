/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Functions to get threads more portable */

#define DONT_REMAP_PTHREAD_FUNCTIONS

#include "mysys_priv.h"
#include <signal.h>
#include <m_string.h>
#include <thr_alarm.h>

#if (defined(__BSD__) || defined(_BSDI_VERSION))
#define SCHED_POLICY SCHED_RR
#else
#define SCHED_POLICY SCHED_OTHER
#endif

uint thd_lib_detected= 0;

/*
  Some functions for RTS threads, AIX, Siemens Unix and UnixWare 7
  (and DEC OSF/1 3.2 too)
*/

int my_pthread_create_detached=1;

/* localtime_r for SCO 3.2V4.2 */

#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)

extern mysql_mutex_t LOCK_localtime_r;

#endif

#if !defined(HAVE_LOCALTIME_R)
struct tm *localtime_r(const time_t *clock, struct tm *res)
{
  struct tm *tmp;
  mysql_mutex_lock(&LOCK_localtime_r);
  tmp=localtime(clock);
  *res= *tmp;
  mysql_mutex_unlock(&LOCK_localtime_r);
  return res;
}
#endif

#if !defined(HAVE_GMTIME_R)
/* 
  Reentrant version of standard gmtime() function. 
  Needed on some systems which don't implement it.
*/

struct tm *gmtime_r(const time_t *clock, struct tm *res)
{
  struct tm *tmp;
  mysql_mutex_lock(&LOCK_localtime_r);
  tmp= gmtime(clock);
  *res= *tmp;
  mysql_mutex_unlock(&LOCK_localtime_r);
  return res;
}
#endif

/****************************************************************************
** Replacement of sigwait if the system doesn't have one (like BSDI 3.0)
**
** Note:
** This version of sigwait() is assumed to called in a loop so the signalmask
** is permanently modified to reflect the signal set. This is done to get
** a much faster implementation.
**
** This implementation isn't thread safe: It assumes that only one
** thread is using sigwait.
**
** If one later supplies a different signal mask, all old signals that
** was used before are unblocked and set to SIGDFL.
**
** Author: Gary Wisniewski <garyw@spidereye.com.au>, much modified by Monty
****************************************************************************/

#if !defined(HAVE_SIGWAIT) && !defined(sigwait) && !defined(__WIN__) && !defined(HAVE_rts_threads)

#if !defined(DONT_USE_SIGSUSPEND)

static sigset_t sigwait_set,rev_sigwait_set,px_recd;

void px_handle_sig(int sig)
{
  sigaddset(&px_recd, sig);
}


void sigwait_setup(sigset_t *set)
{
  int i;
  struct sigaction sact,sact1;
  sigset_t unblock_mask;

  sact.sa_flags = 0;
  sact.sa_handler = px_handle_sig;
  memcpy(&sact.sa_mask, set, sizeof(*set));    /* handler isn't thread_safe */
  sigemptyset(&unblock_mask);
  pthread_sigmask(SIG_UNBLOCK,(sigset_t*) 0,&rev_sigwait_set);

  for (i = 1; i <= sizeof(sigwait_set)*8; i++)
  {
    if (sigismember(set,i))
    {
      sigdelset(&rev_sigwait_set,i);
      if (!sigismember(&sigwait_set,i))
	sigaction(i, &sact, (struct sigaction*) 0);
    }
    else
    {
      sigdelset(&px_recd,i);			/* Don't handle this */
      if (sigismember(&sigwait_set,i))
      {						/* Remove the old handler */
	sigaddset(&unblock_mask,i);
	sigdelset(&rev_sigwait_set,i);
	sact1.sa_flags = 0;
	sact1.sa_handler = SIG_DFL;
	sigemptyset(&sact1.sa_mask);
	sigaction(i, &sact1, 0);
      }
    }
  }
  memcpy(&sigwait_set, set, sizeof(*set));
  pthread_sigmask(SIG_BLOCK,(sigset_t*) set,(sigset_t*) 0);
  pthread_sigmask(SIG_UNBLOCK,&unblock_mask,(sigset_t*) 0);
}


int sigwait(sigset_t *setp, int *sigp)
{
  if (memcmp(setp,&sigwait_set,sizeof(sigwait_set)))
    sigwait_setup(setp);			/* Init or change of set */

  for (;;)
  {
    /*
      This is a fast, not 100% portable implementation to find the signal.
      Because the handler is blocked there should be at most 1 bit set, but
      the specification on this is somewhat shady so we use a set instead a
      single variable.
      */

    ulong *ptr= (ulong*) &px_recd;
    ulong *end=ptr+sizeof(px_recd)/sizeof(ulong);

    for ( ; ptr != end ; ptr++)
    {
      if (*ptr)
      {
	ulong set= *ptr;
	int found= (int) ((char*) ptr - (char*) &px_recd)*8+1;
	while (!(set & 1))
	{
	  found++;
	  set>>=1;
	}
	*sigp=found;
	sigdelset(&px_recd,found);
	return 0;
      }
    }
    sigsuspend(&rev_sigwait_set);
  }
  return 0;
}
#else  /* !DONT_USE_SIGSUSPEND */

/****************************************************************************
** Replacement of sigwait if the system doesn't have one (like BSDI 3.0)
**
** Note:
** This version of sigwait() is assumed to called in a loop so the signalmask
** is permanently modified to reflect the signal set. This is done to get
** a much faster implementation.
**
** This implementation uses a extra thread to handle the signals and one
** must always call sigwait() with the same signal mask!
**
** BSDI 3.0 NOTE:
**
** pthread_kill() doesn't work on a thread in a select() or sleep() loop?
** After adding the sleep to sigwait_thread, all signals are checked and
** delivered every second. This isn't that terrible performance vice, but
** someone should report this to BSDI and ask for a fix!
** Another problem is that when the sleep() ends, every select() in other
** threads are interrupted!
****************************************************************************/

static sigset_t pending_set;
static bool inited=0;
static pthread_cond_t  COND_sigwait;
static pthread_mutex_t LOCK_sigwait;


void sigwait_handle_sig(int sig)
{
  pthread_mutex_lock(&LOCK_sigwait);
  sigaddset(&pending_set, sig);
  pthread_cond_signal(&COND_sigwait); /* inform sigwait() about signal */
  pthread_mutex_unlock(&LOCK_sigwait);
}

void *sigwait_thread(void *set_arg)
{
  sigset_t *set=(sigset_t*) set_arg;

  int i;
  struct sigaction sact;
  sact.sa_flags = 0;
  sact.sa_handler = sigwait_handle_sig;
  memcpy(&sact.sa_mask, set, sizeof(*set));    /* handler isn't thread_safe */
  sigemptyset(&pending_set);

  for (i = 1; i <= sizeof(pending_set)*8; i++)
  {
    if (sigismember(set,i))
    {
      sigaction(i, &sact, (struct sigaction*) 0);
    }
  }
  /* Ensure that init_thr_alarm() is called */
  DBUG_ASSERT(thr_client_alarm);
  sigaddset(set, thr_client_alarm);
  pthread_sigmask(SIG_UNBLOCK,(sigset_t*) set,(sigset_t*) 0);
  alarm_thread=pthread_self();			/* For thr_alarm */

  for (;;)
  {						/* Wait for signals */
    sleep(1);					/* Because of broken BSDI */
  }
}


int sigwait(sigset_t *setp, int *sigp)
{
  if (!inited)
  {
    pthread_attr_t thr_attr;
    pthread_t sigwait_thread_id;
    inited=1;
    sigemptyset(&pending_set);
    pthread_mutex_init(&LOCK_sigwait, MY_MUTEX_INIT_FAST);
    pthread_cond_init(&COND_sigwait, NULL);

    pthread_attr_init(&thr_attr);
    pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
    pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&thr_attr,8196);
    pthread_create(&sigwait_thread_id, &thr_attr, sigwait_thread, setp);
    pthread_attr_destroy(&thr_attr);
  }

  pthread_mutex_lock(&LOCK_sigwait);
  for (;;)
  {
    ulong *ptr= (ulong*) &pending_set;
    ulong *end=ptr+sizeof(pending_set)/sizeof(ulong);

    for ( ; ptr != end ; ptr++)
    {
      if (*ptr)
      {
	ulong set= *ptr;
	int found= (int) ((char*) ptr - (char*) &pending_set)*8+1;
	while (!(set & 1))
	{
	  found++;
	  set>>=1;
	}
	*sigp=found;
	sigdelset(&pending_set,found);
	pthread_mutex_unlock(&LOCK_sigwait);
	return 0;
      }
    }
    pthread_cond_wait(&COND_sigwait, &LOCK_sigwait);
  }
  return 0;
}

#endif /* DONT_USE_SIGSUSPEND */
#endif /* HAVE_SIGWAIT */


/****************************************************************************
 The following functions fixes that all pthread functions should work
 according to latest posix standard
****************************************************************************/

/* Undefined wrappers set my_pthread.h so that we call os functions */
#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_mutex_wait
#undef pthread_mutex_timedwait
#undef pthread_mutex_trylock
#undef pthread_mutex_t
#undef pthread_cond_init
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#undef pthread_cond_t
#undef pthread_attr_getstacksize

/*****************************************************************************
  Patches for HPUX
  We need these because the pthread_mutex.. code returns -1 on error,
  instead of the error code.

  Note that currently we only remap pthread_ functions used by MySQL.
  If we are depending on the value for some other pthread_xxx functions,
  this has to be added here.
****************************************************************************/

#if defined(HPUX10)

int my_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
			      struct timespec *abstime)
{
  int error=pthread_cond_timedwait(cond, mutex, abstime);
  if (error == -1)			/* Safety if the lib is fixed */
  {
    if (!(error=errno))
      error= ETIMEDOUT;			/* Can happen on HPUX */
  }
  if (error == EAGAIN)			/* Correct errno to Posix */
    error= ETIMEDOUT;
  return error;
}
#endif

#if defined(HPUX10)

void my_pthread_attr_getstacksize(pthread_attr_t *connection_attrib,
				  size_t *stack_size)
{
  *stack_size= pthread_attr_getstacksize(*connection_attrib);
}
#endif


#ifdef HAVE_POSIX1003_4a_MUTEX
/*
  In HP-UX-10.20 and other old Posix 1003.4a Draft 4 implementations
  pthread_mutex_trylock returns 1 on success, not 0 like
  pthread_mutex_lock

  From the HP-UX-10.20 man page:
  RETURN VALUES
      If the function fails, errno may be set to one of the following
      values:
           Return | Error    | Description
           _______|__________|_________________________________________
              1   |          | Successful completion.
              0   |          | The mutex is  locked; therefore, it was
                  |          | not acquired.
             -1   | [EINVAL] | The value specified by mutex is invalid.

*/

/*
  Convert pthread_mutex_trylock to return values according to latest POSIX

  RETURN VALUES
  0		If we are able successfully lock the mutex.
  EBUSY		Mutex was locked by another thread
  #		Other error number returned by pthread_mutex_trylock()
		(Not likely)  
*/

int my_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
  int error= pthread_mutex_trylock(mutex);
  if (error == 1)
    return 0;				/* Got lock on mutex */
  if (error == 0)			/* Someon else is locking mutex */
    return EBUSY;
  if (error == -1)			/* Safety if the lib is fixed */
    error= errno;			/* Probably invalid parameter */
   return error;
}
#endif /* HAVE_POSIX1003_4a_MUTEX */

/* Some help functions */

int pthread_dummy(int ret)
{
  return ret;
}
