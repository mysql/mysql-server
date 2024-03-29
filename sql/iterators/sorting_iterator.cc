/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/iterators/sorting_iterator.h"

#include <stdio.h>
#include <sys/types.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

#include "map_helpers.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/field.h"
#include "sql/filesort.h"  // Filesort
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/iterators/basic_row_iterators.h"
#include "sql/mysqld.h"  // stage_executing
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/sort_param.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_show.h"       // get_schema_tables_result
#include "sql/sql_sort.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "thr_lock.h"
#include "varlen_sort.h"

using std::string;
using std::vector;

// If the table is scanned with a FullTextSearchIterator, tell the
// corresponding full-text function that it is no longer using an
// index scan. Used by the sorting iterators when switching the
// underlying scans to random access mode after the sorting is done
// and before the iterator above it starts reading the sorted rows.
static void EndFullTextIndexScan(TABLE *table) {
  if (table->file->ft_handler != nullptr) {
    for (Item_func_match &ft_func :
         *table->pos_in_table_list->query_block->ftfunc_list) {
      if (ft_func.master == nullptr &&
          ft_func.ft_handler == table->file->ft_handler) {
        ft_func.score_from_index_scan = false;
        break;
      }
    }
  }
}

SortFileIndirectIterator::SortFileIndirectIterator(
    THD *thd, Mem_root_array<TABLE *> tables, IO_CACHE *tempfile,
    bool ignore_not_found_rows, bool has_null_flags, ha_rows *examined_rows)
    : RowIterator(thd),
      m_io_cache(tempfile),
      m_examined_rows(examined_rows),
      m_tables(std::move(tables)),
      m_ignore_not_found_rows(ignore_not_found_rows),
      m_has_null_flags(has_null_flags) {}

SortFileIndirectIterator::~SortFileIndirectIterator() {
  for (TABLE *table : m_tables) {
    (void)table->file->ha_index_or_rnd_end();
  }

  close_cached_file(m_io_cache);
  my_free(m_io_cache);
}

bool SortFileIndirectIterator::Init() {
  m_sum_ref_length = 0;

  for (TABLE *table : m_tables) {
    // The sort's source iterator could have initialized an index
    // read, and it won't call end until it's destroyed (which we
    // can't do before destroying SortingIterator, since we may need
    // to scan/sort multiple times). Thus, as a small hack, we need
    // to reset it here.
    table->file->ha_index_or_rnd_end();

    // Item_func_match::val_real() needs to know whether the match
    // score is already present (which is the case when scanning the
    // base table using a FullTextSearchIterator, but not when
    // running this iterator), so we need to tell it that it needs
    // to fetch the score when it's called.
    EndFullTextIndexScan(table);

    int error = table->file->ha_rnd_init(false);
    if (error) {
      table->file->print_error(error, MYF(0));
      return true;
    }

    if (m_has_null_flags && table->is_nullable()) {
      ++m_sum_ref_length;
    }
    m_sum_ref_length += table->file->ref_length;
  }
  if (m_ref_pos == nullptr) {
    m_ref_pos = thd()->mem_root->ArrayAlloc<uchar>(m_sum_ref_length);
  }

  return false;
}

static int HandleError(THD *thd, TABLE *table, int error) {
  if (thd->killed) {
    thd->send_kill_message();
    return 1;
  }

  if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND) {
    table->set_no_row();
    return -1;
  } else {
    table->file->print_error(error, MYF(0));
    return 1;
  }
}

int SortFileIndirectIterator::Read() {
  for (;;) {
    if (my_b_read(m_io_cache, m_ref_pos, m_sum_ref_length))
      return -1; /* End of file */
    uchar *ref_pos = m_ref_pos;
    bool skip = false;
    for (TABLE *table : m_tables) {
      if (m_has_null_flags && table->is_nullable()) {
        if (*ref_pos++) {
          table->set_null_row();
          ref_pos += table->file->ref_length;
          continue;
        } else {
          table->reset_null_row();
        }
      }

      int tmp = table->file->ha_rnd_pos(table->record[0], ref_pos);
      ref_pos += table->file->ref_length;
      /* The following is extremely unlikely to happen */
      if (tmp == HA_ERR_RECORD_DELETED ||
          (tmp == HA_ERR_KEY_NOT_FOUND && m_ignore_not_found_rows)) {
        skip = true;
        break;
      } else if (tmp != 0) {
        return HandleError(thd(), table, tmp);
      }
    }
    if (skip) {
      continue;
    }
    if (m_examined_rows != nullptr) {
      ++*m_examined_rows;
    }
    return 0;
  }
}

template <bool Packed_addon_fields>
SortFileIterator<Packed_addon_fields>::SortFileIterator(
    THD *thd, Mem_root_array<TABLE *> tables, IO_CACHE *tempfile,
    Filesort_info *sort, ha_rows *examined_rows)
    : RowIterator(thd),
      m_rec_buf(sort->addon_fields->get_addon_buf()),
      m_buf_length(sort->addon_fields->get_addon_buf_length()),
      m_tables(std::move(tables)),
      m_io_cache(tempfile),
      m_sort(sort),
      m_examined_rows(examined_rows) {}

template <bool Packed_addon_fields>
SortFileIterator<Packed_addon_fields>::~SortFileIterator() {
  close_cached_file(m_io_cache);
  my_free(m_io_cache);
}

/**
  Read a result set record from a temporary file after sorting.

  The function first reads the next sorted record from the temporary file.
  into a buffer. If a success it calls a callback function that unpacks
  the fields values use in the result set from this buffer into their
  positions in the regular record buffer.

  @tparam Packed_addon_fields Are the addon fields packed?
     This is a compile-time constant, to avoid if (....) tests during execution.
  @retval
    0   Record successfully read.
  @retval
    -1   There is no record to be read anymore.
*/
template <bool Packed_addon_fields>
int SortFileIterator<Packed_addon_fields>::Read() {
  uchar *destination = m_rec_buf;
  if (Packed_addon_fields) {
    const uint len_sz = Addon_fields::size_of_length_field;

    // First read length of the record.
    if (my_b_read(m_io_cache, destination, len_sz)) return -1;
    uint res_length = Addon_fields::read_addon_length(destination);
    assert(res_length > len_sz);
    assert(m_sort->using_addon_fields());

    // Then read the rest of the record.
    if (my_b_read(m_io_cache, destination + len_sz, res_length - len_sz))
      return -1; /* purecov: inspected */
  } else {
    if (my_b_read(m_io_cache, destination, m_buf_length)) return -1;
  }

  m_sort->unpack_addon_fields<Packed_addon_fields>(m_tables, destination);

  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

template <bool Packed_addon_fields>
SortBufferIterator<Packed_addon_fields>::SortBufferIterator(
    THD *thd, Mem_root_array<TABLE *> tables, Filesort_info *sort,
    Sort_result *sort_result, ha_rows *examined_rows)
    : RowIterator(thd),
      m_sort(sort),
      m_sort_result(sort_result),
      m_examined_rows(examined_rows),
      m_tables(std::move(tables)) {}

template <bool Packed_addon_fields>
SortBufferIterator<Packed_addon_fields>::~SortBufferIterator() {
  m_sort_result->sorted_result.reset();
  m_sort_result->sorted_result_in_fsbuf = false;
}

template <bool Packed_addon_fields>
bool SortBufferIterator<Packed_addon_fields>::Init() {
  m_unpack_counter = 0;
  return false;
}

/**
  Read a result set record from a buffer after sorting.

  Get the next record from the filesort buffer,
  then unpack the fields into their positions in the regular record buffer.

  @tparam Packed_addon_fields Are the addon fields packed?
     This is a compile-time constant, to avoid if (....) tests during execution.

  TODO: consider templatizing on is_varlen as well.
  Variable / Fixed size key is currently handled by
  Filesort_info::get_start_of_payload

  @retval
    0   Record successfully read.
  @retval
    -1   There is no record to be read anymore.
*/
template <bool Packed_addon_fields>
int SortBufferIterator<Packed_addon_fields>::Read() {
  if (m_unpack_counter ==
      m_sort_result->found_records)  // XXX send in as a parameter?
    return -1;                       /* End of buffer */

  uchar *record = m_sort->get_sorted_record(m_unpack_counter++);
  uchar *payload = get_start_of_payload(m_sort, record);
  m_sort->unpack_addon_fields<Packed_addon_fields>(m_tables, payload);
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

SortBufferIndirectIterator::SortBufferIndirectIterator(
    THD *thd, Mem_root_array<TABLE *> tables, Sort_result *sort_result,
    bool ignore_not_found_rows, bool has_null_flags, ha_rows *examined_rows)
    : RowIterator(thd),
      m_sort_result(sort_result),
      m_tables(std::move(tables)),
      m_examined_rows(examined_rows),
      m_ignore_not_found_rows(ignore_not_found_rows),
      m_has_null_flags(has_null_flags) {}

SortBufferIndirectIterator::~SortBufferIndirectIterator() {
  m_sort_result->sorted_result.reset();
  assert(!m_sort_result->sorted_result_in_fsbuf);
  m_sort_result->sorted_result_in_fsbuf = false;

  for (TABLE *table : m_tables) {
    (void)table->file->ha_index_or_rnd_end();
  }
}

bool SortBufferIndirectIterator::Init() {
  m_sum_ref_length = 0;
  for (TABLE *table : m_tables) {
    // The sort's source iterator could have initialized an index
    // read, and it won't call end until it's destroyed (which we
    // can't do before destroying SortingIterator, since we may need
    // to scan/sort multiple times). Thus, as a small hack, we need
    // to reset it here.
    table->file->ha_index_or_rnd_end();

    // Item_func_match::val_real() needs to know whether the match
    // score is already present (which is the case when scanning the
    // base table using a FullTextSearchIterator, but not when
    // running this iterator), so we need to tell it that it needs
    // to fetch the score when it's called.
    EndFullTextIndexScan(table);

    int error = table->file->ha_rnd_init(false);
    if (error) {
      table->file->print_error(error, MYF(0));
      return true;
    }

    if (m_has_null_flags && table->is_nullable()) {
      ++m_sum_ref_length;
    }
    m_sum_ref_length += table->file->ref_length;
  }
  m_cache_pos = m_sort_result->sorted_result.get();
  m_cache_end = m_cache_pos + m_sort_result->found_records * m_sum_ref_length;
  return false;
}

int SortBufferIndirectIterator::Read() {
  for (;;) {
    if (m_cache_pos == m_cache_end) return -1; /* End of file */
    uchar *cache_pos = m_cache_pos;
    m_cache_pos += m_sum_ref_length;

    bool skip = false;
    for (TABLE *table : m_tables) {
      if (m_has_null_flags && table->is_nullable()) {
        if (*cache_pos++) {
          table->set_null_row();
          cache_pos += table->file->ref_length;
          continue;
        } else {
          table->reset_null_row();
        }
      }
      int tmp = table->file->ha_rnd_pos(table->record[0], cache_pos);
      cache_pos += table->file->ref_length;
      /* The following is extremely unlikely to happen */
      if (tmp == HA_ERR_RECORD_DELETED ||
          (tmp == HA_ERR_KEY_NOT_FOUND && m_ignore_not_found_rows)) {
        skip = true;
        break;
      } else if (tmp != 0) {
        return HandleError(thd(), table, tmp);
      }
    }
    if (skip) {
      continue;
    }
    if (m_examined_rows != nullptr) {
      ++*m_examined_rows;
    }
    return 0;
  }
}

SortingIterator::SortingIterator(THD *thd, Filesort *filesort,
                                 unique_ptr_destroy_only<RowIterator> source,
                                 ha_rows num_rows_estimate,
                                 table_map tables_to_get_rowid_for,
                                 ha_rows *examined_rows)
    : RowIterator(thd),
      m_filesort(filesort),
      m_source_iterator(std::move(source)),
      m_num_rows_estimate(num_rows_estimate),
      m_tables_to_get_rowid_for(tables_to_get_rowid_for),
      m_examined_rows(examined_rows) {}

SortingIterator::~SortingIterator() {
  ReleaseBuffers();
  CleanupAfterQuery();
}

void SortingIterator::CleanupAfterQuery() {
  m_fs_info.free_sort_buffer();
  my_free(m_fs_info.merge_chunks.array());
  m_fs_info.merge_chunks = Merge_chunk_array(nullptr, 0);
  m_fs_info.addon_fields = nullptr;
}

void SortingIterator::ReleaseBuffers() {
  m_result_iterator.reset();
  if (m_sort_result.io_cache) {
    // NOTE: The io_cache is only owned by us if it were never used.
    close_cached_file(m_sort_result.io_cache);
    my_free(m_sort_result.io_cache);
    m_sort_result.io_cache = nullptr;
  }
  m_sort_result.sorted_result.reset();
  m_sort_result.sorted_result_in_fsbuf = false;

  // Keep the sort buffer in m_fs_info.
}

bool SortingIterator::Init() {
  ReleaseBuffers();

  // Both empty result and error count as errors. (TODO: Why? This is a legacy
  // choice that doesn't always seem right to me, although it should nearly
  // never happen in practice.)
  if (DoSort() != 0) return true;

  // Prepare the result iterator for actually reading the data. Read()
  // will proxy to it.
  Mem_root_array<TABLE *> tables(thd()->mem_root, m_filesort->tables);
  if (m_sort_result.io_cache && my_b_inited(m_sort_result.io_cache)) {
    // Test if ref-records was used
    if (m_fs_info.using_addon_fields()) {
      DBUG_PRINT("info", ("using SortFileIterator"));
      if (m_fs_info.addon_fields->using_packed_addons())
        m_result_iterator.reset(
            new (&m_result_iterator_holder.sort_file_packed_addons)
                SortFileIterator<true>(thd(), std::move(tables),
                                       m_sort_result.io_cache, &m_fs_info,
                                       m_examined_rows));
      else
        m_result_iterator.reset(
            new (&m_result_iterator_holder.sort_file) SortFileIterator<false>(
                thd(), std::move(tables), m_sort_result.io_cache, &m_fs_info,
                m_examined_rows));
    } else {
      m_result_iterator.reset(
          new (&m_result_iterator_holder.sort_file_indirect)
              SortFileIndirectIterator(
                  thd(), std::move(tables), m_sort_result.io_cache,
                  /*ignore_not_found_rows=*/false,
                  /*has_null_flags=*/true, m_examined_rows));
    }
    m_sort_result.io_cache =
        nullptr;  // The result iterator has taken ownership.
  } else {
    assert(m_sort_result.has_result_in_memory());
    if (m_fs_info.using_addon_fields()) {
      DBUG_PRINT("info", ("using SortBufferIterator"));
      assert(m_sort_result.sorted_result_in_fsbuf);
      if (m_fs_info.addon_fields->using_packed_addons())
        m_result_iterator.reset(
            new (&m_result_iterator_holder.sort_buffer_packed_addons)
                SortBufferIterator<true>(thd(), std::move(tables), &m_fs_info,
                                         &m_sort_result, m_examined_rows));
      else
        m_result_iterator.reset(
            new (&m_result_iterator_holder.sort_buffer)
                SortBufferIterator<false>(thd(), std::move(tables), &m_fs_info,
                                          &m_sort_result, m_examined_rows));
    } else {
      DBUG_PRINT("info", ("using SortBufferIndirectIterator (sort)"));
      m_result_iterator.reset(
          new (&m_result_iterator_holder.sort_buffer_indirect)
              SortBufferIndirectIterator(
                  thd(), std::move(tables), &m_sort_result,
                  /*ignore_not_found_rows=*/false,
                  /*has_null_flags=*/true, m_examined_rows));
    }
  }

  return m_result_iterator->Init();
}

void SortingIterator::SetNullRowFlag(bool is_null_row) {
  for (TABLE *table : m_filesort->tables) {
    if (is_null_row) {
      table->set_null_row();
    } else {
      table->reset_null_row();
    }
  }
}

/*
  Do the actual sort, by calling filesort. The result will be left in one of
  several places depending on what sort strategy we chose; it is up to Init() to
  figure out what happened and create the appropriate iterator to read from it.

  RETURN VALUES
    0		ok
    -1		Some fatal error
    1		No records
*/

int SortingIterator::DoSort() {
  assert(m_sort_result.io_cache == nullptr);
  m_sort_result.io_cache =
      (IO_CACHE *)my_malloc(key_memory_TABLE_sort_io_cache, sizeof(IO_CACHE),
                            MYF(MY_WME | MY_ZEROFILL));

  ha_rows found_rows;
  bool error = ::filesort(thd(), m_filesort, m_source_iterator.get(),
                          m_tables_to_get_rowid_for, m_num_rows_estimate,
                          &m_fs_info, &m_sort_result, &found_rows);
  for (TABLE *table : m_filesort->tables) {
    table->set_keyread(false);  // Restore if we used indexes
  }
  return error;
}

template <bool Packed_addon_fields>
inline void Filesort_info::unpack_addon_fields(
    const Mem_root_array<TABLE *> &tables, uchar *buff) {
  const uchar *nulls = buff + addon_fields->skip_bytes();

  // Unpack table NULL flags.
  int table_idx = 0;
  for (TABLE *table : tables) {
    if (table->is_nullable()) {
      if (nulls[table_idx / 8] & (1 << (table_idx & 7))) {
        table->set_null_row();
      } else {
        table->reset_null_row();
      }
      ++table_idx;
    }
  }

  // Unpack the actual addon fields (if any).
  const uchar *start_of_record = buff + addon_fields->first_addon_offset();
  for (const Sort_addon_field &addonf : *addon_fields) {
    Field *field = addonf.field;
    const bool is_null =
        addonf.null_bit && (addonf.null_bit & nulls[addonf.null_offset]);
    if (is_null) {
      field->set_null();
    }
    if (Packed_addon_fields) {
      if (!is_null && !field->table->has_null_row()) {
        field->set_notnull();
        start_of_record = field->unpack(start_of_record);
      }
    } else {
      if (!is_null) {
        field->set_notnull();
        field->unpack(start_of_record);
      }
      start_of_record += addonf.max_length;
    }
  }
}
