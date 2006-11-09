// TODO - allocate everything from dynarrays !!! (benchmark)
// TODO instant duration locks
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

enum lockman_getlock_result
tablockman_getlock(TABLOCKMAN *lm, TABLE_LOCK_OWNER *lo, LOCKED_TABLE *table, enum lock_type lock)
{
  TABLE_LOCK *old, *new, *blocker;
  TABLE_LOCK_OWNER *wait_for;
  int i;
  ulonglong deadline;
  struct timespec timeout;
  enum lock_type new_lock;

  pthread_mutex_lock(& table->mutex);
  old= (TABLE_LOCK *)hash_search(& table->active, (byte *)&lo->loid,
                                 sizeof(lo->loid));

  /* perhaps we have the lock already ? */
  if (old && lock_combining_matrix[old->lock_type][lock] == old->lock_type)
  {
    pthread_mutex_unlock(& table->mutex);
    return getlock_result[old->lock_type][lock];
  }

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

  /* calculate required upgraded lock type */
  new_lock= old ? lock_combining_matrix[old->lock_type][lock] : lock;

  new->loid= lo->loid;
  new->lock_type= new_lock;
  new->table= table;

  for (new->next= table->wait_queue_in ; ; )
  {
    if (!old && new->next)
    {
      /* need to wait */
      DBUG_ASSERT(table->wait_queue_out);
      DBUG_ASSERT(table->wait_queue_in);
      blocker= new->next;
      if (lock_compatibility_matrix[blocker->lock_type][lock])
        wait_for= lm->loid_to_lo(blocker->loid)->waiting_for;
      else
        wait_for= lm->loid_to_lo(blocker->loid);
    }
    else
    {
      /* checking for compatibility with existing locks */
      for (blocker= 0, i= 0; i < LOCK_TYPES; i++)
      {
        if (table->active_locks[i] && !lock_compatibility_matrix[i+1][lock])
        {
          for (blocker= table->active_locks[i];
               blocker && blocker->loid == lo->loid;
               blocker= blocker->next);
          if (blocker)
            break;
        }
      }
      if (!blocker)
        break;
      wait_for= lm->loid_to_lo(blocker->loid);
    }

    lo->waiting_for= wait_for;
    if (!lo->waiting_lock) /* first iteration */
    {
      /* lock upgrade or new lock request ? */
      if (old)
      {
        new->prev= 0;
        if ((new->next= table->wait_queue_out))
          new->next->prev= new;
        table->wait_queue_out= new;
        if (!table->wait_queue_in)
          table->wait_queue_in=table->wait_queue_out;
      }
      else
      {
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

    pthread_mutex_lock(wait_for->mutex);
    pthread_mutex_unlock(& table->mutex);

    pthread_cond_timedwait(wait_for->cond, wait_for->mutex, &timeout);

    pthread_mutex_unlock(wait_for->mutex);
    pthread_mutex_lock(& table->mutex);
  }

  if (lo->waiting_lock)
  {
    if (new->prev)
      new->prev->next= new->next;
    if (new->next)
      new->next->prev= new->prev;
    if (table->wait_queue_in == new)
      table->wait_queue_in= new->prev;
    if (table->wait_queue_out == new)
      table->wait_queue_out= new->next;

    lo->waiting_lock= 0;
  }

  new->next_in_lo= lo->active_locks;
  lo->active_locks= new;

  new->prev= 0;
  if ((new->next= table->active_locks[new_lock-1]))
    new->next->prev= new;
  table->active_locks[new_lock-1]= new;

  /* placing the lock */
  hash_insert(& table->active, (byte *)new);
  if (old)
  {
    new->upgraded_from= old;
    hash_delete(& table->active, (byte *)old);
  }
  else
    new->upgraded_from= 0;

  pthread_mutex_unlock(& table->mutex);
  return getlock_result[lock][lock];
}

void tablockman_release_locks(TABLOCKMAN *lm, TABLE_LOCK_OWNER *lo)
{
  TABLE_LOCK *lock, *tmp, *local_pool= 0, *local_pool_end;

  local_pool_end= lo->waiting_lock ? lo->waiting_lock : lo->active_locks;
  if (!local_pool_end)
    return;

  if ((lock= lo->waiting_lock))
  {
    pthread_mutex_lock(& lock->table->mutex);

    if (lock->prev)
      lock->prev->next= lock->next;
    if (lock->next)
      lock->next->prev= lock->prev;
    if (lock->table->wait_queue_in == lock)
      lock->table->wait_queue_in= lock->prev;
    if (lock->table->wait_queue_out == lock)
      lock->table->wait_queue_out= lock->next;
    pthread_mutex_unlock(& lock->table->mutex);

    lock->next= local_pool;
    local_pool= lock;
    DBUG_ASSERT(lock->loid == lo->loid);
  }

  lock= lo->active_locks;
  while (lock)
  {
    TABLE_LOCK *cur= lock;
    pthread_mutex_t *mutex= & lock->table->mutex;

    lock= lock->next_in_lo;

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
    DBUG_ASSERT(cur->loid == lo->loid);

    pthread_mutex_unlock(mutex);
  }

  lo->waiting_lock= lo->active_locks= 0;

  pthread_mutex_lock(lo->mutex);
  pthread_cond_broadcast(lo->cond);
  pthread_mutex_unlock(lo->mutex);

  pthread_mutex_lock(& lm->pool_mutex);
  local_pool_end->next= lm->pool;
  lm->pool= local_pool;
  pthread_mutex_unlock(& lm->pool_mutex);
}

void tablockman_init(TABLOCKMAN *lm, loid_to_tlo_func *func, uint timeout)
{
  lm->pool= 0;
  lm->loid_to_lo= func;
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

void tablockman_init_locked_table(LOCKED_TABLE *lt)
{
  TABLE_LOCK *unused;
  bzero(lt, sizeof(*lt));
  pthread_mutex_init(& lt->mutex, MY_MUTEX_INIT_FAST);
  hash_init(& lt->active, &my_charset_bin, 10/*FIXME*/,
            offsetof(TABLE_LOCK, loid), sizeof(unused->loid), 0, 0, 0);
}

void tablockman_destroy_locked_table(LOCKED_TABLE *lt)
{
  hash_free(& lt->active);
  pthread_mutex_destroy(& lt->mutex);
}

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

