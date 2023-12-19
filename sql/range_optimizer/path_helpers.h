/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_PATH_HELPERS_H_
#define SQL_RANGE_OPTIMIZER_PATH_HELPERS_H_

/**
  @file
  Various small helpers to abstract over the fact that AccessPath can contain
  a number of different range scan types.
 */

#include "sql/join_optimizer/access_path.h"
#include "sql/range_optimizer/group_index_skip_scan_plan.h"
#include "sql/range_optimizer/index_merge_plan.h"
#include "sql/range_optimizer/index_range_scan_plan.h"
#include "sql/range_optimizer/index_skip_scan_plan.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/rowid_ordered_retrieval_plan.h"
#include "sql/sql_opt_exec_shared.h"

inline bool is_loose_index_scan(const AccessPath *path) {
  return path->type == AccessPath::INDEX_SKIP_SCAN ||
         path->type == AccessPath::GROUP_INDEX_SKIP_SCAN;
}

inline bool is_agg_loose_index_scan(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_SKIP_SCAN:
      return path->index_skip_scan().param->has_aggregate_function;
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      return path->group_index_skip_scan().param->have_agg_distinct;
      break;
    default:
      return false;
  }
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
  Ask the AccessPath to reverse itself; returns false if successful.
  Overridden only in INDEX_RANGE_SCAN.
 */
inline bool make_reverse(uint used_key_parts, AccessPath *path) {
  if (path->type == AccessPath::INDEX_RANGE_SCAN) {
    if (path->index_range_scan().geometry) {
      return true;
    }
    path->index_range_scan().reverse = true;
    TABLE *table = path->index_range_scan().used_key_part[0].field->table;
    path->index_range_scan().using_extended_key_parts =
        (used_key_parts > table->key_info[path->index_range_scan().index]
                              .user_defined_key_parts);
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
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
    case AccessPath::INDEX_SKIP_SCAN:
      // Always sorted already.
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
    case AccessPath::INDEX_SKIP_SCAN:
      return path->index_skip_scan().index;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      return path->group_index_skip_scan().index;
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
    case AccessPath::INDEX_MERGE:
      for (AccessPath *child : *path->index_merge().children) {
        get_fields_used(child, used_fields);
      }
      break;
    case AccessPath::ROWID_INTERSECTION:
      for (AccessPath *child : *path->rowid_intersection().children) {
        get_fields_used(child, used_fields);
      }
      if (path->rowid_intersection().cpk_child != nullptr) {
        get_fields_used(path->rowid_intersection().cpk_child, used_fields);
      }
      break;
    case AccessPath::ROWID_UNION:
      for (AccessPath *child : *path->rowid_union().children) {
        get_fields_used(child, used_fields);
      }
      break;
    case AccessPath::INDEX_SKIP_SCAN:
      for (uint i = 0; i < path->index_skip_scan().num_used_key_parts; ++i) {
        bitmap_set_bit(used_fields, path->index_skip_scan()
                                        .param->index_info->key_part[i]
                                        .field->field_index());
      }
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      for (uint i = 0; i < path->group_index_skip_scan().num_used_key_parts;
           ++i) {
        bitmap_set_bit(used_fields, path->group_index_skip_scan()
                                        .param->index_info->key_part[i]
                                        .field->field_index());
      }
      break;
    default:
      assert(false);
  }
}

inline unsigned get_used_key_parts(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      return path->index_range_scan().num_used_key_parts;
    case AccessPath::INDEX_SKIP_SCAN:
      return path->index_skip_scan().num_used_key_parts;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      return path->group_index_skip_scan().num_used_key_parts;
    case AccessPath::REF:
      return path->ref().ref->key_parts;
    case AccessPath::REF_OR_NULL:
      return path->ref_or_null().ref->key_parts;
    case AccessPath::EQ_REF:
      return path->eq_ref().ref->key_parts;
    case AccessPath::PUSHED_JOIN_REF:
      return path->pushed_join_ref().ref->key_parts;
    case AccessPath::FULL_TEXT_SEARCH:
      return path->full_text_search().ref->key_parts;
    case AccessPath::MRR:
      return path->mrr().ref->key_parts;
    case AccessPath::INDEX_DISTANCE_SCAN:
    case AccessPath::INDEX_SCAN:
    case AccessPath::INDEX_MERGE:
    case AccessPath::ROWID_INTERSECTION:
    case AccessPath::ROWID_UNION:
      return 0;
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
    case AccessPath::INDEX_MERGE:
      for (AccessPath *child : *path->index_merge().children) {
        if (uses_index_on_fields(child, fields)) {
          return true;
        }
      }
      return false;
    case AccessPath::ROWID_INTERSECTION:
      for (AccessPath *child : *path->rowid_intersection().children) {
        if (uses_index_on_fields(child, fields)) {
          return true;
        }
      }
      return path->rowid_intersection().cpk_child != nullptr &&
             uses_index_on_fields(path->rowid_intersection().cpk_child, fields);
    case AccessPath::ROWID_UNION:
      for (AccessPath *child : *path->rowid_union().children) {
        if (uses_index_on_fields(child, fields)) {
          return true;
        }
      }
      return false;
    case AccessPath::INDEX_SKIP_SCAN:
      return is_key_used(path->index_skip_scan().table,
                         path->index_skip_scan().index, fields);
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      return is_key_used(path->group_index_skip_scan().table,
                         path->group_index_skip_scan().index, fields);
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
    case AccessPath::INDEX_SKIP_SCAN: {
      int max_used_key_length = 0;
      KEY_PART_INFO *p = path->index_skip_scan().param->index_info->key_part;
      for (uint i = 0; i < path->index_skip_scan().num_used_key_parts;
           i++, p++) {
        max_used_key_length += p->store_length;
      }
      return max_used_key_length;
    }
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      return path->group_index_skip_scan().param->max_used_key_length;
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
    case AccessPath::INDEX_MERGE: {
      bool first = true;
      TABLE *table = path->index_merge().table;
      str->append(STRING_WITH_LEN("sort_union("));

      // For EXPLAIN compatibility with older versions, PRIMARY is always
      // printed last.
      for (bool print_primary : {false, true}) {
        for (AccessPath *child : *path->index_merge().children) {
          const bool is_primary = table->file->primary_key_is_clustered() &&
                                  used_index(child) == table->s->primary_key;
          if (is_primary != print_primary) continue;
          if (!first)
            str->append(',');
          else
            first = false;
          ::add_info_string(child, str);
        }
      }
      str->append(')');
      break;
    }
    case AccessPath::ROWID_INTERSECTION: {
      bool first = true;
      str->append(STRING_WITH_LEN("intersect("));
      for (AccessPath *current : *path->rowid_intersection().children) {
        if (!first)
          str->append(',');
        else
          first = false;
        ::add_info_string(current, str);
      }
      if (path->rowid_intersection().cpk_child) {
        str->append(',');
        ::add_info_string(path->rowid_intersection().cpk_child, str);
      }
      str->append(')');
      break;
    }
    case AccessPath::ROWID_UNION: {
      bool first = true;
      str->append(STRING_WITH_LEN("union("));
      for (AccessPath *current : *path->rowid_union().children) {
        if (!first)
          str->append(',');
        else
          first = false;
        ::add_info_string(current, str);
      }
      str->append(')');
      break;
    }
    case AccessPath::INDEX_SKIP_SCAN: {
      str->append(STRING_WITH_LEN("index_for_skip_scan("));
      str->append(path->index_skip_scan().param->index_info->name);
      str->append(')');
      break;
    }
    case AccessPath::GROUP_INDEX_SKIP_SCAN: {
      str->append(STRING_WITH_LEN("index_for_group_by("));
      str->append(path->group_index_skip_scan().param->index_info->name);
      str->append(')');
      break;
    }
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
    case AccessPath::INDEX_MERGE:
      add_keys_and_lengths_index_merge(path, key_names, used_lengths);
      break;
    case AccessPath::ROWID_INTERSECTION:
      add_keys_and_lengths_rowid_intersection(path, key_names, used_lengths);
      break;
    case AccessPath::ROWID_UNION:
      add_keys_and_lengths_rowid_union(path, key_names, used_lengths);
      break;
    case AccessPath::INDEX_SKIP_SCAN: {
      key_names->append(path->index_skip_scan().param->index_info->name);

      char buf[64];
      uint length =
          longlong10_to_str(get_max_used_key_length(path), buf, 10) - buf;
      used_lengths->append(buf, length);

      break;
    }
    case AccessPath::GROUP_INDEX_SKIP_SCAN: {
      key_names->append(path->group_index_skip_scan().param->index_info->name);

      char buf[64];
      uint length =
          longlong10_to_str(get_max_used_key_length(path), buf, 10) - buf;
      used_lengths->append(buf, length);

      break;
    }
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
    case AccessPath::INDEX_MERGE:
      trace_basic_info_index_merge(thd, path, param, trace_object);
      break;
    case AccessPath::ROWID_INTERSECTION:
      trace_basic_info_rowid_intersection(thd, path, param, trace_object);
      break;
    case AccessPath::ROWID_UNION:
      trace_basic_info_rowid_union(thd, path, param, trace_object);
      break;
    case AccessPath::INDEX_SKIP_SCAN:
      trace_basic_info_index_skip_scan(thd, path, param, trace_object);
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      trace_basic_info_group_index_skip_scan(thd, path, param, trace_object);
      break;
    default:
      assert(false);
  }
}

inline bool get_forced_by_hint(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      return false;  // There is no hint for plain range scan.
    case AccessPath::INDEX_MERGE:
      return path->index_merge().forced_by_hint;
    case AccessPath::ROWID_INTERSECTION:
      return path->rowid_intersection().forced_by_hint;
    case AccessPath::ROWID_UNION:
      return path->rowid_union().forced_by_hint;
    case AccessPath::INDEX_SKIP_SCAN:
      return path->index_skip_scan().forced_by_hint;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      return path->group_index_skip_scan().forced_by_hint;
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
    case AccessPath::INDEX_MERGE: {
      dbug_dump_index_merge(indent, verbose, *path->index_merge().children);
      break;
    }
    case AccessPath::ROWID_INTERSECTION:
      dbug_dump_rowid_intersection(indent, verbose,
                                   *path->rowid_intersection().children);
      break;
    case AccessPath::ROWID_UNION:
      dbug_dump_rowid_union(indent, verbose, *path->rowid_union().children);
      break;
    case AccessPath::INDEX_SKIP_SCAN:
      dbug_dump_index_skip_scan(indent, verbose, path);
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      dbug_dump_group_index_skip_scan(indent, verbose, path);
      break;
    default:
      assert(false);
  }
}
#endif

#endif  // SQL_RANGE_OPTIMIZER_PATH_HELPERS_H_
