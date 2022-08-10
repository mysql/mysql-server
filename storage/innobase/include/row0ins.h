/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.

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

/** @file include/row0ins.h
 Insert into a table

 Created 4/20/1996 Heikki Tuuri
 *******************************************************/

#ifndef row0ins_h
#define row0ins_h

#include "data0data.h"
#include "dict0types.h"
#include "que0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "univ.i"

/** Checks if foreign key constraint fails for an index entry. Sets shared locks
 which lock either the success or the failure of the constraint. NOTE that
 the caller must have a shared latch on dict_foreign_key_check_lock.
 @return DB_SUCCESS, DB_LOCK_WAIT, DB_NO_REFERENCED_ROW, or
 DB_ROW_IS_REFERENCED */
[[nodiscard]] dberr_t row_ins_check_foreign_constraint(
    bool check_ref,          /*!< in: true If we want to check that
                            the referenced table is ok, false if we
                            want to check the foreign key table */
    dict_foreign_t *foreign, /*!< in: foreign constraint; NOTE that the
                             tables mentioned in it must be in the
                             dictionary cache if they exist at all */
    dict_table_t *table,     /*!< in: if check_ref is true, then the foreign
                             table, else the referenced table */
    dtuple_t *entry,         /*!< in: index entry for index */
    que_thr_t *thr);         /*!< in: query thread */
/** Creates an insert node struct.
 @return own: insert node struct */
ins_node_t *ins_node_create(
    ulint ins_type,      /*!< in: INS_VALUES, ... */
    dict_table_t *table, /*!< in: table where to insert */
    mem_heap_t *heap);   /*!< in: mem heap where created */
/** Sets a new row to insert for an INS_DIRECT node. This function is only used
 if we have constructed the row separately, which is a rare case; this
 function is quite slow. */
void ins_node_set_new_row(
    ins_node_t *node, /*!< in: insert node */
    dtuple_t *row);   /*!< in: new row (or first row) for the node */
/** Tries to insert an entry into a clustered index, ignoring foreign key
constraints. If a record with the same unique key is found, the other
record is necessarily marked deleted by a committed transaction, or a
unique key violation error occurs. The delete marked record is then
updated to an existing record, and we must write an undo log record on
the delete marked record.
@param[in] flags Undo logging and locking flags.
@param[in] mode BTR_MODIFY_LEAF or BTR_MODIFY_TREE, depending on whether we wish
optimistic or pessimistic descent down the index tree.
@param[in] index Clustered index.
@param[in] n_uniq 0 or index->n_uniq.
@param[in] entry Index entry to insert.
@param[in] thr Query thread, or nullptr if flags & (BTR_NO_LOCKING_FLAG |
BTR_NO_UNDO_LOG_FLAG) and a duplicate can't occur.
@param[in] dup_chk_only If true, just do duplicate check and return. don't
execute actual insert.
@retval DB_SUCCESS on success
@retval DB_LOCK_WAIT on lock wait when !(flags & BTR_NO_LOCKING_FLAG)
@retval DB_FAIL if retry with BTR_MODIFY_TREE is needed
@return error code */
[[nodiscard]] dberr_t row_ins_clust_index_entry_low(
    uint32_t flags, ulint mode, dict_index_t *index, ulint n_uniq,
    dtuple_t *entry, que_thr_t *thr, bool dup_chk_only);

/** Tries to insert an entry into a secondary index. If a record with exactly
the same fields is found, the other record is necessarily marked deleted.
It is then unmarked. Otherwise, the entry is just inserted to the index.
@param[in]      flags           undo logging and locking flags
@param[in]      mode            BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
                                depending on whether we wish optimistic or
                                pessimistic descent down the index tree
@param[in]      index           secondary index
@param[in,out]  offsets_heap    memory heap that can be emptied
@param[in,out]  heap            memory heap
@param[in,out]  entry           index entry to insert
@param[in]      trx_id          PAGE_MAX_TRX_ID during row_log_table_apply(),
                                or trx_id when undo log is disabled during
                                alter copy operation or 0
@param[in]      thr             query thread
@param[in]      dup_chk_only    true, just do duplicate check and return.
                                don't execute actual insert
@retval DB_SUCCESS on success
@retval DB_LOCK_WAIT on lock wait when !(flags & BTR_NO_LOCKING_FLAG)
@retval DB_FAIL if retry with BTR_MODIFY_TREE is needed
@return error code */
[[nodiscard]] dberr_t row_ins_sec_index_entry_low(
    uint32_t flags, ulint mode, dict_index_t *index, mem_heap_t *offsets_heap,
    mem_heap_t *heap, dtuple_t *entry, trx_id_t trx_id, que_thr_t *thr,
    bool dup_chk_only);

/** Sets the values of the dtuple fields in entry from the values of appropriate
columns in row.
@param[in]      index   index handler
@param[out]     entry   index entry to make
@param[in]      row     row
@return DB_SUCCESS if the set is successful */
dberr_t row_ins_index_entry_set_vals(const dict_index_t *index, dtuple_t *entry,
                                     const dtuple_t *row);

/** Inserts an entry into a clustered index. Tries first optimistic,
 then pessimistic descent down the tree. If the entry matches enough
 to a delete marked record, performs the insert by updating or delete
 unmarking the delete marked record.
 @return DB_SUCCESS, DB_LOCK_WAIT, DB_DUPLICATE_KEY, or some other error code */
[[nodiscard]] dberr_t row_ins_clust_index_entry(
    dict_index_t *index, /*!< in: clustered index */
    dtuple_t *entry,     /*!< in/out: index entry to insert */
    que_thr_t *thr,      /*!< in: query thread */
    bool dup_chk_only);
/*!< in: if true, just do duplicate check
and return. don't execute actual insert. */
/** Inserts an entry into a secondary index. Tries first optimistic,
 then pessimistic descent down the tree. If the entry matches enough
 to a delete marked record, performs the insert by updating or delete
 unmarking the delete marked record.
 @return DB_SUCCESS, DB_LOCK_WAIT, DB_DUPLICATE_KEY, or some other error code */
[[nodiscard]] dberr_t row_ins_sec_index_entry(
    dict_index_t *index, /*!< in: secondary index */
    dtuple_t *entry,     /*!< in/out: index entry to insert */
    que_thr_t *thr,      /*!< in: query thread */
    bool dup_chk_only);
/*!< in: if true, just do duplicate check
and return. don't execute actual insert. */
/** Inserts a row to a table. This is a high-level function used in
 SQL execution graphs.
 @return query thread to run next or NULL */
que_thr_t *row_ins_step(que_thr_t *thr); /*!< in: query thread */

/* Insert node structure */

struct ins_node_t {
  que_common_t common;     /*!< node type: QUE_NODE_INSERT */
  ulint ins_type;          /* INS_VALUES, INS_SEARCHED, or INS_DIRECT */
  dtuple_t *row;           /*!< row to insert */
  dict_table_t *table;     /*!< table where to insert */
  sel_node_t *select;      /*!< select in searched insert */
  que_node_t *values_list; /* list of expressions to evaluate and
                       insert in an INS_VALUES insert */
  ulint state;             /*!< node execution state */
  dict_index_t *index;     /*!< NULL, or the next index where the index
                           entry should be inserted */
  dtuple_t *entry;         /*!< NULL, or entry to insert in the index;
                           after a successful insert of the entry,
                           this should be reset to NULL */
  UT_LIST_BASE_NODE_T(dtuple_t, tuple_list)
  entry_list;       /* list of entries, one for each index */
  byte *row_id_buf; /* buffer for the row id sys field in row */
  trx_id_t trx_id;  /*!< trx id or the last trx which executed the
                    node */
  byte *trx_id_buf; /* buffer for the trx id sys field in row */
  mem_heap_t *entry_sys_heap;
  /* memory heap used as auxiliary storage;
  entry_list and sys fields are stored here;
  if this is NULL, entry list should be created
  and buffers for sys fields in row allocated */

  /** When there is a lock wait error, this remembers current position of
  the multi-value field, before which the values have been inserted */
  uint32_t ins_multi_val_pos;

  ulint magic_n;
};

constexpr uint32_t INS_NODE_MAGIC_N = 15849075;

/* Insert node types */
/** INSERT INTO ... SELECT ... */
constexpr uint32_t INS_SEARCHED = 0;
/** INSERT INTO ... VALUES ... */
constexpr uint32_t INS_VALUES = 1;
/** this is for internal use in dict0crea: insert the row directly */
constexpr uint32_t INS_DIRECT = 2;

/* Node execution states */
/** we should set an IX lock on table */
constexpr uint32_t INS_NODE_SET_IX_LOCK = 1;
/** row id should be allocated */
constexpr uint32_t INS_NODE_ALLOC_ROW_ID = 2;
/** index entries should be built and inserted */
constexpr uint32_t INS_NODE_INSERT_ENTRIES = 3;

#endif
