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


#include "my_global.h"                // NO_EMBEDDED_ACCESS_CHECKS
#include "sql_trigger.h"

#include "auth_common.h"              // check_table_access
#include "sp.h"                       // sp_add_to_query_tables()
#include "sql_base.h"                 // find_temporary_table()
#include "sql_table.h"                // build_table_filename()
                                      // write_bin_log()
#include "sql_handler.h"              // mysql_ha_rm_tables()
#include "sp_cache.h"                 // sp_invalidate_cache()
#include "trigger_loader.h"           // Trigger_loader
#include "table_trigger_dispatcher.h" // Table_trigger_dispatcher
#include "binlog.h"
#include "sp_head.h"                  // sp_name

#include "mysql/psi/mysql_sp.h"

///////////////////////////////////////////////////////////////////////////

/**
  Create or drop trigger for table.

  @param thd     current thread context (including trigger definition in LEX)
  @param tables  table list containing one table for which trigger is created.
  @param create  whenever we create (TRUE) or drop (FALSE) trigger

  @note
    This function is mainly responsible for opening and locking of table and
    invalidation of all its instances in table cache after trigger creation.
    Real work on trigger creation/dropping is done inside
    Table_trigger_dispatcher methods.

  @todo
    TODO: We should check if user has TRIGGER privilege for table here.
    Now we just require SUPER privilege for creating/dropping because
    we don't have proper privilege checking for triggers in place yet.

  @retval
    FALSE Success
  @retval
    TRUE  error
*/
bool mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create)
{
  /*
    FIXME: The code below takes too many different paths depending on the
    'create' flag, so that the justification for a single function
    'mysql_create_or_drop_trigger', compared to two separate functions
    'mysql_create_trigger' and 'mysql_drop_trigger' is not apparent.
    This is a good candidate for a minor refactoring.
  */
  TABLE *table;
  bool result= TRUE;
  String stmt_query;
  bool lock_upgrade_done= FALSE;
  MDL_ticket *mdl_ticket= NULL;
  Query_tables_list backup;

  DBUG_ENTER("mysql_create_or_drop_trigger");

  /* Charset of the buffer for statement must be system one. */
  stmt_query.set_charset(system_charset_info);

  /*
    QQ: This function could be merged in mysql_alter_table() function
    But do we want this ?
  */

  /*
    Note that once we will have check for TRIGGER privilege in place we won't
    need second part of condition below, since check_access() function also
    checks that db is specified.
  */
  if (!thd->lex->spname->m_db.length || (create && !tables->db_length))
  {
    my_error(ER_NO_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    We don't allow creating triggers on tables in the 'mysql' schema
  */
  if (create && !my_strcasecmp(system_charset_info, "mysql", tables->db))
  {
    my_error(ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA, MYF(0));
    DBUG_RETURN(TRUE);
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
    DBUG_RETURN(TRUE);
  }

  if (!create)
  {
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
      goto end;

    if (add_table_for_trigger(thd,
                              thd->lex->spname->m_db,
                              thd->lex->spname->m_name,
                              if_exists, & tables))
      goto end;

    if (!tables)
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
      result= FALSE;
      /* Still, we need to log the query ... */
      stmt_query.append(thd->query().str, thd->query().length);
      goto end;
    }
  }

  /*
    Check that the user has TRIGGER privilege on the subject table.
  */
  {
    bool err_status;
    TABLE_LIST **save_query_tables_own_last= thd->lex->query_tables_own_last;
    thd->lex->query_tables_own_last= 0;

    err_status= check_table_access(thd, TRIGGER_ACL, tables, FALSE, 1, FALSE);

    thd->lex->query_tables_own_last= save_query_tables_own_last;

    if (err_status)
      goto end;
  }

  /* We should have only one table in table list. */
  DBUG_ASSERT(tables->next_global == 0);

  /* We do not allow creation of triggers on temporary tables. */
  if (create && find_temporary_table(thd, tables))
  {
    my_error(ER_TRG_ON_VIEW_OR_TEMP_TABLE, MYF(0), tables->alias);
    goto end;
  }

  /* We also don't allow creation of triggers on views. */
  tables->required_type= FRMTYPE_TABLE;
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
    if (!(tables->table= find_table_for_mdl_upgrade(thd, tables->db,
                                                    tables->table_name,
                                                    FALSE)))
      goto end;
  }
  else
  {
    tables->table= open_n_lock_single_table(thd, tables,
                                            TL_READ_NO_INSERT, 0);
    if (! tables->table)
      goto end;
    tables->table->use_all_columns();
  }
  table= tables->table;
  table->pos_in_table_list= tables;

  /* Later on we will need it to downgrade the lock */
  mdl_ticket= table->mdl_ticket;

  if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
    goto end;

  lock_upgrade_done= TRUE;

  if (!table->triggers)
  {
    if (!create)
    {
      my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
      goto end;
    }

    if (!(table->triggers= Table_trigger_dispatcher::create(table)))
      goto end;
  }

  if (create)
  {
    result= table->triggers->create_trigger(thd, &stmt_query);
  }
  else
  {
    bool trigger_found;

    result= table->triggers->drop_trigger(thd,
                                          thd->lex->spname->m_name,
                                          &trigger_found);

    if (!result && trigger_found)
      result= stmt_query.append(thd->query().str, thd->query().length);
  }

  if (result)
    goto end;

  close_all_tables_for_name(thd, table->s, false, NULL);
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

end:
  if (!result)
  {
    if (tables)
      thd->add_to_binlog_accessed_dbs(tables->db);
    result= write_bin_log(thd, TRUE, stmt_query.ptr(), stmt_query.length());
  }

  /*
    If we are under LOCK TABLES we should restore original state of
    meta-data locks. Otherwise all locks will be released along
    with the implicit commit.
  */
  if (thd->locked_tables_mode && tables && lock_upgrade_done)
    mdl_ticket->downgrade_lock(MDL_SHARED_NO_READ_WRITE);

  /* Restore the query table list. Used only for drop trigger. */
  if (!create)
    thd->lex->restore_backup_query_tables_list(&backup);

  if (!result)
  {
#ifdef HAVE_PSI_SP_INTERFACE
    /* Drop statistics for this stored program from performance schema. */
    MYSQL_DROP_SP(SP_TYPE_TRIGGER,
                  thd->lex->spname->m_db.str, thd->lex->spname->m_db.length,
                  thd->lex->spname->m_name.str, thd->lex->spname->m_name.length);
#endif
    my_ok(thd);
  }

  DBUG_RETURN(result);
}



/**
  Find trigger's table from trigger identifier and add it to
  the statement table list.

  @param[in] thd       Thread context.
  @param[in] trg_name  Trigger name.
  @param[in] if_exists TRUE if SQL statement contains "IF EXISTS" clause.
                       That means a warning instead of error should be
                       thrown if trigger with given name does not exist.
  @param[out] table    Pointer to TABLE_LIST object for the
                       table trigger.

  @return Operation status
    @retval FALSE On success.
    @retval TRUE  Otherwise.
*/

bool add_table_for_trigger(THD *thd,
                           const LEX_CSTRING &db_name,
                           const LEX_STRING &trigger_name,
                           bool continue_if_not_exist,
                           TABLE_LIST **table)
{
  LEX *lex= thd->lex;
  char trn_path_buff[FN_REFLEN];
  LEX_STRING tbl_name= { NULL, 0 };

  DBUG_ENTER("add_table_for_trigger");

  LEX_STRING trn_path=
    Trigger_loader::build_trn_path(trn_path_buff, FN_REFLEN,
                                   db_name.str, trigger_name.str);

  if (Trigger_loader::check_trn_exists(trn_path))
  {
    if (continue_if_not_exist)
    {
      push_warning(thd, Sql_condition::SL_NOTE,
                   ER_TRG_DOES_NOT_EXIST, ER(ER_TRG_DOES_NOT_EXIST));

      *table= NULL;

      DBUG_RETURN(FALSE);
    }

    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (Trigger_loader::load_trn_file(thd, trigger_name, trn_path, &tbl_name))
    DBUG_RETURN(TRUE);

  *table= sp_add_to_query_tables(thd, lex, db_name.str, tbl_name.str);

  DBUG_RETURN(*table ? FALSE : TRUE);
}


/**
  Update .TRG and .TRN files after renaming triggers' subject table.

  @param[i]] thd            Thread context
  @param[in] db_name        Current database of subject table
  @param[in] table_alias    Current alias of subject table
  @param[in] table_name     Current name of subject table
  @param[in] new_db_name    New database for subject table
  @param[in] new_table_name New name of subject table

  @note
    This method tries to leave trigger related files in consistent state, i.e.
    it either will complete successfully, or will fail leaving files in their
    initial state.

  @note
    This method assumes that subject table is not renamed to itself.

  @note
    This method needs to be called under an exclusive table metadata lock.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool change_trigger_table_name(THD *thd,
                               const char *db_name,
                               const char *table_alias,
                               const char *table_name,
                               const char *new_db_name,
                               const char *new_table_name)
{
  // Check if there is at least one trigger for the given table.

  if (!Trigger_loader::trg_file_exists(db_name, table_name))
    return false;

  /*
    Since triggers should be in the same schema as their subject tables
    moving table with them between two schemas raises too many questions.
    (E.g. what should happen if in new schema we already have trigger
     with same name ?).

    In case of "ALTER DATABASE `#mysql50#db1` UPGRADE DATA DIRECTORY NAME"
    we will be given table name with "#mysql50#" prefix
    To remove this prefix we use check_n_cut_mysql50_prefix().
  */

  bool upgrading50to51= false;

  if (my_strcasecmp(table_alias_charset, db_name, new_db_name))
  {
    char dbname[NAME_LEN + 1];
    if (check_n_cut_mysql50_prefix(db_name, dbname, sizeof(dbname)) &&
        !my_strcasecmp(table_alias_charset, dbname, new_db_name))
    {
      upgrading50to51= true;
    }
    else
    {
      my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
      return true;
    }
  }

  /*
    This method interfaces the mysql server code protected by
    an exclusive metadata lock.
  */
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                                           db_name,
                                                           table_name,
                                                           MDL_EXCLUSIVE));

  DBUG_ASSERT(my_strcasecmp(table_alias_charset, db_name, new_db_name) ||
              my_strcasecmp(table_alias_charset, table_alias, new_table_name));

  Table_trigger_dispatcher d(db_name, table_name);

  return d.check_n_load(thd, true) ||
         d.check_for_broken_triggers() ||
         d.rename_subject_table(thd,
                                db_name, new_db_name,
                                table_alias,
                                new_table_name,
                                upgrading50to51);
}



/**
  Drop all triggers for table.

  @param thd        current thread context
  @param db_name    name of the table schema
  @param table_name table name

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/
bool drop_all_triggers(THD *thd, const char *db_name, const char *table_name)
{
  // Check if there is at least one trigger for the given table.

  if (!Trigger_loader::trg_file_exists(db_name, table_name))
    return false;

  /*
    Here we have to 1) load trigger definitions from TRG-files and 2) parse them
    to find out trigger names. Since trigger names are not stored in the
    TRG-file, it is impossible to avoid parsing just to delete triggers.
  */

  Table_trigger_dispatcher d(db_name, table_name);

  return
    d.check_n_load(thd, true) ||
    Trigger_loader::drop_all_triggers(db_name, table_name,
                                      &d.get_trigger_list());
}
