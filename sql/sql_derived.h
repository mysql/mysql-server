/* Copyright (c) 2006, 2020, Oracle and/or its affiliates.

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

#ifndef SQL_DERIVED_INCLUDED
#define SQL_DERIVED_INCLUDED

#include "sql/item.h"
#include "sql/opt_trace_context.h"
#include "sql/sql_const.h"
struct NESTED_JOIN;
/**
  Class which handles pushing conditions down to a materialized derived
  table. In Query_block::prepare, if it is the outermost query block, and
  if we are at the end of preparation, a WHERE condition from the query
  block is checked to see if it can be pushed to the materialized derived
  table.
  Query_block::prepare
  push_conditions_to_derived_tables()
    For every materialized derived table,
    If there is a where condition in this query block,
      Make condition that can be pushed down to the derived table.
        Extract a part of the condition that has columns belonging to only
        this derived table.
        Check if this condition can be pushed past window functions if any to
        the HAVING clause of the derived table
          Make a condition that could not be pushed past. This will remain in
          the outer query block.
        Check if this condition can be pushed past group by if present to the
        WHERE clause of the derived table.
          Make a condition that could not be pushed past. This will be part of
          the HAVING clause of the derived table query.
      Get the remainder condition which could not be pushed to the derived
      the derived table.
      Push the condition down to derived table's query expression
      REPEAT THE ABOVE for the rest of the derived tables.
   For every query expression inside the current query block
     REPEAT THE ABOVE to keep pushing as far down as possible.
*/

class Condition_pushdown {
 public:
  Condition_pushdown(Item *cond, TABLE_LIST *derived, THD *thd_arg,
                     Opt_trace_context *trace_arg)
      : m_cond_to_check(cond),
        m_derived_table(derived),
        thd(thd_arg),
        trace(trace_arg) {}

  bool make_cond_for_derived();
  Item *make_remainder_cond(Item *cond);
  Item *get_remainder_cond() { return m_remainder_cond; }

 private:
  Item *extract_cond_for_table(Item *cond);
  bool replace_columns_in_cond();
  void push_past_window_functions();
  void push_past_group_by();
  bool attach_cond_to_derived(Item *derived_cond, Item *cond_to_attach,
                              bool having);
  void update_between_count(Item *cond);
  void check_and_remove_sj_exprs(Item *cond);
  void remove_sj_exprs(Item *cond, NESTED_JOIN *sj_nest);

  /// Condition that needs to be checked to push down to the derived table.
  Item *m_cond_to_check;

  /// Derived table to push the condition to.
  TABLE_LIST *m_derived_table;

  /**
   Condition that is extracted from outer WHERE condition to be pushed to
   the derived table.
  */
  Item *m_cond_to_push{nullptr};

  /**
    Condition that would be attached to the HAVING clause of the derived
    table.
   */
  Item *m_having_cond{nullptr};

  /**
    Condition that would be attached to the WHERE clause of the derived table.
   */
  Item *m_where_cond{nullptr};

  /**
    Condition that would be left behind in the outer select. This is the
    condition that could not be pushed down to the derived table.
  */
  Item *m_remainder_cond{nullptr};

  /**
    Enum that represents various stages of checking.
    CHECK_FOR_DERIVED - Checking if a condition has only derived table
                        expressions.
    CHECK_FOR_HAVING  - Checking if condition could be pushed to HAVING
                        clause of the derived table.
    CHECK_FOR_WHERE   - Checking if condition could be pushed to WHERE
                        clause of the derived table.
  */
  enum enum_checking_purpose {
    CHECK_FOR_DERIVED,
    CHECK_FOR_HAVING,
    CHECK_FOR_WHERE
  };
  enum_checking_purpose m_checking_purpose{CHECK_FOR_DERIVED};

  /// Current thread
  THD *thd;

  /// Optimizer trace context
  Opt_trace_context *trace;
};

#endif /* SQL_DERIVED_INCLUDED */
