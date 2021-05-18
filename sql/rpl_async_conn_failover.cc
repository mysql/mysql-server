/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysql/components/services/log_builtins.h"

#include "sql/auto_thd.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/rpl_async_conn_failover.h"
#include "sql/rpl_msr.h"  // channel_map
#include "sql/rpl_slave.h"

#include <algorithm>

bool Async_conn_failover_manager::do_auto_conn_failover(Master_info *mi) {
  DBUG_TRACE;

  /* Current position in m_source_conn_detail_list list */
  auto current_pos{m_pos++};
  auto error{false};
  auto ignore_rm_last_source{false};

  /*
    When sender list is exhausted reset position and enable
    ignore_rm_last_source so that all the senders are considered without
    ignoring last failed sender.
  */
  if (current_pos >= m_source_conn_detail_list.size()) {
    current_pos = 0;
    reset_pos();
    ignore_rm_last_source = true;  // for endless loop add all source
  }

  if (current_pos == 0) {
    m_source_conn_detail_list.clear();

    /* Get network configuration details of all source. */
    {
      Rpl_async_conn_failover_table_operations table_op(TL_READ);
      std::tie(error, m_source_conn_detail_list) =
          table_op.read_rows(mi->get_channel());
    }

    if (!error && Master_info::is_configured(mi) && !ignore_rm_last_source) {
      /*
       Remove the connection details of last failed source from the list
       as it was already tried MASTER_RETRY_COUNT times.
      */
      auto it = std::find_if(
          m_source_conn_detail_list.begin(), m_source_conn_detail_list.end(),
          [mi](const SENDER_CONN_TUPLE &e) {
            return (strcmp((std::get<2>(e)).c_str(), mi->host) == 0 &&
                    std::get<3>(e) == mi->port &&
                    strcmp((std::get<4>(e)).c_str(),
                           mi->network_namespace_str()) == 0);
          });
      if (it != m_source_conn_detail_list.end()) {
        m_source_conn_detail_list.erase(it);
      }
    }

    DBUG_EXECUTE_IF("async_conn_failover_wait_new_sender", {
      const char act[] =
          "now SIGNAL wait_for_new_sender_selection "
          "WAIT_FOR continue_connect_new_sender";
      Auto_THD thd;
      DBUG_ASSERT(m_source_conn_detail_list.size() == 3UL);
      DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    /* if there are no source to connect */
    if (m_source_conn_detail_list.size() == 0) {
      LogErr(SYSTEM_LEVEL, ER_RPL_ASYNC_RECONNECT_FAIL_NO_SOURCE,
             mi->get_channel(),
             "no alternative source is"
             " specified",
             "add new source details for the channel");
      return true;
    }
  }

  /*
    reset current network configuration details with new network
    configuration details of choosen source.
  */
  if (!set_channel_conn_details(
          mi, std::get<2>(m_source_conn_detail_list[current_pos]),
          std::get<3>(m_source_conn_detail_list[current_pos]),
          std::get<4>(m_source_conn_detail_list[current_pos]))) {
    return false;
  }

  return true;
}

bool Async_conn_failover_manager::set_channel_conn_details(
    Master_info *mi, const std::string host, const uint port,
    const std::string network_namespace) {
  DBUG_TRACE;

  /* used as a bit mask to indicate running replica threads. */
  int thread_mask{0};
  bool error{false};

  /*
    CHANGE MASTER command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.slave_master_info' replication
    repository tables.
  */
  channel_map.rdlock();
  mi->channel_wrlock();

  /*
    When we change master, we first decide which thread is running and
    which is not. We dont want this assumption to break while we change master.

    Suppose we decide that receiver thread is running and thus it is
    safe to change receive related options in mi. By this time if
    the receive thread is started, we may have a race condition between
    the client thread and receiver thread.
  */
  lock_slave_threads(mi);

  DBUG_ASSERT(!host.empty());
  strmake(mi->host, host.c_str(), sizeof(mi->host) - 1);

  DBUG_ASSERT(port);
  mi->port = port;

  if (!network_namespace.empty())
    strmake(mi->network_namespace, network_namespace.c_str(),
            sizeof(mi->network_namespace) - 1);

  /*
    Sometimes mi->rli->master_log_pos == 0 (it happens when the SQL thread
    is not initialized), so we use a max(). What happens to
    mi->rli->master_log_pos during the initialization stages of replication
    is not 100% clear, so we guard against problems using max().
  */
  mi->set_master_log_pos(std::max<ulonglong>(
      BIN_LOG_HEADER_SIZE, mi->rli->get_group_master_log_pos()));
  mi->set_master_log_name("");

  /*
    Get a bit mask for the replica threads that are running.
    Since the third argument is false, thread_mask after the function
    returns stands for running threads.
  */
  init_thread_mask(&thread_mask, mi, false);

  /* If the receiver is stopped, flush master_info to disk. */
  if ((thread_mask & SLAVE_IO) == 0 && flush_master_info(mi, true)) {
    error = true;
    my_error(ER_RELAY_LOG_INIT, MYF(0), "Failed to flush master info file");
  }

  unlock_slave_threads(mi);
  mi->channel_unlock();
  channel_map.unlock();
  return error;
}
