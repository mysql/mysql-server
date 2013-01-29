/*
   Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"         // NO_EMBEDDED_ACCESS_CHECKS
#include "sql_priv.h"
#include "unireg.h"
#include "sql_prepare.h"
#include "sql_cache.h"         // query_cache_*
#include "probes_mysql.h"
#include "sql_show.h"          // append_identifier
#include "sql_db.h"            // mysql_opt_change_db, mysql_change_db
#include "sql_table.h"         // prepare_create_field
#include "sql_acl.h"           // *_ACL
#include "sql_array.h"         // Dynamic_array
#include "log_event.h"         // append_query_string, Query_log_event

#include "sp_head.h"
#include "sp_instr.h"
#include "sp.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"
#include "sp_cache.h"
#include "set_var.h"
#include "sql_parse.h"         // cleanup_items
#include "sql_base.h"          // close_thread_tables
#include "transaction.h"       // trans_commit_stmt
#include "opt_trace.h"         // opt_trace_disable_etc
#include "global_threads.h"

#include <my_user.h>           // parse_user

/**
  SP_TABLE represents all instances of one table in an optimized multi-set of
  tables used by a stored program.
*/
struct SP_TABLE
{
  /*
    Multi-set key:
      db_name\0table_name\0alias\0 - for normal tables
      db_name\0table_name\0        - for temporary tables
    Note that in both cases we don't take last '\0' into account when
    we count length of key.
  */
  LEX_STRING qname;
  uint db_length, table_name_length;
  bool temp;               /* true if corresponds to a temporary table */
  thr_lock_type lock_type; /* lock type used for prelocking */
  uint lock_count;
  uint query_lock_count;
  uint8 trg_event_map;
};


///////////////////////////////////////////////////////////////////////////
// Static function implementations.
///////////////////////////////////////////////////////////////////////////


uchar *sp_table_key(const uchar *ptr, size_t *plen, my_bool first)
{
  SP_TABLE *tab= (SP_TABLE *)ptr;
  *plen= tab->qname.length;
  return (uchar *)tab->qname.str;
}


/**
  Helper function which operates on a THD object to set the query start_time to
  the current time.

  @param thd  Thread context.
*/
static void reset_start_time_for_sp(THD *thd)
{
  if (thd->in_sub_stmt)
    return;

  /*
    First investigate if there is a cached time stamp
  */
  if (thd->user_time.tv_sec || thd->user_time.tv_usec)
    thd->start_time= thd->user_time;
  else
    my_micro_time_to_timeval(my_micro_time(), &thd->start_time);
}


/**
  Merge contents of two hashes representing sets of routines used
  by statements or by other routines.

  @param dst   hash to which elements should be added
  @param src   hash from which elements merged

  @note
    This procedure won't create new Sroutine_hash_entry objects,
    instead it will simply add elements from source to destination
    hash. Thus time of life of elements in destination hash becomes
    dependant on time of life of elements from source hash. It also
    won't touch lists linking elements in source and destination
    hashes.

    @return Error status.
*/

static bool sp_update_sp_used_routines(HASH *dst, HASH *src)
{
  for (uint i= 0 ; i < src->records ; i++)
  {
    Sroutine_hash_entry *rt= (Sroutine_hash_entry *)my_hash_element(src, i);
    if (!my_hash_search(dst, (uchar *)rt->mdl_request.key.ptr(),
                        rt->mdl_request.key.length()))
    {
      if (my_hash_insert(dst, (uchar *)rt))
        return true;
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////
// sp_name implementation.
///////////////////////////////////////////////////////////////////////////

/**
  Create temporary sp_name object from MDL key.

  @note The lifetime of this object is bound to the lifetime of the MDL_key.
        This should be fine as sp_name objects created by this constructor
        are mainly used for SP-cache lookups.

  @param key         MDL key containing database and routine name.
  @param qname_buff  Buffer to be used for storing quoted routine name
                     (should be at least 2*NAME_LEN+1+1 bytes).
*/

sp_name::sp_name(const MDL_key *key, char *qname_buff)
{
  m_db.str= (char*)key->db_name();
  m_db.length= key->db_name_length();
  m_name.str= (char*)key->name();
  m_name.length= key->name_length();
  m_qname.str= qname_buff;
  if (m_db.length)
  {
    strxmov(qname_buff, m_db.str, ".", m_name.str, NullS);
    m_qname.length= m_db.length + 1 + m_name.length;
  }
  else
  {
    strmov(qname_buff, m_name.str);
    m_qname.length= m_name.length;
  }
  m_explicit_name= false;
}


/**
  Init the qualified name from the db and name.
*/
void sp_name::init_qname(THD *thd)
{
  const uint dot= !!m_db.length;
  /* m_qname format: [database + dot] + name + '\0' */
  m_qname.length= m_db.length + dot + m_name.length;
  if (!(m_qname.str= (char*) thd->alloc(m_qname.length + 1)))
    return;
  sprintf(m_qname.str, "%.*s%.*s%.*s",
          (int) m_db.length, (m_db.length ? m_db.str : ""),
          dot, ".",
          (int) m_name.length, m_name.str);
}

///////////////////////////////////////////////////////////////////////////
// sp_head implementation.
///////////////////////////////////////////////////////////////////////////

void *sp_head::operator new(size_t size) throw()
{
  MEM_ROOT own_root;

  init_sql_alloc(&own_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);

  sp_head *sp= (sp_head *) alloc_root(&own_root, size);
  if (!sp)
    return NULL;

  sp->main_mem_root= own_root;
  DBUG_PRINT("info", ("mem_root 0x%lx", (ulong) &sp->mem_root));
  return sp;
}

void sp_head::operator delete(void *ptr, size_t size) throw()
{
  if (!ptr)
    return;

  sp_head *sp= (sp_head *) ptr;

  /* Make a copy of main_mem_root as free_root will free the sp */
  MEM_ROOT own_root= sp->main_mem_root;
  DBUG_PRINT("info", ("mem_root 0x%lx moved to 0x%lx",
                      (ulong) &sp->mem_root, (ulong) &own_root));
  free_root(&own_root, MYF(0));
}


sp_head::sp_head(enum_sp_type type)
 :Query_arena(&main_mem_root, STMT_INITIALIZED_FOR_SP),
  m_type(type),
  m_flags(0),
  m_chistics(NULL),
  m_sql_mode(0),
  m_explicit_name(false),
  m_created(0),
  m_modified(0),
  m_recursion_level(0),
  m_next_cached_sp(NULL),
  m_first_instance(NULL),
  m_first_free_instance(NULL),
  m_last_cached_sp(NULL),
  m_trg_list(NULL),
  m_root_parsing_ctx(NULL),
  m_sp_cache_version(0),
  m_creation_ctx(NULL),
  unsafe_flags(0)
{
  m_first_instance= this;
  m_first_free_instance= this;
  m_last_cached_sp= this;

  m_return_field_def.charset = NULL;

  /*
    FIXME: the only use case when name is NULL is events, and it should
    be rewritten soon. Remove the else part and replace 'if' with
    an assert when this is done.
  */

  m_db= NULL_STR;
  m_name= NULL_STR;
  m_qname= NULL_STR;

  m_params= NULL_STR;

  m_defstr= NULL_STR;
  m_body= NULL_STR;
  m_body_utf8= NULL_STR;

  my_hash_init(&m_sptabs, system_charset_info, 0, 0, 0, sp_table_key, 0, 0);
  my_hash_init(&m_sroutines, system_charset_info, 0, 0, 0, sp_sroutine_key,
               0, 0);
}


void sp_head::init_sp_name(THD *thd, sp_name *spname)
{
  /* Must be initialized in the parser. */

  DBUG_ASSERT(spname && spname->m_db.str && spname->m_db.length);

  /* We have to copy strings to get them into the right memroot. */

  m_db.length= spname->m_db.length;
  m_db.str= strmake_root(thd->mem_root, spname->m_db.str, spname->m_db.length);

  m_name.length= spname->m_name.length;
  m_name.str= strmake_root(thd->mem_root, spname->m_name.str,
                           spname->m_name.length);

  m_explicit_name= spname->m_explicit_name;

  if (spname->m_qname.length == 0)
    spname->init_qname(thd);

  m_qname.length= spname->m_qname.length;
  m_qname.str= (char*) memdup_root(thd->mem_root,
                                   spname->m_qname.str,
                                   spname->m_qname.length + 1);
}


void sp_head::set_body_start(THD *thd, const char *begin_ptr)
{
  m_parser_data.set_body_start_ptr(begin_ptr);

  thd->m_parser_state->m_lip.body_utf8_start(thd, begin_ptr);
}


void sp_head::set_body_end(THD *thd)
{
  Lex_input_stream *lip= & thd->m_parser_state->m_lip; /* shortcut */
  const char *end_ptr= lip->get_cpp_ptr(); /* shortcut */

  /* Make the string of parameters. */

  {
    const char *p_start= m_parser_data.get_parameter_start_ptr();
    const char *p_end= m_parser_data.get_parameter_end_ptr();

    if (p_start && p_end)
    {
      m_params.length= p_end - p_start;
      m_params.str= thd->strmake(p_start, m_params.length);
    }
  }

  /* Remember end pointer for further dumping of whole statement. */

  thd->lex->stmt_definition_end= end_ptr;

  /* Make the string of body (in the original character set). */

  m_body.length= end_ptr - m_parser_data.get_body_start_ptr();
  m_body.str= thd->strmake(m_parser_data.get_body_start_ptr(), m_body.length);
  trim_whitespace(thd->charset(), & m_body);

  /* Make the string of UTF-body. */

  lip->body_utf8_append(end_ptr);

  m_body_utf8.length= lip->get_body_utf8_length();
  m_body_utf8.str= thd->strmake(lip->get_body_utf8_str(), m_body_utf8.length);
  trim_whitespace(thd->charset(), & m_body_utf8);

  /*
    Make the string of whole stored-program-definition query (in the
    original character set).
  */

  m_defstr.length= end_ptr - lip->get_cpp_buf();
  m_defstr.str= thd->strmake(lip->get_cpp_buf(), m_defstr.length);
  trim_whitespace(thd->charset(), & m_defstr);
}


sp_head::~sp_head()
{
  LEX *lex;
  sp_instr *i;

  // Parsing of SP-body must have been already finished.
  DBUG_ASSERT(!m_parser_data.is_parsing_sp_body());

  for (uint ip = 0 ; (i = get_instr(ip)) ; ip++)
    delete i;

  delete m_root_parsing_ctx;

  free_items();

  /*
    If we have non-empty LEX stack then we just came out of parser with
    error. Now we should delete all auxiliary LEXes and restore original
    THD::lex. It is safe to not update LEX::ptr because further query
    string parsing and execution will be stopped anyway.
  */
  while ((lex= (LEX *) m_parser_data.pop_lex()))
  {
    THD *thd= lex->thd;
    thd->lex->sphead= NULL;
    lex_end(thd->lex);
    delete thd->lex;
    thd->lex= lex;
  }

  my_hash_free(&m_sptabs);
  my_hash_free(&m_sroutines);

  delete m_next_cached_sp;
}


Field *sp_head::create_result_field(uint field_max_length,
                                    const char *field_name,
                                    TABLE *table)
{
  uint field_length= !m_return_field_def.length ?
                     field_max_length : m_return_field_def.length;

  Field *field=
    ::make_field(table->s,                     /* TABLE_SHARE ptr */
                 (uchar*) 0,                   /* field ptr */
                 field_length,                 /* field [max] length */
                 (uchar*) "",                  /* null ptr */
                 0,                            /* null bit */
                 m_return_field_def.pack_flag,
                 m_return_field_def.sql_type,
                 m_return_field_def.charset,
                 m_return_field_def.geom_type,
                 Field::NONE,                  /* unreg check */
                 m_return_field_def.interval,
                 field_name ? field_name : (const char *) m_name.str);

  if (field)
    field->init(table);

  return field;
}


bool sp_head::execute(THD *thd, bool merge_da_on_success)
{
  char saved_cur_db_name_buf[NAME_LEN+1];
  LEX_STRING saved_cur_db_name=
    { saved_cur_db_name_buf, sizeof(saved_cur_db_name_buf) };
  bool cur_db_changed= FALSE;
  bool err_status= FALSE;
  uint ip= 0;
  sql_mode_t save_sql_mode;
  bool save_abort_on_warning;
  Query_arena *old_arena;
  /* per-instruction arena */
  MEM_ROOT execute_mem_root;
  Query_arena execute_arena(&execute_mem_root, STMT_INITIALIZED_FOR_SP),
              backup_arena;
  query_id_t old_query_id;
  TABLE *old_derived_tables;
  LEX *old_lex;
  Item_change_list old_change_list;
  String old_packet;
  Object_creation_ctx *saved_creation_ctx;
  Diagnostics_area *da= thd->get_stmt_da();
  Warning_info sp_wi(da->warning_info_id(), false);

  /*
    Just reporting a stack overrun error
    (@sa check_stack_overrun()) requires stack memory for error
    message buffer. Thus, we have to put the below check
    relatively close to the beginning of the execution stack,
    where available stack margin is still big. As long as the check
    has to be fairly high up the call stack, the amount of memory
    we "book" for has to stay fairly high as well, and hence
    not very accurate. The number below has been calculated
    by trial and error, and reflects the amount of memory necessary
    to execute a single stored procedure instruction, be it either
    an SQL statement, or, heaviest of all, a CALL, which involves
    parsing and loading of another stored procedure into the cache
    (@sa db_load_routine() and Bug#10100).

    TODO: that should be replaced by proper handling of stack overrun error.

    Stack size depends on the platform:
      - for most platforms (8 * STACK_MIN_SIZE) is enough;
      - for Solaris SPARC 64 (10 * STACK_MIN_SIZE) is required.
  */

  {
#if defined(__sparcv9) && defined(__sun)
    const int sp_stack_size= 10 * STACK_MIN_SIZE;
#else
    const int sp_stack_size=  8 * STACK_MIN_SIZE;
#endif

    if (check_stack_overrun(thd, sp_stack_size, (uchar*) &old_packet))
      return true;
  }

  opt_trace_disable_if_no_security_context_access(thd);

  /* init per-instruction memroot */
  init_sql_alloc(&execute_mem_root, MEM_ROOT_BLOCK_SIZE, 0);

  DBUG_ASSERT(!(m_flags & IS_INVOKED));
  m_flags|= IS_INVOKED;
  m_first_instance->m_first_free_instance= m_next_cached_sp;
  if (m_next_cached_sp)
  {
    DBUG_PRINT("info",
               ("first free for 0x%lx ++: 0x%lx->0x%lx  level: %lu  flags %x",
                (ulong)m_first_instance, (ulong) this,
                (ulong) m_next_cached_sp,
                m_next_cached_sp->m_recursion_level,
                m_next_cached_sp->m_flags));
  }
  /*
    Check that if there are not any instances after this one then
    pointer to the last instance points on this instance or if there are
    some instances after this one then recursion level of next instance
    greater then recursion level of current instance on 1
  */
  DBUG_ASSERT((m_next_cached_sp == 0 &&
               m_first_instance->m_last_cached_sp == this) ||
              (m_recursion_level + 1 == m_next_cached_sp->m_recursion_level));

  /*
    NOTE: The SQL Standard does not specify the context that should be
    preserved for stored routines. However, at SAP/Walldorf meeting it was
    decided that current database should be preserved.
  */

  if (m_db.length &&
      (err_status= mysql_opt_change_db(thd, &m_db, &saved_cur_db_name, FALSE,
                                       &cur_db_changed)))
  {
    goto done;
  }

  thd->is_slave_error= 0;
  old_arena= thd->stmt_arena;

  /* Push a new warning information area. */
  da->copy_sql_conditions_to_wi(thd, &sp_wi);
  da->push_warning_info(&sp_wi);

  /*
    Switch query context. This has to be done early as this is sometimes
    allocated trough sql_alloc
  */
  saved_creation_ctx= m_creation_ctx->set_n_backup(thd);

  /*
    We have to save/restore this info when we are changing call level to
    be able properly do close_thread_tables() in instructions.
  */
  old_query_id= thd->query_id;
  old_derived_tables= thd->derived_tables;
  thd->derived_tables= 0;
  save_sql_mode= thd->variables.sql_mode;
  thd->variables.sql_mode= m_sql_mode;
  save_abort_on_warning= thd->abort_on_warning;
  thd->abort_on_warning= 0;
  /**
    When inside a substatement (a stored function or trigger
    statement), clear the metadata observer in THD, if any.
    Remember the value of the observer here, to be able
    to restore it when leaving the substatement.

    We reset the observer to suppress errors when a substatement
    uses temporary tables. If a temporary table does not exist
    at start of the main statement, it's not prelocked
    and thus is not validated with other prelocked tables.

    Later on, when the temporary table is opened, metadata
    versions mismatch, expectedly.

    The proper solution for the problem is to re-validate tables
    of substatements (Bug#12257, Bug#27011, Bug#32868, Bug#33000),
    but it's not implemented yet.
  */
  thd->push_reprepare_observer(NULL);

  /*
    It is also more efficient to save/restore current thd->lex once when
    do it in each instruction
  */
  old_lex= thd->lex;
  /*
    We should also save Item tree change list to avoid rollback something
    too early in the calling query.
  */
  thd->change_list.move_elements_to(&old_change_list);
  /*
    Cursors will use thd->packet, so they may corrupt data which was prepared
    for sending by upper level. OTOH cursors in the same routine can share this
    buffer safely so let use use routine-local packet instead of having own
    packet buffer for each cursor.

    It is probably safe to use same thd->convert_buff everywhere.
  */
  old_packet.swap(thd->packet);

  /*
    Switch to per-instruction arena here. We can do it since we cleanup
    arena after every instruction.
  */
  thd->set_n_backup_active_arena(&execute_arena, &backup_arena);

  /*
    Save callers arena in order to store instruction results and out
    parameters in it later during sp_eval_func_item()
  */
  thd->sp_runtime_ctx->callers_arena= &backup_arena;

#if defined(ENABLED_PROFILING)
  /* Discard the initial part of executing routines. */
  thd->profiling.discard_current_query();
#endif
  do
  {
    sp_instr *i;

#if defined(ENABLED_PROFILING)
    /*
     Treat each "instr" of a routine as discrete unit that could be profiled.
     Profiling only records information for segments of code that set the
     source of the query, and almost all kinds of instructions in s-p do not.
    */
    thd->profiling.finish_current_query();
    thd->profiling.start_new_query("continuing inside routine");
#endif

    /* get_instr returns NULL when we're done. */
    i = get_instr(ip);
    if (i == NULL)
    {
#if defined(ENABLED_PROFILING)
      thd->profiling.discard_current_query();
#endif
      break;
    }

    /* Reset number of warnings for this query. */
    thd->get_stmt_da()->reset_for_next_command();

    DBUG_PRINT("execute", ("Instruction %u", ip));

    /*
      We need to reset start_time to allow for time to flow inside a stored
      procedure. This is only done for SP since time is suppose to be constant
      during execution of triggers and functions.
    */
    reset_start_time_for_sp(thd);

    /*
      We have to set thd->stmt_arena before executing the instruction
      to store in the instruction free_list all new items, created
      during the first execution (for example expanding of '*' or the
      items made during other permanent subquery transformations).
    */
    thd->stmt_arena= i;

    /*
      Will write this SP statement into binlog separately.
      TODO: consider changing the condition to "not inside event union".
    */
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
      thd->user_var_events_alloc= thd->mem_root;

    err_status= i->execute(thd, &ip);

    if (i->free_list)
      cleanup_items(i->free_list);

    /*
      If we've set thd->user_var_events_alloc to mem_root of this SP
      statement, clean all the events allocated in it.
    */
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
    {
      reset_dynamic(&thd->user_var_events);
      thd->user_var_events_alloc= NULL;//DEBUG
    }

    /* we should cleanup free_list and memroot, used by instruction */
    thd->cleanup_after_query();
    free_root(&execute_mem_root, MYF(0));

    /*
      Find and process SQL handlers unless it is a fatal error (fatal
      errors are not catchable by SQL handlers) or the connection has been
      killed during execution.
    */
    if (!thd->is_fatal_error && !thd->killed_errno() &&
        thd->sp_runtime_ctx->handle_sql_condition(thd, &ip, i))
    {
      err_status= FALSE;
    }

    /* Reset sp_rcontext::end_partial_result_set flag. */
    thd->sp_runtime_ctx->end_partial_result_set= FALSE;

  } while (!err_status && !thd->killed && !thd->is_fatal_error);

#if defined(ENABLED_PROFILING)
  thd->profiling.finish_current_query();
  thd->profiling.start_new_query("tail end of routine");
#endif

  /* Restore query context. */

  m_creation_ctx->restore_env(thd, saved_creation_ctx);

  /* Restore arena. */

  thd->restore_active_arena(&execute_arena, &backup_arena);

  thd->sp_runtime_ctx->pop_all_cursors(); // To avoid memory leaks after an error

  /* Restore all saved */
  old_packet.swap(thd->packet);
  DBUG_ASSERT(thd->change_list.is_empty());
  old_change_list.move_elements_to(&thd->change_list);
  thd->lex= old_lex;
  thd->set_query_id(old_query_id);
  DBUG_ASSERT(!thd->derived_tables);
  thd->derived_tables= old_derived_tables;
  thd->variables.sql_mode= save_sql_mode;
  thd->abort_on_warning= save_abort_on_warning;
  thd->pop_reprepare_observer();

  thd->stmt_arena= old_arena;
  state= STMT_EXECUTED;

  /*
    Restore the caller's original warning information area:
      - warnings generated during trigger execution should not be
        propagated to the caller on success;
      - if there was an exception during execution, warning info should be
        propagated to the caller in any case.
  */
  da->pop_warning_info();

  if (err_status || merge_da_on_success)
  {
    /*
      If a routine body is empty or if a routine did not generate any warnings,
      do not duplicate our own contents by appending the contents of the called
      routine. We know that the called routine did not change its warning info.

      On the other hand, if the routine body is not empty and some statement in
      the routine generates a warning or uses tables, warning info is guaranteed
      to have changed. In this case we know that the routine warning info
      contains only new warnings, and thus we perform a copy.
    */
    if (da->warning_info_changed(&sp_wi))
    {
      /*
        If the invocation of the routine was a standalone statement,
        rather than a sub-statement, in other words, if it's a CALL
        of a procedure, rather than invocation of a function or a
        trigger, we need to clear the current contents of the caller's
        warning info.

        This is per MySQL rules: if a statement generates a warning,
        warnings from the previous statement are flushed.  Normally
        it's done in push_warning(). However, here we don't use
        push_warning() to avoid invocation of condition handlers or
        escalation of warnings to errors.
      */
      da->opt_clear_warning_info(thd->query_id);
      da->copy_sql_conditions_from_wi(thd, &sp_wi);
      da->remove_marked_sql_conditions();
    }
  }

 done:
  DBUG_PRINT("info", ("err_status: %d  killed: %d  is_slave_error: %d  report_error: %d",
                      err_status, thd->killed, thd->is_slave_error,
                      thd->is_error()));

  if (thd->killed)
    err_status= TRUE;
  /*
    If the DB has changed, the pointer has changed too, but the
    original thd->db will then have been freed
  */
  if (cur_db_changed && thd->killed != THD::KILL_CONNECTION)
  {
    /*
      Force switching back to the saved current database, because it may be
      NULL. In this case, mysql_change_db() would generate an error.
    */

    err_status|= mysql_change_db(thd, &saved_cur_db_name, TRUE);
  }
  m_flags&= ~IS_INVOKED;
  DBUG_PRINT("info",
             ("first free for 0x%lx --: 0x%lx->0x%lx, level: %lu, flags %x",
              (ulong) m_first_instance,
              (ulong) m_first_instance->m_first_free_instance,
              (ulong) this, m_recursion_level, m_flags));
  /*
    Check that we have one of following:

    1) there are not free instances which means that this instance is last
    in the list of instances (pointer to the last instance point on it and
    there are not other instances after this one in the list)

    2) There are some free instances which mean that first free instance
    should go just after this one and recursion level of that free instance
    should be on 1 more then recursion level of this instance.
  */
  DBUG_ASSERT((m_first_instance->m_first_free_instance == 0 &&
               this == m_first_instance->m_last_cached_sp &&
               m_next_cached_sp == 0) ||
              (m_first_instance->m_first_free_instance != 0 &&
               m_first_instance->m_first_free_instance == m_next_cached_sp &&
               m_first_instance->m_first_free_instance->m_recursion_level ==
               m_recursion_level + 1));
  m_first_instance->m_first_free_instance= this;

  return err_status;
}


bool sp_head::execute_trigger(THD *thd,
                              const LEX_STRING *db_name,
                              const LEX_STRING *table_name,
                              GRANT_INFO *grant_info)
{
  sp_rcontext *parent_sp_runtime_ctx = thd->sp_runtime_ctx;
  bool err_status= FALSE;
  MEM_ROOT call_mem_root;
  Query_arena call_arena(&call_mem_root, Query_arena::STMT_INITIALIZED_FOR_SP);
  Query_arena backup_arena;

  DBUG_ENTER("sp_head::execute_trigger");
  DBUG_PRINT("info", ("trigger %s", m_name.str));

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_ctx= NULL;


  if (m_chistics->suid != SP_IS_NOT_SUID &&
      m_security_ctx.change_security_context(thd,
                                             &m_definer_user,
                                             &m_definer_host,
                                             &m_db,
                                             &save_ctx))
    DBUG_RETURN(true);

  /*
    Fetch information about table-level privileges for subject table into
    GRANT_INFO instance. The access check itself will happen in
    Item_trigger_field, where this information will be used along with
    information about column-level privileges.
  */

  fill_effective_table_privileges(thd,
                                  grant_info,
                                  db_name->str,
                                  table_name->str);

  /* Check that the definer has TRIGGER privilege on the subject table. */

  if (!(grant_info->privilege & TRIGGER_ACL))
  {
    char priv_desc[128];
    get_privilege_desc(priv_desc, sizeof(priv_desc), TRIGGER_ACL);

    my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0), priv_desc,
             thd->security_ctx->priv_user, thd->security_ctx->host_or_ip,
             table_name->str);

    m_security_ctx.restore_security_context(thd, save_ctx);
    DBUG_RETURN(true);
  }
  /*
    Optimizer trace note: we needn't explicitly test here that the connected
    user has TRIGGER privilege: assume he doesn't have it; two possibilities:
    - connected user == definer: then we threw an error just above;
    - connected user != definer: then in sp_head::execute(), when checking the
    security context we will disable tracing.
  */
#endif // NO_EMBEDDED_ACCESS_CHECKS

  /*
    Prepare arena and memroot for objects which lifetime is whole
    duration of trigger call (sp_rcontext, it's tables and items,
    sp_cursor and Item_cache holders for case expressions).  We can't
    use caller's arena/memroot for those objects because in this case
    some fixed amount of memory will be consumed for each trigger
    invocation and so statements which involve lot of them will hog
    memory.

    TODO: we should create sp_rcontext once per command and reuse it
    on subsequent executions of a trigger.
  */
  init_sql_alloc(&call_mem_root, MEM_ROOT_BLOCK_SIZE, 0);
  thd->set_n_backup_active_arena(&call_arena, &backup_arena);

  sp_rcontext *trigger_runtime_ctx=
    sp_rcontext::create(thd, m_root_parsing_ctx, NULL);

  if (!trigger_runtime_ctx)
  {
    err_status= TRUE;
    goto err_with_cleanup;
  }

  trigger_runtime_ctx->sp= this;
  thd->sp_runtime_ctx= trigger_runtime_ctx;

  err_status= execute(thd, FALSE);

err_with_cleanup:
  thd->restore_active_arena(&call_arena, &backup_arena);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  m_security_ctx.restore_security_context(thd, save_ctx);
#endif // NO_EMBEDDED_ACCESS_CHECKS

  delete trigger_runtime_ctx;
  call_arena.free_items();
  free_root(&call_mem_root, MYF(0));
  thd->sp_runtime_ctx= parent_sp_runtime_ctx;

  if (thd->killed)
    thd->send_kill_message();

  DBUG_RETURN(err_status);
}


bool sp_head::execute_function(THD *thd, Item **argp, uint argcount,
                               Field *return_value_fld)
{
  ulonglong binlog_save_options;
  bool need_binlog_call= FALSE;
  uint arg_no;
  sp_rcontext *parent_sp_runtime_ctx = thd->sp_runtime_ctx;
  char buf[STRING_BUFFER_USUAL_SIZE];
  String binlog_buf(buf, sizeof(buf), &my_charset_bin);
  bool err_status= FALSE;
  MEM_ROOT call_mem_root;
  Query_arena call_arena(&call_mem_root, Query_arena::STMT_INITIALIZED_FOR_SP);
  Query_arena backup_arena;

  DBUG_ENTER("sp_head::execute_function");
  DBUG_PRINT("info", ("function %s", m_name.str));

  LINT_INIT(binlog_save_options);
  // Resetting THD::where to its default value
  thd->where= THD::DEFAULT_WHERE;
  /*
    Check that the function is called with all specified arguments.

    If it is not, use my_error() to report an error, or it will not terminate
    the invoking query properly.
  */
  if (argcount != m_root_parsing_ctx->context_var_count())
  {
    /*
      Need to use my_error here, or it will not terminate the
      invoking query properly.
    */
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0),
             "FUNCTION", m_qname.str,
             m_root_parsing_ctx->context_var_count(), argcount);
    DBUG_RETURN(true);
  }
  /*
    Prepare arena and memroot for objects which lifetime is whole
    duration of function call (sp_rcontext, it's tables and items,
    sp_cursor and Item_cache holders for case expressions).
    We can't use caller's arena/memroot for those objects because
    in this case some fixed amount of memory will be consumed for
    each function/trigger invocation and so statements which involve
    lot of them will hog memory.
    TODO: we should create sp_rcontext once per command and reuse
    it on subsequent executions of a function/trigger.
  */
  init_sql_alloc(&call_mem_root, MEM_ROOT_BLOCK_SIZE, 0);
  thd->set_n_backup_active_arena(&call_arena, &backup_arena);

  sp_rcontext *func_runtime_ctx= sp_rcontext::create(thd, m_root_parsing_ctx,
                                                     return_value_fld);

  if (!func_runtime_ctx)
  {
    thd->restore_active_arena(&call_arena, &backup_arena);
    err_status= TRUE;
    goto err_with_cleanup;
  }

  func_runtime_ctx->sp= this;

  /*
    We have to switch temporarily back to callers arena/memroot.
    Function arguments belong to the caller and so the may reference
    memory which they will allocate during calculation long after
    this function call will be finished (e.g. in Item::cleanup()).
  */
  thd->restore_active_arena(&call_arena, &backup_arena);

  /*
    Pass arguments.

    Note, THD::sp_runtime_ctx must not be switched before the arguments are
    passed. Values are taken from the caller's runtime context and set to the
    runtime context of this function.
  */
  for (arg_no= 0; arg_no < argcount; arg_no++)
  {
    /* Arguments must be fixed in Item_func_sp::fix_fields */
    DBUG_ASSERT(argp[arg_no]->fixed);

    err_status= func_runtime_ctx->set_variable(thd, arg_no, &(argp[arg_no]));

    if (err_status)
      goto err_with_cleanup;
  }

  /*
    If row-based binlogging, we don't need to binlog the function's call, let
    each substatement be binlogged its way.
  */
  need_binlog_call= mysql_bin_log.is_open() &&
                    (thd->variables.option_bits & OPTION_BIN_LOG) &&
                    !thd->is_current_stmt_binlog_format_row();

  /*
    Remember the original arguments for unrolled replication of functions
    before they are changed by execution.

    Note, THD::sp_runtime_ctx must not be switched before the arguments are
    logged. Values are taken from the caller's runtime context.
  */
  if (need_binlog_call)
  {
    binlog_buf.length(0);
    binlog_buf.append(STRING_WITH_LEN("SELECT "));
    append_identifier(thd, &binlog_buf, m_db.str, m_db.length);
    binlog_buf.append('.');
    append_identifier(thd, &binlog_buf, m_name.str, m_name.length);
    binlog_buf.append('(');
    for (arg_no= 0; arg_no < argcount; arg_no++)
    {
      String str_value_holder;
      String *str_value;

      if (arg_no)
        binlog_buf.append(',');

      str_value= sp_get_item_value(thd, func_runtime_ctx->get_item(arg_no),
                                   &str_value_holder);

      if (str_value)
        binlog_buf.append(*str_value);
      else
        binlog_buf.append(STRING_WITH_LEN("NULL"));
    }
    binlog_buf.append(')');
  }

  thd->sp_runtime_ctx= func_runtime_ctx;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_security_ctx;
  if (set_security_ctx(thd, &save_security_ctx))
  {
    err_status= TRUE;
    goto err_with_cleanup;
  }
#endif

  if (need_binlog_call)
  {
    query_id_t q;
    reset_dynamic(&thd->user_var_events);
    /*
      In case of artificially constructed events for function calls
      we have separate union for each such event and hence can't use
      query_id of real calling statement as the start of all these
      unions (this will break logic of replication of user-defined
      variables). So we use artificial value which is guaranteed to
      be greater than all query_id's of all statements belonging
      to previous events/unions.
      Possible alternative to this is logging of all function invocations
      as one select and not resetting THD::user_var_events before
      each invocation.
    */
    mysql_mutex_lock(&LOCK_thread_count);
    q= global_query_id;
    mysql_mutex_unlock(&LOCK_thread_count);
    mysql_bin_log.start_union_events(thd, q + 1);
    binlog_save_options= thd->variables.option_bits;
    thd->variables.option_bits&= ~OPTION_BIN_LOG;
  }

  opt_trace_disable_if_no_stored_proc_func_access(thd, this);

  /*
    Switch to call arena/mem_root so objects like sp_cursor or
    Item_cache holders for case expressions can be allocated on it.

    TODO: In future we should associate call arena/mem_root with
          sp_rcontext and allocate all these objects (and sp_rcontext
          itself) on it directly rather than juggle with arenas.
  */
  thd->set_n_backup_active_arena(&call_arena, &backup_arena);

  err_status= execute(thd, TRUE);

  thd->restore_active_arena(&call_arena, &backup_arena);

  if (need_binlog_call)
  {
    mysql_bin_log.stop_union_events(thd);
    thd->variables.option_bits= binlog_save_options;
    if (thd->binlog_evt_union.unioned_events)
    {
      int errcode = query_error_code(thd, thd->killed == THD::NOT_KILLED);
      Query_log_event qinfo(thd, binlog_buf.ptr(), binlog_buf.length(),
                            thd->binlog_evt_union.unioned_events_trans, FALSE, FALSE, errcode);
      if (mysql_bin_log.write_event(&qinfo) &&
          thd->binlog_evt_union.unioned_events_trans)
      {
        push_warning(thd, Sql_condition::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                     "Invoked ROUTINE modified a transactional table but MySQL "
                     "failed to reflect this change in the binary log");
        err_status= TRUE;
      }
      reset_dynamic(&thd->user_var_events);
      /* Forget those values, in case more function calls are binlogged: */
      thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;
      thd->auto_inc_intervals_in_cur_stmt_for_binlog.empty();
    }
  }

  if (!err_status)
  {
    /* We need result only in function but not in trigger */

    if (!thd->sp_runtime_ctx->is_return_value_set())
    {
      my_error(ER_SP_NORETURNEND, MYF(0), m_name.str);
      err_status= TRUE;
    }
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  m_security_ctx.restore_security_context(thd, save_security_ctx);
#endif

err_with_cleanup:
  delete func_runtime_ctx;
  call_arena.free_items();
  free_root(&call_mem_root, MYF(0));
  thd->sp_runtime_ctx= parent_sp_runtime_ctx;

  /*
    If not inside a procedure and a function printing warning
    messages.
  */
  if (need_binlog_call && 
      thd->sp_runtime_ctx == NULL && !thd->binlog_evt_union.do_union)
    thd->issue_unsafe_warnings();

  DBUG_RETURN(err_status);
}


bool sp_head::execute_procedure(THD *thd, List<Item> *args)
{
  bool err_status= FALSE;
  uint params = m_root_parsing_ctx->context_var_count();
  /* Query start time may be reset in a multi-stmt SP; keep this for later. */
  ulonglong utime_before_sp_exec= thd->utime_after_lock;
  sp_rcontext *parent_sp_runtime_ctx= thd->sp_runtime_ctx;
  sp_rcontext *sp_runtime_ctx_saved= thd->sp_runtime_ctx;
  bool save_enable_slow_log= false;
  bool save_log_general= false;

  DBUG_ENTER("sp_head::execute_procedure");
  DBUG_PRINT("info", ("procedure %s", m_name.str));

  if (args->elements != params)
  {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "PROCEDURE",
             m_qname.str, params, args->elements);
    DBUG_RETURN(true);
  }

  if (!parent_sp_runtime_ctx)
  {
    // Create a temporary old context. We need it to pass OUT-parameter values.
    parent_sp_runtime_ctx= sp_rcontext::create(thd, m_root_parsing_ctx, NULL);

    if (!parent_sp_runtime_ctx)
      DBUG_RETURN(true);

    parent_sp_runtime_ctx->sp= 0;
    thd->sp_runtime_ctx= parent_sp_runtime_ctx;

    /* set callers_arena to thd, for upper-level function to work */
    thd->sp_runtime_ctx->callers_arena= thd;
  }

  sp_rcontext *proc_runtime_ctx=
    sp_rcontext::create(thd, m_root_parsing_ctx, NULL);

  if (!proc_runtime_ctx)
  {
    thd->sp_runtime_ctx= sp_runtime_ctx_saved;

    if (!sp_runtime_ctx_saved)
      delete parent_sp_runtime_ctx;

    DBUG_RETURN(true);
  }

  proc_runtime_ctx->sp= this;

  if (params > 0)
  {
    List_iterator<Item> it_args(*args);

    DBUG_PRINT("info",(" %.*s: eval args", (int) m_name.length, m_name.str));

    for (uint i= 0 ; i < params ; i++)
    {
      Item *arg_item= it_args++;

      if (!arg_item)
        break;

      sp_variable *spvar= m_root_parsing_ctx->find_variable(i);

      if (!spvar)
        continue;

      if (spvar->mode != sp_variable::MODE_IN)
      {
        Settable_routine_parameter *srp=
          arg_item->get_settable_routine_parameter();

        if (!srp)
        {
          my_error(ER_SP_NOT_VAR_ARG, MYF(0), i+1, m_qname.str);
          err_status= TRUE;
          break;
        }

        srp->set_required_privilege(spvar->mode == sp_variable::MODE_INOUT);
      }

      if (spvar->mode == sp_variable::MODE_OUT)
      {
        Item_null *null_item= new Item_null();

        if (!null_item ||
            proc_runtime_ctx->set_variable(thd, i, (Item **)&null_item))
        {
          err_status= TRUE;
          break;
        }
      }
      else
      {
        if (proc_runtime_ctx->set_variable(thd, i, it_args.ref()))
        {
          err_status= TRUE;
          break;
        }
      }
    }

    /*
      Okay, got values for all arguments. Close tables that might be used by
      arguments evaluation. If arguments evaluation required prelocking mode,
      we'll leave it here.
    */
    thd->lex->unit.cleanup();

    if (!thd->in_sub_stmt)
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

    thd->rollback_item_tree_changes();

    DBUG_PRINT("info",(" %.*s: eval args done", (int) m_name.length, 
                       m_name.str));
  }
  if (!(m_flags & LOG_SLOW_STATEMENTS) && thd->enable_slow_log)
  {
    DBUG_PRINT("info", ("Disabling slow log for the execution"));
    save_enable_slow_log= true;
    thd->enable_slow_log= FALSE;
  }
  if (!(m_flags & LOG_GENERAL_LOG) && !(thd->variables.option_bits & OPTION_LOG_OFF))
  {
    DBUG_PRINT("info", ("Disabling general log for the execution"));
    save_log_general= true;
    /* disable this bit */
    thd->variables.option_bits |= OPTION_LOG_OFF;
  }
  thd->sp_runtime_ctx= proc_runtime_ctx;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_security_ctx= 0;
  if (!err_status)
    err_status= set_security_ctx(thd, &save_security_ctx);
#endif

  opt_trace_disable_if_no_stored_proc_func_access(thd, this);

  if (!err_status)
    err_status= execute(thd, TRUE);

  if (save_log_general)
    thd->variables.option_bits &= ~OPTION_LOG_OFF;
  if (save_enable_slow_log)
    thd->enable_slow_log= true;
  /*
    In the case when we weren't able to employ reuse mechanism for
    OUT/INOUT parameters, we should reallocate memory. This
    allocation should be done on the arena which will live through
    all execution of calling routine.
  */
  thd->sp_runtime_ctx->callers_arena= parent_sp_runtime_ctx->callers_arena;

  if (!err_status && params > 0)
  {
    List_iterator<Item> it_args(*args);

    /*
      Copy back all OUT or INOUT values to the previous frame, or
      set global user variables
    */
    for (uint i= 0 ; i < params ; i++)
    {
      Item *arg_item= it_args++;

      if (!arg_item)
        break;

      sp_variable *spvar= m_root_parsing_ctx->find_variable(i);

      if (spvar->mode == sp_variable::MODE_IN)
        continue;

      Settable_routine_parameter *srp=
        arg_item->get_settable_routine_parameter();

      DBUG_ASSERT(srp);

      if (srp->set_value(thd, parent_sp_runtime_ctx, proc_runtime_ctx->get_item_addr(i)))
      {
        err_status= TRUE;
        break;
      }

      Send_field *out_param_info= new (thd->mem_root) Send_field();
      proc_runtime_ctx->get_item(i)->make_field(out_param_info);
      out_param_info->db_name= m_db.str;
      out_param_info->table_name= m_name.str;
      out_param_info->org_table_name= m_name.str;
      out_param_info->col_name= spvar->name.str;
      out_param_info->org_col_name= spvar->name.str;

      srp->set_out_param_info(out_param_info);
    }
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (save_security_ctx)
    m_security_ctx.restore_security_context(thd, save_security_ctx);
#endif

  if (!sp_runtime_ctx_saved)
    delete parent_sp_runtime_ctx;

  delete proc_runtime_ctx;
  thd->sp_runtime_ctx= sp_runtime_ctx_saved;
  thd->utime_after_lock= utime_before_sp_exec;

  /*
    If not insided a procedure and a function printing warning
    messages.
  */ 
  bool need_binlog_call= mysql_bin_log.is_open() &&
                         (thd->variables.option_bits & OPTION_BIN_LOG) &&
                         !thd->is_current_stmt_binlog_format_row();
  if (need_binlog_call && thd->sp_runtime_ctx == NULL &&
      !thd->binlog_evt_union.do_union)
    thd->issue_unsafe_warnings();

  DBUG_RETURN(err_status);
}


bool sp_head::reset_lex(THD *thd)
{
  LEX *oldlex= thd->lex;

  LEX *sublex= new (thd->mem_root)st_lex_local;

  if (!sublex)
    return true;

  thd->lex= sublex;
  m_parser_data.push_lex(oldlex);

  /* Reset most stuff. */
  lex_start(thd);

  /* And keep the SP stuff too */
  sublex->sphead= oldlex->sphead;
  sublex->set_sp_current_parsing_ctx(oldlex->get_sp_current_parsing_ctx());
  sublex->sp_lex_in_use= FALSE;

  /* Reset type info. */

  sublex->charset= NULL;
  sublex->length= NULL;
  sublex->dec= NULL;
  sublex->interval_list.empty();
  sublex->type= 0;

  /* Reset part of parser state which needs this. */
  thd->m_parser_state->m_yacc.reset_before_substatement();

  return false;
}


bool sp_head::restore_lex(THD *thd)
{
  LEX *sublex= thd->lex;

  sublex->set_trg_event_type_for_tables();

  LEX *oldlex= (LEX *) m_parser_data.pop_lex();

  if (!oldlex)
    return false; // Nothing to restore

  /* If this substatement is unsafe, the entire routine is too. */
  DBUG_PRINT("info", ("lex->get_stmt_unsafe_flags: 0x%x",
                      thd->lex->get_stmt_unsafe_flags()));
  unsafe_flags|= sublex->get_stmt_unsafe_flags();

  /*
    Add routines which are used by statement to respective set for
    this routine.
  */
  if (sp_update_sp_used_routines(&m_sroutines, &sublex->sroutines))
    return true;

  /* If this substatement is a update query, then mark MODIFIES_DATA */
  if (is_update_query(sublex->sql_command))
    m_flags|= MODIFIES_DATA;

  /*
    Merge tables used by this statement (but not by its functions or
    procedures) to multiset of tables used by this routine.
  */
  merge_table_list(thd, sublex->query_tables, sublex);

  if (!sublex->sp_lex_in_use)
  {
    sublex->sphead= NULL;
    lex_end(sublex);
    delete sublex;
  }

  thd->lex= oldlex;
  return false;
}

void sp_head::set_info(longlong created,
                       longlong modified,
                       st_sp_chistics *chistics,
                       sql_mode_t sql_mode)
{
  m_created= created;
  m_modified= modified;
  m_chistics= (st_sp_chistics *) memdup_root(mem_root, (char*) chistics,
                                             sizeof(*chistics));
  if (m_chistics->comment.length == 0)
    m_chistics->comment.str= 0;
  else
    m_chistics->comment.str= strmake_root(mem_root,
                                          m_chistics->comment.str,
                                          m_chistics->comment.length);
  m_sql_mode= sql_mode;
}


void sp_head::set_definer(const char *definer, uint definerlen)
{
  char user_name_holder[USERNAME_LENGTH + 1];
  LEX_STRING user_name= { user_name_holder, USERNAME_LENGTH };

  char host_name_holder[HOSTNAME_LENGTH + 1];
  LEX_STRING host_name= { host_name_holder, HOSTNAME_LENGTH };

  parse_user(definer, definerlen, user_name.str, &user_name.length,
             host_name.str, &host_name.length);

  set_definer(&user_name, &host_name);
}


void sp_head::set_definer(const LEX_STRING *user_name,
                          const LEX_STRING *host_name)
{
  m_definer_user.str= strmake_root(mem_root, user_name->str, user_name->length);
  m_definer_user.length= user_name->length;

  m_definer_host.str= strmake_root(mem_root, host_name->str, host_name->length);
  m_definer_host.length= host_name->length;
}


bool sp_head::show_create_routine(THD *thd, enum_sp_type type)
{
  const char *col1_caption= (type == SP_TYPE_PROCEDURE) ?
                            "Procedure" : "Function";

  const char *col3_caption= (type == SP_TYPE_PROCEDURE) ?
                            "Create Procedure" : "Create Function";

  bool err_status;

  Protocol *protocol= thd->protocol;
  List<Item> fields;

  LEX_STRING sql_mode;

  bool full_access;

  DBUG_ASSERT(type == SP_TYPE_PROCEDURE || type == SP_TYPE_FUNCTION);

  if (check_show_access(thd, &full_access))
    return true;

  sql_mode_string_representation(thd, m_sql_mode, &sql_mode);

  /* Send header. */

  fields.push_back(new Item_empty_string(col1_caption, NAME_CHAR_LEN));
  fields.push_back(new Item_empty_string("sql_mode", sql_mode.length));

  {
    /*
      NOTE: SQL statement field must be not less than 1024 in order not to
      confuse old clients.
    */

    Item_empty_string *stmt_fld=
      new Item_empty_string(col3_caption,
                            std::max<size_t>(m_defstr.length, 1024U));

    stmt_fld->maybe_null= TRUE;

    fields.push_back(stmt_fld);
  }

  fields.push_back(new Item_empty_string("character_set_client",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_empty_string("collation_connection",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_empty_string("Database Collation",
                                         MY_CS_NAME_SIZE));

  if (protocol->send_result_set_metadata(&fields,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    return true;
  }

  /* Send data. */

  protocol->prepare_for_resend();

  protocol->store(m_name.str, m_name.length, system_charset_info);
  protocol->store(sql_mode.str, sql_mode.length, system_charset_info);

  if (full_access)
    protocol->store(m_defstr.str, m_defstr.length,
                    m_creation_ctx->get_client_cs());
  else
    protocol->store_null();


  protocol->store(m_creation_ctx->get_client_cs()->csname, system_charset_info);
  protocol->store(m_creation_ctx->get_connection_cl()->name, system_charset_info);
  protocol->store(m_creation_ctx->get_db_cl()->name, system_charset_info);

  err_status= protocol->write();

  if (!err_status)
    my_eof(thd);

  return err_status;
}


bool sp_head::add_instr(THD *thd, sp_instr *instr)
{
  m_parser_data.process_new_sp_instr(thd, instr);

  /*
    Memory root of every instruction is designated for permanent
    transformations (optimizations) made on the parsed tree during
    the first execution. It points to the memory root of the
    entire stored procedure, as their life span is equal.
  */
  instr->mem_root= get_persistent_mem_root();

  return m_instructions.append(instr);
}


void sp_head::optimize()
{
  List<sp_branch_instr> bp;
  sp_instr *i;
  uint src, dst;

  opt_mark();

  bp.empty();
  src= dst= 0;
  while ((i= get_instr(src)))
  {
    if (!i->opt_is_marked())
    {
      delete i;
      src+= 1;
    }
    else
    {
      if (src != dst)
      {
        m_instructions.set(dst, i);

        /* Move the instruction and update prev. jumps */
        sp_branch_instr *ibp;
        List_iterator_fast<sp_branch_instr> li(bp);

        while ((ibp= li++))
          ibp->set_destination(src, dst);
      }
      i->opt_move(dst, &bp);
      src+= 1;
      dst+= 1;
    }
  }

  m_instructions.elements(dst);
  bp.empty();
}


void sp_head::add_mark_lead(uint ip, List<sp_instr> *leads)
{
  sp_instr *i= get_instr(ip);

  if (i && !i->opt_is_marked())
    leads->push_front(i);
}


void sp_head::opt_mark()
{
  uint ip;
  sp_instr *i;
  List<sp_instr> leads;

  /*
    Forward flow analysis algorithm in the instruction graph:
    - first, add the entry point in the graph (the first instruction) to the
      'leads' list of paths to explore.
    - while there are still leads to explore:
      - pick one lead, and follow the path forward. Mark instruction reached.
        Stop only if the end of the routine is reached, or the path converge
        to code already explored (marked).
      - while following a path, collect in the 'leads' list any fork to
        another path (caused by conditional jumps instructions), so that these
        paths can be explored as well.
  */

  /* Add the entry point */
  i= get_instr(0);
  leads.push_front(i);

  /* For each path of code ... */
  while (leads.elements != 0)
  {
    i= leads.pop();

    /* Mark the entire path, collecting new leads. */
    while (i && !i->opt_is_marked())
    {
      ip= i->opt_mark(this, & leads);
      i= get_instr(ip);
    }
  }
}


#ifndef DBUG_OFF
bool sp_head::show_routine_code(THD *thd)
{
  Protocol *protocol= thd->protocol;
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  List<Item> field_list;
  sp_instr *i;
  bool full_access;
  bool res= false;
  uint ip;

  if (check_show_access(thd, &full_access) || !full_access)
    return true;

  field_list.push_back(new Item_uint(NAME_STRING("Pos"), 0, 9));
  // 1024 is for not to confuse old clients
  field_list.push_back(new Item_empty_string("Instruction",
                                             std::max(buffer.length(), 1024U)));
  if (protocol->send_result_set_metadata(&field_list, Protocol::SEND_NUM_ROWS |
                                         Protocol::SEND_EOF))
    return true;

  for (ip= 0; (i = get_instr(ip)) ; ip++)
  {
    /*
      Consistency check. If these are different something went wrong
      during optimization.
    */
    if (ip != i->get_ip())
    {
      const char *format= "Instruction at position %u has m_ip=%u";
      char tmp[sizeof(format) + 2 * sizeof(uint) + 1];

      sprintf(tmp, format, ip, i->get_ip());
      /*
        Since this is for debugging purposes only, we don't bother to
        introduce a special error code for it.
      */
      push_warning(thd, Sql_condition::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR, tmp);
    }
    protocol->prepare_for_resend();
    protocol->store((longlong)ip);

    buffer.set("", 0, system_charset_info);
    i->print(&buffer);
    protocol->store(buffer.ptr(), buffer.length(), system_charset_info);
    if ((res= protocol->write()))
      break;
  }

  if (!res)
    my_eof(thd);

  return res;
}
#endif // ifndef DBUG_OFF


bool sp_head::merge_table_list(THD *thd,
                               TABLE_LIST *table,
                               LEX *lex_for_tmp_check)
{
  if (lex_for_tmp_check->sql_command == SQLCOM_DROP_TABLE &&
      lex_for_tmp_check->drop_temporary)
    return true;

  for (uint i= 0 ; i < m_sptabs.records ; i++)
  {
    SP_TABLE *tab= (SP_TABLE*) my_hash_element(&m_sptabs, i);
    tab->query_lock_count= 0;
  }

  for (; table ; table= table->next_global)
    if (!table->derived && !table->schema_table)
    {
      /*
        Structure of key for the multi-set is "db\0table\0alias\0".
        Since "alias" part can have arbitrary length we use String
        object to construct the key. By default String will use
        buffer allocated on stack with NAME_LEN bytes reserved for
        alias, since in most cases it is going to be smaller than
        NAME_LEN bytes.
      */
      char tname_buff[(NAME_LEN + 1) * 3];
      String tname(tname_buff, sizeof(tname_buff), &my_charset_bin);
      uint temp_table_key_length;

      tname.length(0);
      tname.append(table->db, table->db_length);
      tname.append('\0');
      tname.append(table->table_name, table->table_name_length);
      tname.append('\0');
      temp_table_key_length= tname.length();
      tname.append(table->alias);
      tname.append('\0');

      /*
        Upgrade the lock type because this table list will be used
        only in pre-locked mode, in which DELAYED inserts are always
        converted to normal inserts.
      */
      if (table->lock_type == TL_WRITE_DELAYED)
        table->lock_type= TL_WRITE;

      /*
        We ignore alias when we check if table was already marked as temporary
        (and therefore should not be prelocked). Otherwise we will erroneously
        treat table with same name but with different alias as non-temporary.
      */

      SP_TABLE *tab;

      if ((tab= (SP_TABLE*) my_hash_search(&m_sptabs, (uchar *)tname.ptr(),
                                           tname.length())) ||
          ((tab= (SP_TABLE*) my_hash_search(&m_sptabs, (uchar *)tname.ptr(),
                                            temp_table_key_length)) &&
           tab->temp))
      {
        if (tab->lock_type < table->lock_type)
          tab->lock_type= table->lock_type; // Use the table with the highest lock type
        tab->query_lock_count++;
        if (tab->query_lock_count > tab->lock_count)
          tab->lock_count++;
        tab->trg_event_map|= table->trg_event_map;
      }
      else
      {
        if (!(tab= (SP_TABLE *)thd->calloc(sizeof(SP_TABLE))))
          return false;
        if (lex_for_tmp_check->sql_command == SQLCOM_CREATE_TABLE &&
            lex_for_tmp_check->query_tables == table &&
            lex_for_tmp_check->create_info.options & HA_LEX_CREATE_TMP_TABLE)
        {
          tab->temp= true;
          tab->qname.length= temp_table_key_length;
        }
        else
          tab->qname.length= tname.length();
        tab->qname.str= (char*) thd->memdup(tname.ptr(), tab->qname.length);
        if (!tab->qname.str)
          return false;
        tab->table_name_length= table->table_name_length;
        tab->db_length= table->db_length;
        tab->lock_type= table->lock_type;
        tab->lock_count= tab->query_lock_count= 1;
        tab->trg_event_map= table->trg_event_map;
        if (my_hash_insert(&m_sptabs, (uchar *)tab))
          return false;
      }
    }
  return true;
}


bool sp_head::add_used_tables_to_table_list(THD *thd,
                                            TABLE_LIST ***query_tables_last_ptr,
                                            TABLE_LIST *belong_to_view)
{
  Query_arena *arena, backup;
  bool result= false;

  /*
    Use persistent arena for table list allocation to be PS/SP friendly.
    Note that we also have to copy database/table names and alias to PS/SP
    memory since current instance of sp_head object can pass away before
    next execution of PS/SP for which tables are added to prelocking list.
    This will be fixed by introducing of proper invalidation mechanism
    once new TDC is ready.
  */
  arena= thd->activate_stmt_arena_if_needed(&backup);

  for (uint i= 0; i < m_sptabs.records; i++)
  {
    char *tab_buff, *key_buff;
    SP_TABLE *stab= (SP_TABLE*) my_hash_element(&m_sptabs, i);
    if (stab->temp)
      continue;

    if (!(tab_buff= (char *)thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST)) *
                                        stab->lock_count)) ||
        !(key_buff= (char*)thd->memdup(stab->qname.str,
                                       stab->qname.length)))
      return false;

    for (uint j= 0; j < stab->lock_count; j++)
    {
      TABLE_LIST *table= (TABLE_LIST *)tab_buff;

      table->db= key_buff;
      table->db_length= stab->db_length;
      table->table_name= table->db + table->db_length + 1;
      table->table_name_length= stab->table_name_length;
      table->alias= table->table_name + table->table_name_length + 1;
      table->lock_type= stab->lock_type;
      table->cacheable_table= 1;
      table->prelocking_placeholder= 1;
      table->belong_to_view= belong_to_view;
      table->trg_event_map= stab->trg_event_map;
      /*
        Since we don't allow DDL on base tables in prelocked mode it
        is safe to infer the type of metadata lock from the type of
        table lock.
      */
      table->mdl_request.init(MDL_key::TABLE, table->db, table->table_name,
                              table->lock_type >= TL_WRITE_ALLOW_WRITE ?
                              MDL_SHARED_WRITE : MDL_SHARED_READ,
                              MDL_TRANSACTION);

      /* Everyting else should be zeroed */

      **query_tables_last_ptr= table;
      table->prev_global= *query_tables_last_ptr;
      *query_tables_last_ptr= &table->next_global;

      tab_buff+= ALIGN_SIZE(sizeof(TABLE_LIST));
      result= true;
    }
  }

  if (arena)
    thd->restore_active_arena(arena, &backup);

  return result;
}


bool sp_head::check_show_access(THD *thd, bool *full_access)
{
  TABLE_LIST tables;
  memset(&tables, 0, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "proc";

  *full_access=
    ((!check_table_access(thd, SELECT_ACL, &tables, false, 1, true) &&
      (tables.grant.privilege & SELECT_ACL) != 0) ||
     (!strcmp(m_definer_user.str, thd->security_ctx->priv_user) &&
      !strcmp(m_definer_host.str, thd->security_ctx->priv_host)));

  return *full_access ?
         false :
         check_some_routine_access(thd, m_db.str, m_name.str,
                                   m_type == SP_TYPE_PROCEDURE);
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
bool sp_head::set_security_ctx(THD *thd, Security_context **save_ctx)
{
  *save_ctx= NULL;

  if (m_chistics->suid != SP_IS_NOT_SUID &&
      m_security_ctx.change_security_context(thd,
                                             &m_definer_user, &m_definer_host,
                                             &m_db, save_ctx))
  {
    return true;
  }

  /*
    If we changed context to run as another user, we need to check the
    access right for the new context again as someone may have revoked
    the right to use the procedure from this user.
  */

  if (*save_ctx &&
      check_routine_access(thd, EXECUTE_ACL, m_db.str, m_name.str,
                           m_type == SP_TYPE_PROCEDURE, false))
  {
    m_security_ctx.restore_security_context(thd, *save_ctx);
    *save_ctx= NULL;
    return true;
  }

  return false;
}
#endif // ! NO_EMBEDDED_ACCESS_CHECKS


///////////////////////////////////////////////////////////////////////////
// sp_parser_data implementation.
///////////////////////////////////////////////////////////////////////////


void sp_parser_data::start_parsing_sp_body(THD *thd, sp_head *sp)
{
  m_saved_memroot= thd->mem_root;
  m_saved_free_list= thd->free_list;

  thd->mem_root= sp->get_persistent_mem_root();
  thd->free_list= NULL;
}


bool sp_parser_data::add_backpatch_entry(sp_branch_instr *i,
                                         sp_label *label)
{
  Backpatch_info *bp= (Backpatch_info *)sql_alloc(sizeof(Backpatch_info));

  if (!bp)
    return true;

  bp->label= label;
  bp->instr= i;
  return m_backpatch.push_front(bp);
}


void sp_parser_data::do_backpatch(sp_label *label, uint dest)
{
  Backpatch_info *bp;
  List_iterator_fast<Backpatch_info> li(m_backpatch);

  while ((bp= li++))
  {
    if (bp->label == label)
      bp->instr->backpatch(dest);
  }
}


bool sp_parser_data::add_cont_backpatch_entry(sp_lex_branch_instr *i)
{
  i->set_cont_dest(m_cont_level);
  return m_cont_backpatch.push_front(i);
}


void sp_parser_data::do_cont_backpatch(uint dest)
{
  sp_lex_branch_instr *i;

  while ((i= m_cont_backpatch.head()) && i->get_cont_dest() == m_cont_level)
  {
    i->set_cont_dest(dest);
    m_cont_backpatch.pop();
  }

  --m_cont_level;
}


void sp_parser_data::process_new_sp_instr(THD* thd, sp_instr *i)
{
  /*
    thd->free_list should be cleaned here because it's implicitly expected
    that that process_new_sp_instr() (called from sp_head::add_instr) is
    called as the last action after parsing the SP-instruction's SQL query.

    Thus, at this point thd->free_list contains all Item-objects, created for
    this SP-instruction.

    Next SP-instruction should start its own free-list from the scratch.
  */

  i->free_list= thd->free_list;

  thd->free_list= NULL;
}
