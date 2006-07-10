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
#include "events.h"
#include "event_data_objects.h"
#include "event_db_repository.h"
#include "event_queue.h"
#include "event_scheduler_ng.h"
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

const char *event_scheduler_state_names[]=
    { "OFF", "0", "ON", "1", "SUSPEND", "2", NullS };

TYPELIB Events::opt_typelib=
{
  array_elements(event_scheduler_state_names)-1,
  "",
  event_scheduler_state_names,
  NULL
};

Events Events::singleton;

ulong Events::opt_event_scheduler= 2;


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
      buf - preallocated String buffer to add the value to
      interval - the interval type (for instance YEAR_MONTH)
      expression - the value in the lowest entity

  RETURN VALUE
   0 - OK
   1 - Error
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
  return db_repository->open_event_table(thd, lock_type, table);
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
    !0  Error (Reported)

  NOTES
    In case there is an event with the same name (db) and 
    IF NOT EXISTS is specified, an warning is put into the stack.
*/

int
Events::create_event(THD *thd, Event_parse_data *parse_data, bool if_not_exists,
                     uint *rows_affected)
{
  int ret;
  DBUG_ENTER("Events::create_event");

  pthread_mutex_lock(&LOCK_event_metadata);
  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->create_event(thd, parse_data, if_not_exists,
                                         rows_affected)))
  {
    if ((ret= event_queue->create_event(thd, parse_data->dbname,
                                        parse_data->name)))
    {
      DBUG_ASSERT(ret == OP_LOAD_ERROR);
      my_error(ER_EVENT_MODIFY_QUEUE_ERROR, MYF(0));
    }
  }
  pthread_mutex_unlock(&LOCK_event_metadata);

  DBUG_RETURN(ret);
}


/*
  The function exported to the world for alteration of events.

  SYNOPSIS
    Events::update_event()
      thd        THD
      et         Event's data from parsing stage
      new_name   Set in case of RENAME TO.

  RETURN VALUE
    0   OK
    !0  Error

  NOTES
    et contains data about dbname and event name. 
    new_name is the new name of the event, if not null (this means
    that RENAME TO was specified in the query)
*/

int
Events::update_event(THD *thd, Event_parse_data *parse_data, sp_name *new_name,
                     uint *rows_affected)
{
  int ret;
  DBUG_ENTER("Events::update_event");

  pthread_mutex_lock(&LOCK_event_metadata);
  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->update_event(thd, parse_data, new_name)))
  {
    if ((ret= event_queue->update_event(thd,
                                        parse_data->dbname,
                                        parse_data->name,
                                        new_name? &new_name->m_db: NULL,
                                        new_name? &new_name->m_name: NULL)))
    {
      DBUG_ASSERT(ret == OP_LOAD_ERROR);
      my_error(ER_EVENT_MODIFY_QUEUE_ERROR, MYF(0));
    }
  }
  pthread_mutex_unlock(&LOCK_event_metadata);

  DBUG_RETURN(ret);
}


/*
  Drops an event

  SYNOPSIS
    Events::drop_event()
      thd             THD
      dbname          Event's schema
      name            Event's name
      if_exists       When set and the event does not exist => warning onto
                      the stack
      rows_affected   Affected number of rows is returned heres
      only_from_disk  Whether to remove the event from the queue too. In case
                      of Event_job_data::drop() it's needed to do only disk
                      drop because Event_queue will handle removal from memory
                      queue.

  RETURN VALUE
     0  OK
    !0  Error (reported)
*/

int
Events::drop_event(THD *thd, LEX_STRING dbname, LEX_STRING name, bool if_exists,
                   uint *rows_affected, bool only_from_disk)
{
  int ret;
  DBUG_ENTER("Events::drop_event");

  pthread_mutex_lock(&LOCK_event_metadata);
  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->drop_event(thd, dbname, name, if_exists,
                                       rows_affected)))
  {
    if (!only_from_disk)
      event_queue->drop_event(thd, dbname, name);
  }
  pthread_mutex_unlock(&LOCK_event_metadata);
  DBUG_RETURN(ret);
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

  pthread_mutex_lock(&LOCK_event_metadata);
  event_queue->drop_schema_events(thd, db_lex);
  ret= db_repository->drop_schema_events(thd, db_lex);
  pthread_mutex_unlock(&LOCK_event_metadata);

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
  Event_timed *et= new Event_timed();
  Open_tables_state backup;

  DBUG_ENTER("Events::show_create_event");
  DBUG_PRINT("enter", ("name: %*s", spn->m_name.length, spn->m_name.str));

  thd->reset_n_backup_open_tables_state(&backup);
  ret= db_repository->find_event(thd, spn->m_db, spn->m_name, et);
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
  Proxy for Event_db_repository::fill_schema_events.
  Callback for I_S from sql_show.cc

  SYNOPSIS
    Events::fill_schema_events()
      thd     Thread
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
  DBUG_RETURN(get_instance()->db_repository->fill_schema_events(thd, tables, db));
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
  Event_db_repository *db_repo;
  DBUG_ENTER("Events::init");
  event_queue->init_queue(db_repository, scheduler_ng);
  scheduler_ng->init_scheduler(event_queue);

  /* it should be an assignment! */
  if (opt_event_scheduler)
  {
    DBUG_ASSERT(opt_event_scheduler == 1 || opt_event_scheduler == 2);
    if (opt_event_scheduler == 1)
      DBUG_RETURN(scheduler_ng->start());
  }

  DBUG_RETURN(0);
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

  scheduler_ng->stop();
  scheduler_ng->deinit_scheduler();

  event_queue->deinit_queue();

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

  db_repository= new Event_db_repository;

  event_queue= new Event_queue;
  event_queue->init_mutexes();

  scheduler_ng= new Event_scheduler_ng();
  scheduler_ng->init_mutexes();
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
  scheduler_ng->deinit_mutexes();

  delete scheduler_ng;
  delete db_repository;

  pthread_mutex_destroy(&LOCK_event_metadata);
}


/*
  Proxy for Event_scheduler::dump_internal_status

  SYNOPSIS
    Events::dump_internal_status()
      thd  Thread

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Events::dump_internal_status(THD *thd)
{
  DBUG_ENTER("Events::dump_internal_status");
  Protocol *protocol= thd->protocol;
  List<Item> field_list;

  field_list.push_back(new Item_empty_string("Name", 30));
  field_list.push_back(new Item_empty_string("Value",20));
  if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                         Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  if (scheduler_ng->dump_internal_status(thd) ||
      event_queue->dump_internal_status(thd))
    DBUG_RETURN(TRUE);

  send_eof(thd);
  DBUG_RETURN(FALSE);
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
  DBUG_RETURN(scheduler_ng->start());
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
  DBUG_RETURN(scheduler_ng->stop());
}


/*
  Checks whether the scheduler is running or not.

  SYNOPSIS
    Events::is_started()

  RETURN VALUE
    TRUE  Yes
    FALSE No
*/

bool
Events::is_started()
{
  DBUG_ENTER("Events::is_started");
  DBUG_RETURN(scheduler_ng->get_state() == Event_scheduler_ng::RUNNING);
}
