/* Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/access_path.h"

#include "sql/basic_row_iterators.h"
#include "sql/bka_iterator.h"
#include "sql/composite_iterators.h"
#include "sql/filesort.h"
#include "sql/hash_join_iterator.h"
#include "sql/item_sum.h"
#include "sql/opt_range.h"
#include "sql/ref_row_iterators.h"
#include "sql/sorting_iterator.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "sql/timing_iterator.h"

#include <functional>
#include <string>
#include <vector>

using std::function;
using std::string;
using std::vector;

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

static string JoinTypeToString(JoinType join_type) {
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

static string HashJoinTypeToString(JoinType join_type) {
  switch (join_type) {
    case JoinType::INNER:
      return "Inner hash join";
    case JoinType::OUTER:
      return "Left hash join";
    case JoinType::ANTI:
      return "Hash antijoin";
    case JoinType::SEMI:
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
    SELECT_LEX *select_lex = subselect->unit->first_select();
    char description[256];
    if (select_lex->is_dependent()) {
      snprintf(description, sizeof(description),
               "Select #%d (subquery in %s; dependent)",
               select_lex->select_number, source_text);
    } else if (!select_lex->is_cacheable()) {
      snprintf(description, sizeof(description),
               "Select #%d (subquery in %s; uncacheable)",
               select_lex->select_number, source_text);
    } else {
      snprintf(description, sizeof(description),
               "Select #%d (subquery in %s; run only once)",
               select_lex->select_number, source_text);
    }
    AccessPath *path;
    if (subselect->unit->root_access_path() != nullptr) {
      path = subselect->unit->root_access_path();
    } else {
      path = subselect->unit->item->root_access_path();
    }
    children->push_back({path, description, select_lex->join});
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

ExplainData ExplainAccessPath(const AccessPath *path, JOIN *join);

// The table iterator could be a whole string of iterators
// (sort, filter, etc.) due to add_sorting_to_table(), so show them all.
//
// TODO(sgunders): Make the optimizer put these on top of the
// MaterializeIterator instead (or perhaps better yet, on the subquery
// iterator), so that table_iterator is always just a single basic iterator.
static void AddTableIteratorDescription(const AccessPath *path, JOIN *join,
                                        vector<string> *description) {
  const AccessPath *subpath = path;
  for (;;) {
    ExplainData explain = ExplainAccessPath(subpath, join);
    for (string str : explain.description) {
      if (explain.children.size() > 1) {
        // This can happen if e.g. a filter has subqueries in it.
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
                                         vector<ExplainData::Child> *children) {
  MaterializePathParameters *param = path->materialize().param;

  AddTableIteratorDescription(path->materialize().table_path, join,
                              description);

  const bool is_union = param->query_blocks.size() > 1;
  string str;

  if (param->cte != nullptr && param->cte->recursive) {
    str = "Materialize recursive CTE " + to_string(param->cte->name);
  } else if (param->cte != nullptr) {
    if (is_union) {
      str = "Materialize union CTE " + to_string(param->cte->name);
    } else {
      str = "Materialize CTE " + to_string(param->cte->name);
    }
    if (param->cte->tmp_tables.size() > 1) {
      str += " if needed";
      if (param->cte->tmp_tables[0]->table != param->table) {
        // See children().
        str += " (query plan printed elsewhere)";
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
  if (param->cte != nullptr &&
      param->cte->tmp_tables[0]->table != param->table) {
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

static void AddChildrenFromPushedCondition(
    const TABLE *table, vector<ExplainData::Child> *children) {
  /*
    A table access path is normally a leaf node in the set of paths.
    The exception is if a subquery was included as part of an
    'engine_condition_pushdown'. In such cases the subquery has
    been evaluated prior to acessing this table, and the result(s)
    from the subquery materialized into the pushed condition.
    Report such subqueries as children of this table.
  */
  Item *pushed_cond = const_cast<Item *>(table->file->pushed_cond);

  if (pushed_cond != nullptr) {
    GetAccessPathsFromItem(pushed_cond, "pushed condition", children);
  }
}

ExplainData ExplainAccessPath(const AccessPath *path, JOIN *join) {
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
      string str =
          string("Index scan on ") + table->alias + " using " + key->name;
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
      string str = string("Index lookup on ") + table->alias + " using " +
                   key->name + " (" +
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
          string("Index lookup on ") + table->alias + " using " + key->name +
          " (" +
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
          string("Single-row index lookup on ") + table->alias + " using " +
          key->name + " (" +
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
      DBUG_ASSERT(table->file->pushed_idx_cond == nullptr);
      const KEY *key = &table->key_info[path->pushed_join_ref().ref->key];
      string str;
      if (path->pushed_join_ref().is_unique) {
        str = string("Single-row index");
      } else {
        str = string("Index");
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
      DBUG_ASSERT(table->file->pushed_idx_cond == nullptr);
      const KEY *key = &table->key_info[path->full_text_search().ref->key];
      description.push_back(string("Indexed full text search on ") +
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
      string str = string("Multi-range index lookup on ") + table->alias +
                   " using " + key->name + " (" +
                   RefToString(*path->mrr().ref, key, /*include_nulls=*/false) +
                   ")";
      if (table->file->pushed_idx_cond != nullptr) {
        str += ", with index condition: " +
               ItemToString(table->file->pushed_idx_cond);
      }
      if (path->mrr().cache_idx_cond != nullptr) {
        str += ", with dependent index condition: " +
               ItemToString(path->mrr().cache_idx_cond);
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
      TABLE *table = path->index_range_scan().table;
      // TODO(sgunders): Convert QUICK_SELECT_I to RowIterator so that we can
      // get better outputs here (similar to dbug_dump()).
      String str;
      path->index_range_scan().quick->add_info_string(&str);
      string ret = string("Index range scan on ") + table->alias + " using " +
                   to_string(str);
      if (table->file->pushed_idx_cond != nullptr) {
        ret += ", with index condition: " +
               ItemToString(table->file->pushed_idx_cond);
      }
      ret += table->file->explain_extra();
      description.push_back(move(ret));
      AddChildrenFromPushedCondition(table, &children);
      break;
    }
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN: {
      TABLE *table = path->dynamic_index_range_scan().table;
      // TODO(sgunders): Convert QUICK_SELECT_I to RowIterator so that we can
      // get better outputs here (similar to dbug_dump()), although it might get
      // tricky when there are many alternatives.
      string str = string("Index range scan on ") + table->alias +
                   " (re-planned for each iteration)";
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
      string ret = HashJoinTypeToString(predicate->type);

      if (predicate->equijoin_conditions.empty()) {
        ret.append(" (no condition)");
      } else {
        for (Item_func_eq *cond : predicate->equijoin_conditions) {
          if (cond != predicate->equijoin_conditions[0]) {
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
      for (Item *cond : predicate->join_conditions) {
        if (cond == predicate->join_conditions[0]) {
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
          ret += ItemToString(
              down_cast<Item_rollup_sum_switcher *>(*item)->master());
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
      ExplainData table_explain =
          ExplainAccessPath(path->temptable_aggregate().table_path, join);
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
      ExplainMaterializeAccessPath(path, join, &description, &children);
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
    case AccessPath::WINDOWING: {
      string buf;
      if (path->windowing().needs_buffering) {
        Window *window = path->windowing().temp_table_param->m_window;
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
           *(path->windowing().temp_table_param->items_to_copy)) {
        if (func.func()->m_is_window_function) {
          if (!first) {
            buf += ", ";
          }
          buf += ItemToString(func.func());
          first = false;
        }
      }
      description.push_back(move(buf));
      children.push_back({path->windowing().child});
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
    case AccessPath::REMOVE_DUPLICATES:
      description.push_back(string("Remove duplicates from input sorted on ") +
                            path->remove_duplicates().key->name);
      children.push_back({path->remove_duplicates().child});
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
  }
  if (path->num_output_rows >= 0.0) {
    // NOTE: We cannot use %f, since MSVC and GCC round 0.5 in different
    // directions, so tests would not be reproducible between platforms.
    // Format/round using my_gcvt() and llrint() instead.
    char cost_as_string[FLOATING_POINT_BUFFER];
    my_fcvt(path->cost, 2, cost_as_string, /*error=*/nullptr);
    char str[512];
    snprintf(str, sizeof(str), "  (cost=%s rows=%lld)", cost_as_string,
             llrint(path->num_output_rows));
    description.back() += str;
  }
  if (current_thd->lex->is_explain_analyze && path->iterator != nullptr) {
    if (path->num_output_rows < 0.0) {
      // We always want a double space between the iterator name and the costs.
      description.back().push_back(' ');
    }
    description.back().push_back(' ');
    description.back() += path->iterator->TimingString();
  }
  return {description, children};
}

string PrintQueryPlan(int level, AccessPath *path, JOIN *join,
                      bool is_root_of_join) {
  string ret;

  if (path == nullptr) {
    ret.assign(level * 4, ' ');
    return ret + "<not executable by iterator executor>\n";
  }

  ExplainData explain = ExplainAccessPath(path, join);

  int top_level = level;

  for (const string &str : explain.description) {
    ret.append(level * 4, ' ');
    ret += "-> ";
    ret += str;
    ret += "\n";
    ++level;
  }

  for (const ExplainData::Child &child : explain.children) {
    JOIN *subjoin = child.join != nullptr ? child.join : join;
    bool child_is_root_of_join = subjoin != join;
    if (!child.description.empty()) {
      ret.append(level * 4, ' ');
      ret.append("-> ");
      ret.append(child.description);
      ret.append("\n");
      ret +=
          PrintQueryPlan(level + 1, child.path, subjoin, child_is_root_of_join);
    } else {
      ret += PrintQueryPlan(level, child.path, subjoin, child_is_root_of_join);
    }
  }
  if (is_root_of_join) {
    // If we know that the join will return zero rows, we don't bother
    // optimizing any subqueries in the SELECT list, but end optimization
    // early (see SELECT_LEX::optimize()). If so, don't attempt to print
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
                            /*is_root_of_join=*/true);
    }
  }
  return ret;
}
