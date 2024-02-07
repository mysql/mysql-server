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

#include "plugin/group_replication/include/plugin_handlers/metrics_handler.h"
#include "plugin/group_replication/include/certifier.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/pipeline_stats.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/include/plugin_messages/group_service_message.h"
#include "plugin/group_replication/include/plugin_messages/group_validation_message.h"
#include "plugin/group_replication/include/plugin_messages/recovery_message.h"
#include "plugin/group_replication/include/plugin_messages/recovery_metadata_message.h"
#include "plugin/group_replication/include/plugin_messages/single_primary_message.h"
#include "plugin/group_replication/include/plugin_messages/sync_before_execution_message.h"
#include "plugin/group_replication/include/plugin_messages/transaction_message.h"
#include "plugin/group_replication/include/plugin_messages/transaction_prepared_message.h"
#include "plugin/group_replication/include/plugin_messages/transaction_with_guarantee_message.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"

void Metrics_handler::reset() {
  m_control_messages_sent_count.store(0);
  m_data_messages_sent_count.store(0);
  m_control_messages_sent_bytes_sum.store(0);
  m_data_messages_sent_bytes_sum.store(0);
  m_control_messages_sent_roundtrip_time_sum.store(0);
  m_data_messages_sent_roundtrip_time_sum.store(0);
  m_transactions_consistency_before_begin_count.store(0);
  m_transactions_consistency_before_begin_time_sum.store(0);
  m_transactions_consistency_after_termination_count.store(0);
  m_transactions_consistency_after_termination_time_sum.store(0);
  m_transactions_consistency_after_sync_count.store(0);
  m_transactions_consistency_after_sync_time_sum.store(0);
  m_certification_garbage_collector_count.store(0);
  m_certification_garbage_collector_time_sum.store(0);
}

uint64_t Metrics_handler::get_control_messages_sent_count() const {
  return m_control_messages_sent_count.load();
}

uint64_t Metrics_handler::get_data_messages_sent_count() const {
  return m_data_messages_sent_count.load();
}

uint64_t Metrics_handler::get_control_messages_sent_bytes_sum() const {
  return m_control_messages_sent_bytes_sum.load();
}

uint64_t Metrics_handler::get_data_messages_sent_bytes_sum() const {
  return m_data_messages_sent_bytes_sum.load();
}

uint64_t Metrics_handler::get_control_messages_sent_roundtrip_time_sum() const {
  return m_control_messages_sent_roundtrip_time_sum.load();
}

uint64_t Metrics_handler::get_data_messages_sent_roundtrip_time_sum() const {
  return m_data_messages_sent_roundtrip_time_sum.load();
}

uint64_t Metrics_handler::get_transactions_consistency_before_begin_count()
    const {
  return m_transactions_consistency_before_begin_count.load();
}

uint64_t Metrics_handler::get_transactions_consistency_before_begin_time_sum()
    const {
  return m_transactions_consistency_before_begin_time_sum.load();
}

uint64_t Metrics_handler::get_transactions_consistency_after_termination_count()
    const {
  return m_transactions_consistency_after_termination_count.load();
}

uint64_t
Metrics_handler::get_transactions_consistency_after_termination_time_sum()
    const {
  return m_transactions_consistency_after_termination_time_sum.load();
}

uint64_t Metrics_handler::get_transactions_consistency_after_sync_count()
    const {
  return m_transactions_consistency_after_sync_count.load();
}

uint64_t Metrics_handler::get_transactions_consistency_after_sync_time_sum()
    const {
  return m_transactions_consistency_after_sync_time_sum.load();
}

uint64_t Metrics_handler::get_certification_garbage_collector_count() const {
  return m_certification_garbage_collector_count.load();
}

uint64_t Metrics_handler::get_certification_garbage_collector_time_sum() const {
  return m_certification_garbage_collector_time_sum.load();
}

void Metrics_handler::add_message_sent(const Gcs_message &message) {
  const uint64_t message_received_timestamp =
      Metrics_handler::get_current_time();
  /*
    Only account messages sent by this member.
  */
  if (local_member_info->get_gcs_member_id() == message.get_origin()) {
    uint64_t message_sent_timestamp = 0;
    const Plugin_gcs_message::enum_cargo_type plugin_message_type =
        Plugin_gcs_message::get_cargo_type(
            message.get_message_data().get_payload());
    Metrics_handler::enum_message_type metrics_message_type =
        Metrics_handler::enum_message_type::CONTROL;

    switch (plugin_message_type) {
      case Plugin_gcs_message::CT_TRANSACTION_MESSAGE:
        metrics_message_type = Metrics_handler::enum_message_type::DATA;
        message_sent_timestamp = Transaction_message::get_sent_timestamp(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_TRANSACTION_WITH_GUARANTEE_MESSAGE:
        metrics_message_type = Metrics_handler::enum_message_type::DATA;
        message_sent_timestamp =
            Transaction_with_guarantee_message::get_sent_timestamp(
                message.get_message_data().get_payload(),
                message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_TRANSACTION_PREPARED_MESSAGE:
        message_sent_timestamp =
            Transaction_prepared_message::get_sent_timestamp(
                message.get_message_data().get_payload(),
                message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_SYNC_BEFORE_EXECUTION_MESSAGE:
        message_sent_timestamp =
            Sync_before_execution_message::get_sent_timestamp(
                message.get_message_data().get_payload(),
                message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_CERTIFICATION_MESSAGE:
        message_sent_timestamp = Gtid_Executed_Message::get_sent_timestamp(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_PIPELINE_STATS_MEMBER_MESSAGE:
        message_sent_timestamp =
            Pipeline_stats_member_message::get_sent_timestamp(
                message.get_message_data().get_payload(),
                message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_MESSAGE_SERVICE_MESSAGE:
        message_sent_timestamp = Group_service_message::get_sent_timestamp(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_RECOVERY_MESSAGE:
        message_sent_timestamp = Recovery_message::get_sent_timestamp(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_SINGLE_PRIMARY_MESSAGE:
        message_sent_timestamp = Single_primary_message::get_sent_timestamp(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_GROUP_ACTION_MESSAGE:
        message_sent_timestamp = Group_action_message::get_sent_timestamp(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_GROUP_VALIDATION_MESSAGE:
        message_sent_timestamp = Group_validation_message::get_sent_timestamp(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());
        break;

      case Plugin_gcs_message::CT_RECOVERY_METADATA_MESSAGE:
        message_sent_timestamp = Recovery_metadata_message::get_sent_timestamp(
            message.get_message_data().get_payload(),
            message.get_message_data().get_payload_length());
        break;

        /* purecov: begin inspected */
      default:
        assert(false);
        return;
        /* purecov: end */
    }

    add_message_sent_internal(
        metrics_message_type, message.get_message_data().get_encode_size(),
        message_sent_timestamp, message_received_timestamp);
  }
}

void Metrics_handler::add_message_sent_internal(
    const enum_message_type type, const uint64_t bytes,
    const uint64_t sent_timestamp, const uint64_t received_timestamp) {
  assert(bytes > 0);
  assert(sent_timestamp > 0);
  assert(received_timestamp > 0);
  assert(received_timestamp > sent_timestamp);
  const uint64_t roundtrip = received_timestamp - sent_timestamp;

  switch (type) {
    case Metrics_handler::enum_message_type::DATA:
      m_data_messages_sent_count++;
      m_data_messages_sent_bytes_sum.fetch_add(bytes);
      m_data_messages_sent_roundtrip_time_sum.fetch_add(roundtrip);
      break;

    case Metrics_handler::enum_message_type::CONTROL:
      m_control_messages_sent_count++;
      m_control_messages_sent_bytes_sum.fetch_add(bytes);
      m_control_messages_sent_roundtrip_time_sum.fetch_add(roundtrip);
      break;

      /* purecov: begin inspected */
    default:
      assert(false);
      break;
      /* purecov: end */
  }
}

void Metrics_handler::add_transaction_consistency_before_begin(
    const uint64_t begin_timestamp, const uint64_t end_timestamp) {
  assert(begin_timestamp > 0);
  assert(end_timestamp > 0);
  assert(end_timestamp >= begin_timestamp);
  const auto time = end_timestamp - begin_timestamp;

  m_transactions_consistency_before_begin_count++;
  m_transactions_consistency_before_begin_time_sum.fetch_add(time);
}

void Metrics_handler::add_transaction_consistency_after_termination(
    const uint64_t begin_timestamp, const uint64_t end_timestamp) {
  assert(begin_timestamp > 0);
  assert(end_timestamp > 0);
  assert(end_timestamp >= begin_timestamp);
  const auto time = end_timestamp - begin_timestamp;

  m_transactions_consistency_after_termination_count++;
  m_transactions_consistency_after_termination_time_sum.fetch_add(time);
}

void Metrics_handler::add_transaction_consistency_after_sync(
    const uint64_t begin_timestamp, const uint64_t end_timestamp) {
  assert(begin_timestamp > 0);
  assert(end_timestamp > 0);
  assert(end_timestamp >= begin_timestamp);
  const auto time = end_timestamp - begin_timestamp;

  m_transactions_consistency_after_sync_count++;
  m_transactions_consistency_after_sync_time_sum.fetch_add(time);
}

void Metrics_handler::add_garbage_collection_run(const uint64_t begin_timestamp,
                                                 const uint64_t end_timestamp) {
  assert(begin_timestamp > 0);
  assert(end_timestamp > 0);
  assert(end_timestamp >= begin_timestamp);
  const auto time = end_timestamp - begin_timestamp;

  m_certification_garbage_collector_count++;
  m_certification_garbage_collector_time_sum.fetch_add(time);
}
