/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  Functions for easy reading of records, possible through a cache
*/

#include "sql/records.h"

#include <string.h>
#include <algorithm>
#include <atomic>
#include <new>

#include "map_helpers.h"
#include "my_base.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/opt_range.h"  // QUICK_SELECT_I
#include "sql/psi_memory_key.h"
#include "sql/sort_param.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_sort.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "thr_lock.h"
#include "varlen_sort.h"

/**
  Initialize READ_RECORD structure to perform full index scan in desired
  direction using read_record.read_record() interface

    This function has been added at late stage and is used only by
    UPDATE/DELETE. Other statements perform index scans using
    join_read_first/next functions.

  @param info         READ_RECORD structure to initialize.
  @param thd          Thread handle
  @param table        Table to be accessed
  @param idx          index to scan
  @param reverse      Scan in the reverse direction

  @retval true   error
  @retval false  success
*/

bool init_read_record_idx(READ_RECORD *info, THD *thd, TABLE *table, uint idx,
                          bool reverse) {
  empty_record(table);
  new (info) READ_RECORD;

  unique_ptr_destroy_only<RowIterator> iterator;

  info->read_record = rr_iterator;
  if (reverse) {
    iterator.reset(new (&info->iterator_holder.index_scan_reverse)
                       IndexScanIterator<true>(thd, table, idx));
  } else {
    iterator.reset(new (&info->iterator_holder.index_scan)
                       IndexScanIterator<false>(thd, table, idx));
  }
  if (iterator->Init(nullptr)) {
    return true;
  }
  info->iterator = std::move(iterator);
  return false;
}

template <bool Reverse>
IndexScanIterator<Reverse>::IndexScanIterator(THD *thd, TABLE *table, int idx)
    : RowIterator(thd, table), m_record(table->record[0]), m_idx(idx) {}

template <bool Reverse>
bool IndexScanIterator<Reverse>::Init(QEP_TAB *qep_tab) {
  if (!table()->file->inited) {
    int error = table()->file->ha_index_init(m_idx, 1);
    if (error) {
      PrintError(error);
      return true;
    }
  }
  PushDownCondition(qep_tab);
  return false;
}

// Doxygen gets confused by the explicit specializations.

//! @cond
template <>
int IndexScanIterator<false>::Read() {  // Forward read.
  int error;
  if (m_first) {
    error = table()->file->ha_index_first(m_record);
    m_first = false;
  } else {
    error = table()->file->ha_index_next(m_record);
  }
  if (error) return HandleError(error);
  return 0;
}

template <>
int IndexScanIterator<true>::Read() {  // Backward read.
  int error;
  if (m_first) {
    error = table()->file->ha_index_last(m_record);
    m_first = false;
  } else {
    error = table()->file->ha_index_prev(m_record);
  }
  if (error) return HandleError(error);
  return 0;
}
//! @endcond

/*
  init_read_record is used to scan by using a number of different methods.
  Which method to use is set-up in this call so that you can fetch rows
  through the resulting row iterator afterwards.

  SYNOPSIS
    init_read_record()
      info              OUT read structure
      thd               Thread handle
      table             Table the data [originally] comes from; if NULL,
      'table' is inferred from 'qep_tab'; if non-NULL, 'qep_tab' must be NULL.
      qep_tab           QEP_TAB for 'table', if there is one; we may use
      qep_tab->quick() as data source
      disable_rr_cache  Don't use rr_from_cache (used by sort-union
                        index-merge which produces rowid sequences that
                        are already ordered)
      ignore_not_found_rows
                        Ignore any rows not found in reference tables,
                        as they may already have been deleted by foreign key
                        handling. Only relevant for methods that need to
                        look up rows in tables (those marked “Indirect”).

  @retval true   error
  @retval false  success
*/
bool init_read_record(READ_RECORD *info, THD *thd, TABLE *table,
                      QEP_TAB *qep_tab, bool disable_rr_cache,
                      bool ignore_not_found_rows) {
  DBUG_ENTER("init_read_record");

  // If only 'table' is given, assume no quick, no condition.
  DBUG_ASSERT(!(table && qep_tab));
  if (!table) table = qep_tab->table();
  empty_record(table);

  new (info) READ_RECORD;
  info->table = table;
  info->unlock_row = rr_unlock_row;
  info->read_record = rr_iterator;

  unique_ptr_destroy_only<RowIterator> iterator;

  QUICK_SELECT_I *quick = qep_tab ? qep_tab->quick() : NULL;
  if (quick && quick->clustered_pk_range()) {
    /*
      In case of QUICK_INDEX_MERGE_SELECT with clustered pk range we have to
      use its own access method(i.e QUICK_INDEX_MERGE_SELECT::get_next()) as
      sort file does not contain rowids which satisfy clustered pk range.
    */
    DBUG_PRINT("info", ("using IndexRangeScanIterator"));
    iterator.reset(new (&info->iterator_holder.index_range_scan)
                       IndexRangeScanIterator(thd, table, quick));
  }
  /*
    We test for a Unique result before a filesort result, because on
    any given table, we can have Unique sending its result to filesort
    (in which case filesort would be half-initialized at this point),
    but not the other way round. It's possible that we should actually
    have a “finished” flag instead, though.
  */
  else if (table->unique_result.io_cache &&
           my_b_inited(table->unique_result.io_cache)) {
    DBUG_PRINT("info", ("using SortFileIndirectIterator"));
    iterator.reset(
        new (&info->iterator_holder.sort_file_indirect)
            SortFileIndirectIterator(thd, table, table->unique_result.io_cache,
                                     !disable_rr_cache, ignore_not_found_rows));
  } else if (table->sort_result.io_cache &&
             my_b_inited(table->sort_result.io_cache)) {
    // Test if ref-records was used
    if (table->sort.using_addon_fields()) {
      DBUG_PRINT("info", ("using SortFileIterator"));
      if (table->sort.addon_fields->using_packed_addons())
        iterator.reset(
            new (&info->iterator_holder.sort_file_packed_addons)
                SortFileIterator<true>(thd, table, table->sort_result.io_cache,
                                       &table->sort));
      else
        iterator.reset(
            new (&info->iterator_holder.sort_file) SortFileIterator<false>(
                thd, table, table->sort_result.io_cache, &table->sort));
    } else {
      iterator.reset(new (&info->iterator_holder.sort_file_indirect)
                         SortFileIndirectIterator(
                             thd, table, table->sort_result.io_cache,
                             !disable_rr_cache, ignore_not_found_rows));
    }
  } else if (quick) {
    DBUG_PRINT("info", ("using IndexRangeScanIterator"));
    iterator.reset(new (&info->iterator_holder.index_range_scan)
                       IndexRangeScanIterator(thd, table, quick));
  }
  /*
    See further up in the function for why we test for Unique before filesort.
  */
  else if (table->unique_result.has_result_in_memory()) {
    /*
      The Unique class never puts its results into table->sort's
      Filesort_buffer.
    */
    DBUG_ASSERT(!table->unique_result.sorted_result_in_fsbuf);
    DBUG_PRINT("info", ("using SortBufferIndirectIterator (unique)"));
    iterator.reset(
        new (&info->iterator_holder.sort_buffer_indirect)
            SortBufferIndirectIterator(thd, table, &table->unique_result,
                                       ignore_not_found_rows));
  }
  // See save_index(), which stores the filesort result set.
  else if (table->sort_result.has_result_in_memory()) {
    if (table->sort.using_addon_fields()) {
      DBUG_PRINT("info", ("using SortBufferIterator"));
      DBUG_ASSERT(table->sort_result.sorted_result_in_fsbuf);
      if (table->sort.addon_fields->using_packed_addons())
        iterator.reset(new (&info->iterator_holder.sort_buffer_packed_addons)
                           SortBufferIterator<true>(thd, table, &table->sort,
                                                    &table->sort_result));
      else
        iterator.reset(new (&info->iterator_holder.sort_buffer)
                           SortBufferIterator<false>(thd, table, &table->sort,
                                                     &table->sort_result));
    } else {
      DBUG_PRINT("info", ("using SortBufferIndirectIterator (sort)"));
      iterator.reset(
          new (&info->iterator_holder.sort_buffer_indirect)
              SortBufferIndirectIterator(thd, table, &table->sort_result,
                                         ignore_not_found_rows));
    }
  } else {
    DBUG_PRINT("info", ("using TableScanIterator"));
    iterator.reset(new (&info->iterator_holder.table_scan)
                       TableScanIterator(thd, table));
  }

  if (iterator->Init(qep_tab)) {
    DBUG_RETURN(true);
  }
  info->iterator = std::move(iterator);
  DBUG_RETURN(false);
} /* init_read_record */

void end_read_record(READ_RECORD *info) {
  if (info->iterator) {
    info->iterator.reset();
  }
  if (info->table && info->table->key_read) {
    info->table->set_keyread(false);
  }
}

int RowIterator::HandleError(int error) {
  if (thd()->killed) {
    m_thd->send_kill_message();
    return 1;
  }

  if (error == HA_ERR_END_OF_FILE) {
    return -1;
  } else {
    PrintError(error);
    if (error < 0)  // Fix negative BDB errno
      return 1;
    return error;
  }
}

void RowIterator::PrintError(int error) {
  m_table->file->print_error(error, MYF(0));
}

/*
  Do condition pushdown for UPDATE/DELETE.
  TODO: Remove this from here as it causes two condition pushdown calls
  when we're running a SELECT and the condition cannot be pushed down.
  Some temporary tables do not have a TABLE_LIST object, and it is never
  needed to push down conditions (ECP) for such tables.
*/
void RowIterator::PushDownCondition(QEP_TAB *qep_tab) {
  if (m_thd->optimizer_switch_flag(
          OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN) &&
      qep_tab && qep_tab->condition() && m_table->pos_in_table_list &&
      (qep_tab->condition()->used_tables() &
       m_table->pos_in_table_list->map()) &&
      !m_table->file->pushed_cond) {
    m_table->file->cond_push(qep_tab->condition());
  }
}

IndexRangeScanIterator::IndexRangeScanIterator(THD *thd, TABLE *table,
                                               QUICK_SELECT_I *quick)
    : RowIterator(thd, table), m_quick(quick) {}

bool IndexRangeScanIterator::Init(QEP_TAB *qep_tab) {
  PushDownCondition(qep_tab);
  return false;
}

int IndexRangeScanIterator::Read() {
  int tmp;
  while ((tmp = m_quick->get_next())) {
    if (thd()->killed || (tmp != HA_ERR_RECORD_DELETED)) {
      return HandleError(tmp);
    }
  }

  return 0;
}

// Temporary adapter.
int rr_iterator(READ_RECORD *info) { return info->iterator->Read(); }

TableScanIterator::TableScanIterator(THD *thd, TABLE *table)
    : RowIterator(thd, table), m_record(table->record[0]) {}

TableScanIterator::~TableScanIterator() {
  table()->file->ha_index_or_rnd_end();
}

bool TableScanIterator::Init(QEP_TAB *qep_tab) {
  int error = table()->file->ha_rnd_init(1);
  if (error) {
    PrintError(error);
    return true;
  }

  PushDownCondition(qep_tab);

  return false;
}

int TableScanIterator::Read() {
  int tmp;
  while ((tmp = table()->file->ha_rnd_next(m_record))) {
    /*
      ha_rnd_next can return RECORD_DELETED for MyISAM when one thread is
      reading and another deleting without locks.
    */
    if (tmp == HA_ERR_RECORD_DELETED && !thd()->killed) continue;
    return HandleError(tmp);
  }
  return 0;
}

SortFileIndirectIterator::SortFileIndirectIterator(THD *thd, TABLE *table,
                                                   IO_CACHE *tempfile,
                                                   bool request_cache,
                                                   bool ignore_not_found_rows)
    : RowIterator(thd, table),
      m_io_cache(tempfile),
      m_record(table->record[0]),
      m_ref_pos(table->file->ref),
      m_ignore_not_found_rows(ignore_not_found_rows),
      m_using_cache(request_cache),
      m_ref_length(table->file->ref_length) {}

SortFileIndirectIterator::~SortFileIndirectIterator() {
  (void)table()->file->ha_index_or_rnd_end();
}

bool SortFileIndirectIterator::Init(QEP_TAB *qep_tab) {
  if (!table()->file->inited) {
    int error = table()->file->ha_rnd_init(0);
    if (error) {
      PrintError(error);
      return true;
    }
  }

  /*
    table->sort.addon_field is checked because if we use addon fields,
    it doesn't make sense to use cache - we don't read from the table
    and table->sort.io_cache is read sequentially
  */
  if (m_using_cache && !table()->sort.using_addon_fields() &&
      thd()->variables.read_rnd_buff_size &&
      !(table()->file->ha_table_flags() & HA_FAST_KEY_READ) &&
      (table()->db_stat & HA_READ_ONLY ||
       table()->reginfo.lock_type <= TL_READ_NO_INSERT) &&
      (ulonglong)table()->s->reclength *
              (table()->file->stats.records + table()->file->stats.deleted) >
          (ulonglong)MIN_FILE_LENGTH_TO_USE_ROW_CACHE &&
      m_io_cache->end_of_file / m_ref_length * table()->s->reclength >
          (my_off_t)MIN_ROWS_TO_USE_TABLE_CACHE &&
      !table()->s->blob_fields && m_ref_length <= MAX_REFLENGTH) {
    m_using_cache = !InitCache();
  } else {
    m_using_cache = false;
  }

  PushDownCondition(qep_tab);

  DBUG_PRINT("info", ("using cache: %d", m_using_cache));
  return false;
}

/**
  Initialize caching of records from temporary file.

  @retval
    false OK, use caching.
    true  Buffer is too small, or cannot be allocated.
          Skip caching, and read records directly from temporary file.
 */
bool SortFileIndirectIterator::InitCache() {
  m_struct_length = 3 + MAX_REFLENGTH;
  m_reclength = ALIGN_SIZE(table()->s->reclength + 1);
  if (m_reclength < m_struct_length) m_reclength = ALIGN_SIZE(m_struct_length);

  m_error_offset = table()->s->reclength;
  m_cache_records =
      thd()->variables.read_rnd_buff_size / (m_reclength + m_struct_length);
  uint rec_cache_size = m_cache_records * m_reclength;
  m_rec_cache_size = m_cache_records * m_ref_length;

  if (m_cache_records <= 2) {
    return true;
  }

  m_cache.reset((uchar *)my_malloc(
      key_memory_READ_RECORD_cache,
      rec_cache_size + m_cache_records * m_struct_length, MYF(0)));
  if (m_cache == nullptr) {
    return true;
  }
  DBUG_PRINT("info", ("Allocated buffer for %d records", m_cache_records));
  m_read_positions = m_cache.get() + rec_cache_size;
  m_cache_pos = m_cache_end = m_cache.get();
  return false;
}

int SortFileIndirectIterator::Read() {
  if (m_using_cache) {
    return CachedRead();
  } else {
    return UncachedRead();
  }
}

int SortFileIndirectIterator::UncachedRead() {
  for (;;) {
    if (my_b_read(m_io_cache, m_ref_pos, m_ref_length))
      return -1; /* End of file */
    int tmp = table()->file->ha_rnd_pos(m_record, m_ref_pos);
    if (tmp == 0) return 0;
    /* The following is extremely unlikely to happen */
    if (tmp == HA_ERR_RECORD_DELETED ||
        (tmp == HA_ERR_KEY_NOT_FOUND && m_ignore_not_found_rows))
      continue;
    return HandleError(tmp);
  }
}

int SortFileIndirectIterator::CachedRead() {
  uint i;
  ulong length;
  my_off_t rest_of_file;
  int16 error;
  uchar *position, *ref_position, *record_pos;
  ulong record;

  for (;;) {
    if (m_cache_pos != m_cache_end) {
      if (m_cache_pos[m_error_offset]) {
        shortget(&error, m_cache_pos);
        if (error == HA_ERR_KEY_NOT_FOUND && m_ignore_not_found_rows) {
          m_cache_pos += m_reclength;
          continue;
        }
        PrintError(error);
      } else {
        error = 0;
        memcpy(m_record, m_cache_pos, (size_t)table()->s->reclength);
      }
      m_cache_pos += m_reclength;
      return ((int)error);
    }
    length = m_rec_cache_size;
    rest_of_file = m_io_cache->end_of_file - my_b_tell(m_io_cache);
    if ((my_off_t)length > rest_of_file) length = (ulong)rest_of_file;
    if (!length || my_b_read(m_io_cache, m_cache.get(), length)) {
      DBUG_PRINT("info", ("Found end of file"));
      return -1; /* End of file */
    }

    length /= m_ref_length;
    position = m_cache.get();
    ref_position = m_read_positions;
    for (i = 0; i < length; i++, position += m_ref_length) {
      memcpy(ref_position, position, (size_t)m_ref_length);
      ref_position += MAX_REFLENGTH;
      int3store(ref_position, (long)i);
      ref_position += 3;
    }
    size_t ref_length = m_ref_length;
    DBUG_ASSERT(ref_length <= MAX_REFLENGTH);
    varlen_sort(m_read_positions, m_read_positions + length * m_struct_length,
                m_struct_length, [ref_length](const uchar *a, const uchar *b) {
                  return memcmp(a, b, ref_length) < 0;
                });

    position = m_read_positions;
    for (i = 0; i < length; i++) {
      memcpy(m_ref_pos, position, (size_t)m_ref_length);
      position += MAX_REFLENGTH;
      record = uint3korr(position);
      position += 3;
      record_pos = m_cache.get() + record * m_reclength;
      error = (int16)table()->file->ha_rnd_pos(record_pos, m_ref_pos);
      if (error) {
        record_pos[m_error_offset] = 1;
        shortstore(record_pos, error);
        DBUG_PRINT("error", ("Got error: %d:%d when reading row", my_errno(),
                             (int)error));
      } else
        record_pos[m_error_offset] = 0;
    }
    m_cache_pos = m_cache.get();
    m_cache_end = m_cache_pos + length * m_reclength;
  }
}

template <bool Packed_addon_fields>
inline void Filesort_info::unpack_addon_fields(uchar *buff) {
  Sort_addon_field *addonf = addon_fields->begin();

  const uchar *start_of_record = buff + addonf->offset;

  for (; addonf != addon_fields->end(); ++addonf) {
    Field *field = addonf->field;
    if (addonf->null_bit && (addonf->null_bit & buff[addonf->null_offset])) {
      field->set_null();
      continue;
    }
    field->set_notnull();
    if (Packed_addon_fields)
      start_of_record = field->unpack(field->ptr, start_of_record);
    else
      field->unpack(field->ptr, buff + addonf->offset);
  }
}

template <bool Packed_addon_fields>
SortFileIterator<Packed_addon_fields>::SortFileIterator(THD *thd, TABLE *table,
                                                        IO_CACHE *tempfile,
                                                        Filesort_info *sort)
    : RowIterator(thd, table),
      m_rec_buf(sort->addon_fields->get_addon_buf()),
      m_ref_length(sort->addon_fields->get_addon_buf_length()),
      m_io_cache(tempfile),
      m_sort(sort) {}

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
#ifndef DBUG_OFF
  my_off_t where = my_b_tell(m_io_cache);
#endif
  if (Packed_addon_fields) {
    const uint len_sz = Addon_fields::size_of_length_field;

    // First read length of the record.
    if (my_b_read(m_io_cache, destination, len_sz)) return -1;
    uint res_length = Addon_fields::read_addon_length(destination);
    DBUG_PRINT("info",
               ("rr_unpack from %llu to %p sz %u",
                static_cast<ulonglong>(where), destination, res_length));
    DBUG_ASSERT(res_length > len_sz);
    DBUG_ASSERT(m_sort->using_addon_fields());

    // Then read the rest of the record.
    if (my_b_read(m_io_cache, destination + len_sz, res_length - len_sz))
      return -1; /* purecov: inspected */
  } else {
    if (my_b_read(m_io_cache, destination, m_ref_length)) return -1;
  }

  m_sort->unpack_addon_fields<Packed_addon_fields>(destination);

  return 0;
}

template <bool Packed_addon_fields>
SortBufferIterator<Packed_addon_fields>::SortBufferIterator(
    THD *thd, TABLE *table, Filesort_info *sort, Sort_result *sort_result)
    : RowIterator(thd, table), m_sort(sort), m_sort_result(sort_result) {}

template <bool Packed_addon_fields>
SortBufferIterator<Packed_addon_fields>::~SortBufferIterator() {
  m_sort_result->sorted_result.reset();
  m_sort_result->sorted_result_in_fsbuf = false;
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
  m_sort->unpack_addon_fields<Packed_addon_fields>(payload);
  return 0;
}

SortBufferIndirectIterator::SortBufferIndirectIterator(
    THD *thd, TABLE *table, Sort_result *sort_result,
    bool ignore_not_found_rows)
    : RowIterator(thd, table),
      m_sort_result(sort_result),
      m_ref_length(table->file->ref_length),
      m_record(table->record[0]),
      m_cache_pos(sort_result->sorted_result.get()),
      m_cache_end(m_cache_pos +
                  sort_result->found_records * table->file->ref_length),
      m_ignore_not_found_rows(ignore_not_found_rows) {}

SortBufferIndirectIterator::~SortBufferIndirectIterator() {
  m_sort_result->sorted_result.reset();
  DBUG_ASSERT(!m_sort_result->sorted_result_in_fsbuf);
  m_sort_result->sorted_result_in_fsbuf = false;

  (void)table()->file->ha_index_or_rnd_end();
}

bool SortBufferIndirectIterator::Init(QEP_TAB *qep_tab) {
  int error = table()->file->ha_rnd_init(0);
  if (error) {
    PrintError(error);
    return true;
  }
  PushDownCondition(qep_tab);
  return false;
}

int SortBufferIndirectIterator::Read() {
  for (;;) {
    if (m_cache_pos == m_cache_end) return -1; /* End of file */
    uchar *cache_pos = m_cache_pos;
    m_cache_pos += m_ref_length;

    int tmp = table()->file->ha_rnd_pos(m_record, cache_pos);
    if (tmp == 0) {
      return 0;
    }

    /* The following is extremely unlikely to happen */
    if (tmp == HA_ERR_RECORD_DELETED ||
        (tmp == HA_ERR_KEY_NOT_FOUND && m_ignore_not_found_rows))
      continue;
    return HandleError(tmp);
  }
}

/**
  The default implementation of unlock-row method of READ_RECORD,
  used in all access methods.
*/

void rr_unlock_row(QEP_TAB *tab) {
  READ_RECORD *info = &tab->read_record;
  info->table->file->unlock_row();
}
