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

#include "gcs_corosync_statistics_interface.h"

Gcs_corosync_statistics::Gcs_corosync_statistics()
{
  total_bytes_received= 0;
  total_bytes_sent= 0;
  total_messages_received= 0;
  total_messages_sent= 0;
  max_message_length= 0;
  min_message_length= 0;
}

Gcs_corosync_statistics::~Gcs_corosync_statistics()
{
}

long
Gcs_corosync_statistics::get_total_messages_sent()
{
  return total_messages_sent;
}

long
Gcs_corosync_statistics::get_total_bytes_sent()
{
  return total_bytes_sent;
}

long
Gcs_corosync_statistics::get_total_messages_received()
{
  return total_messages_received;
}

long
Gcs_corosync_statistics::get_total_bytes_received()
{
  return total_bytes_received;
}

long
Gcs_corosync_statistics::get_min_message_length()
{
  return min_message_length;
}

long
Gcs_corosync_statistics::get_max_message_length()
{
  return max_message_length;
}

long
Gcs_corosync_statistics::get_last_message_timestamp()
{
  return last_message_timestamp;
}

void
Gcs_corosync_statistics::update_message_sent(long message_length)
{
    total_messages_sent++;
    total_bytes_sent += message_length;
}

void
Gcs_corosync_statistics::update_message_received(long message_length)
{
    max_message_length= max(max_message_length, message_length);

    //Make the first initialization of min_message_length here
    if(min_message_length == 0)
    {
      min_message_length= message_length;
    }
    min_message_length= min(min_message_length, message_length);

    last_message_timestamp= time(0);

    total_messages_received++;
    total_bytes_received += message_length;
}

