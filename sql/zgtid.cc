/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "zgtids.h"


#ifdef HAVE_GTID


#include "mysqld_error.h"


enum_return_status Gtid::parse(Sid_map *sid_map, const char *text)
{
  DBUG_ENTER("Gtid::parse");
  rpl_sid sid;

  // parse sid
  if (sid.parse(text) == RETURN_STATUS_OK)
  {
    rpl_sidno sidno_var= sid_map->add_sid(&sid);
    if (sidno_var <= 0)
      RETURN_REPORTED_ERROR;
    text += Uuid::TEXT_LENGTH;

    // parse colon
    if (*text == ':')
    {
      text++;

      // parse gno
      rpl_gno gno_var= parse_gno(&text);
      if (gno_var > 0 && *text == 0)
      {
        sidno= sidno_var;
        gno= gno_var;
        RETURN_OK;
      }
    }
  }
  BINLOG_ERROR(("Malformed GTID specification: %.200s", text),
               (ER_MALFORMED_GTID_SPECIFICATION, MYF(0), text));
  RETURN_REPORTED_ERROR;
}


int Gtid::to_string(const rpl_sid *sid, char *buf) const
{
  DBUG_ENTER("Gtid::to_string");
  char *s= buf + sid->to_string(buf);
  *s= ':';
  s++;
  s+= format_gno(s, gno);
  DBUG_RETURN(s - buf);
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
  if (!rpl_sid::is_valid(text))
    DBUG_RETURN(false);
  text += Uuid::TEXT_LENGTH;
  if (*text != ':')
    DBUG_RETURN(false);
  text++;
  if (parse_gno(&text) <= 0)
    DBUG_RETURN(false);
  if (*text != 0)
    DBUG_RETURN(false);
  DBUG_RETURN(true);
}


#endif
