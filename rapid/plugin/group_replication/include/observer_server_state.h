/* Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef OBSERVER_SERVER_STATE
#define OBSERVER_SERVER_STATE

#include "plugin.h"

/*
  DBMS lifecycle events observers.
*/
int group_replication_before_handle_connection(Server_state_param *param);

int group_replication_before_recovery(Server_state_param *param);

int group_replication_after_engine_recovery(Server_state_param *param);

int group_replication_after_recovery(Server_state_param *param);

int group_replication_before_server_shutdown(Server_state_param *param);

int group_replication_after_server_shutdown(Server_state_param *param);

extern Server_state_observer server_state_observer;

#endif /* OBSERVER_SERVER_STATE */
