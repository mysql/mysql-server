/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "i_s.h"
#include "content_tree.h"
#include "my_rapidjson_size_t.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "sql/mem_root_array.h"

#include "sql/dd/impl/system_views/json_duality_view_columns.h"
#include "sql/dd/impl/system_views/json_duality_view_links.h"
#include "sql/dd/impl/system_views/json_duality_view_tables.h"
#include "sql/dd/impl/system_views/json_duality_views.h"

#include <stack>

using namespace rapidjson;

namespace {

constexpr char default_catalog[] = "def";

/**
  Iterative depth first traversal of the tree using a stack. We assign a
  temporary id to each table based on the order of traversal. This ought
  to be repeatable as long as the view does not change. The tree nodes cannot
  be const since we assign tmp ids.

  @param root
  @param visit_single_node
*/
void visit_tree(jdv::Content_tree_node *root, auto &&visit_single_node) {
  std::stack<jdv::Content_tree_node *> nodes_left;
  nodes_left.push(root);
  while (!nodes_left.empty()) {
    jdv::Content_tree_node *cur_node = nodes_left.top();
    nodes_left.pop();
    if (visit_single_node(cur_node)) return;
    for (auto *child : *cur_node->children()) {
      nodes_left.push(child);
    }
  }
}

void get_json_duality_views(Document *doc, jdv::Content_tree_node *root) {
  // Which operations are allowed on the view.
  struct Allow {
    bool ins = false;
    bool upd = false;
    bool del = false;
  } allow;

  visit_tree(root, [&allow](const jdv::Content_tree_node *node) -> bool {
    // Allow actions based on table tags.
    allow.ins |= (node->table_tags() & jdv::DVT_INSERT) != 0;
    allow.upd |= (node->table_tags() & jdv::DVT_UPDATE) != 0;
    allow.del |= (node->table_tags() & jdv::DVT_DELETE) != 0;

    /*
      The view is considered updatable if there is some
      column that is updatable.
    */
    if (!allow.upd) {
      // Otherwise check if there is some column that is updatable
      for (auto &kc : node->key_column_map()) {
        if (node->key_column_info_list().at(kc.second).allows_update()) {
          allow.upd = true;
          break;
        }
      }
    }

    /*
      If some child supports ins/upd/del, then the view is considered
      supporting ins/upd/del. So if both ins, upd and del are true,
      then there is no point continuing.
    */
    return (allow.ins && allow.upd && allow.del);
  });

  Value root_table_catalog(default_catalog, strlen(default_catalog),
                           doc->GetAllocator());
  Value root_table_schema(root->table_ref()->get_db_name(),
                          strlen(root->table_ref()->get_db_name()),
                          doc->GetAllocator());
  Value root_table_name(root->table_ref()->get_table_name(),
                        strlen(root->table_ref()->get_table_name()),
                        doc->GetAllocator());

  Value allow_insert(allow.ins);
  Value allow_update(allow.upd);
  Value allow_delete(allow.del);
  Value read_only(!(allow_insert.GetBool() || allow_update.GetBool() ||
                    allow_delete.GetBool()));

  Value view;
  view.SetObject();
  view.AddMember("root_table_catalog", root_table_catalog, doc->GetAllocator());
  view.AddMember("root_table_schema", root_table_schema, doc->GetAllocator());
  view.AddMember("root_table_name", root_table_name, doc->GetAllocator());
  view.AddMember("allow_insert", allow_insert, doc->GetAllocator());
  view.AddMember("allow_update", allow_update, doc->GetAllocator());
  view.AddMember("allow_delete", allow_delete, doc->GetAllocator());
  view.AddMember("read_only", read_only, doc->GetAllocator());

  (*doc)["entries"].GetArray().PushBack(view, doc->GetAllocator());
}

void get_json_duality_view_tables(Document *doc, jdv::Content_tree_node *root) {
  visit_tree(root, [&doc](const jdv::Content_tree_node *node) -> bool {
    // Save the tmp id that was assigned.
    Value table_id(node->id());
    Value parent_table_id(node->is_root_object() ? 0 : node->parent()->id());

    /*
      Relationship is either "singleton" or "nested". We do not use
      the "referenced" relationship for now.
    */
    const char *relation = node->is_root_object() ? "NA" : node->type_string();
    Value parent_table_relationship(relation, strlen(relation),
                                    doc->GetAllocator());

    // The where clause is defined in the join condition.
    std::ostringstream oss;
    if (node->has_join_condition()) {
      oss << node->qualified_table_name() << '.'
          << node->join_column_info().column_name() << " = "
          << node->parent()->qualified_table_name() << '.'
          << node->parent_join_column_info().column_name();
    }
    Value where(oss.str().c_str(), strlen(oss.str().c_str()),
                doc->GetAllocator());

    Value catalog(default_catalog, strlen(default_catalog),
                  doc->GetAllocator());
    Value schema(node->table_ref()->get_db_name(),
                 strlen(node->table_ref()->get_db_name()), doc->GetAllocator());
    Value name(node->table_ref()->get_table_name(),
               strlen(node->table_ref()->get_table_name()),
               doc->GetAllocator());
    Value is_root_table(node->is_root_object());
    Value allow_insert(node->allows_insert());
    Value allow_update(node->allows_update());
    Value allow_delete(node->allows_delete());
    Value read_only(node->read_only());

    Value table;
    table.SetObject();
    table.AddMember("referenced_table_id", table_id, doc->GetAllocator());
    // Entries to be NULL for root tables
    if (node->parent() != nullptr) {
      table.AddMember("referenced_table_parent_id", parent_table_id,
                      doc->GetAllocator());
      table.AddMember("referenced_table_parent_relationship",
                      parent_table_relationship, doc->GetAllocator());
    }
    table.AddMember("referenced_table_catalog", catalog, doc->GetAllocator());
    table.AddMember("referenced_table_schema", schema, doc->GetAllocator());
    table.AddMember("referenced_table_name", name, doc->GetAllocator());
    table.AddMember("where_clause", where, doc->GetAllocator());
    table.AddMember("is_root_table", is_root_table, doc->GetAllocator());
    table.AddMember("allow_insert", allow_insert, doc->GetAllocator());
    table.AddMember("allow_update", allow_update, doc->GetAllocator());
    table.AddMember("allow_delete", allow_delete, doc->GetAllocator());
    table.AddMember("read_only", read_only, doc->GetAllocator());

    (*doc)["entries"].GetArray().PushBack(table, doc->GetAllocator());
    return false;
  });
}

void get_json_duality_view_columns(Document *doc,
                                   jdv::Content_tree_node *root) {
  visit_tree(root, [&doc](jdv::Content_tree_node *node) -> bool {
    Value table_id(node->id());
    Value catalog(default_catalog, strlen(default_catalog),
                  doc->GetAllocator());
    Value schema(node->table_ref()->get_db_name(),
                 strlen(node->table_ref()->get_db_name()), doc->GetAllocator());
    Value name(node->table_ref()->get_table_name(),
               strlen(node->table_ref()->get_table_name()),
               doc->GetAllocator());
    Value is_root_table(node->is_root_object());

    Value column;
    column.SetObject();
    column.AddMember("referenced_table_id", table_id, doc->GetAllocator());
    column.AddMember("referenced_table_catalog", catalog, doc->GetAllocator());
    column.AddMember("referenced_table_schema", schema, doc->GetAllocator());
    column.AddMember("referenced_table_name", name, doc->GetAllocator());
    column.AddMember("is_root_table", is_root_table, doc->GetAllocator());

    for (const auto &kc : *node->key_column_map()) {
      Value real_column(column, doc->GetAllocator());
      Value json_key_name(kc.first.data(), kc.first.length(),
                          doc->GetAllocator());

      const auto &col = node->key_column_info_list()->at(kc.second);
      auto col_name = col.column_name();
      Value column_name(col_name.data(), col_name.length(),
                        doc->GetAllocator());

      /*
        The table level update tag is "inherited". The tags for the
        column are set to the effective values when the node map is
        prepared, so we can just pick them directly.
      */
      Value allow_insert(col.allows_insert());
      Value allow_update(col.allows_update());
      Value allow_delete(col.allows_delete());
      Value read_only(col.read_only());

      real_column.AddMember("referenced_column_name", column_name,
                            doc->GetAllocator());
      real_column.AddMember("json_key_name", json_key_name,
                            doc->GetAllocator());
      real_column.AddMember("allow_insert", allow_insert, doc->GetAllocator());
      real_column.AddMember("allow_update", allow_update, doc->GetAllocator());
      real_column.AddMember("allow_delete", allow_delete, doc->GetAllocator());
      real_column.AddMember("read_only", read_only, doc->GetAllocator());

      (*doc)["entries"].GetArray().PushBack(real_column, doc->GetAllocator());
    }
    return false;
  });
}

void get_json_duality_view_links(Document *doc, jdv::Content_tree_node *root) {
  visit_tree(root, [&doc](const jdv::Content_tree_node *node) -> bool {
    if (node->is_root_object()) return false;

    if (node->has_join_condition()) {
      Value link;
      link.SetObject();

      const auto &jcol = node->join_column_info();
      const auto table_ref = node->table_ref();

      const auto &parent_jcol = node->parent_join_column_info();
      const auto parent_table_ref = node->parent()->table_ref();

      Value parent_catalog(default_catalog, strlen(default_catalog),
                           doc->GetAllocator());
      Value parent_schema(parent_table_ref->get_db_name(),
                          strlen(parent_table_ref->get_db_name()),
                          doc->GetAllocator());
      Value parent_name(parent_table_ref->get_table_name(),
                        strlen(parent_table_ref->get_table_name()),
                        doc->GetAllocator());
      Value parent_column_name(parent_jcol.column_name().data(),
                               parent_jcol.column_name().length(),
                               doc->GetAllocator());

      Value child_catalog(default_catalog, strlen(default_catalog),
                          doc->GetAllocator());
      Value child_schema(table_ref->get_db_name(),
                         strlen(table_ref->get_db_name()), doc->GetAllocator());
      Value child_name(table_ref->get_table_name(),
                       strlen(table_ref->get_table_name()),
                       doc->GetAllocator());
      Value child_column_name(jcol.column_name().data(),
                              jcol.column_name().length(), doc->GetAllocator());

      link.AddMember("parent_table_catalog", parent_catalog,
                     doc->GetAllocator());
      link.AddMember("parent_table_schema", parent_schema, doc->GetAllocator());
      link.AddMember("parent_table_name", parent_name, doc->GetAllocator());
      link.AddMember("parent_column_name", parent_column_name,
                     doc->GetAllocator());
      link.AddMember("child_table_catalog", child_catalog, doc->GetAllocator());
      link.AddMember("child_table_schema", child_schema, doc->GetAllocator());
      link.AddMember("child_table_name", child_name, doc->GetAllocator());
      link.AddMember("child_column_name", child_column_name,
                     doc->GetAllocator());

      const char *join_type_str =
          (node->is_singleton_child()) ? "outer" : "nested";
      Value join_type(join_type_str, strlen(join_type_str),
                      doc->GetAllocator());
      link.AddMember("join_type", join_type, doc->GetAllocator());
      Value json_key_name(node->name().data(), node->name().length(),
                          doc->GetAllocator());
      link.AddMember("json_key_name", json_key_name, doc->GetAllocator());

      (*doc)["entries"].GetArray().PushBack(link, doc->GetAllocator());
    }

    return false;
  });
}
}  // anonymous namespace

namespace jdv {
void get_i_s_properties(Content_tree_node *root, const char *i_s_view_name,
                        String *properties) {
  // Make sure the root has no parent.
  if (root == nullptr || root->parent() != nullptr) {
    assert(false);
    return;
  }

  // Prepare document to return.
  Document doc;
  doc.SetObject();
  Value entries(kArrayType);
  doc.AddMember("entries", entries, doc.GetAllocator());

  // Dispatch to fill document.
  {
    using namespace dd::system_views;
    if (i_s_view_name == Json_duality_views::view_name())
      get_json_duality_views(&doc, root);
    else if (i_s_view_name == Json_duality_view_tables::view_name())
      get_json_duality_view_tables(&doc, root);
    else if (i_s_view_name == Json_duality_view_columns::view_name())
      get_json_duality_view_columns(&doc, root);
    else if (i_s_view_name == Json_duality_view_links::view_name())
      get_json_duality_view_links(&doc, root);
    else
      assert(false);
  }

  // Serialize document into string to return.
  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  doc.Accept(writer);
  properties->copy(strbuf.GetString(), strbuf.GetLength(), system_charset_info);
}
}  // namespace jdv
