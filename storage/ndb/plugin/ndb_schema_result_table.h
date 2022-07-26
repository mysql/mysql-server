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

#ifndef NDB_SCHEMA_RESULT_TABLE_H
#define NDB_SCHEMA_RESULT_TABLE_H

#include <string>

#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ndb_util_table.h"

// RAII style class for working with the schema result table in NDB
class Ndb_schema_result_table : public Ndb_util_table {
  Ndb_schema_result_table() = delete;
  Ndb_schema_result_table(const Ndb_schema_result_table &) = delete;

  bool define_table_ndb(NdbDictionary::Table &table,
                        unsigned mysql_version) const override;

  bool drop_events_in_NDB() const override;

 public:
  static const std::string DB_NAME;
  static const std::string TABLE_NAME;

  static const char *COL_NODEID;
  static const char *COL_SCHEMA_OP_ID;
  static const char *COL_PARTICIPANT_NODEID;
  static const char *COL_RESULT;
  static const char *COL_MESSAGE;

  Ndb_schema_result_table(class Thd_ndb *);
  virtual ~Ndb_schema_result_table();

  bool check_schema() const override;

  bool need_upgrade() const override;

  std::string define_table_dd() const override;

  void pack_message(const char *message, char *buf);

  std::string unpack_message(const std::string &packed_message);
};

#endif
