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

#ifndef NDB_SCHEMA_DIST_H
#define NDB_SCHEMA_DIST_H

#include <string>
#include <vector>

#include "my_inttypes.h"

/**
  Check if schema distribution has been initialized and is
  ready. Will return true when the component is properly setup
  to receive schema operation events from the cluster.
*/
bool ndb_schema_dist_is_ready(void);


/**
  The numbers below must not change as they are passed
  between MySQL servers as part of the schema distribution
  protocol. Changes would break compatibility between versions.
  Add new numbers to the end.
*/
enum SCHEMA_OP_TYPE
{
  SOT_DROP_TABLE= 0,
  SOT_CREATE_TABLE= 1,
  SOT_RENAME_TABLE_NEW= 2, // Unused, but still reserved
  SOT_ALTER_TABLE_COMMIT= 3,
  SOT_DROP_DB= 4,
  SOT_CREATE_DB= 5,
  SOT_ALTER_DB= 6,
  SOT_CLEAR_SLOCK= 7,
  SOT_TABLESPACE= 8,
  SOT_LOGFILE_GROUP= 9,
  SOT_RENAME_TABLE= 10,
  SOT_TRUNCATE_TABLE= 11,
  SOT_RENAME_TABLE_PREPARE= 12,
  SOT_ONLINE_ALTER_TABLE_PREPARE= 13,
  SOT_ONLINE_ALTER_TABLE_COMMIT= 14,
  SOT_CREATE_USER= 15,
  SOT_DROP_USER= 16,
  SOT_RENAME_USER= 17,
  SOT_GRANT= 18,
  SOT_REVOKE= 19
};

const char* get_schema_type_name(uint type);

/**
  @brief Ndb_schema_dist_client, class represents a Client
  in the schema distribution.

  Contains functionality for distributing a schema operation
  to the other MySQL Server(s) which need to update their
  data structures when a metadata change occurs.

  The Client primarily communicates with the Coordinator(which is
  in the same MySQL Server) while the Coordinator handles communication
  with the Participant nodes(in other MySQL Serve)r. When Coordinator
  have got replies from all Participants, by acknowledging the schema
  operation, the Client will be woken up again.

  Should also have functionality for:
   - checking that "schema dist is ready", i.e checking that
     the mysql.ndb_schema table has been created and schema
     distribution has been initialized properly(by the ndb
     binlog thread)
   - checking that schema distribution of the table and db name
     is suported by the current mysql.ndb_schema, for example
     that length of the table or db name fits in the columns of that
     table
   - checking which functionality the other MySQL Server(s) support,
     for example if they are on an older version they would probably
     still not support longer table names or new schema dist operation types.
*/
class Ndb_schema_dist_client {
  class THD *const m_thd;
  class Thd_ndb* const m_thd_ndb;
  struct NDB_SHARE *m_share{nullptr};
  class Prepared_keys {
    std::vector<std::pair<std::string, std::string>> m_keys;
  public:
    void add_key(const char* db, const char* tabname);
    bool check_key(const char* db, const char* tabname) const;
  } m_prepared_keys;

  /*
    @brief Generate unique id for distribution of objects which doesn't have
           global id in NDB.
    @return unique id
  */
  int unique_id();

  /*
    @brief Generate unique version for distribution of objects which doesn't
           have global id in NDB.
    @return unique version
  */
  int unique_version() const;

  int log_schema_op_impl(Thd_ndb *thd_ndb, const char *query, int query_length,
                         const char *db, const char *table_name,
                         uint32 ndb_table_id, uint32 ndb_table_version,
                         enum SCHEMA_OP_TYPE type, const char *new_db,
                         const char *new_table_name,
                         bool log_query_on_participant);

  /**
    @brief Distribute the schema operation to the other MySQL Server(s)
    @note For now, just call the old log_schema_op_impl(), over time
          the functionality of that function will gradually be moved over
          to this new Ndb_schema_dist_client class
    @return false if schema distribution fails
   */
  bool log_schema_op(const char *query, size_t query_length, const char *db,
                     const char *table_name, int id, int version,
                     SCHEMA_OP_TYPE type, const char *new_db,
                     const char *new_table_name,
                     bool log_query_on_participant = true);

 public:
  Ndb_schema_dist_client() = delete;
  Ndb_schema_dist_client(const Ndb_schema_dist_client &) = delete;
  Ndb_schema_dist_client(class THD *thd);

  ~Ndb_schema_dist_client();

  /**
    @brief Prepare client for schema operation, check that
           schema distribution is ready and other conditions are fulfilled.
    @param db database name
    @param tabname table name
    @note Always done early to avoid changing metadata which is
          hard to rollback at a later stage.
    @return true if prepare succeed
  */
  bool prepare(const char* db, const char* tabname);

  /**
    @brief Prepare client for rename schema operation, check that
           schema distribution is ready and other conditions are fulfilled.
           The rename case is different as two different "keys" may be used
           and need to be prepared.
    @param db database name
    @param table_name table name
    @param new_db new database name
    @param new_tabname new table name
    @note Always done early to avoid changing metadata which is
          hard to rollback at a later stage.
    @return true if prepare succeed
  */
  bool prepare_rename(const char *db, const char *tabname, const char *new_db,
                      const char *new_tabname);

  /**
   * @brief Check if given name is the schema distribtution table, special
            handling for that table is required in a few places.
     @param db database name
     @param table_name table name
     @return true if table is the schema distribution table
   */
  static bool is_schema_dist_table(const char* db, const char* table_name);

  bool create_table(const char *db, const char *table_name, int id,
                    int version);
  bool truncate_table(const char *db, const char *table_name, int id,
                      int version);
  bool alter_table(const char *db, const char *table_name, int id, int version);
  bool alter_table_inplace_prepare(const char *db, const char *table_name,
                                   int id, int version);
  bool alter_table_inplace_commit(const char *db, const char *table_name,
                                  int id, int version);
  bool rename_table_prepare(const char *db, const char *table_name, int id,
                            int version, const char *new_key_for_table);
  bool rename_table(const char *db, const char *table_name, int id, int version,
                    const char *new_dbname, const char *new_tabname,
                    bool log_on_participant);
  bool drop_table(const char *db, const char *table_name, int id, int version);

  bool create_db(const char *query, uint query_length, const char *db);
  bool alter_db(const char *query, uint query_length, const char *db);
  bool drop_db(const char *db);

  bool acl_notify(const char *query, uint query_length, const char *db);
  bool tablespace_changed(const char *tablespace_name, int id, int version);
  bool logfilegroup_changed(const char *logfilegroup_name, int id, int version);
};

#endif
