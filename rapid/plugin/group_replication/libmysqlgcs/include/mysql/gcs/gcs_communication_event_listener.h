/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_COMMUNICATION_EVENT_LISTENER_INCLUDED
#define GCS_COMMUNICATION_EVENT_LISTENER_INCLUDED

#include "gcs_message.h"

/**
  @class Gcs_communication_event_listener

  This interface is implemented by those who wish to receive messages.

  Please check a working example in Gcs_communication_interface.
*/
class Gcs_communication_event_listener
{
public:
  /**
    This method is called whenever a message is to be delivered.

    @param[in] message The received message. This is currently a
                       reference to avoid the existence of dangling
                       pointers.
  */

  virtual void on_message_received(const Gcs_message &message) const= 0;


  virtual ~Gcs_communication_event_listener() {}
};

#endif // GCS_COMMUNICATION_EVENT_LISTENER_INCLUDED
