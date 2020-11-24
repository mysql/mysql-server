/* Copyright (c) 2020, Oracle and/or its affiliates.

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
  // Corresponds to MaterializeIterator::QueryBlock; see it for documentation.
  struct QueryBlock {
    AccessPath *subquery_path;
    int select_number;
    JOIN *join;
    bool disable_deduplication_by_hash_field;
    bool copy_fields_and_items;
    Temp_table_param *temp_table_param;
    bool is_recursive_reference;
  };
  Mem_root_array<QueryBlock> query_blocks;
  Mem_root_array<const AccessPath *> *invalidators;
  TABLE *table;
  Common_table_expr *cte;
  Query_expression *unit;
  int ref_slice;
  bool rematerialize;
  ha_rows limit_rows;
  bool reject_multiple_rows;
};

#endif  // !defined(MATERIALIZE_PATH_PARAMETERS_H)
