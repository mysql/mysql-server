/*****************************************************************************

Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

/** @file ddl/ddl0ctx.cc
 DDL context implementation.
Created 2020-11-01 by Sunny Bains. */

#include "btr0load.h"
#include "clone0api.h"
#include "ddl0fts.h"
#include "ddl0impl-builder.h"
#include "ddl0impl-cursor.h"
#include "ddl0impl-loader.h"
#include "ddl0impl-merge.h"
#include "dict0dd.h"
#include "ha_prototypes.h"
#include "handler0alter.h"
#include "row0log.h"

namespace ddl {

Context::Context(trx_t *trx, dict_table_t *old_table, dict_table_t *new_table,
                 bool online, dict_index_t **indexes, const ulint *key_numbers,
                 size_t n_indexes, TABLE *table, const dtuple_t *add_cols,
                 const ulint *col_map, size_t add_autoinc,
                 ddl::Sequence &sequence, bool skip_pk_sort, Alter_stage *stage,
                 const dict_add_v_col_t *add_v, TABLE *eval_table,
                 size_t max_buffer_size, size_t max_threads) noexcept
    : m_trx(trx),
      m_fts(ddl::fts_parser_threads),
      m_old_table(old_table),
      m_new_table(new_table),
      m_online(online),
      m_table(table),
      m_add_cols(add_cols),
      m_col_map(col_map),
      m_add_autoinc(add_autoinc),
      m_sequence(sequence),
      m_stage(stage),
      m_add_v(add_v),
      m_eval_table(eval_table),
      m_skip_pk_sort(skip_pk_sort),
      m_max_buffer_size(max_buffer_size),
      m_max_threads(max_threads) {
  ut_a(max_threads > 0);

  ut_a(!m_online ||
       trx->isolation_level == trx_t::isolation_level_t::REPEATABLE_READ);

  /* Check if we need a flush observer to flush dirty pages.
  Since we disable redo logging in bulk load, so we should flush
  dirty pages before online log apply, because online log apply enables
  redo logging (we can do further optimization here).

  1. Online add index: flush dirty pages right before row_log_apply().
  2. Table rebuild: flush dirty pages before row_log_table_apply().

  We use bulk load to create all types of indexes except spatial index,
  for which redo logging is enabled. If we create only spatial indexes,
  we don't need to flush dirty pages at all. */
  m_need_observer = m_old_table != m_new_table;

  for (size_t i = 0; i < n_indexes; ++i) {
    m_indexes.push_back(indexes[i]);

    if (i == 0) {
      ut_a(!m_skip_pk_sort || m_indexes.back()->is_clustered());
      m_n_uniq = dict_index_get_n_unique(m_indexes.back());
    }

    if (!dict_index_is_spatial(m_indexes.back())) {
      m_need_observer = true;
    }
    m_key_numbers.push_back(key_numbers[i]);
  }

  ut_a(m_trx->mysql_thd != nullptr);
  ut_a(m_add_cols == nullptr || m_col_map != nullptr);
  ut_a((m_old_table == m_new_table) == (m_col_map == nullptr));

  trx_start_if_not_started_xa(m_trx, true, UT_LOCATION_HERE);

  if (m_need_observer) {
    const auto space_id = m_new_table->space;

    auto observer = ut::new_withkey<Flush_observer>(
        ut::make_psi_memory_key(mem_key_ddl), space_id, m_trx, m_stage);

    trx_set_flush_observer(m_trx, observer);
  }

  mutex_create(LATCH_ID_DDL_AUTOINC, &m_autoinc_mutex);

  m_trx->error_key_num = ULINT_UNDEFINED;

  if (m_add_cols != nullptr) {
    m_dtuple_heap = mem_heap_create(512, UT_LOCATION_HERE);
    ut_a(m_dtuple_heap != nullptr);
  }
}

Context::~Context() noexcept {
  if (m_dtuple_heap != nullptr) {
    ut_a(m_add_cols != nullptr);
    mem_heap_free(m_dtuple_heap);
  }
  mutex_destroy(&m_autoinc_mutex);
}

Context::FTS::Sequence::~Sequence() noexcept {}

Flush_observer *Context::flush_observer() noexcept {
  return m_trx->flush_observer;
}

THD *Context::thd() noexcept {
  ut_a(m_trx->mysql_thd != nullptr);
  return m_trx->mysql_thd;
}

const dict_index_t *Context::index() const noexcept {
  return m_old_table->first_index();
}

Context::Scan_buffer_size Context::scan_buffer_size(
    size_t n_threads) const noexcept {
  ut_a(n_threads > 0);
  auto n_buffers{n_threads};

  /* If there is an FTS index being built, take that into account. */
  if (m_fts.m_ptr != nullptr) {
    n_buffers *= FTS_NUM_AUX_INDEX;
  } else {
    n_buffers *= m_indexes.size();
  }

  /* The maximum size of the record is considered to be srv_page_size/2,
  because one B-tree node should be able to hold at least 2 records. But there
  is also an i/o alignment requirement of IO_BLOCK_SIZE.  This means that the
  min io buffer size should be the sum of these two.  Refer to
  Key_sort_buffer::serialize() function and its write() lambda function to
  understand the reasoning behind this.  */
  const auto min_io_size = (srv_page_size / 2) + IO_BLOCK_SIZE;

  /* A single row *must* fit into an IO block. The IO buffer should be
  greater than the IO physical size buffer makes it easier to handle
  FS block aligned writes. */
  const auto io_block_size = IO_BLOCK_SIZE + ((IO_BLOCK_SIZE * 25) / 100);
  const auto io_size = std::max(size_t(min_io_size), io_block_size);

  Scan_buffer_size size{m_max_buffer_size / n_buffers, io_size};

  if (size.first <= 64 * 1024) {
    if (size.first < srv_page_size) {
      size.first = srv_page_size;
    } else if (size.first >= size.second * 2) {
      size.first -= size.second;
    }
  } else {
    if (size.first >= 2 * 1024 * 1024) {
      size.second = 1024 * 1024;
    } else if (size.first >= 1024 * 1024) {
      size.second = 512 * 1024;
    } else if (size.first >= 512 * 1024) {
      size.second = 128 * 1024;
    } else if (size.first >= 256 * 1024) {
      size.second = 64 * 1024;
    } else {
      size.second = 32 * 1024;
    }
    size.first -= size.second;
  }

  return size;
}

size_t Context::merge_io_buffer_size(size_t n_buffers) const noexcept {
  ut_a(n_buffers > 0);

  const auto io_size = load_io_buffer_size(n_buffers);

  /* We aim to do IO_BLOCK_SIZE writes all the time. */
  ut_a(!(io_size % IO_BLOCK_SIZE));

  return std::max(std::max((ulong)srv_page_size, (ulong)IO_BLOCK_SIZE),
                  (ulong)io_size);
}

size_t Context::load_io_buffer_size(size_t n_buffers) const noexcept {
  ut_a(n_buffers > 0);
  const auto io_size = m_max_buffer_size / n_buffers;

  return std::max(std::max((ulong)srv_page_size, (ulong)IO_BLOCK_SIZE),
                  (ulong)((io_size / IO_BLOCK_SIZE) * IO_BLOCK_SIZE));
}

bool Context::has_virtual_columns() const noexcept {
  if (m_add_v != nullptr || dict_index_has_virtual(index())) {
    return true;
  }

  /* Find out if there are any virtual coumns defined on the table. */
  for (size_t i = 0; i < m_table->s->fields; ++i) {
    if (innobase_is_v_fld(m_table->field[i])) {
      return true;
    }
  }

  return false;
}

dberr_t Context::handle_autoinc(const dtuple_t *dtuple) noexcept {
  ut_ad(m_add_autoinc != ULINT_UNDEFINED);
  ut_ad(m_add_autoinc < m_new_table->get_n_user_cols());

  const auto dfield = dtuple_get_nth_field(dtuple, m_add_autoinc);

  if (dfield_is_null(dfield)) {
    return DB_SUCCESS;
  }

  const auto dtype = dfield_get_type(dfield);
  const auto b = static_cast<byte *>(dfield_get_data(dfield));

  if (m_sequence.eof()) {
    m_trx->error_key_num = SERVER_CLUSTER_INDEX_ID;
    return DB_AUTOINC_READ_ERROR;
  }

  mutex_enter(&m_autoinc_mutex);

  auto value = m_sequence++;

  mutex_exit(&m_autoinc_mutex);

  switch (dtype_get_mtype(dtype)) {
    case DATA_INT: {
      auto len = dfield_get_len(dfield);
      auto usign = dtype_get_prtype(dtype) & DATA_UNSIGNED;
      mach_write_ulonglong(b, value, len, usign);
      break;
    }

    case DATA_FLOAT:
      mach_float_write(b, static_cast<float>(value));
      break;

    case DATA_DOUBLE:
      mach_double_write(b, static_cast<double>(value));
      break;

    default:
      ut_d(ut_error);
  }

  return DB_SUCCESS;
}

dberr_t Context::fts_create(dict_index_t *index) noexcept {
  /* There can only be one FTS index per table. */
  ut_a(m_fts.m_ptr == nullptr);

  m_fts.m_ptr = ut::new_withkey<ddl::FTS>(ut::make_psi_memory_key(mem_key_ddl),
                                          *this, index, m_old_table);

  if (m_fts.m_ptr != nullptr) {
    return m_fts.m_ptr->init(m_fts.m_n_parser_threads);
  } else {
    return DB_OUT_OF_MEMORY;
  }
}

dberr_t Context::cleanup(dberr_t err) noexcept {
  ut_a(err == m_err);

  if (m_err != DB_SUCCESS &&
      m_err_key_number != std::numeric_limits<size_t>::max()) {
    m_trx->error_key_num = m_err_key_number;
  }

  if (m_online && m_old_table == m_new_table && err != DB_SUCCESS) {
    /* On error, flag all online secondary index creation as aborted. */
    for (auto index : m_indexes) {
      ut_a(!index->is_committed());
      ut_a(!index->is_clustered());
      ut_a(!(index->type & DICT_FTS));

      /* Completed indexes should be dropped as well, and indexes whose
      creation was aborted should be dropped from the persistent storage.
      However, at this point we can only set some flags in the
      not-yet-published indexes. These indexes will be dropped later in
      drop_indexes(), called by rollback_inplace_alter_table(). */

      auto latch = dict_index_get_lock(index);

      switch (dict_index_get_online_status(index)) {
        case ONLINE_INDEX_COMPLETE:
          break;
        case ONLINE_INDEX_CREATION:
          rw_lock_x_lock(latch, UT_LOCATION_HERE);
          row_log_abort_sec(index);
          index->type |= DICT_CORRUPT;
          rw_lock_x_unlock(latch);
          m_new_table->drop_aborted = true;
          [[fallthrough]];
        case ONLINE_INDEX_ABORTED:
        case ONLINE_INDEX_ABORTED_DROPPED:
          break;
      }
    }
  }

  DBUG_EXECUTE_IF("ib_index_crash_after_bulk_load", DBUG_SUICIDE(););

  auto observer = m_trx->flush_observer;

  if (observer != nullptr) {
    ut_a(m_need_observer);

    DBUG_EXECUTE_IF("ib_index_build_fail_before_flush", err = DB_FAIL;);

    if (err != DB_SUCCESS) {
      observer->interrupted();
    }

    observer->flush();

    ut::delete_(observer);

    m_trx->flush_observer = nullptr;

    auto space_id = m_new_table != nullptr ? m_new_table->space
                                           : dict_sys_t::s_invalid_space_id;

    /* Notify clone after flushing all pages. */
    Clone_notify notifier(Clone_notify::Type::SPACE_ALTER_INPLACE_BULK,
                          space_id, false);

    if (notifier.failed()) {
      err = DB_ERROR;

    } else if (is_interrupted()) {
      err = DB_INTERRUPTED;
    }

    if (err == DB_SUCCESS) {
      auto first_index = m_new_table->first_index();

      for (auto index = first_index; index != nullptr; index = index->next()) {
        if (m_old_table != m_new_table) {
          Builder::write_redo(index);
        }
      }
    }
  }

  return err;
}

void Context::setup_nonnull() noexcept {
  ut_a(m_nonnull.empty());

  if (m_old_table == m_new_table) {
    return;
  }

  /* The table is being rebuilt.  Identify the columns
  that were flagged NOT nullptr in the new table, so that
  we can quickly check that the records in the old table
  do not violate the added NOT nullptr constraints. */

  for (size_t i = 0; i < m_old_table->get_n_cols(); ++i) {
    if (m_old_table->get_col(i)->prtype & DATA_NOT_NULL) {
      continue;
    }

    const auto col_no = m_col_map[i];

    if (col_no == ULINT_UNDEFINED) {
      /* The column was dropped. */
      continue;
    }

    if (m_new_table->get_col(col_no)->prtype & DATA_NOT_NULL) {
      m_nonnull.push_back(col_no);
    }
  }
}

bool Context::check_null_constraints(const dtuple_t *row) const noexcept {
  for (const auto i : m_nonnull) {
    auto field = &row->fields[i];

    ut_ad(dfield_get_type(field)->prtype & DATA_NOT_NULL);

    if (dfield_is_null(field)) {
      return false;
    }
  }

  return true;
}

bool Context::has_fts_indexes() const noexcept {
  if (dict_table_has_fts_index(m_old_table)) {
    return true;
  }

  for (auto index : m_indexes) {
    if (index->type & DICT_FTS) {
      return true;
    }
  }
  return false;
}

dberr_t Context::setup_fts_build() noexcept {
  for (auto index : m_indexes) {
    if (!(index->type & DICT_FTS)) {
      continue;
    }

    /* There can be only one FTS index on a table. */
    auto err = fts_create(index);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  return DB_SUCCESS;
}

dberr_t Context::check_state_of_online_build_log() noexcept {
  if (m_online && m_old_table != m_new_table) {
    const auto err = row_log_table_get_error(index());

    if (err != DB_SUCCESS) {
      m_trx->error_key_num = SERVER_CLUSTER_INDEX_ID;
      return err;
    }
  }

  return DB_SUCCESS;
}

void Context::note_max_trx_id(dict_index_t *index) noexcept {
  if (!m_online || m_new_table != m_old_table) {
    return;
  }

  auto rw_latch = dict_index_get_lock(index);

  rw_lock_x_lock(rw_latch, UT_LOCATION_HERE);

  ut_a(dict_index_get_online_status(index) == ONLINE_INDEX_CREATION);

  const auto max_trx_id = row_log_get_max_trx(index);

  if (max_trx_id > index->trx_id) {
    index->trx_id = max_trx_id;
  }

  rw_lock_x_unlock(rw_latch);
}

dberr_t Context::setup_pk_sort(Cursor *cursor) noexcept {
  if (m_skip_pk_sort) {
    return cursor->setup_pk_sort(m_n_uniq);
  } else {
    return DB_SUCCESS;
  }
}

dberr_t Context::read_init(Cursor *cursor) noexcept {
  ut_a(m_cursor == nullptr);

  m_cursor = cursor;
  setup_nonnull();

  return setup_pk_sort(cursor);
}

dberr_t Context::build() noexcept {
  Loader loader{*this};

  const auto err = cleanup(loader.build_all());

  /* Validate the indexes  after the pages have been flushed to disk.
  Otherwise we can deadlock between flushing and is_free page check. */
  ut_ad(err != DB_SUCCESS || loader.validate_indexes());

  return err;
}

bool Context::is_interrupted() noexcept { return trx_is_interrupted(m_trx); }

dtuple_t *Context::create_add_cols() noexcept {
  ut_a(m_add_cols != nullptr);
  ut_a(m_dtuple_heap != nullptr);

  auto dtuple = dtuple_copy(m_add_cols, m_dtuple_heap);

  for (size_t i = 0; i < m_add_cols->n_fields; ++i) {
    dfield_dup(&dtuple->fields[i], m_dtuple_heap);
  }

  return dtuple;
}

Sequence::Sequence(THD *thd, ulonglong start_value,
                   ulonglong max_value) noexcept
    : m_max_value(max_value), m_next_value(start_value) {
  if (thd != nullptr && m_max_value > 0) {
    thd_get_autoinc(thd, &m_offset, &m_increment);

    if (m_increment > 1 || m_offset > 1) {
      /* If there is an offset or increment specified
      then we need to work out the exact next value. */

      m_next_value = innobase_next_autoinc(start_value, 1, m_increment,
                                           m_offset, m_max_value);

    } else if (start_value == 0) {
      /* The next value can never be 0. */
      m_next_value = 1;
    }
  } else {
    m_eof = true;
  }
}

ulonglong Sequence::operator++(int) noexcept {
  const auto current = m_next_value;

  ut_ad(!m_eof);
  ut_ad(m_max_value > 0);

  m_next_value =
      innobase_next_autoinc(current, 1, m_increment, m_offset, m_max_value);

  if (m_next_value == m_max_value && current == m_next_value) {
    m_eof = true;
  }

  return current;
}
}  // namespace ddl
