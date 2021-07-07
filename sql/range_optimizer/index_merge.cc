/* Copyright (c) 2000, 2021, Oracle and/or its affiliates.

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
#include "sql/handler.h"
#include "sql/key.h"
#include "sql/psi_memory_key.h"
#include "sql/records.h"
#include "sql/row_iterator.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_select.h"
#include "sql/sql_sort.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"
#include "sql/uniques.h"
#include "sql_string.h"

struct MY_BITMAP;

QUICK_INDEX_MERGE_SELECT::QUICK_INDEX_MERGE_SELECT(MEM_ROOT *return_mem_root,
                                                   TABLE *table)
    : unique(nullptr), pk_quick_select(nullptr), mem_root(return_mem_root) {
  DBUG_TRACE;
  index = MAX_KEY;
  m_table = table;
}

int QUICK_INDEX_MERGE_SELECT::init() {
  DBUG_TRACE;
  return 0;
}

int QUICK_INDEX_MERGE_SELECT::reset() {
  DBUG_TRACE;
  const int retval = read_keys_and_merge();
  return retval;
}

bool QUICK_INDEX_MERGE_SELECT::push_quick_back(
    QUICK_RANGE_SELECT *quick_sel_range) {
  /*
    Save quick_select that does scan on clustered primary key as it will be
    processed separately.
  */
  if (m_table->file->primary_key_is_clustered() &&
      quick_sel_range->index == m_table->s->primary_key)
    pk_quick_select = quick_sel_range;
  else
    return quick_selects.push_back(quick_sel_range);
  return false;
}

QUICK_INDEX_MERGE_SELECT::~QUICK_INDEX_MERGE_SELECT() {
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT *quick;
  DBUG_TRACE;
  bool disable_unique_filter = false;
  destroy(unique);
  quick_it.rewind();
  while ((quick = quick_it++)) {
    // Normally it's disabled by dtor of QUICK_RANGE_SELECT, but it can't be
    // done without table's handler
    disable_unique_filter |=
        (0 != (m_table->key_info[quick->index].flags & HA_MULTI_VALUED_KEY));
    quick->file = nullptr;
  }
  quick_selects.destroy_elements();
  if (disable_unique_filter)
    m_table->file->ha_extra(HA_EXTRA_DISABLE_UNIQUE_RECORD_FILTER);
  destroy(pk_quick_select);
  /* It's ok to call the next two even if they are already deinitialized */
  read_record.reset();
  free_io_cache(m_table);
}

bool QUICK_INDEX_MERGE_SELECT::is_keys_used(const MY_BITMAP *fields) {
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick = it++)) {
    if (is_key_used(m_table, quick->index, fields)) return true;
  }
  return false;
}

/*
  Perform key scans for all used indexes (except CPK), get rowids and merge
  them into an ordered non-recurrent sequence of rowids.

  The merge/duplicate removal is performed using Unique class. We put all
  rowids into Unique, get the sorted sequence and destroy the Unique.

  If table has a clustered primary key that covers all rows (true for bdb
  and innodb currently) and one of the index_merge scans is a scan on PK,
  then rows that will be retrieved by PK scan are not put into Unique and
  primary key scan is not performed here, it is performed later separately.

  RETURN
    0     OK
    other error
*/

int QUICK_INDEX_MERGE_SELECT::read_keys_and_merge() {
  List_iterator_fast<QUICK_RANGE_SELECT> cur_quick_it(quick_selects);
  QUICK_RANGE_SELECT *cur_quick;
  int result;
  handler *file = m_table->file;
  DBUG_TRACE;

  /* We're going to just read rowids. */
  m_table->set_keyread(true);
  m_table->prepare_for_position();

  cur_quick_it.rewind();
  cur_quick = cur_quick_it++;
  assert(cur_quick != nullptr);

  DBUG_EXECUTE_IF("simulate_bug13919180", {
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    return 1;
  });
  /*
    We reuse the same instance of handler so we need to call both init and
    reset here.
  */
  if (cur_quick->init() || cur_quick->reset()) return 1;

  size_t sort_buffer_size = current_thd->variables.sortbuff_size;
#ifndef NDEBUG
  if (DBUG_EVALUATE_IF("sortbuff_size_256", 1, 0)) sort_buffer_size = 256;
#endif /* NDEBUG */

  if (unique == nullptr) {
    DBUG_EXECUTE_IF("index_merge_may_not_create_a_Unique", DBUG_ABORT(););
    DBUG_EXECUTE_IF("only_one_Unique_may_be_created",
                    DBUG_SET("+d,index_merge_may_not_create_a_Unique"););
    unique = new (mem_root) Unique(refpos_order_cmp, (void *)file,
                                   file->ref_length, sort_buffer_size);
  } else {
    unique->reset();
    m_table->unique_result.sorted_result.reset();
    assert(!m_table->unique_result.sorted_result_in_fsbuf);
    m_table->unique_result.sorted_result_in_fsbuf = false;

    if (m_table->unique_result.io_cache) {
      close_cached_file(m_table->unique_result.io_cache);
      my_free(m_table->unique_result.io_cache);
      m_table->unique_result.io_cache = nullptr;
    }
  }

  assert(file->ref_length == unique->get_size());
  assert(sort_buffer_size == unique->get_max_in_memory_size());

  if (!unique) return 1;
  for (;;) {
    while ((result = cur_quick->get_next()) == HA_ERR_END_OF_FILE) {
      cur_quick->range_end();
      cur_quick = cur_quick_it++;
      if (!cur_quick) break;

      if (cur_quick->file->inited) cur_quick->file->ha_index_end();
      if (cur_quick->init() || cur_quick->reset()) return 1;
    }

    if (result) {
      if (result != HA_ERR_END_OF_FILE) {
        cur_quick->range_end();
        return result;
      }
      break;
    }

    if (current_thd->killed) return 1;

    /* skip row if it will be retrieved by clustered PK scan */
    if (pk_quick_select && pk_quick_select->row_in_ranges()) continue;

    cur_quick->file->position(cur_quick->record);
    result = unique->unique_add((char *)cur_quick->file->ref);
    if (result) return 1;
  }

  /*
    Ok all rowids are in the Unique now. The next call will initialize
    m_table->sort structure so it can be used to iterate through the rowids
    sequence.
  */
  result = unique->get(m_table);
  doing_pk_scan = false;
  /* index_merge currently doesn't support "using index" at all */
  m_table->set_keyread(false);
  read_record.reset();  // Clear out any previous iterator.
  read_record = init_table_iterator(current_thd, m_table,
                                    /*ignore_not_found_rows=*/false,
                                    /*count_examined_rows=*/false);
  if (read_record == nullptr) return 1;
  return result;
}

/*
  Get next row for index_merge.
  NOTES
    The rows are read from
      1. rowids stored in Unique.
      2. QUICK_RANGE_SELECT with clustered primary key (if any).
    The sets of rows retrieved in 1) and 2) are guaranteed to be disjoint.
*/

int QUICK_INDEX_MERGE_SELECT::get_next() {
  int result;
  DBUG_TRACE;

  if (doing_pk_scan) return pk_quick_select->get_next();

  if ((result = read_record->Read()) == -1) {
    result = HA_ERR_END_OF_FILE;

    // NOTE: destroying the RowIterator also clears
    // m_table->unique_result.io_cache if it is initialized, since it
    // owns the io_cache it is reading from.
    read_record.reset();

    /* All rows from Unique have been retrieved, do a clustered PK scan */
    if (pk_quick_select) {
      doing_pk_scan = true;
      if ((result = pk_quick_select->init()) ||
          (result = pk_quick_select->reset()))
        return result;
      return pk_quick_select->get_next();
    }
  }

  return result;
}

void QUICK_INDEX_MERGE_SELECT::add_info_string(String *str) {
  QUICK_RANGE_SELECT *quick;
  bool first = true;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  str->append(STRING_WITH_LEN("sort_union("));
  while ((quick = it++)) {
    if (!first)
      str->append(',');
    else
      first = false;
    quick->add_info_string(str);
  }
  if (pk_quick_select) {
    str->append(',');
    pk_quick_select->add_info_string(str);
  }
  str->append(')');
}

void QUICK_INDEX_MERGE_SELECT::add_keys_and_lengths(String *key_names,
                                                    String *used_lengths) {
  char buf[64];
  size_t length;
  bool first = true;
  QUICK_RANGE_SELECT *quick;

  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick = it++)) {
    if (first)
      first = false;
    else {
      key_names->append(',');
      used_lengths->append(',');
    }

    KEY *key_info = m_table->key_info + quick->index;
    key_names->append(key_info->name);
    length = longlong10_to_str(quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(buf, length);
  }
  if (pk_quick_select) {
    KEY *key_info = m_table->key_info + pk_quick_select->index;
    key_names->append(',');
    key_names->append(key_info->name);
    length =
        longlong10_to_str(pk_quick_select->max_used_key_length, buf, 10) - buf;
    used_lengths->append(',');
    used_lengths->append(buf, length);
  }
}

#ifndef NDEBUG
void QUICK_INDEX_MERGE_SELECT::dbug_dump(int indent, bool verbose) {
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  QUICK_RANGE_SELECT *quick;
  fprintf(DBUG_FILE, "%*squick index_merge select\n", indent, "");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  while ((quick = it++)) quick->dbug_dump(indent + 2, verbose);
  if (pk_quick_select) {
    fprintf(DBUG_FILE, "%*sclustered PK quick:\n", indent, "");
    pk_quick_select->dbug_dump(indent + 2, verbose);
  }
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}
#endif
