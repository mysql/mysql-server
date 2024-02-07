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

#include "plugin/group_replication/include/group_actions/multi_primary_migration_action.h"
#include "plugin/group_replication/include/mysql_version_gcs_protocol_map.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/consensus_leaders_handler.h"
#include "plugin/group_replication/include/plugin_handlers/server_ongoing_transactions_handler.h"
#include "plugin/group_replication/include/services/system_variable/set_system_variable.h"

bool send_multi_primary_action_message(Plugin_gcs_message *message) {
  enum_gcs_error msg_error = gcs_module->send_message(*message);
  if (msg_error != GCS_OK) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_MESSAGE_SENDING,
                 "change to multi primary mode."); /* purecov: inspected */
    return true;                                   /* purecov: inspected */
  }
  return false;
}

Multi_primary_migration_action::Multi_primary_migration_action()
    : Multi_primary_migration_action(0) {}

Multi_primary_migration_action::Multi_primary_migration_action(
    my_thread_id thread_id)
    : invoking_thread_id(thread_id),
      multi_primary_switch_aborted(false),
      action_killed(false),
      primary_uuid(""),
      primary_gcs_id(""),
      is_primary(false),
      is_primary_transaction_queue_applied(false) {
  mysql_mutex_init(key_GR_LOCK_multi_primary_action_notification,
                   &notification_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_multi_primary_action_notification,
                  &notification_cond);
  applier_checkpoint_condition = std::make_shared<Continuation>();
}

Multi_primary_migration_action::~Multi_primary_migration_action() {
  mysql_mutex_destroy(&notification_lock);
  mysql_cond_destroy(&notification_cond);
}

void Multi_primary_migration_action::get_action_message(
    Group_action_message **message) {
  *message = new Group_action_message(
      Group_action_message::ACTION_MULTI_PRIMARY_MESSAGE);
}

int Multi_primary_migration_action::process_action_message(
    Group_action_message &, const std::string &) {
  /*
    This means the action started on SPM but the server changed to MPM
    Still, as a new action can only be accepted when the previous terminated, it
    is safe to abort here as all members will do the same.
  */
  if (local_member_info && !local_member_info->in_primary_mode()) {
    execution_message_area.set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "The group already changed to multi primary mode. Aborting group "
        "configuration change."); /* purecov: inspected */
    return 1;                     /* purecov: inspected */
  }

  Group_member_info primary_info;
  if (!group_member_mgr->get_primary_member_info(primary_info)) {
    primary_uuid.assign(primary_info.get_uuid());
    primary_gcs_id.assign(primary_info.get_gcs_member_id().get_member_id());
    is_primary = !primary_uuid.compare(local_member_info->get_uuid());
  }

  group_events_observation_manager->register_group_event_observer(this);
  is_primary_transaction_queue_applied = false;

  return 0;
}

Group_action::enum_action_execution_result
Multi_primary_migration_action::execute_action(
    bool, Plugin_stage_monitor_handler *stage_handler,
    Notification_context *ctx) {
  bool mode_is_set = false;
  bool action_terminated = false;
  int error = 0;

  DBUG_TRACE;

  /**
    Wait for all packets in the applier module to be consumed.
    This safety check prevents certification enabling packets from being read
    while this process executes
  */
  applier_module->queue_and_wait_on_queue_checkpoint(
      applier_checkpoint_condition);
  applier_checkpoint_condition.reset(new Continuation());

  set_enforce_update_everywhere_checks(true);
  group_member_mgr->update_enforce_everywhere_checks_flag(true);
  Single_primary_message single_primary_message(
      Single_primary_message::SINGLE_PRIMARY_NO_RESTRICTED_TRANSACTIONS);

  if (is_primary) {
    stage_handler->set_stage(
        info_GR_STAGE_multi_primary_mode_switch_pending_transactions.m_key,
        __FILE__, __LINE__, 999, 0);

    Server_ongoing_transactions_handler ongoing_transactions_handler;
    ongoing_transactions_handler.initialize_server_service(stage_handler);
    if (ongoing_transactions_handler
            .wait_for_current_transaction_load_execution(
                &multi_primary_switch_aborted, invoking_thread_id)) {
      error = 1; /* purecov: inspected */
      execution_message_area.set_execution_message(
          Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
          "This operation ended in error as it was not possible to wait for "
          "the execution of server running "
          "transactions."); /* purecov: inspected */
      goto end;             /* purecov: inspected */
    }
    if (!multi_primary_switch_aborted) {
      if (send_multi_primary_action_message(&single_primary_message)) {
        error = 1; /* purecov: inspected */
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
            "This operation ended in error as it was not possible to "
            "contact the group allowing the operation "
            "progress."); /* purecov: inspected */
        goto end;         /* purecov: inspected */
      }
    }
  } else {
    stage_handler->set_stage(
        info_GR_STAGE_multi_primary_mode_switch_step_completion.m_key, __FILE__,
        __LINE__, 1, 0);
  }

  mysql_mutex_lock(&notification_lock);
  while (!is_primary_transaction_queue_applied &&
         !multi_primary_switch_aborted) {
    DBUG_PRINT("sleep",
               ("Waiting for transaction to be applied on the primary."));
    mysql_cond_wait(&notification_cond, &notification_lock);
  }
  mysql_mutex_unlock(&notification_lock);

  if (multi_primary_switch_aborted) goto end;

  set_single_primary_mode_var(false);
  group_member_mgr->update_primary_member_flag(false);
  ctx->set_member_role_changed();
  mode_is_set = true;

  if (!multi_primary_switch_aborted) {
    set_auto_increment_handler_values();
  }

  stage_handler->set_stage(
      info_GR_STAGE_multi_primary_mode_switch_buffered_transactions.m_key,
      __FILE__, __LINE__, 1, 0);
  if (!is_primary) {
    if (applier_module->wait_for_current_events_execution(
            applier_checkpoint_condition, &multi_primary_switch_aborted,
            false)) {
      error = 1;
      execution_message_area.set_execution_message(
          Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
          "This operation ended in error as it was not possible to wait for "
          "the execution of the plugin queued transactions.");
      goto end;
    }
    /* Member will reset read-only state if member is lowest version of the
     * group post multi-primary mode switch. */
    if (!multi_primary_switch_aborted) {
      events_handler->disable_read_mode_for_compatible_members(true);
    }
  } else {
    if (!multi_primary_switch_aborted) {
      /* Case when 8.0.13 <> 8.0.16 member is present and 8.0.17(or greater) was
       * primary. Post MPM switch read_only need to be set in 8.0.17 primary. */
      if (Compatibility_module::check_version_incompatibility(
              local_member_info->get_member_version(),
              group_member_mgr->get_group_lowest_online_version()) ==
          READ_COMPATIBLE) {
        if (enable_server_read_mode()) {
          /* purecov: begin inspected */
          LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_ENABLE_READ_ONLY_FAILED);
          /* purecov: end */
        }
      } else {
        /*
          Even when this members was the primary on single-primary mode, it
          might had read_only_mode enabled, as such we need to disable
          read_only_mode.
        */
        events_handler->disable_read_mode_for_compatible_members(true);
      }
    }
  }
  stage_handler->set_completed_work(1);

  if (!multi_primary_switch_aborted) {
    persist_variable_values();
    action_terminated = true;
  }

end:

  // If stopping when it already set the mode, no point in resetting the vars
  if (multi_primary_switch_aborted && !mode_is_set) {
    set_enforce_update_everywhere_checks(false);
    group_member_mgr->update_enforce_everywhere_checks_flag(false);
  }

  group_events_observation_manager->unregister_group_event_observer(this);

  if (!error)
    log_result_execution(multi_primary_switch_aborted && !action_terminated,
                         mode_is_set);

  if ((!multi_primary_switch_aborted && !error) || action_terminated) {
    Member_version const communication_protocol =
        convert_to_mysql_version(gcs_module->get_protocol_version());
    Gcs_member_identifier const my_gcs_id =
        local_member_info->get_gcs_member_id();

    consensus_leaders_handler->set_consensus_leaders(
        communication_protocol, false, Group_member_info::MEMBER_ROLE_PRIMARY,
        my_gcs_id);

    return Group_action::GROUP_ACTION_RESULT_TERMINATED;
  }

  if (action_killed) {
    return Group_action::GROUP_ACTION_RESULT_KILLED;
  }
  if (error) {
    return Group_action::GROUP_ACTION_RESULT_ERROR;
  }

  return Group_action::GROUP_ACTION_RESULT_ABORTED;
}

bool Multi_primary_migration_action::stop_action_execution(bool killed) {
  mysql_mutex_lock(&notification_lock);
  action_killed = killed;
  multi_primary_switch_aborted = true;
  applier_checkpoint_condition->signal();
  mysql_cond_broadcast(&notification_cond);
  mysql_mutex_unlock(&notification_lock);

  return false;
}

Group_action_diagnostics *Multi_primary_migration_action::get_execution_info() {
  return &execution_message_area;
}

PSI_stage_key
Multi_primary_migration_action::get_action_stage_termination_key() {
  return info_GR_STAGE_multi_primary_mode_switch_completion.m_key;
}

int Multi_primary_migration_action::after_view_change(
    const std::vector<Gcs_member_identifier> &,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &, bool is_leaving,
    bool *skip_election, enum_primary_election_mode *, std::string &) {
  if (is_leaving) {
    return 0;
  }

  *skip_election = true;

  bool is_primary_leaving = false;
  for (Gcs_member_identifier leaving_member : leaving) {
    is_primary_leaving = (leaving_member.get_member_id() == primary_gcs_id);
    if (is_primary_leaving) break;
  }

  if (is_primary_leaving) {
    mysql_mutex_lock(&notification_lock);
    is_primary_transaction_queue_applied = true;
    mysql_cond_broadcast(&notification_cond);
    mysql_mutex_unlock(&notification_lock);
    applier_module->queue_certification_enabling_packet();
  }

  return 0;
}

int Multi_primary_migration_action::after_primary_election(
    std::string, enum_primary_election_primary_change_status,
    enum_primary_election_mode, int) {
  return 0; /* purecov: inspected */
}

int Multi_primary_migration_action::before_message_handling(
    const Plugin_gcs_message &message, const std::string &,
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
        Single_primary_message::SINGLE_PRIMARY_NO_RESTRICTED_TRANSACTIONS) {
      mysql_mutex_lock(&notification_lock);
      is_primary_transaction_queue_applied = true;
      mysql_cond_broadcast(&notification_cond);
      mysql_mutex_unlock(&notification_lock);
      applier_module->queue_certification_enabling_packet();
    }
  }
  return 0;
}

bool Multi_primary_migration_action::persist_variable_values() {
  int error = 0;
  Set_system_variable set_system_variable;

  if ((error = set_system_variable
                   .set_persist_only_group_replication_single_primary_mode(
                       false))) {
    goto end; /* purecov: inspected */
  }

  if ((error =
           set_system_variable
               .set_persist_only_group_replication_enforce_update_everywhere_checks(
                   true))) {
    goto end; /* purecov: inspected */
  }

end:
  if (error) {
    execution_message_area.set_warning_message(
        "It was not possible to persist the configuration values for this "
        "mode. Check your server configuration for future server "
        "restarts and/or try to use SET PERSIST_ONLY.");
  }
  return error != 0;
}

void Multi_primary_migration_action::log_result_execution(bool aborted,
                                                          bool mode_changed) {
  if (!aborted) {
    if (execution_message_area.has_warning()) {
      std::string warning_message =
          "Mode switched to multi-primary with some reported warnings: " +
          execution_message_area.get_warning_message();
      execution_message_area.set_execution_message(
          Group_action_diagnostics::GROUP_ACTION_LOG_WARNING, warning_message);
    } else {
      execution_message_area.set_execution_message(
          Group_action_diagnostics::GROUP_ACTION_LOG_INFO,
          "Mode switched to multi-primary successfully.");
    }
  } else {
    if (execution_message_area.get_execution_message().empty()) {
      if (!action_killed) {
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
            "This operation was locally aborted and for that reason "
            "terminated.");
      } else {
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
            "This operation was locally killed and for that reason "
            "terminated.");
      }
      if (mode_changed) {
        execution_message_area.append_execution_message(
            " However the member is already configured to run in multi "
            "primary mode, but the configuration was not "
            "persisted."); /* purecov: inspected */
      }
    }
  }
}
