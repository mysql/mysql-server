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

#ifndef GCS_PS_INFORMATION_INCLUDED
#define GCS_PS_INFORMATION_INCLUDED


#include "plugin_server_include.h"
#include "member_info.h"
#include "gcs_interface.h"
#include "applier.h"

#include <mysql/plugin_group_replication.h>


bool get_gcs_group_members_info(uint index,
                                GROUP_REPLICATION_GROUP_MEMBERS_INFO *info,
                                Cluster_member_info_manager_interface
                                                        *cluster_member_manager,
                                Gcs_interface *gcs_module,
                                char* gcs_group_pointer,
                                char *channel_name);

bool get_gcs_group_member_stats(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO *info,
                                Cluster_member_info_manager_interface
                                                        *cluster_member_manager,
                                Applier_module *applier_module,
                                Gcs_interface *gcs_module,
                                char* gcs_group_pointer,
                                char *channel_name);

bool get_gcs_connection_status(GROUP_REPLICATION_CONNECTION_STATUS_INFO *info,
                               Gcs_interface *gcs_module,
                               char* gcs_group_pointer,
                               char *channel_name,
                               bool is_gcs_running);

#endif	/* GCS_PS_INFORMATION_INCLUDED */

