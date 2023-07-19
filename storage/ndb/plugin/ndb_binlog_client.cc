/*
   Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

// Implements
#include "storage/ndb/plugin/ndb_binlog_client.h"

#include "sql/rpl_filter.h"  // binlog_filter
#include "sql/sql_class.h"
#include "storage/ndb/plugin/ndb_apply_status_table.h"
#include "storage/ndb/plugin/ndb_conflict.h"
#include "storage/ndb/plugin/ndb_dist_priv_util.h"
#include "storage/ndb/plugin/ndb_event_data.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_ndbapi_util.h"
#include "storage/ndb/plugin/ndb_schema_dist.h"
#include "storage/ndb/plugin/ndb_share.h"

Ndb_binlog_client::Ndb_binlog_client(THD *thd, const char *dbname,
                                     const char *tabname)
    : m_thd(thd), m_dbname(dbname), m_tabname(tabname) {}

Ndb_binlog_client::~Ndb_binlog_client() {}

bool Ndb_binlog_client::table_should_have_event(
    NDB_SHARE *share, const NdbDictionary::Table *ndbtab) const {
  DBUG_TRACE;

  // Never create event(or event operation) for legacy distributed
  // privilege tables, which will be seen only when upgrading from
  // an earlier version.
  if (Ndb_dist_priv_util::is_privilege_table(m_dbname, m_tabname)) {
    DBUG_PRINT("info", ("dist priv table"));
    return false;
  }

  // Never create event(or event operation) for tables which have
  // hidden primary key AND blobs
  if (ndb_table_has_hidden_pk(ndbtab) && ndb_table_has_blobs(ndbtab)) {
    // NOTE! Legacy warning message, could certainly be improved to simply
    // just say:
    // "Binlogging of table with blobs and no primary key is not supported"
    log_warning(ER_ILLEGAL_HA_CREATE_OPTION,
                "Table storage engine 'ndbcluster' does not support the create "
                "option 'Binlog of table with BLOB attribute and no PK'");
    return false;
  }

  // Never create event on exceptions table
  if (is_exceptions_table(m_tabname)) {
    DBUG_PRINT("info", ("exceptions table: %s", share->table_name));
    return false;
  }

  // Turn on usage of event for this table, all tables not passing
  // this point are without event
  share->set_have_event();

  return true;
}

extern bool ndb_binlog_running;

bool Ndb_binlog_client::table_should_have_event_op(
    const NDB_SHARE *share) const {
  DBUG_TRACE;

  if (!share->get_have_event()) {
    // No event -> no event op
    DBUG_PRINT("info", ("table without event"));
    return false;
  }

  // Some tables should always have event operation

  // Check for schema dist table
  if (Ndb_schema_dist_client::is_schema_dist_table(share->db,
                                                   share->table_name)) {
    DBUG_PRINT("exit", ("always need event op for %s", share->table_name));
    return true;
  }

  // Check for schema dist result table
  if (Ndb_schema_dist_client::is_schema_dist_result_table(share->db,
                                                          share->table_name)) {
    DBUG_PRINT("exit", ("always need event op for %s", share->table_name));
    return true;
  }

  // Check for mysql.ndb_apply_status
  if (Ndb_apply_status_table::is_apply_status_table(share->db,
                                                    share->table_name)) {
    DBUG_PRINT("exit", ("always need event op for %s", share->table_name));
    return true;
  }

  if (!ndb_binlog_running) {
    DBUG_PRINT("exit", ("this mysqld is not binlogging"));
    return false;
  }

  // Check if database has been filtered(with --binlog-ignore-db etc.)
  if (!binlog_filter->db_ok(share->db)) {
    DBUG_PRINT("info", ("binlog is filtered for db: %s", share->db));
    return false;
  }

  // Don't create event operation if binlogging for this table
  // has been turned off
  if (share->get_binlog_nologging()) {
    DBUG_PRINT("info", ("binlogging turned off for this table"));
    return false;
  }

  return true;
}

std::string Ndb_binlog_client::event_name_for_table(const char *db,
                                                    const char *table_name,
                                                    bool full) {
  if (Ndb_schema_dist_client::is_schema_dist_table(db, table_name) ||
      Ndb_schema_dist_client::is_schema_dist_result_table(db, table_name)) {
    // Always use REPL$ as prefix for the events on schema dist tables
    full = false;
  }

  std::string name;

  // Set prefix
  if (full)
    name.assign("REPLF$", 6);
  else
    name.assign("REPL$", 5);

  name.append(db).append("/").append(table_name);

  DBUG_PRINT("info", ("event_name_for_table: %s", name.c_str()));

  return name;
}

bool Ndb_binlog_client::event_exists_for_table(Ndb *ndb,
                                               const NDB_SHARE *share) const {
  DBUG_TRACE;

  // Generate event name
  const bool use_full_event =
      share->get_binlog_full() || share->get_subscribe_constrained();
  const std::string event_name =
      event_name_for_table(m_dbname, m_tabname, use_full_event);

  // Get event from NDB
  NdbDictionary::Event_ptr existing_event(
      ndb->getDictionary()->getEvent(event_name.c_str()));
  if (existing_event) {
    // The event exist
    ndb_log_verbose(1, "Event '%s' for table '%s.%s' already exists",
                    event_name.c_str(), m_dbname, m_tabname);
    return true;
  }
  return false;  // Does not exist
}

void Ndb_binlog_client::log_warning(uint code, const char *fmt, ...) const {
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (m_thd->get_command() != COM_DAEMON) {
    // Append the error which caused the error to thd's warning list
    push_warning_printf(m_thd, Sql_condition::SL_WARNING, code, "%s", buf);
  } else {
    // Print the warning to log file
    ndb_log_warning("Binlog: [%s.%s] %d: %s", m_dbname, m_tabname, code, buf);
  }
}

void Ndb_binlog_client::log_ndb_error(const struct NdbError &ndberr) const {
  log_warning(ER_GET_ERRMSG, "Got NDB error: %d - %s", ndberr.code,
              ndberr.message);
}
