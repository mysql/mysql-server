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

#include "sql/range_optimizer/skip_scan.h"

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

void QUICK_SKIP_SCAN_SELECT::add_info_string(String *str) {
  str->append(STRING_WITH_LEN("index_for_skip_scan("));
  str->append(index_info->name);
  str->append(')');
}

/**
  Construct new quick select for queries that can do skip scans.
  See get_best_skip_scan() description for more details.

  SYNOPSIS
    QUICK_SKIP_SCAN_SELECT::QUICK_SKIP_SCAN_SELECT()
    table              The table being accessed
    join               Descriptor of the current query
    index_info         The index chosen for data access
    use_index          The id of index_info
    range_part         The keypart belonging to the range condition C
    index_range_tree   The complete range key
    eq_prefix_len      Length of the equality prefix key
    eq_prefix_parts    Number of keyparts in the equality prefix
    used_key_parts_arg Total number of keyparts A_1,...,C
    read_cost_arg      Cost of this access method
    read_records       Number of records returned
    parent_alloc       Memory pool for this class

  RETURN
    None
*/

QUICK_SKIP_SCAN_SELECT::QUICK_SKIP_SCAN_SELECT(
    TABLE *table, JOIN *join, KEY *index_info, uint use_index,
    KEY_PART_INFO *range_part, SEL_ROOT *index_range_tree, uint eq_prefix_len,
    uint eq_prefix_parts, uint used_key_parts_arg,
    const Cost_estimate *read_cost_arg, ha_rows read_records,
    MEM_ROOT *parent_alloc, bool has_aggregate_function)
    : join(join),
      index_info(index_info),
      index_range_tree(index_range_tree),
      eq_prefix_len(eq_prefix_len),
      eq_prefix_key_parts(eq_prefix_parts),
      distinct_prefix(nullptr),
      range_key_part(range_part),
      seen_first_key(false),
      min_range_key(nullptr),
      max_range_key(nullptr),
      min_search_key(nullptr),
      max_search_key(nullptr),
      has_aggregate_function(has_aggregate_function) {
  head = table;
  index = use_index;
  record = head->record[0];
  cost_est = *read_cost_arg;
  records = read_records;

  used_key_parts = used_key_parts_arg;
  max_used_key_length = 0;
  distinct_prefix_len = 0;
  range_cond_flag = 0;
  KEY_PART_INFO *p = index_info->key_part;

  my_bitmap_map *bitmap;
  if (!(bitmap = (my_bitmap_map *)my_malloc(key_memory_my_bitmap_map,
                                            head->s->column_bitmap_size,
                                            MYF(MY_WME)))) {
    column_bitmap.bitmap = nullptr;
  } else
    bitmap_init(&column_bitmap, bitmap, head->s->fields);
  bitmap_copy(&column_bitmap, head->read_set);

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

  /*
    See QUICK_GROUP_MIN_MAX_SELECT::QUICK_GROUP_MIN_MAX_SELECT
    for why parent_alloc should be NULL.
  */
  assert(!parent_alloc);
  if (!parent_alloc) {
    init_sql_alloc(key_memory_quick_group_min_max_select_root, &alloc,
                   join->thd->variables.range_alloc_block_size, 0);
    join->thd->mem_root = &alloc;
  } else
    ::new (&alloc) MEM_ROOT;  // ensure that it's not used
}

/**
  Do post-constructor initialization.

  SYNOPSIS
    QUICK_SKIP_SCAN_SELECT::init()

  DESCRIPTION
    The method performs initialization that cannot be done in the constructor
    such as memory allocations that may fail. It allocates memory for the
    equality prefix and distinct prefix buffers. It also extracts all equality
    prefixes from index_range_tree, as well as allocates memory to store them.

  RETURN
    0      OK
    other  Error code
*/

int QUICK_SKIP_SCAN_SELECT::init() {
  if (distinct_prefix) return 0;

  assert(distinct_prefix_key_parts > 0 && distinct_prefix_len > 0);
  if (!(distinct_prefix = (uchar *)alloc.Alloc(distinct_prefix_len))) return 1;

  if (eq_prefix_len > 0) {
    eq_prefix = (uchar *)alloc.Alloc(eq_prefix_len);
    if (!eq_prefix) return 1;
  } else {
    eq_prefix = nullptr;
  }

  if (eq_prefix_key_parts > 0) {
    if (!(cur_eq_prefix =
              (uint *)alloc.Alloc(eq_prefix_key_parts * sizeof(uint))))
      return 1;
    if (!(eq_key_prefixes =
              (uchar ***)alloc.Alloc(eq_prefix_key_parts * sizeof(uchar **))))
      return 1;
    if (!(eq_prefix_elements =
              (uint *)alloc.Alloc(eq_prefix_key_parts * sizeof(uint))))
      return 1;

    const SEL_ARG *cur_range = index_range_tree->root->first();
    const SEL_ARG *first_range = nullptr;
    const SEL_ROOT *cur_root = index_range_tree;
    for (uint i = 0; i < eq_prefix_key_parts;
         i++, cur_range = cur_range->next_key_part->root) {
      cur_eq_prefix[i] = 0;
      eq_prefix_elements[i] = cur_root->elements;
      cur_root = cur_range->next_key_part;
      assert(eq_prefix_elements[i] > 0);
      if (!(eq_key_prefixes[i] =
                (uchar **)alloc.Alloc(eq_prefix_elements[i] * sizeof(uchar *))))
        return 1;

      uint j = 0;
      first_range = cur_range->first();
      for (cur_range = first_range; cur_range;
           j++, cur_range = cur_range->next) {
        KEY_PART_INFO *keypart = index_info->key_part + i;
        size_t field_length = keypart->store_length;
        //  Store ranges in the reverse order if key part is descending.
        uint pos = cur_range->is_ascending ? j : eq_prefix_elements[i] - j - 1;

        if (!(eq_key_prefixes[i][pos] = (uchar *)alloc.Alloc(field_length)))
          return 1;

        if (cur_range->maybe_null() && cur_range->min_value[0] &&
            cur_range->max_value[0]) {
          assert(field_length > 0);
          eq_key_prefixes[i][pos][0] = 0x1;
        } else {
          assert(memcmp(cur_range->min_value, cur_range->max_value,
                        field_length) == 0);
          memcpy(eq_key_prefixes[i][pos], cur_range->min_value, field_length);
        }
      }
      cur_range = first_range;
      assert(j == eq_prefix_elements[i]);
    }
  }

  return 0;
}

QUICK_SKIP_SCAN_SELECT::~QUICK_SKIP_SCAN_SELECT() {
  DBUG_TRACE;
  if (head->file->inited) head->file->ha_index_or_rnd_end();

  my_free(column_bitmap.bitmap);
  free_root(&alloc, MYF(0));
}

/**
  Setup fileds that hold the range condition on key part C.

  SYNOPSIS
    QUICK_SKIP_SCAN_SELECT::set_range()
    sel_range  Range object from which the fields will be populated with.

  NOTES
    This is only the suffix of the whole key, that we use
    to append to the prefix to get the full key later on.
  RETURN
    true on success
    false otherwise
*/

bool QUICK_SKIP_SCAN_SELECT::set_range(SEL_ARG *sel_range) {
  DBUG_TRACE;
  uchar *min_value, *max_value;

  if (!(sel_range->min_flag & NO_MIN_RANGE) &&
      !(sel_range->max_flag & NO_MAX_RANGE)) {
    // IS NULL condition
    if (sel_range->maybe_null() && sel_range->min_value[0] &&
        sel_range->max_value[0])
      range_cond_flag |= NULL_RANGE;
    // equality condition
    else if (memcmp(sel_range->min_value, sel_range->max_value,
                    range_key_part->store_length) == 0)
      range_cond_flag |= EQ_RANGE;
  }

  if (sel_range->is_ascending) {
    range_cond_flag |= sel_range->min_flag | sel_range->max_flag;
    min_value = sel_range->min_value;
    max_value = sel_range->max_value;
  } else {
    range_cond_flag |= invert_min_flag(sel_range->min_flag) |
                       invert_max_flag(sel_range->max_flag);
    min_value = sel_range->max_value;
    max_value = sel_range->min_value;
  }

  range_key_len = range_key_part->store_length;

  // Allocate storage for min/max key if they exist.
  if (!(range_cond_flag & NO_MIN_RANGE)) {
    if (!(min_range_key = (uchar *)alloc.Alloc(range_key_len))) return false;
    if (!(min_search_key = (uchar *)alloc.Alloc(max_used_key_length)))
      return false;

    memcpy(min_range_key, min_value, range_key_len);
  }
  if (!(range_cond_flag & NO_MAX_RANGE)) {
    if (!(max_range_key = (uchar *)alloc.Alloc(range_key_len))) return false;
    if (!(max_search_key = (uchar *)alloc.Alloc(max_used_key_length)))
      return false;

    memcpy(max_range_key, max_value, range_key_len);
  }

  return true;
}

/**
  Initialize a quick skip scan index select for key retrieval.

  SYNOPSIS
    QUICK_SKIP_SCAN_SELECT::reset()

  DESCRIPTION
    Initialize the index chosen for access and initialize what the first
    equality key prefix should be.

  RETURN
    0      OK
    other  Error code
*/

int QUICK_SKIP_SCAN_SELECT::reset(void) {
  DBUG_TRACE;

  int result;
  seen_first_key = false;
  head->set_keyread(true);  // This access path demands index-only reads.
  MY_BITMAP *const save_read_set = head->read_set;

  head->column_bitmaps_set_no_signal(&column_bitmap, head->write_set);
  if ((result = head->file->ha_index_init(index, true))) {
    head->file->print_error(result, MYF(0));
    return result;
  }

  // Set the first equality prefix.
  size_t offset = 0;
  for (uint i = 0; i < eq_prefix_key_parts; i++) {
    const uchar *key = eq_key_prefixes[i][0];
    cur_eq_prefix[i] = 0;
    uint part_length = (index_info->key_part + i)->store_length;
    memcpy(eq_prefix + offset, key, part_length);
    offset += part_length;
    assert(offset <= eq_prefix_len);
  }

  head->column_bitmaps_set_no_signal(save_read_set, head->write_set);
  return 0;
}

/**
  Increments cur_prefix and sets what the next equality prefix should be.

  SYNOPSIS
    QUICK_SKIP_SCAN_SELECT::next_eq_prefix()

  DESCRIPTION
    Increments cur_prefix and sets what the next equality prefix should be.
    This is done in index order, so the increment happens on the last keypart.
    The key is written to eq_prefix.

  RETURN
    true   OK
    false  No more equality key prefixes.
*/

bool QUICK_SKIP_SCAN_SELECT::next_eq_prefix() {
  DBUG_TRACE;
  /*
    Counts at which position we're at in eq_prefix from the back of the
    string.
  */
  size_t reverse_offset = 0;

  // Increment the cur_prefix count.
  for (uint i = 0; i < eq_prefix_key_parts; i++) {
    uint part = eq_prefix_key_parts - i - 1;
    assert(cur_eq_prefix[part] < eq_prefix_elements[part]);
    uint part_length = (index_info->key_part + part)->store_length;
    reverse_offset += part_length;

    cur_eq_prefix[part]++;
    const uchar *key =
        eq_key_prefixes[part][cur_eq_prefix[part] % eq_prefix_elements[part]];
    memcpy(eq_prefix + eq_prefix_len - reverse_offset, key, part_length);
    if (cur_eq_prefix[part] == eq_prefix_elements[part]) {
      cur_eq_prefix[part] = 0;
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
    QUICK_SKIP_SCAN_SELECT::get_next()

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
    0                  on success
    HA_ERR_END_OF_FILE if returned all keys
    other              if some error occurred
*/

int QUICK_SKIP_SCAN_SELECT::get_next() {
  DBUG_TRACE;
  int result = HA_ERR_END_OF_FILE;
  int past_eq_prefix = 0;
  bool is_prefix_valid = seen_first_key;

  assert(distinct_prefix_len + range_key_len == max_used_key_length);

  MY_BITMAP *const save_read_set = head->read_set;
  head->column_bitmaps_set_no_signal(&column_bitmap, head->write_set);
  do {
    if (!is_prefix_valid) {
      if (!seen_first_key) {
        if (eq_prefix_key_parts == 0) {
          result = head->file->ha_index_first(record);
        } else {
          result = head->file->ha_index_read_map(
              record, eq_prefix, make_prev_keypart_map(eq_prefix_key_parts),
              HA_READ_KEY_OR_NEXT);
        }
        seen_first_key = true;
      } else {
        result = index_next_different(
            false /* is_index_scan */, head->file, index_info->key_part, record,
            distinct_prefix, distinct_prefix_len, distinct_prefix_key_parts);
      }

      if (result) goto exit;

      // Save the prefix of this group for subsequent calls.
      key_copy(distinct_prefix, record, index_info, distinct_prefix_len);

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
          result = HA_ERR_END_OF_FILE;
          goto exit;
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

      result = head->file->ha_read_range_first(
          &start_key, &end_key, range_cond_flag & EQ_RANGE, true /* sorted */);
      if (result) {
        if (result == HA_ERR_END_OF_FILE) {
          is_prefix_valid = false;
          continue;
        }
        goto exit;
      }
    } else {
      result = head->file->ha_read_range_next();
      if (result) {
        if (result == HA_ERR_END_OF_FILE) {
          is_prefix_valid = false;
          continue;
        }
        goto exit;
      }
    }
  } while ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE));

exit:
  head->column_bitmaps_set_no_signal(save_read_set, head->write_set);

  if (result == HA_ERR_KEY_NOT_FOUND) result = HA_ERR_END_OF_FILE;

  return result;
}

/**
  Append comma-separated list of keys this quick select uses to key_names;
  append comma-separated list of corresponding used lengths to used_lengths.

  SYNOPSIS
    QUICK_SKIP_SCAN_SELECT::add_keys_and_lengths()
    key_names    [out] Names of used indexes
    used_lengths [out] Corresponding lengths of the index names

  DESCRIPTION
    This method is used by select_describe to extract the names of the
    indexes used by a quick select.

*/

void QUICK_SKIP_SCAN_SELECT::add_keys_and_lengths(String *key_names,
                                                  String *used_lengths) {
  char buf[64];
  uint length;
  key_names->append(index_info->name);
  length = longlong10_to_str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}

#ifndef NDEBUG
void QUICK_SKIP_SCAN_SELECT::dbug_dump(int indent, bool verbose) {
  fprintf(DBUG_FILE,
          "%*squick_skip_scan_query_block: index %s (%d), length: %d\n", indent,
          "", index_info->name, index, max_used_key_length);
  if (eq_prefix_len > 0) {
    fprintf(DBUG_FILE, "%*susing eq_prefix with length %d:\n", indent, "",
            eq_prefix_len);
  }

  if (verbose) {
    char buff1[512];
    buff1[0] = '\0';
    String range_result(buff1, sizeof(buff1), system_charset_info);

    if (index_range_tree && eq_prefix_key_parts > 0) {
      range_result.length(0);
      char buff2[128];
      String range_so_far(buff2, sizeof(buff2), system_charset_info);
      range_so_far.length(0);
      append_range_all_keyparts(nullptr, &range_result, &range_so_far,
                                index_range_tree, index_info->key_part, false);
      fprintf(DBUG_FILE, "Prefix ranges: %s\n", range_result.c_ptr());
    }

    {
      range_result.length(0);
      append_range(&range_result, range_key_part, min_range_key, max_range_key,
                   range_cond_flag);
      fprintf(DBUG_FILE, "Range: %s\n", range_result.c_ptr());
    }
  }
}
#endif
