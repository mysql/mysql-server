/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>  // setprecision std::setfill std::setw
#include <sstream>
#include <thread>

#include <my_systime.h>
#include <my_thread.h>

#include "resource_manager.h"
#include "resource_manager_channel_lag.h"
#include "resource_manager_stats_collector.h"

#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_management_service.h>
#include <mysql/components/services/group_replication_status_service.h>

extern REQUIRES_SERVICE_PLACEHOLDER_AS(registry, mysql_srv_reg);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(psi_thread_v5, thread_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_cond_v1, cond_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_mutex_v1, mutex_srv);

extern gr_resource_manager::Lag_metadata_thresholds lag_metadata_threshold;

static constexpr uint64_t hundredth_ns{10000000};

#define GR_STOP_SERVICE_STR "group_replication.group_replication_management"
#define GR_STOP_SERVICE group_replication.group_replication_management

namespace gr_resource_manager {
int threshhold_hit{0};
Resource_manager_stats_collector::Resource_manager_stats_collector()
    : m_name("THD_GR_Resource_manager"),
      m_instr_name("GR_RESOURCE_MANAGER"),
      m_os_name("THD_GR_RM"),
      m_description("Resource_manager_collector") {
  thread_srv->register_thread("gr_rm", &m_thread_info, 1);

  mutex_srv->register_info("gr_rm", &m_command_info, 1);
  mutex_srv->init(m_command_key, &m_command_lock, nullptr, __FILE__, __LINE__);

  cond_srv->register_info("gr_rm", &m_cond_info, 1);
  cond_srv->init(m_cond_key, &m_wait, __FILE__, __LINE__);
}

Resource_manager_stats_collector::~Resource_manager_stats_collector() {
  stop_thread();

  cond_srv->destroy(&m_wait, __FILE__, __LINE__);
  mutex_srv->destroy(&m_command_lock, __FILE__, __LINE__);
}

bool Resource_manager_stats_collector::is_time_diff_greater_than(
    uint64_t timepoint, uint64_t now, uint64_t diff) {
  return (now - timepoint) > diff;
}

uint64_t Resource_manager_stats_collector::get_time_now_seconds() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(
             now.time_since_epoch())
      .count();
}

uint64_t Resource_manager_stats_collector::get_time_now_microseconds() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(
             now.time_since_epoch())
      .count();
}

std::string
Resource_manager_stats_collector::convert_microseconds_to_timestamp_string(
    uint64_t microseconds_since_epoch) {
  std::chrono::duration<uint64_t, std::micro> const duration(
      microseconds_since_epoch);
  std::chrono::system_clock::time_point const tp(duration);
  std::time_t const tt = std::chrono::system_clock::to_time_t(tp);
  std::tm *tm = std::localtime(&tt);
  uint64_t const microseconds = microseconds_since_epoch % 1000000;

  std::stringstream ss;
  ss << std::setfill('0');
  ss << std::setw(4) << tm->tm_year + 1900 << "-";
  ss << std::setw(2) << tm->tm_mon + 1 << "-";
  ss << std::setw(2) << tm->tm_mday << " ";
  ss << std::setw(2) << (tm->tm_hour % 24) << ":";
  ss << std::setw(2) << tm->tm_min << ":";
  ss << std::setw(2) << tm->tm_sec << ".";
  ss << std::setw(6) << microseconds;
  return ss.str();
}

std::string Resource_manager_stats_collector::get_applier_eviction_timestamp() {
  const uint64_t t = m_applier_eviction_timestamp;
  return t ? convert_microseconds_to_timestamp_string(t) : "";
}

std::string
Resource_manager_stats_collector::get_recovery_eviction_timestamp() {
  const uint64_t t = m_recovery_eviction_timestamp;
  return t ? convert_microseconds_to_timestamp_string(t) : "";
}

std::string Resource_manager_stats_collector::get_memory_eviction_timestamp() {
  const uint64_t t = m_memory_eviction_timestamp;
  return t ? convert_microseconds_to_timestamp_string(t) : "";
}

std::string
Resource_manager_stats_collector::get_lag_query_last_error_timestamp() {
  const uint64_t t = m_channel_lag_monitoring_error_timestamp;
  return t ? convert_microseconds_to_timestamp_string(t) : "";
}

std::string
Resource_manager_stats_collector::get_memory_last_error_timestamp() {
  const uint64_t t = m_memory_monitoring_error_timestamp;
  return t ? convert_microseconds_to_timestamp_string(t) : "";
}

bool Resource_manager_stats_collector::is_group_replication_running() const {
  const my_service<SERVICE_TYPE(group_replication_management_service_v1)>
      gr_management_service("group_replication.group_replication_management",
                            mysql_srv_reg);
  if (gr_management_service.is_valid()) {
    return gr_management_service->is_member_online_or_recovering();
  }
  return false;
}

bool Resource_manager_stats_collector::
    is_group_in_single_primary_mode_and_im_a_secondary() const {
  // The group replication status service may or may not be available
  const my_service<SERVICE_TYPE(group_replication_status_service_v1)>
      gr_status_srv("group_replication_status_service_v1", mysql_srv_reg);
  if (gr_status_srv.is_valid()) {
    return gr_status_srv->is_group_in_single_primary_mode_and_im_a_secondary();
  }
  return false;
}

eject_status Resource_manager_stats_collector::leave_group() {
  eject_status ret = GR_RM_NOT_A_MEMBER;
  const my_service<SERVICE_TYPE(group_replication_management_service_v1)>
      gr_management_service("group_replication.group_replication_management",
                            mysql_srv_reg);

  if (gr_management_service.is_valid()) {
    ret = gr_management_service->eject(lag_metadata_threshold.quarantine_time,
                                       &m_seconds_since_member_join);
  } else {
    LogComponentErr(WARNING_LEVEL, ER_GR_RM_GR_MGMT_SERVICE_ACQUIRE_FAILED);
  }
  return ret;
}

uint Resource_manager_stats_collector::get_applier_lag() {
  return m_applier_lag_last_fetched;
}
uint Resource_manager_stats_collector::get_recovery_lag() {
  return m_recovery_lag_last_fetched;
}
uint Resource_manager_stats_collector::get_percentage_used_memory() {
  return m_percentage_used_memory_last_fetched;
}

int Resource_manager_stats_collector::fetch_memory_used() {
  int const ret = gr_resource_manager::get_memory_info(m_memory_info);
  if (ret == 0) {
    m_percentage_used_memory_last_fetched =
        100 -
        static_cast<uint>(std::round(((float)m_memory_info.sys.m_avail_bytes /
                                      (float)m_memory_info.sys.m_total_bytes) *
                                     100.0));
    if (0 != m_memory_monitoring_error_timestamp) {
      m_memory_monitoring_error_timestamp = 0;
      LogComponentErr(WARNING_LEVEL, ER_GR_RM_MEMORY_STATS_FETCH_SUCCESS);
    }
  } else {
    if (0 == m_memory_monitoring_error_timestamp) {
      LogComponentErr(WARNING_LEVEL, ER_GR_RM_MEMORY_STATS_FETCH_FAILED);
    }
    m_memory_monitoring_error_timestamp = get_time_now_microseconds();
  }
  return ret;
}

int Resource_manager_stats_collector::fetch_channel_lag() {
  std::vector<lag_record> lags;
  int const ret = GR_resource_manager::fetch_channel_lag(lags);
  if (ret) {
    if (0 == m_channel_lag_monitoring_error_timestamp) {
      LogComponentErr(WARNING_LEVEL,
                      ER_GR_RM_CHANNEL_LAG_QUERY_EXECUTION_FAILED);
    }
    m_channel_lag_monitoring_error_timestamp = get_time_now_microseconds();
    return 1;
  }
  if (0 != m_channel_lag_monitoring_error_timestamp) {
    m_channel_lag_monitoring_error_timestamp = 0;
    LogComponentErr(WARNING_LEVEL,
                    ER_GR_RM_CHANNEL_LAG_QUERY_EXECUTION_SUCCESS);
  }

  /*
    Non-existent channel must be represented as no lag.
  */
  uint applier_lag{0};
  uint recovery_lag{0};
  for (const auto &lag : lags) {
    if (!lag.channel_name.compare("group_replication_applier")) {
      applier_lag = lag.lag_in_seconds;
    } else if (!lag.channel_name.compare("group_replication_recovery")) {
      recovery_lag = lag.lag_in_seconds;
    }
  }
  m_applier_lag_last_fetched = applier_lag;
  m_recovery_lag_last_fetched = recovery_lag;

  return 0;
}

void Resource_manager_stats_collector::process() {
  const bool group_replication_running = is_group_replication_running();

  if (fetch_memory_used()) {
    m_percentage_used_memory_last_fetched = 0;
  }

  if (!group_replication_running || fetch_channel_lag()) {
    m_applier_lag_last_fetched = m_recovery_lag_last_fetched = 0;
  }

  /*
    Only compute threshold hits if the server is a group member.
  */
  if (!group_replication_running) {
    return;
  }

  if (lag_metadata_threshold.used_memory_limit &&
      m_percentage_used_memory_last_fetched >
          lag_metadata_threshold.used_memory_limit) {
    m_memory_hit_number_of_times++;
    m_memory_continuous_excess_usage++;
  } else {
    m_memory_continuous_excess_usage = 0;
  }

  if (lag_metadata_threshold.applier_lag_limit_in_seconds &&
      m_applier_lag_last_fetched >
          lag_metadata_threshold.applier_lag_limit_in_seconds) {
    m_applier_hit_number_of_times++;
    m_applier_continuous_lag++;
  } else {
    m_applier_continuous_lag = 0;
  }

  if (lag_metadata_threshold.recovery_lag_limit_in_seconds &&
      m_recovery_lag_last_fetched >
          lag_metadata_threshold.recovery_lag_limit_in_seconds) {
    m_recovery_hit_number_of_times++;
    m_recovery_continuous_lag++;
  } else {
    m_recovery_continuous_lag = 0;
  }

  /*
    Only proceed to possible eviction on secondary members.
  */
  if (!is_group_in_single_primary_mode_and_im_a_secondary()) {
    return;
  }

  if (m_applier_continuous_lag > s_max_continuous_lag_counter) {
    eject_status const status = leave_group();
    if (status != GR_RM_SUCCESS_LEFT_GROUP) {
      const uint64_t now_ts_s = get_time_now_seconds();
      if (status == GR_RM_QUARANTINE_PERIOD_NOT_OVER) {
        assert(m_seconds_since_member_join <=
               lag_metadata_threshold.quarantine_time.load());
        // Limit the number of log messages to 1 per 5 minutes.
        if (is_time_diff_greater_than(
                m_not_removed_applier_threshold_hit_quarantime_ts_s,
                now_ts_s)) {
          m_not_removed_applier_threshold_hit_quarantime_ts_s = now_ts_s;
          LogComponentErr(
              WARNING_LEVEL,
              ER_GR_RM_GR_MEMBER_NOT_REMOVED_APPLIER_THRESHOLD_HIT_QUARANTINE,
              m_applier_lag_last_fetched.load(),
              lag_metadata_threshold.applier_lag_limit_in_seconds.load(),
              m_seconds_since_member_join,
              lag_metadata_threshold.quarantine_time.load());
        }
      } else if (status == GR_RM_NUMBER_OF_MEMBERS_LESS_THAN_THREE) {
        // Limit the number of log messages to 1 per 5 minutes.
        if (is_time_diff_greater_than(
                m_not_removed_applier_threshold_hit_n_members_ts_s, now_ts_s)) {
          m_not_removed_applier_threshold_hit_n_members_ts_s = now_ts_s;
          LogComponentErr(
              WARNING_LEVEL,
              ER_GR_RM_GR_MEMBER_NOT_REMOVED_APPLIER_THRESHOLD_HIT_N_MEMBERS,
              m_applier_lag_last_fetched.load(),
              lag_metadata_threshold.applier_lag_limit_in_seconds.load());
        }
      }
    } else {
      m_applier_eviction_timestamp = get_time_now_microseconds();
      m_applier_continuous_lag = 0;
      LogComponentErr(
          ERROR_LEVEL, ER_GR_RM_MEMBER_LEAVING_APPLIER_THRESHOLD_HIT,
          m_applier_lag_last_fetched.load(),
          lag_metadata_threshold.applier_lag_limit_in_seconds.load());
    }
  } else if (m_recovery_continuous_lag > s_max_continuous_lag_counter) {
    eject_status const status = leave_group();
    if (status != GR_RM_SUCCESS_LEFT_GROUP) {
      const uint64_t now_ts_s = get_time_now_seconds();
      if (status == GR_RM_QUARANTINE_PERIOD_NOT_OVER) {
        assert(m_seconds_since_member_join <=
               lag_metadata_threshold.quarantine_time.load());
        // Limit the number of log messages to 1 per 5 minutes.
        if (is_time_diff_greater_than(
                m_not_removed_recovery_threshold_hit_quarantime_ts_s,
                now_ts_s)) {
          m_not_removed_recovery_threshold_hit_quarantime_ts_s = now_ts_s;
          LogComponentErr(
              WARNING_LEVEL,
              ER_GR_RM_GR_MEMBER_NOT_REMOVED_RECOVERY_THRESHOLD_HIT_QUARANTINE,
              m_recovery_lag_last_fetched.load(),
              lag_metadata_threshold.recovery_lag_limit_in_seconds.load(),
              m_seconds_since_member_join,
              lag_metadata_threshold.quarantine_time.load());
        }
      } else if (status == GR_RM_NUMBER_OF_MEMBERS_LESS_THAN_THREE) {
        // Limit the number of log messages to 1 per 5 minutes.
        if (is_time_diff_greater_than(
                m_not_removed_recovery_threshold_hit_n_members_ts_s,
                now_ts_s)) {
          m_not_removed_recovery_threshold_hit_n_members_ts_s = now_ts_s;
          LogComponentErr(
              WARNING_LEVEL,
              ER_GR_RM_GR_MEMBER_NOT_REMOVED_RECOVERY_THRESHOLD_HIT_N_MEMBERS,
              m_recovery_lag_last_fetched.load(),
              lag_metadata_threshold.recovery_lag_limit_in_seconds.load());
        }
      }
    } else {
      m_recovery_eviction_timestamp = get_time_now_microseconds();
      m_recovery_continuous_lag = 0;
      LogComponentErr(
          ERROR_LEVEL, ER_GR_RM_MEMBER_LEAVING_RECOVERY_THRESHOLD_HIT,
          m_recovery_lag_last_fetched.load(),
          lag_metadata_threshold.recovery_lag_limit_in_seconds.load());
    }
  } else if (m_memory_continuous_excess_usage > s_max_continuous_lag_counter) {
    eject_status const status = leave_group();
    if (status != GR_RM_SUCCESS_LEFT_GROUP) {
      const uint64_t now_ts_s = get_time_now_seconds();
      if (status == GR_RM_QUARANTINE_PERIOD_NOT_OVER) {
        assert(m_seconds_since_member_join <=
               lag_metadata_threshold.quarantine_time.load());
        // Limit the number of log messages to 1 per 5 minutes.
        if (is_time_diff_greater_than(
                m_not_removed_memory_threshold_hit_quarantime_ts_s, now_ts_s)) {
          m_not_removed_memory_threshold_hit_quarantime_ts_s = now_ts_s;
          LogComponentErr(
              WARNING_LEVEL,
              ER_GR_RM_GR_MEMBER_NOT_REMOVED_MEMORY_THRESHOLD_HIT_QUARANTINE,
              m_percentage_used_memory_last_fetched.load(),
              lag_metadata_threshold.used_memory_limit.load(),
              m_seconds_since_member_join,
              lag_metadata_threshold.quarantine_time.load());
        }
      } else if (status == GR_RM_NUMBER_OF_MEMBERS_LESS_THAN_THREE) {
        // Limit the number of log messages to 1 per 5 minutes.
        if (is_time_diff_greater_than(
                m_not_removed_memory_threshold_hit_n_members_ts_s, now_ts_s)) {
          m_not_removed_memory_threshold_hit_n_members_ts_s = now_ts_s;
          LogComponentErr(
              WARNING_LEVEL,
              ER_GR_RM_GR_MEMBER_NOT_REMOVED_MEMORY_THRESHOLD_HIT_N_MEMBERS,
              m_percentage_used_memory_last_fetched.load(),
              lag_metadata_threshold.used_memory_limit.load());
        }
      }
    } else {
      m_memory_eviction_timestamp = get_time_now_microseconds();
      m_memory_continuous_excess_usage = 0;
      LogComponentErr(ERROR_LEVEL, ER_GR_RM_MEMBER_LEAVING_MEMORY_THRESHOLD_HIT,
                      m_percentage_used_memory_last_fetched.load(),
                      lag_metadata_threshold.used_memory_limit.load());
    }
  }
}

int Resource_manager_stats_collector::join(my_thread_handle *thread,
                                           void **value_ptr [[maybe_unused]]) {
#ifndef _WIN32
  return pthread_join(thread->thread, value_ptr);
#else
  int result{0};
  DWORD ret = WaitForSingleObject(thread->handle, INFINITE);
  if (ret != WAIT_OBJECT_0) {
    result = 1;
  }
  if (thread->handle) CloseHandle(thread->handle);
  thread->thread = 0;
  thread->handle = nullptr;
  return result;
#endif
}

bool Resource_manager_stats_collector::joinable(
    const my_thread_handle &thread) {
#ifndef _WIN32
  return thread.thread != 0;
#else
  return thread.handle != INVALID_HANDLE_VALUE;
#endif
}

void Resource_manager_stats_collector::run_process() {
  bool timed_out{true};
  timespec wait{0, 0};

  // Protect from UNINSTALL component
  mutex_srv->lock(&m_command_lock, __FILE__, __LINE__);
  auto collect_stats{m_thread_state};
  mutex_srv->unlock(&m_command_lock, __FILE__, __LINE__);

  while (collect_stats == RUNNING) {
    if (timed_out) {
      process();
    }
    mutex_srv->lock(&m_command_lock, __FILE__, __LINE__);
    collect_stats = m_thread_state;
    if (m_thread_state == RUNNING) {
      // Wait for 5 seconds or signal received to stop the thread
      const uint64_t sample_rate{s_sample_rate * hundredth_ns};
      wait.tv_sec = (my_getsystime() + sample_rate) / hundredth_ns;
      const auto result = cond_srv->timedwait(&m_wait, &m_command_lock, &wait,
                                              __FILE__, __LINE__);
      timed_out = (is_timeout(result) != 0);
      collect_stats = m_thread_state;
    }
    mutex_srv->unlock(&m_command_lock, __FILE__, __LINE__);
  }
  thread_srv->delete_current_thread();
}

/**
  Static routine for thread instrumentation interface.
*/
static void *start_routine(void *arg) {
  auto *thread = reinterpret_cast<Resource_manager_stats_collector *>(arg);
  if (thread != nullptr) {
    thread->run_process();
  }
  return nullptr;
}

int Resource_manager_stats_collector::start_thread() {
  int error = 0;
  mutex_srv->lock(&m_command_lock, __FILE__, __LINE__);
  if (m_thread_state == IDLE) {
    error = thread_srv->spawn_thread(m_thread_key, 0, &m_thread, nullptr,
                                     start_routine, this);
    if (!error) m_thread_state = RUNNING;
  }
  mutex_srv->unlock(&m_command_lock, __FILE__, __LINE__);
  return error;
}

void Resource_manager_stats_collector::stop_thread() {
  mutex_srv->lock(&m_command_lock, __FILE__, __LINE__);
  if (m_thread_state == RUNNING) {
    m_thread_state = STOPPING;
    cond_srv->signal(&m_wait, __FILE__, __LINE__);
    mutex_srv->unlock(&m_command_lock, __FILE__, __LINE__);
    if (joinable(m_thread)) {
      join(&m_thread, nullptr);
    }
    mutex_srv->lock(&m_command_lock, __FILE__, __LINE__);
    m_thread_state = IDLE;
  }
  mutex_srv->unlock(&m_command_lock, __FILE__, __LINE__);
}

}  // namespace gr_resource_manager