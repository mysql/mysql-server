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


#include <my_global.h>
#include <my_sys.h>
#include <lf.h>
#include <m_string.h>
#include "trnman.h"

/*
  status variables:
  how many trns in the active list currently,
  in the committed list currently, allocated since startup.
*/
uint trnman_active_transactions, trnman_committed_transactions,
  trnman_allocated_transactions;

/* list of active transactions in the trid order */
static TRN active_list_min, active_list_max;
/* list of committed transactions in the trid order */
static TRN committed_list_min, committed_list_max;

/* a counter, used to generate transaction ids */
static TrID global_trid_generator;

/* the mutex for everything above */
static pthread_mutex_t LOCK_trn_list;

/* LIFO pool of unused TRN structured for reuse */
static TRN *pool;

/* a hash for committed transactions that maps trid to a TRN structure */
static LF_HASH trid_to_committed_trn;

/* an array that maps short_trid of an active transaction to a TRN structure */
static TRN **short_trid_to_active_trn;

/* locks for short_trid_to_active_trn and pool */
static my_atomic_rwlock_t LOCK_short_trid_to_trn, LOCK_pool;

static LOCKMAN maria_lockman;

/*
  short transaction id is at the same time its identifier
  for a lock manager - its lock owner identifier (loid)
*/
#define short_id locks.loid

/*
  NOTE
    Just as short_id doubles as loid, this function doubles as
    short_trid_to_LOCK_OWNER. See the compile-time assert below.
*/
static TRN *short_trid_to_TRN(uint16 short_trid)
{
  TRN *trn;
  compile_time_assert(offsetof(TRN, locks) == 0);
  my_atomic_rwlock_rdlock(&LOCK_short_trid_to_trn);
  trn= my_atomic_loadptr((void **)&short_trid_to_active_trn[short_trid]);
  my_atomic_rwlock_rdunlock(&LOCK_short_trid_to_trn);
  return (TRN *)trn;
}

static byte *trn_get_hash_key(const byte *trn, uint* len,
                              my_bool unused __attribute__ ((unused)))
{
  *len= sizeof(TrID);
  return (byte *) & ((*((TRN **)trn))->trid);
}

int trnman_init()
{
  /*
    Initialize lists.
    active_list_max.min_read_from must be larger than any trid,
    so that when an active list is empty we would could free
    all committed list.
    And  committed_list_max itself can not be freed so
    committed_list_max.commit_trid must not be smaller that
    active_list_max.min_read_from
  */

  active_list_max.trid= active_list_min.trid= 0;
  active_list_max.min_read_from= ~0;
  active_list_max.next= active_list_min.prev= 0;
  active_list_max.prev= &active_list_min;
  active_list_min.next= &active_list_max;

  committed_list_max.commit_trid= ~0;
  committed_list_max.next= committed_list_min.prev= 0;
  committed_list_max.prev= &committed_list_min;
  committed_list_min.next= &committed_list_max;

  trnman_active_transactions= 0;
  trnman_committed_transactions= 0;
  trnman_allocated_transactions= 0;

  pool= 0;
  global_trid_generator= 0; /* set later by the recovery code */
  lf_hash_init(&trid_to_committed_trn, sizeof(TRN*), LF_HASH_UNIQUE,
               0, 0, trn_get_hash_key, 0);
  pthread_mutex_init(&LOCK_trn_list, MY_MUTEX_INIT_FAST);
  my_atomic_rwlock_init(&LOCK_short_trid_to_trn);
  my_atomic_rwlock_init(&LOCK_pool);
  short_trid_to_active_trn= (TRN **)my_malloc(SHORT_TRID_MAX*sizeof(TRN*),
                                     MYF(MY_WME|MY_ZEROFILL));
  if (unlikely(!short_trid_to_active_trn))
    return 1;
  short_trid_to_active_trn--; /* min short_trid is 1 */

  lockman_init(&maria_lockman, (loid_to_lo_func *)&short_trid_to_TRN, 10000);

  return 0;
}

/*
  NOTE
    this could only be called in the "idle" state - no transaction can be
    running. See asserts below.
*/
void trnman_destroy()
{
  DBUG_ASSERT(trid_to_committed_trn.count == 0);
  DBUG_ASSERT(trnman_active_transactions == 0);
  DBUG_ASSERT(trnman_committed_transactions == 0);
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
  lf_hash_destroy(&trid_to_committed_trn);
  pthread_mutex_destroy(&LOCK_trn_list);
  my_atomic_rwlock_destroy(&LOCK_short_trid_to_trn);
  my_atomic_rwlock_destroy(&LOCK_pool);
  my_free((void *)(short_trid_to_active_trn+1), MYF(0));
  lockman_destroy(&maria_lockman);
}

/*
  NOTE
    TrID is limited to 6 bytes. Initial value of the generator
    is set by the recovery code - being read from the last checkpoint
    (or 1 on a first run).
*/
static TrID new_trid()
{
  DBUG_ASSERT(global_trid_generator < 0xffffffffffffLL);
  safe_mutex_assert_owner(&LOCK_trn_list);
  return ++global_trid_generator;
}

static void set_short_trid(TRN *trn)
{
  int i= (global_trid_generator + (intptr)trn) * 312089 % SHORT_TRID_MAX + 1;
  my_atomic_rwlock_wrlock(&LOCK_short_trid_to_trn);
  for ( ; ; i= i % SHORT_TRID_MAX + 1) /* the range is [1..SHORT_TRID_MAX] */
  {
    void *tmp= NULL;
    if (short_trid_to_active_trn[i] == NULL &&
        my_atomic_casptr((void **)&short_trid_to_active_trn[i], &tmp, trn))
      break;
  }
  my_atomic_rwlock_wrunlock(&LOCK_short_trid_to_trn);
  trn->short_id= i;
}

/*
  DESCRIPTION
    start a new transaction, allocate and initialize transaction object
    mutex and cond will be used for lock waits
*/
TRN *trnman_new_trn(pthread_mutex_t *mutex, pthread_cond_t *cond)
{
  TRN *trn;

  /*
    we have a mutex, to do simple things under it - allocate a TRN,
    increment trnman_active_transactions, set trn->min_read_from.

    Note that all the above is fast. generating short_trid may be slow,
    as it involves scanning a large array - so it's done outside of the
    mutex.
  */

  pthread_mutex_lock(&LOCK_trn_list);

  /* Allocating a new TRN structure */
  trn= pool;
  /*
    Popping an unused TRN from the pool
    (ABA isn't possible, we're behind a mutex
  */
  my_atomic_rwlock_wrlock(&LOCK_pool);
  while (trn && !my_atomic_casptr((void **)&pool, (void **)&trn,
                                  (void *)trn->next))
    /* no-op */;
  my_atomic_rwlock_wrunlock(&LOCK_pool);

  /* Nothing in the pool ? Allocate a new one */
  if (!trn)
  {
    trn= (TRN *)my_malloc(sizeof(TRN), MYF(MY_WME));
    if (unlikely(!trn))
    {
      pthread_mutex_unlock(&LOCK_trn_list);
      return 0;
    }
    trnman_allocated_transactions++;
  }
  trnman_active_transactions++;

  trn->min_read_from= active_list_min.next->trid;

  trn->trid= new_trid();
  trn->short_id= 0;

  trn->next= &active_list_max;
  trn->prev= active_list_max.prev;
  active_list_max.prev= trn->prev->next= trn;
  pthread_mutex_unlock(&LOCK_trn_list);

  trn->pins= lf_hash_get_pins(&trid_to_committed_trn);

  if (unlikely(!trn->min_read_from))
    trn->min_read_from= trn->trid;

  trn->commit_trid= 0;

  trn->locks.mutex= mutex;
  trn->locks.cond= cond;
  trn->locks.waiting_for= 0;
  trn->locks.all_locks= 0;
  trn->locks.pins= lf_alloc_get_pins(&maria_lockman.alloc);

  /*
    only after the following function TRN is considered initialized,
    so it must be done the last
  */
  set_short_trid(trn);

  return trn;
}

/*
  remove a trn from the active list.
  if necessary - move to committed list and set commit_trid

  NOTE
    Locks are released at the end. In particular, after placing the
    transaction in commit list, and after setting commit_trid. It's
    important, as commit_trid affects visibility.  Locks don't affect
    anything they simply delay execution of other threads - they could be
    released arbitrarily late. In other words, when locks are released it
    serves as a start banner for other threads, they start to run. So
    everything they may need must be ready at that point.
*/
void trnman_end_trn(TRN *trn, my_bool commit)
{
  TRN *free_me= 0;
  LF_PINS *pins= trn->pins;

  pthread_mutex_lock(&LOCK_trn_list);

  /* remove from active list */
  trn->next->prev= trn->prev;
  trn->prev->next= trn->next;

  /*
    if trn was the oldest active transaction, now that it goes away there
    may be committed transactions in the list which no active transaction
    needs to bother about - clean up the committed list
  */
  if (trn->prev == &active_list_min)
  {
    uint free_me_count;
    TRN *t;
    for (t= committed_list_min.next, free_me_count= 0;
         t->commit_trid < active_list_min.next->min_read_from;
         t= t->next, free_me_count++) /* no-op */;

    DBUG_ASSERT((t != committed_list_min.next && free_me_count > 0) ||
                (t == committed_list_min.next && free_me_count == 0));
    /* found transactions committed before the oldest active one */
    if (t != committed_list_min.next)
    {
      free_me= committed_list_min.next;
      committed_list_min.next= t;
      t->prev->next= 0;
      t->prev= &committed_list_min;
      trnman_committed_transactions-= free_me_count;
    }
  }

  /*
    if transaction is committed and it was not the only active transaction -
    add it to the committed list (which is used for read-from relation)
  */
  if (commit && active_list_min.next != &active_list_max)
  {
    int res;

    trn->commit_trid= global_trid_generator;
    trn->next= &committed_list_max;
    trn->prev= committed_list_max.prev;
    committed_list_max.prev= trn->prev->next= trn;
    trnman_committed_transactions++;

    res= lf_hash_insert(&trid_to_committed_trn, pins, &trn);
    DBUG_ASSERT(res == 0);
  }
  else /* otherwise free it right away */
  {
    trn->next= free_me;
    free_me= trn;
  }
  trnman_active_transactions--;
  pthread_mutex_unlock(&LOCK_trn_list);

  /* the rest is done outside of a critical section */
  lockman_release_locks(&maria_lockman, &trn->locks);
  trn->locks.mutex= 0;
  trn->locks.cond= 0;
  my_atomic_rwlock_rdlock(&LOCK_short_trid_to_trn);
  my_atomic_storeptr((void **)&short_trid_to_active_trn[trn->short_id], 0);
  my_atomic_rwlock_rdunlock(&LOCK_short_trid_to_trn);

  /*
    we, under the mutex, removed going-in-free_me transactions from the
    active and committed lists, thus nobody else may see them when it scans
    those lists, and thus nobody may want to free them. Now we don't
    need a mutex to access free_me list
  */
  /* QQ: send them to the purge thread */
  while (free_me)
  {
    TRN *t= free_me;
    free_me= free_me->next;

    lf_hash_delete(&trid_to_committed_trn, pins, &t->trid, sizeof(TrID));

    trnman_free_trn(t);
  }

  lf_hash_put_pins(pins);
  lf_pinbox_put_pins(trn->locks.pins);
}

/*
  free a trn (add to the pool, that is)
  note - we can never really free() a TRN if there's at least one other
  running transaction - see, e.g., how lock waits are implemented in
  lockman.c
  The same is true for other lock-free data structures too. We may need some
  kind of FLUSH command to reset them all - ensuring that no transactions are
  running. It may even be called automatically on checkpoints if no
  transactions are running.
*/
void trnman_free_trn(TRN *trn)
{
  TRN *tmp= pool;

  my_atomic_rwlock_wrlock(&LOCK_pool);
  do
  {
    /*
      without this volatile cast gcc-3.4.4 moved the assignment
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
    return TRUE; /* can read */
  if (trid > trn->trid)
    return FALSE; /* cannot read */

  found= lf_hash_search(&trid_to_committed_trn, trn->pins, &trid, sizeof(trid));
  if (!found)
    return FALSE; /* not in the hash of committed transactions = cannot read */

  can= (*found)->commit_trid < trn->trid;
  lf_unpin(trn->pins, 2);
  return can;
}


/*
  Allocates two buffers and stores in them some information about transactions
  of the active list (into the first buffer) and of the committed list (into
  the second buffer).

  SYNOPSIS
    trnman_collect_transactions()
    str_act    (OUT) pointer to a LEX_STRING where the allocated buffer, and
               its size, will be put
    str_com    (OUT) pointer to a LEX_STRING where the allocated buffer, and
               its size, will be put


  DESCRIPTION
    Does the allocation because the caller cannot know the size itself.
    Memory freeing is to be done by the caller (if the "str" member of the
    LEX_STRING is not NULL).
    The caller has the intention of doing checkpoints.

  RETURN
    0 on success
    1 on error
*/
my_bool trnman_collect_transactions(LEX_STRING *str_act, LEX_STRING *str_com)
{
  my_bool error;
  TRN *trn;
  char *ptr;
  DBUG_ENTER("trnman_collect_transactions");

  DBUG_ASSERT((NULL == str_act->str) && (NULL == str_com->str));

  pthread_mutex_lock(&LOCK_trn_list);
  str_act->length= 8+(6+2+7+7+7)*trnman_active_transactions;
  str_com->length= 8+(6+7+7)*trnman_committed_transactions;
  if ((NULL == (str_act->str= my_malloc(str_act->length, MYF(MY_WME)))) ||
      (NULL == (str_com->str= my_malloc(str_com->length, MYF(MY_WME)))))
    goto err;
  /* First, the active transactions */
  ptr= str_act->str;
  int8store(ptr, (ulonglong)trnman_active_transactions);
  ptr+= 8;
  for (trn= active_list_min.next; trn != &active_list_max; trn= trn->next)
  {
    /*
      trns with a short trid of 0 are not initialized; Recovery will recognize
      this and ignore them.
      State is not needed for now (only when we supported prepared trns).
      For LSNs, Sanja will soon push lsn7store.
    */
    int6store(ptr, trn->trid);
    ptr+= 6;
    int2store(ptr, trn->short_id);
    ptr+= 2;
    /* needed for rollback */
    /* lsn7store(ptr, trn->undo_lsn); */
    ptr+= 7;
    /* needed for purge */
    /* lsn7store(ptr, trn->undo_purge_lsn); */
    ptr+= 7;
    /* needed for low-water mark calculation */
    /* lsn7store(ptr, read_non_atomic(&trn->first_undo_lsn)); */
    ptr+= 7;
  }
  /* do the same for committed ones */
  ptr= str_com->str;
  int8store(ptr, (ulonglong)trnman_committed_transactions);
  ptr+= 8;
  for (trn= committed_list_min.next; trn != &committed_list_max;
       trn= trn->next)
  {
    int6store(ptr, trn->trid);
    ptr+= 6;
    /* mi_int7store(ptr, trn->undo_purge_lsn); */
    ptr+= 7;
    /* mi_int7store(ptr, read_non_atomic(&trn->first_undo_lsn)); */
    ptr+= 7;
  }
  /*
    TODO: if we see there exists no transaction (active and committed) we can
    tell the lock-free structures to do some freeing (my_free()).
  */
  error= 0;
  goto end;
err:
  error= 1;
end:
  pthread_mutex_unlock(&LOCK_trn_list);
  DBUG_RETURN(error);
}
