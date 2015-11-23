/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/types/object_type.h"             // dd::Object_type
#include "dd/types/schema.h"                  // dd::Schema

#include <memory> // unique_ptr

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
  dd::Dictionary *dict= dd::get_dictionary();

  if (dict->is_dd_schema_name(schema_name))
    return false;

  // Create dd::Schema object.
  std::unique_ptr<dd::Schema> sch_obj(dd::create_object<dd::Schema>());

  // Set schema name and collation id.
  sch_obj->set_name(schema_name);
  DBUG_ASSERT(create_info->default_table_charset);
  sch_obj->set_default_collation_id(
    create_info->default_table_charset->number);

  Disable_gtid_state_update_guard disabler(thd);

  // Store the schema. Error will be reported by the dictionary subsystem.
  if (thd->dd_client()->store(sch_obj.get()))
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

  // Collation ID
  DBUG_ASSERT(create_info->default_table_charset);
  const_cast<dd::Schema*>(sch_obj)->set_default_collation_id(
    create_info->default_table_charset->number);

  Disable_gtid_state_update_guard disabler(thd);

  // Update schema.
  if (client->update(const_cast<dd::Schema*>(sch_obj)))
  {
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
  if (client->drop(const_cast<dd::Schema*>(sch_obj)))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) ||
         trans_commit(thd);
}


bool Schema_MDL_locker::is_lock_required(const char* schema_name)
{
  return mysqld_server_started &&
         my_strcasecmp(system_charset_info,
                       MYSQL_SCHEMA_NAME.str, schema_name) != 0;
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

  // If a lock is required, and we do not already have one, acquire a new lock.
  if (is_lock_required(converted_name) &&
      !m_thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::SCHEMA,
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
      DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
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
