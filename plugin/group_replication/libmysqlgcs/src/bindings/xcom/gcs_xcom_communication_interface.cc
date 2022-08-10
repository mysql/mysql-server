/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"  // gcs_protocol_to_mysql_version

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>  // std::find_if
#include <iostream>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/app_data.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/pax_msg.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

#define NUMBER_OF_XCOM_SOCKET_RETRIES 1000

using std::map;

Gcs_xcom_communication::Gcs_xcom_communication(
    Gcs_xcom_statistics_updater *stats, Gcs_xcom_proxy *proxy,
    Gcs_xcom_view_change_control_interface *view_control,
    Gcs_xcom_engine *gcs_engine, Gcs_group_identifier const &group_id,
    std::unique_ptr<Network_provider_management_interface> comms_mgmt)
    : event_listeners(),
      stats(stats),
      m_xcom_proxy(proxy),
      m_view_control(view_control),
      m_msg_pipeline(),
      m_buffered_packets(),
      m_xcom_nodes(),
      m_gid_hash(),
      m_protocol_changer(*gcs_engine, m_msg_pipeline),
      m_comms_mgmt_interface(std::move(comms_mgmt)) {
  const void *id_str = group_id.get_group_id().c_str();
  m_gid_hash = Gcs_xcom_utils::mhash(static_cast<const unsigned char *>(id_str),
                                     group_id.get_group_id().size());
}

Gcs_xcom_communication::~Gcs_xcom_communication() = default;

std::map<int, const Gcs_communication_event_listener &>
    *Gcs_xcom_communication::get_event_listeners() {
  return &event_listeners;
}

enum_gcs_error Gcs_xcom_communication::send_message(
    const Gcs_message &message_to_send) {
  MYSQL_GCS_LOG_DEBUG("Sending message.")

  unsigned long long message_length = 0;
  enum_gcs_error message_result = GCS_NOK;

  /*
    This is an optimistic attempt to avoid sending a message to a
    group when the node doesn't belong to it. If it is kicked out
    of the group while trying to send a message, this function
    should eventually return an error.
  */
  if (!m_view_control->belongs_to_group()) {
    MYSQL_GCS_LOG_ERROR(
        "Message cannot be sent because the member does not belong to "
        "a group.")
    return GCS_NOK;
  }

  message_result = this->do_send_message(message_to_send, &message_length,
                                         Cargo_type::CT_USER_DATA);

  if (message_result == GCS_OK) {
    this->stats->update_message_sent(message_length);
  }

  return message_result;
}

enum_gcs_error Gcs_xcom_communication::do_send_message(
    const Gcs_message &msg, unsigned long long *msg_len, Cargo_type cargo) {
  enum_gcs_error ret = GCS_NOK;
  const Gcs_message_data &msg_data = msg.get_message_data();
  unsigned long long total_buffers_length = 0;
  bool pipeline_error = true;
  std::vector<Gcs_packet> packets_out;
  std::size_t nr_packets_to_send = 0;

  m_protocol_changer.atomically_increment_nr_packets_in_transit(cargo);

  /*
     Apply transformations and move the result to a vector of packets as
     the content may be fragmented into small pieces.
   */
  std::tie(pipeline_error, packets_out) =
      m_msg_pipeline.process_outgoing(msg_data, cargo);
  if (pipeline_error) {
    MYSQL_GCS_LOG_ERROR("Error preparing the message for sending.")
    goto end;
  }

  nr_packets_to_send = packets_out.size();
  if (nr_packets_to_send > 1) {
    m_protocol_changer.adjust_nr_packets_in_transit(cargo,
                                                    nr_packets_to_send - 1);
  }

  /*
    The packet is now part of a vector and it may have been split so we
    have to iterate over each individual packet in the vector and send it.
  */
  for (auto &result_packet : packets_out) {
    Gcs_packet::buffer_ptr serialized_packet;
    unsigned long long msg_buffer_length = 0;
    std::tie(serialized_packet, msg_buffer_length) = result_packet.serialize();
    total_buffers_length += msg_buffer_length;

    MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_MSG_FLOW,
                                    "Sending message with payload length %llu",
                                    msg_buffer_length)

    /* Pass ownership of the buffer to xcom_client_send_data. */
    unsigned char *msg_buffer = serialized_packet.release();
    bool const sent_to_xcom = m_xcom_proxy->xcom_client_send_data(
        msg_buffer_length, reinterpret_cast<char *>(msg_buffer));

    if (!sent_to_xcom) {
      if (!m_view_control->is_leaving() && m_view_control->belongs_to_group()) {
        /* purecov: begin inspected */
        MYSQL_GCS_LOG_ERROR(
            "Error pushing message into group communication engine.")
        /* purecov: end */
      }
      goto end;
    }
  }

  *msg_len = total_buffers_length;
  ret = GCS_OK;

end:
  MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_MSG_FLOW,
                                  "do_send_message enum_gcs_error result(%u).",
                                  static_cast<unsigned int>(ret))

  return ret;
}

int Gcs_xcom_communication::add_event_listener(
    const Gcs_communication_event_listener &event_listener) {
  // This construct avoid the clash of keys in the map
  int handler_key = 0;
  do {
    handler_key = rand();
  } while (event_listeners.count(handler_key) != 0);

  event_listeners.emplace(handler_key, event_listener);

  return handler_key;
}

void Gcs_xcom_communication::remove_event_listener(int event_listener_handle) {
  event_listeners.erase(event_listener_handle);
}

void Gcs_xcom_communication::notify_received_message(
    std::unique_ptr<Gcs_message> &&message) {
  map<int, const Gcs_communication_event_listener &>::iterator callback_it =
      event_listeners.begin();

  while (callback_it != event_listeners.end()) {
    callback_it->second.on_message_received(*message);

    MYSQL_GCS_LOG_TRACE("Delivered message to client handler= %d",
                        (*callback_it).first)
    ++callback_it;
  }

  stats->update_message_received(
      (long)(message->get_message_data().get_header_length() +
             message->get_message_data().get_payload_length()));
  MYSQL_GCS_LOG_TRACE("Delivered message from origin= %s",
                      message->get_origin().get_member_id().c_str())
}

void Gcs_xcom_communication::buffer_incoming_packet(
    Gcs_packet &&packet, std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) {
  assert(m_view_control->is_view_changing());

  MYSQL_GCS_LOG_TRACE("Buffering packet cargo=%u", packet.get_cargo_type());

  m_buffered_packets.push_back(
      std::make_pair(std::move(packet), std::move(xcom_nodes)));
}

void Gcs_xcom_communication::deliver_buffered_packets() {
  for (auto &pair : m_buffered_packets) {
    Gcs_packet &packet = pair.first;
    std::unique_ptr<Gcs_xcom_nodes> &xcom_nodes = pair.second;

    MYSQL_GCS_LOG_TRACE("Delivering buffered packet: cargo=%u",
                        packet.get_cargo_type());

    deliver_user_data_packet(std::move(packet), std::move(xcom_nodes));
  }

  m_buffered_packets.clear();
}

void Gcs_xcom_communication::cleanup_buffered_packets() {
  m_buffered_packets.clear();
}

size_t Gcs_xcom_communication::number_buffered_packets() {
  return m_buffered_packets.size();
}

void Gcs_xcom_communication::update_members_information(
    const Gcs_member_identifier &me, const Gcs_xcom_nodes &members) {
  m_msg_pipeline.update_members_information(me, members);
  m_xcom_nodes.add_nodes(members);
}

std::vector<Gcs_xcom_node_information>
Gcs_xcom_communication::possible_packet_recovery_donors() const {
  auto const &all_members = m_xcom_nodes.get_nodes();
  assert(!all_members.empty());

  std::vector<Gcs_xcom_node_information> donors;

  Gcs_xcom_interface *const xcom_interface =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());
  if (xcom_interface != nullptr) {
    Gcs_member_identifier myself{
        xcom_interface->get_node_address()->get_member_address()};
    auto not_me_predicate =
        [&myself](Gcs_xcom_node_information const &xcom_node) {
          bool const is_me = (xcom_node.get_member_id() == myself);
          return !is_me;
        };
    std::copy_if(all_members.cbegin(), all_members.cend(),
                 std::back_inserter(donors), not_me_predicate);
    assert(donors.size() == all_members.size() - 1);
  }

  return donors;
}

Gcs_xcom_communication::packet_recovery_result
Gcs_xcom_communication::process_recovered_packet(
    synode_app_data const &recovered_data) {
  auto result = packet_recovery_result::ERROR;
  Gcs_pipeline_incoming_result error_code = Gcs_pipeline_incoming_result::ERROR;
  Gcs_packet packet;
  Gcs_packet packet_in;

  /*
   The buffer with the raw data for a given packet needs to be owned by the
   packet, i.e. have the same lifetime of the packet. Therefore, we need an
   individual buffer for each packet.
   */
  auto const &data_len = recovered_data.data.data_len;
  // Create the new buffer.
  Gcs_packet::buffer_ptr data(static_cast<unsigned char *>(std::malloc(
                                  data_len * sizeof(unsigned char))),
                              Gcs_packet_buffer_deleter());
  if (data == nullptr) {
    /* purecov: begin inspected */
    result = packet_recovery_result::NO_MEMORY;
    goto end;
    /* purecov: end */
  }
  // Copy the recovered data to the new buffer.
  std::memcpy(data.get(), recovered_data.data.data_val, data_len);
  // Create the packet.
  packet = Gcs_packet::make_incoming_packet(
      std::move(data), data_len, recovered_data.synode, recovered_data.origin,
      m_msg_pipeline);

  /*
   The packet should always be a user data packet, but rather than asserting
   that, treat it as a failure if it is not.
   */
  if (packet.get_cargo_type() != Cargo_type::CT_USER_DATA) {
    result = packet_recovery_result::PACKET_UNEXPECTED_CARGO;
    goto end;
  }

  /* Send the packet through the pipeline. */
  std::tie(error_code, packet_in) =
      m_msg_pipeline.process_incoming(std::move(packet));

  /*
   The pipeline should process the packet successfully and *not* output
   packets, because the packet we sent through the pipeline is supposed to be
   a fragment.
   But rather than asserting that, treat it as a failure if it does not
   happen.
   */
  switch (error_code) {
    case Gcs_pipeline_incoming_result::OK_NO_PACKET:
      break;
    case Gcs_pipeline_incoming_result::OK_PACKET:
      result = packet_recovery_result::PIPELINE_UNEXPECTED_OUTPUT;
      goto end;
    /* purecov: begin inspected */
    case Gcs_pipeline_incoming_result::ERROR:
      result = packet_recovery_result::PIPELINE_ERROR;
      goto end;
      /* purecov: end */
  }

  result = packet_recovery_result::OK;

end:
  return result;
}

Gcs_xcom_communication::packet_recovery_result
Gcs_xcom_communication::process_recovered_packets(
    synode_app_data_array const &recovered_data) {
  auto result = packet_recovery_result::ERROR;

  auto const &nr_synodes = recovered_data.synode_app_data_array_len;

  for (u_int i = 0; i < nr_synodes; i++) {
    synode_app_data const &recovered_synode_data =
        recovered_data.synode_app_data_array_val[i];

    result = process_recovered_packet(recovered_synode_data);
    if (result != packet_recovery_result::OK) goto end;
  }

  result = packet_recovery_result::OK;

end:
  return result;
}

Gcs_xcom_communication::packet_recovery_result
Gcs_xcom_communication::recover_packets_from_donor(
    Gcs_xcom_node_information const &donor,
    std::unordered_set<Gcs_xcom_synode> const &synodes,
    synode_app_data_array &recovered_data) {
  auto result = packet_recovery_result::ERROR;

  /* Request the payloads from the donor's XCom. */
  bool successful = m_xcom_proxy->xcom_get_synode_app_data(
      donor, m_gid_hash, synodes, recovered_data);

  if (successful) {
    /*
     This should always be true, but rather than asserting it, treat it as a
     failure if it is not.
     */
    successful = (recovered_data.synode_app_data_array_len == synodes.size());
  }

  if (!successful) {
    result = packet_recovery_result::PACKETS_UNRECOVERABLE;
  } else {
    result = packet_recovery_result::OK;
  }

  return result;
}

void Gcs_xcom_communication::log_packet_recovery_failure(
    packet_recovery_result const &error_code,
    Gcs_xcom_node_information const &donor) const {
  switch (error_code) {
    case Gcs_xcom_communication::packet_recovery_result::OK:
      break;
    case packet_recovery_result::PACKETS_UNRECOVERABLE:
      MYSQL_GCS_LOG_DEBUG(
          "%s did not have the GCS packets this server requires to safely join "
          "the group.",
          donor.get_member_id().get_member_id().c_str());
      break;
    /* purecov: begin inspected */
    case packet_recovery_result::NO_MEMORY:
      MYSQL_GCS_LOG_DEBUG(
          "Could not allocate memory to process the recovered GCS packets this "
          "server requires to safely join the group.");
      break;
    /* purecov: end */
    case packet_recovery_result::PIPELINE_ERROR:
      MYSQL_GCS_LOG_DEBUG(
          "The pipeline encountered an error processing the recovered GCS "
          "packets this server requires to safely join the group.");
      break;
    case packet_recovery_result::PIPELINE_UNEXPECTED_OUTPUT:
      MYSQL_GCS_LOG_DEBUG(
          "The pipeline produced an unexpected packet while processing the "
          "recovered GCS packets this server requires to safely join the "
          "group.");
      break;
    case packet_recovery_result::PACKET_UNEXPECTED_CARGO:
      MYSQL_GCS_LOG_DEBUG(
          "One of the recovered GCS packets this server requires to safely "
          "join the group is of an unexpected type.");
      break;
    case packet_recovery_result::ERROR:
      MYSQL_GCS_LOG_DEBUG(
          "There was an error processing the recovered GCS packets this server "
          "requires to safely join the group.");
      break;
  }
}

bool Gcs_xcom_communication::recover_packets(
    std::unordered_set<Gcs_xcom_synode> const &synodes) {
  u_int const nr_synodes = synodes.size();
  bool successful = false;
  packet_recovery_result error_code = packet_recovery_result::ERROR;

  auto donors = possible_packet_recovery_donors();

  /* Go through the possible donors until we can recover from one. */
  for (auto donor_it = donors.begin(); !successful && donor_it != donors.end();
       donor_it++) {
    Gcs_xcom_node_information const &donor = *donor_it;

    MYSQL_GCS_LOG_DEBUG(
        "This server requires %u missing GCS packets to join the group safely. "
        "It will attempt to recover the needed GCS packets from %s.",
        nr_synodes, donor.get_member_id().get_member_id().c_str());

    synode_app_data_array recovered_data;
    recovered_data.synode_app_data_array_len = 0;
    recovered_data.synode_app_data_array_val = nullptr;

    error_code = recover_packets_from_donor(donor, synodes, recovered_data);
    if (error_code != packet_recovery_result::OK) {
      log_packet_recovery_failure(error_code, donor);
      continue;  // Next donor.
    }

    error_code = process_recovered_packets(recovered_data);
    if (error_code != packet_recovery_result::OK) {
      log_packet_recovery_failure(error_code, donor);
      continue;  // Next donor.
    }

    successful = true;

    ::xdr_free(reinterpret_cast<xdrproc_t>(xdr_synode_app_data_array),
               reinterpret_cast<char *>(&recovered_data));
  }

  return successful;
}

Gcs_message *Gcs_xcom_communication::convert_packet_to_message(
    Gcs_packet &&packet, std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) {
  Gcs_message_data *message_data = nullptr;
  Gcs_xcom_synode packet_synode;
  Gcs_xcom_node_information const *node = nullptr;
  Gcs_member_identifier origin;
  Gcs_xcom_interface *intf = nullptr;
  Gcs_group_identifier *destination = nullptr;
  Gcs_message *message = nullptr;

  /* Send the packet through the pipeline. */
  Gcs_pipeline_incoming_result error_code;
  Gcs_packet packet_in;
  std::tie(error_code, packet_in) =
      m_msg_pipeline.process_incoming(std::move(packet));
  switch (error_code) {
    case Gcs_pipeline_incoming_result::OK_PACKET:
      break;
    case Gcs_pipeline_incoming_result::OK_NO_PACKET:
      goto end;
    case Gcs_pipeline_incoming_result::ERROR:
      MYSQL_GCS_LOG_ERROR(
          "Rejecting message since it wasn't processed correctly in the "
          "pipeline.")
      goto end;
  }

  /*
   Transform the incoming packet into the message that will be delivered to
   the upper layer.

   Decode the incoming packet into the message.
   */
  message_data = new Gcs_message_data(packet_in.get_payload_length());
  if (message_data->decode(packet_in.get_payload_pointer(),
                           packet_in.get_payload_length())) {
    /* purecov: begin inspected */
    delete message_data;
    MYSQL_GCS_LOG_WARN("Discarding message. Unable to decode it.");
    goto end;
    /* purecov: end */
  }
  // Get packet origin.
  packet_synode = packet_in.get_origin_synode();
  node = xcom_nodes->get_node(packet_synode.get_synod().node);
  origin = Gcs_member_identifier(node->get_member_id());
  intf = static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());
  destination =
      intf->get_xcom_group_information(packet_synode.get_synod().group_id);
  assert(destination != nullptr);
  // Construct the message.
  message = new Gcs_message(origin, *destination, message_data);

end:
  return message;
}

void Gcs_xcom_communication::process_user_data_packet(
    Gcs_packet &&packet, std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) {
  m_protocol_changer.decrement_nr_packets_in_transit(packet, *xcom_nodes.get());

  /*
   If a view exchange phase is being executed, messages are buffered
   and then delivered to the application after the view has been
   installed. This is done to avoid delivering messages to the
   application in nodes that are joining because it would be strange
   to receive messages before any view.

   We could have relaxed this a little bit and could have let nodes
   from an old view to immediately deliver messages. However, we
   don't do this because we want to provide virtual synchrony. Note
   that we don't guarantee that a message sent in a view will be
   delivered in the same view.

   It is also important to note that this method must be executed by
   the same thread that processes global view messages and data
   message in order to avoid any concurrency issue.
 */
  if (!m_view_control->is_view_changing()) {
    deliver_user_data_packet(std::move(packet), std::move(xcom_nodes));
  } else {
    buffer_incoming_packet(std::move(packet), std::move(xcom_nodes));
  }
}

/*
  Helper function to determine whether this server is still in the group.

  In principle one should be able to simply call view_control.belongs_to_group()
  to check whether this server still belongs to the group. However, testing
  shows that it does not fix the issue, i.e. GCS still delivers messages to
  clients after leaving the group. Since the current logic around the server
  leaving/being expelled from the group is convoluted, as a stop-gap fix we will
  rely on whether we belong to the current view or not to decide whether we
  still belong to the group.
 */
static bool are_we_still_in_the_group(
    Gcs_xcom_view_change_control_interface &view_control) {
  bool still_in_the_group = false;

  Gcs_xcom_interface *const xcom_interface =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());
  if (xcom_interface != nullptr) {
    std::string &myself =
        xcom_interface->get_node_address()->get_member_address();
    Gcs_view const *const view = view_control.get_unsafe_current_view();
    still_in_the_group = (view != nullptr && view->has_member(myself));
  }

  return still_in_the_group;
}

void Gcs_xcom_communication::deliver_user_data_packet(
    Gcs_packet &&packet, std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) {
  Gcs_message *unmanaged_message =
      convert_packet_to_message(std::move(packet), std::move(xcom_nodes));
  std::unique_ptr<Gcs_message> message{unmanaged_message};

  bool const error = (message == nullptr);
  bool const still_in_the_group = are_we_still_in_the_group(*m_view_control);

  bool const should_notify = (!error && still_in_the_group);
  if (should_notify) {
    notify_received_message(std::move(message));
  } else {
    MYSQL_GCS_LOG_TRACE(
        "Did not deliver message error=%d still_in_the_group=%d", error,
        still_in_the_group);
  }
}

Gcs_protocol_version Gcs_xcom_communication::get_protocol_version() const {
  return m_protocol_changer.get_protocol_version();
}

std::pair<bool, std::future<void>> Gcs_xcom_communication::set_protocol_version(
    Gcs_protocol_version new_version) {
  return m_protocol_changer.set_protocol_version(new_version);
}

Gcs_protocol_version
Gcs_xcom_communication::get_maximum_supported_protocol_version() const {
  return m_protocol_changer.get_maximum_supported_protocol_version();
}

void Gcs_xcom_communication::set_maximum_supported_protocol_version(
    Gcs_protocol_version version) {
  return m_protocol_changer.set_maximum_supported_protocol_version(version);
}

void Gcs_xcom_communication::set_communication_protocol(
    enum_transport_protocol protocol) {
  m_comms_mgmt_interface->set_running_protocol(protocol);
}

enum_transport_protocol
Gcs_xcom_communication::get_incoming_connections_protocol() {
  return m_comms_mgmt_interface->get_incoming_connections_protocol();
}
