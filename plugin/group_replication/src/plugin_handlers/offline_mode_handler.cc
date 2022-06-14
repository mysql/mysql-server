/* Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/offline_mode_handler.h"

#include <stddef.h>

#include "my_dbug.h"
#include "mysql/components/services/log_builtins.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_utils.h"

void enable_server_offline_mode(enum_plugin_con_isolation session_isolation) {
  DBUG_TRACE;

  Sql_service_command_interface *sql_command_interface =
      new Sql_service_command_interface();
  int error = sql_command_interface->establish_session_connection(
                  session_isolation, GROUPREPL_USER, get_plugin_pointer()) ||
              sql_command_interface->set_offline_mode();
  delete sql_command_interface;

  if (error) {
    /* purecov: begin inspected */
    abort_plugin_process(
        "cannot enable offline mode after an error was detected.");
    /* purecov: end */
  } else {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_SERVER_SET_TO_OFFLINE_MODE_DUE_TO_ERRORS);
  }
}
