/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/explain_access_path.h"

#include <functional>
#include <string>
#include <vector>

#include <openssl/sha.h>

#include "my_base.h"
#include "sha2.h"
#include "sql-common/json_dom.h"
#include "sql/filesort.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_sum.h"
#include "sql/iterators/basic_row_iterators.h"
#include "sql/iterators/bka_iterator.h"
#include "sql/iterators/composite_iterators.h"
#include "sql/iterators/hash_join_iterator.h"
#include "sql/iterators/ref_row_iterators.h"
#include "sql/iterators/sorting_iterator.h"
#include "sql/iterators/timing_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/cost_model.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/opt_explain.h"
#include "sql/opt_explain_traditional.h"
#include "sql/query_result.h"
#include "sql/range_optimizer/group_index_skip_scan_plan.h"
#include "sql/range_optimizer/index_skip_scan_plan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "template_utils.h"

using std::string;
using std::vector;

/// This structure encapsulates the information needed to create a Json object
/// for a child access path.
struct ExplainChild {
  AccessPath *path;

  // Normally blank. If not blank, a heading for this iterator
  // saying what kind of role it has to the parent if it is not
  // obvious. E.g., FilterIterator can print iterators that are
  // children because they come out of subselect conditions.
  std::string description = "";

  // If this child is the root of a new JOIN, it is contained here.
  JOIN *join = nullptr;

  // If it's convenient to assign json fields for this child while creating this
  // structure, then a json object can be allocated and set here.
  Json_object *obj = nullptr;
};

/// Convenience function to add a json field.
template <class T, class... Args>
static bool AddMemberToObject(Json_object *obj, const char *alias,
                              Args &&... ctor_args) {
  return obj->add_alias(
      alias, create_dom_ptr<T, Args...>(std::forward<Args>(ctor_args)...));
}

template <class T, class... Args>
static bool AddElementToArray(const std::unique_ptr<Json_array> &array,
                              Args &&... ctor_args) {
  return array->append_alias(
      create_dom_ptr<T, Args...>(std::forward<Args>(ctor_args)...));
}

static bool PrintRanges(const QUICK_RANGE *const *ranges, unsigned num_ranges,
                        const KEY_PART_INFO *key_part, bool single_part_only,
                        const std::unique_ptr<Json_array> &range_array,
                        string *ranges_out);
static std::unique_ptr<Json_object> ExplainAccessPath(
    const AccessPath *path, const AccessPath *materialized_path, JOIN *join,
    bool is_root_of_join, Json_object *input_obj = nullptr);
static std::unique_ptr<Json_object> AssignParentPath(
    AccessPath *parent_path, const AccessPath *materialized_path,
    std::unique_ptr<Json_object> obj, JOIN *join);
inline static double GetJSONDouble(const Json_object *obj, const char *key) {
  return down_cast<const Json_double *>(obj->get(key))->value();
}

/*
  The index information is displayed like this :

  [<Prefix>] [COVERING] INDEX <index_operation>
    ON table_alias USING index_name [ (<lookup_condition>) ]
    [ OVER <range> [, <range>, ...] ]
    [ (REVERSE) ]
    [ WITH INDEX CONDITION: <pushed_idx_cond> ]

  where <index_operation> =
     {scan|skip scan|range scan|lookup|search|
      skip scan for grouping|skip scan for deduplication}
  where <Prefix> = {Single-row|Multi-range}

  Return obj. Not necessary, but for the sake of AddMemberToObject() returning
  NULL in case of failure, we need to return something non-NULL to indicate
  success.
*/
static bool SetIndexInfoInObject(
    string *str, const char *json_index_access_type, const char *prefix,
    TABLE *table, const KEY *key, const char *index_access_type,
    const string lookup_condition, const string *ranges_text,
    std::unique_ptr<Json_array> range_arr, bool reverse, Item *pushed_idx_cond,
    Json_object *obj) {
  string idx_cond_str = pushed_idx_cond ? ItemToString(pushed_idx_cond) : "";
  string covering_index =
      string(table->key_read ? "Covering index " : "Index ");
  bool error = false;

  if (prefix) covering_index[0] = tolower(covering_index[0]);

  *str += (prefix ? string(prefix) + " " : "") + covering_index +
          index_access_type +  // lookup/scan/search
          " on " + table->alias + " using " + key->name +
          (!lookup_condition.empty() ? " (" + lookup_condition + ")" : "") +
          (ranges_text != nullptr ? " over " + *ranges_text : "") +
          (reverse ? " (reverse)" : "") +
          (pushed_idx_cond ? ", with index condition: " + idx_cond_str : "");
  *str += table->file->explain_extra();

  error |= AddMemberToObject<Json_string>(obj, "access_type", "index");
  error |= AddMemberToObject<Json_string>(obj, "index_access_type",
                                          json_index_access_type);
  error |= AddMemberToObject<Json_boolean>(obj, "covering", table->key_read);
  error |= AddMemberToObject<Json_string>(obj, "table_name", table->alias);
  error |= AddMemberToObject<Json_string>(obj, "index_name", key->name);
  if (!lookup_condition.empty())
    error |= AddMemberToObject<Json_string>(obj, "lookup_condition",
                                            lookup_condition);
  if (range_arr) error |= obj->add_alias("ranges", std::move(range_arr));
  if (reverse) error |= AddMemberToObject<Json_boolean>(obj, "reverse", true);
  if (pushed_idx_cond)
    error |= AddMemberToObject<Json_string>(obj, "pushed_index_condition",
                                            idx_cond_str);
  if (!table->file->explain_extra().empty())
    error |= AddMemberToObject<Json_string>(obj, "message",
                                            table->file->explain_extra());

  return error;
}

string JoinTypeToString(JoinType join_type) {
  switch (join_type) {
    case JoinType::INNER:
      return "inner join";
    case JoinType::OUTER:
      return "left join";
    case JoinType::ANTI:
      return "antijoin";
    case JoinType::SEMI:
      return "semijoin";
    default:
      assert(false);
      return "<error>";
  }
}

string HashJoinTypeToString(RelationalExpression::Type join_type,
                            string *explain_json_value) {
  switch (join_type) {
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
      if (explain_json_value)
        *explain_json_value = JoinTypeToString(JoinType::INNER);
      return "Inner hash join";
    case RelationalExpression::LEFT_JOIN:
      if (explain_json_value)
        *explain_json_value = JoinTypeToString(JoinType::OUTER);
      return "Left hash join";
    case RelationalExpression::ANTIJOIN:
      if (explain_json_value)
        *explain_json_value = JoinTypeToString(JoinType::ANTI);
      return "Hash antijoin";
    case RelationalExpression::SEMIJOIN:
      if (explain_json_value)
        *explain_json_value = JoinTypeToString(JoinType::SEMI);
      return "Hash semijoin";
    default:
      assert(false);
      return "<error>";
  }
}

static bool GetAccessPathsFromItem(Item *item_arg, const char *source_text,
                                   vector<ExplainChild> *children) {
  return WalkItem(
      item_arg, enum_walk::POSTFIX, [children, source_text](Item *item) {
        if (item->type() != Item::SUBSELECT_ITEM) {
          return false;
        }

        Item_subselect *subselect = down_cast<Item_subselect *>(item);
        Query_block *query_block = subselect->unit->first_query_block();
        char description[256];
        if (query_block->is_dependent()) {
          snprintf(description, sizeof(description),
                   "Select #%d (subquery in %s; dependent)",
                   query_block->select_number, source_text);
        } else if (!query_block->is_cacheable()) {
          snprintf(description, sizeof(description),
                   "Select #%d (subquery in %s; uncacheable)",
                   query_block->select_number, source_text);
        } else {
          snprintf(description, sizeof(description),
                   "Select #%d (subquery in %s; run only once)",
                   query_block->select_number, source_text);
        }
        if (query_block->join->needs_finalize) {
          subselect->unit->finalize(current_thd);
        }
        AccessPath *path;
        if (subselect->unit->root_access_path() != nullptr) {
          path = subselect->unit->root_access_path();
        } else {
          path = subselect->unit->item->root_access_path();
        }
        Json_object *child_obj = new (std::nothrow) Json_object();
        if (child_obj == nullptr) return true;
        // Populate the subquery-specific json fields.
        bool error = false;
        error |= AddMemberToObject<Json_boolean>(child_obj, "subquery", true);
        error |= AddMemberToObject<Json_string>(child_obj, "subquery_location",
                                                source_text);
        if (query_block->is_dependent())
          error |=
              AddMemberToObject<Json_boolean>(child_obj, "dependent", true);
        if (query_block->is_cacheable())
          error |=
              AddMemberToObject<Json_boolean>(child_obj, "cacheable", true);

        children->push_back({path, description, query_block->join, child_obj});

        return error != 0;
      });
}

static bool GetAccessPathsFromSelectList(JOIN *join,
                                         vector<ExplainChild> *children) {
  if (join == nullptr) {
    return false;
  }

  // Look for any Items in the projection list itself.
  for (Item *item : *join->get_current_fields()) {
    if (GetAccessPathsFromItem(item, "projection", children)) return true;
  }

  // Look for any Items that were materialized into fields during execution.
  for (uint table_idx = join->primary_tables; table_idx < join->tables;
       ++table_idx) {
    QEP_TAB *qep_tab = &join->qep_tab[table_idx];
    if (qep_tab != nullptr && qep_tab->tmp_table_param != nullptr) {
      for (Func_ptr &func : *qep_tab->tmp_table_param->items_to_copy) {
        if (GetAccessPathsFromItem(func.func(), "projection", children))
          return true;
      }
    }
  }
  return false;
}

static std::unique_ptr<Json_object> ExplainMaterializeAccessPath(
    const AccessPath *path, JOIN *join, std::unique_ptr<Json_object> ret_obj,
    vector<ExplainChild> *children, bool explain_analyze) {
  Json_object *obj = ret_obj.get();
  bool error = false;
  MaterializePathParameters *param = path->materialize().param;

  /*
    There may be multiple references to a CTE, but we should only print the
    plan once.
  */
  const bool explain_cte_now = param->cte != nullptr && [&]() {
    if (explain_analyze) {
      /*
        Find the temporary table for which the CTE was materialized, if there
        is one.
      */
      if (path->iterator == nullptr ||
          path->iterator->GetProfiler()->GetNumInitCalls() == 0) {
        // If the CTE was never materialized, print it at the first reference.
        return param->table == param->cte->tmp_tables[0]->table &&
               std::none_of(param->cte->tmp_tables.cbegin(),
                            param->cte->tmp_tables.cend(),
                            [](const Table_ref *tab) {
                              return tab->table->materialized;
                            });
      } else {
        // The CTE was materialized here, print it now with cost data.
        return true;
      }
    } else {
      // If we do not want cost data, print the plan at the first reference.
      return param->table == param->cte->tmp_tables[0]->table;
    }
  }();

  const bool is_set_operation = param->query_blocks.size() > 1;
  string str;
  const bool doing_dedup = MaterializeIsDoingDeduplication(param->table);
  if (param->cte != nullptr) {
    error |= AddMemberToObject<Json_boolean>(obj, "cte", true);
    if (param->cte->recursive) {
      error |= AddMemberToObject<Json_boolean>(obj, "recursive", true);
      str = "Materialize recursive CTE " + to_string(param->cte->name);
    } else {
      if (is_set_operation) {
        str = "Materialize union CTE " + to_string(param->cte->name);
        error |= AddMemberToObject<Json_boolean>(obj, "union", true);
      } else {
        str = "Materialize CTE " + to_string(param->cte->name);
      }
      if (param->cte->tmp_tables.size() > 1) {
        str += " if needed";
        if (!explain_cte_now) {
          // See children().
          str += " (query plan printed elsewhere)";
        }
      }
    }
  } else if (is_set_operation) {
    if (param->table->is_union_or_table()) {
      if (doing_dedup) {
        str = "Union materialize";
      } else {
        str = "Union all materialize";
      }
      error |= AddMemberToObject<Json_boolean>(obj, "union", true);
    } else {
      if (param->table->is_except()) {
        if (param->table->is_distinct()) {
          str = "Except materialize";
        } else {
          str = "Except all materialize";
        }
        error |= AddMemberToObject<Json_boolean>(obj, "except", true);
      } else {
        if (param->table->is_distinct()) {
          str = "Intersect materialize";
        } else {
          str = "Intersect all materialize";
        }
        error |= AddMemberToObject<Json_boolean>(obj, "intersect", true);
      }
    }
  } else if (param->rematerialize) {
    error |= AddMemberToObject<Json_boolean>(obj, "temp_table", true);
    str = "Temporary table";
  } else {
    str = "Materialize";
  }
  const bool union_dedup = param->table->is_union_or_table() && doing_dedup;
  if (union_dedup ||
      (!param->table->is_union_or_table() && param->table->is_distinct())) {
    error |= AddMemberToObject<Json_boolean>(obj, "deduplication", true);
    str += " with deduplication";
  }  // else: do not print deduplication for intersect, except

  if (param->invalidators != nullptr) {
    std::unique_ptr<Json_array> cache_invalidators(new (std::nothrow)
                                                       Json_array());
    if (cache_invalidators == nullptr) return nullptr;
    bool first = true;
    str += " (invalidate on row from ";
    for (const AccessPath *invalidator : *param->invalidators) {
      if (!first) {
        str += "; ";
      }

      first = false;
      str += invalidator->cache_invalidator().name;
      error |= AddElementToArray<Json_string>(
          cache_invalidators, invalidator->cache_invalidator().name);
    }
    str += ")";
    error |=
        obj->add_alias("cache_invalidators", std::move(cache_invalidators));
  }

  error |= AddMemberToObject<Json_string>(obj, "operation", str);

  /* Move the Materialize to the bottom of its table path, and return a new
   * object for this table path.
   */
  ret_obj = AssignParentPath(path->materialize().table_path, path,
                             std::move(ret_obj), join);

  // Children.

  // If a CTE is referenced multiple times, only bother printing its query plan
  // once, instead of repeating it over and over again.
  //
  // TODO(sgunders): Consider printing CTE query plans on the top level of the
  // query block instead?
  if (param->cte != nullptr && !explain_cte_now) {
    return (error ? nullptr : std::move(ret_obj));
  }

  char heading[256] = "";

  if (param->limit_rows != HA_POS_ERROR) {
    // We call this “Limit table size” as opposed to “Limit”, to be able
    // to distinguish between the two in EXPLAIN when debugging.
    if (MaterializeIsDoingDeduplication(param->table)) {
      snprintf(heading, sizeof(heading), "Limit table size: %llu unique row(s)",
               param->limit_rows);
    } else {
      snprintf(heading, sizeof(heading), "Limit table size: %llu row(s)",
               param->limit_rows);
    }
  }

  // We don't list the table iterator as an explicit child; we mark it in
  // our description instead. (Anything else would look confusingly much
  // like a join.)
  for (const MaterializePathParameters::QueryBlock &query_block :
       param->query_blocks) {
    string this_heading = heading;

    if (query_block.disable_deduplication_by_hash_field) {
      if (this_heading.empty()) {
        this_heading = "Disable deduplication";
      } else {
        this_heading += ", disable deduplication";
      }
    }
    if (!param->table->is_union_or_table() &&
        (param->table->is_except() && param->table->is_distinct()) &&
        query_block.m_operand_idx > 0 &&
        (query_block.m_operand_idx < query_block.m_first_distinct)) {
      if (this_heading.empty()) {
        this_heading = "Disable deduplication";
      } else {
        this_heading += ", disable deduplication";
      }
    }

    if (query_block.is_recursive_reference) {
      if (this_heading.empty()) {
        this_heading = "Repeat until convergence";
      } else {
        this_heading += ", repeat until convergence";
      }
    }

    children->push_back(
        {query_block.subquery_path, this_heading, query_block.join});
  }

  return (error ? nullptr : std::move(ret_obj));
}

/**
    AccessPath objects of type TEMPTABLE_AGGREGATE, MATERIALIZE, and
    MATERIALIZE_INFORMATION_SCHEMA_TABLE represent a materialized
    set of rows. These materialized AccessPaths have a another path member
    (called table_path) that iterates over the materialized rows.

    So codewise, table_path is a child of the materialized path, even if it
    is logically the parent, as it consumes the results from the materialized
    path. For that reason, we present table_path above the materialized path in
    'explain' output (@see AddPathCost for details).

    This function therefore sets the JSON object for the materialized
    path to be the leaf descendant of the table_path JSON
    object. (Note that in some cases table_path does not operate
    directly on materialized_path. Instead, table_path is the first in
    a chain of paths where the final path is typically a TABLE_SCAN of
    REF access path that the iterates over the materialized rows.)

    @param table_path the head of the chain of paths that iterates over the
           materialized rows.
    @param materialized_path if (the leaf descendant of) table_path iterates
           over the rows from a MATERIALIZE path, then 'materialized_path'
           is that path. Otherwise it is nullptr.
    @param materialized_obj the JSON object describing the materialized path.
    @param join the JOIN to which 'table_path' belongs.
    @returns the JSON object describing table_path.
*/
static std::unique_ptr<Json_object> AssignParentPath(
    AccessPath *table_path, const AccessPath *materialized_path,
    std::unique_ptr<Json_object> materialized_obj, JOIN *join) {
  // We don't want to include the SELECT subquery list in the parent path;
  // Let them get printed in the actual root node. So is_root_of_join=false.
  std::unique_ptr<Json_object> table_obj = ExplainAccessPath(
      table_path, materialized_path, join, /*is_root_of_join=*/false);
  if (table_obj == nullptr) return nullptr;

  /* Get the bottommost object from the new object tree. */
  Json_object *bottom_obj = table_obj.get();
  while (bottom_obj->get("inputs") != nullptr) {
    Json_dom *children = bottom_obj->get("inputs");
    assert(children->json_type() == enum_json_type::J_ARRAY);
    Json_array *children_array = down_cast<Json_array *>(children);
    bottom_obj = down_cast<Json_object *>((*children_array)[0]);
  }

  /* Place the input object as a child of the bottom-most object */
  std::unique_ptr<Json_array> children(new (std::nothrow) Json_array());
  if (children == nullptr ||
      children->append_alias(std::move(materialized_obj)))
    return nullptr;
  if (bottom_obj->add_alias("inputs", std::move(children))) return nullptr;

  return table_obj;
}

static bool ExplainIndexSkipScanAccessPath(Json_object *obj,
                                           const AccessPath *path,
                                           JOIN *join [[maybe_unused]],
                                           string *description) {
  TABLE *table = path->index_skip_scan().table;
  KEY *key_info = table->key_info + path->index_skip_scan().index;
  string ranges;
  IndexSkipScanParameters *param = path->index_skip_scan().param;

  // Print out any equality ranges.
  bool first = true;
  std::unique_ptr<Json_array> range_arr(new (std::nothrow) Json_array());
  if (range_arr == nullptr) return true;
  for (unsigned key_part_idx = 0; key_part_idx < param->eq_prefix_key_parts;
       ++key_part_idx) {
    if (!first) {
      ranges += ", ";
    }
    first = false;

    string range = param->index_info->key_part[key_part_idx].field->field_name;
    string range_short_text;
    Bounds_checked_array<unsigned char *> prefixes =
        param->eq_prefixes[key_part_idx].eq_key_prefixes;
    if (prefixes.size() == 1) {
      range += " = ";
      String out;
      print_key_value(&out, &param->index_info->key_part[key_part_idx],
                      prefixes[0]);
      range += to_string(out);
    } else {
      range += " IN (";
      for (unsigned i = 0; i < prefixes.size(); ++i) {
        if (i == 2 && prefixes.size() > 3) {
          range_short_text =
              range + StringPrintf(", (%zu more))", prefixes.size() - 2);
        }
        if (i != 0) {
          range += ", ";
        }
        String out;
        print_key_value(&out, &param->index_info->key_part[key_part_idx],
                        prefixes[i]);
        range += to_string(out);
      }
      range += ")";
    }
    if (AddElementToArray<Json_string>(range_arr, range)) return true;
    // For IN clause above, we have made range_short_text; so use that if it's
    // available, rather than the full string stored in 'range'.
    ranges += (range_short_text.empty() ? range : range_short_text);
  }

  // Then the ranges.
  if (!first) {
    ranges += ", ";
  }
  String out;
  append_range(&out, param->range_key_part, param->min_range_key,
               param->max_range_key, param->range_cond_flag);
  ranges += to_string(out);
  if (AddElementToArray<Json_string>(range_arr, to_string(out))) return true;

  // NOTE: Currently, index skip scan is always covering, but there's no
  // good reason why we cannot fix this limitation in the future.
  return SetIndexInfoInObject(
      description, "index_skip_scan", nullptr, table, key_info, "skip scan",
      /*lookup condition*/ "", &ranges, std::move(range_arr), /*reverse*/ false,
      /*push_condition*/ nullptr, obj);
}

static bool ExplainGroupIndexSkipScanAccessPath(Json_object *obj,
                                                const AccessPath *path,
                                                JOIN *join [[maybe_unused]],
                                                string *description) {
  TABLE *table = path->group_index_skip_scan().table;
  KEY *key_info = table->key_info + path->group_index_skip_scan().index;
  GroupIndexSkipScanParameters *param = path->group_index_skip_scan().param;
  string ranges;
  bool error = false;
  std::unique_ptr<Json_array> range_arr(new (std::nothrow) Json_array());
  if (range_arr == nullptr) return true;

  // Print out prefix ranges, if any.
  if (!param->prefix_ranges.empty()) {
    error |= PrintRanges(param->prefix_ranges.data(),
                         param->prefix_ranges.size(), key_info->key_part,
                         /*single_part_only=*/false, range_arr, &ranges);
  }

  // Print out the ranges on the MIN/MAX keypart, if we have them.
  // (We don't print infix ranges, because they seem to be in an unusual
  // format.)
  if (!param->min_max_ranges.empty()) {
    if (!param->prefix_ranges.empty()) {
      ranges += ", ";
    }
    error |= PrintRanges(param->min_max_ranges.data(),
                         param->min_max_ranges.size(), param->min_max_arg_part,
                         /*single_part_only=*/true, range_arr, &ranges);
  }

  // NOTE: Currently, group index skip scan is always covering, but there's no
  // good reason why we cannot fix this limitation in the future.
  error |= SetIndexInfoInObject(
      description, "group_index_skip_scan", nullptr, table, key_info,
      (param->min_max_arg_part ? "skip scan for grouping"
                               : "skip scan for deduplication"),
      /*lookup condition*/ "", (!ranges.empty() ? &ranges : nullptr),
      std::move(range_arr),
      /*reverse*/ false, /*push_condition*/ nullptr, obj);

  return error;
}

static bool AddChildrenFromPushedCondition(const TABLE *table,
                                           vector<ExplainChild> *children) {
  /*
    A table access path is normally a leaf node in the set of paths.
    The exception is if a subquery was included as part of an
    'engine_condition_pushdown'. In such cases the subquery has
    been evaluated prior to accessing this table, and the result(s)
    from the subquery materialized into the pushed condition.
    Report such subqueries as children of this table.
  */
  Item *pushed_cond = const_cast<Item *>(table->file->pushed_cond);

  if (pushed_cond != nullptr) {
    if (GetAccessPathsFromItem(pushed_cond, "pushed condition", children))
      return true;
  }
  return false;
}

/*
   Returns the range through the return value (to be used in TREE format
   synopsis), and also appends the range to the range_array (to be used for
   JSON format field). The only reason the return value cannot be used for JSON
   format is because we truncate it when there are too many ranges; we do
   want to keep the full range for JSON format.
*/
static bool PrintRanges(const QUICK_RANGE *const *ranges, unsigned num_ranges,
                        const KEY_PART_INFO *key_part, bool single_part_only,
                        const std::unique_ptr<Json_array> &range_array,
                        string *ranges_out) {
  string range, shortened_range;
  for (unsigned range_idx = 0; range_idx < num_ranges; ++range_idx) {
    if (range_idx == 2 && num_ranges > 3) {
      char str[256];
      snprintf(str, sizeof(str), " OR (%u more)", num_ranges - 2);
      // Save the shortened version for TREE format.
      shortened_range = range + str;
    }
    if (range_idx > 0) range += " OR ";

    String str;
    if (single_part_only) {
      // key_part is the part we are printing on,
      // and we have to ignore min_keypart_map / max_keypart_map,
      // so we cannot use append_range_to_string().
      append_range(&str, key_part, ranges[range_idx]->min_key,
                   ranges[range_idx]->max_key, ranges[range_idx]->flag);
    } else {
      // NOTE: key_part is the first keypart in the key.
      append_range_to_string(ranges[range_idx], key_part, &str);
    }
    range += "(" + to_string(str) + ")";
  }
  if (AddElementToArray<Json_string>(range_array, range)) return true;
  *ranges_out = (shortened_range.empty() ? range : shortened_range);
  return false;
}

static bool AddChildrenToObject(Json_object *obj,
                                const vector<ExplainChild> &children,
                                JOIN *parent_join, bool parent_is_root_of_join,
                                string alias) {
  if (children.empty()) return false;

  std::unique_ptr<Json_array> children_json(new (std::nothrow) Json_array());
  if (children_json == nullptr) return true;

  for (const ExplainChild &child : children) {
    JOIN *subjoin = child.join != nullptr ? child.join : parent_join;
    bool child_is_root_of_join =
        subjoin != parent_join || parent_is_root_of_join;

    std::unique_ptr<Json_object> child_obj = ExplainAccessPath(
        child.path, nullptr, subjoin, child_is_root_of_join, child.obj);
    if (child_obj == nullptr) return true;
    if (!child.description.empty()) {
      if (AddMemberToObject<Json_string>(child_obj.get(), "heading",
                                         child.description))
        return true;
    }
    if (children_json->append_alias(std::move(child_obj))) return true;
  }

  return obj->add_alias(alias, std::move(children_json));
}

static std::unique_ptr<Json_object> ExplainQueryPlan(
    const AccessPath *path, THD::Query_plan const *query_plan, JOIN *join,
    bool is_root_of_join) {
  string dml_desc;
  std::unique_ptr<Json_object> obj = nullptr;

  /* Create a Json object for the SELECT path */
  if (path != nullptr) {
    obj = ExplainAccessPath(path, nullptr, join, is_root_of_join);
    if (obj == nullptr) return nullptr;
  }
  if (query_plan != nullptr) {
    switch (query_plan->get_command()) {
      case SQLCOM_INSERT_SELECT:
      case SQLCOM_INSERT:
        dml_desc = string("Insert into ") +
                   query_plan->get_lex()->insert_table_leaf->table->alias;
        break;
      case SQLCOM_REPLACE_SELECT:
      case SQLCOM_REPLACE:
        dml_desc = string("Replace into ") +
                   query_plan->get_lex()->insert_table_leaf->table->alias;
        break;
      default:
        // SELECTs have no top-level node.
        break;
    }
  }

  /* If there is a DML node, add it on top of the SELECT plan */
  if (!dml_desc.empty()) {
    std::unique_ptr<Json_object> dml_obj(new (std::nothrow) Json_object());
    if (dml_obj == nullptr) return nullptr;
    if (AddMemberToObject<Json_string>(dml_obj.get(), "operation", dml_desc))
      return nullptr;

    /* There might not be a select plan. E.g. INSERT ... VALUES() */
    if (obj != nullptr) {
      std::unique_ptr<Json_array> children(new (std::nothrow) Json_array());
      if (children == nullptr || children->append_alias(std::move(obj)))
        return nullptr;
      if (dml_obj->add_alias("inputs", std::move(children))) return nullptr;
    }
    obj = std::move(dml_obj);
  }

  return obj;
}

/** Append the various costs.
    @param path the path that we add costs for.
    @param materialized_path the MATERIALIZE path for which 'path' is the
           table_path, or nullptr 'path' is not a table_path.
    @param obj the JSON object describing 'path'.
    @param explain_analyze true if we run an 'eaxplain analyze' command.
    @returns true iff there was an error.
*/
static bool AddPathCosts(const AccessPath *path,
                         const AccessPath *materialized_path, Json_object *obj,
                         bool explain_analyze) {
  const AccessPath *const table_path = path->type == AccessPath::MATERIALIZE
                                           ? path->materialize().table_path
                                           : nullptr;

  double cost;

  /*
    A MATERIALIZE AccessPath has a child path (called table_path)
    that iterates over the materialized rows.
    So codewise, table_path is a child of materialized_path, even if it is
    logically the parent, as it consumes the results from materialized_path.
    For that reason, we present table_path above materialized_path in
    'explain' output, e.g.:

    .-> Sort: i  (cost=8.45..8.45 rows=10)
    .    -> Table scan on <union temporary>  (cost=1.76..4.12 rows=10)
    .        -> Union materialize with deduplication  (cost=1.50..1.50 rows=10)
    .            -> Table scan on t1  (cost=0.05..0.25 rows=5)
    .            -> Table scan on t2  (cost=0.05..0.25 rows=5)

    The cost of an access path includes the cost all of its descendants.
    Since table_path is codewise a child of materialized_path, this means that:

    - The cost of table_path is the cost of accessing the materialized
      structure plus the cost of the descendants (inputs) of materialized_path.

    - The cost of materialized_path is the cost of materialization plus
      the cost of table_path.

    When we wish to display table_path as the parent of materialized_path,
    we need to compensate for this:

    - For table_path, we show the cost of materialized_path, as this includes
      the cost of materialization, iteration and the descendants.

    - For the MATERIALIZE AccessPath we show the cost of the descendants plus
      the cost of materialization.
  */
  if (materialized_path == nullptr) {
    if (table_path == nullptr) {
      cost = std::max(0.0, path->cost);
    } else {
      assert(path->materialize().subquery_cost >= 0.0);
      cost = path->materialize().subquery_cost +
             kMaterializeOneRowCost * path->num_output_rows();
    }
  } else {
    assert(materialized_path->cost >= 0.0);
    cost = materialized_path->cost;
  }

  bool error = false;

  if (path->num_output_rows() >= 0.0) {
    // Calculate first row cost
    double init_cost;
    if (materialized_path == nullptr) {
      if (table_path == nullptr) {
        init_cost = path->init_cost;
      } else {
        init_cost = cost;
      }
    } else {
      init_cost = materialized_path->init_cost;
    }

    if (init_cost >= 0.0) {
      double first_row_cost;
      if (path->num_output_rows() <= 1.0) {
        first_row_cost = cost;
      } else {
        first_row_cost =
            init_cost + (cost - init_cost) / path->num_output_rows();
      }
      error |= AddMemberToObject<Json_double>(obj, "estimated_first_row_cost",
                                              first_row_cost);
    }
    error |= AddMemberToObject<Json_double>(obj, "estimated_total_cost", cost);
    error |= AddMemberToObject<Json_double>(obj, "estimated_rows",
                                            path->num_output_rows());
  } /* if (path->num_output_rows() >= 0.0) */

  /* Add analyze figures */
  if (explain_analyze) {
    int num_init_calls = 0;

    if (path->iterator != nullptr) {
      const IteratorProfiler *const profiler = path->iterator->GetProfiler();
      if ((num_init_calls = profiler->GetNumInitCalls()) != 0) {
        error |= AddMemberToObject<Json_double>(
            obj, "actual_first_row_ms",
            profiler->GetFirstRowMs() / profiler->GetNumInitCalls());
        error |= AddMemberToObject<Json_double>(
            obj, "actual_last_row_ms",
            profiler->GetLastRowMs() / profiler->GetNumInitCalls());
        error |= AddMemberToObject<Json_double>(
            obj, "actual_rows",
            static_cast<double>(profiler->GetNumRows()) / num_init_calls);
        error |=
            AddMemberToObject<Json_int>(obj, "actual_loops", num_init_calls);
      }
    }

    if (num_init_calls == 0) {
      error |= AddMemberToObject<Json_null>(obj, "actual_first_row_ms");
      error |= AddMemberToObject<Json_null>(obj, "actual_last_row_ms");
      error |= AddMemberToObject<Json_null>(obj, "actual_rows");
      error |= AddMemberToObject<Json_null>(obj, "actual_loops");
    }
  }
  return error;
}

/**
   Given a json object, update it's appropriate json fields according to the
   input path. Also update the 'children' with a flat list of direct children
   of the passed object.  In most of cases, the returned object is same as the
   input object, but for some paths it can be different. So callers should use
   the returned object.

   Note: This function has shown to consume excessive stack space, particularly
   in debug builds. Hence make sure this function does not directly or
   indirectly create any json children objects recursively. It may cause stack
   overflow. Hence json children are created only after this function returns
   in function ExplainAccessPath().

   @param ret_obj The JSON object describing 'path'.
   @param path the path to describe.
   @param materialized_path if 'path' is the table_path of a MATERIALIZE path,
          then materialized_path is that path. Otherwise it is nullptr.
   @param join the JOIN to which 'path' belongs.
   @param children the paths that are the children of the path that the
          returned JSON object represents (i.e. the next paths to be explained).
   @returns either ret_obj or a new JSON object with ret_obj as a descendant.
*/
static std::unique_ptr<Json_object> SetObjectMembers(
    std::unique_ptr<Json_object> ret_obj, const AccessPath *path,
    const AccessPath *materialized_path, JOIN *join,
    vector<ExplainChild> *children) {
  bool error = false;
  string description;

  // The obj to be returned might get changed when processing some of the
  // paths. So keep a handle to the original object, in case we later add any
  // more fields.
  Json_object *obj = ret_obj.get();

  /* Get path-specific info, including the description string */
  switch (path->type) {
    case AccessPath::TABLE_SCAN: {
      TABLE *table = path->table_scan().table;
      description += string("Table scan on ") + table->alias;
      if (table->s->is_secondary_engine()) {
        error |= AddMemberToObject<Json_string>(obj, "secondary_engine",
                                                table->file->table_type());
        description +=
            string(" in secondary engine ") + table->file->table_type();
      }
      description += table->file->explain_extra();

      error |= AddMemberToObject<Json_string>(obj, "table_name", table->alias);
      error |= AddMemberToObject<Json_string>(obj, "access_type", "table");
      if (!table->file->explain_extra().empty())
        error |= AddMemberToObject<Json_string>(obj, "message",
                                                table->file->explain_extra());
      error |= AddChildrenFromPushedCondition(table, children);
      break;
    }
    case AccessPath::INDEX_SCAN: {
      TABLE *table = path->index_scan().table;
      assert(table->file->pushed_idx_cond == nullptr);

      const KEY *key = &table->key_info[path->index_scan().idx];
      error |= SetIndexInfoInObject(&description, "index_scan", nullptr, table,
                                    key, "scan",
                                    /*lookup condition*/ "", /*range*/ nullptr,
                                    nullptr, path->index_scan().reverse,
                                    /*push_condition*/ nullptr, obj);
      error |= AddChildrenFromPushedCondition(table, children);
      break;
    }
    case AccessPath::REF: {
      TABLE *table = path->ref().table;
      const KEY *key = &table->key_info[path->ref().ref->key];
      error |= SetIndexInfoInObject(
          &description, "index_lookup", nullptr, table, key, "lookup",
          RefToString(*path->ref().ref, key, /*include_nulls=*/false),
          /*ranges=*/nullptr, nullptr, path->ref().reverse,
          table->file->pushed_idx_cond, obj);
      error |= AddChildrenFromPushedCondition(table, children);
      break;
    }
    case AccessPath::REF_OR_NULL: {
      TABLE *table = path->ref_or_null().table;
      const KEY *key = &table->key_info[path->ref_or_null().ref->key];
      error |= SetIndexInfoInObject(
          &description, "index_lookup", nullptr, table, key, "lookup",
          RefToString(*path->ref_or_null().ref, key, /*include_nulls=*/true),
          /*ranges=*/nullptr, nullptr, false, table->file->pushed_idx_cond,
          obj);
      error |= AddChildrenFromPushedCondition(table, children);
      break;
    }
    case AccessPath::EQ_REF: {
      TABLE *table = path->eq_ref().table;
      const KEY *key = &table->key_info[path->eq_ref().ref->key];
      error |= SetIndexInfoInObject(
          &description, "index_lookup", "Single-row", table, key, "lookup",
          RefToString(*path->eq_ref().ref, key, /*include_nulls=*/false),
          /*ranges=*/nullptr, nullptr, false, table->file->pushed_idx_cond,
          obj);
      error |= AddChildrenFromPushedCondition(table, children);
      break;
    }
    case AccessPath::PUSHED_JOIN_REF: {
      TABLE *table = path->pushed_join_ref().table;
      assert(table->file->pushed_idx_cond == nullptr);
      const KEY *key = &table->key_info[path->pushed_join_ref().ref->key];
      error |= SetIndexInfoInObject(
          &description, "pushed_join_ref",
          path->pushed_join_ref().is_unique ? "Single-row" : nullptr, table,
          key, "lookup",
          RefToString(*path->pushed_join_ref().ref, key,
                      /*include_nulls=*/false),
          /*ranges=*/nullptr, nullptr,
          /*reverse=*/false, nullptr, obj);
      break;
    }
    case AccessPath::FULL_TEXT_SEARCH: {
      TABLE *table = path->full_text_search().table;
      assert(table->file->pushed_idx_cond == nullptr);
      const KEY *key = &table->key_info[path->full_text_search().ref->key];
      error |= SetIndexInfoInObject(
          &description, "full_text_search", "Full-text", table, key, "search",
          RefToString(*path->full_text_search().ref, key,
                      /*include_nulls=*/false),
          /*ranges=*/nullptr, nullptr,
          /*reverse=*/false, nullptr, obj);
      break;
    }
    case AccessPath::CONST_TABLE: {
      TABLE *table = path->const_table().table;
      assert(table->file->pushed_idx_cond == nullptr);
      assert(table->file->pushed_cond == nullptr);
      description = string("Constant row from ") + table->alias;
      error |=
          AddMemberToObject<Json_string>(obj, "access_type", "constant_row");
      error |= AddMemberToObject<Json_string>(obj, "table_name", table->alias);
      break;
    }
    case AccessPath::MRR: {
      TABLE *table = path->mrr().table;
      const KEY *key = &table->key_info[path->mrr().ref->key];
      error |= SetIndexInfoInObject(
          &description, "multi_range_read", "Multi-range", table, key, "lookup",
          RefToString(*path->mrr().ref, key, /*include_nulls=*/false),
          /*ranges=*/nullptr, nullptr, false, table->file->pushed_idx_cond,
          obj);
      error |= AddChildrenFromPushedCondition(table, children);
      break;
    }
    case AccessPath::FOLLOW_TAIL:
      description =
          string("Scan new records on ") + path->follow_tail().table->alias;
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "scan_new_records");
      error |= AddMemberToObject<Json_string>(obj, "table_name",
                                              path->follow_tail().table->alias);
      error |=
          AddChildrenFromPushedCondition(path->follow_tail().table, children);
      break;
    case AccessPath::INDEX_RANGE_SCAN: {
      const auto &param = path->index_range_scan();
      TABLE *table = param.used_key_part[0].field->table;
      KEY *key_info = table->key_info + param.index;

      std::unique_ptr<Json_array> range_arr(new (std::nothrow) Json_array());
      if (range_arr == nullptr) return nullptr;
      string ranges;
      error |= PrintRanges(param.ranges, param.num_ranges, key_info->key_part,
                           /*single_part_only=*/false, range_arr, &ranges);
      error |= SetIndexInfoInObject(
          &description, "index_range_scan", nullptr, table, key_info,
          "range scan", /*lookup condition*/ "", &ranges, std::move(range_arr),
          path->index_range_scan().reverse, table->file->pushed_idx_cond, obj);

      error |= AddChildrenFromPushedCondition(table, children);
      break;
    }
    case AccessPath::INDEX_MERGE: {
      const auto &param = path->index_merge();
      error |=
          AddMemberToObject<Json_string>(obj, "access_type", "index_merge");
      description = "Sort-deduplicate by row ID";
      for (AccessPath *child : *path->index_merge().children) {
        if (param.allow_clustered_primary_key_scan &&
            param.table->file->primary_key_is_clustered() &&
            child->index_range_scan().index == param.table->s->primary_key) {
          children->push_back(
              {child, "Clustered primary key (scanned separately)"});
        } else {
          children->push_back({child});
        }
      }
      break;
    }
    case AccessPath::ROWID_INTERSECTION: {
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "rowid_intersection");
      description = "Intersect rows sorted by row ID";
      for (AccessPath *child : *path->rowid_intersection().children) {
        children->push_back({child});
      }
      break;
    }
    case AccessPath::ROWID_UNION: {
      error |=
          AddMemberToObject<Json_string>(obj, "access_type", "rowid_union");
      description = "Deduplicate rows sorted by row ID";
      for (AccessPath *child : *path->rowid_union().children) {
        children->push_back({child});
      }
      break;
    }
    case AccessPath::INDEX_SKIP_SCAN: {
      error |= ExplainIndexSkipScanAccessPath(obj, path, join, &description);
      break;
    }
    case AccessPath::GROUP_INDEX_SKIP_SCAN: {
      error |=
          ExplainGroupIndexSkipScanAccessPath(obj, path, join, &description);
      break;
    }
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN: {
      TABLE *table = path->dynamic_index_range_scan().table;
      description += string(table->key_read ? "Covering index range scan on "
                                            : "Index range scan on ") +
                     table->alias + " (re-planned for each iteration)";
      if (table->file->pushed_idx_cond != nullptr) {
        description += ", with index condition: " +
                       ItemToString(table->file->pushed_idx_cond);
      }
      description += table->file->explain_extra();
      error |= AddMemberToObject<Json_string>(obj, "access_type", "index");
      error |= AddMemberToObject<Json_string>(obj, "index_access_type",
                                              "dynamic_index_range_scan");
      error |=
          AddMemberToObject<Json_boolean>(obj, "covering", table->key_read);
      error |= AddMemberToObject<Json_string>(obj, "table_name", table->alias);
      if (table->file->pushed_idx_cond)
        error |= AddMemberToObject<Json_string>(
            obj, "pushed_index_condition",
            ItemToString(table->file->pushed_idx_cond));
      if (!table->file->explain_extra().empty())
        error |= AddMemberToObject<Json_string>(obj, "message",
                                                table->file->explain_extra());
      error |= AddChildrenFromPushedCondition(table, children);
      break;
    }
    case AccessPath::TABLE_VALUE_CONSTRUCTOR:
    case AccessPath::FAKE_SINGLE_ROW:
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "rows_fetched_before_execution");
      description = "Rows fetched before execution";
      break;
    case AccessPath::ZERO_ROWS:
      error |= AddMemberToObject<Json_string>(obj, "access_type", "zero_rows");
      error |= AddMemberToObject<Json_string>(obj, "zero_rows_cause",
                                              path->zero_rows().cause);
      description = string("Zero rows (") + path->zero_rows().cause + ")";
      // The child is not printed as part of the iterator tree.
      break;
    case AccessPath::ZERO_ROWS_AGGREGATED:
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "zero_rows_aggregated");
      error |= AddMemberToObject<Json_string>(
          obj, "zero_rows_cause", path->zero_rows_aggregated().cause);
      description = string("Zero input rows (") +
                    path->zero_rows_aggregated().cause +
                    "), aggregated into one output row";
      break;
    case AccessPath::MATERIALIZED_TABLE_FUNCTION:
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "materialized_table_function");
      description = "Materialize table function";
      break;
    case AccessPath::UNQUALIFIED_COUNT:
      error |= AddMemberToObject<Json_string>(obj, "access_type", "count_rows");
      error |= AddMemberToObject<Json_string>(obj, "table_name",
                                              join->qep_tab->table()->alias);
      description = "Count rows in " + string(join->qep_tab->table()->alias);
      break;
    case AccessPath::NESTED_LOOP_JOIN: {
      string join_type = JoinTypeToString(path->nested_loop_join().join_type);
      error |= AddMemberToObject<Json_string>(obj, "access_type", "join");
      error |= AddMemberToObject<Json_string>(obj, "join_type", join_type);
      error |=
          AddMemberToObject<Json_string>(obj, "join_algorithm", "nested_loop");
      description = "Nested loop " + join_type;
      children->push_back({path->nested_loop_join().outer});
      children->push_back({path->nested_loop_join().inner});
      break;
    }
    case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL:
      // No json fields since this path is not supported in hypergraph
      description =
          string("Nested loop semijoin with duplicate removal on ") +
          path->nested_loop_semijoin_with_duplicate_removal().key->name;
      children->push_back(
          {path->nested_loop_semijoin_with_duplicate_removal().outer});
      children->push_back(
          {path->nested_loop_semijoin_with_duplicate_removal().inner});
      break;
    case AccessPath::BKA_JOIN: {
      string join_type = JoinTypeToString(path->bka_join().join_type);
      error |= AddMemberToObject<Json_string>(obj, "access_type", "join");
      error |= AddMemberToObject<Json_string>(obj, "join_type", join_type);
      error |= AddMemberToObject<Json_string>(obj, "join_algorithm",
                                              "batch_key_access");
      description = "Batched key access " + join_type;
      children->push_back({path->bka_join().outer, "Batch input rows"});
      children->push_back({path->bka_join().inner});
      break;
    }
    case AccessPath::HASH_JOIN: {
      const JoinPredicate *predicate = path->hash_join().join_predicate;
      RelationalExpression::Type type = path->hash_join().rewrite_semi_to_inner
                                            ? RelationalExpression::INNER_JOIN
                                            : predicate->expr->type;

      string json_join_type;
      description = HashJoinTypeToString(type, &json_join_type);

      std::unique_ptr<Json_array> hash_condition(new (std::nothrow)
                                                     Json_array());
      if (hash_condition == nullptr) return nullptr;

      if (predicate->expr->equijoin_conditions.empty()) {
        description.append(" (no condition)");
      } else {
        for (Item_eq_base *cond : predicate->expr->equijoin_conditions) {
          if (cond != predicate->expr->equijoin_conditions[0]) {
            description.push_back(',');
          }
          string condition_str;
          HashJoinCondition hj_cond(cond, *THR_MALLOC);
          if (!hj_cond.store_full_sort_key()) {
            condition_str =
                "(<hash>(" + ItemToString(hj_cond.left_extractor()) +
                ")=<hash>(" + ItemToString(hj_cond.right_extractor()) + "))";
          } else {
            condition_str = ItemToString(cond);
          }
          error |=
              AddElementToArray<Json_string>(hash_condition, condition_str);
          description.append(" " + condition_str);
        }
      }
      error |= obj->add_alias("hash_condition", std::move(hash_condition));

      std::unique_ptr<Json_array> extra_condition(new (std::nothrow)
                                                      Json_array());
      if (extra_condition == nullptr) return nullptr;
      for (Item *cond : predicate->expr->join_conditions) {
        if (cond == predicate->expr->join_conditions[0]) {
          description.append(", extra conditions: ");
        } else {
          description += " and ";
        }
        string condition_str = ItemToString(cond);
        description += condition_str;
        error |= AddElementToArray<Json_string>(extra_condition, condition_str);
      }
      if (extra_condition->size() > 0)
        error |= obj->add_alias("extra_condition", std::move(extra_condition));

      error |= AddMemberToObject<Json_string>(obj, "access_type", "join");
      error |= AddMemberToObject<Json_string>(obj, "join_type", json_join_type);
      error |= AddMemberToObject<Json_string>(obj, "join_algorithm", "hash");
      children->push_back({path->hash_join().outer});
      children->push_back({path->hash_join().inner, "Hash"});
      break;
    }
    case AccessPath::FILTER: {
      error |= AddMemberToObject<Json_string>(obj, "access_type", "filter");
      string filter = ItemToString(path->filter().condition);
      error |= AddMemberToObject<Json_string>(obj, "condition", filter);
      description = "Filter: " + filter;
      children->push_back({path->filter().child});
      GetAccessPathsFromItem(path->filter().condition, "condition", children);
      break;
    }
    case AccessPath::SORT: {
      error |= AddMemberToObject<Json_string>(obj, "access_type", "sort");
      if (path->sort().force_sort_rowids) {
        description = "Sort row IDs";
        error |= AddMemberToObject<Json_boolean>(obj, "row_ids", true);
      } else {
        description = "Sort";
      }
      if (path->sort().remove_duplicates) {
        description += " with duplicate removal: ";
        error |=
            AddMemberToObject<Json_boolean>(obj, "duplicate_removal", true);
      } else {
        description += ": ";
      }

      std::unique_ptr<Json_array> sort_fields(new (std::nothrow) Json_array());
      if (sort_fields == nullptr) return nullptr;
      for (ORDER *order = path->sort().order; order != nullptr;
           order = order->next) {
        if (order != path->sort().order) {
          description += ", ";
        }

        // We usually want to print the item_name if it's set, so that we get
        // the alias instead of the full expression when there is an alias. If
        // it is a field reference, we prefer ItemToString() because item_name
        // in Item_field doesn't include the table name.
        string sort_field;
        if (const Item *item = *order->item;
            item->item_name.is_set() && item->type() != Item::FIELD_ITEM) {
          sort_field = item->item_name.ptr();
        } else {
          sort_field = ItemToString(item);
        }
        if (order->direction == ORDER_DESC) {
          sort_field += " DESC";
        }
        description += sort_field;
        error |= AddElementToArray<Json_string>(sort_fields, sort_field);
      }
      error |= obj->add_alias("sort_fields", std::move(sort_fields));

      if (const ha_rows limit = path->sort().limit; limit != HA_POS_ERROR) {
        char buf[256];
        error |= AddMemberToObject<Json_int>(obj, "per_chunk_limit", limit);
        snprintf(buf, sizeof(buf), ", limit input to %llu row(s) per chunk",
                 limit);
        description += buf;
      }
      children->push_back({path->sort().child});
      break;
    }
    case AccessPath::AGGREGATE: {
      string ret;
      error |= AddMemberToObject<Json_string>(obj, "access_type", "aggregate");
      if (join->grouped || join->group_optimized_away) {
        error |= AddMemberToObject<Json_boolean>(obj, "group_by", true);
        if (*join->sum_funcs == nullptr) {
          description = "Group (no aggregates)";
        } else if (path->aggregate().rollup) {
          error |= AddMemberToObject<Json_boolean>(obj, "rollup", true);
          description = "Group aggregate with rollup: ";
        } else {
          description = "Group aggregate: ";
        }
      } else {
        description = "Aggregate: ";
      }

      std::unique_ptr<Json_array> funcs(new (std::nothrow) Json_array());
      if (funcs == nullptr) return nullptr;
      bool first = true;
      for (Item_sum **item = join->sum_funcs; *item != nullptr; ++item) {
        if (first) {
          first = false;
        } else {
          description += ", ";
        }
        string func =
            (path->aggregate().rollup ? ItemToString((*item)->unwrap_sum())
                                      : ItemToString(*item));
        description += func;
        error |= AddElementToArray<Json_string>(funcs, func);
      }

      // If there are no aggs, still let this field print a "" rather than
      // omit this field.
      error |= obj->add_alias("functions", std::move(funcs));

      children->push_back({path->aggregate().child});
      break;
    }
    case AccessPath::TEMPTABLE_AGGREGATE: {
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "temp_table_aggregate");
      ret_obj = AssignParentPath(path->temptable_aggregate().table_path,
                                 nullptr, std::move(ret_obj), join);
      if (ret_obj == nullptr) return nullptr;
      description = "Aggregate using temporary table";
      children->push_back({path->temptable_aggregate().subquery_path});
      break;
    }
    case AccessPath::LIMIT_OFFSET: {
      error |= AddMemberToObject<Json_string>(obj, "access_type", "limit");
      char buf[256];
      if (path->limit_offset().offset == 0) {
        snprintf(buf, sizeof(buf), "Limit: %llu row(s)",
                 path->limit_offset().limit);
      } else if (path->limit_offset().limit == HA_POS_ERROR) {
        snprintf(buf, sizeof(buf), "Offset: %llu row(s)",
                 path->limit_offset().offset);
      } else {
        snprintf(buf, sizeof(buf), "Limit/Offset: %llu/%llu row(s)",
                 path->limit_offset().limit - path->limit_offset().offset,
                 path->limit_offset().offset);
      }
      error |=
          AddMemberToObject<Json_int>(obj, "limit", path->limit_offset().limit);
      error |= AddMemberToObject<Json_int>(obj, "limit_offset",
                                           path->limit_offset().offset);
      if (path->limit_offset().count_all_rows) {
        error |= AddMemberToObject<Json_boolean>(obj, "count_all_rows", true);
        description =
            string(buf) + " (no early end due to SQL_CALC_FOUND_ROWS)";
      } else {
        description = buf;
      }
      children->push_back({path->limit_offset().child});
      break;
    }
    case AccessPath::STREAM:
      error |= AddMemberToObject<Json_string>(obj, "access_type", "stream");
      description = "Stream results";
      children->push_back({path->stream().child});
      break;
    case AccessPath::MATERIALIZE:
      error |=
          AddMemberToObject<Json_string>(obj, "access_type", "materialize");
      ret_obj =
          ExplainMaterializeAccessPath(path, join, std::move(ret_obj), children,
                                       current_thd->lex->is_explain_analyze);
      if (ret_obj == nullptr) return nullptr;
      break;
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE: {
      ret_obj = AssignParentPath(
          path->materialize_information_schema_table().table_path, nullptr,
          std::move(ret_obj), join);
      if (ret_obj == nullptr) return nullptr;
      const char *table =
          path->materialize_information_schema_table().table_list->table->alias;
      error |= AddMemberToObject<Json_string>(obj, "table_name", table);
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "materialize_information_schema");
      description = "Fill information schema table " + string(table);
      break;
    }
    case AccessPath::APPEND:
      error |= AddMemberToObject<Json_string>(obj, "access_type", "append");
      description = "Append";
      for (const AppendPathParameters &child : *path->append().children) {
        children->push_back({child.path, "", child.join});
      }
      break;
    case AccessPath::WINDOW: {
      Window *const window = path->window().window;
      if (path->window().needs_buffering) {
        error |= AddMemberToObject<Json_boolean>(obj, "buffering", true);
        if (window->optimizable_row_aggregates() ||
            window->optimizable_range_aggregates() ||
            window->static_aggregates()) {
          description = "Window aggregate with buffering: ";
        } else {
          error |= AddMemberToObject<Json_boolean>(obj, "multi_pass", true);
          description = "Window multi-pass aggregate with buffering: ";
        }
      } else {
        description = "Window aggregate: ";
      }

      std::unique_ptr<Json_array> funcs(new (std::nothrow) Json_array());
      if (funcs == nullptr) return nullptr;
      bool first = true;
      for (const Item_sum &func : window->functions()) {
        if (!first) {
          description += ", ";
        }
        string func_str = ItemToString(&func);
        description += func_str;
        error |= AddElementToArray<Json_string>(funcs, func_str);
        first = false;
      }
      error |= obj->add_alias("functions", std::move(funcs));
      error |= AddMemberToObject<Json_string>(obj, "access_type", "window");
      children->push_back({path->window().child});
      break;
    }
    case AccessPath::WEEDOUT: {
      SJ_TMP_TABLE *sj = path->weedout().weedout_table;
      std::unique_ptr<Json_array> tables(new (std::nothrow) Json_array());
      if (tables == nullptr) return nullptr;

      description = "Remove duplicate ";
      if (sj->tabs_end == sj->tabs + 1) {  // Only one table.
        description += sj->tabs->qep_tab->table()->alias;
        error |= AddElementToArray<Json_string>(
            tables, sj->tabs->qep_tab->table()->alias);
      } else {
        description += "(";
        for (SJ_TMP_TABLE_TAB *tab = sj->tabs; tab != sj->tabs_end; ++tab) {
          if (tab != sj->tabs) {
            description += ", ";
          }
          description += tab->qep_tab->table()->alias;
          error |= AddElementToArray<Json_string>(tables,
                                                  tab->qep_tab->table()->alias);
        }
        description += ")";
      }
      description += " rows using temporary table (weedout)";
      error |= obj->add_alias("tables", std::move(tables));
      error |= AddMemberToObject<Json_string>(obj, "access_type", "weedout");
      children->push_back({path->weedout().child});
      break;
    }
    case AccessPath::REMOVE_DUPLICATES: {
      description = "Remove duplicates from input grouped on ";
      std::unique_ptr<Json_array> group_items(new (std::nothrow) Json_array());
      if (group_items == nullptr) return nullptr;
      for (int i = 0; i < path->remove_duplicates().group_items_size; ++i) {
        string group_item =
            ItemToString(path->remove_duplicates().group_items[i]);
        if (i != 0) {
          description += ", ";
        }
        description += group_item;
        error |= AddElementToArray<Json_string>(group_items, group_item);
      }
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "remove_duplicates_from_groups");
      error |= obj->add_alias("group_items", std::move(group_items));
      children->push_back({path->remove_duplicates().child});
      break;
    }
    case AccessPath::REMOVE_DUPLICATES_ON_INDEX: {
      const char *keyname = path->remove_duplicates_on_index().key->name;
      description = string("Remove duplicates from input sorted on ") + keyname;
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "remove_duplicates_on_index");
      error |= AddMemberToObject<Json_string>(obj, "index_name", keyname);
      children->push_back({path->remove_duplicates_on_index().child});
      break;
    }
    case AccessPath::ALTERNATIVE: {
      const TABLE *table =
          path->alternative().table_scan_path->table_scan().table;
      const Index_lookup *ref = path->alternative().used_ref;
      const KEY *key = &table->key_info[ref->key];

      int num_applicable_cond_guards = 0;
      for (unsigned key_part_idx = 0; key_part_idx < ref->key_parts;
           ++key_part_idx) {
        if (ref->cond_guards[key_part_idx] != nullptr) {
          ++num_applicable_cond_guards;
        }
      }

      description = "Alternative plans for IN subquery: Index lookup unless ";
      if (num_applicable_cond_guards > 1) {
        description += " any of (";
      }
      bool first = true;
      for (unsigned key_part_idx = 0; key_part_idx < ref->key_parts;
           ++key_part_idx) {
        if (ref->cond_guards[key_part_idx] != nullptr) {
          if (!first) {
            description += ", ";
          }
          first = false;
          description += key->key_part[key_part_idx].field->field_name;
        }
      }
      if (num_applicable_cond_guards > 1) {
        description += ")";
      }
      description += " IS NULL";
      error |= AddMemberToObject<Json_string>(
          obj, "access_type", "alternative_plans_for_in_subquery");
      children->push_back({path->alternative().child});
      children->push_back({path->alternative().table_scan_path});
      break;
    }
    case AccessPath::CACHE_INVALIDATOR:
      description = string("Invalidate materialized tables (row from ") +
                    path->cache_invalidator().name + ")";
      error |= AddMemberToObject<Json_string>(obj, "access_type",
                                              "invalidate_materialized_tables");
      error |= AddMemberToObject<Json_string>(obj, "table_name",
                                              path->cache_invalidator().name);
      children->push_back({path->cache_invalidator().child});
      break;
    case AccessPath::DELETE_ROWS: {
      error |=
          AddMemberToObject<Json_string>(obj, "access_type", "delete_rows");
      string tables;
      for (Table_ref *t = join->query_block->leaf_tables; t != nullptr;
           t = t->next_leaf) {
        if (Overlaps(t->map(), path->delete_rows().tables_to_delete_from)) {
          if (!tables.empty()) {
            tables.append(", ");
          }
          tables.append(t->alias);
          if (Overlaps(t->map(), path->delete_rows().immediate_tables)) {
            tables.append(" (immediate)");
          } else {
            tables.append(" (buffered)");
          }
        }
      }
      error |= AddMemberToObject<Json_string>(obj, "tables", tables);
      description = string("Delete from ") + tables;
      children->push_back({path->delete_rows().child});
      break;
    }
    case AccessPath::UPDATE_ROWS: {
      string tables;
      for (Table_ref *t = join->query_block->leaf_tables; t != nullptr;
           t = t->next_leaf) {
        if (Overlaps(t->map(), path->update_rows().tables_to_update)) {
          if (!tables.empty()) {
            tables.append(", ");
          }
          tables.append(t->alias);
          if (Overlaps(t->map(), path->update_rows().immediate_tables)) {
            tables.append(" (immediate)");
          } else {
            tables.append(" (buffered)");
          }
        }
      }
      description = string("Update ") + tables;
      children->push_back({path->update_rows().child});
      break;
    }
  }

  // Append the various costs.
  error |= AddPathCosts(path, materialized_path, obj,
                        current_thd->lex->is_explain_analyze);

  // Empty description means the object already has the description set above.
  if (!description.empty()) {
    // Create JSON objects for description strings.
    error |= AddMemberToObject<Json_string>(obj, "operation", description);
  }

  return (error ? nullptr : std::move(ret_obj));
}

/**
   Convert the AccessPath into a Json object that represents the EXPLAIN output
   This Json object may in turn be used to output in whichever required format.

   @param path the path to describe.
   @param materialized_path if 'path' is the table_path of a MATERIALIZE path,
          then materialized_path is that path. Otherwise it is nullptr.
   @param join the JOIN to which 'path' belongs.
   @param is_root_of_join 'true' if 'path' is the root path of a
          Query_expression that is not a union.
   @param input_obj The JSON object describing 'path', or nullptr if a new
          object should be allocated..
   @returns the root of the tree of JSON objects generated from 'path'.
          (In most cases a single object.)
*/
static std::unique_ptr<Json_object> ExplainAccessPath(
    const AccessPath *path, const AccessPath *materialized_path, JOIN *join,
    bool is_root_of_join, Json_object *input_obj) {
  bool error = false;
  vector<ExplainChild> children;
  Json_object *obj;
  std::unique_ptr<Json_object> ret_obj(input_obj);

  if (ret_obj == nullptr) {
    ret_obj = create_dom_ptr<Json_object>();
  }
  // Keep a handle to the original object.
  obj = ret_obj.get();

  // This should not happen, but some unit tests have shown to cause null child
  // paths to be present in the AccessPath tree.
  if (path == nullptr) {
    if (AddMemberToObject<Json_string>(obj, "operation",
                                       "<not executable by iterator executor>"))
      return nullptr;
    return ret_obj;
  }

  if ((ret_obj = SetObjectMembers(std::move(ret_obj), path, materialized_path,
                                  join, &children)) == nullptr)
    return nullptr;

  // If we are crossing into a different query block, but there's a streaming
  // or materialization node in the way, don't count it as the root; we want
  // any SELECT printouts to be on the actual root node.
  // TODO(sgunders): This gives the wrong result if a query block ends in a
  // materialization.
  bool delayed_root_of_join = false;
  if (path->type == AccessPath::STREAM ||
      path->type == AccessPath::MATERIALIZE) {
    delayed_root_of_join = is_root_of_join;
    is_root_of_join = false;
  }

  if (AddChildrenToObject(obj, children, join, delayed_root_of_join, "inputs"))
    return nullptr;

  // If we know that the join will return zero rows, we don't bother
  // optimizing any subqueries in the SELECT list, but end optimization
  // early (see Query_block::optimize()). If so, don't attempt to print
  // them either, as they have no query plan.
  if (is_root_of_join && path->type != AccessPath::ZERO_ROWS) {
    vector<ExplainChild> children_from_select;
    if (GetAccessPathsFromSelectList(join, &children_from_select))
      return nullptr;
    if (AddChildrenToObject(obj, children_from_select, join,
                            /*is_root_of_join*/ true,
                            "inputs_from_select_list"))
      return nullptr;
  }

  if (error == 0)
    return ret_obj;
  else
    return nullptr;
}

std::string PrintQueryPlan(THD *ethd, const THD *query_thd,
                           Query_expression *unit) {
  JOIN *join = nullptr;
  bool is_root_of_join = (unit != nullptr ? !unit->is_union() : false);
  AccessPath *path = (unit != nullptr ? unit->root_access_path() : nullptr);

  if (path == nullptr) return "<not executable by iterator executor>\n";

  // "join" should be set to the JOIN that "path" is part of (or nullptr
  // if it is not, e.g. if it's a part of executing a UNION).
  if (unit != nullptr && !unit->is_union())
    join = unit->first_query_block()->join;

  /* Create a Json object for the plan */
  std::unique_ptr<Json_object> obj =
      ExplainQueryPlan(path, &query_thd->query_plan, join, is_root_of_join);
  if (obj == nullptr) return "";

  // Append the (rewritten) query string, if any.
  // Skip this if applicable. See print_query_for_explain() comments.
  if (ethd == query_thd) {
    StringBuffer<1024> str;
    print_query_for_explain(query_thd, unit, &str);
    if (!str.is_empty()) {
      if (AddMemberToObject<Json_string>(obj.get(), "query", str.ptr(),
                                         str.length()))
        return "";
    }
  }

  /*
    Output should be either in json format, or a tree format, depending on
    the specified format
   */
  return ethd->lex->explain_format->ExplainJsonToString(obj.get());
}

/* PrintQueryPlan()
 * This overloaded function is for debugging purpose.
 */
std::string PrintQueryPlan(int level, AccessPath *path, JOIN *join,
                           bool is_root_of_join) {
  string ret;
  Explain_format_tree format;

  if (path == nullptr) {
    ret.assign(level * 4, ' ');
    return ret + "<not executable by iterator executor>\n";
  }

  /* Create a Json object for the plan */
  std::unique_ptr<Json_object> json =
      ExplainAccessPath(path, nullptr, join, is_root_of_join);
  if (json == nullptr) return "";

  /* Output in tree format.*/
  string explain;
  format.ExplainPrintTreeNode(json.get(), level, &explain, /*tokens=*/nullptr);
  return explain;
}

// 0x
// truncated_sha256(desc1,desc2,...,[child1_desc:]0xchild1,[child2_desc:]0xchild2,...)
static string GetForceSubplanToken(const Json_object *obj,
                                   const string &children_digest) {
  string digest;
  digest += down_cast<Json_string *>(obj->get("operation"))->value() +
            children_digest;

  unsigned char sha256sum[SHA256_DIGEST_LENGTH];
  (void)SHA_EVP256(pointer_cast<const unsigned char *>(digest.data()),
                   digest.size(), sha256sum);

  char ret[8 * 2 + 2 + 1];
  snprintf(ret, sizeof(ret), "0x%02x%02x%02x%02x%02x%02x%02x%02x", sha256sum[0],
           sha256sum[1], sha256sum[2], sha256sum[3], sha256sum[4], sha256sum[5],
           sha256sum[6], sha256sum[7]);

  return ret;
}

string GetForceSubplanToken(AccessPath *path, JOIN *join) {
  if (path == nullptr) {
    return "";
  }

  Explain_format_tree format;
  string explain;
  vector<string> tokens_for_force_subplan;

  /* Create a Json object for the plan */
  std::unique_ptr<Json_object> json =
      ExplainAccessPath(path, nullptr, join, /*is_root_of_join=*/true);
  if (json == nullptr) return "";

  format.ExplainPrintTreeNode(json.get(), 0, &explain,
                              &tokens_for_force_subplan);

  /* The object's token is present at the end of the token vector */
  return tokens_for_force_subplan.back();
}

/// Convert Json object to string.
string Explain_format_tree::ExplainJsonToString(Json_object *json) {
  string explain;

  vector<string> *token_ptr = nullptr;
#ifndef NDEBUG
  vector<string> tokens_for_force_subplan;
  DBUG_EXECUTE_IF("subplan_tokens", token_ptr = &tokens_for_force_subplan;);
#endif

  this->ExplainPrintTreeNode(json, 0, &explain, token_ptr);
  if (explain.empty()) return "";

  DBUG_EXECUTE_IF("subplan_tokens", {
    explain += "\nTo force this plan, use:\nSET DEBUG='+d,subplan_tokens";
    for (const string &token : tokens_for_force_subplan) {
      explain += ",force_subplan_";
      explain += token;
    }
    explain += "';\n";
  });

  return explain;
}

void Explain_format_tree::ExplainPrintTreeNode(const Json_dom *json, int level,
                                               string *explain,
                                               vector<string> *subplan_token) {
  string children_explain;
  string children_digest;

  explain->append(level * 4, ' ');

  if (json == nullptr || json->json_type() == enum_json_type::J_NULL) {
    explain->append("<not executable by iterator executor>\n");
    return;
  }

  const Json_object *obj = down_cast<const Json_object *>(json);

  AppendChildren(obj->get("inputs"), level + 1, &children_explain,
                 subplan_token, &children_digest);
  AppendChildren(obj->get("inputs_from_select_list"), level, &children_explain,
                 subplan_token, &children_digest);

  *explain += "-> ";
  if (subplan_token) {
    /*
     Include the current subplan node's token into the explain plan.
     Also append it to the subplan_token vector because parent will need it
     for generating its own subplan token.
     */
    string my_subplan_token = GetForceSubplanToken(obj, children_digest);
    *explain += '[' + my_subplan_token + "] ";
    subplan_token->push_back(my_subplan_token);
  }
  assert(obj->get("operation")->json_type() == enum_json_type::J_STRING);
  *explain += down_cast<Json_string *>(obj->get("operation"))->value();

  ExplainPrintCosts(obj, explain);

  *explain += children_explain;
}

void Explain_format_tree::ExplainPrintCosts(const Json_object *obj,
                                            string *explain) {
  bool has_first_cost = obj->get("estimated_first_row_cost") != nullptr;
  bool has_cost = obj->get("estimated_total_cost") != nullptr;

  if (has_cost) {
    double last_cost = GetJSONDouble(obj, "estimated_total_cost");
    assert(obj->get("estimated_rows") != nullptr);
    double rows = GetJSONDouble(obj, "estimated_rows");

    // NOTE: We cannot use %f, since MSVC and GCC round 0.5 in different
    // directions, so tests would not be reproducible between platforms.
    // Format/round using my_gcvt() and llrint() instead.
    char cost_as_string[FLOATING_POINT_BUFFER];
    my_fcvt(last_cost, 2, cost_as_string, /*error=*/nullptr);

    // Nominally, we only write number of rows as an integer.
    // However, if that should end up in zero, it's hard to know
    // whether that was 0.49 or 0.00001, so we add enough precision
    // to get one leading digit in that case.
    char rows_as_string[32];
    if (llrint(rows) == 0 && rows >= 1e-9) {
      snprintf(rows_as_string, sizeof(rows_as_string), "%.1g", rows);
    } else {
      snprintf(rows_as_string, sizeof(rows_as_string), "%lld", llrint(rows));
    }

    char str[1024];
    if (has_first_cost) {
      double first_row_cost = GetJSONDouble(obj, "estimated_first_row_cost");
      char first_row_cost_as_string[FLOATING_POINT_BUFFER];
      my_fcvt(first_row_cost, 2, first_row_cost_as_string, /*error=*/nullptr);
      snprintf(str, sizeof(str), "  (cost=%s..%s rows=%s)",
               first_row_cost_as_string, cost_as_string, rows_as_string);
    } else {
      snprintf(str, sizeof(str), "  (cost=%s rows=%s)", cost_as_string,
               rows_as_string);
    }

    *explain += str;
  }

  /* Show actual figures if timing info is present */
  if (obj->get("actual_rows") != nullptr) {
    if (!has_cost) {
      // We always want a double space between the iterator name and the costs.
      explain->push_back(' ');
    }
    explain->push_back(' ');

    if (obj->get("actual_rows")->json_type() == enum_json_type::J_NULL) {
      *explain += "(never executed)";
    } else {
      double actual_first_row_ms = GetJSONDouble(obj, "actual_first_row_ms");
      double actual_last_row_ms = GetJSONDouble(obj, "actual_last_row_ms");
      double actual_rows = GetJSONDouble(obj, "actual_rows");
      uint64_t actual_loops =
          down_cast<Json_int *>(obj->get("actual_loops"))->value();
      char str[1024];
      snprintf(str, sizeof(str),
               "(actual time=%.3f..%.3f rows=%lld loops=%" PRIu64 ")",
               actual_first_row_ms, actual_last_row_ms,
               llrintf(static_cast<double>(actual_rows)), actual_loops);
      *explain += str;
    }
  }
  *explain += "\n";
}

/*
  The out param 'child_token_digest' will have something like :
  ",[child1_desc:]0xchild1,[child2_desc:]0xchild2,....."
*/
void Explain_format_tree::AppendChildren(
    const Json_dom *children, int level, string *explain,
    vector<string> *tokens_for_force_subplan, string *child_token_digest) {
  if (children == nullptr) {
    return;
  }
  assert(children->json_type() == enum_json_type::J_ARRAY);
  for (const Json_dom_ptr &child : *down_cast<const Json_array *>(children)) {
    if (tokens_for_force_subplan) {
      *child_token_digest += ',';
    }
    if (child->json_type() == enum_json_type::J_OBJECT &&
        down_cast<const Json_object *>(child.get())->get("heading") !=
            nullptr) {
      string heading =
          down_cast<Json_string *>(
              down_cast<const Json_object *>(child.get())->get("heading"))
              ->value();

      /* If a token is being generated, append the child tokens */
      if (tokens_for_force_subplan) {
        *child_token_digest += heading + ":";
      }

      explain->append(level * 4, ' ');
      explain->append("-> ");
      explain->append(heading);
      explain->append("\n");
      this->ExplainPrintTreeNode(child.get(), level + 1, explain,
                                 tokens_for_force_subplan);
    } else {
      this->ExplainPrintTreeNode(child.get(), level, explain,
                                 tokens_for_force_subplan);
    }

    /* Include the child subtoken in the child digest. */
    if (tokens_for_force_subplan) {
      /* The child's token is present at the end of the token vector */
      child_token_digest->append(tokens_for_force_subplan->back());
    }
  }
}
