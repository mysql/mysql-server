/* Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"
#include "parse_tree_nodes.h"

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
  case CUBE_TYPE:
    if (select->linkage == GLOBAL_OPTIONS_TYPE)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "WITH CUBE",
               "global union parameters");
      return true;
    }
    select->olap= CUBE_TYPE;
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "CUBE");
    return true;
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


bool PT_table_factor_select_sym::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  LEX * const lex= pc->thd->lex;
  SELECT_LEX *const outer_select= pc->select;

  if (! lex->parsing_options.allows_derived)
  {
    my_error(ER_VIEW_SELECT_DERIVED, MYF(0));
    return true;
  }

  if (!outer_select->embedding || outer_select->end_nested_join(pc->thd))
  {
    /* we are not in parentheses */
    error(pc, pos);
    return true;
  }
  TABLE_LIST *embedding= outer_select->embedding;
  const bool is_deeply_nested= embedding &&
                               !embedding->nested_join->join_list.elements;

  lex->derived_tables|= DERIVED_SUBQUERY;
  if (!lex->expr_allows_subselect ||
      lex->sql_command == (int)SQLCOM_PURGE)
  {
    error(pc, pos);
    return true;
  }

  outer_select->parsing_place= CTX_DERIVED;
  if (outer_select->linkage == GLOBAL_OPTIONS_TYPE)
    return true; // TODO: error(pc, pos)?

  SELECT_LEX * const child= lex->new_query(pc->select);
  if (child == NULL)
    return true;

  // Note that this current select is different from the one above
  pc->select= child;
  pc->select->linkage= DERIVED_TABLE_TYPE;
  pc->select->parsing_place= CTX_SELECT_LIST;
  outer_select->parsing_place= CTX_NONE;

  Yacc_state *yyps= &pc->thd->m_parser_state->m_yacc;

  if (select_options.query_spec_options & SELECT_HIGH_PRIORITY)
  {
    yyps->m_lock_type= TL_READ_HIGH_PRIORITY;
    yyps->m_mdl_type= MDL_SHARED_READ;
  }
  if (select_options.save_to(pc))
    return true;

  if (select_item_list->contextualize(pc))
    return true;
  DBUG_ASSERT(child == pc->select);

  // Ensure we're resetting parsing place of the right select
  DBUG_ASSERT(child->parsing_place == CTX_SELECT_LIST);
  child->parsing_place= CTX_NONE;

  if (table_expression->contextualize(pc))
    return true;

  if (is_deeply_nested)
  {
    if (child->set_braces(1))
    {
      error(pc, pos);
      return true;
    }
  }
  if (outer_select->init_nested_join(pc->thd))
    return true;

  /* incomplete derived tables return NULL, we must be
     nested in select_derived rule to be here. */
  value= NULL;

  return false;
}


bool PT_table_factor_parenthesis::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  SELECT_LEX * const outer_select= pc->select;

  if (select_derived_union->contextualize(pc))
    return true;

  /*
    Use outer_select instead of pc->select as derived table has
    altered value of pc->select.
  */
  if (!(select_derived_union->value || opt_table_alias) &&
      outer_select->embedding &&
      !outer_select->embedding->nested_join->join_list.elements)
  {
    /*
      we have a derived table (select_derived_union->value == NULL)
      but no alias,
      Since we are nested in further parentheses so we
      can pass NULL to the outer level parentheses
      Permits parsing of "((((select ...))) as xyz)"
    */
    value= 0;
  }
  else if (!select_derived_union->value)
  {
    /*
      Handle case of derived table, alias may be NULL if there
      are no outer parentheses, add_table_to_list() will throw
      error in this case
    */
    SELECT_LEX_UNIT *unit= pc->select->master_unit();
    pc->select= outer_select;
    Table_ident *ti= new Table_ident(unit);
    if (ti == NULL)
      return true;

    value= pc->select->add_table_to_list(pc->thd,
                                         ti, opt_table_alias, 0,
                                         TL_READ, MDL_SHARED_READ);
    if (value == NULL)
      return true;
    pc->select->add_joined_table(value);
    pc->thd->lex->pop_context();
  }
  else if (opt_table_alias != NULL)
  {
    /*
      Tables with or without joins within parentheses cannot
      have aliases, and we ruled out derived tables above.
    */
    error(pc, alias_pos);
    return true;
  }
  else
  {
    /* nested join: FROM (t1 JOIN t2 ...) */
    value= select_derived_union->value;
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

  if (sp && sp->m_type == SP_TYPE_TRIGGER &&
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

  if (sp)
    sp->m_parser_data.push_expr_start_ptr(expr_pos.raw.start);

  if (opt_expr != NULL && opt_expr->itemize(pc, &opt_expr))
    return true;

  const char *expr_start_ptr= NULL;

  if (sp)
    expr_start_ptr= sp->m_parser_data.pop_expr_start_ptr();

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
  lex->autocommit= true;
  lex->is_set_password_sql= true;

  if (sp)
    sp->m_flags|= sp_head::HAS_SET_AUTOCOMMIT_STMT;

  if (sp_create_assignment_instr(pc->thd, expr_pos.raw.end))
    return true;

  return false;
}
