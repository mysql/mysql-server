/* Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */

#include "rpl_gtid.h"

#include "mysqld_error.h"     // ER_*

#ifndef MYSQL_CLIENT
#include "rpl_msr.h"
#include "sql_class.h"        // THD
#include "binlog.h"
#endif // ifndef MYSQL_CLIENT


// Todo: move other global gtid variable declarations here.
Checkable_rwlock *gtid_mode_lock= NULL;

ulong _gtid_mode;
const char *gtid_mode_names[]=
{"OFF", "OFF_PERMISSIVE", "ON_PERMISSIVE", "ON", NullS};
TYPELIB gtid_mode_typelib=
{ array_elements(gtid_mode_names) - 1, "", gtid_mode_names, NULL };


#ifndef MYSQL_CLIENT
enum_gtid_mode get_gtid_mode(enum_gtid_mode_lock have_lock)
{
  switch (have_lock)
  {
  case GTID_MODE_LOCK_NONE:
    global_sid_lock->rdlock();
    break;
  case GTID_MODE_LOCK_SID:
    global_sid_lock->assert_some_lock();
    break;
  case GTID_MODE_LOCK_CHANNEL_MAP:
#ifdef HAVE_REPLICATION
    channel_map.assert_some_lock();
#endif
    break;
  case GTID_MODE_LOCK_GTID_MODE:
    gtid_mode_lock->assert_some_lock();

/*
  This lock is currently not used explicitly by any of the places
  that calls get_gtid_mode.  Still it would be valid for a caller to
  use it to protect reads of GTID_MODE, so we keep the code here in
  case it is needed in the future.

  case GTID_MODE_LOCK_LOG:
    mysql_mutex_assert_owner(mysql_bin_log.get_log_lock());
    break;
*/
  }
  enum_gtid_mode ret= (enum_gtid_mode)_gtid_mode;
  if (have_lock == GTID_MODE_LOCK_NONE)
    global_sid_lock->unlock();
  return ret;
}
#endif


ulong _gtid_consistency_mode;
const char *gtid_consistency_mode_names[]=
{"OFF", "ON", "WARN", NullS};
TYPELIB gtid_consistency_mode_typelib=
{ array_elements(gtid_consistency_mode_names) - 1, "", gtid_consistency_mode_names, NULL };


#ifndef MYSQL_CLIENT
enum_gtid_consistency_mode get_gtid_consistency_mode()
{
  global_sid_lock->assert_some_lock();
  return (enum_gtid_consistency_mode)_gtid_consistency_mode;
}
#endif


enum_return_status Gtid::parse(Sid_map *sid_map, const char *text)
{
  DBUG_ENTER("Gtid::parse");
  rpl_sid sid;
  const char *s= text;

  SKIP_WHITESPACE();

  // parse sid
  if (sid.parse(s) == 0)
  {
    rpl_sidno sidno_var= sid_map->add_sid(sid);
    if (sidno_var <= 0)
      RETURN_REPORTED_ERROR;
    s += binary_log::Uuid::TEXT_LENGTH;

    SKIP_WHITESPACE();

    // parse colon
    if (*s == ':')
    {
      s++;

      SKIP_WHITESPACE();

      // parse gno
      rpl_gno gno_var= parse_gno(&s);
      if (gno_var > 0)
      {
        SKIP_WHITESPACE();
        if (*s == '\0')
        {
          sidno= sidno_var;
          gno= gno_var;
          RETURN_OK;
        }
        else
          DBUG_PRINT("info", ("expected end of string, found garbage '%.80s' "
                              "at char %d in '%s'",
                              s, (int)(s - text), text));
      }
      else
        DBUG_PRINT("info", ("GNO was zero or invalid (%lld) at char %d in '%s'",
                            gno_var, (int)(s - text), text));
    }
    else
      DBUG_PRINT("info", ("missing colon at char %d in '%s'",
                          (int)(s - text), text));
  }
  else
    DBUG_PRINT("info", ("not a uuid at char %d in '%s'",
                        (int)(s - text), text));
  BINLOG_ERROR(("Malformed GTID specification: %.200s", text),
               (ER_MALFORMED_GTID_SPECIFICATION, MYF(0), text));
  RETURN_REPORTED_ERROR;
}


int Gtid::to_string(const rpl_sid &sid, char *buf) const
{
  DBUG_ENTER("Gtid::to_string");
  char *s= buf + sid.to_string(buf);
  *s= ':';
  s++;
  s+= format_gno(s, gno);
  DBUG_RETURN((int)(s - buf));
}


int Gtid::to_string(const Sid_map *sid_map, char *buf, bool need_lock) const
{
  DBUG_ENTER("Gtid::to_string");
  int ret;
  if (sid_map != NULL)
  {
    Checkable_rwlock *lock= sid_map->get_sid_lock();
    if (lock)
    {
      if (need_lock)
        lock->rdlock();
      else
        lock->assert_some_lock();
    }
    const rpl_sid &sid= sid_map->sidno_to_sid(sidno);
    if (lock && need_lock)
      lock->unlock();
    ret= to_string(sid, buf);
  }
  else
  {
#ifdef DBUG_OFF
    /*
      NULL is only allowed in debug mode, since the sidno does not
      make sense for users but is useful to include in debug
      printouts.  Therefore, we want to ASSERT(0) in non-debug mode.
      Since there is no ASSERT in non-debug mode, we use abort
      instead.
    */
    abort();
#endif
    ret= sprintf(buf, "%d:%lld", sidno, gno);
  }
  DBUG_RETURN(ret);
}


bool Gtid::is_valid(const char *text)
{
  DBUG_ENTER("Gtid::is_valid");
  const char *s= text;
  SKIP_WHITESPACE();
  if (!rpl_sid::is_valid(s))
  {
    DBUG_PRINT("info", ("not a uuid at char %d in '%s'",
                        (int)(s - text), text));
    DBUG_RETURN(false);
  }
  s += binary_log::Uuid::TEXT_LENGTH;
  SKIP_WHITESPACE();
  if (*s != ':')
  {
    DBUG_PRINT("info", ("missing colon at char %d in '%s'",
                        (int)(s - text), text));
    DBUG_RETURN(false);
  }
  s++;
  SKIP_WHITESPACE();
  if (parse_gno(&s) <= 0)
  {
    DBUG_PRINT("info", ("GNO was zero or invalid at char %d in '%s'",
                        (int)(s - text), text));
    DBUG_RETURN(false);
  }
  SKIP_WHITESPACE();
  if (*s != 0)
  {
    DBUG_PRINT("info", ("expected end of string, found garbage '%.80s' "
                        "at char %d in '%s'",
                        s, (int)(s - text), text));
    DBUG_RETURN(false);
  }
  DBUG_RETURN(true);
}


#ifndef DBUG_OFF
void check_return_status(enum_return_status status, const char *action,
                         const char *status_name, int allow_unreported)
{
  if (status != RETURN_STATUS_OK)
  {
    DBUG_ASSERT(allow_unreported || status == RETURN_STATUS_REPORTED_ERROR);
    if (status == RETURN_STATUS_REPORTED_ERROR)
    {
#if !defined(MYSQL_CLIENT) && !defined(DBUG_OFF)
      THD *thd= current_thd;
      /*
        We create a new system THD with 'SYSTEM_THREAD_COMPRESS_GTID_TABLE'
        when initializing gtid state by fetching gtids during server startup,
        so we can check on it before diagnostic area is active and skip the
        assert in this case. We assert that diagnostic area logged the error
        outside server startup since the assert is realy useful.
     */
      DBUG_ASSERT(thd == NULL ||
                  thd->get_stmt_da()->status() == Diagnostics_area::DA_ERROR ||
                  (thd->get_stmt_da()->status() == Diagnostics_area::DA_EMPTY &&
                   thd->system_thread == SYSTEM_THREAD_COMPRESS_GTID_TABLE));
#endif
    }
    DBUG_PRINT("info", ("%s error %d (%s)", action, status, status_name));
  }
}
#endif // ! DBUG_OFF


#ifndef MYSQL_CLIENT
rpl_sidno get_sidno_from_global_sid_map(rpl_sid sid)
{
  DBUG_ENTER("get_sidno_from_global_sid_map(rpl_sid)");

  global_sid_lock->rdlock();
  rpl_sidno sidno= global_sid_map->add_sid(sid);
  global_sid_lock->unlock();

  DBUG_RETURN(sidno);
}

rpl_gno get_last_executed_gno(rpl_sidno sidno)
{
  DBUG_ENTER("get_last_executed_gno(rpl_sidno)");

  global_sid_lock->rdlock();
  rpl_gno gno= gtid_state->get_last_executed_gno(sidno);
  global_sid_lock->unlock();

  DBUG_RETURN(gno);
}
#endif // ifndef MYSQL_CLIENT
