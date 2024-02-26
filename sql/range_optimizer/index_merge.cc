/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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

#include "sql/range_optimizer/index_merge.h"

#include <stdio.h>
#include <atomic>
#include <memory>

#include "m_string.h"
#include "map_helpers.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql/handler.h"
#include "sql/iterators/row_iterator.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/key.h"
#include "sql/psi_memory_key.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_select.h"
#include "sql/sql_sort.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"
#include "sql/uniques.h"
#include "sql_string.h"

IndexMergeIterator::IndexMergeIterator(
    THD *thd, MEM_ROOT *return_mem_root, TABLE *table,
    unique_ptr_destroy_only<RowIterator> pk_quick_select,
    Mem_root_array<unique_ptr_destroy_only<RowIterator>> children)
    : TableRowIterator(thd, table),
      pk_quick_select(std::move(pk_quick_select)),
      m_children(std::move(children)),
      mem_root(return_mem_root) {}

IndexMergeIterator::~IndexMergeIterator() {
  DBUG_TRACE;
  bool disable_unique_filter = false;
  for (unique_ptr_destroy_only<RowIterator> &quick : m_children) {
    IndexRangeScanIterator *range =
        down_cast<IndexRangeScanIterator *>(quick.get()->real_iterator());

    // Normally it's disabled by dtor of IndexRangeScanIterator, but it can't be
    // done without table's handler
    disable_unique_filter |=
        Overlaps(table()->key_info[range->index].flags, HA_MULTI_VALUED_KEY);
    range->file = nullptr;
  }
  if (disable_unique_filter)
    table()->file->ha_extra(HA_EXTRA_DISABLE_UNIQUE_RECORD_FILTER);
  /* It's ok to call the next two even if they are already deinitialized */
  read_record.reset();
  free_io_cache(table());
}

/*
  Initialize the iterator for a new scan.

  Perform key scans for all used indexes (except CPK), get rowids and merge
  them into an ordered non-recurrent sequence of rowids.

  The merge/duplicate removal is performed using Unique class. We put all
  rowids into Unique, get the sorted sequence and destroy the Unique.

  If table has a clustered primary key that covers all rows (true for bdb
  and innodb currently) and one of the index_merge scans is a scan on PK,
  then rows that will be retrieved by PK scan are not put into Unique and
  primary key scan is not performed here, it is performed later separately.

  RETURN
    true if error
*/

bool IndexMergeIterator::Init() {
  empty_record(table());

  handler *file = table()->file;
  DBUG_TRACE;

  /* We're going to just read rowids. */
  table()->set_keyread(true);
  table()->prepare_for_position();

  DBUG_EXECUTE_IF("simulate_bug13919180", {
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    return true;
  });

  size_t sort_buffer_size = thd()->variables.sortbuff_size;
#ifndef NDEBUG
  if (DBUG_EVALUATE_IF("sortbuff_size_256", 1, 0)) sort_buffer_size = 256;
#endif /* NDEBUG */

  if (unique == nullptr) {
    DBUG_EXECUTE_IF("index_merge_may_not_create_a_Unique", DBUG_ABORT(););
    DBUG_EXECUTE_IF("only_one_Unique_may_be_created",
                    DBUG_SET("+d,index_merge_may_not_create_a_Unique"););
    unique.reset(new (mem_root) Unique(refpos_order_cmp, (void *)file,
                                       file->ref_length, sort_buffer_size));
    if (unique == nullptr) {
      return true;
    }
  } else {
    unique->reset();
    table()->unique_result.sorted_result.reset();
    assert(!table()->unique_result.sorted_result_in_fsbuf);
    table()->unique_result.sorted_result_in_fsbuf = false;

    if (table()->unique_result.io_cache) {
      close_cached_file(table()->unique_result.io_cache);
      my_free(table()->unique_result.io_cache);
      table()->unique_result.io_cache = nullptr;
    }
  }

  assert(file->ref_length == unique->get_size());
  assert(sort_buffer_size == unique->get_max_in_memory_size());

  {
    const Key_map covering_keys_save = table()->covering_keys;
    const bool no_keyread_save = table()->no_keyread;
    auto reset_keys =
        create_scope_guard([covering_keys_save, no_keyread_save, this] {
          table()->covering_keys = covering_keys_save;
          table()->no_keyread = no_keyread_save;
        });
    table()->no_keyread = false;
    for (unique_ptr_destroy_only<RowIterator> &child : m_children) {
      // Init() might reset table->key_read to false. Take care to let
      // it know that index merge needs to read only index entries.
      IndexRangeScanIterator *range_scan_it =
          down_cast<IndexRangeScanIterator *>(child->real_iterator());
      table()->covering_keys.set_bit(range_scan_it->index);
      if (child->Init()) return true;
      // Make sure that index only access is used.
      assert(table()->key_read == true);

      for (;;) {
        int result = child->Read();
        if (result == -1) {
          break;  // EOF.
        } else if (result != 0 || thd()->killed) {
          return true;
        }

        /* skip row if it will be retrieved by clustered PK scan */
        if (pk_quick_select && down_cast<IndexRangeScanIterator *>(
                                   pk_quick_select.get()->real_iterator())
                                   ->row_in_ranges()) {
          continue;
        }

        handler *child_file = range_scan_it->file;
        child_file->position(table()->record[0]);
        if (unique->unique_add(child_file->ref)) {
          return true;
        }
      }
    }
  }

  /*
    Ok all rowids are in the Unique now. The next call will initialize
    table()->sort structure so it can be used to iterate through the rowids
    sequence.
  */
  if (unique->get(table())) {
    return true;
  }

  doing_pk_scan = false;
  /* index_merge currently doesn't support "using index" at all */
  table()->set_keyread(false);
  read_record.reset();  // Clear out any previous iterator.
  read_record = init_table_iterator(thd(), table(),
                                    /*ignore_not_found_rows=*/false,
                                    /*count_examined_rows=*/false);
  if (read_record == nullptr) return true;
  return false;
}

/*
  Get next row for index_merge.
  NOTES
    The rows are read from
      1. rowids stored in Unique.
      2. IndexRangeScanIterator with clustered primary key (if any).
    The sets of rows retrieved in 1) and 2) are guaranteed to be disjoint.
*/

int IndexMergeIterator::Read() {
  int result;
  DBUG_TRACE;

  if (doing_pk_scan) return pk_quick_select->Read();

  if ((result = read_record->Read()) == -1) {
    // NOTE: destroying the RowIterator also clears
    // table()->unique_result.io_cache if it is initialized, since it
    // owns the io_cache it is reading from.
    read_record.reset();

    /* All rows from Unique have been retrieved, do a clustered PK scan */
    if (pk_quick_select) {
      doing_pk_scan = true;
      if (pk_quick_select->Init()) {
        return 1;
      }
      return pk_quick_select->Read();
    }
  }

  return result;
}
