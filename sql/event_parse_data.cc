/*
   Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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
#include "sp_head.h"
#include "event_parse_data.h"
#include "sql_time.h"                           // TIME_to_timestamp

/*
  Returns a new instance

  SYNOPSIS
    Event_parse_data::new_instance()

  RETURN VALUE
    Address or NULL in case of error

  NOTE
    Created on THD's mem_root
*/

Event_parse_data *
Event_parse_data::new_instance(THD *thd)
{
  return new (thd->mem_root) Event_parse_data;
}


/*
  Constructor

  SYNOPSIS
    Event_parse_data::Event_parse_data()
*/

Event_parse_data::Event_parse_data()
  :on_completion(Event_parse_data::ON_COMPLETION_DEFAULT),
  status(Event_parse_data::ENABLED), status_changed(false),
  do_not_create(FALSE), body_changed(FALSE),
  item_starts(NULL), item_ends(NULL), item_execute_at(NULL),
  starts_null(TRUE), ends_null(TRUE), execute_at_null(TRUE),
  item_expression(NULL), expression(0)
{
  DBUG_ENTER("Event_parse_data::Event_parse_data");

  /* Actually in the parser STARTS is always set */
  starts= ends= execute_at= 0;

  comment.str= NULL;
  comment.length= 0;

  DBUG_VOID_RETURN;
}


/*
  Set a name of the event

  SYNOPSIS
    Event_parse_data::init_name()
      thd   THD
      spn   the name extracted in the parser
*/

void
Event_parse_data::init_name(THD *thd, sp_name *spn)
{
  DBUG_ENTER("Event_parse_data::init_name");

  /* We have to copy strings to get them into the right memroot */
  dbname.length= spn->m_db.length;
  dbname.str= thd->strmake(spn->m_db.str, spn->m_db.length);
  name.length= spn->m_name.length;
  name.str= thd->strmake(spn->m_name.str, spn->m_name.length);

  if (spn->m_qname.length == 0)
    spn->init_qname(thd);

  DBUG_VOID_RETURN;
}


/*
  This function is called on CREATE EVENT or ALTER EVENT.  When either
  ENDS or AT is in the past, we are trying to create an event that
  will never be executed.  If it has ON COMPLETION NOT PRESERVE
  (default), then it would normally be dropped already, so on CREATE
  EVENT we give a warning, and do not create anyting.  On ALTER EVENT
  we give a error, and do not change the event.

  If the event has ON COMPLETION PRESERVE, then we see if the event is
  created or altered to the ENABLED (default) state.  If so, then we
  give a warning, and change the state to DISABLED.

  Otherwise it is a valid event in ON COMPLETION PRESERVE DISABLE
  state.
*/

void
Event_parse_data::check_if_in_the_past(THD *thd, my_time_t ltime_utc)
{
  if (ltime_utc >= (my_time_t) thd->query_start())
    return;

  /*
    We'll come back later when we have the real on_completion value
  */
  if (on_completion == Event_parse_data::ON_COMPLETION_DEFAULT)
    return;

  if (on_completion == Event_parse_data::ON_COMPLETION_DROP)
  {
    switch (thd->lex->sql_command) {
    case SQLCOM_CREATE_EVENT:
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                   ER_EVENT_CANNOT_CREATE_IN_THE_PAST,
                   ER(ER_EVENT_CANNOT_CREATE_IN_THE_PAST));
      break;
    case SQLCOM_ALTER_EVENT:
      my_error(ER_EVENT_CANNOT_ALTER_IN_THE_PAST, MYF(0));
      break;
    default:
      DBUG_ASSERT(0);
    }

    do_not_create= TRUE;
  }
  else if (status == Event_parse_data::ENABLED)
  {
    status= Event_parse_data::DISABLED;
    status_changed= true;
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                 ER_EVENT_EXEC_TIME_IN_THE_PAST,
                 ER(ER_EVENT_EXEC_TIME_IN_THE_PAST));
  }
}


/*
  Check time/dates in ALTER EVENT

  We check whether ALTER EVENT was given dates that are in the past.
  However to know how to react, we need the ON COMPLETION type. Hence,
  the check is deferred until we have the previous ON COMPLETION type
  from the event-db to fall back on if nothing was specified in the
  ALTER EVENT-statement.

  SYNOPSIS
    Event_parse_data::check_dates()
      thd            Thread
      on_completion  ON COMPLETION value currently in event-db.
                     Will be overridden by value in ALTER EVENT if given.

  RETURN VALUE
    TRUE            an error occurred, do not ALTER
    FALSE           OK
*/

bool
Event_parse_data::check_dates(THD *thd, int previous_on_completion)
{
  if (on_completion == Event_parse_data::ON_COMPLETION_DEFAULT)
  {
    on_completion= previous_on_completion;
    if (!ends_null)
      check_if_in_the_past(thd, ends);
    if (!execute_at_null)
      check_if_in_the_past(thd, execute_at);
  }
  return do_not_create;
}



/*
  Sets time for execution for one-time event.

  SYNOPSIS
    Event_parse_data::init_execute_at()
      thd  Thread

  RETURN VALUE
    0               OK
    ER_WRONG_VALUE  Wrong value for execute at (reported)
*/

int
Event_parse_data::init_execute_at(THD *thd)
{
  my_bool not_used;
  MYSQL_TIME ltime;
  my_time_t ltime_utc;

  DBUG_ENTER("Event_parse_data::init_execute_at");

  if (!item_execute_at)
    DBUG_RETURN(0);

  if (item_execute_at->fix_fields(thd, &item_execute_at))
    goto wrong_value;

  /* no starts and/or ends in case of execute_at */
  DBUG_PRINT("info", ("starts_null && ends_null should be 1 is %d",
                      (starts_null && ends_null)));
  DBUG_ASSERT(starts_null && ends_null);

  if ((not_used= item_execute_at->get_date(&ltime, TIME_NO_ZERO_DATE)))
    goto wrong_value;

  ltime_utc= TIME_to_timestamp(thd,&ltime,&not_used);
  if (!ltime_utc)
  {
    DBUG_PRINT("error", ("Execute AT after year 2037"));
    goto wrong_value;
  }

  check_if_in_the_past(thd, ltime_utc);

  execute_at_null= FALSE;
  execute_at= ltime_utc;
  DBUG_RETURN(0);

wrong_value:
  report_bad_value("AT", item_execute_at);
  DBUG_RETURN(ER_WRONG_VALUE);
}


/*
  Sets time for execution of multi-time event.s

  SYNOPSIS
    Event_parse_data::init_interval()
      thd  Thread

  RETURN VALUE
    0                OK
    EVEX_BAD_PARAMS  Interval is not positive or MICROSECOND (reported)
    ER_WRONG_VALUE   Wrong value for interval (reported)
*/

int
Event_parse_data::init_interval(THD *thd)
{
  String value;
  INTERVAL interval_tmp;

  DBUG_ENTER("Event_parse_data::init_interval");
  if (!item_expression)
    DBUG_RETURN(0);

  switch (interval) {
  case INTERVAL_MINUTE_MICROSECOND:
  case INTERVAL_HOUR_MICROSECOND:
  case INTERVAL_DAY_MICROSECOND:
  case INTERVAL_SECOND_MICROSECOND:
  case INTERVAL_MICROSECOND:
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "MICROSECOND");
    DBUG_RETURN(EVEX_BAD_PARAMS);
  default:
    break;
  }

  if (item_expression->fix_fields(thd, &item_expression))
    goto wrong_value;

  value.alloc(MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN);
  if (get_interval_value(item_expression, interval, &value, &interval_tmp))
    goto wrong_value;

  expression= 0;

  switch (interval) {
  case INTERVAL_YEAR:
    expression= interval_tmp.year;
    break;
  case INTERVAL_QUARTER:
  case INTERVAL_MONTH:
    expression= interval_tmp.month;
    break;
  case INTERVAL_WEEK:
  case INTERVAL_DAY:
    expression= interval_tmp.day;
    break;
  case INTERVAL_HOUR:
    expression= interval_tmp.hour;
    break;
  case INTERVAL_MINUTE:
    expression= interval_tmp.minute;
    break;
  case INTERVAL_SECOND:
    expression= interval_tmp.second;
    break;
  case INTERVAL_YEAR_MONTH:                     // Allow YEAR-MONTH YYYYYMM
    expression= interval_tmp.year* 12 + interval_tmp.month;
    break;
  case INTERVAL_DAY_HOUR:
    expression= interval_tmp.day* 24 + interval_tmp.hour;
    break;
  case INTERVAL_DAY_MINUTE:
    expression= (interval_tmp.day* 24 + interval_tmp.hour) * 60 +
                interval_tmp.minute;
    break;
  case INTERVAL_HOUR_SECOND: /* day is anyway 0 */
  case INTERVAL_DAY_SECOND:
    /* DAY_SECOND having problems because of leap seconds? */
    expression= ((interval_tmp.day* 24 + interval_tmp.hour) * 60 +
                  interval_tmp.minute)*60
                 + interval_tmp.second;
    break;
  case INTERVAL_HOUR_MINUTE:
    expression= interval_tmp.hour * 60 + interval_tmp.minute;
    break;
  case INTERVAL_MINUTE_SECOND:
    expression= interval_tmp.minute * 60 + interval_tmp.second;
    break;
  case INTERVAL_LAST:
    DBUG_ASSERT(0);
  default:
    ;/* these are the microsec stuff */
  }
  if (interval_tmp.neg || expression == 0 ||
      expression > EVEX_MAX_INTERVAL_VALUE)
  {
    my_error(ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG, MYF(0));
    DBUG_RETURN(EVEX_BAD_PARAMS);
  }

  DBUG_RETURN(0);

wrong_value:
  report_bad_value("INTERVAL", item_expression);
  DBUG_RETURN(ER_WRONG_VALUE);
}


/*
  Sets STARTS.

  SYNOPSIS
    Event_parse_data::init_starts()
      expr      how much?

  NOTES
    Note that activation time is not execution time.
    EVERY 5 MINUTE STARTS "2004-12-12 10:00:00" means that
    the event will be executed every 5 minutes but this will
    start at the date shown above. Expressions are possible :
    DATE_ADD(NOW(), INTERVAL 1 DAY)  -- start tommorow at
    same time.

  RETURN VALUE
    0                OK
    ER_WRONG_VALUE  Starts before now
*/

int
Event_parse_data::init_starts(THD *thd)
{
  my_bool not_used;
  MYSQL_TIME ltime;
  my_time_t ltime_utc;

  DBUG_ENTER("Event_parse_data::init_starts");
  if (!item_starts)
    DBUG_RETURN(0);

  if (item_starts->fix_fields(thd, &item_starts))
    goto wrong_value;

  if ((not_used= item_starts->get_date(&ltime, TIME_NO_ZERO_DATE)))
    goto wrong_value;

  ltime_utc= TIME_to_timestamp(thd, &ltime, &not_used);
  if (!ltime_utc)
    goto wrong_value;

  DBUG_PRINT("info",("now: %ld  starts: %ld",
                     (long) thd->query_start(), (long) ltime_utc));

  starts_null= FALSE;
  starts= ltime_utc;
  DBUG_RETURN(0);

wrong_value:
  report_bad_value("STARTS", item_starts);
  DBUG_RETURN(ER_WRONG_VALUE);
}


/*
  Sets ENDS (deactivation time).

  SYNOPSIS
    Event_parse_data::init_ends()
      thd       THD

  NOTES
    Note that activation time is not execution time.
    EVERY 5 MINUTE ENDS "2004-12-12 10:00:00" means that
    the event will be executed every 5 minutes but this will
    end at the date shown above. Expressions are possible :
    DATE_ADD(NOW(), INTERVAL 1 DAY)  -- end tommorow at
    same time.

  RETURN VALUE
    0                  OK
    EVEX_BAD_PARAMS    Error (reported)
*/

int
Event_parse_data::init_ends(THD *thd)
{
  my_bool not_used;
  MYSQL_TIME ltime;
  my_time_t ltime_utc;

  DBUG_ENTER("Event_parse_data::init_ends");
  if (!item_ends)
    DBUG_RETURN(0);

  if (item_ends->fix_fields(thd, &item_ends))
    goto error_bad_params;

  DBUG_PRINT("info", ("convert to TIME"));
  if ((not_used= item_ends->get_date(&ltime, TIME_NO_ZERO_DATE)))
    goto error_bad_params;

  ltime_utc= TIME_to_timestamp(thd, &ltime, &not_used);
  if (!ltime_utc)
    goto error_bad_params;

  /* Check whether ends is after starts */
  DBUG_PRINT("info", ("ENDS after STARTS?"));
  if (!starts_null && starts >= ltime_utc)
    goto error_bad_params;

  check_if_in_the_past(thd, ltime_utc);

  ends_null= FALSE;
  ends= ltime_utc;
  DBUG_RETURN(0);

error_bad_params:
  my_error(ER_EVENT_ENDS_BEFORE_STARTS, MYF(0));
  DBUG_RETURN(EVEX_BAD_PARAMS);
}


/*
  Prints an error message about invalid value. Internally used
  during input data verification

  SYNOPSIS
    Event_parse_data::report_bad_value()
      item_name The name of the parameter
      bad_item  The parameter
*/

void
Event_parse_data::report_bad_value(const char *item_name, Item *bad_item)
{
  char buff[120];
  String str(buff,(uint32) sizeof(buff), system_charset_info);
  String *str2= bad_item->fixed? bad_item->val_str(&str):NULL;
  my_error(ER_WRONG_VALUE, MYF(0), item_name, str2? str2->c_ptr_safe():"NULL");
}


/*
  Checks for validity the data gathered during the parsing phase.

  SYNOPSIS
    Event_parse_data::check_parse_data()
      thd  Thread

  RETURN VALUE
    FALSE  OK
    TRUE   Error (reported)
*/

bool
Event_parse_data::check_parse_data(THD *thd)
{
  bool ret;
  DBUG_ENTER("Event_parse_data::check_parse_data");
  DBUG_PRINT("info", ("execute_at: 0x%lx  expr=0x%lx  starts=0x%lx  ends=0x%lx",
                      (long) item_execute_at, (long) item_expression,
                      (long) item_starts, (long) item_ends));

  init_name(thd, identifier);

  init_definer(thd);

  ret= init_execute_at(thd) || init_interval(thd) || init_starts(thd) ||
       init_ends(thd);
  check_originator_id(thd);
  DBUG_RETURN(ret);
}


/*
  Inits definer (definer_user and definer_host) during parsing.

  SYNOPSIS
    Event_parse_data::init_definer()
      thd  Thread
*/

void
Event_parse_data::init_definer(THD *thd)
{
  DBUG_ENTER("Event_parse_data::init_definer");

  DBUG_ASSERT(thd->lex->definer);

  const char *definer_user= thd->lex->definer->user.str;
  const char *definer_host= thd->lex->definer->host.str;
  size_t  definer_user_len= thd->lex->definer->user.length;
  size_t  definer_host_len= thd->lex->definer->host.length;

  DBUG_PRINT("info",("init definer_user thd->mem_root: 0x%lx  "
                     "definer_user: 0x%lx", (long) thd->mem_root,
                     (long) definer_user));

  /* + 1 for @ */
  DBUG_PRINT("info",("init definer as whole"));
  definer.length= definer_user_len + definer_host_len + 1;
  definer.str= (char*) thd->alloc(definer.length + 1);

  DBUG_PRINT("info",("copy the user"));
  memcpy(definer.str, definer_user, definer_user_len);
  definer.str[definer_user_len]= '@';

  DBUG_PRINT("info",("copy the host"));
  memcpy(definer.str + definer_user_len + 1, definer_host, definer_host_len);
  definer.str[definer.length]= '\0';
  DBUG_PRINT("info",("definer [%s] initted", definer.str));

  DBUG_VOID_RETURN;
}


/**
  Set the originator id of the event to the server_id if executing on
  the master or set to the server_id of the master if executing on 
  the slave. If executing on slave, also set status to SLAVESIDE_DISABLED.

  SYNOPSIS
    Event_parse_data::check_originator_id()
*/
void Event_parse_data::check_originator_id(THD *thd)
{
  /* Disable replicated events on slave. */
  if ((thd->system_thread == SYSTEM_THREAD_SLAVE_SQL) ||
      (thd->system_thread == SYSTEM_THREAD_SLAVE_IO))
  {
    DBUG_PRINT("info", ("Invoked object status set to SLAVESIDE_DISABLED."));
    if ((status == Event_parse_data::ENABLED) ||
        (status == Event_parse_data::DISABLED))
    {
      status= Event_parse_data::SLAVESIDE_DISABLED;
      status_changed= true;
    }
    originator = thd->server_id;
  }
  else
    originator = server_id;
}
