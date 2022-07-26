/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

#ifndef RPL_GROUP_REPLICATION_INCLUDED
#define RPL_GROUP_REPLICATION_INCLUDED

#include <violite.h>
#include <string>

class THD;
class View_change_log_event;
struct GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS;
struct GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS;
struct GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS;

/*
  Group Replication plugin handler function accessors.
*/
bool is_group_replication_plugin_loaded();

int group_replication_start(char **error_message, THD *thd);
int group_replication_stop(char **error_message);
bool is_group_replication_running();
bool is_group_replication_cloning();
int set_group_replication_retrieved_certification_info(
    View_change_log_event *view_change_event);

bool get_group_replication_connection_status_info(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS &callbacks);
bool get_group_replication_group_members_info(
    unsigned int index,
    const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS &callbacks);
bool get_group_replication_group_member_stats_info(
    unsigned int index,
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS &callbacks);
unsigned int get_group_replication_members_number_info();
/**
  Getter to extract the group_name in GR which, this can be used
  outside GR to find out the group name.
*/
std::string get_group_replication_group_name();

/**
  Getter to extract the value of variable group_replication_view_change_uuid in
  Group Replication.

  If group_replication_view_change_uuid variable isn't defined or service
  retrieves error when getting variable it will return default value
  "AUTOMATIC".

  @param[out] uuid  Retrieves value of variable group_replication_view_change

    @return the operation status
      @retval false      OK
      @retval true    Error
*/
bool get_group_replication_view_change_uuid(std::string &uuid);

/**
  Checks if this member is part of a group in single-primary mode and if
  this member is a secondary.

  @return status
    @retval true  this member is part of a group in single-primary mode
                  and is a secondary
    @retval false otherwise
*/
bool is_group_replication_member_secondary();

// Callback definition for socket donation
typedef void (*gr_incoming_connection_cb)(THD *thd, int fd, SSL *ssl_ctx);
void set_gr_incoming_connection(gr_incoming_connection_cb x);

#endif /* RPL_GROUP_REPLICATION_INCLUDED */
