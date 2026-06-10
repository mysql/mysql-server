/*****************************************************************************

Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

/** @file ddl/ddl0bulk.cc
BULK Data Load. Currently treated like DDL */

#include "ddl0bulk.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include "api0api.h"
#include "btr0mtib.h"
#include "current_thd.h"
#include "data0data.h"
#include "data0type.h"
#include "db0err.h"
#include "dict0stats.h"
#include "field_types.h"
#include "lob0lob.h"
#include "mach0data.h"
#include "mem0mem.h"
#include "mysql/components/services/bulk_data_service.h"
#include "scope_guard.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/sql_table.h"
#include "trx0roll.h"
#include "trx0sys.h"
#include "trx0undo.h"

namespace ddl_bulk {

/** Fill the tuple to set the column data
@param[in]      tuple               target tuple to fill
@param[in]      prebuilt            prebuilt structures from innodb table
handler
@param[in]      rows                sql rows with column data
@param[in]      row_index           current row index
@param[in,out]  last_rowid          last used row_id
@param[in]      row_id_data         row_id buffer
@param[in,out]  subtrees            list of subtree builds
@param[in]      queue_size          the write queue size used
@param[in]      gcol_heap           memory heap used for generated columns
@param[in,out]  gcol_blobs_flushed  true if blobs are flushed, false
otherwise.  This is needed only when we have gcol on blobs.
@return innodb error code. */
static dberr_t fill_tuple(dtuple_t *tuple, const row_prebuilt_t *prebuilt,
                          const Rows_mysql &rows, size_t row_index,
                          uint64_t &last_rowid, unsigned char *row_id_data,
                          std::list<Btree_multi::Btree_load *> &subtrees,
                          size_t queue_size, mem_heap_t *gcol_heap,
                          bool &gcol_blobs_flushed);

/** Fill the tuple to set the column data
@param[in]      tuple               target tuple to fill
@param[in]      prebuilt            prebuilt structures from innodb table
handler
@param[in]      rows                sql rows with column data
@param[in]      row_index           current row index
@param[in]      n_cols              number of columns in tuple to fill
@param[in,out]  last_rowid         last used row_id
@param[in]      row_id_data         row_id buffer
@param[in,out]  subtrees            list of subtree builds
@param[in]      queue_size          the write queue size used
@param[in]      allocate_subtree    whether to allocate subtrees, only set to
true when filling data tuples, false when filling cmp and search tuples for
existing table.
@param[in]      gcol_heap           memory heap used for generated columns
@param[in,out]  gcol_blobs_flushed  true if blobs are flushed, false
otherwise.  This is needed only when we have gcol on blobs.
@param[in,out]  validate_gcols      whether to validate gcols, false when
building search and cmp tuples for existing table
@return innodb error code. */
static dberr_t fill_tuple_up_to_n_cols(
    dtuple_t *tuple, const row_prebuilt_t *prebuilt, const Rows_mysql &rows,
    size_t row_index, size_t n_cols, uint64_t &last_rowid,
    unsigned char *row_id_data, std::list<Btree_multi::Btree_load *> &subtrees,
    size_t queue_size, bool allocate_subtree, mem_heap_t *gcol_heap,
    bool &gcol_blobs_flushed, bool validate_gcols);

/** Fill system columns for index entry to be loaded.
@param[in, out] entry     the target index entry tuple
@param[in]  prebuilt      prebuilt structures from innodb table handler,
@param[in]  trx_data      transaction id buffer
@param[in]  rollptr_data  roll_ptr buffer
@param[in]  row_id_data   row_id buffer */
static void fill_system_columns(dtuple_t *entry, const row_prebuilt_t *prebuilt,
                                unsigned char *trx_data,
                                unsigned char *rollptr_data,
                                unsigned char *row_id_data);

/** Fill he cluster index entry from tuple data.
@param[in]  entry         target index entry to fill
@param[in]  tuple         source tuple containing user field data
@param[in]  prebuilt      prebuilt structures from innodb table handler
@param[in]  trx_data      transaction id buffer
@param[in]  rollptr_data  roll_ptr buffer
@param[in]  row_id_data   row_id buffer
@param[in]  fill_sys_cols if the target index entry system columns should
be filled.*/
static void fill_index_entry(dtuple_t *entry, const dtuple_t *tuple,
                             const row_prebuilt_t *prebuilt,
                             unsigned char *trx_data,
                             unsigned char *rollptr_data,
                             unsigned char *row_id_data,
                             bool fill_sys_cols = true);

/** Sets up a dfield_t structure for a generated field based on src_dfield and
user data in sql_col
@param[in]  prebuilt      prebuilt structures from innodb table handler
@param[in]  field         field metadata
@param[in]  sql_col       sql column with user data
@param[in]  src_dfield    dfield_t containing the computed gcol value
@param[out] dst_dfield    target dfield_t
@return innodb error code.
*/
static dberr_t setup_dfield(const row_prebuilt_t *prebuilt, Field *field,
                            const Column_mysql &sql_col, dfield_t *src_dfield,
                            dfield_t *dst_dfield);

/** Store integer column in Innodb format.
@param[in]      col       sql column data
@param[in,out]  data_ptr  data buffer for storing converted data
@param[in,out]  data_len  data buffer length
@return true if successful. */
static bool store_int_col(const Column_mysql &col, byte *data_ptr,
                          size_t &data_len);

void Loader::Thread_data::init(const row_prebuilt_t *prebuilt) {
  dict_table_t *table = prebuilt->table;
  dict_index_t *index = prebuilt->index;

  current_thd = prebuilt->m_thd;

  /* Create tuple heap and the empty tuple. */
  m_heap = mem_heap_create(1024, UT_LOCATION_HERE);

  if (index->is_clustered()) {
    auto n_table_cols = table->get_n_cols();
    const ulint n_v_cols = dict_table_get_n_v_cols(table);
    m_input_row = dtuple_create_with_vcol(m_heap, n_table_cols, n_v_cols);
    dict_table_copy_types(m_input_row, index->table);
  } else {
    auto n_index_cols = dict_index_get_n_fields(index);
    m_input_row = dtuple_create(m_heap, n_index_cols);
    dict_index_copy_types(m_input_row, index, n_index_cols);
  }

  /* Create the cluster index tuple to be inserted. */
  auto n_index_cols = dict_index_get_n_fields(index);
  auto n_unique = dict_index_get_n_unique_in_tree(index);
  m_input_entry = dtuple_create(m_heap, n_index_cols);
  m_original_table_entry = dtuple_create(m_heap, n_index_cols);
  dict_index_copy_types(m_input_entry, index, n_index_cols);
  dtuple_set_n_fields_cmp(m_input_entry, n_unique);
  dict_index_copy_types(m_original_table_entry, index, n_index_cols);
  dtuple_set_n_fields_cmp(m_original_table_entry, n_unique);

  trx_start_if_not_started(prebuilt->trx, true, UT_LOCATION_HERE);

  if (index->is_clustered()) {
    /* Fill the system column data. Set INSERT flag for MVCC. */
    auto roll_ptr = trx_undo_build_roll_ptr(true, 0, 0, 0);
    trx_write_trx_id(m_trx_data, prebuilt->trx->id);
    trx_write_roll_ptr(m_rollptr_data, roll_ptr);
  }
}

bool Loader::Thread_data::set_source_table_data(
    const row_prebuilt_t *prebuilt,
    const Bulk_load::Source_table_data &source_table_data) {
  m_original_table_name = source_table_data.table;
  return m_table_reader.init(source_table_data.schema, source_table_data.table,
                             prebuilt, source_table_data.range.first,
                             source_table_data.range.second);
}

void Loader::get_queue_size(size_t memory, size_t &flush_queue_size,
                            bool &allocate_in_pages) const {
  allocate_in_pages = false;
  size_t memory_per_thread = memory / m_num_threads;

  const size_t extent_size = FSP_EXTENT_SIZE * UNIV_PAGE_SIZE;
  const size_t memory_in_extents = memory_per_thread / extent_size;

  /* We maintain 2 queues. One extent can be half filled at any time for each
  level of B-tree. Also, we allocate one extent before adding the current one
  to flush queue - We take a safe margin of 4x. */
  flush_queue_size = memory_in_extents / 4;

  const size_t max_flush_queue_size = 16;
  const size_t min_flush_queue_size = 2;

  flush_queue_size = std::min(max_flush_queue_size, flush_queue_size);

  if (flush_queue_size < min_flush_queue_size) {
    allocate_in_pages = true;
    flush_queue_size = 4;
  }
}

dberr_t Loader::begin(const row_prebuilt_t *prebuilt, size_t data_size,
                      size_t memory) {
  dict_table_t *table = prebuilt->table;
  m_table = table;
  m_index = prebuilt->index;

  m_ctxs.resize(m_num_threads);

  m_queue_size = 2;
  bool in_pages = false;
  get_queue_size(memory, m_queue_size, in_pages);

  /* Initialize thread specific data and create sub-tree loaders. */
  for (size_t index = 0; index < m_num_threads; ++index) {
    m_ctxs[index].init(prebuilt);
    m_ctxs[index].m_queue_size = m_queue_size;

    auto sub_tree_load = ut::new_withkey<Btree_multi::Btree_load>(
        ut::make_psi_memory_key(mem_key_ddl), m_index, prebuilt->trx, index,
        m_queue_size, m_extent_allocator);
    sub_tree_load->init();
    m_ctxs[index].add_subtree(sub_tree_load);
  }

  auto extend_size = m_extent_allocator.init(
      table, m_index, prebuilt->trx, data_size, m_num_threads, in_pages);

  /* Optimize space extension for bulk operation. */
  fil_space_t *space = fil_space_acquire(table->space);
  space->begin_bulk_operation(extend_size);
  fil_space_release(space);

  if (extend_size > 0) {
    m_extent_allocator.start();
  }
  return DB_SUCCESS;
}

dberr_t Loader::load(const row_prebuilt_t *prebuilt, size_t thread_index,
                     const Rows_mysql &rows,
                     Bulk_load::Stat_callbacks &wait_cbk) {
  ut_a(thread_index < m_ctxs.size());
  auto &ctx = m_ctxs[thread_index];
  auto sub_tree = ctx.get_subtree();
  // Take a mutex so that only one thread can evaluate gcol.
  std::unique_lock<std::mutex> lock(m_gcol_mutex, std::defer_lock);
  if (prebuilt->has_gcol()) {
    lock.lock();
  }

  return ctx.load(prebuilt, sub_tree, rows, wait_cbk);
}

dberr_t Loader::copy_existing_data(const row_prebuilt_t *prebuilt,
                                   size_t thread_index,
                                   Bulk_load::Stat_callbacks &wait_cbk) {
  ut_a(thread_index < m_ctxs.size());
  auto &ctx = m_ctxs[thread_index];
  auto sub_tree = ctx.get_subtree();

  return ctx.copy_existing_data(prebuilt, sub_tree, wait_cbk);
}

bool Loader::set_source_table_data(
    const row_prebuilt_t *prebuilt,
    const std::vector<Bulk_load::Source_table_data> &source_table_data) {
  m_original_table_name = source_table_data.at(0).table;
  assert(source_table_data.size() == m_num_threads);
  for (size_t index = 0; index < m_num_threads; ++index) {
    auto success = m_ctxs[index].set_source_table_data(
        prebuilt, source_table_data.at(index));
    if (!success) {
      return false;
    }
  }
  return true;
}

dberr_t Loader::open_blob(size_t thread_index, Blob_context &blob_ctx,
                          lob::ref_t &ref) {
  ut_ad(thread_index < m_ctxs.size());
  auto &ctx = m_ctxs[thread_index];
  auto sub_tree = ctx.get_subtree();

  return ctx.open_blob(sub_tree, blob_ctx, ref);
}

dberr_t Loader::write_blob(size_t thread_index, Blob_context blob_ctx,
                           lob::ref_t &ref, const byte *data, size_t len) {
  ut_ad(thread_index < m_ctxs.size());
  auto &ctx = m_ctxs[thread_index];
  auto sub_tree = ctx.get_subtree();

  return ctx.write_blob(sub_tree, blob_ctx, ref, data, len);
}

dberr_t Loader::close_blob(size_t thread_index, Blob_context blob_ctx,
                           lob::ref_t &ref) {
  ut_ad(thread_index < m_ctxs.size());
  auto &ctx = m_ctxs[thread_index];
  auto sub_tree = ctx.get_subtree(blob_ctx);

  ut_ad(sub_tree->verify_blob_context(blob_ctx));

  return ctx.close_blob(sub_tree, blob_ctx, ref);
}

static inline std::string print_int_field(dfield_t *field) {
  long long total{};
  if (field->data == nullptr || dfield_is_null(field) != 0) {
    return "NULL";
  }
  for (size_t i = 0; i < field->len; ++i) {
    total = (total << 8) + ((byte *)field->data)[i];
  }
  const dtype_t *type = dfield_get_type(field);
  bool is_unsigned = (dtype_get_prtype(type) & DATA_UNSIGNED) != 0;

  if (is_unsigned) {
    return std::to_string(total);
  }
  long long mask = 0x80;
  mask = (mask << ((field->len - 1) * 8));
  total -= mask;
  return std::to_string(total);
}

static inline std::string print_dfield_hex(const dfield_t *field) {
  const auto *data = static_cast<const unsigned char *>(dfield_get_data(field));
  if (data == nullptr || dfield_is_null(field) != 0) {
    return "NULL";
  }
  size_t len = dfield_get_len(field);
  std::stringstream out;
  out << "0x";
  for (size_t i = 0; i < len; ++i) {
    out << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(data[i]);
  }
  return out.str();
}

void Loader::Thread_data::read_input_entry(const Rows_mysql &rows,
                                           size_t &row_index,
                                           const row_prebuilt_t *prebuilt,
                                           mem_heap_t *gcol_heap,
                                           bool &gcol_blobs_flushed) {
  if (row_index >= rows.get_num_rows()) {
    m_more_available_in_input = false;
    return;
  }

  m_err = fill_tuple(m_input_row, prebuilt, rows, row_index, m_last_rowid,
                     m_rowid_data, m_list_subtrees, m_queue_size, gcol_heap,
                     gcol_blobs_flushed);

  if (m_err != DB_SUCCESS) {
    return;
  }
  fill_index_entry(m_input_entry, m_input_row, prebuilt, m_trx_data,
                   m_rollptr_data, m_rowid_data);
  m_more_available_in_input = true;
}

void Loader::Thread_data::read_table_entry(const row_prebuilt_t *prebuilt) {
  if (m_table_reader.is_initialized() &&
      m_table_reader.more_records_available()) {
    m_table_row = ib_tuple_to_dtuple(m_table_reader.read());

    if (prebuilt->index->is_clustered() &&
        prebuilt->clust_index_was_generated) {
      auto *col = prebuilt->table->get_sys_col(DATA_ROW_ID);
      auto *dfield = dtuple_get_nth_field(m_table_row, col->ind);
      memcpy(m_rowid_data, dfield->data, DATA_ROW_ID_LEN);
    }
    fill_index_entry(m_original_table_entry, m_table_row, prebuilt, m_trx_data,
                     m_rollptr_data, m_rowid_data);
    m_more_available_in_original_table = true;
  } else {
    m_more_available_in_original_table = false;
  }
}

void Loader::Thread_data::insert_from_input_and_move_to_next(
    const row_prebuilt_t *prebuilt, const Rows_mysql &rows, size_t &row_index,
    Btree_multi::Btree_load *sub_tree, mem_heap_t *gcol_heap,
    bool &gcol_blobs_flushed) {
  if (prebuilt->index->is_clustered() && prebuilt->clust_index_was_generated) {
    /* For this thread, a new subtree could have been created while
    processing the row id. Use the latest subtree for loading data. */
    sub_tree = get_subtree();
  }

  m_err = sub_tree->insert(m_input_entry, 0);
  mem_heap_empty(gcol_heap);
  if (m_err != DB_SUCCESS) {
    m_error_entry = m_input_entry;
    return;
  }
  ++row_index;
  read_input_entry(rows, row_index, prebuilt, gcol_heap, gcol_blobs_flushed);
}

void Loader::Thread_data::insert_from_original_table_and_move_to_next(
    const row_prebuilt_t *prebuilt, Btree_multi::Btree_load *sub_tree) {
  m_err = sub_tree->insert(m_original_table_entry, 0);
  if (m_err != DB_SUCCESS) {
    m_error_entry = m_original_table_entry;
    return;
  }
  m_table_reader.next();
  read_table_entry(prebuilt);
}

void Loader::Thread_data::insert_smaller_entry(
    const row_prebuilt_t *prebuilt, Btree_multi::Btree_load *sub_tree,
    const Rows_mysql &rows, size_t &row_index,
    const ddl::Compare_key &compare_key, mem_heap_t *gcol_heap,
    bool &gcol_blobs_flushed) {
  auto cmp_result =
      compare_key(m_input_entry->fields, m_original_table_entry->fields);

  if (cmp_result == 0) {
    m_err = DB_DUPLICATE_KEY;
    m_error_entry = m_input_entry;
    return;
  }
  if (cmp_result > 0) {
    insert_from_original_table_and_move_to_next(prebuilt, sub_tree);
  } else {
    insert_from_input_and_move_to_next(prebuilt, rows, row_index, sub_tree,
                                       gcol_heap, gcol_blobs_flushed);
  }
}

dberr_t Loader::Thread_data::load(const row_prebuilt_t *prebuilt,
                                  Btree_multi::Btree_load *sub_tree,
                                  const Rows_mysql &rows,
                                  Bulk_load::Stat_callbacks &wait_cbk) {
  m_err = DB_SUCCESS;
  size_t row_index = 0;
  ddl::Compare_key compare_key(prebuilt->index, nullptr,
                               !prebuilt->index->is_clustered() &&
                                   !dict_index_is_unique(prebuilt->index));
  /* memory heap for generated columns */
  mem_heap_t *gcol_heap = mem_heap_create(128, UT_LOCATION_HERE);
  auto guard = create_scope_guard([gcol_heap]() { mem_heap_free(gcol_heap); });

  /* Blobs must be flushed before gcol evaluation is done. */
  bool gcol_blobs_flushed = false;
  read_input_entry(rows, row_index, prebuilt, gcol_heap, gcol_blobs_flushed);
  if (!prebuilt->index->is_clustered() ||
      !prebuilt->clust_index_was_generated) {
    // skip generated clustered index here, we will migrate all of the data in
    // the copy_existing_data call after we process the full CSV input.
    read_table_entry(prebuilt);
  }

  while (m_more_available_in_input) {
    if (m_more_available_in_original_table) {
      insert_smaller_entry(prebuilt, sub_tree, rows, row_index, compare_key,
                           gcol_heap, gcol_blobs_flushed);
    } else {
      insert_from_input_and_move_to_next(prebuilt, rows, row_index, sub_tree,
                                         gcol_heap, gcol_blobs_flushed);
    }
    if (m_err != DB_SUCCESS) {
      break;
    }
  }

  Btree_multi::Btree_load::Wait_callbacks cbk_set(sub_tree, wait_cbk.m_fn_begin,
                                                  wait_cbk.m_fn_end);

  if (m_err == DB_SUCCESS) {
    /* Trigger flusher before getting out. Also, check and report
    flusher error. */
    m_err = sub_tree->trigger_flusher();
    if (m_err == DB_SUCCESS) {
      return DB_SUCCESS;
    }
  }

  std::string table_name;
  if (m_original_table_name.has_value()) {
    table_name = m_original_table_name.value();
  } else {
    dict_table_t *table = prebuilt->table;
    table_name = table->name.m_name;
  }
  dict_index_t *index = prebuilt->index;
  LogErr(INFORMATION_LEVEL, ER_IB_BULK_LOAD_THREAD_FAIL,
         "ddl_bulk::Loader::Thread_data::load()", (unsigned long)m_err,
         table_name.c_str(), index->name());

  switch (m_err) {
    case DB_DATA_NOT_SORTED:
      m_errcode = ER_LOAD_BULK_DATA_UNSORTED;
      break;
    case DB_INTERRUPTED:
      m_errcode = ER_QUERY_INTERRUPTED;
      break;
    case DB_DUPLICATE_KEY:
      m_errcode = ER_DUP_ENTRY_WITH_KEY_NAME;
      break;
    case DB_OUT_OF_MEMORY:
      m_errcode = ER_LOAD_BULK_DATA_FAILED;
      m_sout << "Innodb: memory allocation failed.";
      break;
    case DB_OUT_OF_DISK_SPACE:
    case DB_OUT_OF_FILE_SPACE:
      m_errcode = ER_LOAD_BULK_DATA_FAILED;
      m_sout << "Innodb: disk space allocation failed.";
      break;
    case DB_IO_ERROR:
      m_errcode = ER_LOAD_BULK_DATA_FAILED;
      m_sout << "Innodb: disk write failed.";
      break;
    case DB_BULK_TOO_BIG_RECORD: {
      m_errcode = ER_LOAD_BULK_DATA_FAILED;
      // TODO fix error converted size call
      auto rec_size = rec_get_converted_size(index, m_input_entry);
      m_sout << "Innodb: Record size: " << rec_size
             << " too big to fit a Page.";
      break;
    }
    case DB_BULK_GCOL_INVALID_DATA: {
      m_errcode = ER_LOAD_BULK_DATA_FAILED;
      m_sout << "Innodb: data for generated column is invalid";
      break;
    }
    default:
      m_errcode = ER_LOAD_BULK_DATA_FAILED;
      /* This error message would be sent to client.  */
      m_sout << "Innodb Error= " << m_err << "(" << ut_strerr(m_err)
             << "), table=" << table_name.c_str()
             << ", index=" << index->name();
      break;
  }

  if (m_err != DB_DUPLICATE_KEY && m_err != DB_DATA_NOT_SORTED) {
    return m_err;
  }

  size_t n_cols = dtuple_get_n_fields_cmp(m_error_entry);
  size_t n_unique = dict_index_get_n_unique(prebuilt->index);
  n_cols = std::min(n_cols, n_unique);
  if (m_err == DB_DATA_NOT_SORTED) {
    m_sout << "Key: ";
  }

  for (size_t field_index = 0; field_index < n_cols; field_index++) {
    if (field_index >= dtuple_get_n_fields(m_error_entry)) {
      break;
    }
    auto *field = dtuple_get_nth_field(m_error_entry, field_index);
    auto *dtype = dfield_get_type(field);

    if (dtype->mtype == DATA_INT) {
      m_sout << print_int_field(field);
    } else if (dtype->mtype == DATA_CHAR || dtype->mtype == DATA_VARCHAR ||
               dtype->mtype == DATA_MYSQL || dtype->mtype == DATA_VARMYSQL) {
      std::string data_str(static_cast<const char *>(field->data), field->len);
      m_sout << data_str.c_str();
    } else if (dtype->mtype == DATA_DOUBLE) {
      auto d = mach_double_read(static_cast<const byte *>(field->data));
      m_sout << d;
    } else if (dtype->mtype == DATA_FLOAT) {
      auto f = mach_float_read(static_cast<const byte *>(field->data));
      m_sout << f;
    } else {
      m_sout << print_dfield_hex(field);
    }
    if (field_index + 1 != n_cols) {
      m_sout << ",";
    }
  }
  return m_err;
}

dberr_t Loader::Thread_data::copy_existing_data(
    const row_prebuilt_t *prebuilt, Btree_multi::Btree_load *sub_tree,
    Bulk_load::Stat_callbacks &wait_cbk) {
  if (prebuilt->index->is_clustered() && prebuilt->clust_index_was_generated) {
    auto *tmp = m_list_subtrees.back();
    auto sub_tree_load = ut::new_withkey<Btree_multi::Btree_load>(
        ut::make_psi_memory_key(mem_key_ddl), prebuilt->index, prebuilt->trx, 0,
        m_queue_size, tmp->get_extent_allocator());
    sub_tree_load->init();
    m_list_subtrees.push_back(sub_tree_load);
  }

  read_table_entry(prebuilt);
  while (m_more_available_in_original_table) {
    sub_tree = get_subtree();
    insert_from_original_table_and_move_to_next(prebuilt, sub_tree);

    switch (m_err) {
      case DB_DATA_NOT_SORTED:
        m_errcode = ER_LOAD_BULK_DATA_UNSORTED;
        break;
      case DB_INTERRUPTED:
        m_errcode = ER_QUERY_INTERRUPTED;
        break;
      case DB_DUPLICATE_KEY:
        m_errcode = ER_DUP_ENTRY_WITH_KEY_NAME;
        break;
      case DB_OUT_OF_MEMORY:
        m_errcode = ER_LOAD_BULK_DATA_FAILED;
        m_sout << "Innodb: memory allocation failed.";
        break;
      case DB_OUT_OF_DISK_SPACE:
      case DB_OUT_OF_FILE_SPACE:
        m_errcode = ER_LOAD_BULK_DATA_FAILED;
        m_sout << "Innodb: disk space allocation failed.";
        break;
      case DB_IO_ERROR:
        m_errcode = ER_LOAD_BULK_DATA_FAILED;
        m_sout << "Innodb: disk write failed.";
        break;
      case DB_BULK_TOO_BIG_RECORD: {
        m_errcode = ER_LOAD_BULK_DATA_FAILED;
        // TODO fix error converted size call
        auto rec_size = rec_get_converted_size(prebuilt->index, m_input_entry);
        m_sout << "Innodb: Record size: " << rec_size
               << " too big to fit a Page.";
        break;
      }
      case DB_BULK_GCOL_INVALID_DATA: {
        m_errcode = ER_LOAD_BULK_DATA_FAILED;
        m_sout << "Innodb: data for generated column is invalid";
        break;
      }
      default:
        m_errcode = ER_LOAD_BULK_DATA_FAILED;
        /* This error message would be sent to client.  */
        m_sout << "Innodb Error= " << m_err << "(" << ut_strerr(m_err)
               << "), table=" << prebuilt->table->name.m_name
               << ", index=" << prebuilt->index->name();
        break;
    }
    if (m_err != DB_SUCCESS) {
      break;
    }
  }

  Btree_multi::Btree_load::Wait_callbacks cbk_set(sub_tree, wait_cbk.m_fn_begin,
                                                  wait_cbk.m_fn_end);

  if (m_err == DB_SUCCESS) {
    /* Trigger flusher before getting out. Also, check and report
    flusher error. */
    m_err = sub_tree->trigger_flusher();
  }
  return m_err;
}

void Loader::Thread_data::free() {
  /* Free the tuple memory */
  mem_heap_free(m_heap);
  m_input_row = nullptr;
  m_table_row = nullptr;
  m_input_entry = nullptr;
  m_original_table_entry = nullptr;
}

dberr_t Loader::end(bool is_error) {
  dberr_t db_err = DB_SUCCESS;

  uint64_t max_rowid{0};
  for (size_t index = 0; index < m_num_threads; ++index) {
    auto &thd_ctx = m_ctxs[index];
    if (thd_ctx.m_last_rowid > max_rowid) {
      max_rowid = thd_ctx.m_last_rowid;
    }
    for (auto subtree : thd_ctx.m_list_subtrees) {
      m_sub_tree_loads.push_back(subtree);
    }
  }
  bool is_subtree = (m_sub_tree_loads.size() > 1);

  for (auto sub_tree_load : m_sub_tree_loads) {
    auto finish_err = sub_tree_load->finish(is_error, is_subtree);
    /* Save the first error. */
    if (finish_err != DB_SUCCESS && db_err == DB_SUCCESS) {
      is_error = true;
      db_err = finish_err;
    }
  }

  m_extent_allocator.stop();

  /* Merge all the sub-trees. The rollback action is in case of an error would
  be performed in post ddl action and would also be executed in case of crash
  recovery. */
  if (!is_error && is_subtree) {
    db_err = merge_subtrees();
  }

  for (auto &ctx : m_ctxs) {
    ctx.free();
  }
  m_ctxs.clear();

  /* Free sub-tree loaders. */
  for (auto sub_tree_load : m_sub_tree_loads) {
    ut::delete_(sub_tree_load);
  }

  m_sub_tree_loads.clear();

  if (!m_table->has_pk()) {
    set_sys_max_rowid(max_rowid);
  }

  fil_space_t *space = fil_space_acquire(m_table->space);
  space->end_bulk_operation();
  fil_space_release(space);

  return db_err;
}

void Loader::set_sys_max_rowid(uint64_t max_rowid) {
  dict_sys_set_min_next_row_id(max_rowid + 1);
}

void fill_system_columns(dtuple_t *entry, const row_prebuilt_t *prebuilt,
                         unsigned char *trx_data, unsigned char *rollptr_data,
                         unsigned char *row_id_data) {
  dict_index_t *primary_key = prebuilt->table->first_index();

  ut_ad(primary_key != nullptr);

  if (!dict_index_is_unique(primary_key)) {
    auto rowid_pos = primary_key->get_sys_col_pos(DATA_ROW_ID);
    auto dfield = dtuple_get_nth_field(entry, rowid_pos);
    dfield_set_data(dfield, row_id_data, DATA_ROW_ID_LEN);
  }

  /* Set transaction ID system column. */
  auto trx_id_pos = primary_key->get_sys_col_pos(DATA_TRX_ID);
  auto trx_id_field = dtuple_get_nth_field(entry, trx_id_pos);
  dfield_set_data(trx_id_field, trx_data, DATA_TRX_ID_LEN);

  /* Set roll pointer system column. */
  auto roll_ptr_pos = primary_key->get_sys_col_pos(DATA_ROLL_PTR);
  auto roll_ptr_field = dtuple_get_nth_field(entry, roll_ptr_pos);
  dfield_set_data(roll_ptr_field, rollptr_data, DATA_ROLL_PTR_LEN);
}

void fill_index_entry(dtuple_t *entry, const dtuple_t *tuple,
                      const row_prebuilt_t *prebuilt, unsigned char *trx_data,
                      unsigned char *rollptr_data, unsigned char *row_id_data,
                      bool fill_sys_cols) {
  dict_index_t *index = prebuilt->index;

  /* This function is a miniature of row_ins_index_entry_set_vals(). */
  auto n_fields = dtuple_get_n_fields(entry);
  for (size_t nth_field = 0; nth_field < n_fields; nth_field++) {
    auto field = dtuple_get_nth_field(entry, nth_field);

    auto column_number =
        index->is_clustered() ? index->get_col_no(nth_field) : nth_field;

    auto row_field = dtuple_get_nth_field(tuple, column_number);
    auto data = dfield_get_data(row_field);
    auto data_len = dfield_get_len(row_field);

    dfield_set_data(field, data, data_len);
    /* TODO:
     1. Handle external field
     2. Handle prefix index. */
    if (row_field->is_ext()) {
      if (!index->is_clustered()) {
        /* sec indexes cannot contain external fields. */
        char query[1024];
        memset(query, '\0', sizeof query);
        size_t len = innobase_get_stmt_safe(prebuilt->trx->mysql_thd, query,
                                            sizeof(query));
        std::cerr << "table=" << index->table_name << std::endl;
        std::cerr << "index=" << index->name() << std::endl;
        std::cerr << "query=" << query << std::endl;
        std::cerr << "query_len=" << len << std::endl;

        ut_a(index->is_clustered());
      }
      dfield_set_ext(field);
    }
  }
  if (index->is_clustered() && fill_sys_cols) {
    fill_system_columns(entry, prebuilt, trx_data, rollptr_data, row_id_data);
  }
}

dberr_t setup_dfield(const row_prebuilt_t *prebuilt, Field *field,
                     const Column_mysql &sql_col, dfield_t *src_dfield,
                     dfield_t *dst_dfield) {
  const space_id_t space_id = prebuilt->space_id();
  auto dtype = dfield_get_type(src_dfield);
  auto data_ptr = (byte *)sql_col.get_data();
  size_t data_len = sql_col.m_data_len;

  dst_dfield->type = src_dfield->type;

  /* For integer data, the column is passed as integer and not in mysql
  format. We use empty column buffer to store column in innobase format. */
  if (dtype->mtype == DATA_INT) {
    const bool is_stored_gcol = field->is_gcol() && !field->is_virtual_gcol();
    if (!is_stored_gcol) {
      /* In the case of stored gcol sql_col is already converted to innodb
      format. Don't do it again. */
      if (!store_int_col(sql_col, data_ptr, data_len)) {
        ib::info(ER_BULK_LOADER_INFO, "Innodb wrong integer data length");
        return DB_ERROR;
      }
      if (!(dtype->prtype & DATA_UNSIGNED)) {
        *data_ptr ^= 128;
      }
    }
    dfield_set_data(dst_dfield, data_ptr, data_len);
  } else if (dtype->mtype == DATA_BLOB || dtype->mtype == DATA_GEOMETRY) {
    auto field_str = (const Field_str *)field;
    const CHARSET_INFO *field_charset = field_str->charset();
    size_t length_size{0};
    switch (sql_col.m_type) {
      case MYSQL_TYPE_TINY_BLOB:
        length_size = 1;
        break;
      case MYSQL_TYPE_BLOB:
        length_size = 2;
        break;
      case MYSQL_TYPE_MEDIUM_BLOB:
        length_size = 3;
        break;
      case MYSQL_TYPE_GEOMETRY:
        [[fallthrough]];
      case MYSQL_TYPE_JSON:
        [[fallthrough]];
      case MYSQL_TYPE_VECTOR:
        [[fallthrough]];
      case MYSQL_TYPE_LONG_BLOB:
        length_size = 4;
        break;
      default:
        assert(0);
        break;
    }
    byte *field_data = data_ptr + length_size;
    dfield_set_data(dst_dfield, field_data, data_len);
    if (data_len == lob::ref_t::SIZE) {
      lob::ref_t ref(field_data);
      if (ref.space_id() == space_id) {
        dfield_set_ext(dst_dfield);
      } else {
        /* Not an externally stored field.  So, validate the string. */
        size_t valid_length{0};
        bool length_error;

        char *tmp = reinterpret_cast<char *>(field_data);

        const bool failure = validate_string(field_charset, tmp, data_len,
                                             &valid_length, &length_error);
        if (failure) {
          my_error(ER_INVALID_CHARACTER_STRING, MYF(0), field_charset->csname,
                   field_data);
          return DB_ERROR;
        }
      }
    }
  } else if ((dtype->mtype == DATA_VARMYSQL || dtype->mtype == DATA_BINARY) &&
             data_len == lob::ref_t::SIZE) {
    byte *field_data = data_ptr;
    dfield_set_data(dst_dfield, field_data, data_len);
    lob::ref_t ref(field_data);
    if (ref.space_id() == space_id) {
      dfield_set_ext(dst_dfield);
    } else {
      /* Not an externally stored field. */
    }
  } else if (dtype->mtype == DATA_SYS) {
    ut_ad(0);
  } else {
    assert(data_len <= dtype->len);
    dfield_set_data(dst_dfield, data_ptr, data_len);
  }

  return DB_SUCCESS;
}

dberr_t fill_tuple_up_to_n_cols(dtuple_t *tuple, const row_prebuilt_t *prebuilt,
                                const Rows_mysql &rows, size_t row_index,
                                size_t n_cols, uint64_t &last_rowid,
                                unsigned char *row_id_data,
                                std::list<Btree_multi::Btree_load *> &subtrees,
                                size_t queue_size, bool allocate_subtree,
                                mem_heap_t *gcol_heap, bool &gcol_blobs_flushed,
                                bool validate_gcols) {
  ut_ad(prebuilt->mysql_template);
  const space_id_t space_id = prebuilt->space_id();
  TABLE *mysql_table = prebuilt->m_mysql_table;
  THD *thd = prebuilt->m_thd;
  auto share = mysql_table->s;
  dict_table_t *table = prebuilt->table;

  /* This function is a miniature of row_mysql_convert_row_to_innobase(). */

  /* The column_number is used to access column in given rows. */
  size_t column_number = 0;

  auto row_offset = rows.get_row_offset(row_index);
  auto row_size = n_cols;
  size_t start_column_number = 0;

  if (prebuilt->clust_index_was_generated) {
    if (prebuilt->index->is_clustered()) {
      start_column_number = 1;
      auto &sql_col = rows.read_column(row_offset, column_number);
      if (allocate_subtree && last_rowid > 0 &&
          sql_col.m_int_data - last_rowid > 1) {
        auto *tmp = subtrees.back();
        auto sub_tree_load = ut::new_withkey<Btree_multi::Btree_load>(
            ut::make_psi_memory_key(mem_key_ddl), prebuilt->index,
            prebuilt->trx, 0, queue_size, tmp->get_extent_allocator());
        sub_tree_load->init();
        gcol_blobs_flushed = false;
        subtrees.push_back(sub_tree_load);
      }
      mach_write_to_6(row_id_data, sql_col.m_int_data);
      if (allocate_subtree) {
        last_rowid = sql_col.m_int_data;
      }
      auto col = prebuilt->table->get_sys_col(DATA_ROW_ID);
      auto dfield = dtuple_get_nth_field(tuple, col->ind);
      dfield_set_data(dfield, row_id_data, DATA_ROW_ID_LEN);
      column_number++;
      n_cols--;
    } else {
      // secondary index.
    }
  }

  /* Used to access fields of m_row tuple. */
  size_t tuple_index = 0;

  for (size_t index = 0; index < n_cols; index++, ++column_number) {
    // Note: For the generated rowid there is no associated field.
    auto *field = share->field[index];

    if (field != nullptr && field->is_virtual_gcol()) {
      continue;
    }

    auto dfield = dtuple_get_nth_field(tuple, tuple_index);

    ut_ad(column_number < row_size);

    ++tuple_index;

    if (column_number >= row_size) {
      ib::info(ER_BULK_LOADER_INFO, "Innodb row has more columns than CSV");
      return DB_ERROR;
    }

    auto &sql_col = rows.read_column(row_offset, column_number);

    if (sql_col.m_is_null) {
      dfield_set_null(dfield);
      continue;
    }

    auto dtype = dfield_get_type(dfield);
    auto data_ptr = (byte *)sql_col.get_data();
    size_t data_len = sql_col.m_data_len;

    /* For integer data, the column is passed as integer and not in mysql
    format. We use empty column buffer to store column in innobase format. */
    if (dtype->mtype == DATA_INT) {
      if (!store_int_col(sql_col, data_ptr, data_len)) {
        ib::info(ER_BULK_LOADER_INFO, "Innodb wrong integer data length");
        ut_ad(false);
        return DB_ERROR;
      }
      if (!(dtype->prtype & DATA_UNSIGNED)) {
        *data_ptr ^= 128;
      }
      dfield_set_data(dfield, data_ptr, data_len);
    } else if (dtype->mtype == DATA_BLOB || dtype->mtype == DATA_GEOMETRY) {
      auto field_str = (const Field_str *)field;
      const CHARSET_INFO *field_charset = field_str->charset();
      size_t length_size{0};
      switch (sql_col.m_type) {
        case MYSQL_TYPE_TINY_BLOB:
          length_size = 1;
          break;
        case MYSQL_TYPE_BLOB:
          length_size = 2;
          break;
        case MYSQL_TYPE_MEDIUM_BLOB:
          length_size = 3;
          break;
        case MYSQL_TYPE_GEOMETRY:
          [[fallthrough]];
        case MYSQL_TYPE_JSON:
          [[fallthrough]];
        case MYSQL_TYPE_VECTOR:
          [[fallthrough]];
        case MYSQL_TYPE_LONG_BLOB:
          length_size = 4;
          break;
        default:
          assert(0);
          break;
      }
      byte *field_data = data_ptr + length_size;
      dfield_set_data(dfield, field_data, data_len);
      if (data_len == lob::ref_t::SIZE) {
        lob::ref_t ref(field_data);
        if (ref.space_id() == space_id) {
          dfield_set_ext(dfield);
        } else {
          /* Not an externally stored field.  So, validate the string. */
          size_t valid_length{0};
          bool length_error;

          char *tmp = reinterpret_cast<char *>(field_data);

          const bool failure = validate_string(field_charset, tmp, data_len,
                                               &valid_length, &length_error);
          if (failure) {
            my_error(ER_INVALID_CHARACTER_STRING, MYF(0), field_charset->csname,
                     field_data);
            return DB_ERROR;
          }
        }
      }
    } else if ((dtype->mtype == DATA_VARMYSQL || dtype->mtype == DATA_BINARY) &&
               data_len == lob::ref_t::SIZE) {
      byte *field_data = data_ptr;
      dfield_set_data(dfield, field_data, data_len);
      lob::ref_t ref(field_data);
      if (ref.space_id() == space_id) {
        dfield_set_ext(dfield);
      } else {
        /* Not an externally stored field. */
      }
    } else if (dtype->mtype == DATA_SYS) {
      ut_ad(!prebuilt->index->is_clustered());
      mach_write_to_6(row_id_data, sql_col.m_int_data);
      dfield_set_data(dfield, row_id_data, DATA_ROW_ID_LEN);
    } else {
      assert(data_len <= dtype->len);
      dfield_set_data(dfield, data_ptr, data_len);
    }
  }

  /* Validation of gcol is done only for clustered index. */
  if (prebuilt->index->is_clustered() && validate_gcols) {
    size_t nth_v_col = 0;
    column_number = start_column_number;

    for (size_t index = 0; index < n_cols; index++, ++column_number) {
      auto *field = share->field[index];

      if (field->is_virtual_gcol()) {
        dict_v_col_t *const col = dict_table_get_nth_v_col(table, nth_v_col++);
        dfield_t *fld1 = innobase_get_computed_value(
            tuple, col, table, &gcol_heap, gcol_heap, thd, mysql_table, nullptr,
            nullptr, nullptr);

        if (fld1 == nullptr) {
          return DB_BULK_GCOL_INVALID_DATA;
        }

        auto &sql_col = rows.read_column(row_offset, column_number);
        dfield_t fld2;
        auto err = setup_dfield(prebuilt, field, sql_col, fld1, &fld2);

        if (err != DB_SUCCESS) {
          return err;
        }

        byte *data2 = static_cast<byte *>(fld2.data);
        size_t data2_len = dfield_get_len(&fld2);

        if (fld2.is_ext()) {
          if (!gcol_blobs_flushed) {
            auto *sub_tree = subtrees.back();
            sub_tree->add_blobs_to_bulk_flusher();
            gcol_blobs_flushed = true;
          }
          const page_size_t page_size(dict_table_page_size(prebuilt->table));
          data2 = lob::btr_copy_externally_stored_field(
              prebuilt->trx, prebuilt->table->first_index(), &data2_len,
              nullptr, reinterpret_cast<const byte *>(fld2.data), page_size,
              dfield_get_len(&fld2), false, gcol_heap);
        }

        if (cmp_data_data(fld1->type.mtype, fld1->type.prtype, true,
                          static_cast<const byte *>(fld1->data),
                          dfield_get_len(fld1), data2, data2_len) != 0) {
          return DB_BULK_GCOL_INVALID_DATA;
        }
      }
    }

    /* Validate the data for the stored gcols. */
    if (table->s_cols != nullptr) {
      for (auto &col : *table->s_cols) {
        auto column_number = col.s_pos + start_column_number;
        auto *field = share->field[col.s_pos];

        dfield_t *fld1 = innobase_compute_stored_gcol(
            tuple, col, table, gcol_heap, thd, mysql_table);

        if (fld1 == nullptr) {
          return DB_BULK_GCOL_INVALID_DATA;
        }

        auto &sql_col = rows.read_column(row_offset, column_number);
        dfield_t fld2;
        auto err = setup_dfield(prebuilt, field, sql_col, fld1, &fld2);

        if (err != DB_SUCCESS) {
          return err;
        }

        byte *data2 = static_cast<byte *>(fld2.data);
        size_t data2_len = dfield_get_len(&fld2);

        if (fld2.is_ext()) {
          if (!gcol_blobs_flushed) {
            auto *sub_tree = subtrees.back();
            sub_tree->add_blobs_to_bulk_flusher();
            gcol_blobs_flushed = true;
          }
          const page_size_t page_size(dict_table_page_size(prebuilt->table));
          data2 = lob::btr_copy_externally_stored_field(
              prebuilt->trx, prebuilt->table->first_index(), &data2_len,
              nullptr, reinterpret_cast<const byte *>(fld2.data), page_size,
              dfield_get_len(&fld2), false, gcol_heap);
        }

        const byte *data1 = static_cast<const byte *>(fld1->data);

        if (cmp_data_data(fld1->type.mtype, fld1->type.prtype, true, data1,
                          dfield_get_len(fld1), data2, data2_len) != 0) {
          if (fld1->type.mtype == DATA_FLOAT) {
            /* In the case of FLOAT data type, the value in the CSV file
            could be rounded up/down and might not match with re-calculated
            value.  So, check approximately.*/
            const float f_1 = mach_float_read(data1);
            const float f_2 = mach_float_read(data2);
            const float epsilon = 0.0001f;
            const float diff = std::abs(f_1 - f_2);
            if (diff <= epsilon) {
              continue;
            }
          }
          return DB_BULK_GCOL_INVALID_DATA;
        }
      }
    }
  }

  return DB_SUCCESS;
}

dberr_t fill_tuple(dtuple_t *tuple, const row_prebuilt_t *prebuilt,
                   const Rows_mysql &rows, size_t row_index,
                   uint64_t &last_rowid, unsigned char *row_id_data,
                   std::list<Btree_multi::Btree_load *> &subtrees,
                   size_t queue_size, mem_heap_t *gcol_heap,
                   bool &gcol_blobs_flushed) {
  const auto n_cols = rows.get_num_cols();
  return fill_tuple_up_to_n_cols(tuple, prebuilt, rows, row_index, n_cols,
                                 last_rowid, row_id_data, subtrees, queue_size,
                                 true, gcol_heap, gcol_blobs_flushed, true);
}

bool store_int_col(const Column_mysql &col, byte *data_ptr, size_t &data_len) {
  switch (col.m_type) {
    case MYSQL_TYPE_LONG:
      if (data_len < sizeof(uint32_t)) {
        return false;
      }
      mach_write_to_4(data_ptr, (uint32_t)col.m_int_data);
      data_len = 4;
      return true;

    case MYSQL_TYPE_LONGLONG:
      if (data_len < sizeof(uint64_t)) {
        return false;
      }
      mach_write_to_8(data_ptr, col.m_int_data);
      data_len = 8;
      return true;

    case MYSQL_TYPE_TINY:
      if (data_len < sizeof(uint8_t)) {
        return false;
      }
      mach_write_to_1(data_ptr, (uint8_t)col.m_int_data);
      data_len = 1;
      return true;

    case MYSQL_TYPE_SHORT:
      if (data_len < sizeof(uint16_t)) {
        return false;
      }
      mach_write_to_2(data_ptr, (uint16_t)col.m_int_data);
      data_len = 2;
      return true;

    case MYSQL_TYPE_INT24: {
      if (data_len < 3) {
        return false;
      }
      const uint32_t val = (uint32_t)col.m_int_data & 0x00FFFFFF;
      mach_write_to_3(data_ptr, val);
      data_len = 3;
      return true;
    }

    default:
      if (data_len > sizeof(uint64_t)) {
        return false;
      }
      break;
  }
  byte temp_buffer[sizeof(uint64_t)];
  byte *data = data_ptr;

  size_t index = data_len;
  while (index > 0) {
    --index;
    temp_buffer[index] = *data;
    ++data;
  }
  memcpy(data_ptr, temp_buffer, data_len);
  return true;
}

dberr_t Loader::merge_subtrees() {
  ut_ad(m_index != nullptr);

  Btree_multi::Btree_load::Merger merger(m_num_threads, m_sub_tree_loads,
                                         m_index, m_trx);
  return merger.merge(true);
}

bool Loader::Table_reader::init(const std::string &schema,
                                const std::string &table,
                                const row_prebuilt_t *prebuilt,
                                const std::optional<Rows_mysql> &lower_bound,
                                const std::optional<Rows_mysql> &upper_bound) {
  assert(!m_initialized);
  m_initialized = true;
  m_table_name = table;
  m_prebuilt = prebuilt;

  m_lower_bound = lower_bound;
  m_upper_bound = upper_bound;

  // For DATA_INT columns we don't allocate a new buffer for col data and
  // store the integer value there for key values generated during data
  // sorting, but rather we use sql_col.m_int_data. However, in
  // store_int_value we use this buffer the actual tuple data so we need
  // to allocate buffers for this here.
  auto allocate_buffers = [](Rows_mysql &dest_bound,
                             const Rows_mysql &src_bound, const dtuple_t *tuple,
                             std::vector<std::unique_ptr<char[]>> &buffers) {
    for (size_t i = 0; i < dtuple_get_n_fields(tuple); ++i) {
      auto col = src_bound.read_column(0, i);
      auto *dfield = dtuple_get_nth_field(tuple, i);
      auto *dtype = dfield_get_type(dfield);
      if (dtype->mtype == DATA_INT) {
        buffers.push_back(std::make_unique<char[]>(col.m_data_len));
        dest_bound.get_column(0, i).set_data(buffers.back().get());
      }
    }
  };

  m_trx = trx_allocate_for_background();
  m_trx->mysql_thd = prebuilt->trx->mysql_thd;
  trx_start_if_not_started(m_trx, true, UT_LOCATION_HERE);

  char path[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path) - 1, schema.c_str(), table.c_str(),
                       nullptr, 0);
  ib_cursor_open_table(path, m_trx, &m_table_cursor);

  if (m_table_cursor == nullptr) {
    return false;
  }

  if (!m_prebuilt->index->is_clustered()) {
    int type;
    ib_id_u64_t id;
    ib_cursor_open_index_using_name(m_table_cursor, m_prebuilt->index->name,
                                    &m_read_cursor, &type, &id);
  } else {
    m_read_cursor = m_table_cursor;
  }
  bool gcols_flushed = false;
  ib_cursor_get_row_prebuilt(m_read_cursor)->clust_index_was_generated =
      prebuilt->clust_index_was_generated;
  ib_cursor_first(m_read_cursor);
  uint64_t last_row_id;
  std::list<Btree_multi::Btree_load *> list_subtrees;
  if (m_lower_bound.has_value()) {
    ib_tpl_t tuple;
    if (!m_prebuilt->index->is_clustered()) {
      tuple = ib_sec_search_tuple_create(m_read_cursor);
    } else {
      tuple = ib_clust_search_tuple_create(m_read_cursor);
    }
    if (tuple == nullptr) {
      return false;
    }

    allocate_buffers(m_lower_bound.value(), lower_bound.value(),
                     ib_tuple_to_dtuple(tuple), m_lower_bound_data);

    unsigned char tuple_row_id_data[DATA_ROW_ID_LEN];
    if (prebuilt->index->is_clustered() &&
        prebuilt->clust_index_was_generated) {
      auto *row_id_field = dtuple_get_nth_field(ib_tuple_to_dtuple(tuple), 0);
      mach_write_to_6(tuple_row_id_data,
                      m_lower_bound->get_column(0, 0).m_int_data);
      dfield_set_data(row_id_field, tuple_row_id_data, DATA_ROW_ID_LEN);
    } else {
      fill_tuple_up_to_n_cols(ib_tuple_to_dtuple(tuple), prebuilt,
                              m_lower_bound.value(), 0,
                              dtuple_get_n_fields(ib_tuple_to_dtuple(tuple)),
                              last_row_id, tuple_row_id_data, list_subtrees, 0,
                              false, nullptr, gcols_flushed, false);
    }

    ib_cursor_moveto(m_read_cursor, tuple, IB_CUR_GE, 0);
    ib_tuple_delete(tuple);
  }

  if (m_upper_bound.has_value()) {
    if (prebuilt->index->is_clustered()) {
      m_cmp_tuple = ib_clust_search_tuple_create(m_read_cursor);
    } else {
      m_cmp_tuple = ib_sec_search_tuple_create(m_read_cursor);
    }
    if (m_cmp_tuple == nullptr) {
      return false;
    }
    allocate_buffers(m_upper_bound.value(), upper_bound.value(),
                     ib_tuple_to_dtuple(m_cmp_tuple), m_upper_bound_data);
    if (prebuilt->index->is_clustered() &&
        prebuilt->clust_index_was_generated) {
      auto *row_id_field =
          dtuple_get_nth_field(ib_tuple_to_dtuple(m_cmp_tuple), 0);
      mach_write_to_6(m_cmp_tuple_row_id_data,
                      m_upper_bound->get_column(0, 0).m_int_data);
      dfield_set_data(row_id_field, m_cmp_tuple_row_id_data, DATA_ROW_ID_LEN);
    } else {
      fill_tuple_up_to_n_cols(
          ib_tuple_to_dtuple(m_cmp_tuple), prebuilt, m_upper_bound.value(), 0,
          std::min(dtuple_get_n_fields(ib_tuple_to_dtuple(m_cmp_tuple)),
                   m_upper_bound->get_num_cols()),
          last_row_id, m_cmp_tuple_row_id_data, list_subtrees, 0, false,
          nullptr, gcols_flushed, false);
    }
  }

  if (prebuilt->index->is_clustered()) {
    m_read_tuple = ib_clust_read_tuple_create(m_read_cursor);
  } else {
    m_read_tuple = ib_sec_search_tuple_create(m_read_cursor);
  }

  auto read_err = ib_cursor_read_row(m_read_cursor, m_read_tuple, m_cmp_tuple,
                                     IB_CUR_L, nullptr, nullptr, nullptr);

  m_more_records_available = read_err == DB_SUCCESS;

  return true;
}

}  // namespace ddl_bulk
