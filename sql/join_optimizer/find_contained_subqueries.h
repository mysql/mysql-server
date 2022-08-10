/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_FIND_CONTAINED_SUBQUERIES
#define SQL_JOIN_OPTIMIZER_FIND_CONTAINED_SUBQUERIES 1

#include "sql/item_subselect.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/sql_const.h"
#include "sql/sql_lex.h"

class Item;
class THD;
class Item;
class Query_block;

// Find out which subqueries are contained in this predicate, if any.
// (This only counts IN/ALL/ANY subqueries, ie., those that we consider
// materializing and have not converted to semijoins.) Note that
// calling this repeatedly can be quite expensive, so many callers will want
// to cache this information.
//
// Func should be on the form void func(ContainedSubquery), or something
// compatible.
template <class Func>
void FindContainedSubqueries(THD *thd, Item *condition,
                             const Query_block *outer_query_block,
                             Func &&func) {
  WalkItem(
      condition, enum_walk::POSTFIX,
      [thd, outer_query_block, &func](Item *item) {
        if (!IsItemInSubSelect(item)) {
          return false;
        }
        Item_in_subselect *item_subs = down_cast<Item_in_subselect *>(item);

        // TODO(sgunders): Respect subquery hints, which can force the
        // strategy to be materialize.
        Query_block *query_block = item_subs->unit->first_query_block();
        const bool materializeable =
            item_subs->subquery_allows_materialization(thd, query_block,
                                                       outer_query_block) &&
            query_block->subquery_strategy(thd) ==
                Subquery_strategy::CANDIDATE_FOR_IN2EXISTS_OR_MAT;

        AccessPath *path = item_subs->unit->root_access_path();
        if (path == nullptr) {
          // In rare situations involving IN subqueries on the left side of
          // other IN subqueries, the query block may not be part of the
          // parent query block's list of inner query blocks. If so, it has
          // not been optimized here. Since this is a rare case, we'll just
          // skip it and assign it zero cost.
          return false;
        }

        ContainedSubquery subquery;
        subquery.row_width = 0;
        for (const Item *qb_item : query_block->fields) {
          subquery.row_width += std::min<size_t>(qb_item->max_length, 4096);
        }
        subquery.materializable = materializeable;
        subquery.path = path;
        func(std::move(subquery));
        return false;
      });
}

#endif  // SQL_JOIN_OPTIMIZER_FIND_CONTAINED_SUBQUERIES
