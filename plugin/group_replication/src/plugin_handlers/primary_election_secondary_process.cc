/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/primary_election_secondary_process.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_utils.h"
#include "plugin/group_replication/include/plugin_handlers/read_mode_handler.h"

static void *launch_handler_thread(void *arg) {
  Primary_election_secondary_process *handler =
      (Primary_election_secondary_process *)arg;
  handler->secondary_election_process_handler();
  return nullptr;
}

Primary_election_secondary_process::Primary_election_secondary_process()
    : election_process_thd_state(),
      election_process_aborted(false),
      waiting_on_old_primary_transactions(false),
      primary_ready(false),
      group_in_read_mode(false),
      is_waiting_on_read_mode_group(false),
      number_of_know_members(0) {
  mysql_mutex_init(key_GR_LOCK_primary_election_secondary_process_run,
                   &election_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_primary_election_secondary_process_run,
                  &election_cond);
}

Primary_election_secondary_process::~Primary_election_secondary_process() {
  mysql_mutex_destroy(&election_lock);
  mysql_cond_destroy(&election_cond);
}

void Primary_election_secondary_process::set_stop_wait_timeout(ulong timeout) {
  stop_wait_timeout = timeout;
}

int Primary_election_secondary_process::launch_secondary_election_process(
    enum_primary_election_mode mode, std::string &primary_to_elect,
    Group_member_info_list *group_members_info) {
  DBUG_TRACE;

  mysql_mutex_lock(&election_lock);

  // Callers should ensure the process is terminated
  assert(election_process_thd_state.is_thread_dead());
  if (election_process_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&election_lock); /* purecov: inspected */
    return 2;                           /* purecov: inspected */
  }

  election_mode = mode;
  primary_uuid.assign(primary_to_elect);
  primary_ready = false;
  group_in_read_mode = false;
  is_waiting_on_read_mode_group = false;
  election_process_aborted = false;

  known_members_addresses.clear();
  for (Group_member_info *member : *group_members_info) {
    known_members_addresses.push_back(
        member->get_gcs_member_id().get_member_id());
  }
  number_of_know_members = known_members_addresses.size();

  stage_handler = new Plugin_stage_monitor_handler();
  // If the service acquirement fails, the calls to this class have no effect
  if (stage_handler->initialize_stage_monitor()) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_NO_STAGE_SERVICE); /* purecov: inspected */
  }

  group_events_observation_manager->register_group_event_observer(this);

  if (mysql_thread_create(key_GR_THD_primary_election_secondary_process,
                          &primary_election_pthd, get_connection_attrib(),
                          launch_handler_thread, (void *)this)) {
    /* purecov: begin inspected */
    group_events_observation_manager->unregister_group_event_observer(this);
    mysql_mutex_unlock(&election_lock);
    return 1;
    /* purecov: end */
  }

  election_process_thd_state.set_created();
  while (election_process_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep",
               ("Waiting for the Primary election process thread to start"));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  mysql_mutex_unlock(&election_lock);

  return 0;

  return 0;
}

int Primary_election_secondary_process::secondary_election_process_handler() {
  DBUG_TRACE;
  int error = 0;
  std::string err_msg;

  THD *thd = nullptr;
  thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = (char *)&thd;
  thd->store_globals();
  global_thd_manager_add_thd(thd);

  mysql_mutex_lock(&election_lock);
  election_process_thd_state.set_running();
  mysql_cond_broadcast(&election_cond);
  mysql_mutex_unlock(&election_lock);

  stage_handler->set_stage(
      info_GR_STAGE_primary_election_pending_transactions.m_key, __FILE__,
      __LINE__, 1, 0);

  mysql_mutex_lock(&election_lock);
  while (!primary_ready && !election_process_aborted) {
    DBUG_PRINT("sleep", ("Waiting for the primary member to be ready"));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  mysql_mutex_unlock(&election_lock);
  stage_handler->set_completed_work(1);

  if (election_process_aborted) goto end;

  if (enable_read_mode_on_server()) {
    if (!election_process_aborted && !get_server_shutdown_status()) {
      abort_plugin_process(
          "Cannot enable the super read only mode on a secondary member.");
      error = 1;
      election_process_aborted = true;
      goto end;
    }
  }

  if (election_mode == DEAD_OLD_PRIMARY) {
    group_events_observation_manager->after_primary_election(
        primary_uuid,
        enum_primary_election_primary_change_status::PRIMARY_DID_CHANGE,
        election_mode);
    goto wait_for_queued_message;
  }

  /* Can only set the stage here as the read mode call removes the stage info */
  mysql_mutex_lock(&election_lock);
  is_waiting_on_read_mode_group = true;
  stage_handler->set_stage(
      info_GR_STAGE_primary_election_group_read_only.m_key, __FILE__, __LINE__,
      number_of_know_members,
      number_of_know_members - known_members_addresses.size());
  mysql_mutex_unlock(&election_lock);

  if (signal_read_mode_ready()) {
    /* purecov: begin inspected */
    error = 1;
    err_msg.assign(
        "Could not notify other members on how the member is now in read "
        "mode.");
    goto end;
    /* purecov: end */
  }

  mysql_mutex_lock(&election_lock);
  while (!group_in_read_mode && !election_process_aborted) {
    DBUG_PRINT("sleep", ("Waiting for the group to be in read mode."));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  mysql_mutex_unlock(&election_lock);

  if (election_process_aborted) goto end;

  stage_handler->set_stage(
      info_GR_STAGE_primary_election_old_primary_transactions.m_key, __FILE__,
      __LINE__, 1, 0);

wait_for_queued_message:

  mysql_mutex_lock(&election_lock);
  while (waiting_on_old_primary_transactions && !election_process_aborted) {
    DBUG_PRINT("sleep", ("Waiting for the primary member to execute all "
                         "previous transactions"));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  mysql_mutex_unlock(&election_lock);

  stage_handler->set_completed_work(1);

end:

  primary_election_handler->set_election_running(false);

  if (!election_process_aborted && !error) {
    Group_member_info primary_member_info;
    if (!group_member_mgr->get_group_member_info(primary_uuid,
                                                 primary_member_info)) {
      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SRV_SECONDARY_MEM,
                   primary_member_info.get_hostname().c_str(),
                   primary_member_info.get_port());
    }
  }

  group_events_observation_manager->unregister_group_event_observer(this);

  if (error && !election_process_aborted) {
    group_events_observation_manager->after_primary_election(
        primary_uuid,
        enum_primary_election_primary_change_status::
            PRIMARY_DID_CHANGE_WITH_ERROR,
        election_mode, error); /* purecov: inspected */
    kill_transactions_and_leave_on_election_error(
        err_msg); /* purecov: inspected */
  }

  stage_handler->end_stage();
  stage_handler->terminate_stage_monitor();
  delete stage_handler;
  stage_handler = nullptr;

  thd->release_resources();
  global_thd_manager_remove_thd(thd);
  delete thd;

  Gcs_interface_factory::cleanup_thread_communication_resources(
      Gcs_operations::get_gcs_engine());

  my_thread_end();

  mysql_mutex_lock(&election_lock);
  election_process_thd_state.set_terminated();
  mysql_cond_broadcast(&election_cond);
  mysql_mutex_unlock(&election_lock);

  return error;
}

bool Primary_election_secondary_process::is_election_process_running() {
  return election_process_thd_state.is_thread_alive();
}

bool Primary_election_secondary_process::enable_read_mode_on_server() {
  int error = 0;
  remote_clone_handler->lock_gr_clone_read_mode_lock();

  if (!plugin_is_group_replication_cloning()) {
    if (!error && !election_process_aborted) {
      error = enable_server_read_mode();
    }
  }

  remote_clone_handler->unlock_gr_clone_read_mode_lock();

  return error != 0;
}

bool Primary_election_secondary_process::signal_read_mode_ready() {
  Single_primary_message single_primary_message(
      Single_primary_message::SINGLE_PRIMARY_READ_MODE_SET);
  return send_message(&single_primary_message);
}

int Primary_election_secondary_process::after_view_change(
    const std::vector<Gcs_member_identifier> &,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &, bool is_leaving,
    bool *skip_election, enum_primary_election_mode *, std::string &) {
  *skip_election = false;

  if (is_leaving) {
    terminate_election_process(false);
    return 0;
  }

  mysql_mutex_lock(&election_lock);
  for (Gcs_member_identifier leaving_member : leaving) {
    known_members_addresses.remove(leaving_member.get_member_id());
  }
  // Update the state only if needed
  if (primary_ready || !group_in_read_mode) {
    stage_handler->set_completed_work(number_of_know_members -
                                      known_members_addresses.size());
  }
  if (known_members_addresses.empty()) {
    if (!group_in_read_mode) {
      group_in_read_mode = true;
      mysql_cond_broadcast(&election_cond);
      /*
       group_in_read_mode is false so response from some member was still
       pending. But known_members_addresses is empty so members on which
       election was waiting have left the group.
       If primary is part of the group then we can end the election normally.
       If primary member has left the group then forcefully end the election so
       that new election can take place.
      */
      const enum_primary_election_primary_change_status primary_changed_status =
          group_member_mgr->is_member_info_present(primary_uuid)
              ? enum_primary_election_primary_change_status::PRIMARY_DID_CHANGE
              : enum_primary_election_primary_change_status::
                    PRIMARY_DID_NOT_CHANGE_PRIMARY_LEFT_FORCE_ELECTION_END;
      group_events_observation_manager->after_primary_election(
          primary_uuid, primary_changed_status, election_mode);
    }
  }

  if (!group_member_mgr->is_member_info_present(primary_uuid)) {
    if (!group_in_read_mode) {
      election_process_aborted = true;
    } else {
      // Let the process end
      waiting_on_old_primary_transactions = true;
    }
    mysql_cond_broadcast(&election_cond);
  }

  mysql_mutex_unlock(&election_lock);
  return 0;
}

int Primary_election_secondary_process::after_primary_election(
    std::string, enum_primary_election_primary_change_status,
    enum_primary_election_mode, int) {
  return 0;
}

int Primary_election_secondary_process::before_message_handling(
    const Plugin_gcs_message &message, const std::string &message_origin,
    bool *skip_message) {
  *skip_message = false;
  Plugin_gcs_message::enum_cargo_type message_type = message.get_cargo_type();

  if (message_type == Plugin_gcs_message::CT_SINGLE_PRIMARY_MESSAGE) {
    const Single_primary_message &single_primary_message =
        down_cast<const Single_primary_message &>(message);
    Single_primary_message::Single_primary_message_type
        single_primary_msg_type =
            single_primary_message.get_single_primary_message_type();

    if (single_primary_msg_type ==
        Single_primary_message::SINGLE_PRIMARY_PRIMARY_READY) {
      mysql_mutex_lock(&election_lock);
      primary_ready = true;
      if (election_mode != DEAD_OLD_PRIMARY) {
        applier_module->queue_certification_enabling_packet();
        waiting_on_old_primary_transactions = true;
      }
      mysql_cond_broadcast(&election_cond);
      mysql_mutex_unlock(&election_lock);
    }

    if (single_primary_msg_type ==
        Single_primary_message::SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE) {
      mysql_mutex_lock(&election_lock);
      waiting_on_old_primary_transactions = false;
      mysql_cond_broadcast(&election_cond);
      mysql_mutex_unlock(&election_lock);
    }

    if (single_primary_msg_type ==
        Single_primary_message::SINGLE_PRIMARY_READ_MODE_SET) {
      mysql_mutex_lock(&election_lock);
      known_members_addresses.remove(message_origin);
      if (is_waiting_on_read_mode_group)
        stage_handler->set_completed_work(number_of_know_members -
                                          known_members_addresses.size());
      if (known_members_addresses.empty()) {
        if (!group_in_read_mode) {
          group_in_read_mode = true;
          mysql_cond_broadcast(&election_cond);
          group_events_observation_manager->after_primary_election(
              primary_uuid,
              enum_primary_election_primary_change_status::PRIMARY_DID_CHANGE,
              election_mode);
        }
      }
      mysql_mutex_unlock(&election_lock);
    }
  }

  return 0;
}

int Primary_election_secondary_process::terminate_election_process(bool wait) {
  mysql_mutex_lock(&election_lock);

  if (election_process_thd_state.is_thread_dead()) {
    mysql_mutex_unlock(&election_lock); /* purecov: inspected */
    return 0;                           /* purecov: inspected */
  }
  election_process_aborted = true;

  // Awake up possible stuck conditions
  mysql_cond_broadcast(&election_cond);

  if (wait) {
    while (election_process_thd_state.is_thread_alive()) {
      DBUG_PRINT("sleep", ("Waiting for the Primary election secondary process "
                           "thread to finish"));
      mysql_cond_wait(&election_cond, &election_lock);
    }

    assert(election_process_thd_state.is_thread_dead());
  }
  mysql_mutex_unlock(&election_lock);

  return 0;
}
