/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Synchronization - readers / writer thread locks */

#include "mysys_priv.h"
#include <my_pthread.h>
#if defined(THREAD) && !defined(HAVE_PTHREAD_RWLOCK_RDLOCK) && !defined(HAVE_RWLOCK_INIT)

/*
 * Source base from Sun Microsystems SPILT, simplified
 * for MySQL use -- Joshua Chamas
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

int my_rwlock_init( rw_lock_t *rwp, void *arg __attribute__((unused)))
{
  pthread_condattr_t	cond_attr;

  pthread_mutex_init( &rwp->lock, NULL );
  pthread_condattr_init( &cond_attr );
  pthread_cond_init( &rwp->readers, &cond_attr );
  pthread_cond_init( &rwp->writers, &cond_attr );
  pthread_condattr_destroy(&cond_attr);

  rwp->state	= 0;
  rwp->waiters	= 0;

  return( 0 );
}

int my_rwlock_destroy( rw_lock_t *rwp ) {
  pthread_mutex_destroy( &rwp->lock );
  pthread_cond_destroy( &rwp->readers );
  pthread_cond_destroy( &rwp->writers );

  return( 0 );
}

int my_rw_rdlock( rw_lock_t *rwp ) {
  pthread_mutex_lock(&rwp->lock);

  /* active or queued writers		*/
  while ( ( rwp->state < 0 ) || rwp->waiters )
    pthread_cond_wait( &rwp->readers, &rwp->lock);

  rwp->state++;
  pthread_mutex_unlock(&rwp->lock);

  return( 0 );
}

int my_rw_wrlock( rw_lock_t *rwp ) {

  pthread_mutex_lock(&rwp->lock);
  rwp->waiters++; /* another writer queued		*/

  while ( rwp->state )
    pthread_cond_wait( &rwp->writers, &rwp->lock);
  rwp->state	= -1;
  --rwp->waiters;
  pthread_mutex_unlock( &rwp->lock );

  return( 0 );
}

int my_rw_unlock( rw_lock_t *rwp ) {
  DBUG_PRINT("rw_unlock",
	     ("state: %d waiters: %d", rwp->state, rwp->waiters));
  pthread_mutex_lock(&rwp->lock);

  if ( rwp->state == -1 ) {	/* writer releasing	*/
    rwp->state	= 0;		/* mark as available	*/

    if ( rwp->waiters )		/* writers queued	*/
      pthread_cond_signal( &rwp->writers );
    else
      pthread_cond_broadcast( &rwp->readers );
  } else {
    if ( --rwp->state == 0 )	/* no more readers	*/
      pthread_cond_signal( &rwp->writers );
  }

  pthread_mutex_unlock( &rwp->lock );

  return( 0 );
}

#endif
