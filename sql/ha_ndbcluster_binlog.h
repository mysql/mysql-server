/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <memory>

#include "map_helpers.h"


// Typedefs for long names
typedef NdbDictionary::Object NDBOBJ;
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Index  NDBINDEX;
typedef NdbDictionary::Dictionary  NDBDICT;
typedef NdbDictionary::Event  NDBEVENT;

#define IS_TMP_PREFIX(A) (is_prefix(A, tmp_file_prefix))

#define INJECTOR_EVENT_LEN 200

#define NDB_INVALID_SCHEMA_OBJECT 241

extern Ndb_cluster_connection* g_ndb_cluster_connection;

extern std::unique_ptr<collation_unordered_map<std::string, NDB_SHARE *>>
  ndbcluster_open_tables;

/*
  Initialize the binlog part of the ndb handlerton
*/
void ndbcluster_binlog_init(handlerton* hton);

/*
  Initialize the binlog part of the NDB_SHARE
*/
int ndbcluster_binlog_init_share(THD *thd, NDB_SHARE *share, TABLE *table);
int ndbcluster_create_binlog_setup(THD *thd, Ndb *ndb, const char *key,
                                   const char *db,
                                   const char *table_name,
                                   TABLE * table);
int ndbcluster_create_event(THD *thd, Ndb *ndb, const NDBTAB *table,
                            const char *event_name, NDB_SHARE *share,
                            int push_warning= 0);
int ndbcluster_create_event_ops(THD *thd,
                                NDB_SHARE *share,
                                const NDBTAB *ndbtab,
                                const char *event_name);
int ndbcluster_drop_event(THD *thd, Ndb *ndb, NDB_SHARE *share,
                          const char * dbname, const char * tabname);
int ndbcluster_handle_drop_table(THD *thd, Ndb *ndb, NDB_SHARE *share,
                                 const char *type_str,
                                 const char * db, const char * tabname);
void ndb_rep_event_name(String *event_name,
                        const char *db, const char *tbl,
                        bool full, bool allow_hardcoded_name = true);

int
ndbcluster_get_binlog_replication_info(THD *thd, Ndb *ndb,
                                       const char* db,
                                       const char* table_name,
                                       uint server_id,
                                       Uint32* binlog_flags,
                                       const st_conflict_fn_def** conflict_fn,
                                       st_conflict_fn_arg* args,
                                       Uint32* num_args);
int
ndbcluster_apply_binlog_replication_info(THD *thd,
                                         NDB_SHARE *share,
                                         const NDBTAB* ndbtab,
                                         const st_conflict_fn_def* conflict_fn,
                                         const st_conflict_fn_arg* args,
                                         Uint32 num_args,
                                         Uint32 binlog_flags);
int
ndbcluster_read_binlog_replication(THD *thd, Ndb *ndb,
                                   NDB_SHARE *share,
                                   const NDBTAB *ndbtab,
                                   uint server_id);

int ndbcluster_binlog_start();

int ndbcluster_binlog_end();

/*
  Will return true when the ndb binlog component is properly setup
  and ready to receive events from the cluster. As long as function
  returns false, all tables in this MySQL Server are opened in read only
  mode to avoid writes before the binlog is ready to record them.
 */
bool ndb_binlog_is_read_only(void);

extern NDB_SHARE *ndb_apply_status_share;

extern bool ndb_binlog_running;

/* Prints ndb binlog status string in buf */
size_t ndbcluster_show_status_binlog(char* buf, size_t buf_size);

/*
  Helper functions
*/
bool
ndbcluster_check_if_local_table(const char *dbname, const char *tabname);



