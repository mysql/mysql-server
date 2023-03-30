/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

// Implements
#include "storage/ndb/plugin/ndb_replica.h"

// Using
#include <algorithm>
#include <utility>

#include "my_dbug.h"
#include "sql/replication.h"  // Binlog_relay_IO_observer
#include "storage/ndb/include/ndb_types.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_plugin_reference.h"

Ndb_replica::Channel::Channel(std::string channel_name, Uint32 own_server_id,
                              Channel_stats *channel_stats)
    : m_channel_name(channel_name),
      m_own_server_id(own_server_id),
      m_channel_stats(channel_stats) {
  ndb_log_info("Creating Ndb_replica::Channel: '%s'", m_channel_name.c_str());
  // Update the published channel stats when channel is created. Otherwise the
  // stats (which are now zero) will not be published untli first commit.
  copyout_channel_stats();
}

Ndb_replica::Channel::~Channel() {
  ndb_log_info("Removing Ndb_replica::Channel: '%s'", m_channel_name.c_str());

  // Update the published channel stats when channel is destroyed.
  //  - the 'max_rep_epoch' is reset to zero while all other are kept as is.
  m_max_rep_epoch.value = 0;
  copyout_channel_stats();
}

bool Ndb_replica::Channel::do_start() {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("channel_name: '%s'", m_channel_name.c_str()));
  assert(m_started == false);
  m_started = true;
  return true;
}

bool Ndb_replica::Channel::is_started() const { return m_started; }

bool Ndb_replica::Channel::do_stop() {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("channel_name: '%s'", m_channel_name.c_str()));
  assert(m_started == true);
  m_started = false;
  return true;
}

void Ndb_replica::Channel::update_global_state(
    Uint64 max_rep_epoch, Uint64 committed_epoch_value,
    const std::vector<Uint32> &written_server_ids,
    const std::array<Uint32, NUM_VIOLATION_COUNTERS> &violation_counters,
    Uint32 delete_delete_count, Uint32 reflect_op_prepare_count,
    Uint32 reflect_op_discard_count, Uint32 refresh_op_count,
    Uint32 trans_row_conflict_count, Uint32 trans_row_reject_count,
    Uint32 trans_in_conflict_count, Uint32 trans_detect_iter_count) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("channel name: '%s'", m_channel_name.c_str()));
  std::lock_guard<std::mutex> lock(m_global_mutex);

  assert(m_max_rep_epoch.initialized);
  if (max_rep_epoch > m_max_rep_epoch.value) {
    DBUG_PRINT("info", ("Max replicated epoch increases from %llu to %llu",
                        m_max_rep_epoch.value, max_rep_epoch));
    m_max_rep_epoch.value = max_rep_epoch;
  } else {
    // Use the max value from here on
    max_rep_epoch = m_max_rep_epoch.value;
  }

  for (const auto &id : written_server_ids) {
    m_existing_server_ids.insert(id);
  }

  // Aggregate counflict counter totals and count conflicts
  Uint32 num_conflicts = 0;
  for (size_t i = 0; i < violation_counters.size(); i++) {
    num_conflicts += violation_counters[i];
    total_violation_counters[i] += violation_counters[i];
  }

  total_delete_delete_count += delete_delete_count;
  total_reflect_op_prepare_count += reflect_op_prepare_count;
  total_reflect_op_discard_count += reflect_op_discard_count;
  total_refresh_op_count += refresh_op_count;

  // Transaction conflict detection counters
  total_trans_row_conflict_count += trans_row_conflict_count;
  total_trans_row_reject_count += trans_row_reject_count;
  total_trans_in_conflict_count += trans_in_conflict_count;
  if (trans_in_conflict_count) {
    total_trans_conflict_commit_count++;
  }
  total_trans_detect_iter_count += trans_detect_iter_count;

  // Update 'last_conflicted_epoch' if local conflicts found
  if (num_conflicts > 0) {
    DBUG_PRINT("info", ("Conflict detected locally, increasing last "
                        "conflicted epoch from %llu to %llu",
                        m_last_conflicted_epoch, committed_epoch_value));
    m_last_conflicted_epoch = committed_epoch_value;
    return;
  }

  /**
   * Update 'last_conflicted_epoch' if reflected or refresh ops applied
   * (Implies Secondary role in asymmetric algorithms)
   */
  assert(reflect_op_prepare_count >= reflect_op_discard_count);
  const Uint32 reflected_ops =
      reflect_op_prepare_count - reflect_op_discard_count;
  if (reflected_ops > 0 || refresh_op_count > 0) {
    DBUG_PRINT(
        "info",
        ("Reflected (%u) or Refresh (%u) operations applied "
         "this epoch, increasing last conflicted epoch from %llu to %llu",
         reflected_ops, refresh_op_count, m_last_conflicted_epoch,
         committed_epoch_value));
    m_last_conflicted_epoch = committed_epoch_value;
    return;
  }

  // Update 'last_stable_epoch' when applying epoch with higher max replicated
  // value than last conflicted
  if (max_rep_epoch >= m_last_conflicted_epoch) {
    /**
     * This epoch which has looped the circle was stable -
     * no new conflicts have been found / corrected since
     * it was logged
     */
    DBUG_PRINT("info", ("Found epoch which has looped the circle, increasing "
                        "stable epoch from %llu to %llu",
                        m_last_stable_epoch, max_rep_epoch));
    m_last_stable_epoch = max_rep_epoch;

    /**
     * Note that max_rep_epoch >= last_conflicted_epoch implies that there are
     * no currently known-about conflicts.
     * On the primary this is a definitive fact as it finds out about all
     * conflicts immediately.
     * On the secondary it does not mean that there are not committed
     * conflicts, just that they have not started being corrected yet.
     */
  }
}

void Ndb_replica::Channel::update_api_stats(
    const std::array<Uint64, Ndb_replica::NUM_API_STATS> &stats_diff) {
  std::lock_guard<std::mutex> lock(m_global_mutex);
  for (size_t i = 0; i < stats_diff.size(); i++) {
    m_total_api_stats[i] += stats_diff[i];
  }
}

void Ndb_replica::Channel::copyout_channel_stats() const {
  DBUG_TRACE;
  if (!m_channel_stats) {
    // Pointer for where to publish stats has not been setup for channel
    return;
  }

  // Copy out global counters and values
  std::lock_guard<std::mutex> lock(m_global_mutex);

  // Epoch related variables
  m_channel_stats->max_rep_epoch = m_max_rep_epoch.value;
  m_channel_stats->last_conflicted_epoch = m_last_conflicted_epoch;
  m_channel_stats->last_stable_epoch = m_last_stable_epoch;

  // Conflict detection counters
  m_channel_stats->violation_count = total_violation_counters;
  m_channel_stats->delete_delete_count = total_delete_delete_count;
  m_channel_stats->reflect_op_prepare_count = total_reflect_op_prepare_count;
  m_channel_stats->reflect_op_discard_count = total_reflect_op_discard_count;
  m_channel_stats->refresh_op_count = total_refresh_op_count;

  // Transactional conflict detection counters
  m_channel_stats->trans_row_conflict_count = total_trans_row_conflict_count;
  m_channel_stats->trans_row_reject_count = total_trans_row_reject_count;
  m_channel_stats->trans_detect_iter_count = total_trans_detect_iter_count;
  m_channel_stats->trans_in_conflict_count = total_trans_in_conflict_count;
  m_channel_stats->trans_conflict_commit_count =
      total_trans_conflict_commit_count;

  // Aggregated NdbApi stats for all Applier's
  m_channel_stats->api_stats = m_total_api_stats;
}

extern Ndb_replica::Channel_stats g_default_channel_stats;

bool Ndb_replica::start_channel(std::string channel_name, Uint32 server_id) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("channel_name: '%s'", channel_name.c_str()));
  DBUG_PRINT("enter", ("server_id: %d", server_id));

  if (!m_start_channel_func()) {
    return false;
  }

  constexpr const char *DEFAULT_CHANNEL_NAME = "";

  std::lock_guard<std::mutex> lock_channels(m_mutex);
  assert(channel_name == DEFAULT_CHANNEL_NAME);

  const auto found = m_channels.find(channel_name);
  if (found != m_channels.end()) {
    DBUG_PRINT("info", ("Channel already exists"));
    ChannelPtr channel = found->second;
    return channel->do_start();
  }

  // Create new Channel
  const auto res = m_channels.emplace(
      channel_name,
      std::make_shared<Channel>(
          channel_name, server_id,
          // Pass pointer to stats instance for default channel
          channel_name == DEFAULT_CHANNEL_NAME ? m_default_channel_stats
                                               : nullptr));
  return res.second;
}

bool Ndb_replica::stop_channel(std::string channel_name) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("channel_name: '%s'", channel_name.c_str()));

  std::lock_guard<std::mutex> lock_channels(m_mutex);
  const auto found = m_channels.find(channel_name);
  if (found != m_channels.end()) {
    DBUG_PRINT("info", ("Channel already exists"));
    ChannelPtr channel = found->second;
    return channel->do_stop();
  }

  // No Channel to stop
  return true;
}

bool Ndb_replica::reset_channel(std::string channel_name) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("channel_name: '%s'", channel_name.c_str()));

  std::lock_guard<std::mutex> lock_channels(m_mutex);
  if (m_channels.count(channel_name) == 0) {
    DBUG_PRINT("info", ("No channel to reset"));
    return true;
  }
  const auto removed = m_channels.erase(channel_name);
  return removed != 0;
}

Ndb_replica::ChannelPtr Ndb_replica::get_channel(
    std::string channel_name) const {
  std::lock_guard<std::mutex> lock_channels(m_mutex);
  const auto channel = m_channels.find(channel_name);
  if (channel == m_channels.end()) {
    return nullptr;
  }
  return channel->second;
}

std::size_t Ndb_replica::num_started_channels() const {
  std::lock_guard<std::mutex> lock_channels(m_mutex);
  return std::count_if(m_channels.begin(), m_channels.end(),
                       [](const auto &c) { return c.second->is_started(); });
}

// The Ndb_replica instance
std::unique_ptr<Ndb_replica> ndb_replica;

/**
   @brief Handle replication applier start.

   @note This function is called when the SQL thread is started. When using
   workers it will act as coordinator for the individual workers which are
   started later.

   @return 0 on success, error code if checks failed
 */
static int replica_applier_start(Binlog_relay_IO_param *param) {
  DBUG_TRACE;

  // Only allow default channel (ie. name = "") to be started
  const std::string channel_name(param->channel_name);
  if (channel_name != "") {
    ndb_log_error(
        "NDB Replica: Multi source replication is not supported when "
        "replicating to NDB. Only the default channel (with name = '') can be "
        "used");
    return 1;
  }

  // Note! The 'param->server_id' is the server_id of this server
  if (!ndb_replica->start_channel(channel_name, param->server_id)) {
    ndb_log_error("NDB Replica: Failed to create channel '%s'",
                  param->channel_name);
    return 1;
  }

  return 0;
}

/**
   @brief Handle replication applier thread stop

   @note This function is called when the SQL thread is stopped. When acting as
   coordinator, this will stop further work being assigned. However the
   individual workers are still alive and will be stopped later.

   @return 0 for success (error code is ignored)
 */
static int replica_applier_stop(Binlog_relay_IO_param *param, bool) {
  DBUG_TRACE;
  if (!ndb_replica->stop_channel(param->channel_name)) {
    ndb_log_error("NDB Replica: Failed to remove channel '%s'",
                  param->channel_name);
    return 1;
  }
  return 0;
}

/**
   @brief Handle replication replica reset.

   @note Function is called when the replica state is reset with RESET REPLICA.

   @return 0 on success, error code if checks failed
 */
static int replica_reset(Binlog_relay_IO_param *param) {
  DBUG_TRACE;

  if (!ndb_replica->reset_channel(param->channel_name)) {
    ndb_log_error("NDB Replica: Failed to reset channel '%s'",
                  param->channel_name);
    return 1;
  }

  return 0;
}

static bool observer_initialized = false;
static Binlog_relay_IO_observer relay_IO_observer = {
    sizeof(Binlog_relay_IO_observer),

    nullptr,                // thread_start
    nullptr,                // thread_stop
    replica_applier_start,  // applier_start
    replica_applier_stop,   // applier_stop
    nullptr,                // before_request_transmit
    nullptr,                // after_read_event
    nullptr,                // after_queue_event
    replica_reset,          // after_reset
    nullptr                 // applier_log_event
};

bool Ndb_replica::init(Start_channel_func start_channel_func,
                       Channel_stats *default_channel_stats) {
  assert(!observer_initialized);

  // Create the Ndb_replica instance
  ndb_replica =
      std::make_unique<Ndb_replica>(start_channel_func, default_channel_stats);

  Ndb_plugin_reference ndbcluster_plugin;

  // Resolve pointer to the ndbcluster plugin
  if (!ndbcluster_plugin.lock()) return false;

  // Install replication observer to be called when applier thread start
  if (register_binlog_relay_io_observer(&relay_IO_observer,
                                        ndbcluster_plugin.handle())) {
    ndb_log_error("Failed to register binlog relay io observer");
    return true;
  }
  observer_initialized = true;

  return false;
}

void Ndb_replica::deinit() {
  if (observer_initialized) {
    unregister_binlog_relay_io_observer(&relay_IO_observer, nullptr);
  }

  // Destroy the Ndb_replica instance
  ndb_replica.reset();
}
