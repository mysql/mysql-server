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

// Implements the functions defined in ndb_schema_dist.h
#include "sql/ndb_schema_dist.h"

#include "my_dbug.h"
#include "sql/ndb_thd.h"

const char*
get_schema_type_name(uint type)
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
  }
  return "<unknown>";
}

bool Ndb_schema_dist_client::log_schema_op(const char* query,
                                           size_t query_length, const char* db,
                                           const char* table_name, int id,
                                           int version, SCHEMA_OP_TYPE type,
                                           const char* new_db,
                                           const char* new_table_name,
                                           bool log_query_on_participant) {
  DBUG_ENTER("Ndb_schema_dist_client::log_schema_op");

  const int result = ndbcluster_log_schema_op(
      m_thd, query, static_cast<int>(query_length), db, table_name,
      static_cast<uint32>(id), static_cast<uint32>(version), type, new_db,
      new_table_name, log_query_on_participant);
  if (result != 0) {
    // Schema distribution failed
    DBUG_RETURN(false);
  }
  DBUG_RETURN(true);
}

Ndb_schema_dist_client::Ndb_schema_dist_client(THD* thd)
    : m_thd(thd), m_thd_ndb(get_thd_ndb(thd)) {}

bool Ndb_schema_dist_client::create_table(const char* db,
                                          const char* table_name, int id,
                                          int version) {
  DBUG_ENTER("Ndb_schema_dist_client::create_table");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version, SOT_CREATE_TABLE,
                            nullptr, nullptr));
}

bool Ndb_schema_dist_client::truncate_table(const char* db,
                                            const char* table_name, int id,
                                            int version) {
  DBUG_ENTER("Ndb_schema_dist_client::truncate_table");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version, SOT_TRUNCATE_TABLE,
                            nullptr, nullptr));
}



bool Ndb_schema_dist_client::alter_table(const char* db, const char* table_name,
                                         int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::alter_table");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version, SOT_ALTER_TABLE_COMMIT,
                            nullptr, nullptr));
}

bool Ndb_schema_dist_client::alter_table_inplace_prepare(const char* db,
                                                         const char* table_name,
                                                         int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::alter_table_inplace_prepare");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version,
                            SOT_ONLINE_ALTER_TABLE_PREPARE, nullptr, nullptr));
}

bool Ndb_schema_dist_client::alter_table_inplace_commit(const char* db,
                                                        const char* table_name,
                                                        int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::alter_table_inplace_commit");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version,
                            SOT_ONLINE_ALTER_TABLE_COMMIT, nullptr, nullptr));
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
                            table_name, id, version, SOT_RENAME_TABLE_PREPARE,
                            nullptr, nullptr));
}

bool Ndb_schema_dist_client::rename_table(const char* db,
                                          const char* table_name, int id,
                                          int version, const char* new_dbname,
                                          const char* new_tabname,
                                          bool log_on_participant) {
  DBUG_ENTER("Ndb_schema_dist_client::rename_table");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version, SOT_RENAME_TABLE,
                            new_dbname, new_tabname, log_on_participant));
}

bool Ndb_schema_dist_client::drop_table(const char* db, const char* table_name,
                                        int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::drop_table");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, table_name, id, version, SOT_DROP_TABLE,
                            nullptr, nullptr));
}

bool Ndb_schema_dist_client::create_db(const char* query, uint query_length,
                                       const char* db) {
  DBUG_ENTER("Ndb_schema_dist_client::create_db");
  DBUG_RETURN(log_schema_op(query, query_length, db, "", RANDOM_ID,
                            RANDOM_VERSION, SOT_CREATE_DB, nullptr, nullptr));
}

bool Ndb_schema_dist_client::alter_db(const char* query, uint query_length,
                                      const char* db) {
  DBUG_ENTER("Ndb_schema_dist_client::alter_db");
  DBUG_RETURN(log_schema_op(query, query_length, db, "", RANDOM_ID,
                            RANDOM_VERSION, SOT_ALTER_DB, nullptr, nullptr));
}

bool Ndb_schema_dist_client::drop_db(const char* db) {
  DBUG_ENTER("Ndb_schema_dist_client::drop_db");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            db, "", RANDOM_ID, RANDOM_VERSION, SOT_DROP_DB,
                            nullptr, nullptr));
}

bool Ndb_schema_dist_client::acl_notify(const char* query, uint query_length,
                                        const char* db) {
  DBUG_ENTER("Ndb_schema_dist_client::acl_notify");
  DBUG_RETURN(log_schema_op(query, query_length, db, "", RANDOM_ID,
                            RANDOM_VERSION, SOT_GRANT, nullptr, nullptr));
}

bool Ndb_schema_dist_client::tablespace_changed(const char* tablespace_name,
                                                int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::tablespace_changed");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            "", tablespace_name, id, version, SOT_TABLESPACE,
                            nullptr, nullptr));
}

bool Ndb_schema_dist_client::logfilegroup_changed(const char* logfilegroup_name,
                                                  int id, int version) {
  DBUG_ENTER("Ndb_schema_dist_client::logfilegroup_changed");
  DBUG_RETURN(log_schema_op(ndb_thd_query(m_thd), ndb_thd_query_length(m_thd),
                            "", logfilegroup_name, id, version,
                            SOT_LOGFILE_GROUP, nullptr, nullptr));
}
