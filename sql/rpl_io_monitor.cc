/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "sql/rpl_io_monitor.h"
#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_status_service.h>
#include "mysql/components/services/log_builtins.h"

#include "sql-common/json_dom.h"
#include "sql/changestreams/apply/replication_thread_status.h"
#include "sql/mysqld.h"
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/protocol_classic.h"
#include "sql/rpl_async_conn_failover.h"  // reset_pos
#include "sql/rpl_async_conn_failover_configuration_propagation.h"
#include "sql/rpl_group_replication.h"
#include "sql/rpl_msr.h" /* Multisource replication */
#include "sql/rpl_replica.h"
#include "sql/rpl_sys_key_access.h"
#include "sql/rpl_sys_table_access.h"
#include "sql/sql_class.h"  // THD
#include "sql/udf_service_util.h"

#include "my_dbug.h"
#include "my_systime.h"

#include <string>

/**
  Restart the IO thread of the given channel.

  @param[in] thd           The running thread.
  @param[in] channel_name  the channel IO thread to restart.
  @param[in] force_sender_with_highest_weight  When true, sender with highest
  weight is chosen, otherwise the next sender from the current one is chosen.

  @return true if IO thread was restarted, false otherwise.
*/
static bool restart_io_thread(THD *thd, const std::string &channel_name,
                              bool force_sender_with_highest_weight);

/*
  The SQL_QUERIES array contains three queries. The enum_sql_query_tag index/tag
  is used to get each query. There are for following purpose:

  1. CONFIG_MODE_QUORUM_MONITOR:
    Its used by Monitor IO thread to determine if given source has Group
    Replication enabled and if enabled whether member is in ONLINE or
    RECOVERING state and has QUORUM.

  2. CONFIG_MODE_QUORUM_IO:
    Its used by IO thread to determine if given source has Group Replication
    enabled and if enabled whether member is in ONLINE state and has QUORUM.

  3. GR_MEMBER_ALL_DETAILS:
    Its used by Monitor IO thread to get following member details:
      group_name, host, port, member state and member role.

  4. GR_MEMBER_ALL_DETAILS_FETCH_FOR_57:
    Its used by Monitor IO thread for mysql-5.7 servers to get following
      member details:
      group_name, host, port, member state and member role.
    In mysql-5.7 performance_schema.replication_group_members do not have
    member role column but its fetched from group_replication_primary_member
    status variable, when group is on single-primary mode.

  5. QUERY_SERVER_SELECT_ONE:
    Its used by Monitor IO thread to check single-server is in working state.
    It establishes connection with single server and executes this
    query to confirm that connection to SOURCE is working.
*/
static const char *SQL_QUERIES[] = {
    "SELECT * FROM ( "
    "  SELECT CASE "
    "    WHEN ((SELECT count(*) from information_schema.plugins WHERE "
    "           PLUGIN_NAME LIKE 'group_replication') <> 1) "
    "    THEN (SELECT 2) "
    "    WHEN ((SELECT IF(((MEMBER_STATE='ONLINE') OR "
    "                      (MEMBER_STATE='RECOVERING')) AND "
    "          ((SELECT COUNT(*) FROM "
    "            performance_schema.replication_group_members "
    "            WHERE MEMBER_STATE != 'ONLINE' AND MEMBER_STATE != "
    "'RECOVERING') "
    "            >= ((SELECT COUNT(*) FROM "
    "                 performance_schema.replication_group_members)/2)=0),1,0) "
    "       FROM performance_schema.replication_group_members "
    "       WHERE member_id=@@global.server_uuid) = 1) "
    "    THEN (SELECT 1) "
    "    ELSE (SELECT 2) "
    "  END AS QUORUM "
    ") Q ",
    "SELECT * FROM ( "
    "  SELECT CASE "
    "    WHEN ((SELECT count(*) from information_schema.plugins WHERE "
    "           PLUGIN_NAME LIKE 'group_replication') <> 1) "
    "    THEN (SELECT 2) "
    "    WHEN ((SELECT IF(MEMBER_STATE='ONLINE' AND "
    "          ((SELECT COUNT(*) FROM "
    "            performance_schema.replication_group_members "
    "            WHERE MEMBER_STATE != 'ONLINE' AND MEMBER_STATE != "
    "'RECOVERING') "
    "            >= ((SELECT COUNT(*) FROM "
    "                 performance_schema.replication_group_members)/2)=0),1,0) "
    "       FROM performance_schema.replication_group_members "
    "       WHERE member_id=@@global.server_uuid) = 1) "
    "    THEN (SELECT 1) "
    "    ELSE (SELECT 2) "
    "  END AS QUORUM "
    ") Q ",
    "SELECT @@global.group_replication_group_name, PRGM.MEMBER_HOST, "
    "       PRGM.MEMBER_PORT, PRGM.MEMBER_STATE, PRGM.MEMBER_ROLE "
    "FROM performance_schema.replication_group_members PRGM",
    "SELECT @@global.group_replication_group_name, PRGM.MEMBER_HOST, "
    "       PRGM.MEMBER_PORT, PRGM.MEMBER_STATE, "
    "       (SELECT IF(GR_SINGLE_PRIMARY_MODE.VARIABLE_VALUE = 'OFF', "
    "                  'PRIMARY', "
    "                  IF(PRGM.MEMBER_ID = GR_PRIMARY_MEMBER.VARIABLE_VALUE, "
    "                     'PRIMARY', 'SECONDARY')) "
    "        FROM (SELECT VARIABLE_VALUE FROM performance_schema.global_status "
    "              WHERE VARIABLE_NAME = 'group_replication_primary_member') "
    "              GR_PRIMARY_MEMBER,"
    "             (SELECT VARIABLE_VALUE FROM "
    "                performance_schema.global_variables "
    "              WHERE "
    "                VARIABLE_NAME='group_replication_single_primary_mode') "
    "                GR_SINGLE_PRIMARY_MODE "
    "       ) MEMBER_ROLE "
    "FROM performance_schema.replication_group_members PRGM",
    "SELECT 1"};

MYSQL_RES_TUPLE execute_query(const Mysql_connection *conn,
                              enum_sql_query_tag qtag) {
  int tag = static_cast<int>(qtag);
  std::string query = SQL_QUERIES[tag];
  return conn->execute_query(query);
}

static void *launch_handler_thread(void *arg) {
  Source_IO_monitor *monitor = (Source_IO_monitor *)arg;
  monitor->source_monitor_handler();
  return nullptr;
}

std::string Source_IO_monitor::get_query(enum_sql_query_tag qtag) {
  int tag = static_cast<int>(qtag);
  std::string query = SQL_QUERIES[tag];
  return query;
}

Source_IO_monitor::Source_IO_monitor() {
#ifdef HAVE_PSI_INTERFACE
  mysql_mutex_init(key_monitor_info_run_lock, &m_run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_monitor_info_run_cond, &m_run_cond);
#else
  mysql_mutex_init(nullptr, &m_run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(nullptr, &m_run_cond);
#endif
}

Source_IO_monitor::~Source_IO_monitor() {
  terminate_monitoring_process();
  mysql_mutex_destroy(&m_run_lock);
  mysql_cond_destroy(&m_run_cond);
}

bool Source_IO_monitor::is_monitor_killed(THD *thd, Master_info *) {
  DBUG_TRACE;
  assert(m_monitor_thd == thd);

  return m_abort_monitor || connection_events_loop_aborted() || thd->killed;
}

bool Source_IO_monitor::launch_monitoring_process(PSI_thread_key thread_key) {
  DBUG_TRACE;

  mysql_mutex_lock(&m_run_lock);

  // Callers should ensure the process is terminated
  assert(!m_monitor_thd_state.is_thread_alive());
  if (m_monitor_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&m_run_lock);
    return true;
  }

  if (mysql_thread_create(thread_key, &m_th, &connection_attrib,
                          launch_handler_thread, (void *)this)) {
    my_error(ER_REPLICA_THREAD, MYF(0));
    mysql_mutex_unlock(&m_run_lock);
    return true;
  }

  m_monitor_thd_state.set_created();

  while (m_monitor_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep", ("Waiting for the Monitoring process thread to start"));
    mysql_cond_wait(&m_run_cond, &m_run_lock);
  }
  mysql_mutex_unlock(&m_run_lock);

  return false;
}

void Source_IO_monitor::source_monitor_handler() {
  THD *thd{nullptr};  // needs to be first for thread_stack
  thd = new THD;      // note that constructor of THD uses DBUG_ !
  m_monitor_thd = thd;
  struct timespec waittime;

  DBUG_TRACE;

  THD_CHECK_SENTRY(thd);
  my_thread_init();

#ifdef HAVE_PSI_THREAD_INTERFACE
  // save the instrumentation for IO thread in mi->info_thd
  struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
  thd_set_psi(thd, psi);
#endif
  thd->thread_stack = (char *)&thd;  // remember where our stack is

  if (init_replica_thread(thd, SLAVE_THD_IO)) {
    my_error(ER_REPLICA_FATAL_ERROR, MYF(0),
             "Failed during Replica IO Monitor thread initialization ");
    goto err;
  }

  thd->security_context()->skip_grants();
  Global_THD_manager::get_instance()->add_thd(thd);

  mysql_mutex_lock(&m_run_lock);
  m_monitor_thd_state.set_running();
  m_abort_monitor = false;
  set_timespec(&waittime, m_retry_monitor_wait);

  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  while (!is_monitor_killed(thd, nullptr) &&
         !is_group_replication_member_secondary()) {
    sync_senders_details(thd);

    THD_STAGE_INFO(thd, stage_rpl_failover_wait_before_next_fetch);
    set_timespec(&waittime, m_retry_monitor_wait);
    mysql_mutex_lock(&m_run_lock);
    mysql_cond_timedwait(&m_run_cond, &m_run_lock, &waittime);
    mysql_mutex_unlock(&m_run_lock);
  }

err:
  LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_MONITOR_IO_THREAD_EXITING);

  /* At this point the I/O thread will not try to reconnect anymore. */
  thd->reset_query();
  thd->reset_db(NULL_CSTR);

  // destructor will not free it, because net.vio is 0
  thd->get_protocol_classic()->end_net();
  thd->release_resources();

  THD_CHECK_SENTRY(thd);
  Global_THD_manager::get_instance()->remove_thd(thd);

  delete thd;

  mysql_mutex_lock(&m_run_lock);
  m_monitor_thd_state.set_terminated();
  m_abort_monitor = true;
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  my_thread_end();
  my_thread_exit(nullptr);

  return;
}

std::tuple<bool, std::string> Source_IO_monitor::delete_rows(
    Rpl_sys_table_access &table_op, TABLE *table,
    std::vector<std::string> field_name,
    std::tuple<std::string, std::string, uint> conn_detail) {
  bool err_val{false};
  std::string err_msg{};

  Rpl_sys_table_access::for_each_in_tuple(
      conn_detail, [&](const auto &n, const auto &x) {
        if (table_op.store_field(table->field[n], x)) {
          err_msg.assign(table_op.get_field_error_msg(field_name[n]));
          err_val = true;
        }
      });
  if (err_val) return std::make_tuple(err_val, err_msg);

  Rpl_sys_table_access::handler_delete_row_func(table_op, err_val, err_msg);
  return std::make_tuple(err_val, err_msg);
}

std::tuple<bool, std::string> Source_IO_monitor::write_rows(
    Rpl_sys_table_access &table_op, TABLE *table,
    std::vector<std::string> field_name,
    RPL_FAILOVER_SOURCE_TUPLE conn_detail) {
  bool err_val{false};
  std::string err_msg{};

  Rpl_sys_table_access::for_each_in_tuple(
      conn_detail, [&](const auto &n, const auto &x) {
        if (table_op.store_field(table->field[n], x)) {
          err_msg.assign(table_op.get_field_error_msg(field_name[n]));
          err_val = true;
        }
      });
  if (err_val) return std::make_tuple(err_val, err_msg);

  Rpl_sys_table_access::handler_write_row_func(table_op, err_val, err_msg);
  return std::make_tuple(err_val, err_msg);
}

int Source_IO_monitor::connect_senders(THD *thd,
                                       const std::string &channel_name) {
  std::vector<SENDER_CONN_MERGE_TUPLE> failover_table_detail_list{};
  std::vector<RPL_FAILOVER_SOURCE_TUPLE> source_conn_detail_list{};
  bool error{false};
  /* highest group failover weight for the current channel. */
  uint curr_highest_group_weight{0};
  /* weight of single senders for the current channel. */
  uint curr_highest_weight_single_sender{0};
  /* weight for current connected sender */
  uint curr_conn_weight{0};

  if (is_monitor_killed(thd, nullptr)) return 1;

  /*
    1. Get stored source details for channel from
       replication_asynchronous_connection_failover table.
  */
  std::tie(error, failover_table_detail_list) =
      get_senders_details(channel_name);
  if (error) {
    return 2;
  }

  if (is_monitor_killed(thd, nullptr)) return 1;

  /*
    2. Get weight of current connected sender.
  */
  {
    Rpl_async_conn_failover_table_operations table_op_src(TL_READ);

    std::tie(error, source_conn_detail_list) =
        table_op_src.read_source_rows_for_channel(channel_name);

    std::sort(source_conn_detail_list.begin(), source_conn_detail_list.end(),
              [](const RPL_FAILOVER_SOURCE_TUPLE &element1,
                 const RPL_FAILOVER_SOURCE_TUPLE &element2) -> bool {
                return std::get<4>(element1) > std::get<4>(element2);
              });

    channel_map.rdlock();
    Master_info *mi = channel_map.get_mi(channel_name.c_str());
    if (nullptr == mi) {
      channel_map.unlock();
      return 2;
    }
    const std::string mi_host{mi->host};
    const uint mi_port = mi->port;
    channel_map.unlock();

    for (auto source_conn_detail : source_conn_detail_list) {
      uint port{0}, weight{0};
      std::string host{""};

      std::tie(std::ignore, host, port, std::ignore, weight, std::ignore) =
          source_conn_detail;

      /* save weight for current connected sender */
      if ((host.compare(mi_host) == 0) && (port == mi_port)) {
        curr_conn_weight = weight;
        break;
      }
    }
  }

  /*
    3. Connect to one of the source from group gathered in step 1, store their
       connection object and get group membership details from the source.
  */
  std::set<std::string> managed_name_list{};
  for (auto failover_table_detail : failover_table_detail_list) {
    uint primary_weight{0}, secondary_weight{0}, port{0}, weight{0};
    std::string channel{}, host{}, managed_name{};

    std::tie(channel, host, port, std::ignore, weight, managed_name,
             primary_weight, secondary_weight) = failover_table_detail;

    if (is_monitor_killed(thd, nullptr)) return 1;

    /*
      3.1. To get group membership details, need to connect to only one member
           from the group.
    */
    if (managed_name_list.find(managed_name) != managed_name_list.end())
      continue;

    /* 3.2. Connect to source and store its connection object. */
    channel_map.rdlock();
    Master_info *mi = channel_map.get_mi(channel_name.c_str());
    if (nullptr == mi) {
      channel_map.unlock();
      return 2;
    }
    std::string mi_host{mi->host};
    uint mi_port = mi->port;
    const std::string mi_network_namespace(mi->network_namespace_str());

    THD_STAGE_INFO(thd, stage_connecting_to_source);
    Mysql_connection *conn =
        new Mysql_connection(thd, mi, host, port, mi_network_namespace);
    if (!conn->is_connected()) {
      LogErr(WARNING_LEVEL, ER_RPL_ASYNC_CHANNEL_CANT_CONNECT, host.c_str(),
             port, "", channel.c_str());
      delete conn;
      conn = nullptr;
      channel_map.unlock();
      continue;
    }

    /*
      3.3. Get group membership details for ONLINE, RECOVERING, UNREACHABLE
           members.
    */
    THD_STAGE_INFO(thd, stage_rpl_failover_fetching_source_member_details);
    std::vector<RPL_FAILOVER_SOURCE_TUPLE> group_membership_list{};
    int err{0};
    bool conn_member_needs_to_change{false}, conn_member_quorum_lost{false};

    /* Connection details of source who lost majority. It will be used to log */
    std::tuple<std::string, std::string, uint>
        conn_member_quorum_lost_details{};

    std::tie(err, conn_member_needs_to_change, conn_member_quorum_lost,
             conn_member_quorum_lost_details) =
        get_online_members(thd, mi, conn, failover_table_detail,
                           group_membership_list, curr_highest_group_weight,
                           curr_conn_weight);
    delete conn;
    conn = nullptr;
    channel_map.unlock();

    if (is_monitor_killed(thd, nullptr)) return 1;

    if (err == ER_RPL_ASYNC_GET_GROUP_MEMBERSHIP_DETAILS_ERROR ||
        err == ER_RPL_ASYNC_MONITOR_IO_THD_FETCH_GROUP_MAJORITY_ERROR) {
      continue;
    }

    /*
      3.4. Store gathered membership details to
           replication_asynchronous_connection_failover table.
    */
    THD_STAGE_INFO(thd, stage_rpl_failover_updating_source_member_details);
    if (!err && !group_membership_list.empty() &&
        !save_group_members(channel, managed_name, group_membership_list)) {
      /*
        Add the managed_name to the managed_name_list so that further members
        from the group can be ignored.
      */
      managed_name_list.insert(managed_name);
    } else if (err == 2) {
      return 1;
    }

    if (is_monitor_killed(thd, nullptr)) return 1;

    /*
      3.5. Disconnect channel if current connected member through
           asynchronous channel has changed the group or has lost quorum.
    */
    if (conn_member_needs_to_change || conn_member_quorum_lost) {
      const std::string error_channel =
          std::get<0>(conn_member_quorum_lost_details);
      const std::string error_host =
          std::get<1>(conn_member_quorum_lost_details);
      const uint error_port = std::get<2>(conn_member_quorum_lost_details);

      /* Get current values from mi. */
      channel_map.rdlock();
      mi = channel_map.get_mi(channel_name.c_str());
      if (nullptr == mi) {
        channel_map.unlock();
        return 2;
      }
      mi_host.assign(mi->host);
      mi_port = mi->port;
      channel_map.unlock();

      /*
        Only trigger the channel reconnection if the sender on which we detect
        the error is still the connected sender.
        Until we reached this point the IO thread may had switch by itself to
        another sender.
      */
      if (!error_channel.compare(channel_name) &&
          !error_host.compare(mi_host) && error_port == mi_port) {
        if (is_monitor_killed(thd, nullptr)) return 1;

        bool restarted = restart_io_thread(thd, channel_name, false);

        if (restarted && conn_member_quorum_lost) {
          LogErr(ERROR_LEVEL, ER_RPL_ASYNC_CHANNEL_STOPPED_QUORUM_LOST,
                 error_host.c_str(), error_port, "", error_channel.c_str());
        }
      }
    }
  }

  if (is_monitor_killed(thd, nullptr)) return 1;

  /*
    4. Get highest weight of single sender.
  */
  {
    channel_map.rdlock();
    Master_info *mi = channel_map.get_mi(channel_name.c_str());
    if (nullptr == mi) {
      channel_map.unlock();
      return 2;
    }
    for (auto source_conn_detail : source_conn_detail_list) {
      std::string group_name{""};
      uint weight{0};
      /* save highest weight of single senders for the current channel */
      std::tie(std::ignore, std::ignore, std::ignore, std::ignore, weight,
               group_name) = source_conn_detail;
      if (weight > curr_highest_weight_single_sender && group_name.empty() &&
          weight > curr_conn_weight && weight > curr_highest_group_weight &&
          check_connection_and_run_query(thd, mi, source_conn_detail)) {
        curr_highest_weight_single_sender = weight;
      }
    }
    channel_map.unlock();
  }

  if (is_monitor_killed(thd, nullptr)) return 1;

  /*
    5. If weight of current connected sender is less then any of
       ONLINE group member or single server, then disconnect it.
       The reconnection would be done by IO thread.
  */
  DBUG_EXECUTE_IF("async_conn_failover_disable_weight_check", return 0;);
  DBUG_EXECUTE_IF("async_conn_failover_check_interim_sender", {
    if (source_conn_detail_list.size() == 4) {
      return 0;
    }
  });
  if ((curr_highest_group_weight > curr_conn_weight) ||
      (curr_highest_weight_single_sender > curr_conn_weight)) {
    restart_io_thread(thd, channel_name, true);
  }

  return 0;
}

bool Source_IO_monitor::check_connection_and_run_query(
    THD *thd, Master_info *mi, RPL_FAILOVER_SOURCE_TUPLE &conn_detail) {
  uint query_failed{1};
  uint port{0};
  std::string host{""};
  const std::string mi_network_namespace(mi->network_namespace_str());
  std::tie(std::ignore, host, port, std::ignore, std::ignore, std::ignore) =
      conn_detail;

  Mysql_connection *conn_single_server =
      new Mysql_connection(thd, mi, host, port, mi_network_namespace);
  if (conn_single_server != nullptr && conn_single_server->is_connected())
    std::tie(query_failed, std::ignore) = execute_query(
        conn_single_server, enum_sql_query_tag::QUERY_SERVER_SELECT_ONE);
  if (query_failed != 0 && conn_single_server != nullptr) {
    Async_conn_failover_manager::log_error_for_async_executing_query_failure(
        ER_RPL_ASYNC_CHECK_CONNECTION_ERROR, conn_single_server->get_mysql(),
        mi);
  }
  delete conn_single_server;
  conn_single_server = nullptr;
  return !query_failed;
}

int Source_IO_monitor::save_group_members(
    std::string channel_name, std::string managed_name,
    std::vector<RPL_FAILOVER_SOURCE_TUPLE> &group_membership_list) {
  bool err_val{true};
  std::string err_msg{};

  std::vector<RPL_FAILOVER_SOURCE_TUPLE> failover_table_detail_list{};

  std::string db{"mysql"};
  std::string table_name{"replication_asynchronous_connection_failover"};
  uint num_field{6};
  enum thr_lock_type lock_type { TL_WRITE };
  std::vector<std::string> field_name{
      "channel", "host", "port", "network_namespace", "weight", "managed_name"};

  /* Open table with OPTION_AUTOCOMMIT disable. */
  Rpl_sys_table_access table_op(db, table_name, num_field);
  if (table_op.open(lock_type)) {
    table_op.set_error();
    return 1;
  }

  TABLE *table{table_op.get_table()};

  /*
    Read stored source details from
    replication_asynchronous_connection_failover table.
  */
  {
    /* Store channel */
    if (table_op.store_field(table->field[0], channel_name)) {
      table_op.set_error();
      return 1;
    }

    /* Store managed_name */
    if (table_op.store_field(table->field[5], managed_name)) {
      table_op.set_error();
    }

    Rpl_sys_key_access key_access;
    if (!key_access.init(table, 1, true, (key_part_map)((1L << 0) | (1L << 1)),
                         HA_READ_KEY_EXACT)) {
      do {
        /* get source detail */
        RPL_FAILOVER_SOURCE_TUPLE source_tuple{};
        Rpl_async_conn_failover_table_operations::get_data<
            RPL_FAILOVER_SOURCE_TUPLE>(table_op, source_tuple);
        failover_table_detail_list.push_back(source_tuple);
      } while (!key_access.next());
    }

    if (key_access.deinit()) {
      table_op.set_error();
      return 1;
    }

    if (failover_table_detail_list.empty()) return 1;
  }

  /*
    For each source from gathered membership details in step 3,
    check whether it's already present in failover table:
    - if present then delete its entry and insert again (weight can change).
    - if not present then insert.
  */
  for (auto group_member_detail : group_membership_list) {
    uint port{0}, weight{0};
    std::string channel{}, host{}, group_name{}, net_ns{};

    std::tie(channel, host, port, net_ns, weight, group_name) =
        group_member_detail;

    auto it = std::find_if(failover_table_detail_list.begin(),
                           failover_table_detail_list.end(),
                           [&](const RPL_FAILOVER_SOURCE_TUPLE &e) {
                             return ((std::get<0>(e).compare(channel) == 0) &&
                                     (std::get<1>(e).compare(host) == 0) &&
                                     (std::get<2>(e) == port));
                           });

    if (it != failover_table_detail_list.end()) {
      auto del_conn_detail = std::make_tuple(channel, host, port);
      std::tie(err_val, err_msg) =
          delete_rows(table_op, table, field_name, del_conn_detail);
      if (err_val) {
        table_op.set_error();
        table_op.close(err_val);
        return 1;
      }

      std::tie(err_val, err_msg) =
          write_rows(table_op, table, field_name, group_member_detail);
      if (err_val) {
        table_op.set_error();
        table_op.close(err_val);
        return 1;
      }
    }

    if (it == failover_table_detail_list.end()) {
      LogErr(SYSTEM_LEVEL, ER_RPL_ASYNC_SENDER_ADDED, host.c_str(), port, "",
             channel.c_str(), group_name.c_str());

      std::tie(err_val, err_msg) =
          write_rows(table_op, table, field_name, group_member_detail);
      if (err_val) {
        table_op.set_error();
        table_op.close(err_val);
        return 1;
      }
    }
  }

  /*
    For each source from failover table, check whether it was
    also found in membership details list :
    - if not found then delete its entry from failover table, as
      source has left the group.
  */
  for (auto failover_table_detail : failover_table_detail_list) {
    uint port{0}, weight{0};
    std::string channel{}, host{}, group_name{}, net_ns{};

    std::tie(channel, host, port, net_ns, weight, group_name) =
        failover_table_detail;

    auto it =
        std::find_if(group_membership_list.begin(), group_membership_list.end(),
                     [&](const RPL_FAILOVER_SOURCE_TUPLE &e) {
                       return ((std::get<1>(e).compare(host) == 0) &&
                               (std::get<2>(e) == port));
                     });

    if (it == group_membership_list.end()) {
      LogErr(SYSTEM_LEVEL, ER_RPL_ASYNC_SENDER_REMOVED, host.c_str(), port, "",
             channel.c_str(), group_name.c_str());
      auto del_conn_detail = std::make_tuple(channel, host, port);
      std::tie(err_val, err_msg) =
          delete_rows(table_op, table, field_name, del_conn_detail);
      if (err_val) {
        table_op.set_error();
        table_op.close(err_val);
        return 1;
      }
    }
  }

  /* Increment member action configuration version. */
  if (table_op.increment_version()) {
    LogErr(ERROR_LEVEL, ER_RPL_INCREMENTING_MEMBER_ACTION_VERSION, db.c_str(),
           table_name.c_str());
    return 1;
  }

  /*
    Send replication_asynchronous_connection_failover data to group replication
    group members.
  */
  if (rpl_acf_configuration_handler->send_failover_data(table_op)) {
    return 1;
  }

  return 0;
}

bool Source_IO_monitor::has_primary_lost_contact_with_majority() {
  bool primary_lost_contact_with_majority = false;
  my_h_service gr_status_service_handler = nullptr;

  srv_registry->acquire("group_replication_status_service_v1",
                        &gr_status_service_handler);
  if (nullptr != gr_status_service_handler) {
    SERVICE_TYPE(group_replication_status_service_v1) *gr_status_service =
        reinterpret_cast<SERVICE_TYPE(group_replication_status_service_v1) *>(
            gr_status_service_handler);

    if (gr_status_service
            ->is_group_in_single_primary_mode_and_im_the_primary() &&
        !gr_status_service->is_member_online_with_group_majority()) {
      primary_lost_contact_with_majority = true;
    }

    srv_registry->release(gr_status_service_handler);
  }

  return primary_lost_contact_with_majority;
}

std::tuple<int, bool, bool, std::tuple<std::string, std::string, uint>>
Source_IO_monitor::get_online_members(
    THD *thd, Master_info *mi, const Mysql_connection *conn,
    SENDER_CONN_MERGE_TUPLE failover_table_detail,
    std::vector<RPL_FAILOVER_SOURCE_TUPLE> &group_membership_list,
    uint &curr_highest_group_weight, uint &curr_conn_weight) {
  channel_map.assert_some_lock();
  uint error{0};
  std::string err_msg;
  bool conn_member_needs_to_change{false}, conn_member_quorum_lost{false};

  /* Connection details of source who lost majority. It will be used to log */
  std::tuple<std::string, std::string, uint> conn_member_quorum_lost_details{};

  uint primary_weight{0}, secondary_weight{0}, port{0}, weight{0};
  std::string channel{}, host{}, managed_name{};

  if (is_monitor_killed(thd, nullptr)) {
    return std::make_tuple(2, conn_member_needs_to_change,
                           conn_member_quorum_lost,
                           conn_member_quorum_lost_details);
  }

  std::tie(channel, host, port, std::ignore, weight, managed_name,
           primary_weight, secondary_weight) = failover_table_detail;

  /* Execute enum_sql_query_tag::CONFIG_MODE_QUORUM_MONITOR query */
  MYSQL_RES_VAL quorum_list{};
  auto qtag{enum_sql_query_tag::CONFIG_MODE_QUORUM_MONITOR};
  std::tie(error, quorum_list) = execute_query(conn, qtag);
  if (error != 0) {
    longlong sql_errno{ER_RPL_ASYNC_MONITOR_IO_THD_FETCH_GROUP_MAJORITY_ERROR};
    Async_conn_failover_manager::log_error_for_async_executing_query_failure(
        sql_errno, const_cast<Mysql_connection *>(conn)->get_mysql(), mi);
    return std::make_tuple(sql_errno, conn_member_needs_to_change,
                           conn_member_quorum_lost,
                           conn_member_quorum_lost_details);
  }

  if (quorum_list.empty() || quorum_list[0].empty()) {
    return std::make_tuple(1, conn_member_needs_to_change,
                           conn_member_quorum_lost,
                           conn_member_quorum_lost_details);
  }

  auto quorum_status{
      static_cast<enum_conf_mode_quorum_status>(std::stoi(quorum_list[0][0]))};
  if (quorum_status == enum_conf_mode_quorum_status::MANAGED_GR_HAS_QUORUM) {
    qtag = enum_sql_query_tag::GR_MEMBER_ALL_DETAILS;
    MYSQL_RES_VAL sender_membership_res{};
    std::tie(error, sender_membership_res) = execute_query(conn, qtag);

    if (error == ER_BAD_FIELD_ERROR) {
      qtag = enum_sql_query_tag::GR_MEMBER_ALL_DETAILS_FETCH_FOR_57;
      std::tie(error, sender_membership_res) = execute_query(conn, qtag);
    }

    if (error != 0) {
      longlong sql_errno{ER_RPL_ASYNC_GET_GROUP_MEMBERSHIP_DETAILS_ERROR};
      Async_conn_failover_manager::log_error_for_async_executing_query_failure(
          sql_errno, const_cast<Mysql_connection *>(conn)->get_mysql(), mi);
      return std::make_tuple(sql_errno, conn_member_needs_to_change,
                             conn_member_quorum_lost,
                             conn_member_quorum_lost_details);
    }

    /*
      If current connected sender is group member and not a single server
      then save its primary/secondary weight based on role.
    */
    for (const auto &m_row_ins : sender_membership_res) {
      if (m_row_ins[COL_HOST].compare(mi->host) == 0 &&
          std::stoul(m_row_ins[COL_PORT]) == mi->port) {
        if (m_row_ins[COL_ROLE].compare("PRIMARY") == 0) {
          curr_conn_weight = primary_weight;
        } else if (m_row_ins[COL_ROLE].compare("SECONDARY") == 0) {
          curr_conn_weight = secondary_weight;
        }
      }
    }

    for (const auto &m_row : sender_membership_res) {
      /*
         If member is ONLINE then add member connection details to
         replication_asynchronous_connection_failover table.
      */
      if (m_row[COL_STATE].compare("ONLINE") == 0 ||
          m_row[COL_STATE].compare("RECOVERING") == 0 ||
          m_row[COL_STATE].compare("UNREACHABLE") == 0) {
        if (is_monitor_killed(thd, nullptr)) {
          return std::make_tuple(2, conn_member_needs_to_change,
                                 conn_member_quorum_lost,
                                 conn_member_quorum_lost_details);
        }

        uint tab_weight{secondary_weight};
        if (m_row[COL_ROLE].compare("PRIMARY") == 0) {
          tab_weight = primary_weight;
          if ((primary_weight > curr_highest_group_weight) &&
              m_row[COL_STATE].compare("ONLINE") == 0)
            curr_highest_group_weight = primary_weight;
        } else if (m_row[COL_ROLE].compare("SECONDARY") == 0) {
          if ((secondary_weight > curr_highest_group_weight) &&
              m_row[COL_STATE].compare("ONLINE") == 0)
            curr_highest_group_weight = secondary_weight;
        }

        auto source_ins_details = std::make_tuple(
            channel, m_row[COL_HOST], std::stoul(m_row[COL_PORT]), "",
            tab_weight, m_row[COL_GROUP_NAME]);
        group_membership_list.push_back(source_ins_details);
      }

      /*
         For the source connected through asynchronous channel,
         if the group_name has changed i.e. member has changed group, or,
         if its state become UNREACHABLE i.e. lost majority, then stop
         the asynchronous channel.
      */
      if (m_row[COL_HOST].compare(mi->host) == 0 &&
          std::stoul(m_row[COL_PORT]) == mi->port &&
          (m_row[COL_GROUP_NAME].compare(managed_name) != 0 ||
           m_row[COL_STATE].compare("UNREACHABLE") == 0)) {
        conn_member_needs_to_change = true;
      }
    }
  }

  if (quorum_status == enum_conf_mode_quorum_status::MANAGED_GR_HAS_ERROR &&
      host.compare(mi->host) == 0 && port == mi->port) {
    conn_member_quorum_lost = true;
    conn_member_quorum_lost_details = std::make_tuple(channel, host, port);
  }

  return std::make_tuple(0, conn_member_needs_to_change,
                         conn_member_quorum_lost,
                         conn_member_quorum_lost_details);
}

int Source_IO_monitor::sync_senders_details(THD *thd) {
  bool primary_lost_contact_with_majority =
      has_primary_lost_contact_with_majority();

  if (primary_lost_contact_with_majority) {
    /* Log the warning only once per majority loss. */
    if (!m_primary_lost_contact_with_majority_warning_logged) {
      m_primary_lost_contact_with_majority_warning_logged = true;
      LogErr(WARNING_LEVEL, ER_GRP_RPL_FAILOVER_PRIMARY_WITHOUT_MAJORITY);
    }
    return 0;
  } else {
    if (m_primary_lost_contact_with_majority_warning_logged) {
      m_primary_lost_contact_with_majority_warning_logged = false;
      LogErr(WARNING_LEVEL, ER_GRP_RPL_FAILOVER_PRIMARY_BACK_TO_MAJORITY);
    }
  }

  std::vector<std::string> channels;
  channel_map.rdlock();
  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    Master_info *mi = it->second;
    if (Master_info::is_configured(mi) &&
        mi->is_source_connection_auto_failover()) {
      channels.push_back(mi->get_channel());
    }
  }
  channel_map.unlock();

  for (const std::string &channel_name : channels) {
    connect_senders(thd, channel_name);
  }

  return 0;
}

std::tuple<bool, std::vector<SENDER_CONN_MERGE_TUPLE>>
Source_IO_monitor::get_senders_details(const std::string &channel_name) {
  DBUG_TRACE;

  /* The list of source connection details. */
  std::vector<SENDER_CONN_MERGE_TUPLE> failover_table_detail_list{};
  std::vector<RPL_FAILOVER_MANAGED_TUPLE> source_managed_list{};
  auto error{false};

  /*
    Check if source needs to be managed, if true then get its network
    configuration details.
    These tables can be modified in parallel, which will cause its open() to
    fail, on that case we do retry the operation.
  */
  int retries = 0;
  do {
    if (retries > 0) {
      my_sleep(1000);
    }

    Rpl_async_conn_failover_table_operations table_op(TL_READ);
    error = table_op.read_managed_rows_for_channel(channel_name,
                                                   source_managed_list);

    if (error) return make_pair(error, failover_table_detail_list);

    for (auto source_managed_detail : source_managed_list) {
      auto primary_weight{std::get<3>(source_managed_detail)},
          secondary_weight{std::get<4>(source_managed_detail)};
      if (!error &&
          strcmp(std::get<2>(source_managed_detail).c_str(),
                 "GroupReplication") == 0 &&
          !std::get<1>(source_managed_detail).empty()) {
        std::vector<RPL_FAILOVER_SOURCE_TUPLE> source_conn_detail_list{};
        Rpl_async_conn_failover_table_operations table_op_src(TL_READ);
        std::tie(error, source_conn_detail_list) =
            table_op_src.read_source_rows_for_channel_and_managed_name(
                channel_name, std::get<1>(source_managed_detail));
        for (auto source_conn_detail : source_conn_detail_list) {
          auto source_conn_detail_merged =
              std::tuple_cat(source_conn_detail,
                             std::make_tuple(primary_weight, secondary_weight));
          failover_table_detail_list.push_back(source_conn_detail_merged);
        }
      }
    }

    retries++;
  } while (error && retries < 10);

  if (error) {
    LogErr(WARNING_LEVEL, ER_RPL_ASYNC_READ_FAILOVER_TABLE,
           channel_name.c_str());
  }

  return make_pair(error, failover_table_detail_list);
}

int Source_IO_monitor::terminate_monitoring_process() {
  mysql_mutex_lock(&m_run_lock);

  if (m_monitor_thd_state.is_thread_dead()) {
    mysql_mutex_unlock(&m_run_lock);
    return 0;
  }

  // Awake up possible stuck conditions
  mysql_cond_broadcast(&m_run_cond);

  ulong stop_wait_timeout = rpl_stop_replica_timeout;
  while (m_monitor_thd_state.is_thread_alive()) {
    DBUG_PRINT("sleep",
               ("Waiting for the Monitoring IO process thread to finish"));

    if (m_monitor_thd_state.is_initialized()) {
      mysql_mutex_lock(&m_monitor_thd->LOCK_thd_data);
      m_monitor_thd->awake(THD::KILL_CONNECTION);
      mysql_mutex_unlock(&m_monitor_thd->LOCK_thd_data);
    }

    struct timespec abstime;
    set_timespec(&abstime, (stop_wait_timeout == 1 ? 1 : 2));
#ifndef NDEBUG
    int error =
#endif
        mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);

    if (stop_wait_timeout >= 1) {
      stop_wait_timeout = stop_wait_timeout - (stop_wait_timeout == 1 ? 1 : 2);
    }

    if (m_monitor_thd_state.is_thread_alive() &&
        stop_wait_timeout <= 0)  // quit waiting
    {
      mysql_mutex_unlock(&m_run_lock);
      return 1;
    }

    assert(error == ETIMEDOUT || error == 0);
  }
  assert(m_monitor_thd_state.is_thread_dead());

  mysql_mutex_unlock(&m_run_lock);
  return 0;
}

void Source_IO_monitor::set_monitoring_wait(uint wait_time) {
  m_retry_monitor_wait = wait_time;
}

uint Source_IO_monitor::get_monitoring_wait() { return m_retry_monitor_wait; }

bool Source_IO_monitor::is_monitoring_process_running() {
  return m_monitor_thd_state.is_thread_alive();
}

Source_IO_monitor *Source_IO_monitor::get_instance() {
  return rpl_source_io_monitor;
}

static bool restart_io_thread(THD *thd, const std::string &channel_name,
                              bool force_sender_with_highest_weight) {
  if (channel_map.trywrlock()) {
    return false;
  }

  Master_info *mi = channel_map.get_mi(channel_name.c_str());
  if (nullptr == mi) {
    channel_map.unlock();
    return false;
  }

  if (Async_conn_failover_manager::do_auto_conn_failover(
          mi, force_sender_with_highest_weight) !=
      Async_conn_failover_manager::DoAutoConnFailoverError::no_error) {
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_MONITOR_IO_THREAD_RECONNECT_CHANNEL,
           "choosing the source for", channel_name.c_str());
    channel_map.unlock();
    return false;
  }

  mi->channel_wrlock();
  lock_slave_threads(mi);

  /*
    IO thread was stopped through STOP REPLICA, do not restart it.
  */
  if (!mi->is_source_connection_auto_failover() || !mi->slave_running) {
    unlock_slave_threads(mi);
    mi->channel_unlock();
    channel_map.unlock();
    return false;
  }

  int thread_mask = 0;
  thread_mask |= REPLICA_IO;
  thd->set_skip_readonly_check();

  if (terminate_slave_threads(mi, thread_mask, rpl_stop_replica_timeout,
                              false /*need_lock_term=false*/)) {
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_MONITOR_IO_THREAD_RECONNECT_CHANNEL,
           "stopping", channel_name.c_str());
  }

  if (start_slave_threads(false /*need_lock_slave=false*/,
                          true /*wait_for_start=true*/, mi, thread_mask)) {
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_MONITOR_IO_THREAD_RECONNECT_CHANNEL,
           "starting", channel_name.c_str());
  }

  thd->reset_skip_readonly_check();
  unlock_slave_threads(mi);
  mi->channel_unlock();
  channel_map.unlock();

  return true;
}
