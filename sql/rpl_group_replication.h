/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_GROUP_REPLICATION_INCLUDED
#define RPL_GROUP_REPLICATION_INCLUDED

#include <string>
#include "sql_plugin.h"
#include <mysql/plugin.h>
#include <mysql/plugin_group_replication.h>
#include "replication.h"
#include "log_event.h"

#include "mysql/plugin_group_replication.h"

class View_change_log_event;


/*
  Group Replication plugin handler function accessors.
*/
bool is_group_replication_plugin_loaded();

int group_replication_start();
int group_replication_stop();
bool is_group_replication_running();
int set_group_replication_retrieved_certification_info(View_change_log_event *view_change_event);

bool get_group_replication_connection_status_info(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS& callbacks);
bool get_group_replication_group_members_info(
    unsigned int index,
    const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS& callbacks);
bool get_group_replication_group_member_stats_info(
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS& callbacks);
unsigned int get_group_replication_members_number_info();


#endif /* RPL_GROUP_REPLICATION_INCLUDED */
