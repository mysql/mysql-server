/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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
#define BTR_MAX_LEVELS 100

/** Latching modes for btr_cur_search_to_nth_level(). */
enum btr_latch_mode {
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
#define BTR_INSERT 512

/** This flag ORed to btr_latch_mode says that we do the search in query
optimization */
#define BTR_ESTIMATE 1024

/** This flag ORed to BTR_INSERT says that we can ignore possible
UNIQUE definition on secondary indexes when we decide if we can use
the insert buffer to speed up inserts */
#define BTR_IGNORE_SEC_UNIQUE 2048

/** Try to delete mark the record at the searched position using the
insert/delete buffer when the record is not in the buffer pool. */
#define BTR_DELETE_MARK 4096

/** Try to purge the record at the searched position using the insert/delete
buffer when the record is not in the buffer pool. */
#define BTR_DELETE 8192

/** In the case of BTR_SEARCH_LEAF or BTR_MODIFY_LEAF, the caller is
already holding an S latch on the index tree */
#define BTR_ALREADY_S_LATCHED 16384

/** In the case of BTR_MODIFY_TREE, the caller specifies the intention
to insert record only. It is used to optimize block->lock range.*/
#define BTR_LATCH_FOR_INSERT 32768

/** In the case of BTR_MODIFY_TREE, the caller specifies the intention
to delete record only. It is used to optimize block->lock range.*/
#define BTR_LATCH_FOR_DELETE 65536

/** This flag is for undo insert of rtree. For rtree, we need this flag
to find proper rec to undo insert.*/
#define BTR_RTREE_UNDO_INS 131072

/** In the case of BTR_MODIFY_LEAF, the caller intends to allocate or
free the pages of externally stored fields. */
#define BTR_MODIFY_EXTERNAL 262144

/** Try to delete mark the record at the searched position when the
record is in spatial index */
#define BTR_RTREE_DELETE_MARK 524288

#define BTR_LATCH_MODE_WITHOUT_FLAGS(latch_mode)                            \
  ((latch_mode) &                                                           \
   ~(BTR_INSERT | BTR_DELETE_MARK | BTR_RTREE_UNDO_INS |                    \
     BTR_RTREE_DELETE_MARK | BTR_DELETE | BTR_ESTIMATE |                    \
     BTR_IGNORE_SEC_UNIQUE | BTR_ALREADY_S_LATCHED | BTR_LATCH_FOR_INSERT | \
     BTR_LATCH_FOR_DELETE | BTR_MODIFY_EXTERNAL))

#define BTR_LATCH_MODE_WITHOUT_INTENTION(latch_mode) \
  ((latch_mode) &                                    \
   ~(BTR_LATCH_FOR_INSERT | BTR_LATCH_FOR_DELETE | BTR_MODIFY_EXTERNAL))

/** Report that an index page is corrupted. */
void btr_corruption_report(const buf_block_t *block, /*!< in: corrupted block */
                           const dict_index_t *index) /*!< in: index tree */
    UNIV_COLD;

/** Assert that a B-tree page is not corrupted.
@param block buffer block containing a B-tree page
@param index the B-tree index */
#define btr_assert_not_corrupted(block, index)              \
  if ((ibool) !!page_is_comp(buf_block_get_frame(block)) != \
      dict_table_is_comp((index)->table)) {                 \
    btr_corruption_report(block, index);                    \
    ut_error;                                               \
  }

/** Gets the root node of a tree and sx-latches it for segment access.
 @return root page, sx-latched */
page_t *btr_root_get(const dict_index_t *index, /*!< in: index tree */
                     mtr_t *mtr);               /*!< in: mtr */

/** Checks and adjusts the root node of a tree during IMPORT TABLESPACE.
 @return error code, or DB_SUCCESS */
dberr_t btr_root_adjust_on_import(
    const dict_index_t *index) /*!< in: index tree */
    MY_ATTRIBUTE((warn_unused_result));

/** Gets the height of the B-tree (the level of the root, when the leaf
 level is assumed to be 0). The caller must hold an S or X latch on
 the index.
 @return tree height (level of the root) */
ulint btr_height_get(dict_index_t *index, /*!< in: index tree */
                     mtr_t *mtr)          /*!< in/out: mini-transaction */
    MY_ATTRIBUTE((warn_unused_result));

/** Gets a buffer page and declares its latching order level.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	mode		latch mode
@param[in]	file		file name
@param[in]	line		line where called
@param[in]	index		index tree, may be NULL if it is not an insert
                                buffer tree
@param[in,out]	mtr		mini-transaction
@return block */
UNIV_INLINE
buf_block_t *btr_block_get_func(const page_id_t &page_id,
                                const page_size_t &page_size, ulint mode,
                                const char *file, ulint line,
#ifdef UNIV_DEBUG
                                const dict_index_t *index,
#endif /* UNIV_DEBUG */
                                mtr_t *mtr);

#ifdef UNIV_DEBUG
/** Gets a buffer page and declares its latching order level.
@param page_id tablespace/page identifier
@param page_size page size
@param mode latch mode
@param index index tree, may be NULL if not the insert buffer tree
@param mtr mini-transaction handle
@return the block descriptor */
#define btr_block_get(page_id, page_size, mode, index, mtr) \
  btr_block_get_func(page_id, page_size, mode, __FILE__, __LINE__, index, mtr)
#else /* UNIV_DEBUG */
/** Gets a buffer page and declares its latching order level.
@param page_id tablespace/page identifier
@param page_size page size
@param mode latch mode
@param index index tree, may be NULL if not the insert buffer tree
@param mtr mini-transaction handle
@return the block descriptor */
#define btr_block_get(page_id, page_size, mode, index, mtr) \
  btr_block_get_func(page_id, page_size, mode, __FILE__, __LINE__, mtr)
#endif /* UNIV_DEBUG */
/** Gets a buffer page and declares its latching order level.
@param page_id tablespace/page identifier
@param page_size page size
@param mode latch mode
@param index index tree, may be NULL if not the insert buffer tree
@param mtr mini-transaction handle
@return the uncompressed page frame */
#define btr_page_get(page_id, page_size, mode, index, mtr) \
  buf_block_get_frame(btr_block_get(page_id, page_size, mode, index, mtr))
/** Gets the index id field of a page.
 @return index id */
UNIV_INLINE
space_index_t btr_page_get_index_id(const page_t *page) /*!< in: index page */
    MY_ATTRIBUTE((warn_unused_result));
/** Gets the node level field in an index page.
 @return level, leaf level == 0 */
UNIV_INLINE
ulint btr_page_get_level_low(const page_t *page) /*!< in: index page */
    MY_ATTRIBUTE((warn_unused_result));
#define btr_page_get_level(page, mtr) btr_page_get_level_low(page)
/** Gets the next index page number.
 @return next page number */
UNIV_INLINE
page_no_t btr_page_get_next(const page_t *page, /*!< in: index page */
                            mtr_t *mtr) /*!< in: mini-transaction handle */
    MY_ATTRIBUTE((warn_unused_result));
/** Gets the previous index page number.
 @return prev page number */
UNIV_INLINE
page_no_t btr_page_get_prev(const page_t *page, /*!< in: index page */
                            mtr_t *mtr) /*!< in: mini-transaction handle */
    MY_ATTRIBUTE((warn_unused_result));

/** Releases the latch on a leaf page and bufferunfixes it.
@param[in]	block		buffer block
@param[in]	latch_mode	BTR_SEARCH_LEAF or BTR_MODIFY_LEAF
@param[in]	mtr		mtr */
UNIV_INLINE
void btr_leaf_page_release(buf_block_t *block, ulint latch_mode, mtr_t *mtr);

/** Gets the child node file address in a node pointer.
 NOTE: the offsets array must contain all offsets for the record since
 we read the last field according to offsets and assume that it contains
 the child page number. In other words offsets must have been retrieved
 with rec_get_offsets(n_fields=ULINT_UNDEFINED).
 @return child node address */
UNIV_INLINE
page_no_t btr_node_ptr_get_child_page_no(
    const rec_t *rec,     /*!< in: node pointer record */
    const ulint *offsets) /*!< in: array returned by rec_get_offsets() */
    MY_ATTRIBUTE((warn_unused_result));
/** Create the root node for a new index tree.
@param[in]	type			type of the index
@param[in]	space			space where created
@param[in]	page_size		page size
@param[in]	index_id		index id
@param[in]	index			index tree
@param[in,out]	mtr			mini-transaction
@return page number of the created root
@retval FIL_NULL if did not succeed */
ulint btr_create(ulint type, space_id_t space, const page_size_t &page_size,
                 space_index_t index_id, dict_index_t *index, mtr_t *mtr);

/** Free a persistent index tree if it exists.
@param[in]	page_id		root page id
@param[in]	page_size	page size
@param[in]	index_id	PAGE_INDEX_ID contents
@param[in,out]	mtr		mini-transaction */
void btr_free_if_exists(const page_id_t &page_id, const page_size_t &page_size,
                        space_index_t index_id, mtr_t *mtr);

/** Free an index tree in a temporary tablespace.
@param[in]	page_id		root page id
@param[in]	page_size	page size */
void btr_free(const page_id_t &page_id, const page_size_t &page_size);

/** Truncate an index tree. We just free all except the root.
Currently, this function is only specific for clustered indexes and the only
caller is DDTableBuffer which manages a table with only a clustered index.
It is up to the caller to ensure atomicity and to implement recovery by
calling btr_truncate_recover().
@param[in]	index		clustered index */
void btr_truncate(const dict_index_t *index);

/** Recovery function for btr_truncate. We will check if there is a
crash during btr_truncate, if so, do recover it, if not, do nothing.
@param[in]	index		clustered index */
void btr_truncate_recover(const dict_index_t *index);

/** Makes tree one level higher by splitting the root, and inserts
 the tuple. It is assumed that mtr contains an x-latch on the tree.
 NOTE that the operation of this function must always succeed,
 we cannot reverse it: therefore enough free disk space must be
 guaranteed to be available before this function is called.
 @return inserted record */
rec_t *btr_root_raise_and_insert(
    ulint flags,           /*!< in: undo logging and locking flags */
    btr_cur_t *cursor,     /*!< in: cursor at which to insert: must be
                           on the root page; when the function returns,
                           the cursor is positioned on the predecessor
                           of the inserted record */
    ulint **offsets,       /*!< out: offsets on inserted record */
    mem_heap_t **heap,     /*!< in/out: pointer to memory heap
                           that can be emptied, or NULL */
    const dtuple_t *tuple, /*!< in: tuple to insert */
    ulint n_ext,           /*!< in: number of externally stored columns */
    mtr_t *mtr)            /*!< in: mtr */
    MY_ATTRIBUTE((warn_unused_result));
/** Reorganizes an index page.

 IMPORTANT: On success, the caller will have to update IBUF_BITMAP_FREE
 if this is a compressed leaf page in a secondary index. This has to
 be done either within the same mini-transaction, or by invoking
 ibuf_reset_free_bits() before mtr_commit(). On uncompressed pages,
 IBUF_BITMAP_FREE is unaffected by reorganization.

 @retval true if the operation was successful
 @retval false if it is a compressed page, and recompression failed */
bool btr_page_reorganize_low(
    bool recovery,       /*!< in: true if called in recovery:
                        locks should not be updated, i.e.,
                        there cannot exist locks on the
                        page, and a hash index should not be
                        dropped: it cannot exist */
    ulint z_level,       /*!< in: compression level to be used
                         if dealing with compressed page */
    page_cur_t *cursor,  /*!< in/out: page cursor */
    dict_index_t *index, /*!< in: the index tree of the page */
    mtr_t *mtr)          /*!< in/out: mini-transaction */
    MY_ATTRIBUTE((warn_unused_result));
/** Reorganizes an index page.

 IMPORTANT: On success, the caller will have to update IBUF_BITMAP_FREE
 if this is a compressed leaf page in a secondary index. This has to
 be done either within the same mini-transaction, or by invoking
 ibuf_reset_free_bits() before mtr_commit(). On uncompressed pages,
 IBUF_BITMAP_FREE is unaffected by reorganization.

 @retval true if the operation was successful
 @retval false if it is a compressed page, and recompression failed */
bool btr_page_reorganize(
    page_cur_t *cursor,  /*!< in/out: page cursor */
    dict_index_t *index, /*!< in: the index tree of the page */
    mtr_t *mtr);         /*!< in/out: mini-transaction */
/** Decides if the page should be split at the convergence point of
 inserts converging to left.
 @return true if split recommended */
ibool btr_page_get_split_rec_to_left(
    btr_cur_t *cursor, /*!< in: cursor at which to insert */
    rec_t **split_rec) /*!< out: if split recommended,
                     the first record on upper half page,
                     or NULL if tuple should be first */
    MY_ATTRIBUTE((warn_unused_result));
/** Decides if the page should be split at the convergence point of
 inserts converging to right.
 @return true if split recommended */
ibool btr_page_get_split_rec_to_right(
    btr_cur_t *cursor, /*!< in: cursor at which to insert */
    rec_t **split_rec) /*!< out: if split recommended,
                     the first record on upper half page,
                     or NULL if tuple should be first */
    MY_ATTRIBUTE((warn_unused_result));

/** Splits an index page to halves and inserts the tuple. It is assumed
 that mtr holds an x-latch to the index tree. NOTE: the tree x-latch is
 released within this function! NOTE that the operation of this
 function must always succeed, we cannot reverse it: therefore enough
 free disk space (2 pages) must be guaranteed to be available before
 this function is called.

 @return inserted record */
rec_t *btr_page_split_and_insert(
    ulint flags,           /*!< in: undo logging and locking flags */
    btr_cur_t *cursor,     /*!< in: cursor at which to insert; when the
                           function returns, the cursor is positioned
                           on the predecessor of the inserted record */
    ulint **offsets,       /*!< out: offsets on inserted record */
    mem_heap_t **heap,     /*!< in/out: pointer to memory heap
                           that can be emptied, or NULL */
    const dtuple_t *tuple, /*!< in: tuple to insert */
    ulint n_ext,           /*!< in: number of externally stored columns */
    mtr_t *mtr)            /*!< in: mtr */
    MY_ATTRIBUTE((warn_unused_result));
/** Inserts a data tuple to a tree on a non-leaf level. It is assumed
 that mtr holds an x-latch on the tree. */
void btr_insert_on_non_leaf_level_func(
    ulint flags,         /*!< in: undo logging and locking flags */
    dict_index_t *index, /*!< in: index */
    ulint level,         /*!< in: level, must be > 0 */
    dtuple_t *tuple,     /*!< in: the record to be inserted */
    const char *file,    /*!< in: file name */
    ulint line,          /*!< in: line where called */
    mtr_t *mtr);         /*!< in: mtr */
#define btr_insert_on_non_leaf_level(f, i, l, t, m) \
  btr_insert_on_non_leaf_level_func(f, i, l, t, __FILE__, __LINE__, m)
/** Sets a record as the predefined minimum record. */
void btr_set_min_rec_mark(rec_t *rec,  /*!< in/out: record */
                          mtr_t *mtr); /*!< in: mtr */
/** Deletes on the upper level the node pointer to a page. */
void btr_node_ptr_delete(
    dict_index_t *index, /*!< in: index tree */
    buf_block_t *block,  /*!< in: page whose node pointer is deleted */
    mtr_t *mtr);         /*!< in: mtr */
#ifdef UNIV_DEBUG
/** Checks that the node pointer to a page is appropriate.
 @return true */
ibool btr_check_node_ptr(dict_index_t *index, /*!< in: index tree */
                         buf_block_t *block,  /*!< in: index page */
                         mtr_t *mtr)          /*!< in: mtr */
    MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */
/** Tries to merge the page first to the left immediate brother if such a
 brother exists, and the node pointers to the current page and to the
 brother reside on the same page. If the left brother does not satisfy these
 conditions, looks at the right brother. If the page is the only one on that
 level lifts the records of the page to the father page, thus reducing the
 tree height. It is assumed that mtr holds an x-latch on the tree and on the
 page. If cursor is on the leaf level, mtr must also hold x-latches to
 the brothers, if they exist.
 @return true on success */
ibool btr_compress(
    btr_cur_t *cursor, /*!< in/out: cursor on the page to merge
                       or lift; the page must not be empty:
                       when deleting records, use btr_discard_page()
                       if the page would become empty */
    ibool adjust,      /*!< in: TRUE if should adjust the
                       cursor position even if compression occurs */
    mtr_t *mtr);       /*!< in/out: mini-transaction */
/** Discards a page from a B-tree. This is used to remove the last record from
 a B-tree page: the whole page must be removed at the same time. This cannot
 be used for the root page, which is allowed to be empty. */
void btr_discard_page(
    btr_cur_t *cursor, /*!< in: cursor on the page to discard: not on
                       the root page */
    mtr_t *mtr);       /*!< in: mtr */
/** Parses the redo log record for setting an index record as the predefined
 minimum record.
 @return end of log record or NULL */
byte *btr_parse_set_min_rec_mark(
    byte *ptr,     /*!< in: buffer */
    byte *end_ptr, /*!< in: buffer end */
    ulint comp,    /*!< in: nonzero=compact page format */
    page_t *page,  /*!< in: page or NULL */
    mtr_t *mtr)    /*!< in: mtr or NULL */
    MY_ATTRIBUTE((warn_unused_result));
/** Parses a redo log record of reorganizing a page.
 @return end of log record or NULL */
byte *btr_parse_page_reorganize(
    byte *ptr,           /*!< in: buffer */
    byte *end_ptr,       /*!< in: buffer end */
    dict_index_t *index, /*!< in: record descriptor */
    bool compressed,     /*!< in: true if compressed page */
    buf_block_t *block,  /*!< in: page to be reorganized, or NULL */
    mtr_t *mtr)          /*!< in: mtr or NULL */
    MY_ATTRIBUTE((warn_unused_result));
/** Gets the number of pages in a B-tree.
 @return number of pages, or ULINT_UNDEFINED if the index is unavailable */
ulint btr_get_size(dict_index_t *index, /*!< in: index */
                   ulint flag, /*!< in: BTR_N_LEAF_PAGES or BTR_TOTAL_SIZE */
                   mtr_t *mtr) /*!< in/out: mini-transaction where index
                               is s-latched */
    MY_ATTRIBUTE((warn_unused_result));
/** Allocates a new file page to be used in an index tree. NOTE: we assume
 that the caller has made the reservation for free extents!
 @retval NULL if no page could be allocated
 @retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
 (init_mtr == mtr, or the page was not previously freed in mtr)
 @retval block (not allocated or initialized) otherwise */
buf_block_t *btr_page_alloc(
    dict_index_t *index,    /*!< in: index tree */
    page_no_t hint_page_no, /*!< in: hint of a good page */
    byte file_direction,    /*!< in: direction where a possible
                            page split is made */
    ulint level,            /*!< in: level where the page is placed
                            in the tree */
    mtr_t *mtr,             /*!< in/out: mini-transaction
                            for the allocation */
    mtr_t *init_mtr)        /*!< in/out: mini-transaction
                            for x-latching and initializing
                            the page */
    MY_ATTRIBUTE((warn_unused_result));
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
 external storage pages. */
void btr_page_free_low(
    dict_index_t *index, /*!< in: index tree */
    buf_block_t *block,  /*!< in: block to be freed, x-latched */
    ulint level,         /*!< in: page level (ULINT_UNDEFINED=BLOB) */
    mtr_t *mtr);         /*!< in: mtr */
/** Gets the root node of a tree and x- or s-latches it.
 @return root page, x- or s-latched */
buf_block_t *btr_root_block_get(
    const dict_index_t *index, /*!< in: index tree */
    ulint mode,                /*!< in: either RW_S_LATCH
                               or RW_X_LATCH */
    mtr_t *mtr);               /*!< in: mtr */

#ifdef UNIV_BTR_PRINT
/** Prints size info of a B-tree. */
void btr_print_size(dict_index_t *index); /*!< in: index tree */
/** Prints directories and other info of all nodes in the index. */
void btr_print_index(dict_index_t *index, /*!< in: index */
                     ulint width); /*!< in: print this many entries from start
                                   and end */
#endif                             /* UNIV_BTR_PRINT */
/** Checks the size and number of fields in a record based on the definition of
 the index.
 @return true if ok */
ibool btr_index_rec_validate(
    const rec_t *rec,          /*!< in: index record */
    const dict_index_t *index, /*!< in: index */
    ibool dump_on_error)       /*!< in: TRUE if the function
                               should print hex dump of record
                               and page on error */
    MY_ATTRIBUTE((warn_unused_result));
/** Checks the consistency of an index tree.
 @return true if ok */
bool btr_validate_index(
    dict_index_t *index, /*!< in: index */
    const trx_t *trx,    /*!< in: transaction or 0 */
    bool lockout)        /*!< in: true if X-latch index is intended */
    MY_ATTRIBUTE((warn_unused_result));

/** Creates SDI index and stores the root page numbers in page 1 & 2
@param[in]	space_id	tablespace id
@param[in]	dict_locked	true if dict_sys mutex is acquired
@return DB_SUCCESS on success, else DB_ERROR on failure */
dberr_t btr_sdi_create_index(space_id_t space_id, bool dict_locked);

#define BTR_N_LEAF_PAGES 1
#define BTR_TOTAL_SIZE 2

#include "btr0btr.ic"

#endif
