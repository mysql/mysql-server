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

/*
  Preload indexes into key cache
*/

#include "maria_def.h"


/*
  Preload pages of the index file for a table into the key cache

  SYNOPSIS
    maria_preload()
      info          open table
      map           map of indexes to preload into key cache
      ignore_leaves only non-leaves pages are to be preloaded

  RETURN VALUE
    0 if a success. error code - otherwise.

  NOTES.
    At present pages for all indexes are preloaded.
    In future only pages for indexes specified in the key_map parameter
    of the table will be preloaded.
    We don't yet use preload_buff_size (we read page after page).
*/

int maria_preload(MARIA_HA *info, ulonglong key_map, my_bool ignore_leaves)
{
  ulong block_length= 0;
  uchar *buff;
  MARIA_SHARE* share= info->s;
  uint keynr;
  my_off_t key_file_length= share->state.state.key_file_length;
  pgcache_page_no_t page_no, page_no_max;
  PAGECACHE_BLOCK_LINK *page_link;
  DBUG_ENTER("maria_preload");

  if (!share->state.header.keys || !maria_is_any_key_active(key_map) ||
      (key_file_length == share->base.keystart))
    DBUG_RETURN(0);

  block_length= share->pagecache->block_size;

  if (!(buff= (uchar *) my_malloc(block_length, MYF(MY_WME))))
    DBUG_RETURN(my_errno= HA_ERR_OUT_OF_MEM);

  if (flush_pagecache_blocks(share->pagecache, &share->kfile, FLUSH_RELEASE))
    goto err;

  /*
    Currently when we come here all other open instances of the table have
    been closed, and we flushed all pages of our own instance, so there
    cannot be any page of this table in the cache. Thus my_pread() would be
    safe. But in the future, we will allow more concurrency during
    preloading, so we use pagecache_read() instead of my_pread() because we
    observed that on some Linux, concurrent pread() and pwrite() (which
    could be from a page eviction by another thread) to the same page can
    make pread() see an half-written page.
    In this future, we should find a way to read state.key_file_length
    reliably, handle concurrent shrinks (delete_all_rows()) etc.
  */
  for ((page_no= share->base.keystart / block_length),
         (page_no_max= key_file_length / block_length);
       page_no < page_no_max; page_no++)
  {
    /**
      @todo instead of reading pages one by one we could have a call
      pagecache_read_several_pages() which does a single my_pread() for many
      consecutive pages (like the my_pread() in mi_preload()).
    */
    if (pagecache_read(share->pagecache, &share->kfile, page_no,
                       DFLT_INIT_HITS, buff, share->page_type,
                       PAGECACHE_LOCK_WRITE, &page_link) == NULL)
      goto err;
    keynr= _ma_get_keynr(share, buff);
    if (((ignore_leaves && !_ma_test_if_nod(share, buff)) ||
         keynr == MARIA_DELETE_KEY_NR ||
         !(key_map & ((ulonglong) 1 << keynr))) &&
        (pagecache_pagelevel(page_link) == DFLT_INIT_HITS))
    {
      /*
        This page is not interesting, and (last condition above) we are the
        ones who put it in the cache, so nobody else is interested in it.
      */
      if (pagecache_delete_by_link(share->pagecache, page_link,
                                   PAGECACHE_LOCK_LEFT_WRITELOCKED, FALSE))
        goto err;
    }
    else /* otherwise it stays in cache: */
      pagecache_unlock_by_link(share->pagecache, page_link,
                               PAGECACHE_LOCK_WRITE_UNLOCK, PAGECACHE_UNPIN,
                               LSN_IMPOSSIBLE, LSN_IMPOSSIBLE, FALSE, FALSE);
  }

  my_free(buff);
  DBUG_RETURN(0);

err:
  my_free(buff);
  DBUG_RETURN(my_errno= errno);
}
