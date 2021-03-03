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

#ifndef SQL_JOIN_OPTIMIZER_MAKE_JOIN_HYPERGRAPH
#define SQL_JOIN_OPTIMIZER_MAKE_JOIN_HYPERGRAPH 1

#include <array>
#include <string>

#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/mem_root_array.h"
#include "sql/sql_const.h"

class SELECT_LEX;
class THD;
struct MEM_ROOT;
struct TABLE;

/**
  A struct containing a join hypergraph of a single query block, encapsulating
  the constraints given by the relational expressions (e.g. inner joins are
  more freely reorderable than outer joins).

  Since the Hypergraph class does not carry any payloads for nodes and edges,
  and we need to associate e.g.  TABLE pointers with each node, we store our
  extra data in “nodes” and “edges”, indexed the same way the hypergraph is
  indexed.
 */
struct JoinHypergraph {
  explicit JoinHypergraph(MEM_ROOT *mem_root)
      : nodes(mem_root), edges(mem_root), predicates(mem_root) {}

  hypergraph::Hypergraph graph;

  // Maps table->tableno() to an index in “nodes”, also suitable for
  // a bit index in a NodeMap. This is normally the identity mapping,
  // except for when scalar-to-derived conversion is active.
  std::array<int, MAX_TABLES> table_num_to_node_num;

  Mem_root_array<TABLE *> nodes;

  // Note that graph.edges contain each edge twice (see Hypergraph
  // for more information), so edges[i] corresponds to graph.edges[i*2].
  Mem_root_array<JoinPredicate> edges;

  Mem_root_array<Predicate> predicates;
};

/**
  Make a join hypergraph from the query block given by “select_lex”,
  converting from MySQL's join list structures to the ones expected
  by the hypergraph join optimizer. This includes pushdown of WHERE
  predicates, and detection of conditions suitable for hash join.
  However, it does not include simplification of outer to inner joins;
  that is presumed to have happened earlier.

  The result is suitable for running DPhyp (subgraph_enumeration.h)
  to find optimal join planning.
 */
bool MakeJoinHypergraph(THD *thd, SELECT_LEX *select_lex, std::string *trace,
                        JoinHypergraph *graph);

#endif  // SQL_JOIN_OPTIMIZER_MAKE_JOIN_HYPERGRAPH
