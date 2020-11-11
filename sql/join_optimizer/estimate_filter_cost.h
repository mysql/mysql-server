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

#ifndef SQL_JOIN_OPTIMIZER_ESTIMATE_FILTER_COST_H
#define SQL_JOIN_OPTIMIZER_ESTIMATE_FILTER_COST_H

class Item;
class Query_block;
class THD;

/// See EstimateFilterCost.
struct FilterCost {
  // Cost of evaluating the filter if nothing in particular is done with it.
  double cost_if_not_materialized;

  // Cost of evaluating the filter if all subqueries in it have been
  // materialized beforehand. If there are no subqueries in the condition,
  // equals cost_if_not_materialized.
  double cost_if_materialized;

  // Cost of materializing all subqueries present in the filter.
  // If there are no subqueries in the condition, equals zero.
  double cost_to_materialize;
};

/**
  Estimate the cost of evaluating “condition”, “num_rows” times.
  This is a fairly rudimentary estimation, _but_ it includes the cost
  of any subqueries that may be present and that need evaluation.
 */
FilterCost EstimateFilterCost(THD *thd, double num_rows, Item *condition,
                              Query_block *outer_query_block);

#endif  // SQL_JOIN_OPTIMIZER_ESTIMATE_FILTER_COST_H
