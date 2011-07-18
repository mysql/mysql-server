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
#include "binlog.h"


#ifdef HAVE_UGID


const int Ugid_specification::MAX_TEXT_LENGTH;


enum_group_status Ugid_specification::parse(const char *text)
{
  DBUG_ENTER("Ugid_specification::parse");
  if (text == NULL || strcmp(text, "AUTOMATIC") == 0)
    type= AUTOMATIC;
  else if (strcmp(text, "ANONYMOUS") == 0)
    type= ANONYMOUS;
  else
  {
    GROUP_STATUS_THROW(group.parse(&mysql_bin_log.sid_map, text));
    type= UGID;
  }
  DBUG_PRINT("error", ("'%s' => type=%d\n", text, type));
  DBUG_RETURN(GS_SUCCESS);
};


int Ugid_specification::to_string(char *buf) const
{
  DBUG_ENTER("Ugid_specification::to_string(char*)");
  DBUG_PRINT("info", ("type=%d\n", type));
  switch (type)
  {
  case AUTOMATIC:
    strcpy(buf, "AUTOMATIC");
    DBUG_RETURN(9);
  case ANONYMOUS:
    strcpy(buf, "ANONYMOUS");
    DBUG_RETURN(9);
  case UGID:
    DBUG_RETURN(group.to_string(&mysql_bin_log.sid_map, buf));
  case INVALID:
    DBUG_ASSERT(0);
  }
  DBUG_ASSERT(0);
}


Ugid_specification::enum_type Ugid_specification::is_valid(const char *text)
{
  DBUG_ENTER("Ugid_specification::is_valid");
  DBUG_ASSERT(text != NULL);
  if (strcmp(text, "AUTOMATIC") == 0)
    DBUG_RETURN(AUTOMATIC);
  else if (strcmp(text, "ANONYMOUS") == 0)
    DBUG_RETURN(ANONYMOUS);
  else
    DBUG_RETURN(Group::is_valid(text) ? UGID : INVALID);
}


#endif /* HAVE_UGID */
