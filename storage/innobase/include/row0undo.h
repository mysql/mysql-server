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

/** @file include/row0undo.h
 Row undo

 Created 1/8/1997 Heikki Tuuri
 *******************************************************/

#ifndef row0undo_h
#define row0undo_h

#include "btr0pcur.h"
#include "btr0types.h"
#include "dict0types.h"
#include "mtr0mtr.h"
#include "que0types.h"
#include "row0types.h"
#include "trx0sys.h"
#include "trx0types.h"
#include "univ.i"
#include "ut0ut.h"

/** Converts an implicit lock on the record to explicit in case of partial
 rollback.
@param[in]      cursor          cursor to record
@param[in]      node            undo node */
void row_convert_impl_to_expl_if_needed(btr_cur_t *cursor, undo_node_t *node);

/** Creates a row undo node to a query graph.
@param[in]      trx              transaction
@param[in]      parent           parent node, i.e., a thr node
@param[in]      heap             memory heap where created
@param[in]      partial_rollback true if partial rollback
@return         undo node */
undo_node_t *row_undo_node_create(trx_t &trx, que_thr_t *parent,
                                  mem_heap_t *heap, bool partial_rollback);

/** Looks for the clustered index record when node has the row reference.
 The pcur in node is used in the search. If found, stores the row to node,
 and stores the position of pcur, and detaches it. The pcur must be closed
 by the caller in any case.
 @return true if found; NOTE the node->pcur must be closed by the
 caller, regardless of the return value */
[[nodiscard]] bool row_undo_search_clust_to_pcur(
    undo_node_t *node); /*!< in/out: row undo node */
/** Undoes a row operation in a table. This is a high-level function used
 in SQL execution graphs.
 @return query thread to run next or NULL */
que_thr_t *row_undo_step(que_thr_t *thr); /*!< in: query thread */

/* A single query thread will try to perform the undo for all successive
versions of a clustered index record, if the transaction has modified it
several times during the execution which is rolled back. It may happen
that the task is transferred to another query thread, if the other thread
is assigned to handle an undo log record in the chain of different versions
of the record, and the other thread happens to get the x-latch to the
clustered index record at the right time.
        If a query thread notices that the clustered index record it is looking
for is missing, or the roll ptr field in the record does not point to the
undo log record the thread was assigned to handle, then it gives up the undo
task for that undo log record, and fetches the next. This situation can occur
just in the case where the transaction modified the same record several times
and another thread is currently doing the undo for successive versions of
that index record. */

/** Execution state of an undo node */
enum undo_exec {
  UNDO_NODE_FETCH_NEXT = 1, /*!< we should fetch the next
                            undo log record */
  UNDO_NODE_INSERT,         /*!< undo a fresh insert of a
                            row to a table */
  UNDO_NODE_MODIFY          /*!< undo a modify operation
                            (DELETE or UPDATE) on a row
                            of a table */
};

/** State of long running undo operation for purpose of writing
periodic messages to error log */
struct Long_undo_state {
  explicit Long_undo_state(const trx_t &);
  /** timer for printing out log messages for long-running rollbacks */
  ib::Throttler throttler;

  /** flag which is set when at least one message is logged */
  bool have_logged{false};

  /** total number of rows to roll back */
  undo_no_t rows_total;

  /** transaction state before rollback */
  trx_state_t trx_state;
};

/** Undo node structure */
struct undo_node_t {
  undo_node_t(trx_t &, que_thr_t *parent, bool partial_rollback);
  /** node type: QUE_NODE_UNDO */
  que_common_t common;

  /** node execution state */
  enum undo_exec state;

  /** trx for which undo is done */
  trx_t &trx;

  /** roll pointer to undo log record */
  roll_ptr_t roll_ptr;

  /** undo log record */
  trx_undo_rec_t *undo_rec;

  /** undo number of the record */
  undo_no_t undo_no;

  /** undo log record type: TRX_UNDO_INSERT_REC, ... */
  ulint rec_type;

  /** trx id to restore to clustered index record */
  trx_id_t new_trx_id;

  /**persistent cursor used in searching the clustered index record */
  btr_pcur_t pcur;

  /** table where undo is done */
  dict_table_t *table;

  /** compiler analysis of an update */
  ulint cmpl_info;

  /** update vector for a clustered index record */
  upd_t *update;

  /** row reference to the next row to handle */
  dtuple_t *ref;

  /** a copy (also fields copied to heap) of the row to handle */
  dtuple_t *row;

  /** NULL, or prefixes of the externally stored columns of the row */
  row_ext_t *ext;

  /** NULL, or the row after undo */
  dtuple_t *undo_row;

  /** NULL, or prefixes of the externally stored columns of undo_row */
  row_ext_t *undo_ext;

  /** the next index whose record should be handled */
  dict_index_t *index;

  /** memory heap used as auxiliary storage for row;
      this must be emptied after undo is tried on a row */
  mem_heap_t *heap;

  /** true if partial rollback */
  bool partial;

  /** state of long-running undo */
  Long_undo_state long_undo_state;
};

#endif
