/* Copyright (C) 2008 MySQL AB

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
  Note that if your lock system satisfy the following condition:

    there exist four lock levels A, B, C, D, such as
      A is compatible with B
      A is not compatible with C
      D is not compatible with B

      (example A=IX, B=IS, C=S, D=X)

   you need to include lock level in the resource identifier - thread 1
   waiting for lock A on resource R and thread 2 waiting for lock B
   on resource R should wait on different WT_RESOURCE structures, on different
   {lock, resource} pairs. Otherwise the following is possible:

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

uint wt_timeout_short=100, wt_deadlock_search_depth_short=4;
uint wt_timeout_long=10000, wt_deadlock_search_depth_long=15;

/*
  status variables:
    distribution of cycle lengths
    wait time log distribution

  Note:

    we call deadlock() twice per wait (with different search lengths).
    it means a deadlock will be counted twice. It's difficult to avoid,
    as on the second search we could find a *different* deadlock and we
    *want* to count it too. So we just count all deadlocks - two searches
    mean two increments on the wt_cycle_stats.
*/

ulonglong wt_wait_table[WT_WAIT_STATS];
uint32    wt_wait_stats[WT_WAIT_STATS+1];
uint32    wt_cycle_stats[2][WT_CYCLE_STATS+1], wt_success_stats;

static my_atomic_rwlock_t cycle_stats_lock, wait_stats_lock, success_stats_lock;

#define increment_success_stats()                                       \
  do {                                                                  \
    my_atomic_rwlock_wrlock(&success_stats_lock);                       \
    my_atomic_add32(&wt_success_stats, 1);                              \
    my_atomic_rwlock_wrunlock(&success_stats_lock);                     \
  } while (0)

#define increment_cycle_stats(X,MAX)                                    \
  do {                                                                  \
    uint i= (X), j= (MAX) == wt_deadlock_search_depth_long;             \
    if (i >= WT_CYCLE_STATS)                                            \
      i= WT_CYCLE_STATS;                                                \
    my_atomic_rwlock_wrlock(&cycle_stats_lock);                         \
    my_atomic_add32(&wt_cycle_stats[j][i], 1);                          \
    my_atomic_rwlock_wrunlock(&cycle_stats_lock);                       \
  } while (0)

#define increment_wait_stats(X,RET)                                     \
  do {                                                                  \
    uint i;                                                             \
    if ((RET) == ETIMEDOUT)                                             \
      i= WT_WAIT_STATS;                                                 \
    else                                                                \
    {                                                                   \
      ulonglong w=(X)/10;                                               \
      for (i=0; i < WT_WAIT_STATS && w > wt_wait_table[i]; i++) ;       \
    }                                                                   \
    my_atomic_rwlock_wrlock(&wait_stats_lock);                          \
    my_atomic_add32(wt_wait_stats+i, 1);                                \
    my_atomic_rwlock_wrunlock(&wait_stats_lock);                        \
  } while (0)

#define rc_rdlock(X)                                                    \
  do {                                                                  \
    WT_RESOURCE *R=(X);                                                 \
    DBUG_PRINT("wt", ("LOCK resid=%lld for READ", R->id.value.num));    \
    pthread_rwlock_rdlock(&R->lock);                                    \
  } while (0)
#define rc_wrlock(X)                                                    \
  do {                                                                  \
    WT_RESOURCE *R=(X);                                                 \
    DBUG_PRINT("wt", ("LOCK resid=%lld for WRITE", R->id.value.num));   \
    pthread_rwlock_wrlock(&R->lock);                                    \
  } while (0)
#define rc_unlock(X)                                                    \
  do {                                                                  \
    WT_RESOURCE *R=(X);                                                 \
    DBUG_PRINT("wt", ("UNLOCK resid=%lld", R->id.value.num));           \
    pthread_rwlock_unlock(&R->lock);                                    \
  } while (0)

static LF_HASH      reshash;

static void wt_resource_init(uchar *arg)
{
  WT_RESOURCE *rc=(WT_RESOURCE*)(arg+LF_HASH_OVERHEAD);
  DBUG_ENTER("wt_resource_init");

  bzero(rc, sizeof(*rc));
  pthread_rwlock_init(&rc->lock, 0);
  pthread_cond_init(&rc->cond, 0);
  my_init_dynamic_array(&rc->owners, sizeof(WT_THD *), 5, 5);
  DBUG_VOID_RETURN;
}

static void wt_resource_destroy(uchar *arg)
{
  WT_RESOURCE *rc=(WT_RESOURCE*)(arg+LF_HASH_OVERHEAD);
  DBUG_ENTER("wt_resource_destroy");

  DBUG_ASSERT(rc->owners.elements == 0);
  pthread_rwlock_destroy(&rc->lock);
  pthread_cond_destroy(&rc->cond);
  delete_dynamic(&rc->owners);
  DBUG_VOID_RETURN;
}

void wt_init()
{
  DBUG_ENTER("wt_init");

  lf_hash_init(&reshash, sizeof(WT_RESOURCE), LF_HASH_UNIQUE, 0,
               sizeof(struct st_wt_resource_id), 0, 0);
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
  bzero(wt_wait_stats, sizeof(wt_wait_stats));
  bzero(wt_cycle_stats, sizeof(wt_cycle_stats));
  wt_success_stats=0;
  {
    int i;
    double from=log(1);   /* 1 us */
    double to=log(60e6);  /* 1 min */
    for (i=0; i < WT_WAIT_STATS; i++)
    {
      wt_wait_table[i]=(ulonglong)exp((to-from)/(WT_WAIT_STATS-1)*i+from);
      DBUG_ASSERT(i==0 || wt_wait_table[i-1] != wt_wait_table[i]);
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

void wt_thd_init(WT_THD *thd)
{
  DBUG_ENTER("wt_thd_init");

  my_init_dynamic_array(&thd->my_resources, sizeof(WT_RESOURCE *), 10, 5);
  thd->pins=lf_hash_get_pins(&reshash);
  thd->waiting_for=0;
  thd->weight=0;
#ifndef DBUG_OFF
  thd->name=my_thread_name();
#endif
  DBUG_VOID_RETURN;
}

void wt_thd_destroy(WT_THD *thd)
{
  DBUG_ENTER("wt_thd_destroy");

  if (thd->my_resources.buffer == 0)
    DBUG_VOID_RETURN; /* nothing to do */

  DBUG_ASSERT(thd->my_resources.elements == 0);
  delete_dynamic(&thd->my_resources);
  lf_hash_put_pins(thd->pins);
  thd->waiting_for=0;
  DBUG_VOID_RETURN;
}

int wt_resource_id_memcmp(void *a, void *b)
{
  return memcmp(a, b, sizeof(WT_RESOURCE_ID));
}

struct deadlock_arg {
  WT_THD *thd;
  uint    max_depth;
  WT_THD *victim;
  WT_RESOURCE *rc;
};

static void change_victim(WT_THD* found, struct deadlock_arg *arg)
{
  if (found->weight < arg->victim->weight)
  {
    if (arg->victim != arg->thd)
    {
      rc_unlock(arg->victim->waiting_for); /* release the previous victim */
      DBUG_ASSERT(arg->rc == found->waiting_for);
    }
    arg->victim= found;
    arg->rc= 0;
  }
}

/*
  loop detection in a wait-for graph with a limited search depth.
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

  arg->rc= 0;

  if (depth > arg->max_depth)
  {
    DBUG_PRINT("wt", ("exit: WT_DEPTH_EXCEEDED (early)"));
    DBUG_RETURN(WT_DEPTH_EXCEEDED);
  }

retry:
  /* safe dereference as explained in lf_alloc-pin.c */
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
    rc_unlock(rc);
    lf_unpin(arg->thd->pins, 0);
    goto retry;
  }
  lf_unpin(arg->thd->pins, 0);

  for (i=0; i < rc->owners.elements; i++)
  {
    cursor= *dynamic_element(&rc->owners, i, WT_THD**);
    if (cursor == arg->thd)
    {
      ret= WT_DEADLOCK;
      increment_cycle_stats(depth, arg->max_depth);
      arg->victim= cursor;
      goto end;
    }
  }
  for (i=0; i < rc->owners.elements; i++)
  {
    cursor= *dynamic_element(&rc->owners, i, WT_THD**);
    switch (deadlock_search(arg, cursor, depth+1)) {
    case WT_DEPTH_EXCEEDED:
      ret= WT_DEPTH_EXCEEDED;
      break;
    case WT_DEADLOCK:
      ret= WT_DEADLOCK;
      change_victim(cursor, arg);
      if (arg->rc)
        rc_unlock(arg->rc);
      goto end;
    case WT_OK:
      break;
    default:
      DBUG_ASSERT(0);
    }
    if (arg->rc)
      rc_unlock(arg->rc);
  }
end:
  arg->rc= rc;
  DBUG_PRINT("wt", ("exit: %s",
                    ret == WT_DEPTH_EXCEEDED ? "WT_DEPTH_EXCEEDED" :
                    ret ? "WT_DEADLOCK" : "OK"));
  DBUG_RETURN(ret);
}

static int deadlock(WT_THD *thd, WT_THD *blocker, uint depth,
                            uint max_depth)
{
  struct deadlock_arg arg= {thd, max_depth, 0, 0};
  int ret;
  DBUG_ENTER("deadlock");
  ret= deadlock_search(&arg, blocker, depth);
  if (ret == WT_DEPTH_EXCEEDED)
  {
    increment_cycle_stats(WT_CYCLE_STATS, max_depth);
    ret= WT_OK;
  }
  if (ret == WT_DEADLOCK && depth)
    change_victim(blocker, &arg);
  if (arg.rc)
    rc_unlock(arg.rc);
  if (ret == WT_DEADLOCK && arg.victim != thd)
  {
    DBUG_PRINT("wt", ("killing %s", arg.victim->name));
    arg.victim->killed=1;
    pthread_cond_broadcast(&arg.victim->waiting_for->cond);
    rc_unlock(arg.victim->waiting_for);
    ret= WT_OK;
  }
  DBUG_RETURN(ret);
}


/*
  Deletes an element from reshash.
  rc->lock must be locked by the caller and it's unlocked on return.
*/
static void unlock_lock_and_free_resource(WT_THD *thd, WT_RESOURCE *rc)
{
  uint keylen;
  const void *key;
  DBUG_ENTER("unlock_lock_and_free_resource");

  DBUG_ASSERT(rc->state == ACTIVE);

  if (rc->owners.elements || rc->waiter_count)
  {
    DBUG_PRINT("wt", ("nothing to do, %d owners, %d waiters",
                      rc->owners.elements, rc->waiter_count));
    rc_unlock(rc);
    DBUG_VOID_RETURN;
  }

  /* XXX if (rc->id.type->make_key) key= rc->id.type->make_key(&rc->id, &keylen); else */
  {
    key= &rc->id;
    keylen= sizeof(rc->id);
  }

  /*
    To free the element correctly we need to:
     1. take its lock (already done).
     2. set the state to FREE
     3. release the lock
     4. remove from the hash

     I *think* it's safe to release the lock while the element is still
     in the hash. If not, the corrected procedure should be
     3. pin; 4; remove; 5; release; 6; unpin and it'll need pin[3].
  */
  rc->state=FREE;
  rc_unlock(rc);
  lf_hash_delete(&reshash, thd->pins, key, keylen);
  DBUG_VOID_RETURN;
}


int wt_thd_dontwait_locked(WT_THD *thd)
{
  WT_RESOURCE *rc= thd->waiting_for;
  DBUG_ENTER("wt_thd_dontwait_locked");

  DBUG_ASSERT(rc->waiter_count);
  DBUG_ASSERT(rc->state == ACTIVE);
  rc->waiter_count--;
  thd->waiting_for= 0;
  unlock_lock_and_free_resource(thd, rc);
  DBUG_RETURN(thd->killed ? WT_DEADLOCK : WT_OK);
}

int wt_thd_dontwait(WT_THD *thd)
{
  int ret;
  WT_RESOURCE *rc= thd->waiting_for;
  DBUG_ENTER("wt_thd_dontwait");

  if (!rc)
    DBUG_RETURN(WT_OK);
  /*
    nobody's trying to free the resource now,
    as its waiter_count is guaranteed to be non-zero
  */
  rc_wrlock(rc);
  ret= wt_thd_dontwait_locked(thd);
  DBUG_RETURN(ret);
}

/*
  called by a *waiter* to declare what resource it will wait for.
  can be called many times, if many blockers own a blocking resource.
  but must always be called with the same resource id - a thread cannot
  wait for more than one resource at a time.
*/
int wt_thd_will_wait_for(WT_THD *thd, WT_THD *blocker, WT_RESOURCE_ID *resid)
{
  uint i;
  WT_RESOURCE *rc;
  DBUG_ENTER("wt_thd_will_wait_for");

  LF_REQUIRE_PINS(3);

  DBUG_PRINT("wt", ("enter: thd=%s, blocker=%s, resid=%llu",
                    thd->name, blocker->name, resid->value.num));

  if (unlikely(thd->my_resources.buffer == 0))
    wt_thd_init(thd);

  if (thd->waiting_for == 0)
  {
    uint keylen;
    const void *key;
    /* XXX if (restype->make_key) key= restype->make_key(resid, &keylen); else */
    {
      key= resid;
      keylen= sizeof(*resid);
    }

    DBUG_PRINT("wt", ("first blocker"));

retry:
    while ((rc= lf_hash_search(&reshash, thd->pins, key, keylen)) == 0)
    {
      WT_RESOURCE tmp;

      DBUG_PRINT("wt", ("failed to find rc in hash, inserting"));
      bzero(&tmp, sizeof(tmp));
      tmp.waiter_count= 0;
      tmp.id= *resid;
      tmp.state= ACTIVE;
#ifndef DBUG_OFF
      tmp.mutex= 0;
#endif

      lf_hash_insert(&reshash, thd->pins, &tmp);
      /*
        Two cases: either lf_hash_insert() failed - because another thread
        has just inserted a resource with the same id - and we need to retry.
        Or lf_hash_insert() succeeded, and then we need to repeat
        lf_hash_search() to find a real address of the newly inserted element.
        That is, we don't care what lf_hash_insert() has returned.
        And we need to repeat the loop anyway.
      */
    }
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
      at least one owner, and non-zero waiter_count
    */
    rc= thd->waiting_for;
    rc_wrlock(rc);
    DBUG_ASSERT(rc->waiter_count);
    DBUG_ASSERT(rc->state == ACTIVE);

    if (thd->killed)
    {
      wt_thd_dontwait_locked(thd);
      DBUG_RETURN(WT_DEADLOCK);
    }
  }
  for (i=0; i < rc->owners.elements; i++)
    if (*dynamic_element(&rc->owners, i, WT_THD**) == blocker)
      break;
  if (i >= rc->owners.elements)
  {
    push_dynamic(&blocker->my_resources, (void*)&rc);
    push_dynamic(&rc->owners, (void*)&blocker);
  }
  rc_unlock(rc);

  if (deadlock(thd, blocker, 1, wt_deadlock_search_depth_short))
  {
    wt_thd_dontwait(thd);
    DBUG_RETURN(WT_DEADLOCK);
  }
  DBUG_RETURN(0);
}

/*
  called by a *waiter* to start waiting

  It's supposed to be a drop-in replacement for
  pthread_cond_timedwait(), and it takes mutex as an argument.
*/
int wt_thd_cond_timedwait(WT_THD *thd, pthread_mutex_t *mutex)
{
  int ret= WT_TIMEOUT;
  struct timespec timeout;
  ulonglong before, after, starttime;
  WT_RESOURCE *rc= thd->waiting_for;
  DBUG_ENTER("wt_thd_cond_timedwait");
  DBUG_PRINT("wt", ("enter: thd=%s, rc=%p", thd->name, rc));

#ifndef DBUG_OFF
  if (rc->mutex)
    DBUG_ASSERT(rc->mutex == mutex);
  else
    rc->mutex= mutex;
  safe_mutex_assert_owner(mutex);
#endif

  before= starttime= my_getsystime();

#ifdef __WIN__
  /*
    only for the sake of Windows we distinguish between
    'before' and 'starttime'
  */
  GetSystemTimeAsFileTime((PFILETIME)&starttime);
#endif

  rc_wrlock(rc);
  if (rc->owners.elements == 0 && thd->killed)
    ret= WT_OK;
  rc_unlock(rc);

  set_timespec_time_nsec(timeout, starttime, wt_timeout_short*ULL(1000));
  if (ret == WT_TIMEOUT)
    ret= pthread_cond_timedwait(&rc->cond, mutex, &timeout);
  if (ret == WT_TIMEOUT)
  {
    if (deadlock(thd, thd, 0, wt_deadlock_search_depth_long))
      ret= WT_DEADLOCK;
    else if (wt_timeout_long > wt_timeout_short)
    {
      set_timespec_time_nsec(timeout, starttime, wt_timeout_long*ULL(1000));
      if (!thd->killed)
        ret= pthread_cond_timedwait(&rc->cond, mutex, &timeout);
    }
  }
  after= my_getsystime();
  if (wt_thd_dontwait(thd) == WT_DEADLOCK)
    ret= WT_DEADLOCK;
  increment_wait_stats(after-before, ret);
  if (ret == WT_OK)
    increment_success_stats();
  DBUG_RETURN(ret);
}

/*
  called by a *blocker* when it releases a resource

  when resid==0 all resources will be freed

  Note: it's conceptually similar to pthread_cond_broadcast, and must be done
  under the same mutex as wt_thd_cond_timedwait().
*/
void wt_thd_release(WT_THD *thd, WT_RESOURCE_ID *resid)
{
  WT_RESOURCE *rc;
  uint i, j;
  DBUG_ENTER("wt_thd_release");

  for (i=0; i < thd->my_resources.elements; i++)
  {
    rc= *dynamic_element(&thd->my_resources, i, WT_RESOURCE**);
    if (!resid || (resid->type->compare(&rc->id, resid) == 0))
    {
      rc_wrlock(rc);
      /*
        nobody's trying to free the resource now,
        as its owners[] array is not empty (at least thd must be there)
      */
      DBUG_ASSERT(rc->state == ACTIVE);
      for (j=0; j < rc->owners.elements; j++)
        if (*dynamic_element(&rc->owners, j, WT_THD**) == thd)
          break;
      DBUG_ASSERT(j < rc->owners.elements);
      delete_dynamic_element(&rc->owners, j);
      if (rc->owners.elements == 0)
      {
        pthread_cond_broadcast(&rc->cond);
#ifndef DBUG_OFF
        if (rc->mutex)
          safe_mutex_assert_owner(rc->mutex);
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

