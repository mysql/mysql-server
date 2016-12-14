/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_xcom_statistics_interface.h"
/* purecov: begin deadcode */
using std::max;
using std::min;

Gcs_xcom_statistics::Gcs_xcom_statistics()
  :total_messages_sent(0),
   total_bytes_sent(0),
   total_messages_received(0),
   total_bytes_received(0),
   min_message_length(0),
   max_message_length(0),
   last_message_timestamp(0)
{}


Gcs_xcom_statistics::~Gcs_xcom_statistics() {}


long Gcs_xcom_statistics::get_total_messages_sent()
{
  return total_messages_sent;
}


long Gcs_xcom_statistics::get_total_bytes_sent()
{
  return total_bytes_sent;
}


long Gcs_xcom_statistics::get_total_messages_received()
{
  return total_messages_received;
}


long Gcs_xcom_statistics::get_total_bytes_received()
{
  return total_bytes_received;
}


long Gcs_xcom_statistics::get_min_message_length()
{
  return min_message_length;
}


long Gcs_xcom_statistics::get_max_message_length()
{
  return max_message_length;
}


long Gcs_xcom_statistics::get_last_message_timestamp()
{
  return last_message_timestamp;
}
/* purecov: end*/

void Gcs_xcom_statistics::update_message_sent(unsigned long long message_length)
{
  total_messages_sent++;
  total_bytes_sent+= message_length;
}


void Gcs_xcom_statistics::update_message_received(long message_length)
{
  max_message_length= max(max_message_length, message_length);

  // Make the first initialization of min_message_length here
  if (min_message_length == 0)
    min_message_length= message_length;

  min_message_length= min(min_message_length, message_length);

  total_messages_received++;
  total_bytes_received+= message_length;
}
