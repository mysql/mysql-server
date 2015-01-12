/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

enum enum_member_state {
  MEMBER_STATE_ONLINE= 1,
  MEMBER_STATE_OFFLINE,
  MEMBER_STATE_RECOVERING
};

enum enum_applier_status {
  APPLIER_STATE_RUNNING= 1,
  APPLIER_STATE_STOP,
  APPLIER_STATE_ERROR
};

typedef struct st_rpl_gcs_group_members_info
{
  char* channel_name;
  const char* member_id;
  const char* member_address;
  enum enum_member_state member_state;
} RPL_GCS_GROUP_MEMBERS_INFO;

typedef struct st_rpl_gcs_connection_status_info
{
  char* group_name;
  bool node_state;
  time_t last_message_timestamp;
} RPL_GCS_CONNECTION_STATUS_INFO;

typedef struct st_rpl_gcs_member_stats_info
{
  char* channel_name;
  char* view_id;
  const char* member_id;
  ulonglong transaction_in_queue;
  ulonglong transaction_certified;
  ulonglong transaction_conflicts_detected;
  ulonglong transactions_in_validation;
  char* committed_transations;
  const char* last_conflict_free_transaction;
} RPL_GCS_GROUP_MEMBER_STATS_INFO;

struct st_mysql_gcs_rpl
{
  int interface_version;

  /*
    This function is used to fetch information for gcs kernel stats.
  */
  bool (*get_gcs_connection_status_info)(RPL_GCS_CONNECTION_STATUS_INFO *info);

  /*
    This function is used to fetch information for gcs members.
  */
  bool (*get_gcs_group_members_info)(uint index, RPL_GCS_GROUP_MEMBERS_INFO *info);

  /*
    This function is used to fetch information for gcs members statistics.
  */
  bool (*get_gcs_group_member_stats_info)(RPL_GCS_GROUP_MEMBER_STATS_INFO* info);

  /*
    Get number of gcs members.
  */
  uint (*get_gcs_members_number_info)();

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
  /*
   This function initializes conflict checking module with info received
   from group.

   @param info  View_change_log_event with conflict checking info.
  */
  int (*gcs_set_retrieved_cert_info)(void* info);
};

#endif

