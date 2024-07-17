/*****************************************************************************

Copyright (c) 1997, 2024, Oracle and/or its affiliates.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/** @file row/row0sel.cc
 Select

 Created 12/19/1997 Heikki Tuuri
 *******************************************************/

#include "row0sel.h"

#include <sys/types.h>

#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0sea.h"
#include "buf0lru.h"
#include "dict0boot.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "eval0eval.h"
#include "gis0rtree.h"
#include "ha_innodb.h"
#include "ha_prototypes.h"
#include "handler.h"
#include "lob0lob.h"
#include "lob0undo.h"
#include "lock0lock.h"
#include "mach0data.h"
#include "pars0pars.h"
#include "pars0sym.h"
#include "que0que.h"
#include "read0read.h"
#include "record_buffer.h"
#include "rem0cmp.h"
#include "row0mysql.h"
#include "row0row.h"
#include "row0upd.h"
#include "row0vers.h"
#include "srv0mon.h"
#include "trx0trx.h"
#include "trx0undo.h"
#include "ut0new.h"

#include "my_dbug.h"

/** Maximum number of rows to prefetch; MySQL interface has another parameter */
constexpr uint32_t SEL_MAX_N_PREFETCH = 16;

/** Number of rows fetched, after which to start prefetching; MySQL interface
has another parameter */
constexpr uint32_t SEL_PREFETCH_LIMIT = 1;

/** When a select has accessed about this many pages, it returns control back
to que_run_threads: this is to allow canceling runaway queries */

constexpr uint32_t SEL_COST_LIMIT = 100;

/** Flags for search shortcut */
constexpr uint32_t SEL_FOUND = 0;
constexpr uint32_t SEL_EXHAUSTED = 1;
constexpr uint32_t SEL_RETRY = 2;

/** Returns true if the user-defined column in a secondary index record
 is alphabetically the same as the corresponding BLOB column in the clustered
 index record.
 NOTE: the comparison is NOT done as a binary comparison, but character
 fields are compared with collation!
 @return true if the columns are equal */
static bool row_sel_sec_rec_is_for_blob(
    trx_t *trx,              /*!< in: the operating transaction */
    ulint mtype,             /*!< in: main type */
    ulint prtype,            /*!< in: precise type */
    ulint mbminmaxlen,       /*!< in: minimum and maximum length of
                             a multi-byte character */
    const byte *clust_field, /*!< in: the locally stored part of
                             the clustered index column, including
                             the BLOB pointer; the clustered
                             index record must be covered by
                             a lock or a page latch to protect it
                             against deletion (rollback or purge) */
    ulint clust_len,         /*!< in: length of clust_field */
    const byte *sec_field,   /*!< in: column in secondary index */
    ulint sec_len,           /*!< in: length of sec_field */
    ulint prefix_len,        /*!< in: index column prefix length
                             in bytes */
    dict_table_t *table)     /*!< in: table */
{
  ulint len;
  byte buf[REC_VERSION_56_MAX_INDEX_COL_LEN];

  /* This function should never be invoked on tables in
  ROW_FORMAT=REDUNDANT or ROW_FORMAT=COMPACT, because they
  should always contain enough prefix in the clustered index record. */
  ut_ad(dict_table_has_atomic_blobs(table));
  ut_a(clust_len >= BTR_EXTERN_FIELD_REF_SIZE);
  ut_ad(prefix_len >= sec_len);
  ut_ad(prefix_len > 0);
  ut_a(prefix_len <= sizeof buf);

  if (!memcmp(clust_field + clust_len - BTR_EXTERN_FIELD_REF_SIZE,
              field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE)) {
    /* The externally stored field was not written yet.
    This record should only be seen by
    trx_rollback_or_clean_all_recovered() or any
    TRX_ISO_READ_UNCOMMITTED transactions. */
    return false;
  }

  len = lob::btr_copy_externally_stored_field_prefix_func(
      trx, table->first_index(), buf, prefix_len,
      dict_tf_get_page_size(table->flags), clust_field,
      IF_DEBUG(dict_table_is_sdi(table->id), ) clust_len);

  if (len == 0) {
    /* The BLOB was being deleted as the server crashed.
    There should not be any secondary index records
    referring to this clustered index record, because
    btr_free_externally_stored_field() is called after all
    secondary index entries of the row have been purged. */
    return false;
  }

  len = dtype_get_at_most_n_mbchars(prtype, mbminmaxlen, prefix_len, len,
                                    (const char *)buf);

  /* We are testing for equality; ASC/DESC does not matter. */
  return (!cmp_data_data(mtype, prtype, true, buf, len, sec_field, sec_len));
}

/** Returns true if the user-defined column values in a secondary index record
are alphabetically the same as the corresponding columns in the clustered
index record.
NOTE: the comparison is NOT done as a binary comparison, but character
fields are compared with collation!
@param[in]      sec_rec         secondary index record
@param[in]      sec_index       secondary index
@param[in]      clust_rec       clustered index record;
                                must be protected by a page s-latch
@param[in]      clust_index     clustered index
@param[in]      thr             query thread
@param[out]     is_equal        set to true if the secondary record is equal
to the corresponding fields in the clustered record, when compared with
                                collation; false if not equal or if the
clustered record has been marked for deletion; only valid if DB_SUCCESS was
returned
@return DB_SUCCESS or error code */
static dberr_t row_sel_sec_rec_is_for_clust_rec(
    const rec_t *sec_rec, dict_index_t *sec_index, const rec_t *clust_rec,
    dict_index_t *clust_index, que_thr_t *thr, bool &is_equal) {
  const byte *sec_field;
  ulint sec_len;
  const byte *clust_field;
  ulint n;
  ulint i;
  mem_heap_t *heap = nullptr;
  ulint clust_offsets_[REC_OFFS_NORMAL_SIZE];
  ulint sec_offsets_[REC_OFFS_SMALL_SIZE];
  ulint *clust_offs = clust_offsets_;
  ulint *sec_offs = sec_offsets_;
  trx_t *trx = thr_get_trx(thr);
  dberr_t err = DB_SUCCESS;

  is_equal = true;

  rec_offs_init(clust_offsets_);
  rec_offs_init(sec_offsets_);

  if (rec_get_deleted_flag(clust_rec, dict_table_is_comp(clust_index->table))) {
    /* The clustered index record is delete-marked;
    it is not visible in the read view.  Besides,
    if there are any externally stored columns,
    some of them may have already been purged. */
    is_equal = false;
    return (DB_SUCCESS);
  }

  heap = mem_heap_create(256, UT_LOCATION_HERE);

  clust_offs = rec_get_offsets(clust_rec, clust_index, clust_offs,
                               ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);
  sec_offs = rec_get_offsets(sec_rec, sec_index, sec_offs, ULINT_UNDEFINED,
                             UT_LOCATION_HERE, &heap);

  n = dict_index_get_n_ordering_defined_by_user(sec_index);

  for (i = 0; i < n; i++) {
    const dict_field_t *ifield;
    const dict_col_t *col;
    ulint clust_pos = 0;
    ulint clust_len;
    ulint len;
    row_ext_t *ext;

    ifield = sec_index->get_field(i);
    col = ifield->col;

    /* For virtual column, its value will need to be
    reconstructed from base column in cluster index */
    if (col->is_virtual()) {
      const dict_v_col_t *v_col;
      const dtuple_t *row;
      dfield_t *vfield;

      v_col = reinterpret_cast<const dict_v_col_t *>(col);

      row = row_build(ROW_COPY_POINTERS, clust_index, clust_rec, clust_offs,
                      nullptr, nullptr, nullptr, &ext, heap);

      vfield = innobase_get_computed_value(row, v_col, clust_index, &heap, heap,
                                           nullptr, thr_get_trx(thr)->mysql_thd,
                                           thr->prebuilt->m_mysql_table,
                                           nullptr, nullptr, nullptr);

      if (vfield == nullptr) {
        /* This may happen e.g. when this statement is executed in
        read-uncommited isolation and value (like json function)
        depends on an externally stored lob (like json) which
        was not written yet. */
        err = DB_COMPUTE_VALUE_FAILED;
        goto func_exit;
      }

      clust_len = vfield->len;
      clust_field = static_cast<byte *>(vfield->data);

    } else {
      clust_pos = dict_col_get_clust_pos(col, clust_index);

      clust_field = rec_get_nth_field_instant(clust_rec, clust_offs, clust_pos,
                                              clust_index, &clust_len);
    }

    sec_field = rec_get_nth_field(nullptr, sec_rec, sec_offs, i, &sec_len);

    len = clust_len;

    if (ifield->prefix_len > 0 && len != UNIV_SQL_NULL &&
        sec_len != UNIV_SQL_NULL && !col->is_virtual()) {
      if (rec_offs_nth_extern(clust_index, clust_offs, clust_pos)) {
        len -= BTR_EXTERN_FIELD_REF_SIZE;
      }

      len = dtype_get_at_most_n_mbchars(col->prtype, col->mbminmaxlen,
                                        ifield->prefix_len, len,
                                        (char *)clust_field);

      /* Check sec index field matches that of cluster index
      in the case of for table with ATOMIC BLOB, note
      we also need to check if sec_len is 0 */
      if (rec_offs_nth_extern(clust_index, clust_offs, clust_pos) &&
          (len < sec_len ||
           (dict_table_has_atomic_blobs(sec_index->table) && sec_len == 0))) {
        if (!row_sel_sec_rec_is_for_blob(
                trx, col->mtype, col->prtype, col->mbminmaxlen, clust_field,
                clust_len, sec_field, sec_len, ifield->prefix_len,
                clust_index->table)) {
          is_equal = false;
          goto func_exit;
        }

        continue;
      }
    }

    /* For spatial index, the first field is MBR, we check
    if the MBR is equal or not. */
    if (dict_index_is_spatial(sec_index) && i == 0) {
      rtr_mbr_t tmp_mbr;
      rtr_mbr_t sec_mbr;
      byte *dptr = const_cast<byte *>(clust_field);

      ut_ad(clust_len != UNIV_SQL_NULL);

      /* For externally stored field, we need to get full
      geo data to generate the MBR for comparing. */
      if (rec_offs_nth_extern(clust_index, clust_offs, clust_pos)) {
        dptr = lob::btr_copy_externally_stored_field(
            trx, clust_index, &clust_len, nullptr, dptr,
            dict_tf_get_page_size(sec_index->table->flags), len,
            dict_index_is_sdi(sec_index), heap);
      }

      get_mbr_from_store(sec_index->rtr_srs.get(), dptr,
                         static_cast<uint>(clust_len), SPDIMS,
                         reinterpret_cast<double *>(&tmp_mbr), nullptr);
      rtr_read_mbr(sec_field, &sec_mbr);

      if (!mbr_equal_cmp(sec_index->rtr_srs.get(), &sec_mbr, &tmp_mbr)) {
        is_equal = false;
        goto func_exit;
      }
    } else if (col->is_multi_value()) {
      if (!is_multi_value_clust_and_sec_equal(clust_field, clust_len, sec_field,
                                              sec_len, col)) {
        is_equal = false;
        goto func_exit;
      }
    } else {
      /* We are testing for equality; ASC/DESC does not
      matter */
      if (0 != cmp_data_data(col->mtype, col->prtype, true, clust_field, len,
                             sec_field, sec_len)) {
        is_equal = false;
        goto func_exit;
      }
    }
  }

func_exit:
  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }
  return (err);
}

/** Creates a select node struct.
 @return own: select node struct */
sel_node_t *sel_node_create(
    mem_heap_t *heap) /*!< in: memory heap where created */
{
  sel_node_t *node;

  node = static_cast<sel_node_t *>(mem_heap_alloc(heap, sizeof(sel_node_t)));

  node->common.type = QUE_NODE_SELECT;
  node->state = SEL_NODE_OPEN;

  node->plans = nullptr;

  return (node);
}

/** Frees the memory private to a select node when a query graph is freed,
 does not free the heap where the node was originally created. */
void sel_node_free_private(sel_node_t *node) /*!< in: select node struct */
{
  ulint i;
  plan_t *plan;

  if (node->plans != nullptr) {
    for (i = 0; i < node->n_tables; i++) {
      plan = sel_node_get_nth_plan(node, i);

      plan->pcur.close();
      plan->clust_pcur.close();

      if (plan->old_vers_heap) {
        mem_heap_free(plan->old_vers_heap);
      }
    }
  }
}

/** Evaluates the values in a select list. If there are aggregate functions,
 their argument value is added to the aggregate total. */
static inline void sel_eval_select_list(
    sel_node_t *node) /*!< in: select node */
{
  que_node_t *exp;

  exp = node->select_list;

  while (exp) {
    eval_exp(exp);

    exp = que_node_get_next(exp);
  }
}

/** Assigns the values in the select list to the possible into-variables in
 SELECT ... INTO ... */
static inline void sel_assign_into_var_values(
    sym_node_t *var,  /*!< in: first variable in a
                      list of  variables */
    sel_node_t *node) /*!< in: select node */
{
  que_node_t *exp;

  if (var == nullptr) {
    return;
  }

  for (exp = node->select_list; var != nullptr;
       var = static_cast<sym_node_t *>(que_node_get_next(var))) {
    ut_ad(exp);

    eval_node_copy_val(var->alias, exp);

    exp = que_node_get_next(exp);
  }
}

/** Resets the aggregate value totals in the select list of an aggregate type
 query. */
static inline void sel_reset_aggregate_vals(
    sel_node_t *node) /*!< in: select node */
{
  func_node_t *func_node;

  ut_ad(node->is_aggregate);

  for (func_node = static_cast<func_node_t *>(node->select_list);
       func_node != nullptr;
       func_node = static_cast<func_node_t *>(que_node_get_next(func_node))) {
    eval_node_set_int_val(func_node, 0);
  }

  node->aggregate_already_fetched = false;
}

/** Copies the input variable values when an explicit cursor is opened. */
static inline void row_sel_copy_input_variable_vals(
    sel_node_t *node) /*!< in: select node */
{
  for (auto var : node->copy_variables) {
    eval_node_copy_val(var, var->alias);

    var->indirection = nullptr;
  }
}

/** Fetches the column values from a record.
@param[in]   trx             the current transaction or nullptr
@param[in]   index           record index
@param[in]   rec             record in a clustered or non-clustered index;
                             must be protected by a page latch
@param[in]   offsets         rec_get_offsets(rec, index)
@param[in]   column          first column in a column list, or NULL
@param[in]   allow_null_lob  allow null lob if true. default is false. */
static void row_sel_fetch_columns(trx_t *trx, dict_index_t *index,
                                  const rec_t *rec, const ulint *offsets,
                                  sym_node_t *column,
                                  bool allow_null_lob = false) {
  dfield_t *val;
  ulint index_type;
  ulint field_no;
  const byte *data;
  ulint len;

  ut_ad(rec_offs_validate(rec, index, offsets));

  if (index->is_clustered()) {
    index_type = SYM_CLUST_FIELD_NO;
  } else {
    index_type = SYM_SEC_FIELD_NO;
  }

  while (column) {
    mem_heap_t *heap = nullptr;
    bool needs_copy;

    field_no = column->field_nos[index_type];

    if (field_no != ULINT_UNDEFINED) {
      if (UNIV_UNLIKELY(rec_offs_nth_extern(index, offsets, field_no))) {
        /* Copy an externally stored field to the
        temporary heap, if possible. */

        heap = mem_heap_create(1, UT_LOCATION_HERE);

        data = lob::btr_rec_copy_externally_stored_field(
            trx, index, rec, offsets, dict_table_page_size(index->table),
            field_no, &len, nullptr, dict_index_is_sdi(index), heap);

        if (data == nullptr) {
          /* This means that the externally stored field was not written yet.
          This record should only be seen by following situations:
          - Read uncommitted transactions (TRX_ISO_READ_UNCOMMITTED)
          - During crash recovery [trx_rollback_or_clean_all_recovered().]
          - During lock-less consistent read, when the trx reads LOB even
             though the clust_rec is not to be seen. */
          ut_ad(allow_null_lob);
          len = UNIV_SQL_NULL;
          needs_copy = false;
        } else {
          needs_copy = true;
        }
      } else {
        data = rec_get_nth_field_instant(rec, offsets, field_no, index, &len);

        needs_copy = column->copy_val;
      }

      if (needs_copy) {
        eval_node_copy_and_alloc_val(column, data, len);
      } else {
        val = que_node_get_val(column);
        dfield_set_data(val, data, len);
      }

      if (UNIV_LIKELY_NULL(heap)) {
        mem_heap_free(heap);
      }
    }

    column = UT_LIST_GET_NEXT(col_var_list, column);
  }
}

/** Allocates a prefetch buffer for a column when prefetch is first time done.
 */
static void sel_col_prefetch_buf_alloc(
    sym_node_t *column) /*!< in: symbol table node for a column */
{
  sel_buf_t *sel_buf;
  ulint i;

  ut_ad(que_node_get_type(column) == QUE_NODE_SYMBOL);

  column->prefetch_buf = static_cast<sel_buf_t *>(ut::malloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, SEL_MAX_N_PREFETCH * sizeof(sel_buf_t)));

  for (i = 0; i < SEL_MAX_N_PREFETCH; i++) {
    sel_buf = column->prefetch_buf + i;

    sel_buf->data = nullptr;
    sel_buf->len = 0;
    sel_buf->val_buf_size = 0;
  }
}

/** Frees a prefetch buffer for a column, including the dynamically allocated
 memory for data stored there. */
void sel_col_prefetch_buf_free(
    sel_buf_t *prefetch_buf) /*!< in, own: prefetch buffer */
{
  sel_buf_t *sel_buf;
  ulint i;

  for (i = 0; i < SEL_MAX_N_PREFETCH; i++) {
    sel_buf = prefetch_buf + i;

    if (sel_buf->val_buf_size > 0) {
      ut::free(sel_buf->data);
    }
  }

  ut::free(prefetch_buf);
}

/** Pops the column values for a prefetched, cached row from the column prefetch
 buffers and places them to the val fields in the column nodes. */
static void sel_dequeue_prefetched_row(
    plan_t *plan) /*!< in: plan node for a table */
{
  sel_buf_t *sel_buf;
  dfield_t *val;
  byte *data;
  ulint len;
  ulint val_buf_size;

  ut_ad(plan->n_rows_prefetched > 0);

  for (auto column : plan->columns) {
    val = que_node_get_val(column);

    if (!column->copy_val) {
      /* We did not really push any value for the
      column */

      ut_ad(!column->prefetch_buf);
      ut_ad(que_node_get_val_buf_size(column) == 0);
      ut_d(dfield_set_null(val));

      continue;
    }

    ut_ad(column->prefetch_buf);
    ut_ad(!dfield_is_ext(val));

    sel_buf = column->prefetch_buf + plan->first_prefetched;

    data = sel_buf->data;
    len = sel_buf->len;
    val_buf_size = sel_buf->val_buf_size;

    /* We must keep track of the allocated memory for
    column values to be able to free it later: therefore
    we swap the values for sel_buf and val */

    sel_buf->data = static_cast<byte *>(dfield_get_data(val));
    sel_buf->len = dfield_get_len(val);
    sel_buf->val_buf_size = que_node_get_val_buf_size(column);

    dfield_set_data(val, data, len);
    que_node_set_val_buf_size(column, val_buf_size);
  }

  plan->n_rows_prefetched--;

  plan->first_prefetched++;
}

/** Pushes the column values for a prefetched, cached row to the column prefetch
 buffers from the val fields in the column nodes. */
static inline void sel_enqueue_prefetched_row(
    plan_t *plan) /*!< in: plan node for a table */
{
  sel_buf_t *sel_buf;
  dfield_t *val;
  byte *data;
  ulint len;
  ulint pos;
  ulint val_buf_size;

  if (plan->n_rows_prefetched == 0) {
    pos = 0;
    plan->first_prefetched = 0;
  } else {
    pos = plan->n_rows_prefetched;

    /* We have the convention that pushing new rows starts only
    after the prefetch stack has been emptied: */

    ut_ad(plan->first_prefetched == 0);
  }

  plan->n_rows_prefetched++;

  ut_ad(pos < SEL_MAX_N_PREFETCH);

  for (auto column : plan->columns) {
    if (!column->copy_val) {
      /* There is no sense to push pointers to database
      page fields when we do not keep latch on the page! */
      continue;
    }

    if (!column->prefetch_buf) {
      /* Allocate a new prefetch buffer */

      sel_col_prefetch_buf_alloc(column);
    }

    sel_buf = column->prefetch_buf + pos;

    val = que_node_get_val(column);

    data = static_cast<byte *>(dfield_get_data(val));
    len = dfield_get_len(val);
    val_buf_size = que_node_get_val_buf_size(column);

    /* We must keep track of the allocated memory for
    column values to be able to free it later: therefore
    we swap the values for sel_buf and val */

    dfield_set_data(val, sel_buf->data, sel_buf->len);
    que_node_set_val_buf_size(column, sel_buf->val_buf_size);

    sel_buf->data = data;
    sel_buf->len = len;
    sel_buf->val_buf_size = val_buf_size;
  }
}

/** Builds a previous version of a clustered index record for a consistent read
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_sel_build_prev_vers(
    ReadView *read_view,        /*!< in: read view */
    dict_index_t *index,        /*!< in: plan node for table */
    rec_t *rec,                 /*!< in: record in a clustered index */
    ulint **offsets,            /*!< in/out: offsets returned by
                                rec_get_offsets(rec, plan->index) */
    mem_heap_t **offset_heap,   /*!< in/out: memory heap from which
                                the offsets are allocated */
    mem_heap_t **old_vers_heap, /*!< out: old version heap to use */
    rec_t **old_vers,           /*!< out: old version, or NULL if the
                                record does not exist in the view:
                                i.e., it was freshly inserted
                                afterwards */
    mtr_t *mtr)                 /*!< in: mtr */
{
  dberr_t err;

  if (*old_vers_heap) {
    mem_heap_empty(*old_vers_heap);
  } else {
    *old_vers_heap = mem_heap_create(512, UT_LOCATION_HERE);
  }

  err = row_vers_build_for_consistent_read(rec, mtr, index, offsets, read_view,
                                           offset_heap, *old_vers_heap,
                                           old_vers, nullptr, nullptr);
  return (err);
}

/** Builds the last committed version of a clustered index record for a
 semi-consistent read. */
static void row_sel_build_committed_vers_for_mysql(
    dict_index_t *clust_index, /*!< in: clustered index */
    row_prebuilt_t *prebuilt,  /*!< in: prebuilt struct */
    const rec_t *rec,          /*!< in: record in a clustered index */
    ulint **offsets,           /*!< in/out: offsets returned by
                               rec_get_offsets(rec, clust_index) */
    mem_heap_t **offset_heap,  /*!< in/out: memory heap from which
                               the offsets are allocated */
    const rec_t **old_vers,    /*!< out: old version, or NULL if the
                               record does not exist in the view:
                               i.e., it was freshly inserted
                               afterwards */
    const dtuple_t **vrow,     /*!< out: to be filled with old virtual
                               column version if any */
    mtr_t *mtr)                /*!< in: mtr */
{
  if (prebuilt->old_vers_heap) {
    mem_heap_empty(prebuilt->old_vers_heap);
  } else {
    prebuilt->old_vers_heap =
        mem_heap_create(rec_offs_size(*offsets), UT_LOCATION_HERE);
  }

  row_vers_build_for_semi_consistent_read(rec, mtr, clust_index, offsets,
                                          offset_heap, prebuilt->old_vers_heap,
                                          old_vers, vrow);
}

/** Tests the conditions which determine when the index segment we are searching
 through has been exhausted.
 @return true if row passed the tests */
static inline bool row_sel_test_end_conds(
    plan_t *plan) /*!< in: plan for the table; the column values must
                  already have been retrieved and the right sides of
                  comparisons evaluated */
{
  /* All conditions in end_conds are comparisons of a column to an
  expression */

  for (auto cond : plan->end_conds) {
    /* Evaluate the left side of the comparison, i.e., get the
    column value if there is an indirection */

    eval_sym(static_cast<sym_node_t *>(cond->args));

    /* Do the comparison */

    if (!eval_cmp(cond)) {
      return false;
    }
  }

  return true;
}

/** Tests the other conditions.
 @return true if row passed the tests */
static inline bool row_sel_test_other_conds(
    plan_t *plan) /*!< in: plan for the table; the column values must
                  already have been retrieved */
{
  for (auto cond : plan->other_conds) {
    eval_exp(cond);

    if (!eval_node_get_bool_val(cond)) {
      return false;
    }
  }

  return true;
}

/** Retrieves the clustered index record corresponding to a record in a
 non-clustered index. Does the necessary locking.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_sel_get_clust_rec(
    sel_node_t *node, /*!< in: select_node */
    plan_t *plan,     /*!< in: plan node for table */
    rec_t *rec,       /*!< in: record in a non-clustered index */
    que_thr_t *thr,   /*!< in: query thread */
    rec_t **out_rec,  /*!< out: clustered record or an old version of
                      it, NULL if the old version did not exist
                      in the read view, i.e., it was a fresh
                      inserted version */
    mtr_t *mtr)       /*!< in: mtr used to get access to the
                      non-clustered record; the same mtr is used to
                      access the clustered index */
{
  dict_index_t *index;
  rec_t *clust_rec;
  rec_t *old_vers;
  dberr_t err;
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  *out_rec = nullptr;

  offsets = rec_get_offsets(rec, plan->pcur.get_btr_cur()->index, offsets,
                            ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  row_build_row_ref_fast(plan->clust_ref, plan->clust_map, rec, offsets);

  index = plan->table->first_index();

  plan->clust_pcur.open_no_init(index, plan->clust_ref, PAGE_CUR_LE,
                                BTR_SEARCH_LEAF, 0, mtr, UT_LOCATION_HERE);

  clust_rec = plan->clust_pcur.get_rec();

  /* Note: only if the search ends up on a non-infimum record is the
  low_match value the real match to the search tuple */

  if (!page_rec_is_user_rec(clust_rec) ||
      plan->clust_pcur.get_low_match() < dict_index_get_n_unique(index)) {
    ut_a(rec_get_deleted_flag(rec, dict_table_is_comp(plan->table)));
    ut_a(node->read_view);

    /* In a rare case it is possible that no clust rec is found
    for a delete-marked secondary index record: if in row0umod.cc
    in row_undo_mod_remove_clust_low() we have already removed
    the clust rec, while purge is still cleaning and removing
    secondary index records associated with earlier versions of
    the clustered index record. In that case we know that the
    clustered index record did not exist in the read view of
    trx. */

    goto func_exit;
  }

  offsets = rec_get_offsets(clust_rec, index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &heap);

  if (!node->read_view) {
    /* Try to place a lock on the index record */

    ulint lock_type;
    trx_t *trx;

    trx = thr_get_trx(thr);

    lock_type = trx->skip_gap_locks() ? LOCK_REC_NOT_GAP : LOCK_ORDINARY;

    err = lock_clust_rec_read_check_and_lock(
        lock_duration_t::REGULAR, plan->clust_pcur.get_block(), clust_rec,
        index, offsets, SELECT_ORDINARY,
        static_cast<lock_mode>(node->row_lock_mode), lock_type, thr);

    switch (err) {
      case DB_SUCCESS:
      case DB_SUCCESS_LOCKED_REC:
        /* Declare the variable uninitialized in Valgrind.
        It should be set to DB_SUCCESS at func_exit. */
        UNIV_MEM_INVALID(&err, sizeof err);
        break;
      default:
        goto err_exit;
    }
  } else {
    /* This is a non-locking consistent read: if necessary, fetch
    a previous version of the record */

    old_vers = nullptr;

    if (!lock_clust_rec_cons_read_sees(clust_rec, index, offsets,
                                       node->read_view)) {
      err =
          row_sel_build_prev_vers(node->read_view, index, clust_rec, &offsets,
                                  &heap, &plan->old_vers_heap, &old_vers, mtr);

      if (err != DB_SUCCESS) {
        goto err_exit;
      }

      clust_rec = old_vers;

      if (clust_rec == nullptr) {
        goto func_exit;
      }
    }

    /* If we had to go to an earlier version of row or the
    secondary index record is delete marked, then it may be that
    the secondary index record corresponding to clust_rec
    (or old_vers) is not rec; in that case we must ignore
    such row because in our snapshot rec would not have existed.
    Remember that from rec we cannot see directly which transaction
    id corresponds to it: we have to go to the clustered index
    record. A query where we want to fetch all rows where
    the secondary index value is in some interval would return
    a wrong result if we would not drop rows which we come to
    visit through secondary index records that would not really
    exist in our snapshot. */

    if (old_vers ||
        rec_get_deleted_flag(rec, dict_table_is_comp(plan->table))) {
      bool rec_equal;

      err = row_sel_sec_rec_is_for_clust_rec(rec, plan->index, clust_rec, index,
                                             thr, rec_equal);
      if (err != DB_SUCCESS) {
        goto err_exit;
      } else if (!rec_equal) {
        goto func_exit;
      }
    }
  }

  /* Fetch the columns needed in test conditions.  The clustered
  index record is protected by a page latch that was acquired
  when plan->clust_pcur was positioned.  The latch will not be
  released until mtr_commit(mtr). */

  ut_ad(!rec_get_deleted_flag(clust_rec, rec_offs_comp(offsets)));
  row_sel_fetch_columns(thr_get_trx(thr), index, clust_rec, offsets,
                        UT_LIST_GET_FIRST(plan->columns));
  *out_rec = clust_rec;
func_exit:
  err = DB_SUCCESS;
err_exit:
  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }
  return (err);
}

/** Sets a lock on a page of R-Tree record. This is all or none action,
mostly due to we cannot reposition a record in R-Tree (with the
nature of splitting)
@param[in]      pcur            cursor
@param[in]      first_rec       record
@param[in]      index           index
@param[in]      offsets         rec_get_offsets(rec, index)
@param[in]      sel_mode        select mode: SELECT_ORDINARY,
                                SELECT_SKIP_LOKCED, or SELECT_NO_WAIT
@param[in]      mode            lock mode
@param[in]      type            LOCK_ORDINARY, LOCK_GAP, or LOC_REC_NOT_GAP
@param[in]      thr             query thread
@param[in]      mtr             mtr
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, or error code */
static inline dberr_t sel_set_rtr_rec_lock(
    btr_pcur_t *pcur, const rec_t *first_rec, dict_index_t *index,
    const ulint *offsets, select_mode sel_mode, ulint mode, ulint type,
    que_thr_t *thr, mtr_t *mtr) {
  matched_rec_t *match = pcur->m_btr_cur.rtr_info->matches;
  mem_heap_t *heap = nullptr;
  dberr_t err = DB_SUCCESS;
  trx_t *trx = thr_get_trx(thr);
  buf_block_t *cur_block = pcur->get_block();
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *my_offsets = const_cast<ulint *>(offsets);
  rec_t *rec = const_cast<rec_t *>(first_rec);
  rtr_rec_vector *match_rec;
  rtr_rec_vector::iterator end;

  rec_offs_init(offsets_);

  if (match->locked || page_rec_is_supremum(first_rec)) {
    return (DB_SUCCESS_LOCKED_REC);
  }

  ut_ad(page_align(first_rec) == cur_block->frame);
  ut_ad(match->valid);

  rw_lock_x_lock(&(match->block.lock), UT_LOCATION_HERE);
retry:
  cur_block = pcur->get_block();
  ut_ad(rw_lock_own(&(match->block.lock), RW_LOCK_X) ||
        rw_lock_own(&(match->block.lock), RW_LOCK_S));
  ut_ad(page_is_leaf(buf_block_get_frame(cur_block)));

  err = lock_sec_rec_read_check_and_lock(
      lock_duration_t::REGULAR, cur_block, rec, index, my_offsets, sel_mode,
      static_cast<lock_mode>(mode), type, thr);

  switch (err) {
    case DB_SUCCESS:
    case DB_SUCCESS_LOCKED_REC:
    case DB_SKIP_LOCKED:
      goto lock_match;

    case DB_LOCK_WAIT:
    re_scan:
      mtr_commit(mtr);
      trx->error_state = err;
      que_thr_stop_for_mysql(thr);
      thr->lock_state = QUE_THR_LOCK_ROW;
      if (row_mysql_handle_errors(&err, trx, thr, nullptr)) {
        thr->lock_state = QUE_THR_LOCK_NOLOCK;
        mtr_start(mtr);

        mutex_enter(&match->rtr_match_mutex);
        if (!match->valid && match->matched_recs->empty()) {
          mutex_exit(&match->rtr_match_mutex);
          err = DB_RECORD_NOT_FOUND;
          goto func_end;
        }
        mutex_exit(&match->rtr_match_mutex);

        page_no_t page_no = page_get_page_no(pcur->get_page());
        page_id_t page_id(dict_index_get_space(index), page_no);

        cur_block = buf_page_get_gen(
            page_id, dict_table_page_size(index->table), RW_X_LATCH, nullptr,
            Page_fetch::NORMAL, UT_LOCATION_HERE, mtr);
      } else {
        mtr_start(mtr);
        goto func_end;
      }

      DEBUG_SYNC_C("rtr_set_lock_wait");

      if (!match->valid) {
        /* Page got deleted */
        mtr_commit(mtr);
        mtr_start(mtr);
        err = DB_RECORD_NOT_FOUND;
        goto func_end;
      }

      match->matched_recs->clear();

      rtr_cur_search_with_match(
          cur_block, index, pcur->m_btr_cur.rtr_info->search_tuple,
          pcur->m_btr_cur.rtr_info->search_mode, &pcur->m_btr_cur.page_cur,
          pcur->m_btr_cur.rtr_info);

      if (!page_is_leaf(buf_block_get_frame(cur_block))) {
        /* Page got split and promoted (only for
        root page it is possible).  Release the
        page and ask for a re-search */
        mtr_commit(mtr);
        mtr_start(mtr);
        err = DB_RECORD_NOT_FOUND;
        goto func_end;
      }

      rec = pcur->get_rec();
      my_offsets = offsets_;
      my_offsets = rec_get_offsets(rec, index, my_offsets, ULINT_UNDEFINED,
                                   UT_LOCATION_HERE, &heap);

      /* No match record */
      if (page_rec_is_supremum(rec) || !match->valid) {
        mtr_commit(mtr);
        mtr_start(mtr);
        err = DB_RECORD_NOT_FOUND;
        goto func_end;
      }

      goto retry;

    default:
      goto func_end;
  }

lock_match:
  my_offsets = offsets_;
  match_rec = match->matched_recs;
  end = match_rec->end();

  for (rtr_rec_vector::iterator it = match_rec->begin(); it != end; ++it) {
    rtr_rec_t *rtr_rec = &(*it);

    my_offsets = rec_get_offsets(rtr_rec->r_rec, index, my_offsets,
                                 ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

    err = lock_sec_rec_read_check_and_lock(
        lock_duration_t::REGULAR, &match->block, rtr_rec->r_rec, index,
        my_offsets, sel_mode, static_cast<lock_mode>(mode), type, thr);

    switch (err) {
      case DB_SUCCESS:
      case DB_SUCCESS_LOCKED_REC:
        rtr_rec->locked = true;
        break;

      case DB_LOCK_WAIT:
        goto re_scan;

      case DB_SKIP_LOCKED:
        break;

      default:
        goto func_end;
    }
  }

  match->locked = true;

func_end:
  rw_lock_x_unlock(&(match->block.lock));
  if (heap != nullptr) {
    mem_heap_free(heap);
  }

  ut_ad(err != DB_LOCK_WAIT);

  return (err);
}

/** Sets a lock on a record.
mostly due to we cannot reposition a record in R-Tree (with the
nature of splitting)
@param[in]      pcur            cursor
@param[in]      rec             record
@param[in]      index           index
@param[in]      offsets         rec_get_offsets(rec, index)
@param[in]      sel_mode        select mode: SELECT_ORDINARY,
                                SELECT_SKIP_LOKCED, or SELECT_NO_WAIT
@param[in]      mode            lock mode
@param[in]      type            LOCK_ORDINARY, LOCK_GAP, or LOC_REC_NOT_GAP
@param[in]      thr             query thread
@param[in]      mtr             mtr
@return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, or error code */
static inline dberr_t sel_set_rec_lock(btr_pcur_t *pcur, const rec_t *rec,
                                       dict_index_t *index,
                                       const ulint *offsets,
                                       select_mode sel_mode, ulint mode,
                                       ulint type, que_thr_t *thr, mtr_t *mtr) {
  trx_t *trx;
  dberr_t err = DB_SUCCESS;
  const buf_block_t *block;

  block = pcur->get_block();

  trx = thr_get_trx(thr);
  ut_ad(trx_can_be_handled_by_current_thread(trx));

  if (UT_LIST_GET_LEN(trx->lock.trx_locks) > 10000) {
    if (buf_LRU_buf_pool_running_out()) {
      return (DB_LOCK_TABLE_FULL);
    }
  }

  if (index->is_clustered()) {
    err = lock_clust_rec_read_check_and_lock(
        lock_duration_t::REGULAR, block, rec, index, offsets, sel_mode,
        static_cast<lock_mode>(mode), type, thr);
  } else {
    if (dict_index_is_spatial(index)) {
      if (type == LOCK_GAP || type == LOCK_ORDINARY) {
        ib::error(ER_IB_MSG_1026) << "Incorrectly request GAP lock "
                                     "on RTree";
        ut_d(ut_error);
        ut_o(return (DB_SUCCESS));
      }
      err = sel_set_rtr_rec_lock(pcur, rec, index, offsets, sel_mode, mode,
                                 type, thr, mtr);
    } else {
      err = lock_sec_rec_read_check_and_lock(
          lock_duration_t::REGULAR, block, rec, index, offsets, sel_mode,
          static_cast<lock_mode>(mode), type, thr);
    }
  }

  return (err);
}

/** Opens a pcur to a table index. */
static void row_sel_open_pcur(plan_t *plan, /*!< in: table plan */
                              bool search_latch_locked,
                              /*!< in: true if the thread currently
                              has the search latch locked in
                              s-mode */
                              mtr_t *mtr) /*!< in: mtr */
{
  dict_index_t *index;
  que_node_t *exp;
  ulint n_fields;
  ulint has_search_latch = 0; /* RW_S_LATCH or 0 */
  ulint i;

  if (search_latch_locked) {
    has_search_latch = RW_S_LATCH;
  }

  index = plan->index;

  /* Calculate the value of the search tuple: the exact match columns
  get their expressions evaluated when we evaluate the right sides of
  end_conds */

  for (auto cond : plan->end_conds) {
    eval_exp(que_node_get_next(cond->args));
  }

  if (plan->tuple) {
    n_fields = dtuple_get_n_fields(plan->tuple);

    if (plan->n_exact_match < n_fields) {
      /* There is a non-exact match field which must be
      evaluated separately */

      eval_exp(plan->tuple_exps[n_fields - 1]);
    }

    for (i = 0; i < n_fields; i++) {
      exp = plan->tuple_exps[i];

      dfield_copy_data(dtuple_get_nth_field(plan->tuple, i),
                       que_node_get_val(exp));
    }

    /* Open pcur to the index */

    plan->pcur.open_no_init(index, plan->tuple, plan->mode, BTR_SEARCH_LEAF,
                            has_search_latch, mtr, UT_LOCATION_HERE);
  } else {
    /* Open the cursor to the start or the end of the index
    (false: no init) */

    plan->pcur.open_at_side(plan->asc, index, BTR_SEARCH_LEAF, false, 0, mtr);
  }

  ut_ad(plan->n_rows_prefetched == 0);
  ut_ad(plan->n_rows_fetched == 0);
  ut_ad(plan->cursor_at_end == false);

  plan->pcur_is_open = true;
}

/** Restores a stored pcur position to a table index.
 @return true if the cursor should be moved to the next record after we
 return from this function (moved to the previous, in the case of a
 descending cursor) without processing again the current cursor
 record */
static bool row_sel_restore_pcur_pos(plan_t *plan, /*!< in: table plan */
                                     mtr_t *mtr)   /*!< in: mtr */
{
  ut_ad(!plan->cursor_at_end);

  auto relative_position = plan->pcur.get_rel_pos();

  auto equal_position =
      plan->pcur.restore_position(BTR_SEARCH_LEAF, mtr, UT_LOCATION_HERE);

  /* If the cursor is traveling upwards, and relative_position is

  (1) BTR_PCUR_BEFORE: this is not allowed, as we did not have a lock
  yet on the successor of the page infimum;
  (2) BTR_PCUR_AFTER: btr_pcur_t::restore_position placed the cursor on the
  first record GREATER than the predecessor of a page supremum; we have
  not yet processed the cursor record: no need to move the cursor to the
  next record;
  (3) BTR_PCUR_ON: btr_pcur_t::restore_position placed the cursor on the
  last record LESS or EQUAL to the old stored user record; (a) if
  equal_position is false, this means that the cursor is now on a record
  less than the old user record, and we must move to the next record;
  (b) if equal_position is true, then if
  plan->stored_cursor_rec_processed is true, we must move to the next
  record, else there is no need to move the cursor. */

  if (plan->asc) {
    if (relative_position == BTR_PCUR_ON) {
      if (equal_position) {
        return (plan->stored_cursor_rec_processed);
      }

      return true;
    }

    ut_ad(relative_position == BTR_PCUR_AFTER ||
          relative_position == BTR_PCUR_AFTER_LAST_IN_TREE);

    return false;
  }

  /* If the cursor is traveling downwards, and relative_position is

  (1) BTR_PCUR_BEFORE: btr_pcur_t::restore_position placed the cursor on
  the last record LESS than the successor of a page infimum; we have not
  processed the cursor record: no need to move the cursor;
  (2) BTR_PCUR_AFTER: btr_pcur_t::restore_position placed the cursor on the
  first record GREATER than the predecessor of a page supremum; we have
  processed the cursor record: we should move the cursor to the previous
  record;
  (3) BTR_PCUR_ON: btr_pcur_t::restore_position placed the cursor on the
  last record LESS or EQUAL to the old stored user record; (a) if
  equal_position is false, this means that the cursor is now on a record
  less than the old user record, and we need not move to the previous
  record; (b) if equal_position is true, then if
  plan->stored_cursor_rec_processed is true, we must move to the previous
  record, else there is no need to move the cursor. */

  if (relative_position == BTR_PCUR_BEFORE ||
      relative_position == BTR_PCUR_BEFORE_FIRST_IN_TREE) {
    return false;
  }

  if (relative_position == BTR_PCUR_ON) {
    if (equal_position) {
      return (plan->stored_cursor_rec_processed);
    }

    return false;
  }

  ut_ad(relative_position == BTR_PCUR_AFTER ||
        relative_position == BTR_PCUR_AFTER_LAST_IN_TREE);

  return true;
}

/** Resets a plan cursor to a closed state. */
static inline void plan_reset_cursor(plan_t *plan) /*!< in: plan */
{
  plan->pcur_is_open = false;
  plan->cursor_at_end = false;
  plan->n_rows_fetched = 0;
  plan->n_rows_prefetched = 0;
}

/** Tries to do a shortcut to fetch a clustered index record with a unique key,
 using the hash index if possible (not always).
 @return SEL_FOUND, SEL_EXHAUSTED, SEL_RETRY */
static ulint row_sel_try_search_shortcut(
    trx_t *trx,       /*!< in: trx doing the operation. */
    sel_node_t *node, /*!< in: select node for a consistent read */
    plan_t *plan,     /*!< in: plan for a unique search in clustered
                      index */
    bool search_latch_locked,
    /*!< in: whether the search holds latch on
    search system. */
    mtr_t *mtr) /*!< in: mtr */
{
  dict_index_t *index;
  rec_t *rec;
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  ulint ret;
  rec_offs_init(offsets_);

  index = plan->index;

  ut_ad(node->read_view);
  ut_ad(plan->unique_search);
  ut_ad(!plan->must_get_clust);
#ifdef UNIV_DEBUG
  if (search_latch_locked) {
    ut_ad(rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
  }
#endif /* UNIV_DEBUG */

  row_sel_open_pcur(plan, search_latch_locked, mtr);

  rec = plan->pcur.get_rec();

  if (!page_rec_is_user_rec(rec)) {
    return (SEL_RETRY);
  }

  ut_ad(plan->mode == PAGE_CUR_GE);

  /* As the cursor is now placed on a user record after a search with
  the mode PAGE_CUR_GE, the up_match field in the cursor tells how many
  fields in the user record matched to the search tuple */

  if (plan->pcur.get_up_match() < plan->n_exact_match) {
    return (SEL_EXHAUSTED);
  }

  /* This is a non-locking consistent read: if necessary, fetch
  a previous version of the record */

  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &heap);

  if (index->is_clustered()) {
    if (!lock_clust_rec_cons_read_sees(rec, index, offsets, node->read_view)) {
      ret = SEL_RETRY;
      goto func_exit;
    }
  } else if (!srv_read_only_mode &&
             !lock_sec_rec_cons_read_sees(rec, index, node->read_view)) {
    ret = SEL_RETRY;
    goto func_exit;
  }

  /* Test the deleted flag. */

  if (rec_get_deleted_flag(rec, dict_table_is_comp(plan->table))) {
    ret = SEL_EXHAUSTED;
    goto func_exit;
  }

  /* Fetch the columns needed in test conditions.  The index
  record is protected by a page latch that was acquired when
  plan->pcur was positioned.  The latch will not be released
  until mtr_commit(mtr). */

  row_sel_fetch_columns(trx, index, rec, offsets,
                        UT_LIST_GET_FIRST(plan->columns));

  /* Test the rest of search conditions */

  if (!row_sel_test_other_conds(plan)) {
    ret = SEL_EXHAUSTED;
    goto func_exit;
  }

  ut_ad(plan->pcur.m_latch_mode == BTR_SEARCH_LEAF);

  plan->n_rows_fetched++;
  ret = SEL_FOUND;
func_exit:
  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }
  return (ret);
}

/** Performs a select step.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_sel(sel_node_t *node, /*!< in: select node */
                                     que_thr_t *thr)   /*!< in: query thread */
{
  dict_index_t *index;
  plan_t *plan;
  mtr_t mtr;
  bool moved;
  rec_t *rec;
  rec_t *old_vers;
  rec_t *clust_rec;
  bool search_latch_locked;
  bool consistent_read;

  /* The following flag becomes true when we are doing a
  consistent read from a non-clustered index and we must look
  at the clustered index to find out the previous delete mark
  state of the non-clustered record: */

  bool cons_read_requires_clust_rec = false;
  ulint cost_counter = 0;
  bool cursor_just_opened;
  bool must_go_to_next;
  bool mtr_has_extra_clust_latch = false;
  /* true if the search was made using
  a non-clustered index, and we had to
  access the clustered record: now &mtr
  contains a clustered index latch, and
  &mtr must be committed before we move
  to the next non-clustered record */
  ulint found_flag;
  dberr_t err;
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(thr->run_node == node);

  search_latch_locked = false;

  if (node->read_view) {
    /* In consistent reads, we try to do with the hash index and
    not to use the buffer page get. This is to reduce memory bus
    load resulting from semaphore operations. The search latch
    will be s-locked when we access an index with a unique search
    condition, but not locked when we access an index with a
    less selective search condition. */

    consistent_read = true;
  } else {
    consistent_read = false;
  }

table_loop:
  /* TABLE LOOP
  ----------
  This is the outer major loop in calculating a join. We come here when
  node->fetch_table changes, and after adding a row to aggregate totals
  and, of course, when this function is called. */

  ut_ad(mtr_has_extra_clust_latch == false);

  plan = sel_node_get_nth_plan(node, node->fetch_table);
  index = plan->index;

  if (plan->n_rows_prefetched > 0) {
    sel_dequeue_prefetched_row(plan);

    goto next_table_no_mtr;
  }

  if (plan->cursor_at_end) {
    /* The cursor has already reached the result set end: no more
    rows to process for this table cursor, as also the prefetch
    stack was empty */

    ut_ad(plan->pcur_is_open);

    goto table_exhausted_no_mtr;
  }

  /* Open a cursor to index, or restore an open cursor position */

  mtr_start(&mtr);

  if (consistent_read && plan->unique_search && !plan->pcur_is_open &&
      !plan->must_get_clust && !plan->table->big_rows) {
    if (!search_latch_locked) {
      rw_lock_s_lock(btr_get_search_latch(index), UT_LOCATION_HERE);

      search_latch_locked = true;
    } else if (rw_lock_get_writer(btr_get_search_latch(index)) ==
               RW_LOCK_X_WAIT) {
      /* There is an x-latch request waiting: release the
      s-latch for a moment; as an s-latch here is often
      kept for some 10 searches before being released,
      a waiting x-latch request would block other threads
      from acquiring an s-latch for a long time, lowering
      performance significantly in multiprocessors. */

      rw_lock_s_unlock(btr_get_search_latch(index));
      rw_lock_s_lock(btr_get_search_latch(index), UT_LOCATION_HERE);
    }

    found_flag = row_sel_try_search_shortcut(thr_get_trx(thr), node, plan,
                                             search_latch_locked, &mtr);

    if (found_flag == SEL_FOUND) {
      goto next_table;

    } else if (found_flag == SEL_EXHAUSTED) {
      goto table_exhausted;
    }

    ut_ad(found_flag == SEL_RETRY);

    plan_reset_cursor(plan);

    mtr_commit(&mtr);
    mtr_start(&mtr);
  }

  if (search_latch_locked) {
    rw_lock_s_unlock(btr_get_search_latch(index));

    search_latch_locked = false;
  }

  if (!plan->pcur_is_open) {
    /* Evaluate the expressions to build the search tuple and
    open the cursor */

    row_sel_open_pcur(plan, search_latch_locked, &mtr);

    cursor_just_opened = true;

    /* A new search was made: increment the cost counter */
    cost_counter++;
  } else {
    /* Restore pcur position to the index */

    must_go_to_next = row_sel_restore_pcur_pos(plan, &mtr);

    cursor_just_opened = false;

    if (must_go_to_next) {
      /* We have already processed the cursor record: move
      to the next */

      goto next_rec;
    }
  }

rec_loop:
  /* RECORD LOOP
  -----------
  In this loop we use pcur and try to fetch a qualifying row, and
  also fill the prefetch buffer for this table if n_rows_fetched has
  exceeded a threshold. While we are inside this loop, the following
  holds:
  (1) &mtr is started,
  (2) pcur is positioned and open.

  NOTE that if cursor_just_opened is true here, it means that we came
  to this point right after row_sel_open_pcur. */

  ut_ad(mtr_has_extra_clust_latch == false);

  rec = plan->pcur.get_rec();

  /* PHASE 1: Set a lock if specified */

  if (!node->asc && cursor_just_opened && !page_rec_is_supremum(rec)) {
    /* Do not support "descending search" for Spatial index */
    ut_ad(!dict_index_is_spatial(index));

    /* When we open a cursor for a descending search, we must set
    a next-key lock on the successor record: otherwise it would
    be possible to insert new records next to the cursor position,
    and it might be that these new records should appear in the
    search result set, resulting in the phantom problem. */

    if (!consistent_read) {
      rec_t *next_rec = page_rec_get_next(rec);
      ulint lock_type;
      trx_t *trx;

      trx = thr_get_trx(thr);

      offsets = rec_get_offsets(next_rec, index, offsets, ULINT_UNDEFINED,
                                UT_LOCATION_HERE, &heap);

      if (trx->skip_gap_locks()) {
        if (page_rec_is_supremum(next_rec)) {
          goto skip_lock;
        }

        lock_type = LOCK_REC_NOT_GAP;
      } else {
        lock_type = LOCK_ORDINARY;
      }

      err = sel_set_rec_lock(&plan->pcur, next_rec, index, offsets,
                             SELECT_ORDINARY, node->row_lock_mode, lock_type,
                             thr, &mtr);

      switch (err) {
        case DB_SUCCESS_LOCKED_REC:
          err = DB_SUCCESS;
        case DB_SUCCESS:
          break;
        default:
          /* Note that in this case we will store in pcur
          the PREDECESSOR of the record we are waiting
          the lock for */
          goto lock_wait_or_error;
      }
    }
  }

skip_lock:
  if (page_rec_is_infimum(rec)) {
    /* The infimum record on a page cannot be in the result set,
    and neither can a record lock be placed on it: we skip such
    a record. We also increment the cost counter as we may have
    processed yet another page of index. */

    cost_counter++;

    goto next_rec;
  }

  if (!consistent_read) {
    /* Try to place a lock on the index record */

    ulint lock_type;
    trx_t *trx;

    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    trx = thr_get_trx(thr);

    if (trx->skip_gap_locks() || dict_index_is_spatial(index)) {
      if (page_rec_is_supremum(rec)) {
        goto next_rec;
      }

      lock_type = LOCK_REC_NOT_GAP;
    } else {
      lock_type = LOCK_ORDINARY;
    }

    err = sel_set_rec_lock(&plan->pcur, rec, index, offsets, SELECT_ORDINARY,
                           node->row_lock_mode, lock_type, thr, &mtr);

    switch (err) {
      case DB_SUCCESS_LOCKED_REC:
        err = DB_SUCCESS;
      case DB_SUCCESS:
        break;
      default:
        goto lock_wait_or_error;
    }
  }

  if (page_rec_is_supremum(rec)) {
    /* A page supremum record cannot be in the result set: skip
    it now when we have placed a possible lock on it */

    goto next_rec;
  }

  ut_ad(page_rec_is_user_rec(rec));

  if (cost_counter > SEL_COST_LIMIT) {
    /* Now that we have placed the necessary locks, we can stop
    for a while and store the cursor position; NOTE that if we
    would store the cursor position BEFORE placing a record lock,
    it might happen that the cursor would jump over some records
    that another transaction could meanwhile insert adjacent to
    the cursor: this would result in the phantom problem. */

    goto stop_for_a_while;
  }

  /* PHASE 2: Check a mixed index mix id if needed */

  if (plan->unique_search && cursor_just_opened) {
    ut_ad(plan->mode == PAGE_CUR_GE);

    /* As the cursor is now placed on a user record after a search
    with the mode PAGE_CUR_GE, the up_match field in the cursor
    tells how many fields in the user record matched to the search
    tuple */

    if (plan->pcur.get_up_match() < plan->n_exact_match) {
      goto table_exhausted;
    }

    /* Ok, no need to test end_conds or mix id */
  }

  /* We are ready to look at a possible new index entry in the result
  set: the cursor is now placed on a user record */

  /* PHASE 3: Get previous version in a consistent read */

  cons_read_requires_clust_rec = false;
  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &heap);

  if (consistent_read) {
    /* This is a non-locking consistent read: if necessary, fetch
    a previous version of the record */

    if (index->is_clustered()) {
      if (!lock_clust_rec_cons_read_sees(rec, index, offsets,
                                         node->read_view)) {
        err = row_sel_build_prev_vers(node->read_view, index, rec, &offsets,
                                      &heap, &plan->old_vers_heap, &old_vers,
                                      &mtr);

        if (err != DB_SUCCESS) {
          goto lock_wait_or_error;
        }

        if (old_vers == nullptr) {
          /* The record does not exist
          in our read view. Skip it, but
          first attempt to determine
          whether the index segment we
          are searching through has been
          exhausted. */

          offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                                    UT_LOCATION_HERE, &heap);

          /* Fetch the columns needed in
          test conditions. The clustered
          index record is protected by a
          page latch that was acquired
          by row_sel_open_pcur() or
          row_sel_restore_pcur_pos().
          The latch will not be released
          until mtr_commit(mtr). */

          const bool allow_null_lob = true;
          row_sel_fetch_columns(thr_get_trx(thr), index, rec, offsets,
                                UT_LIST_GET_FIRST(plan->columns),
                                allow_null_lob);

          if (!row_sel_test_end_conds(plan)) {
            goto table_exhausted;
          }

          goto next_rec;
        }

        rec = old_vers;
      }
    } else if (!srv_read_only_mode &&
               !lock_sec_rec_cons_read_sees(rec, index, node->read_view)) {
      cons_read_requires_clust_rec = true;
    }
  }

  /* PHASE 4: Test search end conditions and deleted flag */

  /* Fetch the columns needed in test conditions.  The record is
  protected by a page latch that was acquired by
  row_sel_open_pcur() or row_sel_restore_pcur_pos().  The latch
  will not be released until mtr_commit(mtr). */

  row_sel_fetch_columns(thr_get_trx(thr), index, rec, offsets,
                        UT_LIST_GET_FIRST(plan->columns));

  /* Test the selection end conditions: these can only contain columns
  which already are found in the index, even though the index might be
  non-clustered */

  if (plan->unique_search && cursor_just_opened) {
    /* No test necessary: the test was already made above */

  } else if (!row_sel_test_end_conds(plan)) {
    goto table_exhausted;
  }

  if (rec_get_deleted_flag(rec, dict_table_is_comp(plan->table)) &&
      !cons_read_requires_clust_rec) {
    /* The record is delete marked: we can skip it if this is
    not a consistent read which might see an earlier version
    of a non-clustered index record */

    if (plan->unique_search) {
      goto table_exhausted;
    }

    goto next_rec;
  }

  /* PHASE 5: Get the clustered index record, if needed and if we did
  not do the search using the clustered index */

  if (plan->must_get_clust || cons_read_requires_clust_rec) {
    /* It was a non-clustered index and we must fetch also the
    clustered index record */

    err = row_sel_get_clust_rec(node, plan, rec, thr, &clust_rec, &mtr);
    mtr_has_extra_clust_latch = true;

    if (err != DB_SUCCESS) {
      goto lock_wait_or_error;
    }

    /* Retrieving the clustered record required a search:
    increment the cost counter */

    cost_counter++;

    if (clust_rec == nullptr) {
      /* The record did not exist in the read view */
      ut_ad(consistent_read);

      goto next_rec;
    }

    if (rec_get_deleted_flag(clust_rec, dict_table_is_comp(plan->table))) {
      /* The record is delete marked: we can skip it */

      goto next_rec;
    }

    if (node->can_get_updated) {
      plan->clust_pcur.store_position(&mtr);
    }
  }

  /* PHASE 6: Test the rest of search conditions */

  if (!row_sel_test_other_conds(plan)) {
    if (plan->unique_search) {
      goto table_exhausted;
    }

    goto next_rec;
  }

  /* PHASE 7: We found a new qualifying row for the current table; push
  the row if prefetch is on, or move to the next table in the join */

  plan->n_rows_fetched++;

  ut_ad(plan->pcur.m_latch_mode == BTR_SEARCH_LEAF);

  if ((plan->n_rows_fetched <= SEL_PREFETCH_LIMIT) || plan->unique_search ||
      plan->no_prefetch || plan->table->big_rows) {
    /* No prefetch in operation: go to the next table */

    goto next_table;
  }

  sel_enqueue_prefetched_row(plan);

  if (plan->n_rows_prefetched == SEL_MAX_N_PREFETCH) {
    /* The prefetch buffer is now full */

    sel_dequeue_prefetched_row(plan);

    goto next_table;
  }

next_rec:
  ut_ad(!search_latch_locked);

  if (mtr_has_extra_clust_latch) {
    /* We must commit &mtr if we are moving to the next
    non-clustered index record, because we could break the
    latching order if we would access a different clustered
    index page right away without releasing the previous. */

    goto commit_mtr_for_a_while;
  }

  if (node->asc) {
    moved = plan->pcur.move_to_next(&mtr);
  } else {
    moved = plan->pcur.move_to_prev(&mtr);
  }

  if (!moved) {
    goto table_exhausted;
  }

  cursor_just_opened = false;

  /* END OF RECORD LOOP
  ------------------ */
  goto rec_loop;

next_table:
  /* We found a record which satisfies the conditions: we can move to
  the next table or return a row in the result set */

  ut_ad(plan->pcur.is_on_user_rec());

  if (plan->unique_search && !node->can_get_updated) {
    plan->cursor_at_end = true;
  } else {
    ut_ad(!search_latch_locked);

    plan->stored_cursor_rec_processed = true;

    plan->pcur.store_position(&mtr);
  }

  mtr_commit(&mtr);

  mtr_has_extra_clust_latch = false;

next_table_no_mtr:
  /* If we use 'goto' to this label, it means that the row was popped
  from the prefetched rows stack, and &mtr is already committed */

  if (node->fetch_table + 1 == node->n_tables) {
    sel_eval_select_list(node);

    if (node->is_aggregate) {
      goto table_loop;
    }

    sel_assign_into_var_values(node->into_list, node);

    thr->run_node = que_node_get_parent(node);

    err = DB_SUCCESS;
    goto func_exit;
  }

  node->fetch_table++;

  /* When we move to the next table, we first reset the plan cursor:
  we do not care about resetting it when we backtrack from a table */

  plan_reset_cursor(sel_node_get_nth_plan(node, node->fetch_table));

  goto table_loop;

table_exhausted:
  /* The table cursor pcur reached the result set end: backtrack to the
  previous table in the join if we do not have cached prefetched rows */

  plan->cursor_at_end = true;

  mtr_commit(&mtr);

  mtr_has_extra_clust_latch = false;

  if (plan->n_rows_prefetched > 0) {
    /* The table became exhausted during a prefetch */

    sel_dequeue_prefetched_row(plan);

    goto next_table_no_mtr;
  }

table_exhausted_no_mtr:
  if (node->fetch_table == 0) {
    err = DB_SUCCESS;

    if (node->is_aggregate && !node->aggregate_already_fetched) {
      node->aggregate_already_fetched = true;

      sel_assign_into_var_values(node->into_list, node);

      thr->run_node = que_node_get_parent(node);
    } else {
      node->state = SEL_NODE_NO_MORE_ROWS;

      thr->run_node = que_node_get_parent(node);
    }

    goto func_exit;
  }

  node->fetch_table--;

  goto table_loop;

stop_for_a_while:
  /* Return control for a while to que_run_threads, so that runaway
  queries can be canceled. NOTE that when we come here, we must, in a
  locking read, have placed the necessary (possibly waiting request)
  record lock on the cursor record or its successor: when we reposition
  the cursor, this record lock guarantees that nobody can meanwhile have
  inserted new records which should have appeared in the result set,
  which would result in the phantom problem. */

  ut_ad(!search_latch_locked);

  plan->stored_cursor_rec_processed = false;
  plan->pcur.store_position(&mtr);

  mtr_commit(&mtr);

#ifdef UNIV_DEBUG
  {
    btrsea_sync_check check(true);

    ut_ad(!sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */

  err = DB_SUCCESS;
  goto func_exit;

commit_mtr_for_a_while:
  /* Stores the cursor position and commits &mtr; this is used if
  &mtr may contain latches which would break the latching order if
  &mtr would not be committed and the latches released. */

  plan->stored_cursor_rec_processed = true;

  ut_ad(!search_latch_locked);
  plan->pcur.store_position(&mtr);

  mtr_commit(&mtr);

  mtr_has_extra_clust_latch = false;

#ifdef UNIV_DEBUG
  {
    dict_sync_check check(true);

    ut_ad(!sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */

  goto table_loop;

lock_wait_or_error:
  /* See the note at stop_for_a_while: the same holds for this case */

  ut_ad(!plan->pcur.is_before_first_on_page() || !node->asc);
  ut_ad(!search_latch_locked);

  plan->stored_cursor_rec_processed = false;
  plan->pcur.store_position(&mtr);

  mtr_commit(&mtr);

#ifdef UNIV_DEBUG
  {
    dict_sync_check check(true);

    ut_ad(!sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */

func_exit:
  if (search_latch_locked) {
    rw_lock_s_unlock(btr_get_search_latch(index));
  }

  if (heap != nullptr) {
    mem_heap_free(heap);
  }
  return (err);
}

/** Performs a select step. This is a high-level function used in SQL execution
 graphs.
 @return query thread to run next or NULL */
que_thr_t *row_sel_step(que_thr_t *thr) /*!< in: query thread */
{
  sel_node_t *node;

  ut_ad(thr);

  node = static_cast<sel_node_t *>(thr->run_node);

  ut_ad(que_node_get_type(node) == QUE_NODE_SELECT);

  /* If this is a new time this node is executed (or when execution
  resumes after wait for a table intention lock), set intention locks
  on the tables, or assign a read view */

  if (node->into_list && (thr->prev_node == que_node_get_parent(node))) {
    node->state = SEL_NODE_OPEN;
  }

  if (node->state == SEL_NODE_OPEN) {
    /* It may be that the current session has not yet started
    its transaction, or it has been committed: */

    trx_start_if_not_started_xa(thr_get_trx(thr), false, UT_LOCATION_HERE);

    plan_reset_cursor(sel_node_get_nth_plan(node, 0));

    if (node->consistent_read) {
      /* Assign a read view for the query */
      trx_assign_read_view(thr_get_trx(thr));

      if (thr_get_trx(thr)->read_view != nullptr) {
        node->read_view = thr_get_trx(thr)->read_view;
      } else {
        node->read_view = nullptr;
      }

    } else {
      sym_node_t *table_node;
      lock_mode i_lock_mode;

      if (node->set_x_locks) {
        i_lock_mode = LOCK_IX;
      } else {
        i_lock_mode = LOCK_IS;
      }

      for (table_node = node->table_list; table_node != nullptr;
           table_node =
               static_cast<sym_node_t *>(que_node_get_next(table_node))) {
        dberr_t err = lock_table(0, table_node->table, i_lock_mode, thr);

        if (err != DB_SUCCESS) {
          trx_t *trx;

          trx = thr_get_trx(thr);
          trx->error_state = err;

          return (nullptr);
        }
      }
    }

    /* If this is an explicit cursor, copy stored procedure
    variable values, so that the values cannot change between
    fetches (currently, we copy them also for non-explicit
    cursors) */

    if (node->explicit_cursor && UT_LIST_GET_FIRST(node->copy_variables)) {
      row_sel_copy_input_variable_vals(node);
    }

    node->state = SEL_NODE_FETCH;
    node->fetch_table = 0;

    if (node->is_aggregate) {
      /* Reset the aggregate total values */
      sel_reset_aggregate_vals(node);
    }
  }

  dberr_t err = row_sel(node, thr);

  /* NOTE! if queries are parallelized, the following assignment may
  have problems; the assignment should be made only if thr is the
  only top-level thr in the graph: */

  thr->graph->last_sel_node = node;

  if (err != DB_SUCCESS) {
    thr_get_trx(thr)->error_state = err;

    return (nullptr);
  }

  return (thr);
}

/** Performs a fetch for a cursor.
 @return query thread to run next or NULL */
que_thr_t *fetch_step(que_thr_t *thr) /*!< in: query thread */
{
  sel_node_t *sel_node;
  fetch_node_t *node;

  ut_ad(thr);

  node = static_cast<fetch_node_t *>(thr->run_node);
  sel_node = node->cursor_def;

  ut_ad(que_node_get_type(node) == QUE_NODE_FETCH);

  if (thr->prev_node != que_node_get_parent(node)) {
    if (sel_node->state != SEL_NODE_NO_MORE_ROWS) {
      if (node->into_list) {
        sel_assign_into_var_values(node->into_list, sel_node);
      } else {
        bool ret = (*node->func->func)(sel_node, node->func->arg);

        if (!ret) {
          sel_node->state = SEL_NODE_NO_MORE_ROWS;
        }
      }
    }

    thr->run_node = que_node_get_parent(node);

    return (thr);
  }

  /* Make the fetch node the parent of the cursor definition for
  the time of the fetch, so that execution knows to return to this
  fetch node after a row has been selected or we know that there is
  no row left */

  sel_node->common.parent = node;

  if (sel_node->state == SEL_NODE_CLOSED) {
    ib::error(ER_IB_MSG_1027) << "fetch called on a closed cursor";

    thr_get_trx(thr)->error_state = DB_ERROR;

    return (nullptr);
  }

  thr->run_node = sel_node;

  return (thr);
}

void row_sel_convert_mysql_key_to_innobase(dtuple_t *tuple, byte *buf,
                                           ulint buf_len, dict_index_t *index,
                                           const byte *key_ptr, ulint key_len) {
  byte *original_buf = buf;
  const byte *original_key_ptr = key_ptr;
  dict_field_t *field;
  dfield_t *dfield;
  ulint data_offset;
  ulint data_len;
  ulint data_field_len;
  bool is_null;
  const byte *key_end;
  ulint n_fields = 0;

  /* For documentation of the key value storage format in MySQL, see
  ha_innobase::store_key_val_for_row() in ha_innodb.cc. */

  key_end = key_ptr + key_len;

  /* Permit us to access any field in the tuple (ULINT_MAX): */

  dtuple_set_n_fields(tuple, ULINT_MAX);

  dfield = dtuple_get_nth_field(tuple, 0);
  field = index->get_field(0);

  if (UNIV_UNLIKELY(dfield_get_type(dfield)->mtype == DATA_SYS)) {
    /* A special case: we are looking for a position in the
    generated clustered index which InnoDB automatically added
    to a table with no primary key: the first and the only
    ordering column is ROW_ID which InnoDB stored to the key_ptr
    buffer. */

    ut_a(key_len == DATA_ROW_ID_LEN);

    dfield_set_data(dfield, key_ptr, DATA_ROW_ID_LEN);

    dtuple_set_n_fields(tuple, 1);

    return;
  }

  while (key_ptr < key_end) {
    ulint type = dfield_get_type(dfield)->mtype;
    ut_a(field->col->mtype == type);

    data_offset = 0;
    is_null = false;

    if (!(dfield_get_type(dfield)->prtype & DATA_NOT_NULL)) {
      /* The first byte in the field tells if this is
      an SQL NULL value */

      data_offset = 1;

      if (*key_ptr != 0) {
        dfield_set_null(dfield);

        is_null = true;
      }
    }

    /* Calculate data length and data field total length */
    if (DATA_LARGE_MTYPE(type) || DATA_GEOMETRY_MTYPE(type)) {
      /* For R-tree index, data length should be the
      total size of the wkb data.*/
      if (dict_index_is_spatial(index)) {
        ut_ad(DATA_GEOMETRY_MTYPE(type));
        data_len = key_len;
        data_field_len = data_offset + data_len;
      } else {
        /* The key field is a column prefix of a BLOB
        or TEXT, except DATA_POINT of GEOMETRY. */

        ut_a(field->prefix_len > 0 || DATA_POINT_MTYPE(type));

        /* MySQL stores the actual data length to the
        first 2 bytes after the optional SQL NULL
        marker byte. The storage format is
        little-endian, that is, the most significant
        byte at a higher address. In UTF-8, MySQL
        seems to reserve field->prefix_len bytes for
        storing this field in the key value buffer,
        even though the actual value only takes data
        len bytes from the start.
        For POINT of GEOMETRY, which has no prefix
        because it's now a fixed length type in
        InnoDB, we have to get DATA_POINT_LEN bytes,
        which is original prefix length of POINT. */

        data_len = key_ptr[data_offset] + 256 * key_ptr[data_offset + 1];
        data_field_len =
            data_offset + 2 +
            (type == DATA_POINT ? DATA_POINT_LEN : field->prefix_len);

        data_offset += 2;

        /* Now that we know the length, we store the
        column value like it would be a fixed char
        field */
      }

    } else if (field->prefix_len > 0) {
      /* Looks like MySQL pads unused end bytes in the
      prefix with space. Therefore, also in UTF-8, it is ok
      to compare with a prefix containing full prefix_len
      bytes, and no need to take at most prefix_len / 3
      UTF-8 characters from the start.
      If the prefix is used as the upper end of a LIKE
      'abc%' query, then MySQL pads the end with chars
      0xff. TODO: in that case does it any harm to compare
      with the full prefix_len bytes. How do characters
      0xff in UTF-8 behave? */

      data_len = field->prefix_len;
      data_field_len = data_offset + data_len;
    } else {
      data_len = dfield_get_type(dfield)->len;
      data_field_len = data_offset + data_len;
    }

    if ((dtype_get_mysql_type(dfield_get_type(dfield)) ==
         DATA_MYSQL_TRUE_VARCHAR) &&
        (type != DATA_INT)) {
      /* In a MySQL key value format, a true VARCHAR is
      always preceded by 2 bytes of a length field.
      dfield_get_type(dfield)->len returns the maximum
      'payload' len in bytes. That does not include the
      2 bytes that tell the actual data length.

      We added the check != DATA_INT to make sure we do
      not treat MySQL ENUM or SET as a true VARCHAR! */

      data_len += 2;
      data_field_len += 2;
    }

    /* Storing may use at most data_len bytes of buf */

    if (UNIV_LIKELY(!is_null)) {
      buf = row_mysql_store_col_in_innobase_format(
          dfield, buf, false, /* MySQL key value format col */
          key_ptr + data_offset, data_len, dict_table_is_comp(index->table));
      ut_a(buf <= original_buf + buf_len);
    }

    key_ptr += data_field_len;

    if (UNIV_UNLIKELY(key_ptr > key_end)) {
      /* The last field in key was not a complete key field
      but a prefix of it.

      Print a warning about this! HA_READ_PREFIX_LAST does
      not currently work in InnoDB with partial-field key
      value prefixes. Since MySQL currently uses a padding
      trick to calculate LIKE 'abc%' type queries there
      should never be partial-field prefixes in searches. */

      ib::warn(ER_IB_MSG_1028)
          << "Using a partial-field key prefix in"
             " search, index "
          << index->name << " of table " << index->table->name
          << ". Last data field length " << data_field_len
          << " bytes, key ptr now"
             " exceeds key end by "
          << (key_ptr - key_end) << " bytes. Key value in the MySQL format:";

      ut_print_buf(stderr, original_key_ptr, key_len);
      putc('\n', stderr);

      if (!is_null) {
        ulint len = dfield_get_len(dfield);
        dfield_set_len(dfield, len - (ulint)(key_ptr - key_end));
      }
      ut_d(ut_error);
    }

    n_fields++;
    field++;
    dfield++;
  }

  ut_a(buf <= original_buf + buf_len);

  /* We set the length of tuple to n_fields: we assume that the memory
  area allocated for it is big enough (usually bigger than n_fields). */

  dtuple_set_n_fields(tuple, n_fields);
}

/** Stores the row id to the prebuilt struct. */
static void row_sel_store_row_id_to_prebuilt(
    row_prebuilt_t *prebuilt,  /*!< in/out: prebuilt */
    const rec_t *index_rec,    /*!< in: record */
    const dict_index_t *index, /*!< in: index of the record */
    const ulint *offsets)      /*!< in: rec_get_offsets
                               (index_rec, index) */
{
  const byte *data;
  ulint len;

  ut_ad(rec_offs_validate(index_rec, index, offsets));

  data = rec_get_nth_field(index, index_rec, offsets,
                           index->get_sys_col_pos(DATA_ROW_ID), &len);

  if (UNIV_UNLIKELY(len != DATA_ROW_ID_LEN)) {
    ib::error(ER_IB_MSG_1029)
        << "Row id field is wrong length " << len
        << " in"
           " index "
        << index->name << " of table " << index->table->name
        << ", Field number " << index->get_sys_col_pos(DATA_ROW_ID)
        << ", record:";

    rec_print_new(stderr, index_rec, offsets);
    putc('\n', stderr);
    ut_error;
  }

  ut_memcpy(prebuilt->row_id, data, len);
}

/** Stores a non-SQL-NULL field in the MySQL format. The counterpart of this
function is row_mysql_store_col_in_innobase_format() in row0mysql.cc.
@param[in,out] dest             buffer where to store; NOTE
                                that BLOBs are not in themselves stored
                                here: the caller must allocate and copy
                                the BLOB into buffer before, and pass
                                the pointer to the BLOB in 'data'
@param[in]      templ           MySQL column template. Its following fields
                                are referenced: type, is_unsigned,
mysql_col_len, mbminlen, mbmaxlen
@param[in]      index           InnoDB index
@param[in]      field_no        templ->rec_field_no or templ->clust_rec_field_no
                                or templ->icp_rec_field_no
@param[in]      data            data to store
@param[in]      len             length of the data
@param[in]      sec_field       secondary index field no if the secondary index
                                record but the prebuilt template is in
                                clustered index format and used only for end
                                range comparison. */
void row_sel_field_store_in_mysql_format_func(
    byte *dest, const mysql_row_templ_t *templ, const dict_index_t *index,
    IF_DEBUG(ulint field_no, ) const byte *data,
    ulint len IF_DEBUG(, ulint sec_field)) {
  byte *ptr;
#ifdef UNIV_DEBUG
  const dict_field_t *field =
      templ->is_virtual ? nullptr : index->get_field(field_no);

  bool clust_templ_for_sec = (sec_field != ULINT_UNDEFINED);
#endif /* UNIV_DEBUG */

  if (templ->is_multi_val) {
    ib::fatal(UT_LOCATION_HERE, ER_CONVERT_MULTI_VALUE)
        << "Table name: " << index->table->name
        << " Index name: " << index->name;
  }

  auto const mysql_col_len = templ->mysql_col_len;

  ut_ad(rec_field_not_null_not_add_col_def(len));
  UNIV_MEM_ASSERT_RW(data, len);
  UNIV_MEM_ASSERT_W(dest, mysql_col_len);
  UNIV_MEM_INVALID(dest, mysql_col_len);

  switch (templ->type) {
    const byte *field_end;
    byte *pad;
    case DATA_INT:
      /* Convert integer data from Innobase to a little-endian
      format, sign bit restored to normal */

      ptr = dest + len;

      for (;;) {
        ptr--;
        *ptr = *data;
        if (ptr == dest) {
          break;
        }
        data++;
      }

      if (!templ->is_unsigned) {
        dest[len - 1] = (byte)(dest[len - 1] ^ 128);
      }

      ut_ad(mysql_col_len == len);

      break;

    case DATA_VARCHAR:
    case DATA_VARMYSQL:
    case DATA_BINARY:
      field_end = dest + mysql_col_len;

      if (templ->mysql_type == DATA_MYSQL_TRUE_VARCHAR) {
        /* This is a >= 5.0.3 type true VARCHAR. Store the
        length of the data to the first byte or the first
        two bytes of dest. */

        dest =
            row_mysql_store_true_var_len(dest, len, templ->mysql_length_bytes);
        /* Copy the actual data. Leave the rest of the
        buffer uninitialized. */
        memcpy(dest, data, len);
        break;
      }

      /* Copy the actual data */
      ut_memcpy(dest, data, len);

      /* Pad with trailing spaces. */

      pad = dest + len;

      ut_ad(templ->mbminlen <= templ->mbmaxlen);

      /* We treat some Unicode charset strings specially. */
      switch (templ->mbminlen) {
        case 4:
          /* InnoDB should never have stripped partial
          UTF-32 characters. */
          ut_a(!(len & 3));
          break;
        case 2:
          /* A space char is two bytes,
          0x0020 in UCS2 and UTF-16 */

          if (UNIV_UNLIKELY(len & 1)) {
            /* A 0x20 has been stripped from the column.
            Pad it back. */

            if (pad < field_end) {
              *pad++ = 0x20;
            }
          }
      }

      row_mysql_pad_col(templ->mbminlen, pad, field_end - pad);
      break;

    case DATA_BLOB:
      /* Store a pointer to the BLOB buffer to dest: the BLOB was
      already copied to the buffer in row_sel_store_mysql_rec */

      row_mysql_store_blob_ref(dest, mysql_col_len, data, len);
      break;

    case DATA_POINT:
    case DATA_VAR_POINT:
    case DATA_GEOMETRY:
      /* We store all geometry data as BLOB data at server layer. */
      row_mysql_store_geometry(dest, mysql_col_len, data, len);
      break;

    case DATA_MYSQL:
      memcpy(dest, data, len);

      ut_ad(mysql_col_len >= len);
      ut_ad(templ->mbmaxlen >= templ->mbminlen);

      /* If field_no equals to templ->icp_rec_field_no, we are examining a row
      pointed by "icp_rec_field_no". There is possibility that icp_rec_field_no
      refers to a field in a secondary index while templ->rec_field_no points
      to field in a primary index. The length should still be equal, unless the
      field pointed by icp_rec_field_no has a prefix or this is a virtual
      column.
      For end range condition check of secondary index with cluster index
      template (clust_templ_for_sec), the index column data length (len)
      could be smaller than the actual column length (mysql_col_len) if index
      is on column prefix. This is not a real issue because the end range check
      would only need the prefix part. The length check assert is relaxed for
      clust_templ_for_sec. */
      ut_ad(templ->is_virtual || templ->mbmaxlen > templ->mbminlen ||
            mysql_col_len == len || clust_templ_for_sec ||
            (field_no == templ->icp_rec_field_no && field->prefix_len > 0));

      /* The following assertion would fail for old tables
      containing UTF-8 ENUM columns due to Bug #9526. */
      ut_ad(!templ->mbmaxlen || !(mysql_col_len % templ->mbmaxlen));
      /* Length of the record will be less in case of
      clust_templ_for_sec is true or if it is fetched
      from prefix virtual column in virtual index. */
      ut_ad(templ->is_virtual || clust_templ_for_sec ||
            len * templ->mbmaxlen >= mysql_col_len ||
            index->has_row_versions() ||
            (field_no == templ->icp_rec_field_no && field->prefix_len > 0));
      ut_ad(templ->is_virtual || !(field->prefix_len % templ->mbmaxlen));

      /* Pad with spaces. This undoes the stripping
      done in row0mysql.cc, function
      row_mysql_store_col_in_innobase_format(). */
      if ((templ->mbminlen == 1 && templ->mbmaxlen != 1) ||
          (templ->is_virtual && mysql_col_len > len)) {
        /* NOTE: This comment is for the second condition:
        This probably comes from a prefix virtual index, where no complete
        value can be got because the full virtual column can only be
        calculated in server layer for now. Since server now assumes the
        returned value should always have padding spaces, thus the fixup.
        However, a proper and more efficient solution is that server does
        not depend on the trailing spaces to check the terminal of the CHAR
        string, because at least in this case,server should know it's a prefix
        index search and no complete value would be got. */
        memset(dest + len, 0x20, mysql_col_len - len);
      }
      break;

    default:
#ifdef UNIV_DEBUG
    case DATA_SYS_CHILD:
    case DATA_SYS:
      /* These column types should never be shipped to MySQL. */
      ut_d(ut_error);
      [[fallthrough]];

    case DATA_CHAR:
    case DATA_FIXBINARY:
    case DATA_FLOAT:
    case DATA_DOUBLE:
    case DATA_DECIMAL:
      /* Above are the valid column types for MySQL data. */
#endif /* UNIV_DEBUG */

      /* If sec_field value is present then mapping of
      secondary index records to clustered index template
      happens for end range comparison. So length can
      vary according to secondary index record length. */
      ut_ad((templ->is_virtual && !field) ||
            ((field && field->prefix_len)
                 ? field->prefix_len == len
                 : (clust_templ_for_sec || mysql_col_len == len)));

      memcpy(dest, data, len);
  }
}

// clang-format off
/** Convert a field in the Innobase format to a field in the MySQL format.
@param[out]     mysql_rec       Record in the MySQL format
@param[in,out]  prebuilt        Prebuilt struct
@param[in]      rec             InnoDB record; must be protected by a page
                                latch
@param[in]      rec_index       Index of rec
@param[in]      prebuilt_index  prebuilt->index
@param[in]      offsets         Array returned by rec_get_offsets()
@param[in]      field_no        templ->rec_field_no or
                                templ->clust_rec_field_no or
                                templ->icp_rec_field_no or sec field no if
                                clust_templ_for_sec is true
@param[in]      templ           row template
@param[in]      sec_field_no    Secondary index field no if the secondary index
                                record but the prebuilt template is in
                                clustered index format and used only for end
                                range comparison.
@param[in]      lob_undo        the LOB undo information.
@param[in,out]  blob_heap       If not null then use this heap for BLOBs */
// clang-format on
[[nodiscard]] static bool row_sel_store_mysql_field(
    byte *mysql_rec, row_prebuilt_t *prebuilt, const rec_t *rec,
    const dict_index_t *rec_index, const dict_index_t *prebuilt_index,
    const ulint *offsets, ulint field_no, const mysql_row_templ_t *templ,
    ulint sec_field_no, lob::undo_vers_t *lob_undo, mem_heap_t *&blob_heap) {
  DBUG_TRACE;

  const byte *data;
  ulint len;
  ulint clust_field_no = 0;
  bool clust_templ_for_sec = (sec_field_no != ULINT_UNDEFINED);
  const dict_index_t *index_used =
      (clust_templ_for_sec) ? prebuilt_index : rec_index;

  ut_ad(templ);
  ut_ad(prebuilt->default_rec);
  ut_ad(templ >= prebuilt->mysql_template);
  ut_ad(templ < &prebuilt->mysql_template[prebuilt->n_template]);

  ut_ad(clust_templ_for_sec || field_no == templ->clust_rec_field_no ||
        field_no == templ->rec_field_no || field_no == templ->icp_rec_field_no);

  ut_ad(rec_offs_validate(rec, index_used, offsets));

  /* If sec_field_no is present then extract the data from record
  using secondary field no. */
  if (clust_templ_for_sec) {
    clust_field_no = field_no;
    field_no = sec_field_no;
  }

  if (rec_offs_nth_extern(index_used, offsets, field_no)) {
    /* Copy an externally stored field to a temporary heap */

    ut_a(!prebuilt->trx->has_search_latch);
    ut_ad(field_no == templ->clust_rec_field_no);
    ut_ad(templ->type != DATA_POINT);

    mem_heap_t *heap;

    if (DATA_LARGE_MTYPE(templ->type)) {
      if (blob_heap == nullptr) {
        blob_heap = mem_heap_create(UNIV_PAGE_SIZE, UT_LOCATION_HERE);
      }

      heap = blob_heap;
    } else {
      heap = mem_heap_create(UNIV_PAGE_SIZE, UT_LOCATION_HERE);
    }

    /* NOTE: if we are retrieving a big BLOB, we may
    already run out of memory in the next call, which
    causes an assert */

    dict_index_t *clust_index = rec_index->table->first_index();

    const page_size_t page_size = dict_table_page_size(rec_index->table);

    size_t lob_version = 0;

    data = lob::btr_rec_copy_externally_stored_field(
        prebuilt->trx, clust_index, rec, offsets, page_size, field_no, &len,
        &lob_version, dict_index_is_sdi(rec_index), heap);

    if (data == nullptr) {
      /* The externally stored field was not written
      yet. This record should only be seen by
      trx_rollback_or_clean_all_recovered() or any
      yet. This can happen after optimization which
      was done after for Bug#23481444 where we read
      last record in the page to find the end range
      scan. If we encounter this we just return false
      In any other case this row should be only seen
      by recv_recovery_rollback_active() or any
      TRX_ISO_READ_UNCOMMITTED transactions. */

      if (heap != blob_heap) {
        mem_heap_free(heap);
      }

      ut_a((!prebuilt->idx_cond &&
            prebuilt->m_mysql_handler->end_range != nullptr) ||
           (prebuilt->trx->isolation_level == TRX_ISO_READ_UNCOMMITTED));
      return false;
    }

    if (lob_undo != nullptr) {
      ulint local_len;
      const byte *field_data = rec_get_nth_field_instant(
          rec, offsets, field_no, index_used, &local_len);
      const byte *field_ref =
          field_data + local_len - BTR_EXTERN_FIELD_REF_SIZE;

      lob::ref_t ref(const_cast<byte *>(field_ref));
      lob_undo->apply(clust_index, field_no, const_cast<byte *>(data), len,
                      lob_version, ref.page_no());
    }

    ut_a(rec_field_not_null_not_add_col_def(len));

    row_sel_field_store_in_mysql_format(mysql_rec + templ->mysql_col_offset,
                                        templ, rec_index, field_no, data, len,
                                        ULINT_UNDEFINED);

    if (heap != blob_heap) {
      mem_heap_free(heap);
    }
  } else {
    /* Field is stored in the row. */

    data = rec_get_nth_field_instant(rec, offsets, field_no, index_used, &len);

    if (len == UNIV_SQL_NULL) {
      /* MySQL assumes that the field for an SQL
      NULL value is set to the default value. */
      ut_ad(templ->mysql_null_bit_mask);

      UNIV_MEM_ASSERT_RW(prebuilt->default_rec + templ->mysql_col_offset,
                         templ->mysql_col_len);
      mysql_rec[templ->mysql_null_byte_offset] |=
          (byte)templ->mysql_null_bit_mask;
      memcpy(mysql_rec + templ->mysql_col_offset,
             (const byte *)prebuilt->default_rec + templ->mysql_col_offset,
             templ->mysql_col_len);
      return true;
    }

    if (DATA_LARGE_MTYPE(templ->type) || DATA_GEOMETRY_MTYPE(templ->type)) {
      /* It is a BLOB field locally stored in the
      InnoDB record: we MUST copy its contents to
      prebuilt->blob_heap here because
      row_sel_field_store_in_mysql_format() stores a
      pointer to the data, and the data passed to us
      will be invalid as soon as the
      mini-transaction is committed and the page
      latch on the clustered index page is
      released.
      For DATA_POINT, it's stored like CHAR in InnoDB,
      but it should be a BLOB field in MySQL layer. So we
      still treated it as BLOB here. */

      mem_heap_t *heap{};

      if (blob_heap == nullptr) {
        blob_heap = mem_heap_create(UNIV_PAGE_SIZE, UT_LOCATION_HERE);
      }

      heap = blob_heap;
      data = static_cast<byte *>(mem_heap_dup(heap, data, len));
    }

    /* Reassign the clustered index field no. */
    if (clust_templ_for_sec) {
      field_no = clust_field_no;
    }

    row_sel_field_store_in_mysql_format(mysql_rec + templ->mysql_col_offset,
                                        templ, rec_index, field_no, data, len,
                                        sec_field_no);
  }

  ut_ad(rec_field_not_null_not_add_col_def(len));

  if (templ->mysql_null_bit_mask) {
    /* It is a nullable column with a non-NULL value */
    mysql_rec[templ->mysql_null_byte_offset] &=
        ~(byte)templ->mysql_null_bit_mask;
  }

  return true;
}

bool row_sel_store_mysql_rec(byte *mysql_rec, row_prebuilt_t *prebuilt,
                             const rec_t *rec, const dtuple_t *vrow,
                             bool rec_clust, const dict_index_t *rec_index,
                             const dict_index_t *prebuilt_index,
                             const ulint *offsets, bool clust_templ_for_sec,
                             lob::undo_vers_t *lob_undo,
                             mem_heap_t *&blob_heap) {
  std::vector<const dict_col_t *> template_col;

  DBUG_TRACE;

  ut_ad(rec_clust || rec_index == prebuilt_index);
  ut_ad(!rec_clust || rec_index->is_clustered());

  /* If blob_heap provided by the caller is not that of prebuilt's blob heap
  then the onus would be on the caller to empty the blob heap if required. */
  if (blob_heap != nullptr && blob_heap == prebuilt->blob_heap) {
    mem_heap_empty(blob_heap);
  }

  if (clust_templ_for_sec) {
    /* Store all clustered index column of secondary index record. */
    for (ulint i = 0; i < dict_index_get_n_fields(prebuilt_index); i++) {
      auto sec_field =
          dict_index_get_nth_field_pos(rec_index, prebuilt_index, i);

      if (sec_field == ULINT_UNDEFINED) {
        template_col.push_back(nullptr);
        continue;
      }

      const auto field = rec_index->get_field(sec_field);
      const auto col = field->col;
      template_col.push_back(col);
    }
  }

  for (ulint i = 0; i < prebuilt->n_template; i++) {
    const auto templ = &prebuilt->mysql_template[i];

    /* Skip multi-value columns; since they can not be explicitly
    requested by the query, they may only be here in scenarios
    where all index columns are included routinely, like these:
    1. Index-only scan (done for covering index): all index field
    are stored regardless of whether they are requested or not
    (depending on optimization options): for multi-values they
    need not be stored, as they may never be requested.
    2. Cross-partition index scan, for the purpose of index merge:
    not needed since multi-values do not introduce ordering and
    so are not needed for index merge. */
    if (templ->is_multi_val) {
      /* Multi-value columns are always virtual */
      ut_ad(templ->is_virtual);
      continue;
    }

    if (templ->is_virtual && rec_index->is_clustered()) {
      /* Skip virtual columns if it is not a covered
      search or virtual key read is not requested. */
      if ((prebuilt_index != nullptr &&
           !dict_index_has_virtual(prebuilt_index)) ||
          (!prebuilt->read_just_key && !prebuilt->m_read_virtual_key) ||
          !rec_clust) {
        continue;
      }

      dict_v_col_t *col;
      col =
          dict_table_get_nth_v_col(rec_index->table, templ->clust_rec_field_no);

      ut_ad(vrow);

      const auto dfield = dtuple_get_nth_v_field(vrow, col->v_pos);

      /* If this is a partitioned table, it might request
      InnoDB to fill out virtual column data for search
      index key values while other non key columns are also
      getting selected. The non-key virtual columns may
      not be materialized and we should skip them. */
      if (dfield_get_type(dfield)->mtype == DATA_MISSING) {
        ut_ad(prebuilt->m_read_virtual_key);

        /* If it is part of index key the data should
        have been materialized. */
        ut_ad(prebuilt_index->get_col_pos(col->v_pos, false, true) ==
              ULINT_UNDEFINED);

        continue;
      }

      if (dfield->len == UNIV_SQL_NULL) {
        mysql_rec[templ->mysql_null_byte_offset] |=
            (byte)templ->mysql_null_bit_mask;
        memcpy(mysql_rec + templ->mysql_col_offset,
               (const byte *)prebuilt->default_rec + templ->mysql_col_offset,
               templ->mysql_col_len);
      } else {
        row_sel_field_store_in_mysql_format(
            mysql_rec + templ->mysql_col_offset, templ, rec_index,
            templ->clust_rec_field_no, (const byte *)dfield->data, dfield->len,
            ULINT_UNDEFINED);
        if (templ->mysql_null_bit_mask) {
          mysql_rec[templ->mysql_null_byte_offset] &=
              ~(byte)templ->mysql_null_bit_mask;
        }
      }

      continue;
    }

    ulint field_no =
        rec_clust ? templ->clust_rec_field_no : templ->rec_field_no;

    ulint sec_field_no = ULINT_UNDEFINED;

    /* We should never deliver column prefixes to MySQL,
    except for evaluating innobase_index_cond() or
    row_search_end_range_check(). */
    ut_ad(rec_index->get_field(field_no)->prefix_len == 0);

    if (clust_templ_for_sec) {
      std::vector<const dict_col_t *>::iterator it;
      const dict_field_t *field = rec_index->get_field(field_no);
      const dict_col_t *col = field->col;

      it = std::find(template_col.begin(), template_col.end(), col);

      if (it == template_col.end()) {
        continue;
      }

      ut_ad(templ->rec_field_no == templ->clust_rec_field_no);

      sec_field_no = it - template_col.begin();
    }

    if (!row_sel_store_mysql_field(mysql_rec, prebuilt, rec, rec_index,
                                   prebuilt_index, offsets, field_no, templ,
                                   sec_field_no, lob_undo, blob_heap)) {
      return false;
    }
  }

  /* FIXME: We only need to read the doc_id if an FTS indexed column is being
  updated.
  NOTE, the record can be cluster or secondary index record.
  If secondary index is used then FTS_DOC_ID column should be part of this
  index. */
  if (dict_table_has_fts_index(rec_index->table)) {
    if ((rec_index->is_clustered() && !clust_templ_for_sec) ||
        prebuilt->fts_doc_id_in_read_set) {
      prebuilt->fts_doc_id =
          fts_get_doc_id_from_rec(rec_index->table, rec, rec_index, nullptr);
    }
  }

  return true;
}

/** Builds a previous version of a clustered index record for a consistent read
@param[in]      read_view       read view
@param[in]      clust_index     clustered index
@param[in]      prebuilt        prebuilt struct
@param[in]      rec             record in clustered index
@param[in,out]  offsets         offsets returned by
                                rec_get_offsets(rec, clust_index)
@param[in,out]  offset_heap     memory heap from which the offsets are
                                allocated
@param[out]     old_vers        old version, or NULL if the record does not
                                exist in the view: i.e., it was freshly
                                inserted afterwards
@param[out]     vrow            dtuple to hold old virtual column data
@param[in]      mtr             the mini-transaction context.
@param[in,out]  lob_undo        Undo information for BLOBs.
@return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_sel_build_prev_vers_for_mysql(
    ReadView *read_view, dict_index_t *clust_index, row_prebuilt_t *prebuilt,
    const rec_t *rec, ulint **offsets, mem_heap_t **offset_heap,
    rec_t **old_vers, const dtuple_t **vrow, mtr_t *mtr,
    lob::undo_vers_t *lob_undo) {
  DBUG_TRACE;

  dberr_t err;

  if (prebuilt->old_vers_heap) {
    mem_heap_empty(prebuilt->old_vers_heap);
  } else {
    prebuilt->old_vers_heap = mem_heap_create(200, UT_LOCATION_HERE);
  }

  err = row_vers_build_for_consistent_read(
      rec, mtr, clust_index, offsets, read_view, offset_heap,
      prebuilt->old_vers_heap, old_vers, vrow, lob_undo);

  return err;
}

/** Helper class to cache clust_rec and old_ver */
class Row_sel_get_clust_rec_for_mysql {
  const rec_t *cached_clust_rec;
  rec_t *cached_old_vers;

 public:
  /** Constructor */
  Row_sel_get_clust_rec_for_mysql()
      : cached_clust_rec(nullptr), cached_old_vers(nullptr) {}

  /** Retrieve the clustered index record corresponding to a record in a
  non-clustered index. Does the necessary locking.
  @param[in]     prebuilt    prebuilt struct in the handle
  @param[in]     sec_index   secondary index where rec resides
  @param[in]     rec         record in a non-clustered index
  @param[in]     thr         query thread
  @param[out]    out_rec     clustered record or an old version of it,
                             NULL if the old version did not exist in the
                             read view, i.e., it was a fresh inserted version
  @param[in,out] offsets     in: offsets returned by
                                 rec_get_offsets(rec, sec_index);
                             out: offsets returned by
                                 rec_get_offsets(out_rec, clust_index)
  @param[in,out] offset_heap memory heap from which the offsets are allocated
  @param[out]    vrow        virtual column to fill
  @param[in]     mtr         mtr used to get access to the non-clustered record;
                             the same mtr is used to access the clustered index
  @param[in]     lob_undo    the LOB undo information.
  @return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, or error code */
  dberr_t operator()(row_prebuilt_t *prebuilt, dict_index_t *sec_index,
                     const rec_t *rec, que_thr_t *thr, const rec_t **out_rec,
                     ulint **offsets, mem_heap_t **offset_heap,
                     const dtuple_t **vrow, mtr_t *mtr,
                     lob::undo_vers_t *lob_undo);
};

/** Retrieve the clustered index record corresponding to a record in a
non-clustered index. Does the necessary locking.
  @return DB_SUCCESS, DB_SUCCESS_LOCKED_REC, or error code */

[[nodiscard]] dberr_t Row_sel_get_clust_rec_for_mysql::operator()(
    row_prebuilt_t *prebuilt, dict_index_t *sec_index, const rec_t *rec,
    que_thr_t *thr, const rec_t **out_rec, ulint **offsets,
    mem_heap_t **offset_heap, const dtuple_t **vrow, mtr_t *mtr,
    lob::undo_vers_t *lob_undo) {
  DBUG_TRACE;

  dict_index_t *clust_index;
  const rec_t *clust_rec;
  rec_t *old_vers;
  dberr_t err;
  trx_t *trx;

  *out_rec = nullptr;
  trx = thr_get_trx(thr);

  row_build_row_ref_in_tuple(prebuilt->clust_ref, rec, sec_index, *offsets);

  clust_index = sec_index->table->first_index();

  prebuilt->clust_pcur->open_no_init(clust_index, prebuilt->clust_ref,
                                     PAGE_CUR_LE, BTR_SEARCH_LEAF, 0, mtr,
                                     UT_LOCATION_HERE);

  clust_rec = prebuilt->clust_pcur->get_rec();

  prebuilt->clust_pcur->m_trx_if_known = trx;

  /* Note: only if the search ends up on a non-infimum record is the
  low_match value the real match to the search tuple */

  if (!page_rec_is_user_rec(clust_rec) ||
      prebuilt->clust_pcur->get_low_match() <
          dict_index_get_n_unique(clust_index)) {
    btr_cur_t *btr_cur = prebuilt->pcur->get_btr_cur();

    /* If this is a spatial index scan, and we are reading
    from a shadow buffer, the record could be already
    deleted (due to rollback etc.). So get the original
    page and verify that */
    if (dict_index_is_spatial(sec_index) && btr_cur->rtr_info->matches &&
        (page_align(rec) == btr_cur->rtr_info->matches->block.frame ||
         rec != prebuilt->pcur->get_rec())) {
#ifdef UNIV_DEBUG
      rtr_info_t *rtr_info = btr_cur->rtr_info;
      mutex_enter(&rtr_info->matches->rtr_match_mutex);
      /* The page could be deallocated (by rollback etc.) */
      if (!rtr_info->matches->valid) {
        mutex_exit(&rtr_info->matches->rtr_match_mutex);
        clust_rec = nullptr;

        err = DB_SUCCESS;
        goto func_exit;
      }
      mutex_exit(&rtr_info->matches->rtr_match_mutex);

      if (rec_get_deleted_flag(rec, dict_table_is_comp(sec_index->table)) &&
          prebuilt->select_lock_type == LOCK_NONE) {
        clust_rec = nullptr;

        err = DB_SUCCESS;
        goto func_exit;
      }

      if (rec != prebuilt->pcur->get_rec()) {
        clust_rec = nullptr;

        err = DB_SUCCESS;
        goto func_exit;
      }

      page_no_t page_no = page_get_page_no(prebuilt->pcur->get_page());

      page_id_t page_id(dict_index_get_space(sec_index), page_no);

      buf_block_t *block = buf_page_get_gen(
          page_id, dict_table_page_size(sec_index->table), RW_NO_LATCH, nullptr,
          Page_fetch::NORMAL, UT_LOCATION_HERE, mtr);

      mem_heap_t *heap = mem_heap_create(256, UT_LOCATION_HERE);
      dtuple_t *tuple =
          dict_index_build_data_tuple(sec_index, const_cast<rec_t *>(rec),
                                      dict_index_get_n_fields(sec_index), heap);
      ;
      page_cur_t page_cursor;

      ulint low_match =
          page_cur_search(block, sec_index, tuple, PAGE_CUR_LE, &page_cursor);

      ut_ad(low_match < dtuple_get_n_fields_cmp(tuple));
      mem_heap_free(heap);
      clust_rec = nullptr;

      err = DB_SUCCESS;
      goto func_exit;
#endif /* UNIV_DEBUG */
    } else if (!rec_get_deleted_flag(rec,
                                     dict_table_is_comp(sec_index->table)) ||
               prebuilt->select_lock_type != LOCK_NONE) {
      /* In a rare case it is possible that no clust
      rec is found for a delete-marked secondary index
      record: if in row0umod.cc in
      row_undo_mod_remove_clust_low() we have already removed
      the clust rec, while purge is still cleaning and
      removing secondary index records associated with
      earlier versions of the clustered index record.
      In that case we know that the clustered index
      record did not exist in the read view of trx. */
      ib::error(ER_IB_MSG_1030)
          << "Clustered record for sec rec not found"
             " index "
          << sec_index->name << " of table " << sec_index->table->name;

      fputs("InnoDB: sec index record ", stderr);
      rec_print(stderr, rec, sec_index);
      fputs(
          "\n"
          "InnoDB: clust index record ",
          stderr);
      rec_print(stderr, clust_rec, clust_index);
      putc('\n', stderr);
      trx_print(stderr, trx, 600);
      fputs(
          "\n"
          "InnoDB: Submit a detailed bug report"
          " to http://bugs.mysql.com\n",
          stderr);
      ut_d(ut_error);
    }

    clust_rec = nullptr;

    err = DB_SUCCESS;
    goto func_exit;
  }

  *offsets = rec_get_offsets(clust_rec, clust_index, *offsets, ULINT_UNDEFINED,
                             UT_LOCATION_HERE, offset_heap);

  if (prebuilt->select_lock_type != LOCK_NONE) {
    /* Try to place a lock on the index record; we are searching
    the clust rec with a unique condition, hence
    we set a LOCK_REC_NOT_GAP type lock */

    err = lock_clust_rec_read_check_and_lock(
        lock_duration_t::REGULAR, prebuilt->clust_pcur->get_block(), clust_rec,
        clust_index, *offsets, prebuilt->select_mode,
        static_cast<lock_mode>(prebuilt->select_lock_type), LOCK_REC_NOT_GAP,
        thr);

    switch (err) {
      case DB_SUCCESS:
      case DB_SUCCESS_LOCKED_REC:
        break;
      default:
        goto err_exit;
    }
  } else {
    /* This is a non-locking consistent read: if necessary, fetch
    a previous version of the record */

    old_vers = nullptr;

    /* If the isolation level allows reading of uncommitted data,
    then we never look for an earlier version */

    if (trx->isolation_level > TRX_ISO_READ_UNCOMMITTED &&
        !lock_clust_rec_cons_read_sees(clust_rec, clust_index, *offsets,
                                       trx_get_read_view(trx))) {
      if (clust_rec != cached_clust_rec) {
        /* The following call returns 'offsets' associated with 'old_vers' */
        err = row_sel_build_prev_vers_for_mysql(
            trx->read_view, clust_index, prebuilt, clust_rec, offsets,
            offset_heap, &old_vers, vrow, mtr, lob_undo);

        if (err != DB_SUCCESS) {
          goto err_exit;
        }
        cached_clust_rec = clust_rec;
        cached_old_vers = old_vers;
      } else {
        err = DB_SUCCESS;
        old_vers = cached_old_vers;

        if (old_vers != nullptr) {
          DBUG_EXECUTE_IF("innodb_cached_old_vers_offsets", {
            rec_offs_make_valid(old_vers, clust_index, *offsets);
            if (!lob::rec_check_lobref_space_id(clust_index, old_vers,
                                                *offsets)) {
              DBUG_SUICIDE();
            }
          });

          /* The offsets need not be same for the latest version of
          clust_rec and its old version old_vers.  Re-calculate the offsets
          for old_vers. */
          *offsets =
              rec_get_offsets(old_vers, clust_index, *offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, offset_heap);
          ut_ad(
              lob::rec_check_lobref_space_id(clust_index, old_vers, *offsets));
        }
      }

      if (old_vers == nullptr) {
        goto err_exit;
      }

      clust_rec = old_vers;
    }

    /* If we had to go to an earlier version of row or the
    secondary index record is delete marked, then it may be that
    the secondary index record corresponding to clust_rec
    (or old_vers) is not rec; in that case we must ignore
    such row because in our snapshot rec would not have existed.
    Remember that from rec we cannot see directly which transaction
    id corresponds to it: we have to go to the clustered index
    record. A query where we want to fetch all rows where
    the secondary index value is in some interval would return
    a wrong result if we would not drop rows which we come to
    visit through secondary index records that would not really
    exist in our snapshot. */

    /* And for spatial index, since the rec is from shadow buffer,
    so we need to check if it's exactly match the clust_rec. */
    if (clust_rec &&
        (old_vers || trx->isolation_level <= TRX_ISO_READ_UNCOMMITTED ||
         dict_index_is_spatial(sec_index) ||
         rec_get_deleted_flag(rec, dict_table_is_comp(sec_index->table)))) {
      bool rec_equal;

      err = row_sel_sec_rec_is_for_clust_rec(rec, sec_index, clust_rec,
                                             clust_index, thr, rec_equal);
      if (err != DB_SUCCESS) {
        goto err_exit;
      } else if (!rec_equal) {
        clust_rec = nullptr;
      }
    }

    err = DB_SUCCESS;
  }

func_exit:
  *out_rec = clust_rec;

  /* Store the current position if select_lock_type is not
  LOCK_NONE or if we are scanning using InnoDB APIs */
  if (prebuilt->select_lock_type != LOCK_NONE || prebuilt->innodb_api) {
    /* We may use the cursor in update or in unlock_row():
    store its position */

    prebuilt->clust_pcur->store_position(mtr);
  }

err_exit:
  return err;
}

/** Restores cursor position after it has been stored. We have to take into
 account that the record cursor was positioned on may have been deleted.
 Then we may have to move the cursor one step up or down.
 @return true if we may need to process the record the cursor is now
 positioned on (i.e. we should not go to the next record yet) */
static bool sel_restore_position_for_mysql(
    bool *same_user_rec, /*!< out: true if we were able to restore
                          the cursor on a user record with the
                          same ordering prefix in in the
                          B-tree index */
    ulint latch_mode,    /*!< in: latch mode wished in
                         restoration */
    btr_pcur_t *pcur,    /*!< in: cursor whose position
                         has been stored */
    bool moves_up,       /*!< in: true if the cursor moves up
                          in the index */
    mtr_t *mtr)          /*!< in: mtr; CAUTION: may commit
                         mtr temporarily! */
{
  auto success = pcur->restore_position(latch_mode, mtr, UT_LOCATION_HERE);

  *same_user_rec = success;

  ut_ad(!success || pcur->m_rel_pos == BTR_PCUR_ON);
#ifdef UNIV_DEBUG
  if (pcur->m_pos_state == BTR_PCUR_IS_POSITIONED_OPTIMISTIC) {
    ut_ad(pcur->m_rel_pos == BTR_PCUR_BEFORE ||
          pcur->m_rel_pos == BTR_PCUR_AFTER);
  } else {
    ut_ad(pcur->m_pos_state == BTR_PCUR_IS_POSITIONED);
    ut_ad((pcur->m_rel_pos == BTR_PCUR_ON) == pcur->is_on_user_rec());
  }
#endif /* UNIV_DEBUG */

  /* The position may need be adjusted for rel_pos and moves_up. */

  switch (pcur->m_rel_pos) {
    case BTR_PCUR_UNSET:
      ut_d(ut_error);
      ut_o(return (true));
    case BTR_PCUR_ON:
      if (!success && moves_up) {
      next:
        pcur->move_to_next(mtr);
        return true;
      }
      return (!success);
    case BTR_PCUR_AFTER_LAST_IN_TREE:
    case BTR_PCUR_BEFORE_FIRST_IN_TREE:
      return true;
    case BTR_PCUR_AFTER:
      /* positioned to record after pcur->old_rec. */
      pcur->m_pos_state = BTR_PCUR_IS_POSITIONED;
    prev:
      if (pcur->is_on_user_rec() && !moves_up) {
        pcur->move_to_prev(mtr);
      }
      return true;
    case BTR_PCUR_BEFORE:
      /* For non optimistic restoration:
      The position is now set to the record before pcur->old_rec.

      For optimistic restoration:
      The position also needs to take the previous search_mode into
      consideration. */

      switch (pcur->m_pos_state) {
        case BTR_PCUR_IS_POSITIONED_OPTIMISTIC:
          pcur->m_pos_state = BTR_PCUR_IS_POSITIONED;
          if (pcur->m_search_mode == PAGE_CUR_GE) {
            /* Positioned during Greater or Equal search
            with BTR_PCUR_BEFORE. Optimistic restore to
            the same record. If scanning for lower then
            we must move to previous record.
            This can happen with:
            HANDLER READ idx a = (const);
            HANDLER READ idx PREV; */
            goto prev;
          }
          return true;
        case BTR_PCUR_IS_POSITIONED:
          if (moves_up && pcur->is_on_user_rec()) {
            goto next;
          }
          return true;
        case BTR_PCUR_WAS_POSITIONED:
        case BTR_PCUR_NOT_POSITIONED:
          break;
      }
  }
  ut_d(ut_error);
  ut_o(return (true));
}

/** Copies a cached field for MySQL from the fetch cache. */
static void row_sel_copy_cached_field_for_mysql(
    byte *buf,                      /*!< in/out: row buffer */
    const byte *cache,              /*!< in: cached row */
    const mysql_row_templ_t *templ) /*!< in: column template */
{
  ut_a(!templ->is_multi_val);

  ulint len;

  buf += templ->mysql_col_offset;
  cache += templ->mysql_col_offset;

  UNIV_MEM_ASSERT_W(buf, templ->mysql_col_len);

  if (templ->mysql_type == DATA_MYSQL_TRUE_VARCHAR &&
      (templ->type != DATA_INT)) {
    /* Check for != DATA_INT to make sure we do
    not treat MySQL ENUM or SET as a true VARCHAR!
    Find the actual length of the true VARCHAR field. */
    row_mysql_read_true_varchar(&len, cache, templ->mysql_length_bytes);
    len += templ->mysql_length_bytes;
    UNIV_MEM_INVALID(buf, templ->mysql_col_len);
  } else {
    len = templ->mysql_col_len;
  }

  /* The buf and cache have each reserved exactly templ->mysql_col_len
  bytes for this column. In case of varchar we might copy fewer. */
  ut_a(len <= templ->mysql_col_len);
  ut_memcpy(buf, cache, len);
}

/** Copy used fields from cached row.
Copy cache record field by field, don't touch fields that
are not covered by current key.
@param[out]     buf             Where to copy the MySQL row.
@param[in]      cached_rec      What to copy (in MySQL row format).
@param[in]      prebuilt        prebuilt struct. */
void row_sel_copy_cached_fields_for_mysql(byte *buf, const byte *cached_rec,
                                          row_prebuilt_t *prebuilt) {
  const mysql_row_templ_t *templ;
  ulint i;
  for (i = 0; i < prebuilt->n_template; i++) {
    templ = prebuilt->mysql_template + i;

    /* Skip virtual columns */
    if (templ->is_virtual) {
      continue;
    }

    row_sel_copy_cached_field_for_mysql(buf, cached_rec, templ);
    /* Copy NULL bit of the current field from cached_rec
    to buf */
    if (templ->mysql_null_bit_mask) {
      buf[templ->mysql_null_byte_offset] ^=
          (buf[templ->mysql_null_byte_offset] ^
           cached_rec[templ->mysql_null_byte_offset]) &
          (byte)templ->mysql_null_bit_mask;
    }
  }
}

/** Get the record buffer provided by the server, if there is one.
@param  prebuilt        prebuilt struct
@return the record buffer, or nullptr if none was provided */
static Record_buffer *row_sel_get_record_buffer(
    const row_prebuilt_t *prebuilt) {
  if (prebuilt->m_mysql_handler == nullptr) {
    return nullptr;
  }
  return prebuilt->m_mysql_handler->ha_get_record_buffer();
}

/** Pops a cached row for MySQL from the fetch cache. */
static inline void row_sel_dequeue_cached_row_for_mysql(
    byte *buf,                /*!< in/out: buffer where to copy the
                              row */
    row_prebuilt_t *prebuilt) /*!< in: prebuilt struct */
{
  ulint i;
  const mysql_row_templ_t *templ;
  const byte *cached_rec;
  ut_ad(prebuilt->n_fetch_cached > 0);
  ut_ad(prebuilt->mysql_prefix_len <= prebuilt->mysql_row_len);

  UNIV_MEM_ASSERT_W(buf, prebuilt->mysql_row_len);

  /* The row is cached in the server-provided buffer, if there
  is one. If not, get it from our own prefetch cache.*/
  const auto record_buffer = row_sel_get_record_buffer(prebuilt);
  cached_rec = record_buffer
                   ? record_buffer->record(prebuilt->fetch_cache_first)
                   : prebuilt->fetch_cache[prebuilt->fetch_cache_first];

  if (UNIV_UNLIKELY(prebuilt->keep_other_fields_on_keyread)) {
    row_sel_copy_cached_fields_for_mysql(buf, cached_rec, prebuilt);
  } else if (prebuilt->mysql_prefix_len > 63) {
    /* The record is long. Copy it field by field, in case
    there are some long VARCHAR column of which only a
    small length is being used. */
    UNIV_MEM_INVALID(buf, prebuilt->mysql_prefix_len);

    /* First copy the NULL bits. */
    ut_memcpy(buf, cached_rec, prebuilt->null_bitmap_len);
    /* Then copy the requested fields. */

    for (i = 0; i < prebuilt->n_template; i++) {
      templ = prebuilt->mysql_template + i;

      /* Skip virtual columns */
      if (templ->is_virtual) {
        if (!(dict_index_has_virtual(prebuilt->index) &&
              prebuilt->read_just_key)) {
          continue;
        }

        if (templ->is_multi_val) {
          continue;
        }
      }
      // Multi-value columns are always virtual
      ut_a(!templ->is_multi_val);

      row_sel_copy_cached_field_for_mysql(buf, cached_rec, templ);
    }
  } else {
    ut_memcpy(buf, cached_rec, prebuilt->mysql_prefix_len);
  }

  prebuilt->n_fetch_cached--;
  prebuilt->fetch_cache_first++;

  if (prebuilt->n_fetch_cached == 0) {
    /* All the prefetched records have been returned.
    Rewind so that we can insert records at the beginning
    of the prefetch cache or record buffer. */
    prebuilt->fetch_cache_first = 0;
    if (record_buffer != nullptr) {
      record_buffer->clear();
    }
  }
}

/** Initialise the prefetch cache. */
static inline void row_sel_prefetch_cache_init(
    row_prebuilt_t *prebuilt) /*!< in/out: prebuilt struct */
{
  ulint i;
  ulint sz;
  byte *ptr;

  /* We use our own prefetch cache only if the server didn't
  provide one. */
  ut_ad(row_sel_get_record_buffer(prebuilt) == nullptr);

  /* Reserve space for the magic number. */
  sz = UT_ARR_SIZE(prebuilt->fetch_cache) * (prebuilt->mysql_row_len + 8);
  ptr = static_cast<byte *>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sz));

  for (i = 0; i < UT_ARR_SIZE(prebuilt->fetch_cache); i++) {
    /* A user has reported memory corruption in these
    buffers in Linux. Put magic numbers there to help
    to track a possible bug. */

    mach_write_to_4(ptr, ROW_PREBUILT_FETCH_MAGIC_N);
    ptr += 4;

    prebuilt->fetch_cache[i] = ptr;
    ptr += prebuilt->mysql_row_len;

    mach_write_to_4(ptr, ROW_PREBUILT_FETCH_MAGIC_N);
    ptr += 4;
  }
}

/** Get the last fetch cache buffer from the queue.
 @return pointer to buffer. */
static inline byte *row_sel_fetch_last_buf(
    row_prebuilt_t *prebuilt) /*!< in/out: prebuilt struct */
{
  const auto record_buffer = row_sel_get_record_buffer(prebuilt);

  ut_ad(!prebuilt->templ_contains_blob);
  if (record_buffer == nullptr) {
    ut_ad(prebuilt->n_fetch_cached < MYSQL_FETCH_CACHE_SIZE);
  } else {
    ut_ad(prebuilt->mysql_prefix_len <= record_buffer->record_size());
    ut_ad(record_buffer->records() == prebuilt->n_fetch_cached);
  }

  if (record_buffer == nullptr && prebuilt->fetch_cache[0] == nullptr) {
    /* Allocate memory for the fetch cache */
    ut_ad(prebuilt->n_fetch_cached == 0);

    row_sel_prefetch_cache_init(prebuilt);
  }

  /* Use the server-provided buffer if there is one. Otherwise,
  use our own prefetch buffer. */
  byte *buf = record_buffer ? record_buffer->add_record()
                            : prebuilt->fetch_cache[prebuilt->n_fetch_cached];

  ut_ad(prebuilt->fetch_cache_first == 0);
  UNIV_MEM_INVALID(buf, record_buffer ? record_buffer->record_size()
                                      : prebuilt->mysql_row_len);

  return (buf);
}

/** Pushes a row for MySQL to the fetch cache. */
static inline void row_sel_enqueue_cache_row_for_mysql(
    byte *mysql_rec,          /*!< in/out: MySQL record */
    row_prebuilt_t *prebuilt) /*!< in/out: prebuilt struct */
{
  /* For non ICP code path the row should already exist in the
  next fetch cache slot. */

  if (prebuilt->idx_cond) {
    byte *dest = row_sel_fetch_last_buf(prebuilt);

    ut_memcpy(dest, mysql_rec, prebuilt->mysql_prefix_len);
  }

  ++prebuilt->n_fetch_cached;
}

/** Tries to do a shortcut to fetch a clustered index record with a unique key,
 using the hash index if possible (not always). We assume that the search
 mode is PAGE_CUR_GE, it is a consistent read, there is a read view in trx,
 btr search latch has been locked in S-mode if AHI is enabled.
 @return SEL_FOUND, SEL_EXHAUSTED, SEL_RETRY */
static ulint row_sel_try_search_shortcut_for_mysql(
    const rec_t **out_rec,    /*!< out: record if found */
    row_prebuilt_t *prebuilt, /*!< in: prebuilt struct */
    ulint **offsets,          /*!< in/out: for rec_get_offsets(*out_rec) */
    mem_heap_t **heap,        /*!< in/out: heap for rec_get_offsets() */
    mtr_t *mtr)               /*!< in: started mtr */
{
  dict_index_t *index = prebuilt->index;
  const dtuple_t *search_tuple = prebuilt->search_tuple;
  btr_pcur_t *pcur = prebuilt->pcur;
  trx_t *trx = prebuilt->trx;
  const rec_t *rec;

  ut_ad(index->is_clustered());
  ut_ad(!prebuilt->templ_contains_blob);

  pcur->open_no_init(index, search_tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF,
                     (trx->has_search_latch) ? RW_S_LATCH : 0, mtr,
                     UT_LOCATION_HERE);
  rec = pcur->get_rec();

  if (!page_rec_is_user_rec(rec)) {
    return (SEL_RETRY);
  }

  /* As the cursor is now placed on a user record after a search with
  the mode PAGE_CUR_GE, the up_match field in the cursor tells how many
  fields in the user record matched to the search tuple */

  if (pcur->get_up_match() < dtuple_get_n_fields(search_tuple)) {
    return (SEL_EXHAUSTED);
  }

  /* This is a non-locking consistent read: if necessary, fetch
  a previous version of the record */

  *offsets = rec_get_offsets(rec, index, *offsets, ULINT_UNDEFINED,
                             UT_LOCATION_HERE, heap);

  if (!lock_clust_rec_cons_read_sees(rec, index, *offsets,
                                     trx_get_read_view(trx))) {
    return (SEL_RETRY);
  }

  if (rec_get_deleted_flag(rec, dict_table_is_comp(index->table))) {
    return (SEL_EXHAUSTED);
  }

  *out_rec = rec;

  return (SEL_FOUND);
}

/** Check a pushed-down index condition.
 @return ICP_NO_MATCH, ICP_MATCH, or ICP_OUT_OF_RANGE */
static ICP_RESULT row_search_idx_cond_check(
    byte *mysql_rec,          /*!< out: record
                              in MySQL format (invalid unless
                              prebuilt->idx_cond == true and
                              we return ICP_MATCH) */
    row_prebuilt_t *prebuilt, /*!< in/out: prebuilt struct
                              for the table handle */
    const rec_t *rec,         /*!< in: InnoDB record */
    const ulint *offsets)     /*!< in: rec_get_offsets() */
{
  ICP_RESULT result;
  ulint i;

  ut_ad(rec_offs_validate(rec, prebuilt->index, offsets));

  if (!prebuilt->idx_cond) {
    return (ICP_MATCH);
  }

  MONITOR_INC(MONITOR_ICP_ATTEMPTS);

  /* Convert to MySQL format those fields that are needed for
  evaluating the index condition. */

  if (prebuilt->blob_heap != nullptr) {
    mem_heap_empty(prebuilt->blob_heap);
  }

  for (i = 0; i < prebuilt->idx_cond_n_cols; i++) {
    const mysql_row_templ_t *templ = &prebuilt->mysql_template[i];

    /* Skip virtual columns */
    if (templ->is_virtual) {
      continue;
    }

    if (!row_sel_store_mysql_field(
            mysql_rec, prebuilt, rec, prebuilt->index, prebuilt->index, offsets,
            templ->icp_rec_field_no, templ, ULINT_UNDEFINED, nullptr,
            prebuilt->blob_heap)) {
      return (ICP_NO_MATCH);
    }
  }

  /* We assume that the index conditions on
  case-insensitive columns are case-insensitive. The
  case of such columns may be wrong in a secondary
  index, if the case of the column has been updated in
  the past, or a record has been deleted and a record
  inserted in a different case. */
  result = innobase_index_cond(prebuilt->m_mysql_handler);
  switch (result) {
    case ICP_MATCH:
      /* Convert the remaining fields to MySQL format.
      If this is a secondary index record, we must defer
      this until we have fetched the clustered index record. */
      if (!prebuilt->need_to_access_clustered ||
          prebuilt->index->is_clustered()) {
        if (!row_sel_store_mysql_rec(mysql_rec, prebuilt, rec, nullptr, false,
                                     prebuilt->index, prebuilt->index, offsets,
                                     false, nullptr, prebuilt->blob_heap)) {
          ut_ad(prebuilt->index->is_clustered());
          return (ICP_NO_MATCH);
        }
      }
      MONITOR_INC(MONITOR_ICP_MATCH);
      return (result);
    case ICP_NO_MATCH:
      MONITOR_INC(MONITOR_ICP_NO_MATCH);
      return (result);
    case ICP_OUT_OF_RANGE:
      MONITOR_INC(MONITOR_ICP_OUT_OF_RANGE);
      const auto record_buffer = row_sel_get_record_buffer(prebuilt);
      if (record_buffer) {
        record_buffer->set_out_of_range(true);
      }
      return (result);
  }

  ut_error;
}

/** Check the pushed-down end-range condition to avoid extra traversal
if records are not with in view and also to avoid prefetching too
many records into the record buffer.
@param[in]      mysql_rec               record in MySQL format
@param[in]      rec                     InnoDB record
@param[in]      prebuilt                prebuilt struct
@param[in]      clust_templ_for_sec     true if \a rec belongs to the secondary
                                        index but the \a prebuilt template is in
                                        clustered index format
@param[in]      offsets                 information about column offsets in the
                                        secondary index, if virtual columns need
                                        to be copied into \a mysql_rec
@param[in,out]  record_buffer           the record buffer we are reading into,
                                        or \c nullptr if there is no buffer
@retval true    if the row in \a mysql_rec is out of range
@retval false   if the row in \a mysql_rec is in range */
static bool row_search_end_range_check(byte *mysql_rec, const rec_t *rec,
                                       row_prebuilt_t *prebuilt,
                                       bool clust_templ_for_sec,
                                       const ulint *offsets,
                                       Record_buffer *record_buffer) {
  const auto handler = prebuilt->m_mysql_handler;
  ut_ad(handler->end_range != nullptr);

  /* When reading from non-covering secondary indexes, mysql_rec won't
  have the values of virtual columns until the handler has called
  update_generated_read_fields(). If the end-range condition refers to a
  virtual column, we may have to copy its value from the secondary index
  before evaluating the condition. */
  if (clust_templ_for_sec && handler->m_virt_gcol_in_end_range) {
    ut_ad(offsets != nullptr);
    for (ulint i = 0; i < prebuilt->n_template; ++i) {
      const auto &templ = prebuilt->mysql_template[i];

      if (templ.is_virtual && templ.icp_rec_field_no != ULINT_UNDEFINED) {
        ut_a(!templ.is_multi_val);
        bool stored = row_sel_store_mysql_field(
            mysql_rec, prebuilt, rec, prebuilt->index, prebuilt->index, offsets,
            templ.icp_rec_field_no, &templ, ULINT_UNDEFINED, nullptr,
            prebuilt->blob_heap);
        /* The only reason row_sel_store_mysql_field might return false
        is when it encounters an externally stored value (blob). However
        such values can't be fields of secondary indexes. */
        ut_ad(stored);
        ut_o(if (!stored) return false);
      }
    }
  }

  if (handler->compare_key_in_buffer(mysql_rec) > 0) {
    if (record_buffer != nullptr) {
      record_buffer->set_out_of_range(true);
    }

    return true;
  }

  return false;
}

/** Traverse to next/previous record.
@param[in]      moves_up        If true, move to next record else previous
@param[in]      match_mode      0 or ROW_SEL_EXACT or ROW_SEL_EXACT_PREFIX
@param[in,out]  pcur            Cursor to record
@param[in]      mtr             Mini-transaction

@return DB_SUCCESS or error code */
static dberr_t row_search_traverse(bool moves_up, ulint match_mode,
                                   btr_pcur_t *pcur, mtr_t *mtr) {
  dberr_t err = DB_SUCCESS;

  if (moves_up) {
    if (!pcur->move_to_next(mtr)) {
      err = (match_mode != 0) ? DB_RECORD_NOT_FOUND : DB_END_OF_INDEX;
      return (err);
    }
  } else {
    if (!pcur->move_to_prev(mtr)) {
      err = (match_mode != 0) ? DB_RECORD_NOT_FOUND : DB_END_OF_INDEX;
      return (err);
    }
  }

  return (err);
}

/** Searches for rows in the database using cursor.
Function is for temporary tables that are not shared across connections
and so lot of complexity is reduced especially locking and transaction related.
The cursor is an iterator over the table/index.

@param[out]     buf             buffer for the fetched row in MySQL format
@param[in]      mode            search mode PAGE_CUR_L
@param[in,out]  prebuilt        prebuilt struct for the table handler;
                                this contains the info to search_tuple,
                                index; if search tuple contains 0 field then
                                we position the cursor at start or the end of
                                index, depending on 'mode'
@param[in]      match_mode      0 or ROW_SEL_EXACT or ROW_SEL_EXACT_PREFIX
@param[in]      direction       0 or ROW_SEL_NEXT or ROW_SEL_PREV;
                                Note: if this is != 0, then prebuilt must has a
                                pcur with stored position! In opening of a
                                cursor 'direction' should be 0.
@return DB_SUCCESS or error code */
dberr_t row_search_no_mvcc(byte *buf, page_cur_mode_t mode,
                           row_prebuilt_t *prebuilt, ulint match_mode,
                           ulint direction) {
  dict_index_t *index = prebuilt->index;
  ut_ad(index->table->is_intrinsic());

  const dtuple_t *search_tuple = prebuilt->search_tuple;
  btr_pcur_t *pcur = prebuilt->pcur;
  Row_sel_get_clust_rec_for_mysql row_sel_get_clust_rec_for_mysql;

  const rec_t *result_rec = nullptr;
  const rec_t *clust_rec = nullptr;

  dberr_t err = DB_SUCCESS;

  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);
  ut_ad(index && pcur && search_tuple);

  /* Step-0: Re-use the cached mtr. */
  mtr_t *mtr;
  dict_index_t *clust_index = index->table->first_index();

  if (!index->last_sel_cur) {
    dict_allocate_mem_intrinsic_cache(index);
  }

  mtr = &index->last_sel_cur->mtr;

  /* Step-1: Build the select graph. */
  if (direction == 0 && prebuilt->sel_graph == nullptr) {
    row_prebuild_sel_graph(prebuilt);
  }

  que_thr_t *thr = que_fork_get_first_thr(prebuilt->sel_graph);

  bool moves_up;

  if (direction == 0) {
    if (mode == PAGE_CUR_GE || mode == PAGE_CUR_G) {
      moves_up = true;
    } else {
      moves_up = false;
    }

  } else if (direction == ROW_SEL_NEXT) {
    moves_up = true;
  } else {
    moves_up = false;
  }

  /* Step-2: Open or Restore the cursor.
  If search key is specified, cursor is open using the key else
  cursor is open to return all the records. */
  if (direction != 0) {
    if (prebuilt->m_temp_read_shared && !prebuilt->m_temp_tree_modified) {
      if (!mtr->is_active()) {
        mtr_start(mtr);

        if (!pcur->m_block_when_stored.run_with_hint([&](buf_block_t *hint) {
              return hint != nullptr &&
                     buf_page_optimistic_get(
                         RW_NO_LATCH, hint, pcur->m_modify_clock,
                         Page_fetch::NORMAL, __FILE__, __LINE__, mtr);
            })) {
          /* block was relocated */
          goto block_relocated;
        }
      }

      /* This is an intrinsic table shared read, so we
      do not rely on index->last_sel_cur, instead we rely
      on "prebuilt->pcur", which supposes to position on
      last read position for each read session. */
      ut_ad(pcur->m_pos_state == BTR_PCUR_IS_POSITIONED);
      err = row_search_traverse(moves_up, match_mode, pcur, mtr);

      if (err != DB_SUCCESS) {
        return (err); /* purecov: inspected */
      }

    } else if (index->last_sel_cur->invalid || prebuilt->m_temp_tree_modified) {
    block_relocated:
      /* Index tree has changed and so active cached cursor is no more valid.
      Re-set it based on the last selected position. */
      index->last_sel_cur->release();
      prebuilt->m_temp_tree_modified = false;

      if (direction == ROW_SEL_NEXT && pcur->m_search_mode == PAGE_CUR_GE) {
        pcur->m_search_mode = PAGE_CUR_G;
      }

      mtr_start(mtr);
      mtr_set_log_mode(mtr, MTR_LOG_NO_REDO);

      mem_heap_t *heap = mem_heap_create(256, UT_LOCATION_HERE);

      dtuple_t *tuple = dict_index_build_data_tuple(index, pcur->m_old_rec,
                                                    pcur->m_old_n_fields, heap);

      pcur->open_no_init(index, tuple, pcur->m_search_mode, BTR_SEARCH_LEAF, 0,
                         mtr, UT_LOCATION_HERE);

      mem_heap_free(heap);
    } else {
      /* Restore the cursor for reading next record from cache
      information. */
      ut_ad(index->last_sel_cur->rec != nullptr);
      pcur->m_btr_cur.page_cur.rec = index->last_sel_cur->rec;
      pcur->m_btr_cur.page_cur.block = index->last_sel_cur->block;

      err = row_search_traverse(moves_up, match_mode, pcur, mtr);
      if (err != DB_SUCCESS) {
        return (err);
      }
    }
  } else {
    /* There could be previous uncommitted transaction if SELECT
    is operation as part of SELECT (IF NOT FOUND) INSERT
    (IF DUPLICATE) UPDATE plan. */
    index->last_sel_cur->release();

    /* Capture table snapshot in form of trx-id. */
    index->trx_id = dict_table_get_curr_table_sess_trx_id(index->table);

    /* Fresh search commences. */
    mtr_start(mtr);
    dict_disable_redo_if_temporary(index->table, mtr);

    if (dtuple_get_n_fields(search_tuple) > 0) {
      pcur->open_no_init(index, search_tuple, mode, BTR_SEARCH_LEAF, 0, mtr,
                         UT_LOCATION_HERE);

    } else if (mode == PAGE_CUR_G || mode == PAGE_CUR_L) {
      pcur->open_at_side(mode == PAGE_CUR_G, index, BTR_SEARCH_LEAF, false, 0,
                         mtr);
    }
  }

  /* Step-3: Traverse the records filtering non-qualifying records. */
  for (/* No op */; err == DB_SUCCESS;
       err = row_search_traverse(moves_up, match_mode, pcur, mtr)) {
    const rec_t *rec = pcur->get_rec();

    if (page_rec_is_infimum(rec) || page_rec_is_supremum(rec) ||
        rec_get_deleted_flag(rec, dict_table_is_comp(index->table))) {
      /* The infimum record on a page cannot be in the
      result set, and neither can a record lock be placed on
      it: we skip such a record. */
      continue;
    }

    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    /* Note that we cannot trust the up_match value in the cursor
    at this place because we can arrive here after moving the
    cursor! Thus we have to recompare rec and search_tuple to
    determine if they match enough. */
    if (match_mode == ROW_SEL_EXACT) {
      /* Test if the index record matches completely to
      search_tuple in prebuilt: if not, then we return with
      DB_RECORD_NOT_FOUND */
      if (0 != cmp_dtuple_rec(search_tuple, rec, index, offsets)) {
        err = DB_RECORD_NOT_FOUND;
        break;
      }
    } else if (match_mode == ROW_SEL_EXACT_PREFIX) {
      if (!cmp_dtuple_is_prefix_of_rec(search_tuple, rec, index, offsets)) {
        err = DB_RECORD_NOT_FOUND;
        break;
      }
    }

    /* Get the clustered index. We always need clustered index
    record for snapshort verification. */
    if (index != clust_index) {
      err = row_sel_get_clust_rec_for_mysql(prebuilt, index, rec, thr,
                                            &clust_rec, &offsets, &heap,
                                            nullptr, mtr, nullptr);

      if (err != DB_SUCCESS) {
        break;
      }

      if (rec_get_deleted_flag(clust_rec, dict_table_is_comp(index->table))) {
        /* The record is delete marked in clustered
        index. We can skip this record. */
        continue;
      }

      result_rec = clust_rec;
    } else {
      result_rec = rec;
    }

    /* Step-4: Cache the row-id of selected row to prebuilt cache.*/
    if (prebuilt->clust_index_was_generated) {
      row_sel_store_row_id_to_prebuilt(prebuilt, result_rec, clust_index,
                                       offsets);
    }

    /* Step-5: Convert selected record to MySQL format and
    store it. */
    if (prebuilt->template_type == ROW_MYSQL_DUMMY_TEMPLATE) {
      const rec_t *ret_rec =
          (index != clust_index && prebuilt->need_to_access_clustered)
              ? result_rec
              : rec;

      offsets = rec_get_offsets(ret_rec, index, offsets, ULINT_UNDEFINED,
                                UT_LOCATION_HERE, &heap);

      memcpy(buf + 4, ret_rec - rec_offs_extra_size(offsets),
             rec_offs_size(offsets));

      mach_write_to_4(buf, rec_offs_extra_size(offsets) + 4);

    } else if (!row_sel_store_mysql_rec(buf, prebuilt, result_rec, nullptr,
                                        true, clust_index, prebuilt->index,
                                        offsets, false, nullptr,
                                        prebuilt->blob_heap)) {
      err = DB_ERROR;
      break;
    }

    /* Step-6: Store cursor position to fetch next record.
    MySQL calls this function iteratively get_next(), get_next()
    fashion. */
    ut_ad(err == DB_SUCCESS);
    index->last_sel_cur->rec = pcur->get_rec();
    index->last_sel_cur->block = pcur->get_block();

    /* This is needed in order to restore the cursor if index
    structure changes while SELECT is still active. */
    pcur->m_old_rec = dict_index_copy_rec_order_prefix(
        index, rec, &pcur->m_old_n_fields, &pcur->m_old_rec_buf,
        &pcur->m_buf_size);

    pcur->m_block_when_stored.store(pcur->get_block());
    pcur->m_modify_clock = pcur->get_block()->get_modify_clock(IF_DEBUG(true));

    break;
  }

  if (err != DB_SUCCESS) {
    index->last_sel_cur->release();
  }

  if (heap != nullptr) {
    mem_heap_free(heap);
  }
  return (err);
}

/** Extract virtual column data from a virtual index record and fill a dtuple
@param[in]      rec             the virtual (secondary) index record
@param[in]      index           the virtual index
@param[in,out]  vrow            the dtuple where data extract to
@param[in]      heap            memory heap to allocate memory
*/
static void row_sel_fill_vrow(const rec_t *rec, dict_index_t *index,
                              const dtuple_t **vrow, mem_heap_t *heap) {
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(!(*vrow));

  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &heap);

  *vrow =
      dtuple_create_with_vcol(heap, 0, dict_table_get_n_v_cols(index->table));

  /* Initialize all virtual row's mtype to DATA_MISSING */
  dtuple_init_v_fld(*vrow);

  for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
    const dict_field_t *field;
    const dict_col_t *col;

    field = index->get_field(i);
    col = field->col;

    if (col->is_virtual()) {
      const byte *data;
      ulint len;

      data = rec_get_nth_field(index, rec, offsets, i, &len);

      const dict_v_col_t *vcol = reinterpret_cast<const dict_v_col_t *>(col);

      dfield_t *dfield = dtuple_get_nth_v_field(*vrow, vcol->v_pos);
      dfield_set_data(dfield, data, len);
      col->copy_type(dfield_get_type(dfield));
    }
  }
}
/** The return type of row_compare_row_to_range() which summarizes information
about the relation between the row being processed, and the range of the scan */
struct row_to_range_relation_t {
  /** true: we don't know, false: row is not in range */
  bool row_can_be_in_range;
  /** true: we don't know, false: gap has nothing in common with range */
  bool gap_can_intersect_range;
  /** true: row exactly matches end of range, false: we don't know */
  bool row_must_be_at_end;
};

/** A helper function extracted from row_search_mvcc() which compares the row
being processed with the range of the scan.
It does not modify any of it's arguments and returns a summary of situation.
All the arguments are named the same way as local variables at place of call,
and have same values. */
static row_to_range_relation_t row_compare_row_to_range(
    const bool set_also_gap_locks, const trx_t *const trx,
    const bool unique_search, const dict_index_t *const index,
    const dict_index_t *const clust_index, const rec_t *const rec,
    const bool comp, const page_cur_mode_t mode, const ulint direction,
    const dtuple_t *search_tuple, const ulint *const offsets,
    const bool moves_up, const row_prebuilt_t *const prebuilt) {
  row_to_range_relation_t row_to_range_relation;
  row_to_range_relation.row_can_be_in_range = true;
  row_to_range_relation.gap_can_intersect_range = true;
  row_to_range_relation.row_must_be_at_end = false;

  /* We don't know how to compare row which is on supremum with range, but this
  should not be a problem because row_search_mvcc "skips" over them without
  calling our function */
  ut_ad(!page_rec_is_supremum(rec));
  if (page_rec_is_supremum(rec)) {
    return (row_to_range_relation);
  }

  /* Try to place a lock on the index record; note that delete
  marked records are a special case in a unique search. If there
  is a non-delete marked record, then it is enough to lock its
  existence with LOCK_REC_NOT_GAP. */

  /* If we are doing a 'greater or equal than a primary key
  value' search from a clustered index, and we find a record
  that has that exact primary key value, then there is no need
  to lock the gap before the record, because no insert in the
  gap can be in our search range. That is, no phantom row can
  appear that way.

  An example: if col1 is the primary key, the search is WHERE
  col1 >= 100, and we find a record where col1 = 100, then no
  need to lock the gap before that record. */

  if (!set_also_gap_locks || trx->skip_gap_locks() ||
      (unique_search && !rec_get_deleted_flag(rec, comp)) ||
      dict_index_is_spatial(index) ||
      (index == clust_index && mode == PAGE_CUR_GE && direction == 0 &&
       dtuple_get_n_fields_cmp(search_tuple) ==
           dict_index_get_n_unique(index) &&
       0 == cmp_dtuple_rec(search_tuple, rec, index, offsets))) {
    row_to_range_relation.gap_can_intersect_range = false;
    return (row_to_range_relation);
  }

  /* We don't know how to handle HANDLER interface */
  if (prebuilt->used_in_HANDLER) {
    return (row_to_range_relation);
  }

  /* While I believe that we handle semi-consistent reads correctly, the proof
  is quite complicated and lingers on the fact that semi-consistent reads are
  used only if we don't use gap locks. And fortunately, we've already checked
  above that trx->skip_gap_locks() is false, so we don't have to go through the
  whole reasoning about what exactly happens in case the row which is at the
  end of the range got locked by another transaction, removed, purged, and while
  we were doing semi-consistent read on it. */
  ut_ad(!trx->skip_gap_locks());
  ut_ad(prebuilt->row_read_type == ROW_READ_WITH_LOCKS);

  /* Following heuristics are meant to avoid locking the row itself, or even
  the gap before it, in case when the row is "after the end of range". The
  difficulty here is in that the index itself can be declared as either
  ascending or descending, separately for each column, and cursor can be
  PAGE_CUR_G(E) or PAGE_CUR_L(E) etc., and direction of scan can be 0,
  ROW_SEL_NEXT or ROW_SEL_PREV, and this might be a secondary index (with
  duplicates). So we limit ourselves just to the cases, which are at the
  same common, tested, actionable and easy to reason about.
  In particular we only handle cases where we iterate the index in its
  natural order. */
  if (index == clust_index && (mode == PAGE_CUR_GE || mode == PAGE_CUR_G) &&
      (direction == 0 || direction == ROW_SEL_NEXT) &&
      prebuilt->is_reading_range()) {
    ut_ad(moves_up);
    const auto stop_len = dtuple_get_n_fields_cmp(prebuilt->m_stop_tuple);
    if (0 < stop_len) {
      const auto index_len = dict_index_get_n_unique(index);
      ut_ad(prebuilt->m_mysql_handler->end_range != nullptr);
      if (stop_len <= index_len) {
        const auto cmp =
            cmp_dtuple_rec(prebuilt->m_stop_tuple, rec, index, offsets);

        if (cmp < 0) {
          row_to_range_relation.row_can_be_in_range = false;
          if (prebuilt->m_stop_tuple_found) {
            ut_ad(stop_len == index_len);
            row_to_range_relation.gap_can_intersect_range = false;
            return (row_to_range_relation);
          }
          return (row_to_range_relation);
        }

        if (cmp == 0) {
          ut_ad(!prebuilt->m_stop_tuple_found);
          row_to_range_relation.row_can_be_in_range =
              prebuilt->m_mysql_handler->end_range->flag != HA_READ_BEFORE_KEY;
          row_to_range_relation.row_must_be_at_end = stop_len == index_len;
          return (row_to_range_relation);
        }
      }
    }
  }
  return (row_to_range_relation);
}

#ifdef UNIV_DEBUG
/** If the record is not old version, copies an initial segment
of a physical record to be compared later for debug assertion code.
@param[in]      pcur            cursor whose position has been stored
@param[in]      index           index
@param[in]      rec             record for which to copy prefix
@param[out]     n_fields        number of fields copied
@param[in,out]  buf             memory buffer for the copied prefix, or nullptr
@param[in,out]  buf_size        buffer size
@return pointer to the prefix record if not old version. or nullptr if old */
static inline rec_t *row_search_debug_copy_rec_order_prefix(
    const btr_pcur_t *pcur, const dict_index_t *index, const rec_t *rec,
    ulint *n_fields, byte **buf, size_t *buf_size) {
  if (pcur->get_rec() == rec) {
    return dict_index_copy_rec_order_prefix(index, rec, n_fields, buf,
                                            buf_size);
  } else {
    return nullptr;
  }
}
#endif /* UNIV_DEBUG */

/** Searches for rows in the database using cursor.
Function is mainly used for tables that are shared accorss connection and
so it employs technique that can help re-construct the rows that
transaction is suppose to see.
It also has optimization such as pre-caching the rows, using AHI, etc.

@param[out]     buf             buffer for the fetched row in MySQL format
@param[in]      mode            search mode PAGE_CUR_L
@param[in,out]  prebuilt        prebuilt struct for the table handler;
                                this contains the info to search_tuple,
                                index; if search tuple contains 0 field then
                                we position the cursor at start or the end of
                                index, depending on 'mode'
@param[in]      match_mode      0 or ROW_SEL_EXACT or ROW_SEL_EXACT_PREFIX
@param[in]      direction       0 or ROW_SEL_NEXT or ROW_SEL_PREV;
                                Note: if this is != 0, then prebuilt must has a
                                pcur with stored position! In opening of a
                                cursor 'direction' should be 0.
@return DB_SUCCESS or error code */
dberr_t row_search_mvcc(byte *buf, page_cur_mode_t mode,
                        row_prebuilt_t *prebuilt, ulint match_mode,
                        const ulint direction) {
  DBUG_TRACE;

  dict_index_t *index = prebuilt->index;
  bool comp = dict_table_is_comp(index->table);
  const dtuple_t *search_tuple = prebuilt->search_tuple;
  btr_pcur_t *pcur = prebuilt->pcur;
  trx_t *trx = prebuilt->trx;
  dict_index_t *clust_index;
  /* True if we are scanning a secondary index, but the template is based
  on the primary index. */
  bool clust_templ_for_sec;
  que_thr_t *thr;
  const rec_t *prev_rec = nullptr;
#ifdef UNIV_DEBUG
  const rec_t *prev_rec_debug = nullptr;
  ulint prev_rec_debug_n_fields = 0;
  byte *prev_rec_debug_buf = nullptr;
  size_t prev_rec_debug_buf_size = 0;
#endif /* UNIV_DEBUG */
  const rec_t *rec = nullptr;
  byte *end_range_cache = nullptr;
  const dtuple_t *prev_vrow = nullptr;
  const dtuple_t *vrow = nullptr;
  const rec_t *result_rec = nullptr;
  const rec_t *clust_rec;
  Row_sel_get_clust_rec_for_mysql row_sel_get_clust_rec_for_mysql;
  dberr_t err = DB_SUCCESS;
  bool unique_search = false;
  bool mtr_has_extra_clust_latch = false;
  bool moves_up = false;
  bool set_also_gap_locks = true;
  /* if the query is a plain locking SELECT, and the isolation level
  is <= TRX_ISO_READ_COMMITTED, then this is set to false */
  bool did_semi_consistent_read = false;
  /* if the returned record was locked and we did a semi-consistent
  read (fetch the newest committed version), then this is set to
  true */
  ulint next_offs;
  bool same_user_rec = false;
  mtr_t mtr;
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  ulint sec_offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *sec_offsets = nullptr;
  bool table_lock_waited = false;
  byte *next_buf = nullptr;
  bool spatial_search = false;
  ulint end_loop = 0;

  rec_offs_init(offsets_);

  ut_ad(index && pcur && search_tuple);
  ut_a(prebuilt->magic_n == ROW_PREBUILT_ALLOCATED);
  ut_a(prebuilt->magic_n2 == ROW_PREBUILT_ALLOCATED);
  ut_a(!trx->has_search_latch);

  /* We don't support FTS queries from the HANDLER interfaces, because
  we implemented FTS as reversed inverted index with auxiliary tables.
  So anything related to traditional index query would not apply to
  it. */
  if (prebuilt->index->type & DICT_FTS) {
    return DB_END_OF_INDEX;
  }

#ifdef UNIV_DEBUG
  {
    btrsea_sync_check check(trx->has_search_latch);
    ut_ad(!sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */

  if (dict_table_is_discarded(prebuilt->table)) {
    return DB_TABLESPACE_DELETED;

  } else if (prebuilt->table->ibd_file_missing) {
    return DB_TABLESPACE_NOT_FOUND;

  } else if (!prebuilt->index_usable) {
    return DB_MISSING_HISTORY;

  } else if (prebuilt->index->is_corrupted()) {
    return DB_CORRUPTION;
  }

  /* We need to get the virtual column values stored in secondary
  index key, if this is covered index scan or virtual key read is
  requested. */
  bool need_vrow = dict_index_has_virtual(prebuilt->index) &&
                   (prebuilt->read_just_key || prebuilt->m_read_virtual_key);

  /* Reset the new record lock info.
  Then we are able to remove the record locks set here on an
  individual row. */
  prebuilt->new_rec_lock.reset();
  /*-------------------------------------------------------------*/
  /* PHASE 1: Try to pop the row from the record buffer or from
  the prefetch cache */

  const auto record_buffer = row_sel_get_record_buffer(prebuilt);

  if (UNIV_UNLIKELY(direction == 0)) {
    trx->op_info = "starting index read";

    prebuilt->n_rows_fetched = 0;
    prebuilt->n_fetch_cached = 0;
    prebuilt->fetch_cache_first = 0;
    prebuilt->m_end_range = false;
    if (record_buffer != nullptr) {
      record_buffer->reset();
    }

    if (prebuilt->sel_graph == nullptr) {
      /* Build a dummy select query graph */
      row_prebuild_sel_graph(prebuilt);
    }
  } else {
    trx->op_info = "fetching rows";

    if (prebuilt->n_rows_fetched == 0) {
      prebuilt->fetch_direction = direction;
    }

    if (UNIV_UNLIKELY(direction != prebuilt->fetch_direction)) {
      if (UNIV_UNLIKELY(prebuilt->n_fetch_cached > 0)) {
        ut_error;
        /* TODO: scrollable cursor: restore cursor to
        the place of the latest returned row,
        or better: prevent caching for a scroll
        cursor! */
      }

      prebuilt->n_rows_fetched = 0;
      prebuilt->n_fetch_cached = 0;
      prebuilt->fetch_cache_first = 0;
      prebuilt->m_end_range = false;

      /* A record buffer is not used for scroll cursors.
      Otherwise, it would have to be reset here too. */
      ut_ad(record_buffer == nullptr);

    } else if (UNIV_LIKELY(prebuilt->n_fetch_cached > 0)) {
      row_sel_dequeue_cached_row_for_mysql(buf, prebuilt);

      prebuilt->n_rows_fetched++;

      err = DB_SUCCESS;
      goto func_exit;
    } else if (prebuilt->m_end_range) {
      err = DB_RECORD_NOT_FOUND;
      goto func_exit;
    }

    /* The prefetch cache is exhausted, so fetch_cache_first
    should point to the beginning of the cache. */
    ut_ad(prebuilt->fetch_cache_first == 0);

    if (record_buffer != nullptr && record_buffer->is_out_of_range()) {
      /* The previous returned row was popped from
      the fetch cache, but the end of the range was
      reached while filling the cache, so there are
      no more rows to put into the cache. */

      err = DB_RECORD_NOT_FOUND;
      goto func_exit;
    }

    prebuilt->n_rows_fetched++;

    if (prebuilt->n_rows_fetched > 1000000000) {
      /* Prevent wrap-over */
      prebuilt->n_rows_fetched = 500000000;
    }

    mode = pcur->m_search_mode;
  }

  /* In a search where at most one record in the index may match, we
  can use a LOCK_REC_NOT_GAP type record lock when locking a
  non-delete-marked matching record.

  Note that in a unique secondary index there may be different
  delete-marked versions of a record where only the primary key
  values differ: thus in a secondary index we must use next-key
  locks when locking delete-marked records. */

  if (match_mode == ROW_SEL_EXACT && dict_index_is_unique(index) &&
      dtuple_get_n_fields(search_tuple) == dict_index_get_n_unique(index) &&
      (index->is_clustered() || !dtuple_contains_null(search_tuple))) {
    /* Note above that a UNIQUE secondary index can contain many
    rows with the same key value if one of the columns is the SQL
    null. A clustered index under MySQL can never contain null
    columns because we demand that all the columns in primary key
    are non-null. */

    unique_search = true;

    /* Even if the condition is unique, MySQL seems to try to
    retrieve also a second row if a primary key contains more than
    1 column. Return immediately if this is not a HANDLER
    command. */

    if (UNIV_UNLIKELY(direction != 0 && !prebuilt->used_in_HANDLER)) {
      err = DB_RECORD_NOT_FOUND;
      goto func_exit;
    }
  }

  /* We don't support sequential scan for Rtree index, because it
  is no meaning to do so. */
  if (dict_index_is_spatial(index) && !RTREE_SEARCH_MODE(mode)) {
    err = DB_END_OF_INDEX;
    goto func_exit;
  }

  mtr_start(&mtr);

  /*-------------------------------------------------------------*/
  /* PHASE 2: Try fast adaptive hash index search if possible */

  /* Next test if this is the special case where we can use the fast
  adaptive hash index to try the search. Since we must release the
  search system latch when we retrieve an externally stored field, we
  cannot use the adaptive hash index in a search in the case the row
  may be long and there may be externally stored fields */

  if (UNIV_UNLIKELY(direction == 0) && unique_search && btr_search_enabled &&
      index->is_clustered() && !prebuilt->templ_contains_blob &&
      !prebuilt->used_in_HANDLER &&
      (prebuilt->mysql_row_len < UNIV_PAGE_SIZE / 8) && !prebuilt->innodb_api) {
    mode = PAGE_CUR_GE;

    if (trx->mysql_n_tables_locked == 0 && !prebuilt->ins_sel_stmt &&
        prebuilt->select_lock_type == LOCK_NONE &&
        trx->isolation_level > TRX_ISO_READ_UNCOMMITTED &&
        MVCC::is_view_active(trx->read_view)) {
      /* This is a SELECT query done as a consistent read,
      and the read view has already been allocated:
      let us try a search shortcut through the hash
      index.
      NOTE that we must also test that
      mysql_n_tables_locked == 0, because this might
      also be INSERT INTO ... SELECT ... or
      CREATE TABLE ... SELECT ... . Our algorithm is
      NOT prepared to inserts interleaved with the SELECT,
      and if we try that, we can deadlock on the adaptive
      hash index semaphore! */

      ut_a(!trx->has_search_latch);
      rw_lock_s_lock(btr_get_search_latch(index), UT_LOCATION_HERE);
      trx->has_search_latch = true;

      switch (row_sel_try_search_shortcut_for_mysql(&rec, prebuilt, &offsets,
                                                    &heap, &mtr)) {
        case SEL_FOUND:
          /* At this point, rec is protected by
          a page latch that was acquired by
          row_sel_try_search_shortcut_for_mysql().
          The latch will not be released until
          mtr_commit(&mtr). */
          ut_ad(!rec_get_deleted_flag(rec, comp));

          if (prebuilt->idx_cond) {
            switch (row_search_idx_cond_check(buf, prebuilt, rec, offsets)) {
              case ICP_NO_MATCH:
              case ICP_OUT_OF_RANGE:
                goto shortcut_mismatch;
              case ICP_MATCH:
                goto shortcut_match;
            }
          }

          if (!row_sel_store_mysql_rec(buf, prebuilt, rec, nullptr, false,
                                       index, prebuilt->index, offsets, false,
                                       nullptr, prebuilt->blob_heap)) {
            /* Only fresh inserts may contain
            incomplete externally stored
            columns. Pretend that such
            records do not exist. Such
            records may only be accessed
            at the READ UNCOMMITTED
            isolation level or when
            rolling back a recovered
            transaction. Rollback happens
            at a lower level, not here. */

            /* Proceed as in case SEL_RETRY. */
            break;
          }

        shortcut_match:
          mtr_commit(&mtr);

          /* NOTE that we do NOT store the cursor
          position */

          err = DB_SUCCESS;

          rw_lock_s_unlock(btr_get_search_latch(index));
          trx->has_search_latch = false;

          goto func_exit;

        case SEL_EXHAUSTED:
        shortcut_mismatch:
          mtr_commit(&mtr);

          err = DB_RECORD_NOT_FOUND;

          rw_lock_s_unlock(btr_get_search_latch(index));
          trx->has_search_latch = false;

          /* NOTE that we do NOT store the cursor
          position */

          goto func_exit;

        case SEL_RETRY:
          break;

        default:
          ut_d(ut_error);
      }

      mtr_commit(&mtr);
      mtr_start(&mtr);

      rw_lock_s_unlock(btr_get_search_latch(index));
      trx->has_search_latch = false;
    }
  }

  /*-------------------------------------------------------------*/
  /* PHASE 3: Open or restore index cursor position */

  spatial_search = dict_index_is_spatial(index) && mode >= PAGE_CUR_CONTAIN;

  /* The state of a running trx can only be changed by the
  thread that is currently serving the transaction. Because we
  are that thread, we can read trx->state without holding any
  mutex. */

  ut_ad(prebuilt->sql_stat_start ||
        trx->state.load(std::memory_order_relaxed) == TRX_STATE_ACTIVE);

  ut_ad(!trx_is_started(trx) ||
        trx->state.load(std::memory_order_relaxed) == TRX_STATE_ACTIVE);

  ut_ad(prebuilt->sql_stat_start || prebuilt->select_lock_type != LOCK_NONE ||
        MVCC::is_view_active(trx->read_view) || srv_read_only_mode);

  trx_start_if_not_started(trx, false, UT_LOCATION_HERE);

  if (prebuilt->table->skip_gap_locks() ||
      (trx->skip_gap_locks() && prebuilt->select_lock_type != LOCK_NONE &&
       trx->mysql_thd != nullptr && thd_is_query_block(trx->mysql_thd))) {
    /* It is a plain locking SELECT and the isolation
    level is low: do not lock gaps */

    /* Reads on DD tables dont require gap-locks as serializability
    between different DDL statements is achieved using
    metadata locks */
    set_also_gap_locks = false;
  }

  /* Note that if the search mode was GE or G, then the cursor
  naturally moves upward (in fetch next) in alphabetical order,
  otherwise downward */

  if (direction == 0) {
    if (mode == PAGE_CUR_GE || mode == PAGE_CUR_G || mode >= PAGE_CUR_CONTAIN) {
      moves_up = true;
    }

  } else if (direction == ROW_SEL_NEXT) {
    moves_up = true;
  }

  thr = que_fork_get_first_thr(prebuilt->sel_graph);

  que_thr_move_to_run_state_for_mysql(thr, trx);

  clust_index = index->table->first_index();

  clust_templ_for_sec =
      index != clust_index && prebuilt->need_to_access_clustered;

  /* Do some start-of-statement preparations */

  if (!prebuilt->sql_stat_start) {
    /* No need to set an intention lock or assign a read view */

    if (!MVCC::is_view_active(trx->read_view) && !srv_read_only_mode &&
        prebuilt->select_lock_type == LOCK_NONE) {
      ib::error(ER_IB_MSG_1031) << "MySQL is trying to perform a"
                                   " consistent read but the read view is not"
                                   " assigned!";
      trx_print(stderr, trx, 600);
      fputc('\n', stderr);
      ut_error;
    }
  } else if (prebuilt->select_lock_type == LOCK_NONE) {
    /* This is a consistent read */
    /* Assign a read view for the query */

    if (!srv_read_only_mode) {
      trx_assign_read_view(trx);
    }

    prebuilt->sql_stat_start = false;
  } else {
  wait_table_again:
    err = lock_table(0, index->table,
                     prebuilt->select_lock_type == LOCK_S ? LOCK_IS : LOCK_IX,
                     thr);

    if (err != DB_SUCCESS) {
      table_lock_waited = true;
      goto lock_table_wait;
    }
    prebuilt->sql_stat_start = false;
  }

  /* Open or restore index cursor position */

  if (UNIV_LIKELY(direction != 0)) {
    if (spatial_search) {
      /* R-Tree access does not need to do
      cursor position and resposition */
      goto next_rec;
    }

    auto need_to_process = sel_restore_position_for_mysql(
        &same_user_rec, BTR_SEARCH_LEAF, pcur, moves_up, &mtr);

    ut_ad(prev_rec == nullptr);

    if (UNIV_UNLIKELY(need_to_process)) {
      if (UNIV_UNLIKELY(prebuilt->row_read_type ==
                        ROW_READ_DID_SEMI_CONSISTENT)) {
        /* We did a semi-consistent read,
        but the record was removed in
        the meantime. */
        prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
      }
    } else if (UNIV_LIKELY(prebuilt->row_read_type !=
                           ROW_READ_DID_SEMI_CONSISTENT)) {
      /* The cursor was positioned on the record
      that we returned previously.  If we need
      to repeat a semi-consistent read as a
      pessimistic locking read, the record
      cannot be skipped. */

      goto next_rec;
    }

  } else if (dtuple_get_n_fields(search_tuple) > 0) {
    pcur->m_btr_cur.thr = thr;

    if (dict_index_is_spatial(index)) {
      bool need_pred_lock = set_also_gap_locks && !trx->skip_gap_locks() &&
                            prebuilt->select_lock_type != LOCK_NONE;

      if (!prebuilt->rtr_info) {
        prebuilt->rtr_info = rtr_create_rtr_info(need_pred_lock, true,
                                                 pcur->get_btr_cur(), index);
        prebuilt->rtr_info->search_tuple = search_tuple;
        prebuilt->rtr_info->search_mode = mode;
        rtr_info_update_btr(pcur->get_btr_cur(), prebuilt->rtr_info);
      } else {
        rtr_info_reinit_in_cursor(pcur->get_btr_cur(), index, need_pred_lock);
        prebuilt->rtr_info->search_tuple = search_tuple;
        prebuilt->rtr_info->search_mode = mode;
      }
    }

    pcur->open_no_init(index, search_tuple, mode, BTR_SEARCH_LEAF, 0, &mtr,
                       UT_LOCATION_HERE);

    pcur->m_trx_if_known = trx;

    rec = pcur->get_rec();

    if (!moves_up && !page_rec_is_supremum(rec) && set_also_gap_locks &&
        !trx->skip_gap_locks() && prebuilt->select_lock_type != LOCK_NONE &&
        !dict_index_is_spatial(index)) {
      /* Try to place a gap lock on the next index record
      to prevent phantoms in ORDER BY ... DESC queries */
      const rec_t *next_rec = page_rec_get_next_const(rec);

      offsets = rec_get_offsets(next_rec, index, offsets, ULINT_UNDEFINED,
                                UT_LOCATION_HERE, &heap);
      err = sel_set_rec_lock(pcur, next_rec, index, offsets,
                             prebuilt->select_mode, prebuilt->select_lock_type,
                             LOCK_GAP, thr, &mtr);

      switch (err) {
        case DB_SUCCESS_LOCKED_REC:
          err = DB_SUCCESS;
        case DB_SUCCESS:
          break;
        case DB_SKIP_LOCKED:
        case DB_LOCK_NOWAIT:
          ut_d(ut_error);
          ut_o(goto next_rec);
        default:
          goto lock_wait_or_error;
      }
    }
  } else if (mode == PAGE_CUR_G || mode == PAGE_CUR_L) {
    pcur->open_at_side(mode == PAGE_CUR_G, index, BTR_SEARCH_LEAF, false, 0,
                       &mtr);
  }

rec_loop:
  ut_ad(trx_can_be_handled_by_current_thread(trx));
  DEBUG_SYNC_C("row_search_rec_loop");

  prebuilt->lob_undo_reset();

  if (trx_is_interrupted(trx)) {
    if (!spatial_search) {
      pcur->store_position(&mtr);
    }
    err = DB_INTERRUPTED;
    goto normal_return;
  }

  /*-------------------------------------------------------------*/
  /* PHASE 4: Look for matching records in a loop */

  rec = pcur->get_rec();

  ut_ad(page_rec_is_comp(rec) == comp);

  if (page_rec_is_infimum(rec)) {
    /* The infimum record on a page cannot be in the result set,
    and neither can a record lock be placed on it: we skip such
    a record. */

    prev_rec = nullptr;
    goto next_rec;
  }

  if (page_rec_is_supremum(rec)) {
    DBUG_EXECUTE_IF(
        "compare_end_range", if (end_loop < 100) { end_loop = 100; });

    /** Compare the last record of the page with end range
    passed to InnoDB when there is no ICP and number of
    loops in row_search_mvcc for rows found but not
    reporting due to search views etc.
    When scanning a multi-value index, we don't perform the
    check because we cannot convert the indexed value
    (single scalar element) into the primary index (virtual)
    column type (array of values).  */
    if (prev_rec != nullptr && !prebuilt->innodb_api &&
        prebuilt->m_mysql_handler->end_range != nullptr &&
        prebuilt->idx_cond == false && end_loop >= 100 &&
        !(clust_templ_for_sec && index->is_multi_value())) {
      dict_index_t *key_index = prebuilt->index;

      if (end_range_cache == nullptr) {
        end_range_cache = static_cast<byte *>(ut::malloc_withkey(
            UT_NEW_THIS_FILE_PSI_KEY, prebuilt->mysql_row_len));
      }

      if (clust_templ_for_sec) {
        /** Secondary index record but the template
        based on PK. */
        key_index = clust_index;
      }

      /** Create offsets based on prebuilt index. */
      offsets = rec_get_offsets(prev_rec, prebuilt->index, offsets,
                                ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

      if (row_sel_store_mysql_rec(end_range_cache, prebuilt, prev_rec,
                                  prev_vrow, clust_templ_for_sec, key_index,
                                  prebuilt->index, offsets, clust_templ_for_sec,
                                  prebuilt->get_lob_undo(),
                                  prebuilt->blob_heap)) {
        if (row_search_end_range_check(end_range_cache, prev_rec, prebuilt,
                                       clust_templ_for_sec, offsets,
                                       record_buffer)) {
          /** In case of prebuilt->fetch,
          set the error in prebuilt->end_range. */
          if (next_buf != nullptr) {
            prebuilt->m_end_range = true;
          }

          err = DB_RECORD_NOT_FOUND;
          goto normal_return;
        }
      }
      DEBUG_SYNC_C("allow_insert");
    }

    if (set_also_gap_locks && !trx->skip_gap_locks() &&
        prebuilt->select_lock_type != LOCK_NONE &&
        !dict_index_is_spatial(index)) {
      /* Try to place a lock on the index record */

      offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                                UT_LOCATION_HERE, &heap);
      err = sel_set_rec_lock(pcur, rec, index, offsets, prebuilt->select_mode,
                             prebuilt->select_lock_type, LOCK_ORDINARY, thr,
                             &mtr);

      switch (err) {
        case DB_SUCCESS_LOCKED_REC:
          err = DB_SUCCESS;
        case DB_SUCCESS:
          break;
        case DB_SKIP_LOCKED:
        case DB_LOCK_NOWAIT:
          ut_d(ut_error);
        default:
          goto lock_wait_or_error;
      }
      DEBUG_SYNC_C("allow_insert");
    }

    /* A page supremum record cannot be in the result set: skip
    it now that we have placed a possible lock on it */

    prev_rec = nullptr;
    goto next_rec;
  }

  /*-------------------------------------------------------------*/
  /* Do sanity checks in case our cursor has bumped into page
  corruption */

  if (comp) {
    next_offs = rec_get_next_offs(rec, true);
    if (UNIV_UNLIKELY(next_offs < PAGE_NEW_SUPREMUM)) {
      goto wrong_offs;
    }
  } else {
    next_offs = rec_get_next_offs(rec, false);
    if (UNIV_UNLIKELY(next_offs < PAGE_OLD_SUPREMUM)) {
      goto wrong_offs;
    }
  }

  if (UNIV_UNLIKELY(next_offs >= UNIV_PAGE_SIZE - PAGE_DIR)) {
  wrong_offs:
    if (srv_force_recovery == 0 || moves_up == false) {
      ib::error(ER_IB_MSG_1032)
          << "Rec address " << static_cast<const void *>(rec)
          << ", buf block fix count "
          << btr_cur_get_block(pcur->get_btr_cur())->page.buf_fix_count;

      ib::error(ER_IB_MSG_1033)
          << "Index corruption: rec offs " << page_offset(rec) << " next offs "
          << next_offs << ", page no " << page_get_page_no(page_align(rec))
          << ", index " << index->name << " of table " << index->table->name
          << ". Run CHECK TABLE. You may need to"
             " restore from a backup, or dump + drop +"
             " reimport the table.";
      err = DB_CORRUPTION;

      ut_d(ut_error);
      ut_o(goto lock_wait_or_error);
    } else {
      /* The user may be dumping a corrupt table. Jump
      over the corruption to recover as much as possible. */

      ib::info(ER_IB_MSG_1034)
          << "Index corruption: rec offs " << page_offset(rec) << " next offs "
          << next_offs << ", page no " << page_get_page_no(page_align(rec))
          << ", index " << index->name << " of table " << index->table->name
          << ". We try to skip the rest of the page.";

      pcur->move_to_last_on_page(&mtr);

      prev_rec = nullptr;
      goto next_rec;
    }
  }
  /*-------------------------------------------------------------*/

  /* Calculate the 'offsets' associated with 'rec' */

  ut_ad(fil_page_index_page_check(pcur->get_page()));
  ut_ad(btr_page_get_index_id(pcur->get_page()) == index->id);

  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &heap);

  if (UNIV_UNLIKELY(srv_force_recovery > 0)) {
    if (!rec_validate(rec, offsets) ||
        !btr_index_rec_validate(rec, index, false)) {
      ib::info(ER_IB_MSG_1035)
          << "Index corruption: rec offs " << page_offset(rec) << " next offs "
          << next_offs << ", page no " << page_get_page_no(page_align(rec))
          << ", index " << index->name << " of table " << index->table->name
          << ". We try to skip the record.";

      prev_rec = nullptr;
      goto next_rec;
    }
  }

  prev_rec = rec;
  ut_d(prev_rec_debug = row_search_debug_copy_rec_order_prefix(
           pcur, index, prev_rec, &prev_rec_debug_n_fields, &prev_rec_debug_buf,
           &prev_rec_debug_buf_size));

  /* Note that we cannot trust the up_match value in the cursor at this
  place because we can arrive here after moving the cursor! Thus
  we have to recompare rec and search_tuple to determine if they
  match enough. */

  if (match_mode == ROW_SEL_EXACT) {
    /* Test if the index record matches completely to search_tuple
    in prebuilt: if not, then we return with DB_RECORD_NOT_FOUND */

    /* fputs("Comparing rec and search tuple\n", stderr); */

    if (0 != cmp_dtuple_rec(search_tuple, rec, index, offsets)) {
      if (set_also_gap_locks && !trx->skip_gap_locks() &&
          prebuilt->select_lock_type != LOCK_NONE &&
          !dict_index_is_spatial(index)) {
        err = sel_set_rec_lock(pcur, rec, index, offsets, prebuilt->select_mode,
                               prebuilt->select_lock_type, LOCK_GAP, thr, &mtr);

        switch (err) {
          case DB_SUCCESS_LOCKED_REC:
          case DB_SUCCESS:
            break;
          case DB_SKIP_LOCKED:
          case DB_LOCK_NOWAIT:
            ut_d(ut_error);
          default:
            goto lock_wait_or_error;
        }
      }

      pcur->store_position(&mtr);

      /* The found record was not a match, but may be used
      as NEXT record (index_next). Set the relative position
      to BTR_PCUR_BEFORE, to reflect that the position of
      the persistent cursor is before the found/stored row
      (pcur->m_old_rec). */
      ut_ad(pcur->m_rel_pos == BTR_PCUR_ON);
      pcur->m_rel_pos = BTR_PCUR_BEFORE;

      err = DB_RECORD_NOT_FOUND;
      goto normal_return;
    }

  } else if (match_mode == ROW_SEL_EXACT_PREFIX) {
    if (!cmp_dtuple_is_prefix_of_rec(search_tuple, rec, index, offsets)) {
      if (set_also_gap_locks && !trx->skip_gap_locks() &&
          prebuilt->select_lock_type != LOCK_NONE &&
          !dict_index_is_spatial(index)) {
        err = sel_set_rec_lock(pcur, rec, index, offsets, prebuilt->select_mode,
                               prebuilt->select_lock_type, LOCK_GAP, thr, &mtr);

        switch (err) {
          case DB_SUCCESS_LOCKED_REC:
          case DB_SUCCESS:
            break;
          case DB_SKIP_LOCKED:
          case DB_LOCK_NOWAIT:
            ut_d(ut_error);
          default:
            goto lock_wait_or_error;
        }
      }

      pcur->store_position(&mtr);

      /* The found record was not a match, but may be used
      as NEXT record (index_next). Set the relative position
      to BTR_PCUR_BEFORE, to reflect that the position of
      the persistent cursor is before the found/stored row
      (pcur->old_rec). */
      ut_ad(pcur->m_rel_pos == BTR_PCUR_ON);
      pcur->m_rel_pos = BTR_PCUR_BEFORE;

      err = DB_RECORD_NOT_FOUND;
      goto normal_return;
    }
  }

  /* We are ready to look at a possible new index entry in the result
  set: the cursor is now placed on a user record */

  if (prebuilt->select_lock_type != LOCK_NONE) {
    auto row_to_range_relation = row_compare_row_to_range(
        set_also_gap_locks, trx, unique_search, index, clust_index, rec, comp,
        mode, direction, search_tuple, offsets, moves_up, prebuilt);

    ulint lock_type;
    if (row_to_range_relation.row_can_be_in_range) {
      if (row_to_range_relation.gap_can_intersect_range) {
        lock_type = LOCK_ORDINARY;
      } else {
        lock_type = LOCK_REC_NOT_GAP;
      }
    } else {
      if (row_to_range_relation.gap_can_intersect_range) {
        lock_type = LOCK_GAP;
      } else {
        err = DB_RECORD_NOT_FOUND;
        goto normal_return;
      }
    }
    /* in case of semi-consistent read, we use SELECT_SKIP_LOCKED, so we don't
    waste time on creating a WAITING lock, as we won't wait on it anyway */
    const bool use_semi_consistent =
        prebuilt->row_read_type == ROW_READ_TRY_SEMI_CONSISTENT &&
        !unique_search && index == clust_index && !trx_is_high_priority(trx);
    err = sel_set_rec_lock(
        pcur, rec, index, offsets,
        use_semi_consistent ? SELECT_SKIP_LOCKED : prebuilt->select_mode,
        prebuilt->select_lock_type, lock_type, thr, &mtr);

    switch (err) {
      const rec_t *old_vers;
      case DB_SUCCESS_LOCKED_REC:
        if (trx->releases_non_matching_rows()) {
          /* Note that a record of
          prebuilt->index was locked. */
          ut_ad(!prebuilt->new_rec_lock[row_prebuilt_t::LOCK_PCUR]);
          prebuilt->new_rec_lock[row_prebuilt_t::LOCK_PCUR] = true;
        }
        err = DB_SUCCESS;
        [[fallthrough]];
      case DB_SUCCESS:
        if (row_to_range_relation.row_must_be_at_end) {
          prebuilt->m_stop_tuple_found = true;
        }
        break;
      case DB_SKIP_LOCKED:
        if (prebuilt->select_mode == SELECT_SKIP_LOCKED) {
          goto next_rec;
        }
        DEBUG_SYNC_C("semi_consistent_read_would_wait");
        ut_a(use_semi_consistent);
        ut_a(trx->allow_semi_consistent());
        /* The following call returns 'offsets' associated with 'old_vers' */
        row_sel_build_committed_vers_for_mysql(
            clust_index, prebuilt, rec, &offsets, &heap, &old_vers,
            need_vrow ? &vrow : nullptr, &mtr);

        ut_ad(!dict_index_is_spatial(index));
        err = DB_SUCCESS;

        if (old_vers == nullptr) {
          /* The row was not yet committed */
          goto next_rec;
        }

        did_semi_consistent_read = true;
        rec = old_vers;
        prev_rec = rec;
        ut_d(prev_rec_debug = row_search_debug_copy_rec_order_prefix(
                 pcur, index, prev_rec, &prev_rec_debug_n_fields,
                 &prev_rec_debug_buf, &prev_rec_debug_buf_size));
        break;
      case DB_LOCK_WAIT:
        /* Lock wait for R-tree should already
        be handled in sel_set_rtr_rec_lock() */
        ut_ad(!dict_index_is_spatial(index));
        /* Never unlock rows that were part of a conflict. */
        prebuilt->new_rec_lock.reset();
        ut_a(!use_semi_consistent);
        goto lock_wait_or_error;
      case DB_RECORD_NOT_FOUND:
        if (dict_index_is_spatial(index)) {
          goto next_rec;
        } else {
          goto lock_wait_or_error;
        }

      default:
        ut_a(!use_semi_consistent);
        goto lock_wait_or_error;
    }
    if (err == DB_SUCCESS && !row_to_range_relation.row_can_be_in_range) {
      err = DB_RECORD_NOT_FOUND;
      goto normal_return;
    }
  } else {
    /* This is a non-locking consistent read: if necessary, fetch
    a previous version of the record */

    if (trx->isolation_level == TRX_ISO_READ_UNCOMMITTED) {
      /* Do nothing: we let a non-locking SELECT read the
      latest version of the record */

    } else if (index == clust_index) {
      /* Fetch a previous version of the row if the current
      one is not visible in the snapshot; if we have a very
      high force recovery level set, we try to avoid crashes
      by skipping this lookup */

      if (srv_force_recovery < 5 &&
          !lock_clust_rec_cons_read_sees(rec, index, offsets,
                                         trx_get_read_view(trx))) {
        rec_t *old_vers;
        /* The following call returns 'offsets' associated with 'old_vers' */
        err = row_sel_build_prev_vers_for_mysql(
            trx->read_view, clust_index, prebuilt, rec, &offsets, &heap,
            &old_vers, need_vrow ? &vrow : nullptr, &mtr,
            prebuilt->get_lob_undo());

        if (err != DB_SUCCESS) {
          goto lock_wait_or_error;
        }

        if (old_vers == nullptr) {
          /* The row did not exist yet in
          the read view */

          goto next_rec;
        }

        rec = old_vers;
        prev_rec = rec;
        ut_d(prev_rec_debug = row_search_debug_copy_rec_order_prefix(
                 pcur, index, prev_rec, &prev_rec_debug_n_fields,
                 &prev_rec_debug_buf, &prev_rec_debug_buf_size));
      }
    } else {
      /* We are looking into a non-clustered index,
      and to get the right version of the record we
      have to look also into the clustered index: this
      is necessary, because we can only get the undo
      information via the clustered index record. */

      ut_ad(!index->is_clustered());

      if (!srv_read_only_mode &&
          !lock_sec_rec_cons_read_sees(rec, index, trx->read_view)) {
        /* We should look at the clustered index.
        However, as this is a non-locking read,
        we can skip the clustered index lookup if
        the condition does not match the secondary
        index entry. */
        switch (row_search_idx_cond_check(buf, prebuilt, rec, offsets)) {
          case ICP_NO_MATCH:
            goto next_rec;
          case ICP_OUT_OF_RANGE:
            err = DB_RECORD_NOT_FOUND;
            goto idx_cond_failed;
          case ICP_MATCH:
            goto requires_clust_rec;
        }

        ut_error;
      }
    }
  }

#ifdef UNIV_DEBUG
  if (did_semi_consistent_read) {
    ut_a(prebuilt->select_lock_type != LOCK_NONE);
    ut_a(!prebuilt->table->is_intrinsic());
    ut_a(prebuilt->row_read_type == ROW_READ_TRY_SEMI_CONSISTENT);
    ut_a(prebuilt->trx->allow_semi_consistent());
    ut_a(prebuilt->new_rec_locks_count() == 0);
  }
#endif /* UNIV_DEBUG */

  /* NOTE that at this point rec can be an old version of a clustered
  index record built for a consistent read. We cannot assume after this
  point that rec is on a buffer pool page. Functions like
  page_rec_is_comp() cannot be used! */

  if (rec_get_deleted_flag(rec, comp)) {
    /* The record is delete-marked: we can skip it */

    /* No need to keep a lock on a delete-marked record in lower isolation
    levels - it's similar to when Server sees the WHERE condition doesn't match
    and calls unlock_row(). */
    prebuilt->try_unlock(true);

    /* This is an optimization to skip setting the next key lock
    on the record that follows this delete-marked record. This
    optimization works because of the unique search criteria
    which precludes the presence of a range lock between this
    delete marked record and the record following it.

    For now this is applicable only to clustered indexes while
    doing a unique search except for HANDLER queries because
    HANDLER allows NEXT and PREV even in unique search on
    clustered index. There is scope for further optimization
    applicable to unique secondary indexes. Current behaviour is
    to widen the scope of a lock on an already delete marked record
    if the same record is deleted twice by the same transaction */
    if (index == clust_index && unique_search && !prebuilt->used_in_HANDLER) {
      err = DB_RECORD_NOT_FOUND;

      goto normal_return;
    }

    goto next_rec;
  }

  /* Check if the record matches the index condition. */
  switch (row_search_idx_cond_check(buf, prebuilt, rec, offsets)) {
    case ICP_NO_MATCH:
      prebuilt->try_unlock(true);
      goto next_rec;
    case ICP_OUT_OF_RANGE:
      err = DB_RECORD_NOT_FOUND;
      prebuilt->try_unlock(true);
      goto idx_cond_failed;
    case ICP_MATCH:
      break;
  }

  /* Get the clustered index record if needed, if we did not do the
  search using the clustered index. */

  if (index != clust_index && prebuilt->need_to_access_clustered) {
  requires_clust_rec:
    ut_ad(index != clust_index);
    /* We use a 'goto' to the preceding label if a consistent
    read of a secondary index record requires us to look up old
    versions of the associated clustered index record. */

    ut_ad(rec_offs_validate(rec, index, offsets));

    /* It was a non-clustered index and we must fetch also the
    clustered index record */

    mtr_has_extra_clust_latch = true;

    ut_ad(!vrow);

    /* The following call returns 'offsets' associated with
    'clust_rec'. Note that 'clust_rec' can be an old version
    built for a consistent read. */
    err = row_sel_get_clust_rec_for_mysql(
        prebuilt, index, rec, thr, &clust_rec, &offsets, &heap,
        need_vrow ? &vrow : nullptr, &mtr, prebuilt->get_lob_undo());
    switch (err) {
      case DB_SUCCESS:
        if (clust_rec == nullptr) {
          /* The record did not exist in the read view */
          ut_ad(prebuilt->select_lock_type == LOCK_NONE ||
                dict_index_is_spatial(index));

          goto next_rec;
        }
        break;
      case DB_SKIP_LOCKED:
        goto next_rec;
      case DB_SUCCESS_LOCKED_REC:
        ut_a(clust_rec != nullptr);
        if (trx->releases_non_matching_rows()) {
          /* Note that the clustered index record
          was locked. */
          ut_ad(!prebuilt->new_rec_lock[row_prebuilt_t::LOCK_CLUST_PCUR]);
          prebuilt->new_rec_lock[row_prebuilt_t::LOCK_CLUST_PCUR] = true;
        }
        err = DB_SUCCESS;
        break;
      default:
        vrow = nullptr;
        goto lock_wait_or_error;
    }

    if (rec_get_deleted_flag(clust_rec, comp)) {
      /* The record is delete marked: we can skip it */

      /* No need to keep a lock on a delete-marked record in lower isolation
      levels - it's similar to when Server sees the WHERE condition doesn't
      match and calls unlock_row(). */
      prebuilt->try_unlock(true);

      goto next_rec;
    }

    if (need_vrow && !vrow) {
      if (!heap) {
        heap = mem_heap_create(100, UT_LOCATION_HERE);
      }
      row_sel_fill_vrow(rec, index, &vrow, heap);
    }

    result_rec = clust_rec;
    ut_ad(rec_offs_validate(result_rec, clust_index, offsets));

    if (prebuilt->idx_cond) {
      /* Convert the record to MySQL format. We were
      unable to do this in row_search_idx_cond_check(),
      because the condition is on the secondary index
      and the requested column is in the clustered index.
      We convert all fields, including those that
      may have been used in ICP, because the
      secondary index may contain a column prefix
      rather than the full column. Also, as noted
      in Bug #56680, the column in the secondary
      index may be in the wrong case, and the
      authoritative case is in result_rec, the
      appropriate version of the clustered index record. */
      if (!row_sel_store_mysql_rec(buf, prebuilt, result_rec, vrow, true,
                                   clust_index, prebuilt->index, offsets, false,
                                   nullptr, prebuilt->blob_heap)) {
        goto next_rec;
      }
    }

    /* TODO: This is for a temporary fix, will be removed later */
    /* Check duplicate rec for spatial index. */
    if (dict_index_is_spatial(index) && rec_get_deleted_flag(rec, comp) &&
        prebuilt->rtr_info->is_dup) {
      dtuple_t *clust_row;
      row_ext_t *ext = nullptr;
      rtr_mbr_t clust_mbr;
      rtr_mbr_t index_mbr;
      ulint *index_offsets;
      const dtuple_t *index_entry;
      bool *is_dup_rec = prebuilt->rtr_info->is_dup;

      *is_dup_rec = false;

      if (!heap) {
        heap = mem_heap_create(100, UT_LOCATION_HERE);
      }

      clust_row = row_build(ROW_COPY_DATA, clust_index, clust_rec, offsets,
                            nullptr, nullptr, nullptr, &ext, heap);
      index_entry = row_build_index_entry(clust_row, ext, index, heap);
      rtr_get_mbr_from_tuple(index_entry, &clust_mbr);

      index_offsets = rec_get_offsets(rec, index, nullptr, ULINT_UNDEFINED,
                                      UT_LOCATION_HERE, &heap);
      rtr_get_mbr_from_rec(rec, index_offsets, &index_mbr);

      if (mbr_equal_cmp(index->rtr_srs.get(), &clust_mbr, &index_mbr)) {
        *is_dup_rec = true;
      }
    }
  } else {
    result_rec = rec;
  }

  /* We found a qualifying record 'result_rec'. At this point,
  'offsets' are associated with 'result_rec'. */

  ut_ad(rec_offs_validate(result_rec, result_rec != rec ? clust_index : index,
                          offsets));
  ut_ad(!rec_get_deleted_flag(result_rec, comp));

  /* If we cannot prefetch records, we should not have a record buffer.
  See ha_innobase::ha_is_record_buffer_wanted(). */
  ut_ad(prebuilt->can_prefetch_records() || record_buffer == nullptr);

  /* Decide whether to prefetch extra rows.
  At this point, the clustered index record is protected
  by a page latch that was acquired when pcur was positioned.
  The latch will not be released until mtr_commit(&mtr). */

  if (record_buffer != nullptr ||
      ((match_mode == ROW_SEL_EXACT ||
        prebuilt->n_rows_fetched >= MYSQL_FETCH_CACHE_THRESHOLD) &&
       prebuilt->can_prefetch_records())) {
    /* Inside an update, for example, we do not cache rows,
    since we may use the cursor position to do the actual
    update, that is why we require ...lock_type == LOCK_NONE.
    Since we keep space in prebuilt only for the BLOBs of
    a single row, we cannot cache rows in the case there
    are BLOBs in the fields to be fetched. In HANDLER (note:
    the HANDLER statement, not the handler class) we do
    not cache rows because there the cursor is a scrollable
    cursor. */

    const auto max_rows_to_cache =
        record_buffer ? record_buffer->max_records() : MYSQL_FETCH_CACHE_SIZE;
    ut_a(prebuilt->n_fetch_cached < max_rows_to_cache);

    /* We only convert from InnoDB row format to MySQL row
    format when ICP is disabled. */

    if (!prebuilt->idx_cond) {
      /* We use next_buf to track the allocation of buffers
      where we store and enqueue the buffers for our
      pre-fetch optimisation.

      If next_buf == 0 then we store the converted record
      directly into the MySQL record buffer (buf). If it is
      != 0 then we allocate a pre-fetch buffer and store the
      converted record there.

      If the conversion fails and the MySQL record buffer
      was not written to then we reset next_buf so that
      we can re-use the MySQL record buffer in the next
      iteration. */
      byte *prev_buf = next_buf;

      next_buf = next_buf ? row_sel_fetch_last_buf(prebuilt) : buf;

      if (!row_sel_store_mysql_rec(
              next_buf, prebuilt, result_rec, vrow, result_rec != rec,
              result_rec != rec ? clust_index : index, prebuilt->index, offsets,
              false, nullptr, prebuilt->blob_heap)) {
        if (next_buf == buf) {
          ut_a(prebuilt->n_fetch_cached == 0);
          next_buf = nullptr;
        }

        /* Only fresh inserts may contain incomplete
        externally stored columns. Pretend that such
        records do not exist. Such records may only be
        accessed at the READ UNCOMMITTED isolation
        level or when rolling back a recovered
        transaction. Rollback happens at a lower
        level, not here. */
        goto next_rec;
      }

      /* If we are filling a server-provided buffer, and the
      server has pushed down an end range condition, evaluate
      the condition to prevent that we read too many rows.
      When scanning a multi-value index, we don't perform the
      check because we cannot convert the indexed value
      (single scalar element) into the primary index (virtual)
      column type (array of values). */
      if (record_buffer != nullptr &&
          prebuilt->m_mysql_handler->end_range != nullptr &&
          !(clust_templ_for_sec && index->is_multi_value())) {
        /* If the end-range condition refers to a
        virtual column and we are reading from the
        clustered index, next_buf does not have the
        value of the virtual column. Get the offsets in
        the secondary index so that we can read the
        virtual column from the index. */
        if (clust_templ_for_sec &&
            prebuilt->m_mysql_handler->m_virt_gcol_in_end_range) {
          if (sec_offsets == nullptr) {
            rec_offs_init(sec_offsets_);
            sec_offsets = sec_offsets_;
          }
          sec_offsets =
              rec_get_offsets(rec, index, sec_offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);
        }

        if (row_search_end_range_check(next_buf, rec, prebuilt,
                                       clust_templ_for_sec, sec_offsets,
                                       record_buffer)) {
          if (next_buf != buf) {
            record_buffer->remove_last();
          }
          next_buf = prev_buf;
          err = DB_RECORD_NOT_FOUND;
          goto normal_return;
        }
      }

      if (next_buf != buf) {
        row_sel_enqueue_cache_row_for_mysql(next_buf, prebuilt);
      }
    } else {
      row_sel_enqueue_cache_row_for_mysql(buf, prebuilt);
    }

    if (prebuilt->n_fetch_cached < max_rows_to_cache) {
      goto next_rec;
    }

  } else {
    /* We cannot use a record buffer for this scan, so assert that
    we don't have one. If we have a record buffer here,
    ha_innobase::is_record_buffer_wanted() should be updated so
    that a buffer is not allocated unnecessarily. */
    ut_ad(record_buffer == nullptr);

    if (UNIV_UNLIKELY(prebuilt->template_type == ROW_MYSQL_DUMMY_TEMPLATE)) {
      /* CHECK TABLE: fetch the row */

      if (result_rec != rec && !prebuilt->need_to_access_clustered) {
        /* We used 'offsets' for the clust
        rec, recalculate them for 'rec' */
        offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                                  UT_LOCATION_HERE, &heap);
        result_rec = rec;
      }

      memcpy(buf + 4, result_rec - rec_offs_extra_size(offsets),
             rec_offs_size(offsets));
      mach_write_to_4(buf, rec_offs_extra_size(offsets) + 4);
    } else if (!prebuilt->idx_cond && !prebuilt->innodb_api) {
      /* The record was not yet converted to MySQL format. */
      if (!row_sel_store_mysql_rec(
              buf, prebuilt, result_rec, vrow, result_rec != rec,
              result_rec != rec ? clust_index : index, prebuilt->index, offsets,
              false, prebuilt->get_lob_undo(), prebuilt->blob_heap)) {
        /* Only fresh inserts may contain
        incomplete externally stored
        columns. Pretend that such records do
        not exist. Such records may only be
        accessed at the READ UNCOMMITTED
        isolation level or when rolling back a
        recovered transaction. Rollback
        happens at a lower level, not here. */
        goto next_rec;
      }
    }

    if (prebuilt->clust_index_was_generated) {
      row_sel_store_row_id_to_prebuilt(prebuilt, result_rec,
                                       result_rec == rec ? index : clust_index,
                                       offsets);
    }
  }

  /* From this point on, 'offsets' are invalid. */

  /* We have an optimization to save CPU time: if this is a consistent
  read on a unique condition on the clustered index, then we do not
  store the pcur position, because any fetch next or prev will anyway
  return 'end of file'. Exceptions are locking reads and the MySQL
  HANDLER command where the user can move the cursor with PREV or NEXT
  even after a unique search. */

  err = DB_SUCCESS;

idx_cond_failed:
  if (!unique_search || !index->is_clustered() || direction != 0 ||
      prebuilt->select_lock_type != LOCK_NONE || prebuilt->used_in_HANDLER ||
      prebuilt->innodb_api) {
    /* Inside an update always store the cursor position */

    if (!spatial_search) {
      pcur->store_position(&mtr);
    }

    if (prebuilt->innodb_api && (pcur->get_rec() != result_rec)) {
      ulint rec_size = rec_offs_size(offsets);
      if (!prebuilt->innodb_api_rec_size ||
          (prebuilt->innodb_api_rec_size < rec_size)) {
        prebuilt->innodb_api_buf = static_cast<byte *>(
            mem_heap_alloc(prebuilt->cursor_heap, rec_size));
        prebuilt->innodb_api_rec_size = rec_size;
      }
      prebuilt->innodb_api_rec =
          rec_copy(prebuilt->innodb_api_buf, result_rec, offsets);
    }
  }

  goto normal_return;

next_rec:

  if (end_loop >= 99 && need_vrow && vrow == nullptr && prev_rec != nullptr) {
    if (!heap) {
      heap = mem_heap_create(100, UT_LOCATION_HERE);
    }

    prev_vrow = nullptr;
    row_sel_fill_vrow(prev_rec, index, &prev_vrow, heap);
  } else {
    prev_vrow = vrow;
  }

  end_loop++;

  /* Reset the old and new "did semi-consistent read" flags. */
  if (UNIV_UNLIKELY(prebuilt->row_read_type == ROW_READ_DID_SEMI_CONSISTENT)) {
    prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
  }
  did_semi_consistent_read = false;
  prebuilt->new_rec_lock.reset();

  vrow = nullptr;

  /*-------------------------------------------------------------*/
  /* PHASE 5: Move the cursor to the next index record */

  /* NOTE: For moves_up==false, the mini-transaction will be
  committed and restarted every time when switching b-tree
  pages. For moves_up==true in index condition pushdown, we can
  scan an entire secondary index tree within a single
  mini-transaction. As long as the prebuilt->idx_cond does not
  match, we do not need to consult the clustered index or
  return records to MySQL, and thus we can avoid repositioning
  the cursor. What prevents us from buffer-fixing all leaf pages
  within the mini-transaction is the btr_leaf_page_release()
  call in btr_pcur::move_to_next_page(). Only the leaf page where
  the cursor is positioned will remain buffer-fixed.
  For R-tree spatial search, we also commit the mini-transaction
  each time  */

  if (mtr_has_extra_clust_latch || spatial_search) {
    /* If we have extra cluster latch, we must commit
    mtr if we are moving to the next non-clustered
    index record, because we could break the latching
    order if we would access a different clustered
    index page right away without releasing the previous. */

    bool is_pcur_rec = (pcur->get_rec() == prev_rec);

    /* No need to do store restore for R-tree */
    if (!spatial_search) {
      pcur->store_position(&mtr);
    }

    mtr_commit(&mtr);
    mtr_has_extra_clust_latch = false;

    DEBUG_SYNC_C("row_search_before_mtr_restart_for_extra_clust");

    mtr_start(&mtr);

    if (!spatial_search) {
      const auto result = sel_restore_position_for_mysql(
          &same_user_rec, BTR_SEARCH_LEAF, pcur, moves_up, &mtr);

      if (result) {
        prev_rec = nullptr;
        goto rec_loop;
      }

      ut_ad(same_user_rec);

      if (is_pcur_rec && pcur->get_rec() != prev_rec) {
        /* prev_rec is invalid. */
        prev_rec = nullptr;
      }

#ifdef UNIV_DEBUG
      if (prev_rec != nullptr && prev_rec_debug != nullptr) {
        const ulint *offsets1;
        const ulint *offsets2;

        auto heap_tmp = mem_heap_create(256, UT_LOCATION_HERE);

        offsets1 = rec_get_offsets(prev_rec_debug, index, nullptr,
                                   prev_rec_debug_n_fields, UT_LOCATION_HERE,
                                   &heap_tmp);

        offsets2 =
            rec_get_offsets(prev_rec, index, nullptr, prev_rec_debug_n_fields,
                            UT_LOCATION_HERE, &heap_tmp);

        ut_ad(!cmp_rec_rec(prev_rec_debug, prev_rec, offsets1, offsets2, index,
                           page_is_spatial_non_leaf(prev_rec, index), nullptr,
                           false));
        mem_heap_free(heap_tmp);
      }
#endif /* UNIV_DEBUG */
    }
  }

  if (moves_up) {
    bool move;

    if (spatial_search) {
      move = rtr_pcur_move_to_next(search_tuple, mode, prebuilt->select_mode,
                                   pcur, 0, &mtr);
    } else {
      move = pcur->move_to_next(&mtr);
    }

    if (!move) {
    not_moved:
      if (!spatial_search) {
        pcur->store_position(&mtr);
      }

      if (match_mode != 0) {
        err = DB_RECORD_NOT_FOUND;
      } else {
        err = DB_END_OF_INDEX;
      }

      goto normal_return;
    }
  } else {
    if (UNIV_UNLIKELY(!pcur->move_to_prev(&mtr))) {
      goto not_moved;
    }
  }

  goto rec_loop;

lock_wait_or_error:
  /* Reset the old and new "did semi-consistent read" flags. */
  if (UNIV_UNLIKELY(prebuilt->row_read_type == ROW_READ_DID_SEMI_CONSISTENT)) {
    prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
  }
  did_semi_consistent_read = false;

  /*-------------------------------------------------------------*/
  if (!dict_index_is_spatial(index)) {
    pcur->store_position(&mtr);
  }

lock_table_wait:
  mtr_commit(&mtr);
  mtr_has_extra_clust_latch = false;

  trx->error_state = err;

  /* The following is a patch for MySQL */

  if (thr->is_active) {
    que_thr_stop_for_mysql(thr);
  }

  thr->lock_state = QUE_THR_LOCK_ROW;

  if (row_mysql_handle_errors(&err, trx, thr, nullptr)) {
    /* It was a lock wait, and it ended */

    thr->lock_state = QUE_THR_LOCK_NOLOCK;
    mtr_start(&mtr);

    /* Table lock waited, go try to obtain table lock
    again */
    if (table_lock_waited) {
      table_lock_waited = false;

      goto wait_table_again;
    }

    if (!dict_index_is_spatial(index)) {
      sel_restore_position_for_mysql(&same_user_rec, BTR_SEARCH_LEAF, pcur,
                                     moves_up, &mtr);
      prev_rec = nullptr;
    }

    if (!same_user_rec && trx->releases_non_matching_rows()) {
      /* Since we were not able to restore the cursor
      on the same user record, we cannot use
      row_prebuilt_t::try_unlock() to unlock any records, and
      we must thus reset the new rec lock info. Since
      in lock0lock.cc we have blocked the inheriting of gap
      X-locks, we actually do not have any new record locks
      set in this case.

      Note that if we were able to restore on the 'same'
      user record, it is still possible that we were actually
      waiting on a delete-marked record, and meanwhile
      it was removed by purge and inserted again by some
      other user. But that is no problem, because in
      rec_loop we will again try to set a lock, and
      new_rec_lock_info in trx will be right at the end. */

      prebuilt->new_rec_lock.reset();
    }

    mode = pcur->m_search_mode;

    goto rec_loop;
  }

  thr->lock_state = QUE_THR_LOCK_NOLOCK;

  goto func_exit;

normal_return:
  /*-------------------------------------------------------------*/
  que_thr_stop_for_mysql_no_error(thr, trx);

  mtr_commit(&mtr);

  /* Rollback blocking transactions from hit list for high priority
  transaction, if any. We should not be holding latches here as
  we are going to rollback the blocking transactions. */
  trx_kill_blocking(trx);

  DEBUG_SYNC_C("row_search_for_mysql_before_return");

  if (prebuilt->idx_cond != 0) {
    /* When ICP is active we don't write to the MySQL buffer
    directly, only to buffers that are enqueued in the pre-fetch
    queue. We need to dequeue the first buffer and copy the contents
    to the record buffer that was passed in by MySQL. */

    if (prebuilt->n_fetch_cached > 0) {
      row_sel_dequeue_cached_row_for_mysql(buf, prebuilt);
      err = DB_SUCCESS;
    }

  } else if (next_buf != nullptr) {
    /* We may or may not have enqueued some buffers to the
    pre-fetch queue, but we definitely wrote to the record
    buffer passed to use by MySQL. */

    DEBUG_SYNC_C("row_search_cached_row");
    err = DB_SUCCESS;
  }

#ifdef UNIV_DEBUG
  if (dict_index_is_spatial(index) && err != DB_SUCCESS &&
      err != DB_END_OF_INDEX && err != DB_INTERRUPTED) {
    rtr_node_path_t *path = pcur->m_btr_cur.rtr_info->path;

    ut_ad(path->empty());
  }
#endif

func_exit:
  trx->op_info = "";

  if (end_range_cache != nullptr) {
    ut::free(end_range_cache);
  }

  if (heap != nullptr) {
    mem_heap_free(heap);
  }

#ifdef UNIV_DEBUG
  if (prev_rec_debug_buf != nullptr) {
    ut::free(prev_rec_debug_buf);
  }
#endif /* UNIV_DEBUG */

  /* Set or reset the "did semi-consistent read" flag on return.
  The flag did_semi_consistent_read is set if and only if
  the record being returned was fetched with a semi-consistent read. */
  ut_ad(prebuilt->row_read_type != ROW_READ_WITH_LOCKS ||
        !did_semi_consistent_read);

  if (prebuilt->row_read_type != ROW_READ_WITH_LOCKS) {
    if (did_semi_consistent_read) {
      prebuilt->row_read_type = ROW_READ_DID_SEMI_CONSISTENT;
    } else {
      prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
    }
  }

#ifdef UNIV_DEBUG
  {
    btrsea_sync_check check(trx->has_search_latch);

    ut_ad(!sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */

  DEBUG_SYNC_C("innodb_row_search_for_mysql_exit");

  prebuilt->lob_undo_reset();

  ut_a(!trx->has_search_latch);

  return err;
}

/** Count rows in a R-Tree leaf level.
 @return DB_SUCCESS if successful */
dberr_t row_count_rtree_recs(
    row_prebuilt_t *prebuilt, /*!< in: prebuilt struct for the
                              table handle; this contains the info
                              of search_tuple, index; if search
                              tuple contains 0 fields then we
                              position the cursor at the start or
                              the end of the index, depending on
                              'mode' */
    ulint *n_rows,            /*!< out: number of entries
                              seen in the consistent read */
    ulint *n_dups)            /*!< out: number of dup entries */
{
  dict_index_t *index = prebuilt->index;
  dberr_t ret = DB_SUCCESS;
  mtr_t mtr;
  mem_heap_t *heap;
  dtuple_t *entry;
  dtuple_t *search_entry = prebuilt->search_tuple;
  ulint entry_len;
  ulint i;
  byte *buf;
  bool is_dup = false;

  ut_a(dict_index_is_spatial(index));

  *n_rows = 0;
  *n_dups = 0;

  heap = mem_heap_create(256, UT_LOCATION_HERE);

  /* Build a search tuple. */
  entry_len = dict_index_get_n_fields(index);
  entry = dtuple_create(heap, entry_len);

  for (i = 0; i < entry_len; i++) {
    const dict_field_t *ind_field = index->get_field(i);
    const dict_col_t *col = ind_field->col;
    dfield_t *dfield = dtuple_get_nth_field(entry, i);

    if (i == 0) {
      double *mbr;
      double tmp_mbr[SPDIMS * 2];

      dfield->type.mtype = DATA_GEOMETRY;
      dfield->type.prtype |= DATA_GIS_MBR;

      /* Allocate memory for mbr field */
      mbr = static_cast<double *>(mem_heap_alloc(heap, DATA_MBR_LEN));

      /* Set mbr field data. */
      dfield_set_data(dfield, mbr, DATA_MBR_LEN);

      for (uint j = 0; j < SPDIMS; j++) {
        tmp_mbr[j * 2] = DBL_MAX;
        tmp_mbr[j * 2 + 1] = -DBL_MAX;
      }
      dfield_write_mbr(dfield, tmp_mbr);
      continue;
    }

    dfield->type.mtype = col->mtype;
    dfield->type.prtype = col->prtype;
  }

  prebuilt->search_tuple = entry;

  ulint bufsize = std::max(UNIV_PAGE_SIZE, prebuilt->mysql_row_len);
  buf = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, bufsize));

  ulint cnt = 1000;

  ut_ad(!index->table->is_intrinsic());

  ret = row_search_mvcc(buf, PAGE_CUR_WITHIN, prebuilt, 0, 0);

  prebuilt->rtr_info->is_dup = &is_dup;

loop:
  /* Check thd->killed every 1,000 scanned rows */
  if (--cnt == 0) {
    if (trx_is_interrupted(prebuilt->trx)) {
      ret = DB_INTERRUPTED;
      goto func_exit;
    }
    cnt = 1000;
  }

  switch (ret) {
    case DB_SUCCESS:
      break;
    case DB_DEADLOCK:
    case DB_LOCK_TABLE_FULL:
    case DB_LOCK_WAIT_TIMEOUT:
    case DB_INTERRUPTED:
      goto func_exit;
    default:
      /* fall through (this error is ignored by CHECK TABLE) */
    case DB_END_OF_INDEX:
      ret = DB_SUCCESS;
    func_exit:
      /* This may be pointing to a local variable. */
      prebuilt->rtr_info->is_dup = nullptr;

      prebuilt->search_tuple = search_entry;
      ut::free(buf);
      mem_heap_free(heap);

      return (ret);
  }

  *n_rows = *n_rows + 1;
  if (is_dup) {
    *n_dups = *n_dups + 1;
    is_dup = false;
  }

  ret = row_search_mvcc(buf, PAGE_CUR_WITHIN, prebuilt, 0, ROW_SEL_NEXT);

  goto loop;
}

/** Read the AUTOINC column from the current row. If the value is less than
 0 and the type is not unsigned then we reset the value to 0.
 @return value read from the column */
static uint64_t row_search_autoinc_read_column(
    dict_index_t *index, /*!< in: index to read from */
    const rec_t *rec,    /*!< in: current rec */
    ulint col_no,        /*!< in: column number */
    ulint mtype,         /*!< in: column main type */
    bool unsigned_type)  /*!< in: signed or unsigned flag */
{
  ulint len;
  const byte *data;
  uint64_t value;
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;

  rec_offs_init(offsets_);

  offsets =
      rec_get_offsets(rec, index, offsets, col_no + 1, UT_LOCATION_HERE, &heap);

  if (rec_offs_nth_sql_null(index, offsets, col_no)) {
    /* There is no non-NULL value in the auto-increment column. */
    value = 0;
    goto func_exit;
  }

  data = rec_get_nth_field(index, rec, offsets, col_no, &len);

  value = row_parse_int(data, len, mtype, unsigned_type);

func_exit:
  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }

  return (value);
}

/** Get the maximum and non-delete-marked record in an index.
@param[in]      index   Index tree
@param[in,out]  mtr     Mini-transaction (may be committed and restarted)
@return maximum record, page s-latched in mtr
@retval NULL if there are no records, or if all of them are delete-marked */
static const rec_t *row_search_get_max_rec(dict_index_t *index, mtr_t *mtr) {
  btr_pcur_t pcur;
  const rec_t *rec;

  /* Open at the high/right end (false), and init cursor */
  pcur.open_at_side(false, index, BTR_SEARCH_LEAF, true, 0, mtr);

  do {
    const page_t *page;

    page = pcur.get_page();
    rec = page_find_rec_last_not_deleted(page);

    if (page_rec_is_user_rec(rec)) {
      break;
    } else {
      rec = nullptr;
    }
    pcur.move_before_first_on_page();
  } while (pcur.move_to_prev(mtr));

  pcur.close();

  return (rec);
}

/** Read the max AUTOINC value from an index.
 @return DB_SUCCESS if all OK else error code, DB_RECORD_NOT_FOUND if
 column name can't be found in index */
dberr_t row_search_max_autoinc(
    dict_index_t *index,  /*!< in: index to search */
    const char *col_name, /*!< in: name of autoinc column */
    uint64_t *value)      /*!< out: AUTOINC value read */
{
  dict_field_t *dfield = index->get_field(0);
  dberr_t error = DB_SUCCESS;
  *value = 0;

  if (strcmp(col_name, dfield->name) != 0) {
    error = DB_RECORD_NOT_FOUND;
  } else {
    mtr_t mtr;
    const rec_t *rec;

    mtr_start(&mtr);

    rec = row_search_get_max_rec(index, &mtr);

    if (rec != nullptr) {
      bool unsigned_type = (dfield->col->prtype & DATA_UNSIGNED) != 0;

      *value = row_search_autoinc_read_column(index, rec, 0, dfield->col->mtype,
                                              unsigned_type);
    }

    mtr_commit(&mtr);
  }

  return (error);
}

/** Convert the innodb_table_stats clustered index record to
table_stats format.
@param[in]      clust_rec       clustered index record
@param[in]      clust_index     clustered index
@param[in]      clust_offsets   offsets of the clustered index
                                record
@param[out]     tbl_stats       table_stats information
                                to be filled. */
static void convert_to_table_stats_record(rec_t *clust_rec,
                                          dict_index_t *clust_index,
                                          ulint *clust_offsets,
                                          TableStatsRecord &tbl_stats) {
  for (ulint i = 0; i < rec_offs_n_fields(clust_offsets); i++) {
    const byte *data;
    ulint len;
    data = rec_get_nth_field(clust_index, clust_rec, clust_offsets, i, &len);

    if (len == UNIV_SQL_NULL) {
      continue;
    }

    tbl_stats.set_data(data, i, len);
  }
}

/** Search the record present in innodb_table_stats table using
db_name, table_name and fill it in table stats structure.
@param[in]      db_name         database name
@param[in]      tbl_name        table name
@param[out]     table_stats     stats table structure.
@return true if successful else false. */
bool row_search_table_stats(const char *db_name, const char *tbl_name,
                            TableStatsRecord &table_stats) {
  mtr_t mtr;
  btr_pcur_t pcur;
  rec_t *rec;
  bool move = true;
  ulint *offsets;
  dict_table_t *table = dict_sys->table_stats;
  dict_index_t *clust_index = table->first_index();
  dtuple_t *dtuple;
  dfield_t *dfield;
  bool found_rec = false;
  mem_heap_t *heap = mem_heap_create(100, UT_LOCATION_HERE);

  dtuple = dtuple_create(heap, clust_index->n_uniq);
  dict_index_copy_types(dtuple, clust_index, clust_index->n_uniq);

  dfield = dtuple_get_nth_field(dtuple, TableStatsRecord::DB_NAME_COL_NO);
  dfield_set_data(dfield, db_name, strlen(db_name));

  dfield = dtuple_get_nth_field(dtuple, TableStatsRecord::TABLE_NAME_COL_NO);
  dfield_set_data(dfield, tbl_name, strlen(tbl_name));

  mtr_start(&mtr);
  pcur.open_no_init(clust_index, dtuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, 0, &mtr,
                    UT_LOCATION_HERE);

  for (; move == true; move = pcur.move_to_next(&mtr)) {
    rec = pcur.get_rec();
    offsets = rec_get_offsets(rec, clust_index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    if (page_rec_is_infimum(rec) || page_rec_is_supremum(rec)) {
      continue;
    }

    if (0 != cmp_dtuple_rec(dtuple, rec, clust_index, offsets)) {
      break;
    }

    if (rec_get_deleted_flag(rec, dict_table_is_comp(table))) {
      continue;
    }

    found_rec = true;
    convert_to_table_stats_record(rec, clust_index, offsets, table_stats);
  }

  mtr_commit(&mtr);
  mem_heap_free(heap);
  return (found_rec);
}

/** Search the record present in innodb_index_stats using
db_name, table name and index_name and fill the
cardinality for the each column.
@param[in]      db_name         database name
@param[in]      tbl_name        table name
@param[in]      index_name      index name
@param[in]      col_offset      offset of the column in the index
@param[out]     cardinality     cardinality of the column.
@return true if successful else false. */
bool row_search_index_stats(const char *db_name, const char *tbl_name,
                            const char *index_name, ulint col_offset,
                            ulonglong *cardinality) {
  mtr_t mtr;
  btr_pcur_t pcur;
  rec_t *rec;
  bool move = true;
  ulint *offsets;
  dict_table_t *table = dict_sys->index_stats;
  dict_index_t *clust_index = table->first_index();
  dtuple_t *dtuple;
  dfield_t *dfield;
  mem_heap_t *heap = mem_heap_create(100, UT_LOCATION_HERE);
  ulint n_recs = 0;

  /** Number of fields to search in the table. */
  static constexpr unsigned N_SEARCH_FIELDS = 3;
  /** Column number of innodb_index_stats.database_name. */
  static constexpr unsigned DB_NAME_COL_NO = 0;
  /** Column number of innodb_index_stats.table_name. */
  static constexpr unsigned TABLE_NAME_COL_NO = 1;
  /** Column number of innodb_index_stats.index_name. */
  static constexpr unsigned INDEX_NAME_COL_NO = 2;
  /** Column number of innodb_index_stats.stat_value. */
  static constexpr unsigned STAT_VALUE_COL_NO = 5;

  ulint cardinality_index_offset = clust_index->get_col_pos(STAT_VALUE_COL_NO);

  /** Search the innodb_index_stats table using
          (database_name, table_name, index_name). */
  dtuple = dtuple_create(heap, N_SEARCH_FIELDS);
  dict_index_copy_types(dtuple, clust_index, N_SEARCH_FIELDS);

  dfield = dtuple_get_nth_field(dtuple, DB_NAME_COL_NO);
  dfield_set_data(dfield, db_name, strlen(db_name));

  dfield = dtuple_get_nth_field(dtuple, TABLE_NAME_COL_NO);
  dfield_set_data(dfield, tbl_name, strlen(tbl_name));

  dfield = dtuple_get_nth_field(dtuple, INDEX_NAME_COL_NO);
  dfield_set_data(dfield, index_name, strlen(index_name));

  mtr_start(&mtr);
  pcur.open_no_init(clust_index, dtuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, 0, &mtr,
                    UT_LOCATION_HERE);

  for (; move == true; move = pcur.move_to_next(&mtr)) {
    rec = pcur.get_rec();
    offsets = rec_get_offsets(rec, clust_index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    if (page_rec_is_infimum(rec) || page_rec_is_supremum(rec)) {
      continue;
    }

    if (0 != cmp_dtuple_rec(dtuple, rec, clust_index, offsets)) {
      break;
    }

    if (rec_get_deleted_flag(rec, dict_table_is_comp(table))) {
      continue;
    }

    if (n_recs == col_offset) {
      const byte *data;
      ulint len;
      data = rec_get_nth_field(clust_index, rec, offsets,
                               cardinality_index_offset, &len);

      *cardinality = static_cast<ulonglong>(round(mach_read_from_8(data)));
      mtr_commit(&mtr);
      mem_heap_free(heap);
      return (true);
    }

    n_recs++;
  }

  mtr_commit(&mtr);
  mem_heap_free(heap);
  return (false);
}
