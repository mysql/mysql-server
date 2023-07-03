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

#include "sql/join_optimizer/cost_model.h"

#include <math.h>
#include <stdio.h>
#include <algorithm>

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

void EstimateSortCost(AccessPath *path) {
  AccessPath *child = path->sort().child;
  const double num_input_rows = child->num_output_rows();
  const double num_output_rows =
      path->sort().limit != HA_POS_ERROR
          ? std::min<double>(num_input_rows, path->sort().limit)
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

  path->set_num_output_rows(num_output_rows);
  path->cost = path->init_cost = child->cost + sort_cost;
  path->init_once_cost = 0.0;
  path->num_output_rows_before_filter = path->num_output_rows();
  path->cost_before_filter = path->cost;
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
          subquery.path->cost +
          kMaterializeOneRowCost * subquery.path->num_output_rows();

      cost->cost_if_not_materialized += num_rows * subquery.path->cost;
    } break;

    case ContainedSubquery::Strategy::kNonMaterializable:
      cost->cost_if_not_materialized += num_rows * subquery.path->cost;
      cost->cost_if_materialized += num_rows * subquery.path->cost;
      break;

    case ContainedSubquery::Strategy::kIndependentSingleRow:
      cost->cost_if_materialized += subquery.path->cost;
      cost->cost_if_not_materialized += subquery.path->cost;
      cost->init_cost_if_not_materialized += subquery.path->cost;
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
  bool left_block = true;
  subquery_cost = 0.0;
  for (const MaterializePathParameters::QueryBlock &block :
       path->materialize().param->query_blocks) {
    if (block.subquery_path->num_output_rows() >= 0.0) {
      // For INTERSECT and EXCEPT we can never get more rows than we have in
      // the left block, so do not add unless we are looking at left block or
      // we have a UNION.
      if (left_block || path->materialize().param->table == nullptr ||
          path->materialize().param->table->is_union_or_table()) {
        path->set_num_output_rows(path->num_output_rows() +
                                  block.subquery_path->num_output_rows());
      } else if (!left_block &&
                 path->materialize().param->table->is_intersect()) {
        // INTERSECT can never give more rows than that of its smallest operand
        path->set_num_output_rows(std::min(
            path->num_output_rows(), block.subquery_path->num_output_rows()));
      }
      subquery_cost += block.subquery_path->cost;
      if (block.join != nullptr && block.join->query_block->is_cacheable()) {
        cost_for_cacheable += block.subquery_path->cost;
      }
    }
    left_block = false;
  }

  if (table_path->type == AccessPath::TABLE_SCAN) {
    path->cost = 0.0;
    path->init_cost = 0.0;
    path->init_once_cost = 0.0;
    table_path->set_num_output_rows(path->num_output_rows());
    table_path->init_cost = subquery_cost;
    table_path->init_once_cost = cost_for_cacheable;

    if (Overlaps(test_flags, TEST_NO_TEMP_TABLES)) {
      // Unit tests don't load any temporary table engines,
      // so just make up a number.
      table_path->cost = subquery_cost + path->num_output_rows() * 0.1;
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
      table_path->cost =
          subquery_cost + temp_table->file->table_scan_cost().total_cost();
    }
  } else {
    // Use the costs of the subquery.
    path->init_cost = subquery_cost;
    path->init_once_cost = cost_for_cacheable;
    path->cost = subquery_cost;
  }

  path->init_cost += std::max(table_path->init_cost, 0.0) +
                     kMaterializeOneRowCost * path->num_output_rows();

  path->init_once_cost += std::max(table_path->init_once_cost, 0.0);

  path->cost += std::max(table_path->cost, 0.0) +
                kMaterializeOneRowCost * path->num_output_rows();
}

namespace {

/// Array of aggregation terms.
using TermArray = Mem_root_array<const Item *>;

/**
   This class finds disjoint sets of aggregation terms that form prefixes of
   some non-hash index, and makes row estimates for those sets based on index
   metadata.
*/
class AggregateRowEstimator {
 public:
  /// @param terms The aggregation terms.
  /// @param trace Append optimizer trace text to this if non-null.
  AggregateRowEstimator(const TermArray &terms, string *trace);

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
  double MakeNextEstimate();

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
  const TermArray *m_terms;

  /// The set of terms mapped to an index so far.
  MutableOverflowBitset m_consumed_terms;

  /// The index prefixes we found for 'm_terms'.
  Mem_root_array<Prefix *> m_prefixes{current_thd->mem_root};

  /// Optimizer trace text.
  string *m_trace;

  /// Find an Item_field pointing to 'field' in 'm_terms', if there is one.
  /// @param field The field we look for.
  /// @returns An iterator to the position of 'field' in m_terms, or
  /// m_terms->cend().
  TermArray::const_iterator FindField(const Field *field) const {
    return std::find_if(
        m_terms->cbegin(), m_terms->cend(), [field](const Item *item) {
          assert(field != nullptr);
          return item->type() == Item::FIELD_ITEM &&
                 down_cast<const Item_field *>(item)->field == field;
        });
  }
};

AggregateRowEstimator::AggregateRowEstimator(const TermArray &terms,
                                             string *trace)
    : m_terms{&terms},
      m_consumed_terms{current_thd->mem_root, terms.size()},
      m_trace(trace) {
  /* Find keys (indexes) for which:
     - One or more of 'terms' form a prefix of the key.
     - Records per key estimates are available for some prefix of the key.
  */
  for (const Item *aggregate_term : terms) {
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

          m_prefixes.push_back(new (current_thd->mem_root)
                                   Prefix({key, key_part_no}));
          if (m_trace != nullptr) {
            *m_trace += "Adding prefix: " + m_prefixes.back()->Print() + "\n";
          }
        }
        key_map.clear_bit(key_idx);
        key_idx = key_map.get_first_set();
      }
    }
  }
}

double AggregateRowEstimator::MakeNextEstimate() {
  // Pick the longest prefix until we have used all terms or m_prefixes,
  // or until all prefixes have length==0.
  while (m_terms->size() >
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
      if (IsBitSet(FindField(field) - m_terms->cbegin(), m_consumed_terms)) {
        // We did not find it, so it must have been removed when we examined
        // some earlier key. We can thus only use the prefix 0..key_part_no of
        // this key.
        const Prefix shortened_prefix{prefix->m_key, key_part_no};
        if (m_trace != nullptr) {
          *m_trace += "Shortening prefix " + prefix->Print() + "\n  into  " +
                      shortened_prefix.Print() + ",\n  since field '" +
                      field->field_name +
                      "' is already covered by an earlier estimate.\n";
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
            m_terms->begin());
      }

      assert(prefix->m_key->records_per_key(prefix->m_length - 1) !=
             REC_PER_KEY_UNKNOWN);

      const double row_estimate =
          prefix->m_key->table->file->stats.records /
          prefix->m_key->records_per_key(prefix->m_length - 1);

      if (m_trace != nullptr) {
        *m_trace += "Choosing longest prefix " + prefix->Print() +
                    " with estimated distinct values: " +
                    StringPrintf("%.1f", row_estimate) + "\n";
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

/**
 We use the following data to make a row estimate, in that priority:

 1. (Non-hash) indexes where the aggregation terms form some prefix of the
  index key. The handler can give good estimates for these.

 2. Histograms for aggregation terms that are fields. The histograms
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

@param terms The aggregation terms.
@param child_rows The row estimate for the input path.
@param trace Append optimizer trace text to this if non-null.
@returns The row estimate for the aggregate operation.
*/
double EstimateAggregateRows(const TermArray &terms, double child_rows,
                             string *trace) {
  // Estimated number of output rows.
  double output_rows = 1.0;
  // No of individual estimates (for disjoint subsets of the aggregation terms).
  size_t estimate_count = 0;
  // The largest individual estimate.
  double top_estimate = 1.0;

  // Make row estimates for sets of aggregation terms that form prefixes
  // of (non-hash) indexes.
  AggregateRowEstimator index_estimator(terms, trace);

  while (true) {
    const double distinct_values = index_estimator.MakeNextEstimate();
    if (distinct_values == AggregateRowEstimator::kNoEstimate) {
      break;
    }
    top_estimate = std::max(distinct_values, top_estimate);
    output_rows *= distinct_values;
    estimate_count++;
  }

  size_t remaining_term_cnt =
      terms.size() - PopulationCount(index_estimator.GetConsumedTerms());

  // Loop over the remaining aggregation terms, i.e. those that were not part of
  // a key prefix. Make row estimates for those that are fields.
  for (TermArray::const_iterator term = terms.cbegin(); term < terms.cend();
       term++) {
    if (!IsBitSet(term - terms.cbegin(), index_estimator.GetConsumedTerms()) &&
        (*term)->type() == Item::FIELD_ITEM) {
      const Field *const field = down_cast<const Item_field *>(*term)->field;
      const histograms::Histogram *const histogram =
          field->table->s->find_histogram(field->field_index());

      double distinct_values;
      if (histogram == nullptr) {
        // We do not have a histogram for 'field', so we make an estimate
        // from the table row count instead.
        distinct_values = std::sqrt(field->table->file->stats.records);

        if (trace != nullptr) {
          *trace += StringPrintf(
              "Estimating %.1f distinct values for field '%s'"
              " from table size.\n",
              distinct_values, field->field_name);
        }

      } else {
        // If term is a field with a histogram, use that to get a row
        // estimate.
        assert(histogram->get_num_distinct_values() >= 1);
        distinct_values = histogram->get_num_distinct_values();

        if (trace != nullptr) {
          *trace += StringPrintf(
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

  if (trace != nullptr) {
    *trace += StringPrintf(
        "Estimating %.1f distinct values for %zu non-field terms"
        " and %.1f in total.\n",
        non_field_values, remaining_term_cnt, output_rows);
  }

  // The estimate could exceed 'child_rows' if there e.g. is a restrictive
  // WHERE-condition, as estimates from indexes or histograms will not reflect
  // that.
  if (estimate_count > 1 || (estimate_count == 1 && remaining_term_cnt > 0)) {
    // Combining estimates from different sources introduces uncertainty.
    // We therefore assume that there will be some reduction in the number
    // of rows.
    return std::min(output_rows, std::pow(child_rows, 0.9));
  } else {
    return std::min(output_rows, child_rows);
  }
}

}  // Anonymous namespace.

void EstimateAggregateCost(AccessPath *path, const Query_block *query_block,
                           string *trace) {
  const AccessPath *child = path->aggregate().child;
  const double child_rows = child->num_output_rows();
  // 'path' may represent 'GROUP BY' or 'DISTINCT'. In the latter case, we fetch
  // the aggregation terms from query_block->join->group_fields.
  const bool distinct = query_block->group_list.first == nullptr;
  bool calculate_rollup =
      !distinct &&
      is_rollup_group_wrapper(*query_block->group_list.first->item);

  const size_t term_count = distinct ? query_block->join->group_fields.size()
                                     : query_block->group_list.size();

  double output_rows;

  if (query_block->is_implicitly_grouped()) {
    // For implicit grouping there will be 1 output row.
    output_rows = 1.0;

  } else if (child_rows < 1.0) {
    output_rows = child_rows;

  } else if (child_rows < 10.0) {
    // Do a simple and cheap calculation for small row sets.
    output_rows = std::sqrt(child_rows);

  } else {
    // The aggregation terms.
    Mem_root_array<const Item *> terms(current_thd->mem_root);

    // Check if this is "SELECT DISTINCT...".
    if (distinct) {
      for (Cached_item &cached : query_block->join->group_fields) {
        terms.push_back(cached.get_item());
      }
    } else {
      for (ORDER *group = query_block->group_list.first; group;
           group = group->next) {
        const Item *term =
            calculate_rollup ?
                             // Extract the real GROUP-BY term.
                down_cast<const Item_rollup_group_item *>(*group->item)
                    ->inner_item()
                             : *group->item;

        terms.push_back(term);
      }
    }

    if (trace != nullptr) {
      *trace += StringPrintf(
          "\nEstimating row count for aggregation on %zu terms.\n", term_count);
    }

    output_rows = EstimateAggregateRows(terms, child_rows, trace);

    /*
      If we have ROLLUP, there will be additional rollup rows. If we group on N
      terms T1..TN, we assume that the number of rollup rows will be:

      1 + CARD(T1) + CARD(T1,T2) +...CARD(T1...T(N-1))

      were CARD(T1...TX) is a row estimate for aggregating on T1..TX.
    */
    if (calculate_rollup && output_rows > 50.0) {
      // Make a more accurate rollup row calculation for larger sets.
      while (terms.size() > 1) {
        terms.resize(terms.size() - 1);

        if (trace != nullptr) {
          *trace +=
              StringPrintf("\nEstimating row count for ROLLUP on %zu terms.\n",
                           terms.size());
        }
        output_rows += EstimateAggregateRows(terms, child_rows, trace);
      }
      output_rows++;
      calculate_rollup = false;
    }
  }

  /*
    Do a cheap rollup calculation for small result sets.
    If we group on n terms and expect k rows in total (before rollup),
    we make the simplifying assumption that each term has k^(1/n)
    distinct values, and that all terms are uncorrelated from each other.
    Then the number of rollup rows can be expressed as the sum of a finite
    geometric series:

    1 + m+ m^2+m^3...m^(n-1)

    where m =  k^(1/n).
  */
  if (calculate_rollup) {
    if (output_rows < 1.1) {
      // A simple calculation for small result sets that also prevents divide by
      // zero in the next formula.
      output_rows += output_rows * term_count;

    } else {
      const double multiplier = std::pow(output_rows, 1.0 / term_count);
      // Sum of infinite geometric series "1 + m+ m^2+m^3...m^(n-1)"
      // where m is 'multiplier' and n is the size of 'terms'.
      const double rollup_rows = (1 - output_rows) / (1 - multiplier);
      output_rows += rollup_rows;
    }
  }

  path->set_num_output_rows(output_rows);
  path->init_cost = child->init_cost;
  path->init_once_cost = child->init_once_cost;
  path->cost = child->cost + kAggregateOneRowCost * child_rows;
  path->num_output_rows_before_filter = path->num_output_rows();
  path->cost_before_filter = path->cost;
  path->ordering_state = child->ordering_state;
}

void EstimateDeleteRowsCost(AccessPath *path) {
  const auto &param = path->delete_rows();
  const AccessPath *child = param.child;

  path->set_num_output_rows(child->num_output_rows());
  path->init_once_cost = child->init_once_cost;
  path->init_cost = child->init_cost;

  // Include the cost of building the temporary tables for the non-immediate
  // (buffered) deletes in the cost estimate.
  const table_map buffered_tables =
      param.tables_to_delete_from & ~param.immediate_tables;
  path->cost = child->cost + kMaterializeOneRowCost *
                                 PopulationCount(buffered_tables) *
                                 child->num_output_rows();
}

void EstimateUpdateRowsCost(AccessPath *path) {
  const auto &param = path->update_rows();
  const AccessPath *child = param.child;

  path->set_num_output_rows(child->num_output_rows());
  path->init_once_cost = child->init_once_cost;
  path->init_cost = child->init_cost;

  // Include the cost of building the temporary tables for the non-immediate
  // (buffered) updates in the cost estimate.
  const table_map buffered_tables =
      param.tables_to_update & ~param.immediate_tables;
  path->cost = child->cost + kMaterializeOneRowCost *
                                 PopulationCount(buffered_tables) *
                                 child->num_output_rows();
}

void EstimateStreamCost(AccessPath *path) {
  AccessPath &child = *path->stream().child;
  path->set_num_output_rows(child.num_output_rows());
  path->cost = child.cost;
  path->init_cost = child.init_cost;
  path->init_once_cost = 0.0;  // Never recoverable across query blocks.
  path->num_output_rows_before_filter = path->num_output_rows();
  path->cost_before_filter = path->cost;
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

  if (child->init_cost < 0.0) {
    // We have nothing better, since we don't know how much is startup cost.
    path->cost = child->cost;
    path->init_cost = -1.0;
  } else if (child->num_output_rows() < 1e-6) {
    path->cost = path->init_cost = child->init_cost;
  } else {
    const double fraction_start_read =
        std::min(1.0, double(lim.offset) / child->num_output_rows());
    const double fraction_full_read =
        std::min(1.0, double(lim.limit) / child->num_output_rows());
    path->cost = child->init_cost +
                 fraction_full_read * (child->cost - child->init_cost);
    path->init_cost = child->init_cost +
                      fraction_start_read * (child->cost - child->init_cost);
  }
}
