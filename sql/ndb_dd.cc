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

#include "dd/impl/sdi.h"

bool ndb_sdi_serialize(class THD *thd,
                       const dd::Table &table_def,
                       const char* schema_name,
                       dd::sdi_t& sdi)
{
  sdi = dd::serialize(thd, table_def, dd::String_type(schema_name));
  if (sdi.empty())
    return false; // Failed to serialize

  return true; // OK
}

#include "sql_class.h"
#include "transaction.h"
#include "mdl.h"            // MDL_*

#include "dd/dd.h"
#include "dd/types/schema.h"
#include "dd/types/table.h"
#include "dd/cache/dictionary_client.h" // dd::Dictionary_client
#include "dd/impl/sdi.h"           // dd::deserialize



bool ndb_dd_serialize_table(class THD *thd,
                            const char* schema_name,
                            const char* table_name,
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
                          *existing,
                          schema_name,
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
