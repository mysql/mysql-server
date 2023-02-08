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

#include "storage/ndb/plugin/ndb_dd_client.h"

#include <assert.h>

#include <iostream>

#include "my_dbug.h"
#include "sql/auth/auth_common.h"  // check_readonly()
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/dd.h"
#include "sql/dd/dd_table.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/schema.h"
#include "sql/dd/types/table.h"
#include "sql/query_options.h"  // OPTION_AUTOCOMMIT
#include "sql/sql_class.h"      // THD
#include "sql/sql_table.h"
#include "sql/sql_trigger.h"  // remove_all_triggers_from_perfschema
#include "sql/system_variables.h"
#include "sql/transaction.h"            // trans_*
#include "storage/ndb/plugin/ndb_dd.h"  // ndb_dd_fs_name_case
#include "storage/ndb/plugin/ndb_dd_disk_data.h"
#include "storage/ndb/plugin/ndb_dd_schema.h"
#include "storage/ndb/plugin/ndb_dd_sdi.h"
#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_dd_upgrade_table.h"
#include "storage/ndb/plugin/ndb_fk_util.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_schema_dist_table.h"
#include "storage/ndb/plugin/ndb_thd.h"

Ndb_dd_client::Ndb_dd_client(THD *thd)
    : m_thd(thd),
      m_client(thd->dd_client()),
      m_save_mdl_locks(thd->mdl_context.mdl_savepoint()) {
  DBUG_TRACE;
  disable_autocommit();

  // Create dictionary client auto releaser, stored as
  // opaque pointer in order to avoid including all of
  // Dictionary_client in the ndb_dd_client header file
  m_auto_releaser =
      (void *)new dd::cache::Dictionary_client::Auto_releaser(m_client);
}

Ndb_dd_client::~Ndb_dd_client() {
  DBUG_TRACE;
  // Automatically restore the option_bits in THD if they have
  // been modified
  if (m_save_option_bits) m_thd->variables.option_bits = m_save_option_bits;

  if (m_auto_rollback) {
    // Automatically rollback unless commit has been called
    if (!m_comitted) rollback();
  }

  // Release MDL locks
  mdl_locks_release();

  // Free the dictionary client auto releaser
  dd::cache::Dictionary_client::Auto_releaser *ar =
      (dd::cache::Dictionary_client::Auto_releaser *)m_auto_releaser;
  delete ar;
}

bool Ndb_dd_client::mdl_lock_table(const char *schema_name,
                                   const char *table_name) {
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&schema_request, MDL_key::SCHEMA, schema_name, "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&mdl_request, MDL_key::TABLE, schema_name, table_name,
                   MDL_SHARED, MDL_EXPLICIT);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&mdl_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout)) {
    return false;
  }

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(schema_request.ticket);
  m_acquired_mdl_tickets.push_back(mdl_request.ticket);

  return true;
}

bool Ndb_dd_client::mdl_lock_schema_exclusive(const char *schema_name,
                                              bool custom_lock_wait,
                                              ulong lock_wait_timeout) {
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request backup_lock_request;
  MDL_request grl_request;

  // If protection against GRL can't be acquired, err out early.
  if (m_thd->global_read_lock.can_acquire_protection()) {
    return false;
  }

  MDL_REQUEST_INIT(&schema_request, MDL_key::SCHEMA, schema_name, "",
                   MDL_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&backup_lock_request, MDL_key::BACKUP_LOCK, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&grl_request, MDL_key::GLOBAL, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&backup_lock_request);
  mdl_requests.push_front(&grl_request);

  if (!custom_lock_wait) {
    lock_wait_timeout = m_thd->variables.lock_wait_timeout;
  }

  if (m_thd->mdl_context.acquire_locks(&mdl_requests, lock_wait_timeout)) {
    return false;
  }

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(schema_request.ticket);
  m_acquired_mdl_tickets.push_back(backup_lock_request.ticket);
  m_acquired_mdl_tickets.push_back(grl_request.ticket);

  return true;
}

bool Ndb_dd_client::mdl_lock_schema(const char *schema_name) {
  MDL_request_list mdl_requests;
  MDL_request schema_request;

  MDL_REQUEST_INIT(&schema_request, MDL_key::SCHEMA, schema_name, "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);
  mdl_requests.push_front(&schema_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout)) {
    return false;
  }

  /*
    Now when we have protection against concurrent change of read_only
    option we can safely re-check its value.
  */
  if (check_readonly(m_thd, true)) return false;

  // Remember ticket(s) of the acquired mdl lock
  m_acquired_mdl_tickets.push_back(schema_request.ticket);

  return true;
}

bool Ndb_dd_client::mdl_lock_logfile_group_exclusive(
    const char *logfile_group_name, bool custom_lock_wait,
    ulong lock_wait_timeout) {
  MDL_request_list mdl_requests;
  MDL_request logfile_group_request;
  MDL_request backup_lock_request;
  MDL_request grl_request;

  // If protection against GRL can't be acquired, err out early.
  if (m_thd->global_read_lock.can_acquire_protection()) {
    return false;
  }

  MDL_REQUEST_INIT(&logfile_group_request, MDL_key::TABLESPACE, "",
                   logfile_group_name, MDL_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&backup_lock_request, MDL_key::BACKUP_LOCK, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&grl_request, MDL_key::GLOBAL, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);

  mdl_requests.push_front(&logfile_group_request);
  mdl_requests.push_front(&backup_lock_request);
  mdl_requests.push_front(&grl_request);

  if (!custom_lock_wait) {
    lock_wait_timeout = m_thd->variables.lock_wait_timeout;
  }

  if (m_thd->mdl_context.acquire_locks(&mdl_requests, lock_wait_timeout)) {
    return false;
  }

  /*
    Now when we have protection against concurrent change of read_only
    option we can safely re-check its value.
  */
  if (check_readonly(m_thd, true)) return false;

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(logfile_group_request.ticket);
  m_acquired_mdl_tickets.push_back(backup_lock_request.ticket);
  m_acquired_mdl_tickets.push_back(grl_request.ticket);

  return true;
}

bool Ndb_dd_client::mdl_lock_logfile_group(const char *logfile_group_name,
                                           bool intention_exclusive) {
  MDL_request_list mdl_requests;
  MDL_request logfile_group_request;

  enum_mdl_type mdl_type =
      intention_exclusive ? MDL_INTENTION_EXCLUSIVE : MDL_SHARED_READ;
  MDL_REQUEST_INIT(&logfile_group_request, MDL_key::TABLESPACE, "",
                   logfile_group_name, mdl_type, MDL_EXPLICIT);

  mdl_requests.push_front(&logfile_group_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout)) {
    return false;
  }

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(logfile_group_request.ticket);

  return true;
}

bool Ndb_dd_client::mdl_lock_tablespace_exclusive(const char *tablespace_name,
                                                  bool custom_lock_wait,
                                                  ulong lock_wait_timeout) {
  MDL_request_list mdl_requests;
  MDL_request tablespace_request;
  MDL_request backup_lock_request;
  MDL_request grl_request;

  // If protection against GRL can't be acquired, err out early.
  if (m_thd->global_read_lock.can_acquire_protection()) {
    return false;
  }

  MDL_REQUEST_INIT(&tablespace_request, MDL_key::TABLESPACE, "",
                   tablespace_name, MDL_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&backup_lock_request, MDL_key::BACKUP_LOCK, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&grl_request, MDL_key::GLOBAL, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);

  mdl_requests.push_front(&tablespace_request);
  mdl_requests.push_front(&backup_lock_request);
  mdl_requests.push_front(&grl_request);

  if (!custom_lock_wait) {
    lock_wait_timeout = m_thd->variables.lock_wait_timeout;
  }

  if (m_thd->mdl_context.acquire_locks(&mdl_requests, lock_wait_timeout)) {
    return false;
  }

  /*
    Now when we have protection against concurrent change of read_only
    option we can safely re-check its value.
  */
  if (check_readonly(m_thd, true)) return false;

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(tablespace_request.ticket);
  m_acquired_mdl_tickets.push_back(backup_lock_request.ticket);
  m_acquired_mdl_tickets.push_back(grl_request.ticket);

  return true;
}

bool Ndb_dd_client::mdl_lock_tablespace(const char *tablespace_name,
                                        bool intention_exclusive) {
  MDL_request_list mdl_requests;
  MDL_request tablespace_request;

  enum_mdl_type mdl_type =
      intention_exclusive ? MDL_INTENTION_EXCLUSIVE : MDL_SHARED_READ;
  MDL_REQUEST_INIT(&tablespace_request, MDL_key::TABLESPACE, "",
                   tablespace_name, mdl_type, MDL_EXPLICIT);

  mdl_requests.push_front(&tablespace_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout)) {
    return false;
  }

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(tablespace_request.ticket);

  return true;
}

bool Ndb_dd_client::mdl_locks_acquire_exclusive(const char *schema_name,
                                                const char *table_name,
                                                bool custom_lock_wait,
                                                ulong lock_wait_timeout) {
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request mdl_request;
  MDL_request backup_lock_request;
  MDL_request grl_request;

  // If we cannot acquire protection against GRL, err out early.
  if (m_thd->global_read_lock.can_acquire_protection()) return false;

  MDL_REQUEST_INIT(&schema_request, MDL_key::SCHEMA, schema_name, "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&mdl_request, MDL_key::TABLE, schema_name, table_name,
                   MDL_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&backup_lock_request, MDL_key::BACKUP_LOCK, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);
  MDL_REQUEST_INIT(&grl_request, MDL_key::GLOBAL, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&mdl_request);
  mdl_requests.push_front(&backup_lock_request);
  mdl_requests.push_front(&grl_request);

  if (!custom_lock_wait) {
    lock_wait_timeout = m_thd->variables.lock_wait_timeout;
  }

  if (m_thd->mdl_context.acquire_locks(&mdl_requests, lock_wait_timeout)) {
    return false;
  }

  /*
    Now when we have protection against concurrent change of read_only
    option we can safely re-check its value.
  */
  if (check_readonly(m_thd, true)) return false;

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(schema_request.ticket);
  m_acquired_mdl_tickets.push_back(mdl_request.ticket);
  m_acquired_mdl_tickets.push_back(backup_lock_request.ticket);
  m_acquired_mdl_tickets.push_back(grl_request.ticket);

  return true;
}

void Ndb_dd_client::mdl_locks_release() {
  // Release MDL locks acquired in EXPLICIT scope
  for (MDL_ticket *ticket : m_acquired_mdl_tickets) {
    m_thd->mdl_context.release_lock(ticket);
  }
  // Release new MDL locks acquired in TRANSACTIONAL and STATEMENT scope
  m_thd->mdl_context.rollback_to_savepoint(m_save_mdl_locks);
}

void Ndb_dd_client::disable_autocommit() {
  /*
    Implementation details from which storage the DD uses leaks out
    and the user of these functions magically need to turn auto commit
    off.

    I.e as in sql_table.cc, execute_ddl_log_recovery()
     'Prevent InnoDB from automatically committing InnoDB transaction
      each time data-dictionary tables are closed after being updated.'
  */

  // Don't allow empty bits as zero is used as indicator
  // to restore the saved bits
  assert(m_thd->variables.option_bits);
  m_save_option_bits = m_thd->variables.option_bits;

  m_thd->variables.option_bits &= ~OPTION_AUTOCOMMIT;
  m_thd->variables.option_bits |= OPTION_NOT_AUTOCOMMIT;
}

void Ndb_dd_client::commit() {
  trans_commit_stmt(m_thd);
  trans_commit(m_thd);
  m_comitted = true;
}

void Ndb_dd_client::rollback() {
  trans_rollback_stmt(m_thd);
  trans_rollback(m_thd);
}

bool Ndb_dd_client::get_engine(const char *schema_name, const char *table_name,
                               dd::String_type *engine) {
  const dd::Table *existing = nullptr;
  if (m_client->acquire(schema_name, table_name, &existing)) {
    return false;
  }

  if (existing == nullptr) {
    // Table does not exist in DD
    return false;
  }

  *engine = existing->engine();

  return true;
}

bool Ndb_dd_client::rename_table(
    const char *old_schema_name, const char *old_table_name,
    const char *new_schema_name, const char *new_table_name, int new_table_id,
    int new_table_version, Ndb_referenced_tables_invalidator *invalidator) {
  // Read new schema from DD
  const dd::Schema *new_schema = nullptr;
  if (m_client->acquire(new_schema_name, &new_schema)) {
    return false;
  }
  if (new_schema == nullptr) {
    // Database does not exist, unexpected
    assert(false);
    return false;
  }

  // Read table from DD
  dd::Table *to_table_def = nullptr;
  if (m_client->acquire_for_modification(old_schema_name, old_table_name,
                                         &to_table_def))
    return false;

  if (invalidator != nullptr &&
      !invalidator->fetch_referenced_tables_to_invalidate(
          old_schema_name, old_table_name, to_table_def, true)) {
    return false;
  }

  // Collect and lock all the tables referencing this table,
  // referenced by this table and the foreign key names.
  // Note : This re-attempts to lock the referenced tables that were already
  //        locked by fetch_referenced_tables_to_invalidate(). This will be
  //        fixed when Bug#30500825 gets fixed.
  if (collect_and_lock_fk_tables_for_rename_table(
          m_thd, old_schema_name, old_table_name, to_table_def, new_schema_name,
          new_table_name, ndbcluster_hton, nullptr)) {
    return false;
  }

  // Set schema id and table name
  to_table_def->set_schema_id(new_schema->id());
  to_table_def->set_name(new_table_name);

  const Ndb_dd_handle dd_handle{new_table_id, new_table_version};
  ndb_dd_table_set_spi_and_version(to_table_def, dd_handle);

  // Rename foreign keys
  if (dd::rename_foreign_keys(m_thd, old_schema_name, old_table_name,
                              ndbcluster_hton, new_schema_name, to_table_def)) {
    // Failed to rename foreign keys or commit/rollback, unexpected
    assert(false);
    return false;
  }

  // Adjust parent table for self-referencing foreign keys.
  dd::Table::Foreign_key_collection *foreign_keys =
      to_table_def->foreign_keys();
  for (dd::Foreign_key *fk : *foreign_keys) {
    if (strcmp(fk->referenced_table_schema_name().c_str(), old_schema_name) ==
            0 &&
        strcmp(fk->referenced_table_name().c_str(), old_table_name) == 0) {
      fk->set_referenced_table_schema_name(new_schema_name);
      fk->set_referenced_table_name(new_table_name);
    }
  }

  // Save table in DD
  if (m_client->update(to_table_def)) {
    // Failed to save, unexpected
    assert(false);
    return false;
  }

  // Update the foreign key information of tables referencing this table in DD
  if (adjust_fks_for_rename_table(m_thd, old_schema_name, old_table_name,
                                  new_schema_name, new_table_name,
                                  ndbcluster_hton)) {
    return false;
  }

  return true;
}

bool Ndb_dd_client::remove_table(
    const dd::Table *table, const char *schema_name, const char *table_name,
    Ndb_referenced_tables_invalidator *invalidator) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("table_name: '%s'", table->name().c_str()));

  if (table == nullptr) {
    // Table does not exist
    return true;
  }

  if (invalidator != nullptr &&
      !invalidator->fetch_referenced_tables_to_invalidate(
          schema_name, table_name, table, true)) {
    return false;
  }

#ifdef HAVE_PSI_SP_INTERFACE
  // Remove statistics, table is not using trigger(s) anymore
  remove_all_triggers_from_perfschema(schema_name, *table);
#endif

  DBUG_PRINT("info", ("removing existing table"));
  if (m_client->drop(table)) {
    // Failed to remove table
    // Catch in debug, unexpected error
    assert(false);
    return false;
  }

  return true;
}

bool Ndb_dd_client::remove_table(const char *schema_name,
                                 const char *table_name,
                                 Ndb_referenced_tables_invalidator *invalidator)

{
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("schema_name: '%s', table_name: '%s'", schema_name, table_name));

  const dd::Table *existing = nullptr;
  if (m_client->acquire(schema_name, table_name, &existing)) {
    return false;
  }

  return remove_table(existing, schema_name, table_name, invalidator);
}

bool Ndb_dd_client::remove_table(
    dd::Object_id spi, Ndb_referenced_tables_invalidator *invalidator) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("se_private_id: '%lld'", spi));

  dd::Table *table;
  dd::Schema *schema;
  const char *engine = "ndbcluster";

  // Find old table using the NDB tables id, skip missing SPI cache
  if (m_client->acquire_uncached_table_by_se_private_id(engine, spi, &table,
                                                        true)) {
    ndb_log_warning("Error retrieving table definition ndbcluster-%lld", spi);
    return false;
  }

  if (table == nullptr) {
    // Table does not exist or was inadvertently cached. Nothing to do.
    ndb_log_verbose(50, "Table definition with %s-%llu does not exist", engine,
                    spi);
    return true;
  }

#ifndef NDEBUG
  // Double check that old table is in NDB
  if (table->engine() != engine) {
    assert(false);
    return false;
  }
#endif

  if (m_client->acquire_uncached(table->schema_id(), &schema)) {
    return false;
  }

  const char *schema_name = schema->name().c_str();
  const char *table_name = table->name().c_str();

  // Take exclusive locks on old table
  if (!mdl_locks_acquire_exclusive(schema_name, table_name)) {
    // Failed to MDL lock old table
    return false;
  }

  return remove_table(table, schema_name, table_name, invalidator);
}

bool Ndb_dd_client::deserialize_table(const dd::sdi_t &sdi,
                                      dd::Table *table_def) {
  if (ndb_dd_sdi_deserialize(m_thd, sdi, table_def)) {
    return false;
  }
  return true;
}

bool Ndb_dd_client::store_table(dd::Table *install_table) const {
  DBUG_TRACE;

  if (!m_client->store(install_table)) {
    return true;  // OK
  }
  return false;
}

bool Ndb_dd_client::store_table(dd::Table *install_table, int ndb_table_id) {
  DBUG_TRACE;

  if (!m_client->store(install_table)) {
    return true;  // OK
  }

  DBUG_PRINT("error", ("Failed to store table, error: '%d %s'",
                       m_thd->get_stmt_da()->mysql_errno(),
                       m_thd->get_stmt_da()->message_text()));

  if (m_thd->get_stmt_da()->mysql_errno() == ER_DUP_ENTRY) {
    // Try to handle the failure which may occur when the DD already
    // have a table definition from an old NDB table which used the
    // same table id but with a different name.
    // This may happen when the MySQL Server reconnects to the cluster
    // and synchronizes its DD with NDB dictionary. Of course it indicates
    // that the DD is out of synch with the dictionary in NDB but that's
    // normal when the MySQL Server haven't taken part in DDL operations.
    // And as usual NDB is the master for all NDB tables.

    // Remove the current ER_DUP_ENTRY error, subsequent failures
    // will set a new error
    m_thd->clear_error();

    if (!remove_table(ndb_table_id)) {
      // Failed to remove old table from DD
      return false;
    }

    // Try to store the new table again
    if (m_client->store(install_table)) {
      DBUG_PRINT("error", ("Failed to store table, error: '%d %s'",
                           m_thd->get_stmt_da()->mysql_errno(),
                           m_thd->get_stmt_da()->message_text()));
      return false;
    }

    // Removed old table and stored the new, return OK
    assert(!m_thd->is_error());
    return true;
  }

  return false;
}

bool Ndb_dd_client::install_table(
    const char *schema_name, const char *table_name, const dd::sdi_t &sdi,
    int ndb_table_id, int ndb_table_version, size_t ndb_num_partitions,
    const std::string &tablespace_name, bool force_overwrite,
    Ndb_referenced_tables_invalidator *invalidator) {
  const dd::Schema *schema = nullptr;

  if (m_client->acquire(schema_name, &schema)) {
    return false;
  }
  if (schema == nullptr) {
    assert(false);  // Database does not exist
    return false;
  }

  std::unique_ptr<dd::Table> install_table{dd::create_object<dd::Table>()};
  if (ndb_dd_sdi_deserialize(m_thd, sdi, install_table.get())) {
    return false;
  }

  // Verify that table_name in the unpacked table definition
  // matches the table name to install
  assert(ndb_dd_fs_name_case(install_table->name()) == table_name);

  // Verify that table definition unpacked from NDB
  // does not have any se_private fields set, those will be set
  // from the NDB table metadata
  assert(install_table->se_private_id() == dd::INVALID_OBJECT_ID);
  assert(install_table->se_private_data().raw_string() == "");

  // Assign the id of the schema to the table_object
  install_table->set_schema_id(schema->id());

  // Assign NDB id and version of the table
  const Ndb_dd_handle dd_handle{ndb_table_id, ndb_table_version};
  ndb_dd_table_set_spi_and_version(install_table.get(), dd_handle);

  // Check if the DD table object has the correct number of partitions.
  // Correct the number of partitions in the DD table object in case of
  // a mismatch
  const bool check_partition_count_result = ndb_dd_table_check_partition_count(
      install_table.get(), ndb_num_partitions);
  if (!check_partition_count_result) {
    ndb_dd_table_fix_partition_count(install_table.get(), ndb_num_partitions);
  }

  // Set the tablespace id if applicable
  if (!tablespace_name.empty()) {
    dd::Object_id tablespace_id;
    if (!lookup_tablespace_id(tablespace_name.c_str(), &tablespace_id)) {
      return false;
    }
    ndb_dd_table_set_tablespace_id(install_table.get(), tablespace_id);
  }

  // Get the old table definition and compare the ndb_table_id with the
  // already installed id (on se_private_id). First, attempt to get the
  // definition using the table's full name. If the ids are different,
  // attempt to retrieve the previous definition using the to-be-installed
  // ndb_table_id. Drop all required definitions to install the new

  const dd::Table *old_table_def = nullptr;
  if (m_client->acquire(schema_name, table_name, &old_table_def)) {
    return false;
  }

  if (invalidator != nullptr &&
      !invalidator->fetch_referenced_tables_to_invalidate(
          schema_name, table_name, old_table_def)) {
    return false;
  }

  if (old_table_def != nullptr) {
    Ndb_dd_handle old_handle = ndb_dd_table_get_spi_and_version(old_table_def);
    if (!old_handle.valid()) {
      DBUG_PRINT("error", ("Could not extract object_id and object_version "
                           "from table definition"));
      assert(false);
      return false;
    }

    // Table already exists so do force overwrite
    if (!force_overwrite) {
      assert(false);
      return false;
    }

    // Check that se_private_id of the existing table in DD
    // matches NDB, otherwise it's a programming error
    // not to request "force_overwrite"
    if (old_handle == dd_handle) {
      // Table is already installed, with same id and version
      // return success
      return true;
    }

    // Drop the old table definition of schema_name.table_name
    // It needs to be done to free up "schema-id"-"table_name" DD entries
    DBUG_PRINT("info", ("dropping existing table"));
    if (m_client->drop(old_table_def)) {
      // Failed to drop existing
      assert(false);  // Catch in debug, unexpected error
      return false;
    }

    // If the table to be installed has a different se_private_id than
    // the existing, then the currently installed se_private_id table must
    // also be removed if exists.
    if (dd_handle.spi != old_handle.spi) {
      ndb_log_info(
          "Cached se_private_id different than ndb_table_id. "
          "Removing ndbcluster-%llu table definition if exists",
          dd_handle.spi);

      if (!remove_table(dd_handle.spi)) {
        ndb_log_warning(
            "Failed to remove table definition from DD for spi=%llu. ",
            dd_handle.spi);
      }
    }
  } else {
    // Remove any stale DD table that may be occupying this
    // ndbcluster-<id> place
    if (!remove_table(dd_handle.spi)) {
      ndb_log_info("Failed to remove ndbcluster-%llu from DD", dd_handle.spi);
    }
  }

  if (!store_table(install_table.get(), ndb_table_id)) {
    const dd::sdi_t new_table_sdi = ndb_dd_sdi_serialize(
        m_thd, *install_table.get(), dd::String_type(schema_name));

    ndb_log_error("Failed to store table: '%s.%s'", schema_name, table_name);
    ndb_log_error_dump("sdi for new table: %s",
                       ndb_dd_sdi_prettify(new_table_sdi).c_str());

    if (old_table_def) {
      const dd::sdi_t old_table_sdi = ndb_dd_sdi_serialize(
          m_thd, *old_table_def, dd::String_type(schema_name));
      ndb_log_error_dump("sdi for existing table: %s",
                         ndb_dd_sdi_prettify(old_table_sdi).c_str());
    }
    DBUG_ABORT();
    return false;
  }

  return true;  // OK
}

bool Ndb_dd_client::migrate_table(const char *schema_name,
                                  const char *table_name,
                                  const unsigned char *frm_data,
                                  unsigned int unpacked_len,
                                  bool force_overwrite) {
  if (force_overwrite) {
    // Remove the old table before migrating
    DBUG_PRINT("info", ("dropping existing table"));
    if (!remove_table(schema_name, table_name)) {
      return false;
    }

    commit();
  }

  const bool migrate_result = dd::ndb_upgrade::migrate_table_to_dd(
      m_thd, this, schema_name, table_name, frm_data, unpacked_len);

  return migrate_result;
}

bool Ndb_dd_client::get_table(const char *schema_name, const char *table_name,
                              const dd::Table **table_def) {
  if (m_client->acquire(schema_name, table_name, table_def)) {
    my_error(ER_NO_SUCH_TABLE, MYF(0), schema_name, table_name);
    return false;
  }
  return true;
}

bool Ndb_dd_client::table_exists(const char *schema_name,
                                 const char *table_name, bool &exists) {
  const dd::Table *table;
  if (m_client->acquire(schema_name, table_name, &table)) {
    // Failed to acquire the requested table
    return false;
  }

  if (table == nullptr) {
    // The table doesn't exist
    exists = false;
    return true;
  }

  // The table exists
  exists = true;
  return true;
}

bool Ndb_dd_client::set_tablespace_id_in_table(const char *schema_name,
                                               const char *table_name,
                                               dd::Object_id tablespace_id) {
  dd::Table *table_def = nullptr;
  if (m_client->acquire_for_modification(schema_name, table_name, &table_def)) {
    return false;
  }
  if (table_def == nullptr) {
    assert(false);
    return false;
  }

  ndb_dd_table_set_tablespace_id(table_def, tablespace_id);

  if (m_client->update(table_def)) {
    return false;
  }
  return true;
}

bool Ndb_dd_client::set_object_id_and_version_in_table(const char *schema_name,
                                                       const char *table_name,
                                                       int object_id,
                                                       int object_version) {
  DBUG_TRACE;

  /* Acquire the table */
  dd::Table *table_def = nullptr;
  if (m_client->acquire_for_modification(schema_name, table_name, &table_def)) {
    DBUG_PRINT("error", ("Failed to load the table from DD"));
    return false;
  }

  /* Update id and version */
  Ndb_dd_handle dd_handle{object_id, object_version};
  ndb_dd_table_set_spi_and_version(table_def, dd_handle);

  /* Update it to DD */
  if (m_client->update(table_def)) {
    return false;
  }

  return true;
}

bool Ndb_dd_client::fetch_all_schemas(
    std::map<std::string, const dd::Schema *> &schemas) {
  DBUG_TRACE;

  std::vector<const dd::Schema *> schemas_list;
  if (m_client->fetch_global_components(&schemas_list)) {
    DBUG_PRINT("error", ("Failed to fetch all schemas"));
    return false;
  }

  for (const dd::Schema *schema : schemas_list) {
    // Convert the schema name to lower case on platforms that have
    // lower_case_table_names set to 2
    const std::string schema_name = ndb_dd_fs_name_case(schema->name());
    schemas.insert(std::make_pair(schema_name.c_str(), schema));
  }
  return true;
}

bool Ndb_dd_client::fetch_schema_names(std::vector<std::string> *names) {
  DBUG_TRACE;

  std::vector<const dd::Schema *> schemas;
  if (m_client->fetch_global_components(&schemas)) {
    return false;
  }

  for (const dd::Schema *schema : schemas) {
    // Convert the schema name to lower case on platforms that have
    // lower_case_table_names set to 2
    const std::string schema_name = ndb_dd_fs_name_case(schema->name());
    names->push_back(schema_name.c_str());
  }
  return true;
}

bool Ndb_dd_client::get_ndb_table_names_in_schema(
    const char *schema_name, std::unordered_set<std::string> *names) {
  DBUG_TRACE;

  const dd::Schema *schema;
  if (m_client->acquire(schema_name, &schema)) {
    // Failed to open the requested Schema object
    return false;
  }

  if (schema == nullptr) {
    // Database does not exist
    return false;
  }

  std::vector<dd::String_type> table_names;
  if (m_client->fetch_schema_table_names_by_engine(schema, "ndbcluster",
                                                   &table_names)) {
    return false;
  }

  for (const auto &name : table_names) {
    if (!mdl_lock_table(schema_name, name.c_str())) {
      // Failed to MDL lock table
      return false;
    }

    // Convert the table name to lower case on platforms that have
    // lower_case_table_names set to 2
    const std::string table_name = ndb_dd_fs_name_case(name);
    names->insert(table_name);
  }
  return true;
}

bool Ndb_dd_client::get_table_names_in_schema(
    const char *schema_name, std::unordered_set<std::string> *ndb_tables,
    std::unordered_set<std::string> *local_tables) {
  DBUG_TRACE;

  const dd::Schema *schema;
  if (m_client->acquire(schema_name, &schema)) {
    // Failed to open the requested Schema object
    return false;
  }

  if (schema == nullptr) {
    // Database does not exist
    return false;
  }

  // Fetch NDB table names
  std::vector<dd::String_type> ndb_table_names;
  if (m_client->fetch_schema_table_names_by_engine(schema, "ndbcluster",
                                                   &ndb_table_names)) {
    return false;
  }
  for (const auto &name : ndb_table_names) {
    // Lock the table in DD
    if (!mdl_lock_table(schema_name, name.c_str())) {
      // Failed to acquire MDL
      return false;
    }
    // Convert the table name to lower case on platforms that have
    // lower_case_table_names set to 2
    const std::string table_name = ndb_dd_fs_name_case(name);
    ndb_tables->insert(table_name);
  }

  // Fetch all table names
  std::vector<dd::String_type> all_table_names;
  if (m_client->fetch_schema_table_names_not_hidden_by_se(schema,
                                                          &all_table_names)) {
    return false;
  }
  for (const auto &name : all_table_names) {
    // Convert the table name to lower case on platforms that have
    // lower_case_table_names set to 2
    const std::string table_name = ndb_dd_fs_name_case(name);
    if (ndb_tables->find(table_name) != ndb_tables->end()) {
      // Skip NDB table
      continue;
    }
    // Lock the table in DD
    if (!mdl_lock_table(schema_name, name.c_str())) {
      // Failed to acquire MDL
      return false;
    }
    local_tables->insert(table_name);
  }
  return true;
}

/*
  Check given schema for local tables(i.e not in NDB)

  @param        schema_name          Name of the schema to check for tables
  @param [out]  found_local_tables   Return parameter indicating if the schema
                                     contained local tables or not.

  @return       false  Failure
  @return       true   Success.
*/

bool Ndb_dd_client::have_local_tables_in_schema(const char *schema_name,
                                                bool *found_local_tables) {
  DBUG_TRACE;

  const dd::Schema *schema;
  if (m_client->acquire(schema_name, &schema)) {
    // Failed to open the requested schema
    return false;
  }

  if (schema == nullptr) {
    // The schema didn't exist, thus it can't have any local tables
    *found_local_tables = false;
    return true;
  }

  // Fetch all table names
  std::vector<dd::String_type> all_table_names;
  if (m_client->fetch_schema_table_names_not_hidden_by_se(schema,
                                                          &all_table_names)) {
    return false;
  }
  // Fetch NDB table names
  std::vector<dd::String_type> ndb_table_names;
  if (m_client->fetch_schema_table_names_by_engine(schema, "ndbcluster",
                                                   &ndb_table_names)) {
    return false;
  }

  *found_local_tables = all_table_names.size() > ndb_table_names.size();

  return true;
}

bool Ndb_dd_client::is_local_table(const char *schema_name,
                                   const char *table_name, bool &local_table) {
  const dd::Table *table;
  if (m_client->acquire(schema_name, table_name, &table)) {
    // Failed to acquire the requested table
    return false;
  }
  if (table == nullptr) {
    // The table doesn't exist
    assert(false);
    return false;
  }
  local_table = table->engine() != "ndbcluster";
  return true;
}

bool Ndb_dd_client::get_schema(const char *schema_name,
                               const dd::Schema **schema_def) const {
  if (m_client->acquire(schema_name, schema_def)) {
    // Error is reported by the dictionary subsystem.
    return false;
  }
  return true;
}

bool Ndb_dd_client::schema_exists(const char *schema_name,
                                  bool *schema_exists) {
  DBUG_TRACE;

  const dd::Schema *schema;
  if (m_client->acquire(schema_name, &schema)) {
    // Failed to open the requested schema
    return false;
  }

  if (schema == nullptr) {
    // The schema didn't exist
    *schema_exists = false;
    return true;
  }

  // The schema exists
  *schema_exists = true;
  return true;
}

bool Ndb_dd_client::update_schema_version(const char *schema_name,
                                          unsigned int counter,
                                          unsigned int node_id) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("Schema : %s, counter : %u, node_id : %u", schema_name,
                       counter, node_id));

  assert(m_thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::SCHEMA, schema_name, "", MDL_EXCLUSIVE));

  dd::Schema *schema;

  if (m_client->acquire_for_modification(schema_name, &schema) ||
      schema == nullptr) {
    DBUG_PRINT("error", ("Failed to fetch the Schema object"));
    return false;
  }

  // Set the values
  ndb_dd_schema_set_counter_and_nodeid(schema, counter, node_id);

  // Update Schema in DD
  if (m_client->update(schema)) {
    DBUG_PRINT("error", ("Failed to update the Schema in DD"));
    return false;
  }

  return true;
}

bool Ndb_dd_client::lookup_tablespace_id(const char *tablespace_name,
                                         dd::Object_id *tablespace_id) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("tablespace_name: %s", tablespace_name));

  assert(m_thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::TABLESPACE, "", tablespace_name, MDL_INTENTION_EXCLUSIVE));

  // Acquire tablespace.
  const dd::Tablespace *ts_obj = nullptr;
  if (m_client->acquire(tablespace_name, &ts_obj)) {
    // acquire() always fails with an error being reported.
    return false;
  }

  if (!ts_obj) {
    my_error(ER_TABLESPACE_MISSING_WITH_NAME, MYF(0), tablespace_name);
    return false;
  }

  *tablespace_id = ts_obj->id();
  DBUG_PRINT("exit", ("tablespace_id: %llu", *tablespace_id));

  return true;
}

bool Ndb_dd_client::get_tablespace(const char *tablespace_name,
                                   const dd::Tablespace **tablespace_def) {
  if (m_client->acquire(tablespace_name, tablespace_def)) {
    return false;
  }
  return true;
}

bool Ndb_dd_client::tablespace_exists(const char *tablespace_name,
                                      bool &exists) {
  const dd::Tablespace *tablespace;
  if (m_client->acquire(tablespace_name, &tablespace)) {
    // Failed to acquire the requested tablespace
    return false;
  }

  if (tablespace == nullptr) {
    // The tablespace doesn't exist
    exists = false;
    return true;
  }

  // The tablespace exists
  exists = true;
  return true;
}

bool Ndb_dd_client::fetch_ndb_tablespace_names(
    std::unordered_set<std::string> &names) {
  DBUG_TRACE;

  std::vector<const dd::Tablespace *> tablespaces;
  if (m_client->fetch_global_components<dd::Tablespace>(&tablespaces)) {
    return false;
  }

  for (const dd::Tablespace *tablespace : tablespaces) {
    if (tablespace->engine() != "ndbcluster") {
      // Skip non-NDB objects
      continue;
    }

    // Find out type of object
    object_type type;

    ndb_dd_disk_data_get_object_type(tablespace->se_private_data(), type);

    if (type != object_type::TABLESPACE) {
      // Skip logfile groups
      continue;
    }

    // Acquire lock in DD
    if (!mdl_lock_tablespace(tablespace->name().c_str(),
                             false /* intention_exclusive */)) {
      // Failed to acquire MDL lock
      return false;
    }

    names.insert(tablespace->name().c_str());
  }
  return true;
}

bool Ndb_dd_client::install_tablespace(
    const char *tablespace_name,
    const std::vector<std::string> &data_file_names, int tablespace_id,
    int tablespace_version, bool force_overwrite) {
  DBUG_TRACE;

  bool exists;
  if (!tablespace_exists(tablespace_name, exists)) {
    // Could not detect if the tablespace exists or not
    return false;
  }

  if (exists) {
    if (force_overwrite) {
      if (!drop_tablespace(tablespace_name)) {
        // Failed to drop tablespace
        return false;
      }
    } else {
      // Error since tablespace exists but force_overwrite not set by caller
      // No point continuing since the subsequent store() will fail
      return false;
    }
  }

  std::unique_ptr<dd::Tablespace> tablespace(
      dd::create_object<dd::Tablespace>());

  // Set name
  tablespace->set_name(tablespace_name);

  // Engine type
  tablespace->set_engine("ndbcluster");

  // Add data files
  for (const auto &data_file_name : data_file_names) {
    ndb_dd_disk_data_add_file(tablespace.get(), data_file_name.c_str());
  }

  // Assign id and version
  ndb_dd_disk_data_set_object_id_and_version(tablespace.get(), tablespace_id,
                                             tablespace_version);

  // Assign object type as tablespace
  ndb_dd_disk_data_set_object_type(tablespace.get()->se_private_data(),
                                   object_type::TABLESPACE);

  // Write changes to dictionary.
  if (m_client->store(tablespace.get())) {
    return false;
  }

  return true;
}

bool Ndb_dd_client::drop_tablespace(const char *tablespace_name,
                                    bool fail_if_not_exists)

{
  DBUG_TRACE;

  const dd::Tablespace *existing = nullptr;
  if (m_client->acquire(tablespace_name, &existing)) {
    return false;
  }

  if (existing == nullptr) {
    // Tablespace does not exist
    if (fail_if_not_exists) {
      return false;
    }
    return true;
  }

  if (m_client->drop(existing)) {
    return false;
  }

  return true;
}

bool Ndb_dd_client::get_logfile_group(
    const char *logfile_group_name, const dd::Tablespace **logfile_group_def) {
  if (m_client->acquire(logfile_group_name, logfile_group_def)) {
    return false;
  }
  return true;
}

bool Ndb_dd_client::logfile_group_exists(const char *logfile_group_name,
                                         bool &exists) {
  const dd::Tablespace *logfile_group;
  if (m_client->acquire(logfile_group_name, &logfile_group)) {
    // Failed to acquire the requested logfile group
    return false;
  }

  if (logfile_group == nullptr) {
    // The logfile group doesn't exist
    exists = false;
    return true;
  }

  // The logfile group exists
  exists = true;
  return true;
}

bool Ndb_dd_client::fetch_ndb_logfile_group_names(
    std::unordered_set<std::string> &names) {
  DBUG_TRACE;

  std::vector<const dd::Tablespace *> tablespaces;
  if (m_client->fetch_global_components<dd::Tablespace>(&tablespaces)) {
    return false;
  }

  for (const dd::Tablespace *tablespace : tablespaces) {
    if (tablespace->engine() != "ndbcluster") {
      // Skip non-NDB objects
      continue;
    }

    // Find out type of object
    object_type type;

    ndb_dd_disk_data_get_object_type(tablespace->se_private_data(), type);

    if (type != object_type::LOGFILE_GROUP) {
      // Skip tablespaces
      continue;
    }

    // Acquire lock in DD
    if (!mdl_lock_logfile_group(tablespace->name().c_str(),
                                false /* intention_exclusive */)) {
      // Failed to acquire MDL lock
      return false;
    }

    names.insert(tablespace->name().c_str());
  }
  return true;
}

bool Ndb_dd_client::install_logfile_group(
    const char *logfile_group_name,
    const std::vector<std::string> &undo_file_names, int logfile_group_id,
    int logfile_group_version, bool force_overwrite) {
  DBUG_TRACE;

  /*
   * Logfile groups are stored as tablespaces in the DD.
   * This is acceptable since the only reason for storing
   * them in the DD is to ensure that INFORMATION_SCHEMA
   * is aware of their presence. Thus, rather than
   * extending DD, we use tablespaces since they resemble
   * logfile groups in terms of metadata structure
   */

  bool exists;
  if (!logfile_group_exists(logfile_group_name, exists)) {
    // Could not detect if the logfile group exists or not
    return false;
  }

  if (exists) {
    if (force_overwrite) {
      if (!drop_logfile_group(logfile_group_name)) {
        // Failed to drop logfile group
        return false;
      }
    } else {
      // Error since logfile group exists but force_overwrite not set to true by
      // caller. No point continuing since the subsequent store() will fail
      return false;
    }
  }

  std::unique_ptr<dd::Tablespace> logfile_group(
      dd::create_object<dd::Tablespace>());

  // Set name
  logfile_group->set_name(logfile_group_name);

  // Engine type
  logfile_group->set_engine("ndbcluster");

  // Add undofiles
  for (const auto &undo_file_name : undo_file_names) {
    ndb_dd_disk_data_add_file(logfile_group.get(), undo_file_name.c_str());
  }

  // Assign id and version
  ndb_dd_disk_data_set_object_id_and_version(
      logfile_group.get(), logfile_group_id, logfile_group_version);

  // Assign object type as logfile group
  ndb_dd_disk_data_set_object_type(logfile_group.get()->se_private_data(),
                                   object_type::LOGFILE_GROUP);

  // Write changes to dictionary.
  if (m_client->store(logfile_group.get())) {
    return false;
  }

  return true;
}

bool Ndb_dd_client::install_undo_file(const char *logfile_group_name,
                                      const char *undo_file_name) {
  DBUG_TRACE;

  // Read logfile group from DD
  dd::Tablespace *new_logfile_group_def = nullptr;
  if (m_client->acquire_for_modification(logfile_group_name,
                                         &new_logfile_group_def))
    return false;

  if (!new_logfile_group_def) return false;

  ndb_dd_disk_data_add_file(new_logfile_group_def, undo_file_name);

  // Write changes to dictionary.
  if (m_client->update(new_logfile_group_def)) {
    return false;
  }

  return true;
}

bool Ndb_dd_client::drop_logfile_group(const char *logfile_group_name,
                                       bool fail_if_not_exists) {
  DBUG_TRACE;

  /*
   * Logfile groups are stored as tablespaces in the DD.
   * This is acceptable since the only reason for storing
   * them in the DD is to ensure that INFORMATION_SCHEMA
   * is aware of their presence. Thus, rather than
   * extending DD, we use tablespaces since they resemble
   * logfile groups in terms of metadata structure
   */

  const dd::Tablespace *existing = nullptr;
  if (m_client->acquire(logfile_group_name, &existing)) {
    return false;
  }

  if (existing == nullptr) {
    // Logfile group does not exist
    if (fail_if_not_exists) {
      return false;
    }
    return true;
  }

  if (m_client->drop(existing)) {
    return false;
  }

  return true;
}

bool Ndb_dd_client::get_schema_uuid(dd::String_type *value) const {
  DBUG_TRACE;

  // Schema UUID will be stored in ndb_schema table definition in DD
  const dd::Table *table = nullptr;
  if (m_client->acquire(Ndb_schema_dist_table::DB_NAME.c_str(),
                        Ndb_schema_dist_table::TABLE_NAME.c_str(), &table)) {
    assert(false);
    return false;
  }

  if (table == nullptr) {
    // Table doesn't exists. This is OK as it might happen
    // if the function is called before ndb_schema is created.
    return true;
  }

  if (!ndb_dd_table_get_schema_uuid(table, value)) {
    // Table has invalid Schema UUID
    return false;
  }

  return true;
}

bool Ndb_dd_client::update_schema_uuid(const char *value) const {
  DBUG_TRACE;

  // Schema UUID should be updated in ndb_schema table definition in DD
  dd::Table *table = nullptr;
  if (m_client->acquire_for_modification(
          Ndb_schema_dist_table::DB_NAME.c_str(),
          Ndb_schema_dist_table::TABLE_NAME.c_str(), &table)) {
    assert(false);
    return false;
  }

  if (table == nullptr) {
    // Table does not exist in DD
    assert(false);
    return false;
  }

  // Set the schema uuid value to the table object
  ndb_dd_table_set_schema_uuid(table, value);

  if (m_client->update(table)) {
    assert(false);
    return false;
  }

  return true;
}

/**
  Lock and add the given referenced table to the set of
  referenced tables maintained by the invalidator.

  @param schema_name  Schema name of the table.
  @param table_name   Name of the table.

  @return true        On success.
  @return false       Unable to lock the table to the list.
*/
bool Ndb_referenced_tables_invalidator::add_and_lock_referenced_table(
    const char *schema_name, const char *table_name) {
  auto result =
      m_referenced_tables.insert(std::make_pair(schema_name, table_name));
  if (result.second) {
    // New parent added to invalidator. Lock it down
    DBUG_PRINT("info", ("Locking '%s.%s'", schema_name, table_name));
    if (!m_dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
      DBUG_PRINT("error", ("Unable to acquire lock to parent table '%s.%s'",
                           schema_name, table_name));
      return false;
    }
  }
  return true;
}

/**
  Fetch the list of referenced tables to add from the local Data Dictionary
  if available and also from the NDB Dictionary if available. Then lock
  them and add them to the unique list maintained by the invalidator.

  @param schema_name          Schema name of the table.
  @param table_name           Name of the table.
  @param table_def            Table object from the DD
  @param skip_ndb_dict_fetch  Bool value. If true, skip fetching the
                              referenced tables from NDB. Default value is
                              false. NDB Dictionary fetch has to be skipped
                              if the DDL being distributed would have dropped
                              the table in NDB dictionary already (like drop
                              table) or if reading the NDB dictionary is
                              redundant as the DDL won't  be adding/dropping
                              any FKs(like rename table).

  @return true        On success.
  @return false       Fetching failed.
*/
bool Ndb_referenced_tables_invalidator::fetch_referenced_tables_to_invalidate(
    const char *schema_name, const char *table_name, const dd::Table *table_def,
    bool skip_ndb_dict_fetch) {
  DBUG_TRACE;

  DBUG_PRINT("info",
             ("Collecting parent tables of '%s.%s' that are to be invalidated",
              schema_name, table_name));

  if (table_def != nullptr) {
    /* Table exists in DD already. Lock and add the parents */
    for (const dd::Foreign_key *fk : table_def->foreign_keys()) {
      const char *parent_db = fk->referenced_table_schema_name().c_str();
      const char *parent_table = fk->referenced_table_name().c_str();
      if (strcmp(parent_db, schema_name) == 0 &&
          strcmp(parent_table, table_name) == 0) {
        // Given table is the parent of this FK. Skip adding.
        continue;
      }
      if (!add_and_lock_referenced_table(parent_db, parent_table)) {
        return false;
      }
    }
  }

  if (!skip_ndb_dict_fetch) {
    std::set<std::pair<std::string, std::string>> referenced_tables;

    /* fetch the foreign key definitions from NDB dictionary */
    if (!fetch_referenced_tables_from_ndb_dictionary(
            m_thd, schema_name, table_name, referenced_tables)) {
      return false;
    }

    /* lock and add any missing parents */
    for (auto const &parent_name : referenced_tables) {
      if (!add_and_lock_referenced_table(parent_name.first.c_str(),
                                         parent_name.second.c_str())) {
        return false;
      }
    }
  }

  return true;
}

/**
  @brief Invalidate all the referenced tables from the MySQL DD cache.

  @return true        On success.
  @return false       Invalidation failed.
*/
bool Ndb_referenced_tables_invalidator::invalidate() const {
  DBUG_TRACE;
  for (auto const &parent_it : m_referenced_tables) {
    // Invalidate the table from DD
    const char *schema_name = parent_it.first.c_str();
    const char *table_name = parent_it.second.c_str();
    DBUG_PRINT("info",
               ("Invalidating parent table '%s.%s'", schema_name, table_name));
    if (m_thd->dd_client()->invalidate(schema_name, table_name) != 0) {
      DBUG_PRINT("error", ("Unable to invalidate table '%s.%s'", schema_name,
                           table_name));
      return false;
    }
  }
  return true;
}

#ifndef NDEBUG
/*
   Tables in DD are stored in the mysql.tables which is defined like this:

   CREATE TABLE mysql.tables (
     object_id BIGINT PRIMARY KEY,  // DD object's id
     schema_name VARCHAR(255),
     table_name VARCHAR(255),
     ..
     engine VARCHAR(255),
     se_private_id INT,
     se_private_data JSON,
     UNIQUE(engine, se_private_id), // Unique index preventing each SE to
                                    // store more than one table with same
                                    // se_private_id
   )
   For each table in NDB, the ndbcluster plugin stores
   the NDB table's id and version in the DD using:
     - se_private_id -> the NDB table id
     - se_private_data["object_version"] -> the NDB table version
   For example, the hypothetical NDB table 'db1.t1' with NDB table id 37 and
   version 9 would be returned like this using SQL:
    SELECT .. FROM mysql.tables WHERE
      schema_name = db1 AND table_name = t1;
        object_id: 479
        schema_name: db1
        table_name: t1
        ...
        engine = ndbcluster
        se_private_id = 37
        se_private_data = {
           object_version: 9
        }
*/
bool Ndb_dd_client::dump_NDB_tables() {
  fprintf(stderr, "Ndb_dd_client::dump_NDB_tables\n");
  fprintf(stderr,
          "| object_id | db_name | table_name | ... | engine | se_private_id | "
          "se_private_data |\n");
  // Fetch list of schemas in DD
  std::vector<std::string> schema_names;
  if (!fetch_schema_names(&schema_names)) {
    return true;
  }
  // Iterate over each schema in DD and show all NDB tables
  for (const std::string &schema_name : schema_names) {
    const dd::Schema *schema = nullptr;
    if (!mdl_lock_schema(schema_name.c_str()) ||
        m_client->acquire(schema_name.c_str(), &schema)) {
      return true;
    }
    assert(schema);
    std::vector<const dd::Table *> ndb_tables;
    if (m_client->fetch_schema_components(schema, &ndb_tables)) {
      return true;
    }
    for (const dd::Table *table_def : ndb_tables) {
      if (table_def->engine() != "ndbcluster") continue;
      const Ndb_dd_handle ndb_id = ndb_dd_table_get_spi_and_version(table_def);
      assert(table_def->se_private_id() == ndb_id.spi);
      fprintf(stderr,
              "| %llu | %s | %s | ... | %s | %llu | { object_version: %d} |\n",
              table_def->id(), schema_name.c_str(), table_def->name().c_str(),
              "ndbcluster", ndb_id.spi, ndb_id.version);
    }
  }
  return false;
}

bool Ndb_dd_client::dbug_shuffle_spi_for_NDB_tables() {
  fprintf(stderr, "Ndb_dd_client::dbug_shuffle_spi_for_NDB_tables\n");
  // Fetch list of schemas in DD
  std::vector<std::string> schema_names;
  if (!fetch_schema_names(&schema_names)) {
    return true;
  }

  dd::Object_id old_id = 0;
  // Iterate over each schema in DD
  for (const std::string &schema_name : schema_names) {
    const dd::Schema *schema = nullptr;
    if (!mdl_lock_schema(schema_name.c_str()) ||
        m_client->acquire(schema_name.c_str(), &schema)) {
      return true;
    }
    assert(schema);
    std::vector<const dd::Table *> ndb_tables;
    if (m_client->fetch_schema_components(schema, &ndb_tables)) {
      return true;
    }
    for (const dd::Table *table_def : ndb_tables) {
      if (table_def->engine() != "ndbcluster") {
        // Skip non NDB tables
        continue;
      }
      if (!mdl_locks_acquire_exclusive(schema_name.c_str(),
                                       table_def->name().c_str())) {
        assert(false);
        return true;
      }
      const Ndb_dd_handle ndb_id = ndb_dd_table_get_spi_and_version(table_def);
      const dd::Object_id new_id = old_id;
      old_id = ndb_id.spi;

      fprintf(stderr, " %s: %llu -> %llu\n", table_def->name().c_str(), old_id,
              new_id);

      if (!set_object_id_and_version_in_table(schema_name.c_str(),
                                              table_def->name().c_str(), new_id,
                                              ndb_id.version)) {
        // Failure to update can happen when there is already another table
        // using the decreased se_private_id, skip these tables for now and
        // thus cause only some tables to later need to be reinstalled from NDB
        // NOTE! To update _all_ tables it would be necessary to update tables
        // in sorted order, starting from table with lowest se_private_id
        const int error = m_thd->get_stmt_da()->mysql_errno();
        assert(error == 1062);
        fprintf(stderr, "Failed to update %s, error: %u\n",
                table_def->name().c_str(), error);
        continue;
      }
    }
  }
  // Save updated table definitions to DD
  commit();
  return false;
}

#endif
