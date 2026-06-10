/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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
#include <cmath>
#include <iterator>

#include "mem_root_deque.h"
#include "my_base.h"
#include "my_bitmap.h"  // bitmap_bits_set
#include "sql/handler.h"
#include "sql/histograms/histogram.h"
#include "sql/item_func.h"
#include "sql/item_subselect.h"
#include "sql/iterators/hash_join_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/find_contained_subqueries.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/join_optimizer/overflow_bitset.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/secondary_statistics.h"
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

// Below are convenience functions that calculate an estimated cost of a given
// path, using either hypergraph cost model or the old model. Linear regression
// was used to produce cost formulae. A common pattern in the below cost
// formulae is that wherever there is deduplication, the cost depends both on
// input rows and output rows. Furthermore, cost always increases not just with
// increasing aggregation functions, but also with the number of GROUP BY
// fields or DISTINCT fields.
//
// The literal constants in the cost model formulas below are in terms of
// microseconds, since the original calibration using linear regression fitted
// a model to the running time in microseconds (see
// MaterializationCostModel.java for details). In order to be compatible with
// the rest of the hypergraph cost model we have to output costs in terms of
// the cost unit (see cost_constants.h) and not directly in microseconds. In
// order to convert from microseconds to cost units we divide the output of
// each linear regression formula by kUnitCostInMicroSecondsWL16117, retaining
// the original calibrated constants for clarity.

/**
  Calculate the estimated cost of Streaming Aggregation, i.e. AGGREGATE
  Accesspath.
  @param thd Current thread.
  @param output_rows Number of rows that the path outputs.
  @param input_rows Number of input rows to the path.
  @param agg_count Number of aggregation functions present in the path.
  @param group_by_field_count Number of GROUP BY columns used in the path.
         In the absence of GROUP BY clause, it can be 0.
  @returns The cost estimate.

  Note: This aggregation cost is independent of the cost of Temp table
  aggregation, and these two paths do not share any logic or cost constants.
 */
static double AggregateCost(THD *thd, double output_rows, double input_rows,
                            int agg_count, int group_by_field_count) {
  if (!thd->lex->using_hypergraph_optimizer()) {
    return kAggregateOneRowCostOldModel * std::max(0.0, input_rows);
  }

  // Use hypergraph optimizer cost model ...

  // Suggested cost formula by linear regression:
  // -95.758E0 + o * 131.99E-3 +
  //  i * 27.353E-3 + i * aggs * 35.718E-3 + i * group_by_fields * 5.5004E-3
  return (0.132 * std::max(0.0, output_rows) +
          (std::max(0.0, input_rows) *
           (.0274 + agg_count * .0357 + group_by_field_count * .006))) /
         kUnitCostInMicrosecondsWL16117;
}

/**
  Calculate the estimated initialization cost of a MATERIALIZE Accesspath that
  involves deduplication. This involves the cost for deduplicating input rows
  and inserting them into the temp table.
  @param use_old_model If false, use the hypergraph cost model based on linear
         regression. If true, continue to use the earlier cost model.
  @param output_rows Number of rows that the path outputs.
  @param input_rows Number of input rows to the path.
  @param field_count Number of GROUP BY or DISTINCT columns present.
  @returns The cost estimate.
 */
static double MaterializationWithDedupCost(bool use_old_model,
                                           double output_rows,
                                           double input_rows, int field_count) {
  if (use_old_model) {
    return kMaterializeOneRowCostOldModel * output_rows;
  }

  // Linear regression formula for 'materialize_dedup':
  // -13.448E3 + o * 292.41E-3 + i * 112.57E-3 + i * fields * 38.639E-3
  return (.292 * output_rows + input_rows * (.113 + .039 * field_count)) /
         kUnitCostInMicrosecondsWL16117;
}

/**
  Calculate the estimated cost of a MATERIALIZE Accesspath that does not involve
  deduplication.
  @param use_old_model If false, use the hypergraph cost model based on linear
         regression. If true, continue to use the earlier cost model.
  @param output_rows Number of rows that the path outputs.
  @param field_count Number of fields present in the materialized rows, which
         translates to the number of temp table columns.
  @returns The cost estimate.
 */
static double MaterializationCost(bool use_old_model, double output_rows,
                                  int field_count) {
  if (use_old_model) {
    return kMaterializeOneRowCostOldModel * output_rows;
  }

  // Linear regression formula for 'materialize':
  // 70.011E0 + i * 62.093E-3 + i * fields * 14.778E-3
  return (output_rows * (.063 + .015 * field_count)) /
         kUnitCostInMicrosecondsWL16117;
}

/**
  Calculate the estimated cost of a Table scan Accesspath for a temporary table
  created for materialization.
  @param thd Current thread.
  @param table_path The TABLE_SCAN AccessPath of the temporary table.
  @param output_rows Number of rows that the path outputs.
  @returns The cost estimate.
 */
static double TempTableScanCost(THD *thd, AccessPath *table_path,
                                double output_rows) {
  if (Overlaps(test_flags, TEST_NO_TEMP_TABLES)) {
    // Unit tests don't load any temporary table engines,
    // so just make up a number.
    return output_rows * 0.1;
  }

  TABLE dummy_table;
  TABLE *temp_table = table_path->table_scan().table;
  if (temp_table == nullptr) {
    // We need a dummy TABLE object to get estimates.
    handlerton *handlerton = ha_default_temp_handlerton(thd);
    dummy_table.file = handlerton->create(handlerton, /*share=*/nullptr,
                                          /*partitioned=*/false, thd->mem_root);
    dummy_table.file->set_ha_table(&dummy_table);
    dummy_table.init_cost_model(thd->cost_model());
    temp_table = &dummy_table;
  }

  // Try to get usable estimates. Ignored by InnoDB, but used by
  // TempTable.
  temp_table->file->stats.records = min(output_rows, LLONG_MAX_DOUBLE);

  if (thd->lex->using_hypergraph_optimizer()) {
    // From linear regression results, it was found that the cost does not
    // increase with number of temp table fields. Calibration was done with
    // temp table in memory. Needs further calibration for tables spilled to
    // disk.
    return (output_rows * 0.082) / kUnitCostInMicrosecondsWL16117;
  }

  return temp_table->file->table_scan_cost().total_cost();
}

/**
  Calculate the estimated cost of a STREAM Accesspath.
  @param thd Current thread.
  @param output_rows Number of rows that the path outputs, which is same as
         input rows.
  @param field_count Number of fields present in the streamed rows, which
         typically translates to the number of JOIN fields.
  @returns The cost estimate.
 */
static double StreamCost(THD *thd, double output_rows, int field_count) {
  if (!thd->lex->using_hypergraph_optimizer()) {
    return 0;
  }

  // Linear regression shows : i * .121 + i * (n-2) .021. During the testing,
  // we had to have an initial count(*) and another field to trigger the
  // Stream plan, but then the fields were increased over and above these
  // fields, hence the (n-2). And we did not want to use aggregation functions
  // because they would incur extra irrelevant cost to the Stream plan.
  return (output_rows * (.079 + .021 * field_count)) /
         kUnitCostInMicrosecondsWL16117;
}

/**
  Add InnoDB engine cost overhead into the in-memory table cost if the
  estimated temp table size exceeds tmp_table_size.
  @param temptable_engine_cost Pre-calculated cost of in-memory table
  @param temp_table_size 'tmp_table_size' system variable value.
  @param output_rows Number of rows that the path outputs.
  @param join_fields Reference to join fields. These are used to estimate the
         temp table row length. See get_tmp_table_rec_length() for details.
  @returns cost after adding the disk cost overhead into the in-memory cost.
*/
static double AddInnodbEngineCostOverhead(
    double temptable_engine_cost, double temp_table_size, double output_rows,
    const mem_root_deque<Item *> &join_fields) {
  // For a temp table that uses InnoDB storage engine, the temp table
  // aggregation cost is observed to be this much times more than the TempTable
  // storage engine. But it is only a rough estimate for temporary tables that
  // fit in the buffer pool. A more detailed calibration is needed.

  constexpr double kInnoDBTemptableAggregationOverhead = 5;

  // The JOIN fields has hidden fields added from the GROUP BY items, and these
  // are also present in the temp table. And, expressions containing aggregates
  // such as '2 * avg(col))' are not included in the temp table; instead,
  // 'avg(col)' is extracted from it and added as a temp table hidden field.
  double rowlen = get_tmp_table_rec_length(join_fields, /*include_hidden=*/true,
                                           /*skip_agg_exprs=*/true);

  // This temp table size estimation is only based on a quick check,
  // and based on the fact that the table's hash index consumes extra
  // space. Proper size estimation is needed.
  double estimated_temptable_size = output_rows * (64 + rowlen);

  double buffer_ratio = estimated_temptable_size / temp_table_size;

  // Make the cost transition gradual. Start doing it only when the estimated
  // size reaches 90% of tmp_table_size.
  double probability_innodb_engine =
      (buffer_ratio <= 0.9)
          ? 0
          : (buffer_ratio >= 1 ? 1 : (buffer_ratio - 0.9) / 0.1);

  double innodb_engine_cost =
      kInnoDBTemptableAggregationOverhead * temptable_engine_cost;
  return std::lerp(temptable_engine_cost, innodb_engine_cost,
                   probability_innodb_engine);
}

/**
  Calculate the estimated initialization cost of a TEMPTABLE_AGGREGATE
  Accesspath This involves the cost for deduplicating input rows, inserting
  them into the temp table, and processing the aggregation functions.
  Cost estimation for this path was introduced only in hypergraph optimizer.
  @param thd Current thread.
  @param output_rows Number of rows that the path outputs.
  @param input_rows Number of input rows to the path.
  @param agg_count Number of aggregation functions present in the path.
  @param group_by_fields Number of GROUP BY columns present.
  @param join_fields Reference to join fields. These are used to estimate the
         temp table row length. See get_tmp_table_rec_length() for details.
  @returns The cost estimate.
 */
static double TempTableAggregationCost(
    THD *thd, double output_rows, double input_rows, int agg_count,
    int group_by_fields, const mem_root_deque<Item *> &join_fields) {
  // Suggested cost formula by regression analysis:
  // -17.931E3 + o * 358.04E-3 +
  //  i * 142.04E-3 + i * aggs * 78.696E-3 + i * fields * 74.319E-3
  double temptable_engine_cost =
      (output_rows * .358 +
       input_rows * (.142 + (.0787 * agg_count) + (.0743 * group_by_fields))) /
      kUnitCostInMicrosecondsWL16117;

  // If temp table exceeds the size threshold, add InnoDB cost overhead.
  return AddInnodbEngineCostOverhead(temptable_engine_cost,
                                     thd->variables.tmp_table_size, output_rows,
                                     join_fields);
}

// End of definitions of convenience cost functions related to materialization,
// aggregation, and streaming.

// The cost of creating a temp table for materialization or temp table
// aggregation. We ignore the y-intercept value in the above linear regression
// formulae, since it is more important to get the scaling right. But the cost
// also cannot be less than the temp table creation cost, hence always add this
// cost.  The value was derived by checking actual materialization cost
// involving one or two rows.
static constexpr double kTempTableCreationCost = 3;

/**
   This class produces an estimate of the size of each field
   in a table. For fixed-size fields, the estimates should be accurate.
   But since there is no statistics on the length of variable size fields,
   we use heuristics to estimate these.
*/
class FieldSizeEstimator final {
 public:
  /// A size estimate for a field.
  struct Estimate final {
    /// This struct refers to table->field[field_no]
    uint field_no;
    /// The estimated size in bytes.
    int64_t size;
  };

  /// Size estimates for all fields.
  using EstimateArray = Prealloced_array<Estimate, 32>;

  explicit FieldSizeEstimator(const TABLE *table);

  const EstimateArray &estimates() const { return m_estimates; }

 private:
  /// Size estimates for all fields.
  EstimateArray m_estimates{PSI_NOT_INSTRUMENTED};
};

FieldSizeEstimator::FieldSizeEstimator(const TABLE *table) {
  /*
    We have no statistics on the size of the individual variable-sized fields,
    only on the combined size of all fields. We therefore estimate the field
    sizes as follows:
    - We order the fields by their maximal size (field->max_data_length()) in
    ascending order.
    - We estimate the size of a field to be the smallest of its maximal size
    and the remaining number of bytes, divided by the remaining number of
    fields.
  */
  int64_t max_row_size{0};

  for (uint i = 0; i < table->s->fields; i++) {
    const int64_t max_length{table->field[i]->max_data_length()};
    m_estimates.push_back({i, max_length});
    max_row_size += max_length;
  }

  std::ranges::sort(m_estimates,
                    [](Estimate a, Estimate b) { return a.size < b.size; });

  /*
    If we have no statistics on actual row size, we assume that the row is
    no longer than this, even if the combined size of the LOBs it contains
    could be greater.
  */
  constexpr int64_t kDefaultLobRowMaxSize{64 * 1024};

  /*
    On InnoDB mean_rec_length is calculated as file size divided by the number
    of rows. And the file is by default allocated in 16kiB blocks. So for
    tables with few rows, this number may be too high, which is why we use
    max_row_size as an upper limit.
  */
  int64_t remaining_bytes{std::min<int64_t>(
      max_row_size, table->file->stats.records == 0
                        ? kDefaultLobRowMaxSize
                        : table->file->stats.mean_rec_length)};

  for (size_t i = 0; i < m_estimates.size(); i++) {
    Estimate &estimate{m_estimates[i]};
    estimate.size = std::min<int64_t>(estimate.size,
                                      remaining_bytes / (table->s->fields - i));

    remaining_bytes -= estimate.size;
  }
}

int64_t CalculateReadSetWidth(const TABLE *table) {
  int64_t width{0};

  for (const FieldSizeEstimator estimator{table};
       const FieldSizeEstimator::Estimate &estimate : estimator.estimates()) {
    if (bitmap_is_set(table->read_set, estimate.field_no)) {
      width += estimate.size;
    }
  }

  return width;
}

BytesPerTableRow EstimateBytesPerRowWideTable(const TABLE *table) {
  // The expected size of the b-tree record.
  double record_size{0.0};
  // The expected number of overflow bytes per record.
  double overflow_size{0.0};
  // The probability of a row having at least one overflow page .
  double overflow_probability{0.0};
  // The maximal size of a b-tree record.
  const double max_record_size{ClampedBlockSize(table) / 2.0};

  for (const FieldSizeEstimator estimator{table};
       const FieldSizeEstimator::Estimate &estimate : estimator.estimates()) {
    const double field_overflow_probability{[&]() {
      if (record_size + table->field[estimate.field_no]->max_data_length() <
          max_record_size) {
        return 0.0;
      }

      /*
        Chance of overflow grows gradually from 0% chance at row size
        80% of kMaxEstimatedBytesPerRow to 100% chance at 120% of
        kMaxEstimatedBytesPerRow.
      */
      return std::clamp(
          2.5 * (estimate.size + record_size) / kMaxEstimatedBytesPerRow - 2.0,
          0.0, 1.0);
    }()};

    record_size += estimate.size * (1 - field_overflow_probability);

    if (bitmap_is_set(table->read_set, estimate.field_no)) {
      overflow_size += estimate.size * field_overflow_probability;
      overflow_probability = field_overflow_probability;
    }
  }

  return {.record_bytes = static_cast<int64_t>(record_size),
          .overflow_bytes = static_cast<int64_t>(overflow_size),
          .overflow_probability = overflow_probability};
}

/**
   Estimate a lower limit for the cache hit ratio when reading from an index
   (or base table), based on the size of the index relative to that of
   the buffer pool.
   @param file The handler to which the index or base table belongs.
   @param file_size The size of the index or base table, in bytes.
   @returns A lower limit for the cache hit ratio.
*/
static double LowerCacheHitRatio(const handler *file, double file_size) {
  const longlong handler_size{file->get_memory_buffer_size()};
  // Assume that the buffer pool is 4GB if we do not know.
  const double pool_size{handler_size >= 0 ? handler_size : 4.29e9};

  // If the index (or table) is smaller than this, we assume that it is
  // fully cached.
  const double fits_entirely{0.05 * pool_size};

  return std::clamp(
      1.0 - (file_size - fits_entirely) / (pool_size - fits_entirely), 0.0,
      1.0);
}

double TableAccessIOCost(const TABLE *table, double num_rows,
                         BytesPerTableRow row_size) {
  if (strcmp("InnoDB", ha_resolve_storage_engine_name(table->file->ht)) != 0) {
    // IO cost not yet implemented for other storage engines.
    return 0.0;
  }

  const double block_size{static_cast<double>(ClampedBlockSize(table))};

  // The cost of reading b-tree records.
  const double record_cost{[&]() {
    if (num_rows < 1.0) {
      return num_rows * (kIOStartCost + block_size * kIOByteCost);
    }

    // May not be accurate, as the row size in the storage engine may be
    // different.
    const double rows_per_block{
        std::max(1.0, (block_size * kBlockFillFactor) / row_size.record_bytes)};

    const double blocks{1 + (num_rows - 1) / rows_per_block};

    return kIOStartCost + blocks * block_size * kIOByteCost;
  }()};

  // The cost of reading overflow pages (long variable-sized fields).
  const double overflow_cost{[&]() {
    if (row_size.overflow_bytes == 0) {
      return 0.0;
    }

    // The expected number of overflow blocks, given that there is overflow.
    const double overflow_blocks{
        std::ceil(row_size.overflow_bytes / block_size)};

    return num_rows * row_size.overflow_probability *
           (kIOStartCost + overflow_blocks * block_size * kIOByteCost);
  }()};

  const double cache_miss_ratio{[&]() {
    // Do "SET DEBUG='d,in_memory_0'" to simulate zero cache hit rate,
    // in_memory_50 or in_memory_100 for 50% or 100%  cache hit rate.
    DBUG_EXECUTE_IF("in_memory_0", { return 1.0; });
    DBUG_EXECUTE_IF("in_memory_50", { return 0.5; });
    DBUG_EXECUTE_IF("in_memory_100", { return 0.0; });

    return 1.0 -
           std::max(table->file->table_in_memory_estimate(),
                    LowerCacheHitRatio(table->file,
                                       table->file->stats.data_file_length));
  }()};

  return cache_miss_ratio * (record_cost + overflow_cost);
}

double CoveringIndexAccessIOCost(const TABLE *table, unsigned key_idx,
                                 double num_rows) {
  assert(!IsClusteredPrimaryKey(table, key_idx));
  if (strcmp("InnoDB", ha_resolve_storage_engine_name(table->file->ht)) != 0) {
    // IO cost not yet implemented for other storage engines.
    return 0.0;
  }

  const double block_size{static_cast<double>(ClampedBlockSize(table))};
  // May not be accurate, as the row size in the storage engine may be
  // different.
  const double rows_per_block{
      std::max(1.0, (block_size * kBlockFillFactor) /
                        EstimateBytesPerRowIndex(table, key_idx))};

  // The IO-cost if there were no caching.
  const double uncached_cost{[&]() {
    if (num_rows < 1.0) {
      return num_rows * (kIOStartCost + block_size * kIOByteCost);
    }

    // The number of (leaf) blocks that we must read.
    const double blocks_read{1 + (num_rows - 1) / rows_per_block};
    return kIOStartCost + blocks_read * block_size * kIOByteCost;
  }()};

  // The size (in bytes) of the entire index.
  const double file_length{table->file->stats.records / rows_per_block *
                           block_size};

  // The fraction of index blocks that will not be found in the buffer pool.
  const double cache_miss_ratio{[&]() {
    // Do "SET DEBUG='d,in_memory_0'" to simulate zero cache hit rate,
    // in_memory_50 or in_memory_100 for 50% or 100%  cache hit rate.
    DBUG_EXECUTE_IF("in_memory_0", { return 1.0; });
    DBUG_EXECUTE_IF("in_memory_50", { return 0.5; });
    DBUG_EXECUTE_IF("in_memory_100", { return 0.0; });

    return 1.0 - std::max(table->file->index_in_memory_estimate(key_idx),
                          LowerCacheHitRatio(table->file, file_length));
  }()};

  return uncached_cost * cache_miss_ratio;
}

double EstimateIndexRangeScanCost(const TABLE *table, unsigned key_idx,
                                  RangeScanType scan_type, double num_ranges,
                                  double num_output_rows) {
  /*
    The cost of performing num_ranges lookups and reading
    num_output_rows from the index (including IO cost). If the index
    is covering we return this cost directly. If it is non-covering we
    account for the additional cost of performing lookups into the
    primary index, either using the standard strategy of performing a
    lookup for each matching record in the secondary index directly,
    or by using the Multi-Range Read (MRR) optimization, that first
    collects (batches of) primary key values and then performs the
    lookups in sorted order to save on IO cost compared to doing
    random lookups.
  */
  const double index_cost{num_ranges * IndexLookupCost(table, key_idx) +
                          RowReadCostIndex(table, key_idx, num_output_rows)};

  if (IsClusteredPrimaryKey(table, key_idx) ||
      table->covering_keys.is_set(key_idx)) {
    return index_cost;
  }

  // If we are operating on a secondary non-covering index we have to perform
  // a lookup into the primary index for each matching row. This is the case
  // for the InnoDB storage engine, but with the MEMORY engine we do not have
  // a primary key, so we instead assign a default lookup cost.
  const double lookup_cost{table->s->is_missing_primary_key()
                               ? kIndexLookupDefaultCost
                               : IndexLookupCost(table, table->s->primary_key)};

  // When this function is called by e.g. EstimateRefAccessCost() we can have
  // num_output_rows < 1 and it becomes important that our cost estimate
  // reflects expected cost, i.e. that it scales linearly with the expected
  // number of output rows.
  switch (scan_type) {
    case RangeScanType::kMultiRange: {
      /*
        Since MRR sorts the primary keys from the secondary index before doing
        lookups on the primary keys, it should not need to read each base table
        leaf block more than once.
        Caveats:
        * data_file_length includes overflow (LOB) blocks. We may or may not
          need those, depending on the projection.
        * If the scan of the seconday index return lots of primary keys, we
          will sort these in batches and do lookups on the base table for each
          batch. Then we may indeed end up reading the same base table blocks
          multiple times.
        * We should add a cost element for sorting the primary keys, so that
          single-range scans will be cheaper if the base table is fully cached.
       */
      const uint fields_read_per_row{bitmap_bits_set(table->read_set)};
      const BytesPerTableRow &bytes_per_row = *table->bytes_per_row();

      // Cost of reading single row from base table, except IO cost.
      const double row_cost{RowReadCost(
          1.0, fields_read_per_row,
          bytes_per_row.record_bytes + bytes_per_row.overflow_bytes)};

      const int64_t blocks{static_cast<int64_t>(
          table->file->stats.data_file_length / ClampedBlockSize(table))};

      const double disk_reads{std::min<double>(num_output_rows, blocks)};

      const double io_cost{
          disk_reads *
          TableAccessIOCost(table, num_output_rows / std::max(1.0, disk_reads),
                            bytes_per_row)};

      return index_cost + num_output_rows * (lookup_cost + row_cost) + io_cost;
    }

    case RangeScanType::kSingleRange:
      return index_cost +
             num_output_rows * (lookup_cost + RowReadCostTable(table, 1));

    default:
      assert(false);
      return index_cost;
  }
}

namespace {
double EstimateAggregateRows(THD *thd, const AccessPath *child,
                             const Query_block *query_block, bool rollup);
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
    sort_cost = kSortOneRowCost * num_input_rows +
                kSortComparisonCost * sort_result_rows *
                    std::max(log2(sort_result_rows), 1.0);
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
          kMaterializeOneRowCostOldModel * subquery.path->num_output_rows();

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

static void AddOperandCosts(const MaterializePathParameters::Operand &operand,
                            double *subquery_cost, double *cost_for_cacheable) {
  // For implicit grouping operand.subquery_path->num_output_rows() may be
  // set (to 1.0) even if operand.subquery_path->cost is undefined (cf.
  // Bug#35240913).
  if (operand.subquery_path->cost() > 0.0) {
    *subquery_cost += operand.subquery_path->cost();
    if (operand.join != nullptr && operand.join->query_block->is_cacheable()) {
      *cost_for_cacheable += operand.subquery_path->cost();
    }
  }
}

static void SetDistinctGroupByOutputRowsAndSubqueryCosts(
    THD *thd, AccessPath *path, double *subquery_cost,
    double *cost_for_cacheable) {
  // For DISTINCT or GROUP BY there is only one operand.
  const MaterializePathParameters::Operand &operand =
      path->materialize().param->m_operands.at(0);

  // For GROUP BY, the number of output rows may or may not be already preset.
  if (path->materialize().param->deduplication_reason ==
          MaterializePathParameters::DEDUP_FOR_GROUP_BY &&
      path->num_output_rows() == kUnknownRowCount) {
    // The number of output rows is equal to the number of distinct groups, so
    // we can reuse our cardinality estimation from regular aggregation.
    path->set_num_output_rows(EstimateAggregateRows(
        thd, operand.subquery_path, operand.join->query_block,
        /*rollup=*/false));  // Temporary tables do not support GROUP BY WITH
                             // ROLLUP.
  }
  // else for DISTINCT, it's always preset.

  *subquery_cost = *cost_for_cacheable = 0;
  AddOperandCosts(operand, subquery_cost, cost_for_cacheable);
}

/**
  Return the cost for materialization used for DISTINCT or GROUP BY, which
  essentially involves deduplication cost.
 */
static double GetDistinctOrGroupByInitCost(bool use_old_cost_model,
                                           AccessPath *path) {
  int num_deduplication_fields = 0;
  const MaterializePathParameters::Operand &operand =
      path->materialize().param->m_operands.at(0);

  if (path->materialize().param->deduplication_reason ==
      MaterializePathParameters::DEDUP_FOR_DISTINCT) {
    num_deduplication_fields = CountVisibleFields(*operand.join->fields);
  } else {  // GROUP BY
    num_deduplication_fields =
        CountOrderElements(operand.join->group_list.order);
  }
  return MaterializationWithDedupCost(
      use_old_cost_model, path->num_output_rows(),
      operand.subquery_path->num_output_rows(), num_deduplication_fields);
}

// Accumulate output rows and subquery costs from the children. Not to be used
// for DISTINCT/GROUP BY
static void AccummulateOutputRowsAndSubqueryCosts(AccessPath *path,
                                                  double *subquery_cost,
                                                  double *cost_for_cacheable) {
  bool left_operand = true;

  path->set_num_output_rows(0);  // Reset any possibly set output rows.
  *subquery_cost = 0.0;

  for (const MaterializePathParameters::Operand &operand :
       path->materialize().param->m_operands) {
    if (operand.subquery_path->num_output_rows() >= 0.0) {
      // Add up output rows.

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

      // Add up subquery costs.
      AddOperandCosts(operand, subquery_cost, cost_for_cacheable);
    }
    left_operand = false;
  }
}

/**
  Provide row estimates and costs for a MATERIALIZE AccessPath.
  MATERIALIZE AccessPath is created by both old and new optimizer in many
  different contexts where temp table needs to be created, both with and
  without deduplication. E.g.
  materialization for a derived table,
  materializing deduplicated input rows for DISTINCT,
  GROUP BY clause without an aggregation functions,
  SET operations, etc

  Note:
  - SET operations that do deduplication (such as UNION DISTINCT, EXCEPT and
    INTERSECT) currently do not consider deduplication cost. They should.
  - There is no aggregation involved in this path. Aggregation with temp table
    uses a different Access path.
*/
void EstimateMaterializeCost(THD *thd, AccessPath *path) {
  AccessPath *table_path = path->materialize().table_path;
  double &subquery_cost = path->materialize().subquery_cost;

  // is_distinct_or_group_by=true means we are materializing in order to
  // deduplicate for a query that either uses DISTINCT or GROUP BY without any
  // aggregation functions.
  // When is_distinct_or_group_by is false, it means :
  // - Either it can be materialization of a single child plan without
  //   deduplication,
  // - Or it can be a SET operations materialization with or without
  //   deduplication.
  //
  // We don't currently consider deduplication cost in case of SET operations.
  // When we would consider it in the future, it should (ideally) share the
  // deduplication cost model currently being used for DISTINCT and GROUP BY.
  const bool is_distinct_or_group_by =
      path->materialize().param->deduplication_reason !=
      MaterializePathParameters::NO_DEDUP;

  double cost_for_cacheable = 0.0;
  double table_scan_cost = 0;
  double init_cost = 0;

  // We support hypergraph model for deduplication unless it's for SET
  // operations.
  bool use_old_cost_model =
      !thd->lex->using_hypergraph_optimizer() ||
      (path->materialize().param->cte != nullptr &&
       path->materialize().param->cte->recursive) ||
      (!is_distinct_or_group_by &&  // we support new model for DISTINCT
       path->materialize().param->table != nullptr &&
       // New model not supported for deduplication used in SET operations.
       MaterializeIsDoingDeduplication(path->materialize().param->table));

  // Accummulate output rows and subquery costs from the children.

  // There are three different strategies for estimating the output row count.
  // If it's for DISTINCT, it's always preset in the access path before calling
  // this function. If it's for GROUP BY, it may have been preset, but we have
  // to calculate it if it's not. For everything else, we calculate it afresh.
  if (!is_distinct_or_group_by) {
    AccummulateOutputRowsAndSubqueryCosts(path, &subquery_cost,
                                          &cost_for_cacheable);
  } else {
    SetDistinctGroupByOutputRowsAndSubqueryCosts(thd, path, &subquery_cost,
                                                 &cost_for_cacheable);
  }

  // Now that the output rows are set, we can calculate the init cost.

  // The materialization cost will be at least the temp table creation cost.
  if (!use_old_cost_model) {
    init_cost = kTempTableCreationCost;
  }

  if (!is_distinct_or_group_by) {
    int num_fields = 0;
    if (JOIN *join = path->materialize().param->m_operands.at(0).join;
        join != nullptr) {
      num_fields = CountVisibleFields(*join->fields);
    }
    // This involves plain materialization. Even though SET operations can
    // involve deduplication, we are not currently considering deduplication
    // cost.  Needs to be fixed in the future.
    init_cost += MaterializationCost(use_old_cost_model,
                                     path->num_output_rows(), num_fields);
  } else {
    init_cost += GetDistinctOrGroupByInitCost(use_old_cost_model, path);
  }

  // Rest of the logic is common for any type of materialization.

  path->materialize().subquery_rows = path->num_output_rows();
  path->num_output_rows_before_filter = path->num_output_rows();

  // Set the table path cost to its own scan cost plus the descendents' cost,
  // or in other words, the complete cost minus materialization cost. But see
  // comments below.
  if (table_path->type == AccessPath::TABLE_SCAN) {
    table_path->set_num_output_rows(path->num_output_rows());
    table_path->num_output_rows_before_filter = path->num_output_rows();
    table_path->set_init_cost(subquery_cost);
    table_path->set_init_once_cost(cost_for_cacheable);

    table_scan_cost =
        TempTableScanCost(thd, table_path, path->num_output_rows());
    table_path->set_cost(table_path->init_cost() + table_scan_cost);
  } else {
    // The table_path is assumed to have updated cost figures.
    table_scan_cost = std::max(table_path->cost(), 0.0);
  }

  path->set_init_cost(subquery_cost + init_cost);
  path->set_init_once_cost(cost_for_cacheable);

  if (table_path->type != AccessPath::TABLE_SCAN) {
    // An assumption here is that a non-TABLE_SCAN path does not include
    // descendants' cost in its own cost. Otherwise the below calculation would
    // cause double inclusion of descendents cost. It is not clear why in the
    // first place we include the descendents cost for a TABLE_SCAN path above.
    // In fact, AddPathCosts() is anyways not going to use table_path->cost. It
    // is going to show the table path cost as the total cost of the whole
    // materialization path (i.e. path->cost()). At a minimum, table_path cost
    // should have some consistency regardless of it's access_type.  TODO: This
    // needs to be fixed in a future cost model WL.
    path->set_init_cost(path->init_cost() +
                        std::max(table_path->init_cost(), 0.0));
    path->set_init_once_cost(path->init_once_cost() +
                             std::max(table_path->init_once_cost(), 0.0));
  }

  path->set_cost(path->init_cost() + table_scan_cost);
}

namespace {

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
  TermArray::iterator FindField(const Field *field) const {
    return std::find_if(
        m_terms.begin(), m_terms.end(), [field](const Item *item) {
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
      if (IsBitSet(FindField(field) - m_terms.begin(), m_consumed_terms)) {
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

 2. Statistics from secondary engine or histograms for terms that are fields.
    Both can give an estimate of the number of unique values.
    (Statistics from secondary engine is preferred if available.)

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
  for (TermArray::iterator term = terms.begin(); term < terms.end(); term++) {
    if (!IsBitSet(term - terms.begin(), index_estimator.GetConsumedTerms()) &&
        (*term)->type() == Item::FIELD_ITEM) {
      const Field *const field = down_cast<const Item_field *>(*term)->field;

      // Check if we can use statistics from secondary engine
      double distinct_values =
          secondary_statistics::NumDistinctValues(thd, *field);

      if (distinct_values <= 0.0) {
        // Try histogram
        const histograms::Histogram *const histogram =
            field->table->find_histogram(field->field_index());
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
    const double high_fraction =
        (argument - lower_limit) / (upper_limit - lower_limit);
    return std::lerp(function_low(argument), function_high(argument),
                     high_fraction);
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
    terms = terms.first(terms.size() - 1);

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
  if (child_rows == kUnknownRowCount) {
    return kUnknownRowCount;
  }

  if (child_rows <= 1.0) {
    // We make the simplifying assumption that the chance of exactly one
    // aggregated row is child_rows, and the chance of zero aggregated rows
    // is 1.0 - child_rows.
    if (rollup) {
      // If there is one child row, we get one result row plus one for
      // each group-by column. If there are zero child rows, we get a
      // single result row.
      return 1.0 + child_rows * query_block->join->group_fields.size();
    }
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

  path->set_cost(
      std::max(0.0, child->cost()) +
      AggregateCost(thd, path->num_output_rows(), child->num_output_rows(),
                    query_block->join->tmp_table_param.sum_func_count,
                    CountOrderElements(query_block->join->group_list.order)));

  path->num_output_rows_before_filter = path->num_output_rows();
  path->set_cost_before_filter(path->cost());
}

double EstimateSkipScanCost(TABLE *table, uint key_idx, uint num_subrange_scans,
                            ha_rows records) {
  // Multiplier to achieve the same cost/time ratio as for other access paths
  constexpr double kSkipScanFactor{3.344};
  return kSkipScanFactor *
         EstimateIndexRangeScanCost(table, key_idx, RangeScanType::kMultiRange,
                                    num_subrange_scans, records);
}

double EstimateGroupSkipScanCost(TABLE *table, uint key_idx, uint num_groups,
                                 bool has_max) {
  double lookups_per_key = has_max ? 1.8 : 1;
  return EstimateIndexRangeScanCost(table, key_idx, RangeScanType::kMultiRange,
                                    lookups_per_key * num_groups,
                                    lookups_per_key * num_groups);
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
  path->set_cost(child->cost() +
                 (kMaterializeOneRowCostOldModel * popcount(buffered_tables) *
                  child->num_output_rows()));
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
  path->set_cost(child->cost() +
                 (kMaterializeOneRowCostOldModel * popcount(buffered_tables) *
                  child->num_output_rows()));
}

void EstimateStreamCost(THD *thd, AccessPath *path) {
  const auto &stream_path = path->stream();
  int numfields =
      (stream_path.join != nullptr && stream_path.join->fields != nullptr
           ? stream_path.join->fields->size()
           : 2);  // We didn't get the fields. Just make up a number.

  AccessPath &child = *path->stream().child;
  path->set_num_output_rows(child.num_output_rows());
  path->set_cost(child.cost() +
                 StreamCost(thd, child.num_output_rows(), numfields));
  path->set_init_cost(child.init_cost());
  path->set_init_once_cost(0.0);  // Never recoverable across query blocks.
  path->num_output_rows_before_filter = path->num_output_rows();
  path->set_cost_before_filter(path->cost());
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
    path->set_cost(
        std::lerp(child->init_cost(), child->cost(), fraction_full_read));
    path->set_init_cost(
        std::lerp(child->init_cost(), child->cost(), fraction_start_read));
  }
}

void EstimateTemptableAggregateCost(THD *thd, AccessPath *path,
                                    const Query_block *query_block) {
  AccessPath *child = path->temptable_aggregate().subquery_path;
  AccessPath *table_path = path->temptable_aggregate().table_path;

  // Calculate estimate of output rows, which is same as the number of rows
  // after aggregation.
  if (path->num_output_rows() == kUnknownRowCount) {
    path->set_num_output_rows(
        EstimateAggregateRows(thd, child, query_block, /*rollup=*/false));
  }

  double num_output_rows = path->num_output_rows();
  double table_scan_cost = TempTableScanCost(thd, table_path, num_output_rows);

  // Add temp table initialization cost....
  double init_cost = kTempTableCreationCost;
  init_cost += TempTableAggregationCost(
      thd, num_output_rows, child->num_output_rows(),
      query_block->join->tmp_table_param.sum_func_count,
      CountOrderElements(query_block->join->group_list.order),
      *query_block->join->fields);

  double child_cost = std::max(child->cost(), 0.0);
  path->set_init_cost(init_cost + child_cost);
  path->set_init_once_cost(path->init_cost());
  path->set_cost(path->init_cost() + table_scan_cost);

  // The logic of setting table path costs is taken from
  // EstimateMaterializeCost(). It is not clear why we are supposed to include
  // child cost in a TABLE_SCAN AccessPath cost. Did this just for consistency.
  // Check EstimateMaterializeCost() for details.
  if (table_path->type == AccessPath::TABLE_SCAN) {
    table_path->set_init_cost(child_cost);
    table_path->set_init_once_cost(child_cost);
    table_path->set_cost(table_path->init_cost() + table_scan_cost);
    table_path->set_num_output_rows(num_output_rows);
  }
  // else the table_path is assumed to have updated cost figures.

  path->num_output_rows_before_filter = num_output_rows;
  path->set_cost_before_filter(path->cost());
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

HashJoinCost::HashJoinCost(THD *thd, const HashJoinMetrics &metrics) {
  // Size of the length field for each hash value.
  constexpr int kHashValueOverhead{10};
  // Size of the length field and other overhead for each hash key.
  constexpr int kHashKeyOverhead{34};

  // The cost of copying a build row into the hash table.
  constexpr double kCostPerBuildRow{144e-3 / kUnitCostInMicroseconds};
  // The cost of copying a build row byte into the hash table.
  constexpr double kCostPerBuildByte{179e-6 / kUnitCostInMicroseconds};
  // The cost of probing a row against the hash table.
  constexpr double kCostPerProbeOperation{58.1e-3 / kUnitCostInMicroseconds};
  // The cost of producing a result row.
  constexpr double kCostPerResultRow{9.87e-3 / kUnitCostInMicroseconds};
  // The cost of copying a byte from the hash table into the TABLE row buffer.
  // We do this with each build row that matches a probe row.
  constexpr double kCostPerCopyBackByte{23.2e-6 / kUnitCostInMicroseconds};
  // The cost per byte read from or written to spill files. We assume that
  // reading a byte has the same cost as writing it.
  constexpr double kCostPerSpillByte{48.7e-6 / kUnitCostInMicroseconds};

  const ulong join_buff_size{thd->variables.join_buff_size};

  // Do a simplified calculation for small joins.
  if (metrics.build_rows *
          (metrics.build_row_size + kHashValueOverhead + kHashKeyOverhead) <
      std::min<double>(join_buff_size, 64 * 1024)) {
    m_spill_to_disk_probability = 0.0;

    m_init_cost =
        metrics.build_rows *
        (kCostPerBuildRow + metrics.build_row_size * kCostPerBuildByte);

    m_cost =
        m_init_cost + metrics.probe_rows * kCostPerProbeOperation +
        metrics.result_rows *
            (kCostPerResultRow + metrics.build_row_size * kCostPerCopyBackByte);
    return;
  }

  // Total hash volume needed.
  const double hash_table_usage{
      metrics.build_rows * (metrics.build_row_size + kHashValueOverhead) +
      /* We estimate the number of unique keys as rows^0.7. Note that
         EstimateDistinctRows() might have provided a better estimate.
         We choose this simplified calculation because:
         - Given that this is a hash join, there is unlikely to be an index
           prefix that corresponds to the key, and this makes
           EstimateDistinctRows() less accurate.
         - This only impacts the volume of the keys - not that of the rows.
           This means that accuracy matters less here.
      */
      std::pow(metrics.build_rows, 0.7) *
          (kHashKeyOverhead + metrics.key_size)};

  /*
    If the full data set from right_path fits in the join buffer,
    we never need to rebuild the hash table. build_cost should
    then be counted as init_once_cost. Otherwise, build_cost will
    be incurred for each re-scan. To get a good estimate of
    init_once_cost we therefore need to estimate the chance of
    exceeding the join buffer size. We estimate this probability as:

    (hash_table_usage / join_buffer_size)^2

    for hash_table_usage < join_buffer_size and 1.0 otherwise.
  */
  m_spill_to_disk_probability =
      std::min(1.0, std::pow(hash_table_usage / join_buff_size, 2.0));

  const bool spill{hash_table_usage >= join_buff_size};

  // Number of iterations over the probe relation. We split the build relation
  // in at most 128 chunks. If a build chunks is too big to fit in the join
  // buffer, we will fill the join buffer from the build chunk,
  // iterate over the corresponding probe chunk, and repeat until we have
  // consumed the entire build chunk.
  const double probe_iterations{std::ceil(
      hash_table_usage / (join_buff_size * HashJoinIterator::kMaxChunks))};

  // The volume written to (and read from) build spill files.
  const double build_spill_volume{
      spill ?
            // The volume of the build relation, except for the part that fills
            // the join buffer the first time.
          metrics.build_rows * metrics.build_row_size *
              (1 - join_buff_size / hash_table_usage)
            : 0.0};

  // The volume written to probe spill files.
  const double probe_spill_write_volume{
      spill ? metrics.probe_rows * metrics.probe_row_size : 0.0};

  // The volume read from probe spill files.
  const double probe_spill_read_volume{
      spill ? metrics.probe_rows * metrics.probe_row_size * probe_iterations
            : 0.0};

  m_init_cost =
      metrics.build_rows *
          (kCostPerBuildRow + metrics.build_row_size * kCostPerBuildByte) +
      // We assume that we write the build chunks and then start reading
      // the probe relation before we can output the first row.
      // Therefore we do not count the cost of reading any chunks here.
      build_spill_volume * kCostPerSpillByte;

  m_cost =
      m_init_cost +
      // The cost of doing the probing.
      metrics.probe_rows * probe_iterations * kCostPerProbeOperation +
      // The cost of generating the result row.
      metrics.result_rows *
          (kCostPerResultRow + metrics.build_row_size * kCostPerCopyBackByte) +
      // The cost of reading the build spill files, plus the cost of reading
      // and writing the probe spill files.
      (build_spill_volume + probe_spill_write_volume +
       probe_spill_read_volume) *
          kCostPerSpillByte;
}
