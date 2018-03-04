/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef READ_MODE_HANDLER_INCLUDE
#define READ_MODE_HANDLER_INCLUDE

#include "my_inttypes.h"
#include "plugin/group_replication/include/sql_service/sql_service_command.h"


/**
  This method creates a server session and connects to the server
  to enable the read mode

  @param session_isolation session creation requirements: use current thread,
                           use thread but initialize it or create it in a
                           dedicated thread

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
int enable_server_read_mode(enum_plugin_con_isolation session_isolation);

/**
  This method creates a server session and connects to the server
  to disable the read mode

   @param session_isolation session creation requirements: use current thread,
                           use thread but initialize it or create it in a
                           dedicated thread

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
int disable_server_read_mode(enum_plugin_con_isolation session_isolation);

/**
  Enable the super read only mode in the server.

  @param sql_service_command  Command interface given to execute the command

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
long enable_super_read_only_mode(Sql_service_command_interface *sql_service_command);

/**
  Disable the read only mode in the server.

  @param sql_service_command  Command interface given to execute the command

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
long disable_super_read_only_mode(Sql_service_command_interface *sql_service_command);

/**
  Get read mode status from server.

  @param sql_service_command        Command interface given to execute the command
  @param read_only_enabled          Update with value of read only mode
  @param super_read_only_enabled    Update with value of super read only mode

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
long get_read_mode_state(Sql_service_command_interface *sql_service_command,
                         bool *read_only_enabled, bool *super_read_only_enabled);

/**
  Set read mode status from server.

  @param sql_service_command        Command interface given to execute the command
  @param read_only_enabled          Value to set on read only mode
  @param super_read_only_enabled    Value to set on super read only mode

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
long set_read_mode_state(Sql_service_command_interface *sql_service_command,
                         bool read_only_enabled, bool super_read_only_enabled);

#endif /* READ_MODE_HANDLER_INCLUDE */
