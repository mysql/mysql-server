/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_TRP_HELPERS_H_
#define SQL_RANGE_OPTIMIZER_TRP_HELPERS_H_

/**
  @file
  Various small helpers to abstract over the fact that AccessPath can contain
  a number of different range scan types. (For the time being, they are all
  pretty similar, since they are grouped under the INDEX_RANGE_SCAN type
  with a TABLE_READ_PLAN inside, but as we start splitting them out into
  individual AccessPath types, they will grow more logic.)
 */

#include "sql/join_optimizer/access_path.h"
#include "sql/range_optimizer/table_read_plan.h"

inline bool is_loose_index_scan(const AccessPath *path) {
  if (path->type != AccessPath::INDEX_RANGE_SCAN) {
    return false;
  }
  int type = path->index_range_scan().trp->get_type();
  return type == QS_TYPE_SKIP_SCAN || type == QS_TYPE_GROUP_MIN_MAX;
}

inline bool is_agg_loose_index_scan(const AccessPath *path) {
  return is_loose_index_scan(path) &&
         path->index_range_scan().trp->is_agg_loose_index_scan();
}

/**
  Whether the range access method is capable of returning records
  in reverse order.
 */
inline bool reverse_sort_possible(const AccessPath *path) {
  if (path->type != AccessPath::INDEX_RANGE_SCAN) {
    return false;
  }
  return path->index_range_scan().trp->get_type() == QS_TYPE_RANGE;
}

/**
  Whether the access path is a TRP_RANGE that returns rows in reverse order.
  (Note that non-range index scans return false here.)
 */
inline bool is_reverse_sorted_range(const AccessPath *path) {
  if (path->type != AccessPath::INDEX_RANGE_SCAN) {
    return false;
  }
  return path->index_range_scan().trp->reverse_sorted();
}

/**
  Ask the TRP to reverse itself; returns false if successful.
  Overridden only in TRP_RANGE.
 */
inline bool make_reverse(uint used_key_parts, AccessPath *path) {
  if (path->type != AccessPath::INDEX_RANGE_SCAN) {
    return false;
  }
  return path->index_range_scan().trp->make_reverse(used_key_parts);
}

inline void set_need_sorted_output(AccessPath *path) {
  path->index_range_scan().trp->need_sorted_output();
}

/**
  If this is an index range scan, and that range scan uses a single
  index, returns the index used. Otherwise, MAX_KEY.
 */
inline unsigned used_index(const AccessPath *path) {
  if (path->type != AccessPath::INDEX_RANGE_SCAN) {
    return MAX_KEY;
  }
  return path->index_range_scan().trp->index;
}

/**
  Return true if there is only one range and this uses the whole unique key.
  Overridden only by TRP_RANGE.
 */
inline bool unique_key_range(const AccessPath *path) {
  if (path->type != AccessPath::INDEX_RANGE_SCAN) {
    return false;
  }
  return path->index_range_scan().trp->unique_key_range();
}

inline void get_fields_used(const AccessPath *path, MY_BITMAP *used_fields) {
  path->index_range_scan().trp->get_fields_used(used_fields);
}

inline unsigned get_used_key_parts(const AccessPath *path) {
  return path->index_range_scan().trp->used_key_parts;
}

/**
  Return whether any index used by this range scan uses the field(s)
  marked in passed bitmap. Assert-fails if not a range scan.
 */
inline bool uses_index_on_fields(const AccessPath *path,
                                 const MY_BITMAP *fields) {
  return path->index_range_scan().trp->is_keys_used(fields);
}

/**
  Get the total length of first used_key_parts parts of the key,
  in bytes. Only applicable for range access types that use a single
  index (others will assert-fail).
 */
inline unsigned get_max_used_key_length(const AccessPath *path) {
  return path->index_range_scan().trp->get_max_used_key_length();
}

/*
  Append text representation of the range scan (what and how is
  merged) to str. The result is added to "Extra" field in EXPLAIN output.
 */
inline void add_info_string(const AccessPath *path, String *str) {
  path->index_range_scan().trp->add_info_string(str);
}

/*
  Append comma-separated list of keys this quick select uses to key_names;
  append comma-separated list of corresponding used lengths to used_lengths.
  This is used by select_describe.

  path must be a range scan, or there will be an assert.
 */
inline void add_keys_and_lengths(const AccessPath *path, String *key_names,
                                 String *used_lengths) {
  path->index_range_scan().trp->add_keys_and_lengths(key_names, used_lengths);
}

/**
  Add basic info for this range scan to the optimizer trace.

  path must be a range scan, or there will be an assert.

  @param thd          Thread handle
  @param param        Parameters for range analysis of this table
  @param trace_object The optimizer trace object the info is appended to
 */
inline void trace_basic_info(THD *thd, const AccessPath *path,
                             const RANGE_OPT_PARAM *param,
                             Opt_trace_object *trace_object) {
  path->index_range_scan().trp->trace_basic_info(
      thd, param, path->cost, path->num_output_rows, trace_object);
}

/**
  Returns the type of range scan this access path represents.
  If not generated by the range optimizer, will assert-fail.
 */
inline RangeScanType get_range_scan_type(const AccessPath *path) {
  return path->index_range_scan().trp->get_type();
}

#ifndef NDEBUG
/*
   Print quick select information to DBUG_FILE. Caller is responsible
   for locking DBUG_FILE before this call and unlocking it afterwards.
 */
inline void dbug_dump(const AccessPath *path, int indent, bool verbose) {
  path->index_range_scan().trp->dbug_dump(indent, verbose);
}
#endif

#endif  // SQL_RANGE_OPTIMIZER_TRP_HELPERS_H_
