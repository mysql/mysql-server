/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "sql/ndb_binlog_client.h"

#include "sql/ha_ndbcluster_tables.h"
#include "sql/ndb_conflict.h"
#include "sql/ndb_dist_priv_util.h"
#include "sql/ndb_log.h"
#include "sql/ndb_ndbapi_util.h"
#include "sql/ndb_share.h"
#include "sql/rpl_filter.h"  // binlog_filter
#include "sql/sql_class.h"

Ndb_binlog_client::Ndb_binlog_client(THD* thd, const char* dbname,
                                     const char* tabname)
    : m_thd(thd), m_dbname(dbname), m_tabname(tabname) {}

Ndb_binlog_client::~Ndb_binlog_client() {}

bool Ndb_binlog_client::table_should_have_event(
    NDB_SHARE* share, const NdbDictionary::Table* ndbtab) const {
  DBUG_ENTER("table_should_have_event");

  // Never create event(or event operation) for legacy distributed
  // privilege tables, which will be seen only when upgrading from
  // an earlier version.
  if (Ndb_dist_priv_util::is_distributed_priv_table(m_dbname, m_tabname)) {
    DBUG_PRINT("info", ("dist priv table"));
    DBUG_RETURN(false);
  }

  // Never create event(or event operation) for tables which have
  // hidden primary key and blobs
  if (ndb_table_has_hidden_pk(ndbtab) && ndb_table_has_blobs(ndbtab)) {
    // NOTE! Legacy warning message, could certainly be improved to simply
    // just say:
    // "Binlogging of table with blobs and no primary key is not supported"
    log_warning(ER_ILLEGAL_HA_CREATE_OPTION,
                "Table storage engine 'ndbcluster' does not support the create "
                "option 'Binlog of table with BLOB attribute and no PK'");
    DBUG_RETURN(false);
  }

  // Never create event on exceptions table
  if (is_exceptions_table(m_tabname)) {
    DBUG_PRINT("info", ("exceptions table: %s", share->table_name));
    DBUG_RETURN(false);
  }

  // Turn on usage of event for this table, all tables not passing
  // this point are without event
  share->set_have_event();

  DBUG_RETURN(true);
}

extern bool ndb_binlog_running;

bool Ndb_binlog_client::table_should_have_event_op(const NDB_SHARE* share) {
  DBUG_ENTER("table_should_have_event_op");

  if (!share->get_have_event()) {
    // No event -> no event op
    DBUG_PRINT("info", ("table without event"));
    DBUG_RETURN(false);
  }

  // Some tables should always have event operation
  if (strcmp(share->db, NDB_REP_DB) == 0) {
    // The table is in "mysql" database

    // Check for mysql.ndb_schema
    if (strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0) {
      DBUG_PRINT("exit", ("always need event op for " NDB_SCHEMA_TABLE));
      DBUG_RETURN(true);
    }

    // Check for mysql.ndb_apply_status
    if (strcmp(share->table_name, NDB_APPLY_TABLE) == 0) {
      DBUG_PRINT("exit", ("always need event op for " NDB_APPLY_TABLE));
      DBUG_RETURN(true);
    }
  }

  if (!ndb_binlog_running) {
    DBUG_PRINT("exit", ("this mysqld is not binlogging"));
    DBUG_RETURN(false);
  }

  // Check if database has been filtered(with --binlog-ignore-db etc.)
  if (!binlog_filter->db_ok(share->db)) {
    DBUG_PRINT("info", ("binlog is filtered for db: %s", share->db));
    DBUG_RETURN(false);
  }

  // Don't create event operation if binlogging for this table
  // has been turned off
  if (share->get_binlog_nologging()) {
    DBUG_PRINT("info", ("binlogging turned off for this table"));
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}

std::string Ndb_binlog_client::event_name_for_table(const char* db,
                                                    const char* table_name,
                                                    bool full,
                                                    bool allow_hardcoded_name) {
  if (allow_hardcoded_name && strcmp(db, NDB_REP_DB) == 0 &&
      strcmp(table_name, NDB_SCHEMA_TABLE) == 0) {
    // Always use REPL$ as prefix for the event on mysql.ndb_schema
    // (unless when dropping events and allow_hardcoded_name is set to false)
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

bool Ndb_binlog_client::event_exists_for_table(Ndb* ndb,
                                               const NDB_SHARE* share) const {
  DBUG_ENTER("Ndb_binlog_client::event_exists_for_table()");

  // Generate event name
  std::string event_name =
      event_name_for_table(m_dbname, m_tabname, share->get_binlog_full());

  // Get event from NDB
  NdbDictionary::Dictionary* dict = ndb->getDictionary();
  const NdbDictionary::Event* existing_event =
      dict->getEvent(event_name.c_str());
  if (existing_event) {
    // The event exist
    delete existing_event;

    ndb_log_verbose(1, "Event '%s' for table '%s.%s' already exists",
                    event_name.c_str(), m_dbname, m_tabname);

    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);  // Does not exist
}

void Ndb_binlog_client::log_warning(uint code, const char* fmt, ...) const {
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
    ndb_log_warning("NDB Binlog: [%s.%s] %d: %s", m_dbname, m_tabname, code,
                    buf);
  }
}
