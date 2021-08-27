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

#ifndef SQL_RANGE_OPTIMIZER_TABLE_READ_PLAN_H_
#define SQL_RANGE_OPTIMIZER_TABLE_READ_PLAN_H_

#include "sql/range_optimizer/range_optimizer.h"

class Opt_trace_object;
class RANGE_OPT_PARAM;
class SEL_ROOT;

/*
  Table rows retrieval plan. Range optimizer creates RowIterators
  from table read plans.
*/
class TABLE_READ_PLAN {
 public:
  /*
    Plan read cost, with or without cost of full row retrieval, depending
    on plan creation parameters.
  */
  Cost_estimate cost_est;
  ha_rows records; /* estimate of #rows to be examined */

  // The table scanned.
  TABLE *const table;

  /*
    Index this quick select uses, or MAX_KEY for quick selects
    that use several indexes
   */
  const uint index;

  /*
    Max. number of (first) key parts this quick select uses for retrieval.
    eg. for "(key1p1=c1 AND key1p2=c2) OR key1p1=c2" used_key_parts == 2.
    Applicable if index!= MAX_KEY.

    For QUICK_GROUP_MIN_MAX_SELECT it includes MIN/MAX argument keyparts.
   */
  uint used_key_parts;

  /*
    Whether we are expected to output rows ordered by row ID, ie.,
    we are the child of a ROR scan. This is only applicable for TRP_RANGE
    and QUICK_ROR_INTERSECT_SELECT.
   */
  bool need_rows_in_rowid_order = false;

  const bool forced_by_hint;

  /*
    Create quick select for this plan.
    SYNOPSIS
     make_quick()
       retrieve_full_rows  If true, created quick select will do full record
                           retrieval.
       expected_rows       Number of rows we expect this iterator to return.
       return_mem_root     Memory pool to use.
       examined_rows       If not nullptr, should be incremented whenever
                           a row is fetched from the storage engine.

    NOTES
      retrieve_full_rows is ignored by some implementations.

    RETURN
      created quick select
      nullptr on any error.
  */
  virtual RowIterator *make_quick(THD *thd, double expected_rows,
                                  bool retrieve_full_rows,
                                  MEM_ROOT *return_mem_root,
                                  ha_rows *examined_rows) = 0;

  TABLE_READ_PLAN(TABLE *table_arg, int index_arg, uint used_key_parts_arg,
                  bool forced_by_hint_arg)
      : table(table_arg),
        index(index_arg),
        used_key_parts(used_key_parts_arg),
        forced_by_hint(forced_by_hint_arg) {}
  virtual ~TABLE_READ_PLAN() = default;

  /**
     Add basic info for this TABLE_READ_PLAN to the optimizer trace.

     @param thd          Thread handle
     @param param        Parameters for range analysis of this table
     @param trace_object The optimizer trace object the info is appended to
  */
  virtual void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                                Opt_trace_object *trace_object) const = 0;

  virtual RangeScanType get_type() const = 0;

  /*
    Append text representation of quick select structure (what and how is
    merged) to str. The result is added to "Extra" field in EXPLAIN output.
   */
  virtual void add_info_string(String *) const = 0;

  /*
    Append comma-separated list of keys this quick select uses to key_names;
    append comma-separated list of corresponding used lengths to used_lengths.
    This is used by select_describe.
  */
  virtual void add_keys_and_lengths(String *key_names,
                                    String *used_lengths) const = 0;

  // Return 1 if there is only one range and this uses the whole unique key.
  // Overridden only by TRP_RANGE.
  virtual bool unique_key_range() const { return false; }

  // Overridden only by TRP_GROUP_MIN_MAX.
  virtual bool is_agg_loose_index_scan() const { return false; }

  // Whether the range access method returns records in reverse order.
  // Overridden only by TRP_RANGE.
  virtual bool reverse_sorted() const { return false; }

  /*
    Request that this quick select produce sorted output.
    Not all quick selects can provide sorted output; the caller is responsible
    for calling this function only for those quick selects that can.
    The implementation is also allowed to provide sorted output even if it
    was not requested if beneficial, or required by implementation
    internals.
   */
  virtual void need_sorted_output() = 0;

  // Ask the TRP to reverse itself; returns false if successful.
  // Overridden only in TRP_RANGE.
  virtual bool make_reverse(uint used_key_parts [[maybe_unused]]) {
    return true;
  }

  /*
    Return 1 if any index used by this quick select
    uses field which is marked in passed bitmap.
   */
  virtual bool is_keys_used(const MY_BITMAP *fields);

  /**
    Get the fields used by the range access method.

    @param[out] used_fields Bitmap of fields that this range access
                            method uses.
   */
  virtual void get_fields_used(MY_BITMAP *used_fields) const = 0;

  /**
    Get the total length of first used_key_parts parts of the key,
    in bytes. Only applicable for the access types that use a single
    index (others will assert-fail).
   */
  virtual unsigned get_max_used_key_length() const = 0;

#ifndef NDEBUG
  /*
    Print quick select information to DBUG_FILE. Caller is responsible
    for locking DBUG_FILE before this call and unlocking it afterwards.
   */
  virtual void dbug_dump(int indent, bool verbose) = 0;
#endif
};

inline bool is_loose_index_scan(const TABLE_READ_PLAN *trp) {
  int type = trp->get_type();
  return type == QS_TYPE_SKIP_SCAN || type == QS_TYPE_GROUP_MIN_MAX;
}

/**
  Whether the range access method is capable of returning records
  in reverse order.
 */
inline bool reverse_sort_possible(const TABLE_READ_PLAN *trp) {
  return trp->get_type() == QS_TYPE_RANGE;
}

void trace_quick_description(TABLE_READ_PLAN *trp, Opt_trace_context *trace);

#endif  // SQL_RANGE_OPTIMIZER_TABLE_READ_PLAN_H_
