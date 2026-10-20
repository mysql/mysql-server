/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#pragma once

#include <shared_mutex>
#include <string>
#include <vector>

#include "component_event_tracking.h"
#include "component_service.h"
#include "health_monitor_query.h"  // Result_Set

namespace hm {

typedef std::vector<std::string> Processlist_Ids;

/**
  Group Replication status.
*/
struct GR_Status {
  GR_Status() = default;
  bool valid{false};
  bool single_primary_mode{false};
  bool single_primary_mode_primary{false};
  bool single_primary_mode_secondary{false};
  bool online_with_group_majority{false};
  std::string role() const;
  std::string view_id;
};

/**
  Group replication status listener.
  Listens for changes to group replication status and roles (primary/secondary).
*/
class GR_Status_Listener {
 public:
  static DEFINE_BOOL_METHOD(notify_member_role_change, (const char *));
  static DEFINE_BOOL_METHOD(notify_member_state_change, (const char *));
};

// When an unfenced secondary that is out of disk space is promoted to primary,
// the new primary will be fenced. The delay until GR sets SUPER_READ_ONLY = OFF
// during switchover is unbounded. The Health Monitor must wait until
// SUPER_READ_ONLY = OFF before fencing the server.

// #define DEBUG_SWITCHOVER_LAG

/**
  Server utility class.
  Provides server information, status and utilities.
*/
class Server {
 public:
  // Group replication
  static int register_group_member_status_listeners();
  static int unregister_group_member_status_listeners();
  static GR_Status gr_status();
  static GR_Status get_group_replication_status();
  static void set_group_replication_status(const char *view_id);
  static bool is_group_replication_primary();
  static bool is_group_replication_secondary();
  static bool is_member_online_with_group_majority();
#ifdef DEBUG_SWITCHOVER_LAG
  static bool m_debug_sro_new_primary;
#endif

  // Processlist
  Processlist_Ids get_fg_sessions_with_global_lock();

  int kill_query(const std::string &pid, std::string &errmsg);
  int kill_query(const std::string &pid);
  int kill_connection(const std::string &pid, std::string &errmsg);
  int kill_connection(const std::string &pid);

  // System variables
  static bool set_system_variable(const std::string &name,
                                  const std::string &value);
  static bool get_system_variable(const std::string &name, std::string &value);
  static bool get_status_variable(const std::string &name, std::string &value);

  static bool get_offline_mode(bool &error);
  static std::string get_offline_mode_str();
  static bool set_offline_mode();
  static bool clear_offline_mode();

  static bool get_super_read_only(bool &error);
  static std::string get_super_read_only_str();
  static bool set_super_read_only();
  static bool clear_super_read_only();

  static bool get_read_only(bool &error);
  // READ_ONLY is set when SUPER_READ_ONLY is set
  static bool clear_read_only();

  static bool get_log_diagnostic_enable(bool &error);

  // Utilities
  static std::string &to_lower(std::string &str);

 private:
  static Processlist_Ids processlist_id_list(Result_Set &results);
  static GR_Status m_gr_status;
  static std::shared_mutex m_gr_mutex;

#ifdef DEBUG_SWITCHOVER_LAG
  // SRO counter to simulate failover lag
  static bool m_debug_sro;
  static int m_debug_sro_max_attempts;
  static int m_debug_sro_count;
  static bool m_debug_sro_value;
#endif
};

}  // namespace hm
