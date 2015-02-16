/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PLUGIN_GROUP_REPLICATION_INCLUDED
#define MYSQL_PLUGIN_GROUP_REPLICATION_INCLUDED

/* API for Group Peplication plugin. (MYSQL_GROUP_REPLICATION_PLUGIN) */

#include <mysql/plugin.h>
#define MYSQL_GROUP_REPLICATION_INTERFACE_VERSION 0x0100

enum enum_member_state {
  MEMBER_STATE_ONLINE= 1,
  MEMBER_STATE_OFFLINE,
  MEMBER_STATE_RECOVERING
};

typedef struct st_group_replication_connection_status_info
{
  char* channel_name;
  char* group_name;
  bool service_state;
} GROUP_REPLICATION_CONNECTION_STATUS_INFO;

typedef struct st_group_replication_group_members_info
{
  char* channel_name;
  char* member_id;
  char* member_host;
  unsigned int member_port;
  enum enum_member_state member_state;
} GROUP_REPLICATION_GROUP_MEMBERS_INFO;

typedef struct st_group_replication_member_stats_info
{
  char* channel_name;
  char* view_id;
  char* member_id;
  unsigned long long int transaction_in_queue;
  unsigned long long int transaction_certified;
  unsigned long long int transaction_conflicts_detected;
  unsigned long long int transactions_in_validation;
  char* committed_transactions;
  char* last_conflict_free_transaction;
} GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO;

struct st_mysql_group_replication
{
  int interface_version;

  /*
    This function is used to start the group replication.
  */
  int (*start)();
  /*
    This function is used to stop the group replication.
  */
  int (*stop)();
  /*
    This function is used to get the current group replication running status.
  */
  bool (*is_running)();
  /*
   This function initializes conflict checking module with info received
   from group on this member.

   @param info  View_change_log_event with conflict checking info.
  */
  int (*set_retrieved_certification_info)(void* info);

  /*
    This function is used to fetch information for group replication kernel stats.

    @param info[out] The retrieved information

    @note The caller is responsible to free memory from the info structure and
          from all its fields.
  */
  bool (*get_connection_status_info)(GROUP_REPLICATION_CONNECTION_STATUS_INFO *info);

  /*
    This function is used to fetch information for group replication members.

    @param info[out] The retrieved information

    @note The caller is responsible to free memory from the info structure and
          from all its fields.
  */
  bool (*get_group_members_info)(unsigned int index, GROUP_REPLICATION_GROUP_MEMBERS_INFO *info);

  /*
    This function is used to fetch information for group replication members statistics.

    @param info[out] The retrieved information

    @note The caller is responsible to free memory from the info structure and
          from all its fields.
  */
  bool (*get_group_member_stats_info)(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO* info);

  /*
    Get number of group replication members.
  */
  unsigned int (*get_members_number_info)();
};

#endif

