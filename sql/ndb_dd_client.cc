/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_dd_client.h"

#include "sql_class.h"      // Using THD
#include "mdl.h"            // MDL_*
#include "transaction.h"    // trans_*

#include "dd/dd.h"
#include "ndb_dd_table.h"
#include "dd/types/table.h"
#include "dd/dd_table.h"
#include "ndb_dd_sdi.h"


Ndb_dd_client::Ndb_dd_client(THD* thd) :
  m_thd(thd),
  m_client(thd->dd_client()),
  m_mdl_locks_acquired(false),
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

  // Automatically release any acquired MDL locks
  if (m_mdl_locks_acquired)
    mdl_locks_release();
  assert(!m_mdl_locks_acquired);

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
Ndb_dd_client::mdl_locks_acquire(const char* schema_name,
                           const char* table_name)
{
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&schema_request,
                   MDL_key::SCHEMA, schema_name, "", MDL_INTENTION_EXCLUSIVE,
                   MDL_TRANSACTION);
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, schema_name, table_name, MDL_SHARED,
                   MDL_TRANSACTION);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&mdl_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout))
  {
    return false;
  }

  // Remember that MDL locks where acquired
  m_mdl_locks_acquired = true;

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
                   MDL_TRANSACTION);
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, schema_name, table_name, MDL_EXCLUSIVE,
                   MDL_TRANSACTION);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&mdl_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout))
  {
    return false;
  }

  // Remember that MDL locks where acquired
  m_mdl_locks_acquired = true;

  return true;
}


void Ndb_dd_client::mdl_locks_release()
{
  m_thd->mdl_context.release_transactional_locks();
  m_mdl_locks_acquired = false;
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
Ndb_dd_client::check_table_exists(const char* schema_name,
                                  const char* table_name,
                                  int& table_id, int& table_version)
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

  ndb_dd_table_get_object_id_and_version(existing, table_id, table_version);

  return true;

}


bool
Ndb_dd_client::get_engine(const char* schema_name,
                          const char* table_name,
                          dd::String_type* engine)
{
  dd::cache::Dictionary_client::Auto_releaser ar{m_client};

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
                            const char* new_table_name)
{
  dd::cache::Dictionary_client::Auto_releaser releaser(m_client);

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
  dd::Table *to_table_def= NULL;
  if (m_client->acquire_for_modification(old_schema_name, old_table_name,
                                         &to_table_def))
    return false;


  // Set schema id and table name
  to_table_def->set_schema_id(new_schema->id());
  to_table_def->set_name(new_table_name);

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
  dd::cache::Dictionary_client::Auto_releaser releaser{m_client};

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
  dd::cache::Dictionary_client::Auto_releaser ar{m_client};

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

  std::unique_ptr<dd::Table> table_object{dd::create_object<dd::Table>()};
  if (ndb_dd_sdi_deserialize(m_thd, sdi, table_object.get()))
  {
    return false;
  }

  // Verfiy that table defintion unpacked from NDB
  // does not have any se_private fields set, those will be set
  // from the NDB table metadata
  DBUG_ASSERT(table_object->se_private_id() == dd::INVALID_OBJECT_ID);
  DBUG_ASSERT(table_object->se_private_data().raw_string() == "");

  // Assign the id of the schema to the table_object
  table_object->set_schema_id(schema->id());

  // Asign NDB id and version of the table
  ndb_dd_table_set_object_id_and_version(table_object.get(),
                                         ndb_table_id, ndb_table_version);

  const dd::Table *existing= nullptr;
  if (m_client->acquire(schema->name(), table_object->name(), &existing))
  {
    return false;
  }

  if (existing != nullptr)
  {
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

  if (m_client->store(table_object.get()))
  {
    DBUG_ASSERT(false); // Failed to store
    return false;
  }

  return true;
}
