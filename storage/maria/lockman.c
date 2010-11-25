/* QQ: TODO - allocate everything from dynarrays !!! (benchmark) */
/* QQ: TODO instant duration locks */
/* QQ: #warning automatically place S instead of LS if possible */

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

/*
  Generic Lock Manager

  Lock manager handles locks on "resources", a resource must be uniquely
  identified by a 64-bit number. Lock manager itself does not imply
  anything about the nature of a resource - it can be a row, a table, a
  database, or just anything.

  Locks belong to "lock owners". A Lock owner is uniquely identified by a
  16-bit number. A function loid2lo must be provided by the application
  that takes such a number as an argument and returns a LOCK_OWNER
  structure.

  Lock levels are completely defined by three tables. Lock compatibility
  matrix specifies which locks can be held at the same time on a resource.
  Lock combining matrix specifies what lock level has the same behaviour as
  a pair of two locks of given levels. getlock_result matrix simplifies
  intention locking and lock escalation for an application, basically it
  defines which locks are intention locks and which locks are "loose"
  locks.  It is only used to provide better diagnostics for the
  application, lock manager itself does not differentiate between normal,
  intention, and loose locks.

  Internally lock manager is based on a lock-free hash, see lf_hash.c for
  details.  All locks are stored in a hash, with a resource id as a search
  key, so all locks for the same resource will be considered collisions and
  will be put in a one (lock-free) linked list.  The main lock-handling
  logic is in the inner loop that searches for a lock in such a linked
  list - lockfind().

  This works as follows. Locks generally are added to the end of the list
  (with one exception, see below). When scanning the list it is always
  possible to determine what locks are granted (active) and what locks are
  waiting - first lock is obviously active, the second is active if it's
  compatible with the first, and so on, a lock is active if it's compatible
  with all previous locks and all locks before it are also active.
  To calculate the "compatible with all previous locks" all locks are
  accumulated in prev_lock variable using lock_combining_matrix.

  Lock upgrades: when a thread that has a lock on a given resource,
  requests a new lock on the same resource and the old lock is not enough
  to satisfy new lock requirements (which is defined by
  lock_combining_matrix[old_lock][new_lock] != old_lock), a new lock is
  placed in the list. Depending on other locks it is immediately active or
  it will wait for other locks. Here's an exception to "locks are added
  to the end" rule - upgraded locks are added after the last active lock
  but before all waiting locks. Old lock (the one we upgraded from) is
  not removed from the list, indeed it may be needed if the new lock was
  in a savepoint that gets rolled back. So old lock is marked as "ignored"
  (IGNORE_ME flag). New lock gets an UPGRADED flag.

  Loose locks add an important exception to the above. Loose locks do not
  always commute with other locks. In the list IX-LS both locks are active,
  while in the LS-IX list only the first lock is active. This creates a
  problem in lock upgrades. If the list was IX-LS and the owner of the
  first lock wants to place LS lock (which can be immediately granted), the
  IX lock is upgraded to LSIX and the list becomes IX-LS-LSIX, which,
  according to the lock compatibility matrix means that the last lock is
  waiting - of course it all happened because IX and LS were swapped and
  they don't commute. To work around this there's ACTIVE flag which is set
  in every lock that never waited (was placed active), and this flag
  overrides "compatible with all previous locks" rule.

  When a lock is placed to the end of the list it's either compatible with
  all locks and all locks are active - new lock becomes active at once, or
  it conflicts with some of the locks, in this case in the 'blocker'
  variable a conflicting lock is returned and the calling thread waits on a
  pthread condition in the LOCK_OWNER structure of the owner of the
  conflicting lock. Or a new lock is compatible with all locks, but some
  existing locks are not compatible with each other (example: request IS,
  when the list is S-IX) - that is not all locks are active. In this case a
  first waiting lock is returned in the 'blocker' variable, lockman_getlock()
  notices that a "blocker" does not conflict with the requested lock, and
  "dereferences" it, to find the lock that it's waiting on.  The calling
  thread than begins to wait on the same lock.

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
*/

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
static enum lockman_lock_type lock_combining_matrix[10][10]=
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

LF_REQUIRE_PINS(4)

typedef struct lockman_lock {
  uint64 resource;
  struct lockman_lock  *lonext;
  intptr volatile link;
  uint32 hashnr;
  /* QQ: TODO - remove hashnr from LOCK */
  uint16 loid;
  uchar lock;              /* sizeof(uchar) <= sizeof(enum) */
  uchar flags;
} LOCK;

#define IGNORE_ME               1
#define UPGRADED                2
#define ACTIVE                  4

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
  intptr        cur_link;
  my_bool       cur_active, compatible, upgrading, prev_active;
  enum lockman_lock_type lock, prev_lock, cur_lock;
  uint16        loid, cur_loid;
  int           cur_flags, flags;

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
  do {
    cursor->curr= PTR(*cursor->prev);
    _lf_pin(pins, 1, cursor->curr);
  } while(*cursor->prev != (intptr)cursor->curr && LF_BACKOFF);
  for (;;)
  {
    if (!cursor->curr)
      break;
    do {
      cur_link= cursor->curr->link;
      cursor->next= PTR(cur_link);
      _lf_pin(pins, 0, cursor->next);
    } while (cur_link != cursor->curr->link && LF_BACKOFF);
    cur_hashnr= cursor->curr->hashnr;
    cur_resource= cursor->curr->resource;
    cur_lock= cursor->curr->lock;
    cur_loid= cursor->curr->loid;
    cur_flags= cursor->curr->flags;
    if (*cursor->prev != (intptr)cursor->curr)
    {
      (void)LF_BACKOFF;
      goto retry;
    }
    if (!DELETED(cur_link))
    {
      if (cur_hashnr > hashnr ||
          (cur_hashnr == hashnr && cur_resource >= resource))
      {
        if (cur_hashnr > hashnr || cur_resource > resource)
          break;
        /* ok, we have a lock for this resource */
        DBUG_ASSERT(lock_compatibility_matrix[prev_lock][cur_lock] >= 0);
        DBUG_ASSERT(lock_compatibility_matrix[cur_lock][lock] >= 0);
        if ((cur_flags & IGNORE_ME) && ! (flags & IGNORE_ME))
        {
          DBUG_ASSERT(cur_active);
          if (cur_loid == loid)
            cursor->upgrade_from= cursor->curr;
        }
        else
        {
          prev_active= cur_active;
          if (cur_flags & ACTIVE)
            DBUG_ASSERT(prev_active == TRUE);
          else
            cur_active&= lock_compatibility_matrix[prev_lock][cur_lock];
          if (upgrading && !cur_active /*&& !(cur_flags & UPGRADED)*/)
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
            DBUG_ASSERT(!upgrading || (flags & IGNORE_ME));
            if (lock_combining_matrix[cur_lock][lock] == cur_lock)
            {
              /* new lock is compatible */
              if (cur_active)
              {
                cursor->blocker= cursor->curr;  /* loose-locks! */
                _lf_unpin(pins, 3);             /* loose-locks! */
                return ALREADY_HAVE_THE_LOCK;
              }
              else
                return ALREADY_HAVE_THE_REQUEST;
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
          }
          prev_lock= lock_combining_matrix[prev_lock][cur_lock];
          DBUG_ASSERT(prev_lock != N);
        }
      }
      cursor->prev= &(cursor->curr->link);
      _lf_pin(pins, 2, cursor->curr);
    }
    else
    {
      if (my_atomic_casptr((void **)cursor->prev,
                           (void **)(char*) &cursor->curr, cursor->next))
        _lf_alloc_free(pins, cursor->curr);
      else
      {
        (void)LF_BACKOFF;
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
    if (compatible /*&& prev_active*/)
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
      if (!(res & NEED_TO_WAIT))
        node->flags|= ACTIVE;
      node->link= (intptr)cursor.curr;
      DBUG_ASSERT(node->link != (intptr)node);
      DBUG_ASSERT(cursor.prev != &node->link);
      if (!my_atomic_casptr((void **)cursor.prev,
                            (void **)(char*) &cursor.curr, node))
      {
        res= REPEAT_ONCE_MORE;
        node->flags&= ~ACTIVE;
      }
      if (res & LOCK_UPGRADE)
        cursor.upgrade_from->flags|= IGNORE_ME;
      /*
        QQ: is this OK ? if a reader has already read upgrade_from,
        it may find it conflicting with node :(
        - see the last test from test_lockman_simple()
      */
    }

  } while (res == REPEAT_ONCE_MORE);
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  /*
    note that blocker is not necessarily pinned here (when it's == curr).
    this is ok as in such a case it's either a dummy node for
    initialize_bucket() and dummy nodes don't need pinning,
    or it's a lock of the same transaction for lockman_getlock,
    and it cannot be removed by another thread
  */
  *blocker= cursor.blocker;
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

    /*
      XXX this does not work with savepoints, as old lock is left ignored.
      It cannot be unignored, as would basically mean moving the lock back
      in the lock chain (from upgraded). And the latter is not allowed -
      because it breaks list scanning. So old ignored lock must be deleted,
      new - same - lock must be installed right after the lock we're deleting,
      then we can delete. Good news is - this is only required when rolling
      back a savepoint.
    */
    if (my_atomic_casptr((void **)(char*)&(cursor.curr->link),
                         (void **)(char*)&cursor.next, 1+(char *)cursor.next))
    {
      if (my_atomic_casptr((void **)cursor.prev,
                           (void **)(char*)&cursor.curr, cursor.next))
        _lf_alloc_free(pins, cursor.curr);
      else
        lockfind(head, node, &cursor, pins);
    }
    else
    {
      res= REPEAT_ONCE_MORE;
      if (cursor.upgrade_from)
        cursor.upgrade_from->flags|= IGNORE_ME;
    }
  } while (res == REPEAT_ONCE_MORE);
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  _lf_unpin(pins, 3);
  return res;
}

void lockman_init(LOCKMAN *lm, loid_to_lo_func *func, uint timeout)
{
  lf_alloc_init(&lm->alloc, sizeof(LOCK), offsetof(LOCK, lonext));
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
      lf_alloc_direct_free(&lm->alloc, el);
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
  my_atomic_casptr((void **)node, (void **)(char*) &tmp, dummy);
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
                                            enum lockman_lock_type lock)
{
  int res;
  uint csize, bucket, hashnr;
  LOCK *node, * volatile *el, *blocker;
  LF_PINS *pins= lo->pins;
  enum lockman_lock_type old_lock;

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
    int r;
    old_lock= blocker->lock;
    _lf_alloc_free(pins, node);
    lf_rwunlock_by_pins(pins);
    r= getlock_result[old_lock][lock];
    DBUG_ASSERT(r);
    return r;
  }
  /* a new value was added to the hash */
  csize= lm->size;
  if ((my_atomic_add32(&lm->count, 1)+1.0) / csize > MAX_LOAD)
    my_atomic_cas32(&lm->size, (int*) &csize, csize*2);
  node->lonext= lo->all_locks;
  lo->all_locks= node;
  for ( ; res & NEED_TO_WAIT; res= lockpeek(el, node, pins, &blocker))
  {
    LOCK_OWNER *wait_for_lo;
    ulonglong deadline;
    struct timespec timeout;

    _lf_assert_pin(pins, 3); /* blocker must be pinned here */
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
      _lf_pin(pins, 3, blocker);
      if (blocker != wait_for_lo->all_locks)
        continue;
      wait_for_lo= wait_for_lo->waiting_for;
    }

    /*
      note that the blocker transaction may have ended by now,
      its LOCK_OWNER and short id were reused, so 'wait_for_lo' may point
      to an unrelated - albeit valid - LOCK_OWNER
    */
    if (!wait_for_lo)
      continue;

    lo->waiting_for= wait_for_lo;
    lf_rwunlock_by_pins(pins);

    /*
      We lock a mutex - it may belong to a wrong LOCK_OWNER, but it must
      belong to _some_ LOCK_OWNER. It means, we can never free() a LOCK_OWNER,
      if there're other active LOCK_OWNERs.
    */
    /* QQ: race condition here */
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
    deadline= my_getsystime() + lm->lock_timeout * 10000;
    set_timespec_nsec(timeout,lm->lock_timeout * 1000000);
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
      _lf_unpin(pins, 3);
      lf_rwunlock_by_pins(pins);
      return DIDNT_GET_THE_LOCK;
    }
  }
  lo->waiting_for= 0;
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
  LOCK * volatile *el, *node, *next;
  uint bucket;
  LF_PINS *pins= lo->pins;

  pthread_mutex_lock(lo->mutex);
  lf_rwlock_by_pins(pins);
  for (node= lo->all_locks; node; node= next)
  {
    next= node->lonext;
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
static const char *lock2str[]=
{ "N", "S", "X", "IS", "IX", "SIX", "LS", "LX", "SLX", "LSIX" };
/*
  NOTE
    the function below is NOT thread-safe !!!
*/
void print_lockhash(LOCKMAN *lm)
{
  LOCK *el= *(LOCK **)_lf_dynarray_lvalue(&lm->array, 0);
  printf("hash: size %u count %u\n", lm->size, lm->count);
  while (el)
  {
    intptr next= el->link;
    if (el->hashnr & 1)
    {
      printf("0x%08lx { resource %lu, loid %u, lock %s",
             (long) el->hashnr, (ulong) el->resource, el->loid,
             lock2str[el->lock]);
      if (el->flags & IGNORE_ME) printf(" IGNORE_ME");
      if (el->flags & UPGRADED) printf(" UPGRADED");
      if (el->flags & ACTIVE) printf(" ACTIVE");
      if (DELETED(next)) printf(" ***DELETED***");
      printf("}\n");
    }
    else
    {
      /*printf("0x%08x { dummy }\n", el->hashnr);*/
      DBUG_ASSERT(el->resource == 0 && el->loid == 0 && el->lock == X);
    }
    el= PTR(next);
  }
}
#endif
