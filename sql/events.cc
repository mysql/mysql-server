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
#include "sp_head.h" // for Stored_program_creation_ctx

/**
  @addtogroup Event_Scheduler
  @{
*/

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

const TYPELIB Events::opt_typelib=
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

const TYPELIB Events::var_typelib=
{
  array_elements(var_event_scheduler_state_names)-1,
  "",
  var_event_scheduler_state_names,
  NULL
};

Event_queue *Events::event_queue;
Event_scheduler *Events::scheduler;
Event_db_repository *Events::db_repository;
enum Events::enum_opt_event_scheduler
Events::opt_event_scheduler= Events::EVENTS_OFF;
pthread_mutex_t Events::LOCK_event_metadata;
bool Events::check_system_tables_error= FALSE;


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


/**
  @brief Initialize the start up option of the Events scheduler.

  Do not initialize the scheduler subsystem yet - the initialization
  is split into steps as it has to fit into the common MySQL
  initialization framework.
  No locking as this is called only at start up.

  @param[in,out]  argument  The value of the argument. If this value
                            is found in the typelib, the argument is
                            updated.

  @retval TRUE  unknown option value
  @retval FALSE success
*/

bool
Events::set_opt_event_scheduler(char *argument)
{
  if (argument == NULL)
    opt_event_scheduler= Events::EVENTS_DISABLED;
  else
  {
    int type;
    /*
      type=   1   2      3   4      5
           (OFF | ON) - (0 | 1) (DISABLE )
    */
    const static enum enum_opt_event_scheduler type2state[]=
    { EVENTS_OFF, EVENTS_ON, EVENTS_OFF, EVENTS_ON, EVENTS_DISABLED };

    type= find_type(argument, &opt_typelib, 1);

    DBUG_ASSERT(type >= 0 && type <= 5); /* guaranteed by find_type */

    if (type == 0)
    {
      fprintf(stderr, "Unknown option to event-scheduler: %s\n", argument);
      return TRUE;
    }
    opt_event_scheduler= type2state[type-1];
  }
  return FALSE;
}


/**
  Return a string representation of the current scheduler mode.
*/

const char *
Events::get_opt_event_scheduler_str()
{
  const char *str;

  pthread_mutex_lock(&LOCK_event_metadata);
  str= opt_typelib.type_names[(int) opt_event_scheduler];
  pthread_mutex_unlock(&LOCK_event_metadata);

  return str;
}


/**
  Push an error into the error stack if the system tables are
  not up to date.
*/

bool Events::check_if_system_tables_error()
{
  DBUG_ENTER("Events::check_if_system_tables_error");

  if (check_system_tables_error)
  {
    my_error(ER_EVENTS_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/**
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


/**
  Create a new event.

  @param[in,out]  thd            THD
  @param[in]      parse_data     Event's data from parsing stage
  @param[in]      if_not_exists  Whether IF NOT EXISTS was
                                 specified
  In case there is an event with the same name (db) and
  IF NOT EXISTS is specified, an warning is put into the stack.
  @sa Events::drop_event for the notes about locking, pre-locking
  and Events DDL.

  @retval  FALSE  OK
  @retval  TRUE   Error (reported)
*/

bool
Events::create_event(THD *thd, Event_parse_data *parse_data,
                     bool if_not_exists)
{
  int ret;
  DBUG_ENTER("Events::create_event");

  /*
    Let's commit the transaction first - MySQL manual specifies
    that a DDL issues an implicit commit, and it doesn't say "successful
    DDL", so that an implicit commit is a property of any successfully
    parsed DDL statement.
  */
  if (end_active_trans(thd))
    DBUG_RETURN(TRUE);

  if (check_if_system_tables_error())
    DBUG_RETURN(TRUE);

  /*
    Perform semantic checks outside of Event_db_repository:
    once CREATE EVENT is supported in prepared statements, the
    checks will be moved to PREPARE phase.
  */
  if (parse_data->check_parse_data(thd))
    DBUG_RETURN(TRUE);

  /* At create, one of them must be set */
  DBUG_ASSERT(parse_data->expression || parse_data->execute_at);

  if (check_access(thd, EVENT_ACL, parse_data->dbname.str, 0, 0, 0,
                   is_schema_db(parse_data->dbname.str)))
    DBUG_RETURN(TRUE);

  if (check_db_dir_existence(parse_data->dbname.str))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), parse_data->dbname.str);
    DBUG_RETURN(TRUE);
  }

  if (parse_data->do_not_create)
    DBUG_RETURN(FALSE);
  /* 
    Turn off row binlogging of this statement and use statement-based 
    so that all supporting tables are updated for CREATE EVENT command.
  */
  if (thd->current_stmt_binlog_row_based)
    thd->clear_current_stmt_binlog_row_based();

  pthread_mutex_lock(&LOCK_event_metadata);

  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->create_event(thd, parse_data, if_not_exists)))
  {
    Event_queue_element *new_element;

    if (!(new_element= new Event_queue_element()))
      ret= TRUE;                                // OOM
    else if ((ret= db_repository->load_named_event(thd, parse_data->dbname,
                                                   parse_data->name,
                                                   new_element)))
    {
      db_repository->drop_event(thd, parse_data->dbname, parse_data->name,
                                TRUE);
      delete new_element;
    }
    else
    {
      /* TODO: do not ignore the out parameter and a possible OOM error! */
      bool created;
      if (event_queue)
        event_queue->create_event(thd, new_element, &created);
      /* Binlog the create event. */
      DBUG_ASSERT(thd->query && thd->query_length);
      write_bin_log(thd, TRUE, thd->query, thd->query_length);
    }
  }
  pthread_mutex_unlock(&LOCK_event_metadata);

  DBUG_RETURN(ret);
}


/**
  Alter an event.

  @param[in,out] thd         THD
  @param[in]     parse_data  Event's data from parsing stage
  @param[in]     new_dbname  A new schema name for the event. Set in the case of
                             ALTER EVENT RENAME, otherwise is NULL.
  @param[in]     new_name    A new name for the event. Set in the case of
                             ALTER EVENT RENAME

  Parameter 'et' contains data about dbname and event name.
  Parameter 'new_name' is the new name of the event, if not null
  this means that RENAME TO was specified in the query
  @sa Events::drop_event for the locking notes.

  @retval  FALSE  OK
  @retval  TRUE   error (reported)
*/

bool
Events::update_event(THD *thd, Event_parse_data *parse_data,
                     LEX_STRING *new_dbname, LEX_STRING *new_name)
{
  int ret;
  Event_queue_element *new_element;

  DBUG_ENTER("Events::update_event");

  /*
    For consistency, implicit COMMIT should be the first thing in the
    execution chain.
  */
  if (end_active_trans(thd))
    DBUG_RETURN(TRUE);

  if (check_if_system_tables_error())
    DBUG_RETURN(TRUE);

  if (parse_data->check_parse_data(thd) || parse_data->do_not_create)
    DBUG_RETURN(TRUE);

  if (check_access(thd, EVENT_ACL, parse_data->dbname.str, 0, 0, 0,
                   is_schema_db(parse_data->dbname.str)))
    DBUG_RETURN(TRUE);

  if (new_dbname)                               /* It's a rename */
  {
    /* Check that the new and the old names differ. */
    if ( !sortcmp_lex_string(parse_data->dbname, *new_dbname,
                             system_charset_info) &&
         !sortcmp_lex_string(parse_data->name, *new_name,
                             system_charset_info))
    {
      my_error(ER_EVENT_SAME_NAME, MYF(0), parse_data->name.str);
      DBUG_RETURN(TRUE);
    }

    /*
      And the user has sufficient privileges to use the target database.
      Do it before checking whether the database exists: we don't want
      to tell the user that a database doesn't exist if they can not
      access it.
    */
    if (check_access(thd, EVENT_ACL, new_dbname->str, 0, 0, 0,
                     is_schema_db(new_dbname->str)))
      DBUG_RETURN(TRUE);

    /* Check that the target database exists */
    if (check_db_dir_existence(new_dbname->str))
    {
      my_error(ER_BAD_DB_ERROR, MYF(0), new_dbname->str);
      DBUG_RETURN(TRUE);
    }
  }

  /* 
    Turn off row binlogging of this statement and use statement-based 
    so that all supporting tables are updated for UPDATE EVENT command.
  */
  if (thd->current_stmt_binlog_row_based)
    thd->clear_current_stmt_binlog_row_based();

  pthread_mutex_lock(&LOCK_event_metadata);

  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->update_event(thd, parse_data,
                                         new_dbname, new_name)))
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
    {
      /*
        TODO: check if an update actually has inserted an entry
        into the queue.
        If not, and the element is ON COMPLETION NOT PRESERVE, delete
        it right away.
      */
      if (event_queue)
        event_queue->update_event(thd, parse_data->dbname, parse_data->name,
                                  new_element);
      /* Binlog the alter event. */
      DBUG_ASSERT(thd->query && thd->query_length);
      write_bin_log(thd, TRUE, thd->query, thd->query_length);
    }
  }
  pthread_mutex_unlock(&LOCK_event_metadata);

  DBUG_RETURN(ret);
}


/**
  Drops an event

  @param[in,out]  thd        THD
  @param[in]      dbname     Event's schema
  @param[in]      name       Event's name
  @param[in]      if_exists  When this is set and the event does not exist
                             a warning is pushed into the warning stack.
                             Otherwise the operation produces an error.

  @note Similarly to DROP PROCEDURE, we do not allow DROP EVENT
  under LOCK TABLES mode, unless table mysql.event is locked.  To
  ensure that, we do not reset & backup the open tables state in
  this function - if in LOCK TABLES or pre-locking mode, this will
  lead to an error 'Table mysql.event is not locked with LOCK
  TABLES' unless it _is_ locked. In pre-locked mode there is
  another barrier - DROP EVENT commits the current transaction,
  and COMMIT/ROLLBACK is not allowed in stored functions and
  triggers.

  @retval  FALSE  OK
  @retval  TRUE   Error (reported)
*/

bool
Events::drop_event(THD *thd, LEX_STRING dbname, LEX_STRING name, bool if_exists)
{
  int ret;
  DBUG_ENTER("Events::drop_event");

  /*
    In MySQL, DDL must always commit: since mysql.* tables are
    non-transactional, we must modify them outside a transaction
    to not break atomicity.
    But the second and more important reason to commit here
    regardless whether we're actually changing mysql.event table
    or not is replication: end_active_trans syncs the binary log,
    and unless we run DDL in it's own transaction it may simply
    never appear on the slave in case the outside transaction
    rolls back.
  */
  if (end_active_trans(thd))
    DBUG_RETURN(TRUE);

  if (check_if_system_tables_error())
    DBUG_RETURN(TRUE);

  if (check_access(thd, EVENT_ACL, dbname.str, 0, 0, 0,
                   is_schema_db(dbname.str)))
    DBUG_RETURN(TRUE);

  /*
    Turn off row binlogging of this statement and use statement-based so
    that all supporting tables are updated for DROP EVENT command.
  */
  if (thd->current_stmt_binlog_row_based)
    thd->clear_current_stmt_binlog_row_based();

  pthread_mutex_lock(&LOCK_event_metadata);
  /* On error conditions my_error() is called so no need to handle here */
  if (!(ret= db_repository->drop_event(thd, dbname, name, if_exists)))
  {
    if (event_queue)
      event_queue->drop_event(thd, dbname, name);
    /* Binlog the drop event. */
    DBUG_ASSERT(thd->query && thd->query_length);
    write_bin_log(thd, TRUE, thd->query, thd->query_length);
  }
  pthread_mutex_unlock(&LOCK_event_metadata);
  DBUG_RETURN(ret);
}


/**
  Drops all events from a schema

  @note We allow to drop all events in a schema even if the
  scheduler is disabled. This is to not produce any warnings
  in case of DROP DATABASE and a disabled scheduler.

  @param[in,out]  thd  Thread
  @param[in]      db   ASCIIZ schema name
*/

void
Events::drop_schema_events(THD *thd, char *db)
{
  LEX_STRING const db_lex= { db, strlen(db) };

  DBUG_ENTER("Events::drop_schema_events");
  DBUG_PRINT("enter", ("dropping events from %s", db));

  /*
    sic: no check if the scheduler is disabled or system tables
    are damaged, as intended.
  */

  pthread_mutex_lock(&LOCK_event_metadata);
  if (event_queue)
    event_queue->drop_schema_events(thd, db_lex);
  db_repository->drop_schema_events(thd, db_lex);
  pthread_mutex_unlock(&LOCK_event_metadata);

  DBUG_VOID_RETURN;
}


/**
  A helper function to generate SHOW CREATE EVENT output from
  a named event
*/

static bool
send_show_create_event(THD *thd, Event_timed *et, Protocol *protocol)
{
  char show_str_buf[10 * STRING_BUFFER_USUAL_SIZE];
  String show_str(show_str_buf, sizeof(show_str_buf), system_charset_info);
  List<Item> field_list;
  LEX_STRING sql_mode;
  const String *tz_name;

  DBUG_ENTER("send_show_create_event");

  show_str.length(0);
  if (et->get_create_event(thd, &show_str))
    DBUG_RETURN(TRUE);

  field_list.push_back(new Item_empty_string("Event", NAME_CHAR_LEN));

  if (sys_var_thd_sql_mode::symbolic_mode_representation(thd, et->sql_mode,
                                                         &sql_mode))
    DBUG_RETURN(TRUE);

  field_list.push_back(new Item_empty_string("sql_mode", sql_mode.length));

  tz_name= et->time_zone->get_name();

  field_list.push_back(new Item_empty_string("time_zone",
                                             tz_name->length()));

  field_list.push_back(new Item_empty_string("Create Event",
                                             show_str.length()));

  field_list.push_back(
    new Item_empty_string("character_set_client", MY_CS_NAME_SIZE));

  field_list.push_back(
    new Item_empty_string("collation_connection", MY_CS_NAME_SIZE));

  field_list.push_back(
    new Item_empty_string("Database Collation", MY_CS_NAME_SIZE));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  protocol->prepare_for_resend();

  protocol->store(et->name.str, et->name.length, system_charset_info);
  protocol->store(sql_mode.str, sql_mode.length, system_charset_info);
  protocol->store(tz_name->ptr(), tz_name->length(), system_charset_info);
  protocol->store(show_str.c_ptr(), show_str.length(),
                  et->creation_ctx->get_client_cs());
  protocol->store(et->creation_ctx->get_client_cs()->csname,
                  strlen(et->creation_ctx->get_client_cs()->csname),
                  system_charset_info);
  protocol->store(et->creation_ctx->get_connection_cl()->name,
                  strlen(et->creation_ctx->get_connection_cl()->name),
                  system_charset_info);
  protocol->store(et->creation_ctx->get_db_cl()->name,
                  strlen(et->creation_ctx->get_db_cl()->name),
                  system_charset_info);

  if (protocol->write())
    DBUG_RETURN(TRUE);

  send_eof(thd);

  DBUG_RETURN(FALSE);
}


/**
  Implement SHOW CREATE EVENT statement

      thd   Thread context
      spn   The name of the event (db, name)

  @retval  FALSE  OK
  @retval  TRUE   error (reported)
*/

bool
Events::show_create_event(THD *thd, LEX_STRING dbname, LEX_STRING name)
{
  Open_tables_state open_tables_backup;
  Event_timed et;
  bool ret;

  DBUG_ENTER("Events::show_create_event");
  DBUG_PRINT("enter", ("name: %s@%s", dbname.str, name.str));

  if (check_if_system_tables_error())
    DBUG_RETURN(TRUE);

  if (check_access(thd, EVENT_ACL, dbname.str, 0, 0, 0,
                   is_schema_db(dbname.str)))
    DBUG_RETURN(TRUE);

  /*
    We would like to allow SHOW CREATE EVENT under LOCK TABLES and
    in pre-locked mode. mysql.event table is marked as a system table.
    This flag reduces the set of its participation scenarios in LOCK TABLES
    operation, and therefore an out-of-bound open of this table
    for reading like the one below (sic, only for reading) is
    more or less deadlock-free. For additional information about when a
    deadlock can occur please refer to the description of 'system table'
    flag.
  */
  thd->reset_n_backup_open_tables_state(&open_tables_backup);
  ret= db_repository->load_named_event(thd, dbname, name, &et);
  thd->restore_backup_open_tables_state(&open_tables_backup);

  if (!ret)
    ret= send_show_create_event(thd, &et, thd->protocol);

  DBUG_RETURN(ret);
}


/**
  Check access rights and fill INFORMATION_SCHEMA.events table.

  @param[in,out]  thd     Thread context
  @param[in]      tables  The temporary table to fill.

  In MySQL INFORMATION_SCHEMA tables are temporary tables that are
  created and filled on demand. In this function, we fill
  INFORMATION_SCHEMA.events. It is a callback for I_S module, invoked from
  sql_show.cc

  @return Has to be integer, as such is the requirement of the I_S API
  @retval  0  success
  @retval  1  an error, pushed into the error stack
*/

int
Events::fill_schema_events(THD *thd, TABLE_LIST *tables, COND * /* cond */)
{
  char *db= NULL;
  int ret;
  Open_tables_state open_tables_backup;
  DBUG_ENTER("Events::fill_schema_events");

  if (check_if_system_tables_error())
    DBUG_RETURN(1);

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
  /*
    Reset and backup of the currently open tables in this thread
    is a way to allow SELECTs from INFORMATION_SCHEMA.events under
    LOCK TABLES and in pre-locked mode. See also
    Events::show_create_event for additional comments.
  */
  thd->reset_n_backup_open_tables_state(&open_tables_backup);
  ret= db_repository->fill_schema_events(thd, tables, db);
  thd->restore_backup_open_tables_state(&open_tables_backup);

  DBUG_RETURN(ret);
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
Events::init(my_bool opt_noacl)
{
  THD *thd;
  bool res= FALSE;

  DBUG_ENTER("Events::init");

  /* Disable the scheduler if running with --skip-grant-tables */
  if (opt_noacl)
    opt_event_scheduler= EVENTS_DISABLED;


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
  lex_start(thd);

  /*
    We will need Event_db_repository anyway, even if the scheduler is
    disabled - to perform events DDL.
  */
  if (!(db_repository= new Event_db_repository))
  {
    res= TRUE; /* fatal error: request unireg_abort */
    goto end;
  }

  /*
    Since we allow event DDL even if the scheduler is disabled,
    check the system tables, as we might need them.
  */
  if (Event_db_repository::check_system_tables(thd))
  {
    sql_print_error("Event Scheduler: An error occurred when initializing "
                    "system tables.%s",
                    opt_event_scheduler == EVENTS_DISABLED ?
                    "" : " Disabling the Event Scheduler.");

    /* Disable the scheduler since the system tables are not up to date */
    opt_event_scheduler= EVENTS_DISABLED;
    check_system_tables_error= TRUE;
    goto end;
  }

  /*
    Was disabled explicitly from the command line, or because we're running
    with --skip-grant-tables, or because we have no system tables.
  */
  if (opt_event_scheduler == Events::EVENTS_DISABLED)
    goto end;


  DBUG_ASSERT(opt_event_scheduler == Events::EVENTS_ON ||
              opt_event_scheduler == Events::EVENTS_OFF);

  if (!(event_queue= new Event_queue) ||
      !(scheduler= new Event_scheduler(event_queue)))
  {
    res= TRUE; /* fatal error: request unireg_abort */
    goto end;
  }

  if (event_queue->init_queue(thd) || load_events_from_db(thd) ||
      opt_event_scheduler == EVENTS_ON && scheduler->start())
  {
    sql_print_error("Event Scheduler: Error while loading from disk.");
    res= TRUE; /* fatal error: request unireg_abort */
    goto end;
  }
  Event_worker_thread::init(db_repository);

end:
  if (res)
  {
    delete db_repository;
    delete event_queue;
    delete scheduler;
  }
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

  if (opt_event_scheduler != EVENTS_DISABLED)
  {
    delete scheduler;
    scheduler= NULL;                            /* safety */
    delete event_queue;
    event_queue= NULL;                          /* safety */
  }

  delete db_repository;
  db_repository= NULL;                          /* safety */

  DBUG_VOID_RETURN;
}


/**
  Inits Events mutexes

  SYNOPSIS
    Events::init_mutexes()
      thd  Thread
*/

void
Events::init_mutexes()
{
  pthread_mutex_init(&LOCK_event_metadata, MY_MUTEX_INIT_FAST);
}


/*
  Destroys Events mutexes

  SYNOPSIS
    Events::destroy_mutexes()
*/

void
Events::destroy_mutexes()
{
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

  pthread_mutex_lock(&LOCK_event_metadata);
  if (opt_event_scheduler == EVENTS_DISABLED)
    puts("The Event Scheduler is disabled");
  else
  {
    scheduler->dump_internal_status();
    event_queue->dump_internal_status();
  }

  pthread_mutex_unlock(&LOCK_event_metadata);
  DBUG_VOID_RETURN;
}


/**
  Starts or stops the event scheduler thread.

  @retval FALSE success
  @retval TRUE  error
*/

bool
Events::switch_event_scheduler_state(enum_opt_event_scheduler new_state)
{
  bool ret= FALSE;

  DBUG_ENTER("Events::switch_event_scheduler_state");

  DBUG_ASSERT(new_state == Events::EVENTS_ON ||
              new_state == Events::EVENTS_OFF);

  /*
    If the scheduler was disabled because there are no/bad
    system tables, produce a more meaningful error message
    than ER_OPTION_PREVENTS_STATEMENT
  */
  if (check_if_system_tables_error())
    DBUG_RETURN(TRUE);

  pthread_mutex_lock(&LOCK_event_metadata);

  if (opt_event_scheduler == EVENTS_DISABLED)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT,
             MYF(0), "--event-scheduler=DISABLED or --skip-grant-tables");
    ret= TRUE;
    goto end;
  }

  if (new_state == EVENTS_ON)
    ret= scheduler->start();
  else
    ret= scheduler->stop();

  if (ret)
  {
    my_error(ER_EVENT_SET_VAR_ERROR, MYF(0));
    goto end;
  }

  opt_event_scheduler= new_state;

end:
  pthread_mutex_unlock(&LOCK_event_metadata);
  DBUG_RETURN(ret);
}


/**
  Loads all ENABLED events from mysql.event into a prioritized
  queue.

  This function is called during the server start up. It reads
  every event, computes the next execution time, and if the event
  needs execution, adds it to a prioritized queue. Otherwise, if
  ON COMPLETION DROP is specified, the event is automatically
  removed from the table.

  @param[in,out] thd Thread context. Used for memory allocation in some cases.

  @retval  FALSE  success
  @retval  TRUE   error, the load is aborted

  @note Reports the error to the console
*/

bool
Events::load_events_from_db(THD *thd)
{
  TABLE *table;
  READ_RECORD read_record_info;
  bool ret= TRUE;
  uint count= 0;

  DBUG_ENTER("Events::load_events_from_db");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));

  if (db_repository->open_event_table(thd, TL_WRITE, &table))
  {
    sql_print_error("Event Scheduler: Failed to open table mysql.event");
    DBUG_RETURN(TRUE);
  }

  init_read_record(&read_record_info, thd, table, NULL, 0, 1);
  while (!(read_record_info.read_record(&read_record_info)))
  {
    Event_queue_element *et;
    bool created;
    bool drop_on_completion;

    if (!(et= new Event_queue_element))
      goto end;

    DBUG_PRINT("info", ("Loading event from row."));

    if (et->load_from_row(thd, table))
    {
      sql_print_error("Event Scheduler: "
                      "Error while loading events from mysql.event. "
                      "The table probably contains bad data or is corrupted");
      delete et;
      goto end;
    }
    drop_on_completion= (et->on_completion ==
                         Event_queue_element::ON_COMPLETION_DROP);


    if (event_queue->create_event(thd, et, &created))
    {
      /* Out of memory */
      delete et;
      goto end;
    }
    if (created)
      count++;
    else if (drop_on_completion)
    {
      /*
        If not created, a stale event - drop if immediately if
        ON COMPLETION NOT PRESERVE
      */
      int rc= table->file->ha_delete_row(table->record[0]);
      if (rc)
      {
        table->file->print_error(rc, MYF(0));
        goto end;
      }
    }
  }
  sql_print_information("Event Scheduler: Loaded %d event%s",
                        count, (count == 1) ? "" : "s");
  ret= FALSE;

end:
  end_read_record(&read_record_info);

  close_thread_tables(thd);

  DBUG_RETURN(ret);
}

/**
  @} (End of group Event_Scheduler)
*/
