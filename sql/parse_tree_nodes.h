/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PARSE_TREE_NODES_INCLUDED
#define PARSE_TREE_NODES_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <cctype>  // std::isspace
#include <limits>

#include "binary_log_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "my_base.h"
#include "my_bit.h"  // is_single_bit
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "my_time.h"
#include "mysql/mysql_lex_string.h"
#include "mysqld_error.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/enum_query_type.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/key_spec.h"
#include "sql/mdl.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"       // table_alias_charset
#include "sql/opt_explain.h"  // Sql_cmd_explain_other_thread
#include "sql/parse_location.h"
#include "sql/parse_tree_helpers.h"  // PT_item_list
#include "sql/parse_tree_node_base.h"
#include "sql/parse_tree_partitions.h"
#include "sql/partition_info.h"
#include "sql/query_result.h"  // Query_result
#include "sql/resourcegroups/resource_group_basic_types.h"
#include "sql/resourcegroups/resource_group_sql_cmd.h"
#include "sql/set_var.h"
#include "sql/sp_head.h"    // sp_head
#include "sql/sql_admin.h"  // Sql_cmd_shutdown etc.
#include "sql/sql_alter.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_cmd_srs.h"
#include "sql/sql_exchange.h"
#include "sql/sql_lex.h"  // LEX
#include "sql/sql_list.h"
#include "sql/sql_load.h"   // Sql_cmd_load_table
#include "sql/sql_parse.h"  // add_join_natural
#include "sql/sql_partition_admin.h"
#include "sql/sql_restart_server.h"  // Sql_cmd_restart_server
#include "sql/sql_show.h"
#include "sql/sql_tablespace.h"  // Tablespace_options
#include "sql/sql_truncate.h"    // Sql_cmd_truncate_table
#include "sql/table.h"           // Common_table_expr
#include "sql/table_function.h"  // Json_table_column
#include "sql/window.h"          // Window
#include "sql/window_lex.h"
#include "sql_string.h"
#include "thr_lock.h"

class PT_field_def_base;
class PT_hint_list;
class PT_query_expression;
class PT_subquery;
class PT_type;
class Sql_cmd;
struct MEM_ROOT;

/**
  @defgroup ptn  Parse tree nodes
  @ingroup  Parser
*/
/**
  @defgroup ptn_stmt  Nodes representing SQL statements
  @ingroup  ptn
*/
/**
  @defgroup ptn_create_table  CREATE TABLE statement
  @ingroup  ptn_stmt
*/
/**
  @defgroup ptn_alter_table  ALTER TABLE statement
  @ingroup  ptn_stmt
*/
/**
  @defgroup ptn_create_table_stuff  Clauses of CREATE TABLE statement
  @ingroup  ptn_create_table
*/
/**
  @defgroup ptn_partitioning CREATE/ALTER TABLE partitioning-related stuff
  @ingroup  ptn_create_table ptn_alter_table
*/
/**
  @defgroup ptn_part_options Partition options in CREATE/ALTER TABLE
  @ingroup  ptn_partitioning
*/
/**
  @defgroup ptn_create_or_alter_table_options  Table options of CREATE/ALTER
  TABLE
  @anchor   ptn_create_or_alter_table_options
  @ingroup  ptn_create_table ptn_alter_table
*/
/**
  @defgroup ptn_col_types  Column types in CREATE/ALTER TABLE
  @ingroup  ptn_create_table ptn_alter_table
*/
/**
  @defgroup ptn_col_attrs  Column attributes in CREATE/ALTER TABLE
  @ingroup  ptn_create_table ptn_alter_table
*/
/**
  @defgroup ptn_not_gcol_attr Non-generated column attributes in CREATE/ALTER
  TABLE
  @ingroup ptn_col_attrs ptn_alter_table
*/

/**
  Calls contextualize() on every node in the array.
*/
template <class Node_type, class Parse_context_type>
bool contextualize_nodes(Mem_root_array_YY<Node_type *> nodes,
                         Parse_context_type *pc) {
  for (Node_type *i : nodes)
    if (i->contextualize(pc)) return true;
  return false;
}

/**
  Base class for all top-level nodes of SQL statements

  @ingroup ptn_stmt
*/
class Parse_tree_root {
  Parse_tree_root(const Parse_tree_root &) = delete;
  void operator=(const Parse_tree_root &) = delete;

 protected:
  virtual ~Parse_tree_root() {}
  Parse_tree_root() {}

 public:
  virtual Sql_cmd *make_cmd(THD *thd) = 0;
};

class PT_table_ddl_stmt_base : public Parse_tree_root {
 public:
  explicit PT_table_ddl_stmt_base(MEM_ROOT *mem_root)
      : m_alter_info(mem_root) {}

  virtual ~PT_table_ddl_stmt_base() = 0;  // force abstract class

 protected:
  Alter_info m_alter_info;
};

inline PT_table_ddl_stmt_base::~PT_table_ddl_stmt_base() {}

/**
  Convenience function that calls Parse_tree_node::contextualize() on the node
  if it's non-NULL.
*/
template <class Context, class Node>
bool contextualize_safe(Context *pc, Node *node) {
  if (node == NULL) return false;

  return node->contextualize(pc);
}

/**
  Parse context for the table DDL (ALTER TABLE and CREATE TABLE) nodes.

  For internal use in the contextualization code.
*/
struct Table_ddl_parse_context final : public Parse_context {
  Table_ddl_parse_context(THD *thd, SELECT_LEX *select, Alter_info *alter_info)
      : Parse_context(thd, select),
        create_info(thd->lex->create_info),
        alter_info(alter_info),
        key_create_info(&thd->lex->key_create_info) {}

  HA_CREATE_INFO *const create_info;
  Alter_info *const alter_info;
  KEY_CREATE_INFO *const key_create_info;
};

/**
  Base class for all table DDL (ALTER TABLE and CREATE TABLE) nodes.
*/
typedef Parse_tree_node_tmpl<Table_ddl_parse_context> Table_ddl_node;

/**
  Convenience function that calls Item::itemize() on the item if it's
  non-NULL.
*/
inline bool itemize_safe(Parse_context *pc, Item **item) {
  if (*item == NULL) return false;
  return (*item)->itemize(pc, item);
}

class PT_order_expr : public Parse_tree_node, public ORDER {
  typedef Parse_tree_node super;

 public:
  PT_order_expr(Item *item_arg, enum_order dir) {
    item_ptr = item_arg;
    direction = (dir == ORDER_DESC) ? ORDER_DESC : ORDER_ASC;
  }

  virtual bool contextualize(Parse_context *pc) {
    return super::contextualize(pc) || item_ptr->itemize(pc, &item_ptr);
  }
};

class PT_order_list : public Parse_tree_node {
  typedef Parse_tree_node super;

 public:
  SQL_I_List<ORDER> value;

 public:
  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;
    for (ORDER *o = value.first; o != NULL; o = o->next) {
      if (static_cast<PT_order_expr *>(o)->contextualize(pc)) return true;
    }
    return false;
  }

  void push_back(PT_order_expr *order) {
    order->item = &order->item_ptr;
    order->used_alias = false;
    order->used = 0;
    order->is_position = false;
    value.link_in_list(order, &order->next);
  }
};

class PT_gorder_list : public PT_order_list {
  typedef PT_order_list super;

 public:
  virtual bool contextualize(Parse_context *pc) {
    return super::contextualize(pc);
  }
};

/**
  Represents an element of the WITH list:
  WITH [...], [...] SELECT ...,
         ^  or  ^
  i.e. a Common Table Expression (CTE, or Query Name in SQL99 terms).
*/
class PT_common_table_expr : public Parse_tree_node {
  typedef Parse_tree_node super;

 public:
  explicit PT_common_table_expr(const LEX_STRING &name,
                                const LEX_STRING &subq_text,
                                uint subq_text_offset, PT_subquery *sn,
                                const Create_col_name_list *column_names,
                                MEM_ROOT *mem_root);

  /// The name after AS
  const LEX_STRING &name() const { return m_name; }
  /**
    @param      thd  Thread handler
    @param[out] node PT_subquery
    @returns a PT_subquery to attach to a table reference for this CTE
  */
  bool make_subquery_node(THD *thd, PT_subquery **node);
  /**
    @param tl  Table reference to match
    @param in_self  If this is a recursive reference
    @param[out]  found Is set to true/false if matches or not
    @returns true if error
  */
  bool match_table_ref(TABLE_LIST *tl, bool in_self, bool *found);
  /**
    @returns true if 'other' is the same instance as 'this'
  */
  bool is(const Common_table_expr *other) const {
    return other == &m_postparse;
  }
  void print(THD *thd, String *str, enum_query_type query_type);

 private:
  LEX_STRING m_name;
  /// Raw text of query expression (including parentheses)
  const LEX_STRING m_subq_text;
  /**
    Offset in bytes of m_subq_text in original statement which had the WITH
    clause.
  */
  uint m_subq_text_offset;
  /// Parsed version of subq_text
  PT_subquery *const m_subq_node;
  /// List of explicitely specified column names; if empty, no list.
  const Create_col_name_list m_column_names;
  /**
    A TABLE_LIST representing a CTE needs access to the WITH list
    element it derives from. However, in order to:
    - limit the members which TABLE_LIST can access
    - avoid including this header file everywhere TABLE_LIST needs to access
    these members,
    these members are relocated into a separate inferior object whose
    declaration is in table.h, like that of TABLE_LIST. It's the "postparse"
    part. TABLE_LIST accesses this inferior object only.
  */
  Common_table_expr m_postparse;

  friend bool SELECT_LEX_UNIT::clear_corr_ctes();
};

/**
   Represents the WITH list.
   WITH [...], [...] SELECT ...,
        ^^^^^^^^^^^^
*/
class PT_with_list : public Parse_tree_node {
  typedef Parse_tree_node super;

 public:
  /// @param mem_root where interior objects are allocated
  explicit PT_with_list(MEM_ROOT *mem_root) : m_elements(mem_root) {}
  bool push_back(PT_common_table_expr *el);
  const Mem_root_array<PT_common_table_expr *> &elements() const {
    return m_elements;
  }

 private:
  Mem_root_array<PT_common_table_expr *> m_elements;
};

/**
  Represents the WITH clause:
  WITH [...], [...] SELECT ...,
  ^^^^^^^^^^^^^^^^^
*/
class PT_with_clause : public Parse_tree_node {
  typedef Parse_tree_node super;

 public:
  PT_with_clause(const PT_with_list *l, bool r)
      : m_list(l), m_recursive(r), m_most_inner_in_parsing(nullptr) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true; /* purecov: inspected */
    // WITH complements a query expression (a unit).
    pc->select->master_unit()->m_with_clause = this;
    return false;
  }

  /**
    Looks up a table reference into the list of CTEs.
    @param      tl    Table reference to look up
    @param[out] found Is set to true/false if found or not
    @returns true if error
  */
  bool lookup(TABLE_LIST *tl, PT_common_table_expr **found);
  /**
    Call this to record in the WITH clause that we are contextualizing the
    CTE definition inserted in table reference 'tl'.
    @returns information which the caller must provide to
    leave_parsing_definition().
  */
  const TABLE_LIST *enter_parsing_definition(TABLE_LIST *tl) {
    auto old = m_most_inner_in_parsing;
    m_most_inner_in_parsing = tl;
    return old;
  }
  void leave_parsing_definition(const TABLE_LIST *old) {
    m_most_inner_in_parsing = old;
  }
  void print(THD *thd, String *str, enum_query_type query_type);

 private:
  /// All CTEs of this clause
  const PT_with_list *const m_list;
  /// True if the user has specified the RECURSIVE keyword.
  const bool m_recursive;
  /**
    The innermost CTE reference which we're parsing at the
    moment. Used to detect forward references, loops and recursiveness.
  */
  const TABLE_LIST *m_most_inner_in_parsing;

  friend bool SELECT_LEX_UNIT::clear_corr_ctes();
};

class PT_select_item_list : public PT_item_list {
  typedef PT_item_list super;

 public:
  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    pc->select->item_list = value;
    return false;
  }
};

class PT_limit_clause : public Parse_tree_node {
  typedef Parse_tree_node super;

  Limit_options limit_options;

 public:
  PT_limit_clause(const Limit_options &limit_options_arg)
      : limit_options(limit_options_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    if (pc->select->master_unit()->is_union() && !pc->select->braces) {
      pc->select = pc->select->master_unit()->fake_select_lex;
      DBUG_ASSERT(pc->select != NULL);
    }

    if (limit_options.is_offset_first && limit_options.opt_offset != NULL &&
        limit_options.opt_offset->itemize(pc, &limit_options.opt_offset))
      return true;

    if (limit_options.limit->itemize(pc, &limit_options.limit)) return true;

    if (!limit_options.is_offset_first && limit_options.opt_offset != NULL &&
        limit_options.opt_offset->itemize(pc, &limit_options.opt_offset))
      return true;

    pc->select->select_limit = limit_options.limit;
    pc->select->offset_limit = limit_options.opt_offset;
    pc->select->explicit_limit = true;

    pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
    return false;
  }
};

class PT_cross_join;
class PT_joined_table;

class PT_table_reference : public Parse_tree_node {
 public:
  TABLE_LIST *value;

  /**
    Lets us build a parse tree top-down, which is necessary due to the
    context-dependent nature of the join syntax. This function adds
    the `<table_ref>` cross join as the left-most leaf in this join tree
    rooted at this node.

    @todo: comment on non-join PT_table_reference objects

    @param cj This `<table ref>` will be added if it represents a cross join.

    @return The new top-level join.
  */
  virtual PT_joined_table *add_cross_join(PT_cross_join *cj);
};

class PT_table_factor_table_ident : public PT_table_reference {
  typedef PT_table_reference super;

  Table_ident *table_ident;
  List<String> *opt_use_partition;
  const char *const opt_table_alias;
  List<Index_hint> *opt_key_definition;

 public:
  PT_table_factor_table_ident(Table_ident *table_ident_arg,
                              List<String> *opt_use_partition_arg,
                              const LEX_CSTRING &opt_table_alias_arg,
                              List<Index_hint> *opt_key_definition_arg)
      : table_ident(table_ident_arg),
        opt_use_partition(opt_use_partition_arg),
        opt_table_alias(opt_table_alias_arg.str),
        opt_key_definition(opt_key_definition_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    THD *thd = pc->thd;
    Yacc_state *yyps = &thd->m_parser_state->m_yacc;

    value = pc->select->add_table_to_list(
        thd, table_ident, opt_table_alias, 0, yyps->m_lock_type,
        yyps->m_mdl_type, opt_key_definition, opt_use_partition, nullptr, pc);
    if (value == NULL) return true;
    if (pc->select->add_joined_table(value)) return true;
    return false;
  }
};

class PT_json_table_column : public Parse_tree_node {
 public:
  virtual Json_table_column *get_column() = 0;
};

class PT_table_factor_function : public PT_table_reference {
  typedef PT_table_reference super;

 public:
  PT_table_factor_function(Item *expr, const LEX_STRING &path,
                           Mem_root_array<PT_json_table_column *> *nested_cols,
                           const LEX_STRING &table_alias)
      : m_expr(expr),
        m_path(path),
        m_nested_columns(nested_cols),
        m_table_alias(table_alias) {}

  bool contextualize(Parse_context *pc) override;

 private:
  Item *m_expr;
  const LEX_STRING m_path;
  Mem_root_array<PT_json_table_column *> *m_nested_columns;
  const LEX_STRING m_table_alias;
};

class PT_table_reference_list_parens : public PT_table_reference {
  typedef PT_table_reference super;

  Mem_root_array_YY<PT_table_reference *> table_list;

 public:
  explicit PT_table_reference_list_parens(
      const Mem_root_array_YY<PT_table_reference *> table_list)
      : table_list(table_list) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || contextualize_array(pc, &table_list))
      return true;

    DBUG_ASSERT(table_list.size() >= 2);
    value = pc->select->nest_last_join(pc->thd, table_list.size());
    return value == NULL;
  }
};

class PT_derived_table : public PT_table_reference {
  typedef PT_table_reference super;

 public:
  PT_derived_table(bool lateral, PT_subquery *subquery,
                   const LEX_CSTRING &table_alias,
                   Create_col_name_list *column_names);

  virtual bool contextualize(Parse_context *pc);

 private:
  bool m_lateral;
  PT_subquery *m_subquery;
  const char *const m_table_alias;
  /// List of explicitely specified column names; if empty, no list.
  const Create_col_name_list column_names;
};

class PT_table_factor_joined_table : public PT_table_reference {
  typedef PT_table_reference super;

 public:
  PT_table_factor_joined_table(PT_joined_table *joined_table)
      : m_joined_table(joined_table) {}

  virtual bool contextualize(Parse_context *pc);

 private:
  PT_joined_table *m_joined_table;
};

class PT_joined_table : public PT_table_reference {
  typedef PT_table_reference super;

 protected:
  PT_table_reference *tab1_node;
  POS join_pos;
  PT_joined_table_type m_type;
  PT_table_reference *tab2_node;

  TABLE_LIST *tr1;
  TABLE_LIST *tr2;

 public:
  PT_joined_table(PT_table_reference *tab1_node_arg, const POS &join_pos_arg,
                  PT_joined_table_type type, PT_table_reference *tab2_node_arg)
      : tab1_node(tab1_node_arg),
        join_pos(join_pos_arg),
        m_type(type),
        tab2_node(tab2_node_arg),
        tr1(NULL),
        tr2(NULL) {
    static_assert(is_single_bit(JTT_INNER), "not a single bit");
    static_assert(is_single_bit(JTT_STRAIGHT), "not a single bit");
    static_assert(is_single_bit(JTT_NATURAL), "not a single bit");
    static_assert(is_single_bit(JTT_LEFT), "not a single bit");
    static_assert(is_single_bit(JTT_RIGHT), "not a single bit");

    DBUG_ASSERT(type == JTT_INNER || type == JTT_STRAIGHT_INNER ||
                type == JTT_NATURAL_INNER || type == JTT_NATURAL_LEFT ||
                type == JTT_NATURAL_RIGHT || type == JTT_LEFT ||
                type == JTT_RIGHT);
  }

  /**
    Adds the cross join to this join operation. The cross join is nested as
    the table reference on the left-hand side.
  */
  PT_joined_table *add_cross_join(PT_cross_join *cj) {
    tab1_node = tab1_node->add_cross_join(cj);
    return this;
  }

  /// Adds the table reference as the right-hand side of this join.
  void add_rhs(PT_table_reference *table) {
    DBUG_ASSERT(tab2_node == NULL);
    tab2_node = table;
  }

  bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || contextualize_tabs(pc)) return true;

    if (m_type & (JTT_LEFT | JTT_RIGHT)) {
      if (m_type & JTT_LEFT)
        tr2->outer_join |= JOIN_TYPE_LEFT;
      else {
        TABLE_LIST *inner_table = pc->select->convert_right_join();
        if (inner_table == NULL) return true;
        /* swap tr1 and tr2 */
        DBUG_ASSERT(inner_table == tr1);
        tr1 = tr2;
        tr2 = inner_table;
      }
    }

    if (m_type & JTT_NATURAL) tr1->add_join_natural(tr2);

    if (m_type & JTT_STRAIGHT) tr2->straight = true;

    return false;
  }

  /// This class is being inherited, it should thus be abstract.
  ~PT_joined_table() = 0;

 protected:
  bool contextualize_tabs(Parse_context *pc) {
    if (tr1 != NULL) return false;  // already done

    if (tab1_node->contextualize(pc) || tab2_node->contextualize(pc))
      return true;

    tr1 = tab1_node->value;
    tr2 = tab2_node->value;

    if (tr1 == NULL || tr2 == NULL) {
      error(pc, join_pos);
      return true;
    }
    return false;
  }
};

inline PT_joined_table::~PT_joined_table() {}

class PT_cross_join : public PT_joined_table {
  typedef PT_joined_table super;

 public:
  PT_cross_join(PT_table_reference *tab1_node_arg, const POS &join_pos_arg,
                PT_joined_table_type Type_arg,
                PT_table_reference *tab2_node_arg)
      : PT_joined_table(tab1_node_arg, join_pos_arg, Type_arg, tab2_node_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;
    value = pc->select->nest_last_join(pc->thd);
    return value == NULL;
  }
};

class PT_joined_table_on : public PT_joined_table {
  typedef PT_joined_table super;
  Item *on;

 public:
  PT_joined_table_on(PT_table_reference *tab1_node_arg, const POS &join_pos_arg,
                     PT_joined_table_type type,
                     PT_table_reference *tab2_node_arg, Item *on_arg)
      : super(tab1_node_arg, join_pos_arg, type, tab2_node_arg), on(on_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (this->contextualize_tabs(pc)) return true;

    if (push_new_name_resolution_context(pc, this->tr1, this->tr2)) {
      this->error(pc, this->join_pos);
      return true;
    }

    SELECT_LEX *sel = pc->select;
    sel->parsing_place = CTX_ON;

    if (super::contextualize(pc) || on->itemize(pc, &on)) return true;
    DBUG_ASSERT(sel == pc->select);

    add_join_on(this->tr2, on);
    pc->thd->lex->pop_context();
    DBUG_ASSERT(sel->parsing_place == CTX_ON);
    sel->parsing_place = CTX_NONE;
    value = pc->select->nest_last_join(pc->thd);
    return value == NULL;
  }
};

class PT_joined_table_using : public PT_joined_table {
  typedef PT_joined_table super;
  List<String> *using_fields;

 public:
  PT_joined_table_using(PT_table_reference *tab1_node_arg,
                        const POS &join_pos_arg, PT_joined_table_type type,
                        PT_table_reference *tab2_node_arg,
                        List<String> *using_fields_arg)
      : super(tab1_node_arg, join_pos_arg, type, tab2_node_arg),
        using_fields(using_fields_arg) {}

  /// A PT_joined_table_using without a list of columns denotes a natural join.
  PT_joined_table_using(PT_table_reference *tab1_node_arg,
                        const POS &join_pos_arg, PT_joined_table_type type,
                        PT_table_reference *tab2_node_arg)
      : PT_joined_table_using(tab1_node_arg, join_pos_arg, type, tab2_node_arg,
                              NULL) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    tr1->add_join_natural(tr2);
    value = pc->select->nest_last_join(pc->thd);
    if (value == NULL) return true;
    value->join_using_fields = using_fields;

    return false;
  }
};

class PT_group : public Parse_tree_node {
  typedef Parse_tree_node super;

  PT_order_list *group_list;
  olap_type olap;

 public:
  PT_group(PT_order_list *group_list_arg, olap_type olap_arg)
      : group_list(group_list_arg), olap(olap_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_order : public Parse_tree_node {
  typedef Parse_tree_node super;

  PT_order_list *order_list;

 public:
  explicit PT_order(PT_order_list *order_list_arg)
      : order_list(order_list_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_locking_clause : public Parse_tree_node {
 public:
  PT_locking_clause(Lock_strength strength, Locked_row_action action)
      : m_lock_strength(strength), m_locked_row_action(action) {}

  virtual bool contextualize(Parse_context *pc) final;

  virtual bool set_lock_for_tables(Parse_context *pc) = 0;

  virtual bool is_legacy_syntax() const = 0;

  Locked_row_action action() const { return m_locked_row_action; }

 protected:
  Lock_descriptor get_lock_descriptor() const {
    thr_lock_type lock_type = TL_IGNORE;
    switch (m_lock_strength) {
      case Lock_strength::UPDATE:
        lock_type = TL_WRITE;
        break;
      case Lock_strength::SHARE:
        lock_type = TL_READ_WITH_SHARED_LOCKS;
        break;
    }

    return {lock_type, static_cast<thr_locked_row_action>(action())};
  }

 private:
  Lock_strength m_lock_strength;
  Locked_row_action m_locked_row_action;
};

class PT_query_block_locking_clause : public PT_locking_clause {
 public:
  PT_query_block_locking_clause(Lock_strength strength,
                                Locked_row_action action)
      : PT_locking_clause(strength, action),
        m_is_legacy_syntax(strength == Lock_strength::UPDATE &&
                           action == Locked_row_action::WAIT) {}

  PT_query_block_locking_clause(Lock_strength strength)
      : PT_locking_clause(strength, Locked_row_action::WAIT),
        m_is_legacy_syntax(true) {}

  bool set_lock_for_tables(Parse_context *pc) override;

  bool is_legacy_syntax() const override { return m_is_legacy_syntax; }

 private:
  bool m_is_legacy_syntax;
};

class PT_table_locking_clause : public PT_locking_clause {
 public:
  typedef Mem_root_array_YY<Table_ident *> Table_ident_list;

  PT_table_locking_clause(Lock_strength strength,
                          Mem_root_array_YY<Table_ident *> tables,
                          Locked_row_action action)
      : PT_locking_clause(strength, action), m_tables(tables) {}

  bool set_lock_for_tables(Parse_context *pc) override;

  bool is_legacy_syntax() const override { return false; }

 private:
  /// @todo Move this function to Table_ident?
  void print_table_ident(THD *thd, const Table_ident *ident, String *s) {
    LEX_CSTRING db = ident->db;
    LEX_CSTRING table = ident->table;
    if (db.length > 0) {
      append_identifier(thd, s, db.str, db.length);
      s->append('.');
    }
    append_identifier(thd, s, table.str, table.length);
  }

  bool raise_error(THD *thd, const Table_ident *name, int error) {
    String s;
    print_table_ident(thd, name, &s);
    my_error(error, MYF(0), s.ptr());
    return true;
  }

  bool raise_error(int error) {
    my_error(error, MYF(0));
    return true;
  }

  Table_ident_list m_tables;
};

class PT_locking_clause_list : public Parse_tree_node {
 public:
  PT_locking_clause_list(MEM_ROOT *mem_root) {
    m_locking_clauses.init(mem_root);
  }

  bool push_back(PT_locking_clause *locking_clause) {
    return m_locking_clauses.push_back(locking_clause);
  }

  bool is_legacy_syntax() const {
    return m_locking_clauses.size() == 1 &&
           m_locking_clauses[0]->is_legacy_syntax();
  }

  bool contextualize(Parse_context *pc) {
    for (auto locking_clause : m_locking_clauses)
      if (locking_clause->contextualize(pc)) return true;
    return false;
  }

 private:
  Mem_root_array_YY<PT_locking_clause *> m_locking_clauses;
};

class PT_query_expression_body : public Parse_tree_node {
 public:
  virtual bool is_union() const = 0;
  virtual void set_containing_qe(PT_query_expression *) {}
  virtual bool has_into_clause() const = 0;
};

class PT_internal_variable_name : public Parse_tree_node {
 public:
  sys_var_with_base value;
};

class PT_internal_variable_name_1d : public PT_internal_variable_name {
  typedef PT_internal_variable_name super;

  LEX_STRING ident;

 public:
  PT_internal_variable_name_1d(const LEX_STRING &ident_arg)
      : ident(ident_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

/**
  Parse tree node class for 2-dimentional variable names (example: \@global.x)
*/
class PT_internal_variable_name_2d : public PT_internal_variable_name {
  typedef PT_internal_variable_name super;

 public:
  const POS pos;

 private:
  LEX_STRING ident1;
  LEX_STRING ident2;

 public:
  PT_internal_variable_name_2d(const POS &pos, const LEX_STRING &ident1_arg,
                               const LEX_STRING &ident2_arg)
      : pos(pos), ident1(ident1_arg), ident2(ident2_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_internal_variable_name_default : public PT_internal_variable_name {
  typedef PT_internal_variable_name super;

  LEX_STRING ident;

 public:
  PT_internal_variable_name_default(const LEX_STRING &ident_arg)
      : ident(ident_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    sys_var *tmp = find_sys_var(pc->thd, ident.str, ident.length);
    if (!tmp) return true;
    if (!tmp->is_struct()) {
      my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), ident.str);
      return true;
    }
    value.var = tmp;
    value.base_name.str = (char *)"default";
    value.base_name.length = 7;
    return false;
  }
};

class PT_option_value_following_option_type : public Parse_tree_node {
  typedef Parse_tree_node super;

  POS pos;
  PT_internal_variable_name *name;
  Item *opt_expr;

 public:
  PT_option_value_following_option_type(const POS &pos,
                                        PT_internal_variable_name *name_arg,
                                        Item *opt_expr_arg)
      : pos(pos), name(name_arg), opt_expr(opt_expr_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || name->contextualize(pc) ||
        (opt_expr != NULL && opt_expr->itemize(pc, &opt_expr)))
      return true;

    if (name->value.var && name->value.var != trg_new_row_fake_var) {
      /* It is a system variable. */
      if (set_system_variable(pc->thd, &name->value, pc->thd->lex->option_type,
                              opt_expr))
        return true;
    } else {
      /*
        Not in trigger assigning value to new row,
        and option_type preceding local variable is illegal.
      */
      error(pc, pos);
      return true;
    }
    return false;
  }
};

class PT_option_value_no_option_type : public Parse_tree_node {};

class PT_option_value_no_option_type_internal
    : public PT_option_value_no_option_type {
  typedef PT_option_value_no_option_type super;

  PT_internal_variable_name *name;
  Item *opt_expr;
  POS expr_pos;

 public:
  PT_option_value_no_option_type_internal(PT_internal_variable_name *name_arg,
                                          Item *opt_expr_arg,
                                          const POS &expr_pos_arg)
      : name(name_arg), opt_expr(opt_expr_arg), expr_pos(expr_pos_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_option_value_no_option_type_user_var
    : public PT_option_value_no_option_type {
  typedef PT_option_value_no_option_type super;

  LEX_STRING name;
  Item *expr;

 public:
  PT_option_value_no_option_type_user_var(const LEX_STRING &name_arg,
                                          Item *expr_arg)
      : name(name_arg), expr(expr_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || expr->itemize(pc, &expr)) return true;

    THD *thd = pc->thd;
    Item_func_set_user_var *item;
    item = new (pc->mem_root) Item_func_set_user_var(name, expr, false);
    if (item == NULL) return true;
    set_var_user *var = new (*THR_MALLOC) set_var_user(item);
    if (var == NULL) return true;
    thd->lex->var_list.push_back(var);
    return false;
  }
};

class PT_option_value_no_option_type_sys_var
    : public PT_option_value_no_option_type {
  typedef PT_option_value_no_option_type super;

  enum_var_type type;
  PT_internal_variable_name *name;
  Item *opt_expr;

 public:
  PT_option_value_no_option_type_sys_var(enum_var_type type_arg,
                                         PT_internal_variable_name *name_arg,
                                         Item *opt_expr_arg)
      : type(type_arg), name(name_arg), opt_expr(opt_expr_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || name->contextualize(pc) ||
        (opt_expr != NULL && opt_expr->itemize(pc, &opt_expr)))
      return true;

    THD *thd = pc->thd;
    struct sys_var_with_base tmp = name->value;
    if (tmp.var == trg_new_row_fake_var) {
      error(pc, down_cast<PT_internal_variable_name_2d *>(name)->pos);
      return true;
    }
    /* Lookup if necessary: must be a system variable. */
    if (tmp.var == NULL) {
      if (find_sys_var_null_base(thd, &tmp)) return true;
    }
    if (set_system_variable(thd, &tmp, type, opt_expr)) return true;
    return false;
  }
};

class PT_option_value_no_option_type_charset
    : public PT_option_value_no_option_type {
  typedef PT_option_value_no_option_type super;

  const CHARSET_INFO *opt_charset;

 public:
  PT_option_value_no_option_type_charset(const CHARSET_INFO *opt_charset_arg)
      : opt_charset(opt_charset_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_option_value_no_option_type_names
    : public PT_option_value_no_option_type {
  typedef PT_option_value_no_option_type super;

  POS pos;

 public:
  explicit PT_option_value_no_option_type_names(const POS &pos) : pos(pos) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_set_names : public PT_option_value_no_option_type {
  typedef PT_option_value_no_option_type super;

  const CHARSET_INFO *opt_charset;
  const CHARSET_INFO *opt_collation;

 public:
  PT_set_names(const CHARSET_INFO *opt_charset_arg,
               const CHARSET_INFO *opt_collation_arg)
      : opt_charset(opt_charset_arg), opt_collation(opt_collation_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_start_option_value_list : public Parse_tree_node {};

class PT_option_value_no_option_type_password
    : public PT_start_option_value_list {
  typedef PT_start_option_value_list super;

  const char *password;
  const char *current_password;
  bool retain_current_password;
  POS expr_pos;

 public:
  PT_option_value_no_option_type_password(const char *password_arg,
                                          const char *current_password_arg,
                                          bool retain_current,
                                          const POS &expr_pos_arg)
      : password(password_arg),
        current_password(current_password_arg),
        retain_current_password(retain_current),
        expr_pos(expr_pos_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_option_value_no_option_type_password_for
    : public PT_start_option_value_list {
  typedef PT_start_option_value_list super;

  LEX_USER *user;
  const char *password;
  const char *current_password;
  bool retain_current_password;
  POS expr_pos;

 public:
  PT_option_value_no_option_type_password_for(LEX_USER *user_arg,
                                              const char *password_arg,
                                              const char *current_password_arg,
                                              bool retain_current,
                                              const POS &expr_pos_arg)
      : user(user_arg),
        password(password_arg),
        current_password(current_password_arg),
        retain_current_password(retain_current),
        expr_pos(expr_pos_arg) {}

  virtual bool contextualize(Parse_context *pc);
};

class PT_option_value_type : public Parse_tree_node {
  typedef Parse_tree_node super;

  enum_var_type type;
  PT_option_value_following_option_type *value;

 public:
  PT_option_value_type(enum_var_type type_arg,
                       PT_option_value_following_option_type *value_arg)
      : type(type_arg), value(value_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    pc->thd->lex->option_type = type;
    return super::contextualize(pc) || value->contextualize(pc);
  }
};

class PT_option_value_list_head : public Parse_tree_node {
  typedef Parse_tree_node super;

  POS delimiter_pos;
  Parse_tree_node *value;
  POS value_pos;

 public:
  PT_option_value_list_head(const POS &delimiter_pos_arg,
                            Parse_tree_node *value_arg,
                            const POS &value_pos_arg)
      : delimiter_pos(delimiter_pos_arg),
        value(value_arg),
        value_pos(value_pos_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    THD *thd = pc->thd;
#ifndef DBUG_OFF
    LEX *old_lex = thd->lex;
#endif  // DBUG_OFF

    sp_create_assignment_lex(thd, delimiter_pos.raw.end);
    DBUG_ASSERT(thd->lex->select_lex == thd->lex->current_select());
    Parse_context inner_pc(pc->thd, thd->lex->select_lex);

    if (value->contextualize(&inner_pc)) return true;

    if (sp_create_assignment_instr(pc->thd, value_pos.raw.end)) return true;
    DBUG_ASSERT(thd->lex == old_lex &&
                thd->lex->current_select() == pc->select);

    return false;
  }
};

class PT_option_value_list : public PT_option_value_list_head {
  typedef PT_option_value_list_head super;

  PT_option_value_list_head *head;

 public:
  PT_option_value_list(PT_option_value_list_head *head_arg,
                       const POS &delimiter_pos_arg, Parse_tree_node *tail,
                       const POS &tail_pos)
      : super(delimiter_pos_arg, tail, tail_pos), head(head_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    return head->contextualize(pc) || super::contextualize(pc);
  }
};

class PT_start_option_value_list_no_type : public PT_start_option_value_list {
  typedef PT_start_option_value_list super;

  PT_option_value_no_option_type *head;
  POS head_pos;
  PT_option_value_list_head *tail;

 public:
  PT_start_option_value_list_no_type(PT_option_value_no_option_type *head_arg,
                                     const POS &head_pos_arg,
                                     PT_option_value_list_head *tail_arg)
      : head(head_arg), head_pos(head_pos_arg), tail(tail_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || head->contextualize(pc)) return true;

    if (sp_create_assignment_instr(pc->thd, head_pos.raw.end)) return true;
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select = pc->thd->lex->select_lex;

    if (tail != NULL && tail->contextualize(pc)) return true;

    return false;
  }
};

class PT_transaction_characteristic : public Parse_tree_node {
  typedef Parse_tree_node super;

  const char *name;
  int32 value;

 public:
  PT_transaction_characteristic(const char *name_arg, int32 value_arg)
      : name(name_arg), value(value_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    THD *thd = pc->thd;
    LEX *lex = thd->lex;
    Item *item = new (pc->mem_root) Item_int(value);
    if (item == NULL) return true;
    set_var *var = new (*THR_MALLOC)
        set_var(lex->option_type, find_sys_var(thd, name), &null_lex_str, item);
    if (var == NULL) return true;
    lex->var_list.push_back(var);
    return false;
  }
};

class PT_transaction_access_mode : public PT_transaction_characteristic {
  typedef PT_transaction_characteristic super;

 public:
  explicit PT_transaction_access_mode(bool is_read_only)
      : super("transaction_read_only", (int32)is_read_only) {}
};

class PT_isolation_level : public PT_transaction_characteristic {
  typedef PT_transaction_characteristic super;

 public:
  explicit PT_isolation_level(enum_tx_isolation level)
      : super("transaction_isolation", (int32)level) {}
};

class PT_transaction_characteristics : public Parse_tree_node {
  typedef Parse_tree_node super;

  PT_transaction_characteristic *head;
  PT_transaction_characteristic *opt_tail;

 public:
  PT_transaction_characteristics(PT_transaction_characteristic *head_arg,
                                 PT_transaction_characteristic *opt_tail_arg)
      : head(head_arg), opt_tail(opt_tail_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    return (super::contextualize(pc) || head->contextualize(pc) ||
            (opt_tail != NULL && opt_tail->contextualize(pc)));
  }
};

class PT_start_option_value_list_transaction
    : public PT_start_option_value_list {
  typedef PT_start_option_value_list super;

  PT_transaction_characteristics *characteristics;
  POS end_pos;

 public:
  PT_start_option_value_list_transaction(
      PT_transaction_characteristics *characteristics_arg,
      const POS &end_pos_arg)
      : characteristics(characteristics_arg), end_pos(end_pos_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    THD *thd = pc->thd;
    thd->lex->option_type = OPT_DEFAULT;
    if (characteristics->contextualize(pc)) return true;

    if (sp_create_assignment_instr(thd, end_pos.raw.end)) return true;
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select = pc->thd->lex->select_lex;

    return false;
  }
};

class PT_start_option_value_list_following_option_type
    : public Parse_tree_node {};

class PT_start_option_value_list_following_option_type_eq
    : public PT_start_option_value_list_following_option_type {
  typedef PT_start_option_value_list_following_option_type super;

  PT_option_value_following_option_type *head;
  POS head_pos;
  PT_option_value_list_head *opt_tail;

 public:
  PT_start_option_value_list_following_option_type_eq(
      PT_option_value_following_option_type *head_arg, const POS &head_pos_arg,
      PT_option_value_list_head *opt_tail_arg)
      : head(head_arg), head_pos(head_pos_arg), opt_tail(opt_tail_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || head->contextualize(pc)) return true;

    if (sp_create_assignment_instr(pc->thd, head_pos.raw.end)) return true;
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select = pc->thd->lex->select_lex;

    if (opt_tail != NULL && opt_tail->contextualize(pc)) return true;

    return false;
  }
};

class PT_start_option_value_list_following_option_type_transaction
    : public PT_start_option_value_list_following_option_type {
  typedef PT_start_option_value_list_following_option_type super;

  PT_transaction_characteristics *characteristics;
  POS characteristics_pos;

 public:
  PT_start_option_value_list_following_option_type_transaction(
      PT_transaction_characteristics *characteristics_arg,
      const POS &characteristics_pos_arg)
      : characteristics(characteristics_arg),
        characteristics_pos(characteristics_pos_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || characteristics->contextualize(pc))
      return true;

    if (sp_create_assignment_instr(pc->thd, characteristics_pos.raw.end))
      return true;
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select = pc->thd->lex->select_lex;

    return false;
  }
};

class PT_start_option_value_list_type : public PT_start_option_value_list {
  typedef PT_start_option_value_list super;

  enum_var_type type;
  PT_start_option_value_list_following_option_type *list;

 public:
  PT_start_option_value_list_type(
      enum_var_type type_arg,
      PT_start_option_value_list_following_option_type *list_arg)
      : type(type_arg), list(list_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    pc->thd->lex->option_type = type;
    return super::contextualize(pc) || list->contextualize(pc);
  }
};

class PT_set : public Parse_tree_node {
  typedef Parse_tree_node super;

  POS set_pos;
  PT_start_option_value_list *list;

 public:
  PT_set(const POS &set_pos_arg, PT_start_option_value_list *list_arg)
      : set_pos(set_pos_arg), list(list_arg) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    THD *thd = pc->thd;
    LEX *lex = thd->lex;
    lex->sql_command = SQLCOM_SET_OPTION;
    lex->option_type = OPT_SESSION;
    lex->var_list.empty();
    lex->autocommit = false;

    sp_create_assignment_lex(thd, set_pos.raw.end);
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select = pc->thd->lex->select_lex;

    return list->contextualize(pc);
  }
};

class PT_into_destination : public Parse_tree_node {
  typedef Parse_tree_node super;
  POS m_pos;

 protected:
  PT_into_destination(const POS &pos) : m_pos(pos) {}

 public:
  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    LEX *lex = pc->thd->lex;
    if (!pc->thd->lex->parsing_options.allows_select_into) {
      if (lex->sql_command == SQLCOM_SHOW_CREATE ||
          lex->sql_command == SQLCOM_CREATE_VIEW)
        my_error(ER_VIEW_SELECT_CLAUSE, MYF(0), "INTO");
      else
        error(pc, m_pos);
      return true;
    }
    return false;
  }
};

class PT_into_destination_outfile final : public PT_into_destination {
  typedef PT_into_destination super;

 public:
  PT_into_destination_outfile(const POS &pos, const LEX_STRING &file_name_arg,
                              const CHARSET_INFO *charset_arg,
                              const Field_separators &field_term_arg,
                              const Line_separators &line_term_arg)
      : PT_into_destination(pos), m_exchange(file_name_arg.str, false) {
    m_exchange.cs = charset_arg;
    m_exchange.field.merge_field_separators(field_term_arg);
    m_exchange.line.merge_line_separators(line_term_arg);
  }

  bool contextualize(Parse_context *pc) override {
    if (super::contextualize(pc)) return true;

    LEX *lex = pc->thd->lex;
    lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
    if (!(lex->result = new (*THR_MALLOC) Query_result_export(&m_exchange)))
      return true;

    return false;
  }

 private:
  sql_exchange m_exchange;
};

class PT_into_destination_dumpfile final : public PT_into_destination {
  typedef PT_into_destination super;

 public:
  PT_into_destination_dumpfile(const POS &pos, const LEX_STRING &file_name_arg)
      : PT_into_destination(pos), m_exchange(file_name_arg.str, true) {}

  bool contextualize(Parse_context *pc) override {
    if (super::contextualize(pc)) return true;

    LEX *lex = pc->thd->lex;
    if (!lex->is_explain()) {
      lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
      if (!(lex->result = new (*THR_MALLOC) Query_result_dump(&m_exchange)))
        return true;
    }
    return false;
  }

 private:
  sql_exchange m_exchange;
};

class PT_select_var : public Parse_tree_node {
 public:
  const LEX_STRING name;

  explicit PT_select_var(const LEX_STRING &name_arg) : name(name_arg) {}

  virtual bool is_local() const { return false; }
  virtual uint get_offset() const {
    DBUG_ASSERT(0);
    return 0;
  }
};

class PT_select_sp_var : public PT_select_var {
  typedef PT_select_var super;

  uint offset;

#ifndef DBUG_OFF
  /*
    Routine to which this Item_splocal belongs. Used for checking if correct
    runtime context is used for variable handling.
  */
  sp_head *sp;
#endif

 public:
  PT_select_sp_var(const LEX_STRING &name_arg) : super(name_arg) {}

  virtual bool is_local() const { return true; }
  virtual uint get_offset() const { return offset; }

  virtual bool contextualize(Parse_context *pc);
};

class PT_select_var_list : public PT_into_destination {
  typedef PT_into_destination super;

 public:
  explicit PT_select_var_list(const POS &pos) : PT_into_destination(pos) {}

  List<PT_select_var> value;

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    List_iterator<PT_select_var> it(value);
    PT_select_var *var;
    while ((var = it++)) {
      if (var->contextualize(pc)) return true;
    }

    LEX *const lex = pc->thd->lex;
    if (lex->is_explain()) return false;

    Query_dumpvar *dumpvar = new (pc->mem_root) Query_dumpvar();
    if (dumpvar == NULL) return true;

    dumpvar->var_list = value;
    lex->result = dumpvar;
    lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);

    return false;
  }

  bool push_back(PT_select_var *var) { return value.push_back(var); }
};

/**
  Parse tree node for a single of a window extent's borders,
  cf. \<window frame extent\> in SQL 2003.
*/
class PT_border : public Parse_tree_node {
  friend class Window;
  Item *m_value{nullptr};  ///< only relevant iff m_border_type == WBT_VALUE_*
 public:
  enum_window_border_type m_border_type;
  const bool m_date_time;
  interval_type m_int_type;

  ///< For unbounded border
  PT_border(enum_window_border_type type)
      : m_border_type(type), m_date_time(false) {
    DBUG_ASSERT(type != WBT_VALUE_PRECEDING && type != WBT_VALUE_FOLLOWING);
  }

  ///< For bounded non-temporal border, e.g. 2 PRECEDING: 'value' is 2.
  PT_border(enum_window_border_type type, Item *value)
      : m_value(value), m_border_type(type), m_date_time(false) {}

  ///< For bounded INTERVAL 2 DAYS, 'value' is 2, int_type is DAYS.
  PT_border(enum_window_border_type type, Item *value, interval_type int_type)
      : m_value(value),
        m_border_type(type),
        m_date_time(true),
        m_int_type(int_type) {}

  ///< @returns the '2' in '2 PRECEDING' or 'INTERVAL 2 DAYS PRECEDING'
  Item *border() const { return m_value; }
  /// Need such low-level access so that fix_fields updates the right pointer
  Item **border_ptr() { return &m_value; }

  /**
    @returns Addition operator for computation of frames, nullptr if error.
    @param  order_expr  Expression to add/substract to
    @param  prec    true if PRECEDING
    @param  asc     true if ASC
    @param  window  only used for error generation
  */
  Item *build_addop(Item_cache *order_expr, bool prec, bool asc,
                    const Window *window);
};

/**
  Parse tree node for one or both of a window extent's borders, cf.
  \<window frame extent\> in SQL 2003.
*/
class PT_borders : public Parse_tree_node {
  PT_border *m_borders[2];
  friend class PT_frame;

 public:
  /**
    Constructor.

    Frames of the form "frame_start no_frame_end" are translated during
    parsing to "BETWEEN frame_start AND CURRENT ROW". So both 'start' and
    'end' are non-nullptr.
  */
  PT_borders(PT_border *start, PT_border *end) {
    m_borders[0] = start;
    m_borders[1] = end;
  }
};

/**
  Parse tree node for a window frame's exclusions, cf. the
  \<window frame exclusion\> clause in SQL 2003.
*/
class PT_exclusion : public Parse_tree_node {
  enum_window_frame_exclusion m_exclusion;

 public:
  PT_exclusion(enum_window_frame_exclusion e) : m_exclusion(e) {}
  // enum_window_frame_exclusion exclusion() { return m_exclusion; }
};

/**
  Parse tree node for a window's frame, cf. the \<window frame clause\>
  in SQL 2003.
*/
class PT_frame : public Parse_tree_node {
 public:
  enum_window_frame_unit m_unit;

  PT_border *m_from;
  PT_border *m_to;

  PT_exclusion *m_exclusion;

  /// If true, this is an artificial frame, not specified by the user
  bool m_originally_absent = false;

  PT_frame(enum_window_frame_unit unit, PT_borders *from_to,
           PT_exclusion *exclusion)
      : m_unit(unit),
        m_from(from_to->m_borders[0]),
        m_to(from_to->m_borders[1]),
        m_exclusion(exclusion) {}
};

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

  virtual bool contextualize(Parse_context *pc);
};

/**
  Parse tree node for a list of window definitions corresponding
  to a \<window clause\> in SQL 2003.
*/
class PT_window_list : public Parse_tree_node {
  typedef Parse_tree_node super;
  List<Window> m_windows;

 public:
  PT_window_list() {}

  virtual bool contextualize(Parse_context *pc);

  bool push_back(PT_window *w) { return m_windows.push_back(w); }
};

class PT_query_primary : public Parse_tree_node {
 public:
  virtual bool has_into_clause() const = 0;
  virtual bool is_union() const = 0;
};

class PT_query_specification : public PT_query_primary {
  typedef PT_query_primary super;

  PT_hint_list *opt_hints;
  Query_options options;
  PT_item_list *item_list;
  PT_into_destination *opt_into1;
  Mem_root_array_YY<PT_table_reference *> from_clause;  // empty list for DUAL
  Item *opt_where_clause;
  PT_group *opt_group_clause;
  Item *opt_having_clause;
  PT_window_list *opt_window_clause;

 public:
  PT_query_specification(
      PT_hint_list *opt_hints_arg, const Query_options &options_arg,
      PT_item_list *item_list_arg, PT_into_destination *opt_into1_arg,
      const Mem_root_array_YY<PT_table_reference *> &from_clause_arg,
      Item *opt_where_clause_arg, PT_group *opt_group_clause_arg,
      Item *opt_having_clause_arg, PT_window_list *opt_window_clause_arg)
      : opt_hints(opt_hints_arg),
        options(options_arg),
        item_list(item_list_arg),
        opt_into1(opt_into1_arg),
        from_clause(from_clause_arg),
        opt_where_clause(opt_where_clause_arg),
        opt_group_clause(opt_group_clause_arg),
        opt_having_clause(opt_having_clause_arg),
        opt_window_clause(opt_window_clause_arg) {}

  PT_query_specification(
      const Query_options &options_arg, PT_item_list *item_list_arg,
      const Mem_root_array_YY<PT_table_reference *> &from_clause_arg,
      Item *opt_where_clause_arg)
      : opt_hints(NULL),
        options(options_arg),
        item_list(item_list_arg),
        opt_into1(NULL),
        from_clause(from_clause_arg),
        opt_where_clause(opt_where_clause_arg),
        opt_group_clause(NULL),
        opt_having_clause(NULL),
        opt_window_clause(NULL) {}

  explicit PT_query_specification(const Query_options &options_arg,
                                  PT_item_list *item_list_arg)
      : opt_hints(NULL),
        options(options_arg),
        item_list(item_list_arg),
        opt_into1(NULL),
        opt_where_clause(NULL),
        opt_group_clause(NULL),
        opt_having_clause(NULL),
        opt_window_clause(NULL) {
    from_clause.init_empty_const();
  }

  virtual bool contextualize(Parse_context *pc);

  virtual bool has_into_clause() const { return opt_into1 != NULL; }

  virtual bool is_union() const { return false; }
};

class PT_query_expression : public Parse_tree_node {
 public:
  PT_query_expression(PT_with_clause *with_clause,
                      PT_query_expression_body *body, PT_order *order,
                      PT_limit_clause *limit,
                      PT_locking_clause_list *locking_clauses)
      : contextualized(false),
        m_body(body),
        m_order(order),
        m_limit(limit),
        m_locking_clauses(locking_clauses),
        m_parentheses(false),
        m_with_clause(with_clause) {}

  PT_query_expression(PT_query_expression_body *body, PT_order *order,
                      PT_limit_clause *limit,
                      PT_locking_clause_list *locking_clauses)
      : PT_query_expression(nullptr, body, order, limit, locking_clauses) {}

  explicit PT_query_expression(PT_query_expression_body *body)
      : PT_query_expression(body, NULL, NULL, NULL) {}

  virtual bool contextualize(Parse_context *pc) {
    if (contextualize_safe(pc, m_with_clause))
      return true; /* purecov: inspected */

    pc->select->set_braces(m_parentheses || pc->select->braces);
    m_body->set_containing_qe(this);

    if (Parse_tree_node::contextualize(pc) || m_body->contextualize(pc))
      return true;

    if (!contextualized && contextualize_order_and_limit(pc)) return true;

    if (contextualize_safe(pc, m_locking_clauses)) return true;

    return false;
  }

  PT_query_expression_body *body() { return m_body; }

  bool has_order() const { return m_order != NULL; }

  bool has_limit() const { return m_limit != NULL; }

  bool is_union() const { return m_body->is_union(); }

  bool has_into_clause() const { return m_body->has_into_clause(); }

  /**
    Callback for deeper nested query expressions. It's mandatory for any
    derived class to call this member function during contextualize.
  */
  bool contextualize_order_and_limit(Parse_context *pc) {
    contextualized = true;

    /*
      We temporarily switch off 'braces' for contextualization of the limit
      and order clauses if this query expression is a
      union. PT_order::contextualize() and PT_limit_clause::contextualize()
      are still used by legacy code where 'braces' is used to communicate
      nesting information. It's not possible to express the difference between

      (SELECT ... UNION SELECT ...) ORDER BY ... LIMIT ...

      and

      SELECT ... UNION (SELECT ... ORDER BY ... LIMIT ...)

      in the SELECT_LEX structure. In other words, this structure does not
      know the difference between a surrounding union and a local
      union. Fortunately, the information is implicit in the parse tree
      structure: is_union() is true if this query expression is a union, but
      not true if it's nested within a union.
    */
    bool braces = pc->select->braces;
    if (is_union()) pc->select->braces = false;
    pc->thd->where = "global ORDER clause";
    bool res =
        contextualize_safe(pc, m_order) || contextualize_safe(pc, m_limit);
    pc->select->braces = braces;
    if (res) return true;

    pc->thd->where = THD::DEFAULT_WHERE;
    return false;
  }

  void set_parentheses() { m_parentheses = true; }

  bool has_parentheses() { return m_parentheses; }

  void remove_parentheses() { m_parentheses = false; }

  /**
    Called by the parser when it has decided that this query expression may
    not contain order or limit clauses because it is part of a union. For
    historical reasons, these clauses are not allowed in non-last branches of
    union expressions.
  */
  void ban_order_and_limit() const {
    if (m_order != NULL) my_error(ER_WRONG_USAGE, MYF(0), "UNION", "ORDER BY");
    if (m_limit != NULL) my_error(ER_WRONG_USAGE, MYF(0), "UNION", "LIMIT");
  }

 private:
  bool contextualized;
  PT_query_expression_body *m_body;
  PT_order *m_order;
  PT_limit_clause *m_limit;
  PT_locking_clause_list *m_locking_clauses;
  bool m_parentheses;
  PT_with_clause *m_with_clause;
};

class PT_subquery : public Parse_tree_node {
  typedef Parse_tree_node super;

  PT_query_expression *qe;
  POS pos;
  SELECT_LEX *select_lex;

 public:
  bool m_is_derived_table;

  PT_subquery(POS p, PT_query_expression *query_expression)
      : qe(query_expression),
        pos(p),
        select_lex(NULL),
        m_is_derived_table(false) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    LEX *lex = pc->thd->lex;
    if (!lex->expr_allows_subselect || lex->sql_command == (int)SQLCOM_PURGE) {
      error(pc, pos);
      return true;
    }

    // Create a SELECT_LEX_UNIT and SELECT_LEX for the subquery's query
    // expression.
    SELECT_LEX *child = lex->new_query(pc->select);
    if (child == NULL) return true;

    Parse_context inner_pc(pc->thd, child);

    if (m_is_derived_table) child->linkage = DERIVED_TABLE_TYPE;

    if (qe->contextualize(&inner_pc)) return true;

    select_lex = inner_pc.select->master_unit()->first_select();

    lex->pop_context();
    pc->select->n_child_sum_items += child->n_sum_items;

    /*
      A subquery (and all the subsequent query blocks in a UNION) can add
      columns to an outer query block. Reserve space for them.
    */
    for (SELECT_LEX *temp = child; temp != nullptr;
         temp = temp->next_select()) {
      pc->select->select_n_where_fields += temp->select_n_where_fields;
      pc->select->select_n_having_items += temp->select_n_having_items;
    }

    return false;
  }

  void remove_parentheses() { qe->remove_parentheses(); }

  bool is_union() { return qe->is_union(); }

  SELECT_LEX *value() { return select_lex; }
};

class PT_query_expression_body_primary : public PT_query_expression_body {
 public:
  PT_query_expression_body_primary(PT_query_primary *query_primary)
      : m_query_primary(query_primary) {}

  virtual bool contextualize(Parse_context *pc) {
    if (PT_query_expression_body::contextualize(pc) ||
        m_query_primary->contextualize(pc))
      return true;
    return false;
  }

  virtual bool is_union() const { return m_query_primary->is_union(); }

  virtual bool has_into_clause() const {
    return m_query_primary->has_into_clause();
  }

 private:
  PT_query_primary *m_query_primary;
};

class PT_union : public PT_query_expression_body {
 public:
  PT_union(PT_query_expression *lhs, const POS &lhs_pos, bool is_distinct,
           PT_query_primary *rhs)
      : m_lhs(lhs),
        m_lhs_pos(lhs_pos),
        m_is_distinct(is_distinct),
        m_rhs(rhs),
        m_containing_qe(NULL) {}

  virtual void set_containing_qe(PT_query_expression *qe) {
    m_containing_qe = qe;
  }

  virtual bool contextualize(Parse_context *pc);

  virtual bool is_union() const { return true; }

  virtual bool has_into_clause() const {
    return m_lhs->has_into_clause() || m_rhs->has_into_clause();
  }

 private:
  PT_query_expression *m_lhs;
  POS m_lhs_pos;
  bool m_is_distinct;
  PT_query_primary *m_rhs;
  PT_into_destination *m_into;
  PT_query_expression *m_containing_qe;
};

class PT_nested_query_expression : public PT_query_primary {
  typedef PT_query_primary super;

 public:
  PT_nested_query_expression(PT_query_expression *qe) : m_qe(qe) {}

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;

    pc->select->set_braces(true);
    bool result = m_qe->contextualize(pc);

    return result;
  }

  bool is_union() const { return m_qe->is_union(); }

  bool has_into_clause() const { return m_qe->has_into_clause(); }

 private:
  PT_query_expression *m_qe;
};

class PT_select_stmt : public Parse_tree_root {
  typedef Parse_tree_root super;

 public:
  /**
    @param qe The query expression.
    @param sql_command The type of SQL command.
  */
  PT_select_stmt(enum_sql_command sql_command, PT_query_expression *qe)
      : m_sql_command(sql_command), m_qe(qe), m_into(NULL) {}

  /**
    Creates a SELECT command. Only SELECT commands can have into.

    @param qe The query expression.
    @param into The trailing INTO destination.
  */
  PT_select_stmt(PT_query_expression *qe, PT_into_destination *into)
      : m_sql_command(SQLCOM_SELECT), m_qe(qe), m_into(into) {}

  PT_select_stmt(PT_query_expression *qe) : PT_select_stmt(qe, NULL) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  enum_sql_command m_sql_command;
  PT_query_expression *m_qe;
  PT_into_destination *m_into;
};

/**
  Top-level node for the DELETE statement

  @ingroup ptn_stmt
*/
class PT_delete final : public Parse_tree_root {
 private:
  PT_with_clause *m_with_clause;
  PT_hint_list *opt_hints;
  const int opt_delete_options;
  Table_ident *table_ident;
  Mem_root_array_YY<Table_ident *> table_list;
  List<String> *opt_use_partition;
  Mem_root_array_YY<PT_table_reference *> join_table_list;
  Item *opt_where_clause;
  PT_order *opt_order_clause;
  Item *opt_delete_limit_clause;
  SQL_I_List<TABLE_LIST> delete_tables;

 public:
  // single-table DELETE node constructor:
  PT_delete(PT_with_clause *with_clause_arg, PT_hint_list *opt_hints_arg,
            int opt_delete_options_arg, Table_ident *table_ident_arg,
            List<String> *opt_use_partition_arg, Item *opt_where_clause_arg,
            PT_order *opt_order_clause_arg, Item *opt_delete_limit_clause_arg)
      : m_with_clause(with_clause_arg),
        opt_hints(opt_hints_arg),
        opt_delete_options(opt_delete_options_arg),
        table_ident(table_ident_arg),
        opt_use_partition(opt_use_partition_arg),
        opt_where_clause(opt_where_clause_arg),
        opt_order_clause(opt_order_clause_arg),
        opt_delete_limit_clause(opt_delete_limit_clause_arg) {
    table_list.init_empty_const();
    join_table_list.init_empty_const();
  }

  // multi-table DELETE node constructor:
  PT_delete(PT_with_clause *with_clause_arg, PT_hint_list *opt_hints_arg,
            int opt_delete_options_arg,
            const Mem_root_array_YY<Table_ident *> &table_list_arg,
            const Mem_root_array_YY<PT_table_reference *> &join_table_list_arg,
            Item *opt_where_clause_arg)
      : m_with_clause(with_clause_arg),
        opt_hints(opt_hints_arg),
        opt_delete_options(opt_delete_options_arg),
        table_ident(NULL),
        table_list(table_list_arg),
        opt_use_partition(NULL),
        join_table_list(join_table_list_arg),
        opt_where_clause(opt_where_clause_arg),
        opt_order_clause(NULL),
        opt_delete_limit_clause(NULL) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  bool is_multitable() const {
    DBUG_ASSERT((table_ident != NULL) ^ (table_list.size() > 0));
    return table_ident == NULL;
  }

  bool add_table(Parse_context *pc, Table_ident *table);
};

/**
  Top-level node for the UPDATE statement

  @ingroup ptn_stmt
*/
class PT_update : public Parse_tree_root {
  PT_with_clause *m_with_clause;
  PT_hint_list *opt_hints;
  thr_lock_type opt_low_priority;
  bool opt_ignore;
  Mem_root_array_YY<PT_table_reference *> join_table_list;
  PT_item_list *column_list;
  PT_item_list *value_list;
  Item *opt_where_clause;
  PT_order *opt_order_clause;
  Item *opt_limit_clause;

 public:
  PT_update(PT_with_clause *with_clause_arg, PT_hint_list *opt_hints_arg,
            thr_lock_type opt_low_priority_arg, bool opt_ignore_arg,
            const Mem_root_array_YY<PT_table_reference *> &join_table_list_arg,
            PT_item_list *column_list_arg, PT_item_list *value_list_arg,
            Item *opt_where_clause_arg, PT_order *opt_order_clause_arg,
            Item *opt_limit_clause_arg)
      : m_with_clause(with_clause_arg),
        opt_hints(opt_hints_arg),
        opt_low_priority(opt_low_priority_arg),
        opt_ignore(opt_ignore_arg),
        join_table_list(join_table_list_arg),
        column_list(column_list_arg),
        value_list(value_list_arg),
        opt_where_clause(opt_where_clause_arg),
        opt_order_clause(opt_order_clause_arg),
        opt_limit_clause(opt_limit_clause_arg) {}

  Sql_cmd *make_cmd(THD *thd) override;
};

class PT_insert_values_list : public Parse_tree_node {
  typedef Parse_tree_node super;

  List<List_item> many_values;

 public:
  virtual bool contextualize(Parse_context *pc);

  bool push_back(List<Item> *x) { return many_values.push_back(x); }

  virtual List<List_item> &get_many_values() {
    DBUG_ASSERT(is_contextualized());
    return many_values;
  }
};

/**
  Top-level node for the INSERT statement

  @ingroup ptn_stmt
*/
class PT_insert final : public Parse_tree_root {
  const bool is_replace;
  PT_hint_list *opt_hints;
  const thr_lock_type lock_option;
  const bool ignore;
  Table_ident *const table_ident;
  List<String> *const opt_use_partition;
  PT_item_list *const column_list;
  PT_insert_values_list *const row_value_list;
  PT_query_expression *const insert_query_expression;
  PT_item_list *const opt_on_duplicate_column_list;
  PT_item_list *const opt_on_duplicate_value_list;

 public:
  PT_insert(bool is_replace_arg, PT_hint_list *opt_hints_arg,
            thr_lock_type lock_option_arg, bool ignore_arg,
            Table_ident *table_ident_arg, List<String> *opt_use_partition_arg,
            PT_item_list *column_list_arg,
            PT_insert_values_list *row_value_list_arg,
            PT_query_expression *insert_query_expression_arg,
            PT_item_list *opt_on_duplicate_column_list_arg,
            PT_item_list *opt_on_duplicate_value_list_arg)
      : is_replace(is_replace_arg),
        opt_hints(opt_hints_arg),
        lock_option(lock_option_arg),
        ignore(ignore_arg),
        table_ident(table_ident_arg),
        opt_use_partition(opt_use_partition_arg),
        column_list(column_list_arg),
        row_value_list(row_value_list_arg),
        insert_query_expression(insert_query_expression_arg),
        opt_on_duplicate_column_list(opt_on_duplicate_column_list_arg),
        opt_on_duplicate_value_list(opt_on_duplicate_value_list_arg) {
    // REPLACE statement can't have IGNORE flag:
    DBUG_ASSERT(!is_replace || !ignore);
    // REPLACE statement can't have ON DUPLICATE KEY UPDATE clause:
    DBUG_ASSERT(!is_replace || opt_on_duplicate_column_list == NULL);
    // INSERT/REPLACE ... SELECT can't have VALUES clause:
    DBUG_ASSERT((row_value_list != NULL) ^ (insert_query_expression != NULL));
    // ON DUPLICATE KEY UPDATE: column and value arrays must have same sizes:
    DBUG_ASSERT((opt_on_duplicate_column_list == NULL &&
                 opt_on_duplicate_value_list == NULL) ||
                (opt_on_duplicate_column_list->elements() ==
                 opt_on_duplicate_value_list->elements()));
  }

  virtual Sql_cmd *make_cmd(THD *thd);

 private:
  bool has_select() const { return insert_query_expression != NULL; }
};

class PT_call final : public Parse_tree_root {
  sp_name *proc_name;
  PT_item_list *opt_expr_list;

 public:
  PT_call(sp_name *proc_name_arg, PT_item_list *opt_expr_list_arg)
      : proc_name(proc_name_arg), opt_expr_list(opt_expr_list_arg) {}

  Sql_cmd *make_cmd(THD *thd) override;
};

/**
  Top-level node for the SHUTDOWN statement

  @ingroup ptn_stmt
*/
class PT_shutdown final : public Parse_tree_root {
  Sql_cmd_shutdown sql_cmd;

 public:
  Sql_cmd *make_cmd(THD *) override { return &sql_cmd; }
};

/**
  Top-level node for the CREATE [OR REPLACE] SPATIAL REFERENCE SYSTEM statement.

  @ingroup ptn_stmt
*/
class PT_create_srs final : public Parse_tree_root {
  /// The SQL command object.
  Sql_cmd_create_srs sql_cmd;
  /// Whether OR REPLACE is specified.
  bool m_or_replace;
  /// Whether IF NOT EXISTS is specified.
  bool m_if_not_exists;
  /// SRID of the SRS to create.
  ///
  /// The range is larger than that of gis::srid_t, so it must be
  /// verified to be less than the uint32 maximum value.
  unsigned long long m_srid;
  /// All attributes except SRID.
  const Sql_cmd_srs_attributes m_attributes;

  /// Check if a UTF-8 string contains control characters.
  ///
  /// @note This function only checks single byte control characters (U+0000 to
  /// U+001F, and U+007F). There are some control characters at U+0080 to U+00A0
  /// that are not detected by this function.
  ///
  /// @param str The string.
  /// @param length Length of the string.
  ///
  /// @retval false The string contains no control characters.
  /// @retval true The string contains at least one control character.
  bool contains_control_char(char *str, size_t length) {
    for (size_t pos = 0; pos < length; pos++) {
      if (std::iscntrl(str[pos])) return true;
    }
    return false;
  }

 public:
  PT_create_srs(unsigned long long srid,
                const Sql_cmd_srs_attributes &attributes, bool or_replace,
                bool if_not_exists)
      : m_or_replace(or_replace),
        m_if_not_exists(if_not_exists),
        m_srid(srid),
        m_attributes(attributes) {}

  Sql_cmd *make_cmd(THD *thd) override {
    // Note: This function hard-codes the maximum length of various
    // strings. These lengths must match those in
    // sql/dd/impl/tables/spatial_reference_systems.cc.

    thd->lex->sql_command = SQLCOM_CREATE_SRS;

    if (m_srid > std::numeric_limits<gis::srid_t>::max()) {
      my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "SRID",
               m_or_replace ? "CREATE OR REPLACE SPATIAL REFERENCE SYSTEM"
                            : "CREATE SPATIAL REFERENCE SYSTEM");
      return nullptr;
    }
    if (m_srid == 0) {
      my_error(ER_CANT_MODIFY_SRID_0, MYF(0));
      return nullptr;
    }

    if (m_attributes.srs_name.str == nullptr) {
      my_error(ER_SRS_MISSING_MANDATORY_ATTRIBUTE, MYF(0), "NAME");
      return nullptr;
    }
    MYSQL_LEX_STRING srs_name_utf8 = {nullptr, 0};
    if (thd->convert_string(&srs_name_utf8, &my_charset_utf8_bin,
                            m_attributes.srs_name.str,
                            m_attributes.srs_name.length, thd->charset())) {
      /* purecov: begin inspected */
      my_error(ER_OOM, MYF(0));
      return nullptr;
      /* purecov: end */
    }
    if (srs_name_utf8.length == 0 || std::isspace(srs_name_utf8.str[0]) ||
        std::isspace(srs_name_utf8.str[srs_name_utf8.length - 1])) {
      my_error(ER_SRS_NAME_CANT_BE_EMPTY_OR_WHITESPACE, MYF(0));
      return nullptr;
    }
    if (contains_control_char(srs_name_utf8.str, srs_name_utf8.length)) {
      my_error(ER_SRS_INVALID_CHARACTER_IN_ATTRIBUTE, MYF(0), "NAME");
      return nullptr;
    }
    String srs_name_str(srs_name_utf8.str, srs_name_utf8.length,
                        &my_charset_utf8_bin);
    if (srs_name_str.numchars() > 80) {
      my_error(ER_SRS_ATTRIBUTE_STRING_TOO_LONG, MYF(0), "NAME", 80);
      return nullptr;
    }

    if (m_attributes.definition.str == nullptr) {
      my_error(ER_SRS_MISSING_MANDATORY_ATTRIBUTE, MYF(0), "DEFINITION");
      return nullptr;
    }
    MYSQL_LEX_STRING definition_utf8 = {nullptr, 0};
    if (thd->convert_string(&definition_utf8, &my_charset_utf8_bin,
                            m_attributes.definition.str,
                            m_attributes.definition.length, thd->charset())) {
      /* purecov: begin inspected */
      my_error(ER_OOM, MYF(0));
      return nullptr;
      /* purecov: end */
    }
    String definition_str(definition_utf8.str, definition_utf8.length,
                          &my_charset_utf8_bin);
    if (contains_control_char(definition_utf8.str, definition_utf8.length)) {
      my_error(ER_SRS_INVALID_CHARACTER_IN_ATTRIBUTE, MYF(0), "DEFINITION");
      return nullptr;
    }
    if (definition_str.numchars() > 4096) {
      my_error(ER_SRS_ATTRIBUTE_STRING_TOO_LONG, MYF(0), "DEFINITION", 4096);
      return nullptr;
    }

    MYSQL_LEX_STRING organization_utf8 = {nullptr, 0};
    if (m_attributes.organization.str != nullptr) {
      if (thd->convert_string(&organization_utf8, &my_charset_utf8_bin,
                              m_attributes.organization.str,
                              m_attributes.organization.length,
                              thd->charset())) {
        /* purecov: begin inspected */
        my_error(ER_OOM, MYF(0));
        return nullptr;
        /* purecov: end */
      }
      if (organization_utf8.length == 0 ||
          std::isspace(organization_utf8.str[0]) ||
          std::isspace(organization_utf8.str[organization_utf8.length - 1])) {
        my_error(ER_SRS_ORGANIZATION_CANT_BE_EMPTY_OR_WHITESPACE, MYF(0));
        return nullptr;
      }
      String organization_str(organization_utf8.str, organization_utf8.length,
                              &my_charset_utf8_bin);
      if (contains_control_char(organization_utf8.str,
                                organization_utf8.length)) {
        my_error(ER_SRS_INVALID_CHARACTER_IN_ATTRIBUTE, MYF(0), "ORGANIZATION");
        return nullptr;
      }
      if (organization_str.numchars() > 256) {
        my_error(ER_SRS_ATTRIBUTE_STRING_TOO_LONG, MYF(0), "ORGANIZATION", 256);
        return nullptr;
      }

      if (m_attributes.organization_coordsys_id >
          std::numeric_limits<gis::srid_t>::max()) {
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "IDENTIFIED BY",
                 m_or_replace ? "CREATE OR REPLACE SPATIAL REFERENCE SYSTEM"
                              : "CREATE SPATIAL REFERENCE SYSTEM");
        return nullptr;
      }
    }

    MYSQL_LEX_STRING description_utf8 = {nullptr, 0};
    if (m_attributes.description.str != nullptr) {
      if (thd->convert_string(&description_utf8, &my_charset_utf8_bin,
                              m_attributes.description.str,
                              m_attributes.description.length,
                              thd->charset())) {
        /* purecov: begin inspected */
        my_error(ER_OOM, MYF(0));
        return nullptr;
        /* purecov: end */
      }
      String description_str(description_utf8.str, description_utf8.length,
                             &my_charset_utf8_bin);
      if (contains_control_char(description_utf8.str,
                                description_utf8.length)) {
        my_error(ER_SRS_INVALID_CHARACTER_IN_ATTRIBUTE, MYF(0), "DESCRIPTION");
        return nullptr;
      }
      if (description_str.numchars() > 2048) {
        my_error(ER_SRS_ATTRIBUTE_STRING_TOO_LONG, MYF(0), "DESCRIPTION", 2048);
        return nullptr;
      }
    }

    sql_cmd.init(m_or_replace, m_if_not_exists, m_srid, srs_name_utf8,
                 definition_utf8, organization_utf8,
                 m_attributes.organization_coordsys_id, description_utf8);
    return &sql_cmd;
  }
};

/**
  Top-level node for the DROP SPATIAL REFERENCE SYSTEM statement.

  @ingroup ptn_stmt
*/
class PT_drop_srs final : public Parse_tree_root {
  /// The SQL command object.
  Sql_cmd_drop_srs sql_cmd;
  /// SRID of the SRS to drop.
  ///
  /// The range is larger than that of gis::srid_t, so it must be
  /// verified to be less than the uint32 maximum value.
  unsigned long long m_srid;

 public:
  PT_drop_srs(unsigned long long srid, bool if_exists)
      : sql_cmd(srid, if_exists), m_srid(srid) {}

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_DROP_SRS;

    if (m_srid > std::numeric_limits<gis::srid_t>::max()) {
      my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "SRID",
               "DROP SPATIAL REFERENCE SYSTEM");
      return nullptr;
    }
    if (m_srid == 0) {
      my_error(ER_CANT_MODIFY_SRID_0, MYF(0));
      return nullptr;
    }

    return &sql_cmd;
  }
};

/**
  Top-level node for the ALTER INSTANCE statement

  @ingroup ptn_stmt
*/
class PT_alter_instance final : public Parse_tree_root {
  Sql_cmd_alter_instance sql_cmd;

 public:
  explicit PT_alter_instance(
      enum alter_instance_action_enum alter_instance_action)
      : sql_cmd(alter_instance_action) {}

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->no_write_to_binlog = false;
    return &sql_cmd;
  }
};

/**
  A template-free base class for index options that we can predeclare in
  sql_lex.h
*/
class PT_base_index_option : public Table_ddl_node {};

/**
  A key part specification.

  This can either be a "normal" key part (a key part that points to a column),
  or this can be a functional key part (a key part that points to an
  expression).
*/
class PT_key_part_specification : public Parse_tree_node {
  typedef Parse_tree_node super;

 public:
  /**
    Constructor for a functional key part.

    @param expression The expression to index.
    @param order The direction of the index.
  */
  PT_key_part_specification(Item *expression, enum_order order);

  /**
    Constructor for a "normal" key part. That is a key part that points to a
    column and not an expression.

    @param column_name The column name that this key part points to.
    @param order The direction of the index.
    @param prefix_length How many bytes or characters this key part should
           index, or zero if it should index the entire column.
  */
  PT_key_part_specification(const LEX_CSTRING &column_name, enum_order order,
                            int prefix_length);

  /**
    Contextualize this key part specification. This will also call itemize on
    the indexed expression if this is a functional key part.

    @param pc The parse context

    @retval true on error
    @retval false on success
  */
  bool contextualize(Parse_context *pc) override;

  /**
    Get the indexed expression. The caller must ensure that has_expression()
    returns true before calling this.

    @returns The indexed expression
  */
  Item *get_expression() const {
    DBUG_ASSERT(has_expression());
    return m_expression;
  }

  /**
    @returns The direction of the index: ORDER_ASC, ORDER_DESC or
             ORDER_NOT_RELEVANT in case the user didn't explicitly specify a
             direction.
  */
  enum_order get_order() const { return m_order; }

  /**
    @retval true if the user explicitly specified a direction (asc/desc).
    @retval false if the user didn't explicitly specify a direction.
  */
  bool is_explicit() const { return get_order() != ORDER_NOT_RELEVANT; }

  /**
    @retval true if the key part contains an expression (and thus is a
            functional key part).
    @retval false if the key part doesn't contain an expression.
  */
  bool has_expression() const { return m_expression != nullptr; }

  /**
    Get the column that this key part points to. This is only valid if this
    key part isn't a functional index. The caller must thus check the return
    value of has_expression() before calling this function.

    @returns The column that this key part points to.
  */
  LEX_CSTRING get_column_name() const {
    DBUG_ASSERT(!has_expression());
    return m_column_name;
  }

  /**
    @returns The number of bytes that this key part should index. If the column
             this key part points to is a non-binary column, this is the number
             of characters. Returns zero if the entire column should be indexed.
  */
  int get_prefix_length() const { return m_prefix_length; }

 private:
  /**
    The indexed expression in case this is a functional key part. Only valid if
    has_expression() returns true.
  */
  Item *m_expression;

  /// The direction of the index.
  enum_order m_order;

  /// The name of the column that this key part indexes.
  LEX_CSTRING m_column_name;

  /**
    If this is greater than zero, it represents how many bytes of the column
    that is indexed. Note that for non-binary columns (VARCHAR, TEXT etc), this
    is the number of characters.
  */
  int m_prefix_length;
};

/**
  A template for options that set a single `<alter option>` value in
  thd->lex->key_create_info.

  @tparam Option_type The data type of the option.
  @tparam Property Pointer-to-member for the option of KEY_CREATE_INFO.
*/
template <typename Option_type, Option_type KEY_CREATE_INFO::*Property>
class PT_index_option : public PT_base_index_option {
 public:
  /// @param option_value The value of the option.
  PT_index_option(Option_type option_value) : m_option_value(option_value) {}

  bool contextualize(Table_ddl_parse_context *pc) {
    pc->key_create_info->*Property = m_option_value;
    return false;
  }

 private:
  Option_type m_option_value;
};

/**
  A template for options that set a single property in a KEY_CREATE_INFO, and
  also records if the option was explicitly set.
*/
template <typename Option_type, Option_type KEY_CREATE_INFO::*Property,
          bool KEY_CREATE_INFO::*Property_is_explicit>
class PT_traceable_index_option : public PT_base_index_option {
 public:
  PT_traceable_index_option(Option_type option_value)
      : m_option_value(option_value) {}

  bool contextualize(Table_ddl_parse_context *pc) {
    pc->key_create_info->*Property = m_option_value;
    pc->key_create_info->*Property_is_explicit = true;
    return false;
  }

 private:
  Option_type m_option_value;
};

typedef Mem_root_array_YY<PT_base_index_option *> Index_options;
typedef PT_index_option<ulong, &KEY_CREATE_INFO::block_size> PT_block_size;
typedef PT_index_option<LEX_CSTRING, &KEY_CREATE_INFO::comment>
    PT_index_comment;
typedef PT_index_option<LEX_CSTRING, &KEY_CREATE_INFO::parser_name>
    PT_fulltext_index_parser_name;
typedef PT_index_option<bool, &KEY_CREATE_INFO::is_visible> PT_index_visibility;

/**
  The data structure (B-tree, Hash, etc) used for an index is called
  'index_type' in the manual. Internally, this is stored in
  KEY_CREATE_INFO::algorithm, while what the manual calls 'algorithm' is
  stored in partition_info::key_algorithm. In an `<create_index_stmt>`
  it's ignored. The terminology is somewhat confusing, but we stick to the
  manual in the parser.
*/
typedef PT_traceable_index_option<ha_key_alg, &KEY_CREATE_INFO::algorithm,
                                  &KEY_CREATE_INFO::is_algorithm_explicit>
    PT_index_type;

class PT_create_index_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_create_index_stmt(MEM_ROOT *mem_root, keytype type_par,
                       const LEX_STRING &name_arg, PT_base_index_option *type,
                       Table_ident *table_ident,
                       List<PT_key_part_specification> *cols,
                       Index_options options,
                       Alter_info::enum_alter_table_algorithm algo,
                       Alter_info::enum_alter_table_lock lock)
      : PT_table_ddl_stmt_base(mem_root),
        m_keytype(type_par),
        m_name(name_arg),
        m_type(type),
        m_table_ident(table_ident),
        m_columns(cols),
        m_options(options),
        m_algo(algo),
        m_lock(lock) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  keytype m_keytype;
  LEX_STRING m_name;
  PT_base_index_option *m_type;
  Table_ident *m_table_ident;
  List<PT_key_part_specification> *m_columns;
  Index_options m_options;
  const Alter_info::enum_alter_table_algorithm m_algo;
  const Alter_info::enum_alter_table_lock m_lock;
};

/**
  Base class for column/constraint definitions in CREATE %TABLE

  @ingroup ptn_create_table_stuff
*/
class PT_table_element : public Table_ddl_node {};

class PT_table_constraint_def : public PT_table_element {};

class PT_inline_index_definition : public PT_table_constraint_def {
  typedef PT_table_constraint_def super;

 public:
  PT_inline_index_definition(keytype type_par, const LEX_STRING &name_arg,
                             PT_base_index_option *type,
                             List<PT_key_part_specification> *cols,
                             Index_options options)
      : m_keytype(type_par),
        m_name(name_arg),
        m_type(type),
        m_columns(cols),
        m_options(options) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

 private:
  keytype m_keytype;
  const LEX_STRING m_name;
  PT_base_index_option *m_type;
  List<PT_key_part_specification> *m_columns;
  Index_options m_options;
};

class PT_foreign_key_definition : public PT_table_constraint_def {
  typedef PT_table_constraint_def super;

 public:
  PT_foreign_key_definition(const LEX_STRING &constraint_name,
                            const LEX_STRING &key_name,
                            List<PT_key_part_specification> *columns,
                            Table_ident *referenced_table,
                            List<Key_part_spec> *ref_list,
                            fk_match_opt fk_match_option,
                            fk_option fk_update_opt, fk_option fk_delete_opt)
      : m_constraint_name(constraint_name),
        m_key_name(key_name),
        m_columns(columns),
        m_referenced_table(referenced_table),
        m_ref_list(ref_list),
        m_fk_match_option(fk_match_option),
        m_fk_update_opt(fk_update_opt),
        m_fk_delete_opt(fk_delete_opt) {}

  bool contextualize(Table_ddl_parse_context *pc);

 private:
  const LEX_STRING m_constraint_name;
  const LEX_STRING m_key_name;
  List<PT_key_part_specification> *m_columns;
  Table_ident *m_referenced_table;
  List<Key_part_spec> *m_ref_list;
  fk_match_opt m_fk_match_option;
  fk_option m_fk_update_opt;
  fk_option m_fk_delete_opt;
};

/**
  Common base class for CREATE TABLE and ALTER TABLE option nodes

  @ingroup ptn_create_or_alter_table_options
*/
class PT_ddl_table_option : public Table_ddl_node {
 public:
  ~PT_ddl_table_option() override = 0;  // Force abstract class declaration

  virtual bool is_rename_table() const { return false; }
};

inline PT_ddl_table_option::~PT_ddl_table_option() {}

/**
  Base class for CREATE TABLE option nodes

  @ingroup ptn_create_or_alter_table_options
*/
class PT_create_table_option : public PT_ddl_table_option {
  typedef PT_ddl_table_option super;

 public:
  ~PT_create_table_option() override = 0;  // Force abstract class declaration

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->alter_info->flags |= Alter_info::ALTER_OPTIONS;
    return false;
  }
};

inline PT_create_table_option::~PT_create_table_option() {}

/**
  A template for options that set a single property in HA_CREATE_INFO, and
  also records if the option was explicitly set.
*/
template <typename Option_type, Option_type HA_CREATE_INFO::*Property,
          ulong Property_flag>
class PT_traceable_create_table_option : public PT_create_table_option {
  typedef PT_create_table_option super;

  const Option_type value;

 public:
  explicit PT_traceable_create_table_option(Option_type value) : value(value) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->create_info->*Property = value;
    pc->create_info->used_fields |= Property_flag;
    return false;
  }
};

#define TYPE_AND_REF(x) decltype(x), &x

/**
  Node for the @SQL{MAX_ROWS [=] @B{@<integer@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
typedef PT_traceable_create_table_option<TYPE_AND_REF(HA_CREATE_INFO::max_rows),
                                         HA_CREATE_USED_MAX_ROWS>
    PT_create_max_rows_option;

/**
  Node for the @SQL{MIN_ROWS [=] @B{@<integer@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
typedef PT_traceable_create_table_option<TYPE_AND_REF(HA_CREATE_INFO::min_rows),
                                         HA_CREATE_USED_MIN_ROWS>
    PT_create_min_rows_option;

/**
  Node for the @SQL{AVG_ROW_LENGTH_ROWS [=] @B{@<integer@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::avg_row_length), HA_CREATE_USED_AVG_ROW_LENGTH>
    PT_create_avg_row_length_option;

/**
  Node for the @SQL{PASSWORD [=] @B{@<string@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
typedef PT_traceable_create_table_option<TYPE_AND_REF(HA_CREATE_INFO::password),
                                         HA_CREATE_USED_PASSWORD>
    PT_create_password_option;

/**
  Node for the @SQL{COMMENT [=] @B{@<string@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
typedef PT_traceable_create_table_option<TYPE_AND_REF(HA_CREATE_INFO::comment),
                                         HA_CREATE_USED_COMMENT>
    PT_create_commen_option;

/**
  Node for the @SQL{COMPRESSION [=] @B{@<string@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
typedef PT_traceable_create_table_option<TYPE_AND_REF(HA_CREATE_INFO::compress),
                                         HA_CREATE_USED_COMPRESS>
    PT_create_compress_option;

/**
  Node for the @SQL{ENGRYPTION [=] @B{@<string@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::encrypt_type), HA_CREATE_USED_ENCRYPT>
    PT_create_encryption_option;

/**
  Node for the @SQL{AUTO_INCREMENT [=] @B{@<integer@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::auto_increment_value), HA_CREATE_USED_AUTO>
    PT_create_auto_increment_option;

typedef PT_traceable_create_table_option<TYPE_AND_REF(HA_CREATE_INFO::row_type),
                                         HA_CREATE_USED_ROW_FORMAT>
    PT_create_row_format_option;

typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::merge_insert_method),
    HA_CREATE_USED_INSERT_METHOD>
    PT_create_insert_method_option;

typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::data_file_name), HA_CREATE_USED_DATADIR>
    PT_create_data_directory_option;

typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::index_file_name), HA_CREATE_USED_INDEXDIR>
    PT_create_index_directory_option;

typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::tablespace), HA_CREATE_USED_TABLESPACE>
    PT_create_tablespace_option;

typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::connect_string), HA_CREATE_USED_CONNECTION>
    PT_create_connection_option;

typedef PT_traceable_create_table_option<
    TYPE_AND_REF(HA_CREATE_INFO::key_block_size), HA_CREATE_USED_KEY_BLOCK_SIZE>
    PT_create_key_block_size_option;

typedef decltype(HA_CREATE_INFO::table_options) table_options_t;

/**
  A template for options that set HA_CREATE_INFO::table_options and
  also records if the option was explicitly set.
*/
template <ulong Property_flag, table_options_t Default, table_options_t Yes,
          table_options_t No>
class PT_ternary_create_table_option : public PT_create_table_option {
  typedef PT_create_table_option super;

  const Ternary_option value;

 public:
  explicit PT_ternary_create_table_option(Ternary_option value)
      : value(value) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->create_info->table_options &= ~(Yes | No);
    switch (value) {
      case Ternary_option::ON:
        pc->create_info->table_options |= Yes;
        break;
      case Ternary_option::OFF:
        pc->create_info->table_options |= No;
        break;
      case Ternary_option::DEFAULT:
        break;
      default:
        DBUG_ASSERT(false);
    }
    pc->create_info->used_fields |= Property_flag;
    return false;
  }
};

/**
  Node for the @SQL{PACK_KEYS [=] @B{1|0|DEFAULT}} table option

  @ingroup ptn_create_or_alter_table_options

  PACK_KEYS | Constructor parameter
  ----------|----------------------
  1         | Ternary_option::ON
  0         | Ternary_option::OFF
  DEFAULT   | Ternary_option::DEFAULT
*/
typedef PT_ternary_create_table_option<HA_CREATE_USED_PACK_KEYS,  // flag
                                       0,                         // DEFAULT
                                       HA_OPTION_PACK_KEYS,       // ON
                                       HA_OPTION_NO_PACK_KEYS>    // OFF
    PT_create_pack_keys_option;

/**
  Node for the @SQL{STATS_PERSISTENT [=] @B{1|0|DEFAULT}} table option

  @ingroup ptn_create_or_alter_table_options

  STATS_PERSISTENT | Constructor parameter
  -----------------|----------------------
  1                | Ternary_option::ON
  0                | Ternary_option::OFF
  DEFAULT          | Ternary_option::DEFAULT
*/
typedef PT_ternary_create_table_option<HA_CREATE_USED_STATS_PERSISTENT,  // flag
                                       0,                           // DEFAULT
                                       HA_OPTION_STATS_PERSISTENT,  // ON
                                       HA_OPTION_NO_STATS_PERSISTENT>  // OFF
    PT_create_stats_persistent_option;

/**
  A template for options that set HA_CREATE_INFO::table_options and
  also records if the option was explicitly set.
*/
template <ulong Property_flag, table_options_t Yes, table_options_t No>
class PT_bool_create_table_option : public PT_create_table_option {
  typedef PT_create_table_option super;

  const bool value;

 public:
  explicit PT_bool_create_table_option(bool value) : value(value) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->create_info->table_options &= ~(Yes | No);
    pc->create_info->table_options |= value ? Yes : No;
    pc->create_info->used_fields |= Property_flag;
    return false;
  }
};

/**
  Node for the @SQL{CHECKSUM|TABLE_CHECKSUM [=] @B{0|@<not 0@>}} table option

  @ingroup ptn_create_or_alter_table_options

  TABLE_CHECKSUM | Constructor parameter
  ---------------|----------------------
  0              | false
  not 0          | true
*/
typedef PT_bool_create_table_option<HA_CREATE_USED_CHECKSUM,  // flag
                                    HA_OPTION_CHECKSUM,       // ON
                                    HA_OPTION_NO_CHECKSUM     // OFF
                                    >
    PT_create_checksum_option;

/**
  Node for the @SQL{DELAY_KEY_WRITE [=] @B{0|@<not 0@>}} table option

  @ingroup ptn_create_or_alter_table_options

  TABLE_CHECKSUM | Constructor parameter
  ---------------|----------------------
  0              | false
  not 0          | true
*/
typedef PT_bool_create_table_option<HA_CREATE_USED_DELAY_KEY_WRITE,  // flag
                                    HA_OPTION_DELAY_KEY_WRITE,       // ON
                                    HA_OPTION_NO_DELAY_KEY_WRITE>    // OFF
    PT_create_delay_key_write_option;

/**
  Node for the @SQL{ENGINE [=] @B{@<identifier@>|@<string@>}} table option

  @ingroup ptn_create_or_alter_table_options
*/
class PT_create_table_engine_option : public PT_create_table_option {
  typedef PT_create_table_option super;

  const LEX_STRING engine;

 public:
  /**
    @param engine       Storage engine name.
  */
  explicit PT_create_table_engine_option(const LEX_STRING &engine)
      : engine(engine) {}

  bool contextualize(Table_ddl_parse_context *pc) override;
};

/**
  Node for the @SQL{SECONDARY_ENGINE [=] @B{@<identifier@>|@<string@>|NULL}}
  table option.

  @ingroup ptn_create_or_alter_table_options
*/
class PT_create_table_secondary_engine_option : public PT_create_table_option {
  using super = PT_create_table_option;

 public:
  explicit PT_create_table_secondary_engine_option() {}
  explicit PT_create_table_secondary_engine_option(
      const LEX_STRING &secondary_engine)
      : m_secondary_engine(secondary_engine) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

 private:
  const LEX_STRING m_secondary_engine{nullptr, 0};
};

/**
  Node for the @SQL{STATS_AUTO_RECALC [=] @B{@<0|1|DEFAULT@>})} table option

  @ingroup ptn_create_or_alter_table_options
*/
class PT_create_stats_auto_recalc_option : public PT_create_table_option {
  typedef PT_create_table_option super;

  const Ternary_option value;

 public:
  /**
    @param value
      STATS_AUTO_RECALC | value
      ------------------|----------------------
      1                 | Ternary_option::ON
      0                 | Ternary_option::OFF
      DEFAULT           | Ternary_option::DEFAULT
  */
  PT_create_stats_auto_recalc_option(Ternary_option value) : value(value) {}

  bool contextualize(Table_ddl_parse_context *pc) override;
};

/**
  Node for the @SQL{STATS_SAMPLE_PAGES [=] @B{@<integer@>|DEFAULT}} table option

  @ingroup ptn_create_or_alter_table_options
*/
class PT_create_stats_stable_pages : public PT_create_table_option {
  typedef PT_create_table_option super;
  typedef decltype(HA_CREATE_INFO::stats_sample_pages) value_t;

  const value_t value;

 public:
  /**
    Constructor for implicit number of pages

    @param value       Nunber of pages, 1@<=N@<=65535.
  */
  explicit PT_create_stats_stable_pages(value_t value) : value(value) {
    DBUG_ASSERT(value != 0 && value <= 0xFFFF);
  }
  /**
    Constructor for the DEFAULT number of pages
  */
  PT_create_stats_stable_pages() : value(0) {}  // DEFAULT

  bool contextualize(Table_ddl_parse_context *pc) override;
};

class PT_create_union_option : public PT_create_table_option {
  typedef PT_create_table_option super;

  const Mem_root_array<Table_ident *> *tables;

 public:
  explicit PT_create_union_option(const Mem_root_array<Table_ident *> *tables)
      : tables(tables) {}

  bool contextualize(Table_ddl_parse_context *pc) override;
};

class PT_create_storage_option : public PT_create_table_option {
  typedef PT_create_table_option super;

  const ha_storage_media value;

 public:
  explicit PT_create_storage_option(ha_storage_media value) : value(value) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->create_info->storage_media = value;
    return false;
  }
};

class PT_create_table_default_charset : public PT_create_table_option {
  typedef PT_create_table_option super;

  const CHARSET_INFO *value;

 public:
  explicit PT_create_table_default_charset(const CHARSET_INFO *value)
      : value(value) {
    DBUG_ASSERT(value != nullptr);
  }

  bool contextualize(Table_ddl_parse_context *pc) override;
};

class PT_create_table_default_collation : public PT_create_table_option {
  typedef PT_create_table_option super;

  const CHARSET_INFO *value;

 public:
  explicit PT_create_table_default_collation(const CHARSET_INFO *value)
      : value(value) {
    DBUG_ASSERT(value != nullptr);
  }

  bool contextualize(Table_ddl_parse_context *pc) override;
};

class PT_check_constraint : public PT_table_constraint_def {
  typedef PT_table_constraint_def super;

  Item *expr;

 public:
  explicit PT_check_constraint(Item *expr) : expr(expr) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    return super::contextualize(pc) && expr->itemize(pc, &expr);
  }
};

class PT_column_def : public PT_table_element {
  typedef PT_table_element super;

  const LEX_STRING field_ident;
  PT_field_def_base *field_def;

  /// Currently we ignore that constraint in the executor.
  PT_table_constraint_def *opt_column_constraint;

  const char *opt_place;

 public:
  PT_column_def(const LEX_STRING &field_ident, PT_field_def_base *field_def,
                PT_table_constraint_def *opt_column_constraint,
                const char *opt_place = NULL)
      : field_ident(field_ident),
        field_def(field_def),
        opt_column_constraint(opt_column_constraint),
        opt_place(opt_place) {}

  bool contextualize(Table_ddl_parse_context *pc) override;
};

/**
  Top-level node for the CREATE %TABLE statement

  @ingroup ptn_create_table
*/
class PT_create_table_stmt final : public PT_table_ddl_stmt_base {
  bool is_temporary;
  bool only_if_not_exists;
  Table_ident *table_name;
  const Mem_root_array<PT_table_element *> *opt_table_element_list;
  const Mem_root_array<PT_create_table_option *> *opt_create_table_options;
  PT_partition *opt_partitioning;
  On_duplicate on_duplicate;
  PT_query_expression *opt_query_expression;
  Table_ident *opt_like_clause;

  HA_CREATE_INFO m_create_info;

 public:
  /**
    @param mem_root                   MEM_ROOT to use for allocation
    @param is_temporary               True if @SQL{CREATE @B{TEMPORARY} %TABLE}
    @param only_if_not_exists  True if @SQL{CREATE %TABLE ... @B{IF NOT EXISTS}}
    @param table_name                 @SQL{CREATE %TABLE ... @B{@<table name@>}}
    @param opt_table_element_list     NULL or a list of table column and
                                      constraint definitions.
    @param opt_create_table_options   NULL or a list of
                                      @ref ptn_create_or_alter_table_options
                                      "table options".
    @param opt_partitioning           NULL or the @SQL{PARTITION BY} clause.
    @param on_duplicate               DUPLICATE, IGNORE or fail with an error
                                      on data duplication errors (relevant
                                      for @SQL{CREATE TABLE ... SELECT}
                                      statements).
    @param opt_query_expression       NULL or the @SQL{@B{SELECT}} clause.
  */
  PT_create_table_stmt(
      MEM_ROOT *mem_root, bool is_temporary, bool only_if_not_exists,
      Table_ident *table_name,
      const Mem_root_array<PT_table_element *> *opt_table_element_list,
      const Mem_root_array<PT_create_table_option *> *opt_create_table_options,
      PT_partition *opt_partitioning, On_duplicate on_duplicate,
      PT_query_expression *opt_query_expression)
      : PT_table_ddl_stmt_base(mem_root),
        is_temporary(is_temporary),
        only_if_not_exists(only_if_not_exists),
        table_name(table_name),
        opt_table_element_list(opt_table_element_list),
        opt_create_table_options(opt_create_table_options),
        opt_partitioning(opt_partitioning),
        on_duplicate(on_duplicate),
        opt_query_expression(opt_query_expression),
        opt_like_clause(NULL) {}
  /**
    @param mem_root           MEM_ROOT to use for allocation
    @param is_temporary       True if @SQL{CREATE @B{TEMPORARY} %TABLE}.
    @param only_if_not_exists True if @SQL{CREATE %TABLE ... @B{IF NOT EXISTS}}.
    @param table_name         @SQL{CREATE %TABLE ... @B{@<table name@>}}.
    @param opt_like_clause    NULL or the @SQL{@B{LIKE @<table name@>}} clause.
  */
  PT_create_table_stmt(MEM_ROOT *mem_root, bool is_temporary,
                       bool only_if_not_exists, Table_ident *table_name,
                       Table_ident *opt_like_clause)
      : PT_table_ddl_stmt_base(mem_root),
        is_temporary(is_temporary),
        only_if_not_exists(only_if_not_exists),
        table_name(table_name),
        opt_table_element_list(NULL),
        opt_create_table_options(NULL),
        opt_partitioning(NULL),
        on_duplicate(On_duplicate::ERROR),
        opt_query_expression(NULL),
        opt_like_clause(opt_like_clause) {}

  Sql_cmd *make_cmd(THD *thd) override;
};

class PT_create_role final : public Parse_tree_root {
  Sql_cmd_create_role sql_cmd;

 public:
  PT_create_role(bool if_not_exists, const List<LEX_USER> *roles)
      : sql_cmd(if_not_exists, roles) {}

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_CREATE_ROLE;
    return &sql_cmd;
  }
};

class PT_drop_role final : public Parse_tree_root {
  Sql_cmd_drop_role sql_cmd;

 public:
  explicit PT_drop_role(bool ignore_errors, const List<LEX_USER> *roles)
      : sql_cmd(ignore_errors, roles) {}

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_DROP_ROLE;
    return &sql_cmd;
  }
};

class PT_set_role : public Parse_tree_root {
  Sql_cmd_set_role sql_cmd;

 public:
  explicit PT_set_role(role_enum role_type,
                       const List<LEX_USER> *opt_except_roles = NULL)
      : sql_cmd(role_type, opt_except_roles) {
    DBUG_ASSERT(role_type == role_enum::ROLE_ALL || opt_except_roles == NULL);
  }
  explicit PT_set_role(const List<LEX_USER> *roles) : sql_cmd(roles) {}

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_SET_ROLE;
    return &sql_cmd;
  }
};

/**
  This class is used for representing both static and dynamic privileges on
  global as well as table and column level.
*/
struct Privilege {
  enum privilege_type { STATIC, DYNAMIC };

  privilege_type type;
  const Mem_root_array<LEX_CSTRING> *columns;

  explicit Privilege(privilege_type type,
                     const Mem_root_array<LEX_CSTRING> *columns)
      : type(type), columns(columns) {}
};

struct Static_privilege : public Privilege {
  const uint grant;

  Static_privilege(uint grant, const Mem_root_array<LEX_CSTRING> *columns)
      : Privilege(STATIC, columns), grant(grant) {}
};

struct Dynamic_privilege : public Privilege {
  const LEX_STRING ident;

  Dynamic_privilege(const LEX_STRING &ident,
                    const Mem_root_array<LEX_CSTRING> *columns)
      : Privilege(DYNAMIC, columns), ident(ident) {}
};

class PT_role_or_privilege : public Parse_tree_node {
 protected:
  POS pos;

 public:
  explicit PT_role_or_privilege(const POS &pos) : pos(pos) {}

  virtual LEX_USER *get_user(THD *thd) {
    thd->syntax_error_at(pos, "Illegal authorization identifier");
    return NULL;
  }
  virtual Privilege *get_privilege(THD *thd) {
    thd->syntax_error_at(pos, "Illegal privilege identifier");
    return NULL;
  }
};

class PT_role_at_host final : public PT_role_or_privilege {
  LEX_STRING role;
  LEX_STRING host;

 public:
  PT_role_at_host(const POS &pos, const LEX_STRING &role,
                  const LEX_STRING &host)
      : PT_role_or_privilege(pos), role(role), host(host) {}

  LEX_USER *get_user(THD *thd) override {
    return LEX_USER::alloc(thd, &role, &host);
  }
};

class PT_role_or_dynamic_privilege final : public PT_role_or_privilege {
  LEX_STRING ident;

 public:
  PT_role_or_dynamic_privilege(const POS &pos, const LEX_STRING &ident)
      : PT_role_or_privilege(pos), ident(ident) {}

  LEX_USER *get_user(THD *thd) override {
    return LEX_USER::alloc(thd, &ident, NULL);
  }

  Privilege *get_privilege(THD *thd) override {
    return new (thd->mem_root) Dynamic_privilege(ident, NULL);
  }
};

class PT_static_privilege final : public PT_role_or_privilege {
  const uint grant;
  const Mem_root_array<LEX_CSTRING> *columns;

 public:
  PT_static_privilege(const POS &pos, uint grant,
                      const Mem_root_array<LEX_CSTRING> *columns = NULL)
      : PT_role_or_privilege(pos), grant(grant), columns(columns) {}

  Privilege *get_privilege(THD *thd) override {
    return new (thd->mem_root) Static_privilege(grant, columns);
  }
};

class PT_dynamic_privilege final : public PT_role_or_privilege {
  LEX_STRING ident;

 public:
  PT_dynamic_privilege(const POS &pos, const LEX_STRING &ident)
      : PT_role_or_privilege(pos), ident(ident) {}

  Privilege *get_privilege(THD *thd) override {
    return new (thd->mem_root) Dynamic_privilege(ident, nullptr);
  }
};

class PT_grant_roles final : public Parse_tree_root {
  const Mem_root_array<PT_role_or_privilege *> *roles;
  const List<LEX_USER> *users;
  const bool with_admin_option;

 public:
  PT_grant_roles(const Mem_root_array<PT_role_or_privilege *> *roles,
                 const List<LEX_USER> *users, bool with_admin_option)
      : roles(roles), users(users), with_admin_option(with_admin_option) {}

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_GRANT_ROLE;

    List<LEX_USER> *role_objects = new (thd->mem_root) List<LEX_USER>;
    if (role_objects == NULL) return NULL;  // OOM
    for (PT_role_or_privilege *r : *roles) {
      LEX_USER *user = r->get_user(thd);
      if (r == NULL || role_objects->push_back(user)) return NULL;
    }

    return new (thd->mem_root)
        Sql_cmd_grant_roles(role_objects, users, with_admin_option);
  }
};

class PT_revoke_roles final : public Parse_tree_root {
  const Mem_root_array<PT_role_or_privilege *> *roles;
  const List<LEX_USER> *users;

 public:
  PT_revoke_roles(Mem_root_array<PT_role_or_privilege *> *roles,
                  const List<LEX_USER> *users)
      : roles(roles), users(users) {}

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_REVOKE_ROLE;

    List<LEX_USER> *role_objects = new (thd->mem_root) List<LEX_USER>;
    if (role_objects == NULL) return NULL;  // OOM
    for (PT_role_or_privilege *r : *roles) {
      LEX_USER *user = r->get_user(thd);
      if (r == NULL || role_objects->push_back(user)) return NULL;
    }
    return new (thd->mem_root) Sql_cmd_revoke_roles(role_objects, users);
  }
};

class PT_alter_user_default_role final : public Parse_tree_root {
  Sql_cmd_alter_user_default_role sql_cmd;

 public:
  PT_alter_user_default_role(bool if_exists, const List<LEX_USER> *users,
                             const List<LEX_USER> *roles,
                             const role_enum role_type)
      : sql_cmd(if_exists, users, roles, role_type) {}

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_ALTER_USER_DEFAULT_ROLE;
    return &sql_cmd;
  }
};

class PT_show_grants final : public Parse_tree_root {
  Sql_cmd_show_grants sql_cmd;

 public:
  PT_show_grants(const LEX_USER *opt_for_user,
                 const List<LEX_USER> *opt_using_users)
      : sql_cmd(opt_for_user, opt_using_users) {
    DBUG_ASSERT(opt_using_users == NULL || opt_for_user != NULL);
  }

  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_SHOW_GRANTS;
    return &sql_cmd;
  }
};

/**
  Base class for Parse tree nodes of SHOW FIELDS/SHOW INDEX statements.
*/
class PT_show_fields_and_keys : public Parse_tree_root {
 protected:
  enum Type { SHOW_FIELDS = SQLCOM_SHOW_FIELDS, SHOW_KEYS = SQLCOM_SHOW_KEYS };

  PT_show_fields_and_keys(const POS &pos, Type type, Table_ident *table_ident,
                          const LEX_STRING &wild, Item *where_condition)
      : m_sql_cmd(static_cast<enum_sql_command>(type)),
        m_pos(pos),
        m_type(type),
        m_table_ident(table_ident),
        m_wild(wild),
        m_where_condition(where_condition) {
    DBUG_ASSERT(wild.str == nullptr || where_condition == nullptr);
  }

 public:
  Sql_cmd *make_cmd(THD *thd) override;

 private:
  // Sql_cmd for SHOW COLUMNS/SHOW INDEX statements.
  Sql_cmd_show m_sql_cmd;

  // Textual location of a token just parsed.
  POS m_pos;

  // SHOW_FIELDS or SHOW_KEYS
  Type m_type;

  // Table used in the statement.
  Table_ident *m_table_ident;

  // Wild or where clause used in the statement.
  LEX_STRING m_wild;
  Item *m_where_condition;
};

/**
  Parse tree node for SHOW FIELDS statement.
*/
class PT_show_fields final : public PT_show_fields_and_keys {
  typedef PT_show_fields_and_keys super;

 public:
  PT_show_fields(const POS &pos, Show_cmd_type show_cmd_type,
                 Table_ident *table, const LEX_STRING &wild)
      : PT_show_fields_and_keys(pos, SHOW_FIELDS, table, wild, nullptr),
        m_show_cmd_type(show_cmd_type) {}

  PT_show_fields(const POS &pos, Show_cmd_type show_cmd_type,
                 Table_ident *table_ident, Item *where_condition = nullptr)
      : PT_show_fields_and_keys(pos, SHOW_FIELDS, table_ident, NULL_STR,
                                where_condition),
        m_show_cmd_type(show_cmd_type) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Show_cmd_type m_show_cmd_type;
};

/**
  Parse tree node for SHOW INDEX statement.
*/
class PT_show_keys final : public PT_show_fields_and_keys {
 public:
  PT_show_keys(const POS &pos, bool extended_show, Table_ident *table,
               Item *where_condition)
      : PT_show_fields_and_keys(pos, SHOW_KEYS, table, NULL_STR,
                                where_condition),
        m_extended_show(extended_show) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  typedef PT_show_fields_and_keys super;

  // Flag to indicate EXTENDED keyword usage in the statement.
  bool m_extended_show;
};

class PT_alter_table_action : public PT_ddl_table_option {
  typedef PT_ddl_table_option super;

 protected:
  explicit PT_alter_table_action(Alter_info::Alter_info_flag flag)
      : flag(flag) {}

 public:
  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->alter_info->flags |= flag;
    return false;
  }

 protected:
  /**
    A routine used by the parser to decide whether we are specifying a full
    partitioning or if only partitions to add or to reorganize.

    @retval  true    ALTER TABLE ADD/REORGANIZE PARTITION.
    @retval  false   Something else.
  */
  bool is_add_or_reorganize_partition() const {
    return (flag == Alter_info::ALTER_ADD_PARTITION ||
            flag == Alter_info::ALTER_REORGANIZE_PARTITION);
  }

 public:
  const Alter_info::Alter_info_flag flag;
};

class PT_alter_table_add_column final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_add_column(const LEX_STRING &field_ident,
                            PT_field_def_base *field_def,
                            PT_table_constraint_def *opt_column_constraint,
                            const char *opt_place)
      : super(Alter_info::ALTER_ADD_COLUMN),
        m_column_def(field_ident, field_def, opt_column_constraint, opt_place) {
  }

  bool contextualize(Table_ddl_parse_context *pc) override {
    return super::contextualize(pc) || m_column_def.contextualize(pc);
  }

 private:
  PT_column_def m_column_def;
};

class PT_alter_table_add_columns final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  explicit PT_alter_table_add_columns(
      const Mem_root_array<PT_table_element *> *columns)
      : super(Alter_info::ALTER_ADD_COLUMN), m_columns(columns) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;

    for (auto *column : *m_columns)
      if (column->contextualize(pc)) return true;

    return false;
  }

 private:
  const Mem_root_array<PT_table_element *> *m_columns;
};

class PT_alter_table_add_constraint final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  explicit PT_alter_table_add_constraint(PT_table_constraint_def *constraint)
      : super(Alter_info::ALTER_ADD_INDEX), m_constraint(constraint) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    return super::contextualize(pc) || m_constraint->contextualize(pc);
  }

 private:
  PT_table_constraint_def *m_constraint;
};

class PT_alter_table_change_column final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_change_column(const LEX_STRING &old_name,
                               const LEX_STRING &new_name,
                               PT_field_def_base *field_def,
                               const char *opt_place)
      : super(Alter_info::ALTER_CHANGE_COLUMN),
        m_old_name(old_name),
        m_new_name(new_name),
        m_field_def(field_def),
        m_opt_place(opt_place) {}

  PT_alter_table_change_column(const LEX_STRING &name,
                               PT_field_def_base *field_def,
                               const char *opt_place)
      : PT_alter_table_change_column(name, name, field_def, opt_place) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

 private:
  const LEX_STRING m_old_name;
  const LEX_STRING m_new_name;
  PT_field_def_base *m_field_def;
  const char *m_opt_place;
};

class PT_alter_table_drop : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 protected:
  PT_alter_table_drop(Alter_drop::drop_type drop_type,
                      Alter_info::Alter_info_flag flag, const char *name)
      : super(flag), m_alter_drop(drop_type, name) {}

 public:
  bool contextualize(Table_ddl_parse_context *pc) override {
    return (super::contextualize(pc) ||
            pc->alter_info->drop_list.push_back(&m_alter_drop));
  }

 private:
  Alter_drop m_alter_drop;
};

class PT_alter_table_drop_column final : public PT_alter_table_drop {
 public:
  explicit PT_alter_table_drop_column(const char *name)
      : PT_alter_table_drop(Alter_drop::COLUMN, Alter_info::ALTER_DROP_COLUMN,
                            name) {}
};

class PT_alter_table_drop_foreign_key final : public PT_alter_table_drop {
 public:
  explicit PT_alter_table_drop_foreign_key(const char *name)
      : PT_alter_table_drop(Alter_drop::FOREIGN_KEY,
                            Alter_info::DROP_FOREIGN_KEY, name) {}
};

class PT_alter_table_drop_key final : public PT_alter_table_drop {
 public:
  explicit PT_alter_table_drop_key(const char *name)
      : PT_alter_table_drop(Alter_drop::KEY, Alter_info::ALTER_DROP_INDEX,
                            name) {}
};

class PT_alter_table_enable_keys final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  explicit PT_alter_table_enable_keys(bool enable)
      : super(Alter_info::ALTER_KEYS_ONOFF), m_enable(enable) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    pc->alter_info->keys_onoff =
        m_enable ? Alter_info::ENABLE : Alter_info::DISABLE;
    return super::contextualize(pc);
  }

 private:
  bool m_enable;
};

class PT_alter_table_set_default final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_set_default(const char *col_name, Item *opt_default_expr)
      : super(Alter_info::ALTER_CHANGE_COLUMN_DEFAULT),
        m_name(col_name),
        m_expr(opt_default_expr) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc) || itemize_safe(pc, &m_expr)) return true;
    Alter_column *alter_column;
    if (m_expr == nullptr || m_expr->basic_const_item()) {
      alter_column = new (pc->mem_root) Alter_column(m_name, m_expr);
    } else {
      auto vg = new (pc->mem_root) Value_generator;
      if (vg == nullptr) return true;  // OOM
      vg->expr_item = m_expr;
      vg->set_field_stored(true);
      alter_column = new (pc->mem_root) Alter_column(m_name, vg);
    }
    if (alter_column == nullptr ||
        pc->alter_info->alter_list.push_back(alter_column)) {
      return true;  // OOM
    }
    return false;
  }

 private:
  const char *m_name;
  Item *m_expr;
};

class PT_alter_table_index_visible final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_index_visible(const char *name, bool visible)
      : super(Alter_info::ALTER_INDEX_VISIBILITY),
        m_alter_index_visibility(name, visible) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    return (super::contextualize(pc) ||
            pc->alter_info->alter_index_visibility_list.push_back(
                &m_alter_index_visibility));
  }

 private:
  Alter_index_visibility m_alter_index_visibility;
};

class PT_alter_table_rename final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  explicit PT_alter_table_rename(const Table_ident *ident)
      : super(Alter_info::ALTER_RENAME), m_ident(ident) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

  bool is_rename_table() const override { return true; }

 private:
  const Table_ident *const m_ident;
};

class PT_alter_table_rename_key final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_rename_key(const char *from, const char *to)
      : super(Alter_info::ALTER_RENAME_INDEX), m_rename_key(from, to) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    return super::contextualize(pc) ||
           pc->alter_info->alter_rename_key_list.push_back(&m_rename_key);
  }

 private:
  Alter_rename_key m_rename_key;
};

class PT_alter_table_rename_column final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_rename_column(const char *from, const char *to)
      : super(Alter_info::ALTER_CHANGE_COLUMN), m_rename_column(from, to) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    return super::contextualize(pc) ||
           pc->alter_info->alter_list.push_back(&m_rename_column);
  }

 private:
  Alter_column m_rename_column;
};

class PT_alter_table_convert_to_charset final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_convert_to_charset(const CHARSET_INFO *charset,
                                    const CHARSET_INFO *opt_collation)
      : super(Alter_info::ALTER_OPTIONS),
        m_charset(charset),
        m_collation(opt_collation) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

 private:
  const CHARSET_INFO *const m_charset;
  const CHARSET_INFO *const m_collation;
};

class PT_alter_table_force final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_force() : super(Alter_info::ALTER_RECREATE) {}
};

class PT_alter_table_order final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  explicit PT_alter_table_order(PT_order_list *order)
      : super(Alter_info::ALTER_ORDER), m_order(order) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc) || m_order->contextualize(pc)) return true;
    pc->select->order_list = m_order->value;
    return false;
  }

 private:
  PT_order_list *const m_order;
};

class PT_alter_table_partition_by final : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  explicit PT_alter_table_partition_by(PT_partition *partition)
      : super(Alter_info::ALTER_PARTITION), m_partition(partition) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc) || m_partition->contextualize(pc)) return true;
    pc->thd->lex->part_info = &m_partition->part_info;
    return false;
  }

 private:
  PT_partition *const m_partition;
};

class PT_alter_table_remove_partitioning : public PT_alter_table_action {
  typedef PT_alter_table_action super;

 public:
  PT_alter_table_remove_partitioning()
      : super(Alter_info::ALTER_REMOVE_PARTITIONING) {}
};

class PT_alter_table_standalone_action : public PT_alter_table_action {
  typedef PT_alter_table_action super;

  friend class PT_alter_table_standalone_stmt;  // to access make_cmd()

 protected:
  PT_alter_table_standalone_action(Alter_info::Alter_info_flag flag)
      : super(flag) {}

 private:
  virtual Sql_cmd *make_cmd(Table_ddl_parse_context *pc) = 0;
};

/**
  Node for the @SQL{ALTER TABLE ADD PARTITION} statement

  @ingroup ptn_alter_table
*/
class PT_alter_table_add_partition : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  explicit PT_alter_table_add_partition(bool no_write_to_binlog)
      : super(Alter_info::ALTER_ADD_PARTITION),
        m_no_write_to_binlog(no_write_to_binlog) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;

    LEX *const lex = pc->thd->lex;
    lex->no_write_to_binlog = m_no_write_to_binlog;
    DBUG_ASSERT(lex->part_info == nullptr);
    lex->part_info = &m_part_info;
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override final {
    return new (pc->mem_root) Sql_cmd_alter_table(pc->alter_info);
  }

 protected:
  partition_info m_part_info;

 private:
  const bool m_no_write_to_binlog;
};

/**
  Node for the @SQL{ALTER TABLE ADD PARTITION (@<partition list@>)} statement

  @ingroup ptn_alter_table
*/
class PT_alter_table_add_partition_def_list final
    : public PT_alter_table_add_partition {
  typedef PT_alter_table_add_partition super;

 public:
  PT_alter_table_add_partition_def_list(
      bool no_write_to_binlog,
      const Mem_root_array<PT_part_definition *> *def_list)
      : super(no_write_to_binlog), m_def_list(def_list) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

 private:
  const Mem_root_array<PT_part_definition *> *m_def_list;
};

/**
  Node for the @SQL{ALTER TABLE ADD PARTITION PARTITIONS (@<n>@)} statement

  @ingroup ptn_alter_table
*/
class PT_alter_table_add_partition_num final
    : public PT_alter_table_add_partition {
  typedef PT_alter_table_add_partition super;

 public:
  PT_alter_table_add_partition_num(bool no_write_to_binlog, uint num_parts)
      : super(no_write_to_binlog) {
    m_part_info.num_parts = num_parts;
  }
};

class PT_alter_table_drop_partition final
    : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  explicit PT_alter_table_drop_partition(const List<String> &partitions)
      : super(Alter_info::ALTER_DROP_PARTITION), m_partitions(partitions) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;

    DBUG_ASSERT(pc->alter_info->partition_names.is_empty());
    pc->alter_info->partition_names = m_partitions;
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override final {
    return new (pc->mem_root) Sql_cmd_alter_table(pc->alter_info);
  }

 private:
  const List<String> m_partitions;
};

class PT_alter_table_partition_list_or_all
    : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  explicit PT_alter_table_partition_list_or_all(
      Alter_info::Alter_info_flag flag, const List<String> *opt_partition_list)
      : super(flag), m_opt_partition_list(opt_partition_list) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    DBUG_ASSERT(pc->alter_info->partition_names.is_empty());
    if (m_opt_partition_list == NULL)
      pc->alter_info->flags |= Alter_info::ALTER_ALL_PARTITION;
    else
      pc->alter_info->partition_names = *m_opt_partition_list;
    return super::contextualize(pc);
  }

 private:
  const List<String> *m_opt_partition_list;
};

class PT_alter_table_rebuild_partition final
    : public PT_alter_table_partition_list_or_all {
  typedef PT_alter_table_partition_list_or_all super;

 public:
  PT_alter_table_rebuild_partition(bool no_write_to_binlog,
                                   const List<String> *opt_partition_list)
      : super(Alter_info::ALTER_REBUILD_PARTITION, opt_partition_list),
        m_no_write_to_binlog(no_write_to_binlog) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_alter_table(pc->alter_info);
  }

 private:
  const bool m_no_write_to_binlog;
};

class PT_alter_table_optimize_partition final
    : public PT_alter_table_partition_list_or_all {
  typedef PT_alter_table_partition_list_or_all super;

 public:
  PT_alter_table_optimize_partition(bool no_write_to_binlog,
                                    const List<String> *opt_partition_list)
      : super(Alter_info::ALTER_ADMIN_PARTITION, opt_partition_list),
        m_no_write_to_binlog(no_write_to_binlog) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
    pc->thd->lex->check_opt.init();
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root)
        Sql_cmd_alter_table_optimize_partition(pc->alter_info);
  }

 private:
  const bool m_no_write_to_binlog;
};

class PT_alter_table_analyze_partition
    : public PT_alter_table_partition_list_or_all {
  typedef PT_alter_table_partition_list_or_all super;

 public:
  PT_alter_table_analyze_partition(bool no_write_to_binlog,
                                   const List<String> *opt_partition_list)
      : super(Alter_info::ALTER_ADMIN_PARTITION, opt_partition_list),
        m_no_write_to_binlog(no_write_to_binlog) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
    pc->thd->lex->check_opt.init();
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root)
        Sql_cmd_alter_table_analyze_partition(pc->thd, pc->alter_info);
  }

 private:
  const bool m_no_write_to_binlog;
};

class PT_alter_table_check_partition
    : public PT_alter_table_partition_list_or_all {
  typedef PT_alter_table_partition_list_or_all super;

 public:
  PT_alter_table_check_partition(const List<String> *opt_partition_list,
                                 uint flags, uint sql_flags)
      : super(Alter_info::ALTER_ADMIN_PARTITION, opt_partition_list),
        m_flags(flags),
        m_sql_flags(sql_flags) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;

    LEX *const lex = pc->thd->lex;
    lex->check_opt.init();
    lex->check_opt.flags |= m_flags;
    lex->check_opt.sql_flags |= m_sql_flags;
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root)
        Sql_cmd_alter_table_check_partition(pc->alter_info);
  }

 private:
  uint m_flags;
  uint m_sql_flags;
};

class PT_alter_table_repair_partition
    : public PT_alter_table_partition_list_or_all {
  typedef PT_alter_table_partition_list_or_all super;

 public:
  PT_alter_table_repair_partition(bool no_write_to_binlog,
                                  const List<String> *opt_partition_list,
                                  uint flags, uint sql_flags)
      : super(Alter_info::ALTER_ADMIN_PARTITION, opt_partition_list),
        m_no_write_to_binlog(no_write_to_binlog),
        m_flags(flags),
        m_sql_flags(sql_flags) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;

    LEX *const lex = pc->thd->lex;
    lex->no_write_to_binlog = m_no_write_to_binlog;

    lex->check_opt.init();
    lex->check_opt.flags |= m_flags;
    lex->check_opt.sql_flags |= m_sql_flags;

    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root)
        Sql_cmd_alter_table_repair_partition(pc->alter_info);
  }

 private:
  const bool m_no_write_to_binlog;
  uint m_flags;
  uint m_sql_flags;
};

class PT_alter_table_coalesce_partition final
    : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  PT_alter_table_coalesce_partition(bool no_write_to_binlog, uint num_parts)
      : super(Alter_info::ALTER_COALESCE_PARTITION),
        m_no_write_to_binlog(no_write_to_binlog),
        m_num_parts(num_parts) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;

    pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
    pc->alter_info->num_parts = m_num_parts;
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_alter_table(pc->alter_info);
  }

 private:
  const bool m_no_write_to_binlog;
  const uint m_num_parts;
};

class PT_alter_table_truncate_partition
    : public PT_alter_table_partition_list_or_all {
  typedef PT_alter_table_partition_list_or_all super;

 public:
  explicit PT_alter_table_truncate_partition(
      const List<String> *opt_partition_list)
      : super(static_cast<Alter_info::Alter_info_flag>(
                  Alter_info::ALTER_ADMIN_PARTITION |
                  Alter_info::ALTER_TRUNCATE_PARTITION),
              opt_partition_list) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->thd->lex->check_opt.init();
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root)
        Sql_cmd_alter_table_truncate_partition(pc->alter_info);
  }
};

class PT_alter_table_reorganize_partition final
    : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  explicit PT_alter_table_reorganize_partition(bool no_write_to_binlog)
      : super(Alter_info::ALTER_TABLE_REORG),
        m_no_write_to_binlog(no_write_to_binlog) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    pc->thd->lex->part_info = &m_partition_info;
    pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
    return false;
  }

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_alter_table(pc->alter_info);
  }

 private:
  const bool m_no_write_to_binlog;
  partition_info m_partition_info;
};

class PT_alter_table_reorganize_partition_into final
    : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  explicit PT_alter_table_reorganize_partition_into(
      bool no_write_to_binlog, const List<String> &partition_names,
      const Mem_root_array<PT_part_definition *> *into)
      : super(Alter_info::ALTER_REORGANIZE_PARTITION),
        m_no_write_to_binlog(no_write_to_binlog),
        m_partition_names(partition_names),
        m_into(into) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_alter_table(pc->alter_info);
  }

 private:
  const bool m_no_write_to_binlog;
  const List<String> m_partition_names;
  const Mem_root_array<PT_part_definition *> *m_into;
  partition_info m_partition_info;
};

class PT_alter_table_exchange_partition final
    : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  PT_alter_table_exchange_partition(const LEX_STRING &partition_name,
                                    Table_ident *table_name,
                                    Alter_info::enum_with_validation validation)
      : super(Alter_info::ALTER_EXCHANGE_PARTITION),
        m_partition_name(partition_name),
        m_table_name(table_name),
        m_validation(validation) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root)
        Sql_cmd_alter_table_exchange_partition(pc->alter_info);
  }

 private:
  const LEX_STRING m_partition_name;
  Table_ident *m_table_name;
  const Alter_info::enum_with_validation m_validation;
};

class PT_alter_table_secondary_load final
    : public PT_alter_table_standalone_action {
  using super = PT_alter_table_standalone_action;

 public:
  explicit PT_alter_table_secondary_load()
      : super(Alter_info::ALTER_SECONDARY_LOAD) {}

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_secondary_load_unload(pc->alter_info);
  }
};

class PT_alter_table_secondary_unload final
    : public PT_alter_table_standalone_action {
  using super = PT_alter_table_standalone_action;

 public:
  explicit PT_alter_table_secondary_unload()
      : super(Alter_info::ALTER_SECONDARY_UNLOAD) {}

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_secondary_load_unload(pc->alter_info);
  }
};

class PT_alter_table_discard_partition_tablespace final
    : public PT_alter_table_partition_list_or_all {
  typedef PT_alter_table_partition_list_or_all super;

 public:
  explicit PT_alter_table_discard_partition_tablespace(
      const List<String> *opt_partition_list)
      : super(Alter_info::ALTER_DISCARD_TABLESPACE, opt_partition_list) {}

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_discard_import_tablespace(pc->alter_info);
  }
};

class PT_alter_table_import_partition_tablespace final
    : public PT_alter_table_partition_list_or_all {
  typedef PT_alter_table_partition_list_or_all super;

 public:
  explicit PT_alter_table_import_partition_tablespace(
      const List<String> *opt_partition_list)
      : super(Alter_info::ALTER_IMPORT_TABLESPACE, opt_partition_list) {}

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_discard_import_tablespace(pc->alter_info);
  }
};

class PT_alter_table_discard_tablespace final
    : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  PT_alter_table_discard_tablespace()
      : super(Alter_info::ALTER_DISCARD_TABLESPACE) {}

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_discard_import_tablespace(pc->alter_info);
  }
};

class PT_alter_table_import_tablespace final
    : public PT_alter_table_standalone_action {
  typedef PT_alter_table_standalone_action super;

 public:
  PT_alter_table_import_tablespace()
      : super(Alter_info::ALTER_IMPORT_TABLESPACE) {}

  Sql_cmd *make_cmd(Table_ddl_parse_context *pc) override {
    return new (pc->mem_root) Sql_cmd_discard_import_tablespace(pc->alter_info);
  }
};

class PT_alter_table_stmt final : public PT_table_ddl_stmt_base {
 public:
  explicit PT_alter_table_stmt(
      MEM_ROOT *mem_root, Table_ident *table_name,
      Mem_root_array<PT_ddl_table_option *> *opt_actions,
      Alter_info::enum_alter_table_algorithm algo,
      Alter_info::enum_alter_table_lock lock,
      Alter_info::enum_with_validation validation)
      : PT_table_ddl_stmt_base(mem_root),
        m_table_name(table_name),
        m_opt_actions(opt_actions),
        m_algo(algo),
        m_lock(lock),
        m_validation(validation) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Table_ident *const m_table_name;
  Mem_root_array<PT_ddl_table_option *> *const m_opt_actions;
  const Alter_info::enum_alter_table_algorithm m_algo;
  const Alter_info::enum_alter_table_lock m_lock;
  const Alter_info::enum_with_validation m_validation;

  HA_CREATE_INFO m_create_info;
};

class PT_alter_table_standalone_stmt final : public PT_table_ddl_stmt_base {
 public:
  explicit PT_alter_table_standalone_stmt(
      MEM_ROOT *mem_root, Table_ident *table_name,
      PT_alter_table_standalone_action *action,
      Alter_info::enum_alter_table_algorithm algo,
      Alter_info::enum_alter_table_lock lock,
      Alter_info::enum_with_validation validation)
      : PT_table_ddl_stmt_base(mem_root),
        m_table_name(table_name),
        m_action(action),
        m_algo(algo),
        m_lock(lock),
        m_validation(validation) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Table_ident *const m_table_name;
  PT_alter_table_standalone_action *const m_action;
  const Alter_info::enum_alter_table_algorithm m_algo;
  const Alter_info::enum_alter_table_lock m_lock;
  const Alter_info::enum_with_validation m_validation;

  HA_CREATE_INFO m_create_info;
};

class PT_repair_table_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_repair_table_stmt(MEM_ROOT *mem_root, bool no_write_to_binlog,
                       Mem_root_array<Table_ident *> *table_list,
                       decltype(HA_CHECK_OPT::flags) flags,
                       decltype(HA_CHECK_OPT::sql_flags) sql_flags)
      : PT_table_ddl_stmt_base(mem_root),
        m_no_write_to_binlog(no_write_to_binlog),
        m_table_list(table_list),
        m_flags(flags),
        m_sql_flags(sql_flags) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  bool m_no_write_to_binlog;
  Mem_root_array<Table_ident *> *m_table_list;
  decltype(HA_CHECK_OPT::flags) m_flags;
  decltype(HA_CHECK_OPT::sql_flags) m_sql_flags;
};

class PT_analyze_table_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_analyze_table_stmt(MEM_ROOT *mem_root, bool no_write_to_binlog,
                        Mem_root_array<Table_ident *> *table_list,
                        Sql_cmd_analyze_table::Histogram_command command,
                        int num_buckets, List<String> *columns)
      : PT_table_ddl_stmt_base(mem_root),
        m_no_write_to_binlog(no_write_to_binlog),
        m_table_list(table_list),
        m_command(command),
        m_num_buckets(num_buckets),
        m_columns(columns) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  const bool m_no_write_to_binlog;
  const Mem_root_array<Table_ident *> *m_table_list;
  const Sql_cmd_analyze_table::Histogram_command m_command;
  const int m_num_buckets;
  List<String> *m_columns;
};

class PT_check_table_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_check_table_stmt(MEM_ROOT *mem_root,
                      Mem_root_array<Table_ident *> *table_list,
                      decltype(HA_CHECK_OPT::flags) flags,
                      decltype(HA_CHECK_OPT::sql_flags) sql_flags)
      : PT_table_ddl_stmt_base(mem_root),
        m_table_list(table_list),
        m_flags(flags),
        m_sql_flags(sql_flags) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Mem_root_array<Table_ident *> *m_table_list;
  decltype(HA_CHECK_OPT::flags) m_flags;
  decltype(HA_CHECK_OPT::sql_flags) m_sql_flags;
};

class PT_optimize_table_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_optimize_table_stmt(MEM_ROOT *mem_root, bool no_write_to_binlog,
                         Mem_root_array<Table_ident *> *table_list)
      : PT_table_ddl_stmt_base(mem_root),
        m_no_write_to_binlog(no_write_to_binlog),
        m_table_list(table_list) {}

  Sql_cmd *make_cmd(THD *thd) override;

  bool m_no_write_to_binlog;
  Mem_root_array<Table_ident *> *m_table_list;
};

class PT_drop_index_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_drop_index_stmt(MEM_ROOT *mem_root, const char *index_name,
                     Table_ident *table,
                     Alter_info::enum_alter_table_algorithm algo,
                     Alter_info::enum_alter_table_lock lock)
      : PT_table_ddl_stmt_base(mem_root),
        m_index_name(index_name),
        m_table(table),
        m_algo(algo),
        m_lock(lock),
        m_alter_drop(Alter_drop::KEY, m_index_name) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  const char *m_index_name;
  Table_ident *m_table;
  Alter_info::enum_alter_table_algorithm m_algo;
  Alter_info::enum_alter_table_lock m_lock;

  Alter_drop m_alter_drop;
};

class PT_truncate_table_stmt final : public Parse_tree_root {
 public:
  explicit PT_truncate_table_stmt(Table_ident *table) : m_table(table) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Table_ident *m_table;

  Sql_cmd_truncate_table m_cmd_truncate_table;
};

class PT_assign_to_keycache final : public Table_ddl_node {
  typedef Table_ddl_node super;

 public:
  PT_assign_to_keycache(Table_ident *table, List<Index_hint> *index_hints)
      : m_table(table), m_index_hints(index_hints) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

 private:
  Table_ident *m_table;
  List<Index_hint> *m_index_hints;
};

class PT_adm_partition final : public Table_ddl_node {
  typedef Table_ddl_node super;

 public:
  explicit PT_adm_partition(List<String> *opt_partitions)
      : m_opt_partitions(opt_partitions) {}

  bool contextualize(Table_ddl_parse_context *pc) override;

 private:
  List<String> *m_opt_partitions;
};

class PT_cache_index_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_cache_index_stmt(MEM_ROOT *mem_root,
                      Mem_root_array<PT_assign_to_keycache *> *tbl_index_lists,
                      const LEX_STRING &key_cache_name)
      : PT_table_ddl_stmt_base(mem_root),
        m_tbl_index_lists(tbl_index_lists),
        m_key_cache_name(key_cache_name) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Mem_root_array<PT_assign_to_keycache *> *m_tbl_index_lists;
  const LEX_STRING m_key_cache_name;
};

class PT_cache_index_partitions_stmt : public PT_table_ddl_stmt_base {
 public:
  PT_cache_index_partitions_stmt(MEM_ROOT *mem_root, Table_ident *table,
                                 PT_adm_partition *partitions,
                                 List<Index_hint> *opt_key_usage_list,
                                 const LEX_STRING &key_cache_name)
      : PT_table_ddl_stmt_base(mem_root),
        m_table(table),
        m_partitions(partitions),
        m_opt_key_usage_list(opt_key_usage_list),
        m_key_cache_name(key_cache_name) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Table_ident *m_table;
  PT_adm_partition *m_partitions;
  List<Index_hint> *m_opt_key_usage_list;
  const LEX_STRING m_key_cache_name;
};

class PT_preload_keys final : public Table_ddl_node {
  typedef Table_ddl_node super;

 public:
  PT_preload_keys(Table_ident *table, List<Index_hint> *opt_cache_key_list,
                  bool ignore_leaves)
      : m_table(table),
        m_opt_cache_key_list(opt_cache_key_list),
        m_ignore_leaves(ignore_leaves) {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (super::contextualize(pc) ||
        !pc->select->add_table_to_list(
            pc->thd, m_table, NULL,
            m_ignore_leaves ? TL_OPTION_IGNORE_LEAVES : 0, TL_READ,
            MDL_SHARED_READ, m_opt_cache_key_list))
      return true;
    return false;
  }

 private:
  Table_ident *m_table;
  List<Index_hint> *m_opt_cache_key_list;
  bool m_ignore_leaves;
};

class PT_load_index_partitions_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_load_index_partitions_stmt(MEM_ROOT *mem_root, Table_ident *table,
                                PT_adm_partition *partitions,
                                List<Index_hint> *opt_cache_key_list,
                                bool ignore_leaves)
      : PT_table_ddl_stmt_base(mem_root),
        m_table(table),
        m_partitions(partitions),
        m_opt_cache_key_list(opt_cache_key_list),
        m_ignore_leaves(ignore_leaves) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Table_ident *m_table;
  PT_adm_partition *m_partitions;
  List<Index_hint> *m_opt_cache_key_list;
  bool m_ignore_leaves;
};

class PT_load_index_stmt final : public PT_table_ddl_stmt_base {
 public:
  PT_load_index_stmt(MEM_ROOT *mem_root,
                     Mem_root_array<PT_preload_keys *> *preload_list)
      : PT_table_ddl_stmt_base(mem_root), m_preload_list(preload_list) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Mem_root_array<PT_preload_keys *> *m_preload_list;
};

/**
  Base class for Parse tree nodes of SHOW TABLES statements.
*/
class PT_show_tables : public Parse_tree_root {
 public:
  PT_show_tables(const POS &pos, Show_cmd_type show_cmd_type, char *opt_db,
                 const LEX_STRING &wild, Item *where_condition)
      : m_pos(pos),
        m_sql_cmd(SQLCOM_SHOW_TABLES),
        m_opt_db(opt_db),
        m_wild(wild),
        m_where_condition(where_condition),
        m_show_cmd_type(show_cmd_type) {
    DBUG_ASSERT(m_wild.str == nullptr || m_where_condition == nullptr);
  }

 public:
  Sql_cmd *make_cmd(THD *thd) override;

 private:
  /// Textual location of a token just parsed.
  POS m_pos;

  /// Sql_cmd for SHOW TABLES statements.
  Sql_cmd_show m_sql_cmd;

  /// Optional schema name in FROM/IN clause.
  char *m_opt_db;

  /// Wild or where clause used in the statement.
  LEX_STRING m_wild;
  Item *m_where_condition;

  Show_cmd_type m_show_cmd_type;
};

class PT_json_table_column_for_ordinality final : public PT_json_table_column {
  typedef PT_json_table_column super;

 public:
  explicit PT_json_table_column_for_ordinality(const LEX_STRING &name)
      : m_column(enum_jt_column::JTC_ORDINALITY), m_name(name.str) {}

  bool contextualize(Parse_context *pc) override {
    m_column.init_for_tmp_table(MYSQL_TYPE_LONGLONG, 10, 0, true, true, 8,
                                m_name);
    return super::contextualize(pc);
  }

  Json_table_column *get_column() override { return &m_column; }

 private:
  Json_table_column m_column;
  const char *m_name;
};

class PT_json_table_column_with_path final : public PT_json_table_column {
  typedef PT_json_table_column super;

 public:
  PT_json_table_column_with_path(const LEX_STRING &name, PT_type *type,
                                 const CHARSET_INFO *collation,
                                 enum_jt_column col_type, LEX_STRING path,
                                 enum_jtc_on on_err,
                                 const LEX_STRING &error_def,
                                 enum_jtc_on on_empty,
                                 const LEX_STRING &missing_def)
      : m_column(col_type, path, on_err, error_def, on_empty, missing_def),
        m_name(name.str),
        m_type(type),
        m_collation(collation) {}

  bool contextualize(Parse_context *pc) override;

  Json_table_column *get_column() override { return &m_column; }

 private:
  Json_table_column m_column;
  const char *m_name;
  PT_type *m_type;
  const CHARSET_INFO *m_collation;
};

class PT_json_table_column_with_nested_path final
    : public PT_json_table_column {
  typedef PT_json_table_column super;

 public:
  PT_json_table_column_with_nested_path(
      const LEX_STRING &path,
      Mem_root_array<PT_json_table_column *> *nested_cols)
      : m_path(path), m_nested_columns(nested_cols), m_column(nullptr) {}

  bool contextualize(Parse_context *pc) override;

  Json_table_column *get_column() override { return m_column; }

 private:
  const LEX_STRING m_path;
  const Mem_root_array<PT_json_table_column *> *m_nested_columns;
  Json_table_column *m_column;
};

struct Alter_tablespace_parse_context : public Tablespace_options {
  THD *const thd;
  MEM_ROOT *const mem_root;

  Alter_tablespace_parse_context(THD *thd)
      : thd(thd), mem_root(thd->mem_root) {}
};

typedef Parse_tree_node_tmpl<Alter_tablespace_parse_context>
    PT_alter_tablespace_option_base;

template <typename Option_type, Option_type Tablespace_options::*Option>
class PT_alter_tablespace_option final
    : public PT_alter_tablespace_option_base /* purecov: inspected */
{
  typedef PT_alter_tablespace_option_base super;

 public:
  explicit PT_alter_tablespace_option(Option_type value) : m_value(value) {}

  bool contextualize(Alter_tablespace_parse_context *pc) override {
    pc->*Option = m_value;
    return super::contextualize(pc);
  }

 private:
  const Option_type m_value;
};

typedef PT_alter_tablespace_option<decltype(
                                       Tablespace_options::autoextend_size),
                                   &Tablespace_options::autoextend_size>
    PT_alter_tablespace_option_autoextend_size;

typedef PT_alter_tablespace_option<decltype(Tablespace_options::extent_size),
                                   &Tablespace_options::extent_size>
    PT_alter_tablespace_option_extent_size;

typedef PT_alter_tablespace_option<decltype(Tablespace_options::initial_size),
                                   &Tablespace_options::initial_size>
    PT_alter_tablespace_option_initial_size;

typedef PT_alter_tablespace_option<decltype(Tablespace_options::max_size),
                                   &Tablespace_options::max_size>
    PT_alter_tablespace_option_max_size;

typedef PT_alter_tablespace_option<decltype(
                                       Tablespace_options::redo_buffer_size),
                                   &Tablespace_options::redo_buffer_size>
    PT_alter_tablespace_option_redo_buffer_size;

typedef PT_alter_tablespace_option<decltype(
                                       Tablespace_options::undo_buffer_size),
                                   &Tablespace_options::undo_buffer_size>
    PT_alter_tablespace_option_undo_buffer_size;

typedef PT_alter_tablespace_option<
    decltype(Tablespace_options::wait_until_completed),
    &Tablespace_options::wait_until_completed>
    PT_alter_tablespace_option_wait_until_completed;

typedef PT_alter_tablespace_option<decltype(Tablespace_options::encryption),
                                   &Tablespace_options::encryption>
    PT_alter_tablespace_option_encryption;

class PT_alter_tablespace_option_nodegroup final
    : public PT_alter_tablespace_option_base /* purecov: inspected */
{
  typedef PT_alter_tablespace_option_base super;
  typedef decltype(Tablespace_options::nodegroup_id) option_type;

 public:
  explicit PT_alter_tablespace_option_nodegroup(option_type nodegroup_id)
      : m_nodegroup_id(nodegroup_id) {}

  bool contextualize(Alter_tablespace_parse_context *pc) override {
    if (super::contextualize(pc)) return true; /* purecov: inspected */  // OOM

    if (pc->nodegroup_id != UNDEF_NODEGROUP) {
      my_error(ER_FILEGROUP_OPTION_ONLY_ONCE, MYF(0), "NODEGROUP");
      return true;
    }
    pc->nodegroup_id = m_nodegroup_id;
    return false;
  }

 private:
  const option_type m_nodegroup_id;
};

class PT_alter_tablespace_option_comment final
    : public PT_alter_tablespace_option_base /* purecov: inspected */
{
  typedef PT_alter_tablespace_option_base super;
  typedef decltype(Tablespace_options::ts_comment) option_type;

 public:
  explicit PT_alter_tablespace_option_comment(option_type comment)
      : m_comment(comment) {}

  bool contextualize(Alter_tablespace_parse_context *pc) override {
    if (super::contextualize(pc)) return true; /* purecov: inspected */  // OOM

    if (pc->ts_comment.str) {
      my_error(ER_FILEGROUP_OPTION_ONLY_ONCE, MYF(0), "COMMENT");
      return true;
    }
    pc->ts_comment = m_comment;
    return false;
  }

 private:
  const option_type m_comment;
};

class PT_alter_tablespace_option_engine final
    : public PT_alter_tablespace_option_base /* purecov: inspected */
{
  typedef PT_alter_tablespace_option_base super;
  typedef decltype(Tablespace_options::engine_name) option_type;

 public:
  explicit PT_alter_tablespace_option_engine(option_type engine_name)
      : m_engine_name(engine_name) {}

  bool contextualize(Alter_tablespace_parse_context *pc) override {
    if (super::contextualize(pc)) return true; /* purecov: inspected */  // OOM

    if (pc->engine_name.str) {
      my_error(ER_FILEGROUP_OPTION_ONLY_ONCE, MYF(0), "STORAGE ENGINE");
      return true;
    }
    pc->engine_name = m_engine_name;
    return false;
  }

 private:
  const option_type m_engine_name;
};

class PT_alter_tablespace_option_file_block_size final
    : public PT_alter_tablespace_option_base /* purecov: inspected */
{
  typedef PT_alter_tablespace_option_base super;
  typedef decltype(Tablespace_options::file_block_size) option_type;

 public:
  explicit PT_alter_tablespace_option_file_block_size(
      option_type file_block_size)
      : m_file_block_size(file_block_size) {}

  bool contextualize(Alter_tablespace_parse_context *pc) override {
    if (super::contextualize(pc)) return true; /* purecov: inspected */  // OOM

    if (pc->file_block_size != 0) {
      my_error(ER_FILEGROUP_OPTION_ONLY_ONCE, MYF(0), "FILE_BLOCK_SIZE");
      return true;
    }
    pc->file_block_size = m_file_block_size;
    return false;
  }

 private:
  const option_type m_file_block_size;
};

/**
  Parse tree node for CREATE RESOURCE GROUP statement.
*/

class PT_create_resource_group final : public Parse_tree_root {
  resourcegroups::Sql_cmd_create_resource_group sql_cmd;
  const bool has_priority;

 public:
  PT_create_resource_group(
      const LEX_CSTRING &name, const resourcegroups::Type type,
      const Mem_root_array<resourcegroups::Range> *cpu_list,
      const Value_or_default<int> &opt_priority, bool enabled)
      : sql_cmd(name, type, cpu_list,
                opt_priority.is_default ? 0 : opt_priority.value, enabled),
        has_priority(!opt_priority.is_default) {}

  Sql_cmd *make_cmd(THD *thd) override {
    if (check_resource_group_support()) return nullptr;

    if (check_resource_group_name_len(sql_cmd.m_name)) return nullptr;

    if (has_priority &&
        validate_resource_group_priority(thd, &sql_cmd.m_priority,
                                         sql_cmd.m_name, sql_cmd.m_type))
      return nullptr;

    for (auto &range : *sql_cmd.m_cpu_list) {
      if (validate_vcpu_range(range)) return nullptr;
    }

    thd->lex->sql_command = SQLCOM_CREATE_RESOURCE_GROUP;
    return &sql_cmd;
  }
};

/**
  Parse tree node for ALTER RESOURCE GROUP statement.
*/

class PT_alter_resource_group final : public Parse_tree_root {
  resourcegroups::Sql_cmd_alter_resource_group sql_cmd;

 public:
  PT_alter_resource_group(const LEX_CSTRING &name,
                          const Mem_root_array<resourcegroups::Range> *cpu_list,
                          const Value_or_default<int> &opt_priority,
                          const Value_or_default<bool> &enable, bool force)
      : sql_cmd(name, cpu_list,
                opt_priority.is_default ? 0 : opt_priority.value,
                enable.is_default ? false : enable.value, force,
                !enable.is_default) {}

  Sql_cmd *make_cmd(THD *thd) override {
    if (check_resource_group_support()) return nullptr;

    if (check_resource_group_name_len(sql_cmd.m_name)) return nullptr;

    for (auto &range : *sql_cmd.m_cpu_list) {
      if (validate_vcpu_range(range)) return nullptr;
    }

    thd->lex->sql_command = SQLCOM_ALTER_RESOURCE_GROUP;
    return &sql_cmd;
  }
};

/**
  Parse tree node for DROP RESOURCE GROUP statement.
*/

class PT_drop_resource_group final : public Parse_tree_root {
  resourcegroups::Sql_cmd_drop_resource_group sql_cmd;

 public:
  PT_drop_resource_group(const LEX_CSTRING &resource_group_name, bool force)
      : sql_cmd(resource_group_name, force) {}

  Sql_cmd *make_cmd(THD *thd) override {
    if (check_resource_group_support()) return nullptr;

    if (check_resource_group_name_len(sql_cmd.m_name)) return nullptr;

    thd->lex->sql_command = SQLCOM_DROP_RESOURCE_GROUP;
    return &sql_cmd;
  }
};

/**
  Parse tree node for SET RESOURCE GROUP statement.
*/

class PT_set_resource_group final : public Parse_tree_root {
  resourcegroups::Sql_cmd_set_resource_group sql_cmd;

 public:
  PT_set_resource_group(const LEX_CSTRING &name,
                        Mem_root_array<ulonglong> *thread_id_list)
      : sql_cmd(name, thread_id_list) {}

  Sql_cmd *make_cmd(THD *thd) override {
    if (check_resource_group_support()) return nullptr;

    if (check_resource_group_name_len(sql_cmd.m_name)) return nullptr;

    thd->lex->sql_command = SQLCOM_SET_RESOURCE_GROUP;
    return &sql_cmd;
  }
};

class PT_explain_for_connection final : public Parse_tree_root {
 public:
  explicit PT_explain_for_connection(my_thread_id thread_id)
      : m_cmd(thread_id) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Sql_cmd_explain_other_thread m_cmd;
};

class PT_explain final : public Parse_tree_root {
 public:
  PT_explain(Explain_format_type format, Parse_tree_root *explainable_stmt)
      : m_format(format), m_explainable_stmt(explainable_stmt) {}

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  const Explain_format_type m_format;
  Parse_tree_root *const m_explainable_stmt;
};

class PT_load_table final : public Parse_tree_root {
 public:
  PT_load_table(enum_filetype filetype, thr_lock_type lock_type,
                bool is_local_file, const LEX_STRING filename,
                On_duplicate on_duplicate, Table_ident *table,
                List<String> *opt_partitions, const CHARSET_INFO *opt_charset,
                String *opt_xml_rows_identified_by,
                const Field_separators &opt_field_separators,
                const Line_separators &opt_line_separators,
                ulong opt_ignore_lines, PT_item_list *opt_fields_or_vars,
                PT_item_list *opt_set_fields, PT_item_list *opt_set_exprs,
                List<String> *opt_set_expr_strings)
      : m_cmd(filetype, is_local_file, filename, on_duplicate, table,
              opt_partitions, opt_charset, opt_xml_rows_identified_by,
              opt_field_separators, opt_line_separators, opt_ignore_lines,
              opt_fields_or_vars ? &opt_fields_or_vars->value : nullptr,
              opt_set_fields ? &opt_set_fields->value : nullptr,
              opt_set_exprs ? &opt_set_exprs->value : nullptr,
              opt_set_expr_strings),
        m_lock_type(lock_type),
        m_opt_fields_or_vars(opt_fields_or_vars),
        m_opt_set_fields(opt_set_fields),
        m_opt_set_exprs(opt_set_exprs) {
    DBUG_ASSERT((opt_set_fields == nullptr) ^ (opt_set_exprs != nullptr));
    DBUG_ASSERT(opt_set_fields == nullptr || opt_set_fields->value.elements ==
                                                 opt_set_exprs->value.elements);
  }

  Sql_cmd *make_cmd(THD *thd) override;

 private:
  Sql_cmd_load_table m_cmd;

  const thr_lock_type m_lock_type;
  PT_item_list *m_opt_fields_or_vars;
  PT_item_list *m_opt_set_fields;
  PT_item_list *m_opt_set_exprs;
};

/**
  Top-level node for the SHUTDOWN statement

  @ingroup ptn_stmt
*/

class PT_restart_server final : public Parse_tree_root {
 public:
  Sql_cmd *make_cmd(THD *thd) override {
    thd->lex->sql_command = SQLCOM_RESTART_SERVER;
    return &sql_cmd;
  }

 private:
  Sql_cmd_restart_server sql_cmd;
};

#endif /* PARSE_TREE_NODES_INCLUDED */
