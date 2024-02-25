#ifndef Query_block_VISITOR_INCLUDED
#define Query_block_VISITOR_INCLUDED
/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

/**
  @file select_lex_visitor.h
  Visitor interface for parse trees.
*/

class Query_expression;
class Query_block;
class Item;

/**
  Abstract base class for traversing the Query_block tree. In order to use it,
  a client defines a subclass, overriding the member functions that visit the
  objects of interest. If a function returns true, traversal is aborted.
*/
class Select_lex_visitor {
 public:
  virtual bool visits_in_prefix_order() const { return true; }

  bool visit(Query_expression *unit) { return visit_union(unit); }
  bool visit(Query_block *query_block) {
    return visit_query_block(query_block);
  }

  /// Called for all nodes of all expression trees (i.e. Item trees).
  bool visit(Item *item) { return visit_item(item); }

  virtual ~Select_lex_visitor() = 0;

 protected:
  virtual bool visit_union(Query_expression *) { return false; }
  virtual bool visit_query_block(Query_block *) { return false; }
  virtual bool visit_item(Item *) { return false; }
};

#endif  // Query_block_VISITOR_INCLUDED
