/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

#include "sql/range_optimizer/index_skip_scan.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "m_ctype.h"
#include "m_string.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/handler.h"
#include "sql/psi_memory_key.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_class.h"
#include "sql/sql_optimizer.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"
#include "sql_string.h"

/**
  Construct new quick select for queries that can do skip scans.
  See get_best_skip_scan() description for more details.

  SYNOPSIS
    IndexSkipScanIterator::IndexSkipScanIterator()
    table              The table being accessed
    index_info         The index chosen for data access
    use_index          The id of index_info
    range_part         The keypart belonging to the range condition C
    index_range_tree   The complete range key
    eq_prefix_len      Length of the equality prefix key
    eq_prefix_key_parts  Number of keyparts in the equality prefix
    eq_prefixes        Array of equality constants (IN list)
    used_key_parts_arg Total number of keyparts A_1,...,C
    read_cost_arg      Cost of this access method
    read_records       Number of records returned
    return_mem_root    Memory pool for this class

  RETURN
    None
*/

IndexSkipScanIterator::IndexSkipScanIterator(
    THD *thd, TABLE *table_arg, KEY *index_info, uint use_index,
    uint eq_prefix_len, uint eq_prefix_key_parts, EQPrefix *eq_prefixes,
    uint used_key_parts_arg, MEM_ROOT *return_mem_root,
    bool has_aggregate_function, uchar *min_range_key_arg,
    uchar *max_range_key_arg, uchar *min_search_key_arg,
    uchar *max_search_key_arg, uint range_cond_flag_arg, uint range_key_len_arg)
    : TableRowIterator(thd, table_arg),
      index_info(index_info),
      eq_prefix_len(eq_prefix_len),
      eq_prefix_key_parts(eq_prefix_key_parts),
      eq_prefixes(eq_prefixes),
      distinct_prefix(nullptr),
      mem_root(return_mem_root),
      range_key_len(range_key_len_arg),
      seen_first_key(false),
      min_range_key(min_range_key_arg),
      max_range_key(max_range_key_arg),
      min_search_key(min_search_key_arg),
      max_search_key(max_search_key_arg),
      range_cond_flag(range_cond_flag_arg),
      has_aggregate_function(has_aggregate_function) {
  index = use_index;

  used_key_parts = used_key_parts_arg;
  max_used_key_length = 0;
  distinct_prefix_len = 0;
  KEY_PART_INFO *p = index_info->key_part;

  my_bitmap_map *bitmap;
  if (!(bitmap = (my_bitmap_map *)return_mem_root->Alloc(
            table()->s->column_bitmap_size))) {
    column_bitmap.bitmap = nullptr;
  } else
    bitmap_init(&column_bitmap, bitmap, table()->s->fields);
  bitmap_copy(&column_bitmap, table()->read_set);

  for (uint i = 0; i < used_key_parts; i++, p++) {
    max_used_key_length += p->store_length;
    /*
      The last key part contains the subrange scan that we want to execute
      for every distinct prefix. There is only ever one keypart, so just
      exclude the last key from the distinct prefix.
    */
    if (i + 1 < used_key_parts) {
      distinct_prefix_len += p->store_length;
      bitmap_set_bit(&column_bitmap, p->field->field_index());
    }
  }
  distinct_prefix_key_parts = used_key_parts - 1;
}

IndexSkipScanIterator::~IndexSkipScanIterator() {
  DBUG_TRACE;
  if (table()->file->inited) table()->file->ha_index_or_rnd_end();
}

bool IndexSkipScanIterator::Init(void) {
  DBUG_TRACE;

  if (distinct_prefix == nullptr) {
    assert(distinct_prefix_key_parts > 0 && distinct_prefix_len > 0);
    if (!(distinct_prefix = mem_root->ArrayAlloc<uchar>(distinct_prefix_len))) {
      table()->file->print_error(HA_ERR_OUT_OF_MEM, MYF(0));
      return true;
    }

    if (eq_prefix_len > 0) {
      eq_prefix = mem_root->ArrayAlloc<uchar>(eq_prefix_len);
      if (!eq_prefix) {
        table()->file->print_error(HA_ERR_OUT_OF_MEM, MYF(0));
        return true;
      }
    } else {
      eq_prefix = nullptr;
    }
  }

  int result;
  seen_first_key = false;
  /* set keyread to true if all the attributes which are required by
     the query are present in the index */
  if (!table()->no_keyread && table()->covering_keys.is_set(index))
    table()->set_keyread(true);
  else
    table()->set_keyread(false);

  MY_BITMAP *const save_read_set = table()->read_set;

  table()->column_bitmaps_set_no_signal(&column_bitmap, table()->write_set);
  if ((result = table()->file->ha_index_init(index, true))) {
    table()->file->print_error(result, MYF(0));
    return true;
  }

  // Set the first equality prefix.
  size_t offset = 0;
  for (uint i = 0; i < eq_prefix_key_parts; i++) {
    const uchar *key = eq_prefixes[i].eq_key_prefixes[0];
    eq_prefixes[i].cur_eq_prefix = 0;
    uint part_length = (index_info->key_part + i)->store_length;
    memcpy(eq_prefix + offset, key, part_length);
    offset += part_length;
    assert(offset <= eq_prefix_len);
  }

  table()->column_bitmaps_set_no_signal(save_read_set, table()->write_set);
  return false;
}

/**
  Increments cur_prefix and sets what the next equality prefix should be.

  SYNOPSIS
    IndexSkipScanIterator::next_eq_prefix()

  DESCRIPTION
    Increments cur_prefix and sets what the next equality prefix should be.
    This is done in index order, so the increment happens on the last keypart.
    The key is written to eq_prefix.

  RETURN
    true   OK
    false  No more equality key prefixes.
*/

bool IndexSkipScanIterator::next_eq_prefix() {
  DBUG_TRACE;
  /*
    Counts at which position we're at in eq_prefix from the back of the
    string.
  */
  size_t reverse_offset = 0;

  // Increment the cur_prefix count.
  for (uint i = 0; i < eq_prefix_key_parts; i++) {
    uint part = eq_prefix_key_parts - i - 1;
    EQPrefix &eqp = eq_prefixes[part];
    assert(eqp.cur_eq_prefix < eqp.eq_key_prefixes.size());
    uint part_length = (index_info->key_part + part)->store_length;
    reverse_offset += part_length;

    ++eqp.cur_eq_prefix;
    const uchar *key =
        eqp.eq_key_prefixes[eqp.cur_eq_prefix % eqp.eq_key_prefixes.size()];
    memcpy(eq_prefix + eq_prefix_len - reverse_offset, key, part_length);
    if (eqp.cur_eq_prefix == eqp.eq_key_prefixes.size()) {
      eqp.cur_eq_prefix = 0;
      if (part == 0) {
        // This is the last key part.
        return false;
      }
    } else {
      break;
    }
  }

  return true;
}

/**
  Get the next row for skip scan.

  SYNOPSIS
    IndexSkipScanIterator::Read()

  DESCRIPTION
    Find the next record in the skip scan. The scan is broken into groups
    based on distinct A_1,...,B_m. The strategy is to have an outer loop
    going through all possible A_1,...,A_k. This work is done in
    next_eq_prefix().

    For each equality prefix that we get from "next_eq_prefix() we loop through
    all distinct B_1,...,B_m within that prefix. And for each of those groups
    we do a subrange scan on keypart C.

    The high level algorithm is like so:
    for (eq_prefix in eq_prefixes)       // (A_1,....A_k)
      for (distinct_prefix in eq_prefix) // A_1-B_1,...,A_k-B_m
        do subrange scan within distinct prefix
          using range_cond               // A_1-B_1-C,...A_k-B_m-C

    But since this is a iterator interface, state needs to be kept between
    calls. State is stored in eq_prefix, cur_eq_prefix and distinct_prefix.

  NOTES
    We can be more memory efficient by combining some of these fields. For
    example, eq_prefix will always be a prefix of distinct_prefix, and
    distinct_prefix will always be a prefix of min_search_key/max_search_key.

  RETURN
    See RowIterator::Read()
 */
int IndexSkipScanIterator::Read() {
  DBUG_TRACE;
  int result = HA_ERR_END_OF_FILE;
  int past_eq_prefix = 0;
  bool is_prefix_valid = seen_first_key;

  assert(distinct_prefix_len + range_key_len == max_used_key_length);

  MY_BITMAP *const save_read_set = table()->read_set;
  table()->column_bitmaps_set_no_signal(&column_bitmap, table()->write_set);
  do {
    if (!is_prefix_valid) {
      if (!seen_first_key) {
        if (eq_prefix_key_parts == 0) {
          result = table()->file->ha_index_first(table()->record[0]);
        } else {
          result = table()->file->ha_index_read_map(
              table()->record[0], eq_prefix,
              make_prev_keypart_map(eq_prefix_key_parts), HA_READ_KEY_OR_NEXT);
        }
        seen_first_key = true;
      } else {
        result = index_next_different(false /* is_index_scan */, table()->file,
                                      index_info->key_part, table()->record[0],
                                      distinct_prefix, distinct_prefix_len,
                                      distinct_prefix_key_parts);
      }

      if (result) return HandleError(result);

      // Save the prefix of this group for subsequent calls.
      key_copy(distinct_prefix, table()->record[0], index_info,
               distinct_prefix_len);

      if (eq_prefix) {
        past_eq_prefix =
            key_cmp(index_info->key_part, eq_prefix, eq_prefix_len);
        assert(past_eq_prefix >= 0);

        // We are past the equality prefix, so get the next prefix.
        if (past_eq_prefix > 0) {
          bool has_next = next_eq_prefix();
          if (has_next) {
            /*
              Reset seen_first_key so that we can determine the next distinct
              prefix.
            */
            seen_first_key = false;
            result = HA_ERR_END_OF_FILE;
            continue;
          }
          return -1;
        }
      }

      // We should not be doing a skip scan if there is no range predicate.
      assert(!(range_cond_flag & NO_MIN_RANGE) ||
             !(range_cond_flag & NO_MAX_RANGE));

      if (!(range_cond_flag & NO_MIN_RANGE)) {
        // If there is a minimum key, append to the distinct prefix.
        memcpy(min_search_key, distinct_prefix, distinct_prefix_len);
        memcpy(min_search_key + distinct_prefix_len, min_range_key,
               range_key_len);
        start_key.key = min_search_key;
        start_key.length = max_used_key_length;
        start_key.keypart_map = make_prev_keypart_map(used_key_parts);
        start_key.flag = (range_cond_flag & (EQ_RANGE | NULL_RANGE))
                             ? HA_READ_KEY_EXACT
                             : (range_cond_flag & NEAR_MIN)
                                   ? HA_READ_AFTER_KEY
                                   : HA_READ_KEY_OR_NEXT;
      } else {
        // If there is no minimum key, just use the distinct prefix.
        start_key.key = distinct_prefix;
        start_key.length = distinct_prefix_len;
        start_key.keypart_map = make_prev_keypart_map(used_key_parts - 1);
        start_key.flag = HA_READ_KEY_OR_NEXT;
      }

      /*
        It is not obvious what the semantics of HA_READ_BEFORE_KEY,
        HA_READ_KEY_EXACT and HA_READ_AFTER_KEY are for end_key.

        See handler::set_end_range for details on what they do.
      */
      if (!(range_cond_flag & NO_MAX_RANGE)) {
        // If there is a maximum key, append to the distinct prefix.
        memcpy(max_search_key, distinct_prefix, distinct_prefix_len);
        memcpy(max_search_key + distinct_prefix_len, max_range_key,
               range_key_len);
        end_key.key = max_search_key;
        end_key.length = max_used_key_length;
        end_key.keypart_map = make_prev_keypart_map(used_key_parts);
        //  See comment in quick_range_seq_next for why these flags are set.
        end_key.flag = (range_cond_flag & NEAR_MAX) ? HA_READ_BEFORE_KEY
                                                    : HA_READ_AFTER_KEY;
      } else {
        // If there is no maximum key, just use the distinct prefix.
        end_key.key = distinct_prefix;
        end_key.length = distinct_prefix_len;
        end_key.keypart_map = make_prev_keypart_map(used_key_parts - 1);
        end_key.flag = HA_READ_AFTER_KEY;
      }
      is_prefix_valid = true;

      result = table()->file->ha_read_range_first(
          &start_key, &end_key, range_cond_flag & EQ_RANGE, true /* sorted */);
      if (result) {
        int error_code = HandleError(result);
        if (error_code == -1) {
          is_prefix_valid = false;
          continue;
        }
        return error_code;
      }
    } else {
      result = table()->file->ha_read_range_next();
      if (result) {
        int error_code = HandleError(result);
        if (error_code == -1) {
          is_prefix_valid = false;
          continue;
        }
        return error_code;
      }
    }
  } while (!thd()->killed &&
           (result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE));

  table()->column_bitmaps_set_no_signal(save_read_set, table()->write_set);

  if (result == 0) {
    return 0;
  }
  return HandleError(result);
}
