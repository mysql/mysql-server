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

#ifndef NDB_UTIL_TABLE_H
#define NDB_UTIL_TABLE_H

#include <string>
#include <vector>

#include "my_compiler.h"
#include "ndb_table_guard.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"

// Base class used for working with tables created in NDB by the
// ndbcluster application
class Ndb_util_table {
  class Thd_ndb* const m_thd_ndb;
  Ndb_table_guard m_table_guard;
  std::string m_db_name;
  std::string m_table_name;

  const NdbDictionary::Table* get_table() const;
  const NdbDictionary::Column* get_column(const char* name) const;
  bool check_column_type(const NdbDictionary::Column*,
                         NdbDictionary::Column::Type type,
                         const char* type_name) const;

 protected:
  void push_warning(const char* fmt, ...) const
      MY_ATTRIBUTE((format(printf, 2, 3)));

  Ndb_util_table(class Thd_ndb*, const char* db_name, const char* table_name);
  ~Ndb_util_table();

  bool check_column_exist(const char* name) const;

  bool check_column_varbinary(const char* name) const;
  bool check_column_binary(const char* name) const;
  bool check_column_unsigned(const char* name) const;
  bool check_column_bigunsigned(const char* name) const;
  bool check_column_blob(const char* name) const;

  bool check_column_minlength(const char* name, int min_length) const;

  bool check_primary_key(const std::vector<const char*> columns) const;

 public:
  bool open();
};

#endif
