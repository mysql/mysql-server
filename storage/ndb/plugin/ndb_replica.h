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

#ifndef NDB_REPLICA_H
#define NDB_REPLICA_H

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "storage/ndb/include/ndb_types.h"

// Repository for the NDB specific state of all started replication channels
class Ndb_replica {
 public:
  static constexpr size_t NUM_VIOLATION_COUNTERS = 10;
  static constexpr size_t NUM_API_STATS = 24;

  // Stats that can be observed for an individual Ndb_replica::Channel
  struct Channel_stats {
    /* Cumulative counter values */
    std::array<Uint64, NUM_VIOLATION_COUNTERS> violation_count{};
    Uint64 delete_delete_count{0};
    Uint64 reflect_op_prepare_count{0};
    Uint64 reflect_op_discard_count{0};
    Uint64 refresh_op_count{0};

    /* Transactional conflict detection */
    Uint64 trans_row_conflict_count{0};
    Uint64 trans_row_reject_count{0};
    Uint64 trans_detect_iter_count{0};
    Uint64 trans_in_conflict_count{0};
    Uint64 trans_conflict_commit_count{0};

    /* Last conflict epoch */
    Uint64 last_conflicted_epoch{0};

    /* Last stable epoch */
    Uint64 last_stable_epoch{0};

    /* Max applied epoch */
    Uint64 max_rep_epoch{0};

    /* NdbApi statistics */
    std::array<Uint64, NUM_API_STATS> api_stats{};
  };

  using Start_channel_func = std::function<bool()>;

  Ndb_replica(Start_channel_func func, Channel_stats *default_channel_stats)
      : m_start_channel_func(func),
        m_default_channel_stats(default_channel_stats) {}

  // State for one channel (from the ndbcluster's point of view)
  class Channel {
    const std::string m_channel_name;
    const Uint32 m_own_server_id;
    std::atomic<Uint32> m_applier_id_counter{0};
    // Pointer to instance where stats will be published
    Ndb_replica::Channel_stats *const m_channel_stats;
    // Protects the Channel's global state
    mutable std::mutex m_global_mutex;

    // The max applied epoch for this channel
    struct {
      bool initialized{false};  // Has 'max_rep_epoch' been initialized
      Uint64 value{0};
    } m_max_rep_epoch;

    // The last conflicted epoch for this channel
    Uint64 m_last_conflicted_epoch{0};

    // The last stable epoch for this channel
    Uint64 m_last_stable_epoch{0};

    // Cumulative counter values
    std::array<Uint64, NUM_VIOLATION_COUNTERS> total_violation_counters{};
    Uint64 total_delete_delete_count{0};
    Uint64 total_reflect_op_prepare_count{0};
    Uint64 total_reflect_op_discard_count{0};
    Uint64 total_refresh_op_count{0};

    // Cumulative transactional conflict detection counter values
    Uint64 total_trans_row_conflict_count{0};
    Uint64 total_trans_row_reject_count{0};
    Uint64 total_trans_detect_iter_count{0};
    Uint64 total_trans_in_conflict_count{0};
    Uint64 total_trans_conflict_commit_count{0};

    // Cumulative NdbApi statistics
    std::array<Uint64, NUM_API_STATS> m_total_api_stats{};

   public:
    Channel(std::string channel_name, Uint32 own_server_id,
            Channel_stats *channel_stats);

    Channel(Channel &) = delete;
    Channel(Channel &&) = delete;
    Channel operator=(Channel &) = delete;
    Channel operator=(Channel &&) = delete;

    ~Channel();

   private:
    // Keeps track of wheter Channel is started or stopped.
    // When creating a Channel instance it will be started. Therafter
    // it can be stopped and started again using start() and stop() while its
    // status can be queried using is_started().
    bool m_started{true};

   public:
    bool do_start();
    bool is_started() const;
    bool do_stop();

    std::string get_channel_name() const {
      // No lock since const member
      return m_channel_name;
    }

    Uint32 get_own_server_id() const {
      // No lock since const member
      return m_own_server_id;
    }

   private:
    // List of server_id's known to exist (as rows) in ndb_apply_status table.
    // NOTE! It's a cache which helps the applier(s) to decide whether to
    // update or write an entire new row.
    std::unordered_set<Uint32> m_existing_server_ids;

   public:
    bool serverid_exists(Uint32 server_id) const {
      std::lock_guard<std::mutex> lock(m_global_mutex);
      return m_existing_server_ids.count(server_id) != 0;
    }

    /**
       @brief Return unique ever increasing number for this channel.
       Used to identify each started Ndb_applier.

       @return ever increasing number
     */
    Uint32 get_next_applier_id() {
      // No lock since atomic variable
      return m_applier_id_counter++;
    }

    /**
       @brief Initialize the max replicated epoch value for this channel. This
       is done when channel is started in order to allow continue where
       it left off last time. All workers will call this function when they
       start but only the first will initialize the max replicated epoch value.

       @param highest_applied_epoch The highest applied epoch (as found in
                                    ndb_apply_status table when starting).

       @return true new value was assigned
     */
    bool initialize_max_rep_epoch(Uint64 highest_applied_epoch) {
      std::lock_guard<std::mutex> lock(m_global_mutex);
      if (m_max_rep_epoch.initialized) {
        return false;  // Already initialized
      }

      m_max_rep_epoch.initialized = true;
      m_max_rep_epoch.value = highest_applied_epoch;
      return true;
    }

    /**
       @brief Get the current max applied epoch for channel
       @return
     */
    Uint64 get_max_rep_epoch() const {
      std::lock_guard<std::mutex> lock(m_global_mutex);
      assert(m_max_rep_epoch.initialized);
      return m_max_rep_epoch.value;
    }

    /**
       @brief Update the channels global state with values and
       the stats collected during applying of epoch.

       @param max_rep_epoch            The max replicated epoch
       @param committed_epoch_value    The new committed epoch
       @param written_server_ids       List of server_id's written by trans
       @param violation_count          Array of violation count
       @param delete_delete_count      The delete-delete count
       @param reflect_op_prepare_count The reflect-op-prepare count
       @param reflect_op_discard_count The reflect-op-discard count
       @param refresh_op_count         The refresh op count
       @param trans_row_conflict_count The trans row conflict count
       @param trans_row_reject_count   The trans row reject count
       @param trans_in_conflict_count  The trans in conflict count
       @param trans_detect_iter_count  The trans detect iter count
     */
    void update_global_state(
        Uint64 max_rep_epoch, Uint64 committed_epoch_value,
        const std::vector<Uint32> &written_server_ids,
        const std::array<Uint32, NUM_VIOLATION_COUNTERS> &violation_count,
        Uint32 delete_delete_count, Uint32 reflect_op_prepare_count,
        Uint32 reflect_op_discard_count, Uint32 refresh_op_count,
        Uint32 trans_row_conflict_count, Uint32 trans_row_reject_count,
        Uint32 trans_in_conflict_count, Uint32 trans_detect_iter_count);

    /**
       @brief Update the channel's NdbApi statistic counters, the channel store
       the counters aggregated for all appliers.

       @param diff List of counter difference since last update
     */
    void update_api_stats(
        const std::array<Uint64, Ndb_replica::NUM_API_STATS> &diff);

    void copyout_channel_stats() const;
  };
  using ChannelPtr = std::shared_ptr<Channel>;

  bool start_channel(std::string channel_name, Uint32 server_id);
  bool stop_channel(std::string channel_name);
  bool reset_channel(std::string channel_name);
  ChannelPtr get_channel(std::string channel_name) const;

  std::size_t num_started_channels() const;

  /**
     @brief Initialize Ndb_replica subsystem

     @param start_applier_func Function to call when starting applier
     @param default_channel_stats Pointer to channel stats for default channel

     @return  false for sucessful initialization
  */
  static bool init(Start_channel_func start_applier_func,
                   Channel_stats *default_channel_stats);

  /**
     @brief Deinitialize Ndb_replica subsystem
   */
  static void deinit();

 private:
  Start_channel_func const m_start_channel_func;
  Channel_stats *const m_default_channel_stats;

  // Lock protecting the list of channels
  mutable std::mutex m_mutex;

  // The list of started channels. Using a reference counting shared_ptr to
  // avoid destroying the state before Ndb_applier has released it.
  std::unordered_map<std::string, ChannelPtr> m_channels;
};

extern std::unique_ptr<Ndb_replica> ndb_replica;

#endif
