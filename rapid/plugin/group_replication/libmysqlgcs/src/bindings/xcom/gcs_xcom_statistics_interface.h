/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_XCOM_STATISTICS_INTERFACE_INCLUDED
#define GCS_XCOM_STATISTICS_INTERFACE_INCLUDED

#include <time.h>
#include <algorithm>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_statistics_interface.h"


/**
  @interface Gcs_xcom_statistics_updater

  This interface contains the public methods that the implementation
  of the Gcs_statistics_interface will provide to the other interfaces that
  update statistics.
*/
class Gcs_xcom_statistics_updater
{
public:
  /**
    Method called to register that a message has been sent.

    This will update:
    - Total Messages Sent;
    - Total Bytes Sent.

    @param[in] message_length the length of the message being sent
   */

  virtual void update_message_sent(unsigned long long message_length)= 0;


  /**
    Method called to register when a message is received.

    This will update:
    - Total Messages Received;
    - Total Bytes Received;
    - Min Message Length;
    - Max Message Lenghth;
    - Last Message Timestamp.

    @param[in] message_length the length of the message received
   */

  virtual void update_message_received(long message_length)= 0;


  virtual ~Gcs_xcom_statistics_updater() {}
};


/**
  @class Gcs_xcom_statistics_interface

  This class implements the Gcs_statistics_interface and updater.
*/
class Gcs_xcom_statistics
  :public Gcs_statistics_interface, public Gcs_xcom_statistics_updater
{
public:
  explicit Gcs_xcom_statistics();
  virtual ~Gcs_xcom_statistics();

  // Implementation of Gcs_statistics_interface
  long get_total_messages_sent();

  long get_total_bytes_sent();

  long get_total_messages_received();

  long get_total_bytes_received();

  long get_min_message_length();

  long get_max_message_length();

  long get_last_message_timestamp();

  // Implementation of Gcs_xcom_statistics_updater
  void update_message_sent(unsigned long long message_length);

  void update_message_received(long message_length);

private:

  long total_messages_sent;
  long total_bytes_sent;
  long total_messages_received;
  long total_bytes_received;
  long min_message_length;
  long max_message_length;
  long last_message_timestamp;
};

#endif  /* GCS_XCOM_STATISTICS_INTERFACE_INCLUDED */
