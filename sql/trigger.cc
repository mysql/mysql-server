/*
   Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "sql_class.h"
#include "trigger.h"
#include "mysys_err.h"            // EE_OUTOFMEMORY
#include "trigger_creation_ctx.h" // Trigger_creation_ctx
#include "sql_parse.h"            // parse_sql
#include "sp.h"                   // sp_update_stmt_used_routines
#include "sql_table.h"            // check_n_cut_mysql50_prefix
#include "sql_show.h"             // append_identifier
#include "sql_db.h"               // get_default_db_collation

#include "mysql/psi/mysql_sp.h"
///////////////////////////////////////////////////////////////////////////

/**
  An error handler that catches all non-OOM errors which can occur during
  parsing of trigger body. Such errors are ignored and corresponding error
  message is used to construct a more verbose error message which contains
  name of problematic trigger. This error message is later emitted when
  one tries to perform DML or some of DDL on this table.
  Also, if possible, grabs name of the trigger being parsed so it can be
  used to correctly drop problematic trigger.
*/
class Deprecated_trigger_syntax_handler : public Internal_error_handler
{
private:
  char m_message[MYSQL_ERRMSG_SIZE];
  LEX_STRING *m_trigger_name;

public:
  Deprecated_trigger_syntax_handler() : m_trigger_name(NULL) {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char *sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char *message)
  {
    if (sql_errno != EE_OUTOFMEMORY &&
        sql_errno != ER_OUT_OF_RESOURCES)
    {
      if (thd->lex->spname)
        m_trigger_name= &thd->lex->spname->m_name;
      if (m_trigger_name)
        my_snprintf(m_message, sizeof(m_message),
                    ER(ER_ERROR_IN_TRIGGER_BODY),
                    m_trigger_name->str, message);
      else
        my_snprintf(m_message, sizeof(m_message),
                    ER(ER_ERROR_IN_UNKNOWN_TRIGGER_BODY), message);
      return true;
    }
    return false;
  }

  LEX_STRING *get_trigger_name() { return m_trigger_name; }
  const char *get_error_message() { return m_message; }
};

///////////////////////////////////////////////////////////////////////////

/**
  Constructs DEFINER clause.

  @param mem_root           mem-root where needed strings will be allocated
  @param lex_definer        DEFINER clause from the parser (as it is specified
                            by the user). It is NULL if DEFINER clause is
                            missing.
  @param[out] definer_user  pointer to the user part of lex_definer (if any)
  @param[out] definer_host  pointer to the host part of lex_definer (if any)
  @param[out] definer       well-formed DEFINER-clause (after successful
                            execution)

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

static bool reconstruct_definer_clause(MEM_ROOT *mem_root,
                                       const LEX_USER *lex_definer,
                                       LEX_CSTRING *definer_user,
                                       LEX_CSTRING *definer_host,
                                       LEX_STRING *definer)
{
  if (lex_definer)
  {
    /* SUID trigger (DEFINER is specified by the user). */

    char definer_buf[USER_HOST_BUFF_SIZE];

    *definer_user= lex_definer->user;
    *definer_host= lex_definer->host;

    size_t definer_len=
      strxmov(definer_buf,
              lex_definer->user.str, "@", lex_definer->host.str, NullS) -
      definer_buf;

    return !lex_string_copy(mem_root, definer, definer_buf, definer_len);
  }

  /* non-SUID trigger. */

  *definer_user= NULL_CSTR;
  *definer_host= NULL_CSTR;
  *definer= EMPTY_STR;

  return false;
}


/**
  Constructs CREATE TRIGGER statement.

  The point of this method is to create two canonical forms of CREATE TRIGGER
  statement: one for storing in the Data Dictionary, the other is for writing
  into the binlog.

  The difference between these two forms is that the Data Dictionary form must
  not contains FOLLOWS/PRECEDES clause, while the binlog form mist preserve it
  if it was in the original statement. The reason for that difference is this:

    - the Data Dictionary preserves the trigger execution order (action_order),
      thus FOLLOWS/PRECEDES clause is not needed.

    - moreover, FOLLOWS/PRECEDES clause usually makes problem in mysqldump,
      because CREATE TRIGGER statement will have a reference to non-yet-existing
      trigger (which is about to be created right after this one).

    - thus, FOLLOWS/PRECEDES must not be stored in the Data Dictionary.

    - on the other hand, the binlog contains statements in the user order (as
      the user executes them). Thus, it is important to preserve
      FOLLOWS/PRECEDES clause if the user has specified it so that the trigger
      execution order on master and slave will be the same.

  Both forms of CREATE TRIGGER must have the DEFINER clause if the user
  specified it (it is a SUID trigger). The DEFINER clause can not be reused
  from the parser.

  @param thd                thread context
  @param mem_root           mem-root where needed strings will be allocated
  @param[out] dd_query      well-formed CREATE TRIGGER statement for storing
                            in the Data Dictionary (after successful execution)
  @param[out] binlog_query  well-formed CREATE TRIGGER statement for putting
                            into binlog (after successful execution)
  @param[out] definer       well-formed DEFINER-clause (after successful
                            execution)

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

static bool reconstruct_create_trigger_statement(THD *thd,
                                                 MEM_ROOT *mem_root,
                                                 String *binlog_query,
                                                 String *dd_query,
                                                 LEX_STRING *definer)
{
  LEX *lex= thd->lex;

  if (dd_query->append(STRING_WITH_LEN("CREATE ")))
    return true; // OOM

  /*
    Append definer-clause if the trigger is SUID (a usual trigger in
    new MySQL versions).
  */

  LEX_CSTRING definer_user;
  LEX_CSTRING definer_host;

  if (reconstruct_definer_clause(mem_root, lex->definer,
                                 &definer_user, &definer_host, definer))
    return true;

  append_definer(thd, dd_query, definer_user, definer_host);

  if (binlog_query->append(*dd_query))
    return true; //OOM

  LEX_STRING dd_definition;
  LEX_STRING binlog_definition;

  binlog_definition.str= (char *) lex->stmt_definition_begin;
  binlog_definition.length= lex->stmt_definition_end -
                            lex->stmt_definition_begin;

  trim_whitespace(thd->charset(), &binlog_definition);

  if (lex->trg_ordering_clause_begin !=
      lex->trg_ordering_clause_end)
  {
    dd_definition.str= (char *) lex->stmt_definition_begin;
    dd_definition.length= lex->trg_ordering_clause_begin -
                          lex->stmt_definition_begin;

    if (dd_query->append(dd_definition.str, dd_definition.length))
      return true;

    dd_definition.str= (char *) lex->trg_ordering_clause_end;
    dd_definition.length= lex->stmt_definition_end -
                          lex->trg_ordering_clause_end;

    trim_whitespace(thd->charset(), &dd_definition);
  }
  else
  {
    dd_definition.str= binlog_definition.str;
    dd_definition.length= binlog_definition.length;
  }

  return dd_query->append(dd_definition.str, dd_definition.length) ||
         binlog_query->append(binlog_definition.str, binlog_definition.length);
}

///////////////////////////////////////////////////////////////////////////

/**
  Creates a new Trigger-instance with the state from the parser. This method is
  used to create a Trigger-object after CREATE TRIGGER statement is parsed.

  @see also Trigger::create_from_dd()

  @param thd                              Thread context with a valid LEX-tree
                                          of CREATE TRIGGER statement
  @param subject_table                    A valid (not fake!) subject
                                          TABLE-object
  @param [out] binlog_create_trigger_stmt Store CREATE TRIGGER appropriate to
                                          writing into the binlog. It should
                                          have DEFINER clause and should not
                                          have FOLLOWS/PRECEDES clause.
*/
Trigger *Trigger::create_from_parser(THD *thd,
                                     TABLE *subject_table,
                                     String *binlog_create_trigger_stmt)
{
  LEX *lex= thd->lex;

  /*
    Fill character set information:
      - client character set contains charset info only;
      - connection collation contains pair {character set, collation};
      - database collation contains pair {character set, collation};

    NOTE: we must allocate strings on Trigger's mem-root.
  */

  LEX_STRING client_cs_name;
  LEX_STRING connection_cl_name;
  LEX_STRING db_cl_name;

  const CHARSET_INFO *default_db_cl=
    get_default_db_collation(thd, subject_table->s->db.str);

  if (!lex_string_copy(&subject_table->mem_root,
                       &client_cs_name,
                       thd->charset()->csname) ||
      !lex_string_copy(&subject_table->mem_root,
                       &connection_cl_name,
                       thd->variables.collation_connection->name) ||
      !lex_string_copy(&subject_table->mem_root,
                       &db_cl_name,
                       default_db_cl->name))
  {
    return NULL;
  }

  // Copy trigger name into the proper mem-root.

  LEX_STRING trigger_name;
  if (!lex_string_copy(&subject_table->mem_root,
                       &trigger_name,
                       lex->spname->m_name))
    return NULL;

  // Construct two CREATE TRIGGER statements, allocate DEFINER-clause.

  String dd_create_trigger_stmt;
  dd_create_trigger_stmt.set_charset(system_charset_info);

  LEX_STRING definer;
  reconstruct_create_trigger_statement(thd,
                                       &subject_table->mem_root,
                                       binlog_create_trigger_stmt,
                                       &dd_create_trigger_stmt,
                                       &definer);

  // Copy CREATE TRIGGER statement for DD into the proper mem-root.

  LEX_STRING definition;
  if (!lex_string_copy(&subject_table->mem_root,
                       &definition,
                       dd_create_trigger_stmt.c_ptr(),
                       dd_create_trigger_stmt.length()))
    return NULL;

  /*
    Calculate time stamp up to tenths of milliseconds elapsed
    from 1 Jan 1970 00:00:00.
  */
  struct timeval cur_time= thd->query_start_timeval_trunc(2);
  longlong created_timestamp= static_cast<longlong>(cur_time.tv_sec) * 100 +
                              (cur_time.tv_usec / 10000);

  // Create a new Trigger instance.

  Trigger *t=
    new (&subject_table->mem_root) Trigger(
      &subject_table->mem_root,
      to_lex_cstring(subject_table->s->db),
      to_lex_cstring(subject_table->s->table_name),
      definition,
      thd->variables.sql_mode,
      definer,
      client_cs_name,
      connection_cl_name,
      db_cl_name,
      lex->sphead->m_trg_chistics.event,
      lex->sphead->m_trg_chistics.action_time,
      created_timestamp);

  if (!t)
    return NULL;

  /*
    NOTE: sp-head is not set in the new trigger object. That's Ok since we're
    not going to execute it, but rather use it for store new trigger in the Data
    Dictionary.
  */

  // Set trigger name.

  t->set_trigger_name(trigger_name);

  return t;
}


/**
  Creates a new Trigger-instance with the state loaded from the Data Dictionary.

  @note the Data Dictionary currently stores not all needed information, so the
  complete state of Trigger-object can be obtained only after parsing the
  definition (CREATE TRIGGER) statement. In order to do that, Trigger::parse()
  should be called.

  @see also Trigger::create_from_parser()
*/
Trigger *Trigger::create_from_dd(MEM_ROOT *mem_root,
                                 const LEX_CSTRING &db_name,
                                 const LEX_CSTRING &subject_table_name,
                                 const LEX_STRING &definition,
                                 sql_mode_t sql_mode,
                                 const LEX_STRING &definer,
                                 const LEX_STRING &client_cs_name,
                                 const LEX_STRING &connection_cl_name,
                                 const LEX_STRING &db_cl_name,
                                 const longlong *created_timestamp)
{
  return new (mem_root) Trigger(
    mem_root,
    db_name,
    subject_table_name,
    definition,
    sql_mode,
    definer,
    client_cs_name,
    connection_cl_name,
    db_cl_name,
    TRG_EVENT_MAX,
    TRG_ACTION_MAX,
    created_timestamp ? *created_timestamp : 0);
}


/**
  Trigger constructor.
*/
Trigger::Trigger(MEM_ROOT *mem_root,
                 const LEX_CSTRING &db_name,
                 const LEX_CSTRING &subject_table_name,
                 const LEX_STRING &definition,
                 sql_mode_t sql_mode,
                 const LEX_STRING &definer,
                 const LEX_STRING &client_cs_name,
                 const LEX_STRING &connection_cl_name,
                 const LEX_STRING &db_cl_name,
                 enum_trigger_event_type event_type,
                 enum_trigger_action_time_type action_time,
                 longlong created_timestamp)
 :m_mem_root(mem_root),
  m_db_name(db_name),
  m_subject_table_name(subject_table_name),
  m_definition(definition),
  m_sql_mode(sql_mode),
  m_definer(definer),
  m_client_cs_name(client_cs_name),
  m_connection_cl_name(connection_cl_name),
  m_db_cl_name(db_cl_name),
  m_event(event_type),
  m_action_time(action_time),
  m_created_timestamp(created_timestamp),
  m_action_order(0),
  m_sp(NULL),
  m_has_parse_error(false)
{
  m_trigger_name= NULL_STR;
  m_on_table_name= NULL_STR;

  m_parse_error_message[0]= 0;
  memset(&m_subject_table_grant, 0, sizeof (m_subject_table_grant));
}


/**
  Destroy associated SP (if any).
*/
Trigger::~Trigger()
{
  delete m_sp;
}


/**
  Execute trigger's body.

  @param [in] thd   Thread context

  @return Operation status
    @retval true   Trigger execution failed or trigger has compilation errors
    @retval false  Success
*/

bool Trigger::execute(THD *thd)
{
  if (m_has_parse_error)
    return true;

  bool err_status;
  Sub_statement_state statement_state;
  SELECT_LEX *save_current_select;

  thd->reset_sub_statement_state(&statement_state, SUB_STMT_TRIGGER);

  /*
    Reset current_select before call execute_trigger() and
    restore it after return from one. This way error is set
    in case of failure during trigger execution.
  */
  save_current_select= thd->lex->current_select();
  thd->lex->set_current_select(NULL);
  err_status=
    m_sp->execute_trigger(thd,
                          m_db_name,
                          m_subject_table_name,
                          &m_subject_table_grant);
  thd->lex->set_current_select(save_current_select);

  thd->restore_sub_statement_state(&statement_state);

  return err_status;
}


/**
  Parse CREATE TRIGGER statement.

  @param [in] thd   Thread context

  @return true if a fatal parse error happened (the parser failed to extract
  even the trigger name), false otherwise (Trigger::has_parse_error() might
  still return true in this case).
*/

bool Trigger::parse(THD *thd)
{
  sql_mode_t sql_mode_saved= thd->variables.sql_mode;
  thd->variables.sql_mode= m_sql_mode;

  Parser_state parser_state;
  if (parser_state.init(thd, m_definition.str, m_definition.length))
  {
    thd->variables.sql_mode= sql_mode_saved;
    return true;
  }

  LEX *lex_saved= thd->lex;

  LEX lex;
  thd->lex= &lex;
  lex_start(thd);

  LEX_CSTRING current_db_name_saved= thd->db();
  thd->reset_db(m_db_name);

  Deprecated_trigger_syntax_handler error_handler;
  thd->push_internal_handler(&error_handler);

  sp_rcontext *sp_runtime_ctx_saved= thd->sp_runtime_ctx;
  thd->sp_runtime_ctx= NULL;

  sql_digest_state *digest_saved= thd->m_digest;
  PSI_statement_locker *statement_locker_saved= thd->m_statement_psi;
  thd->m_digest= NULL;
  thd->m_statement_psi= NULL;

  Trigger_creation_ctx *creation_ctx=
      Trigger_creation_ctx::create(thd,
                                   m_db_name,
                                   m_subject_table_name,
                                   m_client_cs_name,
                                   m_connection_cl_name,
                                   m_db_cl_name);

  bool parse_error= parse_sql(thd, &parser_state, creation_ctx);

  thd->m_digest= digest_saved;
  thd->m_statement_psi= statement_locker_saved;
  thd->sp_runtime_ctx= sp_runtime_ctx_saved;
  thd->variables.sql_mode= sql_mode_saved;

  thd->pop_internal_handler();

  /*
    Not strictly necessary to invoke this method here, since we know
    that we've parsed CREATE TRIGGER and not an
    UPDATE/DELETE/INSERT/REPLACE/LOAD/CREATE TABLE, but we try to
    maintain the invariant that this method is called for each
    distinct statement, in case its logic is extended with other
    types of analyses in future.
  */
  lex.set_trg_event_type_for_tables();

  // Remember parse error message.

  if (parse_error)
    set_parse_error_message(error_handler.get_error_message());

  // Ensure that lex.sp_head is NULL in case of parse errors.

  DBUG_ASSERT(!parse_error || (parse_error && lex.sphead == NULL));

  // fatal_parse_error will be returned from this method.

  bool fatal_parse_error= false;

  // Set trigger name.

  {
    /*
      Get trigger name:
        - in case of parse error, trigger name can be fetched from error
          handler;
        - otherwise it can be retrieved from the parser.
    */

    const LEX_STRING *trigger_name_ptr= NULL;

    if (parse_error)
    {
      if (!error_handler.get_trigger_name())
      {
        // We failed to parse trigger name => fatal error.
        fatal_parse_error= true;
        goto cleanup;
      }

      trigger_name_ptr= error_handler.get_trigger_name();
    }
    else
    {
      trigger_name_ptr= &lex.spname->m_name;
    }

    // Make a copy of trigger name and set it.

    LEX_STRING s;
    if (!lex_string_copy(m_mem_root, &s, *trigger_name_ptr))
    {
      fatal_parse_error= true;
      goto cleanup;
    }

    set_trigger_name(s);
  }

  // That's it in case of parse error.

  if (parse_error)
    goto cleanup;

  // Set correct m_event and m_action_time.

  DBUG_ASSERT(m_event == TRG_EVENT_MAX);
  DBUG_ASSERT(m_action_time == TRG_ACTION_MAX);

  m_event= lex.sphead->m_trg_chistics.event;
  m_action_time= lex.sphead->m_trg_chistics.action_time;

  /*
    Remember a pointer to the "ON <table name>" part of the trigger definition.
    Note, that it is a pointer inside m_definition.str.
  */

  m_on_table_name.str= (char*) lex.raw_trg_on_table_name_begin;
  m_on_table_name.length= lex.raw_trg_on_table_name_end -
                          lex.raw_trg_on_table_name_begin;

  // Take ownership of SP object.

  DBUG_ASSERT(!m_sp);

  m_sp= lex.sphead;
  lex.sphead= NULL; /* Prevent double cleanup. */

  /*
    Set some SP attributes.

    NOTE: sp_head::set_info() is required on slave.
  */

  m_sp->set_info(0, // CREATED timestamp (not used for triggers)
                 0, // MODIFIED timestamp (not used for triggers)
                 &lex.sp_chistics,
                 m_sql_mode);

  DBUG_ASSERT(!m_sp->get_creation_ctx());
  m_sp->set_creation_ctx(creation_ctx);

  // Set the definer attribute in SP.

  if (!m_definer.length)
  {
    DBUG_ASSERT(m_definer.str); // m_definer must be EMPTY_STR here.

    /*
      This trigger was created/imported in MySQL version, which does not support
      triggers definers. We should emit warning here.
    */

    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TRG_NO_DEFINER, ER(ER_TRG_NO_DEFINER),
                        m_db_name.str,
                        m_trigger_name.str);

    /*
      Triggers without definer information are executed under the
      authorization of the invoker.
    */

    m_sp->m_chistics->suid= SP_IS_NOT_SUID;
  }

  m_sp->set_definer(m_definer.str, m_definer.length);

#ifdef HAVE_PSI_SP_INTERFACE
  m_sp->m_sp_share= MYSQL_GET_SP_SHARE(SP_TYPE_TRIGGER,
                                       m_sp->m_db.str, m_sp->m_db.length,
                                       m_sp->m_name.str, m_sp->m_name.length);
#endif

#ifndef DBUG_OFF
  /*
    Check that we correctly update trigger definitions when we rename tables
    with triggers.

    In special cases like "RENAME TABLE `#mysql50#somename` TO `somename`"
    or "ALTER DATABASE `#mysql50#somename` UPGRADE DATA DIRECTORY NAME"
    we might be given table or database name with "#mysql50#" prefix (and
    trigger's definiton contains un-prefixed version of the same name).
    To remove this prefix we use check_n_cut_mysql50_prefix().
  */

  char fname[NAME_LEN + 1];
  DBUG_ASSERT((!my_strcasecmp(table_alias_charset,
                              lex.query_tables->db, m_db_name.str) ||
               (check_n_cut_mysql50_prefix(m_db_name.str,
                                           fname, sizeof(fname)) &&
                !my_strcasecmp(table_alias_charset,
                               lex.query_tables->db, fname))));
  DBUG_ASSERT((!my_strcasecmp(table_alias_charset,
                              lex.query_tables->table_name,
                              m_subject_table_name.str) ||
               (check_n_cut_mysql50_prefix(m_subject_table_name.str,
                                           fname, sizeof(fname)) &&
                !my_strcasecmp(table_alias_charset,
                               lex.query_tables->table_name, fname))));
#endif

cleanup:
  lex_end(&lex);
  thd->reset_db(current_db_name_saved);
  thd->lex= lex_saved;

  return fatal_parse_error;
}


/**
  Add tables and routines used by trigger to the set of elements
  used by statement.

  @param [in]     thd               thread handle
  @param [in out] prelocking_ctx    prelocking context of the statement
  @param [in]     table_list        TABLE_LIST for the table
*/

void Trigger::add_tables_and_routines(THD *thd,
                                      Query_tables_list *prelocking_ctx,
                                      TABLE_LIST *table_list)
{
  if (has_parse_error())
    return;

  MDL_key key(MDL_key::TRIGGER, m_sp->m_db.str, m_sp->m_name.str);

  if (sp_add_used_routine(prelocking_ctx, thd->stmt_arena,
                          &key, table_list->belong_to_view))
  {
    m_sp->add_used_tables_to_table_list(thd,
                                        &prelocking_ctx->query_tables_last,
                                        prelocking_ctx->sql_command,
                                        table_list->belong_to_view);
    sp_update_stmt_used_routines(thd, prelocking_ctx,
                                  &m_sp->m_sroutines,
                                  table_list->belong_to_view);
    m_sp->propagate_attributes(prelocking_ctx);
  }
}


/**
  Print upgrade warnings (if any).
*/
void Trigger::print_upgrade_warning(THD *thd)
{
  if (m_created_timestamp)
    return;

  push_warning_printf(thd,
    Sql_condition::SL_WARNING,
    ER_WARN_TRIGGER_DOESNT_HAVE_CREATED,
    ER(ER_WARN_TRIGGER_DOESNT_HAVE_CREATED),
    get_db_name().str,
    get_subject_table_name().str,
    get_trigger_name().str);
}


/**
  Handles renaming of the subject table.

  The main duty of this method is to properly update m_definition and
  m_on_table_name attributes.

  @param thd              Thread context, used for passing into
                          append_identifier() function, which uses it to know
                          the way to properly escape identifiers
  @param new_table_name   New subject table name
*/
void Trigger::rename_subject_table(THD *thd, const LEX_STRING &new_table_name)
{
  /*
    sql_mode has to be set to the trigger's sql_mode because we're going to
    build a new CREATE TRIGGER statement and sql_mode affects the way we append
    identifiers.
  */

  sql_mode_t sql_mode_saved= thd->variables.sql_mode;
  thd->variables.sql_mode= get_sql_mode();

  // Construct a new CREATE TRIGGER statement with the new table name.

  String new_create_stmt;
  new_create_stmt.length(0);

  // NOTE: 'on_table_name' is supposed to point inside m_definition.

  DBUG_ASSERT(m_on_table_name.str);
  DBUG_ASSERT(m_on_table_name.str > m_definition.str);
  DBUG_ASSERT(m_on_table_name.str < (m_definition.str + m_definition.length));

  size_t before_on_len= m_on_table_name.str - m_definition.str;

  new_create_stmt.append(m_definition.str, before_on_len);
  new_create_stmt.append(STRING_WITH_LEN("ON "));

  append_identifier(thd, &new_create_stmt,
                    new_table_name.str, new_table_name.length);

  new_create_stmt.append(STRING_WITH_LEN(" "));

  size_t on_q_table_name_len= new_create_stmt.length() - before_on_len;

  new_create_stmt.append(
    m_on_table_name.str + m_on_table_name.length,
    m_definition.length - (before_on_len + m_on_table_name.length));

  lex_string_copy(m_mem_root,
                  &m_definition,
                  new_create_stmt.ptr(),
                  new_create_stmt.length());

  lex_string_copy(m_mem_root,
                  &m_on_table_name,
                  m_definition.str + before_on_len,
                  on_q_table_name_len);

  thd->variables.sql_mode= sql_mode_saved;
}
