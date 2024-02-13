/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include <assert.h>
#include <mysql/group_replication_priv.h>
#include <time.h>

#include "my_dbug.h"
#include "my_systime.h"
#include "mysql/components/services/log_builtins.h"
#include "plugin/group_replication/include/compatibility_module.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/recovery_metadata.h"
#include "plugin/group_replication/include/plugin_psi.h"
#include "plugin/group_replication/include/plugin_server_include.h"
#include "plugin/group_replication/include/plugin_variables/recovery_endpoints.h"
#include "plugin/group_replication/include/recovery_channel_state_observer.h"
#include "plugin/group_replication/include/recovery_state_transfer.h"
#include "plugin/group_replication/include/services/system_variable/get_system_variable.h"
#include "string_with_len.h"

using std::string;

Recovery_state_transfer::Recovery_state_transfer(
    char *recovery_channel_name, const string &member_uuid,
    Channel_observation_manager *channel_obsr_mngr)
    : m_recovery_channel_name(recovery_channel_name),
      m_until_condition(CHANNEL_UNTIL_VIEW_ID),
      selected_donor(nullptr),
      group_members(nullptr),
      suitable_donors(
          Malloc_allocator<Group_member_info *>(key_group_member_info)),
      donor_connection_retry_count(0),
      recovery_aborted(false),
      donor_transfer_finished(false),
      connected_to_donor(false),
      on_failover(false),
      donor_connection_interface(recovery_channel_name),
      channel_observation_manager(channel_obsr_mngr),
      recovery_channel_observer(nullptr),
      recovery_use_ssl(false),
      recovery_get_public_key(false),
      recovery_ssl_verify_server_cert(false),
      recovery_tls_ciphersuites_null(true),
      max_connection_attempts_to_donors(0),
      donor_reconnect_interval(0) {
  // set the recovery SSL options to 0
  (void)strncpy(recovery_ssl_ca, "", 1);
  (void)strncpy(recovery_ssl_capath, "", 1);
  (void)strncpy(recovery_ssl_cert, "", 1);
  (void)strncpy(recovery_ssl_cipher, "", 1);
  (void)strncpy(recovery_ssl_key, "", 1);
  (void)strncpy(recovery_ssl_crl, "", 1);
  (void)strncpy(recovery_ssl_crlpath, "", 1);
  (void)strncpy(recovery_public_key_path, "", 1);
  (void)strncpy(recovery_tls_version, "", 1);
  (void)strncpy(recovery_tls_ciphersuites, "", 1);

  this->member_uuid = member_uuid;

  mysql_mutex_init(key_GR_LOCK_recovery, &recovery_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_recovery, &recovery_condition);
  mysql_mutex_init(key_GR_LOCK_recovery_donor_selection, &donor_selection_lock,
                   MY_MUTEX_INIT_FAST);

  recovery_channel_observer = new Recovery_channel_state_observer(this);
}

Recovery_state_transfer::~Recovery_state_transfer() {
  if (group_members != nullptr) {
    Group_member_info_list_iterator member_it = group_members->begin();
    while (member_it != group_members->end()) {
      delete (*member_it);
      ++member_it;
    }
  }
  delete group_members;
  delete recovery_channel_observer;
  delete selected_donor;
  selected_donor = nullptr;
  mysql_mutex_destroy(&recovery_lock);
  mysql_cond_destroy(&recovery_condition);
  mysql_mutex_destroy(&donor_selection_lock);
}

void Recovery_state_transfer::initialize(const string &view_id,
                                         bool is_vcle_enabled) {
  DBUG_TRACE;

  // reset the recovery aborted flag
  recovery_aborted = false;
  // reset the donor transfer ending flag
  donor_transfer_finished = false;
  // reset the failover flag
  on_failover = false;
  // reset the donor channel thread error flag
  donor_channel_thread_error = false;
  // reset the retry count
  donor_connection_retry_count = 0;
  this->view_id.assign(view_id);
  if (is_vcle_enabled) {
    m_until_condition = CHANNEL_UNTIL_VIEW_ID;
  } else {
    m_until_condition = CHANNEL_UNTIL_APPLIER_AFTER_GTIDS;
  }
}

void Recovery_state_transfer::set_until_condition_after_gtids(
    const std::string &after_gtids) {
  m_after_gtids.assign(after_gtids);
}

void Recovery_state_transfer::inform_of_applier_stop(my_thread_id thread_id,
                                                     bool aborted) {
  DBUG_TRACE;
  bool error{false};

  /*
    This method doesn't take any locks as it could lead to dead locks between
    the connection process and this method that can be invoked in that context.
    connection, no major concurrency issues should exist.
  */

  // Act if:
  if (!donor_transfer_finished &&  // we don't have all the data yet
      !recovery_aborted &&         // recovery was not aborted
      // the signal belongs to the recovery donor channel thread
      donor_connection_interface.is_own_event_applier(thread_id)) {
    /*
      if recovery is using CHANNEL_UNTIL_APPLIER_AFTER_GTIDS for start replica
      until condition then,
      - if aborted == true :
        which could be due to some error or sql thread than we should not call
        end_state_transfer() for successful completion, we should set error so
        that recovery_thd can error out.
      - else if aborted != true :
        then we call end_state_transfer() for successful completion.
    */
    if (m_until_condition == CHANNEL_UNTIL_APPLIER_AFTER_GTIDS) {
      if (aborted) {
        error = true;
      } else {
        DBUG_EXECUTE_IF(
            "group_replication_recovery_after_gtids_applier_stop_error_out",
            { error = true; });
        std::string local_gtid_executed_string;
        if (!error &&
            verify_member_has_after_gtids_present(local_gtid_executed_string)) {
          LogPluginErr(INFORMATION_LEVEL,
                       ER_GROUP_REPLICATION_RECOVERY_STOPPED_GTID_PRESENT,
                       m_after_gtids.c_str());
          end_state_transfer();
        } else {
          error = true;
        }
      }
    } else {
      error = true;
    }
  }

  if (error) {
    mysql_mutex_lock(&recovery_lock);
    donor_channel_thread_error = true;
    mysql_cond_broadcast(&recovery_condition);
    mysql_mutex_unlock(&recovery_lock);
  }
}

void Recovery_state_transfer::inform_of_receiver_stop(my_thread_id thread_id) {
  DBUG_TRACE;

  /*
    This method doesn't take any locks as it could lead to dead locks between
    the connection process and this method that can be invoked in that context.
    Since this only affects the recovery loop and the flag is reset at each
    connection, no major concurrency issues should exist.
  */

  // Act if:
  if (!donor_transfer_finished &&  // we don't have all the data yet
      !recovery_aborted &&         // recovery was not aborted
      // the signal belongs to the recovery donor channel thread
      donor_connection_interface.is_own_event_receiver(thread_id)) {
    mysql_mutex_lock(&recovery_lock);
    donor_channel_thread_error = true;
    mysql_cond_broadcast(&recovery_condition);
    mysql_mutex_unlock(&recovery_lock);
  }
}

void Recovery_state_transfer::initialize_group_info() {
  DBUG_TRACE;

  delete selected_donor;
  selected_donor = nullptr;
  selected_donor_hostname.clear();
  // Update the group member info
  mysql_mutex_lock(&donor_selection_lock);
  update_group_membership(false);
  mysql_mutex_unlock(&donor_selection_lock);
}

void Recovery_state_transfer::update_group_membership(bool update_donor) {
  DBUG_TRACE;

#ifndef NDEBUG
  mysql_mutex_assert_owner(&donor_selection_lock);
#endif

  // if needed update the reference to the donor member
  string donor_uuid;
  if (selected_donor != nullptr && update_donor) {
    donor_uuid.assign(selected_donor->get_uuid());
  }

  if (group_members != nullptr) {
    Group_member_info_list_iterator member_it = group_members->begin();
    while (member_it != group_members->end()) {
      delete (*member_it);
      ++member_it;
    }
  }
  delete group_members;

  group_members = group_member_mgr->get_all_members();

  // When updating the member list, also rebuild the suitable donor list
  build_donor_list(&donor_uuid);
}

void Recovery_state_transfer::abort_state_transfer() {
  DBUG_TRACE;

  // Break the wait for view change event
  mysql_mutex_lock(&recovery_lock);
  recovery_aborted = true;
  mysql_cond_broadcast(&recovery_condition);
  mysql_mutex_unlock(&recovery_lock);
}

int Recovery_state_transfer::update_recovery_process(bool did_members_left) {
  DBUG_TRACE;
  int error = 0;

  /*
    Lock to avoid concurrency between this code that handles failover and
    the establish_donor_connection method. We either:
    1) lock first and see that the method did not run yet, updating the list
       of group members that will be used there.
    2) lock after the method executed, and if the selected donor is leaving
       we stop the connection thread and select a new one.
  */
  mysql_mutex_lock(&donor_selection_lock);

  bool donor_left = false;
  string current_donor_uuid;
  string current_donor_hostname;
  uint current_donor_port = 0;
  /*
    The selected donor can be NULL if:
    * The donor was not yet chosen
     or
    * Was deleted in a previous group updated, but there was no need to
      select a new one since as the data transfer is finished
  */
  if (selected_donor != nullptr && did_members_left) {
    current_donor_uuid.assign(selected_donor->get_uuid());
    current_donor_hostname.assign(selected_donor->get_hostname());
    current_donor_port = selected_donor->get_port();
    donor_left = !group_member_mgr->is_member_info_present(current_donor_uuid);
  }

  /*
    Get updated information about the new group members.
  */
  update_group_membership(!donor_left);

  /*
    It makes sense to cut our connection to the donor if:
    1) The donor has left the building
    and
    2) We are already connected to him.
  */
  if (donor_left) {
    // The selected donor no longer holds a meaning after deleting the group
    delete selected_donor;
    selected_donor = nullptr;
    if (connected_to_donor) {
      /*
       The donor_transfer_finished flag is not lock protected on the recovery
       thread so we have the scenarios.
       1) The flag is true and we do nothing
       2) The flag is false and remains false so we restart the connection, and
       that new connection will deliver the rest of the data
       3) The flag turns true while we are restarting the connection. In this
       case we will probably create a new connection that won't be needed and
       will be terminated the instant the lock is freed.
      */
      if (!donor_transfer_finished) {
        LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_MEMBER_NOT_FOUND,
                     current_donor_hostname.c_str(), current_donor_port);

        // Awake the recovery loop to connect to another donor
        donor_failover();
      }  // else do nothing
    }
  }
  mysql_mutex_unlock(&donor_selection_lock);

  return error;
}

void Recovery_state_transfer::end_state_transfer() {
  DBUG_TRACE;

  mysql_mutex_lock(&recovery_lock);
  donor_transfer_finished = true;
  mysql_cond_broadcast(&recovery_condition);
  mysql_mutex_unlock(&recovery_lock);
}

void Recovery_state_transfer::donor_failover() {
  DBUG_TRACE;

  // Awake the recovery process so it can loop again to connect to another donor
  mysql_mutex_lock(&recovery_lock);
  on_failover = true;
  mysql_cond_broadcast(&recovery_condition);
  mysql_mutex_unlock(&recovery_lock);
}

int Recovery_state_transfer::check_recovery_thread_status() {
  DBUG_TRACE;

  // if some of the threads are running
  if (donor_connection_interface.is_receiver_thread_running() ||
      donor_connection_interface.is_applier_thread_running()) {
    return terminate_recovery_slave_threads() != STATE_TRANSFER_OK;
  }
  return 0;
}

bool Recovery_state_transfer::is_own_event_channel(my_thread_id id) {
  DBUG_TRACE;
  return donor_connection_interface.is_own_event_applier(id);
}

void Recovery_state_transfer::build_donor_list(string *selected_donor_uuid) {
  DBUG_TRACE;

  suitable_donors.clear();

  Group_member_info_list_iterator member_it = group_members->begin();

  while (member_it != group_members->end()) {
    Group_member_info *member = *member_it;
    const string m_uuid(member->get_uuid());
    const bool is_online =
        member->get_recovery_status() == Group_member_info::MEMBER_ONLINE;
    const bool not_self = m_uuid.compare(member_uuid);
    bool valid_donor = false;

    // is online and it's not me
    if (is_online && not_self) {
      const Member_version local_member_version =
          local_member_info->get_member_version();
      const Member_version donor_member_version = member->get_member_version();
      std::set<Member_version> local_and_donor_member_versions;
      local_and_donor_member_versions.insert(local_member_version);
      local_and_donor_member_versions.insert(donor_member_version);

      if (donor_member_version <= local_member_version) {
        suitable_donors.push_back(member);
        valid_donor = true;
      } else if (Compatibility_module::do_all_versions_belong_to_the_same_lts(
                     local_and_donor_member_versions)) {
        suitable_donors.push_back(member);
        valid_donor = true;
      } else if (get_allow_local_lower_version_join()) {
        suitable_donors.push_back(member);
        valid_donor = true;
      }
    }

    // if requested, and if the donor is still in the group, update its
    // reference
    if (selected_donor_uuid != nullptr &&
        !m_uuid.compare(*selected_donor_uuid) && valid_donor) {
      if (selected_donor != nullptr) {
        selected_donor->update(*member);
      } else {
        selected_donor = new Group_member_info(*member);
      }
    }

    ++member_it;
  }

  if (suitable_donors.size() > 1) {
    vector_random_shuffle(&suitable_donors);
  }

  // no need for errors if no donors exist, we thrown it in the connection
  // method.
}

int Recovery_state_transfer::establish_donor_connection() {
  DBUG_TRACE;

  int error = -1;
  connected_to_donor = false;

  while (error != 0 && !recovery_aborted) {
    mysql_mutex_lock(&donor_selection_lock);

    DBUG_EXECUTE_IF("gr_reset_max_connection_attempts_to_donors", {
      if (donor_connection_retry_count == 3) {
        const char act[] =
            "now signal signal.connection_attempt_3 wait_for "
            "signal.reset_recovery_retry_count_done";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      }
    };);
    // max number of retries reached, abort
    if (donor_connection_retry_count >= max_connection_attempts_to_donors) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MAXIMUM_CONNECTION_RETRIES_REACHED);
      mysql_mutex_unlock(&donor_selection_lock);
      return error;
    }

    if (group_member_mgr->get_number_of_members() == 1) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ALL_DONORS_LEFT_ABORT_RECOVERY);
      mysql_mutex_unlock(&donor_selection_lock);
      return error;
    }

    if (donor_connection_retry_count == 0) {
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_ESTABLISH_RECOVERY_WITH_DONOR,
                   donor_connection_retry_count + 1,
                   max_connection_attempts_to_donors);
    } else {
      LogPluginErr(
          INFORMATION_LEVEL, ER_GRP_RPL_ESTABLISH_RECOVERY_WITH_ANOTHER_DONOR,
          donor_connection_retry_count + 1, max_connection_attempts_to_donors);
    }

    // Rebuild the list, if empty
    if (suitable_donors.empty()) {
      mysql_mutex_unlock(&donor_selection_lock);

      struct timespec abstime;
      set_timespec(&abstime, donor_reconnect_interval);

      mysql_mutex_lock(&recovery_lock);
      mysql_cond_timedwait(&recovery_condition, &recovery_lock, &abstime);
      mysql_mutex_unlock(&recovery_lock);

      mysql_mutex_lock(&donor_selection_lock);

      build_donor_list(nullptr);
      if (suitable_donors.empty()) {
        LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_NO_VALID_DONOR);
        donor_connection_retry_count++;
        mysql_mutex_unlock(&donor_selection_lock);
        continue;
      }
    }

    donor_channel_thread_error = false;

    // Get the last element and delete it
    if (selected_donor != nullptr) {
      selected_donor->update(*suitable_donors.back());
    } else {
      selected_donor = new Group_member_info(*suitable_donors.back());
    }
    suitable_donors.pop_back();
    // increment the number of tries
    donor_connection_retry_count++;

    Donor_recovery_endpoints donor_endpoints;
    std::vector<std::pair<std::string, uint>> endpoints;
    endpoints = donor_endpoints.get_endpoints(selected_donor);

    for (auto endpoint : endpoints) {
      auto hostname = endpoint.first;
      uint port = endpoint.second;
      if ((error = initialize_donor_connection(hostname, port))) {
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_CONFIG_RECOVERY); /* purecov: inspected */
      }

      if (!error && !recovery_aborted) {
        error = start_recovery_donor_threads();
      }

      if (!error) {
        break;
      }
    }

    if (!error) {
      connected_to_donor = true;
      // if were on failover, now we are again connected to a valid server.
      on_failover = false;
    }

    mysql_mutex_unlock(&donor_selection_lock);

    /*
      sleep so other method (recovery) can get some time
      to grab the lock and update the group.
    */
    my_sleep(100);
  }

  return error;
}

int Recovery_state_transfer::initialize_donor_connection(std::string hostname,
                                                         uint port) {
  DBUG_TRACE;

  int error = 0;

  donor_connection_interface.purge_logs(false);

  /*
    selected_donor->get_hostname(), from Group_member_info, returns
    a string copy which is only valid in this stack lifespan. Since
    the below hostname pointer will be passed all the way down to
    recovery channel lex_mi->host, we need to keep its memory
    attached to this object, more precisely to
    selected_donor_hostname class member.
  */

  selected_donor_hostname.assign(hostname);

  error = donor_connection_interface.initialize_channel(
      const_cast<char *>(hostname.c_str()), port, /*user*/ nullptr,
      /*pass*/ nullptr, recovery_use_ssl, recovery_ssl_ca, recovery_ssl_capath,
      recovery_ssl_cert, recovery_ssl_cipher, recovery_ssl_key,
      recovery_ssl_crl, recovery_ssl_crlpath, recovery_ssl_verify_server_cert,
      DEFAULT_THREAD_PRIORITY, /*retry_count*/ 1, /*preserve_logs*/ false,
      recovery_public_key_path, recovery_get_public_key,
      recovery_compression_algorithm, recovery_zstd_compression_level,
      recovery_tls_version,
      recovery_tls_ciphersuites_null ? nullptr : recovery_tls_ciphersuites,
      /*ignore_ws_mem_limit*/ true, /*allow_drop_write_set*/ true);

  if (!error) {
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_ESTABLISHING_CONN_GRP_REC_DONOR,
                 selected_donor->get_uuid().c_str(), hostname.c_str(), port);
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CREATE_GRP_RPL_REC_CHANNEL,
                 selected_donor->get_uuid().c_str(), hostname.c_str(), port);
  }

  return error;
}

bool Recovery_state_transfer::verify_member_has_after_gtids_present(
    std::string &local_gtid_executed_string) {
  Get_system_variable get_system_variable;
  if (get_system_variable.get_global_gtid_executed(
          local_gtid_executed_string)) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GROUP_REPLICATION_RECOVERY_FETCHING_GTID_EXECUTED_SET);
    return false;
  }

  Tsid_map local_tsid_map(nullptr);
  Gtid_set local_gtid_executed(&local_tsid_map, nullptr);
  if (local_gtid_executed.add_gtid_text(local_gtid_executed_string.c_str()) !=
      RETURN_STATUS_OK) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GROUP_REPLICATION_RECOVERY_ERROR_ADD_GTID_EXECUTED);
    return false;
  }

  Tsid_map donor_tsid_map(nullptr);
  Gtid_set donor_gtid_executed(&donor_tsid_map, nullptr);
  if (donor_gtid_executed.add_gtid_text(m_after_gtids.c_str()) !=
      RETURN_STATUS_OK) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GROUP_REPLICATION_RECOVERY_ERROR_ADD_GTID_EXECUTED);
    return false;
  }

  if (donor_gtid_executed.is_subset(&local_gtid_executed)) {
    return true;
  }

  return false;
}

int Recovery_state_transfer::start_recovery_donor_threads() {
  DBUG_TRACE;
  int error = 1;
  std::string local_gtid_executed_string;

  /*
    If recovery is without VCLE which is START REPLICA SQL_AFTER_GTIDS UNTIL
    clause, then joiner is recovered till Gtid_set sent in Recovery Metadata by
    sender. But before starting recovery threads it's checked if local member
    already has those gtid_set present. If present then recovery is skipped.
  */
  if (m_until_condition == CHANNEL_UNTIL_APPLIER_AFTER_GTIDS) {
    if (verify_member_has_after_gtids_present(local_gtid_executed_string)) {
      LogPluginErr(INFORMATION_LEVEL,
                   ER_GROUP_REPLICATION_RECOVERY_SKIPPED_GTID_PRESENT,
                   local_gtid_executed_string.c_str(), m_after_gtids.c_str());
      end_state_transfer();
      return 0;
    } else {
      // recovery using START REPLICA SQL_AFTER_GTIDS UNTIL clause.
      error = donor_connection_interface.start_threads(
          true, true, &m_after_gtids, true, m_until_condition);
    }
    /*
      If recovery is using VCLE which is START REPLICA SQL_VIEW_ID UNTIL clause.
    */
  } else if (m_until_condition == CHANNEL_UNTIL_VIEW_ID) {
    error = donor_connection_interface.start_threads(true, true, &view_id, true,
                                                     m_until_condition);
  }

  if (!error) {
    DBUG_EXECUTE_IF("pause_after_io_thread_stop_hook", {
      const char act[] =
          "now "
          "WAIT_FOR reached_stopping_io_thread";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    };);
    DBUG_EXECUTE_IF("pause_after_sql_thread_stop_hook", {
      const char act[] =
          "now "
          "WAIT_FOR reached_stopping_sql_thread";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    };);
    DBUG_EXECUTE_IF(
        "group_replication_recovery_sleep_before_wait_for_connection",
        { my_sleep(1000000); });

    /*
      Register a channel observer to detect SQL/IO thread stops
      This is not done before the start as the hooks in place verify the
      stopping thread id and that can lead to deadlocks with start itself.
    */
    channel_observation_manager->register_channel_observer(
        recovery_channel_observer);
  }

  /*
    We should unregister the observer and error out if the threads are stopping
    or have stopped while the observer was being registered and the state
    transfer is not yet completed.
  */
  bool is_receiver_stopping =
      donor_connection_interface.is_receiver_thread_stopping();
  bool is_receiver_stopped =
      !donor_connection_interface.is_receiver_thread_running();
  bool is_applier_stopping =
      donor_connection_interface.is_applier_thread_stopping();
  bool is_applier_stopped =
      !donor_connection_interface.is_applier_thread_running();

  if (m_until_condition == CHANNEL_UNTIL_APPLIER_AFTER_GTIDS) {
    /*
      If applier is stopping we should give some time for it to
      stop, so that we can compare the after gtids set.
    */
    const unsigned long sleep_time{100000UL};  // 100 milliseconds
    constexpr const unsigned long max_channel_stopped_wait_timeout{30UL *
                                                                   1000000UL};
    unsigned long channel_stopped_wait_timeout{0};
    while (!error && !recovery_aborted && !donor_channel_thread_error &&
           !on_failover && !donor_transfer_finished &&
           donor_connection_interface.is_applier_thread_stopping() &&
           channel_stopped_wait_timeout < max_channel_stopped_wait_timeout) {
      my_sleep(sleep_time);
      channel_stopped_wait_timeout += sleep_time;
    }

    /*
      Check if replication applier thread has stopped:
      Verify after_gtids received from recovery metadata
      donor is present on member.
       - If present, recovery has completed.
       - If not present, set error.
    */
    if (!donor_transfer_finished &&
        !donor_connection_interface.is_applier_thread_running()) {
      if (verify_member_has_after_gtids_present(local_gtid_executed_string)) {
        LogPluginErr(INFORMATION_LEVEL,
                     ER_GROUP_REPLICATION_RECOVERY_STOPPED_GTID_PRESENT,
                     m_after_gtids.c_str());
        end_state_transfer();
        error = 0;
      } else {
        if (!error) {
          error = 1;
        }
        channel_observation_manager->unregister_channel_observer(
            recovery_channel_observer);
      }
    }

    if (!error && !donor_transfer_finished &&
        (donor_connection_interface.is_receiver_thread_stopping() ||
         !donor_connection_interface.is_receiver_thread_running() ||
         donor_connection_interface.is_applier_thread_stopping() ||
         !donor_connection_interface.is_applier_thread_running())) {
      error = 1;
      channel_observation_manager->unregister_channel_observer(
          recovery_channel_observer);
    }

  } else if (!error && !donor_transfer_finished &&
             (is_receiver_stopping || is_receiver_stopped ||
              is_applier_stopping || is_applier_stopped)) {
    error = 1;
    channel_observation_manager->unregister_channel_observer(
        recovery_channel_observer);
  }

  DBUG_EXECUTE_IF("pause_after_io_thread_stop_hook", {
    const char act[] = "now SIGNAL continue_to_stop_io_thread";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);
  DBUG_EXECUTE_IF("pause_after_sql_thread_stop_hook", {
    const char act[] = "now SIGNAL continue_to_stop_sql_thread";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  if (error) {
    donor_connection_interface.stop_threads(true /* receiver */,
                                            true /* applier */);

    if (error == RPL_CHANNEL_SERVICE_RECEIVER_CONNECTION_ERROR) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_DONOR_SERVER_CONN);
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CHECK_STATUS_TABLE);
    } else {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_STARTING_GRP_REC);
    }
  }

  return error;
}

State_transfer_status Recovery_state_transfer::terminate_recovery_slave_threads(
    bool purge_logs) {
  DBUG_TRACE;

  LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_DONOR_CONN_TERMINATION);

  State_transfer_status error = STATE_TRANSFER_OK;

  // If the threads never started, the method just returns
  if (donor_connection_interface.stop_threads(true, true)) {
    error = STATE_TRANSFER_STOP;
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_STOPPING_GRP_REC);
  } else {
    if (purge_logs) {
      // If there is no repository in place nothing happens
      if (purge_recovery_slave_threads_repos()) error = STATE_TRANSFER_PURGE;
    }
  }

  return error;
}

int Recovery_state_transfer::purge_recovery_slave_threads_repos() {
  DBUG_TRACE;

  int error = 0;
  if ((error = donor_connection_interface.purge_logs(false))) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_PURGE_REC);
    return error;
    /* purecov: end */
  }
  error = donor_connection_interface.initialize_channel(
      /*host*/ const_cast<char *>("<NULL>"), /*port*/ 0, /*user*/ nullptr,
      /*pass*/ nullptr, /*use_ssl*/ false, /*ssl_ca*/ nullptr,
      /*ssl_capath*/ nullptr, /*ssl_cert*/ nullptr, /*ssl_cipher*/ nullptr,
      /*ssl_key*/ nullptr, /*ssl_crl*/ nullptr, /*ssl_crlpath*/ nullptr,
      /*ssl_verify*/ false,
      /*priority*/ DEFAULT_THREAD_PRIORITY,
      /*retry_count*/ 1,
      /*preserve_logs*/ false, /*public_key_path*/ nullptr,
      /*get_public_key*/ false, /*compression_alg*/ nullptr,
      /*compression_level*/ 0, /*tls_version*/ nullptr, /*tls_cipher*/ nullptr,
      /*ignore_ws_mem_limit*/ true, /*allow_drop_write_set*/ true);

  return error;
}

State_transfer_status Recovery_state_transfer::state_transfer(
    Plugin_stage_monitor_handler &stage_handler) {
  DBUG_TRACE;

  State_transfer_status error = STATE_TRANSFER_OK;

  while (!donor_transfer_finished && !recovery_aborted) {
    /*
      If an applier error happened: stop the slave threads.
      We do not purge logs or reset channel configuration to
      preserve the error information on performance schema
      tables until the next recovery attempt.
      Recovery_state_transfer::initialize_donor_connection() will
      take care of that.
    */
    if (donor_channel_thread_error) {
      // Unsubscribe the listener until it connects again.
      channel_observation_manager->unregister_channel_observer(
          recovery_channel_observer);

      if ((error = terminate_recovery_slave_threads(false))) {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_UNABLE_TO_KILL_CONN_REC_DONOR_APPLIER);
        // if we can't stop, abort recovery
        return error;
        /* purecov: end */
      }
    }

    // If the donor left, just terminate the threads with no log purging
    if (on_failover) {
      // Unsubscribe the listener until it connects again.
      channel_observation_manager->unregister_channel_observer(
          recovery_channel_observer);

      // Stop the threads before reconfiguring the connection
      if (donor_connection_interface.stop_threads(true, true)) {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_UNABLE_TO_KILL_CONN_REC_DONOR_FAILOVER);
        // if we can't stop, abort recovery
        return STATE_TRANSFER_STOP;
        /* purecov: end */
      }
    }

    stage_handler.set_stage(info_GR_STAGE_recovery_connecting_to_donor.m_key,
                            __FILE__, __LINE__, 0, 0);
    if (!recovery_aborted) {
      // if the connection to the donor failed, abort recovery
      if (establish_donor_connection()) {
        error = STATE_TRANSFER_NO_CONNECTION;
        break;
      }
    }
    stage_handler.set_stage(info_GR_STAGE_recovery_transferring_state.m_key,
                            __FILE__, __LINE__, 0, 0);

    /*
      donor_transfer_finished    -> When recovery is using CHANNEL_UNTIL_VIEW_ID
                                 set by the set_retrieved_cert_info method.
                                 When recovery is using
                                 CHANNEL_UNTIL_APPLIER_AFTER_GTIDS for start
                                 replica until condition set by
                                 inform_of_applier_stop method.
                                 lock: recovery_lock
      recovery_aborted           -> set when stopping recovery
                                 lock: run_lock
      on_failover                -> set to true on update_recovery_process.
                                 set to false when connected to a valid donor
                                 lock: donor_selection_lock
      donor_channel_thread_error -> set to true on inform_of_applier_stop or
                                 inform_of_receiver_stop.
                                 set to false before connecting to any donor
                                 lock: donor_selection_lock
    */
    mysql_mutex_lock(&recovery_lock);
    while (!donor_transfer_finished && !recovery_aborted && !on_failover &&
           !donor_channel_thread_error) {
      mysql_cond_wait(&recovery_condition, &recovery_lock);
    }
    mysql_mutex_unlock(&recovery_lock);
  }  // if the current connection was terminated, connect again

  channel_observation_manager->unregister_channel_observer(
      recovery_channel_observer);

  // do not purge logs if an error occur, keep the diagnose on SLAVE STATUS
  bool purge_relay_logs = (error == STATE_TRANSFER_OK);
  DBUG_EXECUTE_IF("gr_recovery_skip_purge_logs", { purge_relay_logs = false; });
  State_transfer_status stop_error =
      terminate_recovery_slave_threads(purge_relay_logs);

  // If a connection error exists, we preserve it
  if (error == STATE_TRANSFER_OK && stop_error != STATE_TRANSFER_OK)
    error = stop_error;

  connected_to_donor = false;

  return error;
}
