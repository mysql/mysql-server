/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_DD_CLIENT_H
#define NDB_DD_CLIENT_H

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "my_inttypes.h"
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/mdl.h"

namespace dd {
typedef String_type sdi_t;
namespace cache {
class Dictionary_client;
}
class Schema;
class Table;
class Tablespace;
}  // namespace dd

/*
 * Helper class to Ndb_dd_client to fetch and
 * invalidate tables referenced by foreign keys.
 * Used by the schema distribution participant
 */
class Ndb_referenced_tables_invalidator {
  std::set<std::pair<std::string, std::string>> m_referenced_tables;
  class THD *const m_thd;
  class Ndb_dd_client &m_dd_client;

  bool add_and_lock_referenced_table(const char *schema_name,
                                     const char *table_name);

 public:
  Ndb_referenced_tables_invalidator(class THD *thd,
                                    class Ndb_dd_client &dd_client)
      : m_thd(thd), m_dd_client(dd_client) {}
  bool fetch_referenced_tables_to_invalidate(const char *schema_name,
                                             const char *table_name,
                                             const dd::Table *table_def,
                                             bool skip_ndb_dict_fetch = false);
  bool invalidate() const;
};

/*
  Class encapculating the code for accessing the DD
  from ndbcluster

  Handles:
   - locking and releasing MDL(metadata locks)
   - disabling and restoring autocommit
   - transaction commit and rollback, will automatically
     rollback in case commit has not been called(unless
     auto rollback has been turned off)
*/

class Ndb_dd_client {
  class THD *const m_thd;
  dd::cache::Dictionary_client *m_client;
  void *m_auto_releaser;  // Opaque pointer
  // List of MDL locks taken in EXPLICIT scope by Ndb_dd_client
  std::vector<class MDL_ticket *> m_acquired_mdl_tickets;
  // MDL savepoint which allows releasing MDL locks taken by called
  // functions in TRANSACTIONAL and STATEMENT scope
  const MDL_savepoint m_save_mdl_locks;
  ulonglong m_save_option_bits{0};
  bool m_comitted{false};
  bool m_auto_rollback{true};

  void disable_autocommit();

  bool store_table(dd::Table *install_table, int ndb_table_id);

 public:
  Ndb_dd_client(class THD *thd);

  ~Ndb_dd_client();

  /**
    @brief Acquire IX MDL on the schema

    @param schema_name Schema name

    @return true if the MDL was acquired successfully, false if not
  */
  bool mdl_lock_schema(const char *schema_name);
  bool mdl_lock_schema_exclusive(const char *schema_name,
                                 bool custom_lock_wait = false,
                                 ulong lock_wait_timeout = 0);
  bool mdl_lock_table(const char *schema_name, const char *table_name);
  bool mdl_locks_acquire_exclusive(const char *schema_name,
                                   const char *table_name,
                                   bool custom_lock_wait = false,
                                   ulong lock_wait_timeout = 0);
  bool mdl_lock_logfile_group(const char *logfile_group_name,
                              bool intention_exclusive);
  bool mdl_lock_logfile_group_exclusive(const char *logfile_group_name,
                                        bool custom_lock_wait = false,
                                        ulong lock_wait_timeout = 0);
  bool mdl_lock_tablespace(const char *tablespace_name,
                           bool intention_exclusive);
  bool mdl_lock_tablespace_exclusive(const char *tablespace_name,
                                     bool custom_lock_wait = false,
                                     ulong lock_wait_timeout = 0);
  void mdl_locks_release();

  // Transaction handling functions
  void commit();
  void rollback();

  /*
    @brief Turn off automatic rollback which otherwise occurs automatically
           when Ndb_dd_client instance goes out of scope and no commit has
           been called
           This is useful when running as part of a higher level DDL command
           which manages the transaction
  */
  void disable_auto_rollback() { m_auto_rollback = false; }

  bool get_engine(const char *schema_name, const char *table_name,
                  dd::String_type *engine);

  bool rename_table(const char *old_schema_name, const char *old_table_name,
                    const char *new_schema_name, const char *new_table_name,
                    int new_table_id, int new_table_version,
                    Ndb_referenced_tables_invalidator *invalidator = nullptr);
  bool remove_table(const dd::Table *table, const char *schema_name,
                    const char *table_name,
                    Ndb_referenced_tables_invalidator *invalidator = nullptr);
  bool remove_table(const char *schema_name, const char *table_name,
                    Ndb_referenced_tables_invalidator *invalidator = nullptr);
  bool remove_table(const dd::Object_id spi,
                    Ndb_referenced_tables_invalidator *invalidator = nullptr);
  bool deserialize_table(const dd::sdi_t &sdi, dd::Table *table_def);
  bool install_table(const char *schema_name, const char *table_name,
                     const dd::sdi_t &sdi, int ndb_table_id,
                     int ndb_table_version, size_t ndb_num_partitions,
                     const std::string &tablespace_name, bool force_overwrite,
                     Ndb_referenced_tables_invalidator *invalidator = nullptr);
  bool migrate_table(const char *schema_name, const char *table_name,
                     const unsigned char *frm_data, unsigned int unpacked_len,
                     bool force_overwrite);
  bool get_table(const char *schema_name, const char *table_name,
                 const dd::Table **table_def);
  bool table_exists(const char *schema_name, const char *table_name,
                    bool &exists);
  bool set_tablespace_id_in_table(const char *schema_name,
                                  const char *table_name,
                                  dd::Object_id tablespace_id);
  bool set_object_id_and_version_in_table(const char *schema_name,
                                          const char *table_name, int object_id,
                                          int object_version);
  bool store_table(dd::Table *install_table) const;

  bool fetch_all_schemas(std::map<std::string, const dd::Schema *> &);
  bool fetch_schema_names(std::vector<std::string> *);
  bool get_ndb_table_names_in_schema(const char *schema_name,
                                     std::unordered_set<std::string> *names);
  bool get_table_names_in_schema(const char *schema_name,
                                 std::unordered_set<std::string> *ndb_tables,
                                 std::unordered_set<std::string> *local_tables);
  bool have_local_tables_in_schema(const char *schema_name,
                                   bool *found_local_tables);
  bool is_local_table(const char *schema_name, const char *table_name,
                      bool &local_table);
  bool get_schema(const char *schema_name, const dd::Schema **schema_def) const;
  bool schema_exists(const char *schema_name, bool *schema_exists);
  bool update_schema_version(const char *schema_name, unsigned int counter,
                             unsigned int node_id);

  /*
     @brief Lookup tablespace id from tablespace name

     @tablespace_name Name of tablespace
     @tablespace_id Id of the tablespace

     @return true if tablespace found
  */
  bool lookup_tablespace_id(const char *tablespace_name,
                            dd::Object_id *tablespace_id);
  bool get_tablespace(const char *tablespace_name,
                      const dd::Tablespace **tablespace_def);
  bool tablespace_exists(const char *tablespace_name, bool &exists);
  bool fetch_ndb_tablespace_names(std::unordered_set<std::string> &names);
  bool install_tablespace(const char *tablespace_name,
                          const std::vector<std::string> &data_file_names,
                          int tablespace_id, int tablespace_version,
                          bool force_overwrite);
  bool drop_tablespace(const char *tablespace_name,
                       bool fail_if_not_exists = true);
  bool get_logfile_group(const char *logfile_group_name,
                         const dd::Tablespace **logfile_group_def);
  bool logfile_group_exists(const char *logfile_group_name, bool &exists);
  bool fetch_ndb_logfile_group_names(std::unordered_set<std::string> &names);
  bool install_logfile_group(const char *logfile_group_name,
                             const std::vector<std::string> &undo_file_names,
                             int logfile_group_id, int logfile_group_version,
                             bool force_overwrite);
  bool install_undo_file(const char *logfile_group_name,
                         const char *undo_file_name);
  bool drop_logfile_group(const char *logfile_group_name,
                          bool fail_if_not_exists = true);
  bool get_schema_uuid(dd::String_type *value) const;
  bool update_schema_uuid(const char *value) const;

  // Print all NDB tables registered in DD to stderr
  bool dump_NDB_tables();
  // Shuffle the se_private_id of all NDB tables installed in DD
  bool dbug_shuffle_spi_for_NDB_tables();
};

#endif
