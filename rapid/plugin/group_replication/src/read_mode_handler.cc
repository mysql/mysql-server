/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "read_mode_handler.h"
#include "plugin.h"

long enable_super_read_only_mode(Sql_service_command_interface *command_interface)
{
  DBUG_ENTER("set_super_read_only_mode");
  long error =0;

#ifndef NDEBUG
  DBUG_EXECUTE_IF("group_replication_skip_read_mode", { DBUG_RETURN(0); });
  DBUG_EXECUTE_IF("group_replication_read_mode_error", { DBUG_RETURN(1); });
#endif

  assert(command_interface != NULL);

  // Extract server values for the super read mode
  longlong server_super_read_only_query=
      command_interface->get_server_super_read_only();

  error= server_super_read_only_query == -1;

  // Setting the super_read_only mode on the server.
  if (!error)
  {
    if(!server_super_read_only_query)
      error= command_interface->set_super_read_only();
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "Can't read the server value for the super_read_only"
                " variable."); /* purecov: inspected */
  }

  DBUG_RETURN(error);
}

long disable_super_read_only_mode(Sql_service_command_interface *command_interface)
{
  DBUG_ENTER("reset_super_read_mode");
  long error =0;

  assert(command_interface != NULL);

  error = command_interface->reset_read_only();

  DBUG_RETURN(error);
}

int enable_server_read_mode(enum_plugin_con_isolation session_isolation)
{
  Sql_service_command_interface *sql_command_interface=
      new Sql_service_command_interface();
  int error=
    sql_command_interface->
      establish_session_connection(session_isolation, get_plugin_pointer()) ||
    sql_command_interface->set_interface_user(GROUPREPL_USER) ||
    enable_super_read_only_mode(sql_command_interface);
  delete sql_command_interface;
  return error;
}

int disable_server_read_mode(enum_plugin_con_isolation session_isolation)
{
  Sql_service_command_interface *sql_command_interface=
      new Sql_service_command_interface();
  int error=
    sql_command_interface->
      establish_session_connection(session_isolation, get_plugin_pointer()) ||
    sql_command_interface->set_interface_user(GROUPREPL_USER) ||
    disable_super_read_only_mode(sql_command_interface);
  delete sql_command_interface;
  return error;
}

long get_read_mode_state(Sql_service_command_interface *sql_command_interface,
                         bool *read_only_enabled, bool *super_read_only_enabled)
{
  DBUG_ENTER("get_read_mode_state");

  long error =0;

  assert(sql_command_interface != NULL);

  // Extract server values for the read mode
  longlong server_read_only_query=
      sql_command_interface->get_server_read_only();
  longlong server_super_read_only_query=
      sql_command_interface->get_server_super_read_only();

  error= server_read_only_query == -1 || server_super_read_only_query == -1;

  if (!error)
  {
    *read_only_enabled= (bool) server_read_only_query;
    *super_read_only_enabled= (bool) server_super_read_only_query;
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "Can't read the server values for the read_only and "
                "super_read_only variables."); /* purecov: inspected */
  }

  DBUG_RETURN(error);
}

long set_read_mode_state(Sql_service_command_interface *sql_service_command,
                         bool read_only_enabled, bool super_read_only_enabled)
{
  DBUG_ENTER("set_read_mode_state");

  long error= 0;

  if (!read_only_enabled)
    error|= sql_service_command->reset_read_only();
  else if (!super_read_only_enabled)
    error|= sql_service_command->reset_super_read_only();

  if (error)
  {
    //Do not throw an error as the user can reset the read mode
    log_message(MY_ERROR_LEVEL,
                "It was not possible to reset the server read mode settings."
                " Try to reset them manually."); /* purecov: inspected */
  }

  DBUG_RETURN(error);
}
