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

#ifndef oq_graphcore_h_
#define oq_graphcore_h_

/* #define GRAPHCORE_INTERNAL __attribute__((visibility("hidden"))) */
#define GRAPHCORE_INTERNAL

#include "graphcore-types.h"

namespace open_query
{
  class oqgraph_share;
  class oqgraph_cursor;

  struct row
  {
    bool latch_indicator;
    bool orig_indicator;
    bool dest_indicator;
    bool weight_indicator;
    bool seq_indicator;
    bool link_indicator;

    int latch;
    VertexID orig;
    VertexID dest;
    EdgeWeight weight;
    unsigned seq;
    VertexID link;
  };

  class oqgraph
  {
    oqgraph_share *const share;
    oqgraph_cursor *cursor;
    row row_info;

    inline oqgraph(oqgraph_share*) throw();
    inline ~oqgraph() throw();
  public:

    enum error_code
    {
      OK= 0,
      NO_MORE_DATA,
      EDGE_NOT_FOUND,
      INVALID_WEIGHT,
      DUPLICATE_EDGE,
      CANNOT_ADD_VERTEX,
      CANNOT_ADD_EDGE,
      MISC_FAIL
    };

    struct current_row_st {};
    static inline current_row_st current_row()
    { return current_row_st(); }

    unsigned vertices_count() const throw();
    unsigned edges_count() const throw();

    int delete_all(void) throw();

    int insert_edge(VertexID, VertexID, EdgeWeight, bool=0) throw();
    int modify_edge(VertexID, VertexID, EdgeWeight) throw();
    int delete_edge(VertexID, VertexID) throw();

    int modify_edge(current_row_st,
                    VertexID*, VertexID*, EdgeWeight*, bool=0) throw();
    int delete_edge(current_row_st) throw();

    int replace_edge(VertexID orig, VertexID dest, EdgeWeight weight) throw()
    { return insert_edge(orig, dest, weight, true); }

    int search(int*, VertexID*, VertexID*) throw();
    int random(bool) throw();

    int fetch_row(row&) throw();
    int fetch_row(row&, const void*) throw();
    void row_ref(void*) throw();

    static oqgraph* create(oqgraph_share*) throw();
    static oqgraph_share *create() throw();

    static void free(oqgraph*) throw();
    static void free(oqgraph_share*) throw();

    static const size_t sizeof_ref;
  };

}
#endif
