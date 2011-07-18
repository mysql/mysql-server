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

#include "zgroups.h"


#ifdef HAVE_UGID


int const Group::MAX_TEXT_LENGTH;


enum_group_status Group::parse(Sid_map *sid_map, const char *text)
{
  DBUG_ENTER("Group::parse");
  rpl_sid sid;

  // parse sid
  GROUP_STATUS_THROW(sid.parse(text));
  sidno= sid_map->add_permanent(&sid);
  if (sidno < 0)
    DBUG_RETURN((enum_group_status)sidno);
  text += Uuid::TEXT_LENGTH;

  // parse colon
  if (*text == ':')
  {
    text++;

    // parse gno
    gno= parse_gno(&text);
    if (gno > 0 && *text == 0)
      DBUG_RETURN(GS_SUCCESS);
  }
  DBUG_RETURN(GS_ERROR_PARSE);
}


int Group::to_string(const Sid_map *sid_map, char *buf) const
{
  DBUG_ENTER("Group::to_string");
  char *s= buf + sid_map->sidno_to_sid(sidno)->to_string(buf);
  *s= ':';
  s++;
  s+= format_gno(s, gno);
  DBUG_RETURN(s - buf);
}


bool Group::is_valid(const char *text)
{
  DBUG_ENTER("Group::is_valid");
  if (!rpl_sid::is_valid(text))
    DBUG_RETURN(false);
  text += Uuid::TEXT_LENGTH;
  if (*text != ':')
    DBUG_RETURN(false);
  text++;
  if (parse_gno(&text) == 0)
    DBUG_RETURN(false);
  if (*text != 0)
    DBUG_RETURN(false);
  DBUG_RETURN(true);
}


#endif
