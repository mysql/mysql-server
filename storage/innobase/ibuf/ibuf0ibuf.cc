/*****************************************************************************

Copyright (c) 1997, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file ibuf/ibuf0ibuf.cc
 Insert buffer

 Created 7/19/1997 Heikki Tuuri
 *******************************************************/

#include <sys/types.h>

#include "btr0sea.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "sync0sync.h"

#include <debug_sync.h>
#include "my_dbug.h"

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
bool srv_ibuf_disable_background_merge;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** Number of bits describing a single page */
constexpr size_t IBUF_BITS_PER_PAGE = 4;
static_assert(IBUF_BITS_PER_PAGE % 2 == 0,
              "IBUF_BITS_PER_PAGE must be an even number!");
/** The start address for an insert buffer bitmap page bitmap */
constexpr uint32_t IBUF_BITMAP = PAGE_DATA;

#ifndef UNIV_HOTBACKUP

#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0pcur.h"
#include "buf0buf.h"
#include "buf0rea.h"
#include "dict0boot.h"
#include "fil0fil.h"
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "fut0lst.h"
#include "lock0lock.h"
#include "log0buf.h"
#include "log0chkp.h"
#include "log0recv.h"
#include "que0que.h"
#include "rem0cmp.h"
#include "rem0rec.h"
#include "row0upd.h"
#include "srv0start.h"
#include "trx0sys.h"

/*      STRUCTURE OF AN INSERT BUFFER RECORD

In versions < 4.1.x:

1. The first field is the page number.
2. The second field is an array which stores type info for each subsequent
   field. We store the information which affects the ordering of records, and
   also the physical storage size of an SQL NULL value. E.g., for CHAR(10) it
   is 10 bytes.
3. Next we have the fields of the actual index record.

In versions >= 4.1.x:

Note that contrary to what we planned in the 1990's, there will only be one
insert buffer tree, and that is in the system tablespace of InnoDB.

1. The first field is the space id.
2. The second field is a one-byte marker (0) which differentiates records from
   the < 4.1.x storage format.
3. The third field is the page number.
4. The fourth field contains the type info, where we have also added 2 bytes to
   store the charset. In the compressed table format of 5.0.x we must add more
   information here so that we can build a dummy 'index' struct which 5.0.x
   can use in the binary search on the index page in the ibuf merge phase.
5. The rest of the fields contain the fields of the actual index record.

In versions >= 5.0.3:

The first byte of the fourth field is an additional marker (0) if the record
is in the compact format.  The presence of this marker can be detected by
looking at the length of the field modulo DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE.

The high-order bit of the character set field in the type info is the
"nullable" flag for the field.

In versions >= 5.5:

The optional marker byte at the start of the fourth field is replaced by
mandatory 3 fields, totaling 4 bytes:

 1. 2 bytes: Counter field, used to sort records within a (space id, page
    no) in the order they were added. This is needed so that for example the
    sequence of operations "INSERT x, DEL MARK x, INSERT x" is handled
    correctly.

 2. 1 byte: Operation type (see ibuf_op_t).

 3. 1 byte: Flags. Currently only one flag exists, IBUF_REC_COMPACT.

To ensure older records, which do not have counters to enforce correct
sorting, are merged before any new records, ibuf_insert checks if we're
trying to insert to a position that contains old-style records, and if so,
refuses the insert. Thus, ibuf pages are gradually converted to the new
format as their corresponding buffer pool pages are read into memory.
*/

/*      PREVENTING DEADLOCKS IN THE INSERT BUFFER SYSTEM

If an OS thread performs any operation that brings in disk pages from
non-system tablespaces into the buffer pool, or creates such a page there,
then the operation may have as a side effect an insert buffer index tree
compression. Thus, the tree latch of the insert buffer tree may be acquired
in the x-mode, and also the file space latch of the system tablespace may
be acquired in the x-mode.

Also, an insert to an index in a non-system tablespace can have the same
effect. How do we know this cannot lead to a deadlock of OS threads? There
is a problem with the i\o-handler threads: they break the latching order
because they own x-latches to pages which are on a lower level than the
insert buffer tree latch, its page latches, and the tablespace latch an
insert buffer operation can reserve.

The solution is the following: Let all the tree and page latches connected
with the insert buffer be later in the latching order than the fsp latch and
fsp page latches.

Insert buffer pages must be such that the insert buffer is never invoked
when these pages are accessed as this would result in a recursion violating
the latching order. We let a special i/o-handler thread take care of i/o to
the insert buffer pages and the ibuf bitmap pages, as well as the fsp bitmap
pages and the first inode page, which contains the inode of the ibuf tree: let
us call all these ibuf pages. To prevent deadlocks, we do not let a read-ahead
access both non-ibuf and ibuf pages.

Then an i/o-handler for the insert buffer never needs to access recursively the
insert buffer tree and thus obeys the latching order. On the other hand, other
i/o-handlers for other tablespaces may require access to the insert buffer,
but because all kinds of latches they need to access there are later in the
latching order, no violation of the latching order occurs in this case,
either.

A problem is how to grow and contract an insert buffer tree. As it is later
in the latching order than the fsp management, we have to reserve the fsp
latch first, before adding or removing pages from the insert buffer tree.
We let the insert buffer tree have its own file space management: a free
list of pages linked to the tree root. To prevent recursive using of the
insert buffer when adding pages to the tree, we must first load these pages
to memory, obtaining a latch on them, and only after that add them to the
free list of the insert buffer tree. More difficult is removing of pages
from the free list. If there is an excess of pages in the free list of the
ibuf tree, they might be needed if some thread reserves the fsp latch,
intending to allocate more file space. So we do the following: if a thread
reserves the fsp latch, we check the writer count field of the latch. If
this field has value 1, it means that the thread did not own the latch
before entering the fsp system, and the mtr of the thread contains no
modifications to the fsp pages. Now we are free to reserve the ibuf latch,
and check if there is an excess of pages in the free list. We can then, in a
separate mini-transaction, take them out of the free list and free them to
the fsp system.

To avoid deadlocks in the ibuf system, we divide file pages into three levels:

(1) non-ibuf pages,
(2) ibuf tree pages and the pages in the ibuf tree free list, and
(3) ibuf bitmap pages.

No OS thread is allowed to access higher level pages if it has latches to
lower level pages; even if the thread owns a B-tree latch it must not access
the B-tree non-leaf pages if it has latches on lower level pages. Read-ahead
is only allowed for level 1 and 2 pages. Dedicated i/o-handler threads handle
exclusively level 1 i/o. A dedicated i/o handler thread handles exclusively
level 2 i/o. However, if an OS thread does the i/o handling for itself, i.e.,
it uses synchronous aio, it can access any pages, as long as it obeys the
access order rules. */

/** Operations that can currently be buffered. */
ulong innodb_change_buffering = IBUF_USE_ALL;

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Flag to control insert buffer debugging. */
uint ibuf_debug;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** The insert buffer control structure */
ibuf_t *ibuf = nullptr;

#ifdef UNIV_IBUF_COUNT_DEBUG
/** Number of tablespaces in the ibuf_counts array */
constexpr uint32_t IBUF_COUNT_N_SPACES = 4;
/** Number of pages within each tablespace in the ibuf_counts array */
constexpr uint32_t IBUF_COUNT_N_PAGES = 130000;

/** Buffered entry counts for file pages, used in debugging */
static ulint ibuf_counts[IBUF_COUNT_N_SPACES][IBUF_COUNT_N_PAGES];

/** Checks that the indexes to ibuf_counts[][] are within limits.
@param[in]      page_id page id */
static inline void ibuf_count_check(const page_id_t &page_id) {
  if (page_id.space() < IBUF_COUNT_N_SPACES &&
      page_id.page_no() < IBUF_COUNT_N_PAGES) {
    return;
  }

  ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_605)
      << "UNIV_IBUF_COUNT_DEBUG limits space_id and page_no"
         " and breaks crash recovery. space_id="
      << page_id.space() << ", should be 0<=space_id<" << IBUF_COUNT_N_SPACES
      << ". page_no=" << page_id.page_no() << ", should be 0<=page_no<"
      << IBUF_COUNT_N_PAGES;
}
#endif

/** @name Offsets to the per-page bits in the insert buffer bitmap */
/** @{ */
/** Bits indicating the amount of free space */
constexpr uint32_t IBUF_BITMAP_FREE = 0;
/** true if there are buffered changes for the page */
constexpr uint32_t IBUF_BITMAP_BUFFERED = 2;
/** true if page is a part of  the ibuf tree, excluding the root page, or is in
 the free list of the ibuf */
constexpr uint32_t IBUF_BITMAP_IBUF = 3;
/** @} */

/** in the pre-4.1 format, the page number. later, the space_id */
constexpr uint32_t IBUF_REC_FIELD_SPACE = 0;
/** starting with 4.1, a marker consisting of 1 byte that is 0 */
constexpr uint32_t IBUF_REC_FIELD_MARKER = 1;
/** starting with 4.1, the page number */
constexpr uint32_t IBUF_REC_FIELD_PAGE = 2;
/** the metadata field */
constexpr uint32_t IBUF_REC_FIELD_METADATA = 3;
/** first user field */
constexpr uint32_t IBUF_REC_FIELD_USER = 4;

/* Various constants for checking the type of an ibuf record and extracting
data from it. For details, see the description of the record format at the
top of this file. */

/** @name Format of the IBUF_REC_FIELD_METADATA of an insert buffer record
The fourth column in the MySQL 5.5 format contains an operation
type, counter, and some flags. */
/** Combined size of info fields at the beginning of the fourth field */
constexpr uint32_t IBUF_REC_INFO_SIZE = 4;
static_assert(IBUF_REC_INFO_SIZE < DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE,
              "IBUF_REC_INFO_SIZE >= DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE");

/* Offsets for the fields at the beginning of the fourth field */
/** Operation counter */
constexpr uint32_t IBUF_REC_OFFSET_COUNTER = 0;
/** Type of operation */
constexpr uint32_t IBUF_REC_OFFSET_TYPE = 2;
/** Additional flags */
constexpr uint32_t IBUF_REC_OFFSET_FLAGS = 3;

/* Record flag masks */
/** Set in IBUF_REC_OFFSET_FLAGS if the user index is in COMPACT format or later
 */
constexpr uint32_t IBUF_REC_COMPACT = 0x1;
/** The mutex used to block pessimistic inserts to ibuf trees */
static ib_mutex_t ibuf_pessimistic_insert_mutex;

/** The mutex protecting the insert buffer structs */
static ib_mutex_t ibuf_mutex;

/** The mutex protecting the insert buffer bitmaps */
static ib_mutex_t ibuf_bitmap_mutex;

/** The area in pages from which contract looks for page numbers for merge */
const ulint IBUF_MERGE_AREA = 8;

/** Inside the merge area, pages which have at most 1 per this number less
buffered entries compared to maximum volume that can buffered for a single
page are merged along with the page whose buffer became full */
const ulint IBUF_MERGE_THRESHOLD = 4;

/** In ibuf_contract at most this number of pages is read to memory in one
batch, in order to merge the entries for them in the insert buffer */
const ulint IBUF_MAX_N_PAGES_MERGED = IBUF_MERGE_AREA;

/** If the combined size of the ibuf trees exceeds ibuf->max_size by this
many pages, we start to contract it in connection to inserts there, using
non-synchronous contract */
const ulint IBUF_CONTRACT_ON_INSERT_NON_SYNC = 0;

/** If the combined size of the ibuf trees exceeds ibuf->max_size by this
many pages, we start to contract it in connection to inserts there, using
synchronous contract */
const ulint IBUF_CONTRACT_ON_INSERT_SYNC = 5;

/** If the combined size of the ibuf trees exceeds ibuf->max_size by
this many pages, we start to contract it synchronous contract, but do
not insert */
const ulint IBUF_CONTRACT_DO_NOT_INSERT = 10;

/* TODO: how to cope with drop table if there are records in the insert
buffer for the indexes of the table? Is there actually any problem,
because ibuf merge is done to a page when it is read in, and it is
still physically like the index page even if the index would have been
dropped! So, there seems to be no problem. */

/** Sets the flag in the current mini-transaction record indicating we're
 inside an insert buffer routine. */
static inline void ibuf_enter(mtr_t *mtr) /*!< in/out: mini-transaction */
{
  ut_ad(!mtr->is_inside_ibuf());
  mtr->enter_ibuf();
}

/** Sets the flag in the current mini-transaction record indicating we're
 exiting an insert buffer routine. */
static inline void ibuf_exit(mtr_t *mtr) /*!< in/out: mini-transaction */
{
  ut_ad(mtr->is_inside_ibuf());
  mtr->exit_ibuf();
}

/** Commits an insert buffer mini-transaction and sets the persistent
 cursor latch mode to BTR_NO_LATCHES, that is, detaches the cursor. */
static inline void ibuf_btr_pcur_commit_specify_mtr(
    btr_pcur_t *pcur, /*!< in/out: persistent cursor */
    mtr_t *mtr)       /*!< in/out: mini-transaction */
{
  ut_d(ibuf_exit(mtr));
  pcur->commit_specify_mtr(mtr);
}

/** Gets the ibuf header page and x-latches it.
 @return insert buffer header page */
static page_t *ibuf_header_page_get(mtr_t *mtr) /*!< in/out: mini-transaction */
{
  buf_block_t *block;

  ut_ad(!ibuf_inside(mtr));

  block = buf_page_get(page_id_t(IBUF_SPACE_ID, FSP_IBUF_HEADER_PAGE_NO),
                       univ_page_size, RW_X_LATCH, UT_LOCATION_HERE, mtr);

  buf_block_dbg_add_level(block, SYNC_IBUF_HEADER);

  return (buf_block_get_frame(block));
}

/** Gets the root page and sx-latches it.
 @return insert buffer tree root page */
static page_t *ibuf_tree_root_get(mtr_t *mtr) /*!< in: mtr */
{
  buf_block_t *block;
  page_t *root;

  ut_ad(ibuf_inside(mtr));
  ut_ad(mutex_own(&ibuf_mutex));

  mtr_sx_lock(dict_index_get_lock(ibuf->index), mtr, UT_LOCATION_HERE);

  /* only segment list access is exclusive each other */
  block = buf_page_get(page_id_t(IBUF_SPACE_ID, FSP_IBUF_TREE_ROOT_PAGE_NO),
                       univ_page_size, RW_SX_LATCH, UT_LOCATION_HERE, mtr);

  buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE_NEW);

  root = buf_block_get_frame(block);

  ut_ad(page_get_space_id(root) == IBUF_SPACE_ID);
  ut_ad(page_get_page_no(root) == FSP_IBUF_TREE_ROOT_PAGE_NO);
  ut_ad(ibuf->empty == page_is_empty(root));

  return (root);
}

#ifdef UNIV_IBUF_COUNT_DEBUG

/** Gets the ibuf count for a given page.
@param[in]      page_id page id
@return number of entries in the insert buffer currently buffered for
this page */
ulint ibuf_count_get(const page_id_t &page_id) {
  ibuf_count_check(page_id);

  return (ibuf_counts[page_id.space()][page_id.page_no()]);
}

/** Sets the ibuf count for a given page.
@param[in]      page_id page id
@param[in]      val     value to set */
static void ibuf_count_set(const page_id_t &page_id, ulint val) {
  ibuf_count_check(page_id);
  ut_a(val < UNIV_PAGE_SIZE);

  ibuf_counts[page_id.space()][page_id.page_no()] = val;
}
#endif

/** Closes insert buffer and frees the data structures. */
void ibuf_close(void) {
  if (ibuf == nullptr) {
    return;
  }

  mutex_free(&ibuf_pessimistic_insert_mutex);

  mutex_free(&ibuf_mutex);

  mutex_free(&ibuf_bitmap_mutex);

  dict_table_t *ibuf_table = ibuf->index->table;
  rw_lock_free(&ibuf->index->lock);
  dict_mem_index_free(ibuf->index);
  dict_mem_table_free(ibuf_table);

  ut::free(ibuf);
  ibuf = nullptr;
}

/** Updates the size information of the ibuf, assuming the segment size has not
 changed. */
static void ibuf_size_update(const page_t *root) /*!< in: ibuf tree root */
{
  ut_ad(mutex_own(&ibuf_mutex));

  ibuf->free_list_len =
      flst_get_len(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST);

  ibuf->height = 1 + btr_page_get_level(root);

  /* the '1 +' is the ibuf header page */
  ibuf->size = ibuf->seg_size - (1 + ibuf->free_list_len);
}

/** Creates the insert buffer data structure at a database startup and
 initializes the data structures for the insert buffer. */
void ibuf_init_at_db_start(void) {
  page_t *root;
  mtr_t mtr;
  ulint n_used;
  page_t *header_page;

  ibuf = static_cast<ibuf_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(ibuf_t)));

  /* At startup we initialize ibuf to have a maximum of
  CHANGE_BUFFER_DEFAULT_SIZE in terms of percentage of the
  buffer pool size. Once ibuf struct is initialized this
  value is updated with the user supplied size by calling
  ibuf_max_size_update(). */
  ibuf->max_size = ((buf_pool_get_curr_size() / UNIV_PAGE_SIZE) *
                    CHANGE_BUFFER_DEFAULT_SIZE) /
                   100;

  mutex_create(LATCH_ID_IBUF, &ibuf_mutex);

  mutex_create(LATCH_ID_IBUF_BITMAP, &ibuf_bitmap_mutex);

  mutex_create(LATCH_ID_IBUF_PESSIMISTIC_INSERT,
               &ibuf_pessimistic_insert_mutex);

  mtr_start(&mtr);

  mtr_x_lock_space(fil_space_get_sys_space(), &mtr);

  mutex_enter(&ibuf_mutex);

  header_page = ibuf_header_page_get(&mtr);

  fseg_n_reserved_pages(header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER,
                        &n_used, &mtr);
  ibuf_enter(&mtr);

  ut_ad(n_used >= 2);

  ibuf->seg_size = n_used;

  {
    buf_block_t *block;

    block = buf_page_get(page_id_t(IBUF_SPACE_ID, FSP_IBUF_TREE_ROOT_PAGE_NO),
                         univ_page_size, RW_X_LATCH, UT_LOCATION_HERE, &mtr);

    buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);

    root = buf_block_get_frame(block);
  }

  ibuf_size_update(root);
  mutex_exit(&ibuf_mutex);

  ibuf->empty = page_is_empty(root);
  ibuf_mtr_commit(&mtr);

  ibuf->index =
      dict_mem_index_create("innodb_change_buffer", "CLUST_IND", IBUF_SPACE_ID,
                            DICT_CLUSTERED | DICT_IBUF, 1);
  ibuf->index->id = DICT_IBUF_ID_MIN + IBUF_SPACE_ID;
  ibuf->index->table = dict_mem_table_create("innodb_change_buffer",
                                             IBUF_SPACE_ID, 1, 0, 0, 0, 0);
  ibuf->index->n_uniq = REC_MAX_N_FIELDS;
  rw_lock_create(index_tree_rw_lock_key, &ibuf->index->lock,
                 LATCH_ID_IBUF_INDEX_TREE);
  ibuf->index->search_info = btr_search_info_create(ibuf->index->heap);
  ibuf->index->page = FSP_IBUF_TREE_ROOT_PAGE_NO;
  ut_d(ibuf->index->cached = true);
}

/** Updates the max_size value for ibuf. */
void ibuf_max_size_update(ulint new_val) /*!< in: new value in terms of
                                         percentage of the buffer pool size */
{
  ulint new_size =
      ((buf_pool_get_curr_size() / UNIV_PAGE_SIZE) * new_val) / 100;
  mutex_enter(&ibuf_mutex);
  ibuf->max_size = new_size;
  mutex_exit(&ibuf_mutex);
}

#endif /* !UNIV_HOTBACKUP */
/** Initializes an ibuf bitmap page. */
void ibuf_bitmap_page_init(buf_block_t *block, /*!< in: bitmap page */
                           mtr_t *mtr)         /*!< in: mtr */
{
  page_t *page;
  ulint byte_offset;

  page = buf_block_get_frame(block);
  fil_page_set_type(page, FIL_PAGE_IBUF_BITMAP);

  /* Write all zeros to the bitmap */

  byte_offset =
      UT_BITS_IN_BYTES(block->page.size.physical() * IBUF_BITS_PER_PAGE);

  memset(page + IBUF_BITMAP, 0, byte_offset);

  /* The remaining area (up to the page trailer) is uninitialized. */

#ifndef UNIV_HOTBACKUP
  mlog_write_initial_log_record(page, MLOG_IBUF_BITMAP_INIT, mtr);
#endif /* !UNIV_HOTBACKUP */
}

const byte *ibuf_parse_bitmap_init(const byte *ptr,
                                   const byte *end_ptr [[maybe_unused]],
                                   buf_block_t *block, mtr_t *mtr) {
  if (block) {
    ibuf_bitmap_page_init(block, mtr);
  }

  return (ptr);
}
#ifndef UNIV_HOTBACKUP
/** Gets the desired bits for a given page from a bitmap page.
@param[in]      page            Bitmap page
@param[in]      page_id         Page id whose bits to get
@param[in]      page_size       Page size
@param[in]      latch_type      MTR_MEMO_PAGE_X_FIX, MTR_MEMO_BUF_FIX, ...
@param[in,out]  mtr             Mini-transaction holding latch_type on the
bitmap page
@param[in]      bit             IBUF_BITMAP_FREE, IBUF_BITMAP_BUFFERED, ...
@return value of bits */
static inline ulint ibuf_bitmap_page_get_bits_low(
    const page_t *page, const page_id_t &page_id, const page_size_t &page_size,
    IF_DEBUG(ulint latch_type, mtr_t *mtr, ) ulint bit) {
  ulint byte_offset;
  ulint bit_offset;
  ulint map_byte;
  ulint value;

  ut_ad(bit < IBUF_BITS_PER_PAGE);
  static_assert(IBUF_BITS_PER_PAGE % 2 == 0,
                "IBUF_BITS_PER_PAGE must be an even number!");
  ut_ad(mtr_memo_contains_page(mtr, page, latch_type));

  bit_offset =
      (page_id.page_no() % page_size.physical()) * IBUF_BITS_PER_PAGE + bit;

  byte_offset = bit_offset / 8;
  bit_offset = bit_offset % 8;

  ut_ad(byte_offset + IBUF_BITMAP < UNIV_PAGE_SIZE);

  map_byte = mach_read_from_1(page + IBUF_BITMAP + byte_offset);

  value = ut_bit_get_nth(map_byte, bit_offset);

  if (bit == IBUF_BITMAP_FREE) {
    ut_ad(bit_offset + 1 < 8);

    value = value * 2 + ut_bit_get_nth(map_byte, bit_offset + 1);
  }

  return (value);
}

/** Gets the desired bits for a given page from a bitmap page.
@param[in]      page            Bitmap page
@param[in]      page_id         Page id whose bits to get
@param[in]      page_size       Page id whose bits to get
@param[in]      bit             IBUF_BITMAP_FREE, IBUF_BITMAP_BUFFERED, ...
@param[in,out]  mtr             Mini-transaction holding an x-latch on the
bitmap page
@return value of bits */
inline ulint ibuf_bitmap_page_get_bits(const page_t *page,
                                       const page_id_t &page_id,
                                       const page_size_t &page_size, ulint bit,
                                       mtr_t *mtr [[maybe_unused]]) {
  return ibuf_bitmap_page_get_bits_low(
      page, page_id, page_size, IF_DEBUG(MTR_MEMO_PAGE_X_FIX, mtr, ) bit);
}

/** Sets the desired bit for a given page in a bitmap page.
@param[in,out]  page            bitmap page
@param[in]      page_id         page id whose bits to set
@param[in]      page_size       page size
@param[in]      bit             IBUF_BITMAP_FREE, IBUF_BITMAP_BUFFERED, ...
@param[in]      val             value to set
@param[in,out]  mtr             mtr containing an x-latch to the bitmap page */
static void ibuf_bitmap_page_set_bits(page_t *page, const page_id_t &page_id,
                                      const page_size_t &page_size, ulint bit,
                                      ulint val, mtr_t *mtr) {
  ulint byte_offset;
  ulint bit_offset;
  ulint map_byte;

  ut_ad(bit < IBUF_BITS_PER_PAGE);
  static_assert(IBUF_BITS_PER_PAGE % 2 == 0,
                "IBUF_BITS_PER_PAGE must be an even number!");
  ut_ad(mtr_memo_contains_page(mtr, page, MTR_MEMO_PAGE_X_FIX));
#ifdef UNIV_IBUF_COUNT_DEBUG
  ut_a((bit != IBUF_BITMAP_BUFFERED) || (val != 0) ||
       (0 == ibuf_count_get(page_id)));
#endif

  bit_offset =
      (page_id.page_no() % page_size.physical()) * IBUF_BITS_PER_PAGE + bit;

  byte_offset = bit_offset / 8;
  bit_offset = bit_offset % 8;

  ut_ad(byte_offset + IBUF_BITMAP < UNIV_PAGE_SIZE);

  map_byte = mach_read_from_1(page + IBUF_BITMAP + byte_offset);

  if (bit == IBUF_BITMAP_FREE) {
    ut_ad(bit_offset + 1 < 8);
    ut_ad(val <= 3);

    map_byte = ut_bit_set_nth(map_byte, bit_offset, val / 2);
    map_byte = ut_bit_set_nth(map_byte, bit_offset + 1, val % 2);
  } else {
    ut_ad(val <= 1);
    map_byte = ut_bit_set_nth(map_byte, bit_offset, val);
  }

  mlog_write_ulint(page + IBUF_BITMAP + byte_offset, map_byte, MLOG_1BYTE, mtr);
}

/** Calculates the bitmap page number for a given page number.
@param[in]      page_id         page id
@param[in]      page_size       page size
@return the bitmap page id where the file page is mapped */
static inline const page_id_t ibuf_bitmap_page_no_calc(
    const page_id_t &page_id, const page_size_t &page_size) {
  page_no_t bitmap_page_no;

  bitmap_page_no = FSP_IBUF_BITMAP_OFFSET +
                   (page_id.page_no() & ~(page_size.physical() - 1));

  return (page_id_t(page_id.space(), bitmap_page_no));
}

/** Gets the ibuf bitmap page where the bits describing a given file page are
stored.
@param[in]      page_id         Page id of the file page
@param[in]      page_size       Page size of the file page
@param[in]      location                Location where called
@param[in,out]  mtr             Mini-transaction
@return bitmap page where the file page is mapped, that is, the bitmap
page containing the descriptor bits for the file page; the bitmap page
is x-latched */
static page_t *ibuf_bitmap_get_map_page(const page_id_t &page_id,
                                        const page_size_t &page_size,
                                        ut::Location location, mtr_t *mtr) {
  buf_block_t *block;

  block =
      buf_page_get_gen(ibuf_bitmap_page_no_calc(page_id, page_size), page_size,
                       RW_X_LATCH, nullptr, Page_fetch::NORMAL, location, mtr);

  buf_block_dbg_add_level(block, SYNC_IBUF_BITMAP);

  return (buf_block_get_frame(block));
}

/** Sets the free bits of the page in the ibuf bitmap. This is done in a
 separate mini-transaction, hence this operation does not restrict further work
 to only ibuf bitmap operations, which would result if the latch to the bitmap
 page were kept. */
static inline void ibuf_set_free_bits_low(
    const buf_block_t *block, /*!< in: index page; free bits are set if
                              the index is non-clustered and page
                              level is 0 */
    ulint val,                /*!< in: value to set: < 4 */
    mtr_t *mtr)               /*!< in/out: mtr */
{
  page_t *bitmap_page;

  if (!page_is_leaf(buf_block_get_frame(block))) {
    return;
  }

  bitmap_page = ibuf_bitmap_get_map_page(block->page.id, block->page.size,
                                         UT_LOCATION_HERE, mtr);

#ifdef UNIV_IBUF_DEBUG
  ut_a(val <= ibuf_index_page_calc_free(block));
#endif /* UNIV_IBUF_DEBUG */

  ibuf_bitmap_page_set_bits(bitmap_page, block->page.id, block->page.size,
                            IBUF_BITMAP_FREE, val, mtr);
}

/** Sets the free bit of the page in the ibuf bitmap. This is done in a separate
 mini-transaction, hence this operation does not restrict further work to only
 ibuf bitmap operations, which would result if the latch to the bitmap page
 were kept. */
void ibuf_set_free_bits_func(
    buf_block_t *block, /*!< in: index page of a non-clustered index;
                        free bit is reset if page level is 0 */
#ifdef UNIV_IBUF_DEBUG
    ulint max_val, /*!< in: ULINT_UNDEFINED or a maximum
                   value which the bits must have before
                   setting; this is for debugging */
#endif             /* UNIV_IBUF_DEBUG */
    ulint val)     /*!< in: value to set: < 4 */
{
  mtr_t mtr;
  page_t *page;
  page_t *bitmap_page;

  page = buf_block_get_frame(block);

  if (!page_is_leaf(page)) {
    return;
  }

  mtr_start(&mtr);

  const fil_space_t *space = fil_space_get(block->page.id.space());

  bitmap_page = ibuf_bitmap_get_map_page(block->page.id, block->page.size,
                                         UT_LOCATION_HERE, &mtr);

  switch (space->purpose) {
    case FIL_TYPE_TABLESPACE:
      break;
    case FIL_TYPE_TEMPORARY:
    case FIL_TYPE_IMPORT:
      mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
  }

#ifdef UNIV_IBUF_DEBUG
  if (max_val != ULINT_UNDEFINED) {
    ulint old_val;

    old_val = ibuf_bitmap_page_get_bits(bitmap_page, block->page.id,
                                        IBUF_BITMAP_FREE, &mtr);
#if 0
    if (old_val != max_val) {
      fprintf(stderr,
        "Ibuf: page %lu old val %lu max val %lu\n",
        page_get_page_no(page),
        old_val, max_val);
    }
#endif

    ut_a(old_val <= max_val);
  }
#if 0
  fprintf(stderr, "Setting page no %lu free bits to %lu should be %lu\n",
    page_get_page_no(page), val,
    ibuf_index_page_calc_free(block));
#endif

  ut_a(val <= ibuf_index_page_calc_free(block));
#endif /* UNIV_IBUF_DEBUG */

  ibuf_bitmap_page_set_bits(bitmap_page, block->page.id, block->page.size,
                            IBUF_BITMAP_FREE, val, &mtr);

  mtr_commit(&mtr);
}

/** Resets the free bits of the page in the ibuf bitmap. This is done in a
 separate mini-transaction, hence this operation does not restrict
 further work to only ibuf bitmap operations, which would result if the
 latch to the bitmap page were kept.  NOTE: The free bits in the insert
 buffer bitmap must never exceed the free space on a page.  It is safe
 to decrement or reset the bits in the bitmap in a mini-transaction
 that is committed before the mini-transaction that affects the free
 space. */
void ibuf_reset_free_bits(
    buf_block_t *block) /*!< in: index page; free bits are set to 0
                        if the index is a non-clustered
                        non-unique, and page level is 0 */
{
  ibuf_set_free_bits(block, 0, ULINT_UNDEFINED);
}

/** Updates the free bits for an uncompressed page to reflect the present
 state.  Does this in the mtr given, which means that the latching
 order rules virtually prevent any further operations for this OS
 thread until mtr is committed.  NOTE: The free bits in the insert
 buffer bitmap must never exceed the free space on a page.  It is safe
 to set the free bits in the same mini-transaction that updated the
 page. */
void ibuf_update_free_bits_low(const buf_block_t *block, /*!< in: index page */
                               ulint max_ins_size,       /*!< in: value of
                                                         maximum insert size
                                                         with reorganize before
                                                         the latest operation
                                                         performed to the page */
                               mtr_t *mtr)               /*!< in/out: mtr */
{
  ulint before;
  ulint after;

  ut_a(!buf_block_get_page_zip(block));

  before =
      ibuf_index_page_calc_free_bits(block->page.size.logical(), max_ins_size);

  after = ibuf_index_page_calc_free(block);

  /* This approach cannot be used on compressed pages, since the
  computed value of "before" often does not match the current
  state of the bitmap.  This is because the free space may
  increase or decrease when a compressed page is reorganized. */
  if (before != after) {
    ibuf_set_free_bits_low(block, after, mtr);
  }
}

/** Updates the free bits for a compressed page to reflect the present
 state.  Does this in the mtr given, which means that the latching
 order rules virtually prevent any further operations for this OS
 thread until mtr is committed.  NOTE: The free bits in the insert
 buffer bitmap must never exceed the free space on a page.  It is safe
 to set the free bits in the same mini-transaction that updated the
 page. */
void ibuf_update_free_bits_zip(buf_block_t *block, /*!< in/out: index page */
                               mtr_t *mtr)         /*!< in/out: mtr */
{
  page_t *bitmap_page;
  ulint after;

  ut_a(page_is_leaf(buf_block_get_frame(block)));
  ut_a(block->page.size.is_compressed());

  bitmap_page = ibuf_bitmap_get_map_page(block->page.id, block->page.size,
                                         UT_LOCATION_HERE, mtr);

  after = ibuf_index_page_calc_free_zip(block);

  if (after == 0) {
    /* We move the page to the front of the buffer pool LRU list:
    the purpose of this is to prevent those pages to which we
    cannot make inserts using the insert buffer from slipping
    out of the buffer pool */

    buf_page_make_young(&block->page);
  }

  ibuf_bitmap_page_set_bits(bitmap_page, block->page.id, block->page.size,
                            IBUF_BITMAP_FREE, after, mtr);
}

/** Updates the free bits for the two pages to reflect the present state.
 Does this in the mtr given, which means that the latching order rules
 virtually prevent any further operations until mtr is committed.
 NOTE: The free bits in the insert buffer bitmap must never exceed the
 free space on a page.  It is safe to set the free bits in the same
 mini-transaction that updated the pages. */
void ibuf_update_free_bits_for_two_pages_low(
    buf_block_t *block1, /*!< in: index page */
    buf_block_t *block2, /*!< in: index page */
    mtr_t *mtr)          /*!< in: mtr */
{
  ulint state;

  ut_ad(block1->page.id.space() == block2->page.id.space());

  /* As we have to x-latch two random bitmap pages, we have to acquire
  the bitmap mutex to prevent a deadlock with a similar operation
  performed by another OS thread. */

  mutex_enter(&ibuf_bitmap_mutex);

  state = ibuf_index_page_calc_free(block1);

  ibuf_set_free_bits_low(block1, state, mtr);

  state = ibuf_index_page_calc_free(block2);

  ibuf_set_free_bits_low(block2, state, mtr);

  mutex_exit(&ibuf_bitmap_mutex);
}

/** Returns true if the page is one of the fixed address ibuf pages.
@param[in]      page_id         page id
@param[in]      page_size       page size
@return true if a fixed address ibuf i/o page */
static inline bool ibuf_fixed_addr_page(const page_id_t &page_id,
                                        const page_size_t &page_size) {
  return ((page_id.space() == IBUF_SPACE_ID &&
           page_id.page_no() == IBUF_TREE_ROOT_PAGE_NO) ||
          ibuf_bitmap_page(page_id, page_size));
}

/** Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages.
Must not be called when recv_no_ibuf_operations==true.
@param[in]      page_id         page id
@param[in]      page_size       page size
@param[in]      x_latch         false if relaxed check (avoid latching the
bitmap page)
@param[in] location Location where called
@param[in,out]  mtr             mtr which will contain an x-latch to the
bitmap page if the page is not one of the fixed address ibuf pages, or NULL,
in which case a new transaction is created.
@return true if level 2 or level 3 page */
bool ibuf_page_low(const page_id_t &page_id, const page_size_t &page_size,
                   IF_DEBUG(bool x_latch, ) ut::Location location, mtr_t *mtr) {
  ulint ret;
  mtr_t local_mtr;
  page_t *bitmap_page;

  ut_ad(!recv_no_ibuf_operations);
  ut_ad(x_latch || mtr == nullptr);

  if (ibuf_fixed_addr_page(page_id, page_size)) {
    return true;
  } else if (page_id.space() != IBUF_SPACE_ID) {
    return false;
  }

  ut_ad(fil_space_get_type(IBUF_SPACE_ID) == FIL_TYPE_TABLESPACE);

#ifdef UNIV_DEBUG
  if (!x_latch) {
    mtr_start(&local_mtr);

    /* Get the bitmap page without a page latch, so that
    we will not be violating the latching order when
    another bitmap page has already been latched by this
    thread. The page will be buffer-fixed, and thus it
    cannot be removed or relocated while we are looking at
    it. The contents of the page could change, but the
    IBUF_BITMAP_IBUF bit that we are interested in should
    not be modified by any other thread. Nobody should be
    calling ibuf_add_free_page() or ibuf_remove_free_page()
    while the page is linked to the insert buffer b-tree. */

    bitmap_page = buf_block_get_frame(buf_page_get_gen(
        ibuf_bitmap_page_no_calc(page_id, page_size), page_size, RW_NO_LATCH,
        nullptr, Page_fetch::NO_LATCH, location, &local_mtr));

    ret = ibuf_bitmap_page_get_bits_low(bitmap_page, page_id, page_size,
                                        MTR_MEMO_BUF_FIX, &local_mtr,
                                        IBUF_BITMAP_IBUF);

    mtr_commit(&local_mtr);
    return (ret);
  }
#endif /* UNIV_DEBUG */

  if (mtr == nullptr) {
    mtr = &local_mtr;
    mtr_start(mtr);
  }

  bitmap_page = ibuf_bitmap_get_map_page(page_id, page_size, location, mtr);

  ret = ibuf_bitmap_page_get_bits(bitmap_page, page_id, page_size,
                                  IBUF_BITMAP_IBUF, mtr);

  if (mtr == &local_mtr) {
    mtr_commit(mtr);
  }

  return ret != 0;
}

/** Returns the page number field of an ibuf record.
 @param[in] mtr mini-transaction owning rec
 @param[in] rec ibuf record
 @return page number */
static page_no_t ibuf_rec_get_page_no_func(IF_DEBUG(mtr_t *mtr, )
                                               const rec_t *rec) {
  const byte *field;
  ulint len;

  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(ibuf_inside(mtr));
  ut_ad(rec_get_n_fields_old_raw(rec) > 2);

  field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_MARKER, &len);

  ut_a(len == 1);

  field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_PAGE, &len);

  ut_a(len == 4);

  return (mach_read_from_4(field));
}

static inline page_no_t ibuf_rec_get_page_no(mtr_t *mtr [[maybe_unused]],
                                             const rec_t *rec) {
  return ibuf_rec_get_page_no_func(IF_DEBUG(mtr, ) rec);
}

/** Returns the space id field of an ibuf record. For < 4.1.x format records
 returns 0.
 @param[in] mtr mini-transaction owning rec
 @param[in] rec ibuf record
 @return space id */
static space_id_t ibuf_rec_get_space_func(IF_DEBUG(mtr_t *mtr, )
                                              const rec_t *rec) {
  const byte *field;
  ulint len;

  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(ibuf_inside(mtr));
  ut_ad(rec_get_n_fields_old_raw(rec) > 2);

  field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_MARKER, &len);

  ut_a(len == 1);

  field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_SPACE, &len);

  ut_a(len == 4);

  return (mach_read_from_4(field));
}

static inline space_id_t ibuf_rec_get_space(mtr_t *mtr [[maybe_unused]],
                                            const rec_t *rec) {
  return ibuf_rec_get_space_func(IF_DEBUG(mtr, ) rec);
}

/** Get various information about an ibuf record in >= 4.1.x format.
 @param[in]     mtr             Mini-transaction owning rec, or nullptr if this
                                is called from ibuf_rec_has_multi_value().
                                Because it's from page_validate() which doesn't
                                have mtr at hand
 @param[in]     rec             Ibuf record
 @param[in,out] op              Operation type, or NULL
 @param[in,out] comp            Compact flag, or NULL
 @param[in,out] info_len        Length of info fields at the start of the fourth
 field, or NULL
 @param[in]     counter         Counter value, or NULL */
static void ibuf_rec_get_info_func(IF_DEBUG(mtr_t *mtr, ) const rec_t *rec,
                                   ibuf_op_t *op, bool *comp, ulint *info_len,
                                   ulint *counter) {
  const byte *types;
  ulint fields;
  ulint len;

  /* Local variables to shadow arguments. */
  ibuf_op_t op_local;
  bool comp_local;
  ulint info_len_local;
  ulint counter_local;

  ut_ad(mtr == nullptr ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(mtr == nullptr || ibuf_inside(mtr));
  fields = rec_get_n_fields_old_raw(rec);
  ut_a(fields > IBUF_REC_FIELD_USER);

  types = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_METADATA, &len);

  info_len_local = len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE;

  switch (info_len_local) {
    case 0:
    case 1:
      op_local = IBUF_OP_INSERT;
      comp_local = info_len_local;
      ut_ad(!counter);
      counter_local = ULINT_UNDEFINED;
      break;

    case IBUF_REC_INFO_SIZE:
      op_local = (ibuf_op_t)types[IBUF_REC_OFFSET_TYPE];
      comp_local = types[IBUF_REC_OFFSET_FLAGS] & IBUF_REC_COMPACT;
      counter_local = mach_read_from_2(types + IBUF_REC_OFFSET_COUNTER);
      break;

    default:
      ut_error;
  }

  ut_a(op_local < IBUF_OP_COUNT);
  ut_a((len - info_len_local) ==
       (fields - IBUF_REC_FIELD_USER) * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

  if (op) {
    *op = op_local;
  }

  if (comp) {
    *comp = comp_local;
  }

  if (info_len) {
    *info_len = info_len_local;
  }

  if (counter) {
    *counter = counter_local;
  }
}

inline void ibuf_rec_get_info(mtr_t *mtr [[maybe_unused]], const rec_t *rec,
                              ibuf_op_t *op, bool *comp, ulint *info_len,
                              ulint *counter) {
  ibuf_rec_get_info_func(IF_DEBUG(mtr, ) rec, op, comp, info_len, counter);
}

/** Returns the operation type field of an ibuf record.
 @param[in] mtr mini-transaction owning rec
 @param[in] rec ibuf record
 @return operation type */
static ibuf_op_t ibuf_rec_get_op_type_func(IF_DEBUG(mtr_t *mtr, )
                                               const rec_t *rec) {
  ulint len;

  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(ibuf_inside(mtr));
  ut_ad(rec_get_n_fields_old_raw(rec) > 2);

  (void)rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_MARKER, &len);

  if (len > 1) {
    /* This is a < 4.1.x format record */

    return (IBUF_OP_INSERT);
  } else {
    ibuf_op_t op;

    ibuf_rec_get_info_func(IF_DEBUG(mtr, ) rec, &op, nullptr, nullptr, nullptr);

    return (op);
  }
}

inline ibuf_op_t ibuf_rec_get_op_type(mtr_t *mtr [[maybe_unused]],
                                      const rec_t *rec) {
  return ibuf_rec_get_op_type_func(IF_DEBUG(mtr, ) rec);
}

/** Read the first two bytes from a record's fourth field (counter field in
 new records; something else in older records).
 @return "counter" field, or ULINT_UNDEFINED if for some reason it
 can't be read */
ulint ibuf_rec_get_counter(const rec_t *rec) /*!< in: ibuf record */
{
  const byte *ptr;
  ulint len;

  if (rec_get_n_fields_old_raw(rec) <= IBUF_REC_FIELD_METADATA) {
    return (ULINT_UNDEFINED);
  }

  /* nullptr for index as it can't be clustered index */
  ptr = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_METADATA, &len);

  if (len >= 2) {
    return (mach_read_from_2(ptr));
  } else {
    return (ULINT_UNDEFINED);
  }
}

bool ibuf_rec_has_multi_value(const rec_t *rec) {
  ulint len;
  ulint info_len;
  uint32_t n_fields = rec_get_n_fields_old_raw(rec) - IBUF_REC_FIELD_USER;
  /* nullptr for index as it can't be clustered index */
  const byte *types =
      rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_METADATA, &len);

  ibuf_rec_get_info(nullptr, rec, nullptr, nullptr, &info_len, nullptr);
  types += info_len;

  for (uint32_t i = 0; i < n_fields; ++i) {
    dtype_t dtype;

    dtype_new_read_for_order_and_null_size(&dtype, types);

    if ((dtype.prtype & DATA_MULTI_VALUE) != 0) {
      return (true);
    }

    types += DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE;
  }

  return (false);
}

/** Add accumulated operation counts to a permanent array. Both arrays must be
 of size IBUF_OP_COUNT. */
static void ibuf_add_ops(
    std::atomic<ulint> *arr, /*!< in/out: array to modify */
    const ulint *ops)        /*!< in: operation counts */

{
  ulint i;

  for (i = 0; i < IBUF_OP_COUNT; i++) {
    arr[i].fetch_add(ops[i]);
  }
}

/** Print operation counts. The array must be of size IBUF_OP_COUNT. */
static void ibuf_print_ops(
    const std::atomic<ulint> *ops, /*!< in: operation counts */
    FILE *file)                    /*!< in: file where to print */
{
  static const char *op_names[] = {"insert", "delete mark", "delete"};
  ulint i;

  static_assert(UT_ARR_SIZE(op_names) == IBUF_OP_COUNT);

  for (i = 0; i < IBUF_OP_COUNT; i++) {
    fprintf(file, "%s %lu%s", op_names[i], (ulong)ops[i].load(),
            (i < (IBUF_OP_COUNT - 1)) ? ", " : "");
  }

  putc('\n', file);
}

/** Creates a dummy index for inserting a record to a non-clustered index.
 @return dummy index */
static dict_index_t *ibuf_dummy_index_create(
    ulint n,   /*!< in: number of fields */
    bool comp) /*!< in: true=use compact record format */
{
  dict_table_t *table;
  dict_index_t *index;

  table = dict_mem_table_create("IBUF_DUMMY", DICT_HDR_SPACE, n, 0, 0,
                                comp ? DICT_TF_COMPACT : 0, 0);

  index =
      dict_mem_index_create("IBUF_DUMMY", "IBUF_DUMMY", DICT_HDR_SPACE, 0, n);

  index->table = table;

  /* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
  index->cached = true;

  return (index);
}
/** Add a column to the dummy index */
static void ibuf_dummy_index_add_col(
    dict_index_t *index, /*!< in: dummy index */
    const dtype_t *type, /*!< in: the data type of the column */
    ulint len)           /*!< in: length of the column */
{
  ulint i = index->table->n_def;
  dict_mem_table_add_col(index->table, nullptr, nullptr, dtype_get_mtype(type),
                         dtype_get_prtype(type), dtype_get_len(type), true);
  dict_index_add_col(index, index->table, index->table->get_col(i), len, true);
}
/** Deallocates a dummy index for inserting a record to a non-clustered index.
 */
static void ibuf_dummy_index_free(
    dict_index_t *index) /*!< in, own: dummy index */
{
  dict_table_t *table = index->table;

  dict_mem_index_free(index);
  dict_mem_table_free(table);
}

/** Builds the entry used to

 1) IBUF_OP_INSERT: insert into a non-clustered index

 2) IBUF_OP_DELETE_MARK: find the record whose delete-mark flag we need to
    activate

 3) IBUF_OP_DELETE: find the record we need to delete

 when we have the corresponding record in an ibuf index.

 NOTE that as we copy pointers to fields in ibuf_rec, the caller must
 hold a latch to the ibuf_rec page as long as the entry is used!
 @param[in] mtr mini-transaction owning rec
 @param[in] ibuf_rec record in an insert buffer
 @param[in] heap heap where built
 @param[out] pindex own: dummy index that describes the entry
 @return own: entry to insert to a non-clustered index */
static dtuple_t *ibuf_build_entry_from_ibuf_rec_func(IF_DEBUG(mtr_t *mtr, )
                                                         const rec_t *ibuf_rec,
                                                     mem_heap_t *heap,
                                                     dict_index_t **pindex) {
  dtuple_t *tuple;
  dfield_t *field;
  ulint n_fields;
  const byte *types;
  const byte *data;
  ulint len;
  ulint info_len;
  ulint i;
  bool comp;
  dict_index_t *index;

  ut_ad(mtr_memo_contains_page(mtr, ibuf_rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, ibuf_rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(ibuf_inside(mtr));

  data = rec_get_nth_field_old(nullptr, ibuf_rec, IBUF_REC_FIELD_MARKER, &len);

  ut_a(len == 1);
  ut_a(*data == 0);
  ut_a(rec_get_n_fields_old_raw(ibuf_rec) > IBUF_REC_FIELD_USER);

  n_fields = rec_get_n_fields_old_raw(ibuf_rec) - IBUF_REC_FIELD_USER;

  tuple = dtuple_create(heap, n_fields);

  types =
      rec_get_nth_field_old(nullptr, ibuf_rec, IBUF_REC_FIELD_METADATA, &len);

  ibuf_rec_get_info_func(IF_DEBUG(mtr, ) ibuf_rec, nullptr, &comp, &info_len,
                         nullptr);

  index = ibuf_dummy_index_create(n_fields, comp);

  len -= info_len;
  types += info_len;

  ut_a(len == n_fields * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

  for (i = 0; i < n_fields; i++) {
    field = dtuple_get_nth_field(tuple, i);

    data =
        rec_get_nth_field_old(nullptr, ibuf_rec, i + IBUF_REC_FIELD_USER, &len);

    dfield_set_data(field, data, len);

    dtype_new_read_for_order_and_null_size(
        dfield_get_type(field), types + i * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

    ibuf_dummy_index_add_col(index, dfield_get_type(field), len);
  }

  /* Prevent an ut_ad() failure in page_zip_write_rec() by
  adding system columns to the dummy table pointed to by the
  dummy secondary index.  The insert buffer is only used for
  secondary indexes, whose records never contain any system
  columns, such as DB_TRX_ID. */
  ut_d(dict_table_add_system_columns(index->table, index->table->heap));

  *pindex = index;

  return (tuple);
}

inline dtuple_t *ibuf_build_entry_from_ibuf_rec(mtr_t *mtr [[maybe_unused]],
                                                const rec_t *ibuf_rec,
                                                mem_heap_t *heap,
                                                dict_index_t **pindex) {
  return ibuf_build_entry_from_ibuf_rec_func(IF_DEBUG(mtr, ) ibuf_rec, heap,
                                             pindex);
}
/** Get the data size.
 @return size of fields */
static inline ulint ibuf_rec_get_size(
    const rec_t *rec,  /*!< in: ibuf record */
    const byte *types, /*!< in: fields */
    ulint n_fields,    /*!< in: number of fields */
    bool comp)         /*!< in: 0=ROW_FORMAT=REDUNDANT,
                        nonzero=ROW_FORMAT=COMPACT */
{
  ulint i;
  ulint field_offset;
  ulint types_offset;
  ulint size = 0;

  field_offset = IBUF_REC_FIELD_USER;
  types_offset = DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE;

  for (i = 0; i < n_fields; i++) {
    ulint len;
    dtype_t dtype;

    /* nullptr for index as it can't be clustered index */
    rec_get_nth_field_offs_old(nullptr, rec, i + field_offset, &len);

    if (len != UNIV_SQL_NULL) {
      size += len;
    } else {
      dtype_new_read_for_order_and_null_size(&dtype, types);

      size += dtype_get_sql_null_size(&dtype, comp);
    }

    types += types_offset;
  }

  return (size);
}

/** Returns the space taken by a stored non-clustered index entry if converted
 to an index record.
 @param[in] mtr mini-transaction owning rec
 @param[in] ibuf_rec ibuf record
 @return size of index record in bytes + an upper limit of the space
 taken in the page directory */
static ulint ibuf_rec_get_volume_func(IF_DEBUG(mtr_t *mtr, )
                                          const rec_t *ibuf_rec) {
  ulint len;
  const byte *data;
  const byte *types;
  ulint n_fields;
  ulint data_size;
  bool comp;
  ibuf_op_t op;
  ulint info_len;

  ut_ad(mtr_memo_contains_page(mtr, ibuf_rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, ibuf_rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(ibuf_inside(mtr));
  ut_ad(rec_get_n_fields_old_raw(ibuf_rec) > 2);

  data = rec_get_nth_field_old(nullptr, ibuf_rec, IBUF_REC_FIELD_MARKER, &len);
  ut_a(len == 1);
  ut_a(*data == 0);

  types =
      rec_get_nth_field_old(nullptr, ibuf_rec, IBUF_REC_FIELD_METADATA, &len);

  ibuf_rec_get_info_func(IF_DEBUG(mtr, ) ibuf_rec, &op, &comp, &info_len,
                         nullptr);

  if (op == IBUF_OP_DELETE_MARK || op == IBUF_OP_DELETE) {
    /* Delete-marking a record doesn't take any
    additional space, and while deleting a record
    actually frees up space, we have to play it safe and
    pretend it takes no additional space (the record
    might not exist, etc.).  */

    return (0);
  } else if (comp) {
    dtuple_t *entry;
    ulint volume;
    dict_index_t *dummy_index;
    mem_heap_t *heap = mem_heap_create(500, UT_LOCATION_HERE);

    entry = ibuf_build_entry_from_ibuf_rec_func(IF_DEBUG(mtr, ) ibuf_rec, heap,
                                                &dummy_index);

    volume = rec_get_converted_size(dummy_index, entry);

    ibuf_dummy_index_free(dummy_index);
    mem_heap_free(heap);

    return (volume + page_dir_calc_reserved_space(1));
  }

  types += info_len;
  n_fields = rec_get_n_fields_old_raw(ibuf_rec) - IBUF_REC_FIELD_USER;

  data_size = ibuf_rec_get_size(ibuf_rec, types, n_fields, comp);

  return (data_size + rec_get_converted_extra_size(data_size, n_fields, false) +
          page_dir_calc_reserved_space(1));
}

inline ulint ibuf_rec_get_volume(mtr_t *mtr [[maybe_unused]],
                                 const rec_t *rec) {
  return ibuf_rec_get_volume_func(IF_DEBUG(mtr, ) rec);
}

/** Builds the tuple to insert to an ibuf tree when we have an entry for a
 non-clustered index.

 NOTE that the original entry must be kept because we copy pointers to
 its fields.

 @return own: entry to insert into an ibuf index tree */
static dtuple_t *ibuf_entry_build(
    ibuf_op_t op,          /*!< in: operation type */
    dict_index_t *index,   /*!< in: non-clustered index */
    const dtuple_t *entry, /*!< in: entry for a non-clustered index */
    space_id_t space_id,   /*!< in: space id */
    page_no_t page_no,     /*!< in: index page number where entry should
                           be inserted */
    ulint counter,         /*!< in: counter value;
                           ULINT_UNDEFINED=not used */
    mem_heap_t *heap)      /*!< in: heap into which to build */
{
  dtuple_t *tuple;
  dfield_t *field;
  const dfield_t *entry_field;
  dtype_t fake_type;
  ulint n_fields;
  byte *buf;
  byte *ti;
  byte *type_info;
  ulint i;

  ut_ad(counter != ULINT_UNDEFINED || op == IBUF_OP_INSERT);
  ut_ad(counter == ULINT_UNDEFINED || counter <= 0xFFFF);
  ut_ad(op < IBUF_OP_COUNT);

  memset(&fake_type, 0, sizeof(dtype_t));

  /* We have to build a tuple with the following fields:

  1-4) These are described at the top of this file.

  5) The rest of the fields are copied from the entry.

  All fields in the tuple are ordered like the type binary in our
  insert buffer tree. */

  n_fields = dtuple_get_n_fields(entry);

  tuple = dtuple_create(heap, n_fields + IBUF_REC_FIELD_USER);

  /* 1) Space Id */

  field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_SPACE);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 4));

  mach_write_to_4(buf, space_id);

  dfield_set_data(field, buf, 4);

  dfield_set_type(field, &fake_type);

  /* 2) Marker byte */

  field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_MARKER);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 1));

  /* We set the marker byte zero */

  mach_write_to_1(buf, 0);

  dfield_set_data(field, buf, 1);

  dfield_set_type(field, &fake_type);

  /* 3) Page number */

  field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_PAGE);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 4));

  mach_write_to_4(buf, page_no);

  dfield_set_data(field, buf, 4);

  dfield_set_type(field, &fake_type);

  /* 4) Type info, part #1 */

  if (counter == ULINT_UNDEFINED) {
    i = dict_table_is_comp(index->table) ? 1 : 0;
  } else {
    ut_ad(counter <= 0xFFFF);
    i = IBUF_REC_INFO_SIZE;
  }

  ti = type_info = static_cast<byte *>(
      mem_heap_alloc(heap, i + n_fields * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE));

  switch (i) {
    default:
      ut_error;
      break;
    case 1:
      /* set the flag for ROW_FORMAT=COMPACT */
      *ti++ = 0;
      [[fallthrough]];
    case 0:
      /* the old format does not allow delete buffering */
      ut_ad(op == IBUF_OP_INSERT);
      break;
    case IBUF_REC_INFO_SIZE:
      mach_write_to_2(ti + IBUF_REC_OFFSET_COUNTER, counter);

      ti[IBUF_REC_OFFSET_TYPE] = (byte)op;
      ti[IBUF_REC_OFFSET_FLAGS] =
          dict_table_is_comp(index->table) ? IBUF_REC_COMPACT : 0;
      ti += IBUF_REC_INFO_SIZE;
      break;
  }

  /* 5+) Fields from the entry */

  for (i = 0; i < n_fields; i++) {
    ulint fixed_len;
    const dict_field_t *ifield;

    field = dtuple_get_nth_field(tuple, i + IBUF_REC_FIELD_USER);
    entry_field = dtuple_get_nth_field(entry, i);
    dfield_copy(field, entry_field);

    ifield = index->get_field(i);
    /* Prefix index columns of fixed-length columns are of
    fixed length.  However, in the function call below,
    dfield_get_type(entry_field) contains the fixed length
    of the column in the clustered index.  Replace it with
    the fixed length of the secondary index column. */
    fixed_len = ifield->fixed_len;

#ifdef UNIV_DEBUG
    if (fixed_len) {
      /* dict_index_add_col() should guarantee these */
      ut_ad(fixed_len <= (ulint)dfield_get_type(entry_field)->len);
      if (ifield->prefix_len) {
        ut_ad(ifield->prefix_len == fixed_len);
      } else {
        ut_ad(fixed_len == (ulint)dfield_get_type(entry_field)->len);
      }
    }
#endif /* UNIV_DEBUG */

    dtype_new_store_for_order_and_null_size(ti, dfield_get_type(entry_field),
                                            fixed_len);
    ti += DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE;
  }

  /* 4) Type info, part #2 */

  field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_METADATA);

  dfield_set_data(field, type_info, ti - type_info);

  dfield_set_type(field, &fake_type);

  /* Set all the types in the new tuple binary */

  dtuple_set_types_binary(tuple, n_fields + IBUF_REC_FIELD_USER);

  return (tuple);
}

/** Builds a search tuple used to search buffered inserts for an index page.
 This is for >= 4.1.x format records.
 @return own: search tuple */
static dtuple_t *ibuf_search_tuple_build(
    space_id_t space,  /*!< in: space id */
    page_no_t page_no, /*!< in: index page number */
    mem_heap_t *heap)  /*!< in: heap into which to build */
{
  dtuple_t *tuple;
  dfield_t *field;
  dtype_t fake_type;
  byte *buf;

  memset(&fake_type, 0, sizeof(dtype_t));

  tuple = dtuple_create(heap, IBUF_REC_FIELD_METADATA);

  /* Store the space id in tuple */

  field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_SPACE);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 4));

  mach_write_to_4(buf, space);

  dfield_set_data(field, buf, 4);

  dfield_set_type(field, &fake_type);

  /* Store the new format record marker byte */

  field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_MARKER);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 1));

  mach_write_to_1(buf, 0);

  dfield_set_data(field, buf, 1);

  dfield_set_type(field, &fake_type);

  /* Store the page number in tuple */

  field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_PAGE);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 4));

  mach_write_to_4(buf, page_no);

  dfield_set_data(field, buf, 4);

  dfield_set_type(field, &fake_type);

  dtuple_set_types_binary(tuple, IBUF_REC_FIELD_METADATA);

  return (tuple);
}

/** Checks if there are enough pages in the free list of the ibuf tree that we
 dare to start a pessimistic insert to the insert buffer.
 @return true if enough free pages in list */
static inline bool ibuf_data_enough_free_for_insert(void) {
  ut_ad(mutex_own(&ibuf_mutex));

  /* We want a big margin of free pages, because a B-tree can sometimes
  grow in size also if records are deleted from it, as the node pointers
  can change, and we must make sure that we are able to delete the
  inserts buffered for pages that we read to the buffer pool, without
  any risk of running out of free space in the insert buffer. */

  return (ibuf->free_list_len >= (ibuf->size / 2) + 3 * ibuf->height);
}

/** Checks if there are enough pages in the free list of the ibuf tree that we
 should remove them and free to the file space management.
 @return true if enough free pages in list */
static inline bool ibuf_data_too_much_free(void) {
  ut_ad(mutex_own(&ibuf_mutex));

  return (ibuf->free_list_len >= 3 + (ibuf->size / 2) + 3 * ibuf->height);
}

/** Allocates a new page from the ibuf file segment and adds it to the free
 list.
 @return true on success, false if no space left */
static bool ibuf_add_free_page(void) {
  mtr_t mtr;
  page_t *header_page;
  buf_block_t *block;
  page_t *page;
  page_t *root;
  page_t *bitmap_page;

  fil_space_t *space = fil_space_get_sys_space();

  mtr_start(&mtr);

  /* Acquire the fsp latch before the ibuf header, obeying the latching
  order */
  mtr_x_lock(&space->latch, &mtr, UT_LOCATION_HERE);
  header_page = ibuf_header_page_get(&mtr);

  /* Allocate a new page: NOTE that if the page has been a part of a
  non-clustered index which has subsequently been dropped, then the
  page may have buffered inserts in the insert buffer, and these
  should be deleted from there. These get deleted when the page
  allocation creates the page in buffer. Thus the call below may end
  up calling the insert buffer routines and, as we yet have no latches
  to insert buffer tree pages, these routines can run without a risk
  of a deadlock. This is the reason why we created a special ibuf
  header page apart from the ibuf tree. */

  block = fseg_alloc_free_page(header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER,
                               0, FSP_UP, &mtr);

  if (block == nullptr) {
    mtr_commit(&mtr);

    return false;
  }

  ut_ad(rw_lock_get_x_lock_count(&block->lock) == 1);
  ibuf_enter(&mtr);
  mutex_enter(&ibuf_mutex);
  root = ibuf_tree_root_get(&mtr);

  buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE_NEW);
  page = buf_block_get_frame(block);

  /* Add the page to the free list and update the ibuf size data */

  flst_add_last(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
                page + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE, &mtr);

  mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_IBUF_FREE_LIST, MLOG_2BYTES,
                   &mtr);

  ibuf->seg_size++;
  ibuf->free_list_len++;

  /* Set the bit indicating that this page is now an ibuf tree page
  (level 2 page) */

  const page_id_t page_id(IBUF_SPACE_ID, block->page.id.page_no());
  const page_size_t page_size(space->flags);

  bitmap_page =
      ibuf_bitmap_get_map_page(page_id, page_size, UT_LOCATION_HERE, &mtr);

  mutex_exit(&ibuf_mutex);

  ibuf_bitmap_page_set_bits(bitmap_page, page_id, page_size, IBUF_BITMAP_IBUF,
                            true, &mtr);

  ibuf_mtr_commit(&mtr);

  return true;
}

/** Removes a page from the free list and frees it to the fsp system. */
static void ibuf_remove_free_page(void) {
  mtr_t mtr;
  mtr_t mtr2;
  page_t *header_page;
  page_no_t page_no;
  page_t *page;
  page_t *root;
  page_t *bitmap_page;

  fil_space_t *space = fil_space_get_sys_space();

  mtr_start(&mtr);

  const page_size_t page_size(space->flags);

  /* Acquire the fsp latch before the ibuf header, obeying the latching
  order */

  mtr_x_lock(&space->latch, &mtr, UT_LOCATION_HERE);
  header_page = ibuf_header_page_get(&mtr);

  /* Prevent pessimistic inserts to insert buffer trees for a while */
  ibuf_enter(&mtr);
  mutex_enter(&ibuf_pessimistic_insert_mutex);
  mutex_enter(&ibuf_mutex);

  if (!ibuf_data_too_much_free()) {
    mutex_exit(&ibuf_mutex);
    mutex_exit(&ibuf_pessimistic_insert_mutex);

    ibuf_mtr_commit(&mtr);

    return;
  }

  ibuf_mtr_start(&mtr2);

  root = ibuf_tree_root_get(&mtr2);

  mutex_exit(&ibuf_mutex);

  page_no =
      flst_get_last(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST, &mtr2).page;

  /* NOTE that we must release the latch on the ibuf tree root
  because in fseg_free_page we access level 1 pages, and the root
  is a level 2 page. */

  ibuf_mtr_commit(&mtr2);
  ibuf_exit(&mtr);

  /* Since pessimistic inserts were prevented, we know that the
  page is still in the free list. NOTE that also deletes may take
  pages from the free list, but they take them from the start, and
  the free list was so long that they cannot have taken the last
  page from it. */

  fseg_free_page(header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER,
                 IBUF_SPACE_ID, page_no, false, &mtr);

  const page_id_t page_id(IBUF_SPACE_ID, page_no);

  ut_d(buf_page_reset_file_page_was_freed(page_id));

  ibuf_enter(&mtr);

  mutex_enter(&ibuf_mutex);

  root = ibuf_tree_root_get(&mtr);

  ut_ad(page_no ==
        flst_get_last(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST, &mtr).page);

  {
    buf_block_t *block;

    block = buf_page_get(page_id, univ_page_size, RW_X_LATCH, UT_LOCATION_HERE,
                         &mtr);

    buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);

    page = buf_block_get_frame(block);
  }

  /* Remove the page from the free list and update the ibuf size data */

  flst_remove(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
              page + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE, &mtr);

  mutex_exit(&ibuf_pessimistic_insert_mutex);

  ibuf->seg_size--;
  ibuf->free_list_len--;

  /* Set the bit indicating that this page is no more an ibuf tree page
  (level 2 page) */

  bitmap_page =
      ibuf_bitmap_get_map_page(page_id, page_size, UT_LOCATION_HERE, &mtr);

  mutex_exit(&ibuf_mutex);

  ibuf_bitmap_page_set_bits(bitmap_page, page_id, page_size, IBUF_BITMAP_IBUF,
                            false, &mtr);

  ut_d(buf_page_set_file_page_was_freed(page_id));

  ibuf_mtr_commit(&mtr);
}

/** Frees excess pages from the ibuf free list. This function is called when an
 OS thread calls fsp services to allocate a new file segment, or a new page to a
 file segment, and the thread did not own the fsp latch before this call. */
void ibuf_free_excess_pages(void) {
  ut_ad(rw_lock_own(fil_space_get_latch(IBUF_SPACE_ID), RW_LOCK_X));

  ut_ad(rw_lock_get_x_lock_count(fil_space_get_latch(IBUF_SPACE_ID)) == 1);

  /* NOTE: We require that the thread did not own the latch before,
  because then we know that we can obey the correct latching order
  for ibuf latches */

  if (!ibuf) {
    /* Not yet initialized; not sure if this is possible, but
    does no harm to check for it. */

    return;
  }

  /* Free at most a few pages at a time, so that we do not delay the
  requested service too much */

  for (ulint i = 0; i < 4; i++) {
    mutex_enter(&ibuf_mutex);
    auto too_much_free = ibuf_data_too_much_free();
    mutex_exit(&ibuf_mutex);

    if (!too_much_free) {
      return;
    }

    ibuf_remove_free_page();
  }
}

/** Reads page numbers from a leaf in an ibuf tree.
 @param[in] contract true if this function is called to contract the tree, false
 if this is called when a single page becomes full and we look if it pays to
 read also nearby pages
 @param[in] rec insert buffer record
 @param[in] mtr mini-transaction holding rec
 @param[in,out] space_ids space id's of the pages
 @param[in,out] page_nos buffer for at least IBUF_MAX_N_PAGES_MERGED many page
 numbers; the page numbers are in an ascending order
 @param[out] n_stored number of page numbers stored to page_nos in this function
 @return a lower limit for the combined volume of records which will be
 merged */
static ulint ibuf_get_merge_page_nos_func(bool contract, const rec_t *rec,
                                          IF_DEBUG(mtr_t *mtr, )
                                              space_id_t *space_ids,
                                          page_no_t *page_nos,
                                          ulint *n_stored) {
  page_no_t prev_page_no;
  space_id_t prev_space_id;
  page_no_t first_page_no;
  space_id_t first_space_id;
  page_no_t rec_page_no;
  space_id_t rec_space_id;
  ulint sum_volumes;
  ulint volume_for_page;
  ulint rec_volume;
  ulint limit;
  ulint n_pages;
#ifndef UNIV_DEBUG
  mtr_t *mtr = nullptr;
#endif

  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(ibuf_inside(mtr));

  *n_stored = 0;

  limit = std::min(IBUF_MAX_N_PAGES_MERGED, buf_pool_get_curr_size() / 4);

  if (page_rec_is_supremum(rec)) {
    rec = page_rec_get_prev_const(rec);
  }

  if (page_rec_is_infimum(rec)) {
    rec = page_rec_get_next_const(rec);
  }

  if (page_rec_is_supremum(rec)) {
    return (0);
  }

  first_page_no = ibuf_rec_get_page_no(mtr, rec);
  first_space_id = ibuf_rec_get_space(mtr, rec);
  n_pages = 0;
  prev_page_no = 0;
  prev_space_id = 0;

  /* Go backwards from the first rec until we reach the border of the
  'merge area', or the page start or the limit of storable pages is
  reached */

  while (!page_rec_is_infimum(rec) && UNIV_LIKELY(n_pages < limit)) {
    rec_page_no = ibuf_rec_get_page_no(mtr, rec);
    rec_space_id = ibuf_rec_get_space(mtr, rec);

    if (rec_space_id != first_space_id ||
        (rec_page_no / IBUF_MERGE_AREA) != (first_page_no / IBUF_MERGE_AREA)) {
      break;
    }

    if (rec_page_no != prev_page_no || rec_space_id != prev_space_id) {
      n_pages++;
    }

    prev_page_no = rec_page_no;
    prev_space_id = rec_space_id;

    rec = page_rec_get_prev_const(rec);
  }

  rec = page_rec_get_next_const(rec);

  /* At the loop start there is no prev page; we mark this with a pair
  of space id, page no (0, 0) for which there can never be entries in
  the insert buffer */

  prev_page_no = 0;
  prev_space_id = 0;
  sum_volumes = 0;
  volume_for_page = 0;

  while (*n_stored < limit) {
    if (page_rec_is_supremum(rec)) {
      /* When no more records available, mark this with
      another 'impossible' pair of space id, page no */
      rec_page_no = 1;
      rec_space_id = 0;
    } else {
      rec_page_no = ibuf_rec_get_page_no(mtr, rec);
      rec_space_id = ibuf_rec_get_space(mtr, rec);
      /* In the system tablespace the smallest
      possible secondary index leaf page number is
      bigger than FSP_DICT_HDR_PAGE_NO (7).
      In all tablespaces, pages 0 and 1 are reserved
      for the allocation bitmap and the change
      buffer bitmap. In file-per-table tablespaces,
      a file segment inode page will be created at
      page 2 and the clustered index tree is created
      at page 3.  So for file-per-table tablespaces,
      page 4 is the smallest possible secondary
      index leaf page. CREATE TABLESPACE also initially
      uses pages 2 and 3 for the first created table,
      but that table may be dropped, allowing page 2
      to be reused for a secondary index leaf page.
      To keep this assertion simple, just
      make sure the page is >= 2. */
      ut_ad(rec_page_no >= FSP_FIRST_INODE_PAGE_NO);
    }

#ifdef UNIV_IBUF_DEBUG
    ut_a(*n_stored < IBUF_MAX_N_PAGES_MERGED);
#endif
    if ((rec_space_id != prev_space_id || rec_page_no != prev_page_no) &&
        (prev_space_id != 0 || prev_page_no != 0)) {
      if (contract ||
          (prev_page_no == first_page_no && prev_space_id == first_space_id) ||
          (volume_for_page > ((IBUF_MERGE_THRESHOLD - 1) * 4 * UNIV_PAGE_SIZE /
                              IBUF_PAGE_SIZE_PER_FREE_SPACE) /
                                 IBUF_MERGE_THRESHOLD)) {
        space_ids[*n_stored] = prev_space_id;
        page_nos[*n_stored] = prev_page_no;

        (*n_stored)++;

        sum_volumes += volume_for_page;
      }

      if (rec_space_id != first_space_id ||
          rec_page_no / IBUF_MERGE_AREA != first_page_no / IBUF_MERGE_AREA) {
        break;
      }

      volume_for_page = 0;
    }

    if (rec_page_no == 1 && rec_space_id == 0) {
      /* Supremum record */

      break;
    }

    rec_volume = ibuf_rec_get_volume(mtr, rec);

    volume_for_page += rec_volume;

    prev_page_no = rec_page_no;
    prev_space_id = rec_space_id;

    rec = page_rec_get_next_const(rec);
  }

#ifdef UNIV_IBUF_DEBUG
  ut_a(*n_stored <= IBUF_MAX_N_PAGES_MERGED);
#endif
#if 0
  fprintf(stderr, "Ibuf merge batch %lu pages %lu volume\n",
    *n_stored, sum_volumes);
#endif
  return (sum_volumes);
}

/** Get the matching records for space id.
 @return current rec or NULL */
[[nodiscard]] static const rec_t *ibuf_get_user_rec(
    btr_pcur_t *pcur, /*!< in: the current cursor */
    mtr_t *mtr)       /*!< in: mini-transaction */
{
  do {
    const rec_t *rec = pcur->get_rec();

    if (page_rec_is_user_rec(rec)) {
      return (rec);
    }
  } while (pcur->move_to_next(mtr));

  return (nullptr);
}

inline ulint ibuf_get_merge_page_nos(bool contract, const rec_t *rec,
                                     mtr_t *mtr [[maybe_unused]],
                                     space_id_t *ids, page_no_t *pages,
                                     ulint *n_stored) {
  return ibuf_get_merge_page_nos_func(contract, rec, IF_DEBUG(mtr, ) ids, pages,
                                      n_stored);
}

/** Reads page numbers for a space id from an ibuf tree.
 @return a lower limit for the combined volume of records which will be
 merged */
[[nodiscard]] static ulint ibuf_get_merge_pages(
    btr_pcur_t *pcur,   /*!< in/out: cursor */
    space_id_t space,   /*!< in: space for which to merge */
    ulint limit,        /*!< in: max page numbers to read */
    page_no_t *pages,   /*!< out: pages read */
    space_id_t *spaces, /*!< out: spaces read */
    ulint *n_pages,     /*!< out: number of pages read */
    mtr_t *mtr)         /*!< in: mini-transaction */
{
  const rec_t *rec;
  ulint volume = 0;

  ut_a(space != SPACE_UNKNOWN);

  *n_pages = 0;

  while ((rec = ibuf_get_user_rec(pcur, mtr)) != nullptr &&
         ibuf_rec_get_space(mtr, rec) == space && *n_pages < limit) {
    page_no_t page_no = ibuf_rec_get_page_no(mtr, rec);

    if (*n_pages == 0 || pages[*n_pages - 1] != page_no) {
      spaces[*n_pages] = space;
      pages[*n_pages] = page_no;
      ++*n_pages;
    }

    volume += ibuf_rec_get_volume(mtr, rec);

    pcur->move_to_next(mtr);
  }

  return (volume);
}

/** Contracts insert buffer trees by reading pages to the buffer pool.
 @return a lower limit for the combined size in bytes of entries which
 will be merged from ibuf trees to the pages read, 0 if ibuf is
 empty */
static ulint ibuf_merge_pages(
    ulint *n_pages, /*!< out: number of pages to which merged */
    bool sync)      /*!< in: true if the caller wants to wait for
                    the issued read with the highest tablespace
                    address to complete */
{
  mtr_t mtr;
  btr_pcur_t pcur;
  ulint sum_sizes;
  page_no_t page_nos[IBUF_MAX_N_PAGES_MERGED];
  space_id_t space_ids[IBUF_MAX_N_PAGES_MERGED];

  *n_pages = 0;

  /* The buf_read_ibuf_merge_pages(sync,..) will result in changes being applied
  to pages, which will generate redo log, so it is important to ensure redo log
  has enough space, if sync=true. We don't call log_free_check() here because
  during ibuf contraction, we are starting a nested mtr and, log_free_check()
  should have been called *before* starting the parent mtr. Usual background
  thread does not start under a parent mtr to do the page merges. It always
  does async IO though */
  ut_ad(mtr_t::is_this_thread_inside_mtr() || !sync);

  ibuf_mtr_start(&mtr);

  /* Open a cursor to a randomly chosen leaf of the tree, at a random
  position within the leaf */
  bool available;

  available = pcur.set_random_position(ibuf->index, BTR_SEARCH_LEAF, &mtr,
                                       UT_LOCATION_HERE);
  /* No one should make this index unavailable when server is running */
  ut_a(available);

  ut_ad(page_validate(pcur.get_page(), ibuf->index));

  if (page_is_empty(pcur.get_page())) {
    /* If a B-tree page is empty, it must be the root page
    and the whole B-tree must be empty. InnoDB does not
    allow empty B-tree pages other than the root. */
    ut_ad(ibuf->empty);
    ut_ad(page_get_space_id(pcur.get_page()) == IBUF_SPACE_ID);
    ut_ad(page_get_page_no(pcur.get_page()) == FSP_IBUF_TREE_ROOT_PAGE_NO);
    ut_ad(!mtr.has_any_log_record());

    ibuf_mtr_commit(&mtr);
    pcur.close();

    return (0);
  }

  sum_sizes = ibuf_get_merge_page_nos(true, pcur.get_rec(), &mtr, space_ids,
                                      page_nos, n_pages);
  ut_ad(!mtr.has_any_log_record());
  ibuf_mtr_commit(&mtr);
  pcur.close();

  buf_read_ibuf_merge_pages(sync, space_ids, page_nos, *n_pages);

  return (sum_sizes + 1);
}

/** Contracts insert buffer trees by reading pages referring to space_id
 to the buffer pool.
 @returns number of pages merged.*/
ulint ibuf_merge_space(space_id_t space) /*!< in: tablespace id to merge */
{
  mtr_t mtr;
  btr_pcur_t pcur;
  mem_heap_t *heap = mem_heap_create(512, UT_LOCATION_HERE);
  dtuple_t *tuple = ibuf_search_tuple_build(space, 0, heap);
  ulint n_pages = 0;

  ut_ad(!dict_sys_t::is_reserved(space));

  ibuf_mtr_start(&mtr);

  /* Position the cursor on the first matching record. */

  pcur.open(ibuf->index, 0, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
            UT_LOCATION_HERE);

  mem_heap_free(heap);

  ut_ad(page_validate(pcur.get_page(), ibuf->index));

  ulint sum_sizes = 0;
  page_no_t pages[IBUF_MAX_N_PAGES_MERGED];
  space_id_t spaces[IBUF_MAX_N_PAGES_MERGED];

  if (page_is_empty(pcur.get_page())) {
    /* If a B-tree page is empty, it must be the root page
    and the whole B-tree must be empty. InnoDB does not
    allow empty B-tree pages other than the root. */
    ut_ad(ibuf->empty);
    ut_ad(page_get_space_id(pcur.get_page()) == IBUF_SPACE_ID);
    ut_ad(page_get_page_no(pcur.get_page()) == FSP_IBUF_TREE_ROOT_PAGE_NO);

  } else {
    sum_sizes = ibuf_get_merge_pages(&pcur, space, IBUF_MAX_N_PAGES_MERGED,
                                     &pages[0], &spaces[0], &n_pages, &mtr);
    ib::info(ER_IB_MSG_606) << "Size of pages merged " << sum_sizes;
  }

  ibuf_mtr_commit(&mtr);

  pcur.close();

  if (n_pages > 0) {
    ut_ad(n_pages <= UT_ARR_SIZE(pages));

#ifdef UNIV_DEBUG
    for (ulint i = 0; i < n_pages; ++i) {
      ut_ad(spaces[i] == space);
    }
#endif /* UNIV_DEBUG */

    buf_read_ibuf_merge_pages(true, spaces, pages, n_pages);
  }

  return (n_pages);
}

/** Contract the change buffer by reading pages to the buffer pool.
@param[out]     n_pages         number of pages merged
@param[in]      sync            whether the caller waits for
the issued reads to complete
@return a lower limit for the combined size in bytes of entries which
will be merged from ibuf trees to the pages read, 0 if ibuf is
empty */
[[nodiscard]] static ulint ibuf_merge(ulint *n_pages, bool sync) {
  *n_pages = 0;

  /* We perform a dirty read of ibuf->empty, without latching
  the insert buffer root page. We trust this dirty read except
  when a slow shutdown is being executed. During a slow
  shutdown, the insert buffer merge must be completed. */

  if (ibuf->empty && srv_shutdown_state.load() < SRV_SHUTDOWN_CLEANUP) {
    return (0);
#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
  } else if (ibuf_debug) {
    return (0);
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */
  } else {
    return (ibuf_merge_pages(n_pages, sync));
  }
}

/** Contract the change buffer by reading pages to the buffer pool.
@param[in]      sync    whether the caller waits for
the issued reads to complete
@return a lower limit for the combined size in bytes of entries which
will be merged from ibuf trees to the pages read, 0 if ibuf is empty */
static ulint ibuf_contract(bool sync) {
  ulint n_pages;
  DEBUG_SYNC_C("ibuf_contract_started");
  return (ibuf_merge_pages(&n_pages, sync));
}

/** Contract the change buffer by reading pages to the buffer pool.
@param[in]      full            If true, do a full contraction based
on PCT_IO(100). If false, the size of contract batch is determined
based on the current size of the change buffer.
@return a lower limit for the combined size in bytes of entries which
will be merged from ibuf trees to the pages read, 0 if ibuf is
empty */
ulint ibuf_merge_in_background(bool full) {
  ulint sum_bytes = 0;
  ulint sum_pages = 0;
  ulint n_pag2;
  ulint n_pages;

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
  if (srv_ibuf_disable_background_merge) {
    return (0);
  }
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

  if (full) {
    /* Caller has requested a full batch */
    n_pages = PCT_IO(100);
  } else {
    /* By default we do a batch of 5% of the io_capacity */
    n_pages = PCT_IO(5);

    mutex_enter(&ibuf_mutex);

    /* If the ibuf->size is more than half the max_size
    then we make more aggressive contraction.
    +1 is to avoid division by zero. */
    if (ibuf->size > ibuf->max_size / 2) {
      ulint diff = ibuf->size - ibuf->max_size / 2;
      /* limits to around 100% value, for shrinking max_size case */
      diff = std::min(diff, ibuf->max_size);
      n_pages += PCT_IO((diff * 100) / (ibuf->max_size + 1));
    }

    mutex_exit(&ibuf_mutex);
  }

  while (sum_pages < n_pages) {
    ulint n_bytes;

    n_bytes = ibuf_merge(&n_pag2, false);

    if (n_bytes == 0) {
      return (sum_bytes);
    }

    sum_bytes += n_bytes;
    sum_pages += n_pag2;
  }

  return (sum_bytes);
}

/** Contract insert buffer trees after insert if they are too big. */
static inline void ibuf_contract_after_insert(
    ulint entry_size) /*!< in: size of a record which was inserted
                      into an ibuf tree */
{
  /* Perform dirty reads of ibuf->size and ibuf->max_size, to
  reduce ibuf_mutex contention. ibuf->max_size remains constant
  after ibuf_init_at_db_start(), but ibuf->size should be
  protected by ibuf_mutex. Given that ibuf->size fits in a
  machine word, this should be OK; at worst we are doing some
  excessive ibuf_contract() or occasionally skipping a
  ibuf_contract(). */
  auto size = ibuf->size;
  auto max_size = ibuf->max_size;

  if (size < max_size + IBUF_CONTRACT_ON_INSERT_NON_SYNC) {
    return;
  }

  auto sync = (size >= max_size + IBUF_CONTRACT_ON_INSERT_SYNC);

  /* Contract at least entry_size many bytes */
  ulint sum_sizes = 0;
  size = 1;

  do {
    size = ibuf_contract(sync);
    sum_sizes += size;
  } while (size > 0 && sum_sizes < entry_size);
}

/** Determine if an insert buffer record has been encountered already.
 @return true if a new record, false if possible duplicate */
static bool ibuf_get_volume_buffered_hash(
    const rec_t *rec,  /*!< in: ibuf record in post-4.1 format */
    const byte *types, /*!< in: fields */
    const byte *data,  /*!< in: start of user record data */
    ulint comp,        /*!< in: 0=ROW_FORMAT=REDUNDANT,
                       nonzero=ROW_FORMAT=COMPACT */
    ulint *hash,       /*!< in/out: hash array */
    ulint size)        /*!< in: number of elements in hash array */
{
  ulint len;
  ulint bitmask;

  len = ibuf_rec_get_size(
      rec, types, rec_get_n_fields_old_raw(rec) - IBUF_REC_FIELD_USER, comp);
  const auto hash_value = ut::hash_binary(data, len);

  hash += (hash_value / (CHAR_BIT * sizeof *hash)) % size;
  bitmask = static_cast<ulint>(1) << (hash_value % (CHAR_BIT * sizeof(*hash)));

  if (*hash & bitmask) {
    return false;
  }

  /* We have not seen this record yet.  Insert it. */
  *hash |= bitmask;

  return true;
}

/** Update the estimate of the number of records on a page, and
 get the space taken by merging the buffered record to the index page.
 @param[in] mtr mini-transaction owning rec
 @param[in] rec insert buffer record
 @param[in,out] hash hash array
 @param[in] size number of elements in hash array
 @param[in,out] n_recs estimated number of records on the page that rec points
 to
 @return size of index record in bytes + an upper limit of the space
 taken in the page directory */
static ulint ibuf_get_volume_buffered_count_func(IF_DEBUG(mtr_t *mtr, )
                                                     const rec_t *rec,
                                                 ulint *hash, ulint size,
                                                 lint *n_recs) {
  ulint len;
  ibuf_op_t ibuf_op;
  const byte *types;
  ulint n_fields;

  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(ibuf_inside(mtr));

  n_fields = rec_get_n_fields_old_raw(rec);
  ut_ad(n_fields > IBUF_REC_FIELD_USER);
  n_fields -= IBUF_REC_FIELD_USER;

  /* nullptr for index as it can't be clustered index */
  rec_get_nth_field_offs_old(nullptr, rec, 1, &len);
  /* This function is only invoked when buffering new
  operations.  All pre-4.1 records should have been merged
  when the database was started up. */
  ut_a(len == 1);

  if (rec_get_deleted_flag(rec, 0)) {
    /* This record has been merged already,
    but apparently the system crashed before
    the change was discarded from the buffer.
    Pretend that the record does not exist. */
    return (0);
  }

  types = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_METADATA, &len);

  switch (UNIV_EXPECT(len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE,
                      IBUF_REC_INFO_SIZE)) {
    default:
      ut_error;
    case 0:
      /* This ROW_TYPE=REDUNDANT record does not include an
      operation counter.  Exclude it from the *n_recs,
      because deletes cannot be buffered if there are
      old-style inserts buffered for the page. */

      len = ibuf_rec_get_size(rec, types, n_fields, 0);

      return (len + rec_get_converted_extra_size(len, n_fields, false) +
              page_dir_calc_reserved_space(1));
    case 1:
      /* This ROW_TYPE=COMPACT record does not include an
      operation counter.  Exclude it from the *n_recs,
      because deletes cannot be buffered if there are
      old-style inserts buffered for the page. */
      goto get_volume_comp;

    case IBUF_REC_INFO_SIZE:
      ibuf_op = (ibuf_op_t)types[IBUF_REC_OFFSET_TYPE];
      break;
  }

  switch (ibuf_op) {
    case IBUF_OP_INSERT:
      /* Inserts can be done by updating a delete-marked record.
      Because delete-mark and insert operations can be pointing to
      the same records, we must not count duplicates. */
    case IBUF_OP_DELETE_MARK:
      /* There must be a record to delete-mark.
      See if this record has been already buffered. */
      if (n_recs &&
          ibuf_get_volume_buffered_hash(
              rec, types + IBUF_REC_INFO_SIZE, types + len,
              types[IBUF_REC_OFFSET_FLAGS] & IBUF_REC_COMPACT, hash, size)) {
        (*n_recs)++;
      }

      if (ibuf_op == IBUF_OP_DELETE_MARK) {
        /* Setting the delete-mark flag does not
        affect the available space on the page. */
        return (0);
      }
      break;
    case IBUF_OP_DELETE:
      /* A record will be removed from the page. */
      if (n_recs) {
        (*n_recs)--;
      }
      /* While deleting a record actually frees up space,
      we have to play it safe and pretend that it takes no
      additional space (the record might not exist, etc.). */
      return (0);
    default:
      ut_error;
  }

  ut_ad(ibuf_op == IBUF_OP_INSERT);

get_volume_comp : {
  dtuple_t *entry;
  ulint volume;
  dict_index_t *dummy_index;
  mem_heap_t *heap = mem_heap_create(500, UT_LOCATION_HERE);

  entry = ibuf_build_entry_from_ibuf_rec_func(IF_DEBUG(mtr, ) rec, heap,
                                              &dummy_index);

  volume = rec_get_converted_size(dummy_index, entry);

  ibuf_dummy_index_free(dummy_index);
  mem_heap_free(heap);

  return (volume + page_dir_calc_reserved_space(1));
}
}

inline static ulint ibuf_get_volume_buffered_count(mtr_t *mtr [[maybe_unused]],
                                                   const rec_t *rec,
                                                   ulint *hash, ulint size,
                                                   lint *n_recs) {
  return ibuf_get_volume_buffered_count_func(IF_DEBUG(mtr, ) rec, hash, size,
                                             n_recs);
}

/** Gets an upper limit for the combined size of entries buffered in the
 insert buffer for a given page.
 @return upper limit for the volume of buffered inserts for the index
 page, in bytes; UNIV_PAGE_SIZE, if the entries for the index page span
 several pages in the insert buffer */
static ulint ibuf_get_volume_buffered(
    const btr_pcur_t *pcur, /*!< in: pcur positioned at a place in an
                            insert buffer tree where we would insert an
                            entry for the index page whose number is
                            page_no, latch mode has to be BTR_MODIFY_PREV
                            or BTR_MODIFY_TREE */
    space_id_t space_id,    /*!< in: space id */
    page_no_t page_no,      /*!< in: page number of an index page */
    lint *n_recs,           /*!< in/out: minimum number of records on the
                            page after the buffered changes have been
                            applied, or NULL to disable the counting */
    mtr_t *mtr)             /*!< in: mini-transaction of pcur */
{
  ulint volume;
  const rec_t *rec;
  const page_t *page;
  page_no_t prev_page_no;
  const page_t *prev_page;
  page_no_t next_page_no;
  const page_t *next_page;
  /* bitmap of buffered recs */
  ulint hash_bitmap[128 / sizeof(ulint)];

  ut_ad((pcur->m_latch_mode == BTR_MODIFY_PREV) ||
        (pcur->m_latch_mode == BTR_MODIFY_TREE));

  /* Count the volume of inserts earlier in the alphabetical order than
  pcur */

  volume = 0;

  if (n_recs) {
    memset(hash_bitmap, 0, sizeof hash_bitmap);
  }

  rec = pcur->get_rec();
  page = page_align(rec);
  ut_ad(page_validate(page, ibuf->index));

  if (page_rec_is_supremum(rec)) {
    rec = page_rec_get_prev_const(rec);
  }

  for (; !page_rec_is_infimum(rec); rec = page_rec_get_prev_const(rec)) {
    ut_ad(page_align(rec) == page);

    if (page_no != ibuf_rec_get_page_no(mtr, rec) ||
        space_id != ibuf_rec_get_space(mtr, rec)) {
      goto count_later;
    }

    volume += ibuf_get_volume_buffered_count(mtr, rec, hash_bitmap,
                                             UT_ARR_SIZE(hash_bitmap), n_recs);
  }

  /* Look at the previous page */

  prev_page_no = btr_page_get_prev(page, mtr);

  if (prev_page_no == FIL_NULL) {
    goto count_later;
  }

  {
    buf_block_t *block;

    block = buf_page_get(page_id_t(IBUF_SPACE_ID, prev_page_no), univ_page_size,
                         RW_X_LATCH, UT_LOCATION_HERE, mtr);

    buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);

    prev_page = buf_block_get_frame(block);
    ut_ad(page_validate(prev_page, ibuf->index));
  }

#ifdef UNIV_BTR_DEBUG
  ut_a(btr_page_get_next(prev_page, mtr) == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

  rec = page_get_supremum_rec(prev_page);
  rec = page_rec_get_prev_const(rec);

  for (;; rec = page_rec_get_prev_const(rec)) {
    ut_ad(page_align(rec) == prev_page);

    if (page_rec_is_infimum(rec)) {
      /* We cannot go to yet a previous page, because we
      do not have the x-latch on it, and cannot acquire one
      because of the latching order: we have to give up */

      return (UNIV_PAGE_SIZE);
    }

    if (page_no != ibuf_rec_get_page_no(mtr, rec) ||
        space_id != ibuf_rec_get_space(mtr, rec)) {
      goto count_later;
    }

    volume += ibuf_get_volume_buffered_count(mtr, rec, hash_bitmap,
                                             UT_ARR_SIZE(hash_bitmap), n_recs);
  }

count_later:
  rec = pcur->get_rec();

  if (!page_rec_is_supremum(rec)) {
    rec = page_rec_get_next_const(rec);
  }

  for (; !page_rec_is_supremum(rec); rec = page_rec_get_next_const(rec)) {
    if (page_no != ibuf_rec_get_page_no(mtr, rec) ||
        space_id != ibuf_rec_get_space(mtr, rec)) {
      return (volume);
    }

    volume += ibuf_get_volume_buffered_count(mtr, rec, hash_bitmap,
                                             UT_ARR_SIZE(hash_bitmap), n_recs);
  }

  /* Look at the next page */

  next_page_no = btr_page_get_next(page, mtr);

  if (next_page_no == FIL_NULL) {
    return (volume);
  }

  {
    buf_block_t *block;

    block = buf_page_get(page_id_t(IBUF_SPACE_ID, next_page_no), univ_page_size,
                         RW_X_LATCH, UT_LOCATION_HERE, mtr);

    buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);

    next_page = buf_block_get_frame(block);
    ut_ad(page_validate(next_page, ibuf->index));
  }

#ifdef UNIV_BTR_DEBUG
  ut_a(btr_page_get_prev(next_page, mtr) == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

  rec = page_get_infimum_rec(next_page);
  rec = page_rec_get_next_const(rec);

  for (;; rec = page_rec_get_next_const(rec)) {
    ut_ad(page_align(rec) == next_page);

    if (page_rec_is_supremum(rec)) {
      /* We give up */

      return (UNIV_PAGE_SIZE);
    }

    if (page_no != ibuf_rec_get_page_no(mtr, rec) ||
        space_id != ibuf_rec_get_space(mtr, rec)) {
      return (volume);
    }

    volume += ibuf_get_volume_buffered_count(mtr, rec, hash_bitmap,
                                             UT_ARR_SIZE(hash_bitmap), n_recs);
  }
}

/** Reads the biggest tablespace id from the high end of the insert buffer
 tree and updates the counter in fil_system. */
void ibuf_update_max_tablespace_id(void) {
  space_id_t max_space_id;
  const rec_t *rec;
  const byte *field;
  ulint len;
  btr_pcur_t pcur;
  mtr_t mtr;

  ut_a(!dict_table_is_comp(ibuf->index->table));

  ibuf_mtr_start(&mtr);

  pcur.open_at_side(false, ibuf->index, BTR_SEARCH_LEAF, true, 0, &mtr);

  ut_ad(page_validate(pcur.get_page(), ibuf->index));

  pcur.move_to_prev(&mtr);

  if (pcur.is_before_first_on_page()) {
    /* The tree is empty */

    max_space_id = 0;
  } else {
    rec = pcur.get_rec();

    field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_SPACE, &len);

    ut_a(len == 4);

    max_space_id = mach_read_from_4(field);
  }

  ibuf_mtr_commit(&mtr);

  /* printf("Maximum space id in insert buffer %lu\n", max_space_id); */

  fil_set_max_space_id_if_bigger(max_space_id);
}

/** Helper function for ibuf_get_entry_counter_func. Checks if rec is for
 (space, page_no), and if so, reads counter value from it and returns
 that + 1.
 @param[in] mtr mini-transaction of rec
 @param[in] rec insert buffer record
 @param[in] space space id
 @param[in] page_no page number
 @retval ULINT_UNDEFINED if the record does not contain any counter
 @retval 0 if the record is not for (space, page_no)
 @retval 1 + previous counter value, otherwise */
static ulint ibuf_get_entry_counter_low_func(IF_DEBUG(mtr_t *mtr, )
                                                 const rec_t *rec,
                                             space_id_t space,
                                             page_no_t page_no) {
  ulint counter;
  const byte *field;
  ulint len;

  ut_ad(ibuf_inside(mtr));
  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX) ||
        mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
  ut_ad(rec_get_n_fields_old_raw(rec) > 2);

  field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_MARKER, &len);

  ut_a(len == 1);

  /* Check the tablespace identifier. */
  field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_SPACE, &len);

  ut_a(len == 4);

  if (mach_read_from_4(field) != space) {
    return (0);
  }

  /* Check the page offset. */
  field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_PAGE, &len);
  ut_a(len == 4);

  if (mach_read_from_4(field) != page_no) {
    return (0);
  }

  /* Check if the record contains a counter field. */
  field = rec_get_nth_field_old(nullptr, rec, IBUF_REC_FIELD_METADATA, &len);

  switch (len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE) {
    default:
      ut_error;
    case 0: /* ROW_FORMAT=REDUNDANT */
    case 1: /* ROW_FORMAT=COMPACT */
      return (ULINT_UNDEFINED);

    case IBUF_REC_INFO_SIZE:
      counter = mach_read_from_2(field + IBUF_REC_OFFSET_COUNTER);
      ut_a(counter < 0xFFFF);
      return (counter + 1);
  }
}

/** Calculate the counter field for an entry based on the current
 last record in ibuf for (space, page_no).
 @return the counter field, or ULINT_UNDEFINED
 if we should abort this insertion to ibuf
 @param[in] space space id of entry
 @param[in] page_no page number of entry
 @param[in] rec the record preceding the insertion point
 @param mtr mini-transaction
 @param[in] only_leaf true if this is the only leaf page that can contain
 entries for (space,page_no), that is, there was no exact match for
 (space,page_no) in the node pointer */
static ulint ibuf_get_entry_counter_func(
    space_id_t space, page_no_t page_no, const rec_t *rec,
    IF_DEBUG(mtr_t *mtr, ) bool only_leaf) {
  ut_ad(ibuf_inside(mtr));
  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX));
  ut_ad(page_validate(page_align(rec), ibuf->index));

  if (page_rec_is_supremum(rec)) {
    /* This is just for safety. The record should be a
    page infimum or a user record. */
    ut_d(ut_error);
    ut_o(return (ULINT_UNDEFINED));
  } else if (!page_rec_is_infimum(rec)) {
    return (
        ibuf_get_entry_counter_low_func(IF_DEBUG(mtr, ) rec, space, page_no));
  } else if (only_leaf || fil_page_get_prev(page_align(rec)) == FIL_NULL) {
    /* The parent node pointer did not contain the
    searched for (space, page_no), which means that the
    search ended on the correct page regardless of the
    counter value, and since we're at the infimum record,
    there are no existing records. */
    return (0);
  } else {
    /* We used to read the previous page here. It would
    break the latching order, because the caller has
    buffer-fixed an insert buffer bitmap page. */
    return (ULINT_UNDEFINED);
  }
}

inline ulint ibuf_get_entry_counter(space_id_t space, page_no_t page_no,
                                    const rec_t *rec,
                                    mtr_t *mtr [[maybe_unused]],
                                    bool exact_leaf) {
  return ibuf_get_entry_counter_func(space, page_no, rec,
                                     IF_DEBUG(mtr, ) exact_leaf);
}

/** Buffer an operation in the insert/delete buffer, instead of doing it
directly to the disk page, if this is possible.
@param[in]      mode            BTR_MODIFY_PREV or BTR_MODIFY_TREE
@param[in]      op              operation type
@param[in]      no_counter      true=use 5.0.3 format; false=allow delete
buffering
@param[in]      entry           index entry to insert
@param[in]      entry_size      rec_get_converted_size(index, entry)
@param[in,out]  index           index where to insert; must not be
unique or clustered
@param[in]      page_id         page id where to insert
@param[in]      page_size       page size
@param[in,out]  thr             query thread
@return DB_SUCCESS, DB_STRONG_FAIL or other error */
[[nodiscard]] static dberr_t ibuf_insert_low(
    ulint mode, ibuf_op_t op, bool no_counter, const dtuple_t *entry,
    ulint entry_size, dict_index_t *index, const page_id_t &page_id,
    const page_size_t &page_size, que_thr_t *thr) {
  big_rec_t *dummy_big_rec;
  btr_pcur_t pcur;
  btr_cur_t *cursor;
  dtuple_t *ibuf_entry;
  mem_heap_t *offsets_heap = nullptr;
  mem_heap_t *heap;
  ulint *offsets = nullptr;
  ulint buffered;
  lint min_n_recs;
  rec_t *ins_rec;
  bool old_bit_value;
  page_t *bitmap_page;
  buf_block_t *block;
  page_t *root;
  dberr_t err;
  space_id_t space_ids[IBUF_MAX_N_PAGES_MERGED];
  page_no_t page_nos[IBUF_MAX_N_PAGES_MERGED];
  ulint n_stored = 0;
  mtr_t mtr;
  mtr_t bitmap_mtr;

  ut_a(!index->is_clustered());
  ut_ad(!dict_index_is_spatial(index));
  ut_ad(dtuple_check_typed(entry));
  ut_ad(!no_counter || op == IBUF_OP_INSERT);
  ut_a(op < IBUF_OP_COUNT);

  auto do_merge = false;

  /* Perform dirty reads of ibuf->size and ibuf->max_size, to
  reduce ibuf_mutex contention. Given that ibuf->max_size and
  ibuf->size fit in a machine word, this should be OK; at worst
  we are doing some excessive ibuf_contract() or occasionally
  skipping an ibuf_contract(). */
  if (ibuf->max_size == 0) {
    return (DB_STRONG_FAIL);
  }

  if (ibuf->size >= ibuf->max_size + IBUF_CONTRACT_DO_NOT_INSERT) {
    /* Insert buffer is now too big, contract it but do not try
    to insert */

#ifdef UNIV_IBUF_DEBUG
    ib::info() << "Ibuf too big";
#endif
    ibuf_contract(true);

    return (DB_STRONG_FAIL);
  }

  heap = mem_heap_create(1024, UT_LOCATION_HERE);

  /* Build the entry which contains the space id and the page number
  as the first fields and the type information for other fields, and
  which will be inserted to the insert buffer. Using a counter value
  of 0xFFFF we find the last record for (space, page_no), from which
  we can then read the counter value N and use N + 1 in the record we
  insert. (We patch the ibuf_entry's counter field to the correct
  value just before actually inserting the entry.) */

  ibuf_entry =
      ibuf_entry_build(op, index, entry, page_id.space(), page_id.page_no(),
                       no_counter ? ULINT_UNDEFINED : 0xFFFF, heap);

  /* Open a cursor to the insert buffer tree to calculate if we can add
  the new entry to it without exceeding the free space limit for the
  page. */

  if (BTR_LATCH_MODE_WITHOUT_INTENTION(mode) == BTR_MODIFY_TREE) {
    for (;;) {
      mutex_enter(&ibuf_pessimistic_insert_mutex);
      mutex_enter(&ibuf_mutex);

      if (UNIV_LIKELY(ibuf_data_enough_free_for_insert())) {
        break;
      }

      mutex_exit(&ibuf_mutex);
      mutex_exit(&ibuf_pessimistic_insert_mutex);

      if (!ibuf_add_free_page()) {
        mem_heap_free(heap);
        return (DB_STRONG_FAIL);
      }
    }
  }

  ibuf_mtr_start(&mtr);

  pcur.open(ibuf->index, 0, ibuf_entry, PAGE_CUR_LE, mode, &mtr,
            UT_LOCATION_HERE);
  ut_ad(page_validate(pcur.get_page(), ibuf->index));

  /* Find out the volume of already buffered inserts for the same index
  page */
  min_n_recs = 0;
  buffered = ibuf_get_volume_buffered(
      &pcur, page_id.space(), page_id.page_no(),
      op == IBUF_OP_DELETE ? &min_n_recs : nullptr, &mtr);

  if (op == IBUF_OP_DELETE &&
      (min_n_recs < 2 || buf_pool_watch_occurred(page_id))) {
    /* The page could become empty after the record is
    deleted, or the page has been read in to the buffer
    pool.  Refuse to buffer the operation. */

    /* The buffer pool watch is needed for IBUF_OP_DELETE
    because of latching order considerations.  We can
    check buf_pool_watch_occurred() only after latching
    the insert buffer B-tree pages that contain buffered
    changes for the page.  We never buffer IBUF_OP_DELETE,
    unless some IBUF_OP_INSERT or IBUF_OP_DELETE_MARK have
    been previously buffered for the page.  Because there
    are buffered operations for the page, the insert
    buffer B-tree page latches held by mtr will guarantee
    that no changes for the user page will be merged
    before mtr_commit(&mtr).  We must not mtr_commit(&mtr)
    until after the IBUF_OP_DELETE has been buffered. */

  fail_exit:
    if (BTR_LATCH_MODE_WITHOUT_INTENTION(mode) == BTR_MODIFY_TREE) {
      mutex_exit(&ibuf_mutex);
      mutex_exit(&ibuf_pessimistic_insert_mutex);
    }

    err = DB_STRONG_FAIL;
    goto func_exit;
  }

  /* After this point, the page could still be loaded to the
  buffer pool, but we do not have to care about it, since we are
  holding a latch on the insert buffer leaf page that contains
  buffered changes for (space, page_no).  If the page enters the
  buffer pool, buf_page_io_complete() for (space, page_no) will
  have to acquire a latch on the same insert buffer leaf page,
  which it cannot do until we have buffered the IBUF_OP_DELETE
  and done mtr_commit(&mtr) to release the latch. */

#ifdef UNIV_IBUF_COUNT_DEBUG
  ut_a((buffered == 0) || ibuf_count_get(page_id));
#endif
  ibuf_mtr_start(&bitmap_mtr);

  bitmap_page = ibuf_bitmap_get_map_page(page_id, page_size, UT_LOCATION_HERE,
                                         &bitmap_mtr);

  /* We check if the index page is suitable for buffered entries */

  if (buf_page_peek(page_id) || lock_rec_expl_exist_on_page(page_id)) {
    ibuf_mtr_commit(&bitmap_mtr);
    goto fail_exit;
  }

  if (op == IBUF_OP_INSERT) {
    ulint bits = ibuf_bitmap_page_get_bits(bitmap_page, page_id, page_size,
                                           IBUF_BITMAP_FREE, &bitmap_mtr);

    if (buffered + entry_size + page_dir_calc_reserved_space(1) >
        ibuf_index_page_calc_free_from_bits(page_size, bits)) {
      /* Release the bitmap page latch early. */
      ibuf_mtr_commit(&bitmap_mtr);

      /* It may not fit */
      do_merge = true;

      ibuf_get_merge_page_nos(false, pcur.get_rec(), &mtr, space_ids, page_nos,
                              &n_stored);

      goto fail_exit;
    }
  }

  if (!no_counter) {
    /* Patch correct counter value to the entry to
    insert. This can change the insert position, which can
    result in the need to abort in some cases. */
    ulint counter = ibuf_get_entry_counter(
        page_id.space(), page_id.page_no(), pcur.get_rec(), &mtr,
        pcur.get_btr_cur()->low_match < IBUF_REC_FIELD_METADATA);
    dfield_t *field;

    if (counter == ULINT_UNDEFINED) {
      ibuf_mtr_commit(&bitmap_mtr);
      goto fail_exit;
    }

    field = dtuple_get_nth_field(ibuf_entry, IBUF_REC_FIELD_METADATA);
    mach_write_to_2((byte *)dfield_get_data(field) + IBUF_REC_OFFSET_COUNTER,
                    counter);
  }

  /* Set the bitmap bit denoting that the insert buffer contains
  buffered entries for this index page, if the bit is not set yet */

  old_bit_value = ibuf_bitmap_page_get_bits(bitmap_page, page_id, page_size,
                                            IBUF_BITMAP_BUFFERED, &bitmap_mtr);

  if (!old_bit_value) {
    ibuf_bitmap_page_set_bits(bitmap_page, page_id, page_size,
                              IBUF_BITMAP_BUFFERED, true, &bitmap_mtr);
  }

  ibuf_mtr_commit(&bitmap_mtr);

  cursor = pcur.get_btr_cur();

  if (mode == BTR_MODIFY_PREV) {
    err = btr_cur_optimistic_insert(BTR_NO_LOCKING_FLAG, cursor, &offsets,
                                    &offsets_heap, ibuf_entry, &ins_rec,
                                    &dummy_big_rec, thr, &mtr);
    block = btr_cur_get_block(cursor);
    ut_ad(block->page.id.space() == IBUF_SPACE_ID);

    /* If this is the root page, update ibuf->empty. */
    if (block->page.id.page_no() == FSP_IBUF_TREE_ROOT_PAGE_NO) {
      const page_t *root = buf_block_get_frame(block);

      ut_ad(page_get_space_id(root) == IBUF_SPACE_ID);
      ut_ad(page_get_page_no(root) == FSP_IBUF_TREE_ROOT_PAGE_NO);

      ibuf->empty = page_is_empty(root);
    }
  } else {
    ut_ad(BTR_LATCH_MODE_WITHOUT_INTENTION(mode) == BTR_MODIFY_TREE);

    /* We acquire an sx-latch to the root page before the insert,
    because a pessimistic insert releases the tree x-latch,
    which would cause the sx-latching of the root after that to
    break the latching order. */

    root = ibuf_tree_root_get(&mtr);

    err = btr_cur_optimistic_insert(BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG,
                                    cursor, &offsets, &offsets_heap, ibuf_entry,
                                    &ins_rec, &dummy_big_rec, thr, &mtr);

    if (err == DB_FAIL) {
      err = btr_cur_pessimistic_insert(
          BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG, cursor, &offsets,
          &offsets_heap, ibuf_entry, &ins_rec, &dummy_big_rec, thr, &mtr);
    }

    mutex_exit(&ibuf_pessimistic_insert_mutex);
    ibuf_size_update(root);
    mutex_exit(&ibuf_mutex);
    ibuf->empty = page_is_empty(root);

    block = btr_cur_get_block(cursor);
    ut_ad(block->page.id.space() == IBUF_SPACE_ID);
  }

  if (offsets_heap) {
    mem_heap_free(offsets_heap);
  }

  if (err == DB_SUCCESS && op != IBUF_OP_DELETE) {
    /* Update the page max trx id field */
    page_update_max_trx_id(block, nullptr, thr_get_trx(thr)->id, &mtr);
  }

func_exit:
#ifdef UNIV_IBUF_COUNT_DEBUG
  if (err == DB_SUCCESS) {
    ib::info(ER_IB_MSG_607)
        << "Incrementing ibuf count of page " << page_id << " from "
        << ibuf_count_get(space, page_no) << " by 1";

    ibuf_count_set(page_id, ibuf_count_get(page_id) + 1);
  }
#endif

  ibuf_mtr_commit(&mtr);
  pcur.close();

  mem_heap_free(heap);

  if (err == DB_SUCCESS &&
      BTR_LATCH_MODE_WITHOUT_INTENTION(mode) == BTR_MODIFY_TREE) {
    ibuf_contract_after_insert(entry_size);
  }

  if (do_merge) {
#ifdef UNIV_IBUF_DEBUG
    ut_a(n_stored <= IBUF_MAX_N_PAGES_MERGED);
#endif
    buf_read_ibuf_merge_pages(false, space_ids, page_nos, n_stored);
  }

  return (err);
}

/** Buffer an operation in the insert/delete buffer, instead of doing it
directly to the disk page, if this is possible. Does not do it if the index
is clustered or unique.
@param[in]      op              operation type
@param[in]      entry           index entry to insert
@param[in,out]  index           index where to insert
@param[in]      page_id         page id where to insert
@param[in]      page_size       page size
@param[in,out]  thr             query thread
@return true if success */
bool ibuf_insert(ibuf_op_t op, const dtuple_t *entry, dict_index_t *index,
                 const page_id_t &page_id, const page_size_t &page_size,
                 que_thr_t *thr) {
  dberr_t err;
  ulint entry_size;
  /* Read the settable global variable ibuf_use only once in
  this function, so that we will have a consistent view of it. */
  assert(innodb_change_buffering <= IBUF_USE_ALL);
  ibuf_use_t use = static_cast<ibuf_use_t>(innodb_change_buffering);

  DBUG_TRACE;

  DBUG_PRINT("ibuf", ("op: %d, space: " UINT32PF ", page_no: " UINT32PF, op,
                      page_id.space(), page_id.page_no()));

  ut_ad(dtuple_check_typed(entry));
  ut_ad(!fsp_is_system_temporary(page_id.space()));

  ut_a(!index->is_clustered());

  auto no_counter = use <= IBUF_USE_INSERT;

  switch (op) {
    case IBUF_OP_INSERT:
      switch (use) {
        case IBUF_USE_NONE:
        case IBUF_USE_DELETE:
        case IBUF_USE_DELETE_MARK:
          return false;
        case IBUF_USE_INSERT:
        case IBUF_USE_INSERT_DELETE_MARK:
        case IBUF_USE_ALL:
          goto check_watch;
      }
      break;
    case IBUF_OP_DELETE_MARK:
      switch (use) {
        case IBUF_USE_NONE:
        case IBUF_USE_INSERT:
          return false;
        case IBUF_USE_DELETE_MARK:
        case IBUF_USE_DELETE:
        case IBUF_USE_INSERT_DELETE_MARK:
        case IBUF_USE_ALL:
          ut_ad(!no_counter);
          goto check_watch;
      }
      break;
    case IBUF_OP_DELETE:
      switch (use) {
        case IBUF_USE_NONE:
        case IBUF_USE_INSERT:
        case IBUF_USE_INSERT_DELETE_MARK:
          return false;
        case IBUF_USE_DELETE_MARK:
        case IBUF_USE_DELETE:
        case IBUF_USE_ALL:
          ut_ad(!no_counter);
          goto skip_watch;
      }
      break;
    case IBUF_OP_COUNT:
      break;
  }

  /* unknown op or use */
  ut_error;

check_watch:
  /* If a thread attempts to buffer an insert on a page while a
  purge is in progress on the same page, the purge must not be
  buffered, because it could remove a record that was
  re-inserted later.  For simplicity, we block the buffering of
  all operations on a page that has a purge pending.

  We do not check this in the IBUF_OP_DELETE case, because that
  would always trigger the buffer pool watch during purge and
  thus prevent the buffering of delete operations.  We assume
  that the issuer of IBUF_OP_DELETE has called
  buf_pool_watch_set(space, page_no). */

  {
    buf_pool_t *buf_pool = buf_pool_get(page_id);
    buf_page_t *bpage = buf_page_get_also_watch(buf_pool, page_id);

    if (bpage != nullptr) {
      /* A buffer pool watch has been set or the
      page has been read into the buffer pool.
      Do not buffer the request.  If a purge operation
      is being buffered, have this request executed
      directly on the page in the buffer pool after the
      buffered entries for this page have been merged. */
      return false;
    }
  }

skip_watch:
  entry_size = rec_get_converted_size(index, entry);

  if (entry_size >=
      page_get_free_space_of_empty(dict_table_is_comp(index->table)) / 2) {
    return false;
  }

  err = ibuf_insert_low(BTR_MODIFY_PREV, op, no_counter, entry, entry_size,
                        index, page_id, page_size, thr);
  if (err == DB_FAIL) {
    err =
        ibuf_insert_low(BTR_MODIFY_TREE | BTR_LATCH_FOR_INSERT, op, no_counter,
                        entry, entry_size, index, page_id, page_size, thr);
  }

  if (err == DB_SUCCESS) {
    /*
    #if defined(UNIV_IBUF_DEBUG)
                    fprintf(stderr, "Ibuf insert for page no %lu of index %s\n",
                            page_no, index->name);
    #endif
    */
    return true;

  } else {
    ut_a(err == DB_STRONG_FAIL || err == DB_TOO_BIG_RECORD);

    return false;
  }
}

/** During merge, inserts to an index page a secondary index entry extracted
 from the insert buffer.
 @return        newly inserted record */
static rec_t *ibuf_insert_to_index_page_low(
    const dtuple_t *entry, /*!< in: buffered entry to insert */
    buf_block_t *block,    /*!< in/out: index page where the buffered
                           entry should be placed */
    dict_index_t *index,   /*!< in: record descriptor */
    ulint **offsets,       /*!< out: offsets on *rec */
    mem_heap_t *heap,      /*!< in/out: memory heap */
    mtr_t *mtr,            /*!< in/out: mtr */
    page_cur_t *page_cur)  /*!< in/out: cursor positioned on the record
                          after which to insert the buffered entry */
{
  const page_t *page;
  const page_t *bitmap_page;
  ulint old_bits;
  rec_t *rec;
  DBUG_TRACE;

  rec = page_cur_tuple_insert(page_cur, entry, index, offsets, &heap, mtr);
  if (rec != nullptr) {
    return rec;
  }

  /* Page reorganization or recompression should already have
  been attempted by page_cur_tuple_insert(). Besides, per
  ibuf_index_page_calc_free_zip() the page should not have been
  recompressed or reorganized. */
  ut_ad(!buf_block_get_page_zip(block));

  /* If the record did not fit, reorganize */

  btr_page_reorganize(page_cur, index, mtr);

  /* This time the record must fit */

  rec = page_cur_tuple_insert(page_cur, entry, index, offsets, &heap, mtr);
  if (rec != nullptr) {
    return rec;
  }

  page = buf_block_get_frame(block);

  ib::error(ER_IB_MSG_608) << "Insert buffer insert fails; page free "
                           << page_get_max_insert_size(page, 1)
                           << ", dtuple size "
                           << rec_get_converted_size(index, entry);

  fputs("InnoDB: Cannot insert index record ", stderr);
  dtuple_print(stderr, entry);
  fputs(
      "\nInnoDB: The table where this index record belongs\n"
      "InnoDB: is now probably corrupt. Please run CHECK TABLE on\n"
      "InnoDB: that table.\n",
      stderr);

  bitmap_page = ibuf_bitmap_get_map_page(block->page.id, block->page.size,
                                         UT_LOCATION_HERE, mtr);
  old_bits = ibuf_bitmap_page_get_bits(bitmap_page, block->page.id,
                                       block->page.size, IBUF_BITMAP_FREE, mtr);

  ib::error(ER_IB_MSG_609) << "page " << block->page.id << ", size "
                           << block->page.size.physical() << ", bitmap bits "
                           << old_bits;

  ib::error(ER_IB_MSG_SUBMIT_DETAILED_BUG_REPORT);

  ut_d(ut_error);
  ut_o(return nullptr);
}

/************************************************************************
During merge, inserts to an index page a secondary index entry extracted
from the insert buffer. */
static void ibuf_insert_to_index_page(
    const dtuple_t *entry, /*!< in: buffered entry to insert */
    buf_block_t *block,    /*!< in/out: index page where the buffered entry
                           should be placed */
    dict_index_t *index,   /*!< in: record descriptor */
    mtr_t *mtr)            /*!< in: mtr */
{
  page_cur_t page_cur;
  ulint low_match;
  page_t *page = buf_block_get_frame(block);
  rec_t *rec;
  ulint *offsets;
  mem_heap_t *heap;

  DBUG_TRACE;

  DBUG_PRINT("ibuf", ("page " UINT32PF ":" UINT32PF, block->page.id.space(),
                      block->page.id.page_no()));

  ut_ad(!dict_index_is_online_ddl(index));  // this is an ibuf_dummy index
  ut_ad(ibuf_inside(mtr));
  ut_ad(dtuple_check_typed(entry));
  /* A change buffer merge must occur before users are granted
  any access to the page. No adaptive hash index entries may
  point to a freshly read page. */
  ut_ad(!block->ahi.index);
  block->ahi.assert_empty();

  if (UNIV_UNLIKELY(dict_table_is_comp(index->table) != page_is_comp(page))) {
    ib::warn(ER_IB_MSG_611)
        << "Trying to insert a record from the insert"
           " buffer to an index page but the 'compact' flag does"
           " not match!";
    goto dump;
  }

  rec = page_rec_get_next(page_get_infimum_rec(page));

  if (page_rec_is_supremum(rec)) {
    ib::warn(ER_IB_MSG_612) << "Trying to insert a record from the insert"
                               " buffer to an index page but the index page"
                               " is empty!";
    goto dump;
  }

  if (!rec_n_fields_is_sane(index, rec, entry)) {
    ib::warn(ER_IB_MSG_613)
        << "Trying to insert a record from the insert"
           " buffer to an index page but the number of fields"
           " does not match!";
    rec_print(stderr, rec, index);
  dump:
    dtuple_print(stderr, entry);
    ib::warn(ER_IB_MSG_614)
        << "The table where this index record belongs"
           " is now probably corrupt. Please run CHECK TABLE on"
           " your tables.";
    ib::warn(ER_IB_MSG_SUBMIT_DETAILED_BUG_REPORT);

    ut_d(ut_error);

    ut_o(return);
  }

  low_match = page_cur_search(block, index, entry, &page_cur);

  heap = mem_heap_create(
      sizeof(upd_t) + REC_OFFS_HEADER_SIZE * sizeof(*offsets) +
          dtuple_get_n_fields(entry) * (sizeof(upd_field_t) + sizeof *offsets),
      UT_LOCATION_HERE);

  if (UNIV_UNLIKELY(low_match == dtuple_get_n_fields(entry))) {
    upd_t *update;
    page_zip_des_t *page_zip;

    rec = page_cur_get_rec(&page_cur);

    /* This is based on
    row_ins_sec_index_entry_by_modify(BTR_MODIFY_LEAF). */
    ut_ad(rec_get_deleted_flag(rec, page_is_comp(page)));

    offsets = rec_get_offsets(rec, index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);
    update = row_upd_build_sec_rec_difference_binary(rec, index, offsets, entry,
                                                     heap);

    page_zip = buf_block_get_page_zip(block);

    if (update->n_fields == 0) {
      /* The records only differ in the delete-mark.
      Clear the delete-mark, like we did before
      Bug #56680 was fixed. */
      btr_cur_set_deleted_flag_for_ibuf(rec, page_zip, false, mtr);
      goto updated_in_place;
    }

    /* Copy the info bits. Clear the delete-mark. */
    update->info_bits = rec_get_info_bits(rec, page_is_comp(page));
    update->info_bits &= ~REC_INFO_DELETED_FLAG;

    /* We cannot invoke btr_cur_optimistic_update() here,
    because we do not have a btr_cur_t or que_thr_t,
    as the insert buffer merge occurs at a very low level. */
    if (!row_upd_changes_field_size_or_external(index, offsets, update) &&
        (!page_zip ||
         btr_cur_update_alloc_zip(page_zip, &page_cur, index, offsets,
                                  rec_offs_size(offsets), false, mtr))) {
      /* This is the easy case. Do something similar
      to btr_cur_update_in_place(). */
      rec = page_cur_get_rec(&page_cur);
      row_upd_rec_in_place(rec, index, offsets, update, page_zip);

      /* Log the update in place operation. During recovery
      MLOG_COMP_REC_UPDATE_IN_PLACE/MLOG_REC_UPDATE_IN_PLACE
      expects trx_id, roll_ptr for secondary indexes. So we
      just write dummy trx_id(0), roll_ptr(0) */
      btr_cur_update_in_place_log(BTR_KEEP_SYS_FLAG, rec, index, update, 0, 0,
                                  mtr);

      DBUG_EXECUTE_IF("crash_after_log_ibuf_upd_inplace",
                      log_buffer_flush_to_disk();
                      ib::info(ER_IB_MSG_615) << "Wrote log record for ibuf"
                                                 " update in place operation";
                      DBUG_SUICIDE(););

      goto updated_in_place;
    }

    /* btr_cur_update_alloc_zip() may have changed this */
    rec = page_cur_get_rec(&page_cur);

    /* A collation may identify values that differ in
    storage length.
    Some examples (1 or 2 bytes):
    utf8mb3_turkish_ci: I = U+0131 LATIN SMALL LETTER DOTLESS I
    utf8mb3_general_ci: S = U+00DF LATIN SMALL LETTER SHARP S
    utf8mb3_general_ci: A = U+00E4 LATIN SMALL LETTER A WITH DIAERESIS

    latin1_german2_ci: SS = U+00DF LATIN SMALL LETTER SHARP S

    Examples of a character (3-byte UTF-8 sequence)
    identified with 2 or 4 characters (1-byte UTF-8 sequences):

    utf8mb3_unicode_ci: 'II' = U+2171 SMALL ROMAN NUMERAL TWO
    utf8mb3_unicode_ci: '(10)' = U+247D PARENTHESIZED NUMBER TEN
    */

    /* Delete the different-length record, and insert the
    buffered one. */

    lock_rec_store_on_page_infimum(block, rec);
    page_cur_delete_rec(&page_cur, index, offsets, mtr);
    page_cur_move_to_prev(&page_cur);
    rec = ibuf_insert_to_index_page_low(entry, block, index, &offsets, heap,
                                        mtr, &page_cur);

    ut_ad(!cmp_dtuple_rec(entry, rec, index, offsets));
    lock_rec_restore_from_page_infimum(block, rec, block);
  } else {
    offsets = nullptr;
    ibuf_insert_to_index_page_low(entry, block, index, &offsets, heap, mtr,
                                  &page_cur);
  }
updated_in_place:
  mem_heap_free(heap);
}

/** During merge, sets the delete mark on a record for a secondary index
 entry. */
static void ibuf_set_del_mark(
    const dtuple_t *entry,     /*!< in: entry */
    buf_block_t *block,        /*!< in/out: block */
    const dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)                /*!< in: mtr */
{
  page_cur_t page_cur;
  ulint low_match;

  ut_ad(ibuf_inside(mtr));
  ut_ad(dtuple_check_typed(entry));

  low_match = page_cur_search(block, index, entry, &page_cur);

  if (low_match == dtuple_get_n_fields(entry)) {
    rec_t *rec;
    page_zip_des_t *page_zip;

    rec = page_cur_get_rec(&page_cur);
    page_zip = page_cur_get_page_zip(&page_cur);

    /* Delete mark the old index record. According to a
    comment in row_upd_sec_index_entry(), it can already
    have been delete marked if a lock wait occurred in
    row_ins_sec_index_entry() in a previous invocation of
    row_upd_sec_index_entry(). */

    if (UNIV_LIKELY(
            !rec_get_deleted_flag(rec, dict_table_is_comp(index->table)))) {
      btr_cur_set_deleted_flag_for_ibuf(rec, page_zip, true, mtr);
    }
  } else {
    const page_t *page = page_cur_get_page(&page_cur);
    const buf_block_t *block = page_cur_get_block(&page_cur);

    ib::error(ER_IB_MSG_616) << "Unable to find a record to delete-mark";
    fputs("InnoDB: tuple ", stderr);
    dtuple_print(stderr, entry);
    fputs(
        "\n"
        "InnoDB: record ",
        stderr);
    rec_print(stderr, page_cur_get_rec(&page_cur), index);

    ib::error(ER_IB_MSG_617)
        << "page " << block->page.id << " (" << page_get_n_recs(page)
        << " records, index id " << btr_page_get_index_id(page) << ").";

    ib::error(ER_IB_MSG_SUBMIT_DETAILED_BUG_REPORT);
    ut_d(ut_error);
  }
}

/** During merge, delete a record for a secondary index entry. */
static void ibuf_delete(const dtuple_t *entry, /*!< in: entry */
                        buf_block_t *block,    /*!< in/out: block */
                        dict_index_t *index,   /*!< in: record descriptor */
                        mtr_t *mtr) /*!< in/out: mtr; must be committed
                                    before latching any further pages */
{
  page_cur_t page_cur;
  ulint low_match;

  ut_ad(ibuf_inside(mtr));
  ut_ad(dtuple_check_typed(entry));
  ut_ad(!dict_index_is_spatial(index));

  low_match = page_cur_search(block, index, entry, &page_cur);

  if (low_match == dtuple_get_n_fields(entry)) {
    page_zip_des_t *page_zip = buf_block_get_page_zip(block);
    page_t *page = buf_block_get_frame(block);
    rec_t *rec = page_cur_get_rec(&page_cur);

    /* TODO: the below should probably be a separate function,
    it's a bastardized version of btr_cur_optimistic_delete. */

    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    ulint *offsets = offsets_;
    mem_heap_t *heap = nullptr;
    ulint max_ins_size = 0;

    rec_offs_init(offsets_);

    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    if (page_get_n_recs(page) <= 1 ||
        !(REC_INFO_DELETED_FLAG & rec_get_info_bits(rec, page_is_comp(page)))) {
      /* Refuse to purge the last record or a
      record that has not been marked for deletion. */
      ib::error(ER_IB_MSG_619) << "Unable to purge a record";
      fputs("InnoDB: tuple ", stderr);
      dtuple_print(stderr, entry);
      fputs(
          "\n"
          "InnoDB: record ",
          stderr);
      rec_print_new(stderr, rec, offsets);
      fprintf(stderr,
              "\nspace " UINT32PF " offset " UINT32PF
              " (%u records, index id %llu)\n"
              "InnoDB: Submit a detailed bug report"
              " to http://bugs.mysql.com\n",
              block->page.id.space(), block->page.id.page_no(),
              (unsigned)page_get_n_recs(page),
              (ulonglong)btr_page_get_index_id(page));

      ut_d(ut_error);
      ut_o(return);
    }

    lock_update_delete(block, rec);

    if (!page_zip) {
      max_ins_size = page_get_max_insert_size_after_reorganize(page, 1);
    }
#ifdef UNIV_ZIP_DEBUG
    ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
    page_cur_delete_rec(&page_cur, index, offsets, mtr);
#ifdef UNIV_ZIP_DEBUG
    ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

    if (page_zip) {
      ibuf_update_free_bits_zip(block, mtr);
    } else {
      ibuf_update_free_bits_low(block, max_ins_size, mtr);
    }

    if (UNIV_LIKELY_NULL(heap)) {
      mem_heap_free(heap);
    }
  } else {
    /* The record must have been purged already. */
  }
}

/** Restores insert buffer tree cursor position
@param[in]        space_id      Tablespace id
@param[in]        page_no       index page number where the record should belong
@param [in]       search_tuple  search tuple for entries of page_no
@param[in]        mode       BTR_MODIFY_LEAF or BTR_MODIFY_TREE
@param[in,out]    pcur       persistent cursor whose position is to be restored
@param[in, out]   mtr        mini-transaction
@return true if the position was restored; false if not */
static bool ibuf_restore_pos(space_id_t space_id, page_no_t page_no,
                             const dtuple_t *search_tuple, ulint mode,
                             btr_pcur_t *pcur, mtr_t *mtr) {
  ut_ad(mode == BTR_MODIFY_LEAF ||
        BTR_LATCH_MODE_WITHOUT_INTENTION(mode) == BTR_MODIFY_TREE);

  if (pcur->restore_position(mode, mtr, UT_LOCATION_HERE)) {
    return true;
  }

  if (const auto space = fil_space_acquire_silent(space_id); space == nullptr) {
    /* The tablespace has been(or being) deleted. Do not complain. */
    ibuf_btr_pcur_commit_specify_mtr(pcur, mtr);
  } else {
    fil_space_release(space);
    ib::error(ER_IB_MSG_IBUF_CURSOR_RESTORATION_FAILED, space_id, page_no);
    ib::error(ER_IB_MSG_SUBMIT_DETAILED_BUG_REPORT);

    rec_print_old(stderr, pcur->get_rec());
    rec_print_old(stderr, pcur->m_old_rec);
    dtuple_print(stderr, search_tuple);

    rec_print_old(stderr, page_rec_get_next(pcur->get_rec()));

    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_IBUF_FAILED_TO_RESTORE_POSITION);
  }
  return false;
}

/** Deletes from ibuf the record on which pcur is positioned. If we have to
 resort to a pessimistic delete, this function commits mtr and closes
 the cursor.
 @return true if mtr was committed and pcur closed in this operation */
[[nodiscard]] static bool ibuf_delete_rec(
    space_id_t space,  /*!< in: space id */
    page_no_t page_no, /*!< in: index page number that the record
                       should belong to */
    btr_pcur_t *pcur,  /*!< in: pcur positioned on the record to
                       delete, having latch mode BTR_MODIFY_LEAF */
    const dtuple_t *search_tuple,
    /*!< in: search tuple for entries of page_no */
    mtr_t *mtr) /*!< in: mtr */
{
  page_t *root;
  dberr_t err;

  ut_ad(ibuf_inside(mtr));
  ut_ad(page_rec_is_user_rec(pcur->get_rec()));
  ut_ad(ibuf_rec_get_page_no(mtr, pcur->get_rec()) == page_no);
  ut_ad(ibuf_rec_get_space(mtr, pcur->get_rec()) == space);

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
  if (ibuf_debug == 2) {
    /* Inject a fault (crash). We do this before trying
    optimistic delete, because a pessimistic delete in the
    change buffer would require a larger test case. */

    /* Flag the buffered record as processed, to avoid
    an assertion failure after crash recovery. */
    btr_cur_set_deleted_flag_for_ibuf(pcur->get_rec(), nullptr, true, mtr);

    ibuf_mtr_commit(mtr);
    log_buffer_flush_to_disk();
    DBUG_SUICIDE();
  }
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

  auto success = btr_cur_optimistic_delete(pcur->get_btr_cur(), 0, mtr);

  const page_id_t page_id(space, page_no);

  if (success) {
    if (page_is_empty(pcur->get_page())) {
      /* If a B-tree page is empty, it must be the root page
      and the whole B-tree must be empty. InnoDB does not
      allow empty B-tree pages other than the root. */
      root = pcur->get_page();

      ut_ad(page_get_space_id(root) == IBUF_SPACE_ID);
      ut_ad(page_get_page_no(root) == FSP_IBUF_TREE_ROOT_PAGE_NO);

      /* ibuf->empty is protected by the root page latch.
      Before the deletion, it had to be false. */
      ut_ad(!ibuf->empty);
      ibuf->empty = true;
    }

#ifdef UNIV_IBUF_COUNT_DEBUG
    ib::info(ER_IB_MSG_623)
        << "Decrementing ibuf count of space " << space << " page " << page_no
        << " from " << ibuf_count_get(page_id) << " by 1";

    ibuf_count_set(page_id, ibuf_count_get(page_id) - 1);
#endif /* UNIV_IBUF_COUNT_DEBUG */

    return false;
  }

  ut_ad(page_rec_is_user_rec(pcur->get_rec()));
  ut_ad(ibuf_rec_get_page_no(mtr, pcur->get_rec()) == page_no);
  ut_ad(ibuf_rec_get_space(mtr, pcur->get_rec()) == space);

  /* We have to resort to a pessimistic delete from ibuf.
  Delete-mark the record so that it will not be applied again,
  in case the server crashes before the pessimistic delete is
  made persistent. */
  btr_cur_set_deleted_flag_for_ibuf(pcur->get_rec(), nullptr, true, mtr);

  pcur->store_position(mtr);
  ibuf_btr_pcur_commit_specify_mtr(pcur, mtr);

  ibuf_mtr_start(mtr);

  mutex_enter(&ibuf_mutex);

  if (!ibuf_restore_pos(space, page_no, search_tuple,
                        BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE, pcur, mtr)) {
    mutex_exit(&ibuf_mutex);
    ut_ad(mtr->has_committed());
    goto func_exit;
  }

  root = ibuf_tree_root_get(mtr);

  btr_cur_pessimistic_delete(&err, true, pcur->get_btr_cur(), 0, false, 0, 0, 0,
                             mtr, nullptr, nullptr);
  ut_a(err == DB_SUCCESS);

#ifdef UNIV_IBUF_COUNT_DEBUG
  ibuf_count_set(page_id, ibuf_count_get(page_id) - 1);
#endif /* UNIV_IBUF_COUNT_DEBUG */

  ibuf_size_update(root);
  mutex_exit(&ibuf_mutex);

  ibuf->empty = page_is_empty(root);
  ibuf_btr_pcur_commit_specify_mtr(pcur, mtr);

func_exit:
  ut_ad(mtr->has_committed());
  pcur->close();

  return true;
}

/** When an index page is read from a disk to the buffer pool, this function
applies any buffered operations to the page and deletes the entries from the
insert buffer. If the page is not read, but created in the buffer pool, this
function deletes its buffered entries from the insert buffer; there can
exist entries for such a page if the page belonged to an index which
subsequently was dropped.
@param[in,out]  block                   if page has been read from disk,
pointer to the page x-latched, else NULL
@param[in]      page_id                 page id of the index page
@param[in]      update_ibuf_bitmap      normally this is set to true, but
if we have deleted or are deleting the tablespace, then we naturally do not
want to update a non-existent bitmap page
@param[in]      page_size               page size */
void ibuf_merge_or_delete_for_page(buf_block_t *block, const page_id_t &page_id,
                                   const page_size_t *page_size,
                                   bool update_ibuf_bitmap) {
  mem_heap_t *heap;
  btr_pcur_t pcur;
  dtuple_t *search_tuple;
#ifdef UNIV_IBUF_DEBUG
  ulint volume = 0;
#endif /* UNIV_IBUF_DEBUG */
  page_zip_des_t *page_zip = nullptr;
  fil_space_t *space = nullptr;
  bool corruption_noticed = false;
  bool success;
  mtr_t mtr;

  /* Counts for merged & discarded operations. */
  ulint mops[IBUF_OP_COUNT];
  ulint dops[IBUF_OP_COUNT];

  ut_ad(block == nullptr || page_id == block->page.id);
  ut_ad(block == nullptr || block->page.is_io_fix_read());

  if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE ||
      trx_sys_hdr_page(page_id) || fsp_is_system_temporary(page_id.space())) {
    return;
  }

  /* We cannot refer to page_size in the following, because it is passed
  as NULL (it is unknown) when buf_read_ibuf_merge_pages() is merging
  (discarding) changes for a dropped tablespace. When block != NULL or
  update_ibuf_bitmap is specified, then page_size must be known.
  That is why we will repeat the check below, with page_size in
  place of univ_page_size. Passing univ_page_size assumes that the
  uncompressed page size always is a power-of-2 multiple of the
  compressed page size. */

  if (ibuf_fixed_addr_page(page_id, univ_page_size) ||
      fsp_descr_page(page_id, univ_page_size)) {
    return;
  }

  if (update_ibuf_bitmap) {
    ut_ad(page_size != nullptr);

    if (ibuf_fixed_addr_page(page_id, *page_size) ||
        fsp_descr_page(page_id, *page_size)) {
      return;
    }

    space = fil_space_acquire_silent(page_id.space());

    if (space == nullptr) {
      /* Do not try to read the bitmap page from space;
      just delete the ibuf records for the page */

      block = nullptr;
      update_ibuf_bitmap = false;
    } else {
      page_t *bitmap_page;
      ulint bitmap_bits;

      ibuf_mtr_start(&mtr);

      bitmap_page =
          ibuf_bitmap_get_map_page(page_id, *page_size, UT_LOCATION_HERE, &mtr);

      bitmap_bits = ibuf_bitmap_page_get_bits(bitmap_page, page_id, *page_size,
                                              IBUF_BITMAP_BUFFERED, &mtr);

      ibuf_mtr_commit(&mtr);

      if (!bitmap_bits) {
        /* No inserts buffered for this page */

        fil_space_release(space);
        return;
      }
    }
  } else if (block != nullptr && (ibuf_fixed_addr_page(page_id, *page_size) ||
                                  fsp_descr_page(page_id, *page_size))) {
    return;
  }

  heap = mem_heap_create(512, UT_LOCATION_HERE);

  search_tuple =
      ibuf_search_tuple_build(page_id.space(), page_id.page_no(), heap);

  if (block != nullptr) {
    /* Move the ownership of the x-latch on the page to this OS
    thread, so that we can acquire a second x-latch on it. This
    is needed for the insert operations to the index page to pass
    the debug checks. */

    rw_lock_x_lock_move_ownership(&(block->lock));
    page_zip = buf_block_get_page_zip(block);

    if (!fil_page_index_page_check(block->frame) ||
        !page_is_leaf(block->frame)) {
      corruption_noticed = true;

      ib::error(ER_IB_MSG_624) << "Corruption in the tablespace. Bitmap"
                                  " shows insert buffer records to page "
                               << page_id << " though the page type is "
                               << fil_page_get_type(block->frame)
                               << ", which is not an index leaf page. We try"
                                  " to resolve the problem by skipping the"
                                  " insert buffer merge for this page. Please"
                                  " run CHECK TABLE on your tables to determine"
                                  " if they are corrupt after this.";

      ib::error(ER_IB_MSG_SUBMIT_DETAILED_BUG_REPORT);
      ut_d(ut_error);
    }
  }

  memset(mops, 0, sizeof(mops));
  memset(dops, 0, sizeof(dops));

loop:
  ibuf_mtr_start(&mtr);

  /* Position pcur in the insert buffer at the first entry for this
  index page */
  pcur.open_on_user_rec(ibuf->index, search_tuple, PAGE_CUR_GE, BTR_MODIFY_LEAF,
                        &mtr, UT_LOCATION_HERE);

  if (block != nullptr) {
    auto success = buf_page_get_known_nowait(
        RW_X_LATCH, block, Cache_hint::KEEP_OLD, __FILE__, __LINE__, &mtr);

    ut_a(success);

    /* This is a user page (secondary index leaf page),
    but we pretend that it is a change buffer page in
    order to obey the latching order. This should be OK,
    because buffered changes are applied immediately while
    the block is io-fixed. Other threads must not try to
    latch an io-fixed block. */
    buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);
  }

  if (!pcur.is_on_user_rec()) {
    ut_ad(pcur.is_after_last_in_tree(&mtr));

    goto reset_bit;
  }

  for (;;) {
    rec_t *rec;

    ut_ad(pcur.is_on_user_rec());

    rec = pcur.get_rec();

    /* Check if the entry is for this index page */
    if (ibuf_rec_get_page_no(&mtr, rec) != page_id.page_no() ||
        ibuf_rec_get_space(&mtr, rec) != page_id.space()) {
      if (block != nullptr) {
        page_header_reset_last_insert(block->frame, page_zip, &mtr);
      }

      goto reset_bit;
    }

    if (corruption_noticed) {
      fputs("InnoDB: Discarding record\n ", stderr);
      rec_print_old(stderr, rec);
      fputs("\nInnoDB: from the insert buffer!\n\n", stderr);
    } else if (block != nullptr && !rec_get_deleted_flag(rec, 0)) {
      /* Now we have at pcur a record which should be
      applied on the index page; NOTE that the call below
      copies pointers to fields in rec, and we must
      keep the latch to the rec page until the
      insertion is finished! */
      dtuple_t *entry;
      trx_id_t max_trx_id;
      dict_index_t *dummy_index;
      ibuf_op_t op = ibuf_rec_get_op_type(&mtr, rec);

      max_trx_id = page_get_max_trx_id(page_align(rec));
      page_update_max_trx_id(block, page_zip, max_trx_id, &mtr);

      ut_ad(page_validate(page_align(rec), ibuf->index));

      entry = ibuf_build_entry_from_ibuf_rec(&mtr, rec, heap, &dummy_index);

      ut_ad(page_validate(block->frame, dummy_index));

      switch (op) {
        case IBUF_OP_INSERT:
#ifdef UNIV_IBUF_DEBUG
          volume += rec_get_converted_size(dummy_index, entry);

          volume += page_dir_calc_reserved_space(1);

          ut_a(volume <= 4 * UNIV_PAGE_SIZE / IBUF_PAGE_SIZE_PER_FREE_SPACE);
#endif
          ibuf_insert_to_index_page(entry, block, dummy_index, &mtr);
          break;

        case IBUF_OP_DELETE_MARK:
          ibuf_set_del_mark(entry, block, dummy_index, &mtr);
          break;

        case IBUF_OP_DELETE:
          ibuf_delete(entry, block, dummy_index, &mtr);
          /* Because ibuf_delete() will latch an
          insert buffer bitmap page, commit mtr
          before latching any further pages.
          Store and restore the cursor position. */
          ut_ad(rec == pcur.get_rec());
          ut_ad(page_rec_is_user_rec(rec));
          ut_ad(ibuf_rec_get_page_no(&mtr, rec) == page_id.page_no());
          ut_ad(ibuf_rec_get_space(&mtr, rec) == page_id.space());

          /* Mark the change buffer record processed,
          so that it will not be merged again in case
          the server crashes between the following
          mtr_commit() and the subsequent mtr_commit()
          of deleting the change buffer record. */

          btr_cur_set_deleted_flag_for_ibuf(pcur.get_rec(), nullptr, true,
                                            &mtr);

          pcur.store_position(&mtr);
          ibuf_btr_pcur_commit_specify_mtr(&pcur, &mtr);

          ibuf_mtr_start(&mtr);

          success =
              buf_page_get_known_nowait(RW_X_LATCH, block, Cache_hint::KEEP_OLD,
                                        __FILE__, __LINE__, &mtr);
          ut_a(success);

          /* This is a user page (secondary
          index leaf page), but it should be OK
          to use too low latching order for it,
          as the block is io-fixed. */
          buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);

          if (!ibuf_restore_pos(page_id.space(), page_id.page_no(),
                                search_tuple, BTR_MODIFY_LEAF, &pcur, &mtr)) {
            ut_ad(mtr.has_committed());
            mops[op]++;
            ibuf_dummy_index_free(dummy_index);
            goto loop;
          }

          break;
        default:
          ut_error;
      }

      mops[op]++;

      ibuf_dummy_index_free(dummy_index);
    } else {
      dops[ibuf_rec_get_op_type(&mtr, rec)]++;
    }

    /* Delete the record from ibuf */
    if (ibuf_delete_rec(page_id.space(), page_id.page_no(), &pcur, search_tuple,
                        &mtr)) {
      /* Deletion was pessimistic and mtr was committed:
      we start from the beginning again */

      ut_ad(mtr.has_committed());
      goto loop;
    } else if (pcur.is_after_last_on_page()) {
      ibuf_mtr_commit(&mtr);
      pcur.close();

      goto loop;
    }
  }

reset_bit:
  if (update_ibuf_bitmap) {
    page_t *bitmap_page;

    bitmap_page =
        ibuf_bitmap_get_map_page(page_id, *page_size, UT_LOCATION_HERE, &mtr);

    ibuf_bitmap_page_set_bits(bitmap_page, page_id, *page_size,
                              IBUF_BITMAP_BUFFERED, false, &mtr);

    if (block != nullptr) {
      ulint old_bits = ibuf_bitmap_page_get_bits(
          bitmap_page, page_id, *page_size, IBUF_BITMAP_FREE, &mtr);

      ulint new_bits = ibuf_index_page_calc_free(block);

      if (old_bits != new_bits) {
        ibuf_bitmap_page_set_bits(bitmap_page, page_id, *page_size,
                                  IBUF_BITMAP_FREE, new_bits, &mtr);
      }
    }
  }

  ibuf_mtr_commit(&mtr);
  pcur.close();
  mem_heap_free(heap);

  ibuf->n_merges.fetch_add(1);
  ibuf_add_ops(ibuf->n_merged_ops, mops);
  ibuf_add_ops(ibuf->n_discarded_ops, dops);

  if (space != nullptr) {
    fil_space_release(space);
  }

#ifdef UNIV_IBUF_COUNT_DEBUG
  ut_a(ibuf_count_get(page_id) == 0);
#endif
}

/** Deletes all entries in the insert buffer for a given space id. This is used
in DISCARD TABLESPACE and IMPORT TABLESPACE.
NOTE: this does not update the page free bitmaps in the space. The space will
become CORRUPT when you call this function! */
void ibuf_delete_for_discarded_space(space_id_t space) /*!< in: space id */
{
  mem_heap_t *heap;
  btr_pcur_t pcur;
  dtuple_t *search_tuple;
  const rec_t *ibuf_rec;
  page_no_t page_no;
  mtr_t mtr;

  /* Counts for discarded operations. */
  ulint dops[IBUF_OP_COUNT];

  heap = mem_heap_create(512, UT_LOCATION_HERE);

  /* Use page number 0 to build the search tuple so that we get the
  cursor positioned at the first entry for this space id */

  search_tuple = ibuf_search_tuple_build(space, 0, heap);

  memset(dops, 0, sizeof(dops));
loop:
  ibuf_mtr_start(&mtr);

  /* Position pcur in the insert buffer at the first entry for the
  space */
  pcur.open_on_user_rec(ibuf->index, search_tuple, PAGE_CUR_GE, BTR_MODIFY_LEAF,
                        &mtr, UT_LOCATION_HERE);

  if (!pcur.is_on_user_rec()) {
    ut_ad(pcur.is_after_last_in_tree(&mtr));

    goto leave_loop;
  }

  for (;;) {
    ut_ad(pcur.is_on_user_rec());

    ibuf_rec = pcur.get_rec();

    /* Check if the entry is for this space */
    if (ibuf_rec_get_space(&mtr, ibuf_rec) != space) {
      goto leave_loop;
    }

    page_no = ibuf_rec_get_page_no(&mtr, ibuf_rec);

    dops[ibuf_rec_get_op_type(&mtr, ibuf_rec)]++;

    /* Delete the record from ibuf */
    if (ibuf_delete_rec(space, page_no, &pcur, search_tuple, &mtr)) {
      /* Deletion was pessimistic and mtr was committed:
      we start from the beginning again */

      ut_ad(mtr.has_committed());
      goto loop;
    }

    if (pcur.is_after_last_on_page()) {
      ibuf_mtr_commit(&mtr);
      pcur.close();

      goto loop;
    }
  }

leave_loop:
  ibuf_mtr_commit(&mtr);
  pcur.close();

  ibuf_add_ops(ibuf->n_discarded_ops, dops);

  mem_heap_free(heap);
}

/** Looks if the insert buffer is empty.
 @return true if empty */
bool ibuf_is_empty(void) {
  bool is_empty;
  const page_t *root;
  mtr_t mtr;

  ibuf_mtr_start(&mtr);

  mutex_enter(&ibuf_mutex);
  root = ibuf_tree_root_get(&mtr);
  mutex_exit(&ibuf_mutex);

  is_empty = page_is_empty(root);
  ut_a(is_empty == ibuf->empty);
  ibuf_mtr_commit(&mtr);

  return (is_empty);
}

/** Prints info of ibuf. */
void ibuf_print(FILE *file) /*!< in: file where to print */
{
#ifdef UNIV_IBUF_COUNT_DEBUG
  space_id_t i;
  page_no_t j;
#endif

  mutex_enter(&ibuf_mutex);

  fprintf(file,
          "Ibuf: size %lu, free list len %lu,"
          " seg size %lu, %lu merges\n",
          (ulong)ibuf->size, (ulong)ibuf->free_list_len, (ulong)ibuf->seg_size,
          (ulong)ibuf->n_merges);

  fputs("merged operations:\n ", file);
  ibuf_print_ops(ibuf->n_merged_ops, file);

  fputs("discarded operations:\n ", file);
  ibuf_print_ops(ibuf->n_discarded_ops, file);

#ifdef UNIV_IBUF_COUNT_DEBUG
  for (i = 0; i < IBUF_COUNT_N_SPACES; i++) {
    for (j = 0; j < IBUF_COUNT_N_PAGES; j++) {
      ulint count = ibuf_count_get(page_id_t(i, j, 0));

      if (count > 0) {
        fprintf(stderr,
                "Ibuf count for space/page %lu/%lu"
                " is %lu\n",
                (ulong)i, (ulong)j, (ulong)count);
      }
    }
  }
#endif /* UNIV_IBUF_COUNT_DEBUG */

  mutex_exit(&ibuf_mutex);
}

/** Checks the insert buffer bitmaps on IMPORT TABLESPACE.
 @return DB_SUCCESS or error code */
dberr_t ibuf_check_bitmap_on_import(
    const trx_t *trx,    /*!< in: transaction */
    space_id_t space_id) /*!< in: tablespace identifier */
{
  page_no_t size;
  page_no_t page_no;

  ut_ad(space_id);
  ut_ad(trx->mysql_thd);

  bool found;

  const page_size_t &page_size = fil_space_get_page_size(space_id, &found);

  if (!found) {
    return (DB_TABLE_NOT_FOUND);
  }

  size = fil_space_get_size(space_id);

  if (size == 0) {
    return (DB_TABLE_NOT_FOUND);
  }

  mutex_enter(&ibuf_mutex);

  /* The two bitmap pages (allocation bitmap and ibuf bitmap) repeat
  every page_size pages. For example if page_size is 16 KiB, then the
  two bitmap pages repeat every 16 KiB * 16384 = 256 MiB. In the loop
  below page_no is measured in number of pages since the beginning of
  the space, as usual. */

  for (page_no = 0; page_no < size;
       page_no += static_cast<page_no_t>(page_size.physical())) {
    mtr_t mtr;
    page_t *bitmap_page;
    page_no_t i;

    if (trx_is_interrupted(trx)) {
      mutex_exit(&ibuf_mutex);
      return (DB_INTERRUPTED);
    }

    mtr_start(&mtr);

    mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

    ibuf_enter(&mtr);

    bitmap_page = ibuf_bitmap_get_map_page(page_id_t(space_id, page_no),
                                           page_size, UT_LOCATION_HERE, &mtr);

    if (buf_page_is_zeroes(bitmap_page, page_size)) {
      /* This means we got all-zero page instead of
      ibuf bitmap page. The subsequent page should be
      all-zero pages. */
#ifdef UNIV_DEBUG
      for (page_no_t curr_page = page_no + 1; curr_page < page_size.physical();
           curr_page++) {
        buf_block_t *block =
            buf_page_get(page_id_t(space_id, curr_page), page_size, RW_S_LATCH,
                         UT_LOCATION_HERE, &mtr);
        page_t *page = buf_block_get_frame(block);
        ut_ad(buf_page_is_zeroes(page, page_size));
      }
#endif /* UNIV_DEBUG */
      ibuf_exit(&mtr);
      mtr_commit(&mtr);
      continue;
    }

    for (i = FSP_IBUF_BITMAP_OFFSET + 1;
         i < static_cast<page_no_t>(page_size.physical()); i++) {
      const page_no_t offset = page_no + i;

      const page_id_t cur_page_id(space_id, offset);

      if (ibuf_bitmap_page_get_bits(bitmap_page, cur_page_id, page_size,
                                    IBUF_BITMAP_IBUF, &mtr)) {
        mutex_exit(&ibuf_mutex);
        ibuf_exit(&mtr);
        mtr_commit(&mtr);

        ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_INNODB_INDEX_CORRUPT,
                "Space %u page %u"
                " is wrongly flagged to belong to the"
                " insert buffer",
                (unsigned)space_id, (unsigned)offset);

        return (DB_CORRUPTION);
      }

      if (ibuf_bitmap_page_get_bits(bitmap_page, cur_page_id, page_size,
                                    IBUF_BITMAP_BUFFERED, &mtr)) {
        ib_errf(trx->mysql_thd, IB_LOG_LEVEL_WARN, ER_INNODB_INDEX_CORRUPT,
                "Buffered changes"
                " for space %u page %u are lost",
                (unsigned)space_id, (unsigned)offset);

        /* Tolerate this error, so that
        slightly corrupted tables can be
        imported and dumped.  Clear the bit. */
        ibuf_bitmap_page_set_bits(bitmap_page, cur_page_id, page_size,
                                  IBUF_BITMAP_BUFFERED, false, &mtr);
      }
    }

    ibuf_exit(&mtr);
    mtr_commit(&mtr);
  }

  mutex_exit(&ibuf_mutex);
  return (DB_SUCCESS);
}

/** Updates free bits and buffered bits for bulk loaded page.
@param[in]      block   index page
@param[in]      reset   flag if reset free val */
void ibuf_set_bitmap_for_bulk_load(buf_block_t *block, bool reset) {
  page_t *bitmap_page;
  mtr_t mtr;
  ulint free_val;

  ut_a(page_is_leaf(buf_block_get_frame(block)));

  free_val = ibuf_index_page_calc_free(block);

  mtr_start(&mtr);

  bitmap_page = ibuf_bitmap_get_map_page(block->page.id, block->page.size,
                                         UT_LOCATION_HERE, &mtr);

  free_val = reset ? 0 : ibuf_index_page_calc_free(block);
  ibuf_bitmap_page_set_bits(bitmap_page, block->page.id, block->page.size,
                            IBUF_BITMAP_FREE, free_val, &mtr);

  ibuf_bitmap_page_set_bits(bitmap_page, block->page.id, block->page.size,
                            IBUF_BITMAP_BUFFERED, false, &mtr);

  mtr_commit(&mtr);
}

#endif /* !UNIV_HOTBACKUP */
