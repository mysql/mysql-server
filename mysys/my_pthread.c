/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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

/* To allow use of pthread_getspecific with two arguments */

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

#if !defined(HAVE_SIGWAIT) && !defined(sigwait) && !defined(__WIN__)

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

/* Some help functions */

int pthread_dummy(int ret)
{
  return ret;
}
