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

sp_rcontext::sp_rcontext(sp_pcontext *root_parsing_ctx,
                         Field *return_value_fld)
  :end_partial_result_set(FALSE),
   m_root_parsing_ctx(root_parsing_ctx),
   m_var_table(0),
   m_var_items(0),
   m_return_value_fld(return_value_fld),
   m_return_value_set(FALSE),
   in_sub_stmt(FALSE),
   m_hcount(0),
   m_hsp(0),
   m_ihsp(0),
   m_ccount(0),
   m_case_expr_holders(0)
{
}


sp_rcontext::~sp_rcontext()
{
  if (m_var_table)
    free_blobs(m_var_table);
}


/*
  Initialize sp_rcontext instance.

  SYNOPSIS
    thd   Thread handle
  RETURN
    FALSE   on success
    TRUE    on error
*/

bool sp_rcontext::init(THD *thd)
{
  uint handler_count= m_root_parsing_ctx->max_handler_index();
  uint i;

  in_sub_stmt= thd->in_sub_stmt;

  if (init_var_table(thd) || init_var_items())
    return TRUE;

  if (!(m_raised_conditions= new (thd->mem_root) Sql_condition[handler_count]))
    return TRUE;

  for (i= 0; i<handler_count; i++)
    m_raised_conditions[i].init(thd->mem_root);

  return
    !(m_handlers=
      (sp_handler_entry*)thd->alloc(handler_count * sizeof(sp_handler_entry))) ||
    !(m_hstack=
      (uint*)thd->alloc(handler_count * sizeof(uint))) ||
    !(m_in_handler=
      (uint*)thd->alloc(handler_count * sizeof(uint))) ||
    !(m_cstack=
      (sp_cursor**)thd->alloc(m_root_parsing_ctx->max_cursor_index() *
                              sizeof(sp_cursor*))) ||
    !(m_case_expr_holders=
      (Item_cache**)thd->calloc(m_root_parsing_ctx->get_num_case_exprs() *
                               sizeof (Item_cache*)));
}


/*
  Create and initialize a table to store SP-vars.

  SYNOPSIS
    thd   Thread handler.
  RETURN
    FALSE   on success
    TRUE    on error
*/

bool
sp_rcontext::init_var_table(THD *thd)
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

bool
sp_rcontext::init_var_items()
{
  uint idx;
  uint num_vars= m_root_parsing_ctx->max_var_index();

  if (!(m_var_items= (Item**) sql_alloc(num_vars * sizeof (Item *))))
    return TRUE;

  for (idx = 0; idx < num_vars; ++idx)
  {
    if (!(m_var_items[idx]= new Item_field(m_var_table->field[idx])))
      return TRUE;
  }

  return FALSE;
}


bool
sp_rcontext::set_return_value(THD *thd, Item **return_value_item)
{
  DBUG_ASSERT(m_return_value_fld);

  m_return_value_set = TRUE;

  return sp_eval_expr(thd, m_return_value_fld, return_value_item);
}


/**
  Find an SQL handler for the given SQL condition.

  SQL handlers are pushed on the stack m_handlers, with the latest/innermost
  one on the top; we then search for matching handlers from the top and
  down.

  We search through all the handlers, looking for the most specific one
  (sql_errno more specific than sqlstate more specific than the rest).
  Note that mysql error code handlers is a MySQL extension, not part of
  the standard.

  @param thd        Thread handle
  @param cur_pctx   Parsing context of the latest SP instruction
  @param sql_errno  SQL-condition error number
  @param sqlstate   SQL-condition SQLSTATE
  @param level      SQL-condition level
  @param msg        SQL-condition message

  @return SQL handler index, or -1 if there is no handler.
*/

int
sp_rcontext::find_handler(THD *thd,
                          sp_pcontext *cur_pctx,
                          uint sql_errno,
                          const char *sqlstate,
                          Sql_condition::enum_warning_level level,
                          const char *msg)
{
  /*
    If this is a fatal sub-statement error, and this runtime
    context corresponds to a sub-statement, no CONTINUE/EXIT
    handlers from this context are applicable: try to locate one
    in the outer scope.
  */
  if (thd->is_fatal_sub_stmt_error && in_sub_stmt)
    return -1;

  sp_handler *found_handler=
    cur_pctx->find_handler(sqlstate, sql_errno, level);

  if (!found_handler)
    return -1;

  for (uint i= 0; i < m_hcount; ++i)
  {
    if (found_handler == m_handlers[i].handler)
    {
      m_raised_conditions[i].clear();
      m_raised_conditions[i].set(sql_errno, sqlstate, level, msg);
      return i;
    }
  }

  DBUG_ASSERT(0); // should never get here.
  return -1;
}

void
sp_rcontext::push_cursor(sp_lex_keeper *lex_keeper, sp_instr_cpush *i)
{
  DBUG_ENTER("sp_rcontext::push_cursor");
  DBUG_ASSERT(m_ccount < m_root_parsing_ctx->max_cursor_index());
  m_cstack[m_ccount++]= new sp_cursor(lex_keeper, i);
  DBUG_PRINT("info", ("m_ccount: %d", m_ccount));
  DBUG_VOID_RETURN;
}

void
sp_rcontext::pop_cursors(uint count)
{
  DBUG_ENTER("sp_rcontext::pop_cursors");
  DBUG_ASSERT(m_ccount >= count);
  while (count--)
  {
    delete m_cstack[--m_ccount];
  }
  DBUG_PRINT("info", ("m_ccount: %d", m_ccount));
  DBUG_VOID_RETURN;
}

void
sp_rcontext::push_handler(sp_handler *handler,
                          uint first_ip)
{
  DBUG_ENTER("sp_rcontext::push_handler");
  DBUG_ASSERT(m_hcount < m_root_parsing_ctx->max_handler_index());

  m_handlers[m_hcount].handler= handler;
  m_handlers[m_hcount].first_ip= first_ip;
  ++m_hcount;

  DBUG_PRINT("info", ("m_hcount: %d", m_hcount));
  DBUG_VOID_RETURN;
}

void
sp_rcontext::pop_handlers(uint count)
{
  DBUG_ENTER("sp_rcontext::pop_handlers");
  DBUG_ASSERT(m_hcount >= count);

  m_hcount-= count;

  DBUG_PRINT("info", ("m_hcount: %d", m_hcount));
  DBUG_VOID_RETURN;
}

/**
  Remember continue instruction pointer (the execution should start from
  continue_ip after handler completion).
*/

void
sp_rcontext::push_hstack(uint continue_ip)
{
  DBUG_ENTER("sp_rcontext::push_hstack");
  DBUG_ASSERT(m_hsp < m_root_parsing_ctx->max_handler_index());

  m_hstack[m_hsp++]= continue_ip;

  DBUG_PRINT("info", ("m_hsp: %d", m_hsp));
  DBUG_VOID_RETURN;
}

/**
  Return continue instruction pointer.
*/

uint
sp_rcontext::pop_hstack()
{
  DBUG_ENTER("sp_rcontext::pop_hstack");
  DBUG_ASSERT(m_hsp);

  uint continue_ip= m_hstack[--m_hsp];

  DBUG_PRINT("info", ("m_hsp: %d", m_hsp));
  DBUG_RETURN(continue_ip);
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

bool
sp_rcontext::handle_sql_condition(THD *thd,
                                  uint *ip,
                                  const sp_instr *cur_spi,
                                  Query_arena *execute_arena,
                                  Query_arena *backup_arena)
{
  int handler_idx= find_handler(thd, cur_spi->m_ctx);

  if (handler_idx < 0)
    return false;

  activate_handler(thd, handler_idx, ip, cur_spi, execute_arena, backup_arena);

  return true;
}


/**
  Find an SQL handler for any condition (warning or error) after execution
  of a stored routine instruction. Basically, this function looks for an
  appropriate SQL handler in RT-contexts. If an SQL handler is found, it is
  remembered in the RT-context for future activation (the context can be
  inactive at the moment).

  If there is no pending condition, the function just returns.

  If there was an error during the execution, an SQL handler for it will be
  searched within the current and outer scopes.

  There might be several errors in the Warning Info (that's possible by using
  SIGNAL/RESIGNAL in nested scopes) -- the function is looking for an SQL
  handler for the latest (current) error only.

  If there was a warning during the execution, an SQL handler for it will be
  searched within the current scope only.

  If several warnings were thrown during the execution and there are different
  SQL handlers for them, it is not determined which SQL handler will be chosen.
  Only one SQL handler will be executed.

  If warnings and errors were thrown during the execution, the error takes
  precedence. I.e. error handler will be executed. If there is no handler
  for that error, condition will remain unhandled.

  According to The Standard (quoting PeterG):

    An SQL procedure statement works like this ...
    SQL/Foundation 13.5 <SQL procedure statement>
    (General Rules) (greatly summarized) says:
    (1) Empty diagnostics area, thus clearing the condition.
    (2) Execute statement.
        During execution, if Exception Condition occurs,
        set Condition Area = Exception Condition and stop
        statement.
        During execution, if No Data occurs,
        set Condition Area = No Data Condition and continue
        statement.
        During execution, if Warning occurs,
        and Condition Area is not already full due to
        an earlier No Data condition, set Condition Area
        = Warning and continue statement.
    (3) Finish statement.
        At end of execution, if Condition Area is not
        already full due to an earlier No Data or Warning,
        set Condition Area = Successful Completion.
        In effect, this system means there is a precedence:
        Exception trumps No Data, No Data trumps Warning,
        Warning trumps Successful Completion.

    NB: "Procedure statements" include any DDL or DML or
    control statements. So CREATE and DELETE and WHILE
    and CALL and RETURN are procedure statements. But
    DECLARE and END are not procedure statements.

  @param thd thread handle

  @return SQL-handler index to activate, or -1 if there is no pending
  SQL-condition, or no SQL-handler has been found.
*/

int
sp_rcontext::find_handler(THD *thd, sp_pcontext *cur_pctx)
{
  Diagnostics_area *da= thd->get_stmt_da();

  if (thd->is_error())
  {

    int handler_idx= find_handler(thd,
                                  cur_pctx,
                                  da->sql_errno(),
                                  da->get_sqlstate(),
                                  Sql_condition::WARN_LEVEL_ERROR,
                                  da->message());
    if (handler_idx >= 0)
      da->remove_sql_condition(da->get_error_condition());

    return handler_idx;
  }
  else if (thd->get_stmt_da()->current_statement_warn_count())
  {
    Diagnostics_area::Sql_condition_iterator it=
      thd->get_stmt_da()->sql_conditions();
    const Sql_condition *c;

    while ((c= it++))
    {
      if (c->get_level() != Sql_condition::WARN_LEVEL_WARN &&
          c->get_level() != Sql_condition::WARN_LEVEL_NOTE)
        continue;

      int handler_idx= find_handler(thd,
                                    cur_pctx,
                                    c->get_sql_errno(),
                                    c->get_sqlstate(),
                                    c->get_level(),
                                    c->get_message_text());
      if (handler_idx >= 0)
      {
        da->remove_sql_condition(c);
        return handler_idx;
      }
    }
  }

  return -1;
}

/**
  Prepare an SQL handler to be executed.

  @param thd            Thread handle.
  @param handler_idx    Handler index (must be >= 0).
  @param ip[out]        Instruction pointer to the first handler instruction.
  @param cur_spi        Current SP instruction.
  @param execute_arena  Current execution arena for SP.
  @param backup_arena   Backup arena for SP.
*/

void
sp_rcontext::activate_handler(THD *thd,
                              int handler_idx,
                              uint *ip,
                              const sp_instr *cur_spi,
                              Query_arena *execute_arena,
                              Query_arena *backup_arena)
{
  DBUG_ASSERT(handler_idx >= 0);
 
  switch (m_handlers[handler_idx].handler->type) {
  case sp_handler::CONTINUE:
    thd->restore_active_arena(execute_arena, backup_arena);
    thd->set_n_backup_active_arena(execute_arena, backup_arena);
    push_hstack(cur_spi->get_cont_dest());

    /* Fall through */

  default:
    /* End aborted result set. */

    if (end_partial_result_set)
      thd->protocol->end_partial_result_set(thd);

    /* Enter handler. */

    DBUG_ASSERT(m_ihsp < m_root_parsing_ctx->max_handler_index());

    m_in_handler[m_ihsp]= handler_idx;
    m_ihsp++;

    DBUG_PRINT("info", ("Entering handler..."));
    DBUG_PRINT("info", ("m_ihsp: %d", m_ihsp));

    /* Reset error state. */

    thd->clear_error();
    thd->killed= THD::NOT_KILLED; // Some errors set thd->killed
                                  // (e.g. "bad data").

    /* Return IP of the activated SQL handler. */
    *ip= m_handlers[handler_idx].first_ip;
  }
}

void
sp_rcontext::exit_handler()
{
  DBUG_ENTER("sp_rcontext::exit_handler");
  DBUG_ASSERT(m_ihsp);

  uint hindex= m_in_handler[m_ihsp-1];
  m_raised_conditions[hindex].clear();
  m_ihsp-= 1;

  DBUG_PRINT("info", ("m_ihsp: %d", m_ihsp));
  DBUG_VOID_RETURN;
}

Sql_condition*
sp_rcontext::raised_condition() const
{
  if (m_ihsp > 0)
  {
    uint hindex= m_in_handler[m_ihsp - 1];
    Sql_condition *raised= & m_raised_conditions[hindex];
    return raised;
  }

  return NULL;
}


int
sp_rcontext::set_variable(THD *thd, uint var_idx, Item **value)
{
  return set_variable(thd, m_var_table->field[var_idx], value);
}


int
sp_rcontext::set_variable(THD *thd, Field *field, Item **value)
{
  if (!value)
  {
    field->set_null();
    return 0;
  }

  return sp_eval_expr(thd, field, value);
}


Item *
sp_rcontext::get_item(uint var_idx)
{
  return m_var_items[var_idx];
}


Item **
sp_rcontext::get_item_addr(uint var_idx)
{
  return m_var_items + var_idx;
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

int
sp_cursor::open(THD *thd)
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


int
sp_cursor::close(THD *thd)
{
  if (! server_side_cursor)
  {
    my_message(ER_SP_CURSOR_NOT_OPEN, ER(ER_SP_CURSOR_NOT_OPEN), MYF(0));
    return -1;
  }
  destroy();
  return 0;
}


void
sp_cursor::destroy()
{
  delete server_side_cursor;
  server_side_cursor= 0;
}


int
sp_cursor::fetch(THD *thd, List<sp_variable> *vars)
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

Item_cache *
sp_rcontext::create_case_expr_holder(THD *thd, const Item *item)
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

int
sp_rcontext::set_case_expr(THD *thd, int case_expr_id, Item **case_expr_item_ptr)
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


Item *
sp_rcontext::get_case_expr(int case_expr_id)
{
  return m_case_expr_holders[case_expr_id];
}


Item **
sp_rcontext::get_case_expr_addr(int case_expr_id)
{
  return (Item**) m_case_expr_holders + case_expr_id;
}


/***************************************************************************
 Select_fetch_into_spvars
****************************************************************************/

int Select_fetch_into_spvars::prepare(List<Item> &fields, SELECT_LEX_UNIT *u)
{
  /*
    Cache the number of columns in the result set in order to easily
    return an error if column count does not match value count.
  */
  field_count= fields.elements;
  return select_result_interceptor::prepare(fields, u);
}


bool Select_fetch_into_spvars::send_data(List<Item> &items)
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
