/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_PARSE_TREE_WINDOW_INCLUDED
#define SQL_PARSE_TREE_WINDOW_INCLUDED

#include "sql/parse_tree_node_base.h"
#include "sql/sql_list.h"
#include "sql/window.h"

class Item_string;
class PT_frame;
class PT_order_list;

/**
  Parse tree node for a window; just a shallow wrapper for
  class Window, q.v.
*/
class PT_window : public Parse_tree_node, public Window {
  typedef Parse_tree_node super;

 public:
  PT_window(PT_order_list *partition_by, PT_order_list *order_by,
            PT_frame *frame)
      : Window(partition_by, order_by, frame) {}

  PT_window(PT_order_list *partition_by, PT_order_list *order_by,
            PT_frame *frame, Item_string *inherit)
      : Window(partition_by, order_by, frame, inherit) {}

  PT_window(Item_string *name) : Window(name) {}

  bool contextualize(Parse_context *pc) override;
};

/**
  Parse tree node for a list of window definitions corresponding
  to a \<window clause\> in SQL 2003.
*/
class PT_window_list : public Parse_tree_node {
  typedef Parse_tree_node super;
  List<Window> m_windows;

 public:
  PT_window_list() = default;

  bool contextualize(Parse_context *pc) override;

  bool push_back(PT_window *w) { return m_windows.push_back(w); }
};

#endif  // SQL_PARSE_TREE_WINDOW_INCLUDED
