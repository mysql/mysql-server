/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef GCS_COMMUNICATION_INTERFACE_INCLUDED
#define GCS_COMMUNICATION_INTERFACE_INCLUDED

#include "gcs_communication_event_listener.h"
#include "gcs_message.h"

/**
  @class Gcs_communication_interface

  This interface represents all the communication facilities that a binding
  implementation should provide. For now, it is limited to sending and receiving
  messages.
 */
class Gcs_communication_interface
{
public:
  /**
    Method used to broadcast a message to a group

    @param[in] message_to_send the Gcs_message object to send
    @return true if any problem occurs like not being in a group
  */
  virtual bool send_message(Gcs_message *message_to_send)= 0;

  /**
    Registers an implementation of a Gcs_communication_event_listener that will
    receive Communication Events

    @param[in] event_listener a class that implements Gcs_communication_event_listener
    @return an handle representing the registration of this object to
            be used in remove_event_listener
   */
  virtual int
        add_event_listener(Gcs_communication_event_listener *event_listener)= 0;

  /**
    Removes a previously registered event listener

    @param[in] event_listener_handle the handle returned when the listener was
                                 registered
   */
  virtual void remove_event_listener(int event_listener_handle)= 0;

public:
  virtual ~Gcs_communication_interface()
  {
  }
};

#endif // GCS_COMMUNICATION_INTERFACE_H
