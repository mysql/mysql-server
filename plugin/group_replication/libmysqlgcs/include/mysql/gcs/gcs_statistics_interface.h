/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_STATISTICS_INTERFACE_INCLUDED
#define GCS_STATISTICS_INTERFACE_INCLUDED

/**
  @class Gcs_statistics_interface

  This interface represents all statistics that a binding implementation should
  provide.
*/
class Gcs_statistics_interface {
 public:
  /**
    @return the total number of messages sent via the Communication Interface
  */

  virtual long get_total_messages_sent() = 0;

  /**
    @return the total amount of data sent via the Communication Interface
  */

  virtual long get_total_bytes_sent() = 0;

  /**
    @return the total number of messages received via the Communication
            Interface
  */

  virtual long get_total_messages_received() = 0;

  /**
    @return the total amount of data received via the Communication Interface
  */

  virtual long get_total_bytes_received() = 0;

  /**
    @return the smallest amount of data received in a message via the
            Communication Interface
  */

  virtual long get_min_message_length() = 0;

  /**
    @return the biggest amount of data received in a message via the
            Communication Interface
  */

  virtual long get_max_message_length() = 0;

  /**
    @return the timestamp in which the last message was received
  */

  virtual long get_last_message_timestamp() = 0;

  virtual ~Gcs_statistics_interface() = default;
};

#endif  // GCS_STATISTICS_INTERFACE_INCLUDED
