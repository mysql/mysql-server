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

#define MYSQL_LEX 1
#include "mysql_priv.h"
#include "events.h"
#include "event_data_objects.h"
#include "event_db_repository.h"
#include "sp_head.h"


#define EVEX_MAX_INTERVAL_VALUE 1000000000L

static bool
event_change_security_context(THD *thd, LEX_STRING user, LEX_STRING host,
                              LEX_STRING db, Security_context *backup);

static void
event_restore_security_context(THD *thd, Security_context *backup);


/*
  Initiliazes dbname and name of an Event_queue_element_for_exec
  object

  SYNOPSIS
    Event_queue_element_for_exec::init()

  RETURN VALUE
    FALSE  OK
    TRUE   Error (OOM)
*/

bool
Event_queue_element_for_exec::init(LEX_STRING db, LEX_STRING n)
{
  if (!(dbname.str= my_strndup(db.str, dbname.length= db.length, MYF(MY_WME))))
    return TRUE;
  if (!(name.str= my_strndup(n.str, name.length= n.length, MYF(MY_WME))))
  {
    my_free((gptr) dbname.str, MYF(0));
    return TRUE;
  }
  return FALSE;
}


/*
  Destructor

  SYNOPSIS
    Event_queue_element_for_exec::~Event_queue_element_for_exec()
*/

Event_queue_element_for_exec::~Event_queue_element_for_exec()
{
  my_free((gptr) dbname.str, MYF(0));
  my_free((gptr) name.str, MYF(0));
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
  :on_completion(Event_basic::ON_COMPLETION_DROP),
  status(Event_basic::ENABLED),
  do_not_create(FALSE),
   item_starts(NULL), item_ends(NULL), item_execute_at(NULL),
   starts_null(TRUE), ends_null(TRUE), execute_at_null(TRUE),
   item_expression(NULL), expression(0)
{
  DBUG_ENTER("Event_parse_data::Event_parse_data");

  /* Actually in the parser STARTS is always set */
  starts= ends= execute_at= 0;

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
  DBUG_PRINT("info", ("body: '%s'  body_begin: 0x%lx end: 0x%lx", body_begin,
                      (long) body_begin, (long) thd->lex->ptr));

  body.length= thd->lex->ptr - body_begin;
  const char *body_end= body_begin + body.length - 1;

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
  body.str= thd->strmake(body_begin, body.length);

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

  if (on_completion == Event_basic::ON_COMPLETION_DROP)
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
  else if (status == Event_basic::ENABLED)
  {
    status= Event_basic::DISABLED;
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                 ER_EVENT_EXEC_TIME_IN_THE_PAST,
                 ER(ER_EVENT_EXEC_TIME_IN_THE_PAST));
  }
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
  if (interval_tmp.neg || expression > EVEX_MAX_INTERVAL_VALUE)
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
  int definer_user_len= thd->lex->definer->user.length;
  int definer_host_len= thd->lex->definer->host.length;

  DBUG_PRINT("info",("init definer_user thd->mem_root: 0x%lx  "
                     "definer_user: 0x%lx", (long) thd->mem_root,
                     (long) definer_user));

  /* + 1 for @ */
  DBUG_PRINT("info",("init definer as whole"));
  definer.length= definer_user_len + definer_host_len + 1;
  definer.str= thd->alloc(definer.length + 1);

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
    if ((status == Event_basic::ENABLED) ||
        (status == Event_basic::DISABLED))
      status = Event_basic::SLAVESIDE_DISABLED;
    originator = thd->server_id;
  }
  else
    originator = server_id;
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
  time_zone= NULL;
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
  while (field_name < ET_FIELD_COUNT)
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


bool
Event_basic::load_time_zone(THD *thd, const LEX_STRING tz_name)
{
  String str(tz_name.str, &my_charset_latin1);
  time_zone= my_tz_find(thd, &str);

  return (time_zone == NULL);
}


/*
  Constructor

  SYNOPSIS
    Event_queue_element::Event_queue_element()
*/

Event_queue_element::Event_queue_element():
  status_changed(FALSE), last_executed_changed(FALSE),
  on_completion(ON_COMPLETION_DROP), status(ENABLED),
  expression(0), dropped(FALSE), execution_count(0)
{
  DBUG_ENTER("Event_queue_element::Event_queue_element");

  starts= ends= execute_at= last_executed= 0;
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

Event_job_data::Event_job_data()
  :sphead(NULL), sql_mode(0)
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
    Event_job_data::load_from_row(THD *thd, TABLE *table)

  RETURN VALUE
    0                      OK
    EVEX_GET_FIELD_FAILED  Error

  NOTES
    This method is silent on errors and should behave like that. Callers
    should handle throwing of error messages. The reason is that the class
    should not know about how to deal with communication.
*/

int
Event_job_data::load_from_row(THD *thd, TABLE *table)
{
  char *ptr;
  uint len;
  DBUG_ENTER("Event_job_data::load_from_row");

  if (!table)
    goto error;

  if (table->s->fields < ET_FIELD_COUNT)
    goto error;

  LEX_STRING tz_name;
  if (load_string_fields(table->field,
                         ET_FIELD_DB, &dbname,
                         ET_FIELD_NAME, &name,
                         ET_FIELD_BODY, &body,
                         ET_FIELD_DEFINER, &definer,
                         ET_FIELD_TIME_ZONE, &tz_name,
                         ET_FIELD_COUNT))
    goto error;

  if (load_time_zone(thd, tz_name))
    goto error;

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
    Event_queue_element::load_from_row(THD *thd, TABLE *table)

  RETURN VALUE
    0                      OK
    EVEX_GET_FIELD_FAILED  Error

  NOTES
    This method is silent on errors and should behave like that. Callers
    should handle throwing of error messages. The reason is that the class
    should not know about how to deal with communication.
*/

int
Event_queue_element::load_from_row(THD *thd, TABLE *table)
{
  char *ptr;
  MYSQL_TIME time;
  LEX_STRING tz_name;

  DBUG_ENTER("Event_queue_element::load_from_row");

  if (!table)
    goto error;

  if (table->s->fields < ET_FIELD_COUNT)
    goto error;

  if (load_string_fields(table->field,
                         ET_FIELD_DB, &dbname,
                         ET_FIELD_NAME, &name,
                         ET_FIELD_DEFINER, &definer,
                         ET_FIELD_TIME_ZONE, &tz_name,
                         ET_FIELD_COUNT))
    goto error;

  if (load_time_zone(thd, tz_name))
    goto error;

  starts_null= table->field[ET_FIELD_STARTS]->is_null();
  if (!starts_null)
  {
    table->field[ET_FIELD_STARTS]->get_date(&time, TIME_NO_ZERO_DATE);
    starts= sec_since_epoch_TIME(&time);
  }

  ends_null= table->field[ET_FIELD_ENDS]->is_null();
  if (!ends_null)
  {
    table->field[ET_FIELD_ENDS]->get_date(&time, TIME_NO_ZERO_DATE);
    ends= sec_since_epoch_TIME(&time);
  }

  if (!table->field[ET_FIELD_INTERVAL_EXPR]->is_null())
    expression= table->field[ET_FIELD_INTERVAL_EXPR]->val_int();
  else
    expression= 0;
  /*
    If neigher STARTS and ENDS is set, then both fields are empty.
    Hence, if ET_FIELD_EXECUTE_AT is empty there is an error.
  */
  execute_at_null= table->field[ET_FIELD_EXECUTE_AT]->is_null();
  DBUG_ASSERT(!(starts_null && ends_null && !expression && execute_at_null));
  if (!expression && !execute_at_null)
  {
    if (table->field[ET_FIELD_EXECUTE_AT]->get_date(&time,
                                                    TIME_NO_ZERO_DATE))
      goto error;
    execute_at= sec_since_epoch_TIME(&time);
  }

  /*
    We load the interval type from disk as string and then map it to
    an integer. This decouples the values of enum interval_type
    and values actually stored on disk. Therefore the type can be
    reordered without risking incompatibilities of data between versions.
  */
  if (!table->field[ET_FIELD_TRANSIENT_INTERVAL]->is_null())
  {
    int i;
    char buff[MAX_FIELD_WIDTH];
    String str(buff, sizeof(buff), &my_charset_bin);
    LEX_STRING tmp;

    table->field[ET_FIELD_TRANSIENT_INTERVAL]->val_str(&str);
    if (!(tmp.length= str.length()))
      goto error;

    tmp.str= str.c_ptr_safe();

    i= find_string_in_array(interval_type_to_name, &tmp, system_charset_info);
    if (i < 0)
      goto error;
    interval= (interval_type) i;
  }

  if (!table->field[ET_FIELD_LAST_EXECUTED]->is_null())
  {
    table->field[ET_FIELD_LAST_EXECUTED]->get_date(&time,
                                                   TIME_NO_ZERO_DATE);
    last_executed= sec_since_epoch_TIME(&time);
  }
  last_executed_changed= FALSE;

  if ((ptr= get_field(&mem_root, table->field[ET_FIELD_STATUS])) == NullS)
    goto error;

  DBUG_PRINT("load_from_row", ("Event [%s] is [%s]", name.str, ptr));

  /* Set event status (ENABLED | SLAVESIDE_DISABLED | DISABLED) */
  switch (ptr[0])
  {
  case 'E' :
    status = Event_queue_element::ENABLED;
    break;
  case 'S' :
    status = Event_queue_element::SLAVESIDE_DISABLED;
    break;
  case 'D' :
  default:
    status = Event_queue_element::DISABLED;
    break;
  }
  if ((ptr= get_field(&mem_root, table->field[ET_FIELD_ORIGINATOR])) == NullS)
    goto error;
  originator = table->field[ET_FIELD_ORIGINATOR]->val_int(); 

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
    Event_timed::load_from_row(THD *thd, TABLE *table)

  RETURN VALUE
    0                      OK
    EVEX_GET_FIELD_FAILED  Error

  NOTES
    This method is silent on errors and should behave like that. Callers
    should handle throwing of error messages. The reason is that the class
    should not know about how to deal with communication.
*/

int
Event_timed::load_from_row(THD *thd, TABLE *table)
{
  char *ptr;
  uint len;

  DBUG_ENTER("Event_timed::load_from_row");

  if (Event_queue_element::load_from_row(thd, table))
    goto error;

  if (load_string_fields(table->field,
                         ET_FIELD_BODY, &body,
                         ET_FIELD_COUNT))
    goto error;


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
  add_interval() adds a specified interval to time 'ltime' in time
  zone 'time_zone', and returns the result converted to the number of
  seconds since epoch (aka Unix time; in UTC time zone).  Zero result
  means an error.
*/
static
my_time_t
add_interval(MYSQL_TIME *ltime, const Time_zone *time_zone,
             interval_type scale, INTERVAL interval)
{
  if (date_add_interval(ltime, scale, interval))
    return 0;

  my_bool not_used;
  return time_zone->TIME_to_gmt_sec(ltime, &not_used);
}


/*
  Computes the sum of a timestamp plus interval.

  SYNOPSIS
    get_next_time()
      time_zone     event time zone
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
*/

static
bool get_next_time(const Time_zone *time_zone, my_time_t *next,
                   my_time_t start, my_time_t time_now,
                   int i_value, interval_type i_type)
{
  DBUG_ENTER("get_next_time");
  DBUG_PRINT("enter", ("start: %lu  now: %lu", (long) start, (long) time_now));

  DBUG_ASSERT(start <= time_now);

  longlong months=0, seconds=0;

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
  DBUG_PRINT("info", ("seconds: %ld  months: %ld", (long) seconds, (long) months));

  MYSQL_TIME local_start;
  MYSQL_TIME local_now;

  /* Convert times from UTC to local. */
  {
    time_zone->gmt_sec_to_TIME(&local_start, start);
    time_zone->gmt_sec_to_TIME(&local_now, time_now);
  }

  INTERVAL interval;
  bzero(&interval, sizeof(interval));
  my_time_t next_time= 0;

  if (seconds)
  {
    longlong seconds_diff;
    long microsec_diff;
    bool negative= calc_time_diff(&local_now, &local_start, 1,
                                  &seconds_diff, &microsec_diff);
    if (!negative)
    {
      /*
        The formula below returns the interval that, when added to
        local_start, will always give the time in the future.
      */
      interval.second= seconds_diff - seconds_diff % seconds + seconds;
      next_time= add_interval(&local_start, time_zone,
                              INTERVAL_SECOND, interval);
      if (next_time == 0)
        goto done;
    }

    if (next_time <= time_now)
    {
      /*
        If 'negative' is true above, then 'next_time == 0', and
        'next_time <= time_now' is also true.  If negative is false,
        then next_time was set, but perhaps to the value that is less
        then time_now.  See below for elaboration.
      */
      DBUG_ASSERT(negative || next_time > 0);

      /*
        If local_now < local_start, i.e. STARTS time is in the future
        according to the local time (it always in the past according
        to UTC---this is a prerequisite of this function), then
        STARTS is almost always in the past according to the local
        time too.  However, in the time zone that has backward
        Daylight Saving Time shift, the following may happen: suppose
        we have a backward DST shift at certain date after 2:59:59,
        i.e. local time goes 1:59:59, 2:00:00, ... , 2:59:59, (shift
        here) 2:00:00 (again), ... , 2:59:59 (again), 3:00:00, ... .
        Now suppose the time has passed the first 2:59:59, has been
        shifted backward, and now is (the second) 2:20:00.  The user
        does CREATE EVENT with STARTS 'current-date 2:40:00'.  Local
        time 2:40:00 from create statement is treated by time
        functions as the first such time, so according to UTC it comes
        before the second 2:20:00.  But according to local time it is
        obviously in the future, so we end up in this branch.

        Since we are in the second pass through 2:00:00--2:59:59, and
        any local time form this interval is treated by system
        functions as the time from the first pass, we have to find the
        time for the next execution that is past the DST-affected
        interval (past the second 2:59:59 for our example,
        i.e. starting from 3:00:00).  We do this in the loop until the
        local time is mapped onto future UTC time.  'start' time is in
        the past, so we may use 'do { } while' here, and add the first
        interval right away.

        Alternatively, it could be that local_now >= local_start.  Now
        for the example above imagine we do CREATE EVENT with STARTS
        'current-date 2:10:00'.  Local start 2:10 is in the past (now
        is local 2:20), so we add an interval, and get next execution
        time, say, 2:40.  It is in the future according to local time,
        but, again, since we are in the second pass through
        2:00:00--2:59:59, 2:40 will be converted into UTC time in the
        past.  So we will end up in this branch again, and may add
        intervals in a 'do { } while' loop.

        Note that for any given event we may end up here only if event
        next execution time will map to the time interval that is
        passed twice, and only if the server was started during the
        second pass, or the event is being created during the second
        pass.  After that, we never will get here (unless we again
        start the server during the second pass).  In other words,
        such a condition is extremely rare.
      */
      interval.second= seconds;
      do
      {
        next_time= add_interval(&local_start, time_zone,
                                INTERVAL_SECOND, interval);
        if (next_time == 0)
          goto done;
      }
      while (next_time <= time_now);
    }
  }
  else
  {
    long diff_months= (long) (local_now.year - local_start.year)*12 +
                      (local_now.month - local_start.month);
    /*
      Unlike for seconds above, the formula below returns the interval
      that, when added to the local_start, will give the time in the
      past, or somewhere in the current month.  We are interested in
      the latter case, to see if this time has already passed, or is
      yet to come this month.

      Note that the time is guaranteed to be in the past unless
      (diff_months % months == 0), but no good optimization is
      possible here, because (diff_months % months == 0) is what will
      happen most of the time, as get_next_time() will be called right
      after the execution of the event.  We could pass last_executed
      time to this function, and see if the execution has already
      happened this month, but for that we will have to convert
      last_executed from seconds since epoch to local broken-down
      time, and this will greatly reduce the effect of the
      optimization.  So instead we keep the code simple and clean.
    */
    interval.month= (ulong) (diff_months - diff_months % months);
    next_time= add_interval(&local_start, time_zone,
                            INTERVAL_MONTH, interval);
    if (next_time == 0)
      goto done;

    if (next_time <= time_now)
    {
      interval.month= (ulong) months;
      next_time= add_interval(&local_start, time_zone,
                              INTERVAL_MONTH, interval);
      if (next_time == 0)
        goto done;
    }
  }

  DBUG_ASSERT(time_now < next_time);

  *next= next_time;

done:
  DBUG_PRINT("info", ("next_time: %ld", (long) next_time));
  DBUG_RETURN(next_time == 0);
}


/*
  Computes next execution time.

  SYNOPSIS
    Event_queue_element::compute_next_execution_time()

  RETURN VALUE
    FALSE  OK
    TRUE   Error

  NOTES
    The time is set in execute_at, if no more executions the latter is
    set to 0.
*/

bool
Event_queue_element::compute_next_execution_time()
{
  my_time_t time_now;
  DBUG_ENTER("Event_queue_element::compute_next_execution_time");
  DBUG_PRINT("enter", ("starts: %lu  ends: %lu  last_executed: %lu  this: 0x%lx",
                       (long) starts, (long) ends, (long) last_executed,
                       (long) this));

  if (status != Event_queue_element::ENABLED)
  {
    DBUG_PRINT("compute_next_execution_time",
               ("Event %s is DISABLED", name.str));
    goto ret;
  }
  /* If one-time, no need to do computation */
  if (!expression)
  {
    /* Let's check whether it was executed */
    if (last_executed)
    {
      DBUG_PRINT("info",("One-time event %s.%s of was already executed",
                         dbname.str, name.str));
      dropped= (on_completion == Event_queue_element::ON_COMPLETION_DROP);
      DBUG_PRINT("info",("One-time event will be dropped: %d.", dropped));

      status= Event_queue_element::DISABLED;
      status_changed= TRUE;
    }
    goto ret;
  }

  time_now= (my_time_t) current_thd->query_start();

  DBUG_PRINT("info",("NOW: [%lu]", (ulong) time_now));

  /* if time_now is after ends don't execute anymore */
  if (!ends_null && ends < time_now)
  {
    DBUG_PRINT("info", ("NOW after ENDS, don't execute anymore"));
    /* time_now is after ends. don't execute anymore */
    execute_at= 0;
    execute_at_null= TRUE;
    if (on_completion == Event_queue_element::ON_COMPLETION_DROP)
      dropped= TRUE;
    DBUG_PRINT("info", ("Dropped: %d", dropped));
    status= Event_queue_element::DISABLED;
    status_changed= TRUE;

    goto ret;
  }

  /*
    Here time_now is before or equals ends if the latter is set.
    Let's check whether time_now is before starts.
    If so schedule for starts.
  */
  if (!starts_null && time_now <= starts)
  {
    if (time_now == starts && starts == last_executed)
    {
      /*
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
      If last_executed is set then increase with m_expression. The new MYSQL_TIME is
      after m_ends set execute_at to 0. And check for on_completion
      If not set then schedule for now.
    */
    DBUG_PRINT("info", ("Both STARTS & ENDS are set"));
    if (!last_executed)
    {
      DBUG_PRINT("info", ("Not executed so far."));
    }

    {
      my_time_t next_exec;

      if (get_next_time(time_zone, &next_exec, starts, time_now,
                        (int) expression, interval))
        goto err;

      /* There was previous execution */
      if (ends < next_exec)
      {
        DBUG_PRINT("info", ("Next execution of %s after ENDS. Stop executing.",
                   name.str));
        /* Next execution after ends. No more executions */
        execute_at= 0;
        execute_at_null= TRUE;
        if (on_completion == Event_queue_element::ON_COMPLETION_DROP)
          dropped= TRUE;
        status= Event_queue_element::DISABLED;
        status_changed= TRUE;
      }
      else
      {
        DBUG_PRINT("info",("Next[%lu]", (ulong) next_exec));
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
    if (last_executed)
    {
      my_time_t next_exec;
      if (get_next_time(time_zone, &next_exec, starts, time_now,
                        (int) expression, interval))
        goto err;
      execute_at= next_exec;
      DBUG_PRINT("info",("Next[%lu]", (ulong) next_exec));
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
      if (!last_executed)
      {
        DBUG_PRINT("info", ("Not executed so far."));
      }

      {
        my_time_t next_exec;
        if (get_next_time(time_zone, &next_exec, starts, time_now,
                          (int) expression, interval))
          goto err;
        execute_at= next_exec;
        DBUG_PRINT("info",("Next[%lu]", (ulong) next_exec));
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

      if (!last_executed)
        execute_at= time_now;
      else
      {
        my_time_t next_exec;

        if (get_next_time(time_zone, &next_exec, starts, time_now,
                          (int) expression, interval))
          goto err;

        if (ends < next_exec)
        {
          DBUG_PRINT("info", ("Next execution after ENDS. Stop executing."));
          execute_at= 0;
          execute_at_null= TRUE;
          status= Event_queue_element::DISABLED;
          status_changed= TRUE;
          if (on_completion == Event_queue_element::ON_COMPLETION_DROP)
            dropped= TRUE;
        }
        else
        {
          DBUG_PRINT("info", ("Next[%lu]", (ulong) next_exec));
          execute_at= next_exec;
          execute_at_null= FALSE;
        }
      }
    }
    goto ret;
  }
ret:
  DBUG_PRINT("info", ("ret: 0 execute_at: %lu", (long) execute_at));
  DBUG_RETURN(FALSE);
err:
  DBUG_PRINT("info", ("ret=1"));
  DBUG_RETURN(TRUE);
}


/*
  Set the internal last_executed MYSQL_TIME struct to now. NOW is the
  time according to thd->query_start(), so the THD's clock.

  SYNOPSIS
    Event_queue_element::mark_last_executed()
      thd   thread context
*/

void
Event_queue_element::mark_last_executed(THD *thd)
{
  thd->end_time();

  last_executed= (my_time_t) thd->query_start();
  last_executed_changed= TRUE;

  execution_count++;
}


/*
  Saves status and last_executed_at to the disk if changed.

  SYNOPSIS
    Event_queue_element::update_timing_fields()
      thd - thread context

  RETURN VALUE
    FALSE   OK
    TRUE    Error while opening mysql.event for writing or during
            write on disk
*/

bool
Event_queue_element::update_timing_fields(THD *thd)
{
  Event_db_repository *db_repository= Events::get_db_repository();
  int ret;

  DBUG_ENTER("Event_queue_element::update_timing_fields");

  DBUG_PRINT("enter", ("name: %*s", name.length, name.str));

  /* No need to update if nothing has changed */
  if (!(status_changed || last_executed_changed))
    DBUG_RETURN(0);

  ret= db_repository->update_timing_fields_for_event(thd,
                                                     dbname, name,
                                                     last_executed_changed,
                                                     last_executed,
                                                     status_changed,
                                                     (ulonglong) status);
  last_executed_changed= status_changed= FALSE;
  DBUG_RETURN(ret);
}


static
void
append_datetime(String *buf, Time_zone *time_zone, my_time_t secs,
                const char *name, uint len)
{
  char dtime_buff[20*2+32];/* +32 to make my_snprintf_{8bit|ucs2} happy */
  buf->append(STRING_WITH_LEN(" "));
  buf->append(name, len);
  buf->append(STRING_WITH_LEN(" '"));
  /*
    Pass the buffer and the second param tells fills the buffer and
    returns the number of chars to copy.
  */
  MYSQL_TIME time;
  time_zone->gmt_sec_to_TIME(&time, secs);
  buf->append(dtime_buff, my_datetime_to_str(&time, dtime_buff));
  buf->append(STRING_WITH_LEN("'"));
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

    if (!starts_null)
      append_datetime(buf, time_zone, starts, STRING_WITH_LEN("STARTS"));

    if (!ends_null)
      append_datetime(buf, time_zone, ends, STRING_WITH_LEN("ENDS"));
  }
  else
  {
    append_datetime(buf, time_zone, execute_at,
                    STRING_WITH_LEN("ON SCHEDULE AT"));
  }

  if (on_completion == Event_timed::ON_COMPLETION_DROP)
    buf->append(STRING_WITH_LEN(" ON COMPLETION NOT PRESERVE "));
  else
    buf->append(STRING_WITH_LEN(" ON COMPLETION PRESERVE "));

  if (status == Event_timed::ENABLED)
    buf->append(STRING_WITH_LEN("ENABLE"));
  else if (status == Event_timed::SLAVESIDE_DISABLED)
    buf->append(STRING_WITH_LEN("DISABLE ON SLAVE"));
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
Event_job_data::get_fake_create_event(String *buf)
{
  DBUG_ENTER("Event_job_data::get_create_event");
  /* FIXME: "EVERY 3337 HOUR" is asking for trouble. */
  buf->append(STRING_WITH_LEN("CREATE EVENT anonymous ON SCHEDULE "
                              "EVERY 3337 HOUR DO "));
  buf->append(body.str, body.length);

  DBUG_RETURN(0);
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
Event_job_data::execute(THD *thd, bool drop)
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
    THD::~THD will clean this or if there is DROP DATABASE in the
    SP then it will be free there. It should not point to our buffer
    which is allocated on a mem_root.
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

    /* Execute the event in its time zone. */
    thd->variables.time_zone= time_zone;

    ret= sphead->execute_procedure(thd, &empty_item_list);
  }
  else
  {
    DBUG_PRINT("error", ("%s@%s has no rights on %s", definer_user.str,
               definer_host.str, dbname.str));
    ret= -99;
  }
  if (drop)
  {
    sql_print_information("Event Scheduler: Dropping %s.%s",
                          dbname.str, name.str);
    /*
      We must do it here since here we're under the right authentication
      ID of the event definer
    */
    if (Events::drop_event(thd, dbname, name, FALSE))
      ret= 1;
  }

  event_restore_security_context(thd, &save_ctx);
done:
  thd->end_statement();
  thd->cleanup_after_query();

  DBUG_PRINT("info", ("EXECUTED %s.%s  ret: %d", dbname.str, name.str, ret));

  DBUG_RETURN(ret);
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

  switch (get_fake_create_event(&show_create)) {
  case EVEX_MICROSECOND_UNSUP:
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

  DBUG_PRINT("info",("old_sql_mode: %lu  new_sql_mode: %lu",old_sql_mode, sql_mode));
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
  DBUG_PRINT("info", ("query: %s",thd->query));

  event_change_security_context(thd, definer_user, definer_host, dbname,
                                &save_ctx);
  thd->lex= &lex;
  mysql_init_query(thd, thd->query, thd->query_length);
  if (MYSQLparse((void *)thd) || thd->is_fatal_error)
  {
    DBUG_PRINT("error", ("error during compile or thd->is_fatal_error: %d",
                          thd->is_fatal_error));
    lex.unit.cleanup();

    sql_print_error("Event Scheduler: "
                    "%serror during compilation of %s.%s",
                    thd->is_fatal_error ? "fatal " : "",
                    dbname.str, name.str);

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
  Checks whether an event has equal `db` and `name`

  SYNOPSIS
    event_basic_identifier_equal()
      db   Schema
      name Name
      et   The event object

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


/*
  Switches the security context.

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
  Restores the security context.

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
