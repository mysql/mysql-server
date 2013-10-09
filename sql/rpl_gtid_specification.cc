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

#ifndef MYSQL_CLIENT
#include "mysqld.h"
#endif

//const int Gtid_specification::MAX_TEXT_LENGTH;


#ifndef MYSQL_CLIENT

enum_return_status Gtid_specification::parse(Sid_map *sid_map, const char *text)
{
  DBUG_ENTER("Gtid_specification::parse");
  DBUG_ASSERT(text != NULL);
  if (my_strcasecmp(&my_charset_latin1, text, "AUTOMATIC") == 0)
  {
    type= AUTOMATIC_GROUP;
    gtid.sidno= 0;
    gtid.gno= 0;
  }
  else if (my_strcasecmp(&my_charset_latin1, text, "ANONYMOUS") == 0)
  {
    type= ANONYMOUS_GROUP;
    gtid.sidno= 0;
    gtid.gno= 0;
  }
  else
  {
    PROPAGATE_REPORTED_ERROR(gtid.parse(sid_map, text));
    type= GTID_GROUP;
  }
  RETURN_OK;
};


enum_group_type Gtid_specification::get_type(const char *text)
{
  DBUG_ENTER("Gtid_specification::get_type");
  DBUG_ASSERT(text != NULL);
  if (my_strcasecmp(&my_charset_latin1, text, "AUTOMATIC") == 0)
    DBUG_RETURN(AUTOMATIC_GROUP);
  else if (my_strcasecmp(&my_charset_latin1, text, "ANONYMOUS") == 0)
    DBUG_RETURN(ANONYMOUS_GROUP);
  else
    DBUG_RETURN(Gtid::is_valid(text) ? GTID_GROUP : INVALID_GROUP);
}

#endif // ifndef MYSQL_CLIENT


int Gtid_specification::to_string(const rpl_sid *sid, char *buf) const
{
  DBUG_ENTER("Gtid_specification::to_string(char*)");
  switch (type)
  {
  case AUTOMATIC_GROUP:
    strcpy(buf, "AUTOMATIC");
    DBUG_RETURN(9);
  case ANONYMOUS_GROUP:
    strcpy(buf, "ANONYMOUS");
    DBUG_RETURN(9);
  /*
    UNDEFINED_GROUP must be printed like GTID_GROUP because of
    SELECT @@SESSION.GTID_NEXT.
  */
  case UNDEFINED_GROUP:
  case GTID_GROUP:
    DBUG_RETURN(gtid.to_string(*sid, buf));
  case INVALID_GROUP:
    DBUG_ASSERT(0);
  }
  DBUG_ASSERT(0);
  DBUG_RETURN(0);
}


int Gtid_specification::to_string(const Sid_map *sid_map, char *buf) const
{
  return to_string(type == GTID_GROUP || type == UNDEFINED_GROUP ?
                   &sid_map->sidno_to_sid(gtid.sidno) : NULL,
                   buf);
}
