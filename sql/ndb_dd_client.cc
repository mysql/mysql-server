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

#include "sql/ndb_dd_client.h"

#include <assert.h>
#include "my_dbug.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/dd.h"
#include "sql/dd/dd_table.h"
#include "sql/dd/types/table.h"
#include "sql/dd/types/schema.h"
#include "sql/mdl.h"            // MDL_*
#include "sql/ndb_dd_sdi.h"
#include "sql/ndb_dd_table.h"
#include "sql/query_options.h"  // OPTION_AUTOCOMMIT
#include "sql/sql_class.h"      // THD
#include "sql/system_variables.h"
#include "sql/transaction.h"    // trans_*


Ndb_dd_client::Ndb_dd_client(THD* thd) :
  m_thd(thd),
  m_client(thd->dd_client()),
  m_save_option_bits(0),
  m_comitted(false)
{
  disable_autocommit();

  // Create dictionary client auto releaser, stored as
  // opaque pointer in order to avoid including all of
  // Dictionary_client in the ndb_dd_client header file
  m_auto_releaser =
      (void*)new dd::cache::Dictionary_client::Auto_releaser(m_client);
}


Ndb_dd_client::~Ndb_dd_client()
{

  // Automatically release acquired MDL locks
  mdl_locks_release();

  // Automatically restore the option_bits in THD if they have
  // been modified
  if (m_save_option_bits)
    m_thd->variables.option_bits = m_save_option_bits;

  // Automatically rollback unless commit has been called
  if (!m_comitted)
    rollback();

  // Free the dictionary client auto releaser
  dd::cache::Dictionary_client::Auto_releaser* ar =
      (dd::cache::Dictionary_client::Auto_releaser*)m_auto_releaser;
  delete ar;
}


bool
Ndb_dd_client::mdl_lock_table(const char* schema_name,
                                 const char* table_name)
{
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&schema_request,
                   MDL_key::SCHEMA, schema_name, "", MDL_INTENTION_EXCLUSIVE,
                   MDL_EXPLICIT);
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, schema_name, table_name, MDL_SHARED,
                   MDL_EXPLICIT);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&mdl_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout))
  {
    return false;
  }

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(schema_request.ticket);
  m_acquired_mdl_tickets.push_back(mdl_request.ticket);

  return true;
}


bool
Ndb_dd_client::mdl_lock_schema(const char* schema_name)
{
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_REQUEST_INIT(&schema_request,
                   MDL_key::SCHEMA, schema_name, "", MDL_INTENTION_EXCLUSIVE,
                   MDL_EXPLICIT);
  mdl_requests.push_front(&schema_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout))
  {
    return false;
  }

  // Remember ticket of the acquired mdl lock
  m_acquired_mdl_tickets.push_back(schema_request.ticket);

  return true;
}

bool
Ndb_dd_client::mdl_locks_acquire_exclusive(const char* schema_name,
                                     const char* table_name)
{
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request mdl_request;

  MDL_REQUEST_INIT(&schema_request,
                   MDL_key::SCHEMA, schema_name, "", MDL_INTENTION_EXCLUSIVE,
                   MDL_EXPLICIT);
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, schema_name, table_name, MDL_EXCLUSIVE,
                   MDL_EXPLICIT);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&mdl_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout))
  {
    return false;
  }

  // Remember tickets of the acquired mdl locks
  m_acquired_mdl_tickets.push_back(schema_request.ticket);
  m_acquired_mdl_tickets.push_back(mdl_request.ticket);

  return true;
}


void Ndb_dd_client::mdl_locks_release()
{
  for (MDL_ticket* ticket : m_acquired_mdl_tickets)
  {
    m_thd->mdl_context.release_lock(ticket);
  }
}


void Ndb_dd_client::disable_autocommit()
{
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

  m_thd->variables.option_bits&= ~OPTION_AUTOCOMMIT;
  m_thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;

}


void Ndb_dd_client::commit()
{
  trans_commit_stmt(m_thd);
  trans_commit(m_thd);
  m_comitted = true;
}


void Ndb_dd_client::rollback()
{
  trans_rollback_stmt(m_thd);
  trans_rollback(m_thd);
}


bool
Ndb_dd_client::get_engine(const char* schema_name,
                          const char* table_name,
                          dd::String_type* engine)
{
  const dd::Table *existing= nullptr;
  if (m_client->acquire(schema_name, table_name, &existing))
  {
    return false;
  }

  if (existing == nullptr)
  {
    // Table does not exist in DD
    return false;
  }

  *engine = existing->engine();

  return true;
}


bool
Ndb_dd_client::rename_table(const char* old_schema_name,
                            const char* old_table_name,
                            const char* new_schema_name,
                            const char* new_table_name,
                            int new_table_id, int new_table_version)
{
  // Read new schema from DD
  const dd::Schema *new_schema= nullptr;
  if (m_client->acquire(new_schema_name, &new_schema))
  {
    return false;
  }
  if (new_schema == nullptr)
  {
    // Database does not exist, unexpected
    DBUG_ASSERT(false);
    return false;
  }

  // Read table from DD
  dd::Table *to_table_def= nullptr;
  if (m_client->acquire_for_modification(old_schema_name, old_table_name,
                                         &to_table_def))
    return false;

  // Set schema id and table name
  to_table_def->set_schema_id(new_schema->id());
  to_table_def->set_name(new_table_name);

  ndb_dd_table_set_object_id_and_version(to_table_def,
                                         new_table_id, new_table_version);

  // Rename foreign keys
  if (dd::rename_foreign_keys(old_table_name, to_table_def))
  {
    // Failed to rename foreign keys or commit/rollback, unexpected
    DBUG_ASSERT(false);
    return false;
  }

  // Save table in DD
  if (m_client->update(to_table_def))
  {
    // Failed to save, unexpected
    DBUG_ASSERT(false);
    return false;
  }

  return true;
}


bool
Ndb_dd_client::drop_table(const char* schema_name,
                          const char* table_name)

{
  const dd::Table *existing= nullptr;
  if (m_client->acquire(schema_name, table_name, &existing))
  {
    return false;
  }

  if (existing == nullptr)
  {
    // Table does not exist
    return false;
  }

  DBUG_PRINT("info", ("dropping existing table"));
  if (m_client->drop(existing))
  {
    // Failed to drop existing
    DBUG_ASSERT(false); // Catch in debug, unexpected error
    return false;
  }
  return true;
}


bool
Ndb_dd_client::install_table(const char* schema_name, const char* table_name,
                             const dd::sdi_t& sdi,
                             int ndb_table_id, int ndb_table_version,
                             bool force_overwrite)
{
  const dd::Schema *schema= nullptr;

  if (m_client->acquire(schema_name, &schema))
  {
    return false;
  }
  if (schema == nullptr)
  {
    DBUG_ASSERT(false); // Database does not exist
    return false;
  }

  std::unique_ptr<dd::Table> install_table{dd::create_object<dd::Table>()};
  if (ndb_dd_sdi_deserialize(m_thd, sdi, install_table.get()))
  {
    return false;
  }

  // Verify that table_name in the unpacked table definition
  // matches the table name to install
  DBUG_ASSERT(install_table->name() == table_name);

  // Verify that table defintion unpacked from NDB
  // does not have any se_private fields set, those will be set
  // from the NDB table metadata
  DBUG_ASSERT(install_table->se_private_id() == dd::INVALID_OBJECT_ID);
  DBUG_ASSERT(install_table->se_private_data().raw_string() == "");

  // Assign the id of the schema to the table_object
  install_table->set_schema_id(schema->id());

  // Asign NDB id and version of the table
  ndb_dd_table_set_object_id_and_version(install_table.get(),
                                         ndb_table_id, ndb_table_version);

  const dd::Table *existing= nullptr;
  if (m_client->acquire(schema_name, table_name, &existing))
  {
    return false;
  }

  if (existing != nullptr)
  {
    // Get id and version of existing table
    int object_id, object_version;
    if (!ndb_dd_table_get_object_id_and_version(existing,
                                                object_id, object_version))
    {
      DBUG_PRINT("error", ("Could not extract object_id and object_version "
                           "from table definition"));
      DBUG_ASSERT(false);
      return false;
    }

    // Check that id and version of the existing table in DD
    // matches NDB, otherwise it's a programming error
    // not to request "force_overwrite"
    if (ndb_table_id == object_id &&
        ndb_table_version == object_version)
    {

      // Table is already installed, with same id and version
      // return sucess
      return true;
    }

    // Table already exists
    if (!force_overwrite)
    {
      // Don't overwrite existing table
      DBUG_ASSERT(false);
      return false;
    }

    // Continue and remove the old table before
    // installing the new
    DBUG_PRINT("info", ("dropping existing table"));
    if (m_client->drop(existing))
    {
      // Failed to drop existing
      DBUG_ASSERT(false); // Catch in debug, unexpected error
      return false;
    }
  }

  if (m_client->store(install_table.get()))
  {
    DBUG_ASSERT(false); // Failed to store
    return false;
  }

  return true;
}

bool
Ndb_dd_client::get_table(const char *schema_name, const char *table_name,
                         const dd::Table **table_def)
{
  if (m_client->acquire(schema_name, table_name, table_def))
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), schema_name, table_name);
    return false;
  }
  return true;
}


bool
Ndb_dd_client::fetch_schema_names(std::vector<std::string>* names)
{
  DBUG_ENTER("Ndb_dd_client::fetch_schema_names");

  std::vector<const dd::Schema*> schemas;
  if (m_client->fetch_global_components(&schemas))
  {
    DBUG_RETURN(false);
  }

  for (const dd::Schema* schema : schemas)
  {
    names->push_back(schema->name().c_str());
  }
  DBUG_RETURN(true);
}


bool
Ndb_dd_client::get_ndb_table_names_in_schema(const char* schema_name,
                                           std::unordered_set<std::string>* names)
{
  DBUG_ENTER("Ndb_dd_client::get_ndb_table_names_in_schema");

  const dd::Schema* schema;
  if (m_client->acquire(schema_name, &schema))
  {
    // Failed to open the requested Schema object
    DBUG_RETURN(false);
  }

  std::vector<const dd::Table*> tables;
  if (m_client->fetch_schema_components(schema, &tables))
  {
    DBUG_RETURN(false);
  }

  for (const dd::Table* table: tables)
  {
    if (table->engine() != "ndbcluster")
    {
      // Skip non NDB tables
      continue;
    }

    // Lock the table in DD
    if (!mdl_lock_table(schema_name, table->name().c_str()))
    {
      // Failed to MDL lock table
      DBUG_RETURN(false);
    }

    names->insert(table->name().c_str());
  }
  DBUG_RETURN(true);
}


/*
  Check given schema for local tables(i.e not in NDB)

  @param        schema_name          Name of the schema to check for tables
  @param [out]  found_local_tables   Return parameter indicating if the schema
                                     contained local tables or not.

  @return       false  Failure
  @return       true   Success.
*/

bool
Ndb_dd_client::have_local_tables_in_schema(const char* schema_name,
                                           bool* found_local_tables)
{
  DBUG_ENTER("Ndb_dd_client::have_local_tables_in_schema");

  const dd::Schema* schema;
  if (m_client->acquire(schema_name, &schema))
  {
    // Failed to open the requested schema
    DBUG_RETURN(false);
  }

  if (schema == nullptr)
  {
    // The schema didn't exist, thus it can't have any local tables
    *found_local_tables = false;
    DBUG_RETURN(true);
  }

  std::vector<const dd::Table*> tables;
  if (m_client->fetch_schema_components(schema, &tables))
  {
    DBUG_RETURN(false);
  }

  // Assume no local table will be found, the loop below will
  // return on first table not in NDB
  *found_local_tables = false;

  for (const dd::Table* table: tables)
  {
    if (table->engine() != "ndbcluster")
    {
      // Found local table
      *found_local_tables = true;
      break;
    }
  }

  DBUG_RETURN(true);
}


bool
Ndb_dd_client::schema_exists(const char* schema_name,
                                  bool* schema_exists)
{
  DBUG_ENTER("Ndb_dd_client::schema_exists");

  const dd::Schema* schema;
  if (m_client->acquire(schema_name, &schema))
  {
    // Failed to open the requested schema
    DBUG_RETURN(false);
  }

  if (schema == nullptr)
  {
    // The schema didn't exist
    *schema_exists = false;
    DBUG_RETURN(true);
  }

  // The schema exists
  *schema_exists = true;
  DBUG_RETURN(true);
}

