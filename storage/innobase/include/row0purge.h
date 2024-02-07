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

/** @file include/row0purge.h
 Purge obsolete records

 Created 3/14/1997 Heikki Tuuri
 *******************************************************/

#ifndef row0purge_h
#define row0purge_h

#include "univ.i"

#include "btr0pcur.h"
#include "btr0types.h"
#include "data0data.h"
#include "dict0types.h"
#include "que0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "ut0vec.h"

/** Create a purge node to a query graph.
@param[in]      parent  parent node, i.e., a thr node
@param[in]      heap    memory heap where created
@return own: purge node */
[[nodiscard]] purge_node_t *row_purge_node_create(que_thr_t *parent,
                                                  mem_heap_t *heap);

/** Determines if it is possible to remove a secondary index entry.
 Removal is possible if the secondary index entry does not refer to any
 not delete marked version of a clustered index record where DB_TRX_ID
 is newer than the purge view.

 NOTE: This function should only be called by the purge thread, only
 while holding a latch on the leaf page of the secondary index entry
 (or keeping the buffer pool watch on the page).  It is possible that
 this function first returns true and then false, if a user transaction
 inserts a record that the secondary index entry would refer to.
 However, in that case, the user transaction would also re-insert the
 secondary index entry after purge has removed it and released the leaf
 page latch.
 @return true if the secondary index record can be purged */
[[nodiscard]] bool row_purge_poss_sec(
    purge_node_t *node,     /*!< in/out: row purge node */
    dict_index_t *index,    /*!< in: secondary index */
    const dtuple_t *entry); /*!< in: secondary index entry */
/***************************************************************
Does the purge operation for a single undo log record. This is a high-level
function used in an SQL execution graph.
@return query thread to run next or NULL */
[[nodiscard]] que_thr_t *row_purge_step(
    que_thr_t *thr); /*!< in: query thread */

using Page_free_tuple = std::tuple<index_id_t, page_id_t, table_id_t>;

struct Compare_page_free_tuple {
  bool operator()(const Page_free_tuple &lhs,
                  const Page_free_tuple &rhs) const {
    const page_id_t &lpage_id = std::get<1>(lhs);
    const page_id_t &rpage_id = std::get<1>(rhs);
    return (lpage_id < rpage_id);
  }
};

/* Purge node structure */

struct purge_node_t {
  /** Info required to purge a record */
  struct rec_t {
    /** Record to purge */
    trx_undo_rec_t *undo_rec;

    /** File pointer to UNDO record */
    roll_ptr_t roll_ptr;

    /** Trx that created this undo record */
    trx_id_t modifier_trx_id;
  };

  using Recs = std::list<rec_t, mem_heap_allocator<rec_t>>;

  /** node type: QUE_NODE_PURGE */
  que_common_t common;

  /* Local storage for this graph node */

  /** roll pointer to undo log record */
  roll_ptr_t roll_ptr;

  /** undo number of the record */
  undo_no_t undo_no;

  /** undo log record type: TRX_UNDO_INSERT_REC, ... */
  ulint rec_type;

  /** table where purge is done */
  dict_table_t *table;

  /** MDL ticket for the table name */
  MDL_ticket *mdl;

  /** parent table for an FTS AUX TABLE */
  dict_table_t *parent;

  /** MDL ticket for the parent table of an FTS AUX TABLE */
  MDL_ticket *parent_mdl;

  /** MySQL table instance */
  TABLE *mysql_table;

  /** compiler analysis info of an update */
  ulint cmpl_info;

  /** update vector for a clustered index record */
  upd_t *update;

  /** NULL, or row reference to the next row to handle */
  dtuple_t *ref;

  /** NULL, or a copy (also fields copied to heap) of the indexed
  fields of the row to handle */
  dtuple_t *row;

  /** NULL, or the next index whose record should be handled */
  dict_index_t *index;

  /** The heap is owned by purge_sys and is reset after a purge
  batch has completed. */
  mem_heap_t *heap;

  /** true if the clustered index record determined by ref was
  found in the clustered index, and we were able to position pcur on it */
  bool found_clust;

  /** persistent cursor used in searching the clustered index record */
  btr_pcur_t pcur;

  /** Debug flag */
  bool done;

  /** trx id for this purging record */
  trx_id_t trx_id;

  /** trx id for this purging record */
  trx_id_t modifier_trx_id;

  /** Undo recs to purge */
  Recs *recs;

  void init() { new (&m_lob_pages) LOB_free_set(); }
  void deinit() {
    mem_heap_free(heap);
    m_lob_pages.~LOB_free_set();
  }

  /** Add an LOB page to the list of pages that will be freed at the end of a
  purge batch.
  @param[in]    index       the clust index to which the LOB belongs.
  @param[in]    page_id     the page_id of the first page of the LOB. */
  void add_lob_page(dict_index_t *index, const page_id_t &page_id);

  /** Free the LOB first pages at end of purge batch. Since this function
  acquires shared MDL table locks, the caller should not hold any latches. */
  void free_lob_pages();

  /** Check if undo records of given table_id is there in this purge node.
  @param[in]    table_id        look for undo records of this table id.
  @return true if undo records of table id exists, false otherwise. */
  bool is_table_id_exists(table_id_t table_id) const;

#ifdef UNIV_DEBUG
  /** Check if there are more than one undo record with same (trx_id, undo_no)
  combination.
  @return true when no duplicates are found, false otherwise. */
  bool check_duplicate_undo_no() const;
#endif /* UNIV_DEBUG */

  trx_rseg_t *rseg;
#ifdef UNIV_DEBUG
  /**   Validate the persistent cursor. The purge node has two references
     to the clustered index record - one via the ref member, and the
     other via the persistent cursor.  These two references must match
     each other if the found_clust flag is set.
     @return true if the persistent cursor is consistent with
     the ref member.*/
  bool validate_pcur();
#endif

 private:
  using LOB_free_set = std::set<Page_free_tuple, Compare_page_free_tuple,
                                ut::allocator<Page_free_tuple>>;

  /** Set of LOB first pages that are to be freed. */
  LOB_free_set m_lob_pages;
};

#endif
