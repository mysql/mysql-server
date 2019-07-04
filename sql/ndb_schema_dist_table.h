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

#ifndef NDB_SCHEMA_DIST_TABLE_H
#define NDB_SCHEMA_DIST_TABLE_H

#include <string>
#include "sql/ndb_util_table.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"

// RAII style class for working with the schema distribution table in NDB
class Ndb_schema_dist_table : public Ndb_util_table {
  Ndb_schema_dist_table() = delete;
  Ndb_schema_dist_table(const Ndb_schema_dist_table&) = delete;

  bool define_table_ndb(NdbDictionary::Table &table,
                        unsigned mysql_version) const override;

 public:
  static const char* COL_DB;
  static const char* COL_NAME;

  Ndb_schema_dist_table(class Thd_ndb*);
  virtual ~Ndb_schema_dist_table();

  bool check_schema() const override;

  bool check_column_identifier_limit(const char* column_name,
                                     const std::string& identifier) const;

  bool need_upgrade() const override;

  std::string define_table_dd() const override;

  /**
     @brief Return number of bits possible to store in the "slock" column

     @return number of bits
  */
  int get_slock_bits() const;
};

#endif
