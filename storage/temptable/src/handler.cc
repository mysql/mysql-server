/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/src/handler.cc
TempTable public handler API implementation. */

#include "storage/temptable/include/temptable/handler.h"

#include <string.h>

#include "my_base.h"
#include "my_dbug.h"
#include "mysql/plugin.h"
#include "sql/mysqld.h"
#include "sql/sql_thd_internal_api.h"
#include "sql/system_variables.h"
#include "storage/temptable/include/temptable/row.h"
#include "storage/temptable/include/temptable/sharded_kv_store.h"
#include "storage/temptable/include/temptable/shared_block_pool.h"

namespace temptable {

/** Key-value store containing all tables for all existing connections.
 * See `Sharded_key_value_store` documentation for more details.
 * */
static Sharded_key_value_store<KV_STORE_SHARDS_COUNT> kv_store_shard;

/* Pool of shared-blocks, an external state to the custom `TempTable` memory
 * allocator. See `Lock_free_shared_block_pool` documentation for more details.
 * */
static Lock_free_shared_block_pool<SHARED_BLOCK_POOL_SIZE> shared_block_pool;

/** Small helper function which debug-prints the miscellaneous statistics which
 * key-value store has collected.
 * */
void kv_store_shards_debug_dump() { kv_store_shard.dbug_print(); }

/** Small helper function which releases the slot (and memory occupied by the
 * Block) in shared-block pool.
 * */
void shared_block_pool_release(THD *thd) {
  shared_block_pool.try_release(thd_thread_id(thd));
}

#if defined(HAVE_WINNUMA)
/** Page size used in memory allocation. */
DWORD win_page_size;
#endif /* HAVE_WINNUMA */

#define DBUG_RET(result) return static_cast<int>(result)

Handler::Handler(handlerton *hton, TABLE_SHARE *table_share_arg)
    : ::handler(hton, table_share_arg),
      m_opened_table(),
      m_shared_block(shared_block_pool.try_acquire(thd_thread_id(ha_thd()))),
      m_rnd_iterator(),
      m_rnd_iterator_is_positioned(),
      m_index_cursor(),
      m_index_read_number_of_cells(),
      m_deleted_rows() {
  handler::ref_length = sizeof(Storage::Element *);

  // Overriding `handlerton::` with a function-pointer in `TempTable`, or
  // any other plugin, is not always sufficient for server to actually invoke
  // that hook. This is the behavior which is in contrast to what one would
  // expect and this is not how most handlerton interfaces work. However, there
  // is a subset of handlerton interfaces which require special care and in
  // addition to overriding the function-pointer, they require initializing
  // `handlerton::ha_data` with some data which is not a `nullptr`.
  //
  // `handlerton::close_connection()` is one of such interfaces, and being
  // relied upon by the `TempTable` implementation, we have to fill in the
  // `ha_data` with some existing data. In this particular case,
  // `&shared_block_pool` is selected but from `TempTable` PoV this does not
  // have any semantic value, e.g. it could very well have been something else.
  //
  // This is a bit confusing and unexpected, and hence this comment to make life
  // easier for readers of this part of the code.
  thd_set_ha_data(ha_thd(), hton, &shared_block_pool);

#if defined(HAVE_WINNUMA)
  SYSTEM_INFO systemInfo;
  GetSystemInfo(&systemInfo);

  win_page_size = systemInfo.dwPageSize;
#endif /* HAVE_WINNUMA */
}

int Handler::create(const char *table_name, TABLE *mysql_table,
                    HA_CREATE_INFO *, dd::Table *) {
  DBUG_TRACE;

  assert(mysql_table != nullptr);
  assert(mysql_table->s != nullptr);
  assert(mysql_table->field != nullptr);
  assert(table_name != nullptr);

  bool all_columns_are_fixed_size = true;
  for (uint i = 0; i < mysql_table->s->fields; ++i) {
    Field *mysql_field = mysql_table->field[i];
    assert(mysql_field != nullptr);
    if (!is_field_type_fixed_size(*mysql_field)) {
      all_columns_are_fixed_size = false;
      break;
    }
  }

  Result ret;

  try {
    DBUG_EXECUTE_IF("temptable_create_return_full",
                    throw Result::RECORD_FILE_FULL;);
    DBUG_EXECUTE_IF("temptable_create_return_non_result_type_exception",
                    throw 42;);

    // Calculate m_number_of_elements_per_page, see Table::Table():
    if (all_columns_are_fixed_size) {
      Storage rows_of_the_table = Storage(nullptr);
      rows_of_the_table.element_size(mysql_table->s->rec_buff_length);
      if (rows_of_the_table.number_of_elements_per_page() == 0)
        DBUG_RET(Result::TOO_BIG_ROW);
    }

    size_t per_table_limit = thd_get_tmp_table_size(ha_thd());
    auto &kv_store = kv_store_shard[thd_thread_id(ha_thd())];
    const auto insert_result = kv_store.emplace(
        std::piecewise_construct, std::forward_as_tuple(table_name),
        std::forward_as_tuple(mysql_table, m_shared_block,
                              all_columns_are_fixed_size, per_table_limit));

    ret = insert_result.second ? Result::OK : Result::TABLE_EXIST;

  } catch (Result ex) {
    ret = ex;
  } catch (...) {
    ret = Result::OUT_OF_MEM;
  }

  DBUG_RET(ret);
}

int Handler::delete_table(const char *table_name, const dd::Table *) {
  DBUG_TRACE;

  assert(table_name != nullptr);

  Result ret;

  try {
    auto &kv_store = kv_store_shard[thd_thread_id(ha_thd())];
    auto table = kv_store.find(table_name);
    if (table) {
      if (m_opened_table != table) {
        kv_store.erase(table_name);
        ret = Result::OK;
      } else {
        /* Attempt to delete the currently opened table. */
        ret = Result::UNSUPPORTED;
      }
    } else {
      ret = Result::NO_SUCH_TABLE;
    }
  } catch (Result ex) {
    ret = ex;
  } catch (std::bad_alloc &) {
    ret = Result::OUT_OF_MEM;
  }

  DBUG_PRINT("temptable_api", ("this=%p %s; return=%s", this, table_name,
                               result_to_string(ret)));

  DBUG_RET(ret);
}

int Handler::open(const char *table_name, int, uint, const dd::Table *) {
  DBUG_TRACE;

  assert(m_opened_table == nullptr);
  assert(table_name != nullptr);
  assert(!m_rnd_iterator_is_positioned);
  assert(!m_index_cursor.is_positioned());
  assert(handler::active_index == MAX_KEY);

  Result ret;

  try {
    auto &kv_store = kv_store_shard[thd_thread_id(ha_thd())];
    m_opened_table = kv_store.find(table_name);
    if (m_opened_table) {
      ret = Result::OK;
      opened_table_validate();
    } else {
      ret = Result::NO_SUCH_TABLE;
    }
  } catch (std::bad_alloc &) {
    ret = Result::OUT_OF_MEM;
  }

  DBUG_PRINT("temptable_api", ("this=%p %s; return=%s", this, table_name,
                               result_to_string(ret)));

  DBUG_RET(ret);
}

int Handler::close() {
  DBUG_TRACE;

  assert(m_opened_table != nullptr);

  m_opened_table = nullptr;

  handler::active_index = MAX_KEY;
  m_rnd_iterator_is_positioned = false;
  m_index_cursor.unposition();

  /* Marked unused - temporary fix for GCC 8 bug 82728. */
  const Result ret TEMPTABLE_UNUSED = Result::OK;

  DBUG_PRINT("temptable_api",
             ("this=%p; return=%s", this, result_to_string(ret)));

  DBUG_RET(ret);
}

int Handler::rnd_init(bool) {
  DBUG_TRACE;

  m_rnd_iterator_is_positioned = false;

  /* Marked unused - temporary fix for GCC 8 bug 82728. */
  const Result ret TEMPTABLE_UNUSED = Result::OK;

  DBUG_PRINT("temptable_api",
             ("this=%p; return=%s", this, result_to_string(ret)));

  DBUG_RET(ret);
}

int Handler::rnd_next(uchar *mysql_row) {
  DBUG_TRACE;

  opened_table_validate();

  handler::ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);

  const Storage &rows = m_opened_table->rows();

  Result ret;

  if (!m_rnd_iterator_is_positioned) {
    /* This is the first call to `rnd_next()` */
    m_rnd_iterator = rows.begin();
    if (m_rnd_iterator != rows.end()) {
      m_rnd_iterator_is_positioned = true;
      m_opened_table->row(m_rnd_iterator, mysql_row);
      ret = Result::OK;
    } else {
      ret = Result::END_OF_FILE;
    }
  } else {
    assert(m_rnd_iterator != rows.end());
    Storage::Element *previous = *m_rnd_iterator;
    ++m_rnd_iterator;
    if (m_rnd_iterator != rows.end()) {
      m_opened_table->row(m_rnd_iterator, mysql_row);
      ret = Result::OK;
    } else {
      /* Undo the ++ operation above. The expectation of the users of the
       * API is that if we hit the end and then new rows are inserted and then
       * `rnd_next()` is called again - that it will fetch the newly inserted
       * rows. For example: let the table have 2 rows: "a" and "b", then:
       * 1. `rnd_next()` moves to "b" and returns it to the caller
       * 2. `rnd_next()` returns END_OF_FILE, but keeps the cursor at "b", it
       *    does not advance it past the end
       * 3. possibly more calls to `rnd_next()`, they act as in 2.
       * 4. another row is inserted: "c"
       * 5. `rnd_next()` moves to "c" and returns it to the caller
       * If we do not undo the ++ and let the cursor move past the last
       * element then we will miss the first newly inserted row in the above
       * scenario:
       * 1. `rnd_next()` moves to "b" and returns it to the caller
       * 2. `rnd_next()` moves after "b" and returns END_OF_FILE
       * 3. two rows are inserted: "c" and "d" (the cursor now points to "c")
       * 4. `rnd_next()` moves to "d" and returns it to the caller
       * 5. "c" has been erroneously skipped */
      m_rnd_iterator = previous;
      ret = Result::END_OF_FILE;
    }
  }

  DBUG_RET(ret);
}

int Handler::rnd_pos(uchar *mysql_row, uchar *position) {
  DBUG_TRACE;

  opened_table_validate();

  handler::ha_statistic_increment(&System_status_var::ha_read_rnd_count);

  Storage::Element *row;
  memcpy(&row, position, sizeof(row));

  m_rnd_iterator = Storage::Iterator(&m_opened_table->rows(), row);

  m_rnd_iterator_is_positioned = true;

  m_opened_table->row(m_rnd_iterator, mysql_row);

  /* Marked unused - temporary fix for GCC 8 bug 82728. */
  const Result ret TEMPTABLE_UNUSED = Result::OK;

  DBUG_RET(ret);
}

int Handler::rnd_end() {
  DBUG_TRACE;

  m_rnd_iterator_is_positioned = false;

  /* Marked unused - temporary fix for GCC 8 bug 82728. */
  const Result ret TEMPTABLE_UNUSED = Result::OK;

  DBUG_PRINT("temptable_api",
             ("this=%p; return=%s", this, result_to_string(ret)));

  DBUG_RET(ret);
}

int Handler::index_init(uint index_no, bool) {
  DBUG_TRACE;

  opened_table_validate();

  Result ret;

  if (index_no >= m_opened_table->number_of_indexes()) {
    ret = Result::WRONG_INDEX;
  } else {
    handler::active_index = index_no;
    ret = Result::OK;
  }

  DBUG_PRINT("temptable_api", ("this=%p index=%d; return=%s", this, index_no,
                               result_to_string(ret)));

  DBUG_RET(ret);
}

int Handler::index_read(uchar *mysql_row, const uchar *mysql_search_cells,
                        uint mysql_search_cells_len_bytes,
                        ha_rkey_function find_flag) {
  DBUG_TRACE;

  opened_table_validate();

  handler::ha_statistic_increment(&System_status_var::ha_read_key_count);

  assert(handler::active_index < m_opened_table->number_of_indexes());

  Result ret = Result::UNSUPPORTED;

  try {
    const Index &index = m_opened_table->index(handler::active_index);

    Indexed_cells search_cells(mysql_search_cells, mysql_search_cells_len_bytes,
                               index);

    switch (find_flag) {
      case HA_READ_KEY_EXACT:

        switch (index.lookup(search_cells, &m_index_cursor)) {
          case Index::Lookup::FOUND:
            ret = Result::OK;
            break;
          case Index::Lookup::NOT_FOUND_CURSOR_POSITIONED_ON_NEXT:
          case Index::Lookup::NOT_FOUND_CURSOR_UNDEFINED:
            ret = Result::KEY_NOT_FOUND;
            break;
        }

        break;

      case HA_READ_AFTER_KEY:

        ret = Result::UNSUPPORTED;
        break;

      case HA_READ_KEY_OR_NEXT:

        if (handler::table->s->key_info[handler::active_index].algorithm !=
            HA_KEY_ALG_BTREE) {
          ret = Result::UNSUPPORTED;
          break;
        }

        switch (index.lookup(search_cells, &m_index_cursor)) {
          case Index::Lookup::FOUND:
          case Index::Lookup::NOT_FOUND_CURSOR_POSITIONED_ON_NEXT:
            ret = Result::OK;
            break;
          case Index::Lookup::NOT_FOUND_CURSOR_UNDEFINED:
            ret = Result::KEY_NOT_FOUND;
            break;
        }

        break;

      case HA_READ_PREFIX_LAST: {
        if (handler::table->s->key_info[handler::active_index].algorithm !=
            HA_KEY_ALG_BTREE) {
          ret = Result::UNSUPPORTED;
          break;
        }

        Cursor first_unused;
        switch (index.lookup(search_cells, &first_unused, &m_index_cursor)) {
          case Index::Lookup::FOUND:
            /* m_index_cursor is positioned after the last matching element. */
            --m_index_cursor;
            ret = Result::OK;
            break;
          case Index::Lookup::NOT_FOUND_CURSOR_POSITIONED_ON_NEXT:
          case Index::Lookup::NOT_FOUND_CURSOR_UNDEFINED:
            ret = Result::KEY_NOT_FOUND;
            break;
        }

        break;
      }

      case HA_READ_KEY_OR_PREV:
      case HA_READ_BEFORE_KEY:
      case HA_READ_PREFIX:
      case HA_READ_PREFIX_LAST_OR_PREV:
      case HA_READ_MBR_CONTAIN:
      case HA_READ_MBR_INTERSECT:
      case HA_READ_MBR_WITHIN:
      case HA_READ_MBR_DISJOINT:
      case HA_READ_MBR_EQUAL:
      case HA_READ_INVALID:
        ret = Result::UNSUPPORTED;
        break;
    }

    if (ret == Result::OK) {
      m_index_cursor.export_row_to_mysql(m_opened_table->columns(), mysql_row,
                                         handler::table->s->rec_buff_length);
      m_index_read_number_of_cells = search_cells.number_of_cells();
    }
  } catch (Result ex) {
    ret = ex;
  } catch (std::bad_alloc &) {
    ret = Result::OUT_OF_MEM;
  }

  DBUG_RET(ret);
}

int Handler::index_next(uchar *mysql_row) {
  DBUG_TRACE;

  opened_table_validate();

  handler::ha_statistic_increment(&System_status_var::ha_read_next_count);

  const Result ret = index_next_conditional(mysql_row, NextCondition::NO);

  DBUG_RET(ret);
}

int Handler::index_next_same(uchar *mysql_row, const uchar *, uint) {
  DBUG_TRACE;

  opened_table_validate();

  handler::ha_statistic_increment(&System_status_var::ha_read_next_count);

  const Result ret =
      index_next_conditional(mysql_row, NextCondition::ONLY_IF_SAME);

  DBUG_RET(ret);
}

Result Handler::index_next_conditional(uchar *mysql_row,
                                       NextCondition condition) {
  DBUG_TRACE;

  opened_table_validate();

  assert(m_index_cursor.is_positioned());

  Result ret;

  try {
    const Index &index = m_opened_table->index(handler::active_index);

    const Cursor &end = index.end();

    if (m_index_cursor == end) {
      ret = Result::END_OF_FILE;
    } else {
      Indexed_cells indexed_cells_previous = m_index_cursor.indexed_cells();
      /* Lower the number of cells to what was given to `index_read()`. */
      assert(m_index_read_number_of_cells <=
             indexed_cells_previous.number_of_cells());
      indexed_cells_previous.number_of_cells(m_index_read_number_of_cells);

      ++m_index_cursor;

      if (m_index_cursor == end) {
        ret = Result::END_OF_FILE;
      } else {
        bool ok = false;
        switch (condition) {
          case NextCondition::NO:
            ok = true;
            break;
          case NextCondition::ONLY_IF_SAME: {
            const Indexed_cells &indexed_cells_current =
                m_index_cursor.indexed_cells();

            const Indexed_cells_equal_to comparator{index};

            /* indexed_cells_previous == indexed_cells_current */
            ok = comparator(indexed_cells_previous, indexed_cells_current);

            break;
          }
        }

        if (ok) {
          m_index_cursor.export_row_to_mysql(
              m_opened_table->columns(), mysql_row,
              handler::table->s->rec_buff_length);
          ret = Result::OK;
        } else {
          ret = Result::END_OF_FILE;
        }
      }
    }

    if (ret != Result::OK) {
      m_index_cursor.unposition();
    }
  } catch (Result ex) {
    ret = ex;
  } catch (std::bad_alloc &) {
    ret = Result::OUT_OF_MEM;
  }

  return ret;
}

int Handler::index_read_last(uchar *mysql_row, const uchar *mysql_search_cells,
                             uint mysql_search_cells_len_bytes) {
  DBUG_TRACE;

  opened_table_validate();

  const Result ret = static_cast<Result>(
      index_read(mysql_row, mysql_search_cells, mysql_search_cells_len_bytes,
                 HA_READ_PREFIX_LAST));

  DBUG_RET(ret);
}

int Handler::index_prev(uchar *mysql_row) {
  DBUG_TRACE;

  opened_table_validate();

  assert(m_index_cursor.is_positioned());

  handler::ha_statistic_increment(&System_status_var::ha_read_prev_count);

  Result ret;

  try {
    const Cursor &begin = m_opened_table->index(handler::active_index).begin();

    if (handler::table->s->key_info[handler::active_index].algorithm !=
        HA_KEY_ALG_BTREE) {
      ret = Result::UNSUPPORTED;
    } else if (m_index_cursor == begin) {
      ret = Result::END_OF_FILE;
    } else {
      --m_index_cursor;
      m_index_cursor.export_row_to_mysql(m_opened_table->columns(), mysql_row,
                                         handler::table->s->rec_buff_length);
      ret = Result::OK;
    }
  } catch (Result ex) {
    ret = ex;
  } catch (std::bad_alloc &) {
    ret = Result::OUT_OF_MEM;
  }

  DBUG_RET(ret);
}

int Handler::index_end() {
  DBUG_TRACE;

  handler::active_index = MAX_KEY;

  m_index_cursor.unposition();

  /* Marked unused - temporary fix for GCC 8 bug 82728. */
  const Result ret TEMPTABLE_UNUSED = Result::OK;

  DBUG_PRINT("temptable_api",
             ("this=%p; return=%s", this, result_to_string(ret)));

  DBUG_RET(ret);
}

void Handler::position(const uchar *) {
  DBUG_TRACE;

  Storage::Element *row;

  if (m_rnd_iterator_is_positioned) {
    assert(!m_index_cursor.is_positioned());
    row = *m_rnd_iterator;
  } else {
    assert(m_index_cursor.is_positioned());
    row = m_index_cursor.row();
  }

  *reinterpret_cast<Storage::Element **>(handler::ref) = row;

  DBUG_PRINT("temptable_api", ("this=%p; saved position=%p", this, row));
}

int Handler::write_row(uchar *mysql_row) {
  DBUG_TRACE;

  opened_table_validate();

  handler::ha_statistic_increment(&System_status_var::ha_write_count);

  const Result ret = m_opened_table->insert(mysql_row);

  DBUG_RET(ret);
}

int Handler::update_row(const uchar *mysql_row_old, uchar *mysql_row_new) {
  DBUG_TRACE;

  opened_table_validate();

  handler::ha_statistic_increment(&System_status_var::ha_update_count);

  Storage::Element *target_row;

  if (m_rnd_iterator_is_positioned) {
    assert(!m_index_cursor.is_positioned());
    target_row = *m_rnd_iterator;
  } else {
    assert(m_index_cursor.is_positioned());
    target_row = m_index_cursor.row();
  }

  const Result ret =
      m_opened_table->update(mysql_row_old, mysql_row_new, target_row);

  DBUG_RET(ret);
}

int Handler::delete_row(const uchar *mysql_row) {
  DBUG_TRACE;

  opened_table_validate();

  assert(m_rnd_iterator_is_positioned);

  ha_statistic_increment(&System_status_var::ha_delete_count);

  const Storage::Iterator victim_position = m_rnd_iterator;

  /* Move `m_rnd_iterator` to the preceding position. */
  if (m_rnd_iterator == m_opened_table->rows().begin()) {
    /* Position before the first. */
    m_rnd_iterator_is_positioned = false;
  } else {
    --m_rnd_iterator;
  }

  const Result ret = m_opened_table->remove(mysql_row, victim_position);

  if (ret == Result::OK) {
    ++m_deleted_rows;
  }

  DBUG_RET(ret);
}

int Handler::truncate(dd::Table *) {
  DBUG_TRACE;

  opened_table_validate();

  m_opened_table->truncate();

  m_rnd_iterator_is_positioned = false;
  m_index_cursor.unposition();

  /* Marked unused - temporary fix for GCC 8 bug 82728. */
  const Result ret TEMPTABLE_UNUSED = Result::OK;

  DBUG_PRINT("temptable_api",
             ("this=%p; return=%s", this, result_to_string(ret)));

  DBUG_RET(ret);
}

int Handler::delete_all_rows() {
  DBUG_TRACE;
  return truncate(nullptr);
}

int Handler::info(uint) {
  DBUG_TRACE;

  opened_table_validate();

  stats.deleted = m_deleted_rows;
  stats.records = m_opened_table->number_of_rows();
  stats.table_in_mem_estimate = 1.0;

  for (uint i = 0; i < table->s->keys; ++i) {
    KEY *key = &table->key_info[i];

    key->set_in_memory_estimate(1.0);
  }

  /* Marked unused - temporary fix for GCC 8 bug 82728. */
  const Result ret TEMPTABLE_UNUSED = Result::OK;

  DBUG_PRINT("temptable_api", ("this=%p out=(stats.records=%llu); return=%s",
                               this, stats.records, result_to_string(ret)));

  DBUG_RET(ret);
}

longlong Handler::get_memory_buffer_size() const {
  DBUG_TRACE;

  DBUG_PRINT("temptable_api",
             ("this=%p; return=%lld", this, temptable_max_ram));

  return temptable_max_ram;
}

const char *Handler::table_type() const {
  DBUG_TRACE;
  return "TempTable";
}

::handler::Table_flags Handler::table_flags() const {
  DBUG_TRACE;

  // clang-format off
  const Table_flags flags =
      HA_NO_TRANSACTIONS |
      HA_CAN_GEOMETRY |
      HA_FAST_KEY_READ |
      HA_NULL_IN_KEY |
      HA_CAN_INDEX_BLOBS |
      HA_STATS_RECORDS_IS_EXACT |
      HA_NO_AUTO_INCREMENT |
      HA_COUNT_ROWS_INSTANT;
  // clang-format on

  DBUG_PRINT("temptable_api", ("this=%p; return=%lld", this, flags));

  return flags;
}

ulong Handler::index_flags(uint index_no, uint, bool) const {
  DBUG_TRACE;

  ulong flags = 0;

  switch (table_share->key_info[index_no].algorithm) {
    case HA_KEY_ALG_BTREE:
      // clang-format off
      flags =
          HA_READ_NEXT |
          HA_READ_PREV |
          HA_READ_ORDER |
          HA_READ_RANGE |
          HA_KEY_SCAN_NOT_ROR;
      // clang-format on
      break;
    case HA_KEY_ALG_HASH:
      // clang-format off
      flags =
          HA_READ_NEXT |
          HA_ONLY_WHOLE_INDEX |
          HA_KEY_SCAN_NOT_ROR;
      // clang-format on
      break;
    case HA_KEY_ALG_SE_SPECIFIC:
    case HA_KEY_ALG_RTREE:
    case HA_KEY_ALG_FULLTEXT:
      flags = 0;
      break;
  }

  DBUG_PRINT("temptable_api", ("this=%p; return=%lu", this, flags));

  return flags;
}

ha_key_alg Handler::get_default_index_algorithm() const {
  DBUG_TRACE;
  return HA_KEY_ALG_HASH;
}

bool Handler::is_index_algorithm_supported(ha_key_alg algorithm) const {
  DBUG_TRACE;
  return algorithm == HA_KEY_ALG_BTREE || algorithm == HA_KEY_ALG_HASH;
}

uint Handler::max_supported_key_length() const {
  DBUG_TRACE;

  const uint length = std::numeric_limits<uint>::max();

  DBUG_PRINT("temptable_api", ("this=%p; return=%u", this, length));

  return length;
}

uint Handler::max_supported_key_part_length(HA_CREATE_INFO *create_info
                                            [[maybe_unused]]) const {
  DBUG_TRACE;

  const uint length = std::numeric_limits<uint>::max();

  DBUG_PRINT("temptable_api", ("this=%p; return=%u", this, length));

  return length;
}

#if 0
/* This is disabled in order to mimic ha_heap's implementation which relies on
 * the method from the parent class which adds a magic +10. */
ha_rows Handler::estimate_rows_upper_bound() {
  DBUG_TRACE;

  assert(m_opened_table != nullptr);

  const ha_rows n = m_opened_table->number_of_rows();

  DBUG_PRINT("temptable_api", ("this=%p; return=%llu", this, n));

  return n;
}
#endif

THR_LOCK_DATA **Handler::store_lock(THD *, THR_LOCK_DATA **, thr_lock_type) {
  DBUG_TRACE;
  return nullptr;
}

double Handler::scan_time() {
  DBUG_TRACE;

  /* Mimic ha_heap::scan_time() to avoid a storm of execution plan changes. */
  const double t = (stats.records + stats.deleted) / 20.0 + 10;

  DBUG_PRINT("temptable_api", ("this=%p; return=%.4lf", this, t));

  return t;
}

double Handler::read_time(uint, uint, ha_rows rows) {
  DBUG_TRACE;

  /* Mimic ha_heap::read_time() to avoid a storm of execution plan changes. */
  const double t = rows / 20.0 + 1;

  DBUG_PRINT("temptable_api", ("this=%p; return=%.4lf", this, t));

  return t;
}

int Handler::disable_indexes(uint mode) {
  DBUG_TRACE;

  assert(m_opened_table != nullptr);

  Result ret;

  if (mode == HA_KEY_SWITCH_ALL) {
    ret = m_opened_table->disable_indexes();
  } else {
    ret = Result::WRONG_COMMAND;
  }

  DBUG_PRINT("temptable_api",
             ("this=%p; return=%s", this, result_to_string(ret)));

  DBUG_RET(ret);
}

int Handler::enable_indexes(uint mode) {
  DBUG_TRACE;

  Result ret;

  if (mode == HA_KEY_SWITCH_ALL) {
    ret = m_opened_table->enable_indexes();
  } else {
    ret = Result::WRONG_COMMAND;
  }

  DBUG_PRINT("temptable_api",
             ("this=%p; return=%s", this, result_to_string(ret)));

  DBUG_RET(ret);
}

/* Not implemented methods. */

int Handler::external_lock(THD *, int) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

void Handler::unlock_row() { DBUG_TRACE; }

handler *Handler::clone(const char *, MEM_ROOT *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return nullptr;
}

int Handler::index_first(uchar *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

int Handler::index_last(uchar *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

int Handler::analyze(THD *, HA_CHECK_OPT *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

int Handler::optimize(THD *, HA_CHECK_OPT *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

int Handler::check(THD *, HA_CHECK_OPT *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

int Handler::start_stmt(THD *, thr_lock_type) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

int Handler::reset() {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

int Handler::records(ha_rows *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

void Handler::update_create_info(HA_CREATE_INFO *) {
  DBUG_TRACE;
  DBUG_ABORT();
}

int Handler::rename_table(const char *, const char *, const dd::Table *,
                          dd::Table *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

void Handler::init_table_handle_for_HANDLER() {
  DBUG_TRACE;
  DBUG_ABORT();
}

bool Handler::get_error_message(int, String *) {
  DBUG_TRACE;
  DBUG_ABORT();
  return false;
}

bool Handler::primary_key_is_clustered() const {
  DBUG_TRACE;
  return false;
}

int Handler::cmp_ref(const uchar *, const uchar *) const {
  DBUG_TRACE;
  DBUG_ABORT();
  return 0;
}

bool Handler::check_if_incompatible_data(HA_CREATE_INFO *, uint) {
  DBUG_TRACE;
  DBUG_ABORT();
  return false;
}

} /* namespace temptable */
