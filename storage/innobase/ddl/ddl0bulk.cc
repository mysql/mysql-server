/*****************************************************************************

Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
#include <cstdint>
#include <iostream>
#include "btr0mtib.h"
#include "dict0stats.h"
#include "field_types.h"
#include "mach0data.h"
#include "trx0roll.h"
#include "trx0sys.h"
#include "trx0undo.h"

namespace ddl_bulk {

void Loader::Thread_data::init(const row_prebuilt_t *prebuilt) {
  dict_table_t *table = prebuilt->table;
  dict_index_t *primary_key = table->first_index();

  /* Create tuple heap and the empty tuple. */
  m_heap = mem_heap_create(1024, UT_LOCATION_HERE);
  auto n_table_cols = table->get_n_cols();
  m_row = dtuple_create(m_heap, n_table_cols);
  dict_table_copy_types(m_row, primary_key->table);

  /* Create the cluster index tuple to be inserted. */
  auto n_index_cols = dict_index_get_n_fields(primary_key);
  auto n_unique = dict_index_get_n_unique_in_tree(primary_key);
  m_entry = dtuple_create(m_heap, n_index_cols);
  dict_index_copy_types(m_entry, primary_key, n_index_cols);
  dtuple_set_n_fields_cmp(m_entry, n_unique);

  trx_start_if_not_started(prebuilt->trx, true, UT_LOCATION_HERE);

  /* Fill the system column data. Set INSERT flag for MVCC. */
  auto roll_ptr = trx_undo_build_roll_ptr(true, 0, 0, 0);
  trx_write_trx_id(m_trx_data, prebuilt->trx->id);
  trx_write_roll_ptr(m_rollptr_data, roll_ptr);
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
  dict_index_t *primary_key = table->first_index();

  m_ctxs.resize(m_num_threads);

  size_t queue_size = 2;
  bool in_pages = false;
  get_queue_size(memory, queue_size, in_pages);

  /* Initialize thread specific data and create sub-tree loaders. */
  for (size_t index = 0; index < m_num_threads; ++index) {
    m_ctxs[index].init(prebuilt);

    auto sub_tree_load = ut::new_withkey<Btree_multi::Btree_load>(
        ut::make_psi_memory_key(mem_key_ddl), primary_key, prebuilt->trx, index,
        queue_size, m_extent_allocator);
    sub_tree_load->init();
    m_sub_tree_loads.push_back(sub_tree_load);
  }

  auto extend_size = m_extent_allocator.init(table, prebuilt->trx, data_size,
                                             m_num_threads, in_pages);

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
  ut_a(thread_index < m_sub_tree_loads.size());
  auto sub_tree = m_sub_tree_loads[thread_index];

  ut_a(thread_index < m_ctxs.size());
  auto &ctx = m_ctxs[thread_index];

  return ctx.load(prebuilt, sub_tree, rows, wait_cbk);
}

dberr_t Loader::Thread_data::load(const row_prebuilt_t *prebuilt,
                                  Btree_multi::Btree_load *sub_tree,
                                  const Rows_mysql &rows,
                                  Bulk_load::Stat_callbacks &wait_cbk) {
  m_err = DB_SUCCESS;
  size_t row_index = 0;

  for (row_index = 0; row_index < rows.get_num_rows(); ++row_index) {
    m_err = fill_tuple(prebuilt, rows, row_index);
    if (m_err != DB_SUCCESS) {
      break;
    }

    Btree_multi::Btree_load::Wait_callbacks cbk_set(
        sub_tree, wait_cbk.m_fn_begin, wait_cbk.m_fn_end);
    fill_index_entry(prebuilt);

    m_err = sub_tree->insert(m_entry, 0);
    if (m_err != DB_SUCCESS) {
      break;
    }
  }

  if (m_err == DB_SUCCESS) {
    /* Trigger flusher before getting out. Also, check and report
    flusher error. */
    m_err = sub_tree->trigger_flusher();
    if (m_err == DB_SUCCESS) {
      return DB_SUCCESS;
    }
  }

  dict_table_t *table = prebuilt->table;
  dict_index_t *index = table->first_index();
  LogErr(INFORMATION_LEVEL, ER_IB_BULK_LOAD_THREAD_FAIL,
         "ddl_bulk::Loader::Thread_data::load()", (unsigned long)m_err,
         table->name.m_name, index->name());

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
      auto rec_size = rec_get_converted_size(index, m_entry);
      m_sout << "Innodb: Record size: " << rec_size
             << " too big to fit a Page.";
      break;
    }
    default:
      m_errcode = ER_LOAD_BULK_DATA_FAILED;
      /* This error message would be sent to client.  */
      m_sout << "Innodb Error= " << m_err << "(" << ut_strerr(m_err)
             << "), table=" << table->name.m_name
             << ", index=" << index->name();
      break;
  }

  if (m_err != DB_DUPLICATE_KEY && m_err != DB_DATA_NOT_SORTED) {
    return m_err;
  }

  auto n_keys = dtuple_get_n_fields_cmp(m_entry);
  auto key_index = n_keys;
  if (m_err == DB_DATA_NOT_SORTED) {
    m_sout << "Key: ";
  }

  auto row_offset = rows.get_row_offset(row_index);
  auto row_size = rows.get_num_cols();

  for (key_index = 0; key_index < n_keys; key_index++) {
    auto field_index = index->get_col_no(key_index);
    if (field_index >= row_size) {
      break;
    }
    auto field = dtuple_get_nth_field(m_row, field_index);
    auto dtype = dfield_get_type(field);
    auto &sql_col = rows.read_column(row_offset, field_index);

    if (dtype->mtype == DATA_INT) {
      if (dtype->prtype & DATA_UNSIGNED) {
        m_sout << " " << sql_col.m_int_data;
      } else {
        m_sout << " " << (int64_t)sql_col.m_int_data;
      }
    } else if (dtype->mtype == DATA_CHAR || dtype->mtype == DATA_VARCHAR ||
               dtype->mtype == DATA_MYSQL || dtype->mtype == DATA_VARMYSQL) {
      std::string data_str(sql_col.m_data_ptr, sql_col.m_data_len);
      m_sout << data_str.c_str();
    }
    if (key_index + 1 != n_keys) {
      m_sout << ",";
    }
  }
  return m_err;
}

void Loader::Thread_data::free() {
  /* Free the tuple memory */
  mem_heap_free(m_heap);
  m_row = nullptr;
  m_entry = nullptr;
}

dberr_t Loader::end(const row_prebuilt_t *prebuilt, bool is_error) {
  bool is_subtree = (m_num_threads > 1);
  dberr_t db_err = DB_SUCCESS;

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
    db_err = merge_subtrees(prebuilt);
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

  dict_table_t *table = prebuilt->table;
  fil_space_t *space = fil_space_acquire(table->space);
  space->end_bulk_operation();
  fil_space_release(space);

  if (db_err == DB_SUCCESS) {
    const dict_stats_upd_option_t option =
        dict_stats_is_persistent_enabled(table) ? DICT_STATS_RECALC_PERSISTENT
                                                : DICT_STATS_RECALC_TRANSIENT;

    const size_t MAX_RETRY = 5;
    for (size_t retry = 0; retry < MAX_RETRY; ++retry) {
      auto savept = trx_savept_take(prebuilt->trx);
      const auto st = dict_stats_update(table, option, prebuilt->trx);

      if (st != DB_SUCCESS) {
        LogErr(WARNING_LEVEL, ER_IB_BULK_LOAD_STATS_WARN,
               "ddl_bulk::Loader::end()", table->name.m_name,
               static_cast<size_t>(st));
        if (st == DB_LOCK_WAIT_TIMEOUT) {
          const auto ms = std::chrono::milliseconds{10 * (1 + retry)};
          std::this_thread::sleep_for(ms);
          trx_rollback_to_savepoint(prebuilt->trx, &savept);
          continue;
        }
      }

      break;
    }
  }

  DBUG_EXECUTE_IF("crash_bulk_load_after_stats", DBUG_SUICIDE(););

  return db_err;
}

void Loader::Thread_data::fill_system_columns(const row_prebuilt_t *prebuilt) {
  dict_index_t *primary_key = prebuilt->table->first_index();

  /* TODO: Handle the case with no primary key. System column : DATA_ROW_ID */
  ut_ad(primary_key != nullptr);
  ut_ad(dict_index_is_unique(primary_key));

  /* Set transaction ID system column. */
  auto trx_id_pos = primary_key->get_sys_col_pos(DATA_TRX_ID);
  auto trx_id_field = dtuple_get_nth_field(m_entry, trx_id_pos);
  dfield_set_data(trx_id_field, m_trx_data, DATA_TRX_ID_LEN);

  /* Set roll pointer system column. */
  auto roll_ptr_pos = primary_key->get_sys_col_pos(DATA_ROLL_PTR);
  auto roll_ptr_field = dtuple_get_nth_field(m_entry, roll_ptr_pos);
  dfield_set_data(roll_ptr_field, m_rollptr_data, DATA_ROLL_PTR_LEN);
}

void Loader::Thread_data::fill_index_entry(const row_prebuilt_t *prebuilt) {
  dict_index_t *primary_key = prebuilt->table->first_index();

  /* This function is a miniature of row_ins_index_entry_set_vals(). */
  auto n_fields = dtuple_get_n_fields(m_entry);

  for (size_t index = 0; index < n_fields; index++) {
    auto field = dtuple_get_nth_field(m_entry, index);

    auto column_number = primary_key->get_col_no(index);
    auto row_field = dtuple_get_nth_field(m_row, column_number);
    auto data = dfield_get_data(row_field);
    auto data_len = dfield_get_len(row_field);

    dfield_set_data(field, data, data_len);
    /* TODO:
     1. Handle external field
     2. Handle prefix index. */
  }
  fill_system_columns(prebuilt);
}

dberr_t Loader::Thread_data::fill_tuple(const row_prebuilt_t *prebuilt,
                                        const Rows_mysql &rows,
                                        size_t row_index) {
  ut_ad(prebuilt->template_type == ROW_MYSQL_WHOLE_ROW);
  ut_ad(prebuilt->mysql_template);

  /* This function is a miniature of row_mysql_convert_row_to_innobase(). */
  size_t column_number = 0;
  auto row_offset = rows.get_row_offset(row_index);
  auto row_size = rows.get_num_cols();

  for (size_t index = 0; index < prebuilt->n_template; index++) {
    const auto templ = prebuilt->mysql_template + index;

    /* Ignore virtual columns. We would insert into cluster index
    and don't support any secondary index yet. */
    if (templ->is_virtual) {
      continue;
    }

    auto dfield = dtuple_get_nth_field(m_row, column_number);
    ut_ad(column_number < row_size);

    if (column_number >= row_size) {
      ib::info(ER_BULK_LOADER_INFO, "Innodb row has more columns than CSV");
      return DB_ERROR;
    }
    auto &sql_col = rows.read_column(row_offset, column_number);
    ++column_number;

    if (sql_col.m_is_null) {
      if (templ->mysql_null_bit_mask == 0) {
        ib::info(ER_BULK_LOADER_INFO,
                 "Innodb: Cannot insert NULL into a not NULL column");
        return DB_ERROR;
      }
      dfield_set_null(dfield);
      continue;
    }

    auto dtype = dfield_get_type(dfield);
    auto data_ptr = (byte *)sql_col.m_data_ptr;
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
    }
    dfield_set_data(dfield, data_ptr, data_len);
  }
  return DB_SUCCESS;
}

bool Loader::Thread_data::store_int_col(const Column_mysql &col, byte *data_ptr,
                                        size_t &data_len) {
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

dberr_t Loader::merge_subtrees(const row_prebuilt_t *prebuilt) {
  auto primary_index = prebuilt->table->first_index();

  Btree_multi::Btree_load::Merger merger(m_sub_tree_loads, primary_index,
                                         prebuilt->trx);
  auto db_err = merger.merge(false);
  return db_err;
}

}  // namespace ddl_bulk
