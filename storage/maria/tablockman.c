// TODO - allocate everything from dynarrays !!! (benchmark)
// automatically place S instead of LS if possible
/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include <my_sys.h>
#include <my_bit.h>
#include <lf.h>
#include "tablockman.h"

/*
  Lock Manager for Table Locks

  The code below handles locks on resources - but it is optimized for a
  case when a number of resources is not very large, and there are many of
  locks per resource - that is a resource is likely to be a table or a
  database, but hardly a row in a table.

  Locks belong to "lock owners". A Lock Owner is uniquely identified by a
  16-bit number - loid (lock owner identifier). A function loid_to_tlo must
  be provided by the application that takes such a number as an argument
  and returns a TABLE_LOCK_OWNER structure.

  Lock levels are completely defined by three tables. Lock compatibility
  matrix specifies which locks can be held at the same time on a resource.
  Lock combining matrix specifies what lock level has the same behaviour as
  a pair of two locks of given levels. getlock_result matrix simplifies
  intention locking and lock escalation for an application, basically it
  defines which locks are intention locks and which locks are "loose"
  locks.  It is only used to provide better diagnostics for the
  application, lock manager itself does not differentiate between normal,
  intention, and loose locks.

  The assumptions are: few distinct resources, many locks are held at the
  same time on one resource. Thus: a lock structure _per resource_ can be
  rather large; a lock structure _per lock_ does not need to be very small
  either; we need to optimize for _speed_. Operations we need are: place a
  lock, check if a particular transaction already has a lock on this
  resource, check if a conflicting lock exists, if yes - find who owns it.

  Solution: every resource has a structure with
  1. Hash of "active" (see below for the description of "active") granted
     locks with loid as a key. Thus, checking if a given transaction has a
     lock on this resource is O(1) operation.
  2. Doubly-linked lists of all granted locks - one list for every lock
     type. Thus, checking if a conflicting lock exists is a check whether
     an appropriate list head pointer is not null, also O(1).
  3. Every lock has a loid of the owner, thus checking who owns a
     conflicting lock is also O(1).
  4. Deque of waiting locks. It's a deque not a fifo, because for lock
     upgrades requests are added to the queue head, not tail. There's never
     a need to scan the queue.

  Result: adding or removing a lock is always a O(1) operation, it does not
  depend on the number of locks on the resource, or number of transactions,
  or number of resources. It _does_ depend on the number of different lock
  levels - O(number_of_lock_levels) - but it's a constant.

  Waiting: if there is a conflicting lock or if wait queue is not empty, a
  requested lock cannot be granted at once. It is added to the end of the
  wait queue. If there is a conflicting lock - the "blocker" transaction is
  the owner of this lock. If there's no conflict but a queue was not empty,
  than the "blocker" is the transaction that the owner of the lock at the
  end of the queue is waiting for (in other words, our lock is added to the
  end of the wait queue, and our blocker is the same as of the lock right
  before us).

  Lock upgrades: when a thread that has a lock on a given resource,
  requests a new lock on the same resource and the old lock is not enough
  to satisfy new lock requirements (which is defined by
  lock_combining_matrix[old_lock][new_lock] != old_lock), a new lock
  (defineded by lock_combining_matrix as above) is placed. Depending on
  other granted locks it is immediately active or it has to wait.  Here the
  lock is added to the start of the waiting queue, not to the end.  Old
  lock, is removed from the hash, but not from the doubly-linked lists.
  (indeed, a transaction checks "do I have a lock on this resource ?" by
  looking in a hash, and it should find a latest lock, so old locks must be
  removed; but a transaction checks "are the conflicting locks ?" by
  checking doubly-linked lists, it doesn't matter if it will find an old
  lock - if it would be removed, a new lock would be also a conflict).

  To better support table-row relations where one needs to lock the table
  with an intention lock before locking the row, extended diagnostics is
  provided.  When an intention lock (presumably on a table) is granted,
  lockman_getlock() returns one of GOT_THE_LOCK (no need to lock the row,
  perhaps the thread already has a normal lock on this table),
  GOT_THE_LOCK_NEED_TO_LOCK_A_SUBRESOURCE (need to lock the row, as usual),
  GOT_THE_LOCK_NEED_TO_INSTANT_LOCK_A_SUBRESOURCE (only need to check
  whether it's possible to lock the row, but no need to lock it - perhaps
  the thread has a loose lock on this table). This is defined by
  getlock_result[] table.

  Instant duration locks are not supported. Though they're trivial to add,
  they are normally only used on rows, not on tables. So, presumably,
  they are not needed here.
*/

/*
  Lock compatibility matrix.

  It's asymmetric. Read it as "Somebody has the lock <value in the row
  label>, can I set the lock <value in the column label> ?"

  ') Though you can take LS lock while somebody has S lock, it makes no
  sense - it's simpler to take S lock too.

  1  - compatible
  0  - incompatible
  -1 - "impossible", so that we can assert the impossibility.
*/
static int lock_compatibility_matrix[10][10]=
{ /* N    S   X  IS  IX  SIX LS  LX  SLX LSIX          */
  {  -1,  1,  1,  1,  1,  1,  1,  1,  1,  1 }, /* N    */
  {  -1,  1,  0,  1,  0,  0,  1,  0,  0,  0 }, /* S    */
  {  -1,  0,  0,  0,  0,  0,  0,  0,  0,  0 }, /* X    */
  {  -1,  1,  0,  1,  1,  1,  1,  1,  1,  1 }, /* IS   */
  {  -1,  0,  0,  1,  1,  0,  1,  1,  0,  1 }, /* IX   */
  {  -1,  0,  0,  1,  0,  0,  1,  0,  0,  0 }, /* SIX  */
  {  -1,  1,  0,  1,  0,  0,  1,  0,  0,  0 }, /* LS   */
  {  -1,  0,  0,  0,  0,  0,  0,  0,  0,  0 }, /* LX   */
  {  -1,  0,  0,  0,  0,  0,  0,  0,  0,  0 }, /* SLX  */
  {  -1,  0,  0,  1,  0,  0,  1,  0,  0,  0 }  /* LSIX */
};

/*
  Lock combining matrix.

  It's symmetric. Read it as "what lock level L is identical to the
  set of two locks A and B"

  One should never get N from it, we assert the impossibility
*/
static enum lock_type lock_combining_matrix[10][10]=
{/*    N    S   X    IS    IX  SIX    LS    LX   SLX   LSIX         */
  {    N,   S,  X,   IS,   IX, SIX,    S,  SLX, SLX,  SIX}, /* N    */
  {    S,   S,  X,    S,  SIX, SIX,    S,  SLX, SLX,  SIX}, /* S    */
  {    X,   X,  X,    X,    X,   X,    X,    X,   X,    X}, /* X    */
  {   IS,   S,  X,   IS,   IX, SIX,   LS,   LX, SLX, LSIX}, /* IS   */
  {   IX, SIX,  X,   IX,   IX, SIX, LSIX,   LX, SLX, LSIX}, /* IX   */
  {  SIX, SIX,  X,  SIX,  SIX, SIX,  SIX,  SLX, SLX,  SIX}, /* SIX  */
  {   LS,   S,  X,   LS, LSIX, SIX,   LS,   LX, SLX, LSIX}, /* LS   */
  {   LX, SLX,  X,   LX,   LX, SLX,   LX,   LX, SLX,   LX}, /* LX   */
  {  SLX, SLX,  X,  SLX,  SLX, SLX,  SLX,  SLX, SLX,  SLX}, /* SLX  */
  { LSIX, SIX,  X, LSIX, LSIX, SIX, LSIX,   LX, SLX, LSIX}  /* LSIX */
};

/*
  the return codes for lockman_getlock

  It's asymmetric. Read it as "I have the lock <value in the row label>,
  what value should be returned for <value in the column label> ?"

  0 means impossible combination (assert!)

  Defines below help to preserve the table structure.
  I/L/A values are self explanatory
  x means the combination is possible (assert should not crash)
    but it cannot happen in row locks, only in table locks (S,X),
    or lock escalations (LS,LX)
*/
#define I GOT_THE_LOCK_NEED_TO_LOCK_A_SUBRESOURCE
#define L GOT_THE_LOCK_NEED_TO_INSTANT_LOCK_A_SUBRESOURCE
#define A GOT_THE_LOCK
#define x GOT_THE_LOCK
static enum lockman_getlock_result getlock_result[10][10]=
{/*    N    S   X    IS    IX  SIX    LS    LX   SLX   LSIX         */
  {    0,   0,  0,    0,    0,   0,    0,    0,   0,    0}, /* N    */
  {    0,   x,  0,    A,    0,   0,    x,    0,   0,    0}, /* S    */
  {    0,   x,  x,    A,    A,   0,    x,    x,   0,    0}, /* X    */
  {    0,   0,  0,    I,    0,   0,    0,    0,   0,    0}, /* IS   */
  {    0,   0,  0,    I,    I,   0,    0,    0,   0,    0}, /* IX   */
  {    0,   x,  0,    A,    I,   0,    x,    0,   0,    0}, /* SIX  */
  {    0,   0,  0,    L,    0,   0,    x,    0,   0,    0}, /* LS   */
  {    0,   0,  0,    L,    L,   0,    x,    x,   0,    0}, /* LX   */
  {    0,   x,  0,    A,    L,   0,    x,    x,   0,    0}, /* SLX  */
  {    0,   0,  0,    L,    I,   0,    x,    0,   0,    0}  /* LSIX */
};
#undef I
#undef L
#undef A
#undef x

/*
  this structure is optimized for a case when there're many locks
  on the same resource - e.g. a table
*/

struct st_table_lock {
  struct st_table_lock *next_in_lo, *upgraded_from, *next, *prev;
  struct st_locked_table *table;
  uint16 loid;
  char   lock_type;
};

#define hash_insert my_hash_insert /* for consistency :) */
#define remove_from_wait_queue(LOCK, TABLE)                     \
  do                                                            \
  {                                                             \
    if ((LOCK)->prev)                                           \
    {                                                           \
      DBUG_ASSERT((TABLE)->wait_queue_out != (LOCK));           \
      (LOCK)->prev->next= (LOCK)->next;                         \
    }                                                           \
    else                                                        \
    {                                                           \
      DBUG_ASSERT((TABLE)->wait_queue_out == (LOCK));           \
      (TABLE)->wait_queue_out= (LOCK)->next;                    \
    }                                                           \
    if ((LOCK)->next)                                           \
    {                                                           \
      DBUG_ASSERT((TABLE)->wait_queue_in != (LOCK));            \
      (LOCK)->next->prev= (LOCK)->prev;                         \
    }                                                           \
    else                                                        \
    {                                                           \
      DBUG_ASSERT((TABLE)->wait_queue_in == (LOCK));            \
      (TABLE)->wait_queue_in= (LOCK)->prev;                     \
    }                                                           \
  } while (0)

/*
  DESCRIPTION
    tries to lock a resource 'table' with a lock level 'lock'.

  RETURN
    see enum lockman_getlock_result
*/
enum lockman_getlock_result
tablockman_getlock(TABLOCKMAN *lm, TABLE_LOCK_OWNER *lo,
                   LOCKED_TABLE *table, enum lock_type lock)
{
  TABLE_LOCK *old, *new, *blocker;
  TABLE_LOCK_OWNER *wait_for;
  ulonglong deadline;
  struct timespec timeout;
  enum lock_type new_lock;
  int i;

  pthread_mutex_lock(& table->mutex);
  /* do we alreasy have a lock on this resource ? */
  old= (TABLE_LOCK *)hash_search(& table->active, (byte *)&lo->loid,
                                 sizeof(lo->loid));

  /* and if yes, is it enough to satisfy the new request */
  if (old && lock_combining_matrix[old->lock_type][lock] == old->lock_type)
  {
    /* yes */
    pthread_mutex_unlock(& table->mutex);
    return getlock_result[old->lock_type][lock];
  }

  /* no, placing a new lock. first - take a free lock structure from the pool */
  pthread_mutex_lock(& lm->pool_mutex);
  new= lm->pool;
  if (new)
  {
    lm->pool= new->next;
    pthread_mutex_unlock(& lm->pool_mutex);
  }
  else
  {
    pthread_mutex_unlock(& lm->pool_mutex);
    new= (TABLE_LOCK *)my_malloc(sizeof(*new), MYF(MY_WME));
    if (!new)
    {
      pthread_mutex_unlock(& table->mutex);
      return DIDNT_GET_THE_LOCK;
    }
  }

  /* calculate the level of the upgraded lock */
  new_lock= old ? lock_combining_matrix[old->lock_type][lock] : lock;

  new->loid= lo->loid;
  new->lock_type= new_lock;
  new->table= table;

  /* and try to place it */
  for (new->prev= table->wait_queue_in ; ; )
  {
    /* waiting queue is not empty and we're not upgrading */
    if (!old && new->prev)
    {
      /* need to wait */
      DBUG_ASSERT(table->wait_queue_out);
      DBUG_ASSERT(table->wait_queue_in);
      blocker= new->prev;
      /* wait for a previous lock in the queue or for a lock it's waiting for */
      if (lock_compatibility_matrix[blocker->lock_type][lock])
        wait_for= lm->loid_to_tlo(blocker->loid)->waiting_for;
      else
        wait_for= lm->loid_to_tlo(blocker->loid);
    }
    else
    {
      /* checking for compatibility with existing locks */
      for (blocker= 0, i= 0; i < LOCK_TYPES; i++)
      {
        if (table->active_locks[i] && !lock_compatibility_matrix[i+1][lock])
        {
          /* the first lock in the list may be our own - skip it */
          for (blocker= table->active_locks[i];
               blocker && blocker->loid == lo->loid;
               blocker= blocker->next) /* no-op */;
          if (blocker)
            break;
        }
      }
      if (!blocker) /* free to go */
        break;
      wait_for= lm->loid_to_tlo(blocker->loid);
    }

    /* ok, we're here - the wait is inevitable */
    lo->waiting_for= wait_for;
    if (!lo->waiting_lock) /* first iteration of the for() loop */
    {
      /* lock upgrade or new lock request ? */
      if (old)
      {
        /* upgrade - add the lock to the _start_ of the wait queue */
        new->prev= 0;
        if ((new->next= table->wait_queue_out))
          new->next->prev= new;
        table->wait_queue_out= new;
        if (!table->wait_queue_in)
          table->wait_queue_in=table->wait_queue_out;
      }
      else
      {
        /* new lock - add the lock to the _end_ of the wait queue */
        new->next= 0;
        if ((new->prev= table->wait_queue_in))
          new->prev->next= new;
        table->wait_queue_in= new;
        if (!table->wait_queue_out)
          table->wait_queue_out=table->wait_queue_in;
      }
      lo->waiting_lock= new;

      deadline= my_getsystime() + lm->lock_timeout * 10000;
      timeout.tv_sec= deadline/10000000;
      timeout.tv_nsec= (deadline % 10000000) * 100;
    }
    else
    {
      if (my_getsystime() > deadline)
      {
        pthread_mutex_unlock(& table->mutex);
        return DIDNT_GET_THE_LOCK;
      }
    }

    /* now really wait */
    pthread_mutex_lock(wait_for->mutex);
    pthread_mutex_unlock(& table->mutex);

    pthread_cond_timedwait(wait_for->cond, wait_for->mutex, &timeout);

    pthread_mutex_unlock(wait_for->mutex);
    pthread_mutex_lock(& table->mutex);

    /* ... and repeat from the beginning */
  }
  /* yeah! we can place the lock now */

  /* remove the lock from the wait queue, if it was there */
  if (lo->waiting_lock)
  {
    remove_from_wait_queue(new, table);
    lo->waiting_lock= 0;
    lo->waiting_for= 0;
  }

  /* add it to the list of all locks of this lock owner */
  new->next_in_lo= lo->active_locks;
  lo->active_locks= new;

  /* and to the list of active locks of this lock type */
  new->prev= 0;
  if ((new->next= table->active_locks[new_lock-1]))
    new->next->prev= new;
  table->active_locks[new_lock-1]= new;

  /* remove the old lock from the hash, if upgrading */
  if (old)
  {
    new->upgraded_from= old;
    hash_delete(& table->active, (byte *)old);
  }
  else
    new->upgraded_from= 0;

  /* and add a new lock to the hash, voila */
  hash_insert(& table->active, (byte *)new);

  pthread_mutex_unlock(& table->mutex);
  return getlock_result[lock][lock];
}

/*
  DESCRIPTION
    release all locks belonging to a transaction.
    signal waiters to continue
*/
void tablockman_release_locks(TABLOCKMAN *lm, TABLE_LOCK_OWNER *lo)
{
  TABLE_LOCK *lock, *local_pool= 0, *local_pool_end;

  /*
    instead of adding released locks to a pool one by one, we'll link
    them in a list and add to a pool in one short action (under a mutex)
  */
  local_pool_end= lo->waiting_lock ? lo->waiting_lock : lo->active_locks;
  if (!local_pool_end)
    return;

  /* release a waiting lock, if any */
  if ((lock= lo->waiting_lock))
  {
    DBUG_ASSERT(lock->loid == lo->loid);
    pthread_mutex_lock(& lock->table->mutex);
    remove_from_wait_queue(lock, lock->table);

    /*
      a special case: if this lock was not the last in the wait queue
      and it's compatible with the next lock, than the next lock
      is waiting for our blocker though really it waits for us, indirectly.
      Signal our blocker to release this next lock (after we removed our
      lock from the wait queue, of course).
    */
    if (lock->prev &&
        lock_compatibility_matrix[lock->prev->lock_type][lock->lock_type])
    {
      pthread_mutex_lock(lo->waiting_for->mutex);
      pthread_cond_broadcast(lo->waiting_for->cond);
      pthread_mutex_unlock(lo->waiting_for->mutex);
    }
    lo->waiting_for= 0;
    pthread_mutex_unlock(& lock->table->mutex);

    lock->next= local_pool;
    local_pool= lock;
  }

  /* now release granted locks */
  lock= lo->active_locks;
  while (lock)
  {
    TABLE_LOCK *cur= lock;
    pthread_mutex_t *mutex= & lock->table->mutex;
    DBUG_ASSERT(cur->loid == lo->loid);

    lock= lock->next_in_lo;

    /* TODO ? group locks by table to reduce the number of mutex locks */
    pthread_mutex_lock(mutex);
    hash_delete(& cur->table->active, (byte *)cur);

    if (cur->prev)
      cur->prev->next= cur->next;
    if (cur->next)
      cur->next->prev= cur->prev;
    if (cur->table->active_locks[cur->lock_type-1] == cur)
      cur->table->active_locks[cur->lock_type-1]= cur->next;

    cur->next= local_pool;
    local_pool= cur;

    pthread_mutex_unlock(mutex);
  }

  lo->waiting_lock= lo->active_locks= 0;

  /*
    okay, all locks released. now signal that we're leaving,
    in case somebody's waiting for it
  */
  pthread_mutex_lock(lo->mutex);
  pthread_cond_broadcast(lo->cond);
  pthread_mutex_unlock(lo->mutex);

  /* and push all freed locks to the lockman's pool */
  pthread_mutex_lock(& lm->pool_mutex);
  local_pool_end->next= lm->pool;
  lm->pool= local_pool;
  pthread_mutex_unlock(& lm->pool_mutex);
}

void tablockman_init(TABLOCKMAN *lm, loid_to_tlo_func *func, uint timeout)
{
  lm->pool= 0;
  lm->loid_to_tlo= func;
  lm->lock_timeout= timeout;
  pthread_mutex_init(&lm->pool_mutex, MY_MUTEX_INIT_FAST);
}

void tablockman_destroy(TABLOCKMAN *lm)
{
  while (lm->pool)
  {
    TABLE_LOCK *tmp= lm->pool;
    lm->pool= tmp->next;
    my_free((void *)tmp, MYF(0));
  }
  pthread_mutex_destroy(&lm->pool_mutex);
}

void tablockman_init_locked_table(LOCKED_TABLE *lt, int initial_hash_size)
{
  TABLE_LOCK *unused;
  bzero(lt, sizeof(*lt));
  pthread_mutex_init(& lt->mutex, MY_MUTEX_INIT_FAST);
  hash_init(& lt->active, &my_charset_bin, initial_hash_size,
            offsetof(TABLE_LOCK, loid), sizeof(unused->loid), 0, 0, 0);
}

void tablockman_destroy_locked_table(LOCKED_TABLE *lt)
{
  hash_free(& lt->active);
  pthread_mutex_destroy(& lt->mutex);
}

#ifdef EXTRA_DEBUG
static char *lock2str[LOCK_TYPES+1]= {"N", "S", "X", "IS", "IX", "SIX",
  "LS", "LX", "SLX", "LSIX"};

void print_tlo(TABLE_LOCK_OWNER *lo)
{
  TABLE_LOCK *lock;
  printf("lo%d>", lo->loid);
  if ((lock= lo->waiting_lock))
    printf(" (%s.%p)", lock2str[lock->lock_type], lock->table);
  for (lock= lo->active_locks; lock && lock != lock->next_in_lo; lock= lock->next_in_lo)
    printf(" %s.%p", lock2str[lock->lock_type], lock->table);
  if (lock && lock == lock->next_in_lo)
    printf("!");
  printf("\n");
}
#endif

