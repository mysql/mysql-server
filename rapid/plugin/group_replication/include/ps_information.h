/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PS_INFORMATION_INCLUDED
#define PS_INFORMATION_INCLUDED


#include "plugin_server_include.h"
#include "member_info.h"
#include "applier.h"
#include "gcs_operations.h"

#include <mysql/plugin_group_replication.h>


bool get_group_members_info(uint index,
                            const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS& callbacks,
                            Group_member_info_manager_interface
                                *group_member_manager,
                            char *group_name_pointer,
                            char *channel_name);

bool get_group_member_stats(const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS& callbacks,
                            Group_member_info_manager_interface
                                *group_member_manager,
                            Applier_module *applier_module,
                            Gcs_operations *gcs_module,
                            char *group_name_pointer,
                            char *channel_name);

bool get_connection_status(const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS& callbacks,
                           char *group_name_pointer,
                           char *channel_name,
                           bool is_group_replication_running);

#endif	/* PS_INFORMATION_INCLUDED */

