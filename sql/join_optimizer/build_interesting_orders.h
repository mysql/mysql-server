/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_BUILD_INTERESTING_ORDERS_H_
#define SQL_JOIN_OPTIMIZER_BUILD_INTERESTING_ORDERS_H_

#include "sql/join_optimizer/interesting_orders.h"
#include "sql/join_optimizer/node_map.h"

#include <string>

class Item_func_match;
template <class T>
class Mem_root_array;
class Query_block;
class THD;
struct JoinHypergraph;
struct ORDER;
struct TABLE;

// An ordering that we could be doing sort-ahead by; typically either an
// interesting ordering or an ordering homogenized from one. It also includes
// orderings that are used for sort-for-grouping, i.e. for GROUP BY,
// PARTITION BY or DISTINCT.
struct SortAheadOrdering {
  // Pointer to an ordering in LogicalOrderings.
  int ordering_idx;

  // Which tables must be present in the join before one can apply
  // this sort (usually because the elements we sort by are contained
  // in these tables).
  //
  // The presence of RAND_TABLE_BIT means that the ordering contains
  // at least one nondeterminstic item; we never allow pushing such
  // orderings into the join (implicitly: sortahead during joins check
  // required_nodes, and never include RAND_TABLE_BIT). This makes sure that we
  // cannot push e.g. ORDER BY rand() into the left side of a join, which would
  // make rows shuffled on that table only, which isn't what the user would
  // expect. We also have special logic to disallow satisfying nondeterministic
  // groupings/orderings others (both in the logic for group covers, and in NFSM
  // construction), so that
  //
  //   GROUP BY a ORDER BY a, func()
  //
  // cannot be done by evaluating func() too early, but we do allow exact
  // matches, so that e.g. GROUP BY func() ORDER BY func() can be done as only
  // one sort (which isn't too unreasonable). This may be a bit conservative
  // or it may be a bit aggressive, depending on who you ask.
  hypergraph::NodeMap required_nodes;

  // Whether aggregates must be computed before one can apply this sort
  // (because it includes at least one aggregate).
  bool aggregates_required;

  // The ordering expressed in a form that filesort can use.
  ORDER *order;
};

// An index that we can use in the query, either for index lookup (ref access)
// or for scanning along to get an interesting ordering.
struct ActiveIndexInfo {
  TABLE *table;
  int key_idx;
  LogicalOrderings::StateIndex forward_order = 0, reverse_order = 0,
                               reverse_order_without_extended_key_parts = 0;
};

// A full-text index that we can use in the query, either for index lookup or
// for scanning along to get an interesting order.
struct FullTextIndexInfo {
  Item_func_match *match;
  LogicalOrderings::StateIndex order = 0;
};

/**
  Build all structures we need for keeping track of interesting orders.
  We collect the actual relevant orderings (e.g. from ORDER BY) and any
  functional dependencies we can find, then ask LogicalOrderings to create
  its state machine (as defined in interesting_orders.h). The result is
  said state machine, a list of potential sort-ahead orderings,
  and a list of what indexes we can use to scan each table (including
  what orderings they yield, if they are interesting).
 */
void BuildInterestingOrders(
    THD *thd, JoinHypergraph *graph, Query_block *query_block,
    LogicalOrderings *orderings,
    Mem_root_array<SortAheadOrdering> *sort_ahead_orderings,
    int *order_by_ordering_idx, int *group_by_ordering_idx,
    int *distinct_ordering_idx, Mem_root_array<ActiveIndexInfo> *active_indexes,
    Mem_root_array<FullTextIndexInfo> *fulltext_searches, std::string *trace);

// Build an ORDER * that we can give to Filesort. It is only suitable for
// sort-ahead, since it assumes no temporary tables have been inserted.
// It can however be used after temporary tables if
// ReplaceOrderItemsWithTempTableFields() is called on it, and
// FinalizePlanForQueryBlock() takes care of this for us.
ORDER *BuildSortAheadOrdering(THD *thd, const LogicalOrderings *orderings,
                              Ordering ordering);

/**
  Creates a reduced ordering for the ordering or grouping specified by
  "ordering_idx". It is assumed that the ordering happens after all joins and
  filters, so that all functional dependencies are active. All parts of the
  ordering that are made redundant by functional dependencies, are removed.

  The returned ordering may be empty if all elements are redundant. This happens
  if all elements are constants, or have predicates that ensure they are
  constant.
 */
Ordering ReduceFinalOrdering(THD *thd, const LogicalOrderings &orderings,
                             int ordering_idx);

#endif  // SQL_JOIN_OPTIMIZER_BUILD_INTERESTING_ORDERS_H_
