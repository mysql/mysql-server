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

#ifndef GCS_XCOM_COMMUNICATION_INTERFACE_INCLUDED
#define GCS_XCOM_COMMUNICATION_INTERFACE_INCLUDED

#include <cstdlib>
#include <map>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_state_exchange.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"

/**
  @interface Gcs_xcom_communication_interface

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
    @param[out] message_length  the length of message which was send
    @param[in] cargo internal message header cargo type
    @return the xcom broadcast message error
      @retval GCS_OK message is transmitted successfully
      @retval GCS_NOK error occurred while transmitting message
      @retval GCS_MESSAGE_TOO_BIG message is bigger then xcom can handle

  */

  virtual enum_gcs_error send_binding_message(
      const Gcs_message &message_to_send, unsigned long long *message_length,
      Gcs_internal_message_header::cargo_type cargo) = 0;

  /**
    The purpose of this method is to be called when in Gcs_xcom_interface
    callback method xcom_receive_data is invoked.

    This allows, in terms of software architecture, to concentrate all the
    message delivery logic and processing in a single place.

    The deliver_message callback that is registered in XCom
    (in gcs_xcom_interface.h) and that actually receives the low-level
    messages, is implemented as a delegator to this method.

    Note that the method will be responsible for deleting the message
    passed as parameter and must be excuted by the same thread that
    processes global view messages and data message in order to avoid
    any concurrency issue.

    @param message
  */

  virtual bool xcom_receive_data(Gcs_message *message) = 0;

  /**
    Buffer messages when a view is not installed yet and the state
    exchange phase is being executed.

    Note that this method must be excuted by the same thread that
    processes global view messages and data message in order to
    avoid any concurrency issue.

    @param[in] message Message to buffer.
  */

  virtual void buffer_message(Gcs_message *message) = 0;

  /**
    The state exchange phase has been executed and the view has been
    installed so this is used to send any buffered message to upper
    layers.

    Note that this method must be excuted by the same thread that
    processes global view messages and data message in order to
    avoid any concurrency issue.
  */

  virtual void deliver_buffered_messages() = 0;

  /**
    Clean up possible buffered messages that were not delivered to
    upper layers because the state exchange has not finished and a
    new global view message was received triggering a new state
    exchange phase.

    Note that this method must be excuted by the same thread that
    processes global view messages and data message in order to
    avoid any concurrency issue.
  */

  virtual void cleanup_buffered_messages() = 0;

  /**
    Return the number of buffered messages.

    Note that this method must be excuted by the same thread that
    processes global view messages and data message in order to
    avoid any concurrency issue.
  */

  virtual size_t number_buffered_messages() = 0;

  virtual ~Gcs_xcom_communication_interface() {}
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
  */

  explicit Gcs_xcom_communication(
      Gcs_xcom_statistics_updater *stats, Gcs_xcom_proxy *proxy,
      Gcs_xcom_view_change_control_interface *view_control);

  virtual ~Gcs_xcom_communication();

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
      @retval GCS_OK, when message is transmitted successfully
      @retval GCS_NOK, when error occurred while transmitting message
      @retval GCS_MESSAGE_TOO_BIG, when message is bigger then
                                   xcom can handle
  */

  enum_gcs_error send_message(const Gcs_message &message_to_send);

  int add_event_listener(
      const Gcs_communication_event_listener &event_listener);

  void remove_event_listener(int event_listener_handle);

  /**
    Delegation method from Gcs_xcom_interface object.

    @return true - delivered to upper layers, false - buffered
  */
  bool xcom_receive_data(Gcs_message *message);

  // Implementation of the Gcs_xcom_communication_interface
  enum_gcs_error send_binding_message(
      const Gcs_message &message_to_send, unsigned long long *message_length,
      Gcs_internal_message_header::cargo_type cargo);

  // For unit testing purposes
  std::map<int, const Gcs_communication_event_listener &>
      *get_event_listeners();

  Gcs_message_pipeline &get_msg_pipeline() { return m_msg_pipeline; }

  void buffer_message(Gcs_message *message);

  void deliver_buffered_messages();

  void cleanup_buffered_messages();

  size_t number_buffered_messages();

 private:
  // Registered event listeners
  std::map<int, const Gcs_communication_event_listener &> event_listeners;

  // Reference to the stats updater interface
  Gcs_xcom_statistics_updater *stats;

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
    Buffer that is used to store messages while the node is about to install
    a view and is running the state exchange phase.
  */
  std::vector<Gcs_message *> m_buffered_messages;

  /**
    Notify upper layers that a message has been received.
  */
  void notify_received_message(Gcs_message *message);

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_communication(const Gcs_xcom_communication &);
  Gcs_xcom_communication &operator=(const Gcs_xcom_communication &);
};

#endif /* GCS_XCOM_COMMUNICATION_INTERFACE_INCLUDED */
