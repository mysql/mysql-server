/*
   Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ndb_table_guard.h"

class NdbRecAttr;
class Thd_ndb;

// Base class used for working with tables created in NDB by the
// ndbcluster plugin
class Ndb_util_table {
  Thd_ndb *const m_thd_ndb;
  Ndb_table_guard m_table_guard;
  const std::string m_db_name;
  const std::string m_table_name;
  const bool m_hidden;
  const bool m_create_events;

  bool check_column_type(const NdbDictionary::Column *,
                         NdbDictionary::Column::Type type,
                         const char *type_name) const;

  void push_ndb_error_warning(const NdbError &ndb_err) const;

 protected:
  const NdbDictionary::Column *get_column(const char *name) const;
  void push_warning(const char *fmt, ...) const
      MY_ATTRIBUTE((format(printf, 2, 3)));

  Ndb_util_table(Thd_ndb *, std::string db_name, std::string table_name,
                 bool hidden, bool create_events = true);
  ~Ndb_util_table();

  bool check_column_exist(const char *name) const;

  bool check_column_varbinary(const char *name) const;
  bool check_column_binary(const char *name) const;
  bool check_column_unsigned(const char *name) const;
  bool check_column_bigunsigned(const char *name) const;
  bool check_column_blob(const char *name) const;

  bool check_column_nullable(const char *name, bool nullable) const;

  bool check_column_minlength(const char *name, int min_length) const;

  bool check_primary_key(const std::vector<const char *> columns) const;

  int get_column_max_length(const char *name) const;

  /**
     @brief Define the NdbApi table definition
     @param table NdbApi table to populate
     @param mysql_version Force the table to be defined as it looked in
     a specifc MySQL version. This is primarily used for testing of upgrade.
     @return true if definition was filled without problem
   */
  virtual bool define_table_ndb(NdbDictionary::Table &table,
                                unsigned mysql_version) const = 0;
  bool define_table_add_column(NdbDictionary::Table &new_table,
                               const NdbDictionary::Column &new_column) const;

  bool create_table_in_NDB(const NdbDictionary::Table &new_table) const;
  bool drop_table_in_NDB(const NdbDictionary::Table &old_table) const;

  virtual bool define_indexes(const NdbDictionary::Table &table,
                              unsigned int mysql_version) const;
  bool create_index(const NdbDictionary::Table &,
                    const NdbDictionary::Index &) const;
  bool create_primary_ordered_index(const NdbDictionary::Table &) const;

  /**
     @brief Drop the events related to this table from NDB
     @return true if events was dropped successfully
  */
  virtual bool drop_events_in_NDB() const = 0;

  /**
    @brief Drop one event from NDB
    @return true if events was dropped successfully
    */
  bool drop_event_in_NDB(const char *event_name) const;

  /**
     @brief Pack the string to be written to a column of a util table
     @note Table definition must be loaded with open() before this function is
           called
     @param column_name  Column name
     @param src          String to be packed
     @param dst [out]    Packed string
     @return true if successful, false if not
  */
  bool pack_varbinary(const char *column_name, const char *src, char *dst);

  /**
     @brief Unpack the string
     @note Table definition must be loaded with open() before this function is
           called
     @param column_name  Column name
     @param packed_str   String to be unpacked
     @return Unpacked string which is empty on failure
  */
  std::string unpack_varbinary(const char *column_name, const char *packed_str);

 public:
  /**
    @brief Create or upgrade the table in NDB, and in the local Data Dictionary,
           and setup NDB binlog events if enabled
    @return true on success
   */
  bool create_or_upgrade(class THD *, bool upgrade_allowed);

  /**
    @brief Check if table exists in NDB
    @return true if table exists
   */
  bool exists() const;

  /**
    @brief Open the table definition from NDB
    @return true if table definition could be opened
   */
  bool open();

  /**
     @brief Check if actual table definition in NDB matches the expected.
     @note This function may return true as long as the table supports minimal
     functionality, the caller still has to check further before using
     functionality which does not exist after or during an upgrade.
     @return true if table definition fulfills minimal functionality.
   */
  virtual bool check_schema() const = 0;

  /**
    @brief Get name of the table
    @return name of the table
  */
  const char *table_name() const { return m_table_name.c_str(); }

  /**
    @brief Get database of the table
    @return database of the table
  */
  const char *db_name() const { return m_db_name.c_str(); }

  /**
    @brief Get hidden status of table
    @return true if table should be hidden
  */
  bool is_hidden() const { return m_hidden; }

  /**
    @brief Get the current NDB table definition
    @note table definition must first be loaded with open()
    @return pointer to table definition
   */
  const NdbDictionary::Table *get_table() const;

  /**
     @brief Create table in NDB
     @return true if table was created sucessfully
   */
  bool create() const;

  /**
     @brief Check if table need to be upgraded
     @return true if upgrade is needed
   */
  virtual bool need_upgrade() const = 0;

  /**
     @brief Upgrade table in NDB
     @return true if table was upgraded sucessfully
   */
  bool upgrade() const;

  /**
     @brief Create DDL for creating the table definition
     @return string with SQL
   */
  virtual std::string define_table_dd() const = 0;

  /**
     @brief Unpack the varbinary column value
     @return the value stored in the varbinary column
   */
  static std::string unpack_varbinary(NdbRecAttr *ndbRecAttr);
};

#endif
