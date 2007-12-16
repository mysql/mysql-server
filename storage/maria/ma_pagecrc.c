/* TODO: copyright & Co */

#include "maria_def.h"


/**
  @brief calculate crc of the page avoiding special values

  @param start           The value to start CRC (we use page number here)
  @param data            data pointer
  @param length          length of the data

  @return crc of the page without special values
*/

static uint32 maria_page_crc(ulong start, uchar *data, uint length)
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

static inline my_bool maria_page_crc_check(uchar *page,
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
    DBUG_PRINT("info", ("No crc: (0x%lx)  crc: (0x%lx) page: %lu  ",
                        (ulong) no_crc_val, (ulong) crc, (ulong) page_no));
#ifndef DBUG_OFF
    if (crc != no_crc_val)
      DBUG_PRINT("CRCerror", ("Wrong no CRC value"));
#endif
    DBUG_RETURN(test(crc != no_crc_val));
  }
  new_crc= maria_page_crc(page_no, page, data_length);
  DBUG_ASSERT(new_crc != no_crc_val);
  res= test(new_crc != crc);
  if (res)
  {
    DBUG_PRINT("CRCerror", ("Page: %lu  crc: 0x%lx  calculated crc: 0x%lx",
                            (ulong) page_no, (ulong) crc, (ulong) new_crc));
    maria_mark_crashed_share(share);
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
                                  uchar* data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  int data_length= share->block_size - CRC_SIZE;
  uint32 crc= maria_page_crc(page_no, page, data_length);
  DBUG_ENTER("maria_page_crc_set");

  DBUG_PRINT("info", ("Page %u  crc: 0x%lx",
                      (uint)page_no, (ulong)crc));

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
                                 uchar* data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  int data_length= _ma_get_page_used(share, page);
  uint32 crc= maria_page_crc(page_no, page, data_length);
  DBUG_ENTER("maria_page_crc_set");

  DBUG_PRINT("info", ("Page %u  crc: 0x%lx",
                      (uint)page_no, (ulong)crc));
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
                                  uchar* data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  return (maria_page_crc_check(page, page_no, share,
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
                                    uchar* data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  return (maria_page_crc_check(page, page_no, share,
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
                                   uchar* data_ptr)
{
  MARIA_SHARE *share= (MARIA_SHARE *)data_ptr;
  return (maria_page_crc_check(page, page_no, share,
                               MARIA_NO_CRC_NORMAL_PAGE,
                               _ma_get_page_used(share, page)));
}


/**
  @brief Maria pages write callback (sets the page filler for index/data)

  @param page            The page data to set
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Write callback data pointer (pointer to MARIA_SHARE)

  @retval 0 OK
*/

my_bool maria_page_filler_set_normal(uchar *page,
                                     __attribute__((unused))
                                     pgcache_page_no_t page_no,
                                     uchar* data_ptr)
{
  DBUG_ENTER("maria_page_filler_set_normal");
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
                                     __attribute__((unused))
                                     pgcache_page_no_t page_no,
                                     uchar* data_ptr)
{
  DBUG_ENTER("maria_page_filler_set_bitmap");
  int4store_aligned(page + ((MARIA_SHARE *)data_ptr)->block_size - CRC_SIZE,
                    MARIA_NO_CRC_BITMAP_PAGE);
  DBUG_RETURN(0);
}
