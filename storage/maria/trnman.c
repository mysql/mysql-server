/* Copyright (C) 2006-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

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
#include <m_string.h>
#include "trnman.h"
#include "ma_checkpoint.h"
#include "ma_control_file.h"

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

/*
  The minimum existing transaction id for trnman_get_min_trid()
  The default value is used when transaction manager not initialize;
  Probably called from maria_chk
*/
static TrID trid_min_read_from= MAX_TRID;

/* the mutex for everything above */
static pthread_mutex_t LOCK_trn_list;

/* LIFO pool of unused TRN structured for reuse */
static TRN *pool;

/* a hash for committed transactions that maps trid to a TRN structure */
static LF_HASH trid_to_trn;

/* an array that maps short_id of an active transaction to a TRN structure */
static TRN **short_trid_to_active_trn;

/* locks for short_trid_to_active_trn and pool */
static my_atomic_rwlock_t LOCK_short_trid_to_trn, LOCK_pool;
static my_bool default_trnman_end_trans_hook(TRN *, my_bool, my_bool);
static void trnman_free_trn(TRN *);

my_bool (*trnman_end_trans_hook)(TRN *, my_bool, my_bool)=
  default_trnman_end_trans_hook;

/*
  Simple interface functions
  QQ: if they stay so simple, should we make them inline?
*/

uint trnman_increment_locked_tables(TRN *trn)
{
  return trn->locked_tables++;
}

uint trnman_has_locked_tables(TRN *trn)
{
  return trn->locked_tables;
}

uint trnman_decrement_locked_tables(TRN *trn)
{
  return --trn->locked_tables;
}

void trnman_reset_locked_tables(TRN *trn, uint locked_tables)
{
  trn->locked_tables= locked_tables;
}

#ifdef EXTRA_DEBUG
uint16 trnman_get_flags(TRN *trn)
{
  return trn->flags;
}

void trnman_set_flags(TRN *trn, uint16 flags)
{
  trn->flags= flags;
}
#endif

/** Wake up threads waiting for this transaction */
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

static my_bool
default_trnman_end_trans_hook(TRN *trn __attribute__ ((unused)),
                              my_bool commit __attribute__ ((unused)),
                              my_bool active_transactions
                              __attribute__ ((unused)))
{
  return 0;
}


static uchar *trn_get_hash_key(const uchar *trn, size_t *len,
                              my_bool unused __attribute__ ((unused)))
{
  *len= sizeof(TrID);
  return (uchar *) & ((*((TRN **)trn))->trid);
}


/**
   @brief Initializes transaction manager.

   @param  initial_trid        Generated TrIDs will start from initial_trid+1.

   @return Operation status
     @retval 0      OK
     @retval !=0    Error
*/

int trnman_init(TrID initial_trid)
{
  DBUG_ENTER("trnman_init");
  DBUG_PRINT("enter", ("initial_trid: %lu", (ulong) initial_trid));

  short_trid_to_active_trn= (TRN **)my_malloc(SHORT_TRID_MAX*sizeof(TRN*),
                                     MYF(MY_WME|MY_ZEROFILL));
  if (unlikely(!short_trid_to_active_trn))
    DBUG_RETURN(1);
  short_trid_to_active_trn--; /* min short_id is 1 */

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
  active_list_max.min_read_from= MAX_TRID;
  active_list_max.next= active_list_min.prev= 0;
  active_list_max.prev= &active_list_min;
  active_list_min.next= &active_list_max;

  committed_list_max.commit_trid= MAX_TRID;
  committed_list_max.next= committed_list_min.prev= 0;
  committed_list_max.prev= &committed_list_min;
  committed_list_min.next= &committed_list_max;

  trnman_active_transactions= 0;
  trnman_committed_transactions= 0;
  trnman_allocated_transactions= 0;
  /* This is needed for recovery and repair */
  dummy_transaction_object.min_read_from= ~(TrID) 0;

  pool= 0;
  global_trid_generator= initial_trid;
  trid_min_read_from= initial_trid;
  lf_hash_init(&trid_to_trn, sizeof(TRN*), LF_HASH_UNIQUE,
               0, 0, trn_get_hash_key, 0);
  DBUG_PRINT("info", ("pthread_mutex_init LOCK_trn_list"));
  pthread_mutex_init(&LOCK_trn_list, MY_MUTEX_INIT_FAST);
  my_atomic_rwlock_init(&LOCK_short_trid_to_trn);
  my_atomic_rwlock_init(&LOCK_pool);

  DBUG_RETURN(0);
}

/*
  NOTE
    this could only be called in the "idle" state - no transaction can be
    running. See asserts below.
*/
void trnman_destroy()
{
  DBUG_ENTER("trnman_destroy");

  if (short_trid_to_active_trn == NULL) /* trnman already destroyed */
    DBUG_VOID_RETURN;
  DBUG_ASSERT(trid_to_trn.count == 0);
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
    DBUG_ASSERT(trn->wt == NULL);
    pthread_mutex_destroy(&trn->state_lock);
    my_free((void *)trn, MYF(0));
  }
  lf_hash_destroy(&trid_to_trn);
  DBUG_PRINT("info", ("pthread_mutex_destroy LOCK_trn_list"));
  pthread_mutex_destroy(&LOCK_trn_list);
  my_atomic_rwlock_destroy(&LOCK_short_trid_to_trn);
  my_atomic_rwlock_destroy(&LOCK_pool);
  my_free((void *)(short_trid_to_active_trn+1), MYF(0));
  short_trid_to_active_trn= NULL;

  DBUG_VOID_RETURN;
}

/*
  NOTE
    TrID is limited to 6 bytes. Initial value of the generator
    is set by the recovery code - being read from the last checkpoint
    (or 1 on a first run).
*/
static TrID new_trid()
{
  DBUG_ENTER("new_trid");
  DBUG_ASSERT(global_trid_generator < 0xffffffffffffLL);
  DBUG_PRINT("info", ("safe_mutex_assert_owner LOCK_trn_list"));
  safe_mutex_assert_owner(&LOCK_trn_list);
  DBUG_RETURN(++global_trid_generator);
}

static uint get_short_trid(TRN *trn)
{
  int i= (int) ((global_trid_generator + (intptr)trn) * 312089 %
                SHORT_TRID_MAX) + 1;
  uint res=0;

  for ( ; !res ; i= 1)
  {
    my_atomic_rwlock_wrlock(&LOCK_short_trid_to_trn);
    for ( ; i <= SHORT_TRID_MAX; i++) /* the range is [1..SHORT_TRID_MAX] */
    {
      void *tmp= NULL;
      if (short_trid_to_active_trn[i] == NULL &&
          my_atomic_casptr((void **)&short_trid_to_active_trn[i], &tmp, trn))
      {
        res= i;
        break;
      }
    }
    my_atomic_rwlock_wrunlock(&LOCK_short_trid_to_trn);
  }
  return res;
}

/**
  Allocates and initialzies a new TRN object

  @note the 'wt' parameter can only be 0 in a single-threaded code (or,
  generally, where threads cannot block each other), otherwise the
  first call to the deadlock detector will sigsegv.
*/

TRN *trnman_new_trn(WT_THD *wt)
{
  int res;
  TRN *trn;
  union { TRN *trn; void *v; } tmp;
  DBUG_ENTER("trnman_new_trn");

  /*
    we have a mutex, to do simple things under it - allocate a TRN,
    increment trnman_active_transactions, set trn->min_read_from.

    Note that all the above is fast. generating short_id may be slow,
    as it involves scanning a large array - so it's done outside of the
    mutex.
  */

  DBUG_PRINT("info", ("pthread_mutex_lock LOCK_trn_list"));
  pthread_mutex_lock(&LOCK_trn_list);

  /* Allocating a new TRN structure */
  tmp.trn= pool;
  /*
    Popping an unused TRN from the pool
    (ABA isn't possible, we're behind a mutex
  */
  my_atomic_rwlock_wrlock(&LOCK_pool);
  while (tmp.trn && !my_atomic_casptr((void **)(char*) &pool, &tmp.v,
                                      (void *)tmp.trn->next))
    /* no-op */;
  my_atomic_rwlock_wrunlock(&LOCK_pool);

  /* Nothing in the pool ? Allocate a new one */
  if (!(trn= tmp.trn))
  {
    /*
      trn should be completely initalized at create time to allow
      one to keep a known state on it.
      (Like redo_lns, which is assumed to be 0 at start of row handling
      and reset to zero before end of row handling)
    */
    trn= (TRN *)my_malloc(sizeof(TRN), MYF(MY_WME | MY_ZEROFILL));
    if (unlikely(!trn))
    {
      DBUG_PRINT("info", ("pthread_mutex_unlock LOCK_trn_list"));
      pthread_mutex_unlock(&LOCK_trn_list);
      return 0;
    }
    trnman_allocated_transactions++;
    pthread_mutex_init(&trn->state_lock, MY_MUTEX_INIT_FAST);
  }
  trn->wt= wt;
  trn->pins= lf_hash_get_pins(&trid_to_trn);
  if (!trn->pins)
  {
    trnman_free_trn(trn);
    pthread_mutex_unlock(&LOCK_trn_list);
    return 0;
  }

  trnman_active_transactions++;

  trn->min_read_from= active_list_min.next->trid;

  trn->trid= new_trid();

  trn->next= &active_list_max;
  trn->prev= active_list_max.prev;
  active_list_max.prev= trn->prev->next= trn;
  trid_min_read_from= active_list_min.next->min_read_from;
  DBUG_PRINT("info", ("pthread_mutex_unlock LOCK_trn_list"));
  pthread_mutex_unlock(&LOCK_trn_list);

  if (unlikely(!trn->min_read_from))
  {
    /*
      We are the only transaction. Set min_read_from so that we can read
      our own rows
    */
    trn->min_read_from= trn->trid + 1;
  }

  /* no other transaction can read changes done by this one */
  trn->commit_trid=  MAX_TRID;
  trn->rec_lsn= trn->undo_lsn= trn->first_undo_lsn= 0;
  trn->used_tables= 0;

  trn->locked_tables= 0;
  trn->flags= 0;

  /*
    only after the following function TRN is considered initialized,
    so it must be done the last
  */
  pthread_mutex_lock(&trn->state_lock);
  trn->short_id= get_short_trid(trn);
  pthread_mutex_unlock(&trn->state_lock);

  res= lf_hash_insert(&trid_to_trn, trn->pins, &trn);
  DBUG_ASSERT(res <= 0);
  if (res)
  {
    trnman_end_trn(trn, 0);
    return 0;
  }

  DBUG_PRINT("exit", ("trn: 0x%lx  trid: 0x%lu",
                      (ulong) trn, (ulong) trn->trid));

  DBUG_RETURN(trn);
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

  RETURN
    0  ok
    1  error
*/
my_bool trnman_end_trn(TRN *trn, my_bool commit)
{
  int res= 1;
  uint16 cached_short_id= trn->short_id; /* we have to cache it, see below */
  TRN *free_me= 0;
  LF_PINS *pins= trn->pins;
  DBUG_ENTER("trnman_end_trn");
  DBUG_PRINT("enter", ("trn=0x%lx commit=%d", (ulong) trn, commit));

  /* if a rollback, all UNDO records should have been executed */
  DBUG_ASSERT(commit || trn->undo_lsn == 0);
  DBUG_ASSERT(trn != &dummy_transaction_object);
  DBUG_PRINT("info", ("pthread_mutex_lock LOCK_trn_list"));

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

  pthread_mutex_lock(&trn->state_lock);
  if (commit)
    trn->commit_trid= global_trid_generator;
  wt_thd_release_self(trn);
  pthread_mutex_unlock(&trn->state_lock);

  /*
    if transaction is committed and it was not the only active transaction -
    add it to the committed list
  */
  if (commit && active_list_min.next != &active_list_max)
  {
    trn->next= &committed_list_max;
    trn->prev= committed_list_max.prev;
    trnman_committed_transactions++;
    committed_list_max.prev= trn->prev->next= trn;
  }
  else
  {
    trn->next= free_me;
    free_me= trn;
  }
  trid_min_read_from= active_list_min.next->min_read_from;

  if ((*trnman_end_trans_hook)(trn, commit,
                               active_list_min.next != &active_list_max))
    res= -1;
  trnman_active_transactions--;

  DBUG_PRINT("info", ("pthread_mutex_unlock LOCK_trn_list"));
  pthread_mutex_unlock(&LOCK_trn_list);

  /*
    the rest is done outside of a critical section

    note that we don't own trn anymore, it may be in a shared list now.
    Thus, we cannot dereference it, and must use cached_short_id below.
  */
  my_atomic_rwlock_rdlock(&LOCK_short_trid_to_trn);
  my_atomic_storeptr((void **)&short_trid_to_active_trn[cached_short_id], 0);
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

    /* ignore OOM. it's harmless, and we can do nothing here anyway */
    (void)lf_hash_delete(&trid_to_trn, pins, &t->trid, sizeof(TrID));

    trnman_free_trn(t);
  }

  lf_hash_put_pins(pins);

  DBUG_RETURN(res < 0);
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
static void trnman_free_trn(TRN *trn)
{
  /*
     union is to solve strict aliasing issue.
     without it gcc 3.4.3 doesn't notice that updating *(void **)&tmp
     modifies the value of tmp.
  */
  union { TRN *trn; void *v; } tmp;

  pthread_mutex_lock(&trn->state_lock);
  trn->short_id= 0;
  pthread_mutex_unlock(&trn->state_lock);

  tmp.trn= pool;

  my_atomic_rwlock_wrlock(&LOCK_pool);
  do
  {
    /*
      without this volatile cast gcc-3.4.4 moves the assignment
      down after the loop at -O2
    */
    *(TRN * volatile *)&(trn->next)= tmp.trn;
  } while (!my_atomic_casptr((void **)(char*)&pool, &tmp.v, trn));
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

  RETURN
    1   can
    0   cannot
   -1   error (OOM)
*/
int trnman_can_read_from(TRN *trn, TrID trid)
{
  TRN **found;
  my_bool can;
  LF_REQUIRE_PINS(3);

  if (trid < trn->min_read_from)
    return 1; /* Row is visible by all transactions in the system */

  if (trid >= trn->trid)
  {
    /*
      We have now two cases
      trid > trn->trid, in which case the row is from a new transaction
      and not visible, in which case we should return 0.
      trid == trn->trid in which case the row is from the current transaction
      and we should return 1
    */
    return trid == trn->trid;
  }

  found= lf_hash_search(&trid_to_trn, trn->pins, &trid, sizeof(trid));
  if (found == NULL)
    return 0; /* not in the hash of transactions = cannot read */
  if (found == MY_ERRPTR)
    return -1;

  can= (*found)->commit_trid < trn->trid;
  lf_hash_search_unpin(trn->pins);
  return can;
}

/**
  Finds a TRN by its TrID

  @param trn    current trn. Needed for pinning pointers (see lf_pin)
  @param trid   trid to search for

  @return found trn or 0

  @note that trn is returned with its state locked!
*/
TRN *trnman_trid_to_trn(TRN *trn, TrID trid)
{
  TRN **found;
  LF_REQUIRE_PINS(3);

  if (trid < trn->min_read_from)
    return 0; /* it's committed eons ago */

  found= lf_hash_search(&trid_to_trn, trn->pins, &trid, sizeof(trid));
  if (found == NULL || found == MY_ERRPTR)
    return 0; /* no luck */

  /* we've found something */
  pthread_mutex_lock(&(*found)->state_lock);

  if ((*found)->short_id == 0)
  {
    pthread_mutex_unlock(&(*found)->state_lock);
    lf_hash_search_unpin(trn->pins);
    return 0; /* but it was a ghost */
  }
  lf_hash_search_unpin(trn->pins);

  /* Gotcha! */
  return *found;
}

/* TODO: the stubs below are waiting for savepoints to be implemented */

void trnman_new_statement(TRN *trn __attribute__ ((unused)))
{
}

void trnman_rollback_statement(TRN *trn __attribute__ ((unused)))
{
}


/**
   @brief Allocates buffers and stores in them some info about transactions

   Does the allocation because the caller cannot know the size itself.
   Memory freeing is to be done by the caller (if the "str" member of the
   LEX_STRING is not NULL).
   The caller has the intention of doing checkpoints.

   @param[out]  str_act    pointer to where the allocated buffer,
                           and its size, will be put; buffer will be filled
                           with info about active transactions
   @param[out]  str_com    pointer to where the allocated buffer,
                           and its size, will be put; buffer will be filled
                           with info about committed transactions
   @param[out]  min_first_undo_lsn pointer to where the minimum
                           first_undo_lsn of all transactions will be put

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

my_bool trnman_collect_transactions(LEX_STRING *str_act, LEX_STRING *str_com,
                                    LSN *min_rec_lsn, LSN *min_first_undo_lsn)
{
  my_bool error;
  TRN *trn;
  char *ptr;
  uint stored_transactions= 0;
  LSN minimum_rec_lsn= LSN_MAX, minimum_first_undo_lsn= LSN_MAX;
  DBUG_ENTER("trnman_collect_transactions");

  DBUG_ASSERT((NULL == str_act->str) && (NULL == str_com->str));

  /* validate the use of read_non_atomic() in general: */
  compile_time_assert((sizeof(LSN) == 8) && (sizeof(LSN_WITH_FLAGS) == 8));
  pthread_mutex_lock(&LOCK_trn_list);
  str_act->length= 2 + /* number of active transactions */
    LSN_STORE_SIZE + /* minimum of their rec_lsn */
    TRANSID_SIZE + /* current TrID generator value */
    (2 + /* short id */
     6 + /* long id */
     LSN_STORE_SIZE + /* undo_lsn */
#ifdef MARIA_VERSIONING /* not enabled yet */
     LSN_STORE_SIZE + /* undo_purge_lsn */
#endif
     LSN_STORE_SIZE /* first_undo_lsn */
     ) * trnman_active_transactions;
  str_com->length= 4 + /* number of committed transactions */
    (6 + /* long id */
#ifdef MARIA_VERSIONING /* not enabled yet */
     LSN_STORE_SIZE + /* undo_purge_lsn */
#endif
     LSN_STORE_SIZE /* first_undo_lsn */
     ) * trnman_committed_transactions;
  if ((NULL == (str_act->str= my_malloc(str_act->length, MYF(MY_WME)))) ||
      (NULL == (str_com->str= my_malloc(str_com->length, MYF(MY_WME)))))
    goto err;
  /* First, the active transactions */
  ptr= str_act->str + 2 + LSN_STORE_SIZE;
  transid_store(ptr, global_trid_generator);
  ptr+= TRANSID_SIZE;
  for (trn= active_list_min.next; trn != &active_list_max; trn= trn->next)
  {
    uint sid;
    LSN rec_lsn, undo_lsn, first_undo_lsn;
    pthread_mutex_lock(&trn->state_lock);
    sid= trn->short_id;
    pthread_mutex_unlock(&trn->state_lock);
    if (sid == 0)
    {
      /*
        Not even inited, has done nothing. Or it is the
        dummy_transaction_object, which does only non-transactional
        immediate-sync operations (CREATE/DROP/RENAME/REPAIR TABLE), and so
        can be forgotten for Checkpoint.
      */
      continue;
    }
    /* needed for low-water mark calculation */
    if (((rec_lsn= lsn_read_non_atomic(trn->rec_lsn)) > 0) &&
        (cmp_translog_addr(rec_lsn, minimum_rec_lsn) < 0))
      minimum_rec_lsn= rec_lsn;
    /*
      If trn has not logged LOGREC_LONG_TRANSACTION_ID, this trn will be
      discovered when seeing that log record which is for sure located after
      checkpoint_start_log_horizon.
    */
    if ((LSN_WITH_FLAGS_TO_FLAGS(trn->first_undo_lsn) &
         TRANSACTION_LOGGED_LONG_ID) == 0)
      continue;
    /*
      On the other hand, if undo_lsn is LSN_IMPOSSIBLE, trn may later log
      records; so we must include trn in the checkpoint now, because we cannot
      count on LOGREC_LONG_TRANSACTION_ID (as we are already past it).
    */
    undo_lsn= trn->undo_lsn;
    stored_transactions++;
    int2store(ptr, sid);
    ptr+= 2;
    int6store(ptr, trn->trid);
    ptr+= 6;
    lsn_store(ptr, undo_lsn); /* needed for rollback */
    ptr+= LSN_STORE_SIZE;
    /* needed for low-water mark calculation */
    if (((first_undo_lsn= lsn_read_non_atomic(trn->first_undo_lsn)) > 0) &&
        (cmp_translog_addr(first_undo_lsn, minimum_first_undo_lsn) < 0))
      minimum_first_undo_lsn= first_undo_lsn;
    lsn_store(ptr, first_undo_lsn);
    ptr+= LSN_STORE_SIZE;
#ifdef MARIA_VERSIONING /* not enabled yet */
    /* to know where purging should start (last delete of this trn) */
    lsn_store(ptr, trn->undo_purge_lsn);
    ptr+= LSN_STORE_SIZE;
#endif
    /**
       @todo RECOVERY: add a comment explaining why we can dirtily read some
       vars, inspired by the text of "assumption 8" in WL#3072
    */
  }
  str_act->length= ptr - str_act->str; /* as we maybe over-estimated */
  ptr= str_act->str;
  DBUG_PRINT("info",("collected %u active transactions",
                     (uint)stored_transactions));
  int2store(ptr, stored_transactions);
  ptr+= 2;
  /* this LSN influences how REDOs for any page can be ignored by Recovery */
  lsn_store(ptr, minimum_rec_lsn);
  /* one day there will also be a list of prepared transactions */
  /* do the same for committed ones */
  ptr= str_com->str;
  int4store(ptr, trnman_committed_transactions);
  ptr+= 4;
  DBUG_PRINT("info",("collected %u committed transactions",
                     (uint)trnman_committed_transactions));
  for (trn= committed_list_min.next; trn != &committed_list_max;
       trn= trn->next)
  {
    LSN first_undo_lsn;
    int6store(ptr, trn->trid);
    ptr+= 6;
#ifdef MARIA_VERSIONING /* not enabled yet */
    lsn_store(ptr, trn->undo_purge_lsn);
    ptr+= LSN_STORE_SIZE;
#endif
    first_undo_lsn= LSN_WITH_FLAGS_TO_LSN(trn->first_undo_lsn);
    if (cmp_translog_addr(first_undo_lsn, minimum_first_undo_lsn) < 0)
      minimum_first_undo_lsn= first_undo_lsn;
    lsn_store(ptr, first_undo_lsn);
    ptr+= LSN_STORE_SIZE;
  }
  /*
    TODO: if we see there exists no transaction (active and committed) we can
    tell the lock-free structures to do some freeing (my_free()).
  */
  error= 0;
  *min_rec_lsn= minimum_rec_lsn;
  *min_first_undo_lsn= minimum_first_undo_lsn;
  goto end;
err:
  error= 1;
end:
  pthread_mutex_unlock(&LOCK_trn_list);
  DBUG_RETURN(error);
}


TRN *trnman_recreate_trn_from_recovery(uint16 shortid, TrID longid)
{
  TrID old_trid_generator= global_trid_generator;
  TRN *trn;
  DBUG_ASSERT(maria_in_recovery && !maria_multi_threaded);
  global_trid_generator= longid-1; /* force a correct trid in the new trn */
  if (unlikely((trn= trnman_new_trn(NULL)) == NULL))
    return NULL;
  /* deallocate excessive allocations of trnman_new_trn() */
  global_trid_generator= old_trid_generator;
  set_if_bigger(global_trid_generator, longid);
  short_trid_to_active_trn[trn->short_id]= 0;
  DBUG_ASSERT(short_trid_to_active_trn[shortid] == NULL);
  short_trid_to_active_trn[shortid]= trn;
  trn->short_id= shortid;
  return trn;
}


TRN *trnman_get_any_trn()
{
  TRN *trn= active_list_min.next;
  return (trn != &active_list_max) ? trn : NULL;
}


/**
  Returns the minimum existing transaction id. May return a too small
  number in race conditions, but this is ok as the value is used to
  remove not visible transid from index/rows.
*/

TrID trnman_get_min_trid()
{
  return trid_min_read_from;
}


/**
  Returns the minimum possible transaction id

  @notes
  If there is no transactions running, returns number for next running
  transaction.
  If one has an active transaction, the returned number will be less or
  equal to this.  If one is not running in a transaction one will ge the
  number for the next started transaction.  This is used in create table
  to get a safe minimum trid to use.
*/

TrID trnman_get_min_safe_trid()
{
  TrID trid;
  pthread_mutex_lock(&LOCK_trn_list);
  trid= min(active_list_min.next->min_read_from,
            global_trid_generator);
  pthread_mutex_unlock(&LOCK_trn_list);
  return trid;
}


/**
  Returns maximum transaction id given to a transaction so far.
*/

TrID trnman_get_max_trid()
{
  TrID id;
  if (short_trid_to_active_trn == NULL)
    return 0;
  pthread_mutex_lock(&LOCK_trn_list);
  id= global_trid_generator;
  pthread_mutex_unlock(&LOCK_trn_list);
  return id;
}

/**
  @brief Check if there exist an active transaction between two commit_id's

  @todo
    Improve speed of this.
      - Store transactions in tree or skip list
      - Have function to copying all active transaction id's to b-tree
        and use b-tree for checking states.  This could be a big win
        for checkpoint that will call this function for a lot of objects.

  @return
    0   No transaction exists
    1   There is at least on active transaction in the given range
*/

my_bool trnman_exists_active_transactions(TrID min_id, TrID max_id,
                                          my_bool trnman_is_locked)
{
  TRN *trn;
  my_bool ret= 0;

  if (!trnman_is_locked)
    pthread_mutex_lock(&LOCK_trn_list);
  safe_mutex_assert_owner(&LOCK_trn_list);
  for (trn= active_list_min.next; trn != &active_list_max; trn= trn->next)
  {
    /*
      We use <= for max_id as max_id is a commit_trid and trn->trid
      is transaction id.  When calculating commit_trid we use the
      current value of global_trid_generator.  global_trid_generator is
      incremented for each new transaction.

      For example, assuming we have
      min_id = 5
      max_id = 10

      A trid of value 5 can't see the history event between 5 & 10
      at it vas started before min_id 5 was committed.
      A trid of value 10 can't see the next history event (max_id = 10)
      as it started before this was committed. In this case it must use
      the this event.
    */
    if (trn->trid > min_id && trn->trid <= max_id)
    {
      ret= 1;
      break;
    }
  }
  if (!trnman_is_locked)
    pthread_mutex_unlock(&LOCK_trn_list);
  return ret;
}


/**
   lock transaction list
*/

void trnman_lock()
{
  pthread_mutex_lock(&LOCK_trn_list);
}


/**
   unlock transaction list
*/

void trnman_unlock()
{
  pthread_mutex_unlock(&LOCK_trn_list);
}


/**
  Is trman initialized
*/

my_bool trman_is_inited()
{
  return (short_trid_to_active_trn != NULL);
}
