/*****************************************************************************

Copyright (c) 1994, 2024, Oracle and/or its affiliates.

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

#ifdef UNIV_DEBUG
/** Returns the page cursor component of a tree cursor.
 @return pointer to page cursor component */
static inline page_cur_t *btr_cur_get_page_cur(
    const btr_cur_t *cursor); /*!< in: tree cursor */
/** Returns the buffer block on which the tree cursor is positioned.
 @return pointer to buffer block */
static inline buf_block_t *btr_cur_get_block(
    const btr_cur_t *cursor); /*!< in: tree cursor */
/** Returns the record pointer of a tree cursor.
 @return pointer to record */
static inline rec_t *btr_cur_get_rec(
    const btr_cur_t *cursor); /*!< in: tree cursor */
#else                         /* UNIV_DEBUG */
#define btr_cur_get_page_cur(cursor) (&(cursor)->page_cur)
#define btr_cur_get_block(cursor) ((cursor)->page_cur.block)
#define btr_cur_get_rec(cursor) ((cursor)->page_cur.rec)
#endif /* UNIV_DEBUG */
/** Returns the compressed page on which the tree cursor is positioned.
 @return pointer to compressed page, or NULL if the page is not compressed */
static inline page_zip_des_t *btr_cur_get_page_zip(
    btr_cur_t *cursor); /*!< in: tree cursor */
/** Returns the page of a tree cursor.
 @return pointer to page */
static inline page_t *btr_cur_get_page(
    btr_cur_t *cursor); /*!< in: tree cursor */

/** Positions a tree cursor at a given record.
@param[in]      index   index
@param[in]      rec     record in tree
@param[in]      block   buffer block of rec
@param[in]      cursor  cursor */
static inline void btr_cur_position(dict_index_t *index, rec_t *rec,
                                    buf_block_t *block, btr_cur_t *cursor);

/** Optimistically latches the leaf page or pages requested.
@param[in]      block           Guessed buffer block
@param[in]      modify_clock    Modify clock value
@param[in,out]  latch_mode      BTR_SEARCH_LEAF, ...
@param[in,out]  cursor          Cursor
@param[in]      file            File name
@param[in]      line            Line where called
@param[in]      mtr             Mini-transaction
@return true if success */
bool btr_cur_optimistic_latch_leaves(buf_block_t *block, uint64_t modify_clock,
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
This function will avoid placing latches while traversing the path and so
should be used only for cases where-in latching is not needed.

@param[in]      index   Index
@param[in]      level   The tree level of search
@param[in]      tuple   Data tuple; Note: n_fields_cmp in compared
                        to the node ptr page node field
@param[in]      mode    PAGE_CUR_L, ....
                        Insert should always be made using PAGE_CUR_LE
                        to search the position.
@param[in,out]  cursor  Tree cursor; points to record of interest.
@param[in]      file    File name
@param[in]      line    Line where called from
@param[in,out]  mtr     Mini-transaction
@param[in]      mark_dirty if true then mark the block as dirty */
void btr_cur_search_to_nth_level_with_no_latch(
    dict_index_t *index, ulint level, const dtuple_t *tuple,
    page_cur_mode_t mode, btr_cur_t *cursor, const char *file, ulint line,
    mtr_t *mtr, bool mark_dirty = true);

/** Opens a cursor at either end of an index.
@param[in]      from_left   True if open to the low end, false if to the high
end
@param[in]      index       Index
@param[in]      latch_mode  Latch mode
@param[in,out]  cursor      Cursor
@param[in]      level       Level to search for (0=leaf)
@param[in]      location    Location where called
@param[in,out] mtr Mini-transaction */
void btr_cur_open_at_index_side(bool from_left, dict_index_t *index,
                                ulint latch_mode, btr_cur_t *cursor,
                                ulint level, ut::Location location, mtr_t *mtr);

/** Opens a cursor at either end of an index.
Avoid taking latches on buffer, just pin (by incrementing fix_count)
to keep them in buffer pool. This mode is used by intrinsic table
as they are not shared and so there is no need of latching.
@param[in]      from_left       true if open to low end, false if open to high
end.
@param[in]      index   Index
@param[in,out]  cursor  Cursor
@param[in]      level   Level to search for (0=leaf)
@param[in]      location        Location where called
@param[in,out]  mtr     Mini-transaction */
void btr_cur_open_at_index_side_with_no_latch(bool from_left,
                                              dict_index_t *index,
                                              btr_cur_t *cursor, ulint level,
                                              ut::Location location,
                                              mtr_t *mtr);

/** Positions a cursor at a randomly chosen position within a B-tree.
 @return true if the index is available and we have put the cursor, false
 if the index is unavailable */
bool btr_cur_open_at_rnd_pos(dict_index_t *index, /*!< in: index */
                             ulint latch_mode,  /*!< in: BTR_SEARCH_LEAF, ... */
                             btr_cur_t *cursor, /*!< in/out: B-tree cursor */
                             const char *file,  /*!< in: file name */
                             ulint line,        /*!< in: line where called */
                             mtr_t *mtr);       /*!< in: mtr */
/** Tries to perform an insert to a page in an index tree, next to cursor.
 It is assumed that mtr holds an x-latch on the page. The operation does
 not succeed if there is too little space on the page. If there is just
 one record on the page, the insert will always succeed; this is to
 prevent trying to split a page with just one record.
 @return DB_SUCCESS, DB_WAIT_LOCK, DB_FAIL, or error number */
[[nodiscard]] dberr_t btr_cur_optimistic_insert(
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
    que_thr_t *thr,      /*!< in: query thread or NULL */
    mtr_t *mtr);         /*!< in/out: mini-transaction;
                        if this function returns DB_SUCCESS on
                        a leaf page of a secondary index in a
                        compressed tablespace, the caller must
                        mtr_commit(mtr) before latching
                        any further pages */
/** Performs an insert on a page of an index tree. It is assumed that mtr
 holds an x-latch on the tree and on the cursor page. If the insert is
 made on the leaf level, to avoid deadlocks, mtr must also own x-latches
 to brothers of page, if those brothers exist.
 @return DB_SUCCESS or error number */
[[nodiscard]] dberr_t btr_cur_pessimistic_insert(
    uint32_t flags,      /*!< in: undo logging and locking flags: if not
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
    que_thr_t *thr,      /*!< in: query thread or NULL */
    mtr_t *mtr);         /*!< in/out: mini-transaction */
/** See if there is enough place in the page modification log to log
 an update-in-place.

 @param[in,out] page_zip Compressed page.
 @param[in,out] cursor   B-tree page cursor.
 @param[in]     index    The index corresponding to cursor.
 @param[in,out] offsets  Offsets of the cursor record.
 @param[in]     length   size needed
 @param[in]     create   true=delete-and-insert, false=update-in-place
 @param[in,out] mtr      Mini-transaction.
 @retval false if out of space; IBUF_BITMAP_FREE will be reset
 outside mtr if the page was re-compressed
 @retval true if enough place;

 IMPORTANT: The caller will have to update IBUF_BITMAP_FREE if this is
 a secondary index leaf page. This has to be done either within the
 same mini-transaction, or by invoking ibuf_reset_free_bits() before
 mtr_commit(mtr). */
[[nodiscard]] bool btr_cur_update_alloc_zip_func(
    page_zip_des_t *page_zip, page_cur_t *cursor, dict_index_t *index,
    IF_DEBUG(ulint *offsets, ) ulint length, bool create, mtr_t *mtr);

inline bool btr_cur_update_alloc_zip(page_zip_des_t *page_zip,
                                     page_cur_t *cursor, dict_index_t *index,
                                     ulint *offsets [[maybe_unused]], ulint len,
                                     bool cr, mtr_t *mtr) {
  return btr_cur_update_alloc_zip_func(page_zip, cursor, index,
                                       IF_DEBUG(offsets, ) len, cr, mtr);
}

/** Updates a record when the update causes no size changes in its fields.
@param[in] flags Undo logging and locking flags
@param[in] cursor Cursor on the record to update; cursor stays valid and
positioned on the same record
@param[in,out] offsets Offsets on cursor->page_cur.rec
@param[in] update Update vector
@param[in] cmpl_info Compiler info on secondary index updates
@param[in] thr Query thread, or null if flags & (btr_no_locking_flag |
btr_no_undo_log_flag | btr_create_flag | btr_keep_sys_flag)
@param[in] trx_id Transaction id
@param[in,out] mtr Mini-transaction; if this is a secondary index, the caller
must mtr_commit(mtr) before latching any further pages
@return locking or undo log related error code, or
@retval DB_SUCCESS on success
@retval DB_ZIP_OVERFLOW if there is not enough space left
on the compressed page (IBUF_BITMAP_FREE was reset outside mtr) */
[[nodiscard]] dberr_t btr_cur_update_in_place(ulint flags, btr_cur_t *cursor,
                                              ulint *offsets,
                                              const upd_t *update,
                                              ulint cmpl_info, que_thr_t *thr,
                                              trx_id_t trx_id, mtr_t *mtr);

/** Writes a redo log record of updating a record in-place.
@param[in] flags Undo logging and locking flags
@param[in] rec Record
@param[in] index Index of the record
@param[in] update Update vector
@param[in] trx_id Transaction id
@param[in] roll_ptr Roll ptr
@param[in] mtr Mini-transaction */
void btr_cur_update_in_place_log(ulint flags, const rec_t *rec,
                                 dict_index_t *index, const upd_t *update,
                                 trx_id_t trx_id, roll_ptr_t roll_ptr,
                                 mtr_t *mtr);

/** Tries to update a record on a page in an index tree. It is assumed that mtr
holds an x-latch on the page. The operation does not succeed if there is too
little space on the page or if the update would result in too empty a page,
so that tree compression is recommended. We assume here that the ordering
fields of the record do not change.
@param[in]     flags     undo logging and locking flags
@param[in]     cursor    cursor on the record to update; cursor stays valid and
positioned on the same record
@param[out]    offsets   offsets on cursor->page_cur.rec
@param[in,out] heap      pointer to nullptr or memory heap
@param[in]     update    update vector; this must also contain trx id and roll
ptr fields
@param[in]     cmpl_info compiler info on secondary index updates
@param[in]     thr       query thread, or nullptr if flags &
(BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG | BTR_CREATE_FLAG |
BTR_KEEP_SYS_FLAG)
@param[in]     trx_id    transaction id
@param[in,out] mtr       mini-transaction; if this is a secondary index, the
caller must mtr_commit(mtr) before latching any further pages
@return error code, including
@retval DB_SUCCESS on success
@retval DB_OVERFLOW if the updated record does not fit
@retval DB_UNDERFLOW if the page would become too empty
@retval DB_ZIP_OVERFLOW if there is not enough space left
on the compressed page (IBUF_BITMAP_FREE was reset outside mtr) */
[[nodiscard]] dberr_t btr_cur_optimistic_update(ulint flags, btr_cur_t *cursor,
                                                ulint **offsets,
                                                mem_heap_t **heap,
                                                const upd_t *update,
                                                ulint cmpl_info, que_thr_t *thr,
                                                trx_id_t trx_id, mtr_t *mtr);

/** Performs an update of a record on a page of a tree. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. If the
update is made on the leaf level, to avoid deadlocks, mtr must also
own x-latches to brothers of page, if those brothers exist.
@param[in]     flags         Undo logging, locking, and rollback flags
@param[in,out] cursor        cursor on the record to update;
                             cursor may become invalid if *big_rec == NULL
                             || !(flags & BTR_KEEP_POS_FLAG)
@param[out]    offsets       Offsets on cursor->page_cur.rec
@param[in,out] offsets_heap  Pointer to memory heap that can be emptied,
                             or NULL
@param[in,out] entry_heap    Memory heap for allocating big_rec and the
                             index tuple.
@param[out]    big_rec       Big rec vector whose fields have to be stored
                             externally by the caller, or NULL
@param[in,out] update        Update vector; this is allowed to also contain
                             trx id and roll ptr fields. Non-updated columns
                             that are moved offpage will be appended to
this.
@param[in]     cmpl_info     Compiler info on secondary index updates
@param[in]     thr           Query thread, or NULL if flags &
                             (BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG |
                              BTR_CREATE_FLAG | BTR_KEEP_SYS_FLAG)
@param[in]     trx_id        Transaction id
@param[in]     undo_no       Undo number of the transaction. This is needed
                             for rollback to savepoint of partially updated
LOB.
@param[in,out] mtr           Mini-transaction; must be committed before
latching any further pages
@param[in]     pcur          The persistent cursor on the record to update.
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t btr_cur_pessimistic_update(
    ulint flags, btr_cur_t *cursor, ulint **offsets, mem_heap_t **offsets_heap,
    mem_heap_t *entry_heap, big_rec_t **big_rec, upd_t *update, ulint cmpl_info,
    que_thr_t *thr, trx_id_t trx_id, undo_no_t undo_no, mtr_t *mtr,
    btr_pcur_t *pcur = nullptr);

/** Marks a clustered index record deleted. Writes an undo log record to
 undo log on this delete marking. Writes in the trx id field the id
 of the deleting transaction, and in the roll ptr field pointer to the
 undo log record created.
 @return DB_SUCCESS, DB_LOCK_WAIT, or error number */
[[nodiscard]] dberr_t btr_cur_del_mark_set_clust_rec(
    ulint flags,           /*!< in: undo logging and locking flags */
    buf_block_t *block,    /*!< in/out: buffer block of the record */
    rec_t *rec,            /*!< in/out: record */
    dict_index_t *index,   /*!< in: clustered index of the record */
    const ulint *offsets,  /*!< in: rec_get_offsets(rec) */
    que_thr_t *thr,        /*!< in: query thread */
    const dtuple_t *entry, /*!< in: dtuple for the deleting record */
    mtr_t *mtr);           /*!< in/out: mini-transaction */
/** Sets a secondary index record delete mark to true or false.
 @return DB_SUCCESS, DB_LOCK_WAIT, or error number */
[[nodiscard]] dberr_t btr_cur_del_mark_set_sec_rec(
    ulint flags,       /*!< in: locking flag */
    btr_cur_t *cursor, /*!< in: cursor */
    bool val,          /*!< in: value to set */
    que_thr_t *thr,    /*!< in: query thread */
    mtr_t *mtr);       /*!< in/out: mini-transaction */
/** Tries to compress a page of the tree if it seems useful. It is assumed
 that mtr holds an x-latch on the tree and on the cursor page. To avoid
 deadlocks, mtr must also own x-latches to brothers of page, if those
 brothers exist. NOTE: it is assumed that the caller has reserved enough
 free extents so that the compression will always succeed if done!
 @return true if compression occurred */
bool btr_cur_compress_if_useful(
    btr_cur_t *cursor, /*!< in/out: cursor on the page to compress;
                       cursor does not stay valid if compression
                       occurs */
    bool adjust,       /*!< in: true if should adjust the
                       cursor position even if compression occurs */
    mtr_t *mtr);       /*!< in/out: mini-transaction */

[[nodiscard]] bool btr_cur_optimistic_delete_func(btr_cur_t *cursor,
                                                  IF_DEBUG(ulint flags, )
                                                      mtr_t *mtr);

/** Removes the record on which the tree cursor is positioned on a leaf page.
 It is assumed that the mtr has an x-latch on the page where the cursor is
 positioned, but no latch on the whole tree.
 @param[in] cursor cursor on leaf page, on the record to delete; cursor stays
 valid: if deletion succeeds, on function exit it points to the successor of the
 deleted record
 @param[in] flags BTR_CREATE_FLAG or 0
 @param[in] mtr if this function returns true on a leaf page of a secondary
 index, the mtr must be committed before latching any further pages
 @return true if success, i.e., the page did not become too empty */
inline bool btr_cur_optimistic_delete(btr_cur_t *cursor,
                                      ulint flags [[maybe_unused]],
                                      mtr_t *mtr) {
  return btr_cur_optimistic_delete_func(cursor, IF_DEBUG(flags, ) mtr);
}

/** Removes the record on which the tree cursor is positioned. Tries
 to compress the page if its fillfactor drops below a threshold
 or if it is the only page on the level. It is assumed that mtr holds
 an x-latch on the tree and on the cursor page. To avoid deadlocks,
 mtr must also own x-latches to brothers of page, if those brothers
 exist.
@param[out] err DB_SUCCESS or DB_OUT_OF_FILE_SPACE; the latter may occur
                because we may have to update node pointers on upper
                levels, and in the case of variable length keys these may
                actually grow in size
@param[in] has_reserved_extents true if the caller has already reserved
                                enough free extents so that he knows
                                that the operation will succeed
@param[in] cursor Cursor on the record to delete; if compression does not
                  occur, the cursor stays valid: it points to successor of
                  deleted record on function exit
@param[in] flags  BTR_CREATE_FLAG or 0
@param[in] rollback     True if performing rollback, false otherwise.
@param[in] trx_id       The current transaction id.
@param[in] undo_no      Undo number of the transaction. This is needed for
                        rollback to savepoint of partially updated LOB.
@param[in] rec_type     Undo record type.
@param[in] mtr          The mini transaction
@param[in] pcur         Persistent cursor on the record to delete.
@param[in,out] node     purge node or nullptr
@return true if compression occurred */
bool btr_cur_pessimistic_delete(dberr_t *err, bool has_reserved_extents,
                                btr_cur_t *cursor, uint32_t flags,
                                bool rollback, trx_id_t trx_id,
                                undo_no_t undo_no, ulint rec_type, mtr_t *mtr,
                                btr_pcur_t *pcur, purge_node_t *node);

/** Parses a redo log record of updating a record in-place.
 @return end of log record or NULL */
const byte *btr_cur_parse_update_in_place(
    const byte *ptr,          /*!< in: buffer */
    const byte *end_ptr,      /*!< in: buffer end */
    page_t *page,             /*!< in/out: page or NULL */
    page_zip_des_t *page_zip, /*!< in/out: compressed page, or NULL */
    dict_index_t *index);     /*!< in: index corresponding to page */
/** Parses the redo log record for delete marking or unmarking of a
 clustered index record.
 @return end of log record or NULL */
const byte *btr_cur_parse_del_mark_set_clust_rec(
    const byte *ptr,          /*!< in: buffer */
    const byte *end_ptr,      /*!< in: buffer end */
    page_t *page,             /*!< in/out: page or NULL */
    page_zip_des_t *page_zip, /*!< in/out: compressed page, or NULL */
    dict_index_t *index);     /*!< in: index corresponding to page */
/** Parses the redo log record for delete marking or unmarking of a
 secondary index record.
 @return end of log record or NULL */
const byte *btr_cur_parse_del_mark_set_sec_rec(
    const byte *ptr,           /*!< in: buffer */
    const byte *end_ptr,       /*!< in: buffer end */
    page_t *page,              /*!< in/out: page or NULL */
    page_zip_des_t *page_zip); /*!< in/out: compressed page, or NULL */
#ifndef UNIV_HOTBACKUP

/** Estimates the number of rows in a given index range.
@param[in]      index   index
@param[in]      tuple1  range start, may also be empty tuple
@param[in]      mode1   search mode for range start
@param[in]      tuple2  range end, may also be empty tuple
@param[in]      mode2   search mode for range end
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
@param[in]      trx             the trx doing the operation.
@param[in]      index           index containing the LOB.
@param[in]      rec             record in a clustered index; must be
                                protected by a lock or a page latch
@param[in]      offsets         array returned by rec_get_offsets()
@param[in]      page_size       BLOB page size
@param[in]      no              field number
@param[out]     len             length of the field
@param[out]     lob_version     version of lob
@param[in]      is_sdi          true for SDI Indexes
@param[in,out]  heap            mem heap
@return the field copied to heap, or NULL if the field is incomplete */
byte *btr_rec_copy_externally_stored_field_func(
    trx_t *trx, dict_index_t *index, const rec_t *rec, const ulint *offsets,
    const page_size_t &page_size, ulint no, ulint *len, size_t *lob_version,
    IF_DEBUG(bool is_sdi, ) mem_heap_t *heap);

/** Sets a secondary index record's delete mark to the given value. This
 function is only used by the insert buffer merge mechanism. */
void btr_cur_set_deleted_flag_for_ibuf(
    rec_t *rec,               /*!< in/out: record */
    page_zip_des_t *page_zip, /*!< in/out: compressed page
                              corresponding to rec, or NULL
                              when the tablespace is uncompressed */
    bool val,                 /*!< in: value to set */
    mtr_t *mtr);              /*!< in/out: mini-transaction */

/** The following function is used to set the deleted bit of a record.
@param[in,out]  rec             physical record
@param[in,out]  page_zip        compressed page (or NULL)
@param[in]      flag            nonzero if delete marked */
static inline void btr_rec_set_deleted_flag(rec_t *rec,
                                            page_zip_des_t *page_zip,
                                            bool flag);

/** Latches the leaf page or pages requested.
@param[in]      block           Leaf page where the search converged
@param[in]      page_id         Page id of the leaf
@param[in]      page_size       Page size
@param[in]      latch_mode      BTR_SEARCH_LEAF, ...
@param[in]      cursor          Cursor
@param[in]      mtr             Mini-transaction
@return blocks and savepoints which actually latched. */
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
  records:              (inf, a, b, c, d, sup)
  index of the record:  0, 1, 2, 3, 4, 5
  */

  /** Index of the record where the page cursor stopped on this level
  (index in alphabetical order). Value ULINT_UNDEFINED denotes array
  end. In the above example, if the search stopped on record 'c', then
  nth_rec will be 3. */
  ulint nth_rec{ULINT_UNDEFINED};

  /** Number of the records on the page, not counting inf and sup.
  In the above example n_recs will be 4. */
  ulint n_recs{ULINT_UNDEFINED};

  /** Number of the page containing the record. */
  page_no_t page_no{FIL_NULL};

  /** Level of the page. If later we fetch the page under page_no
  and it is on a different level then we know that the tree has been
  reorganized. */
  ulint page_level{ULINT_UNDEFINED};
};

/** size of path array (in slots) */
constexpr uint32_t BTR_PATH_ARRAY_N_SLOTS = 250;

/** Values for the flag documenting the used search method */
enum btr_cur_method {
  /** Flag for initialization only, not in real use.*/
  BTR_CUR_UNSET = 0,
  /** successful shortcut using the hash index */
  BTR_CUR_HASH = 1,
  /** a search using hash index was not performed. */
  BTR_CUR_HASH_NOT_ATTEMPTED,
  /** failure using hash, success using binary search. The record pointing by
  the cursor may need to be updated in AHI. */
  BTR_CUR_HASH_FAIL,
  /** success using the binary search */
  BTR_CUR_BINARY,
  /** performed the intended insert to the insert buffer */
  BTR_CUR_INSERT_TO_IBUF,
  /** performed the intended delete mark in the insert/delete buffer */
  BTR_CUR_DEL_MARK_IBUF,
  /** performed the intended delete in the insert/delete buffer */
  BTR_CUR_DELETE_IBUF,
  /** row_purge_poss_sec() failed */
  BTR_CUR_DELETE_REF
};

/** The tree cursor: the definition appears here only for the compiler
to know struct size! */
struct btr_cur_t {
  /** Index on which the cursor is positioned. */
  dict_index_t *index{nullptr};
  /** Page cursor. */
  page_cur_t page_cur;
  /** Purge node, for BTR_DELETE */
  purge_node_t *purge_node{nullptr};
  /** this field is used to store a pointer to the left neighbor page, in the
  cases BTR_SEARCH_PREV and BTR_MODIFY_PREV */
  buf_block_t *left_block{nullptr};

  /** this field is only used when btr_cur_search_to_nth_level is called for an
  index entry insertion: the calling query thread is passed here to be used in
  the insert buffer */
  que_thr_t *thr{nullptr};

  /** The following fields are used in
  btr_cur_search_to_nth_level to pass information:
  @{ */
  /** Search method used. */
  btr_cur_method flag{BTR_CUR_UNSET};
  /** Tree height if the search is done for a pessimistic insert or update
  operation. */
  ulint tree_height{0};
  /** If the search mode was PAGE_CUR_LE, the number of matched fields to the
  the first user record to the right of the cursor record after
  btr_cur_search_to_nth_level; for the mode PAGE_CUR_GE, the matched fields to
  the first user record AT THE CURSOR or to the right of it; NOTE that the
  up_match and low_match values may exceed the correct values for comparison to
  the adjacent user record if that record is on a different leaf page! See the
  note in row_ins_duplicate_error_in_clust.  */
  ulint up_match{0};
  /** Number of matched bytes to the right at the time cursor positioned; only
  used internally in searches: not defined after the search. */
  ulint up_bytes{0};
  /** If search mode was PAGE_CUR_LE, the number of matched fields to the first
  user record AT THE CURSOR or to the left of it after
  btr_cur_search_to_nth_level; NOT defined for PAGE_CUR_GE or any other search
  modes; see also the NOTE in up_match! */
  ulint low_match{0};
  /** Number of matched bytes to the left at the time cursor positioned; only
  used internally in searches: not defined after the search. */
  ulint low_bytes{0};
  /* Structure for AHI-related fields used in a cursor. */
  struct {
    /** AHI prefix used in a hash search if flag is any of BTR_CUR_HASH,
    BTR_CUR_HASH_FAIL or BTR_CUR_HASH_NOT_ATTEMPTED. The cursor does
    not fill nor use the `left_side` member and comparisons to other instances
    should be done with equals_without_left_side(). Ideally we could have a
    separate class without this field that btr_search_prefix_info_t inherits or
    composes from, but this would make it larger than 64bits, at least on VC++,
    even if we inherit from a third (empty) class making all these types
    non-POD, and thus unable to do lock-free atomic operations. */
    btr_search_prefix_info_t prefix_info{};
    /** hash value used in the search if flag is any of BTR_CUR_HASH,
    BTR_CUR_HASH_FAIL or BTR_CUR_HASH_NOT_ATTEMPTED. */
    uint64_t ahi_hash_value{0};
  } ahi;
  /** @} */

  /** In estimating the number of rows in range, we store in this array
  information of the path through the tree. */
  btr_path_t *path_arr{nullptr};

  /** rtree search info. */
  rtr_info_t *rtr_info{nullptr};

  /** Ownership of the above rtr_info member. */
  bool m_own_rtr_info = true;

  /** If cursor is used in a scan or simple page fetch. */
  Page_fetch m_fetch_mode{Page_fetch::NORMAL};
};

/** The following function is used to set the deleted bit of a record.
@param[in,out]  rec             physical record
@param[in,out]  page_zip        compressed page (or NULL)
@param[in]      flag            nonzero if delete marked */
static inline void btr_rec_set_deleted_flag(rec_t *rec,
                                            page_zip_des_t *page_zip,
                                            bool flag);

/** If pessimistic delete fails because of lack of file space, there
is still a good change of success a little later.  Try this many
times. */
constexpr uint32_t BTR_CUR_RETRY_DELETE_N_TIMES = 100;
/** If pessimistic delete fails because of lack of file space, there
is still a good change of success a little later.  Sleep this many
milliseconds between retries. */
constexpr uint32_t BTR_CUR_RETRY_SLEEP_TIME_MS = 50;

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

/** If default value of INSTANT ADD column is to be materialize in updated row.
@param[in]  index  record descriptor
@param[in]  rec    record
@return true if instant add column(s) to be materialized. */
bool materialize_instant_default(const dict_index_t *index, const rec_t *rec);

#include "btr0cur.ic"

#endif
