/* Copyright (C) 2007 Michael Widenius

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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
  1      50-75 % full  (at least room for 2 records)
  2      75-100 % full (at least room for one record)
  3      100 % full    (no more room for records)

  Assuming 8K pages, this will allow us to map:
  8192 (bytes per page) * 4 (pages mapped per byte) * 8192 (page size)= 256M

  (For Maria this will be 7*4 * 8192 = 224K smaller because of LSN)

  Note that for fixed size rows, we can't add more columns without doing
  a full reorganization of the table. The user can always force a dynamic
  size row format by specifying ROW_FORMAT=dynamic.


  Dynamic size records:

  3 bits are used to indicate

  0      Empty page
  1      0-30 % full  (at least room for 3 records)
  2      30-60 % full (at least room for 2 records)
  3      60-90 % full (at least room for one record)
  4      100 % full   (no more room for records)
  5      Tail page,  0-40 % full
  6      Tail page,  40-80 % full
  7      Full tail page or full blob page

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

*/

#include "maria_def.h"
#include "ma_blockrec.h"

/* Number of pages to store blob parts */
#define BLOB_SEGMENT_MIN_SIZE 128

#define FULL_HEAD_PAGE 4
#define FULL_TAIL_PAGE 7

static inline my_bool write_changed_bitmap(MARIA_SHARE *share,
                                           MARIA_FILE_BITMAP *bitmap)
{
  return (key_cache_write(share->key_cache,
                          bitmap->file, bitmap->page * bitmap->block_size, 0,
                          (byte*) bitmap->map,
                          bitmap->block_size, bitmap->block_size, 1));
}

/*
  Initialize bitmap. This is called the first time a file is opened
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

  if (!(bitmap->map= (uchar*) my_malloc(size, MYF(MY_WME))))
    return 1;

  bitmap->file= file;
  bitmap->changed= 0;
  bitmap->block_size= share->block_size;
  /* Size needs to be alligned on 6 */
  aligned_bit_blocks= share->block_size / 6;
  bitmap->total_size= aligned_bit_blocks * 6;
  /*
    In each 6 bytes, we have 6*8/3 = 16 pages covered
    The +1 is to add the bitmap page, as this doesn't have to be covered
  */
  bitmap->pages_covered= aligned_bit_blocks * 16 + 1;

  /* Update size for bits */
  /* TODO; Make this dependent of the row size */
  max_page_size= share->block_size - PAGE_OVERHEAD_SIZE;
  bitmap->sizes[0]= max_page_size;              /* Empty page */
  bitmap->sizes[1]= max_page_size - max_page_size * 30 / 100;
  bitmap->sizes[2]= max_page_size - max_page_size * 60 / 100;
  bitmap->sizes[3]= max_page_size - max_page_size * 90 / 100;
  bitmap->sizes[4]= 0;                          /* Full page */
  bitmap->sizes[5]= max_page_size - max_page_size * 40 / 100;
  bitmap->sizes[6]= max_page_size - max_page_size * 80 / 100;
  bitmap->sizes[7]= 0;

  pthread_mutex_init(&share->bitmap.bitmap_lock, MY_MUTEX_INIT_SLOW);

  /*
    Start by reading first page (assume table scan)
    Later code is simpler if it can assume we always have an active bitmap.
 */
  if (_ma_read_bitmap_page(share, bitmap, (ulonglong) 0))
    return(1);
  return 0;
}


/*
  Free data allocated by _ma_bitmap_init
*/

my_bool _ma_bitmap_end(MARIA_SHARE *share)
{
  my_bool res= 0;
  _ma_flush_bitmap(share);
  pthread_mutex_destroy(&share->bitmap.bitmap_lock);
  my_free((byte*) share->bitmap.map, MYF(MY_ALLOW_ZERO_PTR));
  return res;
}


/*
  Flush bitmap to disk
*/

my_bool _ma_flush_bitmap(MARIA_SHARE *share)
{
  my_bool res= 0;
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
  return res;
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
  Return bitmap pattern for block where there is size bytes free
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
    pattern_to_head_size
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
*/

#ifndef DBUG_OFF

const char *bits_to_txt[]=
{
  "empty", "00-30% full", "30-60% full", "60-90% full", "full",
  "tail 00-40 % full", "tail 40-80 % full", "tail/blob full"
};

static void _ma_print_bitmap(MARIA_FILE_BITMAP *bitmap)
{
  uchar *pos, *end, *org_pos;
  ulong page;

  end= bitmap->map+ bitmap->used_size;
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE,"\nBitmap page changes at page %lu\n",
          (ulong) bitmap->page);

  page= (ulong) bitmap->page+1;
  for (pos= bitmap->map, org_pos= bitmap->map+bitmap->block_size ; pos < end ;
       pos+= 6, org_pos+= 6)
  {
    ulonglong bits= uint6korr(pos);    /* 6 bytes = 6*8/3= 16 patterns */
    ulonglong org_bits= uint6korr(org_pos);
    uint i;
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
  memcpy(bitmap->map+ bitmap->block_size, bitmap->map, bitmap->block_size);
}

#endif /* DBUG_OFF */


/***************************************************************************
  Reading & writing bitmap pages
***************************************************************************/

/*
  Read a given bitmap page

  SYNOPSIS
    read_bitmap_page()
    info                Maria handler
    bitmap              Bitmap handler
    page                Page to read

  TODO
    Update 'bitmap->used_size' to real size of used bitmap

  RETURN
    0  ok
    1  error  (Error writing old bitmap or reading bitmap page)
*/

my_bool _ma_read_bitmap_page(MARIA_SHARE *share, MARIA_FILE_BITMAP *bitmap,
                             ulonglong page)
{
  my_off_t position= page * bitmap->block_size;
  my_bool res;
  DBUG_ENTER("_ma_read_bitmap_page");
  DBUG_ASSERT(page % bitmap->pages_covered == 0);

  bitmap->page= page;
  if (position >= share->state.state.data_file_length)
  {
    share->state.state.data_file_length= position + bitmap->block_size;
    bzero(bitmap->map, bitmap->block_size);
    bitmap->used_size= 0;
    DBUG_RETURN(0);
  }
  bitmap->used_size= bitmap->total_size;
  res= key_cache_read(share->key_cache,
                      bitmap->file, position, 0,
                      (byte*) bitmap->map,
                      bitmap->block_size, bitmap->block_size, 0) == 0;
#ifndef DBUG_OFF
  if (!res)
    memcpy(bitmap->map+ bitmap->block_size, bitmap->map, bitmap->block_size);
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
                                      ulonglong page)
{
  DBUG_ENTER("_ma_change_bitmap_page");
  DBUG_ASSERT(page % bitmap->pages_covered == 0);

  if (bitmap->changed)
  {
    if (write_changed_bitmap(info->s, bitmap))
      DBUG_RETURN(1);
    bitmap->changed= 0;
  }
  DBUG_RETURN(_ma_read_bitmap_page(info->s, bitmap, page));
}


/*
  Read next suitable bitmap

  SYNOPSIS
    move_to_next_bitmap()
    bitmap              Bitmap handle

  TODO
    Add cache of bitmaps to not read something that is not usable

  RETURN
    0  ok
    1  error (either couldn't save old bitmap or read new one
*/

static my_bool move_to_next_bitmap(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap)
{
  ulonglong page= bitmap->page;
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
    best_data		Pointer to best 6 byte aligned area in bitmap->map
    best_pos		Which bit in *best_data the area starts
                        0 = first bit pattern, 1 second bit pattern etc
    fill_pattern	Bitmap pattern to store in best_data[best_pos]
*/

static void fill_block(MARIA_FILE_BITMAP *bitmap,
                       MARIA_BITMAP_BLOCK *block,
                       uchar *best_data, uint best_pos, uint best_bits,
                       uint fill_pattern)
{
  uint page, offset, tmp;
  uchar *data;

  /* For each 6 bytes we have 6*8/3= 16 patterns */
  page= (best_data - bitmap->map) / 6 * 16 + best_pos;
  block->page= bitmap->page + 1 + page;
  block->page_count= 1 + TAIL_BIT;
  block->empty_space= pattern_to_size(bitmap, best_bits);
  block->sub_blocks= 1;
  block->org_bitmap_value= best_bits;
  block->used= BLOCKUSED_TAIL;

  /*
    Mark place used by reading/writing 2 bytes at a time to handle
    bitmaps in overlapping bytes
  */
  best_pos*= 3;
  data= best_data+ best_pos / 8;
  offset= best_pos & 7;
  tmp= uint2korr(data);
  tmp= (tmp & ~(7 << offset)) | (fill_pattern << offset);
  int2store(data, tmp);
  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap(bitmap););
}


/*
  Allocate data for head block

  SYNOPSIS
   allocate_head()
   bitmap       bitmap
   size         Size of block we need to find
   block        Store found information here

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

  for (; data < end; data += 6)
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
      uint pattern= bits & 7;
      if (pattern <= min_bits)
      {
        if (pattern == min_bits)
        {
          /* Found perfect match */
          best_bits= min_bits;
          best_data= data;
          best_pos= i;
          goto found;
        }
        if ((int) pattern > (int) best_bits)
        {
          best_bits= pattern;
          best_data= data;
          best_pos= i;
        }
      }
    }
  }
  if (!best_data)
  {
    if (bitmap->used_size == bitmap->total_size)
      DBUG_RETURN(1);
    /* Allocate data at end of bitmap */
    bitmap->used_size+= 6;
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
  DBUG_ASSERT(size <= FULL_PAGE_SIZE(bitmap->block_size));

  for (; data < end; data += 6)
  {
    ulonglong bits= uint6korr(data);    /* 6 bytes = 6*8/3= 16 patterns */
    uint i;

    /*
      Skip common patterns
      We can skip empty pages (if we already found a match) or
      the following patterns: 1-4 or 7
    */

    if ((!bits && best_data) || bits == LL(0xffffffffffff))
      continue;
    for (i= 0; i < 16; i++, bits >>= 3)
    {
      uint pattern= bits & 7;
      if (pattern <= min_bits && (!pattern || pattern >= 5))
      {
        if (pattern == min_bits)
        {
          best_bits= min_bits;
          best_data= data;
          best_pos= i;
          goto found;
        }
        if ((int) pattern > (int) best_bits)
        {
          best_bits= pattern;
          best_data= data;
          best_pos= i;
        }
      }
    }
  }
  if (!best_data)
  {
    if (bitmap->used_size == bitmap->total_size)
      DBUG_RETURN(1);
    /* Allocate data at end of bitmap */
    bitmap->used_size+= 6;
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
    area_size= (data - data_start) / 6 * 16;
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
      area_size+= (page_end - data) / 6 * 16;
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
  page= ((best_data - bitmap->map) * 8) / 3 + best_prefix_area_size;
  block->page= bitmap->page + 1 + page;
  block->page_count= best_area_size;
  block->empty_space= 0;
  block->sub_blocks= 1;
  block->org_bitmap_value= 0;
  block->used= 0;
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
      DBUG_EXECUTE("bitmap", _ma_print_bitmap(bitmap););
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
    /* fill last byte */
    *best_data|= (uchar) ((1 << best_area_size) -1);
    best_data++;
  }
  if (data_end < best_data)
    bitmap->used_size= (uint) (best_data - bitmap->map);
  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap(bitmap););
  DBUG_RETURN(block->page_count);
}


/****************************************************************************
  Find right bitmaps where to store data
****************************************************************************/

/*
  Find right bitmap and position for head block

  RETURN
    0  ok
    1  error
*/

static my_bool find_head(MARIA_HA *info, uint length, uint position)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  MARIA_BITMAP_BLOCK *block;
  /* There is always place for head blocks in bitmap_blocks */
  block= dynamic_element(&info->bitmap_blocks, position, MARIA_BITMAP_BLOCK *);

  while (allocate_head(bitmap, length, block))
    if (move_to_next_bitmap(info, bitmap))
      return 1;
  return 0;
}


/*
  Find right bitmap and position for tail

  RETURN
    0  ok
    1  error
*/

static my_bool find_tail(MARIA_HA *info, uint length, uint position)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  MARIA_BITMAP_BLOCK *block;
  DBUG_ENTER("find_tail");

  /* Needed, as there is no error checking in dynamic_element */
  if (allocate_dynamic(&info->bitmap_blocks, position))
    DBUG_RETURN(1);
  block= dynamic_element(&info->bitmap_blocks, position, MARIA_BITMAP_BLOCK *);

  while (allocate_tail(bitmap, length, block))
    if (move_to_next_bitmap(info, bitmap))
      DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/*
  Find right bitmap and position for full blocks in one extent

  NOTES
    This is used to allocate the main extent after the 'head' block

  RETURN
    0  ok
    1  error
*/

static my_bool find_mid(MARIA_HA *info, ulong pages, uint position)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  MARIA_BITMAP_BLOCK *block;
  block= dynamic_element(&info->bitmap_blocks, position, MARIA_BITMAP_BLOCK *);

  while (allocate_full_pages(bitmap, pages, block, 1))
  {
    if (move_to_next_bitmap(info, bitmap))
      return 1;
  }
  return 0;
}


/*
  Find right bitmap and position for putting a blob

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

  pages= length / full_page_size;
  rest_length= (uint) (length - pages * full_page_size);
  if (rest_length >= MAX_TAIL_SIZE(info->s->block_size))
  {
    pages++;
    rest_length= 0;
  }

  if (pages)
  {
    MARIA_BITMAP_BLOCK *block;
    if (allocate_dynamic(&info->bitmap_blocks,
                         info->bitmap_blocks.elements +
                         pages / BLOB_SEGMENT_MIN_SIZE + 2))
      DBUG_RETURN(1);
    first_block_pos= info->bitmap_blocks.elements;
    block= dynamic_element(&info->bitmap_blocks, info->bitmap_blocks.elements,
                           MARIA_BITMAP_BLOCK*);
    first_block= block;
    do
    {
      used= allocate_full_pages(bitmap,
                                (pages >= 65535 ? 65535 : (uint) pages), block,
                                0);
      if (!used && move_to_next_bitmap(info, bitmap))
        DBUG_RETURN(1);
      info->bitmap_blocks.elements++;
      block++;
    } while ((pages-= used) != 0);
  }
  if (rest_length && find_tail(info, rest_length,
                               info->bitmap_blocks.elements++))
    DBUG_RETURN(1);
  if (first_block)
    first_block->sub_blocks= info->bitmap_blocks.elements - first_block_pos;
  DBUG_RETURN(0);
}


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


static void use_head(MARIA_HA *info, ulonglong page, uint size,
                     uint block_position)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  MARIA_BITMAP_BLOCK *block;
  uchar *data;
  uint offset, tmp, offset_page;

  block= dynamic_element(&info->bitmap_blocks, block_position,
                         MARIA_BITMAP_BLOCK*);
  block->page= page;
  block->page_count= 1 + TAIL_BIT;
  block->empty_space= size;
  block->sub_blocks= 1;
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
  DBUG_EXECUTE("bitmap", _ma_print_bitmap(bitmap););
}


/*
  Find out where to split the row;
*/

static uint find_where_to_split_row(MARIA_SHARE *share, MARIA_ROW *row,
                                    uint extents_length, uint split_size)
{
  uint row_length= row->base_length;
  uint *lengths, *lengths_end;

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


static my_bool write_rest_of_head(MARIA_HA *info, uint position,
                                  ulong rest_length)
{
  MARIA_SHARE *share= info->s;
  uint full_page_size= FULL_PAGE_SIZE(share->block_size);
  MARIA_BITMAP_BLOCK *block;

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
    if (find_mid(info, rest_length / full_page_size, 1))
      return 1;
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
      return 1;
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
  return 0;
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
    1  error
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
    Reserver place for the following blocks:
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
    goto end;
  }

  /*
    First allocate all blobs (so that we can find out the needed size for
    the main block.
  */
  if (row->blob_length && allocate_blobs(info, row))
    goto abort;

  extents_length= row->extents_count * ROW_EXTENT_SIZE;
  if ((head_length= (row->head_length + extents_length)) <= max_page_size)
  {
    /* Main row part fits into one page */
    position= ELEMENTS_RESERVED_FOR_MAIN_PART - 1;
    if (find_head(info, head_length, position))
      goto abort;
    goto end;
  }

  /* Allocate enough space */
  head_length+= ELEMENTS_RESERVED_FOR_MAIN_PART * ROW_EXTENT_SIZE;

  /* The first segment size is stored in 'row_length' */
  row_length= find_where_to_split_row(share, row, extents_length,
                                      max_page_size);

  full_page_size= FULL_PAGE_SIZE(share->block_size);
  position= 0;
  if (head_length - row_length <= full_page_size)
    position= ELEMENTS_RESERVED_FOR_MAIN_PART -2;    /* Only head and tail */
  if (find_head(info, row_length, position))
    goto abort;
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
                                  ulonglong page, uint free_size,
                                  MARIA_BITMAP_BLOCKS *blocks)
{
  MARIA_SHARE *share= info->s;
  my_bool res= 1;
  uint full_page_size, position;
  uint head_length, row_length, rest_length, extents_length;
  DBUG_ENTER("_ma_bitmap_find_new_place");

  blocks->count= 0;
  blocks->tail_page_skipped= blocks->page_skipped= 0;
  row->extents_count= 0;
  info->bitmap_blocks.elements= ELEMENTS_RESERVED_FOR_MAIN_PART;

  pthread_mutex_lock(&share->bitmap.bitmap_lock);
  if (share->bitmap.page != page / share->bitmap.pages_covered &&
      _ma_change_bitmap_page(info, &share->bitmap,
                             page / share->bitmap.pages_covered))
    goto abort;

  /*
    First allocate all blobs (so that we can find out the needed size for
    the main block.
  */
  if (row->blob_length && allocate_blobs(info, row))
    goto abort;

  extents_length= row->extents_count * ROW_EXTENT_SIZE;
  if ((head_length= (row->head_length + extents_length)) <= free_size)
  {
    /* Main row part fits into one page */
    position= ELEMENTS_RESERVED_FOR_MAIN_PART - 1;
    use_head(info, page, head_length, position);
    goto end;
  }

  /* Allocate enough space */
  head_length+= ELEMENTS_RESERVED_FOR_MAIN_PART * ROW_EXTENT_SIZE;

  /* The first segment size is stored in 'row_length' */
  row_length= find_where_to_split_row(share, row, extents_length, free_size);

  full_page_size= FULL_PAGE_SIZE(share->block_size);
  position= 0;
  if (head_length - row_length <= full_page_size)
    position= ELEMENTS_RESERVED_FOR_MAIN_PART -2;    /* Only head and tail */
  use_head(info, page, row_length, position);
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

static my_bool set_page_bits(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap,
                             ulonglong page, uint fill_pattern)
{
  ulonglong bitmap_page;
  uint offset_page, offset, tmp, org_tmp;
  uchar *data;
  DBUG_ENTER("set_page_bits");

  bitmap_page= page / bitmap->pages_covered;
  if (bitmap_page != bitmap->page &&
      _ma_change_bitmap_page(info, bitmap, bitmap_page))
    DBUG_RETURN(1);

  /* Find page number from start of bitmap */
  offset_page= page - bitmap->page - 1;
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
  DBUG_EXECUTE("bitmap", _ma_print_bitmap(bitmap););
  if (fill_pattern != 3 && fill_pattern != 7 &&
      page < info->s->state.first_bitmap_with_space)
    info->s->state.first_bitmap_with_space= page;
  DBUG_RETURN(0);
}


/*
  Get bitmap pattern for a given page

  SYNOPSIS

  get_page_bits()
  info		Maria handler
  bitmap	Bitmap handler
  page		Page number

  RETURN
    0-7		Bitmap pattern
    ~0		Error (couldn't read page)
*/

static uint get_page_bits(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap,
                          ulonglong page)
{
  ulonglong bitmap_page;
  uint offset_page, offset, tmp;
  uchar *data;
  DBUG_ENTER("get_page_bits");

  bitmap_page= page / bitmap->pages_covered;
  if (bitmap_page != bitmap->page &&
      _ma_change_bitmap_page(info, bitmap, bitmap_page))
    DBUG_RETURN(~ (uint) 0);

  /* Find page number from start of bitmap */
  offset_page= page - bitmap->page - 1;
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
    reset_full_page_bits()
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

my_bool _ma_reset_full_page_bits(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap,
                                 ulonglong page, uint page_count)
{
  ulonglong bitmap_page;
  uint offset, bit_start, bit_count, tmp;
  uchar *data;
  DBUG_ENTER("_ma_reset_full_page_bits");
  DBUG_PRINT("enter", ("page: %lu  page_count: %u", (ulong) page, page_count));
  safe_mutex_assert_owner(&info->s->bitmap.bitmap_lock);
  
  bitmap_page= page / bitmap->pages_covered;
  if (bitmap_page != bitmap->page &&
      _ma_change_bitmap_page(info, bitmap, bitmap_page))
    DBUG_RETURN(1);

  /* Find page number from start of bitmap */
  page= page - bitmap->page - 1;

  /* Clear bits from 'page * 3' -> '(page + page_count) * 3' */
  bit_start= page * 3;
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
  if (bitmap->page < (ulonglong) info->s->state.first_bitmap_with_space)
    info->s->state.first_bitmap_with_space= bitmap->page;
  bitmap->changed= 1;
  DBUG_EXECUTE("bitmap", _ma_print_bitmap(bitmap););
  DBUG_RETURN(0);
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

  pthread_mutex_lock(&info->s->bitmap.bitmap_lock);

  /* First handle head block */
  if (block->used & BLOCKUSED_USED)
  {
    DBUG_PRINT("info", ("head empty_space: %u", block->empty_space));
    bits= _ma_free_size_to_head_pattern(bitmap, block->empty_space);
    if (block->used & BLOCKUSED_USE_ORG_BITMAP)
      current_bitmap_value= block->org_bitmap_value;
  }
  else
    bits= block->org_bitmap_value;
  if (bits != current_bitmap_value &&
      set_page_bits(info, bitmap, block->page, bits))
    goto err;

  
  /* Handle all full pages and tail pages (for head page and blob) */
  for (block++; block < end; block++)
  {
    if (block->used & BLOCKUSED_TAIL)
    {
      if (block->used & BLOCKUSED_USED)
      {
        DBUG_PRINT("info", ("tail empty_space: %u", block->empty_space));
        bits= free_size_to_tail_pattern(bitmap, block->empty_space);
      }
      else
        bits= block->org_bitmap_value;
      if (bits != FULL_TAIL_PAGE &&
          set_page_bits(info, bitmap, block->page, bits))
        goto err;
    }
    if (!(block->used & BLOCKUSED_USED) &&
        _ma_reset_full_page_bits(info, bitmap,
                                 block->page, block->page_count))
      goto err;
  }
  pthread_mutex_unlock(&info->s->bitmap.bitmap_lock);
  DBUG_RETURN(0);

err:
  pthread_mutex_unlock(&info->s->bitmap.bitmap_lock);
  DBUG_RETURN(1);
}


/*
  Free full pages from bitmap

  SYNOPSIS
    _ma_bitmap_free_full_pages()
    info                Maria handle
    extents             Extents (as stored on disk)
    count               Number of extents

  IMPLEMENTATION
    Mark all full pages (not tails) from extents as free

  RETURN
    0  ok
    1  error (Couldn't write or read bitmap page)
*/

my_bool _ma_bitmap_free_full_pages(MARIA_HA *info, const byte *extents,
                                   uint count)
{
  DBUG_ENTER("_ma_bitmap_free_full_pages");

  pthread_mutex_lock(&info->s->bitmap.bitmap_lock);
  for (; count--; extents += ROW_EXTENT_SIZE)
  {
    ulonglong page=  uint5korr(extents);
    uint page_count= uint2korr(extents + ROW_EXTENT_PAGE_SIZE);
    if (!(page_count & TAIL_BIT))
    {
      if (_ma_reset_full_page_bits(info, &info->s->bitmap, page, page_count))
      {
        pthread_mutex_unlock(&info->s->bitmap.bitmap_lock);
        DBUG_RETURN(1);
      }
    }
  }
  pthread_mutex_unlock(&info->s->bitmap.bitmap_lock);
  DBUG_RETURN(0);
}



my_bool _ma_bitmap_set(MARIA_HA *info, ulonglong pos, my_bool head,
                       uint empty_space)
{
  MARIA_FILE_BITMAP *bitmap= &info->s->bitmap;
  uint bits;
  my_bool res;
  DBUG_ENTER("_ma_bitmap_set");

  pthread_mutex_lock(&info->s->bitmap.bitmap_lock);
  bits= (head ?
         _ma_free_size_to_head_pattern(bitmap, empty_space) :
         free_size_to_tail_pattern(bitmap, empty_space));
  res= set_page_bits(info, bitmap, pos, bits);
  pthread_mutex_unlock(&info->s->bitmap.bitmap_lock);
  DBUG_RETURN(res);
}


/*
  Check that bitmap pattern is correct for a page

  NOTES
    Used in maria_chk

  RETURN
    0  ok
    1  error
*/

my_bool _ma_check_bitmap_data(MARIA_HA *info,
                              enum en_page_type page_type, ulonglong page,
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
  }
  return (*bitmap_pattern= get_page_bits(info, &info->s->bitmap, page)) !=
    bits;
}


/*
  Check that bitmap pattern is correct for a page

  NOTES
    Used in maria_chk

  RETURN
    0  ok
    1  error
*/

my_bool _ma_check_if_right_bitmap_type(MARIA_HA *info,
                                       enum en_page_type page_type,
                                       ulonglong page,
                                       uint *bitmap_pattern)
{
  if ((*bitmap_pattern= get_page_bits(info, &info->s->bitmap, page)) > 7)
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
