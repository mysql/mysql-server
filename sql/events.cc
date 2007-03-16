/* Copyright (C) 2004-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include "events.h"
#include "event_data_objects.h"
#include "event_db_repository.h"
#include "event_queue.h"
#include "event_scheduler.h"
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

*/


/*
  If the user (un)intentionally removes an event directly from mysql.event
  the following sequence has to be used to be able to remove the in-memory
  counterpart.
  1. CREATE EVENT the_name ON SCHEDULE EVERY 1 SECOND DISABLE DO SELECT 1;
  2. DROP EVENT the_name
  
  In other words, the first one will create a row in mysql.event . In the
  second step because there will be a line, disk based drop will pass and
  the scheduler will remove the memory counterpart. The reason is that
  in-memory queue does not check whether the event we try to drop from memory
  is disabled. Disabled events are not kept in-memory because they are not
  eligible for execution.
*/

/*
  Keep the order of the first to as in var_typelib
  sys_var_event_scheduler::value_ptr() references this array. Keep in
  mind!
*/
static const char *opt_event_scheduler_state_names[]=
    { "OFF", "ON", "0", "1", "DISABLED", NullS };

TYPELIB Events::opt_typelib=
{
  array_elements(opt_event_scheduler_state_names)-1,
  "",
  opt_event_scheduler_state_names,
  NULL
};


/*
  The order should not be changed. We consider OFF to be equivalent of INT 0
  And ON of 1. If OFF & ON are interchanged the logic in
  sys_var_event_scheduler::update() will be broken!
*/
static const char *var_event_scheduler_state_names[]= { "OFF", "ON", NullS };

TYPELIB Events::var_typelib=
{
  array_elements(var_event_scheduler_state_names)-1,
  "",
  var_event_scheduler_state_names,
  NULL
};


static
Event_queue events_event_queue;

static
Event_scheduler events_event_scheduler;


Event_db_repository events_event_db_repository;

Events Events::singleton;

enum Events::enum_opt_event_scheduler Events::opt_event_scheduler=
     Events::EVENTS_OFF;


/*
  Compares 2 LEX strings regarding case.

  SYNOPSIS
    sortcmp_lex_string()
      s   First LEX_STRING
      t   Second LEX_STRING
      cs  Charset

  RETURN VALUE
   -1   s < t
    0   s == t
    1   s > t
*/

int sortcmp_lex_string(LEX_STRING s, LEX_STRING t, CHARSET_INFO *cs)
{
 return cs->coll->strnncollsp(cs, (uchar *) s.str,s.length,
                                  (uchar *) t.str,t.length, 0);
}


/*
  Accessor for the singleton instance.

  SYNOPSIS
    Events::get_instance()

  RETURN VALUE
    address
*/

Events *
Events::get_instance()
{
  DBUG_ENTER("Events::get_instance");
  DBUG_RETURN(&singleton);
}


/*
  Reconstructs interval expression from interval type and expression
  value that is in form of a value of the smalles entity:
  For
    YEAR_MONTH - expression is in months
    DAY_MINUTE - expression is in minutes

  SYNOPSIS
    Events::reconstruct_interval_expression()
      buf         Preallocated String buffer to add the value to
      interval    The interval type (for instance YEAR_MONTH)
      expression  The value in the lowest entity

  RETURN VALUE
    0  OK
    1  Error
*/

int
Events::reconstruct_interval_expression(String *buf, interval_type interval,
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
  Constructor of Events class. It's called when events.o
  is loaded. Assigning addressed of static variables in this
  object file.

  SYNOPSIS
    Events::Events()
*/

Events::Events()
{
  scheduler=     &events_event_scheduler;
  event_queue=   &events_event_queue;
  db_repository= &events_event_db_repository;
}


/*
  The function exported to the world for creating of events.

  SYNOPSIS
    Events::create_event()
      thd            [in]  THD
      parse_data     [in]  Event's data from parsing stage
      if_not_exists  [in]  Whether IF NOT EXISTS was specified in the DDL

  RETURN VALUE
    FALSE  OK
    TRUE   Error (Reported)

  NOTES
    In case there is an event with the same name (db) and 
    IF NOT EXISTS is specified, an warning is put into the stack.
*/

bool
Events::create_event(THD *thd, Event_parse_data *parse_data, bool if_not_exists)
{
  int ret;
  DBUG_ENTER("Events::create_event");
  if (unlikely(check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  pthread_mutex_lock(&LOCK_event_metadata);
  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->create_event(thd, parse_data, if_not_exists)) &&
      !parse_data->do_not_create)
  {
    Event_queue_element *new_element;

    if (!(new_element= new Event_queue_element()))
      ret= TRUE;                                // OOM
    else if ((ret= db_repository->load_named_event(thd, parse_data->dbname,
                                                   parse_data->name,
                                                   new_element)))
    {
      DBUG_ASSERT(ret == OP_LOAD_ERROR);
      delete new_element;
    }
    else
      event_queue->create_event(thd, new_element);
  }
  pthread_mutex_unlock(&LOCK_event_metadata);

  DBUG_RETURN(ret);
  
}


/*
  The function exported to the world for alteration of events.

  SYNOPSIS
    Events::update_event()
      thd           [in]  THD
      parse_data    [in]  Event's data from parsing stage
      rename_to     [in]  Set in case of RENAME TO.

  RETURN VALUE
    FALSE  OK
    TRUE   Error

  NOTES
    et contains data about dbname and event name. 
    new_name is the new name of the event, if not null this means
    that RENAME TO was specified in the query
*/

bool
Events::update_event(THD *thd, Event_parse_data *parse_data, sp_name *rename_to)
{
  int ret;
  Event_queue_element *new_element;
  DBUG_ENTER("Events::update_event");
  LEX_STRING *new_dbname= rename_to ? &rename_to->m_db : NULL;
  LEX_STRING *new_name= rename_to ? &rename_to->m_name : NULL;
  if (unlikely(check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  pthread_mutex_lock(&LOCK_event_metadata);
  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->update_event(thd, parse_data, new_dbname, new_name)))
  {
    LEX_STRING dbname= new_dbname ? *new_dbname : parse_data->dbname;
    LEX_STRING name= new_name ? *new_name : parse_data->name;

    if (!(new_element= new Event_queue_element()))
      ret= TRUE;                                // OOM
    else if ((ret= db_repository->load_named_event(thd, dbname, name,
                                                   new_element)))
    {
      DBUG_ASSERT(ret == OP_LOAD_ERROR);
      delete new_element;   
    }
    else
      event_queue->update_event(thd, parse_data->dbname, parse_data->name,
                                new_element);
  }
  pthread_mutex_unlock(&LOCK_event_metadata);

  DBUG_RETURN(ret);
}


/*
  Drops an event

  SYNOPSIS
    Events::drop_event()
      thd             [in]  THD
      dbname          [in]  Event's schema
      name            [in]  Event's name
      if_exists       [in]  When set and the event does not exist =>
                            warning onto the stack

  RETURN VALUE
    FALSE  OK
    TRUE   Error (reported)
*/

bool
Events::drop_event(THD *thd, LEX_STRING dbname, LEX_STRING name, bool if_exists)
{
  int ret;
  DBUG_ENTER("Events::drop_event");
  if (unlikely(check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  pthread_mutex_lock(&LOCK_event_metadata);
  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->drop_event(thd, dbname, name, if_exists)))
    event_queue->drop_event(thd, dbname, name);
  pthread_mutex_unlock(&LOCK_event_metadata);
  DBUG_RETURN(ret);
}


/*
  Drops all events from a schema

  SYNOPSIS
    Events::drop_schema_events()
      thd  Thread
      db   ASCIIZ schema name
*/

void
Events::drop_schema_events(THD *thd, char *db)
{
  LEX_STRING const db_lex= { db, strlen(db) };
  
  DBUG_ENTER("Events::drop_schema_events");  
  DBUG_PRINT("enter", ("dropping events from %s", db));
  if (unlikely(check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_VOID_RETURN;
  }

  pthread_mutex_lock(&LOCK_event_metadata);
  event_queue->drop_schema_events(thd, db_lex);
  db_repository->drop_schema_events(thd, db_lex);
  pthread_mutex_unlock(&LOCK_event_metadata);

  DBUG_VOID_RETURN;
}


/*
  SHOW CREATE EVENT

  SYNOPSIS
    Events::show_create_event()
      thd   Thread context
      spn   The name of the event (db, name)

  RETURN VALUE
    FALSE  OK
    TRUE   Error during writing to the wire
*/

bool
Events::show_create_event(THD *thd, LEX_STRING dbname, LEX_STRING name)
{
  CHARSET_INFO *scs= system_charset_info;
  int ret;
  Event_timed *et= new Event_timed();

  DBUG_ENTER("Events::show_create_event");
  DBUG_PRINT("enter", ("name: %s@%s", dbname.str, name.str));
  if (unlikely(check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  ret= db_repository->load_named_event(thd, dbname, name, et);

  if (!ret)
  {
    Protocol *protocol= thd->protocol;
    char show_str_buf[10 * STRING_BUFFER_USUAL_SIZE];
    String show_str(show_str_buf, sizeof(show_str_buf), scs);
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

    const String *tz_name= et->time_zone->get_name();
    field_list.push_back(new Item_empty_string("time_zone",
                                               tz_name->length()));

    field_list.push_back(new Item_empty_string("Create Event",
                                               show_str.length()));

    if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                           Protocol::SEND_EOF))
      goto err;

    protocol->prepare_for_resend();
    protocol->store(et->name.str, et->name.length, scs);

    protocol->store((char*) sql_mode_str, sql_mode_len, scs);

    protocol->store((char*) tz_name->ptr(), tz_name->length(), scs);

    protocol->store(show_str.c_ptr(), show_str.length(), scs);
    ret= protocol->write();
    send_eof(thd);
  }
  delete et;
  DBUG_RETURN(ret);
err:
  delete et;
  DBUG_RETURN(TRUE);
}


/*
  Proxy for Event_db_repository::fill_schema_events.
  Callback for I_S from sql_show.cc

  SYNOPSIS
    Events::fill_schema_events()
      thd     Thread context
      tables  The schema table
      cond    Unused

  RETURN VALUE
    0  OK
    !0 Error
*/

int
Events::fill_schema_events(THD *thd, TABLE_LIST *tables, COND * /* cond */)
{
  char *db= NULL;
  DBUG_ENTER("Events::fill_schema_events");
  Events *myself= get_instance();
  if (unlikely(myself->check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    If it's SHOW EVENTS then thd->lex->select_lex.db is guaranteed not to
    be NULL. Let's do an assert anyway.
  */
  if (thd->lex->sql_command == SQLCOM_SHOW_EVENTS)
  {
    DBUG_ASSERT(thd->lex->select_lex.db);
    if (check_access(thd, EVENT_ACL, thd->lex->select_lex.db, 0, 0, 0,
                     is_schema_db(thd->lex->select_lex.db)))
      DBUG_RETURN(1);
    db= thd->lex->select_lex.db;
  }
  DBUG_RETURN(myself->db_repository->fill_schema_events(thd, tables, db));
}


/*
  Inits the scheduler's structures.

  SYNOPSIS
    Events::init()

  NOTES
    This function is not synchronized.

  RETURN VALUE
    FALSE  OK
    TRUE   Error in case the scheduler can't start
*/

bool
Events::init()
{
  THD *thd;
  bool res= FALSE;
  DBUG_ENTER("Events::init");

  if (opt_event_scheduler == Events::EVENTS_DISABLED)
    DBUG_RETURN(FALSE);

  /* We need a temporary THD during boot */
  if (!(thd= new THD()))
  {
    res= TRUE;
    goto end;
  }
  /*
    The thread stack does not start from this function but we cannot
    guess the real value. So better some value that doesn't assert than
    no value.
  */
  thd->thread_stack= (char*) &thd;
  thd->store_globals();

  if (check_system_tables(thd))
  {
    check_system_tables_error= TRUE;
    sql_print_error("SCHEDULER: The system tables are damaged. "
                    "The scheduler subsystem will be unusable during this run.");
    goto end;
  }
  check_system_tables_error= FALSE;

  if (event_queue->init_queue(thd) || load_events_from_db(thd))
  {
    sql_print_error("SCHEDULER: Error while loading from disk.");
    goto end;
  }

  scheduler->init_scheduler(event_queue);

  DBUG_ASSERT(opt_event_scheduler == Events::EVENTS_ON ||
              opt_event_scheduler == Events::EVENTS_OFF);
  if (opt_event_scheduler == Events::EVENTS_ON)
    res= scheduler->start();

  Event_worker_thread::init(this, db_repository);
end:
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  NULL);

  DBUG_RETURN(res);
}


/*
  Cleans up scheduler's resources. Called at server shutdown.

  SYNOPSIS
    Events::deinit()

  NOTES
    This function is not synchronized.
*/

void
Events::deinit()
{
  DBUG_ENTER("Events::deinit");
  if (likely(!check_system_tables_error))
  {
    scheduler->stop();
    scheduler->deinit_scheduler();

    event_queue->deinit_queue();
  }

  DBUG_VOID_RETURN;
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
  pthread_mutex_init(&LOCK_event_metadata, MY_MUTEX_INIT_FAST);
  event_queue->init_mutexes();
  scheduler->init_mutexes();
}


/*
  Destroys Events mutexes

  SYNOPSIS
    Events::destroy_mutexes()
*/

void
Events::destroy_mutexes()
{
  event_queue->deinit_mutexes();
  scheduler->deinit_mutexes();
  pthread_mutex_destroy(&LOCK_event_metadata);
}


/*
  Dumps the internal status of the scheduler and the memory cache
  into a table with two columns - Name & Value. Different properties
  which could be useful for debugging for instance deadlocks are
  returned.

  SYNOPSIS
    Events::dump_internal_status()
*/

void
Events::dump_internal_status()
{
  DBUG_ENTER("Events::dump_internal_status");
  puts("\n\n\nEvents status:");
  puts("LLA = Last Locked At  LUA = Last Unlocked At");
  puts("WOC = Waiting On Condition  DL = Data Locked");

  scheduler->dump_internal_status();
  event_queue->dump_internal_status();

  DBUG_VOID_RETURN;
}


/*
  Starts execution of events by the scheduler

  SYNOPSIS
    Events::start_execution_of_events()

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Events::start_execution_of_events()
{
  DBUG_ENTER("Events::start_execution_of_events");
  if (unlikely(check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(scheduler->start());
}


/*
  Stops execution of events by the scheduler.
  Already running events will not be stopped. If the user needs
  them stopped manual intervention is needed.

  SYNOPSIS
    Events::stop_execution_of_events()

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Events::stop_execution_of_events()
{
  DBUG_ENTER("Events::stop_execution_of_events");
  if (unlikely(check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(scheduler->stop());
}


/*
  Checks whether the scheduler is running or not.

  SYNOPSIS
    Events::is_started()

  RETURN VALUE
    TRUE   Yes
    FALSE  No
*/

bool
Events::is_execution_of_events_started()
{
  DBUG_ENTER("Events::is_execution_of_events_started");
  if (unlikely(check_system_tables_error))
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(FALSE);
  }
  DBUG_RETURN(scheduler->is_running());
}



/*
  Opens mysql.db and mysql.user and checks whether:
    1. mysql.db has column Event_priv at column 20 (0 based);
    2. mysql.user has column Event_priv at column 29 (0 based);

  SYNOPSIS
    Events::check_system_tables()
      thd  Thread

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Events::check_system_tables(THD *thd)
{
  TABLE_LIST tables;
  Open_tables_state backup;
  bool ret= FALSE;

  DBUG_ENTER("Events::check_system_tables");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));

  thd->reset_n_backup_open_tables_state(&backup);

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "db";
  tables.lock_type= TL_READ;

  if ((ret= simple_open_n_lock_tables(thd, &tables)))
  {
    sql_print_error("SCHEDULER: Cannot open mysql.db");
    ret= TRUE;
  }
  ret= table_check_intact(tables.table, MYSQL_DB_FIELD_COUNT,
                          mysql_db_table_fields, &mysql_db_table_last_check,
                          ER_CANNOT_LOAD_FROM_TABLE);
  close_thread_tables(thd);

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "user";
  tables.lock_type= TL_READ;

  if (simple_open_n_lock_tables(thd, &tables))
  {
    sql_print_error("SCHEDULER: Cannot open mysql.user");
    ret= TRUE;
  }
  else
  {
    if (tables.table->s->fields < 29 ||
        strncmp(tables.table->field[29]->field_name,
                STRING_WITH_LEN("Event_priv")))
    {
      sql_print_error("mysql.user has no `Event_priv` column at position %d",
                      29);
      ret= TRUE;
    }
    close_thread_tables(thd);
  }

  thd->restore_backup_open_tables_state(&backup);

  DBUG_RETURN(ret);
}


/*
  Loads all ENABLED events from mysql.event into the prioritized
  queue. Called during scheduler main thread initialization. Compiles
  the events. Creates Event_queue_element instances for every ENABLED event
  from mysql.event.

  SYNOPSIS
    Events::load_events_from_db()
      thd  Thread context. Used for memory allocation in some cases.

  RETURN VALUE
    0  OK
   !0  Error (EVEX_OPEN_TABLE_FAILED, EVEX_MICROSECOND_UNSUP, 
              EVEX_COMPILE_ERROR) - in all these cases mysql.event was
              tampered.

  NOTES
    Reports the error to the console
*/

int
Events::load_events_from_db(THD *thd)
{
  TABLE *table;
  READ_RECORD read_record_info;
  int ret= -1;
  uint count= 0;
  bool clean_the_queue= TRUE;

  DBUG_ENTER("Events::load_events_from_db");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));

  if ((ret= db_repository->open_event_table(thd, TL_READ, &table)))
  {
    sql_print_error("SCHEDULER: Table mysql.event is damaged. Can not open");
    DBUG_RETURN(EVEX_OPEN_TABLE_FAILED);
  }

  init_read_record(&read_record_info, thd, table ,NULL,1,0);
  while (!(read_record_info.read_record(&read_record_info)))
  {
    Event_queue_element *et;
    if (!(et= new Event_queue_element))
    {
      DBUG_PRINT("info", ("Out of memory"));
      break;
    }
    DBUG_PRINT("info", ("Loading event from row."));

    if ((ret= et->load_from_row(thd, table)))
    {
      sql_print_error("SCHEDULER: Error while loading from mysql.event. "
                      "Table probably corrupted");
      break;
    }
    if (et->status != Event_queue_element::ENABLED)
    {
      DBUG_PRINT("info",("%s is disabled",et->name.str));
      delete et;
      continue;
    }

    /* let's find when to be executed */
    if (et->compute_next_execution_time())
    {
      sql_print_error("SCHEDULER: Error while computing execution time of %s.%s."
                      " Skipping", et->dbname.str, et->name.str);
      continue;
    }

    {
      Event_job_data temp_job_data;
      DBUG_PRINT("info", ("Event %s loaded from row. ", et->name.str));

      temp_job_data.load_from_row(thd, table);

      /*
        We load only on scheduler root just to check whether the body
        compiles.
      */
      switch (ret= temp_job_data.compile(thd, thd->mem_root)) {
      case EVEX_MICROSECOND_UNSUP:
        sql_print_error("SCHEDULER: mysql.event is tampered. MICROSECOND is not "
                        "supported but found in mysql.event");
        break;
      case EVEX_COMPILE_ERROR:
        sql_print_error("SCHEDULER: Error while compiling %s.%s. Aborting load",
                        et->dbname.str, et->name.str);
        break;
      default:
        break;
      }
      thd->end_statement();
      thd->cleanup_after_query();
    }
    if (ret)
    {
      delete et;
      goto end;
    }

    DBUG_PRINT("load_events_from_db", ("Adding 0x%lx to the exec list.",
                                       (long) et));
    event_queue->create_event(thd, et);
    count++;
  }
  clean_the_queue= FALSE;
end:
  end_read_record(&read_record_info);

  if (clean_the_queue)
  {
    event_queue->empty_queue();
    ret= -1;
  }
  else
  {
    ret= 0;
    sql_print_information("SCHEDULER: Loaded %d event%s", count,
                          (count == 1)?"":"s");
  }

  close_thread_tables(thd);

  DBUG_PRINT("info", ("Status code %d. Loaded %d event(s)", ret, count));
  DBUG_RETURN(ret);
}
