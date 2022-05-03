/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"  // NdbDictionary

// Forward Declarations
struct NdbError;
class NdbTransaction;
class THD;
class Thd_ndb;

class Ndb_dd_sync {
  THD *const m_thd;
  Thd_ndb *const m_thd_ndb;

  // Enum defining database ddl types
  enum Ndb_schema_ddl_type : unsigned short {
    SCHEMA_DDL_CREATE = 0,
    SCHEMA_DDL_ALTER = 1,
    SCHEMA_DDL_DROP = 2
  };

  // A tuple to hold the values read from ndb_schema table
  using Ndb_schema_tuple =
      std::tuple<std::string,         /* db name */
                 std::string,         /* query */
                 Ndb_schema_ddl_type, /* database ddl type */
                 unsigned int,        /* id */
                 unsigned int>;       /* version */

  /*
    @brief Removes the table definition from the DD

    @param schema_name  Name of the schema
    @param table_name   Name of the table

    @return true on success, false on error
  */
  bool remove_table(const char *schema_name, const char *table_name) const;

  /*
    @brief Logs the error returned by NDB

    @param ndb_error  Error information including code and message

    @return void
  */
  void log_NDB_error(const NdbError &ndb_error) const;

  /*
    @brief Installs a logfile group in the DD

    @param logfile_group_name  Name of the logfile group
    @param ndb_lfg             NDB Dictionary definition of the logfile group
    @param undofile_names      Names of undofiles assigned to the logfile group
    @param force_overwrite     Controls whether existing definitions of the
                               logfile group (if any) should be overwritten

    @return true on success, false on error
  */
  bool install_logfile_group(const char *logfile_group_name,
                             NdbDictionary::LogfileGroup ndb_lfg,
                             const std::vector<std::string> &undofile_names,
                             bool force_overwrite) const;

  /*
    @brief Compares if files retrieved from NDB Dictionary match those retrieved
           from DD. Used to check undofiles assigned to logfile groups and
           datafiles assigned to tablespaces

    @param file_names_in_NDB  File names retrieved from NDB
    @param file_names_in_DD   File names retrieved from DD

    @return true on success, false on error
  */
  bool compare_file_list(
      const std::vector<std::string> &file_names_in_NDB,
      const std::vector<std::string> &file_names_in_DD) const;

  /*
    @brief Synchronizes a logfile group between NDB Dictionary and DD

    @param logfile_group_name  Name of the logfile group
    @param lfg_in_DD           Unordered set consisting of the names of all the
                               logfile groups present in DD

    @return true on success, false on error
  */
  bool synchronize_logfile_group(
      const char *logfile_group_name,
      const std::unordered_set<std::string> &lfg_in_DD) const;

  /*
    @brief Synchronizes logfile groups between NDB Dictionary and DD. This
           makes the logfile group definitions in the DD match that of the NDB
           Dictionary

    @return true on success, false on error
  */
  bool synchronize_logfile_groups() const;

  /*
    @brief Installs a tablespace in the DD

    @param tablespace_name  Name of the tablespace
    @param ndb_tablespace   NDB Dictionary definition of the tablespace
    @param data_file_names  Names of data files assigned to the tablespace
    @param force_overwrite  Controls whether existing definitions of the
                            tablespace (if any) should be overwritten

    @return true on success, false on error
  */
  bool install_tablespace(const char *tablespace_name,
                          NdbDictionary::Tablespace ndb_tablespace,
                          const std::vector<std::string> &data_file_names,
                          bool force_overwrite) const;

  /*
    @brief Synchronizes a tablespace between NDB Dictionary and DD

    @param tablespace_name    Name of the tablespace
    @param tablespaces_in_DD  Unordered set consisting of the names of all the
                              tablespaces present in DD

    @return true on success, false on error
  */
  bool synchronize_tablespace(
      const char *tablespace_name,
      const std::unordered_set<std::string> &tablespaces_in_DD) const;

  /*
    @brief Synchronizes tablespaces between NDB Dictionary and DD. This makes
           the tablespace definitions in the DD match that of the NDB Dictionary

    @return true on success, false on error
  */
  bool synchronize_tablespaces() const;

  /*
    @brief Retrieves all the database DDLs from the mysql.ndb_schema table

    @note This function is designed to be called through ndb_trans_retry()

    @param ndb_transaction     NdbTransaction object to perform the read
    @param ndb_schema_tab      Pointer to ndb_schema table's
                               NdbDictionary::Table object
    @param[out] database_ddls  Vector of Ndb_schema_tuple consisting DDLs read
                               from ndb_schema table

   @return NdbError On failure
           nullptr  On success
  */
  static const NdbError *fetch_database_ddls(
      NdbTransaction *ndb_transaction,
      const NdbDictionary::Table *ndb_schema_tab,
      std::vector<Ndb_schema_tuple> *database_ddls);

  /*
    @brief Restores the correct state with respect to created databases using
           the information in the mysql.ndb_schema table, NDB Dictionary and DD

    @return true on success, false on error
  */
  bool synchronize_databases() const;

  /*
    @brief Migrates a table that has old metadata to the DD

    @param schema_name      Name of the schema
    @param table_name       Name of the table
    @param unpacked_data    Metadata of the table
    @param unpacked_len     Length of metadata
    @param force_overwrite  Controls whether existing definitions of the
                            table (if any) should be overwritten

    @return true on success, false on error
  */
  bool migrate_table_with_old_extra_metadata(const char *schema_name,
                                             const char *table_name,
                                             void *unpacked_data,
                                             Uint32 unpacked_len,
                                             bool force_overwrite) const;

  /*
    @brief Installs a table to the DD

    @param schema_name      Name of the schema
    @param table_name       Name of the table
    @param ndbtab           NDB Dictionary definition of the table
    @param force_overwrite  Controls whether existing definitions of the
                            table (if any) should be overwritten

    @return true on success, false on error
  */
  bool install_table(const char *schema_name, const char *table_name,
                     const NdbDictionary::Table *ndbtab,
                     bool force_overwrite = false) const;

  /*
    @brief Synchronizes a table between NDB Dictionary and DD

    @param schema_name  Name of the schema
    @param table_name   Name of the table

    @return true on success, false on error
  */
  bool synchronize_table(const char *schema_name, const char *table_name) const;

  /*
    @brief Synchronizes tables in a schema between NDB Dictionary and DD. This
           makes the table definitions in the DD match that of the
           NDB Dictionary

    @return true on success, false on error
  */
  bool synchronize_schema(const char *schema_name) const;

  /*
   * @brief Iterate over all temporary tables in NDB, process them by the name:
   *          - the temporary tables, whose names start with prefix #sql2, will
   *          produce error, because original data has been renamed and cannot
   *          be accessed by user, this tables will be kept to prevent any data
   *          loss,
   *          - the temporary tables, whose names start with prefix #sql (and
   *          not #sql2), will be deleted, they ware created as a copy of
   *          original data, which should exists in the NDB under its original
   *          name or as temporary table prefixed #sql2.
   *
   * @parm schema_name                  Name of the schema
   * @param temp_tables_in_ndb          set of temporary tables in NDB
   * @return void
   */
  void remove_copying_alter_temp_tables(
      const char *schema_name,
      const std::unordered_set<std::string> &temp_tables_in_ndb) const;

 public:
  Ndb_dd_sync(THD *thd, Thd_ndb *thd_ndb) : m_thd(thd), m_thd_ndb(thd_ndb) {}
  Ndb_dd_sync(const Ndb_dd_sync &) = delete;

  /*
    @brief Removes all NDB metadata from the DD. Designed to be called after an
           initial start/restart

    @return true on success, false otherwise
  */
  bool remove_all_metadata() const;

  /*
    @brief Removes all deleted NDB tables from DD by comparing them against
           a list of tables in NDB

    @return true on success, false otherwise
  */
  bool remove_deleted_tables() const;

  /*
    @brief Synchronizes all NDB related content in the DD to match that of the
           NDB Dictionary. This includes logfile groups, tablespaces, schemas,
           and tables. It also sets up subscription to changes that happen in
           NDB

    @return true on success, false otherwise
  */
  bool synchronize() const;
};
