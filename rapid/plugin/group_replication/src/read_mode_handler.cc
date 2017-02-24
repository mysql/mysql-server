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

#include "read_mode_handler.h"
#include "plugin_utils.h"

Read_mode_handler::Read_mode_handler()
  : read_mode_active(false),
    server_read_only(0), server_super_read_only(0)
{
#ifndef DBUG_OFF
  is_set_to_fail= false;
#endif
  mysql_mutex_init(key_GR_LOCK_read_mode, &read_mode_lock, MY_MUTEX_INIT_FAST);
}

Read_mode_handler::~Read_mode_handler()
{
  mysql_mutex_destroy(&read_mode_lock);
}

long Read_mode_handler::
set_super_read_only_mode(Sql_service_command *command_interface)
{
  DBUG_ENTER("set_super_read_only_mode");
  long error =0;

  Mutex_autolock auto_lock_mutex(&read_mode_lock);

  if (read_mode_active)
  {
    DBUG_RETURN(0);
  }

#ifndef DBUG_OFF
  DBUG_EXECUTE_IF("group_replication_skip_read_mode", { DBUG_RETURN(0); });
  if (is_set_to_fail)
  {
    is_set_to_fail= false;
    DBUG_RETURN(1);
  }
#endif

  DBUG_ASSERT(command_interface != NULL);

  // Extract server values for the read mode
  longlong server_read_only_query=
      command_interface->get_server_read_only();
  longlong server_super_read_only_query=
      command_interface->get_server_super_read_only();

  error= server_read_only_query == -1 || server_super_read_only_query == -1;

  // Setting the super_read_only mode on the server.
  if (!error)
  {
    server_read_only= server_read_only_query;
    server_super_read_only= server_super_read_only_query;

    if(!server_super_read_only)
      error= command_interface->set_super_read_only();
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "Can't read the server values for the read_only and "
                "super_read_only variables."); /* purecov: inspected */
  }

  if (!error)
    read_mode_active= true;

  DBUG_RETURN(error);
}

long Read_mode_handler::
reset_super_read_only_mode(Sql_service_command *command_interface, bool force_reset)
{
  DBUG_ENTER("reset_super_read_mode");
  long error =0;

  Mutex_autolock auto_lock_mutex(&read_mode_lock);

  DBUG_ASSERT(command_interface != NULL);

  if (force_reset)
  {
    read_mode_active= false;
    error = command_interface->reset_read_only();

    DBUG_RETURN(error);
  }

  if (!read_mode_active)
    DBUG_RETURN(error);

  /*
    If the server had no read mode active we set the read_only to 0,
    this resets both read_only and super_read_only.
    If the server had the read mode active we reset the super_read_only to 0.
    If super_read_only was active, nothing happens.
  */
  if (server_read_only == 0 && server_super_read_only == 0)
    error = command_interface->reset_read_only();
  else if (server_read_only == 1 && server_super_read_only == 0)
    error = command_interface->reset_super_read_only();

  read_mode_active= false;
  server_read_only= 0;
  server_super_read_only= 0;

  DBUG_RETURN(error);
}
