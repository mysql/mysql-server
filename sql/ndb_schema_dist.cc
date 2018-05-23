/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

// Implements the functions declared in ndb_schema_dist.h
#include "sql/ndb_schema_dist.h"

#include <atomic>

#include "my_dbug.h"
#include "ndbapi/ndb_cluster_connection.hpp"
#include "sql/ha_ndbcluster_tables.h"
#include "sql/ndb_name_util.h"
#include "sql/ndb_share.h"
#include "sql/ndb_thd.h"
#include "sql/ndb_thd_ndb.h"
#include "sql/ndb_schema_dist_table.h"

static const std::string NDB_SCHEMA_TABLE_DB = NDB_REP_DB;
static const std::string NDB_SCHEMA_TABLE_NAME = NDB_SCHEMA_TABLE;

#ifdef _WIN32
#define DIR_SEP "\\"
#else
#define DIR_SEP "/"
#endif

// Temporarily use a fixed string on the form "./mysql/ndb_schema" as key
// for retrieving the NDB_SHARE for mysql.ndb_schema. This will subsequently
// be removed when a NDB_SHARE can be acquired using db+table_name and the
// key is formatted behind the curtains in NDB_SHARE
static const char* NDB_SCHEMA_TABLE_KEY =
    "." DIR_SEP NDB_REP_DB DIR_SEP NDB_SCHEMA_TABLE;

#undef DIR_SEP

bool Ndb_schema_dist_client::is_schema_dist_table(const char* db,
                                                  const char* table_name)
{
  if (db == NDB_SCHEMA_TABLE_DB &&
      table_name == NDB_SCHEMA_TABLE_NAME)
  {
    // This is the NDB table used for schema distribution
    return true;
  }
  return false;
}

Ndb_schema_dist_client::Ndb_schema_dist_client(THD* thd)
    : m_thd(thd), m_thd_ndb(get_thd_ndb(thd)) {}

bool Ndb_schema_dist_client::prepare(const char* db, const char* tabname)
{
  DBUG_ENTER("Ndb_schema_dist_client::prepare");

  // Acquire reference on mysql.ndb_schema
  // NOTE! Using fixed "reference", assuming only one Ndb_schema_dist_client
  // is started at a time since it requires GSL. This may have to be revisited
  m_share =
      NDB_SHARE::acquire_reference_by_key(NDB_SCHEMA_TABLE_KEY,
                                          "ndb_schema_dist_client");
  if (!m_share)
  {
    // Failed to acquire reference to mysql.ndb_schema-> schema distribution
    // is not ready
    m_thd_ndb->push_warning("Schema distribution is not ready");
    DBUG_RETURN(false);
  }

  // Save the prepared "keys"(which are used when communicating with
  // the other MySQL Servers), they should match the keys used in later calls.
  m_prepared_keys.add_key(db, tabname);

  Ndb_schema_dist_table schema_dist_table(m_thd_ndb);
  if (!schema_dist_table.open()) {
    DBUG_RETURN(false);
  }

  if (!schema_dist_table.check_schema()) {
    DBUG_RETURN(false);
  }

  // Schema distribution is ready
  DBUG_RETURN(true);
}

bool Ndb_schema_dist_client::prepare_rename(const char* db, const char* tabname,
                                            const char* new_db,
                                            const char* new_tabname) {
  DBUG_ENTER("Ndb_schema_dist_client::prepare_rename");

  // Normal prepare first
  if (!prepare(db, tabname))
  {
    DBUG_RETURN(false);
  }

  // Allow additional keys for rename which will use the "old" name
  // when communicating with participants until the rename is done.
  // After rename has occured, the new name will be used
  m_prepared_keys.add_key(new_db, new_tabname);

  // Schema distribution is ready
  DBUG_RETURN(true);
}

void Ndb_schema_dist_client::Prepared_keys::add_key(const char* db,
                                                    const char* tabname) {
  m_keys.emplace_back(db, tabname);
}

bool Ndb_schema_dist_client::Prepared_keys::check_key(
    const char* db, const char* tabname) const {
  for (auto key : m_keys) {
    if (key.first == db && key.second == tabname) {
      return true;  // OK, key has been prepared
    }
  }
  return false;
}

extern void update_slave_api_stats(const Ndb*);

Ndb_schema_dist_client::~Ndb_schema_dist_client()
{
  if (m_share)
  {
    // Release the reference to mysql.ndb_schema table
    NDB_SHARE::release_reference(m_share, "ndb_schema_dist_client");
  }

  if (m_thd_ndb->is_slave_thread())
  {
    // Copy-out slave thread statistics
    // NOTE! This is just a "convenient place" to call this
    // function, it could be moved to "end of statement"(if there
    // was such a place..).
    update_slave_api_stats(m_thd_ndb->ndb);
  }
}

/*
  Produce unique identifier for distributing objects that
  does not have any global id from NDB. Use a sequence counter
  which is unique in this node.
*/
static std::atomic<uint32> schema_dist_id_sequence{0};
int Ndb_schema_dist_client::unique_id() {
  int id = ++schema_dist_id_sequence;
  // Handle wraparound
  if (id == 0) {
    id = ++schema_dist_id_sequence;
  }
  DBUG_ASSERT(id != 0);
  return id;
}

/*
  Produce unique identifier for distributing objects that
  does not have any global version from NDB. Use own nodeid
  which is unique in NDB.
*/
int Ndb_schema_dist_client::unique_version() const {
  Thd_ndb* thd_ndb = get_thd_ndb(m_thd);
  const int ver = thd_ndb->connection->node_id();
  DBUG_ASSERT(ver != 0);
  return ver;
}

bool Ndb_schema_dist_client::log_schema_op(const char* query,
                                           size_t query_length, const char* db,
                                           const char* table_name, int id,
                                           int version, SCHEMA_OP_TYPE type,
                                           bool log_query_on_participant) {
  DBUG_ENTER("Ndb_schema_dist_client::log_schema_op");
  DBUG_ASSERT(db && table_name);
  DBUG_ASSERT(id != 0 && version != 0);
  DBUG_ASSERT(m_thd_ndb);

  // Never allow temporary names when communicating with participant
  if (ndb_name_is_temp(db) || ndb_name_is_temp(table_name))
  {
    DBUG_ASSERT(false);
    DBUG_RETURN(false);
  }

  // Check that m_share has been initialized to reference the
  // schema distribution table
  if (m_share == nullptr)
  {
    DBUG_ASSERT(m_share);
    DBUG_RETURN(false);
  }

  // Check that prepared keys match
  if (!m_prepared_keys.check_key(db, table_name))
  {
    m_thd_ndb->push_warning("INTERNAL ERROR: prepared keys didn't match");
    DBUG_ASSERT(false);  // Catch in debug
    DBUG_RETURN(false);
  }

  // Don't distribute if thread has turned off schema distribution
  if (m_thd_ndb->check_option(Thd_ndb::NO_LOG_SCHEMA_OP)) {
    DBUG_PRINT("info", ("NO_LOG_SCHEMA_OP set - > skip schema distribution"));
    DBUG_RETURN(true); // Ok, skipped
  }

  const int result = log_schema_op_impl(
      m_thd_ndb->ndb, query, static_cast<int>(query_length), db, table_name,
      static_cast<uint32>(id), static_cast<uint32>(version), type,
      log_query_on_participant);
  if (result != 0) {
    // Schema distribution failed
    m_thd_ndb->push_warning("Schema distribution failed!");
    DBUG_RETURN(false);
  }
  DBUG_RETURN(true);
}

bool Ndb_schema_dist_client::create_table(const char* db,
                                          const char* table_name, int id,
                                          int version) {
  DBUG_ENTER("Ndb_schema_dist_client::create_table");

  if (is_schema_dist_table(db, table_name))
  {
    // Create of the schema distribution table is not distributed. Instead,
    // every MySQL Server have special handling to create it if not
    // exists and then open it as first step of connecting to the cluster
    DBUG_RETURN(true);
  }

  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version, SOT_CREATE_TABLE));
}

bool Ndb_schema_dist_client::truncate_table(const char* db,
                                            const char* table_name, int id,
                                            int version) {
  DBUG_ENTER("Ndb_schema_dist_client::truncate_table");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version, SOT_TRUNCATE_TABLE));
}



bool Ndb_schema_dist_client::alter_table(const char* db, const char* table_name,
                                         int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::alter_table");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version,
                            SOT_ALTER_TABLE_COMMIT));
}

bool Ndb_schema_dist_client::alter_table_inplace_prepare(const char* db,
                                                         const char* table_name,
                                                         int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::alter_table_inplace_prepare");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version,
                            SOT_ONLINE_ALTER_TABLE_PREPARE));
}

bool Ndb_schema_dist_client::alter_table_inplace_commit(const char* db,
                                                        const char* table_name,
                                                        int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::alter_table_inplace_commit");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version,
                            SOT_ONLINE_ALTER_TABLE_COMMIT));
}

bool Ndb_schema_dist_client::rename_table_prepare(
    const char* db, const char* table_name, int id, int version,
    const char* new_key_for_table) {
  DBUG_ENTER("Ndb_schema_dist_client::rename_table_prepare");
  // NOTE! The rename table prepare phase is primarily done in order to
  // pass the "new key"(i.e db/table_name) for the table to be renamed,
  // that's since there isn't enough placeholders in the subsequent rename
  // table phase.
  DBUG_RETURN(log_schema_op(new_key_for_table, strlen(new_key_for_table), db,
                            table_name, id, version, SOT_RENAME_TABLE_PREPARE));
}

bool Ndb_schema_dist_client::rename_table(const char* db,
                                          const char* table_name, int id,
                                          int version, const char* new_dbname,
                                          const char* new_tabname,
                                          bool log_on_participant) {
  DBUG_ENTER("Ndb_schema_dist_client::rename_table");

  /*
    Rewrite the query, the original query may contain several tables but
    rename_table() is called once for each table in the query.
      ie. RENAME TABLE t1 to tx, t2 to ty;
          -> RENAME TABLE t1 to tx + RENAME TABLE t2 to ty
  */
  std::string rewritten_query;
  rewritten_query.append("rename table `")
      .append(db)
      .append("`.`")
      .append(table_name)
      .append("` to `")
      .append(new_dbname)
      .append("`.`")
      .append(new_tabname)
      .append("`");
  DBUG_PRINT("info", ("rewritten query: '%s'", rewritten_query.c_str()));

  DBUG_RETURN(log_schema_op(rewritten_query.c_str(), rewritten_query.length(),
                            db, table_name, id, version, SOT_RENAME_TABLE,
                            log_on_participant));
}

bool Ndb_schema_dist_client::drop_table(const char* db, const char* table_name,
                                        int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::drop_table");

  /*
    Never distribute each dropped table as part of DROP DATABASE:
    1) as only the DROP DATABASE command should go into binlog
    2) as this MySQL Server is dropping the tables from NDB, when
       the participants get the DROP DATABASE it will remove
       any tables from the DD and then remove the database.
  */
  DBUG_ASSERT(thd_sql_command(m_thd) != SQLCOM_DROP_DB);

  /*
    Rewrite the query, the original query may contain several tables but
    drop_table() is called once for each table in the query.
    ie. DROP TABLE t1, t2;
      -> DROP TABLE t1 + DROP TABLE t2
  */
  std::string rewritten_query;
  rewritten_query.append("drop table `")
      .append(db)
      .append("`.`")
      .append(table_name)
      .append("`");
  DBUG_PRINT("info", ("rewritten query: '%s'", rewritten_query.c_str()));

  DBUG_RETURN(log_schema_op(rewritten_query.c_str(), rewritten_query.length(),
                            db, table_name, id, version, SOT_DROP_TABLE));
}

bool Ndb_schema_dist_client::create_db(const char* query, uint query_length,
                                       const char* db) {
  DBUG_ENTER("Ndb_schema_dist_client::create_db");
  DBUG_RETURN(log_schema_op(query, query_length, db, "", unique_id(),
                            unique_version(), SOT_CREATE_DB));
}

bool Ndb_schema_dist_client::alter_db(const char* query, uint query_length,
                                      const char* db) {
  DBUG_ENTER("Ndb_schema_dist_client::alter_db");
  DBUG_RETURN(log_schema_op(query, query_length, db, "", unique_id(),
                            unique_version(), SOT_ALTER_DB));
}

bool Ndb_schema_dist_client::drop_db(const char* db) {
  DBUG_ENTER("Ndb_schema_dist_client::drop_db");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, "", unique_id(), unique_version(),
                            SOT_DROP_DB));
}

bool Ndb_schema_dist_client::acl_notify(const char* query, uint query_length,
                                        const char* db) {
  DBUG_ENTER("Ndb_schema_dist_client::acl_notify");
  DBUG_RETURN(log_schema_op(query, query_length, db, "", unique_id(),
                            unique_version(), SOT_GRANT));
}

bool Ndb_schema_dist_client::tablespace_changed(const char* tablespace_name,
                                                int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::tablespace_changed");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            "", tablespace_name, id, version, SOT_TABLESPACE));
}

bool Ndb_schema_dist_client::logfilegroup_changed(const char* logfilegroup_name,
                                                  int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::logfilegroup_changed");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            "", logfilegroup_name, id, version,
                            SOT_LOGFILE_GROUP));
}

const char* Ndb_schema_dist_client::type_str(SCHEMA_OP_TYPE type) const {
  switch (type) {
    case SOT_DROP_TABLE:
      return "drop table";
    case SOT_RENAME_TABLE_PREPARE:
      return "rename table prepare";
    case SOT_RENAME_TABLE:
      return "rename table";
    case SOT_CREATE_TABLE:
      return "create table";
    case SOT_ALTER_TABLE_COMMIT:
      return "alter table";
    case SOT_ONLINE_ALTER_TABLE_PREPARE:
      return "online alter table prepare";
    case SOT_ONLINE_ALTER_TABLE_COMMIT:
      return "online alter table commit";
    case SOT_DROP_DB:
      return "drop db";
    case SOT_CREATE_DB:
      return "create db";
    case SOT_ALTER_DB:
      return "alter db";
    case SOT_TABLESPACE:
      return "tablespace";
    case SOT_LOGFILE_GROUP:
      return "logfile group";
    case SOT_TRUNCATE_TABLE:
      return "truncate table";
    case SOT_CREATE_USER:
      return "create user";
    case SOT_DROP_USER:
      return "drop user";
    case SOT_RENAME_USER:
      return "rename user";
    case SOT_GRANT:
      return "grant/revoke";
    case SOT_REVOKE:
      return "revoke all";
    default:
      break;
  }
  // String representation for SCHEMA_OP_TYPE missing
  DBUG_ASSERT(false);  // Catch in debug
  return "<unknown>";
}

const char*
Ndb_schema_dist_client::type_name(SCHEMA_OP_TYPE type)
{
  switch(type){
  case SOT_DROP_TABLE:
    return "DROP_TABLE";
  case SOT_CREATE_TABLE:
    return "CREATE_TABLE";
  case SOT_ALTER_TABLE_COMMIT:
    return "ALTER_TABLE_COMMIT";
  case SOT_DROP_DB:
    return "DROP_DB";
  case SOT_CREATE_DB:
    return "CREATE_DB";
  case SOT_ALTER_DB:
    return "ALTER_DB";
  case SOT_CLEAR_SLOCK:
    return "CLEAR_SLOCK";
  case SOT_TABLESPACE:
    return "TABLESPACE";
  case SOT_LOGFILE_GROUP:
    return "LOGFILE_GROUP";
  case SOT_RENAME_TABLE:
    return "RENAME_TABLE";
  case SOT_TRUNCATE_TABLE:
    return "TRUNCATE_TABLE";
  case SOT_RENAME_TABLE_PREPARE:
    return "RENAME_TABLE_PREPARE";
  case SOT_ONLINE_ALTER_TABLE_PREPARE:
    return "ONLINE_ALTER_TABLE_PREPARE";
  case SOT_ONLINE_ALTER_TABLE_COMMIT:
    return "ONLINE_ALTER_TABLE_COMMIT";
  case SOT_CREATE_USER:
    return "CREATE_USER";
  case SOT_DROP_USER:
    return "DROP_USER";
  case SOT_RENAME_USER:
    return "RENAME_USER";
  case SOT_GRANT:
    return "GRANT";
  case SOT_REVOKE:
    return "REVOKE";
  default:
    break;
  }
  DBUG_ASSERT(false);
  return "<unknown>";
}
