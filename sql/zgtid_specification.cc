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


#ifdef HAVE_GTID


#include "my_global.h" // REQUIRED by binlog.h -> log_event.h -> m_string.h -> my_bitmap.h
#include "binlog.h"


//const int Gtid_specification::MAX_TEXT_LENGTH;


enum_return_status Gtid_specification::parse(const char *text)
{
  DBUG_ENTER("Gtid_specification::parse");
  if (text == NULL || strcmp(text, "AUTOMATIC") == 0)
  {
    type= AUTOMATIC;
    group.sidno= 0;
    group.gno= 0;
  }
  else if (strcmp(text, "ANONYMOUS") == 0)
  {
    type= ANONYMOUS;
    group.sidno= 0;
    group.gno= 0;
  }
  else
  {
    PROPAGATE_REPORTED_ERROR(group.parse(&mysql_bin_log.sid_map, text));
    type= GTID;
  }
  RETURN_OK;
};


int Gtid_specification::to_string(char *buf) const
{
  DBUG_ENTER("Gtid_specification::to_string(char*)");
  switch (type)
  {
  case AUTOMATIC:
    strcpy(buf, "AUTOMATIC");
    DBUG_RETURN(9);
  case ANONYMOUS:
    strcpy(buf, "ANONYMOUS");
    DBUG_RETURN(9);
  case GTID:
    DBUG_RETURN(group.to_string(&mysql_bin_log.sid_map, buf));
  case INVALID:
    DBUG_ASSERT(0);
  }
  DBUG_ASSERT(0);
  DBUG_RETURN(0);
}


Gtid_specification::enum_type Gtid_specification::get_type(const char *text)
{
  DBUG_ENTER("Gtid_specification::is_valid");
  DBUG_ASSERT(text != NULL);
  if (strcmp(text, "AUTOMATIC") == 0)
    DBUG_RETURN(AUTOMATIC);
  else if (strcmp(text, "ANONYMOUS") == 0)
    DBUG_RETURN(ANONYMOUS);
  else
    DBUG_RETURN(Group::is_valid(text) ? GTID : INVALID);
}


#endif /* HAVE_GTID */
