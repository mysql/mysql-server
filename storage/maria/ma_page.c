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

/* Read and write key blocks */

#include "maria_def.h"
#include "trnman.h"
#include "ma_key_recover.h"

/* Fetch a key-page in memory */

uchar *_ma_fetch_keypage(register MARIA_HA *info,
                         const MARIA_KEYDEF *keyinfo __attribute__ ((unused)),
                         my_off_t pos, enum pagecache_page_lock lock,
                         int level, uchar *buff,
                         int return_buffer __attribute__ ((unused)),
                         MARIA_PINNED_PAGE **page_link_res)
{
  uchar *tmp;
  MARIA_PINNED_PAGE page_link;
  MARIA_SHARE *share= info->s;
  uint block_size= share->block_size;
  DBUG_ENTER("_ma_fetch_keypage");
  DBUG_PRINT("enter",("pos: %ld", (long) pos));

  tmp= pagecache_read(share->pagecache, &share->kfile,
                      (pgcache_page_no_t) (pos / block_size), level, buff,
                      share->page_type, lock, &page_link.link);

  if (lock != PAGECACHE_LOCK_LEFT_UNLOCKED)
  {
    DBUG_ASSERT(lock == PAGECACHE_LOCK_WRITE);
    page_link.unlock=  PAGECACHE_LOCK_WRITE_UNLOCK;
    page_link.changed= 0;
    push_dynamic(&info->pinned_pages, (void*) &page_link);
    *page_link_res= dynamic_element(&info->pinned_pages,
                                    info->pinned_pages.elements-1,
                                    MARIA_PINNED_PAGE *);
  }

  if (tmp == info->buff)
    info->keyread_buff_used=1;
  else if (!tmp)
  {
    DBUG_PRINT("error",("Got errno: %d from pagecache_read",my_errno));
    info->last_keypage=HA_OFFSET_ERROR;
    maria_print_error(share, HA_ERR_CRASHED);
    my_errno=HA_ERR_CRASHED;
    DBUG_RETURN(0);
  }
  info->last_keypage= pos;
#ifdef EXTRA_DEBUG
  {
    uint page_size= _ma_get_page_used(share, tmp);
    if (page_size < 4 || page_size > block_size ||
        _ma_get_keynr(share, tmp) != keyinfo->key_nr)
    {
      DBUG_PRINT("error",("page %lu had wrong page length: %u  keynr: %u",
                          (ulong) (pos / block_size), page_size,
                          _ma_get_keynr(share, tmp)));
      DBUG_DUMP("page", tmp, page_size);
      info->last_keypage = HA_OFFSET_ERROR;
      maria_print_error(share, HA_ERR_CRASHED);
      my_errno= HA_ERR_CRASHED;
      tmp= 0;
    }
  }
#endif
  DBUG_RETURN(tmp);
} /* _ma_fetch_keypage */


/* Write a key-page on disk */

int _ma_write_keypage(register MARIA_HA *info,
                      register const MARIA_KEYDEF *keyinfo
                      __attribute__((unused)),
		      my_off_t pos, enum pagecache_page_lock lock,
                      int level, uchar *buff)
{
  MARIA_SHARE *share= info->s;
  MARIA_PINNED_PAGE page_link;
  uint block_size= share->block_size;
  int res;
  DBUG_ENTER("_ma_write_keypage");

#ifdef EXTRA_DEBUG				/* Safety check */
  {
    uint page_length, nod;
    _ma_get_used_and_nod(share, buff, page_length, nod);
    if (pos < share->base.keystart ||
        pos+block_size > share->state.state.key_file_length ||
        (pos & (maria_block_size-1)))
    {
      DBUG_PRINT("error",("Trying to write inside key status region: "
                          "key_start: %lu  length: %lu  page: %lu",
                          (long) share->base.keystart,
                          (long) share->state.state.key_file_length,
                          (long) pos));
      my_errno=EINVAL;
      DBUG_ASSERT(0);
      DBUG_RETURN((-1));
    }
    DBUG_PRINT("page",("write page at: %lu",(long) pos));
    DBUG_DUMP("buff", buff, page_length);
    DBUG_ASSERT(page_length >= share->keypage_header + nod +
                keyinfo->minlength || maria_in_recovery);
  }
#endif

  /* Verify that keynr is correct */
  DBUG_ASSERT(_ma_get_keynr(share, buff) == keyinfo->key_nr);

#if defined(EXTRA_DEBUG) && defined(HAVE_purify) && defined(NOT_ANYMORE)
  {
    /* This is here to catch uninitialized bytes */
    uint length= _ma_get_page_used(share, buff);
    ulong crc= my_checksum(0, buff, length);
    int4store(buff + block_size - KEYPAGE_CHECKSUM_SIZE, crc);
  }
#endif

#ifdef IDENTICAL_PAGES_AFTER_RECOVERY
  {
    uint length= _ma_get_page_used(share, buff);
    DBUG_ASSERT(length <= block_size - KEYPAGE_CHECKSUM_SIZE);
    bzero(buff + length, block_size - length);
  }
#endif
  DBUG_ASSERT(share->pagecache->block_size == block_size);

  res= pagecache_write(share->pagecache,
                       &share->kfile, (pgcache_page_no_t) (pos / block_size),
                       level, buff, share->page_type,
                       lock,
                       lock == PAGECACHE_LOCK_LEFT_WRITELOCKED ?
                       PAGECACHE_PIN_LEFT_PINNED :
                       (lock == PAGECACHE_LOCK_WRITE_UNLOCK ?
                        PAGECACHE_UNPIN : PAGECACHE_PIN),
                       PAGECACHE_WRITE_DELAY, &page_link.link,
		       LSN_IMPOSSIBLE);

  if (lock == PAGECACHE_LOCK_WRITE)
  {
    /* It was not locked before, we have to unlock it when we unpin pages */
    page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
    page_link.changed= 1;
    push_dynamic(&info->pinned_pages, (void*) &page_link);
  }
  DBUG_RETURN(res);
}


/*
  @brief Put page in free list

  @fn    _ma_dispose()
  @param info		Maria handle
  @param pos	 	Address to page
  @param page_not_read  1 if page has not yet been read

  @note
    The page at 'pos' must have been read with a write lock

  @return
  @retval  0    ok
  @retval  1    error

*/

int _ma_dispose(register MARIA_HA *info, my_off_t pos, my_bool page_not_read)
{
  my_off_t old_link;
  uchar buff[MAX_KEYPAGE_HEADER_SIZE+ 8 + 2];
  ulonglong page_no;
  MARIA_SHARE *share= info->s;
  MARIA_PINNED_PAGE page_link;
  uint block_size= share->block_size;
  int result= 0;
  enum pagecache_page_lock lock_method;
  enum pagecache_page_pin pin_method;
  DBUG_ENTER("_ma_dispose");
  DBUG_PRINT("enter",("pos: %ld", (long) pos));
  DBUG_ASSERT(pos % block_size == 0);

  (void) _ma_lock_key_del(info, 0);

  old_link= share->current_key_del;
  share->current_key_del= pos;
  page_no= pos / block_size;
  bzero(buff, share->keypage_header);
  _ma_store_keynr(share, buff, (uchar) MARIA_DELETE_KEY_NR);
  _ma_store_page_used(share, buff, share->keypage_header + 8);
  mi_sizestore(buff + share->keypage_header, old_link);
  share->state.changed|= STATE_NOT_SORTED_PAGES;

  if (share->now_transactional)
  {
    LSN lsn;
    uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE * 2];
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    my_off_t page;

    /* Store address of deleted page */
    page_store(log_data + FILEID_STORE_SIZE, page_no);

    /* Store link to next unused page (the link that is written to page) */
    page= (old_link == HA_OFFSET_ERROR ? IMPOSSIBLE_PAGE_NO :
           old_link / block_size);
    page_store(log_data + FILEID_STORE_SIZE + PAGE_STORE_SIZE, page);

    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);

    if (translog_write_record(&lsn, LOGREC_REDO_INDEX_FREE_PAGE,
                              info->trn, info,
                              (translog_size_t) sizeof(log_data),
                              TRANSLOG_INTERNAL_PARTS + 1, log_array,
                              log_data, NULL))
      result= 1;
  }

  if (page_not_read)
  {
    lock_method= PAGECACHE_LOCK_WRITE;
    pin_method= PAGECACHE_PIN;
  }
  else
  {
    lock_method= PAGECACHE_LOCK_LEFT_WRITELOCKED;
    pin_method= PAGECACHE_PIN_LEFT_PINNED;
  }

  if (pagecache_write_part(share->pagecache,
                           &share->kfile, (pgcache_page_no_t) page_no,
                           PAGECACHE_PRIORITY_LOW, buff,
                           share->page_type,
                           lock_method, pin_method,
                           PAGECACHE_WRITE_DELAY, &page_link.link,
			   LSN_IMPOSSIBLE,
                           0, share->keypage_header + 8))
    result= 1;

#ifdef IDENTICAL_PAGES_AFTER_RECOVERY
  {
    uchar *page_buff= pagecache_block_link_to_buffer(page_link.link);
    bzero(page_buff + share->keypage_header + 8,
          block_size - share->keypage_header - 8 - KEYPAGE_CHECKSUM_SIZE);
  }
#endif

  if (page_not_read)
  {
    /* It was not locked before, we have to unlock it when we unpin pages */
    page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
    page_link.changed= 1;
    push_dynamic(&info->pinned_pages, (void*) &page_link);
  }

  DBUG_RETURN(result);
} /* _ma_dispose */


/**
  @brief Get address for free page to use

  @fn     _ma_new()
  @param  info		Maria handle
  @param  level         Type of key block (caching priority for pagecache)
  @param  page_link	Pointer to page in page cache if read. One can
                        check if this is used by checking if
                        page_link->changed != 0

  @return
    HA_OFFSET_ERROR     File is full or page read error
    #		        Page address to use
*/

my_off_t _ma_new(register MARIA_HA *info, int level,
                 MARIA_PINNED_PAGE **page_link)

{
  my_off_t pos;
  MARIA_SHARE *share= info->s;
  uint block_size= share->block_size;
  DBUG_ENTER("_ma_new");

  if (_ma_lock_key_del(info, 1))
  {
    pthread_mutex_lock(&share->intern_lock);
    pos= share->state.state.key_file_length;
    if (pos >= share->base.max_key_file_length - block_size)
    {
      my_errno=HA_ERR_INDEX_FILE_FULL;
      pthread_mutex_unlock(&share->intern_lock);
      DBUG_RETURN(HA_OFFSET_ERROR);
    }
    share->state.state.key_file_length+= block_size;
    /* Following is for not transactional tables */
    info->state->key_file_length= share->state.state.key_file_length;
    pthread_mutex_unlock(&share->intern_lock);
    (*page_link)->changed= 0;
    (*page_link)->write_lock= PAGECACHE_LOCK_WRITE;
  }
  else
  {
    uchar *buff;
    pos= share->current_key_del;                /* Protected */
    DBUG_ASSERT(share->pagecache->block_size == block_size);
    if (!(buff= pagecache_read(share->pagecache,
                               &share->kfile,
                               (pgcache_page_no_t) (pos / block_size), level,
                               0, share->page_type,
                               PAGECACHE_LOCK_WRITE, &(*page_link)->link)))
      pos= HA_OFFSET_ERROR;
    else
    {
      /*
        Next deleted page's number is in the header of the present page
        (single linked list):
      */
#ifndef DBUG_OFF
      my_off_t current_key_del;
#endif
      share->current_key_del= mi_sizekorr(buff+share->keypage_header);
#ifndef DBUG_OFF
      current_key_del= share->current_key_del;
      DBUG_ASSERT(current_key_del != share->state.key_del &&
                  (current_key_del != 0) &&
                  ((current_key_del == HA_OFFSET_ERROR) ||
                   (current_key_del <=
                    (share->state.state.key_file_length - block_size))));
#endif
    }

    (*page_link)->unlock=     PAGECACHE_LOCK_WRITE_UNLOCK;
    (*page_link)->write_lock= PAGECACHE_LOCK_WRITE;
    /*
      We have to mark it changed as _ma_flush_pending_blocks() uses
      'changed' to know if we used the page cache or not
    */
    (*page_link)->changed= 1;
    push_dynamic(&info->pinned_pages, (void*) *page_link);
    *page_link= dynamic_element(&info->pinned_pages,
                                info->pinned_pages.elements-1,
                                MARIA_PINNED_PAGE *);
  }
  share->state.changed|= STATE_NOT_SORTED_PAGES;
  DBUG_PRINT("exit",("Pos: %ld",(long) pos));
  DBUG_RETURN(pos);
} /* _ma_new */
