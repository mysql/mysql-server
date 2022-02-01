/* Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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
  but is currently in early alpha stage with a very simplistic cost model
  and a large number of limitations: The most notable ones are that
  we do not support:

    - Many SQL features: DISTINCT, recursive CTE, windowing functions,
      LATERAL, JSON_TABLE.
    - Secondary engine.
    - Hints.
    - TRADITIONAL and JSON formats for EXPLAIN (use FORMAT=tree).

  For unsupported queries, we will return an error; every valid SQL
  query should either give such an error a correct result set.

  There are also have many optimization features it does not yet support;
  among them:

    - Reordering of non-inner joins; outer joins work as an optimization
      barrier, pretty much like the existing join optimizer.
    - Indexes of any kind (and thus, no understanding of interesting
      orders); table scans only.
    - Multiple equalities; they are simplified to simple equalities
      before optimization (so some legal join orderings will be missed).
    - Aggregation through a temporary table.
    - Queries with a very large amount of possible orderings, e.g. 30-way
      star joins. (Less extreme queries, such as 30-way chain joins,
      will be fine.) They will receive a similar error message as with
      unsupported SQL features, instead of timing out.
 */

#include <string>

struct AccessPath;
class THD;
class Query_block;

/**
  The main entry point for the hypergraph join optimizer; takes in a query
  block and returns an access path to execute it (or nullptr, for error).
  It works as follows:

    1. Convert the query block from MySQL's TABLE_LIST structures into
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

  @param thd Thread handle.
  @param query_block The query block to find a plan for.
  @param trace If not nullptr, will be filled with human-readable optimizer
    trace showing some of the inner workings of the code.
 */
AccessPath *FindBestQueryPlan(THD *thd, Query_block *query_block,
                              std::string *trace);

void EstimateMaterializeCost(AccessPath *path);

#endif  // SQL_JOIN_OPTIMIZER_JOIN_OPTIMIZER_H
