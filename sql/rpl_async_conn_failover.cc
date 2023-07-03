/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "sql/changestreams/apply/replication_thread_status.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/rpl_async_conn_failover.h"
#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_io_monitor.h"
#include "sql/rpl_msr.h"  // channel_map
#include "sql/rpl_replica.h"

#include <algorithm>

/* replication_asynchronous_connection_failover table column position */
enum class enum_sender_tuple : std::size_t {
  CHANNEL = 0,
  HOST,
  PORT,
  NETNS,
  WEIGHT,
  MANAGED_NAME
};

/* Cast enum_sender_tuple to uint */
constexpr uint enum_convert(enum_sender_tuple eval) {
  return static_cast<uint>(eval);
}

Async_conn_failover_manager::DoAutoConnFailoverError
Async_conn_failover_manager::do_auto_conn_failover(Master_info *mi,
                                                   bool force_highest_weight) {
  DBUG_TRACE;
  channel_map.assert_some_lock();

  /* The list of different source connection details. */
  RPL_FAILOVER_SOURCE_LIST source_conn_detail_list{};

  /*
    On the first connection to a group through a source that is in RECOVERING
    state, the replication_asynchronous_connection_failover table may not be
    yet populated with the group membership. Instead of immediately bailing out
    we do retry read the sources for this channel.
  */
  int retries = 0;
  do {
    if (retries > 0) {
      my_sleep(500000);
    }

    /* Get network configuration details of all sources from this channel. */
    Rpl_async_conn_failover_table_operations table_op(TL_READ);
    auto tmp_details = table_op.read_source_rows_for_channel(mi->get_channel());
    bool table_error = std::get<0>(tmp_details);

    if (!table_error) {
      source_conn_detail_list = std::get<1>(tmp_details);
      std::sort(
          source_conn_detail_list.begin(), source_conn_detail_list.end(),
          [](auto const &t1, auto const &t2) {
            auto tmp_t1 = std::make_tuple(
                std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t1),
                std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t1),
                std::get<enum_convert(enum_sender_tuple::HOST)>(t1),
                std::get<enum_convert(enum_sender_tuple::PORT)>(t1),
                std::get<enum_convert(enum_sender_tuple::NETNS)>(t1));
            auto tmp_t2 = std::make_tuple(
                std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t2),
                std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t2),
                std::get<enum_convert(enum_sender_tuple::HOST)>(t2),
                std::get<enum_convert(enum_sender_tuple::PORT)>(t2),
                std::get<enum_convert(enum_sender_tuple::NETNS)>(t2));
            return (
                (std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t1) >
                 std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t2)) ||
                ((std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t1) >
                  std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t2))) ||
                ((std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::HOST)>(t1) >
                  std::get<enum_convert(enum_sender_tuple::HOST)>(t2))) ||
                ((std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::HOST)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::HOST)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::PORT)>(t1) >
                  std::get<enum_convert(enum_sender_tuple::PORT)>(t2))) ||
                ((std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::HOST)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::HOST)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::PORT)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::PORT)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::NETNS)>(t1) >
                  std::get<enum_convert(enum_sender_tuple::NETNS)>(t2))) ||
                ((std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::WEIGHT)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::CHANNEL)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::HOST)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::HOST)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::PORT)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::PORT)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::NETNS)>(t1) ==
                  std::get<enum_convert(enum_sender_tuple::NETNS)>(t2)) &&
                 (std::get<enum_convert(enum_sender_tuple::MANAGED_NAME)>(t1) >
                  std::get<enum_convert(enum_sender_tuple::MANAGED_NAME)>(
                      t2))));
          });
    }

    retries++;
  } while (source_conn_detail_list.size() == 0 && retries < 10);

  /* if there are no source to connect */
  if (source_conn_detail_list.size() == 0) {
    LogErr(SYSTEM_LEVEL, ER_RPL_ASYNC_RECONNECT_FAIL_NO_SOURCE,
           mi->get_channel(),
           "no alternative source is"
           " specified",
           "add new source details for the channel");
    return DoAutoConnFailoverError::no_sources_error;
  }

  /* When sender list is exhausted reset position. */
  if (force_highest_weight ||
      mi->get_failover_list_position() >= source_conn_detail_list.size()) {
    mi->reset_failover_list_position();
  }

#ifndef NDEBUG
  if (mi->get_failover_list_position() == 0) {
    DBUG_EXECUTE_IF("async_conn_failover_wait_new_sender", {
      const char act[] =
          "now SIGNAL wait_for_new_sender_selection "
          "WAIT_FOR continue_connect_new_sender";
      assert(source_conn_detail_list.size() == 3UL);
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    DBUG_EXECUTE_IF("async_conn_failover_wait_new_4sender", {
      const char act[] =
          "now SIGNAL wait_for_new_4sender_selection "
          "WAIT_FOR continue_connect_new_4sender";
      assert(source_conn_detail_list.size() == 4UL);
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });
  }
#endif

  /*
    reset current network configuration details with new network
    configuration details of chosen source.
  */
  if (!set_channel_conn_details(
          mi,
          std::get<enum_convert(enum_sender_tuple::HOST)>(
              source_conn_detail_list[mi->get_failover_list_position()]),
          std::get<enum_convert(enum_sender_tuple::PORT)>(
              source_conn_detail_list[mi->get_failover_list_position()]),
          std::get<enum_convert(enum_sender_tuple::NETNS)>(
              source_conn_detail_list[mi->get_failover_list_position()]))) {
    /* Increment to next position in source_conn_detail_list list. */
    mi->increment_failover_list_position();
    return DoAutoConnFailoverError::no_error;
  }

  return DoAutoConnFailoverError::retriable_error;
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
  if (mi->channel_trywrlock()) {
    return true;
  }

  /*
    When we change master, we first decide which thread is running and
    which is not. We dont want this assumption to break while we change master.

    Suppose we decide that receiver thread is running and thus it is
    safe to change receive related options in mi. By this time if
    the receive thread is started, we may have a race condition between
    the client thread and receiver thread.
  */
  lock_slave_threads(mi);

  assert(!host.empty());
  strmake(mi->host, host.c_str(), sizeof(mi->host) - 1);

  assert(port);
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
    my_error(ER_RELAY_LOG_INIT, MYF(0),
             "Failed to flush connection metadata repository");
  }

  unlock_slave_threads(mi);
  mi->channel_unlock();
  return error;
}

Async_conn_failover_manager::SourceQuorumStatus
Async_conn_failover_manager::get_source_quorum_status(MYSQL *mysql,
                                                      Master_info *mi) {
  SourceQuorumStatus quorum_status{SourceQuorumStatus::no_error};
  MYSQL_RES *source_res = nullptr;
  MYSQL_ROW source_row = nullptr;
  std::vector<SENDER_CONN_MERGE_TUPLE> source_conn_merged_list{};
  bool error{false}, connected_source_in_sender_list{false};

  mi->reset_network_error();

  /*
    Get stored primary details for channel from
    replication_asynchronous_connection_failover table.
  */
  std::tie(error, source_conn_merged_list) =
      Source_IO_monitor::get_instance()->get_senders_details(mi->get_channel());
  if (error) {
    return SourceQuorumStatus::transient_network_error;
  }

  for (auto source_conn_detail : source_conn_merged_list) {
    std::string host{}, managed_name{};
    uint port{0};

    std::tie(std::ignore, host, port, std::ignore, std::ignore, managed_name,
             std::ignore, std::ignore) = source_conn_detail;
    if (host.compare(mi->host) == 0 && port == mi->port &&
        !managed_name.empty()) {
      connected_source_in_sender_list = true;
      break;
    }
  }

  if (!connected_source_in_sender_list) return SourceQuorumStatus::no_error;

  std::string query = Source_IO_monitor::get_instance()->get_query(
      enum_sql_query_tag::CONFIG_MODE_QUORUM_IO);

  if (!mysql_real_query(mysql, query.c_str(), query.length()) &&
      (source_res = mysql_store_result(mysql)) &&
      (source_row = mysql_fetch_row(source_res))) {
    auto curr_quorum_status{
        static_cast<enum_conf_mode_quorum_status>(std::stoi(source_row[0]))};
    if (curr_quorum_status ==
        enum_conf_mode_quorum_status::MANAGED_GR_HAS_QUORUM) {
      quorum_status = SourceQuorumStatus::no_error;
    } else if (curr_quorum_status ==
               enum_conf_mode_quorum_status::MANAGED_GR_HAS_ERROR) {
      LogErr(ERROR_LEVEL, ER_RPL_ASYNC_CHANNEL_CANT_CONNECT_NO_QUORUM, mi->host,
             mi->port, "", mi->get_channel());
      quorum_status = SourceQuorumStatus::no_quorum_error;
    }
  } else if (mysql_errno(mysql) != ER_UNKNOWN_SYSTEM_VARIABLE) {
    if (is_network_error(mysql_errno(mysql))) {
      mi->set_network_error();
      quorum_status = SourceQuorumStatus::transient_network_error;
    } else {
      LogErr(WARNING_LEVEL, ER_RPL_ASYNC_EXECUTING_QUERY,
             "The IO thread failed to detect if the source belongs to the "
             "group majority",
             mi->host, mi->port, "", mi->get_channel());
      quorum_status = SourceQuorumStatus::fatal_error;
    }
  }

  if (source_res) mysql_free_result(source_res);
  return quorum_status;
}
