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
    if (current_node->table_tags()) {
      join_column.set_column_tags(DVT_UPDATE);
    }
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

[[nodiscard]] static bool prepare_content_tree_node(THD *thd,
                                                    Content_tree_node *node) {
  // Increment usage counter, this will also count failures.
  ++option_tracker_json_duality_view_usage_count;

  DBUG_EXECUTE_IF("simulate_context_prepare_fail", return true;);

  Query_block *sl = node->query_expression()->query_term()->query_block();
  assert(node->query_expression()->is_simple());

  Table_ref *table_ref = sl->m_table_list.first;
  if (!table_ref->is_base_table()) {
    my_error(ER_JDV_INVALID_DEFINITION_NON_BASE_TABLE_NOT_SUPPORTED, MYF(0),
             table_ref->get_db_name(), table_ref->get_table_name());
    return true;
  }

  node->set_table_ref(table_ref);

  // Qualified table name.
  std::string qname(node->table_ref()->get_db_name());
  qname.append(".");
  qname.append(node->table_ref()->get_table_name());
  char *qn = strmake_root(thd->mem_root, qname.c_str(), qname.length());
  if (qn == nullptr) return true;
  node->set_qualified_table_name(qn);

  // Add a single string containing the quoted qualified table name
  std::string qtn;
  append_identifier(&qtn, node->table_ref()->get_db_name());
  qtn.append(".");
  append_identifier(&qtn, node->table_ref()->get_table_name());
  node->set_quoted_qualified_table_name(std::move(qtn));

  // Get primary key column name.
  const char *primary_key_col_name =
      get_primary_key_column_name(node->table_ref());

  for (Item *it : sl->visible_fields()) {
    Item_func *func_item = down_cast<Item_func *>(it);

    // Set node type.
    if (func_item->type() == Item::SUM_FUNC_ITEM) {
      // Get JSON_DUALITY_OBJECT()'s item.
      Item_sum_json_array *json_aragg = down_cast<Item_sum_json_array *>(it);
      func_item = down_cast<Item_func *>(json_aragg->get_arg(0));

      node->set_type(Content_tree_node::Type::NESTED_CHILD);
    } else if (node->type() == Content_tree_node::Type::INVALID) {
      node->set_type(Content_tree_node::Type::SINGLETON_CHILD);
    }

    auto jdv_func_item = down_cast<Item_func_json_duality_object *>(func_item);
    node->set_table_tags(jdv_func_item->table_tags());

    std::unordered_set<std::string> columns_names_seen;
    for (uint i = 0; i < func_item->argument_count();) {
      Item *key_arg_item = func_item->get_arg(i);
      String dummy_str;
      String *name_str = key_arg_item->val_str(&dummy_str);
      assert(name_str != nullptr);

      Item *value_arg_item = func_item->get_arg(i + 1);
      i = i + 2;

      if (value_arg_item->type() == Item::SUBQUERY_ITEM) {
        Content_tree_node *child_node =
            new (thd->mem_root) Content_tree_node(thd->mem_root);

        child_node->set_name(name_str->ptr());
        child_node->set_parent(node);
        auto *subquery_item = down_cast<Item_subselect *>(value_arg_item);
        child_node->set_query_expression(subquery_item->query_expr());

        node->children()->push_back(child_node);
      } else {
        if (node->key_column_map()->find(name_str->ptr()) !=
            node->key_column_map()->end()) {
          my_error(ER_JDV_INVALID_DEFINITION_DUPLICATE_KEYS_NOT_SUPPORTED,
                   MYF(0), node->name().data(), name_str->ptr());
          return true;
        }

        char lowercase_field_name[NAME_LEN + 1];
        Item_field *fld_item = down_cast<Item_field *>(value_arg_item);
        my_stpcpy(lowercase_field_name, fld_item->field_name);
        my_casedn_str(&my_charset_utf8mb3_tolower_ci, lowercase_field_name);
        if (!(columns_names_seen.insert(lowercase_field_name)).second) {
          my_error(ER_JDV_INVALID_DEFINITION_DUPLICATE_COLUMN_NOT_SUPPORTED,
                   MYF(0), node->name().data(),
                   node->qualified_table_name().data(), fld_item->field_name);
          return true;
        }

        Key_column_info key_column_info;
        key_column_info.set_column_name(fld_item->field_name);
        key_column_info.set_key(name_str->ptr());
        key_column_info.set_field(
            get_field_for_column(node->table_ref(), fld_item->field_name));

        bool is_pk_column = false;
        if (primary_key_col_name != nullptr &&
            (my_strcasecmp(system_charset_info, primary_key_col_name,
                           fld_item->field_name) == 0)) {
          is_pk_column = true;
        }

        auto column_tags = DVT_NOUPDATE;
        if (!is_pk_column && node->allows_update()) {
          column_tags = DVT_UPDATE;
        }
        key_column_info.set_column_tags(column_tags);

        if (node->key_column_info_list()->push_back(std::move(key_column_info)))
          return true;

        auto key_column_info_idx = node->key_column_info_list()->size() - 1;

        if (is_pk_column) {
          node->set_primary_key_column_index(key_column_info_idx);
        }

        node->key_column_map()->insert(
            std::make_pair(name_str->ptr(), key_column_info_idx));
      }
    }

    if ((sl->where_cond() != nullptr) &&
        prepare_join_condition(thd, sl, node)) {
      return true;
    }
  }

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

  // Prepare each child node.
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
