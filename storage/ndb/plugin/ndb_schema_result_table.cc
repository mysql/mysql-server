/*
   Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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
#include "storage/ndb/plugin/ndb_schema_result_table.h"

#include <sstream>

#include "storage/ndb/plugin/ndb_thd_ndb.h"

const std::string Ndb_schema_result_table::DB_NAME = "mysql";
const std::string Ndb_schema_result_table::TABLE_NAME = "ndb_schema_result";

const char *Ndb_schema_result_table::COL_NODEID = "nodeid";
const char *Ndb_schema_result_table::COL_SCHEMA_OP_ID = "schema_op_id";
const char *Ndb_schema_result_table::COL_PARTICIPANT_NODEID =
    "participant_nodeid";
const char *Ndb_schema_result_table::COL_RESULT = "result";
const char *Ndb_schema_result_table::COL_MESSAGE = "message";

Ndb_schema_result_table::Ndb_schema_result_table(Thd_ndb *thd_ndb)
    : Ndb_util_table(thd_ndb, DB_NAME, TABLE_NAME, true) {}

Ndb_schema_result_table::~Ndb_schema_result_table() {}

bool Ndb_schema_result_table::check_schema() const {
  // nodeid
  // unsigned int
  if (!(check_column_exist(COL_NODEID) && check_column_unsigned(COL_NODEID))) {
    return false;
  }

  // schema_op_id
  // unsigned int
  if (!(check_column_exist(COL_SCHEMA_OP_ID) &&
        check_column_unsigned(COL_SCHEMA_OP_ID))) {
    return false;
  }

  // participant_nodeid
  // unsigned int
  if (!(check_column_exist(COL_PARTICIPANT_NODEID) &&
        check_column_unsigned(COL_PARTICIPANT_NODEID))) {
    return false;
  }

  // Check that nodeid + schema_op_id + participant_nodeid is the primary key
  if (!check_primary_key(
          {COL_NODEID, COL_SCHEMA_OP_ID, COL_PARTICIPANT_NODEID})) {
    return false;
  }

  // result
  // unsigned int
  if (!(check_column_exist(COL_RESULT) && check_column_unsigned(COL_RESULT))) {
    return false;
  }

  // message
  // varbinary, at least 255 bytes long
  if (!(check_column_exist(COL_MESSAGE) &&
        check_column_varbinary(COL_MESSAGE) &&
        check_column_minlength(COL_MESSAGE, 255))) {
    return false;
  }

  return true;
}

bool Ndb_schema_result_table::define_table_ndb(NdbDictionary::Table &new_table,
                                               unsigned mysql_version) const {
  // Allow later online add column
  new_table.setForceVarPart(true);

  // Allow table to be read+write also in single user mode
  new_table.setSingleUserMode(NdbDictionary::Table::SingleUserModeReadWrite);

  {
    // nodeid UNSIGNED NOT NULL
    NdbDictionary::Column col_nodeid(COL_NODEID);
    col_nodeid.setType(NdbDictionary::Column::Unsigned);
    col_nodeid.setNullable(false);
    col_nodeid.setPrimaryKey(true);
    if (!define_table_add_column(new_table, col_nodeid)) return false;
  }

  {
    // schema_op_id UNSIGNED NOT NULL
    NdbDictionary::Column col_schema_op_id(COL_SCHEMA_OP_ID);
    col_schema_op_id.setType(NdbDictionary::Column::Unsigned);
    col_schema_op_id.setNullable(false);
    col_schema_op_id.setPrimaryKey(true);
    if (!define_table_add_column(new_table, col_schema_op_id)) return false;
  }

  {
    // participant_nodeid UNSIGNED NOT NULL
    NdbDictionary::Column col_participant_nodeid(COL_PARTICIPANT_NODEID);
    col_participant_nodeid.setType(NdbDictionary::Column::Unsigned);
    col_participant_nodeid.setNullable(false);
    col_participant_nodeid.setPrimaryKey(true);
    if (!define_table_add_column(new_table, col_participant_nodeid))
      return false;
  }

  {
    // result UNSIGNED NOT NULL
    NdbDictionary::Column col_result(COL_RESULT);
    col_result.setType(NdbDictionary::Column::Unsigned);
    col_result.setNullable(false);
    if (!define_table_add_column(new_table, col_result)) return false;
  }

  {
    // message VARBINARY(255) NOT NULL
    NdbDictionary::Column col_message(COL_MESSAGE);
    col_message.setType(NdbDictionary::Column::Varbinary);
    col_message.setLength(255);
    col_message.setNullable(false);
    if (!define_table_add_column(new_table, col_message)) return false;
  }

  (void)mysql_version;  // Only one version can be created

  return true;
}

bool Ndb_schema_result_table::need_upgrade() const { return false; }

std::string Ndb_schema_result_table::define_table_dd() const {
  std::stringstream ss;
  ss << "CREATE TABLE " << db_name() << "." << table_name() << "(\n";
  ss << "nodeid INT UNSIGNED NOT NULL,"
        "schema_op_id INT UNSIGNED NOT NULL,"
        "participant_nodeid INT UNSIGNED NOT NULL,"
        "result INT UNSIGNED NOT NULL,"
        "message VARBINARY(255) NOT NULL,"
        "PRIMARY KEY(nodeid, schema_op_id, participant_nodeid)"
     << ") ENGINE=ndbcluster";
  return ss.str();
}

bool Ndb_schema_result_table::drop_events_in_NDB() const {
  // Drop the default event
  if (!drop_event_in_NDB("REPL$mysql/ndb_schema_result")) return false;

  return true;
}

void Ndb_schema_result_table::pack_message(const char *message, char *buf) {
  pack_varbinary(Ndb_schema_result_table::COL_MESSAGE, message, buf);
}

std::string Ndb_schema_result_table::unpack_message(
    const std::string &packed_message) {
  if (!open()) {
    return std::string("");
  }
  return unpack_varbinary(Ndb_schema_result_table::COL_MESSAGE,
                          packed_message.c_str());
}
