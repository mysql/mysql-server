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

/*
  Bitmap handling (for records in block)

  The data file starts with a bitmap page, followed by as many data
  pages as the bitmap can cover. After this there is a new bitmap page
  and more data pages etc.

  The bitmap code assumes there is always an active bitmap page and thus
  that there is at least one bitmap page in the file

  Structure of bitmap page:

  Fixed size records (to be implemented later):

  2 bits are used to indicate:

  0      Empty
  1      0-75 % full  (at least room for 2 records)
  2      75-100 % full (at least room for one record)
  3      100 % full    (no more room for records)

  Assuming 8K pages, this will allow us to map:
  8192 (bytes per page) * 4 (pages mapped per byte) * 8192 (page size)= 256M

  (For Maria this will be 7*4 * 8192 = 224K smaller because of LSN)

  Note that for fixed size rows, we can't add more columns without doing
  a full reorganization of the table. The user can always force a dynamic
  size row format by specifying ROW_FORMAT=dynamic.


  Dynamic size records:

  3 bits are used to indicate				Bytes free in 8K page

  0      Empty page					8176 (head or tail)
  1      0-30 % full  (at least room for 3 records)	5724
  2      30-60 % full (at least room for 2 records)	3271
  3      60-90 % full (at least room for one record)	818
  4      100 % full   (no more room for records)	0
  5      Tail page,  0-40 % full			4906
  6      Tail page,  40-80 % full			1636
  7      Full tail page or full blob page		0

  Assuming 8K pages, this will allow us to map:
  8192 (bytes per page) * 8 bits/byte / 3 bits/page * 8192 (page size)= 170.7M

  Note that values 1-3 may be adjust for each individual table based on
  'min record length'.  Tail pages are for overflow data which can be of
  any size and thus doesn't have to be adjusted for different tables.
  If we add more columns to the table, some of the originally calculated
  'cut off' points may not be optimal, but they shouldn't be 'drasticly
  wrong'.

  When allocating data from the bitmap, we are trying to do it in a
  'best fit' manner. Blobs and varchar blocks are given out in large
  continuous extents to allow fast access to these. Before allowing a
  row to 'flow over' to other blocks, we will compact the page and use
  all space on it. If there is many rows in the page, we will ensure
  there is *LEFT_TO_GROW_ON_SPLIT* bytes left on the page to allow other
  rows to grow.

  The bitmap format allows us to extend the row file in big chunks, if needed.

  When calculating the size for a packed row, we will calculate the following
  things separately:
  - Row header + null_bits + empty_bits fixed size segments etc.
  - Size of all char/varchar fields
  - Size of each blob field

  The bitmap handler will get all the above information and return
  either one page or a set of pages to put the different parts.

  Bitmaps are read on demand in response to insert/delete/update operations.
  The following bitmap pointers will be cached and stored on disk on close:
  - Current insert_bitmap;  When inserting new data we will first try to
    fill this one.
  - First bitmap which is not completely full.  This is updated when we
    free data with an update or delete.

  While flushing out bitmaps, we will cache the status of the bitmap in memory
  to avoid having to read a bitmap for insert of new data that will not
  be of any use
  - Total empty space
  - Largest number of continuous pages

  Bitmap ONLY goes to disk in the following scenarios
  - The file is closed (and we flush all changes to disk)
  - On checkpoint
  (Ie: When we do a checkpoint, we have to ensure that all bitmaps are
  put on disk even if they are not in the page cache).
  - When explicitely requested (for example on backup or after recvoery,
  to simplify things)

 The flow of writing a row is that:
 - Lock the bitmap
 - Decide which data pages we will write to
 - Mark them full in the bitmap page so that other threads do not try to
    use the same data pages as us
 - We unlock the bitmap
 - Write the data pages
 - Lock the bitmap
 - Correct the bitmap page with the true final occupation of the data
   pages (that is, we marked pages full but when we are done we realize
   we didn't fill them)
 - Unlock the bitmap.
*/

#include "maria_def.h"
#include "ma_blockrec.h"

#define FULL_HEAD_PAGE 4
#define FULL_TAIL_PAGE 7

/*#define WRONG_BITMAP_FLUSH 1*/ /*define only for provoking bugs*/
#undef WRONG_BITMAP_FLUSH

static my_bool _ma_read_bitmap_page(MARIA_HA *info,
                                    MARIA_FILE_BITMAP *bitmap,
                                    pgcache_page_no_t page);
static my_bool _ma_bitmap_create_missing(MARIA_HA *info,
                                         MARIA_FILE_BITMAP *bitmap,
                                         pgcache_page_no_t page);

/* Write bitmap page to key cache */

static inline my_bool write_changed_bitmap(MARIA_SHARE *share,
                                           MARIA_FILE_BITMAP *bitmap)
{
  DBUG_ENTER("write_changed_bitmap");
  DBUG_ASSERT(share->pagecache->block_size == bitmap->block_size);
  DBUG_ASSERT(bitmap->file.write_callback != 0);
  DBUG_PRINT("info", ("bitmap->non_flushable: %u", bitmap->non_flushable));

  /*
    Mark that a bitmap page has been written to page cache and we have
    to flush it during checkpoint.
  */
  bitmap->changed_not_flushed= 1;

  if ((bitmap->non_flushable == 0)
#ifdef WRONG_BITMAP_FLUSH
      || 1
#endif
      )
  {
    my_bool res= pagecache_write(share->pagecache,
                                 &bitmap->file, bitmap->page, 0,
                                 bitmap->map, PAGECACHE_PLAIN_PAGE,
                                 PAGECACHE_LOCK_LEFT_UNLOCKED,
                                 PAGECACHE_PIN_LEFT_UNPINNED,
                                 PAGECACHE_WRITE_DELAY, 0, LSN_IMPOSSIBLE);
    DBUG_RETURN(res);
  }
  else
  {
    MARIA_PINNED_PAGE page_link;
    int res= pagecache_write(share->pagecache,
                             &bitmap->file, bitmap->page, 0,
                             bitmap->map, PAGECACHE_PLAIN_PAGE,
                             PAGECACHE_LOCK_LEFT_UNLOCKED, PAGECACHE_PIN,
                             PAGECACHE_WRITE_DELAY, &page_link.link,
                             LSN_IMPOSSIBLE);
    page_link.unlock= PAGECACHE_LOCK_LEFT_UNLOCKED;
    page_link.changed= 1;
    push_dynamic(&bitmap->pinned_pages, (void*) &page_link);
    DBUG_RETURN(res);
  }
}

/*
  Initialize bitmap variables in share

  SYNOPSIS
    _ma_bitmap_init()
    share		Share handler
    file		data file handler

  NOTES
   This is called the first time a file is opened.

  RETURN
    0   ok
    1   error
*/

my_bool _ma_bitmap_init(MARIA_SHARE *share, File file)
{
  uint aligned_bit_blocks;
  uint max_page_size;
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  uint size= share->block_size;
#ifndef DBUG_OFF
  /* We want to have a copy of the bitmap to be able to print differences */
  size*= 2;
#endif

  if (((bitmap->map= (uchar*) my_malloc(size, MYF(MY_WME))) == NULL) ||
      my_init_dynamic_array(&bitmap->pinned_pages,
                            sizeof(MARIA_PINNED_PAGE), 1, 1))
    return 1;

  bitmap->block_size= share->block_size;
  bitmap->file.file= file;
  _ma_bitmap_set_pagecache_callbacks(&bitmap->file, share);

  /* Size needs to be aligned on 6 */
  aligned_bit_blocks= (share->block_size - PAGE_SUFFIX_SIZE) / 6;
  bitmap->total_size= aligned_bit_blocks * 6;
  /*
    In each 6 bytes, we have 6*8/3 = 16 pages covered
    The +1 is to add the bitmap page, as this doesn't have to be covered
  */
  bitmap->pages_covered= aligned_bit_blocks * 16 + 1;
  bitmap->flush_all_requested= 0;
  bitmap->non_flushable= 0;

  /* Update size for bits */
  /* TODO; Make this dependent of the row size */
  max_page_size= share->block_size - PAGE_OVERHEAD_SIZE + DIR_ENTRY_SIZE;
  bitmap->sizes[0]= max_page_size;              /* Empty page */
  bitmap->sizes[1]= max_page_size - max_page_size * 30 / 100;
  bitmap->sizes[2]= max_page_size - max_page_size * 60 / 100;
  bitmap->sizes[3]= max_page_size - max_page_size * 90 / 100;
  bitmap->sizes[4]= 0;                          /* Full page */
  bitmap->sizes[5]= max_page_size - max_page_size * 40 / 100;
  bitmap->sizes[6]= max_page_size - max_page_size * 80 / 100;
  bitmap->sizes[7]= 0;

  pthread_mutex_init(&share->bitmap.bitmap_lock, MY_MUTEX_INIT_SLOW);
  pthread_cond_init(&share->bitmap.bitmap_cond, 0);

  _ma_bitmap_reset_cache(share);

  if (share->state.first_bitmap_with_space == ~(pgcache_page_no_t) 0)
  {
    /* Start scanning for free space from start of file */
    share->state.first_bitmap_with_space = 0;
  }
  return 0;
}


/*
  Free data allocated by _ma_bitmap_init

  SYNOPSIS
    _ma_bitmap_end()
    share		Share handler
*/

my_bool _ma_bitmap_end(MARIA_SHARE *share)
{
  my_bool res= _ma_bitmap_flush(share);
  safe_mutex_assert_owner(&share->close_lock);
  pthread_mutex_destroy(&share->bitmap.bitmap_lock);
  pthread_cond_destroy(&share->bitmap.bitmap_cond);
  delete_dynamic(&share->bitmap.pinned_pages);
  my_free(share->bitmap.map, MYF(MY_ALLOW_ZERO_PTR));
  share->bitmap.map= 0;
  return res;
}


/*
  Send updated bitmap to the page cache

  SYNOPSIS
    _ma_bitmap_flush()
    share		Share handler

  NOTES
    In the future, _ma_bitmap_flush() will be called to flush changes don't
    by this thread (ie, checking the changed flag is ok). The reason we
    check it again in the mutex is that if someone else did a flush at the
    same time, we don't have to do the write.
    This is also ok for _ma_scan_init_block_record() which does not want to
    miss rows: it cares only for committed rows, that is, rows for which there
    was a commit before our transaction started; as commit and transaction's
    start are protected by the same LOCK_trn_list mutex, we see memory at
    least as new as at other transaction's commit time, so if the committed
    rows caused bitmap->changed to be true, we see it; if we see 0 it really
    means a flush happened since then. So, it's ok to read without bitmap's
    mutex.

  RETURN
    0    ok
    1    error
*/

my_bool _ma_bitmap_flush(MARIA_SHARE *share)
{
  my_bool res= 0;
  DBUG_ENTER("_ma_bitmap_flush");
  if (share->bitmap.changed)
  {
    pthread_mutex_lock(&share->bitmap.bitmap_lock);
    if (share->bitmap.changed)
    {
      res= write_changed_bitmap(share, &share->bitmap);
      share->bitmap.changed= 0;
    }
    pthread_mutex_unlock(&share->bitmap.bitmap_lock);
  }
  DBUG_RETURN(res);
}


/**
   Dirty-page filtering criteria for bitmap pages

   @param  type                Page's type
   @param  pageno              Page's number
   @param  rec_lsn             Page's rec_lsn
   @param  arg                 pages_covered of bitmap
*/

static enum pagecache_flush_filter_result
filter_flush_bitmap_pages(enum pagecache_page_type type
                          __attribute__ ((unused)),
                          pgcache_page_no_t pageno,
                          LSN rec_lsn __attribute__ ((unused)),
                          void *arg)
{
  return ((pageno % (*(ulong*)arg)) == 0);
}


/**
   Flushes current bitmap page to the pagecache, and then all bitmap pages
   from pagecache to the file. Used by Checkpoint.

   @param  share               Table's share
*/

my_bool _ma_bitmap_flush_all(MARIA_SHARE *share)
{
  my_bool res= 0;
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  DBUG_ENTER("_ma_bitmap_flush_all");
  pthread_mutex_lock(&bitmap->bitmap_lock);
  if (bitmap->changed || bitmap->changed_not_flushed)
  {
    bitmap->flush_all_requested++;
#ifndef WRONG_BITMAP_FLUSH
    while (bitmap->non_flushable > 0)
    {
      DBUG_PRINT("info", ("waiting for bitmap to be flushable"));
      pthread_cond_wait(&bitmap->bitmap_cond, &bitmap->bitmap_lock);
    }
#endif
    DBUG_ASSERT(bitmap->flush_all_requested == 1);
    /*
      Bitmap is in a flushable state: its contents in memory are reflected by
      log records (complete REDO-UNDO groups) and all bitmap pages are
      unpinned. We keep the mutex to preserve this situation, and flush to the
      file.
    */
    if (bitmap->changed)
    {
      bitmap->changed= FALSE;
      res= write_changed_bitmap(share, bitmap);
    }
    /*
      We do NOT use FLUSH_KEEP_LAZY because we must be sure that bitmap
      pages have been flushed. That's a condition of correctness of
      Recovery: data pages may have been all flushed, if we write the
      checkpoint record Recovery will start from after their REDOs. If
      bitmap page was not flushed, as the REDOs about it will be skipped, it
      will wrongly not be recovered. If bitmap pages had a rec_lsn it would
      be different.
      There should be no pinned pages as bitmap->non_flushable==0.
    */
    if (flush_pagecache_blocks_with_filter(share->pagecache,
                                           &bitmap->file, FLUSH_KEEP,
                                           filter_flush_bitmap_pages,
                                           &bitmap->pages_covered) &
        PCFLUSH_PINNED_AND_ERROR)
      res= TRUE;
    bitmap->changed_not_flushed= FALSE;
    bitmap->flush_all_requested--;
    /*
      Some well-behaved threads may be waiting for flush_all_requested to
      become false, wake them up.
    */
    DBUG_PRINT("info", ("bitmap flusher waking up others"));
    pthread_cond_broadcast(&bitmap->bitmap_cond);
  }
  pthread_mutex_unlock(&bitmap->bitmap_lock);
  DBUG_RETURN(res);
}


/**
   @brief Lock bitmap from being used by another thread

   @fn _ma_bitmap_lock()
   @param  share               Table's share

   @notes
   This is a temporary solution for allowing someone to delete an inserted
   duplicate-key row while someone else is doing concurrent inserts.
   This is ok for now as duplicate key errors are not that common.

   In the future we will add locks for row-pages to ensure two threads doesn't
   work at the same time on the same page.
*/

void _ma_bitmap_lock(MARIA_SHARE *share)
{
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  DBUG_ENTER("_ma_bitmap_lock");

  if (!share->now_transactional)
    DBUG_VOID_RETURN;

  pthread_mutex_lock(&bitmap->bitmap_lock);
  bitmap->flush_all_requested++;
  while (bitmap->non_flushable)
  {
    DBUG_PRINT("info", ("waiting for bitmap to be flushable"));
    pthread_cond_wait(&bitmap->bitmap_cond, &bitmap->bitmap_lock);
  }
  /*
    Ensure that _ma_bitmap_flush_all() and _ma_bitmap_lock() are blocked.
    ma_bitmap_flushable() is blocked thanks to 'flush_all_requested'.
  */
  bitmap->non_flushable= 1;
  pthread_mutex_unlock(&bitmap->bitmap_lock);
  DBUG_VOID_RETURN;
}
  
/**
   @brief Unlock bitmap after _ma_bitmap_lock()

   @fn _ma_bitmap_unlock()
   @param  share               Table's share
*/

void _ma_bitmap_unlock(MARIA_SHARE *share)
{
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  DBUG_ENTER("_ma_bitmap_unlock");

  if (!share->now_transactional)
    DBUG_VOID_RETURN;
  DBUG_ASSERT(bitmap->flush_all_requested > 0 && bitmap->non_flushable == 1);

  pthread_mutex_lock(&bitmap->bitmap_lock);
  bitmap->flush_all_requested--;
  bitmap->non_flushable= 0;
  pthread_mutex_unlock(&bitmap->bitmap_lock);
  pthread_cond_broadcast(&bitmap->bitmap_cond);
  DBUG_VOID_RETURN;
}


/**
  @brief Unpin all pinned bitmap pages

  @param  share            Table's share

  @return Operation status
    @retval   0   ok

  @note This unpins pages pinned by other threads.
*/

static void _ma_bitmap_unpin_all(MARIA_SHARE *share)
{
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  MARIA_PINNED_PAGE *page_link= ((MARIA_PINNED_PAGE*)
                                 dynamic_array_ptr(&bitmap->pinned_pages, 0));
  MARIA_PINNED_PAGE *pinned_page= page_link + bitmap->pinned_pages.elements;
  DBUG_ENTER("_ma_bitmap_unpin_all");
  DBUG_PRINT("info", ("pinned: %u", bitmap->pinned_pages.elements));
  while (pinned_page-- != page_link)
    pagecache_unlock_by_link(share->pagecache, pinned_page->link,
                             pinned_page->unlock, PAGECACHE_UNPIN,
                             LSN_IMPOSSIBLE, LSN_IMPOSSIBLE, TRUE, TRUE);
  bitmap->pinned_pages.elements= 0;
  DBUG_VOID_RETURN;
}


/*
  Intialize bitmap in memory to a zero bitmap

  SYNOPSIS
    _ma_bitmap_delete_all()
    share		Share handler

  NOTES
    This is called on maria_delete_all_rows (truncate data file).
*/

void _ma_bitmap_delete_all(MARIA_SHARE *share)
{
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  DBUG_ENTER("_ma_bitmap_delete_all");
  if (bitmap->map)                              /* Not in create */
  {
    bzero(bitmap->map, bitmap->block_size);
    bitmap->changed= 1;
    bitmap->page= 0;
    bitmap->used_size= bitmap->total_size;
  }
  DBUG_VOID_RETURN;
}


/**
   @brief Reset bitmap caches

   @fn    _ma_bitmap_reset_cache()
   @param share		Maria share

   @notes
   This is called after we have swapped file descriptors and we want
   bitmap to forget all cached information
*/

void _ma_bitmap_reset_cache(MARIA_SHARE *share)
{
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;

  if (bitmap->map)                              /* If using bitmap */
  {
    /* Forget changes in current bitmap page */
    bitmap->changed= 0;

    /*
      We can't read a page yet, as in some case we don't have an active
      page cache yet.
      Pretend we have a dummy, full and not changed bitmap page in memory.
    */
    bitmap->page= ~(ulonglong) 0;
    bitmap->used_size= bitmap->total_size;
    bfill(bitmap->map, share->block_size, 255);
#ifndef DBUG_OFF
    memcpy(bitmap->map + bitmap->block_size, bitmap->map, bitmap->block_size);
#endif
  }
}


/*
  Return bitmap pattern for the smallest head block that can hold 'size'

  SYNOPSIS
    size_to_head_pattern()
    bitmap      Bitmap
    size        Requested size

  RETURN
    0-3         For a description of the bitmap sizes, see the header
*/

static uint size_to_head_pattern(MARIA_FILE_BITMAP *bitmap, uint size)
{
  if (size <= bitmap->sizes[3])
    return 3;
  if (size <= bitmap->sizes[2])
    return 2;
  if (size <= bitmap->sizes[1])
    return 1;
  DBUG_ASSERT(size <= bitmap->sizes[0]);
  return 0;
}


/*
  Return bitmap pattern for head block where there is size bytes free

  SYNOPSIS
    _ma_free_size_to_head_pattern()
    bitmap      Bitmap
    size        Requested size

  RETURN
    0-4  (Possible bitmap patterns for head block)
*/

uint _ma_free_size_to_head_pattern(MARIA_FILE_BITMAP *bitmap, uint size)
{
  if (size < bitmap->sizes[3])
    return 4;
  if (size < bitmap->sizes[2])
    return 3;
  if (size < bitmap->sizes[1])
    return 2;
  return (size < bitmap->sizes[0]) ? 1 : 0;
}


/*
  Return bitmap pattern for the smallest tail block that can hold 'size'

  SYNOPSIS
    size_to_tail_pattern()
    bitmap      Bitmap
    size        Requested size

  RETURN
    0, 5 or 6   For a description of the bitmap sizes, see the header
*/

static uint size_to_tail_pattern(MARIA_FILE_BITMAP *bitmap, uint size)
{
  if (size <= bitmap->sizes[6])
    return 6;
  if (size <= bitmap->sizes[5])
    return 5;
  DBUG_ASSERT(size <= bitmap->sizes[0]);
  return 0;
}


/*
  Return bitmap pattern for tail block where there is size bytes free

  SYNOPSIS
    free_size_to_tail_pattern()
    bitmap      Bitmap
    size        Requested size

  RETURN
    0, 5, 6, 7   For a description of the bitmap sizes, see the header
*/

static uint free_size_to_tail_pattern(MARIA_FILE_BITMAP *bitmap, uint size)
{
  if (size >= bitmap->sizes[0])
    return 0;                                   /* Revert to empty page */
  if (size < bitmap->sizes[6])
    return 7;
  if (size < bitmap->sizes[5])
    return 6;
  return 5;
}


/*
  Return size guranteed to be available on a page

  SYNOPSIS
    pattern_to_head_size()
    bitmap      Bitmap
    pattern     Pattern (0-7)

  RETURN
    0 - block_size
*/

static inline uint pattern_to_size(MARIA_FILE_BITMAP *bitmap, uint pattern)
{
  DBUG_ASSERT(pattern <= 7);
  return bitmap->sizes[pattern];
}


/*
  Print bitmap for debugging

  SYNOPSIS
  _ma_print_bitmap()
  bitmap	Bitmap to print

  IMPLEMENTATION
    Prints all changed bits since last call to _ma_print_bitmap().
    This is done by having a copy of the last bitmap in
    bitmap->map+bitmap->block_size.
*/

#ifndef DBUG_OFF

const char *bits_to_txt[]=
{
  "empty", "00-30% full", "30-60% full", "60-90% full", "full",
  "tail 00-40 % full", "tail 40-80 % full", "tail/blob full"
};

static void _ma_print_bitmap_changes(MARIA_FILE_BITMAP *bitmap)
{
  uchar *pos, *end, *org_pos;
  ulong page;
  DBUG_ENTER("_ma_print_bitmap_changes");

  end= bitmap->map + bitmap->used_size;
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE,"\nBitmap page changes at page: %lu  bitmap: 0x%lx\n",
          (ulong) bitmap->page, (long) bitmap->map);

  page= (ulong) bitmap->page+1;
  for (pos= bitmap->map, org_pos= bitmap->map + bitmap->block_size ;
       pos < end ;
       pos+= 6, org_pos+= 6)
  {
    ulonglong bits= uint6korr(pos);    /* 6 bytes = 6*8/3= 16 patterns */
    ulonglong org_bits= uint6korr(org_pos);
    uint i;

    /*
      Test if there is any changes in the next 16 bitmaps (to not have to
      loop through all bits if we know they are the same)
    */
    if (bits != org_bits)
    {
      for (i= 0; i < 16 ; i++, bits>>= 3, org_bits>>= 3)
      {
        if ((bits & 7) != (org_bits & 7))
          fprintf(DBUG_FILE, "Page: %8lu  %s -> %s\n", page+i,
                  bits_to_txt[org_bits & 7], bits_to_txt[bits & 7]);
      }
    }
    page+= 16;
  }
  fputc('\n', DBUG_FILE);
  DBUG_UNLOCK_FILE;
  memcpy(bitmap->map + bitmap->block_size, bitmap->map, bitmap->block_size);
  DBUG_VOID_RETURN;
}


/* Print content of bitmap for debugging */

void _ma_print_bitmap(MARIA_FILE_BITMAP *bitmap, uchar *data,
                      pgcache_page_no_t page)
{
  uchar *pos, *end;
  char llbuff[22];

  end= bitmap->map + bitmap->used_size;
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE,"\nDump of bitmap page at %s\n", llstr(page, llbuff));

  page++;                                       /* Skip bitmap page */
  for (pos= data, end= pos + bitmap->total_size;
       pos < end ;
       pos+= 6)
  {
    ulonglong bits= uint6korr(pos);    /* 6 bytes = 6*8/3= 16 patterns */

    /*
      Test if there is any changes in the next 16 bitmaps (to not have to
      loop through all bits if we know they are the same)
    */
    if (bits)
    {
      uint i;
      for (i= 0; i < 16 ; i++, bits>>= 3)
      {
        if (bits & 7)
          fprintf(DBUG_FILE, "Page: %8s  %s\n", llstr(page+i, llbuff),
                  bits_to_txt[bits & 7]);
      }
    }
    page+= 16;
  }
  fputc('\n', DBUG_FILE);
  DBUG_UNLOCK_FILE;
}

#endif /* DBUG_OFF */


/***************************************************************************
  Reading & writing bitmap pages
***************************************************************************/

/*
  Read a given bitmap page

  SYNOPSIS
    _ma_read_bitmap_page()
    info                Maria handler
    bitmap              Bitmap handler
    page                Page to read

  TODO
    Update 'bitmap->used_size' to real size of used bitmap

  NOTE
    We don't always have share->bitmap.bitmap_lock here
    (when called from_ma_check_bitmap_data() for example).

  RETURN
    0  ok
    1  error  (Error writing old bitmap or reading bitmap page)
*/

static my_bool _ma_read_bitmap_page(MARIA_HA *info,
                                    MARIA_FILE_BITMAP *bitmap,
                                    pgcache_page_no_t page)
{
  MARIA_SHARE *share= info->s;
  my_bool res;
  DBUG_ENTER("_ma_read_bitmap_page");
  DBUG_ASSERT(page % bitmap->pages_covered == 0);
  DBUG_ASSERT(!bitmap->changed);

  bitmap->page= page;
  if (((page + 1) * bitmap->block_size) > share->state.state.data_file_length)
  {
    /* Inexistent or half-created page */
    res= _ma_bitmap_create_missing(info, bitmap, page);
    DBUG_RETURN(res);
  }
  bitmap->used_size= bitmap->total_size;
  DBUG_ASSERT(share->pagecache->block_size == bitmap->block_size);
  res= pagecache_read(share->pagecache,
                      &bitmap->file, page, 0,
                      bitmap->map, PAGECACHE_PLAIN_PAGE,
                      PAGECACHE_LOCK_LEFT_UNLOCKED, 0) == NULL;

  /*
    We can't check maria_bitmap_marker here as if the bitmap page
    previously had a true checksum and the user switched mode to not checksum
    this may have any value, except maria_normal_page_marker.

    Using maria_normal_page_marker gives us a protection against bugs
    when running without any checksums.
  */

#ifndef DBUG_OFF
  if (!res)
    memcpy(bitmap->map + bitmap->block_size, bitmap->map, bitmap->block_size);
#endif
  DBUG_RETURN(res);
}


/*
  Change to another bitmap page

  SYNOPSIS
  _ma_change_bitmap_page()
    info                Maria handler
    bitmap              Bitmap handler
    page                Bitmap page to read

  NOTES
   If old bitmap was changed, write it out before reading new one
   We return empty bitmap if page is outside of file size

  RETURN
    0  ok
    1  error  (Error writing old bitmap or reading bitmap page)
*/

static my_bool _ma_change_bitmap_page(MARIA_HA *info,
                                      MARIA_FILE_BITMAP *bitmap,
                                      pgcache_page_no_t page)
{
  DBUG_ENTER("_ma_change_bitmap_page");

  if (bitmap->changed)
  {
    if (write_changed_bitmap(info->s, bitmap))
      DBUG_RETURN(1);
    bitmap->changed= 0;
  }
  DBUG_RETURN(_ma_read_bitmap_page(info, bitmap, page));
}


/*
  Read next suitable bitmap

  SYNOPSIS
    move_to_next_bitmap()
    bitmap              Bitmap handle

  NOTES
    The found bitmap may be full, so calling function may need to call this
    repeatedly until it finds enough space.

  TODO
    Add cache of bitmaps to not read something that is not usable

  RETURN
    0  ok
    1  error (either couldn't save old bitmap or read new one)
*/

static my_bool move_to_next_bitmap(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap)
{
  pgcache_page_no_t page= bitmap->page;
  MARIA_STATE_INFO *state= &info->s->state;
  DBUG_ENTER("move_to_next_bitmap");

  if (state->first_bitmap_with_space != ~(ulonglong) 0 &&
      state->first_bitmap_with_space != page)
  {
    page= state->first_bitmap_with_space;
    state->first_bitmap_with_space= ~(ulonglong) 0;
  }
  else
    page+= bitmap->pages_covered;
  DBUG_RETURN(_ma_change_bitmap_page(info, bitmap, page));
}


/****************************************************************************
 Allocate data in bitmaps
****************************************************************************/

/*
  Store data in 'block' and mark the place used in the bitmap

  SYNOPSIS
    fill_block()
    bitmap		Bitmap handle
    block		Store data about what we found
    best_data		Pointer to best 6 uchar aligned area in bitmap->map
    best_pos		Which bit in *best_data the area starts
                        0 = first bit pattern, 1 second bit pattern etc
    best_bits		The original value of the bits at best_pos
    fill_pattern	Bitmap pattern to store in best_data[best_pos]

   NOTES
    We mark all pages to be 'TAIL's, which means that
    block->page_count is really a row position inside the page.
*/

static void fill_block(MARIA_FILE_BITMAP *bitmap,
                       MARIA_BITMAP_BLOCK *block,
                       uchar *best_data, uint best_pos, uint best_bits,
                       uint fill_pattern)
{
  uint page, offset, tmp;
  uchar *data;
  DBUG_ENTER("fill_block");

  /* For each 6 bytes we have 6*8/3= 16 patterns */
  page= ((uint) (best_data - bitmap->map)) / 6 * 16 + best_pos;
  DBUG_ASSERT(page + 1 < bitmap->pages_covered);
  block->page= bitmap->page + 1 + page;
  block->page_count= TAIL_PAGE_COUNT_MARKER;
  block->empty_space= pattern_to_size(bitmap, best_bits);
  block->sub_blocks= 0;
  block->org_bitmap_value= best_bits;
  block->used= BLOCKUSED_TAIL; /* See _ma_bitmap_release_unused() */

  /*
    Mark place used by reading/writing 2 bytes at a time to handle
    bitmaps in overlapping bytes
  */
  best_pos*= 3;
  data= best_data+ best_pos / 8;
  offset= best_pos & 7;
  tmp= uint2korr(data);

  /* we turn off the 3 bits and replace them with fill_pattern */
  tmp= (tmp & ~(7 << offset)) | (fill_pattern << offset);
  int2store(data, tmp);
  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap_changes(bitmap););
  DBUG_VOID_RETURN;
}


/*
  Allocate data for head block

  SYNOPSIS
   allocate_head()
   bitmap       bitmap
   size         Size of data region we need to store
   block        Store found information here

   IMPLEMENTATION
     Find the best-fit page to put a region of 'size'
     This is defined as the first page of the set of pages
     with the smallest free space that can hold 'size'.

   RETURN
    0   ok    (block is updated)
    1   error (no space in bitmap; block is not touched)
*/


static my_bool allocate_head(MARIA_FILE_BITMAP *bitmap, uint size,
                             MARIA_BITMAP_BLOCK *block)
{
  uint min_bits= size_to_head_pattern(bitmap, size);
  uchar *data= bitmap->map, *end= data + bitmap->used_size;
  uchar *best_data= 0;
  uint best_bits= (uint) -1, best_pos;
  DBUG_ENTER("allocate_head");

  LINT_INIT(best_pos);
  DBUG_ASSERT(size <= FULL_PAGE_SIZE(bitmap->block_size));

  for (; data < end; data+= 6)
  {
    ulonglong bits= uint6korr(data);    /* 6 bytes = 6*8/3= 16 patterns */
    uint i;

    /*
      Skip common patterns
      We can skip empty pages (if we already found a match) or
      anything matching the following pattern as this will be either
      a full page or a tail page
    */
    if ((!bits && best_data) ||
        ((bits & LL(04444444444444444)) == LL(04444444444444444)))
      continue;
    for (i= 0; i < 16 ; i++, bits >>= 3)
    {
      uint pattern= (uint) (bits & 7);
      if (pattern <= min_bits)
      {
        /* There is enough space here */
        if ((int) pattern > (int) best_bits)
        {
          /*
            There is more than enough space here and it's better than what
            we have found so far. Remember it, as we will choose it if we
            don't find anything in this bitmap page.
          */
          best_bits= pattern;
          best_data= data;
          best_pos= i;
          if (pattern == min_bits)
            goto found;                         /* Best possible match */
        }
      }
    }
  }
  if (!best_data)                               /* Found no place */
  {
    if (data >= bitmap->map + bitmap->total_size)
      DBUG_RETURN(1);                           /* No space in bitmap */
    /* Allocate data at end of bitmap */
    bitmap->used_size+= 6;
    set_if_smaller(bitmap->used_size, bitmap->total_size);
    best_data= data;
    best_pos= best_bits= 0;
  }

found:
  fill_block(bitmap, block, best_data, best_pos, best_bits, FULL_HEAD_PAGE);
  DBUG_RETURN(0);
}


/*
  Allocate data for tail block

  SYNOPSIS
   allocate_tail()
   bitmap       bitmap
   size         Size of block we need to find
   block        Store found information here

  RETURN
   0    ok      (block is updated)
   1    error   (no space in bitmap; block is not touched)
*/


static my_bool allocate_tail(MARIA_FILE_BITMAP *bitmap, uint size,
                             MARIA_BITMAP_BLOCK *block)
{
  uint min_bits= size_to_tail_pattern(bitmap, size);
  uchar *data= bitmap->map, *end= data + bitmap->used_size;
  uchar *best_data= 0;
  uint best_bits= (uint) -1, best_pos;
  DBUG_ENTER("allocate_tail");
  DBUG_PRINT("enter", ("size: %u", size));

  LINT_INIT(best_pos);
  /*
    We have to add DIR_ENTRY_SIZE here as this is not part of the data size
    See call to allocate_tail() in find_tail().
  */
  DBUG_ASSERT(size <= MAX_TAIL_SIZE(bitmap->block_size) + DIR_ENTRY_SIZE);

  for (; data < end; data += 6)
  {
    ulonglong bits= uint6korr(data);    /* 6 bytes = 6*8/3= 16 patterns */
    uint i;

    /*
      Skip common patterns
      We can skip empty pages (if we already found a match) or
      the following patterns: 1-4 (head pages, not suitable for tail) or
      7 (full tail page). See 'Dynamic size records' comment at start of file.

      At the moment we only skip full head and tail pages (ie, all bits are
      set) as this is easy to detect with one simple test and is a
      quite common case if we have blobs.
    */

    if ((!bits && best_data) || bits == LL(0xffffffffffff) ||
        bits == LL(04444444444444444))
      continue;
    for (i= 0; i < 16; i++, bits >>= 3)
    {
      uint pattern= (uint) (bits & 7);
      if (pattern <= min_bits && (!pattern || pattern >= 5))
      {
        if ((int) pattern > (int) best_bits)
        {
          best_bits= pattern;
          best_data= data;
          best_pos= i;
          if (pattern == min_bits)
            goto found;                         /* Can't be better */
        }
      }
    }
  }
  if (!best_data)
  {
    if (data >= bitmap->map + bitmap->total_size)
      DBUG_RETURN(1);
    /* Allocate data at end of bitmap */
    best_data= data;
    bitmap->used_size+= 6;
    set_if_smaller(bitmap->used_size, bitmap->total_size);
    best_pos= best_bits= 0;
  }

found:
  fill_block(bitmap, block, best_data, best_pos, best_bits, FULL_TAIL_PAGE);
  DBUG_RETURN(0);
}


/*
  Allocate data for full blocks

  SYNOPSIS
   allocate_full_pages()
   bitmap       bitmap
   pages_needed Total size in pages (bitmap->total_size) we would like to have
   block        Store found information here
   full_page    1 if we are not allowed to split extent

  IMPLEMENTATION
    We will return the smallest area >= size.  If there is no such
    block, we will return the biggest area that satisfies
    area_size >= min(BLOB_SEGMENT_MIN_SIZE*full_page_size, size)

    To speed up searches, we will only consider areas that has at least 16 free
    pages starting on an even boundary.  When finding such an area, we will
    extend it with all previous and following free pages.  This will ensure
    we don't get holes between areas

  RETURN
   #            Blocks used
   0            error   (no space in bitmap; block is not touched)
*/

static ulong allocate_full_pages(MARIA_FILE_BITMAP *bitmap,
                                 ulong pages_needed,
                                 MARIA_BITMAP_BLOCK *block, my_bool full_page)
{
  uchar *data= bitmap->map, *data_end= data + bitmap->used_size;
  uchar *page_end= data + bitmap->total_size;
  uchar *best_data= 0;
  uint min_size;
  uint best_area_size, best_prefix_area_size, best_suffix_area_size;
  uint page, size;
  ulonglong best_prefix_bits;
  DBUG_ENTER("allocate_full_pages");
  DBUG_PRINT("enter", ("pages_needed: %lu", pages_needed));

  /* Following variables are only used if best_data is set */
  LINT_INIT(best_prefix_bits);
  LINT_INIT(best_prefix_area_size);
  LINT_INIT(best_suffix_area_size);

  min_size= pages_needed;
  if (!full_page && min_size > BLOB_SEGMENT_MIN_SIZE)
    min_size= BLOB_SEGMENT_MIN_SIZE;
  best_area_size= ~(uint) 0;

  for (; data < page_end; data+= 6)
  {
    ulonglong bits= uint6korr(data);    /* 6 bytes = 6*8/3= 16 patterns */
    uchar *data_start;
    ulonglong prefix_bits= 0;
    uint area_size, prefix_area_size, suffix_area_size;

    /* Find area with at least 16 free pages */
    if (bits)
      continue;
    data_start= data;
    /* Find size of area */
    for (data+=6 ; data < data_end ; data+= 6)
    {
      if ((bits= uint6korr(data)))
        break;
    }
    area_size= (uint) (data - data_start) / 6 * 16;
    if (area_size >= best_area_size)
      continue;
    prefix_area_size= suffix_area_size= 0;
    if (!bits)
    {
      /*
        End of page; All the rest of the bits on page are part of area
        This is needed because bitmap->used_size only covers the set bits
        in the bitmap.
      */
      area_size+= (uint) (page_end - data) / 6 * 16;
      if (area_size >= best_area_size)
        break;
      data= page_end;
    }
    else
    {
      /* Add bits at end of page */
      for (; !(bits & 7); bits >>= 3)
        suffix_area_size++;
      area_size+= suffix_area_size;
    }
    if (data_start != bitmap->map)
    {
      /* Add bits before page */
      bits= prefix_bits= uint6korr(data_start - 6);
      DBUG_ASSERT(bits != 0);
      /* 111 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 */
      if (!(bits & LL(07000000000000000)))
      {
        data_start-= 6;
        do
        {
          prefix_area_size++;
          bits<<= 3;
        } while (!(bits & LL(07000000000000000)));
        area_size+= prefix_area_size;
        /* Calculate offset to page from data_start */
        prefix_area_size= 16 - prefix_area_size;
      }
    }
    if (area_size >= min_size && area_size <= best_area_size)
    {
      best_data= data_start;
      best_area_size= area_size;
      best_prefix_bits= prefix_bits;
      best_prefix_area_size= prefix_area_size;
      best_suffix_area_size= suffix_area_size;

      /* Prefer to put data in biggest possible area */
      if (area_size <= pages_needed)
        min_size= area_size;
      else
        min_size= pages_needed;
    }
  }
  if (!best_data)
    DBUG_RETURN(0);                             /* No room on page */

  /*
    Now allocate min(pages_needed, area_size), starting from
    best_start + best_prefix_area_size
  */
  if (best_area_size > pages_needed)
    best_area_size= pages_needed;

  /* For each 6 bytes we have 6*8/3= 16 patterns */
  page= ((uint) (best_data - bitmap->map) * 8) / 3 + best_prefix_area_size;
  block->page= bitmap->page + 1 + page;
  block->page_count= best_area_size;
  block->empty_space= 0;
  block->sub_blocks= 0;
  block->org_bitmap_value= 0;
  block->used= 0;
  DBUG_ASSERT(page + best_area_size < bitmap->pages_covered);
  DBUG_PRINT("info", ("page: %lu  page_count: %u",
                      (ulong) block->page, block->page_count));

  if (best_prefix_area_size)
  {
    ulonglong tmp;
    /* Convert offset back to bits */
    best_prefix_area_size= 16 - best_prefix_area_size;
    if (best_area_size < best_prefix_area_size)
    {
      tmp= (LL(1) << best_area_size*3) - 1;
      best_area_size= best_prefix_area_size;    /* for easy end test */
    }
    else
      tmp= (LL(1) << best_prefix_area_size*3) - 1;
    tmp<<= (16 - best_prefix_area_size) * 3;
    DBUG_ASSERT((best_prefix_bits & tmp) == 0);
    best_prefix_bits|= tmp;
    int6store(best_data, best_prefix_bits);
    if (!(best_area_size-= best_prefix_area_size))
    {
      DBUG_EXECUTE("bitmap", _ma_print_bitmap_changes(bitmap););
      DBUG_RETURN(block->page_count);
    }
    best_data+= 6;
  }
  best_area_size*= 3;                       /* Bits to set */
  size= best_area_size/8;                   /* Bytes to set */
  bfill(best_data, size, 255);
  best_data+= size;
  if ((best_area_size-= size * 8))
  {
    /* fill last uchar */
    *best_data|= (uchar) ((1 << best_area_size) -1);
    best_data++;
  }
  if (data_end < best_data)
  {
    bitmap->used_size= (uint) (best_data - bitmap->map);
    DBUG_ASSERT(bitmap->used_size <= bitmap->total_size);
  }
  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap_changes(bitmap););
  DBUG_RETURN(block->page_count);
}


/****************************************************************************
  Find right bitmaps where to store data
****************************************************************************/

/*
  Find right bitmap and position for head block

  SYNOPSIS
    find_head()
    info		Maria handler
    length	        Size of data region we need store
    position		Position in bitmap_blocks where to store the
			information for the head block.

  RETURN
    0  ok
    1  error
*/

static my_bool find_head(MARIA_HA *info, uint length, uint position)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  MARIA_BITMAP_BLOCK *block;
  /*
    There is always place for the head block in bitmap_blocks as these are
    preallocated at _ma_init_block_record().
  */
  block= dynamic_element(&info->bitmap_blocks, position, MARIA_BITMAP_BLOCK *);

  /*
    We need to have DIRENTRY_SIZE here to take into account that we may
    need an extra directory entry for the row
  */
  while (allocate_head(bitmap, length + DIR_ENTRY_SIZE, block))
    if (move_to_next_bitmap(info, bitmap))
      return 1;
  return 0;
}


/*
  Find right bitmap and position for tail

  SYNOPSIS
    find_tail()
    info		Maria handler
    length	        Size of data region we need store
    position		Position in bitmap_blocks where to store the
			information for the head block.

  RETURN
    0  ok
    1  error
*/

static my_bool find_tail(MARIA_HA *info, uint length, uint position)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  MARIA_BITMAP_BLOCK *block;
  DBUG_ENTER("find_tail");
  DBUG_ASSERT(length <= info->s->block_size - PAGE_OVERHEAD_SIZE);

  /* Needed, as there is no error checking in dynamic_element */
  if (allocate_dynamic(&info->bitmap_blocks, position))
    DBUG_RETURN(1);
  block= dynamic_element(&info->bitmap_blocks, position, MARIA_BITMAP_BLOCK *);

  /*
    We have to add DIR_ENTRY_SIZE to ensure we have space for the tail and
    it's directroy entry on the page
  */
  while (allocate_tail(bitmap, length + DIR_ENTRY_SIZE, block))
    if (move_to_next_bitmap(info, bitmap))
      DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/*
  Find right bitmap and position for full blocks in one extent

  SYNOPSIS
    find_mid()
    info		Maria handler.
    pages	        How many pages to allocate.
    position		Position in bitmap_blocks where to store the
			information for the head block.
  NOTES
    This is used to allocate the main extent after the 'head' block
    (Ie, the middle part of the head-middle-tail entry)

  RETURN
    0  ok
    1  error
*/

static my_bool find_mid(MARIA_HA *info, ulong pages, uint position)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  MARIA_BITMAP_BLOCK *block;
  block= dynamic_element(&info->bitmap_blocks, position, MARIA_BITMAP_BLOCK *);

  while (!allocate_full_pages(bitmap, pages, block, 1))
  {
    if (move_to_next_bitmap(info, bitmap))
      return 1;
  }
  return 0;
}


/*
  Find right bitmap and position for putting a blob

  SYNOPSIS
    find_blob()
    info		Maria handler.
    length		Length of the blob

  NOTES
    The extents are stored last in info->bitmap_blocks

  IMPLEMENTATION
    Allocate all full pages for the block + optionally one tail

  RETURN
    0  ok
    1  error
*/

static my_bool find_blob(MARIA_HA *info, ulong length)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  uint full_page_size= FULL_PAGE_SIZE(info->s->block_size);
  ulong pages;
  uint rest_length, used;
  uint first_block_pos;
  MARIA_BITMAP_BLOCK *first_block= 0;
  DBUG_ENTER("find_blob");
  DBUG_PRINT("enter", ("length: %lu", length));
  LINT_INIT(first_block_pos);

  pages= length / full_page_size;
  rest_length= (uint) (length - pages * full_page_size);
  if (rest_length >= MAX_TAIL_SIZE(info->s->block_size))
  {
    pages++;
    rest_length= 0;
  }

  first_block_pos= info->bitmap_blocks.elements;
  if (pages)
  {
    MARIA_BITMAP_BLOCK *block;
    if (allocate_dynamic(&info->bitmap_blocks,
                         info->bitmap_blocks.elements +
                         pages / BLOB_SEGMENT_MIN_SIZE + 2))
      DBUG_RETURN(1);
    block= dynamic_element(&info->bitmap_blocks, info->bitmap_blocks.elements,
                           MARIA_BITMAP_BLOCK*);
    do
    {
      /*
        We use 0x3fff here as the two upmost bits are reserved for
        TAIL_BIT and START_EXTENT_BIT
      */
      used= allocate_full_pages(bitmap,
                                (pages >= 0x3fff ? 0x3fff : (uint) pages),
                                block, 0);
      if (!used)
      {
        if (move_to_next_bitmap(info, bitmap))
          DBUG_RETURN(1);
      }
      else
      {
        pages-= used;
        info->bitmap_blocks.elements++;
        block++;
      }
    } while (pages != 0);
  }
  if (rest_length && find_tail(info, rest_length,
                               info->bitmap_blocks.elements++))
    DBUG_RETURN(1);
  first_block= dynamic_element(&info->bitmap_blocks, first_block_pos,
                               MARIA_BITMAP_BLOCK*);
  first_block->sub_blocks= info->bitmap_blocks.elements - first_block_pos;
  DBUG_RETURN(0);
}


/*
  Find pages to put ALL blobs

  SYNOPSIS
  allocate_blobs()
  info		Maria handler
  row		Information of what is in the row (from calc_record_size())

  RETURN
   0    ok
   1    error
*/

static my_bool allocate_blobs(MARIA_HA *info, MARIA_ROW *row)
{
  ulong *length, *end;
  uint elements;
  /*
    Reserve size for:
    head block
    one extent
    tail block
  */
  elements= info->bitmap_blocks.elements;
  for (length= row->blob_lengths, end= length + info->s->base.blobs;
       length < end; length++)
  {
    if (*length && find_blob(info, *length))
      return 1;
  }
  row->extents_count= (info->bitmap_blocks.elements - elements);
  return 0;
}


/*
  Store in the bitmap the new size for a head page

  SYNOPSIS
    use_head()
    info		Maria handler
    page		Page number to update
			(Note that caller guarantees this is in the active
                        bitmap)
    size		How much free space is left on the page
    block_position	In which info->bitmap_block we have the
			information about the head block.

  NOTES
    This is used on update where we are updating an existing head page
*/

static void use_head(MARIA_HA *info, pgcache_page_no_t page, uint size,
                     uint block_position)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  MARIA_BITMAP_BLOCK *block;
  uchar *data;
  uint offset, tmp, offset_page;
  DBUG_ENTER("use_head");

  DBUG_ASSERT(page % bitmap->pages_covered);

  block= dynamic_element(&info->bitmap_blocks, block_position,
                         MARIA_BITMAP_BLOCK*);
  block->page= page;
  block->page_count= 1 + TAIL_BIT;
  block->empty_space= size;
  block->used= BLOCKUSED_TAIL;

  /*
    Mark place used by reading/writing 2 bytes at a time to handle
    bitmaps in overlapping bytes
  */
  offset_page= (uint) (page - bitmap->page - 1) * 3;
  offset= offset_page & 7;
  data= bitmap->map + offset_page / 8;
  tmp= uint2korr(data);
  block->org_bitmap_value= (tmp >> offset) & 7;
  tmp= (tmp & ~(7 << offset)) | (FULL_HEAD_PAGE << offset);
  int2store(data, tmp);
  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap_changes(bitmap););
  DBUG_VOID_RETURN;
}


/*
  Find out where to split the row (ie, what goes in head, middle, tail etc)

  SYNOPSIS
    find_where_to_split_row()
    share           Maria share
    row		    Information of what is in the row (from calc_record_size())
    extents_length  Number of bytes needed to store all extents
    split_size	    Free size on the page (The head length must be less
                    than this)

  RETURN
    row_length for the head block.
*/

static uint find_where_to_split_row(MARIA_SHARE *share, MARIA_ROW *row,
                                    uint extents_length, uint split_size)
{
  uint *lengths, *lengths_end;
  /*
    Ensure we have the minimum required space on head page:
    - Header + length of field lengths (row->min_length)
    - Number of extents
    - One extent
  */
  uint row_length= (row->min_length +
                    size_to_store_key_length(extents_length) +
                    ROW_EXTENT_SIZE);
  DBUG_ASSERT(row_length < split_size);
  /*
    Store first in all_field_lengths the different parts that are written
    to the row. This needs to be in same order as in
    ma_block_rec.c::write_block_record()
  */
  row->null_field_lengths[-3]= extents_length;
  row->null_field_lengths[-2]= share->base.fixed_not_null_fields_length;
  row->null_field_lengths[-1]= row->field_lengths_length;
  for (lengths= row->null_field_lengths - EXTRA_LENGTH_FIELDS,
       lengths_end= (lengths + share->base.pack_fields - share->base.blobs +
                     EXTRA_LENGTH_FIELDS); lengths < lengths_end; lengths++)
  {
    if (row_length + *lengths > split_size)
      break;
    row_length+= *lengths;
  }
  return row_length;
}


/*
  Find where to write the middle parts of the row and the tail

  SYNOPSIS
    write_rest_of_head()
    info	Maria handler
    position    Position in bitmap_blocks. Is 0 for rows that needs
                full blocks (ie, has a head, middle part and optional tail)
   rest_length  How much left of the head block to write.

  RETURN
    0  ok
    1  error
*/

static my_bool write_rest_of_head(MARIA_HA *info, uint position,
                                  ulong rest_length)
{
  MARIA_SHARE *share= info->s;
  uint full_page_size= FULL_PAGE_SIZE(share->block_size);
  MARIA_BITMAP_BLOCK *block;
  DBUG_ENTER("write_rest_of_head");
  DBUG_PRINT("enter", ("position: %u  rest_length: %lu", position,
                       rest_length));

  if (position == 0)
  {
    /* Write out full pages */
    uint pages= rest_length / full_page_size;

    rest_length%= full_page_size;
    if (rest_length >= MAX_TAIL_SIZE(share->block_size))
    {
      /* Put tail on a full page */
      pages++;
      rest_length= 0;
    }
    if (find_mid(info, pages, 1))
      DBUG_RETURN(1);
    /*
      Insert empty block after full pages, to allow write_block_record() to
      split segment into used + free page
    */
    block= dynamic_element(&info->bitmap_blocks, 2, MARIA_BITMAP_BLOCK*);
    block->page_count= 0;
    block->used= 0;
  }
  if (rest_length)
  {
    if (find_tail(info, rest_length, ELEMENTS_RESERVED_FOR_MAIN_PART - 1))
      DBUG_RETURN(1);
  }
  else
  {
    /* Empty tail block */
    block= dynamic_element(&info->bitmap_blocks,
                           ELEMENTS_RESERVED_FOR_MAIN_PART - 1,
                           MARIA_BITMAP_BLOCK *);
    block->page_count= 0;
    block->used= 0;
  }
  DBUG_RETURN(0);
}


/*
  Find where to store one row

  SYNPOSIS
    _ma_bitmap_find_place()
    info                  Maria handler
    row                   Information about row to write
    blocks                Store data about allocated places here

  RETURN
    0  ok
       row->space_on_head_page contains minimum number of bytes we
       expect to put on the head page.
    1  error
       my_errno is set to error
*/

my_bool _ma_bitmap_find_place(MARIA_HA *info, MARIA_ROW *row,
                              MARIA_BITMAP_BLOCKS *blocks)
{
  MARIA_SHARE *share= info->s;
  my_bool res= 1;
  uint full_page_size, position, max_page_size;
  uint head_length, row_length, rest_length, extents_length;
  DBUG_ENTER("_ma_bitmap_find_place");

  blocks->count= 0;
  blocks->tail_page_skipped= blocks->page_skipped= 0;
  row->extents_count= 0;

  /*
    Reserve place for the following blocks:
     - Head block
     - Full page block
     - Marker block to allow write_block_record() to split full page blocks
       into full and free part
     - Tail block
  */

  info->bitmap_blocks.elements= ELEMENTS_RESERVED_FOR_MAIN_PART;
  max_page_size= (share->block_size - PAGE_OVERHEAD_SIZE);

  pthread_mutex_lock(&share->bitmap.bitmap_lock);

  if (row->total_length <= max_page_size)
  {
    /* Row fits in one page */
    position= ELEMENTS_RESERVED_FOR_MAIN_PART - 1;
    if (find_head(info, (uint) row->total_length, position))
      goto abort;
    row->space_on_head_page= row->total_length;
    goto end;
  }

  /*
    First allocate all blobs so that we can find out the needed size for
    the main block.
  */
  if (row->blob_length && allocate_blobs(info, row))
    goto abort;

  extents_length= row->extents_count * ROW_EXTENT_SIZE;
  /*
    The + 3 is reserved for storing the number of segments in the row header.
  */
  if ((head_length= (row->head_length + extents_length + 3)) <=
      max_page_size)
  {
    /* Main row part fits into one page */
    position= ELEMENTS_RESERVED_FOR_MAIN_PART - 1;
    if (find_head(info, head_length, position))
      goto abort;
    row->space_on_head_page= head_length;
    goto end;
  }

  /* Allocate enough space */
  head_length+= ELEMENTS_RESERVED_FOR_MAIN_PART * ROW_EXTENT_SIZE;

  /* The first segment size is stored in 'row_length' */
  row_length= find_where_to_split_row(share, row, extents_length,
                                      max_page_size);

  full_page_size= MAX_TAIL_SIZE(share->block_size);
  position= 0;
  if (head_length - row_length <= full_page_size)
    position= ELEMENTS_RESERVED_FOR_MAIN_PART -2;    /* Only head and tail */
  if (find_head(info, row_length, position))
    goto abort;
  row->space_on_head_page= row_length;

  rest_length= head_length - row_length;
  if (write_rest_of_head(info, position, rest_length))
    goto abort;

end:
  blocks->block= dynamic_element(&info->bitmap_blocks, position,
                                 MARIA_BITMAP_BLOCK*);
  blocks->block->sub_blocks= ELEMENTS_RESERVED_FOR_MAIN_PART - position;
  /* First block's page_count is for all blocks */
  blocks->count= info->bitmap_blocks.elements - position;
  res= 0;

abort:
  pthread_mutex_unlock(&share->bitmap.bitmap_lock);
  DBUG_RETURN(res);
}


/*
  Find where to put row on update (when head page is already defined)

  SYNPOSIS
    _ma_bitmap_find_new_place()
    info                  Maria handler
    row                   Information about row to write
    page                  On which page original row was stored
    free_size             Free size on head page
    blocks                Store data about allocated places here

  NOTES
   This function is only called when the new row can't fit in the space of
   the old row in the head page.

   This is essently same as _ma_bitmap_find_place() except that
   we don't call find_head() to search in bitmaps where to put the page.

  RETURN
    0  ok
    1  error
*/

my_bool _ma_bitmap_find_new_place(MARIA_HA *info, MARIA_ROW *row,
                                  pgcache_page_no_t page, uint free_size,
                                  MARIA_BITMAP_BLOCKS *blocks)
{
  MARIA_SHARE *share= info->s;
  my_bool res= 1;
  uint position;
  uint head_length, row_length, rest_length, extents_length;
  ulonglong bitmap_page;
  DBUG_ENTER("_ma_bitmap_find_new_place");

  blocks->count= 0;
  blocks->tail_page_skipped= blocks->page_skipped= 0;
  row->extents_count= 0;
  info->bitmap_blocks.elements= ELEMENTS_RESERVED_FOR_MAIN_PART;

  pthread_mutex_lock(&share->bitmap.bitmap_lock);

  /*
    First allocate all blobs (so that we can find out the needed size for
    the main block.
  */
  if (row->blob_length && allocate_blobs(info, row))
    goto abort;

  /* Switch bitmap to current head page */
  bitmap_page= page / share->bitmap.pages_covered;
  bitmap_page*= share->bitmap.pages_covered;

  if (share->bitmap.page != bitmap_page &&
      _ma_change_bitmap_page(info, &share->bitmap, bitmap_page))
    goto abort;

  extents_length= row->extents_count * ROW_EXTENT_SIZE;
  if ((head_length= (row->head_length + extents_length + 3)) <= free_size)
  {
    /* Main row part fits into one page */
    position= ELEMENTS_RESERVED_FOR_MAIN_PART - 1;
    use_head(info, page, head_length, position);
    row->space_on_head_page= head_length;
    goto end;
  }

  /* Allocate enough space */
  head_length+= ELEMENTS_RESERVED_FOR_MAIN_PART * ROW_EXTENT_SIZE;

  /* The first segment size is stored in 'row_length' */
  row_length= find_where_to_split_row(share, row, extents_length, free_size);

  position= 0;
  if (head_length - row_length < MAX_TAIL_SIZE(share->block_size))
    position= ELEMENTS_RESERVED_FOR_MAIN_PART -2;    /* Only head and tail */
  use_head(info, page, row_length, position);
  row->space_on_head_page= row_length;

  rest_length= head_length - row_length;
  if (write_rest_of_head(info, position, rest_length))
    goto abort;

end:
  blocks->block= dynamic_element(&info->bitmap_blocks, position,
                                 MARIA_BITMAP_BLOCK*);
  blocks->block->sub_blocks= ELEMENTS_RESERVED_FOR_MAIN_PART - position;
  /* First block's page_count is for all blocks */
  blocks->count= info->bitmap_blocks.elements - position;
  res= 0;

abort:
  pthread_mutex_unlock(&share->bitmap.bitmap_lock);
  DBUG_RETURN(res);
}


/****************************************************************************
  Clear and reset bits
****************************************************************************/

/*
  Set fill pattern for a page

  set_page_bits()
  info		Maria handler
  bitmap	Bitmap handler
  page		Adress to page
  fill_pattern  Pattern (not size) for page

  NOTES
    Page may not be part of active bitmap

  RETURN
    0  ok
    1  error
*/

static my_bool set_page_bits(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap,
                             pgcache_page_no_t page, uint fill_pattern)
{
  pgcache_page_no_t bitmap_page;
  uint offset_page, offset, tmp, org_tmp;
  uchar *data;
  DBUG_ENTER("set_page_bits");
  DBUG_ASSERT(fill_pattern <= 7);

  bitmap_page= page - page % bitmap->pages_covered;
  if (bitmap_page != bitmap->page &&
      _ma_change_bitmap_page(info, bitmap, bitmap_page))
    DBUG_RETURN(1);

  /* Find page number from start of bitmap */
  offset_page= (uint) (page - bitmap->page - 1);
  /*
    Mark place used by reading/writing 2 bytes at a time to handle
    bitmaps in overlapping bytes
  */
  offset_page*= 3;
  offset= offset_page & 7;
  data= bitmap->map + offset_page / 8;
  org_tmp= tmp= uint2korr(data);
  tmp= (tmp & ~(7 << offset)) | (fill_pattern << offset);
  if (tmp == org_tmp)
    DBUG_RETURN(0);                             /* No changes */
  int2store(data, tmp);

  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap_changes(bitmap););
  if (fill_pattern != 3 && fill_pattern != 7)
    set_if_smaller(info->s->state.first_bitmap_with_space, bitmap_page);
  /*
    Note that if the condition above is false (page is full), and all pages of
    this bitmap are now full, and that bitmap page was
    first_bitmap_with_space, we don't modify first_bitmap_with_space, indeed
    its value still tells us where to start our search for a bitmap with space
    (which is for sure after this full one).
    That does mean that first_bitmap_with_space is only a lower bound.
  */
  DBUG_RETURN(0);
}


/*
  Get bitmap pattern for a given page

  SYNOPSIS
    get_page_bits()
    info	Maria handler
    bitmap	Bitmap handler
    page	Page number

  RETURN
    0-7		Bitmap pattern
    ~0		Error (couldn't read page)
*/

uint _ma_bitmap_get_page_bits(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap,
                              pgcache_page_no_t page)
{
  pgcache_page_no_t bitmap_page;
  uint offset_page, offset, tmp;
  uchar *data;
  DBUG_ENTER("_ma_bitmap_get_page_bits");

  bitmap_page= page - page % bitmap->pages_covered;
  if (bitmap_page != bitmap->page &&
      _ma_change_bitmap_page(info, bitmap, bitmap_page))
    DBUG_RETURN(~ (uint) 0);

  /* Find page number from start of bitmap */
  offset_page= (uint) (page - bitmap->page - 1);
  /*
    Mark place used by reading/writing 2 bytes at a time to handle
    bitmaps in overlapping bytes
  */
  offset_page*= 3;
  offset= offset_page & 7;
  data= bitmap->map + offset_page / 8;
  tmp= uint2korr(data);
  DBUG_RETURN((tmp >> offset) & 7);
}


/*
  Mark all pages in a region as free

  SYNOPSIS
    _ma_bitmap_reset_full_page_bits()
    info                Maria handler
    bitmap              Bitmap handler
    page                Start page
    page_count          Number of pages

  NOTES
    We assume that all pages in region is covered by same bitmap
    One must have a lock on info->s->bitmap.bitmap_lock

  RETURN
    0  ok
    1  Error (when reading bitmap)
*/

my_bool _ma_bitmap_reset_full_page_bits(MARIA_HA *info,
                                        MARIA_FILE_BITMAP *bitmap,
                                        pgcache_page_no_t page,
                                        uint page_count)
{
  ulonglong bitmap_page;
  uint offset, bit_start, bit_count, tmp;
  uchar *data;
  DBUG_ENTER("_ma_bitmap_reset_full_page_bits");
  DBUG_PRINT("enter", ("page: %lu  page_count: %u", (ulong) page, page_count));
  safe_mutex_assert_owner(&info->s->bitmap.bitmap_lock);

  bitmap_page= page - page % bitmap->pages_covered;
  DBUG_ASSERT(page != bitmap_page);

  if (bitmap_page != bitmap->page &&
      _ma_change_bitmap_page(info, bitmap, bitmap_page))
    DBUG_RETURN(1);

  /* Find page number from start of bitmap */
  offset= (uint) (page - bitmap->page - 1);

  /* Clear bits from 'page * 3' -> '(page + page_count) * 3' */
  bit_start= offset * 3;
  bit_count= page_count * 3;

  data= bitmap->map + bit_start / 8;
  offset= bit_start & 7;

  tmp= (255 << offset);                         /* Bits to keep */
  if (bit_count + offset < 8)
  {
    /* Only clear bits between 'offset' and 'offset+bit_count-1' */
    tmp^= (255 << (offset + bit_count));
  }
  *data&= ~tmp;

  if ((int) (bit_count-= (8 - offset)) > 0)
  {
    uint fill;
    data++;
    /*
      -1 is here to avoid one 'if' statement and to let the following code
      handle the last byte
    */
    if ((fill= (bit_count - 1) / 8))
    {
      bzero(data, fill);
      data+= fill;
    }
    bit_count-= fill * 8;                       /* Bits left to clear */
    tmp= (1 << bit_count) - 1;
    *data&= ~tmp;
  }
  set_if_smaller(info->s->state.first_bitmap_with_space, bitmap_page);
  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap_changes(bitmap););
  DBUG_RETURN(0);
}

/*
  Set all pages in a region as used

  SYNOPSIS
    _ma_bitmap_set_full_page_bits()
    info                Maria handler
    bitmap              Bitmap handler
    page                Start page
    page_count          Number of pages

  NOTES
    We assume that all pages in region is covered by same bitmap
    One must have a lock on info->s->bitmap.bitmap_lock

  RETURN
    0  ok
    1  Error (when reading bitmap)
*/

my_bool _ma_bitmap_set_full_page_bits(MARIA_HA *info,
                                      MARIA_FILE_BITMAP *bitmap,
                                      pgcache_page_no_t page, uint page_count)
{
  ulonglong bitmap_page;
  uint offset, bit_start, bit_count, tmp;
  uchar *data;
  DBUG_ENTER("_ma_bitmap_set_full_page_bits");
  DBUG_PRINT("enter", ("page: %lu  page_count: %u", (ulong) page, page_count));
  safe_mutex_assert_owner(&info->s->bitmap.bitmap_lock);

  bitmap_page= page - page % bitmap->pages_covered;
  if (page == bitmap_page ||
      page + page_count >= bitmap_page + bitmap->pages_covered)
  {
    DBUG_ASSERT(0);                             /* Wrong in data */
    DBUG_RETURN(1);
  }

  if (bitmap_page != bitmap->page &&
      _ma_change_bitmap_page(info, bitmap, bitmap_page))
    DBUG_RETURN(1);

  /* Find page number from start of bitmap */
  offset= (uint) (page - bitmap->page - 1);

  /* Set bits from 'page * 3' -> '(page + page_count) * 3' */
  bit_start= offset * 3;
  bit_count= page_count * 3;

  data= bitmap->map + bit_start / 8;
  offset= bit_start & 7;

  tmp= (255 << offset);                         /* Bits to keep */
  if (bit_count + offset < 8)
  {
    /* Only set bits between 'offset' and 'offset+bit_count-1' */
    tmp^= (255 << (offset + bit_count));
  }
  *data|= tmp;

  if ((int) (bit_count-= (8 - offset)) > 0)
  {
    uint fill;
    data++;
    /*
      -1 is here to avoid one 'if' statement and to let the following code
      handle the last byte
    */
    if ((fill= (bit_count - 1) / 8))
    {
      bfill(data, fill, 255);
      data+= fill;
    }
    bit_count-= fill * 8;                       /* Bits left to set */
    tmp= (1 << bit_count) - 1;
    *data|= tmp;
  }
  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap_changes(bitmap););
  DBUG_RETURN(0);
}


/**
   @brief
   Make a transition of MARIA_FILE_BITMAP::non_flushable.
   If the bitmap becomes flushable, which requires that REDO-UNDO has been
   logged and all bitmap pages touched by the thread have a correct
   allocation, it unpins all bitmap pages, and if _ma_bitmap_flush_all() is
   waiting (in practice it is a checkpoint), it wakes it up.
   If the bitmap becomes or stays unflushable, the function merely records it
   unless a concurrent _ma_bitmap_flush_all() is happening, in which case the
   function first waits for the flush to be done.

   @note
   this sets info->non_flushable_state to 1 if we have incremented
   bitmap->non_flushable and not yet decremented it.

   @param  share               Table's share
   @param  non_flushable_inc   Increment of MARIA_FILE_BITMAP::non_flushable
                               (-1 or +1).
*/

void _ma_bitmap_flushable(MARIA_HA *info, int non_flushable_inc)
{
  MARIA_SHARE *share= info->s;
  MARIA_FILE_BITMAP *bitmap;
  DBUG_ENTER("_ma_bitmap_flushable");

  /*
    Not transactional tables are never automaticly flushed and needs no
    protection
  */
  if (!share->now_transactional)
    DBUG_VOID_RETURN;

  bitmap= &share->bitmap;
  pthread_mutex_lock(&bitmap->bitmap_lock);

  if (non_flushable_inc == -1)
  {
    DBUG_ASSERT((int) bitmap->non_flushable > 0);
    DBUG_ASSERT(info->non_flushable_state == 1);
    if (--bitmap->non_flushable == 0)
    {
      /*
        We unlock and unpin pages locked and pinned by other threads. It does
        not seem to be an issue as all bitmap changes are serialized with
        the bitmap's mutex.
      */
      _ma_bitmap_unpin_all(share);
      if (unlikely(bitmap->flush_all_requested))
      {
        DBUG_PRINT("info", ("bitmap flushable waking up flusher"));
        pthread_cond_broadcast(&bitmap->bitmap_cond);
      }
    }
    DBUG_PRINT("info", ("bitmap->non_flushable: %u", bitmap->non_flushable));
    pthread_mutex_unlock(&bitmap->bitmap_lock);
    info->non_flushable_state= 0;
    DBUG_VOID_RETURN;
  }
  DBUG_ASSERT(non_flushable_inc == 1);
  DBUG_ASSERT(info->non_flushable_state == 0);
  while (unlikely(bitmap->flush_all_requested))
  {
    /*
      Some other thread is waiting for the bitmap to become
      flushable. Not the moment to make the bitmap unflushable or more
      unflushable; let's rather back off and wait. If we didn't do this, with
      multiple writers, there may always be one thread causing the bitmap to
      be unflushable and _ma_bitmap_flush_all() would wait for long.
      There should not be a deadlock because if our thread increased
      non_flushable (and thus _ma_bitmap_flush_all() is waiting for at least
      our thread), it is not going to increase it more so is not going to come
      here.
    */
    DBUG_PRINT("info", ("waiting for bitmap flusher"));
    pthread_cond_wait(&bitmap->bitmap_cond, &bitmap->bitmap_lock);
  }
  bitmap->non_flushable++;
  DBUG_PRINT("info", ("bitmap->non_flushable: %u", bitmap->non_flushable));
  pthread_mutex_unlock(&bitmap->bitmap_lock);
  info->non_flushable_state= 1;
  DBUG_VOID_RETURN;
}


/*
  Correct bitmap pages to reflect the true allocation

  SYNOPSIS
    _ma_bitmap_release_unused()
    info                Maria handle
    blocks              Bitmap blocks

  IMPLEMENTATION
    If block->used & BLOCKUSED_TAIL is set:
       If block->used & BLOCKUSED_USED is set, then the bits for the
       corresponding page is set according to block->empty_space
       If block->used & BLOCKUSED_USED is not set, then the bits for
       the corresponding page is set to org_bitmap_value;

    If block->used & BLOCKUSED_TAIL is not set:
       if block->used is not set, the bits for the corresponding page are
       cleared

  For the first block (head block) the logic is same as for a tail block

  Note that we may have 'filler blocks' that are used to split a block
  in half; These can be recognized by that they have page_count == 0.

  This code also reverse the effect of ma_bitmap_flushable(.., 1);

  RETURN
    0  ok
    1  error (Couldn't write or read bitmap page)
*/

my_bool _ma_bitmap_release_unused(MARIA_HA *info, MARIA_BITMAP_BLOCKS *blocks)
{
  MARIA_BITMAP_BLOCK *block= blocks->block, *end= block + blocks->count;
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  uint bits, current_bitmap_value;
  DBUG_ENTER("_ma_bitmap_release_unused");

  /*
    We can skip FULL_HEAD_PAGE (4) as the page was marked as 'full'
    when we allocated space in the page
  */
  current_bitmap_value= FULL_HEAD_PAGE;

  pthread_mutex_lock(&bitmap->bitmap_lock);

  /* First handle head block */
  if (block->used & BLOCKUSED_USED)
  {
    DBUG_PRINT("info", ("head page: %lu  empty_space: %u",
                        (ulong) block->page, block->empty_space));
    bits= _ma_free_size_to_head_pattern(bitmap, block->empty_space);
    if (block->used & BLOCKUSED_USE_ORG_BITMAP)
      current_bitmap_value= block->org_bitmap_value;
  }
  else
    bits= block->org_bitmap_value;
  if (bits != current_bitmap_value)
  {
    if (set_page_bits(info, bitmap, block->page, bits))
      goto err;
  }
  else
  {
    DBUG_ASSERT(current_bitmap_value ==
                _ma_bitmap_get_page_bits(info, bitmap, block->page));
  }

  /* Handle all full pages and tail pages (for head page and blob) */
  for (block++; block < end; block++)
  {
    uint page_count;
    if (!block->page_count)
      continue;                               /* Skip 'filler blocks' */

    page_count= block->page_count;
    if (block->used & BLOCKUSED_TAIL)
    {
      current_bitmap_value= FULL_TAIL_PAGE;
      /* The bitmap page is only one page */
      page_count= 1;
      if (block->used & BLOCKUSED_USED)
      {
        DBUG_PRINT("info", ("tail page: %lu  empty_space: %u",
                            (ulong) block->page, block->empty_space));
        bits= free_size_to_tail_pattern(bitmap, block->empty_space);
        if (block->used & BLOCKUSED_USE_ORG_BITMAP)
          current_bitmap_value= block->org_bitmap_value;
      }
      else
        bits= block->org_bitmap_value;

      /*
        The page has all bits set; The following test is an optimization
        to not set the bits to the same value as before.
      */
      if (bits != current_bitmap_value)
      {
        if (set_page_bits(info, bitmap, block->page, bits))
          goto err;
      }
      else
      {
        DBUG_ASSERT(current_bitmap_value ==
                    _ma_bitmap_get_page_bits(info, bitmap, block->page));
      }
    }
    else if (!(block->used & BLOCKUSED_USED) &&
             _ma_bitmap_reset_full_page_bits(info, bitmap,
                                             block->page, page_count))
      goto err;
  }

  /* This duplicates ma_bitmap_flushable(-1) except it already has mutex */
  if (info->non_flushable_state)
  {
    DBUG_ASSERT(((int) (bitmap->non_flushable)) > 0);
    info->non_flushable_state= 0;
    if (--bitmap->non_flushable == 0)
    {
      _ma_bitmap_unpin_all(info->s);
      if (unlikely(bitmap->flush_all_requested))
      {
        DBUG_PRINT("info", ("bitmap flushable waking up flusher"));
        pthread_cond_broadcast(&bitmap->bitmap_cond);
      }
    }
  }
  DBUG_PRINT("info", ("bitmap->non_flushable: %u", bitmap->non_flushable));

  pthread_mutex_unlock(&bitmap->bitmap_lock);
  DBUG_RETURN(0);

err:
  pthread_mutex_unlock(&bitmap->bitmap_lock);
  DBUG_RETURN(1);
}


/*
  Free full pages from bitmap and pagecache

  SYNOPSIS
    _ma_bitmap_free_full_pages()
    info                Maria handle
    extents             Extents (as stored on disk)
    count               Number of extents

  IMPLEMENTATION
    Mark all full pages (not tails) from extents as free, both in bitmap
    and page cache.

  RETURN
    0  ok
    1  error (Couldn't write or read bitmap page)
*/

my_bool _ma_bitmap_free_full_pages(MARIA_HA *info, const uchar *extents,
                                   uint count)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  DBUG_ENTER("_ma_bitmap_free_full_pages");

  pthread_mutex_lock(&bitmap->bitmap_lock);
  for (; count--; extents+= ROW_EXTENT_SIZE)
  {
    pgcache_page_no_t page=  uint5korr(extents);
    uint page_count= (uint2korr(extents + ROW_EXTENT_PAGE_SIZE) &
                      ~START_EXTENT_BIT);
    if (!(page_count & TAIL_BIT))
    {
      if (page == 0 && page_count == 0)
        continue;                               /* Not used extent */
      if (pagecache_delete_pages(info->s->pagecache, &info->dfile, page,
                                 page_count, PAGECACHE_LOCK_WRITE, 1) ||
          _ma_bitmap_reset_full_page_bits(info, bitmap, page, page_count))
      {
        pthread_mutex_unlock(&bitmap->bitmap_lock);
        DBUG_RETURN(1);
      }
    }
  }
  pthread_mutex_unlock(&bitmap->bitmap_lock);
  DBUG_RETURN(0);
}


/*
  Mark in the bitmap how much free space there is on a page

  SYNOPSIS
   _ma_bitmap_set()
   info		Maria handler
   page		Adress to page
   head		1 if page is a head page, 0 if tail page
   empty_space	How much empty space there is on page

  RETURN
    0  ok
    1  error
*/

my_bool _ma_bitmap_set(MARIA_HA *info, pgcache_page_no_t page, my_bool head,
                       uint empty_space)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  uint bits;
  my_bool res;
  DBUG_ENTER("_ma_bitmap_set");
  DBUG_PRINT("enter", ("page: %lu  head: %d  empty_space: %u",
                       (ulong) page, head, empty_space));

  pthread_mutex_lock(&info->s->bitmap.bitmap_lock);
  bits= (head ?
         _ma_free_size_to_head_pattern(bitmap, empty_space) :
         free_size_to_tail_pattern(bitmap, empty_space));
  res= set_page_bits(info, bitmap, page, bits);
  pthread_mutex_unlock(&info->s->bitmap.bitmap_lock);
  DBUG_RETURN(res);
}


/*
  Check that bitmap pattern is correct for a page

  NOTES
    Used in maria_chk

  SYNOPSIS
    _ma_check_bitmap_data()
    info	    Maria handler
    page_type	    What kind of page this is
    page	    Adress to page
    empty_space     Empty space on page
    bitmap_pattern  Store here the pattern that was in the bitmap for the
		    page. This is always updated.

  RETURN
    0  ok
    1  error
*/

my_bool _ma_check_bitmap_data(MARIA_HA *info,
                              enum en_page_type page_type, pgcache_page_no_t page,
                              uint empty_space, uint *bitmap_pattern)
{
  uint bits;
  switch (page_type) {
  case UNALLOCATED_PAGE:
  case MAX_PAGE_TYPE:
    bits= 0;
    break;
  case HEAD_PAGE:
    bits= _ma_free_size_to_head_pattern(&info->s->bitmap, empty_space);
    break;
  case TAIL_PAGE:
    bits= free_size_to_tail_pattern(&info->s->bitmap, empty_space);
    break;
  case BLOB_PAGE:
    bits= FULL_TAIL_PAGE;
    break;
  default:
    bits= 0; /* to satisfy compiler */
    DBUG_ASSERT(0);
  }
  return ((*bitmap_pattern= _ma_bitmap_get_page_bits(info, &info->s->bitmap,
                                                     page)) != bits);
}


/*
  Check if the page type matches the one that we have in the bitmap

  SYNOPSIS
    _ma_check_if_right_bitmap_type()
    info	    Maria handler
    page_type	    What kind of page this is
    page	    Adress to page
    bitmap_pattern  Store here the pattern that was in the bitmap for the
		    page. This is always updated.

  NOTES
    Used in maria_chk

  RETURN
    0  ok
    1  error
*/

my_bool _ma_check_if_right_bitmap_type(MARIA_HA *info,
                                       enum en_page_type page_type,
                                       pgcache_page_no_t page,
                                       uint *bitmap_pattern)
{
  if ((*bitmap_pattern= _ma_bitmap_get_page_bits(info, &info->s->bitmap,
                                                 page)) > 7)
    return 1;                                   /* Couldn't read page */
  switch (page_type) {
  case HEAD_PAGE:
    return *bitmap_pattern < 1 || *bitmap_pattern > 4;
  case TAIL_PAGE:
    return *bitmap_pattern < 5;
  case BLOB_PAGE:
    return *bitmap_pattern != 7;
  default:
    break;
  }
  DBUG_ASSERT(0);
  return 1;
}


/**
   @brief create the first bitmap page of a freshly created data file

   @param  share           table's share

   @return Operation status
     @retval 0      OK
     @retval !=0    Error
*/

int _ma_bitmap_create_first(MARIA_SHARE *share)
{
  uint block_size= share->bitmap.block_size;
  File file= share->bitmap.file.file;
  uchar marker[CRC_SIZE];

  /*
    Next write operation of the page will write correct CRC
    if it is needed
  */
  int4store(marker, MARIA_NO_CRC_BITMAP_PAGE);

  if (my_chsize(file, block_size - sizeof(marker),
                0, MYF(MY_WME)) ||
      my_pwrite(file, marker, sizeof(marker),
                block_size - sizeof(marker),
                MYF(MY_NABP | MY_WME)))
    return 1;
  share->state.state.data_file_length= block_size;
  _ma_bitmap_delete_all(share);
  return 0;
}


/**
  @brief Pagecache callback to get the TRANSLOG_ADDRESS to flush up to, when a
  bitmap page needs to be flushed.

  @param page            Page's content
  @param page_no         Page's number (<offset>/<page length>)
  @param data_ptr        Callback data pointer (pointer to MARIA_SHARE)

  @retval TRANSLOG_ADDRESS to flush up to.
*/

static my_bool
flush_log_for_bitmap(uchar *page __attribute__((unused)),
                     pgcache_page_no_t page_no __attribute__((unused)),
                     uchar *data_ptr __attribute__((unused)))
{
#ifndef DBUG_OFF
  const MARIA_SHARE *share= (MARIA_SHARE*)data_ptr;
#endif
  DBUG_ENTER("flush_log_for_bitmap");
  DBUG_ASSERT(share->now_transactional);
  /*
    WAL imposes that UNDOs reach disk before bitmap is flushed. We don't know
    the LSN of the last UNDO about this bitmap page, so we flush whole log.
  */
  DBUG_RETURN(translog_flush(translog_get_horizon()));
}


/**
   @brief Set callbacks for bitmap pages

   @note
   We don't use pagecache_file_init here, as we want to keep the
   code readable
*/

void _ma_bitmap_set_pagecache_callbacks(PAGECACHE_FILE *file,
                                        MARIA_SHARE *share)
{
  file->callback_data= (uchar*) share;
  file->flush_log_callback= maria_flush_log_for_page_none;
  file->write_fail= maria_page_write_failure;

  if (share->temporary)
  {
    file->read_callback=  &maria_page_crc_check_none;
    file->write_callback= &maria_page_filler_set_none;
  }
  else
  {
    file->read_callback=  &maria_page_crc_check_bitmap;
    if (share->options & HA_OPTION_PAGE_CHECKSUM)
      file->write_callback= &maria_page_crc_set_normal;
    else
      file->write_callback= &maria_page_filler_set_bitmap;
    if (share->now_transactional)
      file->flush_log_callback= flush_log_for_bitmap;
  }
}


/**
  Extends data file with zeroes and creates new bitmap pages into page cache.

  Writes all bitmap pages in [from, to].

  Non-bitmap pages of zeroes are correct as they are marked empty in
  bitmaps. Bitmap pages will not be zeroes: they will get their CRC fixed when
  flushed. And if there is a crash before flush (so they are zeroes at
  restart), a REDO will re-create them in page cache.
*/

static my_bool
_ma_bitmap_create_missing_into_pagecache(MARIA_SHARE *share,
                                         MARIA_FILE_BITMAP *bitmap,
                                         pgcache_page_no_t from,
                                         pgcache_page_no_t to,
                                         uchar *zeroes)
{
  pgcache_page_no_t i;
  /*
    We do not use my_chsize() because there can be a race between when it
    reads the physical size and when it writes (assume data_file_length is 10,
    physical length is 8 and two data pages are in cache, and here we do a
    my_chsize: my_chsize sees physical length is 8, then the two data pages go
    to disk then my_chsize writes from page 8 and so overwrites the two data
    pages, wrongly).
    We instead rely on the filesystem filling gaps with zeroes.
  */
  for (i= from; i <= to; i+= bitmap->pages_covered)
  {
    /**
      No need to keep them pinned, they are new so flushable.
      @todo but we may want to keep them pinned, as an optimization: if they
      are not pinned they may go to disk before the data pages go (so, the
      physical pages would be in non-ascending "sparse" order on disk), or the
      filesystem may fill gaps with zeroes physically which is a waste of
      time.
    */
    if (pagecache_write(share->pagecache,
                        &bitmap->file, i, 0,
                        zeroes, PAGECACHE_PLAIN_PAGE,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                        PAGECACHE_PIN_LEFT_UNPINNED,
                        PAGECACHE_WRITE_DELAY, 0, LSN_IMPOSSIBLE))
      goto err;
  }
  /*
    Data pages after data_file_length are full of zeroes but that is allowed
    as they are marked empty in the bitmap.
  */
  return FALSE;
err:
  return TRUE;
}


/**
 Creates missing bitmaps when we extend the data file.

 At run-time, when we need a new bitmap page we come here; and only one bitmap
 page at a time is created.

 In some recovery cases we insert at a large offset in the data file, way
 beyond state.data_file_length, so can need to create more than one bitmap
 page in one go. Known case is:
 Start a transaction in Maria;
 delete last row of very large table (with delete_row)
 do a bulk insert
 crash
 Then UNDO_BULK_INSERT will truncate table files, and
 UNDO_ROW_DELETE will want to put the row back to its original position,
 extending the data file a lot: bitmap page*s* in the hole must be created,
 or he table would look corrupted.

 We need to log REDOs for bitmap creation, consider: we apply a REDO for a
 data page, which creates the first data page covered by a new bitmap
 not yet created. If the data page is flushed but the bitmap page is not and
 there is a crash, re-execution of the REDO will complain about the zeroed
 bitmap page (see it as corruption). Thus a REDO is needed to re-create the
 bitmap.

 @param  info              Maria handler
 @param  bitmap            Bitmap handler
 @param  page              Last bitmap page to create

 @note When this function is called this must be true:
 ((page + 1) * bitmap->block_size > info->s->state.state.data_file_length)

*/

static my_bool _ma_bitmap_create_missing(MARIA_HA *info,
                                         MARIA_FILE_BITMAP *bitmap,
                                         pgcache_page_no_t page)
{
  MARIA_SHARE *share= info->s;
  uint block_size= bitmap->block_size;
  pgcache_page_no_t from, to;
  my_off_t data_file_length= share->state.state.data_file_length;
  DBUG_ENTER("_ma_bitmap_create_missing");

  /* First (in offset order) bitmap page to create */
  if (data_file_length < block_size)
    goto err; /* corrupted, should have first bitmap page */

  from= (data_file_length / block_size - 1) / bitmap->pages_covered + 1;
  from*= bitmap->pages_covered;
  /*
    page>=from because:
    (page + 1) * bs > dfl, and page == k * pc so:
    (k * pc + 1) * bs > dfl; k * pc + 1 > dfl / bs; k * pc > dfl / bs - 1
    k > (dfl / bs - 1) / pc; k >= (dfl / bs - 1) / pc + 1
    k * pc >= ((dfl / bs - 1) / pc + 1) * pc == from.
  */
  DBUG_ASSERT(page >= from);

  if (share->now_transactional)
  {
    LSN lsn;
    uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE * 2];
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    page_store(log_data + FILEID_STORE_SIZE, from);
    page_store(log_data + FILEID_STORE_SIZE + PAGE_STORE_SIZE, page);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
    /*
      We don't use info->trn so that this REDO is always executed even though
      the UNDO does not reach disk due to crash. This is also consistent with
      the fact that the new bitmap pages are not pinned.
    */
    if (translog_write_record(&lsn, LOGREC_REDO_BITMAP_NEW_PAGE,
                              &dummy_transaction_object, info,
                              (translog_size_t)sizeof(log_data),
                              TRANSLOG_INTERNAL_PARTS + 1, log_array,
                              log_data, NULL))
      goto err;
    /*
      No need to flush the log: the bitmap pages we are going to create will
      flush it when they go to disk.
    */
  }

  /*
    Last bitmap page. It has special creation: will go to the page cache
    only later as we are going to modify it very soon.
  */
  bzero(bitmap->map, bitmap->block_size);
  bitmap->used_size= 0;
#ifndef DBUG_OFF
  memcpy(bitmap->map + bitmap->block_size, bitmap->map, bitmap->block_size);
#endif

  /* Last bitmap page to create before 'page' */
  DBUG_ASSERT(page >= bitmap->pages_covered);
  to= page - bitmap->pages_covered;
  /*
    In run-time situations, from>=to is always false, i.e. we always create
    one bitmap at a time ('page').
  */
  if ((from <= to) &&
      _ma_bitmap_create_missing_into_pagecache(share, bitmap, from, to,
                                               bitmap->map))
    goto err;

  share->state.state.data_file_length= (page + 1) * bitmap->block_size;

 DBUG_RETURN(FALSE);
err:
 DBUG_RETURN(TRUE);
}


my_bool _ma_apply_redo_bitmap_new_page(MARIA_HA *info,
                                       LSN lsn __attribute__ ((unused)),
                                       const uchar *header)
{
  MARIA_SHARE *share= info->s;
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  my_bool error;
  pgcache_page_no_t from, to, min_from;
  DBUG_ENTER("_ma_apply_redo_bitmap_new_page");

  from= page_korr(header);
  to=   page_korr(header + PAGE_STORE_SIZE);
  DBUG_PRINT("info", ("from: %lu to: %lu", (ulong)from, (ulong)to));
  if ((from > to) ||
      (from % bitmap->pages_covered) != 0 ||
      (to % bitmap->pages_covered) != 0)
  {
    error= TRUE; /* corrupted log record */
    goto err;
  }

  min_from= (share->state.state.data_file_length / bitmap->block_size - 1) /
    bitmap->pages_covered + 1;
  min_from*= bitmap->pages_covered;
  if (from < min_from)
  {
    DBUG_PRINT("info", ("overwrite bitmap pages from %lu", (ulong)min_from));
    /*
      We have to overwrite. It could be that there was a bitmap page in
      memory, covering a data page which went to disk, then crash: the
      bitmap page is now full of zeros and is ==min_from, we have to overwrite
      it with correct checksum.
    */
  }
  share->state.changed|= STATE_CHANGED;
  bzero(info->buff, bitmap->block_size);
  if (!(error=
        _ma_bitmap_create_missing_into_pagecache(share, bitmap, from, to,
                                                 info->buff)))
    share->state.state.data_file_length= (to + 1) * bitmap->block_size;

err:
  DBUG_RETURN(error);
}
