/*****************************************************************************

Copyright (c) 1994, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/page0types.h
 Index page routines

 Created 2/2/1994 Heikki Tuuri
 *******************************************************/

#ifndef page0types_h
#define page0types_h

#include "dict0types.h"
#include "fsp0types.h"
#include "mtr0types.h"
#include "univ.i"
#include "ut0new.h"

#include <map>

/*                      PAGE HEADER
                        ===========

Index page header starts at the first offset left free by the FIL-module */

typedef byte page_header_t;

/** index page header starts at this offset */
constexpr uint32_t PAGE_HEADER = FSEG_PAGE_DATA;

/*-----------------------------*/
/** number of slots in page directory */
constexpr uint32_t PAGE_N_DIR_SLOTS = 0;
/** pointer to record heap top */
constexpr uint32_t PAGE_HEAP_TOP = 2;
/** number of records in the heap, bit 15=flag: new-style compact page format */
constexpr uint32_t PAGE_N_HEAP = 4;
/** pointer to start of page free record list */
constexpr uint32_t PAGE_FREE = 6;
/** number of bytes in deleted records */
constexpr uint32_t PAGE_GARBAGE = 8;
/** pointer to the last inserted record, or NULL if this info has been reset by
 a delete, for example */
constexpr uint32_t PAGE_LAST_INSERT = 10;
/** last insert direction: PAGE_LEFT, ... */
constexpr uint32_t PAGE_DIRECTION = 12;
/** number of consecutive inserts to the same direction */
constexpr uint32_t PAGE_N_DIRECTION = 14;
/** number of user records on the page */
constexpr uint32_t PAGE_N_RECS = 16;
/** highest id of a trx which may have modified a record on the page; trx_id_t;
defined only in secondary indexes and in the insert buffer tree */
constexpr uint32_t PAGE_MAX_TRX_ID = 18;
/** end of private data structure of the page header which are set in a page
create */
constexpr uint32_t PAGE_HEADER_PRIV_END = 26;
/*----*/
/** level of the node in an index tree; the leaf level is the level 0.
This field should not be written to after page creation. */
constexpr uint32_t PAGE_LEVEL = 26;
/** index id where the page belongs. This field should not be written to after
 page creation. */
constexpr uint32_t PAGE_INDEX_ID = 28;
/** file segment header for the leaf pages in a B-tree: defined only on the root
 page of a B-tree, but not in the root of an ibuf tree */
constexpr uint32_t PAGE_BTR_SEG_LEAF = 36;
constexpr uint32_t PAGE_BTR_IBUF_FREE_LIST = PAGE_BTR_SEG_LEAF;
constexpr uint32_t PAGE_BTR_IBUF_FREE_LIST_NODE = PAGE_BTR_SEG_LEAF;
/* in the place of PAGE_BTR_SEG_LEAF and _TOP
there is a free list base node if the page is
the root page of an ibuf tree, and at the same
place is the free list node if the page is in
a free list */
constexpr uint32_t PAGE_BTR_SEG_TOP = 36 + FSEG_HEADER_SIZE;
/* file segment header for the non-leaf pages
in a B-tree: defined only on the root page of
a B-tree, but not in the root of an ibuf
tree */
/*----*/
/** start of data on the page */
constexpr uint32_t PAGE_DATA = PAGE_HEADER + 36 + 2 * FSEG_HEADER_SIZE;

/** offset of the page infimum record on an
old-style page */
#define PAGE_OLD_INFIMUM (PAGE_DATA + 1 + REC_N_OLD_EXTRA_BYTES)

/** offset of the page supremum record on an
old-style page */
#define PAGE_OLD_SUPREMUM (PAGE_DATA + 2 + 2 * REC_N_OLD_EXTRA_BYTES + 8)

/** offset of the page supremum record end on an old-style page */
#define PAGE_OLD_SUPREMUM_END (PAGE_OLD_SUPREMUM + 9)

/** offset of the page infimum record on a new-style compact page */
#define PAGE_NEW_INFIMUM (PAGE_DATA + REC_N_NEW_EXTRA_BYTES)

/** offset of the page supremum record on a new-style compact page */
#define PAGE_NEW_SUPREMUM (PAGE_DATA + 2 * REC_N_NEW_EXTRA_BYTES + 8)

/** offset of the page supremum record end on a new-style compact page */
#define PAGE_NEW_SUPREMUM_END (PAGE_NEW_SUPREMUM + 8)

/*-----------------------------*/

/* Heap numbers */
/** Page infimum */
constexpr ulint PAGE_HEAP_NO_INFIMUM = 0;
/** Page supremum */
constexpr ulint PAGE_HEAP_NO_SUPREMUM = 1;

/** First user record in creation (insertion) order, not necessarily collation
order; this record may have been deleted */
constexpr ulint PAGE_HEAP_NO_USER_LOW = 2;

/* Directions of cursor movement */

enum cursor_direction_t : uint8_t {
  PAGE_LEFT = 1,
  PAGE_RIGHT = 2,
  PAGE_SAME_REC = 3,
  PAGE_SAME_PAGE = 4,
  PAGE_NO_DIRECTION = 5
};

/** Eliminates a name collision on HP-UX */
#define page_t ib_page_t
/** Type of the index page */
typedef byte page_t;
/** Index page cursor */
struct page_cur_t;

/** Compressed index page */
typedef byte page_zip_t;

/* The following definitions would better belong to page0zip.h,
but we cannot include page0zip.h from rem0rec.ic, because
page0*.h includes rem0rec.h and may include rem0rec.ic. */

/** Number of bits needed for representing different compressed page sizes */
constexpr uint8_t PAGE_ZIP_SSIZE_BITS = 3;

/** Maximum compressed page shift size */
constexpr uint32_t PAGE_ZIP_SSIZE_MAX =
    UNIV_ZIP_SIZE_SHIFT_MAX - UNIV_ZIP_SIZE_SHIFT_MIN + 1;

/* Make sure there are enough bits available to store the maximum zip
ssize, which is the number of shifts from 512. */
static_assert(PAGE_ZIP_SSIZE_MAX < (1 << PAGE_ZIP_SSIZE_BITS),
              "PAGE_ZIP_SSIZE_MAX >= (1 << PAGE_ZIP_SSIZE_BITS)");

/* Page cursor search modes; the values must be in this order! */
enum page_cur_mode_t {
  PAGE_CUR_UNSUPP = 0,
  PAGE_CUR_G = 1,
  PAGE_CUR_GE = 2,
  PAGE_CUR_L = 3,
  PAGE_CUR_LE = 4,

  /*      PAGE_CUR_LE_OR_EXTENDS = 5,*/ /* This is a search mode used in
                                   "column LIKE 'abc%' ORDER BY column DESC";
                                   we have to find strings which are <= 'abc' or
                                   which extend it */

  /* These search mode is for search R-tree index. */
  PAGE_CUR_CONTAIN = 7,
  PAGE_CUR_INTERSECT = 8,
  PAGE_CUR_WITHIN = 9,
  PAGE_CUR_DISJOINT = 10,
  PAGE_CUR_MBR_EQUAL = 11,
  PAGE_CUR_RTREE_INSERT = 12,
  PAGE_CUR_RTREE_LOCATE = 13,
  PAGE_CUR_RTREE_GET_FATHER = 14
};

/** Compressed page descriptor */
struct page_zip_des_t {
  /** Compressed page data */
  page_zip_t *data;

#ifdef UNIV_DEBUG
  /** Start offset of modification log */
  uint16_t m_start;
  /** Allocated externally, not from the buffer pool */
  bool m_external;
#endif /* UNIV_DEBUG */

  /** End offset of modification log */
  uint16_t m_end;

  /** Number of externally stored columns on the page; the maximum is 744
  on a 16 KiB page */
  uint16_t n_blobs;

  /** true if the modification log is not empty.  */
  bool m_nonempty;

  /** 0 or compressed page shift size; the size in bytes is:
  (UNIV_ZIP_SIZE_MIN * >> 1) << ssize. */
  uint8_t ssize;
};

/** Compression statistics for a given page size */
struct page_zip_stat_t {
  /** Number of page compressions */
  ulint compressed;
  /** Number of successful page compressions */
  ulint compressed_ok;
  /** Number of page decompressions */
  ulint decompressed;
  /** Duration of page compressions */
  std::chrono::microseconds compress_time;
  /** Duration of page decompressions */
  std::chrono::microseconds decompress_time;
  page_zip_stat_t()
      : /* Initialize members to 0 so that when we do
        stlmap[key].compressed++ and element with "key" does not
        exist it gets inserted with zeroed members. */
        compressed(0),
        compressed_ok(0),
        decompressed(0),
        compress_time{},
        decompress_time{} {}
};

/** Compression statistics types */
typedef std::map<index_id_t, page_zip_stat_t, std::less<index_id_t>,
                 ut::allocator<std::pair<const index_id_t, page_zip_stat_t>>>
    page_zip_stat_per_index_t;

/** Statistics on compression, indexed by page_zip_des_t::ssize - 1 */
extern page_zip_stat_t page_zip_stat[PAGE_ZIP_SSIZE_MAX];
/** Statistics on compression, indexed by dict_index_t::id */
extern page_zip_stat_per_index_t page_zip_stat_per_index;

/** Write the "deleted" flag of a record on a compressed page.  The flag must
 already have been written on the uncompressed page. */
void page_zip_rec_set_deleted(
    page_zip_des_t *page_zip, /*!< in/out: compressed page */
    const byte *rec,          /*!< in: record on the uncompressed page */
    bool flag);               /*!< in: the deleted flag (nonzero=true) */

/** Write the "owned" flag of a record on a compressed page.  The n_owned field
 must already have been written on the uncompressed page.
@param[in,out] page_zip Compressed page
@param[in] rec Record on the uncompressed page
@param[in] flag The owned flag (nonzero=true) */
void page_zip_rec_set_owned(page_zip_des_t *page_zip, const byte *rec,
                            ulint flag);

/** Shift the dense page directory when a record is deleted.
@param[in,out]  page_zip        compressed page
@param[in]      rec             deleted record
@param[in]      index           index of rec
@param[in]      offsets         rec_get_offsets(rec)
@param[in]      free            previous start of the free list */
void page_zip_dir_delete(page_zip_des_t *page_zip, byte *rec,
                         dict_index_t *index, const ulint *offsets,
                         const byte *free);

/** Add a slot to the dense page directory.
@param[in,out]  page_zip      Compressed page
@param[in]      is_clustered  Nonzero for clustered index, zero for others */
void page_zip_dir_add_slot(page_zip_des_t *page_zip, bool is_clustered);

#endif
