/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  "waiting threads" subsystem - a unified interface for threads to wait
  on each other, with built-in deadlock detection.

  Main concepts
  ^^^^^^^^^^^^^
  a thread - is represented by a WT_THD structure. One physical thread
    can have only one WT_THD descriptor at any given moment.

  a resource - a thread does not wait for other threads directly,
    instead it waits for a "resource", which is "owned" by other threads.
    It waits, exactly, for all "owners" to "release" a resource.
    It does not have to correspond to a physical resource. For example, it
    may be convenient in certain cases to force resource == thread.
    A resource is represented by a WT_RESOURCE structure. 

  a resource identifier - a pair of {resource type, value}. A value is
    an ulonglong number. Represented by a WT_RESOURCE_ID structure.

  a resource type - a pointer to a statically defined instance of
    WT_RESOURCE_TYPE structure. This structure contains a pointer to
    a function that knows how to compare values of this resource type.
    In the simple case it could be wt_resource_id_memcmp().

  a wait-for graph - a graph, that represenst "wait-for" relationships.
    It has two types of nodes - threads and resources. There are directed
    edges from a thread to a resource it is waiting for (WT_THD::waiting_for),
    from a thread to resources that it "owns" (WT_THD::my_resources),
    and from a resource to threads that "own" it (WT_RESOURCE::owners)

  Graph completeness
  ^^^^^^^^^^^^^^^^^^

  For flawless deadlock detection wait-for graph must be complete.
  It means that when a thread starts waiting it needs to know *all* its
  blockers, and call wt_thd_will_wait_for() for every one of them.
  Otherwise two phenomena should be expected:

  1. Fuzzy timeouts:

    thread A needs to get a lock, and is blocked by a thread B.
    it waits.
    Just before the timeout thread B releases the lock.
    thread A is ready to grab the lock but discovers that it is also
    blocked by a thread C.
    It waits and times out.

    As a result thread A has waited two timeout intervals, instead of one.

  2. Unreliable cycle detection:

     Thread A waits for threads B and C
     Thread C waits for D
     Thread D wants to start waiting for A

     one can see immediately that thread D creates a cycle, and thus
     a deadlock is detected.

     But if thread A would only wait for B, and start waiting for C
     when B would unlock, thread D would be allowed to wait, a deadlock
     would be only detected when B unlocks or somebody times out.

  These two phenomena don't affect a correctness, and strictly speaking,
  the caller is not required to call wt_thd_will_wait_for() for *all*
  blockers - it may optimize wt_thd_will_wait_for() calls. But they
  may be perceived as bugs by users, it must be understood that such
  an optimization comes with its price.

  Usage
  ^^^^^

  First, the wt* subsystem must be initialized by calling
  wt_init(). In the server you don't need to do it, it's done
  in mysqld.cc.

  Similarly, wt_end() frees wt* structures, should be called
  at the end, but in the server mysqld.cc takes care of that.

  Every WT_THD should be initialized with wt_thd_lazy_init().
  After that they can be used in other wt_thd_* calls.
  Before discarding, WT_THD should be free'd with
  wt_thd_destroy(). In the server both are handled in sql_class.cc,
  it's an error to try to do it manually.

  To use the deadlock detection one needs to use this thread's WT_THD,
  call wt_thd_will_wait_for() for every thread it needs to wait on,
  then call wt_thd_cond_timedwait(). When thread releases a resource
  it should call wt_thd_release() (or wt_thd_release_all()) - it will
  notify (send a signal) threads waiting in wt_thd_cond_timedwait(),
  if appropriate.

  Just like with pthread's cond_wait, there could be spurious
  wake-ups from wt_thd_cond_timedwait(). A caller is expected to
  handle that (that is, to re-check the blocking criteria).

  wt_thd_will_wait_for() and wt_thd_cond_timedwait() return either
  WT_OK or WT_DEADLOCK. Additionally wt_thd_cond_timedwait() can return
  WT_TIMEOUT. Out of memory and other fatal errors are reported as
  WT_DEADLOCK - and a transaction must be aborted just the same.

  Configuration
  ^^^^^^^^^^^^^
  There are four config variables. Two deadlock search depths - short and
  long - and two timeouts. Deadlock search is performed with the short
  depth on every wt_thd_will_wait_for() call. wt_thd_cond_timedwait()
  waits with a short timeout, performs a deadlock search with the long
  depth, and waits with a long timeout. As most deadlock cycles are supposed
  to be short, most deadlocks will be detected at once, and waits will
  rarely be necessary.

  These config variables are thread-local. Different threads may have
  different search depth and timeout values.

  Also, deadlock detector supports different killing strategies, the victim
  in a deadlock cycle is selected based on the "weight". See "weight"
  description in waiting_threads.h for details. It's up to the caller to
  set weights accordingly.

  Status
  ^^^^^^
  We calculate the number of successfull waits (WT_OK returned from
  wt_thd_cond_timedwait()), a number of timeouts, a deadlock cycle
  length distribution - number of deadlocks with every length from
  1 to WT_CYCLE_STATS, and a wait time distribution - number
  of waits with a time from 1 us to 1 min in WT_WAIT_STATS
  intervals on a log e scale.

  Sample usage as was done in the Maria engine
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  - in class THD, THD::transaction had a WT_THD object; there were session
  variables to set the short/long depth/timeout.
  - when mysqld started, wt_init() was called; when it ended, wt_end() was
  called.
  - in THD's constructor,
  wt_thd_lazy_init(&transaction.wt, &variables.wt_deadlock_search_depth_short,
    &variables.wt_timeout_short,
    &variables.wt_deadlock_search_depth_long,
    &variables.wt_timeout_long);
  - in THD::cleanup():
  wt_thd_destroy(&transaction.wt);
  - this was sufficient to make the deadlock-detector available to the Maria
  engine (which can grab THD); the engine used it this way:
  - when it wrote a row, and hit a duplicate key, it would find who wrote this
  key, the "blocker" transaction. If "blocker" had committed, duplicate key
  error would be sent. Otherwise, we would wait for it, in the following code
  snippet (originally from storage/maria/ma_write.c). After the blocker is
  gone, we would retry the write:

        while (keyinfo->ck_insert(info,
                 (*keyinfo->make_key)(info, &int_key, i, buff, record,
                                      filepos, info->trn->trid)))
        {
          / * we got a write error * /
          if error is not "duplicate key" then return error;
          info->dup_key_trid has the culprit:
          if the culprit is ourselves then return error;
          otherwise:
          blocker= trnman_trid_to_trn(info->trn, info->dup_key_trid);
          / *
            if blocker TRN was not found, it means that the conflicting
            transaction was committed long time ago. It could not be
            aborted, as it would have to wait on the key tree lock
            to remove the conflicting key it has inserted.
          * /
          if (!blocker || blocker->commit_trid != ~(TrID)0)
          { / * committed * /
            if (blocker)
              pthread_mutex_unlock(& blocker->state_lock);
            rw_unlock(&keyinfo->root_lock);
            goto err;
          }
          / * release root_lock to let blocker finish its work * /
          rw_unlock(&keyinfo->root_lock);
          {
            / * running. now we wait * /
            WT_RESOURCE_ID rc;
            int res;
            const char *old_proc_info;

            rc.type= &ma_rc_dup_unique;
            rc.value= (intptr)blocker;
            res= wt_thd_will_wait_for(info->trn->wt, blocker->wt, & rc);
            if (res != WT_OK)
            {
              pthread_mutex_unlock(& blocker->state_lock);
              my_errno= HA_ERR_LOCK_DEADLOCK;
              goto err;
            }
            old_proc_info= proc_info_hook(0,
                                          "waiting for a resource",
                                          __func__, __FILE__, __LINE__);
            res= wt_thd_cond_timedwait(info->trn->wt, & blocker->state_lock);
            proc_info_hook(0, old_proc_info, __func__, __FILE__, __LINE__);

            pthread_mutex_unlock(& blocker->state_lock);
            if (res != WT_OK)
            {
              my_errno= res == WT_TIMEOUT ? HA_ERR_LOCK_WAIT_TIMEOUT
                                          : HA_ERR_LOCK_DEADLOCK;
              goto err;
            }
            / * if we come here, blocker has rolled back or committed,
            so is gone, we can retry the ck_insert() * /
          }
          rw_wrlock(&keyinfo->root_lock);
#ifndef MARIA_CANNOT_ROLLBACK
          keyinfo->version++;
#endif
        }
  - ma_rc_dup_unique was:
  / * a WT_RESOURCE_TYPE for transactions waiting on a unique key conflict * /
  WT_RESOURCE_TYPE ma_rc_dup_unique={ wt_resource_id_memcmp, 0};
  - When a Maria transaction would commit or rollback it would call:
  / * Wake up threads waiting for this transaction * /
  static void wt_thd_release_self(TRN *trn)
  {
    if (trn->wt)
    {
      WT_RESOURCE_ID rc;
      rc.type= &ma_rc_dup_unique;
      rc.value= (intptr)trn;
      wt_thd_release(trn->wt, & rc);
      trn->wt= 0;
    }
  }

  Tests
  ^^^^^
  unittest/mysys/waiting_threads-t.c, currently disabled.
*/

/*
  Note that if your lock system satisfy the following condition:

    there exist four lock levels A, B, C, D, such as
      A is compatible with B
      A is not compatible with C
      D is not compatible with B

      (example A=IX, B=IS, C=S, D=X)

   you need to include lock level in the resource identifier - a
   thread waiting for lock of the type A on resource R and another
   thread waiting for lock of the type B on resource R should wait on
   different WT_RESOURCE structures, on different {lock, resource}
   pairs.  Otherwise the following is possible:

      thread1> take S-lock on R
      thread2> take IS-lock on R
      thread3> wants X-lock on R, starts waiting for threads 1 and 2 on R.
      thread3 is killed (or timeout or whatever)
      WT_RESOURCE structure for R is still in the hash, as it has two owners
      thread4> wants an IX-lock on R
      WT_RESOURCE for R is found in the hash, thread4 starts waiting on it.
      !! now thread4 is waiting for both thread1 and thread2
      !! while, in fact, IX-lock and IS-lock are compatible and
      !! thread4 should not wait for thread2.
*/

#include <waiting_threads.h>
#include <m_string.h>

/* status variables */

/**
  preset table of wait intervals
*/
ulonglong wt_wait_table[WT_WAIT_STATS];
/**
  wait time distribution (log e scale)
*/
uint32 wt_wait_stats[WT_WAIT_STATS+1];
/**
  distribution of cycle lengths
  first column tells whether this was during short or long detection
*/
uint32 wt_cycle_stats[2][WT_CYCLE_STATS+1];
uint32 wt_success_stats;

static my_atomic_rwlock_t cycle_stats_lock, wait_stats_lock, success_stats_lock;

#ifdef SAFE_STATISTICS
#define incr(VAR, LOCK)                           \
  do {                                            \
    my_atomic_rwlock_wrlock(&(LOCK));             \
    my_atomic_add32(&(VAR), 1);                   \
    my_atomic_rwlock_wrunlock(&(LOCK));           \
  } while(0)
#else
#define incr(VAR,LOCK)  do { (VAR)++; } while(0)
#endif

static void increment_success_stats()
{
  incr(wt_success_stats, success_stats_lock);
}

static void increment_cycle_stats(uint depth, uint slot)
{
  if (depth >= WT_CYCLE_STATS)
    depth= WT_CYCLE_STATS;
  incr(wt_cycle_stats[slot][depth], cycle_stats_lock);
}

static void increment_wait_stats(ulonglong waited,int ret)
{
  uint i;
  if ((ret) == ETIMEDOUT)
    i= WT_WAIT_STATS;
  else
    for (i= 0; i < WT_WAIT_STATS && waited/10 > wt_wait_table[i]; i++) ;
  incr(wt_wait_stats[i], wait_stats_lock);
}

/*
  'lock' protects 'owners', 'state', and 'waiter_count'
  'id' is read-only

  a resource is picked up from a hash in a lock-free manner
  it's returned pinned, so it cannot be freed at once
  but it may be freed right after the pin is removed
  to free a resource it should
    1. have no owners
    2. have no waiters

  two ways to access a resource:
    1. find it in a hash
       - it's returned pinned.
        a) take a lock in exclusive mode
        b) check the state, it should be ACTIVE to be usable
        c) unpin
    2. by a direct reference
       - could only used if a resource cannot be freed
       e.g. accessing a resource by thd->waiting_for is safe,
       a resource cannot be freed as there's a thread waiting for it
*/
struct st_wt_resource {
  WT_RESOURCE_ID  id;
  uint            waiter_count;
  enum { ACTIVE, FREE } state;
#ifndef DBUG_OFF
  mysql_mutex_t  *cond_mutex; /* a mutex for the 'cond' below */
#endif
  /*
    before the 'lock' all elements are mutable, after (and including) -
    immutable in the sense that lf_hash_insert() won't memcpy() over them.
    See wt_init().
  */
#ifdef WT_RWLOCKS_USE_MUTEXES
  /*
    we need a special rwlock-like 'lock' to allow readers bypass
    waiting writers, otherwise readers can deadlock. For example:

      A waits on resource x, owned by B, B waits on resource y, owned
      by A, we have a cycle (A->x->B->y->A)
      Both A and B start deadlock detection:

        A locks x                          B locks y
        A goes deeper                      B goes deeper
        A locks y                          B locks x

      with mutexes it would deadlock. With rwlocks it won't, as long
      as both A and B are taking read locks (and they do).
      But other threads may take write locks. Assume there's
      C who wants to start waiting on x, and D who wants to start
      waiting on y.

        A read-locks x                       B read-locks y
        A goes deeper                        B goes deeper
     => C write-locks x (to add a new edge)  D write-locks y
     .. C is blocked                         D is blocked
        A read-locks y                       B read-locks x

      Now, if a read lock can bypass a pending wrote lock request, we're fine.
      If it can not, we have a deadlock.

    writer starvation is technically possible, but unlikely, because
    the contention is expected to be low.
  */
  struct {
    mysql_cond_t   cond;
    mysql_mutex_t  mutex;
    uint readers: 16;
    uint pending_writers: 15;
    uint write_locked: 1;
  } lock;
#else
  rw_lock_t lock;
#endif
  mysql_cond_t     cond; /* the corresponding mutex is provided by the caller */
  DYNAMIC_ARRAY    owners;
};

#ifdef  WT_RWLOCKS_USE_MUTEXES
static void rc_rwlock_init(WT_RESOURCE *rc)
{
  mysql_cond_init(0, &rc->lock.cond, 0);
  mysql_mutex_init(0, &rc->lock.mutex, MY_MUTEX_INIT_FAST);
}
static void rc_rwlock_destroy(WT_RESOURCE *rc)
{
  DBUG_ASSERT(rc->lock.write_locked == 0);
  DBUG_ASSERT(rc->lock.readers == 0);
  mysql_cond_destroy(&rc->lock.cond);
  mysql_mutex_destroy(&rc->lock.mutex);
}
static void rc_rdlock(WT_RESOURCE *rc)
{
  DBUG_PRINT("wt", ("TRYLOCK resid=%ld for READ", (ulong)rc->id.value));
  mysql_mutex_lock(&rc->lock.mutex);
  while (rc->lock.write_locked)
    mysql_cond_wait(&rc->lock.cond, &rc->lock.mutex);
  rc->lock.readers++;
  mysql_mutex_unlock(&rc->lock.mutex);
  DBUG_PRINT("wt", ("LOCK resid=%ld for READ", (ulong)rc->id.value));
}
static void rc_wrlock(WT_RESOURCE *rc)
{
  DBUG_PRINT("wt", ("TRYLOCK resid=%ld for WRITE", (ulong)rc->id.value));
  mysql_mutex_lock(&rc->lock.mutex);
  while (rc->lock.write_locked || rc->lock.readers)
    mysql_cond_wait(&rc->lock.cond, &rc->lock.mutex);
  rc->lock.write_locked= 1;
  mysql_mutex_unlock(&rc->lock.mutex);
  DBUG_PRINT("wt", ("LOCK resid=%ld for WRITE", (ulong)rc->id.value));
}
static void rc_unlock(WT_RESOURCE *rc)
{
  DBUG_PRINT("wt", ("UNLOCK resid=%ld", (ulong)rc->id.value));
  mysql_mutex_lock(&rc->lock.mutex);
  if (rc->lock.write_locked)
  {
    rc->lock.write_locked= 0;
    mysql_cond_broadcast(&rc->lock.cond);
  }
  else if (--rc->lock.readers == 0)
    mysql_cond_broadcast(&rc->lock.cond);
  mysql_mutex_unlock(&rc->lock.mutex);
}
#else
static void rc_rwlock_init(WT_RESOURCE *rc)
{
  my_rwlock_init(&rc->lock, 0);
}
static void rc_rwlock_destroy(WT_RESOURCE *rc)
{
  rwlock_destroy(&rc->lock);
}
static void rc_rdlock(WT_RESOURCE *rc)
{
  DBUG_PRINT("wt", ("TRYLOCK resid=%ld for READ", (ulong)rc->id.value));
  rw_rdlock(&rc->lock);
  DBUG_PRINT("wt", ("LOCK resid=%ld for READ", (ulong)rc->id.value));
}
static void rc_wrlock(WT_RESOURCE *rc)
{
  DBUG_PRINT("wt", ("TRYLOCK resid=%ld for WRITE", (ulong)rc->id.value));
  rw_wrlock(&rc->lock);
  DBUG_PRINT("wt", ("LOCK resid=%ld for WRITE", (ulong)rc->id.value));
}
static void rc_unlock(WT_RESOURCE *rc)
{
  DBUG_PRINT("wt", ("UNLOCK resid=%ld", (ulong)rc->id.value));
  rw_unlock(&rc->lock);
}
#endif

/*
  All resources are stored in a lock-free hash. Different threads
  may add new resources and perform deadlock detection concurrently.
*/
static LF_HASH      reshash;

/**
  WT_RESOURCE constructor

  It's called from lf_hash and takes a pointer to an LF_SLIST instance.
  WT_RESOURCE is located at arg+sizeof(LF_SLIST)
*/
static void wt_resource_init(uchar *arg)
{
  WT_RESOURCE *rc= (WT_RESOURCE*)(arg+LF_HASH_OVERHEAD);
  DBUG_ENTER("wt_resource_init");

  memset(rc, 0, sizeof(*rc));
  rc_rwlock_init(rc);
  mysql_cond_init(0, &rc->cond, 0);
  my_init_dynamic_array(&rc->owners, sizeof(WT_THD *), 0, 5);
  DBUG_VOID_RETURN;
}

/**
  WT_RESOURCE destructor

  It's called from lf_hash and takes a pointer to an LF_SLIST instance.
  WT_RESOURCE is located at arg+sizeof(LF_SLIST)
*/
static void wt_resource_destroy(uchar *arg)
{
  WT_RESOURCE *rc= (WT_RESOURCE*)(arg+LF_HASH_OVERHEAD);
  DBUG_ENTER("wt_resource_destroy");

  DBUG_ASSERT(rc->owners.elements == 0);
  rc_rwlock_destroy(rc);
  mysql_cond_destroy(&rc->cond);
  delete_dynamic(&rc->owners);
  DBUG_VOID_RETURN;
}

void wt_init()
{
  DBUG_ENTER("wt_init");
  DBUG_ASSERT(reshash.alloc.constructor != wt_resource_init);

  lf_hash_init(&reshash, sizeof(WT_RESOURCE), LF_HASH_UNIQUE, 0,
               sizeof_WT_RESOURCE_ID, 0, 0);
  reshash.alloc.constructor= wt_resource_init;
  reshash.alloc.destructor= wt_resource_destroy;
  /*
    Note a trick: we initialize the hash with the real element size,
    but fix it later to a shortened element size. This way
    the allocator will allocate elements correctly, but
    lf_hash_insert() will only overwrite part of the element with memcpy().
    lock, condition, and dynamic array will be intact.
  */
  reshash.element_size= offsetof(WT_RESOURCE, lock);
  memset(wt_wait_stats, 0, sizeof(wt_wait_stats));
  memset(wt_cycle_stats, 0, sizeof(wt_cycle_stats));
  wt_success_stats= 0;
  { /* initialize wt_wait_table[]. from 1 us to 1 min, log e scale */
    int i;
    double from= log(1);   /* 1 us */
    double to= log(60e6);  /* 1 min */
    for (i= 0; i < WT_WAIT_STATS; i++)
    {
      wt_wait_table[i]= (ulonglong)exp((to-from)/(WT_WAIT_STATS-1)*i+from);
      DBUG_ASSERT(i == 0 || wt_wait_table[i-1] != wt_wait_table[i]);
    }
  }
  my_atomic_rwlock_init(&cycle_stats_lock);
  my_atomic_rwlock_init(&success_stats_lock);
  my_atomic_rwlock_init(&wait_stats_lock);
  DBUG_VOID_RETURN;
}

void wt_end()
{
  DBUG_ENTER("wt_end");

  DBUG_ASSERT(reshash.count == 0);
  lf_hash_destroy(&reshash);
  my_atomic_rwlock_destroy(&cycle_stats_lock);
  my_atomic_rwlock_destroy(&success_stats_lock);
  my_atomic_rwlock_destroy(&wait_stats_lock);
  DBUG_VOID_RETURN;
}

/**
  Lazy WT_THD initialization

  Cheap initialization of WT_THD. Only initialize fields that don't require
  memory allocations - basically, it only does assignments. The rest of the
  WT_THD structure will be initialized on demand, on the first use.
  This allows one to initialize lazily all WT_THD structures, even if some
  (or even most) of them will never be used for deadlock detection.

  @param ds     a pointer to deadlock search depth short value
  @param ts     a pointer to deadlock timeout short value
  @param dl     a pointer to deadlock search depth long value
  @param tl     a pointer to deadlock timeout long value

  @note these are pointers to values, and WT_THD stores them as pointers.
  It allows one later to change search depths and timeouts for existing
  threads. It also means that the pointers must stay valid for the lifetime
  of WT_THD.
*/
void wt_thd_lazy_init(WT_THD *thd, const ulong *ds, const ulong *ts,
                                   const ulong *dl, const ulong *tl)
{
  DBUG_ENTER("wt_thd_lazy_init");
  thd->waiting_for= 0;
  thd->weight= 0;
  thd->deadlock_search_depth_short= ds;
  thd->timeout_short= ts;
  thd->deadlock_search_depth_long= dl;
  thd->timeout_long= tl;
  /* dynamic array is also initialized lazily - without memory allocations */
  my_init_dynamic_array(&thd->my_resources, sizeof(WT_RESOURCE *), 0, 5);
#ifndef DBUG_OFF
  thd->name= my_thread_name();
#endif
  DBUG_VOID_RETURN;
}

/**
  Finalize WT_THD initialization

  After lazy WT_THD initialization, parts of the structure are still
  uninitialized. This function completes the initialization, allocating
  memory, if necessary. It's called automatically on demand, when WT_THD
  is about to be used.
*/
static int fix_thd_pins(WT_THD *thd)
{
  if (unlikely(thd->pins == 0))
  {
    thd->pins= lf_hash_get_pins(&reshash);
#ifndef DBUG_OFF
    thd->name= my_thread_name();
#endif
  }
  return thd->pins == 0;
}

void wt_thd_destroy(WT_THD *thd)
{
  DBUG_ENTER("wt_thd_destroy");

  DBUG_ASSERT(thd->my_resources.elements == 0);
  DBUG_ASSERT(thd->waiting_for == 0);

  if (thd->pins != 0)
    lf_hash_put_pins(thd->pins);

  delete_dynamic(&thd->my_resources);
  DBUG_VOID_RETURN;
}
/**
  Trivial resource id comparison function - bytewise memcmp.

  It can be used in WT_RESOURCE_TYPE structures where bytewise
  comparison of values is sufficient.
*/
my_bool wt_resource_id_memcmp(const void *a, const void *b)
{
  /* we use the fact that there's no padding in the middle of WT_RESOURCE_ID */
  compile_time_assert(offsetof(WT_RESOURCE_ID, type) == sizeof(ulonglong));
  return memcmp(a, b, sizeof_WT_RESOURCE_ID);
}

/**
  arguments for the recursive deadlock_search function
*/
struct deadlock_arg {
  WT_THD * const thd;          /**< starting point of a search */
  uint const max_depth;        /**< search depth limit */
  WT_THD *victim;              /**< a thread to be killed to resolve a deadlock */
  WT_RESOURCE *last_locked_rc; /**< see comment at the end of deadlock_search() */
};

/**
  helper function to change the victim, according to the weight
*/
static void change_victim(WT_THD* found, struct deadlock_arg *arg)
{
  if (found->weight < arg->victim->weight)
  {
    if (arg->victim != arg->thd)
    {
      rc_unlock(arg->victim->waiting_for); /* release the previous victim */
      DBUG_ASSERT(arg->last_locked_rc == found->waiting_for);
    }
    arg->victim= found;
    arg->last_locked_rc= 0;
  }
}

/**
  recursive loop detection in a wait-for graph with a limited search depth
*/
static int deadlock_search(struct deadlock_arg *arg, WT_THD *blocker,
                           uint depth)
{
  WT_RESOURCE *rc, *volatile *shared_ptr= &blocker->waiting_for;
  WT_THD *cursor;
  uint i;
  int ret= WT_OK;
  DBUG_ENTER("deadlock_search");
  DBUG_PRINT("wt", ("enter: thd=%s, blocker=%s, depth=%u",
                    arg->thd->name, blocker->name, depth));

  LF_REQUIRE_PINS(1);

  arg->last_locked_rc= 0;

  if (depth > arg->max_depth)
  {
    DBUG_PRINT("wt", ("exit: WT_DEPTH_EXCEEDED (early)"));
    DBUG_RETURN(WT_DEPTH_EXCEEDED);
  }

retry:
  /*
    safe dereference as explained in lf_alloc-pin.c
    (in short: protects against lf_alloc_free() in lf_hash_delete())
  */
  do
  {
    rc= *shared_ptr;
    lf_pin(arg->thd->pins, 0, rc);
  } while (rc != *shared_ptr && LF_BACKOFF);

  if (rc == 0)
  {
    DBUG_PRINT("wt", ("exit: OK (early)"));
    DBUG_RETURN(0);
  }

  rc_rdlock(rc);
  if (rc->state != ACTIVE || *shared_ptr != rc)
  {
    /* blocker is not waiting on this resource anymore */
    rc_unlock(rc);
    lf_unpin(arg->thd->pins, 0);
    goto retry;
  }
  /* as the state is locked, we can unpin now */
  lf_unpin(arg->thd->pins, 0);

  /*
    Below is not a pure depth-first search. It's a depth-first with a
    slightest hint of breadth-first. Depth-first is:

      check(element, X):
        foreach current in element->nodes[] do:
          if current == X return error;
          check(current, X);

    while we do

      check(element, X):
        foreach current in element->nodes[] do:
          if current == X return error;
        foreach current in element->nodes[] do:
          check(current, X);

    preferring shorter deadlocks over longer ones.
  */
  for (i= 0; i < rc->owners.elements; i++)
  {
    cursor= *dynamic_element(&rc->owners, i, WT_THD**);
    /*
      We're only looking for (and detecting) cycles that include 'arg->thd'.
      That is, only deadlocks that *we* have created. For example,
        thd->A->B->thd
      (thd waits for A, A waits for B, while B is waiting for thd).
      While walking the graph we can encounter other cicles, e.g.
        thd->A->B->C->A
      This will not be detected. Instead we will walk it in circles until
      the search depth limit is reached (the latter guarantees that an
      infinite loop is impossible). We expect the thread that has created
      the cycle (one of A, B, and C) to detect its deadlock.
    */
    if (cursor == arg->thd)
    {
      ret= WT_DEADLOCK;
      increment_cycle_stats(depth, arg->max_depth ==
                                   *arg->thd->deadlock_search_depth_long);
      arg->victim= cursor;
      goto end;
    }
  }
  for (i= 0; i < rc->owners.elements; i++)
  {
    cursor= *dynamic_element(&rc->owners, i, WT_THD**);
    switch (deadlock_search(arg, cursor, depth+1)) {
    case WT_OK:
      break;
    case WT_DEPTH_EXCEEDED:
      ret= WT_DEPTH_EXCEEDED;
      break;
    case WT_DEADLOCK:
      ret= WT_DEADLOCK;
      change_victim(cursor, arg);       /* also sets arg->last_locked_rc to 0 */
      i= rc->owners.elements;           /* jump out of the loop */
      break;
    default:
      DBUG_ASSERT(0);
    }
    if (arg->last_locked_rc)
      rc_unlock(arg->last_locked_rc);
  }
end:
  /*
    Note that 'rc' is locked in this function, but it's never unlocked here.
    Instead it's saved in arg->last_locked_rc and the *caller* is
    expected to unlock it.  It's done to support different killing
    strategies. This is how it works:
    Assuming a graph

      thd->A->B->C->thd

    deadlock_search() function starts from thd, locks it (in fact it locks not
    a thd, but a resource it is waiting on, but below, for simplicity, I'll
    talk about "locking a thd"). Then it goes down recursively, locks A, and so
    on. Goes down recursively, locks B. Goes down recursively, locks C.
    Notices that C is waiting on thd. Deadlock detected. Sets arg->victim=thd.
    Returns from the last deadlock_search() call. C stays locked!
    Now it checks whether C is a more appropriate victim than 'thd'.
    If yes - arg->victim=C, otherwise C is unlocked. Returns. B stays locked.
    Now it checks whether B is a more appropriate victim than arg->victim.
    If yes - old arg->victim is unlocked and arg->victim=B,
    otherwise B is unlocked. Return.
    And so on.

    In short, a resource is locked in a frame. But it's not unlocked in the
    same frame, it's unlocked by the caller, and only after the caller checks
    that it doesn't need to use current WT_THD as a victim. If it does - the
    lock is kept and the old victim's resource is unlocked. When the recursion
    is unrolled and we are back to deadlock() function, there are only two
    locks left - on thd and on the victim.
  */
  arg->last_locked_rc= rc;
  DBUG_PRINT("wt", ("exit: %s",
                    ret == WT_DEPTH_EXCEEDED ? "WT_DEPTH_EXCEEDED" :
                    ret ? "WT_DEADLOCK" : "OK"));
  DBUG_RETURN(ret);
}

/**
  Deadlock detection in a wait-for graph

  A wrapper for recursive deadlock_search() - prepares deadlock_arg structure,
  invokes deadlock_search(), increments statistics, notifies the victim.

  @param thd            thread that is going to wait. Deadlock is detected
                        if, while walking the graph, we reach a thread that
                        is waiting on thd
  @param blocker        starting point of a search. In wt_thd_cond_timedwait()
                        it's thd, in wt_thd_will_wait_for() it's a thread that
                        thd is going to wait for
  @param depth          starting search depth. In general it's the number of
                        edges in the wait-for graph between thd and the
                        blocker. Practically only two values are used (and
                        supported) - when thd == blocker it's 0, when thd
                        waits directly for blocker, it's 1
  @param max_depth      search depth limit
*/
static int deadlock(WT_THD *thd, WT_THD *blocker, uint depth,
                            uint max_depth)
{
  struct deadlock_arg arg= {thd, max_depth, 0, 0};
  int ret;
  DBUG_ENTER("deadlock");
  DBUG_ASSERT(depth < 2);
  ret= deadlock_search(&arg, blocker, depth);
  if (ret == WT_DEPTH_EXCEEDED)
  {
    increment_cycle_stats(WT_CYCLE_STATS, max_depth ==
                                          *thd->deadlock_search_depth_long);
    ret= WT_OK;
  }
  /*
    if we started with depth==1, blocker was never considered for a victim
    in deadlock_search(). Do it here.
  */
  if (ret == WT_DEADLOCK && depth)
    change_victim(blocker, &arg);
  if (arg.last_locked_rc)
  {
    /*
      Special return code if there's nobody to wait for.

      depth == 0 means that we start the search from thd (thd == blocker).
      ret == WT_OK means that no cycle was found and
        arg.last_locked_rc == thd->waiting_for.
      and arg.last_locked_rc->owners.elements == 0 means that
        (applying the rule above) thd->waiting_for->owners.elements == 0,
        and thd doesn't have anybody to wait for.
    */
    if (depth == 0 && ret == WT_OK && arg.last_locked_rc->owners.elements == 0)
    {
      DBUG_ASSERT(thd == blocker);
      DBUG_ASSERT(arg.last_locked_rc == thd->waiting_for);
      ret= WT_FREE_TO_GO;
    }
    rc_unlock(arg.last_locked_rc);
  }
  /* notify the victim, if appropriate */
  if (ret == WT_DEADLOCK && arg.victim != thd)
  {
    DBUG_PRINT("wt", ("killing %s", arg.victim->name));
    arg.victim->killed= 1;
    mysql_cond_broadcast(&arg.victim->waiting_for->cond);
    rc_unlock(arg.victim->waiting_for);
    ret= WT_OK;
  }
  DBUG_RETURN(ret);
}


/**
  Delete an element from reshash if it has no waiters or owners

  rc->lock must be locked by the caller and it's unlocked on return.
*/
static int unlock_lock_and_free_resource(WT_THD *thd, WT_RESOURCE *rc)
{
  uint keylen;
  const void *key;
  DBUG_ENTER("unlock_lock_and_free_resource");

  DBUG_ASSERT(rc->state == ACTIVE);

  if (rc->owners.elements || rc->waiter_count)
  {
    DBUG_PRINT("wt", ("nothing to do, %u owners, %u waiters",
                      rc->owners.elements, rc->waiter_count));
    rc_unlock(rc);
    DBUG_RETURN(0);
  }

  if (fix_thd_pins(thd))
  {
    rc_unlock(rc);
    DBUG_RETURN(1);
  }

  /* XXX if (rc->id.type->make_key) key= rc->id.type->make_key(&rc->id, &keylen); else */
  {
    key= &rc->id;
    keylen= sizeof_WT_RESOURCE_ID;
  }

  /*
    To free the element correctly we need to:
     1. take its lock (already done).
     2. set the state to FREE
     3. release the lock
     4. remove from the hash
  */
  rc->state= FREE;
  rc_unlock(rc);
  DBUG_RETURN(lf_hash_delete(&reshash, thd->pins, key, keylen) == -1);
}


/**
  register the fact that thd is not waiting anymore

  decrease waiter_count, clear waiting_for, free the resource if appropriate.
  thd->waiting_for must be locked!
*/
static int stop_waiting_locked(WT_THD *thd)
{
  int ret;
  WT_RESOURCE *rc= thd->waiting_for;
  DBUG_ENTER("stop_waiting_locked");

  DBUG_ASSERT(rc->waiter_count);
  DBUG_ASSERT(rc->state == ACTIVE);
  rc->waiter_count--;
  thd->waiting_for= 0;
  ret= unlock_lock_and_free_resource(thd, rc);
  DBUG_RETURN((thd->killed || ret) ? WT_DEADLOCK : WT_OK);
}

/**
  register the fact that thd is not waiting anymore

  locks thd->waiting_for and calls stop_waiting_locked().
*/
static int stop_waiting(WT_THD *thd)
{
  int ret;
  WT_RESOURCE *rc= thd->waiting_for;
  DBUG_ENTER("stop_waiting");

  if (!rc)
    DBUG_RETURN(WT_OK);
  /*
    nobody's trying to free the resource now,
    as its waiter_count is guaranteed to be non-zero
  */
  rc_wrlock(rc);
  ret= stop_waiting_locked(thd);
  DBUG_RETURN(ret);
}

/**
  notify the system that a thread needs to wait for another thread

  called by a *waiter* to declare that it (thd) will wait for another
  thread (blocker) on a specific resource (resid).
  can be called many times, if many blockers own a blocking resource.
  but must always be called with the same resource id - a thread cannot
  wait for more than one resource at a time.

  @return WT_OK or WT_DEADLOCK

  As a new edge is added to the wait-for graph, a deadlock detection is
  performed for this new edge.
*/
int wt_thd_will_wait_for(WT_THD *thd, WT_THD *blocker,
                         const WT_RESOURCE_ID *resid)
{
  uint i;
  WT_RESOURCE *rc;
  DBUG_ENTER("wt_thd_will_wait_for");

  LF_REQUIRE_PINS(3);

  DBUG_PRINT("wt", ("enter: thd=%s, blocker=%s, resid=%lu",
                    thd->name, blocker->name, (ulong)resid->value));

  if (fix_thd_pins(thd))
    DBUG_RETURN(WT_DEADLOCK);

  if (thd->waiting_for == 0)
  {
    uint keylen;
    const void *key;
    /* XXX if (restype->make_key) key= restype->make_key(resid, &keylen); else */
    {
      key= resid;
      keylen= sizeof_WT_RESOURCE_ID;
    }

    DBUG_PRINT("wt", ("first blocker"));

retry:
    while ((rc= lf_hash_search(&reshash, thd->pins, key, keylen)) == 0)
    {
      WT_RESOURCE tmp;

      DBUG_PRINT("wt", ("failed to find rc in hash, inserting"));
      memset(&tmp, 0, sizeof(tmp));
      tmp.id= *resid;
      tmp.state= ACTIVE;

      if (lf_hash_insert(&reshash, thd->pins, &tmp) == -1) /* if OOM */
        DBUG_RETURN(WT_DEADLOCK);
      /*
        Two cases: either lf_hash_insert() failed - because another thread
        has just inserted a resource with the same id - and we need to retry.
        Or lf_hash_insert() succeeded, and then we need to repeat
        lf_hash_search() to find a real address of the newly inserted element.
        That is, we don't care what lf_hash_insert() has returned.
        And we need to repeat the loop anyway.
      */
    }
    if (rc == MY_ERRPTR)
      DBUG_RETURN(WT_DEADLOCK);

    DBUG_PRINT("wt", ("found in hash rc=%p", rc));

    rc_wrlock(rc);
    if (rc->state != ACTIVE)
    {
      DBUG_PRINT("wt", ("but it's not active, retrying"));
      /* Somebody has freed the element while we weren't looking */
      rc_unlock(rc);
      lf_hash_search_unpin(thd->pins);
      goto retry;
    }

    lf_hash_search_unpin(thd->pins); /* the element cannot go away anymore */
    thd->waiting_for= rc;
    rc->waiter_count++;
    thd->killed= 0;
  }
  else
  {
    DBUG_ASSERT(thd->waiting_for->id.type == resid->type);
    DBUG_ASSERT(resid->type->compare(&thd->waiting_for->id, resid) == 0);
    DBUG_PRINT("wt", ("adding another blocker"));

    /*
      we can safely access the resource here, it's in the hash as it has
      non-zero waiter_count
    */
    rc= thd->waiting_for;
    rc_wrlock(rc);
    DBUG_ASSERT(rc->waiter_count);
    DBUG_ASSERT(rc->state == ACTIVE);

    if (thd->killed)
    {
      stop_waiting_locked(thd);
      DBUG_RETURN(WT_DEADLOCK);
    }
  }
  /*
    Another thread could be waiting on this resource for this very 'blocker'.
    In this case we should not add it to the list for the second time.
  */
  for (i= 0; i < rc->owners.elements; i++)
    if (*dynamic_element(&rc->owners, i, WT_THD**) == blocker)
      break;
  if (i >= rc->owners.elements)
  {
    if (push_dynamic(&blocker->my_resources, (void*)&rc))
    {
      stop_waiting_locked(thd);
      DBUG_RETURN(WT_DEADLOCK); /* deadlock and OOM use the same error code */
    }
    if (push_dynamic(&rc->owners, (void*)&blocker))
    {
      pop_dynamic(&blocker->my_resources);
      stop_waiting_locked(thd);
      DBUG_RETURN(WT_DEADLOCK);
    }
  }
  rc_unlock(rc);

  if (deadlock(thd, blocker, 1, *thd->deadlock_search_depth_short) != WT_OK)
  {
    stop_waiting(thd);
    DBUG_RETURN(WT_DEADLOCK);
  }
  DBUG_RETURN(WT_OK);
}

/**
  called by a *waiter* (thd) to start waiting

  It's supposed to be a drop-in replacement for
  pthread_cond_timedwait(), and it takes mutex as an argument.

  @return one of WT_TIMEOUT, WT_DEADLOCK, WT_OK
*/
int wt_thd_cond_timedwait(WT_THD *thd, mysql_mutex_t *mutex)
{
  int ret= WT_TIMEOUT;
  struct timespec timeout;
  ulonglong before, after, starttime;
  WT_RESOURCE *rc= thd->waiting_for;
  DBUG_ENTER("wt_thd_cond_timedwait");
  DBUG_PRINT("wt", ("enter: thd=%s, rc=%p", thd->name, rc));

#ifndef DBUG_OFF
  if (rc->cond_mutex)
    DBUG_ASSERT(rc->cond_mutex == mutex);
  else
    rc->cond_mutex= mutex;
  mysql_mutex_assert_owner(mutex);
#endif

  before= starttime= my_getsystime();

#ifdef __WIN__
  /*
    only for the sake of Windows we distinguish between
    'before' and 'starttime':

    my_getsystime() returns high-resolution value, that cannot be used for
    waiting (it doesn't follow system clock changes), but is good for time
    intervals.

    GetSystemTimeAsFileTime() follows system clock, but is low-resolution
    and will result in lousy intervals.
  */
  GetSystemTimeAsFileTime((PFILETIME)&starttime);
#endif

  rc_wrlock(rc);
  if (rc->owners.elements == 0)
    ret= WT_OK;
  rc_unlock(rc);

  set_timespec_time_nsec(timeout, starttime, (*thd->timeout_short)*1000ULL);
  if (ret == WT_TIMEOUT && !thd->killed)
    ret= mysql_cond_timedwait(&rc->cond, mutex, &timeout);
  if (ret == WT_TIMEOUT && !thd->killed)
  {
    int r= deadlock(thd, thd, 0, *thd->deadlock_search_depth_long);
    if (r == WT_FREE_TO_GO)
      ret= WT_OK;
    else if (r != WT_OK)
      ret= WT_DEADLOCK;
    else if (*thd->timeout_long > *thd->timeout_short)
    {
      set_timespec_time_nsec(timeout, starttime, (*thd->timeout_long)*1000ULL);
      if (!thd->killed)
        ret= mysql_cond_timedwait(&rc->cond, mutex, &timeout);
    }
  }
  after= my_getsystime();
  if (stop_waiting(thd) == WT_DEADLOCK) /* if we're killed */
    ret= WT_DEADLOCK;
  increment_wait_stats(after-before, ret);
  if (ret == WT_OK)
    increment_success_stats();
  DBUG_RETURN(ret);
}

/**
  called by a *blocker* when it releases a resource

  it's conceptually similar to pthread_cond_broadcast, and must be done
  under the same mutex as wt_thd_cond_timedwait().

  @param resid   a resource to release. 0 to release all resources
*/

void wt_thd_release(WT_THD *thd, const WT_RESOURCE_ID *resid)
{
  uint i;
  DBUG_ENTER("wt_thd_release");

  for (i= 0; i < thd->my_resources.elements; i++)
  {
    WT_RESOURCE *rc= *dynamic_element(&thd->my_resources, i, WT_RESOURCE**);
    if (!resid || (resid->type->compare(&rc->id, resid) == 0))
    {
      uint j;

      rc_wrlock(rc);
      /*
        nobody's trying to free the resource now,
        as its owners[] array is not empty (at least thd must be there)
      */
      DBUG_ASSERT(rc->state == ACTIVE);
      for (j= 0; j < rc->owners.elements; j++)
        if (*dynamic_element(&rc->owners, j, WT_THD**) == thd)
          break;
      DBUG_ASSERT(j < rc->owners.elements);
      delete_dynamic_element(&rc->owners, j);
      if (rc->owners.elements == 0)
      {
        mysql_cond_broadcast(&rc->cond);
#ifndef DBUG_OFF
        if (rc->cond_mutex)
          mysql_mutex_assert_owner(rc->cond_mutex);
#endif
      }
      unlock_lock_and_free_resource(thd, rc);
      if (resid)
      {
        delete_dynamic_element(&thd->my_resources, i);
        DBUG_VOID_RETURN;
      }
    }
  }
  if (!resid)
    reset_dynamic(&thd->my_resources);
  DBUG_VOID_RETURN;
}

