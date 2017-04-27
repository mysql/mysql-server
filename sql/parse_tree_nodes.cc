/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "parse_tree_nodes.h"

#include <string.h>

#include "dd/info_schema/show.h"             // build_show_...
#include "dd/types/abstract_table.h" // dd::enum_table_type::BASE_TABLE
#include "derror.h"         // ER_THD
#include "key_spec.h"
#include "m_string.h"
#include "mdl.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "mysqld.h"         // global_system_variables
#include "parse_tree_column_attrs.h" // PT_field_def_base
#include "parse_tree_hints.h"
#include "parse_tree_partitions.h" // PT_partition
#include "prealloced_array.h"
#include "query_options.h"
#include "session_tracker.h"
#include "sp.h"             // sp_add_used_routine
#include "sp_instr.h"       // sp_instr_set
#include "sp_pcontext.h"
#include "sql_base.h"                        // find_temporary_table
#include "sql_call.h"       // Sql_cmd_call...
#include "sql_data_change.h"
#include "sql_delete.h"     // Sql_cmd_delete...
#include "sql_do.h"         // Sql_cmd_do...
#include "sql_error.h"
#include "sql_insert.h"     // Sql_cmd_insert...
#include "sql_select.h"     // Sql_cmd_select...
#include "sql_string.h"
#include "sql_update.h"     // Sql_cmd_update...
#include "system_variables.h"
#include "trigger_def.h"

class Sql_cmd;


PT_joined_table *PT_table_reference::add_cross_join(PT_cross_join* cj)
{
  cj->add_rhs(this);
  return cj;
}


/**
  Gcc can't or won't allow a pure virtual destructor without an implementation.
*/
PT_joined_table::~PT_joined_table() {}


bool PT_option_value_no_option_type_charset:: contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  int flags= opt_charset ? 0 : set_var_collation_client::SET_CS_DEFAULT;
  const CHARSET_INFO *cs2;
  cs2= opt_charset ? opt_charset
    : global_system_variables.character_set_client;
  set_var_collation_client *var;
  var= new set_var_collation_client(flags,
                                    cs2,
                                    thd->variables.collation_database,
                                    cs2);
  if (var == NULL)
    return true;
  lex->var_list.push_back(var);
  return false;
}


bool PT_option_value_no_option_type_names::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
  LEX_STRING names= { C_STRING_WITH_LEN("names") };

  if (pctx && pctx->find_variable(names, false))
    my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), names.str);
  else
    error(pc, pos);

  return true; // alwais fails with an error
}


bool 
PT_option_value_no_option_type_names_charset:: contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  const CHARSET_INFO *cs2;
  const CHARSET_INFO *cs3;
  int flags= set_var_collation_client::SET_CS_NAMES
    | (opt_charset ? 0 : set_var_collation_client::SET_CS_DEFAULT)
    | (opt_collation ? set_var_collation_client::SET_CS_COLLATE : 0);
  cs2= opt_charset ? opt_charset 
    : global_system_variables.character_set_client;
  cs3= opt_collation ? opt_collation : cs2;
  if (!my_charset_same(cs2, cs3))
  {
    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
             cs3->name, cs2->csname);
    return true;
  }
  set_var_collation_client *var;
  var= new set_var_collation_client(flags, cs3, cs3, cs3);
  if (var == NULL)
    return true;
  lex->var_list.push_back(var);
  return false;
}


bool PT_group::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  SELECT_LEX *select= pc->select;
  select->parsing_place= CTX_GROUP_BY;

  if (group_list->contextualize(pc))
    return true;
  DBUG_ASSERT(select == pc->select);

  select->group_list= group_list->value;

  // Ensure we're resetting parsing place of the right select
  DBUG_ASSERT(select->parsing_place == CTX_GROUP_BY);
  select->parsing_place= CTX_NONE;

  switch (olap) {
  case UNSPECIFIED_OLAP_TYPE:
    break;
  case ROLLUP_TYPE:
    if (select->linkage == GLOBAL_OPTIONS_TYPE)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "WITH ROLLUP",
               "global union parameters");
      return true;
    }
    if (select->is_distinct())
    {
      // DISTINCT+ROLLUP does not work
      my_error(ER_WRONG_USAGE, MYF(0), "WITH ROLLUP", "DISTINCT");
      return true;
    }
    select->olap= ROLLUP_TYPE;
    break;
  default:
    DBUG_ASSERT(!"unexpected OLAP type!");
  }
  return false;
}


bool PT_order::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  SELECT_LEX_UNIT * const unit= pc->select->master_unit();
  const bool braces= pc->select->braces;

  if (pc->select->linkage != GLOBAL_OPTIONS_TYPE &&
      pc->select->olap != UNSPECIFIED_OLAP_TYPE &&
      (pc->select->linkage != UNION_TYPE || braces))
  {
    my_error(ER_WRONG_USAGE, MYF(0),
             "CUBE/ROLLUP", "ORDER BY");
    return true;
  }
  if (lex->sql_command != SQLCOM_ALTER_TABLE && !unit->fake_select_lex)
  {
    /*
      A query of the of the form (SELECT ...) ORDER BY order_list is
      executed in the same way as the query
      SELECT ... ORDER BY order_list
      unless the SELECT construct contains ORDER BY or LIMIT clauses.
      Otherwise we create a fake SELECT_LEX if it has not been created
      yet.
    */
    SELECT_LEX *first_sl= unit->first_select();
    if (!unit->is_union() &&
        (first_sl->order_list.elements || first_sl->select_limit))
    {
      if (unit->add_fake_select_lex(lex->thd))
        return true;
      pc->select= unit->fake_select_lex;
    }
  }

  bool context_is_pushed= false;
  if (pc->select->parsing_place == CTX_NONE)
  {
    if (unit->is_union() && !braces)
    {
      /*
        At this point we don't know yet whether this is the last
        select in union or not, but we move ORDER BY to
        fake_select_lex anyway. If there would be one more select
        in union mysql_new_select will correctly throw error.
      */
      pc->select= unit->fake_select_lex;
      lex->push_context(&pc->select->context);
      context_is_pushed= true;
    }
    /*
      To preserve correct markup for the case 
       SELECT group_concat(... ORDER BY (subquery))
      we do not change parsing_place if it's not NONE.
    */
    pc->select->parsing_place= CTX_ORDER_BY;
  }

  if (order_list->contextualize(pc))
    return true;

  if (context_is_pushed)
    lex->pop_context();

  pc->select->order_list= order_list->value;

  // Reset parsing place only for ORDER BY
  if (pc->select->parsing_place == CTX_ORDER_BY)
    pc->select->parsing_place= CTX_NONE;
  return false;
}


bool PT_internal_variable_name_1d::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
  sp_variable *spv;

  value.var= NULL;
  value.base_name= ident;

  /* Best effort lookup for system variable. */
  if (!pctx || !(spv= pctx->find_variable(ident, false)))
  {
    /* Not an SP local variable */
    if (find_sys_var_null_base(thd, &value))
      return true;
  }
  else
  {
    /*
      Possibly an SP local variable (or a shadowed sysvar).
      Will depend on the context of the SET statement.
    */
  }
  return false;
}


bool PT_internal_variable_name_2d::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  sp_head *sp= lex->sphead;

  if (check_reserved_words(&ident1))
  {
    error(pc, pos);
    return true;
  }

  if (sp && sp->m_type == enum_sp_type::TRIGGER &&
      (!my_strcasecmp(system_charset_info, ident1.str, "NEW") ||
       !my_strcasecmp(system_charset_info, ident1.str, "OLD")))
  {
    if (ident1.str[0]=='O' || ident1.str[0]=='o')
    {
      my_error(ER_TRG_CANT_CHANGE_ROW, MYF(0), "OLD", "");
      return true;
    }
    if (sp->m_trg_chistics.event == TRG_EVENT_DELETE)
    {
      my_error(ER_TRG_NO_SUCH_ROW_IN_TRG, MYF(0),
               "NEW", "on DELETE");
      return true;
    }
    if (sp->m_trg_chistics.action_time == TRG_ACTION_AFTER)
    {
      my_error(ER_TRG_CANT_CHANGE_ROW, MYF(0), "NEW", "after ");
      return true;
    }
    /* This special combination will denote field of NEW row */
    value.var= trg_new_row_fake_var;
    value.base_name= ident2;
  }
  else
  {
    sys_var *tmp=find_sys_var(thd, ident2.str, ident2.length);
    if (!tmp)
      return true;
    if (!tmp->is_struct())
      my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), ident2.str);
    value.var= tmp;
    value.base_name= ident1;
  }
  return false;
}


bool PT_option_value_no_option_type_internal::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc) || name->contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  sp_head *sp= lex->sphead;

  if (opt_expr != NULL && opt_expr->itemize(pc, &opt_expr))
    return true;

  const char *expr_start_ptr= NULL;

  if (sp)
    expr_start_ptr= expr_pos.raw.start;

  if (name->value.var == trg_new_row_fake_var)
  {
    DBUG_ASSERT(sp);
    DBUG_ASSERT(expr_start_ptr);

    /* We are parsing trigger and this is a trigger NEW-field. */

    LEX_STRING expr_query= EMPTY_STR;

    if (!opt_expr)
    {
      // This is: SET NEW.x = DEFAULT
      // DEFAULT clause is not supported in triggers.

      error(pc, expr_pos);
      return true;
    }
    else if (lex->is_metadata_used())
    {
      expr_query= make_string(thd, expr_start_ptr, expr_pos.raw.end);

      if (!expr_query.str)
        return true;
    }

    if (set_trigger_new_row(pc, name->value.base_name, opt_expr, expr_query))
      return true;
  }
  else if (name->value.var)
  {
    /* We're not parsing SP and this is a system variable. */

    if (set_system_variable(thd, &name->value, lex->option_type, opt_expr))
      return true;
  }
  else
  {
    DBUG_ASSERT(sp);
    DBUG_ASSERT(expr_start_ptr);

    /* We're parsing SP and this is an SP-variable. */

    sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
    sp_variable *spv= pctx->find_variable(name->value.base_name, false);

    LEX_STRING expr_query= EMPTY_STR;

    if (!opt_expr)
    {
      /*
        This is: SET x = DEFAULT, where x is a SP-variable.
        This is not supported.
      */

      error(pc, expr_pos);
      return true;
    }
    else if (lex->is_metadata_used())
    {
      expr_query= make_string(thd, expr_start_ptr, expr_pos.raw.end);

      if (!expr_query.str)
        return true;
    }

    /*
      NOTE: every SET-expression has its own LEX-object, even if it is
      a multiple SET-statement, like:

        SET spv1 = expr1, spv2 = expr2, ...

      Every SET-expression has its own sp_instr_set. Thus, the
      instruction owns the LEX-object, i.e. the instruction is
      responsible for destruction of the LEX-object.
    */

    sp_instr_set *i=
      new sp_instr_set(sp->instructions(), lex,
                       spv->offset, opt_expr, expr_query,
                       true); // The instruction owns its lex.

    if (!i || sp->add_instr(thd, i))
      return true;
  }
  return false;
}


bool PT_option_value_no_option_type_password::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  sp_head *sp= lex->sphead;
  sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
  LEX_STRING pw= { C_STRING_WITH_LEN("password") };

  if (pctx && pctx->find_variable(pw, false))
  {
    my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), pw.str);
    return true;
  }

  LEX_USER *user= (LEX_USER*) thd->alloc(sizeof(LEX_USER));

  if (!user)
    return true;

  LEX_CSTRING sctx_user= thd->security_context()->user();
  user->user.str= (char *) sctx_user.str;
  user->user.length= sctx_user.length;

  LEX_CSTRING sctx_priv_host= thd->security_context()->priv_host();
  DBUG_ASSERT(sctx_priv_host.str);
  user->host.str= (char *) sctx_priv_host.str;
  user->host.length= sctx_priv_host.length;

  set_var_password *var= new set_var_password(user,
                                              const_cast<char *>(password));
  if (var == NULL)
    return true;

  lex->var_list.push_back(var);
  lex->sql_command= SQLCOM_SET_PASSWORD;

  if (sp)
    sp->m_flags|= sp_head::HAS_SET_AUTOCOMMIT_STMT;

  if (sp_create_assignment_instr(pc->thd, expr_pos.raw.end))
    return true;

  return false;
}


bool PT_select_sp_var::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  LEX *lex= pc->thd->lex;
#ifndef DBUG_OFF
  sp= lex->sphead;
#endif
  sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
  sp_variable *spv;

  if (!pctx || !(spv= pctx->find_variable(name, false)))
  {
    my_error(ER_SP_UNDECLARED_VAR, MYF(0), name.str);
    return true;
  }

  offset= spv->offset;

  return false;
}


Sql_cmd *PT_select_stmt::make_cmd(THD *thd)
{
  Parse_context pc(thd, thd->lex->current_select());
  if (contextualize(&pc))
    return NULL;

  if (thd->lex->sql_command == SQLCOM_SELECT)
    return new (thd->mem_root) Sql_cmd_select(thd->lex->result);
  else // (thd->lex->sql_command == SQLCOM_DO)
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

static TABLE_LIST *multi_delete_table_match(TABLE_LIST *tbl, TABLE_LIST *tables)
{
  TABLE_LIST *match= NULL;
  DBUG_ENTER("multi_delete_table_match");

  for (TABLE_LIST *elem= tables; elem; elem= elem->next_local)
  {
    int cmp;

    if (tbl->is_fqtn && elem->is_alias)
      continue; /* no match */
    if (tbl->is_fqtn && elem->is_fqtn)
      cmp= my_strcasecmp(table_alias_charset, tbl->table_name, elem->table_name) ||
           strcmp(tbl->db, elem->db);
    else if (elem->is_alias)
      cmp= my_strcasecmp(table_alias_charset, tbl->alias, elem->alias);
    else
      cmp= my_strcasecmp(table_alias_charset, tbl->table_name, elem->table_name) ||
           strcmp(tbl->db, elem->db);

    if (cmp)
      continue;

    if (match)
    {
      my_error(ER_NONUNIQ_TABLE, MYF(0), elem->alias);
      DBUG_RETURN(NULL);
    }

    match= elem;
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
                                     SQL_I_List<TABLE_LIST> *delete_tables)
{
  DBUG_ENTER("multi_delete_link_tables");

  TABLE_LIST *tables= pc->select->table_list.first;

  for (TABLE_LIST *target_tbl= delete_tables->first;
       target_tbl; target_tbl= target_tbl->next_local)
  {
    /* All tables in aux_tables must be found in FROM PART */
    TABLE_LIST *walk= multi_delete_table_match(target_tbl, tables);
    if (!walk)
      DBUG_RETURN(true);
    if (!walk->is_derived())
    {
      target_tbl->table_name= walk->table_name;
      target_tbl->table_name_length= walk->table_name_length;
    }
    walk->updating= target_tbl->updating;
    walk->set_lock(target_tbl->lock_descriptor());
    /* We can assume that tables to be deleted from are locked for write. */
    DBUG_ASSERT(walk->lock_descriptor().type >= TL_WRITE_ALLOW_WRITE);
    walk->mdl_request.set_type(mdl_type_for_dml(walk->lock_descriptor().type));
    target_tbl->correspondent_table= walk;	// Remember corresponding table
  }
  DBUG_RETURN(false);
}


bool PT_delete::add_table(Parse_context *pc, Table_ident *table)
{
  const ulong table_opts= is_multitable() ? TL_OPTION_UPDATING | TL_OPTION_ALIAS
                                          : TL_OPTION_UPDATING;
  const thr_lock_type lock_type=
    (opt_delete_options & DELETE_LOW_PRIORITY) ? TL_WRITE_LOW_PRIORITY
                                               : TL_WRITE_DEFAULT;
  const enum_mdl_type mdl_type=
    (opt_delete_options & DELETE_LOW_PRIORITY) ? MDL_SHARED_WRITE_LOW_PRIO
                                               : MDL_SHARED_WRITE;
   return !pc->select->add_table_to_list(pc->thd, table, NULL, table_opts,
                                         lock_type, mdl_type, NULL,
                                         opt_use_partition);
}

bool PT_delete::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  LEX * const lex= pc->thd->lex;
  SELECT_LEX *const select= pc->select;

  DBUG_ASSERT(lex->select_lex == select);
  lex->sql_command= is_multitable() ? SQLCOM_DELETE_MULTI : SQLCOM_DELETE;
  lex->set_ignore(MY_TEST(opt_delete_options & DELETE_IGNORE));
  select->init_order();
  if (opt_delete_options & DELETE_QUICK)
    select->add_base_options(OPTION_QUICK);

  if (contextualize_safe(pc, m_with_clause))
    return true;                                /* purecov: inspected */

  if (is_multitable())
  {
    for (Table_ident **i= table_list.begin(); i != table_list.end(); ++i)
    {
      if (add_table(pc, *i))
        return true;
    }
  }
  else if (add_table(pc, table_ident))
    return true;

  if (is_multitable())
  {
    select->table_list.save_and_clear(&delete_tables);
    lex->query_tables= NULL;
    lex->query_tables_last= &lex->query_tables;
  }
  else
  {
    select->top_join_list.push_back(select->get_table_list());
  }
  Yacc_state * const yyps= &pc->thd->m_parser_state->m_yacc;
  yyps->m_lock_type= TL_READ_DEFAULT;
  yyps->m_mdl_type= MDL_SHARED_READ;

  if (is_multitable())
  {
    if (contextualize_array(pc, &join_table_list))
      return true;
    pc->select->context.table_list=
      pc->select->context.first_name_resolution_table=
        pc->select->table_list.first;
  }

  if (opt_where_clause != NULL &&
      opt_where_clause->itemize(pc, &opt_where_clause))
    return true;
  select->set_where_cond(opt_where_clause);

  if (opt_order_clause != NULL && opt_order_clause->contextualize(pc))
    return true;

  DBUG_ASSERT(select->select_limit == NULL);
  if (opt_delete_limit_clause != NULL)
  {
    if (opt_delete_limit_clause->itemize(pc, &opt_delete_limit_clause))
      return true;
    select->select_limit= opt_delete_limit_clause;
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
    select->explicit_limit= true;
  }

  if (is_multitable() && multi_delete_link_tables(pc, &delete_tables))
    return true;

  if (opt_hints != NULL && opt_hints->contextualize(pc))
    return true;

  return false;
}


Sql_cmd *PT_delete::make_cmd(THD *thd)
{
  Parse_context pc(thd, thd->lex->current_select());
  if (contextualize(&pc))
    return NULL;
  return new (thd->mem_root) Sql_cmd_delete(is_multitable(), &delete_tables);
}


bool PT_update::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  LEX *lex= pc->thd->lex;
  SELECT_LEX *const select= pc->select;
  DBUG_ASSERT(select == lex->select_lex);

  lex->sql_command= SQLCOM_UPDATE;
  lex->duplicates= DUP_ERROR;

  lex->set_ignore(opt_ignore);

  if (contextualize_safe(pc, m_with_clause))
    return true;                                /* purecov: inspected */

  if (contextualize_array(pc, &join_table_list))
    return true;
  select->parsing_place= CTX_UPDATE_VALUE_LIST;

  if (column_list->contextualize(pc) ||
      value_list->contextualize(pc))
  {
    return true;
  }
  select->item_list= column_list->value;

  // Ensure we're resetting parsing context of the right select
  DBUG_ASSERT(select->parsing_place == CTX_UPDATE_VALUE_LIST);
  select->parsing_place= CTX_NONE;
  multitable= select->table_list.elements > 1;
  lex->sql_command= multitable ? SQLCOM_UPDATE_MULTI : SQLCOM_UPDATE;

  /*
    In case of multi-update setting write lock for all tables may
    be too pessimistic. We will decrease lock level if possible in
    mysql_multi_update().
  */
  select->set_lock_for_tables(opt_low_priority);

  if (opt_where_clause != NULL &&
      opt_where_clause->itemize(pc, &opt_where_clause))
  {
    return true;
  }
  select->set_where_cond(opt_where_clause);

  if (opt_order_clause != NULL && opt_order_clause->contextualize(pc))
    return true;

  DBUG_ASSERT(select->select_limit == NULL);
  if (opt_limit_clause != NULL)
  {
    if (opt_limit_clause->itemize(pc, &opt_limit_clause))
      return true;
    select->select_limit= opt_limit_clause;
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
    select->explicit_limit= true;
  }

  if (opt_hints != NULL && opt_hints->contextualize(pc))
    return true;

  return false;
}


Sql_cmd *PT_update::make_cmd(THD *thd)
{
  Parse_context pc(thd, thd->lex->current_select());
  if (contextualize(&pc))
    return NULL;

  return new (thd->mem_root) Sql_cmd_update(is_multitable(),
                                            &value_list->value);
}


bool PT_insert_values_list::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;
  List_iterator<List_item> it1(many_values);
  List<Item> *item_list;
  while ((item_list= it1++))
  {
    List_iterator<Item> it2(*item_list);
    Item *item;
    while ((item= it2++))
    {
      if (item->itemize(pc, &item))
        return true;
      it2.replace(item);
    }
  }

  return false;
}


bool PT_insert::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  LEX * const lex= pc->thd->lex;

  if (is_replace)
  {
    lex->sql_command = has_select() ? SQLCOM_REPLACE_SELECT : SQLCOM_REPLACE;
    lex->duplicates= DUP_REPLACE;
  }
  else
  {
    lex->sql_command= has_select() ? SQLCOM_INSERT_SELECT : SQLCOM_INSERT;
    lex->duplicates= DUP_ERROR;
    lex->set_ignore(ignore);
  }

  Yacc_state *yyps= &pc->thd->m_parser_state->m_yacc;
  if (!pc->select->add_table_to_list(pc->thd, table_ident, NULL,
                                     TL_OPTION_UPDATING,
                                     yyps->m_lock_type,
                                     yyps->m_mdl_type,
                                     NULL,
                                     opt_use_partition))
  {
    return true;
  }
  pc->select->set_lock_for_tables(lock_option);

  DBUG_ASSERT(lex->current_select() == lex->select_lex);

  if (column_list->contextualize(pc))
    return true;

  if (has_select())
  {
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
    SELECT_LEX * const save_select= pc->select;
    save_select->table_list.save_and_clear(&save_list);

    if (insert_query_expression->contextualize(pc))
      return true;

    /*
      The following work only with the local list, the global list
      is created correctly in this case
    */
    save_select->table_list.push_front(&save_list);

    lex->bulk_insert_row_cnt= 0;
  }
  else
  {
    if (row_value_list->contextualize(pc))
      return true;
    lex->bulk_insert_row_cnt= row_value_list->get_many_values().elements;
  }

  if (opt_on_duplicate_column_list != NULL)
  {
    DBUG_ASSERT(!is_replace);
    DBUG_ASSERT(opt_on_duplicate_value_list != NULL &&
                opt_on_duplicate_value_list->elements() ==
                  opt_on_duplicate_column_list->elements());

    lex->duplicates= DUP_UPDATE;
    TABLE_LIST *first_table= lex->select_lex->table_list.first;
    /* Fix lock for ON DUPLICATE KEY UPDATE */
    if (first_table->lock_descriptor().type == TL_WRITE_CONCURRENT_DEFAULT)
      first_table->set_lock({TL_WRITE_DEFAULT, THR_DEFAULT});

    pc->select->parsing_place= CTX_UPDATE_VALUE_LIST;

    if (opt_on_duplicate_column_list->contextualize(pc) ||
        opt_on_duplicate_value_list->contextualize(pc))
      return true;

    // Ensure we're resetting parsing context of the right select
    DBUG_ASSERT(pc->select->parsing_place == CTX_UPDATE_VALUE_LIST);
    pc->select->parsing_place= CTX_NONE;
  }

  if (opt_hints != NULL && opt_hints->contextualize(pc))
    return true;

  return false;
}


Sql_cmd *PT_insert::make_cmd(THD *thd)
{
  Parse_context pc(thd, thd->lex->current_select());
  if (contextualize(&pc))
    return NULL;

  Sql_cmd_insert_base *sql_cmd;
  if (has_select())
    sql_cmd= new (thd->mem_root) Sql_cmd_insert_select(is_replace,
                                                       thd->lex->duplicates);
  else
    sql_cmd= new (thd->mem_root) Sql_cmd_insert_values(is_replace,
                                                       thd->lex->duplicates);
  if (sql_cmd == NULL)
    return NULL;

  if (!has_select())
    sql_cmd->insert_many_values= row_value_list->get_many_values();

  sql_cmd->insert_field_list= column_list->value;
  if (opt_on_duplicate_column_list != NULL)
  {
    DBUG_ASSERT(!is_replace);
    sql_cmd->update_field_list= opt_on_duplicate_column_list->value;
    sql_cmd->update_value_list= opt_on_duplicate_value_list->value;
  }

  return sql_cmd;
}


bool PT_call::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;                 /* purecov: inspected */

  THD *const thd= pc->thd;
  LEX *const lex= thd->lex;

  if (opt_expr_list != NULL && opt_expr_list->contextualize(pc))
    return true;                 /* purecov: inspected */

  lex->sql_command= SQLCOM_CALL;

  sp_add_own_used_routine(lex, thd, Sroutine_hash_entry::PROCEDURE, proc_name);

  return false;
}


Sql_cmd *PT_call::make_cmd(THD *thd)
{
  Parse_context pc(thd, thd->lex->current_select());
  if (contextualize(&pc))
    return NULL;
  List<Item> *proc_args= NULL;
  if (opt_expr_list != NULL)
    proc_args= &opt_expr_list->value;

  return new (thd->mem_root) Sql_cmd_call(proc_name, proc_args);
}


bool PT_query_specification::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  pc->select->parsing_place= CTX_SELECT_LIST;

  if (options.query_spec_options & SELECT_HIGH_PRIORITY)
  {
    Yacc_state *yyps= &pc->thd->m_parser_state->m_yacc;
    yyps->m_lock_type= TL_READ_HIGH_PRIORITY;
    yyps->m_mdl_type= MDL_SHARED_READ;
  }
  if (options.save_to(pc))
    return true;

  if (item_list->contextualize(pc))
    return true;

  // Ensure we're resetting parsing place of the right select
  DBUG_ASSERT(pc->select->parsing_place == CTX_SELECT_LIST);
  pc->select->parsing_place= CTX_NONE;

  if (contextualize_safe(pc, opt_into1))
    return true;

  if (!from_clause.empty())
  {
    if (contextualize_array(pc, &from_clause))
      return true;
    pc->select->context.table_list=
      pc->select->context.first_name_resolution_table=
        pc->select->table_list.first;
  }

  if (itemize_safe(pc, &opt_where_clause) ||
      contextualize_safe(pc, opt_group_clause) ||
      itemize_safe(pc, &opt_having_clause))
    return true;

  pc->select->set_where_cond(opt_where_clause);
  pc->select->set_having_cond(opt_having_clause);

  if (opt_hints != NULL)
  {
    if (pc->thd->lex->sql_command == SQLCOM_CREATE_VIEW)
    { // Currently this also affects ALTER VIEW.
      push_warning_printf(pc->thd, Sql_condition::SL_WARNING,
                          ER_WARN_UNSUPPORTED_HINT,
                          ER_THD(pc->thd, ER_WARN_UNSUPPORTED_HINT),
                          "CREATE or ALTER VIEW");
    }
    else if (opt_hints->contextualize(pc))
      return true;
  }
  return false;
}


PT_derived_table::PT_derived_table(PT_subquery *subquery,
                                   LEX_STRING *table_alias,
                                   Create_col_name_list *column_names)
  : m_subquery(subquery),
    m_table_alias(table_alias),
    column_names(*column_names)
{
  m_subquery->m_is_derived_table= true;
}


bool PT_derived_table::contextualize(Parse_context *pc)
{
  SELECT_LEX *outer_select= pc->select;

  outer_select->parsing_place= CTX_DERIVED;
  DBUG_ASSERT(outer_select->linkage != GLOBAL_OPTIONS_TYPE);

  if (m_subquery->contextualize(pc))
    return true;

  outer_select->parsing_place= CTX_NONE;

  DBUG_ASSERT(pc->select->next_select() == NULL);

  SELECT_LEX_UNIT *unit= pc->select->first_inner_unit();
  pc->select= outer_select;
  Table_ident *ti= new Table_ident(unit);
  if (ti == NULL)
    return true;

  value= pc->select->add_table_to_list(pc->thd,
                                       ti, m_table_alias, 0,
                                       TL_READ, MDL_SHARED_READ);
  if (value == NULL)
    return true;
  if (column_names.size())
    value->set_derived_column_names(&column_names);
  if (pc->select->add_joined_table(value))
    return true;

  return false;
}


bool PT_table_factor_joined_table::contextualize(Parse_context *pc)
{
  if (Parse_tree_node::contextualize(pc))
    return true;

  SELECT_LEX *outer_select= pc->select;
  if (outer_select->init_nested_join(pc->thd))
    return true;

  if (m_joined_table->contextualize(pc))
    return true;
  value= m_joined_table->value;

  if (outer_select->end_nested_join() == nullptr)
    return true;

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
bool PT_union::contextualize(Parse_context *pc)
{
  THD *thd= pc->thd;

  if (PT_query_expression_body::contextualize(pc))
    return true;

  if (m_lhs->contextualize(pc))
    return true;

  pc->select=
    pc->thd->lex->new_union_query(pc->select, m_is_distinct, false);

  if (pc->select == NULL || m_rhs->contextualize(pc))
    return true;

  SELECT_LEX_UNIT *unit= pc->select->master_unit();
  if (unit->fake_select_lex == NULL && unit->add_fake_select_lex(thd))
    return true;

  SELECT_LEX *select_lex= pc->select;
  pc->select= unit->fake_select_lex;
  pc->select->no_table_names_allowed= true;

  if (m_containing_qe != NULL &&
      m_containing_qe->contextualize_order_and_limit(pc))
    return true;

  pc->select->no_table_names_allowed= false;
  pc->select= select_lex;

  pc->thd->lex->pop_context();

  return false;
}

/**
  @brief
  make_cmd for PT_alter_instance.
  Contextualize parse tree node and return sql_cmd handle.

  @param [in] thd Thread handle

  @returns
    sql_cmd Success
    NULL    Failure
*/

Sql_cmd *PT_alter_instance::make_cmd(THD *thd)
{
  Parse_context pc(thd, thd->lex->current_select());
  if (contextualize(&pc))
    return NULL;
  return &sql_cmd;
}

/**
  @brief
  Prepare parse tree node and set required information

  @param [in] pc Parser context

  @returns
    false Success
    true Error
*/

bool PT_alter_instance::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  LEX *lex= pc->thd->lex;
  lex->no_write_to_binlog= false;

  return false;
}


static bool setup_index(keytype key_type,
                        const LEX_STRING name,
                        PT_base_index_option *type,
                        List<Key_part_spec> *columns,
                        Index_options options,
                        Index_options lock_and_algorithm_options,
                        Parse_context *pc)
{
  THD *thd= pc->thd;
  LEX *lex= thd->lex;

  lex->key_create_info= default_key_create_info;

  if (type != NULL && type->contextualize(pc))
    return true;

  if (contextualize_nodes(options, pc) ||
      contextualize_nodes(lock_and_algorithm_options, pc))
    return true;

  if ((key_type == KEYTYPE_FULLTEXT || key_type == KEYTYPE_SPATIAL ||
       lex->key_create_info.algorithm == HA_KEY_ALG_HASH))
  {
    List_iterator<Key_part_spec> li(*columns);
    Key_part_spec *kp;
    while((kp= li++))
    {
      if (kp->is_explicit)
      {
        my_error(ER_WRONG_USAGE, MYF(0),"spatial/fulltext/hash index",
                 "explicit index order");
        return TRUE;
      }
    }
  }

  Key_spec *key=
    new Key_spec(thd->mem_root, key_type, to_lex_cstring(name),
                 &lex->key_create_info, false, true, *columns);
  if (key == NULL || lex->alter_info.key_list.push_back(key))
    return true;

  return false;
}


bool PT_index_definition_stmt::contextualize(Parse_context *pc)
{
  THD *thd= pc->thd;
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= lex->current_select();

  lex->sql_command= SQLCOM_CREATE_INDEX;

  if (m_field_ident)
  {
    if (m_field_ident->contextualize(pc))
      return true;
    m_name= m_field_ident->field_name;
  }

  if (select_lex->add_table_to_list(thd, m_table_ident, NULL,
                                    TL_OPTION_UPDATING,
                                    TL_READ_NO_INSERT,
                                    MDL_SHARED_UPGRADABLE) == NULL)
    return true;

  lex->alter_info.reset();
  lex->alter_info.flags= Alter_info::ALTER_ADD_INDEX;

  return setup_index(m_keytype, m_name, m_type, m_columns, m_options,
                     m_lock_and_algorithm_options, pc);
}


bool PT_inline_index_definition::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc) || (m_name && m_name->contextualize(pc)))
    return true;

  const LEX_STRING name= m_name ? m_name->field_name : NULL_STR;
  Index_options empty_lock_and_algorithm_options;
  empty_lock_and_algorithm_options.init(pc->thd->mem_root);
  if (setup_index(m_keytype, name, m_type, m_columns, m_options,
                  empty_lock_and_algorithm_options, pc))
    return true;

  if (m_keytype == KEYTYPE_PRIMARY &&
      !pc->thd->lex->key_create_info.is_visible)
    my_error(ER_PK_INDEX_CANT_BE_INVISIBLE, MYF(0));

  return false;
}


bool PT_foreign_key_definition::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc) ||
      (m_constraint_name && m_constraint_name->contextualize(pc)) ||
      (m_key_name && m_key_name->contextualize(pc)))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;

  lex->key_create_info= default_key_create_info;

  /*
    If defined, the CONSTRAINT symbol value is used.
    Otherwise, the FOREIGN KEY index_name value is used.
  */
  const LEX_CSTRING used_name= to_lex_cstring(
    m_constraint_name ? m_constraint_name->field_name
                      : m_key_name ? m_key_name->field_name : NULL_STR);

  Key_spec *foreign_key=
    new Foreign_key_spec(thd->mem_root,
                         used_name,
                         *m_columns,
                         m_referenced_table->db,
                         m_referenced_table->table,
                         m_ref_list,
                         m_fk_delete_opt,
                         m_fk_update_opt,
                         m_fk_match_option);
  if (foreign_key == NULL || lex->alter_info.key_list.push_back(foreign_key))
    return true;
  /* Only used for ALTER TABLE. Ignored otherwise. */
  lex->alter_info.flags|= Alter_info::ADD_FOREIGN_KEY;

  Key_spec *key=
    new Key_spec(thd->mem_root, KEYTYPE_MULTIPLE, used_name,
                 &default_key_create_info, true, true, *m_columns);
  if (key == NULL || lex->alter_info.key_list.push_back(key))
    return true;

  return false;
}


bool PT_with_list::push_back(PT_common_table_expr *el)
{
  const LEX_STRING &n= el->name();
  for (auto previous : m_elements)
  {
    const LEX_STRING &pn= previous->name();
    if (pn.length == n.length &&
        !memcmp(pn.str, n.str, n.length))
    {
      my_error(ER_NONUNIQ_TABLE, MYF(0), n.str);
      return true;
    }
  }
  return m_elements.push_back(el);
}


PT_common_table_expr::PT_common_table_expr(const LEX_STRING &name,
                                           const LEX_STRING &subq_text,
                                           uint subq_text_offs,
                                           PT_subquery *subq_node,
                                           const Create_col_name_list *column_names,
                                           MEM_ROOT *mem_root)
  : m_name(name), m_subq_text(subq_text), m_subq_text_offset(subq_text_offs),
    m_subq_node(subq_node), m_column_names(*column_names),
    m_postparse(mem_root)
{
  if (lower_case_table_names && m_name.length)
    // Lowercase name, as in SELECT_LEX::add_table_to_list()
    m_name.length= my_casedn_str(files_charset_info, m_name.str);
}


void PT_with_clause::print(THD *thd, String *str, enum_query_type query_type)
{
  size_t len1= str->length();
  str->append("with ");
  if (m_recursive)
    str->append("recursive ");
  size_t len2= str->length(), len3= len2;
  for (auto el : m_list.elements())
  {
    if (str->length() != len3)
    {
      str->append(", ");
      len3= str->length();
    }
    el->print(thd, str, query_type);
  }
  if (str->length() == len2)
    str->length(len1);                   // don't print an empty WITH clause
  else
    str->append(" ");
}


void PT_common_table_expr::print(THD *thd, String *str,
                                 enum_query_type query_type)
{
  size_t len= str->length();
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
  bool found= false;
  for (auto *tl : m_postparse.references)
  {
    if (!tl->is_merged() &&
        // If 2+ references exist, show the one which is shown in EXPLAIN
        tl->query_block_id_for_explain() == tl->query_block_id())
    {
      str->append('(');
      tl->derived_unit()->print(str, query_type);
      str->append(')');
      found= true;
      break;
    }
  }
  if (!found)
    str->length(len);          // don't print a useless CTE definition
}


bool PT_create_table_engine_option::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  HA_CREATE_INFO * const create_info= pc->thd->lex->create_info;

  create_info->used_fields|= HA_CREATE_USED_ENGINE;
  const bool is_temp_table= create_info->options & HA_LEX_CREATE_TMP_TABLE;
  return resolve_engine(pc->thd, engine, is_temp_table, false,
                        &create_info->db_type);
}


bool PT_create_stats_auto_recalc_option::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  HA_CREATE_INFO * const create_info= pc->thd->lex->create_info;

  switch (value) {
  case Ternary_option::ON:
    create_info->stats_auto_recalc= HA_STATS_AUTO_RECALC_ON;
    break;
  case Ternary_option::OFF:
    create_info->stats_auto_recalc= HA_STATS_AUTO_RECALC_OFF;
    break;
  case Ternary_option::DEFAULT:
    create_info->stats_auto_recalc= HA_STATS_AUTO_RECALC_DEFAULT;
    break;
  default:
    DBUG_ASSERT(false);
  }
  create_info->used_fields|= HA_CREATE_USED_STATS_AUTO_RECALC;
  return false;
}


bool PT_create_stats_stable_pages::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  HA_CREATE_INFO * const create_info= pc->thd->lex->create_info;

  create_info->stats_sample_pages= value;
  create_info->used_fields|= HA_CREATE_USED_STATS_SAMPLE_PAGES;
  return false;
}


bool PT_create_union_option::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD * const thd= pc->thd;
  LEX * const lex= thd->lex;
  HA_CREATE_INFO * const create_info= lex->create_info;
  const Yacc_state *yyps= &thd->m_parser_state->m_yacc;

  SQL_I_List<TABLE_LIST> save_list;
  lex->select_lex->table_list.save_and_clear(&save_list);
  if (pc->select->add_tables(thd, tables, TL_OPTION_UPDATING,
                             yyps->m_lock_type, yyps->m_mdl_type))
    return true;
  /*
    Move the union list to the merge_list and exclude its tables
    from the global list.
  */
  create_info->merge_list= lex->select_lex->table_list;
  lex->select_lex->table_list= save_list;
  /*
    When excluding union list from the global list we assume that
    elements of the former immediately follow elements which represent
    table being created/altered and parent tables.
  */
  TABLE_LIST *last_non_sel_table= lex->create_last_non_select_table;
  DBUG_ASSERT(last_non_sel_table->next_global == create_info->merge_list.first);
  last_non_sel_table->next_global= 0;
  lex->query_tables_last= &last_non_sel_table->next_global;

  create_info->used_fields|= HA_CREATE_USED_UNION;
  return false;
}


bool PT_create_table_default_charset::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  HA_CREATE_INFO * const create_info= pc->thd->lex->create_info;
  if ((create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
       create_info->default_table_charset && value &&
       !my_charset_same(create_info->default_table_charset,value))
  {
    my_error(ER_CONFLICTING_DECLARATIONS, MYF(0),
             "CHARACTER SET ", create_info->default_table_charset->csname,
             "CHARACTER SET ", value->csname);
    return true;
  }
  create_info->default_table_charset= value;
  create_info->used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
  return false;
}


bool PT_create_table_default_collation::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  HA_CREATE_INFO * const create_info= pc->thd->lex->create_info;

  if ((create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
       create_info->default_table_charset && value &&
       !(value= merge_charset_and_collation(create_info->default_table_charset,
                                            value)))
  {
    return true;
  }

  create_info->default_table_charset= value;
  create_info->used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
    return false;
}


bool PT_locking_clause::contextualize(Parse_context *pc)
{
  LEX *lex= pc->thd->lex;

  if (lex->is_explain())
    return false;

  if (m_locked_row_action == Locked_row_action::SKIP)
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SKIP_LOCKED);

  if (m_locked_row_action == Locked_row_action::NOWAIT)
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_NOWAIT);

  lex->safe_to_cache_query= false;

  return set_lock_for_tables(pc);
}


bool PT_query_block_locking_clause::set_lock_for_tables(Parse_context *pc)
{
  Local_tables_list local_tables(pc->select->table_list.first);
  for (TABLE_LIST *table_list : local_tables)
    if (!table_list->is_derived())
    {
      if (table_list->lock_descriptor().type != TL_READ_DEFAULT)
      {
        my_error(ER_DUPLICATE_TABLE_LOCK, MYF(0), table_list->alias);
        return true;
      }

      pc->select->set_lock_for_table(get_lock_descriptor(), table_list);
    }
  return false;
}


bool PT_column_def::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc) || field_ident->contextualize(pc) ||
      field_def->contextualize(pc) ||
      (opt_column_constraint && opt_column_constraint->contextualize(pc)))
    return true;

  pc->thd->lex->alter_info.flags|= field_def->alter_info_flags;
  return pc->thd->lex->alter_info.add_field(pc->thd,
                                            &field_ident->field_name,
                                            field_def->type,
                                            field_def->length,
                                            field_def->dec,
                                            field_def->type_flags,
                                            field_def->default_value,
                                            field_def->on_update_value,
                                            &field_def->comment,
                                            NULL,
                                            field_def->interval_list,
                                            field_def->charset,
                                            field_def->uint_geom_type,
                                            field_def->gcol_info,
                                            NULL);
}


bool PT_create_table_stmt::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;

  TABLE_LIST *table= pc->select->add_table_to_list(thd, table_name, NULL,
                                                   TL_OPTION_UPDATING,
                                                   TL_WRITE, MDL_SHARED);
  if (table == NULL)
    return true;
  /*
    Instruct open_table() to acquire SHARED lock to check the
    existance of table. If the table does not exist then
    it will be upgraded EXCLUSIVE MDL lock. If table exist
    then open_table() will return with an error or warning.
  */
  table->open_strategy= TABLE_LIST::OPEN_FOR_CREATE;
  lex->alter_info.reset();
  lex->create_info= thd->alloc_typed<HA_CREATE_INFO>();
  if (lex->create_info == NULL)
    return true; // OOM

  lex->create_info->options= 0;
  if (is_temporary)
    lex->create_info->options|= HA_LEX_CREATE_TMP_TABLE;
  if (only_if_not_exists)
    lex->create_info->options|= HA_LEX_CREATE_IF_NOT_EXISTS;

  lex->create_info->default_table_charset= NULL;
  lex->name.str= 0;
  lex->name.length= 0;
  lex->create_last_non_select_table= lex->last_table();

  if (opt_like_clause != NULL)
  {
    pc->thd->lex->create_info->options|= HA_LEX_CREATE_TABLE_LIKE;
    TABLE_LIST *src_table= pc->select->add_table_to_list(pc->thd,
                                                         opt_like_clause,
                                                         NULL, 0,
                                                         TL_READ,
                                                         MDL_SHARED_READ);
    if (!src_table)
      return true;
    /* CREATE TABLE ... LIKE is not allowed for views. */
    src_table->required_type= dd::enum_table_type::BASE_TABLE;
  }
  else
  {
    if (opt_table_element_list)
    {
      for (auto element : *opt_table_element_list)
      {
        if (element->contextualize(pc))
          return true;
      }
      lex->create_last_non_select_table= lex->last_table();
    }

    if (opt_create_table_options)
    {
      for (auto option : *opt_create_table_options)
        if (option->contextualize(pc))
          return true;
    }

    if (opt_partitioning)
    {
      if (opt_partitioning->contextualize(pc))
        return true;
      /*
        Remove all tables used in PARTITION clause from the global table
        list. Partitioning with subqueries is not allowed anyway.
      */
      TABLE_LIST *last_non_sel_table= lex->create_last_non_select_table;
      last_non_sel_table->next_global= 0;
      lex->query_tables_last= &last_non_sel_table->next_global;

      lex->part_info= &opt_partitioning->part_info;
    }

    switch (on_duplicate) {
    case On_duplicate::IGNORE_DUP:
      lex->set_ignore(true);
      break;
    case On_duplicate::REPLACE_DUP:
      lex->duplicates= DUP_REPLACE;
      break;
    case On_duplicate::ERROR:
      lex->duplicates= DUP_ERROR;
      break;
    }

    if (opt_query_expression)
    {
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
      SELECT_LEX * const save_select= pc->select;
      save_select->table_list.save_and_clear(&save_list);

      if (opt_query_expression->contextualize(pc))
        return true;

      /*
        The following work only with the local list, the global list
        is created correctly in this case
      */
      save_select->table_list.push_front(&save_list);
    }
  }

  lex->set_current_select(pc->select);
  if ((lex->create_info->used_fields & HA_CREATE_USED_ENGINE) &&
      !lex->create_info->db_type)
  {
    lex->create_info->db_type=
      lex->create_info->options & HA_LEX_CREATE_TMP_TABLE ?
      ha_default_temp_handlerton(thd) : ha_default_handlerton(thd);
    push_warning_printf(thd, Sql_condition::SL_WARNING,
      ER_WARN_USING_OTHER_HANDLER,
      ER_THD(thd, ER_WARN_USING_OTHER_HANDLER),
      ha_resolve_storage_engine_name(lex->create_info->db_type),
      table_name->table.str);
  }
  create_table_set_open_action_and_adjust_tables(lex);
  return false;
}


Sql_cmd *PT_create_table_stmt::make_cmd(THD *thd)
{
  thd->lex->sql_command= SQLCOM_CREATE_TABLE;
  Parse_context pc(thd, thd->lex->current_select());
  if (contextualize(&pc))
    return NULL;
  return &cmd;
}


bool PT_table_locking_clause::set_lock_for_tables(Parse_context *pc)
{
  DBUG_ASSERT(!m_tables.empty());
  for (Table_ident *table_ident : m_tables)
  {
    SELECT_LEX *select= pc->select;

    SQL_I_List<TABLE_LIST> tables= select->table_list;
    TABLE_LIST *table_list= select->find_table_by_name(table_ident);

    THD *thd= pc->thd;

    if (table_list == NULL)
      return raise_error(thd, table_ident, ER_UNRESOLVED_TABLE_LOCK);

    if (table_list->lock_descriptor().type != TL_READ_DEFAULT)
      return raise_error(thd, table_ident, ER_DUPLICATE_TABLE_LOCK);

    select->set_lock_for_table(get_lock_descriptor(), table_list);
  }

  return false;
}


Sql_cmd *PT_show_fields_and_keys::make_cmd(THD *thd)
{
  Parse_context pc(thd, thd->lex->current_select());
  if (contextualize(&pc))
    return NULL;
  return &m_sql_cmd;
}


bool PT_show_fields_and_keys::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  THD *thd= pc->thd;
  LEX *lex= thd->lex;

  // Create empty query block and add user specfied table.
  TABLE_LIST **query_tables_last= lex->query_tables_last;
  SELECT_LEX *schema_select_lex= lex->new_empty_query_block();
  if (schema_select_lex == nullptr)
    return true;
  TABLE_LIST *tbl= schema_select_lex->add_table_to_list(thd, m_table_ident,
                                                        0, 0, TL_READ,
                                                        MDL_SHARED_READ);
  if (tbl == nullptr)
    return true;
  lex->query_tables_last= query_tables_last;

  if (m_wild.str && lex->set_wild(m_wild))
    return true; // OOM

  // If its a temporary table then use schema_table implementation.
  if (find_temporary_table(thd, tbl) != nullptr)
  {
    SELECT_LEX *select_lex= lex->current_select();

    if (m_where_condition != nullptr)
    {
      m_where_condition->itemize(pc, &m_where_condition);
      select_lex->set_where_cond(m_where_condition);
    }

    enum enum_schema_tables schema_table=
      (m_type == SHOW_FIELDS) ? SCH_TMP_TABLE_COLUMNS :
                                SCH_TMP_TABLE_KEYS;
    if (make_schema_select(thd, select_lex, schema_table))
      return true;

    TABLE_LIST *table_list= select_lex->table_list.first;
    table_list->schema_select_lex= schema_select_lex;
    table_list->schema_table_reformed= 1;
  }
  else // Use implementation of I_S as system views.
  {
    SELECT_LEX *sel= nullptr;
    switch(m_type) {
    case SHOW_FIELDS:
      sel= dd::info_schema::build_show_columns_query(m_pos, thd, m_table_ident,
                                                     thd->lex->wild,
                                                     m_where_condition);
      break;
    case SHOW_KEYS:
      sel= dd::info_schema::build_show_keys_query(m_pos, thd, m_table_ident,
                                                  m_where_condition);
      break;
    }

    if (sel == nullptr)
      return true;

    TABLE_LIST *table_list= sel->table_list.first;
    table_list->schema_select_lex= schema_select_lex;
  }

  return false;
}


bool PT_show_fields::contextualize(Parse_context *pc)
{
  pc->thd->lex->verbose= false;
  pc->thd->lex->m_extended_show= false;

  switch (m_show_fields_type)
  {
  case Show_fields_type::STANDARD:
    break;
  case Show_fields_type::FULL_SHOW:
    pc->thd->lex->verbose= true;
    break;
  case Show_fields_type::EXTENDED_SHOW:
    pc->thd->lex->m_extended_show= true;
    break;
  case Show_fields_type::EXTENDED_FULL_SHOW:
    pc->thd->lex->verbose= true;
    pc->thd->lex->m_extended_show= true;
    break;
  default:
    DBUG_ASSERT(false);
  }
  return super::contextualize(pc);
}


bool PT_show_keys::contextualize(Parse_context *pc)
{
  pc->thd->lex->m_extended_show= m_extended_show;
  return super::contextualize(pc);
}
