/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
#include "sp_head.h"
#include "sql_table.h"  // check_n_cut_mysql50_prefix
#include "mysys_err.h"    // EE_OUTOFMEMORY
#include "trigger_creation_ctx.h"
#include "sql_parse.h"    // parse_sql()
#include "sql_lex.h" //Query_tables_list
#include "table.h" // TABLE_LIST
#include "sp.h" // sp_update_stmt_used_routines, sp_add_used_routine


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
                                const char* sqlstate,
                                Sql_condition::enum_severity_level level,
                                const char* message,
                                Sql_condition ** cond_hdl)
  {
    if (sql_errno != EE_OUTOFMEMORY &&
        sql_errno != ER_OUT_OF_RESOURCES)
    {
      if(thd->lex->spname)
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


/**
  Initialize an instance of Trigger.

  @param [in] thd               thread handle
  @param [in] lex               LEX for parsing of trigger
  @param [in] trigger_name      trigger name
  @param [in] trg_creation_ctx  creation context of trigger
  @param [in] db_name           name of schema
  @param [in] table             pointer to the trigger's table

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool Trigger::init(THD *thd,
                   LEX *lex,
                   LEX_STRING *trigger_name,
                   Stored_program_creation_ctx *trg_creation_ctx,
                   const LEX_STRING *db_name,
                   TABLE *table)
{
  m_sp= lex->sphead;
  m_sp->set_info(0, 0, &lex->sp_chistics, m_sql_mode);

  m_event= m_sp->m_trg_chistics.event;
  m_action_time= m_sp->m_trg_chistics.action_time;

  lex->sphead= NULL; /* Prevent double cleanup. */

  m_sp->set_creation_ctx(trg_creation_ctx);
  set_trigger_name(trigger_name);

  if (!m_definer->length)
  {
    /*
      This trigger was created/imported from the previous version of
      MySQL, which does not support triggers definers. We should emit
      warning here.
    */

    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TRG_NO_DEFINER, ER(ER_TRG_NO_DEFINER),
                        db_name->str,
                        m_sp->m_name.str);

    /*
      Set definer to the '' to correct displaying in the information
      schema.
    */

    m_sp->set_definer((char*) "", 0);

    /*
      Triggers without definer information are executed under the
      authorization of the invoker.
    */

    m_sp->m_chistics->suid= SP_IS_NOT_SUID;
  }
  else
    m_sp->set_definer(m_definer->str, m_definer->length);

  m_on_table_name= alloc_lex_string(&table->mem_root);

  if (!m_on_table_name)
    return true;

  m_on_table_name->str= (char*) lex->raw_trg_on_table_name_begin;
  m_on_table_name->length= lex->raw_trg_on_table_name_end -
                           lex->raw_trg_on_table_name_begin;

  return false;
}


/**
  Execute trigger's body.

  @param [in] thd               Thread handle

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
  save_current_select= thd->lex->current_select;
  thd->lex->current_select= NULL;
  err_status=
    m_sp->execute_trigger(thd,
                          m_db_name,
                          m_table_name,
                          &m_subject_table_grant);
  thd->lex->current_select= save_current_select;

  thd->restore_sub_statement_state(&statement_state);

  return err_status;
}


/**
  Get information about trigger.

  @param[out] trg_name            trigger name
  @param[out] trigger_stmt        returns statement of trigger
  @param[out] sql_mode            returns sql_mode of trigger
  @param[out] trg_definer         returns definer/creator of trigger
  @param[out] trg_definition      returns definition of trigger
  @param[out] client_cs_name      returns client character set
  @param[out] connection_cl_name  returns connection collation
  @param[out] db_cl_name          returns database collation
*/

void Trigger::get_info(LEX_STRING *trg_name,
                       LEX_STRING *trigger_stmt,
                       sql_mode_t *sql_mode,
                       LEX_STRING *trg_definer,
                       LEX_STRING *trg_definition,
                       LEX_STRING *client_cs_name,
                       LEX_STRING *connection_cl_name,
                       LEX_STRING *db_cl_name) const
{
  *trg_name= *m_trigger_name;
  if (trigger_stmt)
    *trigger_stmt= m_sp->m_body_utf8;
  *sql_mode= m_sql_mode;

  if (trg_definer)
    *trg_definer= *m_definer;

  if (trg_definition)
    *trg_definition= *m_definition;

  *client_cs_name= *m_client_cs_name;
  *connection_cl_name= *m_connection_cl_name;
  *db_cl_name= *m_db_cl_name;
}


/**
  Parse CREATE TRIGGER statement.

  @param [in] thd               thread handle
  @param [in] table             pointer to the trigger's table

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool Trigger::parse_trigger_body(THD *thd, TABLE *table)
{
  LEX *old_lex= thd->lex, lex;
  sp_rcontext *sp_runtime_ctx_saved= thd->sp_runtime_ctx;
  sql_mode_t sql_mode_saved= thd->variables.sql_mode;
  PSI_statement_locker *parent_locker= thd->m_statement_psi;
  LEX_STRING *trigger_name= NULL;
  bool result= false;

  DBUG_ENTER("parse_trigger_body");
  thd->variables.sql_mode= m_sql_mode;

  Parser_state parser_state;
  if (parser_state.init(thd, m_definition->str, m_definition->length))
  {
    thd->variables.sql_mode= sql_mode_saved;
    return true;
  }

  thd->lex= &lex;

  LEX_STRING current_db_name_saved= {thd->db, thd->db_length};
  thd->reset_db(m_db_name->str, m_db_name->length);

  Trigger_creation_ctx *creation_ctx=
      Trigger_creation_ctx::create(thd,
                                   m_db_name->str,
                                   m_table_name->str,
                                   m_client_cs_name,
                                   m_connection_cl_name,
                                   m_db_cl_name);

  lex_start(thd);
  thd->sp_runtime_ctx= NULL;

  Deprecated_trigger_syntax_handler error_handler;
  thd->push_internal_handler(&error_handler);
  thd->m_statement_psi= NULL;
  bool parse_error= parse_sql(thd, &parser_state, creation_ctx);
  thd->m_statement_psi= parent_locker;
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

  const LEX_STRING *trigger_name_ptr= NULL;

  if (parse_error)
  {
    set_parse_error_message(error_handler.get_error_message());

    /* Currently sphead is always set to NULL in case of a parse error */
    DBUG_ASSERT(lex.sphead == NULL);

    if (!error_handler.get_trigger_name())
    {
      result= true;
      goto cleanup;
    }

    trigger_name_ptr= error_handler.get_trigger_name();
  }
  else
  {
    trigger_name_ptr= &lex.spname->m_name;
  }

  // Make a copy of trigger name.

  trigger_name= lex_string_dup(&table->mem_root, trigger_name_ptr);

  if (!trigger_name)
  {
    result= true;
    goto cleanup;
  }

  if (!parse_error)
  {
    if (init(thd, &lex, trigger_name, creation_ctx, m_db_name, table))
    {
      result= true;
      goto cleanup;
    }
  }
  else
    set_trigger_name(trigger_name);

#ifndef DBUG_OFF
  if (!parse_error)
  {
    /*
      Let us check that we correctly update trigger definitions when we
      rename tables with triggers.

      In special cases like "RENAME TABLE `#mysql50#somename` TO `somename`"
      or "ALTER DATABASE `#mysql50#somename` UPGRADE DATA DIRECTORY NAME"
      we might be given table or database name with "#mysql50#" prefix (and
      trigger's definiton contains un-prefixed version of the same name).
      To remove this prefix we use check_n_cut_mysql50_prefix().
    */

    char fname[NAME_LEN + 1];
    DBUG_ASSERT((!my_strcasecmp(table_alias_charset,
                                lex.query_tables->db, m_db_name->str) ||
                 (check_n_cut_mysql50_prefix(m_db_name->str,
                                             fname, sizeof(fname)) &&
                  !my_strcasecmp(table_alias_charset, lex.query_tables->db, fname))));
    DBUG_ASSERT((!my_strcasecmp(table_alias_charset,
                                lex.query_tables->table_name,
                                m_table_name->str) ||
                 (check_n_cut_mysql50_prefix(m_table_name->str,
                                             fname, sizeof(fname)) &&
                  !my_strcasecmp(table_alias_charset,
                                 lex.query_tables->table_name,
                                 fname))));
  }
#endif

cleanup:
  lex_end(&lex);
  thd->reset_db(current_db_name_saved.str, current_db_name_saved.length);
  thd->lex= old_lex;
  thd->sp_runtime_ctx= sp_runtime_ctx_saved;
  thd->variables.sql_mode= sql_mode_saved;

  DBUG_RETURN(result);
}


/**
  Setup table fields referenced from trigger.

  @param [in] thd               thread handle
  @param [in] table             pointer to the trigger's table
  @param [in] dispatcher        table trigger dispatcher
*/

void Trigger::setup_fields(THD *thd,
                           TABLE *table,
                           Table_trigger_dispatcher *dispatcher)
{
  if (!m_sp)
    return;

  /*
    Also let us bind these objects to Field objects in table being
    opened.

    We ignore errors here, because if even something is wrong we still
    will be willing to open table to perform some operations (e.g.
    SELECT)...
    Anyway some things can be checked only during trigger execution.
  */
  for (Item_trigger_field *trg_field= m_sp->m_trg_table_fields.first;
       trg_field;
       trg_field= trg_field->next_trg_field)
  {
    trg_field->setup_field(thd, table, dispatcher, &m_subject_table_grant);
  }
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
  MDL_key key(MDL_key::TRIGGER, m_sp->m_db.str, m_sp->m_name.str);

  if (sp_add_used_routine(prelocking_ctx, thd->stmt_arena,
                          &key, table_list->belong_to_view))
  {
    m_sp->add_used_tables_to_table_list(thd,
                                        &prelocking_ctx->query_tables_last,
                                        table_list->belong_to_view);
    sp_update_stmt_used_routines(thd, prelocking_ctx,
                                  &m_sp->m_sroutines,
                                  table_list->belong_to_view);
    m_sp->propagate_attributes(prelocking_ctx);
  }
}


/**
  Check whether any table's fields are used in trigger.

  @param [in] used_fields       bitmap of fields to check

  @return Check result
    @retval true   Some table fields are used in trigger
    @retval false  None of table fields are used in trigger
*/

bool Trigger::is_fields_updated_in_trigger(const MY_BITMAP *used_fields) const
{
  Item_trigger_field *trg_field;
  for (trg_field= m_sp->m_trg_table_fields.first; trg_field;
       trg_field= trg_field->next_trg_field)
  {
    /* We cannot check fields which does not present in table. */
    if (trg_field->field_idx != (uint)-1)
    {
      if (bitmap_is_set(used_fields, trg_field->field_idx) &&
          trg_field->get_settable_routine_parameter())
        return true;
    }
  }
  return false;
}

/**
  Mark fields of subject table which we read/set in the trigger

  @param [in] trigger_table    pointer to the trigger's table
*/

void Trigger::mark_field_used(TABLE *trigger_table)
{
  Item_trigger_field *trg_field;

  for (trg_field= m_sp->m_trg_table_fields.first; trg_field;
       trg_field= trg_field->next_trg_field)
  {
    /* We cannot mark fields which does not present in table. */
    if (trg_field->field_idx != (uint)-1)
    {
      bitmap_set_bit(trigger_table->read_set, trg_field->field_idx);
      if (trg_field->get_settable_routine_parameter())
        bitmap_set_bit(trigger_table->write_set, trg_field->field_idx);
    }
  }
}
