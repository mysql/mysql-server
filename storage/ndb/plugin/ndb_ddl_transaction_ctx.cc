/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements the interface defined in
#include "storage/ndb/plugin/ndb_ddl_transaction_ctx.h"

#include "my_dbug.h"
#include "sql/handler.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "storage/ndb/plugin/ndb_dd.h"
#include "storage/ndb/plugin/ndb_dd_client.h"
#include "storage/ndb/plugin/ndb_ddl_definitions.h"
#include "storage/ndb/plugin/ndb_name_util.h"
#include "storage/ndb/plugin/ndb_schema_dist.h"
#include "storage/ndb/plugin/ndb_table_guard.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"

void Ndb_DDL_transaction_ctx::log_create_table(const std::string &db_name,
                                               const std::string &table_name) {
  log_ddl_stmt(Ndb_DDL_stmt::CREATE_TABLE, db_name, table_name);
}

bool Ndb_DDL_transaction_ctx::rollback_create_table(
    const Ndb_DDL_stmt &ddl_stmt) {
  DBUG_TRACE;

  /* extract info from ddl_info */
  const std::vector<std::string> &ddl_info = ddl_stmt.get_info();
  assert(ddl_info.size() == 2);
  const char *db_name = ddl_info[0].c_str();
  const char *table_name = ddl_info[1].c_str();

  /* Prepare schema client for rollback if required */
  Thd_ndb *thd_ndb = get_thd_ndb(m_thd);
  Ndb_schema_dist_client schema_dist_client(m_thd);
  bool schema_dist_prepared = false;
  if (ddl_stmt.has_been_distributed()) {
    /* The stmt was distributed.
       So rollback should be distributed too.
       Prepare the schema client */
    schema_dist_prepared = schema_dist_client.prepare(db_name, table_name);
    if (!schema_dist_prepared) {
      /* Report the error and just drop it locally */
      thd_ndb->push_warning(
          "Failed to distribute rollback to connected servers.");
    }
  }

  DBUG_PRINT("info",
             ("Rollback : Dropping table '%s.%s'", db_name, table_name));

  DBUG_EXECUTE_IF("ndb_simulate_failure_during_rollback", {
    DBUG_SET("-d,ndb_simulate_failure_during_rollback");
    thd_ndb->push_warning("Failed to rollback after CREATE TABLE failure.");
    return false;
  });

  /* Drop the table created during this DDL execution */
  Ndb *ndb = thd_ndb->ndb;
  if (drop_table_impl(m_thd, ndb,
                      schema_dist_prepared ? &schema_dist_client : nullptr,
                      db_name, table_name)) {
    thd_ndb->push_warning("Failed to rollback after CREATE TABLE failure.");
    return false;
  }

  return true;
}

void Ndb_DDL_transaction_ctx::log_rename_table(
    const std::string &old_db_name, const std::string &old_table_name,
    const std::string &new_db_name, const std::string &new_table_name,
    const std::string &from, const std::string &to,
    const std::string &orig_sdi) {
  log_ddl_stmt(Ndb_DDL_stmt::RENAME_TABLE, old_db_name, old_table_name,
               new_db_name, new_table_name, from, to, orig_sdi);
}

bool Ndb_DDL_transaction_ctx::rollback_rename_table(
    const Ndb_DDL_stmt &ddl_stmt) {
  DBUG_TRACE;

  /* extract info from ddl_info */
  const std::vector<std::string> &ddl_info = ddl_stmt.get_info();
  assert(ddl_info.size() == 7);
  const char *old_db_name = ddl_info[0].c_str();
  const char *old_table_name = ddl_info[1].c_str();
  const char *new_db_name = ddl_info[2].c_str();
  const char *new_table_name = ddl_info[3].c_str();
  const char *from = ddl_info[4].c_str();
  const char *to = ddl_info[5].c_str();
  m_original_sdi_for_rename = ddl_info[6];

  DBUG_PRINT("info",
             ("Rollback : Renaming table '%s.%s' to '%s.%s'", new_db_name,
              new_table_name, old_db_name, old_table_name));

  /* Load the table from NDB */
  Thd_ndb *thd_ndb = get_thd_ndb(m_thd);
  Ndb *ndb = thd_ndb->ndb;
  Ndb_table_guard ndbtab_g(ndb, new_db_name, new_table_name);
  const NdbDictionary::Table *renamed_table = ndbtab_g.get_table();
  if (renamed_table == nullptr) {
    thd_ndb->push_ndb_error_warning(ndbtab_g.getNdbError());
    thd_ndb->push_warning("Failed to rename table during rollback.");
    return false;
  }

  /* Various parameters to send to rename_table_impl.
     Deduct all these from the available information */
  bool real_rename = false;
  std::string real_rename_db_buff, real_rename_table_buff;
  bool distribute_table_changes = false;
  bool new_table_name_is_temp = ndb_name_is_temp(new_table_name);
  bool old_table_name_is_temp = ndb_name_is_temp(old_table_name);

  /* Decide whether the events have to be dropped and/or created. The new_name
     is the source and the old_name is the target. So, if the new_name is not
     temp, we would have to drop the events and if the old_name is not temp,
     we would have to create the events. */
  const bool drop_events = !new_table_name_is_temp;
  const bool create_events = !old_table_name_is_temp;

  /* Deduce the real rename parameter values. They are set only when a real
     rename, during the actual DDL transaction, got distributed to the
     participants. When these are set during rollback, they distribute the
     rollback of the table rename to the participants. */
  if (ddl_stmt.has_been_distributed() && !old_table_name_is_temp &&
      !new_table_name_is_temp) {
    /* This stmt was a simple RENAME and was distributed successfully. */
    real_rename = true;
    real_rename_db_buff = new_db_name;
    real_rename_table_buff = new_table_name;
    distribute_table_changes = true;
  } else if (!old_table_name_is_temp && new_table_name_is_temp) {
    /* This is the first rename of a COPY ALTER. It renamed the old table from
       the original name to a temp name. We need to retrieve the last RENAME
       of the ALTER to check if the ALTER involved renaming the table. */
    const Ndb_DDL_stmt *ndb_final_rename_stmt =
        retrieve_copy_alter_final_rename_stmt();
    if (ndb_final_rename_stmt != nullptr) {
      /* Found the final RENAME of the ALTER */
      const std::vector<std::string> &final_rename_ddl_info =
          ndb_final_rename_stmt->get_info();

      /* Extract info and use them to set the rename_table_impl parameters */
      assert(final_rename_ddl_info.size() == 7);
      std::string final_db_name = final_rename_ddl_info[2];
      std::string final_table_name = final_rename_ddl_info[3];
      if ((final_db_name.compare(old_db_name) != 0) ||
          (final_table_name.compare(old_table_name) != 0)) {
        /* The actual ALTER renamed the table. */
        real_rename = true;
        real_rename_db_buff = final_db_name;
        real_rename_table_buff = final_table_name;
      }
    }
    /* Always distribute this phase of ALTER during rollback - this is to
       make sure that all the participant's DD gets updated with latest table
       version after rollback. */
    distribute_table_changes = true;
  }

  const char *real_rename_db =
      real_rename_db_buff.empty() ? nullptr : real_rename_db_buff.c_str();
  const char *real_rename_table =
      real_rename_table_buff.empty() ? nullptr : real_rename_table_buff.c_str();

  /* Prepare the schema client if required */
  bool schema_dist_prepared = false;
  Ndb_schema_dist_client schema_dist_client(m_thd);
  if (distribute_table_changes) {
    if (real_rename) {
      /* This is also a rename. Prepare the schema client */
      schema_dist_prepared = schema_dist_client.prepare_rename(
          real_rename_db, real_rename_table, old_db_name, old_table_name);
    } else {
      /* Prepare the schema client for an ALTER */
      schema_dist_prepared =
          schema_dist_client.prepare(old_db_name, old_table_name);
    }
    if (!schema_dist_prepared) {
      /* Report the error and carry on */
      thd_ndb->push_warning(
          "Failed to distribute rollback to connected servers.");
    }
  }

  /* Rename back the table.
     The rename is done from new_name to old_name as this is a rollback. */
  if (rename_table_impl(
          m_thd, ndb, schema_dist_prepared ? &schema_dist_client : nullptr,
          renamed_table,
          nullptr,  // table_def
          to, from, new_db_name, new_table_name, old_db_name, old_table_name,
          real_rename, real_rename_db, real_rename_table, drop_events,
          create_events, distribute_table_changes)) {
    thd_ndb->push_warning("Failed to rollback rename table.");
    return false;
  }

  return true;
}

bool Ndb_DDL_transaction_ctx::update_table_id_and_version_in_DD(
    const char *schema_name, const char *table_name, int object_id,
    int object_version) {
  DBUG_TRACE;
  Ndb_dd_client dd_client(m_thd);
  Thd_ndb *thd_ndb = get_thd_ndb(m_thd);

  /* Lock the table exclusively */
  if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
    thd_ndb->push_warning(
        "Failed to acquire exclusive lock on table : '%s.%s' during rollback",
        schema_name, table_name);
    return false;
  }

  /* Update the table with new object id and version */
  if (!dd_client.set_object_id_and_version_in_table(
          schema_name, table_name, object_id, object_version)) {
    thd_ndb->push_warning(
        "Failed to update id and version of table : '%s.%s' during rollback",
        schema_name, table_name);
    return false;
  }

  /* commit the changes */
  dd_client.commit();

  return true;
}

bool Ndb_DDL_transaction_ctx::post_ddl_hook_rename_table(
    const Ndb_DDL_stmt &ddl_stmt) {
  DBUG_TRACE;
  assert(m_ddl_status != DDL_IN_PROGRESS);

  if (m_ddl_status == DDL_COMMITED) {
    /* DDL committed. Nothing to do */
    return true;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(m_thd);
  Ndb *ndb = thd_ndb->ndb;

  /* extract info from ddl_info */
  const std::vector<std::string> &ddl_info = ddl_stmt.get_info();
  const char *db_name = ddl_info[0].c_str();
  const char *table_name = ddl_info[1].c_str();

  if (ndb_name_is_temp(table_name)) {
    /* The target table was a temp table. No need to update id and version */
    return true;
  }

  /* Load the table from NDB */
  Ndb_table_guard ndbtab_g(ndb, db_name, table_name);
  const NdbDictionary::Table *ndb_table = ndbtab_g.get_table();
  if (ndb_table == nullptr) {
    thd_ndb->push_ndb_error_warning(ndbtab_g.getNdbError());
    thd_ndb->push_warning("Unable to load table during rollback");
    return false;
  }

  /* Update table id and version */
  if (!update_table_id_and_version_in_DD(db_name, table_name,
                                         ndb_table->getObjectId(),
                                         ndb_table->getObjectVersion())) {
    return false;
  }

  return true;
}

void Ndb_DDL_transaction_ctx::log_drop_temp_table(
    const std::string &db_name, const std::string &table_name) {
  log_ddl_stmt(Ndb_DDL_stmt::DROP_TABLE, db_name, table_name);
}

bool Ndb_DDL_transaction_ctx::post_ddl_hook_drop_temp_table(
    const Ndb_DDL_stmt &ddl_stmt) {
  DBUG_TRACE;
  assert(m_ddl_status != DDL_IN_PROGRESS);

  if (m_ddl_status == DDL_ROLLED_BACK) {
    /* DDL was rollbacked. Nothing to do */
    return true;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(m_thd);
  Ndb *ndb = thd_ndb->ndb;

  /* extract info from ddl_info */
  const std::vector<std::string> &ddl_info = ddl_stmt.get_info();
  assert(ddl_info.size() == 2);
  const char *db_name = ddl_info[0].c_str();
  const char *table_name = ddl_info[1].c_str();

  /* Verify that the table is a table with temporary name. */
  if (!ndb_name_is_temp(table_name)) {
    assert(false);
    return false;
  }

  DBUG_PRINT("info", ("Dropping table '%s.%s'", db_name, table_name));

  /* Finally drop the temp table as the DDL has been committed  */
  if (drop_table_impl(m_thd, ndb, nullptr, db_name, table_name) != 0) {
    thd_ndb->push_warning("Failed to drop a temp table.");
    return false;
  }

  /* The table has been dropped successfully. Only thing remaining is handling
     the special case where `ALTER TABLE .. ENGINE` is requested. So exit and
     return if this DDL is not a ALTER query. */
  if (thd_sql_command(m_thd) != SQLCOM_ALTER_TABLE) {
    return true;
  }

  /* Detect the special case which occurs when a table is altered to another
     engine. In such case the altered table has been renamed to a temporary
     name in the same engine before copying the data to the new table in the
     other engine. When copying is successful, the original table
     (which now has a temporary name) is asked to be dropped. Since this table
     has a temporary name, the actual drop was done only after a successful
     commit as a part of this function. Now that the drop is done, inform the
     participants that the original table is no longer in NDB. Unfortunately
     the original table name is not available in this function, but it's
     possible to look that up via THD. */
  const HA_CREATE_INFO *create_info = m_thd->lex->create_info;
  if ((create_info->used_fields & HA_CREATE_USED_ENGINE) &&
      create_info->db_type != ndbcluster_hton) {
    DBUG_PRINT("info", ("ALTER to different engine = '%s' detected",
                        ha_resolve_storage_engine_name(create_info->db_type)));

    const char *orig_db_name = m_thd->lex->query_block->get_table_list()->db;
    const char *orig_table_name =
        m_thd->lex->query_block->get_table_list()->table_name;
    DBUG_PRINT("info",
               ("original table name: '%s.%s'", orig_db_name, orig_table_name));

    Ndb_schema_dist_client schema_dist_client(m_thd);

    /* Prepare the schema client */
    if (!schema_dist_client.prepare(orig_db_name, orig_table_name)) {
      thd_ndb->push_warning("Failed to distribute 'DROP TABLE '%s.%s''",
                            orig_db_name, orig_table_name);
      return false;
    }

    /* Do a drop in all connected servers */
    if (!schema_dist_client.drop_table(orig_db_name, orig_table_name, 0, 0)) {
      thd_ndb->push_warning("Failed to distribute 'DROP TABLE '%s.%s''",
                            orig_db_name, orig_table_name);
      return false;
    }
  }

  return true;
}

const Ndb_DDL_stmt *
Ndb_DDL_transaction_ctx::retrieve_copy_alter_final_rename_stmt() {
  DBUG_TRACE;
  /* Loop all the logged stmts and find the copy alter info */
  for (auto it = m_executed_ddl_stmts.rbegin();
       it != m_executed_ddl_stmts.rend(); ++it) {
    Ndb_DDL_stmt &ddl_stmt = *it;
    switch (ddl_stmt.get_ddl_type()) {
      case Ndb_DDL_stmt::RENAME_TABLE: {
        const std::vector<std::string> &ddl_info = ddl_stmt.get_info();
        const char *old_table_name = ddl_info[1].c_str();
        const char *new_table_name = ddl_info[3].c_str();
        if (ndb_name_is_temp(old_table_name) &&
            !ndb_name_is_temp(new_table_name)) {
          /* This was a rename from #sql -> proper_name.
             This was the final rename of a COPY ALTER. */
          return &ddl_stmt;
        }
      } break;
      default:
        break;
    }
  }
  return nullptr;
}

void Ndb_DDL_transaction_ctx::commit() {
  DBUG_TRACE;
  assert(m_ddl_status == DDL_IN_PROGRESS);
  /* The schema changes would have been already committed internally to the NDB
     by the respective handler functions that made the change. So just update
     the status of the DDL and make note of the latest stmt on which the
     Server has requested a commit. */
  m_ddl_status = DDL_COMMITED;
  m_latest_committed_stmt = m_executed_ddl_stmts.size();
}

bool Ndb_DDL_transaction_ctx::rollback() {
  DBUG_TRACE;
  assert(m_ddl_status == DDL_IN_PROGRESS);

  bool result = true;
  m_ddl_status = DDL_ROLLED_BACK;
  /* Rollback all the uncommitted DDL statements in reverse order */
  for (auto it = m_executed_ddl_stmts.rbegin();
       it != (m_executed_ddl_stmts.rend() - m_latest_committed_stmt); ++it) {
    const Ndb_DDL_stmt &ddl_stmt = *it;
    switch (ddl_stmt.get_ddl_type()) {
      case Ndb_DDL_stmt::CREATE_TABLE:
        result &= rollback_create_table(ddl_stmt);
        break;
      case Ndb_DDL_stmt::RENAME_TABLE:
        result &= rollback_rename_table(ddl_stmt);
        break;
      case Ndb_DDL_stmt::DROP_TABLE:
        /* Nothing to do as the table has not been dropped yet */
        break;
      default:
        result = false;
        assert(false);
        break;
    }
  }
  return result;
}

bool Ndb_DDL_transaction_ctx::run_post_ddl_hooks() {
  DBUG_TRACE;
  if (m_ddl_status == DDL_EMPTY) {
    /* Nothing to run */
    return true;
  }
  assert(m_ddl_status == DDL_COMMITED || m_ddl_status == DDL_ROLLED_BACK);
  bool result = true;
  for (auto it = m_executed_ddl_stmts.begin(); it != m_executed_ddl_stmts.end();
       ++it) {
    const Ndb_DDL_stmt &ddl_stmt = *it;
    switch (ddl_stmt.get_ddl_type()) {
      case Ndb_DDL_stmt::RENAME_TABLE:
        result &= post_ddl_hook_rename_table(ddl_stmt);
        break;
      case Ndb_DDL_stmt::DROP_TABLE:
        result &= post_ddl_hook_drop_temp_table(ddl_stmt);
        break;
      default:
        break;
    }
  }
  return result;
}
