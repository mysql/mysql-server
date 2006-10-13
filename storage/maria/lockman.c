// TODO lock escalation, instant duration locks
// automatically place S instead of LS if possible
/*
  TODO optimization: table locks - they have completely
  different characteristics. long lists, few distinct resources -
  slow to scan, [possibly] high retry rate
*/
/* Copyright (C) 2000 MySQL AB

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
#include "lockman.h"

/*
  Lock compatibility matrix.

  It's asymmetric. Read it as "Somebody has the lock <value in the row
  label>, can I set the lock <value in the column label> ?"

  ') Though you can take LS lock while somebody has S lock, it makes no
  sense - it's simpler to take S lock too.

  ") Strictly speaking you can take LX lock while somebody has S lock.
  But in this case you lock no rows, because all rows are locked by this
  somebody. So we prefer to define that LX cannot be taken when S
  exists. Same about LX and X.

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

#define REPEAT_ONCE_MORE                0
#define OK_TO_PLACE_THE_LOCK            1
#define OK_TO_PLACE_THE_REQUEST         2
#define ALREADY_HAVE_THE_LOCK           4
#define ALREADY_HAVE_THE_REQUEST        8
#define PLACE_NEW_DISABLE_OLD          16
#define REQUEST_NEW_DISABLE_OLD        32
#define RESOURCE_WAS_UNLOCKED          64

#define NEED_TO_WAIT (OK_TO_PLACE_THE_REQUEST | ALREADY_HAVE_THE_REQUEST |\
                      REQUEST_NEW_DISABLE_OLD)
#define ALREADY_HAVE (ALREADY_HAVE_THE_LOCK   | ALREADY_HAVE_THE_REQUEST)
#define LOCK_UPGRADE (PLACE_NEW_DISABLE_OLD   | REQUEST_NEW_DISABLE_OLD)


/*
  the return codes for lockman_getlock

  It's asymmetric. Read it as "I have the lock <value in the row label>,
  what value should be returned for <value in the column label> ?"

  0 means impossible combination (assert!)

  Defines below help to preserve the table structure.
  I/L/A values are self explanatory
  x means the combination is possible (assert should not crash)
    but cannot happen in row locks, only in table locks (S,X), or
    lock escalations (LS,LX)
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

LF_REQUIRE_PINS(4);

typedef struct lockman_lock {
  uint64 resource;
  struct lockman_lock  *lonext;
  intptr volatile link;
  uint32 hashnr;
//#warning TODO - remove hashnr from LOCK
  uint16 loid;
  uchar lock;              /* sizeof(uchar) <= sizeof(enum) */
  uchar flags;
} LOCK;

#define IGNORE_ME               1
#define UPGRADED                2

typedef struct {
  intptr volatile *prev;
  LOCK *curr, *next;
  LOCK *blocker, *upgrade_from;
} CURSOR;

#define PTR(V)      (LOCK *)((V) & (~(intptr)1))
#define DELETED(V)  ((V) & 1)

/*
  NOTE
    cursor is positioned in either case
    pins[0..3] are used, they are NOT removed on return
*/
static int lockfind(LOCK * volatile *head, LOCK *node,
                    CURSOR *cursor, LF_PINS *pins)
{
  uint32        hashnr, cur_hashnr;
  uint64        resource, cur_resource;
  intptr        link;
  my_bool       cur_active, compatible, upgrading, prev_active;
  enum lock_type lock, prev_lock, cur_lock;
  uint16        loid, cur_loid;
  int           upgraded_pairs, cur_flags, flags;

  hashnr= node->hashnr;
  resource= node->resource;
  lock= node->lock;
  loid= node->loid;
  flags= node->flags;

retry:
  cursor->prev= (intptr *)head;
  prev_lock= N;
  cur_active= TRUE;
  compatible= TRUE;
  upgrading= FALSE;
  cursor->blocker= cursor->upgrade_from= 0;
  _lf_unpin(pins, 3);
  upgraded_pairs= 0;
  do {
    cursor->curr= PTR(*cursor->prev);
    _lf_pin(pins,1,cursor->curr);
  } while(*cursor->prev != (intptr)cursor->curr && LF_BACKOFF);
  for (;;)
  {
    if (!cursor->curr)
      break;
    do {
      link= cursor->curr->link;
      cursor->next= PTR(link);
      _lf_pin(pins, 0, cursor->next);
    } while(link != cursor->curr->link && LF_BACKOFF);
    cur_hashnr= cursor->curr->hashnr;
    cur_resource= cursor->curr->resource;
    cur_lock= cursor->curr->lock;
    cur_loid= cursor->curr->loid;
    cur_flags= cursor->curr->flags;
    if (*cursor->prev != (intptr)cursor->curr)
    {
      LF_BACKOFF;
      goto retry;
    }
    if (!DELETED(link))
    {
      if (cur_hashnr > hashnr ||
          (cur_hashnr == hashnr && cur_resource >= resource))
      {
        if (cur_hashnr > hashnr || cur_resource > resource)
        {
          if (upgraded_pairs != 0)
            goto retry;
          break;
        }
        /* ok, we have a lock for this resource */
        DBUG_ASSERT(lock_compatibility_matrix[prev_lock][cur_lock] >= 0);
        DBUG_ASSERT(lock_compatibility_matrix[cur_lock][lock] >= 0);
        if (cur_flags & UPGRADED)
          upgraded_pairs++;
        if ((cur_flags & IGNORE_ME) && ! (flags & IGNORE_ME))
        {
          DBUG_ASSERT(cur_active);
          upgraded_pairs--;
          if (cur_loid == loid)
            cursor->upgrade_from= cursor->curr;
        }
        else
        {
          prev_active= cur_active;
          cur_active&= lock_compatibility_matrix[prev_lock][cur_lock];
          if (upgrading && !cur_active && upgraded_pairs == 0)
            break;
          if (prev_active && !cur_active)
          {
            cursor->blocker= cursor->curr;
            _lf_pin(pins, 3, cursor->curr);
          }
          if (cur_loid == loid)
          {
            /* we already have a lock on this resource */
            DBUG_ASSERT(lock_combining_matrix[cur_lock][lock] != N);
            DBUG_ASSERT(!upgrading); /* can happen only once */
            if (lock_combining_matrix[cur_lock][lock] == cur_lock)
            {
              /* new lock is compatible */
              return cur_active ? ALREADY_HAVE_THE_LOCK
                                : ALREADY_HAVE_THE_REQUEST;
            }
            /* not compatible, upgrading */
            upgrading= TRUE;
            cursor->upgrade_from= cursor->curr;
          }
          else
          {
            if (!lock_compatibility_matrix[cur_lock][lock])
            {
              compatible= FALSE;
              cursor->blocker= cursor->curr;
              _lf_pin(pins, 3, cursor->curr);
            }
            prev_lock= lock_combining_matrix[prev_lock][cur_lock];
            DBUG_ASSERT(prev_lock != N);
          }
        }
      }
      cursor->prev= &(cursor->curr->link);
      _lf_pin(pins, 2, cursor->curr);
    }
    else
    {
      if (my_atomic_casptr((void **)cursor->prev,
                           (void **)&cursor->curr, cursor->next))
        _lf_alloc_free(pins, cursor->curr);
      else
      {
        LF_BACKOFF;
        goto retry;
      }
    }
    cursor->curr= cursor->next;
    _lf_pin(pins, 1, cursor->curr);
  }
  /*
    either the end of lock list - no more locks for this resource,
    or upgrading and the end of active lock list
  */
  if (upgrading)
  {
    if (compatible)
      return PLACE_NEW_DISABLE_OLD;
    else
      return REQUEST_NEW_DISABLE_OLD;
  }
  if (cur_active && compatible)
  {
    /*
      either no locks for this resource or all are compatible.
      ok to place the lock in any case.
    */
    return prev_lock == N ? RESOURCE_WAS_UNLOCKED
                          : OK_TO_PLACE_THE_LOCK;
  }
  /* we have a lock conflict. ok to place a lock request. And wait */
  return OK_TO_PLACE_THE_REQUEST;
}

/*
  NOTE
    it uses pins[0..3], on return pins 0..2 are removed, pin 3 (blocker) stays
*/
static int lockinsert(LOCK * volatile *head, LOCK *node, LF_PINS *pins,
                      LOCK **blocker)
{
  CURSOR         cursor;
  int            res;

  do
  {
    res= lockfind(head, node, &cursor, pins);
    DBUG_ASSERT(res != ALREADY_HAVE_THE_REQUEST);
    if (!(res & ALREADY_HAVE))
    {
      if (res & LOCK_UPGRADE)
      {
        node->flags|= UPGRADED;
        node->lock= lock_combining_matrix[cursor.upgrade_from->lock][node->lock];
      }
      node->link= (intptr)cursor.curr;
      DBUG_ASSERT(node->link != (intptr)node);
      DBUG_ASSERT(cursor.prev != &node->link);
      if (!my_atomic_casptr((void **)cursor.prev, (void **)&cursor.curr, node))
        res= REPEAT_ONCE_MORE;
      if (res & LOCK_UPGRADE)
        cursor.upgrade_from->flags|= IGNORE_ME;
    }

  } while (res == REPEAT_ONCE_MORE);
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  /*
    note that cursor.curr is NOT pinned on return.
    this is ok as it's either a dummy node for initialize_bucket
    and dummy nodes don't need pinning,
    or it's a lock of the same transaction for lockman_getlock,
    and it cannot be removed by another thread
  */
  *blocker= cursor.blocker ? cursor.blocker : cursor.curr;
  return res;
}

/*
  NOTE
    it uses pins[0..3], on return pins 0..2 are removed, pin 3 (blocker) stays
*/
static int lockpeek(LOCK * volatile *head, LOCK *node, LF_PINS *pins,
                    LOCK **blocker)
{
  CURSOR         cursor;
  int            res;

  res= lockfind(head, node, &cursor, pins);

  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  if (blocker)
    *blocker= cursor.blocker;
  return res;
}

/*
  NOTE
    it uses pins[0..3], on return all pins are removed.

    One _must_ have the lock (or request) to call this
*/
static int lockdelete(LOCK * volatile *head, LOCK *node, LF_PINS *pins)
{
  CURSOR cursor;
  int res;

  do
  {
    res= lockfind(head, node, &cursor, pins);
    DBUG_ASSERT(res & ALREADY_HAVE);

    if (cursor.upgrade_from)
      cursor.upgrade_from->flags&= ~IGNORE_ME;

    if (my_atomic_casptr((void **)&(cursor.curr->link),
                         (void **)&cursor.next, 1+(char *)cursor.next))
    {
      if (my_atomic_casptr((void **)cursor.prev,
                           (void **)&cursor.curr, cursor.next))
        _lf_alloc_free(pins, cursor.curr);
      else
        lockfind(head, node, &cursor, pins);
    }
    else
      res= REPEAT_ONCE_MORE;
  } while (res == REPEAT_ONCE_MORE);
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  _lf_unpin(pins, 3);
  return res;
}

void lockman_init(LOCKMAN *lm, loid_to_lo_func *func, uint timeout)
{
  lf_alloc_init(&lm->alloc,sizeof(LOCK));
  lf_dynarray_init(&lm->array, sizeof(LOCK **));
  lm->size= 1;
  lm->count= 0;
  lm->loid_to_lo= func;
  lm->lock_timeout= timeout;
}

void lockman_destroy(LOCKMAN *lm)
{
  LOCK *el= *(LOCK **)_lf_dynarray_lvalue(&lm->array, 0);
  while (el)
  {
    intptr next= el->link;
    if (el->hashnr & 1)
      lf_alloc_real_free(&lm->alloc, el);
    else
      my_free((void *)el, MYF(0));
    el= (LOCK *)next;
  }
  lf_alloc_destroy(&lm->alloc);
  lf_dynarray_destroy(&lm->array);
}

/* TODO: optimize it */
#define MAX_LOAD 1

static void initialize_bucket(LOCKMAN *lm, LOCK * volatile *node,
                              uint bucket, LF_PINS *pins)
{
  int res;
  uint parent= my_clear_highest_bit(bucket);
  LOCK *dummy= (LOCK *)my_malloc(sizeof(LOCK), MYF(MY_WME));
  LOCK **tmp= 0, *cur;
  LOCK * volatile *el= _lf_dynarray_lvalue(&lm->array, parent);

  if (*el == NULL && bucket)
    initialize_bucket(lm, el, parent, pins);
  dummy->hashnr= my_reverse_bits(bucket);
  dummy->loid= 0;
  dummy->lock= X; /* doesn't matter, in fact */
  dummy->resource= 0;
  dummy->flags= 0;
  res= lockinsert(el, dummy, pins, &cur);
  DBUG_ASSERT(res & (ALREADY_HAVE_THE_LOCK | RESOURCE_WAS_UNLOCKED));
  if (res & ALREADY_HAVE_THE_LOCK)
  {
    my_free((void *)dummy, MYF(0));
    dummy= cur;
  }
  my_atomic_casptr((void **)node, (void **)&tmp, dummy);
}

static inline uint calc_hash(uint64 resource)
{
  const uchar *pos= (uchar *)&resource;
  ulong nr1= 1, nr2= 4, i;
  for (i= 0; i < sizeof(resource) ; i++, pos++)
  {
    nr1^= (ulong) ((((uint) nr1 & 63)+nr2) * ((uint)*pos)) + (nr1 << 8);
    nr2+= 3;
  }
  return nr1 & INT_MAX32;
}

/*
  RETURN
    see enum lockman_getlock_result
  NOTE
    uses pins[0..3], they're removed on return
*/
enum lockman_getlock_result lockman_getlock(LOCKMAN *lm, LOCK_OWNER *lo,
                                            uint64 resource,
                                            enum lock_type lock)
{
  int res;
  uint csize, bucket, hashnr;
  LOCK *node, * volatile *el, *blocker;
  LF_PINS *pins= lo->pins;
  enum lock_type old_lock;

  DBUG_ASSERT(lo->loid);
  lf_rwlock_by_pins(pins);
  node= (LOCK *)_lf_alloc_new(pins);
  node->flags= 0;
  node->lock= lock;
  node->loid= lo->loid;
  node->resource= resource;
  hashnr= calc_hash(resource);
  bucket= hashnr % lm->size;
  el= _lf_dynarray_lvalue(&lm->array, bucket);
  if (*el == NULL)
    initialize_bucket(lm, el, bucket, pins);
  node->hashnr= my_reverse_bits(hashnr) | 1;
  res= lockinsert(el, node, pins, &blocker);
  if (res & ALREADY_HAVE)
  {
    old_lock= blocker->lock;
    _lf_assert_unpin(pins, 3); /* unpin should not be needed */
    _lf_alloc_free(pins, node);
    lf_rwunlock_by_pins(pins);
    res= getlock_result[old_lock][lock];
    DBUG_ASSERT(res);
    return res;
  }
  /* a new value was added to the hash */
  csize= lm->size;
  if ((my_atomic_add32(&lm->count, 1)+1.0) / csize > MAX_LOAD)
    my_atomic_cas32(&lm->size, &csize, csize*2);
  node->lonext= lo->all_locks;
  lo->all_locks= node;
  for ( ; res & NEED_TO_WAIT; res= lockpeek(el, node, pins, &blocker))
  {
    LOCK_OWNER *wait_for_lo;
    ulonglong deadline;
    struct timespec timeout;

    _lf_assert_pin(pins, 3); /* blocker must be pinned here */
    lf_rwunlock_by_pins(pins);

    wait_for_lo= lm->loid_to_lo(blocker->loid);
    /*
      now, this is tricky. blocker is not necessarily a LOCK
      we're waiting for. If it's compatible with what we want,
      then we're waiting for a lock that blocker is waiting for
      (see two places where blocker is set in lockfind)
      In the latter case, let's "dereference" it
    */
    if (lock_compatibility_matrix[blocker->lock][lock])
    {
      blocker= wait_for_lo->all_locks;
      lf_pin(pins, 3, blocker);
      if (blocker != wait_for_lo->all_locks)
      {
        lf_rwlock_by_pins(pins);
        continue;
      }
      wait_for_lo= wait_for_lo->waiting_for;
    }

    /*
      note that the blocker transaction may have ended by now,
      its LOCK_OWNER and short id were reused, so 'wait_for_lo' may point
      to an unrelated - albeit valid - LOCK_OWNER
    */
    if (!wait_for_lo)
    {
      /* blocker transaction has ended, short id was released */
      lf_rwlock_by_pins(pins);
      continue;
    }
    /*
      We lock a mutex - it may belong to a wrong LOCK_OWNER, but it must
      belong to _some_ LOCK_OWNER. It means, we can never free() a LOCK_OWNER,
      if there're other active LOCK_OWNERs.
    */
#warning race condition here
    pthread_mutex_lock(wait_for_lo->mutex);
    if (DELETED(blocker->link))
    {
      /*
        blocker transaction was ended, or a savepoint that owned
        the lock was rolled back. Either way - the lock was removed
      */
      pthread_mutex_unlock(wait_for_lo->mutex);
      lf_rwlock_by_pins(pins);
      continue;
    }
    /* yuck. waiting */
    lo->waiting_for= wait_for_lo;

    deadline= my_getsystime() + lm->lock_timeout * 10000;
    timeout.tv_sec= deadline/10000000;
    timeout.tv_nsec= (deadline % 10000000) * 100;
    do
    {
      pthread_cond_timedwait(wait_for_lo->cond, wait_for_lo->mutex, &timeout);
    } while (!DELETED(blocker->link) && my_getsystime() < deadline);
    pthread_mutex_unlock(wait_for_lo->mutex);
    lf_rwlock_by_pins(pins);
    if (!DELETED(blocker->link))
    {
      /*
        timeout.
        note that we _don't_ release the lock request here.
        Instead we're relying on the caller to abort the transaction,
        and release all locks at once - see lockman_release_locks()
      */
      lf_rwunlock_by_pins(pins);
      return DIDNT_GET_THE_LOCK;
    }
    lo->waiting_for= 0;
  }
  _lf_assert_unpin(pins, 3); /* unpin should not be needed */
  lf_rwunlock_by_pins(pins);
  return getlock_result[lock][lock];
}

/*
  RETURN
    0 - deleted
    1 - didn't (not found)
  NOTE
    see lockdelete() for pin usage notes
*/
int lockman_release_locks(LOCKMAN *lm, LOCK_OWNER *lo)
{
  LOCK * volatile *el, *node;
  uint bucket;
  LF_PINS *pins= lo->pins;

  pthread_mutex_lock(lo->mutex);
  lf_rwlock_by_pins(pins);
  for (node= lo->all_locks; node; node= node->lonext)
  {
    bucket= calc_hash(node->resource) % lm->size;
    el= _lf_dynarray_lvalue(&lm->array, bucket);
    if (*el == NULL)
      initialize_bucket(lm, el, bucket, pins);
    lockdelete(el, node, pins);
    my_atomic_add32(&lm->count, -1);
  }
  lf_rwunlock_by_pins(pins);
  lo->all_locks= 0;
  /* now signal all waiters */
  pthread_cond_broadcast(lo->cond);
  pthread_mutex_unlock(lo->mutex);
  return 0;
}

#ifdef MY_LF_EXTRA_DEBUG
/*
  NOTE
    the function below is NOT thread-safe !!!
*/
static char *lock2str[]=
{ "N", "S", "X", "IS", "IX", "SIX", "LS", "LX", "SLX", "LSIX" };
void print_lockhash(LOCKMAN *lm)
{
  LOCK *el= *(LOCK **)_lf_dynarray_lvalue(&lm->array, 0);
  printf("hash: size=%u count=%u\n", lm->size, lm->count);
  while (el)
  {
    intptr next= el->link;
    if (el->hashnr & 1)
      printf("0x%08x { resource %llu, loid %u, lock %s",
             el->hashnr, el->resource, el->loid, lock2str[el->lock]);
    else
    {
      printf("0x%08x { dummy ", el->hashnr);
      DBUG_ASSERT(el->resource == 0 && el->loid == 0 && el->lock == X);
    }
    if (el->flags & IGNORE_ME) printf(" IGNORE_ME");
    if (el->flags & UPGRADED) printf(" UPGRADED");
    printf("}\n");
    el= (LOCK *)next;
  }
}
#endif

