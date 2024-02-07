/* Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_UNION_INCLUDED
#define SQL_UNION_INCLUDED

#include <sys/types.h>

#include "my_inttypes.h"
#include "sql/query_result.h"  // Query_result_interceptor
#include "sql/table.h"
#include "sql/temp_table_param.h"  // Temp_table_param

class Item;
class Query_expression;
class THD;
template <class Element_type>
class mem_root_deque;

class Query_result_union : public Query_result_interceptor {
 protected:
  Temp_table_param tmp_table_param;

 public:
  TABLE *table;

  Query_result_union() : Query_result_interceptor(), table(nullptr) {}
  bool prepare(THD *thd, const mem_root_deque<Item *> &list,
               Query_expression *u) override;
  /**
    Do prepare() if preparation has been postponed until column type
    information is computed (used by Query_result_union_direct).

    @param thd   Thread handle
    @param types Column types

    @return false on success, true on failure
  */
  virtual bool postponed_prepare(THD *thd [[maybe_unused]],
                                 const mem_root_deque<Item *> &types
                                 [[maybe_unused]]) {
    return false;
  }
  bool send_data(THD *thd, const mem_root_deque<Item *> &items) override;
  bool send_eof(THD *thd) override;
  virtual bool flush();
  void cleanup() override { (void)reset(); }
  bool reset() override;
  bool create_result_table(THD *thd, const mem_root_deque<Item *> &column_types,
                           bool is_distinct, ulonglong options,
                           const char *alias, bool bit_fields_as_long,
                           bool create_table, Query_term_set_op *op = nullptr);
  friend bool Table_ref::create_materialized_table(THD *thd);
  friend bool Table_ref::optimize_derived(THD *thd);
  uint get_hidden_field_count() const {
    return tmp_table_param.hidden_field_count;
  }
  bool skip_create_table() const { return tmp_table_param.skip_create_table; }

  /// Set an effective LIMIT for the number of rows coming out of a materialized
  /// temporary table used for implementing INTERSECT or EXCEPT: informs
  /// TableScanIterator::TableScanIterator how many rows to read from the
  /// materialized table. For UNION and simple tables the limitation is enforced
  /// earlier, at materialize time, but this is not possible for INTERSECT and
  /// EXCEPT due to the use of cardinality counters.
  ///
  /// @param limit_rows the effective limit, or HA_POS_ERROR if none.
  void set_limit(ha_rows limit_rows) override;

  // For assignment of tmp_table_param for CTE clones
  friend class Common_table_expr;
};

#endif /* SQL_UNION_INCLUDED */
