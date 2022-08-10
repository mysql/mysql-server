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

#ifndef NDB_SCHEMA_DIST_TABLE_H
#define NDB_SCHEMA_DIST_TABLE_H

#include <string>

#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ndb_util_table.h"

// RAII style class for working with the schema distribution table in NDB
class Ndb_schema_dist_table : public Ndb_util_table {
  Ndb_schema_dist_table() = delete;
  Ndb_schema_dist_table(const Ndb_schema_dist_table &) = delete;

  bool define_table_ndb(NdbDictionary::Table &table,
                        unsigned mysql_version) const override;

  bool drop_events_in_NDB() const override;

  /**
    @brief Update the schema UUID in ndb_schema table.
    @param schema_uuid   The schema UUID string that needs to be written
                         into the NDB table.
    @return true on success
   */
  bool update_schema_uuid_in_NDB(const std::string &schema_uuid) const;

  static std::string old_ndb_schema_uuid;

 public:
  static const std::string DB_NAME;
  static const std::string TABLE_NAME;

  static const char *COL_DB;
  static const char *COL_NAME;
  static const char *COL_QUERY;
  static const char *COL_ID;
  static const char *COL_VERSION;

  Ndb_schema_dist_table(class Thd_ndb *);
  virtual ~Ndb_schema_dist_table();

  bool check_schema() const override;

  bool check_column_identifier_limit(const char *column_name,
                                     const std::string &identifier) const;

  bool need_upgrade() const override;

  std::string define_table_dd() const override;

  bool pre_upgrade() const override;
  bool post_install() const override;

  /**
     @brief Return number of bytes possible to store in the "slock" column

     @return number of bytes
  */
  int get_slock_bytes() const;

  /**
     @brief Check if the table has been upgraded with schema_op_id column
     @return  true if table have the schema_op_id column
   */
  bool have_schema_op_id_column() const;

  /**
    @brief Retrieve the schema UUID from the ndb_schema table in NDB.
    @param[out] schema_uuid schema UUID retrieved from the table in NDB
    @return true on success
   */
  bool get_schema_uuid(std::string *schema_uuid) const;
};

#endif
