/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
#ifndef GCS_STATS_H
#define GCS_STATS_H

#include "gcs_protocol.h"

namespace GCS
{

// Astha,Nuno-todo: wl7331 review - aggregated changes to Stats
// due to wl#7332 and 7331 logics.

/**
   GCS statitics collector.
   getters are wrapped into C-style functions to be invoked by the server
   (see gcs_plugin.cc), setters are worked in binding effectively simulating
   the stats layer of GCS.
   The stats collector references View instance to define few access methods
   on attributes of the View.
*/
class Stats
{
public:
  Stats() :
    number_of_nodes(0),
    total_messages_sent(0), total_bytes_sent(0),
    total_messages_received(0), total_bytes_received(0),
    min_message_length(0), max_message_length(0),
    last_message_timestamp(0), cluster_view(NULL)
    {};
  void reset()
  {
    number_of_nodes= 0;
    total_messages_sent= 0;
    total_bytes_sent= 0;
    total_messages_received= 0;
    total_bytes_received= 0;
    min_message_length= 0;
    max_message_length= 0;
    last_message_timestamp=0;
  }
  void update_per_message_delivery(ulonglong len)
  {
    if (len > max_message_length)
      max_message_length= len;
    if (len < min_message_length || min_message_length == 0)
      min_message_length= len;
    set_last_message_timestamp();
    total_messages_received++;
    total_bytes_received += len;
  };

  void update_per_message_sent(ulonglong len)
  {
    total_messages_sent++;
    total_bytes_sent += len;
  };

  void update_per_view_change()
  {
    number_of_nodes= cluster_view->get_members().size();
  }

  void   set_last_message_timestamp(time_t arg= 0)
  {
    last_message_timestamp= (arg == 0 ? time(0): arg);
  };
  time_t get_last_message_timestamp() { return last_message_timestamp; };

  ulong get_view_id() { return cluster_view->get_view_id(); };
  const char* get_node_id(ulong index)
  {
    return get_number_of_nodes() == 0 ? NULL :
      cluster_view->get_member(index).get_uuid().c_str();
  };

  // TODO/fixme: wl7331 provide the exact def of the node address. Does it include port?
  // I guess it should 'cos
  // PERFORMANCE_SCHEMA.REPLICATION_CONNECTION_NODES does not specify port.

  const char* get_node_address(ulong index)
  {
    return get_number_of_nodes() == 0 ? NULL :
      cluster_view->get_member(index).get_hostname().c_str();
  };
  uint get_number_of_nodes() { return number_of_nodes; };
  ulonglong get_total_messages_sent() { return total_messages_sent; };
  ulonglong get_total_bytes_sent() { return total_bytes_sent; };
  ulonglong get_total_messages_received() { return total_messages_received; };
  ulonglong get_total_bytes_received() { return total_bytes_received; };
  ulong get_min_message_length() { return min_message_length; };
  ulong get_max_message_length() { return max_message_length; };
  void  set_view(View* view_arg) { cluster_view= view_arg; };

private:

  /*
     Todo: most of the attributes require atomic type.
     Convert into that.
  */

  uint number_of_nodes;  // todo: can be optimized out, moreover replaced with a string containing all node id:s
  ulonglong total_messages_sent;
  ulonglong total_bytes_sent;
  ulonglong total_messages_received;
  ulonglong total_bytes_received;
  ulong min_message_length;
  ulong max_message_length;
  time_t    last_message_timestamp;
  View*  cluster_view;
};

} // end of namespace

#endif
