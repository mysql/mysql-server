/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_SCHEMA_DIST_H
#define NDB_SCHEMA_DIST_H

#include <string>
#include <vector>

#include "my_inttypes.h"

/**
  The numbers below must not change as they are passed
  between MySQL servers as part of the schema distribution
  protocol. Changes would break compatibility between versions.
  Add new numbers to the end.
*/
enum SCHEMA_OP_TYPE {
  SOT_DROP_TABLE = 0,
  SOT_CREATE_TABLE = 1,
  SOT_RENAME_TABLE_NEW = 2,  // Unused, but still reserved
  SOT_ALTER_TABLE_COMMIT = 3,
  SOT_DROP_DB = 4,
  SOT_CREATE_DB = 5,
  SOT_ALTER_DB = 6,
  SOT_CLEAR_SLOCK = 7,
  SOT_TABLESPACE = 8,     // Never sent since 8.0.14, still reserved
  SOT_LOGFILE_GROUP = 9,  // Never sent since 8.0.14, still reserved
  SOT_RENAME_TABLE = 10,
  SOT_TRUNCATE_TABLE = 11,
  SOT_RENAME_TABLE_PREPARE = 12,
  SOT_ONLINE_ALTER_TABLE_PREPARE = 13,
  SOT_ONLINE_ALTER_TABLE_COMMIT = 14,
  SOT_CREATE_USER = 15,
  SOT_DROP_USER = 16,
  SOT_RENAME_USER = 17,
  SOT_GRANT = 18,
  SOT_REVOKE = 19,
  SOT_CREATE_TABLESPACE = 20,
  SOT_ALTER_TABLESPACE = 21,
  SOT_DROP_TABLESPACE = 22,
  SOT_CREATE_LOGFILE_GROUP = 23,
  SOT_ALTER_LOGFILE_GROUP = 24,
  SOT_DROP_LOGFILE_GROUP = 25,
  SOT_ACL_SNAPSHOT = 26,
  SOT_ACL_STATEMENT = 27,
  SOT_ACL_STATEMENT_REFRESH = 28,
};

namespace Ndb_schema_dist {

// Schema operation result codes
enum Schema_op_result_code {
  NODE_UNSUBSCRIBE = 9001,   // Node unsubscribe during
  NODE_FAILURE = 9002,       // Node failed during
  NODE_TIMEOUT = 9003,       // Node timeout during
  COORD_ABORT = 9004,        // Coordinator aborted
  CLIENT_ABORT = 9005,       // Client aborted
  CLIENT_TIMEOUT = 9006,     // Client timeout
  CLIENT_KILLED = 9007,      // Client killed
  SCHEMA_OP_FAILURE = 9008,  // Failure not related to protocol but the actual
                             // schema operation to be distributed
  NDB_TRANS_FAILURE = 9009   // An NDB read/write transaction failed
};

/**
  Check if schema distribution has been initialized and is
  ready to communicate with the other MySQL Server(s) in the cluster.

  @param requestor Pointer value identifying caller

  @return true schema distribution is ready
*/
bool is_ready(void *requestor);

}  // namespace Ndb_schema_dist

class Ndb;
class NDB_SCHEMA_OBJECT;

/**
  @brief Ndb_schema_dist_client, class represents a Client
  in the schema distribution.

  Contains functionality for distributing a schema operation
  to the other MySQL Server(s) which need to update their
  data structures when a metadata change occurs.

  The Client primarily communicates with the Coordinator(which is
  in the same MySQL Server) while the Coordinator handles communication
  with the Participant nodes(in other MySQL Server). When Coordinator
  have got replies from all Participants, by acknowledging the schema
  operation, the Client will be woken up again.

  Should also have functionality for:
   - checking that "schema dist is ready", i.e checking that
     the mysql.ndb_schema table has been created and schema
     distribution has been initialized properly(by the ndb
     binlog thread)
   - checking that schema distribution of the table and db name
     is supported by the current mysql.ndb_schema, for example
     that length of the table or db name fits in the columns of that
     table
   - checking which functionality the other MySQL Server(s) support,
     for example if they are on an older version they would probably
     still not support longer table names or new schema dist operation types.
*/
class Ndb_schema_dist_client {
  class THD *const m_thd;
  class Thd_ndb *const m_thd_ndb;
  struct NDB_SHARE *m_share{nullptr};
  const std::string m_share_reference;
  class Prepared_keys {
    using Key = std::pair<std::string, std::string>;
    std::vector<Key> m_keys;

   public:
    const std::vector<Key> &keys() { return m_keys; }
    void add_key(const char *db, const char *tabname);
    bool check_key(const char *db, const char *tabname) const;
  } m_prepared_keys;
  bool m_holding_acl_mutex;

  // List of schema operation results, populated when schema operation has
  // completed
  struct Schema_op_result {
    uint32 nodeid;
    uint32 result;
    std::string message;
  };
  std::vector<Schema_op_result> m_schema_op_results;
  // Save results from schema operation for later
  void save_schema_op_results(const NDB_SCHEMA_OBJECT *ndb_schema_object);
  // Push save results as warnings and clear results
  void push_and_clear_schema_op_results();

  static bool m_ddl_blocked;

  bool log_schema_op_impl(Ndb *ndb, const char *query, int query_length,
                          const char *db, const char *table_name,
                          uint32 ndb_table_id, uint32 ndb_table_version,
                          SCHEMA_OP_TYPE type, uint32 anyvalue);

  /**
     @brief Write row to ndb_schema to initiate the schema operation
     @return true on success and false on failure
   */
  bool write_schema_op_to_NDB(Ndb *ndb, const char *query, int query_length,
                              const char *db, const char *name, uint32 id,
                              uint32 version, uint32 nodeid, uint32 type,
                              uint32 schema_op_id, uint32 anyvalue);
  /**
    @brief Distribute the schema operation to the other MySQL Server(s)
    @note For now, just call the old log_schema_op_impl(), over time
          the functionality of that function will gradually be moved over
          to this new Ndb_schema_dist_client class
    @return false if schema distribution fails
   */
  bool log_schema_op(const char *query, size_t query_length, const char *db,
                     const char *table_name, uint32 id, uint32 version,
                     SCHEMA_OP_TYPE type, bool log_query_on_participant = true);

  /**
     @brief Calculate the anyvalue to use for this schema change. The anyvalue
     is used to transport additional settings from client to the participants.

     @param force_nologging Force setting anyvalue to not log schema change on
     participant

     @return The anyvalue to use for schema change
   */
  uint32 calculate_anyvalue(bool force_nologging) const;

  /**
     @brief Acquire the ACL change mutex
   */
  void acquire_acl_lock();

 public:
  Ndb_schema_dist_client() = delete;
  Ndb_schema_dist_client(const Ndb_schema_dist_client &) = delete;
  Ndb_schema_dist_client(class THD *thd);

  ~Ndb_schema_dist_client();

  static void block_ddl(bool ddl_blocked) { m_ddl_blocked = ddl_blocked; }
  static bool is_ddl_blocked() { return m_ddl_blocked; }

  /*
    @brief Generate unique id for distribution of objects which doesn't have
           global id in NDB.
    @return unique id
  */
  uint32 unique_id() const;

  /*
    @brief Generate unique version for distribution of objects which doesn't
           have global id in NDB.
    @return unique version
  */
  uint32 unique_version() const;

  /**
    @brief Prepare client for schema operation, check that
           schema distribution is ready and other conditions are fulfilled.
    @param db database name
    @param tabname table name
    @note Always done early to avoid changing metadata which is
          hard to rollback at a later stage.
    @return true if prepare succeed
  */
  bool prepare(const char *db, const char *tabname);

  /**
    @brief Prepare client for rename schema operation, check that
           schema distribution is ready and other conditions are fulfilled.
           The rename case is different as two different "keys" may be used
           and need to be prepared.
    @param db database name
    @param tabname table name
    @param new_db new database name
    @param new_tabname new table name
    @note Always done early to avoid changing metadata which is
          hard to rollback at a later stage.
    @return true if prepare succeed
  */
  bool prepare_rename(const char *db, const char *tabname, const char *new_db,
                      const char *new_tabname);

  /**
    @brief Prepare client for an ACL change notification
           (e.g. CREATE USER, GRANT, REVOKE, etc.).
    @param node_id Unique number identifying this mysql server
    @return true if prepare succeed
  */
  bool prepare_acl_change(uint node_id);

  /**
    @brief Check that the prepared identifiers is supported by the schema
           distribution. For example long identifiers can't be communicated
           between the MySQL Servers unless the table used for communication
           have large enough columns.
    @note This is done separately from @prepare since different error
          code(or none at all) should be returned for this error.
    @note Always done early to avoid changing metadata which is
          hard to rollback at a later stage.
    @param invalid_identifier The name of the identifier that failed the check
    @return true if check succeed
  */
  bool check_identifier_limits(std::string &invalid_identifier);

  /**
   * @brief Check if given name is the schema distribution table, special
            handling for that table is required in a few places.
     @param db database name
     @param table_name table name
     @return true if table is the schema distribution table
   */
  static bool is_schema_dist_table(const char *db, const char *table_name);

  /**
   * @brief Check if given name is the schema distribution result table, special
            handling for that table is required in a few places.
     @param db database name
     @param table_name table name
     @return true if table is the schema distribution result table
   */
  static bool is_schema_dist_result_table(const char *db,
                                          const char *table_name);

  /**
   * @brief Convert SCHEMA_OP_TYPE to string
   * @return string describing the type
   */
  static const char *type_name(SCHEMA_OP_TYPE type);

  bool create_table(const char *db, const char *table_name, int id,
                    int version);
  bool truncate_table(const char *db, const char *table_name, int id,
                      int version);
  bool alter_table(const char *db, const char *table_name, int id, int version,
                   bool log_on_participant = true);
  bool alter_table_inplace_prepare(const char *db, const char *table_name,
                                   int id, int version);
  bool alter_table_inplace_commit(const char *db, const char *table_name,
                                  int id, int version);
  bool rename_table_prepare(const char *db, const char *table_name, int id,
                            int version, const char *new_key_for_table);
  bool rename_table(const char *db, const char *table_name, int id, int version,
                    const char *new_dbname, const char *new_tabname,
                    bool log_on_participant);
  bool drop_table(const char *db, const char *table_name, int id, int version,
                  bool log_on_participant = true);

  bool create_db(const char *query, uint query_length, const char *db,
                 unsigned int id, unsigned int version);
  bool alter_db(const char *query, uint query_length, const char *db,
                unsigned int id, unsigned int version);
  bool drop_db(const char *db);

  bool acl_notify(const char *db, const char *query, uint query_length,
                  bool participants_must_refresh);
  bool acl_notify(std::string user_list);

  bool create_tablespace(const char *tablespace_name, int id, int version);
  bool alter_tablespace(const char *tablespace_name, int id, int version);
  bool drop_tablespace(const char *tablespace_name, int id, int version);

  bool create_logfile_group(const char *logfile_group_name, int id,
                            int version);
  bool alter_logfile_group(const char *logfile_group_name, int id, int version);
  bool drop_logfile_group(const char *logfile_group_name, int id, int version);
};

#endif
