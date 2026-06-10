/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#ifndef RESOURCE_MANAGER_STATS_COLLECTOR_H
#define RESOURCE_MANAGER_STATS_COLLECTOR_H

#include <atomic>
#include <memory>
#include <thread>

#include <my_inttypes.h>
#include <mysql/components/services/group_replication_management_service.h>
#include <mysql/components/services/mysql_cond_service.h>
#include <mysql/components/services/mysql_mutex_service.h>
#include <mysql/components/services/mysql_rwlock.h>
#include <mysql/components/services/mysql_system_variable.h>
#include <mysql/components/services/psi_thread_service.h>

#include "resource_manager_memory_stats.h"

namespace gr_resource_manager {
/**
 @class Lag_metadata_thresholds

 This class stores the lag thesholds controlled by system variables.
*/
class Lag_metadata_thresholds {
 public:
  /// Applier channel threshold.
  std::atomic<uint> applier_lag_limit_in_seconds{3600};
  /// Recover channel threshold.
  std::atomic<uint> recovery_lag_limit_in_seconds{3600};
  /// Used memory threshold.
  std::atomic<uint> used_memory_limit{100};
  /// Quarantine time.
  std::atomic<uint> quarantine_time{3600};
};

class Resource_manager_stats_collector {
 private:
  gr_resource_manager::Memory_Info m_memory_info;

 public:
  Resource_manager_stats_collector();
  ~Resource_manager_stats_collector();
  Resource_manager_stats_collector(const Resource_manager_stats_collector &) =
      delete;
  Resource_manager_stats_collector &operator=(
      const Resource_manager_stats_collector &) = delete;
  Resource_manager_stats_collector(Resource_manager_stats_collector &&) =
      delete;
  Resource_manager_stats_collector &operator=(
      Resource_manager_stats_collector &&) = delete;

 public:
  // Functions for channel and memory
  /**
    Return applier lag.

    @retval applier lag in seconds
  */
  uint get_applier_lag();
  /**
    Return recovery lag.

    @retval recovery lag in seconds
  */
  uint get_recovery_lag();
  /**
    Return used memory.

    @retval percentage of used memory
  */
  uint get_percentage_used_memory();

  /**
    Returns number of times applier lag was seen.

    @retval counter of applier lag seen
  */
  uint get_applier_hit_number_of_times() {
    return m_applier_hit_number_of_times;
  }
  /**
    Returns number of times recovery lag was seen.

    @retval counter of recovery lag seen
  */
  uint get_recovery_hit_number_of_times() {
    return m_recovery_hit_number_of_times;
  }
  /**
    Returns number of times low memory was observed.

    @retval counter of low memory seen
  */
  uint get_memory_hit_number_of_times() { return m_memory_hit_number_of_times; }

  /**
    Check if the time difference between `timepoint` and `now` is
    greater than `diff`.
    Parameters must be of the same unit: seconds, microseconds...

    @param[in] timepoint  timepoint in time
    @param[in] now        a later timepoint in time
    @param[in] diff       the difference to compare

    @returns true   time difference between `timepoint` and `now` is
                    greater than `diff`
             false  otherwise
  */
  static bool is_time_diff_greater_than(uint64_t timepoint, uint64_t now,
                                        uint64_t diff = 300);

  /**
    Returns the current time represented by seconds
    elapsed since the Epoch.

    @returns the current time represented by seconds
    elapsed since the Epoch.
  */
  static uint64_t get_time_now_seconds();

  /**
    Returns the current time represented by micro-seconds
    elapsed since the Epoch.

    @returns the current time represented by micro-seconds
    elapsed since the Epoch.
  */
  static uint64_t get_time_now_microseconds();

  /**
    Converts a time point represented by micro-seconds
    elapsed since the Epoch to the string format
    'YYYY-MM-DD hh:mm:ss.ffffff'.

    @param[in] microseconds_since_epoch time point represented
    by micro-seconds elapsed since the Epoch

    @returns a string with the format 'YYYY-MM-DD hh:mm:ss.ffffff'
  */
  static std::string convert_microseconds_to_timestamp_string(
      uint64_t microseconds_since_epoch);

  eject_status leave_group();

  /// Checks if group_replication is running.
  bool is_group_replication_running() const;

  /// Checks if group is in single primary mode and secondary
  bool is_group_in_single_primary_mode_and_im_a_secondary() const;

  /// Timestamp of last eviction caused by applier lag
  std::string get_applier_eviction_timestamp();

  /// Timestamp of last eviction caused by recovery lag
  std::string get_recovery_eviction_timestamp();

  /// Timestamp of last eviction caused by low memory
  std::string get_memory_eviction_timestamp();

  /// Timestamp of last channel lag query failure
  std::string get_lag_query_last_error_timestamp();

  /// Timestamp of last memory status failure
  std::string get_memory_last_error_timestamp();

 private:
  /// Wrappers to call API and do error handling.
  int fetch_memory_used();
  int fetch_channel_lag();

 private:
  /// frequency of sample collection, in seconds
  static constexpr uint s_sample_rate{5};
  /// Continuous lag tolerance limit.
  static constexpr uint s_max_continuous_lag_counter{10};

  /// Timestamp of last channel lag query failure.
  std::atomic<uint64_t> m_channel_lag_monitoring_error_timestamp{0};
  /// Timestamp of last memory information failure.
  std::atomic<uint64_t> m_memory_monitoring_error_timestamp{0};

  /// Number of times applier channel lag was hit continuously.
  uint m_applier_continuous_lag{0};
  /// Number of times recovery channel lag was hit continuously.
  uint m_recovery_continuous_lag{0};
  /// Number of times memory consumption cross the limit continuously.
  uint m_memory_continuous_excess_usage{0};

 private:
  /// Below information is needed by status variables.

  /// Applier channel lag fetched from SQL Query.
  std::atomic<uint> m_applier_lag_last_fetched{0};
  /// Recovery channel lag fetched from SQL Query.
  std::atomic<uint> m_recovery_lag_last_fetched{0};
  /// Used memory in percentage fetched from system.
  std::atomic<uint> m_percentage_used_memory_last_fetched{0};

  /// Number of times applier channel lag exceeded the limit.
  std::atomic<uint> m_applier_hit_number_of_times{0};
  /// Number of times recovery channel lag exceeded the limit.
  std::atomic<uint> m_recovery_hit_number_of_times{0};
  /// Number of times memory comsuption exceeded the limit.
  std::atomic<uint> m_memory_hit_number_of_times{0};

  /// Last timestamp member left the group due to applier lag.
  std::atomic<uint64_t> m_applier_eviction_timestamp{0};
  /// Last timestamp member left the group due to recovery lag.
  std::atomic<uint64_t> m_recovery_eviction_timestamp{0};
  /// Last timestamp member left the group due to memory excessive consumption.
  std::atomic<uint64_t> m_memory_eviction_timestamp{0};

 private:
  /// Time difference in seconds between last time this member joined and now.
  unsigned int m_seconds_since_member_join{0};

  /// Below variables are needed to reduce the frequency of logging.
  uint64_t m_not_removed_applier_threshold_hit_quarantime_ts_s{0};
  uint64_t m_not_removed_recovery_threshold_hit_quarantime_ts_s{0};
  uint64_t m_not_removed_memory_threshold_hit_quarantime_ts_s{0};
  uint64_t m_not_removed_applier_threshold_hit_n_members_ts_s{0};
  uint64_t m_not_removed_recovery_threshold_hit_n_members_ts_s{0};
  uint64_t m_not_removed_memory_threshold_hit_n_members_ts_s{0};

 public:
  /// Thread functions
  int start_thread();
  void stop_thread();
  void run_process();

 private:
  static int join(my_thread_handle *thread, void **value_ptr);
  static bool joinable(const my_thread_handle &thread);
  void process();

 private:
  /// Needed by THD
  const std::string m_name;
  const std::string m_instr_name;
  const std::string m_os_name;
  const std::string m_description;

  enum gr_rm_thread_state { IDLE, RUNNING, STOPPING };
  gr_rm_thread_state m_thread_state{IDLE};

  /// Thread structure.
  my_thread_handle m_thread;
  PSI_thread_key m_thread_key{0};
  PSI_thread_info m_thread_info{
      &m_thread_key,
      m_instr_name.c_str(),
      m_os_name.c_str(),
      PSI_FLAG_SINGLETON | PSI_FLAG_THREAD_SYSTEM,
      PSI_VOLATILITY_PROVISIONING,  // instruments are created/destroyed
      m_description.c_str()};

  /// Synchronize start, stop and update
  mysql_mutex_t m_command_lock;
  PSI_mutex_key m_command_key{0};
  PSI_mutex_info m_command_info{&m_command_key, m_instr_name.c_str(),
                                PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING,
                                PSI_DOCUMENT_ME};

  mysql_cond_t m_wait;
  PSI_cond_key m_cond_key{0};
  PSI_cond_info m_cond_info{&m_cond_key, m_instr_name.c_str(),
                            PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING,
                            PSI_DOCUMENT_ME};
};

}  // namespace gr_resource_manager

#endif /* RESOURCE_MANAGER_STATS_COLLECTOR_H */