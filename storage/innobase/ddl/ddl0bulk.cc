/*****************************************************************************

Copyright (c) 2022, Oracle and/or its affiliates.

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

/** @file ddl/ddl0bulk.cc
BULK Data Load. Currently treated like DDL */

#include "ddl0bulk.h"
#include <cstdint>
#include "btr0load.h"
#include "field_types.h"
#include "mach0data.h"
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

dberr_t Loader::begin(const row_prebuilt_t *prebuilt) {
  dict_table_t *table = prebuilt->table;
  m_table = table;
  dict_index_t *primary_key = table->first_index();

  m_ctxs.resize(m_num_threads);

  /* Initialze thread specific data and create sub-tree loaders. */
  for (size_t index = 0; index < m_num_threads; ++index) {
    m_ctxs[index].init(prebuilt);

    auto sub_tree_load =
        ut::new_withkey<Btree_load>(ut::make_psi_memory_key(mem_key_ddl),
                                    primary_key, prebuilt->trx, nullptr);
    sub_tree_load->init();
    sub_tree_load->set_cached_range(4);
    m_sub_tree_loads.push_back(sub_tree_load);
  }

  /* Optimize space extension for bulk operation. */
  fil_space_t *space = fil_space_acquire(table->space);
  space->begin_bulk_operation(auto_extend_size());
  fil_space_release(space);

  return DB_SUCCESS;
}

uint64_t Loader::auto_extend_size() {
  /* Extend with larger size for larger number of threads. */
  uint64_t value = m_num_threads * S_BULK_EXTEND_SIZE_MIN;

  if (value < S_BULK_EXTEND_SIZE_MIN) {
    value = S_BULK_EXTEND_SIZE_MIN;
  }
  if (value > S_BULK_EXTEND_SIZE_MAX) {
    value = S_BULK_EXTEND_SIZE_MAX;
  }
  return value;
}

dberr_t Loader::load(const row_prebuilt_t *prebuilt, size_t thread_index,
                     const Rows_mysql &rows) {
  ut_a(thread_index < m_sub_tree_loads.size());
  auto sub_tree = m_sub_tree_loads[thread_index];

  ut_a(thread_index < m_ctxs.size());
  auto &ctx = m_ctxs[thread_index];

  return ctx.load(prebuilt, sub_tree, rows);
}

dberr_t Loader::Thread_data::load(const row_prebuilt_t *prebuilt,
                                  Btree_load *sub_tree,
                                  const Rows_mysql &rows) {
  m_err = DB_SUCCESS;

  for (size_t row_index = 0; row_index < rows.get_num_rows(); ++row_index) {
    m_err = fill_tuple(prebuilt, rows, row_index);

    if (m_err == DB_SUCCESS) {
      fill_index_entry(prebuilt);
      m_err = sub_tree->insert(m_entry, 0);

      if (m_err == DB_DATA_NOT_SORTED) {
        m_errcode = ER_LOAD_BULK_DATA_UNSORTED;
      }
    }

    if (m_err != DB_SUCCESS) {
      dict_table_t *table = prebuilt->table;
      dict_index_t *index = table->first_index();
      LogErr(WARNING_LEVEL, ER_IB_BULK_LOAD_THREAD_FAIL,
             "ddl_bulk::Loader::Thread_data::load()", (unsigned long)m_err,
             table->name.m_name, index->name());
      /* This error message would be sent to client.  */
      m_sout << "err=" << m_err << ", table=" << table->name.m_name
             << ", index=" << index->name();
      break;
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
  dberr_t db_err = DB_SUCCESS;

  bool is_subtree = (m_num_threads > 1);

  for (auto sub_tree_load : m_sub_tree_loads) {
    db_err = sub_tree_load->finish(db_err, is_subtree);
  }

  /* Merge all the sub-trees. The rollback action is in case of an error would
  be performed in post ddl action and would also be executed in case of crash
  recovery. */
  if (!is_error && is_subtree) {
    db_err = merge_sutrees(prebuilt);
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

  /* This function is a miniatre of row_ins_index_entry_set_vals(). */
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

  /* This function is a miniatre of row_mysql_convert_row_to_innobase(). */
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
      ib::info(ER_IB_MSG_1381, "Innodb row has more columns than CSV");
      return DB_ERROR;
    }
    auto &sql_col = rows.read_column(row_offset, column_number);
    ++column_number;

    if (templ->mysql_null_bit_mask != 0 && sql_col.m_is_null) {
      dfield_set_null(dfield);
      continue;
    }

    auto dtype = dfield_get_type(dfield);
    auto data_ptr = (byte *)sql_col.m_data_ptr;
    auto data_len = sql_col.m_data_len;

    /* For integer data, the column is passed as integer and not in mysql
    format. We use the empty column buffer to store colum in innobase format. */
    if (dtype->mtype == DATA_INT) {
      store_int_col(dfield, sql_col, data_ptr, data_len);
      continue;
    }

    row_mysql_store_col_in_innobase_format(dfield, nullptr, true, data_ptr,
                                           data_len,
                                           dict_table_is_comp(prebuilt->table));
  }
  return DB_SUCCESS;
}

void Loader::Thread_data::store_int_col(dfield_t *dfield,
                                        const Column_mysql &col, byte *data_ptr,
                                        size_t data_len) {
  if (col.m_type == MYSQL_TYPE_TINY) {
    ut_a(data_len == sizeof(uint8_t));
    mach_write_to_1(data_ptr, (uint8_t)col.m_int_data);
  } else if (col.m_type == MYSQL_TYPE_SHORT) {
    ut_a(data_len == sizeof(uint16_t));
    mach_write_to_2(data_ptr, (uint16_t)col.m_int_data);
  } else if (col.m_type == MYSQL_TYPE_INT24) {
    ut_a(data_len == 3);
    const uint32_t val = (uint32_t)col.m_int_data & 0x00FFFFFF;
    mach_write_to_3(data_ptr, val);
  } else if (col.m_type == MYSQL_TYPE_LONG) {
    ut_a(data_len == sizeof(uint32_t));
    mach_write_to_4(data_ptr, (uint32_t)col.m_int_data);
  } else if (col.m_type == MYSQL_TYPE_LONGLONG) {
    ut_a(data_len == sizeof(uint64_t));
    mach_write_to_8(data_ptr, col.m_int_data);
  } else {
    ut_a(data_len <= sizeof(uint64_t));
    byte temp_buffer[sizeof(uint64_t)];
    byte *data = data_ptr;

    size_t index = data_len;
    while (index > 0) {
      --index;
      temp_buffer[index] = *data;
      ++data;
    }
    memcpy(data_ptr, temp_buffer, data_len);
  }

  auto dtype = dfield_get_type(dfield);

  if (!(dtype->prtype & DATA_UNSIGNED)) {
    *data_ptr ^= 128;
  }
  dfield_set_data(dfield, data_ptr, data_len);
}

dberr_t Loader::merge_sutrees(const row_prebuilt_t *prebuilt) {
  auto primary_index = prebuilt->table->first_index();

  Btree_load::Merger merger(m_sub_tree_loads, primary_index, prebuilt->trx);
  auto db_err = merger.merge(false);
  return db_err;
}

}  // namespace ddl_bulk
