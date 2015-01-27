#ifndef SELECT_LEX_VISITOR_INCLUDED
#define SELECT_LEX_VISITOR_INCLUDED
/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file select_lex_visitor.h
  Visitor interface for parse trees.
*/

typedef class st_select_lex_unit SELECT_LEX_UNIT;
typedef class st_select_lex SELECT_LEX;
class Item;

/**
  Abstract base class for traversing the SELECT_LEX tree. In order to use it,
  a client defines a subclass, overriding the member functions that visit the
  objects of interest. If a function returns true, traversal is aborted.
*/
class Select_lex_visitor
{
public:
  virtual bool visits_in_prefix_order() const { return true; }

  bool visit(SELECT_LEX_UNIT *unit) { return visit_union(unit); }
  bool visit(SELECT_LEX *select_lex) { return visit_query_block(select_lex); }

  /// Called for all nodes of all expression trees (i.e. Item trees).
  bool visit(Item *item) { return visit_item(item); }

  virtual ~Select_lex_visitor() = 0;

protected:
  virtual bool visit_union(SELECT_LEX_UNIT *) { return false; }
  virtual bool visit_query_block(SELECT_LEX *) { return false; }
  virtual bool visit_item(Item *) { return false; }
};

#endif // SELECT_LEX_VISITOR_INCLUDED
