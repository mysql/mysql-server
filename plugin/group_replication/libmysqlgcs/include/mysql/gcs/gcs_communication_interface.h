/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_COMMUNICATION_INTERFACE_INCLUDED
#define GCS_COMMUNICATION_INTERFACE_INCLUDED

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_event_listener.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"

/**
  @class Gcs_communication_interface

  This interface represents all the communication facilities that a binding
  implementation should provide. Like all interfaces in this API, it is
  group-oriented, meaning that a single instance shall exist per group,
  as it was returned by the class Gcs_interface::get_communication_session.

  It has two major subsections:
  - Message sending: represented by the method send_message, it will
    broadcast a message to the group in which this interface is bound.
    All message data is encapsulated in the Gcs_message object that
    contains more detail about the information to be sent.
    Guarantees are bound to the binding implementation. This means that the
    implementation shall send message of this message and shall not be
    maintained between different groups.

  - Message receiving: Due to the asynchronous nature of this interface,
    message reception is done via an event. They shall be delivered in
    form of callbacks defined in Gcs_communication_event_listener class,
    that should be implemented by a client of this interface interested
    in receiving those notifications.

  A typical usage for this interface would be:

  @code{.cpp}
  class my_Gcs_communication_event_listener: Gcs_communication_event_listener
  {
    void on_message_received(const Gcs_message &message)
    {
      //Do something when message arrives...
    }
  }

  //meanwhile in your client code...
  Gcs_communication_interface *gcs_comm_interface_instance; // obtained
                                                            // previously

  Gcs_communication_event_listener listener_instance;

  int ref_handler=
    gcs_comm_interface_instance->add_event_listener(listener_instance);

  Gcs_message *msg= new Gcs_message(<message_params>);

  gcs_comm_interface_instance->send_message(msg);

  //Normal program flow...

  //In the end...
  gcs_comm_interface_instance->remove_event_listener(ref_handler);

  @endcode

*/
class Gcs_communication_interface {
 public:
  /**
    Method used to broadcast a message to a group in which this interface
    pertains.

    Note that one must belong to an active group to send messages.

    @param[in] message_to_send the Gcs_message object to send
    @return A gcs_error value
      @retval GCS_OK When message is transmitted successfully
      @retval GCS_ERROR When error occurred while transmitting message
      @retval GCS_MESSAGE_TOO_BIG When message is bigger than
                                  the GCS can handle
  */

  virtual enum_gcs_error send_message(const Gcs_message &message_to_send) = 0;

  /**
    Registers an implementation of a Gcs_communication_event_listener that will
    receive Communication Events.

    Note that a binding implementation shall not offer the possibility of
    changing listeners while the system is up and running. In that sense,
    listeners must be added to it only when booting up the system.

    @param[in] event_listener a class that implements
                              Gcs_communication_event_listener
    @return an handle representing the registration of this object to
            be used in remove_event_listener
  */

  virtual int add_event_listener(
      const Gcs_communication_event_listener &event_listener) = 0;

  /**
    Removes a previously registered event listener.

    Note that a binding implementation shall not offer the possibility of
    changing listeners while the system is up and running. In that sense
    listeners must be removed from it only when shutting down the system.

    @param[in] event_listener_handle the handle returned when the listener was
                                 registered
  */

  virtual void remove_event_listener(int event_listener_handle) = 0;

  virtual ~Gcs_communication_interface() {}
};

#endif  // GCS_COMMUNICATION_INTERFACE_H
