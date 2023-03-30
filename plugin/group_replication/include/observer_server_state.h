/* Copyright (c) 2013, 2023, Oracle and/or its affiliates.

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

#ifndef OBSERVER_SERVER_STATE
#define OBSERVER_SERVER_STATE

#include "plugin/group_replication/include/plugin.h"

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
