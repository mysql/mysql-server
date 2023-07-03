/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

/**
@file clone/src/clone_client.cc
Clone Plugin: Client implementation

*/
#include <inttypes.h>

#include "plugin/clone/include/clone_client.h"
#include "plugin/clone/include/clone_os.h"

#include "my_byteorder.h"
#include "my_systime.h"  // my_sleep()
#include "sql/sql_thd_internal_api.h"
#include "sql_string.h"

/* Namespace for all clone data types */
namespace myclone {

/** Default timeout is 300 seconds */
Time_Sec Client::s_reconnect_timeout{300};

/** Minimum interval is 5 seconds. The actual value could be more based on
MySQL connect_timeout configuration. */
Time_Sec Client::s_reconnect_interval{5};

/** Start concurrent clone  operation
@param[in]	share	shared client information
@param[in]	index	current thread index */
static void clone_client(Client_Share *share, uint32_t index) {
  THD *thd = nullptr;

  /* Create a session statement and set PFS keys */
  mysql_service_clone_protocol->mysql_clone_start_statement(
      thd, clone_client_thd_key, PSI_NOT_INSTRUMENTED);

  Client clone_inst(thd, share, index, false);

  clone_inst.clone();

  /* Drop the statement and session */
  mysql_service_clone_protocol->mysql_clone_finish_statement(thd);
}

uint64_t Thread_Info::get_target_time(uint64_t current, uint64_t prev,
                                      uint64_t target) {
  /* Target zero implies no throttling. */
  if (target == 0) {
    return (target);
  }
  assert(current >= prev);
  auto bytes = current - prev;
  auto target_time_ms = (bytes * 1000) / target;
  return (target_time_ms);
}

void Thread_Info::throttle(uint64_t data_target, uint64_t net_target) {
  auto cur_time = Clock::now();
  auto duration =
      std::chrono::duration_cast<Time_Msec>(cur_time - m_last_update);

  /* Check only at specific intervals. */
  if (duration < m_interval) {
    return;
  }

  /* Find the amount of time we should have taken based on the targets. */
  auto d_tm = get_target_time(m_data_bytes, m_last_data_bytes, data_target);
  auto n_tm =
      get_target_time(m_network_bytes, m_last_network_bytes, net_target);
  auto target_ms = std::max(d_tm, n_tm);

  auto duration_ms = static_cast<uint64_t>(duration.count());

  /* Sleep for the remaining time to throttle clone data transfer. */
  if (target_ms > duration_ms) {
    auto sleep_ms = target_ms - duration_ms;

    /* Don't sleep for more than 1 second so that we don't get into
    network timeout and can respond to abort/shutdown request. */
    if (sleep_ms > 1000) {
      sleep_ms = 1000;
      /* Lower check interval as we need to sleep more. This way
      we sleep more frequently. */
      m_interval = m_interval / 2;
    }
    Time_Msec sleep_time(sleep_ms);
    std::this_thread::sleep_for(sleep_time);
  } else {
    /* Reset interval back to default 100ms. */
    m_interval = Time_Msec{100};
  }
  m_last_data_bytes = m_data_bytes;
  m_last_network_bytes = m_network_bytes;
  m_last_update = Clock::now();
}

void Client_Stat::update(bool reset, const Thread_Vector &threads,
                         uint32_t num_workers) {
  /* Ignore reset requests when stat is not initialized. */
  if (!m_initialized && reset) {
    return;
  }
  auto cur_time = Clock::now();

  /* Start time is set at first call. */
  if (!m_initialized) {
    m_start_time = cur_time;
    m_initialized = true;
    reset_history(true);
    set_target_bandwidth(num_workers, true, 0, 0);
    return;
  }

  auto duration_ms =
      std::chrono::duration_cast<Time_Msec>(cur_time - m_eval_time);
  if (duration_ms < m_interval && !reset) {
    return;
  }

  m_eval_time = cur_time;
  uint64_t value_ms = duration_ms.count();

  uint64_t data_bytes = m_finished_data_bytes;
  uint64_t net_bytes = m_finished_network_bytes;

  /* Evaluate total data and network bytes transferred till now. */
  for (uint32_t index = 0; index <= num_workers; ++index) {
    auto &thread_info = threads[index];
    data_bytes += thread_info.m_data_bytes;
    net_bytes += thread_info.m_network_bytes;
  }

  /* Evaluate the transfer speed from last evaluation time. */
  auto cur_index = m_current_history_index % STAT_HISTORY_SIZE;
  ++m_current_history_index;

  uint64_t data_speed{};
  uint64_t net_speed{};
  if (value_ms == 0) {
    /* We might be too early here during reset. */
    assert(reset);
  } else {
    /* Update PFS in bytes per second. */
    assert(data_bytes >= m_eval_data_bytes);
    auto data_inc = data_bytes - m_eval_data_bytes;

    assert(net_bytes >= m_eval_network_bytes);
    auto net_inc = net_bytes - m_eval_network_bytes;

    data_speed = (data_inc * 1000) / value_ms;
    net_speed = (net_inc * 1000) / value_ms;
    Client::update_pfs_data(data_inc, net_inc, data_speed, net_speed,
                            num_workers);
  }

  /* Calculate speed in MiB per second. */
  auto data_speed_mib = data_speed / (1024 * 1024);
  auto net_speed_mib = net_speed / (1024 * 1024);

  m_data_speed_history[cur_index] = data_speed_mib;
  m_network_speed_history[cur_index] = net_speed_mib;

  /* Set currently evaluated data. */
  m_eval_data_bytes = data_bytes;
  m_eval_network_bytes = net_bytes;

  if (reset) {
    /* Convert to Mebibytes (MiB) */
    auto total_data_mb = data_bytes / (1024 * 1024);
    auto total_net_mb = net_bytes / (1024 * 1024);

    /* Find and log cumulative data transfer rate. */
    duration_ms =
        std::chrono::duration_cast<Time_Msec>(cur_time - m_start_time);
    value_ms = duration_ms.count();

    data_speed_mib = (value_ms == 0) ? 0 : (total_data_mb * 1000) / value_ms;
    net_speed_mib = (value_ms == 0) ? 0 : (total_net_mb * 1000) / value_ms;

    /* Log current speed. */
    char info_mesg[128];

    snprintf(info_mesg, 128,
             "Total Data: %" PRIu64 " MiB @ %" PRIu64
             " MiB/sec, Network: %" PRIu64 " MiB @ %" PRIu64 " MiB/sec",
             total_data_mb, data_speed_mib, total_net_mb, net_speed_mib);

    LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, info_mesg);
    reset_history(false);
  }

  /* Set targets for all tasks. */
  set_target_bandwidth(num_workers, reset, data_speed, net_speed);
}

uint64_t Client_Stat::task_target(uint64_t target_speed, uint64_t current_speed,
                                  uint64_t current_target, uint32_t num_tasks) {
  assert(num_tasks > 0);

  /* Zero is special value indicating unlimited bandwidth. */
  if (target_speed == 0) {
    return (0);
  }

  /* Estimate number of active tasks based on current performance. If target is
  not set yet, start by assuming all active thread. */
  auto active_tasks =
      (current_target == 0) ? num_tasks : (current_speed / current_target);

  /* Keep the value within current boundary. */
  if (active_tasks == 0) {
    active_tasks = 1;
  } else if (active_tasks > num_tasks) {
    active_tasks = num_tasks;
  }

  auto task_target = target_speed / active_tasks;

  /* Don't set anything lower than a minimum threshold. Protection against
  bad configuration asking too many threads and very less bandwidth. */
  if (task_target < m_minimum_speed) {
    task_target = m_minimum_speed;
  }
  return (task_target);
}

void Client_Stat::set_target_bandwidth(uint32_t num_workers, bool is_reset,
                                       uint64_t data_speed,
                                       uint64_t net_speed) {
  ++num_workers;

  uint64_t data_target = clone_max_io_bandwidth * 1024 * 1024;
  uint64_t net_target = clone_max_network_bandwidth * 1024 * 1024;

  if (!is_reset) {
    data_target =
        task_target(data_target, data_speed, m_target_data_speed, num_workers);

    net_target =
        task_target(net_target, net_speed, m_target_network_speed, num_workers);
  }

  m_target_data_speed.store(data_target);
  m_target_network_speed.store(net_target);
}

void Client_Stat::reset_history(bool init) {
  m_data_speed_history.fill(0);
  m_network_speed_history.fill(0);
  m_current_history_index = 0;

  /* Set evaluation results during initialization. */
  if (init) {
    m_eval_data_bytes = 0;
    m_finished_data_bytes = 0;
    m_eval_network_bytes = 0;
    m_finished_network_bytes = 0;
    m_eval_time = Clock::now();
  }

  /** Reset auto tuning information. */
  m_tune.reset();
}

bool Client_Stat::is_bandwidth_saturated() {
  if (m_current_history_index == 0) {
    return (false);
  }
  auto last_index = (m_current_history_index - 1) % STAT_HISTORY_SIZE;

  /* Check if data speed is close to the limit. We consider it saturated if 90%
  is reached and stop spawning more threads. */
  auto data_speed = m_data_speed_history[last_index];
  auto max_io = clone_max_io_bandwidth;

  /* Zero implies no limit on bandwidth. */
  if (max_io != 0) {
    max_io *= 0.9;
    if (data_speed > max_io) {
      return (true);
    }
  }
  /* Check if network speed is close to the limit. */
  auto net_speed = m_network_speed_history[last_index];
  auto max_net = clone_max_network_bandwidth;

  if (max_net != 0) {
    max_net *= 0.9;
    if (net_speed > max_net) {
      return (true);
    }
  }
  return (false);
}

bool Client_Stat::tune_has_improved(uint32_t num_threads) {
  char info_mesg[128];
  if (m_tune.m_cur_number != num_threads) {
    snprintf(info_mesg, 128, "Tune stop, current: %u expected: %u", num_threads,
             m_tune.m_cur_number);
    return (false);
  }
  auto gap_target = m_tune.m_next_number - m_tune.m_prev_number;
  auto gap_current = m_tune.m_cur_number - m_tune.m_prev_number;

  assert(m_current_history_index > 0);
  auto last_index = (m_current_history_index - 1) % STAT_HISTORY_SIZE;
  auto data_speed = m_data_speed_history[last_index];
  auto target_speed = m_tune.m_prev_speed;

  if (gap_target == gap_current) {
    /* We continue if at least 25% improvement after reaching target. */
    target_speed *= 1.25;
  } else if (gap_current >= gap_target / 2) {
    /* We continue if at least 10% improvement after reaching 50% target. */
    target_speed *= 1.10;
  } else if (gap_current >= gap_target / 4) {
    /* We continue if at least 5% improvement after reaching 25% target. */
    target_speed *= 1.05;
  } else {
    /* we continue only if hasn't degraded for other steps. */
    target_speed = m_tune.m_last_step_speed;
    target_speed *= 0.95;
  }

  if (data_speed < target_speed) {
    snprintf(info_mesg, 128,
             "Tune stop, Data: %" PRIu64 " MiB/sec, Target: %" PRIu64
             " MiB/sec.",
             data_speed, target_speed);
  } else {
    snprintf(info_mesg, 128,
             "Tune continue, Data: %" PRIu64 " MiB/sec, Target: %" PRIu64
             " MiB/sec",
             data_speed, target_speed);
  }
  LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, info_mesg);

  return (data_speed >= target_speed);
}

void Client_Stat::tune_set_target(uint32_t num_threads, uint32_t max_threads) {
  /* Note the current speed of data transfer. */
  assert(m_current_history_index > 0);
  auto last_index = (m_current_history_index - 1) % STAT_HISTORY_SIZE;
  auto current_speed = m_data_speed_history[last_index];
  /* Check if we have reached current target. */
  if (m_tune.m_cur_number == m_tune.m_next_number) {
    /* Set new target */
    m_tune.m_prev_number = num_threads;
    m_tune.m_cur_number = num_threads;
    /* Next target is double the number of threads. */
    m_tune.m_next_number = 2 * num_threads;
    /* Should not exceed maximum concurrency. */
    if (m_tune.m_next_number > max_threads) {
      m_tune.m_next_number = max_threads;
    }
    m_tune.m_prev_speed = current_speed;
  }
  assert(m_tune.m_cur_number == num_threads);
  /* We attempt to improve performance by adding more threads in steps. */
  m_tune.m_cur_number += m_tune.m_step;
  m_tune.m_last_step_speed = current_speed;

  /* Should not set more than the current target. */
  if (m_tune.m_cur_number > m_tune.m_next_number) {
    m_tune.m_cur_number = m_tune.m_next_number;
  }
  char info_mesg[128];
  snprintf(info_mesg, 128, "Tune Threads from: %u to: %u prev: %u target: %u",
           num_threads, m_tune.m_cur_number, m_tune.m_prev_number,
           m_tune.m_next_number);
  LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, info_mesg);
}

uint32_t Client_Stat::get_tuned_thread_number(uint32_t num_threads,
                                              uint32_t max_threads) {
  if (m_current_history_index < m_tune.m_prev_history_index) {
    assert(false); /* purecov: inspected */
    return (num_threads);
  }
  auto interval = m_current_history_index - m_tune.m_prev_history_index;
  /* Wait till some history is populated. */
  if (interval < m_tune.m_history_interval) {
    return (num_threads);
  }
  m_tune.m_prev_history_index = m_current_history_index;
  /* No more tuning once we have reached DONE state. */
  if (m_tune.m_state == Thread_Tune_Auto::State::DONE) {
    return (num_threads);
  }
  /** Cannot go beyond maximum number of threads. */
  if (num_threads >= max_threads || is_bandwidth_saturated()) {
    finish_tuning();
    return (num_threads);
  }
  /* Go to active state and set target. */
  if (m_tune.m_state == Thread_Tune_Auto::State::INIT) {
    tune_set_target(num_threads, max_threads);
    m_tune.m_state = Thread_Tune_Auto::State::ACTIVE;
    return (m_tune.m_cur_number);
  }
  assert(m_tune.m_state == Thread_Tune_Auto::State::ACTIVE);
  /* If it failed to improve speed, give up tuning. */
  if (!tune_has_improved(num_threads)) {
    finish_tuning();
    return (m_tune.m_cur_number);
  }
  /* Successfully increased threads with good improvement. */
  tune_set_target(num_threads, max_threads);

  return (m_tune.m_cur_number);
}

Client::Client(THD *thd, Client_Share *share, uint32_t index, bool is_master)
    : m_server_thd(thd),
      m_conn(),
      m_is_master(is_master),
      m_thread_index(index),
      m_num_active_workers(),
      m_storage_initialized(false),
      m_storage_active(false),
      m_acquired_backup_lock(false),
      m_share(share) {
  m_ext_link.set_socket(MYSQL_INVALID_SOCKET);

  /* Master must be at index zero */
  if (is_master) {
    assert(index == 0);
    m_thread_index = 0;
  }

  /* Reset thread statistics. */
  auto &info = get_thread_info();
  info.reset();

  m_tasks.reserve(MAX_CLONE_STORAGE_ENGINE);

  m_copy_buff.init();
  m_cmd_buff.init();

  m_conn_aux.m_conn = nullptr;
  m_conn_aux.reset();

  net_server_ext_init(&m_conn_server_extn);
}

Client::~Client() {
  assert(!m_storage_initialized);
  assert(!m_storage_active);
  m_copy_buff.free();
  m_cmd_buff.free();
}

bool Client::is_network_error(int err, bool protocol_error) {
  /* Check for read/write error */
  if (err == ER_NET_ERROR_ON_WRITE || err == ER_NET_READ_ERROR ||
      err == ER_NET_WRITE_INTERRUPTED || err == ER_NET_READ_INTERRUPTED ||
      err == ER_NET_WAIT_ERROR) {
    return (true);
  }

  /* Check for protocol/shutdown error */
  if (err == ER_NET_PACKETS_OUT_OF_ORDER || err == ER_NET_UNCOMPRESS_ERROR ||
      err == ER_NET_PACKET_TOO_LARGE || err == ER_QUERY_INTERRUPTED ||
      err == ER_CLONE_PROTOCOL) {
    return (protocol_error);
  }

  return (false);
}

uint32_t Client::update_stat(bool is_reset) {
  /* Statistics is updated by master task. */
  if (!is_master()) {
    return (m_num_active_workers);
  }
  auto &stat = m_share->m_stat;
  stat.update(is_reset, m_share->m_threads, m_num_active_workers);

  if (is_reset) {
    return (m_num_active_workers);
  }
  /** Check if we need to spawn more threads. */
  auto num_threads = stat.get_tuned_thread_number(m_num_active_workers + 1,
                                                  get_max_concurrency());
  assert(num_threads >= 1);
  return (num_threads - 1);
}

void Client::check_and_throttle() {
  uint64_t data_speed{};
  uint64_t net_speed{};

  auto &stat = m_share->m_stat;
  stat.get_target(data_speed, net_speed);

  auto &info = get_thread_info();
  info.throttle(data_speed, net_speed);
}

uchar *Client::get_aligned_buffer(uint32_t len) {
  auto err = m_copy_buff.allocate(len + CLONE_OS_ALIGN);

  if (err != 0) {
    return (nullptr);
  }

  /* Align buffer to CLONE_OS_ALIGN[4K] for O_DIRECT */
  auto buf_ptr = clone_os_align(m_copy_buff.m_buffer);

  return (buf_ptr);
}

void Client::wait_for_workers() {
  if (!is_master()) {
    assert(m_num_active_workers == 0);
    return;
  }
  /* Wait for concurrent worker tasks to finish. */
  auto &thread_vector = m_share->m_threads;
  assert(thread_vector.size() > m_num_active_workers);
  auto &stat = m_share->m_stat;

  while (m_num_active_workers > 0) {
    auto &info = thread_vector[m_num_active_workers];
    info.m_thread.join();

    /* Save all transferred bytes by the thread. */
    stat.save_at_exit(info.m_data_bytes, info.m_network_bytes);
    info.reset();

    --m_num_active_workers;
  }
  /* Save all transferred bytes by master thread. */
  auto &info = get_thread_info();
  stat.save_at_exit(info.m_data_bytes, info.m_network_bytes);
  info.reset();

  /* Reset stat and tuning information for next cycle after restart. */
  stat.reset_history(false);
}

int Client::pfs_begin_state() {
  if (!is_master()) {
    return (0);
  }
  mysql_mutex_lock(&s_table_mutex);
  /* Check and exit if concurrent clone in progress. */
  if (s_num_clones != 0) {
    mysql_mutex_unlock(&s_table_mutex);
    assert(s_num_clones == 1);
    my_error(ER_CLONE_TOO_MANY_CONCURRENT_CLONES, MYF(0), 1);
    return (ER_CLONE_TOO_MANY_CONCURRENT_CLONES);
  }
  s_num_clones = 1;
  s_status_data.begin(1, get_thd(), m_share->m_host, m_share->m_port,
                      get_data_dir());
  s_progress_data.init_stage(get_data_dir());
  mysql_mutex_unlock(&s_table_mutex);

  return (0);
}

void Client::pfs_change_stage(uint64_t estimate) {
  if (!is_master()) {
    return;
  }
  mysql_mutex_lock(&s_table_mutex);
  s_progress_data.end_stage(false, get_data_dir());
  s_progress_data.begin_stage(1, get_data_dir(), m_num_active_workers + 1,
                              estimate);
  s_status_data.write(false);
  mysql_mutex_unlock(&s_table_mutex);
}

void Client::pfs_end_state(uint32_t err_num, const char *err_mesg) {
  if (!is_master()) {
    return;
  }
  mysql_mutex_lock(&s_table_mutex);
  assert(s_num_clones == 1);

  bool provisioning = (get_data_dir() == nullptr);
  bool failed = (err_num != 0);

  /* In case provisioning is successful, clone operation is still
  in progress and will continue after restart. */
  if (!provisioning || failed) {
    s_num_clones = 0;
  }

  s_progress_data.end_stage(failed, get_data_dir());
  s_status_data.end(err_num, err_mesg, provisioning);
  mysql_mutex_unlock(&s_table_mutex);
}

void Client::copy_pfs_data(Status_pfs::Data &pfs_data) {
  mysql_mutex_lock(&s_table_mutex);
  /* If clone operation is started skip recovering previous data. */
  if (s_num_clones == 0) {
    s_status_data.recover();
  }
  pfs_data = s_status_data;
  mysql_mutex_unlock(&s_table_mutex);
}

void Client::copy_pfs_data(Progress_pfs::Data &pfs_data) {
  mysql_mutex_lock(&s_table_mutex);
  pfs_data = s_progress_data;
  mysql_mutex_unlock(&s_table_mutex);
}

void Client::update_pfs_data(uint64_t data, uint64_t network,
                             uint32_t data_speed, uint32_t net_speed,
                             uint32_t num_workers) {
  s_progress_data.update_data(data, network, data_speed, net_speed,
                              num_workers);
}

bool Client::s_pfs_initialized = false;

void Client::init_pfs() {
  mysql_mutex_init(PSI_NOT_INSTRUMENTED, &s_table_mutex, MY_MUTEX_INIT_FAST);
  /* Recover PFS data. */
  s_progress_data.read();
  s_status_data.read();
  s_pfs_initialized = true;
}

void Client::uninit_pfs() {
  if (s_pfs_initialized) {
    mysql_mutex_destroy(&s_table_mutex);
  }
  s_pfs_initialized = false;
}

uint32_t Client::limit_buffer(uint32_t buffer_size) {
  /* Limit total buffer size to 128 M */
  uint32_t max_buffer_size = 128 * 1024 * 1024;
  auto num_tasks = get_max_concurrency();

  auto limit = max_buffer_size / num_tasks;

  if (buffer_size > limit) {
    buffer_size = limit;
  }
  return (buffer_size);
}

uint32_t Client::limit_workers(uint32_t num_workers) {
  /* Adjust if network bandwidth is limited. Currently 64 M
  minimum per task is ensured before spawning task. */
  if (clone_max_network_bandwidth > 0) {
    /* Zero is also valid result for the limit. Workers are over and above
    the master task. So, anything less than 64M would mean no workers to
    spawn immediately. */
    uint32_t limit = clone_max_network_bandwidth / 64;
    if (num_workers > limit) {
      num_workers = limit;
    }
  }

  /* Adjust if data bandwidth is limited. Currently 64 M
  minimum per task is ensured before spawning task. */
  if (clone_max_io_bandwidth > 0) {
    /* Zero is also valid result for the limit. Workers are over and above
    the master task. So, anything less than 64M would mean no workers to
    spawn immediately. */
    uint32_t limit = clone_max_io_bandwidth / 64;
    if (num_workers > limit) {
      num_workers = limit;
    }
  }
  return (num_workers);
}

int Client::clone() {
  bool restart = false;
  uint restart_count = 0;
  char info_mesg[128];

  auto num_workers = get_max_concurrency() - 1;

  /* Begin PFS state if no concurrent clone in progress. */
  auto err = pfs_begin_state();
  if (err != 0) {
    return (err);
  }

  do {
    ++restart_count;

    err = connect_remote(restart, false);
    log_error(get_thd(), true, err, "Task Connect");

    if (err != 0) {
      break;
    }

    /* Make another auxiliary connection for ACK */
    err = connect_remote(restart, true);

    if (is_master()) {
      log_error(get_thd(), true, err, "Master ACK Connect");
    }

    if (err != 0) {
      assert(is_master());
      assert(m_conn == nullptr);
      assert(m_conn_aux.m_conn == nullptr);

      if (restart) {
        continue;
      }
      break;
    }

    auto rpc_com = is_master() ? COM_INIT : COM_ATTACH;

    if (restart) {
      assert(is_master());
      rpc_com = COM_REINIT;
    }

    /* Negotiate clone protocol and SE versions */
    err = remote_command(rpc_com, false);

    /* Delay clone after dropping database if requested */

    if (err == 0 && rpc_com == COM_INIT) {
      assert(is_master());
      err = delay_if_needed();
    }
    snprintf(
        info_mesg, 128, "Command %s",
        is_master() ? (restart ? "COM_REINIT" : "COM_INIT") : "COM_ATTACH");
    log_error(get_thd(), true, err, &info_mesg[0]);

    /* Execute clone command */
    if (err == 0) {
      /* Spawn concurrent client tasks if auto tuning is off. */
      if (!clone_autotune_concurrency) {
        /* Limit number of workers based on other configurations. */
        auto to_spawn = limit_workers(num_workers);
        using namespace std::placeholders;
        auto func = std::bind(clone_client, _1, _2);
        spawn_workers(to_spawn, func);
      }

      err = remote_command(COM_EXECUTE, false);
      log_error(get_thd(), true, err, "Command COM_EXECUTE");

      /* For network error master would attempt
      to restart clone. */
      if (is_master() && is_network_error(err, false)) {
        log_error(get_thd(), true, err, "Master Network issue");
        restart = true;
      }
    }

    /* Break from restart loop if not network error */
    if (restart && !is_network_error(err, false)) {
      log_error(get_thd(), true, err, "Master break restart loop");
      restart = false;
    }

    /* Disconnect auxiliary connection for master */
    if (is_master()) {
      /* Ask other end to exit clone protocol */
      auto err2 = remote_command(COM_EXIT, true);
      log_error(get_thd(), true, err2, "Master ACK COM_EXIT");

      /* If clone is interrupted, ask the remote to exit. */
      if (err2 == 0 && err == ER_QUERY_INTERRUPTED) {
        err2 = mysql_service_clone_protocol->mysql_clone_kill(m_conn_aux.m_conn,
                                                              m_conn);
        log_error(get_thd(), true, err2, "Master Interrupt");
      }

      /* if COM_EXIT is unsuccessful, abort the connection */
      auto abort_net_aux = (err2 != 0);

      mysql_service_clone_protocol->mysql_clone_disconnect(
          nullptr, m_conn_aux.m_conn, abort_net_aux, false);
      m_conn_aux.m_conn = nullptr;

      snprintf(info_mesg, 128, "Master ACK Disconnect : abort: %s",
               abort_net_aux ? "true" : "false");
      LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, info_mesg);
    }

    /* For network, protocol error and shutdown, abort network connection. */
    auto abort_net = is_network_error(err, true);

    /* Exit clone and disconnect from remote. */
    if (!abort_net) {
      auto err2 = remote_command(COM_EXIT, false);
      /* if COM_EXIT is unsuccessful, abort the connection */
      if (err2 != 0) {
        abort_net = true;
      }
      log_error(get_thd(), true, err2, "Task COM_EXIT");
    } else {
      log_error(get_thd(), true, err, "Task skip COM_EXIT");
    }

    /* If clone is successful, clear any error happened during exit. */
    bool clear_err = (err == 0);
    mysql_service_clone_protocol->mysql_clone_disconnect(get_thd(), m_conn,
                                                         abort_net, clear_err);

    snprintf(info_mesg, 128, "Task Disconnect : abort: %s",
             abort_net ? "true" : "false");
    LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, info_mesg);

    m_conn = nullptr;

    /* Set any error to storage to inform other tasks */
    if (err != 0 && m_storage_active) {
      hton_clone_apply_error(m_server_thd, m_share->m_storage_vec, m_tasks,
                             err);
    }

    /* Wait for concurrent tasks to finish. */
    wait_for_workers();

    if (restart && thd_killed(get_thd())) {
      assert(is_master());
      assert(err != 0);
      break;
    }

  } while (err != 0 && restart && restart_count < CLONE_MAX_RESTART);

  /* Check if storage is initialized and close. */
  if (m_storage_initialized) {
    hton_clone_apply_end(m_server_thd, m_share->m_storage_vec, m_tasks, err);

    m_storage_initialized = false;
    m_storage_active = false;
  }

  if (m_acquired_backup_lock) {
    assert(is_master());
    assert(get_data_dir() == nullptr);

    /* Don't release the backup lock for success case. Server would be
    restarted once the call returns. */
    if (err != 0) {
      mysql_service_mysql_backup_lock->release(get_thd());
      m_acquired_backup_lock = false;
    }
  }

  /* End PFS table state. */
  const char *err_mesg = nullptr;
  uint32_t err_number = 0;
  mysql_service_clone_protocol->mysql_clone_get_error(get_thd(), &err_number,
                                                      &err_mesg);
  pfs_end_state(err_number, err_mesg);

  return (err);
}

int Client::connect_remote(bool is_restart, bool use_aux) {
  MYSQL_SOCKET conn_socket;
  mysql_clone_ssl_context ssl_context;

  ssl_context.m_enable_compression = clone_enable_compression;
  ssl_context.m_server_extn =
      ssl_context.m_enable_compression ? &m_conn_server_extn : nullptr;
  ssl_context.m_ssl_mode = m_share->m_ssl_mode;

  /* Get Clone SSL configuration parameter value safely. */
  Key_Values ssl_configs = {
      {"clone_ssl_key", ""}, {"clone_ssl_cert", ""}, {"clone_ssl_ca", ""}};
  auto err = mysql_service_clone_protocol->mysql_clone_get_configs(get_thd(),
                                                                   ssl_configs);

  if (err != 0) {
    return err;
  }
  ssl_context.m_ssl_key = nullptr;
  ssl_context.m_ssl_cert = nullptr;
  ssl_context.m_ssl_ca = nullptr;

  if (ssl_configs[0].second.length() > 0) {
    ssl_context.m_ssl_key = ssl_configs[0].second.c_str();
  }

  if (ssl_configs[1].second.length() > 0) {
    ssl_context.m_ssl_cert = ssl_configs[1].second.c_str();
  }

  if (ssl_configs[2].second.length() > 0) {
    ssl_context.m_ssl_ca = ssl_configs[2].second.c_str();
  }

  char info_mesg[128];
  /* Establish auxiliary connection */
  if (use_aux) {
    /* Only master creates the auxiliary connection */
    if (!is_master()) {
      return 0;
    }

    /* Connect to remote server and load clone protocol. */
    m_conn_aux.m_conn = mysql_service_clone_protocol->mysql_clone_connect(
        nullptr, m_share->m_host, m_share->m_port, m_share->m_user,
        m_share->m_passwd, &ssl_context, &conn_socket);

    if (m_conn_aux.m_conn == nullptr) {
      /* Disconnect from remote and return */
      err = remote_command(COM_EXIT, false);
      log_error(get_thd(), true, err, "Master Task COM_EXIT");

      bool abort_net = (err != 0);
      mysql_service_clone_protocol->mysql_clone_disconnect(get_thd(), m_conn,
                                                           abort_net, false);
      snprintf(info_mesg, 128, "Master Task Disconnect: abort: %s",
               abort_net ? "true" : "false");
      LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, info_mesg);

      m_conn = nullptr;
      return ER_CLONE_DONOR;
    }

    return 0;
  }

  uint loop_count = 0;
  auto start_time = Clock::now();

  while (true) {
    auto connect_time = Clock::now();

    /* Connect to remote server and load clone protocol. */
    m_conn = mysql_service_clone_protocol->mysql_clone_connect(
        m_server_thd, m_share->m_host, m_share->m_port, m_share->m_user,
        m_share->m_passwd, &ssl_context, &conn_socket);

    if (m_conn != nullptr) {
      break;
    }

    if (!is_master() || !is_restart ||
        s_reconnect_timeout == Time_Sec::zero()) {
      return ER_CLONE_DONOR;
    }

    ++loop_count;
    snprintf(info_mesg, 128, "Master re-connect failed: count: %u", loop_count);
    LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, info_mesg);

    if (is_master() && thd_killed(get_thd())) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      return ER_QUERY_INTERRUPTED;
    }

    /* Check and exit if we have exceeded total reconnect time. */
    auto cur_time = Clock::now();
    auto elapsed_time = cur_time - start_time;

    if (elapsed_time > s_reconnect_timeout) {
      return ER_CLONE_DONOR;
    }

    /* Check and sleep between multiple connect attempt. */
    auto next_connect_time = connect_time + s_reconnect_interval;

    if (next_connect_time > cur_time) {
      std::this_thread::sleep_until(next_connect_time);
    }
  }

  m_ext_link.set_socket(conn_socket);
  return (0);
}

bool Client::plugin_is_loadable(std::string &so_name) {
  Key_Values configs = {{"plugin_dir", ""}};
  auto err =
      mysql_service_clone_protocol->mysql_clone_get_configs(get_thd(), configs);

  if (err != 0) {
    return false;
  }

  std::string path(configs[0].second);
  path.append("/");
  path.append(so_name);

  return clone_os_test_load(path);
}

bool Client::plugin_is_installed(std::string &plugin_name) {
  /* Attempt to lock plugin by name. */
  auto plugin = my_plugin_lock_by_name(
      get_thd(), to_lex_cstring(plugin_name.c_str()), MYSQL_ANY_PLUGIN);

  if (plugin) {
    plugin_unlock(get_thd(), plugin);
    return true;
  }
  return false;
}

int Client::validate_remote_params() {
  int last_error = 0;

  /* Validate plugins from old version CLONE_PROTOCOL_VERSION_V1.*/
  for (auto &plugin_name : m_parameters.m_plugins) {
    assert(m_share->m_protocol_version == CLONE_PROTOCOL_VERSION_V1);

    if (plugin_is_installed(plugin_name)) {
      continue;
    }
    /* Plugin is not installed. */
    my_error(ER_CLONE_PLUGIN_MATCH, MYF(0), plugin_name.c_str());
    last_error = ER_CLONE_PLUGIN_MATCH;
  }

  /* Validate plugins and check if shared objects can be loaded. */
  for (auto &plugin : m_parameters.m_plugins_with_so) {
    assert(m_share->m_protocol_version > CLONE_PROTOCOL_VERSION_V1);

    auto &plugin_name = plugin.first;
    auto &so_name = plugin.second;

    if (plugin_is_installed(plugin_name)) {
      continue;
    }

    /* Built-in plugins with no shared object should already be installed. */
    assert(!so_name.empty());

    if (so_name.empty() || plugin_is_loadable(so_name)) {
      continue;
    }

    /* Donor plugin is not there in recipient. */
    my_error(ER_CLONE_PLUGIN_MATCH, MYF(0), plugin_name.c_str());
    last_error = ER_CLONE_PLUGIN_MATCH;
  }

  /* Validate character sets */
  auto err = mysql_service_clone_protocol->mysql_clone_validate_charsets(
      get_thd(), m_parameters.m_charsets);
  if (err != 0) {
    last_error = err;
  }

  /* Validate configurations */
  err = mysql_service_clone_protocol->mysql_clone_validate_configs(
      get_thd(), m_parameters.m_configs);
  if (err != 0) {
    last_error = err;
  }
  return (last_error);
}

int Client::extract_string(const uchar *&packet, size_t &length,
                           String_Key &str) {
  /* Check length. */
  if (length >= 4) {
    auto name_length = uint4korr(packet);
    length -= 4;
    packet += 4;

    /* Check length. */
    if (length >= name_length) {
      str.clear();
      if (name_length > 0) {
        auto char_str = reinterpret_cast<const char *>(packet);
        auto str_len = static_cast<size_t>(name_length);
        str.assign(char_str, str_len);

        length -= name_length;
        packet += name_length;
      }
      return (0);
    }
  }
  /* purecov: begin deadcode */
  int err = ER_CLONE_PROTOCOL;
  my_error(err, MYF(0), "Wrong Clone RPC response length for parameters");
  return (err);
  /* purecov: end */
}

int Client::extract_key_value(const uchar *&packet, size_t &length,
                              Key_Value &keyval) {
  /* Get configuration parameter name. */
  String_Key key;
  auto err = extract_string(packet, length, key);
  if (err != 0) {
    return (err); /* purecov: inspected */
  }

  /* Get configuration parameter value */
  String_Key value;
  err = extract_string(packet, length, value);
  if (err == 0) {
    keyval = std::make_pair(key, value);
  }
  return (err);
}

int Client::add_plugin(const uchar *packet, size_t length) {
  /* Get plugin name. */
  String_Key plugin_name;
  auto err = extract_string(packet, length, plugin_name);
  if (err == 0) {
    m_parameters.m_plugins.push_back(plugin_name);
  }
  return (err);
}

int Client::add_plugin_with_so(const uchar *packet, size_t length) {
  /* Get plugin name name and shared object name. */
  Key_Value plugin;

  auto err = extract_key_value(packet, length, plugin);

  if (err == 0) {
    m_parameters.m_plugins_with_so.push_back(plugin);
  }
  return (err);
}

int Client::add_charset(const uchar *packet, size_t length) {
  /* Get character set collation name. */
  String_Key charset_name;
  auto err = extract_string(packet, length, charset_name);
  if (err == 0) {
    m_parameters.m_charsets.push_back(charset_name);
  }
  return (err);
}

void Client::use_other_configs() {
  /* Keep default as 5 minutes if remote is old version plugin and has not sent
  the configuration */
  s_reconnect_timeout = Time_Min(5);

  for (auto &key_val : m_parameters.m_other_configs) {
    auto &config_name = key_val.first;
    auto res = config_name.compare("clone_donor_timeout_after_network_failure");
    if (res == 0) {
      try {
        int timeout_minutes = std::stoi(key_val.second);
        s_reconnect_timeout = Time_Min(timeout_minutes);
      } catch (...) {
        assert(false);
      }
    }
  }
}

int Client::add_config(const uchar *packet, size_t length, bool other) {
  /* Get configuration parameter name and value. */
  Key_Value config;

  auto err = extract_key_value(packet, length, config);

  if (err != 0) {
    return err;
  }

  if (other) {
    m_parameters.m_other_configs.push_back(config);
  } else {
    m_parameters.m_configs.push_back(config);
  }
  return 0;
}

int Client::remote_command(Command_RPC com, bool use_aux) {
  size_t cmd_buff_len;

  /* Prepare command buffer */
  auto err = prepare_command_buffer(com, cmd_buff_len);

  if (err != 0) {
    return (err);
  }

  assert(cmd_buff_len <= m_cmd_buff.m_length);

  /* Use auxiliary connection for ACK */
  auto conn = use_aux ? m_conn_aux.m_conn : m_conn;

  assert(conn != nullptr);

  auto command = static_cast<uchar>(com);
  /* Send remote command */
  err = mysql_service_clone_protocol->mysql_clone_send_command(
      get_thd(), conn, !use_aux, command, m_cmd_buff.m_buffer, cmd_buff_len);
  if (err != 0) {
    return (err);
  }

  /* Receive response from remote server */
  err = receive_response(com, use_aux);

  /* Re-Check and match remote server parameters. Old server 8.0.17-19
  would send configurations later and this is must to recheck it. */
  if (com == COM_INIT && err == 0) {
    err = validate_remote_params();

    /* Validate local configurations. */
    if (err == 0) {
      err = validate_local_params(get_thd());
    }
  }
  return (err);
}

int Client::init_storage(enum Ha_clone_mode mode, size_t &cmd_len) {
  /* Get locators for negotiating with remote server */
  auto err = hton_clone_apply_begin(m_server_thd, m_share->m_data_dir,
                                    m_share->m_storage_vec, m_tasks, mode);
  if (err == 0) {
    m_storage_initialized = true;
    err = serialize_init_cmd(cmd_len);
  }
  return (err);
}

int Client::prepare_command_buffer(Command_RPC com, size_t &buf_len) {
  int err = 0;
  buf_len = 0;

  switch (com) {
    case COM_REINIT:
      assert(is_master());
      err = init_storage(HA_CLONE_MODE_RESTART, buf_len);
      break;

    case COM_INIT:
      assert(is_master());
      err = init_storage(HA_CLONE_MODE_VERSION, buf_len);
      break;

    case COM_ATTACH:
      err = serialize_init_cmd(buf_len);
      break;

    case COM_EXECUTE:
      /* No data is passed right now */
      break;

    case COM_ACK:
      err = serialize_ack_cmd(buf_len);
      break;

    case COM_EXIT:
      /* No data is passed right now */
      break;

    case COM_MAX:
      [[fallthrough]];

    default:
      assert(false);
      err = ER_CLONE_PROTOCOL;
      my_error(err, MYF(0), "Wrong Clone RPC");
  }

  return (err);
}

int Client::serialize_ack_cmd(size_t &buf_len) {
  assert(is_master());

  /* Add Error number */
  buf_len = 4;

  /* Add locator */
  auto loc = &m_share->m_storage_vec[m_conn_aux.m_cur_index];
  buf_len += loc->serlialized_length();

  /* Add descriptor */
  buf_len += 4;
  buf_len += m_conn_aux.m_buf_len;

  /* Allocate for command buffer */
  auto err = m_cmd_buff.allocate(buf_len);
  auto buf_ptr = m_cmd_buff.m_buffer;

  if (err != 0) {
    return (err);
  }

  /* Store error number */
  int4store(buf_ptr, m_conn_aux.m_error);
  buf_ptr += 4;

  /* Store Locator */
  buf_ptr += loc->serialize(buf_ptr);

  /* Store descriptor length */
  int4store(buf_ptr, m_conn_aux.m_buf_len);
  buf_ptr += 4;

  /* Store descriptor length */
  if (m_conn_aux.m_buf_len != 0) {
    memcpy(buf_ptr, m_conn_aux.m_buffer, m_conn_aux.m_buf_len);
  }

  return (0);
}

int Client::serialize_init_cmd(size_t &buf_len) {
  /* Add length of protocol Version */
  buf_len = sizeof(m_share->m_protocol_version);

  /* Add length for DDL timeout value */
  buf_len += 4;

  /* Add SE and locator length */
  for (auto &loc : m_share->m_storage_vec) {
    buf_len += loc.serlialized_length();
  }

  /* Allocate for command buffer */
  auto err = m_cmd_buff.allocate(buf_len);
  auto buf_ptr = m_cmd_buff.m_buffer;

  if (err != 0) {
    return (err);
  }

  /* Store version */
  int4store(buf_ptr, m_share->m_protocol_version);
  buf_ptr += 4;

  /* Store DDL timeout value. Default is no lock. */
  uint32_t timeout_value = clone_ddl_timeout;

  if (!clone_block_ddl) {
    timeout_value |= NO_BACKUP_LOCK_FLAG;
  }

  int4store(buf_ptr, timeout_value);
  buf_ptr += 4;

  /* Store SE information and Locators */
  for (auto &loc : m_share->m_storage_vec) {
    buf_ptr += loc.serialize(buf_ptr);
  }

  return (err);
}

int Client::receive_response(Command_RPC com, bool use_aux) {
  int err = 0;
  int saved_err = 0;
  bool last_packet = false;
  auto &info = get_thread_info();

  /* Skip setting returned locators for restart */
  bool skip_apply = (com == COM_REINIT);

  /* For graceful exit we wait for remote to send
  the end of command message */
  ulonglong err_start_time = 0;
  uint32_t timeout_sec = 0;

  /* Need to wait a little more than DDL lock timeout during INIT
  to avoid network timeout. Other than DDL lock, we currently would
  need to load the tablespaces [clone_init_tablespaces] and check
  through all tables for compression in donor[clone_init_compression]. */
  if (com == COM_INIT) {
    timeout_sec = clone_ddl_timeout + 300;
  }

  while (!last_packet) {
    uchar *packet;
    size_t length, network_length;

    auto conn = use_aux ? m_conn_aux.m_conn : m_conn;

    /* Set current socket as active for clone data connection. */
    err = mysql_service_clone_protocol->mysql_clone_get_response(
        get_thd(), conn, !use_aux, timeout_sec, &packet, &length,
        &network_length);

    if (err != 0) {
      saved_err = err;
      break;
    }
    /* Data length is not updated for meta information. */
    info.update(0, network_length);

    err = handle_response(packet, length, saved_err, skip_apply, last_packet);

    if (handle_error(err, saved_err, err_start_time)) {
      break;
    }
  }
  return (saved_err);
}

bool Client::handle_error(int current_err, int &first_err,
                          ulonglong &first_err_time) {
  /* If no error, need to continue */
  if (current_err == 0 && first_err == 0) {
    return (false);
  }

  /* If error repeats then exit */
  if (current_err != 0 && first_err != 0) {
    return (true);
  }

  if (current_err != 0) {
    assert(first_err == 0);
    first_err = current_err;
    first_err_time = my_micro_time() / 1000;

    /* Set any error to storage to inform other tasks */
    if (m_storage_active) {
      hton_clone_apply_error(m_server_thd, m_share->m_storage_vec, m_tasks,
                             current_err);
    }

    /* If network error, no need to wait for remote */
    if (is_network_error(current_err, true)) {
      return (true);
    }
    log_error(get_thd(), true, current_err,
              "Wait for remote after local issue");
    return (false);
  }

  assert(first_err != 0);

  auto cur_time = my_micro_time() / 1000;

  assert(cur_time >= first_err_time);
  assert(current_err == 0);

  /* If wait for remote is long [30 sec] exit */
  if (cur_time - first_err_time > 30 * 1000) {
    log_error(get_thd(), true, first_err, /* purecov: inspected */
              "No error from remote in 30 sec after local issue");
    /* Exit with protocol error */
    first_err = ER_NET_PACKETS_OUT_OF_ORDER;
    my_error(first_err, MYF(0));

    return (true);
  }

  /* Need to continue till remote reports error or we hit timeout. */
  return (false);
}

int Client::handle_response(const uchar *packet, size_t length, int in_err,
                            bool skip_loc, bool &is_last) {
  int err = 0;

  /* Read response command */
  auto res_com = static_cast<Command_Response>(packet[0]);

  packet++;
  length--;

  is_last = false;

  switch (res_com) {
    case COM_RES_PLUGIN:
      err = add_plugin(packet, length);
      break;

    case COM_RES_PLUGIN_V2:
      err = add_plugin_with_so(packet, length);
      break;

    case COM_RES_CONFIG:
      err = add_config(packet, length, false);
      break;

    case COM_RES_CONFIG_V3:
      err = add_config(packet, length, true);
      break;

    case COM_RES_COLLATION:
      err = add_charset(packet, length);
      break;

    case COM_RES_LOCS:
      /* Skip applying locator for restart */
      if (!skip_loc && in_err == 0) {
        err = set_locators(packet, length);
      }
      break;

    case COM_RES_DATA_DESC:
      /* Skip processing data in case of an error till last */
      if (in_err == 0) {
        err = set_descriptor(packet, length);
      }
      break;

    case COM_RES_COMPLETE:
      is_last = true;
      break;

    case COM_RES_ERROR:
      err = set_error(packet, length);
      is_last = true;
      break;

    case COM_RES_DATA:
      /* Allow data packet to skip */
      if (in_err != 0) {
        break;
      }

      /* COM_RES_DATA must follow COM_RES_DATA_DESC and is handled
      in apply_file_cbk(). Fall through to return error. */
      [[fallthrough]];
    default:
      assert(false);
      err = ER_CLONE_PROTOCOL;
      my_error(err, MYF(0), "Wrong Clone RPC response");
  }

  return (err);
}

int Client::set_locators(const uchar *buffer, size_t length) {
  bool init_failed = false;
  int err = 0;

  if (length < 4) {
    /* purecov: begin deadcode */
    err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC response length for COM_RES_LOCS");
    return (err);
    /* purecov: end */
  }

  m_share->m_protocol_version = uint4korr(buffer);
  buffer += 4;
  length -= 4;

  assert(m_share->m_protocol_version <= CLONE_PROTOCOL_VERSION);

  Storage_Vector local_locators;

  /* Initialize locators */
  for (auto &st_loc : m_share->m_storage_vec) {
    Locator loc = st_loc;

    auto serialized_length = loc.deserialize(get_thd(), buffer);

    buffer += serialized_length;

    if (length < serialized_length || loc.m_loc_len == 0) {
      init_failed = true;
      break;
    }

    length -= serialized_length;
    local_locators.push_back(loc);
  }

  if (length != 0 || init_failed) {
    /* purecov: begin deadcode */
    err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC response length for COM_RES_LOCS");
    return (err);
    /* purecov: end */
  }

  auto begin_mode = is_master() ? HA_CLONE_MODE_START : HA_CLONE_MODE_ADD_TASK;

  /* Close the version locators */
  if (is_master()) {
    assert(m_storage_initialized);
    assert(!m_storage_active);

    hton_clone_apply_end(m_server_thd, m_share->m_storage_vec, m_tasks, 0);
    m_storage_initialized = false;

    /* Check and match remote server parameters. */
    err = validate_remote_params();
    if (err != 0) {
      return (err);
    }

    /* Validate local configurations. */
    err = validate_local_params(get_thd());
    if (err != 0) {
      return (err);
    }

    /* Check and use additional configurations from donor. */
    use_other_configs();

    /* If cloning to current data directory, prevent any DDL. */
    if (get_data_dir() == nullptr) {
      auto failed = mysql_service_mysql_backup_lock->acquire(
          get_thd(), BACKUP_LOCK_SERVICE_DEFAULT, clone_ddl_timeout);

      if (failed) {
        return (ER_LOCK_WAIT_TIMEOUT);
      }
      m_acquired_backup_lock = true;
    }
  }

  /* Move to first stage only after validations are over. */
  pfs_change_stage(0);

  /* Re-initialize SE locators based on remote locators */
  err = hton_clone_apply_begin(m_server_thd, m_share->m_data_dir,
                               local_locators, m_tasks, begin_mode);
  if (err != 0) {
    return (err);
  }

  /* Master should set locators */
  if (is_master()) {
    int index = 0;

    for (auto &st_loc : m_share->m_storage_vec) {
      st_loc = local_locators[index++];
    }
  }

  m_storage_initialized = true;
  m_storage_active = true;

  return (err);
}

int Client::set_descriptor(const uchar *buffer, size_t length) {
  int err = 0;

  /* Get Storage Engine */
  auto db_type = static_cast<enum legacy_db_type>(*buffer);
  ++buffer;
  length--;

  /* Get Locator Index */
  auto loc_index = *buffer;
  ++buffer;
  length--;

  auto loc = &m_share->m_storage_vec[loc_index];
  auto hton = loc->m_hton;

  if (hton->db_type != db_type) {
    err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Remote descriptor handlerton type mismatch");
    return (err);
  }

  Ha_clone_cbk *clone_callback = new Client_Cbk(this);

  clone_callback->set_data_desc(buffer, length);
  clone_callback->clear_flags();

  /* Apply using descriptor */
  assert(loc_index < m_tasks.size());
  err = hton->clone_interface.clone_apply(loc->m_hton, get_thd(), loc->m_loc,
                                          loc->m_loc_len, m_tasks[loc_index], 0,
                                          clone_callback);

  delete clone_callback;

  if (!is_master() || err == 0 || err == ER_CLONE_DONOR) {
    return (err);
  }

  /* Inform the source database about any local error using the
  auxiliary connection. Only master client task should use it. */
  assert(is_master());

  auto aux_conn = get_aux();

  aux_conn->reset();
  aux_conn->m_error = err;
  aux_conn->m_cur_index = loc_index;

  remote_command(COM_ACK, true);

  /* Reset buffers */
  aux_conn->reset();

  return (err);
}

int Client::set_error(const uchar *buffer, size_t length) {
  auto remote_err = sint4korr(buffer);

  buffer += 4;
  length -= 4;

  int err = ER_CLONE_DONOR;

  if (is_master()) {
    char err_buf[MYSYS_ERRMSG_SIZE];

    snprintf(err_buf, MYSYS_ERRMSG_SIZE, "%d : %.*s", remote_err,
             static_cast<int>(length), buffer);

    my_error(err, MYF(0), err_buf);
  }

  return (err);
}

int Client::wait(Time_Sec wait_time) {
  int ret_error = 0;
  auto start_time = Clock::now();
  auto print_time = start_time;
  auto sec = wait_time;
  auto min = std::chrono::duration_cast<Time_Min>(wait_time);
  std::ostringstream log_strm;

  LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE,
               "Begin Delay after data drop");

  sec -= std::chrono::duration_cast<Time_Sec>(min);
  log_strm << "Wait time remaining is " << min.count() << " minutes and "
           << sec.count() << " seconds.";
  std::string log_str(log_strm.str());
  LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, log_str.c_str());
  log_strm.str("");

  for (;;) {
    Time_Msec sleep_time(100);
    std::this_thread::sleep_for(sleep_time);
    auto cur_time = Clock::now();

    auto duration_sec =
        std::chrono::duration_cast<Time_Sec>(cur_time - start_time);

    /* Check for total time elapsed. */
    if (duration_sec >= wait_time) {
      break;
    }

    auto duration_print =
        std::chrono::duration_cast<Time_Min>(cur_time - print_time);

    if (duration_print.count() >= 1) {
      print_time = Clock::now();
      auto remaining_time = wait_time - duration_sec;
      min = std::chrono::duration_cast<Time_Min>(remaining_time);
      log_strm << "Wait time remaining is " << min.count() << " minutes.";
      std::string log_str(log_strm.str());
      LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, log_str.c_str());
      log_strm.str("");
    }

    /* Check for interrupt */
    if (thd_killed(get_thd())) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      ret_error = ER_QUERY_INTERRUPTED;
      break;
    }
  }

  LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE,
               "End Delay after data drop");
  return ret_error;
}

int Client::delay_if_needed() {
  /* Delay only if replacing current data directory. */
  if (get_data_dir() != nullptr) {
    return 0;
  }

  if (clone_delay_after_data_drop == 0) {
    return 0;
  }

  auto err = wait(Time_Sec(clone_delay_after_data_drop));

  return err;
}

int Client_Cbk::file_cbk(Ha_clone_file from_file [[maybe_unused]],
                         uint len [[maybe_unused]]) {
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Remote Clone Client");
  return (ER_NOT_SUPPORTED_YET);
}

int Client_Cbk::buffer_cbk(uchar *from_buffer [[maybe_unused]], uint buf_len) {
  auto client = get_clone_client();

  uint64_t data_estimate = 0;
  if (is_state_change(data_estimate)) {
    client->pfs_change_stage(data_estimate);
    return (0);
  }

  /* Reset statistics information when state is finished */
  client->update_stat(true);

  assert(client->is_master());

  if (thd_killed(client->get_thd())) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    return (ER_QUERY_INTERRUPTED);
  }

  auto aux_conn = client->get_aux();

  aux_conn->reset();
  aux_conn->m_buffer = get_data_desc(&buf_len);

  aux_conn->m_buf_len = static_cast<size_t>(buf_len);
  aux_conn->m_cur_index = get_loc_index();

  /* Send ACK back to remote */
  auto err = client->remote_command(COM_ACK, true);

  /* Reset buffers */
  aux_conn->reset();

  return (err);
}

int Client_Cbk::apply_buffer_cbk(uchar *&to_buffer, uint &len) {
  Ha_clone_file dummy_file;
  dummy_file.type = Ha_clone_file::FILE_HANDLE;
  dummy_file.file_handle = nullptr;
  return (apply_cbk(dummy_file, false, to_buffer, len));
}

int Client_Cbk::apply_file_cbk(Ha_clone_file to_file) {
  uchar *bufp = nullptr;
  uint buf_len = 0;
  return (apply_cbk(to_file, true, bufp, buf_len));
}

int Client_Cbk::apply_cbk(Ha_clone_file to_file, bool apply_file,
                          uchar *&to_buffer, uint &to_len) {
  auto client = get_clone_client();
  auto &info = client->get_thread_info();

  MYSQL *conn;
  client->get_data_link(conn);

  /* Update statistics information. */
  auto num_workers = client->update_stat(false);

  /* Spawn more concurrent client tasks if suggested. */
  using namespace std::placeholders;
  auto func = std::bind(clone_client, _1, _2);
  client->spawn_workers(num_workers, func);

  uchar *packet;
  size_t length, network_length;

  /* Get clone data response command */
  auto err = mysql_service_clone_protocol->mysql_clone_get_response(
      client->get_thd(), conn, true, 0, &packet, &length, &network_length);

  if (err != 0) {
    return (err);
  }

  auto res_com = static_cast<Command_Response>(packet[0]);

  /* Read response command */
  if (res_com != COM_RES_DATA) {
    assert(false);
    err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0),
             "Wrong Clone RPC response, "
             "expecting data packet COM_RES_DATA");

    return (err);
  }

  packet++;
  length--;

  auto buf_ptr = packet;

  if (!is_os_buffer_cache()) {
    /* Allocate aligned buffer */
    buf_ptr = client->get_aligned_buffer(length);

    if (buf_ptr == nullptr) {
      err = ER_OUTOFMEMORY;
      return (err);
    }

    memcpy(buf_ptr, packet, length);
  }

  if (apply_file) {
    err = clone_os_copy_buf_to_file(buf_ptr, to_file, length, get_dest_name());
  } else {
    err = 0;
    to_buffer = buf_ptr;
    to_len = static_cast<uint>(length);
  }

  if (err == 0 && client->is_master() && thd_killed(client->get_thd())) {
    err = ER_QUERY_INTERRUPTED;
    my_error(err, MYF(0));
  }

  if (err == 0) {
    /* Update data transfer information. */
    info.update(length, network_length);

    /* Check limits and throttle if needed. */
    client->check_and_throttle();
  }
  return (err);
}

}  // namespace myclone
