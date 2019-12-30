/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
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

#include "plugin/group_replication/include/plugin_handlers/remote_clone_handler.h"
#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/plugin.h"

[[noreturn]] void *Remote_clone_handler::launch_thread(void *arg) {
  Remote_clone_handler *thd = static_cast<Remote_clone_handler *>(arg);
  thd->clone_thread_handle();
}

Remote_clone_handler::Remote_clone_handler(ulonglong threshold,
                                           ulong components_stop_timeout)
    : m_group_name(""),
      m_view_id(""),
      m_clone_thd(nullptr),
      m_being_terminated(false),
      m_clone_query_status(CLONE_QUERY_NOT_EXECUTING),
      m_clone_query_session_id(0),
      m_clone_activation_threshold(threshold),
      m_current_donor_address(nullptr),
      m_stop_wait_timeout(components_stop_timeout) {
  mysql_mutex_init(key_GR_LOCK_clone_handler_run, &m_run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_clone_handler_run, &m_run_cond);
  mysql_mutex_init(key_GR_LOCK_clone_donor_list, &m_donor_list_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_clone_query, &m_clone_query_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_clone_read_mode, &m_clone_read_mode_lock,
                   MY_MUTEX_INIT_FAST);
}

Remote_clone_handler::~Remote_clone_handler() {
  delete m_current_donor_address;
  auto member_it = m_suitable_donors.begin();
  while (member_it != m_suitable_donors.end()) {
    delete (*member_it);
    member_it = m_suitable_donors.erase(member_it);
  }

  mysql_mutex_destroy(&m_run_lock);
  mysql_cond_destroy(&m_run_cond);
  mysql_mutex_destroy(&m_donor_list_lock);
  mysql_mutex_destroy(&m_clone_query_lock);
  mysql_mutex_destroy(&m_clone_read_mode_lock);
}

int Remote_clone_handler::after_view_change(
    const std::vector<Gcs_member_identifier> &,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &, bool is_leaving,
    bool *skip_election, enum_primary_election_mode *, std::string &) {
  *skip_election = false;

  bool donor_left = false;
  mysql_mutex_lock(&m_donor_list_lock);
  for (const Gcs_member_identifier &member_identifier : leaving) {
    if (m_current_donor_address &&
        member_identifier == *m_current_donor_address)
      donor_left = true;

    auto member_it = m_suitable_donors.begin();
    while (member_it != m_suitable_donors.end()) {
      if ((*member_it)->get_gcs_member_id() == member_identifier) {
        delete (*member_it);
        member_it = m_suitable_donors.erase(member_it);
      } else
        ++member_it;
    }
  }

  if (!is_leaving && donor_left) {
    kill_clone_query();
  }

  mysql_mutex_unlock(&m_donor_list_lock);

  return 0;
}

int Remote_clone_handler::after_primary_election(std::string, bool,
                                                 enum_primary_election_mode,
                                                 int) {
  return 0;
}

int Remote_clone_handler::before_message_handling(const Plugin_gcs_message &,
                                                  const std::string &, bool *) {
  return 0;
}

Remote_clone_handler::enum_clone_presence_query_result
Remote_clone_handler::check_clone_plugin_presence() {
  enum_clone_presence_query_result result = CLONE_CHECK_QUERY_ERROR;
  Sql_service_command_interface *sql_command_interface =
      new Sql_service_command_interface();
  if (sql_command_interface->establish_session_connection(
          PSESSION_DEDICATED_THREAD, GROUPREPL_USER, get_plugin_pointer())) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CONN_INTERNAL_PLUGIN_FAIL);
    delete sql_command_interface;
    return result;
    /* purecov: end */
  }

  std::string conditional_query =
      "SELECT COUNT(*)=1 FROM information_schema.plugins WHERE plugin_name = "
      "\'clone\' AND plugin_status = \'ACTIVE\';";
  bool is_present = false;
  std::string error_msg;
  long error = sql_command_interface->execute_conditional_query(
      conditional_query, &is_present, error_msg);
  if (!error) {
    result = CLONE_PLUGIN_NOT_PRESENT;
    if (is_present) result = CLONE_PLUGIN_PRESENT;
  } else {
    /* purecov: begin inspected */
    std::string err_msg("Error while checking the clone plugin status: ");
    if (!error_msg.empty()) err_msg.append(" " + error_msg);
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CLONE_PROCESS_PREPARE_ERROR,
                 err_msg.c_str());
    /* purecov: end */
  }

  delete sql_command_interface;
  return result;
}

int Remote_clone_handler::extract_donor_info(
    std::tuple<uint, uint, uint, ulonglong> *donor_info) {
  int error = 0;

  uint valid_clone_donors = 0;
  uint valid_recovery_donors = 0;
  uint valid_recovering_donors = 0;
  ulonglong number_gtids_missing = 0;

  std::vector<Group_member_info *> *all_members_info =
      group_member_mgr->get_all_members();

  Sid_map local_sid_map(NULL);
  Sid_map group_sid_map(NULL);
  Gtid_set local_member_set(&local_sid_map, NULL);
  Gtid_set group_set(&group_sid_map, NULL);
  Sid_map purged_sid_map(NULL);
  Gtid_set purged_set(&purged_sid_map, NULL);

  if (local_member_set.add_gtid_text(
          local_member_info->get_gtid_executed().c_str()) != RETURN_STATUS_OK ||
      local_member_set.add_gtid_text(
          local_member_info->get_gtid_retrieved().c_str()) !=
          RETURN_STATUS_OK) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_LOCAL_GTID_SETS_PROCESS_ERROR);
    error = 1;
    goto cleaning;
    /* purecov: end */
  }

  for (Group_member_info *member : *all_members_info) {
    std::string m_uuid = member->get_uuid();
    bool is_online =
        member->get_recovery_status() == Group_member_info::MEMBER_ONLINE;
    bool not_self = m_uuid.compare(local_member_info->get_uuid());
    // We can only clone from our own version.
    bool supports_clone =
        member->get_member_version().get_version() >=
            CLONE_GR_SUPPORT_VERSION &&
        member->get_member_version().get_version() ==
            local_member_info->get_member_version().get_version();

    std::string member_exec_set_str = member->get_gtid_executed();
    std::string applier_ret_set_str = member->get_gtid_retrieved();

    if (is_online) {
      if (not_self) {
        // Check if it support cloning looking at the server version
        if (supports_clone) valid_clone_donors++;

        if (group_set.add_gtid_text(member_exec_set_str.c_str()) !=
                RETURN_STATUS_OK ||
            group_set.add_gtid_text(applier_ret_set_str.c_str()) !=
                RETURN_STATUS_OK) {
          /* purecov: begin inspected */
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_LOCAL_GTID_SETS_PROCESS_ERROR);
          error = 1;
          goto cleaning;
          /* purecov: end */
        }
      }
    }
  }

  // Calculate the number of missing gtids
  group_set.remove_gtid_set(&local_member_set);
  number_gtids_missing = group_set.get_gtid_number();

  // Before deciding calculate also the number of valid recovery donors
  for (Group_member_info *member : *all_members_info) {
    std::string member_purged_set_str = member->get_gtid_purged();

    std::string m_uuid = member->get_uuid();
    bool is_online =
        member->get_recovery_status() == Group_member_info::MEMBER_ONLINE;
    bool is_recovering =
        member->get_recovery_status() == Group_member_info::MEMBER_IN_RECOVERY;
    bool not_self = m_uuid.compare(local_member_info->get_uuid());

    std::string member_exec_set_str = member->get_gtid_executed();
    std::string applier_ret_set_str = member->get_gtid_retrieved();

    if (not_self) {
      if (is_online || is_recovering) {
        purged_set.clear_set_and_sid_map();
        if (purged_set.add_gtid_text(member_purged_set_str.c_str()) !=
            RETURN_STATUS_OK) {
          /* purecov: begin inspected */
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_LOCAL_GTID_SETS_PROCESS_ERROR);
          error = 1;
          goto cleaning;
          /* purecov: end */
        }
        // Are some of missing GTIDS already purged in this server?
        if (!group_set.is_intersection_nonempty(&purged_set)) {
          if (is_online) {
            valid_recovery_donors++;
          } else if (is_recovering) {
            valid_recovering_donors++;
          }
        }
      }
    }
  }

cleaning:

  std::get<0>(*donor_info) = valid_clone_donors;
  std::get<1>(*donor_info) = valid_recovery_donors;
  std::get<2>(*donor_info) = valid_recovering_donors;
  std::get<3>(*donor_info) = number_gtids_missing;

  // clean the members
  for (Group_member_info *member : *all_members_info) {
    delete member;
  }
  delete all_members_info;

  return error;
}

Remote_clone_handler::enum_clone_check_result
Remote_clone_handler::check_clone_preconditions() {
  Remote_clone_handler::enum_clone_check_result result = NO_RECOVERY_POSSIBLE;

  std::tuple<uint, uint, uint, ulonglong> donor_info(0, 0, 0, 0);
  if (extract_donor_info(&donor_info)) {
    return CHECK_ERROR; /* purecov: inspected */
  }

  uint valid_clone_donors = std::get<0>(donor_info);
  uint valid_recovery_donors = std::get<1>(donor_info);
  uint valid_recovering_donors = std::get<2>(donor_info);
  ulonglong number_gtids_missing = std::get<3>(donor_info);
  ulonglong threshold = m_clone_activation_threshold;

  if (number_gtids_missing >= threshold && valid_clone_donors > 0) {
    result = DO_CLONE;
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_RECOVERY_STRAT_CLONE_THRESHOLD,
                 threshold);
    goto end;
  }

  if (valid_recovery_donors == 0 && valid_clone_donors > 0) {
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_RECOVERY_STRAT_CLONE_PURGED);
    result = DO_CLONE;
  }

  if (valid_recovery_donors > 0) result = DO_RECOVERY;

  /*
    If we only have RECOVERING members, lets allow recovery strategy,
    this may be during a full group start on which situation eventually
    all will be ONLINE, the joiner will retry recovery until it exhausts
    group_replication_recovery_retry_count.
  */
  if (valid_recovery_donors == 0 && valid_clone_donors == 0 &&
      valid_recovering_donors > 0) {
    result = DO_RECOVERY;
  }

end:

  /*Due to the costs of opening a connection we only check this here*/
  if (DO_CLONE == result) {
    enum_clone_presence_query_result plugin_presence =
        check_clone_plugin_presence();
    bool check_error = false;
    if (CLONE_CHECK_QUERY_ERROR == plugin_presence) {
      /* purecov: begin inspected */
      // When there is an error checking try distributing recovery if possible
      plugin_presence = CLONE_PLUGIN_NOT_PRESENT;
      check_error = true;
      /* purecov: end */
    }
    if (CLONE_PLUGIN_NOT_PRESENT == plugin_presence) {
      if (!check_error)
        LogPluginErr(
            ERROR_LEVEL, ER_GRP_RPL_CLONE_PROCESS_PREPARE_ERROR,
            "The clone plugin is not present or active in this server.");

      if (valid_recovery_donors > 0 || valid_recovering_donors > 0) {
        result = DO_RECOVERY;
      } else {
        result = NO_RECOVERY_POSSIBLE;
      }
    }
  }

  return result;
}

void Remote_clone_handler::get_clone_donors(
    std::list<Group_member_info *> &suitable_donors) {
  std::vector<Group_member_info *> *all_members_info =
      group_member_mgr->get_all_members();
  if (all_members_info->size() > 1) {
    std::random_shuffle(all_members_info->begin(), all_members_info->end());
  }

  for (Group_member_info *member : *all_members_info) {
    std::string m_uuid = member->get_uuid();
    bool is_online =
        member->get_recovery_status() == Group_member_info::MEMBER_ONLINE;
    bool not_self = m_uuid.compare(local_member_info->get_uuid());
    bool supports_clone =
        member->get_member_version().get_version() >=
            CLONE_GR_SUPPORT_VERSION &&
        member->get_member_version().get_version() ==
            local_member_info->get_member_version().get_version();

    if (is_online && not_self && supports_clone) {
      suitable_donors.push_back(member);
    } else {
      delete member;
    }
  }

  delete all_members_info;
}

int Remote_clone_handler::set_clone_ssl_options(
    Sql_service_command_interface *sql_command_interface) {
  std::string ssl_ca, ssl_cert, ssl_key;
  recovery_module->get_recovery_base_ssl_options(&ssl_ca, &ssl_cert, &ssl_key);

  int error = 0;

  if (!ssl_ca.empty()) {
    std::string ssl_ca_query = " SET GLOBAL clone_ssl_ca = \'";
    ssl_ca_query.append(ssl_ca);
    ssl_ca_query.append("\'");
    error = sql_command_interface->execute_query(ssl_ca_query);
  }
  if (!error && !ssl_cert.empty()) {
    std::string ssl_cert_query = " SET GLOBAL clone_ssl_cert = \'";
    ssl_cert_query.append(ssl_cert);
    ssl_cert_query.append("\'");
    error = sql_command_interface->execute_query(ssl_cert_query);
  }
  if (!error && !ssl_key.empty()) {
    std::string ssl_key_query = " SET GLOBAL clone_ssl_key = \'";
    ssl_key_query.append(ssl_key);
    ssl_key_query.append("\'");
    error = sql_command_interface->execute_query(ssl_key_query);
  }
  return error;
}

int Remote_clone_handler::fallback_to_recovery_or_leave(
    Sql_service_command_interface *sql_command_interface, bool critical_error) {
  // Do nothing if the server is shutting down.
  // The stop process will leave the group
  if (get_server_shutdown_status()) return 0;

  // If it failed to (re)connect to the server or the set read only query
  if (!sql_command_interface->is_session_valid() ||
      sql_command_interface->set_super_read_only()) {
    abort_plugin_process(
        "Cannot re-enable the super read only after clone failure.");
    return 1;
  }

  /**
   Before falling back to recovery check if there are valid donors
   Since cloning can be time consuming valid members may have left
   or joined in the meanwhile.
  */
  std::tuple<uint, uint, uint, ulonglong> donor_info(0, 0, 0, 0);
  if (extract_donor_info(&donor_info)) {
    critical_error = true; /* purecov: inspected */
  } else {
    uint valid_recovery_donors = std::get<1>(donor_info);
    uint valid_recovering_donors = std::get<2>(donor_info);
    uint valid_donors = valid_recovery_donors + valid_recovering_donors;
    if (valid_donors == 0) critical_error = true;
  }

  if (!critical_error) {
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_RECOVERY_STRAT_FALLBACK,
                 "Incremental Recovery.");
    recovery_module->start_recovery(this->m_group_name, this->m_view_id);
    return 0;
  } else {
    const char *exit_state_action_abort_log_message =
        "Fatal error while Group Replication was provisoning with Clone.";
    leave_group_on_failure::mask leave_actions;
    leave_actions.set(leave_group_on_failure::SKIP_SET_READ_ONLY, true);
    leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
    leave_group_on_failure::leave(
        leave_actions, ER_GRP_RPL_RECOVERY_STRAT_NO_FALLBACK,
        PSESSION_INIT_THREAD, nullptr, exit_state_action_abort_log_message);
    return 1;
  }
}

int Remote_clone_handler::update_donor_list(
    Sql_service_command_interface *sql_command_interface, std::string &hostname,
    std::string &port) {
  std::string donor_list_query = " SET GLOBAL clone_valid_donor_list = \'";

  // Escape possible weird hostnames
  plugin_escape_string(hostname);

  donor_list_query.append(hostname);
  donor_list_query.append(":");
  donor_list_query.append(port);
  donor_list_query.append("\'");

  std::string error_msg;
  if (sql_command_interface->execute_query(donor_list_query, error_msg)) {
    /* purecov: begin inspected */
    std::string err_msg("Error while updating the clone donor list.");
    if (!error_msg.empty()) err_msg.append(" " + error_msg);
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CLONE_PROCESS_PREPARE_ERROR,
                 err_msg.c_str());
    return 1;
    /* purecov: end */
  }
  return 0;
}

int Remote_clone_handler::run_clone_query(
    Sql_service_command_interface *sql_command_interface, std::string &hostname,
    std::string &port, std::string &username, std::string &password,
    bool use_ssl) {
  int error = 0;
  mysql_mutex_lock(&m_clone_query_lock);
  m_clone_query_session_id =
      sql_command_interface->get_sql_service_interface()->get_session_id();
  m_clone_query_status = CLONE_QUERY_EXECUTING;
  mysql_mutex_unlock(&m_clone_query_lock);

  if (!m_being_terminated) {
    std::string error_msg;
    if ((error = sql_command_interface->clone_server(
             hostname, port, username, password, use_ssl, error_msg))) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CLONE_PROCESS_EXEC_ERROR,
                   error_msg.c_str());
    }
  }

  mysql_mutex_lock(&m_clone_query_lock);
  m_clone_query_status = CLONE_QUERY_EXECUTED;
  mysql_mutex_unlock(&m_clone_query_lock);

  return error;
}

int Remote_clone_handler::kill_clone_query() {
  int error = 0;

  mysql_mutex_lock(&m_clone_query_lock);

  if (m_clone_query_status == CLONE_QUERY_EXECUTING) {
    DBUG_ASSERT(m_clone_query_session_id != 0);
    Sql_service_command_interface *sql_command_interface =
        new Sql_service_command_interface();
    error = sql_command_interface->establish_session_connection(
        PSESSION_DEDICATED_THREAD, GROUPREPL_USER, get_plugin_pointer());
    if (!error) {
      error = sql_command_interface->kill_session(m_clone_query_session_id);
      // If the thread is no longer there don't report an warning
      /* purecov: begin inspected */
      if (ER_NO_SUCH_THREAD == error) {
        error = 0;
      } else if (error) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CANT_KILL_THREAD,
                     "the cloning process",
                     "The termination process will wait for the process to "
                     "end.");
      }
      /* purecov: end */
    }
    delete sql_command_interface;
  }

  mysql_mutex_unlock(&m_clone_query_lock);

  return error != 0;
}

int Remote_clone_handler::evaluate_server_connection(
    Sql_service_command_interface *sql_command_interface) {
  if (sql_command_interface->is_session_killed()) {
    if (sql_command_interface->reestablish_connection(
            PSESSION_DEDICATED_THREAD, GROUPREPL_USER, get_plugin_pointer())) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CONN_INTERNAL_PLUGIN_FAIL);
      return 1;
      /* purecov: end */
    }
  }
  return 0;
}

void Remote_clone_handler::terminate_clone_process(bool rejoin) {
  mysql_mutex_lock(&m_run_lock);

  m_being_terminated = true;

  if (!rejoin) kill_clone_query();

  while (m_clone_process_thd_state.is_thread_alive()) {
    mysql_mutex_lock(&m_clone_thd->LOCK_thd_data);
    m_clone_thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&m_clone_thd->LOCK_thd_data);

    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
  }

  mysql_mutex_unlock(&m_run_lock);
}

int Remote_clone_handler::clone_server(const std::string &group_name,
                                       const std::string &view_id) {
  DBUG_ENTER("Remote_clone_handler::clone_server");
  int ret = 0;

  mysql_mutex_lock(&m_run_lock);

  if (m_clone_process_thd_state.is_thread_alive()) goto end;

  get_clone_donors(m_suitable_donors);

  if (m_suitable_donors.empty()) {
    /* purecov: begin inspected */
    ret = 1;
    goto end;
    /* purecov: end */
  }

  m_being_terminated = false;
  m_group_name.assign(group_name);
  m_view_id.assign(view_id);
  group_events_observation_manager->register_group_event_observer(this);

  /*
    Attempt to create a MySQL instrumented thread. Return an error if not
    possible.
  */
  if (mysql_thread_create(
          key_GR_THD_clone_thd, &m_thd_handle, get_connection_attrib(),
          Remote_clone_handler::launch_thread, static_cast<void *>(this))) {
    /* purecov: begin inspected */
    m_clone_process_thd_state.set_terminated();
    group_events_observation_manager->unregister_group_event_observer(this);
    ret = 1;
    goto end;
    /* purecov: end */
  }
  m_clone_process_thd_state.set_created();

  /*
    Wait until the thread actually starts.
  */
  while (m_clone_process_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep", ("Waiting for the clone process thread to start"));
    mysql_cond_wait(&m_run_cond, &m_run_lock);
  }

end:
  mysql_mutex_unlock(&m_run_lock);
  DBUG_RETURN(ret);
}

#ifndef DBUG_OFF
void Remote_clone_handler::gr_clone_debug_point() {
  DBUG_EXECUTE_IF("gr_clone_process_before_execution", {
    const char act[] =
        "now signal signal.gr_clone_thd_paused wait_for "
        "signal.gr_clone_thd_continue";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
}
#endif /* DBUG_OFF */

[[noreturn]] void Remote_clone_handler::clone_thread_handle() {
  int error = 0;
  bool use_ssl = false;
  std::string username;
  std::string password;
  Replication_thread_api recovery_channel("group_replication_recovery");
  bool empty_donor_list = false;
  bool critical_error = false;
  int number_attempts = 0;
  int number_servers = 0;

  // Initialize the MySQL thread infrastructure.
  m_clone_thd = new THD;
  my_thread_init();
  m_clone_thd->set_new_thread_id();
  m_clone_thd->thread_stack = reinterpret_cast<const char *>(&m_clone_thd);
  m_clone_thd->store_globals();
  global_thd_manager_add_thd(m_clone_thd);

  Plugin_stage_monitor_handler stage_handler;
  if (stage_handler.initialize_stage_monitor()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_NO_STAGE_SERVICE);
    /* purecov: end */
  }

  // declare the thread as running

  mysql_mutex_lock(&m_run_lock);
  m_clone_process_thd_state.set_running();
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  stage_handler.set_stage(info_GR_STAGE_clone_prepare.m_key, __FILE__, __LINE__,
                          0, 0);

  Sql_service_command_interface *sql_command_interface =
      new Sql_service_command_interface();
  if (sql_command_interface->establish_session_connection(
          PSESSION_DEDICATED_THREAD, GROUPREPL_USER, get_plugin_pointer())) {
    /* purecov: begin inspected */
    LogPluginErr(
        ERROR_LEVEL, ER_GRP_RPL_CLONE_PROCESS_PREPARE_ERROR,
        "The plugin can't establish an internal connection to the server.");
    error = 1;
    goto thd_end;
    /* purecov: end */
  }

  if (m_being_terminated) goto thd_end;

  // This lock should prevent current threads of setting the read mode again
  mysql_mutex_lock(&m_clone_read_mode_lock);

  declare_plugin_cloning(true);

  /* The clone operation does not work with read mode so we have to disable it
   * here */
  if (sql_command_interface->reset_read_only()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CLONE_PROCESS_PREPARE_ERROR,
                 "Could not disable the server read only mode for cloning.");
    error = 1;
    mysql_mutex_unlock(&m_clone_read_mode_lock);
    goto thd_end;
    /* purecov: end */
  }
  mysql_mutex_unlock(&m_clone_read_mode_lock);

  /* The clone credentials are the ones from recovery */
  if (recovery_channel.get_channel_credentials(username, password)) {
    /* purecov: begin inspected */
    LogPluginErr(
        ERROR_LEVEL, ER_GRP_RPL_CLONE_PROCESS_PREPARE_ERROR,
        "Could not extract the access credentials for the clone process");
    error = 1;
    goto thd_end;
    /* purecov: end */
  }

  if (m_being_terminated) goto thd_end;

  /* If using SSL, we need to configure the SSL options */
  use_ssl = recovery_module->get_recovery_use_ssl();
  if (use_ssl)
    if (set_clone_ssl_options(sql_command_interface)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CLONE_PROCESS_PREPARE_ERROR,
                   "The plugin could not configure the SSL options for the "
                   "clone process.");
      error = 1;
      goto thd_end;
      /* purecov: end */
    }

  number_attempts = 1;
  mysql_mutex_lock(&m_donor_list_lock);
  number_servers = m_suitable_donors.size();
  mysql_mutex_unlock(&m_donor_list_lock);

  stage_handler.set_stage(info_GR_STAGE_clone_execute.m_key, __FILE__, __LINE__,
                          number_servers, number_attempts);

#ifndef DBUG_OFF
  gr_clone_debug_point();
#endif /* DBUG_OFF */

  while (!empty_donor_list && !m_being_terminated) {
    stage_handler.set_completed_work(number_attempts);
    number_attempts++;

    std::string hostname("");
    std::string port("");

    mysql_mutex_lock(&m_donor_list_lock);
    empty_donor_list = m_suitable_donors.empty();
    if (!empty_donor_list) {
      Group_member_info *member = m_suitable_donors.front();
      hostname.assign(member->get_hostname());
      port.assign(std::to_string(member->get_port()));
      // define the current donor
      delete m_current_donor_address;
      m_current_donor_address =
          new Gcs_member_identifier(member->get_gcs_member_id());

      m_suitable_donors.pop_front();
      delete member;
      empty_donor_list = m_suitable_donors.empty();
      number_servers = m_suitable_donors.size();
    }
    mysql_mutex_unlock(&m_donor_list_lock);

    // No valid donor remains in the list
    if (hostname.empty()) continue;

    /* Update the allowed donor list */
    if ((error = update_donor_list(sql_command_interface, hostname, port))) {
      break; /* purecov: inspected */
    }

    if (m_being_terminated) goto thd_end;

    terminate_wait_on_start_process(true);

    error = run_clone_query(sql_command_interface, hostname, port, username,
                            password, use_ssl);

    switch (error) {
      case ER_RESTART_SERVER_FAILED:
        critical_error = true;
        goto thd_end;
    }

    if (error && !m_being_terminated) {
      if (evaluate_server_connection(sql_command_interface)) {
        /* purecov: begin inspected */
        // This is a bad sign, might as well quit
        critical_error = true;
        goto thd_end;
        /* purecov: end */
      }

      /*
       If we reach this point and we are now alone it means no alternatives
       remain for recovery. A fallback to distributed recovery would be
       interpreted as a solo member recovery.
      */
      if (group_member_mgr->get_number_of_members() == 1) {
        critical_error = true;
        goto thd_end;
      }
    }

    // try till there is a clone command that succeeds
    if (!error) break;
  }

thd_end:

  declare_plugin_cloning(false);

  if (error && !m_being_terminated) {
    fallback_to_recovery_or_leave(sql_command_interface, critical_error);
  }

  delete sql_command_interface;

  group_events_observation_manager->unregister_group_event_observer(this);

  stage_handler.end_stage();
  stage_handler.terminate_stage_monitor();

  mysql_mutex_lock(&m_run_lock);
  m_clone_thd->release_resources();
  global_thd_manager_remove_thd(m_clone_thd);
  delete m_clone_thd;
  m_clone_thd = nullptr;
  m_clone_process_thd_state.set_terminated();
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  my_thread_end();
  my_thread_exit(nullptr);
}
