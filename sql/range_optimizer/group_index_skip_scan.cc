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

#include "sql/range_optimizer/group_index_skip_scan.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <new>

#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "prealloced_array.h"
#include "sql/handler.h"
#include "sql/item_sum.h"
#include "sql/psi_memory_key.h"
#include "sql/range_optimizer/index_range_scan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_class.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"

/*
  Construct new quick select for group queries with min/max.

  SYNOPSIS
    GroupIndexSkipScanIterator::GroupIndexSkipScanIterator()
    thd                Thead handle
    table              The table being accessed
    min_functions      MIN() functions in query block
    max_functions      MAX() functions in query block
    have_agg_distinct  Whether any aggregates are DISTINCT
    min_max_arg_part   The only argument field of all MIN/MAX functions
    group_prefix_len   Length of all key parts in the group prefix
    group_key_parts
    real_key_parts_arg Same, but excluding any min_max key part
    max_used_key_length_arg
                       Length of longest key
    index_info         The index chosen for data access
    use_index          The id of index_info
    key_infix_len
    read_cost          Cost of this access method
    records            Number of records returned
    key_infix_len      Length of the key infix appended to the group prefix
    return_mem_root    Memory pool to allocate temporary buffers on
    is_index_scan      get the next different key not by jumping on it via
                       index read, but by scanning until the end of the
                       rows with equal key value.
    quick_prefix_query_block_arg
                       Child scan to get prefix keys
    prefix_ranges      Ranges to scan for the prefix key part(s)
    key_infix_ranges   Ranges to scan for the infix key part(s)
    min_max_ranges     Ranges to scan for the MIN/MAX key part
 */
GroupIndexSkipScanIterator::GroupIndexSkipScanIterator(
    THD *thd, TABLE *table, const Mem_root_array<Item_sum *> *min_functions,
    const Mem_root_array<Item_sum *> *max_functions, bool have_agg_distinct,
    KEY_PART_INFO *min_max_arg_part, uint group_prefix_len,
    uint group_key_parts, uint real_key_parts_arg, uint max_used_key_length_arg,
    KEY *index_info, uint use_index, uint key_infix_len,
    MEM_ROOT *return_mem_root, bool is_index_scan,
    const Quick_ranges *prefix_ranges,
    const Quick_ranges_array *key_infix_ranges,
    const Quick_ranges *min_max_ranges)
    : TableRowIterator(thd, table),
      index(use_index),
      index_info(index_info),
      group_prefix(nullptr),
      group_prefix_len(group_prefix_len),
      group_key_parts(group_key_parts),
      have_agg_distinct(have_agg_distinct),
      seen_first_key(false),
      min_max_arg_part(min_max_arg_part),
      key_infix_len(key_infix_len),
      max_used_key_length(max_used_key_length_arg),
      prefix_ranges(prefix_ranges),
      min_max_ranges(min_max_ranges),
      key_infix_ranges(key_infix_ranges),
      real_key_parts(real_key_parts_arg),
      min_functions(min_functions),
      max_functions(max_functions),
      is_index_scan(is_index_scan),
      mem_root(return_mem_root) {
  real_prefix_len = group_prefix_len + key_infix_len;
  min_max_arg_len = min_max_arg_part ? min_max_arg_part->store_length : 0;
  min_max_keypart_asc =
      min_max_arg_part ? !(min_max_arg_part->key_part_flag & HA_REVERSE_SORT)
                       : false;
  memset(cur_infix_range_position, 0, sizeof(cur_infix_range_position));
}

GroupIndexSkipScanIterator::~GroupIndexSkipScanIterator() {
  DBUG_TRACE;
  if (table()->file->inited)
    /*
      We may have used this object for index access during
      create_sort_index() and then switched to rnd access for the rest
      of execution. Since we don't do cleanup until now, we must call
      ha_*_end() for whatever is the current access method.
    */
    table()->file->ha_index_or_rnd_end();
}

/*
  Initialize a quick group min/max select for key retrieval.

  SYNOPSIS
    GroupIndexSkipScanIterator::reset()

  DESCRIPTION
    Initialize the index chosen for access and find and store the prefix
    of the last group. The method is expensive since it performs disk access.

  RETURN
    true if error
*/

bool GroupIndexSkipScanIterator::Init() {
  empty_record(table());
  m_seen_eof = false;

  if (group_prefix == nullptr) {
    // First-time initialization.
    if (!(last_prefix = mem_root->ArrayAlloc<uchar>(group_prefix_len))) {
      table()->file->print_error(HA_ERR_OUT_OF_MEM, MYF(0));
      return true;
    }
    /*
      We may use group_prefix to store keys with all select fields, so allocate
      enough space for it.
    */
    if (!(group_prefix =
              mem_root->ArrayAlloc<uchar>(real_prefix_len + min_max_arg_len))) {
      table()->file->print_error(HA_ERR_OUT_OF_MEM, MYF(0));
      return true;
    }
  }

  seen_first_key = false;
  table()->set_keyread(true); /* We need only the key attributes */
  /*
    Request ordered index access as usage of ::index_last(),
    ::index_first() within GroupIndexSkipScanIterator depends on it.
  */
  if (table()->file->inited) table()->file->ha_index_or_rnd_end();
  if (int result = table()->file->ha_index_init(index, true); result != 0) {
    table()->file->print_error(result, MYF(0));
    return true;
  }

  cur_prefix_range_idx = 0;
  last_prefix_range = nullptr;

  if (int result = table()->file->ha_index_last(table()->record[0]);
      result != 0) {
    if (result == HA_ERR_END_OF_FILE) {
      m_seen_eof = true;
      return false;
    } else {
      table()->file->print_error(result, MYF(0));
      return true;
    }
  }

  /* Save the prefix of the last group. */
  key_copy(last_prefix, table()->record[0], index_info, group_prefix_len);

  return false;
}

/*
  Get the next key containing the MIN and/or MAX key for the next group.

  SYNOPSIS
    GroupIndexSkipScanIterator::Read()

  DESCRIPTION
    The method finds the next subsequent group of records that satisfies the
    query conditions and finds the keys that contain the MIN/MAX values for
    the key part referenced by the MIN/MAX function(s). Once a group and its
    MIN/MAX values are found, store these values in the Item_sum objects for
    the MIN/MAX functions. The rest of the values in the result row are stored
    in the Item_field::result_field of each select field. If the query does
    not contain MIN and/or MAX functions, then the function only finds the
    group prefix, which is a query answer itself.

  NOTES
    If both MIN and MAX are computed, then we use the fact that if there is
    no MIN key, there can't be a MAX key as well, so we can skip looking
    for a MAX key in this case.

  RETURN
    See RowIterator::Read()
 */
int GroupIndexSkipScanIterator::Read() {
  if (m_seen_eof) {
    return -1;
  }

  int result;
  int is_last_prefix = 0;

  DBUG_TRACE;

  /*
    Loop until a group is found that satisfies all query conditions or the last
    group is reached.
  */
  do {
    result = next_prefix();
    /*
      Check if this is the last group prefix. Notice that at this point
      this->record contains the current prefix in record format.
    */
    if (!result) {
      is_last_prefix =
          key_cmp(index_info->key_part, last_prefix, group_prefix_len);
      assert(is_last_prefix <= 0);
    } else {
      if (result == HA_ERR_KEY_NOT_FOUND) continue;
      break;
    }

    // Reset current infix range and min/max function as a new group is
    // starting.
    reset_group();
    // TRUE if at least one group satisfied the prefix and infix condition is
    // found.
    bool found_result = false;
    // Reset MIN/MAX value only for the first infix range.
    bool reset_min_value = true;
    bool reset_max_value = true;
    while (!thd()->killed && !append_next_infix()) {
      assert(!result || !is_index_access_error(result));
      if (!min_functions->empty() || !max_functions->empty()) {
        if (min_max_keypart_asc) {
          if (!min_functions->empty()) {
            if (!(result = next_min()))
              update_min_result(&reset_min_value);
            else {
              DBUG_EXECUTE_IF("bug30769515_QUERY_INTERRUPTED",
                              result = HA_ERR_QUERY_INTERRUPTED;);
              if (is_index_access_error(result)) {
                return HandleError(result);
              }
              continue;  // Record is not found, no reason to call next_max()
            }
          }
          if (!max_functions->empty()) {
            if (!(result = next_max()))
              update_max_result(&reset_max_value);
            else if (is_index_access_error(result))
              return HandleError(result);
          }
        } else {
          // Call next_max() first and then next_min() if
          // MIN/MAX key part is descending.
          if (!max_functions->empty()) {
            if (!(result = next_max()))
              update_max_result(&reset_max_value);
            else {
              DBUG_EXECUTE_IF("bug30769515_QUERY_INTERRUPTED",
                              result = HA_ERR_QUERY_INTERRUPTED;);
              if (is_index_access_error(result)) return HandleError(result);
              continue;  // Record is not found, no reason to call next_min()
            }
          }
          if (!min_functions->empty()) {
            if (!(result = next_min()))
              update_min_result(&reset_min_value);
            else if (is_index_access_error(result))
              return HandleError(result);
          }
        }
        if (!result) found_result = true;
      } else if (key_infix_len > 0) {
        /*
          If this is just a GROUP BY or DISTINCT without MIN or MAX and there
          are equality predicates for the key parts after the group, find the
          first sub-group with the extended prefix. There is no need to iterate
          through the whole group to accumulate the MIN/MAX and returning just
          the one distinct record is enough.
        */
        result = table()->file->ha_index_read_map(
            table()->record[0], group_prefix,
            make_prev_keypart_map(real_key_parts), HA_READ_KEY_EXACT);
        if (result == 0) {
          return 0;
        }
        if (is_index_access_error(result)) {
          return HandleError(result);
        }
      }
    }
    if (seen_all_infix_ranges && found_result) return 0;
  } while (!thd()->killed && !is_index_access_error(result) &&
           is_last_prefix != 0);

  if (result == 0) {
    return 0;
  }

  int error_code = HandleError(result);
  if (error_code == -1) {
    m_seen_eof = true;
  }
  return error_code;
}

/*
  Retrieve the minimal key in the next group.

  SYNOPSIS
    GroupIndexSkipScanIterator::next_min()

  DESCRIPTION
    Find the minimal key within this group such that the key satisfies the query
    conditions and NULL semantics. The found key is loaded into this->record.

  IMPLEMENTATION
    Depending on the values of min_max_ranges.elements, key_infix_len, and
    whether there is a  NULL in the MIN field, this function may directly
    return without any data access. In this case we use the key loaded into
    this->record by the call to this->next_prefix() just before this call.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if no MIN key was found that fulfills all conditions.
    HA_ERR_END_OF_FILE   - "" -
    other                if some error occurred
*/

int GroupIndexSkipScanIterator::next_min() {
  int result = 0;
  DBUG_TRACE;

  /* Find the MIN key using the eventually extended group prefix. */
  if (!min_max_ranges->empty()) {
    uchar key_buf[MAX_KEY_LENGTH];
    key_copy(key_buf, table()->record[0], index_info, max_used_key_length);
    result = next_min_in_range();
    if (result)
      key_restore(table()->record[0], key_buf, index_info, max_used_key_length);
  } else {
    /*
      Apply the constant equality conditions to the non-group select fields.
      There is no reason to call handler method if MIN/MAX key part is
      ascending since  MIN/MAX field points to min value after
      next_prefix() call.
    */
    if (key_infix_len > 0 || !min_max_keypart_asc) {
      if ((result = table()->file->ha_index_read_map(
               table()->record[0], group_prefix,
               make_prev_keypart_map(real_key_parts),
               min_max_keypart_asc ? HA_READ_KEY_EXACT : HA_READ_PREFIX_LAST)))
        return result;
    }

    /*
      If the min/max argument field is NULL, skip subsequent rows in the same
      group with NULL in it. Notice that:
      - if the first row in a group doesn't have a NULL in the field, no row
      in the same group has (because NULL < any other value),
      - min_max_arg_part->field->ptr points to some place in
      'table()->record[0]'.
    */
    if (min_max_arg_part && min_max_arg_part->field->is_null()) {
      uchar key_buf[MAX_KEY_LENGTH];

      /* Find the first subsequent record without NULL in the MIN/MAX field. */
      key_copy(key_buf, table()->record[0], index_info, max_used_key_length);
      result = table()->file->ha_index_read_map(
          table()->record[0], key_buf, make_keypart_map(real_key_parts),
          min_max_keypart_asc ? HA_READ_AFTER_KEY : HA_READ_BEFORE_KEY);
      /*
        Check if the new record belongs to the current group by comparing its
        prefix with the group's prefix. If it is from the next group, then the
        whole group has NULLs in the MIN/MAX field, so use the first record in
        the group as a result.
        TODO:
        It is possible to reuse this new record as the result candidate for
        the next call to next_min(), and to save one lookup in the next call.
        For this add a new member 'this->next_group_prefix'.
      */
      if (!result) {
        if (key_cmp(index_info->key_part, group_prefix, real_prefix_len))
          key_restore(table()->record[0], key_buf, index_info, 0);
      } else if (result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE)
        result = 0; /* There is a result in any case. */
    }
  }

  /*
    If the MIN attribute is non-nullable, this->record already contains the
    MIN key in the group, so just return.
  */
  return result;
}

/*
  Retrieve the maximal key in the next group.

  SYNOPSIS
    GroupIndexSkipScanIterator::next_max()

  DESCRIPTION
    Lookup the maximal key of the group, and store it into this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if no MAX key was found that fulfills all conditions.
    HA_ERR_END_OF_FILE	 - "" -
    other                if some error occurred
*/

int GroupIndexSkipScanIterator::next_max() {
  int result = 0;

  DBUG_TRACE;

  /* Get the last key in the (possibly extended) group. */
  if (!min_max_ranges->empty()) {
    uchar key_buf[MAX_KEY_LENGTH];
    key_copy(key_buf, table()->record[0], index_info, max_used_key_length);
    result = next_max_in_range();
    if (result)
      key_restore(table()->record[0], key_buf, index_info, max_used_key_length);
  } else {
    /*
      There is no reason to call handler method if MIN/MAX key part is
      descending since  MIN/MAX field points to max value after
      next_prefix() call.
    */
    if (key_infix_len > 0 || min_max_keypart_asc)
      result = table()->file->ha_index_read_map(
          table()->record[0], group_prefix,
          make_prev_keypart_map(real_key_parts),
          min_max_keypart_asc ? HA_READ_PREFIX_LAST : HA_READ_KEY_EXACT);
  }
  return result;
}

/*
  Determine the prefix of the next group.

  SYNOPSIS
    GroupIndexSkipScanIterator::next_prefix()

  DESCRIPTION
    Determine the prefix of the next group that satisfies the query conditions.
    If there is a range condition referencing the group attributes, use a
    IndexRangeScanIterator object to retrieve the *first* key that satisfies the
    condition. The prefix is stored in this->group_prefix. The first key of
    the found group is stored in this->record, on which relies this->next_min().

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the formed prefix
    HA_ERR_END_OF_FILE   if there are no more keys
    other                if some error occurred
*/
int GroupIndexSkipScanIterator::next_prefix() {
  DBUG_TRACE;

  if (!prefix_ranges->empty()) {
    uchar *cur_prefix = seen_first_key ? group_prefix : nullptr;
    if (int result =
            get_next_prefix(group_prefix_len, group_key_parts, cur_prefix)) {
      return result;
    }
    seen_first_key = true;
  } else {
    if (!seen_first_key) {
      int result = table()->file->ha_index_first(table()->record[0]);
      if (result) return result;
      seen_first_key = true;
    } else {
      /* Load the first key in this group into record. */
      int result = index_next_different(
          is_index_scan, table()->file, index_info->key_part,
          table()->record[0], group_prefix, group_prefix_len, group_key_parts);
      if (result) return result;
    }
  }

  /* Save the prefix of this group for subsequent calls. */
  key_copy(group_prefix, table()->record[0], index_info, group_prefix_len);
  return 0;
}

/*
  Get the next record with a different prefix.

  @param prefix_length   length of cur_prefix
  @param group_key_parts The number of key parts in the group prefix
  @param cur_prefix      prefix of a key to be searched for

  Each subsequent call to the method retrieves the first record that has a
  prefix with length prefix_length and which is different from cur_prefix,
  such that the record with the new prefix is within the ranges described by
  this->ranges. The record found is stored into the buffer pointed by
  this->record. The method is useful for GROUP-BY queries with range
  conditions to discover the prefix of the next group that satisfies the range
  conditions.

  @retval 0                  on success
  @retval HA_ERR_END_OF_FILE if returned all keys
  @retval other              if some error occurred
*/

int GroupIndexSkipScanIterator::get_next_prefix(uint prefix_length,
                                                uint group_key_parts,
                                                uchar *cur_prefix) {
  DBUG_TRACE;
  const key_part_map keypart_map = make_prev_keypart_map(group_key_parts);

  for (;;) {
    if (last_prefix_range != nullptr) {
      /* Read the next record in the same range with prefix after cur_prefix. */
      assert(cur_prefix != nullptr);
      table()->file->set_end_range(nullptr, handler::RANGE_SCAN_ASC);
      int result = table()->file->ha_index_read_map(
          table()->record[0], cur_prefix, keypart_map, HA_READ_AFTER_KEY);
      if (result || last_prefix_range->max_keypart_map == 0) return result;

      key_range previous_endpoint;
      last_prefix_range->make_max_endpoint(&previous_endpoint, prefix_length,
                                           keypart_map);
      if (table()->file->compare_key(&previous_endpoint) <= 0) return 0;
    }

    if (cur_prefix_range_idx == prefix_ranges->size()) {
      /* Ranges have already been used up before. None is left for read. */
      last_prefix_range = nullptr;
      return HA_ERR_END_OF_FILE;
    }
    last_prefix_range = (*prefix_ranges)[cur_prefix_range_idx++];

    key_range start_key, end_key;
    last_prefix_range->make_min_endpoint(&start_key, prefix_length,
                                         keypart_map);
    last_prefix_range->make_max_endpoint(&end_key, prefix_length, keypart_map);

    int result = table()->file->ha_read_range_first(
        last_prefix_range->min_keypart_map ? &start_key : nullptr,
        last_prefix_range->max_keypart_map ? &end_key : nullptr,
        last_prefix_range->flag & EQ_RANGE, /*sorted=*/true);
    if ((last_prefix_range->flag & (UNIQUE_RANGE | EQ_RANGE)) ==
        (UNIQUE_RANGE | EQ_RANGE))
      last_prefix_range = nullptr;  // Stop searching

    if (result != HA_ERR_END_OF_FILE) return result;
    last_prefix_range = nullptr;  // No matching rows; go to next range
  }
}

/*
  Determine and append the next infix.

  SYNOPSIS
    GroupIndexSkipScanIterator::append_next_infix()

  DESCRIPTION
    Appends the next infix onto this->group_prefix based on the current
    position stored in cur_infix_range_position

  RETURN
    true                 No next infix exists
    false                on success
*/

bool GroupIndexSkipScanIterator::append_next_infix() {
  if (seen_all_infix_ranges) return true;

  if (key_infix_len > 0) {
    uchar *key_ptr = group_prefix + group_prefix_len;
    // For each infix keypart, get the participating range for
    // the next row retrieval. cur_key_infix_position determines
    // which range needs to be used.
    for (uint i = 0; i < key_infix_ranges->size(); i++) {
      QUICK_RANGE *cur_range = nullptr;
      assert(!(*key_infix_ranges)[i]->empty());

      Quick_ranges *infix_range_array = (*key_infix_ranges)[i];
      cur_range = infix_range_array->at(cur_infix_range_position[i]);
      memcpy(key_ptr, cur_range->min_key, cur_range->min_length);
      key_ptr += cur_range->min_length;
    }

    // cur_infix_range_position is updated with the next infix range
    // position.
    for (int i = key_infix_ranges->size() - 1; i >= 0; i--) {
      cur_infix_range_position[i]++;
      if (cur_infix_range_position[i] == (*key_infix_ranges)[i]->size()) {
        // All the ranges for infix keypart "i" is done
        cur_infix_range_position[i] = 0;
        if (i == 0) seen_all_infix_ranges = true;
      } else
        break;
    }
  } else
    seen_all_infix_ranges = true;

  return false;
}

/*
  Reset all the variables that need to be updated for the new group.

  SYNOPSIS
    GroupIndexSkipScanIterator::reset_group()

  DESCRIPTION
    It is called before a new group is processed.

  RETURN
    None
*/

void GroupIndexSkipScanIterator::reset_group() {
  // Reset the current infix range before a new group is processed.
  seen_all_infix_ranges = false;
  memset(cur_infix_range_position, 0, sizeof(cur_infix_range_position));

  for (Item_sum *min_func : *min_functions) {
    min_func->aggregator_clear();
  }
  for (Item_sum *max_func : *max_functions) {
    max_func->aggregator_clear();
  }
}

/**
  Function returns search mode that needs to be used to read
  the next record. It takes the type of the range, the
  key part's order (ascending or descending) and if the
  range is on MIN function or a MAX function to get the
  right search mode.
  For "MIN" function:
   - ASC keypart
   We need to
    1. Read the first key that matches the range
       a) if a minimum value is not specified in the condition
       b) if it is a equality or is NULL condition
    2. Read the first key after a range value if range is like "a > 10"
    3. Read the key that matches the condition or any key after
       the range value for any other condition
   - DESC keypart
   We need to
    4. Read the last value for the key prefix if there is no minimum
       range specified.
    5. Read the first key that matches the range if it is a equality
       condition.
    6. Read the first key before a range value if range is like "a > 10"
    7. Read the key that matches the prefix or any key before for any
       other condition
  For MAX function:
   - ASC keypart
   We need to
    8. Read the last value for the key prefix if there is no maximum
       range specified
    9. Read the first key that matches the range if it is a equality
       condition
   10. Read the first key before a range value if range is like "a < 10"
   11. Read the key that matches the condition or any key before
       the range value for any other condition
   - DESC keypart
   We need to
   12. Read the first key that matches the range
       a) if a minimum value is not specified in the condition
       b) if it is a equality
   13. Read the first key after a range value if range is like "a < 10"
   14. Read the key that matches the prefix or any key after for
       any other condition


  @param cur_range         pointer to QUICK_RANGE.
  @param is_asc            TRUE if key part is ascending,
                           FALSE otherwise.
  @param is_min            TRUE if the range is on MIN function.
                           FALSE for MAX function.
  @return search mode
*/

static ha_rkey_function get_search_mode(QUICK_RANGE *cur_range, bool is_asc,
                                        bool is_min) {
  // If MIN function
  if (is_min) {
    if (is_asc) {                          // key part is ascending
      if (cur_range->flag & NO_MIN_RANGE)  // 1a
        return HA_READ_KEY_EXACT;
      else
        return (cur_range->flag & (EQ_RANGE | NULL_RANGE))  // 1b
                   ? HA_READ_KEY_EXACT
                   : (cur_range->flag & NEAR_MIN) ? HA_READ_AFTER_KEY     // 2
                                                  : HA_READ_KEY_OR_NEXT;  // 3
    } else {  // key parts is descending
      if (cur_range->flag & NO_MIN_RANGE)
        return HA_READ_PREFIX_LAST;  // 4
      else
        return (cur_range->flag & EQ_RANGE) ? HA_READ_KEY_EXACT  // 5
               : (cur_range->flag & NEAR_MIN)
                   ? HA_READ_BEFORE_KEY            // 6
                   : HA_READ_PREFIX_LAST_OR_PREV;  // 7
    }
  }

  // If max function
  if (is_asc) {  // key parts is ascending
    if (cur_range->flag & NO_MAX_RANGE)
      return HA_READ_PREFIX_LAST;  // 8
    else
      return (cur_range->flag & EQ_RANGE) ? HA_READ_KEY_EXACT  // 9
             : (cur_range->flag & NEAR_MAX)
                 ? HA_READ_BEFORE_KEY            // 10
                 : HA_READ_PREFIX_LAST_OR_PREV;  // 11
  } else {                                       // key parts is descending
    if (cur_range->flag & NO_MAX_RANGE)
      return HA_READ_KEY_EXACT;  // 12a
    else
      return (cur_range->flag & EQ_RANGE)   ? HA_READ_KEY_EXACT     // 12b
             : (cur_range->flag & NEAR_MAX) ? HA_READ_AFTER_KEY     // 13
                                            : HA_READ_KEY_OR_NEXT;  // 14
  }
}

/*
  Find the minimal key in a group that satisfies some range conditions for the
  min/max argument field.

  SYNOPSIS
    GroupIndexSkipScanIterator::next_min_in_range()

  DESCRIPTION
    Given the sequence of ranges min_max_ranges, find the minimal key that is
    in the left-most possible range. If there is no such key, then the current
    group does not have a MIN key that satisfies the WHERE clause. If a key is
    found, its value is stored in this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the given prefix in any of
                         the ranges
    HA_ERR_END_OF_FILE   - "" -
    other                if some error
*/

int GroupIndexSkipScanIterator::next_min_in_range() {
  ha_rkey_function search_mode;
  key_part_map keypart_map;
  bool found_null = false;
  int result = HA_ERR_KEY_NOT_FOUND;

  assert(!min_max_ranges->empty());

  /* Search from the left-most range to the right. */
  for (Quick_ranges::const_iterator it = min_max_ranges->begin();
       it != min_max_ranges->end(); ++it) {
    QUICK_RANGE *cur_range = *it;
    /*
      If the current value for the min/max argument is bigger than the right
      boundary of cur_range, there is no need to check this range.
    */
    if (it != min_max_ranges->begin() && !(cur_range->flag & NO_MAX_RANGE) &&
        (key_cmp(min_max_arg_part, (const uchar *)cur_range->max_key,
                 min_max_arg_len) == (min_max_keypart_asc ? 1 : -1)) &&
        !result)
      continue;

    if (cur_range->flag & NO_MIN_RANGE) {
      keypart_map = make_prev_keypart_map(real_key_parts);
      search_mode = get_search_mode(cur_range, min_max_keypart_asc, true);
    } else {
      /* Extend the search key with the lower boundary for this range. */
      memcpy(group_prefix + real_prefix_len, cur_range->min_key,
             cur_range->min_length);
      keypart_map = make_keypart_map(real_key_parts);
      search_mode = get_search_mode(cur_range, min_max_keypart_asc, true);
    }

    result = table()->file->ha_index_read_map(table()->record[0], group_prefix,
                                              keypart_map, search_mode);
    if (result) {
      if ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE) &&
          (cur_range->flag & (EQ_RANGE | NULL_RANGE)))
        continue; /* Check the next range. */

      /*
        In all other cases (HA_ERR_*, HA_READ_KEY_EXACT with NO_MIN_RANGE,
        HA_READ_AFTER_KEY, HA_READ_KEY_OR_NEXT) if the lookup failed for this
        range, it can't succeed for any other subsequent range.
      */
      break;
    }

    /* A key was found. */
    if (cur_range->flag & EQ_RANGE)
      break; /* No need to perform the checks below for equal keys. */

    if (min_max_keypart_asc && cur_range->flag & NULL_RANGE) {
      /*
        Remember this key, and continue looking for a non-NULL key that
        satisfies some other condition.
      */
      memcpy(table()->record[1], table()->record[0],
             table()->s->rec_buff_length);
      found_null = true;
      continue;
    }

    /* Check if record belongs to the current group. */
    if (key_cmp(index_info->key_part, group_prefix, real_prefix_len)) {
      result = HA_ERR_KEY_NOT_FOUND;
      continue;
    }

    /* If there is an upper limit, check if the found key is in the range. */
    if (!(cur_range->flag & NO_MAX_RANGE)) {
      /* Compose the MAX key for the range. */
      uchar *max_key = (uchar *)my_alloca(real_prefix_len + min_max_arg_len);
      memcpy(max_key, group_prefix, real_prefix_len);
      memcpy(max_key + real_prefix_len, cur_range->max_key,
             cur_range->max_length);
      /* Compare the found key with max_key. */
      int cmp_res = key_cmp(index_info->key_part, max_key,
                            real_prefix_len + min_max_arg_len);
      /*
        The key is outside of the range if:
        the interval is open and the key is equal to the maximum boundary
        or
        the key is greater than the maximum
      */
      if (((cur_range->flag & NEAR_MAX) && cmp_res == 0) ||
          (min_max_keypart_asc ? (cmp_res > 0) : (cmp_res < 0))) {
        result = HA_ERR_KEY_NOT_FOUND;
        continue;
      }
    }
    /* If we got to this point, the current key qualifies as MIN. */
    assert(result == 0);
    break;
  }
  /*
    If there was a key with NULL in the MIN/MAX field, and there was no other
    key without NULL from the same group that satisfies some other condition,
    then use the key with the NULL.
  */
  if (found_null && result) {
    memcpy(table()->record[0], table()->record[1], table()->s->rec_buff_length);
    result = 0;
  }
  return result;
}

/*
  Find the maximal key in a group that satisfies some range conditions for the
  min/max argument field.

  SYNOPSIS
    GroupIndexSkipScanIterator::next_max_in_range()

  DESCRIPTION
    Given the sequence of ranges min_max_ranges, find the maximal key that is
    in the right-most possible range. If there is no such key, then the current
    group does not have a MAX key that satisfies the WHERE clause. If a key is
    found, its value is stored in this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the given prefix in any of
                         the ranges
    HA_ERR_END_OF_FILE   - "" -
    other                if some error
*/

int GroupIndexSkipScanIterator::next_max_in_range() {
  ha_rkey_function search_mode;
  key_part_map keypart_map;
  int result = HA_ERR_KEY_NOT_FOUND;

  assert(!min_max_ranges->empty());

  /* Search from the right-most range to the left. */
  for (Quick_ranges::const_iterator it = min_max_ranges->end();
       it != min_max_ranges->begin(); --it) {
    QUICK_RANGE *cur_range = *(it - 1);
    /*
      If the current value for the min/max argument is smaller than the left
      boundary of cur_range, there is no need to check this range.
    */
    if (it != min_max_ranges->end() && !(cur_range->flag & NO_MIN_RANGE) &&
        (key_cmp(min_max_arg_part, (const uchar *)cur_range->min_key,
                 min_max_arg_len) == (min_max_keypart_asc ? -1 : 1)) &&
        !result)
      continue;

    if (cur_range->flag & NO_MAX_RANGE) {
      keypart_map = make_prev_keypart_map(real_key_parts);
      search_mode = get_search_mode(cur_range, min_max_keypart_asc, false);
    } else {
      /* Extend the search key with the upper boundary for this range. */
      memcpy(group_prefix + real_prefix_len, cur_range->max_key,
             cur_range->max_length);
      keypart_map = make_keypart_map(real_key_parts);
      search_mode = get_search_mode(cur_range, min_max_keypart_asc, false);
    }

    result = table()->file->ha_index_read_map(table()->record[0], group_prefix,
                                              keypart_map, search_mode);

    if (result) {
      if ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE) &&
          (cur_range->flag & EQ_RANGE))
        continue; /* Check the next range. */

      /*
        In no key was found with this upper bound, there certainly are no keys
        in the ranges to the left.
      */
      return result;
    }
    /* A key was found. */
    if (cur_range->flag & EQ_RANGE)
      return 0; /* No need to perform the checks below for equal keys. */

    /* Check if record belongs to the current group. */
    if (key_cmp(index_info->key_part, group_prefix, real_prefix_len)) {
      result = HA_ERR_KEY_NOT_FOUND;
      continue;  // Row not found
    }

    /* If there is a lower limit, check if the found key is in the range. */
    if (!(cur_range->flag & NO_MIN_RANGE)) {
      /* Compose the MIN key for the range. */
      uchar *min_key = (uchar *)my_alloca(real_prefix_len + min_max_arg_len);
      memcpy(min_key, group_prefix, real_prefix_len);
      memcpy(min_key + real_prefix_len, cur_range->min_key,
             cur_range->min_length);
      /* Compare the found key with min_key. */
      int cmp_res = key_cmp(index_info->key_part, min_key,
                            real_prefix_len + min_max_arg_len);
      /*
        The key is outside of the range if:
        the interval is open and the key is equal to the minimum boundary
        or
        the key is less than the minimum
      */
      if (((cur_range->flag & NEAR_MIN) && cmp_res == 0) ||
          (min_max_keypart_asc ? (cmp_res < 0) : (cmp_res > 0))) {
        result = HA_ERR_KEY_NOT_FOUND;
        continue;
      }
    }
    /* If we got to this point, the current key qualifies as MAX. */
    return result;
  }
  return HA_ERR_KEY_NOT_FOUND;
}

/*
  Update all MIN function results with the newly found value.

  SYNOPSIS
    GroupIndexSkipScanIterator::update_min_result()

  DESCRIPTION
    The method iterates through all MIN functions and updates the result value
    of each function by calling Item_sum::aggregator_add(), which in turn picks
    the new result value from this->table()->record[0], previously updated by
    next_min(). The updated value is stored in a member variable of each of the
    Item_sum objects, depending on the value type.

  IMPLEMENTATION
    The update must be done separately for MIN and MAX, immediately after
    next_min() was called and before next_max() is called, because both MIN and
    MAX take their result value from the same buffer this->table()->record[0]
    (i.e.  this->record).

  @param  reset   IN/OUT reset MIN value if TRUE.

  RETURN
    None
*/

void GroupIndexSkipScanIterator::update_min_result(bool *reset) {
  for (Item_sum *min_func : *min_functions) {
    if (*reset) {
      min_func->aggregator_clear();
      *reset = false;
    }
    min_func->aggregator_add();
  }
}

/*
  Update all MAX function results with the newly found value.

  SYNOPSIS
    GroupIndexSkipScanIterator::update_max_result()

  DESCRIPTION
    The method iterates through all MAX functions and updates the result value
    of each function by calling Item_sum::aggregator_add(), which in turn picks
    the new result value from this->table()->record[0], previously updated by
    next_max(). The updated value is stored in a member variable of each of the
    Item_sum objects, depending on the value type.

  IMPLEMENTATION
    The update must be done separately for MIN and MAX, immediately after
    next_max() was called, because both MIN and MAX take their result value
    from the same buffer this->table()->record[0] (i.e.  this->record).

  @param  reset   IN/OUT reset MAX value if TRUE.

  RETURN
    None
*/

void GroupIndexSkipScanIterator::update_max_result(bool *reset) {
  for (Item_sum *max_func : *max_functions) {
    if (*reset) {
      max_func->aggregator_clear();
      *reset = false;
    }
    max_func->aggregator_add();
  }
}
