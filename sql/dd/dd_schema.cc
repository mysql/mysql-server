/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "dd_schema.h"

#include "debug_sync.h"                       // DEBUG_SYNC
#include "handler.h"                          // HA_CREATE_INFO
#include "mysqld.h"                           // lower_case_table_names
#include "sql_class.h"                        // THD
#include "transaction.h"                      // trans_commit

#include "dd/dd.h"                            // dd::get_dictionary
#include "dd/dictionary.h"                    // dd::Dictionary
#include "dd/sdi.h"                           // dd::store_sdi
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/types/object_type.h"             // dd::Object_type
#include "dd/types/schema.h"                  // dd::Schema

#include <memory>                             // unique_ptr

namespace dd {

bool schema_exists(THD *thd, const char *schema_name, bool *exists)
{
  DBUG_ENTER("dd_schema_exists");

  // We must make sure the schema is released and unlocked in the right order.
  Schema_MDL_locker mdl_handler(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch= NULL;
  bool error= mdl_handler.ensure_locked(schema_name) ||
              thd->dd_client()->acquire<dd::Schema>(schema_name, &sch);
  DBUG_ASSERT(exists);
  *exists= (sch != NULL);
  // Error has been reported by the dictionary subsystem.
  DBUG_RETURN(error);
}


bool create_schema(THD *thd, const char *schema_name,
                   const HA_CREATE_INFO *create_info)
{
  // Create dd::Schema object.
  dd::Schema *sch_obj= dd::create_object<dd::Schema>();

  // Set schema name and collation id.
  sch_obj->set_name(schema_name);
  DBUG_ASSERT(create_info->default_table_charset);
  sch_obj->set_default_collation_id(
    create_info->default_table_charset->number);

  Disable_gtid_state_update_guard disabler(thd);

  // If this is the dd schema, "store" it temporarily in the shared cache.
  if (get_dictionary()->is_dd_schema_name(schema_name))
  {
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    thd->dd_client()->add_and_reset_id(sch_obj);
    thd->dd_client()->set_sticky(sch_obj, true);
    // Note that the object is now owned by the shared cache, so we cannot
    // delete it here.
    return false;
  }

  // Wrap the pointer in a unique_ptr to ease memory management.
  std::unique_ptr<dd::Schema> wrapped_sch_obj(sch_obj);

  // Store the schema. Error will be reported by the dictionary subsystem.
  if (thd->dd_client()->store(wrapped_sch_obj.get()) ||
      dd::store_sdi(thd, wrapped_sch_obj.get()))

  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) ||
         trans_commit(thd);
}


bool alter_schema(THD *thd, const char *schema_name,
                  const HA_CREATE_INFO *create_info)
{
  dd::cache::Dictionary_client *client= thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  // Get dd::Schema object.
  const dd::Schema *sch_obj= NULL;
  if (client->acquire<dd::Schema>(schema_name, &sch_obj))
  {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  if (!sch_obj)
  {
    my_error(ER_NO_SUCH_DB, MYF(0), schema_name);
    return true;
  }

  Sdi_updater update_sdi= make_sdi_updater(sch_obj);

  // Clone the schema object. The clone is owned here, and must be deleted
  // eventually.
  std::unique_ptr<dd::Schema> new_sch_obj(sch_obj->clone());

  // Set new collation ID.
  DBUG_ASSERT(create_info->default_table_charset);
  new_sch_obj->set_default_collation_id(
    create_info->default_table_charset->number);

  Disable_gtid_state_update_guard disabler(thd);

  // Update schema.
  if (client->update(&sch_obj, new_sch_obj.get()))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  if (update_sdi(thd, new_sch_obj.get()))
  {
    // At this point the cache already contains the new value, and
    // aborting the transaction will not undo this automatically. To
    // remedy this the schema is dropped to remove it from the
    // cache. This will obviously also remove it from the DD
    // tables, but this is ok since the removal will be rolled back
    // when the transaction aborts. Since there is exclusive MDL on
    // the schema, other threads will not see the removal, but will
    // have to reload the schema into the cache when they get MDL.
#ifndef DBUG_OFF
    bool drop_error=
#endif /* DBUG_OFF */
      client->drop(sch_obj);
    DBUG_ASSERT(drop_error == false);

    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) ||
         trans_commit(thd);
}


bool drop_schema(THD *thd, const char *schema_name)
{
  dd::cache::Dictionary_client *client= thd->dd_client();

  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  // Get the schema.
  const dd::Schema *sch_obj= NULL;
  DEBUG_SYNC(thd, "before_acquire_in_drop_schema");
  if (client->acquire<dd::Schema>(schema_name, &sch_obj))
  {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  DBUG_EXECUTE_IF("pretend_no_schema_in_drop_schema",
  {
    sch_obj= NULL;
  });

  if (!sch_obj)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name);
    return true;
  }

  Disable_gtid_state_update_guard disabler(thd);

  // Drop the schema.
  if (dd::remove_sdi(thd, sch_obj) ||
      client->drop(sch_obj))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) ||
         trans_commit(thd);
}


bool Schema_MDL_locker::ensure_locked(const char* schema_name)
{
  // Make sure we have at least an IX lock on the schema name.
  // Acquire a lock unless we already have it.
  char name_buf[NAME_LEN + 1];
  const char *converted_name= schema_name;
  if (lower_case_table_names == 2)
  {
    // Lower case table names == 2 is tested on OSX.
    /* purecov: begin tested */
    my_stpcpy(name_buf, converted_name);
    my_casedn_str(&my_charset_utf8_tolower_ci, name_buf);
    converted_name= name_buf;
    /* purecov: end */
  }

  // If we do not already have one, acquire a new lock.
  if (!m_thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::SCHEMA,
                                                      converted_name, "",
                                                      MDL_INTENTION_EXCLUSIVE))
  {
    // Create a request for an IX_lock with explicit duration
    // on the converted schema name.
    MDL_request mdl_request;
    MDL_REQUEST_INIT(&mdl_request, MDL_key::SCHEMA,
                     converted_name, "",
                     MDL_INTENTION_EXCLUSIVE,
                     MDL_EXPLICIT);

    // Acquire the lock request created above, and check if
    // acquisition fails (e.g. timeout or deadlock).
    if (m_thd->mdl_context.acquire_lock(&mdl_request,
                                        m_thd->variables.lock_wait_timeout))
    {
      DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
      return true;
    }
    m_ticket= mdl_request.ticket;
  }
  return false;
}


Schema_MDL_locker::~Schema_MDL_locker()
{
  if (m_ticket)
    m_thd->mdl_context.release_lock(m_ticket);
}

} // namespace dd
