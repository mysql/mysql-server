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

#include "content_tree.h"

#include "m_string.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "option_usage.h"
#include "utils.h"

#include "sql/field.h"
#include "sql/item_json_func.h"
#include "sql/item_sum.h"
#include "sql/mem_root_array.h"
#include "sql/sql_lex.h"
namespace jdv {

const Field *Key_column_info::field() const { return m_field; }

enum_field_types Key_column_info::field_type() const {
  assert(m_field != nullptr);
  return m_field->type();
}

bool Key_column_info::is_generated_column() const {
  assert(m_field != nullptr);
  return m_field->is_gcol();
}

/**
  Returns primary key name given table.
  @param [in]   table_ref   Table_ref instance of a table. current THD

  @returns primary key column name if exists, otherwise nullptr.
*/
[[nodiscard]] static const char *get_primary_key_column_name(
    const Table_ref *table_ref) {
  if (!table_ref->is_base_table()) return nullptr;

  TABLE *table = table_ref->table;
  assert(table != nullptr);

  TABLE_SHARE *table_share = table->s;

  if (table_share->is_missing_primary_key()) return nullptr;

  return table->key_info[table_share->primary_key].key_part->field->field_name;
}

/**
 * @brief Get the field instance of a column.
 *
 * @param [in]   table_ref   Table_ref instance of a table.
 * @param [in]   fld_name    Field name.
 *
 * @returns Field instance if found, nullptr otherwise.
 */
[[nodiscard]] static Field *get_field_for_column(const Table_ref *table_ref,
                                                 const char *fld_name) {
  TABLE *table = table_ref->table;
  assert(table != nullptr);

  Field *fld = nullptr;
  for (uint i = 0; i < table->s->fields; i++) {
    if (my_strcasecmp(system_charset_info, fld_name,
                      table->field[i]->field_name) == 0) {
      fld = table->field[i];
      break;
    }
  }

  return fld;
}

/**
 * @brief  Helper function to set join column index for a given node
 *
 * @param      child_node  Content_tree_node object representing the current
 *                         node
 * @param      parent_node Content_tree_node object representing the parent node
 * @param      side_ident  Item_ident object representing one side of equality
 *                         condition
 * @param      is_parent   specifies whether current node is to be processed or
 * parent
 *
 * @return     true       On failure.
 * @return     false      On success.
 */
[[nodiscard]] static bool set_join_column_index_for_node(
    Content_tree_node *child_node, Content_tree_node *parent_node,
    Item_ident *side_ident, bool is_parent) {
  Content_tree_node *current_node = is_parent ? parent_node : child_node;

  // Start by marking the join column index as not found.
  child_node->set_join_column_index(VOID_COLUMN_INDEX, is_parent);

  // Try to find the column in the key_column_info_list
  size_t idx = 0;
  for (auto key_column_info : *current_node->key_column_info_list()) {
    if (my_strcasecmp(system_charset_info, side_ident->field_name,
                      key_column_info.column_name().data()) == 0) {
      child_node->set_join_column_index(idx, is_parent);
      break;
    }
    idx++;
  }

  bool index_not_found =
      (child_node->join_column_index(is_parent) == VOID_COLUMN_INDEX);

  if (index_not_found) {
    Key_column_info join_column;
    join_column.set_column_name(side_ident->field_name);
    join_column.set_key("");
    join_column.set_field(get_field_for_column(current_node->table_ref(),
                                               side_ident->field_name));
    // Projected join columns are handled above and keep their effective tags.
    // For an unprojected child column, its value is copied from the parent to
    // maintain the join, so allow this internal write. Changing an unprojected
    // parent column remaps the child, so allow it only when the parent table
    // allows UPDATE.
    join_column.set_column_tags(!is_parent || current_node->allows_update()
                                    ? DVT_UPDATE
                                    : DVT_NOUPDATE);
    join_column.set_column_projected(false);

    if (current_node->key_column_info_list()->push_back(join_column)) {
      return true;
    }

    size_t new_index = current_node->key_column_info_list()->size() - 1;
    child_node->set_join_column_index(new_index, is_parent);
  }

  return false;  // Success
}

/**
 * @brief  Helper function to prepare join condition for sub-object.
 *
 * @param      thd        Thread Handle.
 * @param      sl         Sub-object's query block.
 * @param      node       Content tree node for a sub-object.
 *
 * @return     true       On failure.
 * @return     false      On success.
 */
[[nodiscard]] static bool prepare_join_condition(THD *thd, Query_block *sl,
                                                 Content_tree_node *node) {
  assert(!node->is_root_object() && sl->where_cond() != nullptr);

  Item_func *item = down_cast<Item_func *>(sl->where_cond());

  Item_ident *lhs_ident = down_cast<Item_ident *>(item->get_arg(0));
  Item_ident *rhs_ident = down_cast<Item_ident *>(item->get_arg(1));

  // If LHS operand is not on sub-object's table column, then swap.
  bool alias_mismatch =
      my_strcasecmp(table_alias_charset, lhs_ident->table_name,
                    node->table_ref()->alias) != 0;
  if (thd->lex->create_view_type != enum_view_type::JSON_DUALITY_VIEW) {
    if (alias_mismatch) {
      std::swap(rhs_ident, lhs_ident);
    }
  } else {
    if (alias_mismatch ||
        my_strcasecmp(table_alias_charset, lhs_ident->original_table_name(),
                      node->table_ref()->get_table_name()) != 0) {
      std::swap(rhs_ident, lhs_ident);
    }
  }

  if ((thd->lex->sql_command == enum_sql_command::SQLCOM_CREATE_VIEW) &&
      (my_strcasecmp(table_alias_charset, lhs_ident->original_db_name(),
                     node->table_ref()->get_db_name()) ||
       my_strcasecmp(table_alias_charset, lhs_ident->original_table_name(),
                     node->table_ref()->get_table_name()) ||
       my_strcasecmp(table_alias_charset, rhs_ident->original_db_name(),
                     node->parent()->table_ref()->get_db_name()) ||
       my_strcasecmp(table_alias_charset, rhs_ident->original_table_name(),
                     node->parent()->table_ref()->get_table_name()))) {
    my_error(ER_JDV_INVALID_DEFINITION_WHERE_USES_NON_IMMEDIATE_PARENT, MYF(0),
             node->name().data());
    return true;
  }

  if (set_join_column_index_for_node(node, node->parent(), lhs_ident, false) ||
      set_join_column_index_for_node(node, node->parent(), rhs_ident, true))
    return true;

  return false;
}

static thread_local uint next_id = 0;

/**
  Builds the dotted JSON path for a projection from its content-tree ancestry.

  @param node containing content-tree node
  @param key  projected JSON key

  @returns path in the form Root Node.child.key
*/
static std::string get_projection_path(const Content_tree_node *node,
                                       std::string_view key) {
  std::string path(key);
  while (node != nullptr) {
    path.insert(0, ".");
    path.insert(0, node->name().data(), node->name().length());
    node = node->parent();
  }
  return path;
}

/**
  Returns the effective table tags. This applies the default CHECK tag so every
  projected column has a table setting to inherit.
*/
static Duality_view_tags resolve_effective_table_tags(
    Duality_view_tags specified_tags) {
  Duality_view_tags effective_tags = specified_tags;
  if ((specified_tags & DVT_CHECK) == 0 &&
      (specified_tags & DVT_NOCHECK) == 0) {
    effective_tags = static_cast<Duality_view_tags>(effective_tags | DVT_CHECK);
  }
  return effective_tags;
}

/**
  Resolves the effective tags for a projected column. This keeps CHECK and
  UPDATE inheritance, including the primary-key restriction, consistent for
  all content-tree consumers.

  @param node                  containing content-tree node
  @param table_tags            effective tags of the containing table
  @param key                   projected JSON key
  @param is_primary_key        whether the column is the table primary key
  @param specified_column_tags tags written on the column projection
  @param effective_column_tags resolved tags returned to the caller

  @retval false success
  @retval true  invalid UPDATE tag on a primary key
*/
static bool resolve_effective_column_tags(
    const Content_tree_node *node, Duality_view_tags table_tags,
    std::string_view key, bool is_primary_key,
    Duality_view_tags specified_column_tags,
    Duality_view_tags *effective_column_tags) {
  if (is_primary_key && (specified_column_tags & DVT_UPDATE) != 0) {
    const std::string json_path = get_projection_path(node, key);
    my_error(ER_JDV_UPDATE_COLUMN_TAG_NOT_SUPPORTED_FOR_PK, MYF(0),
             json_path.c_str());
    return true;
  }

  Duality_view_tags resolved_tags = specified_column_tags;
  if ((resolved_tags & DVT_UPDATE) == 0 &&
      (resolved_tags & DVT_NOUPDATE) == 0) {
    resolved_tags = static_cast<Duality_view_tags>(
        resolved_tags |
        (!is_primary_key && (table_tags & DVT_UPDATE) != 0 ? DVT_UPDATE
                                                           : DVT_NOUPDATE));
  }
  if ((resolved_tags & DVT_CHECK) == 0 && (resolved_tags & DVT_NOCHECK) == 0) {
    resolved_tags = static_cast<Duality_view_tags>(
        resolved_tags |
        ((table_tags & DVT_CHECK) != 0 ? DVT_CHECK : DVT_NOCHECK));
  }

  *effective_column_tags = resolved_tags;
  return false;
}

/**
  Sets the base-table identity stored on a content-tree node. This provides the
  shared qualified names used by validation, errors, and generated DML.
*/
static bool initialize_table_identity(THD *thd, Query_block *query_block,
                                      Content_tree_node *node) {
  Table_ref *table_ref = query_block->m_table_list.first;
  if (!table_ref->is_base_table()) {
    my_error(ER_JDV_INVALID_DEFINITION_NON_BASE_TABLE_NOT_SUPPORTED, MYF(0),
             table_ref->get_db_name(), table_ref->get_table_name());
    return true;
  }

  node->set_table_ref(table_ref);

  std::string qualified_name(table_ref->get_db_name());
  qualified_name.append(".");
  qualified_name.append(table_ref->get_table_name());
  char *qualified_name_root = strmake_root(
      thd->mem_root, qualified_name.c_str(), qualified_name.length());
  if (qualified_name_root == nullptr) return true;
  node->set_qualified_table_name(qualified_name_root);

  std::string quoted_qualified_name;
  append_identifier(&quoted_qualified_name, table_ref->get_db_name());
  quoted_qualified_name.append(".");
  append_identifier(&quoted_qualified_name, table_ref->get_table_name());
  node->set_quoted_qualified_table_name(std::move(quoted_qualified_name));
  return false;
}

/**
  Returns the JSON_DUALITY_OBJECT item represented by a visible field and sets
  the node type. This distinguishes nested arrays from singleton child objects
  before their projections are prepared.
*/
static Item_func_json_duality_object *get_duality_object_item(
    Item *visible_field, Content_tree_node *node) {
  auto *function = down_cast<Item_func *>(visible_field);
  if (function->type() == Item::SUM_FUNC_ITEM) {
    auto *json_array = down_cast<Item_sum_json_array *>(visible_field);
    function = down_cast<Item_func *>(json_array->get_arg(0));
    node->set_type(Content_tree_node::Type::NESTED_CHILD);
  } else if (node->type() == Content_tree_node::Type::INVALID) {
    node->set_type(Content_tree_node::Type::SINGLETON_CHILD);
  }

  return down_cast<Item_func_json_duality_object *>(function);
}

/**
  Adds a child object projection to the current node. This retains its query
  expression for recursive preparation and rejects column tags on child
  objects.
*/
static bool add_child_projection(THD *thd, Content_tree_node *node, String *key,
                                 Item *value,
                                 Duality_view_tags specified_tags) {
  if (specified_tags != DVT_INVALID) {
    const std::string json_path =
        get_projection_path(node, std::string_view(key->ptr(), key->length()));
    my_error(ER_JDV_COLUMN_TAG_NOT_SUPPORTED_FOR_SUBQUERY, MYF(0),
             json_path.c_str());
    return true;
  }

  auto *child = new (thd->mem_root) Content_tree_node(thd->mem_root);
  if (child == nullptr) return true;

  child->set_name(key->ptr());
  child->set_parent(node);
  auto *subquery = down_cast<Item_subselect *>(value);
  child->set_query_expression(subquery->query_expr());
  return node->children()->push_back(child);
}

/**
  Adds a projected base-table column and its effective tags to the current node.
  This centralizes duplicate checks, primary-key detection, and tag resolution.
*/
static bool add_column_projection(
    Content_tree_node *node, String *key, Item *value,
    const char *primary_key_column_name,
    Duality_view_tags specified_column_tags,
    std::unordered_set<std::string> *column_names_seen) {
  if (node->key_column_map()->contains(key->ptr())) {
    my_error(ER_JDV_INVALID_DEFINITION_DUPLICATE_KEYS_NOT_SUPPORTED, MYF(0),
             node->name().data(), key->ptr());
    return true;
  }

  auto *field_item = down_cast<Item_field *>(value);
  char lowercase_field_name[NAME_LEN + 1];
  my_stpcpy(lowercase_field_name, field_item->field_name);
  my_casedn_str(&my_charset_utf8mb3_tolower_ci, lowercase_field_name);
  if (!column_names_seen->insert(lowercase_field_name).second) {
    my_error(ER_JDV_INVALID_DEFINITION_DUPLICATE_COLUMN_NOT_SUPPORTED, MYF(0),
             node->name().data(), node->qualified_table_name().data(),
             field_item->field_name);
    return true;
  }

  const bool is_primary_key =
      primary_key_column_name != nullptr &&
      my_strcasecmp(system_charset_info, primary_key_column_name,
                    field_item->field_name) == 0;

  Duality_view_tags effective_column_tags;
  if (resolve_effective_column_tags(
          node, node->table_tags(), std::string_view(key->ptr(), key->length()),
          is_primary_key, specified_column_tags, &effective_column_tags)) {
    return true;
  }

  Key_column_info column_info;
  column_info.set_column_name(field_item->field_name);
  column_info.set_key(key->ptr());
  column_info.set_field(
      get_field_for_column(node->table_ref(), field_item->field_name));
  column_info.set_column_tags(effective_column_tags);
  if (node->key_column_info_list()->push_back(column_info)) {
    return true;
  }

  const auto column_index = node->key_column_info_list()->size() - 1;
  if (is_primary_key) node->set_primary_key_column_index(column_index);
  node->key_column_map()->insert(std::make_pair(key->ptr(), column_index));
  return false;
}

/**
  Adds the column and child object projections of one JSON_DUALITY_OBJECT to a
  content-tree node. This keeps projection dispatch in a single pass.
*/
static bool add_object_projections(
    THD *thd, Content_tree_node *node,
    Item_func_json_duality_object *duality_object,
    const char *primary_key_column_name) {
  const auto *specified_column_tags = duality_object->col_tags_list();
  assert(specified_column_tags->size() == duality_object->argument_count() / 2);

  std::unordered_set<std::string> column_names_seen;
  for (uint argument_index = 0, member_index = 0;
       argument_index < duality_object->argument_count();
       argument_index += 2, ++member_index) {
    Item *key_item = duality_object->get_arg(argument_index);
    String key_buffer;
    String *key = key_item->val_str(&key_buffer);
    assert(key != nullptr);

    Item *value = duality_object->get_arg(argument_index + 1);
    const auto specified_tags =
        static_cast<Duality_view_tags>(specified_column_tags->at(member_index));

    if (value->type() == Item::SUBQUERY_ITEM) {
      if (add_child_projection(thd, node, key, value, specified_tags)) {
        return true;
      }
    } else if (add_column_projection(node, key, value, primary_key_column_name,
                                     specified_tags, &column_names_seen)) {
      return true;
    }
  }
  return false;
}

/**
  Sets a node's dependency weight. This preserves parent-child foreign-key order
  when DML statements are executed.
*/
static void set_dependency_weight(Content_tree_node *node) {
  switch (node->type()) {
    case Content_tree_node::Type::ROOT:
      assert(node->dependency_weight() == 0);
      break;
    case Content_tree_node::Type::SINGLETON_CHILD:
      node->set_dependency_weight(node->parent()->dependency_weight() - 1);
      break;
    case Content_tree_node::Type::NESTED_CHILD:
      node->set_dependency_weight(node->parent()->dependency_weight() + 1);
      break;
    default:
      assert(false);
      break;
  }
}

/**
  Prepares one content-tree node and recursively prepares its children. This
  gives DDL, DML, I_S, and ETAG calculation one shared projection model.
*/
[[nodiscard]] static bool prepare_content_tree_node(THD *thd,
                                                    Content_tree_node *node) {
  // Failed preparation attempts must also count as feature use.
  ++option_tracker_json_duality_view_usage_count;

  DBUG_EXECUTE_IF("simulate_context_prepare_fail", return true;);

  Query_block *sl = node->query_expression()->query_term()->query_block();
  assert(node->query_expression()->is_simple());

  if (initialize_table_identity(thd, sl, node)) return true;

  const char *primary_key_col_name =
      get_primary_key_column_name(node->table_ref());

  for (Item *visible_field : sl->visible_fields()) {
    auto *duality_object = get_duality_object_item(visible_field, node);
    node->set_table_tags(
        resolve_effective_table_tags(duality_object->table_tags()));

    if (add_object_projections(thd, node, duality_object,
                               primary_key_col_name)) {
      return true;
    }

    if ((sl->where_cond() != nullptr) &&
        prepare_join_condition(thd, sl, node)) {
      return true;
    }
  }

  set_dependency_weight(node);

  node->set_id(next_id);
  ++next_id;
  for (auto *child_node : *node->children()) {
    if (prepare_content_tree_node(thd, child_node)) return true;
  }

  return false;
}

Content_tree_node *prepare_content_tree(THD *thd, LEX *view_lex) {
  Content_tree_node *root =
      new (thd->mem_root) Content_tree_node(thd->mem_root);
  root->set_type(Content_tree_node::Type::ROOT);
  root->set_name("Root Node");
  root->set_query_expression(view_lex->unit);

  next_id = 0;
  if (prepare_content_tree_node(thd, root)) {
    destroy_content_tree(root);
    my_error(ER_JDV_INVALID_DEFINITION_CONTEXT_PREPARE_FAILED, MYF(0));
    return nullptr;
  }

  return root;
}

void destroy_content_tree(Content_tree_node *root) {
  if (!root) return;

  std::stack<Content_tree_node *> stack;
  stack.push(root);

  while (!stack.empty()) {
    Content_tree_node *node = stack.top();
    stack.pop();

    // Push children to the stack
    for (auto *child : *node->children()) {
      stack.push(child);
    }

    // Manually call destructor
    node->~Content_tree_node();
  }
}
}  // namespace jdv
