/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_COST_MODEL_H_
#define SQL_JOIN_OPTIMIZER_COST_MODEL_H_

#include "my_base.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/mem_root_array.h"

struct AccessPath;
struct ContainedSubquery;
class Item;
class Query_block;
class THD;
struct TABLE;

/**
   When we make cost estimates, we use this as the maximal length the
   values we get from evaluating an Item (in bytes). Actual values of
   e.g. blobs may be much longer, but even so we use this as an upper
   limit when doing cost calculations. (For context, @see Item#max_length .)
*/
constexpr size_t kMaxItemLengthEstimate = 4096;

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

/// A fallback cardinality estimate that is used in case the storage engine
/// cannot provide one (like for table functions). It's a fairly arbitrary
/// non-zero value.
constexpr ha_rows kRowEstimateFallback = 1000;

/// See EstimateFilterCost.
struct FilterCost {
  /// Cost of evaluating the filter for all rows if subqueries are not
  /// materialized.  (Note that this includes the contribution from
  /// init_cost_if_not_materialized.)
  double cost_if_not_materialized{0.0};

  /// Initial cost before the filter can be applied for the first time.
  /// Typically the cost of executing 'independent subquery' in queries like:
  /// "SELECT * FROM tab WHERE field = <independent subquery>".
  /// (That corresponds to the Item_singlerow_subselect class.)
  double init_cost_if_not_materialized{0.0};

  /// Cost of evaluating the filter for all rows if all subqueries in
  /// it have been materialized beforehand. If there are no subqueries
  /// in the condition, equals cost_if_not_materialized.
  double cost_if_materialized{0.0};

  /// Cost of materializing all subqueries present in the filter.
  /// If there are no subqueries in the condition, equals zero.
  double cost_to_materialize{0.0};
};

/// Used internally by EstimateFilterCost() only.
void AddCost(THD *thd, const ContainedSubquery &subquery, double num_rows,
             FilterCost *cost);

/**
  Estimate the cost of evaluating “condition”, “num_rows” times.
  This is a fairly rudimentary estimation, _but_ it includes the cost
  of any subqueries that may be present and that need evaluation.
 */
FilterCost EstimateFilterCost(THD *thd, double num_rows, Item *condition,
                              const Query_block *outer_query_block);

/**
  A cheaper overload of EstimateFilterCost() that assumes that all
  contained subqueries have already been extracted (ie., it skips the
  walking, which can be fairly expensive). This data is typically
  computed by FindContainedSubqueries().
 */
inline FilterCost EstimateFilterCost(
    THD *thd, double num_rows,
    const Mem_root_array<ContainedSubquery> &contained_subqueries) {
  FilterCost cost;
  cost.cost_if_not_materialized = num_rows * kApplyOneFilterCost;
  cost.cost_if_materialized = num_rows * kApplyOneFilterCost;

  for (const ContainedSubquery &subquery : contained_subqueries) {
    AddCost(thd, subquery, num_rows, &cost);
  }
  return cost;
}

double EstimateCostForRefAccess(THD *thd, TABLE *table, unsigned key_idx,
                                double num_output_rows);

/**
  Estimate costs and output rows for a SORT AccessPath.
  @param thd Current thread.
  @param path the AccessPath.
  @param distinct_rows An estimate of the number of distinct rows, if
     remove_duplicates==true and we have an estimate already.
*/
void EstimateSortCost(THD *thd, AccessPath *path,
                      double distinct_rows = kUnknownRowCount);

void EstimateMaterializeCost(THD *thd, AccessPath *path);

/**
   Estimate the number of rows with a distinct combination of values for
   'terms'. @see EstimateDistinctRowsFromStatistics for additional details.
   @param thd The current thread.
   @param child_rows The number of input rows.
   @param terms The terms for which we estimate the number of unique
                combinations.
   @returns The estimated number of output rows.
*/
double EstimateDistinctRows(THD *thd, double child_rows,
                            Bounds_checked_array<const Item *const> terms);
/**
   Estimate costs and result row count for an aggregate operation.
   @param[in,out] thd The current thread.
   @param[in,out] path The AGGREGATE path.
   @param[in] query_block The Query_block to which 'path' belongs.
 */
void EstimateAggregateCost(THD *thd, AccessPath *path,
                           const Query_block *query_block);
void EstimateDeleteRowsCost(AccessPath *path);
void EstimateUpdateRowsCost(AccessPath *path);

/// Estimate the costs and row count for a STREAM AccessPath.
void EstimateStreamCost(AccessPath *path);

/**
   Estimate the costs and row count for a WINDOW AccessPath. As described in
   @see AccessPath::m_init_cost, the cost to read k out of N rows would be
   init_cost + (k/N) * (cost - init_cost).
*/
void EstimateLimitOffsetCost(AccessPath *path);

/// Estimate the costs and row count for a WINDOW AccessPath.
void EstimateWindowCost(AccessPath *path);

/**
   Estimate the fan out for a left semijoin or a left antijoin. The fan out
   is defined as the number of result rows, divided by the number of input
   rows from the left hand relation. For a semijoin, J1:

   SELECT ... FROM t1 WHERE EXISTS (SELECT ... FROM t2 WHERE predicate)

   we know that the fan out of the corresponding inner join J2:

   SELECT ... FROM t1, t2 WHERE predicate

   is: F(J2) = CARD(t2) * SELECTIVITY(predicate) , where CARD(t2)=right_rows,
   and SELECTIVITY(predicate)=edge.selectivity. If 'predicate' is a
   deterministic function of t1 and t2 rows, then J1 is equivalent to an inner
   join J3:

   SELECT ... FROM t1 JOIN (SELECT DISTINCT f1,..fn FROM t2) d ON predicate

   where f1,..fn are those fields from t2 that appear in the predicate.

   Then F(J1) = F(J3) = F(J2) * CARD(d) / CARD(t2)
        = CARD(d) * SELECTIVITY(predicate).

   This function therefore collects f1..fn and estimates CARD(d). As a special
   case, 'predicate' may be independent of t2. The query is then equivalent to:

   SELECT ... FROM t1 WHERE predicate AND (SELECT COUNT(*) FROM t2) > 0

   The fan out is then the selectivity of 'predicate' multiplied by the
   probability of t2 having at least one row.

   @param thd The current thread.
   @param right_rows The number of input rows from the right hand relation.
   @param edge Join edge.
   @returns fan out.
 */
double EstimateSemijoinFanOut(THD *thd, double right_rows,
                              const JoinPredicate &edge);

/**
   Estimate the number of output rows from joining two relations.
   @param thd The current thread.
   @param left_rows Number of rows in the left hand relation.
   @param right_rows Number of rows in the right hand relation.
   @param edge The join between the two relations.
*/
inline double FindOutputRowsForJoin(THD *thd, double left_rows,
                                    double right_rows,
                                    const JoinPredicate *edge) {
  switch (edge->expr->type) {
    case RelationalExpression::LEFT_JOIN:
      // For outer joins, every outer row produces at least one row (if none
      // are matching, we get a NULL-complemented row).
      // Note that this can cause inconsistent row counts; see bug #33550360
      // and/or JoinHypergraph::has_reordered_left_joins.
      return left_rows * std::max(right_rows * edge->selectivity, 1.0);

    case RelationalExpression::SEMIJOIN:
      return left_rows * EstimateSemijoinFanOut(thd, right_rows, *edge);

    case RelationalExpression::ANTIJOIN:
      // Antijoin are estimated as simply the opposite of semijoin (see above),
      // but wrongly estimating 0 rows (or, of course, a negative amount) could
      // be really bad, so we assume at least 10% coming out as a fudge factor.
      // It's better to estimate too high than too low here.
      return left_rows *
             std::max(1.0 - EstimateSemijoinFanOut(thd, right_rows, *edge),
                      0.1);

    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
      return left_rows * right_rows * edge->selectivity;

    case RelationalExpression::FULL_OUTER_JOIN:   // Not implemented.
    case RelationalExpression::MULTI_INNER_JOIN:  // Should not appear here.
    case RelationalExpression::TABLE:             // Should not appear here.
      assert(false);
      return 0;

    default:
      assert(false);
      return 0;
  }
}

#endif  // SQL_JOIN_OPTIMIZER_COST_MODEL_H_
