/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include <stddef.h>

#include "sql_show_processlist.h"

#include "auth/auth_acls.h"
#include "debug_sync.h"
#include "item_cmpfunc.h"  // Item_func_like
#include "item_func.h"     // Item_func_connection_id
#include "m_string.h"      // STRING_WITH_LEN
#include "mem_root_array.h"
#include "parse_tree_items.h"  // PTI_simple_ident_ident
#include "parse_tree_nodes.h"  // PT_select_item_list
#include "sql_class.h"         // THD
#include "sql_const.h"         // PROCESS_LIST_WIDTH
#include "sql_lex.h"           // Query_options
#include "sql_parse.h"         // check_table_access
#include "sql_string.h"
#include "strfunc.h"

bool pfs_processlist_enabled = false;

static const LEX_STRING field_id = {C_STRING_WITH_LEN("ID")};
static const LEX_STRING alias_id = {C_STRING_WITH_LEN("Id")};
static const LEX_STRING field_user = {C_STRING_WITH_LEN("USER")};
static const LEX_STRING alias_user = {C_STRING_WITH_LEN("User")};
static const LEX_STRING field_host = {C_STRING_WITH_LEN("HOST")};
static const LEX_STRING alias_host = {C_STRING_WITH_LEN("Host")};
static const LEX_STRING field_db = {C_STRING_WITH_LEN("DB")};
static const LEX_STRING alias_db = {C_STRING_WITH_LEN("db")};
static const LEX_STRING field_command = {C_STRING_WITH_LEN("COMMAND")};
static const LEX_STRING alias_command = {C_STRING_WITH_LEN("Command")};
static const LEX_STRING field_time = {C_STRING_WITH_LEN("TIME")};
static const LEX_STRING alias_time = {C_STRING_WITH_LEN("Time")};
static const LEX_STRING field_state = {C_STRING_WITH_LEN("STATE")};
static const LEX_STRING alias_state = {C_STRING_WITH_LEN("State")};
static const LEX_STRING field_info = {C_STRING_WITH_LEN("INFO")};
static const LEX_STRING alias_info = {C_STRING_WITH_LEN("Info")};

static const LEX_STRING pfs = {C_STRING_WITH_LEN("performance_schema")};
static const LEX_STRING table_processlist = {C_STRING_WITH_LEN("processlist")};

static const Query_options options = {
    0 /* query_spec_options */,
    SELECT_LEX::SQL_CACHE_UNSPECIFIED /* sql_cache */
};

static const Select_lock_type lock_type = {
    false,   /* is_set */
    TL_READ, /* lock_type */
    false    /* is_safe_to_cache_query */
};

/**
  Build a replacement query for SHOW PROCESSLIST.
  When the parser accepts the following syntax:

  <code>
    SHOW [FULL] PROCESSLIST
  </code>

  the parsed tree built for this query is in fact:

  <code>
  SELECT * FROM
    (SELECT ID Id, USER User, HOST Host, DB db, COMMAND Command,
       TIME Time, STATE State, LEFT(INFO, <info_len>) Info
     FROM performance_schema.processlist)
    AS show_processlist;
  </code>

  where @c info_len is 100 by default, otherwise 1024 for FULL PROCESSLIST.

  MAINTAINER:
  This code builds a parsed tree for a query.
  Write the query to build in SQL first, then see turn_parser_debug_on()
  in sql_yacc.yy to understand which grammar actions are needed to build
  a parsed tree for this SQL query.
*/

static bool add_expression(const POS &pos, THD *thd,
                           PT_select_item_list *item_list,
                           const LEX_STRING &field, const LEX_STRING &alias) {
  /* Field */
  PTI_simple_ident_ident *ident;
  ident = new (thd->mem_root) PTI_simple_ident_ident(pos, field);
  if (ident == NULL) return true;

  /* Alias */
  PTI_expr_with_alias *expr;
  expr = new (thd->mem_root) PTI_expr_with_alias(pos, ident, pos.cpp, alias);
  if (expr == NULL) return true;

  /* Add to item list */
  item_list->push_back(expr);
  return false;
}

bool build_processlist_query(const POS &pos, THD *thd, bool verbose) {
  /* ... * ... */
  Item *star = new (thd->mem_root) Item_asterisk(pos, NULL, NULL);
  if (star == NULL) return true;

  PT_select_item_list *item_list_star =
      new (thd->mem_root) PT_select_item_list();
  if (item_list_star == NULL) return true;

  item_list_star->push_back(star);

  /* SELECT * ... */
  PT_select_options_and_item_list *select_options_and_item_list =
      new (thd->mem_root)
          PT_select_options_and_item_list(options, item_list_star);
  if (select_options_and_item_list == NULL) return true;

  LEX_STRING info_len;
  /*
    Default Info field length is 100. Verbose field length is limited to the
    size of the INFO columns in the Performance Schema.
  */
  assert(PROCESS_LIST_WIDTH == 100);
  if (verbose) {
    if (!thd->make_lex_string(&info_len, "1024", 4, false)) return true;
  } else {
    if (!thd->make_lex_string(&info_len, "100", 3, false)) return true;
  }

  /* Id, User, Host, db, Command, Time, State */
  PT_select_item_list *item_list = new (thd->mem_root) PT_select_item_list();
  if (item_list == NULL) return true;

  if (add_expression(pos, thd, item_list, field_id, alias_id)) return true;
  if (add_expression(pos, thd, item_list, field_user, alias_user)) return true;
  if (add_expression(pos, thd, item_list, field_host, alias_host)) return true;
  if (add_expression(pos, thd, item_list, field_db, alias_db)) return true;
  if (add_expression(pos, thd, item_list, field_command, alias_command))
    return true;
  if (add_expression(pos, thd, item_list, field_time, alias_time)) return true;
  if (add_expression(pos, thd, item_list, field_state, alias_state))
    return true;

  /* ... INFO ... */
  PTI_simple_ident_ident *ident_info =
      new (thd->mem_root) PTI_simple_ident_ident(pos, field_info);
  if (ident_info == NULL) return true;

  /* Info length is either "100" or "1024" depending on verbose */
  Item_int *item_info_len =
      new (thd->mem_root) PTI_num_literal_num(pos, info_len);
  if (item_info_len == NULL) return true;

  /* ... LEFT(INFO, <info_len>) AS Info ...*/
  Item_func_left *func_left =
      new (thd->mem_root) Item_func_left(pos, ident_info, item_info_len);
  if (func_left == NULL) return true;

  PTI_expr_with_alias *expr_left = new (thd->mem_root)
      PTI_expr_with_alias(pos, func_left, pos.cpp, alias_info);
  if (expr_left == NULL) return true;

  item_list->push_back(expr_left);

  /*
    make_table_list() might alter the database and table name strings. Create
    copies and leave the original values unaltered.
  */

  /* ... performance_schema ... */
  LEX_CSTRING tmp_db_name;
  if (!thd->make_lex_string(&tmp_db_name, pfs.str, pfs.length, false))
    return true;

  /* ... processlist ... */
  LEX_CSTRING tmp_table_processlist;
  if (!thd->make_lex_string(&tmp_table_processlist, table_processlist.str,
                            table_processlist.length, false))
    return true;

  /* ... performance_schema.processlist ... */
  Table_ident *table_ident_processlist =
      new (thd->mem_root) Table_ident(tmp_db_name, tmp_table_processlist);
  if (table_ident_processlist == NULL) return true;

  /* ... FROM performance_schema.processlist ... */
  PT_table_factor_table_ident *table_factor_processlist = new (thd->mem_root)
      PT_table_factor_table_ident(table_ident_processlist, NULL, NULL, NULL);
  if (table_factor_processlist == NULL) return true;

  PT_join_table_list *join_table_list_1 =
      new (thd->mem_root) PT_join_table_list(pos, table_factor_processlist);
  if (join_table_list_1 == NULL) return true;

  PT_table_reference_list *table_reference_list_1 =
      new (thd->mem_root) PT_table_reference_list(join_table_list_1);
  if (table_reference_list_1 == NULL) return true;

  PT_table_expression *table_expression = new (thd->mem_root)
      PT_table_expression(table_reference_list_1, NULL, NULL, NULL, NULL, NULL,
                          NULL, lock_type);
  if (table_expression == NULL) return true;

  /* ... FROM (SELECT ...) ... */
  PT_table_factor_select_sym *table_factor_nested =
      new (thd->mem_root) PT_table_factor_select_sym(
          pos, NULL, options, item_list, table_expression);
  if (table_factor_nested == NULL) return true;

  PT_select_derived *derived =
      new (thd->mem_root) PT_select_derived(pos, table_factor_nested);
  if (derived == NULL) return true;

  PT_select_derived_union_select *derived_union =
      new (thd->mem_root) PT_select_derived_union_select(derived, NULL, pos);
  if (derived_union == NULL) return true;

  /* ... derived_table ... */
  LEX_STRING derived_table_name;
  if (!thd->make_lex_string(&derived_table_name, table_processlist.str,
                            table_processlist.length, false))
    return true;

  PT_table_factor_parenthesis *table_factor_2 = new (thd->mem_root)
      PT_table_factor_parenthesis(derived_union, &derived_table_name, pos);
  if (table_factor_2 == NULL) return true;

  PT_join_table_list *join_table_list_2 =
      new (thd->mem_root) PT_join_table_list(pos, table_factor_2);
  if (join_table_list_2 == NULL) return true;

  PT_table_reference_list *table_reference_list_2 =
      new (thd->mem_root) PT_table_reference_list(join_table_list_2);
  if (table_reference_list_2 == NULL) return true;

  PT_select_part2 *select_part2;
  select_part2 = new (thd->mem_root) PT_select_part2(
      select_options_and_item_list, /* select_options_and_item_list */
      NULL,                         /* opt_into */
      table_reference_list_2,       /* from_clause */
      NULL,                         /* opt_where_clause */
      NULL,                         /* opt_group_clause */
      NULL,                         /* opt_having_clause */
      NULL,                         /* opt_order_clause */
      NULL,                         /* opt_limit_clause */
      NULL,                         /* opt_procedure_analyse_clause */
      NULL,                         /* opt_into */
      lock_type);                   /* opt_select_lock_type */
  if (select_part2 == NULL) return true;

  PT_select_init2 *select_init2 =
      new (thd->mem_root) PT_select_init2(NULL, select_part2, NULL);
  if (select_init2 == NULL) return true;

  PT_select *select =
      new (thd->mem_root) PT_select(select_init2, SQLCOM_SELECT);
  if (select == NULL) return true;

  LEX *lex = thd->lex;
  SELECT_LEX *current_select = lex->current_select();
  Parse_context pc(thd, current_select);
  if (thd->is_error()) return true;

  if (select->contextualize(&pc)) return true;

  /* contextualize sets to COM_SELECT */
  lex->sql_command = SQLCOM_SHOW_PROCESSLIST;

  return false;
}
