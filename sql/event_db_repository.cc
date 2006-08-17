/* Copyright (C) 2004-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include "event_db_repository.h"
#include "event_data_objects.h"
#include "events.h"
#include "sql_show.h"
#include "sp.h"
#include "sp_head.h"


static
time_t mysql_event_last_create_time= 0L;

static
const TABLE_FIELD_W_TYPE event_table_fields[ET_FIELD_COUNT] =
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
    { C_STRING_WITH_LEN("enum('ENABLED','DISABLED')") },
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
    "'HIGH_NOT_PRECEDENCE')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  }
};


/*
  Puts some data common to CREATE and ALTER EVENT into a row.

  SYNOPSIS
    mysql_event_fill_row()
      thd        THD
      table      The row to fill out
      et         Event's data
      is_update  CREATE EVENT or ALTER EVENT

  RETURN VALUE
    0                       OK
    EVEX_GENERAL_ERROR      Bad data
    EVEX_GET_FIELD_FAILED   Field count does not match. table corrupted?

  DESCRIPTION 
    Used both when an event is created and when it is altered.
*/

static int
mysql_event_fill_row(THD *thd, TABLE *table, Event_parse_data *et,
                     my_bool is_update)
{
  CHARSET_INFO *scs= system_charset_info;
  enum enum_events_table_field f_num;
  Field **fields= table->field;

  DBUG_ENTER("mysql_event_fill_row");

  DBUG_PRINT("info", ("dbname=[%s]", et->dbname.str));
  DBUG_PRINT("info", ("name  =[%s]", et->name.str));
  DBUG_PRINT("info", ("body  =[%s]", et->body.str));

  if (fields[f_num= ET_FIELD_DEFINER]->
                              store(et->definer.str, et->definer.length, scs))
    goto err_truncate;

  if (fields[f_num= ET_FIELD_DB]->store(et->dbname.str, et->dbname.length, scs))
    goto err_truncate;

  if (fields[f_num= ET_FIELD_NAME]->store(et->name.str, et->name.length, scs))
    goto err_truncate;

  /* both ON_COMPLETION and STATUS are NOT NULL thus not calling set_notnull()*/
  fields[ET_FIELD_ON_COMPLETION]->store((longlong)et->on_completion, TRUE);

  fields[ET_FIELD_STATUS]->store((longlong)et->status, TRUE);

  /*
    Change the SQL_MODE only if body was present in an ALTER EVENT and of course
    always during CREATE EVENT.
  */ 
  if (et->body.str)
  {
    fields[ET_FIELD_SQL_MODE]->store((longlong)thd->variables.sql_mode, TRUE);
    if (fields[f_num= ET_FIELD_BODY]->store(et->body.str, et->body.length, scs))
      goto err_truncate;
  }

  if (et->expression)
  {
    fields[ET_FIELD_INTERVAL_EXPR]->set_notnull();
    fields[ET_FIELD_INTERVAL_EXPR]->store((longlong)et->expression, TRUE);

    fields[ET_FIELD_TRANSIENT_INTERVAL]->set_notnull();
    /*
      In the enum (C) intervals start from 0 but in mysql enum valid values
      start from 1. Thus +1 offset is needed!
    */
    fields[ET_FIELD_TRANSIENT_INTERVAL]->store((longlong)et->interval+1, TRUE);

    fields[ET_FIELD_EXECUTE_AT]->set_null();

    if (!et->starts_null)
    {
      fields[ET_FIELD_STARTS]->set_notnull();
      fields[ET_FIELD_STARTS]->store_time(&et->starts, MYSQL_TIMESTAMP_DATETIME);
    }

    if (!et->ends_null)
    {
      fields[ET_FIELD_ENDS]->set_notnull();
      fields[ET_FIELD_ENDS]->store_time(&et->ends, MYSQL_TIMESTAMP_DATETIME);
    }
  }
  else if (et->execute_at.year)
  {
    fields[ET_FIELD_INTERVAL_EXPR]->set_null();
    fields[ET_FIELD_TRANSIENT_INTERVAL]->set_null();
    fields[ET_FIELD_STARTS]->set_null();
    fields[ET_FIELD_ENDS]->set_null();
    
    fields[ET_FIELD_EXECUTE_AT]->set_notnull();
    fields[ET_FIELD_EXECUTE_AT]->
                        store_time(&et->execute_at, MYSQL_TIMESTAMP_DATETIME);
  }
  else
  {
    DBUG_ASSERT(is_update);
    /*
      it is normal to be here when the action is update
      this is an error if the action is create. something is borked
    */
  }
    
  ((Field_timestamp *)fields[ET_FIELD_MODIFIED])->set_time();

  if (et->comment.str)
  {
    if (fields[f_num= ET_FIELD_COMMENT]->
                          store(et->comment.str, et->comment.length, scs))
      goto err_truncate;
  }

  DBUG_RETURN(0);

err_truncate:
  my_error(ER_EVENT_DATA_TOO_LONG, MYF(0), fields[f_num]->field_name);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
  Performs an index scan of event_table (mysql.event) and fills schema_table.

  SYNOPSIS
    Event_db_repository::index_read_for_db_for_i_s()
      thd          Thread
      schema_table The I_S.EVENTS table
      event_table  The event table to use for loading (mysql.event)

  RETURN VALUE
    0  OK
    1  Error
*/

bool
Event_db_repository::index_read_for_db_for_i_s(THD *thd, TABLE *schema_table,
                                               TABLE *event_table, char *db)
{
  int ret=0;
  CHARSET_INFO *scs= system_charset_info;
  KEY *key_info;
  uint key_len;
  byte *key_buf= NULL;
  LINT_INIT(key_buf);

  DBUG_ENTER("Event_db_repository::index_read_for_db_for_i_s");

  DBUG_PRINT("info", ("Using prefix scanning on PK"));
  event_table->file->ha_index_init(0, 1);
  event_table->field[ET_FIELD_DB]->store(db, strlen(db), scs);
  key_info= event_table->key_info;
  key_len= key_info->key_part[0].store_length;

  if (!(key_buf= (byte *)alloc_root(thd->mem_root, key_len)))
  {
    ret= 1;
    /* Don't send error, it would be done by sql_alloc_error_handler() */
  }
  else
  {
    key_copy(key_buf, event_table->record[0], key_info, key_len);
    if (!(ret= event_table->file->index_read(event_table->record[0], key_buf,
                                             key_len, HA_READ_PREFIX)))
    {
      DBUG_PRINT("info",("Found rows. Let's retrieve them. ret=%d", ret));
      do
      {
        ret= copy_event_to_schema_table(thd, schema_table, event_table);
        if (ret == 0)
          ret= event_table->file->index_next_same(event_table->record[0],
                                                  key_buf, key_len); 
      } while (ret == 0);
    }
    DBUG_PRINT("info", ("Scan finished. ret=%d", ret));
  }
  event_table->file->ha_index_end(); 
  /*  ret is guaranteed to be != 0 */
  if (ret == HA_ERR_END_OF_FILE || ret == HA_ERR_KEY_NOT_FOUND)
    DBUG_RETURN(FALSE);

  DBUG_RETURN(TRUE);
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

  init_read_record(&read_record_info, thd, event_table, NULL, 1, 0);

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
  DBUG_RETURN(ret == -1? FALSE:TRUE);
}


/*
  Fills I_S.EVENTS with data loaded from mysql.event. Also used by
  SHOW EVENTS

  SYNOPSIS
    Event_db_repository::fill_schema_events()
      thd     Thread
      tables  The schema table
      db      If not NULL then get events only from this schema

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

int
Event_db_repository::fill_schema_events(THD *thd, TABLE_LIST *tables, char *db)
{
  TABLE *schema_table= tables->table;
  TABLE *event_table= NULL;
  Open_tables_state backup;
  int ret= 0;

  DBUG_ENTER("Event_db_repository::fill_schema_events");
  DBUG_PRINT("info",("db=%s", db? db:"(null)"));

  thd->reset_n_backup_open_tables_state(&backup);
  if (open_event_table(thd, TL_READ, &event_table))
  {
    sql_print_error("Table mysql.event is damaged.");
    thd->restore_backup_open_tables_state(&backup);
    DBUG_RETURN(1);
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
    ret= index_read_for_db_for_i_s(thd, schema_table, event_table, db);
  else
    ret= table_scan_all_for_i_s(thd, schema_table, event_table);

  close_thread_tables(thd);
  thd->restore_backup_open_tables_state(&backup);

  DBUG_PRINT("info", ("Return code=%d", ret));
  DBUG_RETURN(ret);
}


/*
  Open mysql.event table for read

  SYNOPSIS
    Events::open_event_table()
    thd         [in]  Thread context
    lock_type   [in]  How to lock the table
    table       [out] We will store the open table here

  RETURN VALUE
    1   Cannot lock table
    2   The table is corrupted - different number of fields
    0   OK
*/

int
Event_db_repository::open_event_table(THD *thd, enum thr_lock_type lock_type,
                                      TABLE **table)
{
  TABLE_LIST tables;
  DBUG_ENTER("Event_db_repository::open_event_table");

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "event";
  tables.lock_type= lock_type;

  if (simple_open_n_lock_tables(thd, &tables))
    DBUG_RETURN(1);

  if (table_check_intact(tables.table, ET_FIELD_COUNT,
                         event_table_fields,
                         &mysql_event_last_create_time,
                         ER_CANNOT_LOAD_FROM_TABLE))
  {
    close_thread_tables(thd);
    DBUG_RETURN(2);
  }
  *table= tables.table;
  tables.table->use_all_columns();
  DBUG_RETURN(0);
}


/*
  Checks parameters which we got from the parsing phase.

  SYNOPSIS
    check_parse_params()
      thd         Thread context
      parse_data  Event's data
  
  RETURN VALUE
    FALSE  OK
    TRUE   Error (reported)
*/

static int
check_parse_params(THD *thd, Event_parse_data *parse_data)
{
  const char *pos= NULL;
  Item *bad_item;
  int res;

  DBUG_ENTER("check_parse_params");

  if (parse_data->check_parse_data(thd))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  if (!parse_data->dbname.str ||
      (thd->lex->sql_command == SQLCOM_ALTER_EVENT && thd->lex->spname &&
       !thd->lex->spname->m_db.str))
  {
    my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
    DBUG_RETURN(EVEX_BAD_PARAMS);
  }

  if (check_access(thd, EVENT_ACL, parse_data->dbname.str, 0, 0, 0,
                   is_schema_db(parse_data->dbname.str)) ||
      (thd->lex->sql_command == SQLCOM_ALTER_EVENT && thd->lex->spname &&
       (check_access(thd, EVENT_ACL, thd->lex->spname->m_db.str, 0, 0, 0,
                     is_schema_db(thd->lex->spname->m_db.str)))))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  DBUG_RETURN(0);
}


/*
  Creates an event in mysql.event

  SYNOPSIS
    Event_db_repository::create_event()
      thd             [in]  THD
      parse_data      [in]  Object containing info about the event
      create_if_not   [in]  Whether to generate anwarning in case event exists
      rows_affected   [out] How many rows were affected

  RETURN VALUE
    0                   OK
    EVEX_GENERAL_ERROR  Failure

  DESCRIPTION 
    Creates an event. Relies on mysql_event_fill_row which is shared with
    ::update_event. The name of the event is inside "et".
*/

bool
Event_db_repository::create_event(THD *thd, Event_parse_data *parse_data,
                                  my_bool create_if_not, uint *rows_affected)
{
  int ret= 0;
  CHARSET_INFO *scs= system_charset_info;
  TABLE *table= NULL;
  char old_db_buf[NAME_LEN+1];
  LEX_STRING old_db= { old_db_buf, sizeof(old_db_buf) };
  bool dbchanged= FALSE;

  DBUG_ENTER("Event_db_repository::create_event");

  *rows_affected= 0;
  if (check_parse_params(thd, parse_data))
    goto err;

  DBUG_PRINT("info", ("open mysql.event for update"));
  if (open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }


  DBUG_PRINT("info", ("name: %.*s", parse_data->name.length,
             parse_data->name.str));

  DBUG_PRINT("info", ("check existance of an event with the same name"));
  if (!find_named_event(thd, parse_data->dbname, parse_data->name, table))
  {
    if (create_if_not)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_EVENT_ALREADY_EXISTS, ER(ER_EVENT_ALREADY_EXISTS),
                          parse_data->name.str);
      goto ok;
    }
    my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), parse_data->name.str);
    goto err;
  }

  DBUG_PRINT("info", ("non-existant, go forward"));

  if ((ret= sp_use_new_db(thd, parse_data->dbname, &old_db, 0, &dbchanged)))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0));
    goto err;
  }

  restore_record(table, s->default_values);     // Get default values for fields

  if (system_charset_info->cset->
        numchars(system_charset_info, parse_data->dbname.str,
                 parse_data->dbname.str + parse_data->dbname.length) >
      table->field[ET_FIELD_DB]->char_length())
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), parse_data->dbname.str);
    goto err;
  }

  if (system_charset_info->cset->
        numchars(system_charset_info, parse_data->name.str,
                 parse_data->name.str + parse_data->name.length) >
      table->field[ET_FIELD_NAME]->char_length())
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), parse_data->name.str);
    goto err;
  }

  if (parse_data->body.length > table->field[ET_FIELD_BODY]->field_length)
  {
    my_error(ER_TOO_LONG_BODY, MYF(0), parse_data->name.str);
    goto err;
  }

  if (!(parse_data->expression) && !(parse_data->execute_at.year))
  {
    DBUG_PRINT("error", ("neither expression nor execute_at are set!"));
    my_error(ER_EVENT_NEITHER_M_EXPR_NOR_M_AT, MYF(0));
    goto err;
  }

  ((Field_timestamp *)table->field[ET_FIELD_CREATED])->set_time();

  /*
    mysql_event_fill_row() calls my_error() in case of error so no need to
    handle it here
  */
  if ((ret= mysql_event_fill_row(thd, table, parse_data, FALSE)))
    goto err; 

  /* Close active transaction only if We are going to modify disk */
  if (end_active_trans(thd))
    goto err;

  if (table->file->ha_write_row(table->record[0]))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), parse_data->name.str, ret);
    goto err;
  }

  *rows_affected= 1;
ok:
  if (dbchanged)
    (void) mysql_change_db(thd, old_db.str, 1);
  /*
    This statement may cause a spooky valgrind warning at startup
    inside init_key_cache on my system (ahristov, 2006/08/10) 
  */
  close_thread_tables(thd);
  DBUG_RETURN(FALSE);

err:
  if (dbchanged)
    (void) mysql_change_db(thd, old_db.str, 1);
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(TRUE);
}


/*
  Used to execute ALTER EVENT. Pendant to Events::update_event().

  SYNOPSIS
    Event_db_repository::update_event()
      thd      THD
      sp_name  the name of the event to alter
      et       event's data

  RETURN VALUE
    FALSE  OK
    TRUE   Error (reported)

  NOTES
    sp_name is passed since this is the name of the event to
    alter in case of RENAME TO.
*/

bool
Event_db_repository::update_event(THD *thd, Event_parse_data *parse_data,
                                  LEX_STRING *new_dbname, LEX_STRING *new_name)
{
  CHARSET_INFO *scs= system_charset_info;
  TABLE *table= NULL;
  DBUG_ENTER("Event_db_repository::update_event");

  if (open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }

  if (check_parse_params(thd, parse_data))
    goto err;

  DBUG_PRINT("info", ("dbname: %s", parse_data->dbname.str));
  DBUG_PRINT("info", ("name: %s", parse_data->name.str));
  DBUG_PRINT("info", ("user: %s", parse_data->definer.str));
  if (new_dbname)
    DBUG_PRINT("info", ("rename to: %s@%s", new_dbname->str, new_name->str));

  /* first look whether we overwrite */
  if (new_name)
  {
    if (!sortcmp_lex_string(parse_data->name, *new_name, scs) &&
        !sortcmp_lex_string(parse_data->dbname, *new_dbname, scs))
    {
      my_error(ER_EVENT_SAME_NAME, MYF(0), parse_data->name.str);
      goto err;
    }

    if (!find_named_event(thd, *new_dbname, *new_name, table))
    {
      my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), new_name->str);
      goto err;
    }
  }
  /*
    ...and then if there is such an event. Don't exchange the blocks
    because you will get error 120 from table handler because new_name will
    overwrite the key and SE will tell us that it cannot find the already found
    row (copied into record[1] later
  */
  if (find_named_event(thd, parse_data->dbname, parse_data->name, table))
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), parse_data->name.str);
    goto err;
  }

  store_record(table,record[1]);

  /* Don't update create on row update. */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  /*
    mysql_event_fill_row() calls my_error() in case of error so no need to
    handle it here
  */
  if (mysql_event_fill_row(thd, table, parse_data, TRUE))
    goto err;

  if (new_dbname)
  {
    table->field[ET_FIELD_DB]->store(new_dbname->str, new_dbname->length, scs);
    table->field[ET_FIELD_NAME]->store(new_name->str, new_name->length, scs);
  }

  /* Close active transaction only if We are going to modify disk */
  if (end_active_trans(thd))
    goto err;

  int res;
  if ((res= table->file->ha_update_row(table->record[1], table->record[0])))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), parse_data->name.str, res);
    goto err;
  }

  /* close mysql.event or we crash later when loading the event from disk */
  close_thread_tables(thd);
  DBUG_RETURN(FALSE);

err:
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(TRUE);
}


/*
  Drops an event

  SYNOPSIS
    Event_db_repository::drop_event()
      thd             [in]  THD
      db              [in]  Database name
      name            [in]  Event's name
      drop_if_exists  [in]  If set and the event not existing => warning
                            onto the stack
      rows_affected   [out] Affected number of rows is returned heres

  RETURN VALUE
    FALSE  OK
    TRUE   Error (reported)
*/

bool
Event_db_repository::drop_event(THD *thd, LEX_STRING db, LEX_STRING name,
                                bool drop_if_exists, uint *rows_affected)
{
  TABLE *table= NULL;
  Open_tables_state backup;
  int ret;

  DBUG_ENTER("Event_db_repository::drop_event");
  DBUG_PRINT("enter", ("%s@%s", db.str, name.str));

  thd->reset_n_backup_open_tables_state(&backup);
  if ((ret= open_event_table(thd, TL_WRITE, &table)))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto done;
  }

  if (!(ret= find_named_event(thd, db, name, table)))
  {
    /* Close active transaction only if we are actually going to modify disk */
    if (!(ret= end_active_trans(thd)) &&
        (ret= table->file->ha_delete_row(table->record[0])))
      my_error(ER_EVENT_CANNOT_DELETE, MYF(0));
  }
  else
  {
    if (drop_if_exists)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
                          "Event", name.str);
      ret= 0;
    } else
      my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
  }

done:
  if (table)
    close_thread_tables(thd);
  thd->restore_backup_open_tables_state(&backup);

  DBUG_RETURN(ret);
}


/*
  Positions the internal pointer of `table` to the place where (db, name)
  is stored.

  SYNOPSIS
    Event_db_repository::find_named_event()
      thd    Thread
      db     Schema
      name   Event name
      table  Opened mysql.event

  RETURN VALUE
    FALSE  OK
    TRUE   No such event
*/

bool
Event_db_repository::find_named_event(THD *thd, LEX_STRING db, LEX_STRING name,
                                     TABLE *table)
{
  byte key[MAX_KEY_LENGTH];
  DBUG_ENTER("Event_db_repository::find_named_event");
  DBUG_PRINT("enter", ("name: %.*s", name.length, name.str));

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

  if (table->file->index_read_idx(table->record[0], 0, key,
                                  table->key_info->key_length,
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
  DBUG_ENTER("Event_db_repository::drop_schema_events");
  drop_events_by_field(thd, ET_FIELD_DB, schema);
  DBUG_VOID_RETURN;
}


/*
  Drops all events by field which has specific value of the field

  SYNOPSIS
    Event_db_repository::drop_events_by_field()
      thd         Thread
      table       mysql.event TABLE
      field       Which field of the row to use for matching
      field_value The value that should match
*/

void
Event_db_repository::drop_events_by_field(THD *thd,  
                                          enum enum_events_table_field field,
                                          LEX_STRING field_value)
{
  int ret= 0;
  TABLE *table= NULL;
  READ_RECORD read_record_info;
  DBUG_ENTER("Event_db_repository::drop_events_by_field");  
  DBUG_PRINT("enter", ("field=%d field_value=%s", field, field_value.str));

  if (open_event_table(thd, TL_WRITE, &table))
  {
    /*
      Currently being used only for DROP DATABASE - In this case we don't need
      error message since the OK packet has been sent. But for DROP USER we
      could need it.

      my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    */
    DBUG_VOID_RETURN;
  }

  /* only enabled events are in memory, so we go now and delete the rest */
  init_read_record(&read_record_info, thd, table, NULL, 1, 0);
  while (!ret && !(read_record_info.read_record(&read_record_info)) )
  {
    char *et_field= get_field(thd->mem_root, table->field[field]);

    LEX_STRING et_field_lex= { et_field, strlen(et_field) };
    DBUG_PRINT("info", ("Current event %s name=%s", et_field,
               get_field(thd->mem_root, table->field[ET_FIELD_NAME])));

    if (!sortcmp_lex_string(et_field_lex, field_value, system_charset_info))
    {
      DBUG_PRINT("info", ("Dropping"));
      ret= table->file->ha_delete_row(table->record[0]);
    }
  }
  end_read_record(&read_record_info);
  close_thread_tables(thd);

  DBUG_VOID_RETURN;
}


/*
  Looks for a named event in mysql.event and then loads it from
  the table, compiles and inserts it into the cache.

  SYNOPSIS
    Event_db_repository::load_named_event()
      thd      [in]  Thread context
      dbname   [in]  Event's db name
      name     [in]  Event's name
      etn      [out] The loaded event

  RETURN VALUE
    FALSE  OK
    TRUE   Error (reported)
*/

bool
Event_db_repository::load_named_event(THD *thd, LEX_STRING dbname,
                                      LEX_STRING name, Event_basic *etn)
{
  TABLE *table= NULL;
  int ret= 0;
  Open_tables_state backup;

  DBUG_ENTER("Event_db_repository::load_named_event");
  DBUG_PRINT("enter",("thd=0x%lx name:%*s",thd, name.length, name.str));

  thd->reset_n_backup_open_tables_state(&backup);

  if ((ret= open_event_table(thd, TL_READ, &table)))
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
  else if ((ret= find_named_event(thd, dbname, name, table)))
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
  else if ((ret= etn->load_from_row(table)))
    my_error(ER_CANNOT_LOAD_FROM_TABLE, MYF(0), "event");

  if (table)
    close_thread_tables(thd);

  thd->restore_backup_open_tables_state(&backup);
  /* In this case no memory was allocated so we don't need to clean */

  DBUG_RETURN(ret);
}
