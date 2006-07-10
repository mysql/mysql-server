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
#include "events_priv.h"
#include "events.h"
#include "event_timed.h"
#include "event_scheduler.h"
#include "sp.h"
#include "sp_head.h"

/*
 TODO list :
 - CREATE EVENT should not go into binary log! Does it now? The SQL statements
   issued by the EVENT are replicated.
   I have an idea how to solve the problem at failover. So the status field
   will be ENUM('DISABLED', 'ENABLED', 'SLAVESIDE_DISABLED').
   In this case when CREATE EVENT is replicated it should go into the binary
   as SLAVESIDE_DISABLED if it is ENABLED, when it's created as DISABLEd it
   should be replicated as disabled. If an event is ALTERed as DISABLED the
   query should go untouched into the binary log, when ALTERed as enable then
   it should go as SLAVESIDE_DISABLED. This is regarding the SQL interface.
   TT routines however modify mysql.event internally and this does not go the
   log so in this case queries has to be injected into the log...somehow... or
   maybe a solution is RBR for this case, because the event may go only from
   ENABLED to DISABLED status change and this is safe for replicating. As well
   an event may be deleted which is also safe for RBR.

 - Add logging to file

Warning:
 - For now parallel execution is not possible because the same sp_head cannot
   be executed few times!!! There is still no lock attached to particular
   event.
*/


MEM_ROOT evex_mem_root;
time_t mysql_event_last_create_time= 0L;


const char *event_scheduler_state_names[]=
    { "OFF", "0", "ON", "1", "SUSPEND", "2", NullS };

TYPELIB Events::opt_typelib=
{
  array_elements(event_scheduler_state_names)-1,
  "",
  event_scheduler_state_names,
  NULL
};


ulong Events::opt_event_scheduler= 2;

static
TABLE_FIELD_W_TYPE event_table_fields[Events::FIELD_COUNT] = {
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
  Compares 2 LEX strings regarding case.

  SYNOPSIS
    sortcmp_lex_string()

      s - first LEX_STRING
      t - second LEX_STRING
      cs - charset

  RETURN VALUE
   -1   - s < t
    0   - s == t
    1   - s > t

  Notes
    TIME.second_part is not considered during comparison
*/

int sortcmp_lex_string(LEX_STRING s, LEX_STRING t, CHARSET_INFO *cs)
{
 return cs->coll->strnncollsp(cs, (uchar *) s.str,s.length,
                                  (uchar *) t.str,t.length, 0);
}


/*
  Reconstructs interval expression from interval type and expression
  value that is in form of a value of the smalles entity:
  For
    YEAR_MONTH - expression is in months
    DAY_MINUTE - expression is in minutes

  SYNOPSIS
    Events::reconstruct_interval_expression()
      buf - preallocated String buffer to add the value to
      interval - the interval type (for instance YEAR_MONTH)
      expression - the value in the lowest entity

  RETURNS
   0 - OK
   1 - Error
*/

int
Events::reconstruct_interval_expression(String *buf,
                                                  interval_type interval,
                                                  longlong expression)
{
  ulonglong expr= expression;
  char tmp_buff[128], *end;
  bool close_quote= TRUE;
  int multipl= 0;
  char separator=':';

  switch (interval) {
  case INTERVAL_YEAR_MONTH:
    multipl= 12;
    separator= '-';
    goto common_1_lev_code;
  case INTERVAL_DAY_HOUR:
    multipl= 24;
    separator= ' ';
    goto common_1_lev_code;
  case INTERVAL_HOUR_MINUTE:
  case INTERVAL_MINUTE_SECOND:
    multipl= 60;
common_1_lev_code:
    buf->append('\'');
    end= longlong10_to_str(expression/multipl, tmp_buff, 10);
    buf->append(tmp_buff, (uint) (end- tmp_buff));
    expr= expr - (expr/multipl)*multipl;
    break;
  case INTERVAL_DAY_MINUTE:
  {
    ulonglong tmp_expr= expr;

    tmp_expr/=(24*60);
    buf->append('\'');
    end= longlong10_to_str(tmp_expr, tmp_buff, 10);
    buf->append(tmp_buff, (uint) (end- tmp_buff));// days
    buf->append(' ');

    tmp_expr= expr - tmp_expr*(24*60);//minutes left
    end= longlong10_to_str(tmp_expr/60, tmp_buff, 10);
    buf->append(tmp_buff, (uint) (end- tmp_buff));// hours

    expr= tmp_expr - (tmp_expr/60)*60;
    /* the code after the switch will finish */
  }
    break;
  case INTERVAL_HOUR_SECOND:
  {
    ulonglong tmp_expr= expr;

    buf->append('\'');
    end= longlong10_to_str(tmp_expr/3600, tmp_buff, 10);
    buf->append(tmp_buff, (uint) (end- tmp_buff));// hours
    buf->append(':');

    tmp_expr= tmp_expr - (tmp_expr/3600)*3600;
    end= longlong10_to_str(tmp_expr/60, tmp_buff, 10);
    buf->append(tmp_buff, (uint) (end- tmp_buff));// minutes

    expr= tmp_expr - (tmp_expr/60)*60;
    /* the code after the switch will finish */
  }
    break;      
  case INTERVAL_DAY_SECOND:
  {
    ulonglong tmp_expr= expr;

    tmp_expr/=(24*3600);
    buf->append('\'');
    end= longlong10_to_str(tmp_expr, tmp_buff, 10);
    buf->append(tmp_buff, (uint) (end- tmp_buff));// days
    buf->append(' ');

    tmp_expr= expr - tmp_expr*(24*3600);//seconds left
    end= longlong10_to_str(tmp_expr/3600, tmp_buff, 10);
    buf->append(tmp_buff, (uint) (end- tmp_buff));// hours
    buf->append(':');

    tmp_expr= tmp_expr - (tmp_expr/3600)*3600;
    end= longlong10_to_str(tmp_expr/60, tmp_buff, 10);
    buf->append(tmp_buff, (uint) (end- tmp_buff));// minutes

    expr= tmp_expr - (tmp_expr/60)*60;
    /* the code after the switch will finish */
  }
    break;  
  case INTERVAL_DAY_MICROSECOND:
  case INTERVAL_HOUR_MICROSECOND:
  case INTERVAL_MINUTE_MICROSECOND:
  case INTERVAL_SECOND_MICROSECOND:
  case INTERVAL_MICROSECOND:
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "MICROSECOND");
    return 1;
    break;
  case INTERVAL_QUARTER:
    expr/= 3;
    close_quote= FALSE;
    break;
  case INTERVAL_WEEK:
    expr/= 7;
  default:
    close_quote= FALSE;
    break;
  }
  if (close_quote)
    buf->append(separator);
  end= longlong10_to_str(expr, tmp_buff, 10);
  buf->append(tmp_buff, (uint) (end- tmp_buff));
  if (close_quote)
    buf->append('\'');

  return 0;
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
Events::open_event_table(THD *thd, enum thr_lock_type lock_type,
                                   TABLE **table)
{
  TABLE_LIST tables;
  DBUG_ENTER("open_events_table");

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "event";
  tables.lock_type= lock_type;

  if (simple_open_n_lock_tables(thd, &tables))
    DBUG_RETURN(1);
  
  if (table_check_intact(tables.table, Events::FIELD_COUNT,
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
  Find row in open mysql.event table representing event

  SYNOPSIS
    evex_db_find_event_aux()
      thd    Thread context
      et     event_timed object containing dbname & name
      table  TABLE object for open mysql.event table.

  RETURN VALUE
    0                  - Routine found
    EVEX_KEY_NOT_FOUND - No routine with given name
*/

inline int
evex_db_find_event_aux(THD *thd, Event_timed *et, TABLE *table)
{
  return evex_db_find_event_by_name(thd, et->dbname, et->name, table);
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
  byte key[MAX_KEY_LENGTH];
  DBUG_ENTER("evex_db_find_event_by_name");
  DBUG_PRINT("enter", ("name: %.*s", ev_name.length, ev_name.str));

  /*
    Create key to find row. We have to use field->store() to be able to
    handle VARCHAR and CHAR fields.
    Assumption here is that the two first fields in the table are
    'db' and 'name' and the first key is the primary key over the
    same fields.
  */
  if (dbname.length > table->field[Events::FIELD_DB]->field_length ||
      ev_name.length > table->field[Events::FIELD_NAME]->field_length)
      
    DBUG_RETURN(EVEX_KEY_NOT_FOUND);

  table->field[Events::FIELD_DB]->store(dbname.str, dbname.length,
                                        &my_charset_bin);
  table->field[Events::FIELD_NAME]->store(ev_name.str, ev_name.length,
                                          &my_charset_bin);

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


/*
  Puts some data common to CREATE and ALTER EVENT into a row.

  SYNOPSIS
    evex_fill_row()
      thd    THD
      table  the row to fill out
      et     Event's data

  RETURN VALUE
    0 - OK
    EVEX_GENERAL_ERROR    - bad data
    EVEX_GET_FIELD_FAILED - field count does not match. table corrupted?

  DESCRIPTION 
    Used both when an event is created and when it is altered.
*/

static int
evex_fill_row(THD *thd, TABLE *table, Event_timed *et, my_bool is_update)
{
  CHARSET_INFO *scs= system_charset_info;
  enum Events::enum_table_field field_num;

  DBUG_ENTER("evex_fill_row");

  DBUG_PRINT("info", ("dbname=[%s]", et->dbname.str));
  DBUG_PRINT("info", ("name  =[%s]", et->name.str));
  DBUG_PRINT("info", ("body  =[%s]", et->body.str));

  if (table->field[field_num= Events::FIELD_DEFINER]->
                  store(et->definer.str, et->definer.length, scs))
    goto err_truncate;

  if (table->field[field_num= Events::FIELD_DB]->
                  store(et->dbname.str, et->dbname.length, scs))
    goto err_truncate;

  if (table->field[field_num= Events::FIELD_NAME]->
                  store(et->name.str, et->name.length, scs))
    goto err_truncate;

  /* both ON_COMPLETION and STATUS are NOT NULL thus not calling set_notnull()*/
  table->field[Events::FIELD_ON_COMPLETION]->
                                       store((longlong)et->on_completion, true);

  table->field[Events::FIELD_STATUS]->store((longlong)et->status, true);

  /*
    Change the SQL_MODE only if body was present in an ALTER EVENT and of course
    always during CREATE EVENT.
  */ 
  if (et->body.str)
  {
    table->field[Events::FIELD_SQL_MODE]->
                               store((longlong)thd->variables.sql_mode, true);

    if (table->field[field_num= Events::FIELD_BODY]->
                     store(et->body.str, et->body.length, scs))
      goto err_truncate;
  }

  if (et->expression)
  {
    table->field[Events::FIELD_INTERVAL_EXPR]->set_notnull();
    table->field[Events::FIELD_INTERVAL_EXPR]->
                                          store((longlong)et->expression, true);

    table->field[Events::FIELD_TRANSIENT_INTERVAL]->set_notnull();
    /*
      In the enum (C) intervals start from 0 but in mysql enum valid values start
      from 1. Thus +1 offset is needed!
    */
    table->field[Events::FIELD_TRANSIENT_INTERVAL]->
                                         store((longlong)et->interval+1, true);

    table->field[Events::FIELD_EXECUTE_AT]->set_null();

    if (!et->starts_null)
    {
      table->field[Events::FIELD_STARTS]->set_notnull();
      table->field[Events::FIELD_STARTS]->
                            store_time(&et->starts, MYSQL_TIMESTAMP_DATETIME);
    }	   

    if (!et->ends_null)
    {
      table->field[Events::FIELD_ENDS]->set_notnull();
      table->field[Events::FIELD_ENDS]->
                            store_time(&et->ends, MYSQL_TIMESTAMP_DATETIME);
    }
  }
  else if (et->execute_at.year)
  {
    table->field[Events::FIELD_INTERVAL_EXPR]->set_null();
    table->field[Events::FIELD_TRANSIENT_INTERVAL]->set_null();
    table->field[Events::FIELD_STARTS]->set_null();
    table->field[Events::FIELD_ENDS]->set_null();
    
    table->field[Events::FIELD_EXECUTE_AT]->set_notnull();
    table->field[Events::FIELD_EXECUTE_AT]->
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
    
  ((Field_timestamp *)table->field[Events::FIELD_MODIFIED])->set_time();

  if (et->comment.str)
  {
    if (table->field[field_num= Events::FIELD_COMMENT]->
                 store(et->comment.str, et->comment.length, scs))
      goto err_truncate;
  }

  DBUG_RETURN(0);
err_truncate:
  my_error(ER_EVENT_DATA_TOO_LONG, MYF(0), table->field[field_num]->field_name);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
  Creates an event in mysql.event

  SYNOPSIS
    db_create_event()
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
db_create_event(THD *thd, Event_timed *et, my_bool create_if_not,
                uint *rows_affected)
{
  int ret= 0;
  CHARSET_INFO *scs= system_charset_info;
  TABLE *table;
  char old_db_buf[NAME_LEN+1];
  LEX_STRING old_db= { old_db_buf, sizeof(old_db_buf) };
  bool dbchanged= FALSE;
  DBUG_ENTER("db_create_event");
  DBUG_PRINT("enter", ("name: %.*s", et->name.length, et->name.str));

  *rows_affected= 0;
  DBUG_PRINT("info", ("open mysql.event for update"));
  if (Events::open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }

  DBUG_PRINT("info", ("check existance of an event with the same name"));
  if (!evex_db_find_event_aux(thd, et, table))
  {
    if (create_if_not)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_EVENT_ALREADY_EXISTS, ER(ER_EVENT_ALREADY_EXISTS),
                          et->name.str);
      goto ok;
    }
    my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), et->name.str);
    goto err;
  }

  DBUG_PRINT("info", ("non-existant, go forward"));
  if ((ret= sp_use_new_db(thd, et->dbname, &old_db, 0, &dbchanged)))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0));
    goto err;
  }

  restore_record(table, s->default_values);     // Get default values for fields

  if (system_charset_info->cset->numchars(system_charset_info, et->dbname.str,
                                    et->dbname.str + et->dbname.length)
                                    > EVEX_DB_FIELD_LEN)
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), et->dbname.str);
    goto err;
  }
  if (system_charset_info->cset->numchars(system_charset_info, et->name.str,
                                    et->name.str + et->name.length)
                                    > EVEX_DB_FIELD_LEN)
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), et->name.str);
    goto err;
  }

  if (et->body.length > table->field[Events::FIELD_BODY]->field_length)
  {
    my_error(ER_TOO_LONG_BODY, MYF(0), et->name.str);
    goto err;
  }

  if (!(et->expression) && !(et->execute_at.year))
  {
    DBUG_PRINT("error", ("neither expression nor execute_at are set!"));
    my_error(ER_EVENT_NEITHER_M_EXPR_NOR_M_AT, MYF(0));
    goto err;
  }

  ((Field_timestamp *)table->field[Events::FIELD_CREATED])->set_time();

  /*
    evex_fill_row() calls my_error() in case of error so no need to
    handle it here
  */
  if ((ret= evex_fill_row(thd, table, et, false)))
    goto err; 

  if (table->file->ha_write_row(table->record[0]))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->name.str, ret);
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
    (void) mysql_change_db(thd, old_db.str, 1);
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(EVEX_OK);

err:
  if (dbchanged)
    (void) mysql_change_db(thd, old_db.str, 1);
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
  Used to execute ALTER EVENT. Pendant to Events::update_event().

  SYNOPSIS
    db_update_event()
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

static int
db_update_event(THD *thd, Event_timed *et, sp_name *new_name)
{
  CHARSET_INFO *scs= system_charset_info;
  TABLE *table;
  int ret= EVEX_OPEN_TABLE_FAILED;
  DBUG_ENTER("db_update_event");
  DBUG_PRINT("enter", ("dbname: %.*s", et->dbname.length, et->dbname.str));
  DBUG_PRINT("enter", ("name: %.*s", et->name.length, et->name.str));
  DBUG_PRINT("enter", ("user: %.*s", et->definer.length, et->definer.str));
  if (new_name)
    DBUG_PRINT("enter", ("rename to: %.*s", new_name->m_name.length,
                                            new_name->m_name.str));

  if (Events::open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }
  
  /* first look whether we overwrite */
  if (new_name)
  {
    if (!sortcmp_lex_string(et->name, new_name->m_name, scs) &&
        !sortcmp_lex_string(et->dbname, new_name->m_db, scs))
    {
      my_error(ER_EVENT_SAME_NAME, MYF(0), et->name.str);
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
  if (EVEX_KEY_NOT_FOUND == evex_db_find_event_aux(thd, et, table))
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), et->name.str);
    goto err;
  }

  store_record(table,record[1]);

  /* Don't update create on row update. */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  /*
    evex_fill_row() calls my_error() in case of error so no need to
    handle it here
  */
  if ((ret= evex_fill_row(thd, table, et, true)))
    goto err;

  if (new_name)
  {    
    table->field[Events::FIELD_DB]->
      store(new_name->m_db.str, new_name->m_db.length, scs);
    table->field[Events::FIELD_NAME]->
      store(new_name->m_name.str, new_name->m_name.length, scs);
  }

  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->name.str, ret);
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
  Looks for a named event in mysql.event and in case of success returns
  an object will data loaded from the table.

  SYNOPSIS
    db_find_event()
      thd      THD
      name     the name of the event to find
      ett      event's data if event is found
      tbl      TABLE object to use when not NULL

  NOTES
    1) Use sp_name for look up, return in **ett if found
    2) tbl is not closed at exit

  RETURN VALUE
    0  ok     In this case *ett is set to the event
    #  error  *ett == 0
*/

int
db_find_event(THD *thd, sp_name *name, Event_timed **ett, TABLE *tbl,
              MEM_ROOT *root)
{
  TABLE *table;
  int ret;
  Event_timed *et= NULL;
  DBUG_ENTER("db_find_event");
  DBUG_PRINT("enter", ("name: %*s", name->m_name.length, name->m_name.str));

  if (!root)
    root= &evex_mem_root;

  if (tbl)
    table= tbl;
  else if (Events::open_event_table(thd, TL_READ, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    ret= EVEX_GENERAL_ERROR;
    goto done;
  }

  if ((ret= evex_db_find_event_by_name(thd, name->m_db, name->m_name, table)))
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name->m_name.str);
    goto done;    
  }
  et= new Event_timed;
  
  /*
    1)The table should not be closed beforehand.  ::load_from_row() only loads
      and does not compile

    2)::load_from_row() is silent on error therefore we emit error msg here
  */
  if ((ret= et->load_from_row(root, table)))
  {
    my_error(ER_CANNOT_LOAD_FROM_TABLE, MYF(0));
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


/*
  The function exported to the world for creating of events.

  SYNOPSIS
    Events::create_event()
      thd            THD
      et             event's data
      create_options Options specified when in the query. We are
                     interested whether there is IF NOT EXISTS
      rows_affected  How many rows were affected

  RETURN VALUE
    0   OK
    !0  Error

  NOTES
    - in case there is an event with the same name (db) and 
      IF NOT EXISTS is specified, an warning is put into the W stack.
*/

int
Events::create_event(THD *thd, Event_timed *et, uint create_options,
                     uint *rows_affected)
{
  int ret;

  DBUG_ENTER("Events::create_event");
  DBUG_PRINT("enter", ("name: %*s options:%d", et->name.length,
                et->name.str, create_options));

  if (!(ret = db_create_event(thd, et,
                             create_options & HA_LEX_CREATE_IF_NOT_EXISTS,
                             rows_affected)))
  {
    Event_scheduler *scheduler= Event_scheduler::get_instance();
    if (scheduler->initialized() &&
        (ret= scheduler->create_event(thd, et, true)))
      my_error(ER_EVENT_MODIFY_QUEUE_ERROR, MYF(0), ret);
  }
  /* No need to close the table, it will be closed in sql_parse::do_command */

  DBUG_RETURN(ret);
}


/*
  The function exported to the world for alteration of events.

  SYNOPSIS
    Events::update_event()
      thd        THD
      et         event's data
      new_name   set in case of RENAME TO.    

  RETURN VALUE
    0   OK
    !0  Error

  NOTES
    et contains data about dbname and event name. 
    new_name is the new name of the event, if not null (this means
    that RENAME TO was specified in the query)
*/

int
Events::update_event(THD *thd, Event_timed *et, sp_name *new_name,
                               uint *rows_affected)
{
  int ret;

  DBUG_ENTER("Events::update_event");
  DBUG_PRINT("enter", ("name: %*s", et->name.length, et->name.str));
  /*
    db_update_event() opens & closes the table to prevent
    crash later in the code when loading and compiling the new definition.
    Also on error conditions my_error() is called so no need to handle here
  */
  if (!(ret= db_update_event(thd, et, new_name)))
  {
    Event_scheduler *scheduler= Event_scheduler::get_instance();
    if (scheduler->initialized() &&
        (ret= scheduler->update_event(thd, et,
                                       new_name? &new_name->m_db: NULL,
                                       new_name? &new_name->m_name: NULL)))
      my_error(ER_EVENT_MODIFY_QUEUE_ERROR, MYF(0), ret);
  }
  DBUG_RETURN(ret);
}


/*
  Drops an event

  SYNOPSIS
    db_drop_event()
      thd             THD
      et              event's name
      drop_if_exists  if set and the event not existing => warning onto the stack
      rows_affected   affected number of rows is returned heres

  RETURN VALUE
    0   OK
    !0  Error (my_error() called)
*/

int db_drop_event(THD *thd, Event_timed *et, bool drop_if_exists,
                  uint *rows_affected)
{
  TABLE *table;
  Open_tables_state backup;
  int ret;

  DBUG_ENTER("db_drop_event");
  ret= EVEX_OPEN_TABLE_FAILED;

  thd->reset_n_backup_open_tables_state(&backup);
  if (Events::open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto done;
  }

  if (!(ret= evex_db_find_event_aux(thd, et, table)))
  {
    if ((ret= table->file->ha_delete_row(table->record[0])))
    { 	
      my_error(ER_EVENT_CANNOT_DELETE, MYF(0));
      goto done;
    }
  }
  else if (ret == EVEX_KEY_NOT_FOUND)
  { 
    if (drop_if_exists)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
                          "Event", et->name.str);
      ret= 0;
    } else
      my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), et->name.str);
    goto done;
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


/*
  Drops an event

  SYNOPSIS
    Events::drop_event()
      thd             THD
      et              event's name
      drop_if_exists  if set and the event not existing => warning onto the stack
      rows_affected   affected number of rows is returned heres

  RETURN VALUE
     0  OK
    !0  Error (reported)
*/

int
Events::drop_event(THD *thd, Event_timed *et, bool drop_if_exists,
                             uint *rows_affected)
{
  int ret;

  DBUG_ENTER("Events::drop_event");
  if (!(ret= db_drop_event(thd, et, drop_if_exists, rows_affected)))
  {
    Event_scheduler *scheduler= Event_scheduler::get_instance();
    if (scheduler->initialized() && (ret= scheduler->drop_event(thd, et)))
      my_error(ER_EVENT_MODIFY_QUEUE_ERROR, MYF(0), ret);
  }

  DBUG_RETURN(ret);
}


/*
  SHOW CREATE EVENT

  SYNOPSIS
    Events::show_create_event()
      thd        THD
      spn        the name of the event (db, name)

  RETURN VALUE
    0  OK
    1  Error during writing to the wire
*/

int
Events::show_create_event(THD *thd, sp_name *spn)
{
  int ret;
  Event_timed *et= NULL;
  Open_tables_state backup;

  DBUG_ENTER("evex_update_event");
  DBUG_PRINT("enter", ("name: %*s", spn->m_name.length, spn->m_name.str));

  thd->reset_n_backup_open_tables_state(&backup);
  ret= db_find_event(thd, spn, &et, NULL, thd->mem_root);
  thd->restore_backup_open_tables_state(&backup);

  if (!ret)
  {
    Protocol *protocol= thd->protocol;
    char show_str_buf[768];
    String show_str(show_str_buf, sizeof(show_str_buf), system_charset_info);
    List<Item> field_list;
    byte *sql_mode_str;
    ulong sql_mode_len=0;

    show_str.length(0);
    show_str.set_charset(system_charset_info);

    if (et->get_create_event(thd, &show_str))
      goto err;

    field_list.push_back(new Item_empty_string("Event", NAME_LEN));

    sql_mode_str=
      sys_var_thd_sql_mode::symbolic_mode_representation(thd, et->sql_mode,
                                                         &sql_mode_len);

    field_list.push_back(new Item_empty_string("sql_mode", sql_mode_len));

    field_list.push_back(new Item_empty_string("Create Event",
                                               show_str.length()));
    if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                           Protocol::SEND_EOF))
      goto err;

    protocol->prepare_for_resend();
    protocol->store(et->name.str, et->name.length, system_charset_info);

    protocol->store((char*) sql_mode_str, sql_mode_len, system_charset_info);

    protocol->store(show_str.c_ptr(), show_str.length(), system_charset_info);
    ret= protocol->write();
    send_eof(thd);
  }
  delete et;
  DBUG_RETURN(ret);
err:
  delete et;
  DBUG_RETURN(1);  
}


/*
  Drops all events from a schema

  SYNOPSIS
    Events::drop_schema_events()
      thd  Thread
      db   ASCIIZ schema name

  RETURN VALUE
    0   OK
    !0  Error
*/

int
Events::drop_schema_events(THD *thd, char *db)
{
  int ret= 0;
  LEX_STRING db_lex= {db, strlen(db)};
  
  DBUG_ENTER("evex_drop_db_events");  
  DBUG_PRINT("enter", ("dropping events from %s", db));

  Event_scheduler *scheduler= Event_scheduler::get_instance();
  if (scheduler->initialized())
    ret= scheduler->drop_schema_events(thd, &db_lex);
  else
    ret= db_drop_events_from_table(thd, &db_lex);

  DBUG_RETURN(ret);
}


/*
  Drops all events in the selected database, from mysql.event.

  SYNOPSIS
    evex_drop_db_events_from_table()
      thd  Thread
      db   Schema name

  RETURN VALUE
     0  OK
    !0  Error from ha_delete_row
*/

int
db_drop_events_from_table(THD *thd, LEX_STRING *db)
{
  int ret;
  TABLE *table;
  READ_RECORD read_record_info;
  DBUG_ENTER("db_drop_events_from_table");  
  DBUG_PRINT("info", ("dropping events from %s", db->str));

  if ((ret= Events::open_event_table(thd, TL_WRITE, &table)))
  {
    if (my_errno != ENOENT)
      sql_print_error("Table mysql.event is damaged. Got error %d on open",
                      my_errno);
    DBUG_RETURN(ret);
  }
  /* only enabled events are in memory, so we go now and delete the rest */
  init_read_record(&read_record_info, thd, table, NULL, 1, 0);
  while (!(read_record_info.read_record(&read_record_info)) && !ret)
  {
    char *et_db= get_field(thd->mem_root,
                           table->field[Events::FIELD_DB]);

    LEX_STRING et_db_lex= {et_db, strlen(et_db)};
    DBUG_PRINT("info", ("Current event %s.%s", et_db,
               get_field(thd->mem_root,
               table->field[Events::FIELD_NAME])));

    if (!sortcmp_lex_string(et_db_lex, *db, system_charset_info))
    {
      DBUG_PRINT("info", ("Dropping"));
      if ((ret= table->file->ha_delete_row(table->record[0])))
        my_error(ER_EVENT_DROP_FAILED, MYF(0),
                 get_field(thd->mem_root,
                           table->field[Events::FIELD_NAME]));
    }
  }
  end_read_record(&read_record_info);
  thd->version--;   /* Force close to free memory */

  close_thread_tables(thd);

  DBUG_RETURN(ret);
}



/*
  Inits the scheduler's structures.

  SYNOPSIS
    Events::init()

  NOTES
    This function is not synchronized.

  RETURN VALUE
    0  OK
    1  Error
*/

int
Events::init()
{
  int ret= 0;
  DBUG_ENTER("Events::init");

  /* it should be an assignment! */
  if (opt_event_scheduler)
  {
    Event_scheduler *scheduler= Event_scheduler::get_instance();
    DBUG_ASSERT(opt_event_scheduler == 1 || opt_event_scheduler == 2);
    DBUG_RETURN(scheduler->init() || 
                (opt_event_scheduler == 1? scheduler->start():
                                           scheduler->start_suspended()));
  }
  DBUG_RETURN(0);
}


/*
  Cleans up scheduler's resources. Called at server shutdown.

  SYNOPSIS
    Events::shutdown()

  NOTES
    This function is not synchronized.
*/

void
Events::shutdown()
{
  DBUG_ENTER("Events::shutdown");
  Event_scheduler *scheduler= Event_scheduler::get_instance();
  if (scheduler->initialized())
  {
    scheduler->stop();
    scheduler->destroy();
  }

  DBUG_VOID_RETURN;
}


/*
  Proxy for Event_scheduler::dump_internal_status

  SYNOPSIS
    Events::dump_internal_status()
      thd  Thread
  
  RETURN VALUE
    0  OK
    !0 Error
*/

int
Events::dump_internal_status(THD *thd)
{
  return Event_scheduler::dump_internal_status(thd);
}


/*
  Inits Events mutexes

  SYNOPSIS
    Events::init_mutexes()
      thd  Thread
*/

void
Events::init_mutexes()
{
  Event_scheduler::init_mutexes();
}


/*
  Destroys Events mutexes

  SYNOPSIS
    Events::destroy_mutexes()
*/

void
Events::destroy_mutexes()
{
  Event_scheduler::destroy_mutexes();
}
