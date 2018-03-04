/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_COMMUNICATION_EVENT_LISTENER_INCLUDED
#define GCS_COMMUNICATION_EVENT_LISTENER_INCLUDED

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"

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
