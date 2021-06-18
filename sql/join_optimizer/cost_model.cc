/* Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/cost_model.h"

#include <math.h>
#include <stdio.h>
#include <algorithm>

#include "mem_root_deque.h"
#include "my_base.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_subselect.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"
#include "sql/opt_costmodel.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_planner.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "template_utils.h"

using std::min;

double EstimateCostForRefAccess(THD *thd, TABLE *table, unsigned key_idx,
                                double num_output_rows) {
  // When asking the cost model for costs, the API takes in a double,
  // but truncates it to an unsigned integer. This means that if we
  // expect an index lookup to give e.g. 0.9 rows on average, the cost
  // model will assume we get back 0 -- and even worse, InnoDB's
  // cost model gives a cost of exactly zero for this case, ignoring
  // entirely the startup cost! Obviously, a cost of zero would make
  // it very attractive to line up a bunch of such lookups in a nestloop
  // and nestloop-join against them, crowding out pretty much any other
  // way to do a join, so to counteract both of these issues, we
  // explicitly round up here. This can be removed if InnoDB's
  // cost model is tuned better for this case.
  const double hacked_num_output_rows = ceil(num_output_rows);

  const double table_scan_cost = table->file->table_scan_cost().total_cost();
  const double worst_seeks = find_worst_seeks(
      table->cost_model(), hacked_num_output_rows, table_scan_cost);
  return find_cost_for_ref(thd, table, key_idx, hacked_num_output_rows,
                           worst_seeks);
}

void EstimateSortCost(AccessPath *path, ha_rows limit_rows) {
  AccessPath *child = path->sort().child;
  const double num_input_rows = child->num_output_rows;
  const double num_output_rows =
      path->sort().use_limit ? std::min<double>(num_input_rows, limit_rows)
                             : num_input_rows;

  double sort_cost;
  if (num_input_rows <= 1.0) {
    // Avoid NaNs from log2().
    sort_cost = kSortOneRowCost;
  } else {
    // Filesort's complexity is O(n + k log k) with a limit, or O(n log n)
    // without. See comment in Filesort_buffer::sort_buffer(). We can use the
    // same calculation for both. If n = k (no limit, or the limit is higher
    // than the number of input rows), O(n + k log k) is the same as
    // O(n + n log n), which is equivalent to O(n log n) because n < n log n for
    // large values of n. So we always calculate it as n + k log k:
    sort_cost = kSortOneRowCost *
                (num_input_rows +
                 num_output_rows * std::max(log2(num_output_rows), 1.0));
  }

  path->num_output_rows = num_output_rows;
  path->cost = path->init_cost = child->cost + sort_cost;
  path->init_once_cost = 0.0;
  path->num_output_rows_before_filter = path->num_output_rows;
  path->cost_before_filter = path->cost;
}

// Estimate the width of each row produced by “query_block”,
// for temporary table materialization.
//
// See EstimateRowWidth() in make_join_hypergraph.cc.
static size_t EstimateRowWidth(const Query_block &query_block) {
  size_t ret = 0;
  for (const Item *item : query_block.fields) {
    ret += min<size_t>(item->max_length, 4096);
  }
  return ret;
}

FilterCost EstimateFilterCost(THD *thd, double num_rows, Item *condition,
                              Query_block *outer_query_block) {
  FilterCost cost{0.0, 0.0, 0.0};
  cost.cost_if_not_materialized = num_rows * kApplyOneFilterCost;
  cost.cost_if_materialized = num_rows * kApplyOneFilterCost;
  WalkItem(condition, enum_walk::POSTFIX,
           [thd, num_rows, outer_query_block, &cost](Item *item) {
             if (!IsItemInSubSelect(item)) {
               return false;
             }
             Item_in_subselect *item_subs =
                 down_cast<Item_in_subselect *>(item);

             // TODO(sgunders): Respect subquery hints, which can force the
             // strategy to be materialize.
             Query_block *query_block = item_subs->unit->first_query_block();
             const bool materializeable =
                 item_subs->subquery_allows_materialization(
                     thd, query_block, outer_query_block) &&
                 query_block->subquery_strategy(thd) ==
                     Subquery_strategy::CANDIDATE_FOR_IN2EXISTS_OR_MAT;

             AccessPath *path = item_subs->unit->root_access_path();
             if (path == nullptr) {
               // In rare situations involving IN subqueries on the left side of
               // other IN subqueries, the query block may not be part of the
               // parent query block's list of inner query blocks. If so, it has
               // not been optimized here. Since this is a rare case, we'll just
               // skip it and assign it zero cost.
               return false;
             }

             cost.cost_if_not_materialized += num_rows * path->cost;
             if (materializeable) {
               // We can't ask the handler for costs at this stage, since that
               // requires an actual TABLE, and we don't want to be creating
               // them every time we're evaluating a cost. Thus, instead,
               // we ask the cost model for an estimate. Longer-term, these two
               // estimates should really be guaranteed to be the same somehow.
               Cost_model_server::enum_tmptable_type tmp_table_type;
               if (EstimateRowWidth(*query_block) * num_rows <
                   thd->variables.max_heap_table_size) {
                 tmp_table_type = Cost_model_server::MEMORY_TMPTABLE;
               } else {
                 tmp_table_type = Cost_model_server::DISK_TMPTABLE;
               }
               cost.cost_if_materialized +=
                   thd->cost_model()->tmptable_readwrite_cost(
                       tmp_table_type, /*write_rows=*/0,
                       /*read_rows=*/num_rows);
               cost.cost_to_materialize +=
                   path->cost + kMaterializeOneRowCost * path->num_output_rows;
             } else {
               cost.cost_if_materialized += num_rows * path->cost;
             }
             return false;
           });
  return cost;
}

// Very rudimentary (assuming no deduplication; it's better to overestimate
// than to understimate), so that we get something that isn't “unknown”.
void EstimateMaterializeCost(THD *thd, AccessPath *path) {
  AccessPath *table_path = path->materialize().table_path;

  path->cost = 0;
  path->num_output_rows = 0;
  double cost_for_cacheable = 0.0;
  for (const MaterializePathParameters::QueryBlock &block :
       path->materialize().param->query_blocks) {
    if (block.subquery_path->num_output_rows >= 0.0) {
      path->num_output_rows += block.subquery_path->num_output_rows;
      path->cost += block.subquery_path->cost;
      if (block.join != nullptr && block.join->query_block->is_cacheable()) {
        cost_for_cacheable += block.subquery_path->cost;
      }
    }
  }
  path->cost += kMaterializeOneRowCost * path->num_output_rows;

  if (table_path->type == AccessPath::TABLE_SCAN) {
    table_path->num_output_rows = path->num_output_rows;
    table_path->init_cost = table_path->init_once_cost = 0.0;

    if (Overlaps(test_flags, TEST_NO_TEMP_TABLES)) {
      // Unit tests don't load any temporary table engines,
      // so just make up a number.
      table_path->cost = path->num_output_rows * 0.1;
    } else {
      TABLE dummy_table;
      TABLE *temp_table = table_path->table_scan().table;
      if (temp_table == nullptr) {
        // We need a dummy TABLE object to get estimates.
        handlerton *handlerton = ha_default_temp_handlerton(thd);
        dummy_table.file =
            handlerton->create(handlerton, /*share=*/nullptr,
                               /*partitioned=*/false, thd->mem_root);
        dummy_table.file->set_ha_table(&dummy_table);
        dummy_table.init_cost_model(thd->cost_model());
        temp_table = &dummy_table;
      }

      // Try to get usable estimates. Ignored by InnoDB, but used by
      // TempTable.
      temp_table->file->stats.records = path->num_output_rows;
      table_path->cost = temp_table->file->table_scan_cost().total_cost();
    }
  }

  path->init_cost = path->cost + std::max(table_path->init_cost, 0.0);
  path->init_once_cost = cost_for_cacheable;
  path->cost = path->cost + std::max(table_path->cost, 0.0);
}

void EstimateAggregateCost(AccessPath *path) {
  AccessPath *child = path->aggregate().child;

  // TODO(sgunders): How do we estimate how many rows aggregation
  // will be reducing the output by?
  path->num_output_rows = child->num_output_rows;
  path->init_cost = child->init_cost;
  path->init_once_cost = child->init_once_cost;
  path->cost = child->cost + kAggregateOneRowCost * child->num_output_rows;
  path->num_output_rows_before_filter = path->num_output_rows;
  path->cost_before_filter = path->cost;
  path->ordering_state = child->ordering_state;
}

double FindOutputRowsForJoin(AccessPath *left_path, AccessPath *right_path,
                             const JoinPredicate *edge,
                             double already_applied_selectivity) {
  const double outer_rows = left_path->num_output_rows;
  const double inner_rows = right_path->num_output_rows;
  const double selectivity = edge->selectivity / already_applied_selectivity;
  if (edge->expr->type == RelationalExpression::ANTIJOIN) {
    return outer_rows * (1.0 - selectivity);
  } else {
    double num_output_rows = outer_rows * inner_rows * selectivity;
    if (edge->expr->type == RelationalExpression::LEFT_JOIN) {
      num_output_rows = std::max(num_output_rows, outer_rows);
    }
    if (edge->expr->type == RelationalExpression::SEMIJOIN) {
      num_output_rows =
          std::min(num_output_rows, outer_rows / already_applied_selectivity);
      num_output_rows =
          std::min(num_output_rows, inner_rows / already_applied_selectivity);
    }
    return num_output_rows;
  }
}
