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

#ifndef MATERIALIZE_PATH_PARAMETERS_H
#define MATERIALIZE_PATH_PARAMETERS_H 1

// Split out into its own file to reduce the amount of dependencies on
// access_path.h.

#include "sql/mem_root_array.h"
#include "sql/sql_class.h"

struct AccessPath;
struct TABLE;
class JOIN;
class Temp_table_param;
class Common_table_expr;
class Query_expression;

struct MaterializePathParameters {
  // Corresponds to MaterializeIterator::Operand; see it for documentation.
  struct Operand {
    AccessPath *subquery_path;
    int select_number;
    JOIN *join;
    bool disable_deduplication_by_hash_field;
    bool copy_items;
    Temp_table_param *temp_table_param;
    bool is_recursive_reference;
    /// The block no is the first to be materialized with DISTINCT: for EXCEPT
    /// set operation in a materialization for EXCEPT.
    uint m_first_distinct{0};
    /// The index of this block number
    uint m_operand_idx{0};
    /// The number of materialized blocks, i.e. set operands
    uint m_total_operands{0};
  };
  Mem_root_array<Operand> m_operands;
  Mem_root_array<const AccessPath *> *invalidators;

  /// Handle to table to materialize into.
  TABLE *table;

  /// If materializing a CTE, points to it (see m_cte), otherwise nullptr.
  Common_table_expr *cte;

  /// The query expression we are materializing.
  Query_expression *unit;

  /**
      @see JOIN. If we are materializing across JOINs, e.g. derived tables,
      ref_slice should be left at -1.
  */
  int ref_slice;

  /**
     True if rematerializing on every Init() call (e.g., because we
     have a dependency on a value from outside the query block).
  */
  bool rematerialize;

  /**
     Used for when pushing LIMIT down to MaterializeIterator; this is
     more efficient than having a LimitOffsetIterator above the
     MaterializeIterator, since we can stop materializing when there are
     enough rows. (This is especially important for recursive CTEs.)
     Note that we cannot have a LimitOffsetIterator _below_ the
     MaterializeIterator, as that would count wrong if we have deduplication,
     and would not work at all for recursive CTEs.
     Set to HA_POS_ERROR for no limit.
  */
  ha_rows limit_rows;

  /**
     True if this is the top level iterator for a
     materialized derived table transformed from a scalar subquery which needs
     run-time cardinality check.
  */
  bool reject_multiple_rows;
};

#endif  // !defined(MATERIALIZE_PATH_PARAMETERS_H)
