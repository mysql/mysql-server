/* Copyright (C) 2002 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif

#if defined(WIN32) || defined(__WIN__)
#undef SAFEMALLOC				/* Problems with threads */
#endif

#include "mysql.h"
#include "sp_head.h"
#include "sql_cursor.h"
#include "sp_rcontext.h"
#include "sp_pcontext.h"


sp_rcontext::sp_rcontext(sp_pcontext *root_parsing_ctx,
                         Field *return_value_fld,
                         sp_rcontext *prev_runtime_ctx)
  :m_root_parsing_ctx(root_parsing_ctx),
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
  in_sub_stmt= thd->in_sub_stmt;

  if (init_var_table(thd) || init_var_items())
    return TRUE;

  return
    !(m_handler=
      (sp_handler_t*)thd->alloc(m_root_parsing_ctx->max_handler_index() *
                                sizeof(sp_handler_t))) ||
    !(m_hstack=
      (uint*)thd->alloc(m_root_parsing_ctx->max_handler_index() *
                        sizeof(uint))) ||
    !(m_in_handler=
      (uint*)thd->alloc(m_root_parsing_ctx->max_handler_index() *
                        sizeof(uint))) ||
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

/*
  Find a handler for the given errno.
  This is called from all error message functions (e.g. push_warning,
  net_send_error, et al) when a sp_rcontext is in effect. If a handler
  is found, no error is sent, and the the SP execution loop will instead
  invoke the found handler.
  This might be called several times before we get back to the execution
  loop, so m_hfound can be >= 0 if a handler has already been found.
  (In which case we don't search again - the first found handler will
   be used.)
  Handlers are pushed on the stack m_handler, with the latest/innermost
  one on the top; we then search for matching handlers from the top and
  down.
  We search through all the handlers, looking for the most specific one
  (sql_errno more specific than sqlstate more specific than the rest).
  Note that mysql error code handlers is a MySQL extension, not part of
  the standard.

  SYNOPSIS
    sql_errno     The error code
    level         Warning level

  RETURN
    1             if a handler was found, m_hfound is set to its index (>= 0)
    0             if not found, m_hfound is -1
*/

bool
sp_rcontext::find_handler(THD *thd, uint sql_errno,
                          MYSQL_ERROR::enum_warning_level level)
{
  if (m_hfound >= 0)
    return 1;			// Already got one

  const char *sqlstate= mysql_errno_to_sqlstate(sql_errno);
  int i= m_hcount, found= -1;

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
      if (m_in_handler[j] == m_handler[i].handler)
	break;
    if (j >= 0)
      continue;                 // Already executing this handler

    switch (cond->type)
    {
    case sp_cond_type_t::number:
      if (sql_errno == cond->mysqlerr &&
          (found < 0 || m_handler[found].cond->type > sp_cond_type_t::number))
	found= i;		// Always the most specific
      break;
    case sp_cond_type_t::state:
      if (strcmp(sqlstate, cond->sqlstate) == 0 &&
	  (found < 0 || m_handler[found].cond->type > sp_cond_type_t::state))
	found= i;
      break;
    case sp_cond_type_t::warning:
      if ((IS_WARNING_CONDITION(sqlstate) ||
           level == MYSQL_ERROR::WARN_LEVEL_WARN) &&
          found < 0)
	found= i;
      break;
    case sp_cond_type_t::notfound:
      if (IS_NOT_FOUND_CONDITION(sqlstate) && found < 0)
	found= i;
      break;
    case sp_cond_type_t::exception:
      if (IS_EXCEPTION_CONDITION(sqlstate) &&
	  level == MYSQL_ERROR::WARN_LEVEL_ERROR &&
	  found < 0)
	found= i;
      break;
    }
  }
  if (found < 0)
  {
    /*
      Only "exception conditions" are propagated to handlers in calling
      contexts. If no handler is found locally for a "completion condition"
      (warning or "not found") we will simply resume execution.
    */
    if (m_prev_runtime_ctx && IS_EXCEPTION_CONDITION(sqlstate) &&
        level == MYSQL_ERROR::WARN_LEVEL_ERROR)
      return m_prev_runtime_ctx->find_handler(thd, sql_errno, level);
    return FALSE;
  }
  m_hfound= found;
  return TRUE;
}

/*
   Handle the error for a given errno.
   The severity of the error is adjusted depending of the current sql_mode.
   If an handler is present for the error (see find_handler()),
   this function will return true.
   If a handler is found and if the severity of the error indicate
   that the current instruction executed should abort,
   the flag thd->net.report_error is also set.
   This will cause the execution of the current instruction in a
   sp_instr* to fail, and give control to the handler code itself
   in the sp_head::execute() loop.

  SYNOPSIS
    sql_errno     The error code
    level         Warning level
    thd           The current thread

  RETURN
    TRUE       if a handler was found.
    FALSE      if no handler was found.
*/
bool
sp_rcontext::handle_error(uint sql_errno,
                          MYSQL_ERROR::enum_warning_level level,
                          THD *thd)
{
  MYSQL_ERROR::enum_warning_level elevated_level= level;


  /* Depending on the sql_mode of execution,
     warnings may be considered errors */
  if ((level == MYSQL_ERROR::WARN_LEVEL_WARN) &&
      thd->really_abort_on_warning())
  {
    elevated_level= MYSQL_ERROR::WARN_LEVEL_ERROR;
  }

  return find_handler(thd, sql_errno, elevated_level);
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
sp_rcontext::push_handler(struct sp_cond_type *cond, uint h, int type, uint f)
{
  DBUG_ENTER("sp_rcontext::push_handler");
  DBUG_ASSERT(m_hcount < m_root_parsing_ctx->max_handler_index());

  m_handler[m_hcount].cond= cond;
  m_handler[m_hcount].handler= h;
  m_handler[m_hcount].type= type;
  m_handler[m_hcount].foffset= f;
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

void
sp_rcontext::enter_handler(int hid)
{
  DBUG_ENTER("sp_rcontext::enter_handler");
  DBUG_ASSERT(m_ihsp < m_root_parsing_ctx->max_handler_index());
  m_in_handler[m_ihsp++]= hid;
  DBUG_PRINT("info", ("m_ihsp: %d", m_ihsp));
  DBUG_VOID_RETURN;
}

void
sp_rcontext::exit_handler()
{
  DBUG_ENTER("sp_rcontext::exit_handler");
  DBUG_ASSERT(m_ihsp);
  m_ihsp-= 1;
  DBUG_PRINT("info", ("m_ihsp: %d", m_ihsp));
  DBUG_VOID_RETURN;
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
  if (mysql_open_cursor(thd, (uint) ALWAYS_MATERIALIZED_CURSOR, &result,
                        &server_side_cursor))
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
