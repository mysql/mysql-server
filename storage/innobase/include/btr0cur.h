/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include <stddef.h>
#include <sys/types.h>

/** @file include/btr0cur.h
 The index tree cursor

 Created 10/16/1994 Heikki Tuuri
 *******************************************************/

#ifndef btr0cur_h
#define btr0cur_h

#include "btr0types.h"
#include "dict0dict.h"
#include "gis0type.h"
#include "page0cur.h"
#include "univ.i"

/** Mode flags for btr_cur operations; these can be ORed */
enum {
  /** do no undo logging */
  BTR_NO_UNDO_LOG_FLAG = 1,
  /** do no record lock checking */
  BTR_NO_LOCKING_FLAG = 2,
  /** sys fields will be found in the update vector or inserted
  entry */
  BTR_KEEP_SYS_FLAG = 4,
  /** btr_cur_pessimistic_update() must keep cursor position
  when moving columns to big_rec */
  BTR_KEEP_POS_FLAG = 8,
  /** the caller is creating the index or wants to bypass the
  index->info.online creation log */
  BTR_CREATE_FLAG = 16,
  /** the caller of btr_cur_optimistic_update() or
  btr_cur_update_in_place() will take care of
  updating IBUF_BITMAP_FREE */
  BTR_KEEP_IBUF_BITMAP = 32
};

/* btr_cur_latch_leaves() returns latched blocks and savepoints. */
struct btr_latch_leaves_t {
  /* left block, target block and right block */
  buf_block_t *blocks[3];
  ulint savepoints[3];
};

#ifndef UNIV_HOTBACKUP
#include "ha0ha.h"
#include "que0types.h"
#include "row0types.h"
#endif /* !UNIV_HOTBACKUP */

#define BTR_CUR_ADAPT
#define BTR_CUR_HASH_ADAPT

#ifdef UNIV_DEBUG
/** Returns the page cursor component of a tree cursor.
 @return pointer to page cursor component */
UNIV_INLINE
page_cur_t *btr_cur_get_page_cur(
    const btr_cur_t *cursor); /*!< in: tree cursor */
/** Returns the buffer block on which the tree cursor is positioned.
 @return pointer to buffer block */
UNIV_INLINE
buf_block_t *btr_cur_get_block(const btr_cur_t *cursor); /*!< in: tree cursor */
/** Returns the record pointer of a tree cursor.
 @return pointer to record */
UNIV_INLINE
rec_t *btr_cur_get_rec(const btr_cur_t *cursor); /*!< in: tree cursor */
#else                                            /* UNIV_DEBUG */
#define btr_cur_get_page_cur(cursor) (&(cursor)->page_cur)
#define btr_cur_get_block(cursor) ((cursor)->page_cur.block)
#define btr_cur_get_rec(cursor) ((cursor)->page_cur.rec)
#endif /* UNIV_DEBUG */
/** Returns the compressed page on which the tree cursor is positioned.
 @return pointer to compressed page, or NULL if the page is not compressed */
UNIV_INLINE
page_zip_des_t *btr_cur_get_page_zip(btr_cur_t *cursor); /*!< in: tree cursor */
/** Returns the page of a tree cursor.
 @return pointer to page */
UNIV_INLINE
page_t *btr_cur_get_page(btr_cur_t *cursor); /*!< in: tree cursor */
/** Returns the index of a cursor.
 @param cursor b-tree cursor
 @return index */
#define btr_cur_get_index(cursor) ((cursor)->index)

/** Positions a tree cursor at a given record.
@param[in]	index	index
@param[in]	rec	record in tree
@param[in]	block	buffer block of rec
@param[in]	cursor	cursor */
UNIV_INLINE
void btr_cur_position(dict_index_t *index, rec_t *rec, buf_block_t *block,
                      btr_cur_t *cursor);

/** Optimistically latches the leaf page or pages requested.
@param[in]	block		guessed buffer block
@param[in]	modify_clock	modify clock value
@param[in,out]	latch_mode	BTR_SEARCH_LEAF, ...
@param[in,out]	cursor		cursor
@param[in]	file		file name
@param[in]	line		line where called
@param[in]	mtr		mini-transaction
@return true if success */
bool btr_cur_optimistic_latch_leaves(buf_block_t *block,
                                     ib_uint64_t modify_clock,
                                     ulint *latch_mode, btr_cur_t *cursor,
                                     const char *file, ulint line, mtr_t *mtr);

/** Searches an index tree and positions a tree cursor on a given level.
 NOTE: n_fields_cmp in tuple must be set so that it cannot be compared
 to node pointer page number fields on the upper levels of the tree!
 Note that if mode is PAGE_CUR_LE, which is used in inserts, then
 cursor->up_match and cursor->low_match both will have sensible values.
 If mode is PAGE_CUR_GE, then up_match will a have a sensible value. */
void btr_cur_search_to_nth_level(
    dict_index_t *index,   /*!< in: index */
    ulint level,           /*!< in: the tree level of search */
    const dtuple_t *tuple, /*!< in: data tuple; NOTE: n_fields_cmp in
                           tuple must be set so that it cannot get
                           compared to the node ptr page number field! */
    page_cur_mode_t mode,  /*!< in: PAGE_CUR_L, ...;
                           NOTE that if the search is made using a unique
                           prefix of a record, mode should be PAGE_CUR_LE,
                           not PAGE_CUR_GE, as the latter may end up on
                           the previous page of the record! Inserts
                           should always be made using PAGE_CUR_LE to
                           search the position! */
    ulint latch_mode,      /*!< in: BTR_SEARCH_LEAF, ..., ORed with
                       at most one of BTR_INSERT, BTR_DELETE_MARK,
                       BTR_DELETE, or BTR_ESTIMATE;
                       cursor->left_block is used to store a pointer
                       to the left neighbor page, in the cases
                       BTR_SEARCH_PREV and BTR_MODIFY_PREV;
                       NOTE that if has_search_latch
                       is != 0, we maybe do not have a latch set
                       on the cursor page, we assume
                       the caller uses his search latch
                       to protect the record! */
    btr_cur_t *cursor,     /*!< in/out: tree cursor; the cursor page is
                           s- or x-latched, but see also above! */
    ulint has_search_latch,
    /*!< in: latch mode the caller
    currently has on search system:
    RW_S_LATCH, or 0 */
    const char *file, /*!< in: file name */
    ulint line,       /*!< in: line where called */
    mtr_t *mtr);      /*!< in: mtr */

/** Searches an index tree and positions a tree cursor on a given level.
This function will avoid placing latches the travesal path and so
should be used only for cases where-in latching is not needed.

@param[in]	index	index
@param[in]	level	the tree level of search
@param[in]	tuple	data tuple; Note: n_fields_cmp in compared
                        to the node ptr page node field
@param[in]	mode	PAGE_CUR_L, ....
                        Insert should always be made using PAGE_CUR_LE
                        to search the position.
@param[in,out]	cursor	tree cursor; points to record of interest.
@param[in]	file	file name
@param[in]	line	line where called from
@param[in,out]	mtr	mtr
@param[in]	mark_dirty
                        if true then mark the block as dirty */
void btr_cur_search_to_nth_level_with_no_latch(
    dict_index_t *index, ulint level, const dtuple_t *tuple,
    page_cur_mode_t mode, btr_cur_t *cursor, const char *file, ulint line,
    mtr_t *mtr, bool mark_dirty = true);

/** Opens a cursor at either end of an index. */
void btr_cur_open_at_index_side_func(
    bool from_left,      /*!< in: true if open to the low end,
                         false if to the high end */
    dict_index_t *index, /*!< in: index */
    ulint latch_mode,    /*!< in: latch mode */
    btr_cur_t *cursor,   /*!< in/out: cursor */
    ulint level,         /*!< in: level to search for
                         (0=leaf) */
    const char *file,    /*!< in: file name */
    ulint line,          /*!< in: line where called */
    mtr_t *mtr);         /*!< in/out: mini-transaction */
#define btr_cur_open_at_index_side(f, i, l, c, lv, m) \
  btr_cur_open_at_index_side_func(f, i, l, c, lv, __FILE__, __LINE__, m)

/** Opens a cursor at either end of an index.
Avoid taking latches on buffer, just pin (by incrementing fix_count)
to keep them in buffer pool. This mode is used by intrinsic table
as they are not shared and so there is no need of latching.
@param[in]	from_left	true if open to low end, false if open
                                to high end.
@param[in]	index		index
@param[in,out]	cursor		cursor
@param[in]	file		file name
@param[in]	line		line where called
@param[in,out]	mtr		mini transaction
*/
void btr_cur_open_at_index_side_with_no_latch_func(
    bool from_left, dict_index_t *index, btr_cur_t *cursor, ulint level,
    const char *file, ulint line, mtr_t *mtr);
#define btr_cur_open_at_index_side_with_no_latch(f, i, c, lv, m)       \
  btr_cur_open_at_index_side_with_no_latch_func(f, i, c, lv, __FILE__, \
                                                __LINE__, m)

/** Positions a cursor at a randomly chosen position within a B-tree.
 @return true if the index is available and we have put the cursor, false
 if the index is unavailable */
bool btr_cur_open_at_rnd_pos_func(
    dict_index_t *index, /*!< in: index */
    ulint latch_mode,    /*!< in: BTR_SEARCH_LEAF, ... */
    btr_cur_t *cursor,   /*!< in/out: B-tree cursor */
    const char *file,    /*!< in: file name */
    ulint line,          /*!< in: line where called */
    mtr_t *mtr);         /*!< in: mtr */
#define btr_cur_open_at_rnd_pos(i, l, c, m) \
  btr_cur_open_at_rnd_pos_func(i, l, c, __FILE__, __LINE__, m)
/** Tries to perform an insert to a page in an index tree, next to cursor.
 It is assumed that mtr holds an x-latch on the page. The operation does
 not succeed if there is too little space on the page. If there is just
 one record on the page, the insert will always succeed; this is to
 prevent trying to split a page with just one record.
 @return DB_SUCCESS, DB_WAIT_LOCK, DB_FAIL, or error number */
dberr_t btr_cur_optimistic_insert(
    ulint flags,         /*!< in: undo logging and locking flags: if not
                         zero, the parameters index and thr should be
                         specified */
    btr_cur_t *cursor,   /*!< in: cursor on page after which to insert;
                         cursor stays valid */
    ulint **offsets,     /*!< out: offsets on *rec */
    mem_heap_t **heap,   /*!< in/out: pointer to memory heap, or NULL */
    dtuple_t *entry,     /*!< in/out: entry to insert */
    rec_t **rec,         /*!< out: pointer to inserted record if
                         succeed */
    big_rec_t **big_rec, /*!< out: big rec vector whose fields have to
                         be stored externally by the caller, or
                         NULL */
    ulint n_ext,         /*!< in: number of externally stored columns */
    que_thr_t *thr,      /*!< in: query thread or NULL */
    mtr_t *mtr)          /*!< in/out: mini-transaction;
                         if this function returns DB_SUCCESS on
                         a leaf page of a secondary index in a
                         compressed tablespace, the caller must
                         mtr_commit(mtr) before latching
                         any further pages */
    MY_ATTRIBUTE((warn_unused_result));
/** Performs an insert on a page of an index tree. It is assumed that mtr
 holds an x-latch on the tree and on the cursor page. If the insert is
 made on the leaf level, to avoid deadlocks, mtr must also own x-latches
 to brothers of page, if those brothers exist.
 @return DB_SUCCESS or error number */
dberr_t btr_cur_pessimistic_insert(
    ulint flags,         /*!< in: undo logging and locking flags: if not
                         zero, the parameter thr should be
                         specified; if no undo logging is specified,
                         then the caller must have reserved enough
                         free extents in the file space so that the
                         insertion will certainly succeed */
    btr_cur_t *cursor,   /*!< in: cursor after which to insert;
                         cursor stays valid */
    ulint **offsets,     /*!< out: offsets on *rec */
    mem_heap_t **heap,   /*!< in/out: pointer to memory heap
                         that can be emptied, or NULL */
    dtuple_t *entry,     /*!< in/out: entry to insert */
    rec_t **rec,         /*!< out: pointer to inserted record if
                         succeed */
    big_rec_t **big_rec, /*!< out: big rec vector whose fields have to
                         be stored externally by the caller, or
                         NULL */
    ulint n_ext,         /*!< in: number of externally stored columns */
    que_thr_t *thr,      /*!< in: query thread or NULL */
    mtr_t *mtr)          /*!< in/out: mini-transaction */
    MY_ATTRIBUTE((warn_unused_result));
/** See if there is enough place in the page modification log to log
 an update-in-place.

 @retval false if out of space; IBUF_BITMAP_FREE will be reset
 outside mtr if the page was recompressed
 @retval true if enough place;

 IMPORTANT: The caller will have to update IBUF_BITMAP_FREE if this is
 a secondary index leaf page. This has to be done either within the
 same mini-transaction, or by invoking ibuf_reset_free_bits() before
 mtr_commit(mtr). */
bool btr_cur_update_alloc_zip_func(
    page_zip_des_t *page_zip, /*!< in/out: compressed page */
    page_cur_t *cursor,       /*!< in/out: B-tree page cursor */
    dict_index_t *index,      /*!< in: the index corresponding to cursor */
#ifdef UNIV_DEBUG
    ulint *offsets, /*!< in/out: offsets of the cursor record */
#endif              /* UNIV_DEBUG */
    ulint length,   /*!< in: size needed */
    bool create,    /*!< in: true=delete-and-insert,
                    false=update-in-place */
    mtr_t *mtr)     /*!< in/out: mini-transaction */
    MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
#define btr_cur_update_alloc_zip(page_zip, cursor, index, offsets, len, cr, \
                                 mtr)                                       \
  btr_cur_update_alloc_zip_func(page_zip, cursor, index, offsets, len, cr, mtr)
#else /* UNIV_DEBUG */
#define btr_cur_update_alloc_zip(page_zip, cursor, index, offsets, len, cr, \
                                 mtr)                                       \
  btr_cur_update_alloc_zip_func(page_zip, cursor, index, len, cr, mtr)
#endif /* UNIV_DEBUG */
/** Updates a record when the update causes no size changes in its fields.
 @return locking or undo log related error code, or
 @retval DB_SUCCESS on success
 @retval DB_ZIP_OVERFLOW if there is not enough space left
 on the compressed page (IBUF_BITMAP_FREE was reset outside mtr) */
dberr_t btr_cur_update_in_place(
    ulint flags,         /*!< in: undo logging and locking flags */
    btr_cur_t *cursor,   /*!< in: cursor on the record to update;
                         cursor stays valid and positioned on the
                         same record */
    ulint *offsets,      /*!< in/out: offsets on cursor->page_cur.rec */
    const upd_t *update, /*!< in: update vector */
    ulint cmpl_info,     /*!< in: compiler info on secondary index
                       updates */
    que_thr_t *thr,      /*!< in: query thread, or NULL if
                         flags & (BTR_NO_LOCKING_FLAG
                         | BTR_NO_UNDO_LOG_FLAG
                         | BTR_CREATE_FLAG
                         | BTR_KEEP_SYS_FLAG) */
    trx_id_t trx_id,     /*!< in: transaction id */
    mtr_t *mtr)          /*!< in/out: mini-transaction; if this
                         is a secondary index, the caller must
                         mtr_commit(mtr) before latching any
                         further pages */
    MY_ATTRIBUTE((warn_unused_result));
/** Writes a redo log record of updating a record in-place. */
void btr_cur_update_in_place_log(
    ulint flags,         /*!< in: flags */
    const rec_t *rec,    /*!< in: record */
    dict_index_t *index, /*!< in: index of the record */
    const upd_t *update, /*!< in: update vector */
    trx_id_t trx_id,     /*!< in: transaction id */
    roll_ptr_t roll_ptr, /*!< in: roll ptr */
    mtr_t *mtr);         /*!< in: mtr */
/** Tries to update a record on a page in an index tree. It is assumed that mtr
 holds an x-latch on the page. The operation does not succeed if there is too
 little space on the page or if the update would result in too empty a page,
 so that tree compression is recommended.
 @return error code, including
 @retval DB_SUCCESS on success
 @retval DB_OVERFLOW if the updated record does not fit
 @retval DB_UNDERFLOW if the page would become too empty
 @retval DB_ZIP_OVERFLOW if there is not enough space left
 on the compressed page */
dberr_t btr_cur_optimistic_update(
    ulint flags,         /*!< in: undo logging and locking flags */
    btr_cur_t *cursor,   /*!< in: cursor on the record to update;
                         cursor stays valid and positioned on the
                         same record */
    ulint **offsets,     /*!< out: offsets on cursor->page_cur.rec */
    mem_heap_t **heap,   /*!< in/out: pointer to NULL or memory heap */
    const upd_t *update, /*!< in: update vector; this must also
                         contain trx id and roll ptr fields */
    ulint cmpl_info,     /*!< in: compiler info on secondary index
                       updates */
    que_thr_t *thr,      /*!< in: query thread, or NULL if
                         flags & (BTR_NO_UNDO_LOG_FLAG
                         | BTR_NO_LOCKING_FLAG
                         | BTR_CREATE_FLAG
                         | BTR_KEEP_SYS_FLAG) */
    trx_id_t trx_id,     /*!< in: transaction id */
    mtr_t *mtr)          /*!< in/out: mini-transaction; if this
                         is a secondary index, the caller must
                         mtr_commit(mtr) before latching any
                         further pages */
    MY_ATTRIBUTE((warn_unused_result));
/** Performs an update of a record on a page of a tree. It is assumed
 that mtr holds an x-latch on the tree and on the cursor page. If the
 update is made on the leaf level, to avoid deadlocks, mtr must also
 own x-latches to brothers of page, if those brothers exist.
 @return DB_SUCCESS or error code */
dberr_t btr_cur_pessimistic_update(
    ulint flags,       /*!< in: undo logging, locking, and rollback
                       flags */
    btr_cur_t *cursor, /*!< in/out: cursor on the record to update;
                       cursor may become invalid if *big_rec == NULL
                       || !(flags & BTR_KEEP_POS_FLAG) */
    ulint **offsets,   /*!< out: offsets on cursor->page_cur.rec */
    mem_heap_t **offsets_heap,
    /*!< in/out: pointer to memory heap
    that can be emptied, or NULL */
    mem_heap_t *entry_heap,
    /*!< in/out: memory heap for allocating
    big_rec and the index tuple */
    big_rec_t **big_rec, /*!< out: big rec vector whose fields have to
                         be stored externally by the caller, or NULL */
    upd_t *update,       /*!< in/out: update vector; this is allowed to
                         also contain trx id and roll ptr fields.
                         Non-updated columns that are moved offpage will
                         be appended to this. */
    ulint cmpl_info,     /*!< in: compiler info on secondary index
                       updates */
    que_thr_t *thr,      /*!< in: query thread, or NULL if
                         flags & (BTR_NO_UNDO_LOG_FLAG
                         | BTR_NO_LOCKING_FLAG
                         | BTR_CREATE_FLAG
                         | BTR_KEEP_SYS_FLAG) */
    trx_id_t trx_id,     /*!< in: transaction id */
    undo_no_t undo_no,
    /*!< in: undo number of the transaction. This
    is needed for rollback to savepoint of
    partially updated LOB.*/
    mtr_t *mtr) /*!< in/out: mini-transaction; must be committed
                before latching any further pages */
    MY_ATTRIBUTE((warn_unused_result));
/** Marks a clustered index record deleted. Writes an undo log record to
 undo log on this delete marking. Writes in the trx id field the id
 of the deleting transaction, and in the roll ptr field pointer to the
 undo log record created.
 @return DB_SUCCESS, DB_LOCK_WAIT, or error number */
dberr_t btr_cur_del_mark_set_clust_rec(
    ulint flags,           /*!< in: undo logging and locking flags */
    buf_block_t *block,    /*!< in/out: buffer block of the record */
    rec_t *rec,            /*!< in/out: record */
    dict_index_t *index,   /*!< in: clustered index of the record */
    const ulint *offsets,  /*!< in: rec_get_offsets(rec) */
    que_thr_t *thr,        /*!< in: query thread */
    const dtuple_t *entry, /*!< in: dtuple for the deleting record */
    mtr_t *mtr)            /*!< in/out: mini-transaction */
    MY_ATTRIBUTE((warn_unused_result));
/** Sets a secondary index record delete mark to TRUE or FALSE.
 @return DB_SUCCESS, DB_LOCK_WAIT, or error number */
dberr_t btr_cur_del_mark_set_sec_rec(
    ulint flags,       /*!< in: locking flag */
    btr_cur_t *cursor, /*!< in: cursor */
    ibool val,         /*!< in: value to set */
    que_thr_t *thr,    /*!< in: query thread */
    mtr_t *mtr)        /*!< in/out: mini-transaction */
    MY_ATTRIBUTE((warn_unused_result));
/** Tries to compress a page of the tree if it seems useful. It is assumed
 that mtr holds an x-latch on the tree and on the cursor page. To avoid
 deadlocks, mtr must also own x-latches to brothers of page, if those
 brothers exist. NOTE: it is assumed that the caller has reserved enough
 free extents so that the compression will always succeed if done!
 @return true if compression occurred */
ibool btr_cur_compress_if_useful(
    btr_cur_t *cursor, /*!< in/out: cursor on the page to compress;
                       cursor does not stay valid if compression
                       occurs */
    ibool adjust,      /*!< in: TRUE if should adjust the
                       cursor position even if compression occurs */
    mtr_t *mtr);       /*!< in/out: mini-transaction */
/** Removes the record on which the tree cursor is positioned. It is assumed
 that the mtr has an x-latch on the page where the cursor is positioned,
 but no latch on the whole tree.
 @return true if success, i.e., the page did not become too empty */
ibool btr_cur_optimistic_delete_func(
    btr_cur_t *cursor, /*!< in: cursor on the record to delete;
                       cursor stays valid: if deletion succeeds,
                       on function exit it points to the successor
                       of the deleted record */
#ifdef UNIV_DEBUG
    ulint flags, /*!< in: BTR_CREATE_FLAG or 0 */
#endif           /* UNIV_DEBUG */
    mtr_t *mtr)  /*!< in: mtr; if this function returns
                 TRUE on a leaf page of a secondary
                 index, the mtr must be committed
                 before latching any further pages */
    MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
#define btr_cur_optimistic_delete(cursor, flags, mtr) \
  btr_cur_optimistic_delete_func(cursor, flags, mtr)
#else /* UNIV_DEBUG */
#define btr_cur_optimistic_delete(cursor, flags, mtr) \
  btr_cur_optimistic_delete_func(cursor, mtr)
#endif /* UNIV_DEBUG */
/** Removes the record on which the tree cursor is positioned. Tries
 to compress the page if its fillfactor drops below a threshold
 or if it is the only page on the level. It is assumed that mtr holds
 an x-latch on the tree and on the cursor page. To avoid deadlocks,
 mtr must also own x-latches to brothers of page, if those brothers
 exist.
 @return true if compression occurred */
ibool btr_cur_pessimistic_delete(
    dberr_t *err,               /*!< out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE;
                        the latter may occur because we may have
                        to update node pointers on upper levels,
                        and in the case of variable length keys
                        these may actually grow in size */
    ibool has_reserved_extents, /*!< in: TRUE if the
                  caller has already reserved enough free
                  extents so that he knows that the operation
                  will succeed */
    btr_cur_t *cursor,          /*!< in: cursor on the record to delete;
                                if compression does not occur, the cursor
                                stays valid: it points to successor of
                                deleted record on function exit */
    ulint flags,                /*!< in: BTR_CREATE_FLAG or 0 */
    bool rollback,              /*!< in: performing rollback? */
    trx_id_t trx_id,            /*!< in: the current transaction id. */
    undo_no_t undo_no,
    /*!< in: undo number of the transaction. This
    is needed for rollback to savepoint of
    partially updated LOB.*/
    ulint rec_type,
    /*!< in: undo record type. */
    mtr_t *mtr); /*!< in: mtr */
/** Parses a redo log record of updating a record in-place.
 @return end of log record or NULL */
byte *btr_cur_parse_update_in_place(
    byte *ptr,                /*!< in: buffer */
    byte *end_ptr,            /*!< in: buffer end */
    page_t *page,             /*!< in/out: page or NULL */
    page_zip_des_t *page_zip, /*!< in/out: compressed page, or NULL */
    dict_index_t *index);     /*!< in: index corresponding to page */
/** Parses the redo log record for delete marking or unmarking of a clustered
 index record.
 @return end of log record or NULL */
byte *btr_cur_parse_del_mark_set_clust_rec(
    byte *ptr,                /*!< in: buffer */
    byte *end_ptr,            /*!< in: buffer end */
    page_t *page,             /*!< in/out: page or NULL */
    page_zip_des_t *page_zip, /*!< in/out: compressed page, or NULL */
    dict_index_t *index);     /*!< in: index corresponding to page */
/** Parses the redo log record for delete marking or unmarking of a secondary
 index record.
 @return end of log record or NULL */
byte *btr_cur_parse_del_mark_set_sec_rec(
    byte *ptr,                 /*!< in: buffer */
    byte *end_ptr,             /*!< in: buffer end */
    page_t *page,              /*!< in/out: page or NULL */
    page_zip_des_t *page_zip); /*!< in/out: compressed page, or NULL */
#ifndef UNIV_HOTBACKUP

/** Estimates the number of rows in a given index range.
@param[in]	index	index
@param[in]	tuple1	range start, may also be empty tuple
@param[in]	mode1	search mode for range start
@param[in]	tuple2	range end, may also be empty tuple
@param[in]	mode2	search mode for range end
@return estimated number of rows */
int64_t btr_estimate_n_rows_in_range(dict_index_t *index,
                                     const dtuple_t *tuple1,
                                     page_cur_mode_t mode1,
                                     const dtuple_t *tuple2,
                                     page_cur_mode_t mode2);

/** Estimates the number of different key values in a given index, for
 each n-column prefix of the index where 1 <= n <=
 dict_index_get_n_unique(index). The estimates are stored in the array
 index->stat_n_diff_key_vals[] (indexed 0..n_uniq-1) and the number of pages
 that were sampled is saved in index->stat_n_sample_sizes[]. If
 innodb_stats_method is nulls_ignored, we also record the number of non-null
 values for each prefix and stored the estimates in array
 index->stat_n_non_null_key_vals.
 @return true if the index is available and we get the estimated numbers,
 false if the index is unavailable. */
bool btr_estimate_number_of_different_key_vals(
    dict_index_t *index); /*!< in: index */

/** Copies an externally stored field of a record to mem heap.
@param[in]	trx		the trx doing the operation.
@param[in]	index		index containing the LOB.
@param[in]	rec		record in a clustered index; must be
                                protected by a lock or a page latch
@param[in]	offsets		array returned by rec_get_offsets()
@param[in]	page_size	BLOB page size
@param[in]	no		field number
@param[out]	len		length of the field
@param[out]	lob_version	version of lob
@param[in]	is_sdi		true for SDI Indexes
@param[in,out]	heap		mem heap
@return the field copied to heap, or NULL if the field is incomplete */
byte *btr_rec_copy_externally_stored_field_func(
    trx_t *trx, dict_index_t *index, const rec_t *rec, const ulint *offsets,
    const page_size_t &page_size, ulint no, ulint *len, size_t *lob_version,
#ifdef UNIV_DEBUG
    bool is_sdi,
#endif /* UNIV_DEBUG */
    mem_heap_t *heap);

/** Sets a secondary index record's delete mark to the given value. This
 function is only used by the insert buffer merge mechanism. */
void btr_cur_set_deleted_flag_for_ibuf(
    rec_t *rec,               /*!< in/out: record */
    page_zip_des_t *page_zip, /*!< in/out: compressed page
                              corresponding to rec, or NULL
                              when the tablespace is uncompressed */
    ibool val,                /*!< in: value to set */
    mtr_t *mtr);              /*!< in/out: mini-transaction */

/** The following function is used to set the deleted bit of a record.
@param[in,out]	rec		physical record
@param[in,out]	page_zip	compressed page (or NULL)
@param[in]	flag		nonzero if delete marked */
UNIV_INLINE
void btr_rec_set_deleted_flag(rec_t *rec, page_zip_des_t *page_zip, ulint flag);

/** Latches the leaf page or pages requested.
@param[in]	block		leaf page where the search converged
@param[in]	page_id		page id of the leaf
@param[in]	latch_mode	BTR_SEARCH_LEAF, ...
@param[in]	cursor		cursor
@param[in]	mtr		mini-transaction
@return	blocks and savepoints which actually latched. */
btr_latch_leaves_t btr_cur_latch_leaves(buf_block_t *block,
                                        const page_id_t &page_id,
                                        const page_size_t &page_size,
                                        ulint latch_mode, btr_cur_t *cursor,
                                        mtr_t *mtr);
#endif /* !UNIV_HOTBACKUP */

/*######################################################################*/

/** In the pessimistic delete, if the page data size drops below this
limit, merging it to a neighbor is tried */
#define BTR_CUR_PAGE_COMPRESS_LIMIT(index) \
  ((UNIV_PAGE_SIZE * (ulint)((index)->merge_threshold)) / 100)

/** A slot in the path array. We store here info on a search path down the
tree. Each slot contains data on a single level of the tree. */
struct btr_path_t {
  /* Assume a page like:
  records:		(inf, a, b, c, d, sup)
  index of the record:	0, 1, 2, 3, 4, 5
  */

  /** Index of the record where the page cursor stopped on this level
  (index in alphabetical order). Value ULINT_UNDEFINED denotes array
  end. In the above example, if the search stopped on record 'c', then
  nth_rec will be 3. */
  ulint nth_rec;

  /** Number of the records on the page, not counting inf and sup.
  In the above example n_recs will be 4. */
  ulint n_recs;

  /** Number of the page containing the record. */
  page_no_t page_no;

  /** Level of the page. If later we fetch the page under page_no
  and it is no different level then we know that the tree has been
  reorganized. */
  ulint page_level;
};

#define BTR_PATH_ARRAY_N_SLOTS 250 /*!< size of path array (in slots) */

/** Values for the flag documenting the used search method */
enum btr_cur_method {
  BTR_CUR_UNSET = 0,      /*!< Flag for initialization only,
                          not in real use. */
  BTR_CUR_HASH = 1,       /*!< successful shortcut using
                          the hash index */
  BTR_CUR_HASH_FAIL,      /*!< failure using hash, success using
                          binary search: the misleading hash
                          reference is stored in the field
                          hash_node, and might be necessary to
                          update */
  BTR_CUR_BINARY,         /*!< success using the binary search */
  BTR_CUR_INSERT_TO_IBUF, /*!< performed the intended insert to
                          the insert buffer */
  BTR_CUR_DEL_MARK_IBUF,  /*!< performed the intended delete
                          mark in the insert/delete buffer */
  BTR_CUR_DELETE_IBUF,    /*!< performed the intended delete in
                          the insert/delete buffer */
  BTR_CUR_DELETE_REF      /*!< row_purge_poss_sec() failed */
};

/** The tree cursor: the definition appears here only for the compiler
to know struct size! */
struct btr_cur_t {
  dict_index_t *index{nullptr};      /*!< index where positioned */
  page_cur_t page_cur;               /*!< page cursor */
  purge_node_t *purge_node{nullptr}; /*!< purge node, for BTR_DELETE */
  buf_block_t *left_block{nullptr};  /*!< this field is used to store
                             a pointer to the left neighbor
                             page, in the cases
                             BTR_SEARCH_PREV and
                             BTR_MODIFY_PREV */
  /*------------------------------*/
  que_thr_t *thr{nullptr}; /*!< this field is only used
                           when btr_cur_search_to_nth_level
                           is called for an index entry
                           insertion: the calling query
                           thread is passed here to be
                           used in the insert buffer */
  /*------------------------------*/
  /** The following fields are used in
  btr_cur_search_to_nth_level to pass information: */
  /* @{ */
  btr_cur_method flag{BTR_CUR_UNSET}; /*!< Search method used */
  ulint tree_height{0};               /*!< Tree height if the search is done
                                      for a pessimistic insert or update
                                      operation */
  ulint up_match{0};                  /*!< If the search mode was PAGE_CUR_LE,
                                      the number of matched fields to the
                                      the first user record to the right of
                                      the cursor record after
                                      btr_cur_search_to_nth_level;
                                      for the mode PAGE_CUR_GE, the matched
                                      fields to the first user record AT THE
                                      CURSOR or to the right of it;
                                      NOTE that the up_match and low_match
                                      values may exceed the correct values
                                      for comparison to the adjacent user
                                      record if that record is on a
                                      different leaf page! (See the note in
                                      row_ins_duplicate_error_in_clust.) */
  ulint up_bytes{0};                  /*!< number of matched bytes to the
                                      right at the time cursor positioned;
                                      only used internally in searches: not
                                      defined after the search */
  ulint low_match{0};                 /*!< if search mode was PAGE_CUR_LE,
                                      the number of matched fields to the
                                      first user record AT THE CURSOR or
                                      to the left of it after
                                      btr_cur_search_to_nth_level;
                                      NOT defined for PAGE_CUR_GE or any
                                      other search modes; see also the NOTE
                                      in up_match! */
  ulint low_bytes{0};                 /*!< number of matched bytes to the
                                      left at the time cursor positioned;
                                      only used internally in searches: not
                                      defined after the search */
  ulint n_fields{0};                  /*!< prefix length used in a hash
                                      search if hash_node != NULL */
  ulint n_bytes{0};                   /*!< hash prefix bytes if hash_node !=
                                      NULL */
  ulint fold{0};                      /*!< fold value used in the search if
                                      flag is BTR_CUR_HASH */
  /* @} */
  btr_path_t *path_arr{nullptr}; /*!< in estimating the number of
                         rows in range, we store in this array
                         information of the path through
                         the tree */
  rtr_info_t *rtr_info{nullptr}; /*!< rtree search info */
};

/** The following function is used to set the deleted bit of a record.
@param[in,out]	rec		physical record
@param[in,out]	page_zip	compressed page (or NULL)
@param[in]	flag		nonzero if delete marked */
UNIV_INLINE
void btr_rec_set_deleted_flag(rec_t *rec, page_zip_des_t *page_zip, ulint flag);

/** If pessimistic delete fails because of lack of file space, there
is still a good change of success a little later.  Try this many
times. */
#define BTR_CUR_RETRY_DELETE_N_TIMES 100
/** If pessimistic delete fails because of lack of file space, there
is still a good change of success a little later.  Sleep this many
microseconds between retries. */
#define BTR_CUR_RETRY_SLEEP_TIME 50000

/** Number of searches down the B-tree in btr_cur_search_to_nth_level(). */
extern ulint btr_cur_n_non_sea;
/** Number of successful adaptive hash index lookups in
btr_cur_search_to_nth_level(). */
extern ulint btr_cur_n_sea;
/** Old value of btr_cur_n_non_sea.  Copied by
srv_refresh_innodb_monitor_stats().  Referenced by
srv_printf_innodb_monitor(). */
extern ulint btr_cur_n_non_sea_old;
/** Old value of btr_cur_n_sea.  Copied by
srv_refresh_innodb_monitor_stats().  Referenced by
srv_printf_innodb_monitor(). */
extern ulint btr_cur_n_sea_old;

#ifdef UNIV_DEBUG
/* Flag to limit optimistic insert records */
extern uint btr_cur_limit_optimistic_insert_debug;
#endif /* UNIV_DEBUG */

#include "btr0cur.ic"

#endif
