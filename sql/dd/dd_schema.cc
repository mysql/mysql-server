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

#include <memory>                             // unique_ptr

#include "auth_common.h"
#include "binlog_event.h"
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/dd.h"                            // dd::get_dictionary
#include "dd/dictionary.h"                    // dd::Dictionary
#include "dd/types/schema.h"                  // dd::Schema
#include "debug_sync.h"                       // DEBUG_SYNC
#include "m_ctype.h"
#include "m_string.h"
#include "mdl.h"
#include "my_dbug.h"
#include "my_global.h"
#include "my_sys.h"
#include "mysqld.h"                           // lower_case_table_names
#include "mysqld_error.h"
#include "sql_class.h"                        // THD
#include "system_variables.h"
#include "transaction.h"                      // trans_commit

namespace dd {

bool schema_exists(THD *thd, const char *schema_name, bool *exists)
{
  DBUG_ENTER("dd_schema_exists");

  // We must make sure the schema is released and unlocked in the right order.
  Schema_MDL_locker mdl_handler(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch= NULL;
  bool error= mdl_handler.ensure_locked(schema_name) ||
              thd->dd_client()->acquire(schema_name, &sch);
  DBUG_ASSERT(exists);
  *exists= (sch != NULL);
  // Error has been reported by the dictionary subsystem.
  DBUG_RETURN(error);
}


bool create_schema(THD *thd, const char *schema_name,
                   const CHARSET_INFO *charset_info)
{
  // Create dd::Schema object.
  dd::Schema *sch_obj= dd::create_object<dd::Schema>();

  // Set schema name and collation id.
  sch_obj->set_name(schema_name);
  DBUG_ASSERT(charset_info);
  sch_obj->set_default_collation_id(charset_info->number);

  Disable_gtid_state_update_guard disabler(thd);

  // Wrap the pointer in a unique_ptr to ease memory management.
  std::unique_ptr<dd::Schema> wrapped_sch_obj(sch_obj);

  // Store the schema. Error will be reported by the dictionary subsystem.
  if (thd->dd_client()->store(wrapped_sch_obj.get()))
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
                  const CHARSET_INFO *charset_info)
{
  dd::cache::Dictionary_client *client= thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  // Get dd::Schema object.
  dd::Schema *new_sch_obj= nullptr;
  if (client->acquire_for_modification(schema_name, &new_sch_obj))
  {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  if (!new_sch_obj)
  {
    my_error(ER_NO_SUCH_DB, MYF(0), schema_name);
    return true;
  }

  // Set new collation ID.
  new_sch_obj->set_default_collation_id(charset_info->number);

  Disable_gtid_state_update_guard disabler(thd);

  // Update schema.
  if (client->update(new_sch_obj))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) || trans_commit(thd);
}


bool drop_schema(THD *thd, const char *schema_name)
{
  dd::cache::Dictionary_client *client= thd->dd_client();

  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  // Get the schema.
  const dd::Schema *sch_obj= NULL;
  DEBUG_SYNC(thd, "before_acquire_in_drop_schema");
  if (client->acquire(schema_name, &sch_obj))
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
  if (client->drop(sch_obj))
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
