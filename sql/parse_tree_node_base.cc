/* Copyright (c) 2013, 2023, Oracle and/or its affiliates.

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

#include "sql/parse_tree_node_base.h"

#include "sql/query_term.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
Parse_context::Parse_context(THD *thd_arg, Query_block *sl_arg)
    : thd(thd_arg),
      mem_root(thd->mem_root),
      select(sl_arg),
      m_stack(thd->mem_root) {
  m_stack.push_back(QueryLevel(thd->mem_root, SC_TOP));
}

/**
  Set the parsed query expression's query term. For its construction, see
  parse_tree_nodes.cc's contextualize methods. Query_term is documented in
  query_term.h .
*/
bool Parse_context::finalize_query_expression() {
  QueryLevel ql = m_stack.back();
  m_stack.pop_back();
  assert(ql.m_elts.size() == 1);
  Query_term *top = ql.m_elts.back();
  top = top->pushdown_limit_order_by();
  select->master_query_expression()->set_query_term(top);
  if (top->validate_structure(nullptr)) return true;
  return false;
}

bool Parse_context::is_top_level_union_all(Surrounding_context op) {
  if (op == SC_EXCEPT_ALL || op == SC_INTERSECT_ALL) return false;
  assert(op == SC_UNION_ALL);
  for (size_t i = m_stack.size(); i > 0; i--) {
    switch (m_stack[i - 1].m_type) {
      case SC_UNION_DISTINCT:
      case SC_INTERSECT_DISTINCT:
      case SC_INTERSECT_ALL:
      case SC_EXCEPT_DISTINCT:
      case SC_EXCEPT_ALL:
      case SC_SUBQUERY:
        return false;
      case SC_QUERY_EXPRESSION:
        // Ordering above this level in the context stack (syntactically
        // outside) precludes streaming of UNION ALL.
        if (m_stack[i - 1].m_has_order) return false;
        [[fallthrough]];
      default:;
    }
  }
  return true;
}
