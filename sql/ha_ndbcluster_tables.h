/*
   Copyright (C) 2000-2003 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define NDB_REP_DB      "mysql"
#define NDB_REP_TABLE   "ndb_binlog_index"
#define NDB_APPLY_TABLE "ndb_apply_status"
#define NDB_SCHEMA_TABLE "ndb_schema"
#define NDB_REPLICATION_TABLE "ndb_replication"

enum Ndb_binlog_type
{
  NBT_DEFAULT                   = 0
  ,NBT_NO_LOGGING               = 1
  ,NBT_UPDATED_ONLY             = 2
  ,NBT_FULL                     = 3
  ,NBT_USE_UPDATE               = 4 /* bit 0x4 indicates USE_UPDATE */
  ,NBT_UPDATED_ONLY_USE_UPDATE  = NBT_UPDATED_ONLY | NBT_USE_UPDATE
  ,NBT_FULL_USE_UPDATE          = NBT_FULL         | NBT_USE_UPDATE
};
