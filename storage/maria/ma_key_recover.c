/* Copyright (C) 2007 Michael Widenius

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

/* Redo of index */

#include "maria_def.h"
#include "ma_blockrec.h"
#include "trnman.h"
#include "ma_key_recover.h"
#include "ma_rt_index.h"

/****************************************************************************
  Some helper functions used both by key page loggin and block page loggin
****************************************************************************/

/**
  @brief Unpin all pinned pages

  @fn _ma_unpin_all_pages()
  @param info	   Maria handler
  @param undo_lsn  LSN for undo pages. LSN_IMPOSSIBLE if we shouldn't write
                   undo (like on duplicate key errors)

  info->pinned_pages is the list of pages to unpin. Each member of the list
  must have its 'changed' saying if the page was changed or not.

  @note
    We unpin pages in the reverse order as they where pinned; This is not
    necessary now, but may simplify things in the future.

  @return
  @retval   0   ok
  @retval   1   error (fatal disk error)
*/

void _ma_unpin_all_pages(MARIA_HA *info, LSN undo_lsn)
{
  MARIA_PINNED_PAGE *page_link= ((MARIA_PINNED_PAGE*)
                                 dynamic_array_ptr(&info->pinned_pages, 0));
  MARIA_PINNED_PAGE *pinned_page= page_link + info->pinned_pages.elements;
  DBUG_ENTER("_ma_unpin_all_pages");
  DBUG_PRINT("info", ("undo_lsn: %lu", (ulong) undo_lsn));

  if (!info->s->now_transactional)
    DBUG_ASSERT(undo_lsn == LSN_IMPOSSIBLE || maria_in_recovery);

  while (pinned_page-- != page_link)
  {
    /*
      Note this assert fails if we got a disk error or the record file
      is corrupted, which means we should have this enabled only in debug
      builds.
    */
#ifdef EXTRA_DEBUG
    DBUG_ASSERT((!pinned_page->changed ||
                 undo_lsn != LSN_IMPOSSIBLE || !info->s->now_transactional) ||
                (info->s->state.changed & STATE_CRASHED_FLAGS));
#endif
    pagecache_unlock_by_link(info->s->pagecache, pinned_page->link,
                             pinned_page->unlock, PAGECACHE_UNPIN,
                             info->trn->rec_lsn, undo_lsn,
                             pinned_page->changed, FALSE);
  }

  info->pinned_pages.elements= 0;
  DBUG_VOID_RETURN;
}


my_bool _ma_write_clr(MARIA_HA *info, LSN undo_lsn,
                      enum translog_record_type undo_type,
                      my_bool store_checksum, ha_checksum checksum,
                      LSN *res_lsn, void *extra_msg)
{
  uchar log_data[LSN_STORE_SIZE + FILEID_STORE_SIZE + CLR_TYPE_STORE_SIZE +
                 HA_CHECKSUM_STORE_SIZE+ KEY_NR_STORE_SIZE + PAGE_STORE_SIZE];
  uchar *log_pos;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
  struct st_msg_to_write_hook_for_clr_end msg;
  my_bool res;
  DBUG_ENTER("_ma_write_clr");

  /* undo_lsn must be first for compression to work */
  lsn_store(log_data, undo_lsn);
  clr_type_store(log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE, undo_type);
  log_pos= log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE + CLR_TYPE_STORE_SIZE;

  /* Extra_msg is handled in write_hook_for_clr_end() */
  msg.undone_record_type= undo_type;
  msg.previous_undo_lsn=  undo_lsn;
  msg.extra_msg= extra_msg;
  msg.checksum_delta= 0;

  if (store_checksum)
  {
    msg.checksum_delta= checksum;
    ha_checksum_store(log_pos, checksum);
    log_pos+= HA_CHECKSUM_STORE_SIZE;
  }
  else if (undo_type == LOGREC_UNDO_KEY_INSERT_WITH_ROOT ||
           undo_type == LOGREC_UNDO_KEY_DELETE_WITH_ROOT)
  {
    /* Key root changed. Store new key root */
    struct st_msg_to_write_hook_for_undo_key *undo_msg= extra_msg;
    pgcache_page_no_t page;
    key_nr_store(log_pos, undo_msg->keynr);
    page= (undo_msg->value == HA_OFFSET_ERROR ? IMPOSSIBLE_PAGE_NO :
           undo_msg->value / info->s->block_size);
    page_store(log_pos + KEY_NR_STORE_SIZE, page);
    log_pos+= KEY_NR_STORE_SIZE + PAGE_STORE_SIZE;
  }
  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos - log_data);


  /*
    We need intern_lock mutex for calling _ma_state_info_write in the trigger.
    We do it here to have the same sequence of mutexes locking everywhere
    (first intern_lock then transactional log  buffer lock)
  */
  if (undo_type == LOGREC_UNDO_BULK_INSERT)
    pthread_mutex_lock(&info->s->intern_lock);

  res= translog_write_record(res_lsn, LOGREC_CLR_END,
                             info->trn, info,
                             (translog_size_t)
                             log_array[TRANSLOG_INTERNAL_PARTS + 0].length,
                             TRANSLOG_INTERNAL_PARTS + 1, log_array,
                             log_data + LSN_STORE_SIZE, &msg);
  if (undo_type == LOGREC_UNDO_BULK_INSERT)
    pthread_mutex_unlock(&info->s->intern_lock);
  DBUG_RETURN(res);
}


/**
   @brief Sets transaction's undo_lsn, first_undo_lsn if needed

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_clr_end(enum translog_record_type type
                               __attribute__ ((unused)),
                               TRN *trn, MARIA_HA *tbl_info,
                               LSN *lsn __attribute__ ((unused)),
                               void *hook_arg)
{
  MARIA_SHARE *share= tbl_info->s;
  struct st_msg_to_write_hook_for_clr_end *msg=
    (struct st_msg_to_write_hook_for_clr_end *)hook_arg;
  my_bool error= FALSE;
  DBUG_ENTER("write_hook_for_clr_end");
  DBUG_ASSERT(trn->trid != 0);
  trn->undo_lsn= msg->previous_undo_lsn;

  switch (msg->undone_record_type) {
  case LOGREC_UNDO_ROW_DELETE:
    share->state.state.records++;
    share->state.state.checksum+= msg->checksum_delta;
    break;
  case LOGREC_UNDO_ROW_INSERT:
    share->state.state.records--;
    share->state.state.checksum+= msg->checksum_delta;
    break;
  case LOGREC_UNDO_ROW_UPDATE:
    share->state.state.checksum+= msg->checksum_delta;
    break;
  case LOGREC_UNDO_KEY_INSERT_WITH_ROOT:
  case LOGREC_UNDO_KEY_DELETE_WITH_ROOT:
  {
    /* Update key root */
    struct st_msg_to_write_hook_for_undo_key *extra_msg=
      (struct st_msg_to_write_hook_for_undo_key *) msg->extra_msg;
    *extra_msg->root= extra_msg->value;
    break;
  }
  case LOGREC_UNDO_KEY_INSERT:
  case LOGREC_UNDO_KEY_DELETE:
    break;
  case LOGREC_UNDO_BULK_INSERT:
    safe_mutex_assert_owner(&share->intern_lock);
    error= (maria_enable_indexes(tbl_info) ||
            /* we enabled indices, need '2' below */
            _ma_state_info_write(share,
                                 MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET |
                                 MA_STATE_INFO_WRITE_FULL_INFO));
    /* no need for _ma_reset_status(): REDO_DELETE_ALL is just before us */
    break;
  default:
    DBUG_ASSERT(0);
  }
  if (trn->undo_lsn == LSN_IMPOSSIBLE) /* has fully rolled back */
    trn->first_undo_lsn= LSN_WITH_FLAGS_TO_FLAGS(trn->first_undo_lsn);
  DBUG_RETURN(error);
}


/**
  @brief write hook for undo key
*/

my_bool write_hook_for_undo_key(enum translog_record_type type,
                                TRN *trn, MARIA_HA *tbl_info,
                                LSN *lsn, void *hook_arg)
{
  struct st_msg_to_write_hook_for_undo_key *msg=
    (struct st_msg_to_write_hook_for_undo_key *) hook_arg;

  *msg->root= msg->value;
  _ma_fast_unlock_key_del(tbl_info);
  return write_hook_for_undo(type, trn, tbl_info, lsn, 0);
}


/**
   Updates "auto_increment" and calls the generic UNDO_KEY hook

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_undo_key_insert(enum translog_record_type type,
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg)
{
  struct st_msg_to_write_hook_for_undo_key *msg=
    (struct st_msg_to_write_hook_for_undo_key *) hook_arg;
  MARIA_SHARE *share= tbl_info->s;
  if (msg->auto_increment > 0)
  {
    /*
      Only reason to set it here is to have a mutex protect from checkpoint
      reading at the same time (would see a corrupted value).

      The purpose of the following code is to set auto_increment if the row
      has a with auto_increment value higher than the current one. We also
      want to be able to restore the old value, in case of rollback,
      if no one else has tried to set the value.

      The logic used is that we only restore the auto_increment value if
      tbl_info->last_auto_increment == share->last_auto_increment
      when it's time to do the rollback.
    */
    DBUG_PRINT("info",("auto_inc: %lu new auto_inc: %lu",
                       (ulong)share->state.auto_increment,
                       (ulong)msg->auto_increment));
    if (share->state.auto_increment < msg->auto_increment)
    {
      /* Remember the original value, in case of rollback */
      tbl_info->last_auto_increment= share->last_auto_increment=
        share->state.auto_increment;
      share->state.auto_increment= msg->auto_increment;
    }
    else
    {
      /*
        If the current value would have affected the original auto_increment
        value, set it to an impossible value so that it's not restored on
        rollback
      */
      if (msg->auto_increment > share->last_auto_increment)
        share->last_auto_increment= ~(ulonglong) 0;
    }
  }
  return write_hook_for_undo_key(type, trn, tbl_info, lsn, hook_arg);
}


/**
   @brief Updates "share->auto_increment" in case of abort and calls
   generic UNDO_KEY hook

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_undo_key_delete(enum translog_record_type type,
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg)
{
  struct st_msg_to_write_hook_for_undo_key *msg=
    (struct st_msg_to_write_hook_for_undo_key *) hook_arg;
  MARIA_SHARE *share= tbl_info->s;
  if (msg->auto_increment > 0)                  /* If auto increment key */
  {
    /* Restore auto increment if no one has changed it in between */
    if (share->last_auto_increment == tbl_info->last_auto_increment &&
        tbl_info->last_auto_increment != ~(ulonglong) 0)
      share->state.auto_increment= tbl_info->last_auto_increment;
  }
  return write_hook_for_undo_key(type, trn, tbl_info, lsn, hook_arg);
}


/*****************************************************************************
  Functions for logging of key page changes
*****************************************************************************/

/**
   @brief
   Write log entry for page that has got data added or deleted at start of page
*/

my_bool _ma_log_prefix(MARIA_PAGE *ma_page, uint changed_length,
                       int move_length,
                       enum en_key_debug debug_marker __attribute__((unused)))
{
  uint translog_parts;
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 7 + 7 + 2 + 2];
  uchar *log_pos;
  uchar *buff= ma_page->buff;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 4];
  MARIA_HA *info= ma_page->info;
  pgcache_page_no_t page= ma_page->pos / info->s->block_size;
  DBUG_ENTER("_ma_log_prefix");
  DBUG_PRINT("enter", ("page: %lu  changed_length: %u  move_length: %d",
                       (ulong) page, changed_length, move_length));

  DBUG_ASSERT(ma_page->size == ma_page->org_size + move_length);

  log_pos= log_data + FILEID_STORE_SIZE;
  page_store(log_pos, page);
  log_pos+= PAGE_STORE_SIZE;

#ifdef EXTRA_DEBUG_KEY_CHANGES
  (*log_pos++)= KEY_OP_DEBUG;
  (*log_pos++)= debug_marker;
#endif

  /* Store keypage_flag */
  *log_pos++= KEY_OP_SET_PAGEFLAG;
  *log_pos++= buff[KEYPAGE_TRANSFLAG_OFFSET];

  if (move_length < 0)
  {
    /* Delete prefix */
    log_pos[0]= KEY_OP_DEL_PREFIX;
    int2store(log_pos+1, -move_length);
    log_pos+= 3;
    if (changed_length)
    {
      /*
        We don't need a KEY_OP_OFFSET as KEY_OP_DEL_PREFIX has an implicit
        offset
      */
      log_pos[0]= KEY_OP_CHANGE;
      int2store(log_pos+1, changed_length);
      log_pos+= 3;
    }
  }
  else
  {
    /* Add prefix */
    DBUG_ASSERT(changed_length >0 && (int) changed_length >= move_length);
    log_pos[0]= KEY_OP_ADD_PREFIX;
    int2store(log_pos+1, move_length);
    int2store(log_pos+3, changed_length);
    log_pos+= 5;
  }

  translog_parts= 1;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos -
                                                         log_data);
  if (changed_length)
  {
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    (buff +
                                                    info->s->keypage_header);
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= changed_length;
    translog_parts= 2;
  }

  _ma_log_key_changes(ma_page, log_array + TRANSLOG_INTERNAL_PARTS +
                      translog_parts, log_pos, &changed_length,
                      &translog_parts);
  /* Remember new page length for future log entires for same page */
  ma_page->org_size= ma_page->size;

  DBUG_RETURN(translog_write_record(&lsn, LOGREC_REDO_INDEX,
                                    info->trn, info,
                                    (translog_size_t)
                                    log_array[TRANSLOG_INTERNAL_PARTS +
                                              0].length + changed_length,
                                    TRANSLOG_INTERNAL_PARTS + translog_parts,
                                    log_array, log_data, NULL));
}


/**
   @brief
   Write log entry for page that has got data added or deleted at end of page
*/

my_bool _ma_log_suffix(MARIA_PAGE *ma_page, uint org_length, uint new_length)
{
  LSN lsn;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 4];
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 10 + 7 + 2], *log_pos;
  uchar *buff= ma_page->buff;
  int diff;
  uint translog_parts, extra_length;
  MARIA_HA *info= ma_page->info;
  pgcache_page_no_t page= ma_page->pos / info->s->block_size;
  DBUG_ENTER("_ma_log_suffix");
  DBUG_PRINT("enter", ("page: %lu  org_length: %u  new_length: %u",
                       (ulong) page, org_length, new_length));
  DBUG_ASSERT(ma_page->size == new_length);
  DBUG_ASSERT(ma_page->org_size == org_length);

  log_pos= log_data + FILEID_STORE_SIZE;
  page_store(log_pos, page);
  log_pos+= PAGE_STORE_SIZE;

  /* Store keypage_flag */
  *log_pos++= KEY_OP_SET_PAGEFLAG;
  *log_pos++= buff[KEYPAGE_TRANSFLAG_OFFSET];

  if ((diff= (int) (new_length - org_length)) < 0)
  {
    log_pos[0]= KEY_OP_DEL_SUFFIX;
    int2store(log_pos+1, -diff);
    log_pos+= 3;
    translog_parts= 1;
    extra_length= 0;
  }
  else
  {
    log_pos[0]= KEY_OP_ADD_SUFFIX;
    int2store(log_pos+1, diff);
    log_pos+= 3;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    buff + org_length;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= (uint) diff;
    translog_parts= 2;
    extra_length= (uint) diff;
  }

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos -
                                                         log_data);

  _ma_log_key_changes(ma_page,
                      log_array + TRANSLOG_INTERNAL_PARTS + translog_parts,
                      log_pos, &extra_length, &translog_parts);
  /* Remember new page length for future log entires for same page */
  ma_page->org_size= ma_page->size;

  DBUG_RETURN(translog_write_record(&lsn, LOGREC_REDO_INDEX,
                                    info->trn, info,
                                    (translog_size_t)
                                    log_array[TRANSLOG_INTERNAL_PARTS +
                                              0].length + extra_length,
                                    TRANSLOG_INTERNAL_PARTS + translog_parts,
                                    log_array, log_data, NULL));
}


/**
   @brief Log that a key was added to the page

   @param ma_page          Changed page
   @param org_page_length  Length of data in page before key was added
			   Final length in ma_page->size

   @note
     If handle_overflow is set, then we have to protect against
     logging changes that is outside of the page.
     This may happen during underflow() handling where the buffer
     in memory temporary contains more data than block_size

     ma_page may be a page that was previously logged and cuted down
     becasue it's too big. (org_page_length > ma_page->org_size)
*/

my_bool _ma_log_add(MARIA_PAGE *ma_page,
                    uint org_page_length __attribute__ ((unused)),
                    uchar *key_pos, uint changed_length, int move_length,
                    my_bool handle_overflow __attribute__ ((unused)),
                    enum en_key_debug debug_marker __attribute__((unused)))
{
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 2 + 3 + 3 + 3 + 3 + 7 +
                 3 + 2];
  uchar *log_pos;
  uchar *buff= ma_page->buff;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 6];
  MARIA_HA *info= ma_page->info;
  uint offset= (uint) (key_pos - buff);
  uint max_page_size= info->s->max_index_block_size;
  uint translog_parts, current_size;
  pgcache_page_no_t page_pos= ma_page->pos / info->s->block_size;
  DBUG_ENTER("_ma_log_add");
  DBUG_PRINT("enter", ("page: %lu  org_page_length: %u  changed_length: %u  "
                       "move_length: %d",
                       (ulong) page_pos, org_page_length, changed_length,
                       move_length));
  DBUG_ASSERT(info->s->now_transactional);
  DBUG_ASSERT(move_length <= (int) changed_length);
  DBUG_ASSERT(ma_page->org_size == min(org_page_length, max_page_size));
  DBUG_ASSERT(ma_page->size == org_page_length + move_length);
  DBUG_ASSERT(offset <= ma_page->org_size);

  /*
    Write REDO entry that contains the logical operations we need
    to do the page
  */
  log_pos= log_data + FILEID_STORE_SIZE;
  page_store(log_pos, page_pos);
  current_size= ma_page->org_size;
  log_pos+= PAGE_STORE_SIZE;

#ifdef EXTRA_DEBUG_KEY_CHANGES
  *log_pos++= KEY_OP_DEBUG;
  *log_pos++= debug_marker;
#endif

  /* Store keypage_flag */
  *log_pos++= KEY_OP_SET_PAGEFLAG;
  *log_pos++= buff[KEYPAGE_TRANSFLAG_OFFSET];

  /*
    Don't overwrite page boundary
    It's ok to cut this as we will append the data at end of page
    in the next log entry
  */
  if (offset + changed_length > max_page_size)
  {
    DBUG_ASSERT(handle_overflow);
    changed_length= max_page_size - offset;   /* Update to end of page */
    move_length= 0;                             /* Nothing to move */
    /* Extend the page to max length on recovery */
    *log_pos++= KEY_OP_MAX_PAGELENGTH;
    current_size= max_page_size;
  }

  /* Check if adding the key made the page overflow */
  if (current_size + move_length > max_page_size)
  {
    /*
      Adding the key caused an overflow. Cut away the part of the
      page that doesn't fit.
    */
    uint diff;
    DBUG_ASSERT(handle_overflow);
    diff= current_size + move_length - max_page_size;
    log_pos[0]= KEY_OP_DEL_SUFFIX;
    int2store(log_pos+1, diff);
    log_pos+= 3;
    current_size= max_page_size - move_length;
  }

  if (offset == current_size)
  {
    log_pos[0]= KEY_OP_ADD_SUFFIX;
    current_size+= changed_length;
  }
  else
  {
    log_pos[0]= KEY_OP_OFFSET;
    int2store(log_pos+1, offset);
    log_pos+= 3;
    if (move_length)
    {
      if (move_length < 0)
      {
        DBUG_ASSERT(offset - move_length <= org_page_length);
        if (offset - move_length > current_size)
        {
          /*
            Truncate to end of page. We will add data to it from
            the page buffer below
          */
          move_length= (int) offset - (int) current_size;
        }
      }
      log_pos[0]= KEY_OP_SHIFT;
      int2store(log_pos+1, move_length);
      log_pos+= 3;
      current_size+= move_length;
    }
    /*
      Handle case where page was shortend but 'changed_length' goes over
      'current_size'. This can only happen when there was a page overflow
      and we will below add back the overflow part
    */
    if (offset + changed_length > current_size)
    {
      DBUG_ASSERT(offset + changed_length <= ma_page->size);
      changed_length= current_size - offset;
    }
    log_pos[0]= KEY_OP_CHANGE;
  }
  int2store(log_pos+1, changed_length);
  log_pos+= 3;

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos -
                                                         log_data);
  log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    key_pos;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= changed_length;
  translog_parts= TRANSLOG_INTERNAL_PARTS + 2;

  /*
    If page was originally > block_size before operation and now all data
    fits, append the end data that was not part of the previous logged
    page to it.
  */
  DBUG_ASSERT(current_size <= max_page_size && current_size <= ma_page->size);
  if (current_size != ma_page->size && current_size != max_page_size)
  {
    uint length= min(ma_page->size, max_page_size) - current_size;
    uchar *data= ma_page->buff + current_size;

    log_pos[0]= KEY_OP_ADD_SUFFIX;
    int2store(log_pos+1, length);
    log_array[translog_parts].str=      log_pos;
    log_array[translog_parts].length=   3;
    log_array[translog_parts+1].str=    data;
    log_array[translog_parts+1].length= length;
    log_pos+= 3;
    translog_parts+= 2;
    current_size+=   length;
    changed_length+= length + 3;
  }

  _ma_log_key_changes(ma_page, log_array + translog_parts,
                      log_pos, &changed_length, &translog_parts);
  /*
    Remember new page length for future log entries for same page
    Note that this can be different from ma_page->size in case of page
    overflow!
  */
  ma_page->org_size= current_size;
  DBUG_ASSERT(ma_page->org_size == min(ma_page->size, max_page_size));

  if (translog_write_record(&lsn, LOGREC_REDO_INDEX,
                            info->trn, info,
                            (translog_size_t)
                            log_array[TRANSLOG_INTERNAL_PARTS + 0].length +
                            changed_length, translog_parts,
                            log_array, log_data, NULL))
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}


#ifdef EXTRA_DEBUG_KEY_CHANGES

/* Log checksum and optionally key page to log */

void _ma_log_key_changes(MARIA_PAGE *ma_page, LEX_CUSTRING *log_array,
                         uchar *log_pos, uint *changed_length,
                         uint *translog_parts)
{
  MARIA_SHARE *share= ma_page->info->s;
  int page_length= min(ma_page->size, share->max_index_block_size);
  uint org_length;
  ha_checksum crc;

  DBUG_ASSERT(ma_page->flag == (uint) ma_page->buff[KEYPAGE_TRANSFLAG_OFFSET]);

  /* We have to change length as the page may have been shortened */
  org_length= _ma_get_page_used(share, ma_page->buff);
  _ma_store_page_used(share, ma_page->buff, page_length);
  crc= my_checksum(0, ma_page->buff + LSN_STORE_SIZE,
                   page_length - LSN_STORE_SIZE);
  _ma_store_page_used(share, ma_page->buff, org_length);

  log_pos[0]= KEY_OP_CHECK;
  int2store(log_pos+1, page_length);
  int4store(log_pos+3, crc);

  log_array[0].str=    log_pos;
  log_array[0].length= 7;
  (*changed_length)+=  7;
  (*translog_parts)++;
#ifdef EXTRA_STORE_FULL_PAGE_IN_KEY_CHANGES
  log_array[1].str=    ma_page->buff;
  log_array[1].length= page_length;
  (*changed_length)+=  page_length;
  (*translog_parts)++;
#endif /* EXTRA_STORE_FULL_PAGE_IN_KEY_CHANGES */
}

#endif /* EXTRA_DEBUG_KEY_CHANGES */

/****************************************************************************
  Redo of key pages
****************************************************************************/

/**
   @brief Apply LOGREC_REDO_INDEX_NEW_PAGE

   @param  info            Maria handler
   @param  header          Header (without FILEID)

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

uint _ma_apply_redo_index_new_page(MARIA_HA *info, LSN lsn,
                                   const uchar *header, uint length)
{
  pgcache_page_no_t root_page= page_korr(header);
  pgcache_page_no_t free_page= page_korr(header + PAGE_STORE_SIZE);
  uint      key_nr=    key_nr_korr(header + PAGE_STORE_SIZE * 2);
  my_bool   page_type_flag= header[PAGE_STORE_SIZE * 2 + KEY_NR_STORE_SIZE];
  enum pagecache_page_lock unlock_method;
  enum pagecache_page_pin unpin_method;
  MARIA_PINNED_PAGE page_link;
  my_off_t file_size;
  uchar *buff;
  uint result;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_apply_redo_index_new_page");
  DBUG_PRINT("enter", ("root_page: %lu  free_page: %lu",
                       (ulong) root_page, (ulong) free_page));

  /* Set header to point at key data */

  share->state.changed|= (STATE_CHANGED | STATE_NOT_OPTIMIZED_KEYS |
                          STATE_NOT_SORTED_PAGES | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);

  header+= PAGE_STORE_SIZE * 2 + KEY_NR_STORE_SIZE + 1;
  length-= PAGE_STORE_SIZE * 2 + KEY_NR_STORE_SIZE + 1;

  file_size= (my_off_t) (root_page + 1) * share->block_size;
  if (cmp_translog_addr(lsn, share->state.is_of_horizon) >= 0)
  {
    /* free_page is 0 if we shouldn't set key_del */
    if (free_page)
    {
      if (free_page != IMPOSSIBLE_PAGE_NO)
        share->state.key_del= (my_off_t) free_page * share->block_size;
      else
        share->state.key_del= HA_OFFSET_ERROR;
    }
    if (page_type_flag)     /* root page */
      share->state.key_root[key_nr]= file_size - share->block_size;
  }

  if (file_size > share->state.state.key_file_length)
  {
    share->state.state.key_file_length= file_size;
    buff= info->keyread_buff;
    info->keyread_buff_used= 1;
    unlock_method= PAGECACHE_LOCK_WRITE;
    unpin_method=  PAGECACHE_PIN;
  }
  else
  {
    if (!(buff= pagecache_read(share->pagecache, &share->kfile,
                               root_page, 0, 0,
                               PAGECACHE_PLAIN_PAGE, PAGECACHE_LOCK_WRITE,
                               &page_link.link)))
    {
      if (my_errno != HA_ERR_FILE_TOO_SHORT &&
          my_errno != HA_ERR_WRONG_CRC)
      {
        result= 1;
        goto err;
      }
      buff= pagecache_block_link_to_buffer(page_link.link);
    }
    else if (lsn_korr(buff) >= lsn)
    {
      /* Already applied */
      DBUG_PRINT("info", ("Page is up to date, skipping redo"));
      result= 0;
      goto err;
    }
    unlock_method= PAGECACHE_LOCK_LEFT_WRITELOCKED;
    unpin_method=  PAGECACHE_PIN_LEFT_PINNED;
  }

  /* Write modified page */
  bzero(buff, LSN_STORE_SIZE);
  memcpy(buff + LSN_STORE_SIZE, header, length);
  bzero(buff + LSN_STORE_SIZE + length,
        share->max_index_block_size - LSN_STORE_SIZE -  length);
  bfill(buff + share->block_size - KEYPAGE_CHECKSUM_SIZE,
        KEYPAGE_CHECKSUM_SIZE, (uchar) 255);

  result= 0;
  if (unlock_method == PAGECACHE_LOCK_WRITE &&
      pagecache_write(share->pagecache,
                      &share->kfile, root_page, 0,
                      buff, PAGECACHE_PLAIN_PAGE,
                      unlock_method, unpin_method,
                      PAGECACHE_WRITE_DELAY, &page_link.link,
                      LSN_IMPOSSIBLE))
    result= 1;

  /* Mark page to be unlocked and written at _ma_unpin_all_pages() */
  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= 1;
  push_dynamic(&info->pinned_pages, (void*) &page_link);
  DBUG_RETURN(result);

err:
  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 0, FALSE);
  DBUG_RETURN(result);
}


/**
   @brief Apply LOGREC_REDO_INDEX_FREE_PAGE

   @param  info            Maria handler
   @param  header          Header (without FILEID)

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

uint _ma_apply_redo_index_free_page(MARIA_HA *info,
                                    LSN lsn,
                                    const uchar *header)
{
  pgcache_page_no_t page= page_korr(header);
  pgcache_page_no_t free_page= page_korr(header + PAGE_STORE_SIZE);
  my_off_t old_link;
  MARIA_PINNED_PAGE page_link;
  MARIA_SHARE *share= info->s;
  uchar *buff;
  int result;
  DBUG_ENTER("_ma_apply_redo_index_free_page");
  DBUG_PRINT("enter", ("page: %lu  free_page: %lu",
                       (ulong) page, (ulong) free_page));

  share->state.changed|= (STATE_CHANGED | STATE_NOT_OPTIMIZED_KEYS |
                          STATE_NOT_SORTED_PAGES | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);

  if (cmp_translog_addr(lsn, share->state.is_of_horizon) >= 0)
    share->state.key_del= (my_off_t) page * share->block_size;

  old_link=  ((free_page != IMPOSSIBLE_PAGE_NO) ?
              (my_off_t) free_page * share->block_size :
              HA_OFFSET_ERROR);
  if (!(buff= pagecache_read(share->pagecache, &share->kfile,
                             page, 0, 0,
                             PAGECACHE_PLAIN_PAGE, PAGECACHE_LOCK_WRITE,
                             &page_link.link)))
  {
    result= (uint) my_errno;
    goto err;
  }
  if (lsn_korr(buff) >= lsn)
  {
    /* Already applied */
    result= 0;
    goto err;
  }
  /* Free page */
  bzero(buff + LSN_STORE_SIZE, share->keypage_header - LSN_STORE_SIZE);
  _ma_store_keynr(share, buff, (uchar) MARIA_DELETE_KEY_NR);
  _ma_store_page_used(share, buff, share->keypage_header + 8);
  mi_sizestore(buff + share->keypage_header, old_link);

#ifdef IDENTICAL_PAGES_AFTER_RECOVERY
  {
    bzero(buff + share->keypage_header + 8,
          share->block_size - share->keypage_header - 8 -
          KEYPAGE_CHECKSUM_SIZE);
  }
#endif

  /* Mark page to be unlocked and written at _ma_unpin_all_pages() */
  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= 1;
  push_dynamic(&info->pinned_pages, (void*) &page_link);
  DBUG_RETURN(0);

err:
  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 0, FALSE);
  DBUG_RETURN(result);
}


/**
   @brief Apply LOGREC_REDO_INDEX

   @fn ma_apply_redo_index()
   @param  info            Maria handler
   @param  header          Header (without FILEID)

   @notes
     Data for this part is a set of logical instructions of how to
     construct the key page.

   Information of the layout of the components for REDO_INDEX:

   Name              Parameters (in byte) Information
   KEY_OP_OFFSET     2                    Set position for next operations
   KEY_OP_SHIFT      2 (signed int)       How much to shift down or up
   KEY_OP_CHANGE     2 length,  data      Data to replace at 'pos'
   KEY_OP_ADD_PREFIX 2 move-length        How much data should be moved up
                     2 change-length      Data to be replaced at page start
   KEY_OP_DEL_PREFIX 2 length             Bytes to be deleted at page start
   KEY_OP_ADD_SUFFIX 2 length, data       Add data to end of page
   KEY_OP_DEL_SUFFIX 2 length             Reduce page length with this
				          Sets position to start of page
   KEY_OP_CHECK      6 page_length[2],CRC  Used only when debugging
					  This may be followed by page_length
                                          of data (until end of log record)
   KEY_OP_COMPACT_PAGE  6 transid
   KEY_OP_SET_PAGEFLAG  1 flag for page
   KEY_OP_MAX_PAGELENGTH 0                Set page to max length
   KEY_OP_DEBUG	     1                    Info where logging was done

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

long my_counter= 0;

uint _ma_apply_redo_index(MARIA_HA *info,
                          LSN lsn, const uchar *header, uint head_length)
{
  MARIA_SHARE *share= info->s;
  pgcache_page_no_t page_pos= page_korr(header);
  MARIA_PINNED_PAGE page_link;
  uchar *buff;
  const uchar *header_end= header + head_length;
  uint page_offset= 0, org_page_length;
  uint nod_flag, page_length, keypage_header, keynr;
  uint max_page_size= share->max_index_block_size;
  int result;
  MARIA_PAGE page;
  DBUG_ENTER("_ma_apply_redo_index");
  DBUG_PRINT("enter", ("page: %lu", (ulong) page_pos));

  /* Set header to point at key data */
  header+= PAGE_STORE_SIZE;

  if (!(buff= pagecache_read(share->pagecache, &share->kfile,
                             page_pos, 0, 0,
                             PAGECACHE_PLAIN_PAGE, PAGECACHE_LOCK_WRITE,
                             &page_link.link)))
  {
    result= 1;
    goto err;
  }
  if (lsn_korr(buff) >= lsn)
  {
    /* Already applied */
    DBUG_PRINT("info", ("Page is up to date, skipping redo"));
    result= 0;
    goto err;
  }

  keynr= _ma_get_keynr(share, buff);
  _ma_page_setup(&page, info, share->keyinfo + keynr, page_pos, buff);
  nod_flag=    page.node;
  org_page_length= page_length= page.size;

  keypage_header= share->keypage_header;
  DBUG_PRINT("redo", ("page_length: %u", page_length));

  /* Apply modifications to page */
  do
  {
    switch ((enum en_key_op) (*header++)) {
    case KEY_OP_OFFSET:                         /* 1 */
      page_offset= uint2korr(header);
      header+= 2;
      DBUG_PRINT("redo", ("key_op_offset: %u", page_offset));
      DBUG_ASSERT(page_offset >= keypage_header && page_offset <= page_length);
      break;
    case KEY_OP_SHIFT:                          /* 2 */
    {
      int length= sint2korr(header);
      header+= 2;
      DBUG_PRINT("redo", ("key_op_shift: %d", length));
      DBUG_ASSERT(page_offset != 0 && page_offset <= page_length &&
                  page_length + length <= max_page_size);

      if (length < 0)
      {
        DBUG_ASSERT(page_offset - length <= page_length);
        bmove(buff + page_offset, buff + page_offset - length,
              page_length - page_offset + length);
      }
      else if (page_length != page_offset)
        bmove_upp(buff + page_length + length, buff + page_length,
                  page_length - page_offset);
      page_length+= length;
      break;
    }
    case KEY_OP_CHANGE:                         /* 3 */
    {
      uint length= uint2korr(header);
      DBUG_PRINT("redo", ("key_op_change: %u", length));
      DBUG_ASSERT(page_offset != 0 && page_offset + length <= page_length);

      memcpy(buff + page_offset, header + 2 , length);
      page_offset+= length;           /* Put offset after changed length */
      header+= 2 + length;
      break;
    }
    case KEY_OP_ADD_PREFIX:                     /* 4 */
    {
      uint insert_length= uint2korr(header);
      uint changed_length= uint2korr(header+2);
      DBUG_PRINT("redo", ("key_op_add_prefix: %u  %u",
                          insert_length, changed_length));

      DBUG_ASSERT(insert_length <= changed_length &&
                  page_length + changed_length <= max_page_size);

      bmove_upp(buff + page_length + insert_length, buff + page_length,
                page_length - keypage_header);
      memcpy(buff + keypage_header, header + 4 , changed_length);
      header+= 4 + changed_length;
      page_length+= insert_length;
      break;
    }
    case KEY_OP_DEL_PREFIX:                     /* 5 */
    {
      uint length= uint2korr(header);
      header+= 2;
      DBUG_PRINT("redo", ("key_op_del_prefix: %u", length));
      DBUG_ASSERT(length <= page_length - keypage_header);

      bmove(buff + keypage_header, buff + keypage_header +
            length, page_length - keypage_header - length);
      page_length-= length;

      page_offset= keypage_header;              /* Prepare for change */
      break;
    }
    case KEY_OP_ADD_SUFFIX:                     /* 6 */
    {
      uint insert_length= uint2korr(header);
      DBUG_PRINT("redo", ("key_op_add_suffix: %u", insert_length));
      DBUG_ASSERT(page_length + insert_length <= max_page_size);
      memcpy(buff + page_length, header+2, insert_length);

      page_length+= insert_length;
      header+= 2 + insert_length;
      break;
    }
    case KEY_OP_DEL_SUFFIX:                     /* 7 */
    {
      uint del_length= uint2korr(header);
      header+= 2;
      DBUG_PRINT("redo", ("key_op_del_suffix: %u", del_length));
      DBUG_ASSERT(page_length - del_length >= keypage_header);
      page_length-= del_length;
      break;
    }
    case KEY_OP_CHECK:                          /* 8 */
    {
#ifdef EXTRA_DEBUG_KEY_CHANGES
      uint check_page_length;
      ha_checksum crc;
      check_page_length= uint2korr(header);
      crc=               uint4korr(header+2);
      _ma_store_page_used(share, buff, page_length);
      if (check_page_length != page_length ||
          crc != (uint32) my_checksum(0, buff + LSN_STORE_SIZE,
                                      page_length - LSN_STORE_SIZE))
      {
        DBUG_DUMP("KEY_OP_CHECK bad page", buff, page_length);
        if (header + 6 + check_page_length <= header_end)
        {
          DBUG_DUMP("KEY_OP_CHECK org page", header + 6, check_page_length);
        }
        DBUG_ASSERT("crc failure in REDO_INDEX" == 0);
      }
#endif
      DBUG_PRINT("redo", ("key_op_check"));
      /*
        This is the last entry in the block and it can contain page_length
        data or not
      */
      DBUG_ASSERT(header + 6 == header_end ||
                  header + 6 + page_length == header_end);
      header= header_end;
      break;
    }
    case KEY_OP_DEBUG:
      DBUG_PRINT("redo", ("Debug: %u", (uint) header[0]));
      header++;
      break;
    case KEY_OP_DEBUG_2:
      DBUG_PRINT("redo", ("org_page_length: %u  new_page_length: %u",
                          uint2korr(header), uint2korr(header+2)));
      header+= 4;
      break;
    case KEY_OP_MAX_PAGELENGTH:
      DBUG_PRINT("redo", ("key_op_max_page_length"));
      page_length= max_page_size;
      break;
    case KEY_OP_MULTI_COPY:                     /* 9 */
    {
      /*
        List of fixed-len memcpy() operations with their source located inside
        the page. The log record's piece looks like:
        first the length 'full_length' to be used by memcpy()
        then the number of bytes used by the list of (to,from) pairs
        then the (to,from) pairs, so we do:
        for (t,f) in [list of (to,from) pairs]:
            memcpy(t, f, full_length).
      */
      uint full_length, log_memcpy_length;
      const uchar *log_memcpy_end;

      DBUG_PRINT("redo", ("key_op_multi_copy"));
      full_length= uint2korr(header);
      header+= 2;
      log_memcpy_length= uint2korr(header);
      header+= 2;
      log_memcpy_end= header + log_memcpy_length;
      DBUG_ASSERT(full_length <= max_page_size);
      while (header < log_memcpy_end)
      {
        uint to, from;
        to= uint2korr(header);
        header+= 2;
        from= uint2korr(header);
        header+= 2;
        /* "from" is a place in the existing page */
        DBUG_ASSERT(max(from, to) < max_page_size);
        memcpy(buff + to, buff + from, full_length);
      }
      break;
    }
    case KEY_OP_SET_PAGEFLAG:
      DBUG_PRINT("redo", ("key_op_set_pageflag"));
      buff[KEYPAGE_TRANSFLAG_OFFSET]= *header++;
      break;
    case KEY_OP_COMPACT_PAGE:
    {
      TrID transid= transid_korr(header);

      DBUG_PRINT("redo", ("key_op_compact_page"));
      header+= TRANSID_SIZE;
      if (_ma_compact_keypage(&page, transid))
      {
        result= 1;
        goto err;
      }
      page_length= page.size;
    }
    case KEY_OP_NONE:
    default:
      DBUG_ASSERT(0);
      result= 1;
      goto err;
    }
  } while (header < header_end);
  DBUG_ASSERT(header == header_end);

  /* Write modified page */
  page.size= page_length;
  _ma_store_page_used(share, buff, page_length);

  /*
    Clean old stuff up. Gives us better compression of we archive things
    and makes things easer to debug
  */
  if (page_length < org_page_length)
    bzero(buff + page_length, org_page_length-page_length);

  /* Mark page to be unlocked and written at _ma_unpin_all_pages() */
  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= 1;
  push_dynamic(&info->pinned_pages, (void*) &page_link);
  DBUG_RETURN(0);

err:
  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 0, FALSE);
  if (result)
    _ma_mark_file_crashed(share);
  DBUG_RETURN(result);
}


/****************************************************************************
  Undo of key block changes
****************************************************************************/

/**
   @brief Undo of insert of key (ie, delete the inserted key)
*/

my_bool _ma_apply_undo_key_insert(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, uint length)
{
  LSN lsn;
  my_bool res;
  uint keynr;
  uchar key_buff[MARIA_MAX_KEY_BUFF];
  MARIA_SHARE *share= info->s;
  MARIA_KEY key;
  my_off_t new_root;
  struct st_msg_to_write_hook_for_undo_key msg;
  DBUG_ENTER("_ma_apply_undo_key_insert");

  share->state.changed|= (STATE_CHANGED | STATE_NOT_OPTIMIZED_KEYS |
                          STATE_NOT_SORTED_PAGES | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);
  keynr= key_nr_korr(header);
  length-= KEY_NR_STORE_SIZE;

  /* We have to copy key as _ma_ck_real_delete() may change it */
  memcpy(key_buff, header + KEY_NR_STORE_SIZE, length);
  DBUG_DUMP("key_buff", key_buff, length);

  new_root= share->state.key_root[keynr];
  /*
    Change the key to an internal structure.
    It's safe to have SEARCH_USER_KEY_HAS_TRANSID even if there isn't
    a transaction id, as ha_key_cmp() will stop comparison when key length
    is reached.
    For index with transid flag, the ref_length of the key is not correct.
    This should however be safe as long as this key is only used for
    comparsion against other keys (not for packing or for read-next etc as
    in this case we use data_length + ref_length, which is correct.
  */
  key.keyinfo=     share->keyinfo + keynr;
  key.data=        key_buff;
  key.data_length= length - share->rec_reflength;
  key.ref_length=  share->rec_reflength;
  key.flag=        SEARCH_USER_KEY_HAS_TRANSID;

  res= ((share->keyinfo[keynr].key_alg == HA_KEY_ALG_RTREE) ?
        maria_rtree_real_delete(info, &key, &new_root) :
        _ma_ck_real_delete(info, &key, &new_root));
  if (res)
    _ma_mark_file_crashed(share);
  msg.root= &share->state.key_root[keynr];
  msg.value= new_root;
  msg.keynr= keynr;

  if (_ma_write_clr(info, undo_lsn, *msg.root == msg.value ?
                    LOGREC_UNDO_KEY_INSERT : LOGREC_UNDO_KEY_INSERT_WITH_ROOT,
                    0, 0, &lsn, (void*) &msg))
    res= 1;

  _ma_fast_unlock_key_del(info);
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res);
}


/**
   @brief Undo of delete of key (ie, insert the deleted key)

   @param  with_root       If the UNDO is UNDO_KEY_DELETE_WITH_ROOT
*/

my_bool _ma_apply_undo_key_delete(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, uint length,
                                  my_bool with_root)
{
  LSN lsn;
  my_bool res;
  uint keynr, skip_bytes;
  uchar key_buff[MARIA_MAX_KEY_BUFF];
  MARIA_SHARE *share= info->s;
  my_off_t new_root;
  struct st_msg_to_write_hook_for_undo_key msg;
  MARIA_KEY key;
  DBUG_ENTER("_ma_apply_undo_key_delete");

  share->state.changed|= (STATE_CHANGED | STATE_NOT_OPTIMIZED_KEYS |
                          STATE_NOT_SORTED_PAGES | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);
  keynr= key_nr_korr(header);
  skip_bytes= KEY_NR_STORE_SIZE + (with_root ? PAGE_STORE_SIZE : 0);
  header+= skip_bytes;
  length-= skip_bytes;

  /* We have to copy key as _ma_ck_real_write_btree() may change it */
  memcpy(key_buff, header, length);
  DBUG_DUMP("key", key_buff, length);

  key.keyinfo=     share->keyinfo + keynr;
  key.data=        key_buff;
  key.data_length= length - share->rec_reflength;
  key.ref_length=  share->rec_reflength;
  key.flag=        SEARCH_USER_KEY_HAS_TRANSID;

  new_root= share->state.key_root[keynr];
  res= (share->keyinfo[keynr].key_alg == HA_KEY_ALG_RTREE) ?
    maria_rtree_insert_level(info, &key, -1, &new_root) :
    _ma_ck_real_write_btree(info, &key, &new_root,
                            share->keyinfo[keynr].write_comp_flag |
                            key.flag);
  if (res)
    _ma_mark_file_crashed(share);

  msg.root= &share->state.key_root[keynr];
  msg.value= new_root;
  msg.keynr= keynr;
  if (_ma_write_clr(info, undo_lsn,
                    *msg.root == msg.value ?
                    LOGREC_UNDO_KEY_DELETE : LOGREC_UNDO_KEY_DELETE_WITH_ROOT,
                    0, 0, &lsn,
                    (void*) &msg))
    res= 1;

  _ma_fast_unlock_key_del(info);
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res);
}


/****************************************************************************
  Handle some local variables
****************************************************************************/

/**
  @brief lock key_del for other threads usage

  @fn     _ma_lock_key_del()
  @param  info            Maria handler
  @param  insert_at_end   Set to 1 if we are doing an insert

  @note
    To allow higher concurrency in the common case where we do inserts
    and we don't have any linked blocks we do the following:
    - Mark in info->key_del_used that we are not using key_del
    - Return at once (without marking key_del as used)

    This is safe as we in this case don't write key_del_current into
    the redo log and during recover we are not updating key_del.

  @retval 1  Use page at end of file
  @retval 0  Use page at share->key_del_current
*/

my_bool _ma_lock_key_del(MARIA_HA *info, my_bool insert_at_end)
{
  MARIA_SHARE *share= info->s;

  /*
    info->key_del_used is 0 initially.
    If the caller needs a block (_ma_new()), we look at the free list:
    - looks empty? then caller will create a new block at end of file and
    remember (through info->key_del_used==2) that it will not change
    state.key_del and does not need to wake up waiters as nobody will wait for
    it.
    - non-empty? then we wait for other users of the state.key_del list to
    have finished, then we lock this list (through share->key_del_used==1)
    because we need to prevent some other thread to also read state.key_del
    and use the same page as ours. We remember through info->key_del_used==1
    that we will have to set state.key_del at unlock time and wake up
    waiters.
    If the caller wants to free a block (_ma_dispose()), "empty" and
    "non-empty" are treated as "non-empty" is treated above.
    When we are ready to unlock, we copy share->key_del_current into
    state.key_del. Unlocking happens when writing the UNDO log record, that
    can make a long lock time.
    Why we wrote "*looks* empty": because we are looking at state.key_del
    which may be slightly old (share->key_del_current may be more recent and
    exact): when we want a new page, we tolerate to treat "there was no free
    page 1 millisecond ago"  as "there is no free page". It's ok to non-pop
    (_ma_new(), page will be found later anyway) but it's not ok to non-push
    (_ma_dispose(), page would be lost).
    When we leave this function, info->key_del_used is always 1 or 2.
  */
  if (info->key_del_used != 1)
  {
    pthread_mutex_lock(&share->key_del_lock);
    if (share->state.key_del == HA_OFFSET_ERROR && insert_at_end)
    {
      pthread_mutex_unlock(&share->key_del_lock);
      info->key_del_used= 2;                  /* insert-with-append */
      return 1;
    }
#ifdef THREAD
    while (share->key_del_used)
      pthread_cond_wait(&share->key_del_cond, &share->key_del_lock);
#endif
    info->key_del_used= 1;
    share->key_del_used= 1;
    share->key_del_current= share->state.key_del;
    pthread_mutex_unlock(&share->key_del_lock);
  }
  return share->key_del_current == HA_OFFSET_ERROR;
}


/**
  @brief copy changes to key_del and unlock it

  @notes
  In case of many threads using the maria table, we always have a lock
  on the translog when comming here.
*/

void _ma_unlock_key_del(MARIA_HA *info)
{
  DBUG_ASSERT(info->key_del_used);
  if (info->key_del_used == 1)                  /* Ignore insert-with-append */
  {
    MARIA_SHARE *share= info->s;
    pthread_mutex_lock(&share->key_del_lock);
    share->key_del_used= 0;
    share->state.key_del= share->key_del_current;
    pthread_mutex_unlock(&share->key_del_lock);
    pthread_cond_signal(&share->key_del_cond);
  }
  info->key_del_used= 0;
}
