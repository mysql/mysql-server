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

#ifndef SQL_JOIN_OPTIMIZER_COST_MODEL_H_
#define SQL_JOIN_OPTIMIZER_COST_MODEL_H_

#include <algorithm>  // std::clamp
#include <span>

#include "my_base.h"
#include "my_bitmap.h"  // bitmap_bits_set

#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/cost_constants.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/mem_root_array.h"
#include "sql/table.h"

struct AccessPath;
struct ContainedSubquery;
class Item;
class Query_block;
class THD;

/**
   When we make cost estimates, we use this as the maximal length the
   values we get from evaluating an Item (in bytes). Actual values of
   e.g. blobs may be much longer, but even so we use this as an upper
   limit when doing cost calculations. (For context, @see Item#max_length .)
*/
constexpr size_t kMaxItemLengthEstimate = 4096;

/// A fallback cardinality estimate that is used in case the storage engine
/// cannot provide one (like for table functions). It's a fairly arbitrary
/// non-zero value.
constexpr ha_rows kRowEstimateFallback = 1000;

/**
   We model the IO cost for InnoDB tables with the DYNAMIC row format. For
   other storage engines the IO cost is currently set to zero. For other
   InnoDB row formats, the model may not be a good fit.

   We only count the cost of accessing the leaf pages of indexes (clustered
   or unclustered)that are not already in the buffer pool. For tables/indexes
   that are (estimated to be) fully cached we get an IO cost of zero.
   Tables that are small relative to the buffer pool are assumed to be fully
   cached (all pages are in the buffer pool).
   The only operations for which we count IO cost are random and sequential
   page reads.

   The cost of a disk IO operation is modeled as an affine function:

   io_cost = kIOStartCost + no_of_bytes * kIOByteCost

   Note that the granularity of no_of_bytes is the storage engine page size.
   The values used here are derived from measurements in a cloud setting.
   This means that they may be wrong in another context, such as when
   running against a local SSD. Cloud measurements may also give inconsistent
   results due to heterogeneous cloud hardware, and the impact of activity
   in other cloud VMs.
   Ideally we should be able to configure these parameters, or even set them
   dynamically based on observed behavior.
*/
constexpr double kIOStartCost{937.0};

/// The additional cost of reading an extra byte from disk.
constexpr double kIOByteCost{0.0549};

/// This is the estimated fraction of an (innodb) block that is in use
/// (i.e. not free for future inserts).
constexpr double kBlockFillFactor{0.75};

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

/// Array of aggregation terms.
using TermArray = std::span<const Item *const>;

/**
   Estimate the number of rows with a distinct combination of values for
   'terms'. @see EstimateDistinctRowsFromStatistics for additional details.
   @param thd The current thread.
   @param child_rows The number of input rows.
   @param terms The terms for which we estimate the number of unique
                combinations.
   @returns The estimated number of output rows.
*/
double EstimateDistinctRows(THD *thd, double child_rows, TermArray terms);
/**
   Estimate costs and result row count for an aggregate operation.
   @param[in,out] thd The current thread.
   @param[in,out] path The AGGREGATE path.
   @param[in] query_block The Query_block to which 'path' belongs.
 */
void EstimateAggregateCost(THD *thd, AccessPath *path,
                           const Query_block *query_block);

/**
  Estimate costs and result row count for a skip scan operation.
   @param[in] table              The table which is to be scanned
   @param[in] key_idx            The location of the index to be scanned
   @param[in] num_subrange_scans Number of subrange scans to be executed
   @param[in] records            Number of rows to be scanned
   @retval                       Calculated cost value
 */
double EstimateSkipScanCost(TABLE *table, uint key_idx, uint num_subrange_scans,
                            ha_rows records);

/**
  Estimate costs for a group skip scan operation.
   @param[in] table              The table which is to be scanned
   @param[in] key_idx            The location of the index to be scanned
   @param[in] num_groups         Number of GROUP BY groups to be scanned
   @param[in] has_max            True if the query contains MAX(), else false
   @retval                       Calculated cost value
 */
double EstimateGroupSkipScanCost(TABLE *table, uint key_idx, uint num_groups,
                                 bool has_max);

void EstimateDeleteRowsCost(AccessPath *path);
void EstimateUpdateRowsCost(AccessPath *path);

/// Estimate the costs and row count for a STREAM AccessPath.
void EstimateStreamCost(THD *thd, AccessPath *path);

/**
   Estimate the costs and row count for a WINDOW AccessPath. As described in
   @see AccessPath::m_init_cost, the cost to read k out of N rows would be
   init_cost + (k/N) * (cost - init_cost).
*/
void EstimateLimitOffsetCost(AccessPath *path);

/// Estimate the costs and row count for a Temp table Aggregate AccessPath.
void EstimateTemptableAggregateCost(THD *thd, AccessPath *path,
                                    const Query_block *query_block);

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

/**
  Determines whether a given key on a table is both clustered and primary.

  @param table The table to which the index belongs.
  @param key_idx The position of the key in table->key_info[].

  @return True if the key is clustered and primary, false otherwise.
*/
inline bool IsClusteredPrimaryKey(const TABLE *table, unsigned key_idx) {
  if (table->s->is_missing_primary_key()) return false;
  return key_idx == table->s->primary_key &&
         table->file->primary_key_is_clustered();
}

/// The minimum number of bytes to return for row length estimates. This is
/// mostly to guard against returning estimates of zero, which may or may not
/// actually be able to happen in practice.
constexpr unsigned kMinEstimatedBytesPerRow = 8;

/// The maximum number of bytes to return for row length estimates. The current
/// value of this constant is set to cover a wide range of row sizes and should
/// capture the most common row lengths in bytes. We place an upper limit on the
/// estimates since we have performed our calibration within this range, and we
/// would like to prevent cost estimates from running away in case the
/// underlying statistics are off in some instances. In such cases we prefer
/// capping the resulting estimate. As a reasonable upper limit we use half the
/// default InnoDB page size of 2^14 = 16384 bytes.
constexpr unsigned kMaxEstimatedBytesPerRow = 8 * 1024;

/**
   This struct represents the number of bytes we expect to read for a table row.
   Note that the split between b-tree and overflow pages is specific to InnoDB
   and may not be a good fit for other storage engines. Ideally we should
   calculate row size and IO-cost in the handler, in a way that is specific to
   each particular storage engine.
   When using the DYNAMIC row format, an InnoDB B-tree record cannot be bigger
   than about half a page. (The default page size is 16KB). Above that, the
   longest fields are stored in separate overflow pages.
*/
struct BytesPerTableRow {
  /**
      The number of bytes read from the B-tree record. This also includes those
      fields that were not in the projection.
  */
  int64_t record_bytes;

  /**
     The number of bytes read from overflow pages. This is the combined size
     of those long variable-sized fields that are in the projection but stored
     in overflow pages.
  */
  int64_t overflow_bytes;

  /*
    The probability of reading from an overflow page (i.e. an estimate
    of the probability that at least one of the columns in the
    projection overflows).
  */
  double overflow_probability;
};

/**
   Calculate an estimate of the row size of the read set of 'table'.
*/
int64_t CalculateReadSetWidth(const TABLE *table);

/**
   Estimate the average number of bytes that we need to read from the
   storage engine when reading a row from 'table'. This is the size of
   the (b-tree) record and the overflow pages of any field that is
   part of the projection. This is similar to what EstimateBytesPerRowTable()
   does, but this function is intended to do a more accurate but also more
   expensive calculation for tables with potentially large rows (i.e. tables
   with BLOBs or large VARCHAR fields).

   Note that this function tailored for InnoDB (and also for the
   DYNAMIC row format of InnoDB). At some point we may want to move
   this logic to the handler, so that we can customize it for other
   engines as well.

   @param table The target table
   @returns The estimated row size.
*/
BytesPerTableRow EstimateBytesPerRowWideTable(const TABLE *table);

/// We clamp the block size to lie in the interval between the max and min
/// allowed block size for InnoDB (2^12 to 2^16). Ideally we would have a
/// guarantee that stats.block_size has a reasonable value (across all storage
/// engines, types of tables, state of statistics), but in the absence of such
/// a guarantee we clamp to the values for the InnoDB storage engine since the
/// cost model has been calibrated for these values.
inline unsigned ClampedBlockSize(const TABLE *table) {
  constexpr unsigned kMinEstimatedBlockSize = 4096;
  constexpr unsigned kMaxEstimatedBlockSize = 65536;
  return std::clamp(table->file->stats.block_size, kMinEstimatedBlockSize,
                    kMaxEstimatedBlockSize);
}

/**
  Estimates the number of bytes that MySQL must process when reading a row from
  a table, independently of the size of the read set.

  @param table The table to produce an estimate for.

  @returns The estimated row size.

  @note There are two different relevant concepts of bytes per row:

    1. The bytes per row on the storage engine side.
    2. The bytes per row on the server side.

  One the storage engine side, for InnoDB at least, we compute
  stats.mean_rec_length as the size of the data file divided by the number of
  rows in the table.

  On the server side we are interested in the size of the MySQL representation
  in bytes. This could be a more accurate statistic when determining the CPU
  cost of processing a row (i.e., it does not matter very much if InnoDB pages
  are only half-full). As an approximation to the size of a row in bytes on the
  server side we use the length of the record buffer for rows that should
  not exceed the maximal size of an InnoDB B-tree record. (Otherwise, we call
  EstimateBytesPerRowWideTable() to make the estimate).

  Note that when the table fits in a single page stats.mean_rec_length
  will tend to overestimate the record length since it is computed as
  stats.data_file_length / stats.records and the data file length is
  at least a full page which defaults to 16384 bytes (for InnoDB at
  least). We may then get a better estimate from table->s->rec_buff_length.
*/
inline BytesPerTableRow EstimateBytesPerRowTable(const TABLE *table) {
  int64_t max_bytes{0};

  for (uint i = 0; i < table->s->fields; i++) {
    // max_data_length() is the maximal size (in bytes) of this field.
    max_bytes += table->field[i]->max_data_length();
  }

  if (max_bytes < ClampedBlockSize(table) / 2) {
    // The row should fit in a b-tree record.
    return {.record_bytes =
                std::clamp(table->s->rec_buff_length, kMinEstimatedBytesPerRow,
                           kMaxEstimatedBytesPerRow),
            .overflow_bytes = 0,
            .overflow_probability = 0.0};
  }

  // Make a more sophisticated estimate for tables that may have very
  // large rows.
  return EstimateBytesPerRowWideTable(table);
}

/**
  Estimates the number of bytes that MySQL must process when reading a row from
  a secondary index, independently of the size of the read set.

  @param table The table to which the index belongs.
  @param key_idx The position of the key in table->key_info[].

  @return The estimated number of bytes per row in the index.
*/
inline unsigned EstimateBytesPerRowIndex(const TABLE *table, unsigned key_idx) {
  // key_length should correspond to the length of the field(s) of the key in
  // bytes and ref_length is the length of the field(s) of the primary key in
  // bytes. Secondary indexes (in InnoDB) contain a copy of the value of the
  // primary key associated with a given row, in order to make it possible to
  // retrieve the corresponding row from the primary index in case we use a
  // non-covering index operation.
  unsigned estimate =
      table->key_info[key_idx].key_length + table->file->ref_length;
  return std::clamp(estimate, kMinEstimatedBytesPerRow,
                    kMaxEstimatedBytesPerRow);
}

/**
  Estimates the height of a B-tree index.

  We estimate the height of the index to be the smallest positive integer h such
  that table_records <= (1 + records_per_page)^h.

  This function supports both clustered primary indexes and secondary indexes.
  Secondary indexes will tend to have more records per page compared to primary
  clustered indexes and as a consequence they will tend to be shorter.

  @param table The table to which the index belongs.
  @param key_idx The position of the key in table->key_info[].

  @return The estimated height of the index.
*/
inline int IndexHeight(const TABLE *table, unsigned key_idx) {
  unsigned block_size = ClampedBlockSize(table);
  unsigned bytes_per_row = IsClusteredPrimaryKey(table, key_idx)
                               ? table->bytes_per_row()->record_bytes
                               : EstimateBytesPerRowIndex(table, key_idx);

  // Ideally we should always have that block_size >= bytes_per_row, but since
  // the storage engine and MySQL row formats differ, this is not always the
  // case. Therefore we manually ensure that records_per_page >= 1.0.
  double records_per_page =
      std::max(1.0, static_cast<double>(block_size) / bytes_per_row);

  // Computing the height using a while loop instead of using std::log turns out
  // to be about 5 times faster in microbenchmarks when the measurement is made
  // using a somewhat realistic and representative set of values for the number
  // of records per page and the number of records in the table. In the worst
  // case, if the B-tree contains only a single record per page, the table would
  // have to contain 2^30 pages (corresponding to more than 16 terabytes of
  // data) for this loop to run 30 times. A B-tree with 1 billion records and
  // 100 records per page only uses 4 iterations of the loop (the height is 5).
  int height = 1;
  double r = 1.0 + records_per_page;
  while (r < table->file->stats.records) {
    r = r * (1.0 + records_per_page);
    height += 1;
  }
  return height;
}

/// Calculate the IO-cost of reading 'num_rows' rows from 'table'.
double TableAccessIOCost(const TABLE *table, double num_rows,
                         BytesPerTableRow row_size);

/// Calculate the IO-cost of doing a lookup on index 'key_idx' on 'table'
/// and then read 'num_rows' rows.
double CoveringIndexAccessIOCost(const TABLE *table, unsigned key_idx,
                                 double num_rows);

/**
  Computes the expected cost of reading a number of rows. The cost model takes
  into account the number of fields that is being read from the row and the
  width of the row in bytes. Both RowReadCostTable() and RowReadCostIndex() call
  this function and thus use the same cost model.

  @param num_rows The (expected) number of rows to read.
  @param fields_read_per_row The number of fields to read per row.
  @param bytes_per_row The total length of the row to be processed (including
  fields that are not read) in bytes.

  @returns The expected cost of reading num_rows.

  @note It is important that this function be robust to fractional row
  estimates. For example, if we index nested loop join two primary key columns
  and the inner table is smaller than the outer table, we should see that
  num_rows for the inner index lookup is less than one. In this case it is
  important that we return the expected cost of the operation. For example, if
  we only expect to read 0.1 rows the cost should be 0.1 of the cost of reading
  one row (we are working with a linear cost model, so we only have to take the
  expected number of rows into account, and not the complete distribution).
*/
inline double RowReadCost(double num_rows, double fields_read_per_row,
                          double bytes_per_row) {
  return (kReadOneRowCost + kReadOneFieldCost * fields_read_per_row +
          kReadOneByteCost * bytes_per_row) *
         num_rows;
}

/**
  Computes the cost of reading a number of rows from a table.
  @see ReadRowCost() for further details.

  @param table The table to read from.
  @param num_rows The (expected) number of rows to read.

  @returns The cost of reading num_rows.
*/
inline double RowReadCostTable(const TABLE *table, double num_rows) {
  double fields_read_per_row = bitmap_bits_set(table->read_set);
  const BytesPerTableRow &bytes_per_row = *table->bytes_per_row();
  return RowReadCost(
             num_rows, fields_read_per_row,
             bytes_per_row.record_bytes + bytes_per_row.overflow_bytes) +
         TableAccessIOCost(table, num_rows, bytes_per_row);
}

/**
  Computes the cost of reading a number of rows from an index.
  @see ReadRowCost() for further details.

  @param table The table to which the index belongs.
  @param key_idx The position of the key in table->key_info[].
  @param num_rows The (expected) number of rows to read.

  @returns The cost of reading num_rows.
*/
inline double RowReadCostIndex(const TABLE *table, unsigned key_idx,
                               double num_rows) {
  if (IsClusteredPrimaryKey(table, key_idx)) {
    return RowReadCostTable(table, num_rows);
  }
  // Assume we read two fields from the index record if it is not covering. The
  // exact assumption here is not very important as the cost should be dominated
  // by the additional lookup into the primary index.
  constexpr double kDefaultFieldsReadFromCoveringIndex = 2;
  double fields_read_per_row = table->covering_keys.is_set(key_idx)
                                   ? bitmap_bits_set(table->read_set)
                                   : kDefaultFieldsReadFromCoveringIndex;

  double bytes_per_row = EstimateBytesPerRowIndex(table, key_idx);
  return RowReadCost(num_rows, fields_read_per_row, bytes_per_row) +
         CoveringIndexAccessIOCost(table, key_idx, num_rows);
}

/**
  Estimates the cost of a full table scan. Primarily used to assign a cost to
  the TABLE_SCAN access path.

  @param table The table to estimate cost for.

  @returns The cost of scanning the table.
*/
inline double EstimateTableScanCost(const TABLE *table) {
  return RowReadCostTable(table, table->file->stats.records);
}

/**
  Estimates the cost of an index lookup.

  @param table The table to which the index belongs.
  @param key_idx The position of the key in table->key_info[].

  @return The estimated cost of an index lookup.

  @note The model "cost ~ index_height" works well when the Adaptive Hash Index
  (AHI) is disabled. The AHI essentially works as a dynamic cache for the most
  frequently accessed index pages that sits on top of the B-tree. With AHI
  enabled the cost of random lookups does not appear to be predictable using
  standard explanatory variables such as index height or the logarithm of the
  number of rows in the index. The performance of AHI will also be dependent on
  the access pattern, so it is fundamentally difficult to create an accurate
  model. However, our calibration experiments reveal two things that hold true
  both with and without AHI enabled:

  1. Index lookups usually take 1-3 microseconds. The height of a B-tree grows
     very slowly (proportional to log(N)/log(R) for tables with N rows and R
     records per page), making it near-constant in the common case where tables
     have many records per page, even without AHI.

  2. Random lookups in large indexes tend to be slower. A random access pattern
     will cause more cache misses, both for regular hardware caching and AHI. In
     addition, for a larger B-tree only the top levels fit in cache and we will
     invariably see a few cache misses with random accesses.

  We model the cost of index lookups by interpolating between a model with
  constant cost and a model that depends entirely on the height of the index.
  The constants are based on calibration experiments with and without AHI.

  Another factor that is important when calibrating the cost of index lookups is
  whether we are interested in the average cost when performing many lookups
  such as when performing an index nested loop join or scanning along a
  secondary non-covering index and lookup into the primary index, or the cost of
  a single lookup such as a point select that uses an index. From our
  measurements we see that the average running time of an index lookup can
  easily be a factor ~5x faster when performing thousands of successive lookups
  compared to just one. This is likely due to hardware caching effects. Since
  getting the cost right in the case when we perform many lookups is typically
  more important, we have opted to calibrate costs based on operations that
  perform many lookups.

  For adding IO costs to this model (future work) we probably want to assume
  that we only fetch a single page when performing an index lookup, as
  everything but leaf pages will tend to be cached, at least when performing
  many index lookups in a query plan, which is exactly the case where it is
  important to get costs right.
*/
inline double IndexLookupCost(const TABLE *table, unsigned key_idx) {
  assert(key_idx < table->s->keys);
  double cost_with_ahi = kIndexLookupFixedCost;
  double cost_without_ahi = kIndexLookupPageCost * IndexHeight(table, key_idx);
  return 0.5 * (cost_with_ahi + cost_without_ahi);
}

/// Type of range scan operation.
enum class RangeScanType : char {
  /// Using the MRR optimization.
  kMultiRange,

  /// Plain range scan, without the MRR optimization.
  kSingleRange
};

/**
   Estimates the cost of an index range scan.

   The cost model for index range scans accounts for the index lookup cost as
   well as the cost of reading rows. Both index scans and ref accesses can be
   viewed as special cases of index range scans, so the cost functions for those
   operations call this function under the hood.

  @note In the future this function should be extended to account for IO cost.

   @param table The table to which the index belongs.
   @param key_idx The position of the key in table->key_info[].
   @param scan_type Whether this an MRR or a regular index range scan.
   @param num_ranges The number of ranges.
   @param num_output_rows The estimated expected number of output rows.

   @returns The estimated cost of the index range scan operation.
*/
double EstimateIndexRangeScanCost(const TABLE *table, unsigned key_idx,
                                  RangeScanType scan_type, double num_ranges,
                                  double num_output_rows);

/**
   Estimates the cost of an index scan. An index scan scans all rows in the
   table along the supplied index.

   @param table The table to which the index belongs.
   @param key_idx The position of the key in table->key_info[].

   @returns The estimated cost of the index scan.
*/
inline double EstimateIndexScanCost(const TABLE *table, unsigned key_idx) {
  if (IsClusteredPrimaryKey(table, key_idx)) {
    return EstimateTableScanCost(table);
  }

  return EstimateIndexRangeScanCost(table, key_idx, RangeScanType::kSingleRange,
                                    1.0, table->file->stats.records);
}

/**
   Estimates the cost of an index lookup (ref access).

   @param table The table to which the index belongs.
   @param key_idx The position of the key in table->key_info[].
   @param num_output_rows The estimated number of output rows.

   @returns The estimated cost of the index scan.
*/
inline double EstimateRefAccessCost(const TABLE *table, unsigned key_idx,
                                    double num_output_rows) {
  // We want the optimizer to prefer ref acccesses to range scans when they both
  // have the same cost. This is particularly important for the EQ_REF access
  // path (index lookups with at most one matching row) since the EQ_REF
  // iterator uses caching to improve performance.
  constexpr double kRefAccessCostDiscount = 0.05;
  return (1.0 - kRefAccessCostDiscount) *
         EstimateIndexRangeScanCost(table, key_idx, RangeScanType::kSingleRange,
                                    1.0, num_output_rows);
}

/**
   Input to HashJoinCost, for calculating the cost of a hash join.
*/
struct HashJoinMetrics final {
  /// The number of rows in the 'build' input.
  double build_rows;
  /// The average size of rows in the 'build' input (in bytes).
  double build_row_size;
  /// The size of the join key, in bytes.
  double key_size;
  /// The number of rows in the 'probe' input.
  double probe_rows;
  /// The average size of rows in the 'probe' input (in bytes).
  double probe_row_size;
  /// The number of rows in the result set.
  double result_rows;
};

/** This class represents the cost of a hash join (excluding the cost
    of sub-paths).
*/
class HashJoinCost final {
 public:
  HashJoinCost(THD *thd, const HashJoinMetrics &metrics);

  double spill_to_disk_probability() const {
    return m_spill_to_disk_probability;
  }

  double init_cost() const { return m_init_cost; }

  double cost() const { return m_cost; }

 private:
  /// The probability (range [0.0, 1.0]) of needing spill to disk.
  double m_spill_to_disk_probability;
  /// The cost of preparing to produce the first result row.
  double m_init_cost;
  /// The cost of the hash join.
  double m_cost;
};

#endif  // SQL_JOIN_OPTIMIZER_COST_MODEL_H_
