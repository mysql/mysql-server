/*****************************************************************************

Copyright (c) 1994, 2024, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

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

/** @file include/btr0btr.h
 The B-tree

 Created 6/2/1994 Heikki Tuuri
 *******************************************************/

#ifndef btr0btr_h
#define btr0btr_h

#include "btr0types.h"
#include "data0data.h"
#include "dict0dict.h"
#include "gis0type.h"
#include "mtr0mtr.h"
#include "page0cur.h"
#include "univ.i"

/** Maximum record size which can be stored on a page, without using the
special big record storage structure */
#define BTR_PAGE_MAX_REC_SIZE (UNIV_PAGE_SIZE / 2 - 200)

/** @brief Maximum depth of a B-tree in InnoDB.

Note that this isn't a maximum as such; none of the tree operations
avoid producing trees bigger than this. It is instead a "max depth
that other code must work with", useful for e.g.  fixed-size arrays
that must store some information about each level in a tree. In other
words: if a B-tree with bigger depth than this is encountered, it is
not acceptable for it to lead to mysterious memory corruption, but it
is acceptable for the program to die with a clear assert failure. */
constexpr uint32_t BTR_MAX_LEVELS = 100;

/** Latching modes for btr_cur_search_to_nth_level(). */
enum btr_latch_mode : size_t {
  /** Search a record on a leaf page and S-latch it. */
  BTR_SEARCH_LEAF = RW_S_LATCH,
  /** (Prepare to) modify a record on a leaf page and X-latch it. */
  BTR_MODIFY_LEAF = RW_X_LATCH,
  /** Obtain no latches. */
  BTR_NO_LATCHES = RW_NO_LATCH,
  /** Start modifying the entire B-tree. */
  BTR_MODIFY_TREE = 33,
  /** Continue modifying the entire B-tree. */
  BTR_CONT_MODIFY_TREE = 34,
  /** Search the previous record. */
  BTR_SEARCH_PREV = 35,
  /** Modify the previous record. */
  BTR_MODIFY_PREV = 36,
  /** Start searching the entire B-tree. */
  BTR_SEARCH_TREE = 37,
  /** Continue searching the entire B-tree. */
  BTR_CONT_SEARCH_TREE = 38
};

/* BTR_INSERT, BTR_DELETE and BTR_DELETE_MARK are mutually exclusive. */

/** If this is ORed to btr_latch_mode, it means that the search tuple
will be inserted to the index, at the searched position.
When the record is not in the buffer pool, try to use the insert buffer. */
constexpr size_t BTR_INSERT = 512;

/** This flag ORed to btr_latch_mode says that we do the search in query
optimization */
constexpr size_t BTR_ESTIMATE = 1024;

/** This flag ORed to BTR_INSERT says that we can ignore possible
UNIQUE definition on secondary indexes when we decide if we can use
the insert buffer to speed up inserts */
constexpr size_t BTR_IGNORE_SEC_UNIQUE = 2048;

/** Try to delete mark the record at the searched position using the
insert/delete buffer when the record is not in the buffer pool. */
constexpr size_t BTR_DELETE_MARK = 4096;

/** Try to purge the record at the searched position using the insert/delete
buffer when the record is not in the buffer pool. */
constexpr size_t BTR_DELETE = 8192;

/** In the case of BTR_SEARCH_LEAF or BTR_MODIFY_LEAF, the caller is
already holding an S latch on the index tree */
constexpr size_t BTR_ALREADY_S_LATCHED = 16384;

/** In the case of BTR_MODIFY_TREE, the caller specifies the intention
to insert record only. It is used to optimize block->lock range.*/
constexpr size_t BTR_LATCH_FOR_INSERT = 32768;

/** In the case of BTR_MODIFY_TREE, the caller specifies the intention
to delete record only. It is used to optimize block->lock range.*/
constexpr size_t BTR_LATCH_FOR_DELETE = 65536;

/** This flag is for undo insert of rtree. For rtree, we need this flag
to find proper rec to undo insert.*/
constexpr size_t BTR_RTREE_UNDO_INS = 131072;

/** In the case of BTR_MODIFY_LEAF, the caller intends to allocate or
free the pages of externally stored fields. */
constexpr size_t BTR_MODIFY_EXTERNAL = 262144;

/** Try to delete mark the record at the searched position when the
record is in spatial index */
constexpr size_t BTR_RTREE_DELETE_MARK = 524288;

using Page_range_t = std::pair<page_no_t, page_no_t>;

constexpr ulint BTR_LATCH_MODE_WITHOUT_FLAGS(ulint latch_mode) {
  return latch_mode &
         ~(BTR_INSERT | BTR_DELETE_MARK | BTR_RTREE_UNDO_INS |
           BTR_RTREE_DELETE_MARK | BTR_DELETE | BTR_ESTIMATE |
           BTR_IGNORE_SEC_UNIQUE | BTR_ALREADY_S_LATCHED |
           BTR_LATCH_FOR_INSERT | BTR_LATCH_FOR_DELETE | BTR_MODIFY_EXTERNAL);
}

inline ulint BTR_LATCH_MODE_WITHOUT_INTENTION(ulint latch_mode) {
  return latch_mode &
         ~(BTR_LATCH_FOR_INSERT | BTR_LATCH_FOR_DELETE | BTR_MODIFY_EXTERNAL);
}

/** Report that an index page is corrupted. */
void btr_corruption_report(const buf_block_t *block, /*!< in: corrupted block */
                           const dict_index_t *index) /*!< in: index tree */
    UNIV_COLD;

/** Assert that a B-tree page is not corrupted.
@param block buffer block containing a B-tree page
@param index the B-tree index */
inline void btr_assert_not_corrupted(const buf_block_t *block,
                                     const dict_index_t *index) {
  if (page_is_comp(buf_block_get_frame(block)) !=
      dict_table_is_comp((index)->table)) {
    btr_corruption_report(block, index);
    ut_error;
  }
}

/** Gets the root node of a tree and sx-latches it for segment access.
 @return root page, sx-latched */
page_t *btr_root_get(const dict_index_t *index, /*!< in: index tree */
                     mtr_t *mtr);               /*!< in: mtr */

/** Checks and adjusts the root node of a tree during IMPORT TABLESPACE.
 @return error code, or DB_SUCCESS */
[[nodiscard]] dberr_t btr_root_adjust_on_import(
    const dict_index_t *index); /*!< in: index tree */

/** Gets the height of the B-tree (the level of the root, when the leaf
 level is assumed to be 0). The caller must hold an S or X latch on
 the index.
 @return tree height (level of the root) */
[[nodiscard]] ulint btr_height_get(dict_index_t *index, /*!< in: index tree */
                                   mtr_t *mtr); /*!< in/out: mini-transaction */

#ifndef UNIV_HOTBACKUP
/** Gets a buffer page and declares its latching order level.
@param[in]      page_id         Page id
@param[in]      page_size       Page size
@param[in]      mode              Latch mode
@param[in]      location  Location from where this method is called.
@param[in]      index           Index tree, may be NULL if it is not an insert
                                buffer tree
@param[in,out]  mtr             Mini-transaction
@return block */
static inline buf_block_t *btr_block_get_func(
    const page_id_t &page_id, const page_size_t &page_size, ulint mode,
    ut::Location location, IF_DEBUG(const dict_index_t *index, ) mtr_t *mtr);

/** Gets a buffer page and declares its latching order level.
@param page_id Tablespace/page identifier
@param page_size Page size
@param mode Latch mode
@param[in]      location  Location from where this method is called.
@param index Index tree, may be NULL if not the insert buffer tree
@param mtr Mini-transaction handle
@return the block descriptor */
static inline buf_block_t *btr_block_get(const page_id_t &page_id,
                                         const page_size_t &page_size,
                                         ulint mode, ut::Location location,
                                         const dict_index_t *index,
                                         mtr_t *mtr) {
  return btr_block_get_func(page_id, page_size, mode, location,
                            IF_DEBUG(index, ) mtr);
}

#endif /* !UNIV_HOTBACKUP */

/** Gets the index id field of a page.
 @return index id */
[[nodiscard]] static inline space_index_t btr_page_get_index_id(
    const page_t *page); /*!< in: index page */
/** Gets the node level field in an index page.
 @param[in] page index page
 @return level, leaf level == 0 */
[[nodiscard]] static inline ulint btr_page_get_level(const page_t *page);
/** Gets the next index page number.
@param[in] page Index page.
@param[in] mtr  Mini-transaction handle.
@return next page number */
[[nodiscard]] static inline page_no_t btr_page_get_next(const page_t *page,
                                                        mtr_t *mtr);
/** Gets the previous index page number.
@param[in] page Index page.
@param[in] mtr  Mini-transaction handle.
@return prev page number */
[[nodiscard]] static inline page_no_t btr_page_get_prev(const page_t *page,
                                                        mtr_t *mtr);

#ifndef UNIV_HOTBACKUP
/** Releases the latch on a leaf page and bufferunfixes it.
@param[in]      block           buffer block
@param[in]      latch_mode      BTR_SEARCH_LEAF or BTR_MODIFY_LEAF
@param[in]      mtr             mtr */
static inline void btr_leaf_page_release(buf_block_t *block, ulint latch_mode,
                                         mtr_t *mtr);
#endif /* !UNIV_HOTBACKUP */

/** Gets the child node file address in a node pointer.
 NOTE: the offsets array must contain all offsets for the record since
 we read the last field according to offsets and assume that it contains
 the child page number. In other words offsets must have been retrieved
 with rec_get_offsets(n_fields=ULINT_UNDEFINED).
 @param[in] rec node Pointer record
 @param[in] offsets  Array returned by rec_get_offsets()
 @return child node address */
[[nodiscard]] static inline page_no_t btr_node_ptr_get_child_page_no(
    const rec_t *rec, const ulint *offsets);

/** Returns the child page of a node pointer and sx-latches it.
@param[in]  node_ptr  node pointer
@param[in]  index index
@param[in]  offsets array returned by rec_get_offsets()
@param[in]  mtr mtr
@param[in]  type latch type
@return child page, latched as per the type */
buf_block_t *btr_node_ptr_get_child(const rec_t *node_ptr, dict_index_t *index,
                                    const ulint *offsets, mtr_t *mtr,
                                    rw_lock_type_t type = RW_SX_LATCH);

/** Create the root node for a new index tree.
@param[in]      type                    Type of the index
@param[in]      space                   Space where created
@param[in]      index_id                Index id
@param[in]      index                   Index tree
@param[in,out]  mtr                     Mini-transaction
@return page number of the created root
@retval FIL_NULL if did not succeed */
ulint btr_create(ulint type, space_id_t space, space_index_t index_id,
                 dict_index_t *index, mtr_t *mtr);

/** Free a persistent index tree if it exists.
@param[in]      page_id         Root page id
@param[in]      page_size       Page size
@param[in]      index_id        PAGE_INDEX_ID contents
@param[in,out]  mtr             Mini-transaction */
void btr_free_if_exists(const page_id_t &page_id, const page_size_t &page_size,
                        space_index_t index_id, mtr_t *mtr);

/** Free an index tree in a temporary tablespace.
@param[in]      page_id         root page id
@param[in]      page_size       page size */
void btr_free(const page_id_t &page_id, const page_size_t &page_size);

/** Truncate an index tree. We just free all except the root.
Currently, this function is only specific for clustered indexes and the only
caller is DDTableBuffer which manages a table with only a clustered index.
It is up to the caller to ensure atomicity and to ensure correct recovery by
calling btr_truncate_recover().
@param[in]      index           clustered index */
void btr_truncate(const dict_index_t *index);

/** Recovery function for btr_truncate. We will check if there is a
crash during btr_truncate, if so, do recover it, if not, do nothing.
@param[in]      index           clustered index */
void btr_truncate_recover(const dict_index_t *index);

/** Makes tree one level higher by splitting the root, and inserts
 the tuple. It is assumed that mtr contains an x-latch on the tree.
 NOTE that the operation of this function must always succeed,
 we cannot reverse it: therefore enough free disk space must be
 guaranteed to be available before this function is called.
 @return inserted record */
[[nodiscard]] rec_t *btr_root_raise_and_insert(
    uint32_t flags,        /*!< in: undo logging and locking flags */
    btr_cur_t *cursor,     /*!< in: cursor at which to insert: must be
                           on the root page; when the function returns,
                           the cursor is positioned on the predecessor
                           of the inserted record */
    ulint **offsets,       /*!< out: offsets on inserted record */
    mem_heap_t **heap,     /*!< in/out: pointer to memory heap
                           that can be emptied, or NULL */
    const dtuple_t *tuple, /*!< in: tuple to insert */
    mtr_t *mtr);           /*!< in: mtr */
/** Reorganizes an index page.

IMPORTANT: On success, the caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index. This has to
be done either within the same mini-transaction, or by invoking
ibuf_reset_free_bits() before mtr_commit(). On uncompressed pages,
IBUF_BITMAP_FREE is unaffected by reorganization.

@param[in]     recovery True if called in recovery: locks should not be updated,
i.e., there cannot exist locks on the page, and a hash index should not be
dropped: it cannot exist.
@param[in]     z_level  Compression level to be used if dealing with compressed
page.
@param[in,out] cursor   Page cursor.
@param[in]     index    The index tree of the page.
@param[in,out] mtr      Mini-transaction
@retval true if the operation was successful
@retval false if it is a compressed page, and re-compression failed */
[[nodiscard]] bool btr_page_reorganize_low(bool recovery, ulint z_level,
                                           page_cur_t *cursor,
                                           dict_index_t *index, mtr_t *mtr);

/** Reorganizes an index page.

IMPORTANT: On success, the caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index. This has to
be done either within the same mini-transaction, or by invoking
ibuf_reset_free_bits() before mtr_commit(). On uncompressed pages,
IBUF_BITMAP_FREE is unaffected by reorganization.

@param[in,out] cursor Page cursor
@param[in] index The index tree of the page
@param[in,out] mtr Mini-transaction
@retval true if the operation was successful
@retval false if it is a compressed page, and recompression failed */
bool btr_page_reorganize(page_cur_t *cursor, dict_index_t *index, mtr_t *mtr);

/** Decides if the page should be split at the convergence point of
 inserts converging to left.
 @return true if split recommended */
[[nodiscard]] bool btr_page_get_split_rec_to_left(
    btr_cur_t *cursor,  /*!< in: cursor at which to insert */
    rec_t **split_rec); /*!< out: if split recommended,
                     the first record on upper half page,
                     or NULL if tuple should be first */
/** Decides if the page should be split at the convergence point of
 inserts converging to right.
 @return true if split recommended */
[[nodiscard]] bool btr_page_get_split_rec_to_right(
    btr_cur_t *cursor,  /*!< in: cursor at which to insert */
    rec_t **split_rec); /*!< out: if split recommended,
                     the first record on upper half page,
                     or NULL if tuple should be first */

/** Splits an index page to halves and inserts the tuple. It is assumed
 that mtr holds an x-latch to the index tree. NOTE: the tree x-latch is
 released within this function! NOTE that the operation of this
 function must always succeed, we cannot reverse it: therefore enough
 free disk space (2 pages) must be guaranteed to be available before
 this function is called.

 @return inserted record */
[[nodiscard]] rec_t *btr_page_split_and_insert(
    uint32_t flags,        /*!< in: undo logging and locking flags */
    btr_cur_t *cursor,     /*!< in: cursor at which to insert; when the
                           function returns, the cursor is positioned
                           on the predecessor of the inserted record */
    ulint **offsets,       /*!< out: offsets on inserted record */
    mem_heap_t **heap,     /*!< in/out: pointer to memory heap
                           that can be emptied, or NULL */
    const dtuple_t *tuple, /*!< in: tuple to insert */
    mtr_t *mtr);           /*!< in: mtr */
/** Inserts a data tuple to a tree on a non-leaf level. It is assumed
 that mtr holds an x-latch on the tree.
 @param[in] flags undo logging and locking flags
 @param[in] index index
 @param[in] level level, must be > 0
 @param[in] tuple the record to be inserted
 @param[in] location location where called
 @param[in] mtr mtr */
void btr_insert_on_non_leaf_level(uint32_t flags, dict_index_t *index,
                                  ulint level, dtuple_t *tuple,
                                  ut::Location location, mtr_t *mtr);

/** Sets a record as the predefined minimum record.
@param[in,out] rec Record
@param[in] mtr Mini-transaction
*/
void btr_set_min_rec_mark(rec_t *rec, mtr_t *mtr);

/** Removes a record as the predefined minimum record.
@param[in]  block  buffer block containing the record.
@param[in]  rec    the record who info bits will be modified by clearing
                   the REC_INFO_MIN_REC_FLAG bit.
@param[in]  mtr    mini transaction context. */
void btr_unset_min_rec_mark(buf_block_t *block, rec_t *rec, mtr_t *mtr);

/** Deletes on the upper level the node pointer to a page.
@param[in] index Index tree
@param[in] block Page whose node pointer is deleted
@param[in] mtr Mini-transaction
*/
void btr_node_ptr_delete(dict_index_t *index, buf_block_t *block, mtr_t *mtr);
#ifdef UNIV_DEBUG
/** Asserts that the node pointer to a page is appropriate.
 @param[in] index index tree
 @param[in] block index page
 @param[in] mtr mtr
 @return true */
bool btr_check_node_ptr(dict_index_t *index, buf_block_t *block, mtr_t *mtr);
#endif /* UNIV_DEBUG */
/** Tries to merge the page first to the left immediate brother if such a
 brother exists, and the node pointers to the current page and to the brother
 reside on the same page. If the left brother does not satisfy these
 conditions, looks at the right brother. If the page is the only one on that
 level lifts the records of the page to the father page, thus reducing the
 tree height. It is assumed that mtr holds an x-latch on the tree and on the
 page. If cursor is on the leaf level, mtr must also hold x-latches to the
 brothers, if they exist.
 @param[in,out] cursor cursor on the page to merge or lift; the page must not be
 empty: when deleting records, use btr_discard_page() if the page would become
 empty
 @param[in] adjust true if should adjust the cursor position even if compression
 occurs.
 @param[in,out] mtr mini-transaction
 @return true on success */
bool btr_compress(btr_cur_t *cursor, bool adjust, mtr_t *mtr);
/** Discards a page from a B-tree. This is used to remove the last record from
 a B-tree page: the whole page must be removed at the same time. This cannot
 be used for the root page, which is allowed to be empty. */
void btr_discard_page(btr_cur_t *cursor, /*!< in: cursor on the page to
                                         discard: not on the root page */
                      mtr_t *mtr);       /*!< in: mtr */
/** Parses the redo log record for setting an index record as the predefined
 minimum record.
 @return end of log record or NULL */
[[nodiscard]] const byte *btr_parse_set_min_rec_mark(
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    ulint comp,          /*!< in: nonzero=compact page format */
    page_t *page,        /*!< in: page or NULL */
    mtr_t *mtr);         /*!< in: mtr or NULL */
/** Parses a redo log record of reorganizing a page.
 @return end of log record or NULL */
[[nodiscard]] const byte *btr_parse_page_reorganize(
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    dict_index_t *index, /*!< in: record descriptor */
    bool compressed,     /*!< in: true if compressed page */
    buf_block_t *block,  /*!< in: page to be reorganized, or NULL */
    mtr_t *mtr);         /*!< in: mtr or NULL */
/** Gets the number of pages in a B-tree.
 @return number of pages, or ULINT_UNDEFINED if the index is unavailable */
[[nodiscard]] ulint btr_get_size(
    dict_index_t *index, /*!< in: index */
    ulint flag,          /*!< in: BTR_N_LEAF_PAGES or BTR_TOTAL_SIZE */
    mtr_t *mtr);         /*!< in/out: mini-transaction where index
                        is s-latched */

#ifdef UNIV_DEBUG
#define btr_page_alloc(index, hint_page_no, file_direction, level, mtr, \
                       init_mtr)                                        \
  btr_page_alloc_priv(index, hint_page_no, file_direction, level, mtr,  \
                      init_mtr, UT_LOCATION_HERE)
#else /* UNIV_DEBUG */
#define btr_page_alloc(index, hint_page_no, file_direction, level, mtr, \
                       init_mtr)                                        \
  btr_page_alloc_priv(index, hint_page_no, file_direction, level, mtr, init_mtr)
#endif /* UNIV_DEBUG */

/** Allocates a new file page to be used in an index tree. NOTE: we assume
that the caller has made the reservation for free extents!
@param[in] index Index tree
@param[in] hint_page_no Hint of a good page
@param[in] file_direction Direction where a possible page split is made
@param[in] level Level where the page is placed in the tree
@param[in,out] mtr Mini-transaction for the allocation
@param[in,out] init_mtr Mini-transaction for x-latching and initializing the
page
@param[in]   loc   debug only parameter providing caller source location.
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr),
returned block is not allocated nor initialized otherwise */
[[nodiscard]] buf_block_t *btr_page_alloc_priv(
    dict_index_t *index, page_no_t hint_page_no, byte file_direction,
    ulint level, mtr_t *mtr, mtr_t *init_mtr IF_DEBUG(, const ut::Location &loc)

);

/** Allocates all pages of one extent to be used in an index tree.
@param[in]  index  the index for which pages are allocated.
@param[in]  is_leaf  true if leaf segment and false if non-leaf segment
@param[out]  page_range  All pages within this pair of page numbers are
allocated for this B-tree. The page_range.first is part of the range, while the
page_range.second is not part of the range.
@param[in]  mtr  mini transaction context for this operation.
@return DB_SUCCESS on success, error code on failure. */
[[nodiscard]] dberr_t btr_extent_alloc(const dict_index_t *const index,
                                       bool is_leaf, Page_range_t &page_range,
                                       mtr_t *mtr);

/** Frees a file page used in an index tree. NOTE: cannot free field external
 storage pages because the page must contain info on its level. */
void btr_page_free(dict_index_t *index, /*!< in: index tree */
                   buf_block_t *block,  /*!< in: block to be freed, x-latched */
                   mtr_t *mtr);         /*!< in: mtr */
/** Creates a new index page (not the root, and also not
 used in page reorganization).  @see btr_page_empty(). */
void btr_page_create(
    buf_block_t *block,       /*!< in/out: page to be created */
    page_zip_des_t *page_zip, /*!< in/out: compressed page, or NULL */
    dict_index_t *index,      /*!< in: index */
    ulint level,              /*!< in: the B-tree level of the page */
    mtr_t *mtr);              /*!< in: mtr */

/** Frees a file page used in an index tree. Can be used also to BLOB
 external storage pages.
@param[in]   index   the index to which the page belongs
@param[in]   block   block to be freed, x-latched
@param[in]   level   page level (ULINT_UNDEFINED=BLOB)
@param[in]   mtr     mini transaction context. */
void btr_page_free_low(dict_index_t *index, buf_block_t *block, ulint level,
                       mtr_t *mtr);

/** Gets the root node of a tree and x- or s-latches it.
 @return root page, x- or s-latched */
buf_block_t *btr_root_block_get(
    const dict_index_t *index, /*!< in: index tree */
    ulint mode,                /*!< in: either RW_S_LATCH
                               or RW_X_LATCH */
    mtr_t *mtr);               /*!< in: mtr */

/** Prints size info of a B-tree. */
void btr_print_size(dict_index_t *index); /*!< in: index tree */

/** Prints directories and other info of all nodes in the index.
@param[in] index  the index to be printed.
@param[in] width  number of entries to print from start and end. */
void btr_print_index(dict_index_t *index, ulint width);

/** Checks the size and number of fields in a record based on the definition of
the index.
 @return true if ok */
[[nodiscard]] bool btr_index_rec_validate(
    const rec_t *rec,          /*!< in: index record */
    const dict_index_t *index, /*!< in: index */
    bool dump_on_error);       /*!< in: true if the function
                               should print hex dump of
                               record and page on error */
/** Checks the consistency of an index tree.
 @return true if ok */
[[nodiscard]] bool btr_validate_index(
    dict_index_t *index, /*!< in: index */
    const trx_t *trx,    /*!< in: transaction or 0 */
    bool lockout);       /*!< in: true if X-latch index is intended */

/** Creates SDI index and stores the root page numbers in page 1 & 2
@param[in]      space_id        tablespace id
@param[in]      dict_locked     true if dict_sys mutex is acquired
@return DB_SUCCESS on success, else DB_ERROR on failure */
dberr_t btr_sdi_create_index(space_id_t space_id, bool dict_locked);

constexpr uint32_t BTR_N_LEAF_PAGES = 1;
constexpr uint32_t BTR_TOTAL_SIZE = 2;

/** Check if the given index is empty.  An index is considered empty if it
has only the root page with no user records, including del-marked records.
@param[in]   index   index
@return true if index is empty, false otherwise. */
bool btr_is_index_empty(const dict_index_t *index);

#ifdef UNIV_DEBUG
/** Does a breadth first traversal (BFT) of the B-tree, and invokes the
callback for each of the B-tree nodes. */
struct BFT {
  struct Callback {
    struct Page_details {
      page_no_t m_page_no;
      size_t m_nrows;
      size_t m_level;
      std::ostream &print(std::ostream &out) const;
    };
    void init(size_t max_level) { m_data.resize(max_level); }
    void operator()(buf_block_t *block);
    std::ostream &print(std::ostream &out) const;

   public:
    BFT *m_bft;

   private:
    std::vector<std::list<Page_details>> m_data;
  };
  BFT(const dict_index_t *index, Callback &cb);
  void traverse();

  const dict_index_t *index() const { return m_index; }

 private:
  void children_to_visit(buf_block_t *block);
  page_no_t visit_next();
  std::list<page_no_t> m_pages_to_visit;
  const dict_index_t *m_index;
  Callback &m_callback;
};

inline std::ostream &operator<<(std::ostream &out,
                                const BFT::Callback::Page_details &obj) {
  return obj.print(out);
}

inline std::ostream &operator<<(std::ostream &out, const BFT::Callback &obj) {
  return obj.print(out);
}

#endif /* UNIV_DEBUG */

/** NOTE - Changing this from the original number of 50 to 45 as
insert_debug.test was failing in ASAN build because of a stack overflow issue.
It was found that rtr_info_t was taking up a lot of stack space in the function
btr_insert_on_non_leaf_level_func which is part of the recursive stack
trace. */
/** Maximum B-tree page level (not really a hard limit). Used in debug
 assertions in btr_page_set_level and btr_page_get_level */
constexpr uint32_t BTR_MAX_NODE_LEVEL = 45;
#include "btr0btr.ic"

#endif
