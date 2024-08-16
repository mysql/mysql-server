/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

// Implements the functions declared in ndb_schema_dist.h
#include "storage/ndb/plugin/ndb_schema_dist.h"

#include <atomic>
#include <mutex>

#include "my_dbug.h"
#include "mysqld_error.h"
#include "ndbapi/ndb_cluster_connection.hpp"
#include "sql/query_options.h"  // OPTION_BIN_LOG
#include "sql/sql_error.h"
#include "sql/sql_thd_internal_api.h"
#include "storage/ndb/plugin/ndb_anyvalue.h"
#include "storage/ndb/plugin/ndb_applier.h"
#include "storage/ndb/plugin/ndb_dist_priv_util.h"
#include "storage/ndb/plugin/ndb_name_util.h"
#include "storage/ndb/plugin/ndb_require.h"
#include "storage/ndb/plugin/ndb_schema_dist_table.h"
#include "storage/ndb/plugin/ndb_schema_object.h"
#include "storage/ndb/plugin/ndb_schema_result_table.h"
#include "storage/ndb/plugin/ndb_share.h"
#include "storage/ndb/plugin/ndb_thd.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"
#include "storage/ndb/plugin/ndb_upgrade_util.h"

bool Ndb_schema_dist::is_ready(void *requestor) {
  DBUG_TRACE;

  std::stringstream ss;
  ss << "is_ready_" << std::hex << requestor;
  const std::string reference = ss.str();

  // Acquire reference on mysql.ndb_schema
  NDB_SHARE *schema_share = NDB_SHARE::acquire_reference(
      Ndb_schema_dist_table::DB_NAME.c_str(),
      Ndb_schema_dist_table::TABLE_NAME.c_str(), reference.c_str());
  if (schema_share == nullptr) return false;  // Not ready

  if (!schema_share->have_event_operation()) {
    NDB_SHARE::release_reference(schema_share, reference.c_str());
    return false;  // Not ready
  }

  NDB_SHARE::release_reference(schema_share, reference.c_str());
  return true;
}

bool Ndb_schema_dist_client::m_ddl_blocked = true;

bool Ndb_schema_dist_client::is_schema_dist_table(const char *db,
                                                  const char *table_name) {
  if (db == Ndb_schema_dist_table::DB_NAME &&
      table_name == Ndb_schema_dist_table::TABLE_NAME) {
    // This is the NDB table used for schema distribution
    return true;
  }
  return false;
}

bool Ndb_schema_dist_client::is_schema_dist_result_table(
    const char *db, const char *table_name) {
  if (db == Ndb_schema_result_table::DB_NAME &&
      table_name == Ndb_schema_result_table::TABLE_NAME) {
    // This is the NDB table used for schema distribution results
    return true;
  }
  return false;
}

/*
  Actual schema change operations that effect the local Data Dictionary are
  performed with the Global Schema Lock held, but ACL operations are not.
  Use acl_change_mutex to serialize all ACL changes on this server.
*/
static std::mutex acl_change_mutex;

void Ndb_schema_dist_client::acquire_acl_lock() {
  acl_change_mutex.lock();
  m_holding_acl_mutex = true;
}

static std::string unique_reference(void *owner) {
  std::stringstream ss;
  ss << "ndb_schema_dist_client" << std::hex << owner;
  return ss.str();
}

Ndb_schema_dist_client::Ndb_schema_dist_client(THD *thd)
    : m_thd(thd),
      m_thd_ndb(get_thd_ndb(thd)),
      m_share_reference(unique_reference(this)),
      m_holding_acl_mutex(false) {}

bool Ndb_schema_dist_client::prepare(const char *db, const char *tabname) {
  DBUG_TRACE;

  // Check local schema distribution state
  if (!check_local_schema_dist_available()) {
    push_warning(m_thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                 "Schema distribution is not ready");
    return false;
  }

  // Acquire reference on mysql.ndb_schema
  m_share = NDB_SHARE::acquire_reference(
      Ndb_schema_dist_table::DB_NAME.c_str(),
      Ndb_schema_dist_table::TABLE_NAME.c_str(), m_share_reference.c_str());

  if (m_share == nullptr || m_share->have_event_operation() == false ||
      DBUG_EVALUATE_IF("ndb_schema_dist_not_ready_early", true, false)) {
    // The NDB_SHARE for mysql.ndb_schema hasn't been created or not setup
    // yet -> schema distribution is not ready
    push_warning(m_thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                 "Schema distribution is not ready");
    return false;
  }

  // Acquire reference also on mysql.ndb_schema_result
  m_result_share = NDB_SHARE::acquire_reference(
      Ndb_schema_result_table::DB_NAME.c_str(),
      Ndb_schema_result_table::TABLE_NAME.c_str(), m_share_reference.c_str());
  if (m_result_share == nullptr ||
      m_result_share->have_event_operation() == false) {
    // The mysql.ndb_schema_result hasn't been created or not setup yet ->
    // schema distribution is not ready
    push_warning(m_thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                 "Schema distribution is not ready (ndb_schema_result)");
    return false;
  }

  if (unlikely(m_ddl_blocked)) {
    // If a data node gets upgraded after this MySQL Server is upgraded, this
    // MySQL Server will not be aware of the upgrade due to Bug#30930132.
    // So as a workaround, re-evaluate again if the DDL needs to be blocked
    if (ndb_all_nodes_support_mysql_dd()) {
      // All nodes connected to cluster support MySQL DD.
      // No need to continue blocking the DDL.
      m_ddl_blocked = false;
    } else {
      // Non database DDLs are blocked in plugin due to an ongoing upgrade.
      // Database DDLs are allowed as they are actually executed in the Server
      // layer and ndbcluster is only responsible for distributing the change to
      // other MySQL Servers.
      if (strlen(tabname) != 0) {
        // If the tablename is not empty, it is a non database DDL. Block it.
        my_printf_error(
            ER_DISALLOWED_OPERATION,
            "DDLs are disallowed on NDB SE as there is at least one node "
            "without MySQL DD support connected to the cluster.",
            MYF(0));
        return false;
      }
    }
  }

  // Save the prepared "keys"(which are used when communicating with
  // the other MySQL Servers), they should match the keys used in later calls.
  m_prepared_keys.add_key(db, tabname);

  Ndb_schema_dist_table schema_dist_table(m_thd_ndb);
  if (!schema_dist_table.open()) {
    return false;
  }

  if (!schema_dist_table.check_schema()) {
    return false;
  }

  // Open the ndb_schema_result table, the table is created by ndbcluster
  // when connecting to NDB and thus it shall exist at this time.
  Ndb_schema_result_table schema_result_table(m_thd_ndb);
  if (!schema_result_table.open()) {
    return false;
  }

  if (!schema_result_table.check_schema()) {
    return false;
  }

  // Schema distribution is ready
  return true;
}

bool Ndb_schema_dist_client::prepare_rename(const char *db, const char *tabname,
                                            const char *new_db,
                                            const char *new_tabname) {
  DBUG_TRACE;

  // Normal prepare first
  if (!prepare(db, tabname)) {
    /* During upgrade to 8.0, distributed privilege tables must get renamed
       as part of a statement "ALTER TABLE ... ENGINE=innodb" before schema
       distribution has started running.
    */
    if (Ndb_dist_priv_util::is_privilege_table(db, tabname)) return true;

    return false;
  }

  // Allow additional keys for rename which will use the "old" name
  // when communicating with participants until the rename is done.
  // After rename has occurred, the new name will be used
  m_prepared_keys.add_key(new_db, new_tabname);

  // Schema distribution is ready
  return true;
}

bool Ndb_schema_dist_client::prepare_acl_change(uint node_id) {
  /* Acquire the ACL change mutex. It will be released by the destructor.
   */
  acquire_acl_lock();

  /*
    There is no table name required to log an ACL operation, but the table
    name is a part of the primary key in ndb_schema. Fabricate a name
    that is unique to this MySQL server, so that ACL changes originating
    from different servers use different rows in ndb_schema.
  */
  std::string server_key = "acl_dist_from_" + std::to_string(node_id);

  /*
    Always use "mysql" as the db part of the primary key.
    If the current database is set to something other than "mysql", the
    database will be transmitted as part of GRANT and REVOKE statements.
  */
  return prepare("mysql", server_key.c_str());
}

bool Ndb_schema_dist_client::check_identifier_limits(
    std::string &invalid_identifier) {
  DBUG_TRACE;

  Ndb_schema_dist_table schema_dist_table(m_thd_ndb);
  if (!schema_dist_table.open()) {
    invalid_identifier = "<open failed>";
    return false;
  }

  // Check that identifiers does not exceed the limits imposed
  // by the ndb_schema table layout
  for (const auto &key : m_prepared_keys.keys()) {
    // db
    if (!schema_dist_table.check_column_identifier_limit(
            Ndb_schema_dist_table::COL_DB, key.first)) {
      invalid_identifier = key.first;
      return false;
    }
    // name
    if (!schema_dist_table.check_column_identifier_limit(
            Ndb_schema_dist_table::COL_NAME, key.second)) {
      invalid_identifier = key.second;
      return false;
    }
  }
  return true;
}

void Ndb_schema_dist_client::Prepared_keys::add_key(const char *db,
                                                    const char *tabname) {
  m_keys.emplace_back(db, tabname);
}

bool Ndb_schema_dist_client::Prepared_keys::check_key(
    const char *db, const char *tabname) const {
  for (const auto &key : m_keys) {
    if (key.first == db && key.second == tabname) {
      return true;  // OK, key has been prepared
    }
  }
  return false;
}

Ndb_schema_dist_client::~Ndb_schema_dist_client() {
  if (m_share) {
    // Release the reference to mysql.ndb_schema table
    NDB_SHARE::release_reference(m_share, m_share_reference.c_str());
  }
  if (m_result_share) {
    // Release the reference to mysql.ndb_schema_result table
    NDB_SHARE::release_reference(m_result_share, m_share_reference.c_str());
  }

  if (m_thd_ndb) {
    // Inform Applier that one schema distribution has completed
    Ndb_applier *const applier = m_thd_ndb->get_applier();
    if (applier) {
      applier->atSchemaDistCompleted();
    }
  }

  if (m_holding_acl_mutex) {
    acl_change_mutex.unlock();
  }
}

/*
  Produce unique identifier for distributing objects that
  does not have any global id from NDB. Use a sequence counter
  which is unique in this node.
*/
static std::atomic<uint32> schema_dist_id_sequence{0};
uint32 Ndb_schema_dist_client::unique_id() const {
  uint32 id = ++schema_dist_id_sequence;
  // Handle wraparound
  if (id == 0) {
    id = ++schema_dist_id_sequence;
  }
  assert(id != 0);
  return id;
}

/*
  Produce unique identifier for distributing objects that
  does not have any global version from NDB. Use own nodeid
  which is unique in NDB.
*/
uint32 Ndb_schema_dist_client::unique_version() const {
  const uint32 ver = m_thd_ndb->connection->node_id();
  assert(ver != 0);
  return ver;
}

void Ndb_schema_dist_client::save_schema_op_results(
    const NDB_SCHEMA_OBJECT *ndb_schema_object) {
  std::vector<NDB_SCHEMA_OBJECT::Result> participant_results;
  ndb_schema_object->client_get_schema_op_results(participant_results);
  for (const auto &it : participant_results) {
    m_schema_op_results.push_back({it.nodeid, it.result, it.message});
  }
}

void Ndb_schema_dist_client::push_and_clear_schema_op_results() {
  if (m_schema_op_results.empty()) {
    return;
  }

  // Push results received from participant(s) as warnings. These are meant to
  // indicate that schema distribution has failed on one of the nodes. For more
  // information on how and why the failure occurred, the relevant error log
  // remains the place to look
  for (const Schema_op_result &op_result : m_schema_op_results) {
    // Warning consists of the node id and message but not result code since
    // that's an internal detail
    m_thd_ndb->push_warning("Node %d: '%s'", op_result.nodeid,
                            op_result.message.c_str());
  }
  // Clear the results. This is needed when the Ndb_schema_dist_client object
  // is reused as is the case during an inplace alter where the same object is
  // used during both prepare and commit
  m_schema_op_results.clear();
}

bool Ndb_schema_dist_client::log_schema_op(const char *query,
                                           size_t query_length, const char *db,
                                           const char *table_name, uint32 id,
                                           uint32 version, SCHEMA_OP_TYPE type,
                                           bool log_query_on_participant) {
  DBUG_TRACE;
  assert(db && table_name);
  assert(id != 0 && version != 0);
  assert(m_thd_ndb);

  // Never allow temporary names when communicating with participant
  if (ndb_name_is_temp(db) || ndb_name_is_temp(table_name)) {
    assert(false);
    return false;
  }

  // Require that references to schema distribution tables has been initialized
  ndbcluster::ndbrequire(m_share);
  ndbcluster::ndbrequire(m_result_share);

  // Check that prepared keys match
  if (!m_prepared_keys.check_key(db, table_name)) {
    m_thd_ndb->push_warning("INTERNAL ERROR: prepared keys didn't match");
    assert(false);  // Catch in debug
    return false;
  }

  // Don't distribute if thread has turned off schema distribution
  if (m_thd_ndb->check_option(Thd_ndb::NO_LOG_SCHEMA_OP)) {
    DBUG_PRINT("info", ("NO_LOG_SCHEMA_OP set - > skip schema distribution"));
    return true;  // Ok, skipped
  }

  // Verify identifier limits, this should already have been caught earlier
  {
    std::string invalid_identifier;
    if (!check_identifier_limits(invalid_identifier)) {
      m_thd_ndb->push_warning("INTERNAL ERROR: identifier limits exceeded");
      // Catch in debug, but allow failure from opening ndb_schema table
      assert(invalid_identifier == "<open failed>");
      return false;
    }
  }

  // Calculate anyvalue
  const Uint32 anyvalue = calculate_anyvalue(log_query_on_participant);

  const bool result =
      log_schema_op_impl(m_thd_ndb->ndb, query, static_cast<int>(query_length),
                         db, table_name, id, version, type, anyvalue);
  if (!result) {
    // Schema distribution failed
    push_and_clear_schema_op_results();
    m_thd_ndb->push_warning("Schema distribution failed");
    return false;
  }

  // Schema distribution passed but the schema op may have failed on
  // participants. Push and clear results (if any)
  push_and_clear_schema_op_results();
  return true;
}

bool Ndb_schema_dist_client::create_table(const char *db,
                                          const char *table_name, int id,
                                          int version) {
  DBUG_TRACE;

  if (is_schema_dist_table(db, table_name)) {
    // Create of the schema distribution table is not distributed. Instead,
    // every MySQL Server have special handling to create it if not
    // exists and then open it as first step of connecting to the cluster
    return true;
  }

  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), db,
                       table_name, id, version, SOT_CREATE_TABLE);
}

bool Ndb_schema_dist_client::truncate_table(const char *db,
                                            const char *table_name, int id,
                                            int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), db,
                       table_name, id, version, SOT_TRUNCATE_TABLE);
}

bool Ndb_schema_dist_client::alter_table(const char *db, const char *table_name,
                                         int id, int version,
                                         bool log_on_participant) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), db,
                       table_name, id, version, SOT_ALTER_TABLE_COMMIT,
                       log_on_participant);
}

bool Ndb_schema_dist_client::alter_table_inplace_prepare(const char *db,
                                                         const char *table_name,
                                                         int id, int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), db,
                       table_name, id, version, SOT_ONLINE_ALTER_TABLE_PREPARE);
}

bool Ndb_schema_dist_client::alter_table_inplace_commit(const char *db,
                                                        const char *table_name,
                                                        int id, int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), db,
                       table_name, id, version, SOT_ONLINE_ALTER_TABLE_COMMIT);
}

bool Ndb_schema_dist_client::rename_table_prepare(
    const char *db, const char *table_name, int id, int version,
    const char *new_key_for_table) {
  DBUG_TRACE;
  // NOTE! The rename table prepare phase is primarily done in order to
  // pass the "new key"(i.e db/table_name) for the table to be renamed,
  // that's since there isn't enough placeholders in the subsequent rename
  // table phase.
  // NOTE2! The "new key" is sent in filesystem format where multibyte or
  // characters who are deemed not suitable as filenames have been encoded. This
  // differs from the db and tablename parameters in the schema dist
  // protocol who are just passed as they are.
  return log_schema_op(new_key_for_table, strlen(new_key_for_table), db,
                       table_name, id, version, SOT_RENAME_TABLE_PREPARE);
}

bool Ndb_schema_dist_client::rename_table(const char *db,
                                          const char *table_name, int id,
                                          int version, const char *new_dbname,
                                          const char *new_tabname,
                                          bool log_on_participant) {
  DBUG_TRACE;

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

  return log_schema_op(rewritten_query.c_str(), rewritten_query.length(), db,
                       table_name, id, version, SOT_RENAME_TABLE,
                       log_on_participant);
}

bool Ndb_schema_dist_client::drop_table(const char *db, const char *table_name,
                                        int id, int version,
                                        bool log_on_participant) {
  DBUG_TRACE;

  /*
    Never distribute each dropped table as part of DROP DATABASE:
    1) as only the DROP DATABASE command should go into binlog
    2) as this MySQL Server is dropping the tables from NDB, when
       the participants get the DROP DATABASE it will remove
       any tables from the DD and then remove the database.
  */
  assert(thd_sql_command(m_thd) != SQLCOM_DROP_DB);

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

  // Special case where the table to be dropped was already dropped in the
  // client. This is considered acceptable behavior and the query is distributed
  // to ensure that the table is dropped in the pariticipants. Assign values to
  // id and version to workaround the assumption that they will always be != 0
  if (id == 0 && version == 0) {
    id = unique_id();
    version = unique_version();
  }

  return log_schema_op(rewritten_query.c_str(), rewritten_query.length(), db,
                       table_name, id, version, SOT_DROP_TABLE,
                       log_on_participant);
}

bool Ndb_schema_dist_client::create_db(const char *query, uint query_length,
                                       const char *db, unsigned int id,
                                       unsigned int version) {
  DBUG_TRACE;

  // Checking identifier limits "late", there is no way to return
  // an error to fail the CREATE DATABASE command
  std::string invalid_identifier;
  if (!check_identifier_limits(invalid_identifier)) {
    // Check of db name limit failed
    m_thd_ndb->push_warning("Identifier name '%-.100s' is too long",
                            invalid_identifier.c_str());
    return false;
  }

  return log_schema_op(query, query_length, db, "", id, version, SOT_CREATE_DB);
}

bool Ndb_schema_dist_client::alter_db(const char *query, uint query_length,
                                      const char *db, unsigned int id,
                                      unsigned int version) {
  DBUG_TRACE;

  // Checking identifier limits "late", there is no way to return
  // an error to fail the ALTER DATABASE command
  std::string invalid_identifier;
  if (!check_identifier_limits(invalid_identifier)) {
    // Check of db name limit failed
    m_thd_ndb->push_warning("Identifier name '%-.100s' is too long",
                            invalid_identifier.c_str());
    return false;
  }

  return log_schema_op(query, query_length, db, "", id, version, SOT_ALTER_DB);
}

bool Ndb_schema_dist_client::drop_db(const char *db) {
  DBUG_TRACE;

  // Checking identifier limits "late", there is no way to return
  // an error to fail the DROP DATABASE command
  std::string invalid_identifier;
  if (!check_identifier_limits(invalid_identifier)) {
    // Check of db name limit failed
    m_thd_ndb->push_warning("Identifier name '%-.100s' is too long",
                            invalid_identifier.c_str());
    return false;
  }

  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), db,
                       "", unique_id(), unique_version(), SOT_DROP_DB);
}

/* STATEMENT-style ACL change distribution */
bool Ndb_schema_dist_client::acl_notify(const char *database, const char *query,
                                        uint query_length,
                                        bool participant_refresh) {
  DBUG_TRACE;
  assert(m_holding_acl_mutex);
  auto key = m_prepared_keys.keys()[0];
  std::string new_query("use ");
  if (database != nullptr && strcmp(database, "mysql")) {
    new_query.append(database).append(";").append(query, query_length);
    query = new_query.c_str();
    query_length = new_query.size();
  }
  SCHEMA_OP_TYPE type =
      participant_refresh ? SOT_ACL_STATEMENT : SOT_ACL_STATEMENT_REFRESH;
  return log_schema_op(query, query_length, key.first.c_str(),
                       key.second.c_str(), unique_id(), unique_version(), type);
}

/* SNAPSHOT-style ACL change distribution */
bool Ndb_schema_dist_client::acl_notify(std::string user_list) {
  DBUG_TRACE;
  assert(m_holding_acl_mutex);
  auto key = m_prepared_keys.keys()[0];

  return log_schema_op(user_list.c_str(), user_list.length(), key.first.c_str(),
                       key.second.c_str(), unique_id(), unique_version(),
                       SOT_ACL_SNAPSHOT);
}

bool Ndb_schema_dist_client::create_tablespace(const char *tablespace_name,
                                               int id, int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), "",
                       tablespace_name, id, version, SOT_CREATE_TABLESPACE);
}

bool Ndb_schema_dist_client::alter_tablespace(const char *tablespace_name,
                                              int id, int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), "",
                       tablespace_name, id, version, SOT_ALTER_TABLESPACE);
}

bool Ndb_schema_dist_client::drop_tablespace(const char *tablespace_name,
                                             int id, int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), "",
                       tablespace_name, id, version, SOT_DROP_TABLESPACE);
}

bool Ndb_schema_dist_client::create_logfile_group(
    const char *logfile_group_name, int id, int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), "",
                       logfile_group_name, id, version,
                       SOT_CREATE_LOGFILE_GROUP);
}

bool Ndb_schema_dist_client::alter_logfile_group(const char *logfile_group_name,
                                                 int id, int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), "",
                       logfile_group_name, id, version,
                       SOT_ALTER_LOGFILE_GROUP);
}

bool Ndb_schema_dist_client::drop_logfile_group(const char *logfile_group_name,
                                                int id, int version) {
  DBUG_TRACE;
  return log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd), "",
                       logfile_group_name, id, version, SOT_DROP_LOGFILE_GROUP);
}

const char *Ndb_schema_dist_client::type_name(SCHEMA_OP_TYPE type) {
  switch (type) {
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
    case SOT_CREATE_TABLESPACE:
      return "CREATE_TABLESPACE";
    case SOT_ALTER_TABLESPACE:
      return "ALTER_TABLESPACE";
    case SOT_DROP_TABLESPACE:
      return "DROP_TABLESPACE";
    case SOT_CREATE_LOGFILE_GROUP:
      return "CREATE_LOGFILE_GROUP";
    case SOT_ALTER_LOGFILE_GROUP:
      return "ALTER_LOGFILE_GROUP";
    case SOT_DROP_LOGFILE_GROUP:
      return "DROP_LOGFILE_GROUP";
    case SOT_ACL_SNAPSHOT:
      return "ACL_SNAPSHOT";
    case SOT_ACL_STATEMENT:
      return "ACL_STATEMENT";
    case SOT_ACL_STATEMENT_REFRESH:
      return "ACL_STATEMENT_REFRESH";
    default:
      break;
  }
  assert(false);
  return "<unknown>";
}

uint32 Ndb_schema_dist_client::calculate_anyvalue(bool force_nologging) const {
  Uint32 anyValue = 0;
  if (!m_thd_ndb->get_applier()) {
    /* Schema change originating from this MySQLD, check SQL_LOG_BIN
     * variable and pass 'setting' to all logging MySQLDs via AnyValue
     */
    if (thd_test_options(m_thd, OPTION_BIN_LOG)) /* e.g. SQL_LOG_BIN == on */
    {
      DBUG_PRINT("info", ("Schema event for binlogging"));
      ndbcluster_anyvalue_set_normal(anyValue);
    } else {
      DBUG_PRINT("info", ("Schema event not for binlogging"));
      ndbcluster_anyvalue_set_nologging(anyValue);
    }

    if (!force_nologging) {
      DBUG_PRINT("info", ("Forcing query not to be binlogged on participant"));
      ndbcluster_anyvalue_set_nologging(anyValue);
    }
  } else {
    /*
       Slave propagating replicated schema event in ndb_schema
       In case replicated serverId is composite
       (server-id-bits < 31) we copy it into the
       AnyValue as-is
       This is for 'future', as currently Schema operations
       do not have composite AnyValues.
       In future it may be useful to support *not* mapping composite
       AnyValues to/from Binlogged server-ids.
    */
    DBUG_PRINT("info", ("Replicated schema event with original server id"));
    anyValue = thd_unmasked_server_id(m_thd);
  }

#ifndef NDEBUG
  /*
    MySQLD will set the user-portion of AnyValue (if any) to all 1s
    This tests code filtering ServerIds on the value of server-id-bits.
  */
  const char *p = getenv("NDB_TEST_ANYVALUE_USERDATA");
  if (p != nullptr && *p != 0 && *p != '0' && *p != 'n' && *p != 'N') {
    dbug_ndbcluster_anyvalue_set_userbits(anyValue);
  }
#endif
  DBUG_PRINT("info", ("anyvalue: %u", anyValue));
  return anyValue;
}
