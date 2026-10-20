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

#include "health_monitor_server.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <mutex>
#include <string>

#include "mysql/components/services/group_member_status_listener.h"
#include "mysql/components/services/group_replication_status_service.h"

#include "component_event_tracking.h"
#include "component_register.h"
#include "component_service.h"
#include "component_status.h"
#include "component_string.h"
#include "component_sysvar.h"

#include "health_monitor.h"
#include "health_monitor_log.h"
#include "health_monitor_query.h"

// clang-format off

namespace Event_tracking_implementation {

/**
  Startup event callback: Not used.
*/
mysql_event_tracking_startup_subclass_t
    Event_tracking_lifecycle_implementation::startup_filtered_sub_events = 0;
bool Event_tracking_lifecycle_implementation::callback(
    const mysql_event_tracking_startup_data *data [[maybe_unused]]) {
  return false;
}

/**
  Shutdown event callback: Notifies the Health Monitor that the server is
  shutting down.
*/
mysql_event_tracking_shutdown_subclass_t
    Event_tracking_lifecycle_implementation::shutdown_filtered_sub_events = 0;
bool Event_tracking_lifecycle_implementation::callback(
    const mysql_event_tracking_shutdown_data *data [[maybe_unused]]) {
  hm::Health_Monitor::instance()->set_server_shutting_down(true); // stop all work
  return false;
}

} // namespace Event_tracking_implementation

namespace hm {

/**
  Group Replication status listener.
  Register listener methods to receive Group Replication status changes.
  Note: These calls hold a mutex, so process quickly and don't call too
  frequently.
*/
static const char *k_GR_listener_service_name{"group_member_status_listener.health_monitor"};

static SERVICE_TYPE(group_member_status_listener) svc_gmst_def = {
    GR_Status_Listener::notify_member_role_change,
    GR_Status_Listener::notify_member_state_change};

using svc_gmst_t = SERVICE_TYPE_NO_CONST(group_member_status_listener);
my_h_service h_gmst_listener = reinterpret_cast<my_h_service>(const_cast<svc_gmst_t *>(&svc_gmst_def));

DEFINE_BOOL_METHOD(GR_Status_Listener::notify_member_role_change,
                   (const char *view_id)) {
  const std::string last_role{Server::gr_status().role()};  // ok if GR status not valid
  Server::set_group_replication_status(view_id);
  const std::string new_role{Server::gr_status().role()};
  if (new_role != last_role) {
    std::string msg{"Detected HA role change from "};
    msg += last_role + " to " + new_role;
    log_info("%s", msg.c_str());
  }
#ifdef DEBUG_SWITCHOVER_LAG
  if (new_role == "PRIMARY") {
    Server::m_debug_sro_new_primary = true;
  }
#endif
  return 0;
}

DEFINE_BOOL_METHOD(GR_Status_Listener::notify_member_state_change,
                   (const char *view_id)) {
  Server::set_group_replication_status(view_id);
  return 0;
}

/**
  Group replication status.
*/
GR_Status Server::m_gr_status;

/**
  Group Replication role name
  @return Role name
*/
std::string GR_Status::role() const {
  if (single_primary_mode_primary) {
    return "PRIMARY";
  }
  if (single_primary_mode_secondary) {
    return "SECONDARY";
  }
  return "n/a";
}

/**
  Register the group member status listener.
  @return 0 if successful, non-zero otherwise
*/
int Server::register_group_member_status_listeners() {
  return registry_reg_srv->register_service(k_GR_listener_service_name,
                                            h_gmst_listener);
}

/**
  Unregister the group member status listener.
  @return 0 if successful, non-zero otherwise
*/
int Server::unregister_group_member_status_listeners() {
  return registry_reg_srv->unregister(k_GR_listener_service_name);
}

/**
  Get the group replication status.
  @return GR_Status
*/
GR_Status Server::gr_status() {
  std::shared_lock const lock(m_gr_mutex);  // read lock
  return m_gr_status;
}

/**
  Get the current group replication status.
  Group replication status is dynamic and must be refreshed for each check.
  @return GR_Status
*/
GR_Status Server::get_group_replication_status() {
  GR_Status gr_status;
  // The group replication status service may or may not be available when the
  // Health Monitor component is loaded
  const my_service<SERVICE_TYPE_NO_CONST(
      group_replication_status_service_v1)>
      gr_status_srv("group_replication_status_service_v1", registry_srv);

  if (gr_status_srv.is_valid()) {
    // NOLINTBEGIN(readability-implicit-bool-conversion)
    gr_status.single_primary_mode = gr_status_srv->is_group_in_single_primary_mode();
    gr_status.single_primary_mode_primary = gr_status_srv->is_group_in_single_primary_mode_and_im_the_primary();
    gr_status.single_primary_mode_secondary = gr_status_srv->is_group_in_single_primary_mode_and_im_a_secondary();
    gr_status.online_with_group_majority = gr_status_srv->is_member_online_with_group_majority();
    gr_status.valid = true;
    // NOLINTEND(readability-implicit-bool-conversion)
  }
  return gr_status;
}

/**
  Initialize static members.
*/
std::shared_mutex Server::m_gr_mutex;

#ifdef DEBUG_SWITCHOVER_LAG
bool Server::m_debug_sro{true};
int Server::m_debug_sro_max_attempts{9};  // x10 seconds
int Server::m_debug_sro_count{0};
bool Server::m_debug_sro_value{true};  // true for SUPER_READ_ONLY = ON
bool Server::m_debug_sro_new_primary{false};
#endif

/**
  Set the internal group replication status.
  @param view_id  Group replication view id
*/
void Server::set_group_replication_status(const char *view_id) {
  std::unique_lock const lock(m_gr_mutex);  // write lock
  m_gr_status = get_group_replication_status();
  m_gr_status.view_id = view_id;
}

/**
  Determine if the server is a group replication primary.
  @return true if server is a primary, false otherwise
*/
bool Server::is_group_replication_primary() {
  return gr_status().single_primary_mode_primary;
}

/**
  Determine if the server is a group replication secondary.
  @return true if server is a secondary, false otherwise
*/
bool Server::is_group_replication_secondary() {
  return gr_status().single_primary_mode_secondary;
}

/**
  Determine if the server is online and in a group majority.
  @return true if server is in a group majority, false otherwise
*/
bool Server::is_member_online_with_group_majority() {
  return gr_status().online_with_group_majority;
}

/**
  Get list of foreground sessions holding a global read lock.
  Only one session is expected, but this will accommodate multiple ids.
  Filter protected service accounts, allow root for MTR.
  @return String vector with 0 or more processlist ids
*/
Processlist_Ids Server::get_fg_sessions_with_global_lock() {
  Result_Set results;
  const Query query{
      "WITH table_tids AS ("
      "  SELECT processlist_id AS pid, thread_id AS tid"
      "    FROM performance_schema.threads"
      "    WHERE type = 'foreground'"
      "      AND name like '%one_connection%'"
#ifndef NDEBUG
      "      AND processlist_user != 'root'"
#endif
#ifdef HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER
      "      AND processlist_user != '"
      HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER
      "'"
#endif
      "      AND processlist_user NOT LIKE 'mysql_innodb&'"
      "      AND processlist_user NOT LIKE 'mysql_router%'"
      "      AND (processlist_state <> 'query end' OR processlist_state IS NULL)"
      ") "
      "SELECT tts.pid AS pid"
      "  FROM table_tids tts"
      "  JOIN performance_schema.metadata_locks mdl"
      "    ON tts.tid = mdl.owner_thread_id"
      "      AND mdl.object_type = 'global'"
      "      AND mdl.lock_type like '%exclusive%'"
      "      AND mdl.lock_status IN ('granted', 'pending')"};
  query.run(results);
  return processlist_id_list(results);
}

/**
  Build a list of processlist ids from a result set.
  @param results  Query results
  @return String vector with 0 or more processlist ids
*/
Processlist_Ids Server::processlist_id_list(Result_Set &results) {
  Processlist_Ids processlist_ids;
  for (size_t rdx = 0; rdx < results.size(); rdx++) {
    if (rdx > 1) {  // skip title and column headers
      const Row row = results.at(rdx);
      if (row.empty()) continue;
      for (const auto &field : row) {
        // cppcheck-suppress useStlAlgorithm
        processlist_ids.push_back(field);
      }
    }
  }
  return processlist_ids;
}

/**
  Terminate the query running on a connection.
  Return error message if command fails.
  @param[in] pid  Processlist id string
  @param[out] errmsg  Error message if any
  @return 0 Success, 1 query error
*/
int Server::kill_query(const std::string &pid, std::string &errmsg) {
  Result_Set results;
  const Query query{"KILL QUERY " + pid};
  const int rc = query.run(results);
  if (rc != 0 && !results.empty()) {
    Row row{results.at(0)};
    errmsg = row.at(0);
  }
  return rc;
}

/**
  Terminate the query running on a connection.
  @param[in] pid  Processlist id string
  @return 0 Success, 1 query error
*/
int Server::kill_query(const std::string &pid) {
  std::string errmsg;
  return kill_query(pid, errmsg);
}

/**
  Terminate a server thread given a proceslist id.
  Note that mysql client apps may attempt to reconnect.
  Issue warning if command fails.
  @param[in] pid  Processlist id string
  @param[out] errmsg  Error message if any
  @return 0 Success, 1 query error
*/
int Server::kill_connection(const std::string &pid, std::string &errmsg) {
  Result_Set results;
  const Query query{"KILL CONNECTION " + pid};
  const int rc = query.run(results);
  if (rc != 0 && !results.empty()) {
    Row row{results.at(0)};
    errmsg = row.at(0);
  }
  return rc;
}

/**
  Terminate a server thread given a processlist id.
  @param[in] pid  Processlist id string
  @return 0 Success, 1 query error
*/
int Server::kill_connection(const std::string &pid) {
  std::string errmsg;
  return kill_connection(pid, errmsg);
}

/**
  Set server system variable.
  @param[in] name  variable name
  @param[in] value variable value
  @return false if successful, true otherwise
*/
bool Server::set_system_variable(const std::string &name,
                                 const std::string &value) {
  if (string_converter_srv == nullptr || charset_srv == nullptr ||
      sysvar_update_srv == nullptr || string_factory_srv == nullptr ||
      sysvar_attributes_srv == nullptr)
    return true;

  my_h_string var_name{nullptr};
  my_h_string var_value{nullptr};
  CHARSET_INFO_h utf8mb4_h{charset_srv->get_utf8mb4()};

  // Build the system variable name and value strings
  string_factory_srv->create(&var_name);
  string_factory_srv->create(&var_value);
  int error = string_converter_srv->convert_from_buffer(
      var_name, name.c_str(), name.length(), utf8mb4_h);
  if (error == 0) {
    error = string_converter_srv->convert_from_buffer(
        var_value, value.c_str(), value.length(), utf8mb4_h);
  }

  // Set the system variable value
  if (error == 0) {
    MYSQL_THD thd{nullptr};
    // Note that the system variable service sets a temporary 5 second
    // lock wait timeout in case the update is blocked
    error = sysvar_update_srv->set(thd, "GLOBAL", nullptr, var_name, var_value);
  }

  if (error == 0) {
    // special case, attach (the only possible) reason attribute when setting
    // OFFLINE_MODE or SUPER_READ_ONLY to "ON"
    if ((name == "OFFLINE_MODE" || name == "SUPER_READ_ONLY") &&
        value == "ON") {
      bool const retval = sysvar_attributes_srv->set(
          nullptr, name.c_str(), "reason", "(HM) low disk space");
      error = retval ? 1 : 0;
    }
  }

  // Clean up the system variable string memory
  if (var_name != nullptr) string_factory_srv->destroy(var_name);
  if (var_value != nullptr) string_factory_srv->destroy(var_value);

  return error != 0;
}

/**
  Get server system variable.
  @param[in]  name  variable name
  @param[out] value variable value
  @return false if successful, true otherwise
*/
bool Server::get_system_variable(const std::string &name, std::string &value) {
  bool error{false};
  std::string result(2048, ' ');
  char *buffer_ptr{result.data()};
  size_t buffer_size{result.length()};

  if (sysvar_reader_srv->get(nullptr, "GLOBAL", "mysql_server", name.c_str(),
                             reinterpret_cast<void **>(&buffer_ptr),
                             &buffer_size) != 0) {
    error = true;
    return error;
  }
  result.resize(buffer_size);
  value = result;
  return error;
}

/**
  Get server global status variable.
  @param[in]  name  variable name
  @param[out] value variable value
  @return false if successful, true otherwise
*/
bool Server::get_status_variable(const std::string &name, std::string &value) {
  bool error{true};
  std::string result(2048, ' ');
  char *buffer_ptr{result.data()};
  size_t const buffer_size{result.length()};
  my_h_string val{nullptr};
  CHARSET_INFO_h utf8mb4_h = charset_srv->get_utf8mb4();

  if (status_variable_reader_srv->get(nullptr, name.c_str(), true, &val) == 0 &&
      val != nullptr &&
      string_converter_srv->convert_to_buffer(val, buffer_ptr, buffer_size - 1,
                                              utf8mb4_h) == 0) {
    result.resize(buffer_size);
    value = result;
    error = false;
  }
  // Unable to get the value, do cleanup.
  if (val != nullptr) string_factory_srv->destroy(val);
  return error;
}

/**
  Get the value of OFFLINE_MODE.
  @param[in,out] error  Error code, 0 if successful
  @return true if OFFLINE_MODE = ON, false otherwise
*/
bool Server::get_offline_mode(bool &error) {
  std::string value;
  error = get_system_variable("OFFLINE_MODE", value);
  if (!error) {
    if (to_lower(value) == "on") return true;
  }
  return false;
}

/**
  OFFLINE_MODE message string.
  @return "OFFLINE_MODE = ON/OFF" or "" if error
*/
std::string Server::get_offline_mode_str() {
  bool error{false};
  std::string message, value_str;
  bool value = Server::get_offline_mode(error);
  if (!error) {
    message = "OFFLINE_MODE = ";
    value_str = value ? "ON" : "OFF";
    message += value_str;
  }
  return message;
}

/**
  Set OFFLINE_MODE = ON.
  @return false if successful, true otherwise
*/
bool Server::set_offline_mode() {
  assert(!is_group_replication_secondary());
  return set_system_variable("OFFLINE_MODE", "ON");
}

/**
  Clear OFFLINE_MODE, set to OFF.
  @return false if successful, true otherwise
*/
bool Server::clear_offline_mode() {
  return set_system_variable("OFFLINE_MODE", "OFF");
}

/**
  Get the value of SUPER_READ_ONLY.
  @param[in,out] error  Error code, 0 if successful
  @return true if SUPER_READ_ONLY = ON, false otherwise
*/
bool Server::get_super_read_only(bool &error) {
  std::string value;
#ifdef DEBUG_SWITCHOVER_LAG
  if (m_debug_sro && m_debug_sro_new_primary) {
    log_info("Simulating failover lag: SUPER_READ_ONLY = %s",
             on_off(m_debug_sro_value).c_str());
    if (m_debug_sro_count++ <= m_debug_sro_max_attempts) {
      return m_debug_sro_value;
    }
    m_debug_sro_count = 0;
    m_debug_sro_new_primary = false;
  }
#endif
  error = get_system_variable("SUPER_READ_ONLY", value);
  if (!error) {
    if (to_lower(value) == "on") return true;
  }
  return false;
}

/**
  SUPER_READ_ONLY message string.
  @return "SUPER_READ_ONLY = ON/OFF" or "" if error
*/
std::string Server::get_super_read_only_str() {
  bool error{false};
  std::string message, value_str;
  bool value = Server::get_super_read_only(error);
  if (!error) {
    message = "SUPER_READ_ONLY = ";
    value_str = value ? "ON" : "OFF";
    message += value_str;
  }
  return message;
}

/**
  Set SUPER_READ_ONLY = ON.
  Setting SUPER_READ_ONLY = ON also sets READ_ONLY = ON.
  @return false if successful, true otherwise
*/
bool Server::set_super_read_only() {
  return set_system_variable("SUPER_READ_ONLY", "ON");
}

/**
  Clear SUPER_READ_ONLY, set to OFF.
  @return false if successful, true otherwise
*/
bool Server::clear_super_read_only() {
  // Setting READ_ONLY = OFF also sets SUPER_READ_ONLY = OFF
  return clear_read_only();
}

/**
  Get the value of READ_ONLY.
  @return true if READ_ONLY = ON, false otherwise
*/
bool Server::get_read_only(bool &error) {
  std::string value;
  error = get_system_variable("READ_ONLY", value);
  if (!error) {
    if (to_lower(value) == "on") return true;
  }
  return false;
}

/**
  Clear READ_ONLY, set to OFF.
  Setting READ_ONLY = OFF also sets SUPER_READ_ONLY = OFF.
  @return false if successful, true otherwise
*/
bool Server::clear_read_only() {
  if (is_group_replication_secondary()) {
    log_info(
        "Not clearing SUPER_READ_ONLY because the instance is a "
        "secondary");
    return false;
  }
  return set_system_variable("READ_ONLY", "OFF");
}

/**
  Get the value of LOG_DIAGNOSTIC_ENABLE.
  @return true if LOG_DIAGNOSTIC_ENABLE = ON, false otherwise
*/
bool Server::get_log_diagnostic_enable(bool &error) {
  std::string value;
  error = get_system_variable("LOG_DIAGNOSTIC_ENABLE", value);
  if (!error) {
    if (to_lower(value) == "on") return true;
  }
  return false;
}

/**
  Convert string to lower case.
  @param str String to convert
  @return Converted string
*/
std::string &Server::to_lower(std::string &str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  return str;
}

// clang-format on

}  // namespace hm
