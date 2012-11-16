/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

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
#include "sp.h"                                // sp_eval_expr
#include "sql_cursor.h"
#include "sp_rcontext.h"
#include "sp_pcontext.h"
#include "sql_tmp_table.h"                     // create_virtual_tmp_table
#include "sp_instr.h"


///////////////////////////////////////////////////////////////////////////
// sp_rcontext implementation.
///////////////////////////////////////////////////////////////////////////


sp_rcontext::sp_rcontext(const sp_pcontext *root_parsing_ctx,
                         Field *return_value_fld,
                         bool in_sub_stmt)
  :end_partial_result_set(false),
   m_root_parsing_ctx(root_parsing_ctx),
   m_var_table(NULL),
   m_return_value_fld(return_value_fld),
   m_return_value_set(false),
   m_in_sub_stmt(in_sub_stmt),
   m_ccount(0)
{
}


sp_rcontext::~sp_rcontext()
{
  if (m_var_table)
    free_blobs(m_var_table);

  while (m_activated_handlers.elements())
    delete m_activated_handlers.pop();

  // Leave m_visible_handlers, m_var_items, m_cstack
  // and m_case_expr_holders untouched.
  // They are allocated in mem roots and will be freed accordingly.
}


sp_rcontext *sp_rcontext::create(THD *thd,
                                 const sp_pcontext *root_parsing_ctx,
                                 Field *return_value_fld)
{
  sp_rcontext *ctx= new (thd->mem_root) sp_rcontext(root_parsing_ctx,
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
    return false;

  m_root_parsing_ctx->retrieve_field_definitions(&field_def_lst);

  DBUG_ASSERT(field_def_lst.elements == m_root_parsing_ctx->max_var_index());

  if (!(m_var_table= create_virtual_tmp_table(thd, field_def_lst)))
    return true;

  m_var_table->copy_blobs= true;
  m_var_table->alias= "";

  return false;
}


bool sp_rcontext::init_var_items(THD *thd)
{
  uint num_vars= m_root_parsing_ctx->max_var_index();

  m_var_items.reset(
    static_cast<Item **> (
      thd->alloc(num_vars * sizeof (Item *))),
    num_vars);

  if (!m_var_items.array())
    return true;

  for (uint idx = 0; idx < num_vars; ++idx)
  {
    if (!(m_var_items[idx]= new Item_field(m_var_table->field[idx])))
      return true;
  }

  return false;
}


bool sp_rcontext::set_return_value(THD *thd, Item **return_value_item)
{
  DBUG_ASSERT(m_return_value_fld);

  m_return_value_set = true;

  return sp_eval_expr(thd, m_return_value_fld, return_value_item);
}


bool sp_rcontext::push_cursor(sp_instr_cpush *i)
{
  /*
    We should create cursors in the callers arena, as
    it could be (and usually is) used in several instructions.
  */
  sp_cursor *c= new (callers_arena->mem_root) sp_cursor(i);

  if (c == NULL)
    return true;

  m_cstack[m_ccount++]= c;
  return false;
}


void sp_rcontext::pop_cursors(uint count)
{
  DBUG_ASSERT(m_ccount >= count);

  while (count--)
    delete m_cstack[--m_ccount];
}


bool sp_rcontext::push_handler(sp_handler *handler, uint first_ip)
{
  /*
    We should create handler entries in the callers arena, as
    they could be (and usually are) used in several instructions.
  */
  sp_handler_entry *he=
    new (callers_arena->mem_root) sp_handler_entry(handler, first_ip);

  if (he == NULL)
    return true;

  return m_visible_handlers.append(he);
}


void sp_rcontext::pop_handlers(sp_pcontext *current_scope)
{
  for (int i= m_visible_handlers.elements() - 1; i >= 0; --i)
  {
    int handler_level= m_visible_handlers.at(i)->handler->scope->get_level();

    if (handler_level >= current_scope->get_level())
      m_visible_handlers.pop();
  }
}


void sp_rcontext::pop_handler_frame(THD *thd)
{
  Handler_call_frame *frame= m_activated_handlers.pop();

  // Also pop matching DA and copy new conditions.
  DBUG_ASSERT(thd->get_stmt_da() == &frame->handler_da);
  thd->pop_diagnostics_area();
  thd->get_stmt_da()->reset_condition_info(thd->query_id);
  thd->get_stmt_da()->copy_new_sql_conditions(thd, &frame->handler_da);

  delete frame;
}


void sp_rcontext::exit_handler(THD *thd, sp_pcontext *target_scope)
{
  /*
    The handler has successfully completed. We should now pop the current
    handler frame and pop the diagnostics area which was pushed when the
    handler was activated.

    The diagnostics area of the caller should then contain only any
    conditions pushed during handler execution. The conditions present
    when the handler was activated should be removed since they have
    been successfully handled.
  */
  pop_handler_frame(thd);

  // Pop frames below the target scope level.
  for (int i= m_activated_handlers.elements() - 1; i >= 0; --i)
  {
    int handler_level= m_activated_handlers.at(i)->handler->scope->get_level();

    if (handler_level <= target_scope->get_level())
      break;

    pop_handler_frame(thd);
  }

  /*
    Condition was successfully handled, reset condition count
    so we don't trigger the handler again for the same warning.
  */
  thd->get_stmt_da()->reset_statement_cond_count();
}


bool sp_rcontext::handle_sql_condition(THD *thd,
                                       uint *ip,
                                       const sp_instr *cur_spi)
{
  DBUG_ENTER("sp_rcontext::handle_sql_condition");

  /*
    If this is a fatal sub-statement error, and this runtime
    context corresponds to a sub-statement, no CONTINUE/EXIT
    handlers from this context are applicable: try to locate one
    in the outer scope.
  */
  if (thd->is_fatal_sub_stmt_error && m_in_sub_stmt)
    DBUG_RETURN(false);

  Diagnostics_area *da= thd->get_stmt_da();
  const sp_handler *found_handler= NULL;
  Sql_condition *found_condition= NULL;

  if (thd->is_error())
  {
    sp_pcontext *cur_pctx= cur_spi->get_parsing_ctx();

    found_handler= cur_pctx->find_handler(da->returned_sqlstate(),
                                          da->mysql_errno(),
                                          Sql_condition::SL_ERROR);

    if (found_handler)
    {
      found_condition= da->error_condition();

      /*
        error_condition can be NULL if the Diagnostics Area was full
        when the error was raised. It can also be NULL if
        Diagnostics_area::set_error_status(uint sql_error) was used.
        In these cases, make a temporary Sql_condition here so the
        error can be handled.
      */
      if (!found_condition)
      {
        found_condition= new (callers_arena->mem_root)
          Sql_condition(callers_arena->mem_root,
                        da->mysql_errno(),
                        da->returned_sqlstate(),
                        Sql_condition::SL_ERROR,
                        da->message_text());
      }
    }
  }
  else if (da->current_statement_cond_count())
  {
    Diagnostics_area::Sql_condition_iterator it= da->sql_conditions();
    const Sql_condition *c;

    // Here we need to find the last warning/note from the stack.
    // In MySQL most substantial warning is the last one.
    // (We could have used a reverse iterator here if one existed)

    while ((c= it++))
    {
      if (c->severity() == Sql_condition::SL_WARNING ||
          c->severity() == Sql_condition::SL_NOTE)
      {
        sp_pcontext *cur_pctx= cur_spi->get_parsing_ctx();

        const sp_handler *handler=
          cur_pctx->find_handler(c->returned_sqlstate(),
                                 c->mysql_errno(),
                                 c->severity());
        if (handler)
        {
          found_handler= handler;
          found_condition= const_cast<Sql_condition*>(c);
        }
      }
    }
  }

  if (!found_handler)
    DBUG_RETURN(false);

  // At this point, we know that:
  //  - there is a pending SQL-condition (error or warning);
  //  - there is an SQL-handler for it.

  DBUG_ASSERT(found_condition);

  sp_handler_entry *handler_entry= NULL;
  for (int i= 0; i < m_visible_handlers.elements(); ++i)
  {
    sp_handler_entry *h= m_visible_handlers.at(i);

    if (h->handler == found_handler)
    {
      handler_entry= h;
      break;
    }
  }

  /*
    handler_entry usually should not be NULL here, as that indicates
    that the parser context thinks a HANDLER should be activated,
    but the runtime context cannot find it.

    However, this can happen (and this is in line with the Standard)
    if SQL-condition has been raised before DECLARE HANDLER instruction
    is processed.

    For example:
    CREATE PROCEDURE p()
    BEGIN
      DECLARE v INT DEFAULT 'get'; -- raises SQL-warning here
      DECLARE EXIT HANDLER ...     -- this handler does not catch the warning
    END
  */
  if (!handler_entry)
    DBUG_RETURN(false);

  uint continue_ip= handler_entry->handler->type == sp_handler::CONTINUE ?
    cur_spi->get_cont_dest() : 0;

  /* Add a frame to handler-call-stack. */
  Handler_call_frame *frame= new Handler_call_frame(found_handler,
                                                    found_condition,
                                                    continue_ip,
                                                    da->statement_id());
  m_activated_handlers.append(frame);

  /* End aborted result set. */
  if (end_partial_result_set)
    thd->protocol->end_partial_result_set(thd);

  /* Reset error state. */
  thd->clear_error();
  thd->killed= THD::NOT_KILLED; // Some errors set thd->killed
                                // (e.g. "bad data").

  thd->push_diagnostics_area(&frame->handler_da);

  /*
    Mark current conditions so we later will know which conditions
    were added during handler execution (if any).
  */
  frame->handler_da.mark_preexisting_sql_conditions();

  *ip= handler_entry->first_ip;

  DBUG_RETURN(true);
}


bool sp_rcontext::set_variable(THD *thd, Field *field, Item **value)
{
  if (!value)
  {
    field->set_null();
    return false;
  }

  return sp_eval_expr(thd, field, value);
}


Item_cache *sp_rcontext::create_case_expr_holder(THD *thd,
                                                 const Item *item) const
{
  Item_cache *holder;
  Query_arena current_arena;

  thd->set_n_backup_active_arena(thd->sp_runtime_ctx->callers_arena,
                                 &current_arena);

  holder= Item_cache::get_cache(item);

  thd->restore_active_arena(thd->sp_runtime_ctx->callers_arena, &current_arena);

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


/**
  Open an SP cursor

  @param thd  Thread context

  @return Error status
*/

bool sp_cursor::open(THD *thd)
{
  if (m_server_side_cursor)
  {
    my_message(ER_SP_CURSOR_ALREADY_OPEN, ER(ER_SP_CURSOR_ALREADY_OPEN),
               MYF(0));
    return true;
  }

  return mysql_open_cursor(thd, &m_result, &m_server_side_cursor);
}


bool sp_cursor::close(THD *thd)
{
  if (! m_server_side_cursor)
  {
    my_message(ER_SP_CURSOR_NOT_OPEN, ER(ER_SP_CURSOR_NOT_OPEN), MYF(0));
    return true;
  }

  destroy();
  return false;
}


void sp_cursor::destroy()
{
  delete m_server_side_cursor;
  m_server_side_cursor= NULL;
}


bool sp_cursor::fetch(THD *thd, List<sp_variable> *vars)
{
  if (! m_server_side_cursor)
  {
    my_message(ER_SP_CURSOR_NOT_OPEN, ER(ER_SP_CURSOR_NOT_OPEN), MYF(0));
    return true;
  }

  if (vars->elements != m_result.get_field_count())
  {
    my_message(ER_SP_WRONG_NO_OF_FETCH_ARGS,
               ER(ER_SP_WRONG_NO_OF_FETCH_ARGS), MYF(0));
    return true;
  }

  DBUG_EXECUTE_IF("bug23032_emit_warning",
                  push_warning(thd, Sql_condition::SL_WARNING,
                               ER_UNKNOWN_ERROR,
                               ER(ER_UNKNOWN_ERROR)););

  m_result.set_spvar_list(vars);

  /* Attempt to fetch one row */
  if (m_server_side_cursor->is_open())
    m_server_side_cursor->fetch(1);

  /*
    If the cursor was pointing after the last row, the fetch will
    close it instead of sending any rows.
  */
  if (! m_server_side_cursor->is_open())
  {
    my_message(ER_SP_FETCH_NO_DATA, ER(ER_SP_FETCH_NO_DATA), MYF(0));
    return true;
  }

  return false;
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
    if (thd->sp_runtime_ctx->set_variable(thd, spvar->offset, &item))
      return true;
  }
  return false;
}
