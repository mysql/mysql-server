/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_index_stat_sample_table.h"

#include <sstream>

#include <kernel/ndb_limits.h>
#include <ndbapi/NdbDictionary.hpp>

/*
  The Ndb_index_stat_sample_table class is used to create the
  ndb_index_stat_sample system table. The table will be hidden in the MySQL Data
  Dictionary in a similar manner to other system utility tables.

  The table definition details have been extracted from similar code implemented
  in the NdbIndexStat class which is part of NdbApi. The table definition must
  remain the same regardless of the mechanism used to create it.
*/

static constexpr int stat_key_length =
    MAX_INDEX_STAT_KEY_SIZE * 4;  // MaxKeyCount
static constexpr int stat_value_length =
    MAX_INDEX_STAT_VALUE_CSIZE * 4;  // MaxValueCBytes

Ndb_index_stat_sample_table::Ndb_index_stat_sample_table(Thd_ndb *thd_ndb)
    : Ndb_util_table(thd_ndb, "mysql", "ndb_index_stat_sample", true, false) {}

bool Ndb_index_stat_sample_table::define_table_ndb(NdbDictionary::Table &table,
                                                   unsigned) const {
  {
    NdbDictionary::Column col_index_id("index_id");
    col_index_id.setType(NdbDictionary::Column::Unsigned);
    col_index_id.setPrimaryKey(true);
    if (!define_table_add_column(table, col_index_id)) return false;
  }

  {
    NdbDictionary::Column col_index_version("index_version");
    col_index_version.setType(NdbDictionary::Column::Unsigned);
    col_index_version.setPrimaryKey(true);
    if (!define_table_add_column(table, col_index_version)) return false;
  }

  {
    NdbDictionary::Column col_sample_version("sample_version");
    col_sample_version.setType(NdbDictionary::Column::Unsigned);
    col_sample_version.setPrimaryKey(true);
    if (!define_table_add_column(table, col_sample_version)) return false;
  }

  {
    NdbDictionary::Column col_stat_key("stat_key");
    col_stat_key.setType(NdbDictionary::Column::Longvarbinary);
    col_stat_key.setPrimaryKey(true);
    col_stat_key.setLength(stat_key_length);
    if (!define_table_add_column(table, col_stat_key)) return false;
  }

  {
    NdbDictionary::Column col_stat_value("stat_value");
    col_stat_value.setType(NdbDictionary::Column::Longvarbinary);
    col_stat_value.setNullable(false);
    col_stat_value.setLength(stat_value_length);
    if (!define_table_add_column(table, col_stat_value)) return false;
  }

  return true;
}

bool Ndb_index_stat_sample_table::create_indexes(
    const NdbDictionary::Table &table) const {
  NdbDictionary::Index index("ndb_index_stat_sample_x1");
  index.setType(NdbDictionary::Index::OrderedIndex);
  index.setLogging(false);
  index.addColumnName("index_id");
  index.addColumnName("index_version");
  index.addColumnName("sample_version");
  return create_index(table, index);
}

bool Ndb_index_stat_sample_table::check_schema() const { return true; }

bool Ndb_index_stat_sample_table::need_upgrade() const { return false; }

std::string Ndb_index_stat_sample_table::define_table_dd() const {
  std::stringstream ss;
  ss << "CREATE TABLE " << db_name() << "." << table_name() << "(\n";
  ss << "index_id INT UNSIGNED NOT NULL,"
        "index_version INT UNSIGNED NOT NULL,"
        "sample_version INT UNSIGNED NOT NULL,"
        "stat_key VARBINARY(3056) NOT NULL,"
        "stat_value VARBINARY(2048) NOT NULL,"
        "PRIMARY KEY USING HASH (index_id, index_version, sample_version, "
        "stat_key),"
        "INDEX ndb_index_stat_sample_x1 (index_id, index_version, "
        "sample_version)"
     << ") ENGINE=ndbcluster CHARACTER SET latin1";
  return ss.str();
}
