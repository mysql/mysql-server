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

#ifndef SQL_JOIN_OPTIMIZER_JOIN_OPTIMIZER_H
#define SQL_JOIN_OPTIMIZER_JOIN_OPTIMIZER_H

/**
  @file

  The hypergraph join optimizer takes a query block and decides how to
  execute it as fast as possible (within a given cost model), based on
  the idea of expressing the join relations as edges in a hypergraph.
  (See subgraph_enumeration.h for more details on the core algorithm,
  or FindBestQueryPlan() for more information on overall execution.)

  It is intended to eventually take over completely from the older join
  optimizer based on prefix search (sql_planner.cc and related code),
  and is nearly feature complete, but is currently in the early stages
  with a very simplistic cost model and certain limitations.
  The most notable ones are that we do not support:

    - Hints (except STRAIGHT_JOIN).
    - TRADITIONAL and JSON formats for EXPLAIN (use FORMAT=tree).
    - UPDATE.

  There are also have many optimization features it does not yet support;
  among them:

    - Aggregation through a temporary table.
    - Some range optimizer features (notably MIN/MAX optimization).
    - Materialization of arbitrary access paths (note that nested loop
      joins against these can enable a limited form of hash join
      that preserves ordering on the left side).
 */

#include <string>

class Query_block;
class THD;
struct AccessPath;
struct JoinHypergraph;

/**
  The main entry point for the hypergraph join optimizer; takes in a query
  block and returns an access path to execute it (or nullptr, for error).
  It works as follows:

    1. Convert the query block from MySQL's Table_ref structures into
       a hypergraph (see make_join_hypergraph.h).
    2. Find all legal subplans in the hypergraph, calculate costs for
       them and create access paths -- if there are multiple ways to make a
       given subplan (e.g. multiple join types, or joining {t1,t2,t3} can be
       made through either {t1}-{t2,t3} or {t1,t2}-{t3}), keep only the cheapest
       one. Filter predicates (from WHERE and pushed-down join conditions)
       are added as soon down as it is legal, which is usually (but not
       universally) optimal. The algorithm works so that we always see smaller
       subplans first and then end at the complete join plan containing all the
       tables in the query block.
    3. Add an access path for non-pushable filter predicates.
    4. Add extra access paths for operations done after the joining,
       such as ORDER BY, GROUP BY, LIMIT, etc..
    5. Make access paths for the filters in nodes made by #2
       (see ExpandFilterAccessPaths()).

  Materializing subqueries need some extra care. (These are typically IN
  subqueries that for whatever reason could not be rewritten to semijoin,
  e.g. because they have GROUP BY.) The decision on whether to materialize
  or not needs to be done cost-based, and depends both on the inner and outer
  query block, so it needs to be done cost-based. (Materializiation gives
  a high up-front cost, but each execution is cheaper, so it will depend on
  how many times we expect to execute the subquery and now expensive it is
  to run unmaterialized.) Following the flow through the different steps:

  First of all, these go through a stage known as in2exists, rewriting them
  from e.g.

    WHERE t1_outer.x IN ( SELECT t2.y FROM t2 GROUP BY ... )

  to

    WHERE EXISTS ( SELECT 1 FROM t2 GROUP BY ... HAVING t2.y = t1_outer.x )

  This happens before the join optimizer, and the idea is that the HAVING
  condition (known as a “created_by_in2exists condition”, possibly in WHERE
  instead of HAVING) can be attempted pushed down into an index or similar,
  giving more efficient execution. However, if we want to materialize the
  subquery, these extra conditions need to be removed before materialization;
  not only do they give the wrong result, but they can also need to wrong
  costs and a suboptimal join order.

  Thus, whenever we plan such a subquery, we plan it twice; once as usual,
  and then a second time with all in2exists conditions removed. This gives
  EstimateFilterCost() precise cost information for both cases, or at least
  as precise as the cost model itself is. In the outer query block, we can
  then weigh the two alternatives against each other when we add a filter
  with such a subquery; we can choose to materialize it or not, and propose
  both alternatives as with any other subplan. When we've decided on the
  final plan, we go through all access paths and actually materialize the
  subqueries it says to materialize.

  There are lots of places these conditions can show up; to reduce complexity,
  we only consider materialization in the most common places (filters on
  base tables, filters after joins, filters from HAVING) -- in particular,
  we don't bother checking on join conditions. It is never wrong to not
  materialize a subquery, though it may be suboptimal.


  Note that the access path returned by FindBestQueryPlan() is not ready
  for immediate conversion to iterators; see FinalizePlanForQueryBlock().
  You may call FindBestQueryPlan() any number of times for a query block,
  but FinalizePlanForQueryBlock() only once, as finalization generates
  temporary tables and may rewrite expressions in ways that are incompatible
  with future planning. The difference is most striking with the planning
  done twice by in2exists (see above).

  @param thd Thread handle.
  @param query_block The query block to find a plan for.
  @param trace If not nullptr, will be filled with human-readable optimizer
    trace showing some of the inner workings of the code.
 */
AccessPath *FindBestQueryPlan(THD *thd, Query_block *query_block,
                              std::string *trace);

// See comment in .cc file.
bool FinalizePlanForQueryBlock(THD *thd, Query_block *query_block);

// Exposed for unit testing only.
void FindSargablePredicates(THD *thd, std::string *trace,
                            JoinHypergraph *graph);

void EstimateAggregateCost(AccessPath *path);
void EstimateMaterializeCost(THD *thd, AccessPath *path);

#endif  // SQL_JOIN_OPTIMIZER_JOIN_OPTIMIZER_H
