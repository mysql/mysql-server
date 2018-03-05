/*
   Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_base.h"                           // close_thread_tables
#include "event_db_repository.h"
#include "key.h"                                // key_copy
#include "sql_db.h"                        // get_default_db_collation
#include "sql_time.h"                      // interval_type_to_name
#include "tztime.h"                             // struct Time_zone
#include "sql_acl.h" // SUPER_ACL, MYSQL_DB_FIELD_COUNT, mysql_db_table_fields
#include "records.h"          // init_read_record, end_read_record
#include "sp_head.h"
#include "event_data_objects.h"
#include "events.h"
#include "sql_show.h"
#include "lock.h"                               // MYSQL_LOCK_IGNORE_TIMEOUT

/**
  @addtogroup Event_Scheduler
  @{
*/

static
const TABLE_FIELD_TYPE event_table_fields[ET_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body") },
    { C_STRING_WITH_LEN("longblob") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("definer") },
    { C_STRING_WITH_LEN("char(77)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("execute_at") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("interval_value") },
    { C_STRING_WITH_LEN("int(11)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("interval_field") },
    { C_STRING_WITH_LEN("enum('YEAR','QUARTER','MONTH','DAY',"
    "'HOUR','MINUTE','WEEK','SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR',"
    "'DAY_MINUTE','DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND',"
    "'DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND',"
    "'SECOND_MICROSECOND')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("created") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("modified") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("last_executed") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("starts") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ends") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("status") },
    { C_STRING_WITH_LEN("enum('ENABLED','DISABLED','SLAVESIDE_DISABLED')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("on_completion") },
    { C_STRING_WITH_LEN("enum('DROP','PRESERVE')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("sql_mode") },
    { C_STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("originator") },
    { C_STRING_WITH_LEN("int(10)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("time_zone") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("latin1") }
  },
  {
    { C_STRING_WITH_LEN("character_set_client") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("collation_connection") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("db_collation") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body_utf8") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  event_table_def= {ET_FIELD_COUNT, event_table_fields};

class Event_db_intact : public Table_check_intact
{
protected:
  void report_error(uint, const char *fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    error_log_print(ERROR_LEVEL, fmt, args);
    va_end(args);
  }
public:
  Event_db_intact() { has_keys= TRUE; }
};

/** In case of an error, a message is printed to the error log. */
static Event_db_intact table_intact;


/**
  Puts some data common to CREATE and ALTER EVENT into a row.

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

  DBUG_ASSERT(et->on_completion != Event_parse_data::ON_COMPLETION_DEFAULT);

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
    goto err_truncate;

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
  rs|= fields[ET_FIELD_ORIGINATOR]->store((longlong)et->originator, TRUE);

  /*
    Change the SQL_MODE only if body was present in an ALTER EVENT and of course
    always during CREATE EVENT.
  */
  if (et->body_changed)
  {
    DBUG_ASSERT(sp->m_body.str);

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
    rs|= fields[ET_FIELD_INTERVAL_EXPR]->store((longlong)et->expression, TRUE);

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
    DBUG_ASSERT(is_update);
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
  LINT_INIT(key_buf);

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

  if (open_system_tables_for_read(thd, &event_table, &open_tables_backup))
    DBUG_RETURN(TRUE);

  if (table_intact.check(event_table.table, &event_table_def))
  {
    close_system_tables(thd, &open_tables_backup);
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

  close_system_tables(thd, &open_tables_backup);

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

  if (open_and_lock_tables(thd, &tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
    DBUG_RETURN(TRUE);

  *table= tables.table;
  tables.table->use_all_columns();

  if (table_intact.check(*table, &event_table_def))
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

  @param[in,out] thd                   THD
  @param[in]     parse_data            Parsed event definition
  @param[in]     create_if_not         TRUE if IF NOT EXISTS clause was provided
                                       to CREATE EVENT statement
  @param[out]    event_already_exists  When method is completed successfully
                                       set to true if event already exists else
                                       set to false
  @retval FALSE  success
  @retval TRUE   error
*/

bool
Event_db_repository::create_event(THD *thd, Event_parse_data *parse_data,
                                  bool create_if_not,
                                  bool *event_already_exists)
{
  int ret= 1;
  TABLE *table= NULL;
  sp_head *sp= thd->lex->sphead;
  sql_mode_t saved_mode= thd->variables.sql_mode;
  /*
    Take a savepoint to release only the lock on mysql.event
    table at the end but keep the global read lock and
    possible other locks taken by the caller.
  */
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();

  DBUG_ENTER("Event_db_repository::create_event");

  DBUG_PRINT("info", ("open mysql.event for update"));
  DBUG_ASSERT(sp);

  /* Reset sql_mode during data dictionary operations. */
  thd->variables.sql_mode= 0;

  if (open_event_table(thd, TL_WRITE, &table))
    goto end;

  DBUG_PRINT("info", ("name: %.*s", (int) parse_data->name.length,
             parse_data->name.str));

  DBUG_PRINT("info", ("check existance of an event with the same name"));
  if (!find_named_event(parse_data->dbname, parse_data->name, table))
  {
    if (create_if_not)
    {
      *event_already_exists= true;
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_NOTE,
                          ER_EVENT_ALREADY_EXISTS, ER(ER_EVENT_ALREADY_EXISTS),
                          parse_data->name.str);
      ret= 0;
    }
    else
      my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), parse_data->name.str);

    goto end;
  } else
    *event_already_exists= false;

  DBUG_PRINT("info", ("non-existent, go forward"));

  restore_record(table, s->default_values);     // Get default values for fields

  if (system_charset_info->cset->
        numchars(system_charset_info, parse_data->dbname.str,
                 parse_data->dbname.str + parse_data->dbname.length) >
      table->field[ET_FIELD_DB]->char_length())
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), parse_data->dbname.str);
    goto end;
  }

  if (system_charset_info->cset->
        numchars(system_charset_info, parse_data->name.str,
                 parse_data->name.str + parse_data->name.length) >
      table->field[ET_FIELD_NAME]->char_length())
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), parse_data->name.str);
    goto end;
  }

  if (sp->m_body.length > table->field[ET_FIELD_BODY]->field_length)
  {
    my_error(ER_TOO_LONG_BODY, MYF(0), parse_data->name.str);
    goto end;
  }

  Item_func_now_local::store_in(table->field[ET_FIELD_CREATED]);

  /*
    mysql_event_fill_row() calls my_error() in case of error so no need to
    handle it here
  */
  if (mysql_event_fill_row(thd, table, parse_data, sp, saved_mode, FALSE))
    goto end;

  if ((ret= table->file->ha_write_row(table->record[0])))
  {
    table->file->print_error(ret, MYF(0));
    goto end;
  }
  ret= 0;

end:
  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  thd->variables.sql_mode= saved_mode;
  DBUG_RETURN(MY_TEST(ret));
}


/**
  Used to execute ALTER EVENT. Pendant to Events::update_event().

  @param[in,out]  thd         thread handle
  @param[in]      parse_data  parsed event definition
  @param[in]      new_dbname  not NULL if ALTER EVENT RENAME
                              points at a new database name
  @param[in]      new_name    not NULL if ALTER EVENT RENAME
                              points at a new event name

  @pre All semantic checks are performed outside this function,
  it only updates the event definition on disk.
  @pre We don't have any tables open in the given thread.

  @retval FALSE success
  @retval TRUE error (reported)
*/

bool
Event_db_repository::update_event(THD *thd, Event_parse_data *parse_data,
                                  LEX_STRING *new_dbname,
                                  LEX_STRING *new_name)
{
  CHARSET_INFO *scs= system_charset_info;
  TABLE *table= NULL;
  sp_head *sp= thd->lex->sphead;
  sql_mode_t saved_mode= thd->variables.sql_mode;
  /*
    Take a savepoint to release only the lock on mysql.event
    table at the end but keep the global read lock and
    possible other locks taken by the caller.
  */
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();
  int ret= 1;

  DBUG_ENTER("Event_db_repository::update_event");

  /* None or both must be set */
  DBUG_ASSERT((new_dbname && new_name) || new_dbname == new_name);

  /* Reset sql_mode during data dictionary operations. */
  thd->variables.sql_mode= 0;

  if (open_event_table(thd, TL_WRITE, &table))
    goto end;

  DBUG_PRINT("info", ("dbname: %s", parse_data->dbname.str));
  DBUG_PRINT("info", ("name: %s", parse_data->name.str));
  DBUG_PRINT("info", ("user: %s", parse_data->definer.str));

  /* first look whether we overwrite */
  if (new_name)
  {
    DBUG_PRINT("info", ("rename to: %s@%s", new_dbname->str, new_name->str));
    if (!find_named_event(*new_dbname, *new_name, table))
    {
      my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), new_name->str);
      goto end;
    }
  }
  /*
    ...and then if there is such an event. Don't exchange the blocks
    because you will get error 120 from table handler because new_name will
    overwrite the key and SE will tell us that it cannot find the already found
    row (copied into record[1] later
  */
  if (find_named_event(parse_data->dbname, parse_data->name, table))
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), parse_data->name.str);
    goto end;
  }

  store_record(table,record[1]);

  /*
    We check whether ALTER EVENT was given dates that are in the past.
    However to know how to react, we need the ON COMPLETION type. The
    check is deferred to this point because by now we have the previous
    setting (from the event-table) to fall back on if nothing was specified
    in the ALTER EVENT-statement.
  */

  if (parse_data->check_dates(thd,
                              (int) table->field[ET_FIELD_ON_COMPLETION]->val_int()))
    goto end;

  /*
    mysql_event_fill_row() calls my_error() in case of error so no need to
    handle it here
  */
  if (mysql_event_fill_row(thd, table, parse_data, sp, saved_mode, TRUE))
    goto end;

  if (new_dbname)
  {
    table->field[ET_FIELD_DB]->store(new_dbname->str, new_dbname->length, scs);
    table->field[ET_FIELD_NAME]->store(new_name->str, new_name->length, scs);
  }

  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
  {
    table->file->print_error(ret, MYF(0));
    goto end;
  }
  ret= 0;

end:
  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  thd->variables.sql_mode= saved_mode;
  DBUG_RETURN(MY_TEST(ret));
}


/**
  Delete event record from mysql.event table.

  @param[in,out] thd            thread handle
  @param[in]     db             Database name
  @param[in]     name           Event name
  @param[in]     drop_if_exists DROP IF EXISTS clause was specified.
                                If set, and the event does not exist,
                                the error is downgraded to a warning.

  @retval FALSE success
  @retval TRUE error (reported)
*/

bool
Event_db_repository::drop_event(THD *thd, LEX_STRING db, LEX_STRING name,
                                bool drop_if_exists)
{
  TABLE *table= NULL;
  /*
    Take a savepoint to release only the lock on mysql.event
    table at the end but keep the global read lock and
    possible other locks taken by the caller.
  */
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();
  int ret= 1;

  DBUG_ENTER("Event_db_repository::drop_event");
  DBUG_PRINT("enter", ("%s@%s", db.str, name.str));

  if (open_event_table(thd, TL_WRITE, &table))
    goto end;

  if (!find_named_event(db, name, table))
  {
    if ((ret= table->file->ha_delete_row(table->record[0])))
      table->file->print_error(ret, MYF(0));
    goto end;
  }

  /* Event not found */
  if (!drop_if_exists)
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
    goto end;
  }

  push_warning_printf(thd, Sql_condition::WARN_LEVEL_NOTE,
                      ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
                      "Event", name.str);
  ret= 0;

end:
  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  DBUG_RETURN(MY_TEST(ret));
}


/**
  Positions the internal pointer of `table` to the place where (db, name)
  is stored.

  In case search succeeded, the table cursor points at the found row.

  @param[in]      db     database name
  @param[in]      name   event name
  @param[in,out]  table  mysql.event table


  @retval FALSE  an event with such db/name key exists
  @retval  TRUE   no record found or an error occured.
*/

bool
Event_db_repository::find_named_event(LEX_STRING db, LEX_STRING name,
                                      TABLE *table)
{
  uchar key[MAX_KEY_LENGTH];
  DBUG_ENTER("Event_db_repository::find_named_event");
  DBUG_PRINT("enter", ("name: %.*s", (int) name.length, name.str));

  /*
    Create key to find row. We have to use field->store() to be able to
    handle VARCHAR and CHAR fields.
    Assumption here is that the two first fields in the table are
    'db' and 'name' and the first key is the primary key over the
    same fields.
  */
  if (db.length > table->field[ET_FIELD_DB]->field_length ||
      name.length > table->field[ET_FIELD_NAME]->field_length)
    DBUG_RETURN(TRUE);
  
  table->field[ET_FIELD_DB]->store(db.str, db.length, &my_charset_bin);
  table->field[ET_FIELD_NAME]->store(name.str, name.length, &my_charset_bin);

  key_copy(key, table->record[0], table->key_info, table->key_info->key_length);

  if (table->file->ha_index_read_idx_map(table->record[0], 0, key, HA_WHOLE_KEY,
                                         HA_READ_KEY_EXACT))
  {
    DBUG_PRINT("info", ("Row not found"));
    DBUG_RETURN(TRUE);
  }

  DBUG_PRINT("info", ("Row found!"));
  DBUG_RETURN(FALSE);
}


/*
  Drops all events in the selected database, from mysql.event.

  SYNOPSIS
    Event_db_repository::drop_schema_events()
      thd     Thread
      schema  The database to clean from events
*/

void
Event_db_repository::drop_schema_events(THD *thd, LEX_STRING schema)
{
  int ret= 0;
  TABLE *table= NULL;
  READ_RECORD read_record_info;
  enum enum_events_table_field field= ET_FIELD_DB;
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();
  DBUG_ENTER("Event_db_repository::drop_schema_events");
  DBUG_PRINT("enter", ("field=%d schema=%s", field, schema.str));

  if (open_event_table(thd, TL_WRITE, &table))
    DBUG_VOID_RETURN;

  /* only enabled events are in memory, so we go now and delete the rest */
  if (init_read_record(&read_record_info, thd, table, NULL, 1, 1, FALSE))
    DBUG_VOID_RETURN;
  while (!ret && !(read_record_info.read_record(&read_record_info)) )
  {
    char *et_field= get_field(thd->mem_root, table->field[field]);

    /* et_field may be NULL if the table is corrupted or out of memory */
    if (et_field)
    {
      LEX_STRING et_field_lex= { et_field, strlen(et_field) };
      DBUG_PRINT("info", ("Current event %s name=%s", et_field,
                          get_field(thd->mem_root,
                                    table->field[ET_FIELD_NAME])));

      if (!sortcmp_lex_string(et_field_lex, schema, system_charset_info))
      {
        DBUG_PRINT("info", ("Dropping"));
        if ((ret= table->file->ha_delete_row(table->record[0])))
          table->file->print_error(ret, MYF(0));
      }
    }
  }
  end_read_record(&read_record_info);
  close_thread_tables(thd);
  /*
    Make sure to only release the MDL lock on mysql.event, not other
    metadata locks DROP DATABASE might have acquired.
  */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  DBUG_VOID_RETURN;
}


/**
  Looks for a named event in mysql.event and then loads it from
  the table.

  @pre The given thread does not have open tables.

  @retval FALSE  success
  @retval TRUE   error
*/

bool
Event_db_repository::load_named_event(THD *thd, LEX_STRING dbname,
                                      LEX_STRING name, Event_basic *etn)
{
  bool ret;
  sql_mode_t saved_mode= thd->variables.sql_mode;
  Open_tables_backup open_tables_backup;
  TABLE_LIST event_table;

  DBUG_ENTER("Event_db_repository::load_named_event");
  DBUG_PRINT("enter",("thd: 0x%lx  name: %*s", (long) thd,
                      (int) name.length, name.str));

  event_table.init_one_table("mysql", 5, "event", 5, "event", TL_READ);

  /* Reset sql_mode during data dictionary operations. */
  thd->variables.sql_mode= 0;

  /*
    We don't use open_event_table() here to make sure that SHOW
    CREATE EVENT works properly in transactional context, and
    does not release transactional metadata locks when the
    event table is closed.
  */
  if (!(ret= open_system_tables_for_read(thd, &event_table, &open_tables_backup)))
  {
    if (table_intact.check(event_table.table, &event_table_def))
    {
      close_system_tables(thd, &open_tables_backup);
      my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
      DBUG_RETURN(TRUE);
    }

    if ((ret= find_named_event(dbname, name, event_table.table)))
      my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
    else if ((ret= etn->load_from_row(thd, event_table.table)))
      my_error(ER_CANNOT_LOAD_FROM_TABLE_V2, MYF(0), "mysql", "event");

    close_system_tables(thd, &open_tables_backup);
  }

  thd->variables.sql_mode= saved_mode;
  DBUG_RETURN(ret);
}


/**
  Update the event record in mysql.event table with a changed status
  and/or last execution time.

  @pre The thread handle does not have open tables.
*/

bool
Event_db_repository::
update_timing_fields_for_event(THD *thd,
                               LEX_STRING event_db_name,
                               LEX_STRING event_name,
                               my_time_t last_executed,
                               ulonglong status)
{
  TABLE *table= NULL;
  Field **fields;
  int ret= 1;
  bool save_binlog_row_based;
  MYSQL_TIME time;

  DBUG_ENTER("Event_db_repository::update_timing_fields_for_event");

  /*
    Turn off row binlogging of event timing updates. These are not used
    for RBR of events replicated to the slave.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  DBUG_ASSERT(thd->security_ctx->master_access & SUPER_ACL);

  if (open_event_table(thd, TL_WRITE, &table))
    goto end;

  fields= table->field;

  if (find_named_event(event_db_name, event_name, table))
    goto end;

  store_record(table, record[1]);

  my_tz_OFFSET0->gmt_sec_to_TIME(&time, last_executed);
  fields[ET_FIELD_LAST_EXECUTED]->set_notnull();
  fields[ET_FIELD_LAST_EXECUTED]->store_time(&time);

  fields[ET_FIELD_STATUS]->set_notnull();
  fields[ET_FIELD_STATUS]->store(status, TRUE);

  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
  {
    table->file->print_error(ret, MYF(0));
    goto end;
  }

  ret= 0;

end:
  if (table)
    close_mysql_tables(thd);

  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
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
  const unsigned int event_priv_column_position= 29;

  DBUG_ENTER("Event_db_repository::check_system_tables");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));

  /* Check mysql.db */
  tables.init_one_table("mysql", 5, "db", 2, "db", TL_READ);

  if (open_and_lock_tables(thd, &tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
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

  if (open_and_lock_tables(thd, &tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
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

  if (open_and_lock_tables(thd, &tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    ret= 1;
    sql_print_error("Cannot open mysql.event");
  }
  else
  {
    if (table_intact.check(tables.table, &event_table_def))
      ret= 1;
    close_mysql_tables(thd);
  }

  DBUG_RETURN(MY_TEST(ret));
}

/**
  @} (End of group Event_Scheduler)
*/
