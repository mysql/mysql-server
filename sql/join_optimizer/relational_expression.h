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

#ifndef SQL_JOIN_OPTIMIZER_RELATIONAL_EXPRESSION_H
#define SQL_JOIN_OPTIMIZER_RELATIONAL_EXPRESSION_H

#include <stdint.h>

#include "sql/join_type.h"
#include "sql/mem_root_array.h"
#include "sql/sql_class.h"

class Item_func_eq;

/**
  Represents an expression tree in the relational algebra of joins.
  Expressions are either tables, or joins of two expressions.
  (Joins can have join conditions, but more general filters are
  not represented in this structure.)

  These are used as an abstract precursor to the join hypergraph;
  they represent the joins in the query block more or less directly,
  without any reordering. (The parser should largely have output a
  structure like this instead of TABLE_LIST, but we are not there yet.)
  The only real manipulation we do on them is pushing down conditions,
  identifying equijoin conditions from other join conditions,
  and identifying join conditions that touch given tables (also a form
  of pushdown).
 */
struct RelationalExpression {
  explicit RelationalExpression(THD *thd)
      : join_conditions(thd->mem_root), equijoin_conditions(thd->mem_root) {}

  enum Type {
    INNER_JOIN = static_cast<int>(JoinType::INNER),
    LEFT_JOIN = static_cast<int>(JoinType::OUTER),
    SEMIJOIN = static_cast<int>(JoinType::SEMI),
    ANTIJOIN = static_cast<int>(JoinType::ANTI),

    // Generally supported by the conflict detector only, not the parser
    // or any iterators. We include this because we will be needing it
    // when we actually implement full outer join, and because it helps
    // verifying semijoin correctness in the unit tests (see the CountPlans*
    // tests).
    FULL_OUTER_JOIN = static_cast<int>(JoinType::FULL_OUTER),

    TABLE = 100
  } type;
  table_map tables_in_subtree;

  // If type == TABLE.
  const TABLE_LIST *table;
  Mem_root_array<Item *> join_conditions_pushable_to_this;

  // If type != TABLE. Note that equijoin_conditions will be split off
  // from join_conditions fairly late (at CreateHashJoinConditions()),
  // so often, you will see equijoin conditions in join_condition..
  RelationalExpression *left, *right;
  Mem_root_array<Item *> join_conditions;
  Mem_root_array<Item_func_eq *> equijoin_conditions;
};

#endif  // SQL_JOIN_OPTIMIZER_RELATIONAL_EXPRESSION_H
