/* Copyright (C) 2004-2005 MySQL AB

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

#include "event_priv.h"
#include "event.h"
#include "sp.h"

/*
 TODO list :
 - The default value of created/modified should not be 0000-00-00 because of
   STRICT mode restricions.

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

 - Maybe move all allocations during parsing to evex_mem_root thus saving
    double parsing in evex_create_event!

 - If the server is killed (stopping) try to kill executing events?
 
 - What happens if one renames an event in the DB while it is in memory?
   Or even deleting it?
  
 - Consider using conditional variable when doing shutdown instead of
   waiting till all worker threads end.
 
 - Make Event_timed::get_show_create_event() work

 - Add logging to file

 - Move comparison code to class Event_timed

Warning:
 - For now parallel execution is not possible because the same sp_head cannot
   be executed few times!!! There is still no lock attached to particular
   event.
*/


QUEUE EVEX_EQ_NAME;
MEM_ROOT evex_mem_root;
time_t mysql_event_last_create_time= 0L;


static TABLE_FIELD_W_TYPE event_table_fields[EVEX_FIELD_COUNT] = {
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


LEX_STRING interval_type_to_name[] = {
  {(char *) STRING_WITH_LEN("YEAR")},
  {(char *) STRING_WITH_LEN("QUARTER")},
  {(char *) STRING_WITH_LEN("MONTH")},
  {(char *) STRING_WITH_LEN("DAY")},
  {(char *) STRING_WITH_LEN("HOUR")},
  {(char *) STRING_WITH_LEN("MINUTE")},
  {(char *) STRING_WITH_LEN("WEEK")},
  {(char *) STRING_WITH_LEN("SECOND")},
  {(char *) STRING_WITH_LEN("MICROSECOND")},
  {(char *) STRING_WITH_LEN("YEAR_MONTH")},
  {(char *) STRING_WITH_LEN("DAY_HOUR")},
  {(char *) STRING_WITH_LEN("DAY_MINUTE")},
  {(char *) STRING_WITH_LEN("DAY_SECOND")},
  {(char *) STRING_WITH_LEN("HOUR_MINUTE")},
  {(char *) STRING_WITH_LEN("HOUR_SECOND")},
  {(char *) STRING_WITH_LEN("MINUTE_SECOND")},
  {(char *) STRING_WITH_LEN("DAY_MICROSECOND")},
  {(char *) STRING_WITH_LEN("HOUR_MICROSECOND")},
  {(char *) STRING_WITH_LEN("MINUTE_MICROSECOND")},
  {(char *) STRING_WITH_LEN("SECOND_MICROSECOND")}
}; 



/*
  Inits the scheduler queue - prioritized queue from mysys/queue.c

  Synopsis
    evex_queue_init()

      queue - pointer the the memory to be initialized as queue. has to be
              allocated from the caller

  Notes
    During initialization the queue is sized for 30 events, and when is full
    will auto extent with 30.
*/

void
evex_queue_init(EVEX_QUEUE_TYPE *queue)
{
  if (init_queue_ex(queue, 30 /*num_el*/, 0 /*offset*/, 0 /*smallest_on_top*/,
                    event_timed_compare_q, NULL, 30 /*auto_extent*/))
    sql_print_error("Insufficient memory to initialize executing queue.");
}


/*
  Compares 2 LEX strings regarding case.

  Synopsis
    my_time_compare()

      s - first LEX_STRING
      t - second LEX_STRING
      cs - charset

  RETURNS:
   -1   - s < t
    0   - s == t
    1   - s > t

  Notes
    TIME.second_part is not considered during comparison
*/

int sortcmp_lex_string(LEX_STRING s, LEX_STRING t, CHARSET_INFO *cs)
{
 return cs->coll->strnncollsp(cs, (unsigned char *) s.str,s.length,
                                  (unsigned char *) t.str,t.length, 0);
}


/*
  Compares 2 TIME structures

  Synopsis
    my_time_compare()

      a - first TIME
      b - second time

  RETURNS:
   -1   - a < b
    0   - a == b
    1   - a > b

  Notes
    TIME.second_part is not considered during comparison
*/

int
my_time_compare(TIME *a, TIME *b)
{

#ifdef ENABLE_WHEN_WE_HAVE_MILLISECOND_IN_TIMESTAMPS
  my_ulonglong a_t= TIME_to_ulonglong_datetime(a)*100L + a->second_part;
  my_ulonglong b_t= TIME_to_ulonglong_datetime(b)*100L + b->second_part;
#else
  my_ulonglong a_t= TIME_to_ulonglong_datetime(a);
  my_ulonglong b_t= TIME_to_ulonglong_datetime(b);
#endif

  if (a_t > b_t)
    return 1;
  else if (a_t < b_t)
    return -1;

  return 0;
}


/*
  Compares the execute_at members of 2 Event_timed instances

  Synopsis
    event_timed_compare()

      a - first Event_timed object
      b - second Event_timed object

  RETURNS:
   -1   - a->execute_at < b->execute_at
    0   - a->execute_at == b->execute_at
    1   - a->execute_at > b->execute_at

  Notes
    execute_at.second_part is not considered during comparison
*/

int
event_timed_compare(Event_timed *a, Event_timed *b)
{
  return my_time_compare(&a->execute_at, &b->execute_at);
}


/*
  Compares the execute_at members of 2 Event_timed instances.
  Used as callback for the prioritized queue when shifting
  elements inside.

  Synopsis
    event_timed_compare()
  
      vptr - not used (set it to NULL)
      a    - first Event_timed object
      b    - second Event_timed object

  RETURNS:
   -1   - a->execute_at < b->execute_at
    0   - a->execute_at == b->execute_at
    1   - a->execute_at > b->execute_at
    
  Notes
    execute_at.second_part is not considered during comparison
*/

int 
event_timed_compare_q(void *vptr, byte* a, byte *b)
{
  return event_timed_compare((Event_timed *)a, (Event_timed *)b);
}


/*
  Reconstructs interval expression from interval type and expression
  value that is in form of a value of the smalles entity:
  For
    YEAR_MONTH - expression is in months
    DAY_MINUTE - expression is in minutes

  Synopsis
    event_reconstruct_interval_expression()
      buf - preallocated String buffer to add the value to
      interval - the interval type (for instance YEAR_MONTH)
      expression - the value in the lowest entity

  RETURNS
   0 - OK
   1 - Error
*/

int
event_reconstruct_interval_expression(String *buf,
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
    evex_open_event_table()
    thd         Thread context
    lock_type   How to lock the table
    table       We will store the open table here

  RETURN
    1   Cannot lock table
    2   The table is corrupted - different number of fields
    0   OK
*/

int
evex_open_event_table(THD *thd, enum thr_lock_type lock_type, TABLE **table)
{
  TABLE_LIST tables;
  DBUG_ENTER("open_proc_table");

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "event";
  tables.lock_type= lock_type;

  if (simple_open_n_lock_tables(thd, &tables))
    DBUG_RETURN(1);
  
  if (table_check_intact(tables.table, EVEX_FIELD_COUNT, event_table_fields,
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
      et     evet_timed object containing dbname, name & definer
      table  TABLE object for open mysql.event table.

  RETURN VALUE
    0                  - Routine found
    EVEX_KEY_NOT_FOUND - No routine with given name
*/

inline int
evex_db_find_event_aux(THD *thd, Event_timed *et, TABLE *table)
{
  return evex_db_find_event_by_name(thd, et->dbname, et->name,
                                    et->definer, table);
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
                          const LEX_STRING user_name,
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
  if (dbname.length > table->field[EVEX_FIELD_DB]->field_length ||
      ev_name.length > table->field[EVEX_FIELD_NAME]->field_length ||
      user_name.length > table->field[EVEX_FIELD_DEFINER]->field_length)
      
    DBUG_RETURN(EVEX_KEY_NOT_FOUND);

  table->field[EVEX_FIELD_DB]->store(dbname.str, dbname.length, &my_charset_bin);
  table->field[EVEX_FIELD_NAME]->store(ev_name.str, ev_name.length,
                                       &my_charset_bin);
  table->field[EVEX_FIELD_DEFINER]->store(user_name.str, user_name.length,
                                          &my_charset_bin);

  key_copy(key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx(table->record[0], 0, key,
                                 table->key_info->key_length,
                                  HA_READ_KEY_EXACT))
    DBUG_RETURN(EVEX_KEY_NOT_FOUND);

  DBUG_RETURN(0);
}


/*
   Puts some data common to CREATE and ALTER EVENT into a row.

   SYNOPSIS
     evex_fill_row()
       thd    THD
       table  the row to fill out
       et     Event's data

   Returns
     0 - ok
     EVEX_GENERAL_ERROR    - bad data
     EVEX_GET_FIELD_FAILED - field count does not match. table corrupted?

   DESCRIPTION 
     Used both when an event is created and when it is altered.
*/

static int
evex_fill_row(THD *thd, TABLE *table, Event_timed *et, my_bool is_update)
{
  enum evex_table_field field_num;

  DBUG_ENTER("evex_fill_row");

  DBUG_PRINT("info", ("dbname=[%s]", et->dbname.str));
  DBUG_PRINT("info", ("name  =[%s]", et->name.str));
  DBUG_PRINT("info", ("body  =[%s]", et->body.str));

  if (table->field[field_num= EVEX_FIELD_DB]->
                  store(et->dbname.str, et->dbname.length, system_charset_info))
    goto trunc_err;

  if (table->field[field_num= EVEX_FIELD_NAME]->
                  store(et->name.str, et->name.length, system_charset_info))
    goto trunc_err;

  /* both ON_COMPLETION and STATUS are NOT NULL thus not calling set_notnull() */
  table->field[EVEX_FIELD_ON_COMPLETION]->store((longlong)et->on_completion,
                                                true);

  table->field[EVEX_FIELD_STATUS]->store((longlong)et->status, true);

  /*
    Change the SQL_MODE only if body was present in an ALTER EVENT and of course
    always during CREATE EVENT.
  */ 
  if (et->body.str)
  {
    table->field[EVEX_FIELD_SQL_MODE]->store((longlong)thd->variables.sql_mode,
                                             true);

    if (table->field[field_num= EVEX_FIELD_BODY]->
                     store(et->body.str, et->body.length, system_charset_info))
      goto trunc_err;
  }

  if (et->expression)
  {
    table->field[EVEX_FIELD_INTERVAL_EXPR]->set_notnull();
    table->field[EVEX_FIELD_INTERVAL_EXPR]->store((longlong)et->expression,true);

    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->set_notnull();
    /*
      In the enum (C) intervals start from 0 but in mysql enum valid values start
      from 1. Thus +1 offset is needed!
    */
    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->store((longlong)et->interval+1,
                                                       true);

    table->field[EVEX_FIELD_EXECUTE_AT]->set_null();

    if (!et->starts_null)
    {
      table->field[EVEX_FIELD_STARTS]->set_notnull();
      table->field[EVEX_FIELD_STARTS]->
                            store_time(&et->starts, MYSQL_TIMESTAMP_DATETIME);
    }	   

    if (!et->ends_null)
    {
      table->field[EVEX_FIELD_ENDS]->set_notnull();
      table->field[EVEX_FIELD_ENDS]->
                            store_time(&et->ends, MYSQL_TIMESTAMP_DATETIME);
    }
  }
  else if (et->execute_at.year)
  {
    table->field[EVEX_FIELD_INTERVAL_EXPR]->set_null();
    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->set_null();
    table->field[EVEX_FIELD_STARTS]->set_null();
    table->field[EVEX_FIELD_ENDS]->set_null();
    
    table->field[EVEX_FIELD_EXECUTE_AT]->set_notnull();
    table->field[EVEX_FIELD_EXECUTE_AT]->store_time(&et->execute_at,
                                                    MYSQL_TIMESTAMP_DATETIME);
  }
  else
  {
    DBUG_ASSERT(is_update);
    /*
      it is normal to be here when the action is update
      this is an error if the action is create. something is borked
    */
  }
    
  ((Field_timestamp *)table->field[EVEX_FIELD_MODIFIED])->set_time();

  if (et->comment.str)
  {
    if (table->field[field_num= EVEX_FIELD_COMMENT]->store(et->comment.str,
                                                           et->comment.length,
                                                           system_charset_info))
      goto trunc_err;
  }

  DBUG_RETURN(0);
trunc_err:
  my_error(ER_EVENT_DATA_TOO_LONG, MYF(0), table->field[field_num]->field_name);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
   Creates an event in mysql.event

   SYNOPSIS
     db_create_event()
       thd             THD
       et              Event_timed object containing information for the event
       create_if_not - if an warning should be generated in case event exists
       rows_affected - how many rows were affected

     Return value
                        0 - OK
       EVEX_GENERAL_ERROR - Failure
   DESCRIPTION 
     Creates an event. Relies on evex_fill_row which is shared with
     db_update_event. The name of the event is inside "et".
*/

static int
db_create_event(THD *thd, Event_timed *et, my_bool create_if_not,
                uint *rows_affected)
{
  int ret= 0;
  TABLE *table;
  char olddb[128];
  bool dbchanged= false;
  DBUG_ENTER("db_create_event");
  DBUG_PRINT("enter", ("name: %.*s", et->name.length, et->name.str));

  *rows_affected= 0;
  DBUG_PRINT("info", ("open mysql.event for update"));
  if (evex_open_event_table(thd, TL_WRITE, &table))
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
  if ((ret= sp_use_new_db(thd, et->dbname.str,olddb, sizeof(olddb),0,
                          &dbchanged)))
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

  if (et->body.length > table->field[EVEX_FIELD_BODY]->field_length)
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

  if ((ret=table->field[EVEX_FIELD_DEFINER]->store(et->definer.str,
                                                   et->definer.length,
                                                   system_charset_info)))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->name.str, ret);
    goto err;
  }

  ((Field_timestamp *)table->field[EVEX_FIELD_CREATED])->set_time();

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
    thd->binlog_query(THD::MYSQL_QUERY_TYPE,
                      thd->query, thd->query_length, FALSE, FALSE);
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
   Used to execute ALTER EVENT. Pendant to evex_update_event().

   SYNOPSIS
     db_update_event()
       thd      THD
       sp_name  the name of the event to alter
       et       event's data

   NOTES
     sp_name is passed since this is the name of the event to
     alter in case of RENAME TO.
*/

static int
db_update_event(THD *thd, Event_timed *et, sp_name *new_name)
{
  TABLE *table;
  int ret= EVEX_OPEN_TABLE_FAILED;
  DBUG_ENTER("db_update_event");
  DBUG_PRINT("enter", ("dbname: %.*s", et->dbname.length, et->dbname.str));
  DBUG_PRINT("enter", ("name: %.*s", et->name.length, et->name.str));
  DBUG_PRINT("enter", ("user: %.*s", et->name.length, et->name.str));
  if (new_name)
    DBUG_PRINT("enter", ("rename to: %.*s", new_name->m_name.length,
                                            new_name->m_name.str));

  if (evex_open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }
  
  /* first look whether we overwrite */
  if (new_name)
  {
    if (!sortcmp_lex_string(et->name, new_name->m_name, system_charset_info) &&
        !sortcmp_lex_string(et->dbname, new_name->m_db, system_charset_info))
    {
      my_error(ER_EVENT_SAME_NAME, MYF(0), et->name.str);
      goto err;    
    }
  
    if (!evex_db_find_event_by_name(thd, new_name->m_db, new_name->m_name,
                                et->definer, table))
    {
      my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), new_name->m_name.str);
      goto err;
    }  
  }
  /*
    ...and then whether there is such an event. don't exchange the blocks
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

  /* evex_fill_row() calls my_error() in case of error so no need to handle it here */
  if ((ret= evex_fill_row(thd, table, et, true)))
    goto err;

  if (new_name)
  {    
    table->field[EVEX_FIELD_DB]->
      store(new_name->m_db.str, new_name->m_db.length, system_charset_info);
    table->field[EVEX_FIELD_NAME]->
      store(new_name->m_name.str, new_name->m_name.length, system_charset_info);
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
       definer  who owns the event
       ett      event's data if event is found
       tbl      TABLE object to use when not NULL

   NOTES
     1) Use sp_name for look up, return in **ett if found
     2) tbl is not closed at exit

   RETURN
     0  ok     In this case *ett is set to the event
     #  error  *ett == 0
*/

static int
db_find_event(THD *thd, sp_name *name, LEX_STRING *definer, Event_timed **ett,
              TABLE *tbl, MEM_ROOT *root)
{
  TABLE *table;
  int ret;
  Event_timed *et= 0;
  DBUG_ENTER("db_find_event");
  DBUG_PRINT("enter", ("name: %*s", name->m_name.length, name->m_name.str));

  if (!root)
    root= &evex_mem_root;

  if (tbl)
    table= tbl;
  else if (evex_open_event_table(thd, TL_READ, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    ret= EVEX_GENERAL_ERROR;
    goto done;
  }

  if ((ret= evex_db_find_event_by_name(thd, name->m_db, name->m_name, *definer,
                                       table)))
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
   Looks for a named event in mysql.event and then loads it from 
   the table, compiles it and insert it into the cache.

   SYNOPSIS
     evex_load_and_compile_event()
       thd       THD
       spn       the name of the event to alter
       definer   who is the owner
       use_lock  whether to obtain a lock on LOCK_event_arrays or not

   RETURN VALUE
       0   - OK
       < 0 - error (in this case underlying functions call my_error()).
*/

static int
evex_load_and_compile_event(THD * thd, sp_name *spn, LEX_STRING definer,
                            bool use_lock)
{
  int ret= 0;
  MEM_ROOT *tmp_mem_root;
  Event_timed *ett;
  Open_tables_state backup;

  DBUG_ENTER("db_load_and_compile_event");
  DBUG_PRINT("enter", ("name: %*s", spn->m_name.length, spn->m_name.str));

  tmp_mem_root= thd->mem_root;
  thd->mem_root= &evex_mem_root;

  thd->reset_n_backup_open_tables_state(&backup);
  /* no need to use my_error() here because db_find_event() has done it */
  ret= db_find_event(thd, spn, &definer, &ett, NULL, NULL);
  thd->restore_backup_open_tables_state(&backup);
  if (ret)
    goto done;
  
  ett->compute_next_execution_time();
  if (use_lock)
    VOID(pthread_mutex_lock(&LOCK_event_arrays));

  evex_queue_insert(&EVEX_EQ_NAME, (EVEX_PTOQEL) ett);

  /*
    There is a copy in the array which we don't need. sphead won't be
    destroyed.
  */

  if (use_lock)
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));

done:
  if (thd->mem_root != tmp_mem_root)
    thd->mem_root= tmp_mem_root;  

  DBUG_RETURN(ret);
}


/*
  Removes from queue in memory the event which is identified by the tupple
  (db, name).

   SYNOPSIS
     evex_remove_from_cache()
  
       db       - db name
       name     - event name
       use_lock - whether to lock the mutex LOCK_event_arrays or not in case it
                  has been already locked outside
       is_drop  - if an event is currently being executed then we can also delete
                  the Event_timed instance, so we alarm the event that it should
                  drop itself if this parameter is set to TRUE. It's false on
                  ALTER EVENT.

   RETURNS
     0  OK (always)
*/

static int
evex_remove_from_cache(LEX_STRING *db, LEX_STRING *name, bool use_lock,
                       bool is_drop)
{
  //ToDo : Add definer to the tuple (db, name) to become triple
  uint i;
  int ret= 0;

  DBUG_ENTER("evex_remove_from_cache");
  /*
    It is possible that 2 (or 1) pass(es) won't find the event in memory.
    The reason is that DISABLED events are not cached.
  */

  if (use_lock)
    VOID(pthread_mutex_lock(&LOCK_event_arrays));

  for (i= 0; i < evex_queue_num_elements(EVEX_EQ_NAME); ++i)
  {
    Event_timed *et= evex_queue_element(&EVEX_EQ_NAME, i, Event_timed*);
    DBUG_PRINT("info", ("[%s.%s]==[%s.%s]?",db->str,name->str, et->dbname.str,
                        et->name.str));
    if (!sortcmp_lex_string(*name, et->name, system_charset_info) &&
        !sortcmp_lex_string(*db, et->dbname, system_charset_info))
    {
      if (et->can_spawn_now())
      {
        DBUG_PRINT("evex_remove_from_cache", ("not running - free and delete"));
        et->free_sp();
        delete et;
      }
      else
      {
        DBUG_PRINT("evex_remove_from_cache",
               ("running.defer mem free. is_drop=%d", is_drop));
        et->flags|= EVENT_EXEC_NO_MORE;
        et->dropped= is_drop;
      }
      DBUG_PRINT("evex_remove_from_cache", ("delete from queue"));
      evex_queue_delete_element(&EVEX_EQ_NAME, i);
      /* ok, we have cleaned */
      ret= 0;
      goto done;
    }
  }

done:
  if (use_lock)
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));

  DBUG_RETURN(ret);
}


/*
   The function exported to the world for creating of events.

   SYNOPSIS
     evex_create_event()
       thd            THD
       et             event's data
       create_options Options specified when in the query. We are
                      interested whether there is IF NOT EXISTS
       rows_affected  How many rows were affected

   NOTES
     - in case there is an event with the same name (db) and 
       IF NOT EXISTS is specified, an warning is put into the W stack.
*/

int
evex_create_event(THD *thd, Event_timed *et, uint create_options,
                  uint *rows_affected)
{
  int ret = 0;

  DBUG_ENTER("evex_create_event");
  DBUG_PRINT("enter", ("name: %*s options:%d", et->name.length,
                et->name.str, create_options));

  if ((ret = db_create_event(thd, et,
                             create_options & HA_LEX_CREATE_IF_NOT_EXISTS,
                             rows_affected)))
    goto done;

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (evex_is_running && et->status == MYSQL_EVENT_ENABLED)
  {
    sp_name spn(et->dbname, et->name);
    ret= evex_load_and_compile_event(thd, &spn, et->definer, true);
  }
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

done:
  /* No need to close the table, it will be closed in sql_parse::do_command */

  DBUG_RETURN(ret);
}


/*
   The function exported to the world for alteration of events.

   SYNOPSIS
     evex_update_event()
       thd        THD
       et         event's data
       new_name   set in case of RENAME TO.    

   NOTES
     et contains data about dbname and event name. 
     new_name is the new name of the event, if not null (this means
     that RENAME TO was specified in the query)
*/

int
evex_update_event(THD *thd, Event_timed *et, sp_name *new_name,
                  uint *rows_affected)
{
  int ret;
  bool need_second_pass= true;

  DBUG_ENTER("evex_update_event");
  DBUG_PRINT("enter", ("name: %*s", et->name.length, et->name.str));

  /*
    db_update_event() opens & closes the table to prevent
    crash later in the code when loading and compiling the new definition.
    Also on error conditions my_error() is called so no need to handle here
  */
  if ((ret= db_update_event(thd, et, new_name)))
    goto done;

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
    UNLOCK_MUTEX_AND_BAIL_OUT(LOCK_evex_running, done);

  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  evex_remove_from_cache(&et->dbname, &et->name, false, false);
  if (et->status == MYSQL_EVENT_ENABLED)
  {
    if (new_name)
      ret= evex_load_and_compile_event(thd, new_name, et->definer, false);
    else
    {
      sp_name spn(et->dbname, et->name);
      ret= evex_load_and_compile_event(thd, &spn, et->definer, false);
    }
    if (ret == EVEX_COMPILE_ERROR)
      my_error(ER_EVENT_COMPILE_ERROR, MYF(0));
  }
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

done:
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
  if (evex_open_event_table(thd, TL_WRITE, &table))
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
   evex_drop_event()
     thd             THD
     et              event's name
     drop_if_exists  if set and the event not existing => warning onto the stack
     rows_affected   affected number of rows is returned heres
          
*/

int
evex_drop_event(THD *thd, Event_timed *et, bool drop_if_exists,
                uint *rows_affected)
{
  int ret= 0;

  DBUG_ENTER("evex_drop_event");


  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (evex_is_running)
    ret= evex_remove_from_cache(&et->dbname, &et->name, true, true);
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  if (ret == 1)
    ret= 0;
  else if (ret == 0)   
    ret= db_drop_event(thd, et, drop_if_exists, rows_affected);
  else
    my_error(ER_UNKNOWN_ERROR, MYF(0));

  DBUG_RETURN(ret);
}


/*
   SHOW CREATE EVENT

   SYNOPSIS
     evex_show_create_event()
       thd        THD
       spn        the name of the event (db, name)
       definer    the definer of the event

   RETURNS
     0  -  OK
     1  - Error during writing to the wire
*/

int
evex_show_create_event(THD *thd, sp_name *spn, LEX_STRING definer)
{
  int ret;
  Event_timed *et= NULL;
  Open_tables_state backup;

  DBUG_ENTER("evex_update_event");
  DBUG_PRINT("enter", ("name: %*s", spn->m_name.length, spn->m_name.str));

  thd->reset_n_backup_open_tables_state(&backup);
  ret= db_find_event(thd, spn, &definer, &et, NULL, thd->mem_root);
  thd->restore_backup_open_tables_state(&backup);

  if (et)
  {
    Protocol *protocol= thd->protocol;
    char show_str_buf[768];
    String show_str(show_str_buf, sizeof(show_str_buf), system_charset_info);
    List<Item> field_list;
    byte *sql_mode_str;
    ulong sql_mode_len=0;

    show_str.length(0);

    if (et->get_create_event(thd, &show_str))
    {
      delete et;
      DBUG_RETURN(1);
    }

    field_list.push_back(new Item_empty_string("Event", NAME_LEN));

    sql_mode_str=
      sys_var_thd_sql_mode::symbolic_mode_representation(thd, et->sql_mode,
                                                         &sql_mode_len);

    field_list.push_back(new Item_empty_string("sql_mode", sql_mode_len));

    field_list.push_back(new Item_empty_string("Create Event",
                                               show_str.length()));
    if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                           Protocol::SEND_EOF))
    {
      delete et;
      DBUG_RETURN(1);
    }
    protocol->prepare_for_resend();
    protocol->store(et->name.str, et->name.length, system_charset_info);

    protocol->store((char*) sql_mode_str, sql_mode_len, system_charset_info);

    protocol->store(show_str.ptr(), show_str.length(), system_charset_info);
    ret= protocol->write();
    send_eof(thd);
    delete et;
  }

  DBUG_RETURN(ret);
}


/*
  evex_drop_db_events - Drops all events in the selected database

  thd  - Thread
  db   - ASCIIZ the name of the database
  
  Returns:
    0  - OK
    1  - Failed to delete a specific row
    2  - Got NULL while reading db name from a row

  Note:
    The algo is the following
    1. Go through the in-memory cache, if the scheduler is working
       and for every event whose dbname matches the database we drop
       check whether is currently in execution:
       - Event_timed::can_spawn() returns true -> the event is not
         being executed in a child thread. The reason not to use
         Event_timed::is_running() is that the latter shows only if
         it is being executed, which is 99% of the time in the thread
         but there are some initiliazations before and after the
         anonymous SP is being called. So if we delete in this moment
         -=> *boom*, so we have to check whether the thread has been
         spawned and can_spawn() is the right method.
       - Event_timed::can_spawn() returns false -> being runned ATM
         just set the flags so it should drop itself.
*/

int
evex_drop_db_events(THD *thd, char *db)
{
  TABLE *table;
  READ_RECORD read_record_info;
  int ret= 0;
  uint i;
  LEX_STRING db_lex= {db, strlen(db)};
  
  DBUG_ENTER("evex_drop_db_events");  
  DBUG_PRINT("info",("dropping events from %s", db));

  VOID(pthread_mutex_lock(&LOCK_event_arrays));

  if ((ret= evex_open_event_table(thd, TL_WRITE, &table)))
  {
    if (errno != ENOENT)
      sql_print_error("Table mysql.event is damaged. Got errno: %d on open",
                      my_errno);
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);
  }

  DBUG_PRINT("info",("%d elements in the queue",
             evex_queue_num_elements(EVEX_EQ_NAME)));
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
    goto skip_memory;

  for (i= 0; i < evex_queue_num_elements(EVEX_EQ_NAME); ++i)
  {
    Event_timed *et= evex_queue_element(&EVEX_EQ_NAME, i, Event_timed*);
    if (sortcmp_lex_string(et->dbname, db_lex, system_charset_info))
      continue;

    if (et->can_spawn_now_n_lock(thd))
    {
      DBUG_PRINT("info",("event %s not running - direct delete", et->name.str));
      if (!(ret= evex_db_find_event_aux(thd, et, table)))
      {
        DBUG_PRINT("info",("event %s found on disk", et->name.str));
        if ((ret= table->file->ha_delete_row(table->record[0])))
        {
          sql_print_error("Error while deleting a row - dropping "
                          "a database. Skipping the rest.");
          my_error(ER_EVENT_DROP_FAILED, MYF(0), et->name.str);
          goto end;
        }
        DBUG_PRINT("info",("deleted event [%s] num [%d]. Time to free mem",
                   et->name.str, i));
      }
      else if (ret == EVEX_KEY_NOT_FOUND)
      {
        sql_print_error("Expected to find event %s.%s of %s on disk-not there.",
                        et->dbname.str, et->name.str, et->definer.str);
      }
      et->free_sp();
      delete et;
      et= 0;
      /* no need to call et->spawn_unlock because we already cleaned et */
    }
    else
    {
      DBUG_PRINT("info",("event %s is running. setting exec_no_more and dropped",
                  et->name.str));
      et->flags|= EVENT_EXEC_NO_MORE;
      et->dropped= TRUE;
    }
    DBUG_PRINT("info",("%d elements in the queue",
               evex_queue_num_elements(EVEX_EQ_NAME)));
    evex_queue_delete_element(&EVEX_EQ_NAME, i);// 0 is top
    DBUG_PRINT("info",("%d elements in the queue",
               evex_queue_num_elements(EVEX_EQ_NAME)));
    /*
      decrease so we start at the same position, there will be
      less elements in the queue, it will still be ordered so on
      next iteration it will be again i the current element or if
      no more we finish.
    */
    --i;
  }

skip_memory:
  /*
   The reasoning behind having two loops is the following:
   If there was only one loop, the table-scan, then for every element which
   matches, the queue in memory has to be searched to remove the element.
   While if we go first over the queue and remove what's in there we have only
   one pass over it and after finishing it, moving to table-scan for the disabled
   events. This needs quite less time and means quite less locking on
   LOCK_event_arrays.
  */
  DBUG_PRINT("info",("Mem-cache checked, now going to db for disabled events"));
  /* only enabled events are in memory, so we go now and delete the rest */
  init_read_record(&read_record_info, thd, table ,NULL,1,0);
  while (!(read_record_info.read_record(&read_record_info)) && !ret)
  {
    char *et_db;

    if ((et_db= get_field(thd->mem_root, table->field[EVEX_FIELD_DB])) == NULL)
    {
      ret= 2;
      break;
    }
    
    LEX_STRING et_db_lex= {et_db, strlen(et_db)};
    if (!sortcmp_lex_string(et_db_lex, db_lex, system_charset_info))
    {
      Event_timed ett;
      char *ptr;
      
      if ((ptr= get_field(thd->mem_root, table->field[EVEX_FIELD_STATUS]))
           == NullS)
      {
        sql_print_error("Error while loading from mysql.event. "
                        "Table probably corrupted");
        goto end;
      }
      /*
        When not running nothing is in memory so we have to clean
        everything.
        We don't delete EVENT_ENABLED events when the scheduler is running
        because maybe this is an event which we asked to drop itself when
        it is finished and it hasn't finished yet, so we don't touch it.
        It will drop itself. The not running ENABLED events has been already
        deleted from ha_delete_row() above in the loop over the QUEUE
        (in case the executor is running).
        'D' stands for DISABLED, 'E' for ENABLED - it's an enum
      */
      if ((evex_is_running && ptr[0] == 'D') || !evex_is_running)
      {
        DBUG_PRINT("info", ("Dropping %s.%s", et_db, ett.name.str));
        if ((ret= table->file->ha_delete_row(table->record[0])))
        {
          my_error(ER_EVENT_DROP_FAILED, MYF(0), ett.name.str);
          goto end;
        }
      }
    }
  }
  DBUG_PRINT("info",("Disk checked for disabled events. Finishing."));

end:
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));
  end_read_record(&read_record_info);

  thd->version--;   /* Force close to free memory */

  close_thread_tables(thd);

  DBUG_RETURN(ret);
}
