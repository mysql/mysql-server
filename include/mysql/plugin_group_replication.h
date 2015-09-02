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
#define MYSQL_GROUP_REPLICATION_INTERFACE_VERSION 0x0101

/*
  Callbacks for get_connection_status_info function.

  context field can have NULL value, plugin will always pass it
  through all callbacks, independent of its value.
  Its value will not be used by plugin.

  All callbacks are mandatory.
*/
typedef struct st_group_replication_connection_status_callbacks
{
  void* const context;
  void (*set_channel_name)(void* const context, const char& value, size_t length);
  void (*set_group_name)(void* const context, const char& value, size_t length);
  void (*set_source_uuid)(void* const context, const char& value, size_t length);
  void (*set_service_state)(void* const context, bool state);
} GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS;

/*
  Callbacks for get_group_members_info function.

  context field can have NULL value, plugin will always pass it
  through all callbacks, independent of its value.
  Its value will not be used by plugin.

  All callbacks are mandatory.
*/
typedef struct st_group_replication_group_members_callbacks
{
  void* const context;
  void (*set_channel_name)(void* const context, const char& value, size_t length);
  void (*set_member_id)(void* const context, const char& value, size_t length);
  void (*set_member_host)(void* const context, const char& value, size_t length);
  void (*set_member_port)(void* const context, unsigned int value);
  void (*set_member_state)(void* const context, const char& value, size_t length);
} GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS;

/*
  Callbacks for get_group_member_stats_info function.

  context field can have NULL value, plugin will always pass it
  through all callbacks, independent of its value.
  Its value will not be used by plugin.

  All callbacks are mandatory.
*/
typedef struct st_group_replication_member_stats_callbacks
{
  void* const context;
  void (*set_channel_name)(void* const context, const char& value, size_t length);
  void (*set_view_id)(void* const context, const char& value, size_t length);
  void (*set_member_id)(void* const context, const char& value, size_t length);
  void (*set_transactions_committed)(void* const context, const char& value, size_t length);
  void (*set_last_conflict_free_transaction)(void* const context, const char& value, size_t length);
  void (*set_transactions_in_queue)(void* const context, unsigned long long int value);
  void (*set_transactions_certified)(void* const context, unsigned long long int value);
  void (*set_transactions_conflicts_detected)(void* const context, unsigned long long int value);
  void (*set_transactions_in_validation)(void* const context, unsigned long long int value);
} GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS;

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

    @param callbacks The set of callbacks and its context used to set the
                     information on caller.

    @note The caller is responsible to free memory from the info structure and
          from all its fields.
  */
  bool (*get_connection_status_info)
       (const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS& callbacks);

  /*
    This function is used to fetch information for group replication members.

    @param callbacks The set of callbacks and its context used to set the
                     information on caller.

    @note The caller is responsible to free memory from the info structure and
          from all its fields.
  */
  bool (*get_group_members_info)
       (unsigned int index,
        const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS& callbacks);

  /*
    This function is used to fetch information for group replication members statistics.

    @param callbacks The set of callbacks and its context used to set the
                     information on caller.

    @note The caller is responsible to free memory from the info structure and
          from all its fields.
  */
  bool (*get_group_member_stats_info)
       (const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS& callbacks);

  /*
    Get number of group replication members.
  */
  unsigned int (*get_members_number_info)();
};

#endif

