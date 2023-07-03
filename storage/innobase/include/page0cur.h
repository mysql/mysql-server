/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

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

/** @file include/page0cur.h
 The page cursor

 Created 10/4/1994 Heikki Tuuri
 *************************************************************************/

#ifndef page0cur_h
#define page0cur_h

#include "univ.i"

#include "buf0types.h"
#include "data0data.h"
#include "gis0type.h"
#include "mtr0mtr.h"
#include "page0page.h"
#include "rem0rec.h"
#include "rem0wrec.h"

#define PAGE_CUR_ADAPT

#ifdef UNIV_DEBUG
/** Gets pointer to the page frame where the cursor is positioned.
 @return page */
static inline page_t *page_cur_get_page(
    page_cur_t *cur); /*!< in: page cursor */
/** Gets pointer to the buffer block where the cursor is positioned.
 @return page */
static inline buf_block_t *page_cur_get_block(
    page_cur_t *cur); /*!< in: page cursor */
/** Gets pointer to the page frame where the cursor is positioned.
 @return page */
static inline page_zip_des_t *page_cur_get_page_zip(
    page_cur_t *cur); /*!< in: page cursor */
/** Gets the record where the cursor is positioned.
 @return record */
static inline rec_t *page_cur_get_rec(page_cur_t *cur); /*!< in: page cursor */
#else                                                   /* UNIV_DEBUG */
#define page_cur_get_page(cur) page_align((cur)->rec)
#define page_cur_get_block(cur) (cur)->block
#define page_cur_get_page_zip(cur) buf_block_get_page_zip((cur)->block)
#define page_cur_get_rec(cur) (cur)->rec
#endif /* UNIV_DEBUG */

/** Sets the cursor object to point before the first user record on the page.
@param[in]      block   index page
@param[in]      cur     cursor */
static inline void page_cur_set_before_first(const buf_block_t *block,
                                             page_cur_t *cur);

/** Sets the cursor object to point after the last user record on the page.
@param[in]      block   index page
@param[in]      cur     cursor */
static inline void page_cur_set_after_last(const buf_block_t *block,
                                           page_cur_t *cur);

/** Returns true if the cursor is before first user record on page.
 @return true if at start */
static inline bool page_cur_is_before_first(
    const page_cur_t *cur); /*!< in: cursor */
/** Returns true if the cursor is after last user record.
 @return true if at end */
static inline bool page_cur_is_after_last(
    const page_cur_t *cur); /*!< in: cursor */

/** Positions the cursor on the given record.
@param[in]      rec     record on a page
@param[in]      block   buffer block containing the record
@param[out]     cur     page cursor */
static inline void page_cur_position(const rec_t *rec, const buf_block_t *block,
                                     page_cur_t *cur);

/** Moves the cursor to the next record on page. */
static inline void page_cur_move_to_next(
    page_cur_t *cur); /*!< in/out: cursor; must not be after last */
/** Moves the cursor to the previous record on page. */
static inline void page_cur_move_to_prev(
    page_cur_t *cur); /*!< in/out: cursor; not before first */
#ifndef UNIV_HOTBACKUP
/** Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The cursor stays at the
same logical position, but the physical position may change if it is pointing to
a compressed page that was reorganized.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE if this is a
compressed leaf page in a secondary index. This has to be done either within the
same mini-transaction, or by invoking ibuf_reset_free_bits() before
mtr_commit().

@param[in,out] cursor    Page cursor.
@param[in]     tuple     Pointer to a data tuple
@param[in]     index     Index descriptor.
@param[in]      offsets  Offsets on *rec.
@param[in,out] heap      Pointer to memory heap, or to nullptr.
@param[in]     mtr       Mini-transaction handle, or nullptr.
@return pointer to record if succeed, NULL otherwise */
[[nodiscard]] static inline rec_t *page_cur_tuple_insert(
    page_cur_t *cursor, const dtuple_t *tuple, dict_index_t *index,
    ulint **offsets, mem_heap_t **heap, mtr_t *mtr);
#endif /* !UNIV_HOTBACKUP */

/** Inserts a record next to page cursor. Returns pointer to inserted record
if succeed, i.e., enough space available, NULL otherwise. The cursor stays at
the same logical position, but the physical position may change if it is
pointing to a compressed page that was reorganized.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE if this is a
compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction, or by invoking
ibuf_reset_free_bits() before mtr_commit().

@param[in,out]  cursor  A page cursor
@param[in]      rec     record To insert
@param[in]      index   Record descriptor
@param[in,out]  offsets rec_get_offsets(rec, index)
@param[in]      mtr     Mini-transaction handle, or NULL
@return pointer to record if succeed, NULL otherwise */
static inline rec_t *page_cur_rec_insert(page_cur_t *cursor, const rec_t *rec,
                                         dict_index_t *index, ulint *offsets,
                                         mtr_t *mtr);

/** Inserts a record next to page cursor on an uncompressed page.
 Returns pointer to inserted record if succeed, i.e., enough
 space available, NULL otherwise. The cursor stays at the same position.
 @return pointer to record if succeed, NULL otherwise */
[[nodiscard]] rec_t *page_cur_insert_rec_low(
    rec_t *current_rec,  /*!< in: pointer to current record after
                     which the new record is inserted */
    dict_index_t *index, /*!< in: record descriptor */
    const rec_t *rec,    /*!< in: pointer to a physical record */
    ulint *offsets,      /*!< in/out: rec_get_offsets(rec, index) */
    mtr_t *mtr);         /*!< in: mini-transaction handle, or NULL */

/** Inserts a record next to page cursor on an uncompressed page.
@param[in]      current_rec     Pointer to current record after which
                                the new record is inserted.
@param[in]      index           Record descriptor
@param[in]      tuple           Pointer to a data tuple
@param[in]      mtr             Mini-transaction handle, or NULL
@param[in]      rec_size        The size of new record

@return pointer to record if succeed, NULL otherwise */
rec_t *page_cur_direct_insert_rec_low(rec_t *current_rec, dict_index_t *index,
                                      const dtuple_t *tuple, mtr_t *mtr,
                                      ulint rec_size);

/** Inserts a record next to page cursor on a compressed and uncompressed
 page. Returns pointer to inserted record if succeed, i.e.,
 enough space available, NULL otherwise.
 The cursor stays at the same position.

 IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
 if this is a compressed leaf page in a secondary index.
 This has to be done either within the same mini-transaction,
 or by invoking ibuf_reset_free_bits() before mtr_commit().

 @return pointer to record if succeed, NULL otherwise */
[[nodiscard]] rec_t *page_cur_insert_rec_zip(
    page_cur_t *cursor,  /*!< in/out: page cursor */
    dict_index_t *index, /*!< in: record descriptor */
    const rec_t *rec,    /*!< in: pointer to a physical record */
    ulint *offsets,      /*!< in/out: rec_get_offsets(rec, index) */
    mtr_t *mtr);         /*!< in: mini-transaction handle, or NULL */
/** Copies records from page to a newly created page, from a given record
 onward, including that record. Infimum and supremum records are not copied.

 IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
 if this is a compressed leaf page in a secondary index.
 This has to be done either within the same mini-transaction,
 or by invoking ibuf_reset_free_bits() before mtr_commit(). */
void page_copy_rec_list_end_to_created_page(
    page_t *new_page,    /*!< in/out: index page to copy to */
    rec_t *rec,          /*!< in: first record to copy */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr);         /*!< in: mtr */
/** Deletes a record at the page cursor. The cursor is moved to the
 next record after the deleted one. */
void page_cur_delete_rec(
    page_cur_t *cursor,        /*!< in/out: a page cursor */
    const dict_index_t *index, /*!< in: record descriptor */
    const ulint *offsets,      /*!< in: rec_get_offsets(
                               cursor->rec, index) */
    mtr_t *mtr);               /*!< in: mini-transaction handle */
#ifndef UNIV_HOTBACKUP
/** Search the right position for a page cursor.
@param[in] block buffer block
@param[in] index index tree
@param[in] tuple data tuple
@param[in] mode PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G, or PAGE_CUR_GE
@param[out] cursor page cursor
@return number of matched fields on the left */
static inline ulint page_cur_search(const buf_block_t *block,
                                    const dict_index_t *index,
                                    const dtuple_t *tuple, page_cur_mode_t mode,
                                    page_cur_t *cursor);

/** Search the right position for a page cursor.
@param[in] block buffer block
@param[in] index index tree
@param[in] tuple data tuple
@param[out] cursor page cursor
@return number of matched fields on the left */
static inline ulint page_cur_search(const buf_block_t *block,
                                    const dict_index_t *index,
                                    const dtuple_t *tuple, page_cur_t *cursor);
#endif /* !UNIV_HOTBACKUP */

/** Searches the right position for a page cursor.
@param[in] block Buffer block
@param[in] index Record descriptor
@param[in] tuple Data tuple
@param[in] mode PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G, or PAGE_CUR_GE
@param[in,out] iup_matched_fields Already matched fields in upper limit record
@param[in,out] ilow_matched_fields Already matched fields in lower limit record
@param[out] cursor Page cursor
@param[in,out] rtr_info Rtree search stack */
void page_cur_search_with_match(const buf_block_t *block,
                                const dict_index_t *index,
                                const dtuple_t *tuple, page_cur_mode_t mode,
                                ulint *iup_matched_fields,

                                ulint *ilow_matched_fields,

                                page_cur_t *cursor, rtr_info_t *rtr_info);

/** Search the right position for a page cursor.
@param[in]      block                   buffer block
@param[in]      index                   index tree
@param[in]      tuple                   key to be searched for
@param[in]      mode                    search mode
@param[in,out]  iup_matched_fields      already matched fields in the
upper limit record
@param[in,out]  iup_matched_bytes       already matched bytes in the
first partially matched field in the upper limit record
@param[in,out]  ilow_matched_fields     already matched fields in the
lower limit record
@param[in,out]  ilow_matched_bytes      already matched bytes in the
first partially matched field in the lower limit record
@param[out]     cursor                  page cursor */
void page_cur_search_with_match_bytes(
    const buf_block_t *block, const dict_index_t *index, const dtuple_t *tuple,
    page_cur_mode_t mode, ulint *iup_matched_fields, ulint *iup_matched_bytes,
    ulint *ilow_matched_fields, ulint *ilow_matched_bytes, page_cur_t *cursor);
/** Positions a page cursor on a randomly chosen user record on a page. If there
 are no user records, sets the cursor on the infimum record. */
void page_cur_open_on_rnd_user_rec(buf_block_t *block,  /*!< in: page */
                                   page_cur_t *cursor); /*!< out: page cursor */
/** Parses a log record of a record insert on a page.
 @return end of log record or NULL */
byte *page_cur_parse_insert_rec(
    bool is_short,       /*!< in: true if short inserts */
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    buf_block_t *block,  /*!< in: page or NULL */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr);         /*!< in: mtr or NULL */
/** Parses a log record of copying a record list end to a new created page.
 @return end of log record or NULL */
byte *page_parse_copy_rec_list_to_created_page(
    byte *ptr,           /*!< in: buffer */
    byte *end_ptr,       /*!< in: buffer end */
    buf_block_t *block,  /*!< in: page or NULL */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr);         /*!< in: mtr or NULL */
/** Parses log record of a record delete on a page.
 @return pointer to record end or NULL */
byte *page_cur_parse_delete_rec(
    byte *ptr,           /*!< in: buffer */
    byte *end_ptr,       /*!< in: buffer end */
    buf_block_t *block,  /*!< in: page or NULL */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr);         /*!< in: mtr or NULL */
/** Removes the record from a leaf page. This function does not log
any changes. It is used by the IMPORT tablespace functions.
@return true if success, i.e., the page did not become too empty
@param[in] index The index that the record belongs to.
@param[in,out] pcur Page cursor on record to delete.
@param[in] offsets Offsets for record. */
bool page_delete_rec(const dict_index_t *index, page_cur_t *pcur,
                     const ulint *offsets);

/** Index page cursor */

struct page_cur_t {
  /** Index the cursor is on. */
  const dict_index_t *index{nullptr};

  /** pointer to a record on page */
  rec_t *rec{nullptr};

  /** Current offsets of the record. */
  ulint *offsets{nullptr};

  /** Pointer to the current block containing rec. */
  buf_block_t *block{nullptr};
};

#include "page0cur.ic"

#endif
