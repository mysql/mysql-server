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

/*
Read and write locks for Posix threads. All tread must acquire
all locks it needs through thr_multi_lock() to avoid dead-locks.
A lock consists of a master lock (THR_LOCK), and lock instances
(THR_LOCK_DATA).
Any thread can have any number of lock instances (read and write:s) on
any lock. All lock instances must be freed.
Locks are prioritized according to:

The current lock types are:

TL_READ	 		# Low priority read
TL_READ_HIGH_PRIORITY	# High priority read
TL_READ_NO_INSERT	# Read without concurrent inserts
TL_WRITE_ALLOW_WRITE	# Write lock that allows other writers
TL_WRITE_ALLOW_READ	# Write lock, but allow reading
TL_WRITE_CONCURRENT_INSERT
			# Insert that can be mixed when selects
TL_WRITE_DELAYED	# Used by delayed insert
			# Allows lower locks to take over
TL_WRITE_LOW_PRIORITY	# Low priority write
TL_WRITE		# High priority write
TL_WRITE_ONLY		# High priority write
			# Abort all new lock request with an error

Locks are prioritized according to:

WRITE_ALLOW_WRITE, WRITE_ALLOW_READ, WRITE_CONCURRENT_INSERT, WRITE_DELAYED,
WRITE_LOW_PRIORITY, READ, WRITE, READ_HIGH_PRIORITY and WRITE_ONLY

Locks in the same privilege level are scheduled in first-in-first-out order.

To allow concurrent read/writes locks, with 'WRITE_CONCURRENT_INSERT' one
should put a pointer to the following functions in the lock structure:
(If the pointer is zero (default), the function is not called)

check_status:
	 Before giving a lock of type TL_WRITE_CONCURRENT_INSERT,
         we check if this function exists and returns 0.
	 If not, then the lock is upgraded to TL_WRITE_LOCK
	 In MyISAM this is a simple check if the insert can be done
	 at the end of the datafile.
update_status:
	Before a write lock is released, this function is called.
	In MyISAM this functions updates the count and length of the datafile
get_status:
	When one gets a lock this functions is called.
	In MyISAM this stores the number of rows and size of the datafile
	for concurrent reads.

The lock algorithm allows one to have one TL_WRITE_ALLOW_READ,
TL_WRITE_CONCURRENT_INSERT or one TL_WRITE_DELAYED lock at the same time as
multiple read locks.

*/

#if !defined(MAIN) && !defined(DBUG_OFF) && !defined(EXTRA_DEBUG)
#define DBUG_OFF
#endif

#include "mysys_priv.h"

#ifdef THREAD
#include "thr_lock.h"
#include <m_string.h>
#include <errno.h>

my_bool thr_lock_inited=0;
ulong locks_immediate = 0L, locks_waited = 0L;

/* The following constants are only for debug output */
#define MAX_THREADS 100
#define MAX_LOCKS   100


static LIST *thread_list;			/* List of threads in use */
ulong max_write_lock_count= ~(ulong) 0L;

static inline pthread_cond_t *get_cond(void)
{
  return &my_thread_var->suspend;
}

/*
** For the future (now the thread specific cond is alloced by my_pthread.c)
*/

my_bool init_thr_lock()
{
  thr_lock_inited=1;
  return 0;
}

#ifdef EXTRA_DEBUG
static int found_errors=0;

static int check_lock(struct st_lock_list *list, const char* lock_type,
		      const char *where, my_bool same_thread)
{
  THR_LOCK_DATA *data,**prev;
  uint count=0;
  pthread_t first_thread;
  LINT_INIT(first_thread);

  prev= &list->data;
  if (list->data)
  {
    enum thr_lock_type last_lock_type=list->data->type;

    if (same_thread && list->data)
      first_thread=list->data->thread;
    for (data=list->data; data && count++ < MAX_LOCKS ; data=data->next)
    {
      if (data->type != last_lock_type)
	last_lock_type=TL_IGNORE;
      if (data->prev != prev)
      {
	fprintf(stderr,
		"Warning: prev link %d didn't point at previous lock at %s: %s\n",
		count, lock_type, where);
	return 1;
      }
      if (same_thread && ! pthread_equal(data->thread,first_thread) &&
	  last_lock_type != TL_WRITE_ALLOW_WRITE)
      {
	fprintf(stderr,
		"Warning: Found locks from different threads in %s: %s\n",
		lock_type,where);
	return 1;
      }
      prev= &data->next;
    }
    if (data)
    {
      fprintf(stderr,"Warning: found too many locks at %s: %s\n",
	      lock_type,where);
      return 1;
    }
  }
  if (prev != list->last)
  {
    fprintf(stderr,"Warning: last didn't point at last lock at %s: %s\n",
	    lock_type, where);
    return 1;
  }
  return 0;
}

static void check_locks(THR_LOCK *lock, const char *where,
			my_bool allow_no_locks)
{
  if (!found_errors)
  {
    if (check_lock(&lock->write,"write",where,1) |
	check_lock(&lock->write_wait,"write_wait",where,0) |
	check_lock(&lock->read,"read",where,0) |
	check_lock(&lock->read_wait,"read_wait",where,0))
      found_errors=1;

    if (!found_errors)
    {
      uint count=0;
      THR_LOCK_DATA *data;
      for (data=lock->read.data ; data ; data=data->next)
      {
	if ((int) data->type == (int) TL_READ_NO_INSERT)
	  count++;
      }
      if (count != lock->read_no_write_count)
      {
	found_errors=1;
	fprintf(stderr,
		"Warning at '%s': Locks read_no_write_count was %u when it should have been %u\n", where, lock->read_no_write_count,count);
      }      

      if (!lock->write.data)
      {
	if (!allow_no_locks && !lock->read.data &&
	    (lock->write_wait.data || lock->read_wait.data))
	{
	  found_errors=1;
	  fprintf(stderr,
		  "Warning at '%s': No locks in use but locks are in wait queue\n",
		  where);
	}
	if (!lock->write_wait.data)
	{
	  if (!allow_no_locks && lock->read_wait.data)
	  {
	    found_errors=1;
	    fprintf(stderr,
		    "Warning at '%s': No write locks and waiting read locks\n",
		    where);
	  }
	}
	else
	{
	  if (!allow_no_locks &&
	      (((lock->write_wait.data->type == TL_WRITE_CONCURRENT_INSERT ||
		 lock->write_wait.data->type == TL_WRITE_ALLOW_WRITE) &&
		!lock->read_no_write_count) ||
	       lock->write_wait.data->type == TL_WRITE_ALLOW_READ ||
	       (lock->write_wait.data->type == TL_WRITE_DELAYED &&
		!lock->read.data)))
	  {
	    found_errors=1;
	    fprintf(stderr,
		    "Warning at '%s': Write lock %d waiting while no exclusive read locks\n",where,(int) lock->write_wait.data->type);
	  }
	}	      
      }
      else
      {						/* Have write lock */
	if (lock->write_wait.data)
	{
	  if (!allow_no_locks && 
	      lock->write.data->type == TL_WRITE_ALLOW_WRITE &&
	      lock->write_wait.data->type == TL_WRITE_ALLOW_WRITE)
	  {
	    found_errors=1;
	    fprintf(stderr,
		    "Warning at '%s': Found WRITE_ALLOW_WRITE lock waiting for WRITE_ALLOW_WRITE lock\n",
		    where);
	  }
	}
	if (lock->read.data)
	{
	  if ((!pthread_equal(lock->write.data->thread,lock->read.data->thread) &&
	       lock->write.data->type > TL_WRITE_DELAYED) ||
	      ((lock->write.data->type == TL_WRITE_CONCURRENT_INSERT ||
		lock->write.data->type == TL_WRITE_ALLOW_WRITE) &&
	       lock->read_no_write_count))
	  {
	    found_errors=1;
	    fprintf(stderr,
		    "Warning at '%s': Found lock that is write and read locked\n",
		    where);
	  }
	}
	if (lock->read_wait.data)
	{
	  if (!allow_no_locks && lock->write.data->type <= TL_WRITE_DELAYED &&
	      lock->read_wait.data->type <= TL_READ_HIGH_PRIORITY)
	  {
	    found_errors=1;
	    fprintf(stderr,
		    "Warning at '%s': Found read lock of type %d waiting for write lock of type %d\n",
		    where,
		    (int) lock->read_wait.data->type,
		    (int) lock->write.data->type);
	  }
	}
      }
    }
    if (found_errors)
    {
      DBUG_PRINT("error",("Found wrong lock"));
    }
  }
}

#else /* EXTRA_DEBUG */
#define check_locks(A,B,C)
#endif


	/* Initialize a lock */

void thr_lock_init(THR_LOCK *lock)
{
  DBUG_ENTER("thr_lock_init");
  bzero((char*) lock,sizeof(*lock));
  VOID(pthread_mutex_init(&lock->mutex,NULL));
  lock->read.last= &lock->read.data;
  lock->read_wait.last= &lock->read_wait.data;
  lock->write_wait.last= &lock->write_wait.data;
  lock->write.last= &lock->write.data;

  pthread_mutex_lock(&THR_LOCK_lock);		/* Add to locks in use */
  lock->list.data=(void*) lock;
  thread_list=list_add(thread_list,&lock->list);
  pthread_mutex_unlock(&THR_LOCK_lock);
  DBUG_VOID_RETURN;
}


void thr_lock_delete(THR_LOCK *lock)
{
  DBUG_ENTER("thr_lock_delete");
  VOID(pthread_mutex_destroy(&lock->mutex));
  pthread_mutex_lock(&THR_LOCK_lock);
  thread_list=list_delete(thread_list,&lock->list);
  pthread_mutex_unlock(&THR_LOCK_lock);
  DBUG_VOID_RETURN;
}

	/* Initialize a lock instance */

void thr_lock_data_init(THR_LOCK *lock,THR_LOCK_DATA *data, void *param)
{
  data->lock=lock;
  data->type=TL_UNLOCK;
  data->thread=pthread_self();
  data->thread_id=my_thread_id();		/* for debugging */
  data->status_param=param;
}


static inline my_bool have_old_read_lock(THR_LOCK_DATA *data,pthread_t thread)
{
  for ( ; data ; data=data->next)
  {
    if ((pthread_equal(data->thread,thread)))
      return 1;					/* Already locked by thread */
  }
  return 0;
}

static inline my_bool have_specific_lock(THR_LOCK_DATA *data,
					 enum thr_lock_type type)
{
  for ( ; data ; data=data->next)
  {
    if (data->type == type)
      return 1;
  }
  return 0;
}


static my_bool wait_for_lock(struct st_lock_list *wait, THR_LOCK_DATA *data,
			     my_bool in_wait_list)
{
  pthread_cond_t *cond=get_cond();
  struct st_my_thread_var *thread_var=my_thread_var;
  int result;

  if (!in_wait_list)
  {
    (*wait->last)=data;				/* Wait for lock */
    data->prev= wait->last;
    wait->last= &data->next;
  }

  /* Set up control struct to allow others to abort locks */
  pthread_mutex_lock(&thread_var->mutex);
  thread_var->current_mutex= &data->lock->mutex;
  thread_var->current_cond=  cond;
  pthread_mutex_unlock(&thread_var->mutex);

  data->cond=cond;
  do
  {
    pthread_cond_wait(cond,&data->lock->mutex);
  } while (data->cond == cond && !thread_var->abort);

  if (data->cond || data->type == TL_UNLOCK)
  {
    if (data->cond)				/* aborted */
    {
      if (((*data->prev)=data->next))		/* remove from wait-list */
	data->next->prev= data->prev;
      else
	wait->last=data->prev;
    }
    data->type=TL_UNLOCK;			/* No lock */
    result=1;					/* Didn't get lock */
    check_locks(data->lock,"failed wait_for_lock",0);
  }
  else
  {
    result=0;
    statistic_increment(locks_waited, &THR_LOCK_lock);
    if (data->lock->get_status)
      (*data->lock->get_status)(data->status_param);
    check_locks(data->lock,"got wait_for_lock",0);
  }
  pthread_mutex_unlock(&data->lock->mutex);

  /* The following must be done after unlock of lock->mutex */
  pthread_mutex_lock(&thread_var->mutex);
  thread_var->current_mutex= 0;
  thread_var->current_cond=  0;
  pthread_mutex_unlock(&thread_var->mutex);
  return result;
}


int thr_lock(THR_LOCK_DATA *data,enum thr_lock_type lock_type)
{
  THR_LOCK *lock=data->lock;
  int result=0;
  DBUG_ENTER("thr_lock");

  data->next=0;
  data->type=lock_type;
  data->thread=pthread_self();			/* Must be reset ! */
  data->thread_id=my_thread_id();		/* Must be reset ! */
  VOID(pthread_mutex_lock(&lock->mutex));
  DBUG_PRINT("lock",("data: %lx  thread: %ld  lock: %lx  type: %d",
		      data,data->thread_id,lock,(int) lock_type));
  check_locks(lock,(uint) lock_type <= (uint) TL_READ_NO_INSERT ?
	      "enter read_lock" : "enter write_lock",0);
  if ((int) lock_type <= (int) TL_READ_NO_INSERT)
  {
    /* Request for READ lock */
    if (lock->write.data)
    {
      /* We can allow a read lock even if there is already a write lock
	 on the table in one the following cases:
	 - This thread alread have a write lock on the table
	 - The write lock is TL_WRITE_ALLOW_READ or TL_WRITE_DELAYED
           and the read lock is TL_READ_HIGH_PRIORITY or TL_READ
         - The write lock is TL_WRITE_CONCURRENT_INSERT or TL_WRITE_ALLOW_WRITE
	   and the read lock is not TL_READ_NO_INSERT
      */

      DBUG_PRINT("lock",("write locked by thread: %ld",
			 lock->write.data->thread_id));
      if (pthread_equal(data->thread,lock->write.data->thread) ||
	  (lock->write.data->type <= TL_WRITE_DELAYED &&
	   (((int) lock_type <= (int) TL_READ_HIGH_PRIORITY) ||
	    (lock->write.data->type != TL_WRITE_CONCURRENT_INSERT &&
	     lock->write.data->type != TL_WRITE_ALLOW_READ))))
      {						/* Already got a write lock */
	(*lock->read.last)=data;		/* Add to running FIFO */
	data->prev=lock->read.last;
	lock->read.last= &data->next;
	if ((int) lock_type == (int) TL_READ_NO_INSERT)
	  lock->read_no_write_count++;
	check_locks(lock,"read lock with old write lock",0);
	if (lock->get_status)
	  (*lock->get_status)(data->status_param);
	++locks_immediate;
	goto end;
      }
      if (lock->write.data->type == TL_WRITE_ONLY)
      {
	/* We are not allowed to get a READ lock in this case */
	data->type=TL_UNLOCK;
	result=1;				/* Can't wait for this one */
	goto end;
      }
    }
    else if (!lock->write_wait.data ||
	     lock->write_wait.data->type <= TL_WRITE_LOW_PRIORITY ||
	     lock_type == TL_READ_HIGH_PRIORITY ||
	     have_old_read_lock(lock->read.data,data->thread))
    {						/* No important write-locks */
      (*lock->read.last)=data;			/* Add to running FIFO */
      data->prev=lock->read.last;
      lock->read.last= &data->next;
      if (lock->get_status)
	(*lock->get_status)(data->status_param);
      if ((int) lock_type == (int) TL_READ_NO_INSERT)
	lock->read_no_write_count++;
      check_locks(lock,"read lock with no write locks",0);
      ++locks_immediate;
      goto end;
    }
    /* Can't get lock yet;  Wait for it */
    DBUG_RETURN(wait_for_lock(&lock->read_wait,data,0));
  }
  else						/* Request for WRITE lock */
  {
    if (lock_type == TL_WRITE_DELAYED)
    {
      if (lock->write.data && lock->write.data->type == TL_WRITE_ONLY)
      {
	data->type=TL_UNLOCK;
	result=1;				/* Can't wait for this one */
	goto end;
      }
      /*
	if there is a TL_WRITE_ALLOW_READ lock, we have to wait for a lock
	(TL_WRITE_ALLOW_READ is used for ALTER TABLE in MySQL)
      */
      if ((!lock->write.data ||
	   lock->write.data->type != TL_WRITE_ALLOW_READ) &&
	  !have_specific_lock(lock->write_wait.data,TL_WRITE_ALLOW_READ) &&
	  (lock->write.data || lock->read.data))
      {
	/* Add delayed write lock to write_wait queue, and return at once */
	(*lock->write_wait.last)=data;
	data->prev=lock->write_wait.last;
	lock->write_wait.last= &data->next;
	data->cond=get_cond();
	if (lock->get_status)
	  (*lock->get_status)(data->status_param);
	++locks_immediate;
	goto end;
      }
    }
    else if (lock_type == TL_WRITE_CONCURRENT_INSERT && ! lock->check_status)
      data->type=lock_type= TL_WRITE;		/* not supported */

    if (lock->write.data)			/* If there is a write lock */
    {
      if (lock->write.data->type == TL_WRITE_ONLY)
      {
	/* We are not allowed to get a lock in this case */
	data->type=TL_UNLOCK;
	result=1;				/* Can't wait for this one */
	goto end;
      }

      /*
	The following test will not work if the old lock was a
	TL_WRITE_ALLOW_WRITE, TL_WRITE_ALLOW_READ or TL_WRITE_DELAYED in
	the same thread, but this will never happen within MySQL.
      */
      if (pthread_equal(data->thread,lock->write.data->thread) ||
	  (lock_type == TL_WRITE_ALLOW_WRITE &&
	   !lock->write_wait.data &&
	   lock->write.data->type == TL_WRITE_ALLOW_WRITE))
      {
	/* We have already got a write lock or all locks are
	   TL_WRITE_ALLOW_WRITE */
	(*lock->write.last)=data;	/* Add to running fifo */
	data->prev=lock->write.last;
	lock->write.last= &data->next;
	check_locks(lock,"second write lock",0);
	if (data->lock->get_status)
	  (*data->lock->get_status)(data->status_param);
	++locks_immediate;
	goto end;
      }
      DBUG_PRINT("lock",("write locked by thread: %ld",
			 lock->write.data->thread_id));
    }
    else
    {
      if (!lock->write_wait.data)
      {						/* no scheduled write locks */
	if (lock_type == TL_WRITE_CONCURRENT_INSERT &&
	    (*lock->check_status)(data->status_param))
	  data->type=lock_type=TL_WRITE;	/* Upgrade lock */

	if (!lock->read.data ||
	    (lock_type <= TL_WRITE_DELAYED &&
	     ((lock_type != TL_WRITE_CONCURRENT_INSERT &&
	       lock_type != TL_WRITE_ALLOW_WRITE) ||
	      !lock->read_no_write_count)))
	{
	  (*lock->write.last)=data;		/* Add as current write lock */
	  data->prev=lock->write.last;
	  lock->write.last= &data->next;
	  if (data->lock->get_status)
	    (*data->lock->get_status)(data->status_param);
	  check_locks(lock,"only write lock",0);
	  ++locks_immediate;
	  goto end;
	}
      }
      DBUG_PRINT("lock",("write locked by thread: %ld, type: %ld",
			 lock->read.data->thread_id,data->type));
    }
    DBUG_RETURN(wait_for_lock(&lock->write_wait,data,0));
  }
end:
  pthread_mutex_unlock(&lock->mutex);
  DBUG_RETURN(result);
}


static inline void free_all_read_locks(THR_LOCK *lock,
				       bool using_concurrent_insert)
{
  THR_LOCK_DATA *data=lock->read_wait.data;

  check_locks(lock,"before freeing read locks",1);

  /* move all locks from read_wait list to read list */
  (*lock->read.last)=data;
  data->prev=lock->read.last;
  lock->read.last=lock->read_wait.last;

  /* Clear read_wait list */
  lock->read_wait.last= &lock->read_wait.data;

  do
  {
    pthread_cond_t *cond=data->cond;
    if ((int) data->type == (int) TL_READ_NO_INSERT)
    {
      if (using_concurrent_insert)
      {
	/*
	  We can't free this lock; 
	  Link lock away from read chain back into read_wait chain
	*/
	if (((*data->prev)=data->next))
	  data->next->prev=data->prev;
	else
	  lock->read.last=data->prev;
	*lock->read_wait.last= data;
	data->prev= lock->read_wait.last;
	lock->read_wait.last= &data->next;
	continue;
      }
      lock->read_no_write_count++;
    }      
    DBUG_PRINT("lock",("giving read lock to thread: %ld",
		       data->thread_id));
    data->cond=0;				/* Mark thread free */
    VOID(pthread_cond_signal(cond));
  } while ((data=data->next));
  *lock->read_wait.last=0;
  if (!lock->read_wait.data)
    lock->write_lock_count=0;
  check_locks(lock,"after giving read locks",0);
}

	/* Unlock lock and free next thread on same lock */

void thr_unlock(THR_LOCK_DATA *data)
{
  THR_LOCK *lock=data->lock;
  enum thr_lock_type lock_type=data->type;
  DBUG_ENTER("thr_unlock");
  DBUG_PRINT("lock",("data: %lx  thread: %ld  lock: %lx",
		     data,data->thread_id,lock));
  pthread_mutex_lock(&lock->mutex);
  check_locks(lock,"start of release lock",0);

  if (((*data->prev)=data->next))		/* remove from lock-list */
    data->next->prev= data->prev;
  else if (lock_type <= TL_READ_NO_INSERT)
    lock->read.last=data->prev;
  else if (lock_type == TL_WRITE_DELAYED && data->cond)
  {
    /* This only happens in extreme circumstances when a 
       write delayed lock that is waiting for a lock */
    lock->write_wait.last=data->prev;		/* Put it on wait queue */
  }
  else
    lock->write.last=data->prev;
  if (lock_type >= TL_WRITE_CONCURRENT_INSERT && lock->update_status)
    (*lock->update_status)(data->status_param);
  if (lock_type == TL_READ_NO_INSERT)
    lock->read_no_write_count--;
  data->type=TL_UNLOCK;				/* Mark unlocked */
  check_locks(lock,"after releasing lock",1);

  if (!lock->write.data)			/* If no active write locks */
  {
    data=lock->write_wait.data;
    if (!lock->read.data)			/* If no more locks in use */
    {
      /* Release write-locks with TL_WRITE or TL_WRITE_ONLY priority first */
      if (data &&
	  (data->type != TL_WRITE_LOW_PRIORITY || !lock->read_wait.data ||
	   lock->read_wait.data->type == TL_READ))
      {
	if (lock->write_lock_count++ > max_write_lock_count)
	{
	  /* Too many write locks in a row;  Release all waiting read locks */
	  lock->write_lock_count=0;
	  if (lock->read_wait.data)
	  {
	    DBUG_PRINT("info",("Freeing all read_locks because of max_write_lock_count"));
	    free_all_read_locks(lock,0);
	    goto end;
	  }
	}
	for (;;)
	{
	  if (((*data->prev)=data->next))	/* remove from wait-list */
	    data->next->prev= data->prev;
	  else
	    lock->write_wait.last=data->prev;
	  (*lock->write.last)=data;		/* Put in execute list */
	  data->prev=lock->write.last;
	  data->next=0;
	  lock->write.last= &data->next;
	  if (data->type == TL_WRITE_CONCURRENT_INSERT &&
	      (*lock->check_status)(data->status_param))
	    data->type=TL_WRITE;			/* Upgrade lock */
	  DBUG_PRINT("lock",("giving write lock of type %d to thread: %ld",
			     data->type,data->thread_id));
	  {
	    pthread_cond_t *cond=data->cond;
	    data->cond=0;				/* Mark thread free */
	    VOID(pthread_cond_signal(cond));	/* Start waiting thread */
	  }
	  if (data->type != TL_WRITE_ALLOW_WRITE ||
	      !lock->write_wait.data ||
	      lock->write_wait.data->type != TL_WRITE_ALLOW_WRITE)
	    break;
	  data=lock->write_wait.data;		/* Free this too */
	}
	if (data->type >= TL_WRITE_LOW_PRIORITY)
	{
	  check_locks(lock,"giving write lock",0);
	  pthread_mutex_unlock(&lock->mutex);
	  DBUG_VOID_RETURN;
	}
	/* Release possible read locks together with the write lock */
      }
      if (lock->read_wait.data)
	free_all_read_locks(lock,
			    data &&
			    (data->type == TL_WRITE_CONCURRENT_INSERT ||
			     data->type == TL_WRITE_ALLOW_WRITE));
      else
      {
	DBUG_PRINT("lock",("No locks to free"));
      }
    }
    else if (data &&
	     (lock_type=data->type) <= TL_WRITE_DELAYED &&
	     ((lock_type != TL_WRITE_CONCURRENT_INSERT &&
	       lock_type != TL_WRITE_ALLOW_WRITE) ||
	      !lock->read_no_write_count))
    {
      /*
	For DELAYED, ALLOW_READ, WRITE_ALLOW_WRITE or CONCURRENT_INSERT locks
	start WRITE locks together with the READ locks
      */
      if (lock_type == TL_WRITE_CONCURRENT_INSERT &&
	  (*lock->check_status)(data->status_param))
      {
	data->type=TL_WRITE;			/* Upgrade lock */
	if (lock->read_wait.data)
	  free_all_read_locks(lock,0);
	goto end;
      }
      do {
	pthread_cond_t *cond=data->cond;
	if (((*data->prev)=data->next))		/* remove from wait-list */
	  data->next->prev= data->prev;
	else
	  lock->write_wait.last=data->prev;
	(*lock->write.last)=data;		/* Put in execute list */
	data->prev=lock->write.last;
	lock->write.last= &data->next;
	data->next=0;				/* Only one write lock */
	data->cond=0;				/* Mark thread free */
	VOID(pthread_cond_signal(cond));	/* Start waiting thread */
      } while (lock_type == TL_WRITE_ALLOW_WRITE &&
	       (data=lock->write_wait.data) &&
	       data->type == TL_WRITE_ALLOW_WRITE);
      if (lock->read_wait.data)
	free_all_read_locks(lock,
			    (lock_type == TL_WRITE_CONCURRENT_INSERT ||
			     lock_type == TL_WRITE_ALLOW_WRITE));
    }
    else if (!data && lock->read_wait.data)
      free_all_read_locks(lock,0);
  }
end:
  check_locks(lock,"thr_unlock",0);
  pthread_mutex_unlock(&lock->mutex);
  DBUG_VOID_RETURN;
}


/*
** Get all locks in a specific order to avoid dead-locks
** Sort acording to lock position and put write_locks before read_locks if
** lock on same lock.
*/


#define LOCK_CMP(A,B) ((byte*) (A->lock) - (uint) ((A)->type) < (byte*) (B->lock)- (uint) ((B)->type))

static void sort_locks(THR_LOCK_DATA **data,uint count)
{
  THR_LOCK_DATA **pos,**end,**prev,*tmp;

  /* Sort locks with insertion sort (fast because almost always few locks) */

  for (pos=data+1,end=data+count; pos < end ; pos++)
  {
    tmp= *pos;
    if (LOCK_CMP(tmp,pos[-1]))
    {
      prev=pos;
      do {
	prev[0]=prev[-1];
      } while (--prev != data && LOCK_CMP(tmp,prev[-1]));
      prev[0]=tmp;
    }
  }
}


int thr_multi_lock(THR_LOCK_DATA **data,uint count)
{
  THR_LOCK_DATA **pos,**end;
  DBUG_ENTER("thr_multi_lock");
  DBUG_PRINT("lock",("data: %lx  count: %d",data,count));
  if (count > 1)
    sort_locks(data,count);
  /* lock everything */
  for (pos=data,end=data+count; pos < end ; pos++)
  {
    if (thr_lock(*pos,(*pos)->type))
    {						/* Aborted */
      thr_multi_unlock(data,(uint) (pos-data));
      DBUG_RETURN(1);
    }
#ifdef MAIN
    printf("Thread: %s  Got lock: %lx  type: %d\n",my_thread_name(),
	   (long) pos[0]->lock, pos[0]->type); fflush(stdout);
#endif
  }
  /*
    Ensure that all get_locks() have the same status
    If we lock the same table multiple times, we must use the same
    status_param!
  */
#if !defined(DONT_USE_RW_LOCKS)
  if (count > 1)
  {
    THR_LOCK_DATA *last_lock= end[-1];
    pos=end-1;
    do
    {
      pos--;
      if (last_lock->lock == (*pos)->lock &&
	  last_lock->lock->copy_status)
      {
	if (last_lock->type <= TL_READ_NO_INSERT)
	{
	  THR_LOCK_DATA **read_lock;
	  /*
	    If we are locking the same table with read locks we must ensure
	    that all tables share the status of the last write lock or
	    the same read lock.
	  */
	  for (;
	       (*pos)->type <= TL_READ_NO_INSERT &&
		 pos != data &&
		 pos[-1]->lock == (*pos)->lock ;
	       pos--) ;

	  read_lock = pos+1;
	  do
	  {
	    (last_lock->lock->copy_status)((*read_lock)->status_param,
					   (*pos)->status_param);
	  } while (*(read_lock++) != last_lock);
	  last_lock= (*pos);			/* Point at last write lock */
	}
	else
	  (*last_lock->lock->copy_status)((*pos)->status_param,
					  last_lock->status_param);
      }
      else
	last_lock=(*pos);
    } while (pos != data);
  }
#endif
  DBUG_RETURN(0);
}

  /* free all locks */

void thr_multi_unlock(THR_LOCK_DATA **data,uint count)
{
  THR_LOCK_DATA **pos,**end;
  DBUG_ENTER("thr_multi_unlock");
  DBUG_PRINT("lock",("data: %lx  count: %d",data,count));

  for (pos=data,end=data+count; pos < end ; pos++)
  {
#ifdef MAIN
    printf("Thread: %s  Rel lock: %lx  type: %d\n",
	   my_thread_name(), (long) pos[0]->lock, pos[0]->type);
    fflush(stdout);
#endif
    if ((*pos)->type != TL_UNLOCK)
      thr_unlock(*pos);
    else
    {
      DBUG_PRINT("lock",("Free lock: data: %lx  thread: %ld  lock: %lx",
			 *pos,(*pos)->thread_id,(*pos)->lock));
    }
  }
  DBUG_VOID_RETURN;
}

/* Abort all threads waiting for a lock. The lock will be upgraded to a
  TL_WRITE_ONLY to abort any new accesses to the lock
*/

void thr_abort_locks(THR_LOCK *lock)
{
  THR_LOCK_DATA *data;
  DBUG_ENTER("thr_abort_locks");
  pthread_mutex_lock(&lock->mutex);

  for (data=lock->read_wait.data; data ; data=data->next)
  {
    data->type=TL_UNLOCK;			/* Mark killed */
    pthread_cond_signal(data->cond);
    data->cond=0;				/* Removed from list */
  }
  for (data=lock->write_wait.data; data ; data=data->next)
  {
    data->type=TL_UNLOCK;
    pthread_cond_signal(data->cond);
    data->cond=0;
  }
  lock->read_wait.last= &lock->read_wait.data;
  lock->write_wait.last= &lock->write_wait.data;
  lock->read_wait.data=lock->write_wait.data=0;
  if (lock->write.data)
    lock->write.data->type=TL_WRITE_ONLY;
  pthread_mutex_unlock(&lock->mutex);
  DBUG_VOID_RETURN;
}


/* Upgrade a WRITE_DELAY lock to a WRITE_LOCK */

my_bool thr_upgrade_write_delay_lock(THR_LOCK_DATA *data)
{
  THR_LOCK *lock=data->lock;
  DBUG_ENTER("thr_upgrade_write_delay_lock");

  pthread_mutex_lock(&lock->mutex);
  if (data->type == TL_UNLOCK || data->type == TL_WRITE) /* Aborted */
  {
    pthread_mutex_unlock(&lock->mutex);
    DBUG_RETURN(data->type == TL_UNLOCK);
  }
  check_locks(lock,"before upgrading lock",0);
  /* TODO:  Upgrade to TL_WRITE_CONCURRENT_INSERT in some cases */
  data->type=TL_WRITE;				/* Upgrade lock */

  /* Check if someone has given us the lock */
  if (!data->cond)
  {
    if (!lock->read.data)			/* No read locks */
    {						/* We have the lock */
      if (data->lock->get_status)
	(*data->lock->get_status)(data->status_param);
      pthread_mutex_unlock(&lock->mutex);
      DBUG_RETURN(0);
    }

    if (((*data->prev)=data->next))		/* remove from lock-list */
      data->next->prev= data->prev;
    else
      lock->write.last=data->prev;

    if ((data->next=lock->write_wait.data))	/* Put first in lock_list */
      data->next->prev= &data->next;
    else
      lock->write_wait.last= &data->next;
    data->prev= &lock->write_wait.data;
    lock->write_wait.data=data;
    check_locks(lock,"upgrading lock",0);
  }
  DBUG_RETURN(wait_for_lock(&lock->write_wait,data,1));
}


/* downgrade a WRITE lock to a WRITE_DELAY lock if there is pending locks */

my_bool thr_reschedule_write_lock(THR_LOCK_DATA *data)
{
  THR_LOCK *lock=data->lock;
  DBUG_ENTER("thr_reschedule_write_lock");

  pthread_mutex_lock(&lock->mutex);
  if (!lock->read_wait.data)			/* No waiting read locks */
  {
    pthread_mutex_unlock(&lock->mutex);
    DBUG_RETURN(0);
  }

  data->type=TL_WRITE_DELAYED;
  if (lock->update_status)
    (*lock->update_status)(data->status_param);
  if (((*data->prev)=data->next))		/* remove from lock-list */
    data->next->prev= data->prev;
  else
    lock->write.last=data->prev;

  if ((data->next=lock->write_wait.data))	/* Put first in lock_list */
    data->next->prev= &data->next;
  else
    lock->write_wait.last= &data->next;
  data->prev= &lock->write_wait.data;
  data->cond=get_cond();			/* This was zero */
  lock->write_wait.data=data;
  free_all_read_locks(lock,0);

  pthread_mutex_unlock(&lock->mutex);
  DBUG_RETURN(thr_upgrade_write_delay_lock(data));
}


#include <my_sys.h>

static void thr_print_lock(const char* name,struct st_lock_list *list)
{
  THR_LOCK_DATA *data,**prev;
  uint count=0;

  if (list->data)
  {
    printf("%-10s: ",name);
    prev= &list->data;
    for (data=list->data; data && count++ < MAX_LOCKS ; data=data->next)
    {
      printf("%lx (%lu:%d); ",(ulong) data,data->thread_id,(int) data->type);
      if (data->prev != prev)
	printf("\nWarning: prev didn't point at previous lock\n");
      prev= &data->next;
    }
    puts("");
    if (prev != list->last)
      printf("Warning: last didn't point at last lock\n");
  }
}

void thr_print_locks(void)
{
  LIST *list;
  uint count=0;

  pthread_mutex_lock(&THR_LOCK_lock);
  puts("Current locks:");
  for (list=thread_list ; list && count++ < MAX_THREADS ; list=rest(list))
  {
    THR_LOCK *lock=(THR_LOCK*) list->data;
    VOID(pthread_mutex_lock(&lock->mutex));
    printf("lock: %lx:",(ulong) lock);
    if ((lock->write_wait.data || lock->read_wait.data) &&
	(! lock->read.data && ! lock->write.data))
      printf(" WARNING: ");
    if (lock->write.data)
      printf(" write");
    if (lock->write_wait.data)
      printf(" write_wait");
    if (lock->read.data)
      printf(" read");
    if (lock->read_wait.data)
      printf(" read_wait");
    puts("");
    thr_print_lock("write",&lock->write);
    thr_print_lock("write_wait",&lock->write_wait);
    thr_print_lock("read",&lock->read);
    thr_print_lock("read_wait",&lock->read_wait);
    VOID(pthread_mutex_unlock(&lock->mutex));
    puts("");
  }
  fflush(stdout);
  pthread_mutex_unlock(&THR_LOCK_lock);
}

#endif /* THREAD */

/*****************************************************************************
** Test of thread locks
****************************************************************************/

#ifdef MAIN

#ifdef THREAD

struct st_test {
  uint lock_nr;
  enum thr_lock_type lock_type;
};

THR_LOCK locks[5];			/* 4 locks */

struct st_test test_0[] = {{0,TL_READ}};	/* One lock */
struct st_test test_1[] = {{0,TL_READ},{0,TL_WRITE}}; /* Read and write lock of lock 0 */
struct st_test test_2[] = {{1,TL_WRITE},{0,TL_READ},{2,TL_READ}};
struct st_test test_3[] = {{2,TL_WRITE},{1,TL_READ},{0,TL_READ}}; /* Deadlock with test_2 ? */
struct st_test test_4[] = {{0,TL_WRITE},{0,TL_READ},{0,TL_WRITE},{0,TL_READ}};
struct st_test test_5[] = {{0,TL_READ},{1,TL_READ},{2,TL_READ},{3,TL_READ}}; /* Many reads */
struct st_test test_6[] = {{0,TL_WRITE},{1,TL_WRITE},{2,TL_WRITE},{3,TL_WRITE}}; /* Many writes */
struct st_test test_7[] = {{3,TL_READ}};
struct st_test test_8[] = {{1,TL_READ_NO_INSERT},{2,TL_READ_NO_INSERT},{3,TL_READ_NO_INSERT}};	/* Should be quick */
struct st_test test_9[] = {{4,TL_READ_HIGH_PRIORITY}};
struct st_test test_10[] ={{4,TL_WRITE}};
struct st_test test_11[] = {{0,TL_WRITE_LOW_PRIORITY},{1,TL_WRITE_LOW_PRIORITY},{2,TL_WRITE_LOW_PRIORITY},{3,TL_WRITE_LOW_PRIORITY}}; /* Many writes */
struct st_test test_12[] = {{0,TL_WRITE_ALLOW_READ},{1,TL_WRITE_ALLOW_READ},{2,TL_WRITE_ALLOW_READ},{3,TL_WRITE_ALLOW_READ}}; /* Many writes */
struct st_test test_13[] = {{0,TL_WRITE_CONCURRENT_INSERT},{1,TL_WRITE_CONCURRENT_INSERT},{2,TL_WRITE_CONCURRENT_INSERT},{3,TL_WRITE_CONCURRENT_INSERT}};
struct st_test test_14[] = {{0,TL_WRITE_CONCURRENT_INSERT},{1,TL_READ}};
struct st_test test_15[] = {{0,TL_WRITE_ALLOW_WRITE},{1,TL_READ}};
struct st_test test_16[] = {{0,TL_WRITE_ALLOW_WRITE},{1,TL_WRITE_ALLOW_WRITE}};

struct st_test *tests[] = {test_0,test_1,test_2,test_3,test_4,test_5,test_6,
			   test_7,test_8,test_9,test_10,test_11,test_12,
			   test_13,test_14,test_15,test_16};
int lock_counts[]= {sizeof(test_0)/sizeof(struct st_test),
		    sizeof(test_1)/sizeof(struct st_test),
		    sizeof(test_2)/sizeof(struct st_test),
		    sizeof(test_3)/sizeof(struct st_test),
		    sizeof(test_4)/sizeof(struct st_test),
		    sizeof(test_5)/sizeof(struct st_test),
		    sizeof(test_6)/sizeof(struct st_test),
		    sizeof(test_7)/sizeof(struct st_test),
		    sizeof(test_8)/sizeof(struct st_test),
		    sizeof(test_9)/sizeof(struct st_test),
		    sizeof(test_10)/sizeof(struct st_test),
		    sizeof(test_11)/sizeof(struct st_test),
		    sizeof(test_12)/sizeof(struct st_test),
		    sizeof(test_13)/sizeof(struct st_test),
		    sizeof(test_14)/sizeof(struct st_test),
		    sizeof(test_15)/sizeof(struct st_test),
		    sizeof(test_16)/sizeof(struct st_test)
};


static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;
static uint thread_count;
static ulong sum=0;

#define MAX_LOCK_COUNT 8

/* The following functions is for WRITE_CONCURRENT_INSERT */

static void test_get_status(void* param __attribute__((unused)))
{
}

static void test_copy_status(void* to __attribute__((unused)) ,
			     void *from __attribute__((unused)))
{
}

static my_bool test_check_status(void* param __attribute__((unused)))
{
  return 0;
}


static void *test_thread(void *arg)
{
  int i,j,param=*((int*) arg);
  THR_LOCK_DATA data[MAX_LOCK_COUNT];
  THR_LOCK_DATA *multi_locks[MAX_LOCK_COUNT];
  my_thread_init();

  printf("Thread %s (%d) started\n",my_thread_name(),param); fflush(stdout);

  for (i=0; i < lock_counts[param] ; i++)
    thr_lock_data_init(locks+tests[param][i].lock_nr,data+i,NULL);
  for (j=1 ; j < 10 ; j++)		/* try locking 10 times */
  {
    for (i=0; i < lock_counts[param] ; i++)
    {					/* Init multi locks */
      multi_locks[i]= &data[i];
      data[i].type= tests[param][i].lock_type;
    }
    thr_multi_lock(multi_locks,lock_counts[param]);
    pthread_mutex_lock(&LOCK_thread_count);
    {
      int tmp=rand() & 7;			/* Do something from 0-2 sec */
      if (tmp == 0)
	sleep(1);
      else if (tmp == 1)
	sleep(2);
      else
      {
	ulong k;
	for (k=0 ; k < (ulong) (tmp-2)*100000L ; k++)
	  sum+=k;
      }
    }
    pthread_mutex_unlock(&LOCK_thread_count);
    thr_multi_unlock(multi_locks,lock_counts[param]);
  }

  printf("Tread %s (%d) ended\n",my_thread_name(),param); fflush(stdout);
  thr_print_locks();
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  free((gptr) arg);
  return 0;
}


int main(int argc __attribute__((unused)),char **argv __attribute__((unused)))
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  int i,*param,error;
  MY_INIT(argv[0]);
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '#')
    DBUG_PUSH(argv[1]+2);

  printf("Main thread: %s\n",my_thread_name());

  if ((error=pthread_cond_init(&COND_thread_count,NULL)))
  {
    fprintf(stderr,"Got error: %d from pthread_cond_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  if ((error=pthread_mutex_init(&LOCK_thread_count,NULL)))
  {
    fprintf(stderr,"Got error: %d from pthread_cond_init (errno: %d)",
	    error,errno);
    exit(1);
  }

  for (i=0 ; i < (int) array_elements(locks) ; i++)
  {
    thr_lock_init(locks+i);
    locks[i].check_status= test_check_status;
    locks[i].update_status=test_get_status;
    locks[i].copy_status=  test_copy_status;
    locks[i].get_status=   test_get_status;
  }
  if ((error=pthread_attr_init(&thr_attr)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  if ((error=pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED)))
  {
    fprintf(stderr,
	    "Got error: %d from pthread_attr_setdetachstate (errno: %d)",
	    error,errno);
    exit(1);
  }
#ifndef pthread_attr_setstacksize		/* void return value */
  if ((error=pthread_attr_setstacksize(&thr_attr,65536L)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_setstacksize (errno: %d)",
	    error,errno);
    exit(1);
  }
#endif
#ifdef HAVE_THR_SETCONCURRENCY
  VOID(thr_setconcurrency(2));
#endif
  for (i=0 ; i < (int) array_elements(lock_counts) ; i++)
  {
    param=(int*) malloc(sizeof(int));
    *param=i;

    if ((error=pthread_mutex_lock(&LOCK_thread_count)))
    {
      fprintf(stderr,"Got error: %d from pthread_mutex_lock (errno: %d)",
	      error,errno);
      exit(1);
    }
    if ((error=pthread_create(&tid,&thr_attr,test_thread,(void*) param)))
    {
      fprintf(stderr,"Got error: %d from pthread_create (errno: %d)\n",
	      error,errno);
      pthread_mutex_unlock(&LOCK_thread_count);
      exit(1);
    }
    thread_count++;
    pthread_mutex_unlock(&LOCK_thread_count);
  }

  pthread_attr_destroy(&thr_attr);
  if ((error=pthread_mutex_lock(&LOCK_thread_count)))
    fprintf(stderr,"Got error: %d from pthread_mutex_lock\n",error);
  while (thread_count)
  {
    if ((error=pthread_cond_wait(&COND_thread_count,&LOCK_thread_count)))
      fprintf(stderr,"Got error: %d from pthread_cond_wait\n",error);
  }
  if ((error=pthread_mutex_unlock(&LOCK_thread_count)))
    fprintf(stderr,"Got error: %d from pthread_mutex_unlock\n",error);
  for (i=0 ; i < (int) array_elements(locks) ; i++)
    thr_lock_delete(locks+i);
#ifdef EXTRA_DEBUG
  if (found_errors)
    printf("Got %d warnings\n",found_errors);
  else
#endif
    printf("Test succeeded\n");
  return 0;
}

#else /* THREAD */

int main(int argc __attribute__((unused)),char **argv __attribute__((unused)))
{
  printf("thr_lock disabled because we are not using threads\n");
  exit(1);
}

#endif /* THREAD */
#endif /* MAIN */
