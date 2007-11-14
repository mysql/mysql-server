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
  enum pagecache_page_lock unlock_method;
  enum pagecache_page_pin unpin_method;
  MARIA_PINNED_PAGE page_link;
  my_off_t file_size;
  uchar *buff;
  uint result;
  DBUG_ENTER("_ma_apply_redo_index_new_page");

  /* Set header to point at key data */
  header+= PAGE_STORE_SIZE*2;
  length-= PAGE_STORE_SIZE*2;

  if (free_page != IMPOSSIBLE_PAGE_NO)
    info->s->state.key_del= (my_off_t) free_page * info->s->block_size;
  else
    info->s->state.key_del= HA_OFFSET_ERROR;

  file_size= (my_off_t) (root_page + 1) * info->s->block_size;
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
    if (!(buff= pagecache_read(info->s->pagecache, &info->dfile,
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
#ifdef IDENTICAL_PAGES_AFTER_RECOVERY
  bzero(buff + LSN_STORE_SIZE + length,
        info->s->block_size - LSN_STORE_SIZE - length);
#endif
  if (pagecache_write(info->s->pagecache,
                      &info->dfile, root_page, 0,
                      buff, PAGECACHE_PLAIN_PAGE,
                      unlock_method, unpin_method,
                      PAGECACHE_WRITE_DELAY, 0))
    DBUG_RETURN(my_errno);
  DBUG_RETURN(0);

err:
  pagecache_unlock_by_link(info->s->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE);
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

  old_link= share->state.key_del;
  share->state.key_del=  ((free_page != IMPOSSIBLE_PAGE_NO) ?
                          (my_off_t) free_page * share->block_size :
                          HA_OFFSET_ERROR);
  if (!(buff= pagecache_read(share->pagecache, &info->dfile,
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
                      &info->dfile, page, 0,
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
                           LSN_IMPOSSIBLE);
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
                          LSN lsn, const uchar *header, uint length)
{
  ulonglong root_page= page_korr(header);
  MARIA_PINNED_PAGE page_link;
  uchar *buff;
  const uchar *header_end= header + length;
  uint page_offset= 0;
  uint nod_flag, page_length, keypage_header;
  int result;
  DBUG_ENTER("_ma_apply_redo_index");

  /* Set header to point at key data */
  header+= PAGE_STORE_SIZE;

  if (!(buff= pagecache_read(info->s->pagecache, &info->dfile,
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

  _ma_get_used_and_nod(info, buff, page_length, nod_flag);
  keypage_header= info->s->keypage_header;

  /* Apply modifications to page */
  do
  {
    switch ((enum en_key_op) (*header++)) {
    case KEY_OP_NONE:
      DBUG_ASSERT(0);                           /* Impossible */
      break;
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
                  page_length + length < info->s->block_size);

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
                  page_length + change_length < info->s->block_size);

      bmove_upp(buff + page_length + insert_length, buff + page_length,
                page_length - keypage_header);
      memcpy(buff + keypage_header, header + 2 , change_length);
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
      DBUG_ASSERT(page_length + insert_length < info->s->block_size);
      memcpy(buff + page_length, header, insert_length);
      page_length= insert_length;
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
    }
  } while (header < header_end);
  DBUG_ASSERT(header == header_end);

  /* Write modified page */
  lsn_store(buff, lsn);
  memcpy(buff + LSN_STORE_SIZE, header, length);
  _ma_store_page_used(info, buff, page_length, nod_flag);

  if (pagecache_write(info->s->pagecache,
                      &info->dfile, root_page, 0,
                      buff, PAGECACHE_PLAIN_PAGE,
                      PAGECACHE_LOCK_WRITE_UNLOCK, PAGECACHE_UNPIN,
                      PAGECACHE_WRITE_DELAY, 0))
    DBUG_RETURN(my_errno);
  DBUG_RETURN(0);

err:
  pagecache_unlock_by_link(info->s->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE);
  DBUG_RETURN(result);
}

/*
  Unpin all pinned pages

  SYNOPSIS
    _ma_unpin_all_pages()
    info	Maria handler
    undo_lsn	LSN for undo pages. LSN_IMPOSSIBLE if we shouldn't write undo
                (error)

  NOTE
    We unpin pages in the reverse order as they where pinned; This may not
    be strictly necessary but may simplify things in the future.

  RETURN
    0   ok
    1   error (fatal disk error)

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
    DBUG_ASSERT(pinned_page->changed &&
                ((undo_lsn != LSN_IMPOSSIBLE) || !info->s->now_transactional));
    pagecache_unlock_by_link(info->s->pagecache, pinned_page->link,
                             pinned_page->unlock, PAGECACHE_UNPIN,
                             info->trn->rec_lsn, undo_lsn);
  }

  info->pinned_pages.elements= 0;
  DBUG_VOID_RETURN;
}


/****************************************************************************
  Undo of key block changes
****************************************************************************/


/*
  Undo of insert of key (ie, delete the inserted key)
*/

my_bool _ma_apply_undo_key_insert(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, uint length)
{
  ulonglong page;
  uint rownr;
  LEX_STRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
  uchar log_data[LSN_STORE_SIZE + FILEID_STORE_SIZE +
                 CLR_TYPE_STORE_SIZE + HA_CHECKSUM_STORE_SIZE],
    *buff;
  my_bool res= 1;
  MARIA_PINNED_PAGE page_link;
  LSN lsn= LSN_IMPOSSIBLE;
  MARIA_SHARE *share= info->s;
  struct st_msg_to_write_hook_for_clr_end msg;
  DBUG_ENTER("_ma_apply_undo_key_insert");

  keynr= keynr_korr(header);
  key=   header+ KEY_NR_STORE_SIZE;
  length-= KEYNR_STORE_SIZE;

  res= _ma_ck_delete(info, info->s->keyinfo+keynr, key, length,
                     &info->s->state.key_root[keynr]);
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res);
}
