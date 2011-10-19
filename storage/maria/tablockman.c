/* QQ: TODO - allocate everything from dynarrays !!! (benchmark) */
/* QQ: automatically place S instead of LS if possible */
/* Copyright (C) 2006 MySQL AB

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

#include <my_base.h>
#include <hash.h>
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
  1. Hash of latest (see the lock upgrade section below) granted locks with
     loid as a key. Thus, checking if a given transaction has a lock on
     this resource is O(1) operation.
  2. Doubly-linked lists of all granted locks - one list for every lock
     type. Thus, checking if a conflicting lock exists is a check whether
     an appropriate list head pointer is not null, also O(1).
  3. Every lock has a loid of the owner, thus checking who owns a
     conflicting lock is also O(1).
  4. Deque of waiting locks. It's a deque (double-ended queue) not a fifo,
     because for lock upgrades requests are added to the queue head, not
     tail. This is a single place where there it gets O(N) on number
     of locks - when a transaction wakes up from waiting on a condition,
     it may need to scan the queue backward to the beginning to find
     a conflicting lock. It is guaranteed though that "all transactions
     before it" received the same - or earlier - signal.  In other words a
     transaction needs to scan all transactions before it that received the
     signal but didn't have a chance to resume the execution yet, so
     practically OS scheduler won't let the scan to be O(N).

  Waiting: if there is a conflicting lock or if wait queue is not empty, a
  requested lock cannot be granted at once. It is added to the end of the
  wait queue. If a queue was empty and there is a conflicting lock - the
  "blocker" transaction is the owner of this lock. If a queue is not empty,
  an owner of the previous lock in the queue is the "blocker". But if the
  previous lock is compatible with the request, then the "blocker" is the
  transaction that the owner of the lock at the end of the queue is waiting
  for (in other words, our lock is added to the end of the wait queue, and
  our blocker is the same as of the lock right before us).

  Lock upgrades: when a thread that has a lock on a given resource,
  requests a new lock on the same resource and the old lock is not enough
  to satisfy new lock requirements (which is defined by
  lock_combining_matrix[old_lock][new_lock] != old_lock), a new lock
  (defined by lock_combining_matrix as above) is placed. Depending on
  other granted locks it is immediately granted or it has to wait.  Here the
  lock is added to the start of the waiting queue, not to the end.  Old
  lock, is removed from the hash, but not from the doubly-linked lists.
  (indeed, a transaction checks "do I have a lock on this resource ?" by
  looking in a hash, and it should find a latest lock, so old locks must be
  removed; but a transaction checks "are there conflicting locks ?" by
  checking doubly-linked lists, it doesn't matter if it will find an old
  lock - if it would be removed, a new lock would be also a conflict).
  So, a hash contains only "latest" locks - there can be only one latest
  lock per resource per transaction. But doubly-linked lists contain all
  locks, even "obsolete" ones, because it doesnt't hurt. Note that old
  locks can not be freed early, in particular they stay in the
  'active_locks' list of a lock owner, because they may be "re-enabled"
  on a savepoint rollback.

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

  Mutexes: there're table mutexes (LOCKED_TABLE::mutex), lock owner mutexes
  (TABLE_LOCK_OWNER::mutex), and a pool mutex (TABLOCKMAN::pool_mutex).
  table mutex protects operations on the table lock structures, and lock
  owner pointers waiting_for and waiting_for_loid.
  lock owner mutex is only used to wait on lock owner condition
  (TABLE_LOCK_OWNER::cond), there's no need to protect owner's lock
  structures, and only lock owner itself may access them.
  The pool mutex protects a pool of unused locks. Note the locking order:
  first the table mutex, then the owner mutex or a pool mutex.
  Table mutex lock cannot be attempted when owner or pool mutex are locked.
  No mutex lock can be attempted if owner or pool mutex are locked.
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
static const int lock_compatibility_matrix[10][10]=
{ /* N    S   X  IS  IX  SIX LS  LX  SLX LSIX          */
  {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, /* N    */
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
static const enum lockman_lock_type lock_combining_matrix[10][10]=
{/*    N    S   X    IS    IX  SIX    LS    LX   SLX   LSIX         */
  {    N,   N,  N,    N,    N,   N,    N,    N,   N,    N}, /* N    */
  {    N,   S,  X,    S,  SIX, SIX,    S,  SLX, SLX,  SIX}, /* S    */
  {    N,   X,  X,    X,    X,   X,    X,    X,   X,    X}, /* X    */
  {    N,   S,  X,   IS,   IX, SIX,   LS,   LX, SLX, LSIX}, /* IS   */
  {    N, SIX,  X,   IX,   IX, SIX, LSIX,   LX, SLX, LSIX}, /* IX   */
  {    N, SIX,  X,  SIX,  SIX, SIX,  SIX,  SLX, SLX,  SIX}, /* SIX  */
  {    N,   S,  X,   LS, LSIX, SIX,   LS,   LX, SLX, LSIX}, /* LS   */
  {    N, SLX,  X,   LX,   LX, SLX,   LX,   LX, SLX,   LX}, /* LX   */
  {    N, SLX,  X,  SLX,  SLX, SLX,  SLX,  SLX, SLX,  SLX}, /* SLX  */
  {    N, SIX,  X, LSIX, LSIX, SIX, LSIX,   LX, SLX, LSIX}  /* LSIX */
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
static const enum lockman_getlock_result getlock_result[10][10]=
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
  /* QQ: do we need upgraded_from ? */
  struct st_table_lock *next_in_lo, *upgraded_from, *next, *prev;
  struct st_locked_table *table;
  uint16 loid;
  uchar  lock_type;
};

static inline
TABLE_LOCK *find_by_loid(LOCKED_TABLE *table, uint16 loid)
{
  return (TABLE_LOCK *)my_hash_search(& table->latest_locks,
                                   (uchar *)& loid, sizeof(loid));
}

static inline
void remove_from_wait_queue(TABLE_LOCK *lock, LOCKED_TABLE *table)
{
  DBUG_ASSERT(table == lock->table);
  if (lock->prev)
  {
    DBUG_ASSERT(table->wait_queue_out != lock);
    lock->prev->next= lock->next;
  }
  else
  {
    DBUG_ASSERT(table->wait_queue_out == lock);
    table->wait_queue_out= lock->next;
  }
  if (lock->next)
  {
    DBUG_ASSERT(table->wait_queue_in != lock);
    lock->next->prev= lock->prev;
  }
  else
  {
    DBUG_ASSERT(table->wait_queue_in == lock);
    table->wait_queue_in= lock->prev;
  }
}

/*
  DESCRIPTION
    tries to lock a resource 'table' with a lock level 'lock'.

  RETURN
    see enum lockman_getlock_result
*/
enum lockman_getlock_result
tablockman_getlock(TABLOCKMAN *lm, TABLE_LOCK_OWNER *lo,
                   LOCKED_TABLE *table, enum lockman_lock_type lock)
{
  TABLE_LOCK *old, *new, *blocker, *blocker2;
  TABLE_LOCK_OWNER *wait_for;
  struct timespec timeout;
  enum lockman_lock_type new_lock;
  enum lockman_getlock_result res;
  int i;

  DBUG_ASSERT(lo->waiting_lock == 0);
  DBUG_ASSERT(lo->waiting_for == 0);
  DBUG_ASSERT(lo->waiting_for_loid == 0);

  mysql_mutex_lock(& table->mutex);
  /* do we already have a lock on this resource ? */
  old= find_by_loid(table, lo->loid);

  /* calculate the level of the upgraded lock, if yes */
  new_lock= old ? lock_combining_matrix[old->lock_type][lock] : lock;

  /* and check if old lock is enough to satisfy the new request */
  if (old && new_lock == old->lock_type)
  {
    /* yes */
    res= getlock_result[old->lock_type][lock];
    goto ret;
  }

  /* no, placing a new lock. first - take a free lock structure from the pool */
  mysql_mutex_lock(& lm->pool_mutex);
  new= lm->pool;
  if (new)
  {
    lm->pool= new->next;
    mysql_mutex_unlock(& lm->pool_mutex);
  }
  else
  {
    mysql_mutex_unlock(& lm->pool_mutex);
    new= (TABLE_LOCK *)my_malloc(sizeof(*new), MYF(MY_WME));
    if (unlikely(!new))
    {
      res= NO_MEMORY_FOR_LOCK;
      goto ret;
    }
  }

  new->loid= lo->loid;
  new->lock_type= new_lock;
  new->table= table;

  /* and try to place it */
  for (new->prev= table->wait_queue_in;;)
  {
    wait_for= 0;
    if (!old)
    {
      /* not upgrading - a lock must be added to the _end_ of the wait queue */
      for (blocker= new->prev; blocker && !wait_for; blocker= blocker->prev)
      {
        TABLE_LOCK_OWNER *tmp= lm->loid_to_tlo(blocker->loid);

        /* find a blocking lock */
        DBUG_ASSERT(table->wait_queue_out);
        DBUG_ASSERT(table->wait_queue_in);
        if (!lock_compatibility_matrix[blocker->lock_type][lock])
        {
          /* found! */
          wait_for= tmp;
          break;
        }

        /*
          hmm, the lock before doesn't block us, let's look one step further.
          the condition below means:

            if we never waited on a condition yet
            OR
            the lock before ours (blocker) waits on a lock (blocker2) that is
               present in the hash AND and conflicts with 'blocker'

            the condition after OR may fail if 'blocker2' was removed from
            the hash, its signal woke us up, but 'blocker' itself didn't see
            the signal yet.
        */
        if (!lo->waiting_lock ||
            ((blocker2= find_by_loid(table, tmp->waiting_for_loid)) &&
            !lock_compatibility_matrix[blocker2->lock_type]
                                      [blocker->lock_type]))
        {
          /* but it's waiting for a real lock. we'll wait for the same lock */
          wait_for= tmp->waiting_for;
          /*
            We don't really need tmp->waiting_for, as tmp->waiting_for_loid
            is enough.  waiting_for is just a local cache to avoid calling
            loid_to_tlo().
            But it's essensial that tmp->waiting_for pointer can ONLY
            be dereferenced if find_by_loid() above returns a non-null
            pointer, because a TABLE_LOCK_OWNER object that it points to
            may've been freed when we come here after a signal.
            In particular tmp->waiting_for_loid cannot be replaced
            with tmp->waiting_for->loid.
          */
          DBUG_ASSERT(wait_for == lm->loid_to_tlo(tmp->waiting_for_loid));
          break;
        }

        /*
          otherwise - a lock it's waiting for doesn't exist.
          We've no choice but to scan the wait queue backwards, looking
          for a conflicting lock or a lock waiting for a real lock.
          QQ is there a way to avoid this scanning ?
        */
      }
    }

    if (wait_for == 0)
    {
      /* checking for compatibility with existing locks */
      for (blocker= 0, i= 0; i < LOCK_TYPES; i++)
      {
        if (table->active_locks[i] && !lock_compatibility_matrix[i+1][lock])
        {
          blocker= table->active_locks[i];
          /* if the first lock in the list is our own - skip it */
          if (blocker->loid == lo->loid)
            blocker= blocker->next;
          if (blocker) /* found a conflicting lock, need to wait */
            break;
        }
      }
      if (!blocker) /* free to go */
        break;
      wait_for= lm->loid_to_tlo(blocker->loid);
    }

    /* ok, we're here - the wait is inevitable */
    lo->waiting_for= wait_for;
    lo->waiting_for_loid= wait_for->loid;
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
          table->wait_queue_in= table->wait_queue_out;
      }
      else
      {
        /* new lock - add the lock to the _end_ of the wait queue */
        new->next= 0;
        if ((new->prev= table->wait_queue_in))
          new->prev->next= new;
        table->wait_queue_in= new;
        if (!table->wait_queue_out)
          table->wait_queue_out= table->wait_queue_in;
      }
      lo->waiting_lock= new;

      set_timespec_nsec(timeout,lm->lock_timeout * 1000000);

    }

    /*
      prepare to wait.
      we must lock blocker's mutex to wait on blocker's cond.
      and we must release table's mutex.
      note that blocker's mutex is locked _before_ table's mutex is released
    */
    mysql_mutex_lock(wait_for->mutex);
    mysql_mutex_unlock(& table->mutex);

    /* now really wait */
    i= mysql_cond_timedwait(wait_for->cond, wait_for->mutex, & timeout);

    mysql_mutex_unlock(wait_for->mutex);

    if (i == ETIMEDOUT || i == ETIME)
    {
      /* we rely on the caller to rollback and release all locks */
      res= LOCK_TIMEOUT;
      goto ret2;
    }

    mysql_mutex_lock(& table->mutex);

    /* ... and repeat from the beginning */
  }
  /* yeah! we can place the lock now */

  /* remove the lock from the wait queue, if it was there */
  if (lo->waiting_lock)
  {
    remove_from_wait_queue(new, table);
    lo->waiting_lock= 0;
    lo->waiting_for= 0;
    lo->waiting_for_loid= 0;
  }

  /* add it to the list of all locks of this lock owner */
  new->next_in_lo= lo->active_locks;
  lo->active_locks= new;

  /* and to the list of active locks of this lock type */
  new->prev= 0;
  if ((new->next= table->active_locks[new_lock-1]))
    new->next->prev= new;
  table->active_locks[new_lock-1]= new;

  /* update the latest_locks hash */
  if (old)
    my_hash_delete(& table->latest_locks, (uchar *)old);
  my_hash_insert(& table->latest_locks, (uchar *)new);

  new->upgraded_from= old;

  res= getlock_result[lock][lock];

ret:
  mysql_mutex_unlock(& table->mutex);
ret2:
  DBUG_ASSERT(res);
  return res;
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
    mysql_mutex_lock(& lock->table->mutex);
    remove_from_wait_queue(lock, lock->table);

    /*
      a special case: if this lock was not the last in the wait queue
      and it's compatible with the next lock, than the next lock
      is waiting for our blocker though really it waits for us, indirectly.
      Signal our blocker to release this next lock (after we removed our
      lock from the wait queue, of course).
    */
    /*
      An example to clarify the above:
        trn1> S-lock the table. Granted.
        trn2> IX-lock the table. Added to the wait queue. trn2 waits on trn1
        trn3> IS-lock the table.  The queue is not empty, so IS-lock is added
              to the queue. It's compatible with the waiting IX-lock, so trn3
              waits for trn2->waiting_for, that is trn1.
      if trn1 releases the lock it signals trn1->cond and both waiting
      transactions are awaken. But if trn2 times out, trn3 must be notified
      too (as IS and S locks are compatible). So trn2 must signal trn1->cond.
    */
    if (lock->next &&
        lock_compatibility_matrix[lock->next->lock_type][lock->lock_type])
    {
      mysql_mutex_lock(lo->waiting_for->mutex);
      mysql_cond_broadcast(lo->waiting_for->cond);
      mysql_mutex_unlock(lo->waiting_for->mutex);
    }
    lo->waiting_for= 0;
    lo->waiting_for_loid= 0;
    mysql_mutex_unlock(& lock->table->mutex);

    lock->next= local_pool;
    local_pool= lock;
  }

  /* now release granted locks */
  lock= lo->active_locks;
  while (lock)
  {
    TABLE_LOCK *cur= lock;
    mysql_mutex_t *mutex= & lock->table->mutex;
    DBUG_ASSERT(cur->loid == lo->loid);

    DBUG_ASSERT(lock != lock->next_in_lo);
    lock= lock->next_in_lo;

    /* TODO ? group locks by table to reduce the number of mutex locks */
    mysql_mutex_lock(mutex);
    my_hash_delete(& cur->table->latest_locks, (uchar *)cur);

    if (cur->prev)
      cur->prev->next= cur->next;
    if (cur->next)
      cur->next->prev= cur->prev;
    if (cur->table->active_locks[cur->lock_type-1] == cur)
      cur->table->active_locks[cur->lock_type-1]= cur->next;

    cur->next= local_pool;
    local_pool= cur;

    mysql_mutex_unlock(mutex);
  }

  lo->waiting_lock= lo->active_locks= 0;

  /*
    okay, all locks released. now signal that we're leaving,
    in case somebody's waiting for it
  */
  mysql_mutex_lock(lo->mutex);
  mysql_cond_broadcast(lo->cond);
  mysql_mutex_unlock(lo->mutex);

  /* and push all freed locks to the lockman's pool */
  mysql_mutex_lock(& lm->pool_mutex);
  local_pool_end->next= lm->pool;
  lm->pool= local_pool;
  mysql_mutex_unlock(& lm->pool_mutex);
}

void tablockman_init(TABLOCKMAN *lm, loid_to_tlo_func *func, uint timeout)
{
  lm->pool= 0;
  lm->loid_to_tlo= func;
  lm->lock_timeout= timeout;
  mysql_mutex_init(& lm->pool_mutex, MY_MUTEX_INIT_FAST);
  my_interval_timer(); /* ensure that my_interval_timer() is initialized */
}

void tablockman_destroy(TABLOCKMAN *lm)
{
  while (lm->pool)
  {
    TABLE_LOCK *tmp= lm->pool;
    lm->pool= tmp->next;
    my_free(tmp);
  }
  mysql_mutex_destroy(& lm->pool_mutex);
}

/*
  initialize a LOCKED_TABLE structure

  SYNOPSYS
    lt                          a LOCKED_TABLE to initialize
    initial_hash_size           initial size for 'latest_locks' hash
*/
void tablockman_init_locked_table(LOCKED_TABLE *lt, int initial_hash_size)
{
  bzero(lt, sizeof(*lt));
  mysql_mutex_init(& lt->mutex, MY_MUTEX_INIT_FAST);
  my_hash_init(& lt->latest_locks, & my_charset_bin, initial_hash_size,
            offsetof(TABLE_LOCK, loid),
            sizeof(((TABLE_LOCK*)0)->loid), 0, 0, 0);
}

void tablockman_destroy_locked_table(LOCKED_TABLE *lt)
{
  int i;

  DBUG_ASSERT(lt->wait_queue_out == 0);
  DBUG_ASSERT(lt->wait_queue_in == 0);
  DBUG_ASSERT(lt->latest_locks.records == 0);
  for (i= 0; i<LOCK_TYPES; i++)
     DBUG_ASSERT(lt->active_locks[i] == 0);

  my_hash_free(& lt->latest_locks);
  mysql_mutex_destroy(& lt->mutex);
}

#ifdef EXTRA_DEBUG
static const char *lock2str[LOCK_TYPES+1]= {"N", "S", "X", "IS", "IX", "SIX",
  "LS", "LX", "SLX", "LSIX"};

void tablockman_print_tlo(TABLE_LOCK_OWNER *lo)
{
  TABLE_LOCK *lock;

  printf("lo%d>", lo->loid);
  if ((lock= lo->waiting_lock))
    printf(" (%s.0x%lx)", lock2str[lock->lock_type], (ulong)lock->table);
  for (lock= lo->active_locks;
       lock && lock != lock->next_in_lo;
       lock= lock->next_in_lo)
    printf(" %s.0x%lx", lock2str[lock->lock_type], (ulong)lock->table);
  if (lock && lock == lock->next_in_lo)
    printf("!");
  printf("\n");
}
#endif

