/*
<<<<<<< HEAD
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
=======
   Copyright (c) 2000, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6

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

#include <stddef.h>

namespace dd {
class Table;
}

/*
  Initialize the binlog part of the ndb handlerton
*/
void ndbcluster_binlog_init(struct handlerton* hton);

int ndbcluster_binlog_setup_table(class THD* thd, class Ndb* ndb,
                                  const char* db, const char* table_name,
                                  const dd::Table* table_def);

int ndbcluster_binlog_wait_synch_drop_table(class THD* thd,
                                            struct NDB_SHARE* share);

<<<<<<< HEAD
=======
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
void ndbcluster_handle_incomplete_binlog_setup();
void ndb_rep_event_name(String *event_name,
                        const char *db, const char *tbl,
                        bool full, bool allow_hardcoded_name = true);
#ifdef HAVE_NDB_BINLOG
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
#endif
int ndb_create_table_from_engine(THD *thd, const char *db,
                                 const char *table_name);
>>>>>>> upstream/cluster-7.6
int ndbcluster_binlog_start();

void ndbcluster_binlog_set_server_started();

int ndbcluster_binlog_end();

/*
  Will return true when the ndb binlog component is properly setup
  and ready to receive events from the cluster. As long as function
  returns false, all tables in this MySQL Server are opened in read only
  mode to avoid writes before the binlog is ready to record them.
 */
bool ndb_binlog_is_read_only(void);

/* Prints ndb binlog status string in buf */
size_t ndbcluster_show_status_binlog(char* buf, size_t buf_size);
