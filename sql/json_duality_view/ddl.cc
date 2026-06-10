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

#include "ddl.h"

#include "content_tree.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/dd_view.h"
#include "sql/dd/types/table.h"
#include "sql/item_sum.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"
#include "sql/thd_raii.h"

namespace jdv {

/**
   Performs syntax validation of a given JSON DUALITY VIEW.

    @param [in] thd Current THD object
    @param [in] qe Root object of metadata tree
    @param [in] object_name key name for sub-objects, "Root Node" for root node
    @param [in] root_query does given qe represent root query expression?
    @retval false in case of success, true in case of failure
*/
[[nodiscard]] static bool validate_view_syntax(THD *thd, Query_expression *qe,
                                               const char *object_name,
                                               bool root_query) {
  if ((thd->lex->create_view_type == enum_view_type::JSON_DUALITY_VIEW) &&
      thd->lex->create_view_algorithm == VIEW_ALGORITHM_TEMPTABLE) {
    my_error(ER_JDV_ALGO_TEMPTABLE_NOT_SUPPORTED, MYF(0));
    return true;
  }

  if (!qe->is_simple()) {
    my_error(ER_JDV_INVALID_DEFINITION_NON_SIMPLE_SELECT_NOT_SUPPORTED, MYF(0),
             object_name);
    return true;
  }

  if (qe->m_with_clause) {
    my_error(ER_JDV_INVALID_DEFINITION_CTE_NOT_SUPPORTED, MYF(0), object_name);
    return true;
  }

  Query_block *sl = qe->query_term()->query_block();
  assert(sl->next_query_block() == nullptr);  // Simple query.

  if (sl->is_explicitly_grouped() || (sl->having_cond() != nullptr)) {
    my_error(ER_JDV_INVALID_DEFINITION_GROUPBY_NOT_SUPPORTED, MYF(0),
             object_name);
    return true;
  }

  if (sl->has_windows() || (sl->qualify_cond() != nullptr)) {
    my_error(ER_JDV_INVALID_DEFINITION_WINDOW_FUNCTION_NOT_SUPPORTED, MYF(0),
             object_name);
    return true;
  }

  if (sl->is_ordered()) {
    my_error(ER_JDV_INVALID_DEFINITION_ORDERBY_NOT_SUPPORTED, MYF(0),
             object_name);
    return true;
  }

  if (sl->has_limit()) {
    my_error(ER_JDV_INVALID_DEFINITION_LIMIT_NOT_SUPPORTED, MYF(0),
             object_name);
    return true;
  }

  Table_ref *table_ref = sl->m_table_list.first;
  for (auto *table_ref_it = table_ref; table_ref_it != nullptr;
       table_ref_it = table_ref_it->next_local) {
    if (table_ref_it && !table_ref_it->is_base_table()) {
      my_error(ER_JDV_INVALID_DEFINITION_NON_BASE_TABLE_NOT_SUPPORTED, MYF(0),
               table_ref_it->get_db_name(), table_ref_it->get_table_name());
      return true;
    }
  }

  if (sl->m_table_list.elements != 1) {
    my_error(ER_JDV_INVALID_DEFINITION_MULTI_TABLES_NOT_SUPPORTED, MYF(0),
             object_name);
    return true;
  }

  if ((root_query && sl->where_cond() != nullptr)) {
    my_error(ER_JDV_INVALID_DEFINITION_WHERE_CONDITION_IN_ROOTOBJECT, MYF(0));
    return true;
  }

  if (!root_query && sl->where_cond() == nullptr) {
    my_error(ER_JDV_INVALID_DEFINITION_NO_WHERE_CONDITION_IN_SUBOBJECT, MYF(0),
             object_name);
    return true;
  }

  if (sl->where_cond() != nullptr) {
    Item *where_cond = sl->where_cond();
    Item_func *item = dynamic_cast<Item_func *>(where_cond);
    if (item == nullptr || item->argument_count() != 2 ||
        dynamic_cast<Item_ident *>(item->get_arg(0)) == nullptr ||
        dynamic_cast<Item_ident *>(item->get_arg(1)) == nullptr ||
        item->functype() != Item_func::EQ_FUNC) {
      my_error(ER_JDV_INVALID_DEFINITION_WRONG_WHERE_FORMAT_FOR_SUBOBJECT,
               MYF(0), object_name);
      return true;
    }
  }

  for (Item *it : sl->visible_fields()) {
    Item_func *item = dynamic_cast<Item_func *>(it);
    if (root_query) {
      if (item == nullptr ||
          item->functype() != Item_func::JSON_DUALITY_OBJECT_FUNC) {
        my_error(ER_JDV_INVALID_DEFINITION_NO_JSON_OBJECT_IN_ROOT, MYF(0));
        return true;
      }
    } else {
      if (item != nullptr && item->type() != Item::SUM_FUNC_ITEM) {
        if (item->functype() != Item_func::JSON_DUALITY_OBJECT_FUNC) {
          my_error(ER_JDV_INVALID_DEFINITION_MUSTBE_JSON_OBJECT_FOR_SINGLETON,
                   MYF(0), object_name);
          return true;
        }
      } else {
        Item_sum_json_array *json_array_func =
            dynamic_cast<Item_sum_json_array *>(it);
        if (!json_array_func ||
            json_array_func->sum_func() != Item_sum::JSON_ARRAYAGG_FUNC) {
          my_error(ER_JDV_INVALID_DEFINITION_MUSTBE_JSON_OBJECT_FOR_NESTED,
                   MYF(0), object_name);
          return true;
        }

        Item_func *arg_func_item =
            dynamic_cast<Item_func *>(json_array_func->get_arg(0));
        if (arg_func_item == nullptr || (arg_func_item->functype() !=
                                         Item_func::JSON_DUALITY_OBJECT_FUNC)) {
          my_error(ER_JDV_INVALID_DEFINITION_NO_JSON_OBJ_IN_ARRAYAGG, MYF(0),
                   object_name);
          return true;
        }

        item = arg_func_item;
      }
    }

    for (uint i = 0; i < item->argument_count();) {
      Item *key_item = item->get_arg(i);
      Item *value_arg_item = item->get_arg(i + 1);
      i = i + 2;

      String buffer;
      const char *obj_name = key_item->val_str(&buffer)->c_ptr_safe();

      if ((value_arg_item->type() != Item::FIELD_ITEM &&
           value_arg_item->type() != Item::SUBQUERY_ITEM)) {
        my_error(ER_JDV_INVALID_DEFINITION_WRONG_FIELD_TYPE, MYF(0), obj_name);
        return true;
      }

      if (value_arg_item->type() == Item::SUBQUERY_ITEM) {
        Item_subselect *value_arg_subquery_item =
            down_cast<Item_subselect *>(value_arg_item);
        if (value_arg_subquery_item->subquery_type() ==
            Item_subselect::SCALAR_SUBQUERY) {
          if (validate_view_syntax(thd, value_arg_subquery_item->query_expr(),
                                   obj_name, false)) {
            return true;
          }
        } else {
          my_error(ER_JDV_INVALID_DEFINITION_NON_SIMPLE_SELECT_NOT_SUPPORTED,
                   MYF(0), obj_name);
          return true;
        }
      } else {
        Item_field *fld_item = down_cast<Item_field *>(value_arg_item);
        if (my_strcasecmp(table_alias_charset, table_ref->get_db_name(),
                          fld_item->original_db_name()) != 0 ||
            my_strcasecmp(table_alias_charset, table_ref->get_table_name(),
                          fld_item->original_table_name()) != 0) {
          my_error(
              ER_JDV_INVALID_DEFINITION_INCONSISTENT_TABLE_FIELD_IN_THE_OBJECT,
              MYF(0), object_name, table_ref->get_db_name(),
              table_ref->get_table_name(), fld_item->original_db_name(),
              fld_item->original_table_name());
          return true;
        }
      }
    }
  }

  return false;
}

/**
   Performs validation of below semantic rules:

   Rule: If child object is a singleton descendent, then primary key column of
         a child object's table should part of a join condition.
         If child is a nested descendent, then primary key column of a parent
         object should be part of a join condition.

    @param [in]       node      Current content tree node.

    @retval true in case of success, false in case of failure.
*/
[[nodiscard]] static bool meets_relationship_rules(Content_tree_node *node) {
  if (node->is_singleton_child() &&
      node->join_column_index() == node->primary_key_column_index()) {
    return true;
  }

  if (node->is_nested_child() &&
      node->parent_join_column_index() ==
          node->parent()->primary_key_column_index()) {
    return true;
  }

  my_error(ER_JDV_INVALID_DEFINITION_RELATIONSHIP_RULES_VIOLATED, MYF(0),
           node->name().data(), node->table_ref()->get_db_name(),
           node->table_ref()->get_table_name());
  return false;
}

/**
   Performs validation of below semantic rules:

   1. Projection of JSON, GEOMETRY columns/fields is not supported

   2. Projection of Virtual columns/fields is not supported

   @param [in] node current node of metadata tree
   @retval true in case if unsupported column is projected
*/
[[nodiscard]] static bool is_column_with_unsupported_type_projected(
    Content_tree_node *node) {
  for (auto col_info : *node->key_column_info_list()) {
    if (col_info.field() == nullptr) return false;

    if (col_info.is_generated_column()) {
      my_error(ER_JDV_INVALID_DEFINITION_COLUMN_TYPE_NOT_SUPPORTED, MYF(0),
               node->name().data(), node->table_ref()->get_db_name(),
               node->table_ref()->get_table_name(),
               col_info.column_name().data(), "GENERATED");
      return true;
    } else if (col_info.field_type() == enum_field_types::MYSQL_TYPE_JSON) {
      my_error(ER_JDV_INVALID_DEFINITION_COLUMN_TYPE_NOT_SUPPORTED, MYF(0),
               node->name().data(), node->table_ref()->get_db_name(),
               node->table_ref()->get_table_name(),
               col_info.column_name().data(), "JSON");
      return true;
    } else if (col_info.field_type() == enum_field_types::MYSQL_TYPE_GEOMETRY) {
      my_error(ER_JDV_INVALID_DEFINITION_COLUMN_TYPE_NOT_SUPPORTED, MYF(0),
               node->name().data(), node->table_ref()->get_db_name(),
               node->table_ref()->get_table_name(),
               col_info.column_name().data(), "GEOMETRY");
      return true;
    }
  }

  return false;
}

/**
    Apply duality view create rules.

    @param [in]      node               current object of metadata tree
    @param [in, out] table_columns_map  map of table name to column
                                            projections
    @retval true in case of success, false in case of failure
*/
[[nodiscard]] static bool meets_all_semantic_rules(
    Content_tree_node *node,
    std::map<std::string, Mem_root_array<Key_column_info> *>
        &table_columns_map) {
  // Rule: When parent and child table are same, they should be used with alias
  if (!node->is_root_object() &&
      !my_strcasecmp(table_alias_charset, node->qualified_table_name().data(),
                     node->parent()->qualified_table_name().data()) &&
      !(node->table_ref()->is_alias || node->parent()->table_ref()->is_alias)) {
    my_error(ER_JDV_INVALID_DEFINITION_ALIAS_NOT_USED_FOR_SAME_TABLES, MYF(0),
             node->name().data(), node->parent()->name().data());
    return false;
  }

  // Rule: Participating table should have primary key.
  if (node->table_ref()->table->s->is_missing_primary_key()) {
    my_error(ER_JDV_INVALID_DEFINITION_TABLE_WITHOUT_PK_FOUND, MYF(0),
             node->table_ref()->get_db_name(),
             node->table_ref()->get_table_name());
    return false;
  }

  // Rule: Composite Primary keys are not supported.
  TABLE *table = node->table_ref()->table;
  if (table->key_info[table->s->primary_key].user_defined_key_parts != 1) {
    my_error(ER_JDV_INVALID_DEFINITION_COMPOSITE_KEY_USED, MYF(0),
             node->name().data(), node->qualified_table_name().data());
    return false;
  }

  // Rule: Primary key column of a table must be projected.
  if (!node->is_primary_key_column_projected()) {
    my_error(ER_JDV_INVALID_DEFINITION_TABLE_WITHOUT_PK_PROJECTION_FOUND,
             MYF(0), node->name().data(), node->table_ref()->get_db_name(),
             node->table_ref()->get_table_name());
    return false;
  }

  // V1-Rule: Projection of columns of type JSON & Geometry type is not
  //          supported.
  //        : Projection of virtual columns is not supported.
  if (is_column_with_unsupported_type_projected(node)) {
    return false;
  }

  // Rule: Primary key of a Root Object must be projected with key "_id".
  // V1-Rule: Projection of any column in sub-object's with key "_id" is not
  //          supported.
  if (node->is_root_object()) {
    if (strcmp(node->primary_key_column().key().data(), "_id") != 0) {
      my_error(ER_JDV_INVALID_DEFINITION_ID_KEY_NOT_USED_BY_ROOT_TABLE, MYF(0),
               node->primary_key_column().column_name().data(),
               node->table_ref()->get_db_name(),
               node->table_ref()->get_table_name());
      return false;
    }
  } else {
    if (node->key_column_map()->find("_id") != node->key_column_map()->end()) {
      my_error(ER_JDV_INVALID_DEFINITION_ID_KEY_USED_BY_NOT_ROOT_TABLE, MYF(0),
               node->primary_key_column().column_name().data(),
               node->table_ref()->get_db_name(),
               node->table_ref()->get_table_name());
      return false;
    }
  }

  // Rule: If child object is a singleton descendent, then primary key column of
  //       a child object's table should part of a join condition.
  //       If child is a nested descendent, then primary key column of a parent
  //       object should be part of a join condition.
  if (!node->is_root_object() && !meets_relationship_rules(node)) {
    return false;
  }

  // Rule: Any combination of tags is allowed unless it is a singleton
  // descendant object. For singleton descendant objects,
  // DELETE tag is not allowed.
  if (node->table_tags() != 0) {
    bool is_singleton_child_joined_with_pk =
        node->is_singleton_child() &&
        node->join_column_index() == node->primary_key_column_index();

    if (is_singleton_child_joined_with_pk) {
      // For singleton descendant objects, check if DELETE tag is present
      if (node->table_tags() & DVT_DELETE) {
        my_error(ER_JDV_INVALID_TABLE_ANNOTATIONS_FOR_SINGLETON_OBJ, MYF(0),
                 node->name().data());
        return false;
      }
      // Any other combination (INSERT, UPDATE, INSERT|UPDATE) is allowed
    }
    // For non-singleton objects, any combination of tags is allowed
  }

  // Rule: If a table is projected multiple times, then set of columns projected
  //       must be consistent across all instances.
  std::string table_name_key = std::string(node->qualified_table_name());
  auto table_it = table_columns_map.find(table_name_key);
  if (table_it != table_columns_map.end()) {
    std::set<std::string> seen_column_projection_set;
    for (auto &kcol : *table_it->second) {
      if (kcol.key().length() != 0) {
        seen_column_projection_set.insert(std::string(kcol.column_name()));
      }
    }

    std::set<std::string> column_projection_set;
    for (auto kcol : *node->key_column_map()) {
      column_projection_set.insert(std::string(
          node->key_column_info_list()->at(kcol.second).column_name()));
    }

    if (column_projection_set != seen_column_projection_set) {
      my_error(ER_JDV_INVALID_DEFINITION_SAME_TABLE_INCONSISTENT_PROJECTION,
               MYF(0), node->table_ref()->get_db_name(),
               node->table_ref()->get_table_name());
      return false;
    }
  } else {
    table_columns_map.insert(
        std::make_pair(table_name_key, node->key_column_info_list()));
  }
  return true;
}

/**
   Performs semantic validation of a given JSON DUALITY VIEW.

   @param [in] root Object of metadata tree
   @param [in, out] table_columns_map map of table name to column projections

   @retval false in case of success, true in case of failure
*/
[[nodiscard]] static bool validate_view_semantics(
    Content_tree_node *root,
    std::map<std::string, Mem_root_array<Key_column_info> *>
        &table_columns_map) {
  std::stack<Content_tree_node *> nodes_left;
  nodes_left.push(root);

  while (!nodes_left.empty()) {
    Content_tree_node *curr_node = nodes_left.top();
    nodes_left.pop();

    if (!meets_all_semantic_rules(curr_node, table_columns_map)) {
      return true;
    }

    for (auto *child : *curr_node->children()) {
      nodes_left.push(child);
    }
  }

  return false;
}

/**
 * @brief Class to handle view Lex.
 *        View lex for view query is used to validate the syntax,
 *        prepare content tree and for semantic validation. View lex is
 *        available at a) THD::lex while creating or altering a duality view,
 *                     b) Table_ref::view_query() when a view is opened for any
 *                        operation,
 *                     c) While executing a Prepared Statement and Stored
 *                        Program statement, view lex in not available.
 *                        Hence, it should be re-prepared.
 *        This class is responsible for getting lex from the sources listed for
 *        duality view validation and content tree preparation.
 */
class View_lex_handler {
 public:
  View_lex_handler(THD *thd, Table_ref *view_ref)
      : m_thd(thd), m_view_ref(view_ref) {}

  View_lex_handler() = delete;
  View_lex_handler(const View_lex_handler &) = delete;
  View_lex_handler(View_lex_handler &&) = delete;
  View_lex_handler &operator=(const View_lex_handler &) = delete;
  View_lex_handler &operator=(View_lex_handler &&) = delete;

  LEX *get_view_lex() {
    // CREATE VIEW, CREATE OR REPLACE VIEW or ALTER VIEW statement.
    if (m_thd->lex->sql_command == enum_sql_command::SQLCOM_CREATE_VIEW) {
      /**
        While executing a prepared statement or SP instruction, the first
        Table_ref instance in the query_tables could be Table_ref instance of
        a view being created. Unlink it from the view query tables list.
      */
      if (m_thd->stmt_arena->get_state() == Query_arena::STMT_EXECUTED &&
          m_thd->lex->query_tables->is_json_duality_view()) {
        m_first_table = m_thd->lex->query_tables;
        m_thd->lex->unlink_first_table(&m_link_to_local);
      }

      return m_thd->lex;
    }

    // Regular statement or Prepare for PS and SP.
    if (m_thd->stmt_arena->is_regular() ||
        m_thd->stmt_arena->is_stmt_prepare_or_first_sp_execute()) {
      m_view_ref->view_query();
    }

    /**
      Execute prepared PS and SP statement.
      For syntax validation of a view query, content tree preparation and
      sematic validations, AST of a view query is used. While executing PS and
      SP statement, view is opened but LEX instance for view query is not
      prepared. PS or SP instruction, contains resolved LEX of a statement. AST
      for view_query is not available. Hence, re-preparing only AST of a view
      query here.
    */
    {
      LEX *const old_lex = m_thd->lex;
      m_view_lex = (LEX *)new (m_thd->mem_root) st_lex_local;
      if (m_view_lex == nullptr) return nullptr;
      m_thd->lex = m_view_lex;

      Parser_state parser_state;
      if (parser_state.init(m_thd, m_view_ref->select_stmt.str,
                            m_view_ref->select_stmt.length))
        return nullptr;

      /*
        Use view db name as thread default database, in order to ensure
        that the view is parsed and prepared correctly.
      */
      LEX_CSTRING current_db_name_saved = m_thd->db();
      mysql_mutex_lock(&m_thd->LOCK_thd_data);
      m_thd->reset_db({m_view_ref->db, m_view_ref->db_length});
      mysql_mutex_unlock(&m_thd->LOCK_thd_data);

      lex_start(m_thd);

      bool parsing_json_duality_view_saved = m_thd->parsing_json_duality_view;
      m_thd->parsing_json_duality_view = true;

      bool result = false;
      {
        // Switch off modes which can prevent normal parsing of VIEW
        Sql_mode_parse_guard parse_guard(m_thd);

        // Parse the query text of the view
        result = parse_sql(m_thd, &parser_state, m_view_ref->view_creation_ctx);
      }

      m_thd->parsing_json_duality_view = parsing_json_duality_view_saved;

      mysql_mutex_lock(&m_thd->LOCK_thd_data);
      m_thd->reset_db(current_db_name_saved);
      mysql_mutex_unlock(&m_thd->LOCK_thd_data);

      lex_end(m_view_lex);
      m_thd->lex = old_lex;

      if (result) {
        return nullptr;
      }

      /**
        At this stage, view_ref->query_tables are already open.
        Not opening tables for m_view_lex->query_tables here again.
        But, content_tree references TABLE instance of m_view_lex->query_tables.
        TABLE instance is referenced only to prepare content tree and validate
        semantics. Hence, just pointing m_view_lex->query_tables to TABLE
        referred in view_ref->query_tables.
      */
      for (Table_ref *view_query_table = m_view_lex->query_tables;
           view_query_table != nullptr;
           view_query_table = view_query_table->next_global) {
        for (auto view_ref_table : *m_view_ref->view_tables) {
          if (my_strcasecmp(table_alias_charset,
                            view_query_table->get_db_name(),
                            view_ref_table->get_db_name()) == 0 &&
              my_strcasecmp(table_alias_charset,
                            view_query_table->get_table_name(),
                            view_ref_table->get_table_name()) == 0) {
            view_query_table->table = view_ref_table->table;
          }
        }
      }
    }

    return m_view_lex;
  }

  ~View_lex_handler() {
    if (m_first_table) {
      assert(m_thd->lex->sql_command == enum_sql_command::SQLCOM_CREATE_VIEW);
      m_thd->lex->link_first_table_back(m_first_table, m_link_to_local);
      m_first_table = nullptr;
    }
  }

 private:
  /// Thread Handle.
  THD *m_thd{nullptr};

  /// Table_ref instance of a duality view.
  Table_ref *m_view_ref{nullptr};

  /// If view query is re-parsed, then contains LEX instance of a view_query.
  LEX *m_view_lex{nullptr};

  /// Indicates if first table from query tables is unlinked.
  Table_ref *m_first_table{nullptr};
  bool m_link_to_local{false};
};

bool is_prepare_required(THD *thd, Table_ref *view) {
  // Skip semantic validation and content-tree preparation for SHOW CREATE
  // operation.
  if (thd->lex->sql_command == enum_sql_command::SQLCOM_SHOW_CREATE)
    return false;

  // Prepare for CREATE and ALTER JSON DUALITY view operations.
  if (thd->lex->sql_command == enum_sql_command::SQLCOM_CREATE_VIEW)
    return true;

  // While opening a duality view, prepare only if all base tables used by
  // duality view are opened. While opening table for stored programs, opening a
  // view is successful even if base table doesn't exists. Error for the
  // non-existing table is reported later.  Hence, preparing only if all base
  // tables are opened.
  return std::all_of(
      view->view_tables->cbegin(), view->view_tables->cend(),
      [](Table_ref *view_table) { return view_table->table != nullptr; });
}

bool prepare(THD *thd, Table_ref *view) {
  View_lex_handler view_lex_handler(thd, view);
  LEX *view_lex = view_lex_handler.get_view_lex();
  if (view_lex == nullptr) return true;

  // Validate syntax only while creating a view. Once view is created with valid
  // syntax, for other operations while opening a view syntax validation is
  // skipped.
  bool is_ddl_statement =
      (thd->lex->sql_command == enum_sql_command::SQLCOM_CREATE_VIEW);
  if (is_ddl_statement &&
      validate_view_syntax(thd, view_lex->unit, "Root Node", true))
    return true;

  // Prepare content tree for a duality view.
  Content_tree_node *content_tree = prepare_content_tree(thd, view_lex);
  if (content_tree == nullptr) return true;

  // Apply create rules.
  std::map<std::string, Mem_root_array<Key_column_info> *> table_to_columns_map;
  if (validate_view_semantics(content_tree, table_to_columns_map)) {
    destroy_content_tree(content_tree);
    return true;
  }

  view->jdv_content_tree = content_tree;
  return false;
}
}  // namespace jdv
