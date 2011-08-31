/* Copyright (c) 2002, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "unireg.h"
#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif

#include "mysql.h"
#include "sp_head.h"
#include "sql_cursor.h"
#include "sp_rcontext.h"
#include "sp_pcontext.h"
#include "sql_select.h"                     // create_virtual_tmp_table

sp_rcontext::sp_rcontext(sp_pcontext *root_parsing_ctx,
                         Field *return_value_fld,
                         sp_rcontext *prev_runtime_ctx)
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
   m_hfound(-1),
   m_ccount(0),
   m_case_expr_holders(0),
   m_prev_runtime_ctx(prev_runtime_ctx)
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

  if (!(m_raised_conditions= new (thd->mem_root) MYSQL_ERROR[handler_count]))
    return TRUE;

  for (i= 0; i<handler_count; i++)
    m_raised_conditions[i].init(thd->mem_root);

  return
    !(m_handler=
      (sp_handler_t*)thd->alloc(handler_count * sizeof(sp_handler_t))) ||
    !(m_hstack=
      (uint*)thd->alloc(handler_count * sizeof(uint))) ||
    !(m_in_handler=
      (sp_active_handler_t*)thd->alloc(handler_count *
                                       sizeof(sp_active_handler_t))) ||
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


#define IS_WARNING_CONDITION(S)   ((S)[0] == '0' && (S)[1] == '1')
#define IS_NOT_FOUND_CONDITION(S) ((S)[0] == '0' && (S)[1] == '2')
#define IS_EXCEPTION_CONDITION(S) ((S)[0] != '0' || (S)[1] > '2')

/**
  Find an SQL handler for the given error.

  SQL handlers are pushed on the stack m_handler, with the latest/innermost
  one on the top; we then search for matching handlers from the top and
  down.

  We search through all the handlers, looking for the most specific one
  (sql_errno more specific than sqlstate more specific than the rest).
  Note that mysql error code handlers is a MySQL extension, not part of
  the standard.

  SQL handlers for warnings are searched in the current scope only.

  SQL handlers for errors are searched in the current and in outer scopes.
  That's why finding and activation of handler must be separated: an errror
  handler might be located in the outer scope, which is not active at the
  moment. Before such handler can be activated, execution flow should
  unwind to that scope.

  Found SQL handler is remembered in m_hfound for future activation.
  If no handler is found, m_hfound is -1.

  @param thd        Thread handle
  @param sql_errno  The error code
  @param sqlstate   The error SQL state
  @param level      The error level
  @param msg        The error message

  @retval TRUE  if an SQL handler was found
  @retval FALSE otherwise
*/

bool
sp_rcontext::find_handler(THD *thd,
                          uint sql_errno,
                          const char *sqlstate,
                          MYSQL_ERROR::enum_warning_level level,
                          const char *msg)
{
  int i= m_hcount;

  /* Reset previously found handler. */
  m_hfound= -1;

  /*
    If this is a fatal sub-statement error, and this runtime
    context corresponds to a sub-statement, no CONTINUE/EXIT
    handlers from this context are applicable: try to locate one
    in the outer scope.
  */
  if (thd->is_fatal_sub_stmt_error && in_sub_stmt)
    i= 0;

  /* Search handlers from the latest (innermost) to the oldest (outermost) */
  while (i--)
  {
    sp_cond_type_t *cond= m_handler[i].cond;
    int j= m_ihsp;

    /* Check active handlers, to avoid invoking one recursively */
    while (j--)
      if (m_in_handler[j].ip == m_handler[i].handler)
	break;
    if (j >= 0)
      continue;                 // Already executing this handler

    switch (cond->type)
    {
    case sp_cond_type_t::number:
      if (sql_errno == cond->mysqlerr &&
          (m_hfound < 0 || m_handler[m_hfound].cond->type > sp_cond_type_t::number))
	m_hfound= i;		// Always the most specific
      break;
    case sp_cond_type_t::state:
      if (strcmp(sqlstate, cond->sqlstate) == 0 &&
	  (m_hfound < 0 || m_handler[m_hfound].cond->type > sp_cond_type_t::state))
	m_hfound= i;
      break;
    case sp_cond_type_t::warning:
      if ((IS_WARNING_CONDITION(sqlstate) ||
           level == MYSQL_ERROR::WARN_LEVEL_WARN) &&
          m_hfound < 0)
	m_hfound= i;
      break;
    case sp_cond_type_t::notfound:
      if (IS_NOT_FOUND_CONDITION(sqlstate) && m_hfound < 0)
	m_hfound= i;
      break;
    case sp_cond_type_t::exception:
      if (IS_EXCEPTION_CONDITION(sqlstate) &&
	  level == MYSQL_ERROR::WARN_LEVEL_ERROR &&
	  m_hfound < 0)
	m_hfound= i;
      break;
    }
  }

  if (m_hfound >= 0)
  {
    DBUG_ASSERT((uint) m_hfound < m_root_parsing_ctx->max_handler_index());

    m_raised_conditions[m_hfound].clear();
    m_raised_conditions[m_hfound].set(sql_errno, sqlstate, level, msg);

    return TRUE;
  }

  /*
    Only "exception conditions" are propagated to handlers in calling
    contexts. If no handler is found locally for a "completion condition"
    (warning or "not found") we will simply resume execution.
  */
  if (m_prev_runtime_ctx && IS_EXCEPTION_CONDITION(sqlstate) &&
      level == MYSQL_ERROR::WARN_LEVEL_ERROR)
  {
    return m_prev_runtime_ctx->find_handler(thd, sql_errno, sqlstate,
                                            level, msg);
  }

  return FALSE;
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
sp_rcontext::push_handler(struct sp_cond_type *cond, uint h, int type)
{
  DBUG_ENTER("sp_rcontext::push_handler");
  DBUG_ASSERT(m_hcount < m_root_parsing_ctx->max_handler_index());

  m_handler[m_hcount].cond= cond;
  m_handler[m_hcount].handler= h;
  m_handler[m_hcount].type= type;
  m_hcount+= 1;

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

void
sp_rcontext::push_hstack(uint h)
{
  DBUG_ENTER("sp_rcontext::push_hstack");
  DBUG_ASSERT(m_hsp < m_root_parsing_ctx->max_handler_index());

  m_hstack[m_hsp++]= h;

  DBUG_PRINT("info", ("m_hsp: %d", m_hsp));
  DBUG_VOID_RETURN;
}

uint
sp_rcontext::pop_hstack()
{
  uint handler;
  DBUG_ENTER("sp_rcontext::pop_hstack");
  DBUG_ASSERT(m_hsp);

  handler= m_hstack[--m_hsp];

  DBUG_PRINT("info", ("m_hsp: %d", m_hsp));
  DBUG_RETURN(handler);
}

/**
  Prepare found handler to be executed.

  @retval TRUE if an SQL handler is activated (was found) and IP of the
          first handler instruction.
  @retval FALSE if there is no active handler
*/

bool
sp_rcontext::activate_handler(THD *thd,
                              uint *ip,
                              sp_instr *instr,
                              Query_arena *execute_arena,
                              Query_arena *backup_arena)
{
  if (m_hfound < 0)
    return FALSE;

  switch (m_handler[m_hfound].type) {
  case SP_HANDLER_NONE:
    break;

  case SP_HANDLER_CONTINUE:
    thd->restore_active_arena(execute_arena, backup_arena);
    thd->set_n_backup_active_arena(execute_arena, backup_arena);
    push_hstack(instr->get_cont_dest());

    /* Fall through */

  default:
    /* End aborted result set. */

    if (end_partial_result_set)
      thd->protocol->end_partial_result_set(thd);

    /* Enter handler. */

    DBUG_ASSERT(m_ihsp < m_root_parsing_ctx->max_handler_index());
    DBUG_ASSERT(m_hfound >= 0);

    m_in_handler[m_ihsp].ip= m_handler[m_hfound].handler;
    m_in_handler[m_ihsp].index= m_hfound;
    m_ihsp++;

    DBUG_PRINT("info", ("Entering handler..."));
    DBUG_PRINT("info", ("m_ihsp: %d", m_ihsp));

    /* Reset error state. */

    thd->clear_error();
    thd->killed= THD::NOT_KILLED; // Some errors set thd->killed
                                  // (e.g. "bad data").

    /* Return IP of the activated SQL handler. */
    *ip= m_handler[m_hfound].handler;

    /* Reset found handler. */
    m_hfound= -1;
  }

  return TRUE;
}

void
sp_rcontext::exit_handler()
{
  DBUG_ENTER("sp_rcontext::exit_handler");
  DBUG_ASSERT(m_ihsp);

  uint hindex= m_in_handler[m_ihsp-1].index;
  m_raised_conditions[hindex].clear();
  m_ihsp-= 1;

  DBUG_PRINT("info", ("m_ihsp: %d", m_ihsp));
  DBUG_VOID_RETURN;
}

MYSQL_ERROR*
sp_rcontext::raised_condition() const
{
  if (m_ihsp > 0)
  {
    uint hindex= m_in_handler[m_ihsp - 1].index;
    MYSQL_ERROR *raised= & m_raised_conditions[hindex];
    return raised;
  }

  if (m_prev_runtime_ctx)
    return m_prev_runtime_ctx->raised_condition();

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
sp_cursor::fetch(THD *thd, List<struct sp_variable> *vars)
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
                  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
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
  List_iterator_fast<struct sp_variable> spvar_iter(*spvar_list);
  List_iterator_fast<Item> item_iter(items);
  sp_variable_t *spvar;
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
