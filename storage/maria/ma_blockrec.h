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
  Storage of records in block
*/

#define LSN_SIZE		7
#define DIRCOUNT_SIZE		1	/* Stores number of rows on page */
#define EMPTY_SPACE_SIZE	2	/* Stores empty space on page */
#define PAGE_TYPE_SIZE		1
#define PAGE_SUFFIX_SIZE	0	/* Bytes for page suffix */
#define PAGE_HEADER_SIZE	(LSN_SIZE + DIRCOUNT_SIZE + EMPTY_SPACE_SIZE +\
                                 PAGE_TYPE_SIZE)
#define PAGE_OVERHEAD_SIZE	(PAGE_HEADER_SIZE + DIR_ENTRY_SIZE + \
                                 PAGE_SUFFIX_SIZE)
#define BLOCK_RECORD_POINTER_SIZE	6

#define FULL_PAGE_SIZE(block_size) ((block_size) - LSN_SIZE - PAGE_TYPE_SIZE)

#define ROW_EXTENT_PAGE_SIZE	5
#define ROW_EXTENT_COUNT_SIZE   2
#define ROW_EXTENT_SIZE		(ROW_EXTENT_PAGE_SIZE + ROW_EXTENT_COUNT_SIZE)
#define TAIL_BIT		0x8000	/* Bit in page_count to signify tail */
#define ELEMENTS_RESERVED_FOR_MAIN_PART 4
#define EXTRA_LENGTH_FIELDS		3

#define FLAG_SIZE		1
#define TRANSID_SIZE		6
#define VERPTR_SIZE		7
#define DIR_ENTRY_SIZE		4
#define FIELD_OFFSET_SIZE	2

/* Minimum header size needed for a new row */
#define BASE_ROW_HEADER_SIZE FLAG_SIZE
#define TRANS_ROW_EXTRA_HEADER_SIZE TRANSID_SIZE

#define PAGE_TYPE_MASK 127
enum en_page_type { UNALLOCATED_PAGE, HEAD_PAGE, TAIL_PAGE, BLOB_PAGE, MAX_PAGE_TYPE };

#define PAGE_TYPE_OFFSET        LSN_SIZE
#define DIR_ENTRY_OFFSET        LSN_SIZE+PAGE_TYPE_SIZE
#define EMPTY_SPACE_OFFSET      (DIR_ENTRY_OFFSET + DIRCOUNT_SIZE)

#define PAGE_CAN_BE_COMPACTED   128             /* Bit in PAGE_TYPE */

/* Bits used for flag byte (one byte, first in record) */
#define ROW_FLAG_TRANSID                1
#define ROW_FLAG_VER_PTR                2
#define ROW_FLAG_DELETE_TRANSID         4
#define ROW_FLAG_NULLS_EXTENDED         8
#define ROW_FLAG_EXTENTS                128
#define ROW_FLAG_ALL			(1+2+4+8+128)

/* Variables that affects how data pages are utilized */
#define MIN_TAIL_SIZE           32

/* Fixed part of Max possible header size; See table in ma_blockrec.c */
#define MAX_FIXED_HEADER_SIZE (FLAG_SIZE + 3 + ROW_EXTENT_SIZE + 3)
#define TRANS_MAX_FIXED_HEADER_SIZE (MAX_FIXED_HEADER_SIZE + \
                                     FLAG_SIZE + TRANSID_SIZE + VERPTR_SIZE + \
                                     TRANSID_SIZE)

/* We use 1 byte in record header to store number of directory entries */
#define MAX_ROWS_PER_PAGE	255

/* Bits for MARIA_BITMAP_BLOCKS->used */
#define BLOCKUSED_USED		 1
#define BLOCKUSED_USE_ORG_BITMAP 2
#define BLOCKUSED_TAIL		 4

/* defines that affects allocation (density) of data */

/* If we fill up a block to 75 %, don't create a new tail page for it */
#define MAX_TAIL_SIZE(block_size) ((block_size) *3 / 4)

/* Functions to convert MARIA_RECORD_POS to/from page:offset */

static inline MARIA_RECORD_POS ma_recordpos(ulonglong page, uint offset)
{
  return (MARIA_RECORD_POS) ((page << 8) | offset);
}

static inline my_off_t ma_recordpos_to_page(MARIA_RECORD_POS record_pos)
{
  return record_pos >> 8;
}

static inline my_off_t ma_recordpos_to_offset(MARIA_RECORD_POS record_pos)
{
  return record_pos & 255;
}

/* ma_blockrec.c */
void _ma_init_block_record_data(void);
my_bool _ma_once_init_block_row(MARIA_SHARE *share, File dfile);
my_bool _ma_once_end_block_row(MARIA_SHARE *share);
my_bool _ma_init_block_row(MARIA_HA *info);
void _ma_end_block_row(MARIA_HA *info);

my_bool _ma_update_block_record(MARIA_HA *info, MARIA_RECORD_POS pos,
                                const byte *record);
my_bool _ma_delete_block_record(MARIA_HA *info);
int     _ma_read_block_record(MARIA_HA *info, byte *record,
                              MARIA_RECORD_POS record_pos);
int _ma_read_block_record2(MARIA_HA *info, byte *record,
                           byte *data, byte *end_of_data);
int     _ma_scan_block_record(MARIA_HA *info, byte *record,
                              MARIA_RECORD_POS, my_bool);
my_bool _ma_cmp_block_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                             const byte *record, MARIA_RECORD_POS pos);
my_bool _ma_scan_init_block_record(MARIA_HA *info);
void _ma_scan_end_block_record(MARIA_HA *info);

MARIA_RECORD_POS _ma_write_init_block_record(MARIA_HA *info,
                                             const byte *record);
my_bool _ma_write_block_record(MARIA_HA *info, const byte *record);
my_bool _ma_write_abort_block_record(MARIA_HA *info);
my_bool _ma_compare_block_record(register MARIA_HA *info,
                                 register const byte *record);

/* ma_bitmap.c */
my_bool _ma_bitmap_init(MARIA_SHARE *share, File file);
my_bool _ma_bitmap_end(MARIA_SHARE *share);
my_bool _ma_flush_bitmap(MARIA_SHARE *share);
my_bool _ma_read_bitmap_page(MARIA_SHARE *share, MARIA_FILE_BITMAP *bitmap,
                             ulonglong page);
my_bool _ma_bitmap_find_place(MARIA_HA *info, MARIA_ROW *row,
                              MARIA_BITMAP_BLOCKS *result_blocks);
my_bool _ma_bitmap_release_unused(MARIA_HA *info, MARIA_BITMAP_BLOCKS *blocks);
my_bool _ma_bitmap_free_full_pages(MARIA_HA *info, const byte *extents,
                                   uint count);
my_bool _ma_bitmap_set(MARIA_HA *info, ulonglong pos, my_bool head,
                       uint empty_space);
my_bool _ma_reset_full_page_bits(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap,
                                 ulonglong page, uint page_count);
uint _ma_free_size_to_head_pattern(MARIA_FILE_BITMAP *bitmap, uint size);
my_bool _ma_bitmap_find_new_place(MARIA_HA *info, MARIA_ROW *new_row,
                                  ulonglong page, uint free_size,
                                  MARIA_BITMAP_BLOCKS *result_blocks);
my_bool _ma_check_bitmap_data(MARIA_HA *info,
                              enum en_page_type page_type, ulonglong page,
                              uint empty_space, uint *bitmap_pattern);
my_bool _ma_check_if_right_bitmap_type(MARIA_HA *info,
                                       enum en_page_type page_type,
                                       ulonglong page,
                                       uint *bitmap_pattern);
