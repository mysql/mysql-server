#pragma once

/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "sql/sql_class.h"
#include "sql/table.h"

class Field;

namespace jdv {

enum Duality_view_tags : int {
  DVT_INVALID = 0,
  DVT_INSERT = 1,
  DVT_UPDATE = 2,
  DVT_DELETE = 4,
  DVT_NOINSERT = 8,
  DVT_NOUPDATE = 16,
  DVT_NODELETE = 32
};

constexpr std::size_t VOID_COLUMN_INDEX =
    std::numeric_limits<std::size_t>::max();

/**
 * @brief  Class to represent each key and column information from JSON duality
 *         object.
 */
class Key_column_info {
 private:
  /// Base column name.
  std::string_view m_column_name;

  /// Key in JDV definition.
  std::string_view m_key;

  /// Field instance of a column.
  const Field *m_field{nullptr};

  /// Column tags.
  Duality_view_tags m_column_tags{0};

  /// Flag to indicate column is projected or not.
  bool m_is_column_projected{true};

 public:
  /////////////////////////////////////////////////////////////////////////////
  // Column name.
  /////////////////////////////////////////////////////////////////////////////
  void set_column_name(const char *column_name) { m_column_name = column_name; }
  const std::string_view &column_name() const { return m_column_name; }

  /////////////////////////////////////////////////////////////////////////////
  // Key.
  /////////////////////////////////////////////////////////////////////////////
  void set_key(const char *key) { m_key = key; }
  const std::string_view &key() const { return m_key; }

  /////////////////////////////////////////////////////////////////////////////
  // Field instance of a column.
  /////////////////////////////////////////////////////////////////////////////
  void set_field(Field *fld) { m_field = fld; }
  const Field *field() const;
  enum_field_types field_type() const;
  bool is_generated_column() const;

  /////////////////////////////////////////////////////////////////////////////
  // Column tags.
  /////////////////////////////////////////////////////////////////////////////
  void set_column_tags(Duality_view_tags tags) { m_column_tags = tags; }
  Duality_view_tags column_tags() const { return m_column_tags; }

  bool allows_insert() const {
    return static_cast<bool>(m_column_tags & DVT_INSERT);
  }
  bool allows_update() const {
    return static_cast<bool>(m_column_tags & DVT_UPDATE);
  }
  bool allows_delete() const {
    return static_cast<bool>(m_column_tags & DVT_DELETE);
  }
  bool read_only() const {
    return !(m_column_tags & (DVT_INSERT | DVT_UPDATE | DVT_DELETE));
  }

  /////////////////////////////////////////////////////////////////////////////
  // Column projected.
  /////////////////////////////////////////////////////////////////////////////
  void set_column_projected(bool col_projected) {
    m_is_column_projected = col_projected;
  }
  bool is_column_projected() const { return m_is_column_projected; }
};

/*
 * @brief Class to represent each object of JSON duality view in the Content
 *        tree.
 */
class Content_tree_node {
 public:
  /// Types of object.
  enum class Type { INVALID, ROOT, SINGLETON_CHILD, NESTED_CHILD };

 private:
  /// Name of this node.  Holds "Root Node" for Root and Key name for
  /// descendents.
  std::string_view m_name;

  /// Node id. Displayed in I_S.
  uint m_id{0};

  /// Object query expression.
  const Query_expression *m_query_expression{nullptr};

  /// Table_ref instance of query table.
  const Table_ref *m_table_ref{nullptr};

  /// Qualified table name.
  std::string_view m_qualified_table_name;

  /// Qualified table name with quotes.
  std::string m_quoted_qualified_table_name;

  /// Table level DV tags.
  Duality_view_tags m_table_tags{0};

  /// Node type.
  Type m_type{Type::INVALID};

  /// Parent node.
  Content_tree_node *m_parent{nullptr};

  /// List of children nodes.
  Mem_root_array<Content_tree_node *> m_children;

  /// List of base columns with tags and key information.
  Mem_root_array<Key_column_info> m_key_column_info_list;

  /// Key to base columns in m_key_column_info_list map.
  std::map<std::string_view, std::size_t> m_key_column_map;

  /// Index of primary key column in m_key_column_info_list.
  std::size_t m_primary_key_column{VOID_COLUMN_INDEX};

  /// Index of join condition column in m_key_column_info_list.
  std::size_t m_join_column_index{VOID_COLUMN_INDEX};

  /// Index of join condition column in Parent's m_key_column_info_list.
  std::size_t m_parent_join_column_index{VOID_COLUMN_INDEX};

  /// Dependency weight to order DML operations.
  int m_dependency_weight{0};

 public:
  Content_tree_node() = default;
  ~Content_tree_node() = default;

  Content_tree_node(MEM_ROOT *mem_root)
      : m_children(mem_root), m_key_column_info_list(mem_root) {}

  /////////////////////////////////////////////////////////////////////////////
  // Name of node.
  /////////////////////////////////////////////////////////////////////////////
  void set_name(const char *name) { m_name = name; }
  const std::string_view &name() const { return m_name; }

  /////////////////////////////////////////////////////////////////////////////
  // Node id (Table_id in I_S)
  /////////////////////////////////////////////////////////////////////////////
  void set_id(uint id) { m_id = id; }
  uint id() const { return m_id; }

  /////////////////////////////////////////////////////////////////////////////
  // Query expression.
  /////////////////////////////////////////////////////////////////////////////
  void set_query_expression(Query_expression *qe) { m_query_expression = qe; }
  const Query_expression *query_expression() const {
    return m_query_expression;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Table_ref.
  /////////////////////////////////////////////////////////////////////////////
  void set_table_ref(Table_ref *table_ref) { m_table_ref = table_ref; }
  const Table_ref *table_ref() const { return m_table_ref; }

  /////////////////////////////////////////////////////////////////////////////
  // Qualified table name.
  /////////////////////////////////////////////////////////////////////////////
  void set_qualified_table_name(const char *qname) {
    m_qualified_table_name = qname;
  }
  const std::string_view &qualified_table_name() const {
    return m_qualified_table_name;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Quoted qualified table name.
  /////////////////////////////////////////////////////////////////////////////
  void set_quoted_qualified_table_name(std::string &&qname) {
    m_quoted_qualified_table_name = qname;
  }
  const std::string &quoted_qualified_table_name() const {
    return m_quoted_qualified_table_name;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Table tags.
  /////////////////////////////////////////////////////////////////////////////
  void set_table_tags(Duality_view_tags table_tags) {
    assert(!((table_tags & DVT_INSERT && table_tags & DVT_NOINSERT) ||
             (table_tags & DVT_UPDATE && table_tags & DVT_NOUPDATE) ||
             (table_tags & DVT_DELETE && table_tags & DVT_NODELETE)));
    m_table_tags = table_tags;
  }
  Duality_view_tags table_tags() const { return m_table_tags; }
  bool allows_insert() const { return (table_tags() & DVT_INSERT) != 0; }

  bool allows_update() const { return (table_tags() & DVT_UPDATE) != 0; }

  bool allows_delete() const { return (table_tags() & DVT_DELETE) != 0; }

  bool read_only() const {
    return !allows_insert() && !allows_update() && !allows_delete();
  }

  /////////////////////////////////////////////////////////////////////////////
  // Node type.
  /////////////////////////////////////////////////////////////////////////////
  void set_type(Type type) { m_type = type; }
  Type type() const { return m_type; }
  const char *type_string() const {
    return (type() == Type::SINGLETON_CHILD ? "singleton" : "nested");
  }
  bool is_root_object() const { return type() == Type::ROOT; }
  bool is_singleton_child() const { return type() == Type::SINGLETON_CHILD; }
  bool is_nested_child() const { return type() == Type::NESTED_CHILD; }

  /////////////////////////////////////////////////////////////////////////////
  // Parent node.
  /////////////////////////////////////////////////////////////////////////////
  void set_parent(Content_tree_node *parent) { m_parent = parent; }
  Content_tree_node *parent() const { return m_parent; }

  /////////////////////////////////////////////////////////////////////////////
  // Children nodes.
  /////////////////////////////////////////////////////////////////////////////
  Mem_root_array<Content_tree_node *> *children() { return &m_children; }
  const Mem_root_array<Content_tree_node *> &children() const {
    return m_children;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Key columns info list.
  /////////////////////////////////////////////////////////////////////////////
  Mem_root_array<Key_column_info> *key_column_info_list() {
    return &m_key_column_info_list;
  }
  const Mem_root_array<Key_column_info> &key_column_info_list() const {
    return m_key_column_info_list;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Key to column map.
  /////////////////////////////////////////////////////////////////////////////
  std::map<std::string_view, std::size_t> *key_column_map() {
    return &m_key_column_map;
  }
  const std::map<std::string_view, std::size_t> &key_column_map() const {
    return m_key_column_map;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Primary key column index in key columns info list.
  /////////////////////////////////////////////////////////////////////////////
  void set_primary_key_column_index(std::size_t key_col_idx) {
    m_primary_key_column = key_col_idx;
  }
  std::size_t primary_key_column_index() const { return m_primary_key_column; }

  bool is_primary_key_column_projected() {
    return m_primary_key_column != VOID_COLUMN_INDEX;
  }

  const Key_column_info &primary_key_column() const {
    return m_key_column_info_list.at(m_primary_key_column);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Join column index in key columns info list.
  /////////////////////////////////////////////////////////////////////////////
  void set_join_column_index(std::size_t join_col_idx) {
    m_join_column_index = join_col_idx;
  }
  std::size_t join_column_index() const { return m_join_column_index; }

  bool has_join_condition() const {
    return m_join_column_index != VOID_COLUMN_INDEX;
  }

  const Key_column_info &join_column_info() const {
    return m_key_column_info_list.at(m_join_column_index);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Join column index in parent node's key columns info list.
  /////////////////////////////////////////////////////////////////////////////
  void set_parent_join_column_index(std::size_t join_col_idx) {
    m_parent_join_column_index = join_col_idx;
  }
  std::size_t parent_join_column_index() const {
    return m_parent_join_column_index;
  }

  const Key_column_info &parent_join_column_info() const {
    assert(!is_root_object() && m_parent != nullptr);
    return m_parent->key_column_info_list()->at(m_parent_join_column_index);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Join column index in current node and parent node's key columns info list.
  /////////////////////////////////////////////////////////////////////////////
  void set_join_column_index(std::size_t join_col_idx, bool is_parent) {
    if (is_parent)
      set_parent_join_column_index(join_col_idx);
    else
      set_join_column_index(join_col_idx);
  }

  std::size_t join_column_index(bool is_parent) const {
    return is_parent ? parent_join_column_index() : join_column_index();
  }

  /////////////////////////////////////////////////////////////////////////////
  // Dependency weight of a node.
  /////////////////////////////////////////////////////////////////////////////
  void set_dependency_weight(int weight) { m_dependency_weight = weight; }
  int dependency_weight() const { return m_dependency_weight; }
};

/**
   Constructs the content tree for given JSON duality view.

   @param [in] thd      THD context.
   @param [in] view_lex LEX* object for the current query

   @returns Content_tree on success, nullptr otherwise.
*/
Content_tree_node *prepare_content_tree(THD *thd, LEX *view_lex);

/**
   Deletes the content tree for given JSON duality view.

   @param [in] root  Root object of content tree
*/
void destroy_content_tree(Content_tree_node *root);
}  // namespace jdv
