/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>

#include "plugin/group_replication/include/delayed_plugin_initialization.h"
#include "plugin/group_replication/include/observer_server_state.h"

using std::string;

/*
  DBMS lifecycle events observers.
*/
int group_replication_before_handle_connection(Server_state_param *) {
  if (wait_on_engine_initialization) {
    delayed_initialization_thread->signal_thread_ready();
    delayed_initialization_thread->wait_for_read_mode();
  }
  return 0;
}

int group_replication_before_recovery(Server_state_param *) { return 0; }

int group_replication_after_engine_recovery(Server_state_param *) { return 0; }

int group_replication_after_recovery(Server_state_param *) { return 0; }

int group_replication_before_server_shutdown(Server_state_param *) { return 0; }

int group_replication_after_server_shutdown(Server_state_param *) {
  server_shutdown_status = true;
  plugin_group_replication_stop();

  return 0;
}

Server_state_observer server_state_observer = {
    sizeof(Server_state_observer),

    group_replication_before_handle_connection,  // before the client connects
                                                 // to the server
    group_replication_before_recovery,           // before recovery
    group_replication_after_engine_recovery,     // after engine recovery
    group_replication_after_recovery,            // after recovery
    group_replication_before_server_shutdown,    // before shutdown
    group_replication_after_server_shutdown,     // after shutdown
};
