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
#if defined(THREAD)
#if defined(NEED_MY_RW_LOCK)
#include <errno.h>

/*
  Source base from Sun Microsystems SPILT, simplified for MySQL use
  -- Joshua Chamas
  Some cleanup and additional code by Monty
*/

/*
*  Multithreaded Demo Source
*
*  Copyright (C) 1995 by Sun Microsystems, Inc.
*  All rights reserved.
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

int my_rw_init(my_rw_lock_t *rwp, my_bool *prefer_readers_attr)
{
  pthread_condattr_t	cond_attr;

  pthread_mutex_init( &rwp->lock, MY_MUTEX_INIT_FAST);
  pthread_condattr_init( &cond_attr );
  pthread_cond_init( &rwp->readers, &cond_attr );
  pthread_cond_init( &rwp->writers, &cond_attr );
  pthread_condattr_destroy(&cond_attr);

  rwp->state	= 0;
  rwp->waiters	= 0;
  /* If attribute argument is NULL use default value - prefer writers. */
  rwp->prefer_readers= prefer_readers_attr ? *prefer_readers_attr : FALSE;

  return(0);
}


int my_rw_destroy(my_rw_lock_t *rwp)
{
  pthread_mutex_destroy( &rwp->lock );
  pthread_cond_destroy( &rwp->readers );
  pthread_cond_destroy( &rwp->writers );
  return(0);
}


int my_rw_rdlock(my_rw_lock_t *rwp)
{
  pthread_mutex_lock(&rwp->lock);

  /* active or queued writers */
  while (( rwp->state < 0 ) ||
         (rwp->waiters && ! rwp->prefer_readers))
    pthread_cond_wait( &rwp->readers, &rwp->lock);

  rwp->state++;
  pthread_mutex_unlock(&rwp->lock);
  return(0);
}

int my_rw_tryrdlock(my_rw_lock_t *rwp)
{
  int res;
  pthread_mutex_lock(&rwp->lock);
  if ((rwp->state < 0 ) ||
      (rwp->waiters && ! rwp->prefer_readers))
    res= EBUSY;					/* Can't get lock */
  else
  {
    res=0;
    rwp->state++;
  }
  pthread_mutex_unlock(&rwp->lock);
  return(res);
}


int my_rw_wrlock(my_rw_lock_t *rwp)
{
  pthread_mutex_lock(&rwp->lock);
  rwp->waiters++;				/* another writer queued */

  while (rwp->state)
    pthread_cond_wait(&rwp->writers, &rwp->lock);
  rwp->state	= -1;
  rwp->waiters--;
  pthread_mutex_unlock(&rwp->lock);
  return(0);
}


int my_rw_trywrlock(my_rw_lock_t *rwp)
{
  int res;
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


int my_rw_unlock(my_rw_lock_t *rwp)
{
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
    if ( --rwp->state == 0 &&   /* no more readers */
        rwp->waiters)
      pthread_cond_signal( &rwp->writers );
  }

  pthread_mutex_unlock( &rwp->lock );
  return(0);
}


int rw_pr_init(struct st_my_rw_lock_t *rwlock)
{
  my_bool prefer_readers_attr= TRUE;
  return my_rw_init(rwlock, &prefer_readers_attr);
}

#else

/*
  We are on system which has native read/write locks which support
  preferring of readers.
*/

int rw_pr_init(rw_pr_lock_t *rwlock)
{
  pthread_rwlockattr_t rwlock_attr;

  pthread_rwlockattr_init(&rwlock_attr);
  pthread_rwlockattr_setkind_np(&rwlock_attr, PTHREAD_RWLOCK_PREFER_READER_NP);
  pthread_rwlock_init(rwlock, NULL);
  pthread_rwlockattr_destroy(&rwlock_attr);
  return 0;
}

#endif /* defined(NEED_MY_RW_LOCK) */
#endif /* defined(THREAD) */
