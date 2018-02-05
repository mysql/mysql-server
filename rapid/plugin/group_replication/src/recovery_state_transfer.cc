/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <assert.h>
#include <mysql/group_replication_priv.h>
#include <time.h>

#include "my_dbug.h"
#include "my_systime.h"
#include "mysql/components/services/log_builtins.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_psi.h"
#include "plugin/group_replication/include/plugin_server_include.h"
#include "plugin/group_replication/include/recovery_channel_state_observer.h"
#include "plugin/group_replication/include/recovery_state_transfer.h"

using std::string;

Recovery_state_transfer::Recovery_state_transfer(
    char *recovery_channel_name, const string &member_uuid,
    Channel_observation_manager *channel_obsr_mngr)
    : selected_donor(NULL),
      group_members(NULL),
      donor_connection_retry_count(0),
      recovery_aborted(false),
      donor_transfer_finished(false),
      connected_to_donor(false),
      on_failover(false),
      donor_connection_interface(recovery_channel_name),
      channel_observation_manager(channel_obsr_mngr),
      recovery_channel_observer(NULL),
      recovery_use_ssl(false),
      recovery_get_public_key(false),
      recovery_ssl_verify_server_cert(false),
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

  this->member_uuid = member_uuid;

  mysql_mutex_init(key_GR_LOCK_recovery, &recovery_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_recovery, &recovery_condition);
  mysql_mutex_init(key_GR_LOCK_recovery_donor_selection, &donor_selection_lock,
                   MY_MUTEX_INIT_FAST);

  recovery_channel_observer = new Recovery_channel_state_observer(this);
}

Recovery_state_transfer::~Recovery_state_transfer() {
  if (group_members != NULL) {
    std::vector<Group_member_info *>::iterator member_it =
        group_members->begin();
    while (member_it != group_members->end()) {
      delete (*member_it);
      ++member_it;
    }
  }
  delete group_members;
  delete recovery_channel_observer;
  mysql_mutex_destroy(&recovery_lock);
  mysql_cond_destroy(&recovery_condition);
  mysql_mutex_destroy(&donor_selection_lock);
}

void Recovery_state_transfer::initialize(const string &rec_view_id) {
  DBUG_ENTER("Recovery_state_transfer::initialize");

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

  this->view_id.clear();
  this->view_id.append(rec_view_id);

  DBUG_VOID_RETURN;
}

void Recovery_state_transfer::inform_of_applier_stop(my_thread_id thread_id,
                                                     bool) {
  DBUG_ENTER("Recovery_state_transfer::inform_of_applier_stop");

  /*
    This method doesn't take any locks as it could lead to dead locks between
    the connection process and this method that can be invoked in that context.
    Since this only affects the recovery loop and the flag is reset at each
    connection, no major concurrency issues should exist.
  */

  // Act if:
  if (
      // we don't have all the data yet
      !donor_transfer_finished &&
      // recovery was not aborted
      !recovery_aborted &&
      // the signal belongs to the recovery donor channel thread
      donor_connection_interface.is_own_event_applier(thread_id)) {
    mysql_mutex_lock(&recovery_lock);
    donor_channel_thread_error = true;
    mysql_cond_broadcast(&recovery_condition);
    mysql_mutex_unlock(&recovery_lock);
  }

  DBUG_VOID_RETURN;
}

void Recovery_state_transfer::inform_of_receiver_stop(my_thread_id thread_id) {
  DBUG_ENTER("Recovery_state_transfer::inform_of_receiver_stop");

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

  DBUG_VOID_RETURN;
}

void Recovery_state_transfer::initialize_group_info() {
  DBUG_ENTER("Recovery_state_transfer::initialize_group_info");

  selected_donor = NULL;
  // Update the group member info
  mysql_mutex_lock(&donor_selection_lock);
  update_group_membership(false);
  mysql_mutex_unlock(&donor_selection_lock);

  DBUG_VOID_RETURN;
}

void Recovery_state_transfer::update_group_membership(bool update_donor) {
  DBUG_ENTER("Recovery_state_transfer::update_group_membership");

#ifndef DBUG_OFF
  mysql_mutex_assert_owner(&donor_selection_lock);
#endif

  // if needed update the reference to the donor member
  string donor_uuid;
  if (selected_donor != NULL && update_donor) {
    donor_uuid.assign(selected_donor->get_uuid());
  }

  if (group_members != NULL) {
    std::vector<Group_member_info *>::iterator member_it =
        group_members->begin();
    while (member_it != group_members->end()) {
      delete (*member_it);
      ++member_it;
    }
  }
  delete group_members;

  group_members = group_member_mgr->get_all_members();

  // When updating the member list, also rebuild the suitable donor list
  build_donor_list(&donor_uuid);

  DBUG_VOID_RETURN;
}

void Recovery_state_transfer::abort_state_transfer() {
  DBUG_ENTER("Recovery_state_transfer::abort_state_transfer");

  // Break the wait for view change event
  mysql_mutex_lock(&recovery_lock);
  recovery_aborted = true;
  mysql_cond_broadcast(&recovery_condition);
  mysql_mutex_unlock(&recovery_lock);

  DBUG_VOID_RETURN;
}

int Recovery_state_transfer::update_recovery_process(bool did_members_left) {
  DBUG_ENTER("Recovery_state_transfer::update_recovery_process");
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
  if (selected_donor != NULL && did_members_left) {
    current_donor_uuid.assign(selected_donor->get_uuid());
    current_donor_hostname.assign(selected_donor->get_hostname());
    current_donor_port = selected_donor->get_port();
    Group_member_info *current_donor =
        group_member_mgr->get_group_member_info(current_donor_uuid);
    donor_left = (current_donor == NULL);
    delete current_donor;
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
    selected_donor = NULL;
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

  DBUG_RETURN(error);
}

void Recovery_state_transfer::end_state_transfer() {
  DBUG_ENTER("Recovery_state_transfer::end_state_transfer");

  mysql_mutex_lock(&recovery_lock);
  donor_transfer_finished = true;
  mysql_cond_broadcast(&recovery_condition);
  mysql_mutex_unlock(&recovery_lock);

  DBUG_VOID_RETURN;
}

void Recovery_state_transfer::donor_failover() {
  DBUG_ENTER("Recovery_state_transfer::donor_failover");

  // Awake the recovery process so it can loop again to connect to another donor
  mysql_mutex_lock(&recovery_lock);
  on_failover = true;
  mysql_cond_broadcast(&recovery_condition);
  mysql_mutex_unlock(&recovery_lock);

  DBUG_VOID_RETURN;
}

int Recovery_state_transfer::check_recovery_thread_status() {
  DBUG_ENTER("Recovery_state_transfer::check_recovery_thread_status");

  // if some of the threads are running
  if (donor_connection_interface.is_receiver_thread_running() ||
      donor_connection_interface.is_applier_thread_running()) {
    return terminate_recovery_slave_threads(); /* purecov: inspected */
  }
  DBUG_RETURN(0);
}

bool Recovery_state_transfer::is_own_event_channel(my_thread_id id) {
  DBUG_ENTER("Recovery_state_transfer::is_own_event_channel");
  DBUG_RETURN(donor_connection_interface.is_own_event_applier(id));
}

void Recovery_state_transfer::build_donor_list(string *selected_donor_uuid) {
  DBUG_ENTER("Recovery_state_transfer::build_donor_list");

  suitable_donors.clear();

  std::vector<Group_member_info *>::iterator member_it = group_members->begin();

  while (member_it != group_members->end()) {
    Group_member_info *member = *member_it;
    // is online and it's not me
    string m_uuid = member->get_uuid();
    bool is_online =
        member->get_recovery_status() == Group_member_info::MEMBER_ONLINE;
    bool not_self = m_uuid.compare(member_uuid);

    if (is_online && not_self) {
      suitable_donors.push_back(member);
    }

    // if requested, and if the donor is still in the group, update its
    // reference
    if (selected_donor_uuid != NULL && !m_uuid.compare(*selected_donor_uuid)) {
      selected_donor = member;
    }

    ++member_it;
  }

  if (suitable_donors.size() > 1) {
    std::random_shuffle(suitable_donors.begin(), suitable_donors.end());
  }

  // no need for errors if no donors exist, we thrown it in the connection
  // method.
  DBUG_VOID_RETURN;
}

int Recovery_state_transfer::establish_donor_connection() {
  DBUG_ENTER("Recovery_state_transfer::establish_donor_connection");

  int error = -1;
  connected_to_donor = false;

  while (error != 0 && !recovery_aborted) {
    mysql_mutex_lock(&donor_selection_lock);

    // max number of retries reached, abort
    if (donor_connection_retry_count == max_connection_attempts_to_donors) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MAXIMUM_CONNECTION_RETRIES_REACHED);
      mysql_mutex_unlock(&donor_selection_lock);
      DBUG_RETURN(error);
    }

    if (group_member_mgr->get_number_of_members() == 1) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ALL_DONORS_LEFT_ABORT_RECOVERY);
      mysql_mutex_unlock(&donor_selection_lock);
      DBUG_RETURN(error);
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

      build_donor_list(NULL);
      if (suitable_donors.empty()) {
        LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_NO_VALID_DONOR);
        donor_connection_retry_count++;
        mysql_mutex_unlock(&donor_selection_lock);
        continue;
      }
    }

    donor_channel_thread_error = false;

    // Get the last element and delete it
    selected_donor = suitable_donors.back();
    suitable_donors.pop_back();
    // increment the number of tries
    donor_connection_retry_count++;

    if ((error = initialize_donor_connection())) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_CONFIG_RECOVERY); /* purecov: inspected */
    }

    if (!error && !recovery_aborted) {
      error = start_recovery_donor_threads();
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

  DBUG_RETURN(error);
}

int Recovery_state_transfer::initialize_donor_connection() {
  DBUG_ENTER("Recovery_state_transfer::initialize_donor_connection");

  int error = 0;

  donor_connection_interface.purge_logs(false);

  char *hostname = const_cast<char *>(selected_donor->get_hostname().c_str());
  uint port = selected_donor->get_port();

  error = donor_connection_interface.initialize_channel(
      hostname, port, NULL, NULL, recovery_use_ssl, recovery_ssl_ca,
      recovery_ssl_capath, recovery_ssl_cert, recovery_ssl_cipher,
      recovery_ssl_key, recovery_ssl_crl, recovery_ssl_crlpath,
      recovery_ssl_verify_server_cert, DEFAULT_THREAD_PRIORITY, 1, false,
      recovery_public_key_path, recovery_get_public_key);

  if (!error) {
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_ESTABLISHING_CONN_GRP_REC_DONOR,
                 selected_donor->get_uuid().c_str(), hostname, port);
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CREATE_GRP_RPL_REC_CHANNEL,
                 selected_donor->get_uuid().c_str(), hostname,
                 port); /* purecov: inspected */
  }

  DBUG_RETURN(error);
}

int Recovery_state_transfer::start_recovery_donor_threads() {
  DBUG_ENTER("Recovery_state_transfer::start_recovery_donor_threads");

  int error =
      donor_connection_interface.start_threads(true, true, &view_id, true);

  if (!error) {
    DBUG_EXECUTE_IF("pause_after_io_thread_stop_hook", {
      const char act[] =
          "now "
          "WAIT_FOR reached_stopping_io_thread";
      DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    };);
    DBUG_EXECUTE_IF("pause_after_sql_thread_stop_hook", {
      const char act[] =
          "now "
          "WAIT_FOR reached_stopping_sql_thread";
      DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    };);

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

  if (!error && !donor_transfer_finished &&
      (is_receiver_stopping || is_receiver_stopped || is_applier_stopping ||
       is_applier_stopped)) {
    error = 1;
    channel_observation_manager->unregister_channel_observer(
        recovery_channel_observer);
  }

  DBUG_EXECUTE_IF("pause_after_io_thread_stop_hook", {
    const char act[] = "now SIGNAL continue_to_stop_io_thread";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);
  DBUG_EXECUTE_IF("pause_after_sql_thread_stop_hook", {
    const char act[] = "now SIGNAL continue_to_stop_sql_thread";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
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

  DBUG_RETURN(error);
}

int Recovery_state_transfer::terminate_recovery_slave_threads() {
  DBUG_ENTER("Recovery_state_transfer::terminate_recovery_slave_threads");

  LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_DONOR_CONN_TERMINATION);

  int error = 0;

  // If the threads never started, the method just returns
  if ((error = donor_connection_interface.stop_threads(true, true))) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_STOPPING_GRP_REC); /* purecov: inspected */
  } else {
    // If there is no repository in place nothing happens
    error = purge_recovery_slave_threads_repos();
  }

  DBUG_RETURN(error);
}

int Recovery_state_transfer::purge_recovery_slave_threads_repos() {
  DBUG_ENTER("Recovery_state_transfer::purge_recovery_slave_threads_repos");

  int error = 0;
  if ((error = donor_connection_interface.purge_logs(false))) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_PURGE_REC);
    DBUG_RETURN(error);
    /* purecov: end */
  }
  error = donor_connection_interface.initialize_channel(
      const_cast<char *>("<NULL>"), 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, DEFAULT_THREAD_PRIORITY, 1, false, NULL, false);

  DBUG_RETURN(error);
}

int Recovery_state_transfer::state_transfer(THD *recovery_thd) {
  DBUG_ENTER("Recovery_state_transfer::state_transfer");

  int error = 0;

  while (!donor_transfer_finished && !recovery_aborted) {
    // If an applier error happened: stop the receiver thread and purge the logs
    if (donor_channel_thread_error) {
      // Unsubscribe the listener until it connects again.
      channel_observation_manager->unregister_channel_observer(
          recovery_channel_observer);

      if ((error = terminate_recovery_slave_threads())) {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_UNABLE_TO_KILL_CONN_REC_DONOR_APPLIER);
        // if we can't stop, abort recovery
        DBUG_RETURN(error);
        /* purecov: end */
      }
    }

    // If the donor left, just terminate the threads with no log purging
    if (on_failover) {
      // Unsubscribe the listener until it connects again.
      channel_observation_manager->unregister_channel_observer(
          recovery_channel_observer);

      // Stop the threads before reconfiguring the connection
      if ((error = donor_connection_interface.stop_threads(true, true))) {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_UNABLE_TO_KILL_CONN_REC_DONOR_FAILOVER);
        // if we can't stop, abort recovery
        DBUG_RETURN(error);
        /* purecov: end */
      }
    }

#ifndef _WIN32
    THD_STAGE_INFO(recovery_thd, stage_connecting_to_master);
#endif

    if (!recovery_aborted) {
      // if the connection to the donor failed, abort recovery
      if ((error = establish_donor_connection())) {
        break;
      }
    }

#ifndef _WIN32
    THD_STAGE_INFO(recovery_thd, stage_executing);
#endif

    /*
      donor_transfer_finished    -> set by the set_retrieved_cert_info method.
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
  terminate_recovery_slave_threads();
  connected_to_donor = false;

  DBUG_RETURN(error);
}
