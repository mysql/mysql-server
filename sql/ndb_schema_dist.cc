/*
   Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_global.h"
#include "ndb_schema_dist.h"


const char*
get_schema_type_name(uint type)
{
  switch(type){
  case SOT_DROP_TABLE:
    return "DROP_TABLE";
  case SOT_CREATE_TABLE:
    return "CREATE_TABLE";
  case SOT_ALTER_TABLE_COMMIT:
    return "ALTER_TABLE_COMMIT";
  case SOT_DROP_DB:
    return "DROP_DB";
  case SOT_CREATE_DB:
    return "CREATE_DB";
  case SOT_ALTER_DB:
    return "ALTER_DB";
  case SOT_CLEAR_SLOCK:
    return "CLEAR_SLOCK";
  case SOT_TABLESPACE:
    return "TABLESPACE";
  case SOT_LOGFILE_GROUP:
    return "LOGFILE_GROUP";
  case SOT_RENAME_TABLE:
    return "RENAME_TABLE";
  case SOT_TRUNCATE_TABLE:
    return "TRUNCATE_TABLE";
  case SOT_RENAME_TABLE_PREPARE:
    return "RENAME_TABLE_PREPARE";
  case SOT_ONLINE_ALTER_TABLE_PREPARE:
    return "ONLINE_ALTER_TABLE_PREPARE";
  case SOT_ONLINE_ALTER_TABLE_COMMIT:
    return "ONLINE_ALTER_TABLE_COMMIT";
  case SOT_CREATE_USER:
    return "CREATE_USER";
  case SOT_DROP_USER:
    return "DROP_USER";
  case SOT_RENAME_USER:
    return "RENAME_USER";
  case SOT_GRANT:
    return "GRANT";
  case SOT_REVOKE:
    return "REVOKE";
  }
  return "<unknown>";
}

extern struct NDB_SHARE* ndb_schema_share;

bool ndb_schema_dist_is_ready(void)
{
  if (ndb_schema_share)
    return true;

  DBUG_PRINT("info", ("ndb schema dist not ready"));
  return false;
}
