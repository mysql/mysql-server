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

#ifndef SQL_JOIN_OPTIMIZER_NODE_MAP_H
#define SQL_JOIN_OPTIMIZER_NODE_MAP_H 1

#include <stdint.h>

namespace hypergraph {

/**
  Since our graphs can never have more than 61 tables, node sets and edge lists
  are implemented using 64-bit bit sets. This allows for a compact
  representation and very fast set manipulation; the algorithm does a fair
  amount of intersections and unions. If we should need extensions to larger
  graphs later (this will require additional heuristics for reducing the search
  space), we can use dynamic bit sets, although at a performance cost (we'd
  probably templatize off the NodeMap type).
 */
using NodeMap = uint64_t;

}  // namespace hypergraph

#endif  // SQL_JOIN_OPTIMIZER_NODE_MAP_H
