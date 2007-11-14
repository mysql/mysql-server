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

/****************************************************************************
  Some helper functions used both by key page loggin and block page loggin
****************************************************************************/

/*
  @brief Unpin all pinned pages

  @fn _ma_unpin_all_pages()
  @param info	   Maria handler
  @param undo_lsn  LSN for undo pages. LSN_IMPOSSIBLE if we shouldn't write
                   undo (like on duplicate key errors)

  @note
    We unpin pages in the reverse order as they where pinned; This may not
    be strictly necessary but may simplify things in the future.

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
    undo_lsn= LSN_IMPOSSIBLE; /* don't try to set a LSN on pages */

  while (pinned_page-- != page_link)
  {
    DBUG_ASSERT(!pinned_page->changed ||
                undo_lsn != LSN_IMPOSSIBLE || !info->s->now_transactional);
    pagecache_unlock_by_link(info->s->pagecache, pinned_page->link,
                             pinned_page->unlock, PAGECACHE_UNPIN,
                             info->trn->rec_lsn, undo_lsn,
                             pinned_page->changed);
  }

  info->pinned_pages.elements= 0;
  DBUG_VOID_RETURN;
}


my_bool _ma_write_clr(MARIA_HA *info, LSN undo_lsn,
                      enum translog_record_type undo_type,
                      my_bool store_checksum, ha_checksum checksum,
                      LSN *res_lsn)
{
  uchar log_data[LSN_STORE_SIZE + FILEID_STORE_SIZE +
                 PAGE_STORE_SIZE + DIRPOS_STORE_SIZE +
                 HA_CHECKSUM_STORE_SIZE];
  LEX_STRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
  struct st_msg_to_write_hook_for_clr_end msg;
  my_bool res;
  DBUG_ENTER("_ma_write_clr");

  /* undo_lsn must be first for compression to work */
  lsn_store(log_data, undo_lsn);
  clr_type_store(log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE,
                 undo_type);
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length=
    sizeof(log_data) - HA_CHECKSUM_STORE_SIZE;
  msg.undone_record_type= undo_type;
  msg.previous_undo_lsn=  undo_lsn;

  if (store_checksum)
  {
    ha_checksum_store(log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE +
                      CLR_TYPE_STORE_SIZE, checksum);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
  }
  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    (char*) log_data;

  res= translog_write_record(res_lsn, LOGREC_CLR_END,
                             info->trn, info, log_array[TRANSLOG_INTERNAL_PARTS
                                                        + 0].length,
                             TRANSLOG_INTERNAL_PARTS + 1, log_array,
                             log_data + LSN_STORE_SIZE, &msg);
  DBUG_RETURN(res);
}


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
  ulonglong root_page= page_korr(header);
  ulonglong free_page= page_korr(header + PAGE_STORE_SIZE);
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

  /* Set header to point at key data */

  share->state.changed|= (STATE_CHANGED | STATE_NOT_OPTIMIZED_KEYS |
                          STATE_NOT_SORTED_PAGES);

  header+= PAGE_STORE_SIZE * 2 + KEY_NR_STORE_SIZE + 1;
  length-= PAGE_STORE_SIZE * 2 + KEY_NR_STORE_SIZE + 1;

  /* free_page is 0 if we shouldn't set key_del */
  if (free_page)
  {
    if (free_page != IMPOSSIBLE_PAGE_NO)
      share->state.key_del= (my_off_t) free_page * share->block_size;
    else
      share->state.key_del= HA_OFFSET_ERROR;
  }
  file_size= (my_off_t) (root_page + 1) * share->block_size;

  /* If root page */
  if (page_type_flag)
    share->state.key_root[key_nr]= file_size - share->block_size;

  if (file_size > info->state->key_file_length)
  {
    info->state->key_file_length= file_size;
    buff= info->keyread_buff;
    info->keyread_buff_used= 1;
    unlock_method= PAGECACHE_LOCK_LEFT_UNLOCKED;
    unpin_method=  PAGECACHE_PIN_LEFT_UNPINNED;
  }
  else
  {
    if (!(buff= pagecache_read(share->pagecache, &share->kfile,
                               root_page, 0, 0,
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
    unlock_method= PAGECACHE_LOCK_WRITE_UNLOCK;
    unpin_method=  PAGECACHE_UNPIN;
  }

  /* Write modified page */
  lsn_store(buff, lsn);
  memcpy(buff + LSN_STORE_SIZE, header, length);
  bzero(buff + LSN_STORE_SIZE + length,
        share->block_size - LSN_STORE_SIZE - KEYPAGE_CHECKSUM_SIZE - length);
  bfill(buff + share->block_size - KEYPAGE_CHECKSUM_SIZE,
        KEYPAGE_CHECKSUM_SIZE, (uchar) 255);
  if (pagecache_write(share->pagecache,
                      &share->kfile, root_page, 0,
                      buff, PAGECACHE_PLAIN_PAGE,
                      unlock_method, unpin_method,
                      PAGECACHE_WRITE_DELAY, 0))
    DBUG_RETURN(my_errno);
  DBUG_RETURN(0);

err:
  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 0);
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
  ulonglong page= page_korr(header);
  ulonglong free_page= page_korr(header + PAGE_STORE_SIZE);
  my_off_t old_link;
  MARIA_PINNED_PAGE page_link;
  MARIA_SHARE *share= info->s;
  uchar *buff;
  int result;
  DBUG_ENTER("_ma_apply_redo_index_free_page");

  share->state.changed|= (STATE_CHANGED | STATE_NOT_OPTIMIZED_KEYS |
                          STATE_NOT_SORTED_PAGES);

  old_link= share->state.key_del;
  share->state.key_del=  ((free_page != IMPOSSIBLE_PAGE_NO) ?
                          (my_off_t) free_page * share->block_size :
                          HA_OFFSET_ERROR);
  if (!(buff= pagecache_read(share->pagecache, &info->s->kfile,
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
  /* Write modified page */
  lsn_store(buff, lsn);
  bzero(buff + LSN_STORE_SIZE, share->keypage_header - LSN_STORE_SIZE);
  _ma_store_keynr(info, buff, (uchar) MARIA_DELETE_KEY_NR);
  mi_sizestore(buff + share->keypage_header, old_link);
  share->state.changed|= STATE_NOT_SORTED_PAGES;

  if (pagecache_write(share->pagecache,
                      &info->s->kfile, page, 0,
                      buff, PAGECACHE_PLAIN_PAGE,
                      PAGECACHE_LOCK_WRITE_UNLOCK,
                      PAGECACHE_UNPIN,
                      PAGECACHE_WRITE_DELAY, 0))
    DBUG_RETURN(my_errno);
  DBUG_RETURN(0);

err:
  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 0);
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

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

uint _ma_apply_redo_index(MARIA_HA *info,
                          LSN lsn, const uchar *header, uint head_length)
{
  MARIA_SHARE *share= info->s;
  ulonglong page= page_korr(header);
  MARIA_PINNED_PAGE page_link;
  uchar *buff;
  const uchar *header_end= header + head_length;
  uint page_offset= 0;
  uint nod_flag, page_length, keypage_header;
  int result;
  uint org_page_length;
  DBUG_ENTER("_ma_apply_redo_index");

  /* Set header to point at key data */
  header+= PAGE_STORE_SIZE;

  if (!(buff= pagecache_read(share->pagecache, &info->s->kfile,
                             page, 0, 0,
                             PAGECACHE_PLAIN_PAGE, PAGECACHE_LOCK_WRITE,
                             &page_link.link)))
  {
    result= 1;
    goto err;
  }
  if (lsn_korr(buff) >= lsn)
  {
    /* Already applied */
    result= 0;
    goto err;
  }

  _ma_get_used_and_nod(info, buff, page_length, nod_flag);
  keypage_header= share->keypage_header;
  org_page_length= page_length;

  /* Apply modifications to page */
  do
  {
    switch ((enum en_key_op) (*header++)) {
    case KEY_OP_OFFSET:
      page_offset= uint2korr(header);
      header+= 2;
      DBUG_ASSERT(page_offset >= keypage_header && page_offset <= page_length);
      break;
    case KEY_OP_SHIFT:
    {
      int length= sint2korr(header);
      header+= 2;
      DBUG_ASSERT(page_offset != 0 && page_offset < page_length &&
                  page_length + length < share->block_size);

      if (length < 0)
        bmove(buff + page_offset, buff + page_offset - length,
              page_length - page_offset + length);
      else
        bmove_upp(buff + page_length + length, buff + page_length,
                  page_length - page_offset);
      page_length+= length;
      break;
    }
    case KEY_OP_CHANGE:
    {
      uint length= uint2korr(header);
      DBUG_ASSERT(page_offset != 0 && page_offset + length <= page_length);

      memcpy(buff + page_offset, header + 2 , length);
      header+= 2 + length;
      break;
    }
    case KEY_OP_ADD_PREFIX:
    {
      uint insert_length= uint2korr(header);
      uint change_length= uint2korr(header+2);
      DBUG_ASSERT(insert_length <= change_length &&
                  page_length + change_length <= share->block_size);

      bmove_upp(buff + page_length + insert_length, buff + page_length,
                page_length - keypage_header);
      memcpy(buff + keypage_header, header + 4 , change_length);
      header+= 4 + change_length;
      page_length+= insert_length;
      break;
    }
    case KEY_OP_DEL_PREFIX:
    {
      uint length= uint2korr(header);
      header+= 2;
      DBUG_ASSERT(length <= page_length - keypage_header);

      bmove(buff + keypage_header, buff + keypage_header +
            length, page_length - keypage_header - length);
      page_length-= length;
      break;
    }
    case KEY_OP_ADD_SUFFIX:
    {
      uint insert_length= uint2korr(header);
      DBUG_ASSERT(page_length + insert_length <= share->block_size);
      memcpy(buff + page_length, header+2, insert_length);

      page_length+= insert_length;
      header+= 2 + insert_length;
      break;
    }
    case KEY_OP_DEL_SUFFIX:
    {
      uint del_length= uint2korr(header);
      header+= 2;
      DBUG_ASSERT(page_length - del_length >= keypage_header);
      page_length-= del_length;
      break;
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
  lsn_store(buff, lsn);
  _ma_store_page_used(info, buff, page_length, nod_flag);

  /*
    Clean old stuff up. Gives us better compression of we archive things
    and makes things easer to debug
  */
  if (page_length < org_page_length)
    bzero(buff + page_length, org_page_length-page_length);

  if (pagecache_write(share->pagecache,
                      &info->s->kfile, page, 0,
                      buff, PAGECACHE_PLAIN_PAGE,
                      PAGECACHE_LOCK_WRITE_UNLOCK, PAGECACHE_UNPIN,
                      PAGECACHE_WRITE_DELAY, 0))
    DBUG_RETURN(my_errno);
  DBUG_RETURN(0);

err:
  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 0);
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
  uchar key[HA_MAX_KEY_BUFF];
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_apply_undo_key_insert");

  share->state.changed|= (STATE_CHANGED | STATE_NOT_OPTIMIZED_KEYS |
                          STATE_NOT_SORTED_PAGES);
  keynr= key_nr_korr(header);
  length-= KEY_NR_STORE_SIZE;

  /* We have to copy key as _ma_ck_real_delete() may change it */
  memcpy(key, header+ KEY_NR_STORE_SIZE, length);

  res= _ma_ck_real_delete(info, share->keyinfo+keynr, key, length,
                          &share->state.key_root[keynr]);

  if (_ma_write_clr(info, undo_lsn, LOGREC_UNDO_KEY_INSERT, 1, 0, &lsn))
    res= 1;

  _ma_fast_unlock_key_del(info);
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res);
}


/**
   @brief Undo of insert of key (ie, delete the inserted key)
*/

my_bool _ma_apply_undo_key_delete(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, uint length)
{
  LSN lsn;
  my_bool res;
  uint keynr;
  uchar key[HA_MAX_KEY_BUFF];
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_apply_undo_key_delete");

  share->state.changed|= (STATE_CHANGED | STATE_NOT_OPTIMIZED_KEYS |
                          STATE_NOT_SORTED_PAGES);
  keynr= key_nr_korr(header);
  length-= KEY_NR_STORE_SIZE;

  /* We have to copy key as _ma_ck_real_delete() may change it */
  memcpy(key, header+ KEY_NR_STORE_SIZE, length);

  res= _ma_ck_real_write_btree(info, share->keyinfo+keynr, key, length,
                               &share->state.key_root[keynr],
                               share->keyinfo[keynr].write_comp_flag);

  if (_ma_write_clr(info, undo_lsn, LOGREC_UNDO_KEY_DELETE, 1, 0, &lsn))
    res= 1;

  _ma_fast_unlock_key_del(info);
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res);
}


/****************************************************************************
  Handle some local variables
****************************************************************************/

/*
  @brief lock key_del for other threads usage

  @fn     _ma_lock_key_del()
  @param  info            Maria handler
  @param  insert_at_end   Set to 1 if we are doing an insert

  @notes
    To allow higher concurrency in the common case where we do inserts
    and we don't have any linked blocks we do the following:
    - Mark in info->used_key_del that we are not using key_del
    - Return at once (without marking key_del as used)

    This is safe as we in this case don't write current_key_del into
    the redo log and during recover we are not updating key_del.
*/

my_bool _ma_lock_key_del(MARIA_HA *info, my_bool insert_at_end)
{
  MARIA_SHARE *share= info->s;

  if (info->used_key_del != 1)
  {
    pthread_mutex_lock(&share->intern_lock);
    if (share->state.key_del == HA_OFFSET_ERROR && insert_at_end)
    {
      pthread_mutex_unlock(&share->intern_lock);
      info->used_key_del= 2;                  /* insert-with-append */
      return 1;
    }
#ifdef THREAD
    while (share->used_key_del)
      pthread_cond_wait(&share->intern_cond, &share->intern_lock);
#endif
    info->used_key_del= 1;
    share->used_key_del= 1;
    pthread_mutex_unlock(&share->intern_lock);
  }
  return 0;
}


/*
  @brief copy changes to key_del and unlock it
*/

void _ma_unlock_key_del(MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;
  DBUG_ASSERT(info->used_key_del);
  if (info->used_key_del == 1)                  /* Ignore insert-with-append */
  {
    pthread_mutex_lock(&share->intern_lock);
    share->used_key_del= 0;
    info->s->state.key_del= info->s->current_key_del;
    pthread_mutex_unlock(&share->intern_lock);
    pthread_cond_signal(&share->intern_cond);
  }
  info->used_key_del= 0;
}
