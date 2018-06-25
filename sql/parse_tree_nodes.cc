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

#include "sql/parse_tree_nodes.h"

#include <string.h>
#include <algorithm>

#include "m_ctype.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_dbug.h"
#include "mysql/udf_registration_types.h"
#include "scope_guard.h"
#include "sql/dd/info_schema/show.h"      // build_show_...
#include "sql/dd/types/abstract_table.h"  // dd::enum_table_type::BASE_TABLE
#include "sql/derror.h"                   // ER_THD
#include "sql/gis/srid.h"
#include "sql/item_timefunc.h"
#include "sql/key_spec.h"
#include "sql/mdl.h"
#include "sql/mysqld.h"                   // global_system_variables
#include "sql/opt_explain_json.h"         // Explain_format_JSON
#include "sql/opt_explain_traditional.h"  // Explain_format_traditional
#include "sql/parse_tree_column_attrs.h"  // PT_field_def_base
#include "sql/parse_tree_hints.h"
#include "sql/parse_tree_partitions.h"  // PT_partition
#include "sql/query_options.h"
#include "sql/sp.h"        // sp_add_used_routine
#include "sql/sp_instr.h"  // sp_instr_set
#include "sql/sp_pcontext.h"
#include "sql/sql_base.h"  // find_temporary_table
#include "sql/sql_call.h"  // Sql_cmd_call...
#include "sql/sql_cmd.h"
#include "sql/sql_cmd_ddl_table.h"
#include "sql/sql_data_change.h"
#include "sql/sql_delete.h"  // Sql_cmd_delete...
#include "sql/sql_do.h"      // Sql_cmd_do...
#include "sql/sql_error.h"
#include "sql/sql_insert.h"  // Sql_cmd_insert...
#include "sql/sql_select.h"  // Sql_cmd_select...
#include "sql/sql_update.h"  // Sql_cmd_update...
#include "sql/system_variables.h"
#include "sql/thr_malloc.h"
#include "sql/trigger_def.h"
#include "sql_string.h"

PT_joined_table *PT_table_reference::add_cross_join(PT_cross_join *cj) {
  cj->add_rhs(this);
  return cj;
}

bool PT_option_value_no_option_type_charset::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  int flags = opt_charset ? 0 : set_var_collation_client::SET_CS_DEFAULT;
  const CHARSET_INFO *cs2;
  cs2 =
      opt_charset ? opt_charset : global_system_variables.character_set_client;
  set_var_collation_client *var;
  var = new (*THR_MALLOC) set_var_collation_client(
      flags, cs2, thd->variables.collation_database, cs2);
  if (var == NULL) return true;
  lex->var_list.push_back(var);
  return false;
}

bool PT_option_value_no_option_type_names::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();
  LEX_STRING names = {C_STRING_WITH_LEN("names")};

  if (pctx && pctx->find_variable(names, false))
    my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), names.str);
  else
    error(pc, pos);

  return true;  // alwais fails with an error
}

bool PT_set_names::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  const CHARSET_INFO *cs2;
  const CHARSET_INFO *cs3;
  int flags = set_var_collation_client::SET_CS_NAMES |
              (opt_charset ? 0 : set_var_collation_client::SET_CS_DEFAULT) |
              (opt_collation ? set_var_collation_client::SET_CS_COLLATE : 0);
  cs2 =
      opt_charset ? opt_charset : global_system_variables.character_set_client;
  if (opt_collation != nullptr) {
    if (!my_charset_same(cs2, opt_collation)) {
      my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0), opt_collation->name,
               cs2->csname);
      return true;
    }
    cs3 = opt_collation;
  } else {
    if (cs2 == &my_charset_utf8mb4_0900_ai_ci &&
        cs2 != thd->variables.default_collation_for_utf8mb4)
      cs3 = thd->variables.default_collation_for_utf8mb4;
    else
      cs3 = cs2;
  }
  set_var_collation_client *var;
  var = new (*THR_MALLOC) set_var_collation_client(flags, cs3, cs3, cs3);
  if (var == NULL) return true;
  lex->var_list.push_back(var);
  return false;
}

bool PT_group::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  SELECT_LEX *select = pc->select;
  select->parsing_place = CTX_GROUP_BY;

  if (group_list->contextualize(pc)) return true;
  DBUG_ASSERT(select == pc->select);

  select->group_list = group_list->value;

  // Ensure we're resetting parsing place of the right select
  DBUG_ASSERT(select->parsing_place == CTX_GROUP_BY);
  select->parsing_place = CTX_NONE;

  switch (olap) {
    case UNSPECIFIED_OLAP_TYPE:
      break;
    case ROLLUP_TYPE:
      if (select->linkage == GLOBAL_OPTIONS_TYPE) {
        my_error(ER_WRONG_USAGE, MYF(0), "WITH ROLLUP",
                 "global union parameters");
        return true;
      }
      select->olap = ROLLUP_TYPE;
      break;
    default:
      DBUG_ASSERT(!"unexpected OLAP type!");
  }
  return false;
}

bool PT_order::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  SELECT_LEX_UNIT *const unit = pc->select->master_unit();
  const bool braces = pc->select->braces;

  if (lex->sql_command != SQLCOM_ALTER_TABLE && !unit->fake_select_lex) {
    /*
      A query of the of the form (SELECT ...) ORDER BY order_list is
      executed in the same way as the query
      SELECT ... ORDER BY order_list
      unless the SELECT construct contains ORDER BY or LIMIT clauses.
      Otherwise we create a fake SELECT_LEX if it has not been created
      yet.
    */
    SELECT_LEX *first_sl = unit->first_select();
    if (!unit->is_union() &&
        (first_sl->order_list.elements || first_sl->select_limit)) {
      if (unit->add_fake_select_lex(lex->thd)) return true;
      pc->select = unit->fake_select_lex;
    }
  }

  bool context_is_pushed = false;
  if (pc->select->parsing_place == CTX_NONE) {
    if (unit->is_union() && !braces) {
      /*
        At this point we don't know yet whether this is the last
        select in union or not, but we move ORDER BY to
        fake_select_lex anyway. If there would be one more select
        in union mysql_new_select will correctly throw error.
      */
      pc->select = unit->fake_select_lex;
      lex->push_context(&pc->select->context);
      context_is_pushed = true;
    }
    /*
      To preserve correct markup for the case
       SELECT group_concat(... ORDER BY (subquery))
      we do not change parsing_place if it's not NONE.
    */
    pc->select->parsing_place = CTX_ORDER_BY;
  }

  if (order_list->contextualize(pc)) return true;

  if (context_is_pushed) lex->pop_context();

  pc->select->order_list = order_list->value;

  // Reset parsing place only for ORDER BY
  if (pc->select->parsing_place == CTX_ORDER_BY)
    pc->select->parsing_place = CTX_NONE;
  return false;
}

bool PT_internal_variable_name_1d::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();
  sp_variable *spv;

  value.var = NULL;
  value.base_name = ident;

  /* Best effort lookup for system variable. */
  if (!pctx || !(spv = pctx->find_variable(ident, false))) {
    /* Not an SP local variable */
    if (find_sys_var_null_base(thd, &value)) return true;
  } else {
    /*
      Possibly an SP local variable (or a shadowed sysvar).
      Will depend on the context of the SET statement.
    */
  }
  return false;
}

bool PT_internal_variable_name_2d::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  sp_head *sp = lex->sphead;

  if (check_reserved_words(&ident1)) {
    error(pc, pos);
    return true;
  }

  if (sp && sp->m_type == enum_sp_type::TRIGGER &&
      (!my_strcasecmp(system_charset_info, ident1.str, "NEW") ||
       !my_strcasecmp(system_charset_info, ident1.str, "OLD"))) {
    if (ident1.str[0] == 'O' || ident1.str[0] == 'o') {
      my_error(ER_TRG_CANT_CHANGE_ROW, MYF(0), "OLD", "");
      return true;
    }
    if (sp->m_trg_chistics.event == TRG_EVENT_DELETE) {
      my_error(ER_TRG_NO_SUCH_ROW_IN_TRG, MYF(0), "NEW", "on DELETE");
      return true;
    }
    if (sp->m_trg_chistics.action_time == TRG_ACTION_AFTER) {
      my_error(ER_TRG_CANT_CHANGE_ROW, MYF(0), "NEW", "after ");
      return true;
    }
    /* This special combination will denote field of NEW row */
    value.var = trg_new_row_fake_var;
    value.base_name = ident2;
  } else {
    const LEX_STRING *domain;
    const LEX_STRING *variable;
    bool is_key_cache_variable = false;
    sys_var *tmp;
    if (ident2.str && is_key_cache_variable_suffix(ident2.str)) {
      is_key_cache_variable = true;
      domain = &ident2;
      variable = &ident1;
      tmp = find_sys_var(thd, domain->str, domain->length);
    } else {
      domain = &ident1;
      variable = &ident2;
      /*
        We are getting the component name as domain->str and variable name
        as variable->str, and we are adding the "." as a separator to find
        the variable from systam_variable_hash.
        We are doing this, because we use the structured variable syntax for
        component variables.
      */
      String tmp_name;
      if (tmp_name.reserve(domain->length + 1 + variable->length + 1) ||
          tmp_name.append(domain->str) || tmp_name.append(".") ||
          tmp_name.append(variable->str))
        return true;  // OOM
      tmp = find_sys_var(thd, tmp_name.c_ptr(), tmp_name.length());
    }
    if (!tmp) return true;

    if (is_key_cache_variable && !tmp->is_struct())
      my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), domain->str);

    value.var = tmp;
    if (is_key_cache_variable)
      value.base_name = *variable;
    else
      value.base_name = null_lex_str;
  }
  return false;
}

bool PT_option_value_no_option_type_internal::contextualize(Parse_context *pc) {
  if (super::contextualize(pc) || name->contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  sp_head *sp = lex->sphead;

  if (opt_expr != NULL && opt_expr->itemize(pc, &opt_expr)) return true;

  const char *expr_start_ptr = NULL;

  if (sp) expr_start_ptr = expr_pos.raw.start;

  if (name->value.var == trg_new_row_fake_var) {
    DBUG_ASSERT(sp);
    DBUG_ASSERT(expr_start_ptr);

    /* We are parsing trigger and this is a trigger NEW-field. */

    LEX_STRING expr_query = EMPTY_STR;

    if (!opt_expr) {
      // This is: SET NEW.x = DEFAULT
      // DEFAULT clause is not supported in triggers.

      error(pc, expr_pos);
      return true;
    } else if (lex->is_metadata_used()) {
      expr_query = make_string(thd, expr_start_ptr, expr_pos.raw.end);

      if (!expr_query.str) return true;
    }

    if (set_trigger_new_row(pc, name->value.base_name, opt_expr, expr_query))
      return true;
  } else if (name->value.var) {
    /* We're not parsing SP and this is a system variable. */

    if (set_system_variable(thd, &name->value, lex->option_type, opt_expr))
      return true;
  } else {
    DBUG_ASSERT(sp);
    DBUG_ASSERT(expr_start_ptr);

    /* We're parsing SP and this is an SP-variable. */

    sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();
    sp_variable *spv = pctx->find_variable(name->value.base_name, false);

    LEX_STRING expr_query = EMPTY_STR;

    if (!opt_expr) {
      /*
        This is: SET x = DEFAULT, where x is a SP-variable.
        This is not supported.
      */

      error(pc, expr_pos);
      return true;
    } else if (lex->is_metadata_used()) {
      expr_query = make_string(thd, expr_start_ptr, expr_pos.raw.end);

      if (!expr_query.str) return true;
    }

    /*
      NOTE: every SET-expression has its own LEX-object, even if it is
      a multiple SET-statement, like:

        SET spv1 = expr1, spv2 = expr2, ...

      Every SET-expression has its own sp_instr_set. Thus, the
      instruction owns the LEX-object, i.e. the instruction is
      responsible for destruction of the LEX-object.
    */

    sp_instr_set *i = new (*THR_MALLOC)
        sp_instr_set(sp->instructions(), lex, spv->offset, opt_expr, expr_query,
                     true);  // The instruction owns its lex.

    if (!i || sp->add_instr(thd, i)) return true;
  }
  return false;
}

bool PT_option_value_no_option_type_password_for::contextualize(
    Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  set_var_password *var;
  lex->contains_plaintext_password = true;

  /*
    In case of anonymous user, user->user is set to empty string with
    length 0. But there might be case when user->user.str could be NULL.
    For Ex: "set password for current_user() = password('xyz');".
    In this case, set user information as of the current user.
  */
  if (!user->user.str) {
    LEX_CSTRING sctx_priv_user = thd->security_context()->priv_user();
    DBUG_ASSERT(sctx_priv_user.str);
    user->user.str = sctx_priv_user.str;
    user->user.length = sctx_priv_user.length;
  }
  if (!user->host.str) {
    LEX_CSTRING sctx_priv_host = thd->security_context()->priv_host();
    DBUG_ASSERT(sctx_priv_host.str);
    user->host.str = (char *)sctx_priv_host.str;
    user->host.length = sctx_priv_host.length;
  }

  // Current password is specified through the REPLACE clause hence set the flag
  if (current_password != nullptr) user->uses_replace_clause = true;

  var = new (*THR_MALLOC) set_var_password(
      user, const_cast<char *>(password), const_cast<char *>(current_password));

  if (var == NULL || lex->var_list.push_back(var)) {
    return true;  // Out of memory
  }
  lex->sql_command = SQLCOM_SET_PASSWORD;
  if (lex->sphead) lex->sphead->m_flags |= sp_head::HAS_SET_AUTOCOMMIT_STMT;
  if (sp_create_assignment_instr(pc->thd, expr_pos.raw.end)) return true;
  return false;
}

bool PT_option_value_no_option_type_password::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  sp_head *sp = lex->sphead;
  sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();
  LEX_STRING pw = {C_STRING_WITH_LEN("password")};
  lex->contains_plaintext_password = true;

  if (pctx && pctx->find_variable(pw, false)) {
    my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), pw.str);
    return true;
  }

  LEX_CSTRING sctx_user = thd->security_context()->user();
  LEX_CSTRING sctx_priv_host = thd->security_context()->priv_host();
  DBUG_ASSERT(sctx_priv_host.str);

  LEX_USER *user = LEX_USER::alloc(thd, (LEX_STRING *)&sctx_user,
                                   (LEX_STRING *)&sctx_priv_host);
  if (!user) return true;

  set_var_password *var = new (*THR_MALLOC) set_var_password(
      user, const_cast<char *>(password), const_cast<char *>(current_password));
  if (var == NULL || lex->var_list.push_back(var)) {
    return true;  // Out of Memory
  }
  lex->sql_command = SQLCOM_SET_PASSWORD;

  if (sp) sp->m_flags |= sp_head::HAS_SET_AUTOCOMMIT_STMT;

  if (sp_create_assignment_instr(pc->thd, expr_pos.raw.end)) return true;

  return false;
}

bool PT_select_sp_var::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  LEX *lex = pc->thd->lex;
#ifndef DBUG_OFF
  sp = lex->sphead;
#endif
  sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();
  sp_variable *spv;

  if (!pctx || !(spv = pctx->find_variable(name, false))) {
    my_error(ER_SP_UNDECLARED_VAR, MYF(0), name.str);
    return true;
  }

  offset = spv->offset;

  return false;
}

Sql_cmd *PT_select_stmt::make_cmd(THD *thd) {
  Parse_context pc(thd, thd->lex->current_select());

  thd->lex->sql_command = m_sql_command;

  if (m_qe->contextualize(&pc) || contextualize_safe(&pc, m_into))
    return nullptr;

  if (thd->lex->sql_command == SQLCOM_SELECT)
    return new (thd->mem_root) Sql_cmd_select(thd->lex->result);
  else  // (thd->lex->sql_command == SQLCOM_DO)
    return new (thd->mem_root) Sql_cmd_do(NULL);
}

/*
  Given a table in the source list, find a correspondent table in the
  list of table references.

  @param tbl    Source table to match.
  @param tables Table references list.

  @remark The source table list (tables listed before the FROM clause
  or tables listed in the FROM clause before the USING clause) may
  contain table names or aliases that must match unambiguously one,
  and only one, table in the target table list (table references list,
  after FROM/USING clause).

  @return Matching table, NULL if error.
*/

static TABLE_LIST *multi_delete_table_match(TABLE_LIST *tbl,
                                            TABLE_LIST *tables) {
  TABLE_LIST *match = NULL;
  DBUG_ENTER("multi_delete_table_match");

  for (TABLE_LIST *elem = tables; elem; elem = elem->next_local) {
    int cmp;

    if (tbl->is_fqtn && elem->is_alias) continue; /* no match */
    if (tbl->is_fqtn && elem->is_fqtn)
      cmp = my_strcasecmp(table_alias_charset, tbl->table_name,
                          elem->table_name) ||
            strcmp(tbl->db, elem->db);
    else if (elem->is_alias)
      cmp = my_strcasecmp(table_alias_charset, tbl->alias, elem->alias);
    else
      cmp = my_strcasecmp(table_alias_charset, tbl->table_name,
                          elem->table_name) ||
            strcmp(tbl->db, elem->db);

    if (cmp) continue;

    if (match) {
      my_error(ER_NONUNIQ_TABLE, MYF(0), elem->alias);
      DBUG_RETURN(NULL);
    }

    match = elem;
  }

  if (!match)
    my_error(ER_UNKNOWN_TABLE, MYF(0), tbl->table_name, "MULTI DELETE");

  DBUG_RETURN(match);
}

/**
  Link tables in auxiliary table list of multi-delete with corresponding
  elements in main table list, and set proper locks for them.

  @param pc   Parse context
  @param delete_tables  List of tables to delete from

  @returns false if success, true if error
*/

static bool multi_delete_link_tables(Parse_context *pc,
                                     SQL_I_List<TABLE_LIST> *delete_tables) {
  DBUG_ENTER("multi_delete_link_tables");

  TABLE_LIST *tables = pc->select->table_list.first;

  for (TABLE_LIST *target_tbl = delete_tables->first; target_tbl;
       target_tbl = target_tbl->next_local) {
    /* All tables in aux_tables must be found in FROM PART */
    TABLE_LIST *walk = multi_delete_table_match(target_tbl, tables);
    if (!walk) DBUG_RETURN(true);
    if (!walk->is_derived()) {
      target_tbl->table_name = walk->table_name;
      target_tbl->table_name_length = walk->table_name_length;
    }
    walk->updating = target_tbl->updating;
    walk->set_lock(target_tbl->lock_descriptor());
    /* We can assume that tables to be deleted from are locked for write. */
    DBUG_ASSERT(walk->lock_descriptor().type >= TL_WRITE_ALLOW_WRITE);
    walk->mdl_request.set_type(mdl_type_for_dml(walk->lock_descriptor().type));
    target_tbl->correspondent_table = walk;  // Remember corresponding table
  }
  DBUG_RETURN(false);
}

bool PT_delete::add_table(Parse_context *pc, Table_ident *table) {
  const ulong table_opts = is_multitable()
                               ? TL_OPTION_UPDATING | TL_OPTION_ALIAS
                               : TL_OPTION_UPDATING;
  const thr_lock_type lock_type = (opt_delete_options & DELETE_LOW_PRIORITY)
                                      ? TL_WRITE_LOW_PRIORITY
                                      : TL_WRITE_DEFAULT;
  const enum_mdl_type mdl_type = (opt_delete_options & DELETE_LOW_PRIORITY)
                                     ? MDL_SHARED_WRITE_LOW_PRIO
                                     : MDL_SHARED_WRITE;
  return !pc->select->add_table_to_list(pc->thd, table, NULL, table_opts,
                                        lock_type, mdl_type, NULL,
                                        opt_use_partition);
}

Sql_cmd *PT_delete::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  SELECT_LEX *const select = lex->current_select();

  Parse_context pc(thd, select);

  DBUG_ASSERT(lex->select_lex == select);
  lex->sql_command = is_multitable() ? SQLCOM_DELETE_MULTI : SQLCOM_DELETE;
  lex->set_ignore(opt_delete_options & DELETE_IGNORE);
  select->init_order();
  if (opt_delete_options & DELETE_QUICK) select->add_base_options(OPTION_QUICK);

  if (contextualize_safe(&pc, m_with_clause))
    return NULL; /* purecov: inspected */

  if (is_multitable()) {
    for (Table_ident **i = table_list.begin(); i != table_list.end(); ++i) {
      if (add_table(&pc, *i)) return NULL;
    }
  } else if (add_table(&pc, table_ident))
    return NULL;

  if (is_multitable()) {
    select->table_list.save_and_clear(&delete_tables);
    lex->query_tables = NULL;
    lex->query_tables_last = &lex->query_tables;
  } else {
    select->top_join_list.push_back(select->get_table_list());
  }
  Yacc_state *const yyps = &pc.thd->m_parser_state->m_yacc;
  yyps->m_lock_type = TL_READ_DEFAULT;
  yyps->m_mdl_type = MDL_SHARED_READ;

  if (is_multitable()) {
    if (contextualize_array(&pc, &join_table_list)) return NULL;
    pc.select->context.table_list =
        pc.select->context.first_name_resolution_table =
            pc.select->table_list.first;
  }

  if (opt_where_clause != NULL &&
      opt_where_clause->itemize(&pc, &opt_where_clause))
    return NULL;
  select->set_where_cond(opt_where_clause);

  if (opt_order_clause != NULL && opt_order_clause->contextualize(&pc))
    return NULL;

  DBUG_ASSERT(select->select_limit == NULL);
  if (opt_delete_limit_clause != NULL) {
    if (opt_delete_limit_clause->itemize(&pc, &opt_delete_limit_clause))
      return NULL;
    select->select_limit = opt_delete_limit_clause;
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
    select->explicit_limit = true;
  }

  if (is_multitable() && multi_delete_link_tables(&pc, &delete_tables))
    return NULL;

  if (opt_hints != NULL && opt_hints->contextualize(&pc)) return NULL;

  return new (thd->mem_root) Sql_cmd_delete(is_multitable(), &delete_tables);
}

Sql_cmd *PT_update::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  SELECT_LEX *const select = lex->current_select();

  Parse_context pc(thd, select);

  lex->duplicates = DUP_ERROR;

  lex->set_ignore(opt_ignore);

  if (contextualize_safe(&pc, m_with_clause))
    return NULL; /* purecov: inspected */

  if (contextualize_array(&pc, &join_table_list)) return NULL;
  select->parsing_place = CTX_UPDATE_VALUE;

  if (column_list->contextualize(&pc) || value_list->contextualize(&pc)) {
    return NULL;
  }
  select->item_list = column_list->value;

  // Ensure we're resetting parsing context of the right select
  DBUG_ASSERT(select->parsing_place == CTX_UPDATE_VALUE);
  select->parsing_place = CTX_NONE;
  const bool is_multitable = select->table_list.elements > 1;
  lex->sql_command = is_multitable ? SQLCOM_UPDATE_MULTI : SQLCOM_UPDATE;

  /*
    In case of multi-update setting write lock for all tables may
    be too pessimistic. We will decrease lock level if possible in
    mysql_multi_update().
  */
  select->set_lock_for_tables(opt_low_priority);

  if (opt_where_clause != NULL &&
      opt_where_clause->itemize(&pc, &opt_where_clause)) {
    return NULL;
  }
  select->set_where_cond(opt_where_clause);

  if (opt_order_clause != NULL && opt_order_clause->contextualize(&pc))
    return NULL;

  DBUG_ASSERT(select->select_limit == NULL);
  if (opt_limit_clause != NULL) {
    if (opt_limit_clause->itemize(&pc, &opt_limit_clause)) return NULL;
    select->select_limit = opt_limit_clause;
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
    select->explicit_limit = true;
  }

  if (opt_hints != NULL && opt_hints->contextualize(&pc)) return NULL;

  return new (thd->mem_root) Sql_cmd_update(is_multitable, &value_list->value);
}

bool PT_insert_values_list::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;
  List_iterator<List_item> it1(many_values);
  List<Item> *item_list;
  while ((item_list = it1++)) {
    List_iterator<Item> it2(*item_list);
    Item *item;
    while ((item = it2++)) {
      if (item->itemize(pc, &item)) return true;
      it2.replace(item);
    }
  }

  return false;
}

Sql_cmd *PT_insert::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  Parse_context pc(thd, lex->current_select());

  if (is_replace) {
    lex->sql_command = has_select() ? SQLCOM_REPLACE_SELECT : SQLCOM_REPLACE;
    lex->duplicates = DUP_REPLACE;
  } else {
    lex->sql_command = has_select() ? SQLCOM_INSERT_SELECT : SQLCOM_INSERT;
    lex->duplicates = DUP_ERROR;
    lex->set_ignore(ignore);
  }

  Yacc_state *yyps = &pc.thd->m_parser_state->m_yacc;
  if (!pc.select->add_table_to_list(thd, table_ident, NULL, TL_OPTION_UPDATING,
                                    yyps->m_lock_type, yyps->m_mdl_type, NULL,
                                    opt_use_partition)) {
    return NULL;
  }
  pc.select->set_lock_for_tables(lock_option);

  DBUG_ASSERT(lex->current_select() == lex->select_lex);

  if (column_list->contextualize(&pc)) return NULL;

  if (has_select()) {
    /*
      In INSERT/REPLACE INTO t ... SELECT the table_list initially contains
      here a table entry for the destination table `t'.
      Backup it and clean the table list for the processing of
      the query expression and push `t' back to the beginning of the
      table_list finally.

      @todo: Don't save the INSERT/REPLACE destination table in
             SELECT_LEX::table_list and remove this backup & restore.

      The following work only with the local list, the global list
      is created correctly in this case
    */
    SQL_I_List<TABLE_LIST> save_list;
    SELECT_LEX *const save_select = pc.select;
    save_select->table_list.save_and_clear(&save_list);

    if (insert_query_expression->contextualize(&pc)) return NULL;

    /*
      The following work only with the local list, the global list
      is created correctly in this case
    */
    save_select->table_list.push_front(&save_list);

    lex->bulk_insert_row_cnt = 0;
  } else {
    pc.select->parsing_place = CTX_INSERT_VALUES;
    if (row_value_list->contextualize(&pc)) return NULL;
    // Ensure we're resetting parsing context of the right select
    DBUG_ASSERT(pc.select->parsing_place == CTX_INSERT_VALUES);
    pc.select->parsing_place = CTX_NONE;

    lex->bulk_insert_row_cnt = row_value_list->get_many_values().elements;
  }

  if (opt_on_duplicate_column_list != NULL) {
    DBUG_ASSERT(!is_replace);
    DBUG_ASSERT(opt_on_duplicate_value_list != NULL &&
                opt_on_duplicate_value_list->elements() ==
                    opt_on_duplicate_column_list->elements());

    lex->duplicates = DUP_UPDATE;
    TABLE_LIST *first_table = lex->select_lex->table_list.first;
    /* Fix lock for ON DUPLICATE KEY UPDATE */
    if (first_table->lock_descriptor().type == TL_WRITE_CONCURRENT_DEFAULT)
      first_table->set_lock({TL_WRITE_DEFAULT, THR_DEFAULT});

    pc.select->parsing_place = CTX_INSERT_UPDATE;

    if (opt_on_duplicate_column_list->contextualize(&pc) ||
        opt_on_duplicate_value_list->contextualize(&pc))
      return NULL;

    // Ensure we're resetting parsing context of the right select
    DBUG_ASSERT(pc.select->parsing_place == CTX_INSERT_UPDATE);
    pc.select->parsing_place = CTX_NONE;
  }

  if (opt_hints != NULL && opt_hints->contextualize(&pc)) return NULL;

  Sql_cmd_insert_base *sql_cmd;
  if (has_select())
    sql_cmd =
        new (thd->mem_root) Sql_cmd_insert_select(is_replace, lex->duplicates);
  else
    sql_cmd =
        new (thd->mem_root) Sql_cmd_insert_values(is_replace, lex->duplicates);
  if (sql_cmd == NULL) return NULL;

  if (!has_select())
    sql_cmd->insert_many_values = row_value_list->get_many_values();

  sql_cmd->insert_field_list = column_list->value;
  if (opt_on_duplicate_column_list != NULL) {
    DBUG_ASSERT(!is_replace);
    sql_cmd->update_field_list = opt_on_duplicate_column_list->value;
    sql_cmd->update_value_list = opt_on_duplicate_value_list->value;
  }

  return sql_cmd;
}

Sql_cmd *PT_call::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  Parse_context pc(thd, lex->current_select());

  if (opt_expr_list != NULL && opt_expr_list->contextualize(&pc))
    return NULL; /* purecov: inspected */

  lex->sql_command = SQLCOM_CALL;

  sp_add_own_used_routine(lex, thd, Sroutine_hash_entry::PROCEDURE, proc_name);

  List<Item> *proc_args = NULL;
  if (opt_expr_list != NULL) proc_args = &opt_expr_list->value;

  return new (thd->mem_root) Sql_cmd_call(proc_name, proc_args);
}

bool PT_query_specification::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  pc->select->parsing_place = CTX_SELECT_LIST;

  if (options.query_spec_options & SELECT_HIGH_PRIORITY) {
    Yacc_state *yyps = &pc->thd->m_parser_state->m_yacc;
    yyps->m_lock_type = TL_READ_HIGH_PRIORITY;
    yyps->m_mdl_type = MDL_SHARED_READ;
  }
  if (options.save_to(pc)) return true;

  if (item_list->contextualize(pc)) return true;

  // Ensure we're resetting parsing place of the right select
  DBUG_ASSERT(pc->select->parsing_place == CTX_SELECT_LIST);
  pc->select->parsing_place = CTX_NONE;

  if (contextualize_safe(pc, opt_into1)) return true;

  if (!from_clause.empty()) {
    if (contextualize_array(pc, &from_clause)) return true;
    pc->select->context.table_list =
        pc->select->context.first_name_resolution_table =
            pc->select->table_list.first;
  }

  if (itemize_safe(pc, &opt_where_clause) ||
      contextualize_safe(pc, opt_group_clause) ||
      itemize_safe(pc, &opt_having_clause))
    return true;

  pc->select->set_where_cond(opt_where_clause);
  pc->select->set_having_cond(opt_having_clause);

  /*
    Window clause is resolved under CTX_SELECT_LIST and not
    under CTX_WINDOW. Reasons being:
    1. Window functions are part of select list and the
    resolution of window definition happens along with
    window functions.
    2. It is tricky to resolve window definition under CTX_WINDOW
    and window functions under CTX_SELECT_LIST.
    3. Unnamed window definitions are anyways naturally placed in
    select list.
    4. Named window definition are not placed in select list of
    the query. But if this window definition is
    used by any window functions, then we resolve under CTX_SELECT_LIST.
    5. Because of all of the above, unused window definitions are
    resolved under CTX_SELECT_LIST. (These unused window definitions
    are removed after syntactic and semantic checks are done).
  */

  pc->select->parsing_place = CTX_SELECT_LIST;
  if (contextualize_safe(pc, opt_window_clause)) return true;
  pc->select->parsing_place = CTX_NONE;

  if (opt_hints != NULL) {
    if (pc->thd->lex->sql_command ==
        SQLCOM_CREATE_VIEW) {  // Currently this also affects ALTER VIEW.
      push_warning_printf(
          pc->thd, Sql_condition::SL_WARNING, ER_WARN_UNSUPPORTED_HINT,
          ER_THD(pc->thd, ER_WARN_UNSUPPORTED_HINT), "CREATE or ALTER VIEW");
    } else if (opt_hints->contextualize(pc))
      return true;
  }
  return false;
}

bool PT_table_factor_function::contextualize(Parse_context *pc) {
  if (super::contextualize(pc) || m_expr->itemize(pc, &m_expr)) return true;

  auto nested_columns = new (pc->mem_root) List<Json_table_column>;
  if (nested_columns == nullptr) return true;  // OOM

  for (auto col : *m_nested_columns) {
    if (col->contextualize(pc) || nested_columns->push_back(col->get_column()))
      return true;
  }

  auto root_el = new (pc->mem_root) Json_table_column(m_path, nested_columns);
  auto *root_list = new (pc->mem_root) List<Json_table_column>;
  if (root_el == NULL || root_list == NULL || root_list->push_front(root_el))
    return true;  // OOM

  auto jtf = new (pc->mem_root)
      Table_function_json(pc->thd, m_table_alias.str, m_expr, root_list);
  if (jtf == nullptr) return true;  // OOM

  LEX_CSTRING alias;
  alias.length = strlen(jtf->func_name());
  alias.str = sql_strmake(jtf->func_name(), alias.length);
  if (alias.str == nullptr) return true;  // OOM

  auto ti = new (pc->mem_root) Table_ident(alias, jtf);
  if (ti == nullptr) return true;

  value = pc->select->add_table_to_list(pc->thd, ti, m_table_alias.str, 0,
                                        TL_READ, MDL_SHARED_READ);
  if (value == NULL || pc->select->add_joined_table(value)) return true;

  return false;
}

PT_derived_table::PT_derived_table(PT_subquery *subquery,
                                   const LEX_CSTRING &table_alias,
                                   Create_col_name_list *column_names)
    : m_subquery(subquery),
      m_table_alias(table_alias.str),
      column_names(*column_names) {
  m_subquery->m_is_derived_table = true;
}

bool PT_derived_table::contextualize(Parse_context *pc) {
  SELECT_LEX *outer_select = pc->select;

  outer_select->parsing_place = CTX_DERIVED;
  DBUG_ASSERT(outer_select->linkage != GLOBAL_OPTIONS_TYPE);

  if (m_subquery->contextualize(pc)) return true;

  outer_select->parsing_place = CTX_NONE;

  DBUG_ASSERT(pc->select->next_select() == NULL);

  SELECT_LEX_UNIT *unit = pc->select->first_inner_unit();
  pc->select = outer_select;
  Table_ident *ti = new (*THR_MALLOC) Table_ident(unit);
  if (ti == NULL) return true;

  value = pc->select->add_table_to_list(pc->thd, ti, m_table_alias, 0, TL_READ,
                                        MDL_SHARED_READ);
  if (value == NULL) return true;
  if (column_names.size()) value->set_derived_column_names(&column_names);
  if (pc->select->add_joined_table(value)) return true;

  return false;
}

bool PT_table_factor_joined_table::contextualize(Parse_context *pc) {
  if (Parse_tree_node::contextualize(pc)) return true;

  SELECT_LEX *outer_select = pc->select;
  if (outer_select->init_nested_join(pc->thd)) return true;

  if (m_joined_table->contextualize(pc)) return true;
  value = m_joined_table->value;

  if (outer_select->end_nested_join() == nullptr) return true;

  return false;
}

/**
  A SELECT_LEX_UNIT has to be built in a certain order: First the SELECT_LEX
  representing the left-hand side of the union is built ("contextualized",)
  then the right hand side, and lastly the "fake" SELECT_LEX is built and made
  the "current" one. Only then can the order and limit clauses be
  contextualized, because they are attached to the fake SELECT_LEX. This is a
  bit unnatural, as these clauses belong to the surrounding `<query
  expression>`, not the `<query expression body>` which is the union (and
  represented by this class). For this reason, the PT_query_expression is
  expected to call `set_containing_qe(this)` on this object, so that during
  this contextualize() call, a call to contextualize_order_and_limit() can be
  made at just the right time.
*/
bool PT_union::contextualize(Parse_context *pc) {
  THD *thd = pc->thd;

  if (PT_query_expression_body::contextualize(pc)) return true;

  if (m_lhs->contextualize(pc)) return true;

  pc->select = pc->thd->lex->new_union_query(pc->select, m_is_distinct, false);

  if (pc->select == NULL || m_rhs->contextualize(pc)) return true;

  SELECT_LEX_UNIT *unit = pc->select->master_unit();
  if (unit->fake_select_lex == NULL && unit->add_fake_select_lex(thd))
    return true;

  SELECT_LEX *select_lex = pc->select;
  pc->select = unit->fake_select_lex;
  pc->select->no_table_names_allowed = true;

  if (m_containing_qe != NULL &&
      m_containing_qe->contextualize_order_and_limit(pc))
    return true;

  pc->select->no_table_names_allowed = false;
  pc->select = select_lex;

  pc->thd->lex->pop_context();

  return false;
}

static bool setup_index(keytype key_type, const LEX_STRING name,
                        PT_base_index_option *type,
                        List<Key_part_spec> *columns, Index_options options,
                        Table_ddl_parse_context *pc) {
  *pc->key_create_info = default_key_create_info;

  if (type != NULL && type->contextualize(pc)) return true;

  if (contextualize_nodes(options, pc)) return true;

  if ((key_type == KEYTYPE_FULLTEXT || key_type == KEYTYPE_SPATIAL ||
       pc->key_create_info->algorithm == HA_KEY_ALG_HASH)) {
    List_iterator<Key_part_spec> li(*columns);
    Key_part_spec *kp;
    while ((kp = li++)) {
      if (kp->is_explicit) {
        my_error(ER_WRONG_USAGE, MYF(0), "spatial/fulltext/hash index",
                 "explicit index order");
        return true;
      }
    }
  }

  Key_spec *key =
      new (*THR_MALLOC) Key_spec(pc->mem_root, key_type, to_lex_cstring(name),
                                 pc->key_create_info, false, true, *columns);
  if (key == NULL || pc->alter_info->key_list.push_back(key)) return true;

  return false;
}

Sql_cmd *PT_create_index_stmt::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  SELECT_LEX *select_lex = lex->current_select();

  thd->lex->sql_command = SQLCOM_CREATE_INDEX;

  if (select_lex->add_table_to_list(thd, m_table_ident, NULL,
                                    TL_OPTION_UPDATING, TL_READ_NO_INSERT,
                                    MDL_SHARED_UPGRADABLE) == NULL)
    return NULL;

  Table_ddl_parse_context pc(thd, select_lex, &m_alter_info);

  m_alter_info.flags = Alter_info::ALTER_ADD_INDEX;

  if (setup_index(m_keytype, m_name, m_type, m_columns, m_options, &pc))
    return NULL;

  m_alter_info.requested_algorithm = m_algo;
  m_alter_info.requested_lock = m_lock;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_create_index(&m_alter_info);
}

bool PT_inline_index_definition::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  if (setup_index(m_keytype, m_name, m_type, m_columns, m_options, pc))
    return true;

  if (m_keytype == KEYTYPE_PRIMARY && !pc->key_create_info->is_visible)
    my_error(ER_PK_INDEX_CANT_BE_INVISIBLE, MYF(0));

  return false;
}

bool PT_foreign_key_definition::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *const thd = pc->thd;
  LEX *const lex = thd->lex;

  LEX_CSTRING db;
  LEX_CSTRING orig_db;

  if (m_referenced_table->db.str) {
    orig_db = m_referenced_table->db;

    if (check_db_name(orig_db.str, orig_db.length) != Ident_name_check::OK)
      return true;

    if (lower_case_table_names) {
      char *db_str = thd->strmake(orig_db.str, orig_db.length);
      if (db_str == nullptr) return true;  // OOM
      db.length = my_casedn_str(files_charset_info, db_str);
      db.str = db_str;
    } else
      db = orig_db;
  } else {
    /*
      Before 8.0 foreign key metadata was handled by SEs and they
      assumed that parent table belongs to the same database as
      child table unless FQTN was used (and connection's current
      database was ignored). We keep behavior compatible even
      though this is inconsistent with interpretation of non-FQTN
      table names in other contexts.

      If this is ALTER TABLE with RENAME TO <db_name.table_name>
      clause we need to use name of the target database.
    */
    if (pc->alter_info->new_db_name.str) {
      db = orig_db = pc->alter_info->new_db_name;
    } else {
      TABLE_LIST *child_table = lex->select_lex->get_table_list();
      db = orig_db = LEX_CSTRING{child_table->db, child_table->db_length};
    }
  }

  Ident_name_check ident_check_status = check_table_name(
      m_referenced_table->table.str, m_referenced_table->table.length);
  if (ident_check_status != Ident_name_check::OK) {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), m_referenced_table->table.str);
    return true;
  }

  LEX_CSTRING table_name;

  if (lower_case_table_names) {
    char *table_name_str = thd->strmake(m_referenced_table->table.str,
                                        m_referenced_table->table.length);
    if (table_name_str == nullptr) return true;  // OOM
    table_name.length = my_casedn_str(files_charset_info, table_name_str);
    table_name.str = table_name_str;
  } else
    table_name = m_referenced_table->table;

  lex->key_create_info = default_key_create_info;

  /*
    If defined, the CONSTRAINT symbol value is used.
    Otherwise, the FOREIGN KEY index_name value is used.
  */
  const LEX_CSTRING used_name = to_lex_cstring(
      m_constraint_name.str ? m_constraint_name
                            : m_key_name.str ? m_key_name : NULL_STR);

  if (used_name.str && check_string_char_length(used_name, "", NAME_CHAR_LEN,
                                                system_charset_info, 1)) {
    my_error(ER_TOO_LONG_IDENT, MYF(0), used_name.str);
    return true;
  }

  Key_spec *foreign_key = new (*THR_MALLOC)
      Foreign_key_spec(thd->mem_root, used_name, *m_columns, db, orig_db,
                       table_name, m_referenced_table->table, m_ref_list,
                       m_fk_delete_opt, m_fk_update_opt, m_fk_match_option);
  if (foreign_key == NULL || pc->alter_info->key_list.push_back(foreign_key))
    return true;
  /* Only used for ALTER TABLE. Ignored otherwise. */
  pc->alter_info->flags |= Alter_info::ADD_FOREIGN_KEY;

  Key_spec *key = new (*THR_MALLOC)
      Key_spec(thd->mem_root, KEYTYPE_MULTIPLE, used_name,
               &default_key_create_info, true, true, *m_columns);
  if (key == NULL || pc->alter_info->key_list.push_back(key)) return true;

  return false;
}

bool PT_with_list::push_back(PT_common_table_expr *el) {
  const LEX_STRING &n = el->name();
  for (auto previous : m_elements) {
    const LEX_STRING &pn = previous->name();
    if (pn.length == n.length && !memcmp(pn.str, n.str, n.length)) {
      my_error(ER_NONUNIQ_TABLE, MYF(0), n.str);
      return true;
    }
  }
  return m_elements.push_back(el);
}

PT_common_table_expr::PT_common_table_expr(
    const LEX_STRING &name, const LEX_STRING &subq_text, uint subq_text_offs,
    PT_subquery *subq_node, const Create_col_name_list *column_names,
    MEM_ROOT *mem_root)
    : m_name(name),
      m_subq_text(subq_text),
      m_subq_text_offset(subq_text_offs),
      m_subq_node(subq_node),
      m_column_names(*column_names),
      m_postparse(mem_root) {
  if (lower_case_table_names && m_name.length)
    // Lowercase name, as in SELECT_LEX::add_table_to_list()
    m_name.length = my_casedn_str(files_charset_info, m_name.str);
}

void PT_with_clause::print(THD *thd, String *str, enum_query_type query_type) {
  size_t len1 = str->length();
  str->append("with ");
  if (m_recursive) str->append("recursive ");
  size_t len2 = str->length(), len3 = len2;
  for (auto el : m_list->elements()) {
    if (str->length() != len3) {
      str->append(", ");
      len3 = str->length();
    }
    el->print(thd, str, query_type);
  }
  if (str->length() == len2)
    str->length(len1);  // don't print an empty WITH clause
  else
    str->append(" ");
}

void PT_common_table_expr::print(THD *thd, String *str,
                                 enum_query_type query_type) {
  size_t len = str->length();
  append_identifier(thd, str, m_name.str, m_name.length);
  if (m_column_names.size())
    print_derived_column_names(thd, str, &m_column_names);
  str->append(" as ");

  /*
    Printing the raw text (this->m_subq_text) would lack:
    - expansion of '||' (which can mean CONCAT or OR, depending on
    sql_mode's PIPES_AS_CONCAT (the effect would be that a view containing
    a CTE containing '||' would change behaviour if sql_mode was
    changed between its creation and its usage).
    - quoting of table identifiers
    - expansion of the default db.
    So, we rather locate one resolved query expression for this CTE; for
    it to be intact this query expression must be non-merged. And we print
    it.
    If query expression has been merged everywhere, its SELECT_LEX_UNIT is
    gone and printing this CTE can be skipped. Note that when we print the
    view's body to the data dictionary, no merging is done.
  */
  bool found = false;
  for (auto *tl : m_postparse.references) {
    if (!tl->is_merged() &&
        // If 2+ references exist, show the one which is shown in EXPLAIN
        tl->query_block_id_for_explain() == tl->query_block_id()) {
      str->append('(');
      tl->derived_unit()->print(str, query_type);
      str->append(')');
      found = true;
      break;
    }
  }
  if (!found) str->length(len);  // don't print a useless CTE definition
}

bool PT_create_table_engine_option::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  pc->create_info->used_fields |= HA_CREATE_USED_ENGINE;
  const bool is_temp_table = pc->create_info->options & HA_LEX_CREATE_TMP_TABLE;
  return resolve_engine(pc->thd, engine, is_temp_table, false,
                        &pc->create_info->db_type);
}

bool PT_create_stats_auto_recalc_option::contextualize(
    Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  switch (value) {
    case Ternary_option::ON:
      pc->create_info->stats_auto_recalc = HA_STATS_AUTO_RECALC_ON;
      break;
    case Ternary_option::OFF:
      pc->create_info->stats_auto_recalc = HA_STATS_AUTO_RECALC_OFF;
      break;
    case Ternary_option::DEFAULT:
      pc->create_info->stats_auto_recalc = HA_STATS_AUTO_RECALC_DEFAULT;
      break;
    default:
      DBUG_ASSERT(false);
  }
  pc->create_info->used_fields |= HA_CREATE_USED_STATS_AUTO_RECALC;
  return false;
}

bool PT_create_stats_stable_pages::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  pc->create_info->stats_sample_pages = value;
  pc->create_info->used_fields |= HA_CREATE_USED_STATS_SAMPLE_PAGES;
  return false;
}

bool PT_create_union_option::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  THD *const thd = pc->thd;
  LEX *const lex = thd->lex;
  const Yacc_state *yyps = &thd->m_parser_state->m_yacc;

  TABLE_LIST **exclude_merge_engine_tables = lex->query_tables_last;
  SQL_I_List<TABLE_LIST> save_list;
  lex->select_lex->table_list.save_and_clear(&save_list);
  if (pc->select->add_tables(thd, tables, TL_OPTION_UPDATING, yyps->m_lock_type,
                             yyps->m_mdl_type))
    return true;
  /*
    Move the union list to the merge_list and exclude its tables
    from the global list.
  */
  pc->create_info->merge_list = lex->select_lex->table_list;
  lex->select_lex->table_list = save_list;
  /*
    When excluding union list from the global list we assume that
    elements of the former immediately follow elements which represent
    table being created/altered and parent tables.
  */
  DBUG_ASSERT(*exclude_merge_engine_tables ==
              pc->create_info->merge_list.first);
  *exclude_merge_engine_tables = nullptr;
  lex->query_tables_last = exclude_merge_engine_tables;

  pc->create_info->used_fields |= HA_CREATE_USED_UNION;
  return false;
}

bool set_default_charset(HA_CREATE_INFO *create_info,
                         const CHARSET_INFO *value) {
  DBUG_ASSERT(value != nullptr);

  if ((create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
      create_info->default_table_charset &&
      !my_charset_same(create_info->default_table_charset, value)) {
    my_error(ER_CONFLICTING_DECLARATIONS, MYF(0), "CHARACTER SET ",
             create_info->default_table_charset->csname, "CHARACTER SET ",
             value->csname);
    return true;
  }
  create_info->default_table_charset = value;
  create_info->used_fields |= HA_CREATE_USED_DEFAULT_CHARSET;
  return false;
}

bool PT_create_table_default_charset::contextualize(
    Table_ddl_parse_context *pc) {
  return (super::contextualize(pc) ||
          set_default_charset(pc->create_info, value));
}

bool set_default_collation(HA_CREATE_INFO *create_info,
                           const CHARSET_INFO *value) {
  DBUG_ASSERT(value != nullptr);

  if ((create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
      create_info->default_table_charset &&
      !(value = merge_charset_and_collation(create_info->default_table_charset,
                                            value))) {
    return true;
  }

  create_info->default_table_charset = value;
  create_info->used_fields |= HA_CREATE_USED_DEFAULT_CHARSET;
  create_info->used_fields |= HA_CREATE_USED_DEFAULT_COLLATE;
  return false;
}

bool PT_create_table_default_collation::contextualize(
    Table_ddl_parse_context *pc) {
  return (super::contextualize(pc) ||
          set_default_collation(pc->create_info, value));
}

bool PT_locking_clause::contextualize(Parse_context *pc) {
  LEX *lex = pc->thd->lex;

  if (lex->is_explain()) return false;

  if (m_locked_row_action == Locked_row_action::SKIP)
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SKIP_LOCKED);

  if (m_locked_row_action == Locked_row_action::NOWAIT)
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_NOWAIT);

  lex->safe_to_cache_query = false;

  return set_lock_for_tables(pc);
}

bool PT_query_block_locking_clause::set_lock_for_tables(Parse_context *pc) {
  Local_tables_list local_tables(pc->select->table_list.first);
  for (TABLE_LIST *table_list : local_tables)
    if (!table_list->is_derived()) {
      if (table_list->lock_descriptor().type != TL_READ_DEFAULT) {
        my_error(ER_DUPLICATE_TABLE_LOCK, MYF(0), table_list->alias);
        return true;
      }

      pc->select->set_lock_for_table(get_lock_descriptor(), table_list);
    }
  return false;
}

bool PT_column_def::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc) || field_def->contextualize(pc) ||
      contextualize_safe(pc, opt_column_constraint))
    return true;

  pc->alter_info->flags |= field_def->alter_info_flags;
  return pc->alter_info->add_field(
      pc->thd, &field_ident, field_def->type, field_def->length, field_def->dec,
      field_def->type_flags, field_def->default_value,
      field_def->on_update_value, &field_def->comment, NULL,
      field_def->interval_list, field_def->charset,
      field_def->has_explicit_collation, field_def->uint_geom_type,
      field_def->gcol_info, opt_place, field_def->m_srid);
}

Sql_cmd *PT_create_table_stmt::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  lex->sql_command = SQLCOM_CREATE_TABLE;

  Parse_context pc(thd, lex->current_select());

  TABLE_LIST *table = pc.select->add_table_to_list(
      thd, table_name, NULL, TL_OPTION_UPDATING, TL_WRITE, MDL_SHARED);
  if (table == NULL) return NULL;

  table->open_strategy = TABLE_LIST::OPEN_FOR_CREATE;

  lex->create_info = &m_create_info;
  Table_ddl_parse_context pc2(thd, pc.select, &m_alter_info);

  pc2.create_info->options = 0;
  if (is_temporary) pc2.create_info->options |= HA_LEX_CREATE_TMP_TABLE;
  if (only_if_not_exists)
    pc2.create_info->options |= HA_LEX_CREATE_IF_NOT_EXISTS;

  pc2.create_info->default_table_charset = NULL;

  lex->name.str = 0;
  lex->name.length = 0;

  TABLE_LIST *qe_tables = nullptr;

  if (opt_like_clause != NULL) {
    pc2.create_info->options |= HA_LEX_CREATE_TABLE_LIKE;
    TABLE_LIST **like_clause_table = &lex->query_tables->next_global;
    TABLE_LIST *src_table = pc.select->add_table_to_list(
        thd, opt_like_clause, NULL, 0, TL_READ, MDL_SHARED_READ);
    if (!src_table) return NULL;
    /* CREATE TABLE ... LIKE is not allowed for views. */
    src_table->required_type = dd::enum_table_type::BASE_TABLE;
    qe_tables = *like_clause_table;
  } else {
    if (opt_table_element_list) {
      for (auto element : *opt_table_element_list) {
        if (element->contextualize(&pc2)) return NULL;
      }
    }

    if (opt_create_table_options) {
      for (auto option : *opt_create_table_options)
        if (option->contextualize(&pc2)) return NULL;
    }

    if (opt_partitioning) {
      TABLE_LIST **exclude_part_tables = lex->query_tables_last;
      if (opt_partitioning->contextualize(&pc)) return NULL;
      /*
        Remove all tables used in PARTITION clause from the global table
        list. Partitioning with subqueries is not allowed anyway.
      */
      *exclude_part_tables = nullptr;
      lex->query_tables_last = exclude_part_tables;

      lex->part_info = &opt_partitioning->part_info;
    }

    switch (on_duplicate) {
      case On_duplicate::IGNORE_DUP:
        lex->set_ignore(true);
        break;
      case On_duplicate::REPLACE_DUP:
        lex->duplicates = DUP_REPLACE;
        break;
      case On_duplicate::ERROR:
        lex->duplicates = DUP_ERROR;
        break;
    }

    if (opt_query_expression) {
      TABLE_LIST **query_expression_tables = &lex->query_tables->next_global;
      /*
        In CREATE TABLE t ... SELECT the table_list initially contains
        here a table entry for the destination table `t'.
        Backup it and clean the table list for the processing of
        the query expression and push `t' back to the beginning of the
        table_list finally.

        @todo: Don't save the CREATE destination table in
               SELECT_LEX::table_list and remove this backup & restore.

        The following work only with the local list, the global list
        is created correctly in this case
      */
      SQL_I_List<TABLE_LIST> save_list;
      SELECT_LEX *const save_select = pc.select;
      save_select->table_list.save_and_clear(&save_list);

      if (opt_query_expression->contextualize(&pc)) return NULL;

      /*
        The following work only with the local list, the global list
        is created correctly in this case
      */
      save_select->table_list.push_front(&save_list);
      qe_tables = *query_expression_tables;
    }
  }

  lex->set_current_select(pc.select);
  if ((pc2.create_info->used_fields & HA_CREATE_USED_ENGINE) &&
      !pc2.create_info->db_type) {
    pc2.create_info->db_type =
        pc2.create_info->options & HA_LEX_CREATE_TMP_TABLE
            ? ha_default_temp_handlerton(thd)
            : ha_default_handlerton(thd);
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_WARN_USING_OTHER_HANDLER,
        ER_THD(thd, ER_WARN_USING_OTHER_HANDLER),
        ha_resolve_storage_engine_name(pc2.create_info->db_type),
        table_name->table.str);
  }
  create_table_set_open_action_and_adjust_tables(lex);

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_create_table(&m_alter_info, qe_tables);
}

bool PT_table_locking_clause::set_lock_for_tables(Parse_context *pc) {
  DBUG_ASSERT(!m_tables.empty());
  for (Table_ident *table_ident : m_tables) {
    SELECT_LEX *select = pc->select;

    TABLE_LIST *table_list = select->find_table_by_name(table_ident);

    THD *thd = pc->thd;

    if (table_list == NULL)
      return raise_error(thd, table_ident, ER_UNRESOLVED_TABLE_LOCK);

    if (table_list->lock_descriptor().type != TL_READ_DEFAULT)
      return raise_error(thd, table_ident, ER_DUPLICATE_TABLE_LOCK);

    select->set_lock_for_table(get_lock_descriptor(), table_list);
  }

  return false;
}

Sql_cmd *PT_show_fields_and_keys::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  Parse_context pc(thd, lex->current_select());

  // Create empty query block and add user specfied table.
  TABLE_LIST **query_tables_last = lex->query_tables_last;
  SELECT_LEX *schema_select_lex = lex->new_empty_query_block();
  if (schema_select_lex == nullptr) return NULL;
  TABLE_LIST *tbl = schema_select_lex->add_table_to_list(
      thd, m_table_ident, 0, 0, TL_READ, MDL_SHARED_READ);
  if (tbl == nullptr) return NULL;
  lex->query_tables_last = query_tables_last;

  if (m_wild.str && lex->set_wild(m_wild)) return NULL;  // OOM

  // If its a temporary table then use schema_table implementation.
  if (find_temporary_table(thd, tbl) != nullptr) {
    SELECT_LEX *select_lex = lex->current_select();

    if (m_where_condition != nullptr) {
      m_where_condition->itemize(&pc, &m_where_condition);
      select_lex->set_where_cond(m_where_condition);
    }

    enum enum_schema_tables schema_table =
        (m_type == SHOW_FIELDS) ? SCH_TMP_TABLE_COLUMNS : SCH_TMP_TABLE_KEYS;
    if (make_schema_select(thd, select_lex, schema_table)) return NULL;

    TABLE_LIST *table_list = select_lex->table_list.first;
    table_list->schema_select_lex = schema_select_lex;
    table_list->schema_table_reformed = 1;
  } else  // Use implementation of I_S as system views.
  {
    SELECT_LEX *sel = nullptr;
    switch (m_type) {
      case SHOW_FIELDS:
        sel = dd::info_schema::build_show_columns_query(
            m_pos, thd, m_table_ident, lex->wild, m_where_condition);
        break;
      case SHOW_KEYS:
        sel = dd::info_schema::build_show_keys_query(m_pos, thd, m_table_ident,
                                                     m_where_condition);
        break;
    }

    if (sel == nullptr) return NULL;

    TABLE_LIST *table_list = sel->table_list.first;
    table_list->schema_select_lex = schema_select_lex;
  }

  return &m_sql_cmd;
}

static void setup_lex_show_cmd_type(THD *thd, Show_cmd_type show_cmd_type) {
  thd->lex->verbose = false;
  thd->lex->m_extended_show = false;

  switch (show_cmd_type) {
    case Show_cmd_type::STANDARD:
      break;
    case Show_cmd_type::FULL_SHOW:
      thd->lex->verbose = true;
      break;
    case Show_cmd_type::EXTENDED_SHOW:
      thd->lex->m_extended_show = true;
      break;
    case Show_cmd_type::EXTENDED_FULL_SHOW:
      thd->lex->verbose = true;
      thd->lex->m_extended_show = true;
      break;
    default:
      DBUG_ASSERT(false);
  }
}

Sql_cmd *PT_show_fields::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  lex->select_lex->db = nullptr;

  setup_lex_show_cmd_type(thd, m_show_cmd_type);
  lex->current_select()->parsing_place = CTX_SELECT_LIST;
  lex->sql_command = SQLCOM_SHOW_FIELDS;
  Sql_cmd *ret = super::make_cmd(thd);
  if (ret == nullptr) return nullptr;
  // WL#6599 opt_describe_column is handled during prepare stage in
  // prepare_schema_dd_view instead of execution stage
  lex->current_select()->parsing_place = CTX_NONE;

  return ret;
}

Sql_cmd *PT_show_keys::make_cmd(THD *thd) {
  thd->lex->m_extended_show = m_extended_show;
  return super::make_cmd(thd);
}

bool PT_alter_table_change_column::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc) || m_field_def->contextualize(pc)) return true;
  if (m_field_def->default_value)
    pc->alter_info->flags |= Alter_info::ALTER_CHANGE_COLUMN_DEFAULT;
  pc->alter_info->flags |= m_field_def->alter_info_flags;
  return pc->alter_info->add_field(
      pc->thd, &m_new_name, m_field_def->type, m_field_def->length,
      m_field_def->dec, m_field_def->type_flags, m_field_def->default_value,
      m_field_def->on_update_value, &m_field_def->comment, m_old_name.str,
      m_field_def->interval_list, m_field_def->charset,
      m_field_def->has_explicit_collation, m_field_def->uint_geom_type,
      m_field_def->gcol_info, m_opt_place, m_field_def->m_srid);
}

bool PT_alter_table_rename::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;  // OOM

  if (m_ident->db.str) {
    LEX_STRING db_str = to_lex_string(m_ident->db);
    if (check_and_convert_db_name(&db_str, false) != Ident_name_check::OK)
      return true;
    pc->alter_info->new_db_name = to_lex_cstring(db_str);
  } else if (pc->thd->lex->copy_db_to(&pc->alter_info->new_db_name.str,
                                      &pc->alter_info->new_db_name.length)) {
    return true;
  }
  switch (check_table_name(m_ident->table.str, m_ident->table.length)) {
    case Ident_name_check::WRONG:
      my_error(ER_WRONG_TABLE_NAME, MYF(0), m_ident->table.str);
      return true;
    case Ident_name_check::TOO_LONG:
      my_error(ER_TOO_LONG_IDENT, MYF(0), m_ident->table.str);
      return true;
    case Ident_name_check::OK:
      break;
  }
  pc->alter_info->new_table_name = m_ident->table;
  return false;
}

bool PT_alter_table_convert_to_charset::contextualize(
    Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;  // OOM

  const CHARSET_INFO *const cs =
      m_charset ? m_charset : pc->thd->variables.collation_database;
  const CHARSET_INFO *const collation = m_collation ? m_collation : cs;

  if (!my_charset_same(cs, collation)) {
    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0), collation->name,
             cs->csname);
    return true;
  }

  if ((pc->create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
      pc->create_info->default_table_charset && collation &&
      !my_charset_same(pc->create_info->default_table_charset, collation)) {
    my_error(ER_CONFLICTING_DECLARATIONS, MYF(0), "CHARACTER SET ",
             pc->create_info->default_table_charset->csname, "CHARACTER SET ",
             collation->csname);
    return true;
  }

  pc->create_info->table_charset = pc->create_info->default_table_charset =
      collation;
  pc->create_info->used_fields |=
      HA_CREATE_USED_CHARSET | HA_CREATE_USED_DEFAULT_CHARSET;
  if (m_collation != nullptr)
    pc->create_info->used_fields |= HA_CREATE_USED_DEFAULT_COLLATE;
  return false;
}

bool PT_alter_table_add_partition_def_list::contextualize(
    Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  Partition_parse_context part_pc(pc->thd, &m_part_info,
                                  is_add_or_reorganize_partition());
  for (auto *part_def : *m_def_list) {
    if (part_def->contextualize(&part_pc)) return true;
  }
  m_part_info.num_parts = m_part_info.partitions.elements;

  return false;
}

bool PT_alter_table_reorganize_partition_into::contextualize(
    Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  LEX *const lex = pc->thd->lex;
  lex->no_write_to_binlog = m_no_write_to_binlog;

  DBUG_ASSERT(pc->alter_info->partition_names.is_empty());
  pc->alter_info->partition_names = m_partition_names;

  Partition_parse_context ppc(pc->thd, &m_partition_info,
                              is_add_or_reorganize_partition());

  for (auto *part_def : *m_into)
    if (part_def->contextualize(&ppc)) return true;

  m_partition_info.num_parts = m_partition_info.partitions.elements;
  lex->part_info = &m_partition_info;
  return false;
}

bool PT_alter_table_exchange_partition::contextualize(
    Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  pc->alter_info->with_validation = m_validation;

  String *s = new (pc->mem_root) String(
      m_partition_name.str, m_partition_name.length, system_charset_info);
  if (s == nullptr || pc->alter_info->partition_names.push_back(s) ||
      !pc->select->add_table_to_list(pc->thd, m_table_name, NULL,
                                     TL_OPTION_UPDATING, TL_READ_NO_INSERT,
                                     MDL_SHARED_NO_WRITE)) {
    return true;
  }

  return false;
}

/**
  A common initialization part of ALTER TABLE statement variants.

  @param pc             The parse context.
  @param table_name     The name of a table to alter.
  @param algo           The ALGORITHM clause: inplace, copy etc.
  @param lock           The LOCK clause: none, shared, exclusive etc.
  @param validation     The WITH or WITHOUT VALIDATION clause.

  @returns false on success, true on error.

*/
static bool init_alter_table_stmt(Table_ddl_parse_context *pc,
                                  Table_ident *table_name,
                                  Alter_info::enum_alter_table_algorithm algo,
                                  Alter_info::enum_alter_table_lock lock,
                                  Alter_info::enum_with_validation validation) {
  LEX *lex = pc->thd->lex;
  if (!lex->select_lex->add_table_to_list(pc->thd, table_name, NULL,
                                          TL_OPTION_UPDATING, TL_READ_NO_INSERT,
                                          MDL_SHARED_UPGRADABLE))
    return true;
  lex->select_lex->init_order();
  pc->create_info->db_type = 0;
  pc->create_info->default_table_charset = NULL;
  pc->create_info->row_type = ROW_TYPE_NOT_USED;

  pc->alter_info->new_db_name =
      LEX_CSTRING{lex->select_lex->table_list.first->db,
                  lex->select_lex->table_list.first->db_length};
  lex->no_write_to_binlog = 0;
  pc->create_info->storage_media = HA_SM_DEFAULT;

  pc->alter_info->requested_algorithm = algo;
  pc->alter_info->requested_lock = lock;
  pc->alter_info->with_validation = validation;
  return false;
}

Sql_cmd *PT_alter_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ALTER_TABLE;

  thd->lex->create_info = &m_create_info;
  Table_ddl_parse_context pc(thd, thd->lex->current_select(), &m_alter_info);

  if (init_alter_table_stmt(&pc, m_table_name, m_algo, m_lock, m_validation))
    return NULL;

  if (m_opt_actions) {
    /*
      Move RENAME TO <table_name> clauses to the head of array, so they are
      processed before ADD FOREIGN KEY clauses. The latter need to know target
      database name for proper contextualization.

      Use stable sort to preserve order of other clauses which might be
      sensitive to it.
    */
    std::stable_sort(
        m_opt_actions->begin(), m_opt_actions->end(),
        [](const PT_ddl_table_option *lhs, const PT_ddl_table_option *rhs) {
          return lhs->is_rename_table() && !rhs->is_rename_table();
        });

    for (auto *action : *m_opt_actions)
      if (action->contextualize(&pc)) return NULL;
  }

  if ((pc.create_info->used_fields & HA_CREATE_USED_ENGINE) &&
      !pc.create_info->db_type) {
    pc.create_info->used_fields &= ~HA_CREATE_USED_ENGINE;
  }

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_alter_table(&m_alter_info);
}

Sql_cmd *PT_alter_table_standalone_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ALTER_TABLE;

  thd->lex->create_info = &m_create_info;

  Table_ddl_parse_context pc(thd, thd->lex->current_select(), &m_alter_info);
  if (init_alter_table_stmt(&pc, m_table_name, m_algo, m_lock, m_validation) ||
      m_action->contextualize(&pc))
    return NULL;

  thd->lex->alter_info = &m_alter_info;
  return m_action->make_cmd(&pc);
}

Sql_cmd *PT_repair_table_stmt::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  lex->sql_command = SQLCOM_REPAIR;

  SELECT_LEX *const select = lex->current_select();

  lex->no_write_to_binlog = m_no_write_to_binlog;
  lex->check_opt.init();
  lex->check_opt.flags |= m_flags;
  lex->check_opt.sql_flags |= m_sql_flags;
  if (select->add_tables(thd, m_table_list, TL_OPTION_UPDATING, TL_UNLOCK,
                         MDL_SHARED_READ))
    return NULL;

  lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_repair_table(&m_alter_info);
}

Sql_cmd *PT_analyze_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ANALYZE;

  LEX *const lex = thd->lex;
  SELECT_LEX *const select = lex->current_select();

  lex->no_write_to_binlog = m_no_write_to_binlog;
  lex->check_opt.init();
  if (select->add_tables(thd, m_table_list, TL_OPTION_UPDATING, TL_UNLOCK,
                         MDL_SHARED_READ))
    return NULL;

  thd->lex->alter_info = &m_alter_info;
  auto cmd = new (thd->mem_root)
      Sql_cmd_analyze_table(thd, &m_alter_info, m_command, m_num_buckets);
  if (cmd == NULL) return NULL;
  if (m_command != Sql_cmd_analyze_table::Histogram_command::NONE) {
    if (cmd->set_histogram_fields(m_columns)) return NULL;
  }
  return cmd;
}

Sql_cmd *PT_check_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_CHECK;

  LEX *const lex = thd->lex;
  SELECT_LEX *const select = lex->current_select();

  if (lex->sphead) {
    my_error(ER_SP_BADSTATEMENT, MYF(0), "CHECK");
    return NULL;
  }

  lex->check_opt.init();
  lex->check_opt.flags |= m_flags;
  lex->check_opt.sql_flags |= m_sql_flags;
  if (select->add_tables(thd, m_table_list, TL_OPTION_UPDATING, TL_UNLOCK,
                         MDL_SHARED_READ))
    return NULL;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_check_table(&m_alter_info);
}

Sql_cmd *PT_optimize_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_OPTIMIZE;

  LEX *const lex = thd->lex;
  SELECT_LEX *const select = lex->current_select();

  lex->no_write_to_binlog = m_no_write_to_binlog;
  lex->check_opt.init();
  if (select->add_tables(thd, m_table_list, TL_OPTION_UPDATING, TL_UNLOCK,
                         MDL_SHARED_READ))
    return NULL;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_optimize_table(&m_alter_info);
}

Sql_cmd *PT_drop_index_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_DROP_INDEX;

  LEX *const lex = thd->lex;
  SELECT_LEX *const select = lex->current_select();

  m_alter_info.flags = Alter_info::ALTER_DROP_INDEX;
  m_alter_info.drop_list.push_back(&m_alter_drop);
  if (!select->add_table_to_list(thd, m_table, NULL, TL_OPTION_UPDATING,
                                 TL_READ_NO_INSERT, MDL_SHARED_UPGRADABLE))
    return NULL;

  m_alter_info.requested_algorithm = m_algo;
  m_alter_info.requested_lock = m_lock;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_drop_index(&m_alter_info);
}

Sql_cmd *PT_truncate_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_TRUNCATE;

  LEX *const lex = thd->lex;
  SELECT_LEX *const select = lex->current_select();

  if (!select->add_table_to_list(thd, m_table, NULL, TL_OPTION_UPDATING,
                                 TL_WRITE, MDL_EXCLUSIVE))
    return NULL;
  return &m_cmd_truncate_table;
}

bool PT_assign_to_keycache::contextualize(Table_ddl_parse_context *pc) {
  if (super::contextualize(pc)) return true;

  if (!pc->select->add_table_to_list(pc->thd, m_table, NULL, 0, TL_READ,
                                     MDL_SHARED_READ, m_index_hints))
    return true;
  return false;
}

bool PT_adm_partition::contextualize(Table_ddl_parse_context *pc) {
  pc->alter_info->flags |= Alter_info::ALTER_ADMIN_PARTITION;

  DBUG_ASSERT(pc->alter_info->partition_names.is_empty());
  if (m_opt_partitions == NULL)
    pc->alter_info->flags |= Alter_info::ALTER_ALL_PARTITION;
  else
    pc->alter_info->partition_names = *m_opt_partitions;
  return false;
}

Sql_cmd *PT_cache_index_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ASSIGN_TO_KEYCACHE;

  Table_ddl_parse_context pc(thd, thd->lex->current_select(), &m_alter_info);

  for (auto *tbl_index_list : *m_tbl_index_lists)
    if (tbl_index_list->contextualize(&pc)) return NULL;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root)
      Sql_cmd_cache_index(&m_alter_info, m_key_cache_name);
}

Sql_cmd *PT_cache_index_partitions_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ASSIGN_TO_KEYCACHE;

  SELECT_LEX *const select = thd->lex->current_select();

  Table_ddl_parse_context pc(thd, select, &m_alter_info);

  if (m_partitions->contextualize(&pc)) return NULL;

  if (!select->add_table_to_list(thd, m_table, NULL, 0, TL_READ,
                                 MDL_SHARED_READ, m_opt_key_usage_list))
    return NULL;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root)
      Sql_cmd_cache_index(&m_alter_info, m_key_cache_name);
}

Sql_cmd *PT_load_index_partitions_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_PRELOAD_KEYS;

  SELECT_LEX *const select = thd->lex->current_select();

  Table_ddl_parse_context pc(thd, select, &m_alter_info);

  if (m_partitions->contextualize(&pc)) return NULL;

  if (!select->add_table_to_list(
          thd, m_table, NULL, m_ignore_leaves ? TL_OPTION_IGNORE_LEAVES : 0,
          TL_READ, MDL_SHARED_READ, m_opt_cache_key_list))
    return NULL;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_load_index(&m_alter_info);
}

Sql_cmd *PT_load_index_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_PRELOAD_KEYS;

  Table_ddl_parse_context pc(thd, thd->lex->current_select(), &m_alter_info);

  for (auto *preload_keys : *m_preload_list)
    if (preload_keys->contextualize(&pc)) return NULL;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_load_index(&m_alter_info);
}

/* Window functions */

Item *PT_border::build_addop(Item_cache *order_expr, bool prec, bool asc,
                             const Window *window) {
  /*
    Check according to SQL 2014 7.15 <window clause> SR 13.a.iii:
    ORDER BY expression is temporal iff bound is temporal.
  */
  if (order_expr->result_type() == STRING_RESULT && order_expr->is_temporal()) {
    if (!m_date_time) {
      my_error(ER_WINDOW_RANGE_FRAME_TEMPORAL_TYPE, MYF(0),
               window->printable_name());
      return nullptr;
    }
  } else {
    if (m_date_time) {
      my_error(ER_WINDOW_RANGE_FRAME_NUMERIC_TYPE, MYF(0),
               window->printable_name());
      return nullptr;
    }
  }

  Item *addop;
  const bool substract = prec ? asc : !asc;
  if (m_date_time) {
    addop =
        new Item_date_add_interval(order_expr, m_value, m_int_type, substract);
  } else {
    if (substract)
      addop = new Item_func_minus(order_expr, m_value);
    else
      addop = new Item_func_plus(order_expr, m_value);
  }
  return addop;
}

bool PT_window::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  if (m_partition_by != NULL) {
    if (m_partition_by->contextualize(pc)) return true;
  }

  if (m_order_by != NULL) {
    if (m_order_by->contextualize(pc)) return true;
  }

  if (m_frame != NULL) {
    for (auto bound : {m_frame->m_from, m_frame->m_to}) {
      if (bound->m_border_type == WBT_VALUE_PRECEDING ||
          bound->m_border_type == WBT_VALUE_FOLLOWING) {
        auto **bound_i_ptr = bound->border_ptr();
        if ((*bound_i_ptr)->itemize(pc, bound_i_ptr)) return true;
      }
    }
  }

  return false;
}

bool PT_window_list::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;

  uint count = pc->select->m_windows.elements;
  List_iterator<Window> wi(m_windows);
  Window *w;
  while ((w = wi++)) {
    static_cast<PT_window *>(w)->contextualize(pc);
    w->set_def_pos(++count);
  }

  SELECT_LEX *select = pc->select;
  select->m_windows.prepend(&m_windows);

  return false;
}

Sql_cmd *PT_show_tables::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = SQLCOM_SHOW_TABLES;

  lex->select_lex->db = m_opt_db;
  setup_lex_show_cmd_type(thd, m_show_cmd_type);

  if (m_wild.str && lex->set_wild(m_wild)) return NULL;  // OOM

  SELECT_LEX *sel = dd::info_schema::build_show_tables_query(
      m_pos, thd, lex->wild, m_where_condition, false);
  if (sel == nullptr) return NULL;

  return &m_sql_cmd;
}

bool PT_json_table_column_with_path::contextualize(Parse_context *pc) {
  if (super::contextualize(pc) || m_type->contextualize(pc)) return true;

  const CHARSET_INFO *cs = m_type->get_charset()
                               ? m_type->get_charset()
                               : global_system_variables.character_set_results;

  m_column.init(pc->thd,
                m_name,                        // Alias
                m_type->type,                  // Type
                m_type->get_length(),          // Length
                m_type->get_dec(),             // Decimals
                m_type->get_type_flags(),      // Type modifier
                nullptr,                       // Default value
                nullptr,                       // On update value
                &EMPTY_STR,                    // Comment
                nullptr,                       // Change
                m_type->get_interval_list(),   // Interval list
                cs,                            // Charset
                false,                         // No "COLLATE" clause
                m_type->get_uint_geom_type(),  // Geom type
                NULL,                          // Gcol_info
                {});                           // SRID
  return false;
}

bool PT_json_table_column_with_nested_path::contextualize(Parse_context *pc) {
  if (super::contextualize(pc)) return true;  // OOM

  auto nested_columns = new (pc->mem_root) List<Json_table_column>;
  if (nested_columns == nullptr) return true;  // OOM

  for (auto col : *m_nested_columns) {
    if (col->contextualize(pc) || nested_columns->push_back(col->get_column()))
      return true;
  }

  m_column = new (pc->mem_root) Json_table_column(m_path, nested_columns);
  if (m_column == nullptr) return true;  // OOM

  return false;
}

Sql_cmd *PT_explain_for_connection::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_EXPLAIN_OTHER;

  if (thd->lex->sphead) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "non-standalone EXPLAIN FOR CONNECTION");
    return nullptr;
  }
  return &m_cmd;
}

Sql_cmd *PT_explain::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  switch (m_format) {
    case Explain_format_type::TRADITIONAL:
      lex->explain_format = new (thd->mem_root) Explain_format_traditional;
      break;
    case Explain_format_type::JSON:
      lex->explain_format = new (thd->mem_root) Explain_format_JSON;
      break;
  }
  if (lex->explain_format == nullptr) return nullptr;  // OOM

  Sql_cmd *ret = m_explainable_stmt->make_cmd(thd);
  if (ret == nullptr) return nullptr;  // OOM

  auto code = ret->sql_command_code();
  if (!is_explainable_query(code) && code != SQLCOM_EXPLAIN_OTHER) {
    DBUG_ASSERT(!"Should not happen!");
    my_error(ER_WRONG_USAGE, MYF(0), "EXPLAIN", "non-explainable query");
    return nullptr;
  }

  return ret;
}

Sql_cmd *PT_load_table::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  SELECT_LEX *const select = lex->current_select();

  if (lex->sphead) {
    my_error(
        ER_SP_BADSTATEMENT, MYF(0),
        m_cmd.m_exchange.filetype == FILETYPE_CSV ? "LOAD DATA" : "LOAD XML");
    return nullptr;
  }

  lex->sql_command = SQLCOM_LOAD;

  switch (m_cmd.m_on_duplicate) {
    case On_duplicate::ERROR:
      lex->duplicates = DUP_ERROR;
      break;
    case On_duplicate::IGNORE_DUP:
      lex->set_ignore(true);
      break;
    case On_duplicate::REPLACE_DUP:
      lex->duplicates = DUP_REPLACE;
      break;
  }

  /* Fix lock for LOAD DATA CONCURRENT REPLACE */
  thr_lock_type lock_type = m_lock_type;
  if (lex->duplicates == DUP_REPLACE && lock_type == TL_WRITE_CONCURRENT_INSERT)
    lock_type = TL_WRITE_DEFAULT;

  if (!select->add_table_to_list(
          thd, m_cmd.m_table, nullptr, TL_OPTION_UPDATING, lock_type,
          lock_type == TL_WRITE_LOW_PRIORITY ? MDL_SHARED_WRITE_LOW_PRIO
                                             : MDL_SHARED_WRITE,
          nullptr, m_cmd.m_opt_partitions))
    return nullptr;

  /* We can't give an error in the middle when using LOCAL files */
  if (m_cmd.m_is_local_file && lex->duplicates == DUP_ERROR)
    lex->set_ignore(true);

  Parse_context pc(thd, select);
  if (contextualize_safe(&pc, m_opt_fields_or_vars)) return nullptr;

  if (m_opt_set_fields != nullptr) {
    if (m_opt_set_fields->contextualize(&pc) ||
        m_opt_set_exprs->contextualize(&pc))
      return nullptr;
  }

  return &m_cmd;
}
