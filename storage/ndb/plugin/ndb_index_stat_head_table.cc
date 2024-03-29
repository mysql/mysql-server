/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_index_stat_head_table.h"

#include <sstream>

#include <ndbapi/NdbDictionary.hpp>

/*
  The Ndb_index_stat_head_table class is used to create the ndb_index_stat_head
  system table. The table will be hidden in the MySQL Data Dictionary in a
  similar manner to other system utility tables.

  The table definition details have been extracted from similar code implemented
  in the NdbIndexStat class which is part of NdbApi. The table definition must
  remain the same regardless of the mechanism used to create it.
*/

Ndb_index_stat_head_table::Ndb_index_stat_head_table(Thd_ndb *thd_ndb)
    : Ndb_util_table(thd_ndb, "mysql", "ndb_index_stat_head", true, false) {}

bool Ndb_index_stat_head_table::define_table_ndb(NdbDictionary::Table &table,
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
    NdbDictionary::Column col_table_id("table_id");
    col_table_id.setType(NdbDictionary::Column::Unsigned);
    col_table_id.setNullable(false);
    if (!define_table_add_column(table, col_table_id)) return false;
  }

  {
    NdbDictionary::Column col_frag_count("frag_count");
    col_frag_count.setType(NdbDictionary::Column::Unsigned);
    col_frag_count.setNullable(false);
    if (!define_table_add_column(table, col_frag_count)) return false;
  }

  {
    NdbDictionary::Column col_value_format("value_format");
    col_value_format.setType(NdbDictionary::Column::Unsigned);
    col_value_format.setNullable(false);
    if (!define_table_add_column(table, col_value_format)) return false;
  }

  {
    NdbDictionary::Column col_sample_version("sample_version");
    col_sample_version.setType(NdbDictionary::Column::Unsigned);
    col_sample_version.setNullable(false);
    if (!define_table_add_column(table, col_sample_version)) return false;
  }

  {
    NdbDictionary::Column col_load_time("load_time");
    col_load_time.setType(NdbDictionary::Column::Unsigned);
    col_load_time.setNullable(false);
    if (!define_table_add_column(table, col_load_time)) return false;
  }

  {
    NdbDictionary::Column col_sample_count("sample_count");
    col_sample_count.setType(NdbDictionary::Column::Unsigned);
    col_sample_count.setNullable(false);
    if (!define_table_add_column(table, col_sample_count)) return false;
  }

  {
    NdbDictionary::Column col_key_bytes("key_bytes");
    col_key_bytes.setType(NdbDictionary::Column::Unsigned);
    col_key_bytes.setNullable(false);
    if (!define_table_add_column(table, col_key_bytes)) return false;
  }

  return true;
}

bool Ndb_index_stat_head_table::check_schema() const { return true; }

bool Ndb_index_stat_head_table::need_upgrade() const { return false; }

std::string Ndb_index_stat_head_table::define_table_dd() const {
  std::stringstream ss;
  ss << "CREATE TABLE " << db_name() << "." << table_name() << "(\n";
  ss << "index_id INT UNSIGNED NOT NULL,"
        "index_version INT UNSIGNED NOT NULL,"
        "table_id INT UNSIGNED NOT NULL,"
        "frag_count INT UNSIGNED NOT NULL,"
        "value_format INT UNSIGNED NOT NULL,"
        "sample_version INT UNSIGNED NOT NULL,"
        "load_time INT UNSIGNED NOT NULL,"
        "sample_count INT UNSIGNED NOT NULL,"
        "key_bytes INT UNSIGNED NOT NULL,"
        "PRIMARY KEY USING HASH (index_id, index_version)"
     << ") ENGINE=ndbcluster CHARACTER SET latin1";
  return ss.str();
}

bool Ndb_index_stat_head_table::create_events_in_NDB(
    const NdbDictionary::Table &table) const {
  NdbDictionary::Event event("ndb_index_stat_head_event", table);
  event.addTableEvent(NdbDictionary::Event::TE_INSERT);
  event.addTableEvent(NdbDictionary::Event::TE_DELETE);
  event.addTableEvent(NdbDictionary::Event::TE_UPDATE);
  for (int i = 0; i < table.getNoOfColumns(); i++) event.addEventColumn(i);
  event.setReport(NdbDictionary::Event::ER_UPDATED);
  return create_event_in_NDB(event);
}
