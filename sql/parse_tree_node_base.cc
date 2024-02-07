/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

#include "sql/parse_tree_node_base.h"
#include "sql/query_term.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"

#ifdef HAVE_ABI_CXA_DEMANGLE
#include <cxxabi.h>
#endif

Parse_context::Parse_context(THD *thd_arg, Query_block *sl_arg,
                             bool show_parse_tree,
                             Show_parse_tree *parent_show_parent_tree)
    : Parse_context_base(show_parse_tree, parent_show_parent_tree),
      thd(thd_arg),
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

  // Ensure that further expressions are resolved against first query block
  select = select->master_query_expression()->first_query_block();

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

/**
  Given a mangled class name, return an unmangled one.
 */
static std::string unmangle_typename(const char *name) {
#ifdef HAVE_ABI_CXA_DEMANGLE
  int status = 0;
  char *const readable_name =
      abi::__cxa_demangle(name, nullptr, nullptr, &status);
  if (status == 0) {
    std::string ret_string = readable_name;
    free(readable_name);
    return ret_string;
  }
#endif
  // Failback mechanism.
  // Exclude the leading mangled characters from the class name. The assumption
  // is that the class names start with PT_, PTI_ or Item_, since they are
  // derived from either Parse_tree_node, Item or Parse_tree_root.
  std::string class_name(name);
  size_t strpos = class_name.find("PT_");
  if (strpos == std::string::npos) strpos = class_name.find("PTI_");
  if (strpos == std::string::npos) strpos = class_name.find("Item_");
  if (strpos == std::string::npos) strpos = 0;
  return class_name.substr(strpos);
}

bool Show_parse_tree::push_level(const POS &pos, const char *typname) {
  Json_object *ret_obj = new (std::nothrow) Json_object;
  if (ret_obj == nullptr) return true;  // OOM

  // If position is not set, can't extract the text of the SQL clause.
  if (!pos.is_empty()) {
    ret_obj->add_alias(
        "text", create_dom_ptr<Json_string>(pos.cpp.start, pos.cpp.length()));

    // If this is the very first object, treat it's position as a reference
    // position. All the subsequent objects' positions will be relative to this
    // position.
    if (m_json_obj_stack.empty()) m_reference_pos = pos.cpp.start;

    // Position is required to sort children.
    ret_obj->add_alias(
        "startpos", create_dom_ptr<Json_int>(pos.cpp.start - m_reference_pos));
  }

  // Assign the class name as the node type.
  std::string unmangled_name = unmangle_typename(typname);
  if (unmangled_name.empty()) return true;  // Unexpected.
  ret_obj->add_alias("type", create_dom_ptr<Json_string>(unmangled_name));

  m_json_obj_stack.push_back(ret_obj);
  return false;
}

Json_object *Show_parse_tree::pop_json_object() {
  Json_object *obj = m_json_obj_stack.back();

  m_json_obj_stack.pop_back();

  // Object is being popped out; it implies that we are done adding children for
  // this object. So it's time to sort the children by their syntax position.
  Json_array *children = dynamic_cast<Json_array *>(obj->get("components"));
  if (children != nullptr) {
    children->sort(m_comparator);
    // Don't require the position field anymore, now that sorting is done.
    for (size_t i = 0; i < children->size(); ++i) {
      (down_cast<Json_object *>((*children)[i]))->remove("startpos");
    }
  }
  return obj;
}

std::string Show_parse_tree::get_parse_tree() {
  if (m_root_obj == nullptr) return "";

  // This will be the root node. Serialize the JSON tree to a string.
  Json_wrapper wrapper(m_root_obj.get(), /*alias=*/true);
  StringBuffer<STRING_BUFFER_USUAL_SIZE> jsonstring;
  if (wrapper.to_pretty_string(&jsonstring, "Show_parse_tree::get_parse_tree()",
                               JsonDepthErrorHandler)) {
    return "";
  }
  return {jsonstring.ptr(), jsonstring.length()};
}

/**
  If there is a current parent, assign this object as child of that parent.
  If there is no parent, make this object the root of this parse tree,
  unless there is a parent parse tree in which case make this object a child
  of the parent explain tree's leaf parent.
*/
bool Show_parse_tree::make_child(Json_object *obj) {
  Json_object *parent = get_current_parent();

  if (parent == nullptr && m_parent_show_parse_tree != nullptr) {
    parent = m_parent_show_parse_tree->get_current_parent();
    assert(parent != nullptr);
  }
  if (parent == nullptr) {
    // This will be the root node.
    m_root_obj = std::unique_ptr<Json_object>(obj);
    // It's the parent that removes it's children's position field. Since this
    // is the root node, we need to explicitly remove it's position because
    // there is no parent.
    obj->remove("startpos");
  } else {
    Json_array *children =
        dynamic_cast<Json_array *>(parent->get("components"));
    // If parent has no children yet, create a new array for it's children.
    if (children == nullptr) {
      children = new (std::nothrow) Json_array();
      if (children == nullptr) return true;
      parent->add_alias("components", std::unique_ptr<Json_array>(children));
    }
    // Add this obj as one of the children of the parent.
    children->append_alias(std::unique_ptr<Json_object>(obj));
  }

  return false;
}
