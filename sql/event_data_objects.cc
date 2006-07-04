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


#define EVEX_MAX_INTERVAL_VALUE 2147483647L


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
  item_execute_at= item_expression= item_starts= item_ends= NULL;
  status= ENABLED;
  on_completion= ON_COMPLETION_DROP;
  expression= 0;

  /* Actually in the parser STARTS is always set */
  set_zero_time(&starts, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&ends, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
  starts_null= ends_null= execute_at_null= TRUE;

  body.str= comment.str= 0;
  body.length= comment.length= 0;
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
  DBUG_ASSERT(spn);
  dbname.length= spn->m_db.length;
  if (spn->m_db.length == 0)
    dbname.str= NULL;
  else
    dbname.str= thd->strmake(spn->m_db.str, spn->m_db.length);
  name.length= spn->m_name.length;
  name.str= thd->strmake(spn->m_name.str, spn->m_name.length);

  DBUG_PRINT("dbname", ("len=%d db=%s",dbname.length, dbname.str));
  DBUG_PRINT("name", ("len=%d name=%s",name.length, name.str));

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
  DBUG_PRINT("info", ("body=[%s] body_begin=0x%ld end=0x%ld", body_begin,
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
  
  RETURN VALUE
    0  OK
*/

int
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

  DBUG_RETURN(0);
}


/*
  Set time for execution for one time events.

  SYNOPSIS
    Event_parse_data::init_execute_at()
      expr   when (datetime)

  RETURN VALUE
    0                  OK
    EVEX_PARSE_ERROR   fix_fields failed
    EVEX_BAD_PARAMS    datetime is in the past
    ER_WRONG_VALUE     wrong value for execute at
*/

int
Event_parse_data::init_execute_at(THD *thd, Item *expr)
{
  my_bool not_used;
  TIME ltime;
  my_time_t t;

  TIME time_tmp;
  DBUG_ENTER("Event_parse_data::init_execute_at");

  if (expr->fix_fields(thd, &expr))
    DBUG_RETURN(EVEX_PARSE_ERROR);
  
  /* no starts and/or ends in case of execute_at */
  DBUG_PRINT("info", ("starts_null && ends_null should be 1 is %d",
                      (starts_null && ends_null)));
  DBUG_ASSERT(starts_null && ends_null);

  /* let's check whether time is in the past */
  thd->variables.time_zone->gmt_sec_to_TIME(&time_tmp, 
                                            (my_time_t) thd->query_start());

  if ((not_used= expr->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(ER_WRONG_VALUE);

  if (TIME_to_ulonglong_datetime(&ltime) <
      TIME_to_ulonglong_datetime(&time_tmp))
    DBUG_RETURN(EVEX_BAD_PARAMS);

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
    DBUG_RETURN(ER_WRONG_VALUE);
  }

  execute_at_null= FALSE;
  execute_at= ltime;
  DBUG_RETURN(0);
}


/*
  Set time for execution for transient events.

  SYNOPSIS
    Event_parse_data::init_interval()
      expr      how much?
      new_interval  what is the interval

  RETURN VALUE
    0                  OK
    EVEX_PARSE_ERROR   fix_fields failed
    EVEX_BAD_PARAMS    Interval is not positive
    EVEX_MICROSECOND_UNSUP  Microseconds are not supported.
*/

int
Event_parse_data::init_interval(THD *thd, Item *expr, interval_type new_interval)
{
  String value;
  INTERVAL interval_tmp;

  DBUG_ENTER("Event_parse_data::init_interval");

  if (expr->fix_fields(thd, &expr))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  value.alloc(MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN);
  if (get_interval_value(expr, new_interval, &value, &interval_tmp))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  expression= 0;

  switch (new_interval) {
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
  case INTERVAL_MINUTE_MICROSECOND: /* day and hour are 0 */
  case INTERVAL_HOUR_MICROSECOND:   /* day is anyway 0    */
  case INTERVAL_DAY_MICROSECOND:
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);
    expression= ((((interval_tmp.day*24) + interval_tmp.hour)*60+
                  interval_tmp.minute)*60 +
                 interval_tmp.second) * 1000000L + interval_tmp.second_part;
    break;
  case INTERVAL_HOUR_MINUTE:
    expression= interval_tmp.hour * 60 + interval_tmp.minute;
    break;
  case INTERVAL_MINUTE_SECOND:
    expression= interval_tmp.minute * 60 + interval_tmp.second;
    break;
  case INTERVAL_SECOND_MICROSECOND:
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);
    expression= interval_tmp.second * 1000000L + interval_tmp.second_part;
    break;
  case INTERVAL_MICROSECOND:
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);  
  case INTERVAL_LAST:
    DBUG_ASSERT(0);
  }
  if (interval_tmp.neg || expression > EVEX_MAX_INTERVAL_VALUE)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  interval= new_interval;
  DBUG_RETURN(0);
}


/*
  Set activation time.

  SYNOPSIS
    Event_parse_data::init_starts()
      expr      how much?
      interval  what is the interval

  NOTES
    Note that activation time is not execution time.
    EVERY 5 MINUTE STARTS "2004-12-12 10:00:00" means that
    the event will be executed every 5 minutes but this will
    start at the date shown above. Expressions are possible :
    DATE_ADD(NOW(), INTERVAL 1 DAY)  -- start tommorow at
    same time.

  RETURN VALUE
    0                  OK
    EVEX_PARSE_ERROR   fix_fields failed
    EVEX_BAD_PARAMS    starts before now
*/

int
Event_parse_data::init_starts(THD *thd, Item *new_starts)
{
  my_bool not_used;
  TIME ltime, time_tmp;
  my_time_t t;

  DBUG_ENTER("Event_parse_data::init_starts");

  if (new_starts->fix_fields(thd, &new_starts))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  if ((not_used= new_starts->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /* Let's check whether time is in the past */
  thd->variables.time_zone->gmt_sec_to_TIME(&time_tmp,
                                            (my_time_t) thd->query_start());

  DBUG_PRINT("info",("now   =%lld", TIME_to_ulonglong_datetime(&time_tmp)));
  DBUG_PRINT("info",("starts=%lld", TIME_to_ulonglong_datetime(&ltime)));
  if (TIME_to_ulonglong_datetime(&ltime) <
      TIME_to_ulonglong_datetime(&time_tmp))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx.
    CONVERT_TZ has similar problem.
    mysql_priv.h currently lists 
      #define TIMESTAMP_MAX_YEAR 2038 (see TIME_to_timestamp())
  */
  my_tz_UTC->gmt_sec_to_TIME(&ltime,t=TIME_to_timestamp(thd, &ltime, &not_used));
  if (!t)
  {
    DBUG_PRINT("error", ("STARTS after year 2037"));
    DBUG_RETURN(EVEX_BAD_PARAMS);
  }

  starts= ltime;
  starts_null= FALSE;
  DBUG_RETURN(0);
}


/*
  Set deactivation time.

  SYNOPSIS
    Event_parse_data::init_ends()
      thd       THD
      new_ends  when?

  NOTES
    Note that activation time is not execution time.
    EVERY 5 MINUTE ENDS "2004-12-12 10:00:00" means that
    the event will be executed every 5 minutes but this will
    end at the date shown above. Expressions are possible :
    DATE_ADD(NOW(), INTERVAL 1 DAY)  -- end tommorow at
    same time.

  RETURN VALUE
    0                  OK
    EVEX_PARSE_ERROR   fix_fields failed
    ER_WRONG_VALUE     starts distant date (after year 2037)
    EVEX_BAD_PARAMS    ENDS before STARTS
*/

int
Event_parse_data::init_ends(THD *thd, Item *new_ends)
{
  TIME ltime, ltime_now;
  my_bool not_used;
  my_time_t t;

  DBUG_ENTER("Event_parse_data::init_ends");

  if (new_ends->fix_fields(thd, &new_ends))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  DBUG_PRINT("info", ("convert to TIME"));
  if ((not_used= new_ends->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx.
    CONVERT_TZ has similar problem.
    mysql_priv.h currently lists 
      #define TIMESTAMP_MAX_YEAR 2038 (see TIME_to_timestamp())
  */
  DBUG_PRINT("info", ("get the UTC time"));
  my_tz_UTC->gmt_sec_to_TIME(&ltime,t=TIME_to_timestamp(thd, &ltime, &not_used));
  if (!t)
  {
    DBUG_PRINT("error", ("ENDS after year 2037"));
    DBUG_RETURN(EVEX_BAD_PARAMS);
  }

  /* Check whether ends is after starts */
  DBUG_PRINT("info", ("ENDS after STARTS?"));
  if (!starts_null && my_time_compare(&starts, &ltime) != -1)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
    The parser forces starts to be provided but one day STARTS could be
    set before NOW() and in this case the following check should be done.
    Check whether ENDS is not in the past.
  */
  DBUG_PRINT("info", ("ENDS after NOW?"));
  my_tz_UTC->gmt_sec_to_TIME(&ltime_now, thd->query_start());
  if (my_time_compare(&ltime_now, &ltime) == 1)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  ends= ltime;
  ends_null= FALSE;
  DBUG_RETURN(0);
}


/*
  Constructor

  SYNOPSIS
    Event_timed::Event_timed()
*/

Event_timed::Event_timed():in_spawned_thread(0),locked_by_thread_id(0),
                           running(0), thread_id(0), status_changed(false),
                           last_executed_changed(false), expression(0),
                           created(0), modified(0),
                           on_completion(Event_timed::ON_COMPLETION_DROP),
                           status(Event_timed::ENABLED), sphead(0),
                           sql_mode(0), dropped(false),
                           free_sphead_on_delete(true), flags(0)
                
{
  pthread_mutex_init(&this->LOCK_running, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&this->COND_finished, NULL);
  init();
}


/*
  Destructor

  SYNOPSIS
    Event_timed::~Event_timed()
*/

Event_timed::~Event_timed()
{    
  deinit_mutexes();
  free_root(&mem_root, MYF(0));

  if (free_sphead_on_delete)
    free_sp();
}


/*
  Destructor

  SYNOPSIS
    Event_timed::deinit_mutexes()
*/

void
Event_timed::deinit_mutexes()
{
  pthread_mutex_destroy(&this->LOCK_running);
  pthread_cond_destroy(&this->COND_finished);
}


/*
  Checks whether the event is running

  SYNOPSIS
    Event_timed::is_running()
*/

bool
Event_timed::is_running()
{
  bool ret;

  VOID(pthread_mutex_lock(&this->LOCK_running));
  ret= running;
  VOID(pthread_mutex_unlock(&this->LOCK_running));

  return ret;
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

  dbname.str= name.str= body.str= comment.str= 0;
  dbname.length= name.length= body.length= comment.length= 0;

  set_zero_time(&starts, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&ends, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&execute_at, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&last_executed, MYSQL_TIMESTAMP_DATETIME);
  starts_null= ends_null= execute_at_null= TRUE;

  definer_user.str= definer_host.str= 0;
  definer_user.length= definer_host.length= 0;

  sql_mode= 0;
  /* init memory root */
  init_alloc_root(&mem_root, 256, 512);

  DBUG_VOID_RETURN;
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
  Event_timed *et;
  uint len;
  bool res1, res2;

  DBUG_ENTER("Event_timed::load_from_row");

  if (!table)
    goto error;

  et= this;

  if (table->s->fields != ET_FIELD_COUNT)
    goto error;

  if ((et->dbname.str= get_field(&mem_root, table->field[ET_FIELD_DB])) == NULL)
    goto error;

  et->dbname.length= strlen(et->dbname.str);

  if ((et->name.str= get_field(&mem_root, table->field[ET_FIELD_NAME])) == NULL)
    goto error;

  et->name.length= strlen(et->name.str);

  if ((et->body.str= get_field(&mem_root, table->field[ET_FIELD_BODY])) == NULL)
    goto error;

  et->body.length= strlen(et->body.str);

  if ((et->definer.str= get_field(&mem_root,
                                  table->field[ET_FIELD_DEFINER])) == NullS)
    goto error;
  et->definer.length= strlen(et->definer.str);

  ptr= strchr(et->definer.str, '@');

  if (! ptr)
    ptr= et->definer.str;

  len= ptr - et->definer.str;

  et->definer_user.str= strmake_root(&mem_root, et->definer.str, len);
  et->definer_user.length= len;
  len= et->definer.length - len - 1;            //1 is because of @
  et->definer_host.str= strmake_root(&mem_root, ptr + 1, len);/* 1:because of @*/
  et->definer_host.length= len;
  
  et->starts_null= table->field[ET_FIELD_STARTS]->is_null();
  res1= table->field[ET_FIELD_STARTS]->
                                    get_date(&et->starts,TIME_NO_ZERO_DATE);

  et->ends_null= table->field[ET_FIELD_ENDS]->is_null();
  res2= table->field[ET_FIELD_ENDS]->get_date(&et->ends, TIME_NO_ZERO_DATE);
  
  if (!table->field[ET_FIELD_INTERVAL_EXPR]->is_null())
    et->expression= table->field[ET_FIELD_INTERVAL_EXPR]->val_int();
  else
    et->expression= 0;
  /*
    If res1 and res2 are true then both fields are empty.
    Hence if ET_FIELD_EXECUTE_AT is empty there is an error.
  */
  et->execute_at_null=
            table->field[ET_FIELD_EXECUTE_AT]->is_null();
  DBUG_ASSERT(!(et->starts_null && et->ends_null && !et->expression &&
              et->execute_at_null));
  if (!et->expression &&
      table->field[ET_FIELD_EXECUTE_AT]-> get_date(&et->execute_at,
                                                        TIME_NO_ZERO_DATE))
    goto error;

  /*
    In DB the values start from 1 but enum interval_type starts
    from 0
  */
  if (!table->field[ET_FIELD_TRANSIENT_INTERVAL]->is_null())
    et->interval= (interval_type) ((ulonglong)
          table->field[ET_FIELD_TRANSIENT_INTERVAL]->val_int() - 1);
  else
    et->interval= (interval_type) 0;

  et->created= table->field[ET_FIELD_CREATED]->val_int();
  et->modified= table->field[ET_FIELD_MODIFIED]->val_int();

  table->field[ET_FIELD_LAST_EXECUTED]->
                     get_date(&et->last_executed, TIME_NO_ZERO_DATE);

  last_executed_changed= false;

  /* ToDo : Andrey . Find a way not to allocate ptr on event_mem_root */
  if ((ptr= get_field(&mem_root, table->field[ET_FIELD_STATUS])) == NullS)
    goto error;

  DBUG_PRINT("load_from_row", ("Event [%s] is [%s]", et->name.str, ptr));
  et->status= (ptr[0]=='E'? Event_timed::ENABLED:Event_timed::DISABLED);

  /* ToDo : Andrey . Find a way not to allocate ptr on event_mem_root */
  if ((ptr= get_field(&mem_root,
                  table->field[ET_FIELD_ON_COMPLETION])) == NullS)
    goto error;

  et->on_completion= (ptr[0]=='D'? Event_timed::ON_COMPLETION_DROP:
                                   Event_timed::ON_COMPLETION_PRESERVE);

  et->comment.str= get_field(&mem_root, table->field[ET_FIELD_COMMENT]);
  if (et->comment.str != NullS)
    et->comment.length= strlen(et->comment.str);
  else
    et->comment.length= 0;

  et->sql_mode= (ulong) table->field[ET_FIELD_SQL_MODE]->val_int();

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
    Event_timed::compute_next_execution_time()

  RETURN VALUE
    FALSE  OK
    TRUE   Error

  NOTES
    The time is set in execute_at, if no more executions the latter is set to
    0000-00-00.
*/

bool
Event_timed::compute_next_execution_time()
{
  TIME time_now;
  int tmp;

  DBUG_ENTER("Event_timed::compute_next_execution_time");
  DBUG_PRINT("enter", ("starts=%llu ends=%llu last_executed=%llu this=%p",
                        TIME_to_ulonglong_datetime(&starts),
                        TIME_to_ulonglong_datetime(&ends),
                        TIME_to_ulonglong_datetime(&last_executed), this));

  if (status == Event_timed::DISABLED)
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
      dropped= (on_completion == Event_timed::ON_COMPLETION_DROP);
      DBUG_PRINT("info",("One-time event will be dropped=%d.", dropped));

      status= Event_timed::DISABLED;
      status_changed= true;
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
    if (on_completion == Event_timed::ON_COMPLETION_DROP)
      dropped= true;
    DBUG_PRINT("info", ("Dropped=%d", dropped));
    status= Event_timed::DISABLED;
    status_changed= true;

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
        if (on_completion == Event_timed::ON_COMPLETION_DROP)
          dropped= true;
        status= Event_timed::DISABLED;
        status_changed= true;
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
          status= Event_timed::DISABLED;
          status_changed= true;
          if (on_completion == Event_timed::ON_COMPLETION_DROP)
            dropped= true;
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
  DBUG_RETURN(false);
err:
  DBUG_PRINT("info", ("ret=1"));
  DBUG_RETURN(true);
}


/*
  Set the internal last_executed TIME struct to now. NOW is the
  time according to thd->query_start(), so the THD's clock.

  SYNOPSIS
    Event_timed::drop()
      thd   thread context
*/

void
Event_timed::mark_last_executed(THD *thd)
{
  TIME time_now;

  thd->end_time();
  my_tz_UTC->gmt_sec_to_TIME(&time_now, (my_time_t) thd->query_start());

  last_executed= time_now; /* was execute_at */
  last_executed_changed= true;
}



/*
  Drops the event

  SYNOPSIS
    Event_timed::drop()
      thd   thread context

  RETURN VALUE
    0       OK
   -1       Cannot open mysql.event
   -2       Cannot find the event in mysql.event (already deleted?)

   others   return code from SE in case deletion of the event row
            failed.
*/

int
Event_timed::drop(THD *thd)
{
  uint tmp= 0;
  DBUG_ENTER("Event_timed::drop");

  DBUG_RETURN(Events::get_instance()->
                db_repository->drop_event(thd, dbname, name, false, &tmp));
}


/*
  Saves status and last_executed_at to the disk if changed.

  SYNOPSIS
    Event_timed::update_fields()
      thd - thread context

  RETURN VALUE
    0   OK
    EVEX_OPEN_TABLE_FAILED    Error while opening mysql.event for writing
    EVEX_WRITE_ROW_FAILED   On error to write to disk

   others                   return code from SE in case deletion of the event
                            row failed.
*/

bool
Event_timed::update_fields(THD *thd)
{
  TABLE *table;
  Open_tables_state backup;
  int ret;

  DBUG_ENTER("Event_timed::update_time_fields");

  DBUG_PRINT("enter", ("name: %*s", name.length, name.str));

  /* No need to update if nothing has changed */
  if (!(status_changed || last_executed_changed))
    DBUG_RETURN(0);

  thd->reset_n_backup_open_tables_state(&backup);

  if (Events::get_instance()->open_event_table(thd, TL_WRITE, &table))
  {
    ret= EVEX_OPEN_TABLE_FAILED;
    goto done;
  }


  if ((ret= evex_db_find_event_by_name(thd, dbname, name, table)))
    goto done;

  store_record(table,record[1]);
  /* Don't update create on row update. */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  if (last_executed_changed)
  {
    table->field[ET_FIELD_LAST_EXECUTED]->set_notnull();
    table->field[ET_FIELD_LAST_EXECUTED]->store_time(&last_executed,
                                               MYSQL_TIMESTAMP_DATETIME);
    last_executed_changed= false;
  }
  if (status_changed)
  {
    table->field[ET_FIELD_STATUS]->set_notnull();
    table->field[ET_FIELD_STATUS]->store((longlong)status, true);
    status_changed= false;
  }

  if ((table->file->ha_update_row(table->record[1],table->record[0])))
    ret= EVEX_WRITE_ROW_FAILED;

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
  char tmp_buff[128];
  String expr_buf(tmp_buff, sizeof(tmp_buff), system_charset_info);
  expr_buf.length(0);

  DBUG_ENTER("get_create_event");
  DBUG_PRINT("ret_info",("body_len=[%d]body=[%s]", body.length, body.str));

  if (expression && Events::reconstruct_interval_expression(&expr_buf, interval,
                                                            expression))
    DBUG_RETURN(EVEX_MICROSECOND_UNSUP);

  buf->append(STRING_WITH_LEN("CREATE EVENT "));
  append_identifier(thd, buf, name.str, name.length);

  buf->append(STRING_WITH_LEN(" ON SCHEDULE "));
  if (expression)
  {
    buf->append(STRING_WITH_LEN("EVERY "));
    buf->append(expr_buf);
    buf->append(' ');
    LEX_STRING *ival= &interval_type_to_name[interval];
    buf->append(ival->str, ival->length);
  }
  else
  {
    char dtime_buff[20*2+32];/* +32 to make my_snprintf_{8bit|ucs2} happy */
    buf->append(STRING_WITH_LEN("AT '"));
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
  Executes the event (the underlying sp_head object);

  SYNOPSIS
    evex_fill_row()
      thd       THD
      mem_root  If != NULL use it to compile the event on it

  RETURN VALUE
    0        success
    -99      No rights on this.dbname.str
    -100     event in execution (parallel execution is impossible)
    others   retcodes of sp_head::execute_procedure()
*/

int
Event_timed::execute(THD *thd, MEM_ROOT *mem_root)
{
  Security_context *save_ctx;
  /* this one is local and not needed after exec */
  Security_context security_ctx;
  int ret= 0;

  DBUG_ENTER("Event_timed::execute");
  DBUG_PRINT("info", ("    EVEX EXECUTING event %s.%s [EXPR:%d]",
               dbname.str, name.str, (int) expression));

  thd->change_security_context(definer_user, definer_host, dbname,
                               &security_ctx, &save_ctx);

  if (!sphead && (ret= compile(thd, mem_root)))
    goto done;
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
  /* Will compile every time a new sp_head on different root */
  free_sp();

done:
  thd->restore_security_context(save_ctx);
  /*
    1. Don't cache sphead if allocated on another mem_root
    2. Don't call security_ctx.destroy() because this will free our dbname.str
       name.str and definer.str
  */
  if (mem_root && sphead)
  {
    delete sphead;
    sphead= 0;
  }
  DBUG_PRINT("info", ("    EVEX EXECUTED event %s.%s  [EXPR:%d]. RetCode=%d",
                      dbname.str, name.str, (int) expression, ret));

  DBUG_RETURN(ret);
}


/*
  Frees the memory of the sp_head object we hold
  SYNOPSIS
    Event_timed::free_sp()
*/

void
Event_timed::free_sp()
{
  delete sphead;
  sphead= 0;
}


/*
  Compiles an event before it's execution. Compiles the anonymous
  sp_head object held by the event

  SYNOPSIS
    Event_timed::compile()
      thd        thread context, used for memory allocation mostly
      mem_root   if != NULL then this memory root is used for allocs
                 instead of thd->mem_root

  RETURN VALUE
    0                       success
    EVEX_COMPILE_ERROR      error during compilation
    EVEX_MICROSECOND_UNSUP  mysql.event was tampered 
*/

int
Event_timed::compile(THD *thd, MEM_ROOT *mem_root)
{
  int ret= 0;
  MEM_ROOT *tmp_mem_root= 0;
  LEX *old_lex= thd->lex, lex;
  char *old_db;
  int old_db_length;
  char *old_query;
  uint old_query_len;
  ulong old_sql_mode= thd->variables.sql_mode;
  char create_buf[2048];
  String show_create(create_buf, sizeof(create_buf), system_charset_info);
  CHARSET_INFO *old_character_set_client,
               *old_collation_connection,
               *old_character_set_results;
  Security_context *save_ctx;
  /* this one is local and not needed after exec */
  Security_context security_ctx;

  DBUG_ENTER("Event_timed::compile");

  show_create.length(0);

  switch (get_create_event(thd, &show_create)) {
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

  thd->query= show_create.c_ptr();
  thd->query_length= show_create.length();
  DBUG_PRINT("info", ("query:%s",thd->query));

  thd->change_security_context(definer_user, definer_host, dbname,
                               &security_ctx, &save_ctx);
  thd->lex= &lex;
  lex_start(thd, (uchar*)thd->query, thd->query_length);
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
    if (lex.sphead)
    {
      if (&lex != thd->lex)
        thd->lex->sphead->restore_lex(thd);
      delete lex.sphead;
      lex.sphead= 0;
    }
    ret= EVEX_COMPILE_ERROR;
    goto done;
  }
  DBUG_PRINT("note", ("success compiling %s.%s", dbname.str, name.str));

  sphead= lex.sphead;
  sphead->m_db= dbname;

  sphead->set_definer(definer.str, definer.length);
  sphead->set_info(0, 0, &lex.sp_chistics, sql_mode);
  sphead->optimize();
  ret= 0;
done:

  lex_end(&lex);
  thd->restore_security_context(save_ctx);
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


extern pthread_attr_t connection_attrib;

/*
  Checks whether is possible and forks a thread. Passes self as argument.

  RETURN VALUE
    EVENT_EXEC_STARTED       OK
    EVENT_EXEC_ALREADY_EXEC  Thread not forked, already working
    EVENT_EXEC_CANT_FORK     Unable to spawn thread (error)
*/

int
Event_timed::spawn_now(void * (*thread_func)(void*), void *arg)
{
  THD *thd= current_thd;
  int ret= EVENT_EXEC_STARTED;
  DBUG_ENTER("Event_timed::spawn_now");
  DBUG_PRINT("info", ("[%s.%s]", dbname.str, name.str));

  VOID(pthread_mutex_lock(&this->LOCK_running));

  DBUG_PRINT("info", ("SCHEDULER: execute_at of %s is %lld", name.str,
             TIME_to_ulonglong_datetime(&execute_at)));
  mark_last_executed(thd);
  if (compute_next_execution_time())
  {
    sql_print_error("SCHEDULER: Error while computing time of %s.%s . "
                    "Disabling after execution.", dbname.str, name.str);
    status= DISABLED;
  }
  DBUG_PRINT("evex manager", ("[%10s] next exec at [%llu]", name.str,
             TIME_to_ulonglong_datetime(&execute_at)));
   /*
    1. For one-time event : year is > 0 and expression is 0
    2. For recurring, expression is != -=> check execute_at_null in this case
  */
  if ((execute_at.year && !expression) || execute_at_null)
  {
    sql_print_information("SCHEDULER: [%s.%s of %s] no more executions "
                          "after this one", dbname.str, name.str,
                          definer.str);
    flags |= EVENT_EXEC_NO_MORE | EVENT_FREE_WHEN_FINISHED;
  }

  update_fields(thd);

  if (!in_spawned_thread)
  {
    pthread_t th;
    in_spawned_thread= true;

    if (pthread_create(&th, &connection_attrib, thread_func, arg))
    {
      DBUG_PRINT("info", ("problem while spawning thread"));
      ret= EVENT_EXEC_CANT_FORK;
      in_spawned_thread= false;
    }
  }
  else
  {
    DBUG_PRINT("info", ("already in spawned thread. skipping"));
    ret= EVENT_EXEC_ALREADY_EXEC;
  }
  VOID(pthread_mutex_unlock(&this->LOCK_running));

  DBUG_RETURN(ret);  
}


bool
Event_timed::spawn_thread_finish(THD *thd)
{
  bool should_free;
  DBUG_ENTER("Event_timed::spawn_thread_finish");
  VOID(pthread_mutex_lock(&LOCK_running));
  in_spawned_thread= false;
  DBUG_PRINT("info", ("Sending COND_finished for thread %d", thread_id));
  thread_id= 0;
  if (dropped)
    drop(thd);
  pthread_cond_broadcast(&COND_finished);
  should_free= flags & EVENT_FREE_WHEN_FINISHED;
  VOID(pthread_mutex_unlock(&LOCK_running));
  DBUG_RETURN(should_free);
}


/*
  Kills a running event
  SYNOPSIS
    Event_timed::kill_thread()
    
  RETURN VALUE 
     0    OK
    -1    EVEX_CANT_KILL
    !0   Error 
*/

int
Event_timed::kill_thread(THD *thd)
{
  int ret= 0;
  DBUG_ENTER("Event_timed::kill_thread");
  pthread_mutex_lock(&LOCK_running);
  DBUG_PRINT("info", ("thread_id=%lu", thread_id));

  if (thread_id == thd->thread_id)
  {
    /*
      We don't kill ourselves in cases like :
      alter event e_43 do alter event e_43 do set @a = 4 because
      we will never receive COND_finished.
    */
    DBUG_PRINT("info", ("It's not safe to kill ourselves in self altering queries"));
    ret= EVEX_CANT_KILL;
  }
  else if (thread_id && !(ret= kill_one_thread(thd, thread_id, false)))
  {
    thd->enter_cond(&COND_finished, &LOCK_running, "Waiting for finished");
    DBUG_PRINT("info", ("Waiting for COND_finished from thread %d", thread_id));
    while (thread_id)
      pthread_cond_wait(&COND_finished, &LOCK_running);

    DBUG_PRINT("info", ("Got COND_finished"));
    /* This will implicitly unlock LOCK_running. Hence we return before that */
    thd->exit_cond("");

    DBUG_RETURN(0);
  }
  else if (!thread_id && in_spawned_thread)
  {
    /*
      Because the manager thread waits for the forked thread to update thread_id
      this situation is impossible.
    */
    DBUG_ASSERT(0);
  }
  pthread_mutex_unlock(&LOCK_running);
  DBUG_PRINT("exit", ("%d", ret));
  DBUG_RETURN(ret);
}


/*
  Checks whether two events have the same name

  SYNOPSIS
    event_timed_name_equal()

  RETURN VALUE
    TRUE  names are equal
    FALSE names are not equal
*/

bool
event_timed_name_equal(Event_timed *et, LEX_STRING *name)
{
  return !sortcmp_lex_string(et->name, *name, system_charset_info);
}


/*
  Checks whether two events are in the same schema

  SYNOPSIS
    event_timed_db_equal()

  RETURN VALUE
    TRUE  schemas are equal
    FALSE schemas are not equal
*/

bool
event_timed_db_equal(Event_timed *et, LEX_STRING *db)
{
  return !sortcmp_lex_string(et->dbname, *db, system_charset_info);
}


/*
  Checks whether two events are equal by identifiers

  SYNOPSIS
    event_timed_identifier_equal()

  RETURN VALUE
    TRUE   equal
    FALSE  not equal
*/

bool
event_timed_identifier_equal(LEX_STRING db, LEX_STRING name, Event_timed *b)
{
  return !sortcmp_lex_string(name, b->name, system_charset_info) &&
         !sortcmp_lex_string(db, b->dbname, system_charset_info);
}

