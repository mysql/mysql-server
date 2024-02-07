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

#include "sql/join_optimizer/cost_model.h"

#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <bit>
#include <iterator>

#include "mem_root_deque.h"
#include "my_base.h"
#include "sql/handler.h"
#include "sql/item_func.h"
#include "sql/item_subselect.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/find_contained_subqueries.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/join_optimizer/overflow_bitset.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"
#include "sql/opt_costmodel.h"
#include "sql/opt_trace.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_planner.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "template_utils.h"

using std::min;
using std::popcount;
using std::string;

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

  // We call find_cost_for_ref(), which is the same cost model used
  // in the old join optimizer, but without the “worst_seek” cap,
  // which gives ref access with high row counts an artificially low cost.
  // Removing this cap hurts us a bit if the buffer pool gets filled
  // with useful data _while running this query_, but it is just a really
  // bad idea overall, that makes the join optimizer prefer such plans
  // by a mile. The original comment says that it's there to prevent
  // choosing table scan too often, but table scans are not a problem
  // if we hash join on them. (They can be dangerous with nested-loop
  // joins, though!)
  //
  // TODO(sgunders): This is still a very primitive, and rather odd,
  // cost model. In particular, why don't we ask the storage engine for
  // the cost of scanning non-covering secondary indexes?
  return find_cost_for_ref(thd, table, key_idx, hacked_num_output_rows,
                           /*worst_seeks=*/DBL_MAX);
}

void EstimateSortCost(THD *thd, AccessPath *path, double distinct_rows) {
  const auto &sort{path->sort()};
  assert(sort.remove_duplicates || distinct_rows == kUnknownRowCount);

  const double limit{sort.limit == HA_POS_ERROR
                         ? std::numeric_limits<double>::max()
                         : sort.limit};

  const double num_input_rows{sort.child->num_output_rows()};

  if (sort.remove_duplicates && distinct_rows == kUnknownRowCount) {
    Prealloced_array<const Item *, 4> sort_items(PSI_NOT_INSTRUMENTED);
    for (const ORDER *order = sort.order; order != nullptr;
         order = order->next) {
      sort_items.push_back(*order->item);
    }

    distinct_rows = EstimateDistinctRows(
        thd, num_input_rows, {sort_items.cbegin(), sort_items.size()});
  }

  /*
    If remove_duplicates is set, we incur the cost of sorting the entire
    input, even if 'limit' is set. (See check_if_pq_applicable() for details.)
   */
  const double sort_result_rows{sort.remove_duplicates
                                    ? num_input_rows
                                    : std::min(limit, num_input_rows)};

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
                 sort_result_rows * std::max(log2(sort_result_rows), 1.0));
  }

  path->set_cost(sort.child->cost() + sort_cost);
  path->set_init_cost(path->cost());
  path->set_init_once_cost(0.0);

  path->set_num_output_rows(sort.remove_duplicates
                                ? std::min(distinct_rows, limit)
                                : std::min(num_input_rows, limit));

  path->num_output_rows_before_filter = path->num_output_rows();
  path->set_cost_before_filter(path->cost());
}

void AddCost(THD *thd, const ContainedSubquery &subquery, double num_rows,
             FilterCost *cost) {
  switch (subquery.strategy) {
    case ContainedSubquery::Strategy::kMaterializable: {
      // We can't ask the handler for costs at this stage, since that
      // requires an actual TABLE, and we don't want to be creating
      // them every time we're evaluating a cost-> Thus, instead,
      // we ask the cost model for an estimate. Longer-term, these two
      // estimates should really be guaranteed to be the same somehow.
      Cost_model_server::enum_tmptable_type tmp_table_type;
      if (subquery.row_width * num_rows < thd->variables.max_heap_table_size) {
        tmp_table_type = Cost_model_server::MEMORY_TMPTABLE;
      } else {
        tmp_table_type = Cost_model_server::DISK_TMPTABLE;
      }
      cost->cost_if_materialized += thd->cost_model()->tmptable_readwrite_cost(
          tmp_table_type, /*write_rows=*/0,
          /*read_rows=*/num_rows);
      cost->cost_to_materialize +=
          subquery.path->cost() +
          kMaterializeOneRowCost * subquery.path->num_output_rows();

      cost->cost_if_not_materialized += num_rows * subquery.path->cost();
    } break;

    case ContainedSubquery::Strategy::kNonMaterializable:
      cost->cost_if_not_materialized += num_rows * subquery.path->cost();
      cost->cost_if_materialized += num_rows * subquery.path->cost();
      break;

    case ContainedSubquery::Strategy::kIndependentSingleRow:
      cost->cost_if_materialized += subquery.path->cost();
      cost->cost_if_not_materialized += subquery.path->cost();
      cost->init_cost_if_not_materialized += subquery.path->cost();
      break;

    default:
      assert(false);
  }
}

FilterCost EstimateFilterCost(THD *thd, double num_rows, Item *condition,
                              const Query_block *outer_query_block) {
  FilterCost cost;
  cost.cost_if_not_materialized = num_rows * kApplyOneFilterCost;
  cost.cost_if_materialized = num_rows * kApplyOneFilterCost;
  FindContainedSubqueries(
      condition, outer_query_block,
      [thd, num_rows, &cost](const ContainedSubquery &subquery) {
        AddCost(thd, subquery, num_rows, &cost);
      });
  return cost;
}

// Very rudimentary (assuming no deduplication; it's better to overestimate
// than to understimate), so that we get something that isn't “unknown”.
void EstimateMaterializeCost(THD *thd, AccessPath *path) {
  AccessPath *table_path = path->materialize().table_path;
  double &subquery_cost = path->materialize().subquery_cost;

  path->set_num_output_rows(0);
  double cost_for_cacheable = 0.0;
  bool left_operand = true;
  subquery_cost = 0.0;
  for (const MaterializePathParameters::Operand &operand :
       path->materialize().param->m_operands) {
    if (operand.subquery_path->num_output_rows() >= 0.0) {
      // For INTERSECT and EXCEPT we can never get more rows than we have in
      // the left block, so do not add unless we are looking at left block or
      // we have a UNION.
      if (left_operand || path->materialize().param->table == nullptr ||
          path->materialize().param->table->is_union_or_table()) {
        path->set_num_output_rows(path->num_output_rows() +
                                  operand.subquery_path->num_output_rows());
      } else if (!left_operand &&
                 path->materialize().param->table->is_intersect()) {
        // INTERSECT can never give more rows than that of its smallest operand
        path->set_num_output_rows(std::min(
            path->num_output_rows(), operand.subquery_path->num_output_rows()));
      }
      // For implicit grouping operand.subquery_path->num_output_rows() may be
      // set (to 1.0) even if operand.subquery_path->cost is undefined (cf.
      // Bug#35240913).
      if (operand.subquery_path->cost() > 0.0) {
        subquery_cost += operand.subquery_path->cost();
        if (operand.join != nullptr &&
            operand.join->query_block->is_cacheable()) {
          cost_for_cacheable += operand.subquery_path->cost();
        }
      }
    }
    left_operand = false;
  }

  if (table_path->type == AccessPath::TABLE_SCAN) {
    path->set_cost(0.0);
    path->set_init_cost(0.0);
    path->set_init_once_cost(0.0);
    table_path->set_num_output_rows(path->num_output_rows());
    table_path->set_init_cost(subquery_cost);
    table_path->set_init_once_cost(cost_for_cacheable);

    if (Overlaps(test_flags, TEST_NO_TEMP_TABLES)) {
      // Unit tests don't load any temporary table engines,
      // so just make up a number.
      table_path->set_cost(subquery_cost + path->num_output_rows() * 0.1);
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
      temp_table->file->stats.records =
          min(path->num_output_rows(), LLONG_MAX_DOUBLE);
      table_path->set_cost(subquery_cost +
                           temp_table->file->table_scan_cost().total_cost());
    }
  } else {
    // Use the costs of the subquery.
    path->set_init_cost(subquery_cost);
    path->set_init_once_cost(cost_for_cacheable);
    path->set_cost(subquery_cost);
  }

  path->set_init_cost(path->init_cost() +
                      std::max(table_path->init_cost(), 0.0) +
                      kMaterializeOneRowCost * path->num_output_rows());

  path->set_init_once_cost(path->init_once_cost() +
                           std::max(table_path->init_once_cost(), 0.0));

  path->set_cost(path->cost() + std::max(table_path->cost(), 0.0) +
                 kMaterializeOneRowCost * path->num_output_rows());
}

namespace {

/// Array of aggregation terms.
using TermArray = Bounds_checked_array<const Item *const>;

/**
   This class finds disjoint sets of aggregation terms that form prefixes of
   some non-hash index, and makes row estimates for those sets based on index
   metadata.
*/
class AggregateRowEstimator {
 public:
  /// @param thd Current thread.
  /// @param terms The aggregation terms.
  AggregateRowEstimator(THD *thd, TermArray terms);

  // No copying of this type.
  AggregateRowEstimator(const AggregateRowEstimator &) = delete;
  AggregateRowEstimator &operator=(const AggregateRowEstimator &) = delete;

  /// Used to indicate that no more suitable indexes could be found.
  static constexpr double kNoEstimate = -1.0;

  /**
      Get the next row estimate. We make the estimate as follows:

      1. Find the (non-hash) index where the remaining aggregation terms form
         the longest prefix of the index fields. For example, if we have
         aggregation terms [a,b,c,d] and we have indexes ix1 on [a,b], ix2 on
         [d,c,b,e], we pick ix2.

      2. Make an estimate of the number of distinct values for those fields
         (i.e. [d,c,b]) using index statistics. This is the row estimate.

      3. Remove those fields from the set of remaining terms. (In the example
         above, only [a] would now remain.

      4. Return the row estimate to the caller.

      @returns The estimate, or kNoEstimate if no more suitable indexes could be
      found.
  */
  double MakeNextEstimate(THD *thd);

  /// Get the set of terms for which we have found an index.
  /// Bit number corresponds to position in the 'terms' argument to the
  /// constructor.
  const MutableOverflowBitset &GetConsumedTerms() const {
    return m_consumed_terms;
  }

 private:
  /// A prefix of some key where each key_part corresponds to an aggregation
  /// term.
  struct Prefix {
    /// The key (index).
    const KEY *m_key;
    /// The number of key_parts found in 'terms'.
    uint m_length;

    /// @returns A string representation of this object (for optimizer trace).
    string Print() const;
  };

  ///  The aggregation terms.
  TermArray m_terms;

  /// The set of terms mapped to an index so far.
  MutableOverflowBitset m_consumed_terms;

  /// The index prefixes we found for 'm_terms'.
  Mem_root_array<Prefix *> m_prefixes;

  /// Find an Item_field pointing to 'field' in 'm_terms', if there is one.
  /// @param field The field we look for.
  /// @returns An iterator to the position of 'field' in m_terms, or
  /// m_terms.cend().
  TermArray::const_iterator FindField(const Field *field) const {
    return std::find_if(
        m_terms.cbegin(), m_terms.cend(), [field](const Item *item) {
          assert(field != nullptr);
          return item->type() == Item::FIELD_ITEM &&
                 down_cast<const Item_field *>(item)->field == field;
        });
  }
};

AggregateRowEstimator::AggregateRowEstimator(THD *thd, TermArray terms)
    : m_terms{terms},
      m_consumed_terms{thd->mem_root, terms.size()},
      m_prefixes{thd->mem_root} {
  /* Find keys (indexes) for which:
     - One or more of 'terms' form a prefix of the key.
     - Records per key estimates are available for some prefix of the key.
  */
  for (const Item *term : terms) {
    const Item *aggregate_term = term->real_item();

    if (aggregate_term->type() == Item::FIELD_ITEM) {
      // aggregate_term is a field, so it may be the first field of an index.
      const Field *const field =
          down_cast<const Item_field *>(aggregate_term)->field;
      Key_map key_map = field->key_start;
      uint key_idx = key_map.get_first_set();

      // Loop over the indexes where aggregate_term is the first field.
      while (key_idx != MY_BIT_NONE) {
        const KEY *const key = &field->table->key_info[key_idx];
        uint key_part_no = 1;

        if (key->has_records_per_key(0)) {
          /*
            Find the number of aggregation terms that form a prefix of 'key'
            and allows records_per_key to be calculated.
          */
          while (key_part_no < key->actual_key_parts &&
                 key->has_records_per_key(key_part_no) &&
                 FindField(key->key_part[key_part_no].field) != terms.end()) {
            key_part_no++;
          }

          m_prefixes.push_back(new (thd->mem_root) Prefix({key, key_part_no}));
          if (TraceStarted(thd)) {
            Trace(thd) << "Adding prefix: " << m_prefixes.back()->Print()
                       << "\n";
          }
        }
        key_map.clear_bit(key_idx);
        key_idx = key_map.get_first_set();
      }
    }
  }
}

double AggregateRowEstimator::MakeNextEstimate(THD *thd) {
  // Pick the longest prefix until we have used all terms or m_prefixes,
  // or until all prefixes have length==0.
  while (m_terms.size() >
             static_cast<size_t>(PopulationCount(m_consumed_terms)) &&
         !m_prefixes.empty()) {
    // Find the longest prefix.
    auto prefix_iter = std::max_element(m_prefixes.begin(), m_prefixes.end(),
                                        [](const Prefix *a, const Prefix *b) {
                                          return a->m_length < b->m_length;
                                        });

    Prefix *const prefix = *prefix_iter;

    if (prefix->m_length == 0) {
      return kNoEstimate;
    }

    bool terms_missing = false;

    for (uint key_part_no = 0; key_part_no < prefix->m_length; key_part_no++) {
      Field *const field = prefix->m_key->key_part[key_part_no].field;
      /*
        For each KEY_PART, check if there is still a corresponding aggregation
        item in m_terms.
      */
      if (IsBitSet(FindField(field) - m_terms.cbegin(), m_consumed_terms)) {
        // We did not find it, so it must have been removed when we examined
        // some earlier key. We can thus only use the prefix 0..key_part_no of
        // this key.
        const Prefix shortened_prefix{prefix->m_key, key_part_no};
        if (TraceStarted(thd)) {
          Trace(thd) << "Shortening prefix " << prefix->Print() << "\n  into  "
                     << shortened_prefix.Print() << ",\n  since field '"
                     << field->field_name
                     << "' is already covered by an earlier estimate.\n";
        }
        *prefix = shortened_prefix;
        terms_missing = true;
        break;
      }
    }

    if (!terms_missing) {
      m_prefixes.erase(prefix_iter);

      for (uint key_part_no = 0; key_part_no < prefix->m_length;
           key_part_no++) {
        // Remove the term, so that we do not use two indexes to estimate the
        // row count from a single term.
        m_consumed_terms.SetBit(
            FindField(prefix->m_key->key_part[key_part_no].field) -
            m_terms.begin());
      }

      assert(prefix->m_key->records_per_key(prefix->m_length - 1) !=
             REC_PER_KEY_UNKNOWN);

      const double row_estimate =
          prefix->m_key->table->file->stats.records /
          prefix->m_key->records_per_key(prefix->m_length - 1);

      if (TraceStarted(thd)) {
        Trace(thd) << "Choosing longest prefix " << prefix->Print()
                   << " with estimated distinct values: "
                   << StringPrintf("%.1f", row_estimate) << "\n";
      }

      return row_estimate;
    }
  }

  return kNoEstimate;
}

string AggregateRowEstimator::Prefix::Print() const {
  string result("[index: '");
  result += m_key->name;
  result += "' on '";
  result += m_key->table->alias;
  result += "', fields: '";

  for (uint i = 0; i < m_length; i++) {
    if (i > 0) {
      result += "', '";
    }
    result += m_key->key_part[i].field->field_name;
  }

  result += "']";
  return result;
}

TermArray GetAggregationTerms(const JOIN &join) {
  auto terms = Bounds_checked_array<const Item *>::Alloc(
      join.thd->mem_root, join.group_fields.size());

  // JOIN::group_fields contains the grouping expressions in reverse order.
  // While the order does not matter for regular GROUP BY, it may affect the
  // number of output rows for ROLLUP. Reverse the order again so that the terms
  // have the same order as in the query text.
  transform(join.group_fields.cbegin(), join.group_fields.cend(),
            std::make_reverse_iterator(terms.end()),
            [](const Cached_item &cached) {
              return unwrap_rollup_group(cached.get_item());
            });

  return {terms.data(), terms.size()};
}

/**
 Estimate the number of distinct tuples in the projection defined by
 'terms'.  We use the following data to make a row estimate, in that
 priority:

 1. (Non-hash) indexes where the terms form some prefix of the
  index key. The handler can give good estimates for these.

 2. Histograms for terms that are fields. The histograms
 give an estimate of the number of unique values.

 3. The table size (in rows) for terms that are fields without histograms.
 (If we have "SELECT ... FROM t1 JOIN t2 GROUP BY t2.f1", there cannot
 be more results rows than there are rows in t2.) We also make the
 pragmatic assumption that that field values are not unique, and
 therefore make a row estimate somewhat lower than the table row count.

 4. In the remaining cases we make an estimate based on the input row
 estimate. This is based on two assumptions: a) There will be fewer output
 rows than input rows, as one rarely aggregates on a set of terms that are
 unique for each row, b) The more terms there are, the more output rows one
 can expect.

 We may need to combine multiple estimates into one. As an example,
 assume that we aggregate on three fields: f1, f2 and f3. There is and index
 where f1, f2 are a key prefix, and we have a histogram on f3. Then we
 could make good estimates for "GROUP BY f1,f2" or "GROUP BY f3". But how
 do we combine these into an estimate for "GROUP BY f1,f2,f3"? If f3 and
 f1,f2 are uncorrelated, then we should multiply the individual estimates.
 But if f3 is functionally dependent on f1,f2 (or vice versa), we should
 pick the larger of the two estimates.

 Since we do not know if these fields are correlated or not, we
 multiply the individual estimates and then multiply with a
 damping factor. The damping factor is a function of the number
 of estimates (two in the example above). That way, we get a
 combined estimate that falls between the two extremes of
 functional dependence and no correlation.

@param thd Current thread.
@param terms The terms for which we estimate the number of distinct
             combinations.
@param child_rows The row estimate for the input path.
@returns The row estimate for the aggregate operation.
*/
double EstimateDistinctRowsFromStatistics(THD *thd, TermArray terms,
                                          double child_rows) {
  // Estimated number of output rows.
  double output_rows = 1.0;
  // No of individual estimates (for disjoint subsets of the terms).
  size_t estimate_count = 0;
  // The largest individual estimate.
  double top_estimate = 1.0;

  // Make row estimates for sets of terms that form prefixes
  // of (non-hash) indexes.
  AggregateRowEstimator index_estimator(thd, terms);

  while (true) {
    const double distinct_values = index_estimator.MakeNextEstimate(thd);
    if (distinct_values == AggregateRowEstimator::kNoEstimate) {
      break;
    }
    top_estimate = std::max(distinct_values, top_estimate);
    output_rows *= distinct_values;
    estimate_count++;
  }

  size_t remaining_term_cnt =
      terms.size() - PopulationCount(index_estimator.GetConsumedTerms());

  // Loop over the remaining terms, i.e. those that were not part of
  // a key prefix. Make row estimates for those that are fields.
  for (TermArray::const_iterator term = terms.cbegin(); term < terms.cend();
       term++) {
    if (!IsBitSet(term - terms.cbegin(), index_estimator.GetConsumedTerms()) &&
        (*term)->type() == Item::FIELD_ITEM) {
      const Field *const field = down_cast<const Item_field *>(*term)->field;
      const histograms::Histogram *const histogram =
          field->table->find_histogram(field->field_index());

      double distinct_values;
      if (histogram == nullptr || empty(*histogram)) {
        // Make an estimate from the table row count.
        distinct_values = std::sqrt(field->table->file->stats.records);

        if (TraceStarted(thd)) {
          Trace(thd) << StringPrintf(
              "Estimating %.1f distinct values for field '%s'"
              " from table size.\n",
              distinct_values, field->field_name);
        }

      } else {
        // If 'term' is a field with a histogram, use that to get a row
        // estimate.
        distinct_values = histogram->get_num_distinct_values();

        if (histogram->get_null_values_fraction() > 0.0) {
          // If there are NULL values, those will also form distinct
          // combinations of terms.
          ++distinct_values;
        }

        if (TraceStarted(thd)) {
          Trace(thd) << StringPrintf(
              "Estimating %.1f distinct values for field '%s'"
              " from histogram.\n",
              distinct_values, field->field_name);
        }
      }

      top_estimate = std::max(distinct_values, top_estimate);
      output_rows *= distinct_values;
      remaining_term_cnt--;
      estimate_count++;
    }
  }

  // Multiplying individual estimates gives too many rows if distinct estimates
  // covers dependent terms. We apply a damping formula to compensate
  // for this.
  output_rows = top_estimate * std::pow(output_rows / top_estimate, 0.67);

  // Multiply with an estimate for any non-field terms.
  const double non_field_values =
      std::pow(child_rows, remaining_term_cnt / (remaining_term_cnt + 1.0));

  output_rows *= non_field_values;

  // The estimate could exceed 'child_rows' if there e.g. is a restrictive
  // WHERE-condition, as estimates from indexes or histograms will not reflect
  // that.
  if (estimate_count > 1 || (estimate_count == 1 && remaining_term_cnt > 0)) {
    // Combining estimates from different sources introduces uncertainty.
    // We therefore assume that there will be some reduction in the number
    // of rows.
    output_rows = std::min(output_rows, std::pow(child_rows, 0.9));
  } else {
    output_rows = std::min(output_rows, child_rows);
  }

  if (TraceStarted(thd)) {
    Trace(thd) << "Estimating " << non_field_values << " distinct values for "
               << remaining_term_cnt << " non-field terms and " << output_rows
               << " in total.\n";
  }
  return output_rows;
}

/**
   For a function f(x) such that:
   f(x) = g(x) for x<=l
   f(x) = h(x) for x>l

   tweak f(x) so that it is continuous at l even if g(l) != h(l).
   We obtain this by doing a gradual transition between g(x) and h(x)
   in an interval [l, l+k] for some constant k.
   @param function_low g(x)
   @param function_high h(x)
   @param lower_limit l
   @param upper_limit l+k
   @param argument x (for f(x))
   @returns Tweaked f(x).
*/
template <typename FunctionLow, typename FunctionHigh>
double SmoothTransition(FunctionLow function_low, FunctionHigh function_high,
                        double lower_limit, double upper_limit,
                        double argument) {
  assert(upper_limit > lower_limit);
  if (argument <= lower_limit) {
    return function_low(argument);

  } else if (argument >= upper_limit) {
    return function_high(argument);

  } else {
    // Might use std::lerp() in C++ 20.
    const double high_fraction =
        (argument - lower_limit) / (upper_limit - lower_limit);

    return function_low(argument) * (1.0 - high_fraction) +
           function_high(argument) * high_fraction;
  }
}

/**
  Do a cheap rollup row estimate for small result sets.
  If we group on n terms and expect k rows in total (before rollup),
  we make the simplifying assumption that each term has k^(1/n)
  distinct values, and that all terms are uncorrelated from each other.
  Then the number of rollup rows can be expressed as the sum of a finite
  geometric series:

  1 + m+ m^2+m^3...m^(n-1)

  where m =  k^(1/n).

  @param aggregate_rows Number of rows after aggregation.
  @param grouping_expressions Number of terms that we aggregated on.
  @return Estimated number of rollup rows.
*/
double EstimateRollupRowsPrimitively(double aggregate_rows,
                                     size_t grouping_expressions) {
  return SmoothTransition(
      [=](double input_rows) {
        // Prevent divide by zero in the next formula for input_rows close
        // to 1.0.
        return input_rows * grouping_expressions;
      },
      [=](double input_rows) {
        const double multiplier =
            std::pow(input_rows, 1.0 / grouping_expressions);
        // Sum of infinite geometric series "1 + m+ m^2+m^3...m^(n-1)"
        // where m is 'multiplier' and n is the size of 'terms'.
        return (1.0 - input_rows) / (1.0 - multiplier);
      },
      1.01, 1.02, aggregate_rows);
}

/**
  Do more precise rollup row estimate for larger result sets.
  If we have ROLLUP, there will be additional rollup rows. If we group on N
  terms T1..TN, we assume that the number of rollup rows will be:

  1 + CARD(T1) + CARD(T1,T2) +...CARD(T1...T(N-1))

  were CARD(T1...TX) is a row estimate for aggregating on T1..TX.

  @param thd Current thread.
  @param aggregate_rows Number of rows after aggregation.
  @param terms The group-by terms.
  @return Estimated number of rollup rows.
*/
double EstimateRollupRowsAdvanced(THD *thd, double aggregate_rows,
                                  TermArray terms) {
  // Make a more accurate rollup row calculation for larger sets.
  double rollup_rows = 1.0;
  while (terms.size() > 1) {
    terms.resize(terms.size() - 1);

    if (TraceStarted(thd)) {
      Trace(thd) << StringPrintf(
          "\nEstimating row count for ROLLUP on %zu terms.\n", terms.size());
    }
    rollup_rows +=
        EstimateDistinctRowsFromStatistics(thd, terms, aggregate_rows);
  }
  return rollup_rows;
}

/**
   Estimate the row count for an aggregate operation (including ROLLUP rows
   for GROUP BY ... WITH ROLLUP).
   @param thd Current thread.
   @param child The input to the aggregate path.
   @param query_block The query block to which the aggregation belongs.
   @param rollup True if we should add rollup rows to the estimate.
   @returns The row estimate.
*/
double EstimateAggregateRows(THD *thd, const AccessPath *child,
                             const Query_block *query_block, bool rollup) {
  if (query_block->is_implicitly_grouped()) {
    // For implicit grouping there will be 1 output row.
    return 1.0;
  }

  const double child_rows = child->num_output_rows();
  if (child_rows < 1.0) {
    // No rows in the input gives no groups.
    return child_rows;
  }

  // The aggregation terms.
  TermArray terms = GetAggregationTerms(*query_block->join);
  if (TraceStarted(thd)) {
    Trace(thd) << StringPrintf(
        "\nEstimating row count for aggregation on %zu terms.\n", terms.size());
  }

  double output_rows = EstimateDistinctRows(thd, child_rows, terms);

  if (rollup) {
    // Do a simple and cheap calculation for small result sets.
    constexpr double simple_rollup_limit = 50.0;

    output_rows += SmoothTransition(
        [terms](double aggregate_rows) {
          return EstimateRollupRowsPrimitively(aggregate_rows, terms.size());
        },
        [thd, terms](double aggregate_rows) {
          return EstimateRollupRowsAdvanced(thd, aggregate_rows, terms);
        },
        simple_rollup_limit, simple_rollup_limit * 1.1, output_rows);
  }

  return output_rows;
}

}  // Anonymous namespace.

double EstimateDistinctRows(THD *thd, double child_rows, TermArray terms) {
  if (terms.empty()) {
    // DISTINCT/GROUP BY on a constant gives at most one row.
    return min(1.0, child_rows);
  }
  if (child_rows < 1.0) {
    return child_rows;
  }

  // Do a simple but fast calculation of the row estimate if child_rows is
  // less than this.
  constexpr double simple_limit = 10.0;

  // EstimateDistinctRows() must be a continuous function of
  // child_rows.  If two alternative access paths have slightly
  // different child_rows values (e.g. 9.9999 and 10.0001) due to
  // rounding errors, EstimateDistinctRows() must return estimates
  // that are very close to each other. If not, cost calculation and
  // comparison for these two paths would be distorted. Therefore, we
  // cannot have a discrete jump at child_rows==10.0 (or any other
  // value). See also bug #34795264.
  return SmoothTransition(
      [&](double input_rows) { return std::sqrt(input_rows); },
      [&](double input_rows) {
        return EstimateDistinctRowsFromStatistics(thd, terms, input_rows);
      },
      simple_limit, simple_limit * 1.1, child_rows);
}

void EstimateAggregateCost(THD *thd, AccessPath *path,
                           const Query_block *query_block) {
  const AccessPath *child = path->aggregate().child;
  if (path->num_output_rows() == kUnknownRowCount) {
    path->set_num_output_rows(EstimateAggregateRows(
        thd, child, query_block, path->aggregate().olap == ROLLUP_TYPE));
  }

  path->set_init_cost(child->init_cost());
  path->set_init_once_cost(child->init_once_cost());

  path->set_cost(child->cost() + kAggregateOneRowCost *
                                     std::max(0.0, child->num_output_rows()));

  path->num_output_rows_before_filter = path->num_output_rows();
  path->set_cost_before_filter(path->cost());
  path->ordering_state = child->ordering_state;
}

void EstimateDeleteRowsCost(AccessPath *path) {
  const auto &param = path->delete_rows();
  const AccessPath *child = param.child;

  path->set_num_output_rows(child->num_output_rows());
  path->set_init_once_cost(child->init_once_cost());
  path->set_init_cost(child->init_cost());

  // Include the cost of building the temporary tables for the non-immediate
  // (buffered) deletes in the cost estimate.
  const table_map buffered_tables =
      param.tables_to_delete_from & ~param.immediate_tables;
  path->set_cost(child->cost() + kMaterializeOneRowCost *
                                     popcount(buffered_tables) *
                                     child->num_output_rows());
}

void EstimateUpdateRowsCost(AccessPath *path) {
  const auto &param = path->update_rows();
  const AccessPath *child = param.child;

  path->set_num_output_rows(child->num_output_rows());
  path->set_init_once_cost(child->init_once_cost());
  path->set_init_cost(child->init_cost());

  // Include the cost of building the temporary tables for the non-immediate
  // (buffered) updates in the cost estimate.
  const table_map buffered_tables =
      param.tables_to_update & ~param.immediate_tables;
  path->set_cost(child->cost() + kMaterializeOneRowCost *
                                     popcount(buffered_tables) *
                                     child->num_output_rows());
}

void EstimateStreamCost(AccessPath *path) {
  AccessPath &child = *path->stream().child;
  path->set_num_output_rows(child.num_output_rows());
  path->set_cost(child.cost());
  path->set_init_cost(child.init_cost());
  path->set_init_once_cost(0.0);  // Never recoverable across query blocks.
  path->num_output_rows_before_filter = path->num_output_rows();
  path->set_cost_before_filter(path->cost());
  path->ordering_state = child.ordering_state;
  path->safe_for_rowid = child.safe_for_rowid;
  // Streaming paths are usually added after all filters have been applied, so
  // we don't expect any delayed predicates. If there are any, we need to copy
  // them into path.
  assert(IsEmpty(child.delayed_predicates));
}

void EstimateLimitOffsetCost(AccessPath *path) {
  auto &lim = path->limit_offset();
  AccessPath *&child = lim.child;

  if (child->num_output_rows() >= 0.0) {
    path->set_num_output_rows(
        lim.offset >= child->num_output_rows()
            ? 0.0
            : (std::min<double>(child->num_output_rows(), lim.limit) -
               lim.offset));
  } else {
    path->set_num_output_rows(-1.0);
  }

  if (child->init_cost() < 0.0) {
    // We have nothing better, since we don't know how much is startup cost.
    path->set_cost(child->cost());
    path->set_init_cost(kUnknownCost);
  } else if (child->num_output_rows() < 1e-6) {
    path->set_cost(child->init_cost());
    path->set_init_cost(child->init_cost());
  } else {
    const double fraction_start_read =
        std::min(1.0, double(lim.offset) / child->num_output_rows());
    const double fraction_full_read =
        std::min(1.0, double(lim.limit) / child->num_output_rows());
    path->set_cost(child->init_cost() +
                   fraction_full_read * (child->cost() - child->init_cost()));
    path->set_init_cost(child->init_cost() +
                        fraction_start_read *
                            (child->cost() - child->init_cost()));
  }
}

void EstimateWindowCost(AccessPath *path) {
  auto &win = path->window();
  AccessPath *child = win.child;

  path->set_num_output_rows(child->num_output_rows());
  path->set_init_cost(child->init_cost());
  path->set_init_once_cost(child->init_once_cost());
  path->set_cost(child->cost() + kWindowOneRowCost * child->num_output_rows());
}

double EstimateSemijoinFanOut(THD *thd, double right_rows,
                              const JoinPredicate &edge) {
  // The fields from edge.expr->right that appear in the join condition.
  Prealloced_array<const Item *, 6> condition_fields(PSI_NOT_INSTRUMENTED);

  // For any Item_field in the subtree of 'item', add it to condition_fields
  // if it belongs to any table in edge.expr->right.
  const auto collect_field = [&](const Item *item) {
    if (item->type() == Item::FIELD_ITEM &&
        (item->used_tables() & edge.expr->right->tables_in_subtree) != 0) {
      const Item_field *const field = down_cast<const Item_field *>(item);

      // Make sure that we do not add the same field twice.
      if (std::none_of(
              condition_fields.cbegin(), condition_fields.cend(),
              [&](const Item *other_field) {
                return down_cast<const Item_field *>(other_field)->field ==
                       field->field;
              })) {
        condition_fields.push_back(field);
      }
    }
    return false;
  };

  for (const Item_eq_base *eq : edge.expr->equijoin_conditions) {
    WalkItem(eq, enum_walk::PREFIX, collect_field);
  }

  // Non-equijoin conditions.
  for (const Item *item : edge.expr->join_conditions) {
    WalkItem(item, enum_walk::PREFIX, collect_field);
  }

  const double distinct_rows = EstimateDistinctRows(
      thd, right_rows, {condition_fields.begin(), condition_fields.size()});

  return std::min(1.0, distinct_rows * edge.selectivity);
}
