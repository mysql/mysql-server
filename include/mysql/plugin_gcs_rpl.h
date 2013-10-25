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

#ifndef MYSQL_PLUGIN_GCS_REPLICATION_INCLUDED
#define MYSQL_PLUGIN_GCS_REPLICATION_INCLUDED

/* API for gcs replication plugin. (MYSQL_GCS_RPL_PLUGIN) */

#include <mysql/plugin.h>
#define MYSQL_GCS_REPLICATION_INTERFACE_VERSION 0x0100

typedef struct st_rpl_gcs_nodes_info
{
  char*group_name;
  uint node_id;
  bool node_state;
} RPL_GCS_NODES_INFO;

typedef struct st_rpl_gcs_stats_info
{
  char* group_name;
  bool node_state;
  ulong view_id;
  ulonglong total_messages_sent;
  ulonglong total_bytes_sent;
  ulonglong total_messages_received;
  ulonglong total_bytes_received;
  time_t last_message_timestamp;
  ulong min_message_length;
  ulong max_message_length;
  uint number_of_nodes;
} RPL_GCS_STATS_INFO;

struct st_mysql_gcs_rpl
{
  int interface_version;

  /*
    This function is used to fetch information for gcs kernel stats.
  */
  bool (*get_gcs_stats_info)(RPL_GCS_STATS_INFO *info);

  /*
    This function is used to fetch information for gcs nodes.
  */
  bool (*get_gcs_nodes_info)(RPL_GCS_NODES_INFO *info);
  /*
    This function is to used to start the gcs replication based on the
    gcs group that is specified by the user.
  */
  int (*gcs_rpl_start)();
  /*
    This function is used by the stop the gcs replication based in a given
    group.
  */
  int (*gcs_rpl_stop)();
  /*
    This function is used to get the current gcs plugin running status.
  */
  bool (*is_gcs_rpl_running)();
};

#endif

