/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/parse_tree_window.h"

#include <sys/types.h>  // uint. TODO: replace with cstdint

#include <initializer_list>

#include "sql/item.h"  // Item
#include "sql/parse_tree_nodes.h"
#include "sql/sql_lex.h"     // Query_block
#include "sql/window_lex.h"  // WBT_VALUE_FOLLOWING

bool PT_window::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  if (m_partition_by != nullptr) {
    if (m_partition_by->contextualize(pc)) return true;
  }

  if (m_order_by != nullptr) {
    if (m_order_by->contextualize(pc)) return true;
  }

  if (m_frame != nullptr && !m_frame->m_originally_absent) {
    if (m_frame->contextualize(pc)) return true;
  }

  return false;
}

void PT_window::add_json_info(Json_object *obj) {
  if (m_name != nullptr)
    obj->add_alias("window_name",
                   create_dom_ptr<Json_string>(m_name->item_name.ptr(),
                                               m_name->item_name.length()));
}

bool PT_frame::do_contextualize(Parse_context *pc) {
  for (auto *bound : {m_from, m_to})
    if (bound->contextualize(pc)) return true;
  if (m_exclusion != nullptr)
    if (m_exclusion->contextualize(pc)) return true;
  return false;
}

bool PT_border::do_contextualize(Parse_context *pc) {
  if (m_border_type == WBT_VALUE_PRECEDING ||
      m_border_type == WBT_VALUE_FOLLOWING) {
    auto **bound_i_ptr = border_ptr();
    if ((*border_ptr())->itemize(pc, bound_i_ptr)) return true;
  }
  return false;
}

bool PT_window_list::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  uint count = pc->select->m_windows.elements;
  List_iterator<Window> wi(m_windows);
  Window *w;
  while ((w = wi++)) {
    if (static_cast<PT_window *>(w)->contextualize(pc)) return true;
    w->set_def_pos(++count);
  }

  Query_block *select = pc->select;
  select->m_windows.prepend(&m_windows);

  return false;
}
