/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#include <stddef.h>
#include <algorithm>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <mysql/components/services/log_builtins.h>
#include "mutex_lock.h"
#include "my_dbug.h"
#include "plugin/group_replication/include/autorejoin.h"
#include "plugin/group_replication/include/gcs_event_handlers.h"
#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/observer_trans.h"
#include "plugin/group_replication/include/pipeline_stats.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/member_actions_handler.h"
#include "plugin/group_replication/include/plugin_handlers/metrics_handler.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_invocation_handler.h"
#include "plugin/group_replication/include/plugin_handlers/remote_clone_handler.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/include/plugin_messages/group_service_message.h"
#include "plugin/group_replication/include/plugin_messages/group_validation_message.h"
#include "plugin/group_replication/include/plugin_messages/sync_before_execution_message.h"
#include "plugin/group_replication/include/plugin_messages/transaction_prepared_message.h"
#include "plugin/group_replication/include/plugin_messages/transaction_with_guarantee_message.h"
#include "plugin/group_replication/include/services/system_variable/get_system_variable.h"

using std::vector;

Plugin_gcs_events_handler::Plugin_gcs_events_handler(
    Applier_module_interface *applier_module, Recovery_module *recovery_module,
    Compatibility_module *compatibility_module, ulong components_stop_timeout)
    : applier_module(applier_module),
      recovery_module(recovery_module),
      compatibility_manager(compatibility_module),
      stop_wait_timeout(components_stop_timeout) {
  this->temporary_states =
      new std::set<Group_member_info *, Group_member_info_pointer_comparator>();
  this->joiner_compatibility_status = new st_compatibility_types(INCOMPATIBLE);

#ifndef NDEBUG
  set_number_of_members_on_view_changed_to_10 = false;
  DBUG_EXECUTE_IF(
      "group_replication_set_number_of_members_on_view_changed_to_10",
      { set_number_of_members_on_view_changed_to_10 = true; };);
#endif
}

Plugin_gcs_events_handler::~Plugin_gcs_events_handler() {
  delete temporary_states;
  delete joiner_compatibility_status;
}

void Plugin_gcs_events_handler::on_message_received(
    const Gcs_message &message) const {
  metrics_handler->add_message_sent(message);

  const Plugin_gcs_message::enum_cargo_type message_type =
      Plugin_gcs_message::get_cargo_type(
          message.get_message_data().get_payload());

  const std::string message_origin = message.get_origin().get_member_id();
  Plugin_gcs_message *processed_message = nullptr;

  switch (message_type) {
    case Plugin_gcs_message::CT_TRANSACTION_MESSAGE:
      handle_transactional_message(message);
      break;

    case Plugin_gcs_message::CT_TRANSACTION_WITH_GUARANTEE_MESSAGE:
      handle_transactional_with_guarantee_message(message);
      break;

    case Plugin_gcs_message::CT_TRANSACTION_PREPARED_MESSAGE:
      handle_transaction_prepared_message(message);
      break;

    case Plugin_gcs_message::CT_SYNC_BEFORE_EXECUTION_MESSAGE:
      handle_sync_before_execution_message(message);
      break;

    case Plugin_gcs_message::CT_CERTIFICATION_MESSAGE:
      handle_certifier_message(message);
      break;

    case Plugin_gcs_message::CT_PIPELINE_STATS_MEMBER_MESSAGE:
      handle_stats_message(message);
      break;

    case Plugin_gcs_message::CT_MESSAGE_SERVICE_MESSAGE: {
      Group_service_message *service_message = new Group_service_message(
          message.get_message_data().get_payload(),
          message.get_message_data().get_payload_length());

      message_service_handler->add(service_message);
    } break;

      /**
        From this point messages are sent to message listeners and may be
        skipped Messages above are directly processed and/or for performance we
        do not want to add that extra weight.
      */

    case Plugin_gcs_message::CT_RECOVERY_MESSAGE:
      processed_message =
          new Recovery_message(message.get_message_data().get_payload(),
                               message.get_message_data().get_payload_length());
      if (!pre_process_message(processed_message, message_origin))
        handle_recovery_message(processed_message);
      delete processed_message;
      break;

    case Plugin_gcs_message::CT_SINGLE_PRIMARY_MESSAGE:
      processed_message = new Single_primary_message(
          message.get_message_data().get_payload(),
          message.get_message_data().get_payload_length());
      if (!pre_process_message(processed_message, message_origin))
        handle_single_primary_message(processed_message);
      delete processed_message;
      break;

    case Plugin_gcs_message::CT_GROUP_ACTION_MESSAGE:
      handle_group_action_message(message);
      break;

    case Plugin_gcs_message::CT_GROUP_VALIDATION_MESSAGE:
      processed_message = new Group_validation_message(
          message.get_message_data().get_payload(),
          message.get_message_data().get_payload_length());
      pre_process_message(processed_message, message_origin);
      delete processed_message;
      break;

    case Plugin_gcs_message::CT_RECOVERY_METADATA_MESSAGE:
      handle_recovery_metadata(message);
      break;

    default:
      break; /* purecov: inspected */
  }

  /*
   We need to see if a notification should be sent at this
   point in time because we may have received a recovery
   message that has updated our state.
  */
  notify_and_reset_ctx(m_notification_ctx);
}

bool Plugin_gcs_events_handler::pre_process_message(
    Plugin_gcs_message *plugin_message,
    const std::string &message_origin) const {
  bool skip_message = false;
  int error = group_events_observation_manager->before_message_handling(
      *plugin_message, message_origin, &skip_message);
  return (error || skip_message);
}

void Plugin_gcs_events_handler::handle_transactional_message(
    const Gcs_message &message) const {
  const Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  if ((member_status == Group_member_info::MEMBER_IN_RECOVERY ||
       member_status == Group_member_info::MEMBER_ONLINE) &&
      this->applier_module) {
    if (member_status == Group_member_info::MEMBER_IN_RECOVERY) {
      applier_module->get_pipeline_stats_member_collector()
          ->increment_transactions_delivered_during_recovery();
    }

    const unsigned char *payload_data = nullptr;
    size_t payload_size = 0;
    Plugin_gcs_message::get_first_payload_item_raw_data(
        message.get_message_data().get_payload(), &payload_data, &payload_size);

    this->applier_module->handle(payload_data, static_cast<ulong>(payload_size),
                                 GROUP_REPLICATION_CONSISTENCY_EVENTUAL,
                                 nullptr, key_transaction_data);
  } else {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MSG_DISCARDED);
    /* purecov: end */
  }
}

void Plugin_gcs_events_handler::handle_transactional_with_guarantee_message(
    const Gcs_message &message) const {
  const Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  if ((member_status == Group_member_info::MEMBER_IN_RECOVERY ||
       member_status == Group_member_info::MEMBER_ONLINE) &&
      this->applier_module) {
    if (member_status == Group_member_info::MEMBER_IN_RECOVERY) {
      applier_module->get_pipeline_stats_member_collector()
          ->increment_transactions_delivered_during_recovery();
    }

    const unsigned char *payload_data = nullptr;
    size_t payload_size = 0;
    Plugin_gcs_message::get_first_payload_item_raw_data(
        message.get_message_data().get_payload(), &payload_data, &payload_size);

    enum_group_replication_consistency_level consistency_level =
        Transaction_with_guarantee_message::decode_and_get_consistency_level(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());

    // Get ONLINE members that did receive this message.
    std::list<Gcs_member_identifier> *online_members =
        group_member_mgr->get_online_members_with_guarantees(
            message.get_origin());

    this->applier_module->handle(payload_data, static_cast<ulong>(payload_size),
                                 consistency_level, online_members,
                                 key_transaction_data);
  } else {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MSG_DISCARDED);
    /* purecov: end */
  }
}

void Plugin_gcs_events_handler::handle_transaction_prepared_message(
    const Gcs_message &message) const {
  if (this->applier_module == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MISSING_GRP_RPL_APPLIER);
    return;
    /* purecov: end */
  }

  Transaction_prepared_message transaction_prepared_message(
      message.get_message_data().get_payload(),
      message.get_message_data().get_payload_length());

  if (transaction_prepared_message.is_valid() == false) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MSG_DECODING_FAILED,
                 "Transaction_prepared_message",
                 transaction_prepared_message.get_error()->get_message());
    auto error_msg =
        "Failure when processing a received transaction prepared message from "
        "the communication layer.";
    Error_action_packet *error_action = new Error_action_packet(error_msg);
    this->applier_module->add_packet(error_action);
    return;
  }

  Transaction_prepared_action_packet *transaction_prepared_action =
      new Transaction_prepared_action_packet(
          transaction_prepared_message.get_tsid(),
          transaction_prepared_message.is_tsid_specified(),
          transaction_prepared_message.get_gno(), message.get_origin());
  this->applier_module->add_transaction_prepared_action_packet(
      transaction_prepared_action);
}

void Plugin_gcs_events_handler::handle_sync_before_execution_message(
    const Gcs_message &message) const {
  if (this->applier_module == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MISSING_GRP_RPL_APPLIER);
    return;
    /* purecov: end */
  }

  Sync_before_execution_message sync_before_execution_message(
      message.get_message_data().get_payload(),
      message.get_message_data().get_payload_length());

  Sync_before_execution_action_packet *sync_before_execution_action =
      new Sync_before_execution_action_packet(
          sync_before_execution_message.get_thread_id(), message.get_origin());
  this->applier_module->add_sync_before_execution_action_packet(
      sync_before_execution_action);
}

void Plugin_gcs_events_handler::handle_certifier_message(
    const Gcs_message &message) const {
  if (this->applier_module == nullptr) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_MISSING_GRP_RPL_APPLIER); /* purecov: inspected */
    return;                                           /* purecov: inspected */
  }

  Certifier_interface *certifier =
      this->applier_module->get_certification_handler()->get_certifier();

  const unsigned char *payload_data = nullptr;
  size_t payload_size = 0;
  Plugin_gcs_message::get_first_payload_item_raw_data(
      message.get_message_data().get_payload(), &payload_data, &payload_size);

  if (certifier->handle_certifier_data(payload_data,
                                       static_cast<ulong>(payload_size),
                                       message.get_origin())) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_CERTIFIER_MSSG_PROCESS_ERROR); /* purecov: inspected */
  }
}

void Plugin_gcs_events_handler::handle_recovery_message(
    Plugin_gcs_message *processed_message) const {
  Recovery_message *recovery_message = (Recovery_message *)processed_message;

  std::string member_uuid = recovery_message->get_member_uuid();

  bool is_local = !member_uuid.compare(local_member_info->get_uuid());
  if (is_local) {
    // Only change member status if member is still on recovery.
    Group_member_info::Group_member_status member_status =
        local_member_info->get_recovery_status();
    if (member_status != Group_member_info::MEMBER_IN_RECOVERY) {
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_SRV_NOT_ONLINE,
                   Group_member_info::get_member_status_string(
                       member_status)); /* purecov: inspected */
      return;                           /* purecov: inspected */
    }

    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SRV_ONLINE);

    /*
     The member is declared as online upon receiving this message

     A notification may be flagged and eventually triggered when
     the on_message handle is finished.
    */
    group_member_mgr->update_member_status(
        member_uuid, Group_member_info::MEMBER_ONLINE, m_notification_ctx);

    /*
     Take View_change_log_event transaction into account, that
     despite being queued on applier channel was applied through
     recovery channel.
    */
    if (group_member_mgr->get_number_of_members() != 1) {
      Pipeline_stats_member_collector *pipeline_stats =
          applier_module->get_pipeline_stats_member_collector();
      pipeline_stats->decrement_transactions_waiting_apply();
    }

    /*
      unblock threads waiting for the member to become ONLINE
    */
    terminate_wait_on_start_process();

    /**
      Re-check compatibility, members may leave during recovery.
      Disable the read mode in the server if the member is:
      - joining
      - doesn't have a higher possible incompatible version
      - We are not on Primary mode.
    */
    disable_read_mode_for_compatible_members(true);
  } else {
    Group_member_info member_info;
    if (!group_member_mgr->get_group_member_info(member_uuid, member_info)) {
      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_MEM_ONLINE,
                   member_info.get_hostname().c_str(), member_info.get_port());

      /*
       The member is declared as online upon receiving this message
       We need to run this before running update_recovery_process

       A notification may be flagged and eventually triggered when
       the on_message handle is finished.
      */
      group_member_mgr->update_member_status(
          member_uuid, Group_member_info::MEMBER_ONLINE, m_notification_ctx);

      if (local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_IN_RECOVERY) {
        /*
          Inform recovery of a possible new donor
        */
        recovery_module->update_recovery_process(false, false);
      }
    }
  }

  /*
   Check if we were waiting for some server to recover to
   elect a new leader.

   Following line protects against servers joining the group
   while the bootstrapped node has not yet finished recovery.
   Therefore, it is going to become primary when it finishes recovery.
   */
  std::string no_primary("");
  this->handle_leader_election_if_needed(DEAD_OLD_PRIMARY, no_primary);
}

void Plugin_gcs_events_handler::handle_stats_message(
    const Gcs_message &message) const {
  if (this->applier_module == nullptr) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_MISSING_GRP_RPL_APPLIER); /* purecov: inspected */
    return;                                           /* purecov: inspected */
  }

  this->applier_module->get_flow_control_module()->handle_stats_data(
      message.get_message_data().get_payload(),
      message.get_message_data().get_payload_length(),
      message.get_origin().get_member_id());
}

void Plugin_gcs_events_handler::handle_single_primary_message(
    Plugin_gcs_message *processed_message) const {
  if (this->applier_module == nullptr) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_MISSING_GRP_RPL_APPLIER); /* purecov: inspected */
    return;                                           /* purecov: inspected */
  }

  Single_primary_message *single_primary_message =
      (Single_primary_message *)processed_message;

  if (single_primary_message->get_single_primary_message_type() ==
      Single_primary_message::SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE) {
    Single_primary_action_packet *single_primary_action =
        new Single_primary_action_packet(
            Single_primary_action_packet::QUEUE_APPLIED);
    primary_election_handler->set_election_running(false);
    this->applier_module->add_single_primary_action_packet(
        single_primary_action);
  }
  if (single_primary_message->get_single_primary_message_type() ==
      Single_primary_message::SINGLE_PRIMARY_PRIMARY_ELECTION) {
    primary_election_handler->handle_primary_election_message(
        single_primary_message, &m_notification_ctx);
  }
}

void Plugin_gcs_events_handler::handle_group_action_message(
    const Gcs_message &message) const {
  if (group_action_coordinator == nullptr) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_MISSING_GRP_RPL_ACTION_COORDINATOR); /* purecov: inspected */
    return;                                             /* purecov: inspected */
  }

  Group_action_message::enum_action_message_type action_message_type =
      Group_action_message::get_action_type(
          message.get_message_data().get_payload());

  Group_action_message *group_action_message = nullptr;
  switch (action_message_type) {
    case Group_action_message::ACTION_MULTI_PRIMARY_MESSAGE:
    case Group_action_message::ACTION_PRIMARY_ELECTION_MESSAGE:
    case Group_action_message::ACTION_SET_COMMUNICATION_PROTOCOL_MESSAGE:
      group_action_message = new Group_action_message(
          message.get_message_data().get_payload(),
          message.get_message_data().get_payload_length());
      break;
    default:
      break; /* purecov: inspected */
  }

  if (!pre_process_message(group_action_message,
                           message.get_origin().get_member_id())) {
    group_action_coordinator->handle_action_message(
        group_action_message, message.get_origin().get_member_id());
  }
  delete group_action_message;
}

void Plugin_gcs_events_handler::handle_recovery_metadata(
    const Gcs_message &message) const {
  bool error{false};
  if (this->applier_module == nullptr) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_MISSING_GRP_RPL_APPLIER); /* purecov: inspected */
    return;
  }

  /*
    Initialize Recovery_metadata_message which will save position of received
    recovery metadata message and further each get_decoded_*() can be used to
    decode their payload type.
  */
  Recovery_metadata_message *recovery_metadata_message =
      new Recovery_metadata_message(
          message.get_message_data().get_payload(),
          message.get_message_data().get_payload_length());

  // Get View ID from received metadata.
  std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
            std::reference_wrapper<std::string>>
      payload_view_id_error = recovery_metadata_message->get_decoded_view_id();

  if (payload_view_id_error.first !=
      Recovery_metadata_message::enum_recovery_metadata_message_error::
          RECOVERY_METADATA_MESSAGE_OK) {
    delete recovery_metadata_message;
    return;
  }

  /**
    Check if the View_ID in the received recovery metadata matches with the
    locally stored View_ID on the joiner. This locally stored View_ID is on
    which joining member joined the group.

    - View_ID matches it's JOINER:
      a. It means the joiner has received the metadata and it can use it to
         recover and come ONLINE.
      b. Also local copy of View_ID stored in
         Recovery_metadata_joiner_information can be removed.
      c. It also creates a copy of recovery metadata which is processed later by
         recovery thread. GCS thread which holds current copy of recovery
         metadata may delete it before recovery process it, so another copy
         needs to be created.
      d. Awake Recovery module waiting for Metadata to be received.

    - View_ID does not match it's SENDER:
      e. The recovery metadata broadcast was successful now local copy of
         recovery metadata is deleted.

    - If there is error, cleanup and member leaves the group.
  */
  std::string metadata_view_id{payload_view_id_error.second.get()};
  // JOINER CODE
  if (recovery_metadata_module->is_joiner_recovery_metadata(metadata_view_id)) {
    std::string exit_state_action_abort_log_message{
        "Error in joiner processing received Recovery Metadata Message."};
    /*
      View-id and valid sender list of the joiner was temporary stored to
      uniquely identify the recovery metadata. Since joiner has received it's
      metadata now  this information can now be deleted.
    */
    recovery_metadata_module->delete_joiner_view_id();

    // Get payload send message error from received recovery metadata.
    std::pair<
        Recovery_metadata_message::enum_recovery_metadata_message_error,
        Recovery_metadata_message::Recovery_metadata_message_payload_error>
        payload_message_send_error =
            recovery_metadata_message->get_decoded_message_error();

    if (payload_message_send_error.first !=
        Recovery_metadata_message::enum_recovery_metadata_message_error::
            RECOVERY_METADATA_MESSAGE_OK) {
      error = true;
    }

    if (!error) {
      Recovery_metadata_message::Recovery_metadata_message_payload_error
          payload_send_error{payload_message_send_error.second};
      if (payload_send_error ==
          Recovery_metadata_message::RECOVERY_METADATA_ERROR) {
        error = true;
        LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_SEND_ERROR);
        exit_state_action_abort_log_message.assign(
            "The group was unable to send the Recovery Metadata to a joining "
            "member.");
      } else {
        /*
          Save a copy of recovery metadata message, so that it can be
          processed later by recovery thread, and before it gets deleted
          by GCS thread.
        */
        if (recovery_metadata_message
                ->save_copy_of_recovery_metadata_payload()) {
          error = true;
          LogPluginErr(ERROR_LEVEL,
                       ER_GROUP_REPLICATION_METADATA_SAVE_RECOVERY_COPY);
        }

        /*
          Set recovery_metadata_message pointer for recovery thread to read and
          process Recovery Metadata.
        */
        if (recovery_module->set_recovery_metadata_message(
                recovery_metadata_message)) {
          error = true;
          LogPluginErr(ERROR_LEVEL,
                       ER_GROUP_REPLICATION_METADATA_SET_IN_RECOVERY_FAILED);
        }
      }
    }

    // awake blocked recovery on joiner even if there is error
    recovery_module->awake_recovery_metadata_suspension(error);
    if (error) {
      leave_group_on_recovery_metadata_error(
          exit_state_action_abort_log_message);
    }

  } else {
    // SENDER
    // If member is part of the valid sender group then this message indicates
    // that the message has been successfully delivered to the joiner. Which
    // means now the sender can delete the saved recovery metadata. To delete
    // the recovery metadata we need to add a
    // "Recovery_metadata_processing_packets" packet in the applier-pipeline
    // with the VIEW_ID. Since add of the Recovery metadata is handled by the
    // applier-pipeline and delete of Recovery metadata is also being done from
    // the applier pipeline we need not synchronize the add and delete
    // operations.
    Recovery_metadata_processing_packets *metadata_packet =
        new Recovery_metadata_processing_packets();
    metadata_packet->m_view_id_to_be_deleted.push_back(metadata_view_id);
    applier_module->add_metadata_processing_packet(metadata_packet);
    delete recovery_metadata_message;
  }

  if (error) {
    recovery_metadata_module->delete_joiner_view_id();
    delete recovery_metadata_message;
  }
}

void Plugin_gcs_events_handler::leave_group_on_recovery_metadata_error(
    std::string error_message) const {
  leave_group_on_failure::mask leave_actions;
  leave_actions.set(leave_group_on_failure::STOP_APPLIER, true);
  leave_actions.set(leave_group_on_failure::CLEAN_GROUP_MEMBERSHIP, true);
  leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
  leave_group_on_failure::leave(leave_actions, 0, nullptr,
                                error_message.c_str());
}

void Plugin_gcs_events_handler::on_suspicions(
    const std::vector<Gcs_member_identifier> &members,
    const std::vector<Gcs_member_identifier> &unreachable) const {
  if (members.empty() && unreachable.empty())  // nothing to do
    return;                                    /* purecov: inspected */

  assert(members.size() >= unreachable.size());

  std::vector<Gcs_member_identifier> tmp_unreachable(unreachable);
  std::vector<Gcs_member_identifier>::const_iterator mit;
  std::vector<Gcs_member_identifier>::iterator uit;

  if (!members.empty()) {
    for (mit = members.begin(); mit != members.end(); mit++) {
      Gcs_member_identifier member = *mit;
      Group_member_info member_info;

      if (group_member_mgr->get_group_member_info_by_member_id(member,
                                                               member_info)) {
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEMBER_INFO_DOES_NOT_EXIST,
                     "by the Gcs_member_identifier",
                     member.get_member_id().c_str(),
                     "REACHABLE/UNREACHABLE notification from group "
                     "communication engine");
        continue; /* purecov: inspected */
      }

      uit = std::find(tmp_unreachable.begin(), tmp_unreachable.end(), member);
      if (uit != tmp_unreachable.end()) {
        if (!member_info.is_unreachable()) {
          LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEM_UNREACHABLE,
                       member_info.get_hostname().c_str(),
                       member_info.get_port());
          // flag as a member having changed state
          m_notification_ctx.set_member_state_changed();
          group_member_mgr->set_member_unreachable(member_info.get_uuid());
        }
        // remove to not check again against this one
        tmp_unreachable.erase(uit);
      } else {
        if (member_info.is_unreachable()) {
          LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEM_REACHABLE,
                       member_info.get_hostname().c_str(),
                       member_info.get_port());
          /* purecov: begin inspected */
          // flag as a member having changed state
          m_notification_ctx.set_member_state_changed();
          group_member_mgr->set_member_reachable(member_info.get_uuid());
          /* purecov: end */
        }
      }
    }
  }

  if ((members.size() - unreachable.size()) <= (members.size() / 2)) {
    if (!group_partition_handler->get_timeout_on_unreachable())
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SRV_BLOCKED);
    else
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SRV_BLOCKED_FOR_SECS,
                   group_partition_handler->get_timeout_on_unreachable());

    if (!group_partition_handler->is_partition_handler_running() &&
        !group_partition_handler->is_partition_handling_terminated())
      group_partition_handler->launch_partition_handler_thread();

    // flag as having lost quorum
    m_notification_ctx.set_quorum_lost();
  } else {
    /*
      This code is present on on_view_changed and on_suspicions as no assumption
      can be made about the order in which these methods are invoked.
    */
    if (group_partition_handler->is_member_on_partition()) {
      if (group_partition_handler->abort_partition_handler_if_running()) {
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_CHANGE_GRP_MEM_NOT_PROCESSED);
      } else {
        /* If it was not running or we canceled it in time */
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEMBER_CONTACT_RESTORED);
      }
    }
  }
  notify_and_reset_ctx(m_notification_ctx);
}

void Plugin_gcs_events_handler::log_messages_during_member_leave(
    const Gcs_view &new_view) const {
  std::string leaving_members_string;
  std::string primary_member_host;
  const std::vector<Gcs_member_identifier> &leaving_members =
      new_view.get_leaving_members();

  get_hosts_from_view(leaving_members, leaving_members_string,
                      primary_member_host);

  LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEMBER_REMOVED,
               leaving_members_string.c_str());

  if (!primary_member_host.empty())
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_PRIMARY_MEMBER_LEFT_GRP,
                 primary_member_host.c_str());

  {
    /* Atleast one member is present in the group that supports only VCLE */
    bool vcle_only_member_present = false;
    /* Atleast one member left the group that supported only VCLE */
    bool vcle_only_member_left = false;
    Member_version version_removing_vcle(MEMBER_VERSION_REMOVING_VCLE);
    Group_member_info_list *all_members_info =
        group_member_mgr->get_all_members();
    for (Group_member_info *member : *all_members_info) {
      if (member->get_member_version() < version_removing_vcle) {
        vcle_only_member_left = true;
        vcle_only_member_present =
            (vcle_only_member_present ||
             std::find(leaving_members.begin(), leaving_members.end(),
                       member->get_gcs_member_id()) == leaving_members.end());
      }
      delete member;
    }
    if (vcle_only_member_left && !vcle_only_member_present) {
      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_VCLE_NOT_BEING_LOGGED);
    }
    delete all_members_info;
  }
}

void Plugin_gcs_events_handler::log_members_joining_message(
    const Gcs_view &new_view) const {
  std::string members_joining;
  std::string primary_member_host;

  get_hosts_from_view(new_view.get_joined_members(), members_joining,
                      primary_member_host);

  LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_MEMBER_ADDED,
               members_joining.c_str());
}

void Plugin_gcs_events_handler::get_hosts_from_view(
    const std::vector<Gcs_member_identifier> &members, std::string &all_hosts,
    std::string &primary_host) const {
  std::stringstream hosts_string;
  std::stringstream primary_string;
  std::vector<Gcs_member_identifier>::const_iterator all_members_it =
      members.begin();

  while (all_members_it != members.end()) {
    Group_member_info member_info;
    const bool member_not_found =
        group_member_mgr->get_group_member_info_by_member_id((*all_members_it),
                                                             member_info);
    all_members_it++;

    if (member_not_found) continue;

    hosts_string << member_info.get_hostname() << ":" << member_info.get_port();

    /**
     Check in_primary_mode has been added for safety.
     Since primary role is in single-primary mode.
    */
    if (member_info.in_primary_mode() &&
        member_info.get_role() == Group_member_info::MEMBER_ROLE_PRIMARY) {
      if (primary_string.rdbuf()->in_avail() != 0) primary_string << ", ";
      primary_string << member_info.get_hostname() << ":"
                     << member_info.get_port();
    }

    if (all_members_it != members.end()) {
      hosts_string << ", ";
    }
  }
  all_hosts.assign(hosts_string.str());
  primary_host.assign(primary_string.str());
}

void Plugin_gcs_events_handler::on_view_changed(
    const Gcs_view &new_view, const Exchanged_data &exchanged_data) const {
  bool is_leaving = is_member_on_vector(new_view.get_leaving_members(),
                                        local_member_info->get_gcs_member_id());

  bool is_primary =
      (local_member_info->in_primary_mode() &&
       local_member_info->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY);

  bool is_joining = is_member_on_vector(new_view.get_joined_members(),
                                        local_member_info->get_gcs_member_id());

  bool skip_election = false;
  enum_primary_election_mode election_mode = DEAD_OLD_PRIMARY;
  std::string suggested_primary("");
  // Was member expelled from the group due to network failures?
  if (this->was_member_expelled_from_group(new_view)) {
    assert(is_leaving);
    group_events_observation_manager->after_view_change(
        new_view.get_joined_members(), new_view.get_leaving_members(),
        new_view.get_members(), is_leaving, &skip_election, &election_mode,
        suggested_primary);
    goto end;
  }

  // An early error on the applier can render the join invalid
  if (is_joining &&
      local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_ERROR &&
      !autorejoin_module->is_autorejoin_ongoing()) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_EXIT_PLUGIN_ERROR);
    gcs_module->notify_of_view_change_cancellation(
        GROUP_REPLICATION_CONFIGURATION_ERROR);
  } else {
    /*
      This code is present on on_view_changed and on_suspicions as no assumption
      can be made about the order in which these methods are invoked.
    */
    if (!is_leaving && group_partition_handler->is_member_on_partition()) {
      if (group_partition_handler->abort_partition_handler_if_running()) {
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_CHANGE_GRP_MEM_NOT_PROCESSED);
        goto end;
      } else {
        /* If it was not running or we canceled it in time */
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEMBER_CONTACT_RESTORED);
      }
    }

    /*
      Maybe on_suspicions we already executed the above block but it was too
      late. No point in repeating the message, but we need to break the view
      install.
    */
    if (!is_leaving &&
        group_partition_handler->is_partition_handling_terminated())
      goto end;

    if (!is_leaving && new_view.get_leaving_members().size() > 0)
      log_messages_during_member_leave(new_view);

    // update the Group Manager with all the received states
    if (update_group_info_manager(new_view, exchanged_data, is_joining,
                                  is_leaving) &&
        is_joining) {
      gcs_module->notify_of_view_change_cancellation();
      return;
    }

    if (!is_joining && new_view.get_joined_members().size() > 0)
      log_members_joining_message(new_view);

    // enable conflict detection if someone on group have it enabled
    if (local_member_info->in_primary_mode() &&
        group_member_mgr->is_conflict_detection_enabled()) {
      Certifier_interface *certifier =
          this->applier_module->get_certification_handler()->get_certifier();
      certifier->enable_conflict_detection();
    }

    // Inform any interested handler that the view changed
    View_change_pipeline_action *vc_action =
        new View_change_pipeline_action(is_leaving);

    applier_module->handle_pipeline_action(vc_action);
    delete vc_action;

    // Update any recovery running process and handle state changes
    this->handle_leaving_members(new_view, is_joining, is_leaving);

    // Handle joining members
    this->handle_joining_members(new_view, is_joining, is_leaving);

    if (is_leaving) gcs_module->leave_coordination_member_left();

    // Signal that the injected view was delivered
    if (gcs_module->is_injected_view_modification())
      gcs_module->notify_of_view_change_end();

    group_events_observation_manager->after_view_change(
        new_view.get_joined_members(), new_view.get_leaving_members(),
        new_view.get_members(), is_leaving, &skip_election, &election_mode,
        suggested_primary);

    // Handle leader election if needed
    if (!skip_election && !is_leaving) {
      this->handle_leader_election_if_needed(election_mode, suggested_primary);
    }
  }

  if (!is_leaving) {
    disable_read_mode_for_compatible_members();
    LogPluginErr(
        SYSTEM_LEVEL, ER_GRP_RPL_MEMBER_CHANGE,
        group_member_mgr->get_string_current_view_active_hosts().c_str(),
        new_view.get_view_id().get_representation().c_str());
  } else {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_MEMBER_LEFT_GRP);
  }

end:
  /* if I am the primary and I am leaving, notify about role change */
  if (is_leaving && is_primary) {
    group_member_mgr->update_member_role(
        local_member_info->get_uuid(), Group_member_info::MEMBER_ROLE_SECONDARY,
        m_notification_ctx);
  }

  /* flag view change */
  m_notification_ctx.set_view_changed();
  if (is_leaving)
    /*
      The leave view is an optimistic and local view.
      Therefore its ID is not meaningful, since it is not
      a global one.
     */
    m_notification_ctx.set_view_id("");
  else
    m_notification_ctx.set_view_id(new_view.get_view_id().get_representation());

  /* trigger notification */
  notify_and_reset_ctx(m_notification_ctx);
}

bool Plugin_gcs_events_handler::was_member_expelled_from_group(
    const Gcs_view &view) const {
  DBUG_TRACE;
  bool result = false;

  if (view.get_error_code() == Gcs_view::MEMBER_EXPELLED) {
    result = true;
    const char *exit_state_action_abort_log_message =
        "Member was expelled from the group due to network failures.";
    leave_group_on_failure::mask leave_actions;
    leave_actions.set(leave_group_on_failure::ALREADY_LEFT_GROUP, true);
    leave_actions.set(leave_group_on_failure::CLEAN_GROUP_MEMBERSHIP, true);
    leave_actions.set(leave_group_on_failure::STOP_APPLIER, true);
    leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
    leave_actions.set(leave_group_on_failure::HANDLE_AUTO_REJOIN, true);
    leave_group_on_failure::leave(leave_actions, ER_GRP_RPL_MEMBER_EXPELLED,
                                  &m_notification_ctx,
                                  exit_state_action_abort_log_message);
  }

  return result;
}

void Plugin_gcs_events_handler::handle_leader_election_if_needed(
    enum_primary_election_mode election_mode,
    std::string &suggested_primary) const {
  /*
    Can we get here when a change to multi master is cancelled and is being
    undone? Yes but only on situations where the action was killed or the member
    is stopping that will always result in a plugin restart.
  */
  if (election_mode == DEAD_OLD_PRIMARY &&
      !local_member_info->in_primary_mode())
    return;

  primary_election_handler->execute_primary_election(
      suggested_primary, election_mode, &m_notification_ctx);
}

int Plugin_gcs_events_handler::update_group_info_manager(
    const Gcs_view &new_view, const Exchanged_data &exchanged_data,
    bool is_joining, bool is_leaving) const {
  int error = 0;

  // update the Group Manager with all the received states
  Group_member_info_list to_update(
      (Malloc_allocator<Group_member_info *>(key_group_member_info)));

  if (!is_leaving) {
    // Process local state of exchanged data.
    if ((error = process_local_exchanged_data(exchanged_data, is_joining)))
      goto err;

    to_update.insert(to_update.end(), temporary_states->begin(),
                     temporary_states->end());

    // Clean-up members that are leaving
    vector<Gcs_member_identifier> leaving = new_view.get_leaving_members();
    vector<Gcs_member_identifier>::iterator left_it;
    Group_member_info_list_iterator to_update_it;
    for (left_it = leaving.begin(); left_it != leaving.end(); left_it++) {
      for (to_update_it = to_update.begin(); to_update_it != to_update.end();
           to_update_it++) {
        if ((*left_it) == (*to_update_it)->get_gcs_member_id()) {
          /* purecov: begin inspected */
          delete (*to_update_it);
          to_update.erase(to_update_it);
          break;
          /* purecov: end */
        }
      }
    }
  }
  group_member_mgr->update(&to_update);
  temporary_states->clear();

err:
  if (error) {
    // Clean up temporary states.
    std::set<Group_member_info *,
             Group_member_info_pointer_comparator>::iterator
        temporary_states_it;
    for (temporary_states_it = temporary_states->begin();
         temporary_states_it != temporary_states->end();
         temporary_states_it++) {
      delete (*temporary_states_it);
    }
    temporary_states->clear();
  }

  assert(temporary_states->size() == 0);
  return error;
}

View_change_packet *prepare_view_change_packet(const Gcs_view &new_view) {
  std::string view_id = new_view.get_view_id().get_representation();

  bool need_vcle{false};  // Check if all members of the group support member
                          // join without VCLE or not
  std::vector<Gcs_member_identifier>
      online_members;  // Online members are also valid senders

  Member_version version_removing_vcle(MEMBER_VERSION_REMOVING_VCLE);
  Group_member_info_list *all_members_info =
      group_member_mgr->get_all_members();
  for (Group_member_info *member : *all_members_info) {
    if (member->get_member_version() < version_removing_vcle) {
      need_vcle = true;
    }
    if (member->get_recovery_status() == Group_member_info::MEMBER_ONLINE) {
      online_members.push_back(member->get_gcs_member_id());
    }
    delete member;
  }
  delete all_members_info;
  View_change_packet *view_change_packet =
      new View_change_packet(view_id, need_vcle);
  std::copy(new_view.get_joined_members().begin(),
            new_view.get_joined_members().end(),
            std::back_inserter(view_change_packet->m_members_joining_in_view));
  std::copy(online_members.begin(), online_members.end(),
            std::back_inserter(view_change_packet->m_valid_sender_list));

  return view_change_packet;
}

void Plugin_gcs_events_handler::handle_joining_members(const Gcs_view &new_view,
                                                       bool is_joining,
                                                       bool is_leaving) const {
  // nothing to do here
  size_t number_of_members = new_view.get_members().size();
  if (number_of_members == 0 || is_leaving) {
    return;
  }
  size_t number_of_joining_members = new_view.get_joined_members().size();
  size_t number_of_leaving_members = new_view.get_leaving_members().size();

  /*
   If we are joining, 3 scenarios exist:
   1) We are incompatible with the group so we leave
   2) We are alone so we declare ourselves online
   3) We are in a group and recovery must happen
  */
  if (is_joining) {
    int error = 0;
    if ((error = check_group_compatibility(number_of_members))) {
      gcs_module->notify_of_view_change_cancellation(error);
      return;
    }

    /**
     On the joining list there can be 3 types of members:
      1) ONLINE/RECOVERING members coming from old views where this member
         was not present;
      2) new joining members that still have their status as OFFLINE;
      3) old members that were expelled and did auto-rejoin, that have
         their status as ERROR.

     As so, for members with state OFFLINE and ERROR, their state is changed
     to RECOVERING after member compatibility with group is checked.
    */
#if !defined(NDEBUG)
    if (autorejoin_module->is_autorejoin_ongoing()) {
      assert(local_member_info->get_recovery_status() ==
             Group_member_info::MEMBER_ERROR);
    } else {
      assert(local_member_info->get_recovery_status() ==
             Group_member_info::MEMBER_OFFLINE);
    }
#endif

    /*
      Only declare the view delivery complete after the above asserts,
      this will allow check them while join and automatic rejoin are
      still ongoing.
    */
    gcs_module->notify_of_view_change_end();

    update_member_status(
        new_view.get_joined_members(), Group_member_info::MEMBER_IN_RECOVERY,
        Group_member_info::MEMBER_OFFLINE, Group_member_info::MEMBER_END);
    update_member_status(
        new_view.get_joined_members(), Group_member_info::MEMBER_IN_RECOVERY,
        Group_member_info::MEMBER_ERROR, Group_member_info::MEMBER_END);

    /** Is an election running while I'm joining?*/
    primary_election_handler->set_election_running(
        is_group_running_a_primary_election());

    /**
      Set the read mode if not set during start (auto-start)
    */
    if (enable_server_read_mode("(GR) join group")) {
      /*
        The notification will be triggered in the top level handle function
        that calls this one. In this case, the on_view_changed handle.
      */
      leave_group_on_failure::mask leave_actions;
      leave_actions.set(leave_group_on_failure::SKIP_SET_READ_ONLY, true);
      leave_actions.set(leave_group_on_failure::SKIP_LEAVE_VIEW_WAIT, true);
      leave_actions.set(leave_group_on_failure::CLEAN_GROUP_MEMBERSHIP, true);
      leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
      leave_group_on_failure::leave(leave_actions,
                                    ER_GRP_RPL_SUPER_READ_ONLY_ACTIVATE_ERROR,
                                    &m_notification_ctx, "");
      set_plugin_is_setting_read_mode(false);

      return;
    } else {
      set_plugin_is_setting_read_mode(false);
    }

    /**
      On the joining member log an error when group contains more members than
      auto_increment_increment variable.
    */
    ulong auto_increment_increment = get_auto_increment_increment();

    if (!local_member_info->in_primary_mode() &&
        new_view.get_members().size() > auto_increment_increment) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_EXCEEDS_AUTO_INC_VALUE,
                   new_view.get_members().size(), auto_increment_increment);
    }

    /*
     During the view change, a suspension packet is sent to the applier module
     so all posterior transactions inbound are not applied, but queued, until
     the member finishes recovery.
    */
    applier_module->add_suspension_packet();

    /*
     Marking the view in the joiner since the incoming event from the donor
     is discarded in the Recovery process.
    */
    std::string view_id = new_view.get_view_id().get_representation();
    View_change_packet *view_change_packet =
        prepare_view_change_packet(new_view);

    recovery_module->set_vcle_enabled(view_change_packet->m_need_vcle);

    if (number_of_members > 1) {
      if (!view_change_packet->m_need_vcle &&
          view_change_packet->m_valid_sender_list.size() == 0) {
        delete view_change_packet;
        LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_NO_VALID_DONOR);
        leave_group_on_recovery_metadata_error(
            "There are no valid recovery metadata donors.");
        return;
      }
      // set joiner parameters
      if (!view_change_packet->m_need_vcle) {
        recovery_module->suspend_recovery_metadata();
        // We need to save view-id and valid sender list to identify the
        // recovery metadata once received.
        recovery_metadata_module->store_joiner_view_id_and_valid_senders(
            view_id, view_change_packet->m_valid_sender_list);
      }
    }
    // Add a view-change packet to the applier-pipeline.
    applier_module->add_view_change_packet(view_change_packet);

    /*
     Chose what is the strategy for recovery.
     Note that even if clone is chosen, if an error occurs on its launch,
     incremental recovery is again selected as the default choice.
    */
    Remote_clone_handler::enum_clone_check_result recovery_strategy =
        Remote_clone_handler::DO_RECOVERY;

    // The check is not needed if the member is alone
    if (number_of_members > 1)
      recovery_strategy = remote_clone_handler->check_clone_preconditions();

    if (Remote_clone_handler::DO_CLONE == recovery_strategy) {
      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_RECOVERY_STRAT_CHOICE,
                   "Cloning from a remote group donor.");
      /*
       Launch the clone process. It will configure SSL options and the list
       of allowed donors.
       When terminated, the clone process will restart the server.
       The whole start join process is still done as an error on cloning can
       mean we fall back to incremental recovery.
      */
      if (remote_clone_handler->clone_server(
              new_view.get_group_id().get_group_id(),
              new_view.get_view_id().get_representation())) {
        /* purecov: begin inspected */
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_RECOVERY_STRAT_FALLBACK,
                     "Incremental Recovery.");
        recovery_strategy = Remote_clone_handler::DO_RECOVERY;
        /* purecov: end */
      }
    }

    if (Remote_clone_handler::DO_RECOVERY == recovery_strategy) {
      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_RECOVERY_STRAT_CHOICE,
                   "Incremental recovery from a group donor");
      /*
       Launch the recovery thread so we can receive missing data and the
       certification information needed to apply the transactions queued after
       this view change.

       Recovery receives a view id, as a means to identify logically on joiners
       and donors alike where this view change happened in the data. With that
       info we can then ask for the donor to give the member all the data until
       this point in the data, and the certification information for all the
       data that comes next.

       When alone, the server will go through Recovery to wait for the
       consumption of his applier relay log that may contain transactions from
       previous executions.
      */
      recovery_module->start_recovery(
          new_view.get_group_id().get_group_id(),
          new_view.get_view_id().get_representation());
    } else if (Remote_clone_handler::CHECK_ERROR == recovery_strategy ||
               Remote_clone_handler::NO_RECOVERY_POSSIBLE ==
                   recovery_strategy) {
      if (Remote_clone_handler::NO_RECOVERY_POSSIBLE == recovery_strategy)
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_NO_POSSIBLE_RECOVERY);
      else {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_RECOVERY_EVAL_ERROR, "");
        /* purecov: end */
      }

      /*
        The notification will be triggered in the top level handle function
        that calls this one. In this case, the on_view_changed handle.
      */
      leave_group_on_failure::mask leave_actions;
      leave_actions.set(leave_group_on_failure::SKIP_LEAVE_VIEW_WAIT, true);
      leave_actions.set(leave_group_on_failure::CLEAN_GROUP_MEMBERSHIP, true);
      leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
      leave_group_on_failure::leave(leave_actions, 0, &m_notification_ctx, "");
      return;
    }
  }
  /*
    The condition
      number_of_joining_members == 0 && number_of_leaving_members == 0
    is needed due to the following scenario:
    We have a group with 2 members, one does crash (M2), and the group
    blocks with M1 ONLINE and M2 UNREACHABLE.
    Then M2 rejoins and the group unblocks.
    When M2 rejoins the group, from M2 perspective it is joining
    the group, that is, it does receive a view (V3) on which it is
    marked as a joining member.
    But from M1 perspective, M2 may never left, so the view delivered
    (V3) has the same members as V2, that is, M1 and M2, without joining
    members, thence we need to consider that condition and log that view.
  */
  else if (number_of_joining_members > 0 ||
           (number_of_joining_members == 0 && number_of_leaving_members == 0)) {
    /**
     On the joining list there can be 3 types of members:
      1) ONLINE/RECOVERING members coming from old views where this member
         was not present;
      2) new joining members that still have their status as OFFLINE;
      3) old members that were expelled and did auto-rejoin, that have
         their status as ERROR.

     As so, for members with state OFFLINE and ERROR, their state is changed
     to RECOVERING after member compatibility with group is checked.
    */
    update_member_status(
        new_view.get_joined_members(), Group_member_info::MEMBER_IN_RECOVERY,
        Group_member_info::MEMBER_OFFLINE, Group_member_info::MEMBER_END);
    update_member_status(
        new_view.get_joined_members(), Group_member_info::MEMBER_IN_RECOVERY,
        Group_member_info::MEMBER_ERROR, Group_member_info::MEMBER_END);

    /**
     If not a joining member, all members should record on their own binlogs a
     marking event that identifies the frontier between the data the joining
     member was to receive and the data it should queue.
     The joining member can then wait for this event to know it was all the
     needed data.

     This packet will also pass in the certification process at this exact
     frontier giving us the opportunity to gather the necessary certification
     information to certify the transactions that will come after this view
     change. If selected as a donor, this info will also be sent to the joiner.

     Associated to this process, we collect and intersect the executed GTID sets
     of all ONLINE members so we can cut the certification info to gather and
     transmit to the minimum.
    */
    View_change_packet *view_change_packet =
        prepare_view_change_packet(new_view);
    collect_members_executed_sets(view_change_packet);
    applier_module->add_view_change_packet(view_change_packet);

    if (number_of_joining_members > 0) {
      std::pair<std::string, std::string> action_initiator_and_description;
      if (group_action_coordinator->is_group_action_running(
              action_initiator_and_description))
        LogPluginErr(WARNING_LEVEL,
                     ER_GRP_RPL_JOINER_EXIT_WHEN_GROUP_ACTION_RUNNING,
                     action_initiator_and_description.second.c_str(),
                     action_initiator_and_description.first.c_str());
    }
  }
}

void Plugin_gcs_events_handler::handle_leaving_members(const Gcs_view &new_view,
                                                       bool is_joining,
                                                       bool is_leaving) const {
  Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();

  bool members_left = (new_view.get_leaving_members().size() > 0);

  // if the member is joining or not in recovery, no need to update the process
  if (!is_joining && member_status == Group_member_info::MEMBER_IN_RECOVERY) {
    /*
     This method has 2 purposes:
     If a donor leaves, recovery needs to switch donor
     If this member leaves, recovery needs to shutdown.
    */
    recovery_module->update_recovery_process(members_left, is_leaving);
  }

  if (members_left) {
    update_member_status(
        new_view.get_leaving_members(), Group_member_info::MEMBER_OFFLINE,
        Group_member_info::MEMBER_END, Group_member_info::MEMBER_ERROR);

    if (!is_leaving) {
      Leaving_members_action_packet *leaving_members_action =
          new Leaving_members_action_packet(new_view.get_leaving_members());
      this->applier_module->add_leaving_members_action_packet(
          leaving_members_action);
    }
  }

  if (is_leaving) {
    gcs_module->notify_of_view_change_end();
  }
}

bool Plugin_gcs_events_handler::is_member_on_vector(
    const vector<Gcs_member_identifier> &members,
    const Gcs_member_identifier &member_id) const {
  vector<Gcs_member_identifier>::const_iterator it;

  it = std::find(members.begin(), members.end(), member_id);

  return it != members.end();
}

int Plugin_gcs_events_handler::process_local_exchanged_data(
    const Exchanged_data &exchanged_data, bool is_joining) const {
  int error = 0;
  uint local_uuid_found = 0;
  std::vector<std::string> exchanged_members_actions_serialized_configuration;
  std::vector<std::string>
      exchanged_replication_failover_channels_serialized_configuration;

  /*
  For now, we are only carrying Group Member Info on Exchangeable data
  Since we are receiving the state from all Group members, one shall
  store it in a set to ensure that we don't have repetitions.

  All collected data will be given to Group Member Manager at view install
  time.
  */
  for (Exchanged_data::const_iterator exchanged_data_it =
           exchanged_data.begin();
       exchanged_data_it != exchanged_data.end(); exchanged_data_it++) {
    const uchar *data = exchanged_data_it->second->get_payload();
    size_t length = exchanged_data_it->second->get_payload_length();
    Gcs_member_identifier *member_id = exchanged_data_it->first;
    if (data == nullptr) {
      /* purecov: begin inspected */
      Group_member_info member_info;
      if (!group_member_mgr->get_group_member_info_by_member_id(*member_id,
                                                                member_info)) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_DATA_NOT_PROVIDED_BY_MEM,
                     member_info.get_hostname().c_str(),
                     member_info.get_port());
      }
      continue;
      /* purecov: end */
    }

    // Process data provided by member.
    Group_member_info_list *member_infos =
        group_member_mgr->decode(data, length);

    // This construct is here in order to deallocate memory of duplicates
    Group_member_info_list_iterator member_infos_it;
    for (member_infos_it = member_infos->begin();
         member_infos_it != member_infos->end(); member_infos_it++) {
      if (local_member_info->get_uuid() == (*member_infos_it)->get_uuid()) {
        local_uuid_found++;
      }

      /*
        Accept only the information the member has about himself
        Information received about other members is probably outdated
      */
      if (local_uuid_found < 2 &&
          (*member_infos_it)->get_gcs_member_id() == *member_id) {
        this->temporary_states->insert((*member_infos_it));
      } else {
        delete (*member_infos_it); /* purecov: inspected */
      }
    }

    member_infos->clear();
    delete member_infos;

    if (local_uuid_found > 1) {
      if (is_joining) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_ALREADY_EXISTS,
                     local_member_info->get_uuid().c_str());
      }

      // Clean up temporary states.
      std::set<Group_member_info *,
               Group_member_info_pointer_comparator>::iterator
          temporary_states_it;
      for (temporary_states_it = temporary_states->begin();
           temporary_states_it != temporary_states->end();
           temporary_states_it++) {
        delete (*temporary_states_it);
      }
      temporary_states->clear();

      return 1;
    }

    /*
      Group wide configuration.
    */
    if (is_joining && local_member_info->in_primary_mode()) {
      Group_member_info_manager_message message;

      /* member actions */
      const unsigned char *member_actions_serialized_configuration = nullptr;
      size_t member_actions_serialized_configuration_length = 0;
      bool error_get_member_actions = message.get_pit_data(
          Group_member_info_manager_message::PIT_MEMBER_ACTIONS, data, length,
          &member_actions_serialized_configuration,
          &member_actions_serialized_configuration_length);

      /*
        Members from versions lower than 8.0.25 do not support member
        actions, as such exchanged data will not contain configuration
        from them.
      */
      if (!error_get_member_actions) {
        exchanged_members_actions_serialized_configuration.push_back(
            std::string(pointer_cast<const char *>(
                            member_actions_serialized_configuration),
                        member_actions_serialized_configuration_length));
      }

      /* replication failover configuration */
      const unsigned char
          *replication_failover_channels_serialized_configuration = nullptr;
      size_t replication_failover_channels_serialized_configuration_length = 0;
      bool error_get_replication_failover_channels = message.get_pit_data(
          Group_member_info_manager_message::PIT_RPL_FAILOVER_CONFIGURATION,
          data, length, &replication_failover_channels_serialized_configuration,
          &replication_failover_channels_serialized_configuration_length);

      if (!error_get_replication_failover_channels) {
        exchanged_replication_failover_channels_serialized_configuration
            .push_back(std::string(
                pointer_cast<const char *>(
                    replication_failover_channels_serialized_configuration),
                replication_failover_channels_serialized_configuration_length));
      }
    }
  }

  if (is_joining && local_member_info->in_primary_mode()) {
    /*
      A member that bootstraps a group is joining the group, thence it
      does not add its configuration to the state exchange. In this
      case there is nothing to do, the member configuration will be
      preserved.
      When the number of members is higher than one, then the existent
      members will add their configuration on the state exchange.
    */
    if (exchanged_data.size() > 1) {
      /*
         We already know that this member will be a SECONDARY,
         thence we can stop existing channels trying to start.
      */
      terminate_wait_on_start_process(
          WAIT_ON_START_PROCESS_ABORT_SECONDARY_MEMBER);

      error = member_actions_handler->replace_all_actions(
          exchanged_members_actions_serialized_configuration);
      error |= static_cast<int>(set_replication_failover_channels_configuration(
          exchanged_replication_failover_channels_serialized_configuration));
    }
  }

  return error;
}

Gcs_message_data *Plugin_gcs_events_handler::get_exchangeable_data() const {
  std::string server_executed_gtids;
  std::string server_purged_gtids;
  std::string applier_retrieved_gtids;
  Replication_thread_api applier_channel("group_replication_applier");

  Get_system_variable *get_system_variable = new Get_system_variable();

  if (get_system_variable->get_global_gtid_executed(server_executed_gtids)) {
    /* purecov: begin inspected */
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_GTID_EXECUTED_EXTRACT_ERROR);
    goto sending;
    /* purecov: inspected */
  }
  if (get_system_variable->get_global_gtid_purged(server_purged_gtids)) {
    /* purecov: begin inspected */
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_GTID_PURGED_EXTRACT_ERROR);
    goto sending;
    /* purecov: end */
  }
  if (applier_channel.get_retrieved_gtid_set(applier_retrieved_gtids)) {
    LogPluginErr(WARNING_LEVEL,
                 ER_GRP_RPL_GTID_SET_EXTRACT_ERROR); /* purecov: inspected */
  }

  group_member_mgr->update_gtid_sets(local_member_info->get_uuid(),
                                     server_executed_gtids, server_purged_gtids,
                                     applier_retrieved_gtids);
sending:

  delete get_system_variable;

  std::vector<uchar> data;

  // alert joiners that an action or election is running
  {
    std::pair<std::string, std::string> action_initiator_and_description;
    if (group_action_coordinator->is_group_action_running(
            action_initiator_and_description)) {
      local_member_info->set_is_group_action_running(true);
      local_member_info->set_group_action_running_name(
          action_initiator_and_description.first);
      local_member_info->set_group_action_running_description(
          action_initiator_and_description.second);
    } else {
      local_member_info->set_is_group_action_running(false);
    }
  }
  local_member_info->set_is_primary_election_running(
      primary_election_handler->is_an_election_running());
  Group_member_info *local_member_copy =
      new Group_member_info(*local_member_info);
  Group_member_info_manager_message *group_info_message =
      new Group_member_info_manager_message(local_member_copy);
  group_info_message->encode(&data);

  /*
    Group wide configuration.

    All members except the joining ones, need to send its local
    configuration.
  */
  bool joining = (!plugin_is_group_replication_running() ||
                  autorejoin_module->is_autorejoin_ongoing());

#if !defined(NDEBUG)
  /*
    Simulate a member version lower than 8.0.25 which does not support
    member actions, thence does not include its configuration on the
    exchangeable data.
  */
  if (!joining && local_member_info->in_primary_mode()) {
    DBUG_EXECUTE_IF(
        "group_replication_skip_add_member_actions_to_exchangeable_data",
        joining = true;);
  }
#endif

  if (!joining && local_member_info->in_primary_mode()) {
    std::string member_actions_serialized_configuration;
    std::string replication_failover_channels_serialized_configuration;

    bool error_reading_member_actions = member_actions_handler->get_all_actions(
        member_actions_serialized_configuration);
    bool error_reading_failover_channels_configuration =
        get_replication_failover_channels_configuration(
            replication_failover_channels_serialized_configuration);

    if (error_reading_member_actions) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_ACTION_GET_EXCHANGEABLE_DATA);
    }
    if (error_reading_failover_channels_configuration) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILOVER_CONF_GET_EXCHANGEABLE_DATA);
    }

    /*
      Even if there was a error reading the member actions, we send
      a empty serialized configuration to allow the joining member
      to distinguish between a invalid configuration and lower version
      members that do not have configuration.
    */
    group_info_message->add_member_actions_serialized_configuration(
        &data, member_actions_serialized_configuration);
    group_info_message
        ->add_replication_failover_channels_serialized_configuration(
            &data, replication_failover_channels_serialized_configuration);
  }

  delete group_info_message;

  Gcs_message_data *msg_data = new Gcs_message_data(0, data.size());
  msg_data->append_to_payload(&data.front(), data.size());

  return msg_data;
}

void Plugin_gcs_events_handler::update_member_status(
    const vector<Gcs_member_identifier> &members,
    Group_member_info::Group_member_status status,
    Group_member_info::Group_member_status old_status_equal_to,
    Group_member_info::Group_member_status old_status_different_from) const {
  for (vector<Gcs_member_identifier>::const_iterator it = members.begin();
       it != members.end(); ++it) {
    Gcs_member_identifier member = *it;
    Group_member_info member_info;
    if (group_member_mgr->get_group_member_info_by_member_id(member,
                                                             member_info)) {
      // Trying to update a non-existing member
      continue;
    }

    // if  (the old_status_equal_to is not defined or
    //      the previous status is equal to old_status_equal_to)
    //    and
    //     (the old_status_different_from is not defined or
    //      the previous status is different from old_status_different_from)
    if ((old_status_equal_to == Group_member_info::MEMBER_END ||
         member_info.get_recovery_status() == old_status_equal_to) &&
        (old_status_different_from == Group_member_info::MEMBER_END ||
         member_info.get_recovery_status() != old_status_different_from)) {
      /*
        The notification will be handled on the top level handle
        function that calls this one down the stack.
      */
      group_member_mgr->update_member_status(member_info.get_uuid(), status,
                                             m_notification_ctx);
    }
  }
}

/**
  Here we check:
  1) If the number of members was exceeded
  2) If member version is compatible with the group
  3) If the gtid_assignment_block_size is equal to the group
  4) If the hash algorithm used is equal to the group
  5) If the member has more known transactions than the group
  6) If the member has the same configuration flags that the group has
*/
int Plugin_gcs_events_handler::check_group_compatibility(
    size_t number_of_members) const {
/*
  Check if group size did reach the maximum number of members.
*/
#ifndef NDEBUG
  if (set_number_of_members_on_view_changed_to_10) number_of_members = 10;
#endif
  if (number_of_members > 9) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_START_FAILED);
    return GROUP_REPLICATION_MAX_GROUP_SIZE;
  }

  /*
    Check if the member is compatible with the group.
    It can be incompatible because its major version is lower or a rule says it.
    If incompatible notify whoever is waiting for the view with an error, so
    the plugin exits the group.
  */
  *joiner_compatibility_status = COMPATIBLE;
  int group_data_compatibility = 0;
  if (number_of_members > 1) {
    *joiner_compatibility_status = check_version_compatibility_with_group();
    group_data_compatibility = compare_member_transaction_sets();
  }

  if (*joiner_compatibility_status == INCOMPATIBLE) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_VER_INCOMPATIBLE);
    return GROUP_REPLICATION_CONFIGURATION_ERROR;
  }
  if (*joiner_compatibility_status == READ_COMPATIBLE) {
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEMBER_VER_READ_COMPATIBLE);
  }

  /*
    All group members must have the same gtid_assignment_block_size
    and transaction-write-set-extraction value, if joiner has a
    different value it is not allowed to join.
  */
  if (number_of_members > 1 && compare_member_option_compatibility()) {
    return GROUP_REPLICATION_CONFIGURATION_ERROR;
  }

  /*
    Check that the joiner doesn't has more GTIDs than the rest of the group.
    All the executed and received transactions in the group are collected and
    merged into a GTID set and all joiner transactions must be contained in it.
  */
  if (group_data_compatibility) {
    if (group_data_compatibility > 0) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_TRANS_NOT_PRESENT_IN_GRP);
      return GROUP_REPLICATION_CONFIGURATION_ERROR;
    } else  // error
    {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_TRANS_GREATER_THAN_GRP);
      return GROUP_REPLICATION_CONFIGURATION_ERROR;
      /* purecov: end */
    }
  }

  std::string action_name;
  std::string action_description;
  if (is_group_running_a_configuration_change(action_name,
                                              action_description)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_JOIN_WHEN_GROUP_ACTION_RUNNING,
                 action_description.c_str(), action_name.c_str());
    return GROUP_REPLICATION_CONFIGURATION_ERROR;
  }

  return 0;
}

Compatibility_type
Plugin_gcs_events_handler::check_version_compatibility_with_group() const {
  bool override_lower_incompatibility = false;
  Compatibility_type compatibility_type = COMPATIBLE;
  bool read_compatible = false;

  Group_member_info_list *all_members = group_member_mgr->get_all_members();
  Group_member_info_list_iterator all_members_it;

  Member_version lowest_version(0xFFFFFF);
  /* Does not include local member version. */
  std::set<Member_version> unique_version_set;
  /* Find lowest member version and unique versions of the group for
   * comparison. */
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++) {
    /* Skip self */
    if ((*all_members_it)->get_uuid() != local_member_info->get_uuid()) {
      if ((*all_members_it)->get_member_version() < lowest_version)
        lowest_version = (*all_members_it)->get_member_version();
      unique_version_set.insert((*all_members_it)->get_member_version());
    }
  }

  /* Fetch all unique server versions in the group. */
  std::set<Member_version> all_members_versions;
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++) {
    all_members_versions.insert((*all_members_it)->get_member_version());
  }

  for (auto it = unique_version_set.begin();
       it != unique_version_set.end() && compatibility_type != INCOMPATIBLE;
       ++it) {
    Member_version ver(*it);
    compatibility_type = compatibility_manager->check_local_incompatibility(
        ver, (ver == lowest_version), all_members_versions);

    if (compatibility_type == READ_COMPATIBLE) {
      read_compatible = true;
    }

    if (compatibility_type == INCOMPATIBLE_LOWER_VERSION) {
      if (get_allow_local_lower_version_join()) {
        /*
          Despite between these two members the compatibility type
          is INCOMPATIBLE_LOWER_VERSION, when compared with others
          group members this server may be INCOMPATIBLE, so we need
          to test with all group members.
        */
        override_lower_incompatibility = true;
        compatibility_type = COMPATIBLE;
      } else {
        compatibility_type = INCOMPATIBLE;
      }
    }
  }

  if (compatibility_type != INCOMPATIBLE && override_lower_incompatibility) {
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_MEMBER_VERSION_LOWER_THAN_GRP);
  }

  if (read_compatible && compatibility_type != INCOMPATIBLE) {
    compatibility_type = READ_COMPATIBLE;
  }

  // clean the members
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++) {
    delete (*all_members_it);
  }
  delete all_members;

  return compatibility_type;
}

int Plugin_gcs_events_handler::compare_member_transaction_sets() const {
  int result = 0;

  Tsid_map local_tsid_map(nullptr);
  Tsid_map group_tsid_map(nullptr);
  Gtid_set local_member_set(&local_tsid_map, nullptr);
  Gtid_set group_set(&group_tsid_map, nullptr);

  Group_member_info_list *all_members = group_member_mgr->get_all_members();
  Group_member_info_list_iterator all_members_it;
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++) {
    std::string member_exec_set_str = (*all_members_it)->get_gtid_executed();
    std::string applier_ret_set_str = (*all_members_it)->get_gtid_retrieved();
    if ((*all_members_it)->get_gcs_member_id() ==
        local_member_info->get_gcs_member_id()) {
      if (local_member_set.add_gtid_text(member_exec_set_str.c_str()) !=
              RETURN_STATUS_OK ||
          local_member_set.add_gtid_text(applier_ret_set_str.c_str()) !=
              RETURN_STATUS_OK) {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_LOCAL_GTID_SETS_PROCESS_ERROR);
        result = -1;
        goto cleaning;
        /* purecov: end */
      }
    } else {
      if (group_set.add_gtid_text(member_exec_set_str.c_str()) !=
              RETURN_STATUS_OK ||
          group_set.add_gtid_text(applier_ret_set_str.c_str()) !=
              RETURN_STATUS_OK) {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_LOCAL_GTID_SETS_PROCESS_ERROR);
        result = -1;
        goto cleaning;
        /* purecov: end */
      }
    }
  }

  /*
    Here we only error out if the joiner set is bigger, i.e, if they are equal
    no error is returned.
    One could argue that if a joiner has the same transaction set as the group
    then something is wrong as the group also has transaction associated to
    previous view changes.
    To reject this cases cause however false negatives when members leave and
    quickly rejoin the group or when groups are started by add several nodes at
    once.
  */
  if (!local_member_set.is_subset(&group_set)) {
    char *local_gtid_set_buf;
    local_member_set.to_string(&local_gtid_set_buf);
    char *group_gtid_set_buf;
    group_set.to_string(&group_gtid_set_buf);
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_TRANS_GREATER_THAN_GRP,
                 local_gtid_set_buf, group_gtid_set_buf);
    my_free(local_gtid_set_buf);
    my_free(group_gtid_set_buf);
    result = 1;
  }

cleaning:

  // clean the members
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++) {
    delete (*all_members_it);
  }
  delete all_members;

  return result;
}

void Plugin_gcs_events_handler::collect_members_executed_sets(
    View_change_packet *view_packet) const {
  Group_member_info_list *all_members = group_member_mgr->get_all_members();
  Group_member_info_list_iterator all_members_it;
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++) {
    // Joining/Recovering members don't have valid GTID executed information
    if ((*all_members_it)->get_recovery_status() ==
        Group_member_info::MEMBER_IN_RECOVERY) {
      continue;
    }

    std::string exec_set_str = (*all_members_it)->get_gtid_executed();
    view_packet->group_executed_set.push_back(exec_set_str);
  }

  // clean the members
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++) {
    delete (*all_members_it);
  }
  delete all_members;
}

int Plugin_gcs_events_handler::compare_member_option_compatibility() const {
  int result = 0;

  Group_member_info_list *all_members = group_member_mgr->get_all_members();
  Group_member_info_list_iterator all_members_it;
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++) {
    if (local_member_info->get_gtid_assignment_block_size() !=
        (*all_members_it)->get_gtid_assignment_block_size()) {
      result = 1;
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_BLOCK_SIZE_DIFF_FROM_GRP,
                   local_member_info->get_gtid_assignment_block_size(),
                   (*all_members_it)->get_gtid_assignment_block_size());
      goto cleaning;
    }

    if (local_member_info->get_write_set_extraction_algorithm() !=
        (*all_members_it)->get_write_set_extraction_algorithm()) {
      result = 1;
      LogPluginErr(
          ERROR_LEVEL, ER_GRP_RPL_TRANS_WRITE_SET_EXTRACT_DIFF_FROM_GRP,
          local_member_info->get_write_set_extraction_algorithm_name(),
          (*all_members_it)->get_write_set_extraction_algorithm_name());
      goto cleaning;
    }

    if (local_member_info->get_configuration_flags() !=
        (*all_members_it)->get_configuration_flags()) {
      const uint32 member_configuration_flags =
          (*all_members_it)->get_configuration_flags();
      const uint32 local_configuration_flags =
          local_member_info->get_configuration_flags();

      result = 1;
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_CFG_INCOMPATIBLE_WITH_GRP_CFG,
                   Group_member_info::get_configuration_flags_string(
                       local_configuration_flags)
                       .c_str(),
                   Group_member_info::get_configuration_flags_string(
                       member_configuration_flags)
                       .c_str());
      goto cleaning;
    }

    if ((*all_members_it)->get_lower_case_table_names() !=
            DEFAULT_NOT_RECEIVED_LOWER_CASE_TABLE_NAMES &&
        local_member_info->get_lower_case_table_names() !=
            (*all_members_it)->get_lower_case_table_names()) {
      result = 1;
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_LOWER_CASE_TABLE_NAMES_DIFF_FROM_GRP,
                   local_member_info->get_lower_case_table_names(),
                   (*all_members_it)->get_lower_case_table_names());
      goto cleaning;
    }

    if (local_member_info->get_default_table_encryption() !=
        (*all_members_it)->get_default_table_encryption()) {
      result = 1;
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_DEFAULT_TABLE_ENCRYPTION_DIFF_FROM_GRP,
                   local_member_info->get_default_table_encryption(),
                   (*all_members_it)->get_default_table_encryption());
      goto cleaning;
    }

    if (is_view_change_log_event_required() &&
        local_member_info->get_view_change_uuid() !=
            (*all_members_it)->get_view_change_uuid()) {
      result = 1;
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_VIEW_CHANGE_UUID_DIFF_FROM_GRP,
                   local_member_info->get_view_change_uuid().c_str(),
                   (*all_members_it)->get_view_change_uuid().c_str());
      goto cleaning;
    }

    Member_version const version_that_supports_paxos_single_leader(
        FIRST_PROTOCOL_WITH_SUPPORT_FOR_CONSENSUS_LEADERS);
    Member_version protocol_version_mysql =
        convert_to_mysql_version(gcs_module->get_protocol_version());

    if ((local_member_info->get_allow_single_leader() !=
         (*all_members_it)->get_allow_single_leader())) {
      result = 1;

      // If PAXOS Single Leader is enabled but we are trying to enter a group
      //  that uses a protocol below 8.0.27
      if (local_member_info->get_allow_single_leader() &&
          protocol_version_mysql < version_that_supports_paxos_single_leader) {
        // We error out and force this node to enter the group with the value
        // ZERO
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_PAXOS_SINGLE_LEADER_DIFF_FROM_OLD_GRP);
      } else {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_PAXOS_SINGLE_LEADER_DIFF_FROM_GRP,
                     local_member_info->get_allow_single_leader(),
                     (*all_members_it)->get_allow_single_leader());
      }
      goto cleaning;
    }

    if (local_member_info->get_preemptive_garbage_collection() !=
        (*all_members_it)->get_preemptive_garbage_collection()) {
      result = 1;
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_PREEMPTIVE_GARBAGE_COLLECTION_DIFF_FROM_GRP,
                   local_member_info->get_preemptive_garbage_collection(),
                   (*all_members_it)->get_preemptive_garbage_collection());
      goto cleaning;
    }
  }

cleaning:
  for (all_members_it = all_members->begin();
       all_members_it != all_members->end(); all_members_it++)
    delete (*all_members_it);
  delete all_members;

  return result;
}

bool Plugin_gcs_events_handler::is_group_running_a_configuration_change(
    std::string &group_action_running_name,
    std::string &group_action_running_description) const {
  bool is_action_running = false;
  Group_member_info_list *all_members = group_member_mgr->get_all_members();
  for (Group_member_info *member_info : *all_members) {
    if (member_info->is_group_action_running()) {
      is_action_running = true;
      group_action_running_name = member_info->get_group_action_running_name();
      group_action_running_description =
          member_info->get_group_action_running_description();
      break;
    }
  }
  for (Group_member_info *member_info : *all_members) delete member_info;
  delete all_members;

  return is_action_running;
}

bool Plugin_gcs_events_handler::is_group_running_a_primary_election() const {
  bool is_election_running = false;
  Group_member_info_list *all_members = group_member_mgr->get_all_members();
  for (Group_member_info *member_info : *all_members) {
    if (member_info->is_primary_election_running()) {
      is_election_running = true;
      break;
    }
  }
  for (Group_member_info *member_info : *all_members) delete member_info;
  delete all_members;

  return is_election_running;
}

void Plugin_gcs_events_handler::disable_read_mode_for_compatible_members(
    bool force_check) const {
  Member_version lowest_version =
      group_member_mgr->get_group_lowest_online_version();
  /* We need to lock the operations of group_member_mgr so that member does not
   * changes it state to ERROR and enables read only mode after we check its
   * state here. If we read old ONLINE value and continue to disable read mode,
   * member will continue to be writable even in ERROR state. So lock protects
   * from this situation. */
  MUTEX_LOCK(lock, group_member_mgr->get_update_lock());
  if (local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_ONLINE &&
      (force_check || *joiner_compatibility_status != COMPATIBLE)) {
    *joiner_compatibility_status =
        Compatibility_module::check_version_incompatibility(
            local_member_info->get_member_version(), lowest_version);
    /* Some lower version left the group, now this member is new lowest
     * version. */
    if (!local_member_info->in_primary_mode() &&
        *joiner_compatibility_status == COMPATIBLE) {
      if (disable_server_read_mode()) {
        LogPluginErr(WARNING_LEVEL,
                     ER_GRP_RPL_DISABLE_SRV_READ_MODE_RESTRICTED);
      }
    }
  }
}
