/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/recovery_metadata.h"
#include "mysql/components/services/log_builtins.h"
#include "plugin/group_replication/include/gcs_operations.h"
#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/recovery.h"

Recovery_metadata_module::Recovery_metadata_module() {}

Recovery_metadata_module::~Recovery_metadata_module() {
  delete_all_recovery_view_metadata();
  delete_joiner_view_id();
}

enum_gcs_error Recovery_metadata_module::send_recovery_metadata(
    Recovery_metadata_message *recovery_metadata_msg) {
  /*
    For code readability nested if-else have not been used.
    1. Get the GCS Member ID of the lowest member UUID.
    2. Check if the lowest member UUID of the group matches with
       local_member_info.
       Return status is std::pair<status,gcs_member_id of sender>.
       Status is false on success.
    3. If gcs_member_id matches with local_member_info the member sends the
       metadata.
    4. If send fails set the ERROR in metadata
    5. If ERROR flag is set send the ERROR message. Error flag may be set during
       certification compression also.
  */
  std::string sender_hostname{};
  uint sender_port{0};
  Group_member_info sender_member_info;
  bool sender_member_info_not_found = true;

  std::pair<bool, Gcs_member_identifier> metadata_sender_information =
      recovery_metadata_msg->compute_and_get_current_metadata_sender();
  if (!metadata_sender_information.first) {
    sender_member_info_not_found =
        group_member_mgr->get_group_member_info_by_member_id(
            metadata_sender_information.second, sender_member_info);
  }

  // Check if Metadata sender information was successfully fetched or not.
  if (sender_member_info_not_found) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GROUP_REPLICATION_RECOVERY_METADATA_SENDER_NOT_FOUND);
    return GCS_NOK;
  }

  sender_hostname = sender_member_info.get_hostname();
  sender_port = sender_member_info.get_port();
  enum_gcs_error msg_error = GCS_OK;

#ifndef NDEBUG
  DBUG_EXECUTE_IF("gr_recovery_metadata_verify_metadata_exist", {
    assert(!recovery_metadata_msg->get_encode_view_id().empty());
    assert(recovery_metadata_msg->get_encode_message_error() !=
           Recovery_metadata_message::Recovery_metadata_message_payload_error::
               RECOVERY_METADATA_ERROR);
    assert(recovery_metadata_msg->get_encode_compression_type() ==
           GR_compress::enum_compression_type::ZSTD_COMPRESSION);
    assert(recovery_metadata_msg->get_encode_compressor_list().size() > 0);

    std::string decode_gtid_executed{};
    Tsid_map gtid_executed_tsid_map(nullptr);
    Gtid_set gtid_executed_set(&gtid_executed_tsid_map, nullptr);
    std::string gtid_executed_aux{
        recovery_metadata_msg->get_encode_group_gtid_executed()};
    if (gtid_executed_set.add_gtid_encoding(
            reinterpret_cast<const uchar *>(gtid_executed_aux.c_str()),
            gtid_executed_aux.length()) == RETURN_STATUS_OK) {
      char *gtid_executed_string = nullptr;
      gtid_executed_set.to_string(&gtid_executed_string, true);
      decode_gtid_executed.assign(gtid_executed_string);
      my_free(gtid_executed_string);
    }
    assert(!decode_gtid_executed.empty());

    std::string verify_metadata_exist_buffer{
        "gr_recovery_metadata_verify_metadata_exist debug point view_id: "};
    verify_metadata_exist_buffer.append(
        recovery_metadata_msg->get_encode_view_id());
    verify_metadata_exist_buffer.append(" and gtid executed set: ");
    verify_metadata_exist_buffer.append(decode_gtid_executed);
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 verify_metadata_exist_buffer.c_str());
  });
#endif  // NDEBUG

  if (recovery_metadata_msg->am_i_recovery_metadata_sender() &&
      recovery_metadata_msg->get_encode_message_error() ==
          Recovery_metadata_message::RECOVERY_METADATA_NO_ERROR) {
    LogPluginErr(SYSTEM_LEVEL, ER_GROUP_REPLICATION_METADATA_SENDER,
                 sender_hostname.c_str(), sender_port);
#ifndef NDEBUG
    DBUG_EXECUTE_IF("gr_wait_before_sending_metadata", {
      const char act[] =
          "now signal signal.reached_recovery_metadata_send wait_for "
          "signal.send_the_recovery_metadata";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    DBUG_EXECUTE_IF("gr_crash_before_recovery_metadata_send", DBUG_SUICIDE(););

    DBUG_EXECUTE_IF("gr_force_recovery_metadata_send_failure", {
      msg_error = GCS_NOK;
      goto failure;
    });
#endif  // NDEBUG

    /*
      If the send message is unsuccessful in sending the message, the joiner
      is informed to leave the group.
      The enum_gcs_error msg_error have following errors:

      GCS_OK              : Message send was successful.
      GCS_NOK             : Error occurred while message communication.
      GCS_MESSAGE_TOO_BIG : Message was bigger then what gcs can successfully
                            communicate/handle.
    */
    msg_error = gcs_module->send_message(*recovery_metadata_msg);

#ifndef NDEBUG
  failure:
#endif  // NDEBUG
    if (msg_error != GCS_OK) {
      if (msg_error == GCS_MESSAGE_TOO_BIG) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_MESSAGE_SENDING,
                     "Failed to send the recovery metadata as message was "
                     "bigger then what gcs can successfully communicate/handle."
                     " Sending ERROR message to the joiner to leave the "
                     "group.");
      } else {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_MESSAGE_SENDING,
                     "Failed to send the recovery metadata. Sending ERROR "
                     "message to the joiner to leave the group.");
      }
      recovery_metadata_msg->set_encode_message_error();
    }
  }

  if (recovery_metadata_msg->get_encode_message_error() ==
          Recovery_metadata_message::RECOVERY_METADATA_ERROR &&
      recovery_metadata_msg->am_i_recovery_metadata_sender()) {
    msg_error = send_error_message_internal(recovery_metadata_msg);
    if (msg_error != GCS_OK) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_MESSAGE_SENDING,
                   "Failed to send error message to the group for the recovery"
                   " metadata send failure.");
    }
  }

  if (!recovery_metadata_msg->am_i_recovery_metadata_sender()) {
    LogPluginErr(SYSTEM_LEVEL, ER_GROUP_REPLICATION_METADATA_SENDER_IS_REMOTE,
                 sender_hostname.c_str(), sender_port);
  }

  return msg_error;
}

enum_gcs_error Recovery_metadata_module::send_error_message_internal(
    Recovery_metadata_message *recovery_metadata_msg) {
  LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_CERTIFIER_MESSAGE_LARGE);

  /*
    If the send_message is unsuccessful in sending the error message, then two
    things will happen:
    1. this was a temporary failure, the send_message failed but the member did
       not left the group. The joiner will timeout waiting for the Recovery
       metadata message.
    2. this was a permanent failure, this member will leave the group and
       another member will send the Recovery metadata message.
  */
  enum_gcs_error msg_error = gcs_module->send_message(*recovery_metadata_msg);

  if (msg_error != GCS_OK) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GROUP_REPLICATION_METADATA_ERROR_ON_SEND_ERROR_MESSAGE);
  }

  return msg_error;
}

enum_gcs_error Recovery_metadata_module::send_error_message(
    const std::string &view_id) {
  enum_gcs_error msg_error = GCS_OK;
  Recovery_metadata_message *recovery_metadata_error_msg =
      new (std::nothrow) Recovery_metadata_message(
          view_id, Recovery_metadata_message::RECOVERY_METADATA_ERROR);
  /*
    If the send_message is unsuccessful in sending the error message, then two
    things will happen:
    1. this was a temporary failure, the send_message failed but the member did
       not left the group. The joiner will timeout waiting for the Recovery
       metadata message.
    2. this was a permanent failure, this member will leave the group and
       another member will send the Recovery metadata message.
  */
  if (recovery_metadata_error_msg != nullptr) {
    msg_error = gcs_module->send_message(*recovery_metadata_error_msg);
    delete recovery_metadata_error_msg;
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_MEMORY_ALLOC,
                 "sending error message.");
  }

  if (msg_error != GCS_OK) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GROUP_REPLICATION_METADATA_ERROR_ON_SEND_ERROR_MESSAGE);
  }

  return msg_error;
}
std::pair<std::map<const std::string, Recovery_metadata_message *>::iterator,
          bool>
Recovery_metadata_module::add_recovery_view_metadata(
    const std::string &view_id) {
  return recovery_view_metadata_table.insert(
      std::pair<const std::string, Recovery_metadata_message *>(
          view_id, new (std::nothrow) Recovery_metadata_message(view_id)));
}

bool Recovery_metadata_module::delete_recovery_view_metadata_internal(
    const std::string view_id) {
  auto find_element = recovery_view_metadata_table.find(view_id);
  bool result = (find_element != recovery_view_metadata_table.end());

  if (result) {
    delete find_element->second;
    recovery_view_metadata_table.erase(view_id);
  }

  DBUG_EXECUTE_IF(
      "group_replication_recovery_metadata_module_delete_one_stored_metadata", {
        const char act[] =
            "now signal "
            "signal.group_replication_recovery_metadata_module_delete_one_"
            "stored_metadata_reached";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });

  return !result;
}

void Recovery_metadata_module::delete_all_recovery_view_metadata() {
  for (const auto &it : recovery_view_metadata_table) delete it.second;
  recovery_view_metadata_table.clear();

  DBUG_EXECUTE_IF(
      "group_replication_recovery_metadata_module_delete_all_stored_metadata", {
        const char act[] =
            "now signal "
            "signal.group_replication_recovery_metadata_module_delete_all_"
            "stored_metadata_reached";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });
}

void Recovery_metadata_module::
    delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left(
        std::vector<Gcs_member_identifier> &members_left,
        std::vector<std::string> &view_id_delete_list) {
  /*
    1. Loop through all the stored metadata and delete the members that left the
       group from the joining member list and the valid metadata sender list.
    2. If post cleanup there are no joiner or the valid sender for the view,
       mark such views for deletion.
  */
  if (members_left.size()) {
    delete_members_from_all_recovery_view_metadata_internal(
        members_left, view_id_delete_list);
  }

  /*
    3. Delete all the stored metadata of the views marked for deletion.
  */
  for (auto &view_id : view_id_delete_list) {
    delete_recovery_view_metadata_internal(view_id);
  }

  /*
    4. If metadata sender has left the group, find the new metadata sender
       and re-send the recovery metadata.
  */
  for (auto &record : recovery_view_metadata_table) {
    if (record.second->donor_left()) {
      send_recovery_metadata(record.second);
    }
  }
}

void Recovery_metadata_module::
    delete_members_from_all_recovery_view_metadata_internal(
        std::vector<Gcs_member_identifier> &members_left,
        std::vector<std::string> &view_id_delete_list) {
  for (auto &record : recovery_view_metadata_table) {
    record.second->delete_members_left(members_left);
    if (record.second->is_joiner_or_valid_sender_list_empty()) {
      view_id_delete_list.emplace_back(record.first);
    }
  }
}

void Recovery_metadata_module::store_joiner_view_id_and_valid_senders(
    const std::string &view_id,
    const std::vector<Gcs_member_identifier> &valid_senders) {
  // View-ID in which joiner joined should be called only once.
  // Hence the assert.
  assert(m_recovery_metadata_joiner_information == nullptr);
  m_recovery_metadata_joiner_information =
      new Recovery_metadata_joiner_information(view_id);
  m_recovery_metadata_joiner_information->set_valid_sender_list_of_joiner(
      valid_senders);
}

void Recovery_metadata_module::delete_joiner_view_id() {
  delete m_recovery_metadata_joiner_information;
  m_recovery_metadata_joiner_information = nullptr;
}

bool Recovery_metadata_module::is_joiner_recovery_metadata(
    const std::string &view_id) {
  return (m_recovery_metadata_joiner_information != nullptr)
             ? m_recovery_metadata_joiner_information
                   ->is_joiner_recovery_metadata(view_id)
             : false;
}

void Recovery_metadata_module::leave_the_group_internal(std::string err_msg) {
  leave_group_on_failure::mask leave_actions;
  leave_actions.set(leave_group_on_failure::STOP_APPLIER, true);
  leave_actions.set(leave_group_on_failure::CLEAN_GROUP_MEMBERSHIP, true);
  leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
  leave_group_on_failure::leave(leave_actions, 0, nullptr, err_msg.c_str());
}

void Recovery_metadata_module::
    delete_leaving_members_from_joiner_and_leave_group_if_no_valid_sender(
        const std::vector<Gcs_member_identifier> &leaving, bool is_leaving) {
  if (m_recovery_metadata_joiner_information != nullptr &&
      m_recovery_metadata_joiner_information->is_member_waiting_on_metadata()) {
    m_recovery_metadata_joiner_information->delete_leaving_members_from_sender(
        leaving);
    if (m_recovery_metadata_joiner_information->is_valid_sender_list_empty() ||
        is_leaving) {
      delete_joiner_view_id();
      if (!is_leaving) {
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_NO_CERTIFICATION_DONOR_AVAILABLE);
        leave_the_group_internal("All valid senders have left the group.");
      }
    }
  }
}
