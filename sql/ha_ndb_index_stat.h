/*
   Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef HA_NDB_INDEX_STAT_H
#define HA_NDB_INDEX_STAT_H

#include "ndb_component.h"
#include <mysql/psi/mysql_thread.h>

struct NDB_SHARE;
class Ndb_cluster_connection;

class Ndb_index_stat_thread : public Ndb_component
{
  // Someone is waiting for stats
  bool client_waiting;
  mysql_mutex_t LOCK_client_waiting;
  mysql_cond_t COND_client_waiting;
public:
  Ndb_index_stat_thread();
  virtual ~Ndb_index_stat_thread();

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
  virtual int do_init();
  virtual void do_run();
  virtual int do_deinit();
  // Wakeup for stop
  virtual void do_wakeup();

  int check_or_create_systables(struct Ndb_index_stat_proc& pr);
  int check_or_create_sysevents(struct Ndb_index_stat_proc& pr);
  void drop_ndb(struct Ndb_index_stat_proc& pr);
  int start_listener(struct Ndb_index_stat_proc& pr);
  int create_ndb(struct Ndb_index_stat_proc& pr,
                 Ndb_cluster_connection* connection);
  void stop_listener(struct Ndb_index_stat_proc& pr);
};

/* free entries from share or at end */
void ndb_index_stat_free(NDB_SHARE*, int iudex_id, int index_version);
void ndb_index_stat_free(NDB_SHARE*);
void ndb_index_stat_end();

#endif
