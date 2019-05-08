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

// Implements the interface defined in
#include "sql/ndb_util_table.h"

#include "sql/ndb_thd_ndb.h"

class Db_name_guard {
  Ndb * const m_ndb;
  const std::string m_save_old_dbname;
  Db_name_guard() = delete;
  Db_name_guard(const Db_name_guard &) = delete;

 public:
  Db_name_guard(Ndb *ndb, const std::string dbname)
      : m_ndb(ndb), m_save_old_dbname(ndb->getDatabaseName()) {
    m_ndb->setDatabaseName(dbname.c_str());
  }

  ~Db_name_guard() {
    // Restore old dbname
    m_ndb->setDatabaseName(m_save_old_dbname.c_str());
  }
};

Ndb_util_table::Ndb_util_table(Thd_ndb* thd_ndb, const char* db_name,
                               const char* table_name, bool hidden)
    : m_thd_ndb(thd_ndb),
      m_table_guard(thd_ndb->ndb->getDictionary()),
      m_db_name(db_name),
      m_table_name(table_name),
      m_hidden(hidden) {}

Ndb_util_table::~Ndb_util_table() {}

void Ndb_util_table::push_warning(const char* fmt, ...) const {
  // Assemble the message
  char message[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  m_thd_ndb->push_warning("[%s.%s] %s", m_db_name.c_str(), m_table_name.c_str(),
                          message);
}

void Ndb_util_table::push_ndb_error_warning(const NdbError &ndb_err) const {
  push_warning("NDB error: %d %s", ndb_err.code, ndb_err.message);
}

bool Ndb_util_table::exists() const {
  Ndb *ndb = m_thd_ndb->ndb;

  // Set correct database name on the Ndb object
  Db_name_guard db_guard(ndb, m_db_name.c_str());

  // Load up the table definition from NDB dictionary
  Ndb_table_guard ndb_tab(ndb->getDictionary(), m_table_name.c_str());

  if (ndb_tab.get_table() == nullptr) {
    // Table does not exist in NDB
    return false;
  }

  // Table exists in NDB
  return true;
}

bool Ndb_util_table::open() {
  Ndb* ndb = m_thd_ndb->ndb;

  // Set correct database name on the Ndb object
  Db_name_guard db_guard(ndb, m_db_name.c_str());

  // Load up the table definition from NDB dictionary
  m_table_guard.init(m_table_name.c_str());

  const NdbDictionary::Table* tab = m_table_guard.get_table();
  if (!tab) {
    push_warning("Failed to open table from NDB");
    return false;
  }

  return true;
}

const NdbDictionary::Table* Ndb_util_table::get_table() const {
  return m_table_guard.get_table();
}

const NdbDictionary::Column* Ndb_util_table::get_column(
    const char* name) const {
  return get_table()->getColumn(name);
}


bool Ndb_util_table::check_column_exist(const char* name) const {
  if (get_column(name) == nullptr) {
    push_warning("Could not find expected column '%s'", name);
    return false;
  }
  return true;
}

bool Ndb_util_table::check_primary_key(
    const std::vector<const char*> columns) const {
  // Check that the primary key of the table matches the given columns
  int keys = 0;
  for (const char* name : columns) {
    if (!get_column(name)->getPrimaryKey())
    {
      push_warning("Column '%s' is not part of primary key", name);
      return false;
    }
    keys++;
  }
  if (keys != get_table()->getNoOfPrimaryKeys())
  {
    push_warning("Invalid primary key");
    return false;
  }
  return true;
}

int Ndb_util_table::get_column_max_length(const char* name) const
{
  return get_column(name)->getLength();
}

bool Ndb_util_table::check_column_type(const NdbDictionary::Column* col,
                                       NdbDictionary::Column::Type type,
                                       const char* type_name) const {
  if (col->getType() != type) {
    push_warning("Column '%s' must be defined as '%s'", col->getName(),
                 type_name);
    return false;
  }
  return true;
}

bool Ndb_util_table::check_column_minlength(const char* name,
                                            int min_length) const {
  if (get_column(name)->getLength() < min_length) {
    push_warning("Column '%s' is too short, need at least %d bytes", name,
                 min_length);
    return false;
  }
  return true;
}

bool Ndb_util_table::check_column_varbinary(const char* name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Varbinary,
                           "VARBINARY");
}

bool Ndb_util_table::check_column_binary(const char* name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Binary,
                           "BINARY");
}

bool Ndb_util_table::check_column_unsigned(const char* name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Unsigned,
                           "INT UNSIGNED ");
}

bool Ndb_util_table::check_column_bigunsigned(const char* name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Bigunsigned,
                           "BIGINT UNSIGNED");
}

bool Ndb_util_table::check_column_blob(const char* name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Blob,
                           "BLOB");
}

bool Ndb_util_table::define_table_add_column(
    NdbDictionary::Table &new_table,
    const NdbDictionary::Column &new_column) const {
  if (new_table.addColumn(new_column) != 0) {
    push_warning("Failed to add column '%s'", new_column.getName());
    return false;
  }
  return true;
}

bool Ndb_util_table::create_table_in_NDB(
    NdbDictionary::Table &new_table) const {

  // Set correct database name on the Ndb object
  Db_name_guard db_guard(m_thd_ndb->ndb, m_db_name.c_str());

  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  if (dict->createTable(new_table) != 0) {
    push_ndb_error_warning(dict->getNdbError());;
    push_warning("Failed to create table '%s'", new_table.getName());
    return false;
  }
  return true;
}

bool Ndb_util_table::drop_table_in_NDB(
    const NdbDictionary::Table &old_table) const {
  // Set correct database name on the Ndb object
  Db_name_guard db_guard(m_thd_ndb->ndb, m_db_name.c_str());

  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  if (dict->dropTableGlobal(old_table) != 0) {
    push_ndb_error_warning(dict->getNdbError());
    push_warning("Failed to drop table '%s'", old_table.getName());
    return false;
  }
  return true;
}


bool Ndb_util_table::create() const {
  NdbDictionary::Table new_table(m_table_name.c_str());

  const unsigned mysql_version = MYSQL_VERSION_ID;
  if (!define_table_ndb(new_table, mysql_version))
    return false;

  if (!create_table_in_NDB(new_table))
    return false;

  return true;
}


// Upgrade table
bool Ndb_util_table::upgrade() const {
  NdbDictionary::Table new_table(m_table_name.c_str());
  if (!define_table_ndb(new_table, MYSQL_VERSION_ID)) {
    return false;
  }

  const NdbDictionary::Table *old_table = get_table();

  // Could copy stuff from old to new table if necessary...

  // Drop the old table
  if (!drop_table_in_NDB(*old_table)) {
    return false;
  }

  // Create new table
  if (!create_table_in_NDB(new_table)) {
    return false;
  }

  return true;
}
