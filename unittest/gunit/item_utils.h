#ifndef ITEM_UTILS_H
#define ITEM_UTILS_H

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

#include "sql/parse_tree_nodes.h"
#include "sql/sql_class.h"                   // THD
#include "unittest/gunit/mock_parse_tree.h"  // Mock_pt_item_list

using my_testing::Mock_pt_item_list;

/// @file Utilities for creating Item objects.

/// Creates and resolves an Item.
template <typename Func_item, typename... Arg_type>
static Item *make_resolved(THD *thd, Arg_type... args) {
  auto arglist = new (thd->mem_root) Mock_pt_item_list(args...);
  Parse_context pc(thd, thd->lex->query_block);
  POS pos;  // We expect this object to be copied.
  Item *item = new (thd->mem_root) Func_item(pos, arglist);
  item->itemize(&pc, &item);
  item->fix_fields(thd, nullptr);
  return item;
}

#endif  // ITEM_UTILS_H
