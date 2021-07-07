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

#include "sql/range_optimizer/range_scan_desc.h"

#include <assert.h>

#include "my_base.h"
#include "my_dbug.h"
#include "sql/handler.h"
#include "sql/key.h"
#include "sql/table.h"

/*
  This is a hack: we inherit from QUICK_RANGE_SELECT so that we can use the
  get_next() interface, but we have to hold a pointer to the original
  QUICK_RANGE_SELECT because its data are used all over the place. What
  should be done is to factor out the data that is needed into a base
  class (QUICK_SELECT), and then have two subclasses (_ASC and _DESC)
  which handle the ranges and implement the get_next() function.  But
  for now, this seems to work right at least.
*/

QUICK_SELECT_DESC::QUICK_SELECT_DESC(QUICK_RANGE_SELECT &&q,
                                     uint used_key_parts_arg)
    : QUICK_RANGE_SELECT(std::move(q)),
      rev_it(rev_ranges),
      m_used_key_parts(used_key_parts_arg) {
  QUICK_RANGE *r;
  /*
    Use default MRR implementation for reverse scans. No table engine
    currently can do an MRR scan with output in reverse index order.
  */
  mrr_buf_desc = nullptr;
  mrr_flags |= HA_MRR_USE_DEFAULT_IMPL;
  mrr_flags |= HA_MRR_SORTED;  // 'sorted' as internals use index_last/_prev
  mrr_buf_size = 0;

  Quick_ranges::const_iterator pr = ranges.begin();
  Quick_ranges::const_iterator end_range = ranges.end();
  for (; pr != end_range; pr++) {
    rev_ranges.push_front(*pr);
  }

  /* Remove EQ_RANGE flag for keys that are not using the full key */
  for (r = rev_it++; r; r = rev_it++) {
    if ((r->flag & EQ_RANGE) &&
        m_table->key_info[index].key_length != r->max_length)
      r->flag &= ~EQ_RANGE;
  }
  rev_it.rewind();
  q.dont_free = true;  // Don't free shared mem
}

int QUICK_SELECT_DESC::get_next() {
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
    int result;
    if (last_range) {  // Already read through key
      result =
          ((last_range->flag & EQ_RANGE &&
            m_used_key_parts <= m_table->key_info[index].user_defined_key_parts)
               ? file->ha_index_next_same(record, last_range->min_key,
                                          last_range->min_length)
               : file->ha_index_prev(record));
      if (!result) {
        if (cmp_prev(*rev_it.ref()) == 0) return 0;
      } else if (result != HA_ERR_END_OF_FILE)
        return result;
    }

    if (!(last_range = rev_it++)) return HA_ERR_END_OF_FILE;  // All ranges used

    // Case where we can avoid descending scan, see comment above
    const bool eqrange_all_keyparts =
        (last_range->flag & EQ_RANGE) &&
        (m_used_key_parts <= m_table->key_info[index].user_defined_key_parts);

    /*
      If we have pushed an index condition (ICP) and this quick select
      will use ha_index_prev() to read data, we need to let the
      handler know where to end the scan in order to avoid that the
      ICP implemention continues to read past the range boundary.
    */
    if (file->pushed_idx_cond) {
      if (!eqrange_all_keyparts) {
        key_range min_range;
        last_range->make_min_endpoint(&min_range);
        if (min_range.length > 0)
          file->set_end_range(&min_range, handler::RANGE_SCAN_DESC);
        else
          file->set_end_range(nullptr, handler::RANGE_SCAN_DESC);
      } else {
        /*
          Will use ha_index_next_same() for reading records. In case we have
          set the end range for an earlier range, this need to be cleared.
        */
        file->set_end_range(nullptr, handler::RANGE_SCAN_ASC);
      }
    }

    if (last_range->flag & NO_MAX_RANGE)  // Read last record
    {
      int local_error;
      if ((local_error = file->ha_index_last(record))) {
        /*
          HA_ERR_END_OF_FILE is returned both when the table is empty and when
          there are no qualifying records in the range (when using ICP).
          Interpret this return value as "no qualifying rows in the range" to
          avoid loss of records. If the error code truly meant "empty table"
          the next iteration of the loop will exit.
        */
        if (local_error != HA_ERR_END_OF_FILE) return local_error;
        last_range = nullptr;  // Go to next range
        continue;
      }

      if (cmp_prev(last_range) == 0) return 0;
      last_range = nullptr;  // No match; go to next range
      continue;
    }

    if (eqrange_all_keyparts)

    {
      result = file->ha_index_read_map(record, last_range->max_key,
                                       last_range->max_keypart_map,
                                       HA_READ_KEY_EXACT);
    } else {
      assert(last_range->flag & NEAR_MAX ||
             (last_range->flag & EQ_RANGE &&
              m_used_key_parts >
                  m_table->key_info[index].user_defined_key_parts) ||
             range_reads_after_key(last_range));
      result = file->ha_index_read_map(
          record, last_range->max_key, last_range->max_keypart_map,
          ((last_range->flag & NEAR_MAX) ? HA_READ_BEFORE_KEY
                                         : HA_READ_PREFIX_LAST_OR_PREV));
    }
    if (result) {
      if (result != HA_ERR_KEY_NOT_FOUND && result != HA_ERR_END_OF_FILE)
        return result;
      last_range = nullptr;  // Not found, to next range
      continue;
    }
    if (cmp_prev(last_range) == 0) {
      if ((last_range->flag & (UNIQUE_RANGE | EQ_RANGE)) ==
          (UNIQUE_RANGE | EQ_RANGE))
        last_range = nullptr;  // Stop searching
      return 0;                // Found key is in range
    }
    last_range = nullptr;  // To next range
  }
}
/*
  true if this range will require using HA_READ_AFTER_KEY
  See comment in get_next() about this
*/

bool QUICK_SELECT_DESC::range_reads_after_key(QUICK_RANGE *range_arg) {
  return ((range_arg->flag & (NO_MAX_RANGE | NEAR_MAX)) ||
          !(range_arg->flag & EQ_RANGE) ||
          m_table->key_info[index].key_length != range_arg->max_length)
             ? true
             : false;
}
