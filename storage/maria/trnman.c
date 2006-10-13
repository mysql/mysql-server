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
#include <lf.h>
#include "trnman.h"

uint trnman_active_transactions, trnman_allocated_transactions;

static TRN active_list_min, active_list_max,
       committed_list_min, committed_list_max, *pool;

static pthread_mutex_t LOCK_trn_list;
static TrID global_trid_generator;

static LF_HASH trid_to_trn;
static LOCKMAN maria_lockman;

static TRN **short_trid_to_trn;
static my_atomic_rwlock_t LOCK_short_trid_to_trn, LOCK_pool;

static byte *trn_get_hash_key(const byte *trn,uint* len, my_bool unused)
{
  *len= sizeof(TrID);
  return (byte *) & ((*((TRN **)trn))->trid);
}

static LOCK_OWNER *trnman_short_trid_to_TRN(uint16 short_trid)
{
  TRN *trn;
  my_atomic_rwlock_rdlock(&LOCK_short_trid_to_trn);
  trn= my_atomic_loadptr((void **)&short_trid_to_trn[short_trid]);
  my_atomic_rwlock_rdunlock(&LOCK_short_trid_to_trn);
  return (LOCK_OWNER *)trn;
}

int trnman_init()
{
  pthread_mutex_init(&LOCK_trn_list, MY_MUTEX_INIT_FAST);
  active_list_max.trid= active_list_min.trid= 0;
  active_list_max.min_read_from= ~0;
  active_list_max.next= active_list_min.prev= 0;
  active_list_max.prev= &active_list_min;
  active_list_min.next= &active_list_max;
  trnman_active_transactions= 0;
  trnman_allocated_transactions= 0;

  committed_list_max.commit_trid= ~0;
  committed_list_max.next= committed_list_min.prev= 0;
  committed_list_max.prev= &committed_list_min;
  committed_list_min.next= &committed_list_max;

  pool= 0;
  global_trid_generator= 0; /* set later by recovery code */
  lf_hash_init(&trid_to_trn, sizeof(TRN*), LF_HASH_UNIQUE,
               0, 0, trn_get_hash_key, 0);
  my_atomic_rwlock_init(&LOCK_short_trid_to_trn);
  my_atomic_rwlock_init(&LOCK_pool);
  short_trid_to_trn= (TRN **)my_malloc(SHORT_TRID_MAX*sizeof(TRN*),
                                     MYF(MY_WME|MY_ZEROFILL));
  if (!short_trid_to_trn)
    return 1;
  short_trid_to_trn--; /* min short_trid is 1 */

  lockman_init(&maria_lockman, &trnman_short_trid_to_TRN, 10000);

  return 0;
}

int trnman_destroy()
{
  DBUG_ASSERT(trid_to_trn.count == 0);
  DBUG_ASSERT(trnman_active_transactions == 0);
  DBUG_ASSERT(active_list_max.prev == &active_list_min);
  DBUG_ASSERT(active_list_min.next == &active_list_max);
  DBUG_ASSERT(committed_list_max.prev == &committed_list_min);
  DBUG_ASSERT(committed_list_min.next == &committed_list_max);
  while (pool)
  {
    TRN *trn= pool;
    pool= pool->next;
    DBUG_ASSERT(trn->locks.mutex == 0);
    DBUG_ASSERT(trn->locks.cond == 0);
    my_free((void *)trn, MYF(0));
  }
  lf_hash_destroy(&trid_to_trn);
  pthread_mutex_destroy(&LOCK_trn_list);
  my_atomic_rwlock_destroy(&LOCK_short_trid_to_trn);
  my_atomic_rwlock_destroy(&LOCK_pool);
  my_free((void *)(short_trid_to_trn+1), MYF(0));
  lockman_destroy(&maria_lockman);
}

static TrID new_trid()
{
  DBUG_ASSERT(global_trid_generator < 0xffffffffffffLL);
  safe_mutex_assert_owner(&LOCK_trn_list);
  return ++global_trid_generator;
}

static void set_short_trid(TRN *trn)
{
  int i= (global_trid_generator + (intptr)trn) * 312089 % SHORT_TRID_MAX;
  my_atomic_rwlock_wrlock(&LOCK_short_trid_to_trn);
  for ( ; ; i= i % SHORT_TRID_MAX + 1) /* the range is [1..SHORT_TRID_MAX] */
  {
    void *tmp= NULL;
    if (short_trid_to_trn[i] == NULL &&
        my_atomic_casptr((void **)&short_trid_to_trn[i], &tmp, trn))
      break;
  }
  my_atomic_rwlock_wrunlock(&LOCK_short_trid_to_trn);
  trn->locks.loid= i;
}

TRN *trnman_new_trn(pthread_mutex_t *mutex, pthread_cond_t *cond)
{
  TRN *trn;

  /*
    see trnman_end_trn to see why we need a mutex here

    and as we have a mutex, we can as well do everything
    under it - allocating a TRN, incrementing trnman_active_transactions,
    setting trn->min_read_from.

    Note that all the above is fast. generating short_trid may be slow,
    as it involves scanning a big array - so it's still done
    outside of the mutex.
  */

  pthread_mutex_lock(&LOCK_trn_list);
  trnman_active_transactions++;

  trn= pool;
  my_atomic_rwlock_wrlock(&LOCK_pool);
  while (trn && !my_atomic_casptr((void **)&pool, (void **)&trn,
                                  (void *)trn->next))
    /* no-op */;
  my_atomic_rwlock_wrunlock(&LOCK_pool);

  if (!trn)
  {
    trn= (TRN *)my_malloc(sizeof(TRN), MYF(MY_WME));
    if (!trn)
    {
      pthread_mutex_unlock(&LOCK_trn_list);
      return 0;
    }
    trnman_allocated_transactions++;
  }

  trn->min_read_from= active_list_min.next->trid;

  trn->trid= new_trid();
  trn->locks.loid= 0;

  trn->next= &active_list_max;
  trn->prev= active_list_max.prev;
  active_list_max.prev= trn->prev->next= trn;
  pthread_mutex_unlock(&LOCK_trn_list);

  trn->pins= lf_hash_get_pins(&trid_to_trn);

  if (!trn->min_read_from)
    trn->min_read_from= trn->trid;

  trn->locks.mutex= mutex;
  trn->locks.cond= cond;
  trn->commit_trid= 0;
  trn->locks.waiting_for= 0;
  trn->locks.all_locks= 0;
  trn->locks.pins= lf_alloc_get_pins(&maria_lockman.alloc);

  set_short_trid(trn); /* this must be the last! */

  return trn;
}

/*
  remove a trn from the active list,
  move to committed list,
  set commit_trid

  TODO
    integrate with log manager. That means:
    a common "commit" mutex - forcing the log and setting commit_trid
    must be done atomically (QQ how the heck it could be done with
    group commit ???) XXX - why did I think it must be done atomically ?

    trid_to_trn, active_list_*, and committed_list_* can be
    updated asyncronously.
*/
void trnman_end_trn(TRN *trn, my_bool commit)
{
  int res;
  TRN *free_me= 0;
  LF_PINS *pins= trn->pins;

  pthread_mutex_lock(&LOCK_trn_list);
  trn->next->prev= trn->prev;
  trn->prev->next= trn->next;

  if (trn->prev == &active_list_min)
  {
    TRN *t;
    for (t= committed_list_min.next;
         t->commit_trid < active_list_min.next->min_read_from;
         t= t->next) /* no-op */;

    if (t != committed_list_min.next)
    {
      free_me= committed_list_min.next;
      committed_list_min.next= t;
      t->prev->next= 0;
      t->prev= &committed_list_min;
    }
  }

  if (commit && active_list_min.next != &active_list_max)
  {
    trn->commit_trid= global_trid_generator;

    trn->next= &committed_list_max;
    trn->prev= committed_list_max.prev;
    committed_list_max.prev= trn->prev->next= trn;

    res= lf_hash_insert(&trid_to_trn, pins, &trn);
    DBUG_ASSERT(res == 0);
  }
  else
  {
    trn->next= free_me;
    free_me= trn;
  }
  trnman_active_transactions--;
  pthread_mutex_unlock(&LOCK_trn_list);

  lockman_release_locks(&maria_lockman, &trn->locks);
  trn->locks.mutex= 0;
  trn->locks.cond= 0;
  my_atomic_rwlock_rdlock(&LOCK_short_trid_to_trn);
  my_atomic_storeptr((void **)&short_trid_to_trn[trn->locks.loid], 0);
  my_atomic_rwlock_rdunlock(&LOCK_short_trid_to_trn);


  while (free_me) // XXX send them to the purge thread
  {
    int res;
    TRN *t= free_me;
    free_me= free_me->next;

    res= lf_hash_delete(&trid_to_trn, pins, &t->trid, sizeof(TrID));

    trnman_free_trn(t);
  }

  lf_hash_put_pins(pins);
  lf_pinbox_put_pins(trn->locks.pins);
}

/*
  free a trn (add to the pool, that is)
  note - we can never really free() a TRN if there's at least one
  other running transaction - see, e.g., how lock waits are implemented
  in lockman.c
*/
void trnman_free_trn(TRN *trn)
{
  TRN *tmp= pool;

  my_atomic_rwlock_wrlock(&LOCK_pool);
  do
  {
    /*
      without volatile cast gcc-3.4.4 moved the assignment
      down after the loop at -O2
    */
    *(TRN * volatile *)&(trn->next)= tmp;
  } while (!my_atomic_casptr((void **)&pool, (void **)&tmp, trn));
  my_atomic_rwlock_wrunlock(&LOCK_pool);
}

/*
  NOTE
    here we access the hash in a lock-free manner.
    It's safe, a 'found' TRN can never be freed/reused before we access it.
    In fact, it cannot be freed before 'trn' ends, because a 'found' TRN
    can only be removed from the hash when:
                found->commit_trid < ALL (trn->min_read_from)
    that is, at least
                found->commit_trid < trn->min_read_from
    but
                found->trid >= trn->min_read_from
    and
                found->commit_trid > found->trid
*/
my_bool trnman_can_read_from(TRN *trn, TrID trid)
{
  TRN **found;
  my_bool can;
  LF_REQUIRE_PINS(3);

  if (trid < trn->min_read_from)
    return TRUE;
  if (trid > trn->trid)
    return FALSE;

  found= lf_hash_search(&trid_to_trn, trn->pins, &trid, sizeof(trid));
  if (!found)
    return FALSE; /* not in the hash of committed transactions = cannot read*/

  can= (*found)->commit_trid < trn->trid;
  lf_unpin(trn->pins, 2);
  return can;
}

