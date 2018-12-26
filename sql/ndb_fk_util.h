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

#ifndef NDB_FK_UTIL_H
#define NDB_FK_UTIL_H

#include <set>
#include <string>
#include "storage/ndb/include/ndbapi/NdbApi.hpp"

// Database name guard for Ndb objects
struct Ndb_db_guard
{
  Ndb_db_guard(class Ndb* ndb) {
    this->ndb = ndb;
    save_db= ndb->getDatabaseName();
  }

  void restore() {
    ndb->setDatabaseName(save_db.c_str());
  }

  ~Ndb_db_guard() {
    ndb->setDatabaseName(save_db.c_str());
  }
private:
  Ndb* ndb;
  std::string save_db;
};

const char* fk_split_name(char dst[], const char * src, bool index= false);
bool fetch_referenced_tables_from_ndb_dictionary(
    class THD* thd, const char* schema_name, const char* table_name,
    std::set<std::pair<std::string, std::string>> &referenced_tables);

#endif
