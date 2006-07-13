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

#define MYSQL_LEX 1
#include "mysql_priv.h"
#include "events.h"
#include "event_data_objects.h"
#include "event_db_repository.h"
#include "sp_head.h"


#define EVEX_MAX_INTERVAL_VALUE 1000000000L


/*
  Switches the security context
  SYNOPSIS
    event_change_security_context()
      thd     Thread
      user    The user
      host    The host of the user
      db      The schema for which the security_ctx will be loaded
      backup  Where to store the old context
  
  RETURN VALUE
    FALSE  OK
    TRUE   Error (generates error too)
*/

static bool
event_change_security_context(THD *thd, LEX_STRING user, LEX_STRING host,
                              LEX_STRING db, Security_context *backup)
{
  DBUG_ENTER("event_change_security_context");
  DBUG_PRINT("info",("%s@%s@%s", user.str, host.str, db.str));
#ifndef NO_EMBEDDED_ACCESS_CHECKS

  *backup= thd->main_security_ctx;
  if (acl_getroot_no_password(&thd->main_security_ctx, user.str, host.str,
                              host.str, db.str))
  {
    my_error(ER_NO_SUCH_USER, MYF(0), user.str, host.str);
    DBUG_RETURN(TRUE);
  }
  thd->security_ctx= &thd->main_security_ctx;
#endif
  DBUG_RETURN(FALSE);
} 


/*
  Restores the security context
  SYNOPSIS
    event_restore_security_context()
      thd     Thread
      backup  Context to switch to
*/

static void
event_restore_security_context(THD *thd, Security_context *backup)
{
  DBUG_ENTER("event_restore_security_context");
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (backup)
  {
    thd->main_security_ctx= *backup;
    thd->security_ctx= &thd->main_security_ctx;
  }
#endif
  DBUG_VOID_RETURN;
}


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
{
  DBUG_ENTER("Event_parse_data::Event_parse_data");

  item_execute_at= item_expression= item_starts= item_ends= NULL;
  status= ENABLED;
  on_completion= ON_COMPLETION_DROP;
  expression= 0;

  /* Actually in the parser STARTS is always set */
  set_zero_time(&starts, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&ends, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
  starts_null= ends_null= execute_at_null= TRUE;

  body.str= comment.str= NULL;
  body.length= comment.length= 0;

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
  Set body of the event - what should be executed.

  SYNOPSIS
    Event_parse_data::init_body()
      thd   THD

  NOTE
    The body is extracted by copying all data between the
    start of the body set by another method and the current pointer in Lex.

    Some questionable removal of characters is done in here, and that part
    should be refactored when the parser is smarter.
*/

void
Event_parse_data::init_body(THD *thd)
{
  DBUG_ENTER("Event_parse_data::init_body");
  DBUG_PRINT("info", ("body=[%s] body_begin=0x%lx end=0x%lx", body_begin,
             body_begin, thd->lex->ptr));

  body.length= thd->lex->ptr - body_begin;
  const uchar *body_end= body_begin + body.length - 1;

  /* Trim nuls or close-comments ('*'+'/') or spaces at the end */
  while (body_begin < body_end)
  {

    if ((*body_end == '\0') || 
        (my_isspace(thd->variables.character_set_client, *body_end)))
    { /* consume NULs and meaningless whitespace */
      --body.length;
      --body_end;
      continue;
    }

    /*
       consume closing comments

       This is arguably wrong, but it's the best we have until the parser is
       changed to be smarter.   FIXME PARSER 

       See also the sp_head code, where something like this is done also.

       One idea is to keep in the lexer structure the count of the number of
       open-comments we've entered, and scan left-to-right looking for a
       closing comment IFF the count is greater than zero.

       Another idea is to remove the closing comment-characters wholly in the
       parser, since that's where it "removes" the opening characters.
    */
    if ((*(body_end - 1) == '*') && (*body_end == '/'))
    {
      DBUG_PRINT("info", ("consumend one '*" "/' comment in the query '%s'", 
          body_begin));
      body.length-= 2;
      body_end-= 2;
      continue;
    }

    break;  /* none were found, so we have excised all we can. */
  }

  /* the first is always whitespace which I cannot skip in the parser */
  while (my_isspace(thd->variables.character_set_client, *body_begin))
  {
    ++body_begin;
    --body.length;
  }
  body.str= thd->strmake((char *)body_begin, body.length);

  DBUG_VOID_RETURN;
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
  int definer_user_len;
  int definer_host_len;
  DBUG_ENTER("Event_parse_data::init_definer");

  DBUG_PRINT("info",("init definer_user thd->mem_root=0x%lx "
                     "thd->sec_ctx->priv_user=0x%lx", thd->mem_root,
                     thd->security_ctx->priv_user));

  definer_user_len= strlen(thd->security_ctx->priv_user);
  definer_host_len= strlen(thd->security_ctx->priv_host);

  /* + 1 for @ */
  DBUG_PRINT("info",("init definer as whole"));
  definer.length= definer_user_len + definer_host_len + 1;
  definer.str= thd->alloc(definer.length + 1);

  DBUG_PRINT("info",("copy the user"));
  memcpy(definer.str, thd->security_ctx->priv_user, definer_user_len);
  definer.str[definer_user_len]= '@';

  DBUG_PRINT("info",("copy the host"));
  memcpy(definer.str + definer_user_len + 1, thd->security_ctx->priv_host,
         definer_host_len);
  definer.str[definer.length]= '\0';
  DBUG_PRINT("info",("definer [%s] initted", definer.str));

  DBUG_VOID_RETURN;
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
  TIME ltime;
  my_time_t t;
  TIME time_tmp;

  DBUG_ENTER("Event_parse_data::init_execute_at");

  if (!item_execute_at)
    DBUG_RETURN(0);

  if (item_execute_at->fix_fields(thd, &item_execute_at))
    goto wrong_value;
  
  /* no starts and/or ends in case of execute_at */
  DBUG_PRINT("info", ("starts_null && ends_null should be 1 is %d",
                      (starts_null && ends_null)));
  DBUG_ASSERT(starts_null && ends_null);

  /* let's check whether time is in the past */
  thd->variables.time_zone->gmt_sec_to_TIME(&time_tmp, 
                                            (my_time_t) thd->query_start());

  if ((not_used= item_execute_at->get_date(&ltime, TIME_NO_ZERO_DATE)))
    goto wrong_value;

  if (TIME_to_ulonglong_datetime(&ltime) <
      TIME_to_ulonglong_datetime(&time_tmp))
  {
    my_error(ER_EVENT_EXEC_TIME_IN_THE_PAST, MYF(0));
    DBUG_RETURN(ER_WRONG_VALUE);
  }

  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx.
    CONVERT_TZ has similar problem.
    mysql_priv.h currently lists 
      #define TIMESTAMP_MAX_YEAR 2038 (see TIME_to_timestamp())
  */
  my_tz_UTC->gmt_sec_to_TIME(&ltime,t=TIME_to_timestamp(thd,&ltime,&not_used));
  if (!t)
  {
    DBUG_PRINT("error", ("Execute AT after year 2037"));
    goto wrong_value;
  }

  execute_at_null= FALSE;
  execute_at= ltime;
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
  if (interval_tmp.neg || expression > EVEX_MAX_INTERVAL_VALUE)
  {
    my_error(ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG, MYF(0));
    DBUG_RETURN(EVEX_BAD_PARAMS);
  }

  DBUG_RETURN(0);

wrong_value:
  report_bad_value("INTERVAL", item_execute_at);
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
  TIME ltime, time_tmp;
  my_time_t t;

  DBUG_ENTER("Event_parse_data::init_starts");
  if (!item_starts)
    DBUG_RETURN(0);

  if (item_starts->fix_fields(thd, &item_starts))
    goto wrong_value;

  if ((not_used= item_starts->get_date(&ltime, TIME_NO_ZERO_DATE)))
    goto wrong_value;

  /* Let's check whether time is in the past */
  thd->variables.time_zone->gmt_sec_to_TIME(&time_tmp,
                                            (my_time_t) thd->query_start());

  DBUG_PRINT("info",("now   =%lld", TIME_to_ulonglong_datetime(&time_tmp)));
  DBUG_PRINT("info",("starts=%lld", TIME_to_ulonglong_datetime(&ltime)));
  if (TIME_to_ulonglong_datetime(&ltime) <
      TIME_to_ulonglong_datetime(&time_tmp))
    goto wrong_value;

  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx.
    CONVERT_TZ has similar problem.
    mysql_priv.h currently lists 
      #define TIMESTAMP_MAX_YEAR 2038 (see TIME_to_timestamp())
  */
  my_tz_UTC->gmt_sec_to_TIME(&ltime,t=TIME_to_timestamp(thd, &ltime, &not_used));
  if (!t)
    goto wrong_value;

  starts= ltime;
  starts_null= FALSE;
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
  TIME ltime, ltime_now;
  my_bool not_used;
  my_time_t t;

  DBUG_ENTER("Event_parse_data::init_ends");
  if (!item_ends)
    DBUG_RETURN(0);

  if (item_ends->fix_fields(thd, &item_ends))
    goto error_bad_params;

  DBUG_PRINT("info", ("convert to TIME"));
  if ((not_used= item_ends->get_date(&ltime, TIME_NO_ZERO_DATE)))
    goto error_bad_params;

  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx.
    CONVERT_TZ has similar problem.
    mysql_priv.h currently lists 
      #define TIMESTAMP_MAX_YEAR 2038 (see TIME_to_timestamp())
  */
  DBUG_PRINT("info", ("get the UTC time"));
  my_tz_UTC->gmt_sec_to_TIME(&ltime,t=TIME_to_timestamp(thd, &ltime, &not_used));
  if (!t)
    goto error_bad_params;

  /* Check whether ends is after starts */
  DBUG_PRINT("info", ("ENDS after STARTS?"));
  if (!starts_null && my_time_compare(&starts, &ltime) != -1)
    goto error_bad_params;

  /*
    The parser forces starts to be provided but one day STARTS could be
    set before NOW() and in this case the following check should be done.
    Check whether ENDS is not in the past.
  */
  DBUG_PRINT("info", ("ENDS after NOW?"));
  my_tz_UTC->gmt_sec_to_TIME(&ltime_now, thd->query_start());
  if (my_time_compare(&ltime_now, &ltime) == 1)
    goto error_bad_params;

  ends= ltime;
  ends_null= FALSE;
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
  DBUG_PRINT("info", ("execute_at=0x%lx expr=0x%lx starts=0x%lx ends=0x%lx",
             item_execute_at, item_expression, item_starts, item_ends));

  init_name(thd, identifier);

  init_definer(thd);

  ret= init_execute_at(thd) || init_interval(thd) || init_starts(thd) ||
       init_ends(thd);
  DBUG_RETURN(ret);
}


/*
  Constructor

  SYNOPSIS
    Event_basic::Event_basic()
*/

Event_basic::Event_basic()
{
  DBUG_ENTER("Event_basic::Event_basic");
  /* init memory root */
  init_alloc_root(&mem_root, 256, 512);
  dbname.str= name.str= NULL;
  dbname.length= name.length= 0;
  DBUG_VOID_RETURN;
}


/*
  Destructor

  SYNOPSIS
    Event_basic::Event_basic()
*/

Event_basic::~Event_basic()
{
  DBUG_ENTER("Event_basic::~Event_basic");
  free_root(&mem_root, MYF(0));
  DBUG_VOID_RETURN;
}


/*
  Short function to load a char column into a LEX_STRING

  SYNOPSIS
    Event_basic::load_string_field()
      field_name  The field( enum_events_table_field is not actually used
                  because it's unknown in event_data_objects.h)
      fields      The Field array
      field_value The value
*/

bool
Event_basic::load_string_fields(Field **fields, ...)
{
  bool ret= FALSE;
  va_list args;
  enum enum_events_table_field field_name;
  LEX_STRING *field_value;

  DBUG_ENTER("Event_basic::load_string_fields");

  va_start(args, fields);
  field_name= (enum enum_events_table_field) va_arg(args, int);
  while (field_name != ET_FIELD_COUNT)
  {
    field_value= va_arg(args, LEX_STRING *);
    if ((field_value->str= get_field(&mem_root, fields[field_name])) == NullS)
    {
      ret= TRUE;
      break;
    }
    field_value->length= strlen(field_value->str);  

    field_name= (enum enum_events_table_field) va_arg(args, int);
  }
  va_end(args);

  DBUG_RETURN(ret);
}


/*
  Constructor

  SYNOPSIS
    Event_queue_element::Event_queue_element()
*/

Event_queue_element::Event_queue_element():
  status_changed(FALSE), last_executed_changed(FALSE),
  on_completion(ON_COMPLETION_DROP), status(ENABLED),
  expression(0), dropped(FALSE), flags(0)
{
  DBUG_ENTER("Event_queue_element::Event_queue_element");

  set_zero_time(&starts, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&ends, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&last_executed, MYSQL_TIMESTAMP_DATETIME);
  starts_null= ends_null= execute_at_null= TRUE;

  DBUG_VOID_RETURN;
}


/*
  Destructor

  SYNOPSIS
    Event_queue_element::Event_queue_element()
*/
Event_queue_element::~Event_queue_element()
{
}


/*
  Constructor

  SYNOPSIS
    Event_timed::Event_timed()
*/

Event_timed::Event_timed():
  created(0), modified(0), sql_mode(0)
{
  DBUG_ENTER("Event_timed::Event_timed");
  init();
  DBUG_VOID_RETURN;
}


/*
  Destructor

  SYNOPSIS
    Event_timed::~Event_timed()
*/

Event_timed::~Event_timed()
{    
}


/*
  Constructor

  SYNOPSIS
    Event_job_data::Event_job_data()
*/

Event_job_data::Event_job_data():
  thd(NULL), sphead(NULL), sql_mode(0)
{
}


/*
  Destructor

  SYNOPSIS
    Event_timed::~Event_timed()
*/

Event_job_data::~Event_job_data()
{
  DBUG_ENTER("Event_job_data::~Event_job_data");
  delete sphead;
  sphead= NULL;
  DBUG_VOID_RETURN;
}


/*
  Init all member variables

  SYNOPSIS
    Event_timed::init()
*/

void
Event_timed::init()
{
  DBUG_ENTER("Event_timed::init");

  definer_user.str= definer_host.str= body.str= comment.str= NULL;
  definer_user.length= definer_host.length= body.length= comment.length= 0;

  sql_mode= 0;

  DBUG_VOID_RETURN;
}


/*
  Loads an event's body from a row from mysql.event

  SYNOPSIS
    Event_job_data::load_from_row(MEM_ROOT *mem_root, TABLE *table)

  RETURN VALUE
    0                      OK
    EVEX_GET_FIELD_FAILED  Error

  NOTES
    This method is silent on errors and should behave like that. Callers
    should handle throwing of error messages. The reason is that the class
    should not know about how to deal with communication.
*/

int
Event_job_data::load_from_row(TABLE *table)
{
  char *ptr;
  uint len;
  DBUG_ENTER("Event_job_data::load_from_row");

  if (!table)
    goto error;

  if (table->s->fields != ET_FIELD_COUNT)
    goto error;

  load_string_fields(table->field, ET_FIELD_DB, &dbname, ET_FIELD_NAME, &name,
                     ET_FIELD_BODY, &body, ET_FIELD_DEFINER, &definer,
                     ET_FIELD_COUNT);

  ptr= strchr(definer.str, '@');

  if (! ptr)
    ptr= definer.str;

  len= ptr - definer.str;
  definer_user.str= strmake_root(&mem_root, definer.str, len);
  definer_user.length= len;
  len= definer.length - len - 1;
  /* 1:because of @ */
  definer_host.str= strmake_root(&mem_root, ptr + 1, len);
  definer_host.length= len;

  sql_mode= (ulong) table->field[ET_FIELD_SQL_MODE]->val_int();

  DBUG_RETURN(0);
error:
  DBUG_RETURN(EVEX_GET_FIELD_FAILED);
}


/*
  Loads an event from a row from mysql.event

  SYNOPSIS
    Event_queue_element::load_from_row(MEM_ROOT *mem_root, TABLE *table)

  RETURN VALUE
    0                      OK
    EVEX_GET_FIELD_FAILED  Error

  NOTES
    This method is silent on errors and should behave like that. Callers
    should handle throwing of error messages. The reason is that the class
    should not know about how to deal with communication.
*/

int
Event_queue_element::load_from_row(TABLE *table)
{
  char *ptr;
  bool res1, res2;

  DBUG_ENTER("Event_queue_element::load_from_row");

  if (!table)
    goto error;

  if (table->s->fields != ET_FIELD_COUNT)
    goto error;

  load_string_fields(table->field, ET_FIELD_DB, &dbname, ET_FIELD_NAME, &name,
                     ET_FIELD_DEFINER, &definer, ET_FIELD_COUNT);

  starts_null= table->field[ET_FIELD_STARTS]->is_null();
  res1= table->field[ET_FIELD_STARTS]->get_date(&starts, TIME_NO_ZERO_DATE);

  ends_null= table->field[ET_FIELD_ENDS]->is_null();
  res2= table->field[ET_FIELD_ENDS]->get_date(&ends, TIME_NO_ZERO_DATE);

  if (!table->field[ET_FIELD_INTERVAL_EXPR]->is_null())
    expression= table->field[ET_FIELD_INTERVAL_EXPR]->val_int();
  else
    expression= 0;
  /*
    If res1 and res2 are TRUE then both fields are empty.
    Hence, if ET_FIELD_EXECUTE_AT is empty there is an error.
  */
  execute_at_null= table->field[ET_FIELD_EXECUTE_AT]->is_null();
  DBUG_ASSERT(!(starts_null && ends_null && !expression && execute_at_null));
  if (!expression &&
      table->field[ET_FIELD_EXECUTE_AT]->get_date(&execute_at,
                                                  TIME_NO_ZERO_DATE))
    goto error;

  /*
    In DB the values start from 1 but enum interval_type starts
    from 0
  */
  if (!table->field[ET_FIELD_TRANSIENT_INTERVAL]->is_null())
    interval= (interval_type) ((ulonglong)
          table->field[ET_FIELD_TRANSIENT_INTERVAL]->val_int() - 1);
  else
    interval= (interval_type) 0;

  table->field[ET_FIELD_LAST_EXECUTED]->get_date(&last_executed,
                                                 TIME_NO_ZERO_DATE);
  last_executed_changed= FALSE;


  if ((ptr= get_field(&mem_root, table->field[ET_FIELD_STATUS])) == NullS)
    goto error;

  DBUG_PRINT("load_from_row", ("Event [%s] is [%s]", name.str, ptr));
  status= (ptr[0]=='E'? Event_queue_element::ENABLED:
                        Event_queue_element::DISABLED);

  /* ToDo : Andrey . Find a way not to allocate ptr on event_mem_root */
  if ((ptr= get_field(&mem_root,
                      table->field[ET_FIELD_ON_COMPLETION])) == NullS)
    goto error;

  on_completion= (ptr[0]=='D'? Event_queue_element::ON_COMPLETION_DROP:
                               Event_queue_element::ON_COMPLETION_PRESERVE);

  DBUG_RETURN(0);
error:
  DBUG_RETURN(EVEX_GET_FIELD_FAILED);
}


/*
  Loads an event from a row from mysql.event

  SYNOPSIS
    Event_timed::load_from_row(MEM_ROOT *mem_root, TABLE *table)

  RETURN VALUE
    0                      OK
    EVEX_GET_FIELD_FAILED  Error

  NOTES
    This method is silent on errors and should behave like that. Callers
    should handle throwing of error messages. The reason is that the class
    should not know about how to deal with communication.
*/

int
Event_timed::load_from_row(TABLE *table)
{
  char *ptr;
  uint len;

  DBUG_ENTER("Event_timed::load_from_row");

  if (Event_queue_element::load_from_row(table))
    goto error;

  load_string_fields(table->field, ET_FIELD_BODY, &body, ET_FIELD_COUNT);

  ptr= strchr(definer.str, '@');

  if (! ptr)
    ptr= definer.str;

  len= ptr - definer.str;
  definer_user.str= strmake_root(&mem_root, definer.str, len);
  definer_user.length= len;
  len= definer.length - len - 1;
  /* 1:because of @ */
  definer_host.str= strmake_root(&mem_root, ptr + 1,  len);
  definer_host.length= len;

  created= table->field[ET_FIELD_CREATED]->val_int();
  modified= table->field[ET_FIELD_MODIFIED]->val_int();

  comment.str= get_field(&mem_root, table->field[ET_FIELD_COMMENT]);
  if (comment.str != NullS)
    comment.length= strlen(comment.str);
  else
    comment.length= 0;

  sql_mode= (ulong) table->field[ET_FIELD_SQL_MODE]->val_int();

  DBUG_RETURN(0);
error:
  DBUG_RETURN(EVEX_GET_FIELD_FAILED);
}


/*
  Computes the sum of a timestamp plus interval. Presumed is that at least one
  previous execution has occured.

  SYNOPSIS
    get_next_time(TIME *start, int interval_value, interval_type interval)
      next          the sum
      start         add interval_value to this time
      time_now      current time
      i_value       quantity of time type interval to add
      i_type        type of interval to add (SECOND, MINUTE, HOUR, WEEK ...)

  RETURN VALUE
    0  OK
    1  Error

  NOTES
    1) If the interval is conversible to SECOND, like MINUTE, HOUR, DAY, WEEK.
       Then we use TIMEDIFF()'s implementation as underlying and number of
       seconds as resolution for computation.
    2) In all other cases - MONTH, QUARTER, YEAR we use MONTH as resolution
       and PERIOD_DIFF()'s implementation
    3) We get the difference between time_now and `start`, then divide it
       by the months, respectively seconds and round up. Then we multiply
       monts/seconds by the rounded value and add it to `start` -> we get
       the next execution time.
*/

static
bool get_next_time(TIME *next, TIME *start, TIME *time_now, TIME *last_exec,
                   int i_value, interval_type i_type)
{
  bool ret;
  INTERVAL interval;
  TIME tmp;
  longlong months=0, seconds=0;
  DBUG_ENTER("get_next_time");
  DBUG_PRINT("enter", ("start=%llu now=%llu", TIME_to_ulonglong_datetime(start),
                      TIME_to_ulonglong_datetime(time_now)));

  bzero(&interval, sizeof(interval));

  switch (i_type) {
  case INTERVAL_YEAR:
    months= i_value*12;
    break;
  case INTERVAL_QUARTER:
    /* Has already been converted to months */
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_MONTH:
    months= i_value;
    break;
  case INTERVAL_WEEK:
    /* WEEK has already been converted to days */
  case INTERVAL_DAY:
    seconds= i_value*24*3600;
    break;
  case INTERVAL_DAY_HOUR:
  case INTERVAL_HOUR:
    seconds= i_value*3600;
    break;
  case INTERVAL_DAY_MINUTE:
  case INTERVAL_HOUR_MINUTE:
  case INTERVAL_MINUTE:
    seconds= i_value*60;
    break;
  case INTERVAL_DAY_SECOND:
  case INTERVAL_HOUR_SECOND:
  case INTERVAL_MINUTE_SECOND:
  case INTERVAL_SECOND:
    seconds= i_value;
    break;
  case INTERVAL_DAY_MICROSECOND:
  case INTERVAL_HOUR_MICROSECOND:
  case INTERVAL_MINUTE_MICROSECOND:
  case INTERVAL_SECOND_MICROSECOND:
  case INTERVAL_MICROSECOND:
    /*
     We should return an error here so SHOW EVENTS/ SELECT FROM I_S.EVENTS
     would give an error then.
    */
    DBUG_RETURN(1);
    break;
  case INTERVAL_LAST:
    DBUG_ASSERT(0);
  }
  DBUG_PRINT("info", ("seconds=%ld months=%ld", seconds, months));
  if (seconds)
  {
    longlong seconds_diff;
    long microsec_diff;
    
    if (calc_time_diff(time_now, start, 1, &seconds_diff, &microsec_diff))
    {
      DBUG_PRINT("error", ("negative difference"));
      DBUG_ASSERT(0);
    }
    uint multiplier= seconds_diff / seconds;
    /*
      Increase the multiplier is the modulus is not zero to make round up.
      Or if time_now==start then we should not execute the same 
      event two times for the same time
      get the next exec if the modulus is not
    */
    DBUG_PRINT("info", ("multiplier=%d", multiplier));
    if (seconds_diff % seconds || (!seconds_diff && last_exec->year) ||
        TIME_to_ulonglong_datetime(time_now) ==
          TIME_to_ulonglong_datetime(last_exec))
      ++multiplier;
    interval.second= seconds * multiplier;
    DBUG_PRINT("info", ("multiplier=%u interval.second=%u", multiplier,
                        interval.second));
    tmp= *start;
    if (!(ret= date_add_interval(&tmp, INTERVAL_SECOND, interval)))
      *next= tmp;
  }
  else
  {
    /* PRESUMED is that at least one execution took already place */
    int diff_months= (time_now->year - start->year)*12 +
                     (time_now->month - start->month);
    /*
      Note: If diff_months is 0 that means we are in the same month as the
      last execution which is also the first execution.
    */
    /*
      First we try with the smaller if not then + 1, because if we try with
      directly with +1 we will be after the current date but it could be that
      we will be 1 month ahead, so 2 steps are necessary.
    */
    interval.month= (diff_months / months)*months;
    /*
      Check if the same month as last_exec (always set - prerequisite)
      An event happens at most once per month so there is no way to schedule
      it two times for the current month. This saves us from two calls to
      date_add_interval() if the event was just executed.  But if the scheduler
      is started and there was at least 1 scheduled date skipped this one does
      not help and two calls to date_add_interval() will be done, which is a
      bit more expensive but compared to the rareness of the case is neglectable.
    */
    if (time_now->year==last_exec->year && time_now->month==last_exec->month)
      interval.month+= months;

    tmp= *start;
    if ((ret= date_add_interval(&tmp, INTERVAL_MONTH, interval)))
      goto done;

    /* If `tmp` is still before time_now just add one more time the interval */
    if (my_time_compare(&tmp, time_now) == -1)
    { 
      interval.month+= months;
      tmp= *start;
      if ((ret= date_add_interval(&tmp, INTERVAL_MONTH, interval)))
        goto done;
    }
    *next= tmp;
    /* assert on that the next is after now */
    DBUG_ASSERT(1==my_time_compare(next, time_now));
  }

done:
  DBUG_PRINT("info", ("next=%llu", TIME_to_ulonglong_datetime(next)));
  DBUG_RETURN(ret);
}


/*
  Computes next execution time.

  SYNOPSIS
    Event_queue_element::compute_next_execution_time()

  RETURN VALUE
    FALSE  OK
    TRUE   Error

  NOTES
    The time is set in execute_at, if no more executions the latter is set to
    0000-00-00.
*/

bool
Event_queue_element::compute_next_execution_time()
{
  TIME time_now;
  int tmp;

  DBUG_ENTER("Event_queue_element::compute_next_execution_time");
  DBUG_PRINT("enter", ("starts=%llu ends=%llu last_executed=%llu this=0x%lx",
                        TIME_to_ulonglong_datetime(&starts),
                        TIME_to_ulonglong_datetime(&ends),
                        TIME_to_ulonglong_datetime(&last_executed), this));

  if (status == Event_queue_element::DISABLED)
  {
    DBUG_PRINT("compute_next_execution_time",
                  ("Event %s is DISABLED", name.str));
    goto ret;
  }
  /* If one-time, no need to do computation */
  if (!expression)
  {
    /* Let's check whether it was executed */
    if (last_executed.year)
    {
      DBUG_PRINT("info",("One-time event %s.%s of was already executed",
                         dbname.str, name.str, definer.str));
      dropped= (on_completion == Event_queue_element::ON_COMPLETION_DROP);
      DBUG_PRINT("info",("One-time event will be dropped=%d.", dropped));

      status= Event_queue_element::DISABLED;
      status_changed= TRUE;
    }
    goto ret;
  }

  my_tz_UTC->gmt_sec_to_TIME(&time_now, current_thd->query_start());

  DBUG_PRINT("info",("NOW=[%llu]", TIME_to_ulonglong_datetime(&time_now)));

  /* if time_now is after ends don't execute anymore */
  if (!ends_null && (tmp= my_time_compare(&ends, &time_now)) == -1)
  {
    DBUG_PRINT("info", ("NOW after ENDS, don't execute anymore"));
    /* time_now is after ends. don't execute anymore */
    set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
    execute_at_null= TRUE;
    if (on_completion == Event_queue_element::ON_COMPLETION_DROP)
      dropped= TRUE;
    DBUG_PRINT("info", ("Dropped=%d", dropped));
    status= Event_queue_element::DISABLED;
    status_changed= TRUE;

    goto ret;
  }

  /*
    Here time_now is before or equals ends if the latter is set.
    Let's check whether time_now is before starts.
    If so schedule for starts.
  */
  if (!starts_null && (tmp= my_time_compare(&time_now, &starts)) < 1)
  {
    if (tmp == 0 && my_time_compare(&starts, &last_executed) == 0)
    {
      /*
        time_now = starts = last_executed
        do nothing or we will schedule for second time execution at starts.
      */
    }
    else
    {
      DBUG_PRINT("info", ("STARTS is future, NOW <= STARTS,sched for STARTS"));
      /*
        starts is in the future
        time_now before starts. Scheduling for starts
      */
      execute_at= starts;
      execute_at_null= FALSE;
      goto ret;
    }
  }

  if (!starts_null && !ends_null)
  {
    /*
      Both starts and m_ends are set and time_now is between them (incl.)
      If last_executed is set then increase with m_expression. The new TIME is
      after m_ends set execute_at to 0. And check for on_completion
      If not set then schedule for now.
    */
    DBUG_PRINT("info", ("Both STARTS & ENDS are set"));
    if (!last_executed.year)
    {
      DBUG_PRINT("info", ("Not executed so far."));
    }

    {
      TIME next_exec;

      if (get_next_time(&next_exec, &starts, &time_now,
                        last_executed.year? &last_executed:&starts,
                        expression, interval))
        goto err;

      /* There was previous execution */
      if (my_time_compare(&ends, &next_exec) == -1)
      {
        DBUG_PRINT("info", ("Next execution of %s after ENDS. Stop executing.",
                   name.str));
        /* Next execution after ends. No more executions */
        set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
        execute_at_null= TRUE;
        if (on_completion == Event_queue_element::ON_COMPLETION_DROP)
          dropped= TRUE;
        status= Event_queue_element::DISABLED;
        status_changed= TRUE;
      }
      else
      {
        DBUG_PRINT("info",("Next[%llu]",TIME_to_ulonglong_datetime(&next_exec)));
        execute_at= next_exec;
        execute_at_null= FALSE;
      }
    }
    goto ret;
  }
  else if (starts_null && ends_null) 
  {
    /* starts is always set, so this is a dead branch !! */
    DBUG_PRINT("info", ("Neither STARTS nor ENDS are set"));
    /*
      Both starts and m_ends are not set, so we schedule for the next
      based on last_executed.
    */
    if (last_executed.year)
    {
      TIME next_exec;
      if (get_next_time(&next_exec, &starts, &time_now, &last_executed,
                        expression, interval))
        goto err;
      execute_at= next_exec;
      DBUG_PRINT("info",("Next[%llu]",TIME_to_ulonglong_datetime(&next_exec)));
    }
    else
    {
      /* last_executed not set. Schedule the event for now */
      DBUG_PRINT("info", ("Execute NOW"));
      execute_at= time_now;
    }
    execute_at_null= FALSE;
  }
  else
  {
    /* either starts or m_ends is set */
    if (!starts_null)
    {
      DBUG_PRINT("info", ("STARTS is set"));
      /*
        - starts is set.
        - starts is not in the future according to check made before
        Hence schedule for starts + m_expression in case last_executed
        is not set, otherwise to last_executed + m_expression
      */
      if (!last_executed.year)
      {
        DBUG_PRINT("info", ("Not executed so far."));
      }

      {
        TIME next_exec;
        if (get_next_time(&next_exec, &starts, &time_now, 
                          last_executed.year? &last_executed:&starts,
                          expression, interval))
          goto err;
        execute_at= next_exec;
        DBUG_PRINT("info",("Next[%llu]",TIME_to_ulonglong_datetime(&next_exec)));
      }
      execute_at_null= FALSE;
    }
    else
    {
      /* this is a dead branch, because starts is always set !!! */
      DBUG_PRINT("info", ("STARTS is not set. ENDS is set"));
      /*
        - m_ends is set
        - m_ends is after time_now or is equal
        Hence check for m_last_execute and increment with m_expression.
        If last_executed is not set then schedule for now
      */

      if (!last_executed.year)
        execute_at= time_now;
      else
      {
        TIME next_exec;

        if (get_next_time(&next_exec, &starts, &time_now, &last_executed,
                          expression, interval))
          goto err;

        if (my_time_compare(&ends, &next_exec) == -1)
        {
          DBUG_PRINT("info", ("Next execution after ENDS. Stop executing."));
          set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
          execute_at_null= TRUE;
          status= Event_queue_element::DISABLED;
          status_changed= TRUE;
          if (on_completion == Event_queue_element::ON_COMPLETION_DROP)
            dropped= TRUE;
        }
        else
        {
          DBUG_PRINT("info", ("Next[%llu]",
                              TIME_to_ulonglong_datetime(&next_exec)));
          execute_at= next_exec;
          execute_at_null= FALSE;
        }
      }
    }
    goto ret;
  }
ret:
  DBUG_PRINT("info", ("ret=0 execute_at=%llu",
             TIME_to_ulonglong_datetime(&execute_at)));
  DBUG_RETURN(FALSE);
err:
  DBUG_PRINT("info", ("ret=1"));
  DBUG_RETURN(TRUE);
}


/*
  Set the internal last_executed TIME struct to now. NOW is the
  time according to thd->query_start(), so the THD's clock.

  SYNOPSIS
    Event_queue_element::mark_last_executed()
      thd   thread context
*/

void
Event_queue_element::mark_last_executed(THD *thd)
{
  TIME time_now;

  thd->end_time();
  my_tz_UTC->gmt_sec_to_TIME(&time_now, (my_time_t) thd->query_start());

  last_executed= time_now; /* was execute_at */
  last_executed_changed= TRUE;
}


/*
  Drops the event

  SYNOPSIS
    Event_queue_element::drop()
      thd   thread context

  RETURN VALUE
    0       OK
   -1       Cannot open mysql.event
   -2       Cannot find the event in mysql.event (already deleted?)

   others   return code from SE in case deletion of the event row
            failed.
*/

int
Event_queue_element::drop(THD *thd)
{
  uint tmp= 0;
  DBUG_ENTER("Event_queue_element::drop");

  DBUG_RETURN(Events::get_instance()->
                    drop_event(thd, dbname, name, FALSE, &tmp, TRUE));
}


/*
  Saves status and last_executed_at to the disk if changed.

  SYNOPSIS
    Event_queue_element::update_timing_fields()
      thd - thread context

  RETURN VALUE
    FALSE   OK
    TRUE    Error while opening mysql.event for writing or during write on disk
*/

bool
Event_queue_element::update_timing_fields(THD *thd)
{
  TABLE *table;
  Field **fields;
  Open_tables_state backup;
  int ret= FALSE;

  DBUG_ENTER("Event_queue_element::update_timing_fields");

  DBUG_PRINT("enter", ("name: %*s", name.length, name.str));

  /* No need to update if nothing has changed */
  if (!(status_changed || last_executed_changed))
    DBUG_RETURN(0);

  thd->reset_n_backup_open_tables_state(&backup);

  if (Events::get_instance()->open_event_table(thd, TL_WRITE, &table))
  {
    ret= TRUE;
    goto done;
  }
  fields= table->field;
  if ((ret= Events::get_instance()->db_repository->
                                 find_named_event(thd, dbname, name, table)))
    goto done;

  store_record(table,record[1]);
  /* Don't update create on row update. */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  if (last_executed_changed)
  {
    fields[ET_FIELD_LAST_EXECUTED]->set_notnull();
    fields[ET_FIELD_LAST_EXECUTED]->store_time(&last_executed,
                                               MYSQL_TIMESTAMP_DATETIME);
    last_executed_changed= FALSE;
  }
  if (status_changed)
  {
    fields[ET_FIELD_STATUS]->set_notnull();
    fields[ET_FIELD_STATUS]->store((longlong)status, TRUE);
    status_changed= FALSE;
  }

  if ((table->file->ha_update_row(table->record[1], table->record[0])))
    ret= TRUE;

done:
  close_thread_tables(thd);
  thd->restore_backup_open_tables_state(&backup);

  DBUG_RETURN(ret);
}


/*
  Get SHOW CREATE EVENT as string

  SYNOPSIS
    Event_timed::get_create_event(THD *thd, String *buf)
      thd    Thread
      buf    String*, should be already allocated. CREATE EVENT goes inside.

  RETURN VALUE
    0                       OK
    EVEX_MICROSECOND_UNSUP  Error (for now if mysql.event has been
                            tampered and MICROSECONDS interval or
                            derivative has been put there.
*/

int
Event_timed::get_create_event(THD *thd, String *buf)
{
  int multipl= 0;
  char tmp_buf[2 * STRING_BUFFER_USUAL_SIZE];
  String expr_buf(tmp_buf, sizeof(tmp_buf), system_charset_info);
  expr_buf.length(0);

  DBUG_ENTER("get_create_event");
  DBUG_PRINT("ret_info",("body_len=[%d]body=[%s]", body.length, body.str));

  if (expression && Events::reconstruct_interval_expression(&expr_buf, interval,
                                                            expression))
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);

  buf->append(STRING_WITH_LEN("CREATE EVENT "));
  append_identifier(thd, buf, name.str, name.length);

  if (expression)
  {
    buf->append(STRING_WITH_LEN(" ON SCHEDULE EVERY "));
    buf->append(expr_buf);
    buf->append(' ');
    LEX_STRING *ival= &interval_type_to_name[interval];
    buf->append(ival->str, ival->length);
  }
  else
  {
    char dtime_buff[20*2+32];/* +32 to make my_snprintf_{8bit|ucs2} happy */
    buf->append(STRING_WITH_LEN(" ON SCHEDULE AT '"));
    /*
      Pass the buffer and the second param tells fills the buffer and
      returns the number of chars to copy.
    */
    buf->append(dtime_buff, my_datetime_to_str(&execute_at, dtime_buff));
    buf->append(STRING_WITH_LEN("'"));
  }

  if (on_completion == Event_timed::ON_COMPLETION_DROP)
    buf->append(STRING_WITH_LEN(" ON COMPLETION NOT PRESERVE "));
  else
    buf->append(STRING_WITH_LEN(" ON COMPLETION PRESERVE "));

  if (status == Event_timed::ENABLED)
    buf->append(STRING_WITH_LEN("ENABLE"));
  else
    buf->append(STRING_WITH_LEN("DISABLE"));

  if (comment.length)
  {
    buf->append(STRING_WITH_LEN(" COMMENT "));
    append_unescaped(buf, comment.str, comment.length);
  }
  buf->append(STRING_WITH_LEN(" DO "));
  buf->append(body.str, body.length);

  DBUG_RETURN(0);
}


/*
  Get SHOW CREATE EVENT as string

  SYNOPSIS
    Event_job_data::get_create_event(THD *thd, String *buf)
      thd    Thread
      buf    String*, should be already allocated. CREATE EVENT goes inside.

  RETURN VALUE
    0                       OK
    EVEX_MICROSECOND_UNSUP  Error (for now if mysql.event has been
                            tampered and MICROSECONDS interval or
                            derivative has been put there.
*/

int
Event_job_data::get_fake_create_event(THD *thd, String *buf)
{
  DBUG_ENTER("Event_job_data::get_create_event");
  buf->append(STRING_WITH_LEN("CREATE EVENT anonymous ON SCHEDULE "
                              "EVERY 3337 HOUR DO "));
  buf->append(body.str, body.length);

  DBUG_RETURN(0);
}


/*
  Compiles an event before it's execution. Compiles the anonymous
  sp_head object held by the event

  SYNOPSIS
    Event_job_data::compile()
      thd        thread context, used for memory allocation mostly
      mem_root   if != NULL then this memory root is used for allocs
                 instead of thd->mem_root

  RETURN VALUE
    0                       success
    EVEX_COMPILE_ERROR      error during compilation
    EVEX_MICROSECOND_UNSUP  mysql.event was tampered 
*/

int
Event_job_data::compile(THD *thd, MEM_ROOT *mem_root)
{
  int ret= 0;
  MEM_ROOT *tmp_mem_root= 0;
  LEX *old_lex= thd->lex, lex;
  char *old_db;
  int old_db_length;
  char *old_query;
  uint old_query_len;
  ulong old_sql_mode= thd->variables.sql_mode;
  char create_buf[15 * STRING_BUFFER_USUAL_SIZE];
  String show_create(create_buf, sizeof(create_buf), system_charset_info);
  CHARSET_INFO *old_character_set_client,
               *old_collation_connection,
               *old_character_set_results;
  Security_context save_ctx;

  DBUG_ENTER("Event_job_data::compile");

  show_create.length(0);

  switch (get_fake_create_event(thd, &show_create)) {
  case EVEX_MICROSECOND_UNSUP:
    sql_print_error("Scheduler");
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);
  case 0:
    break;
  default:
    DBUG_ASSERT(0);
  }

  old_character_set_client= thd->variables.character_set_client;
  old_character_set_results= thd->variables.character_set_results;
  old_collation_connection= thd->variables.collation_connection;

  thd->variables.character_set_client=
    thd->variables.character_set_results=
      thd->variables.collation_connection=
           get_charset_by_csname("utf8", MY_CS_PRIMARY, MYF(MY_WME));

  thd->update_charset();

  DBUG_PRINT("info",("old_sql_mode=%d new_sql_mode=%d",old_sql_mode, sql_mode));
  thd->variables.sql_mode= this->sql_mode;
  /* Change the memory root for the execution time */
  if (mem_root)
  {
    tmp_mem_root= thd->mem_root;
    thd->mem_root= mem_root;
  }
  old_query_len= thd->query_length;
  old_query= thd->query;
  old_db= thd->db;
  old_db_length= thd->db_length;
  thd->db= dbname.str;
  thd->db_length= dbname.length;

  thd->query= show_create.c_ptr_safe();
  thd->query_length= show_create.length();
  DBUG_PRINT("info", ("query:%s",thd->query));

  event_change_security_context(thd, definer_user, definer_host, dbname,
                                &save_ctx);
  thd->lex= &lex;
  mysql_init_query(thd, (uchar*) thd->query, thd->query_length);
  if (MYSQLparse((void *)thd) || thd->is_fatal_error)
  {
    DBUG_PRINT("error", ("error during compile or thd->is_fatal_error=%d",
                          thd->is_fatal_error));
    /*
      Free lex associated resources
      QQ: Do we really need all this stuff here?
    */
    sql_print_error("error during compile of %s.%s or thd->is_fatal_error=%d",
                    dbname.str, name.str, thd->is_fatal_error);

    lex.unit.cleanup();
    delete lex.sphead;
    sphead= lex.sphead= NULL;
    ret= EVEX_COMPILE_ERROR;
    goto done;
  }
  DBUG_PRINT("note", ("success compiling %s.%s", dbname.str, name.str));

  sphead= lex.sphead;

  sphead->set_definer(definer.str, definer.length);
  sphead->set_info(0, 0, &lex.sp_chistics, sql_mode);
  sphead->optimize();
  ret= 0;
done:

  lex_end(&lex);
  event_restore_security_context(thd, &save_ctx);
  DBUG_PRINT("note", ("return old data on its place. set back NAMES"));

  thd->lex= old_lex;
  thd->query= old_query;
  thd->query_length= old_query_len;
  thd->db= old_db;

  thd->variables.sql_mode= old_sql_mode;
  thd->variables.character_set_client= old_character_set_client;
  thd->variables.character_set_results= old_character_set_results;
  thd->variables.collation_connection= old_collation_connection;
  thd->update_charset();

  /* Change the memory root for the execution time. */
  if (mem_root)
    thd->mem_root= tmp_mem_root;

  DBUG_RETURN(ret);
}


/*
  Executes the event (the underlying sp_head object);

  SYNOPSIS
    Event_job_data::execute()
      thd       THD

  RETURN VALUE
    0        success
    -99      No rights on this.dbname.str
    others   retcodes of sp_head::execute_procedure()
*/

int
Event_job_data::execute(THD *thd)
{
  Security_context save_ctx;
  /* this one is local and not needed after exec */
  int ret= 0;

  DBUG_ENTER("Event_job_data::execute");
  DBUG_PRINT("info", ("EXECUTING %s.%s", dbname.str, name.str));

  if ((ret= compile(thd, NULL)))
    goto done;

  event_change_security_context(thd, definer_user, definer_host, dbname,
                                &save_ctx);
  /*
    THD::~THD will clean this or if there is DROP DATABASE in the SP then
    it will be free there. It should not point to our buffer which is allocated
    on a mem_root.
  */
  thd->db= my_strdup(dbname.str, MYF(0));
  thd->db_length= dbname.length;
  if (!check_access(thd, EVENT_ACL,dbname.str, 0, 0, 0,is_schema_db(dbname.str)))
  {
    List<Item> empty_item_list;
    empty_item_list.empty();
    if (thd->enable_slow_log)
      sphead->m_flags|= sp_head::LOG_SLOW_STATEMENTS;
    sphead->m_flags|= sp_head::LOG_GENERAL_LOG;

    ret= sphead->execute_procedure(thd, &empty_item_list);
  }
  else
  {
    DBUG_PRINT("error", ("%s@%s has no rights on %s", definer_user.str,
               definer_host.str, dbname.str));
    ret= -99;
  }

  event_restore_security_context(thd, &save_ctx);
done:
  thd->end_statement();
  thd->cleanup_after_query();

  DBUG_PRINT("info", ("EXECUTED %s.%s ret=%d", dbname.str, name.str, ret));

  DBUG_RETURN(ret);
}


/*
  Checks whether two events are in the same schema

  SYNOPSIS
    event_basic_db_equal()
      db  Schema
      et  Compare et->dbname to `db`

  RETURN VALUE
    TRUE   Equal
    FALSE  Not equal
*/

bool
event_basic_db_equal(LEX_STRING db, Event_basic *et)
{
  return !sortcmp_lex_string(et->dbname, db, system_charset_info);
}


/*
  Checks whether two events are equal by identifiers

  SYNOPSIS
    event_basic_identifier_equal()

  RETURN VALUE
    TRUE   Equal
    FALSE  Not equal
*/

bool
event_basic_identifier_equal(LEX_STRING db, LEX_STRING name, Event_basic *b)
{
  return !sortcmp_lex_string(name, b->name, system_charset_info) &&
         !sortcmp_lex_string(db, b->dbname, system_charset_info);
}
