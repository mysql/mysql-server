/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Synchronization - readers / writer thread locks */

#include "mysys_priv.h"
#if defined(THREAD) && !defined(HAVE_PTHREAD_RWLOCK_RDLOCK) && !defined(HAVE_RWLOCK_INIT)
#include <errno.h>

#ifdef _WIN32

static BOOL have_srwlock= FALSE;
/* Prototypes and function pointers for windows  functions */
typedef VOID (WINAPI* srw_func) (PSRWLOCK SRWLock);
typedef BOOL (WINAPI* srw_bool_func) (PSRWLOCK SRWLock);

static srw_func my_InitializeSRWLock;
static srw_func my_AcquireSRWLockExclusive;
static srw_func my_ReleaseSRWLockExclusive;
static srw_func my_AcquireSRWLockShared;
static srw_func my_ReleaseSRWLockShared;

static srw_bool_func my_TryAcquireSRWLockExclusive;
static srw_bool_func my_TryAcquireSRWLockShared;

/**
  Check for presence of Windows slim reader writer lock function.
  Load function pointers.
*/

static void check_srwlock_availability(void)
{
  HMODULE module= GetModuleHandle("kernel32");

  my_InitializeSRWLock= (srw_func) GetProcAddress(module,
    "InitializeSRWLock");
  my_AcquireSRWLockExclusive= (srw_func) GetProcAddress(module,
    "AcquireSRWLockExclusive");
  my_AcquireSRWLockShared= (srw_func) GetProcAddress(module,
    "AcquireSRWLockShared");
  my_ReleaseSRWLockExclusive= (srw_func) GetProcAddress(module,
    "ReleaseSRWLockExclusive");
  my_ReleaseSRWLockShared= (srw_func) GetProcAddress(module,
    "ReleaseSRWLockShared");
  my_TryAcquireSRWLockExclusive=  (srw_bool_func) GetProcAddress(module,
    "TryAcquireSRWLockExclusive");
  my_TryAcquireSRWLockShared=  (srw_bool_func) GetProcAddress(module,
    "TryAcquireSRWLockShared");

  /*
    We currently require TryAcquireSRWLockExclusive. This API is missing on 
    Vista, this means SRWLock are only used starting with Win7.

    If "trylock" usage for rwlocks is eliminated from server codebase (it is used 
    in a single place currently, in query cache), then SRWLock can be enabled on 
    Vista too. In this case  condition below needs to be changed to  e.g check 
    for my_InitializeSRWLock.
  */

  if (my_TryAcquireSRWLockExclusive)
    have_srwlock= TRUE;

}


static int srw_init(my_rw_lock_t *rwp)
{
  my_InitializeSRWLock(&rwp->srwlock);
  rwp->have_exclusive_srwlock = FALSE;
  return 0;
}


static int srw_rdlock(my_rw_lock_t *rwp)
{
  my_AcquireSRWLockShared(&rwp->srwlock);
  return 0;
}


static int srw_tryrdlock(my_rw_lock_t *rwp)
{

  if (!my_TryAcquireSRWLockShared(&rwp->srwlock))
    return EBUSY;
  return 0;
}


static int srw_wrlock(my_rw_lock_t *rwp)
{
  my_AcquireSRWLockExclusive(&rwp->srwlock);
  rwp->have_exclusive_srwlock= TRUE;
  return 0;
}


static int srw_trywrlock(my_rw_lock_t *rwp)
{
  if (!my_TryAcquireSRWLockExclusive(&rwp->srwlock))
    return EBUSY;
  rwp->have_exclusive_srwlock= TRUE;
  return 0;
}


static int srw_unlock(my_rw_lock_t *rwp)
{
  if (rwp->have_exclusive_srwlock)
  {
    rwp->have_exclusive_srwlock= FALSE;
    my_ReleaseSRWLockExclusive(&rwp->srwlock);
  }
  else
  {
    my_ReleaseSRWLockShared(&rwp->srwlock);
  }
  return 0;
}

#endif /*_WIN32 */

/*
  Source base from Sun Microsystems SPILT, simplified for MySQL use
  -- Joshua Chamas
  Some cleanup and additional code by Monty
*/

/*
*  Multithreaded Demo Source
*
*  Copyright (C) 1995 by Sun Microsystems, Inc.
* 
*
*  This file is a product of SunSoft, Inc. and is provided for
*  unrestricted use provided that this legend is included on all
*  media and as a part of the software program in whole or part.
*  Users may copy, modify or distribute this file at will.
*
*  THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING
*  THE WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
*  PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
*
*  This file is provided with no support and without any obligation on the
*  part of SunSoft, Inc. to assist in its use, correction, modification or
*  enhancement.
*
*  SUNSOFT AND SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT
*  TO THE INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS
*  FILE OR ANY PART THEREOF.
*
*  IN NO EVENT WILL SUNSOFT OR SUN MICROSYSTEMS, INC. BE LIABLE FOR ANY
*  LOST REVENUE OR PROFITS OR OTHER SPECIAL, INDIRECT AND CONSEQUENTIAL
*  DAMAGES, EVEN IF THEY HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
*  DAMAGES.
*
*  SunSoft, Inc.
*  2550 Garcia Avenue
*  Mountain View, California  94043
*/

int my_rwlock_init(rw_lock_t *rwp, void *arg __attribute__((unused)))
{
  pthread_condattr_t	cond_attr;

#ifdef _WIN32
  /*
    Once initialization is used here rather than in my_init(), in order to
    - avoid  my_init() pitfalls- (undefined order in which initialization should
    run)
    - be potentially useful C++ (static constructors) 
    - just to simplify  the API. 
    Also, the overhead is of my_pthread_once is very small.
  */
  static my_pthread_once_t once_control= MY_PTHREAD_ONCE_INIT;
  my_pthread_once(&once_control, check_srwlock_availability);

  if (have_srwlock)
    return srw_init(rwp);
#endif

  pthread_mutex_init( &rwp->lock, MY_MUTEX_INIT_FAST);
  pthread_condattr_init( &cond_attr );
  pthread_cond_init( &rwp->readers, &cond_attr );
  pthread_cond_init( &rwp->writers, &cond_attr );
  pthread_condattr_destroy(&cond_attr);

  rwp->state	= 0;
  rwp->waiters	= 0;

  return(0);
}


int my_rwlock_destroy(rw_lock_t *rwp)
{
#ifdef _WIN32
  if (have_srwlock)
    return 0; /* no destroy function */
#endif
  pthread_mutex_destroy( &rwp->lock );
  pthread_cond_destroy( &rwp->readers );
  pthread_cond_destroy( &rwp->writers );
  return(0);
}


int my_rw_rdlock(rw_lock_t *rwp)
{
#ifdef _WIN32
  if (have_srwlock)
    return srw_rdlock(rwp);
#endif

  pthread_mutex_lock(&rwp->lock);

  /* active or queued writers */
  while ((rwp->state < 0 ) || rwp->waiters)
    pthread_cond_wait( &rwp->readers, &rwp->lock);

  rwp->state++;
  pthread_mutex_unlock(&rwp->lock);
  return(0);
}

int my_rw_tryrdlock(rw_lock_t *rwp)
{
  int res;

#ifdef _WIN32
  if (have_srwlock)
    return srw_tryrdlock(rwp);
#endif

  pthread_mutex_lock(&rwp->lock);
  if ((rwp->state < 0 ) || rwp->waiters)
    res= EBUSY;					/* Can't get lock */
  else
  {
    res=0;
    rwp->state++;
  }
  pthread_mutex_unlock(&rwp->lock);
  return(res);
}


int my_rw_wrlock(rw_lock_t *rwp)
{
#ifdef _WIN32
  if (have_srwlock)
    return srw_wrlock(rwp);
#endif

  pthread_mutex_lock(&rwp->lock);
  rwp->waiters++;				/* another writer queued */

  while (rwp->state)
    pthread_cond_wait(&rwp->writers, &rwp->lock);
  rwp->state	= -1;
  rwp->waiters--;
  pthread_mutex_unlock(&rwp->lock);
  return(0);
}


int my_rw_trywrlock(rw_lock_t *rwp)
{
  int res;

#ifdef _WIN32
  if (have_srwlock)
    return srw_trywrlock(rwp);
#endif

  pthread_mutex_lock(&rwp->lock);
  if (rwp->state)
    res= EBUSY;					/* Can't get lock */    
  else
  {
    res=0;
    rwp->state	= -1;
  }
  pthread_mutex_unlock(&rwp->lock);
  return(res);
}


int my_rw_unlock(rw_lock_t *rwp)
{
#ifdef _WIN32
  if (have_srwlock)
    return srw_unlock(rwp);
#endif

  DBUG_PRINT("rw_unlock",
	     ("state: %d waiters: %d", rwp->state, rwp->waiters));
  pthread_mutex_lock(&rwp->lock);

  if (rwp->state == -1)		/* writer releasing */
  {
    rwp->state= 0;		/* mark as available */

    if ( rwp->waiters )		/* writers queued */
      pthread_cond_signal( &rwp->writers );
    else
      pthread_cond_broadcast( &rwp->readers );
  }
  else
  {
    if ( --rwp->state == 0 )	/* no more readers */
      pthread_cond_signal( &rwp->writers );
  }

  pthread_mutex_unlock( &rwp->lock );
  return(0);
}

#endif
