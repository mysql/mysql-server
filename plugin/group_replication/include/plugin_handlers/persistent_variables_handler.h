/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PERSISTENT_VARIABLES_HANDLER_INCLUDED
#define PERSISTENT_VARIABLES_HANDLER_INCLUDED

#include "plugin/group_replication/include/sql_service/sql_service_command.h"

/**
  Persist a variable in the configuration but do not set the value
  @param name the name of the query
  @param value the value to set in the variable
  @param command_interface the interface to the session API

  @note use this method when there is already an open server connection

  @returns 0 in case of success, or the error value from the query
*/
long set_persist_only_variable(
    std::string &name, std::string &value,
    Sql_service_command_interface *command_interface);

#endif /* PERSISTENT_VARIABLES_HANDLER_INCLUDED */
