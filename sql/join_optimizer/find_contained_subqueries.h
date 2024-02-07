/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

/***
Find out which subqueries are contained in this predicate, if any.
(This only counts IN/ALL/ANY/comparison_operator subqueries, ie.,
those that we consider materializing and have not converted to
semijoins.) Note that calling this repeatedly can be quite expensive,
so many callers will want to cache this information.

Func should be on the form void func(ContainedSubquery), or something
compatible.

@param condition the root of the predicate.
@param outer_query_block the Query_block to which 'condition' belongs.
@param func a function that is called for each contained subquery.
*/
template <class Func>
void FindContainedSubqueries(Item *condition,
                             const Query_block *outer_query_block,
                             Func &&func) {
  WalkItem(condition, enum_walk::POSTFIX,
           [outer_query_block, &func](Item *item) {
             std::optional<ContainedSubquery> subquery =
                 item->get_contained_subquery(outer_query_block);

             if (subquery.has_value()) {
               func(subquery.value());
             }
             return false;
           });
}

#endif  // SQL_JOIN_OPTIMIZER_FIND_CONTAINED_SUBQUERIES
