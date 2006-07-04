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
#include "sp_head.h"
#include "sp.h"
#include "events.h"
#include "sql_show.h"

#define EVEX_DB_FIELD_LEN 64
#define EVEX_NAME_FIELD_LEN 64


time_t mysql_event_last_create_time= 0L;

static
TABLE_FIELD_W_TYPE event_table_fields[ET_FIELD_COUNT] = {
  {
    {(char *) STRING_WITH_LEN("db")},
    {(char *) STRING_WITH_LEN("char(64)")},
    {(char *) STRING_WITH_LEN("utf8")}
  },
  {
    {(char *) STRING_WITH_LEN("name")},
    {(char *) STRING_WITH_LEN("char(64)")},
    {(char *) STRING_WITH_LEN("utf8")}
  },
  {
    {(char *) STRING_WITH_LEN("body")},
    {(char *) STRING_WITH_LEN("longblob")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("definer")},
    {(char *) STRING_WITH_LEN("char(77)")},
    {(char *) STRING_WITH_LEN("utf8")}
  },
  {
    {(char *) STRING_WITH_LEN("execute_at")},
    {(char *) STRING_WITH_LEN("datetime")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("interval_value")},
    {(char *) STRING_WITH_LEN("int(11)")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("interval_field")},
    {(char *) STRING_WITH_LEN("enum('YEAR','QUARTER','MONTH','DAY',"
    "'HOUR','MINUTE','WEEK','SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR',"
    "'DAY_MINUTE','DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND',"
    "'DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND',"
    "'SECOND_MICROSECOND')")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("created")},
    {(char *) STRING_WITH_LEN("timestamp")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("modified")},
    {(char *) STRING_WITH_LEN("timestamp")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("last_executed")},
    {(char *) STRING_WITH_LEN("datetime")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("starts")},
    {(char *) STRING_WITH_LEN("datetime")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("ends")},
    {(char *) STRING_WITH_LEN("datetime")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("status")},
    {(char *) STRING_WITH_LEN("enum('ENABLED','DISABLED')")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("on_completion")},
    {(char *) STRING_WITH_LEN("enum('DROP','PRESERVE')")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("sql_mode")},
    {(char *) STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE')")},
    {NULL, 0}
  },
  {
    {(char *) STRING_WITH_LEN("comment")},
    {(char *) STRING_WITH_LEN("char(64)")},
    {(char *) STRING_WITH_LEN("utf8")}
  }
};


/*
  Puts some data common to CREATE and ALTER EVENT into a row.

  SYNOPSIS
    evex_fill_row()
      thd        THD
      table      The row to fill out
      et         Event's data
      is_update  CREATE EVENT or ALTER EVENT

  RETURN VALUE
    0 - OK
    EVEX_GENERAL_ERROR    - bad data
    EVEX_GET_FIELD_FAILED - field count does not match. table corrupted?

  DESCRIPTION 
    Used both when an event is created and when it is altered.
*/

static int
evex_fill_row(THD *thd, TABLE *table, Event_parse_data *et, my_bool is_update)
{
  CHARSET_INFO *scs= system_charset_info;
  enum enum_events_table_field field_num;

  DBUG_ENTER("evex_fill_row");

  DBUG_PRINT("info", ("dbname=[%s]", et->dbname.str));
  DBUG_PRINT("info", ("name  =[%s]", et->name.str));
  DBUG_PRINT("info", ("body  =[%s]", et->body.str));

  if (table->field[field_num= ET_FIELD_DEFINER]->
                  store(et->definer.str, et->definer.length, scs))
    goto err_truncate;

  if (table->field[field_num= ET_FIELD_DB]->
                  store(et->dbname.str, et->dbname.length, scs))
    goto err_truncate;

  if (table->field[field_num= ET_FIELD_NAME]->
                  store(et->name.str, et->name.length, scs))
    goto err_truncate;

  /* both ON_COMPLETION and STATUS are NOT NULL thus not calling set_notnull()*/
  table->field[ET_FIELD_ON_COMPLETION]->
                                       store((longlong)et->on_completion, true);

  table->field[ET_FIELD_STATUS]->store((longlong)et->status, true);

  /*
    Change the SQL_MODE only if body was present in an ALTER EVENT and of course
    always during CREATE EVENT.
  */ 
  if (et->body.str)
  {
    table->field[ET_FIELD_SQL_MODE]->
                               store((longlong)thd->variables.sql_mode, true);

    if (table->field[field_num= ET_FIELD_BODY]->
                     store(et->body.str, et->body.length, scs))
      goto err_truncate;
  }

  if (et->expression)
  {
    table->field[ET_FIELD_INTERVAL_EXPR]->set_notnull();
    table->field[ET_FIELD_INTERVAL_EXPR]->store((longlong)et->expression, true);

    table->field[ET_FIELD_TRANSIENT_INTERVAL]->set_notnull();
    /*
      In the enum (C) intervals start from 0 but in mysql enum valid values start
      from 1. Thus +1 offset is needed!
    */
    table->field[ET_FIELD_TRANSIENT_INTERVAL]->
                                         store((longlong)et->interval+1, true);

    table->field[ET_FIELD_EXECUTE_AT]->set_null();

    if (!et->starts_null)
    {
      table->field[ET_FIELD_STARTS]->set_notnull();
      table->field[ET_FIELD_STARTS]->
                            store_time(&et->starts, MYSQL_TIMESTAMP_DATETIME);
    }	   

    if (!et->ends_null)
    {
      table->field[ET_FIELD_ENDS]->set_notnull();
      table->field[ET_FIELD_ENDS]->
                            store_time(&et->ends, MYSQL_TIMESTAMP_DATETIME);
    }
  }
  else if (et->execute_at.year)
  {
    table->field[ET_FIELD_INTERVAL_EXPR]->set_null();
    table->field[ET_FIELD_TRANSIENT_INTERVAL]->set_null();
    table->field[ET_FIELD_STARTS]->set_null();
    table->field[ET_FIELD_ENDS]->set_null();
    
    table->field[ET_FIELD_EXECUTE_AT]->set_notnull();
    table->field[ET_FIELD_EXECUTE_AT]->
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
    
  ((Field_timestamp *)table->field[ET_FIELD_MODIFIED])->set_time();

  if (et->comment.str)
  {
    if (table->field[field_num= ET_FIELD_COMMENT]->
                 store(et->comment.str, et->comment.length, scs))
      goto err_truncate;
  }

  DBUG_RETURN(0);
err_truncate:
  my_error(ER_EVENT_DATA_TOO_LONG, MYF(0), table->field[field_num]->field_name);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
  Find row in open mysql.event table representing event

  SYNOPSIS
    evex_db_find_event_by_name()
      thd    Thread context
      dbname Name of event's database
      rname  Name of the event inside the db  
      table  TABLE object for open mysql.event table.

  RETURN VALUE
    0                  - Routine found
    EVEX_KEY_NOT_FOUND - No routine with given name
*/

int
evex_db_find_event_by_name(THD *thd, const LEX_STRING dbname,
                          const LEX_STRING ev_name,
                          TABLE *table)
{
  return Events::get_instance()->db_repository->
            find_event_by_name(thd, dbname, ev_name, table);
}


/*
  Performs an index scan of event_table (mysql.event) and fills schema_table.

  Synopsis
    Event_db_repository::index_read_for_db_for_i_s()
      thd          Thread
      schema_table The I_S.EVENTS table
      event_table  The event table to use for loading (mysql.event)

  Returns
    0  OK
    1  Error
*/

int
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
    /* don't send error, it would be done by sql_alloc_error_handler() */
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
    DBUG_RETURN(0);
  DBUG_RETURN(1);
}


/*
  Performs a table scan of event_table (mysql.event) and fills schema_table.

  Synopsis
    Events_db_repository::table_scan_all_for_i_s()
      thd          Thread
      schema_table The I_S.EVENTS in memory table
      event_table  The event table to use for loading.

  Returns
    0  OK
    1  Error
*/

int
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
  }
  while (ret == 0);

  DBUG_PRINT("info", ("Scan finished. ret=%d", ret));
  end_read_record(&read_record_info);

  /*  ret is guaranteed to be != 0 */
  DBUG_RETURN(ret == -1? 0:1);
}


/*
  Fills I_S.EVENTS with data loaded from mysql.event. Also used by
  SHOW EVENTS

  Synopsis
    Event_db_repository::fill_schema_events()
      thd     Thread
      tables  The schema table
      db      If not NULL then get events only from this schema

  Returns
    0  OK
    1  Error
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
  Looks for a named event in mysql.event and in case of success returns
  an object will data loaded from the table.

  SYNOPSIS
    Event_db_repository::find_event()
      thd      THD
      name     the name of the event to find
      ett      event's data if event is found
      tbl      TABLE object to use when not NULL
      root     On which root to load the event

  NOTES
    1) Use sp_name for look up, return in **ett if found
    2) tbl is not closed at exit

  RETURN VALUE
    0  ok     In this case *ett is set to the event
    #  error  *ett == 0
*/

int
Event_db_repository::find_event(THD *thd, LEX_STRING dbname, LEX_STRING name,
                                Event_timed **ett,
                                TABLE *tbl, MEM_ROOT *root)
{
  TABLE *table;
  int ret;
  Event_timed *et= NULL;
  DBUG_ENTER("Event_db_repository::find_event");
  DBUG_PRINT("enter", ("name: %*s", name.length, name.str));

  if (tbl)
    table= tbl;
  else if (open_event_table(thd, TL_READ, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    ret= EVEX_GENERAL_ERROR;
    goto done;
  }

  if ((ret= evex_db_find_event_by_name(thd, dbname, name, table)))
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
    goto done;
  }
  et= new Event_timed;

  /*
    1)The table should not be closed beforehand. ::load_from_row() only loads
      and does not compile

    2)::load_from_row() is silent on error therefore we emit error msg here
  */
  if ((ret= et->load_from_row(table)))
  {
    my_error(ER_CANNOT_LOAD_FROM_TABLE, MYF(0), "event");
    goto done;
  }

done:
  if (ret)
  {
    delete et;
    et= 0;
  }
  /* don't close the table if we haven't opened it ourselves */
  if (!tbl && table)
    close_thread_tables(thd);
  *ett= et;
  DBUG_RETURN(ret);
}


int
Event_db_repository::init_repository()
{
  init_alloc_root(&repo_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);
  return 0;
}


void
Event_db_repository::deinit_repository()
{
  free_root(&repo_root, MYF(0));
}


/*
  Open mysql.event table for read

  SYNOPSIS
    Events::open_event_table()
    thd         Thread context
    lock_type   How to lock the table
    table       We will store the open table here

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
     evex_check_params()
       thd            THD
       et             event's data
  
   RETURNS
     0                OK
     EVEX_BAD_PARAMS  Error
  
   REMARKS
     Issues error messages
*/

int
evex_check_params(THD *thd, Event_parse_data *parse_data)
{
  const char *pos= NULL;
  Item *bad_item;

  DBUG_ENTER("evex_check_params");
  DBUG_PRINT("info", ("execute_at=0x%d expr=0x%d starts=0x%d ends=0x%d",
                      parse_data->item_execute_at,
                      parse_data->item_expression,
                      parse_data->item_starts,
                      parse_data->item_ends));

  parse_data->init_name(thd, parse_data->identifier);
  parse_data->init_definer(thd);

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

  if (parse_data->item_execute_at)
  {
    DBUG_PRINT("info", ("ONE TIME"));
    if (parse_data->init_execute_at(thd, parse_data->item_execute_at))
    {
      pos= "AT";
      bad_item= parse_data->item_execute_at;
      goto wrong_value;
    }
  }
  else
  {
    int res;
    DBUG_PRINT("info", ("RECURRING"));

    if (parse_data->item_expression &&
        (res= parse_data->init_interval(thd, parse_data->item_expression,
                                        parse_data->interval)))
    {
      switch (res) {
      case EVEX_BAD_PARAMS:
        my_error(ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG, MYF(0));
        break;
      case EVEX_MICROSECOND_UNSUP:
        my_error(ER_NOT_SUPPORTED_YET, MYF(0), "MICROSECOND");
        break;
      default:
        pos= "INTERVAL";
        bad_item= parse_data->item_expression;
        goto wrong_value;
      }
      DBUG_RETURN(EVEX_BAD_PARAMS);
    }

    if (parse_data->item_starts &&
        parse_data->init_starts(thd, parse_data->item_starts))
    {
      pos= "STARTS";
      bad_item= parse_data->item_starts;
      goto wrong_value;
    }

    if (parse_data->item_ends &&
        parse_data->init_ends(thd, parse_data->item_ends))
    {
      /*
        despite the error name the value is
        eng "ENDS is either invalid or before STARTS"
      */
      my_error(ER_EVENT_ENDS_BEFORE_STARTS, MYF(0));
      DBUG_RETURN(EVEX_BAD_PARAMS);
    }
  }
  DBUG_RETURN(0);
wrong_value:
  {
    char buff[120];
    String str(buff,(uint32) sizeof(buff), system_charset_info);
    String *str2= bad_item->fixed? bad_item->val_str(&str):NULL;
    my_error(ER_WRONG_VALUE, MYF(0), pos, str2? str2->c_ptr():"NULL");
    DBUG_RETURN(EVEX_BAD_PARAMS);
  }
}


/*
  Creates an event in mysql.event

  SYNOPSIS
    Event_db_repository::create_event()
      thd             THD
      et              Event_timed object containing information for the event
      create_if_not   If an warning should be generated in case event exists
      rows_affected   How many rows were affected

  RETURN VALUE
                     0 - OK
    EVEX_GENERAL_ERROR - Failure

  DESCRIPTION 
    Creates an event. Relies on evex_fill_row which is shared with
    db_update_event. The name of the event is inside "et".
*/

int
Event_db_repository::create_event(THD *thd, Event_parse_data *parse_data,
                                  my_bool create_if_not, uint *rows_affected)
{
  int ret= 0;
  CHARSET_INFO *scs= system_charset_info;
  TABLE *table;
  char olddb[128];
  bool dbchanged= false;
  DBUG_ENTER("Event_db_repository::create_event");

  *rows_affected= 0;
  DBUG_PRINT("info", ("open mysql.event for update"));
  if (open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }

  if (evex_check_params(thd, parse_data))
    goto err;

  DBUG_PRINT("info", ("name: %.*s", parse_data->name.length,
             parse_data->name.str));

  DBUG_PRINT("info", ("check existance of an event with the same name"));
  if (!evex_db_find_event_by_name(thd, parse_data->dbname, parse_data->name, table))
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
  if ((ret= sp_use_new_db(thd, parse_data->dbname.str, olddb, sizeof(olddb), 0,
                          &dbchanged)))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0));
    goto err;
  }

  restore_record(table, s->default_values);     // Get default values for fields

  if (system_charset_info->cset->
        numchars(system_charset_info, parse_data->dbname.str,
                 parse_data->dbname.str +
                 parse_data->dbname.length) > EVEX_DB_FIELD_LEN)
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), parse_data->dbname.str);
    goto err;
  }
  if (system_charset_info->cset->
        numchars(system_charset_info, parse_data->name.str,
                 parse_data->name.str +
                 parse_data->name.length) > EVEX_DB_FIELD_LEN)
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
    evex_fill_row() calls my_error() in case of error so no need to
    handle it here
  */
  if ((ret= evex_fill_row(thd, table, parse_data, false)))
    goto err; 

  /* Close active transaction only if We are going to modify disk */
  if (end_active_trans(thd))
    goto err;

  if (table->file->ha_write_row(table->record[0]))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), parse_data->name.str, ret);
    goto err;
  }

#ifdef USE_THIS_CODE_AS_TEMPLATE_WHEN_EVENT_REPLICATION_IS_AGREED
  if (mysql_bin_log.is_open())
  {
    thd->clear_error();
    /* Such a statement can always go directly to binlog, no trans cache */
    thd->binlog_query(THD::MYSQL_QUERY_TYPE, thd->query, thd->query_length,
                      FALSE, FALSE);
  }
#endif

  *rows_affected= 1;
ok:
  if (dbchanged)
    (void) mysql_change_db(thd, olddb, 1);
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(EVEX_OK);

err:
  if (dbchanged)
    (void) mysql_change_db(thd, olddb, 1);
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
  Used to execute ALTER EVENT. Pendant to Events::update_event().

  SYNOPSIS
    Event_db_repository::update_event()
      thd      THD
      sp_name  the name of the event to alter
      et       event's data

  RETURN VALUE
    0  OK
    EVEX_GENERAL_ERROR  Error occured (my_error() called)

  NOTES
    sp_name is passed since this is the name of the event to
    alter in case of RENAME TO.
*/

int
Event_db_repository::update_event(THD *thd, Event_parse_data *parse_data,
                                  sp_name *new_name)
{
  CHARSET_INFO *scs= system_charset_info;
  TABLE *table;
  int ret= EVEX_OPEN_TABLE_FAILED;
  DBUG_ENTER("Event_db_repository::update_event");

  if (open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }

  if (evex_check_params(thd, parse_data))
    goto err;

  DBUG_PRINT("info", ("dbname: %s", parse_data->dbname.str));
  DBUG_PRINT("info", ("name: %s", parse_data->name.str));
  DBUG_PRINT("info", ("user: %s", parse_data->definer.str));
  if (new_name)
    DBUG_PRINT("info", ("rename to: %s", new_name->m_name.str));

  /* first look whether we overwrite */
  if (new_name)
  {
    if (!sortcmp_lex_string(parse_data->name, new_name->m_name, scs) &&
        !sortcmp_lex_string(parse_data->dbname, new_name->m_db, scs))
    {
      my_error(ER_EVENT_SAME_NAME, MYF(0), parse_data->name.str);
      goto err;
    }

    if (!evex_db_find_event_by_name(thd,new_name->m_db,new_name->m_name,table))
    {
      my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), new_name->m_name.str);
      goto err;
    }
  }
  /*
    ...and then if there is such an event. Don't exchange the blocks
    because you will get error 120 from table handler because new_name will
    overwrite the key and SE will tell us that it cannot find the already found
    row (copied into record[1] later
  */
  if (EVEX_KEY_NOT_FOUND == find_event_by_name(thd, parse_data->dbname,
                                               parse_data->name, table))
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), parse_data->name.str);
    goto err;
  }

  store_record(table,record[1]);

  /* Don't update create on row update. */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  /*
    evex_fill_row() calls my_error() in case of error so no need to
    handle it here
  */
  if ((ret= evex_fill_row(thd, table, parse_data, true)))
    goto err;

  if (new_name)
  {
    table->field[ET_FIELD_DB]->
      store(new_name->m_db.str, new_name->m_db.length, scs);
    table->field[ET_FIELD_NAME]->
      store(new_name->m_name.str, new_name->m_name.length, scs);
  }

  /* Close active transaction only if We are going to modify disk */
  if (end_active_trans(thd))
    goto err;

  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), parse_data->name.str, ret);
    goto err;
  }

  /* close mysql.event or we crash later when loading the event from disk */
  close_thread_tables(thd);
  DBUG_RETURN(0);

err:
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
  Drops an event

  SYNOPSIS
    Event_db_repository::drop_event()
      thd             THD
      db              database name
      name            event's name
      drop_if_exists  if set and the event not existing => warning onto the stack
      rows_affected   affected number of rows is returned heres

  RETURN VALUE
    0   OK
    !0  Error (my_error() called)
*/

int
Event_db_repository::drop_event(THD *thd, LEX_STRING db, LEX_STRING name,
                                bool drop_if_exists, uint *rows_affected)
{
  TABLE *table;
  Open_tables_state backup;
  int ret;

  DBUG_ENTER("Event_db_repository::drop_event");
  DBUG_PRINT("enter", ("db=%s name=%s", db.str, name.str));
  ret= EVEX_OPEN_TABLE_FAILED;

  thd->reset_n_backup_open_tables_state(&backup);
  if (open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto done;
  }

  if (!(ret= evex_db_find_event_by_name(thd, db, name, table)))
  {
    /* Close active transaction only if We are going to modify disk */
    if ((ret= end_active_trans(thd)))
      goto done;

    if ((ret= table->file->ha_delete_row(table->record[0])))
      my_error(ER_EVENT_CANNOT_DELETE, MYF(0));
  }
  else if (ret == EVEX_KEY_NOT_FOUND)
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
  /*
    evex_drop_event() is used by Event_timed::drop therefore
    we have to close our thread tables.
  */
  close_thread_tables(thd);
  thd->restore_backup_open_tables_state(&backup);
  DBUG_RETURN(ret);
}


int
Event_db_repository::find_event_by_name(THD *thd, LEX_STRING db,
                                        LEX_STRING name, TABLE *table)
{
  byte key[MAX_KEY_LENGTH];
  DBUG_ENTER("Event_db_repository::find_event_by_name");
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
      
    DBUG_RETURN(EVEX_KEY_NOT_FOUND);

  table->field[ET_FIELD_DB]->store(db.str, db.length, &my_charset_bin);
  table->field[ET_FIELD_NAME]->store(name.str, name.length, &my_charset_bin);

  key_copy(key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx(table->record[0], 0, key,
                                  table->key_info->key_length,
                                  HA_READ_KEY_EXACT))
  {
    DBUG_PRINT("info", ("Row not found"));
    DBUG_RETURN(EVEX_KEY_NOT_FOUND);
  }

  DBUG_PRINT("info", ("Row found!"));
  DBUG_RETURN(0);
}


int
Event_db_repository::drop_schema_events(THD *thd, LEX_STRING schema)
{
  return drop_events_by_field(thd, ET_FIELD_DB, schema);
}


int
Event_db_repository::drop_user_events(THD *thd, LEX_STRING definer)
{
  return drop_events_by_field(thd, ET_FIELD_DEFINER, definer);
}


/*
  Drops all events in the selected database, from mysql.event.

  SYNOPSIS
    drop_events_from_table_by_field()
      thd         Thread
      table       mysql.event TABLE
      field       Which field of the row to use for matching
      field_value The value that should match

  RETURN VALUE
     0  OK
    !0  Error from ha_delete_row
*/

int
Event_db_repository::drop_events_by_field(THD *thd,  
                                          enum enum_events_table_field field,
                                          LEX_STRING field_value)
{
  int ret= 0;
  TABLE *table;
  Open_tables_state backup;
  READ_RECORD read_record_info;
  DBUG_ENTER("Event_db_repository::drop_events_by_field");  
  DBUG_PRINT("enter", ("field=%d field_value=%s", field, field_value.str));

  if (open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    DBUG_RETURN(1);
  }

  /* only enabled events are in memory, so we go now and delete the rest */
  init_read_record(&read_record_info, thd, table, NULL, 1, 0);
  while (!ret && !(read_record_info.read_record(&read_record_info)) )
  {
    char *et_field= get_field(thd->mem_root, table->field[field]);

    LEX_STRING et_field_lex= {et_field, strlen(et_field)};
    DBUG_PRINT("info", ("Current event %s name=%s", et_field,
               get_field(thd->mem_root, table->field[ET_FIELD_NAME])));

    if (!sortcmp_lex_string(et_field_lex, field_value, system_charset_info))
    {
      DBUG_PRINT("info", ("Dropping"));
      if ((ret= table->file->ha_delete_row(table->record[0])))
        my_error(ER_EVENT_DROP_FAILED, MYF(0),
                 get_field(thd->mem_root, table->field[ET_FIELD_NAME]));
    }
  }
  end_read_record(&read_record_info);
  thd->version--;   /* Force close to free memory */

  DBUG_RETURN(ret);
}


/*
  Looks for a named event in mysql.event and then loads it from
  the table, compiles and inserts it into the cache.

  SYNOPSIS
    Event_scheduler::load_named_event()
      thd      THD
      etn      The name of the event to load and compile on scheduler's root
      etn_new  The loaded event

  RETURN VALUE
    NULL       Error during compile or the event is non-enabled.
    otherwise  Address
*/

int
Event_db_repository::load_named_event(THD *thd, LEX_STRING dbname, LEX_STRING name,
                                      Event_timed **etn_new)
{
  int ret= 0;
  MEM_ROOT *tmp_mem_root;
  Event_timed *et_loaded= NULL;
  Open_tables_state backup;

  DBUG_ENTER("Event_db_repository::load_named_event");
  DBUG_PRINT("enter",("thd=%p name:%*s",thd, name.length, name.str));

  thd->reset_n_backup_open_tables_state(&backup);
  /* No need to use my_error() here because db_find_event() has done it */
  ret= find_event(thd, dbname, name, &et_loaded, NULL, &repo_root);
  thd->restore_backup_open_tables_state(&backup);
  /* In this case no memory was allocated so we don't need to clean */
  if (ret)
    DBUG_RETURN(OP_LOAD_ERROR);

  if (et_loaded->status != Event_timed::ENABLED)
  {
    /*
      We don't load non-enabled events.
      In db_find_event() `et_new` was allocated on the heap and not on
      scheduler_root therefore we delete it here.
    */
    delete et_loaded;
    DBUG_RETURN(OP_DISABLED_EVENT);
  }

  et_loaded->compute_next_execution_time();
  *etn_new= et_loaded;

  DBUG_RETURN(OP_OK);
}
