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

#ifndef SQL_JOIN_OPTIMIZER_COST_MODEL_H_
#define SQL_JOIN_OPTIMIZER_COST_MODEL_H_

#include "my_base.h"

struct AccessPath;
class Item;
struct JoinPredicate;
class Query_block;
class THD;
struct TABLE;

// These are extremely arbitrary cost model constants. We should revise them
// based on actual query times (possibly using linear regression?), and then
// put them into the cost model to make them user-tunable.
constexpr double kApplyOneFilterCost = 0.1;
constexpr double kAggregateOneRowCost = 0.1;
constexpr double kSortOneRowCost = 0.1;
constexpr double kHashBuildOneRowCost = 0.1;
constexpr double kHashProbeOneRowCost = 0.1;
constexpr double kHashReturnOneRowCost = 0.07;
constexpr double kMaterializeOneRowCost = 0.1;
constexpr double kWindowOneRowCost = 0.1;

/// See EstimateFilterCost.
struct FilterCost {
  // Cost of evaluating the filter if nothing in particular is done with it.
  double cost_if_not_materialized;

  // Cost of evaluating the filter if all subqueries in it have been
  // materialized beforehand. If there are no subqueries in the condition,
  // equals cost_if_not_materialized.
  double cost_if_materialized;

  // Cost of materializing all subqueries present in the filter.
  // If there are no subqueries in the condition, equals zero.
  double cost_to_materialize;
};

/**
  Estimate the cost of evaluating “condition”, “num_rows” times.
  This is a fairly rudimentary estimation, _but_ it includes the cost
  of any subqueries that may be present and that need evaluation.
 */
FilterCost EstimateFilterCost(THD *thd, double num_rows, Item *condition,
                              Query_block *outer_query_block);

double EstimateCostForRefAccess(THD *thd, TABLE *table, unsigned key_idx,
                                double num_output_rows);
void EstimateSortCost(AccessPath *path, ha_rows limit_rows = HA_POS_ERROR);
void EstimateMaterializeCost(THD *thd, AccessPath *path);
void EstimateAggregateCost(AccessPath *path);
double FindOutputRowsForJoin(AccessPath *left_path, AccessPath *right_path,
                             const JoinPredicate *edge,
                             double already_applied_selectivity);

#endif  // SQL_JOIN_OPTIMIZER_COST_MODEL_H_
