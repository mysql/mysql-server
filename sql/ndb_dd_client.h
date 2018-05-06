/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <vector>
#include <unordered_set>

#include "my_inttypes.h"
#include "sql/dd/string_type.h"


namespace dd {
  typedef String_type sdi_t;
  namespace cache {
    class Dictionary_client;
  }
  class Table;
}


/*
  Class encapculating the code for accessing the DD
  from ndbcluster

  Handles:
   - locking and releasing MDL(metadata locks)
   - disabling and restoring autocommit
   - transaction commit and rollback, will automatically
     rollback in case commit has not been called
*/

class Ndb_dd_client {
  class THD* const m_thd;
  dd::cache::Dictionary_client* m_client;
  void* m_auto_releaser; // Opaque pointer
  std::vector<class MDL_ticket*> m_acquired_mdl_tickets;
  ulonglong m_save_option_bits;
  bool m_comitted;

  void disable_autocommit();

  bool store_table(dd::Table* install_table, int ndb_table_id);

public:
  Ndb_dd_client(class THD* thd);

  ~Ndb_dd_client();

  // Metadata lock functions
  bool mdl_lock_schema(const char* schema_name);
  bool mdl_lock_table(const char* schema_name, const char* table_name);
  bool mdl_locks_acquire_exclusive(const char* schema_name,
                                   const char* table_name);
  void mdl_locks_release();

  // Transaction handling functions
  void commit();
  void rollback();

  bool get_engine(const char* schema_name, const char* table_name,
                  dd::String_type* engine);

  bool rename_table(const char *old_schema_name, const char *old_table_name,
                    const char *new_schema_name, const char *new_table_name,
                    int new_table_id, int new_table_version);
  bool remove_table(const char* schema_name, const char* table_name);
  bool install_table(const char* schema_name, const char* table_name,
                     const dd::sdi_t &sdi,
                     int ndb_table_id, int ndb_table_version,
                     bool force_overwrite);
  bool get_table(const char* schema_name, const char* table_name,
                 const dd::Table **table_def);

  bool fetch_schema_names(class std::vector<std::string>*);
  bool get_ndb_table_names_in_schema(const char* schema_name,
                                     std::unordered_set<std::string> *names);
  bool have_local_tables_in_schema(const char* schema_name,
                                   bool* found_local_tables);
  bool schema_exists(const char* schema_name, bool* schema_exists);
};




#endif
