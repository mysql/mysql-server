/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_SYNC_EXCLUDED_OBJECTS_TABLE_H
#define NDB_SYNC_EXCLUDED_OBJECTS_TABLE_H

#include <string>
#include <vector>

#include "storage/ndb/plugin/ndb_pfs_table.h"  // Ndb_pfs_table

class Ndb_sync_excluded_objects_table_share : public Ndb_pfs_table_share {
 public:
  Ndb_sync_excluded_objects_table_share();
  Ndb_sync_excluded_objects_table_share(
      const Ndb_sync_excluded_objects_table_share &) = delete;
};

class Ndb_sync_excluded_objects_table : public Ndb_pfs_table {
  struct Excluded_object {
    std::string
        m_schema_name;   // Schema name, "" for logfile groups & tablespaces
    std::string m_name;  // Object name, "" for schema objects
    int m_type;
    std::string m_reason;  // Reason for exclusion

    Excluded_object(std::string schema_name, std::string name, int type,
                    std::string reason)
        : m_schema_name{std::move(schema_name)},
          m_name{std::move(name)},
          m_type{type},
          m_reason{std::move(reason)} {}
  };
  std::vector<Excluded_object> m_excluded_objects;

 public:
  Ndb_sync_excluded_objects_table() = default;
  Ndb_sync_excluded_objects_table(const Ndb_sync_excluded_objects_table &) =
      delete;

  /*
    @brief Add an object to the back of the object vector

    @param schema_name  Schema the object belongs to
    @param name         Name of the object
    @param type         Type of the object
    @param reason       Reason for exclusion of the object

    @return void
  */
  void add_excluded_object(const std::string &schema_name,
                           const std::string &name, int type,
                           const std::string &reason) {
    m_excluded_objects.emplace_back(schema_name, name, type, reason);
  }

  /*
    @brief Read column at index of current row. Implementation
           is specific to table

    @param field [out]  Field	column value
    @param index [in]	  Index	column position within row

    @return 0 on success, plugin table error code on failure
  */
  int read_column_value(PSI_field *field, unsigned int index) override;

  /*
    @brief Initialize the table

    @return 0 on success, plugin table error code on failure
  */
  int rnd_init() override;

  /*
    @brief Close the table

    @return void
  */
  void close() override;
};

#endif
