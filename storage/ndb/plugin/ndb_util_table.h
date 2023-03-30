/*
   Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
namespace dd {
class Table;
}

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

 protected:
  const NdbDictionary::Column *get_column_by_number(Uint32 number) const;
  const NdbDictionary::Column *get_column(const char *name) const;
  void push_warning(const char *fmt, ...) const
      MY_ATTRIBUTE((format(printf, 2, 3)));
  void push_ndb_error_warning(const NdbError &ndb_err) const;

  Ndb_util_table(Thd_ndb *, std::string db_name, std::string table_name,
                 bool hidden, bool create_events = true);
  ~Ndb_util_table();

  const class THD *get_thd() const;
  Ndb *get_ndb() const;

  bool check_column_exist(const char *name) const;

  bool check_column_varbinary(const char *name) const;
  bool check_column_varchar(const char *name) const;
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
     a specific MySQL version. This is primarily used for testing of upgrade.
     @return true if definition was filled without problem
   */
  virtual bool define_table_ndb(NdbDictionary::Table &table,
                                unsigned mysql_version) const = 0;
  bool define_table_add_column(NdbDictionary::Table &new_table,
                               const NdbDictionary::Column &new_column) const;

  bool create_table_in_NDB(const NdbDictionary::Table &new_table) const;
  bool drop_table_in_NDB(const NdbDictionary::Table &old_table) const;

  virtual bool create_indexes(const NdbDictionary::Table &new_table) const;
  bool create_index(const NdbDictionary::Table &new_table,
                    const NdbDictionary::Index &new_index) const;
  bool create_primary_ordered_index(
      const NdbDictionary::Table &new_table) const;

  virtual bool create_events_in_NDB(const NdbDictionary::Table &new_table
                                    [[maybe_unused]]) const {
    return true;
  }
  bool create_event_in_NDB(const NdbDictionary::Event &new_event) const;

  /**
    @brief Code to be executed before upgrading the table.

    @note  The derived class has to override this method if it wants to
           execute code before upgrading the table.

    @return true on success.
   */
  virtual bool pre_upgrade() { return true; }

  /**
    @brief Code to be executed after installing the table in NDB.

    @note  The derived class has to override this method if it wants to
           execute code after installing the table.

    @return true on success.
   */
  virtual bool post_install() const { return true; }

  /**
    @brief Code to be executed after installing the table in the data
    dictionary.

    @note The derived class has to override this method to execute additional
    code.

    @return true on success, false otherwise
  */
  virtual bool post_install_in_DD() const { return true; }

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
  */
  void pack_varbinary(const char *column_name, const char *src,
                      char *dst) const;

  /**
     @brief Unpack the string
     @note Table definition must be loaded with open() before this function is
           called
     @param column_name  Column name
     @param packed_str   String to be unpacked
     @return Unpacked string which is empty on failure
  */
  std::string unpack_varbinary(const char *column_name,
                               const char *packed_str) const;

  /**
     @brief Pack the string to be written to a column of a util table
     @note Table definition must be loaded with open() before this function is
           called
     @param column_name  Column name
     @param src          String to be packed
     @param dst [out]    Packed string
  */
  void pack_varchar(const char *column_name, std::string_view src,
                    char *dst) const;

  /**
     @brief Pack the string to be written to a column of a util table
     @note Table definition must be loaded with open() before this function is
           called
     @param column_number Column number
     @param src          String to be packed
     @param dst [out]    Packed string
  */
  void pack_varchar(Uint32 column_number, std::string_view src,
                    char *dst) const;

  /**
     @brief Unpack a non nullable blob column
     @param ndb_blob_handle     The NDB Blob handle
     @param blob_value [out]    Extracted string
     @return true if successful, false if not
   */
  static bool unpack_blob_not_null(NdbBlob *ndb_blob_handle,
                                   std::string *blob_value);

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
    @param reload_table  Boolean flag. Reload the table definition if true.
    @return true if table definition could be opened
   */
  bool open(bool reload_table = false);

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
     @brief Create table in NDB and open it
     @return true if table was created successfully
   */
  bool create(bool is_upgrade = false);

  /**
     @brief Create table in DD and finalize it
     @return true if successful, false otherwise
  */
  bool create_in_DD();

  /**
     @brief Check if table need to be upgraded
     @return true if upgrade is needed
   */
  virtual bool need_upgrade() const = 0;

  /**
     @brief Upgrade table in NDB and open it
     @return true if table was upgraded successfully
   */
  bool upgrade();

  /**
     @brief Check if table need to be reinstalled in DD.
     This mechanism can be used to rewrite the table definition in DD without
     changing the physical table in NDB.
     @return true if reinstall is needed
   */
  virtual bool need_reinstall(const dd::Table *) const { return false; }

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

  /**
     @brief Return the id of the given column within the table
     @return the column number
   */
  int get_column_num(const char *col_name) const;

  /**
     @brief Delete all util table rows.
     @return true on success. false otherwise.
 */
  bool delete_all_rows();
};

#endif
