/*
   Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/changestreams/apply/replication_thread_status.h"
#include "sql/changestreams/apply/constants.h"
#include "sql/rpl_io_monitor.h"

void lock_slave_threads(Master_info *mi) {
  DBUG_TRACE;

  // protection against mixed locking order (see rpl_slave.cc header)
  mi->channel_assert_some_wrlock();

  // TODO: see if we can do this without dual mutex
  mysql_mutex_lock(&mi->run_lock);
  mysql_mutex_lock(&mi->rli->run_lock);
}

void unlock_slave_threads(Master_info *mi) {
  DBUG_TRACE;

  // TODO: see if we can do this without dual mutex
  mysql_mutex_unlock(&mi->rli->run_lock);
  mysql_mutex_unlock(&mi->run_lock);
}

void init_thread_mask(int *mask, Master_info *mi, bool inverse,
                      bool ignore_monitor_thread) {
  bool set_io = mi->slave_running, set_sql = mi->rli->slave_running;
  bool set_monitor{
      Source_IO_monitor::get_instance()->is_monitoring_process_running()};
  int tmp_mask{0};
  DBUG_TRACE;

  if (set_io) tmp_mask |= REPLICA_IO;
  if (set_sql) tmp_mask |= REPLICA_SQL;
  if (!ignore_monitor_thread && set_monitor &&
      mi->is_source_connection_auto_failover()) {
    tmp_mask |= SLAVE_MONITOR;
  }

  if (inverse) {
    tmp_mask ^= (REPLICA_IO | REPLICA_SQL);
    if (!ignore_monitor_thread && mi->is_source_connection_auto_failover()) {
      tmp_mask ^= SLAVE_MONITOR;
    }
  }

  *mask = tmp_mask;
}
