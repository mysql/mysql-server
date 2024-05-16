/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_COST_CONSTANTS_H_
#define SQL_JOIN_OPTIMIZER_COST_CONSTANTS_H_

/**
  @file sql/join_optimizer/cost_constants.h Hypergraph optimizer cost constants.

  This file contains cost constants that are used during optimization by the
  hypergraph optimizer. Ideally all (server) cost constants should be contained
  in this file, but some code paths might still lead to the old cost model (with
  constants in sql/opt_costconstants.h).

  As we integrate more storage engines into the cost model we may add
  engine-specific constants. Eventually we might make some constants (or groups
  of related constants) user-configurable to provide users with the opportunity
  to customize the cost model to better reflect their actual costs.

  The cost constants here have generally been calibrated in microseconds using
  regression analysis on a release build of the server. In order to avoid tying
  these constants to the execution time on a particular machine we define a cost
  unit in terms of a fundamental operation in MySQL (reading a row during a
  table scan, @see kUnitCostInMicroseconds). Cost constants are then defined
  relative to the unit cost, with the idea that the ratio between running times
  is less sensitive to changes in hardware.

  For this batch of constants we include a particular measure of the unit cost
  in terms of microseconds. When adjusting the cost model in the future the
  following approach should be adopted:

    1. Determine the unit cost c1 in microseconds.
    2. Determine the cost c2 of the constant of interest in microseonds.
    3. Set the value of the constant to the ratio c2 / c1.
*/

/// We define the cost unit for the MySQL hypergraph cost model as follows: A
/// cost of 1.0 represents the average cost per row of a "SELECT * FROM t" table
/// scan where t is an InnoDB table with ten integer columns and one million
/// rows. We assume that the InnoDB table is optimized (pages are full) and
/// loaded into the buffer pool.
constexpr double kUnitCostInMicroseconds = 0.434;

/*
  To compute the server cost of reading a row we use the following three
  constants: kReadOneRowCost, kReadOneFieldCost, and kReadOneBytecost. For a
  table scan under InnoDB the cost of reading one row is based on the following
  model which has been calibrated using linear regression and predicts actual
  running time well on tables with integer columns:

  cost of reading a single row = kReadOneRowCost + kReadOneFieldCost *
  num_fields_in_read_set + kReadOneByteCost * length_of_record_buffer_in_bytes.

  In the future the cost model for reading rows may be extended to include
  storage engine specific costs and IO cost.
*/

/// Fixed cost of reading a row from the storage engine into the record buffer.
/// Used in base table access paths such as TABLE_SCAN, INDEX_SCAN,
/// INDEX_RANGE_SCAN.
constexpr double kReadOneRowCost = 0.1 / kUnitCostInMicroseconds;

/// Cost of per field in the read set. Used to account for the increase in cost
/// when reading more fields from a row.
constexpr double kReadOneFieldCost = 0.02 / kUnitCostInMicroseconds;

/// Overhead per byte when reading a row. With a row-based format we have to
/// process more data to extract the same number of fields when rows are larger,
/// as measured by row length in bytes.
///
/// Note: This constant has been calibrated on tables with integer columns. We
/// should therefore be careful about applying this cost to variable-length
/// fields that are stored off-page. We use the length of the record buffer
/// (TABLE_SHARE::rec_buff_length).
constexpr double kReadOneByteCost = 0.001 / kUnitCostInMicroseconds;

/// Cost of evaluating one filter on one row. Calibrated using simple integer
/// filters, e.g. x < k, so it might be prudent to use a higher number, but then
/// again, almost everything is calibrated on integers.
///
/// From calibration experiments we would prefer a cost model for filtering to
/// consist of a fixed cost for filtering the row, together with a variable cost
/// for the number of filter operations:
///
/// cost = kFilterOneRowCost + kApplyOneFilterCost * num_filter_evaluations
///
/// The expected number of filter evaluations for a row can be estimated. For
/// example, the condition x < k1 AND x < k2 will require more filter
/// evaluations if the selectivity of x < k1 is high, as then the second
/// condition will also have to be evaluated. If we consider x < k1 OR x < k2,
/// then a low selectivity of the first term will make it likely that the second
/// term will have to be evaluated as well. Unfortunately the current cost model
/// only provides partial support for these mechanisms, and does not support
/// using a fixed filtering cost per row, so the constant has been adjusted to
/// reflect this, pending a rewrite/refactoring of the filtering cost.
/// @note See the filtering section in hypergraph_cost_model.test.
constexpr double kApplyOneFilterCost = 0.025 / kUnitCostInMicroseconds;

// For index lookups the Adaptive Hash Index (AHI) makes it difficult to
// accurately predict costs. We opt to interpolate between a cost model with and
// without AHI. See IndexLookupCost() in cost_model.h for further details.

/// The cost per page that is visited when performing an index lookup in an
/// InnoDB B-tree. When the Adaptive Hash Index (AHI) is disabled the number of
/// pages visited when performing an index lookup is equal to the height of the
/// index since we traverse the tree from the root node to a leaf node,
/// performing a binary search within each page. This constant has been
/// calibrated with AHI disabled.
constexpr double kIndexLookupPageCost = 0.5 / kUnitCostInMicroseconds;

/// Fixed cost of an index lookup when AHI is enabled (default).
constexpr double kIndexLookupFixedCost = 1.0 / kUnitCostInMicroseconds;

/// Default cost of an index lookup when we are missing information to compute a
/// more accurate cost estimate. Used e.g. with the MEMORY engine when computing
/// the cost of index operations on a secondary non-covering index.
// TODO(tlchrist): Calibrate this.
constexpr double kIndexLookupDefaultCost = 1.0 / kUnitCostInMicroseconds;

/// Fixed overhead per input row when sorting. This represents the cost of
/// reading a row into the sort buffer. The accuracy of the cost model could be
/// further improved if we take into account the amount of data that is read
/// into the sort buffer.
constexpr double kSortOneRowCost = 0.15 / kUnitCostInMicroseconds;

/// Cost per comparison during sorting. Calibrated using ORDER BY on a single
/// INT column. The cost is of course higher if we sort on multiple columns, and
/// of the data type is something more complex, but not so much higher that it
/// is clear that it would be worth taking this into account in the cost model.
constexpr double kSortComparisonCost = 0.014 / kUnitCostInMicroseconds;

/// Hash join constants.
constexpr double kHashBuildOneRowCost = 0.65 / kUnitCostInMicroseconds;
constexpr double kHashProbeOneRowCost = 0.09 / kUnitCostInMicroseconds;
constexpr double kHashReturnOneRowCost = 0.06 / kUnitCostInMicroseconds;

/// In need of calibration.
constexpr double kAggregateOneRowCost = 0.1 / kUnitCostInMicroseconds;
constexpr double kStreamOneRowCost = 0.01 / kUnitCostInMicroseconds;
constexpr double kMaterializeOneRowCost = 0.1 / kUnitCostInMicroseconds;
constexpr double kWindowOneRowCost = 0.1 / kUnitCostInMicroseconds;
constexpr double kTempTableAggLookupCost = 0.1 / kUnitCostInMicroseconds;

#endif  // SQL_JOIN_OPTIMIZER_COST_CONSTANTS_H_
