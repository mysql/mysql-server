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

#include "sha2.h"
#include "sql/filesort.h"
#include "sql/item_sum.h"
#include "sql/iterators/basic_row_iterators.h"
#include "sql/iterators/bka_iterator.h"
#include "sql/iterators/composite_iterators.h"
#include "sql/iterators/hash_join_iterator.h"
#include "sql/iterators/ref_row_iterators.h"
#include "sql/iterators/sorting_iterator.h"
#include "sql/iterators/timing_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/range_optimizer/group_index_skip_scan_plan.h"
#include "sql/range_optimizer/index_skip_scan_plan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "template_utils.h"

using std::string;
using std::vector;

static string PrintRanges(const QUICK_RANGE *const *ranges, unsigned num_ranges,
                          const KEY_PART_INFO *key_part, bool single_part_only);

struct ExplainData {
  /**
    Returns a short string (used for EXPLAIN FORMAT=tree) with user-readable
    information for this iterator. When implementing these, try to avoid
    internal jargon (e.g. “eq_ref”); prefer things that read like normal,
    technical English (e.g. “single-row index lookup”).

    For certain complex operations, such as MaterializeIterator, there can be
    multiple strings. If so, they are interpreted as nested operations,
    with the outermost, last-done operation first and the other ones indented
    as if they were child iterators.
   */
  vector<string> description;

  struct Child {
    AccessPath *path;

    // Normally blank. If not blank, a heading for this iterator
    // saying what kind of role it has to the parent if it is not
    // obvious. E.g., FilterIterator can print iterators that are
    // children because they come out of subselect conditions.
    std::string description = "";

    // If this child is the root of a new JOIN, it is contained here.
    JOIN *join = nullptr;
  };

  /// List of zero or more access paths which are direct children of this one.
  /// By convention, if there are multiple ones (ie., we're doing a join),
  /// the outer iterator is listed first. So for a LEFT JOIN b, we'd list
  /// a before b.
  vector<Child> children;
};

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

string HashJoinTypeToString(RelationalExpression::Type join_type) {
  switch (join_type) {
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
      return "Inner hash join";
    case RelationalExpression::LEFT_JOIN:
      return "Left hash join";
    case RelationalExpression::ANTIJOIN:
      return "Hash antijoin";
    case RelationalExpression::SEMIJOIN:
      return "Hash semijoin";
    default:
      assert(false);
      return "<error>";
  }
}

static void GetAccessPathsFromItem(Item *item_arg, const char *source_text,
                                   vector<ExplainData::Child> *children) {
  WalkItem(item_arg, enum_walk::POSTFIX, [children, source_text](Item *item) {
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
    children->push_back({path, description, query_block->join});
    return false;
  });
}

vector<ExplainData::Child> GetAccessPathsFromSelectList(JOIN *join) {
  vector<ExplainData::Child> ret;
  if (join == nullptr) {
    return ret;
  }

  // Look for any Items in the projection list itself.
  for (Item *item : *join->get_current_fields()) {
    GetAccessPathsFromItem(item, "projection", &ret);
  }

  // Look for any Items that were materialized into fields during execution.
  for (uint table_idx = join->primary_tables; table_idx < join->tables;
       ++table_idx) {
    QEP_TAB *qep_tab = &join->qep_tab[table_idx];
    if (qep_tab != nullptr && qep_tab->tmp_table_param != nullptr) {
      for (Func_ptr &func : *qep_tab->tmp_table_param->items_to_copy) {
        GetAccessPathsFromItem(func.func(), "projection", &ret);
      }
    }
  }
  return ret;
}

ExplainData ExplainAccessPath(const AccessPath *path, JOIN *join,
                              bool include_costs);

// The table iterator could be a slightly more complicated iterator than
// the basic iterators (in particular, ALTERNATIVE), so show the entire
// thing.
static void AddTableIteratorDescription(const AccessPath *path, JOIN *join,
                                        vector<string> *description) {
  const AccessPath *subpath = path;
  for (;;) {
    ExplainData explain =
        ExplainAccessPath(subpath, join, /*include_costs=*/true);
    for (string str : explain.description) {
      if (explain.children.size() > 1) {
        // This can happen if we have AlternativeIterator.
        // TODO(sgunders): Consider having a RowIterator::parent(),
        // so that we can show the entire tree.
        str += " [other sub-iterators not shown]";
      }
      description->push_back(str);
    }
    if (explain.children.empty()) break;
    subpath = explain.children[0].path;
  }
}

static void ExplainMaterializeAccessPath(const AccessPath *path, JOIN *join,
                                         vector<string> *description,
                                         vector<ExplainData::Child> *children,
                                         bool explain_analyze) {
  MaterializePathParameters *param = path->materialize().param;

  AddTableIteratorDescription(path->materialize().table_path, join,
                              description);

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
                            [](const TABLE_LIST *tab) {
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

  const bool is_union = param->query_blocks.size() > 1;
  string str;

  if (param->cte != nullptr) {
    if (param->cte->recursive) {
      str = "Materialize recursive CTE " + to_string(param->cte->name);
    } else {
      if (is_union) {
        str = "Materialize union CTE " + to_string(param->cte->name);
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
  } else if (is_union) {
    str = "Union materialize";
  } else if (param->rematerialize) {
    str = "Temporary table";
  } else {
    str = "Materialize";
  }

  if (MaterializeIsDoingDeduplication(param->table)) {
    str += " with deduplication";
  }

  if (param->invalidators != nullptr) {
    bool first = true;
    str += " (invalidate on row from ";
    for (const AccessPath *invalidator : *param->invalidators) {
      if (!first) {
        str += "; ";
      }

      first = false;
      str += invalidator->cache_invalidator().name;
    }
    str += ")";
  }

  description->push_back(str);

  // Children.

  // If a CTE is referenced multiple times, only bother printing its query plan
  // once, instead of repeating it over and over again.
  //
  // TODO(sgunders): Consider printing CTE query plans on the top level of the
  // query block instead?
  if (param->cte != nullptr && !explain_cte_now) {
    return;
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
}

static void ExplainIndexSkipScanAccessPath(const AccessPath *path,
                                           JOIN *join [[maybe_unused]],
                                           vector<string> *description,
                                           vector<ExplainData::Child> *children
                                           [[maybe_unused]]) {
  TABLE *table = path->index_skip_scan().table;
  KEY *key_info = table->key_info + path->index_skip_scan().index;

  // NOTE: Currently, index skip scan is always covering, but there's no
  // good reason why we cannot fix this limitation in the future.
  string ret = string(table->key_read ? "Covering index skip scan on "
                                      : "Index skip scan on ") +
               table->alias + " using " + key_info->name + " over ";
  IndexSkipScanParameters *param = path->index_skip_scan().param;

  // Print out any equality ranges.
  bool first = true;
  for (unsigned key_part_idx = 0; key_part_idx < param->eq_prefix_key_parts;
       ++key_part_idx) {
    if (!first) {
      ret += ", ";
    }
    first = false;

    ret += param->index_info->key_part[key_part_idx].field->field_name;
    Bounds_checked_array<unsigned char *> prefixes =
        param->eq_prefixes[key_part_idx].eq_key_prefixes;
    if (prefixes.size() == 1) {
      ret += " = ";
      String out;
      print_key_value(&out, &param->index_info->key_part[key_part_idx],
                      prefixes[0]);
      ret += to_string(out);
    } else {
      ret += " IN (";
      for (unsigned i = 0; i < prefixes.size(); ++i) {
        if (i == 2 && prefixes.size() > 3) {
          ret += StringPrintf(", (%zu more)", prefixes.size() - 2);
          break;
        } else if (i != 0) {
          ret += ", ";
        }
        String out;
        print_key_value(&out, &param->index_info->key_part[key_part_idx],
                        prefixes[i]);
        ret += to_string(out);
      }
      ret += ")";
    }
  }

  // Then the ranges.
  if (!first) {
    ret += ", ";
  }
  String out;
  append_range(&out, param->range_key_part, param->min_range_key,
               param->max_range_key, param->range_cond_flag);
  ret += to_string(out);

  description->push_back(ret);
}

static void ExplainGroupIndexSkipScanAccessPath(
    const AccessPath *path, JOIN *join [[maybe_unused]],
    vector<string> *description,
    vector<ExplainData::Child> *children [[maybe_unused]]) {
  TABLE *table = path->group_index_skip_scan().table;
  KEY *key_info = table->key_info + path->group_index_skip_scan().index;
  GroupIndexSkipScanParameters *param = path->group_index_skip_scan().param;

  // NOTE: Currently, group index skip scan is always covering, but there's no
  // good reason why we cannot fix this limitation in the future.
  string ret;
  if (param->min_max_arg_part != nullptr) {
    ret = string(table->key_read ? "Covering index skip scan for grouping on "
                                 : "Index skip scan for grouping on ") +
          table->alias + " using " + key_info->name;
  } else {
    ret = string(table->key_read
                     ? "Covering index skip scan for deduplication on "
                     : "Index skip scan for deduplication on ") +
          table->alias + " using " + key_info->name;
  }

  // Print out prefix ranges, if any.
  if (!param->prefix_ranges.empty()) {
    ret += " over ";
    ret += PrintRanges(param->prefix_ranges.data(), param->prefix_ranges.size(),
                       key_info->key_part, /*single_part_only=*/false);
  }

  // Print out the ranges on the MIN/MAX keypart, if we have them.
  // (We don't print infix ranges, because they seem to be in an unusual
  // format.)
  if (!param->min_max_ranges.empty()) {
    if (param->prefix_ranges.empty()) {
      ret += " over ";
    } else {
      ret += ", ";
    }
    ret += PrintRanges(param->min_max_ranges.data(),
                       param->min_max_ranges.size(), param->min_max_arg_part,
                       /*single_part_only=*/true);
  }

  description->push_back(ret);
}

static void AddChildrenFromPushedCondition(
    const TABLE *table, vector<ExplainData::Child> *children) {
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
    GetAccessPathsFromItem(pushed_cond, "pushed condition", children);
  }
}

static string PrintRanges(const QUICK_RANGE *const *ranges, unsigned num_ranges,
                          const KEY_PART_INFO *key_part,
                          bool single_part_only) {
  string ret;
  for (unsigned range_idx = 0; range_idx < num_ranges; ++range_idx) {
    if (range_idx == 2 && num_ranges > 3) {
      char str[256];
      snprintf(str, sizeof(str), " OR (%u more)", num_ranges - 2);
      ret += str;
      break;
    } else if (range_idx > 0) {
      ret += " OR ";
    }
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
    ret += "(" + to_string(str) + ")";
  }
  return ret;
}

ExplainData ExplainAccessPath(const AccessPath *path, JOIN *join,
                              bool include_costs) {
  vector<string> description;
  vector<ExplainData::Child> children;
  switch (path->type) {
    case AccessPath::TABLE_SCAN:
      description.push_back(string("Table scan on ") +
                            path->table_scan().table->alias +
                            path->table_scan().table->file->explain_extra());
      AddChildrenFromPushedCondition(path->table_scan().table, &children);
      break;
    case AccessPath::INDEX_SCAN: {
      TABLE *table = path->index_scan().table;
      assert(table->file->pushed_idx_cond == nullptr);

      const KEY *key = &table->key_info[path->index_scan().idx];
      string str = string(table->key_read ? "Covering index scan on "
                                          : "Index scan on ") +
                   table->alias + " using " + key->name;
      if (path->index_scan().reverse) {
        str += " (reverse)";
      }
      str += table->file->explain_extra();

      description.push_back(move(str));
      AddChildrenFromPushedCondition(table, &children);
      break;
    }
    case AccessPath::REF: {
      TABLE *table = path->ref().table;
      const KEY *key = &table->key_info[path->ref().ref->key];
      string str = string(table->key_read ? "Covering index lookup on "
                                          : "Index lookup on ") +
                   table->alias + " using " + key->name + " (" +
                   RefToString(*path->ref().ref, key, /*include_nulls=*/false);
      if (path->ref().reverse) {
        str += "; iterate backwards";
      }
      str += ")";
      if (table->file->pushed_idx_cond != nullptr) {
        str += ", with index condition: " +
               ItemToString(table->file->pushed_idx_cond);
      }
      str += table->file->explain_extra();
      description.push_back(move(str));
      AddChildrenFromPushedCondition(table, &children);
      break;
    }
    case AccessPath::REF_OR_NULL: {
      TABLE *table = path->ref_or_null().table;
      const KEY *key = &table->key_info[path->ref_or_null().ref->key];
      string str =
          string(table->key_read ? "Covering index lookup on "
                                 : "Index lookup on ") +
          table->alias + " using " + key->name + " (" +
          RefToString(*path->ref_or_null().ref, key, /*include_nulls=*/true) +
          ")";
      if (table->file->pushed_idx_cond != nullptr) {
        str += ", with index condition: " +
               ItemToString(table->file->pushed_idx_cond);
      }
      str += table->file->explain_extra();
      description.push_back(move(str));
      AddChildrenFromPushedCondition(table, &children);
      break;
    }
    case AccessPath::EQ_REF: {
      TABLE *table = path->eq_ref().table;
      const KEY *key = &table->key_info[path->eq_ref().ref->key];
      string str =
          string(table->key_read ? "Single-row covering index lookup on "
                                 : "Single-row index lookup on ") +
          table->alias + " using " + key->name + " (" +
          RefToString(*path->eq_ref().ref, key, /*include_nulls=*/false) + ")";
      if (table->file->pushed_idx_cond != nullptr) {
        str += ", with index condition: " +
               ItemToString(table->file->pushed_idx_cond);
      }
      str += table->file->explain_extra();
      description.push_back(move(str));
      AddChildrenFromPushedCondition(table, &children);
      break;
    }
    case AccessPath::PUSHED_JOIN_REF: {
      TABLE *table = path->pushed_join_ref().table;
      assert(table->file->pushed_idx_cond == nullptr);
      const KEY *key = &table->key_info[path->pushed_join_ref().ref->key];
      string str;
      if (path->pushed_join_ref().is_unique) {
        str =
            table->key_read ? "Single-row covering index" : "Single-row index";
      } else {
        str = table->key_read ? "Covering index" : "Index";
      }
      str += " lookup on " + string(table->alias) + " using " + key->name +
             " (" +
             RefToString(*path->pushed_join_ref().ref, key,
                         /*include_nulls=*/false) +
             ")" + table->file->explain_extra();
      description.push_back(move(str));
      break;
    }
    case AccessPath::FULL_TEXT_SEARCH: {
      TABLE *table = path->full_text_search().table;
      assert(table->file->pushed_idx_cond == nullptr);
      const KEY *key = &table->key_info[path->full_text_search().ref->key];
      description.push_back(string(table->key_read
                                       ? "Full-text covering index search on "
                                       : "Full-text index search on ") +
                            table->alias + " using " + key->name + " (" +
                            RefToString(*path->full_text_search().ref, key,
                                        /*include_nulls=*/false) +
                            ")" + table->file->explain_extra());
      break;
    }
    case AccessPath::CONST_TABLE: {
      TABLE *table = path->const_table().table;
      assert(table->file->pushed_idx_cond == nullptr);
      assert(table->file->pushed_cond == nullptr);
      description.push_back(string("Constant row from ") + table->alias);
      break;
    }
    case AccessPath::MRR: {
      TABLE *table = path->mrr().table;
      const KEY *key = &table->key_info[path->mrr().ref->key];
      string str =
          string(table->key_read ? "Multi-range covering index lookup on "
                                 : "Multi-range index lookup on ") +
          table->alias + " using " + key->name + " (" +
          RefToString(*path->mrr().ref, key, /*include_nulls=*/false) + ")";
      if (table->file->pushed_idx_cond != nullptr) {
        str += ", with index condition: " +
               ItemToString(table->file->pushed_idx_cond);
      }
      str += table->file->explain_extra();
      description.push_back(move(str));
      AddChildrenFromPushedCondition(table, &children);
      break;
    }
    case AccessPath::FOLLOW_TAIL:
      description.push_back(string("Scan new records on ") +
                            path->follow_tail().table->alias);
      AddChildrenFromPushedCondition(path->follow_tail().table, &children);
      break;
    case AccessPath::INDEX_RANGE_SCAN: {
      const auto &param = path->index_range_scan();
      TABLE *table = param.used_key_part[0].field->table;
      KEY *key_info = table->key_info + param.index;
      string ret = string(table->key_read ? "Covering index range scan on "
                                          : "Index range scan on ") +
                   table->alias + " using " + key_info->name + " over ";
      ret += PrintRanges(param.ranges, param.num_ranges, key_info->key_part,
                         /*single_part_only=*/false);
      if (path->index_range_scan().reverse) {
        ret += " (reverse)";
      }
      if (table->file->pushed_idx_cond != nullptr) {
        ret += ", with index condition: " +
               ItemToString(table->file->pushed_idx_cond);
      }
      ret += table->file->explain_extra();
      description.push_back(move(ret));
      AddChildrenFromPushedCondition(table, &children);
      break;
    }
    case AccessPath::INDEX_MERGE: {
      const auto &param = path->index_merge();
      description.emplace_back("Sort-deduplicate by row ID");
      for (AccessPath *child : *path->index_merge().children) {
        if (param.allow_clustered_primary_key_scan &&
            param.table->file->primary_key_is_clustered() &&
            child->index_range_scan().index == param.table->s->primary_key) {
          children.push_back(
              {child, "Clustered primary key (scanned separately)"});
        } else {
          children.push_back({child});
        }
      }
      break;
    }
    case AccessPath::ROWID_INTERSECTION: {
      description.emplace_back("Intersect rows sorted by row ID");
      for (AccessPath *child : *path->rowid_intersection().children) {
        children.push_back({child});
      }
      break;
    }
    case AccessPath::ROWID_UNION: {
      description.emplace_back("Deduplicate rows sorted by row ID");
      for (AccessPath *child : *path->rowid_union().children) {
        children.push_back({child});
      }
      break;
    }
    case AccessPath::INDEX_SKIP_SCAN: {
      ExplainIndexSkipScanAccessPath(path, join, &description, &children);
      break;
    }
    case AccessPath::GROUP_INDEX_SKIP_SCAN: {
      ExplainGroupIndexSkipScanAccessPath(path, join, &description, &children);
      break;
    }
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN: {
      TABLE *table = path->dynamic_index_range_scan().table;
      string str = string(table->key_read ? "Covering index range scan on "
                                          : "Index range scan on ") +
                   table->alias + " (re-planned for each iteration)";
      if (table->file->pushed_idx_cond != nullptr) {
        str += ", with index condition: " +
               ItemToString(table->file->pushed_idx_cond);
      }
      str += table->file->explain_extra();
      description.push_back(move(str));
      AddChildrenFromPushedCondition(table, &children);
      break;
    }
    case AccessPath::TABLE_VALUE_CONSTRUCTOR:
    case AccessPath::FAKE_SINGLE_ROW:
      description.emplace_back("Rows fetched before execution");
      break;
    case AccessPath::ZERO_ROWS:
      description.push_back(string("Zero rows (") + path->zero_rows().cause +
                            ")");
      // The child is not printed as part of the iterator tree.
      break;
    case AccessPath::ZERO_ROWS_AGGREGATED:
      description.push_back(string("Zero input rows (") +
                            path->zero_rows_aggregated().cause +
                            "), aggregated into one output row");
      break;
    case AccessPath::MATERIALIZED_TABLE_FUNCTION:
      description.emplace_back("Materialize table function");
      break;
    case AccessPath::UNQUALIFIED_COUNT:
      description.push_back("Count rows in " +
                            string(join->qep_tab->table()->alias));
      break;
    case AccessPath::NESTED_LOOP_JOIN:
      description.push_back(
          "Nested loop " +
          JoinTypeToString(path->nested_loop_join().join_type));
      children.push_back({path->nested_loop_join().outer});
      children.push_back({path->nested_loop_join().inner});
      break;
    case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL:
      description.push_back(
          string("Nested loop semijoin with duplicate removal on ") +
          path->nested_loop_semijoin_with_duplicate_removal().key->name);
      children.push_back(
          {path->nested_loop_semijoin_with_duplicate_removal().outer});
      children.push_back(
          {path->nested_loop_semijoin_with_duplicate_removal().inner});
      break;
    case AccessPath::BKA_JOIN:
      description.push_back("Batched key access " +
                            JoinTypeToString(path->bka_join().join_type));
      children.push_back({path->bka_join().outer, "Batch input rows"});
      children.push_back({path->bka_join().inner});
      break;
    case AccessPath::HASH_JOIN: {
      const JoinPredicate *predicate = path->hash_join().join_predicate;
      RelationalExpression::Type type = path->hash_join().rewrite_semi_to_inner
                                            ? RelationalExpression::INNER_JOIN
                                            : predicate->expr->type;
      string ret = HashJoinTypeToString(type);

      if (predicate->expr->equijoin_conditions.empty()) {
        ret.append(" (no condition)");
      } else {
        for (Item_func_eq *cond : predicate->expr->equijoin_conditions) {
          if (cond != predicate->expr->equijoin_conditions[0]) {
            ret.push_back(',');
          }
          HashJoinCondition hj_cond(cond, *THR_MALLOC);
          if (!hj_cond.store_full_sort_key()) {
            ret.append(" (<hash>(" + ItemToString(hj_cond.left_extractor()) +
                       ")=<hash>(" + ItemToString(hj_cond.right_extractor()) +
                       "))");
          } else {
            ret.append(" " + ItemToString(cond));
          }
        }
      }
      for (Item *cond : predicate->expr->join_conditions) {
        if (cond == predicate->expr->join_conditions[0]) {
          ret.append(", extra conditions: ");
        } else {
          ret += " and ";
        }
        ret += ItemToString(cond);
      }

      description.push_back(move(ret));
      children.push_back({path->hash_join().outer});
      children.push_back({path->hash_join().inner, "Hash"});
      break;
    }
    case AccessPath::FILTER:
      description.push_back("Filter: " +
                            ItemToString(path->filter().condition));
      children.push_back({path->filter().child});
      GetAccessPathsFromItem(path->filter().condition, "condition", &children);
      break;
    case AccessPath::SORT: {
      string ret;
      if (path->sort().filesort == nullptr) {
        // This is a hack for when computing digests for forcing subplans (which
        // happens on non-finalized plans, which don't have a filesort object
        // yet). It means that sorts won't be correctly forced.
        // TODO(sgunders): Print based on the flags and order instead of the
        // filesort object, when using the hypergraph join optimizer.
        description.emplace_back("Sort");
        children.push_back({path->sort().child});
        break;
      }

      if (path->sort().filesort->using_addon_fields()) {
        ret = "Sort";
      } else {
        ret = "Sort row IDs";
      }
      if (path->sort().filesort->m_remove_duplicates) {
        ret += " with duplicate removal: ";
      } else {
        ret += ": ";
      }

      bool first = true;
      for (unsigned i = 0; i < path->sort().filesort->sort_order_length();
           ++i) {
        if (first) {
          first = false;
        } else {
          ret += ", ";
        }

        const st_sort_field *order = &path->sort().filesort->sortorder[i];
        ret += ItemToString(order->item);
        if (order->reverse) {
          ret += " DESC";
        }
      }
      if (path->sort().filesort->limit != HA_POS_ERROR) {
        char buf[256];
        snprintf(buf, sizeof(buf), ", limit input to %llu row(s) per chunk",
                 path->sort().filesort->limit);
        ret += buf;
      }
      description.push_back(move(ret));
      children.push_back({path->sort().child});
      break;
    }
    case AccessPath::AGGREGATE: {
      string ret;
      if (join->grouped || join->group_optimized_away) {
        if (*join->sum_funcs == nullptr) {
          ret = "Group (no aggregates)";
        } else if (path->aggregate().rollup) {
          ret = "Group aggregate with rollup: ";
        } else {
          ret = "Group aggregate: ";
        }
      } else {
        ret = "Aggregate: ";
      }

      bool first = true;
      for (Item_sum **item = join->sum_funcs; *item != nullptr; ++item) {
        if (first) {
          first = false;
        } else {
          ret += ", ";
        }
        if (path->aggregate().rollup) {
          ret += ItemToString((*item)->unwrap_sum());
        } else {
          ret += ItemToString(*item);
        }
      }
      description.push_back(move(ret));
      children.push_back({path->aggregate().child});
      break;
    }
    case AccessPath::TEMPTABLE_AGGREGATE: {
      // We don't list the table iterator as an explicit child; we mark it in
      // our description instead. (Anything else would look confusingly much
      // like a join.)
      ExplainData table_explain = ExplainAccessPath(
          path->temptable_aggregate().table_path, join, include_costs);
      description = move(table_explain.description);
      description.emplace_back("Aggregate using temporary table");
      children.push_back({path->temptable_aggregate().subquery_path});
      break;
    }
    case AccessPath::LIMIT_OFFSET: {
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
      if (path->limit_offset().count_all_rows) {
        description.push_back(string(buf) +
                              " (no early end due to SQL_CALC_FOUND_ROWS)");
      } else {
        description.emplace_back(buf);
      }
      children.push_back({path->limit_offset().child});
      break;
    }
    case AccessPath::STREAM:
      description.emplace_back("Stream results");
      children.push_back({path->stream().child});
      break;
    case AccessPath::MATERIALIZE:
      ExplainMaterializeAccessPath(
          path, join, &description, &children,
          include_costs && current_thd->lex->is_explain_analyze);
      break;
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE:
      AddTableIteratorDescription(
          path->materialize_information_schema_table().table_path, join,
          &description);
      description.push_back("Fill information schema table " +
                            string(path->materialize_information_schema_table()
                                       .table_list->table->alias));
      break;
    case AccessPath::APPEND:
      description.emplace_back("Append");
      for (const AppendPathParameters &child : *path->append().children) {
        children.push_back({child.path, "", child.join});
      }
      break;
    case AccessPath::WINDOW: {
      string buf;
      if (path->window().needs_buffering) {
        Window *window = path->window().temp_table_param->m_window;
        if (window->optimizable_row_aggregates() ||
            window->optimizable_range_aggregates() ||
            window->static_aggregates()) {
          buf = "Window aggregate with buffering: ";
        } else {
          buf = "Window multi-pass aggregate with buffering: ";
        }
      } else {
        buf = "Window aggregate: ";
      }

      bool first = true;
      for (const Func_ptr &func :
           *(path->window().temp_table_param->items_to_copy)) {
        if (func.func()->m_is_window_function) {
          if (!first) {
            buf += ", ";
          }
          buf += ItemToString(func.func());
          first = false;
        }
      }
      description.push_back(move(buf));
      children.push_back({path->window().child});
      break;
    }
    case AccessPath::WEEDOUT: {
      SJ_TMP_TABLE *sj = path->weedout().weedout_table;

      string ret = "Remove duplicate ";
      if (sj->tabs_end == sj->tabs + 1) {  // Only one table.
        ret += sj->tabs->qep_tab->table()->alias;
      } else {
        ret += "(";
        for (SJ_TMP_TABLE_TAB *tab = sj->tabs; tab != sj->tabs_end; ++tab) {
          if (tab != sj->tabs) {
            ret += ", ";
          }
          ret += tab->qep_tab->table()->alias;
        }
        ret += ")";
      }
      ret += " rows using temporary table (weedout)";
      description.push_back(ret);
      children.push_back({path->weedout().child});
      break;
    }
    case AccessPath::REMOVE_DUPLICATES: {
      string ret = "Remove duplicates from input grouped on ";
      for (int i = 0; i < path->remove_duplicates().group_items_size; ++i) {
        if (i != 0) {
          ret += ", ";
        }
        ret += ItemToString(path->remove_duplicates().group_items[i]);
      }
      description.push_back(std::move(ret));
      children.push_back({path->remove_duplicates().child});
      break;
    }
    case AccessPath::REMOVE_DUPLICATES_ON_INDEX:
      description.push_back(string("Remove duplicates from input sorted on ") +
                            path->remove_duplicates_on_index().key->name);
      children.push_back({path->remove_duplicates_on_index().child});
      break;
    case AccessPath::ALTERNATIVE: {
      const TABLE *table =
          path->alternative().table_scan_path->table_scan().table;
      const TABLE_REF *ref = path->alternative().used_ref;
      const KEY *key = &table->key_info[ref->key];

      int num_applicable_cond_guards = 0;
      for (unsigned key_part_idx = 0; key_part_idx < ref->key_parts;
           ++key_part_idx) {
        if (ref->cond_guards[key_part_idx] != nullptr) {
          ++num_applicable_cond_guards;
        }
      }

      string ret = "Alternative plans for IN subquery: Index lookup unless ";
      if (num_applicable_cond_guards > 1) {
        ret += " any of (";
      }
      bool first = true;
      for (unsigned key_part_idx = 0; key_part_idx < ref->key_parts;
           ++key_part_idx) {
        if (ref->cond_guards[key_part_idx] != nullptr) {
          if (!first) {
            ret += ", ";
          }
          first = false;
          ret += key->key_part[key_part_idx].field->field_name;
        }
      }
      if (num_applicable_cond_guards > 1) {
        ret += ")";
      }
      ret += " IS NULL";
      description.push_back(move(ret));
      children.push_back({path->alternative().child});
      children.push_back({path->alternative().table_scan_path});
      break;
    }
    case AccessPath::CACHE_INVALIDATOR:
      description.push_back(
          string("Invalidate materialized tables (row from ") +
          path->cache_invalidator().name + ")");
      children.push_back({path->cache_invalidator().child});
      break;
    case AccessPath::DELETE_ROWS: {
      string tables;
      for (TABLE_LIST *t = join->query_block->leaf_tables; t != nullptr;
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
      description.push_back(string("Delete from ") + tables);
      children.push_back({path->delete_rows().child});
      break;
    }
    case AccessPath::UPDATE_ROWS: {
      string tables;
      for (TABLE_LIST *t = join->query_block->leaf_tables; t != nullptr;
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
      description.push_back(string("Update ") + tables);
      children.push_back({path->update_rows().child});
      break;
    }
  }
  if (include_costs && path->num_output_rows >= 0.0) {
    double first_row_cost;
    if (path->num_output_rows <= 1.0) {
      first_row_cost = path->cost;
    } else {
      first_row_cost = path->init_cost +
                       (path->cost - path->init_cost) / path->num_output_rows;
    }

    // NOTE: We cannot use %f, since MSVC and GCC round 0.5 in different
    // directions, so tests would not be reproducible between platforms.
    // Format/round using my_gcvt() and llrint() instead.
    char first_row_cost_as_string[FLOATING_POINT_BUFFER];
    char cost_as_string[FLOATING_POINT_BUFFER];
    my_fcvt(first_row_cost, 2, first_row_cost_as_string, /*error=*/nullptr);
    my_fcvt(path->cost, 2, cost_as_string, /*error=*/nullptr);

    // Nominally, we only write number of rows as an integer.
    // However, if that should end up in zero, it's hard to know
    // whether that was 0.49 or 0.00001, so we add enough precision
    // to get one leading digit in that case.
    char rows_as_string[32];
    if (llrint(path->num_output_rows) == 0 && path->num_output_rows >= 1e-9) {
      snprintf(rows_as_string, sizeof(rows_as_string), "%.1g",
               path->num_output_rows);
    } else {
      snprintf(rows_as_string, sizeof(rows_as_string), "%lld",
               llrint(path->num_output_rows));
    }

    char str[1024];
    if (path->init_cost >= 0.0) {
      snprintf(str, sizeof(str), "  (cost=%s..%s rows=%s)",
               first_row_cost_as_string, cost_as_string, rows_as_string);
    } else {
      snprintf(str, sizeof(str), "  (cost=%s rows=%s)", cost_as_string,
               rows_as_string);
    }
    description.back() += str;
  }
  if (include_costs && current_thd->lex->is_explain_analyze) {
    if (path->iterator == nullptr) {
      description.back() += " (never executed)";
    } else {
      if (path->num_output_rows < 0.0) {
        // We always want a double space between the iterator name and the
        // costs.
        description.back().push_back(' ');
      }
      description.back().push_back(' ');
      const IteratorProfiler *const profiler = path->iterator->GetProfiler();
      if (profiler->GetNumInitCalls() == 0) {
        description.back() += "(never executed)";
      } else {
        char buf[1024];
        snprintf(buf, sizeof(buf),
                 "(actual time=%.3f..%.3f rows=%lld loops=%" PRIu64 ")",
                 profiler->GetFirstRowMs() / profiler->GetNumInitCalls(),
                 profiler->GetLastRowMs() / profiler->GetNumInitCalls(),
                 llrintf(static_cast<double>(profiler->GetNumRows()) /
                         profiler->GetNumInitCalls()),
                 profiler->GetNumInitCalls());

        description.back() += buf;
      }
    }
  }
  return {description, children};
}

string PrintQueryPlan(int level, AccessPath *path, JOIN *join,
                      bool is_root_of_join,
                      vector<string> *tokens_for_force_subplan) {
  string ret;

  if (path == nullptr) {
    ret.assign(level * 4, ' ');
    return ret + "<not executable by iterator executor>\n";
  }

  ExplainData explain = ExplainAccessPath(path, join, /*include_costs=*/true);

  int top_level = level;

  bool print_token = (tokens_for_force_subplan != nullptr);
  for (const string &str : explain.description) {
    ret.append(level * 4, ' ');
    ret += "-> ";
    if (print_token) {
      string token = GetForceSubplanToken(path, join);
      ret += '[';
      ret += token;
      ret += "] ";
      print_token = false;
      tokens_for_force_subplan->push_back(move(token));
    }
    ret += str;
    ret += "\n";
    ++level;
  }

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

  for (const ExplainData::Child &child : explain.children) {
    JOIN *subjoin = child.join != nullptr ? child.join : join;
    bool child_is_root_of_join = subjoin != join || delayed_root_of_join;
    if (!child.description.empty()) {
      ret.append(level * 4, ' ');
      ret.append("-> ");
      ret.append(child.description);
      ret.append("\n");
      ret += PrintQueryPlan(level + 1, child.path, subjoin,
                            child_is_root_of_join, tokens_for_force_subplan);
    } else {
      ret += PrintQueryPlan(level, child.path, subjoin, child_is_root_of_join,
                            tokens_for_force_subplan);
    }
  }
  if (is_root_of_join) {
    // If we know that the join will return zero rows, we don't bother
    // optimizing any subqueries in the SELECT list, but end optimization
    // early (see Query_block::optimize()). If so, don't attempt to print
    // them either, as they have no query plan.
    if (path->type == AccessPath::ZERO_ROWS) {
      return ret;
    }

    for (const auto &child : GetAccessPathsFromSelectList(join)) {
      ret.append(top_level * 4, ' ');
      ret.append("-> ");
      ret.append(child.description);
      ret.append("\n");
      ret += PrintQueryPlan(top_level + 1, child.path, child.join,
                            /*is_root_of_join=*/true, tokens_for_force_subplan);
    }
  }
  return ret;
}

// 0x
// truncated_sha256(desc1,desc2,...,[child1_desc:]0xchild1,[child2_desc:]0xchild2,...)
string GetForceSubplanToken(AccessPath *path, JOIN *join) {
  if (path == nullptr) {
    return "";
  }
  ExplainData explain = ExplainAccessPath(path, join, /*include_costs=*/false);

  string digest = explain.description[0];
  for (size_t desc_idx = 1; desc_idx < explain.description.size(); ++desc_idx) {
    digest += ',';
    digest += explain.description[desc_idx];
  }

  for (const ExplainData::Child &child : explain.children) {
    digest += ',';
    if (!child.description.empty()) {
      digest += child.description;
      digest += ':';
    }
    digest += GetForceSubplanToken(child.path, join);
  }

  unsigned char sha256sum[SHA256_DIGEST_LENGTH];
  (void)SHA_EVP256(pointer_cast<const unsigned char *>(digest.data()),
                   digest.size(), sha256sum);

  char ret[8 * 2 + 2 + 1];
  snprintf(ret, sizeof(ret), "0x%02x%02x%02x%02x%02x%02x%02x%02x", sha256sum[0],
           sha256sum[1], sha256sum[2], sha256sum[3], sha256sum[4], sha256sum[5],
           sha256sum[6], sha256sum[7]);

  return ret;
}
