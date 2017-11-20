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

#include "ndb_dd.h"

#include "sql_class.h"
#include "transaction.h"
#include "mdl.h"            // MDL_*

#include "dd/dd.h"
#include "dd/types/schema.h"
#include "dd/types/table.h"
#include "dd/cache/dictionary_client.h" // dd::Dictionary_client
#include "dd/impl/sdi.h"           // dd::deserialize
#include "dd/dd_table.h"


bool ndb_sdi_serialize(THD *thd,
                       const dd::Table *table_def,
                       const char* schema_name,
                       const char* tablespace_name,
                       dd::sdi_t& sdi)
{
  // Require the table to be visible or else have temporary name
  DBUG_ASSERT(table_def->hidden() == dd::Abstract_table::HT_VISIBLE ||
              is_prefix(table_def->name().c_str(), tmp_file_prefix));

  MDL_ticket *mdl_ticket = NULL;
  if (tablespace_name &&
      !thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLESPACE,
                                                    "", tablespace_name,
                                                    MDL_INTENTION_EXCLUSIVE))
  {
    // Acquire mdl lock on the tables tablespace name since the DD is
    // acessed when serializing the table and everything which is aqcuired
    // need to be mdl locked.
    // NOTE! A normal handler::open() are called without any lock on the
    // tablespace name
    MDL_request mdl_request;
    MDL_REQUEST_INIT(&mdl_request,
                     MDL_key::TABLESPACE,
                     "", tablespace_name,
                     MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);

    if (thd->mdl_context.acquire_lock(&mdl_request,
                                      thd->variables.lock_wait_timeout))
    {
      return false; // Failed to acquire MDL lock for the tablespace
    }
    // Save the ticket to allow only this lock be released
    mdl_ticket= mdl_request.ticket;
  }

  sdi = dd::serialize(thd, *table_def, dd::String_type(schema_name));
  if (sdi.empty())
    return false; // Failed to serialize

  if (mdl_ticket)
    thd->mdl_context.release_lock(mdl_ticket);

  return true; // OK
}


/*
  Workaround for BUG#25657041

  During inplace alter table, the table has a temporary
  tablename and is also marked as hidden. Since the temporary
  name and hidden status is part of the serialized table
  definition, there's a mismatch down the line when this is
  stored as extra metadata in the NDB dictionary.

  The workaround for now involves setting the table as a user
  visible table and restoring the original table name
*/

void ndb_dd_fix_inplace_alter_table_def(dd::Table* table_def,
                                        const char* proper_table_name)
{
  DBUG_ENTER("ndb_dd_fix_inplace_alter_table_def");
  DBUG_PRINT("enter", ("table_name: %s", table_def->name().c_str()));
  DBUG_PRINT("enter", ("proper_table_name: %s", proper_table_name));

  // Check that the proper_table_name is not a temporary name
  DBUG_ASSERT(!is_prefix(proper_table_name, tmp_file_prefix));

  table_def->set_name(proper_table_name);
  table_def->set_hidden(dd::Abstract_table::HT_VISIBLE);

  DBUG_VOID_RETURN;
}


bool ndb_dd_serialize_table(class THD *thd,
                            const char* schema_name,
                            const char* table_name,
                            const char* tablespace_name,
                            dd::sdi_t& sdi)

{
  DBUG_ENTER("ndb_dd_serialize_table");

  // First aquire MDL locks
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

    if (thd->mdl_context.acquire_locks(&mdl_requests,
                                       thd->variables.lock_wait_timeout))
    {
      DBUG_RETURN(false);
    }

    // Acquired MDL on the schema and table involved
  }

  {
    /*
       Implementation details from which storage the DD uses leaks out
       and the user of these functions magically need to turn auto commit
       off.
       I.e as in sql_table.cc, execute_ddl_log_recovery()
           'Prevent InnoDB from automatically committing InnoDB transaction
            each time data-dictionary tables are closed after being updated.'

      Need to check how it can be hidden or if the THD settings need to be
      restored
    */
    thd->variables.option_bits&= ~OPTION_AUTOCOMMIT;
    thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;
  }

  {
    dd::cache::Dictionary_client* client= thd->dd_client();
    dd::cache::Dictionary_client::Auto_releaser ar{client};

    const dd::Schema *schema= nullptr;
    if (client->acquire(schema_name, &schema))
    {
      DBUG_RETURN(false);
    }
    if (schema == nullptr)
    {
      DBUG_ASSERT(false); // Database does not exist
      DBUG_RETURN(false);
    }

    const dd::Table *existing= nullptr;
    if (client->acquire(schema->name(), table_name, &existing))
    {
      DBUG_RETURN(false);
    }

    if (existing == nullptr)
    {
      // Table does not exist in DD
      DBUG_RETURN(false);
    }

    const bool serialize_res =
        ndb_sdi_serialize(thd,
                          existing,
                          schema_name,
                          tablespace_name,
                          sdi);
    if (!serialize_res)
    {
      // Failed to serialize table
      DBUG_ASSERT(false); // Should not happen
      DBUG_RETURN(false);
    }

    trans_commit_stmt(thd);
    trans_commit(thd);
  }

  // TODO Must be done in _all_ return paths
  thd->mdl_context.release_transactional_locks();

  DBUG_RETURN(true); // OK!
}


bool
ndb_dd_install_table(class THD *thd,
                     const char* schema_name,
                     const char* table_name,
                     const dd::sdi_t &sdi, bool force_overwrite)
{

  DBUG_ENTER("ndb_dd_install_table");

  // First aquire MDL locks
  // NOTE! Consider using the dd::Schema_MDL_locker here
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

    if (thd->mdl_context.acquire_locks(&mdl_requests,
                                       thd->variables.lock_wait_timeout))
    {
      DBUG_RETURN(false);
    }

    // Acquired MDL on the schema and table involved
  }

  /*
     Implementation details from which storage the DD uses leaks out
     and the user of these functions magically need to turn auto commit
     off by fiddeling with bits in the THD.
     I.e as in sql_table.cc, execute_ddl_log_recovery()
         'Prevent InnoDB from automatically committing InnoDB transaction
          each time data-dictionary tables are closed after being updated.'

     Turn off autocommit

     NOTE! Raw implementation since usage of the Disable_autocommit_guard
     class detects how the "ndb binlog injector thread loop" is holding a
     transaction open. It's not a good idea to flip these bits while
     transaction is open, but that is another problem.
  */
  ulonglong save_option_bits = thd->variables.option_bits;
  thd->variables.option_bits&= ~OPTION_AUTOCOMMIT;
  thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;

  {
    dd::cache::Dictionary_client* client= thd->dd_client();
    dd::cache::Dictionary_client::Auto_releaser ar{client};

    const dd::Schema *schema= nullptr;

    if (client->acquire(schema_name, &schema))
    {
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }
    if (schema == nullptr)
    {
      DBUG_ASSERT(false); // Database does not exist
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    std::unique_ptr<dd::Table> table_object{dd::create_object<dd::Table>()};
    if (dd::deserialize(thd, sdi, table_object.get()))
    {
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    // Assign the id of the schema to the table_object
    table_object->set_schema_id(schema->id());


    const dd::Table *existing= nullptr;
    if (client->acquire(schema->name(), table_object->name(), &existing))
    {
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    if (existing != nullptr)
    {
      // Table already exists
      if (!force_overwrite)
      {
        // Don't overwrite existing table
        DBUG_ASSERT(false);
        thd->variables.option_bits = save_option_bits;
        DBUG_RETURN(false);
      }

      // Continue and remove the old table before
      // installing the new
      DBUG_PRINT("info", ("dropping existing table"));
      if (client->drop(existing))
      {
        // Failed to drop existing
        DBUG_ASSERT(false); // Catch in debug, unexpected error
        thd->variables.option_bits = save_option_bits;
        DBUG_RETURN(false);
      }

    }

    if (client->store(table_object.get()))
    {
      // trans_rollback...
      DBUG_ASSERT(false); // Failed to store
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    trans_commit_stmt(thd);
    trans_commit(thd);
  }

  thd->variables.option_bits = save_option_bits;

  // TODO Must be done in _all_ return paths
  thd->mdl_context.release_transactional_locks();

  DBUG_RETURN(true); // OK!
}


bool
ndb_dd_drop_table(THD *thd,
                  const char *schema_name, const char *table_name)
{
  DBUG_ENTER("ndb_dd_drop_table");

  ulonglong save_option_bits = thd->variables.option_bits;
  thd->variables.option_bits&= ~OPTION_AUTOCOMMIT;
  thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;

  {
    dd::cache::Dictionary_client* client= thd->dd_client();
    dd::cache::Dictionary_client::Auto_releaser releaser{client};

    const dd::Table *existing= nullptr;
    if (client->acquire(schema_name, table_name, &existing))
    {
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    if (existing == nullptr)
    {
      // Table does not exist
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    DBUG_PRINT("info", ("dropping existing table"));
    if (client->drop(existing))
    {
      // Failed to drop existing
      DBUG_ASSERT(false); // Catch in debug, unexpected error
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    trans_commit_stmt(thd);
    trans_commit(thd);
  }

  thd->variables.option_bits = save_option_bits;

  DBUG_RETURN(true); // OK
}


bool
ndb_dd_rename_table(THD *thd,
                    const char *old_schema_name, const char *old_table_name,
                    const char *new_schema_name, const char *new_table_name)
{
  DBUG_ENTER("ndb_dd_rename_table");
  DBUG_PRINT("enter", ("old: '%s'.'%s'  new: '%s'.'%s'",
                       old_schema_name, old_table_name,
                       new_schema_name, new_table_name));

  dd::cache::Dictionary_client* client= thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  // Read new schema from DD
  const dd::Schema *new_schema= nullptr;
  if (client->acquire(new_schema_name, &new_schema))
  {
    DBUG_RETURN(false);
  }
  if (new_schema == nullptr)
  {
    // Database does not exist, unexpected
    DBUG_ASSERT(false);
    DBUG_RETURN(false);
  }

  // Read table from DD
  dd::Table *to_table_def= NULL;
  if (client->acquire_for_modification(old_schema_name, old_table_name,
                                       &to_table_def))
    DBUG_RETURN(false);


  // Set schema id and table name
  to_table_def->set_schema_id(new_schema->id());
  to_table_def->set_name(new_table_name);

  // Rename foreign keys
  if (dd::rename_foreign_keys(old_table_name, to_table_def))
  {
    // Failed to rename foreign keys or commit/rollback, unexpected
    DBUG_ASSERT(false);
    DBUG_RETURN(false);
  }

  // Save table in DD
  if (client->update(to_table_def))
  {
    // Failed to save, unexpected
    DBUG_ASSERT(false);
    DBUG_RETURN(false);
  }

  trans_commit_stmt(thd);
  trans_commit(thd);

  DBUG_RETURN(true); // OK
}


bool
ndb_dd_table_get_engine(THD *thd,
                        const char *schema_name,
                        const char *table_name,
                        dd::String_type* engine)
{
  DBUG_ENTER("ndb_dd_table_get_engine");

  ulonglong save_option_bits = thd->variables.option_bits;
  thd->variables.option_bits&= ~OPTION_AUTOCOMMIT;
  thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;

  {
    dd::cache::Dictionary_client* client= thd->dd_client();
    dd::cache::Dictionary_client::Auto_releaser releaser{client};

    const dd::Table *existing= nullptr;
    if (client->acquire(schema_name, table_name, &existing))
    {
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    if (existing == nullptr)
    {
      // Table does not exist
      thd->variables.option_bits = save_option_bits;
      DBUG_RETURN(false);
    }

    DBUG_PRINT("info", (("table '%s.%s' exists in DD, engine: '%s'"),
                        schema_name, table_name, existing->engine().c_str()));
    *engine = existing->engine();

    trans_commit_stmt(thd);
    trans_commit(thd);
  }

  thd->variables.option_bits = save_option_bits;

  DBUG_RETURN(true); // Table exist
}


void
ndb_dd_table_set_se_private_id(dd::Table* table_def, int private_id)
{
  table_def->set_se_private_id(private_id);
}
