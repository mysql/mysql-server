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
  pretty similar, since they are grouped under the TRP_WRAPPER type
  with a TABLE_READ_PLAN inside, but as we start splitting them out into
  individual AccessPath types, they will grow more logic.)
 */

#include "sql/join_optimizer/access_path.h"
#include "sql/range_optimizer/range_scan_plan.h"
#include "sql/range_optimizer/table_read_plan.h"

inline bool is_loose_index_scan(const AccessPath *path) {
  if (path->type != AccessPath::TRP_WRAPPER) {
    return false;
  }
  int type = path->trp_wrapper().trp->get_type();
  return type == QS_TYPE_SKIP_SCAN || type == QS_TYPE_GROUP_MIN_MAX;
}

inline bool is_agg_loose_index_scan(const AccessPath *path) {
  return is_loose_index_scan(path) &&
         path->trp_wrapper().trp->is_agg_loose_index_scan();
}

/**
  Whether the range access method is capable of returning records
  in reverse order.
 */
inline bool reverse_sort_possible(const AccessPath *path) {
  return path->type == AccessPath::INDEX_RANGE_SCAN;
}

/**
  Whether the access path is an INDEX_RANGE_SCAN that returns rows in reverse
  order. (Note that non-range index scans return false here.)
 */
inline bool is_reverse_sorted_range(const AccessPath *path) {
  return path->type == AccessPath::INDEX_RANGE_SCAN &&
         path->index_range_scan().reverse;
}

/**
  Ask the TRP to reverse itself; returns false if successful.
  Overridden only in INDEX_RANGE_SCAN.
 */
inline bool make_reverse(uint used_key_parts, AccessPath *path) {
  if (path->type == AccessPath::INDEX_RANGE_SCAN) {
    if (path->index_range_scan().geometry) {
      return true;
    }
    path->index_range_scan().reverse = true;
    path->index_range_scan().num_used_key_parts = used_key_parts;
    return false;
  } else {
    return true;
  }
}

inline void set_need_sorted_output(AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      path->index_range_scan().mrr_flags |= HA_MRR_SORTED;
      break;
    case AccessPath::TRP_WRAPPER:
      path->trp_wrapper().trp->need_sorted_output();
      break;
    default:
      assert(false);
  }
}

/**
  If this is an index range scan, and that range scan uses a single
  index, returns the index used. Otherwise, MAX_KEY.
 */
inline unsigned used_index(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      return path->index_range_scan().index;
    case AccessPath::TRP_WRAPPER:
      return path->trp_wrapper().trp->index;
    default:
      return MAX_KEY;
  }
}

/**
  Return true if there is only one range and this uses the whole unique key.
 */
inline bool unique_key_range(const AccessPath *path) {
  if (path->type != AccessPath::INDEX_RANGE_SCAN) {
    return false;
  }
  if (path->index_range_scan().num_ranges == 1) {
    QUICK_RANGE *tmp = path->index_range_scan().ranges[0];
    if ((tmp->flag & (EQ_RANGE | NULL_RANGE)) == EQ_RANGE) {
      KEY *key =
          path->index_range_scan().used_key_part[0].field->table->key_info +
          path->index_range_scan().index;
      return (key->flags & HA_NOSAME) && key->key_length == tmp->min_length;
    }
  }
  return false;
}

inline void get_fields_used(const AccessPath *path, MY_BITMAP *used_fields) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      for (uint i = 0; i < path->index_range_scan().num_used_key_parts; ++i) {
        bitmap_set_bit(
            used_fields,
            path->index_range_scan().used_key_part[i].field->field_index());
      }
      break;
    case AccessPath::TRP_WRAPPER:
      path->trp_wrapper().trp->get_fields_used(used_fields);
      break;
    default:
      assert(false);
  }
}

inline unsigned get_used_key_parts(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      return path->index_range_scan().num_used_key_parts;
    case AccessPath::TRP_WRAPPER:
      return path->trp_wrapper().trp->used_key_parts;
    default:
      assert(false);
      return 0;
  }
}

/**
  Return whether any index used by this range scan uses the field(s)
  marked in passed bitmap. Assert-fails if not a range scan.
 */
inline bool uses_index_on_fields(const AccessPath *path,
                                 const MY_BITMAP *fields) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      return is_key_used(path->index_range_scan().used_key_part[0].field->table,
                         path->index_range_scan().index, fields);
    case AccessPath::TRP_WRAPPER:
      return path->trp_wrapper().trp->is_keys_used(fields);
    default:
      assert(false);
      return false;
  }
}

/**
  Get the total length of first used_key_parts parts of the key,
  in bytes. Only applicable for range access types that use a single
  index (others will assert-fail).
 */
inline unsigned get_max_used_key_length(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN: {
      int max_used_key_length = 0;
      Bounds_checked_array ranges{path->index_range_scan().ranges,
                                  path->index_range_scan().num_ranges};
      for (const QUICK_RANGE *range : ranges) {
        max_used_key_length =
            std::max<int>(max_used_key_length, range->min_length);
        max_used_key_length =
            std::max<int>(max_used_key_length, range->max_length);
      }
      return max_used_key_length;
    }
    case AccessPath::TRP_WRAPPER:
      return path->trp_wrapper().trp->get_max_used_key_length();
    default:
      assert(false);
      return 0;
  }
}

/*
  Append text representation of the range scan (what and how is
  merged) to str. The result is added to "Extra" field in EXPLAIN output.
 */
inline void add_info_string(const AccessPath *path, String *str) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN: {
      TABLE *table = path->index_range_scan().used_key_part[0].field->table;
      KEY *key_info = table->key_info + path->index_range_scan().index;
      str->append(key_info->name);
      break;
    }
    case AccessPath::TRP_WRAPPER:
      path->trp_wrapper().trp->add_info_string(str);
      break;
    default:
      assert(false);
  }
}

/*
  Append comma-separated list of keys this quick select uses to key_names;
  append comma-separated list of corresponding used lengths to used_lengths.
  This is used by select_describe.

  path must be a range scan, or there will be an assert.
 */
inline void add_keys_and_lengths(const AccessPath *path, String *key_names,
                                 String *used_lengths) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN: {
      TABLE *table = path->index_range_scan().used_key_part[0].field->table;
      KEY *key_info = table->key_info + path->index_range_scan().index;
      key_names->append(key_info->name);

      char buf[64];
      size_t length =
          longlong10_to_str(get_max_used_key_length(path), buf, 10) - buf;
      used_lengths->append(buf, length);
      break;
    }
    case AccessPath::TRP_WRAPPER:
      path->trp_wrapper().trp->add_keys_and_lengths(key_names, used_lengths);
      break;
    default:
      assert(false);
  }
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
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      trace_basic_info_index_range_scan(thd, path, param, trace_object);
      break;
    case AccessPath::TRP_WRAPPER:
      path->trp_wrapper().trp->trace_basic_info(
          thd, param, path->cost, path->num_output_rows, trace_object);
      break;
    default:
      assert(false);
  }
}

/**
  Returns the type of range scan this access path represents.
  If not generated by the range optimizer, will assert-fail.
 */
inline RangeScanType get_range_scan_type(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      return QS_TYPE_RANGE;
    case AccessPath::TRP_WRAPPER:
      return path->trp_wrapper().trp->get_type();
    default:
      assert(false);
      return QS_TYPE_RANGE;
  }
}

inline bool get_forced_by_hint(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      return false;  // There is no hint for plain range scan.
    case AccessPath::TRP_WRAPPER:
      return path->trp_wrapper().trp->forced_by_hint;
    default:
      assert(false);
      return false;
  }
}

#ifndef NDEBUG
/*
   Print quick select information to DBUG_FILE. Caller is responsible
   for locking DBUG_FILE before this call and unlocking it afterwards.
 */
inline void dbug_dump(const AccessPath *path, int indent, bool verbose) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN: {
      const auto &param = path->index_range_scan();
      dbug_dump_range(indent, verbose, param.used_key_part[0].field->table,
                      param.index, param.used_key_part,
                      {param.ranges, param.num_ranges});
      break;
    }
    case AccessPath::TRP_WRAPPER:
      path->trp_wrapper().trp->dbug_dump(0, true);
      break;
    default:
      assert(false);
  }
}
#endif

#endif  // SQL_RANGE_OPTIMIZER_TRP_HELPERS_H_
