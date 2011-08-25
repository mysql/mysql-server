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


sp_rcontext * sp_rcontext::create(THD *thd,
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
      ctx->init_var_items())
  {
    delete ctx;
    return NULL;
  }

  return ctx;
}


/**
  Internal function to allocate memory for arrays.

  @param thd Thread handle.

  @return error flag: false on success, true in case of failure.
*/

bool sp_rcontext::alloc_arrays(THD *thd)
{
  {
    size_t n= m_root_parsing_ctx->max_cursor_index();
    m_cstack.reset(
      reinterpret_cast<sp_cursor **> (
        thd->alloc(n * sizeof (sp_cursor*))),
      n);
  }

  {
    size_t n= m_root_parsing_ctx->get_num_case_exprs();
    m_case_expr_holders.reset(
      reinterpret_cast<Item_cache **> (
        thd->calloc(n * sizeof (Item_cache*))),
      n);
  }

  return !m_cstack.array() || !m_case_expr_holders.array();
}


/*
  Create and initialize a table to store SP-vars.

  SYNOPSIS
    thd   Thread handler.
  RETURN
    FALSE   on success
    TRUE    on error
*/

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


/*
  Create and initialize an Item-adapter (Item_field) for each SP-var field.

  RETURN
    FALSE   on success
    TRUE    on error
*/

bool sp_rcontext::init_var_items()
{
  uint num_vars= m_root_parsing_ctx->max_var_index();

  m_var_items.reset(
    reinterpret_cast<Item **> (
      sql_alloc(num_vars * sizeof (Item *))),
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


/**
  Create a new sp_cursor instance and push it to the cursor stack.

  @param lex_keeper SP-instruction execution helper.
  @param i          Cursor-push instruction.
*/

void sp_rcontext::push_cursor(sp_lex_keeper *lex_keeper, sp_instr_cpush *i)
{
  DBUG_ENTER("sp_rcontext::push_cursor");

  m_cstack[m_ccount++]= new sp_cursor(lex_keeper, i);

  DBUG_PRINT("info", ("m_ccount: %d", m_ccount));
  DBUG_VOID_RETURN;
}


/**
  Pop and delete given number of sp_cursor instance from the cursor stack.

  @param count Number of cursors to pop & delete.
*/

void sp_rcontext::pop_cursors(uint count)
{
  DBUG_ENTER("sp_rcontext::pop_cursors");

  while (count--)
    delete m_cstack[--m_ccount];

  DBUG_PRINT("info", ("m_ccount: %d", m_ccount));
  DBUG_VOID_RETURN;
}


/**
  Create a new sp_handler_entry instance and push it to the handler call stack.

  @param handler  SQL-handler object.
  @param first_ip First instruction pointer of the handler.
*/

void sp_rcontext::push_handler(sp_handler *handler, uint first_ip)
{
  DBUG_ENTER("sp_rcontext::push_handler");

  m_handlers.append(new sp_handler_entry(handler, first_ip));

  DBUG_VOID_RETURN;
}


/**
  Pop and delete given number of sp_handler_entry instances from the handler
  call stack.

  @param count Number of handler entries to pop & delete.
*/

void sp_rcontext::pop_handlers(int count)
{
  DBUG_ENTER("sp_rcontext::pop_handlers");
  DBUG_ASSERT(m_handlers.elements() >= count);

  for (int i= 0; i < count; ++i)
    delete m_handlers.pop();

  DBUG_VOID_RETURN;
}


/**
  Handle current SQL condition (if any).

  This is the public-interface function to handle SQL conditions in stored
  routines.

  @param thd            Thread handle.
  @param ip[out]        Instruction pointer to the first handler instruction.
  @param cur_spi        Current SP instruction.
  @param execute_arena  Current execution arena for SP.
  @param backup_arena   Backup arena for SP.

  @retval true if an SQL-handler has been activated. That means, all of the
  following conditions are satisfied:
    - the SP-instruction raised SQL-condition(s),
    - and there is an SQL-handler to process at least one of those SQL-conditions,
    - and that SQL-handler has been activated.
  Note, that the return value has nothing to do with "error flag" semantics.
  @retval false otherwise.
*/

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
  sp_handler *found_handler= NULL;
  Sql_condition_info *sql_condition= NULL;

  if (thd->is_error())
  {
    found_handler=
      cur_spi->m_ctx->find_handler(da->get_sqlstate(),
                                   da->sql_errno(),
                                   Sql_condition::WARN_LEVEL_ERROR);

    if (!found_handler)
      return false;

    da->remove_sql_condition(da->get_error_condition());

    sql_condition= new Sql_condition_info(da->sql_errno(),
                                          Sql_condition::WARN_LEVEL_ERROR,
                                          da->get_sqlstate(),
                                          da->message());
  }
  else if (da->current_statement_warn_count())
  {
    Diagnostics_area::Sql_condition_iterator it= da->sql_conditions();
    const Sql_condition *c;

    while ((c= it++))
    {
      if (c->get_level() != Sql_condition::WARN_LEVEL_WARN &&
          c->get_level() != Sql_condition::WARN_LEVEL_NOTE)
        continue;

      found_handler= cur_spi->m_ctx->find_handler(c->get_sqlstate(),
                                                  c->get_sql_errno(),
                                                  c->get_level());
      if (found_handler)
      {
        da->remove_sql_condition(c);

        sql_condition= new Sql_condition_info(c->get_sql_errno(),
                                              c->get_level(),
                                              c->get_sqlstate(),
                                              c->get_message_text());
        break;
      }
    }

    if (!found_handler)
      return false;
  }
  else
  {
    // No pending SQL-condition.
    return false;
  }

  // At this point, we know that:
  //  - there is a pending SQL-condition (error or warning);
  //  - there is an SQL-handler for it.

  DBUG_ASSERT(found_handler);
  DBUG_ASSERT(sql_condition);

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

  activate_handler(thd,
                   handler_entry,
                   sql_condition,
                   cur_spi,
                   execute_arena,
                   backup_arena);

  *ip= handler_entry->first_ip;

  return true;
}


/**
  Prepare an SQL handler to be executed.

  @param thd            Thread handle.
  @param handler_entry  Handler entry.
  @param cur_spi        Current SP instruction.
  @param execute_arena  Current execution arena for SP.
  @param backup_arena   Backup arena for SP.
*/

void sp_rcontext::activate_handler(THD *thd,
                                   const sp_handler_entry *handler_entry,
                                   Sql_condition_info *sql_condition,
                                   const sp_instr *cur_spi,
                                   Query_arena *execute_arena,
                                   Query_arena *backup_arena)
{
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
    new Handler_call_frame(sql_condition, continue_ip));
}


/**
  Remove latest call frame from the handler call stack.
  @return continue instruction pointer of the removed handler.
*/

uint sp_rcontext::exit_handler()
{
  DBUG_ENTER("sp_rcontext::exit_handler");
  DBUG_ASSERT(m_handler_call_stack.elements() > 0);

  Handler_call_frame *f= m_handler_call_stack.pop();

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


/*
 *
 *  sp_cursor
 *
 */

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


/*
  Create an instance of appropriate Item_cache class depending on the
  specified type in the callers arena.

  SYNOPSIS
    thd           thread handler
    result_type   type of the expression

  RETURN
    Pointer to valid object     on success
    NULL                        on error

  NOTE
    We should create cache items in the callers arena, as they are used
    between in several instructions.
*/

Item_cache * sp_rcontext::create_case_expr_holder(THD *thd,
                                                  const Item *item) const
{
  Item_cache *holder;
  Query_arena current_arena;

  thd->set_n_backup_active_arena(thd->spcont->callers_arena, &current_arena);

  holder= Item_cache::get_cache(item);

  thd->restore_active_arena(thd->spcont->callers_arena, &current_arena);

  return holder;
}


/*
  Set CASE expression to the specified value.

  SYNOPSIS
    thd             thread handler
    case_expr_id    identifier of the CASE expression
    case_expr_item  a value of the CASE expression

  RETURN
    FALSE   on success
    TRUE    on error

  NOTE
    The idea is to reuse Item_cache for the expression of the one CASE
    statement. This optimization takes place when there is CASE statement
    inside of a loop. So, in other words, we will use the same object on each
    iteration instead of creating a new one for each iteration.

  TODO
    Hypothetically, a type of CASE expression can be different for each
    iteration. For instance, this can happen if the expression contains a
    session variable (something like @@VAR) and its type is changed from one
    iteration to another.

    In order to cope with this problem, we check type each time, when we use
    already created object. If the type does not match, we re-create Item.
    This also can (should?) be optimized.
*/

int sp_rcontext::set_case_expr(THD *thd, int case_expr_id,
                               Item **case_expr_item_ptr)
{
  Item *case_expr_item= sp_prepare_func_item(thd, case_expr_item_ptr);
  if (!case_expr_item)
    return TRUE;

  if (!m_case_expr_holders[case_expr_id] ||
      m_case_expr_holders[case_expr_id]->result_type() !=
        case_expr_item->result_type())
  {
    m_case_expr_holders[case_expr_id]=
      create_case_expr_holder(thd, case_expr_item);
  }

  m_case_expr_holders[case_expr_id]->store(case_expr_item);
  m_case_expr_holders[case_expr_id]->cache_value();
  return FALSE;
}


/***************************************************************************
 Select_fetch_into_spvars
****************************************************************************/

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
