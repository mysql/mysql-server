/* Copyright (C) 2007-2008 MySQL AB

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

#include "maria_def.h"


/**
  @brief calculate crc of the page avoiding special values

  @param start           The value to start CRC (we use page number here)
  @param data            data pointer
  @param length          length of the data

  @return crc of the page without special values
*/

static uint32 maria_page_crc(uint32 start, uchar *data, uint length)
{
  uint32 crc= crc32(start, data, length);

  /* we need this assert to get following comparison working */
  compile_time_assert(MARIA_NO_CRC_BITMAP_PAGE ==
                      MARIA_NO_CRC_NORMAL_PAGE - 1 &&
                      MARIA_NO_CRC_NORMAL_PAGE == 0xffffffff);
  if (crc >= MARIA_NO_CRC_BITMAP_PAGE)
    crc= MARIA_NO_CRC_BITMAP_PAGE - 1;

  return(crc);
}

/**
  @brief Maria pages read callback (checks the page CRC)

  @param page            The page data to check
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        pointer to MARIA_SHARE
  @param no_crc_val      Value which means CRC absence
                         (MARIA_NO_CRC_NORMAL_PAGE or MARIA_NO_CRC_BITMAP_PAGE)
  @param data_length     length of data to calculate CRC

  @retval 0 OK
  @retval 1 Error
*/

static my_bool maria_page_crc_check(uchar *page,
                                    pgcache_page_no_t page_no,
                                    MARIA_SHARE *share,
                                    uint32 no_crc_val,
                                    int data_length)
{
  uint32 crc= uint4korr(page + share->block_size - CRC_SIZE), new_crc;
  my_bool res;
  DBUG_ENTER("maria_page_crc_check");

  DBUG_ASSERT((uint)data_length <= share->block_size - CRC_SIZE);

  /* we need this assert to get following comparison working */
  compile_time_assert(MARIA_NO_CRC_BITMAP_PAGE ==
                      MARIA_NO_CRC_NORMAL_PAGE - 1 &&
                      MARIA_NO_CRC_NORMAL_PAGE == 0xffffffff);
  /*
    If crc is no_crc_val then
    the page has no crc, so there is nothing to check.
  */
  if (crc >= MARIA_NO_CRC_BITMAP_PAGE)
  {
    DBUG_PRINT("info", ("No crc: %lu  crc: %lu  page: %lu  ",
                        (ulong) no_crc_val, (ulong) crc, (ulong) page_no));
    if (crc != no_crc_val)
    {
      my_errno= HA_ERR_WRONG_CRC;
      DBUG_PRINT("error", ("Wrong no CRC value"));
      DBUG_RETURN(1);
    }
    DBUG_RETURN(0);
  }
  new_crc= maria_page_crc((uint32) page_no, page, data_length);
  DBUG_ASSERT(new_crc != no_crc_val);
  res= test(new_crc != crc);
  if (res)
  {
    /*
      Bitmap pages may be totally zero filled in some cases.
      This happens when we get a crash after the pagecache has written
      out a page that is on a newly created bitmap page and we get
      a crash before the bitmap page is written out.

      We handle this case with the following logic:
      When reading, approve of bitmap pages where all bytes are zero
      (This is after all a bitmap pages where no data is reserved and
      the CRC will be corrected at next write)
    */
    if (no_crc_val == MARIA_NO_CRC_BITMAP_PAGE &&
        crc == 0 && _ma_check_if_zero(page, data_length))
    {
      DBUG_PRINT("warning", ("Found bitmap page that was not initialized"));
      DBUG_RETURN(0);
    }

    DBUG_PRINT("error", ("Page: %lu  crc: %lu  calculated crc: %lu",
                         (ulong) page_no, (ulong) crc, (ulong) new_crc));
    my_errno= HA_ERR_WRONG_CRC;
  }
  DBUG_RETURN(res);
}


/**
  @brief Maria pages write callback (sets the page CRC for data and index
  files)

  @param page            The page data to set
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Write callback data pointer (pointer to MARIA_SHARE)

  @retval 0 OK
*/

my_bool maria_page_crc_set_normal(uchar *page,
                                  pgcache_page_no_t page_no,
                                  uchar *data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  int data_length= share->block_size - CRC_SIZE;
  uint32 crc= maria_page_crc((uint32) page_no, page, data_length);
  DBUG_ENTER("maria_page_crc_set_normal");
  DBUG_PRINT("info", ("Page %lu  crc: %lu", (ulong) page_no, (ulong)crc));

  /* crc is on the stack so it is aligned, pagecache buffer is aligned, too */
  int4store_aligned(page + data_length, crc);
  DBUG_RETURN(0);
}


/**
  @brief Maria pages write callback (sets the page CRC for keys)

  @param page            The page data to set
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Write callback data pointer (pointer to MARIA_SHARE)

  @retval 0 OK
*/

my_bool maria_page_crc_set_index(uchar *page,
                                 pgcache_page_no_t page_no,
                                 uchar *data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  int data_length= _ma_get_page_used(share, page);
  uint32 crc= maria_page_crc((uint32) page_no, page, data_length);
  DBUG_ENTER("maria_page_crc_set_index");
  DBUG_PRINT("info", ("Page %lu  crc: %lu",
                      (ulong) page_no, (ulong) crc));
  DBUG_ASSERT((uint)data_length <= share->block_size - CRC_SIZE);
  /* crc is on the stack so it is aligned, pagecache buffer is aligned, too */
  int4store_aligned(page + share->block_size - CRC_SIZE, crc);
  DBUG_RETURN(0);
}


/* interface functions */


/**
  @brief Maria pages read callback (checks the page CRC) for index/data pages

  @param page            The page data to check
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Read callback data pointer (pointer to MARIA_SHARE)

  @retval 0 OK
  @retval 1 Error
*/

my_bool maria_page_crc_check_data(uchar *page,
                                  pgcache_page_no_t page_no,
                                  uchar *data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  return (maria_page_crc_check(page, (uint32) page_no, share,
                               MARIA_NO_CRC_NORMAL_PAGE,
                               share->block_size - CRC_SIZE));
}


/**
  @brief Maria pages read callback (checks the page CRC) for bitmap pages

  @param page            The page data to check
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Read callback data pointer (pointer to MARIA_SHARE)

  @retval 0 OK
  @retval 1 Error
*/

my_bool maria_page_crc_check_bitmap(uchar *page,
                                    pgcache_page_no_t page_no,
                                    uchar *data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  return (maria_page_crc_check(page, (uint32) page_no, share,
                               MARIA_NO_CRC_BITMAP_PAGE,
                               share->block_size - CRC_SIZE));
}


/**
  @brief Maria pages read callback (checks the page CRC) for index pages

  @param page            The page data to check
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Read callback data pointer (pointer to MARIA_SHARE)

  @retval 0 OK
  @retval 1 Error
*/

my_bool maria_page_crc_check_index(uchar *page,
                                   pgcache_page_no_t page_no,
                                   uchar *data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  uint length= _ma_get_page_used(share, page);
  if (length > share->block_size - CRC_SIZE)
  {
    DBUG_PRINT("error", ("Wrong page length: %u", length));
    return (my_errno= HA_ERR_WRONG_CRC);
  }
  return maria_page_crc_check(page, (uint32) page_no, share,
                               MARIA_NO_CRC_NORMAL_PAGE,
                              length);
}


/**
  @brief Maria pages dumme read callback for temporary tables

  @retval 0 OK
  @retval 1 Error
*/

my_bool maria_page_crc_check_none(uchar *page __attribute__((unused)),
                                  pgcache_page_no_t page_no
                                  __attribute__((unused)),
                                  uchar *data_ptr __attribute__((unused)))
{
  return 0;
}


/**
  @brief Maria pages write callback (sets the page filler for index/data)

  @param page            The page data to set
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Write callback data pointer (pointer to MARIA_SHARE)

  @retval 0 OK
*/

my_bool maria_page_filler_set_normal(uchar *page,
                                     pgcache_page_no_t page_no
                                     __attribute__((unused)),
                                     uchar *data_ptr)
{
  DBUG_ENTER("maria_page_filler_set_normal");
  DBUG_ASSERT(page_no != 0);                    /* Catches some simple bugs */
  int4store_aligned(page + ((MARIA_SHARE *)data_ptr)->block_size - CRC_SIZE,
                    MARIA_NO_CRC_NORMAL_PAGE);
  DBUG_RETURN(0);
}


/**
  @brief Maria pages write callback (sets the page filler for bitmap)

  @param page            The page data to set
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Write callback data pointer (pointer to MARIA_SHARE)

  @retval 0 OK
*/

my_bool maria_page_filler_set_bitmap(uchar *page,
                                     pgcache_page_no_t page_no
                                     __attribute__((unused)),
                                     uchar *data_ptr)
{
  DBUG_ENTER("maria_page_filler_set_bitmap");
  int4store_aligned(page + ((MARIA_SHARE *)data_ptr)->block_size - CRC_SIZE,
                    MARIA_NO_CRC_BITMAP_PAGE);
  DBUG_RETURN(0);
}


/**
  @brief Maria pages dummy write callback for temporary tables

  @retval 0 OK
*/

my_bool maria_page_filler_set_none(uchar *page __attribute__((unused)),
                                   pgcache_page_no_t page_no
                                   __attribute__((unused)),
                                   uchar *data_ptr __attribute__((unused)))
{
#ifdef HAVE_valgrind
  int4store_aligned(page + ((MARIA_SHARE *)data_ptr)->block_size - CRC_SIZE,
                    0);
#endif
  return 0;
}


/**
  @brief Write failure callback (mark table as corrupted)

  @param data_ptr        Write callback data pointer (pointer to MARIA_SHARE)
*/

void maria_page_write_failure(uchar* data_ptr)
{
  maria_mark_crashed_share((MARIA_SHARE *)data_ptr);
}


/**
  @brief Maria flush log log if needed

  @param page            The page data to set
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Write callback data pointer (pointer to MARIA_SHARE)

  @retval 0  OK
  @retval 1  error
*/

my_bool maria_flush_log_for_page(uchar *page,
                                 pgcache_page_no_t page_no
                                 __attribute__((unused)),
                                 uchar *data_ptr __attribute__((unused)))
{
  LSN lsn;
  MARIA_SHARE *share= (MARIA_SHARE*) data_ptr;
  DBUG_ENTER("maria_flush_log_for_page");
  /* share is 0 here only in unittest */
  DBUG_ASSERT(!share || share->page_type == PAGECACHE_LSN_PAGE);
  lsn= lsn_korr(page);
  if (translog_flush(lsn))
    DBUG_RETURN(1);
  /*
    Now when log is written, it's safe to incremented 'open' counter for
    the table so that we know it was not closed properly.
  */
  if (share && !share->global_changed)
    _ma_mark_file_changed_now(share);
  DBUG_RETURN(0);
}


my_bool maria_flush_log_for_page_none(uchar *page __attribute__((unused)),
                                      pgcache_page_no_t page_no
                                      __attribute__((unused)),
                                      uchar *data_ptr __attribute__((unused)))
{
  return 0;
}
