/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* close a isam-database */
/*
  TODO:
   We need to have a separate mutex on the closed file to allow other threads
   to open other files during the time we flush the cache and close this file
*/

#include "maria_def.h"

int maria_close(register MARIA_HA *info)
{
  int error=0,flag;
  my_bool share_can_be_freed= FALSE;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("maria_close");
  DBUG_PRINT("enter",("base: 0x%lx  reopen: %u  locks: %u",
		      (long) info, (uint) share->reopen,
                      (uint) share->tot_locks));

  /* Check that we have unlocked key delete-links properly */
  DBUG_ASSERT(info->key_del_used == 0);

  pthread_mutex_lock(&THR_LOCK_maria);
  if (info->lock_type == F_EXTRA_LCK)
    info->lock_type=F_UNLCK;			/* HA_EXTRA_NO_USER_CHANGE */

  if (share->reopen == 1 && share->kfile.file >= 0)
    _ma_decrement_open_count(info);

  if (info->lock_type != F_UNLCK)
  {
    if (maria_lock_database(info,F_UNLCK))
      error=my_errno;
  }
  pthread_mutex_lock(&share->close_lock);
  pthread_mutex_lock(&share->intern_lock);

  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    share->r_locks--;
    share->tot_locks--;
  }
  if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
  {
    if (end_io_cache(&info->rec_cache))
      error=my_errno;
    info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  }
  flag= !--share->reopen;
  maria_open_list=list_delete(maria_open_list,&info->open_list);

  my_free(info->rec_buff, MYF(MY_ALLOW_ZERO_PTR));
  (*share->end)(info);

  if (flag)
  {
    /* Last close of file; Flush everything */

    /* Check that we don't have any dangling pointers from the transaction */
    DBUG_ASSERT(share->in_trans == 0);

    if (share->kfile.file >= 0)
    {
      if ((*share->once_end)(share))
        error= my_errno;
      if (flush_pagecache_blocks(share->pagecache, &share->kfile,
                                 ((share->temporary || share->deleting) ?
                                  FLUSH_IGNORE_CHANGED :
                                  FLUSH_RELEASE)))
        error= my_errno;
#ifdef HAVE_MMAP
      if (share->file_map)
        _ma_unmap_file(info);
#endif
      /*
        If we are crashed, we can safely flush the current state as it will
        not change the crashed state.
        We can NOT write the state in other cases as other threads
        may be using the file at this point
        IF using --external-locking, which does not apply to Maria.
      */
      if (((share->changed && share->base.born_transactional) ||
           maria_is_crashed(info)))
      {
        /*
          State must be written to file as it was not done at table's
          unlocking.
        */
        if (_ma_state_info_write(share, MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET))
          error= my_errno;
      }
      /*
        File must be synced as it is going out of the maria_open_list and so
        becoming unknown to future Checkpoints.
      */
      if (share->now_transactional && my_sync(share->kfile.file, MYF(MY_WME)))
        error= my_errno;
      if (my_close(share->kfile.file, MYF(0)))
        error= my_errno;
    }
#ifdef THREAD
    thr_lock_delete(&share->lock);
    (void) pthread_mutex_destroy(&share->key_del_lock);
    {
      int i,keys;
      keys = share->state.header.keys;
      VOID(rwlock_destroy(&share->mmap_lock));
      for(i=0; i<keys; i++) {
	VOID(rwlock_destroy(&share->keyinfo[i].root_lock));
      }
    }
#endif
    DBUG_ASSERT(share->now_transactional == share->base.born_transactional);
    /*
      We assign -1 because checkpoint does not need to flush (in case we
      have concurrent checkpoint if no then we do not need it here also)
    */
    share->kfile.file= -1;

    /*
      Remember share->history for future opens

      We have to unlock share->intern_lock then lock it after
      LOCK_trn_list (trnman_lock()) to avoid dead locks.
    */
    pthread_mutex_unlock(&share->intern_lock);
    _ma_remove_not_visible_states_with_lock(share, TRUE);
    pthread_mutex_lock(&share->intern_lock);

    if (share->in_checkpoint & MARIA_CHECKPOINT_LOOKS_AT_ME)
    {
      /* we cannot my_free() the share, Checkpoint would see a bad pointer */
      share->in_checkpoint|= MARIA_CHECKPOINT_SHOULD_FREE_ME;
    }
    else
      share_can_be_freed= TRUE;

    if (share->state_history)
    {
      MARIA_STATE_HISTORY_CLOSED *history;
      /*
        Here we ignore the unlikely case that we don't have memory to
        store the state. In the worst case what happens is that any transaction
        that tries to access this table will get a wrong status information.
      */
      if ((history= (MARIA_STATE_HISTORY_CLOSED *)
           my_malloc(sizeof(*history), MYF(MY_WME))))
      {
        history->create_rename_lsn= share->state.create_rename_lsn;
        history->state_history= share->state_history;
        if (my_hash_insert(&maria_stored_state, (uchar*) history))
          my_free(history, MYF(0));
      }
      /* Marker for concurrent checkpoint */
      share->state_history= 0;
    }
  }
  pthread_mutex_unlock(&THR_LOCK_maria);
  pthread_mutex_unlock(&share->intern_lock);
  pthread_mutex_unlock(&share->close_lock);
  if (share_can_be_freed)
  {
    (void) pthread_mutex_destroy(&share->intern_lock);
    (void) pthread_mutex_destroy(&share->close_lock);
    (void) pthread_cond_destroy(&share->key_del_cond);
    my_free((uchar *)share, MYF(0));
    /*
      If share cannot be freed, it's because checkpoint has previously
      recorded to include this share in the checkpoint and so is soon going to
      look at some of its content (share->in_checkpoint/id/last_version).
    */
  }
  my_free(info->ftparser_param, MYF(MY_ALLOW_ZERO_PTR));
  if (info->dfile.file >= 0)
  {
    /*
      This is outside of mutex so would confuse a concurrent
      Checkpoint. Fortunately in BLOCK_RECORD we close earlier under mutex.
    */
    if (my_close(info->dfile.file, MYF(0)))
      error= my_errno;
  }

  delete_dynamic(&info->pinned_pages);
  my_free(info, MYF(0));

  if (error)
  {
    DBUG_PRINT("error", ("Got error on close: %d", my_errno));
    DBUG_RETURN(my_errno= error);
  }
  DBUG_RETURN(0);
} /* maria_close */
