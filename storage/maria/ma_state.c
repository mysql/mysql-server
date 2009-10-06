/* Copyright (C) 2008 Sun AB and Michael Widenius

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
  Functions to maintain live statistics for Maria transactional tables
  and versioning for not transactional tables

  See WL#3138; Maria - fast "SELECT COUNT(*) FROM t;" and "CHECKSUM TABLE t"
  for details about live number of rows and live checksums

  TODO
   - Allocate MA_USED_TABLES and MA_HISTORY_STATE from a global pool (to
     avoid calls to malloc()
   - In trnamn_end_trans_hook(), don't call _ma_remove_not_visible_states()
     every time. One could for example call it if there has been more than
     10 ended transactions since last time it was called.
*/

#include "maria_def.h"
#include "trnman.h"
#include "ma_blockrec.h"

/**
   @brief Setup initial start-of-transaction state for a table

   @fn     _ma_setup_live_state
   @param info		Maria handler

   @notes
     This function ensures that trn->used_tables contains a list of
     start and live states for tables that are part of the transaction
     and that info->state points to the current live state for the table.

   @TODO
     Change trn->table_list to a hash and share->state_history to a binary tree

   @return
   @retval 0  ok
   @retval 1  error (out of memory)
*/

my_bool _ma_setup_live_state(MARIA_HA *info)
{
  TRN *trn= info->trn;
  MARIA_SHARE *share= info->s;
  MARIA_USED_TABLES *tables;
  MARIA_STATE_HISTORY *history;
  DBUG_ENTER("_ma_setup_live_state");

  for (tables= (MARIA_USED_TABLES*) info->trn->used_tables;
       tables;
       tables= tables->next)
  {
    if (tables->share == share)
    {
      /* Table is already used by transaction */
      goto end;
    }
  }
  /* Table was not used before, create new table state entry */
  if (!(tables= (MARIA_USED_TABLES*) my_malloc(sizeof(*tables),
                                               MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(1);
  tables->next= trn->used_tables;
  trn->used_tables= tables;
  tables->share= share;

  pthread_mutex_lock(&share->intern_lock);
  share->in_trans++;
  DBUG_PRINT("info", ("share: 0x%lx  in_trans: %d",
                      (ulong) share, share->in_trans));

  history= share->state_history;

  /*
    We must keep share locked to ensure that we don't access a history
    link that is deleted by concurrently running checkpoint.

    It's enough to compare trids here (instead of calling
    tranman_can_read_from) as history->trid is a commit_trid
  */
  while (trn->trid <= history->trid)
    history= history->next;
  pthread_mutex_unlock(&share->intern_lock);
  /* The current item can't be deleted as it's the first one visible for us */
  tables->state_start=  tables->state_current= history->state;
  tables->state_current.changed= tables->state_current.no_transid= 0;

  DBUG_PRINT("info", ("records: %ld", (ulong) tables->state_start.records));

end:
  info->state_start= &tables->state_start;
  info->state= &tables->state_current;

  /*
    Mark in transaction state if we are not using transid (versioning)
    on rows. If not, then we will in _ma_trnman_end_trans_hook()
    ensure that the state is visible for all at end of transaction
  */
  tables->state_current.no_transid|= !(info->row_flag & ROW_FLAG_TRANSID);

  DBUG_RETURN(0);
}


/**
   @brief Remove states that are not visible by anyone

   @fn   _ma_remove_not_visible_states()
   @param org_history    List to history
   @param all            1 if we should delete the first state if it's
                         visible for all.  For the moment this is only used
                         on close() of table.
   @param trnman_is_locked  Set to 1 if we have already a lock on trnman.

   @notes
     The assumption is that items in the history list is ordered by
     commit_trid.

     A state is not visible anymore if there is no new transaction
     that has been started between the commit_trid's of two states

     As long as some states exists, we keep the newest = (last commit)
     state as first state in the history.  This is to allow us to just move
     the history from the global list to the share when we open the table.

     Note that if 'all' is set trnman_is_locked must be 0, becasue
     trnman_get_min_trid() will take a lock on trnman.

   @return
   @retval Pointer to new history list
*/

MARIA_STATE_HISTORY
*_ma_remove_not_visible_states(MARIA_STATE_HISTORY *org_history,
                               my_bool all,
                               my_bool trnman_is_locked)
{
  TrID last_trid;
  MARIA_STATE_HISTORY *history, **parent, *next;
  DBUG_ENTER("_ma_remove_not_visible_states");

  if (!org_history)
    DBUG_RETURN(0);                          /* Not versioned table */

  last_trid= org_history->trid;
  parent= &org_history->next;
  for (history= org_history->next; history; history= next)
  {
    next= history->next;
    if (!trnman_exists_active_transactions(history->trid, last_trid,
                                           trnman_is_locked))
    {
      DBUG_PRINT("info", ("removing history->trid: %lu  next: %lu",
                          (ulong) history->trid, (ulong) last_trid));
      my_free(history, MYF(0));
      continue;
    }
    *parent= history;
    parent= &history->next;
    last_trid= history->trid;
  }
  *parent= 0;

  if (all && parent == &org_history->next)
  {
    /* There is only one state left. Delete this if it's visible for all */
    if (last_trid < trnman_get_min_trid())
    {
      my_free(org_history, MYF(0));
      org_history= 0;
    }
  }
  DBUG_RETURN(org_history);
}


/**
   @brief Remove not used state history

   @param share          Maria table information
   @param all            1 if we should delete the first state if it's
                         visible for all.  For the moment this is only used
                         on close() of table.

   @notes
   share and trnman are not locked.

   We must first lock trnman and then share->intern_lock. This is becasue
   _ma_trnman_end_trans_hook() has a lock on trnman and then
   takes share->intern_lock.
*/

void _ma_remove_not_visible_states_with_lock(MARIA_SHARE *share,
                                             my_bool all)
{
  my_bool is_lock_trman;
  if ((is_lock_trman= trman_is_inited()))
    trnman_lock();

  pthread_mutex_lock(&share->intern_lock);
  share->state_history=  _ma_remove_not_visible_states(share->state_history,
                                                       all, 1);
  pthread_mutex_unlock(&share->intern_lock);
  if (is_lock_trman)
    trnman_unlock();
}


/*
  Free state history information from share->history and reset information
  to current state.

  @notes
  Used after repair as then all rows are visible for everyone
*/

void _ma_reset_state(MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;
  MARIA_STATE_HISTORY *history= share->state_history;

  if (history)
  {
    MARIA_STATE_HISTORY *next;

    /* Set the current history to current state */
    share->state_history->state= share->state.state;
    /* Set current table handler to point to new history state */
    info->state= info->state_start= &share->state_history->state;
    for (history= history->next ; history ; history= next)
    {
      next= history->next;
      my_free(history, MYF(0));
    }
    share->state_history->next= 0;
    share->state_history->trid= 0;              /* Visibile for all */
  }
}


/****************************************************************************
  The following functions are called by thr_lock() in threaded applications
  for not transactional tables
****************************************************************************/

/*
  Create a copy of the current status for the table

  SYNOPSIS
    _ma_get_status()
    param		Pointer to Myisam handler
    concurrent_insert	Set to 1 if we are going to do concurrent inserts
			(THR_WRITE_CONCURRENT_INSERT was used)
*/

void _ma_get_status(void* param, my_bool concurrent_insert)
{
  MARIA_HA *info=(MARIA_HA*) param;
  DBUG_ENTER("_ma_get_status");
  DBUG_PRINT("info",("key_file: %ld  data_file: %ld  concurrent_insert: %d",
		     (long) info->s->state.state.key_file_length,
		     (long) info->s->state.state.data_file_length,
                     concurrent_insert));
#ifndef DBUG_OFF
  if (info->state->key_file_length > info->s->state.state.key_file_length ||
      info->state->data_file_length > info->s->state.state.data_file_length)
    DBUG_PRINT("warning",("old info:  key_file: %ld  data_file: %ld",
			  (long) info->state->key_file_length,
			  (long) info->state->data_file_length));
#endif
  info->state_save= info->s->state.state;
  info->state= &info->state_save;
  info->state->changed= 0;
  info->append_insert_at_end= concurrent_insert;
  DBUG_VOID_RETURN;
}


void _ma_update_status(void* param)
{
  MARIA_HA *info=(MARIA_HA*) param;
  /*
    Because someone may have closed the table we point at, we only
    update the state if its our own state.  This isn't a problem as
    we are always pointing at our own lock or at a read lock.
    (This is enforced by thr_multi_lock.c)
  */
  if (info->state == &info->state_save)
  {
    MARIA_SHARE *share= info->s;
#ifndef DBUG_OFF
    DBUG_PRINT("info",("updating status:  key_file: %ld  data_file: %ld",
		       (long) info->state->key_file_length,
		       (long) info->state->data_file_length));
    if (info->state->key_file_length < share->state.state.key_file_length ||
	info->state->data_file_length < share->state.state.data_file_length)
      DBUG_PRINT("warning",("old info:  key_file: %ld  data_file: %ld",
			    (long) share->state.state.key_file_length,
			    (long) share->state.state.data_file_length));
#endif
    /*
      we are going to modify the state without lock's log, this would break
      recovery if done with a transactional table.
    */
    DBUG_ASSERT(!info->s->base.born_transactional);
    share->state.state= *info->state;
    info->state= &share->state.state;
#ifdef HAVE_QUERY_CACHE
    DBUG_PRINT("info", ("invalidator... '%s' (status update)",
                        info->s->data_file_name.str));
    DBUG_ASSERT(info->s->chst_invalidator != NULL);
    (*info->s->chst_invalidator)((const char *)info->s->data_file_name.str);
#endif

  }
  info->append_insert_at_end= 0;
}


void _ma_restore_status(void *param)
{
  MARIA_HA *info= (MARIA_HA*) param;
  info->state= &info->s->state.state;
  info->append_insert_at_end= 0;
}


void _ma_copy_status(void* to, void *from)
{
  ((MARIA_HA*) to)->state= &((MARIA_HA*) from)->state_save;
}


void _ma_reset_update_flag(void *param,
                           my_bool concurrent_insert __attribute__((unused)))
{
  MARIA_HA *info=(MARIA_HA*) param;
  info->state->changed= 0;
}


/**
   @brief Check if should allow concurrent inserts

   @implementation
     Allow concurrent inserts if we don't have a hole in the table or
     if there is no active write lock and there is active read locks and
     maria_concurrent_insert == 2. In this last case the new
     row('s) are inserted at end of file instead of filling up the hole.

     The last case is to allow one to inserts into a heavily read-used table
     even if there is holes.

   @notes
     If there is a an rtree indexes in the table, concurrent inserts are
     disabled in maria_open()

  @return
  @retval 0  ok to use concurrent inserts
  @retval 1  not ok
*/

my_bool _ma_check_status(void *param)
{
  MARIA_HA *info=(MARIA_HA*) param;
  /*
    The test for w_locks == 1 is here because this thread has already done an
    external lock (in other words: w_locks == 1 means no other threads has
    a write lock)
  */
  DBUG_PRINT("info",("dellink: %ld  r_locks: %u  w_locks: %u",
                     (long) info->s->state.dellink, (uint) info->s->r_locks,
                     (uint) info->s->w_locks));
  return (my_bool) !(info->s->state.dellink == HA_OFFSET_ERROR ||
                     (maria_concurrent_insert == 2 && info->s->r_locks &&
                      info->s->w_locks == 1));
}


/**
   @brief write hook at end of trans to store status for all used table

   @Notes
   This function must be called under trnman_lock in trnman_end_trn()
   because of the following reasons:
   - After trnman_end_trn() is called, the current transaction will be
   regarded as committed and all used tables state_history will be
   visible to other transactions.  To do this, we loop over all used
   tables and create/update a history entries that contains the correct
   state_history for them.
*/

my_bool _ma_trnman_end_trans_hook(TRN *trn, my_bool commit,
                                  my_bool active_transactions)
{
  my_bool error= 0;
  MARIA_USED_TABLES *tables, *next;
  DBUG_ENTER("_ma_trnman_end_trans_hook");
  
  for (tables= (MARIA_USED_TABLES*) trn->used_tables;
       tables;
       tables= next)
  {
    MARIA_SHARE *share= tables->share;
    next= tables->next;
    if (commit)
    {
      MARIA_STATE_HISTORY *history;

      pthread_mutex_lock(&share->intern_lock);

      /* We only have to update history state if something changed */
      if (tables->state_current.changed)
      {
        if (tables->state_current.no_transid)
        {
          /*
            The change was done without using transid on rows (like in
            bulk insert). In this case this thread is the only one
            that is using the table and all rows will be visble
            for all transactions.
          */
          _ma_reset_history(share);
        }
        else
        {
          if (active_transactions && share->now_transactional &&
              trnman_exists_active_transactions(share->state_history->trid,
                                                trn->commit_trid, 1))
          {
            /*
              There exist transactions that are still using the current
              share->state_history.  Create a new history item for this
              commit and add it first in the state_history list. This
              ensures that all history items are stored in the list in
              decresing trid order.
            */
            if (!(history= my_malloc(sizeof(*history), MYF(MY_WME))))
            {
              /* purecov: begin inspected */
              error= 1;
              pthread_mutex_unlock(&share->intern_lock);
              my_free(tables, MYF(0));
              continue;
              /* purecov: end */
            }
            history->state= share->state_history->state;
            history->next= share->state_history;
            share->state_history= history;
          }
          else
          {
            /* Previous history can't be seen by anyone, reuse old memory */
            history= share->state_history;
            DBUG_PRINT("info", ("removing history->trid: %lu  new: %lu",
                                (ulong) history->trid,
                                (ulong) trn->commit_trid));
          }

          history->state.records+= (tables->state_current.records -
                                    tables->state_start.records);
          history->state.checksum+= (tables->state_current.checksum -
                                     tables->state_start.checksum);
          history->trid= trn->commit_trid;

          share->state.last_change_trn= trn->commit_trid;

          if (history->next)
          {
            /* Remove not visible states */
            share->state_history= _ma_remove_not_visible_states(history, 0, 1);
          }
          DBUG_PRINT("info", ("share: 0x%lx  in_trans: %d",
                              (ulong) share, share->in_trans));
        }
      }
      share->in_trans--;
      pthread_mutex_unlock(&share->intern_lock);
    }
    else
    {
#ifndef DBUG_OFF
      /*
        We need to keep share->in_trans correct in the debug library
        because of the assert in maria_close()
      */
      pthread_mutex_lock(&share->intern_lock);
      share->in_trans--;
      pthread_mutex_unlock(&share->intern_lock);
#endif
    }
    my_free(tables, MYF(0));
  }
  trn->used_tables= 0;
  DBUG_RETURN(error);
}


/**
   Remove table from trnman_list

   @notes
     This is used when we unlock a table from a group of locked tables
     just before doing a rename or drop table.

     share->internal_lock must be locked when function is called
*/

void _ma_remove_table_from_trnman(MARIA_SHARE *share, TRN *trn)
{
  MARIA_USED_TABLES *tables, **prev;
  DBUG_ENTER("_ma_remove_table_from_trnman");
  DBUG_PRINT("enter", ("share: 0x%lx  in_trans: %d",
                       (ulong) share, share->in_trans));

  safe_mutex_assert_owner(&share->intern_lock);
  
  for (prev= (MARIA_USED_TABLES**) &trn->used_tables, tables= *prev;
       tables;
       tables= *prev)
  {
    if (tables->share == share)
    {
      *prev= tables->next;
      share->in_trans--;
      DBUG_PRINT("info", ("in_trans: %d", share->in_trans));
      my_free(tables, MYF(0));
      break;
    }
    prev= &tables->next;
  }
  DBUG_VOID_RETURN;
}



/****************************************************************************
  The following functions are called by thr_lock() in threaded applications
  for transactional tables.
****************************************************************************/

/*
  Create a copy of the current status for the table

  SYNOPSIS
    _ma_get_status()
    param		Pointer to Myisam handler
    concurrent_insert	Set to 1 if we are going to do concurrent inserts
			(THR_WRITE_CONCURRENT_INSERT was used)
*/

void _ma_block_get_status(void* param, my_bool concurrent_insert)
{
  MARIA_HA *info=(MARIA_HA*) param;
  DBUG_ENTER("_ma_block_get_status");
  DBUG_PRINT("info", ("concurrent_insert %d", concurrent_insert));
  info->row_base_length= info->s->base_length;
  info->row_flag= info->s->base.default_row_flag;
  if (concurrent_insert)
  {
    DBUG_ASSERT(info->lock.type == TL_WRITE_CONCURRENT_INSERT);
    info->row_flag|= ROW_FLAG_TRANSID;
    info->row_base_length+= TRANSID_SIZE;
  }
  else
  {
    DBUG_ASSERT(info->lock.type != TL_WRITE_CONCURRENT_INSERT);
  }

  if (info->s->lock_key_trees)
  {
    /*
      Assume for now that this doesn't fail (It can only fail in
      out of memory conditions)
      TODO: Fix this by having one extra state pre-allocated
    */
    (void) _ma_setup_live_state(info);
  }
  DBUG_VOID_RETURN;
}


void _ma_block_update_status(void *param __attribute__((unused)))
{
}

void _ma_block_restore_status(void *param __attribute__((unused)))
{
}


/**
  Check if should allow concurrent inserts

  @return
  @retval 0  ok to use concurrent inserts
  @retval 1  not ok
*/

my_bool _ma_block_check_status(void *param __attribute__((unused)))
{
  return (my_bool) 0;
}


/**
  Enable/disable versioning
*/

void maria_versioning(MARIA_HA *info, my_bool versioning)
{
  /* For now, this is a hack */
  if (info->s->have_versioning)
  {
    enum thr_lock_type save_lock_type;
    /* Assume is a non threaded application (for now) */
    info->s->lock_key_trees= 0;
    /* Set up info->lock.type temporary for _ma_block_get_status() */
    save_lock_type= info->lock.type;
    info->lock.type= versioning ? TL_WRITE_CONCURRENT_INSERT : TL_WRITE;
    _ma_block_get_status((void*) info, versioning);
    info->lock.type= save_lock_type;
  }
}


/**
   Update data_file_length to new length

   NOTES
     Only used by block records
*/

void _ma_set_share_data_file_length(MARIA_SHARE *share, ulonglong new_length)
{
  pthread_mutex_lock(&share->intern_lock);
  if (share->state.state.data_file_length < new_length)
    share->state.state.data_file_length= new_length;
  pthread_mutex_unlock(&share->intern_lock);
}


/**
   Copy state information that where updated while the table was used
   in not transactional mode
*/

void _ma_copy_nontrans_state_information(MARIA_HA *info)
{
  info->s->state.state.records=          info->state->records;
  info->s->state.state.checksum=         info->state->checksum;
}


void _ma_reset_history(MARIA_SHARE *share)
{
  MARIA_STATE_HISTORY *history, *next;
  DBUG_ENTER("_ma_reset_history");

  share->state_history->trid= 0;          /* Visibly by all */
  share->state_history->state= share->state.state;
  history= share->state_history->next;
  share->state_history->next= 0;

  for (; history; history= next)
  {
    next= history->next;
    my_free(history, MYF(0));
  }
  DBUG_VOID_RETURN;
}


/****************************************************************************
  Virtual functions to check if row is visible
****************************************************************************/

/**
   Row is always visible
   This is for tables without concurrent insert
*/

my_bool _ma_row_visible_always(MARIA_HA *info __attribute__((unused)))
{
  return 1;
}


/**
   Row visibility for non transactional tables with concurrent insert

   @implementation
   When we got our table lock, we saved the current
   data_file_length. Concurrent inserts always go to the end of the
   file. So we can test if the found key references a new record.
*/

my_bool _ma_row_visible_non_transactional_table(MARIA_HA *info)
{
  return info->cur_row.lastpos < info->state->data_file_length;
}


/**
   Row visibility for transactional tables with versioning


   @TODO
   Add test if found key was marked deleted and it was deleted by
   us. In that case we should return 0
*/

my_bool _ma_row_visible_transactional_table(MARIA_HA *info)
{
  return trnman_can_read_from(info->trn, info->cur_row.trid);
}
