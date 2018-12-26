/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

// Implements the interface defined in
#include "sql/ndb_schema_dist_table.h"

#include "sql/ndb_thd_ndb.h"
#include "sql/ha_ndbcluster_tables.h"

Ndb_schema_dist_table::Ndb_schema_dist_table(Thd_ndb* thd_ndb)
    : Ndb_util_table(thd_ndb, NDB_REP_DB, NDB_SCHEMA_TABLE) {}

Ndb_schema_dist_table::~Ndb_schema_dist_table() {}

static const char* COL_DB = "db";
static const char* COL_NAME = "name";
static const char* COL_SLOCK = "slock";
static const char* COL_QUERY = "query";
static const char* COL_NODEID = "node_id";
static const char* COL_EPOCH = "epoch";
static const char* COL_ID = "id";
static const char* COL_VERSION = "version";
static const char* COL_TYPE = "type";

bool Ndb_schema_dist_table::check_schema() const {
  // db
  // varbinary, at least 63 bytes long
  // NOTE! The 63 bytes length for the db and name column is a legacy bug
  // which doesn't have enough room for MySQL's max identitfier size. For
  // backwards compatiblity reasons it's allowed to use such a schema
  // distribution table but not all identifiers will be possible to distribute.
  if (!(check_column_exist(COL_DB) && check_column_varbinary(COL_DB) &&
        check_column_minlength(COL_DB, 63))) {
    return false;
  }

  // name
  // varbinary, at least 63 bytes long
  if (!(check_column_exist(COL_NAME) && check_column_varbinary(COL_NAME) &&
        check_column_minlength(COL_NAME, 63))) {
    return false;
  }
  // Check that db + name is the primary key, otherwise pk operations
  // using that key won't work
  if (!check_primary_key({COL_DB, COL_NAME})) {
    return false;
  }

  // slock
  // binary, need room for at least 32 bytes(i.e 32*8 bits for 256 nodes)
  if (!(check_column_exist(COL_SLOCK) && check_column_binary(COL_SLOCK) &&
        check_column_minlength(COL_SLOCK, 32))) {
    return false;
  }

  // query
  // blob
  if (!(check_column_exist(COL_QUERY) && check_column_blob(COL_QUERY))) {
    return false;
  }

  // nodeid
  // unsigned int
  if (!(check_column_exist(COL_NODEID) && check_column_unsigned(COL_NODEID))) {
    return false;
  }

  // epoch
  // unsigned bigint
  if (!(check_column_exist(COL_EPOCH) && check_column_bigunsigned(COL_EPOCH))) {
    return false;
  }

  // id
  // unsigned int
  if (!(check_column_exist(COL_ID) && check_column_unsigned(COL_ID))) {
    return false;
  }

  // version
  // unsigned int
  if (!(check_column_exist(COL_VERSION) &&
        check_column_unsigned(COL_VERSION))) {
    return false;
  }

  // type
  // unsigned int
  if (!(check_column_exist(COL_TYPE) && check_column_unsigned(COL_TYPE))) {
    return false;
  }

  return true;
}
