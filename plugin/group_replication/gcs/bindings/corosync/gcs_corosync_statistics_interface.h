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

#ifndef GCS_COROSYNC_STATISTICS_INTERFACE_INCLUDED
#define	GCS_COROSYNC_STATISTICS_INTERFACE_INCLUDED

#include "gcs_statistics_interface.h"

#include <algorithm>
#include <time.h>

using std::max;
using std::min;

/**
  @interface Gcs_corosync_statistics_updater

  This interface contains the public methods that the implementation
  of the Gcs_statistics_interface will provide to the other interfaces that
  update statistics.
 */
class Gcs_corosync_statistics_updater
{
public:
  /**
    Method called to register that a message has been sent.

    This will update:
    - Total Messages Sent
    - Total Bytes Sent

    @param[in] message_length the length of the message being sent
   */
  virtual void update_message_sent(long message_length)= 0;

  /**
    Method called to register when a message is received

    This will update:
    - Total Messages Received
    - Total Bytes Received
    - Min Message Length
    - Max Message Lenghth
    - Last Message Timestamp

    @param[in] message_length the length of the message received
   */
  virtual void update_message_received(long message_length)= 0;

  virtual ~Gcs_corosync_statistics_updater(){}
};

/**
  @class Gcs_corosync_statistics_interface

  This class implements the Gcs_statistics_interface and updater
 */
class Gcs_corosync_statistics : public Gcs_statistics_interface,
                                public Gcs_corosync_statistics_updater
{
public:
  Gcs_corosync_statistics();
  virtual ~Gcs_corosync_statistics();

  //Implementation of Gcs_statistics_interface
  long get_total_messages_sent();

  long get_total_bytes_sent();

  long get_total_messages_received();

  long get_total_bytes_received();

  long get_min_message_length();

  long get_max_message_length();

  long get_last_message_timestamp();

  // Implementation of Gcs_corosync_statistics_updater
  void update_message_sent(long message_length);

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

#endif	/* GCS_COROSYNC_STATISTICS_INTERFACE_INCLUDED */

