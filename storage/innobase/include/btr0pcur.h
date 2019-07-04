/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/btr0pcur.h
 The index tree persistent cursor

 Created 2/23/1996 Heikki Tuuri
 *******************************************************/

#ifndef btr0pcur_h
#define btr0pcur_h

#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0types.h"
#include "data0data.h"
#include "dict0dict.h"
#include "mtr0mtr.h"
#include "page0cur.h"
#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "gis0rtree.h"
#endif /* UNIV_HOTBACKUP */

/** Relative positions for a stored cursor position */
enum btr_pcur_pos_t {
  BTR_PCUR_UNSET = 0,
  BTR_PCUR_ON = 1,
  BTR_PCUR_BEFORE = 2,
  BTR_PCUR_AFTER = 3,
  /* Note that if the tree is not empty, btr_pcur_store_position does
  not use the following, but only uses the above three alternatives,
  where the position is stored relative to a specific record: this makes
  implementation of a scroll cursor easier */
  BTR_PCUR_BEFORE_FIRST_IN_TREE = 4, /* in an empty tree */
  BTR_PCUR_AFTER_LAST_IN_TREE = 5    /* in an empty tree */
};

#define btr_pcur_create_for_mysql() btr_pcur_t::create_for_mysql()
#define btr_pcur_free_for_mysql(p) btr_pcur_t::free_for_mysql(p)

#define btr_pcur_reset(p) (p)->reset();

#define btr_pcur_copy_stored_position(d, s) \
  btr_pcur_t::copy_stored_position(d, s)

#define btr_pcur_free(p) (p)->free_rec_buf()

#define btr_pcur_open(i, t, md, l, p, m) \
  (p)->open((i), 0, (t), (md), (l), m, __FILE__, __LINE__)

#define btr_pcur_init(p) (p)->init()
#define btr_pcur_close(p) (p)->close()

#define btr_pcur_open_at_rnd_pos(i, l, p, m) \
  (p)->set_random_position((i), (l), (m), __FILE__, __LINE__)

#define btr_pcur_open_low(i, lv, md, lm, p, f, ln, mr) \
  (p)->open((i), (lv), (t), (md), (lm), (mr), (f), (l))

#define btr_pcur_open_at_index_side(e, i, lm, p, ip, lv, m) \
  (p)->open_at_side((e), (i), (lm), (ip), (lv), (m))

#define btr_pcur_open_on_user_rec(i, t, md, l, p, m) \
  (p)->open_on_user_rec((i), (t), (md), (l), (m), __FILE__, __LINE__)

#define btr_pcur_open_with_no_init(i, t, md, l, p, has, m) \
  (p)->open_no_init((i), (t), (md), (l), (has), (m), __FILE__, __LINE__)

#define btr_pcur_restore_position(l, p, mtr) \
  (p)->restore_position(l, mtr, __FILE__, __LINE__)

#define btr_pcur_store_position(p, m) (p)->store_position(m)

#define btr_pcur_get_rel_pos(p) (p)->get_rel_pos()

#define btr_pcur_commit_specify_mtr(p, m) (p)->commit_specify_mtr(m)

#define btr_pcur_move_to_next(p, m) (p)->move_to_next(m)
#define btr_pcur_move_to_prev(p, m) (p)->move_to_prev(m)

#define btr_pcur_move_to_last_on_page(p, m) (p)->move_to_last_on_page(m)

#define btr_pcur_move_to_next_user_rec(p, m) \
  ((p)->move_to_next_user_rec(m) == DB_SUCCESS)

#define btr_pcur_move_to_next_page(p, m) (p)->move_to_next_page(m)

#define btr_pcur_get_btr_cur(p) (p)->get_btr_cur()

#define btr_pcur_get_page_cur(p) (p)->get_page_cur()

#define btr_pcur_get_page(p) (p)->get_page()

#define btr_pcur_get_block(p) (p)->get_block()

#define btr_pcur_get_rec(p) (p)->get_rec()

#define btr_pcur_is_on_user_rec(p) (p)->is_on_user_rec()

#define btr_pcur_is_after_last_on_page(p) (p)->is_after_last_on_page()

#define btr_pcur_is_before_first_on_page(p) (p)->is_before_first_on_page()

#define btr_pcur_is_before_first_in_tree(p, m) (p)->is_before_first_in_tree(m)

#define btr_pcur_is_after_last_in_tree(p, m) (p)->is_after_last_in_tree(m)

#define btr_pcur_move_to_next_on_page(p) (p)->move_to_next_on_page()

#define btr_pcur_move_to_prev_on_page(p) (p)->move_to_prev_on_page()

#define btr_pcur_move_before_first_on_page(p) (p)->move_before_first_on_page()

#define btr_pcur_get_low_match(p) (p)->get_low_match()

#define btr_pcur_get_up_match(p) (p)->get_up_match()

/** Position state of persistent B-tree cursor. */
enum pcur_pos_t {

  /** The persistent cursor is not positioned. */
  BTR_PCUR_NOT_POSITIONED = 0,

  /** The persistent cursor was previously positioned.
  TODO: currently, the state can be BTR_PCUR_IS_POSITIONED,
  though it really should be BTR_PCUR_WAS_POSITIONED,
  because we have no obligation to commit the cursor with
  mtr; similarly latch_mode may be out of date. This can
  lead to problems if btr_pcur is not used the right way;
  all current code should be ok. */
  BTR_PCUR_WAS_POSITIONED,

  /** The persistent cursor is positioned by optimistic get to the same
  record as it was positioned at. Not used for rel_pos == BTR_PCUR_ON.
  It may need adjustment depending on previous/current search direction
  and rel_pos. */
  BTR_PCUR_IS_POSITIONED_OPTIMISTIC,

  /** The persistent cursor is positioned by index search.
  Or optimistic get for rel_pos == BTR_PCUR_ON. */
  BTR_PCUR_IS_POSITIONED
};

/* The persistent B-tree cursor structure. This is used mainly for SQL
selects, updates, and deletes. */

struct btr_pcur_t {
  /** Sets the old_rec_buf field to nullptr. */
  void init();

  /** @return the index of this persistent cursor */
  dict_index_t *index() { return (m_btr_cur.index); }

  /** Positions a cursor at a randomly chosen position within a B-tree.
  @param[in]	    index		    Index to position on.
  @param[in]	    latch_mode	BTR_SEARCH_LEAF, ...
  @param[in,out]	mtr		      Mini transaction.
  @param[in]	    file	  File name from where called.
  @param[in]	    line		    Line number within filename
  @return true if the index is available and we have put the cursor, false
          if the index is unavailable */
  bool set_random_position(dict_index_t *index, ulint latch_mode, mtr_t *mtr,
                           const char *file, ulint line);

  /** Opens a persistent cursor at either end of an index.
  @param[in]	    from_left   true if open to the low end, false
                              if to the high end.
  @param[in]	    index		    index
  @param[in]	    latch_mode	latch mode
  @param[in]	    init_pcur	  whether to initialize pcur.
  @param[in]	    level		    level to search for (0=leaf).
  @param[in,out]	mtr		      mini-transaction */
  void open_at_side(bool from_left, dict_index_t *index, ulint latch_mode,
                    bool init_pcur, ulint level, mtr_t *mtr);

  /** Opens a persistent cursor at first leaf page (low end). It will not call
  init().
  @param[in]	    index		    index
  @param[in]	    latch_mode	latch mode
  @param[in,out]	mtr		      mini-transaction */
  void begin_leaf(dict_index_t *index, ulint latch_mode, mtr_t *mtr) {
    open_at_side(true, index, latch_mode, false, 0, mtr);
  }

  /** Opens an persistent cursor to an index tree without initializing
  the cursor.
  @param[in]	    index	      Index.
  @param[in]	    tuple	      Tuple on which search done.
  @param[in]	    mode	      PAGE_CUR_L, ...;
                              NOTE that if the search is made using a unique
                              prefix of a record, mode should be
                              PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
                              may end up on the previous page of the
                              record!
  @param[in]	    latch_mode	BTR_SEARCH_LEAF, ...;
                              NOTE that if has_search_latch != 0 then
                              we maybe do not acquire a latch on the cursor
                              page, but assume that the caller uses his
                              btr search latch to protect the record!
  @param[in]	    has_search_latch	latch mode the caller
                              currently has on search system: RW_S_LATCH, or 0
  @param[in]	    mtr	        Mtr
  @param[in]	    file	      File name.
  @param[in]	    line	      Line where called */
  void open_no_init(dict_index_t *index, const dtuple_t *tuple,
                    page_cur_mode_t mode, ulint latch_mode,
                    ulint has_search_latch, mtr_t *mtr, const char *file,
                    ulint line);

  /** If mode is PAGE_CUR_G or PAGE_CUR_GE, opens a persistent cursor
  on the first user record satisfying the search condition, in the case
  PAGE_CUR_L or PAGE_CUR_LE, on the last user record. If no such user
  record exists, then in the first case sets the cursor after last in
  tree, and in the latter case before first in tree. The latching mode
  must be BTR_SEARCH_LEAF or BTR_MODIFY_LEAF.
  @param[in]	    index		    Index
  @param[in]	    tuple		  Tuple on which search done.
  @param[in]	    mode		    PAGE_CUR_L, ...
  @param[in]	    latch_mode	BTR_SEARCH_LEAF or BTR_MODIFY_LEAF
  @param[in]	    mtr		      Mini transaction.
  @param[in]	    file		    File name from where called.
  @param[in]	    line		    Line number in file from where
                              called.*/
  void open_on_user_rec(dict_index_t *index, const dtuple_t *tuple,
                        page_cur_mode_t mode, ulint latch_mode, mtr_t *mtr,
                        const char *file, ulint line);

  /** Initializes and opens a persistent cursor to an index tree
  It should be closed with close().
  @param[in]	    index		    Index.
  @param[in]	    level		    Level in the btree.
  @param[in]	    tuple		    Tuple on which search done.
  @param[in]	    mode		    PAGE_CUR_L, ...;
                              NOTE that if the search is made using
                              a unique prefix of a record, mode
                              should be PAGE_CUR_LE, not PAGE_CUR_GE,
                              as the latter may end up on the
                              previous page from the record!
  @param[in]	    latch_mode	BTR_SEARCH_LEAF, ...
  @param[in]	    mtr		      Mini-transaction.
  @param[in]	    file		    File name
  @param[in]	    line		    Line in file, from where called. */
  void open(dict_index_t *index, ulint level, const dtuple_t *tuple,
            page_cur_mode_t mode, ulint latch_mode, mtr_t *mtr,
            const char *file, ulint line);

  /** Restores the stored position of a persistent cursor bufferfixing
  the page and obtaining the specified latches. If the cursor position
  was saved when the
  (1) cursor was positioned on a user record: this function restores
  the position to the last record LESS OR EQUAL to the stored record;
  (2) cursor was positioned on a page infimum record: restores the
  position to the last record LESS than the user record which was the
  successor of the page infimum;
  (3) cursor was positioned on the page supremum: restores to the first
  record GREATER than the user record which was the predecessor of the
  supremum.
  (4) cursor was positioned before the first or after the last in an
  empty tree: restores to before first or after the last in the tree.
  @param[in]	    latch_mode	BTR_SEARCH_LEAF, ...
  @param[in,out]  mtr		      Mini transaction
  @param[in]	    file		    File name.
  @param[in]	    line		    Line where called.
  @return true if the cursor position was stored when it was on a user
          record and it can be restored on a user record whose ordering
          fields are identical to the ones of the original user record */
  bool restore_position(ulint latch_mode, mtr_t *mtr, const char *file,
                        ulint line);

  /** Frees the possible memory heap of a persistent cursor and
  sets the latch mode of the persistent cursor to BTR_NO_LATCHES.
  WARNING: this function does not release the latch on the page where the
  cursor is currently positioned. The latch is acquired by the
  "move to next/previous" family of functions. Since recursive shared
  locks are not allowed, you must take care (if using the cursor in
  S-mode) to manually release the latch by either calling
  btr_leaf_page_release(btr_pcur_get_block(&pcur), pcur.latch_mode, mtr)
  or by committing the mini-transaction right after btr_pcur_close().
  A subsequent attempt to crawl the same page in the same mtr would
  cause an assertion failure. */
  void close();

  /** Free old_rec_buf. */
  void free_rec_buf() {
    ut_free(m_old_rec_buf);
    m_old_rec_buf = nullptr;
  }

#ifndef UNIV_HOTBACKUP

  /** Gets the rel_pos field for a cursor whose position has been stored.
  @return BTR_PCUR_ON, ... */
  ulint get_rel_pos() const;

#endif /* !UNIV_HOTBACKUP */

  /** @return the btree cursor (const version). */
  const btr_cur_t *get_btr_cur() const;

  /** @return the btree cursor (non const version). */
  btr_cur_t *get_btr_cur();

  /** @return the btree page cursor (non const version). */
  page_cur_t *get_page_cur();

  /** @return the btree cursor (const version). */
  const page_cur_t *get_page_cur() const;

  /** Returns the page of a persistent pcur (non const version).
  @return pointer to the page */
  page_t *get_page();

  /** Returns the page of a persistent pcur (const version).
  @return pointer to the page */
  const page_t *get_page() const;

  /** Returns the current buffer block (non const version).
  @return pointer to the block */
  buf_block_t *get_block();

  /** Returns the current buffer block (const version).
  @return pointer to the block */
  const buf_block_t *get_block() const;

  /** Returns the current record (non const version).
  @return pointer to the record */
  rec_t *get_rec();

  /** Returns the current record (const version).
  @return pointer to the record */
  const rec_t *get_rec() const;

#ifndef UNIV_HOTBACKUP
  /** Gets the up_match value for a pcur after a search.
  @return number of matched fields at the cursor or to the right if
  search mode was PAGE_CUR_GE, otherwise undefined */
  ulint get_up_match() const;

  /** Gets the low_match value for a pcur after a search.
  @return number of matched fields at the cursor or to the right if
  search mode was PAGE_CUR_LE, otherwise undefined */
  ulint get_low_match() const;

  /** Checks if the persistent cursor is after the last user record
  on a page.
  @return true if after last on page. */
  bool is_after_last_on_page() const;

  /** Checks if the persistent cursor is before the first user record
  on a page.
  @return true if before first on page. */
  bool is_before_first_on_page() const;

  /** Checks if the persistent cursor is on a user record.
  @return true if on user record. */
  bool is_on_user_rec() const;

  /** Checks if the persistent cursor is before the first user record
  in the index tree.
  @param[in,out]	mtr		      Mini transaction.
  @return true if is before first in tree. */
  bool is_before_first_in_tree(mtr_t *mtr) const;

  /** Checks if the persistent cursor is after the last user record in
  the index tree.
  @param[in,out]	mtr		      Mini transaction.
  @return is after last in tree. */
  bool is_after_last_in_tree(mtr_t *mtr) const;

  /** Moves the persistent cursor to the next record on the same page. */
  void move_to_next_on_page();

  /** Moves the persistent cursor to the prev record on the same page. */
  void move_to_prev_on_page();

  /** Moves the persistent cursor to the last record on the same page.
  @param[in,out]	mtr		      Mini transaction. */
  void move_to_last_on_page(mtr_t *mtr);

  /** Moves the persistent cursor to the next user record in the tree.
  If no user records are left, the cursor ends up 'after last in tree'.
  @param[in,out]	mtr		      Mini transaction.
  @return DB_SUCCESS or DB_END_OF_INDEX. */
  dberr_t move_to_next_user_rec(mtr_t *mtr);

  /** Moves the persistent cursor to the next record in the tree. If no
  records are left, the cursor stays 'after last in tree'.
  Note: Function may release the page latch.
  @param[in,out]	mtr		      Mini transaction.
  @return true if the cursor was not after last in tree */
  bool move_to_next(mtr_t *mtr);

  /** Moves the persistent cursor to the previous record in the tree.
  If no records are left, the cursor stays 'before first in tree'.
  Note: Function may release the page latch.
  @param[in,out]	mtr		      Mini transaction.
  @return true if the cursor was not before first in tree */
  bool move_to_prev(mtr_t *mtr);

  /** Moves the persistent cursor to the first record on the next page.
  Releases the latch on the current page, and bufferunfixes it.
  Note that there must not be modifications on the current page, as
  then the x-latch can be released only in mtr_commit.
  @param[in,out] mtr          Mini transaction. */
  void move_to_next_page(mtr_t *mtr);

  /** Commits the mtr and sets the pcur latch mode to BTR_NO_LATCHES,
  that is, the cursor becomes detached.
  Function btr_pcur_store_position should be used before calling this,
  if restoration of cursor is wanted later.
  @param[in,out]	mtr		      Mini transaction. */
  void commit_specify_mtr(mtr_t *mtr);

  /** Moves the persistent cursor to the infimum record on the same page. */
  void move_before_first_on_page();
#endif /* !UNIV_HOTBACKUP */

  /** The position of the cursor is stored by taking an initial segment
  of the record the cursor is positioned on, before, or after, and
  copying it to the cursor data structure, or just setting a flag if
  the cursor id before the first in an EMPTY tree, or after the last
  in an EMPTY tree. NOTE that the page where the cursor is positioned
  must not be empty if the index tree is not totally empty!
  @param[in,out]	mtr		      Mini-transaction. */
  void store_position(mtr_t *mtr);

  /** @return true if the cursor is positioned. */
  bool is_positioned() const {
    return (m_old_stored && (m_pos_state == BTR_PCUR_IS_POSITIONED ||
                             m_pos_state == BTR_PCUR_WAS_POSITIONED));
  }

  /** @return true if the cursor is for a clustered index. */
  bool is_clustered() const { return (m_btr_cur.index->is_clustered()); }

  /** Resets a persistent cursor object, freeing "::old_rec_buf" if it is
  allocated and resetting the other members to their initial values. */
  void reset();

  /** Copies the stored position of a pcur to another pcur.
  @param[in,out]	dst		      Which will receive the position
  info.
  @param[in]	    src		      From which the info is copied */
  static void copy_stored_position(btr_pcur_t *dst, const btr_pcur_t *src);

  /** Allocates memory for a persistent cursor object and initializes
  the cursor.
  @return own: persistent cursor */
  static btr_pcur_t *create_for_mysql() {
    auto pcur = UT_NEW_NOKEY(btr_pcur_t());

    pcur->m_btr_cur.index = nullptr;

    pcur->init();

    return (pcur);
  }

  /** Frees the memory for a persistent cursor object and the cursor itself.
  @param[in,out]  pcur      Cursor to free. */
  static void free_for_mysql(btr_pcur_t *&pcur) {
    pcur->free_rec_buf();

    UT_DELETE(pcur);

    pcur = nullptr;
  }

  /** Set the cursor access type: Normal or Scan.
  @param[in]  fetch_mode      One of Page_fetch::NORMAL or Page_fetch::SCAN.
  @return the old fetch mode. */
  Page_fetch set_fetch_type(Page_fetch fetch_mode) {
    ut_ad(fetch_mode == Page_fetch::NORMAL || fetch_mode == Page_fetch::SCAN);

    auto old_fetch_mode = m_btr_cur.m_fetch_mode;

    m_btr_cur.m_fetch_mode = fetch_mode;

    return (old_fetch_mode);
  }

 private:
  /** Moves the persistent cursor backward if it is on the first record
  of the page. Commits mtr. Note that to prevent a possible deadlock, the
  operation first stores the position of the cursor, commits mtr, acquires
  the necessary latches and restores the cursor position again before
  returning. The alphabetical position of the cursor is guaranteed to
  be sensible on return, but it may happen that the cursor is not
  positioned on the last record of any page, because the structure
  of the tree may have changed during the time when the cursor had
  no latches.
  @param[in,out]	mtr		      Mini-tranaction. */
  void move_backward_from_page(mtr_t *mtr);

 public:
  /** a B-tree cursor */
  btr_cur_t m_btr_cur;

  /** see TODO note below!
  BTR_SEARCH_LEAF, BTR_MODIFY_LEAF, BTR_MODIFY_TREE or BTR_NO_LATCHES,
  depending on the latching state of the page and tree where the cursor
  is positioned; BTR_NO_LATCHES means that the cursor is not currently
  positioned:
  we say then that the cursor is detached; it can be restored to
  attached if the old position was stored in old_rec */
  ulint m_latch_mode{0};

  /** true if old_rec is stored */
  bool m_old_stored{false};

  /** if cursor position is stored, contains an initial segment of the
  latest record cursor was positioned either on, before or after */
  rec_t *m_old_rec{nullptr};

  /** number of fields in old_rec */
  ulint m_old_n_fields{0};

  /** BTR_PCUR_ON, BTR_PCUR_BEFORE, or BTR_PCUR_AFTER, depending on
  whether cursor was on, before, or after the old_rec record */
  btr_pcur_pos_t m_rel_pos{BTR_PCUR_UNSET};

  /** buffer block when the position was stored */
  buf_block_t *m_block_when_stored{nullptr};

  /** the modify clock value of the buffer block when the cursor position
  was stored */
  ib_uint64_t m_modify_clock{0};

  /** the withdraw clock value of the buffer pool when the cursor
  position was stored */
  ulint m_withdraw_clock{0};

  /** position() and restore_position() state. */
  pcur_pos_t m_pos_state{BTR_PCUR_NOT_POSITIONED};

  /** PAGE_CUR_G, ... */
  page_cur_mode_t m_search_mode{PAGE_CUR_UNSUPP};

  /** the transaction, if we know it; otherwise this field is not defined;
  can ONLY BE USED in error prints in fatal assertion failures! */
  trx_t *m_trx_if_known{nullptr};

  /* NOTE that the following fields may possess dynamically allocated
  memory which should be freed if not needed anymore! */

  /** nullptr, or a dynamically allocated buffer for old_rec */
  byte *m_old_rec_buf{nullptr};

  /** old_rec_buf size if old_rec_buf is not nullptr */
  size_t m_buf_size{0};
};

inline void btr_pcur_t::init() {
  set_fetch_type(Page_fetch::NORMAL);

  m_old_stored = false;
  m_old_rec_buf = nullptr;
  m_old_rec = nullptr;
  m_btr_cur.rtr_info = nullptr;
}

/** Initializes and opens a persistent cursor to an index tree
It should be closed with btr_pcur_close.
@param[in]	  index		        Index.
@param[in]	  level		        Level in the btree.
@param[in]	  tuple		        Tuple on which search done.
@param[in]	  mode		        PAGE_CUR_L, ...; NOTE that if
the search is made using a unique prefix of a record, mode should be
PAGE_CUR_LE, not PAGE_CUR_GE, as the latter may end up on the previous page from
the record!
@param[in]	  latch_mode	    BTR_SEARCH_LEAF, ...
@param[in]	  mtr		          Mini-transaction.
@param[in]	  file		        File name
@param[in]	  line		        Line in file, from where called. */
inline void btr_pcur_t::open(dict_index_t *index, ulint level,
                             const dtuple_t *tuple, page_cur_mode_t mode,
                             ulint latch_mode, mtr_t *mtr, const char *file,
                             ulint line) {
  init();

  m_search_mode = mode;
  m_latch_mode = BTR_LATCH_MODE_WITHOUT_FLAGS(latch_mode);

  /* Search with the tree cursor */

  auto cur = get_btr_cur();

  ut_ad(!dict_index_is_spatial(index));

  if (index->table->is_intrinsic()) {
    ut_ad((latch_mode & BTR_MODIFY_LEAF) || (latch_mode & BTR_SEARCH_LEAF) ||
          (latch_mode & BTR_MODIFY_TREE));

    btr_cur_search_to_nth_level_with_no_latch(
        index, level, tuple, mode, cur, file, line, mtr,
        (((latch_mode & BTR_MODIFY_LEAF) || (latch_mode & BTR_MODIFY_TREE))
             ? true
             : false));
  } else {
    btr_cur_search_to_nth_level(index, level, tuple, mode, latch_mode, cur, 0,
                                file, line, mtr);
  }

  m_pos_state = BTR_PCUR_IS_POSITIONED;

  m_trx_if_known = nullptr;
}

inline void btr_pcur_t::open_at_side(bool from_left, dict_index_t *index,
                                     ulint latch_mode, bool init_pcur,
                                     ulint level, mtr_t *mtr) {
  m_latch_mode = BTR_LATCH_MODE_WITHOUT_FLAGS(latch_mode);

  m_search_mode = from_left ? PAGE_CUR_G : PAGE_CUR_L;

  if (init_pcur) {
    init();
  }

  if (index->table->is_intrinsic()) {
    btr_cur_open_at_index_side_with_no_latch(from_left, index, get_btr_cur(),
                                             level, mtr);
  } else {
    btr_cur_open_at_index_side(from_left, index, latch_mode, get_btr_cur(),
                               level, mtr);
  }

  m_pos_state = BTR_PCUR_IS_POSITIONED;

  m_old_stored = false;

  m_trx_if_known = nullptr;
}

inline bool btr_pcur_t::set_random_position(dict_index_t *index,
                                            ulint latch_mode, mtr_t *mtr,
                                            const char *file, ulint line) {
  m_latch_mode = latch_mode;
  m_search_mode = PAGE_CUR_G;

  init();

  auto positioned = btr_cur_open_at_rnd_pos_func(
      index, latch_mode, get_btr_cur(), file, line, mtr);

  m_old_stored = false;

  m_trx_if_known = nullptr;

  m_pos_state = BTR_PCUR_IS_POSITIONED;

  return (positioned);
}

inline void btr_pcur_t::open_no_init(dict_index_t *index, const dtuple_t *tuple,
                                     page_cur_mode_t mode, ulint latch_mode,
                                     ulint has_search_latch, mtr_t *mtr,
                                     const char *file, ulint line) {
  m_latch_mode = BTR_LATCH_MODE_WITHOUT_INTENTION(latch_mode);

  m_search_mode = mode;

  /* Search with the tree cursor */

  auto cur = get_btr_cur();

  if (index->table->is_intrinsic()) {
    ut_ad((latch_mode & BTR_MODIFY_LEAF) || (latch_mode & BTR_SEARCH_LEAF));

    btr_cur_search_to_nth_level_with_no_latch(
        index, 0, tuple, mode, cur, file, line, mtr,
        ((latch_mode & BTR_MODIFY_LEAF) ? true : false));
  } else {
    btr_cur_search_to_nth_level(index, 0, tuple, mode, latch_mode, cur,
                                has_search_latch, file, line, mtr);
  }

  m_pos_state = BTR_PCUR_IS_POSITIONED;

  m_old_stored = false;

  m_trx_if_known = nullptr;
}

inline const btr_cur_t *btr_pcur_t::get_btr_cur() const { return (&m_btr_cur); }

inline btr_cur_t *btr_pcur_t::get_btr_cur() {
  return (const_cast<btr_cur_t *>(&m_btr_cur));
}

#ifdef UNIV_DEBUG
inline page_cur_t *btr_pcur_t::get_page_cur() {
  return (btr_cur_get_page_cur(get_btr_cur()));
}

inline const page_cur_t *btr_pcur_t::get_page_cur() const {
  return (btr_cur_get_page_cur(get_btr_cur()));
}

inline page_t *btr_pcur_t::get_page() {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);

  return (btr_cur_get_page(get_btr_cur()));
}

inline const page_t *btr_pcur_t::get_page() const {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);

  return (btr_cur_get_page(const_cast<btr_pcur_t *>(this)->get_btr_cur()));
}

inline buf_block_t *btr_pcur_t::get_block() {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);

  return (btr_cur_get_block(get_btr_cur()));
}

inline const buf_block_t *btr_pcur_t::get_block() const {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);

  return (btr_cur_get_block(get_btr_cur()));
}

inline rec_t *btr_pcur_t::get_rec() {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  return (btr_cur_get_rec(get_btr_cur()));
}

inline const rec_t *btr_pcur_t::get_rec() const {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  return (btr_cur_get_rec(get_btr_cur()));
}
#else

inline page_cur_t *btr_pcur_t::get_page_cur() { return (&m_btr_cur.page_cur); }

inline const page_cur_t *btr_pcur_t::get_page_cur() const {
  return (&m_btr_cur.page_cur);
}

inline page_t *btr_pcur_t::get_page() {
  return (m_btr_cur.page_cur.block->frame);
}

inline const page_t *btr_pcur_t::get_page() const {
  return (m_btr_cur.page_cur.block->frame);
}

inline buf_block_t *btr_pcur_t::get_block() {
  return (m_btr_cur.page_cur.block);
}

inline const buf_block_t *btr_pcur_t::get_block() const {
  return (m_btr_cur.page_cur.block);
}

inline rec_t *btr_pcur_t::get_rec() { return (m_btr_cur.page_cur.rec); }

inline const rec_t *btr_pcur_t::get_rec() const {
  return (m_btr_cur.page_cur.rec);
}

#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP

inline ulint btr_pcur_t::get_rel_pos() const {
  ut_ad(m_old_rec != nullptr);
  ut_ad(m_old_stored);
  ut_ad(m_pos_state == BTR_PCUR_WAS_POSITIONED ||
        m_pos_state == BTR_PCUR_IS_POSITIONED);

  return (m_rel_pos);
}

inline void btr_pcur_t::close() {
  free_rec_buf();

  if (m_btr_cur.rtr_info != nullptr) {
    rtr_clean_rtr_info(m_btr_cur.rtr_info, true);
    m_btr_cur.rtr_info = nullptr;
  }

  m_old_rec = nullptr;
  m_btr_cur.page_cur.rec = nullptr;
  m_btr_cur.page_cur.block = nullptr;

  m_old_rec = nullptr;
  m_old_stored = false;

  m_latch_mode = BTR_NO_LATCHES;
  m_pos_state = BTR_PCUR_NOT_POSITIONED;

  m_trx_if_known = nullptr;
}

inline ulint btr_pcur_t::get_up_match() const {
  ut_ad(m_pos_state == BTR_PCUR_WAS_POSITIONED ||
        m_pos_state == BTR_PCUR_IS_POSITIONED);

  const auto cur = get_btr_cur();

  ut_ad(cur->up_match != ULINT_UNDEFINED);

  return (cur->up_match);
}

inline ulint btr_pcur_t::get_low_match() const {
  ut_ad(m_pos_state == BTR_PCUR_WAS_POSITIONED ||
        m_pos_state == BTR_PCUR_IS_POSITIONED);

  const auto cur = get_btr_cur();

  ut_ad(cur->low_match != ULINT_UNDEFINED);

  return (cur->low_match);
}

inline bool btr_pcur_t::is_after_last_on_page() const {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  return (page_cur_is_after_last(get_page_cur()));
}

inline bool btr_pcur_t::is_before_first_on_page() const {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  return (page_cur_is_before_first(get_page_cur()));
}

inline bool btr_pcur_t::is_on_user_rec() const {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  return (!is_before_first_on_page() && !is_after_last_on_page());
}

inline bool btr_pcur_t::is_before_first_in_tree(mtr_t *mtr) const {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  if (btr_page_get_prev(get_page(), mtr) != FIL_NULL) {
    return (false);
  }

  return (page_cur_is_before_first(get_page_cur()));
}

inline bool btr_pcur_t::is_after_last_in_tree(mtr_t *mtr) const {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  if (btr_page_get_next(get_page(), mtr) != FIL_NULL) {
    return (false);
  }

  return (page_cur_is_after_last(get_page_cur()));
}

inline void btr_pcur_t::move_to_next_on_page() {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  page_cur_move_to_next(get_page_cur());

  m_old_stored = false;
}

inline void btr_pcur_t::move_to_prev_on_page() {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  page_cur_move_to_prev(get_page_cur());

  m_old_stored = false;
}

inline void btr_pcur_t::move_to_last_on_page(mtr_t *mtr) {
  UT_NOT_USED(mtr);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  page_cur_set_after_last(get_block(), get_page_cur());

  m_old_stored = false;
}

inline dberr_t btr_pcur_t::move_to_next_user_rec(mtr_t *mtr) {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  for (;;) {
    m_old_stored = false;

    if (is_after_last_on_page()) {
      if (is_after_last_in_tree(mtr)) {
        return (DB_END_OF_INDEX);
      }

      move_to_next_page(mtr);
    } else {
      move_to_next_on_page();
    }

    if (is_on_user_rec()) {
      return (DB_SUCCESS);
    }
  }
}

inline bool btr_pcur_t::move_to_next(mtr_t *mtr) {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  m_old_stored = false;

  if (is_after_last_on_page()) {
    if (is_after_last_in_tree(mtr)) {
      return (false);
    }

    move_to_next_page(mtr);

    return (true);
  }

  move_to_next_on_page();

  return (true);
}

inline void btr_pcur_t::commit_specify_mtr(mtr_t *mtr) {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);

  m_latch_mode = BTR_NO_LATCHES;

  mtr_commit(mtr);

  m_pos_state = BTR_PCUR_WAS_POSITIONED;
}

inline void btr_pcur_t::move_before_first_on_page() {
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  page_cur_set_before_first(get_block(), get_page_cur());

  m_old_stored = false;
}

inline void btr_pcur_t::reset() {
  free_rec_buf();

  m_old_rec_buf = nullptr;
  m_btr_cur.index = nullptr;
  m_btr_cur.page_cur.rec = nullptr;
  m_old_rec = nullptr;
  m_old_n_fields = 0;
  m_old_stored = false;

  m_latch_mode = BTR_NO_LATCHES;
  m_pos_state = BTR_PCUR_NOT_POSITIONED;
}

#endif /* !UNIV_HOTBACKUP */

#endif /* !btr0pcur_h */
