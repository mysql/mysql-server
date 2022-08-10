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

#ifndef NDB_APPLY_STATUS_TABLE_H
#define NDB_APPLY_STATUS_TABLE_H

#include <string>

#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ndb_util_table.h"

// RAII style class for working with the apply status table in NDB
class Ndb_apply_status_table : public Ndb_util_table {
  Ndb_apply_status_table() = delete;
  Ndb_apply_status_table(const Ndb_apply_status_table &) = delete;

  bool define_table_ndb(NdbDictionary::Table &table,
                        unsigned mysql_version) const override;

  bool drop_events_in_NDB() const override;

 public:
  static const std::string DB_NAME;
  static const std::string TABLE_NAME;

  Ndb_apply_status_table(class Thd_ndb *);
  virtual ~Ndb_apply_status_table();

  bool check_schema() const override;

  bool need_upgrade() const override;

  bool need_reinstall(const dd::Table *table_def) const override;

  std::string define_table_dd() const override;

  /**
   * @brief Check if given name is the apply status table, special
            handling for the table is required in a few places.
     @param db database name
     @param table_name table name
     @return true if table is the apply status table
   */
  static bool is_apply_status_table(const char *db, const char *table_name);
};

#endif
