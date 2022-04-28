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
#include "storage/ndb/plugin/ndb_apply_status_table.h"

#include <sstream>

#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"

const std::string Ndb_apply_status_table::DB_NAME = "mysql";
const std::string Ndb_apply_status_table::TABLE_NAME = "ndb_apply_status";

static const char *COL_SERVER_ID = "server_id";
static const char *COL_EPOCH = "epoch";
static const char *COL_LOG_NAME = "log_name";
static const char *COL_START_POS = "start_pos";
static const char *COL_END_POS = "end_pos";

Ndb_apply_status_table::Ndb_apply_status_table(Thd_ndb *thd_ndb)
    : Ndb_util_table(thd_ndb, DB_NAME, TABLE_NAME, false) {}

Ndb_apply_status_table::~Ndb_apply_status_table() {}

bool Ndb_apply_status_table::check_schema() const {
  // server_id
  // unsigned int
  if (!(check_column_exist(COL_SERVER_ID) &&
        check_column_unsigned(COL_SERVER_ID))) {
    return false;
  }

  // Check that server_id is the primary key
  if (!check_primary_key({COL_SERVER_ID})) {
    return false;
  }

  // epoch
  // unsigned bigint
  if (!(check_column_exist(COL_EPOCH) && check_column_bigunsigned(COL_EPOCH))) {
    return false;
  }

  // log_name
  // varchar, min 255 long
  if (!(check_column_exist(COL_LOG_NAME) &&
        check_column_varchar(COL_LOG_NAME) &&
        check_column_minlength(COL_LOG_NAME, 255))) {
    return false;
  }

  // start_pos
  // unsigned bigint
  if (!(check_column_exist(COL_START_POS) &&
        check_column_bigunsigned(COL_START_POS))) {
    return false;
  }

  // end_pos
  // unsigned bigint
  if (!(check_column_exist(COL_END_POS) &&
        check_column_bigunsigned(COL_END_POS))) {
    return false;
  }

  return true;
}

bool Ndb_apply_status_table::define_table_ndb(NdbDictionary::Table &new_table,
                                              unsigned mysql_version) const {
  // Set metadata for backwards compatibility support, earlier versions
  // will see what they expect and can connect to NDB properly. The physical
  // table in NDB may be extended to support new functionality but should still
  // be possible to use.
  constexpr uint8 legacy_metadata[346] = {
      0x01, 0x00, 0x00, 0x00, 0x0c, 0x22, 0x00, 0x00, 0x4e, 0x01, 0x00, 0x00,
      0x78, 0x9c, 0xed, 0xda, 0x31, 0x4f, 0xc2, 0x50, 0x10, 0x07, 0xf0, 0xff,
      0x83, 0xb6, 0x94, 0x17, 0x41, 0x06, 0x74, 0x30, 0x0c, 0x8f, 0xc1, 0x44,
      0x48, 0xb4, 0x60, 0xa2, 0x9b, 0x89, 0x90, 0xa0, 0x36, 0x8a, 0x90, 0xa6,
      0x0b, 0x13, 0x41, 0x68, 0x14, 0x82, 0x40, 0x0a, 0x3a, 0xfb, 0xe9, 0xfc,
      0x58, 0x3e, 0xaf, 0x0d, 0xa0, 0xa3, 0x5b, 0x49, 0xbc, 0xdf, 0xd2, 0xbb,
      0xeb, 0xa5, 0xfd, 0x77, 0x6c, 0xd3, 0x2f, 0x21, 0xf3, 0x69, 0xa0, 0x20,
      0x80, 0x1a, 0x30, 0x16, 0x25, 0x2a, 0xd6, 0x52, 0x65, 0x64, 0x01, 0x33,
      0x2a, 0xed, 0xcd, 0x6c, 0x4c, 0xa7, 0x8f, 0x3f, 0x01, 0x3f, 0xee, 0xf2,
      0x80, 0xe3, 0x00, 0x0a, 0x8c, 0x31, 0xc6, 0x18, 0x63, 0x8c, 0x31, 0xc6,
      0x18, 0xdb, 0x65, 0x82, 0x5e, 0xe8, 0x25, 0x1d, 0x0d, 0x88, 0x34, 0x75,
      0x1f, 0xd4, 0x1e, 0x5d, 0x1b, 0xd0, 0x5d, 0xcf, 0x6d, 0x37, 0xbc, 0x9e,
      0x4e, 0x3a, 0x1f, 0xfb, 0x17, 0x24, 0x66, 0xa3, 0xa7, 0xe1, 0xf4, 0x6d,
      0xb9, 0x0a, 0xc2, 0x33, 0xea, 0x55, 0xb7, 0xe1, 0xf9, 0xae, 0xef, 0x76,
      0x1e, 0x55, 0xb3, 0xa7, 0xee, 0x5b, 0x3d, 0xe5, 0x54, 0xcb, 0x17, 0xb5,
      0xcb, 0x7a, 0x5d, 0x35, 0x1e, 0x6e, 0x3b, 0x9e, 0xeb, 0xdf, 0xb5, 0xd5,
      0x95, 0x3a, 0x57, 0x55, 0x47, 0x9d, 0x54, 0x20, 0xf2, 0x49, 0x3f, 0x00,
      0x63, 0x8c, 0x31, 0xc6, 0x18, 0x63, 0x8c, 0xed, 0xba, 0xbd, 0x14, 0x0a,
      0x49, 0x67, 0x48, 0x92, 0x80, 0x89, 0x09, 0x5a, 0x02, 0x28, 0x89, 0xd3,
      0xed, 0xb4, 0x8b, 0xc3, 0x75, 0x35, 0x81, 0x65, 0xa6, 0x8a, 0x15, 0xf5,
      0x47, 0x30, 0x20, 0x97, 0x41, 0xf8, 0x1e, 0x84, 0xfd, 0xf1, 0x88, 0x2e,
      0x6d, 0x05, 0x8b, 0xf9, 0xf0, 0x05, 0x16, 0xb2, 0xd3, 0xf9, 0x73, 0x7f,
      0x36, 0x78, 0x0d, 0x90, 0xa1, 0x8d, 0xd5, 0x20, 0x5c, 0xf5, 0x17, 0xf3,
      0x25, 0x6c, 0xd8, 0xc1, 0x6c, 0x14, 0x97, 0x86, 0x94, 0x12, 0xf1, 0xe7,
      0x17, 0xba, 0x6d, 0xda, 0x06, 0x4c, 0xab, 0x58, 0x8c, 0x7e, 0xbf, 0x68,
      0x46, 0x03, 0x9b, 0x06, 0x56, 0xf6, 0x46, 0x23, 0x47, 0x99, 0xa3, 0xc1,
      0xbe, 0x03, 0x64, 0x24, 0x6d, 0xe4, 0xc4, 0xcf, 0x86, 0x6d, 0xd3, 0xe0,
      0xe0, 0xd7, 0x40, 0x6f, 0xc3, 0xe8, 0x38, 0x89, 0xde, 0xc4, 0xd0, 0xdb,
      0x0c, 0x7a, 0x1d, 0x40, 0xe3, 0x1b, 0x18, 0x8d, 0x4a, 0x95};
  if (new_table.setFrm(legacy_metadata, sizeof(legacy_metadata)) != 0) {
    push_warning("Failed to set legacy metadata");
    return false;
  }

  new_table.setForceVarPart(true);

  {
    // server_id INT UNSIGNED NOT NULL
    NdbDictionary::Column col_server_id(COL_SERVER_ID);
    col_server_id.setType(NdbDictionary::Column::Unsigned);
    col_server_id.setNullable(false);
    col_server_id.setPrimaryKey(true);
    if (!define_table_add_column(new_table, col_server_id)) return false;
  }

  {
    // epoch BIGINT UNSIGNED NOT NULL
    NdbDictionary::Column col_epoch(COL_EPOCH);
    col_epoch.setType(NdbDictionary::Column::Bigunsigned);
    col_epoch.setNullable(false);
    if (!define_table_add_column(new_table, col_epoch)) return false;
  }

  {
    // log_name VARCHAR(255) NOT NULL
    NdbDictionary::Column col_log_name(COL_LOG_NAME);
    col_log_name.setType(NdbDictionary::Column::Varchar);
    col_log_name.setCharset(&my_charset_latin1_bin);
    col_log_name.setLength(255);
    col_log_name.setNullable(false);
    if (!define_table_add_column(new_table, col_log_name)) return false;
  }

  {
    // start_pos BIGINT UNSIGNED NOT NULL
    NdbDictionary::Column col_start_pos(COL_START_POS);
    col_start_pos.setType(NdbDictionary::Column::Bigunsigned);
    col_start_pos.setNullable(false);
    if (!define_table_add_column(new_table, col_start_pos)) return false;
  }

  {
    // end_pos BIGINT UNSIGNED NOT NULL
    NdbDictionary::Column col_end_pos(COL_END_POS);
    col_end_pos.setType(NdbDictionary::Column::Bigunsigned);
    col_end_pos.setNullable(false);
    if (!define_table_add_column(new_table, col_end_pos)) return false;
  }

  (void)mysql_version;  // Only one version can be created

  return true;
}

bool Ndb_apply_status_table::drop_events_in_NDB() const {
  // Drop the default event
  if (!drop_event_in_NDB("REPL$mysql/ndb_apply_status")) return false;
  return true;
}

bool Ndb_apply_status_table::need_upgrade() const { return false; }

bool Ndb_apply_status_table::need_reinstall(const dd::Table *table_def) const {
  // Detect "log_name" column being VARBINARY and reinstall the table def in DD
  return ndb_dd_table_check_column_varbinary(table_def, COL_LOG_NAME);
}

std::string Ndb_apply_status_table::define_table_dd() const {
  std::stringstream ss;
  ss << "CREATE TABLE " << db_name() << "." << table_name() << "(\n";
  ss << "server_id INT UNSIGNED NOT NULL,"
        "epoch BIGINT UNSIGNED NOT NULL,"
        "log_name VARCHAR(255) NOT NULL,"
        "start_pos BIGINT UNSIGNED NOT NULL,"
        "end_pos BIGINT UNSIGNED NOT NULL,"
        "PRIMARY KEY USING HASH (server_id)\n"
        ") ENGINE=ndbcluster CHARACTER SET latin1";
  return ss.str();
}

bool Ndb_apply_status_table::is_apply_status_table(const char *db,
                                                   const char *table_name) {
  if (db == Ndb_apply_status_table::DB_NAME &&
      table_name == Ndb_apply_status_table::TABLE_NAME) {
    // This is the NDB table used for apply status information
    return true;
  }
  return false;
}
