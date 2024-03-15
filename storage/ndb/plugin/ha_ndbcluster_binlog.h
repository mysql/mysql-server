/*
   Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#include <stddef.h>

#include <string>

constexpr int DEFAULT_ZSTD_COMPRESSION_LEVEL = 3;

class THD;
struct SHOW_VAR;
namespace dd {
class Table;
}
class Ndb_sync_pending_objects_table;
class Ndb_sync_excluded_objects_table;
struct NDB_SHARE;

/*
  Initialize the binlog part of the ndbcluster plugin
*/
bool ndbcluster_binlog_init(struct handlerton *hton);

int ndbcluster_binlog_setup_table(THD *thd, class Ndb *ndb, const char *db,
                                  const char *table_name,
                                  const dd::Table *table_def,
                                  const bool skip_error_handling = false);

int ndbcluster_binlog_wait_synch_drop_table(THD *thd, const NDB_SHARE *share);

int ndbcluster_binlog_start();

void ndbcluster_binlog_set_server_started();

void ndbcluster_binlog_pre_dd_shutdown();

void ndbcluster_binlog_end();

/*
  Will return true when the ndb binlog component is properly setup
  and ready to receive events from the cluster. As long as function
  returns false, all tables in this MySQL Server are opened in read only
  mode to avoid writes before the binlog is ready to record them.
 */
bool ndb_binlog_is_read_only(void);

bool ndb_binlog_is_initialized(void);

/* Prints ndb binlog status string in buf */
size_t ndbcluster_show_status_binlog(char *buf, size_t buf_size);

/*
  Called as part of SHOW STATUS or performance_schema
  queries. Returns injector related status variables.
*/
int show_ndb_status_injector(THD *, SHOW_VAR *var, char *);

/**
 @brief Validate excluded objects
 @param thd  Thread handle
*/
void ndbcluster_binlog_validate_sync_excluded_objects(THD *thd);

/**
 @brief Clear the list of objects excluded from sync
*/
void ndbcluster_binlog_clear_sync_excluded_objects();

/**
 @brief Clear the list of objects whose synchronization have been retried
*/
void ndbcluster_binlog_clear_sync_retry_objects();

/**
 @brief Queue up tables which the ndb binlog thread needs to check for changes
 @param db_name     The name of database the table belongs to
 @param table_name  The name of table to check
 @return true if the workitem was accepted, false if not
*/
bool ndbcluster_binlog_check_table_async(const std::string &db_name,
                                         const std::string &table_name);

/**
 @brief Queue up logfile group items which the ndb binlog thread needs to check
        for changes
 @param lfg_name  The name of logfile group to check. This cannot be empty
 @return true if the workitem was accepted, false if not
*/
bool ndbcluster_binlog_check_logfile_group_async(const std::string &lfg_name);

/**
 @brief Queue up tablespace items which the ndb binlog thread needs to check for
        changes
 @param tablespace_name  The name of tablespace to check. This cannot be empty
 @return true if the workitem was accepted, false if not
*/
bool ndbcluster_binlog_check_tablespace_async(
    const std::string &tablespace_name);

/**
 @brief Queue up schema items which the ndb binlog thread needs to check for
        changes
 @param schema_name  The name of the schema to check. This cannot be empty
 @return true if the workitem was accepted, false if not
*/
bool ndbcluster_binlog_check_schema_async(const std::string &schema_name);

/**
 @brief Retrieve information about objects currently excluded from sync
 @param excluded_table  Pointer to excluded objects table object
*/
void ndbcluster_binlog_retrieve_sync_excluded_objects(
    Ndb_sync_excluded_objects_table *excluded_table);

/**
 @brief Get the number of objects currently excluded from sync
 @return excluded objects count
*/
unsigned int ndbcluster_binlog_get_sync_excluded_objects_count();

/**
 @brief Retrieve information about objects currently pending sync
 @param pending_table  Pointer to pending objects table object
*/
void ndbcluster_binlog_retrieve_sync_pending_objects(
    Ndb_sync_pending_objects_table *pending_table);

/**
 @brief Get the number of objects currently awaiting sync
 @return pending objects count
*/
unsigned int ndbcluster_binlog_get_sync_pending_objects_count();
