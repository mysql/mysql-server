/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_plugin_utils.h"

enum enum_node_state
map_protocol_node_state_to_server_node_state(GCS::Member_recovery_status protocol_status)
{
  switch(protocol_status)
  {
    case 1:  // MEMBER_ONLINE
      return NODE_STATE_ONLINE;
    case 2:  // MEMBER_OFFLINE
      return NODE_STATE_OFFLINE;
    case 3:  // MEMBER_IN_RECOVERY
      return NODE_STATE_RECOVERING;
    default:
      DBUG_ASSERT(0);
      return NODE_STATE_OFFLINE;
  }
}

