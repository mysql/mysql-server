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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_communication_protocol_changer.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"  // gcs_protocol_to_mysql_version

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"

Gcs_xcom_communication_protocol_changer::
    Gcs_xcom_communication_protocol_changer(Gcs_xcom_engine &gcs_engine,
                                            Gcs_message_pipeline &pipeline)
    : m_tagged_lock(),
      m_mutex(),
      m_protocol_change_finished(),
      m_promise(),
      m_tentative_new_protocol(Gcs_protocol_version::UNKNOWN),
      m_max_supported_protocol(Gcs_protocol_version::HIGHEST_KNOWN),
      m_nr_packets_in_transit(0),
      m_gcs_engine(gcs_engine),
      m_msg_pipeline(pipeline) {}

Gcs_protocol_version
Gcs_xcom_communication_protocol_changer::get_protocol_version() const {
  return m_msg_pipeline.get_version();
}

std::pair<bool, std::future<void>>
Gcs_xcom_communication_protocol_changer::set_protocol_version(
    Gcs_protocol_version new_version) {
  bool will_change_protocol = false;
  std::future<void> future;

  /*
   Begins buffering outgoing messages.

   Protocol version changes are initiated by GR group actions.
   There is at most one group action executing at a time, so by definition we
   should always be able to acquire the lock.
  */
#ifndef NDEBUG
  bool const we_acquired_lock =
#endif
      m_tagged_lock.try_lock();
  assert(we_acquired_lock);

  std::string new_version_readable = gcs_protocol_to_mysql_version(new_version);

  MYSQL_GCS_LOG_INFO(
      "This node has started changing the protocol version from "
      << gcs_protocol_to_mysql_version(get_protocol_version()).c_str() << "to "
      << new_version_readable.c_str());

  if (new_version <= get_maximum_supported_protocol_version()) {
    begin_protocol_version_change(new_version);
    will_change_protocol = true;
    future = m_promise.get_future();
  } else {
    /* The protocol change will not proceed. */
    MYSQL_GCS_LOG_WARN(
        "This node has failed to apply a protocol version change. The proposed "
        "protocol version ("
        << new_version_readable.c_str()
        << ") is above the maximum supported version or you have "
           "input an incompatible version. Please "
           "review the proposed version and retry it.");
    release_tagged_lock_and_notify_waiters();
  }

  return std::make_pair(will_change_protocol, std::move(future));
}

void Gcs_xcom_communication_protocol_changer::begin_protocol_version_change(
    Gcs_protocol_version new_version) {
  assert(is_protocol_change_ongoing() &&
         "A protocol change should have been ongoing");

  m_tentative_new_protocol = new_version;
  m_promise = std::promise<void>();

  std::string old_version_readable =
      gcs_protocol_to_mysql_version(get_protocol_version());

  /* Change the pipeline. */
#ifndef NDEBUG
  bool const failed =
#endif
      m_msg_pipeline.set_version(
          static_cast<Gcs_protocol_version>(m_tentative_new_protocol));
  assert(!failed && "Setting the pipeline version should not have failed");

  std::string new_version_readable = gcs_protocol_to_mysql_version(new_version);

  MYSQL_GCS_LOG_INFO(
      "Message Pipeline version has been modified to protocol version "
      << new_version_readable.c_str()
      << ". We will now try and change to this new protocol version.");

  /*
   Finish the protocol change if all my in-transit messages have been delivered.
   */
  bool const no_messages_in_transit = (get_nr_packets_in_transit() == 0);
  if (no_messages_in_transit) {
    MYSQL_GCS_LOG_INFO(
        "There are no messages in transit. We will now change to the new "
        "protocol "
        "version "
        << new_version_readable.c_str() << ", from protocol version"
        << old_version_readable.c_str()
        << ", effectively changing the running protocol version.");
    commit_protocol_version_change();
    MYSQL_GCS_LOG_INFO("Successfully changed protocol version from "
                       << old_version_readable.c_str() << " to "
                       << new_version_readable.c_str());
  } else {
    MYSQL_GCS_LOG_INFO(
        "There are still messages in transit after setting the proposed "
        "protocol "
        "version in the pipeline. We will not change to the new protocol "
        "version "
        << new_version_readable.c_str() << ", from protocol version"
        << gcs_protocol_to_mysql_version(get_protocol_version()).c_str()
        << ", and we will wait for 0 messages in transit");
  }
}

void Gcs_xcom_communication_protocol_changer::commit_protocol_version_change() {
  assert(is_protocol_change_ongoing() &&
         "A protocol change should have been ongoing");
  assert(m_tentative_new_protocol != Gcs_protocol_version::UNKNOWN &&
         "Protocol version should have been set");

  /* Stop buffering outgoing messages. */
  release_tagged_lock_and_notify_waiters();

  /* All done, notify caller. */
  m_promise.set_value();

  MYSQL_GCS_LOG_INFO(
      "Changed to group communication protocol version "
      << gcs_protocol_to_mysql_version(m_tentative_new_protocol));
}

void Gcs_xcom_communication_protocol_changer::
    release_tagged_lock_and_notify_waiters() {
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_tagged_lock.unlock();
  }
  m_protocol_change_finished.notify_all();
}

void Gcs_xcom_communication_protocol_changer::finish_protocol_version_change(
    Gcs_tagged_lock::Tag const caller_tag) {
  /*
   Finish the ongoing protocol change.

   Note that we only want to finish the ongoing change if it is the one that
   triggered the call to this method.
   We identify if that is the case by comparing caller_tag with the current lock
   tag.
   If they match, it is still the same protocol change that is ongoing.
   Otherwise, it is another protocol change, so we do nothing.
   */
  if (is_protocol_change_ongoing()) {
    auto current_tag = m_tagged_lock.optimistic_read();
    bool const same_tags = (current_tag == caller_tag);
    if (same_tags) commit_protocol_version_change();
  }
}

bool Gcs_xcom_communication_protocol_changer::is_protocol_change_ongoing()
    const {
  return m_tagged_lock.is_locked();
}

Gcs_protocol_version Gcs_xcom_communication_protocol_changer::
    get_maximum_supported_protocol_version() const {
  return m_max_supported_protocol.load(std::memory_order_relaxed);
}

void Gcs_xcom_communication_protocol_changer::
    set_maximum_supported_protocol_version(Gcs_protocol_version version) {
  m_max_supported_protocol.store(version, std::memory_order_relaxed);

  MYSQL_GCS_LOG_INFO(
      "Group is able to support up to communication protocol version "
      << gcs_protocol_to_mysql_version(version));
}

unsigned long
Gcs_xcom_communication_protocol_changer::get_nr_packets_in_transit() const {
  return m_nr_packets_in_transit.load(std::memory_order_relaxed);
}

void do_function_finish_protocol_version_change(
    Gcs_xcom_communication_protocol_changer *protocol_changer,
    Gcs_tagged_lock::Tag const tag) {
  protocol_changer->finish_protocol_version_change(tag);
}

std::pair<bool, Gcs_tagged_lock::Tag> Gcs_xcom_communication_protocol_changer::
    optimistically_increment_nr_packets_in_transit() {
  auto tag = m_tagged_lock.optimistic_read();

  auto previous_nr_packets_in_transit =
      m_nr_packets_in_transit.fetch_add(1, std::memory_order_relaxed);

  bool const successful = m_tagged_lock.validate_optimistic_read(tag);

  MYSQL_GCS_LOG_TRACE(
      "optimistically_increment_nr_packets_in_transit: successful=%d "
      "nr_packets_in_transit=%d",
      successful, previous_nr_packets_in_transit + 1);

  return {successful, tag};
}

void Gcs_xcom_communication_protocol_changer::
    rollback_increment_nr_packets_in_transit(Gcs_tagged_lock::Tag const &tag) {
  auto const previous_nr_packets_in_transit =
      m_nr_packets_in_transit.fetch_sub(1, std::memory_order_relaxed);

  MYSQL_GCS_LOG_TRACE(
      "rollback_increment_nr_packets_in_transit: rolled back increment "
      "nr_packets_in_transit=%d",
      previous_nr_packets_in_transit - 1);

  /*
   If our rollback sets the number of packets in transit to 0, we may need to
   finish the protocol change.
   */
  bool const may_need_to_finish_protocol_change =
      (previous_nr_packets_in_transit == 1);

  if (may_need_to_finish_protocol_change) {
    MYSQL_GCS_LOG_TRACE(
        "rollback_increment_nr_packets_in_transit: attempting to finish "
        "protocol change");

    MYSQL_GCS_LOG_INFO(
        "During rollback of the number of packets in transit, we detected that "
        "we can finish the protocol version change to "
        << gcs_protocol_to_mysql_version(get_protocol_version()).c_str())

    Gcs_xcom_notification *notification = new Protocol_change_notification(
        do_function_finish_protocol_version_change, this, tag);
    bool scheduled = m_gcs_engine.push(notification);
    if (!scheduled) {
      MYSQL_GCS_LOG_DEBUG(
          "Tried to enqueue a protocol change request but the member is "
          "about to stop.")
      delete notification;
    }
  }
}

void Gcs_xcom_communication_protocol_changer::
    wait_for_protocol_change_to_finish() {
  MYSQL_GCS_LOG_TRACE("wait_for_protocol_change_to_finish: waiting");

  std::unique_lock<std::mutex> lock(m_mutex);
  m_protocol_change_finished.wait(
      lock, [this]() { return !is_protocol_change_ongoing(); });

  MYSQL_GCS_LOG_INFO("Successfully changed protocol version to "
                     << gcs_protocol_to_mysql_version(get_protocol_version()));

  MYSQL_GCS_LOG_TRACE("wait_for_protocol_change_to_finish: done");
}

void Gcs_xcom_communication_protocol_changer::
    atomically_increment_nr_packets_in_transit(Cargo_type const &cargo) {
  /*
   If there is a protocol change ongoing, wait until it is over.
   If not, increment the number of in-transit messages.

   Unless we are sending a state exchange message.
   State exchange messages are special because:
   (1) they pose no harm to protocol changes, since they are always sent using
       the original pipeline, and
   (2) they are sent by the GCS thread, which must never block.
   */
  bool need_to_wait_for_protocol_change =
      (cargo != Cargo_type::CT_INTERNAL_STATE_EXCHANGE);
  while (need_to_wait_for_protocol_change) {
    /* Optimistically assume a protocol change will not start meanwhile. */
    bool successful = false;
    Gcs_tagged_lock::Tag tag = 0;
    std::tie(successful, tag) =
        optimistically_increment_nr_packets_in_transit();

    /*
     If a protocol change started meanwhile, rollback the increment to the
     counter of messages in transit.
     */
    bool const protocol_change_started = !successful;
    if (protocol_change_started) {
      MYSQL_GCS_LOG_INFO(
          "Rolling back the last increment for the packets in transit, because "
          "a protocol version change has started.");
      rollback_increment_nr_packets_in_transit(tag);
    }

    need_to_wait_for_protocol_change = protocol_change_started;

    /* A protocol change has started meanwhile, wait for it. */
    if (need_to_wait_for_protocol_change) {
      MYSQL_GCS_LOG_INFO(
          "Protocol version change has started. Not sending new messages until "
          "this "
          "change finishes. The current number of packets in transit is:"
          << get_nr_packets_in_transit());
      wait_for_protocol_change_to_finish();
    }
  }
}

void Gcs_xcom_communication_protocol_changer::adjust_nr_packets_in_transit(
    Cargo_type const &cargo, std::size_t const &nr_additional_packets_to_send) {
  /*
   If the pipeline split the original message, we are going to send more than
   one packet.
   We need to adjust the number of in-transit packets.

   Unless we are sending a state exchange message, because of the reasons
   specified in atomically_increment_nr_packets_in_transit.
   */
  bool const need_to_adjust = (cargo != Cargo_type::CT_INTERNAL_STATE_EXCHANGE);
  if (need_to_adjust) {
    auto previous_nr_packets_in_transit = m_nr_packets_in_transit.fetch_add(
        nr_additional_packets_to_send, std::memory_order_relaxed);

    MYSQL_GCS_LOG_TRACE(
        "adjust_nr_packets_in_transit: nr_packets_in_transit=%d",
        previous_nr_packets_in_transit + nr_additional_packets_to_send);
  }
}

void Gcs_xcom_communication_protocol_changer::decrement_nr_packets_in_transit(
    Gcs_packet const &packet, Gcs_xcom_nodes const &xcom_nodes) {
  assert(packet.get_cargo_type() != Cargo_type::CT_INTERNAL_STATE_EXCHANGE);

  /* Get the packet's origin. */
  auto node_id = packet.get_origin_synode().get_synod().node;
  auto const *node = xcom_nodes.get_node(node_id);

  if (!node) {
    std::ostringstream node_and_nodes;
    node_and_nodes << " requested node_id = " << node_id;
    node_and_nodes << " provided config members:";
    for (const Gcs_xcom_node_information &xcom_node : xcom_nodes.get_nodes()) {
      node_and_nodes << " node_no[" << xcom_node.get_node_no();
      node_and_nodes << "]="
                     << xcom_node.get_member_id().get_member_id().c_str();
    }

    if (!is_protocol_change_ongoing()) {
      std::string log_message(
          "Received a network packet from an unrecognised sender. "
          "Will ignore this message. No need to take any further "
          "action. If this behaviour persists, consider restarting "
          "the group at the next convenient time. Details:");
      log_message.append(node_and_nodes.str().c_str());
      MYSQL_GCS_LOG_WARN(log_message.c_str());
    } else {
      std::string log_message(
          "There is an ongoing Protocol Change and we were not able to "
          "decrement the number of packets in transit due to unrecognised "
          "sender "
          "from an incoming packet. This node will block sending messages. "
          "Consider restarting the group at the next convenient time to fix "
          "it. "
          "Details:");
      log_message.append(node_and_nodes.str().c_str());
      MYSQL_GCS_LOG_ERROR(log_message.c_str());
    }
    return;
  }

  const Gcs_member_identifier origin_member_id = node->get_member_id();
  if (origin_member_id.get_member_id().empty()) {
    MYSQL_GCS_LOG_INFO(
        "Not able to decrement number of packets in transit. Non-existing "
        "member identifier from incoming packet.");
  }

  Gcs_member_identifier origin(origin_member_id);

  /*
   If the packet comes from me, decrement the number of packets in transit.

   Unless it is a state exchange packet, because of the reasons specified in
   atomically_increment_nr_packets_in_transit.
   */
  Gcs_xcom_interface *const xcom_interface =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());
  if (xcom_interface != nullptr) {
    Gcs_xcom_node_address *myself_node_address =
        xcom_interface->get_node_address();

    if (!myself_node_address) {
      MYSQL_GCS_LOG_INFO(
          "Not able to decrement number of packets in transit. Non-existing "
          "own address from currently installed configuration.")
    }

    std::string myself_node_address_string =
        myself_node_address->get_member_address();

    if (myself_node_address_string.empty()) {
      MYSQL_GCS_LOG_INFO(
          "Not able to decrement number of packets in transit. Non-existing "
          "own address representation from currently installed configuration.")
    }

    Gcs_member_identifier myself{myself_node_address_string};

    bool const message_comes_from_me = (origin == myself);
    std::string new_version_readable =
        gcs_protocol_to_mysql_version(get_protocol_version());

    if (message_comes_from_me) {
      assert(get_nr_packets_in_transit() > 0 &&
             "Number of packets in transit should not have been 0");

      // Update number of packets in transit
      auto previous_nr_of_packets_in_transit =
          m_nr_packets_in_transit.fetch_sub(1, std::memory_order_relaxed);

      MYSQL_GCS_LOG_TRACE(
          "decrement_nr_packets_in_transit: nr_packets_in_transit=%d",
          previous_nr_of_packets_in_transit - 1);

      // Finish the protocol change if we delivered the last pending packet.
      bool const delivered_last_pending_packet =
          (previous_nr_of_packets_in_transit == 1);
      if (is_protocol_change_ongoing()) {
        if (delivered_last_pending_packet) {
          MYSQL_GCS_LOG_INFO(
              "Last packet for this protocol version change processed. It is "
              "safe to "
              "change to the new protocol "
              "version: "
              << new_version_readable.c_str())
          commit_protocol_version_change();
          MYSQL_GCS_LOG_INFO("Successfully changed protocol version to "
                             << new_version_readable.c_str());
        } else {
          MYSQL_GCS_LOG_INFO(
              "One ongoing packet decremented. Waiting for more packets to "
              "arrive before changing to new protocol version: "
              << new_version_readable.c_str()
              << ". We are currently waiting for "
              << get_nr_packets_in_transit() << " packets");
        }
      }
    } else {
      if (is_protocol_change_ongoing()) {
        MYSQL_GCS_LOG_INFO(
            "One ongoing packet processeed that was not sent by this node. "
            "Waiting for more packets sent by this node to "
            "arrive before changing new protocol version: "
            << new_version_readable.c_str() << ". We are currently waiting for "
            << get_nr_packets_in_transit() << " packets");
      }
    }
  }
}
