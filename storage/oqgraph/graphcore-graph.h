/* Copyright (C) 2007-2009 Arjen G Lentz & Antony T Curtis for Open Query

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* ======================================================================
   Open Query Graph Computation Engine, based on a concept by Arjen Lentz
   Mk.II implementation by Antony Curtis & Arjen Lentz
   For more information, documentation, support, enhancement engineering,
   and non-GPL licensing, see http://openquery.com/graph
   or contact graph@openquery.com
   For packaged binaries, see http://ourdelta.org
   ======================================================================
*/

#ifndef oq_graphcore_graph_h_
#define oq_graphcore_graph_h_

typedef adjacency_list
<
  vecS,
  vecS,
  bidirectionalS,
  VertexInfo,
  EdgeInfo
> Graph;

#define GRAPH_WEIGHTMAP(G) get(&EdgeInfo::weight, G)
typedef property_map<Graph, EdgeWeight EdgeInfo::*>::type weightmap_type;

#define GRAPH_INDEXMAP(G)  get(vertex_index, G)
typedef property_map<Graph, vertex_index_t>::type indexmap_type;

#define GRAPH_IDMAP(G)     get(&VertexInfo::id, G)
typedef property_map<Graph, VertexID VertexInfo::*>::type idmap_type;

#endif
