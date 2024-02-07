/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/range_optimizer/reverse_index_range_scan.h"

#include <assert.h>

#include "my_base.h"
#include "my_dbug.h"
#include "sql/handler.h"
#include "sql/key.h"
#include "sql/sql_executor.h"
#include "sql/table.h"

ReverseIndexRangeScanIterator::ReverseIndexRangeScanIterator(
    THD *thd, TABLE *table, ha_rows *examined_rows, double expected_rows,
    int index, MEM_ROOT *return_mem_root, uint mrr_flags,
    Bounds_checked_array<QUICK_RANGE *> ranges, bool using_extended_key_parts)
    : TableRowIterator(thd, table),
      m_index(index),
      m_expected_rows(expected_rows),
      m_examined_rows(examined_rows),
      mem_root(return_mem_root),
      m_mrr_flags(mrr_flags),
      ranges(ranges),
      last_range(nullptr),
      m_using_extended_key_parts(using_extended_key_parts) {
  /*
    Use default MRR implementation for reverse scans. No table engine
    currently can do an MRR scan with output in reverse index order.
  */
  m_mrr_flags |= HA_MRR_USE_DEFAULT_IMPL;
  m_mrr_flags |= HA_MRR_SORTED;  // 'sorted' as internals use index_last/_prev

  for (QUICK_RANGE *r : ranges) {
    if ((r->flag & EQ_RANGE) &&
        table->key_info[m_index].key_length != r->max_length) {
      r->flag &= ~EQ_RANGE;
    }
  }

  key_part_info = table->key_info[m_index].key_part;
}

ReverseIndexRangeScanIterator::~ReverseIndexRangeScanIterator() {
  if (table()->key_info[m_index].flags & HA_MULTI_VALUED_KEY && table()->file) {
    table()->file->ha_extra(HA_EXTRA_DISABLE_UNIQUE_RECORD_FILTER);
  }
}

bool ReverseIndexRangeScanIterator::Init() {
  current_range_idx = ranges.size();
  empty_record(table());

  /*
    Only attempt to allocate a record buffer the first time the handler is
    initialized.
  */
  const bool first_init = !table()->file->inited;

  if (!inited) {
    if (column_bitmap.bitmap == nullptr) {
      /* Allocate a bitmap for used columns */
      my_bitmap_map *bitmap =
          (my_bitmap_map *)mem_root->Alloc(table()->s->column_bitmap_size);
      if (bitmap == nullptr) {
        return true;
      }
      bitmap_init(&column_bitmap, bitmap, table()->s->fields);
    }
    inited = true;
  }
  if (table()->file->inited) table()->file->ha_index_or_rnd_end();

  last_range = nullptr;
  if (InitIndexRangeScan(table(), table()->file, m_index, m_mrr_flags,
                         /*in_ror_merged_scan=*/false, &column_bitmap)) {
    return true;
  }

  if (first_init && table()->file->inited) {
    if (set_record_buffer(table(), m_expected_rows)) {
      return true; /* purecov: inspected */
    }
  }

  HANDLER_BUFFER empty_buf;
  empty_buf.buffer = empty_buf.buffer_end = empty_buf.end_of_used_area =
      nullptr;

  RANGE_SEQ_IF seq_funcs = {quick_range_rev_seq_init, quick_range_seq_next,
                            nullptr};
  if (int error = table()->file->multi_range_read_init(
          &seq_funcs, this, ranges.size(), m_mrr_flags, &empty_buf);
      error != 0) {
    (void)report_handler_error(table(), error);
    return true;
  }

  return false;
}

int ReverseIndexRangeScanIterator::Read() {
  DBUG_TRACE;

  /* The max key is handled as follows:
   *   - if there is NO_MAX_RANGE, start at the end and move backwards
   *   - if it is an EQ_RANGE (which means that max key covers the entire
   *     key) and the query does not use any hidden key fields that are
   *     not considered when the range optimzier sets EQ_RANGE (e.g. the
   *     primary key added by InnoDB), then go directly to the key and
   *     read through it (sorting backwards is same as sorting forwards).
   *   - if it is NEAR_MAX, go to the key or next, step back once, and
   *     move backwards
   *   - otherwise (not NEAR_MAX == include the key), go after the key,
   *     step back once, and move backwards
   */

  for (;;) {
    if (last_range != nullptr) {  // Keep on reading from the same key.
      int result = ((last_range->flag & EQ_RANGE && !m_using_extended_key_parts)
                        ? table()->file->ha_index_next_same(
                              table()->record[0], last_range->min_key,
                              last_range->min_length)
                        : table()->file->ha_index_prev(table()->record[0]));
      if (result == 0) {
        if (cmp_prev(last_range) == 0) {
          if (m_examined_rows != nullptr) {
            ++*m_examined_rows;
          }
          return 0;
        }
      } else {
        if (int error_code = HandleError(result); error_code != -1) {
          return error_code;
        }
      }
    }

    // EOF from this range, so read the next one.
    if (current_range_idx == 0) {
      return -1;  // No more ranges.
    }
    last_range = ranges[--current_range_idx];

    // Case where we can avoid descending scan, see comment above
    const bool eqrange_all_keyparts =
        (last_range->flag & EQ_RANGE) && !m_using_extended_key_parts;

    /*
      If we have pushed an index condition (ICP) and this quick select
      will use ha_index_prev() to read data, we need to let the
      handler know where to end the scan in order to avoid that the
      ICP implementation continues to read past the range boundary.
    */
    if (table()->file->pushed_idx_cond) {
      if (!eqrange_all_keyparts) {
        key_range min_range;
        last_range->make_min_endpoint(&min_range);
        if (min_range.length > 0)
          table()->file->set_end_range(&min_range, handler::RANGE_SCAN_DESC);
        else
          table()->file->set_end_range(nullptr, handler::RANGE_SCAN_DESC);
      } else {
        /*
          Will use ha_index_next_same() for reading records. In case we have
          set the end range for an earlier range, this need to be cleared.
        */
        table()->file->set_end_range(nullptr, handler::RANGE_SCAN_ASC);
      }
    }

    if (last_range->flag & NO_MAX_RANGE)  // Read last record
    {
      if (int result = table()->file->ha_index_last(table()->record[0]);
          result != 0) {
        /*
          HA_ERR_END_OF_FILE is returned both when the table is empty and when
          there are no qualifying records in the range (when using ICP).
          Interpret this return value as "no qualifying rows in the range" to
          avoid loss of records. If the error code truly meant "empty table"
          the next iteration of the loop will exit.
        */
        if (int error_code = HandleError(result); error_code != -1) {
          return error_code;
        }
        last_range = nullptr;  // Go to next range
        continue;
      }

      if (cmp_prev(last_range) == 0) {
        if (m_examined_rows != nullptr) {
          ++*m_examined_rows;
        }
        return 0;
      }
      last_range = nullptr;  // No match; go to next range
      continue;
    }

    int result;
    if (eqrange_all_keyparts) {
      result = table()->file->ha_index_read_map(
          table()->record[0], last_range->max_key, last_range->max_keypart_map,
          HA_READ_KEY_EXACT);
    } else {
      assert(last_range->flag & NEAR_MAX ||
             (last_range->flag & EQ_RANGE && m_using_extended_key_parts) ||
             range_reads_after_key(last_range));
      result = table()->file->ha_index_read_map(
          table()->record[0], last_range->max_key, last_range->max_keypart_map,
          ((last_range->flag & NEAR_MAX) ? HA_READ_BEFORE_KEY
                                         : HA_READ_PREFIX_LAST_OR_PREV));
    }
    if (result != 0) {
      if (int error_code = HandleError(result); error_code != -1) {
        return error_code;
      }
      last_range = nullptr;  // Not found, to next range
      continue;
    }
    if (cmp_prev(last_range) == 0) {
      if ((last_range->flag & (UNIQUE_RANGE | EQ_RANGE)) ==
          (UNIQUE_RANGE | EQ_RANGE))
        last_range = nullptr;  // Stop searching
      if (m_examined_rows != nullptr) {
        ++*m_examined_rows;
      }
      return 0;  // Found key is in range
    }
    last_range = nullptr;  // To next range
  }
}
/*
  true if this range will require using HA_READ_AFTER_KEY
  See comment in Read() about this
*/

bool ReverseIndexRangeScanIterator::range_reads_after_key(
    QUICK_RANGE *range_arg) {
  return ((range_arg->flag & (NO_MAX_RANGE | NEAR_MAX)) ||
          !(range_arg->flag & EQ_RANGE) ||
          table()->key_info[m_index].key_length != range_arg->max_length)
             ? true
             : false;
}

/*
  Returns 0 if found key is inside range (found key >= range->min_key).
*/

int ReverseIndexRangeScanIterator::cmp_prev(QUICK_RANGE *range_arg) {
  int cmp;
  if (range_arg->flag & NO_MIN_RANGE) return 0; /* key can't be to small */

  cmp = key_cmp(key_part_info, range_arg->min_key, range_arg->min_length);
  if (cmp > 0 || (cmp == 0 && !(range_arg->flag & NEAR_MIN))) return 0;
  return 1;  // outside of range
}

// Pretty much the same as quick_range_seq_init(), just with a different class.
range_seq_t ReverseIndexRangeScanIterator::quick_range_rev_seq_init(
    void *init_param, uint, uint) {
  ReverseIndexRangeScanIterator *quick =
      static_cast<ReverseIndexRangeScanIterator *>(init_param);
  QUICK_RANGE **first = quick->ranges.begin();
  QUICK_RANGE **last = quick->ranges.end();
  quick->qr_traversal_ctx.first = first;
  quick->qr_traversal_ctx.cur = first;
  quick->qr_traversal_ctx.last = last;
  return &quick->qr_traversal_ctx;
}
