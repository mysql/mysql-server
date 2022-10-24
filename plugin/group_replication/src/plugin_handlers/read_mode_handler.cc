/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/read_mode_handler.h"

#include "my_dbug.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_utils.h"
#include "plugin/group_replication/include/services/system_variable/get_system_variable.h"
#include "plugin/group_replication/include/services/system_variable/set_system_variable.h"

int enable_server_read_mode() {
  DBUG_TRACE;
  int error = 0;

  // Extract server values for super read mode
  bool super_read_only_value = false;
  Get_system_variable get_system_variable;
  get_system_variable.get_global_super_read_only(super_read_only_value);

  // Setting the super_read_only mode on the server.
  // We do log the log message even when the super_read_only is already
  // enabled to avoid the possible misinterpretations if it was omitted.
  LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SUPER_READ_ON);
  if (!super_read_only_value) {
    Set_system_variable set_system_variable;
    error = set_system_variable.set_global_super_read_only(true);
  }

  return error;
}

int disable_server_read_mode() {
  DBUG_TRACE;

  LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SUPER_READ_OFF);
  Set_system_variable set_system_variable;
  int error = set_system_variable.set_global_read_only(false);

  return error;
}

int get_read_mode_state(bool *read_only_enabled,
                        bool *super_read_only_enabled) {
  DBUG_TRACE;
  int error = 0;
  bool read_only_value = false;
  bool super_read_only_value = false;
  Get_system_variable get_system_variable;

  error |= get_system_variable.get_global_read_only(read_only_value);
  error |=
      get_system_variable.get_global_super_read_only(super_read_only_value);

  if (!error) {
    *read_only_enabled = read_only_value;
    *super_read_only_enabled = super_read_only_value;
  } else {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_READ_UNABLE_FOR_READ_ONLY_SUPER_READ_ONLY); /* purecov:
                                                                  inspected */
  }

  return error;
}

int set_read_mode_state(bool read_only_enabled, bool super_read_only_enabled) {
  DBUG_TRACE;
  int error = 0;
  Set_system_variable set_system_variable;

  if (!read_only_enabled) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SUPER_READ_OFF);
    error |= set_system_variable.set_global_read_only(false);
  } else if (!super_read_only_enabled) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SUPER_READ_OFF);
    error |= set_system_variable.set_global_super_read_only(false);
  }

  if (error) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_UNABLE_TO_RESET_SERVER_READ_MODE); /* purecov: inspected */
  }

  return error;
}
