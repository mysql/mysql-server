/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"    // NO_EMBEDDED_ACCESS_CHECKS
#include "sql_priv.h"
#include "sp_instr.h"
#include "item.h"         // Item_splocal
#include "opt_trace.h"    // opt_trace_disable_etc
#include "probes_mysql.h" // MYSQL_QUERY_EXEC_START
#include "sp_head.h"      // sp_head
#include "sp.h"           // sp_get_item_value
#include "sp_rcontext.h"  // sp_rcontext
#include "sql_acl.h"      // SELECT_ACL
#include "sql_base.h"     // open_temporary_tables
#include "sql_parse.h"    // check_table_access
#include "sql_prepare.h"  // reinit_stmt_before_use
#include "transaction.h"  // trans_commit_stmt

#include <algorithm>

///////////////////////////////////////////////////////////////////////////
// Static function implementation.
///////////////////////////////////////////////////////////////////////////


static int cmp_splocal_locations(Item_splocal * const *a,
                                 Item_splocal * const *b)
{
  return (int)((*a)->pos_in_query - (*b)->pos_in_query);
}


/*
  StoredRoutinesBinlogging
  This paragraph applies only to statement-based binlogging. Row-based
  binlogging does not need anything special like this.

  Top-down overview:

  1. Statements

  Statements that have is_update_query(stmt) == TRUE are written into the
  binary log verbatim.
  Examples:
    UPDATE tbl SET tbl.x = spfunc_w_side_effects()
    UPDATE tbl SET tbl.x=1 WHERE spfunc_w_side_effect_that_returns_false(tbl.y)

  Statements that have is_update_query(stmt) == FALSE (e.g. SELECTs) are not
  written into binary log. Instead we catch function calls the statement
  makes and write it into binary log separately (see #3).

  2. PROCEDURE calls

  CALL statements are not written into binary log. Instead
  * Any FUNCTION invocation (in SET, IF, WHILE, OPEN CURSOR and other SP
    instructions) is written into binlog separately.

  * Each statement executed in SP is binlogged separately, according to rules
    in #1, with the exception that we modify query string: we replace uses
    of SP local variables with NAME_CONST('spvar_name', <spvar-value>) calls.
    This substitution is done in subst_spvars().

  3. FUNCTION calls

  In sp_head::execute_function(), we check
   * If this function invocation is done from a statement that is written
     into the binary log.
   * If there were any attempts to write events to the binary log during
     function execution (grep for start_union_events and stop_union_events)

   If the answers are No and Yes, we write the function call into the binary
   log as "SELECT spfunc(<param1value>, <param2value>, ...)"


  4. Miscellaneous issues.

  4.1 User variables.

  When we call mysql_bin_log.write() for an SP statement, thd->user_var_events
  must hold set<{var_name, value}> pairs for all user variables used during
  the statement execution.
  This set is produced by tracking user variable reads during statement
  execution.

  For SPs, this has the following implications:
  1) thd->user_var_events may contain events from several SP statements and
     needs to be valid after execution of these statements was finished. In
     order to achieve that, we
     * Allocate user_var_events array elements on appropriate mem_root (grep
       for user_var_events_alloc).
     * Use is_query_in_union() to determine if user_var_event is created.

  2) We need to empty thd->user_var_events after we have wrote a function
     call. This is currently done by making
     reset_dynamic(&thd->user_var_events);
     calls in several different places. (TODO consider moving this into
     mysql_bin_log.write() function)

  4.2 Auto_increment storage in binlog

  As we may write two statements to binlog from one single logical statement
  (case of "SELECT func1(),func2()": it is binlogged as "SELECT func1()" and
  then "SELECT func2()"), we need to reset auto_increment binlog variables
  after each binlogged SELECT. Otherwise, the auto_increment value of the
  first SELECT would be used for the second too.
*/

/**
  Replace thd->query{_length} with a string that one can write to
  the binlog.

  The binlog-suitable string is produced by replacing references to SP local
  variables with NAME_CONST('sp_var_name', value) calls.

  @param thd        Current thread.
  @param instr      Instruction (we look for Item_splocal instances in
                    instr->free_list)
  @param query_str  Original query string

  @retval false on success.
  thd->query{_length} either has been appropriately replaced or there
  is no need for replacements.

  @retval true in case of out of memory error.
*/
static bool subst_spvars(THD *thd, sp_instr *instr, LEX_STRING *query_str)
{
  Dynamic_array<Item_splocal*> sp_vars_uses;
  char *pbuf, *cur, buffer[512];
  String qbuf(buffer, sizeof(buffer), &my_charset_bin);
  int prev_pos, res, buf_len;

  /* Find all instances of Item_splocal used in this statement */
  for (Item *item= instr->free_list; item; item= item->next)
  {
    if (item->is_splocal())
    {
      Item_splocal *item_spl= (Item_splocal*)item;
      if (item_spl->pos_in_query)
        sp_vars_uses.append(item_spl);
    }
  }

  if (!sp_vars_uses.elements())
    return false;

  /* Sort SP var refs by their occurrences in the query */
  sp_vars_uses.sort(cmp_splocal_locations);

  /*
    Construct a statement string where SP local var refs are replaced
    with "NAME_CONST(name, value)"
  */
  qbuf.length(0);
  cur= query_str->str;
  prev_pos= res= 0;
  thd->query_name_consts= 0;

  for (Item_splocal **splocal= sp_vars_uses.front(); 
       splocal <= sp_vars_uses.back(); splocal++)
  {
    Item *val;

    char str_buffer[STRING_BUFFER_USUAL_SIZE];
    String str_value_holder(str_buffer, sizeof(str_buffer),
                            &my_charset_latin1);
    String *str_value;

    /* append the text between sp ref occurrences */
    res|= qbuf.append(cur + prev_pos, (*splocal)->pos_in_query - prev_pos);
    prev_pos= (*splocal)->pos_in_query + (*splocal)->len_in_query;

    res|= (*splocal)->fix_fields(thd, (Item **) splocal);
    if (res)
      break;

    if ((*splocal)->limit_clause_param)
    {
      res|= qbuf.append_ulonglong((*splocal)->val_uint());
      if (res)
        break;
      continue;
    }

    /* append the spvar substitute */
    res|= qbuf.append(STRING_WITH_LEN(" NAME_CONST('"));
    res|= qbuf.append((*splocal)->m_name);
    res|= qbuf.append(STRING_WITH_LEN("',"));

    if (res)
      break;

    val= (*splocal)->this_item();
    str_value= sp_get_item_value(thd, val, &str_value_holder);
    if (str_value)
      res|= qbuf.append(*str_value);
    else
      res|= qbuf.append(STRING_WITH_LEN("NULL"));
    res|= qbuf.append(')');
    if (res)
      break;

    thd->query_name_consts++;
  }
  if (res ||
      qbuf.append(cur + prev_pos, query_str->length - prev_pos))
    return true;

  /*
    Allocate additional space at the end of the new query string for the
    query_cache_send_result_to_client function.

    The query buffer layout is:
       buffer :==
            <statement>   The input statement(s)
            '\0'          Terminating null char
            <length>      Length of following current database name (size_t)
            <db_name>     Name of current database
            <flags>       Flags struct
  */
  buf_len= qbuf.length() + 1 + sizeof(size_t) + thd->db_length + 
           QUERY_CACHE_FLAGS_SIZE + 1;
  if ((pbuf= (char *) alloc_root(thd->mem_root, buf_len)))
  {
    memcpy(pbuf, qbuf.ptr(), qbuf.length());
    pbuf[qbuf.length()]= 0;
    memcpy(pbuf+qbuf.length()+1, (char *) &thd->db_length, sizeof(size_t));
  }
  else
    return true;

  thd->set_query(pbuf, qbuf.length());

  return false;
}

///////////////////////////////////////////////////////////////////////////
// Sufficient max length of printed destinations and frame offsets (all uints).
///////////////////////////////////////////////////////////////////////////

#define SP_INSTR_UINT_MAXLEN  8
#define SP_STMT_PRINT_MAXLEN 40

///////////////////////////////////////////////////////////////////////////
// sp_lex_instr implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_lex_instr::reset_lex_and_exec_core(THD *thd,
                                           uint *nextp,
                                           bool open_tables)
{
  bool rc= false;

  /*
    The flag is saved at the entry to the following substatement.
    It's reset further in the common code part.
    It's merged with the saved parent's value at the exit of this func.
  */

  unsigned int parent_unsafe_rollback_flags=
    thd->transaction.stmt.get_unsafe_rollback_flags();
  thd->transaction.stmt.reset_unsafe_rollback_flags();

  /* Check pre-conditions. */

  DBUG_ASSERT(!thd->derived_tables);
  DBUG_ASSERT(thd->change_list.is_empty());

  /*
    Use our own lex.

    Although it is saved/restored in sp_head::execute() when we are
    entering/leaving routine, it's still should be saved/restored here,
    in order to properly behave in case of ER_NEED_REPREPARE error
    (when ER_NEED_REPREPARE happened, and we failed to re-parse the query).
  */

  LEX *lex_saved= thd->lex;
  thd->lex= m_lex;

  /* Set new query id. */

  thd->set_query_id(next_query_id());

  if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
  {
    /*
      This statement will enter/leave prelocked mode on its own.
      Entering prelocked mode changes table list and related members
      of LEX, so we'll need to restore them.
    */
    if (m_lex_query_tables_own_last)
    {
      /*
        We've already entered/left prelocked mode with this statement.
        Attach the list of tables that need to be prelocked and mark m_lex
        as having such list attached.
      */
      *m_lex_query_tables_own_last= m_prelocking_tables;
      m_lex->mark_as_requiring_prelocking(m_lex_query_tables_own_last);
    }
  }

  /* Reset LEX-object before re-use. */

  reinit_stmt_before_use(thd, m_lex);

  /* Open tables if needed. */

  if (open_tables)
  {
    /*
      IF, CASE, DECLARE, SET, RETURN, have 'open_tables' true; they may
      have a subquery in parameter and are worth tracing. They don't
      correspond to a SQL command so we pretend that they are SQLCOM_SELECT.
    */
    Opt_trace_start ots(thd, m_lex->query_tables, SQLCOM_SELECT,
                        &m_lex->var_list, NULL, 0, this,
                        thd->variables.character_set_client);
    Opt_trace_object trace_command(&thd->opt_trace);
    Opt_trace_array trace_command_steps(&thd->opt_trace, "steps");

    /*
      Check whenever we have access to tables for this statement
      and open and lock them before executing instructions core function.
      If we are not opening any tables, we don't need to check permissions
      either.
    */
    if (m_lex->query_tables)
      rc= (open_temporary_tables(thd, m_lex->query_tables) ||
            check_table_access(thd, SELECT_ACL, m_lex->query_tables, false,
                               UINT_MAX, false));

    if (!rc)
      rc= open_and_lock_tables(thd, m_lex->query_tables, true, 0);

    if (!rc)
    {
      rc= exec_core(thd, nextp);
      DBUG_PRINT("info",("exec_core returned: %d", rc));
    }

    /*
      Call after unit->cleanup() to close open table
      key read.
    */

    m_lex->unit.cleanup();

    /* Here we also commit or rollback the current statement. */

    if (! thd->in_sub_stmt)
    {
      thd->get_stmt_da()->set_overwrite_status(true);
      thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
      thd->get_stmt_da()->set_overwrite_status(false);
    }
    thd_proc_info(thd, "closing tables");
    close_thread_tables(thd);
    thd_proc_info(thd, 0);

    if (! thd->in_sub_stmt && ! thd->in_multi_stmt_transaction_mode())
      thd->mdl_context.release_transactional_locks();
    else if (! thd->in_sub_stmt)
      thd->mdl_context.release_statement_locks();
  }
  else
  {
    rc= exec_core(thd, nextp);
    DBUG_PRINT("info",("exec_core returned: %d", rc));
  }

  if (m_lex->query_tables_own_last)
  {
    /*
      We've entered and left prelocking mode when executing statement
      stored in m_lex.
      m_lex->query_tables(->next_global)* list now has a 'tail' - a list
      of tables that are added for prelocking. (If this is the first
      execution, the 'tail' was added by open_tables(), otherwise we've
      attached it above in this function).
      Now we'll save the 'tail', and detach it.
    */
    m_lex_query_tables_own_last= m_lex->query_tables_own_last;
    m_prelocking_tables= *m_lex_query_tables_own_last;
    *m_lex_query_tables_own_last= NULL;
    m_lex->mark_as_requiring_prelocking(NULL);
  }

  /* Rollback changes to the item tree during execution. */

  thd->rollback_item_tree_changes();

  /*
    Update the state of the active arena if no errors on
    open_tables stage.
  */

  if (!rc || !thd->is_error() ||
      (thd->get_stmt_da()->mysql_errno() != ER_CANT_REOPEN_TABLE &&
       thd->get_stmt_da()->mysql_errno() != ER_NO_SUCH_TABLE &&
       thd->get_stmt_da()->mysql_errno() != ER_UPDATE_TABLE_USED))
    thd->stmt_arena->state= Query_arena::STMT_EXECUTED;

  /*
    Merge here with the saved parent's values
    what is needed from the substatement gained
  */

  thd->transaction.stmt.add_unsafe_rollback_flags(parent_unsafe_rollback_flags);

  /* Restore original lex. */

  thd->lex= lex_saved;

  /*
    Unlike for PS we should not call Item's destructors for newly created
    items after execution of each instruction in stored routine. This is
    because SP often create Item (like Item_int, Item_string etc...) when
    they want to store some value in local variable, pass return value and
    etc... So their life time should be longer than one instruction.

    cleanup_items() is called in sp_head::execute()
  */

  return rc || thd->is_error();
}


LEX *sp_lex_instr::parse_expr(THD *thd, sp_head *sp)
{
  String sql_query;
  PSI_statement_locker *parent_locker= thd->m_statement_psi;
  sql_query.set_charset(system_charset_info);

  get_query(&sql_query);

  if (sql_query.length() == 0)
  {
    // The instruction has returned zero-length query string. That means, the
    // re-preparation of the instruction is not possible. We should not come
    // here in the normal life.
    DBUG_ASSERT(false);
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    return NULL;
  }

  // Prepare parser state. It can be done just before parse_sql(), do it here
  // only to simplify exit in case of failure (out-of-memory error).

  Parser_state parser_state;

  if (parser_state.init(thd, sql_query.c_ptr(), sql_query.length()))
    return NULL;

  // Cleanup current THD from previously held objects before new parsing.

  cleanup_before_parsing(thd);

  // Switch mem-roots. We need to store new LEX and its Items in the persistent
  // SP-memory (memory which is not freed between executions).

  MEM_ROOT *execution_mem_root= thd->mem_root;

  thd->mem_root= thd->sp_runtime_ctx->sp->get_persistent_mem_root();

  // Switch THD::free_list. It's used to remember the newly created set of Items
  // during parsing. We should clean those items after each execution.

  Item *execution_free_list= thd->free_list;
  thd->free_list= NULL;

  // Create a new LEX and intialize it.

  LEX *lex_saved= thd->lex;

  thd->lex= new (thd->mem_root) st_lex_local;
  lex_start(thd);

  thd->lex->sphead= sp;
  thd->lex->set_sp_current_parsing_ctx(get_parsing_ctx());
  sp->m_parser_data.set_current_stmt_start_ptr(sql_query.c_ptr());

  // Parse the just constructed SELECT-statement.

  thd->m_statement_psi= NULL;
  bool parsing_failed= parse_sql(thd, &parser_state, NULL);
  thd->m_statement_psi= parent_locker;

  if (!parsing_failed)
  {
    thd->lex->set_trg_event_type_for_tables();

    if (sp->m_type == SP_TYPE_TRIGGER)
    {
      /*
        Also let us bind these objects to Field objects in table being opened.

        We ignore errors of setup_field() here, because if even something is
        wrong we still will be willing to open table to perform some operations
        (e.g.  SELECT)... Anyway some things can be checked only during trigger
        execution.
      */

      Table_triggers_list *ttl= sp->m_trg_list;
      int event= sp->m_trg_chistics.event;
      int action_time= sp->m_trg_chistics.action_time;
      GRANT_INFO *grant_table= &ttl->subject_table_grants[event][action_time];

      for (Item_trigger_field *trg_field= sp->m_trg_table_fields.first;
           trg_field;
           trg_field= trg_field->next_trg_field)
      {
        trg_field->setup_field(thd, ttl->trigger_table, grant_table);
      }
    }

    // Call after-parsing callback.

    parsing_failed= on_after_expr_parsing(thd);

    // Append newly created Items to the list of Items, owned by this
    // instruction.

    free_list= thd->free_list;
  }

  // Restore THD::lex.

  thd->lex->sphead= NULL;
  thd->lex->set_sp_current_parsing_ctx(NULL);

  LEX *expr_lex= thd->lex;
  thd->lex= lex_saved;

  // Restore execution mem-root and THD::free_list.

  thd->mem_root= execution_mem_root;
  thd->free_list= execution_free_list;

  // That's it.

  return parsing_failed ? NULL : expr_lex;
}


bool sp_lex_instr::validate_lex_and_execute_core(THD *thd,
                                                 uint *nextp,
                                                 bool open_tables)
{
  Reprepare_observer reprepare_observer;
  int reprepare_attempt= 0;

  while (true)
  {
    if (is_invalid())
    {
      LEX *lex= parse_expr(thd, thd->sp_runtime_ctx->sp);

      if (!lex)
        return true;

      set_lex(lex, true);

      m_first_execution= true;
    }

    /*
      Install the metadata observer. If some metadata version is
      different from prepare time and an observer is installed,
      the observer method will be invoked to push an error into
      the error stack.
    */
    Reprepare_observer *stmt_reprepare_observer= NULL;

    /*
      Meta-data versions are stored in the LEX-object on the first execution.
      Thus, the reprepare observer should not be installed for the first
      execution, because it will always be triggered.

      Then, the reprepare observer should be installed for the statements, which
      are marked by CF_REEXECUTION_FRAGILE (@sa CF_REEXECUTION_FRAGILE) or if
      the SQL-command is SQLCOM_END, which means that the LEX-object is
      representing an expression, so the exact SQL-command does not matter.
    */

    if (!m_first_execution &&
        (sql_command_flags[m_lex->sql_command] & CF_REEXECUTION_FRAGILE ||
         m_lex->sql_command == SQLCOM_END))
    {
      reprepare_observer.reset_reprepare_observer();
      stmt_reprepare_observer= &reprepare_observer;
    }

    thd->push_reprepare_observer(stmt_reprepare_observer);

    bool rc= reset_lex_and_exec_core(thd, nextp, open_tables);

    thd->pop_reprepare_observer();

    m_first_execution= false;

    if (!rc)
      return false;

    /*
      Here is why we need all the checks below:
        - if the reprepare observer is not set, we've got an error, which should
          be raised to the user;
        - if we've got fatal error, it should be raised to the user;
        - if our thread got killed during execution, the error should be raised
          to the user;
        - if we've got an error, different from ER_NEED_REPREPARE, we need to
          raise it to the user;
        - we take only 3 attempts to reprepare the query, otherwise we might end
          up in the endless loop.
    */
    if (stmt_reprepare_observer &&
        !thd->is_fatal_error &&
        !thd->killed &&
        thd->get_stmt_da()->mysql_errno() == ER_NEED_REPREPARE &&
        reprepare_attempt++ < 3)
    {
      DBUG_ASSERT(stmt_reprepare_observer->is_invalidated());

      thd->clear_error();
      free_lex();
      invalidate();
    }
    else
      return true;
  }
}


void sp_lex_instr::set_lex(LEX *lex, bool is_lex_owner)
{
  free_lex();

  m_lex= lex;
  m_is_lex_owner= is_lex_owner;
  m_lex_query_tables_own_last= NULL;

  if (m_lex)
    m_lex->sp_lex_in_use= true;
}


void sp_lex_instr::free_lex()
{
  if (!m_is_lex_owner || !m_lex)
    return;

  /* Prevent endless recursion. */
  m_lex->sphead= NULL;
  lex_end(m_lex);
  delete (st_lex_local *) m_lex;

  m_lex= NULL;
  m_is_lex_owner= false;
  m_lex_query_tables_own_last= NULL;
}


void sp_lex_instr::cleanup_before_parsing(THD *thd)
{
  /*
    Destroy items in the instruction's free list before re-parsing the
    statement query string (and thus, creating new items).
  */
  Item *p= free_list;
  while (p)
  {
    Item *next= p->next;
    p->delete_self();
    p= next;
  }

  free_list= NULL;

  // Remove previously stored trigger-field items.
  sp_head *sp= thd->sp_runtime_ctx->sp;

  if (sp->m_type == SP_TYPE_TRIGGER)
    sp->m_trg_table_fields.empty();
}


void sp_lex_instr::get_query(String *sql_query) const
{
  LEX_STRING expr_query= this->get_expr_query();

  if (!expr_query.str)
  {
    sql_query->length(0);
    return;
  }

  sql_query->append("SELECT ");
  sql_query->append(expr_query.str, expr_query.length);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_stmt implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_stmt::execute(THD *thd, uint *nextp)
{
  bool need_subst= false;
  bool rc= false;

  DBUG_PRINT("info", ("query: '%.*s'", (int) m_query.length, m_query.str));

  const CSET_STRING query_backup= thd->query_string;

#if defined(ENABLED_PROFILING)
  /* This SP-instr is profilable and will be captured. */
  thd->profiling.set_query_source(m_query.str, m_query.length);
#endif

  /*
    If we can't set thd->query_string at all, we give up on this statement.
  */
  if (alloc_query(thd, m_query.str, m_query.length))
    return true;

  /*
    Check whether we actually need a substitution of SP variables with
    NAME_CONST(...) (using subst_spvars()).
    If both of the following apply, we won't need to substitute:

    - general log is off

    - binary logging is off, or not in statement mode

    We don't have to substitute on behalf of the query cache as
    queries with SP vars are not cached, anyway.

    query_name_consts is used elsewhere in a special case concerning
    CREATE TABLE, but we do not need to do anything about that here.

    The slow query log is another special case: we won't know whether a
    query qualifies for the slow query log until after it's been
    executed. We assume that most queries are not slow, so we do not
    pre-emptively substitute just for the slow query log. If a query
    ends up being slow after all and we haven't done the substitution
    already for any of the above (general log etc.), we'll do the
    substitution immediately before writing to the log.
  */

  need_subst= ((thd->variables.option_bits & OPTION_LOG_OFF) &&
               (!(thd->variables.option_bits & OPTION_BIN_LOG) ||
                !mysql_bin_log.is_open() ||
                thd->is_current_stmt_binlog_format_row())) ? FALSE : TRUE;

  /*
    If we need to do a substitution but can't (OOM), give up.
  */

  if (need_subst && subst_spvars(thd, this, &m_query))
    return true;

  /*
    (the order of query cache and subst_spvars calls is irrelevant because
    queries with SP vars can't be cached)
  */
  if (unlikely((thd->variables.option_bits & OPTION_LOG_OFF)==0))
    general_log_write(thd, COM_QUERY, thd->query(), thd->query_length());

  if (query_cache_send_result_to_client(thd, thd->query(),
                                        thd->query_length()) <= 0)
  {
    rc= validate_lex_and_execute_core(thd, nextp, false);

    if (thd->get_stmt_da()->is_eof())
    {
      /* Finalize server status flags after executing a statement. */
      thd->update_server_status();

      thd->protocol->end_statement();
    }

    query_cache_end_of_result(thd);

    if (!rc && unlikely(log_slow_applicable(thd)))
    {
      /*
        We actually need to write the slow log. Check whether we already
        called subst_spvars() above, otherwise, do it now.  In the highly
        unlikely event of subst_spvars() failing (OOM), we'll try to log
        the unmodified statement instead.
      */
      if (!need_subst)
        rc= subst_spvars(thd, this, &m_query);
      log_slow_do(thd);
    }

    /*
      With the current setup, a subst_spvars() and a mysql_rewrite_query()
      (rewriting passwords etc.) will not both happen to a query.
      If this ever changes, we give the engineer pause here so they will
      double-check whether the potential conflict they created is a
      problem.
    */
    DBUG_ASSERT((thd->query_name_consts == 0) ||
                (thd->rewritten_query.length() == 0));
  }
  else
    *nextp= get_ip() + 1;

  thd->set_query(query_backup);
  thd->query_name_consts= 0;

  if (!thd->is_error())
    thd->get_stmt_da()->reset_diagnostics_area();

  return rc || thd->is_error();
}


void sp_instr_stmt::print(String *str)
{
  /* stmt CMD "..." */
  if (str->reserve(SP_STMT_PRINT_MAXLEN + SP_INSTR_UINT_MAXLEN + 8))
    return;
  str->qs_append(STRING_WITH_LEN("stmt"));
  str->qs_append(STRING_WITH_LEN(" \""));

  /*
    Print the query string (but not too much of it), just to indicate which
    statement it is.
  */
  uint len= m_query.length;
  if (len > SP_STMT_PRINT_MAXLEN)
    len= SP_STMT_PRINT_MAXLEN-3;

  /* Copy the query string and replace '\n' with ' ' in the process */
  for (uint i= 0 ; i < len ; i++)
  {
    char c= m_query.str[i];
    if (c == '\n')
      c= ' ';
    str->qs_append(c);
  }
  if (m_query.length > SP_STMT_PRINT_MAXLEN)
    str->qs_append(STRING_WITH_LEN("...")); /* Indicate truncated string */
  str->qs_append('"');
}


bool sp_instr_stmt::exec_core(THD *thd, uint *nextp)
{
  MYSQL_QUERY_EXEC_START(thd->query(),
                         thd->thread_id,
                         (char *) (thd->db ? thd->db : ""),
                         &thd->security_ctx->priv_user[0],
                         (char *)thd->security_ctx->host_or_ip,
                         3);

  thd->lex->set_sp_current_parsing_ctx(get_parsing_ctx());
  thd->lex->sphead= thd->sp_runtime_ctx->sp;

  PSI_statement_locker *statement_psi_saved= thd->m_statement_psi;
  thd->m_statement_psi= NULL;

  bool rc= mysql_execute_command(thd);

  thd->lex->set_sp_current_parsing_ctx(NULL);
  thd->lex->sphead= NULL;
  thd->m_statement_psi= statement_psi_saved;

  MYSQL_QUERY_EXEC_DONE(rc);

  *nextp= get_ip() + 1;

  return rc;
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_set implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_set::exec_core(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  if (!thd->sp_runtime_ctx->set_variable(thd, m_offset, &m_value_item))
    return false;

  /* Failed to evaluate the value. Reset the variable to NULL. */

  if (thd->sp_runtime_ctx->set_variable(thd, m_offset, 0))
  {
    /* If this also failed, let's abort. */
    my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
  }

  return true;
}


void sp_instr_set::print(String *str)
{
  /* set name@offset ... */
  int rsrv = SP_INSTR_UINT_MAXLEN+6;
  sp_variable *var = m_parsing_ctx->find_variable(m_offset);

  /* 'var' should always be non-null, but just in case... */
  if (var)
    rsrv+= var->name.length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("set "));
  if (var)
  {
    str->qs_append(var->name.str, var->name.length);
    str->qs_append('@');
  }
  str->qs_append(m_offset);
  str->qs_append(' ');
  m_value_item->print(str, QT_TO_ARGUMENT_CHARSET);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_set_trigger_field implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_set_trigger_field::exec_core(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;
  thd->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  return m_trigger_field->set_value(thd, &m_value_item);
}


void sp_instr_set_trigger_field::print(String *str)
{
  str->append(STRING_WITH_LEN("set_trigger_field "));
  m_trigger_field->print(str, QT_ORDINARY);
  str->append(STRING_WITH_LEN(":="));
  m_value_item->print(str, QT_TO_ARGUMENT_CHARSET);
}


bool sp_instr_set_trigger_field::on_after_expr_parsing(THD *thd)
{
  DBUG_ASSERT(thd->lex->select_lex.item_list.elements == 1);

  m_value_item= thd->lex->select_lex.item_list.head();

  DBUG_ASSERT(!m_trigger_field);

  m_trigger_field=
    new (thd->mem_root) Item_trigger_field(thd->lex->current_context(),
                                           Item_trigger_field::NEW_ROW,
                                           m_trigger_field_name.str,
                                           UPDATE_ACL,
                                           false);

  return m_value_item == NULL || m_trigger_field == NULL;
}


void sp_instr_set_trigger_field::cleanup_before_parsing(THD *thd)
{
  sp_lex_instr::cleanup_before_parsing(thd);

  m_trigger_field= NULL;
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_jump implementation.
///////////////////////////////////////////////////////////////////////////


void sp_instr_jump::print(String *str)
{
  /* jump dest */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+5))
    return;
  str->qs_append(STRING_WITH_LEN("jump "));
  str->qs_append(m_dest);
}


uint sp_instr_jump::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_dest= opt_shortcut_jump(sp, this);
  if (m_dest != get_ip() + 1)   /* Jumping to following instruction? */
    m_marked= true;
  m_optdest= sp->get_instr(m_dest);
  return m_dest;
}


uint sp_instr_jump::opt_shortcut_jump(sp_head *sp, sp_instr *start)
{
  uint dest= m_dest;
  sp_instr *i;

  while ((i= sp->get_instr(dest)))
  {
    uint ndest;

    if (start == i || this == i)
      break;
    ndest= i->opt_shortcut_jump(sp, start);
    if (ndest == dest)
      break;
    dest= ndest;
  }
  return dest;
}


void sp_instr_jump::opt_move(uint dst, List<sp_branch_instr> *bp)
{
  if (m_dest > get_ip())
    bp->push_back(this);      // Forward
  else if (m_optdest)
    m_dest= m_optdest->get_ip();  // Backward
  m_ip= dst;
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_jump_if_not class implementation
///////////////////////////////////////////////////////////////////////////


bool sp_instr_jump_if_not::exec_core(THD *thd, uint *nextp)
{
  DBUG_ASSERT(m_expr_item);

  Item *item= sp_prepare_func_item(thd, &m_expr_item);

  if (!item)
    return true;

  *nextp= item->val_bool() ? get_ip() + 1 : m_dest;

  return false;
}


void sp_instr_jump_if_not::print(String *str)
{
  /* jump_if_not dest(cont) ... */
  if (str->reserve(2*SP_INSTR_UINT_MAXLEN+14+32)) // Add some for the expr. too
    return;
  str->qs_append(STRING_WITH_LEN("jump_if_not "));
  str->qs_append(m_dest);
  str->qs_append('(');
  str->qs_append(m_cont_dest);
  str->qs_append(STRING_WITH_LEN(") "));
  m_expr_item->print(str, QT_ORDINARY);
}


///////////////////////////////////////////////////////////////////////////
// sp_lex_branch_instr implementation.
///////////////////////////////////////////////////////////////////////////


uint sp_lex_branch_instr::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_marked= true;

  sp_instr *i= sp->get_instr(m_dest);

  if (i)
  {
    m_dest= i->opt_shortcut_jump(sp, this);
    m_optdest= sp->get_instr(m_dest);
  }

  sp->add_mark_lead(m_dest, leads);

  i= sp->get_instr(m_cont_dest);

  if (i)
  {
    m_cont_dest= i->opt_shortcut_jump(sp, this);
    m_cont_optdest= sp->get_instr(m_cont_dest);
  }

  sp->add_mark_lead(m_cont_dest, leads);

  return get_ip() + 1;
}


void sp_lex_branch_instr::opt_move(uint dst, List<sp_branch_instr> *bp)
{
  /*
    cont. destinations may point backwards after shortcutting jumps
    during the mark phase. If it's still pointing forwards, only
    push this for backpatching if sp_instr_jump::opt_move() will not
    do it (i.e. if the m_dest points backwards).
   */
  if (m_cont_dest > get_ip())
  {                             // Forward
    if (m_dest < get_ip())
      bp->push_back(this);
  }
  else if (m_cont_optdest)
    m_cont_dest= m_cont_optdest->get_ip(); // Backward

  /* This will take care of m_dest and m_ip */
  if (m_dest > get_ip())
    bp->push_back(this);      // Forward
  else if (m_optdest)
    m_dest= m_optdest->get_ip();  // Backward
  m_ip= dst;
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_jump_case_when implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_jump_case_when::exec_core(THD *thd, uint *nextp)
{
  DBUG_ASSERT(m_eq_item);

  Item *item= sp_prepare_func_item(thd, &m_eq_item);

  if (!item)
    return true;

  *nextp= item->val_bool() ? get_ip() + 1 : m_dest;

  return false;
}


void sp_instr_jump_case_when::print(String *str)
{
  /* jump_if_not dest(cont) ... */
  if (str->reserve(2*SP_INSTR_UINT_MAXLEN+14+32)) // Add some for the expr. too
    return;
  str->qs_append(STRING_WITH_LEN("jump_if_not_case_when "));
  str->qs_append(m_dest);
  str->qs_append('(');
  str->qs_append(m_cont_dest);
  str->qs_append(STRING_WITH_LEN(") "));
  m_eq_item->print(str, QT_ORDINARY);
}


bool sp_instr_jump_case_when::build_expr_items(THD *thd)
{
  // Setup CASE-expression item (m_case_expr_item).

  m_case_expr_item= new Item_case_expr(m_case_expr_id);

  if (!m_case_expr_item)
    return true;

#ifndef DBUG_OFF
  m_case_expr_item->m_sp= thd->lex->sphead;
#endif

  // Setup WHEN-expression item (m_expr_item) if it is not already set.
  //
  // This function can be called in two cases:
  //
  //   - during initial (regular) parsing of SP. In this case we don't have
  //     lex->select_lex (because it's not a SELECT statement), but
  //     m_expr_item is already set in constructor.
  //
  //   - during re-parsing after meta-data change. In this case we've just
  //     parsed aux-SELECT statement, so we need to take 1st (and the only one)
  //     item from its list.

  if (!m_expr_item)
  {
    DBUG_ASSERT(thd->lex->select_lex.item_list.elements == 1);

    m_expr_item= thd->lex->select_lex.item_list.head();
  }

  // Setup main expression item (m_expr_item).

  m_eq_item= new Item_func_eq(m_case_expr_item, m_expr_item);

  if (!m_eq_item)
    return true;

  return false;
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_freturn implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_freturn::exec_core(THD *thd, uint *nextp)
{
  /*
    RETURN is a "procedure statement" (in terms of the SQL standard).
    That means, Diagnostics Area should be clean before its execution.
  */

  Diagnostics_area *da= thd->get_stmt_da();
  da->reset_condition_info(da->statement_id());

  /*
    Change <next instruction pointer>, so that this will be the last
    instruction in the stored function.
  */

  *nextp= UINT_MAX;

  /*
    Evaluate the value of return expression and store it in current runtime
    context.

    NOTE: It's necessary to evaluate result item right here, because we must
    do it in scope of execution the current context/block.
  */

  return thd->sp_runtime_ctx->set_return_value(thd, &m_expr_item);
}


void sp_instr_freturn::print(String *str)
{
  /* freturn type expr... */
  if (str->reserve(1024+8+32)) // Add some for the expr. too
    return;
  str->qs_append(STRING_WITH_LEN("freturn "));
  str->qs_append((uint) m_return_field_type);
  str->qs_append(' ');
  m_expr_item->print(str, QT_ORDINARY);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_hpush_jump implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_hpush_jump::execute(THD *thd, uint *nextp)
{
  *nextp= m_dest;

  return thd->sp_runtime_ctx->push_handler(m_handler, get_ip() + 1);
}


void sp_instr_hpush_jump::print(String *str)
{
  /* hpush_jump dest fsize type */
  if (str->reserve(SP_INSTR_UINT_MAXLEN*2 + 21))
    return;

  str->qs_append(STRING_WITH_LEN("hpush_jump "));
  str->qs_append(m_dest);
  str->qs_append(' ');
  str->qs_append(m_frame);

  switch (m_handler->type) {
  case sp_handler::EXIT:
    str->qs_append(STRING_WITH_LEN(" EXIT"));
    break;
  case sp_handler::CONTINUE:
    str->qs_append(STRING_WITH_LEN(" CONTINUE"));
    break;
  default:
    // The handler type must be either CONTINUE or EXIT.
    DBUG_ASSERT(0);
  }
}


uint sp_instr_hpush_jump::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_marked= true;

  sp_instr *i= sp->get_instr(m_dest);

  if (i)
  {
    m_dest= i->opt_shortcut_jump(sp, this);
    m_optdest= sp->get_instr(m_dest);
  }

  sp->add_mark_lead(m_dest, leads);

  /*
    For continue handlers, all instructions in the scope of the handler
    are possible leads. For example, the instruction after freturn might
    be executed if the freturn triggers the condition handled by the
    continue handler.

    m_dest marks the start of the handler scope. It's added as a lead
    above, so we start on m_dest+1 here.
    m_opt_hpop is the hpop marking the end of the handler scope.
  */
  if (m_handler->type == sp_handler::CONTINUE)
  {
    for (uint scope_ip= m_dest+1; scope_ip <= m_opt_hpop; scope_ip++)
      sp->add_mark_lead(scope_ip, leads);
  }

  return get_ip() + 1;
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_hpop implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_hpop::execute(THD *thd, uint *nextp)
{
  thd->sp_runtime_ctx->pop_handlers(m_parsing_ctx);
  *nextp= get_ip() + 1;
  return false;
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_hreturn implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_hreturn::execute(THD *thd, uint *nextp)
{
  /*
    Obtain next instruction pointer (m_dest is set for EXIT handlers, retrieve
    the instruction pointer from runtime context for CONTINUE handlers).
  */

  sp_rcontext *rctx= thd->sp_runtime_ctx;

  *nextp= m_dest ? m_dest : rctx->get_last_handler_continue_ip();

  /*
    Remove call frames for handlers, which are "below" the BEGIN..END block of
    the next instruction.
  */

  sp_instr *next_instr= rctx->sp->get_instr(*nextp);
  rctx->exit_handler(thd, next_instr->get_parsing_ctx());

  return false;
}


void sp_instr_hreturn::print(String *str)
{
  /* hreturn framesize dest */
  if (str->reserve(SP_INSTR_UINT_MAXLEN*2 + 9))
    return;
  str->qs_append(STRING_WITH_LEN("hreturn "));
  if (m_dest)
  {
    // NOTE: this is legacy: hreturn instruction for EXIT handler
    // should print out 0 as frame index.
    str->qs_append(STRING_WITH_LEN("0 "));
    str->qs_append(m_dest);
  }
  else
  {
    str->qs_append(m_frame);
  }
}


uint sp_instr_hreturn::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_marked= true;

  if (m_dest)
  {
    /*
      This is an EXIT handler; next instruction step is in m_dest.
     */
    return m_dest;
  }

  /*
    This is a CONTINUE handler; next instruction step will come from
    the handler stack and not from opt_mark.
   */
  return UINT_MAX;
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_cpush implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_cpush::execute(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  // sp_instr_cpush::execute() just registers the cursor in the runtime context.

  return thd->sp_runtime_ctx->push_cursor(this);
}


bool sp_instr_cpush::exec_core(THD *thd, uint *nextp)
{
  sp_cursor *c= thd->sp_runtime_ctx->get_cursor(m_cursor_idx);

  // sp_instr_cpush::exec_core() opens the cursor (it's called from
  // sp_instr_copen::execute().

  return c ? c->open(thd) : true;
}


void sp_instr_cpush::print(String *str)
{
  const LEX_STRING *cursor_name= m_parsing_ctx->find_cursor(m_cursor_idx);

  uint rsrv= SP_INSTR_UINT_MAXLEN + 7 + m_cursor_query.length + 1;

  if (cursor_name)
    rsrv+= cursor_name->length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cpush "));
  if (cursor_name)
  {
    str->qs_append(cursor_name->str, cursor_name->length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor_idx);

  str->qs_append(':');
  str->qs_append(m_cursor_query.str, m_cursor_query.length);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_cpop implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_cpop::execute(THD *thd, uint *nextp)
{
  thd->sp_runtime_ctx->pop_cursors(m_count);
  *nextp= get_ip() + 1;

  return false;
}


void sp_instr_cpop::print(String *str)
{
  /* cpop count */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+5))
    return;
  str->qs_append(STRING_WITH_LEN("cpop "));
  str->qs_append(m_count);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_copen implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_copen::execute(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  // Get the cursor pointer.

  sp_cursor *c= thd->sp_runtime_ctx->get_cursor(m_cursor_idx);

  if (!c)
    return true;

  // Retrieve sp_instr_cpush instance.

  sp_instr_cpush *push_instr= c->get_push_instr();

  // Switch Statement Arena to the sp_instr_cpush object. It contains the
  // free_list of the query, so new items (if any) are stored in the right
  // free_list, and we can cleanup after each open.

  Query_arena *stmt_arena_saved= thd->stmt_arena;
  thd->stmt_arena= push_instr;

  // Switch to the cursor's lex and execute sp_instr_cpush::exec_core().
  // sp_instr_cpush::exec_core() is *not* executed during
  // sp_instr_cpush::execute(). sp_instr_cpush::exec_core() is intended to be
  // executed on cursor opening.

  bool rc= push_instr->validate_lex_and_execute_core(thd, nextp, false);

  // Cleanup the query's items.

  if (push_instr->free_list)
    cleanup_items(push_instr->free_list);

  // Restore Statement Arena.

  thd->stmt_arena= stmt_arena_saved;

  return rc;
}


void sp_instr_copen::print(String *str)
{
  const LEX_STRING *cursor_name= m_parsing_ctx->find_cursor(m_cursor_idx);

  /* copen name@offset */
  uint rsrv= SP_INSTR_UINT_MAXLEN+7;

  if (cursor_name)
    rsrv+= cursor_name->length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("copen "));
  if (cursor_name)
  {
    str->qs_append(cursor_name->str, cursor_name->length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor_idx);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_cclose implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_cclose::execute(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  sp_cursor *c= thd->sp_runtime_ctx->get_cursor(m_cursor_idx);

  return c ? c->close(thd) : true;
}


void sp_instr_cclose::print(String *str)
{
  const LEX_STRING *cursor_name= m_parsing_ctx->find_cursor(m_cursor_idx);

  /* cclose name@offset */
  uint rsrv= SP_INSTR_UINT_MAXLEN+8;

  if (cursor_name)
    rsrv+= cursor_name->length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cclose "));
  if (cursor_name)
  {
    str->qs_append(cursor_name->str, cursor_name->length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor_idx);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_cfetch implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_cfetch::execute(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  sp_cursor *c= thd->sp_runtime_ctx->get_cursor(m_cursor_idx);

  return c ? c->fetch(thd, &m_varlist) : true;
}


void sp_instr_cfetch::print(String *str)
{
  List_iterator_fast<sp_variable> li(m_varlist);
  sp_variable *pv;
  const LEX_STRING *cursor_name= m_parsing_ctx->find_cursor(m_cursor_idx);

  /* cfetch name@offset vars... */
  uint rsrv= SP_INSTR_UINT_MAXLEN+8;

  if (cursor_name)
    rsrv+= cursor_name->length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cfetch "));
  if (cursor_name)
  {
    str->qs_append(cursor_name->str, cursor_name->length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor_idx);
  while ((pv= li++))
  {
    if (str->reserve(pv->name.length+SP_INSTR_UINT_MAXLEN+2))
      return;
    str->qs_append(' ');
    str->qs_append(pv->name.str, pv->name.length);
    str->qs_append('@');
    str->qs_append(pv->offset);
  }
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_error implementation.
///////////////////////////////////////////////////////////////////////////


void sp_instr_error::print(String *str)
{
  /* error code */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+6))
    return;
  str->qs_append(STRING_WITH_LEN("error "));
  str->qs_append(m_errcode);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_set_case_expr implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_instr_set_case_expr::exec_core(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  sp_rcontext *rctx= thd->sp_runtime_ctx;

  if (rctx->set_case_expr(thd, m_case_expr_id, &m_expr_item) &&
      !rctx->get_case_expr(m_case_expr_id))
  {
    // Failed to evaluate the value, the case expression is still not
    // initialized. Set to NULL so we can continue.

    Item *null_item= new Item_null();

    if (!null_item || rctx->set_case_expr(thd, m_case_expr_id, &null_item))
    {
      // If this also failed, we have to abort.
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
    }

    return true;
  }

  return false;
}


void sp_instr_set_case_expr::print(String *str)
{
  /* set_case_expr (cont) id ... */
  str->reserve(2*SP_INSTR_UINT_MAXLEN+18+32); // Add some extra for expr too
  str->qs_append(STRING_WITH_LEN("set_case_expr ("));
  str->qs_append(m_cont_dest);
  str->qs_append(STRING_WITH_LEN(") "));
  str->qs_append(m_case_expr_id);
  str->qs_append(' ');
  m_expr_item->print(str, QT_ORDINARY);
}


uint sp_instr_set_case_expr::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_marked= true;

  sp_instr *i= sp->get_instr(m_cont_dest);

  if (i)
  {
    m_cont_dest= i->opt_shortcut_jump(sp, this);
    m_cont_optdest= sp->get_instr(m_cont_dest);
  }

  sp->add_mark_lead(m_cont_dest, leads);
  return get_ip() + 1;
}


void sp_instr_set_case_expr::opt_move(uint dst, List<sp_branch_instr> *bp)
{
  if (m_cont_dest > get_ip())
    bp->push_back(this);        // Forward
  else if (m_cont_optdest)
    m_cont_dest= m_cont_optdest->get_ip(); // Backward
  m_ip= dst;
}
