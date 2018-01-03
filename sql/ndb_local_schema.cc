/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ndb_local_schema.h"

#include "sql/dd/dd_trigger.h" // dd::table_has_triggers
#include "sql/mdl.h"
#include "sql/ndb_dd.h"
#include "sql/ndb_log.h"
#include "sql/sql_class.h"
#include "sql/sql_trigger.h" // drop_all_triggers


bool Ndb_local_schema::Base::mdl_try_lock(void) const
{
  DBUG_ENTER("mdl_try_lock");
  DBUG_PRINT("enter", ("db: '%s, name: '%s'", m_db, m_name));

  MDL_request_list mdl_requests;
  MDL_request global_request;
  MDL_request schema_request;
  MDL_request mdl_request;

  MDL_REQUEST_INIT(&global_request,
                   MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                   MDL_STATEMENT);
  MDL_REQUEST_INIT(&schema_request,
                   MDL_key::SCHEMA, m_db, "", MDL_INTENTION_EXCLUSIVE,
                   MDL_TRANSACTION);
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, m_db, m_name, MDL_SHARED,
                   MDL_TRANSACTION);

  mdl_requests.push_front(&mdl_request);
  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&global_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       0 /* don't wait for lock */))
  {
    // Check that an error has been pushed to thd and then
    // clear it since this is just a _try lock_
    assert(m_thd->is_error());
    m_thd->clear_error();

    log_warning("Failed to acquire metadata lock");

    DBUG_RETURN(false);
  }
  DBUG_PRINT("info", ("acquired metadata lock"));
  DBUG_RETURN(true);
}


void Ndb_local_schema::Base::mdl_unlock(void)
{
  m_thd->mdl_context.release_transactional_locks();
}


void Ndb_local_schema::Base::log_warning(const char* fmt, ...) const
{
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  my_vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (m_push_warnings)
  {
    // Append the error which caused the error to thd's warning list
    push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG, "Ndb schema[%s.%s]: %s",
                        m_db, m_name, buf);
  }
  else
  {
    // Print the warning to log file
    ndb_log_warning("[%s.%s], %s",
                    m_db, m_name, buf);
  }
}


Ndb_local_schema::Base::Base(THD* thd, const char* db, const char* name) :
  m_thd(thd),
  m_db(db), m_name(name)
{
  /*
    System(or daemon) threads report error to log file
    all other threads use push_warning
  */
  m_push_warnings = (thd->get_command() != COM_DAEMON);

  m_have_mdl_lock= mdl_try_lock();
}


Ndb_local_schema::Base::~Base()
{
  // Release MDL locks
  if (m_have_mdl_lock)
  {
   DBUG_PRINT("info", ("releasing mdl lock"));
    mdl_unlock();
  }
}


Ndb_local_schema::Table::Table(THD* thd,
                               const char* db, const char* name) :
  Ndb_local_schema::Base(thd, db, name),
  m_has_triggers(false)
{
  DBUG_ENTER("Ndb_local_schema::Table");
  DBUG_PRINT("enter", ("name: '%s.%s'", db, name));

  // Check if there are trigger files
  // Ignore possible error from dd::table_has_triggers since
  // Caller has to check Diagnostics_area to detect whether error happened.
  (void)dd::table_has_triggers(thd, db, name, &m_has_triggers);

  DBUG_VOID_RETURN;
}


bool
Ndb_local_schema::Table::is_local_table(bool* exists) const
{
  dd::String_type engine;
  if (!ndb_dd_get_engine_for_table(m_thd, m_db, m_name, &engine))
  {
    // Can't fetch engine for table, table does not exist
    // and thus not local table
    *exists = false;
    return false;
  }

  *exists = true;

  if (engine == "ndbcluster")
  {
    // Table is marked as being in NDB, not a local table
    return false;
  }

  // This is a local table
  return true;
}


bool
Ndb_local_schema::Table::mdl_try_lock_exclusive(void) const
{
  DBUG_ENTER("mdl_try_lock_exclusive");

  // Upgrade lock on the table from shared to exclusive
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, m_db, m_name, MDL_EXCLUSIVE,
                   MDL_TRANSACTION);

  if (m_thd->mdl_context.acquire_lock(&mdl_request,
                                      0 /* don't wait for lock */))
  {
    log_warning("Failed to acquire exclusive metadata lock");
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}


void
Ndb_local_schema::Table::remove_table(void) const
{
  // Acquire exclusive MDL lock on the table
  if (!mdl_try_lock_exclusive())
  {
    return;
  }

  // Remove the table from DD
  if (!ndb_dd_drop_table(m_thd, m_db, m_name))
  {
    log_warning("Failed to drop table from DD");
    return;
  }

  if (m_has_triggers)
  {
    // NOTE! Should not call drop_all_triggers() here but rather
    // implement functionality to remove the triggers from DD
    // using DD API
    if (drop_all_triggers(m_thd, m_db, m_name))
    {
      log_warning("Failed to drop all triggers");
    }
  }

  // TODO Presumably also referencing views need to be updated here.
  // They should probably not be dropped by their references
  // to the now non existing table must be removed. Assumption is
  // that if user tries to open such a table an error
  // saying 'no such table' will be returned
}


bool
Ndb_local_schema::Table::mdl_try_lock_for_rename(const char* new_db,
                                                 const char* new_name) const
{
  DBUG_ENTER("mdl_try_lock_for_rename");
  DBUG_PRINT("enter", ("new_db: '%s, new_name: '%s'", new_db, new_name));

  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request mdl_request;

  MDL_REQUEST_INIT(&schema_request,
                   MDL_key::SCHEMA, new_db, "", MDL_INTENTION_EXCLUSIVE,
                   MDL_TRANSACTION);
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, new_db, new_name, MDL_EXCLUSIVE,
                   MDL_TRANSACTION);

  mdl_requests.push_front(&mdl_request);
  mdl_requests.push_front(&schema_request);
  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       0 /* don't wait for lock */))
  {
    // Check that an error has been pushed to thd and then
    // clear it since this is just a _try lock_
    assert(m_thd->is_error());
    m_thd->clear_error();

    log_warning("Failed to acquire exclusive metadata lock for %s.%s",
                new_db, new_name);

    DBUG_RETURN(false);
  }
  DBUG_PRINT("info", ("acquired metadata lock"));
  DBUG_RETURN(true);
}


void
Ndb_local_schema::Table::rename_table(const char* new_db,
                                      const char* new_name,
                                      int new_id, int new_version) const
{
  // Acquire exclusive MDL lock on the table
  if (!mdl_try_lock_exclusive())
  {
    return;
  }

  // Take write lock for the new table name
  if (!mdl_try_lock_for_rename(new_db, new_name))
  {
    log_warning("Failed to acquire MDL lock for rename");
    return;
  }

  if (!ndb_dd_rename_table(m_thd,
                           m_db, m_name,
                           new_db, new_name,
                           new_id, new_version))
  {
    log_warning("Failed to rename table in DD");
    return;
  }
}
