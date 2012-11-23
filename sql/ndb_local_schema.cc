/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_local_schema.h"

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "sql_class.h"
#include "sql_table.h"
#include "mdl.h"

static const char *ndb_ext=".ndb";


bool Ndb_local_schema::Base::mdl_try_lock(void) const
{
  MDL_request_list mdl_requests;
  MDL_request global_request;
  MDL_request schema_request;
  MDL_request mdl_request;

  global_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                      MDL_STATEMENT);
  schema_request.init(MDL_key::SCHEMA, m_db, "", MDL_INTENTION_EXCLUSIVE,
                      MDL_TRANSACTION);
  mdl_request.init(MDL_key::TABLE, m_db, m_name, MDL_EXCLUSIVE,
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

    return false;
  }
  DBUG_PRINT("info", ("acquired metadata lock"));
  return true;
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
    push_warning_printf(m_thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_GET_ERRMSG, "Ndb schema[%s.%s]: %s",
                        m_db, m_name, buf);
  }
  else
  {
    // Print the warning to log file
    sql_print_warning("Ndb schema[%s.%s]: %s",
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
  m_push_warnings = (thd->command != COM_DAEMON);

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


bool
Ndb_local_schema::Table::file_exists(const char* ext) const
{
  char buf[FN_REFLEN + 1];
  build_table_filename(buf, sizeof(buf)-1,
                       m_db, m_name, ext, 0);

  if (my_access(buf, F_OK))
  {
    DBUG_PRINT("info", ("File '%s' does not exist", buf));
    return false;
  }

  DBUG_PRINT("info", ("File '%s' exist", buf));
  return true;
}


bool
Ndb_local_schema::Table::remove_file(const char* ext) const
{
  char buf[FN_REFLEN + 1];
  build_table_filename(buf, sizeof(buf)-1,
                       m_db, m_name, ext, 0);

  int error = my_delete(buf, 0);
  if (!error || errno == ENOENT)
    return true;

  log_warning("Failed to remove file '%s', errno: %d", buf, errno);
  return false;
}


bool
Ndb_local_schema::Table::rename_file(const char* new_db, const char* new_name,
                             const char* ext) const
{
  char from[FN_REFLEN + 1];
  build_table_filename(from, sizeof(from)-1,
                       m_db, m_name, ext, 0);

  char to[FN_REFLEN + 1];
  build_table_filename(to, sizeof(to) - 1, new_db, new_name, ext, 0);

  int error = my_rename(from, to, 0);
  if (!error)
    return true;

  log_warning("Failed to rename file '%s' to '%s', errno: %d",
            from, to, errno);
  return false;
}


// Read the engine type from .frm and return true if it says NDB
bool
Ndb_local_schema::Table::frm_engine_is_ndb(void) const
{
  char buf[FN_REFLEN + 1];
  build_table_filename(buf, sizeof(buf)-1,
                       m_db, m_name, reg_ext, 0);

  legacy_db_type engine_type;
  (void)dd_frm_type(m_thd, buf, &engine_type);
  DBUG_PRINT("info", ("engine_type: %d", engine_type));

  return (engine_type == DB_TYPE_NDBCLUSTER);
}


Ndb_local_schema::Table::Table(THD* thd,
                               const char* db, const char* name) :
  Ndb_local_schema::Base(thd, db, name),
  m_ndb_file_exist(false),
  m_has_triggers(false)
{
  DBUG_ENTER("Ndb_local_table");
  DBUG_PRINT("enter", ("name: '%s.%s'", db, name));

  // Check if .frm file exist
  m_frm_file_exist = file_exists(reg_ext);
  if (!m_frm_file_exist)
  {
    // Check for stray .ndb file
    assert(!file_exists(ndb_ext));
    DBUG_VOID_RETURN;
  }

  // Check if .ndb file exist
  m_ndb_file_exist = file_exists(ndb_ext);

  // Check if there are trigger files
  m_has_triggers = file_exists(TRG_EXT);

  DBUG_VOID_RETURN;
}


bool
Ndb_local_schema::Table::is_local_table(void) const
{
  if (m_frm_file_exist && !m_ndb_file_exist)
  {
    // The .frm exist but no .ndb file , this is a "local" table

    // Double check that the engine type in .frm doesn't say NDB
    assert(!frm_engine_is_ndb());

    return true;
  }
  return false;
}


void
Ndb_local_schema::Table::remove_table(void) const
{
  (void)remove_file(reg_ext);
  (void)remove_file(ndb_ext);

  if (m_has_triggers)
  {
    // Copy to buffers since 'drop_all_triggers' want char*
    char db_name_buf[FN_REFLEN + 1], table_name_buf[FN_REFLEN + 1];
    strmov(db_name_buf, m_db);
    strmov(table_name_buf, m_name);

    if (Table_triggers_list::drop_all_triggers(m_thd,
                                               db_name_buf,
                                               table_name_buf))
    {
      log_warning("Failed to drop all triggers");
    }
  }
}


void
Ndb_local_schema::Table::rename_table(const char* new_db,
                                      const char* new_name) const
{
  (void)rename_file(new_db, new_name, reg_ext);
  (void)rename_file(new_db, new_name, ndb_ext);

  if (m_has_triggers)
  {
    if (!have_mdl_lock())
    {
      // change_table_name requires an EXLUSIVE mdl lock
      // so if the mdl lock was not aquired, skip this part
      log_warning("Can't rename triggers, no mdl lock");
    }
    else
    {
      if (Table_triggers_list::change_table_name(m_thd,
                                                 m_db, m_name, m_name,
                                                 new_db, new_name))
      {
        log_warning("Failed to rename all triggers");
      }
    }
  }
}
