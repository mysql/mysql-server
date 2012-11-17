/* Copyright (C) 2007-2008 Michael Widenius

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
  Storage of records in block

  Some clarifications about the abbrev used:

  NULL fields      -> Fields that may have contain a NULL value.
  Not null fields  -> Fields that may not contain a NULL value.
  Critical fields  -> Fields that can't be null and can't be dropped without
		      causing a table reorganization.


  Maria will have a LSN at start of each page (excluding the bitmap pages)

  The different page types that are in a data file are:

  Bitmap pages     Map of free pages in the next extent (8192 page size
                   gives us 256M of mapped pages / bitmap)
  Head page        Start of rows are stored on this page.
                   A rowid always points to a head page
  Blob page        This page is totally filled with data from one blob or by
                   a set of long VARCHAR/CHAR fields
  Tail page        This contains the last part from different rows, blobs
                   or varchar fields.

  The data file starts with a bitmap page, followed by as many data
  pages as the bitmap can cover. After this there is a new bitmap page
  and more data pages etc.

  For information about the bitmap page, see ma_bitmap.c

  Structure of data and tail page:

  The page has a row directory at end of page to allow us to do deletes
  without having to reorganize the page.  It also allows us to later store
  some more bytes after each row to allow them to grow without having to move
  around other rows.

  Page header:

  LSN            7 bytes   Log position for last page change
  PAGE_TYPE      1 uchar   1 for head / 2 for tail / 3 for blob
  DIR_COUNT      1 uchar   Number of row/tail entries on page
  FREE_DIR_LINK  1 uchar   Pointer to first free director entry or 255 if no
  empty space    2 bytes  Empty space on page

  The most significant bit in PAGE_TYPE is set to 1 if the data on the page
  can be compacted to get more space. (PAGE_CAN_BE_COMPACTED)

  Row data

  Row directory of NO entries, that consist of the following for each row
  (in reverse order; i.e., first record is stored last):

  Position     2 bytes Position of row on page
  Length       2 bytes Length of entry

  For Position and Length, the 1 most significant bit of the position and
  the 1 most significant bit of the length could be used for some states of
  the row (in other words, we should try to keep these reserved)

  Position is 0 if the entry is not used.  In this case length[0] points
  to a previous free entry (255 if no previous entry) and length[1]
  to the next free entry (or 255 if last free entry). This works because
  the directory entry 255 can never be marked free (if the first directory
  entry is freed, the directory is shrinked).

  checksum     4 bytes  Reserved for full page read testing and live backup.

  ----------------

  Structure of blob pages:

  LSN          7 bytes  Log position for last page change
  PAGE_TYPE    1 uchar   3

  data

  -----------------

  Row data structure:

  Flag                          1 uchar   Marker of which header field exists
  TRANSID                       6 bytes  TRANSID of changing transaction
                                         (optional, added on insert and first
                                         update/delete)
  VER_PTR                       7 bytes  Pointer to older version in log
                                         (undo record)
                                         (optional, added after first
                                         update/delete)
  DELETE_TRANSID                6 bytes  (optional). TRANSID of original row.
                                         Added on delete.
  Nulls_extended                1 uchar   To allow us to add new DEFAULT NULL
                                         fields (optional, added after first
                                         change of row after alter table)
  Number of ROW_EXTENT's        1-3 uchar Length encoded, optional
                                         This is the number of extents the
                                         row is split into
  First row_extent              7 uchar  Pointer to first row extent (optional)

  Total length of length array  1-3 uchar Only used if we have
                                         char/varchar/blob fields.
  Row checksum		        1 uchar   Only if table created with checksums
  Null_bits             ..      One bit for each NULL field (a field that may
				have the value NULL)
  Empty_bits            ..      One bit for each field that may be 'empty'.
				(Both for null and not null fields).
                                This bit is 1 if the value for the field is
                                0 or empty string.

  field_offsets                 2 byte/offset
                                  For each 32'th field, there is one offset
                                  that points to where the field information
                                  starts in the block. This is to provide
                                  fast access to later field in the row
                                  when we only need to return a small
                                  set of fields.
                                  TODO: Implement this.

  Things marked above as 'optional' will only be present if the
  corresponding bit is set in 'Flag' field.  Flag gives us a way to
  get more space on a page when doing page compaction as we don't need
  to store TRANSID that have committed before the smallest running
  transaction we have in memory.

  Data in the following order:
  (Field order is precalculated when table is created)

  Critical fixed length, not null, fields. (Note, these can't be dropped)
  Fixed length, null fields

  Length array, 1-4 uchar per field for all CHAR/VARCHAR/BLOB fields.
  Number of bytes used in length array per entry is depending on max length
  for field.

  ROW_EXTENT's
  CHAR data (space stripped)
  VARCHAR data
  BLOB data

  Fields marked in null_bits or empty_bits are not stored in data part or
  length array.

  If row doesn't fit into the given block, then the first EXTENT will be
  stored last on the row. This is done so that we don't break any field
  data in the middle.

  We first try to store the full row into one block. If that's not possible
  we move out each big blob into their own extents. If this is not enough we
  move out a concatenation of all varchars to their own extent.

  Each blob and the concatenated char/varchar fields are stored the following
  way:
  - Store the parts in as many full-contiguous pages as possible.
  - The last part, that doesn't fill a full page, is stored in tail page.

  When doing an insert of a new row, we don't have to have
  VER_PTR in the row. This will make rows that are not changed stored
  efficiently. On update and delete we would add TRANSID (if it was an old
  committed row) and VER_PTR to
  the row. On row page compaction we can easily detect rows where
  TRANSID was committed before the longest running transaction
  started and we can then delete TRANSID and VER_PTR from the row to
  gain more space.

  If a row is deleted in Maria, we change TRANSID to the deleting
  transaction's id, change VER_PTR to point to the undo record for the delete,
  and add DELETE_TRANSID (the id of the transaction which last
  inserted/updated the row before its deletion). DELETE_TRANSID allows an old
  transaction to avoid reading the log to know if it can see the last version
  before delete (in other words it reduces the probability of having to follow
  VER_PTR). TODO: depending on a compilation option, evaluate the performance
  impact of not storing DELETE_TRANSID (which would make the row smaller).

  Description of the different parts:

  Flag is coded as:

  Description           bit
  TRANS_ID_exists       0
  VER_PTR_exists        1
  Row is deleted        2       (Means that DELETE_TRANSID exists)
  Nulls_extended_exists 3
  Row is split          7       This means that 'Number_of_row_extents' exists

  Nulls_extended is the number of new DEFAULT NULL fields in the row
  compared to the number of DEFAULT NULL fields when the first version
  of the table was created.  If Nulls_extended doesn't exist in the row,
  we know it's 0 as this must be one of the original rows from when the
  table was created first time.  This coding allows us to add 255*8 =
  2048 new fields without requiring a full alter table.

  Empty_bits is used to allow us to store 0, 0.0, empty string, empty
  varstring and empty blob efficiently. (This is very good for data
  warehousing where NULL's are often regarded as evil). Having this
  bitmap also allows us to drop information of a field during a future
  delete if field was deleted with ALTER TABLE DROP COLUMN.  To be able
  to handle DROP COLUMN, we must store in the index header the fields
  that has been dropped. When unpacking a row we will ignore dropped
  fields. When storing a row, we will mark a dropped field either with a
  null in the null bit map or in the empty_bits and not store any data
  for it.
  TODO: Add code for handling dropped fields.


  A ROW EXTENT is range of pages. One ROW_EXTENT is coded as:

  START_PAGE            5 bytes
  PAGE_COUNT            2 bytes.  Bit 16 is set if this is a tail page.
                                  Bit 15 is to set if this is start of a new
                                  blob extent.

  With 8K pages, we can cover 256M in one extent. This coding gives us a
  maximum file size of 2^40*8192 = 8192 tera

  As an example of ROW_EXTENT handling, assume a row with one integer
  field (value 5), two big VARCHAR fields (size 250 and 8192*3), and 2
  big BLOB fields that we have updated.

  The record format for storing this into an empty file would be:

  Page 1:

  00 00 00 00 00 00 00          LSN
  01                            Only one row in page
  FF                            No free dir entry
  xx xx                         Empty space on page

  10                            Flag: row split, VER_PTR exists
  01 00 00 00 00 00             TRANSID 1
  00 00 00 00 00 01 00          VER_PTR to first block in LOG file 1
  5                             Number of row extents
  02 00 00 00 00 03 00          VARCHAR's are stored in full pages 2,3,4
  0                             No null fields
  0                             No empty fields
  05 00 00 00 00 00 80          Tail page for VARCHAR, rowid 0
  06 00 00 00 00 80 00          First blob, stored at page 6-133
  05 00 00 00 00 01 80          Tail of first blob (896 bytes) at page 5
  86 00 00 00 00 80 00          Second blob, stored at page 134-262
  05 00 00 00 00 02 80          Tail of second blob (896 bytes) at page 5
  05 00                         5 integer
  FA                            Length of first varchar field (size 250)
  00 60                         Length of second varchar field (size 8192*3)
  00 60 10                      First medium BLOB, 1M
  01 00 10 00                   Second BLOB, 1M
  xx xx xx xx xx xx             Varchars are stored here until end of page

  ..... until end of page

  09 00 F4 1F                   Start position 9, length 8180
  xx xx xx xx			Checksum

  A data page is allowed to have a wrong CRC and header as long as it is
  marked empty in the bitmap and its directory's count is 0.
*/

#include "maria_def.h"
#include "ma_blockrec.h"
#include "trnman.h"
#include "ma_key_recover.h"
#include "ma_recovery_util.h"
#include <lf.h>

/*
  Struct for having a cursor over a set of extent.
  This is used to loop over all extents for a row when reading
  the row data. It's also used to store the tail positions for
  a read row to be used by a later update/delete command.
*/

typedef struct st_maria_extent_cursor
{
  /*
    Pointer to packed uchar array of extents for the row.
    Format is described above in the header
  */
  uchar *extent;
  /* Where data starts on page; Only for debugging */
  uchar *data_start;
  /* Position to all tails in the row. Updated when reading a row */
  MARIA_RECORD_POS *tail_positions;
  /* Current page */
  pgcache_page_no_t page;
  /* How many pages in the page region */
  uint page_count;
  /* What kind of lock to use for tail pages */
  enum pagecache_page_lock lock_for_tail_pages;
  /* Total number of extents (i.e., entries in the 'extent' slot) */
  uint extent_count;
  /* <> 0 if current extent is a tail page; Set while using cursor */
  uint tail;
  /* Position for tail on tail page */
  uint tail_row_nr;
  /*
    == 1 if we are working on the first extent (i.e., the one that is stored in
    the row header, not an extent that is stored as part of the row data).
  */
  my_bool first_extent;
} MARIA_EXTENT_CURSOR;


/**
   @brief Structure for passing down info to write_hook_for_clr_end().
   This hooks needs to know the variation of the live checksum caused by the
   current operation to update state.checksum under log's mutex,
   needs to know the transaction's previous undo_lsn to set
   trn->undo_lsn under log mutex, and needs to know the type of UNDO being
   undone now to modify state.records under log mutex.
*/

/** S:share,D:checksum_delta,E:expression,P:pointer_into_record,L:length */
#define store_checksum_in_rec(S,D,E,P,L)        do      \
  {                                                     \
    D= 0;                                               \
    if ((S)->calc_checksum != NULL)                     \
    {                                                   \
      D= (E);                                           \
      ha_checksum_store(P, D);                          \
      L+= HA_CHECKSUM_STORE_SIZE;                       \
    }                                                   \
  } while (0)


static my_bool delete_tails(MARIA_HA *info, MARIA_RECORD_POS *tails);
static my_bool delete_head_or_tail(MARIA_HA *info,
                                   pgcache_page_no_t page, uint record_number,
                                   my_bool head, my_bool from_update);
#ifndef DBUG_OFF
static void _ma_print_directory(FILE *file, uchar *buff, uint block_size);
#endif
static uchar *store_page_range(uchar *to, MARIA_BITMAP_BLOCK *block,
                               uint block_size, ulong length,
                               uint *tot_ranges);
static size_t fill_insert_undo_parts(MARIA_HA *info, const uchar *record,
                                     LEX_CUSTRING *log_parts,
                                     uint *log_parts_count);
static size_t fill_update_undo_parts(MARIA_HA *info, const uchar *oldrec,
                                     const uchar *newrec,
                                     LEX_CUSTRING *log_parts,
                                     uint *log_parts_count);

/****************************************************************************
  Initialization
****************************************************************************/

/*
  Initialize data needed for block structures
*/


/* Size of the different header elements for a row */

static uchar header_sizes[]=
{
  TRANSID_SIZE,
  VERPTR_SIZE,
  TRANSID_SIZE,                                 /* Delete transid */
  1                                             /* Null extends */
};

/*
  Calculate array of all used headers

  Used to speed up:

  size= 1;
  if (flag & 1)
    size+= TRANSID_SIZE;
  if (flag & 2)
    size+= VERPTR_SIZE;
  if (flag & 4)
    size+= TRANSID_SIZE
  if (flag & 8)
    size+= 1;

   NOTES
     This is called only once at startup of Maria
*/

static uchar total_header_size[1 << array_elements(header_sizes)];
#define PRECALC_HEADER_BITMASK (array_elements(total_header_size) -1)

void _ma_init_block_record_data(void)
{
  uint i;
  bzero(total_header_size, sizeof(total_header_size));
  total_header_size[0]= FLAG_SIZE;              /* Flag uchar */
  for (i= 1; i < array_elements(total_header_size); i++)
  {
    uint size= FLAG_SIZE, j, bit;
    for (j= 0; (bit= (1 << j)) <= i; j++)
    {
      if (i & bit)
        size+= header_sizes[j];
    }
    total_header_size[i]= size;
  }
}


my_bool _ma_once_init_block_record(MARIA_SHARE *share, File data_file)
{
  my_bool res;
  pgcache_page_no_t last_page; 

  /*
    First calculate the max file length with can have with a pointer of size
    rec_reflength.

    The 'rec_reflength - 1' is because one byte is used for row
    position withing the page.
    The /2 comes from _ma_transaction_recpos_to_keypos() where we use
    the lowest bit to mark if there is a transid following the rownr.
  */
  last_page= ((ulonglong) 1 << ((share->base.rec_reflength-1)*8))/2;
  if (!last_page)                                  /* Overflow; set max size */
    last_page= ~(pgcache_page_no_t) 0;

  res= _ma_bitmap_init(share, data_file, &last_page);
  share->base.max_data_file_length= _ma_safe_mul(last_page + 1,
                                                 share->block_size);
#if SIZEOF_OFF_T == 4
  set_if_smaller(share->base.max_data_file_length, INT_MAX32);
#endif
  return res;
}


my_bool _ma_once_end_block_record(MARIA_SHARE *share)
{
  int res= _ma_bitmap_end(share);
  if (share->bitmap.file.file >= 0)
  {
    if (flush_pagecache_blocks(share->pagecache, &share->bitmap.file,
                               ((share->temporary || share->deleting) ?
                                FLUSH_IGNORE_CHANGED :
                                FLUSH_RELEASE)))
      res= 1;
    /*
      File must be synced as it is going out of the maria_open_list and so
      becoming unknown to Checkpoint.
    */
    if (share->now_transactional &&
        my_sync(share->bitmap.file.file, MYF(MY_WME)))
      res= 1;
    if (my_close(share->bitmap.file.file, MYF(MY_WME)))
      res= 1;
    /*
      Trivial assignment to guard against multiple invocations
      (May happen if file are closed but we want to keep the maria object
      around a bit longer)
    */
    share->bitmap.file.file= -1;
  }
  if (share->id != 0)
  {
    /*
      We de-assign the id even though index has not been flushed, this is ok
      as close_lock serializes us with a Checkpoint looking at our share.
    */
    translog_deassign_id_from_share(share);
  }
  return res;
}


/* Init info->cur_row structure */

my_bool _ma_init_block_record(MARIA_HA *info)
{
  MARIA_ROW *row= &info->cur_row, *new_row= &info->new_row;
  MARIA_SHARE *share= info->s;
  uint default_extents;
  DBUG_ENTER("_ma_init_block_record");

  if (!my_multi_malloc(MY_WME,
                       &row->empty_bits, share->base.pack_bytes,
                       &row->field_lengths,
                       share->base.max_field_lengths + 2,
                       &row->blob_lengths, sizeof(ulong) * share->base.blobs,
                       &row->null_field_lengths, (sizeof(uint) *
                                                  (share->base.fields -
                                                   share->base.blobs +
                                                   EXTRA_LENGTH_FIELDS)),
                       &row->tail_positions, (sizeof(MARIA_RECORD_POS) *
                                              (share->base.blobs + 2)),
                       &new_row->empty_bits, share->base.pack_bytes,
                       &new_row->field_lengths,
                       share->base.max_field_lengths + 2,
                       &new_row->blob_lengths,
                       sizeof(ulong) * share->base.blobs,
                       &new_row->null_field_lengths, (sizeof(uint) *
                                                      (share->base.fields -
                                                       share->base.blobs +
                                                       EXTRA_LENGTH_FIELDS)),
                       &info->log_row_parts,
                       sizeof(*info->log_row_parts) *
                       (TRANSLOG_INTERNAL_PARTS + 3 +
                        share->base.fields + 3),
                       &info->update_field_data,
                       (share->base.fields * 4 +
                        share->base.max_field_lengths + 1 + 4),
                       NullS, 0))
    DBUG_RETURN(1);
  /* Skip over bytes used to store length of field length for logging */
  row->field_lengths+= 2;
  new_row->field_lengths+= 2;

  /* Reserve some initial space to avoid mallocs during execution */
  default_extents= (ELEMENTS_RESERVED_FOR_MAIN_PART + 1 +
                    (AVERAGE_BLOB_SIZE /
                     FULL_PAGE_SIZE(share->block_size) /
                     BLOB_SEGMENT_MIN_SIZE));

  if (my_init_dynamic_array(&info->bitmap_blocks,
                            sizeof(MARIA_BITMAP_BLOCK), default_extents,
                            64))
    goto err;
  info->cur_row.extents_buffer_length= default_extents * ROW_EXTENT_SIZE;
  if (!(info->cur_row.extents= my_malloc(info->cur_row.extents_buffer_length,
                                         MYF(MY_WME))))
    goto err;

  info->row_base_length= share->base_length;
  info->row_flag= share->base.default_row_flag;

  /*
    We need to reserve 'EXTRA_LENGTH_FIELDS' number of parts in
    null_field_lengths to allow splitting of rows in 'find_where_to_split_row'
  */
  row->null_field_lengths+= EXTRA_LENGTH_FIELDS;
  new_row->null_field_lengths+= EXTRA_LENGTH_FIELDS;

  DBUG_RETURN(0);

err:
  _ma_end_block_record(info);
  DBUG_RETURN(1);
}


void _ma_end_block_record(MARIA_HA *info)
{
  DBUG_ENTER("_ma_end_block_record");
  my_free(info->cur_row.empty_bits, MYF(MY_ALLOW_ZERO_PTR));
  delete_dynamic(&info->bitmap_blocks);
  my_free(info->cur_row.extents, MYF(MY_ALLOW_ZERO_PTR));
  my_free(info->blob_buff, MYF(MY_ALLOW_ZERO_PTR));
  /*
    The data file is closed, when needed, in ma_once_end_block_record().
    The following protects us from doing an extra, not allowed, close
    in maria_close()
  */
  info->dfile.file= -1;
  DBUG_VOID_RETURN;
}


/****************************************************************************
  Helper functions
****************************************************************************/

/*
  Return the next unused postion on the page after a directory entry.

  SYNOPSIS
    start_of_next_entry()
    dir		Directory entry to be used. This can not be the
                the last entry on the page!

  RETURN
    #   Position in page where next entry starts.
        Everything between the '*dir' and this are free to be used.
*/

static inline uint start_of_next_entry(uchar *dir)
{
  uchar *prev;
  /*
     Find previous used entry. (There is always a previous entry as
     the directory never starts with a deleted entry)
  */
  for (prev= dir - DIR_ENTRY_SIZE ;
       prev[0] == 0 && prev[1] == 0 ;
       prev-= DIR_ENTRY_SIZE)
  {}
  return (uint) uint2korr(prev);
}


/*
  Return the offset where the previous entry ends (before on page)

  SYNOPSIS
    end_of_previous_entry()
    dir		Address for current directory entry
    end         Address to last directory entry

  RETURN
    #   Position where previous entry ends (smallest address on page)
        Everything between # and current entry are free to be used.
*/


static inline uint end_of_previous_entry(uchar *dir, uchar *end)
{
  uchar *pos;
  for (pos= dir + DIR_ENTRY_SIZE ; pos < end ; pos+= DIR_ENTRY_SIZE)
  {
    uint offset;
    if ((offset= uint2korr(pos)))
      return offset + uint2korr(pos+2);
  }
  return PAGE_HEADER_SIZE;
}


#ifndef DBUG_OFF

static void _ma_print_directory(FILE *file, uchar *buff, uint block_size)
{
  uint max_entry= (uint) ((uchar *) buff)[DIR_COUNT_OFFSET], row= 0;
  uint end_of_prev_row= PAGE_HEADER_SIZE;
  uchar *dir, *end;

  dir= dir_entry_pos(buff, block_size, max_entry-1);
  end= dir_entry_pos(buff, block_size, 0);

  DBUG_LOCK_FILE;                               /* If using DBUG_FILE */
  fprintf(file,"Directory dump (pos:length):\n");

  for (row= 1; dir <= end ; end-= DIR_ENTRY_SIZE, row++)
  {
    uint offset= uint2korr(end);
    uint length= uint2korr(end+2);
    fprintf(file, "   %4u:%4u", offset, offset ? length : 0);
    if (!(row % (80/12)))
      fputc('\n', file);
    if (offset)
    {
      DBUG_ASSERT(offset >= end_of_prev_row);
      end_of_prev_row= offset + length;
    }
  }
  fputc('\n', file);
  fflush(file);
  DBUG_UNLOCK_FILE;
}


static void check_directory(uchar *buff, uint block_size, uint min_row_length,
                            uint real_empty_size)
{
  uchar *dir, *end;
  uint max_entry= (uint) buff[DIR_COUNT_OFFSET];
  uint start_of_dir, deleted;
  uint end_of_prev_row= PAGE_HEADER_SIZE;
  uint empty_size_on_page;
  uint empty_size;
  uchar free_entry, prev_free_entry;

  dir= dir_entry_pos(buff, block_size, max_entry-1);
  start_of_dir= (uint) (dir - buff);
  end= dir_entry_pos(buff, block_size, 0);
  deleted= empty_size= 0;

  empty_size_on_page= (real_empty_size != (uint) -1 ? real_empty_size :
                       uint2korr(buff + EMPTY_SPACE_OFFSET));

  /* Ensure that all rows are in increasing order and no overlaps */
  for (; dir <= end ; end-= DIR_ENTRY_SIZE)
  {
    uint offset= uint2korr(end);
    uint length= uint2korr(end+2);
    if (offset)
    {
      DBUG_ASSERT(offset >= end_of_prev_row);
      DBUG_ASSERT(!length || length >= min_row_length);
      empty_size+= offset - end_of_prev_row;
      end_of_prev_row= offset + length;
    }
    else
      deleted++;
  }
  empty_size+= start_of_dir - end_of_prev_row;
  DBUG_ASSERT(end_of_prev_row <= start_of_dir);
  DBUG_ASSERT(empty_size == empty_size_on_page);

  /* check free links */
  free_entry= buff[DIR_FREE_OFFSET];
  prev_free_entry= END_OF_DIR_FREE_LIST;
  while (free_entry != END_OF_DIR_FREE_LIST)
  {
    uchar *dir= dir_entry_pos(buff, block_size, free_entry);
    DBUG_ASSERT(dir[0] == 0 && dir[1] == 0);
    DBUG_ASSERT(dir[2] == prev_free_entry);
    prev_free_entry= free_entry;
    free_entry= dir[3];
    deleted--;
  }
  DBUG_ASSERT(deleted == 0);
}
#else
#define check_directory(A,B,C,D)
#endif /* DBUG_OFF */


/**
   @brief Calculate if there is enough entries on the page
*/

static my_bool enough_free_entries(uchar *buff, uint block_size,
                                   uint wanted_entries)
{
  uint entries= (uint) buff[DIR_COUNT_OFFSET];
  uint needed_free_entries, free_entry;

  if (entries + wanted_entries <= MAX_ROWS_PER_PAGE)
    return 1;

  /* Check if enough free entries in free list */
  needed_free_entries= entries + wanted_entries - MAX_ROWS_PER_PAGE;

  free_entry= (uint) buff[DIR_FREE_OFFSET];
  while (free_entry != END_OF_DIR_FREE_LIST)
  {
    uchar *dir;
    if (!--needed_free_entries)
      return 1;
    dir= dir_entry_pos(buff, block_size, free_entry);
    free_entry= dir[3];
  }
  return 0;                                     /* Not enough entries */
}


/**
   @brief Check if there is room for more rows on page

   @fn enough_free_entries_on_page

   @return 0    Directory is full
   @return 1	There is room for more entries on the page
*/

my_bool enough_free_entries_on_page(MARIA_SHARE *share,
                                    uchar *page_buff)
{
  enum en_page_type page_type;
  page_type= (enum en_page_type) (page_buff[PAGE_TYPE_OFFSET] &
                                  ~(uchar) PAGE_CAN_BE_COMPACTED);

  if (page_type == HEAD_PAGE)
  {
    uint row_count= (uint) page_buff[DIR_COUNT_OFFSET];
    return !(row_count == MAX_ROWS_PER_PAGE &&
             page_buff[DIR_FREE_OFFSET] == END_OF_DIR_FREE_LIST);
  }
  return enough_free_entries(page_buff, share->block_size,
                             1 + share->base.blobs);
}


/**
   @brief Extend a record area to fit a given size block

   @fn extend_area_on_page()
   @param info                  Handler if head page and 0 if tail page
   @param buff			Page buffer
   @param dir			Pointer to dir entry in buffer
   @param rownr			Row number we working on
   @param block_size		Block size of buffer
   @param request_length	How much data we want to put at [dir]
   @param empty_space		Total empty space in buffer
			        This is updated with length after dir
                                is allocated and current block freed

  @implementation
    The logic is as follows (same as in _ma_update_block_record())
    - If new data fits in old block, use old block.
    - Extend block with empty space before block. If enough, use it.
    - Extend block with empty space after block. If enough, use it.
    - Use _ma_compact_block_page() to get all empty space at dir.

  @note
    The given directory entry is set to rec length.
    empty_space doesn't include the new directory entry


  @return
  @retval 0   ok
  @retval ret_offset		Pointer to store offset to found area
  @retval ret_length		Pointer to store length of found area
  @retval [dir]                 rec_offset is store here too

  @retval 1   error (wrong info in block)
*/

static my_bool extend_area_on_page(MARIA_HA *info,
                                   uchar *buff, uchar *dir,
                                   uint rownr, uint block_size,
                                   uint request_length,
                                   uint *empty_space, uint *ret_offset,
                                   uint *ret_length)
{
  uint rec_offset, length, org_rec_length;
  uint max_entry= (uint) buff[DIR_COUNT_OFFSET];
  DBUG_ENTER("extend_area_on_page");

  /*
    We can't check for min length here as we may have called
    extend_directory() to create a new (empty) entry just before
  */
  check_directory(buff, block_size, 0, *empty_space);

  rec_offset= uint2korr(dir);
  if (rec_offset)
  {
    /* Extending old row;  Mark current space as 'free' */
    length= org_rec_length= uint2korr(dir + 2);
    DBUG_PRINT("info", ("rec_offset: %u  length: %u  request_length: %u  "
                        "empty_space: %u",
                        rec_offset, org_rec_length, request_length,
                        *empty_space));

    *empty_space+= org_rec_length;
  }
  else
  {
    /* Reusing free directory entry; Free it from the directory list */
    if (dir[2] == END_OF_DIR_FREE_LIST)
      buff[DIR_FREE_OFFSET]= dir[3];
    else
    {
      uchar *prev_dir= dir_entry_pos(buff, block_size, (uint) dir[2]);
      DBUG_ASSERT(uint2korr(prev_dir) == 0 && prev_dir[3] == (uchar) rownr);
      prev_dir[3]= dir[3];
    }
    if (dir[3] != END_OF_DIR_FREE_LIST)
    {
      uchar *next_dir= dir_entry_pos(buff, block_size, (uint) dir[3]);
      DBUG_ASSERT(uint2korr(next_dir) == 0 && next_dir[2] == (uchar) rownr);
      next_dir[2]= dir[2];
    }
    rec_offset= start_of_next_entry(dir);
    length= 0;
  }
  if (length < request_length)
  {
    uint old_rec_offset;
    /*
      New data did not fit in old position.
      Find first possible position where to put new data.
    */
    old_rec_offset= rec_offset;
    rec_offset= end_of_previous_entry(dir, buff + block_size -
                                      PAGE_SUFFIX_SIZE);
    length+= (uint) (old_rec_offset - rec_offset);
    DBUG_ASSERT(old_rec_offset);
    /*
      'length' is 0 if we are doing an insert into a not allocated block.
      This can only happen during "REDO of INSERT" or "UNDO of DELETE."
    */
    if (length < request_length)
    {
      /*
        Did not fit in current block + empty space. Extend with
        empty space after block.
      */
      if (rownr == max_entry - 1)
      {
        /* Last entry; Everything is free between this and directory */
        length= ((block_size - PAGE_SUFFIX_SIZE - DIR_ENTRY_SIZE * max_entry) -
                 rec_offset);
      }
      else
        length= start_of_next_entry(dir) - rec_offset;
      DBUG_ASSERT((int) length >= 0);
      if (length < request_length)
      {
        /* Not enough continuous space, compact page to get more */
        int2store(dir, rec_offset);
        /* Reset length, as this may be a deleted block */
        int2store(dir+2, 0);
        _ma_compact_block_page(buff, block_size, rownr, 1,
                               info ? info->trn->min_read_from: 0,
                               info ? info->s->base.min_block_length : 0);
        rec_offset= uint2korr(dir);
        length=     uint2korr(dir+2);
        if (length < request_length)
        {
          DBUG_PRINT("error", ("Not enough space: "
                               "length: %u  request_length: %u",
                               length, request_length));
          _ma_set_fatal_error(info->s, HA_ERR_WRONG_IN_RECORD);
          DBUG_RETURN(1);                       /* Error in block */
        }
        *empty_space= length;                   /* All space is here */
      }
    }
  }
  int2store(dir, rec_offset);
  int2store(dir + 2, length);
  *ret_offset= rec_offset;
  *ret_length= length;

  check_directory(buff, block_size, info ? info->s->base.min_block_length : 0,
                  *empty_space - length);
  DBUG_RETURN(0);
}


/**
   @brief Copy not changed fields from 'from' to 'to'

   @notes
   Assumption is that most fields are not changed!
   (Which is why we don't test if all bits are set for some bytes in bitmap)
*/

void copy_not_changed_fields(MARIA_HA *info, MY_BITMAP *changed_fields,
                             uchar *to, uchar *from)
{
  MARIA_COLUMNDEF *column, *end_column;
  uchar *bitmap= (uchar*) changed_fields->bitmap;
  MARIA_SHARE *share= info->s;
  uint bit= 1;

  for (column= share->columndef, end_column= column+ share->base.fields;
       column < end_column; column++)
  {
    if (!(*bitmap & bit))
    {
      uint field_length= column->length;
      if (column->type == FIELD_VARCHAR)
      {
        if (column->fill_length == 1)
          field_length= (uint) from[column->offset] + 1;
        else
          field_length= uint2korr(from + column->offset) + 2;
      }
      memcpy(to + column->offset, from + column->offset, field_length);
    }
    if ((bit= (bit << 1)) == 256)
    {
      bitmap++;
      bit= 1;
    }
  }
}

#ifdef NOT_YET_NEEDED
/* Calculate empty space on a page */

static uint empty_space_on_page(uchar *buff, uint block_size)
{
  enum en_page_type;
  page_type= (enum en_page_type) (buff[PAGE_TYPE_OFFSET] &
                                  ~(uchar) PAGE_CAN_BE_COMPACTED);
  if (page_type == UNALLOCATED_PAGE)
    return block_size;
  if ((uint) page_type <= TAIL_PAGE)
    return uint2korr(buff+EMPTY_SPACE_OFFSET);
  return 0;                                     /* Blob page */
}
#endif


/*
  @brief Ensure we have space for new directory entries

  @fn make_space_for_directory()
  @param buff		Page buffer
  @param block_size	Block size for pages
  @param max_entry	Number of current entries in directory
  @param count		Number of new entries to be added to directory
  @param first_dir	First directory entry on page
  @param empty_space    Total empty space in buffer. It's updated
			to reflect the new empty space
  @param first_pos      Store position to last data byte on page here

  @note
  This function is inline as the argument passing is the biggest
  part of the function

  @return
  @retval 0  ok
  @retval 1  error (No data on page, fatal error)
*/

static inline my_bool
make_space_for_directory(MARIA_HA *info,
                         uchar *buff, uint block_size, uint max_entry,
                         uint count, uchar *first_dir, uint *empty_space,
                         uint *first_pos)
{
  uint length_needed= DIR_ENTRY_SIZE * count;

  /*
    The following is not true only in the case and UNDO is used to reinsert
    a row on a previously not used page
  */
  if (likely(max_entry))
  {
    /* Check if there is place for the directory entry on the page */
    *first_pos= uint2korr(first_dir) + uint2korr(first_dir + 2);

    if ((uint) (first_dir - buff) < *first_pos + length_needed)
    {
      /* Create place for directory */
      _ma_compact_block_page(buff, block_size, max_entry - 1, 0,
                             info ? info->trn->min_read_from : 0,
                             info ? info->s->base.min_block_length : 0);
      *first_pos= (uint2korr(first_dir) + uint2korr(first_dir + 2));
      *empty_space= uint2korr(buff + EMPTY_SPACE_OFFSET);
      if (*empty_space < length_needed)
      {
        /*
          We should always have space, as we only come here for
          UNDO of DELETE (in which case we know the row was on the
          page before) or if the bitmap told us there was space on page
        */
        DBUG_ASSERT(!maria_assert_if_crashed_table);
        return(1);
      }
    }
  }
  else
    *first_pos= PAGE_HEADER_SIZE;

  /* Reduce directory entry size from free space size */
  (*empty_space)-= length_needed;
  buff[DIR_COUNT_OFFSET]= (uchar) (max_entry + count);
  return(0);
}


/*
  Find free position in directory

  SYNOPSIS
  find_free_position()
    info                Handler if head page and 0 otherwise
    buff                Page
    block_size          Size of page
    res_rownr           Store index to free position here
    res_length		Store length of found segment here
    empty_space		Store length of empty space on disk here. This is
		        all empty space, including the found block.

  NOTES
    If there is a free directory entry (entry with position == 0),
    then use it and change it to be the size of the empty block
    after the previous entry. This guarantees that all row entries
    are stored on disk in inverse directory order, which makes life easier for
    '_ma_compact_block_page()' and to know if there is free space after any
    block.

    If there is no free entry (entry with position == 0), then we create
    a new one. If there is not space for the directory entry (because
    the last block overlapps with the directory), we compact the page.

    We will update the offset and the length of the found dir entry to
    match the position and empty space found.

    buff[EMPTY_SPACE_OFFSET] is NOT updated but left up to the caller

    See start of file for description of how free directory entires are linked

  RETURN
    0      Error (directory full or last block goes over directory)
    #      Pointer to directory entry on page
*/

static uchar *find_free_position(MARIA_HA *info,
                                 uchar *buff, uint block_size, uint *res_rownr,
                                 uint *res_length, uint *empty_space)
{
  uint max_entry, free_entry;
  uint length, first_pos;
  uchar *dir, *first_dir;
  DBUG_ENTER("find_free_position");

  max_entry= (uint) buff[DIR_COUNT_OFFSET];
  free_entry= (uint) buff[DIR_FREE_OFFSET];
  *empty_space= uint2korr(buff + EMPTY_SPACE_OFFSET);

  DBUG_PRINT("info", ("max_entry: %u  free_entry: %u", max_entry, free_entry));

  first_dir= dir_entry_pos(buff, block_size, max_entry - 1);

  /* Search after first free position */
  if (free_entry != END_OF_DIR_FREE_LIST)
  {
    if (free_entry >= max_entry)
      DBUG_RETURN(0);                           /* Consistency error */
    dir= dir_entry_pos(buff, block_size, free_entry);
    DBUG_ASSERT(uint2korr(dir) == 0 && dir[2] == END_OF_DIR_FREE_LIST);
    /* Relink free list */
    if ((buff[DIR_FREE_OFFSET]= dir[3]) != END_OF_DIR_FREE_LIST)
    {
      uchar *next_entry= dir_entry_pos(buff, block_size, (uint) dir[3]);
      DBUG_ASSERT((uint) next_entry[2] == free_entry &&
                  uint2korr(next_entry) == 0);
      next_entry[2]= END_OF_DIR_FREE_LIST;      /* Backlink */
    }

    first_pos= end_of_previous_entry(dir, buff + block_size -
                                     PAGE_SUFFIX_SIZE);
    length= start_of_next_entry(dir) - first_pos;
    int2store(dir, first_pos);                /* Update dir entry */
    int2store(dir + 2, 0);
    *res_rownr= free_entry;
    *res_length= length;

    check_directory(buff, block_size,
                    info ? info->s->base.min_block_length : 0, (uint) -1);
    DBUG_RETURN(dir);
  }
  /* No free places in dir; create a new one */

  /* Check if there is place for the directory entry */
  if (max_entry == MAX_ROWS_PER_PAGE)
    DBUG_RETURN(0);

  if (make_space_for_directory(info, buff, block_size, max_entry, 1,
                               first_dir, empty_space, &first_pos))
    DBUG_RETURN(0);

  dir= first_dir - DIR_ENTRY_SIZE;
  length= (uint) (dir - buff - first_pos);
  DBUG_ASSERT(length <= *empty_space);
  int2store(dir, first_pos);
  int2store(dir + 2, 0);                      /* Max length of region */
  *res_rownr= max_entry;
  *res_length= length;

  check_directory(buff, block_size, info ? info->s->base.min_block_length : 0,
                  *empty_space);
  DBUG_RETURN(dir);
}


/**
   @brief Enlarge page directory to hold more entries

   @fn extend_directory()
   @param info          Handler if head page and 0 otherwise
   @param buff		Page buffer
   @param block_size	Block size
   @param max_entry	Number of directory entries on page
   @param new_entry	Position for new entry
   @param empty_space	Total empty space in buffer. It's updated
			to reflect the new empty space

   @note
   This is only called on UNDO when we want to expand the directory
   to be able to re-insert row in a given position

   The new directory entry will be set to cover the maximum possible space

   @return
   @retval 0  ok
   @retval 1  error (No data on page, fatal error)
*/

static my_bool extend_directory(MARIA_HA *info, uchar *buff, uint block_size,
                                uint max_entry, uint new_entry,
                                uint *empty_space)
{
  uint length, first_pos;
  uchar *dir, *first_dir;
  DBUG_ENTER("extend_directory");

  /*
    Note that in if max_entry is 0, then first_dir will point to
    an illegal directory entry. This is ok, as in this case we will
    not access anything through first_dir.
  */
  first_dir= dir_entry_pos(buff, block_size, max_entry) + DIR_ENTRY_SIZE;

  if (make_space_for_directory(info, buff, block_size, max_entry,
                               new_entry - max_entry + 1,
                               first_dir, empty_space, &first_pos))
    DBUG_RETURN(1);

  /* Set the new directory entry to cover the max possible length */
  dir= first_dir - DIR_ENTRY_SIZE * (new_entry - max_entry + 1);
  length= (uint) (dir - buff - first_pos);
  int2store(dir, first_pos);
  int2store(dir+2, length);
  *empty_space-= length;

  if (new_entry-- > max_entry)
  {
    /* Link all row entries between new_entry and max_entry into free list */
    uint free_entry= (uint) buff[DIR_FREE_OFFSET];
    uint prev_entry= END_OF_DIR_FREE_LIST;
    buff[DIR_FREE_OFFSET]= new_entry;
    do
    {
      dir+= DIR_ENTRY_SIZE;
      dir[0]= dir[1]= 0;
      dir[2]= (uchar) prev_entry;
      dir[3]= (uchar) new_entry-1;
      prev_entry= new_entry;
    } while (new_entry-- > max_entry);
    if ((dir[3]= free_entry) != END_OF_DIR_FREE_LIST)
    {
      /* Relink next entry to point to newly freed entry */
      uchar *next_entry= dir_entry_pos(buff, block_size, (uint) dir[3]);
      DBUG_ASSERT(uint2korr(next_entry) == 0 &&
                  next_entry[2] == END_OF_DIR_FREE_LIST);
      next_entry[2]= max_entry;
    }
  }

  check_directory(buff, block_size,
                  info ? min(info->s->base.min_block_length, length) : 0,
                  *empty_space);
  DBUG_RETURN(0);
}


/****************************************************************************
  Updating records
****************************************************************************/

/*
  Calculate length of all the different field parts

  SYNOPSIS
    calc_record_size()
    info	Maria handler
    record      Row to store
    row		Store statistics about row here

  NOTES
    The statistics is used to find out how much space a row will need
    and also where we can split a row when we need to split it into several
    extents.
*/

static void calc_record_size(MARIA_HA *info, const uchar *record,
                             MARIA_ROW *row)
{
  MARIA_SHARE *share= info->s;
  uchar *field_length_data;
  MARIA_COLUMNDEF *column, *end_column;
  uint *null_field_lengths= row->null_field_lengths;
  ulong *blob_lengths= row->blob_lengths;
  DBUG_ENTER("calc_record_size");

  row->normal_length= row->char_length= row->varchar_length=
    row->blob_length= row->extents_count= 0;

  /* Create empty bitmap and calculate length of each varlength/char field */
  bzero(row->empty_bits, share->base.pack_bytes);
  field_length_data= row->field_lengths;
  for (column= share->columndef + share->base.fixed_not_null_fields,
       end_column= share->columndef + share->base.fields;
       column < end_column; column++, null_field_lengths++)
  {
    if ((record[column->null_pos] & column->null_bit))
    {
      if (column->type != FIELD_BLOB)
        *null_field_lengths= 0;
      else
        *blob_lengths++= 0;
      continue;
    }
    switch (column->type) {
    case FIELD_CHECK:
    case FIELD_NORMAL:                          /* Fixed length field */
    case FIELD_ZERO:
      DBUG_ASSERT(column->empty_bit == 0);
      /* fall through */
    case FIELD_SKIP_PRESPACE:                   /* Not packed */
      row->normal_length+= column->length;
      *null_field_lengths= column->length;
      break;
    case FIELD_SKIP_ZERO:                       /* Fixed length field */
      if (memcmp(record+ column->offset, maria_zero_string,
                 column->length) == 0)
      {
        row->empty_bits[column->empty_pos] |= column->empty_bit;
        *null_field_lengths= 0;
      }
      else
      {
        row->normal_length+= column->length;
        *null_field_lengths= column->length;
      }
      break;
    case FIELD_SKIP_ENDSPACE:                   /* CHAR */
    {
      const uchar *pos, *end;
      for (pos= record + column->offset, end= pos + column->length;
           end > pos && end[-1] == ' '; end--)
        ;
      if (pos == end)                           /* If empty string */
      {
        row->empty_bits[column->empty_pos]|= column->empty_bit;
        *null_field_lengths= 0;
      }
      else
      {
        uint length= (uint) (end - pos);
        if (column->length <= 255)
          *field_length_data++= (uchar) length;
        else
        {
          int2store(field_length_data, length);
          field_length_data+= 2;
        }
        row->char_length+= length;
        *null_field_lengths= length;
      }
      break;
    }
    case FIELD_VARCHAR:
    {
      uint length, field_length_data_length;
      const uchar *field_pos= record + column->offset;

      /* 256 is correct as this includes the length uchar */
      field_length_data[0]= field_pos[0];
      if (column->length <= 256)
      {
        length= (uint) (uchar) *field_pos;
        field_length_data_length= 1;
      }
      else
      {
        length= uint2korr(field_pos);
        field_length_data[1]= field_pos[1];
        field_length_data_length= 2;
      }
      *null_field_lengths= length;
      if (!length)
      {
        row->empty_bits[column->empty_pos]|= column->empty_bit;
        break;
      }
      row->varchar_length+= length;
      *null_field_lengths= length;
      field_length_data+= field_length_data_length;
      break;
    }
    case FIELD_BLOB:
    {
      const uchar *field_pos= record + column->offset;
      uint size_length= column->length - portable_sizeof_char_ptr;
      ulong blob_length= _ma_calc_blob_length(size_length, field_pos);

      *blob_lengths++= blob_length;
      if (!blob_length)
        row->empty_bits[column->empty_pos]|= column->empty_bit;
      else
      {
        row->blob_length+= blob_length;
        memcpy(field_length_data, field_pos, size_length);
        field_length_data+= size_length;
      }
      break;
    }
    default:
      DBUG_ASSERT(0);
    }
  }
  row->field_lengths_length= (uint) (field_length_data - row->field_lengths);
  /*
    - info->row_base_length is base information we must have on a page in first
      extent:
      - flag byte (1) + is_nulls_extended (0 | 1) + null_bytes + pack_bytes +
        table_checksum (0 | 1)
    - row->min_length is minimum amount of data we must store on
      a page. bitmap code will ensure we get at list this much +
      total number of extents and one extent information
    - fixed_not_null_fields_length is length of fixed length fields that can't
      be compacted
    - head_length is the amount of data for the head page
     (ie, all fields except blobs)
  */
  row->min_length=   (info->row_base_length +
                      (share->base.max_field_lengths ?
                       size_to_store_key_length(row->field_lengths_length) :
                       0));
  row->head_length= (row->min_length +
                     share->base.fixed_not_null_fields_length +
                     row->field_lengths_length +
                     row->normal_length +
                     row->char_length + row->varchar_length);
  row->total_length= (row->head_length + row->blob_length);
  if (row->total_length < share->base.min_block_length)
    row->total_length= share->base.min_block_length;
  DBUG_PRINT("exit", ("head_length: %lu  total_length: %lu",
                      (ulong) row->head_length, (ulong) row->total_length));
  DBUG_VOID_RETURN;
}


/**
  Compact page by removing all space between rows

  Moves up all rows to start of page. Moves blocks that are directly after
  each other with one memmove.

  @note if rownr is the last row in the page, and extend_block is false,
  caller has to make sure to update bitmap page afterwards to reflect freed
  space.

  @param  buff          Page to compact
  @param  block_size    Size of page
  @param  rownr         Put empty data after this row
  @param  extend_block	If 1, extend the block at 'rownr' to cover the
                        whole block.
  @param  min_read_from If <> 0, remove all trid's that are less than this
*/

void _ma_compact_block_page(uchar *buff, uint block_size, uint rownr,
                            my_bool extend_block, TrID min_read_from,
                            uint min_row_length)
{
  uint max_entry= (uint) buff[DIR_COUNT_OFFSET];
  uint page_pos, next_free_pos, start_of_found_block, diff, end_of_found_block;
  uint freed_size= 0;
  uchar *dir, *end;
  DBUG_ENTER("_ma_compact_block_page");
  DBUG_PRINT("enter", ("rownr: %u  min_read_from: %lu", rownr,
                       (ulong) min_read_from));
  DBUG_ASSERT(max_entry > 0 &&
              max_entry < (block_size - PAGE_HEADER_SIZE -
                           PAGE_SUFFIX_SIZE) / DIR_ENTRY_SIZE);

  /* Move all entries before and including rownr up to start of page */
  dir= dir_entry_pos(buff, block_size, rownr);
  end= dir_entry_pos(buff, block_size, 0);
  page_pos= next_free_pos= start_of_found_block= PAGE_HEADER_SIZE;
  diff= 0;
  for (; dir <= end ; end-= DIR_ENTRY_SIZE)
  {
    uint offset= uint2korr(end);

    if (offset)
    {
      uint row_length= uint2korr(end + 2);
      DBUG_ASSERT(offset >= page_pos);
      DBUG_ASSERT(buff + offset + row_length <= dir);
      DBUG_ASSERT(row_length >= min_row_length || row_length == 0);

      /* Row length can be zero if row is to be deleted */
      if (min_read_from && row_length && (buff[offset] & ROW_FLAG_TRANSID))
      {
        TrID transid= transid_korr(buff+offset+1);
        if (transid < min_read_from)
        {
          /* Remove transid from row by moving the start point of the row up */
          buff[offset + TRANSID_SIZE]= buff[offset] & ~ROW_FLAG_TRANSID;
          offset+= TRANSID_SIZE;
          freed_size+= TRANSID_SIZE;
          row_length-= TRANSID_SIZE;
          int2store(end+2, row_length);
        }
      }

      if (offset != next_free_pos)
      {
        uint length= (next_free_pos - start_of_found_block);
        /*
          There was empty space before this and prev block
          Check if we have to move previous block up to page start
        */
        if (page_pos != start_of_found_block)
        {
          /* move up previous block */
          memmove(buff + page_pos, buff + start_of_found_block, length);
        }
        page_pos+= length;
        /* next continuous block starts here */
        start_of_found_block= offset;
        diff= offset - page_pos;
      }
      int2store(end, offset - diff);            /* correct current pos */
      next_free_pos= offset + row_length;

      if (unlikely(row_length < min_row_length) && row_length)
      {
        /*
          This can only happen in the case we compacted transid and
          the row become 'too short'

          Move the current row down to it's right place and extend it
          with 0.
        */
        uint row_diff= min_row_length - row_length;
        uint length= (next_free_pos - start_of_found_block);

        DBUG_ASSERT(page_pos != start_of_found_block);
        bmove(buff + page_pos, buff + start_of_found_block, length);
        bzero(buff+ page_pos + length, row_diff);
        page_pos+= min_row_length;
        int2store(end+2, min_row_length);
        freed_size-= row_diff;
        next_free_pos= start_of_found_block= page_pos;
        diff= 0;
      }
    }
  }
  if (page_pos != start_of_found_block)
  {
    uint length= (next_free_pos - start_of_found_block);
    memmove(buff + page_pos, buff + start_of_found_block, length);
  }
  start_of_found_block= uint2korr(dir);

  if (rownr != max_entry - 1)
  {
    /* Move all entries after rownr to end of page */
    uint rownr_length;

    DBUG_ASSERT(extend_block);                  /* Should always be true */
    next_free_pos= end_of_found_block= page_pos=
      block_size - DIR_ENTRY_SIZE * max_entry - PAGE_SUFFIX_SIZE;
    diff= 0;
    /* End points to entry before 'rownr' */
    for (dir= buff + end_of_found_block ; dir <= end ; dir+= DIR_ENTRY_SIZE)
    {
      uint offset= uint2korr(dir);
      uint row_length;
      uint row_end;
      if (!offset)
        continue;
      row_length= uint2korr(dir + 2);
      row_end= offset + row_length;
      DBUG_ASSERT(offset >= start_of_found_block &&
                  row_end <= next_free_pos && row_length >= min_row_length);

      if (min_read_from && (buff[offset] & ROW_FLAG_TRANSID))
      {
        TrID transid= transid_korr(buff + offset+1);
        if (transid < min_read_from)
        {
          /* Remove transid from row */
          buff[offset + TRANSID_SIZE]= buff[offset] & ~ROW_FLAG_TRANSID;
          offset+= TRANSID_SIZE;
          row_length-= TRANSID_SIZE;
          int2store(dir+2, row_length);
        }
        if (unlikely(row_length < min_row_length))
        {
          /*
            This can only happen in the case we compacted transid and
            the row become 'too short'
          */
          uint row_diff= min_row_length - row_length;
          if (next_free_pos < row_end + row_diff)
          {
            /*
              Not enough space for extending next block with enough
              end 0's. Move current data down to get place for them
            */
            uint move_down= row_diff - (next_free_pos - row_end);
            bmove(buff + offset - move_down, buff + offset, row_length);
            offset-= move_down;
          }
          /*
            Extend the next block with 0, which will be part of current
            row when the blocks are joined together later
          */
          bzero(buff + next_free_pos - row_diff, row_diff);
          next_free_pos-= row_diff;
          int2store(dir+2, min_row_length);
        }
        row_end= offset + row_length;
      }

      if (row_end != next_free_pos)
      {
        uint length= (end_of_found_block - next_free_pos);
        if (page_pos != end_of_found_block)
        {
          /* move next block down */
          memmove(buff + page_pos - length, buff + next_free_pos, length);
        }
        page_pos-= length;
        /* next continuous block starts here */
        end_of_found_block= row_end;
        diff= page_pos - row_end;
      }
      int2store(dir, offset + diff);            /* correct current pos */
      next_free_pos= offset;
    }
    if (page_pos != end_of_found_block)
    {
      uint length= (end_of_found_block - next_free_pos);
      memmove(buff + page_pos - length, buff + next_free_pos, length);
      next_free_pos= page_pos- length;
    }

    /* Extend rownr block to cover hole */
    rownr_length= next_free_pos - start_of_found_block;
    int2store(dir+2, rownr_length);
    DBUG_ASSERT(rownr_length >= min_row_length);
  }
  else
  {
    if (extend_block)
    {
      /* Extend last block to cover whole page */
      uint length= ((uint) (dir - buff) - start_of_found_block);
      int2store(dir+2, length);
      DBUG_ASSERT(length >= min_row_length);
    }
    else
    {
      /* Add length gained from freed transaction id's to this page */
      uint length= uint2korr(buff+ EMPTY_SPACE_OFFSET) + freed_size;
      int2store(buff + EMPTY_SPACE_OFFSET, length);
    }
    buff[PAGE_TYPE_OFFSET]&= ~(uchar) PAGE_CAN_BE_COMPACTED;
  }
  check_directory(buff, block_size, min_row_length,
                  extend_block ? 0 : (uint) -1);
  DBUG_EXECUTE("directory", _ma_print_directory(DBUG_FILE, buff, block_size););
  DBUG_VOID_RETURN;
}


/*
  Create an empty tail or head page

  SYNOPSIS
    make_empty_page()
    buff		Page buffer
    block_size		Block size
    page_type		HEAD_PAGE or TAIL_PAGE
    create_dir_entry	TRUE of we should create a directory entry

  NOTES
    EMPTY_SPACE is not updated
*/

static void make_empty_page(MARIA_HA *info, uchar *buff, uint page_type,
                            my_bool create_dir_entry)
{
  uint block_size= info->s->block_size;
  DBUG_ENTER("make_empty_page");

  bzero(buff, PAGE_HEADER_SIZE);

#if !defined(DONT_ZERO_PAGE_BLOCKS) || defined(HAVE_valgrind)
  /*
    We zero the rest of the block to avoid getting old memory information
    to disk and to allow the file to be compressed better if archived.
    The code does not assume the block is zeroed.
  */
  if (page_type != BLOB_PAGE)
    bzero(buff+ PAGE_HEADER_SIZE, block_size - PAGE_HEADER_SIZE);
#endif
  buff[PAGE_TYPE_OFFSET]= (uchar) page_type;
  buff[DIR_COUNT_OFFSET]= (int) create_dir_entry;
  buff[DIR_FREE_OFFSET]=  END_OF_DIR_FREE_LIST;
  if (create_dir_entry)
  {
    /* Create directory entry to point to start of page with size 0 */
    buff+= block_size - PAGE_SUFFIX_SIZE - DIR_ENTRY_SIZE;
    int2store(buff, PAGE_HEADER_SIZE);
    int2store(buff+2, 0);
  }
  DBUG_VOID_RETURN;
}


/*
  Read or initialize new head or tail page

  SYNOPSIS
    get_head_or_tail_page()
    info                        Maria handler
    block                       Block to read
    buff                        Suggest this buffer to key cache
    length                      Minimum space needed
    page_type			HEAD_PAGE || TAIL_PAGE
    res                         Store result position here

  NOTES
    We don't decremented buff[EMPTY_SPACE_OFFSET] with the allocated data
    as we don't know how much data the caller will actually use.

    res->empty_space is set to length of empty space

  RETURN
    0  ok     All slots in 'res' are updated
    1  error  my_errno is set
*/

struct st_row_pos_info
{
  uchar *buff;                                  /* page buffer */
  uchar *data;                                  /* Place for data */
  uchar *dir;                                   /* Directory */
  uint length;                                  /* Length for data */
  uint rownr;                                   /* Offset in directory */
  uint empty_space;                             /* Space left on page */
};


static my_bool get_head_or_tail_page(MARIA_HA *info,
                                     const MARIA_BITMAP_BLOCK *block,
                                     uchar *buff, uint length, uint page_type,
                                     enum pagecache_page_lock lock,
                                     struct st_row_pos_info *res)
{
  uint block_size;
  MARIA_PINNED_PAGE page_link;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("get_head_or_tail_page");
  DBUG_PRINT("enter", ("page_type: %u  length: %u", page_type, length));

  block_size= share->block_size;
  if (block->org_bitmap_value == 0)             /* Empty block */
  {
    /* New page */
    make_empty_page(info, buff, page_type, 1);
    res->buff= buff;
    res->empty_space= res->length= (block_size - PAGE_OVERHEAD_SIZE);
    res->data= (buff + PAGE_HEADER_SIZE);
    res->dir= res->data + res->length;
    res->rownr= 0;
    DBUG_ASSERT(length <= res->length);
  }
  else
  {
    uchar *dir;
    /* Read old page */
    page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
    res->buff= pagecache_read(share->pagecache, &info->dfile,
                              block->page, 0, 0, share->page_type,
                              lock, &page_link.link);
    page_link.changed= res->buff != 0;
    push_dynamic(&info->pinned_pages, (void*) &page_link);
    if (!page_link.changed)
      goto crashed;

    DBUG_ASSERT((uint) (res->buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) ==
                page_type);
    if (!(dir= find_free_position(page_type == HEAD_PAGE ? info : 0,
                                  res->buff, block_size, &res->rownr,
                                  &res->length, &res->empty_space)))
      goto crashed;

    if (res->length < length)
    {
      if (res->empty_space + res->length >= length)
      {
        _ma_compact_block_page(res->buff, block_size, res->rownr, 1,
                               (page_type == HEAD_PAGE ?
                                info->trn->min_read_from : 0),
                               (page_type == HEAD_PAGE ?
                                share->base.min_block_length :
                                0));
        /* All empty space are now after current position */
        dir= dir_entry_pos(res->buff, block_size, res->rownr);
        res->length= res->empty_space= uint2korr(dir+2);
      }
      if (res->length < length)
      {
        DBUG_PRINT("error", ("length: %u  res->length: %u  empty_space: %u",
                             length, res->length, res->empty_space));
        goto crashed;                         /* Wrong bitmap information */
      }
    }
    res->dir= dir;
    res->data= res->buff + uint2korr(dir);
  }
  DBUG_RETURN(0);

crashed:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);  /* File crashed */
  DBUG_RETURN(1);
}


/*
  @brief Create room for a head or tail row on a given page at given position

  @fn get_rowpos_in_head_or_tail_page()
  @param info                        Maria handler
  @param block                       Block to read
  @param buff                        Suggest this buffer to key cache
  @param length                      Minimum space needed
  @param page_type	             HEAD_PAGE || TAIL_PAGE
  @param rownr			     Rownr to use
  @param res                         Store result position here

  @note
    This is essential same as get_head_or_tail_page, with the difference
    that the caller species at what position the row should be put.
    This is used when restoring a row to it's original position as
    part of UNDO DELETE or UNDO UPDATE

  @return
  @retval 0  ok     All slots in 'res' are updated
  @retval 1  error  my_errno is set
*/

static my_bool get_rowpos_in_head_or_tail_page(MARIA_HA *info,
                                               const MARIA_BITMAP_BLOCK *block,
                                               uchar *buff, uint length,
                                               uint page_type,
                                               enum pagecache_page_lock lock,
                                               uint rownr,
                                               struct st_row_pos_info *res)
{
  MARIA_PINNED_PAGE page_link;
  MARIA_SHARE *share= info->s;
  uchar *dir;
  uint block_size= share->block_size;
  uint max_entry, max_length, rec_offset;
  DBUG_ENTER("get_rowpos_in_head_or_tail_page");

  if (block->org_bitmap_value == 0)             /* Empty block */
  {
    /* New page */
    make_empty_page(info, buff, page_type, 0);
    res->empty_space= block_size - PAGE_HEADER_SIZE - PAGE_SUFFIX_SIZE;
  }
  else
  {
    page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
    buff= pagecache_read(share->pagecache, &info->dfile,
                         block->page, 0, 0, share->page_type,
                         lock, &page_link.link);
    page_link.changed= buff != 0;
    push_dynamic(&info->pinned_pages, (void*) &page_link);
    if (!page_link.changed)                     /* Read error */
      goto err;
    DBUG_ASSERT((buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) ==
                (uchar) page_type);
    if ((buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) != (uchar) page_type)
      goto err;
    res->empty_space= uint2korr(buff + EMPTY_SPACE_OFFSET);
  }

  max_entry= (uint) buff[DIR_COUNT_OFFSET];
  if (max_entry <= rownr)
  {
    if (extend_directory(page_type == HEAD_PAGE ? info : 0, buff, block_size,
                         max_entry, rownr, &res->empty_space))
      goto err;
  }

  /*
    The following dir entry is unused in case of insert / update but
    not in case of undo_update / undo_delete
  */
  dir= dir_entry_pos(buff, block_size, rownr);

  if (extend_area_on_page(page_type == HEAD_PAGE ? info : 0, buff, dir,
                          rownr, block_size, length,
                          &res->empty_space, &rec_offset, &max_length))
    goto err;

  res->buff= buff;
  res->rownr= rownr;
  res->dir= dir;
  res->data= buff + rec_offset;
  res->length= length;
  DBUG_RETURN(0);

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);  /* File crashed */
  DBUG_RETURN(1);
}


/*
  Write tail for head data or blob

  SYNOPSIS
    write_tail()
    info                Maria handler
    block               Block to tail page
    row_part            Data to write to page
    length              Length of data

  NOTES
    block->page_count is updated to the directory offset for the tail
    so that we can store the position in the row extent information

  RETURN
    0  ok
       block->page_count is set to point (dir entry + TAIL_BIT)

    1  error; In this case my_errno is set to the error
*/

static my_bool write_tail(MARIA_HA *info,
                          MARIA_BITMAP_BLOCK *block,
                          uchar *row_part, uint org_length)
{
  MARIA_SHARE *share= info->s;
  MARIA_PINNED_PAGE page_link;
  uint block_size= share->block_size, empty_space, length= org_length;
  struct st_row_pos_info row_pos;
  my_off_t position;
  my_bool res, block_is_read;
  DBUG_ENTER("write_tail");
  DBUG_PRINT("enter", ("page: %lu  length: %u",
                       (ulong) block->page, length));

  info->keyread_buff_used= 1;
  /*
    Don't allocate smaller block than MIN_TAIL_SIZE (we want to give rows
    some place to grow in the future)
  */
  if (length < MIN_TAIL_SIZE)
    length= MIN_TAIL_SIZE;

  if (block->page_count == TAIL_PAGE_COUNT_MARKER)
  {
    /*
      Create new tail
      page will be pinned & locked by get_head_or_tail_page
    */
    if (get_head_or_tail_page(info, block, info->keyread_buff, length,
                              TAIL_PAGE, PAGECACHE_LOCK_WRITE,
                              &row_pos))
      DBUG_RETURN(1);
  }
  else
  {
    /* Write tail on predefined row position */
    if (get_rowpos_in_head_or_tail_page(info, block, info->keyread_buff,
                                        length, TAIL_PAGE,
                                        PAGECACHE_LOCK_WRITE,
                                        block->page_count & ~TAIL_BIT,
                                        &row_pos))
      DBUG_RETURN(1);
  }
  DBUG_PRINT("info", ("tailid: %lu (%lu:%u)",
                      (ulong) ma_recordpos(block->page, row_pos.rownr),
                      (ulong) block->page, row_pos.rownr));

  block_is_read= block->org_bitmap_value != 0;

  memcpy(row_pos.data, row_part, org_length);

  if (share->now_transactional)
  {
    /* Log changes in tail block */
    uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE];
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];
    LSN lsn;

    /*
      Log REDO changes of tail page
      Note that we have to log length, not org_length, to be sure that
      REDO, which doesn't use write_tail, also creates a block of at least
      MIN_TAIL_SIZE
     */
    page_store(log_data + FILEID_STORE_SIZE, block->page);
    dirpos_store(log_data + FILEID_STORE_SIZE + PAGE_STORE_SIZE,
                 row_pos.rownr);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    row_pos.data;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= length;
    if (translog_write_record(&lsn,
                              (block_is_read ? LOGREC_REDO_INSERT_ROW_TAIL :
                               LOGREC_REDO_NEW_ROW_TAIL),
                              info->trn, info,
                              (translog_size_t) (sizeof(log_data) + length),
                              TRANSLOG_INTERNAL_PARTS + 2, log_array,
                              log_data, NULL))
      DBUG_RETURN(1);
  }

  int2store(row_pos.dir + 2, length);
  empty_space= row_pos.empty_space - length;
  int2store(row_pos.buff + EMPTY_SPACE_OFFSET, empty_space);
  block->page_count= row_pos.rownr + TAIL_BIT;
  /*
    If there is less directory entries free than number of possible tails
    we can write for a row, we mark the page full to ensure that we don't
    during _ma_bitmap_find_place() allocate more entries on the tail page
    than it can hold
  */
  block->empty_space= (enough_free_entries(row_pos.buff, share->block_size,
                                           1 + share->base.blobs) ?
                       empty_space : 0);
  /* Keep BLOCKUSED_USE_ORG_BITMAP */
  block->used|= BLOCKUSED_USED | BLOCKUSED_TAIL;

  if (block_is_read)
  {
    /* Current page link is last element in pinned_pages */
    MARIA_PINNED_PAGE *page_link;
    page_link= dynamic_element(&info->pinned_pages,
                               info->pinned_pages.elements-1,
                               MARIA_PINNED_PAGE*);
    pagecache_unlock_by_link(share->pagecache, page_link->link,
                             PAGECACHE_LOCK_WRITE_TO_READ,
                             PAGECACHE_PIN_LEFT_PINNED, LSN_IMPOSSIBLE,
                             LSN_IMPOSSIBLE, 1, FALSE);
    DBUG_ASSERT(page_link->changed);
    page_link->unlock= PAGECACHE_LOCK_READ_UNLOCK;
    res= 0;
  }
  else
  {
    if (!(res= pagecache_write(share->pagecache,
                               &info->dfile, block->page, 0,
                               row_pos.buff,share->page_type,
                               PAGECACHE_LOCK_READ,
                               PAGECACHE_PIN,
                               PAGECACHE_WRITE_DELAY, &page_link.link,
                               LSN_IMPOSSIBLE)))
    {
      DBUG_ASSERT(page_link.link);
      page_link.unlock= PAGECACHE_LOCK_READ_UNLOCK;
      page_link.changed= 1;
      push_dynamic(&info->pinned_pages, (void*) &page_link);
    }

    /* Increase data file size, if extended */
    position= (my_off_t) block->page * block_size;
    if (share->state.state.data_file_length <= position)
    {
      /*
        We are modifying a state member before writing the UNDO; this is a WAL
        violation. But for data_file_length this is ok, as long as we change
        data_file_length after writing any log record (FILE_ID/REDO/UNDO) (see
        collect_tables()).
      */
      _ma_set_share_data_file_length(share, position + block_size);
    }
  }
  DBUG_RETURN(res);
}


/*
  Write full pages

  SYNOPSIS
    write_full_pages()
    info                Maria handler
    lsn			LSN for the undo record
    block               Where to write data
    data                Data to write
    length              Length of data

  NOTES
    Logging of the changes to the full pages are done in the caller
    write_block_record().

  RETURN
    0  ok
    1  error on write
*/

static my_bool write_full_pages(MARIA_HA *info,
                                LSN lsn,
                                MARIA_BITMAP_BLOCK *block,
                                uchar *data, ulong length)
{
  pgcache_page_no_t page;
  MARIA_SHARE *share= info->s;
  uint block_size= share->block_size;
  uint data_size= FULL_PAGE_SIZE(block_size);
  uchar *buff= info->keyread_buff;
  uint page_count, sub_blocks;
  my_off_t position, max_position;
  DBUG_ENTER("write_full_pages");
  DBUG_PRINT("enter", ("length: %lu  page: %lu  page_count: %lu",
                       (ulong) length, (ulong) block->page,
                       (ulong) block->page_count));
  DBUG_ASSERT((block->page_count & TAIL_BIT) == 0);

  info->keyread_buff_used= 1;
  page=       block->page;
  page_count= block->page_count;
  sub_blocks= block->sub_blocks;

  max_position= (my_off_t) (page + page_count) * block_size;

  /* Increase data file size, if extended */

  for (; length; data+= data_size)
  {
    uint copy_length;
    if (!page_count--)
    {
      if (!--sub_blocks)
      {
        _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
        DBUG_RETURN(1);
      }

      block++;
      page= block->page;
      page_count= block->page_count - 1;
      DBUG_PRINT("info", ("page: %lu  page_count: %lu",
                          (ulong) block->page, (ulong) block->page_count));

      position= (page + page_count + 1) * block_size;
      set_if_bigger(max_position, position);
    }
    lsn_store(buff, lsn);
    buff[PAGE_TYPE_OFFSET]= (uchar) BLOB_PAGE;
    copy_length= min(data_size, length);
    memcpy(buff + LSN_SIZE + PAGE_TYPE_SIZE, data, copy_length);
    length-= copy_length;

    /*
      Zero out old information from the block. This removes possible
      sensitive information from the block and also makes the file
      easier to compress and easier to compare after recovery.
    */
    if (copy_length != data_size)
      bzero(buff + block_size - PAGE_SUFFIX_SIZE - (data_size - copy_length),
            (data_size - copy_length) + PAGE_SUFFIX_SIZE);

    if (pagecache_write(share->pagecache,
                        &info->dfile, page, 0,
                        buff, share->page_type,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                        PAGECACHE_PIN_LEFT_UNPINNED,
                        PAGECACHE_WRITE_DELAY,
                        0, info->trn->rec_lsn))
      DBUG_RETURN(1);
    page++;
    DBUG_ASSERT(block->used & BLOCKUSED_USED);
  }
  if (share->state.state.data_file_length < max_position)
    _ma_set_share_data_file_length(share, max_position);
  DBUG_RETURN(0);
}


/*
  Store ranges of full pages in compact format for logging

  SYNOPSIS
    store_page_range()
    to		Store data here
    block       Where pages are to be written
    block_size  block size
    length	Length of data to be written
		Normally this is full pages, except for the last
                tail block that may only partly fit the last page.
    tot_ranges  Add here the number of ranges used

  NOTES
    The format of one entry is:

     Ranges				 SUB_RANGE_SIZE
     Empty bytes at end of last byte     BLOCK_FILLER_SIZE
     For each range
       Page number                       PAGE_STORE_SIZE
       Number of pages			 PAGERANGE_STORE_SIZE

  RETURN
    #  end position for 'to'
*/

static uchar *store_page_range(uchar *to, MARIA_BITMAP_BLOCK *block,
                               uint block_size, ulong length,
                               uint *tot_ranges)
{
  uint data_size= FULL_PAGE_SIZE(block_size);
  ulong pages_left= (length + data_size -1) / data_size;
  uint page_count, ranges, empty_space;
  uchar *to_start;
  DBUG_ENTER("store_page_range");

  to_start= to;
  to+= SUB_RANGE_SIZE;

  /* Store number of unused bytes at last page */
  empty_space= (uint) (pages_left * data_size - length);
  int2store(to, empty_space);
  to+= BLOCK_FILLER_SIZE;

  ranges= 0;
  do
  {
    pgcache_page_no_t page;
    page=       block->page;
    page_count= block->page_count;
    block++;
    if (page_count > pages_left)
      page_count= pages_left;

    page_store(to, page);
    to+= PAGE_STORE_SIZE;
    pagerange_store(to, page_count);
    to+= PAGERANGE_STORE_SIZE;
    ranges++;
  } while ((pages_left-= page_count));
  /* Store number of ranges for this block */
  int2store(to_start, ranges);
  (*tot_ranges)+= ranges;

  DBUG_RETURN(to);
}


/*
  Store packed extent data

  SYNOPSIS
   store_extent_info()
   to				Store first packed data here
   row_extents_second_part	Store rest here
   first_block		        First block to store
   count			Number of blocks

  NOTES
    We don't have to store the position for the head block

    We have to set the START_EXTENT_BIT for every extent where the
    blob will be stored on a page of it's own. We need this in the
    UNDO phase to generate MARIA_BITMAP_BLOCK's for undo-delete and
    undo-update.
*/

static void store_extent_info(uchar *to,
                              uchar *row_extents_second_part,
                              MARIA_BITMAP_BLOCK *first_block,
                              uint count)
{
  MARIA_BITMAP_BLOCK *block, *end_block;
  uint copy_length;
  my_bool first_found= 0;
  DBUG_ENTER("store_extent_info");
  DBUG_PRINT("enter", ("count: %u", count));

  for (block= first_block, end_block= first_block+count ;
       block < end_block; block++)
  {
    /* The following is only false for marker (unused) blocks */
    if (likely(block->used & BLOCKUSED_USED))
    {
      uint page_count= block->page_count;
      DBUG_ASSERT(page_count != 0);
      page_store(to, block->page);
      if (block->sub_blocks)
      {
        /*
          Set a bit so that we later know that this was the first block
          for a blob
        */
        page_count|= START_EXTENT_BIT;
      }
      pagerange_store(to + PAGE_STORE_SIZE, page_count);
      DBUG_DUMP("extent", to, ROW_EXTENT_SIZE);
      to+= ROW_EXTENT_SIZE;
      if (!first_found)
      {
        first_found= 1;
        to= row_extents_second_part;
      }
    }
  }
  copy_length= (count - 1) * ROW_EXTENT_SIZE;
  /*
    In some unlikely cases we have allocated to many blocks. Clear this
    data.
  */
  bzero(to, (size_t) (row_extents_second_part + copy_length - to));
  DBUG_VOID_RETURN;
}


/**
   @brief
   Convert extent info read from file to MARIA_BITMAP_BLOCKS suitable
   for write_block_record

   @note
   In case of blobs, this function marks all the blob pages in the bitmap
   as full pages. The bitmap bits for other pages will be marked
   when write_block_record() calls _ma_bitmap_release_unused().

   This function will be removed in Maria 2.0 when we instead of delete rows
   mark them as deleted and only remove them after commit.

   @return
   @retval 0  ok
   @retval 1  Error (out of memory or disk error changing bitmap) or
              wrong information in extent information
*/

static my_bool extent_to_bitmap_blocks(MARIA_HA *info,
                                       MARIA_BITMAP_BLOCKS *blocks,
                                       pgcache_page_no_t head_page,
                                       uint extent_count,
                                       const uchar *extent_info)
{
  MARIA_BITMAP_BLOCK *block, *start_block;
  MARIA_SHARE *share= info->s;
  uint i, tail_page;
  DBUG_ENTER("extent_to_bitmap_blocks");

  if (allocate_dynamic(&info->bitmap_blocks, extent_count + 2))
    DBUG_RETURN(1);
  block= blocks->block=  dynamic_element(&info->bitmap_blocks, 0,
                                        MARIA_BITMAP_BLOCK*);
  blocks->count= extent_count + 1;
  blocks->tail_page_skipped= blocks->page_skipped= 0;
  block->page= head_page;
  block->page_count= 1;
  block->used= BLOCKUSED_USED | BLOCKUSED_USE_ORG_BITMAP;
  /* Impossible value, will force storage of real value */
  block->org_bitmap_value= 255;

  start_block= block++;
  for (i=0 ;
       i++ < extent_count ;
       block++, extent_info+= ROW_EXTENT_SIZE)
  {
    uint page_count= uint2korr(extent_info + ROW_EXTENT_PAGE_SIZE);
    if (page_count & START_EXTENT_BIT)
    {
      page_count&= ~START_EXTENT_BIT;
      start_block->sub_blocks= (uint) (block - start_block);
      start_block= block;
    }
    block->page= page_korr(extent_info);
    block->page_count= page_count;
    block->sub_blocks= 0;
    if (block->page_count == 0)
    {
      /* Extend allocated but not used by write_block_record() */
      DBUG_ASSERT(block->page == 0);
      /* This is the last block */
      blocks->count= i;
      break;
    }
    if ((tail_page= page_count & TAIL_BIT))
      page_count= 1;

    /* Check if wrong data */
    if (block->page == 0 || page_count == 0 ||
        (block->page + page_count) * share->block_size >
         share->state.state.data_file_length)
    {
      DBUG_PRINT("error", ("page: %lu  page_count: %u  tail: %u  length: %ld  data_length: %ld",
                           (ulong) block->page,
                           (block->page_count & ~TAIL_BIT),
                           (uint) test(block->page_count & TAIL_BIT),
                           (ulong) ((block->page + (page_count & ~TAIL_BIT)) *
                                    share->block_size),
                           (ulong) share->state.state.data_file_length));
      DBUG_RETURN(1);
    }
    if (tail_page)
    {
      block->org_bitmap_value= _ma_bitmap_get_page_bits(info, &share->bitmap,
                                                        block->page);
      block->used= (BLOCKUSED_TAIL | BLOCKUSED_USED |
                    BLOCKUSED_USE_ORG_BITMAP);
    }
    else
    {
      my_bool res;
      pthread_mutex_lock(&share->bitmap.bitmap_lock);
      res= _ma_bitmap_set_full_page_bits(info, &share->bitmap,
                                         block->page, page_count);
      pthread_mutex_unlock(&share->bitmap.bitmap_lock);
      if (res)
        DBUG_RETURN(1);
      block->used= BLOCKUSED_USED;
    }
  }
  start_block->sub_blocks= (uint) (block - start_block);
  DBUG_RETURN(0);
}


/*
  Free regions of pages with logging

  NOTES
    We are removing filler events and tail page events from
    row->extents to get smaller log.

  RETURN
    0   ok
    1   error
*/

static my_bool free_full_pages(MARIA_HA *info, MARIA_ROW *row)
{
  uchar log_data[FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE];
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];
  LSN lsn;
  size_t extents_length;
  uchar *extents= row->extents;
  DBUG_ENTER("free_full_pages");

  if (info->s->now_transactional)
  {
    /* Compact events by removing filler and tail events */
    uchar *new_block= 0;
    uchar *end, *to, *compact_extent_info;
    my_bool res;
    uint extents_count;

    if (!(compact_extent_info= my_alloca(row->extents_count *
                                         ROW_EXTENT_SIZE)))
      DBUG_RETURN(1);

    to= compact_extent_info;
    for (end= extents + row->extents_count * ROW_EXTENT_SIZE ;
         extents < end ;
         extents+= ROW_EXTENT_SIZE)
    {
      uint page_count= uint2korr(extents + ROW_EXTENT_PAGE_SIZE);
      page_count&= ~START_EXTENT_BIT;
      if (! (page_count & TAIL_BIT) && page_count != 0)
      {
        /* Found correct extent */
        if (!new_block)
          new_block= extents;                   /* First extent in range */
        continue;
      }
      /* Found extent to remove, copy everything found so far */
      if (new_block)
      {
        size_t length= (size_t) (extents - new_block);
        memcpy(to, new_block, length);
        to+= length;
        new_block= 0;
      }
    }
    if (new_block)
    {
      size_t length= (size_t) (extents - new_block);
      memcpy(to, new_block, length);
      to+= length;
    }

    if (!unlikely(extents_length= (uint) (to - compact_extent_info)))
    {
      /*
        No ranges. This happens in the rear case when we have a allocated
        place for a blob on a tail page but it did fit into the main page.
      */
      my_afree(compact_extent_info);
      DBUG_RETURN(0);
    }
    extents_count= (uint) (extents_length / ROW_EXTENT_SIZE);
    pagerange_store(log_data + FILEID_STORE_SIZE, extents_count);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    compact_extent_info;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= extents_length;
    res= translog_write_record(&lsn, LOGREC_REDO_FREE_BLOCKS, info->trn,
                               info,
                               (translog_size_t) (sizeof(log_data) +
                                                  extents_length),
                               TRANSLOG_INTERNAL_PARTS + 2, log_array,
                               log_data, NULL);
    my_afree(compact_extent_info);
    if (res)
      DBUG_RETURN(1);
  }

  DBUG_RETURN(_ma_bitmap_free_full_pages(info, row->extents,
                                         row->extents_count));
}


/*
  Free one page range

  NOTES
    This is very similar to free_full_pages()

  RETURN
    0   ok
    1   error
*/

static my_bool free_full_page_range(MARIA_HA *info, pgcache_page_no_t page,
                                    uint count)
{
  my_bool res= 0;
  uint delete_count;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("free_full_page_range");

  delete_count= count;
  if (share->state.state.data_file_length ==
      (page + count) * share->block_size)
  {
    /*
      Don't delete last page from pagecache as this will make the file
      shorter than expected if the last operation extended the file
    */
    delete_count--;
  }
  if (delete_count &&
      pagecache_delete_pages(share->pagecache, &info->dfile,
                             page, delete_count, PAGECACHE_LOCK_WRITE, 1))
    res= 1;

  if (share->now_transactional)
  {
    LSN lsn;
    /** @todo unify log_data's shape with delete_head_or_tail() */
    uchar log_data[FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE +
                   ROW_EXTENT_SIZE];
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    DBUG_ASSERT(info->trn->rec_lsn);
    pagerange_store(log_data + FILEID_STORE_SIZE, 1);
    page_store(log_data + FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE,
              page);
    int2store(log_data + FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE +
              PAGE_STORE_SIZE, count);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);

    if (translog_write_record(&lsn, LOGREC_REDO_FREE_BLOCKS,
                              info->trn, info,
                              (translog_size_t) sizeof(log_data),
                              TRANSLOG_INTERNAL_PARTS + 1, log_array,
                              log_data, NULL))
      res= 1;
  }
  pthread_mutex_lock(&share->bitmap.bitmap_lock);
  if (_ma_bitmap_reset_full_page_bits(info, &share->bitmap, page, count))
    res= 1;
  pthread_mutex_unlock(&share->bitmap.bitmap_lock);
  DBUG_RETURN(res);
}


/**
   @brief Write a record to a (set of) pages

   @fn     write_block_record()
   @param  info            Maria handler
   @param  old_record      Original record in case of update; NULL in case of
                           insert
   @param  record          Record we should write
   @param  row             Statistics about record (calculated by
                           calc_record_size())
   @param  map_blocks      On which pages the record should be stored
   @param  row_pos         Position on head page where to put head part of
                           record
   @param  undo_lsn	   <> LSN_ERROR if we are executing an UNDO
   @param  old_record_checksum Checksum of old_record: ignored if table does
                               not have live checksum; otherwise if
                               old_record==NULL it must be 0.

   @note
     On return all pinned pages are released.

     [page_buff + EMPTY_SPACE_OFFSET] is set to
     row_pos->empty_space - head_length

   @return Operation status
   @retval 0      OK
   @retval 1      Error
*/

static my_bool write_block_record(MARIA_HA *info,
                                  const uchar *old_record,
                                  const uchar *record,
                                  MARIA_ROW *row,
                                  MARIA_BITMAP_BLOCKS *bitmap_blocks,
                                  my_bool head_block_is_read,
                                  struct st_row_pos_info *row_pos,
                                  LSN undo_lsn,
                                  ha_checksum old_record_checksum)
{
  uchar *data, *end_of_data, *tmp_data_used, *tmp_data;
  uchar *row_extents_first_part, *row_extents_second_part;
  uchar *field_length_data;
  uchar *page_buff;
  MARIA_BITMAP_BLOCK *block, *head_block;
  MARIA_SHARE *share= info->s;
  MARIA_COLUMNDEF *column, *end_column;
  MARIA_PINNED_PAGE page_link;
  uint block_size, flag, head_length;
  ulong *blob_lengths;
  my_bool row_extents_in_use, blob_full_pages_exists;
  LSN lsn;
  my_off_t position;
  uint save_my_errno;
  DBUG_ENTER("write_block_record");

  LINT_INIT(row_extents_first_part);
  LINT_INIT(row_extents_second_part);

  head_block= bitmap_blocks->block;
  block_size= share->block_size;

  page_buff= row_pos->buff;
  /* Position on head page where we should store the head part */
  data= row_pos->data;
  end_of_data= data + row_pos->length;

  /* Write header */
  flag= info->row_flag;
  row_extents_in_use= 0;
  if (unlikely(row->total_length > row_pos->length))
  {
    /* Need extent */
    DBUG_ASSERT(bitmap_blocks->count > 1);
    if (bitmap_blocks->count <= 1)
      goto crashed;                             /* Wrong in bitmap */
    flag|= ROW_FLAG_EXTENTS;
    row_extents_in_use= 1;
  }
  /* For now we have only a minimum header */
  *data++= (uchar) flag;
  if (flag & ROW_FLAG_TRANSID)
  {
    transid_store(data, info->trn->trid);
    data+= TRANSID_SIZE;
  }

  if (unlikely(flag & ROW_FLAG_NULLS_EXTENDED))
    *data++= (uchar) (share->base.null_bytes -
                      share->base.original_null_bytes);
  if (row_extents_in_use)
  {
    /* Store first extent in header */
    store_key_length_inc(data, bitmap_blocks->count - 1);
    row_extents_first_part= data;
    data+= ROW_EXTENT_SIZE;
  }
  if (share->base.max_field_lengths)
    store_key_length_inc(data, row->field_lengths_length);
  if (share->calc_checksum)
  {
    *(data++)= (uchar) (row->checksum); /* store least significant byte */
    DBUG_ASSERT(!((old_record_checksum != 0) && (old_record == NULL)));
  }
  memcpy(data, record, share->base.null_bytes);
  data+= share->base.null_bytes;
  memcpy(data, row->empty_bits, share->base.pack_bytes);
  data+= share->base.pack_bytes;

  DBUG_ASSERT(row_extents_in_use || undo_lsn != LSN_ERROR ||
              (uint) (data - row_pos->data) == row->min_length);

  /*
    Allocate a buffer of rest of data (except blobs)

    To avoid double copying of data, we copy as many columns that fits into
    the page. The rest goes into info->packed_row.

    Using an extra buffer, instead of doing continuous writes to different
    pages, uses less code and we don't need to have to do a complex call
    for every data segment we want to store.
  */
  if (_ma_alloc_buffer(&info->rec_buff, &info->rec_buff_size,
                       row->head_length))
    DBUG_RETURN(1);

  tmp_data_used= 0;                 /* Either 0 or last used uchar in 'data' */
  tmp_data= data;

  if (row_extents_in_use)
  {
    uint copy_length= (bitmap_blocks->count - 2) * ROW_EXTENT_SIZE;
    if (!tmp_data_used && tmp_data + copy_length > end_of_data)
    {
      tmp_data_used= tmp_data;
      tmp_data= info->rec_buff;
    }
    row_extents_second_part= tmp_data;
    /*
       We will copy the extents here when we have figured out the tail
       positions.
    */
    tmp_data+= copy_length;
  }

  /* Copy fields that has fixed lengths (primary key etc) */
  for (column= share->columndef,
         end_column= column + share->base.fixed_not_null_fields;
       column < end_column; column++)
  {
    if (!tmp_data_used && tmp_data + column->length > end_of_data)
    {
      tmp_data_used= tmp_data;
      tmp_data= info->rec_buff;
    }
    memcpy(tmp_data, record + column->offset, column->length);
    tmp_data+= column->length;
  }

  /* Copy length of data for variable length fields */
  if (!tmp_data_used && tmp_data + row->field_lengths_length > end_of_data)
  {
    tmp_data_used= tmp_data;
    tmp_data= info->rec_buff;
  }
  field_length_data= row->field_lengths;
  memcpy(tmp_data, field_length_data, row->field_lengths_length);
  tmp_data+= row->field_lengths_length;

  DBUG_ASSERT(row_extents_in_use || undo_lsn != LSN_ERROR ||
              (uint) (tmp_data - row_pos->data) == row->min_length +
              share->base.fixed_not_null_fields_length +
              row->field_lengths_length);

  /* Copy variable length fields and fields with null/zero */
  for (end_column= share->columndef + share->base.fields - share->base.blobs;
       column < end_column ;
       column++)
  {
    const uchar *field_pos;
    ulong length;
    if ((record[column->null_pos] & column->null_bit) ||
        (row->empty_bits[column->empty_pos] & column->empty_bit))
      continue;

    field_pos= record + column->offset;
    switch (column->type) {
    case FIELD_NORMAL:                          /* Fixed length field */
    case FIELD_SKIP_PRESPACE:
    case FIELD_SKIP_ZERO:                       /* Fixed length field */
      length= column->length;
      break;
    case FIELD_SKIP_ENDSPACE:                   /* CHAR */
      /* Char that is space filled */
      if (column->length <= 255)
        length= (uint) (uchar) *field_length_data++;
      else
      {
        length= uint2korr(field_length_data);
        field_length_data+= 2;
      }
      break;
    case FIELD_VARCHAR:
      if (column->length <= 256)
      {
        length= (uint) (uchar) *field_length_data++;
        field_pos++;                            /* Skip length uchar */
      }
      else
      {
        length= uint2korr(field_length_data);
        field_length_data+= 2;
        field_pos+= 2;
      }
      DBUG_ASSERT(length <= column->length);
      break;
    default:                                    /* Wrong data */
      DBUG_ASSERT(!maria_assert_if_crashed_table);
      length=0;
      break;
    }
    if (!tmp_data_used && tmp_data + length > end_of_data)
    {
      /* Data didn't fit in page; Change to use tmp buffer */
      tmp_data_used= tmp_data;
      tmp_data= info->rec_buff;
    }
    memcpy((char*) tmp_data, field_pos, length);
    tmp_data+= length;
  }

  block= head_block + head_block->sub_blocks;   /* Point to first blob data */

  end_column= column + share->base.blobs;
  blob_lengths= row->blob_lengths;
  if (!tmp_data_used)
  {
    /* Still room on page; Copy as many blobs we can into this page */
    data= tmp_data;
    for (; column < end_column &&
           *blob_lengths <= (ulong)(end_of_data - data);
         column++, blob_lengths++)
    {
      uchar *tmp_pos;
      uint length;
      if (!*blob_lengths)                       /* Null or "" */
        continue;
      length= column->length - portable_sizeof_char_ptr;
      memcpy_fixed((uchar*) &tmp_pos, record + column->offset + length,
                   sizeof(char*));
      memcpy(data, tmp_pos, *blob_lengths);
      data+= *blob_lengths;
      /*
        The following is not true when we want to insert data into original
        place. In this case we don't have any extra blocks allocated
      */
      if (likely(undo_lsn == LSN_ERROR))
      {
        /* Skip over tail page that was prepared for storing blob */
        block++;
        bitmap_blocks->tail_page_skipped= 1;
      }
    }
    if (head_block->sub_blocks > 1)
    {
      /* We have allocated pages that where not used */
      bitmap_blocks->page_skipped= 1;
    }
  }
  else
    data= tmp_data_used;                        /* Get last used on page */

  /* Update page directory */
  head_length= (uint) (data - row_pos->data);
  DBUG_PRINT("info", ("Used head length on page: %u  header_length: %u",
                      head_length,
                      (uint) (flag & ROW_FLAG_TRANSID ? TRANSID_SIZE : 0)));
  if (head_length < share->base.min_block_length)
  {
    /* Extend row to be of size min_block_length */
    uint diff_length= share->base.min_block_length - head_length;
    bzero(data, diff_length);
    data+= diff_length;
    head_length= share->base.min_block_length;
  }
  DBUG_ASSERT(data <= end_of_data);
  /*
    If this is a redo entry (ie, undo_lsn != LSN_ERROR) then we should have
    written exactly head_length bytes (same as original record).
  */
  DBUG_ASSERT(undo_lsn == LSN_ERROR || head_length == row_pos->length);
  int2store(row_pos->dir + 2, head_length);
  /* update empty space at start of block */
  row_pos->empty_space-= head_length;
  int2store(page_buff + EMPTY_SPACE_OFFSET, row_pos->empty_space);
  /* Mark in bitmaps how the current page was actually used */
  head_block->empty_space= row_pos->empty_space;
  if (page_buff[DIR_COUNT_OFFSET] == MAX_ROWS_PER_PAGE &&
      page_buff[DIR_FREE_OFFSET] == END_OF_DIR_FREE_LIST)
    head_block->empty_space= 0;               /* Page is full */
  head_block->used|= BLOCKUSED_USED;

  check_directory(page_buff, share->block_size, share->base.min_block_length,
                  (uint) -1);

  /*
     Now we have to write tail pages, as we need to store the position
     to them in the row extent header.

     We first write out all blob tails, to be able to store them in
     the current page or 'tmp_data'.

     Then we write the tail of the non-blob fields (The position to the
     tail page is stored either in row header, the extents in the head
     page or in the first full page of the non-blob data. It's never in
     the tail page of the non-blob data)
  */

  blob_full_pages_exists= 0;
  if (row_extents_in_use)
  {
    if (column != end_column)                   /* If blob fields */
    {
      MARIA_COLUMNDEF    *save_column=       column;
      MARIA_BITMAP_BLOCK *save_block=        block;
      MARIA_BITMAP_BLOCK *end_block;
      ulong              *save_blob_lengths= blob_lengths;

      for (; column < end_column; column++, blob_lengths++)
      {
        uchar *blob_pos;
        if (!*blob_lengths)                     /* Null or "" */
          continue;
        if (block[block->sub_blocks - 1].used & BLOCKUSED_TAIL)
        {
          uint length;
          length= column->length - portable_sizeof_char_ptr;
          memcpy_fixed((uchar *) &blob_pos, record + column->offset + length,
                       sizeof(char*));
          length= *blob_lengths % FULL_PAGE_SIZE(block_size);   /* tail size */
          if (length != *blob_lengths)
            blob_full_pages_exists= 1;
          if (write_tail(info, block + block->sub_blocks-1,
                         blob_pos + *blob_lengths - length,
                         length))
            goto disk_err;
        }
        else
          blob_full_pages_exists= 1;

        for (end_block= block + block->sub_blocks; block < end_block; block++)
        {
          /*
            Set only a bit, to not cause bitmap code to believe a block is full
            when there is still a lot of entries in it.
          */
          block->used|= BLOCKUSED_USED;
        }
      }
      DBUG_ASSERT((undo_lsn == LSN_ERROR ||
                   block == bitmap_blocks->block + bitmap_blocks->count));
      column= save_column;
      block= save_block;
      blob_lengths= save_blob_lengths;
    }

    if (tmp_data_used)                          /* non blob data overflows */
    {
      MARIA_BITMAP_BLOCK *cur_block, *end_block, *last_head_block;
      MARIA_BITMAP_BLOCK *head_tail_block= 0;
      ulong length;
      ulong data_length= (ulong) (tmp_data - info->rec_buff);

#ifdef SANITY_CHECKS
      DBUG_ASSERT(head_block->sub_blocks != 1);
      if (head_block->sub_blocks == 1)
        goto crashed;                           /* no reserved full or tails */
#endif
      /*
        Find out where to write tail for non-blob fields.

        Problem here is that the bitmap code may have allocated more
        space than we need. We have to handle the following cases:

        - Bitmap code allocated a tail page we don't need.
        - The last full page allocated needs to be changed to a tail page
        (Because we where able to put more data on the head page than
        the bitmap allocation assumed)

        The reserved pages in bitmap_blocks for the main page has one of
        the following allocations:
        - Full pages, with following blocks:
          # * full pages
          empty page  ; To be used if we change last full to tail page. This
          has 'count' = 0.
          tail page  (optional, if last full page was part full)
        - One tail page
      */

      cur_block= head_block + 1;
      end_block= head_block + head_block->sub_blocks;
      /*
        Loop until we have find a block bigger than we need or
        we find the empty page block.
      */
      while (data_length >= (length= (cur_block->page_count *
                                      FULL_PAGE_SIZE(block_size))) &&
             cur_block->page_count)
      {
#ifdef SANITY_CHECKS
        DBUG_ASSERT(!((cur_block == end_block) ||
                      (cur_block->used & BLOCKUSED_USED)));
        if ((cur_block == end_block) || (cur_block->used & BLOCKUSED_USED))
          goto crashed;
#endif
        data_length-= length;
        (cur_block++)->used|= BLOCKUSED_USED;
      }
      last_head_block= cur_block;
      if (data_length)
      {
        if (cur_block->page_count == 0)
        {
          /* Skip empty filler block */
          cur_block++;
        }
#ifdef SANITY_CHECKS
        DBUG_ASSERT(!(cur_block >= end_block));
        if ((cur_block >= end_block))
          goto crashed;
#endif
        if (cur_block->used & BLOCKUSED_TAIL)
        {
          DBUG_ASSERT(data_length < MAX_TAIL_SIZE(block_size));
          /* tail written to tail page */
          cur_block->used|= BLOCKUSED_USED;
          head_tail_block= cur_block;
        }
        else if (data_length > length - MAX_TAIL_SIZE(block_size))
        {
          /* tail written to full page */
          cur_block->used|= BLOCKUSED_USED;
          if ((cur_block != end_block - 1) &&
              (end_block[-1].used & BLOCKUSED_TAIL))
            bitmap_blocks->tail_page_skipped= 1;
        }
        else
        {
          /*
            cur_block is a full block, followed by an empty and optional
            tail block. Change cur_block to a tail block or split it
            into full blocks and tail blocks.

            TODO:
             If there is enough space on the following tail block, use
             this instead of creating a new tail block.
          */
          DBUG_ASSERT(cur_block[1].page_count == 0);
          if (cur_block->page_count == 1)
          {
            /* convert full block to tail block */
            cur_block->used|= BLOCKUSED_USED | BLOCKUSED_TAIL;
            head_tail_block= cur_block;
          }
          else
          {
            DBUG_ASSERT(data_length < length - FULL_PAGE_SIZE(block_size));
            DBUG_PRINT("info", ("Splitting blocks into full and tail"));
            cur_block[1].page= (cur_block->page + cur_block->page_count - 1);
            cur_block[1].page_count= 1;         /* Avoid DBUG_ASSERT */
            cur_block[1].used= BLOCKUSED_USED | BLOCKUSED_TAIL;
            cur_block->page_count--;
            cur_block->used|= BLOCKUSED_USED;
            last_head_block= head_tail_block= cur_block+1;
          }
          if (end_block[-1].used & BLOCKUSED_TAIL)
            bitmap_blocks->tail_page_skipped= 1;
        }
      }
      else
      {
        /* Must be an empty or tail page */
        DBUG_ASSERT(cur_block->page_count == 0 ||
                    cur_block->used & BLOCKUSED_TAIL);
        if (end_block[-1].used & BLOCKUSED_TAIL)
          bitmap_blocks->tail_page_skipped= 1;
      }

      /*
        Write all extents into page or tmp_data

        Note that we still don't have a correct position for the tail
        of the non-blob fields.
      */
      store_extent_info(row_extents_first_part,
                        row_extents_second_part,
                        head_block+1, bitmap_blocks->count - 1);
      if (head_tail_block)
      {
        ulong block_length= (ulong) (tmp_data - info->rec_buff);
        uchar *extent_data;

        length= (uint) (block_length % FULL_PAGE_SIZE(block_size));
        if (write_tail(info, head_tail_block,
                       info->rec_buff + block_length - length,
                       length))
          goto disk_err;
        tmp_data-= length;                      /* Remove the tail */
        if (tmp_data == info->rec_buff)
        {
          /* We have no full blocks to write for the head part */
          tmp_data_used= 0;
        }

        /* Store the tail position for the non-blob fields */
        if (head_tail_block == head_block + 1)
        {
          /*
            We had a head block + tail block, which means that the
            tail block is the first extent
          */
          extent_data= row_extents_first_part;
        }
        else
        {
          /*
            We have a head block + some full blocks + tail block
            last_head_block is pointing after the last used extent
            for the head block.
          */
          extent_data= row_extents_second_part +
            ((last_head_block - head_block) - 2) * ROW_EXTENT_SIZE;
        }
        /* Write information for tail block in the reserved space */
        page_store(extent_data, head_tail_block->page);
        pagerange_store(extent_data + PAGE_STORE_SIZE,
                        head_tail_block->page_count);
      }
    }
    else
      store_extent_info(row_extents_first_part,
                        row_extents_second_part,
                        head_block+1, bitmap_blocks->count - 1);
  }

  if (share->now_transactional)
  {
    uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE];
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];

    /* Log REDO changes of head page */
    page_store(log_data + FILEID_STORE_SIZE, head_block->page);
    dirpos_store(log_data + FILEID_STORE_SIZE + PAGE_STORE_SIZE,
                 row_pos->rownr);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    row_pos->data;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= head_length;
    if (translog_write_record(&lsn,
                              head_block_is_read ?
                              LOGREC_REDO_INSERT_ROW_HEAD :
                              LOGREC_REDO_NEW_ROW_HEAD,
                              info->trn,
                              info,
                              (translog_size_t) (sizeof(log_data) +
                                                 head_length),
                              TRANSLOG_INTERNAL_PARTS + 2, log_array,
                              log_data, NULL))
      goto disk_err;
  }

#ifdef RECOVERY_EXTRA_DEBUG
  if (info->trn->undo_lsn != LSN_IMPOSSIBLE)
  {
    /* Stop right after the REDO; testing incomplete log record groups */
    DBUG_EXECUTE_IF("maria_flush_whole_log",
                    {
                      DBUG_PRINT("maria_flush_whole_log", ("now"));
                      translog_flush(translog_get_horizon());
                    });
    DBUG_EXECUTE_IF("maria_crash",
                    { DBUG_PRINT("maria_crash", ("now")); DBUG_ABORT(); });
  }
#endif

  if (head_block_is_read)
  {
    MARIA_PINNED_PAGE *page_link;
    /* Head page is always the first pinned page */
    page_link= dynamic_element(&info->pinned_pages, 0,
                               MARIA_PINNED_PAGE*);
    pagecache_unlock_by_link(share->pagecache, page_link->link,
                             PAGECACHE_LOCK_WRITE_TO_READ,
                             PAGECACHE_PIN_LEFT_PINNED, LSN_IMPOSSIBLE,
                             LSN_IMPOSSIBLE, 1, FALSE);
    page_link->unlock= PAGECACHE_LOCK_READ_UNLOCK;
    page_link->changed= 1;
  }
  else
  {
    if (pagecache_write(share->pagecache,
                        &info->dfile, head_block->page, 0,
                        page_buff, share->page_type,
                        head_block_is_read ? PAGECACHE_LOCK_WRITE_TO_READ :
                        PAGECACHE_LOCK_READ,
                        head_block_is_read ? PAGECACHE_PIN_LEFT_PINNED :
                        PAGECACHE_PIN,
                        PAGECACHE_WRITE_DELAY, &page_link.link,
                        LSN_IMPOSSIBLE))
      goto disk_err;
    DBUG_ASSERT(page_link.link);
    page_link.unlock= PAGECACHE_LOCK_READ_UNLOCK;
    page_link.changed= 1;
    push_dynamic(&info->pinned_pages, (void*) &page_link);

    /* Increase data file size, if extended */
    position= (my_off_t) head_block->page * block_size;
    if (share->state.state.data_file_length <= position)
      _ma_set_share_data_file_length(share, position + block_size);
  }

  if (share->now_transactional && (tmp_data_used || blob_full_pages_exists))
  {
    /*
      Log REDO writes for all full pages (head part and all blobs)
      We write all here to be able to generate the UNDO record early
      so that we can write the LSN for the UNDO record to all full pages.
    */
    uchar tmp_log_data[FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE +
                       (ROW_EXTENT_SIZE + BLOCK_FILLER_SIZE + SUB_RANGE_SIZE) *
                       ROW_EXTENTS_ON_STACK];
    uchar *log_data, *log_pos;
    LEX_CUSTRING tmp_log_array[TRANSLOG_INTERNAL_PARTS + 2 +
                               ROW_EXTENTS_ON_STACK];
    LEX_CUSTRING *log_array_pos, *log_array;
    int error;
    translog_size_t log_entry_length= 0;
    uint ext_length, extents= 0, sub_extents= 0;

    /* If few extents, then allocate things on stack to avoid a malloc call */
    if (bitmap_blocks->count < ROW_EXTENTS_ON_STACK)
    {
      log_array= tmp_log_array;
      log_data= tmp_log_data;
    }
    else
    {
      if (!my_multi_malloc(MY_WME, &log_array,
                          (uint) ((bitmap_blocks->count +
                                   TRANSLOG_INTERNAL_PARTS + 2) *
                                  sizeof(*log_array)),
                          &log_data, FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE +
                          bitmap_blocks->count * (ROW_EXTENT_SIZE +
                                                  BLOCK_FILLER_SIZE +
                                                  SUB_RANGE_SIZE),
                          NullS))
        goto disk_err;
    }
    log_pos= log_data + FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE * 2;
    log_array_pos= log_array+ TRANSLOG_INTERNAL_PARTS+1;

    if (tmp_data_used)
    {
      /* Full head page */
      translog_size_t block_length= (translog_size_t) (tmp_data -
                                                       info->rec_buff);
      log_pos= store_page_range(log_pos, head_block+1, block_size,
                                (ulong) block_length, &extents);
      log_array_pos->str= info->rec_buff;
      log_array_pos->length= block_length;
      log_entry_length+= block_length;
      log_array_pos++;
      sub_extents++;
    }
    if (blob_full_pages_exists)
    {
      MARIA_COLUMNDEF *tmp_column= column;
      ulong *tmp_blob_lengths= blob_lengths;
      MARIA_BITMAP_BLOCK *tmp_block= block;

      /* Full blob pages */
      for (; tmp_column < end_column; tmp_column++, tmp_blob_lengths++)
      {
        ulong blob_length;
        uint length;

        if (!*tmp_blob_lengths)                 /* Null or "" */
          continue;
        blob_length= *tmp_blob_lengths;
        length= tmp_column->length - portable_sizeof_char_ptr;
        /*
          If last part of blog was on tail page, change blob_length to
          reflect this
        */
        if (tmp_block[tmp_block->sub_blocks - 1].used & BLOCKUSED_TAIL)
          blob_length-= (blob_length % FULL_PAGE_SIZE(block_size));
        if (blob_length)
        {
          memcpy_fixed((uchar*) &log_array_pos->str,
                       record + tmp_column->offset + length,
                       sizeof(uchar*));
          log_array_pos->length= blob_length;
          log_entry_length+= blob_length;
          log_array_pos++;
          sub_extents++;

          log_pos= store_page_range(log_pos, tmp_block, block_size,
                                    blob_length, &extents);
        }
        tmp_block+= tmp_block->sub_blocks;
      }
    }

    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    ext_length=  (uint) (log_pos - log_data);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= ext_length;
    pagerange_store(log_data+ FILEID_STORE_SIZE, extents);
    pagerange_store(log_data+ FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE,
                    sub_extents);

    log_entry_length+= ext_length;
    /* trn->rec_lsn is already set earlier in this function */
    error= translog_write_record(&lsn, LOGREC_REDO_INSERT_ROW_BLOBS,
                                 info->trn, info, log_entry_length,
                                 (uint) (log_array_pos - log_array),
                                 log_array, log_data, NULL);
    if (log_array != tmp_log_array)
      my_free(log_array, MYF(0));
    if (error)
      goto disk_err;
  }

  /* Write UNDO or CLR record */
  lsn= LSN_IMPOSSIBLE;
  if (share->now_transactional)
  {
    LEX_CUSTRING *log_array= info->log_row_parts;

    if (undo_lsn != LSN_ERROR)
    {
      /*
        Store if this CLR is about UNDO_DELETE or UNDO_UPDATE;
        in the first case, Recovery, when it sees the CLR_END in the
        REDO phase, may decrement the records' count.
      */
      if (_ma_write_clr(info, undo_lsn,
                        old_record ? LOGREC_UNDO_ROW_UPDATE :
                        LOGREC_UNDO_ROW_DELETE,
                        share->calc_checksum != 0,
                        row->checksum - old_record_checksum,
                        &lsn, (void*) 0))
        goto disk_err;
    }
    else
    {
      uchar log_data[LSN_STORE_SIZE + FILEID_STORE_SIZE +
                     PAGE_STORE_SIZE + DIRPOS_STORE_SIZE + 2 +
                     HA_CHECKSUM_STORE_SIZE + 2 + PAGERANGE_STORE_SIZE +
                     ROW_EXTENT_SIZE];
      uchar *log_pos;
      ha_checksum checksum_delta;

      /* LOGREC_UNDO_ROW_INSERT & LOGREC_UNDO_ROW_UPDATE share same header */
      lsn_store(log_data, info->trn->undo_lsn);
      page_store(log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE,
                 head_block->page);
      dirpos_store(log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE +
                   PAGE_STORE_SIZE,
                   row_pos->rownr);
      log_pos= (log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE +
                PAGE_STORE_SIZE + DIRPOS_STORE_SIZE);
      store_checksum_in_rec(share, checksum_delta,
                            row->checksum - old_record_checksum,
                            log_pos, log_pos);
      compile_time_assert(sizeof(ha_checksum) == HA_CHECKSUM_STORE_SIZE);

      log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
      log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos -
                                                             log_data);

      if (!old_record)
      {
        /* Store undo_lsn in case we are aborting the insert */
        row->orig_undo_lsn= info->trn->undo_lsn;
        /* Write UNDO log record for the INSERT */
        if (translog_write_record(&lsn, LOGREC_UNDO_ROW_INSERT,
                                  info->trn, info,
                                  (translog_size_t)
                                  log_array[TRANSLOG_INTERNAL_PARTS +
                                            0].length,
                                  TRANSLOG_INTERNAL_PARTS + 1,
                                  log_array,
                                  log_data + LSN_STORE_SIZE, &checksum_delta))
          goto disk_err;
      }
      else
      {
        /* Write UNDO log record for the UPDATE */
        size_t row_length, extents_length;
        uint row_parts_count, cur_head_length;

        /*
          Write head length and extents of the original row so that we
          during UNDO can put it back in the original position.
          We don't store size for TRANSID, as we don't write this during
          UNDO.
        */
        cur_head_length= (info->cur_row.head_length -
                          info->cur_row.header_length);
        int2store(log_pos, cur_head_length);
        pagerange_store(log_pos + 2, info->cur_row.extents_count);
        log_pos+= 2 + PAGERANGE_STORE_SIZE;
        log_array[TRANSLOG_INTERNAL_PARTS + 0].length+= (2 +
                                                         PAGERANGE_STORE_SIZE);
        info->log_row_parts[TRANSLOG_INTERNAL_PARTS+1].str=
          info->cur_row.extents;
        info->log_row_parts[TRANSLOG_INTERNAL_PARTS+1].length=
          extents_length= info->cur_row.extents_count * ROW_EXTENT_SIZE;

        row_length= fill_update_undo_parts(info, old_record, record,
                                           log_array +
                                           TRANSLOG_INTERNAL_PARTS + 2,
                                           &row_parts_count);
        if (translog_write_record(&lsn, LOGREC_UNDO_ROW_UPDATE, info->trn,
                                  info,
                                  (translog_size_t)
                                  (log_array[TRANSLOG_INTERNAL_PARTS +
                                             0].length + extents_length +
                                   row_length),
                                  TRANSLOG_INTERNAL_PARTS + 2 +
                                  row_parts_count,
                                  log_array,
                                  log_data + LSN_STORE_SIZE,
                                  &checksum_delta))
          goto disk_err;
      }
    }
  }
  /* Release not used space in used pages */
  if (_ma_bitmap_release_unused(info, bitmap_blocks))
    goto disk_err;
  _ma_unpin_all_pages(info, lsn);

  if (tmp_data_used)
  {
    /*
      Write data stored in info->rec_buff to pages
      This is the char/varchar data that didn't fit into the head page.
    */
    DBUG_ASSERT(bitmap_blocks->count != 0);
    if (write_full_pages(info, lsn, head_block + 1,
                         info->rec_buff, (ulong) (tmp_data - info->rec_buff)))
      goto disk_err;
  }

  /* Write rest of blobs (data, but no tails as they are already written) */
  for (; column < end_column; column++, blob_lengths++)
  {
    uchar *blob_pos;
    uint length;
    ulong blob_length;
    if (!*blob_lengths)                         /* Null or "" */
      continue;
    length= column->length - portable_sizeof_char_ptr;
    memcpy_fixed((uchar*) &blob_pos, record + column->offset + length,
                 sizeof(char*));
    /* remove tail part */
    blob_length= *blob_lengths;
    if (block[block->sub_blocks - 1].used & BLOCKUSED_TAIL)
      blob_length-= (blob_length % FULL_PAGE_SIZE(block_size));

    if (blob_length && write_full_pages(info, lsn, block,
                                         blob_pos, blob_length))
      goto disk_err;
    block+= block->sub_blocks;
  }

  _ma_finalize_row(info);
  DBUG_RETURN(0);

crashed:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  /* Something was wrong with data on page */
  _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);

disk_err:
  /**
     @todo RECOVERY we are going to let dirty pages go to disk while we have
     logged UNDO, this violates WAL. We must mark the table corrupted!

     @todo RECOVERY we have written some REDOs without a closing UNDO,
     it's possible that a next operation by this transaction succeeds and then
     Recovery would glue the "orphan REDOs" to the succeeded operation and
     execute the failed REDOs. We need some mark "abort this group" in the
     log, or mark the table corrupted (then user will repair it and thus REDOs
     will be skipped).

     @todo RECOVERY to not let write errors go unnoticed, pagecache_write()
     should take a MARIA_HA* in argument, and it it
     fails when flushing a page to disk it should call
     (*the_maria_ha->write_error_func)(the_maria_ha)
     and this hook will mark the table corrupted.
     Maybe hook should be stored in the pagecache's block structure, or in a
     hash "file->maria_ha*".

     @todo RECOVERY we should distinguish below between log write error and
     table write error. The former should stop Maria immediately, the latter
     should mark the table corrupted.
  */
  /*
    Unpin all pinned pages to not cause problems for disk cache. This is
    safe to call even if we already called _ma_unpin_all_pages() above.
  */
  save_my_errno= my_errno;
  _ma_unpin_all_pages_and_finalize_row(info, LSN_IMPOSSIBLE);
  my_errno= save_my_errno;
  DBUG_RETURN(1);
}


/*
  @brief Write a record

  @fn    allocate_and_write_block_record()
  @param info                Maria handler
  @param record              Record to write
  @param row		     Information about fields in 'record'
  @param undo_lsn	     <> LSN_ERROR if we are executing an UNDO

  @return
  @retval 0	ok
  @retval 1	Error
*/

static my_bool allocate_and_write_block_record(MARIA_HA *info,
                                               const uchar *record,
                                               MARIA_ROW *row,
                                               LSN undo_lsn)
{
  struct st_row_pos_info row_pos;
  MARIA_BITMAP_BLOCKS *blocks= &row->insert_blocks;
  int save_my_errno;
  DBUG_ENTER("allocate_and_write_block_record");

  _ma_bitmap_flushable(info, 1);
  if (_ma_bitmap_find_place(info, row, blocks))
    goto err;                         /* Error reading bitmap */

  /*
    Sleep; a checkpoint will happen and should not send this over-allocated
    bitmap to disk but rather wait.
  */
  DBUG_EXECUTE_IF("maria_over_alloc_bitmap", sleep(10););

  /* page will be pinned & locked by get_head_or_tail_page */
  if (get_head_or_tail_page(info, blocks->block, info->buff,
                            max(row->space_on_head_page,
                                info->s->base.min_block_length),
                            HEAD_PAGE,
                            PAGECACHE_LOCK_WRITE, &row_pos))
    goto err;
  row->lastpos= ma_recordpos(blocks->block->page, row_pos.rownr);
  if (info->s->calc_checksum)
  {
    if (undo_lsn == LSN_ERROR)
      row->checksum= (info->s->calc_checksum)(info, record);
    else
    {
      /* _ma_apply_undo_row_delete() already set row's checksum. Verify it. */
      DBUG_ASSERT(row->checksum == (info->s->calc_checksum)(info, record));
    }
  }
  DBUG_PRINT("info", ("rowid: %lu (%lu:%u) length: %u", (ulong) row->lastpos,
                      (ulong) ma_recordpos_to_page(row->lastpos),
                      ma_recordpos_to_dir_entry(row->lastpos),
                      row_pos.length));
  if (write_block_record(info, (uchar*) 0, record, row,
                         blocks, blocks->block->org_bitmap_value != 0,
                         &row_pos, undo_lsn, 0))
    goto err;
  /* Now let checkpoint happen but don't commit */
  DBUG_EXECUTE_IF("maria_over_alloc_bitmap", sleep(1000););
  DBUG_RETURN(0);

err:
  save_my_errno= my_errno;
  if (info->non_flushable_state)
    _ma_bitmap_flushable(info, -1);
  _ma_unpin_all_pages_and_finalize_row(info, LSN_IMPOSSIBLE);
  my_errno= save_my_errno;
  DBUG_RETURN(1);
}


/*
  Write a record and return rowid for it

  SYNOPSIS
    _ma_write_init_block_record()
    info                Maria handler
    record              Record to write

  NOTES
    This is done BEFORE we write the keys to the row!

  RETURN
    HA_OFFSET_ERROR     Something went wrong
    #                   Rowid for row
*/

MARIA_RECORD_POS _ma_write_init_block_record(MARIA_HA *info,
                                             const uchar *record)
{
  DBUG_ENTER("_ma_write_init_block_record");

  calc_record_size(info, record, &info->cur_row);
  if (allocate_and_write_block_record(info, record,
                                      &info->cur_row, LSN_ERROR))
    DBUG_RETURN(HA_OFFSET_ERROR);
  DBUG_RETURN(info->cur_row.lastpos);
}


/*
  Dummy function for (*info->s->write_record)()

  Nothing to do here, as we already wrote the record in
  _ma_write_init_block_record()
*/

my_bool _ma_write_block_record(MARIA_HA *info __attribute__ ((unused)),
                               const uchar *record __attribute__ ((unused)))
{
  return 0;                                     /* Row already written */
}


/**
   @brief Remove row written by _ma_write_block_record() and log undo

   @param  info            Maria handler

   @note
     This is called in case we got a duplicate unique key while
     writing keys.

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

my_bool _ma_write_abort_block_record(MARIA_HA *info)
{
  my_bool res= 0;
  MARIA_BITMAP_BLOCKS *blocks= &info->cur_row.insert_blocks;
  MARIA_BITMAP_BLOCK *block, *end;
  LSN lsn= LSN_IMPOSSIBLE;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_write_abort_block_record");

  _ma_bitmap_lock(share);  /* Lock bitmap from other insert threads */
  if (delete_head_or_tail(info,
                          ma_recordpos_to_page(info->cur_row.lastpos),
                          ma_recordpos_to_dir_entry(info->cur_row.lastpos), 1,
                          0))
    res= 1;
  for (block= blocks->block + 1, end= block + blocks->count - 1; block < end;
       block++)
  {
    if (block->used & BLOCKUSED_USED)
    {
      if (block->used & BLOCKUSED_TAIL)
      {
        /*
          block->page_count is set to the tail directory entry number in
          write_block_record()
        */
        if (delete_head_or_tail(info, block->page,
                                block->page_count & ~TAIL_BIT,
                                0, 0))
          res= 1;
      }
      else
      {
        if (free_full_page_range(info, block->page, block->page_count))
          res= 1;
      }
    }
  }
  _ma_bitmap_unlock(share);
  if (share->now_transactional)
  {
    if (_ma_write_clr(info, info->cur_row.orig_undo_lsn,
                      LOGREC_UNDO_ROW_INSERT,
                      share->calc_checksum != 0,
                      (ha_checksum) 0 - info->cur_row.checksum,
                      &lsn, (void*) 0))
      res= 1;
  }
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res);
}


/*
  Update a record

  NOTES
    For the moment, we assume that info->curr_row.extents is always updated
    when a row is read. In the future we may decide to read this on demand
    for rows split into many extents.
*/

static my_bool _ma_update_block_record2(MARIA_HA *info,
                                        MARIA_RECORD_POS record_pos,
                                        const uchar *oldrec,
                                        const uchar *record,
                                        LSN undo_lsn)
{
  MARIA_BITMAP_BLOCKS *blocks= &info->cur_row.insert_blocks;
  uchar *buff;
  MARIA_ROW *cur_row= &info->cur_row, *new_row= &info->new_row;
  MARIA_PINNED_PAGE page_link;
  uint rownr, org_empty_size, head_length;
  uint block_size= info->s->block_size;
  uint errpos __attribute__((unused)) = 0;
  uchar *dir;
  pgcache_page_no_t page;
  struct st_row_pos_info row_pos;
  my_bool res;
  ha_checksum old_checksum;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_update_block_record2");
  DBUG_PRINT("enter", ("rowid: %lu", (long) record_pos));

#ifdef ENABLE_IF_PROBLEM_WITH_UPDATE
  DBUG_DUMP("oldrec", oldrec, share->base.reclength);
  DBUG_DUMP("newrec", record, share->base.reclength);
#endif

  /*
    Checksums of new and old rows were computed by callers already; new
    row's was put into cur_row, old row's was put into new_row.
  */
  old_checksum= new_row->checksum;
  new_row->checksum= cur_row->checksum;
  calc_record_size(info, record, new_row);
  page= ma_recordpos_to_page(record_pos);

  _ma_bitmap_flushable(info, 1);
  buff= pagecache_read(share->pagecache,
                       &info->dfile, (pgcache_page_no_t) page, 0, 0,
                       share->page_type,
                       PAGECACHE_LOCK_WRITE, &page_link.link);
  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= buff != 0;
  push_dynamic(&info->pinned_pages, (void*) &page_link);
  if (!buff)
    goto err;

  org_empty_size= uint2korr(buff + EMPTY_SPACE_OFFSET);
  rownr= ma_recordpos_to_dir_entry(record_pos);
  dir= dir_entry_pos(buff, block_size, rownr);

  /*
    We can't use cur_row->head_length as the block may have been compacted
    since we read it.
  */
  head_length= uint2korr(dir + 2);

  if ((org_empty_size + head_length) >= new_row->total_length)
  {
    uint rec_offset, length;
    MARIA_BITMAP_BLOCK block;

    DBUG_PRINT("info", ("org_empty_size: %u  org_length: %u  new_length: %lu",
                        org_empty_size, head_length,
                        new_row->total_length));

    /*
      We can fit the new row in the same page as the original head part
      of the row
    */
    block.org_bitmap_value= _ma_free_size_to_head_pattern(&share->bitmap,
                                                          org_empty_size);
    if (extend_area_on_page(info, buff, dir, rownr, block_size,
                            new_row->total_length, &org_empty_size,
                            &rec_offset, &length))
    {
      errpos= 1;
      goto err;
    }

    row_pos.buff= buff;
    row_pos.rownr= rownr;
    row_pos.empty_space= org_empty_size;
    row_pos.dir= dir;
    row_pos.data= buff + rec_offset;
    row_pos.length= length;
    blocks->block= &block;
    blocks->count= 1;
    block.page= page;
    block.sub_blocks= 1;
    block.used= BLOCKUSED_USED | BLOCKUSED_USE_ORG_BITMAP;
    block.empty_space= row_pos.empty_space;

    if (*cur_row->tail_positions &&
        delete_tails(info, cur_row->tail_positions))
    {
      errpos= 2;
      goto err;
    }
    if (cur_row->extents_count && free_full_pages(info, cur_row))
    {
      errpos= 3;
      goto err;
    }
    res= write_block_record(info, oldrec, record, new_row, blocks,
                            1, &row_pos, undo_lsn, old_checksum);
    /* We can't update or delete this without re-reading it again */
    info->update&= ~HA_STATE_AKTIV;
    DBUG_RETURN(res);
  }
  /* Delete old row */
  if (*cur_row->tail_positions &&
      delete_tails(info, cur_row->tail_positions))
  {
    errpos= 4;
    goto err;
  }
  if (cur_row->extents_count && free_full_pages(info, cur_row))
  {
    errpos= 5;
    goto err;
  }

  head_length= uint2korr(dir + 2);
  if (_ma_bitmap_find_new_place(info, new_row, page, head_length +
                                org_empty_size, blocks))
  {
    errpos= 6;
    goto err;
  }

  /*
    Allocate all size in block for record
    TODO:
    Need to improve this to do compact if we can fit one more blob into
    the head page
  */
  if ((head_length < new_row->space_on_head_page ||
       (new_row->total_length <= head_length &&
        org_empty_size + head_length >= new_row->total_length)))
  {
    _ma_compact_block_page(buff, block_size, rownr, 1,
                           info->trn->min_read_from,
                           share->base.min_block_length);
    org_empty_size= 0;
    head_length= uint2korr(dir + 2);
  }

  row_pos.buff= buff;
  row_pos.rownr= rownr;
  row_pos.empty_space= org_empty_size + head_length;
  row_pos.dir= dir;
  row_pos.data= buff + uint2korr(dir);
  row_pos.length= head_length;
  if ((res= write_block_record(info, oldrec, record, new_row, blocks, 1,
                               &row_pos, undo_lsn, old_checksum)))
  {
    errpos= 7;
    goto err;
  }
  DBUG_RETURN(0);

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  DBUG_PRINT("error", ("errpos: %d", errpos));
  if (info->non_flushable_state)
    _ma_bitmap_flushable(info, -1);
  _ma_unpin_all_pages_and_finalize_row(info, LSN_IMPOSSIBLE);
  DBUG_RETURN(1);
}


/*
  @brief Store new row on it's original position

  @note
  This is basicly a copy of _ma_update_block_record2
  When we have a purge thread for deleted row, we can remove this function
  and use _ma_update_block_record2 instead.

  This is the main reason we don't make a lot of subfunctions that are
  common between _ma_update_block_record2() and this function.

  Note: If something goes wrong we mark the file crashed
*/

static my_bool _ma_update_at_original_place(MARIA_HA *info,
                                            pgcache_page_no_t page,
                                            uint rownr,
                                            uint length_on_head_page,
                                            uint extent_count,
                                            const uchar *extent_info,
                                            const uchar *oldrec,
                                            const uchar *record,
                                            LSN undo_lsn)
{
  MARIA_BITMAP_BLOCKS *blocks;
  MARIA_BITMAP_BLOCK *block;
  MARIA_ROW *cur_row= &info->cur_row, *new_row= &info->new_row;
  MARIA_PINNED_PAGE page_link;
  MARIA_SHARE *share= info->s;
  ha_checksum old_checksum;
  uint org_empty_size, empty_size;
  uint block_size= info->s->block_size;
  uchar *dir, *buff;
  struct st_row_pos_info row_pos;
  my_bool res;
  uint rec_offset, length;
  DBUG_ENTER("_ma_update_at_original_place");

#ifdef ENABLE_IF_PROBLEM_WITH_UPDATE
  DBUG_DUMP("oldrec", oldrec, share->base.reclength);
  DBUG_DUMP("newrec", record, share->base.reclength);
#endif

  /*
    Checksums of new and old rows were computed by callers already; new
    row's was put into cur_row, old row's was put into new_row.
  */
  old_checksum= new_row->checksum;
  new_row->checksum= cur_row->checksum;
  calc_record_size(info, record, new_row);

  _ma_bitmap_flushable(info, 1);
  buff= pagecache_read(share->pagecache,
                       &info->dfile, (pgcache_page_no_t) page, 0, 0,
                       share->page_type,
                       PAGECACHE_LOCK_WRITE, &page_link.link);
  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= buff != 0;
  push_dynamic(&info->pinned_pages, (void*) &page_link);
  if (!buff)
    goto err;

  org_empty_size= uint2korr(buff + EMPTY_SPACE_OFFSET);
  dir= dir_entry_pos(buff, block_size, rownr);

  if ((org_empty_size + cur_row->head_length) < length_on_head_page)
  {
    DBUG_PRINT("error",
               ("org_empty_size: %u  head_length: %u  length_on_page: %u",
                org_empty_size, (uint) cur_row->head_length,
                length_on_head_page));
    _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
    goto err;
  }

  /*
    We can fit the new row in the same page as the original head part
    of the row
  */
  empty_size= org_empty_size;
  if (extend_area_on_page(info, buff, dir, rownr, block_size,
                          length_on_head_page, &empty_size,
                          &rec_offset, &length))
    goto err;

  row_pos.buff= buff;
  row_pos.rownr= rownr;
  row_pos.empty_space= empty_size;
  row_pos.dir= dir;
  row_pos.data= buff + rec_offset;

  /* Delete old row */
  if (*cur_row->tail_positions &&
      delete_tails(info, cur_row->tail_positions))
    goto err;
  if (cur_row->extents_count && free_full_pages(info, cur_row))
    goto err;

  /* Change extent information to be usable by write_block_record() */
  blocks= &cur_row->insert_blocks;
  if (extent_to_bitmap_blocks(info, blocks, page, extent_count, extent_info))
    goto err;
  block= blocks->block;
  block->empty_space= row_pos.empty_space;
  block->org_bitmap_value=
    _ma_free_size_to_head_pattern(&share->bitmap,
                                  (enough_free_entries_on_page(share, buff) ?
                                   org_empty_size : 0));
                            
  DBUG_ASSERT(block->org_bitmap_value ==
              _ma_bitmap_get_page_bits(info, &info->s->bitmap, page));
  block->used|= BLOCKUSED_USE_ORG_BITMAP;

  /*
    We have to use <= below as the new_row may be smaller than the original
    row as the new row doesn't have transaction id
  */

  DBUG_ASSERT(blocks->count > 1 ||
              max(new_row->total_length, share->base.min_block_length) <=
              length_on_head_page);

  /* Store same amount of data on head page as on original page */
  row_pos.length= (length_on_head_page - 
                   (extent_count + 1 - blocks->count) * ROW_EXTENT_SIZE);
  set_if_bigger(row_pos.length, share->base.min_block_length);
  if ((res= write_block_record(info, oldrec, record, new_row, blocks,
                               1, &row_pos, undo_lsn, old_checksum)))
    goto err;
  DBUG_RETURN(0);

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  _ma_mark_file_crashed(share);
  if (info->non_flushable_state)
    _ma_bitmap_flushable(info, -1);
  _ma_unpin_all_pages_and_finalize_row(info, LSN_IMPOSSIBLE);
  DBUG_RETURN(1);
}


/* Wrapper for _ma_update_block_record2() used by ma_update() */

my_bool _ma_update_block_record(MARIA_HA *info, MARIA_RECORD_POS record_pos,
                                const uchar *orig_rec, const uchar *new_rec)
{
  return _ma_update_block_record2(info, record_pos, orig_rec, new_rec,
                                  LSN_ERROR);
}


/*
  Delete a directory entry

  SYNOPSIS
    delete_dir_entry()
    buff		Page buffer
    block_size		Block size
    record_number	Record number to delete
    empty_space		Empty space on page after delete

  RETURN
    -1    Error on page
    0     ok
    1     Page is now empty
*/

static int delete_dir_entry(uchar *buff, uint block_size, uint record_number,
                            uint *empty_space_res)
{
  uint number_of_records= (uint) buff[DIR_COUNT_OFFSET];
  uint length, empty_space;
  uchar *dir;
  DBUG_ENTER("delete_dir_entry");

#ifdef SANITY_CHECKS
  if (record_number >= number_of_records ||
      record_number > ((block_size - LSN_SIZE - PAGE_TYPE_SIZE - 1 -
                        PAGE_SUFFIX_SIZE) / DIR_ENTRY_SIZE))
  {
    DBUG_PRINT("error", ("record_number: %u  number_of_records: %u",
                         record_number, number_of_records));

    DBUG_RETURN(-1);
  }
#endif

  check_directory(buff, block_size, 0, (uint) -1);
  empty_space= uint2korr(buff + EMPTY_SPACE_OFFSET);
  dir= dir_entry_pos(buff, block_size, record_number);
  length= uint2korr(dir + 2);

  if (record_number == number_of_records - 1)
  {
    /* Delete this entry and all following free directory entries */
    uchar *end= buff + block_size - PAGE_SUFFIX_SIZE;
    number_of_records--;
    dir+= DIR_ENTRY_SIZE;
    empty_space+= DIR_ENTRY_SIZE;

    /* Unlink and free the next empty ones */
    while (dir < end && dir[0] == 0 && dir[1] == 0)
    {
      number_of_records--;
      if (dir[2] == END_OF_DIR_FREE_LIST)
        buff[DIR_FREE_OFFSET]= dir[3];
      else
      {
        uchar *prev_entry= dir_entry_pos(buff, block_size, (uint) dir[2]);
        DBUG_ASSERT(uint2korr(prev_entry) == 0 && prev_entry[3] ==
                    number_of_records);
        prev_entry[3]= dir[3];
      }
      if (dir[3] != END_OF_DIR_FREE_LIST)
      {
        uchar *next_entry= dir_entry_pos(buff, block_size, (uint) dir[3]);
        DBUG_ASSERT(uint2korr(next_entry) == 0 && next_entry[2] ==
                    number_of_records);
        next_entry[2]= dir[2];
      }
      dir+= DIR_ENTRY_SIZE;
      empty_space+= DIR_ENTRY_SIZE;
    }

    if (number_of_records == 0)
    {
      /* All entries on page deleted */
      DBUG_PRINT("info", ("Page marked as unallocated"));
      buff[PAGE_TYPE_OFFSET]= UNALLOCATED_PAGE;
#ifdef IDENTICAL_PAGES_AFTER_RECOVERY
      {
        dir= dir_entry_pos(buff, block_size, record_number);
        bzero(dir, (record_number+1) * DIR_ENTRY_SIZE);
      }
#endif
      *empty_space_res= block_size;
      DBUG_RETURN(1);
    }
    buff[DIR_COUNT_OFFSET]= (uchar) number_of_records;
  }
  else
  {
    /* Update directory */
    dir[0]= dir[1]= 0;
    dir[2]= END_OF_DIR_FREE_LIST;
    if ((dir[3]= buff[DIR_FREE_OFFSET]) != END_OF_DIR_FREE_LIST)
    {
      /* Relink next entry to point to newly freed entry */
      uchar *next_entry= dir_entry_pos(buff, block_size, (uint) dir[3]);
      DBUG_ASSERT(uint2korr(next_entry) == 0 &&
                  next_entry[2] == END_OF_DIR_FREE_LIST);
      next_entry[2]= record_number;
    }
    buff[DIR_FREE_OFFSET]= record_number;
  }
  empty_space+= length;

  int2store(buff + EMPTY_SPACE_OFFSET, empty_space);
  buff[PAGE_TYPE_OFFSET]|= (uchar) PAGE_CAN_BE_COMPACTED;

  *empty_space_res= empty_space;

  check_directory(buff, block_size, 0, empty_space);
  DBUG_RETURN(0);
}


/*
  Delete a head a tail part

  SYNOPSIS
    delete_head_or_tail()
    info                Maria handler
    page                Page (not file offset!) on which the row is
    head                1 if this is a head page
    from_update		1 if we are called from update. In this case we
			leave the page as write locked as we may put
                        the new row into the old position.

  RETURN
    0  ok
    1  error
*/

static my_bool delete_head_or_tail(MARIA_HA *info,
                                   pgcache_page_no_t page, uint record_number,
                                   my_bool head, my_bool from_update)
{
  MARIA_SHARE *share= info->s;
  uint empty_space;
  int res;
  my_bool page_is_empty;
  uchar *buff;
  LSN lsn;
  MARIA_PINNED_PAGE page_link;
  enum pagecache_page_lock lock_at_write, lock_at_unpin;
  DBUG_ENTER("delete_head_or_tail");
  DBUG_PRINT("enter", ("id: %lu (%lu:%u)",
                       (ulong) ma_recordpos(page, record_number),
                       (ulong) page, record_number));

  buff= pagecache_read(share->pagecache,
                       &info->dfile, page, 0, 0,
                       share->page_type,
                       PAGECACHE_LOCK_WRITE, &page_link.link);
  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= buff != 0;
  push_dynamic(&info->pinned_pages, (void*) &page_link);
  if (!buff)
    DBUG_RETURN(1);
  DBUG_ASSERT((buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) ==
              (head ? HEAD_PAGE : TAIL_PAGE));

  if (from_update)
  {
    lock_at_write= PAGECACHE_LOCK_LEFT_WRITELOCKED;
    lock_at_unpin= PAGECACHE_LOCK_WRITE_UNLOCK;
  }
  else
  {
    lock_at_write= PAGECACHE_LOCK_WRITE_TO_READ;
    lock_at_unpin= PAGECACHE_LOCK_READ_UNLOCK;
  }

  res= delete_dir_entry(buff, share->block_size, record_number, &empty_space);
  if (res < 0)
    DBUG_RETURN(1);
  if (res == 0) /* after our deletion, page is still not empty */
  {
    uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE];
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    page_is_empty= 0;
    if (share->now_transactional)
    {
      /* Log REDO data */
      page_store(log_data + FILEID_STORE_SIZE, page);
      dirpos_store(log_data + FILEID_STORE_SIZE + PAGE_STORE_SIZE,
                   record_number);

      log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
      log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
      if (translog_write_record(&lsn, (head ? LOGREC_REDO_PURGE_ROW_HEAD :
                                       LOGREC_REDO_PURGE_ROW_TAIL),
                                info->trn, info,
                                (translog_size_t) sizeof(log_data),
                                TRANSLOG_INTERNAL_PARTS + 1, log_array,
                                log_data, NULL))
        DBUG_RETURN(1);
    }
  }
  else /* page is now empty */
  {
    page_is_empty= 1;
    if (share->now_transactional)
    {
      uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE];
      LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
      page_store(log_data + FILEID_STORE_SIZE, page);
      log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
      log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
      if (translog_write_record(&lsn, LOGREC_REDO_FREE_HEAD_OR_TAIL,
                                info->trn, info,
                                (translog_size_t) sizeof(log_data),
                                TRANSLOG_INTERNAL_PARTS + 1, log_array,
                                log_data, NULL))
        DBUG_RETURN(1);
    }
    /*
      Mark that this page must be written to disk by page cache, even
      if we could call pagecache_delete() on it.
      This is needed to ensure that repair finds the empty page on disk
      and not old data.
    */
    pagecache_set_write_on_delete_by_link(page_link.link);
    DBUG_ASSERT(empty_space >= share->bitmap.sizes[0]);
  }

  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           lock_at_write,
                           PAGECACHE_PIN_LEFT_PINNED, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 1, FALSE);
  page_link.unlock= lock_at_unpin;
  set_dynamic(&info->pinned_pages, (void*) &page_link,
              info->pinned_pages.elements-1);

  DBUG_PRINT("info", ("empty_space: %u", empty_space));

  /*
    If there is not enough space for all possible tails, mark the
    page full
  */
  if (!head && !page_is_empty && !enough_free_entries(buff, share->block_size,
                                                      1 + share->base.blobs))
    empty_space= 0;

  DBUG_RETURN(_ma_bitmap_set(info, page, head, empty_space));
}


/*
  delete all tails

  SYNOPSIS
    delete_tails()
    info                Handler
    tails               Pointer to vector of tail positions, ending with 0

  RETURN
    0  ok
    1  error
*/

static my_bool delete_tails(MARIA_HA *info, MARIA_RECORD_POS *tails)
{
  my_bool res= 0;
  DBUG_ENTER("delete_tails");
  for (; *tails; tails++)
  {
    if (delete_head_or_tail(info,
                            ma_recordpos_to_page(*tails),
                            ma_recordpos_to_dir_entry(*tails), 0, 1))
      res= 1;
  }
  DBUG_RETURN(res);
}


/*
  Delete a record

  NOTES
   For the moment, we assume that info->cur_row.extents is always updated
   when a row is read. In the future we may decide to read this on demand
   for rows with many splits.
*/

my_bool _ma_delete_block_record(MARIA_HA *info, const uchar *record)
{
  pgcache_page_no_t page;
  uint record_number;
  MARIA_SHARE *share= info->s;
  LSN lsn= LSN_IMPOSSIBLE;
  DBUG_ENTER("_ma_delete_block_record");

  page=          ma_recordpos_to_page(info->cur_row.lastpos);
  record_number= ma_recordpos_to_dir_entry(info->cur_row.lastpos);
  DBUG_PRINT("enter", ("rowid: %lu (%lu:%u)", (ulong) info->cur_row.lastpos,
                       (ulong) page, record_number));

  _ma_bitmap_flushable(info, 1);
  if (delete_head_or_tail(info, page, record_number, 1, 0) ||
      delete_tails(info, info->cur_row.tail_positions))
    goto err;

  if (info->cur_row.extents_count && free_full_pages(info, &info->cur_row))
    goto err;

  if (share->now_transactional)
  {
    uchar log_data[LSN_STORE_SIZE + FILEID_STORE_SIZE + PAGE_STORE_SIZE +
                   DIRPOS_STORE_SIZE + 2 + PAGERANGE_STORE_SIZE +
                   HA_CHECKSUM_STORE_SIZE];
    uchar *log_pos;
    size_t row_length;
    uint row_parts_count, extents_length;
    ha_checksum checksum_delta;

    /* Write UNDO record */
    lsn_store(log_data, info->trn->undo_lsn);
    page_store(log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE, page);
    log_pos= log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE + PAGE_STORE_SIZE;
    dirpos_store(log_pos, record_number);
    log_pos+= DIRPOS_STORE_SIZE;
    int2store(log_pos, info->cur_row.head_length -
              info->cur_row.header_length);
    log_pos+= 2;
    pagerange_store(log_pos, info->cur_row.extents_count);
    log_pos+= PAGERANGE_STORE_SIZE;

    info->log_row_parts[TRANSLOG_INTERNAL_PARTS].str= log_data;
    info->log_row_parts[TRANSLOG_INTERNAL_PARTS].length=
      sizeof(log_data) - HA_CHECKSUM_STORE_SIZE;
    store_checksum_in_rec(share, checksum_delta,
                          (ha_checksum) 0 - info->cur_row.checksum, log_pos,
                          info->log_row_parts[TRANSLOG_INTERNAL_PARTS +
                                              0].length);
    info->log_row_parts[TRANSLOG_INTERNAL_PARTS+1].str=
      info->cur_row.extents;
    info->log_row_parts[TRANSLOG_INTERNAL_PARTS+1].length=
      extents_length= info->cur_row.extents_count * ROW_EXTENT_SIZE;

    row_length= fill_insert_undo_parts(info, record,
                                       (info->log_row_parts +
                                        TRANSLOG_INTERNAL_PARTS + 2),
                                       &row_parts_count);

    if (translog_write_record(&lsn, LOGREC_UNDO_ROW_DELETE, info->trn,
                              info,
                              (translog_size_t)
                              (info->log_row_parts[TRANSLOG_INTERNAL_PARTS +
                                                   0].length + row_length +
                               extents_length),
                              TRANSLOG_INTERNAL_PARTS + 2 + row_parts_count,
                              info->log_row_parts,
                              log_data + LSN_STORE_SIZE,
                              &checksum_delta))
      goto err;
  }

  _ma_bitmap_flushable(info, -1);
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(0);

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  _ma_bitmap_flushable(info, -1);
  _ma_unpin_all_pages_and_finalize_row(info, LSN_IMPOSSIBLE);
  DBUG_RETURN(1);
}


/****************************************************************************
  Reading of records
****************************************************************************/

/*
  Read position to record from record directory at end of page

  SYNOPSIS
   get_record_position()
   buff                 page buffer
   block_size           block size for page
   record_number        Record number in index
   end_of_data          pointer to end of data for record

  RETURN
    0  Error in data
    #  Pointer to start of record.
       In this case *end_of_data is set.
*/

static uchar *get_record_position(uchar *buff, uint block_size,
                                 uint record_number, uchar **end_of_data)
{
  uint number_of_records= (uint) buff[DIR_COUNT_OFFSET];
  uchar *dir;
  uchar *data;
  uint offset, length;

#ifdef SANITY_CHECKS
  if (record_number >= number_of_records ||
      record_number > ((block_size - PAGE_HEADER_SIZE - PAGE_SUFFIX_SIZE) /
                       DIR_ENTRY_SIZE))
  {
    DBUG_PRINT("error",
               ("Wrong row number: record_number: %u  number_of_records: %u",
                record_number, number_of_records));
    return 0;
  }
#endif

  dir= dir_entry_pos(buff, block_size, record_number);
  offset= uint2korr(dir);
  length= uint2korr(dir + 2);
#ifdef SANITY_CHECKS
  if (offset < PAGE_HEADER_SIZE ||
      offset + length > (block_size -
                         number_of_records * DIR_ENTRY_SIZE -
                         PAGE_SUFFIX_SIZE))
  {
    DBUG_PRINT("error",
               ("Wrong row position:  record_number: %u  offset: %u  "
                "length: %u  number_of_records: %u",
                record_number, offset, length, number_of_records));
    return 0;
  }
#endif
  data= buff + offset;
  *end_of_data= data + length;
  return data;
}


/*
  Init extent

  NOTES
    extent is a cursor over which pages to read
*/

static void init_extent(MARIA_EXTENT_CURSOR *extent, uchar *extent_info,
                        uint extents, MARIA_RECORD_POS *tail_positions)
{
  uint page_count;
  extent->extent=       extent_info;
  extent->extent_count= extents;
  extent->page=         page_korr(extent_info);         /* First extent */
  page_count=           (uint2korr(extent_info + ROW_EXTENT_PAGE_SIZE) &
                         ~START_EXTENT_BIT);
  extent->tail=         page_count & TAIL_BIT;
  if (extent->tail)
  {
    extent->page_count=   1;
    extent->tail_row_nr=  page_count & ~TAIL_BIT;
  }
  else
    extent->page_count=   page_count;
  extent->tail_positions= tail_positions;
  extent->lock_for_tail_pages= PAGECACHE_LOCK_LEFT_UNLOCKED;
}


/*
  Read next extent

  SYNOPSIS
    read_next_extent()
    info                Maria handler
    extent              Pointer to current extent (this is updated to point
                        to next)
    end_of_data         Pointer to end of data in read block (out)

  NOTES
    New block is read into info->buff

  RETURN
    0   Error;  my_errno is set
    #   Pointer to start of data in read block
        In this case end_of_data is updated to point to end of data.
*/

static uchar *read_next_extent(MARIA_HA *info, MARIA_EXTENT_CURSOR *extent,
                              uchar **end_of_data)
{
  MARIA_SHARE *share= info->s;
  uchar *buff, *data;
  MARIA_PINNED_PAGE page_link;
  enum pagecache_page_lock lock;
  DBUG_ENTER("read_next_extent");

  if (!extent->page_count)
  {
    uint page_count;
    if (!--extent->extent_count)
      goto crashed;
    extent->extent+=    ROW_EXTENT_SIZE;
    extent->page=       page_korr(extent->extent);
    page_count=         (uint2korr(extent->extent+ROW_EXTENT_PAGE_SIZE) &
                         ~START_EXTENT_BIT);
    if (!page_count)
      goto crashed;
    extent->tail=       page_count & TAIL_BIT;
    if (extent->tail)
      extent->tail_row_nr= page_count & ~TAIL_BIT;
    else
      extent->page_count= page_count;
    DBUG_PRINT("info",("New extent.  Page: %lu  page_count: %u  tail_flag: %d",
                       (ulong) extent->page, extent->page_count,
                       extent->tail != 0));
  }
  extent->first_extent= 0;

  lock= PAGECACHE_LOCK_LEFT_UNLOCKED;
  if (extent->tail)
    lock= extent->lock_for_tail_pages;

  buff= pagecache_read(share->pagecache,
                       &info->dfile, extent->page, 0,
                       info->buff, share->page_type,
                       lock, &page_link.link);
  if (lock != PAGECACHE_LOCK_LEFT_UNLOCKED)
  {
    /* Read during UNDO */
    page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
    page_link.changed= buff != 0;
    push_dynamic(&info->pinned_pages, (void*) &page_link);
  }
  if (!buff)
  {
    /* check if we tried to read over end of file (ie: bad data in record) */
    if ((extent->page + 1) * share->block_size >
        share->state.state.data_file_length)
      goto crashed;
    DBUG_RETURN(0);
  }

  if (!extent->tail)
  {
    /* Full data page */
    if ((buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) != BLOB_PAGE)
      goto crashed;
    extent->page++;                             /* point to next page */
    extent->page_count--;
    *end_of_data= buff + share->block_size - PAGE_SUFFIX_SIZE;
    info->cur_row.full_page_count++;            /* For maria_chk */
    DBUG_RETURN(extent->data_start= buff + LSN_SIZE + PAGE_TYPE_SIZE);
  }

  /* Found tail */
  if ((buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) != TAIL_PAGE)
    goto crashed;
  *(extent->tail_positions++)= ma_recordpos(extent->page,
                                            extent->tail_row_nr);
  info->cur_row.tail_count++;                   /* For maria_chk */

  if (!(data= get_record_position(buff, share->block_size,
                                  extent->tail_row_nr,
                                  end_of_data)))
    goto crashed;
  extent->data_start= data;
  extent->page_count= 0;                        /* No more data in extent */
  DBUG_RETURN(data);


crashed:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
  DBUG_PRINT("error", ("wrong extent information"));
  DBUG_RETURN(0);
}


/*
  Read data that may be split over many blocks

  SYNOPSIS
    read_long_data()
    info                Maria handler
    to                  Store result string here (this is allocated)
    extent              Pointer to current extent position
    data                Current position in buffer
    end_of_data         End of data in buffer

  NOTES
    When we have to read a new buffer, it's read into info->buff

    This loop is implemented by goto's instead of a for() loop as
    the code is notable smaller and faster this way (and it's not nice
    to jump into a for loop() or into a 'then' clause)

  RETURN
    0   ok
    1   error
*/

static my_bool read_long_data2(MARIA_HA *info, uchar *to, ulong length,
                              MARIA_EXTENT_CURSOR *extent,
                              uchar **data, uchar **end_of_data)
{
  uint left_length= (uint) (*end_of_data - *data);
  DBUG_ENTER("read_long_data2");
  DBUG_PRINT("enter", ("length: %lu  left_length: %u",
                       length, left_length));
  DBUG_ASSERT(*data <= *end_of_data);

  /*
    Fields are never split in middle. This means that if length > rest-of-data
    we should start reading from the next extent.  The reason we may have
    data left on the page is that if the fixed part of the row was less than
    min_block_length the head block was extended to min_block_length.

    This may change in the future, which is why we have the loop written
    the way it's written.
  */
  if (extent->first_extent && length > left_length)
  {
    *end_of_data= *data;
    left_length= 0;
  }

  for(;;)
  {
    if (unlikely(left_length >= length))
    {
      memcpy(to, *data, length);
      (*data)+= length;
      DBUG_PRINT("info", ("left_length: %u", left_length - (uint) length));
      DBUG_RETURN(0);
    }
    memcpy(to, *data, left_length);
    to+= left_length;
    length-= left_length;
    if (!(*data= read_next_extent(info, extent, end_of_data)))
      break;
    left_length= (uint) (*end_of_data - *data);
  }
  DBUG_RETURN(1);
}

static inline my_bool read_long_data(MARIA_HA *info, uchar *to, ulong length,
                              MARIA_EXTENT_CURSOR *extent,
                              uchar **data, uchar **end_of_data)
{
  uint left_length= (uint) (*end_of_data - *data);
  if (likely(left_length >= length))
  {
    memcpy(to, *data, length);
    (*data)+= length;
    return 0;
  }
  return read_long_data2(info, to, length, extent, data, end_of_data);
}


/*
  Read a record from page (helper function for _ma_read_block_record())

  SYNOPSIS
    _ma_read_block_record2()
    info                Maria handler
    record              Store record here
    data                Start of head data for row
    end_of_data         End of data for row

  NOTES
    The head page is already read by caller
    Following data is update in info->cur_row:

    cur_row.head_length is set to size of entry in head block
    cur_row.tail_positions is set to point to all tail blocks
    cur_row.extents points to extents data
    cur_row.extents_counts contains number of extents
    cur_row.empty_bits is set to empty bits
    cur_row.field_lengths contains packed length of all fields
    cur_row.blob_length contains total length of all blobs
    cur_row.checksum contains checksum of read record.

   RETURN
     0  ok
     #  Error code
*/

int _ma_read_block_record2(MARIA_HA *info, uchar *record,
                           uchar *data, uchar *end_of_data)
{
  MARIA_SHARE *share= info->s;
  uchar *UNINIT_VAR(field_length_data), *UNINIT_VAR(blob_buffer), *start_of_data;
  uint flag, null_bytes, cur_null_bytes, row_extents, field_lengths;
  my_bool found_blob= 0;
  MARIA_EXTENT_CURSOR extent;
  MARIA_COLUMNDEF *column, *end_column;
  MARIA_ROW *cur_row= &info->cur_row;
  DBUG_ENTER("_ma_read_block_record2");

  start_of_data= data;
  flag= (uint) (uchar) data[0];
  cur_null_bytes= share->base.original_null_bytes;
  null_bytes=     share->base.null_bytes;
  cur_row->head_length= (uint) (end_of_data - data);
  cur_row->full_page_count= cur_row->tail_count= 0;
  cur_row->blob_length= 0;
  /* Number of bytes in header that we don't need to write during undo */
  cur_row->header_length= total_header_size[(flag & PRECALC_HEADER_BITMASK)]-1;

  if (flag & ROW_FLAG_TRANSID)
  {
    cur_row->trid= transid_korr(data+1);
    if (!info->trn)
    {
      /* File crashed */
      DBUG_ASSERT(!maria_assert_if_crashed_table);
      _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
      DBUG_RETURN(HA_ERR_WRONG_IN_RECORD);
    }
    if (!trnman_can_read_from(info->trn, cur_row->trid))
      DBUG_RETURN(my_errno= HA_ERR_ROW_NOT_VISIBLE);
  }

  /* Skip trans header (for now, until we have MVCC csupport) */
  data+= cur_row->header_length + 1 ;
  if (flag & ROW_FLAG_NULLS_EXTENDED)
    cur_null_bytes+= data[-1];

  row_extents= 0;
  if (flag & ROW_FLAG_EXTENTS)
  {
    uint row_extent_size;
    /*
      Record is split over many data pages.
      Get number of extents and first extent
    */
    get_key_length(row_extents, data);
    cur_row->extents_count= row_extents;
    row_extent_size= row_extents * ROW_EXTENT_SIZE;
    if (cur_row->extents_buffer_length < row_extent_size &&
        _ma_alloc_buffer(&cur_row->extents,
                         &cur_row->extents_buffer_length,
                         row_extent_size))
      DBUG_RETURN(my_errno);
    memcpy(cur_row->extents, data, ROW_EXTENT_SIZE);
    data+= ROW_EXTENT_SIZE;
    init_extent(&extent, cur_row->extents, row_extents,
                cur_row->tail_positions);
  }
  else
  {
    cur_row->extents_count= 0;
    (*cur_row->tail_positions)= 0;
    extent.page_count= 0;
    extent.extent_count= 1;
  }
  extent.first_extent= 1;

  field_lengths= 0;
  if (share->base.max_field_lengths)
  {
    get_key_length(field_lengths, data);
    cur_row->field_lengths_length= field_lengths;
#ifdef SANITY_CHECKS
    if (field_lengths > share->base.max_field_lengths)
      goto err;
#endif
  }

  if (share->calc_checksum)
    cur_row->checksum= (uint) (uchar) *data++;
  /* data now points on null bits */
  memcpy(record, data, cur_null_bytes);
  if (unlikely(cur_null_bytes != null_bytes))
  {
    /*
      This only happens if we have added more NULL columns with
      ALTER TABLE and are fetching an old, not yet modified old row
    */
    bzero(record + cur_null_bytes, (uint) (null_bytes - cur_null_bytes));
  }
  data+= null_bytes;
  /* We copy the empty bits to be able to use them for delete/update */
  memcpy(cur_row->empty_bits, data, share->base.pack_bytes);
  data+= share->base.pack_bytes;

  /* TODO: Use field offsets, instead of just skipping them */
  data+= share->base.field_offsets * FIELD_OFFSET_SIZE;

  /*
    Read row extents (note that first extent was already read into
    cur_row->extents above)
  */
  if (row_extents > 1)
  {
    if (read_long_data(info, cur_row->extents + ROW_EXTENT_SIZE,
                       (row_extents - 1) * ROW_EXTENT_SIZE,
                       &extent, &data, &end_of_data))
      DBUG_RETURN(my_errno);
  }

  /*
    Data now points to start of fixed length field data that can't be null
    or 'empty'. Note that these fields can't be split over blocks.
  */
  for (column= share->columndef,
         end_column= column + share->base.fixed_not_null_fields;
       column < end_column; column++)
  {
    uint column_length= column->length;
    if (data + column_length > end_of_data &&
        !(data= read_next_extent(info, &extent, &end_of_data)))
      goto err;
    memcpy(record + column->offset, data, column_length);
    data+= column_length;
  }

  /* Read array of field lengths. This may be stored in several extents */
  if (field_lengths)
  {
    field_length_data= cur_row->field_lengths;
    if (read_long_data(info, field_length_data, field_lengths, &extent,
                       &data, &end_of_data))
      DBUG_RETURN(my_errno);
  }

  /* Read variable length data. Each of these may be split over many extents */
  for (end_column= share->columndef + share->base.fields;
       column < end_column; column++)
  {
    enum en_fieldtype type= column->type;
    uchar *field_pos= record + column->offset;
    /* First check if field is present in record */
    if ((record[column->null_pos] & column->null_bit) ||
        (cur_row->empty_bits[column->empty_pos] & column->empty_bit))
    {
      bfill(record + column->offset, column->fill_length,
            type == FIELD_SKIP_ENDSPACE ? ' ' : 0);
      continue;
    }
    switch (type) {
    case FIELD_NORMAL:                          /* Fixed length field */
    case FIELD_SKIP_PRESPACE:
    case FIELD_SKIP_ZERO:                       /* Fixed length field */
      if (data + column->length > end_of_data &&
          !(data= read_next_extent(info, &extent, &end_of_data)))
        goto err;
      memcpy(field_pos, data, column->length);
      data+= column->length;
      break;
    case FIELD_SKIP_ENDSPACE:                   /* CHAR */
    {
      /* Char that is space filled */
      uint length;
      if (column->length <= 255)
        length= (uint) (uchar) *field_length_data++;
      else
      {
        length= uint2korr(field_length_data);
        field_length_data+= 2;
      }
#ifdef SANITY_CHECKS
      if (length > column->length)
        goto err;
#endif
      if (read_long_data(info, field_pos, length, &extent, &data,
                         &end_of_data))
        DBUG_RETURN(my_errno);
      bfill(field_pos + length, column->length - length, ' ');
      break;
    }
    case FIELD_VARCHAR:
    {
      ulong length;
      if (column->length <= 256)
      {
        length= (uint) (uchar) (*field_pos++= *field_length_data++);
      }
      else
      {
        length= uint2korr(field_length_data);
        field_pos[0]= field_length_data[0];
        field_pos[1]= field_length_data[1];
        field_pos+= 2;
        field_length_data+= 2;
      }
#ifdef SANITY_CHECKS
      if (length > column->length)
        goto err;
#endif
      if (read_long_data(info, field_pos, length, &extent, &data,
                         &end_of_data))
        DBUG_RETURN(my_errno);
      break;
    }
    case FIELD_BLOB:
    {
      uint column_size_length= column->length - portable_sizeof_char_ptr;
      ulong blob_length= _ma_calc_blob_length(column_size_length,
                                              field_length_data);

      if (!found_blob)
      {
        /* Calculate total length for all blobs */
        ulong blob_lengths= 0;
        uchar *length_data= field_length_data;
        MARIA_COLUMNDEF *blob_field= column;

        found_blob= 1;
        for (; blob_field < end_column; blob_field++)
        {
          uint size_length;
          if ((record[blob_field->null_pos] & blob_field->null_bit) ||
              (cur_row->empty_bits[blob_field->empty_pos] &
               blob_field->empty_bit))
            continue;
          size_length= blob_field->length - portable_sizeof_char_ptr;
          blob_lengths+= _ma_calc_blob_length(size_length, length_data);
          length_data+= size_length;
        }
        cur_row->blob_length= blob_lengths;
        DBUG_PRINT("info", ("Total blob length: %lu", blob_lengths));
        if (_ma_alloc_buffer(&info->blob_buff, &info->blob_buff_size,
                             blob_lengths))
          DBUG_RETURN(my_errno);
        blob_buffer= info->blob_buff;
      }

      memcpy(field_pos, field_length_data, column_size_length);
      memcpy_fixed(field_pos + column_size_length, (uchar *) &blob_buffer,
                   sizeof(char*));
      field_length_data+= column_size_length;

      /*
        After we have read one extent, then each blob is in it's own extent
      */
      if (!extent.first_extent || (ulong) (end_of_data - data) < blob_length)
        end_of_data= data;                      /* Force read of next extent */

      if (read_long_data(info, blob_buffer, blob_length, &extent, &data,
                         &end_of_data))
        DBUG_RETURN(my_errno);
      blob_buffer+= blob_length;
      break;
    }
    default:
#ifdef EXTRA_DEBUG
      DBUG_ASSERT(0);                           /* purecov: deadcode */
#endif
      goto err;
    }
    continue;
  }

  if (row_extents)
  {
    DBUG_PRINT("info", ("Row read:  page_count: %u  extent_count: %u",
                        extent.page_count, extent.extent_count));
    *extent.tail_positions= 0;                  /* End marker */
    if (extent.page_count)
      goto err;
    if (extent.extent_count > 1)
    {
      if (_ma_check_if_zero(extent.extent + ROW_EXTENT_SIZE,
                            (extent.extent_count-1) * ROW_EXTENT_SIZE))
      {
        DBUG_PRINT("error", ("Data in extent is not zero"));
        DBUG_DUMP("extent", extent.extent + ROW_EXTENT_SIZE,
                  (extent.extent_count-1) * ROW_EXTENT_SIZE);
        goto err;
      }
    }
  }
  else
  {
    DBUG_PRINT("info", ("Row read"));
    /*
      data should normally point to end_of_date. The only exception is if
      the row is very short in which case we allocated 'min_block_length' data
      for allowing the row to expand.
    */
    if (data != end_of_data && (uint) (end_of_data - start_of_data) >
        share->base.min_block_length)
      goto err;
  }
#ifdef EXTRA_DEBUG
  if (share->calc_checksum && !info->in_check_table)
  {
    /* Esnure that row checksum is correct */
    DBUG_ASSERT(((share->calc_checksum)(info, record) & 255) ==
                cur_row->checksum);
  }
#endif
  info->update|= HA_STATE_AKTIV;	/* We have an active record */
  DBUG_RETURN(0);

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  /* Something was wrong with data on record */
  DBUG_PRINT("error", ("Found record with wrong data"));
  _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
  DBUG_RETURN(HA_ERR_WRONG_IN_RECORD);
}


/** @brief Read positions to tail blocks and full blocks

  @fn    read_row_extent_info()
  @param info	Handler

  @notes
    This function is a simpler version of _ma_read_block_record2()
    The data about the used pages is stored in info->cur_row.

  @return Status
  @retval 0   ok
  @retval 1   Error. my_errno contains error number
*/

static my_bool read_row_extent_info(MARIA_HA *info, uchar *buff,
                                    uint record_number)
{
  MARIA_SHARE *share= info->s;
  MARIA_EXTENT_CURSOR extent;
  MARIA_RECORD_POS *tail_pos;
  uchar *data, *end_of_data;
  uint flag, row_extents, row_extents_size;
  uint field_lengths __attribute__ ((unused));
  uchar *extents, *end;
  DBUG_ENTER("read_row_extent_info");

  if (!(data= get_record_position(buff, share->block_size,
                                  record_number, &end_of_data)))
    DBUG_RETURN(1);                             /* Wrong in record */

  flag= (uint) (uchar) data[0];
  /* Skip trans header */
  data+= total_header_size[(flag & PRECALC_HEADER_BITMASK)];

  row_extents= 0;
  row_extents_size= 0;
  if (flag & ROW_FLAG_EXTENTS)
  {
    /*
      Record is split over many data pages.
      Get number of extents and first extent
    */
    get_key_length(row_extents, data);
    row_extents_size= row_extents * ROW_EXTENT_SIZE;
    if (info->cur_row.extents_buffer_length < row_extents_size &&
        _ma_alloc_buffer(&info->cur_row.extents,
                         &info->cur_row.extents_buffer_length,
                         row_extents_size))
      DBUG_RETURN(1);
    memcpy(info->cur_row.extents, data, ROW_EXTENT_SIZE);
    data+= ROW_EXTENT_SIZE;
    init_extent(&extent, info->cur_row.extents, row_extents,
                info->cur_row.tail_positions);
    extent.first_extent= 1;
  }
  info->cur_row.extents_count= row_extents;

  /*
    field_lengths looks unused but get_key_length will
    increment data, which is required as data it's used later.
  */
  if (share->base.max_field_lengths)
    get_key_length(field_lengths, data);

  if (share->calc_checksum)
    info->cur_row.checksum= (uint) (uchar) *data++;
  if (row_extents > 1)
  {
    data+= share->base.null_bytes;
    data+= share->base.pack_bytes;
    data+= share->base.field_offsets * FIELD_OFFSET_SIZE;

    /*
      Read row extents (note that first extent was already read into
      info->cur_row.extents above)
      Lock tails with write lock as we will delete them later.
    */
    extent.lock_for_tail_pages= PAGECACHE_LOCK_LEFT_WRITELOCKED;
    if (read_long_data(info, info->cur_row.extents + ROW_EXTENT_SIZE,
                       row_extents_size - ROW_EXTENT_SIZE,
                       &extent, &data, &end_of_data))
      DBUG_RETURN(1);
  }

  /* Update tail_positions with pointer to tails */
  tail_pos= info->cur_row.tail_positions;
  for (extents= info->cur_row.extents, end= extents + row_extents_size;
       extents < end;
       extents+= ROW_EXTENT_SIZE)
  {
    pgcache_page_no_t page=  uint5korr(extents);
    uint page_count= uint2korr(extents + ROW_EXTENT_PAGE_SIZE);
    if (page_count & TAIL_BIT)
      *(tail_pos++)= ma_recordpos(page, (page_count & ~ (TAIL_BIT |
                                                         START_EXTENT_BIT)));
  }
  *tail_pos= 0;                               /* End marker */
  DBUG_RETURN(0);
}


/*
  Read a record based on record position

  @fn     _ma_read_block_record()
  @param info                Maria handler
  @param record              Store record here
  @param record_pos          Record position

  @return Status
  @retval 0  ok
  @retval #  Error number
*/

int _ma_read_block_record(MARIA_HA *info, uchar *record,
                          MARIA_RECORD_POS record_pos)
{
  MARIA_SHARE *share= info->s;
  uchar *data, *end_of_data, *buff;
  uint offset;
  uint block_size= share->block_size;
  int ret;
  DBUG_ENTER("_ma_read_block_record");
  DBUG_PRINT("enter", ("rowid: %lu  page: %lu  rownr: %u",
                       (ulong) record_pos,
                       (ulong) ma_recordpos_to_page(record_pos),
                       ma_recordpos_to_dir_entry(record_pos)));

  offset= ma_recordpos_to_dir_entry(record_pos);

  if (!(buff= pagecache_read(share->pagecache,
                             &info->dfile, ma_recordpos_to_page(record_pos), 0,
                             info->buff, share->page_type,
                             PAGECACHE_LOCK_LEFT_UNLOCKED, 0)))
    DBUG_RETURN(my_errno);
  DBUG_ASSERT((buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) == HEAD_PAGE);
  if (!(data= get_record_position(buff, block_size, offset, &end_of_data)))
  {
    DBUG_ASSERT(!maria_assert_if_crashed_table);
    DBUG_PRINT("error", ("Wrong directory entry in data block"));
    my_errno= HA_ERR_RECORD_DELETED;           /* File crashed */
    DBUG_RETURN(HA_ERR_RECORD_DELETED);
  }
  ret= _ma_read_block_record2(info, record, data, end_of_data);
  DBUG_RETURN(ret);
}


/* compare unique constraint between stored rows */

my_bool _ma_cmp_block_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                             const uchar *record, MARIA_RECORD_POS pos)
{
  uchar *org_rec_buff, *old_record;
  size_t org_rec_buff_size;
  int error;
  DBUG_ENTER("_ma_cmp_block_unique");

  if (!(old_record= my_alloca(info->s->base.reclength)))
    DBUG_RETURN(1);

  /* Don't let the compare destroy blobs that may be in use */
  org_rec_buff=      info->rec_buff;
  org_rec_buff_size= info->rec_buff_size;
  if (info->s->base.blobs)
  {
    /* Force realloc of record buffer*/
    info->rec_buff= 0;
    info->rec_buff_size= 0;
  }
  error= _ma_read_block_record(info, old_record, pos);
  if (!error)
    error= _ma_unique_comp(def, record, old_record, def->null_are_equal);
  if (info->s->base.blobs)
  {
    my_free(info->rec_buff, MYF(MY_ALLOW_ZERO_PTR));
    info->rec_buff=      org_rec_buff;
    info->rec_buff_size= org_rec_buff_size;
  }
  DBUG_PRINT("exit", ("result: %d", error));
  my_afree(old_record);
  DBUG_RETURN(error != 0);
}


/****************************************************************************
  Table scan
****************************************************************************/

/*
  Allocate buffers for table scan

  SYNOPSIS
   _ma_scan_init_block_record(MARIA_HA *info)

  IMPLEMENTATION
    We allocate one buffer for the current bitmap and one buffer for the
    current page

  RETURN
    0  ok
    1  error (couldn't allocate memory or disk error)
*/

my_bool _ma_scan_init_block_record(MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_scan_init_block_record");
  /*
    bitmap_buff may already be allocated if this is the second call to
    rnd_init() without a rnd_end() in between, see sql/handler.h
  */
  if (!(info->scan.bitmap_buff ||
        ((info->scan.bitmap_buff=
          (uchar *) my_malloc(share->block_size * 2, MYF(MY_WME))))))
    DBUG_RETURN(1);
  info->scan.page_buff= info->scan.bitmap_buff + share->block_size;
  info->scan.bitmap_end= info->scan.bitmap_buff + share->bitmap.max_total_size;

  /* Set scan variables to get _ma_scan_block() to start with reading bitmap */
  info->scan.number_of_rows= 0;
  info->scan.bitmap_pos= info->scan.bitmap_end;
  info->scan.bitmap_page= (pgcache_page_no_t) 0 - share->bitmap.pages_covered;
  info->scan.max_page= share->state.state.data_file_length / share->block_size;
  /*
    We need to flush what's in memory (bitmap.map) to page cache otherwise, as
    we are going to read bitmaps from page cache in table scan (see
    _ma_scan_block_record()), we may miss recently inserted rows (bitmap page
    in page cache would be too old).
  */
  DBUG_RETURN(_ma_bitmap_flush(info->s));
}


/* Free buffers allocated by _ma_scan_block_init() */

void _ma_scan_end_block_record(MARIA_HA *info)
{
  DBUG_ENTER("_ma_scan_end_block_record");
  my_free(info->scan.bitmap_buff, MYF(MY_ALLOW_ZERO_PTR));
  info->scan.bitmap_buff= 0;
  if (info->scan_save)
  {
    my_free(info->scan_save, MYF(0));
    info->scan_save= 0;
  }
  DBUG_VOID_RETURN;
}


/**
  @brief Save current scan position

  @note
  For the moment we can only remember one position, but this is
  good enough for MySQL usage

  @Warning
    When this function is called, we assume that the thread is not deleting
    or updating the current row before ma_scan_restore_block_record()
    is called!

  @return
  @retval 0			  ok
  @retval HA_ERR_WRONG_IN_RECORD  Could not allocate memory to hold position
*/

int _ma_scan_remember_block_record(MARIA_HA *info,
                                   MARIA_RECORD_POS *lastpos)
{
  uchar *bitmap_buff;
  DBUG_ENTER("_ma_scan_remember_block_record");
  if (!(info->scan_save))
  {
    if (!(info->scan_save= my_malloc(ALIGN_SIZE(sizeof(*info->scan_save)) +
                                     info->s->block_size * 2,
                                     MYF(MY_WME))))
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    info->scan_save->bitmap_buff= ((uchar*) info->scan_save +
                                   ALIGN_SIZE(sizeof(*info->scan_save)));
  }
  /* Point to the last read row */
  *lastpos= info->cur_row.nextpos - 1;
  info->scan.dir+= DIR_ENTRY_SIZE;

  /* Remember used bitmap and used head page */
  bitmap_buff= info->scan_save->bitmap_buff;
  memcpy(info->scan_save, &info->scan, sizeof(*info->scan_save));
  info->scan_save->bitmap_buff= bitmap_buff;
  memcpy(bitmap_buff, info->scan.bitmap_buff, info->s->block_size * 2);
  DBUG_RETURN(0);
}


/**
   @brief restore scan block it's original values

   @note
   In theory we could swap bitmap buffers instead of copy them.
   For the moment we don't do that because there are variables pointing
   inside the buffers and it's a bit of hassle to either make them relative
   or repoint them.
*/

void _ma_scan_restore_block_record(MARIA_HA *info,
                                   MARIA_RECORD_POS lastpos)
{
  uchar *bitmap_buff;
  DBUG_ENTER("_ma_scan_restore_block_record");

  info->cur_row.nextpos= lastpos;
  bitmap_buff= info->scan.bitmap_buff;
  memcpy(&info->scan, info->scan_save, sizeof(*info->scan_save));
  info->scan.bitmap_buff= bitmap_buff;
  memcpy(bitmap_buff, info->scan_save->bitmap_buff, info->s->block_size * 2);

  DBUG_VOID_RETURN;
}


/*
  Read next record while scanning table

  SYNOPSIS
    _ma_scan_block_record()
    info                Maria handler
    record              Store found here
    record_pos          Value stored in info->cur_row.next_pos after last call
    skip_deleted

  NOTES
    - One must have called mi_scan() before this
    - In this version, we don't actually need record_pos, we as easily
      use a variable in info->scan

  IMPLEMENTATION
    Current code uses a lot of goto's to separate the different kind of
    states we may be in. This gives us a minimum of executed if's for
    the normal cases.  I tried several different ways to code this, but
    the current one was in the end the most readable and fastest.

  RETURN
    0   ok
    #   Error code
*/

int _ma_scan_block_record(MARIA_HA *info, uchar *record,
                          MARIA_RECORD_POS record_pos,
                          my_bool skip_deleted __attribute__ ((unused)))
{
  uint block_size;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_scan_block_record");

restart_record_read:
  /* Find next row in current page */
  while (likely(record_pos < info->scan.number_of_rows))
  {
    uint length, offset;
    uchar *data, *end_of_data;
    int error;

    while (!(offset= uint2korr(info->scan.dir)))
    {
      info->scan.dir-= DIR_ENTRY_SIZE;
      record_pos++;
#ifdef SANITY_CHECKS
      if (info->scan.dir < info->scan.dir_end)
      {
        DBUG_ASSERT(!maria_assert_if_crashed_table);
        goto err;
      }
#endif
    }
    /* found row */
    info->cur_row.lastpos= info->scan.row_base_page + record_pos;
    info->cur_row.nextpos= record_pos + 1;
    data= info->scan.page_buff + offset;
    length= uint2korr(info->scan.dir + 2);
    end_of_data= data + length;
    info->scan.dir-= DIR_ENTRY_SIZE;          /* Point to previous row */
#ifdef SANITY_CHECKS
    if (end_of_data > info->scan.dir_end ||
        offset < PAGE_HEADER_SIZE || length < share->base.min_block_length)
    {
      DBUG_ASSERT(!(end_of_data > info->scan.dir_end));
      DBUG_ASSERT(!(offset < PAGE_HEADER_SIZE));
      DBUG_ASSERT(!(length < share->base.min_block_length));
      goto err;
    }
#endif
    DBUG_PRINT("info", ("rowid: %lu", (ulong) info->cur_row.lastpos));
    error= _ma_read_block_record2(info, record, data, end_of_data);
    if (error != HA_ERR_ROW_NOT_VISIBLE)
      DBUG_RETURN(error);
    record_pos++;
  }

  /* Find next head page in current bitmap */
restart_bitmap_scan:
  block_size= share->block_size;
  if (likely(info->scan.bitmap_pos < info->scan.bitmap_end))
  {
    uchar *data=    info->scan.bitmap_pos;
    longlong bits= info->scan.bits;
    uint bit_pos=  info->scan.bit_pos;

    do
    {
      while (likely(bits))
      {
        uint pattern= (uint) (bits & 7);
        bits >>= 3;
        bit_pos++;
        if (pattern > 0 && pattern <= 4)
        {
          /* Found head page; Read it */
          pgcache_page_no_t page;
          info->scan.bitmap_pos= data;
          info->scan.bits= bits;
          info->scan.bit_pos= bit_pos;
          page= (info->scan.bitmap_page + 1 +
                 (data - info->scan.bitmap_buff) / 6 * 16 + bit_pos - 1);
          info->scan.row_base_page= ma_recordpos(page, 0);
          if (page >= info->scan.max_page)
          {
            DBUG_PRINT("info", ("Found end of file"));
            DBUG_RETURN((my_errno= HA_ERR_END_OF_FILE));
          }
          if (!(pagecache_read(share->pagecache,
                               &info->dfile,
                               page, 0, info->scan.page_buff,
                               share->page_type,
                               PAGECACHE_LOCK_LEFT_UNLOCKED, 0)))
            DBUG_RETURN(my_errno);
          if (((info->scan.page_buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) !=
               HEAD_PAGE))
          {
            /*
              This may happen if someone has been deleting all rows
              from a page since we read the bitmap, so it may be ok.
              Print warning in debug log and continue.
            */
            DBUG_PRINT("warning",
                       ("Found page of type %d when expecting head page",
                        (info->scan.page_buff[PAGE_TYPE_OFFSET] &
                         PAGE_TYPE_MASK)));
            continue;
          }
          if ((info->scan.number_of_rows=
               (uint) (uchar) info->scan.page_buff[DIR_COUNT_OFFSET]) == 0)
          {
            DBUG_PRINT("error", ("Wrong page header"));
            _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
            DBUG_RETURN(HA_ERR_WRONG_IN_RECORD);
          }
          DBUG_PRINT("info", ("Page %lu has %u rows",
                              (ulong) page, info->scan.number_of_rows));
          info->scan.dir= (info->scan.page_buff + block_size -
                           PAGE_SUFFIX_SIZE - DIR_ENTRY_SIZE);
          info->scan.dir_end= (info->scan.dir -
                               (info->scan.number_of_rows - 1) *
                               DIR_ENTRY_SIZE);
          record_pos= 0;
          goto restart_record_read;
        }
      }
      for (data+= 6; data < info->scan.bitmap_end; data+= 6)
      {
        bits= uint6korr(data);
        /* Skip not allocated pages and blob / full tail pages */
        if (bits && bits != LL(07777777777777777))
          break;
      }
      bit_pos= 0;
    } while (data < info->scan.bitmap_end);
  }

  /* Read next bitmap */
  info->scan.bitmap_page+= share->bitmap.pages_covered;
  if (unlikely(info->scan.bitmap_page >= info->scan.max_page))
  {
    DBUG_PRINT("info", ("Found end of file"));
    DBUG_RETURN((my_errno= HA_ERR_END_OF_FILE));
  }
  DBUG_PRINT("info", ("Reading bitmap at %lu",
                      (ulong) info->scan.bitmap_page));
  if (!(pagecache_read(share->pagecache, &info->s->bitmap.file,
                       info->scan.bitmap_page,
                       0, info->scan.bitmap_buff, PAGECACHE_PLAIN_PAGE,
                       PAGECACHE_LOCK_LEFT_UNLOCKED, 0)))
    DBUG_RETURN(my_errno);
  /* Skip scanning 'bits' in bitmap scan code */
  info->scan.bitmap_pos= info->scan.bitmap_buff - 6;
  info->scan.bits= 0;
  goto restart_bitmap_scan;

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  DBUG_PRINT("error", ("Wrong data on page"));
  _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
  DBUG_RETURN(HA_ERR_WRONG_IN_RECORD);
}


/*
  Compare a row against a stored one

  NOTES
    Not implemented, as block record is not supposed to be used in a shared
    global environment
*/

my_bool _ma_compare_block_record(MARIA_HA *info __attribute__ ((unused)),
                                 const uchar *record __attribute__ ((unused)))
{
  return 0;
}


/*
  Store an integer with simple packing

  SYNOPSIS
    ma_store_integer()
    to                  Store the packed integer here
    nr                  Integer to store

  NOTES
    This is mostly used to store field numbers and lengths of strings.
    We have to cast the result for the LL() becasue of a bug in Forte CC
    compiler.

    Packing used is:
    nr < 251 is stored as is (in 1 byte)
    Numbers that require 1-4 bytes are stored as char(250+byte_length), data
    Bigger numbers are stored as 255, data as ulonglong (not yet done).

  RETURN
    Position in 'to' after the packed length
*/

uchar *ma_store_length(uchar *to, ulong nr)
{
  if (nr < 251)
  {
    *to=(uchar) nr;
    return to+1;
  }
  if (nr < 65536)
  {
    if (nr <= 255)
    {
      to[0]= (uchar) 251;
      to[1]= (uchar) nr;
      return to+2;
    }
    to[0]= (uchar) 252;
    int2store(to+1, nr);
    return to+3;
  }
  if (nr < 16777216)
  {
    *to++= (uchar) 253;
    int3store(to, nr);
    return to+3;
  }
  *to++= (uchar) 254;
  int4store(to, nr);
  return to+4;
}


/* Calculate how many bytes needed to store a number */

uint ma_calc_length_for_store_length(ulong nr)
{
  if (nr < 251)
    return 1;
  if (nr < 65536)
  {
    if (nr <= 255)
      return 2;
    return 3;
  }
  if (nr < 16777216)
    return 4;
  return 5;
}


/* Retrive a stored number */

static ulong ma_get_length(const uchar **packet)
{
  reg1 const uchar *pos= *packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (ulong) *pos;
  }
  if (*pos == 251)
  {
    (*packet)+= 2;
    return (ulong) pos[1];
  }
  if (*pos == 252)
  {
    (*packet)+= 3;
    return (ulong) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+= 4;
    return (ulong) uint3korr(pos+1);
  }
  DBUG_ASSERT(*pos == 254);
  (*packet)+= 5;
  return (ulong) uint4korr(pos+1);
}


/*
  Fill array with pointers to field parts to be stored in log for insert

  SYNOPSIS
    fill_insert_undo_parts()
    info                Maria handler
    record              Inserted row
    log_parts           Store pointers to changed memory areas here
    log_parts_count     See RETURN

  NOTES
    We have information in info->cur_row about the read row.

  RETURN
    length of data in log_parts.
    log_parts_count contains number of used log_parts
*/

static size_t fill_insert_undo_parts(MARIA_HA *info, const uchar *record,
                                     LEX_CUSTRING *log_parts,
                                     uint *log_parts_count)
{
  MARIA_SHARE *share= info->s;
  MARIA_COLUMNDEF *column, *end_column;
  uchar *field_lengths= info->cur_row.field_lengths;
  size_t row_length;
  MARIA_ROW *cur_row= &info->cur_row;
  LEX_CUSTRING *start_log_parts;
  DBUG_ENTER("fill_insert_undo_parts");

  start_log_parts= log_parts;

  /* Store null bits */
  log_parts->str=      record;
  log_parts->length=   share->base.null_bytes;
  row_length=          log_parts->length;
  log_parts++;

  /* Stored bitmap over packed (zero length or all-zero fields) */
  log_parts->str= info->cur_row.empty_bits;
  log_parts->length= share->base.pack_bytes;
  row_length+=       log_parts->length;
  log_parts++;

  if (share->base.max_field_lengths)
  {
    /* Store length of all not empty char, varchar and blob fields */
    log_parts->str= field_lengths - 2;
    log_parts->length=   info->cur_row.field_lengths_length+2;
    int2store(log_parts->str, info->cur_row.field_lengths_length);
    row_length+= log_parts->length;
    log_parts++;
  }

  if (share->base.blobs)
  {
    /*
      Store total blob length to make buffer allocation easier during UNDO
     */
    log_parts->str=  info->length_buff;
    log_parts->length= (uint) (ma_store_length(info->length_buff,
                                                 info->cur_row.blob_length) -
                                 (uchar*) log_parts->str);
    row_length+=          log_parts->length;
    log_parts++;
  }

  /* Handle constant length fields that are always present */
  for (column= share->columndef,
       end_column= column+ share->base.fixed_not_null_fields;
       column < end_column;
       column++)
  {
    log_parts->str= record + column->offset;
    log_parts->length= column->length;
    row_length+= log_parts->length;
    log_parts++;
  }

  /* Handle NULL fields and CHAR/VARCHAR fields */
  for (end_column= share->columndef + share->base.fields - share->base.blobs;
       column < end_column;
       column++)
  {
    const uchar *column_pos;
    size_t column_length;
    if ((record[column->null_pos] & column->null_bit) ||
        cur_row->empty_bits[column->empty_pos] & column->empty_bit)
      continue;

    column_pos=    record+ column->offset;
    column_length= column->length;

    switch (column->type) {
    case FIELD_CHECK:
    case FIELD_NORMAL:                          /* Fixed length field */
    case FIELD_ZERO:
    case FIELD_SKIP_PRESPACE:                   /* Not packed */
    case FIELD_SKIP_ZERO:                       /* Fixed length field */
      break;
    case FIELD_SKIP_ENDSPACE:                   /* CHAR */
    {
      if (column->length <= 255)
        column_length= *field_lengths++;
      else
      {
        column_length= uint2korr(field_lengths);
        field_lengths+= 2;
      }
      break;
    }
    case FIELD_VARCHAR:
    {
      if (column->fill_length == 1)
        column_length= *field_lengths;
      else
        column_length= uint2korr(field_lengths);
      field_lengths+= column->fill_length;
      column_pos+= column->fill_length;
      break;
    }
    default:
      DBUG_ASSERT(0);
    }
    log_parts->str= column_pos;
    log_parts->length= column_length;
    row_length+= log_parts->length;
    log_parts++;
  }

  /* Add blobs */
  for (end_column+= share->base.blobs; column < end_column; column++)
  {
    const uchar *field_pos= record + column->offset;
    uint size_length= column->length - portable_sizeof_char_ptr;
    ulong blob_length= _ma_calc_blob_length(size_length, field_pos);

    /*
      We don't have to check for null, as blob_length is guranteed to be 0
      if the blob is null
    */
    if (blob_length)
    {
      uchar *blob_pos;
      memcpy_fixed(&blob_pos, record + column->offset + size_length,
                   sizeof(blob_pos));
      log_parts->str= blob_pos;
      log_parts->length= blob_length;
      row_length+= log_parts->length;
      log_parts++;
    }
  }
  *log_parts_count= (uint) (log_parts - start_log_parts);
  DBUG_RETURN(row_length);
}


/*
   Fill array with pointers to field parts to be stored in log for update

  SYNOPSIS
    fill_update_undo_parts()
    info                Maria handler
    oldrec		Original row
    newrec              New row
    log_parts           Store pointers to changed memory areas here
    log_parts_count     See RETURN

  IMPLEMENTATION
    Format of undo record:

    Fields are stored in same order as the field array.

    Offset to changed field data (packed)

    For each changed field
      Fieldnumber (packed)
      Length, if variable length field (packed)

    For each changed field
     Data

   Packing is using ma_store_integer()

   The reason we store field numbers & length separated from data (ie, not
   after each other) is to get better cpu caching when we loop over
   fields (as we probably don't have to access data for each field when we
   want to read and old row through the undo log record).

   As a special case, we use '255' for the field number of the null bitmap.

  RETURN
    length of data in log_parts.
    log_parts_count contains number of used log_parts
*/

static size_t fill_update_undo_parts(MARIA_HA *info, const uchar *oldrec,
                                     const uchar *newrec,
                                     LEX_CUSTRING *log_parts,
                                     uint *log_parts_count)
{
  MARIA_SHARE *share= info->s;
  MARIA_COLUMNDEF *column, *end_column;
  MARIA_ROW *old_row= &info->cur_row, *new_row= &info->new_row;
  uchar *field_data, *start_field_data, *length_str;
  uchar *old_field_lengths= old_row->field_lengths;
  uchar *new_field_lengths= new_row->field_lengths;
  size_t row_length= 0;
  uint field_lengths;
  LEX_CUSTRING *start_log_parts;
  my_bool new_column_is_empty;
  DBUG_ENTER("fill_update_undo_parts");

  start_log_parts= log_parts;

  /*
    First log part is for number of fields, field numbers and lengths
    The +4 is to reserve place for the number of changed fields.
  */
  start_field_data= field_data= info->update_field_data + 4;
  log_parts++;

  if (memcmp(oldrec, newrec, share->base.null_bytes))
  {
    /* Store changed null bits */
    *field_data++=       (uchar) 255;           /* Special case */
    log_parts->str=      oldrec;
    log_parts->length=   share->base.null_bytes;
    row_length=          log_parts->length;
    log_parts++;
  }

  /* Handle constant length fields */
  for (column= share->columndef,
       end_column= column+ share->base.fixed_not_null_fields;
       column < end_column;
       column++)
  {
    if (memcmp(oldrec + column->offset, newrec + column->offset,
               column->length))
    {
      field_data= ma_store_length(field_data,
                                  (uint) (column - share->columndef));
      log_parts->str= oldrec + column->offset;
      log_parts->length= column->length;
      row_length+=       column->length;
      log_parts++;
    }
  }

  /* Handle the rest: NULL fields and CHAR/VARCHAR fields and BLOB's */
  for (end_column= share->columndef + share->base.fields;
       column < end_column;
       column++)
  {
    const uchar *new_column_pos, *old_column_pos;
    size_t new_column_length, old_column_length;

    /* First check if old column is null or empty */
    if (oldrec[column->null_pos] & column->null_bit)
    {
      /*
        It's safe to skip this one as either the new column is also null
        (no change) or the new_column is not null, in which case the null-bit
        maps differed and we have already stored the null bitmap.
      */
      continue;
    }
    if (old_row->empty_bits[column->empty_pos] & column->empty_bit)
    {
      if (new_row->empty_bits[column->empty_pos] & column->empty_bit)
        continue;                               /* Both are empty; skip */

      /* Store null length column */
      field_data= ma_store_length(field_data,
                                  (uint) (column - share->columndef));
      field_data= ma_store_length(field_data, 0);
      continue;
    }
    /*
      Remember if the 'new' value is empty (as in this case we must always
      log the original value
    */
    new_column_is_empty= ((newrec[column->null_pos] & column->null_bit) ||
                          (new_row->empty_bits[column->empty_pos] &
                           column->empty_bit));

    old_column_pos=      oldrec + column->offset;
    new_column_pos=      newrec + column->offset;
    old_column_length= new_column_length= column->length;

    switch (column->type) {
    case FIELD_CHECK:
    case FIELD_NORMAL:                          /* Fixed length field */
    case FIELD_ZERO:
    case FIELD_SKIP_PRESPACE:                   /* Not packed */
    case FIELD_SKIP_ZERO:                       /* Fixed length field */
      break;
    case FIELD_VARCHAR:
      new_column_length--;                      /* Skip length prefix */
      old_column_pos+= column->fill_length;
      new_column_pos+= column->fill_length;
      /* Fall through */
    case FIELD_SKIP_ENDSPACE:                   /* CHAR */
    {
      if (new_column_length <= 255)
      {
        old_column_length= *old_field_lengths++;
        if (!new_column_is_empty)
          new_column_length= *new_field_lengths++;
      }
      else
      {
        old_column_length= uint2korr(old_field_lengths);
        old_field_lengths+= 2;
        if (!new_column_is_empty)
        {
          new_column_length= uint2korr(new_field_lengths);
          new_field_lengths+= 2;
        }
      }
      break;
    }
    case FIELD_BLOB:
    {
      uint size_length= column->length - portable_sizeof_char_ptr;
      old_column_length= _ma_calc_blob_length(size_length, old_column_pos);
      memcpy_fixed((uchar*) &old_column_pos,
                   oldrec + column->offset + size_length,
                   sizeof(old_column_pos));
      if (!new_column_is_empty)
      {
        new_column_length= _ma_calc_blob_length(size_length, new_column_pos);
        memcpy_fixed((uchar*) &new_column_pos,
                     newrec + column->offset + size_length,
                     sizeof(old_column_pos));
      }
      break;
    }
    default:
      DBUG_ASSERT(0);
    }

    if (new_column_is_empty || new_column_length != old_column_length ||
        memcmp(old_column_pos, new_column_pos, new_column_length))
    {
      field_data= ma_store_length(field_data,
                                  (ulong) (column - share->columndef));
      field_data= ma_store_length(field_data, (ulong) old_column_length);

      log_parts->str=     old_column_pos;
      log_parts->length=  old_column_length;
      row_length+=        old_column_length;
      log_parts++;
    }
  }

  *log_parts_count= (uint) (log_parts - start_log_parts);

  /* Store length of field length data before the field/field_lengths */
  field_lengths= (uint) (field_data - start_field_data);
  length_str= start_field_data - ma_calc_length_for_store_length(field_lengths);
  start_log_parts->str= length_str;
  ma_store_length(length_str, field_lengths);
  start_log_parts->length= (size_t) (field_data - start_log_parts->str);
  row_length+= start_log_parts->length;
  DBUG_RETURN(row_length);
}

/***************************************************************************
  In-write hooks called under log's lock when log record is written
***************************************************************************/

/**
   @brief Sets transaction's rec_lsn if needed

   A transaction sometimes writes a REDO even before the page is in the
   pagecache (example: brand new head or tail pages; full pages). So, if
   Checkpoint happens just after the REDO write, it needs to know that the
   REDO phase must start before this REDO. Scanning the pagecache cannot
   tell that as the page is not in the cache. So, transaction sets its rec_lsn
   to the REDO's LSN or somewhere before, and Checkpoint reads the
   transaction's rec_lsn.

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_redo(enum translog_record_type type
                            __attribute__ ((unused)),
                            TRN *trn, MARIA_HA *tbl_info
                            __attribute__ ((unused)),
                            LSN *lsn, void *hook_arg
                            __attribute__ ((unused)))
{
  /*
    Users of dummy_transaction_object must keep this TRN clean as it
    is used by many threads (like those manipulating non-transactional
    tables). It might be dangerous if one user sets rec_lsn or some other
    member and it is picked up by another user (like putting this rec_lsn into
    a page of a non-transactional table); it's safer if all members stay 0. So
    non-transactional log records (REPAIR, CREATE, RENAME, DROP) should not
    call this hook; we trust them but verify ;)
  */
  DBUG_ASSERT(trn->trid != 0);
  /*
    If the hook stays so simple, it would be faster to pass
    !trn->rec_lsn ? trn->rec_lsn : some_dummy_lsn
    to translog_write_record(), like Monty did in his original code, and not
    have a hook. For now we keep it like this.
  */
  if (trn->rec_lsn == 0)
    trn->rec_lsn= *lsn;
  return 0;
}


/**
   @brief Sets transaction's undo_lsn, first_undo_lsn if needed

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_undo(enum translog_record_type type
                            __attribute__ ((unused)),
                            TRN *trn, MARIA_HA *tbl_info
                            __attribute__ ((unused)),
                            LSN *lsn, void *hook_arg
                            __attribute__ ((unused)))
{
  DBUG_ASSERT(trn->trid != 0);
  trn->undo_lsn= *lsn;
  if (unlikely(LSN_WITH_FLAGS_TO_LSN(trn->first_undo_lsn) == 0))
    trn->first_undo_lsn=
      trn->undo_lsn | LSN_WITH_FLAGS_TO_FLAGS(trn->first_undo_lsn);
  return 0;
  /*
    when we implement purging, we will specialize this hook: UNDO_PURGE
    records will additionally set trn->undo_purge_lsn
  */
}


/**
   @brief Sets the table's records count and checksum and others to 0, then
   calls the generic REDO hook.

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_redo_delete_all(enum translog_record_type type
                                       __attribute__ ((unused)),
                                       TRN *trn, MARIA_HA *tbl_info
                                       __attribute__ ((unused)),
                                       LSN *lsn, void *hook_arg)
{
  _ma_reset_status(tbl_info);
  return write_hook_for_redo(type, trn, tbl_info, lsn, hook_arg);
}


/**
   @brief Updates "records" and "checksum" and calls the generic UNDO hook

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_undo_row_insert(enum translog_record_type type
                                       __attribute__ ((unused)),
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg)
{
  MARIA_SHARE *share= tbl_info->s;
  share->state.state.records++;
  share->state.state.checksum+= *(ha_checksum *)hook_arg;
  return write_hook_for_undo(type, trn, tbl_info, lsn, hook_arg);
}


/**
   @brief Upates "records" and calls the generic UNDO hook

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_undo_row_delete(enum translog_record_type type
                                       __attribute__ ((unused)),
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg)
{
  MARIA_SHARE *share= tbl_info->s;
  share->state.state.records--;
  share->state.state.checksum+= *(ha_checksum *)hook_arg;
  return write_hook_for_undo(type, trn, tbl_info, lsn, hook_arg);
}


/**
   @brief Upates "records" and "checksum" and calls the generic UNDO hook

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_undo_row_update(enum translog_record_type type
                                       __attribute__ ((unused)),
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg)
{
  MARIA_SHARE *share= tbl_info->s;
  share->state.state.checksum+= *(ha_checksum *)hook_arg;
  return write_hook_for_undo(type, trn, tbl_info, lsn, hook_arg);
}


my_bool write_hook_for_undo_bulk_insert(enum translog_record_type type
                                        __attribute__ ((unused)),
                                        TRN *trn, MARIA_HA *tbl_info,
                                        LSN *lsn, void *hook_arg)
{
  /*
    We are going to call maria_delete_all_rows(), but without logging and
    syncing, as an optimization (if we crash before commit, the UNDO will
    empty; if we crash after commit, we have flushed and forced the files).
    Status still needs to be reset under log mutex, in case of a concurrent
    checkpoint.
  */
  _ma_reset_status(tbl_info);
  return write_hook_for_undo(type, trn, tbl_info, lsn, hook_arg);
}


/**
   @brief Updates table's lsn_of_file_id.

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_file_id(enum translog_record_type type
                               __attribute__ ((unused)),
                               TRN *trn
                               __attribute__ ((unused)),
                               MARIA_HA *tbl_info,
                               LSN *lsn,
                               void *hook_arg
                               __attribute__ ((unused)))
{
  DBUG_ASSERT(cmp_translog_addr(tbl_info->s->lsn_of_file_id, *lsn) < 0);
  tbl_info->s->lsn_of_file_id= *lsn;
  return 0;
}


/**
   Updates transaction's rec_lsn when committing.

   A transaction writes its commit record before being committed in trnman, so
   if Checkpoint happens just between the COMMIT record log write and the
   commit in trnman, it will record that transaction is not committed. Assume
   the transaction (trn1) did an INSERT; after the checkpoint, a second
   transaction (trn2) does a DELETE of what trn1 has inserted. Then crash,
   Checkpoint record says that trn1 was not committed, and REDO phase starts
   from Checkpoint record's LSN. So it will not find the COMMIT record of
   trn1, will want to roll back trn1, which will fail because the row/key
   which it wants to delete does not exist anymore.
   To avoid this, Checkpoint needs to know that the REDO phase must start
   before this COMMIT record, so transaction sets its rec_lsn to the COMMIT's
   record LSN, and as Checkpoint reads the transaction's rec_lsn, Checkpoint
   will know.

   @note so after commit trn->rec_lsn is a "commit LSN", which could be of
   use later.

   @return Operation status, always 0 (success)
*/

my_bool write_hook_for_commit(enum translog_record_type type
                              __attribute__ ((unused)),
                              TRN *trn,
                              MARIA_HA *tbl_info
                              __attribute__ ((unused)),
                              LSN *lsn,
                              void *hook_arg
                              __attribute__ ((unused)))
{
  trn->rec_lsn= *lsn;
  return 0;
}


/***************************************************************************
  Applying of REDO log records
***************************************************************************/

/*
  Apply changes to head and tail pages

  SYNOPSIS
    _ma_apply_redo_insert_row_head_or_tail()
    info		Maria handler
    lsn			LSN to put on page
    page_type		HEAD_PAGE or TAIL_PAGE
    new_page		True if this is first entry on page
    header		Header (without FILEID)
    data		Data to be put on page
    data_length		Length of data

  NOTE
    Handles LOGREC_REDO_INSERT_ROW_HEAD, LOGREC_REDO_INSERT_ROW_TAIL
    LOGREC_REDO_NEW_ROW_HEAD and LOGREC_REDO_NEW_ROW_TAIL

  RETURN
    0   ok
    #   Error number
*/

uint _ma_apply_redo_insert_row_head_or_tail(MARIA_HA *info, LSN lsn,
                                            uint page_type,
                                            my_bool new_page,
                                            const uchar *header,
                                            const uchar *data,
                                            size_t data_length)
{
  MARIA_SHARE *share= info->s;
  pgcache_page_no_t page;
  uint      rownr, empty_space;
  uint      block_size= share->block_size;
  uint      rec_offset;
  uchar      *buff, *dir;
  uint      result;
  MARIA_PINNED_PAGE page_link;
  enum pagecache_page_lock unlock_method;
  enum pagecache_page_pin unpin_method;
  my_off_t end_of_page;
  uint error;
  DBUG_ENTER("_ma_apply_redo_insert_row_head_or_tail");

  page=  page_korr(header);
  rownr= dirpos_korr(header + PAGE_STORE_SIZE);

  DBUG_PRINT("enter", ("rowid: %lu  page: %lu  rownr: %u  data_length: %u",
                       (ulong) ma_recordpos(page, rownr),
                       (ulong) page, rownr, (uint) data_length));

  share->state.changed|= (STATE_CHANGED | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);

  end_of_page= (page + 1) * share->block_size;
  if (end_of_page > share->state.state.data_file_length)
  {
    DBUG_PRINT("info", ("Enlarging data file from %lu to %lu",
                        (ulong) share->state.state.data_file_length,
                        (ulong) end_of_page));
    /*
      New page at end of file. Note that the test above is also positive if
      data_file_length is not a multiple of block_size (system crashed while
      writing the last page): in this case we just extend the last page and
      fill it entirely with zeroes, then the REDO will put correct data on
      it.
    */
    unlock_method= PAGECACHE_LOCK_WRITE;
    unpin_method=  PAGECACHE_PIN;

    DBUG_ASSERT(rownr == 0 && new_page);
    if (rownr != 0 || !new_page)
      goto crashed_file;

    buff= info->keyread_buff;
    info->keyread_buff_used= 1;
    make_empty_page(info, buff, page_type, 1);
    empty_space= (block_size - PAGE_OVERHEAD_SIZE);
    rec_offset= PAGE_HEADER_SIZE;
    dir= buff+ block_size - PAGE_SUFFIX_SIZE - DIR_ENTRY_SIZE;
  }
  else
  {
    unlock_method= PAGECACHE_LOCK_LEFT_WRITELOCKED;
    unpin_method=  PAGECACHE_PIN_LEFT_PINNED;

    share->pagecache->readwrite_flags&= ~MY_WME;
    buff= pagecache_read(share->pagecache, &info->dfile,
                         page, 0, 0,
                         PAGECACHE_PLAIN_PAGE, PAGECACHE_LOCK_WRITE,
                         &page_link.link);
    share->pagecache->readwrite_flags= share->pagecache->org_readwrite_flags;
    if (!buff)
    {
      /* Skip errors when reading outside of file and uninitialized pages */
      if (!new_page || (my_errno != HA_ERR_FILE_TOO_SHORT &&
                        my_errno != HA_ERR_WRONG_CRC))
      {
        DBUG_PRINT("error", ("Error %d when reading page", (int) my_errno));
        goto err;
      }
      /* Create new page */
      buff= pagecache_block_link_to_buffer(page_link.link);
      buff[PAGE_TYPE_OFFSET]= UNALLOCATED_PAGE;
    }
    else if (lsn_korr(buff) >= lsn)           /* Test if already applied */
    {
      /* Fix bitmap, just in case */
      empty_space= uint2korr(buff + EMPTY_SPACE_OFFSET);
      if (!enough_free_entries_on_page(share, buff))
        empty_space= 0;                         /* Page is full */

      if (_ma_bitmap_set(info, page, page_type == HEAD_PAGE, empty_space))
        goto err;
      pagecache_unlock_by_link(share->pagecache, page_link.link,
                               PAGECACHE_LOCK_WRITE_UNLOCK,
                               PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                               LSN_IMPOSSIBLE, 0, FALSE);
      DBUG_RETURN(0);
    }

    if (((uint) (buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) != page_type))
    {
      /*
        This is a page that has been freed before and now should be
        changed to new type.
      */
      if (!new_page)
      {
        DBUG_PRINT("error",
                   ("Found page of wrong type: %u, should have been %u",
                    (uint) (buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK),
                    page_type));
        goto crashed_file;
      }
      make_empty_page(info, buff, page_type, 0);
      empty_space= block_size - PAGE_HEADER_SIZE - PAGE_SUFFIX_SIZE;
      (void) extend_directory(page_type == HEAD_PAGE ? info: 0, buff,
                              block_size, 0, rownr, &empty_space);
      rec_offset= PAGE_HEADER_SIZE;
      dir= dir_entry_pos(buff, block_size, rownr);
      empty_space+= uint2korr(dir+2);
    }
    else
    {
      uint max_entry= (uint) buff[DIR_COUNT_OFFSET];
      uint length;

      DBUG_ASSERT(!new_page);
      dir= dir_entry_pos(buff, block_size, rownr);
      empty_space= uint2korr(buff + EMPTY_SPACE_OFFSET);

      if (max_entry <= rownr)
      {
        /* Add directory entry first in directory and data last on page */
        if (extend_directory(page_type == HEAD_PAGE ? info : 0, buff,
                             block_size, max_entry, rownr, &empty_space))
          goto crashed_file;
      }
      if (extend_area_on_page(page_type == HEAD_PAGE ? info : 0, buff,
                              dir, rownr, block_size,
                              (uint) data_length, &empty_space,
                              &rec_offset, &length))
        goto crashed_file;
    }
  }
  /* Copy data */
  int2store(dir+2, data_length);
  memcpy(buff + rec_offset, data, data_length);
  empty_space-= (uint) data_length;
  int2store(buff + EMPTY_SPACE_OFFSET, empty_space);

  /* Fix bitmap */
  if (!enough_free_entries_on_page(share, buff))
    empty_space= 0;                         /* Page is full */
  if (_ma_bitmap_set(info, page, page_type == HEAD_PAGE, empty_space))
    goto err;

  /*
    If page was not read before, write it but keep it pinned.
    We don't update its LSN When we have processed all REDOs for this page
    in the current REDO's group, we will stamp page with UNDO's LSN
    (if we stamped it now, a next REDO, in
    this group, for this page, would be skipped) and unpin then.
  */
  result= 0;
  if (unlock_method == PAGECACHE_LOCK_WRITE &&
      pagecache_write(share->pagecache,
                      &info->dfile, page, 0,
                      buff, PAGECACHE_PLAIN_PAGE,
                      unlock_method, unpin_method,
                      PAGECACHE_WRITE_DELAY, &page_link.link,
                      LSN_IMPOSSIBLE))
    result= my_errno;

  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= 1;
  push_dynamic(&info->pinned_pages, (void*) &page_link);

  /*
    Data page and bitmap page are in place, we can update data_file_length in
    case we extended the file. We could not do it earlier: bitmap code tests
    data_file_length to know if it has to create a new page or not.
  */
  set_if_bigger(share->state.state.data_file_length, end_of_page);
  DBUG_RETURN(result);

crashed_file:
  _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
err:
  error= my_errno;
  if (unlock_method == PAGECACHE_LOCK_LEFT_WRITELOCKED)
    pagecache_unlock_by_link(share->pagecache, page_link.link,
                             PAGECACHE_LOCK_WRITE_UNLOCK,
                             PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                             LSN_IMPOSSIBLE, 0, FALSE);
  _ma_mark_file_crashed(share);
  DBUG_ASSERT(!maria_assert_if_crashed_table); /* catch recovery error early */
  DBUG_RETURN((my_errno= error));
}


/*
  Apply LOGREC_REDO_PURGE_ROW_HEAD & LOGREC_REDO_PURGE_ROW_TAIL

  SYNOPSIS
    _ma_apply_redo_purge_row_head_or_tail()
    info		Maria handler
    lsn			LSN to put on page
    page_type		HEAD_PAGE or TAIL_PAGE
    header		Header (without FILEID)

  NOTES
    This function is very similar to delete_head_or_tail()

  RETURN
    0   ok
    #   Error number
*/

uint _ma_apply_redo_purge_row_head_or_tail(MARIA_HA *info, LSN lsn,
                                           uint page_type,
                                           const uchar *header)
{
  MARIA_SHARE *share= info->s;
  pgcache_page_no_t page;
  uint      rownr, empty_space;
  uint      block_size= share->block_size;
  uchar     *buff;
  int result;
  uint error;
  MARIA_PINNED_PAGE page_link;
  DBUG_ENTER("_ma_apply_redo_purge_row_head_or_tail");

  page=  page_korr(header);
  rownr= dirpos_korr(header+PAGE_STORE_SIZE);
  DBUG_PRINT("enter", ("rowid: %lu  page: %lu  rownr: %u",
                       (ulong) ma_recordpos(page, rownr),
                       (ulong) page, rownr));

  share->state.changed|= (STATE_CHANGED | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);

  if (!(buff= pagecache_read(share->pagecache, &info->dfile,
                             page, 0, 0,
                             PAGECACHE_PLAIN_PAGE, PAGECACHE_LOCK_WRITE,
                             &page_link.link)))
    goto err;

  if (lsn_korr(buff) >= lsn)
  {
    /*
      Already applied
      Note that in case the page is not anymore a head or tail page
      a future redo will fix the bitmap.
    */
    if ((uint) (buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) == page_type)
    {
      empty_space= uint2korr(buff+EMPTY_SPACE_OFFSET);
      if (!enough_free_entries_on_page(share, buff))
        empty_space= 0;                         /* Page is full */
      if (_ma_bitmap_set(info, page, page_type == HEAD_PAGE,
                         empty_space))
        goto err;
    }
    pagecache_unlock_by_link(share->pagecache, page_link.link,
                             PAGECACHE_LOCK_WRITE_UNLOCK,
                             PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                             LSN_IMPOSSIBLE, 0, FALSE);
    DBUG_RETURN(0);
  }

  DBUG_ASSERT((buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK) == (uchar) page_type);

  if (delete_dir_entry(buff, block_size, rownr, &empty_space) < 0)
  {
    _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
    goto err;
  }

  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= 1;
  push_dynamic(&info->pinned_pages, (void*) &page_link);

  result= 0;
  if (!enough_free_entries_on_page(share, buff))
    empty_space= 0;                         /* Page is full */
  /* This will work even if the page was marked as UNALLOCATED_PAGE */
  if (_ma_bitmap_set(info, page, page_type == HEAD_PAGE, empty_space))
    result= my_errno;

  DBUG_RETURN(result);

err:
  error= my_errno;
  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 0, FALSE);
  _ma_mark_file_crashed(share);
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  DBUG_RETURN((my_errno= error));

}


/**
   @brief Apply LOGREC_REDO_FREE_BLOCKS

   @param  info            Maria handler
   @param  header          Header (without FILEID)

   Mark the pages free in the bitmap.

   We have to check against _ma_redo_not_needed_for_page()
   to guard against the case where we first clear a block and after
   that insert new data into the blocks.  If we would unconditionally
   clear the bitmap here, future changes would be ignored for the page
   if it's not in the dirty list (ie, it would be flushed).

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

uint _ma_apply_redo_free_blocks(MARIA_HA *info,
                                LSN lsn __attribute__((unused)),
                                LSN redo_lsn,
                                const uchar *header)
{
  MARIA_SHARE *share= info->s;
  uint ranges;
  uint16 sid;
  DBUG_ENTER("_ma_apply_redo_free_blocks");

  share->state.changed|= (STATE_CHANGED | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);

  sid= fileid_korr(header);
  header+= FILEID_STORE_SIZE;
  ranges= pagerange_korr(header);
  header+= PAGERANGE_STORE_SIZE;
  DBUG_ASSERT(ranges > 0);

  /** @todo leave bitmap lock to the bitmap code... */
  pthread_mutex_lock(&share->bitmap.bitmap_lock);
  while (ranges--)
  {
    my_bool res;
    uint page_range;
    pgcache_page_no_t page, start_page;

    start_page= page= page_korr(header);
    header+= PAGE_STORE_SIZE;
    /* Page range may have this bit set to indicate a tail page */
    page_range= pagerange_korr(header) & ~(TAIL_BIT | START_EXTENT_BIT);
    DBUG_ASSERT(page_range > 0);

    header+= PAGERANGE_STORE_SIZE;

    DBUG_PRINT("info", ("page: %lu  pages: %u", (long) page, page_range));

    for ( ; page_range-- ; start_page++)
    {
      if (_ma_redo_not_needed_for_page(sid, redo_lsn, start_page, FALSE))
        continue;
      res= _ma_bitmap_reset_full_page_bits(info, &share->bitmap, start_page,
                                           1);
      if (res)
      {
        pthread_mutex_unlock(&share->bitmap.bitmap_lock);
        _ma_mark_file_crashed(share);
        DBUG_ASSERT(!maria_assert_if_crashed_table);
        DBUG_RETURN(res);
      }
    }
  }
  pthread_mutex_unlock(&share->bitmap.bitmap_lock);
  DBUG_RETURN(0);
}


/**
   @brief Apply LOGREC_REDO_FREE_HEAD_OR_TAIL

   @param  info            Maria handler
   @param  header          Header (without FILEID)

   @note It marks the page free in the bitmap, and sets the directory's count
   to 0.

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

uint _ma_apply_redo_free_head_or_tail(MARIA_HA *info, LSN lsn,
                                      const uchar *header)
{
  MARIA_SHARE *share= info->s;
  uchar *buff;
  pgcache_page_no_t page;
  MARIA_PINNED_PAGE page_link;
  my_bool res;
  DBUG_ENTER("_ma_apply_redo_free_head_or_tail");

  share->state.changed|= (STATE_CHANGED | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);

  page= page_korr(header);

  if (!(buff= pagecache_read(share->pagecache,
                             &info->dfile,
                             page, 0, 0,
                             PAGECACHE_PLAIN_PAGE,
                             PAGECACHE_LOCK_WRITE, &page_link.link)))
  {
    pagecache_unlock_by_link(share->pagecache, page_link.link,
                             PAGECACHE_LOCK_WRITE_UNLOCK,
                             PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                             LSN_IMPOSSIBLE, 0, FALSE);
    goto err;
  }
  if (lsn_korr(buff) >= lsn)
  {
    /* Already applied */
    pagecache_unlock_by_link(share->pagecache, page_link.link,
                             PAGECACHE_LOCK_WRITE_UNLOCK,
                             PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                             LSN_IMPOSSIBLE, 0, FALSE);
  }
  else
  {
    buff[PAGE_TYPE_OFFSET]= UNALLOCATED_PAGE;
#ifdef IDENTICAL_PAGES_AFTER_RECOVERY
    {
      uint number_of_records= (uint) buff[DIR_COUNT_OFFSET];
      uchar *dir= dir_entry_pos(buff, share->block_size,
                                number_of_records-1);
      buff[DIR_FREE_OFFSET]=  END_OF_DIR_FREE_LIST;
      bzero(dir, number_of_records * DIR_ENTRY_SIZE);
    }
#endif

    page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
    page_link.changed= 1;
    push_dynamic(&info->pinned_pages, (void*) &page_link);
  }
  /** @todo leave bitmap lock to the bitmap code... */
  pthread_mutex_lock(&share->bitmap.bitmap_lock);
  res= _ma_bitmap_reset_full_page_bits(info, &share->bitmap, page, 1);
  pthread_mutex_unlock(&share->bitmap.bitmap_lock);
  if (res)
    goto err;
  DBUG_RETURN(0);

err:
  _ma_mark_file_crashed(share);
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  DBUG_RETURN(1);
}


/**
   @brief Apply LOGREC_REDO_INSERT_ROW_BLOBS

   @param  info            Maria handler
   @parma  lsn             LSN to put on pages
   @param  header          Header (with FILEID)
   @param  redo_lsn        REDO record's LSN
   @param[out] number_of_blobs Number of blobs found in log record
   @param[out] number_of_ranges Number of ranges found
   @param[out] first_page  First page touched
   @param[out] last_page   Last page touched

   @note Write full pages (full head & blob pages)

   @return Operation status
     @retval 0      OK
     @retval !=0    Error
*/

uint _ma_apply_redo_insert_row_blobs(MARIA_HA *info,
                                     LSN lsn, const uchar *header,
                                     LSN redo_lsn,
                                     uint * const number_of_blobs,
                                     uint * const number_of_ranges,
                                     pgcache_page_no_t * const first_page,
                                     pgcache_page_no_t * const last_page)
{
  MARIA_SHARE *share= info->s;
  const uchar *data;
  uint      data_size= FULL_PAGE_SIZE(share->block_size);
  uint      blob_count, ranges;
  uint16    sid;
  pgcache_page_no_t first_page2= ULONGLONG_MAX, last_page2= 0;
  DBUG_ENTER("_ma_apply_redo_insert_row_blobs");

  share->state.changed|= (STATE_CHANGED | STATE_NOT_ZEROFILLED |
                          STATE_NOT_MOVABLE);

  sid= fileid_korr(header);
  header+= FILEID_STORE_SIZE;
  *number_of_ranges= ranges= pagerange_korr(header);
  header+= PAGERANGE_STORE_SIZE;
  *number_of_blobs= blob_count= pagerange_korr(header);
  header+= PAGERANGE_STORE_SIZE;
  DBUG_ASSERT(ranges >= blob_count);

  data= (header + ranges * ROW_EXTENT_SIZE +
         blob_count * (SUB_RANGE_SIZE + BLOCK_FILLER_SIZE));

  while (blob_count--)
  {
    uint sub_ranges, empty_space;

    sub_ranges=  uint2korr(header);
    header+= SUB_RANGE_SIZE;
    empty_space= uint2korr(header);
    header+= BLOCK_FILLER_SIZE;
    DBUG_ASSERT(sub_ranges <= ranges && empty_space < data_size);
    ranges-= sub_ranges;

    while (sub_ranges--)
    {
      uint i;
      uint      res;
      uint      page_range;
      pgcache_page_no_t page;
      uchar     *buff;
      uint	data_on_page= data_size;

      page= page_korr(header);
      header+= PAGE_STORE_SIZE;
      page_range= pagerange_korr(header);
      header+= PAGERANGE_STORE_SIZE;

      for (i= page_range; i-- > 0 ; page++, data+= data_on_page)
      {
        MARIA_PINNED_PAGE page_link;
        enum pagecache_page_lock unlock_method;
        enum pagecache_page_pin unpin_method;

        set_if_smaller(first_page2, page);
        set_if_bigger(last_page2, page);
        if (i == 0 && sub_ranges == 0)
          data_on_page= data_size - empty_space; /* data on last page */
        if (_ma_redo_not_needed_for_page(sid, redo_lsn, page, FALSE))
          continue;

        if (((page + 1) * share->block_size) >
            share->state.state.data_file_length)
        {
          /* New page or half written page at end of file */
          DBUG_PRINT("info", ("Enlarging data file from %lu to %lu",
                              (ulong) share->state.state.data_file_length,
                              (ulong) ((page + 1 ) * share->block_size)));
          share->state.state.data_file_length= (page + 1) * share->block_size;
          buff= info->keyread_buff;
          info->keyread_buff_used= 1;
          make_empty_page(info, buff, BLOB_PAGE, 0);
          unlock_method= PAGECACHE_LOCK_LEFT_UNLOCKED;
          unpin_method=  PAGECACHE_PIN_LEFT_UNPINNED;
        }
        else
        {
          share->pagecache->readwrite_flags&= ~MY_WME;
          buff= pagecache_read(share->pagecache,
                               &info->dfile,
                               page, 0, 0,
                               PAGECACHE_PLAIN_PAGE,
                               PAGECACHE_LOCK_WRITE, &page_link.link);
          share->pagecache->readwrite_flags= share->pagecache->
            org_readwrite_flags;
          if (!buff)
          {
            if (my_errno != HA_ERR_FILE_TOO_SHORT &&
                my_errno != HA_ERR_WRONG_CRC)
            {
              /* If not read outside of file */
              pagecache_unlock_by_link(share->pagecache, page_link.link,
                                       PAGECACHE_LOCK_WRITE_UNLOCK,
                                       PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                                       LSN_IMPOSSIBLE, 0, FALSE);
              goto err;
            }
            /*
              Physical file was too short, create new page. It can be that
              recovery started with a file with N pages, wrote page N+2 into
              pagecache (increased data_file_length but not physical file
              length), now reads page N+1: the read fails.
            */
            buff= pagecache_block_link_to_buffer(page_link.link);
            make_empty_page(info, buff, BLOB_PAGE, 0);
          }
          else
          {
#ifndef DBUG_OFF
            uchar found_page_type= (buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK);
#endif
            if (lsn_korr(buff) >= lsn)
            {
              /* Already applied */
              DBUG_PRINT("info", ("already applied %llu >= %llu",
                                  lsn_korr(buff), lsn));
              pagecache_unlock_by_link(share->pagecache, page_link.link,
                                       PAGECACHE_LOCK_WRITE_UNLOCK,
                                       PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                                       LSN_IMPOSSIBLE, 0, FALSE);
              goto fix_bitmap;
            }
            DBUG_ASSERT((found_page_type == (uchar) BLOB_PAGE) ||
                        (found_page_type == (uchar) UNALLOCATED_PAGE));
          }
          unlock_method= PAGECACHE_LOCK_WRITE_UNLOCK;
          unpin_method=  PAGECACHE_UNPIN;
        }

        /*
          Blob pages are never updated twice in same redo-undo chain, so
          it's safe to update lsn for them here
        */
        lsn_store(buff, lsn);
        buff[PAGE_TYPE_OFFSET]= BLOB_PAGE;

        if (data_on_page != data_size)
        {
          /*
            Last page may be only partly filled. We zero the rest, like
            write_full_pages() does.
          */
          bzero(buff + share->block_size - PAGE_SUFFIX_SIZE - empty_space,
                empty_space);
        }
        memcpy(buff+ PAGE_TYPE_OFFSET + 1, data, data_on_page);
        if (pagecache_write(share->pagecache,
                            &info->dfile, page, 0,
                            buff, PAGECACHE_PLAIN_PAGE,
                            unlock_method, unpin_method,
                            PAGECACHE_WRITE_DELAY, 0, LSN_IMPOSSIBLE))
          goto err;

    fix_bitmap:
      /** @todo leave bitmap lock to the bitmap code... */
        pthread_mutex_lock(&share->bitmap.bitmap_lock);
        res= _ma_bitmap_set_full_page_bits(info, &share->bitmap, page,
                                           1);
        pthread_mutex_unlock(&share->bitmap.bitmap_lock);
        if (res)
          goto err;
      }
    }
  }
  *first_page= first_page2;
  *last_page=  last_page2;
  DBUG_RETURN(0);

err:
  _ma_mark_file_crashed(share);
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  DBUG_RETURN(1);
}


/****************************************************************************
 Applying of UNDO entries
****************************************************************************/

/** Execute undo of a row insert (delete the inserted row) */

my_bool _ma_apply_undo_row_insert(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header)
{
  pgcache_page_no_t page;
  uint rownr;
  uchar *buff;
  my_bool res;
  MARIA_PINNED_PAGE page_link;
  MARIA_SHARE *share= info->s;
  ha_checksum checksum;
  LSN lsn;
  DBUG_ENTER("_ma_apply_undo_row_insert");

  page=  page_korr(header);
  header+= PAGE_STORE_SIZE;
  rownr= dirpos_korr(header);
  header+= DIRPOS_STORE_SIZE;
  DBUG_PRINT("enter", ("rowid: %lu  page: %lu  rownr: %u",
                       (ulong) ma_recordpos(page, rownr),
                       (ulong) page, rownr));

  buff= pagecache_read(share->pagecache,
                       &info->dfile, page, 0,
                       0, share->page_type,
                       PAGECACHE_LOCK_WRITE,
                       &page_link.link);
  page_link.unlock= PAGECACHE_LOCK_WRITE_UNLOCK;
  page_link.changed= buff != 0;
  push_dynamic(&info->pinned_pages, (void*) &page_link);
  if (!buff)
    goto err;

  if (read_row_extent_info(info, buff, rownr))
    goto err;

  _ma_bitmap_flushable(info, 1);
  if (delete_head_or_tail(info, page, rownr, 1, 1) ||
      delete_tails(info, info->cur_row.tail_positions))
    goto err;

  if (info->cur_row.extents_count && free_full_pages(info, &info->cur_row))
    goto err;

  checksum= 0;
  if (share->calc_checksum)
    checksum= (ha_checksum) 0 - ha_checksum_korr(header);
  info->last_auto_increment= ~ (ulonglong) 0;
  if (_ma_write_clr(info, undo_lsn, LOGREC_UNDO_ROW_INSERT,
                    share->calc_checksum != 0, checksum, &lsn, (void*) 0))
    goto err;

  res= 0;
end:
  if (info->non_flushable_state)
    _ma_bitmap_flushable(info, -1);
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res);

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  res= 1;
  _ma_mark_file_crashed(share);
  goto end;
}


/** Execute undo of a row delete (insert the row back where it was) */

my_bool _ma_apply_undo_row_delete(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, size_t header_length
                                  __attribute__((unused)))
{
  MARIA_SHARE *share= info->s;
  MARIA_ROW row;
  MARIA_COLUMNDEF *column, *end_column;
  MARIA_BITMAP_BLOCKS *blocks;
  struct st_row_pos_info row_pos;
  uchar *record;
  const uchar *null_bits, *field_length_data, *extent_info;
  pgcache_page_no_t page;
  ulong *blob_lengths;
  uint *null_field_lengths, extent_count, rownr, length_on_head_page;
  DBUG_ENTER("_ma_apply_undo_row_delete");

  /*
    Use cur row as a base;  We need to make a copy as we will change
    some buffers to point directly to 'header'
  */
  memcpy(&row, &info->cur_row, sizeof(row));

  page=  page_korr(header);
  header+= PAGE_STORE_SIZE;
  rownr= dirpos_korr(header);
  header+= DIRPOS_STORE_SIZE;
  length_on_head_page= uint2korr(header);
  header+= 2;
  extent_count= pagerange_korr(header);
  header+= PAGERANGE_STORE_SIZE;
  DBUG_PRINT("enter", ("rowid: %lu  page: %lu  rownr: %u",
                       (ulong) ma_recordpos(page, rownr),
                       (ulong) page, rownr));

  if (share->calc_checksum)
  {
    /*
      We extract the checksum delta here, saving a recomputation in
      allocate_and_write_block_record(). It's only an optimization.
    */
    row.checksum= (ha_checksum) 0 - ha_checksum_korr(header);
    header+= HA_CHECKSUM_STORE_SIZE;
  }
  extent_info= header;
  header+= extent_count * ROW_EXTENT_SIZE;

  null_field_lengths= row.null_field_lengths;
  blob_lengths= row.blob_lengths;

  /*
    Fill in info->cur_row with information about the row, like in
    calc_record_size(), to be used by write_block_record()
  */

  row.normal_length= row.char_length= row.varchar_length=
    row.blob_length= row.extents_count= row.field_lengths_length= 0;

  null_bits= header;
  header+= share->base.null_bytes;
  /* This will not be changed */
  row.empty_bits= (uchar*) header;
  header+= share->base.pack_bytes;
  if (share->base.max_field_lengths)
  {
    row.field_lengths_length= uint2korr(header);
    row.field_lengths= (uchar*) header + 2 ;
    header+= 2 + row.field_lengths_length;
  }
  if (share->base.blobs)
    row.blob_length= ma_get_length(&header);

  /* We need to build up a record (without blobs) in rec_buff */
  if (!(record= my_malloc(share->base.reclength, MYF(MY_WME))))
    DBUG_RETURN(1);

  memcpy(record, null_bits, share->base.null_bytes);

  /* Copy field information from header to record */

  /* Handle constant length fields that are always present */
  for (column= share->columndef,
         end_column= column+ share->base.fixed_not_null_fields;
       column < end_column;
       column++)
  {
    memcpy(record + column->offset, header, column->length);
    header+= column->length;
  }

  /* Handle NULL fields and CHAR/VARCHAR fields */
  field_length_data= row.field_lengths;
  for (end_column= share->columndef + share->base.fields;
       column < end_column;
       column++, null_field_lengths++)
  {
    if ((record[column->null_pos] & column->null_bit) ||
        row.empty_bits[column->empty_pos] & column->empty_bit)
    {
      if (column->type != FIELD_BLOB)
        *null_field_lengths= 0;
      else
        *blob_lengths++= 0;
      if (share->calc_checksum)
        bfill(record + column->offset, column->fill_length,
              column->type == FIELD_SKIP_ENDSPACE ? ' ' : 0);
      continue;
    }
    switch (column->type) {
    case FIELD_CHECK:
    case FIELD_NORMAL:                          /* Fixed length field */
    case FIELD_ZERO:
    case FIELD_SKIP_PRESPACE:                   /* Not packed */
    case FIELD_SKIP_ZERO:                       /* Fixed length field */
      row.normal_length+= column->length;
      *null_field_lengths= column->length;
      memcpy(record + column->offset, header, column->length);
      header+= column->length;
      break;
    case FIELD_SKIP_ENDSPACE:                   /* CHAR */
    {
      uint length;
      if (column->length <= 255)
        length= (uint) *field_length_data++;
      else
      {
        length= uint2korr(field_length_data);
        field_length_data+= 2;
      }
      row.char_length+= length;
      *null_field_lengths= length;
      memcpy(record + column->offset, header, length);
      if (share->calc_checksum)
        bfill(record + column->offset + length, (column->length - length),
              ' ');
      header+= length;
      break;
    }
    case FIELD_VARCHAR:
    {
      uint length;
      uchar *field_pos= record + column->offset;

      /* 256 is correct as this includes the length uchar */
      if (column->fill_length == 1)
      {
        field_pos[0]= *field_length_data;
        length= (uint) *field_length_data;
      }
      else
      {
        field_pos[0]= field_length_data[0];
        field_pos[1]= field_length_data[1];
        length= uint2korr(field_length_data);
      }
      field_length_data+= column->fill_length;
      field_pos+= column->fill_length;
      row.varchar_length+= length;
      *null_field_lengths= length;
      memcpy(field_pos, header, length);
      header+= length;
      break;
    }
    case FIELD_BLOB:
    {
      /* Copy length of blob and pointer to blob data to record */
      uchar *field_pos= record + column->offset;
      uint size_length= column->length - portable_sizeof_char_ptr;
      ulong blob_length= _ma_calc_blob_length(size_length, field_length_data);

      memcpy(field_pos, field_length_data, size_length);
      field_length_data+= size_length;
      memcpy(field_pos + size_length, &header, sizeof(&header));
      header+= blob_length;
      *blob_lengths++= blob_length;
      break;
    }
    default:
      DBUG_ASSERT(0);
    }
  }
  row.head_length= (info->row_base_length +
                    share->base.fixed_not_null_fields_length +
                    row.field_lengths_length +
                    size_to_store_key_length(row.field_lengths_length) +
                    row.normal_length +
                    row.char_length + row.varchar_length);
  row.total_length= (row.head_length + row.blob_length);
  if (row.total_length < share->base.min_block_length)
    row.total_length= share->base.min_block_length;

  /*
    Row is now generated. Now we need to insert record on the original
    pages with original size on each page.
  */

  _ma_bitmap_flushable(info, 1);
  /* Change extent information to be usable by write_block_record() */
  blocks= &row.insert_blocks;
  if (extent_to_bitmap_blocks(info, blocks, page, extent_count, extent_info))
    goto err;
  blocks->block->org_bitmap_value= _ma_bitmap_get_page_bits(info,
                                                            &share->bitmap,
                                                            page);
  blocks->block->used|= BLOCKUSED_USE_ORG_BITMAP;

  /* Read head page and allocate data for rowid */
  if (get_rowpos_in_head_or_tail_page(info, blocks->block,
                                      info->buff,
                                      length_on_head_page,
                                      HEAD_PAGE, PAGECACHE_LOCK_WRITE,
                                      rownr, &row_pos))
    goto err;

  if (share->calc_checksum)
  {
    DBUG_ASSERT(row.checksum == (share->calc_checksum)(info, record));
  }
  /* Store same amount of data on head page as on original page */
  row_pos.length= (length_on_head_page - 
                   (extent_count + 1 - blocks->count) * ROW_EXTENT_SIZE);
  set_if_bigger(row_pos.length, share->base.min_block_length);
  if (write_block_record(info, (uchar*) 0, record, &row,
                         blocks, blocks->block->org_bitmap_value != 0,
                         &row_pos, undo_lsn, 0))
    goto err;

  my_free(record, MYF(0));
  DBUG_RETURN(0);

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  _ma_mark_file_crashed(share);
  if (info->non_flushable_state)
    _ma_bitmap_flushable(info, -1);
  _ma_unpin_all_pages_and_finalize_row(info, LSN_IMPOSSIBLE);
  my_free(record, MYF(0));
  DBUG_RETURN(1);
}


/**
  Execute undo of a row update

  @fn _ma_apply_undo_row_update()

  @return Operation status
    @retval 0      OK
    @retval 1      Error
*/

my_bool _ma_apply_undo_row_update(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header,
                                  size_t header_length
                                  __attribute__((unused)))
{
  MARIA_SHARE *share= info->s;
  MARIA_RECORD_POS record_pos;
  const uchar *field_length_data, *field_length_data_end, *extent_info;
  uchar *current_record, *orig_record;
  pgcache_page_no_t page;
  ha_checksum checksum_delta;
  uint rownr, field_length_header, extent_count, length_on_head_page;
  int error;
  DBUG_ENTER("_ma_apply_undo_row_update");
  LINT_INIT(checksum_delta);

  page=  page_korr(header);
  header+= PAGE_STORE_SIZE;
  rownr= dirpos_korr(header);
  header+= DIRPOS_STORE_SIZE;

  record_pos= ma_recordpos(page, rownr);
  DBUG_PRINT("enter", ("rowid: %lu  page: %lu  rownr: %u",
                       (ulong) record_pos, (ulong) page, rownr));

  if (share->calc_checksum)
  {
    checksum_delta= ha_checksum_korr(header);
    header+= HA_CHECKSUM_STORE_SIZE;
  }
  length_on_head_page= uint2korr(header);
  set_if_bigger(length_on_head_page, share->base.min_block_length);
  header+= 2;
  extent_count= pagerange_korr(header);
  header+= PAGERANGE_STORE_SIZE;
  extent_info= header;
  header+= extent_count * ROW_EXTENT_SIZE;

  /*
    Set header to point to old field values, generated by
    fill_update_undo_parts()
  */
  field_length_header= ma_get_length(&header);
  field_length_data= (uchar*) header;
  header+= field_length_header;
  field_length_data_end= header;

  /* Allocate buffer for current row & original row */
  if (!(current_record= my_malloc(share->base.reclength * 2, MYF(MY_WME))))
    DBUG_RETURN(1);
  orig_record= current_record+ share->base.reclength;

  /* Read current record */
  if (_ma_read_block_record(info, current_record, record_pos))
    goto err;

  if (*field_length_data == 255)
  {
    /* Bitmap changed */
    field_length_data++;
    memcpy(orig_record, header, share->base.null_bytes);
    header+= share->base.null_bytes;
  }
  else
    memcpy(orig_record, current_record, share->base.null_bytes);
  bitmap_clear_all(&info->changed_fields);

  while (field_length_data < field_length_data_end)
  {
    uint field_nr= ma_get_length(&field_length_data), field_length;
    MARIA_COLUMNDEF *column= share->columndef + field_nr;
    uchar *orig_field_pos= orig_record + column->offset;

    bitmap_set_bit(&info->changed_fields, field_nr);
    if (field_nr >= share->base.fixed_not_null_fields)
    {
      if (!(field_length= ma_get_length(&field_length_data)))
      {
        /* Null field or empty field */
        bfill(orig_field_pos, column->fill_length,
              column->type == FIELD_SKIP_ENDSPACE ? ' ' : 0);
        continue;
      }
    }
    else
      field_length= column->length;

    switch (column->type) {
    case FIELD_CHECK:
    case FIELD_NORMAL:                          /* Fixed length field */
    case FIELD_ZERO:
    case FIELD_SKIP_PRESPACE:                   /* Not packed */
      memcpy(orig_field_pos, header, column->length);
      header+= column->length;
      break;
    case FIELD_SKIP_ZERO:                       /* Number */
    case FIELD_SKIP_ENDSPACE:                   /* CHAR */
    {
      uint diff;
      memcpy(orig_field_pos, header, field_length);
      if ((diff= (column->length - field_length)))
        bfill(orig_field_pos + column->length - diff, diff,
              column->type == FIELD_SKIP_ENDSPACE ? ' ' : 0);
      header+= field_length;
    }
    break;
    case FIELD_VARCHAR:
      if (column->length <= 256)
      {
        *orig_field_pos++= (uchar) field_length;
      }
      else
      {
        int2store(orig_field_pos, field_length);
        orig_field_pos+= 2;
      }
      memcpy(orig_field_pos, header, field_length);
      header+= field_length;
      break;
    case FIELD_BLOB:
    {
      uint size_length= column->length - portable_sizeof_char_ptr;
      _ma_store_blob_length(orig_field_pos, size_length, field_length);
      memcpy_fixed(orig_field_pos + size_length, &header, sizeof(header));
      header+= field_length;
      break;
    }
    default:
      DBUG_ASSERT(0);
    }
  }
  copy_not_changed_fields(info, &info->changed_fields,
                          orig_record, current_record);

  if (share->calc_checksum)
  {
    info->new_row.checksum= checksum_delta +
      (info->cur_row.checksum= (*share->calc_checksum)(info, orig_record));
    /* verify that record's content is sane */
    DBUG_ASSERT(info->new_row.checksum ==
                (*share->calc_checksum)(info, current_record));
  }

  info->last_auto_increment= ~ (ulonglong) 0;
  /* Now records are up to date, execute the update to original values */
  if (_ma_update_at_original_place(info, page, rownr, length_on_head_page,
                                   extent_count, extent_info,
                                   current_record, orig_record, undo_lsn))
    goto err;

  error= 0;
end:
  my_free(current_record, MYF(0));
  DBUG_RETURN(error);

err:
  DBUG_ASSERT(!maria_assert_if_crashed_table);
  error= 1;
  _ma_mark_file_crashed(share);
  goto end;
}


/**
  Execute undo of a bulk insert which used repair

  @return Operation status
    @retval 0      OK
    @retval 1      Error
*/

my_bool _ma_apply_undo_bulk_insert(MARIA_HA *info, LSN undo_lsn)
{
  my_bool error;
  LSN lsn;
  DBUG_ENTER("_ma_apply_undo_bulk_insert");
  /*
    We delete all rows, re-enable indices as bulk insert had disabled
    non-unique ones.
  */
  error= (maria_delete_all_rows(info) ||
          maria_enable_indexes(info) ||
          /* we enabled indices so need '2' below */
          _ma_state_info_write(info->s,
                               MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET |
                               MA_STATE_INFO_WRITE_FULL_INFO |
                               MA_STATE_INFO_WRITE_LOCK) ||
          _ma_write_clr(info, undo_lsn, LOGREC_UNDO_BULK_INSERT,
                        FALSE, 0, &lsn, NULL));
  DBUG_RETURN(error);
}


/**
  @brief Get the TRANSLOG_ADDRESS to flush up to

  @param page            Page's content
  @param page_no         Page's number (<offset>/<page length>)
  @param data_ptr        Callback data pointer (pointer to MARIA_SHARE)

  @note
  Usable for data (non-bitmap) and index pages

  @retval LSN to flush up to
*/

TRANSLOG_ADDRESS
maria_page_get_lsn(uchar *page,
                   pgcache_page_no_t page_no __attribute__((unused)),
                   uchar* data_ptr __attribute__((unused)))
{
#ifndef DBUG_OFF
  const MARIA_SHARE *share= (MARIA_SHARE*)data_ptr;
  DBUG_ASSERT(share->page_type == PAGECACHE_LSN_PAGE &&
              share->now_transactional);
#endif
  return lsn_korr(page);
}


/**
  @brief Enable reading of all rows, ignoring versioning

  @note
    This is mainly useful in single user applications, like maria_pack,
    where we want to be able to read all rows without having to read the
    transaction id from the control file
*/

void maria_ignore_trids(MARIA_HA *info)
{
  if (info->s->base.born_transactional)
  {
    if (!info->trn)
      _ma_set_trn_for_table(info, &dummy_transaction_object);
    /* Ignore transaction id when row is read */
    info->trn->min_read_from= ~(TrID) 0;
  }
}


#ifndef DBUG_OFF

/* The following functions are useful to call from debugger */

void _ma_print_block_info(uchar *buff)
{
  LSN lsn= lsn_korr(buff);

  printf("LSN: %lu,0x%lx  type: %u  dir_entries: %u  dir_free: %u  empty_space: %u\n",
         LSN_IN_PARTS(lsn),
         (uint)buff[PAGE_TYPE_OFFSET],
         (uint)buff[DIR_COUNT_OFFSET],
         (uint)buff[DIR_FREE_OFFSET],
         (uint) uint2korr(buff + EMPTY_SPACE_OFFSET));
  printf("Start of directory: %lu\n",
         maria_block_size - PAGE_SUFFIX_SIZE -
         (uint) buff[DIR_COUNT_OFFSET] * DIR_ENTRY_SIZE);
  _ma_print_directory(stdout, buff, maria_block_size);
}
#endif
