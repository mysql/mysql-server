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
#define DIR_COUNT_SIZE		1	/* Stores number of rows on page */
#define DIR_FREE_SIZE		1	/* Pointer to first free dir entry */
#define EMPTY_SPACE_SIZE	2	/* Stores empty space on page */
#define PAGE_TYPE_SIZE		1
#define PAGE_SUFFIX_SIZE	4	/* Bytes for checksum */
#define PAGE_HEADER_SIZE	(LSN_SIZE + DIR_COUNT_SIZE + DIR_FREE_SIZE +\
                                 EMPTY_SPACE_SIZE + PAGE_TYPE_SIZE)
#define PAGE_OVERHEAD_SIZE	(PAGE_HEADER_SIZE + DIR_ENTRY_SIZE + \
                                 PAGE_SUFFIX_SIZE)
#define BLOCK_RECORD_POINTER_SIZE	6

#define FULL_PAGE_SIZE(block_size) ((block_size) - LSN_SIZE - \
                                    PAGE_TYPE_SIZE - PAGE_SUFFIX_SIZE)

#define ROW_EXTENT_PAGE_SIZE	5
#define ROW_EXTENT_COUNT_SIZE   2
#define SUB_RANGE_SIZE		2
#define BLOCK_FILLER_SIZE	2
#define ROW_EXTENT_SIZE		(ROW_EXTENT_PAGE_SIZE + ROW_EXTENT_COUNT_SIZE)
#define TAIL_BIT		0x8000	/* Bit in page_count to signify tail */
#define START_EXTENT_BIT	0x4000	/* Bit in page_count to signify start*/
/* page_count set by bitmap code for tail pages */
#define TAIL_PAGE_COUNT_MARKER  0xffff
/* Number of extents reserved MARIA_BITMAP_BLOCKS to store head part */
#define ELEMENTS_RESERVED_FOR_MAIN_PART 4
/* This is just used to prealloc a dynamic array */
#define AVERAGE_BLOB_SIZE      1024L*1024L
/* Number of pages to store continuous blob parts */
#define BLOB_SEGMENT_MIN_SIZE 128

/* Fields before 'row->null_field_lengths' used by find_where_to_split_row */
#define EXTRA_LENGTH_FIELDS		3

/* Size for the different parts in the row header (and head page) */
#define FLAG_SIZE		1
#define VERPTR_SIZE		7
#define DIR_ENTRY_SIZE		4
#define FIELD_OFFSET_SIZE	2      /* size of pointers to field starts */

/* Minimum header size needed for a new row */
#define BASE_ROW_HEADER_SIZE FLAG_SIZE
#define TRANS_ROW_EXTRA_HEADER_SIZE TRANSID_SIZE

#define PAGE_TYPE_MASK 7
enum en_page_type { UNALLOCATED_PAGE, HEAD_PAGE, TAIL_PAGE, BLOB_PAGE, MAX_PAGE_TYPE };
#define PAGE_CAN_BE_COMPACTED   128             /* Bit in PAGE_TYPE */

#define PAGE_TYPE_OFFSET        LSN_SIZE
#define DIR_COUNT_OFFSET        (LSN_SIZE+PAGE_TYPE_SIZE)
#define DIR_FREE_OFFSET         (DIR_COUNT_OFFSET+DIR_COUNT_SIZE)
#define EMPTY_SPACE_OFFSET      (DIR_FREE_OFFSET+DIR_FREE_SIZE)

/* Bits used for flag uchar (one byte, first in record) */
#define ROW_FLAG_TRANSID                1
#define ROW_FLAG_VER_PTR                2
#define ROW_FLAG_DELETE_TRANSID         4
#define ROW_FLAG_NULLS_EXTENDED         8
#define ROW_FLAG_EXTENTS                128
#define ROW_FLAG_ALL			(1+2+4+8+128)

/* Size for buffer to hold information about bitmap */
#define MAX_BITMAP_INFO_LENGTH ((MARIA_MAX_KEY_BLOCK_LENGTH*8/3)*(61*11/60)+10)


/******** Variables that affects how data pages are utilized ********/

/* Minium size of tail segment */
#define MIN_TAIL_SIZE           32

/*
  Fixed length part of Max possible header size; See row data structure
  table in ma_blockrec.c.
*/
#define MAX_FIXED_HEADER_SIZE (FLAG_SIZE + 3 + ROW_EXTENT_SIZE + 3)
#define TRANS_MAX_FIXED_HEADER_SIZE (MAX_FIXED_HEADER_SIZE + \
                                     TRANSID_SIZE + VERPTR_SIZE + \
                                     TRANSID_SIZE)

/* We use 1 uchar in record header to store number of directory entries */
#define MAX_ROWS_PER_PAGE	255
#define END_OF_DIR_FREE_LIST	((uchar) 255)

/* Bits for MARIA_BITMAP_BLOCKS->used */
/* We stored data on disk in the block */
#define BLOCKUSED_USED		 1
/* Bitmap on disk is block->org_bitmap_value ; Happens only on update */
#define BLOCKUSED_USE_ORG_BITMAP 2
/* We stored tail data on disk for the block */
#define BLOCKUSED_TAIL		 4

/******* defines that affects allocation (density) of data *******/

/*
  If the tail part (from the main block or a blob) would use more than 75 % of
  the size of page, store the tail on a full page instead of a shared
 tail page.
*/
#define MAX_TAIL_SIZE(block_size) ((block_size) *3 / 4)

/* Don't allocate memory for too many row extents on the stack */
#define ROW_EXTENTS_ON_STACK	32

/* Functions to convert MARIA_RECORD_POS to/from page:offset */

static inline MARIA_RECORD_POS ma_recordpos(pgcache_page_no_t page,
                                            uint dir_entry)
{
  DBUG_ASSERT(dir_entry <= 255);
  DBUG_ASSERT(page > 0); /* page 0 is bitmap, not data page */
  return (MARIA_RECORD_POS) (((ulonglong) page << 8) | dir_entry);
}

static inline pgcache_page_no_t ma_recordpos_to_page(MARIA_RECORD_POS record_pos)
{
  return (pgcache_page_no_t) (record_pos >> 8);
}

static inline uint ma_recordpos_to_dir_entry(MARIA_RECORD_POS record_pos)
{
  return (uint) (record_pos & 255);
}

static inline uchar *dir_entry_pos(uchar *buff, uint block_size, uint pos)
{
  return (buff + block_size - DIR_ENTRY_SIZE * pos - PAGE_SUFFIX_SIZE -
          DIR_ENTRY_SIZE);
}

/* ma_blockrec.c */
void _ma_init_block_record_data(void);
my_bool _ma_once_init_block_record(MARIA_SHARE *share, File dfile);
my_bool _ma_once_end_block_record(MARIA_SHARE *share);
my_bool _ma_init_block_record(MARIA_HA *info);
void _ma_end_block_record(MARIA_HA *info);

my_bool _ma_update_block_record(MARIA_HA *info, MARIA_RECORD_POS pos,
                                const uchar *oldrec, const uchar *newrec);
my_bool _ma_delete_block_record(MARIA_HA *info, const uchar *record);
int     _ma_read_block_record(MARIA_HA *info, uchar *record,
                              MARIA_RECORD_POS record_pos);
int _ma_read_block_record2(MARIA_HA *info, uchar *record,
                           uchar *data, uchar *end_of_data);
int     _ma_scan_block_record(MARIA_HA *info, uchar *record,
                              MARIA_RECORD_POS, my_bool);
my_bool _ma_cmp_block_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                             const uchar *record, MARIA_RECORD_POS pos);
my_bool _ma_scan_init_block_record(MARIA_HA *info);
void _ma_scan_end_block_record(MARIA_HA *info);
int _ma_scan_remember_block_record(MARIA_HA *info,
                                   MARIA_RECORD_POS *lastpos);
void _ma_scan_restore_block_record(MARIA_HA *info,
                                   MARIA_RECORD_POS lastpos);

MARIA_RECORD_POS _ma_write_init_block_record(MARIA_HA *info,
                                             const uchar *record);
my_bool _ma_write_block_record(MARIA_HA *info, const uchar *record);
my_bool _ma_write_abort_block_record(MARIA_HA *info);
my_bool _ma_compare_block_record(register MARIA_HA *info,
                                 register const uchar *record);
void    _ma_compact_block_page(uchar *buff, uint block_size, uint rownr,
                               my_bool extend_block, TrID min_read_from,
                               uint min_row_length);
my_bool enough_free_entries_on_page(MARIA_SHARE *share, uchar *page_buff);
TRANSLOG_ADDRESS
maria_page_get_lsn(uchar *page, pgcache_page_no_t page_no, uchar* data_ptr);

/* ma_bitmap.c */
extern const char *bits_to_txt[];

my_bool _ma_bitmap_init(MARIA_SHARE *share, File file);
my_bool _ma_bitmap_end(MARIA_SHARE *share);
my_bool _ma_bitmap_flush(MARIA_SHARE *share);
my_bool _ma_bitmap_flush_all(MARIA_SHARE *share);
void _ma_bitmap_reset_cache(MARIA_SHARE *share);
my_bool _ma_bitmap_find_place(MARIA_HA *info, MARIA_ROW *row,
                              MARIA_BITMAP_BLOCKS *result_blocks);
my_bool _ma_bitmap_release_unused(MARIA_HA *info, MARIA_BITMAP_BLOCKS *blocks);
my_bool _ma_bitmap_free_full_pages(MARIA_HA *info, const uchar *extents,
                                   uint count);
my_bool _ma_bitmap_set(MARIA_HA *info, pgcache_page_no_t pos, my_bool head,
                       uint empty_space);
my_bool _ma_bitmap_reset_full_page_bits(MARIA_HA *info,
                                        MARIA_FILE_BITMAP *bitmap,
                                        pgcache_page_no_t page,
                                        uint page_count);
my_bool _ma_bitmap_set_full_page_bits(MARIA_HA *info,
                                      MARIA_FILE_BITMAP *bitmap,
                                      pgcache_page_no_t page, uint page_count);
uint _ma_free_size_to_head_pattern(MARIA_FILE_BITMAP *bitmap, uint size);
my_bool _ma_bitmap_find_new_place(MARIA_HA *info, MARIA_ROW *new_row,
                                  pgcache_page_no_t page, uint free_size,
                                  MARIA_BITMAP_BLOCKS *result_blocks);
my_bool _ma_check_bitmap_data(MARIA_HA *info,
                              enum en_page_type page_type,
                              uint empty_space, uint bitmap_pattern);
my_bool _ma_check_if_right_bitmap_type(MARIA_HA *info,
                                       enum en_page_type page_type,
                                       pgcache_page_no_t page,
                                       uint *bitmap_pattern);
uint _ma_bitmap_get_page_bits(MARIA_HA *info, MARIA_FILE_BITMAP *bitmap,
                              pgcache_page_no_t page);
void _ma_bitmap_delete_all(MARIA_SHARE *share);
int  _ma_bitmap_create_first(MARIA_SHARE *share);
void _ma_bitmap_flushable(MARIA_HA *info, int non_flushable_inc);
void _ma_bitmap_lock(MARIA_SHARE *share);
void _ma_bitmap_unlock(MARIA_SHARE *share);
void _ma_bitmap_set_pagecache_callbacks(PAGECACHE_FILE *file,
                                        MARIA_SHARE *share);
#ifndef DBUG_OFF
void _ma_print_bitmap(MARIA_FILE_BITMAP *bitmap, uchar *data,
                      pgcache_page_no_t page);
#endif
void _ma_get_bitmap_description(MARIA_FILE_BITMAP *bitmap,
                                uchar *bitmap_data,
                                pgcache_page_no_t page,
                                char *out);

uint _ma_apply_redo_insert_row_head_or_tail(MARIA_HA *info, LSN lsn,
                                            uint page_type,
                                            my_bool new_page,
                                            const uchar *header,
                                            const uchar *data,
                                            size_t data_length);
uint _ma_apply_redo_purge_row_head_or_tail(MARIA_HA *info, LSN lsn,
                                           uint page_type,
                                           const uchar *header);
uint _ma_apply_redo_free_blocks(MARIA_HA *info, LSN lsn, LSN rec_lsn,
                                const uchar *header);
uint _ma_apply_redo_free_head_or_tail(MARIA_HA *info, LSN lsn,
                                      const uchar *header);
uint _ma_apply_redo_insert_row_blobs(MARIA_HA *info, LSN lsn,
                                     const uchar *header, LSN redo_lsn,
                                     uint * const number_of_blobs,
                                     uint * const number_of_ranges,
                                     pgcache_page_no_t * const first_page,
                                     pgcache_page_no_t * const last_page);
my_bool _ma_apply_redo_bitmap_new_page(MARIA_HA *info, LSN lsn,
                                       const uchar *header);
my_bool _ma_apply_undo_row_insert(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header);
my_bool _ma_apply_undo_row_delete(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, size_t length);
my_bool _ma_apply_undo_row_update(MARIA_HA *info, LSN undo_lsn,
                                  const uchar *header, size_t length);
my_bool _ma_apply_undo_bulk_insert(MARIA_HA *info, LSN undo_lsn);

my_bool write_hook_for_redo(enum translog_record_type type,
                            TRN *trn, MARIA_HA *tbl_info, LSN *lsn,
                            void *hook_arg);
my_bool write_hook_for_undo(enum translog_record_type type,
                            TRN *trn, MARIA_HA *tbl_info, LSN *lsn,
                            void *hook_arg);
my_bool write_hook_for_redo_delete_all(enum translog_record_type type,
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg);
my_bool write_hook_for_undo_row_insert(enum translog_record_type type,
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg);
my_bool write_hook_for_undo_row_delete(enum translog_record_type type,
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg);
my_bool write_hook_for_undo_row_update(enum translog_record_type type,
                                       TRN *trn, MARIA_HA *tbl_info,
                                       LSN *lsn, void *hook_arg);
my_bool write_hook_for_undo_bulk_insert(enum translog_record_type type,
                                        TRN *trn, MARIA_HA *tbl_info,
                                        LSN *lsn, void *hook_arg);
my_bool write_hook_for_file_id(enum translog_record_type type,
                               TRN *trn, MARIA_HA *tbl_info, LSN *lsn,
                               void *hook_arg);
my_bool write_hook_for_commit(enum translog_record_type type,
                              TRN *trn, MARIA_HA *tbl_info, LSN *lsn,
                              void *hook_arg);
void _ma_block_get_status(void *param, my_bool concurrent_insert);
my_bool _ma_block_start_trans(void* param);
my_bool _ma_block_start_trans_no_versioning(void *param);
void _ma_block_update_status(void *param);
void _ma_block_restore_status(void *param);
my_bool _ma_block_check_status(void *param);
