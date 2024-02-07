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

#ifndef GCS_XCOM_COMMUNICATION_INTERFACE_INCLUDED
#define GCS_XCOM_COMMUNICATION_INTERFACE_INCLUDED

#include <cstdlib>
#include <map>      // std::map
#include <utility>  // std::pair
#include <vector>   // std::vector

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_communication_protocol_changer.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_notification.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_proxy.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_state_exchange.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_manager.h"

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/include/network_management_interface.h"

/**
  @class Gcs_xcom_communication_interface

  Abstraction layer that adds XCom specific methods to the generic
  communication interface.

  This adds the following functionalities to the generic
  Gcs_communication_interface:
  - Ability to send messages without view safety and stats counter. This method
    shall be used by the State Exchange algorithm when the high-level view
    change is still occurring.
  - Delegation method that will contain all the business logic related with
    messages delivery to registered clients.
*/
class Gcs_xcom_communication_interface : public Gcs_communication_interface {
 public:
  /**
    Sends a message that is internal to the binding implementation.
    This message will not be subject to the same restrictions of send_message.
    As such, it will not observe view safety nor will count for the statistics
    of messages sent.

    @param[in]  message_to_send the message to send
    @param[out] message_length the length of message which was send if GCS_OK,
                               unspecified otherwise
    @param[in] cargo internal message header cargo type
    @return the xcom broadcast message error
      @retval GCS_OK message is transmitted successfully
      @retval GCS_NOK error occurred while transmitting message
      @retval GCS_MESSAGE_TOO_BIG message is bigger then xcom can handle

  */

  virtual enum_gcs_error do_send_message(const Gcs_message &message_to_send,
                                         unsigned long long *message_length,
                                         Cargo_type cargo) = 0;

  virtual Gcs_message_pipeline &get_msg_pipeline() = 0;

  /**
    Buffer packets when a view is not installed yet and the state
    exchange phase is being executed.

    Note that this method must be executed by the same thread that
    processes global view messages and data message in order to
    avoid any concurrency issue.

    @param packet Packet to buffer.
    @param xcom_nodes Membership at the time the packet was received
  */

  virtual void buffer_incoming_packet(
      Gcs_packet &&packet, std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) = 0;

  /**
    The state exchange phase has been executed and the view has been
    installed so this is used to send any buffered packet to upper
    layers.

    Note that this method must be executed by the same thread that
    processes global view messages and data message in order to
    avoid any concurrency issue.
  */

  virtual void deliver_buffered_packets() = 0;

  /*
    Clean up possible buffered packets that were not delivered to
    upper layers because the state exchange has not finished and a
    new global view message was received triggering a new state
    exchange phase.

    Note that this method must be executed by the same thread that
    processes global view messages and data message in order to
    avoid any concurrency issue.
  */

  virtual void cleanup_buffered_packets() = 0;

  /**
    Return the number of buffered packets.

    Note that this method must be executed by the same thread that
    processes global view messages and data message in order to
    avoid any concurrency issue.
  */

  virtual size_t number_buffered_packets() = 0;

  /**
   Notify the pipeline about the new XCom membership when a state exchange
   begins.

   Note that this method must be executed by the same thread that processes
   global view messages and data message in order to avoid any concurrency
   issue.

   @param me The identifier of this server
   @param members The XCom membership
   */
  virtual void update_members_information(const Gcs_member_identifier &me,
                                          const Gcs_xcom_nodes &members) = 0;

  /**
   Attempts to recover the missing packets that are required for a node to
   join the group successfully.
   For example, the missing packets may be some fragments of a message that
   have already been delivered by XCom to the existing members of the group.
   The joining node needs those fragments in order to be able to deliver the
   reassembled message when the final fragments are delivered by XCom.

   Note that this method must be executed by the same thread that processes
   global view messages and data message in order to avoid any concurrency
   issue.

   @param synodes The synodes where the required packets were decided
   @returns true If successful, false otherwise
   */
  virtual bool recover_packets(Gcs_xcom_synode_set const &synodes) = 0;

  /**
   Converts the packet into a message that can be delivered to the upper
   layer.

   @param packet The packet to convert
   @param xcom_nodes The membership at the time the packet was delivered
   @retval Gcs_message* if successful
   @retval nullptr if unsuccessful
   */
  virtual Gcs_message *convert_packet_to_message(
      Gcs_packet &&packet, std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) = 0;

  /**
   The purpose of this method is to be called when in Gcs_xcom_interface
   callback method xcom_receive_data is invoked.

   This allows, in terms of software architecture, to concentrate all the
   message delivery logic and processing in a single place.

   The deliver_message callback that is registered in XCom
   (in gcs_xcom_interface.h) and that actually receives the low-level
   messages, is implemented as a delegator to this method.

   Note that the method will be responsible for deleting the message
   passed as parameter and must be executed by the same thread that
   processes global view messages and data message in order to avoid
   any concurrency issue.
   */

  virtual void process_user_data_packet(
      Gcs_packet &&packet, std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) = 0;

  ~Gcs_xcom_communication_interface() override = default;
};

/**
  @class Gcs_xcom_communication_interface

  Implementation of the Gcs_communication_interface for xcom.
*/
class Gcs_xcom_communication : public Gcs_xcom_communication_interface {
 public:
  /**
    Gcs_xcom_communication_interface constructor.

    @param[in] stats a reference to the statistics interface
    @param[in] proxy a reference to an implementation of
                 Gcs_xcom_communication_proxy
    @param[in] view_control a reference to a
    gcs_xcom_view_change_control_interface implementation
    @param[in] gcs_engine Pointer to gcs engine
    @param[in] group_id reference to the group identifier
    @param[in] comms_mgmt an unique_ptr to a
                          Network_provider_management_interface
  */

  explicit Gcs_xcom_communication(
      Gcs_xcom_statistics_manager_interface *stats, Gcs_xcom_proxy *proxy,
      Gcs_xcom_view_change_control_interface *view_control,
      Gcs_xcom_engine *gcs_engine, Gcs_group_identifier const &group_id,
      std::unique_ptr<Network_provider_management_interface> comms_mgmt);

  ~Gcs_xcom_communication() override;

  // Implementation of the Gcs_communication_interface

  /**
    Implementation of the public send_message method defined in
    Gcs_xcom_communication.
    Besides sending a message to the group, this method does two extra things:
    - Guarantees view safety, in which no messages can be sent when a view
    change is occurring.
    - Registers in the statistics interface that a message was sent.

    @param[in] message_to_send the message to send
    @return the xcom broadcast message error
      @retval GCS_OK when message is transmitted successfully
      @retval GCS_NOK when error occurred while transmitting message
      @retval GCS_MESSAGE_TOO_BIG when message is bigger then
                                   xcom can handle
  */

  enum_gcs_error send_message(const Gcs_message &message_to_send) override;

  int add_event_listener(
      const Gcs_communication_event_listener &event_listener) override;

  void remove_event_listener(int event_listener_handle) override;

  // Implementation of the Gcs_xcom_communication_interface
  enum_gcs_error do_send_message(const Gcs_message &message_to_send,
                                 unsigned long long *message_length,
                                 Cargo_type cargo) override;

  // For unit testing purposes
  std::map<int, const Gcs_communication_event_listener &>
      *get_event_listeners();

  Gcs_message_pipeline &get_msg_pipeline() override { return m_msg_pipeline; }

  void buffer_incoming_packet(
      Gcs_packet &&packet,
      std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) override;

  void deliver_buffered_packets() override;

  void cleanup_buffered_packets() override;

  size_t number_buffered_packets() override;

  void update_members_information(const Gcs_member_identifier &me,
                                  const Gcs_xcom_nodes &members) override;

  bool recover_packets(Gcs_xcom_synode_set const &synodes) override;

  Gcs_message *convert_packet_to_message(
      Gcs_packet &&packet,
      std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) override;

  void process_user_data_packet(
      Gcs_packet &&packet,
      std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes) override;

  Gcs_protocol_version get_protocol_version() const override;

  std::pair<bool, std::future<void>> set_protocol_version(
      Gcs_protocol_version new_version) override;

  Gcs_protocol_version get_maximum_supported_protocol_version() const override;

  void set_maximum_supported_protocol_version(Gcs_protocol_version version);

  void set_communication_protocol(enum_transport_protocol protocol) override;

  enum_transport_protocol get_incoming_connections_protocol() override;

 private:
  // Registered event listeners
  std::map<int, const Gcs_communication_event_listener &> event_listeners;

  // Reference to the stats updater interface
  Gcs_xcom_statistics_manager_interface *m_stats;

  // Reference to the xcom proxy interface
  Gcs_xcom_proxy *m_xcom_proxy;

  // Reference to the view change control object
  Gcs_xcom_view_change_control_interface *m_view_control;

  /**
   The pipeline of stages a message has to go through before it is delivered
   to the application or sent to the network.
   */
  Gcs_message_pipeline m_msg_pipeline;

  /**
    Buffer that is used to store packets while the node is about to install
    a view and is running the state exchange phase.
  */
  std::vector<std::pair<Gcs_packet, std::unique_ptr<Gcs_xcom_nodes>>>
      m_buffered_packets;

  /** Most recent XCom membership known. */
  Gcs_xcom_nodes m_xcom_nodes;

  /** Hash of the group. */
  unsigned int m_gid_hash;

  /** Protocol changer. */
  Gcs_xcom_communication_protocol_changer m_protocol_changer;

  /***/
  std::unique_ptr<Network_provider_management_interface> m_comms_mgmt_interface;

  /** Notify upper layers that a message has been received. */
  void notify_received_message(std::unique_ptr<Gcs_message> &&message);

  /** Delivers the packet to the upper layer. */
  void deliver_user_data_packet(Gcs_packet &&packet,
                                std::unique_ptr<Gcs_xcom_nodes> &&xcom_nodes);

  /**
   @returns the list of possible donors from which to recover the missing
   packets this server requires to successfully join the group.
   */
  std::vector<Gcs_xcom_node_information> possible_packet_recovery_donors()
      const;

  /**
   Error code for the packet recovery proceess.
   */
  enum class packet_recovery_result {
    OK,
    PACKETS_UNRECOVERABLE,
    NO_MEMORY,
    PIPELINE_ERROR,
    PIPELINE_UNEXPECTED_OUTPUT,
    PACKET_UNEXPECTED_CARGO,
    ERROR
  };

  /**
   Attempts to recover the packets delivered in @c synodes from @c donor.

   @c recovered_data is an out parameter.
   */
  packet_recovery_result recover_packets_from_donor(
      Gcs_xcom_node_information const &donor,
      Gcs_xcom_synode_set const &synodes,
      synode_app_data_array &recovered_data);

  /**
   Processes all the recovered packets.
   */
  packet_recovery_result process_recovered_packets(
      synode_app_data_array const &recovered_data);

  /**
   Processes a single recovered packet.
   */
  packet_recovery_result process_recovered_packet(
      synode_app_data const &recovered_data);

  /**
   Logs the packet recovery failure.
   */
  void log_packet_recovery_failure(
      packet_recovery_result const &error_code,
      Gcs_xcom_node_information const &donor) const;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_communication(const Gcs_xcom_communication &);
  Gcs_xcom_communication &operator=(const Gcs_xcom_communication &);
};

#endif /* GCS_XCOM_COMMUNICATION_INTERFACE_INCLUDED */
