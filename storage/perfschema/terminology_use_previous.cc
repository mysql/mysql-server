/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "storage/perfschema/terminology_use_previous.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "include/my_base.h"  // ulong
#include "my_dbug.h"
#include "sql/sql_class.h"  // thd_get_current_thd_terminology_use_previous
#include "storage/perfschema/pfs_instr_class.h"  // PFS_class_type

// Map from strings to strings
using str_map_t = std::unordered_map<std::string, const char *>;
// Map from a "class type" to a str_map_t.
using class_map_t = std::unordered_map<PFS_class_type, str_map_t>;
// Map from a version to a class_map_t.
using version_vector_t = std::vector<class_map_t>;

/**
  Maps that describe the name changes enabled by setting
  terminology_use_previous.

  The version_vector is a vector where each element holds information
  about names changed in a given server release.  Each element of
  version_vector is a map where keys are the instrumentation class of
  type PFS_class_type, and the values are maps that hold information
  about the names changed for that class, within the given server
  release.  In the latter map, the keys and values are strings,
  represented as std::string and const char *, respectively, where
  each key is the version of a name introduced in the given release
  and the corresponding value is the name that was used before that
  release.

  When you add elements, note that you need to increment the number
  passed as argument in the call to plan() in main() in
  storage/perfschema/unittest/pfs-t.cc, by the number of added
  elements.
*/
static str_map_t mutex_map_8_0_26 = {
    {"wait/synch/mutex/sql/Source_info::data_lock",
     "wait/synch/mutex/sql/Master_info::data_lock"},
    {"wait/synch/mutex/sql/Source_info::run_lock",
     "wait/synch/mutex/sql/Master_info::run_lock"},
    {"wait/synch/mutex/sql/Source_info::sleep_lock",
     "wait/synch/mutex/sql/Master_info::sleep_lock"},
    {"wait/synch/mutex/sql/Source_info::info_thd_lock",
     "wait/synch/mutex/sql/Master_info::info_thd_lock"},
    {"wait/synch/mutex/sql/Source_info::rotate_lock",
     "wait/synch/mutex/sql/Master_info::rotate_lock"},
    {"wait/synch/mutex/sql/Replica_reporting_capability::err_lock",
     "wait/synch/mutex/sql/Slave_reporting_capability::err_lock"},
    {"wait/synch/mutex/sql/key_mta_temp_table_LOCK",
     "wait/synch/mutex/sql/key_mts_temp_table_LOCK"},
    {"wait/synch/mutex/sql/key_mta_gaq_LOCK",
     "wait/synch/mutex/sql/key_mts_gaq_LOCK"},
    {"wait/synch/mutex/sql/Relay_log_info::replica_worker_hash_lock",
     "wait/synch/mutex/sql/Relay_log_info::slave_worker_hash_lock"},
    {"wait/synch/mutex/sql/LOCK_replica_list",
     "wait/synch/mutex/sql/LOCK_slave_list"},
    {"wait/synch/mutex/sql/LOCK_replica_net_timeout",
     "wait/synch/mutex/sql/LOCK_slave_net_timeout"},
    {"wait/synch/mutex/sql/LOCK_sql_replica_skip_counter",
     "wait/synch/mutex/sql/LOCK_sql_slave_skip_counter"},
};

static str_map_t rwlock_map_8_0_26 = {
    {"wait/synch/rwlock/sql/LOCK_sys_init_replica",
     "wait/synch/rwlock/sql/LOCK_sys_init_slave"},
};

static str_map_t cond_map_8_0_26 = {
    // Yes, it actually was called _lock! That was a typo.
    {"wait/synch/cond/sql/Relay_log_info::replica_worker_hash_cond",
     "wait/synch/cond/sql/Relay_log_info::slave_worker_hash_lock"},
    {"wait/synch/cond/sql/Source_info::data_cond",
     "wait/synch/cond/sql/Master_info::data_cond"},
    {"wait/synch/cond/sql/Source_info::start_cond",
     "wait/synch/cond/sql/Master_info::start_cond"},
    {"wait/synch/cond/sql/Source_info::stop_cond",
     "wait/synch/cond/sql/Master_info::stop_cond"},
    {"wait/synch/cond/sql/Source_info::sleep_cond",
     "wait/synch/cond/sql/Master_info::sleep_cond"},
    {"wait/synch/cond/sql/Source_info::rotate_cond",
     "wait/synch/cond/sql/Master_info::rotate_cond"},
    {"wait/synch/cond/sql/Relay_log_info::mta_gaq_cond",
     "wait/synch/cond/sql/Relay_log_info::mts_gaq_cond"},
};

static str_map_t memory_map_8_0_26 = {
    {"memory/sql/Replica_job_group::group_relay_log_name",
     "memory/sql/Slave_job_group::group_relay_log_name"},
    {"memory/sql/rpl_replica::check_temp_dir",
     "memory/sql/rpl_slave::check_temp_dir"},
    {"memory/sql/REPLICA_INFO", "memory/sql/SLAVE_INFO"},
    {"memory/sql/show_replica_status_io_gtid_set",
     "memory/sql/show_slave_status_io_gtid_set"},
    {"memory/sql/Relay_log_info::mta_coor",
     "memory/sql/Relay_log_info::mts_coor"},
};

static str_map_t thread_map_8_0_26 = {
    {"thread/sql/replica_io", "thread/sql/slave_io"},
    {"thread/sql/replica_sql", "thread/sql/slave_sql"},
    {"thread/sql/replica_worker", "thread/sql/slave_worker"},
};

static str_map_t stage_map_8_0_26 = {
    {"stage/sql/Changing replication source", "stage/sql/Changing master"},
    {"stage/sql/Checking source version", "stage/sql/Checking master version"},
    {"stage/sql/Connecting to source", "stage/sql/Connecting to master"},
    {"stage/sql/Flushing relay log and source info repository.",
     "stage/sql/Flushing relay log and master info repository."},
    {"stage/sql/Killing replica", "stage/sql/Killing slave"},
    {"stage/sql/Source has sent all binlog to replica; waiting for more "
     "updates",
     "stage/sql/Master has sent all binlog to slave; waiting for more updates"},
    {"stage/sql/Queueing source event to the relay log",
     "stage/sql/Queueing master event to the relay log"},
    {"stage/sql/Reconnecting after a failed source event read",
     "stage/sql/Reconnecting after a failed master event read"},
    {"stage/sql/Reconnecting after a failed registration on source",
     "stage/sql/Reconnecting after a failed registration on master"},
    {"stage/sql/Registering replica on source",
     "stage/sql/Registering slave on master"},
    {"stage/sql/Sending binlog event to replica",
     "stage/sql/Sending binlog event to slave"},
    {"stage/sql/Replica has read all relay log; waiting for more updates",
     "stage/sql/Slave has read all relay log; waiting for more updates"},
    {"stage/sql/Waiting for replica workers to process their queues",
     "stage/sql/Waiting for slave workers to process their queues"},
    {"stage/sql/Waiting for Replica Worker queue",
     "stage/sql/Waiting for Slave Worker queue"},
    {"stage/sql/Waiting for Replica Workers to free pending events",
     "stage/sql/Waiting for Slave Workers to free pending events"},
    {"stage/sql/Waiting for Replica Worker to release partition",
     "stage/sql/Waiting for Slave Worker to release partition"},
    {"stage/sql/Waiting until SOURCE_DELAY seconds after source executed event",
     "stage/sql/Waiting until MASTER_DELAY seconds after master executed "
     "event"},
    {"stage/sql/Waiting for source to send event",
     "stage/sql/Waiting for master to send event"},
    {"stage/sql/Waiting for source update",
     "stage/sql/Waiting for master update"},
    {"stage/sql/Waiting for the replica SQL thread to free relay log space",
     "stage/sql/Waiting for the slave SQL thread to free enough relay log "
     "space"},
    {"stage/sql/Waiting for replica mutex on exit",
     "stage/sql/Waiting for slave mutex on exit"},
    {"stage/sql/Waiting for replica thread to start",
     "stage/sql/Waiting for slave thread to start"},
    {"stage/sql/Waiting for the replica SQL thread to advance position",
     "stage/sql/Waiting for the slave SQL thread to advance position"},
    {"stage/sql/Waiting to reconnect after a failed registration on source",
     "stage/sql/Waiting to reconnect after a failed registration on master"},
    {"stage/sql/Waiting to reconnect after a failed source event read",
     "stage/sql/Waiting to reconnect after a failed master event read"}};

static str_map_t thread_command_map_8_0_26 = {
    {"statement/com/Register Replica", "statement/com/Register Slave"}};

static class_map_t class_map_8_0_26 = {
    {PFS_CLASS_MUTEX, mutex_map_8_0_26},
    {PFS_CLASS_RWLOCK, rwlock_map_8_0_26},
    {PFS_CLASS_COND, cond_map_8_0_26},
    {PFS_CLASS_MEMORY, memory_map_8_0_26},
    {PFS_CLASS_THREAD, thread_map_8_0_26},
    {PFS_CLASS_STAGE, stage_map_8_0_26},
    {PFS_CLASS_STATEMENT, thread_command_map_8_0_26},
};

// This should have one element corresponding to each member of
// enum_compatibility_version, except NONE.
static version_vector_t version_vector = {
    class_map_8_0_26,
};

namespace terminology_use_previous {

compatible_name_t lookup(PFS_class_type class_type, const std::string &str,
                         bool use_prefix) {
  for (version_vector_t::size_type int_version = 0;
       int_version < version_vector.size(); ++int_version) {
    const enum_compatibility_version enum_version{
        static_cast<enum_compatibility_version>(int_version + 1)};
    auto &class_map = version_vector[int_version];
    auto class_name_pair = class_map.find(class_type);
    if (class_name_pair != class_map.end()) {
      auto &name_map = class_name_pair->second;
      size_t prefix_length{0};
      std::string lookup_str;
      if (use_prefix)
        lookup_str = str;
      else {
        // Get length of prefix and prepend it to str
        assert(!name_map.empty());
        const auto &elem = name_map.begin()->first;
        prefix_length = elem.rfind('/') + 1;
        lookup_str = elem.substr(0, prefix_length) + str;
      }
      auto name_pair = name_map.find(lookup_str);
      if (name_pair != name_map.end()) {
        const auto *name = name_pair->second;
        return compatible_name_t{name + prefix_length, enum_version};
      }
    }
  }
  return compatible_name_t{nullptr, NONE};
}

bool is_older_required(enum_compatibility_version version) {
  const ulong i_n_c = thd_get_current_thd_terminology_use_previous();
  return i_n_c != 0 && i_n_c <= (ulong)version;
}

}  // namespace terminology_use_previous
