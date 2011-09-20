/* Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql_priv.h"
#include "unireg.h"
#include "mysql.h"
#include "sp_head.h"
#include "sql_cursor.h"
#include "sp_rcontext.h"
#include "sp_pcontext.h"
#include "sql_select.h"                     // create_virtual_tmp_table


///////////////////////////////////////////////////////////////////////////
// sp_rcontext implementation.
///////////////////////////////////////////////////////////////////////////


sp_rcontext::sp_rcontext(const sp_pcontext *root_parsing_ctx,
                         Field *return_value_fld,
                         bool in_sub_stmt)
  :end_partial_result_set(FALSE),
   m_root_parsing_ctx(root_parsing_ctx),
   m_var_table(0),
   m_return_value_fld(return_value_fld),
   m_return_value_set(FALSE),
   m_in_sub_stmt(in_sub_stmt),
   m_ccount(0)
{
}


sp_rcontext::~sp_rcontext()
{
  if (m_var_table)
    free_blobs(m_var_table);

  while (m_handlers.elements() > 0)
    delete m_handlers.pop();

  while (m_handler_call_stack.elements() > 0)
    delete m_handler_call_stack.pop();

  // Leave m_var_items, m_cstack and m_case_expr_holders untouched.
  // They are allocated in THD's memory and will be freed accordingly.
}


sp_rcontext *sp_rcontext::create(THD *thd,
                                 const sp_pcontext *root_parsing_ctx,
                                 Field *return_value_fld)
{
  sp_rcontext *ctx= new sp_rcontext(root_parsing_ctx,
                                    return_value_fld,
                                    thd->in_sub_stmt);

  if (!ctx)
    return NULL;

  if (ctx->alloc_arrays(thd) ||
      ctx->init_var_table(thd) ||
      ctx->init_var_items(thd))
  {
    delete ctx;
    return NULL;
  }

  return ctx;
}


bool sp_rcontext::alloc_arrays(THD *thd)
{
  {
    size_t n= m_root_parsing_ctx->max_cursor_index();
    m_cstack.reset(
      static_cast<sp_cursor **> (
        thd->alloc(n * sizeof (sp_cursor*))),
      n);
  }

  {
    size_t n= m_root_parsing_ctx->get_num_case_exprs();
    m_case_expr_holders.reset(
      static_cast<Item_cache **> (
        thd->calloc(n * sizeof (Item_cache*))),
      n);
  }

  return !m_cstack.array() || !m_case_expr_holders.array();
}


bool sp_rcontext::init_var_table(THD *thd)
{
  List<Create_field> field_def_lst;

  if (!m_root_parsing_ctx->max_var_index())
    return FALSE;

  m_root_parsing_ctx->retrieve_field_definitions(&field_def_lst);

  DBUG_ASSERT(field_def_lst.elements == m_root_parsing_ctx->max_var_index());

  if (!(m_var_table= create_virtual_tmp_table(thd, field_def_lst)))
    return TRUE;

  m_var_table->copy_blobs= TRUE;
  m_var_table->alias= "";

  return FALSE;
}


bool sp_rcontext::init_var_items(THD *thd)
{
  uint num_vars= m_root_parsing_ctx->max_var_index();

  m_var_items.reset(
    static_cast<Item **> (
      thd->alloc(num_vars * sizeof (Item *))),
    num_vars);

  if (!m_var_items.array())
    return TRUE;

  for (uint idx = 0; idx < num_vars; ++idx)
  {
    if (!(m_var_items[idx]= new Item_field(m_var_table->field[idx])))
      return TRUE;
  }

  return FALSE;
}


bool sp_rcontext::set_return_value(THD *thd, Item **return_value_item)
{
  DBUG_ASSERT(m_return_value_fld);

  m_return_value_set = TRUE;

  return sp_eval_expr(thd, m_return_value_fld, return_value_item);
}


void sp_rcontext::push_cursor(sp_lex_keeper *lex_keeper, sp_instr_cpush *i)
{
  DBUG_ENTER("sp_rcontext::push_cursor");

  m_cstack[m_ccount++]= new sp_cursor(lex_keeper, i);

  DBUG_PRINT("info", ("m_ccount: %d", m_ccount));
  DBUG_VOID_RETURN;
}


void sp_rcontext::pop_cursors(uint count)
{
  DBUG_ENTER("sp_rcontext::pop_cursors");

  while (count--)
    delete m_cstack[--m_ccount];

  DBUG_PRINT("info", ("m_ccount: %d", m_ccount));
  DBUG_VOID_RETURN;
}


void sp_rcontext::push_handler(sp_handler *handler, uint first_ip)
{
  DBUG_ENTER("sp_rcontext::push_handler");

  m_handlers.append(new sp_handler_entry(handler, first_ip));

  DBUG_VOID_RETURN;
}


void sp_rcontext::pop_handlers(int count)
{
  DBUG_ENTER("sp_rcontext::pop_handlers");
  DBUG_ASSERT(m_handlers.elements() >= count);

  for (int i= 0; i < count; ++i)
    delete m_handlers.pop();

  DBUG_VOID_RETURN;
}


bool sp_rcontext::handle_sql_condition(THD *thd,
                                       uint *ip,
                                       const sp_instr *cur_spi,
                                       Query_arena *execute_arena,
                                       Query_arena *backup_arena)
{
  /*
    If this is a fatal sub-statement error, and this runtime
    context corresponds to a sub-statement, no CONTINUE/EXIT
    handlers from this context are applicable: try to locate one
    in the outer scope.
  */
  if (thd->is_fatal_sub_stmt_error && m_in_sub_stmt)
    return false;

  Diagnostics_area *da= thd->get_stmt_da();
  const sp_handler *found_handler= NULL;
  const Sql_condition *found_condition= NULL;

  if (thd->is_error())
  {
    found_handler=
      cur_spi->m_ctx->find_handler(da->get_sqlstate(),
                                   da->sql_errno(),
                                   Sql_condition::WARN_LEVEL_ERROR);

    if (found_handler)
      found_condition= da->get_error_condition();
  }
  else if (da->current_statement_warn_count())
  {
    Diagnostics_area::Sql_condition_iterator it= da->sql_conditions();
    const Sql_condition *c;

    while (!found_handler && (c= it++))
    {
      if (c->get_level() == Sql_condition::WARN_LEVEL_WARN ||
          c->get_level() == Sql_condition::WARN_LEVEL_NOTE)
      {
        found_handler= cur_spi->m_ctx->find_handler(c->get_sqlstate(),
                                                    c->get_sql_errno(),
                                                    c->get_level());
      }
    }
    if (found_handler)
      found_condition= c;
  }

  if (!found_handler)
    return false;

  // At this point, we know that:
  //  - there is a pending SQL-condition (error or warning);
  //  - there is an SQL-handler for it.

  DBUG_ASSERT(found_condition);

  // Mark active conditions so that they can be deleted when the handler exits.
  da->mark_sql_conditions_for_removal();

  sp_handler_entry *handler_entry= NULL;
  for (int i= 0; i < m_handlers.elements(); ++i)
  {
    sp_handler_entry *h= m_handlers.at(i);

    if (h->handler == found_handler)
    {
      handler_entry= h;
      break;
    }
  }

  DBUG_ASSERT(handler_entry);

  uint continue_ip= 0;
  if (handler_entry->handler->type == sp_handler::CONTINUE)
  {
    thd->restore_active_arena(execute_arena, backup_arena);
    thd->set_n_backup_active_arena(execute_arena, backup_arena);

    continue_ip= cur_spi->get_cont_dest();
  }

  /* End aborted result set. */
  if (end_partial_result_set)
    thd->protocol->end_partial_result_set(thd);

  /* Reset error state. */
  thd->clear_error();
  thd->killed= THD::NOT_KILLED; // Some errors set thd->killed
                                // (e.g. "bad data").

  /* Add a frame to handler-call-stack. */
  m_handler_call_stack.append(
    new Handler_call_frame(
      new Sql_condition_info(found_condition),
      continue_ip));

  *ip= handler_entry->first_ip;

  return true;
}


uint sp_rcontext::exit_handler(Diagnostics_area *da)
{
  DBUG_ENTER("sp_rcontext::exit_handler");
  DBUG_ASSERT(m_handler_call_stack.elements() > 0);

  Handler_call_frame *f= m_handler_call_stack.pop();

  /*
    Remove the SQL conditions that were present in DA when the
    handler was activated.
  */
  da->remove_marked_sql_conditions();

  uint continue_ip= f->continue_ip;

  delete f;

  DBUG_RETURN(continue_ip);
}


int sp_rcontext::set_variable(THD *thd, Field *field, Item **value)
{
  if (!value)
  {
    field->set_null();
    return 0;
  }

  return sp_eval_expr(thd, field, value);
}


Item_cache *sp_rcontext::create_case_expr_holder(THD *thd,
                                                 const Item *item) const
{
  Item_cache *holder;
  Query_arena current_arena;

  thd->set_n_backup_active_arena(thd->spcont->callers_arena, &current_arena);

  holder= Item_cache::get_cache(item);

  thd->restore_active_arena(thd->spcont->callers_arena, &current_arena);

  return holder;
}


bool sp_rcontext::set_case_expr(THD *thd, int case_expr_id,
                                Item **case_expr_item_ptr)
{
  Item *case_expr_item= sp_prepare_func_item(thd, case_expr_item_ptr);
  if (!case_expr_item)
    return true;

  if (!m_case_expr_holders[case_expr_id] ||
      m_case_expr_holders[case_expr_id]->result_type() !=
        case_expr_item->result_type())
  {
    m_case_expr_holders[case_expr_id]=
      create_case_expr_holder(thd, case_expr_item);
  }

  m_case_expr_holders[case_expr_id]->store(case_expr_item);
  m_case_expr_holders[case_expr_id]->cache_value();
  return false;
}


///////////////////////////////////////////////////////////////////////////
// sp_cursor implementation.
///////////////////////////////////////////////////////////////////////////


sp_cursor::sp_cursor(sp_lex_keeper *lex_keeper, sp_instr_cpush *i)
  :m_lex_keeper(lex_keeper),
   server_side_cursor(NULL),
   m_i(i)
{
  /*
    currsor can't be stored in QC, so we should prevent opening QC for
    try to write results which are absent.
  */
  lex_keeper->disable_query_cache();
}


/*
  Open an SP cursor

  SYNOPSIS
    open()
    THD		         Thread handler


  RETURN
   0 in case of success, -1 otherwise
*/

int sp_cursor::open(THD *thd)
{
  if (server_side_cursor)
  {
    my_message(ER_SP_CURSOR_ALREADY_OPEN, ER(ER_SP_CURSOR_ALREADY_OPEN),
               MYF(0));
    return -1;
  }
  if (mysql_open_cursor(thd, &result, &server_side_cursor))
    return -1;
  return 0;
}


int sp_cursor::close(THD *thd)
{
  if (! server_side_cursor)
  {
    my_message(ER_SP_CURSOR_NOT_OPEN, ER(ER_SP_CURSOR_NOT_OPEN), MYF(0));
    return -1;
  }
  destroy();
  return 0;
}


void sp_cursor::destroy()
{
  delete server_side_cursor;
  server_side_cursor= 0;
}


int sp_cursor::fetch(THD *thd, List<sp_variable> *vars)
{
  if (! server_side_cursor)
  {
    my_message(ER_SP_CURSOR_NOT_OPEN, ER(ER_SP_CURSOR_NOT_OPEN), MYF(0));
    return -1;
  }
  if (vars->elements != result.get_field_count())
  {
    my_message(ER_SP_WRONG_NO_OF_FETCH_ARGS,
               ER(ER_SP_WRONG_NO_OF_FETCH_ARGS), MYF(0));
    return -1;
  }

  DBUG_EXECUTE_IF("bug23032_emit_warning",
                  push_warning(thd, Sql_condition::WARN_LEVEL_WARN,
                               ER_UNKNOWN_ERROR,
                               ER(ER_UNKNOWN_ERROR)););

  result.set_spvar_list(vars);

  /* Attempt to fetch one row */
  if (server_side_cursor->is_open())
    server_side_cursor->fetch(1);

  /*
    If the cursor was pointing after the last row, the fetch will
    close it instead of sending any rows.
  */
  if (! server_side_cursor->is_open())
  {
    my_message(ER_SP_FETCH_NO_DATA, ER(ER_SP_FETCH_NO_DATA), MYF(0));
    return -1;
  }

  return 0;
}


///////////////////////////////////////////////////////////////////////////
// sp_cursor::Select_fetch_into_spvars implementation.
///////////////////////////////////////////////////////////////////////////


int sp_cursor::Select_fetch_into_spvars::prepare(List<Item> &fields,
                                                 SELECT_LEX_UNIT *u)
{
  /*
    Cache the number of columns in the result set in order to easily
    return an error if column count does not match value count.
  */
  field_count= fields.elements;
  return select_result_interceptor::prepare(fields, u);
}


bool sp_cursor::Select_fetch_into_spvars::send_data(List<Item> &items)
{
  List_iterator_fast<sp_variable> spvar_iter(*spvar_list);
  List_iterator_fast<Item> item_iter(items);
  sp_variable *spvar;
  Item *item;

  /* Must be ensured by the caller */
  DBUG_ASSERT(spvar_list->elements == items.elements);

  /*
    Assign the row fetched from a server side cursor to stored
    procedure variables.
  */
  for (; spvar= spvar_iter++, item= item_iter++; )
  {
    if (thd->spcont->set_variable(thd, spvar->offset, &item))
      return TRUE;
  }
  return FALSE;
}
