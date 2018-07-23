/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.
Copyright (c) 2012, Facebook Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/** @file btr/btr0cur.cc
 The index tree cursor

 All changes that row operations make to a B-tree or the records
 there must go through this module! Undo log records are written here
 of every modify or insert of a clustered index record.

                         NOTE!!!
 To make sure we do not run out of disk space during a pessimistic
 insert or update, we have to reserve 2 x the height of the index tree
 many pages in the tablespace before we start the operation, because
 if leaf splitting has been started, it is difficult to undo, except
 by crashing the database and doing a roll-forward.

 Created 10/16/1994 Heikki Tuuri
 *******************************************************/

#include "btr0cur.h"

#include <assert.h>

#include "my_dbug.h"

#ifndef UNIV_HOTBACKUP
#include <zlib.h>
#include "btr0btr.h"
#include "btr0sea.h"
#include "buf0lru.h"
#ifdef UNIV_DEBUG
#include "current_thd.h"
#include "debug_sync.h"
#endif /* UNIV_DEBUG */
#include "ibuf0ibuf.h"
#include "lob0lob.h"
#include "lock0lock.h"
#include "mtr0log.h"
#include "row0upd.h"
#endif /* !UNIV_HOTBACKUP */
#include "page0page.h"
#include "page0zip.h"
#ifndef UNIV_HOTBACKUP
#include "que0que.h"
#endif /* !UNIV_HOTBACKUP */
#include "rem0cmp.h"
#include "rem0rec.h"
#include "row0log.h"
#ifndef UNIV_HOTBACKUP
#include "row0purge.h"
#include "row0row.h"
#endif /* !UNIV_HOTBACKUP */
#include "row0upd.h"
#ifndef UNIV_HOTBACKUP
#include "srv0srv.h"
#endif /* !UNIV_HOTBACKUP */
#include "srv0start.h"
#ifndef UNIV_HOTBACKUP
#include "trx0rec.h"
#include "trx0roll.h"
#endif /* !UNIV_HOTBACKUP */

/** Buffered B-tree operation types, introduced as part of delete buffering. */
enum btr_op_t {
  BTR_NO_OP = 0,               /*!< Not buffered */
  BTR_INSERT_OP,               /*!< Insert, do not ignore UNIQUE */
  BTR_INSERT_IGNORE_UNIQUE_OP, /*!< Insert, ignoring UNIQUE */
  BTR_DELETE_OP,               /*!< Purge a delete-marked record */
  BTR_DELMARK_OP               /*!< Mark a record for deletion */
};

/** Modification types for the B-tree operation. Note that the order of
the enum values is important.*/
enum btr_intention_t {
  BTR_INTENTION_DELETE,
  BTR_INTENTION_BOTH,
  BTR_INTENTION_INSERT
};

/** For the index->lock scalability improvement, only possibility of clear
performance regression observed was caused by grown huge history list length.
That is because the exclusive use of index->lock also worked as reserving
free blocks and read IO bandwidth with priority. To avoid huge glowing history
list as same level with previous implementation, prioritizes pessimistic tree
operations by purge as the previous, when it seems to be growing huge.

 Experimentally, the history list length starts to affect to performance
throughput clearly from about 100000. */
#define BTR_CUR_FINE_HISTORY_LENGTH 100000

/** Number of searches down the B-tree in btr_cur_search_to_nth_level(). */
ulint btr_cur_n_non_sea = 0;
/** Number of successful adaptive hash index lookups in
btr_cur_search_to_nth_level(). */
ulint btr_cur_n_sea = 0;
/** Old value of btr_cur_n_non_sea.  Copied by
srv_refresh_innodb_monitor_stats().  Referenced by
srv_printf_innodb_monitor(). */
ulint btr_cur_n_non_sea_old = 0;
/** Old value of btr_cur_n_sea.  Copied by
srv_refresh_innodb_monitor_stats().  Referenced by
srv_printf_innodb_monitor(). */
ulint btr_cur_n_sea_old = 0;

#ifdef UNIV_DEBUG
/* Flag to limit optimistic insert records */
uint btr_cur_limit_optimistic_insert_debug = 0;
#endif /* UNIV_DEBUG */

/** In the optimistic insert, if the insert does not fit, but this much space
can be released by page reorganize, then it is reorganized */
#define BTR_CUR_PAGE_REORGANIZE_LIMIT (UNIV_PAGE_SIZE / 32)

/** Estimated table level stats from sampled value.
@param value sampled stats
@param index index being sampled
@param sample number of sampled rows
@param ext_size external stored data size
@param not_empty table not empty
@return estimated table wide stats from sampled value */
#define BTR_TABLE_STATS_FROM_SAMPLE(value, index, sample, ext_size, not_empty) \
  (((value) * static_cast<int64_t>(index->stat_n_leaf_pages) + (sample)-1 +    \
    (ext_size) + (not_empty)) /                                                \
   ((sample) + (ext_size)))

#ifndef UNIV_HOTBACKUP
/** Adds path information to the cursor for the current page, for which
 the binary search has been performed. */
static void btr_cur_add_path_info(
    btr_cur_t *cursor,  /*!< in: cursor positioned on a page */
    ulint height,       /*!< in: height of the page in tree;
                        0 means leaf node */
    ulint root_height); /*!< in: root node height in tree */

/*==================== B-TREE SEARCH =========================*/

/** Latches the leaf page or pages requested.
@param[in]	block		leaf page where the search converged
@param[in]	page_id		page id of the leaf
@param[in]	page_size	page size
@param[in]	latch_mode	BTR_SEARCH_LEAF, ...
@param[in]	cursor		cursor
@param[in]	mtr		mini-transaction
@return	blocks and savepoints which actually latched. */
btr_latch_leaves_t btr_cur_latch_leaves(buf_block_t *block,
                                        const page_id_t &page_id,
                                        const page_size_t &page_size,
                                        ulint latch_mode, btr_cur_t *cursor,
                                        mtr_t *mtr) {
  ulint mode;
  page_no_t left_page_no;
  page_no_t right_page_no;
  buf_block_t *get_block;
  page_t *page = buf_block_get_frame(block);
  bool spatial;
  btr_latch_leaves_t latch_leaves = {{NULL, NULL, NULL}, {0, 0, 0}};

  spatial = dict_index_is_spatial(cursor->index) && cursor->rtr_info;
  ut_ad(buf_page_in_file(&block->page));

  switch (latch_mode) {
    case BTR_SEARCH_LEAF:
    case BTR_MODIFY_LEAF:
    case BTR_SEARCH_TREE:
      if (spatial) {
        cursor->rtr_info->tree_savepoints[RTR_MAX_LEVELS] =
            mtr_set_savepoint(mtr);
      }

      mode = latch_mode == BTR_MODIFY_LEAF ? RW_X_LATCH : RW_S_LATCH;
      latch_leaves.savepoints[1] = mtr_set_savepoint(mtr);
      get_block = btr_block_get(page_id, page_size, mode, cursor->index, mtr);
      latch_leaves.blocks[1] = get_block;
#ifdef UNIV_BTR_DEBUG
      ut_a(page_is_comp(get_block->frame) == page_is_comp(page));
#endif /* UNIV_BTR_DEBUG */
      if (spatial) {
        cursor->rtr_info->tree_blocks[RTR_MAX_LEVELS] = get_block;
      }

      return (latch_leaves);
    case BTR_MODIFY_TREE:
      /* It is exclusive for other operations which calls
      btr_page_set_prev() */
      ut_ad(mtr_memo_contains_flagged(mtr, dict_index_get_lock(cursor->index),
                                      MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
            cursor->index->table->is_intrinsic());
      /* x-latch also siblings from left to right */
      left_page_no = btr_page_get_prev(page, mtr);

      if (left_page_no != FIL_NULL) {
        if (spatial) {
          cursor->rtr_info->tree_savepoints[RTR_MAX_LEVELS] =
              mtr_set_savepoint(mtr);
        }

        latch_leaves.savepoints[0] = mtr_set_savepoint(mtr);
        get_block = btr_block_get(page_id_t(page_id.space(), left_page_no),
                                  page_size, RW_X_LATCH, cursor->index, mtr);
        latch_leaves.blocks[0] = get_block;

        if (spatial) {
          cursor->rtr_info->tree_blocks[RTR_MAX_LEVELS] = get_block;
        }
      }

      if (spatial) {
        cursor->rtr_info->tree_savepoints[RTR_MAX_LEVELS + 1] =
            mtr_set_savepoint(mtr);
      }

      latch_leaves.savepoints[1] = mtr_set_savepoint(mtr);
      get_block =
          btr_block_get(page_id, page_size, RW_X_LATCH, cursor->index, mtr);
      latch_leaves.blocks[1] = get_block;

#ifdef UNIV_BTR_DEBUG
      /* Sanity check only after both the blocks are latched. */
      if (latch_leaves.blocks[0] != NULL) {
        ut_a(page_is_comp(latch_leaves.blocks[0]->frame) == page_is_comp(page));
        ut_a(btr_page_get_next(latch_leaves.blocks[0]->frame, mtr) ==
             page_get_page_no(page));
      }
      ut_a(page_is_comp(get_block->frame) == page_is_comp(page));
#endif /* UNIV_BTR_DEBUG */

      if (spatial) {
        cursor->rtr_info->tree_blocks[RTR_MAX_LEVELS + 1] = get_block;
      }

      right_page_no = btr_page_get_next(page, mtr);

      if (right_page_no != FIL_NULL) {
        if (spatial) {
          cursor->rtr_info->tree_savepoints[RTR_MAX_LEVELS + 2] =
              mtr_set_savepoint(mtr);
        }
        latch_leaves.savepoints[2] = mtr_set_savepoint(mtr);
        get_block = btr_block_get(page_id_t(page_id.space(), right_page_no),
                                  page_size, RW_X_LATCH, cursor->index, mtr);
        latch_leaves.blocks[2] = get_block;
#ifdef UNIV_BTR_DEBUG
        ut_a(page_is_comp(get_block->frame) == page_is_comp(page));
        ut_a(btr_page_get_prev(get_block->frame, mtr) ==
             page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
        if (spatial) {
          cursor->rtr_info->tree_blocks[RTR_MAX_LEVELS + 2] = get_block;
        }
      }

      return (latch_leaves);

    case BTR_SEARCH_PREV:
    case BTR_MODIFY_PREV:
      mode = latch_mode == BTR_SEARCH_PREV ? RW_S_LATCH : RW_X_LATCH;
      /* latch also left sibling */
      rw_lock_s_lock(&block->lock);
      left_page_no = btr_page_get_prev(page, mtr);
      rw_lock_s_unlock(&block->lock);

      if (left_page_no != FIL_NULL) {
        latch_leaves.savepoints[0] = mtr_set_savepoint(mtr);
        get_block = btr_block_get(page_id_t(page_id.space(), left_page_no),
                                  page_size, mode, cursor->index, mtr);
        latch_leaves.blocks[0] = get_block;
        cursor->left_block = get_block;
#ifdef UNIV_BTR_DEBUG
        ut_a(page_is_comp(get_block->frame) == page_is_comp(page));
        ut_a(btr_page_get_next(get_block->frame, mtr) ==
             page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
      }

      latch_leaves.savepoints[1] = mtr_set_savepoint(mtr);
      get_block = btr_block_get(page_id, page_size, mode, cursor->index, mtr);
      latch_leaves.blocks[1] = get_block;
#ifdef UNIV_BTR_DEBUG
      ut_a(page_is_comp(get_block->frame) == page_is_comp(page));
#endif /* UNIV_BTR_DEBUG */
      return (latch_leaves);
    case BTR_CONT_MODIFY_TREE:
      ut_ad(dict_index_is_spatial(cursor->index));
      return (latch_leaves);
  }

  ut_error;
}

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
                                     const char *file, ulint line, mtr_t *mtr) {
  ulint mode;
  page_no_t left_page_no;

  switch (*latch_mode) {
    case BTR_SEARCH_LEAF:
    case BTR_MODIFY_LEAF:
      return (buf_page_optimistic_get(*latch_mode, block, modify_clock, file,
                                      line, mtr));
    case BTR_SEARCH_PREV:
    case BTR_MODIFY_PREV:
      mode = *latch_mode == BTR_SEARCH_PREV ? RW_S_LATCH : RW_X_LATCH;

      buf_page_mutex_enter(block);
      if (buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE) {
        buf_page_mutex_exit(block);
        return (false);
      }
      /* pin the block not to be relocated */
      buf_block_buf_fix_inc(block, file, line);
      buf_page_mutex_exit(block);

      rw_lock_s_lock(&block->lock);
      if (block->modify_clock != modify_clock) {
        rw_lock_s_unlock(&block->lock);

        goto unpin_failed;
      }
      left_page_no = btr_page_get_prev(buf_block_get_frame(block), mtr);
      rw_lock_s_unlock(&block->lock);

      if (left_page_no != FIL_NULL) {
        const page_id_t page_id(dict_index_get_space(cursor->index),
                                left_page_no);

        cursor->left_block =
            btr_block_get(page_id, dict_table_page_size(cursor->index->table),
                          mode, cursor->index, mtr);
      } else {
        cursor->left_block = NULL;
      }

      if (buf_page_optimistic_get(mode, block, modify_clock, file, line, mtr)) {
        if (btr_page_get_prev(buf_block_get_frame(block), mtr) ==
            left_page_no) {
          /* adjust buf_fix_count */
          buf_page_mutex_enter(block);
          buf_block_buf_fix_dec(block);
          buf_page_mutex_exit(block);

          *latch_mode = mode;
          return (true);
        } else {
          /* release the block */
          btr_leaf_page_release(block, mode, mtr);
        }
      }

      /* release the left block */
      if (cursor->left_block != NULL) {
        btr_leaf_page_release(cursor->left_block, mode, mtr);
      }
    unpin_failed:
      /* unpin the block */
      buf_page_mutex_enter(block);
      buf_block_buf_fix_dec(block);
      buf_page_mutex_exit(block);

      return (false);

    default:
      ut_error;
  }
}

/**
Gets intention in btr_intention_t from latch_mode, and cleares the intention
at the latch_mode.
@param latch_mode	in/out: pointer to latch_mode
@return intention for latching tree */
static btr_intention_t btr_cur_get_and_clear_intention(ulint *latch_mode) {
  btr_intention_t intention;

  switch (*latch_mode & (BTR_LATCH_FOR_INSERT | BTR_LATCH_FOR_DELETE)) {
    case BTR_LATCH_FOR_INSERT:
      intention = BTR_INTENTION_INSERT;
      break;
    case BTR_LATCH_FOR_DELETE:
      intention = BTR_INTENTION_DELETE;
      break;
    default:
      /* both or unknown */
      intention = BTR_INTENTION_BOTH;
  }
  *latch_mode &= ~(BTR_LATCH_FOR_INSERT | BTR_LATCH_FOR_DELETE);

  return (intention);
}

/**
Gets the desired latch type for the root leaf (root page is root leaf)
at the latch mode.
@param latch_mode	in: BTR_SEARCH_LEAF, ...
@return latch type */
static rw_lock_type_t btr_cur_latch_for_root_leaf(ulint latch_mode) {
  switch (latch_mode) {
    case BTR_SEARCH_LEAF:
    case BTR_SEARCH_TREE:
    case BTR_SEARCH_PREV:
      return (RW_S_LATCH);
    case BTR_MODIFY_LEAF:
    case BTR_MODIFY_TREE:
    case BTR_MODIFY_PREV:
      return (RW_X_LATCH);
    case BTR_CONT_MODIFY_TREE:
    case BTR_CONT_SEARCH_TREE:
      /* A root page should be latched already,
      and don't need to be latched here.
      fall through (RW_NO_LATCH) */
    case BTR_NO_LATCHES:
      return (RW_NO_LATCH);
  }

  ut_error;
}

/** Detects whether the modifying record might need a modifying tree structure.
@param[in]	index		index
@param[in]	page		page
@param[in]	lock_intention	lock intention for the tree operation
@param[in]	rec		record (current node_ptr)
@param[in]	rec_size	size of the record or max size of node_ptr
@param[in]	page_size	page size
@param[in]	mtr		mtr
@return true if tree modification is needed */
static bool btr_cur_will_modify_tree(dict_index_t *index, const page_t *page,
                                     btr_intention_t lock_intention,
                                     const rec_t *rec, ulint rec_size,
                                     const page_size_t &page_size, mtr_t *mtr) {
  ut_ad(!page_is_leaf(page));
  ut_ad(mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                  MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
        index->table->is_intrinsic());

  /* Pessimistic delete of the first record causes delete & insert
  of node_ptr at upper level. And a subsequent page shrink is
  possible. It causes delete of node_ptr at the upper level.
  So we should pay attention also to 2nd record not only
  first record and last record. Because if the "delete & insert" are
  done for the different page, the 2nd record become
  first record and following compress might delete the record and causes
  the uppper level node_ptr modification. */

  if (lock_intention <= BTR_INTENTION_BOTH) {
    ulint margin;

    /* check delete will cause. (BTR_INTENTION_BOTH
    or BTR_INTENTION_DELETE) */
    /* first, 2nd, 2nd-last and last records are 4 records */
    if (page_get_n_recs(page) < 5) {
      return (true);
    }

    /* is first, 2nd or last record */
    if (page_rec_is_first(rec, page) ||
        (mach_read_from_4(page + FIL_PAGE_NEXT) != FIL_NULL &&
         (page_rec_is_last(rec, page) || page_rec_is_second_last(rec, page))) ||
        (mach_read_from_4(page + FIL_PAGE_PREV) != FIL_NULL &&
         page_rec_is_second(rec, page))) {
      return (true);
    }

    if (lock_intention == BTR_INTENTION_BOTH) {
      /* Delete at leftmost record in a page causes delete
      & insert at its parent page. After that, the delete
      might cause btr_compress() and delete record at its
      parent page. Thus we should consider max 2 deletes. */

      margin = rec_size * 2;
    } else {
      ut_ad(lock_intention == BTR_INTENTION_DELETE);

      margin = rec_size;
    }
    /* NOTE: call mach_read_from_4() directly to avoid assertion
    failure. It is safe because we already have SX latch of the
    index tree */
    if (page_get_data_size(page) <
            margin + BTR_CUR_PAGE_COMPRESS_LIMIT(index) ||
        (mach_read_from_4(page + FIL_PAGE_NEXT) == FIL_NULL &&
         mach_read_from_4(page + FIL_PAGE_PREV) == FIL_NULL)) {
      return (true);
    }
  }

  if (lock_intention >= BTR_INTENTION_BOTH) {
    /* check insert will cause. BTR_INTENTION_BOTH
    or BTR_INTENTION_INSERT*/

    /* Once we invoke the btr_cur_limit_optimistic_insert_debug,
    we should check it here in advance, since the max allowable
    records in a page is limited. */
    LIMIT_OPTIMISTIC_INSERT_DEBUG(page_get_n_recs(page), return (true));

    /* needs 2 records' space for the case the single split and
    insert cannot fit.
    page_get_max_insert_size_after_reorganize() includes space
    for page directory already */
    ulint max_size = page_get_max_insert_size_after_reorganize(page, 2);

    if (max_size < BTR_CUR_PAGE_REORGANIZE_LIMIT + rec_size ||
        max_size < rec_size * 2) {
      return (true);
    }
    /* TODO: optimize this condition for compressed page.
    this is based on the worst compress rate.
    currently looking only uncompressed page, but we can look
    also compressed page page_zip_available() if already in the
    buffer pool */
    /* needs 2 records' space also for worst compress rate. */
    if (page_size.is_compressed() &&
        page_zip_empty_size(index->n_fields, page_size.physical()) <
            rec_size * 2 + page_get_data_size(page) +
                page_dir_calc_reserved_space(page_get_n_recs(page) + 2) + 1) {
      return (true);
    }
  }

  return (false);
}

/** Detects whether the modifying record might need a opposite modification
to the intention.
@param[in]	page		page
@param[in]	lock_intention	lock intention for the tree operation
@param[in]	rec		record (current node_ptr)
@return	true if tree modification is needed */
static bool btr_cur_need_opposite_intention(const page_t *page,
                                            btr_intention_t lock_intention,
                                            const rec_t *rec) {
  switch (lock_intention) {
    case BTR_INTENTION_DELETE:
      return ((mach_read_from_4(page + FIL_PAGE_PREV) != FIL_NULL &&
               page_rec_is_first(rec, page)) ||
              (mach_read_from_4(page + FIL_PAGE_NEXT) != FIL_NULL &&
               page_rec_is_last(rec, page)));
    case BTR_INTENTION_INSERT:
      return (mach_read_from_4(page + FIL_PAGE_NEXT) != FIL_NULL &&
              page_rec_is_last(rec, page));
    case BTR_INTENTION_BOTH:
      return (false);
  }

  ut_error;
}

/** Searches an index tree and positions a tree cursor on a given level.
 NOTE: n_fields_cmp in tuple must be set so that it cannot be compared
 to node pointer page number fields on the upper levels of the tree!
 Note that if mode is PAGE_CUR_LE, which is used in inserts, then
 cursor->up_match and cursor->low_match both will have sensible values.
 If mode is PAGE_CUR_GE, then up_match will a have a sensible value.

 If mode is PAGE_CUR_LE , cursor is left at the place where an insert of the
 search tuple should be performed in the B-tree. InnoDB does an insert
 immediately after the cursor. Thus, the cursor may end up on a user record,
 or on a page infimum record. */
void btr_cur_search_to_nth_level(
    dict_index_t *index,   /*!< in: index */
    ulint level,           /*!< in: the tree level of search */
    const dtuple_t *tuple, /*!< in: data tuple; NOTE: n_fields_cmp in
                           tuple must be set so that it cannot get
                           compared to the node ptr page number field! */
    page_cur_mode_t mode,  /*!< in: PAGE_CUR_L, ...;
                           Inserts should always be made using
                           PAGE_CUR_LE to search the position! */
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
    /*!< in: info on the latch mode the
    caller currently has on search system:
    RW_S_LATCH, or 0 */
    const char *file, /*!< in: file name */
    ulint line,       /*!< in: line where called */
    mtr_t *mtr)       /*!< in: mtr */
{
  page_t *page = NULL; /* remove warning */
  buf_block_t *block;
  buf_block_t *guess;
  ulint height;
  ulint up_match;
  ulint up_bytes;
  ulint low_match;
  ulint low_bytes;
  ulint savepoint;
  ulint rw_latch;
  page_cur_mode_t page_mode;
  page_cur_mode_t search_mode = PAGE_CUR_UNSUPP;
  ulint buf_mode;
  ulint estimate;
  ulint node_ptr_max_size = UNIV_PAGE_SIZE / 2;
  page_cur_t *page_cursor;
  btr_op_t btr_op;
  ulint root_height = 0; /* remove warning */

  ulint upper_rw_latch, root_leaf_rw_latch;
  btr_intention_t lock_intention;
  bool modify_external;
  buf_block_t *tree_blocks[BTR_MAX_LEVELS];
  ulint tree_savepoints[BTR_MAX_LEVELS];
  ulint n_blocks = 0;
  ulint n_releases = 0;
  bool detected_same_key_root = false;

  bool retrying_for_search_prev = false;
  ulint leftmost_from_level = 0;
  buf_block_t **prev_tree_blocks = NULL;
  ulint *prev_tree_savepoints = NULL;
  ulint prev_n_blocks = 0;
  ulint prev_n_releases = 0;
  bool need_path = true;
  bool rtree_parent_modified = false;
  bool mbr_adj = false;
  bool found = false;

  DBUG_ENTER("btr_cur_search_to_nth_level");

#ifdef BTR_CUR_ADAPT
  btr_search_t *info;
#endif /* BTR_CUR_ADAPT */
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  ulint offsets2_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets2 = offsets2_;
  rec_offs_init(offsets_);
  rec_offs_init(offsets2_);
  /* Currently, PAGE_CUR_LE is the only search mode used for searches
  ending to upper levels */

  ut_ad(level == 0 || mode == PAGE_CUR_LE || RTREE_SEARCH_MODE(mode));
  ut_ad(dict_index_check_search_tuple(index, tuple));
  ut_ad(!dict_index_is_ibuf(index) || ibuf_inside(mtr));

  UNIV_MEM_INVALID(&cursor->up_match, sizeof cursor->up_match);
  UNIV_MEM_INVALID(&cursor->up_bytes, sizeof cursor->up_bytes);
  UNIV_MEM_INVALID(&cursor->low_match, sizeof cursor->low_match);
  UNIV_MEM_INVALID(&cursor->low_bytes, sizeof cursor->low_bytes);
#ifdef UNIV_DEBUG
  cursor->up_match = ULINT_UNDEFINED;
  cursor->low_match = ULINT_UNDEFINED;
#endif /* UNIV_DEBUG */

  ibool s_latch_by_caller;

  s_latch_by_caller = latch_mode & BTR_ALREADY_S_LATCHED;

  ut_ad(!s_latch_by_caller || srv_read_only_mode ||
        mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                  MTR_MEMO_S_LOCK | MTR_MEMO_SX_LOCK));

  /* These flags are mutually exclusive, they are lumped together
  with the latch mode for historical reasons. It's possible for
  none of the flags to be set. */
  switch (UNIV_EXPECT(latch_mode & (BTR_INSERT | BTR_DELETE | BTR_DELETE_MARK),
                      0)) {
    case 0:
      btr_op = BTR_NO_OP;
      break;
    case BTR_INSERT:
      btr_op = (latch_mode & BTR_IGNORE_SEC_UNIQUE)
                   ? BTR_INSERT_IGNORE_UNIQUE_OP
                   : BTR_INSERT_OP;
      break;
    case BTR_DELETE:
      btr_op = BTR_DELETE_OP;
      ut_a(cursor->purge_node);
      break;
    case BTR_DELETE_MARK:
      btr_op = BTR_DELMARK_OP;
      break;
    default:
      /* only one of BTR_INSERT, BTR_DELETE, BTR_DELETE_MARK
      should be specified at a time */
      ut_error;
  }

  /* Operations on the insert buffer tree cannot be buffered. */
  ut_ad(btr_op == BTR_NO_OP || !dict_index_is_ibuf(index));
  /* Operations on the clustered index cannot be buffered. */
  ut_ad(btr_op == BTR_NO_OP || !index->is_clustered());
  /* Operations on the temporary table(indexes) cannot be buffered. */
  ut_ad(btr_op == BTR_NO_OP || !index->table->is_temporary());
  /* Operation on the spatial index cannot be buffered. */
  ut_ad(btr_op == BTR_NO_OP || !dict_index_is_spatial(index));

  estimate = latch_mode & BTR_ESTIMATE;

  lock_intention = btr_cur_get_and_clear_intention(&latch_mode);

  modify_external = latch_mode & BTR_MODIFY_EXTERNAL;

  /* Turn the flags unrelated to the latch mode off. */
  latch_mode = BTR_LATCH_MODE_WITHOUT_FLAGS(latch_mode);

  ut_ad(!modify_external || latch_mode == BTR_MODIFY_LEAF);

  ut_ad(!s_latch_by_caller || latch_mode == BTR_SEARCH_LEAF ||
        latch_mode == BTR_SEARCH_TREE || latch_mode == BTR_MODIFY_LEAF);

  cursor->flag = BTR_CUR_BINARY;
  cursor->index = index;

#ifndef BTR_CUR_ADAPT
  guess = NULL;
#else
  info = btr_search_get_info(index);

  if (!buf_pool_is_obsolete(info->withdraw_clock)) {
    guess = info->root_guess;
  } else {
    guess = NULL;
  }

#ifdef BTR_CUR_HASH_ADAPT

#ifdef UNIV_SEARCH_PERF_STAT
  info->n_searches++;
#endif
  /* Use of AHI is disabled for intrinsic table as these tables re-use
  the index-id and AHI validation is based on index-id. */
  if (rw_lock_get_writer(btr_get_search_latch(index)) == RW_LOCK_NOT_LOCKED &&
      latch_mode <= BTR_MODIFY_LEAF && info->last_hash_succ &&
      !index->disable_ahi && !estimate
#ifdef PAGE_CUR_LE_OR_EXTENDS
      && mode != PAGE_CUR_LE_OR_EXTENDS
#endif /* PAGE_CUR_LE_OR_EXTENDS */
      && !dict_index_is_spatial(index)
      /* If !has_search_latch, we do a dirty read of
      btr_search_enabled below, and btr_search_guess_on_hash()
      will have to check it again. */
      && UNIV_LIKELY(btr_search_enabled) && !modify_external &&
      btr_search_guess_on_hash(index, info, tuple, mode, latch_mode, cursor,
                               has_search_latch, mtr)) {

    /* Search using the hash index succeeded */

    ut_ad(cursor->up_match != ULINT_UNDEFINED || mode != PAGE_CUR_GE);
    ut_ad(cursor->up_match != ULINT_UNDEFINED || mode != PAGE_CUR_LE);
    ut_ad(cursor->low_match != ULINT_UNDEFINED || mode != PAGE_CUR_LE);
    btr_cur_n_sea++;

    DBUG_VOID_RETURN;
  }
#endif /* BTR_CUR_HASH_ADAPT */
#endif /* BTR_CUR_ADAPT */
  btr_cur_n_non_sea++;
  DBUG_EXECUTE_IF("non_ahi_search",
                  DBUG_ASSERT(!strcmp(index->table->name.m_name, "test/t1")););

  /* If the hash search did not succeed, do binary search down the
  tree */

  if (has_search_latch) {
    /* Release possible search latch to obey latching order */
    rw_lock_s_unlock(btr_get_search_latch(index));
  }

  /* Store the position of the tree latch we push to mtr so that we
  know how to release it when we have latched leaf node(s) */

  savepoint = mtr_set_savepoint(mtr);

  switch (latch_mode) {
    case BTR_MODIFY_TREE:
      /* Most of delete-intended operations are purging.
      Free blocks and read IO bandwidth should be prior
      for them, when the history list is glowing huge. */
      if (lock_intention == BTR_INTENTION_DELETE &&
          trx_sys->rseg_history_len > BTR_CUR_FINE_HISTORY_LENGTH &&
          buf_get_n_pending_read_ios()) {
        mtr_x_lock(dict_index_get_lock(index), mtr);
      } else if (dict_index_is_spatial(index) &&
                 lock_intention <= BTR_INTENTION_BOTH) {
        /* X lock the if there is possibility of
        pessimistic delete on spatial index. As we could
        lock upward for the tree */

        mtr_x_lock(dict_index_get_lock(index), mtr);
      } else {
        mtr_sx_lock(dict_index_get_lock(index), mtr);
      }
      upper_rw_latch = RW_X_LATCH;
      break;
    case BTR_CONT_MODIFY_TREE:
    case BTR_CONT_SEARCH_TREE:
      /* Do nothing */
      ut_ad(srv_read_only_mode ||
            mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                      MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK));
      if (dict_index_is_spatial(index) && latch_mode == BTR_CONT_MODIFY_TREE) {
        /* If we are about to locating parent page for split
        and/or merge operation for R-Tree index, X latch
        the parent */
        upper_rw_latch = RW_X_LATCH;
      } else {
        upper_rw_latch = RW_NO_LATCH;
      }
      break;
    default:
      if (!srv_read_only_mode) {
        if (s_latch_by_caller) {
          /* The BTR_ALREADY_S_LATCHED indicates that the index->lock has been
           * taken either in RW_S_LATCH or RW_SX_LATCH mode. */
          ut_ad(rw_lock_own_flagged(dict_index_get_lock(index),
                                    RW_LOCK_FLAG_S | RW_LOCK_FLAG_SX));

        } else if (!modify_external) {
          /* BTR_SEARCH_TREE is intended to be used with
          BTR_ALREADY_S_LATCHED */
          ut_ad(latch_mode != BTR_SEARCH_TREE);

          mtr_s_lock(dict_index_get_lock(index), mtr);
        } else {
          /* BTR_MODIFY_EXTERNAL needs to be excluded */
          mtr_sx_lock(dict_index_get_lock(index), mtr);
        }
        upper_rw_latch = RW_S_LATCH;
      } else {
        upper_rw_latch = RW_NO_LATCH;
      }
  }
  root_leaf_rw_latch = btr_cur_latch_for_root_leaf(latch_mode);

  page_cursor = btr_cur_get_page_cur(cursor);

  const space_id_t space = dict_index_get_space(index);
  const page_size_t page_size(dict_table_page_size(index->table));

  /* Start with the root page. */
  page_id_t page_id(space, dict_index_get_page(index));

  if (root_leaf_rw_latch == RW_X_LATCH) {
    node_ptr_max_size = dict_index_node_ptr_max_size(index);
  }

  up_match = 0;
  up_bytes = 0;
  low_match = 0;
  low_bytes = 0;

  height = ULINT_UNDEFINED;

  /* We use these modified search modes on non-leaf levels of the
  B-tree. These let us end up in the right B-tree leaf. In that leaf
  we use the original search mode. */

  switch (mode) {
    case PAGE_CUR_GE:
      page_mode = PAGE_CUR_L;
      break;
    case PAGE_CUR_G:
      page_mode = PAGE_CUR_LE;
      break;
    default:
#ifdef PAGE_CUR_LE_OR_EXTENDS
      ut_ad(mode == PAGE_CUR_L || mode == PAGE_CUR_LE ||
            RTREE_SEARCH_MODE(mode) || mode == PAGE_CUR_LE_OR_EXTENDS);
#else  /* PAGE_CUR_LE_OR_EXTENDS */
      ut_ad(mode == PAGE_CUR_L || mode == PAGE_CUR_LE ||
            RTREE_SEARCH_MODE(mode));
#endif /* PAGE_CUR_LE_OR_EXTENDS */
      page_mode = mode;
      break;
  }

  /* Loop and search until we arrive at the desired level */
  btr_latch_leaves_t latch_leaves = {{NULL, NULL, NULL}, {0, 0, 0}};

search_loop:
  buf_mode = BUF_GET;
  rw_latch = RW_NO_LATCH;
  rtree_parent_modified = false;

  if (height != 0) {
    /* We are about to fetch the root or a non-leaf page. */
    if ((latch_mode != BTR_MODIFY_TREE || height == level) &&
        !retrying_for_search_prev) {
      /* If doesn't have SX or X latch of index,
      each pages should be latched before reading. */
      if (modify_external && height == ULINT_UNDEFINED &&
          upper_rw_latch == RW_S_LATCH) {
        /* needs sx-latch of root page
        for fseg operation */
        rw_latch = RW_SX_LATCH;
      } else {
        rw_latch = upper_rw_latch;
      }
    }
  } else if (latch_mode <= BTR_MODIFY_LEAF) {
    rw_latch = latch_mode;

    if (btr_op != BTR_NO_OP &&
        ibuf_should_try(index, btr_op != BTR_INSERT_OP)) {
      /* Try to buffer the operation if the leaf
      page is not in the buffer pool. */

      buf_mode = btr_op == BTR_DELETE_OP ? BUF_GET_IF_IN_POOL_OR_WATCH
                                         : BUF_GET_IF_IN_POOL;
    }
  }

retry_page_get:
  ut_ad(n_blocks < BTR_MAX_LEVELS);
  tree_savepoints[n_blocks] = mtr_set_savepoint(mtr);
  block = buf_page_get_gen(page_id, page_size, rw_latch, guess, buf_mode, file,
                           line, mtr);
  tree_blocks[n_blocks] = block;

  if (block == NULL) {
    /* This must be a search to perform an insert/delete
    mark/ delete; try using the insert/delete buffer */

    ut_ad(height == 0);
    ut_ad(cursor->thr);

    switch (btr_op) {
      case BTR_INSERT_OP:
      case BTR_INSERT_IGNORE_UNIQUE_OP:
        ut_ad(buf_mode == BUF_GET_IF_IN_POOL);
        ut_ad(!dict_index_is_spatial(index));

        if (ibuf_insert(IBUF_OP_INSERT, tuple, index, page_id, page_size,
                        cursor->thr)) {
          cursor->flag = BTR_CUR_INSERT_TO_IBUF;

          goto func_exit;
        }
        break;

      case BTR_DELMARK_OP:
        ut_ad(buf_mode == BUF_GET_IF_IN_POOL);
        ut_ad(!dict_index_is_spatial(index));

        if (ibuf_insert(IBUF_OP_DELETE_MARK, tuple, index, page_id, page_size,
                        cursor->thr)) {
          cursor->flag = BTR_CUR_DEL_MARK_IBUF;

          goto func_exit;
        }

        break;

      case BTR_DELETE_OP:
        ut_ad(buf_mode == BUF_GET_IF_IN_POOL_OR_WATCH);
        ut_ad(!dict_index_is_spatial(index));

        if (!row_purge_poss_sec(cursor->purge_node, index, tuple)) {
          /* The record cannot be purged yet. */
          cursor->flag = BTR_CUR_DELETE_REF;
        } else if (ibuf_insert(IBUF_OP_DELETE, tuple, index, page_id, page_size,
                               cursor->thr)) {
          /* The purge was buffered. */
          cursor->flag = BTR_CUR_DELETE_IBUF;
        } else {
          /* The purge could not be buffered. */
          buf_pool_watch_unset(page_id);
          break;
        }

        buf_pool_watch_unset(page_id);
        goto func_exit;

      default:
        ut_error;
    }

    /* Insert to the insert/delete buffer did not succeed, we
    must read the page from disk. */

    buf_mode = BUF_GET;

    goto retry_page_get;
  }

  if (retrying_for_search_prev && height != 0) {
    /* also latch left sibling */
    page_no_t left_page_no;
    buf_block_t *get_block;

    ut_ad(rw_latch == RW_NO_LATCH);

    rw_latch = upper_rw_latch;

    rw_lock_s_lock(&block->lock);
    left_page_no = btr_page_get_prev(buf_block_get_frame(block), mtr);
    rw_lock_s_unlock(&block->lock);

    if (left_page_no != FIL_NULL) {
      ut_ad(prev_n_blocks < leftmost_from_level);

      prev_tree_savepoints[prev_n_blocks] = mtr_set_savepoint(mtr);
      get_block =
          buf_page_get_gen(page_id_t(page_id.space(), left_page_no), page_size,
                           rw_latch, NULL, buf_mode, file, line, mtr);
      prev_tree_blocks[prev_n_blocks] = get_block;
      prev_n_blocks++;

      /* BTR_MODIFY_TREE doesn't update prev/next_page_no,
      without their parent page's lock. So, not needed to
      retry here, because we have the parent page's lock. */
    }

    /* release RW_NO_LATCH page and lock with RW_S_LATCH */
    mtr_release_block_at_savepoint(mtr, tree_savepoints[n_blocks],
                                   tree_blocks[n_blocks]);

    tree_savepoints[n_blocks] = mtr_set_savepoint(mtr);
    block = buf_page_get_gen(page_id, page_size, rw_latch, NULL, buf_mode, file,
                             line, mtr);
    tree_blocks[n_blocks] = block;
  }

  page = buf_block_get_frame(block);

  if (height == ULINT_UNDEFINED && page_is_leaf(page) &&
      rw_latch != RW_NO_LATCH && rw_latch != root_leaf_rw_latch) {
    /* We should retry to get the page, because the root page
    is latched with different level as a leaf page. */
    ut_ad(root_leaf_rw_latch != RW_NO_LATCH);
    ut_ad(rw_latch == RW_S_LATCH || rw_latch == RW_SX_LATCH);
    ut_ad(rw_latch == RW_S_LATCH || modify_external);

    ut_ad(n_blocks == 0);
    mtr_release_block_at_savepoint(mtr, tree_savepoints[n_blocks],
                                   tree_blocks[n_blocks]);

    upper_rw_latch = root_leaf_rw_latch;
    goto search_loop;
  }

  if (rw_latch != RW_NO_LATCH) {
#ifdef UNIV_ZIP_DEBUG
    const page_zip_des_t *page_zip = buf_block_get_page_zip(block);
    ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

    buf_block_dbg_add_level(block, dict_index_is_ibuf(index)
                                       ? SYNC_IBUF_TREE_NODE
                                       : SYNC_TREE_NODE);
  }

  ut_ad(fil_page_index_page_check(page));
  ut_ad(index->id == btr_page_get_index_id(page));

  if (UNIV_UNLIKELY(height == ULINT_UNDEFINED)) {
    /* We are in the root node */

    height = btr_page_get_level(page, mtr);
    root_height = height;
    cursor->tree_height = root_height + 1;

    if (dict_index_is_spatial(index)) {
      ut_ad(cursor->rtr_info);

      node_seq_t seq_no = rtr_get_current_ssn_id(index);

      /* If SSN in memory is not initialized, fetch
      it from root page */
      if (seq_no < 1) {
        node_seq_t root_seq_no;

        root_seq_no = page_get_ssn_id(page);

        mutex_enter(&(index->rtr_ssn.mutex));
        index->rtr_ssn.seq_no = root_seq_no + 1;
        mutex_exit(&(index->rtr_ssn.mutex));
      }

      /* Save the MBR */
      cursor->rtr_info->thr = cursor->thr;
      rtr_get_mbr_from_tuple(tuple, &cursor->rtr_info->mbr);
    }

#ifdef BTR_CUR_ADAPT
    if (block != guess) {
      info->root_guess = block;
      info->withdraw_clock = buf_withdraw_clock;
    }
#endif
  }

  if (height == 0) {
    if (rw_latch == RW_NO_LATCH) {
      latch_leaves = btr_cur_latch_leaves(block, page_id, page_size, latch_mode,
                                          cursor, mtr);
    }

    switch (latch_mode) {
      case BTR_MODIFY_TREE:
      case BTR_CONT_MODIFY_TREE:
      case BTR_CONT_SEARCH_TREE:
        break;
      default:
        if (!s_latch_by_caller && !srv_read_only_mode && !modify_external) {
          /* Release the tree s-latch */
          /* NOTE: BTR_MODIFY_EXTERNAL
          needs to keep tree sx-latch */
          mtr_release_s_latch_at_savepoint(mtr, savepoint,
                                           dict_index_get_lock(index));
        }

        /* release upper blocks */
        if (retrying_for_search_prev) {
          for (; prev_n_releases < prev_n_blocks; prev_n_releases++) {
            mtr_release_block_at_savepoint(
                mtr, prev_tree_savepoints[prev_n_releases],
                prev_tree_blocks[prev_n_releases]);
          }
        }

        for (; n_releases < n_blocks; n_releases++) {
          if (n_releases == 0 && modify_external) {
            /* keep latch of root page */
            ut_ad(mtr_memo_contains_flagged(
                mtr, tree_blocks[n_releases],
                MTR_MEMO_PAGE_SX_FIX | MTR_MEMO_PAGE_X_FIX));
            continue;
          }

          mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                         tree_blocks[n_releases]);
        }
    }

    page_mode = mode;
  }

  if (dict_index_is_spatial(index)) {
    /* Remember the page search mode */
    search_mode = page_mode;

    /* Some adjustment on search mode, when the
    page search mode is PAGE_CUR_RTREE_LOCATE
    or PAGE_CUR_RTREE_INSERT, as we are searching
    with MBRs. When it is not the target level, we
    should search all sub-trees that "CONTAIN" the
    search range/MBR. When it is at the target
    level, the search becomes PAGE_CUR_LE */
    if (page_mode == PAGE_CUR_RTREE_LOCATE && level == height) {
      if (level == 0) {
        page_mode = PAGE_CUR_LE;
      } else {
        page_mode = PAGE_CUR_RTREE_GET_FATHER;
      }
    }

    if (page_mode == PAGE_CUR_RTREE_INSERT) {
      page_mode = (level == height) ? PAGE_CUR_LE : PAGE_CUR_RTREE_INSERT;

      ut_ad(!page_is_leaf(page) || page_mode == PAGE_CUR_LE);
    }

    /* "need_path" indicates if we need to tracking the parent
    pages, if it is not spatial comparison, then no need to
    track it */
    if (page_mode < PAGE_CUR_CONTAIN) {
      need_path = false;
    }

    up_match = 0;
    low_match = 0;

    if (latch_mode == BTR_MODIFY_TREE || latch_mode == BTR_CONT_MODIFY_TREE ||
        latch_mode == BTR_CONT_SEARCH_TREE) {
      /* Tree are locked, no need for Page Lock to protect
      the "path" */
      cursor->rtr_info->need_page_lock = false;
    }
  }

  if (dict_index_is_spatial(index) && page_mode >= PAGE_CUR_CONTAIN) {
    ut_ad(need_path);
    found = rtr_cur_search_with_match(block, index, tuple, page_mode,
                                      page_cursor, cursor->rtr_info);

    /* Need to use BTR_MODIFY_TREE to do the MBR adjustment */
    if (search_mode == PAGE_CUR_RTREE_INSERT && cursor->rtr_info->mbr_adj) {
      if (latch_mode & BTR_MODIFY_LEAF) {
        /* Parent MBR needs updated, should retry
        with BTR_MODIFY_TREE */
        goto func_exit;
      } else if (latch_mode & BTR_MODIFY_TREE) {
        rtree_parent_modified = true;
        cursor->rtr_info->mbr_adj = false;
        mbr_adj = true;
      } else {
        ut_ad(0);
      }
    }

    if (found && page_mode == PAGE_CUR_RTREE_GET_FATHER) {
      cursor->low_match = DICT_INDEX_SPATIAL_NODEPTR_SIZE + 1;
    }
  } else if (height == 0 && btr_search_enabled &&
             !dict_index_is_spatial(index)) {
    /* The adaptive hash index is only used when searching
    for leaf pages (height==0), but not in r-trees.
    We only need the byte prefix comparison for the purpose
    of updating the adaptive hash index. */
    page_cur_search_with_match_bytes(block, index, tuple, page_mode, &up_match,
                                     &up_bytes, &low_match, &low_bytes,
                                     page_cursor);
  } else {
    /* Search for complete index fields. */
    up_bytes = low_bytes = 0;
    page_cur_search_with_match(block, index, tuple, page_mode, &up_match,
                               &low_match, page_cursor,
                               need_path ? cursor->rtr_info : NULL);
  }

  if (estimate) {
    btr_cur_add_path_info(cursor, height, root_height);
  }

  /* If this is the desired level, leave the loop */

  ut_ad(height == btr_page_get_level(page_cur_get_page(page_cursor), mtr));

  /* Add Predicate lock if it is serializable isolation
  and only if it is in the search case */
  if (dict_index_is_spatial(index) && cursor->rtr_info->need_prdt_lock &&
      mode != PAGE_CUR_RTREE_INSERT && mode != PAGE_CUR_RTREE_LOCATE &&
      mode >= PAGE_CUR_CONTAIN) {
    trx_t *trx = thr_get_trx(cursor->thr);
    lock_prdt_t prdt;

    lock_mutex_enter();
    lock_init_prdt_from_mbr(&prdt, &cursor->rtr_info->mbr, mode,
                            trx->lock.lock_heap);
    lock_mutex_exit();

    if (rw_latch == RW_NO_LATCH && height != 0) {
      rw_lock_s_lock(&(block->lock));
    }

    lock_prdt_lock(block, &prdt, index, LOCK_S, LOCK_PREDICATE, cursor->thr,
                   mtr);

    if (rw_latch == RW_NO_LATCH && height != 0) {
      rw_lock_s_unlock(&(block->lock));
    }
  }

  if (level != height) {
    const rec_t *node_ptr;
    ut_ad(height > 0);

    height--;
    guess = NULL;

    node_ptr = page_cur_get_rec(page_cursor);

    offsets = rec_get_offsets(node_ptr, index, offsets, ULINT_UNDEFINED, &heap);

    /* If the rec is the first or last in the page for
    pessimistic delete intention, it might cause node_ptr insert
    for the upper level. We should change the intention and retry.
    */
    if (latch_mode == BTR_MODIFY_TREE &&
        btr_cur_need_opposite_intention(page, lock_intention, node_ptr)) {
    need_opposite_intention:
      ut_ad(upper_rw_latch == RW_X_LATCH);

      if (n_releases > 0) {
        /* release root block */
        mtr_release_block_at_savepoint(mtr, tree_savepoints[0], tree_blocks[0]);
      }

      /* release all blocks */
      for (; n_releases <= n_blocks; n_releases++) {
        mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                       tree_blocks[n_releases]);
      }

      lock_intention = BTR_INTENTION_BOTH;

      page_id.reset(space, dict_index_get_page(index));
      up_match = 0;
      low_match = 0;
      height = ULINT_UNDEFINED;

      n_blocks = 0;
      n_releases = 0;

      goto search_loop;
    }

    if (dict_index_is_spatial(index)) {
      if (page_rec_is_supremum(node_ptr)) {
        cursor->low_match = 0;
        cursor->up_match = 0;
        goto func_exit;
      }

      /* If we are doing insertion or record locating,
      remember the tree nodes we visited */
      if (page_mode == PAGE_CUR_RTREE_INSERT ||
          (search_mode == PAGE_CUR_RTREE_LOCATE &&
           (latch_mode != BTR_MODIFY_LEAF))) {
        bool add_latch = false;

        if (latch_mode == BTR_MODIFY_TREE && rw_latch == RW_NO_LATCH) {
          ut_ad(mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                          MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK));
          rw_lock_s_lock(&block->lock);
          add_latch = true;
        }

          /* Store the parent cursor location */
#ifdef UNIV_DEBUG
        ulint num_stored =
            rtr_store_parent_path(block, cursor, latch_mode, height + 1, mtr);
#else
        rtr_store_parent_path(block, cursor, latch_mode, height + 1, mtr);
#endif

        if (page_mode == PAGE_CUR_RTREE_INSERT) {
          btr_pcur_t *r_cursor =
              rtr_get_parent_cursor(cursor, height + 1, true);
          /* If it is insertion, there should
          be only one parent for each level
          traverse */
#ifdef UNIV_DEBUG
          ut_ad(num_stored == 1);
#endif

          node_ptr = btr_pcur_get_rec(r_cursor);
        }

        if (add_latch) {
          rw_lock_s_unlock(&block->lock);
        }

        ut_ad(!page_rec_is_supremum(node_ptr));
      }

      ut_ad(page_mode == search_mode || (page_mode == PAGE_CUR_WITHIN &&
                                         search_mode == PAGE_CUR_RTREE_LOCATE));

      page_mode = search_mode;
    }

    /* If the first or the last record of the page
    or the same key value to the first record or last record,
    the another page might be choosen when BTR_CONT_MODIFY_TREE.
    So, the parent page should not released to avoiding deadlock
    with blocking the another search with the same key value. */
    if (!detected_same_key_root && lock_intention == BTR_INTENTION_BOTH &&
        !dict_index_is_unique(index) && latch_mode == BTR_MODIFY_TREE &&
        (up_match >= rec_offs_n_fields(offsets) - 1 ||
         low_match >= rec_offs_n_fields(offsets) - 1)) {
      const rec_t *first_rec =
          page_rec_get_next_const(page_get_infimum_rec(page));
      ulint matched_fields;

      ut_ad(upper_rw_latch == RW_X_LATCH);

      if (node_ptr == first_rec || page_rec_is_last(node_ptr, page)) {
        detected_same_key_root = true;
      } else {
        matched_fields = 0;

        offsets2 =
            rec_get_offsets(first_rec, index, offsets2, ULINT_UNDEFINED, &heap);
        cmp_rec_rec_with_match(node_ptr, first_rec, offsets, offsets2, index,
                               FALSE, &matched_fields);

        if (matched_fields >= rec_offs_n_fields(offsets) - 1) {
          detected_same_key_root = true;
        } else {
          const rec_t *last_rec;

          last_rec = page_rec_get_prev_const(page_get_supremum_rec(page));

          matched_fields = 0;

          offsets2 = rec_get_offsets(last_rec, index, offsets2, ULINT_UNDEFINED,
                                     &heap);
          cmp_rec_rec_with_match(node_ptr, last_rec, offsets, offsets2, index,
                                 FALSE, &matched_fields);
          if (matched_fields >= rec_offs_n_fields(offsets) - 1) {
            detected_same_key_root = true;
          }
        }
      }
    }

    /* If the page might cause modify_tree,
    we should not release the parent page's lock. */
    if (!detected_same_key_root && latch_mode == BTR_MODIFY_TREE &&
        !btr_cur_will_modify_tree(index, page, lock_intention, node_ptr,
                                  node_ptr_max_size, page_size, mtr) &&
        !rtree_parent_modified) {
      ut_ad(upper_rw_latch == RW_X_LATCH);
      ut_ad(n_releases <= n_blocks);

      /* we can release upper blocks */
      for (; n_releases < n_blocks; n_releases++) {
        if (n_releases == 0) {
          /* we should not release root page
          to pin to same block. */
          continue;
        }

        /* release unused blocks to unpin */
        mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                       tree_blocks[n_releases]);
      }
    }

    if (height == level && latch_mode == BTR_MODIFY_TREE) {
      ut_ad(upper_rw_latch == RW_X_LATCH);
      /* we should sx-latch root page, if released already.
      It contains seg_header. */
      if (n_releases > 0) {
        mtr_block_sx_latch_at_savepoint(mtr, tree_savepoints[0],
                                        tree_blocks[0]);
      }

      /* x-latch the branch blocks not released yet. */
      for (ulint i = n_releases; i <= n_blocks; i++) {
        mtr_block_x_latch_at_savepoint(mtr, tree_savepoints[i], tree_blocks[i]);
      }
    }

    /* We should consider prev_page of parent page, if the node_ptr
    is the leftmost of the page. because BTR_SEARCH_PREV and
    BTR_MODIFY_PREV latches prev_page of the leaf page. */
    if ((latch_mode == BTR_SEARCH_PREV || latch_mode == BTR_MODIFY_PREV) &&
        !retrying_for_search_prev) {
      /* block should be latched for consistent
         btr_page_get_prev() */
      ut_ad(mtr_memo_contains_flagged(
          mtr, block, MTR_MEMO_PAGE_S_FIX | MTR_MEMO_PAGE_X_FIX));

      if (btr_page_get_prev(page, mtr) != FIL_NULL &&
          page_rec_is_first(node_ptr, page)) {
        if (leftmost_from_level == 0) {
          leftmost_from_level = height + 1;
        }
      } else {
        leftmost_from_level = 0;
      }

      if (height == 0 && leftmost_from_level > 0) {
        /* should retry to get also prev_page
        from level==leftmost_from_level. */
        retrying_for_search_prev = true;

        prev_tree_blocks = static_cast<buf_block_t **>(
            ut_malloc_nokey(sizeof(buf_block_t *) * leftmost_from_level));

        prev_tree_savepoints = static_cast<ulint *>(
            ut_malloc_nokey(sizeof(ulint) * leftmost_from_level));

        /* back to the level (leftmost_from_level+1) */
        ulint idx = n_blocks - (leftmost_from_level - 1);

        page_id.reset(space, tree_blocks[idx]->page.id.page_no());

        for (ulint i = n_blocks - (leftmost_from_level - 1); i <= n_blocks;
             i++) {
          mtr_release_block_at_savepoint(mtr, tree_savepoints[i],
                                         tree_blocks[i]);
        }

        n_blocks -= (leftmost_from_level - 1);
        height = leftmost_from_level;
        ut_ad(n_releases == 0);

        /* replay up_match, low_match */
        up_match = 0;
        low_match = 0;
        rtr_info_t *rtr_info = need_path ? cursor->rtr_info : NULL;

        for (ulint i = 0; i < n_blocks; i++) {
          page_cur_search_with_match(tree_blocks[i], index, tuple, page_mode,
                                     &up_match, &low_match, page_cursor,
                                     rtr_info);
        }

        goto search_loop;
      }
    }

    /* Go to the child node */
    page_id.reset(space, btr_node_ptr_get_child_page_no(node_ptr, offsets));

    n_blocks++;

    if (UNIV_UNLIKELY(height == 0 && dict_index_is_ibuf(index))) {
      /* We're doing a search on an ibuf tree and we're one
      level above the leaf page. */

      ut_ad(level == 0);

      buf_mode = BUF_GET;
      rw_latch = RW_NO_LATCH;
      goto retry_page_get;
    }

    if (dict_index_is_spatial(index) && page_mode >= PAGE_CUR_CONTAIN &&
        page_mode != PAGE_CUR_RTREE_INSERT) {
      ut_ad(need_path);
      rtr_node_path_t *path = cursor->rtr_info->path;

      if (!path->empty() && found) {
#ifdef UNIV_DEBUG
        node_visit_t last_visit = path->back();

        ut_ad(last_visit.page_no == page_id.page_no());
#endif /* UNIV_DEBUG */

        path->pop_back();

#ifdef UNIV_DEBUG
        if (page_mode == PAGE_CUR_RTREE_LOCATE &&
            (latch_mode != BTR_MODIFY_LEAF)) {
          btr_pcur_t *cur = cursor->rtr_info->parent_path->back().cursor;
          rec_t *my_node_ptr = btr_pcur_get_rec(cur);

          offsets = rec_get_offsets(my_node_ptr, index, offsets,
                                    ULINT_UNDEFINED, &heap);

          page_no_t my_page_no =
              btr_node_ptr_get_child_page_no(my_node_ptr, offsets);

          ut_ad(page_id.page_no() == my_page_no);
        }
#endif
      }
    }

    goto search_loop;
  } else if (!dict_index_is_spatial(index) && latch_mode == BTR_MODIFY_TREE &&
             lock_intention == BTR_INTENTION_INSERT &&
             mach_read_from_4(page + FIL_PAGE_NEXT) != FIL_NULL &&
             page_rec_is_last(page_cur_get_rec(page_cursor), page)) {
    /* btr_insert_into_right_sibling() might cause
    deleting node_ptr at upper level */

    guess = NULL;

    if (height == 0) {
      /* release the leaf pages if latched */
      for (uint i = 0; i < 3; i++) {
        if (latch_leaves.blocks[i] != NULL) {
          mtr_release_block_at_savepoint(mtr, latch_leaves.savepoints[i],
                                         latch_leaves.blocks[i]);
          latch_leaves.blocks[i] = NULL;
        }
      }
    }

    goto need_opposite_intention;
  }

  if (level != 0) {
    if (upper_rw_latch == RW_NO_LATCH) {
      /* latch the page */
      buf_block_t *child_block;

      if (latch_mode == BTR_CONT_MODIFY_TREE) {
        child_block = btr_block_get(page_id, page_size, RW_X_LATCH, index, mtr);
      } else {
        ut_ad(latch_mode == BTR_CONT_SEARCH_TREE);
        child_block =
            btr_block_get(page_id, page_size, RW_SX_LATCH, index, mtr);
      }

      btr_assert_not_corrupted(child_block, index);
    } else {
      ut_ad(mtr_memo_contains(mtr, block, upper_rw_latch));
      btr_assert_not_corrupted(block, index);

      if (s_latch_by_caller) {
        ut_ad(latch_mode == BTR_SEARCH_TREE);
        /* to exclude modifying tree operations
        should sx-latch the index. */
        ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
                                MTR_MEMO_SX_LOCK));
        /* because has sx-latch of index,
        can release upper blocks. */
        for (; n_releases < n_blocks; n_releases++) {
          mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                         tree_blocks[n_releases]);
        }
      }
    }

    if (page_mode <= PAGE_CUR_LE) {
      cursor->low_match = low_match;
      cursor->up_match = up_match;
    }
  } else {
    cursor->low_match = low_match;
    cursor->low_bytes = low_bytes;
    cursor->up_match = up_match;
    cursor->up_bytes = up_bytes;

#ifdef BTR_CUR_ADAPT
    /* We do a dirty read of btr_search_enabled here.  We
    will properly check btr_search_enabled again in
    btr_search_build_page_hash_index() before building a
    page hash index, while holding search latch. */
    if (btr_search_enabled && !index->disable_ahi) {
      btr_search_info_update(index, cursor);
    }
#endif
    ut_ad(cursor->up_match != ULINT_UNDEFINED || mode != PAGE_CUR_GE);
    ut_ad(cursor->up_match != ULINT_UNDEFINED || mode != PAGE_CUR_LE);
    ut_ad(cursor->low_match != ULINT_UNDEFINED || mode != PAGE_CUR_LE);
  }

  /* For spatial index, remember  what blocks are still latched */
  if (dict_index_is_spatial(index) &&
      (latch_mode == BTR_MODIFY_TREE || latch_mode == BTR_MODIFY_LEAF)) {
    for (ulint i = 0; i < n_releases; i++) {
      cursor->rtr_info->tree_blocks[i] = NULL;
      cursor->rtr_info->tree_savepoints[i] = 0;
    }

    for (ulint i = n_releases; i <= n_blocks; i++) {
      cursor->rtr_info->tree_blocks[i] = tree_blocks[i];
      cursor->rtr_info->tree_savepoints[i] = tree_savepoints[i];
    }
  }

func_exit:

  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }

  if (retrying_for_search_prev) {
    ut_free(prev_tree_blocks);
    ut_free(prev_tree_savepoints);
  }

  if (has_search_latch) {
    rw_lock_s_lock(btr_get_search_latch(index));
  }

  if (mbr_adj) {
    /* remember that we will need to adjust parent MBR */
    cursor->rtr_info->mbr_adj = true;
  }

  DBUG_VOID_RETURN;
}

/** Searches an index tree and positions a tree cursor on a given level.
This function will avoid latching the traversal path and so should be
used only for cases where-in latching is not needed.

@param[in,out]	index	index
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
void btr_cur_search_to_nth_level_with_no_latch(dict_index_t *index, ulint level,
                                               const dtuple_t *tuple,
                                               page_cur_mode_t mode,
                                               btr_cur_t *cursor,
                                               const char *file, ulint line,
                                               mtr_t *mtr, bool mark_dirty) {
  page_t *page = NULL; /* remove warning */
  buf_block_t *block;
  ulint height;
  ulint up_match;
  ulint low_match;
  ulint rw_latch;
  page_cur_mode_t page_mode;
  ulint buf_mode;
  page_cur_t *page_cursor;
  ulint root_height = 0; /* remove warning */
  ulint n_blocks = 0;

  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  DBUG_ENTER("btr_cur_search_to_nth_level_with_no_latch");

  ut_ad(index->table->is_intrinsic());
  ut_ad(level == 0 || mode == PAGE_CUR_LE);
  ut_ad(dict_index_check_search_tuple(index, tuple));

  UNIV_MEM_INVALID(&cursor->up_match, sizeof cursor->up_match);
  UNIV_MEM_INVALID(&cursor->low_match, sizeof cursor->low_match);
#ifdef UNIV_DEBUG
  cursor->up_match = ULINT_UNDEFINED;
  cursor->low_match = ULINT_UNDEFINED;
#endif /* UNIV_DEBUG */

  cursor->flag = BTR_CUR_BINARY;
  cursor->index = index;

  page_cursor = btr_cur_get_page_cur(cursor);

  const space_id_t space = dict_index_get_space(index);
  const page_size_t page_size(dict_table_page_size(index->table));
  /* Start with the root page. */
  page_id_t page_id(space, dict_index_get_page(index));

  up_match = 0;
  low_match = 0;

  height = ULINT_UNDEFINED;

  /* We use these modified search modes on non-leaf levels of the
  B-tree. These let us end up in the right B-tree leaf. In that leaf
  we use the original search mode. */

  switch (mode) {
    case PAGE_CUR_GE:
      page_mode = PAGE_CUR_L;
      break;
    case PAGE_CUR_G:
      page_mode = PAGE_CUR_LE;
      break;
    default:
      page_mode = mode;
      break;
  }

  /* Loop and search until we arrive at the desired level */
  bool at_desired_level = false;
  while (!at_desired_level) {
    buf_mode = BUF_GET;
    rw_latch = RW_NO_LATCH;

    ut_ad(n_blocks < BTR_MAX_LEVELS);

    block = buf_page_get_gen(page_id, page_size, rw_latch, NULL, buf_mode, file,
                             line, mtr, mark_dirty);

    page = buf_block_get_frame(block);

    if (height == ULINT_UNDEFINED) {
      /* We are in the root node */

      height = btr_page_get_level(page, mtr);
      root_height = height;
      cursor->tree_height = root_height + 1;
    }

    if (height == 0) {
      /* On leaf level. Switch back to original search mode.*/
      page_mode = mode;
    }

    page_cur_search_with_match(block, index, tuple, page_mode, &up_match,
                               &low_match, page_cursor, NULL);

    ut_ad(height == btr_page_get_level(page_cur_get_page(page_cursor), mtr));

    if (level != height) {
      const rec_t *node_ptr;
      ut_ad(height > 0);

      height--;

      node_ptr = page_cur_get_rec(page_cursor);

      offsets =
          rec_get_offsets(node_ptr, index, offsets, ULINT_UNDEFINED, &heap);

      /* Go to the child node */
      page_id.reset(space, btr_node_ptr_get_child_page_no(node_ptr, offsets));

      n_blocks++;
    } else {
      /* If this is the desired level, leave the loop */
      at_desired_level = true;
    }
  }

  cursor->low_match = low_match;
  cursor->up_match = up_match;

  if (heap != NULL) {
    mem_heap_free(heap);
  }

  DBUG_VOID_RETURN;
}

/** Opens a cursor at either end of an index. */
void btr_cur_open_at_index_side_func(
    bool from_left,      /*!< in: true if open to the low end,
                         false if to the high end */
    dict_index_t *index, /*!< in: index */
    ulint latch_mode,    /*!< in: latch mode */
    btr_cur_t *cursor,   /*!< in/out: cursor */
    ulint level,         /*!< in: level to search for
                         (0=leaf). */
    const char *file,    /*!< in: file name */
    ulint line,          /*!< in: line where called */
    mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  page_cur_t *page_cursor;
  ulint node_ptr_max_size = UNIV_PAGE_SIZE / 2;
  ulint height;
  ulint root_height = 0; /* remove warning */
  rec_t *node_ptr;
  ulint estimate;
  ulint savepoint;
  ulint upper_rw_latch, root_leaf_rw_latch;
  btr_intention_t lock_intention;
  buf_block_t *tree_blocks[BTR_MAX_LEVELS];
  ulint tree_savepoints[BTR_MAX_LEVELS];
  ulint n_blocks = 0;
  ulint n_releases = 0;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  estimate = latch_mode & BTR_ESTIMATE;
  latch_mode &= ~BTR_ESTIMATE;

  ut_ad(level != ULINT_UNDEFINED);

  bool s_latch_by_caller;

  s_latch_by_caller = latch_mode & BTR_ALREADY_S_LATCHED;
  latch_mode &= ~BTR_ALREADY_S_LATCHED;

  lock_intention = btr_cur_get_and_clear_intention(&latch_mode);

  ut_ad(!(latch_mode & BTR_MODIFY_EXTERNAL));

  /* This function doesn't need to lock left page of the leaf page */
  if (latch_mode == BTR_SEARCH_PREV) {
    latch_mode = BTR_SEARCH_LEAF;
  } else if (latch_mode == BTR_MODIFY_PREV) {
    latch_mode = BTR_MODIFY_LEAF;
  }

  /* Store the position of the tree latch we push to mtr so that we
  know how to release it when we have latched the leaf node */

  savepoint = mtr_set_savepoint(mtr);

  switch (latch_mode) {
    case BTR_CONT_MODIFY_TREE:
    case BTR_CONT_SEARCH_TREE:
      upper_rw_latch = RW_NO_LATCH;
      break;
    case BTR_MODIFY_TREE:
      /* Most of delete-intended operations are purging.
      Free blocks and read IO bandwidth should be prior
      for them, when the history list is glowing huge. */
      if (lock_intention == BTR_INTENTION_DELETE &&
          trx_sys->rseg_history_len > BTR_CUR_FINE_HISTORY_LENGTH &&
          buf_get_n_pending_read_ios()) {
        mtr_x_lock(dict_index_get_lock(index), mtr);
      } else {
        mtr_sx_lock(dict_index_get_lock(index), mtr);
      }
      upper_rw_latch = RW_X_LATCH;
      break;
    default:
      ut_ad(!s_latch_by_caller ||
            mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                      MTR_MEMO_SX_LOCK | MTR_MEMO_S_LOCK));
      if (!srv_read_only_mode) {
        if (!s_latch_by_caller) {
          /* BTR_SEARCH_TREE is intended to be used with
          BTR_ALREADY_S_LATCHED */
          ut_ad(latch_mode != BTR_SEARCH_TREE);

          mtr_s_lock(dict_index_get_lock(index), mtr);
        }
        upper_rw_latch = RW_S_LATCH;
      } else {
        upper_rw_latch = RW_NO_LATCH;
      }
  }
  root_leaf_rw_latch = btr_cur_latch_for_root_leaf(latch_mode);

  page_cursor = btr_cur_get_page_cur(cursor);
  cursor->index = index;

  page_id_t page_id(dict_index_get_space(index), dict_index_get_page(index));
  const page_size_t &page_size = dict_table_page_size(index->table);

  if (root_leaf_rw_latch == RW_X_LATCH) {
    node_ptr_max_size = dict_index_node_ptr_max_size(index);
  }

  height = ULINT_UNDEFINED;

  for (;;) {
    buf_block_t *block;
    page_t *page;
    ulint rw_latch;

    ut_ad(n_blocks < BTR_MAX_LEVELS);

    if (height != 0 && (latch_mode != BTR_MODIFY_TREE || height == level)) {
      rw_latch = upper_rw_latch;
    } else {
      rw_latch = RW_NO_LATCH;
    }

    tree_savepoints[n_blocks] = mtr_set_savepoint(mtr);
    block = buf_page_get_gen(page_id, page_size, rw_latch, NULL, BUF_GET, file,
                             line, mtr);
    tree_blocks[n_blocks] = block;

    page = buf_block_get_frame(block);

    if (height == ULINT_UNDEFINED && btr_page_get_level(page, mtr) == 0 &&
        rw_latch != RW_NO_LATCH && rw_latch != root_leaf_rw_latch) {
      /* We should retry to get the page, because the root page
      is latched with different level as a leaf page. */
      ut_ad(root_leaf_rw_latch != RW_NO_LATCH);
      ut_ad(rw_latch == RW_S_LATCH);

      ut_ad(n_blocks == 0);
      mtr_release_block_at_savepoint(mtr, tree_savepoints[n_blocks],
                                     tree_blocks[n_blocks]);

      upper_rw_latch = root_leaf_rw_latch;
      continue;
    }

    ut_ad(fil_page_index_page_check(page));
    ut_ad(index->id == btr_page_get_index_id(page));

    if (height == ULINT_UNDEFINED) {
      /* We are in the root node */

      height = btr_page_get_level(page, mtr);
      root_height = height;
      ut_a(height >= level);
    } else {
      /* TODO: flag the index corrupted if this fails */
      ut_ad(height == btr_page_get_level(page, mtr));
    }

    if (height == level) {
      if (srv_read_only_mode) {
        btr_cur_latch_leaves(block, page_id, page_size, latch_mode, cursor,
                             mtr);
      } else if (height == 0) {
        if (rw_latch == RW_NO_LATCH) {
          btr_cur_latch_leaves(block, page_id, page_size, latch_mode, cursor,
                               mtr);
        }
        /* In versions <= 3.23.52 we had
        forgotten to release the tree latch
        here. If in an index scan we had to
        scan far to find a record visible to
        the current transaction, that could
        starve others waiting for the tree
        latch. */

        switch (latch_mode) {
          case BTR_MODIFY_TREE:
          case BTR_CONT_MODIFY_TREE:
          case BTR_CONT_SEARCH_TREE:
            break;
          default:
            if (!s_latch_by_caller) {
              /* Release the tree s-latch */
              mtr_release_s_latch_at_savepoint(mtr, savepoint,
                                               dict_index_get_lock(index));
            }

            /* release upper blocks */
            for (; n_releases < n_blocks; n_releases++) {
              mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                             tree_blocks[n_releases]);
            }
        }
      } else { /* height != 0 */
        /* We already have the block latched. */
        ut_ad(latch_mode == BTR_SEARCH_TREE);
        ut_ad(s_latch_by_caller);
        ut_ad(upper_rw_latch == RW_S_LATCH);

        ut_ad(mtr_memo_contains(mtr, block, upper_rw_latch));

        if (s_latch_by_caller) {
          /* to exclude modifying tree operations
          should sx-latch the index. */
          ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
                                  MTR_MEMO_SX_LOCK));
          /* because has sx-latch of index,
          can release upper blocks. */
          for (; n_releases < n_blocks; n_releases++) {
            mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                           tree_blocks[n_releases]);
          }
        }
      }
    }

    if (from_left) {
      page_cur_set_before_first(block, page_cursor);
    } else {
      page_cur_set_after_last(block, page_cursor);
    }

    if (height == level) {
      if (estimate) {
        btr_cur_add_path_info(cursor, height, root_height);
      }

      break;
    }

    ut_ad(height > 0);

    if (from_left) {
      page_cur_move_to_next(page_cursor);
    } else {
      page_cur_move_to_prev(page_cursor);
    }

    if (estimate) {
      btr_cur_add_path_info(cursor, height, root_height);
    }

    height--;

    node_ptr = page_cur_get_rec(page_cursor);
    offsets = rec_get_offsets(node_ptr, cursor->index, offsets, ULINT_UNDEFINED,
                              &heap);

    /* If the rec is the first or last in the page for
    pessimistic delete intention, it might cause node_ptr insert
    for the upper level. We should change the intention and retry.
    */
    if (latch_mode == BTR_MODIFY_TREE &&
        btr_cur_need_opposite_intention(page, lock_intention, node_ptr)) {
      ut_ad(upper_rw_latch == RW_X_LATCH);
      /* release all blocks */
      for (; n_releases <= n_blocks; n_releases++) {
        mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                       tree_blocks[n_releases]);
      }

      lock_intention = BTR_INTENTION_BOTH;

      page_id.set_page_no(dict_index_get_page(index));

      height = ULINT_UNDEFINED;

      n_blocks = 0;
      n_releases = 0;

      continue;
    }

    if (latch_mode == BTR_MODIFY_TREE &&
        !btr_cur_will_modify_tree(cursor->index, page, lock_intention, node_ptr,
                                  node_ptr_max_size, page_size, mtr)) {
      ut_ad(upper_rw_latch == RW_X_LATCH);
      ut_ad(n_releases <= n_blocks);

      /* we can release upper blocks */
      for (; n_releases < n_blocks; n_releases++) {
        if (n_releases == 0) {
          /* we should not release root page
          to pin to same block. */
          continue;
        }

        /* release unused blocks to unpin */
        mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                       tree_blocks[n_releases]);
      }
    }

    if (height == level && latch_mode == BTR_MODIFY_TREE) {
      ut_ad(upper_rw_latch == RW_X_LATCH);
      /* we should sx-latch root page, if released already.
      It contains seg_header. */
      if (n_releases > 0) {
        mtr_block_sx_latch_at_savepoint(mtr, tree_savepoints[0],
                                        tree_blocks[0]);
      }

      /* x-latch the branch blocks not released yet. */
      for (ulint i = n_releases; i <= n_blocks; i++) {
        mtr_block_x_latch_at_savepoint(mtr, tree_savepoints[i], tree_blocks[i]);
      }
    }

    /* Go to the child node */
    page_id.set_page_no(btr_node_ptr_get_child_page_no(node_ptr, offsets));

    n_blocks++;
  }

  if (heap) {
    mem_heap_free(heap);
  }
}

/** Opens a cursor at either end of an index.
Avoid taking latches on buffer, just pin (by incrementing fix_count)
to keep them in buffer pool. This mode is used by intrinsic table
as they are not shared and so there is no need of latching.
@param[in]	from_left	true if open to low end, false if open to high
                                end.
@param[in]	index		index
@param[in,out]	cursor		cursor
@param[in]	level		level to search for (0=leaf)
@param[in]	file		file name
@param[in]	line		line where called
@param[in,out]	mtr		mini transaction */
void btr_cur_open_at_index_side_with_no_latch_func(
    bool from_left, dict_index_t *index, btr_cur_t *cursor, ulint level,
    const char *file, ulint line, mtr_t *mtr) {
  page_cur_t *page_cursor;
  ulint height;
  rec_t *node_ptr;
  ulint n_blocks = 0;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(level != ULINT_UNDEFINED);

  page_cursor = btr_cur_get_page_cur(cursor);
  cursor->index = index;
  page_id_t page_id(dict_index_get_space(index), dict_index_get_page(index));
  const page_size_t &page_size = dict_table_page_size(index->table);

  height = ULINT_UNDEFINED;

  for (;;) {
    buf_block_t *block;
    page_t *page;
    ulint rw_latch = RW_NO_LATCH;

    ut_ad(n_blocks < BTR_MAX_LEVELS);

    block = buf_page_get_gen(page_id, page_size, rw_latch, NULL, BUF_GET, file,
                             line, mtr);

    page = buf_block_get_frame(block);

    ut_ad(fil_page_index_page_check(page));
    ut_ad(index->id == btr_page_get_index_id(page));

    if (height == ULINT_UNDEFINED) {
      /* We are in the root node */

      height = btr_page_get_level(page, mtr);
      ut_a(height >= level);
    } else {
      /* TODO: flag the index corrupted if this fails */
      ut_ad(height == btr_page_get_level(page, mtr));
    }

    if (from_left) {
      page_cur_set_before_first(block, page_cursor);
    } else {
      page_cur_set_after_last(block, page_cursor);
    }

    if (height == level) {
      break;
    }

    ut_ad(height > 0);

    if (from_left) {
      page_cur_move_to_next(page_cursor);
    } else {
      page_cur_move_to_prev(page_cursor);
    }

    height--;

    node_ptr = page_cur_get_rec(page_cursor);
    offsets = rec_get_offsets(node_ptr, cursor->index, offsets, ULINT_UNDEFINED,
                              &heap);

    /* Go to the child node */
    page_id.set_page_no(btr_node_ptr_get_child_page_no(node_ptr, offsets));

    n_blocks++;
  }

  if (heap != NULL) {
    mem_heap_free(heap);
  }
}

/** Positions a cursor at a randomly chosen position within a B-tree.
 @return true if the index is available and we have put the cursor, false
 if the index is unavailable */
bool btr_cur_open_at_rnd_pos_func(
    dict_index_t *index, /*!< in: index */
    ulint latch_mode,    /*!< in: BTR_SEARCH_LEAF, ... */
    btr_cur_t *cursor,   /*!< in/out: B-tree cursor */
    const char *file,    /*!< in: file name */
    ulint line,          /*!< in: line where called */
    mtr_t *mtr)          /*!< in: mtr */
{
  page_cur_t *page_cursor;
  ulint node_ptr_max_size = UNIV_PAGE_SIZE / 2;
  ulint height;
  rec_t *node_ptr;
  ulint savepoint;
  ulint upper_rw_latch, root_leaf_rw_latch;
  btr_intention_t lock_intention;
  buf_block_t *tree_blocks[BTR_MAX_LEVELS];
  ulint tree_savepoints[BTR_MAX_LEVELS];
  ulint n_blocks = 0;
  ulint n_releases = 0;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(!dict_index_is_spatial(index));

  lock_intention = btr_cur_get_and_clear_intention(&latch_mode);

  ut_ad(!(latch_mode & BTR_MODIFY_EXTERNAL));

  savepoint = mtr_set_savepoint(mtr);

  switch (latch_mode) {
    case BTR_MODIFY_TREE:
      /* Most of delete-intended operations are purging.
      Free blocks and read IO bandwidth should be prior
      for them, when the history list is glowing huge. */
      if (lock_intention == BTR_INTENTION_DELETE &&
          trx_sys->rseg_history_len > BTR_CUR_FINE_HISTORY_LENGTH &&
          buf_get_n_pending_read_ios()) {
        mtr_x_lock(dict_index_get_lock(index), mtr);
      } else {
        mtr_sx_lock(dict_index_get_lock(index), mtr);
      }
      upper_rw_latch = RW_X_LATCH;
      break;
    case BTR_SEARCH_PREV:
    case BTR_MODIFY_PREV:
      /* This function doesn't support left uncle
         page lock for left leaf page lock, when
         needed. */
    case BTR_SEARCH_TREE:
    case BTR_CONT_MODIFY_TREE:
    case BTR_CONT_SEARCH_TREE:
      ut_ad(0);
      /* fall through */
    default:
      if (!srv_read_only_mode) {
        mtr_s_lock(dict_index_get_lock(index), mtr);
        upper_rw_latch = RW_S_LATCH;
      } else {
        upper_rw_latch = RW_NO_LATCH;
      }
  }

  DBUG_EXECUTE_IF("test_index_is_unavailable", return (false););

  if (index->page == FIL_NULL) {
    /* Since we don't hold index lock until just now, the index
    could be modified by others, for example, if this is a
    statistics updater for referenced table, it could be marked
    as unavailable by 'DROP TABLE' in the mean time, since
    we don't hold lock for statistics updater */
    return (false);
  }

  root_leaf_rw_latch = btr_cur_latch_for_root_leaf(latch_mode);

  page_cursor = btr_cur_get_page_cur(cursor);
  cursor->index = index;

  page_id_t page_id(dict_index_get_space(index), dict_index_get_page(index));
  const page_size_t &page_size = dict_table_page_size(index->table);

  if (root_leaf_rw_latch == RW_X_LATCH) {
    node_ptr_max_size = dict_index_node_ptr_max_size(index);
  }

  height = ULINT_UNDEFINED;

  for (;;) {
    buf_block_t *block;
    page_t *page;
    ulint rw_latch;

    ut_ad(n_blocks < BTR_MAX_LEVELS);

    if (height != 0 && latch_mode != BTR_MODIFY_TREE) {
      rw_latch = upper_rw_latch;
    } else {
      rw_latch = RW_NO_LATCH;
    }

    tree_savepoints[n_blocks] = mtr_set_savepoint(mtr);
    block = buf_page_get_gen(page_id, page_size, rw_latch, NULL, BUF_GET, file,
                             line, mtr);
    tree_blocks[n_blocks] = block;

    page = buf_block_get_frame(block);

    if (height == ULINT_UNDEFINED && btr_page_get_level(page, mtr) == 0 &&
        rw_latch != RW_NO_LATCH && rw_latch != root_leaf_rw_latch) {
      /* We should retry to get the page, because the root page
      is latched with different level as a leaf page. */
      ut_ad(root_leaf_rw_latch != RW_NO_LATCH);
      ut_ad(rw_latch == RW_S_LATCH);

      ut_ad(n_blocks == 0);
      mtr_release_block_at_savepoint(mtr, tree_savepoints[n_blocks],
                                     tree_blocks[n_blocks]);

      upper_rw_latch = root_leaf_rw_latch;
      continue;
    }

    ut_ad(fil_page_index_page_check(page));
    ut_ad(index->id == btr_page_get_index_id(page));

    if (height == ULINT_UNDEFINED) {
      /* We are in the root node */

      height = btr_page_get_level(page, mtr);
    }

    if (height == 0) {
      if (rw_latch == RW_NO_LATCH || srv_read_only_mode) {
        btr_cur_latch_leaves(block, page_id, page_size, latch_mode, cursor,
                             mtr);
      }

      /* btr_cur_open_at_index_side_func() and
      btr_cur_search_to_nth_level() release
      tree s-latch here.*/
      switch (latch_mode) {
        case BTR_MODIFY_TREE:
        case BTR_CONT_MODIFY_TREE:
        case BTR_CONT_SEARCH_TREE:
          break;
        default:
          /* Release the tree s-latch */
          if (!srv_read_only_mode) {
            mtr_release_s_latch_at_savepoint(mtr, savepoint,
                                             dict_index_get_lock(index));
          }

          /* release upper blocks */
          for (; n_releases < n_blocks; n_releases++) {
            mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                           tree_blocks[n_releases]);
          }
      }
    }

    page_cur_open_on_rnd_user_rec(block, page_cursor);

    if (height == 0) {
      break;
    }

    ut_ad(height > 0);

    height--;

    node_ptr = page_cur_get_rec(page_cursor);
    offsets = rec_get_offsets(node_ptr, cursor->index, offsets, ULINT_UNDEFINED,
                              &heap);

    /* If the rec is the first or last in the page for
    pessimistic delete intention, it might cause node_ptr insert
    for the upper level. We should change the intention and retry.
    */
    if (latch_mode == BTR_MODIFY_TREE &&
        btr_cur_need_opposite_intention(page, lock_intention, node_ptr)) {
      ut_ad(upper_rw_latch == RW_X_LATCH);
      /* release all blocks */
      for (; n_releases <= n_blocks; n_releases++) {
        mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                       tree_blocks[n_releases]);
      }

      lock_intention = BTR_INTENTION_BOTH;

      page_id.set_page_no(dict_index_get_page(index));

      height = ULINT_UNDEFINED;

      n_blocks = 0;
      n_releases = 0;

      continue;
    }

    if (latch_mode == BTR_MODIFY_TREE &&
        !btr_cur_will_modify_tree(cursor->index, page, lock_intention, node_ptr,
                                  node_ptr_max_size, page_size, mtr)) {
      ut_ad(upper_rw_latch == RW_X_LATCH);
      ut_ad(n_releases <= n_blocks);

      /* we can release upper blocks */
      for (; n_releases < n_blocks; n_releases++) {
        if (n_releases == 0) {
          /* we should not release root page
          to pin to same block. */
          continue;
        }

        /* release unused blocks to unpin */
        mtr_release_block_at_savepoint(mtr, tree_savepoints[n_releases],
                                       tree_blocks[n_releases]);
      }
    }

    if (height == 0 && latch_mode == BTR_MODIFY_TREE) {
      ut_ad(upper_rw_latch == RW_X_LATCH);
      /* we should sx-latch root page, if released already.
      It contains seg_header. */
      if (n_releases > 0) {
        mtr_block_sx_latch_at_savepoint(mtr, tree_savepoints[0],
                                        tree_blocks[0]);
      }

      /* x-latch the branch blocks not released yet. */
      for (ulint i = n_releases; i <= n_blocks; i++) {
        mtr_block_x_latch_at_savepoint(mtr, tree_savepoints[i], tree_blocks[i]);
      }
    }

    /* Go to the child node */
    page_id.set_page_no(btr_node_ptr_get_child_page_no(node_ptr, offsets));

    n_blocks++;
  }

  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }

  return (true);
}

/*==================== B-TREE INSERT =========================*/

/** Inserts a record if there is enough space, or if enough space can
 be freed by reorganizing. Differs from btr_cur_optimistic_insert because
 no heuristics is applied to whether it pays to use CPU time for
 reorganizing the page or not.

 IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
 if this is a compressed leaf page in a secondary index.
 This has to be done either within the same mini-transaction,
 or by invoking ibuf_reset_free_bits() before mtr_commit().

 @return pointer to inserted record if succeed, else NULL */
static MY_ATTRIBUTE((warn_unused_result)) rec_t *btr_cur_insert_if_possible(
    btr_cur_t *cursor,     /*!< in: cursor on page after which to insert;
                           cursor stays valid */
    const dtuple_t *tuple, /*!< in: tuple to insert; the size info need not
                           have been stored to tuple */
    ulint **offsets,       /*!< out: offsets on *rec */
    mem_heap_t **heap,     /*!< in/out: pointer to memory heap, or NULL */
    ulint n_ext,           /*!< in: number of externally stored columns */
    mtr_t *mtr)            /*!< in/out: mini-transaction */
{
  page_cur_t *page_cursor;
  rec_t *rec;

  ut_ad(dtuple_check_typed(tuple));

  ut_ad(mtr_is_block_fix(mtr, btr_cur_get_block(cursor), MTR_MEMO_PAGE_X_FIX,
                         cursor->index->table));
  page_cursor = btr_cur_get_page_cur(cursor);

  /* Now, try the insert */
  rec = page_cur_tuple_insert(page_cursor, tuple, cursor->index, offsets, heap,
                              n_ext, mtr);

  /* If the record did not fit, reorganize.
  For compressed pages, page_cur_tuple_insert()
  attempted this already. */
  if (!rec && !page_cur_get_page_zip(page_cursor) &&
      btr_page_reorganize(page_cursor, cursor->index, mtr)) {
    rec = page_cur_tuple_insert(page_cursor, tuple, cursor->index, offsets,
                                heap, n_ext, mtr);
  }

  ut_ad(!rec || rec_offs_validate(rec, cursor->index, *offsets));
  return (rec);
}

/** For an insert, checks the locks and does the undo logging if desired.
 @return DB_SUCCESS, DB_WAIT_LOCK, DB_FAIL, or error number */
UNIV_INLINE MY_ATTRIBUTE((warn_unused_result)) dberr_t
    btr_cur_ins_lock_and_undo(
        ulint flags,       /*!< in: undo logging and locking flags: if
                           not zero, the parameters index and thr
                           should be specified */
        btr_cur_t *cursor, /*!< in: cursor on page after which to insert */
        dtuple_t *entry,   /*!< in/out: entry to insert */
        que_thr_t *thr,    /*!< in: query thread or NULL */
        mtr_t *mtr,        /*!< in/out: mini-transaction */
        ibool *inherit)    /*!< out: TRUE if the inserted new record maybe
                           should inherit LOCK_GAP type locks from the
                           successor record */
{
  dict_index_t *index;
  dberr_t err = DB_SUCCESS;
  rec_t *rec;
  roll_ptr_t roll_ptr;

  /* Check if we have to wait for a lock: enqueue an explicit lock
  request if yes */

  rec = btr_cur_get_rec(cursor);
  index = cursor->index;

  ut_ad(!dict_index_is_online_ddl(index) || index->is_clustered() ||
        (flags & BTR_CREATE_FLAG));

  /* Check if there is predicate or GAP lock preventing the insertion */
  if (!(flags & BTR_NO_LOCKING_FLAG)) {
    if (dict_index_is_spatial(index)) {
      lock_prdt_t prdt;
      rtr_mbr_t mbr;

      rtr_get_mbr_from_tuple(entry, &mbr);

      /* Use on stack MBR variable to test if a lock is
      needed. If so, the predicate (MBR) will be allocated
      from lock heap in lock_prdt_insert_check_and_lock() */
      lock_init_prdt_from_mbr(&prdt, &mbr, 0, NULL);

      err = lock_prdt_insert_check_and_lock(
          flags, rec, btr_cur_get_block(cursor), index, thr, mtr, &prdt);
      *inherit = false;
    } else {
      err = lock_rec_insert_check_and_lock(
          flags, rec, btr_cur_get_block(cursor), index, thr, mtr, inherit);
    }
  }

  if (err != DB_SUCCESS || !index->is_clustered() ||
      dict_index_is_ibuf(index)) {
    return (err);
  }

  err = trx_undo_report_row_operation(flags, TRX_UNDO_INSERT_OP, thr, index,
                                      entry, NULL, 0, NULL, NULL, &roll_ptr);
  if (err != DB_SUCCESS) {
    return (err);
  }

  /* Now we can fill in the roll ptr field in entry
  (except if table is intrinsic) */

  if (!(flags & BTR_KEEP_SYS_FLAG) && !index->table->is_intrinsic()) {
    /* Roll_ptr is zero during copy alter table.
    So pretend to be freshly inserted row. */
    if (index->table->skip_alter_undo) {
      ut_ad(roll_ptr == 0);
      roll_ptr = trx_undo_build_roll_ptr(TRUE, 0, 0, 0);
      ut_ad(roll_ptr == (1ULL << 55));
    }

    row_upd_index_entry_sys_field(entry, index, DATA_ROLL_PTR, roll_ptr);
  }

  return (DB_SUCCESS);
}

/**
Prefetch siblings of the leaf for the pessimistic operation.
@param block	leaf page */
static void btr_cur_prefetch_siblings(buf_block_t *block) {
  page_t *page = buf_block_get_frame(block);

  ut_ad(page_is_leaf(page));

  page_no_t left_page_no = fil_page_get_prev(page);
  page_no_t right_page_no = fil_page_get_next(page);

  if (left_page_no != FIL_NULL) {
    buf_read_page_background(page_id_t(block->page.id.space(), left_page_no),
                             block->page.size, false);
  }
  if (right_page_no != FIL_NULL) {
    buf_read_page_background(page_id_t(block->page.id.space(), right_page_no),
                             block->page.size, false);
  }
  if (left_page_no != FIL_NULL || right_page_no != FIL_NULL) {
    os_aio_simulated_wake_handler_threads();
  }
}

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
{
  big_rec_t *big_rec_vec = NULL;
  dict_index_t *index;
  page_cur_t *page_cursor;
  buf_block_t *block;
  page_t *page;
  rec_t *dummy;
  ibool leaf;
  ibool reorg;
  ibool inherit = TRUE;
  ulint rec_size;
  dberr_t err;

  *big_rec = NULL;

  block = btr_cur_get_block(cursor);
  page = buf_block_get_frame(block);
  index = cursor->index;

  /* Block are not latched for insert if table is intrinsic
  and index is auto-generated clustered index. */
  ut_ad(mtr_is_block_fix(mtr, block, MTR_MEMO_PAGE_X_FIX, index->table));
  ut_ad(!dict_index_is_online_ddl(index) || index->is_clustered() ||
        (flags & BTR_CREATE_FLAG));
  ut_ad(dtuple_check_typed(entry));

  const page_size_t &page_size = block->page.size;

#ifdef UNIV_DEBUG_VALGRIND
  if (page_size.is_compressed()) {
    UNIV_MEM_ASSERT_RW(page, page_size.logical());
    UNIV_MEM_ASSERT_RW(block->page.zip.data, page_size.physical());
  }
#endif /* UNIV_DEBUG_VALGRIND */

  leaf = page_is_leaf(page);

  /* Calculate the record size when entry is converted to a record */
  rec_size = rec_get_converted_size(index, entry, n_ext);

  if (page_zip_rec_needs_ext(rec_size, page_is_comp(page),
                             dtuple_get_n_fields(entry), page_size)) {
    /* The record is so big that we have to store some fields
    externally on separate database pages */
    big_rec_vec = dtuple_convert_big_rec(index, 0, entry, &n_ext);

    if (UNIV_UNLIKELY(big_rec_vec == NULL)) {
      return (DB_TOO_BIG_RECORD);
    }

    rec_size = rec_get_converted_size(index, entry, n_ext);
  }

  if (page_size.is_compressed() && page_zip_is_too_big(index, entry)) {
    if (big_rec_vec != NULL) {
      dtuple_convert_back_big_rec(index, entry, big_rec_vec);
    }

    return (DB_TOO_BIG_RECORD);
  }

  LIMIT_OPTIMISTIC_INSERT_DEBUG(page_get_n_recs(page), goto fail);

  if (leaf && page_size.is_compressed() &&
      (page_get_data_size(page) + rec_size >=
       dict_index_zip_pad_optimal_page_size(index))) {
    /* If compression padding tells us that insertion will
    result in too packed up page i.e.: which is likely to
    cause compression failure then don't do an optimistic
    insertion. */
  fail:
    err = DB_FAIL;

    /* prefetch siblings of the leaf for the pessimistic
    operation, if the page is leaf. */
    if (page_is_leaf(page)) {
      btr_cur_prefetch_siblings(block);
    }
  fail_err:

    if (big_rec_vec) {
      dtuple_convert_back_big_rec(index, entry, big_rec_vec);
    }

    return (err);
  }

  ulint max_size = page_get_max_insert_size_after_reorganize(page, 1);

  if (page_has_garbage(page)) {
    if ((max_size < rec_size || max_size < BTR_CUR_PAGE_REORGANIZE_LIMIT) &&
        page_get_n_recs(page) > 1 &&
        page_get_max_insert_size(page, 1) < rec_size) {
      goto fail;
    }
  } else if (max_size < rec_size) {
    goto fail;
  }

  /* If there have been many consecutive inserts to the
  clustered index leaf page of an uncompressed table, check if
  we have to split the page to reserve enough free space for
  future updates of records. */

  if (leaf && !page_size.is_compressed() && index->is_clustered() &&
      page_get_n_recs(page) >= 2 &&
      dict_index_get_space_reserve() + rec_size > max_size &&
      (btr_page_get_split_rec_to_right(cursor, &dummy) ||
       btr_page_get_split_rec_to_left(cursor, &dummy))) {
    goto fail;
  }

  page_cursor = btr_cur_get_page_cur(cursor);

  DBUG_PRINT(
      "ib_cur",
      ("insert %s (" IB_ID_FMT ") by " TRX_ID_FMT ": %s", index->name(),
       index->id, thr != NULL ? trx_get_id_for_print(thr_get_trx(thr)) : 0,
       rec_printer(entry).str().c_str()));

  DBUG_EXECUTE_IF("do_page_reorganize",
                  btr_page_reorganize(page_cursor, index, mtr););

  /* Now, try the insert */
  {
    const rec_t *page_cursor_rec = page_cur_get_rec(page_cursor);

    if (index->table->is_intrinsic()) {
      index->rec_cache.rec_size = rec_size;

      *rec =
          page_cur_tuple_direct_insert(page_cursor, entry, index, n_ext, mtr);
    } else {
      /* Check locks and write to the undo log,
      if specified */
      err = btr_cur_ins_lock_and_undo(flags, cursor, entry, thr, mtr, &inherit);

      if (err != DB_SUCCESS) {
        goto fail_err;
      }

      DBUG_EXECUTE_IF(
          "btr_ins_pause_on_mtr_redo_before_add_dirty_blocks",
          ut_ad(!debug_sync_set_action(
              current_thd, STRING_WITH_LEN("mtr_redo_before_add_dirty_blocks "
                                           "SIGNAL btr_ins_paused "
                                           "WAIT_FOR btr_ins_resume "
                                           "NO_CLEAR_EVENT"))););

      DBUG_EXECUTE_IF(
          "btr_ins_pause_on_mtr_noredo_before_add_dirty_blocks",
          ut_ad(!debug_sync_set_action(
              current_thd, STRING_WITH_LEN("mtr_noredo_before_add_dirty_blocks "
                                           "SIGNAL btr_ins_paused "
                                           "WAIT_FOR btr_ins_resume "
                                           "NO_CLEAR_EVENT"))););

      *rec = page_cur_tuple_insert(page_cursor, entry, index, offsets, heap,
                                   n_ext, mtr);
    }

    reorg = page_cursor_rec != page_cur_get_rec(page_cursor);
  }

  if (*rec) {
  } else if (page_size.is_compressed()) {
    ut_ad(!index->table->is_temporary());
    /* Reset the IBUF_BITMAP_FREE bits, because
    page_cur_tuple_insert() will have attempted page
    reorganize before failing. */
    if (leaf && !index->is_clustered()) {
      ibuf_reset_free_bits(block);
    }

    goto fail;
  } else {
    /* For intrinsic table we take a consistent path
    to re-organize using pessimistic path. */
    if (index->table->is_intrinsic()) {
      goto fail;
    }

    ut_ad(!reorg);

    /* If the record did not fit, reorganize */
    if (!btr_page_reorganize(page_cursor, index, mtr)) {
      ut_ad(0);
      goto fail;
    }

    ut_ad(page_get_max_insert_size(page, 1) == max_size);

    reorg = TRUE;

    *rec = page_cur_tuple_insert(page_cursor, entry, index, offsets, heap,
                                 n_ext, mtr);

    if (UNIV_UNLIKELY(!*rec)) {
      ib::fatal(ER_IB_MSG_44)
          << "Cannot insert tuple " << *entry << "into index " << index->name
          << " of table " << index->table->name << ". Max size: " << max_size;
    }
  }

#ifdef BTR_CUR_HASH_ADAPT
  if (!index->disable_ahi) {
    if (!reorg && leaf && (cursor->flag == BTR_CUR_HASH)) {
      btr_search_update_hash_node_on_insert(cursor);
    } else {
      btr_search_update_hash_on_insert(cursor);
    }
  }
#endif /* BTR_CUR_HASH_ADAPT */

  if (!(flags & BTR_NO_LOCKING_FLAG) && inherit) {
    lock_update_insert(block, *rec);
  }

  if (leaf && !index->is_clustered() && !index->table->is_temporary()) {
    /* Update the free bits of the B-tree page in the
    insert buffer bitmap. */

    /* The free bits in the insert buffer bitmap must
    never exceed the free space on a page.  It is safe to
    decrement or reset the bits in the bitmap in a
    mini-transaction that is committed before the
    mini-transaction that affects the free space. */

    /* It is unsafe to increment the bits in a separately
    committed mini-transaction, because in crash recovery,
    the free bits could momentarily be set too high. */

    if (page_size.is_compressed()) {
      /* Update the bits in the same mini-transaction. */
      ibuf_update_free_bits_zip(block, mtr);
    } else {
      /* Decrement the bits in a separate
      mini-transaction. */
      ibuf_update_free_bits_if_full(block, max_size,
                                    rec_size + PAGE_DIR_SLOT_SIZE);
    }
  }

  *big_rec = big_rec_vec;

  return (DB_SUCCESS);
}

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
{
  dict_index_t *index = cursor->index;
  big_rec_t *big_rec_vec = NULL;
  dberr_t err;
  ibool inherit = FALSE;
  bool success;
  ulint n_reserved = 0;

  ut_ad(dtuple_check_typed(entry));

  *big_rec = NULL;

  ut_ad(mtr_memo_contains_flagged(
            mtr, dict_index_get_lock(btr_cur_get_index(cursor)),
            MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
        cursor->index->table->is_intrinsic());
  ut_ad(mtr_is_block_fix(mtr, btr_cur_get_block(cursor), MTR_MEMO_PAGE_X_FIX,
                         cursor->index->table));
  ut_ad(!dict_index_is_online_ddl(index) || index->is_clustered() ||
        (flags & BTR_CREATE_FLAG));

  cursor->flag = BTR_CUR_BINARY;

  /* Check locks and write to undo log, if specified */

  err = btr_cur_ins_lock_and_undo(flags, cursor, entry, thr, mtr, &inherit);

  if (err != DB_SUCCESS) {
    return (err);
  }

  if (!(flags & BTR_NO_UNDO_LOG_FLAG) || index->table->is_intrinsic()) {
    /* First reserve enough free space for the file segments
    of the index tree, so that the insert will not fail because
    of lack of space */

    ulint n_extents = cursor->tree_height / 16 + 3;

    success = fsp_reserve_free_extents(&n_reserved, index->space, n_extents,
                                       FSP_NORMAL, mtr);
    if (!success) {
      return (DB_OUT_OF_FILE_SPACE);
    }
  }

  if (page_zip_rec_needs_ext(rec_get_converted_size(index, entry, n_ext),
                             dict_table_is_comp(index->table),
                             dtuple_get_n_fields(entry),
                             dict_table_page_size(index->table))) {
    /* The record is so big that we have to store some fields
    externally on separate database pages */

    if (UNIV_LIKELY_NULL(big_rec_vec)) {
      /* This should never happen, but we handle
      the situation in a robust manner. */
      ut_ad(0);
      dtuple_convert_back_big_rec(index, entry, big_rec_vec);
    }

    big_rec_vec = dtuple_convert_big_rec(index, 0, entry, &n_ext);

    if (big_rec_vec == NULL) {
      if (n_reserved > 0) {
        fil_space_release_free_extents(index->space, n_reserved);
      }
      return (DB_TOO_BIG_RECORD);
    }
  }

  if (dict_index_get_page(index) ==
      btr_cur_get_block(cursor)->page.id.page_no()) {
    /* The page is the root page */
    *rec = btr_root_raise_and_insert(flags, cursor, offsets, heap, entry, n_ext,
                                     mtr);
  } else {
    *rec = btr_page_split_and_insert(flags, cursor, offsets, heap, entry, n_ext,
                                     mtr);
  }

  ut_ad(page_rec_get_next(btr_cur_get_rec(cursor)) == *rec ||
        dict_index_is_spatial(index));

  if (!(flags & BTR_NO_LOCKING_FLAG)) {
    ut_ad(!index->table->is_temporary());
    if (dict_index_is_spatial(index)) {
      /* Do nothing */
    } else {
      /* The cursor might be moved to the other page
      and the max trx id field should be updated after
      the cursor was fixed. */
      if (!index->is_clustered()) {
        page_update_max_trx_id(btr_cur_get_block(cursor),
                               btr_cur_get_page_zip(cursor),
                               thr_get_trx(thr)->id, mtr);
      }
      if (!page_rec_is_infimum(btr_cur_get_rec(cursor)) ||
          btr_page_get_prev(buf_block_get_frame(btr_cur_get_block(cursor)),
                            mtr) == FIL_NULL) {
        /* split and inserted need to call
        lock_update_insert() always. */
        inherit = TRUE;
      }
    }
  }

#ifdef BTR_CUR_ADAPT
  if (!index->disable_ahi) {
    btr_search_update_hash_on_insert(cursor);
  }
#endif
  if (inherit && !(flags & BTR_NO_LOCKING_FLAG)) {
    lock_update_insert(btr_cur_get_block(cursor), *rec);
  }

  if (n_reserved > 0) {
    fil_space_release_free_extents(index->space, n_reserved);
  }

  *big_rec = big_rec_vec;

  return (DB_SUCCESS);
}

/*==================== B-TREE UPDATE =========================*/

/** For an update, checks the locks and does the undo logging.
 @return DB_SUCCESS, DB_WAIT_LOCK, or error number */
UNIV_INLINE MY_ATTRIBUTE((warn_unused_result)) dberr_t
    btr_cur_upd_lock_and_undo(
        ulint flags,          /*!< in: undo logging and locking flags */
        btr_cur_t *cursor,    /*!< in: cursor on record to update */
        const ulint *offsets, /*!< in: rec_get_offsets() on cursor */
        const upd_t *update,  /*!< in: update vector */
        ulint cmpl_info,      /*!< in: compiler info on secondary index
                            updates */
        que_thr_t *thr,       /*!< in: query thread
                              (can be NULL if BTR_NO_LOCKING_FLAG) */
        mtr_t *mtr,           /*!< in/out: mini-transaction */
        roll_ptr_t *roll_ptr) /*!< out: roll pointer */
{
  dict_index_t *index;
  const rec_t *rec;
  dberr_t err;

  ut_ad(thr != NULL || (flags & BTR_NO_LOCKING_FLAG));

  rec = btr_cur_get_rec(cursor);
  index = cursor->index;

  ut_ad(rec_offs_validate(rec, index, offsets));

  if (!index->is_clustered()) {
    ut_ad(dict_index_is_online_ddl(index) == !!(flags & BTR_CREATE_FLAG));

    /* We do undo logging only when we update a clustered index
    record */
    return (lock_sec_rec_modify_check_and_lock(flags, btr_cur_get_block(cursor),
                                               rec, index, thr, mtr));
  }

  /* Check if we have to wait for a lock: enqueue an explicit lock
  request if yes */

  if (!(flags & BTR_NO_LOCKING_FLAG)) {
    err = lock_clust_rec_modify_check_and_lock(flags, btr_cur_get_block(cursor),
                                               rec, index, offsets, thr);
    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  /* Append the info about the update in the undo log */

  return (trx_undo_report_row_operation(flags, TRX_UNDO_MODIFY_OP, thr, index,
                                        NULL, update, cmpl_info, rec, offsets,
                                        roll_ptr));
}

/** Writes a redo log record of updating a record in-place. */
void btr_cur_update_in_place_log(
    ulint flags,         /*!< in: flags */
    const rec_t *rec,    /*!< in: record */
    dict_index_t *index, /*!< in: index of the record */
    const upd_t *update, /*!< in: update vector */
    trx_id_t trx_id,     /*!< in: transaction id */
    roll_ptr_t roll_ptr, /*!< in: roll ptr */
    mtr_t *mtr)          /*!< in: mtr */
{
  byte *log_ptr;
  const page_t *page = page_align(rec);
  ut_ad(flags < 256);
  ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));

  log_ptr = mlog_open_and_write_index(
      mtr, rec, index,
      page_is_comp(page) ? MLOG_COMP_REC_UPDATE_IN_PLACE
                         : MLOG_REC_UPDATE_IN_PLACE,
      1 + DATA_ROLL_PTR_LEN + 14 + 2 + MLOG_BUF_MARGIN);

  if (!log_ptr) {
    /* Logging in mtr is switched off during crash recovery */
    return;
  }

  /* For secondary indexes, we could skip writing the dummy system fields
  to the redo log but we have to change redo log parsing of
  MLOG_REC_UPDATE_IN_PLACE/MLOG_COMP_REC_UPDATE_IN_PLACE or we have to add
  new redo log record. For now, just write dummy sys fields to the redo
  log if we are updating a secondary index record.
  */
  mach_write_to_1(log_ptr, flags);
  log_ptr++;

  if (index->is_clustered()) {
    log_ptr =
        row_upd_write_sys_vals_to_log(index, trx_id, roll_ptr, log_ptr, mtr);
  } else {
    /* Dummy system fields for a secondary index */
    /* TRX_ID Position */
    log_ptr += mach_write_compressed(log_ptr, 0);
    /* ROLL_PTR */
    trx_write_roll_ptr(log_ptr, 0);
    log_ptr += DATA_ROLL_PTR_LEN;
    /* TRX_ID */
    log_ptr += mach_u64_write_compressed(log_ptr, 0);
  }

  mach_write_to_2(log_ptr, page_offset(rec));
  log_ptr += 2;

  row_upd_index_write_log(update, log_ptr, mtr);
}
#endif /* UNIV_HOTBACKUP */

/** Parses a redo log record of updating a record in-place.
 @return end of log record or NULL */
byte *btr_cur_parse_update_in_place(
    byte *ptr,                /*!< in: buffer */
    byte *end_ptr,            /*!< in: buffer end */
    page_t *page,             /*!< in/out: page or NULL */
    page_zip_des_t *page_zip, /*!< in/out: compressed page, or NULL */
    dict_index_t *index)      /*!< in: index corresponding to page */
{
  ulint flags;
  rec_t *rec;
  upd_t *update;
  ulint pos;
  trx_id_t trx_id;
  roll_ptr_t roll_ptr;
  ulint rec_offset;
  mem_heap_t *heap;
  ulint *offsets;

  if (end_ptr < ptr + 1) {
    return (NULL);
  }

  flags = mach_read_from_1(ptr);
  ptr++;

  ptr = row_upd_parse_sys_vals(ptr, end_ptr, &pos, &trx_id, &roll_ptr);

  if (ptr == NULL) {
    return (NULL);
  }

  if (end_ptr < ptr + 2) {
    return (NULL);
  }

  rec_offset = mach_read_from_2(ptr);
  ptr += 2;

  ut_a(rec_offset <= UNIV_PAGE_SIZE);

  heap = mem_heap_create(256);

  ptr = row_upd_index_parse(ptr, end_ptr, heap, &update);

  if (!ptr || !page) {
    goto func_exit;
  }

  ut_a((ibool) !!page_is_comp(page) == dict_table_is_comp(index->table));
  rec = page + rec_offset;

  /* We do not need to reserve search latch, as the page is only
  being recovered, and there cannot be a hash index to it. */

  offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

  if (!(flags & BTR_KEEP_SYS_FLAG)) {
    row_upd_rec_sys_fields_in_recovery(rec, page_zip, offsets, pos, trx_id,
                                       roll_ptr);
  }

  row_upd_rec_in_place(rec, index, offsets, update, page_zip);

func_exit:
  mem_heap_free(heap);

  return (ptr);
}

#ifndef UNIV_HOTBACKUP
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
{
  const page_t *page = page_cur_get_page(cursor);

  ut_ad(page_zip == page_cur_get_page_zip(cursor));
  ut_ad(page_zip);
  ut_ad(!dict_index_is_ibuf(index));
  ut_ad(rec_offs_validate(page_cur_get_rec(cursor), index, offsets));

  if (page_zip_available(page_zip, index->is_clustered(), length, create)) {
    return (true);
  }

  if (!page_zip->m_nonempty && !page_has_garbage(page)) {
    /* The page has been freshly compressed, so
    reorganizing it will not help. */
    return (false);
  }

  if (create && page_is_leaf(page) &&
      (length + page_get_data_size(page) >=
       dict_index_zip_pad_optimal_page_size(index))) {
    return (false);
  }

  if (!btr_page_reorganize(cursor, index, mtr)) {
    goto out_of_space;
  }

  rec_offs_make_valid(page_cur_get_rec(cursor), index, offsets);

  /* After recompressing a page, we must make sure that the free
  bits in the insert buffer bitmap will not exceed the free
  space on the page.  Because this function will not attempt
  recompression unless page_zip_available() fails above, it is
  safe to reset the free bits if page_zip_available() fails
  again, below.  The free bits can safely be reset in a separate
  mini-transaction.  If page_zip_available() succeeds below, we
  can be sure that the btr_page_reorganize() above did not reduce
  the free space available on the page. */

  if (page_zip_available(page_zip, index->is_clustered(), length, create)) {
    return (true);
  }

out_of_space:
  ut_ad(rec_offs_validate(page_cur_get_rec(cursor), index, offsets));

  /* Out of space: reset the free bits. */
  if (!index->is_clustered() && !index->table->is_temporary() &&
      page_is_leaf(page)) {
    ibuf_reset_free_bits(page_cur_get_block(cursor));
  }

  return (false);
}

/** Updates a record when the update causes no size changes in its fields.
 We assume here that the ordering fields of the record do not change.
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
{
  dict_index_t *index;
  buf_block_t *block;
  page_zip_des_t *page_zip;
  dberr_t err;
  rec_t *rec;
  roll_ptr_t roll_ptr = 0;
  ulint was_delete_marked;
  ibool is_hashed;

  rec = btr_cur_get_rec(cursor);
  index = cursor->index;
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));
  ut_ad(trx_id > 0 || (flags & BTR_KEEP_SYS_FLAG) ||
        index->table->is_intrinsic());
  /* The insert buffer tree should never be updated in place. */
  ut_ad(!dict_index_is_ibuf(index));
  ut_ad(dict_index_is_online_ddl(index) == !!(flags & BTR_CREATE_FLAG) ||
        index->is_clustered());
  ut_ad((flags & ~(BTR_KEEP_POS_FLAG | BTR_KEEP_IBUF_BITMAP)) ==
            (BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG | BTR_CREATE_FLAG |
             BTR_KEEP_SYS_FLAG) ||
        thr_get_trx(thr)->id == trx_id);
  ut_ad(fil_page_index_page_check(btr_cur_get_page(cursor)));
  ut_ad(btr_page_get_index_id(btr_cur_get_page(cursor)) == index->id);

  DBUG_PRINT("ib_cur",
             ("update-in-place %s (" IB_ID_FMT ") by " TRX_ID_FMT ": %s",
              index->name(), index->id, trx_id,
              rec_printer(rec, offsets).str().c_str()));

  block = btr_cur_get_block(cursor);
  page_zip = buf_block_get_page_zip(block);

  /* Check that enough space is available on the compressed page. */
  if (page_zip) {
    ut_ad(!index->table->is_temporary());

    if (!btr_cur_update_alloc_zip(page_zip, btr_cur_get_page_cur(cursor), index,
                                  offsets, rec_offs_size(offsets), false,
                                  mtr)) {
      return (DB_ZIP_OVERFLOW);
    }

    rec = btr_cur_get_rec(cursor);
  }

  /* Do lock checking and undo logging */
  err = btr_cur_upd_lock_and_undo(flags, cursor, offsets, update, cmpl_info,
                                  thr, mtr, &roll_ptr);
  if (UNIV_UNLIKELY(err != DB_SUCCESS)) {
    /* We may need to update the IBUF_BITMAP_FREE
    bits after a reorganize that was done in
    btr_cur_update_alloc_zip(). */
    goto func_exit;
  }

  if (!(flags & BTR_KEEP_SYS_FLAG) && !index->table->is_intrinsic()) {
    row_upd_rec_sys_fields(rec, NULL, index, offsets, thr_get_trx(thr),
                           roll_ptr);
  }

  was_delete_marked =
      rec_get_deleted_flag(rec, page_is_comp(buf_block_get_frame(block)));

  is_hashed = (block->index != NULL);

  if (is_hashed) {
    /* TO DO: Can we skip this if none of the fields
    index->search_info->curr_n_fields
    are being updated? */

    /* The function row_upd_changes_ord_field_binary works only
    if the update vector was built for a clustered index, we must
    NOT call it if index is secondary */

    if (!index->is_clustered() ||
        row_upd_changes_ord_field_binary(index, update, thr, NULL, NULL)) {
      /* Remove possible hash index pointer to this record */
      btr_search_update_hash_on_delete(cursor);
    }

    rw_lock_x_lock(btr_get_search_latch(index));
  }

  assert_block_ahi_valid(block);
  row_upd_rec_in_place(rec, index, offsets, update, page_zip);

  if (is_hashed) {
    rw_lock_x_unlock(btr_get_search_latch(index));
  }

  btr_cur_update_in_place_log(flags, rec, index, update, trx_id, roll_ptr, mtr);

  if (was_delete_marked &&
      !rec_get_deleted_flag(rec, page_is_comp(buf_block_get_frame(block)))) {
    /* The new updated record owns its possible externally
    stored fields */

    lob::BtrContext btr_ctx(mtr, NULL, index, rec, offsets, block);
    btr_ctx.unmark_extern_fields();
  }

  ut_ad(err == DB_SUCCESS);

func_exit:
  if (page_zip && !(flags & BTR_KEEP_IBUF_BITMAP) && !index->is_clustered() &&
      page_is_leaf(buf_block_get_frame(block))) {
    /* Update the free bits in the insert buffer. */
    ibuf_update_free_bits_zip(block, mtr);
  }

  return (err);
}

/** Tries to update a record on a page in an index tree. It is assumed that mtr
 holds an x-latch on the page. The operation does not succeed if there is too
 little space on the page or if the update would result in too empty a page,
 so that tree compression is recommended. We assume here that the ordering
 fields of the record do not change.
 @return error code, including
 @retval DB_SUCCESS on success
 @retval DB_OVERFLOW if the updated record does not fit
 @retval DB_UNDERFLOW if the page would become too empty
 @retval DB_ZIP_OVERFLOW if there is not enough space left
 on the compressed page (IBUF_BITMAP_FREE was reset outside mtr) */
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
{
  dict_index_t *index;
  page_cur_t *page_cursor;
  dberr_t err;
  buf_block_t *block;
  page_t *page;
  page_zip_des_t *page_zip;
  rec_t *rec;
  ulint max_size;
  ulint new_rec_size;
  ulint old_rec_size;
  ulint max_ins_size = 0;
  dtuple_t *new_entry;
  roll_ptr_t roll_ptr;
  ulint i;
  ulint n_ext;

  block = btr_cur_get_block(cursor);
  page = buf_block_get_frame(block);
  rec = btr_cur_get_rec(cursor);
  index = cursor->index;
  ut_ad(trx_id > 0 || (flags & BTR_KEEP_SYS_FLAG) ||
        index->table->is_intrinsic());
  ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));
  ut_ad(mtr_is_block_fix(mtr, block, MTR_MEMO_PAGE_X_FIX, index->table));
  /* This is intended only for leaf page updates */
  ut_ad(page_is_leaf(page));
  /* The insert buffer tree should never be updated in place. */
  ut_ad(!dict_index_is_ibuf(index));
  ut_ad(dict_index_is_online_ddl(index) == !!(flags & BTR_CREATE_FLAG) ||
        index->is_clustered());
  ut_ad((flags & ~(BTR_KEEP_POS_FLAG | BTR_KEEP_IBUF_BITMAP)) ==
            (BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG | BTR_CREATE_FLAG |
             BTR_KEEP_SYS_FLAG) ||
        thr_get_trx(thr)->id == trx_id);
  ut_ad(fil_page_index_page_check(page));
  ut_ad(btr_page_get_index_id(page) == index->id);

  *offsets = rec_get_offsets(rec, index, *offsets, ULINT_UNDEFINED, heap);
#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
  ut_a(!rec_offs_any_null_extern(rec, *offsets) ||
       (flags & ~(BTR_KEEP_POS_FLAG | BTR_KEEP_IBUF_BITMAP)) ==
           (BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG | BTR_CREATE_FLAG |
            BTR_KEEP_SYS_FLAG) ||
       trx_is_recv(thr_get_trx(thr)));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

  if (!row_upd_changes_field_size_or_external(index, *offsets, update)) {
    /* The simplest and the most common case: the update does not
    change the size of any field and none of the updated fields is
    externally stored in rec or update, and there is enough space
    on the compressed page to log the update. */

    return (btr_cur_update_in_place(flags, cursor, *offsets, update, cmpl_info,
                                    thr, trx_id, mtr));
  }

  if (rec_offs_any_extern(*offsets)) {
  any_extern:
    /* Externally stored fields are treated in pessimistic
    update */

    /* prefetch siblings of the leaf for the pessimistic
    operation. */
    btr_cur_prefetch_siblings(block);

    return (DB_OVERFLOW);
  }

  for (i = 0; i < upd_get_n_fields(update); i++) {
    if (dfield_is_ext(&upd_get_nth_field(update, i)->new_val)) {
      goto any_extern;
    }
  }

  DBUG_PRINT("ib_cur",
             ("update %s (" IB_ID_FMT ") by " TRX_ID_FMT ": %s", index->name(),
              index->id, trx_id, rec_printer(rec, *offsets).str().c_str()));

  page_cursor = btr_cur_get_page_cur(cursor);

  if (!*heap) {
    *heap = mem_heap_create(rec_offs_size(*offsets) +
                            DTUPLE_EST_ALLOC(rec_offs_n_fields(*offsets)));
  }

  new_entry = row_rec_to_index_entry(rec, index, *offsets, &n_ext, *heap);
  /* We checked above that there are no externally stored fields. */
  ut_a(!n_ext);

  /* The page containing the clustered index record
  corresponding to new_entry is latched in mtr.
  Thus the following call is safe. */
  row_upd_index_replace_new_col_vals_index_pos(new_entry, index, update, FALSE,
                                               *heap);

  new_entry->ignore_trailing_default(index);

  old_rec_size = rec_offs_size(*offsets);
  new_rec_size = rec_get_converted_size(index, new_entry, 0);

  page_zip = buf_block_get_page_zip(block);
#ifdef UNIV_ZIP_DEBUG
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

  if (page_zip) {
    ut_ad(!index->table->is_temporary());

    if (!btr_cur_update_alloc_zip(page_zip, page_cursor, index, *offsets,
                                  new_rec_size, true, mtr)) {
      return (DB_ZIP_OVERFLOW);
    }

    rec = page_cur_get_rec(page_cursor);
  }

  /* We limit max record size to 16k even for 64k page size. */
  if (new_rec_size >= REC_MAX_DATA_SIZE) {
    err = DB_OVERFLOW;

    goto func_exit;
  }

  if (UNIV_UNLIKELY(new_rec_size >=
                    (page_get_free_space_of_empty(page_is_comp(page)) / 2))) {
    /* We may need to update the IBUF_BITMAP_FREE
    bits after a reorganize that was done in
    btr_cur_update_alloc_zip(). */
    err = DB_OVERFLOW;
    goto func_exit;
  }

  if (UNIV_UNLIKELY(page_get_data_size(page) - old_rec_size + new_rec_size <
                    BTR_CUR_PAGE_COMPRESS_LIMIT(index))) {
    /* We may need to update the IBUF_BITMAP_FREE
    bits after a reorganize that was done in
    btr_cur_update_alloc_zip(). */

    /* The page would become too empty */
    err = DB_UNDERFLOW;
    goto func_exit;
  }

  /* We do not attempt to reorganize if the page is compressed.
  This is because the page may fail to compress after reorganization. */
  max_size =
      page_zip
          ? page_get_max_insert_size(page, 1)
          : (old_rec_size + page_get_max_insert_size_after_reorganize(page, 1));

  if (!page_zip) {
    max_ins_size = page_get_max_insert_size_after_reorganize(page, 1);
  }

  if (!(((max_size >= BTR_CUR_PAGE_REORGANIZE_LIMIT) &&
         (max_size >= new_rec_size)) ||
        (page_get_n_recs(page) <= 1))) {
    /* We may need to update the IBUF_BITMAP_FREE
    bits after a reorganize that was done in
    btr_cur_update_alloc_zip(). */

    /* There was not enough space, or it did not pay to
    reorganize: for simplicity, we decide what to do assuming a
    reorganization is needed, though it might not be necessary */

    err = DB_OVERFLOW;
    goto func_exit;
  }

  /* Do lock checking and undo logging */
  err = btr_cur_upd_lock_and_undo(flags, cursor, *offsets, update, cmpl_info,
                                  thr, mtr, &roll_ptr);
  if (err != DB_SUCCESS) {
    /* We may need to update the IBUF_BITMAP_FREE
    bits after a reorganize that was done in
    btr_cur_update_alloc_zip(). */
    goto func_exit;
  }

  /* Ok, we may do the replacement. Store on the page infimum the
  explicit locks on rec, before deleting rec (see the comment in
  btr_cur_pessimistic_update). */
  if (!dict_table_is_locking_disabled(index->table)) {
    lock_rec_store_on_page_infimum(block, rec);
  }

  btr_search_update_hash_on_delete(cursor);

  page_cur_delete_rec(page_cursor, index, *offsets, mtr);

  page_cur_move_to_prev(page_cursor);

  if (!(flags & BTR_KEEP_SYS_FLAG) && !index->table->is_intrinsic()) {
    row_upd_index_entry_sys_field(new_entry, index, DATA_ROLL_PTR, roll_ptr);
    row_upd_index_entry_sys_field(new_entry, index, DATA_TRX_ID, trx_id);
  }

  /* There are no externally stored columns in new_entry */
  rec = btr_cur_insert_if_possible(cursor, new_entry, offsets, heap,
                                   0 /*n_ext*/, mtr);
  ut_a(rec); /* <- We calculated above the insert would fit */

  /* Restore the old explicit lock state on the record */
  if (!dict_table_is_locking_disabled(index->table)) {
    lock_rec_restore_from_page_infimum(block, rec, block);
  }

  page_cur_move_to_next(page_cursor);
  ut_ad(err == DB_SUCCESS);

func_exit:
  if (!(flags & BTR_KEEP_IBUF_BITMAP) && !index->is_clustered()) {
    /* Update the free bits in the insert buffer. */
    if (page_zip) {
      ibuf_update_free_bits_zip(block, mtr);
    } else {
      ibuf_update_free_bits_low(block, max_ins_size, mtr);
    }
  }

  if (err != DB_SUCCESS) {
    /* prefetch siblings of the leaf for the pessimistic
    operation. */
    btr_cur_prefetch_siblings(block);
  }

  return (err);
}

/** If, in a split, a new supremum record was created as the predecessor of the
 updated record, the supremum record must inherit exactly the locks on the
 updated record. In the split it may have inherited locks from the successor
 of the updated record, which is not correct. This function restores the
 right locks for the new supremum. */
static void btr_cur_pess_upd_restore_supremum(
    buf_block_t *block, /*!< in: buffer block of rec */
    const rec_t *rec,   /*!< in: updated record */
    mtr_t *mtr)         /*!< in: mtr */
{
  page_t *page;
  buf_block_t *prev_block;

  page = buf_block_get_frame(block);

  if (page_rec_get_next(page_get_infimum_rec(page)) != rec) {
    /* Updated record is not the first user record on its page */

    return;
  }

  const page_no_t prev_page_no = btr_page_get_prev(page, mtr);

  const page_id_t page_id(block->page.id.space(), prev_page_no);

  ut_ad(prev_page_no != FIL_NULL);
  prev_block = buf_page_get_with_no_latch(page_id, block->page.size, mtr);
#ifdef UNIV_BTR_DEBUG
  ut_a(btr_page_get_next(prev_block->frame, mtr) == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

  /* We must already have an x-latch on prev_block! */
  ut_ad(mtr_memo_contains(mtr, prev_block, MTR_MEMO_PAGE_X_FIX));

  lock_rec_reset_and_inherit_gap_locks(prev_block, block, PAGE_HEAP_NO_SUPREMUM,
                                       page_rec_get_heap_no(rec));
}
/** Performs an update of a record on a page of a tree. It is assumed
 that mtr holds an x-latch on the tree and on the cursor page. If the
 update is made on the leaf level, to avoid deadlocks, mtr must also
 own x-latches to brothers of page, if those brothers exist. We assume
 here that the ordering fields of the record do not change.
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
    mtr_t *mtr) /*!< in/out: mini-transaction; must be
                committed before latching any further pages */
{
  DBUG_ENTER("btr_cur_pessimistic_update");
  big_rec_t *big_rec_vec = NULL;
  big_rec_t *dummy_big_rec;
  dict_index_t *index;
  buf_block_t *block;
  page_t *page;
  page_zip_des_t *page_zip;
  rec_t *rec;
  page_cur_t *page_cursor;
  dberr_t err;
  dberr_t optim_err;
  roll_ptr_t roll_ptr;
  ibool was_first;
  ulint n_reserved = 0;
  ulint n_ext;
  ulint max_ins_size = 0;

  *offsets = NULL;
  *big_rec = NULL;

  block = btr_cur_get_block(cursor);
  page = buf_block_get_frame(block);
  page_zip = buf_block_get_page_zip(block);
  index = cursor->index;

  ut_ad(mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                  MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
        index->table->is_intrinsic());
  ut_ad(mtr_is_block_fix(mtr, block, MTR_MEMO_PAGE_X_FIX, index->table));
#ifdef UNIV_ZIP_DEBUG
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
  ut_ad(!page_zip || !index->table->is_temporary());
  /* The insert buffer tree should never be updated in place. */
  ut_ad(!dict_index_is_ibuf(index));
  ut_ad(trx_id > 0 || (flags & BTR_KEEP_SYS_FLAG) ||
        index->table->is_intrinsic());
  ut_ad(dict_index_is_online_ddl(index) == !!(flags & BTR_CREATE_FLAG) ||
        index->is_clustered());
  ut_ad((flags & ~BTR_KEEP_POS_FLAG) ==
            (BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG | BTR_CREATE_FLAG |
             BTR_KEEP_SYS_FLAG) ||
        thr_get_trx(thr)->id == trx_id);

  err = optim_err = btr_cur_optimistic_update(
      flags | BTR_KEEP_IBUF_BITMAP, cursor, offsets, offsets_heap, update,
      cmpl_info, thr, trx_id, mtr);

  switch (err) {
    case DB_ZIP_OVERFLOW:
    case DB_UNDERFLOW:
    case DB_OVERFLOW:
      break;
    default:
    err_exit:
      /* We suppressed this with BTR_KEEP_IBUF_BITMAP.
      For DB_ZIP_OVERFLOW, the IBUF_BITMAP_FREE bits were
      already reset by btr_cur_update_alloc_zip() if the
      page was recompressed. */
      if (page_zip && optim_err != DB_ZIP_OVERFLOW && !index->is_clustered() &&
          page_is_leaf(page)) {
        ut_ad(!index->table->is_temporary());
        ibuf_update_free_bits_zip(block, mtr);
      }

      if (big_rec_vec != NULL) {
        dtuple_big_rec_free(big_rec_vec);
      }

      DBUG_RETURN(err);
  }

  rec = btr_cur_get_rec(cursor);

  *offsets =
      rec_get_offsets(rec, index, *offsets, ULINT_UNDEFINED, offsets_heap);

  dtuple_t *new_entry =
      row_rec_to_index_entry(rec, index, *offsets, &n_ext, entry_heap);

  /* The page containing the clustered index record
  corresponding to new_entry is latched in mtr.  If the
  clustered index record is delete-marked, then its externally
  stored fields cannot have been purged yet, because then the
  purge would also have removed the clustered index record
  itself.  Thus the following call is safe. */
  row_upd_index_replace_new_col_vals_index_pos(new_entry, index, update, FALSE,
                                               entry_heap);

  new_entry->ignore_trailing_default(index);

  /* We have to set appropriate extern storage bits in the new
  record to be inserted: we have to remember which fields were such */

  ut_ad(!page_is_comp(page) || !rec_get_node_ptr_flag(rec));
  ut_ad(rec_offs_validate(rec, index, *offsets));
  n_ext += lob::btr_push_update_extern_fields(new_entry, update, entry_heap);

  /* UNDO logging is also turned-off during normal operation on intrinsic
  table so condition needs to ensure that table is not intrinsic. */
  if ((flags & BTR_NO_UNDO_LOG_FLAG) && rec_offs_any_extern(*offsets) &&
      !index->table->is_intrinsic()) {
    /* We are in a transaction rollback undoing a row
    update: we must free possible externally stored fields
    which got new values in the update, if they are not
    inherited values. They can be inherited if we have
    updated the primary key to another value, and then
    update it back again. */

    ut_ad(big_rec_vec == NULL);
    ut_ad(index->is_clustered());
    ut_ad((flags & ~BTR_KEEP_POS_FLAG) ==
              (BTR_NO_LOCKING_FLAG | BTR_CREATE_FLAG | BTR_KEEP_SYS_FLAG) ||
          thr_get_trx(thr)->id == trx_id);

    DBUG_EXECUTE_IF("ib_blob_update_rollback", DBUG_SUICIDE(););
    RECOVERY_CRASH(99);

    lob::BtrContext ctx(mtr, nullptr, index, rec, *offsets, block);

    ctx.free_updated_extern_fields(trx_id, undo_no, update, true);
  }

  if (page_zip_rec_needs_ext(rec_get_converted_size(index, new_entry, n_ext),
                             page_is_comp(page), dict_index_get_n_fields(index),
                             block->page.size)) {
    big_rec_vec = dtuple_convert_big_rec(index, update, new_entry, &n_ext);
    if (UNIV_UNLIKELY(big_rec_vec == NULL)) {
    /* We cannot goto return_after_reservations,
    because we may need to update the
    IBUF_BITMAP_FREE bits, which was suppressed by
    BTR_KEEP_IBUF_BITMAP. */
#ifdef UNIV_ZIP_DEBUG
      ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
      if (n_reserved > 0) {
        fil_space_release_free_extents(index->space, n_reserved);
      }

      err = DB_TOO_BIG_RECORD;
      goto err_exit;
    }

    ut_ad(page_is_leaf(page));
    ut_ad(index->is_clustered());
    ut_ad(flags & BTR_KEEP_POS_FLAG);
  }

  /* Do lock checking and undo logging */
  err = btr_cur_upd_lock_and_undo(flags, cursor, *offsets, update, cmpl_info,
                                  thr, mtr, &roll_ptr);
  if (err != DB_SUCCESS) {
    goto err_exit;
  }

  if (optim_err == DB_OVERFLOW) {
    /* First reserve enough free space for the file segments
    of the index tree, so that the update will not fail because
    of lack of space */

    ulint n_extents = cursor->tree_height / 16 + 3;

    if (!fsp_reserve_free_extents(
            &n_reserved, index->space, n_extents,
            flags & BTR_NO_UNDO_LOG_FLAG ? FSP_CLEANING : FSP_NORMAL, mtr)) {
      err = DB_OUT_OF_FILE_SPACE;
      goto err_exit;
    }
  }

  if (!(flags & BTR_KEEP_SYS_FLAG) && !index->table->is_intrinsic()) {
    row_upd_index_entry_sys_field(new_entry, index, DATA_ROLL_PTR, roll_ptr);
    row_upd_index_entry_sys_field(new_entry, index, DATA_TRX_ID, trx_id);
  }

  if (!page_zip) {
    max_ins_size = page_get_max_insert_size_after_reorganize(page, 1);
  }

  /* Store state of explicit locks on rec on the page infimum record,
  before deleting rec. The page infimum acts as a dummy carrier of the
  locks, taking care also of lock releases, before we can move the locks
  back on the actual record. There is a special case: if we are
  inserting on the root page and the insert causes a call of
  btr_root_raise_and_insert. Therefore we cannot in the lock system
  delete the lock structs set on the root page even if the root
  page carries just node pointers. */
  if (!dict_table_is_locking_disabled(index->table)) {
    lock_rec_store_on_page_infimum(block, rec);
  }

  btr_search_update_hash_on_delete(cursor);

#ifdef UNIV_ZIP_DEBUG
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
  page_cursor = btr_cur_get_page_cur(cursor);

  page_cur_delete_rec(page_cursor, index, *offsets, mtr);

  page_cur_move_to_prev(page_cursor);

  rec = btr_cur_insert_if_possible(cursor, new_entry, offsets, offsets_heap,
                                   n_ext, mtr);

  if (rec) {
    page_cursor->rec = rec;

    if (!dict_table_is_locking_disabled(index->table)) {
      lock_rec_restore_from_page_infimum(btr_cur_get_block(cursor), rec, block);
    }

    if (!rec_get_deleted_flag(rec, rec_offs_comp(*offsets))) {
      /* The new inserted record owns its possible externally
      stored fields */
      lob::BtrContext btr_ctx(mtr, NULL, index, rec, *offsets, block);
      btr_ctx.unmark_extern_fields();
    }

    bool adjust = big_rec_vec && (flags & BTR_KEEP_POS_FLAG);

    if (btr_cur_compress_if_useful(cursor, adjust, mtr)) {
      if (adjust) {
        rec_offs_make_valid(page_cursor->rec, index, *offsets);
      }
    } else if (!index->is_clustered() && page_is_leaf(page)) {
      /* Update the free bits in the insert buffer.
      This is the same block which was skipped by
      BTR_KEEP_IBUF_BITMAP. */
      if (page_zip) {
        ibuf_update_free_bits_zip(block, mtr);
      } else {
        ibuf_update_free_bits_low(block, max_ins_size, mtr);
      }
    }

    if (!srv_read_only_mode && !big_rec_vec && page_is_leaf(page) &&
        !dict_index_is_online_ddl(index)) {
      mtr_memo_release(mtr, dict_index_get_lock(index),
                       MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK);

      /* NOTE: We cannot release root block latch here, because it
      has segment header and already modified in most of cases.*/
    }

    err = DB_SUCCESS;
    goto return_after_reservations;
  } else {
    /* If the page is compressed and it initially
    compresses very well, and there is a subsequent insert
    of a badly-compressing record, it is possible for
    btr_cur_optimistic_update() to return DB_UNDERFLOW and
    btr_cur_insert_if_possible() to return FALSE. */
    ut_a(page_zip || optim_err != DB_UNDERFLOW);

    /* Out of space: reset the free bits.
    This is the same block which was skipped by
    BTR_KEEP_IBUF_BITMAP. */
    if (!index->is_clustered() && !index->table->is_temporary() &&
        page_is_leaf(page)) {
      ibuf_reset_free_bits(block);
    }
  }

  if (big_rec_vec != NULL && !index->table->is_intrinsic()) {
    ut_ad(page_is_leaf(page));
    ut_ad(index->is_clustered());
    ut_ad(flags & BTR_KEEP_POS_FLAG);

    /* btr_page_split_and_insert() in
    btr_cur_pessimistic_insert() invokes
    mtr_memo_release(mtr, index->lock, MTR_MEMO_SX_LOCK).
    We must keep the index->lock when we created a
    big_rec, so that row_upd_clust_rec() can store the
    big_rec in the same mini-transaction. */

    ut_ad(mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                    MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK));

    mtr_sx_lock(dict_index_get_lock(index), mtr);
  }

  /* Was the record to be updated positioned as the first user
  record on its page? */
  was_first = page_cur_is_before_first(page_cursor);

  /* Lock checks and undo logging were already performed by
  btr_cur_upd_lock_and_undo(). We do not try
  btr_cur_optimistic_insert() because
  btr_cur_insert_if_possible() already failed above. */

  err = btr_cur_pessimistic_insert(
      BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG | BTR_KEEP_SYS_FLAG, cursor,
      offsets, offsets_heap, new_entry, &rec, &dummy_big_rec, n_ext, NULL, mtr);
  ut_a(rec);
  ut_a(err == DB_SUCCESS);
  ut_a(dummy_big_rec == NULL);
  ut_ad(rec_offs_validate(rec, cursor->index, *offsets));
  page_cursor->rec = rec;

  /* Multiple transactions cannot simultaneously operate on the
  same temp-table in parallel.
  max_trx_id is ignored for temp tables because it not required
  for MVCC. */
  if (dict_index_is_sec_or_ibuf(index) && !index->table->is_temporary()) {
    /* Update PAGE_MAX_TRX_ID in the index page header.
    It was not updated by btr_cur_pessimistic_insert()
    because of BTR_NO_LOCKING_FLAG. */
    buf_block_t *rec_block;

    rec_block = btr_cur_get_block(cursor);

    page_update_max_trx_id(rec_block, buf_block_get_page_zip(rec_block), trx_id,
                           mtr);
  }

  if (!rec_get_deleted_flag(rec, rec_offs_comp(*offsets))) {
    /* The new inserted record owns its possible externally
    stored fields */
    buf_block_t *rec_block = btr_cur_get_block(cursor);

#ifdef UNIV_ZIP_DEBUG
    ut_a(!page_zip || page_zip_validate(page_zip, page, index));
    page = buf_block_get_frame(rec_block);
#endif /* UNIV_ZIP_DEBUG */
    page_zip = buf_block_get_page_zip(rec_block);

    lob::BtrContext btr_ctx(mtr, NULL, index, rec, *offsets, rec_block);
    btr_ctx.unmark_extern_fields();
  }

  if (!dict_table_is_locking_disabled(index->table)) {
    lock_rec_restore_from_page_infimum(btr_cur_get_block(cursor), rec, block);
  }

  /* If necessary, restore also the correct lock state for a new,
  preceding supremum record created in a page split. While the old
  record was nonexistent, the supremum might have inherited its locks
  from a wrong record. */

  if (!was_first && !dict_table_is_locking_disabled(index->table)) {
    btr_cur_pess_upd_restore_supremum(btr_cur_get_block(cursor), rec, mtr);
  }

return_after_reservations:
#ifdef UNIV_ZIP_DEBUG
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

  if (n_reserved > 0) {
    fil_space_release_free_extents(index->space, n_reserved);
  }

  *big_rec = big_rec_vec;

  DBUG_RETURN(err);
}

/*==================== B-TREE DELETE MARK AND UNMARK ===============*/

/** Writes the redo log record for delete marking or unmarking of an index
 record. */
UNIV_INLINE
void btr_cur_del_mark_set_clust_rec_log(
    rec_t *rec,          /*!< in: record */
    dict_index_t *index, /*!< in: index of the record */
    trx_id_t trx_id,     /*!< in: transaction id */
    roll_ptr_t roll_ptr, /*!< in: roll ptr to the undo log record */
    mtr_t *mtr)          /*!< in: mtr */
{
  byte *log_ptr;

  ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));

  log_ptr = mlog_open_and_write_index(mtr, rec, index,
                                      page_rec_is_comp(rec)
                                          ? MLOG_COMP_REC_CLUST_DELETE_MARK
                                          : MLOG_REC_CLUST_DELETE_MARK,
                                      1 + 1 + DATA_ROLL_PTR_LEN + 14 + 2);

  if (!log_ptr) {
    /* Logging in mtr is switched off during crash recovery */
    return;
  }

  *log_ptr++ = 0;
  *log_ptr++ = 1;

  log_ptr =
      row_upd_write_sys_vals_to_log(index, trx_id, roll_ptr, log_ptr, mtr);
  mach_write_to_2(log_ptr, page_offset(rec));
  log_ptr += 2;

  mlog_close(mtr, log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses the redo log record for delete marking or unmarking of a clustered
 index record.
 @return end of log record or NULL */
byte *btr_cur_parse_del_mark_set_clust_rec(
    byte *ptr,                /*!< in: buffer */
    byte *end_ptr,            /*!< in: buffer end */
    page_t *page,             /*!< in/out: page or NULL */
    page_zip_des_t *page_zip, /*!< in/out: compressed page, or NULL */
    dict_index_t *index)      /*!< in: index corresponding to page */
{
  ulint flags;
  ulint val;
  ulint pos;
  trx_id_t trx_id;
  roll_ptr_t roll_ptr;
  ulint offset;
  rec_t *rec;

  ut_ad(!page || !!page_is_comp(page) == dict_table_is_comp(index->table));

  if (end_ptr < ptr + 2) {
    return (NULL);
  }

  flags = mach_read_from_1(ptr);
  ptr++;
  val = mach_read_from_1(ptr);
  ptr++;

  ptr = row_upd_parse_sys_vals(ptr, end_ptr, &pos, &trx_id, &roll_ptr);

  if (ptr == NULL) {
    return (NULL);
  }

  if (end_ptr < ptr + 2) {
    return (NULL);
  }

  offset = mach_read_from_2(ptr);
  ptr += 2;

  ut_a(offset <= UNIV_PAGE_SIZE);

  if (page) {
    rec = page + offset;

    /* We do not need to reserve search latch, as the page
    is only being recovered, and there cannot be a hash index to
    it. Besides, these fields are being updated in place
    and the adaptive hash index does not depend on them. */

    btr_rec_set_deleted_flag(rec, page_zip, val);

    if (!(flags & BTR_KEEP_SYS_FLAG)) {
      mem_heap_t *heap = NULL;
      ulint offsets_[REC_OFFS_NORMAL_SIZE];
      rec_offs_init(offsets_);

      row_upd_rec_sys_fields_in_recovery(
          rec, page_zip,
          rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap), pos,
          trx_id, roll_ptr);
      if (UNIV_LIKELY_NULL(heap)) {
        mem_heap_free(heap);
      }
    }
  }

  return (ptr);
}

#ifndef UNIV_HOTBACKUP
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
    const dtuple_t *entry, /*!< in: dtuple for the deleting record, also
                           contains the virtual cols if there are any */
    mtr_t *mtr)            /*!< in/out: mini-transaction */
{
  roll_ptr_t roll_ptr;
  dberr_t err;
  page_zip_des_t *page_zip;
  trx_t *trx;

  ut_ad(index->is_clustered());
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));
  ut_ad(buf_block_get_frame(block) == page_align(rec));
  ut_ad(page_is_leaf(page_align(rec)));

  if (rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
    /* While cascading delete operations, this becomes possible. */
    ut_ad(rec_get_trx_id(rec, index) == thr_get_trx(thr)->id);
    return (DB_SUCCESS);
  }

  err = lock_clust_rec_modify_check_and_lock(BTR_NO_LOCKING_FLAG, block, rec,
                                             index, offsets, thr);

  if (err != DB_SUCCESS) {
    return (err);
  }

  err = trx_undo_report_row_operation(flags, TRX_UNDO_MODIFY_OP, thr, index,
                                      entry, NULL, 0, rec, offsets, &roll_ptr);
  if (err != DB_SUCCESS) {
    return (err);
  }

  /* The search latch is not needed here, because
  the adaptive hash index does not depend on the delete-mark
  and the delete-mark is being updated in place. */

  page_zip = buf_block_get_page_zip(block);

  btr_rec_set_deleted_flag(rec, page_zip, TRUE);

  /* For intrinsic table, roll-ptr is not maintained as there is no UNDO
  logging. Skip updating it. */
  if (index->table->is_intrinsic()) {
    return (err);
  }

  trx = thr_get_trx(thr);
  /* This function must not be invoked during rollback
  (of a TRX_STATE_PREPARE transaction or otherwise). */
  ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));
  ut_ad(!trx->in_rollback);

  DBUG_PRINT("ib_cur",
             ("delete-mark clust %s (" IB_ID_FMT ") by " TRX_ID_FMT ": %s",
              index->table_name, index->id, trx_get_id_for_print(trx),
              rec_printer(rec, offsets).str().c_str()));

  if (dict_index_is_online_ddl(index)) {
    row_log_table_delete(trx, rec, entry, index, offsets, NULL);
  }

  row_upd_rec_sys_fields(rec, page_zip, index, offsets, trx, roll_ptr);

  btr_cur_del_mark_set_clust_rec_log(rec, index, trx->id, roll_ptr, mtr);

  return (err);
}

/** Writes the redo log record for a delete mark setting of a secondary
 index record. */
UNIV_INLINE
void btr_cur_del_mark_set_sec_rec_log(rec_t *rec, /*!< in: record */
                                      ibool val,  /*!< in: value to set */
                                      mtr_t *mtr) /*!< in: mtr */
{
  byte *log_ptr;
  ut_ad(val <= 1);

  log_ptr = mlog_open(mtr, 11 + 1 + 2);

  if (!log_ptr) {
    /* Logging in mtr is switched off during crash recovery:
    in that case mlog_open returns NULL */
    return;
  }

  log_ptr = mlog_write_initial_log_record_fast(rec, MLOG_REC_SEC_DELETE_MARK,
                                               log_ptr, mtr);
  mach_write_to_1(log_ptr, val);
  log_ptr++;

  mach_write_to_2(log_ptr, page_offset(rec));
  log_ptr += 2;

  mlog_close(mtr, log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses the redo log record for delete marking or unmarking of a secondary
 index record.
 @return end of log record or NULL */
byte *btr_cur_parse_del_mark_set_sec_rec(
    byte *ptr,                /*!< in: buffer */
    byte *end_ptr,            /*!< in: buffer end */
    page_t *page,             /*!< in/out: page or NULL */
    page_zip_des_t *page_zip) /*!< in/out: compressed page, or NULL */
{
  ulint val;
  ulint offset;
  rec_t *rec;

  if (end_ptr < ptr + 3) {
    return (NULL);
  }

  val = mach_read_from_1(ptr);
  ptr++;

  offset = mach_read_from_2(ptr);
  ptr += 2;

  ut_a(offset <= UNIV_PAGE_SIZE);

  if (page) {
    rec = page + offset;

    /* We do not need to reserve search latch, as the page
    is only being recovered, and there cannot be a hash index to
    it. Besides, the delete-mark flag is being updated in place
    and the adaptive hash index does not depend on it. */

    btr_rec_set_deleted_flag(rec, page_zip, val);
  }

  return (ptr);
}

#ifndef UNIV_HOTBACKUP
/** Sets a secondary index record delete mark to TRUE or FALSE.
 @return DB_SUCCESS, DB_LOCK_WAIT, or error number */
dberr_t btr_cur_del_mark_set_sec_rec(
    ulint flags,       /*!< in: locking flag */
    btr_cur_t *cursor, /*!< in: cursor */
    ibool val,         /*!< in: value to set */
    que_thr_t *thr,    /*!< in: query thread */
    mtr_t *mtr)        /*!< in/out: mini-transaction */
{
  buf_block_t *block;
  rec_t *rec;
  dberr_t err;

  block = btr_cur_get_block(cursor);
  rec = btr_cur_get_rec(cursor);

  err = lock_sec_rec_modify_check_and_lock(flags, btr_cur_get_block(cursor),
                                           rec, cursor->index, thr, mtr);
  if (err != DB_SUCCESS) {
    return (err);
  }

  ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(cursor->index->table));

  DBUG_PRINT("ib_cur",
             ("delete-mark=%u sec %u:%u:%u in %s(" IB_ID_FMT ") by " TRX_ID_FMT,
              unsigned(val), block->page.id.space(), block->page.id.page_no(),
              unsigned(page_rec_get_heap_no(rec)), cursor->index->name(),
              cursor->index->id, trx_get_id_for_print(thr_get_trx(thr))));

  /* We do not need to reserve search latch, as the
  delete-mark flag is being updated in place and the adaptive
  hash index does not depend on it. */
  btr_rec_set_deleted_flag(rec, buf_block_get_page_zip(block), val);

  btr_cur_del_mark_set_sec_rec_log(rec, val, mtr);

  return (DB_SUCCESS);
}

/** Sets a secondary index record's delete mark to the given value. This
 function is only used by the insert buffer merge mechanism. */
void btr_cur_set_deleted_flag_for_ibuf(
    rec_t *rec,               /*!< in/out: record */
    page_zip_des_t *page_zip, /*!< in/out: compressed page
                              corresponding to rec, or NULL
                              when the tablespace is
                              uncompressed */
    ibool val,                /*!< in: value to set */
    mtr_t *mtr)               /*!< in/out: mini-transaction */
{
  /* We do not need to reserve search latch, as the page
  has just been read to the buffer pool and there cannot be
  a hash index to it.  Besides, the delete-mark flag is being
  updated in place and the adaptive hash index does not depend
  on it. */

  btr_rec_set_deleted_flag(rec, page_zip, val);

  btr_cur_del_mark_set_sec_rec_log(rec, val, mtr);
}

/*==================== B-TREE RECORD REMOVE =========================*/

/** Tries to compress a page of the tree if it seems useful. It is assumed
 that mtr holds an x-latch on the tree and on the cursor page. To avoid
 deadlocks, mtr must also own x-latches to brothers of page, if those
 brothers exist. NOTE: it is assumed that the caller has reserved enough
 free extents so that the compression will always succeed if done!
 @return true if compression occurred */
ibool btr_cur_compress_if_useful(
    btr_cur_t *cursor, /*!< in/out: cursor on the page to compress;
                       cursor does not stay valid if !adjust and
                       compression occurs */
    ibool adjust,      /*!< in: TRUE if should adjust the
                       cursor position even if compression occurs */
    mtr_t *mtr)        /*!< in/out: mini-transaction */
{
  /* Avoid applying compression as we don't accept lot of page garbage
  given the workload of intrinsic table. */
  if (cursor->index->table->is_intrinsic()) {
    return (FALSE);
  }

  ut_ad(mtr_memo_contains_flagged(
            mtr, dict_index_get_lock(btr_cur_get_index(cursor)),
            MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
        cursor->index->table->is_intrinsic());
  ut_ad(mtr_is_block_fix(mtr, btr_cur_get_block(cursor), MTR_MEMO_PAGE_X_FIX,
                         cursor->index->table));

  if (dict_index_is_spatial(cursor->index)) {
    const page_t *page = btr_cur_get_page(cursor);
    const trx_t *trx = NULL;

    if (cursor->rtr_info->thr != NULL) {
      trx = thr_get_trx(cursor->rtr_info->thr);
    }

    /* Check whether page lock prevents the compression */
    if (!lock_test_prdt_page_lock(trx, page_get_space_id(page),
                                  page_get_page_no(page))) {
      return (false);
    }
  }

  return (btr_cur_compress_recommendation(cursor, mtr) &&
          btr_compress(cursor, adjust, mtr));
}

/** Removes the record on which the tree cursor is positioned on a leaf page.
 It is assumed that the mtr has an x-latch on the page where the cursor is
 positioned, but no latch on the whole tree.
 @return true if success, i.e., the page did not become too empty */
ibool btr_cur_optimistic_delete_func(
    btr_cur_t *cursor, /*!< in: cursor on leaf page, on the record to
                       delete; cursor stays valid: if deletion
                       succeeds, on function exit it points to the
                       successor of the deleted record */
#ifdef UNIV_DEBUG
    ulint flags, /*!< in: BTR_CREATE_FLAG or 0 */
#endif           /* UNIV_DEBUG */
    mtr_t *mtr)  /*!< in: mtr; if this function returns
                 TRUE on a leaf page of a secondary
                 index, the mtr must be committed
                 before latching any further pages */
{
  buf_block_t *block;
  rec_t *rec;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  ibool no_compress_needed;
  rec_offs_init(offsets_);

  ut_ad(flags == 0 || flags == BTR_CREATE_FLAG);
  ut_ad(mtr_memo_contains(mtr, btr_cur_get_block(cursor), MTR_MEMO_PAGE_X_FIX));
  ut_ad(mtr_is_block_fix(mtr, btr_cur_get_block(cursor), MTR_MEMO_PAGE_X_FIX,
                         cursor->index->table));

  /* This is intended only for leaf page deletions */

  block = btr_cur_get_block(cursor);

  ut_ad(page_is_leaf(buf_block_get_frame(block)));
  ut_ad(!dict_index_is_online_ddl(cursor->index) ||
        cursor->index->is_clustered() || (flags & BTR_CREATE_FLAG));

  rec = btr_cur_get_rec(cursor);
  offsets =
      rec_get_offsets(rec, cursor->index, offsets, ULINT_UNDEFINED, &heap);

  no_compress_needed =
      !rec_offs_any_extern(offsets) &&
      btr_cur_can_delete_without_compress(cursor, rec_offs_size(offsets), mtr);

  if (no_compress_needed) {
    page_t *page = buf_block_get_frame(block);
    page_zip_des_t *page_zip = buf_block_get_page_zip(block);

    lock_update_delete(block, rec);

    btr_search_update_hash_on_delete(cursor);

    if (page_zip) {
#ifdef UNIV_ZIP_DEBUG
      ut_a(page_zip_validate(page_zip, page, cursor->index));
#endif /* UNIV_ZIP_DEBUG */
      page_cur_delete_rec(btr_cur_get_page_cur(cursor), cursor->index, offsets,
                          mtr);
#ifdef UNIV_ZIP_DEBUG
      ut_a(page_zip_validate(page_zip, page, cursor->index));
#endif /* UNIV_ZIP_DEBUG */

      /* On compressed pages, the IBUF_BITMAP_FREE
      space is not affected by deleting (purging)
      records, because it is defined as the minimum
      of space available *without* reorganize, and
      space available in the modification log. */
    } else {
      const ulint max_ins = page_get_max_insert_size_after_reorganize(page, 1);

      page_cur_delete_rec(btr_cur_get_page_cur(cursor), cursor->index, offsets,
                          mtr);

      /* The change buffer does not handle inserts
      into non-leaf pages, into clustered indexes,
      or into the change buffer. */
      if (!cursor->index->is_clustered() &&
          !cursor->index->table->is_temporary() &&
          !dict_index_is_ibuf(cursor->index)) {
        ibuf_update_free_bits_low(block, max_ins, mtr);
      }
    }
  } else {
    /* prefetch siblings of the leaf for the pessimistic
    operation. */
    btr_cur_prefetch_siblings(block);
  }

  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }

  return (no_compress_needed);
}

/** Removes the record on which the tree cursor is positioned. Tries
 to compress the page if its fillfactor drops below a threshold
 or if it is the only page on the level. It is assumed that mtr holds
 an x-latch on the tree and on the cursor page. To avoid deadlocks,
 mtr must also own x-latches to brothers of page, if those brothers
 exist.
 @return true if compression occurred and false if not or something
 wrong. */
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
    /*!< in: the undo number within the
    current trx, used for rollback to savepoint
    for an LOB. */
    ulint rec_type,
    /*!< in: undo record type. */
    mtr_t *mtr) /*!< in: mtr */
{
  DBUG_ENTER("btr_cur_pessimistic_delete");

  DBUG_LOG("btr", "rollback=" << rollback << ", trxid=" << trx_id);

  buf_block_t *block;
  page_t *page;
  dict_index_t *index;
  rec_t *rec;
  ulint n_reserved = 0;
  bool success;
  ibool ret = FALSE;
  ulint level;
  mem_heap_t *heap;
  ulint *offsets;
#ifdef UNIV_DEBUG
  bool parent_latched = false;
#endif /* UNIV_DEBUG */

  block = btr_cur_get_block(cursor);
  page = buf_block_get_frame(block);
  index = btr_cur_get_index(cursor);

  ut_ad(flags == 0 || flags == BTR_CREATE_FLAG);
  ut_ad(!dict_index_is_online_ddl(index) || index->is_clustered() ||
        (flags & BTR_CREATE_FLAG));
  ut_ad(mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                  MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
        index->table->is_intrinsic());
  ut_ad(mtr_is_block_fix(mtr, block, MTR_MEMO_PAGE_X_FIX, index->table));

  if (!has_reserved_extents) {
    /* First reserve enough free space for the file segments
    of the index tree, so that the node pointer updates will
    not fail because of lack of space */

    ulint n_extents = cursor->tree_height / 32 + 1;

    success = fsp_reserve_free_extents(&n_reserved, index->space, n_extents,
                                       FSP_CLEANING, mtr);
    if (!success) {
      *err = DB_OUT_OF_FILE_SPACE;

      DBUG_RETURN(FALSE);
    }
  }

  heap = mem_heap_create(1024);
  rec = btr_cur_get_rec(cursor);
#ifdef UNIV_ZIP_DEBUG
  page_zip_des_t *page_zip = buf_block_get_page_zip(block);
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

  offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

  if (rec_offs_any_extern(offsets)) {
    lob::BtrContext btr_ctx(mtr, NULL, index, rec, offsets, block);

    btr_ctx.free_externally_stored_fields(trx_id, undo_no, rollback, rec_type);
#ifdef UNIV_ZIP_DEBUG
    ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
  }

  if (UNIV_UNLIKELY(page_get_n_recs(page) < 2) &&
      UNIV_UNLIKELY(dict_index_get_page(index) != block->page.id.page_no())) {
    /* If there is only one record, drop the whole page in
    btr_discard_page, if this is not the root page */

    btr_discard_page(cursor, mtr);

    ret = TRUE;

    goto return_after_reservations;
  }

  if (flags == 0) {
    lock_update_delete(block, rec);
  }

  level = btr_page_get_level(page, mtr);

  if (level > 0 &&
      UNIV_UNLIKELY(rec == page_rec_get_next(page_get_infimum_rec(page)))) {
    rec_t *next_rec = page_rec_get_next(rec);

    if (btr_page_get_prev(page, mtr) == FIL_NULL) {
      /* If we delete the leftmost node pointer on a
      non-leaf level, we must mark the new leftmost node
      pointer as the predefined minimum record */

      /* This will make page_zip_validate() fail until
      page_cur_delete_rec() completes.  This is harmless,
      because everything will take place within a single
      mini-transaction and because writing to the redo log
      is an atomic operation (performed by mtr_commit()). */
      btr_set_min_rec_mark(next_rec, mtr);
    } else if (dict_index_is_spatial(index)) {
      /* For rtree, if delete the leftmost node pointer,
      we need to update parent page. */
      rtr_mbr_t father_mbr;
      rec_t *father_rec;
      btr_cur_t father_cursor;
      ulint *offsets;
      bool upd_ret;
      ulint len;

      rtr_page_get_father_block(NULL, heap, index, block, mtr, NULL,
                                &father_cursor);
      offsets = rec_get_offsets(btr_cur_get_rec(&father_cursor), index, NULL,
                                ULINT_UNDEFINED, &heap);

      father_rec = btr_cur_get_rec(&father_cursor);
      rtr_read_mbr(rec_get_nth_field(father_rec, offsets, 0, &len),
                   &father_mbr);

      upd_ret = rtr_update_mbr_field(&father_cursor, offsets, NULL, page,
                                     &father_mbr, next_rec, mtr);

      if (!upd_ret) {
        *err = DB_ERROR;

        mem_heap_free(heap);
        DBUG_RETURN(FALSE);
      }

      ut_d(parent_latched = true);
    } else {
      /* Otherwise, if we delete the leftmost node pointer
      on a page, we have to change the parent node pointer
      so that it is equal to the new leftmost node pointer
      on the page */

      btr_node_ptr_delete(index, block, mtr);

      dtuple_t *node_ptr = dict_index_build_node_ptr(
          index, next_rec, block->page.id.page_no(), heap, level);

      btr_insert_on_non_leaf_level(flags, index, level + 1, node_ptr, mtr);

      ut_d(parent_latched = true);
    }
  }

  btr_search_update_hash_on_delete(cursor);

  page_cur_delete_rec(btr_cur_get_page_cur(cursor), index, offsets, mtr);
#ifdef UNIV_ZIP_DEBUG
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

  /* btr_check_node_ptr() needs parent block latched */
  ut_ad(!parent_latched || btr_check_node_ptr(index, block, mtr));

return_after_reservations:
  *err = DB_SUCCESS;

  mem_heap_free(heap);

  if (ret == FALSE) {
    ret = btr_cur_compress_if_useful(cursor, FALSE, mtr);
  }

  if (!srv_read_only_mode && page_is_leaf(page) &&
      !dict_index_is_online_ddl(index)) {
    mtr_memo_release(mtr, dict_index_get_lock(index),
                     MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK);

    /* NOTE: We cannot release root block latch here, because it
    has segment header and already modified in most of cases.*/
  }

  if (n_reserved > 0) {
    fil_space_release_free_extents(index->space, n_reserved);
  }

  DBUG_RETURN(ret);
}

/** Adds path information to the cursor for the current page, for which
 the binary search has been performed. */
static void btr_cur_add_path_info(
    btr_cur_t *cursor, /*!< in: cursor positioned on a page */
    ulint height,      /*!< in: height of the page in tree;
                       0 means leaf node */
    ulint root_height) /*!< in: root node height in tree */
{
  btr_path_t *slot;
  const rec_t *rec;
  const page_t *page;

  ut_a(cursor->path_arr);

  if (root_height >= BTR_PATH_ARRAY_N_SLOTS - 1) {
    /* Do nothing; return empty path */

    slot = cursor->path_arr;
    slot->nth_rec = ULINT_UNDEFINED;

    return;
  }

  if (height == 0) {
    /* Mark end of slots for path */
    slot = cursor->path_arr + root_height + 1;
    slot->nth_rec = ULINT_UNDEFINED;
  }

  rec = btr_cur_get_rec(cursor);

  slot = cursor->path_arr + (root_height - height);

  page = page_align(rec);

  slot->nth_rec = page_rec_get_n_recs_before(rec);
  slot->n_recs = page_get_n_recs(page);
  slot->page_no = page_get_page_no(page);
  slot->page_level = btr_page_get_level_low(page);
}

/** Estimate the number of rows between slot1 and slot2 for any level on a
 B-tree. This function starts from slot1->page and reads a few pages to
 the right, counting their records. If we reach slot2->page quickly then
 we know exactly how many records there are between slot1 and slot2 and
 we set is_n_rows_exact to TRUE. If we cannot reach slot2->page quickly
 then we calculate the average number of records in the pages scanned
 so far and assume that all pages that we did not scan up to slot2->page
 contain the same number of records, then we multiply that average to
 the number of pages between slot1->page and slot2->page (which is
 n_rows_on_prev_level). In this case we set is_n_rows_exact to FALSE.
 @return number of rows, not including the borders (exact or estimated) */
static int64_t btr_estimate_n_rows_in_range_on_level(
    dict_index_t *index,          /*!< in: index */
    btr_path_t *slot1,            /*!< in: left border */
    btr_path_t *slot2,            /*!< in: right border */
    int64_t n_rows_on_prev_level, /*!< in: number of rows
                                  on the previous level for the
                                  same descend paths; used to
                                  determine the number of pages
                                  on this level */
    ibool *is_n_rows_exact)       /*!< out: TRUE if the returned
                                  value is exact i.e. not an
                                  estimation */
{
  int64_t n_rows;
  ulint n_pages_read;
  ulint level;

  n_rows = 0;
  n_pages_read = 0;

  /* Assume by default that we will scan all pages between
  slot1->page_no and slot2->page_no. */
  *is_n_rows_exact = TRUE;

  /* Add records from slot1->page_no which are to the right of
  the record which serves as a left border of the range, if any
  (we don't include the record itself in this count). */
  if (slot1->nth_rec <= slot1->n_recs) {
    n_rows += slot1->n_recs - slot1->nth_rec;
  }

  /* Add records from slot2->page_no which are to the left of
  the record which servers as a right border of the range, if any
  (we don't include the record itself in this count). */
  if (slot2->nth_rec > 1) {
    n_rows += slot2->nth_rec - 1;
  }

    /* Count the records in the pages between slot1->page_no and
    slot2->page_no (non inclusive), if any. */

    /* Do not read more than this number of pages in order not to hurt
    performance with this code which is just an estimation. If we read
    this many pages before reaching slot2->page_no then we estimate the
    average from the pages scanned so far. */
#define N_PAGES_READ_LIMIT 10

  page_id_t page_id(dict_index_get_space(index), slot1->page_no);
  const fil_space_t *space = fil_space_get(index->space);
  ut_ad(space);
  const page_size_t page_size(space->flags);

  level = slot1->page_level;

  do {
    mtr_t mtr;
    page_t *page;
    buf_block_t *block;

    mtr_start(&mtr);

    /* Fetch the page. Because we are not holding the
    index->lock, the tree may have changed and we may be
    attempting to read a page that is no longer part of
    the B-tree. We pass BUF_GET_POSSIBLY_FREED in order to
    silence a debug assertion about this. */
    block = buf_page_get_gen(page_id, page_size, RW_S_LATCH, NULL,
                             BUF_GET_POSSIBLY_FREED, __FILE__, __LINE__, &mtr);

    page = buf_block_get_frame(block);

    /* It is possible that the tree has been reorganized in the
    meantime and this is a different page. If this happens the
    calculated estimate will be bogus, which is not fatal as
    this is only an estimate. We are sure that a page with
    page_no exists because InnoDB never frees pages, only
    reuses them. */
    if (!fil_page_index_page_check(page) ||
        btr_page_get_index_id(page) != index->id ||
        btr_page_get_level_low(page) != level) {
      /* The page got reused for something else */
      mtr_commit(&mtr);
      goto inexact;
    }

    /* It is possible but highly unlikely that the page was
    originally written by an old version of InnoDB that did
    not initialize FIL_PAGE_TYPE on other than B-tree pages.
    For example, this could be an almost-empty BLOB page
    that happens to contain the magic values in the fields
    that we checked above. */

    n_pages_read++;

    if (page_id.page_no() != slot1->page_no) {
      /* Do not count the records on slot1->page_no,
      we already counted them before this loop. */
      n_rows += page_get_n_recs(page);
    }

    page_id.set_page_no(btr_page_get_next(page, &mtr));

    mtr_commit(&mtr);

    if (n_pages_read == N_PAGES_READ_LIMIT || page_id.page_no() == FIL_NULL) {
      /* Either we read too many pages or
      we reached the end of the level without passing
      through slot2->page_no, the tree must have changed
      in the meantime */
      goto inexact;
    }

  } while (page_id.page_no() != slot2->page_no);

  return (n_rows);

inexact:

  *is_n_rows_exact = FALSE;

  /* We did interrupt before reaching slot2->page */

  if (n_pages_read > 0) {
    /* The number of pages on this level is
    n_rows_on_prev_level, multiply it by the
    average number of recs per page so far */
    n_rows = n_rows_on_prev_level * n_rows / n_pages_read;
  } else {
    /* The tree changed before we could even
    start with slot1->page_no */
    n_rows = 10;
  }

  return (n_rows);
}

/** If the tree gets changed too much between the two dives for the left
and right boundary then btr_estimate_n_rows_in_range_low() will retry
that many times before giving up and returning the value stored in
rows_in_range_arbitrary_ret_val. */
static const unsigned rows_in_range_max_retries = 4;

/** We pretend that a range has that many records if the tree keeps changing
for rows_in_range_max_retries retries while we try to estimate the records
in a given range. */
static const int64_t rows_in_range_arbitrary_ret_val = 10;

/** Estimates the number of rows in a given index range.
@param[in]	index		index
@param[in]	tuple1		range start, may also be empty tuple
@param[in]	mode1		search mode for range start
@param[in]	tuple2		range end, may also be empty tuple
@param[in]	mode2		search mode for range end
@param[in]	nth_attempt	if the tree gets modified too much while
we are trying to analyze it, then we will retry (this function will call
itself, incrementing this parameter)
@return estimated number of rows; if after rows_in_range_max_retries
retries the tree keeps changing, then we will just return
rows_in_range_arbitrary_ret_val as a result (if
nth_attempt >= rows_in_range_max_retries and the tree is modified between
the two dives). */
static int64_t btr_estimate_n_rows_in_range_low(
    dict_index_t *index, const dtuple_t *tuple1, page_cur_mode_t mode1,
    const dtuple_t *tuple2, page_cur_mode_t mode2, unsigned nth_attempt) {
  btr_path_t path1[BTR_PATH_ARRAY_N_SLOTS];
  btr_path_t path2[BTR_PATH_ARRAY_N_SLOTS];
  btr_cur_t cursor;
  btr_path_t *slot1;
  btr_path_t *slot2;
  ibool diverged;
  ibool diverged_lot;
  ulint divergence_level;
  int64_t n_rows;
  ibool is_n_rows_exact;
  ulint i;
  mtr_t mtr;
  int64_t table_n_rows;

  table_n_rows = dict_table_get_n_rows(index->table);

  /* Below we dive to the two records specified by tuple1 and tuple2 and
  we remember the entire dive paths from the tree root. The place where
  the tuple1 path ends on the leaf level we call "left border" of our
  interval and the place where the tuple2 path ends on the leaf level -
  "right border". We take care to either include or exclude the interval
  boundaries depending on whether <, <=, > or >= was specified. For
  example if "5 < x AND x <= 10" then we should not include the left
  boundary, but should include the right one. */

  mtr_start(&mtr);

  cursor.path_arr = path1;

  bool should_count_the_left_border;

  if (dtuple_get_n_fields(tuple1) > 0) {
    btr_cur_search_to_nth_level(index, 0, tuple1, mode1,
                                BTR_SEARCH_LEAF | BTR_ESTIMATE, &cursor, 0,
                                __FILE__, __LINE__, &mtr);

    ut_ad(!page_rec_is_infimum(btr_cur_get_rec(&cursor)));

    /* We should count the border if there are any records to
    match the criteria, i.e. if the maximum record on the tree is
    5 and x > 3 is specified then the cursor will be positioned at
    5 and we should count the border, but if x > 7 is specified,
    then the cursor will be positioned at 'sup' on the rightmost
    leaf page in the tree and we should not count the border. */
    should_count_the_left_border =
        !page_rec_is_supremum(btr_cur_get_rec(&cursor));
  } else {
    btr_cur_open_at_index_side(true, index, BTR_SEARCH_LEAF | BTR_ESTIMATE,
                               &cursor, 0, &mtr);

    ut_ad(page_rec_is_infimum(btr_cur_get_rec(&cursor)));

    /* The range specified is wihout a left border, just
    'x < 123' or 'x <= 123' and btr_cur_open_at_index_side()
    positioned the cursor on the infimum record on the leftmost
    page, which must not be counted. */
    should_count_the_left_border = false;
  }

  mtr_commit(&mtr);

  mtr_start(&mtr);

  cursor.path_arr = path2;

  bool should_count_the_right_border;

  if (dtuple_get_n_fields(tuple2) > 0) {
    btr_cur_search_to_nth_level(index, 0, tuple2, mode2,
                                BTR_SEARCH_LEAF | BTR_ESTIMATE, &cursor, 0,
                                __FILE__, __LINE__, &mtr);

    const rec_t *rec = btr_cur_get_rec(&cursor);

    ut_ad(!(mode2 == PAGE_CUR_L && page_rec_is_supremum(rec)));

    should_count_the_right_border =
        (mode2 == PAGE_CUR_LE /* if the range is '<=' */
         /* and the record was found */
         && cursor.low_match >= dtuple_get_n_fields(tuple2)) ||
        (mode2 == PAGE_CUR_L /* or if the range is '<' */
         /* and there are any records to match the criteria,
         i.e. if the minimum record on the tree is 5 and
         x < 7 is specified then the cursor will be
         positioned at 5 and we should count the border, but
         if x < 2 is specified, then the cursor will be
         positioned at 'inf' and we should not count the
         border */
         && !page_rec_is_infimum(rec));
    /* Notice that for "WHERE col <= 'foo'" MySQL passes to
    ha_innobase::records_in_range():
    min_key=NULL (left-unbounded) which is expected
    max_key='foo' flag=HA_READ_AFTER_KEY (PAGE_CUR_G), which is
    unexpected - one would expect
    flag=HA_READ_KEY_OR_PREV (PAGE_CUR_LE). In this case the
    cursor will be positioned on the first record to the right of
    the requested one (can also be positioned on the 'sup') and
    we should not count the right border. */
  } else {
    btr_cur_open_at_index_side(false, index, BTR_SEARCH_LEAF | BTR_ESTIMATE,
                               &cursor, 0, &mtr);

    ut_ad(page_rec_is_supremum(btr_cur_get_rec(&cursor)));

    /* The range specified is wihout a right border, just
    'x > 123' or 'x >= 123' and btr_cur_open_at_index_side()
    positioned the cursor on the supremum record on the rightmost
    page, which must not be counted. */
    should_count_the_right_border = false;
  }

  mtr_commit(&mtr);

  /* We have the path information for the range in path1 and path2 */

  n_rows = 0;
  is_n_rows_exact = TRUE;

  /* This becomes true when the two paths do not pass through the
  same pages anymore. */
  diverged = FALSE;

  /* This becomes true when the paths are not the same or adjacent
  any more. This means that they pass through the same or
  neighboring-on-the-same-level pages only. */
  diverged_lot = FALSE;

  /* This is the level where paths diverged a lot. */
  divergence_level = 1000000;

  for (i = 0;; i++) {
    ut_ad(i < BTR_PATH_ARRAY_N_SLOTS);

    slot1 = path1 + i;
    slot2 = path2 + i;

    if (slot1->nth_rec == ULINT_UNDEFINED ||
        slot2->nth_rec == ULINT_UNDEFINED) {
      /* Here none of the borders were counted. For example,
      if on the leaf level we descended to:
      (inf, a, b, c, d, e, f, sup)
               ^        ^
             path1    path2
      then n_rows will be 2 (c and d). */

      if (is_n_rows_exact) {
        /* Only fiddle to adjust this off-by-one
        if the number is exact, otherwise we do
        much grosser adjustments below. */

        btr_path_t *last1 = &path1[i - 1];
        btr_path_t *last2 = &path2[i - 1];

        /* If both paths end up on the same record on
        the leaf level. */
        if (last1->page_no == last2->page_no &&
            last1->nth_rec == last2->nth_rec) {
          /* n_rows can be > 0 here if the paths
          were first different and then converged
          to the same record on the leaf level.
          For example:
          SELECT ... LIKE 'wait/synch/rwlock%'
          mode1=PAGE_CUR_GE,
          tuple1="wait/synch/rwlock"
          path1[0]={nth_rec=58, n_recs=58,
                    page_no=3, page_level=1}
          path1[1]={nth_rec=56, n_recs=55,
                    page_no=119, page_level=0}

          mode2=PAGE_CUR_G
          tuple2="wait/synch/rwlock"
          path2[0]={nth_rec=57, n_recs=57,
                    page_no=3, page_level=1}
          path2[1]={nth_rec=56, n_recs=55,
                    page_no=119, page_level=0} */

          /* If the range is such that we should
          count both borders, then avoid
          counting that record twice - once as a
          left border and once as a right
          border. */
          if (should_count_the_left_border && should_count_the_right_border) {
            n_rows = 1;
          } else {
            /* Some of the borders should
            not be counted, e.g. [3,3). */
            n_rows = 0;
          }
        } else {
          if (should_count_the_left_border) {
            n_rows++;
          }

          if (should_count_the_right_border) {
            n_rows++;
          }
        }
      }

      if (i > divergence_level + 1 && !is_n_rows_exact) {
        /* In trees whose height is > 1 our algorithm
        tends to underestimate: multiply the estimate
        by 2: */

        n_rows = n_rows * 2;
      }

      DBUG_EXECUTE_IF("bug14007649", return (n_rows););

      /* Do not estimate the number of rows in the range
      to over 1 / 2 of the estimated rows in the whole
      table */

      if (n_rows > table_n_rows / 2 && !is_n_rows_exact) {
        n_rows = table_n_rows / 2;

        /* If there are just 0 or 1 rows in the table,
        then we estimate all rows are in the range */

        if (n_rows == 0) {
          n_rows = table_n_rows;
        }
      }

      return (n_rows);
    }

    if (!diverged && slot1->nth_rec != slot2->nth_rec) {
      /* If both slots do not point to the same page,
      this means that the tree must have changed between
      the dive for slot1 and the dive for slot2 at the
      beginning of this function. */
      if (slot1->page_no != slot2->page_no ||
          slot1->page_level != slot2->page_level) {
        /* If the tree keeps changing even after a
        few attempts, then just return some arbitrary
        number. */
        if (nth_attempt >= rows_in_range_max_retries) {
          return (rows_in_range_arbitrary_ret_val);
        }

        const int64_t ret = btr_estimate_n_rows_in_range_low(
            index, tuple1, mode1, tuple2, mode2, nth_attempt + 1);

        return (ret);
      }

      diverged = TRUE;

      if (slot1->nth_rec < slot2->nth_rec) {
        /* We do not count the borders (nor the left
        nor the right one), thus "- 1". */
        n_rows = slot2->nth_rec - slot1->nth_rec - 1;

        if (n_rows > 0) {
          /* There is at least one row between
          the two borders pointed to by slot1
          and slot2, so on the level below the
          slots will point to non-adjacent
          pages. */
          diverged_lot = TRUE;
          divergence_level = i;
        }
      } else {
        /* It is possible that
        slot1->nth_rec >= slot2->nth_rec
        if, for example, we have a single page
        tree which contains (inf, 5, 6, supr)
        and we select where x > 20 and x < 30;
        in this case slot1->nth_rec will point
        to the supr record and slot2->nth_rec
        will point to 6. */
        n_rows = 0;
        should_count_the_left_border = false;
        should_count_the_right_border = false;
      }

    } else if (diverged && !diverged_lot) {
      if (slot1->nth_rec < slot1->n_recs || slot2->nth_rec > 1) {
        diverged_lot = TRUE;
        divergence_level = i;

        n_rows = 0;

        if (slot1->nth_rec < slot1->n_recs) {
          n_rows += slot1->n_recs - slot1->nth_rec;
        }

        if (slot2->nth_rec > 1) {
          n_rows += slot2->nth_rec - 1;
        }
      }
    } else if (diverged_lot) {
      n_rows = btr_estimate_n_rows_in_range_on_level(index, slot1, slot2,
                                                     n_rows, &is_n_rows_exact);
    }
  }
}

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
                                     page_cur_mode_t mode2) {
  const int64_t ret = btr_estimate_n_rows_in_range_low(
      index, tuple1, mode1, tuple2, mode2, 1 /* first attempt */);

  return (ret);
}

/** Record the number of non_null key values in a given index for
 each n-column prefix of the index where 1 <= n <=
 dict_index_get_n_unique(index). The estimates are eventually stored in the
 array: index->stat_n_non_null_key_vals[], which is indexed from 0 to n-1. */
static void btr_record_not_null_field_in_rec(
    ulint n_unique,          /*!< in: dict_index_get_n_unique(index),
                             number of columns uniquely determine
                             an index entry */
    const ulint *offsets,    /*!< in: rec_get_offsets(rec, index),
                             its size could be for all fields or
                             that of "n_unique" */
    ib_uint64_t *n_not_null) /*!< in/out: array to record number of
                             not null rows for n-column prefix */
{
  ulint i;

  ut_ad(rec_offs_n_fields(offsets) >= n_unique);

  if (n_not_null == NULL) {
    return;
  }

  for (i = 0; i < n_unique; i++) {
    if (rec_offs_nth_sql_null(offsets, i)) {
      break;
    }

    n_not_null[i]++;
  }
}

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
    dict_index_t *index) /*!< in: index */
{
  btr_cur_t cursor;
  page_t *page;
  rec_t *rec;
  ulint n_cols;
  ib_uint64_t *n_diff;
  ib_uint64_t *n_not_null;
  ibool stats_null_not_equal;
  uintmax_t n_sample_pages; /* number of pages to sample */
  ulint not_empty_flag = 0;
  ulint total_external_size = 0;
  ulint i;
  ulint j;
  uintmax_t add_on;
  mtr_t mtr;
  mem_heap_t *heap = NULL;
  ulint *offsets_rec = NULL;
  ulint *offsets_next_rec = NULL;

  /* For spatial index, there is no such stats can be
  fetched. */
  if (dict_index_is_spatial(index)) {
    return (false);
  }

  n_cols = dict_index_get_n_unique(index);

  heap = mem_heap_create((sizeof *n_diff + sizeof *n_not_null) * n_cols +
                         dict_index_get_n_fields(index) *
                             (sizeof *offsets_rec + sizeof *offsets_next_rec));

  n_diff = (ib_uint64_t *)mem_heap_zalloc(heap, n_cols * sizeof(n_diff[0]));

  n_not_null = NULL;

  /* Check srv_innodb_stats_method setting, and decide whether we
  need to record non-null value and also decide if NULL is
  considered equal (by setting stats_null_not_equal value) */
  switch (srv_innodb_stats_method) {
    case SRV_STATS_NULLS_IGNORED:
      n_not_null =
          (ib_uint64_t *)mem_heap_zalloc(heap, n_cols * sizeof *n_not_null);
      /* fall through */

    case SRV_STATS_NULLS_UNEQUAL:
      /* for both SRV_STATS_NULLS_IGNORED and SRV_STATS_NULLS_UNEQUAL
      case, we will treat NULLs as unequal value */
      stats_null_not_equal = TRUE;
      break;

    case SRV_STATS_NULLS_EQUAL:
      stats_null_not_equal = FALSE;
      break;

    default:
      ut_error;
  }

  /* It makes no sense to test more pages than are contained
  in the index, thus we lower the number if it is too high */
  if (srv_stats_transient_sample_pages > index->stat_index_size) {
    if (index->stat_index_size > 0) {
      n_sample_pages = index->stat_index_size;
    } else {
      n_sample_pages = 1;
    }
  } else {
    n_sample_pages = srv_stats_transient_sample_pages;
  }

  /* We sample some pages in the index to get an estimate */

  for (i = 0; i < n_sample_pages; i++) {
    mtr_start(&mtr);

    bool available;

    available = btr_cur_open_at_rnd_pos(index, BTR_SEARCH_LEAF, &cursor, &mtr);

    if (!available) {
      mtr_commit(&mtr);
      mem_heap_free(heap);

      return (false);
    }

    /* Count the number of different key values for each prefix of
    the key on this index page. If the prefix does not determine
    the index record uniquely in the B-tree, then we subtract one
    because otherwise our algorithm would give a wrong estimate
    for an index where there is just one key value. */

    page = btr_cur_get_page(&cursor);

    rec = page_rec_get_next(page_get_infimum_rec(page));

    if (!page_rec_is_supremum(rec)) {
      not_empty_flag = 1;
      offsets_rec =
          rec_get_offsets(rec, index, offsets_rec, ULINT_UNDEFINED, &heap);

      if (n_not_null != NULL) {
        btr_record_not_null_field_in_rec(n_cols, offsets_rec, n_not_null);
      }
    }

    while (!page_rec_is_supremum(rec)) {
      ulint matched_fields;
      rec_t *next_rec = page_rec_get_next(rec);
      if (page_rec_is_supremum(next_rec)) {
        total_external_size +=
            lob::btr_rec_get_externally_stored_len(rec, offsets_rec);
        break;
      }

      offsets_next_rec = rec_get_offsets(next_rec, index, offsets_next_rec,
                                         ULINT_UNDEFINED, &heap);

      cmp_rec_rec_with_match(rec, next_rec, offsets_rec, offsets_next_rec,
                             index, stats_null_not_equal, &matched_fields);

      for (j = matched_fields; j < n_cols; j++) {
        /* We add one if this index record has
        a different prefix from the previous */

        n_diff[j]++;
      }

      if (n_not_null != NULL) {
        btr_record_not_null_field_in_rec(n_cols, offsets_next_rec, n_not_null);
      }

      total_external_size +=
          lob::btr_rec_get_externally_stored_len(rec, offsets_rec);

      rec = next_rec;
      /* Initialize offsets_rec for the next round
      and assign the old offsets_rec buffer to
      offsets_next_rec. */
      {
        ulint *offsets_tmp = offsets_rec;
        offsets_rec = offsets_next_rec;
        offsets_next_rec = offsets_tmp;
      }
    }

    if (n_cols == dict_index_get_n_unique_in_tree(index)) {
      /* If there is more than one leaf page in the tree,
      we add one because we know that the first record
      on the page certainly had a different prefix than the
      last record on the previous index page in the
      alphabetical order. Before this fix, if there was
      just one big record on each clustered index page, the
      algorithm grossly underestimated the number of rows
      in the table. */

      if (btr_page_get_prev(page, &mtr) != FIL_NULL ||
          btr_page_get_next(page, &mtr) != FIL_NULL) {
        n_diff[n_cols - 1]++;
      }
    }

    mtr_commit(&mtr);
  }

  /* If we saw k borders between different key values on
  n_sample_pages leaf pages, we can estimate how many
  there will be in index->stat_n_leaf_pages */

  /* We must take into account that our sample actually represents
  also the pages used for external storage of fields (those pages are
  included in index->stat_n_leaf_pages) */

  for (j = 0; j < n_cols; j++) {
    index->stat_n_diff_key_vals[j] = BTR_TABLE_STATS_FROM_SAMPLE(
        n_diff[j], index, n_sample_pages, total_external_size, not_empty_flag);

    /* If the tree is small, smaller than
    10 * n_sample_pages + total_external_size, then
    the above estimate is ok. For bigger trees it is common that we
    do not see any borders between key values in the few pages
    we pick. But still there may be n_sample_pages
    different key values, or even more. Let us try to approximate
    that: */

    add_on = index->stat_n_leaf_pages /
             (10 * (n_sample_pages + total_external_size));

    if (add_on > n_sample_pages) {
      add_on = n_sample_pages;
    }

    index->stat_n_diff_key_vals[j] += add_on;

    index->stat_n_sample_sizes[j] = n_sample_pages;

    /* Update the stat_n_non_null_key_vals[] with our
    sampled result. stat_n_non_null_key_vals[] is created
    and initialized to zero in dict_index_add_to_cache(),
    along with stat_n_diff_key_vals[] array */
    if (n_not_null != NULL) {
      index->stat_n_non_null_key_vals[j] =
          BTR_TABLE_STATS_FROM_SAMPLE(n_not_null[j], index, n_sample_pages,
                                      total_external_size, not_empty_flag);
    }
  }

  mem_heap_free(heap);

  return (true);
}

#endif /* !UNIV_HOTBACKUP */
