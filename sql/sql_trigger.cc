/*
   Copyright (c) 2004, 2016, Oracle and/or its affiliates. All rights reserved.

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


#include <stddef.h>

#include "auth_acls.h"
#include "auth_common.h"              // check_table_access
#include "binlog.h"
#include "dd/cache/dictionary_client.h"
#include "dd/dd_schema.h"
#include "dd/dd_trigger.h"            // dd::table_has_triggers
#include "dd/string_type.h"
#include "dd/types/abstract_table.h"  // dd::enum_table_type
#include "debug_sync.h"               // DEBUG_SYNC
#include "derror.h"                   // ER_THD
#include "m_ctype.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_global.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "mysql/psi/mysql_sp.h"
#include "mysql_com.h"
#include "mysqld.h"                   // trust_function_creators
#include "mysqld_error.h"
#include "sp.h"                       // sp_add_to_query_tables()
#include "sp_cache.h"                 // sp_invalidate_cache()
#include "sp_head.h"                  // sp_name
#include "sql_base.h"                 // find_temporary_table()
#include "sql_class.h"
#include "sql_error.h"
                                      // write_bin_log()
#include "sql_handler.h"              // mysql_ha_rm_tables()
#include "sql_lex.h"
#include "sql_list.h"
#include "sql_plugin.h"
#include "sql_security_ctx.h"
#include "sql_string.h"
#include "sql_table.h"                // build_table_filename()
#include "sql_trigger.h"
#include "system_variables.h"
#include "table.h"
#include "table_trigger_dispatcher.h" // Table_trigger_dispatcher
#include "thr_lock.h"
#include "transaction.h"              // trans_commit_stmt, trans_commit
#include "trigger.h"                  // Trigger
///////////////////////////////////////////////////////////////////////////

bool add_table_for_trigger(THD *thd,
                           const LEX_CSTRING &db_name,
                           const LEX_STRING &trigger_name,
                           bool continue_if_not_exist,
                           TABLE_LIST **table)
{
  LEX *lex= thd->lex;

  DBUG_ENTER("add_table_for_trigger");

  dd::String_type table_name;
  bool trigger_found;
  if (dd::get_table_name_for_trigger(thd, db_name.str,
                                     trigger_name.str,
                                     &table_name,
                                     &trigger_found,
                                     continue_if_not_exist))
    DBUG_RETURN(true);

  if (!trigger_found)
  {
    if (continue_if_not_exist)
    {
      push_warning(thd, Sql_condition::SL_NOTE,
                   ER_TRG_DOES_NOT_EXIST,
                   ER_THD(thd, ER_TRG_DOES_NOT_EXIST));

      *table= NULL;

      DBUG_RETURN(false);
    }

    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    DBUG_RETURN(true);
  }

  char lc_table_name[NAME_LEN + 1];
  const char *table_name_ptr= table_name.c_str();
  if (lower_case_table_names == 2)
  {
    my_stpncpy(lc_table_name, table_name.c_str(), NAME_LEN);
    my_casedn_str(files_charset_info, lc_table_name);
    lc_table_name[NAME_LEN]= '\0';
    table_name_ptr= lc_table_name;
  }
  *table= sp_add_to_query_tables(thd, lex, db_name.str, table_name_ptr);

  DBUG_RETURN(*table ? false : true);
}


bool drop_all_triggers(THD *thd, const char *db_name, const char *table_name)
{
  // Check if there is at least one trigger for the given table.

  bool table_has_trigger;
  if (dd::table_has_triggers(thd, db_name, table_name, &table_has_trigger))
    return true;

  if (!table_has_trigger)
    return false;

  List<Trigger> triggers;
  Table_trigger_dispatcher tbl_trg_dsp(db_name, table_name);

  /*
    Load triggers from the Data Dictionary without subsequent parsing.
    To drop triggers it is required to know only trigger name, db name,
    trigger event and action time. All these information are stored in
    the Data Dictionary.
  */
  return
    tbl_trg_dsp.load_triggers(thd) ||
    tbl_trg_dsp.fill_and_return_trigger_list(&triggers) == nullptr ||
    dd::drop_all_triggers(thd, db_name, table_name,
                          &triggers);
}


bool remove_all_triggers_from_perfschema(THD *thd, TABLE_LIST *table)
{
  List<LEX_CSTRING> trigger_names;

  if (dd::load_trigger_names(thd,
                             thd->mem_root,
                             table->db,
                             table->table_name,
                             &trigger_names))
    return true;

  List_iterator_fast<LEX_CSTRING> it(trigger_names);
  LEX_CSTRING *trigger_name;

  while ((trigger_name= it++))
  {
    /* Drop statistics for this stored program from performance schema. */
    MYSQL_DROP_SP(to_uint(enum_sp_type::TRIGGER),
                  table->db, table->db_length,
                  trigger_name->str, trigger_name->length);

  }

  return false;
}


bool check_table_triggers_are_not_in_the_same_schema(THD *thd,
                                                     const char *db_name,
                                                     const char *table_name,
                                                     const char *new_db_name)
{
  // Check if there is at least one trigger for the given table.

  bool table_has_trigger;
  if (dd::table_has_triggers(thd, db_name, table_name, &table_has_trigger))
    return true;

  if (!table_has_trigger)
    return false;

  /*
    Since triggers should be in the same schema as their subject tables
    moving table with them between two schemas raises too many questions.
    (E.g. what should happen if in new schema we already have trigger
     with same name ?).
  */

  if (my_strcasecmp(table_alias_charset, db_name, new_db_name))
  {
    my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
    return true;
  }

  return false;
}


bool reload_triggers_for_table(THD *thd,
                               const char *db_name,
                               const char *table_alias,
                               const char *table_name,
                               const char *new_db_name,
                               const char *new_table_name)
{
  // Check if there is at least one trigger for the given table.

  bool table_has_trigger;
  if (dd::table_has_triggers(thd, new_db_name, new_table_name,
                              &table_has_trigger))
    return true;

  if (!table_has_trigger)
    return false;

  /*
    Since triggers should be in the same schema as their subject tables
    moving table with them between two schemas raises too many questions.
    (E.g. what should happen if in new schema we already have trigger
     with same name ?).
  */

  if (my_strcasecmp(table_alias_charset, db_name, new_db_name))
  {
    my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
    return true;
  }

  /*
    This method interfaces the mysql server code protected by
    an exclusive metadata lock.
  */
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                                           db_name,
                                                           table_name,
                                                           MDL_EXCLUSIVE));

  DBUG_ASSERT(my_strcasecmp(table_alias_charset, table_alias, new_table_name));

  Table_trigger_dispatcher tbl_trg_dsp(new_db_name, new_table_name);

  return tbl_trg_dsp.check_n_load(thd, true) ||
         tbl_trg_dsp.check_for_broken_triggers();
}


bool acquire_mdl_for_trigger(THD *thd,
                             const char *db,
                             const char *trg_name,
                             enum_mdl_type trigger_name_mdl_type)
{
  MDL_request_list mdl_requests;
  MDL_request mdl_request;

  DBUG_ASSERT(trg_name != nullptr);
  DBUG_ASSERT(trigger_name_mdl_type == MDL_EXCLUSIVE ||
              trigger_name_mdl_type == MDL_SHARED_HIGH_PRIO);

  char lc_name[NAME_LEN + 1];
  my_stpncpy(lc_name, trg_name, NAME_LEN);
  my_casedn_str(system_charset_info, lc_name);
  lc_name[NAME_LEN]= '\0';

  if (thd->global_read_lock.can_acquire_protection())
    return true;

  /*
    It isn't required to create MDL request for MDL_key::GLOBAL,
    MDL_key::SCHEMA since it was already done before while
    calling the method open_and_lock_subj_table().
  */
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TRIGGER, db, lc_name, trigger_name_mdl_type,
                   MDL_TRANSACTION);

  mdl_requests.push_front(&mdl_request);

  if (thd->mdl_context.acquire_locks(&mdl_requests,
                                     thd->variables.lock_wait_timeout))
    return true;

  return false;
}


/**
  Check that the user has TRIGGER privilege on the subject table.

  @param thd  current thread context
  @param table  table to check

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Sql_cmd_ddl_trigger_common::check_trg_priv_on_subj_table(
  THD *thd,
  TABLE_LIST *table) const
{
  TABLE_LIST **save_query_tables_own_last= thd->lex->query_tables_own_last;
  thd->lex->query_tables_own_last= nullptr;

  bool err_status= check_table_access(thd, TRIGGER_ACL, table, false, 1,
                                      false);

  thd->lex->query_tables_own_last= save_query_tables_own_last;

  return err_status;
}


/**
  Open and lock a table associated with a trigger.

  @param[in] thd  current thread context
  @param[in] tables  trigger's table
  @param[out] mdl_ticket  granted metadata lock

  @return Opened TABLE object on success, nullptr on failure.
*/

TABLE* Sql_cmd_ddl_trigger_common::open_and_lock_subj_table(
  THD *thd,
  TABLE_LIST *tables,
  MDL_ticket **mdl_ticket) const
{
  /* We should have only one table in table list. */
  DBUG_ASSERT(tables->next_global == nullptr);

  /* We also don't allow creation of triggers on views. */
  tables->required_type= dd::enum_table_type::BASE_TABLE;
  /*
    Also prevent DROP TRIGGER from opening temporary table which might
    shadow the subject table on which trigger to be dropped is defined.
  */
  tables->open_type= OT_BASE_ONLY;

  /* Keep consistent with respect to other DDL statements */
  mysql_ha_rm_tables(thd, tables);

  if (thd->locked_tables_mode)
  {
    /* Under LOCK TABLES we must only accept write locked tables. */
    tables->table= find_table_for_mdl_upgrade(thd, tables->db,
                                              tables->table_name,
                                              false);
    if (tables->table == nullptr)
      return nullptr;
  }
  else
  {
    tables->table= open_n_lock_single_table(thd, tables,
                                            TL_READ_NO_INSERT, 0);
    if (tables->table == nullptr)
      return nullptr;
    tables->table->use_all_columns();
  }

  TABLE *table= tables->table;
  table->pos_in_table_list= tables;

  /* Later on we will need it to downgrade the lock */
  *mdl_ticket= table->mdl_ticket;

  if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
    return nullptr;

  return table;
}


/**
  Close all open instances of a trigger's table, reopen it if needed,
  invalidate SP-cache and possibly write a statement to binlog.

  @param[in] thd         Current thread context
  @param[in] db_name     Database name where trigger's table defined
  @param[in] table       Table associated with a trigger
  @param[in] stmt_query  Query string to write to binlog
  @param[in] binlog_stmt Should the statement be binlogged?

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

static bool finalize_trigger_ddl(
  THD *thd,
  const char *db_name,
  TABLE *table,
  const String &stmt_query,
  bool binlog_stmt)
{
  close_all_tables_for_name(thd, table->s, false, nullptr);
  /*
    Reopen the table if we were under LOCK TABLES.
    Ignore the return value for now. It's better to
    keep master/slave in consistent state.
  */
  thd->locked_tables_list.reopen_tables(thd);
  /*
    Invalidate SP-cache. That's needed because triggers may change list of
    pre-locking tables.
  */
  sp_cache_invalidate();

  if (!binlog_stmt)
    return false;

  thd->add_to_binlog_accessed_dbs(db_name);

  DEBUG_SYNC(thd, "trigger_ddl_stmt_before_write_to_binlog");
  return write_bin_log(thd, true, stmt_query.ptr(), stmt_query.length());
}


void Sql_cmd_ddl_trigger_common::restore_original_mdl_state(
  THD *thd, MDL_ticket *mdl_ticket) const
{
  /*
    If we are under LOCK TABLES we should restore original state of
    meta-data locks. Otherwise all locks will be released along
    with the implicit commit.
  */
  if (thd->locked_tables_mode)
    mdl_ticket->downgrade_lock(MDL_SHARED_NO_READ_WRITE);
}


/**
  Execute CREATE TRIGGER statement.

  @param thd     current thread context (including trigger definition in LEX)

  @note
    This method is mainly responsible for opening and locking of table and
    invalidation of all its instances in table cache after trigger creation.
    Real work on trigger creation is done inside Table_trigger_dispatcher
    methods.

  @todo
    TODO: We should check if user has TRIGGER privilege for table here.
    Now we just require SUPER privilege for creating/dropping because
    we don't have proper privilege checking for triggers in place yet.

  @retval false on success.
  @retval true on error
*/

bool Sql_cmd_create_trigger::execute(THD *thd)
{
  bool result= true;
  String stmt_query;
  MDL_ticket *mdl_ticket= nullptr;
  Query_tables_list backup;

  DBUG_ENTER("mysql_create_trigger");

  // This auto releaser will own the DD objects that we commit
  // at the bottom of this function.
  dd::Schema_MDL_locker schema_mdl_locker(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /* Charset of the buffer for statement must be system one. */
  stmt_query.set_charset(system_charset_info);

  if (!thd->lex->spname->m_db.length || !m_trigger_table->db_length)
  {
    my_error(ER_NO_DB_ERROR, MYF(0));
    DBUG_RETURN(true);
  }

  if (schema_mdl_locker.ensure_locked(m_trigger_table->db))
    DBUG_RETURN(true);

  /*
    We don't allow creating triggers on tables in the 'mysql' schema
  */
  if (!my_strcasecmp(system_charset_info, "mysql", m_trigger_table->db))
  {
    my_error(ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA, MYF(0));
    DBUG_RETURN(true);
  }

  /*
    There is no DETERMINISTIC clause for triggers, so can't check it.
    But a trigger can in theory be used to do nasty things (if it supported
    DROP for example) so we do the check for privileges. For now there is
    already a stronger test right above; but when this stronger test will
    be removed, the test below will hold. Because triggers have the same
    nature as functions regarding binlogging: their body is implicitly
    binlogged, so they share the same danger, so trust_function_creators
    applies to them too.
  */
  if (!trust_function_creators && mysql_bin_log.is_open() &&
      !(thd->security_context()->check_access(SUPER_ACL)))
  {
    my_error(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER, MYF(0));
    DBUG_RETURN(true);
  }

  if (check_trg_priv_on_subj_table(thd, m_trigger_table))
    DBUG_RETURN(true);

  /* We do not allow creation of triggers on temporary tables. */
  if (find_temporary_table(thd, m_trigger_table) != nullptr)
  {
    my_error(ER_TRG_ON_VIEW_OR_TEMP_TABLE, MYF(0), m_trigger_table->alias);
    DBUG_RETURN(true);
  }

  TABLE *table= open_and_lock_subj_table(thd, m_trigger_table, &mdl_ticket);
  if (table == nullptr)
    DBUG_RETURN(true);

  if (acquire_exclusive_mdl_for_trigger(thd, thd->lex->spname->m_db.str,
                                        thd->lex->spname->m_name.str))
  {
    restore_original_mdl_state(thd, mdl_ticket);
    DBUG_RETURN(true);
  }

  DEBUG_SYNC(thd, "create_trigger_has_acquired_mdl");

  if (table->triggers == nullptr &&
      (table->triggers= Table_trigger_dispatcher::create(table)) == nullptr)
  {
    restore_original_mdl_state(thd, mdl_ticket);
    DBUG_RETURN(true);
  }

  result= table->triggers->create_trigger(thd, &stmt_query);

  result|= finalize_trigger_ddl(thd, m_trigger_table->db, table, stmt_query,
                                !result);

  if (!result)
  {
    if (!(result= trans_commit_stmt(thd)) &&
        !(result= trans_commit(thd)))
    {
#ifdef HAVE_PSI_SP_INTERFACE
      /* Drop statistics for this stored program from performance schema. */
      MYSQL_DROP_SP(to_uint(enum_sp_type::TRIGGER),
                    thd->lex->spname->m_db.str, thd->lex->spname->m_db.length,
                    thd->lex->spname->m_name.str,
                    thd->lex->spname->m_name.length);
#endif
      my_ok(thd);
    }
  }

  restore_original_mdl_state(thd, mdl_ticket);

  DBUG_RETURN(result);
}


/**
  Execute DROP TRIGGER statement.

  @param thd     current thread context

  @note
    This method is mainly responsible for opening and locking of table and
    invalidation of all its instances in table cache after trigger creation.
    Real work on trigger creation/dropping is done inside
    Table_trigger_dispatcher methods.

  @todo
    TODO: We should check if user has TRIGGER privilege for table here.
    Now we just require SUPER privilege for creating/dropping because
    we don't have proper privilege checking for triggers in place yet.

  @retval
    false Success
  @retval
    true  error
*/

bool Sql_cmd_drop_trigger::execute(THD *thd)
{
  bool result= true;
  String stmt_query;
  MDL_ticket *mdl_ticket= nullptr;
  Query_tables_list backup;
  TABLE_LIST *tables= nullptr;

  DBUG_ENTER("Sql_cmd_drop_trigger::execute");

  // This auto releaser will own the DD objects that we commit
  // at the bottom of this function.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /* Charset of the buffer for statement must be system one. */
  stmt_query.set_charset(system_charset_info);

  if (!thd->lex->spname->m_db.length)
  {
    my_error(ER_NO_DB_ERROR, MYF(0));
    DBUG_RETURN(true);
  }

  bool if_exists= thd->lex->drop_if_exists;

  /*
    Protect the query table list from the temporary and potentially
    destructive changes necessary to open the trigger's table.
  */
  thd->lex->reset_n_backup_query_tables_list(&backup);

  /*
    Restore Query_tables_list::sql_command, which was
    reset above, as the code that writes the query to the
    binary log assumes that this value corresponds to the
    statement that is being executed.
  */
  thd->lex->sql_command= backup.sql_command;

  if (check_readonly(thd, true))
  {
    thd->lex->restore_backup_query_tables_list(&backup);
    DBUG_RETURN(true);
  }

  if (acquire_exclusive_mdl_for_trigger(thd, thd->lex->spname->m_db.str,
                                        thd->lex->spname->m_name.str))
  {
    thd->lex->restore_backup_query_tables_list(&backup);
    DBUG_RETURN(true);
  }

  if (add_table_for_trigger(thd,
                            thd->lex->spname->m_db,
                            thd->lex->spname->m_name,
                            if_exists, &tables))
  {
    thd->lex->restore_backup_query_tables_list(&backup);
    DBUG_RETURN(true);
  }

  if (tables == nullptr)
  {
    DBUG_ASSERT(if_exists);
    /*
      Since the trigger does not exist, there is no associated table,
      and therefore :
      - no TRIGGER privileges to check,
      - no trigger to drop,
      - no table to lock/modify,
      so the drop statement is successful.
    */

    /* Still, we need to log the query ... */
    stmt_query.append(thd->query().str, thd->query().length);
    result= write_bin_log(thd, true, stmt_query.ptr(), stmt_query.length());

    thd->lex->restore_backup_query_tables_list(&backup);

    if (!result)
      my_ok(thd);
    DBUG_RETURN(result);
  }

  if (check_trg_priv_on_subj_table(thd, tables))
  {
    thd->lex->restore_backup_query_tables_list(&backup);
    DBUG_RETURN(true);
  }

  TABLE *table= open_and_lock_subj_table(thd, tables, &mdl_ticket);
  if (table == nullptr)
  {
    thd->lex->restore_backup_query_tables_list(&backup);
    DBUG_RETURN(true);
  }

  if (table->triggers == nullptr)
  {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    restore_original_mdl_state(thd, mdl_ticket);
    thd->lex->restore_backup_query_tables_list(&backup);
    DBUG_RETURN(true);
  }

  DEBUG_SYNC(thd, "drop_trigger_has_acquired_mdl");

  bool trigger_found;

  result= table->triggers->drop_trigger(thd,
                                        thd->lex->spname->m_name,
                                        &trigger_found);

  if (!result && trigger_found)
    result= stmt_query.append(thd->query().str, thd->query().length);

  result|= finalize_trigger_ddl(thd, tables->db, table, stmt_query, !result);

  /* Restore the query table list. */
  thd->lex->restore_backup_query_tables_list(&backup);

  if (!result)
  {
    if (!(result= trans_commit_stmt(thd)) &&
        !(result= trans_commit(thd)))
    {
#ifdef HAVE_PSI_SP_INTERFACE
      /* Drop statistics for this stored program from performance schema. */
      MYSQL_DROP_SP(to_uint(enum_sp_type::TRIGGER),
                    thd->lex->spname->m_db.str, thd->lex->spname->m_db.length,
                    thd->lex->spname->m_name.str,
                    thd->lex->spname->m_name.length);
#endif
      my_ok(thd);
    }
  }

  restore_original_mdl_state(thd, mdl_ticket);

  DBUG_RETURN(result);
}
