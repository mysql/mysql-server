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

#ifndef GCS_COMMUNICATION_EVENT_LISTENER_INCLUDED
#define GCS_COMMUNICATION_EVENT_LISTENER_INCLUDED

#include "gcs_message.h"

/**
  @class Gcs_communication_event_listener

  This interface is implemented by those who wish to receive messages.
 */
class Gcs_communication_event_listener
{
public:
  /**
   This method is called whenever a message is to be delivered

   @param[in] message the received message
   */
  virtual void on_message_received(Gcs_message& message)= 0;

public:
  virtual ~Gcs_communication_event_listener()
  {
  }
};

#endif // GCS_COMMUNICATION_EVENT_LISTENER_INCLUDED
