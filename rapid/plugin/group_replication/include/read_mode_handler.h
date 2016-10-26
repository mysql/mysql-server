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

#ifndef READ_MODE_HANDLER_INCLUDE
#define READ_MODE_HANDLER_INCLUDE

#include "sql_service_command.h"

class Read_mode_handler
{
public:
  /**
    Create a new handler to set and unset the super read only mode in the server
  */
  Read_mode_handler();

  /** Destructor*/
 ~Read_mode_handler();

  /**
    Set the super read only mode in the server.

    @param sql_service_command  Command interface given to execute the command

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  long set_super_read_only_mode(Sql_service_command *sql_service_command);

  /**
    Reset the read only mode in the server.

    @param sql_service_command  Command interface given to execute the command
    @param force_reset          Always reset super_read_only variable

    @note: if force_reset is false, the value is reset according to the value
    present in the server when set_super_read_only_mode was executed

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  long reset_super_read_only_mode(Sql_service_command *sql_service_command,
                                  bool force_reset= false);

  /**
    Returns true if the assessment and activation of the read mode is already done
  */
  bool is_read_mode_active()
  {
    return read_mode_active;
  }

#ifndef DBUG_OFF
  void set_to_fail()
  {
    is_set_to_fail= true;
  }
#endif

private:
  /** If the mode was set or not */
  bool read_mode_active;
  /** If the server is in (simple) read mode */
  long server_read_only;
  /** If the server is in super read mode*/
  long server_super_read_only;

#ifndef DBUG_OFF
  /** Make the read mode activation fail (when debug flags don't work) */
  bool is_set_to_fail;
#endif
  mysql_mutex_t read_mode_lock;
};

#endif /* READ_MODE_HANDLER_INCLUDE */
