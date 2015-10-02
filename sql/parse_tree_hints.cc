/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "parse_tree_hints.h"
#include "sql_class.h"
#include "mysqld.h"        // table_alias_charset
#include "sql_lex.h"


extern struct st_opt_hint_info opt_hint_info[];


/**
  Returns pointer to Opt_hints_global object,
  create Opt_hints object if not exist.

  @param lex   pointer to Parse_context object

  @return  pointer to Opt_hints object,
           NULL if failed to create the object
*/

static Opt_hints_global *get_global_hints(Parse_context *pc)
{
  LEX *lex= pc->thd->lex;

  if (!lex->opt_hints_global)
    lex->opt_hints_global= new Opt_hints_global(pc->thd->mem_root);
  if (lex->opt_hints_global)
    lex->opt_hints_global->set_resolved();
  return lex->opt_hints_global;
}


/**
  Returns pointer to Opt_hints_qb object
  for query block given by parse context,
  create Opt_hints_qb object if not exist.

  @param pc   pointer to Parse_context object

  @return  pointer to Opt_hints_qb object,
           NULL if failed to create the object
*/

static Opt_hints_qb *get_qb_hints(Parse_context *pc)
{

  if (pc->select->opt_hints_qb)
    return pc->select->opt_hints_qb;

  Opt_hints_global *global_hints= get_global_hints(pc);
  if (global_hints == NULL)
    return NULL;

  Opt_hints_qb *qb= new Opt_hints_qb(global_hints, pc->thd->mem_root,
                                     pc->select->select_number);
  if (qb)
  {
    global_hints->register_child(qb);
    pc->select->opt_hints_qb= qb;
    qb->set_resolved();
  }
  return qb;
}

/**
  Find existing Opt_hints_qb object, print warning
  if the query block is not found.

  @param pc          pointer to Parse_context object
  @param table_name  query block name
  @param hint        processed hint

  @return  pointer to Opt_hints_table object if found,
           NULL otherwise
*/

static Opt_hints_qb *find_qb_hints(Parse_context *pc,
                                   const LEX_CSTRING *qb_name,
                                   PT_hint *hint)
{
  if (qb_name->length == 0) // no QB NAME is used
    return pc->select->opt_hints_qb;

  Opt_hints_qb *qb= static_cast<Opt_hints_qb *>
    (pc->thd->lex->opt_hints_global->find_by_name(qb_name, system_charset_info));

  if (qb == NULL)
  {
    hint->print_warn(pc->thd, ER_WARN_UNKNOWN_QB_NAME,
                     qb_name, NULL, NULL, NULL);
  }

  return qb;
}


/**
  Returns pointer to Opt_hints_table object,
  create Opt_hints_table object if not exist.

  @param pc          pointer to Parse_context object
  @param table_name  pointer to Hint_param_table object
  @param qb          pointer to Opt_hints_qb object

  @return   pointer to Opt_hints_table object,
            NULL if failed to create the object
*/

static Opt_hints_table *get_table_hints(Parse_context *pc,
                                        Hint_param_table *table_name,
                                        Opt_hints_qb *qb)
{
  Opt_hints_table *tab=
    static_cast<Opt_hints_table *> (qb->find_by_name(&table_name->table,
                                                     table_alias_charset));
  if (!tab)
  {
    tab= new Opt_hints_table(&table_name->table, qb, pc->thd->mem_root);
    qb->register_child(tab);
  }

  return tab;
}


void PT_hint::print_warn(THD *thd, uint err_code,
                         const LEX_CSTRING *qb_name_arg,
                         LEX_CSTRING *table_name_arg,
                         LEX_CSTRING *key_name_arg,
                         PT_hint *hint) const
{
  String str;

  /* Append hint name */
  if (!state)
    str.append(STRING_WITH_LEN("NO_"));
  str.append(opt_hint_info[hint_type].hint_name);

  /* ER_WARN_UNKNOWN_QB_NAME with two arguments */
  if (err_code == ER_WARN_UNKNOWN_QB_NAME)
  {
    String qb_name_str;
    append_identifier(thd, &qb_name_str, qb_name_arg->str, qb_name_arg->length);
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        err_code, ER_THD(thd, err_code),
                        qb_name_str.c_ptr_safe(), str.c_ptr_safe());
    return;
  }

  /* ER_WARN_CONFLICTING_HINT with one argument */
  str.append('(');

  /* Append table name */
  if (table_name_arg && table_name_arg->length > 0)
    append_identifier(thd, &str, table_name_arg->str, table_name_arg->length);

  /* Append QB name */
  if (qb_name_arg && qb_name_arg->length > 0)
  {
    str.append(STRING_WITH_LEN("@"));
    append_identifier(thd, &str, qb_name_arg->str, qb_name_arg->length);
  }

  /* Append key name */
  if (key_name_arg && key_name_arg->length > 0)
  {
    str.append(' ');
    append_identifier(thd, &str, key_name_arg->str, key_name_arg->length);
  }

  /* Append additional hint arguments if they exist */
  if (hint)
  {
    if (qb_name_arg || table_name_arg || key_name_arg)
      str.append(' ');

    hint->append_args(thd, &str);
  }

  str.append(')');

  push_warning_printf(thd, Sql_condition::SL_WARNING,
                      err_code, ER_THD(thd, err_code), str.c_ptr_safe());
}


bool PT_qb_level_hint::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  Opt_hints_qb *qb= find_qb_hints(pc, &qb_name, this);
  if (qb == NULL)
    return false;  // TODO: Should this generate a warning?

  bool conflict= false;  // true if this hint conflicts with a previous hint
  switch (type()) {
  case SEMIJOIN_HINT_ENUM:
    if (qb->subquery_hint)
      conflict= true;
    else if(!qb->semijoin_hint)
      qb->semijoin_hint= this;
    break;
  case SUBQUERY_HINT_ENUM:
    if (qb->semijoin_hint)
      conflict= true;
    else if (!qb->subquery_hint)
      qb->subquery_hint= this;
    break;
  default:
      DBUG_ASSERT(0);
  }

  if (conflict ||
      // Set hint or detect if hint has been set before
      qb->set_switch(switch_on(), type(), false))
    print_warn(pc->thd, ER_WARN_CONFLICTING_HINT, &qb_name, NULL, NULL, this);

  return false;
}

void PT_qb_level_hint::append_args(THD *thd, String *str) const
{
  switch (type()) {
  case SEMIJOIN_HINT_ENUM:
  {
    int count= 0;
    if (args & OPTIMIZER_SWITCH_FIRSTMATCH)
    {
      str->append(STRING_WITH_LEN(" FIRSTMATCH"));
      ++count;
    }
    if (args & OPTIMIZER_SWITCH_LOOSE_SCAN)
    {
      if (count++ > 0)
        str->append(STRING_WITH_LEN(","));
      str->append(STRING_WITH_LEN(" LOOSESCAN"));
    }
    if (args & OPTIMIZER_SWITCH_MATERIALIZATION)
    {
      if (count++ > 0)
        str->append(STRING_WITH_LEN(","));
      str->append(STRING_WITH_LEN(" MATERIALIZATION"));
    }
    if (args & OPTIMIZER_SWITCH_DUPSWEEDOUT)
    {
      if (count++ > 0)
        str->append(STRING_WITH_LEN(","));
      str->append(STRING_WITH_LEN(" DUPSWEEDOUT"));
    }
    break;
  }
  case SUBQUERY_HINT_ENUM:
    switch (args) {
    case Item_exists_subselect::EXEC_MATERIALIZATION:
      str->append(STRING_WITH_LEN(" MATERIALIZATION"));
      break;
    case Item_exists_subselect::EXEC_EXISTS:
      str->append(STRING_WITH_LEN(" INTOEXISTS"));
      break;
    default:      // Exactly one of above strategies should always be specified
      DBUG_ASSERT(false);
    }
    break;
  default:
    DBUG_ASSERT(false);
  }
}


bool PT_hint_list::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  if (!get_qb_hints(pc))
    return true;

  for (PT_hint **h= hints.begin(), **end= hints.end(); h < end; h++)
  {
    if (*h != NULL && (*h)->contextualize(pc))
      return true;
  }
  return false;
}


bool PT_table_level_hint::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  if (table_list.empty())  // Query block level hint
  {
    Opt_hints_qb *qb= find_qb_hints(pc, &qb_name, this);
    if (qb == NULL)
      return false;

    if (qb->set_switch(switch_on(), type(), false))
      print_warn(pc->thd, ER_WARN_CONFLICTING_HINT,
                 &qb_name, NULL, NULL, this);
    return false;
  }

  for (uint i= 0; i < table_list.size(); i++)
  {
    Hint_param_table *table_name= &table_list.at(i);
    /*
      If qb name exists then syntax '@qb_name table_name..' is used and
      we should use qb_name for finding query block. Otherwise syntax
      'table_name@qb_name' is used, so use table_name->opt_query_block.
    */
    const LEX_CSTRING *qb_name_str=
      qb_name.length > 0 ? &qb_name : &table_name->opt_query_block;

    Opt_hints_qb *qb= find_qb_hints(pc, qb_name_str, this);
    if (qb == NULL)
      return false;

    Opt_hints_table *tab= get_table_hints(pc, table_name, qb);
    if (!tab)
      return true;

    if (tab->set_switch(switch_on(), type(), true))
      print_warn(pc->thd, ER_WARN_CONFLICTING_HINT,
                 &table_name->opt_query_block,
                 &table_name->table, NULL, this);
  }

  return false;
}


bool PT_key_level_hint::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  Opt_hints_qb *qb= find_qb_hints(pc, &table_name.opt_query_block, this);
  if (qb == NULL)
    return false;

  Opt_hints_table *tab= get_table_hints(pc, &table_name, qb);
  if (!tab)
    return true;

  if (key_list.empty())  // Table level hint
  {
    if (tab->set_switch(switch_on(), type(), false))
      print_warn(pc->thd, ER_WARN_CONFLICTING_HINT,
                 &table_name.opt_query_block,
                 &table_name.table, NULL, this);
    return false;
  }

  for (uint i= 0; i < key_list.size(); i++)
  {
    LEX_CSTRING *key_name= &key_list.at(i);
    Opt_hints_key *key= (Opt_hints_key *)tab->find_by_name(key_name,
                                                           system_charset_info);

    if (!key)
    {
      key= new Opt_hints_key(key_name, tab, pc->thd->mem_root);
      tab->register_child(key);
    }

    if (key->set_switch(switch_on(), type(), true))
      print_warn(pc->thd, ER_WARN_CONFLICTING_HINT,
                 &table_name.opt_query_block,
                 &table_name.table, key_name, this);
  }

  return false;
}


bool PT_hint_qb_name::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  Opt_hints_qb *qb= pc->select->opt_hints_qb;

  DBUG_ASSERT(qb);

  if (qb->get_name() ||                         // QB name is already set
      qb->get_parent()->find_by_name(&qb_name,  // Name is already used
                                     system_charset_info))
  {
    print_warn(pc->thd, ER_WARN_CONFLICTING_HINT,
               NULL, NULL, NULL, this);
    return false;
  }

  qb->set_name(&qb_name);
  return false;
}


bool PT_hint_max_execution_time::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;

  if (pc->thd->lex->sql_command != SQLCOM_SELECT || // not a SELECT statement
      pc->thd->lex->sphead ||                       // or in a SP/trigger/event
      pc->select != pc->thd->lex->select_lex)       // or in a subquery
  {
    push_warning(pc->thd, Sql_condition::SL_WARNING,
                 ER_WARN_UNSUPPORTED_MAX_EXECUTION_TIME,
                 ER_THD(pc->thd, ER_WARN_UNSUPPORTED_MAX_EXECUTION_TIME));
    return false;
  }

  Opt_hints_global *global_hint= get_global_hints(pc);
  if (global_hint->is_specified(type()))
  {
    // Hint duplication: /*+ MAX_EXECUTION_TIME ... MAX_EXECUTION_TIME */
    print_warn(pc->thd, ER_WARN_CONFLICTING_HINT,
               NULL, NULL, NULL, this);
    return false;
  }

  pc->thd->lex->max_execution_time= milliseconds;
  global_hint->set_switch(switch_on(), type(), false);
  global_hint->max_exec_time= this;
  return false;
}

