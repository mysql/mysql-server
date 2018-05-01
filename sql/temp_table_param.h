/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TEMP_TABLE_PARAM_INCLUDED
#define TEMP_TABLE_PARAM_INCLUDED

#include <vector>

#include "my_base.h"
#include "sql/mem_root_array.h"
#include "sql/sql_list.h"
#include "sql/thr_malloc.h"
#include "sql/window.h"

struct MI_COLUMNDEF;
class KEY;
class Copy_field;
class Item;
class Window;

template <typename T>
using Memroot_vector = std::vector<T, Memroot_allocator<T>>;

/**
   Helper class for copy_funcs(); represents an Item to copy from table to
   next tmp table.
*/
class Func_ptr {
 public:
  Func_ptr(Item *f) : m_func(f), m_contains_alias_of_expr(false) {}
  /**
    Calculates if m_func contains an alias to an expression of the SELECT list
    of 'select'.
    @param          select     query block to search in.
    @returns the true/false result and also stores it in the object.
  */
  bool set_contains_alias_of_expr(const SELECT_LEX *select);
  /// @returns the previously calculated information.
  bool contains_alias_of_expr() const { return m_contains_alias_of_expr; }
  Item *func() const { return m_func; }

 private:
  Item *m_func;
  bool m_contains_alias_of_expr;
};

/// Used by copy_funcs()
typedef Mem_root_array<Func_ptr> Func_ptr_array;

/**
  Object containing parameters used when creating and using temporary
  tables. Temporary tables created with the help of this object are
  used only internally by the query execution engine.
*/

class Temp_table_param {
 public:
  /**
    Used to store the values of grouped non-column-reference expressions in
    between groups, so they can be retreived when the group changes.

    @see setup_copy_fields
    @see copy_fields
  */
  Memroot_vector<Item_copy *> grouped_expressions;
  Memroot_vector<Copy_field> copy_fields;

  uchar *group_buff;
  Func_ptr_array *items_to_copy; /* Fields in tmp table */
  MI_COLUMNDEF *recinfo, *start_recinfo;

  /**
    After temporary table creation, points to an index on the table
    created depending on the purpose of the table - grouping,
    duplicate elimination, etc. There is at most one such index.
  */
  KEY *keyinfo;
  ha_rows end_write_records;
  /**
    Number of normal fields in the query, including those referred to
    from aggregate functions. Hence, "SELECT `field1`,
    SUM(`field2`) from t1" sets this counter to 2.

    @see count_field_types
  */
  uint field_count;
  /**
    Number of fields in the query that have functions. Includes both
    aggregate functions (e.g., SUM) and non-aggregates (e.g., RAND)
    and windowing functions.
    Also counts functions referred to from windowing or aggregate functions,
    i.e., "SELECT SUM(RAND())" sets this counter to 2.

    @see count_field_types
  */
  uint func_count;
  /**
    Number of fields in the query that have aggregate functions. Note
    that the optimizer may choose to optimize away these fields by
    replacing them with constants, in which case sum_func_count will
    need to be updated.

    @see opt_sum_query, count_field_types
  */
  uint sum_func_count;
  uint hidden_field_count;
  uint group_parts, group_length, group_null_parts;
  uint quick_group;
  /**
    Number of outer_sum_funcs i.e the number of set functions that are
    aggregated in a query block outer to this subquery.

    @see count_field_types
  */
  uint outer_sum_func_count;
  /**
    Enabled when we have atleast one outer_sum_func. Needed when used
    along with distinct.

    @see create_tmp_table
  */
  bool using_outer_summary_function;
  CHARSET_INFO *table_charset;
  bool schema_table;
  /*
    True if GROUP BY and its aggregate functions are already computed
    by a table access method (e.g. by loose index scan). In this case
    query execution should not perform aggregation and should treat
    aggregate functions as normal functions.
  */
  bool precomputed_group_by;
  bool force_copy_fields;
  /**
    true <=> don't actually create table handler when creating the result
    table. This allows range optimizer to add indexes later.
    Used for materialized derived tables/views.
    @see TABLE_LIST::update_derived_keys.
  */
  bool skip_create_table;
  /*
    If true, create_tmp_field called from create_tmp_table will convert
    all BIT fields to 64-bit longs. This is a workaround the limitation
    that MEMORY tables cannot index BIT columns.
  */
  bool bit_fields_as_long;
  /// Whether the UNIQUE index can be promoted to PK
  bool can_use_pk_for_unique;

  bool
      m_window_short_circuit;  ///< (Last) window's tmp file step can be skipped
  /// If this is the out table of a window: the said window
  Window *m_window;

  Temp_table_param(MEM_ROOT *mem_root = *THR_MALLOC)
      : grouped_expressions(Memroot_allocator<Item_copy *>(mem_root)),
        copy_fields(Memroot_allocator<Copy_field>(mem_root)),
        group_buff(nullptr),
        items_to_copy(nullptr),
        recinfo(NULL),
        start_recinfo(NULL),
        keyinfo(NULL),
        end_write_records(0),
        field_count(0),
        func_count(0),
        sum_func_count(0),
        hidden_field_count(0),
        group_parts(0),
        group_length(0),
        group_null_parts(0),
        quick_group(1),
        outer_sum_func_count(0),
        using_outer_summary_function(false),
        table_charset(NULL),
        schema_table(false),
        precomputed_group_by(false),
        force_copy_fields(false),
        skip_create_table(false),
        bit_fields_as_long(false),
        can_use_pk_for_unique(true),
        m_window_short_circuit(false),
        m_window(nullptr) {}

  void cleanup() {
    grouped_expressions.clear();
    copy_fields.clear();
  }
};

#endif  // TEMP_TABLE_PARAM_INCLUDED
