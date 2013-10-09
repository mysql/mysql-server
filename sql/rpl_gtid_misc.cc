/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include <ctype.h>

#include "mysqld_error.h"
#ifndef MYSQL_CLIENT
#include "sql_class.h"
#endif // ifndef MYSQL_CLIENT

enum_return_status Gtid::parse(Sid_map *sid_map, const char *text)
{
  DBUG_ENTER("Gtid::parse");
  rpl_sid sid;
  const char *s= text;

  SKIP_WHITESPACE();

  // parse sid
  if (sid.parse(s) == RETURN_STATUS_OK)
  {
    rpl_sidno sidno_var= sid_map->add_sid(sid);
    if (sidno_var <= 0)
      RETURN_REPORTED_ERROR;
    s += Uuid::TEXT_LENGTH;

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


int Gtid::to_string(const Sid_map *sid_map, char *buf) const
{
  DBUG_ENTER("Gtid::to_string");
  int ret= to_string(sid_map->sidno_to_sid(sidno), buf);
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
  s += Uuid::TEXT_LENGTH;
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
      DBUG_ASSERT(thd == NULL ||
                  thd->get_stmt_da()->status() == Diagnostics_area::DA_ERROR);
#endif
    }
    DBUG_PRINT("info", ("%s error %d (%s)", action, status, status_name));
  }
}
#endif // ! DBUG_OFF
