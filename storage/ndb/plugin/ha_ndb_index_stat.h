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

#ifndef HA_NDB_INDEX_STAT_H
#define HA_NDB_INDEX_STAT_H

#include <mysql/psi/mysql_thread.h>

#include "storage/ndb/plugin/ndb_component.h"

struct NDB_SHARE;
class Ndb_cluster_connection;
struct SHOW_VAR;
struct SYS_VAR;
struct Ndb_index_stat_proc;

class Ndb_index_stat_thread : public Ndb_component {
  // Someone is waiting for stats
  bool client_waiting;
  mysql_mutex_t LOCK_client_waiting;
  mysql_cond_t COND_client_waiting;

 public:
  Ndb_index_stat_thread();
  ~Ndb_index_stat_thread() override;

  /*
    protect stats entry lists where needed
    protect and signal changes in stats entries
  */
  mysql_mutex_t stat_mutex;
  mysql_cond_t stat_cond;

  // Wake thread up to fetch stats or do other stuff
  void wakeup();

  /* are we setup */
  bool is_setup_complete();

 private:
  int do_init() override;
  void do_run() override;
  int do_deinit() override;
  // Wakeup for stop
  void do_wakeup() override;

  int check_systables(const Ndb_index_stat_proc &pr) const;
  int check_sysevents(const Ndb_index_stat_proc &pr) const;
  void drop_ndb(Ndb_index_stat_proc *const pr) const;
  int start_listener(const Ndb_index_stat_proc &pr) const;
  int create_ndb(Ndb_index_stat_proc *const pr,
                 Ndb_cluster_connection *const connection) const;
  void stop_listener(const Ndb_index_stat_proc &pr) const;
};

/* free entries from share or at end */
void ndb_index_stat_free(NDB_SHARE *, int index_id, int index_version);
void ndb_index_stat_free(NDB_SHARE *);
void ndb_index_stat_end();

/**
  show_ndb_status_index_stat

  Called as part of SHOW STATUS or performance_schema
  queries. Returns info about ndb index stat related status variables.
*/

int show_ndb_status_index_stat(THD *thd, SHOW_VAR *var, char *buff);

// Check and update functions for  --ndb-index-stat-option=
int ndb_index_stat_option_check(THD *, SYS_VAR *, void *save,
                                struct st_mysql_value *value);
void ndb_index_stat_option_update(THD *, SYS_VAR *, void *var_ptr,
                                  const void *save);
// Storage for --ndb-index-stat-option=
extern char ndb_index_stat_option_buf[];

#endif
