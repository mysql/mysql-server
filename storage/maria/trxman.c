
#include <my_global.h>
#include <my_sys.h>
#include <lf.h>
#include "trxman.h"

TRX active_list_min, active_list_max,
       committed_list_min, committed_list_max, *pool;

pthread_mutex_t LOCK_trx_list;
uint trxman_active_transactions, trxman_allocated_transactions;
TrID global_trid_generator;

TRX **short_id_to_trx;
my_atomic_rwlock_t LOCK_short_id_to_trx;

LF_HASH trid_to_trx;

static byte *trx_get_hash_key(const byte *trx,uint* len, my_bool unused)
{
  *len= sizeof(TrID);
  return (byte *) & ((*((TRX **)trx))->trid);
}

int trxman_init()
{
  pthread_mutex_init(&LOCK_trx_list, MY_MUTEX_INIT_FAST);
  active_list_max.trid= active_list_min.trid= 0;
  active_list_max.min_read_from=~0;
  active_list_max.next= active_list_min.prev= 0;
  active_list_max.prev= &active_list_min;
  active_list_min.next= &active_list_max;
  trxman_active_transactions= 0;
  trxman_allocated_transactions= 0;

  committed_list_max.commit_trid= ~0;
  committed_list_max.next= committed_list_min.prev= 0;
  committed_list_max.prev= &committed_list_min;
  committed_list_min.next= &committed_list_max;

  pool=0;
  global_trid_generator=0; /* set later by recovery code */
  lf_hash_init(&trid_to_trx, sizeof(TRX*), LF_HASH_UNIQUE,
               0, 0, trx_get_hash_key, 0);
  my_atomic_rwlock_init(&LOCK_short_id_to_trx);
  short_id_to_trx=(TRX **)my_malloc(SHORT_ID_MAX*sizeof(TRX*),
                                     MYF(MY_WME|MY_ZEROFILL));
  if (!short_id_to_trx)
    return 1;
  short_id_to_trx--; /* min short_id is 1 */

  return 0;
}

int trxman_destroy()
{
  DBUG_ASSERT(trid_to_trx.count == 0);
  DBUG_ASSERT(trxman_active_transactions == 0);
  DBUG_ASSERT(active_list_max.prev == &active_list_min);
  DBUG_ASSERT(active_list_min.next == &active_list_max);
  DBUG_ASSERT(committed_list_max.prev == &committed_list_min);
  DBUG_ASSERT(committed_list_min.next == &committed_list_max);
  while (pool)
  {
    TRX *tmp=pool->next;
    my_free((void *)pool, MYF(0));
    pool=tmp;
  }
  lf_hash_destroy(&trid_to_trx);
  pthread_mutex_destroy(&LOCK_trx_list);
  my_atomic_rwlock_destroy(&LOCK_short_id_to_trx);
  my_free((void *)(short_id_to_trx+1), MYF(0));
}

static TrID new_trid()
{
  DBUG_ASSERT(global_trid_generator < 0xffffffffffffLL);
  safe_mutex_assert_owner(&LOCK_trx_list);
  return ++global_trid_generator;
}

static void set_short_id(TRX *trx)
{
  int i= (global_trid_generator + (intptr)trx) * 312089 % SHORT_ID_MAX;
  my_atomic_rwlock_wrlock(&LOCK_short_id_to_trx);
  for ( ; ; i= i % SHORT_ID_MAX + 1) /* the range is [1..SHORT_ID_MAX] */
  {
    void *tmp=NULL;
    if (short_id_to_trx[i] == NULL &&
        my_atomic_casptr((void **)&short_id_to_trx[i], &tmp, trx))
      break;
  }
  my_atomic_rwlock_wrunlock(&LOCK_short_id_to_trx);
  trx->short_id= i;
}

TRX *trxman_new_trx()
{
  TRX *trx;

  my_atomic_add32(&trxman_active_transactions, 1);

  /*
    see trxman_end_trx to see why we need a mutex here

    and as we have a mutex, we can as well do everything
    under it - allocating a TRX, incrementing trxman_active_transactions,
    setting trx->min_read_from.

    Note that all the above is fast. generating short_id may be slow,
    as it involves scanning a big array - so it's still done
    outside of the mutex.
  */

  pthread_mutex_lock(&LOCK_trx_list);
  trx=pool;
  while (trx && !my_atomic_casptr((void **)&pool, (void **)&trx, trx->next))
    /* no-op */;

  if (!trx)
  {
    trx=(TRX *)my_malloc(sizeof(TRX), MYF(MY_WME));
    trxman_allocated_transactions++;
  }
  if (!trx)
    return 0;

  trx->min_read_from= active_list_min.next->trid;

  trx->trid= new_trid();
  trx->short_id= 0;

  trx->next= &active_list_max;
  trx->prev= active_list_max.prev;
  active_list_max.prev= trx->prev->next= trx;
  pthread_mutex_unlock(&LOCK_trx_list);

  trx->pins=lf_hash_get_pins(&trid_to_trx);

  if (!trx->min_read_from)
    trx->min_read_from= trx->trid;

  trx->commit_trid=0;

  set_short_id(trx); /* this must be the last! */


  return trx;
}

/*
  remove a trx from the active list,
  move to committed list,
  set commit_trid

  TODO
    integrate with lock manager, log manager. That means:
    a common "commit" mutex - forcing the log and setting commit_trid
    must be done atomically (QQ how the heck it could be done with
    group commit ???)

    trid_to_trx, active_list_*, and committed_list_* can be
    updated asyncronously.
*/
void trxman_end_trx(TRX *trx, my_bool commit)
{
  int res;
  TRX *free_me= 0;
  LF_PINS *pins= trx->pins;

  pthread_mutex_lock(&LOCK_trx_list);
  trx->next->prev= trx->prev;
  trx->prev->next= trx->next;

  if (trx->prev == &active_list_min)
  {
    TRX *t;
    for (t= committed_list_min.next;
         t->commit_trid < active_list_min.next->min_read_from;
         t= t->next) /* no-op */;

    if (t != committed_list_min.next)
    {
      free_me= committed_list_min.next;
      committed_list_min.next= t;
      t->prev->next=0;
      t->prev= &committed_list_min;
    }
  }

  my_atomic_rwlock_wrlock(&LOCK_short_id_to_trx);
  my_atomic_storeptr((void **)&short_id_to_trx[trx->short_id], 0);
  my_atomic_rwlock_wrunlock(&LOCK_short_id_to_trx);

  if (commit && active_list_min.next != &active_list_max)
  {
    trx->commit_trid= global_trid_generator;

    trx->next= &committed_list_max;
    trx->prev= committed_list_max.prev;
    committed_list_max.prev= trx->prev->next= trx;

    res= lf_hash_insert(&trid_to_trx, pins, &trx);
    DBUG_ASSERT(res == 0);
  }
  else
  {
    trx->next=free_me;
    free_me=trx;
  }
  pthread_mutex_unlock(&LOCK_trx_list);

  my_atomic_add32(&trxman_active_transactions, -1);

  while (free_me)
  {
    int res;
    TRX *t= free_me;
    free_me= free_me->next;

    res= lf_hash_delete(&trid_to_trx, pins, &t->trid, sizeof(TrID));

    trxman_free_trx(t);
  }

  lf_hash_put_pins(pins);
}

/* free a trx (add to the pool, that is */
void trxman_free_trx(TRX *trx)
{
  TRX *tmp=pool;

  do
  {
    trx->next=tmp;
  } while (!my_atomic_casptr((void **)&pool, (void **)&tmp, trx));
}

my_bool trx_can_read_from(TRX *trx, TrID trid)
{
  TRX *found;
  my_bool can;

  if (trid < trx->min_read_from)
    return TRUE;
  if (trid > trx->trid)
    return FALSE;

  found= lf_hash_search(&trid_to_trx, trx->pins, &trid, sizeof(trid));
  if (!found)
    return FALSE; /* not in the hash = cannot read */

  can= found->commit_trid < trx->trid;
  lf_unpin(trx->pins, 2);
  return can;
}

