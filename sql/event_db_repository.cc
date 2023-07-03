<<<<<<< HEAD
/* Copyright (c) 2006, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
=======
/*
   Copyright (c) 2006, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/event_db_repository.h"

#include <vector>

#include "lex_string.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/dd/cache/dictionary_client.h"  // fetch_schema_components
#include "sql/dd/dd_event.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/event.h"
#include "sql/derror.h"
#include "sql/event_data_objects.h"
#include "sql/event_parse_data.h"
#include "sql/sp_head.h"
#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/thd_raii.h"
#include "sql/transaction.h"
#include "sql/tztime.h"  // struct Time_zone

/**
  @addtogroup Event_Scheduler
  @{
*/

/**
  Creates an event object and persist to Data Dictionary.

<<<<<<< HEAD
  @pre All semantic checks must be performed outside.
=======
  Used both when an event is created and when it is altered.

  @param   thd        THD
  @param   table      The row to fill out
  @param   et         Event's data
  @param   sp         Event stored routine
  @param   is_update  CREATE EVENT or ALTER EVENT

  @retval  FALSE success
  @retval  TRUE error
*/

static bool
mysql_event_fill_row(THD *thd,
                     TABLE *table,
                     Event_parse_data *et,
                     sp_head *sp,
                     sql_mode_t sql_mode,
                     my_bool is_update)
{
  CHARSET_INFO *scs= system_charset_info;
  enum enum_events_table_field f_num;
  Field **fields= table->field;
  int rs= FALSE;

  DBUG_ENTER("mysql_event_fill_row");

  DBUG_PRINT("info", ("dbname=[%s]", et->dbname.str));
  DBUG_PRINT("info", ("name  =[%s]", et->name.str));

  assert(et->on_completion != Event_parse_data::ON_COMPLETION_DEFAULT);

  if (table->s->fields < ET_FIELD_COUNT)
  {
    /*
      Safety: this can only happen if someone started the server
      and then altered mysql.event.
    */
    my_error(ER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2, MYF(0),
             table->s->db.str, table->s->table_name.str,
             (int) ET_FIELD_COUNT, table->s->fields);
    DBUG_RETURN(TRUE);
  }

  if (fields[f_num= ET_FIELD_DEFINER]->
                              store(et->definer.str, et->definer.length, scs))
  {
    if(fields[ET_FIELD_DEFINER]->field_length <
        (USERNAME_CHAR_LENGTH + HOSTNAME_LENGTH + 1) *
        table->field[3]->charset()->mbmaxlen)
    {
      my_error(ER_USER_COLUMN_OLD_LENGTH, MYF(0),
               fields[ET_FIELD_DEFINER]->field_name);
      DBUG_RETURN(TRUE);
    }
    goto err_truncate;
  }

  if (fields[f_num= ET_FIELD_DB]->store(et->dbname.str, et->dbname.length, scs))
    goto err_truncate;

  if (fields[f_num= ET_FIELD_NAME]->store(et->name.str, et->name.length, scs))
    goto err_truncate;

  /* ON_COMPLETION field is NOT NULL thus not calling set_notnull()*/
  rs|= fields[ET_FIELD_ON_COMPLETION]->store((longlong)et->on_completion, TRUE);

  /*
    Set STATUS value unconditionally in case of CREATE EVENT.
    For ALTER EVENT set it only if value of this field was changed.
    Since STATUS field is NOT NULL call to set_notnull() is not needed.
  */
  if (!is_update || et->status_changed)
    rs|= fields[ET_FIELD_STATUS]->store((longlong)et->status, TRUE);
  rs|= fields[ET_FIELD_ORIGINATOR]->store(et->originator, TRUE);

  /*
    Change the SQL_MODE only if body was present in an ALTER EVENT and of course
    always during CREATE EVENT.
  */
  if (et->body_changed)
  {
    assert(sp->m_body.str);

    rs|= fields[ET_FIELD_SQL_MODE]->store((longlong)sql_mode, TRUE);

    if (fields[f_num= ET_FIELD_BODY]->store(sp->m_body.str,
                                            sp->m_body.length,
                                            scs))
    {
      goto err_truncate;
    }
  }

  if (et->expression)
  {
    const String *tz_name= thd->variables.time_zone->get_name();
    if (!is_update || !et->starts_null)
    {
      fields[ET_FIELD_TIME_ZONE]->set_notnull();
      rs|= fields[ET_FIELD_TIME_ZONE]->store(tz_name->ptr(), tz_name->length(),
                                             tz_name->charset());
    }

    fields[ET_FIELD_INTERVAL_EXPR]->set_notnull();
    rs|= fields[ET_FIELD_INTERVAL_EXPR]->store(et->expression, TRUE);

    fields[ET_FIELD_TRANSIENT_INTERVAL]->set_notnull();

    rs|= fields[ET_FIELD_TRANSIENT_INTERVAL]->
                            store(interval_type_to_name[et->interval].str,
                                  interval_type_to_name[et->interval].length,
                                  scs);

    fields[ET_FIELD_EXECUTE_AT]->set_null();

    if (!et->starts_null)
    {
      MYSQL_TIME time;
      my_tz_OFFSET0->gmt_sec_to_TIME(&time, et->starts);

      fields[ET_FIELD_STARTS]->set_notnull();
      fields[ET_FIELD_STARTS]->store_time(&time);
    }

    if (!et->ends_null)
    {
      MYSQL_TIME time;
      my_tz_OFFSET0->gmt_sec_to_TIME(&time, et->ends);

      fields[ET_FIELD_ENDS]->set_notnull();
      fields[ET_FIELD_ENDS]->store_time(&time);
    }
  }
  else if (et->execute_at)
  {
    const String *tz_name= thd->variables.time_zone->get_name();
    fields[ET_FIELD_TIME_ZONE]->set_notnull();
    rs|= fields[ET_FIELD_TIME_ZONE]->store(tz_name->ptr(), tz_name->length(),
                                           tz_name->charset());

    fields[ET_FIELD_INTERVAL_EXPR]->set_null();
    fields[ET_FIELD_TRANSIENT_INTERVAL]->set_null();
    fields[ET_FIELD_STARTS]->set_null();
    fields[ET_FIELD_ENDS]->set_null();

    MYSQL_TIME time;
    my_tz_OFFSET0->gmt_sec_to_TIME(&time, et->execute_at);

    fields[ET_FIELD_EXECUTE_AT]->set_notnull();
    fields[ET_FIELD_EXECUTE_AT]->store_time(&time);
  }
  else
  {
    assert(is_update);
    /*
      it is normal to be here when the action is update
      this is an error if the action is create. something is borked
    */
  }

  Item_func_now_local::store_in(fields[ET_FIELD_MODIFIED]);

  if (et->comment.str)
  {
    if (fields[f_num= ET_FIELD_COMMENT]->
                          store(et->comment.str, et->comment.length, scs))
      goto err_truncate;
  }

  fields[ET_FIELD_CHARACTER_SET_CLIENT]->set_notnull();
  rs|= fields[ET_FIELD_CHARACTER_SET_CLIENT]->store(
    thd->variables.character_set_client->csname,
    strlen(thd->variables.character_set_client->csname),
    system_charset_info);

  fields[ET_FIELD_COLLATION_CONNECTION]->set_notnull();
  rs|= fields[ET_FIELD_COLLATION_CONNECTION]->store(
    thd->variables.collation_connection->name,
    strlen(thd->variables.collation_connection->name),
    system_charset_info);

  {
    const CHARSET_INFO *db_cl= get_default_db_collation(thd, et->dbname.str);

    fields[ET_FIELD_DB_COLLATION]->set_notnull();
    rs|= fields[ET_FIELD_DB_COLLATION]->store(db_cl->name,
                                              strlen(db_cl->name),
                                              system_charset_info);
  }

  if (et->body_changed)
  {
    fields[ET_FIELD_BODY_UTF8]->set_notnull();
    rs|= fields[ET_FIELD_BODY_UTF8]->store(sp->m_body_utf8.str,
                                           sp->m_body_utf8.length,
                                           system_charset_info);
  }

  if (rs)
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), fields[f_num]->field_name, rs);
    DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);

err_truncate:
  my_error(ER_EVENT_DATA_TOO_LONG, MYF(0), fields[f_num]->field_name);
  DBUG_RETURN(TRUE);
}


/*
  Performs an index scan of event_table (mysql.event) and fills schema_table.

  SYNOPSIS
    Event_db_repository::index_read_for_db_for_i_s()
      thd          Thread
      schema_table The I_S.EVENTS table
      event_table  The event table to use for loading (mysql.event)
      db           For which schema to do an index scan.

  RETURN VALUE
    0  OK
    1  Error
*/

bool
Event_db_repository::index_read_for_db_for_i_s(THD *thd, TABLE *schema_table,
                                               TABLE *event_table,
                                               const char *db)
{
  CHARSET_INFO *scs= system_charset_info;
  KEY *key_info;
  uint key_len;
  uchar *key_buf= NULL;

  DBUG_ENTER("Event_db_repository::index_read_for_db_for_i_s");

  DBUG_PRINT("info", ("Using prefix scanning on PK"));

  int ret= event_table->file->ha_index_init(0, 1);
  if (ret)
  {
    event_table->file->print_error(ret, MYF(0));
    DBUG_RETURN(true);
  }

  key_info= event_table->key_info;

  if (key_info->user_defined_key_parts == 0 ||
      key_info->key_part[0].field != event_table->field[ET_FIELD_DB])
  {
    /* Corrupted table: no index or index on a wrong column */
    my_error(ER_CANNOT_LOAD_FROM_TABLE_V2, MYF(0), "mysql", "event");
    ret= 1;
    goto end;
  }

  event_table->field[ET_FIELD_DB]->store(db, strlen(db), scs);
  key_len= key_info->key_part[0].store_length;

  if (!(key_buf= (uchar *)alloc_root(thd->mem_root, key_len)))
  {
    /* Don't send error, it would be done by sql_alloc_error_handler() */
    ret= 1;
    goto end;
  }

  key_copy(key_buf, event_table->record[0], key_info, key_len);
  if (!(ret= event_table->file->ha_index_read_map(event_table->record[0], key_buf,
                                                  (key_part_map)1,
                                                  HA_READ_KEY_EXACT)))
  {
    DBUG_PRINT("info",("Found rows. Let's retrieve them. ret=%d", ret));
    do
    {
      ret= copy_event_to_schema_table(thd, schema_table, event_table);
      if (ret == 0)
        ret= event_table->file->ha_index_next_same(event_table->record[0],
                                                   key_buf, key_len);
    } while (ret == 0);
  }
  DBUG_PRINT("info", ("Scan finished. ret=%d", ret));

  /*  ret is guaranteed to be != 0 */
  if (ret == HA_ERR_END_OF_FILE || ret == HA_ERR_KEY_NOT_FOUND)
    ret= 0;
  else
    event_table->file->print_error(ret, MYF(0));

end:
  event_table->file->ha_index_end();

  DBUG_RETURN(MY_TEST(ret));
}


/*
  Performs a table scan of event_table (mysql.event) and fills schema_table.

  SYNOPSIS
    Events_db_repository::table_scan_all_for_i_s()
      thd          Thread
      schema_table The I_S.EVENTS in memory table
      event_table  The event table to use for loading.

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Event_db_repository::table_scan_all_for_i_s(THD *thd, TABLE *schema_table,
                                            TABLE *event_table)
{
  int ret;
  READ_RECORD read_record_info;
  DBUG_ENTER("Event_db_repository::table_scan_all_for_i_s");

  if (init_read_record(&read_record_info, thd, event_table, NULL, 1, 1, FALSE))
    DBUG_RETURN(TRUE);

  /*
    rr_sequential, in read_record(), returns 137==HA_ERR_END_OF_FILE,
    but rr_handle_error returns -1 for that reason. Thus, read_record()
    returns -1 eventually.
  */
  do
  {
    ret= read_record_info.read_record(&read_record_info);
    if (ret == 0)
      ret= copy_event_to_schema_table(thd, schema_table, event_table);
  } while (ret == 0);

  DBUG_PRINT("info", ("Scan finished. ret=%d", ret));
  end_read_record(&read_record_info);

  /*  ret is guaranteed to be != 0 */
  DBUG_RETURN(ret == -1 ? FALSE : TRUE);
}


/**
  Fills I_S.EVENTS with data loaded from mysql.event. Also used by
  SHOW EVENTS

  The reason we reset and backup open tables here is that this
  function may be called from any query that accesses
  INFORMATION_SCHEMA - including a query that is issued from
  a pre-locked statement, one that already has open and locked
  tables.

  @retval FALSE  success
  @retval TRUE   error
*/

bool
Event_db_repository::fill_schema_events(THD *thd, TABLE_LIST *i_s_table,
                                        const char *db)
{
  TABLE *schema_table= i_s_table->table;
  Open_tables_backup open_tables_backup;
  TABLE_LIST event_table;
  int ret= 0;

  DBUG_ENTER("Event_db_repository::fill_schema_events");
  DBUG_PRINT("info",("db=%s", db? db:"(null)"));

  event_table.init_one_table("mysql", 5, "event", 5, "event", TL_READ);

  if (open_nontrans_system_tables_for_read(thd, &event_table,
                                           &open_tables_backup))
  {
    DBUG_RETURN(TRUE);
  }

  if (table_intact.check_event_table(event_table.table))
  {
    close_nontrans_system_tables(thd, &open_tables_backup);
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    1. SELECT I_S => use table scan. I_S.EVENTS does not guarantee order
                     thus we won't order it. OTOH, SHOW EVENTS will be
                     ordered.
    2. SHOW EVENTS => PRIMARY KEY with prefix scanning on (db)
       Reasoning: Events are per schema, therefore a scan over an index
                  will save use from doing a table scan and comparing
                  every single row's `db` with the schema which we show.
  */
  if (db)
    ret= index_read_for_db_for_i_s(thd, schema_table, event_table.table, db);
  else
    ret= table_scan_all_for_i_s(thd, schema_table, event_table.table);

  close_nontrans_system_tables(thd, &open_tables_backup);

  DBUG_PRINT("info", ("Return code=%d", ret));
  DBUG_RETURN(ret);
}


/**
  Open mysql.event table for read.

  It's assumed that the caller knows what they are doing:
  - whether it was necessary to reset-and-backup the open tables state
  - whether the requested lock does not lead to a deadlock
  - whether this open mode would work under LOCK TABLES, or inside a
  stored function or trigger.

  Note that if the table can't be locked successfully this operation will
  close it. Therefore it provides guarantee that it either opens and locks
  table or fails without leaving any tables open.

  @param[in]  thd  Thread context
  @param[in]  lock_type  How to lock the table
  @param[out] table  We will store the open table here

  @retval TRUE open and lock failed - an error message is pushed into the
               stack
  @retval FALSE success
*/

bool
Event_db_repository::open_event_table(THD *thd, enum thr_lock_type lock_type,
                                      TABLE **table)
{
  TABLE_LIST tables;
  DBUG_ENTER("Event_db_repository::open_event_table");

  tables.init_one_table("mysql", 5, "event", 5, "event", lock_type);

  if (open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
    DBUG_RETURN(TRUE);

  *table= tables.table;
  tables.table->use_all_columns();

  if (table_intact.check_event_table(*table))
  {
    close_thread_tables(thd);
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/**
  Creates an event record in mysql.event table.

  Creates an event. Relies on mysql_event_fill_row which is shared with
  ::update_event.

  @pre All semantic checks must be performed outside. This function
  only creates a record on disk.
  @pre The thread handle has no open tables.
>>>>>>> upstream/cluster-7.6

  @param[in,out] thd                   THD
  @param[in]     parse_data            Parsed event definition
  @param[in]     create_if_not         true if IF NOT EXISTS clause was provided
                                       to CREATE EVENT statement
  @param[out]    event_already_exists  When method is completed successfully
                                       set to true if event already exists else
                                       set to false
  @retval false  Success
  @retval true   Error
*/

bool Event_db_repository::create_event(THD *thd, Event_parse_data *parse_data,
                                       bool create_if_not,
                                       bool *event_already_exists) {
<<<<<<< HEAD
  DBUG_TRACE;
  sp_head *sp = thd->lex->sphead;
  assert(sp);
=======
  DBUG_ENTER("Event_db_repository::create_event");
<<<<<<< HEAD
  sp_head *sp = thd->lex->sphead;
  DBUG_ASSERT(sp);
=======

  DBUG_PRINT("info", ("open mysql.event for update"));
  assert(sp);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

  const dd::Schema *schema = nullptr;
  const dd::Event *event = nullptr;
  if (thd->dd_client()->acquire(parse_data->dbname.str, &schema) ||
      thd->dd_client()->acquire(parse_data->dbname.str, parse_data->name.str,
                                &event))
    return true;

  if (schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), parse_data->dbname.str);
    return true;
  }

  *event_already_exists = (event != nullptr);

  if (*event_already_exists) {
    if (create_if_not) {
      push_warning_printf(thd, Sql_condition::SL_NOTE, ER_EVENT_ALREADY_EXISTS,
                          ER_THD(thd, ER_EVENT_ALREADY_EXISTS),
                          parse_data->name.str);
      return false;
    }
    my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), parse_data->name.str);
    return true;
  }

  return dd::create_event(thd, *schema, parse_data->name.str, sp->m_body.str,
                          sp->m_body_utf8.str, thd->lex->definer, parse_data);
}

/**
  Used to execute ALTER EVENT. Pendant to Events::update_event().

  @param[in]      thd         THD context
  @param[in]      parse_data  parsed event definition
  @param[in]      new_dbname  not NULL if ALTER EVENT RENAME
                              points at a new database name
  @param[in]      new_name    not NULL if ALTER EVENT RENAME
                              points at a new event name

  @pre All semantic checks are performed outside this function.

  @retval false Success
  @retval true Error (reported)
*/

bool Event_db_repository::update_event(THD *thd, Event_parse_data *parse_data,
                                       const LEX_CSTRING *new_dbname,
                                       const LEX_CSTRING *new_name) {
  DBUG_TRACE;
  sp_head *sp = thd->lex->sphead;

  /* None or both must be set */
  assert((new_dbname && new_name) || new_dbname == new_name);

  DBUG_PRINT("info", ("dbname: %s", parse_data->dbname.str));
  DBUG_PRINT("info", ("name: %s", parse_data->name.str));
  DBUG_PRINT("info", ("user: %s", parse_data->definer.str));

  /* first look whether we overwrite */
  if (new_name) {
    DBUG_PRINT("info", ("rename to: %s@%s", new_dbname->str, new_name->str));

    const dd::Event *new_event = nullptr;
    if (thd->dd_client()->acquire(new_dbname->str, new_name->str, &new_event))
      return true;

    if (new_event != nullptr) {
      my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), new_name->str);
      return true;
    }
  }

  const dd::Schema *new_schema = nullptr;
  if (new_dbname != nullptr) {
    if (thd->dd_client()->acquire(new_dbname->str, &new_schema)) return true;

    if (new_schema == nullptr) {
      my_error(ER_BAD_DB_ERROR, MYF(0), new_dbname->str);
      return true;
    }
  }

  const dd::Schema *schema = nullptr;
  dd::Event *event = nullptr;
  if (thd->dd_client()->acquire(parse_data->dbname.str, &schema) ||
      thd->dd_client()->acquire_for_modification(parse_data->dbname.str,
                                                 parse_data->name.str, &event))
    return true;

  if (event == nullptr) {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), parse_data->name.str);
    return true;
  }
  assert(schema != nullptr);  // Must exist if event exists.

  /*
    If definer has the SYSTEM_USER privilege then invoker can alter event
    only if latter also has same privilege.
  */
  Security_context *sctx = thd->security_context();
  Auth_id definer(event->definer_user().c_str(), event->definer_host().c_str());
  if (sctx->can_operate_with(definer, consts::system_user, true)) return true;

  // Update Event in the data dictionary with altered event object attributes.
  bool ret = dd::update_event(
      thd, event, *schema, new_schema, new_name != nullptr ? new_name->str : "",
      (parse_data->body_changed) ? sp->m_body.str : event->definition(),
      (parse_data->body_changed) ? sp->m_body_utf8.str
                                 : event->definition_utf8(),
      thd->lex->definer, parse_data);
  return ret;
}

/**
  Delete event.

  @param[in]     thd            THD context
  @param[in]     db             Database name
  @param[in]     name           Event name
  @param[in]     drop_if_exists DROP IF EXISTS clause was specified.
                                If set, and the event does not exist,
                                the error is downgraded to a warning.
  @param[out]   event_exists    Set to true if event exists. Set to
                                false otherwise.

  @retval false success
  @retval true error (reported)
*/

bool Event_db_repository::drop_event(THD *thd, LEX_CSTRING db, LEX_CSTRING name,
                                     bool drop_if_exists, bool *event_exists) {
  DBUG_TRACE;
  /*
    Turn off row binlogging of this statement and use statement-based
    so that all supporting tables are updated for CREATE EVENT command.
    When we are going out of the function scope, the original binary
    format state will be restored.
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  DBUG_PRINT("enter", ("%s@%s", db.str, name.str));

  const dd::Event *event_ptr = nullptr;
  if (thd->dd_client()->acquire(db.str, name.str, &event_ptr)) {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  if (event_ptr == nullptr) {
    *event_exists = false;

    // Event not found
    if (!drop_if_exists) {
      my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
      return true;
    }

    push_warning_printf(thd, Sql_condition::SL_NOTE, ER_SP_DOES_NOT_EXIST,
                        ER_THD(thd, ER_SP_DOES_NOT_EXIST), "Event", name.str);
    return false;
  }
  /*
    If definer has the SYSTEM_USER privilege then invoker can drop event
    only if latter also has same privilege.
  */
  Auth_id definer(event_ptr->definer_user().c_str(),
                  event_ptr->definer_host().c_str());
  Security_context *sctx = thd->security_context();
  if (sctx->can_operate_with(definer, consts::system_user, true)) return true;

<<<<<<< HEAD
  *event_exists = true;
<<<<<<< HEAD
  return thd->dd_client()->drop(event_ptr);
=======
  DBUG_RETURN(thd->dd_client()->drop(event_ptr));
=======
  push_warning_printf(thd, Sql_condition::SL_NOTE,
                      ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
                      "Event", name.str);
  ret= 0;

end:
  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  /* Restore the state of binlog format */
  assert(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  DBUG_RETURN(MY_TEST(ret));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
}

/**
  Drops all events in the selected database.

  @param      thd     THD context
  @param      schema  The database under which events are to be dropped.

  @returns true on error, false on success.
*/

bool Event_db_repository::drop_schema_events(THD *thd,
                                             const dd::Schema &schema) {
  DBUG_TRACE;

  std::vector<dd::String_type> event_names;
  if (thd->dd_client()->fetch_schema_component_names<dd::Event>(&schema,
                                                                &event_names))
    return true;

  for (const dd::String_type &name : event_names) {
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Event *event_obj = nullptr;

    if (thd->dd_client()->acquire(schema.name(), name, &event_obj)) return true;

    if (event_obj == nullptr || thd->dd_client()->drop(event_obj)) {
      assert(event_obj != nullptr);
      my_error(ER_SP_DROP_FAILED, MYF(0), "Drop failed for Event: %s",
               event_obj->name().c_str());
      return true;
    }
  }

  return false;
}

/**
  Looks for a named event in the Data Dictionary and load it.

  @pre The given thread does not have open tables.

  @retval false  success
  @retval true   error
*/

bool Event_db_repository::load_named_event(THD *thd, LEX_CSTRING dbname,
                                           LEX_CSTRING name, Event_basic *etn) {
  const dd::Event *event_obj = nullptr;

  DBUG_TRACE;
  DBUG_PRINT("enter", ("thd: %p  name: %*s", thd, (int)name.length, name.str));

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  if (thd->dd_client()->acquire(dbname.str, name.str, &event_obj)) {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  if (event_obj == nullptr) {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
    return true;
  }

  if (etn->fill_event_info(thd, *event_obj, dbname.str)) {
    my_error(ER_CANNOT_LOAD_FROM_TABLE_V2, MYF(0), "mysql", "events");
    return true;
  }

  return false;
}

/**
   Update the event in Data Dictionary with changed status
   and/or last execution time.
*/

bool Event_db_repository::update_timing_fields_for_event(
    THD *thd, LEX_CSTRING event_db_name, LEX_CSTRING event_name,
    my_time_t last_executed, ulonglong status) {
  DBUG_TRACE;
  // Turn off autocommit.
  Disable_autocommit_guard autocommit_guard(thd);

  /*
    Turn off row binlogging of this statement and use statement-based
    so that all supporting tables are updated for CREATE EVENT command.
    When we are going out of the function scope, the original binary
    format state will be restored.
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  assert(thd->security_context()->check_access(SUPER_ACL));

  dd::Event *event = nullptr;
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  if (thd->dd_client()->acquire_for_modification(event_db_name.str,
                                                 event_name.str, &event))
    return true;
  if (event == nullptr) return true;

  if (dd::update_event_time_and_status(thd, event, last_executed, status)) {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

<<<<<<< HEAD
  return trans_commit_stmt(thd) || trans_commit(thd);
=======
<<<<<<< HEAD
  DBUG_RETURN(trans_commit_stmt(thd) || trans_commit(thd));
=======
  ret= 0;

end:
  if (table)
    close_mysql_tables(thd);

  /* Restore the state of binlog format */
  assert(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  DBUG_RETURN(MY_TEST(ret));
}


/**
  Open mysql.db, mysql.user and mysql.event and check whether:
    - mysql.db exists and is up to date (or from a newer version of MySQL),
    - mysql.user has column Event_priv at an expected position,
    - mysql.event exists and is up to date (or from a newer version of
      MySQL)

  This function is called only when the server is started.
  @pre The passed in thread handle has no open tables.

  @retval FALSE  OK
  @retval TRUE   Error, an error message is output to the error log.
*/

bool
Event_db_repository::check_system_tables(THD *thd)
{
  TABLE_LIST tables;
  int ret= FALSE;
  const unsigned int event_priv_column_position= 28;

  DBUG_ENTER("Event_db_repository::check_system_tables");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));

  /* Check mysql.db */
  tables.init_one_table("mysql", 5, "db", 2, "db", TL_READ);

  if (open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    ret= 1;
    sql_print_error("Cannot open mysql.db");
  }
  else
  {
    if (table_intact.check(tables.table, &mysql_db_table_def))
      ret= 1;
    close_acl_tables(thd);
  }
  /* Check mysql.user */
  tables.init_one_table("mysql", 5, "user", 4, "user", TL_READ);

  if (open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    ret= 1;
    sql_print_error("Cannot open mysql.user");
  }
  else
  {
    if (tables.table->s->fields < event_priv_column_position ||
        strncmp(tables.table->field[event_priv_column_position]->field_name,
                STRING_WITH_LEN("Event_priv")))
    {
      sql_print_error("mysql.user has no `Event_priv` column at position %d",
                      event_priv_column_position);
      ret= 1;
    }
    close_acl_tables(thd);
  }
  /* Check mysql.event */
  tables.init_one_table("mysql", 5, "event", 5, "event", TL_READ);

  if (open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    ret= 1;
    sql_print_error("Cannot open mysql.event");
  }
  else
  {
    if(table_intact.check_event_table(tables.table))
      ret= 1;
    close_mysql_tables(thd);
  }

  DBUG_RETURN(MY_TEST(ret));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
}

/**
  @} (End of group Event_Scheduler)
*/
// XXX:
