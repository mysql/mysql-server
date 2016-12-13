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

#ifndef SQL_SERVICE_COMMAND_INCLUDE
#define SQL_SERVICE_COMMAND_INCLUDE

#include "sql_service_interface.h"

class Sql_service_command
{
public:
  Sql_service_command();
  ~Sql_service_command();

  /*
    Establishes the connection to the server.

    @param threaded        if the connection should have an attached thread
    @param plugin_pointer  the plugin pointer for threaded connections

    @return the connection was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int establish_session_connection(bool threaded, void *plugin_pointer=NULL);

  /*
    Returns the SQL service interface associated to this class

    @return the sql service interface field
  */
  Sql_service_interface* get_sql_service_interface();

  /*
    Sets the SQL API user to be used on security checks

    @param user the user to be used

    @return the operation was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int set_interface_user(const char* user);

  /*
    Method to set the super_read_only variable "ON" on the server at two
    possible places :

    1. During server compatibility check in the versioning module.
    2. During server recovery handling in the recovery module.

    @retval error code during execution of the sql query.
       0  -  success
       >0 - failure
  */
  long set_super_read_only();

  /*
    Method to reset the super_read_only mode back to "OFF" on the
    server.

    @retval error code during execution of the sql query.
       0  -  success
       >0 - failure
  */
  long reset_super_read_only();

  /*
    Method to reset the read_only mode back to "OFF" on the
    server.

    @retval error code during execution of the sql query.
       0  -  success
       >0 - failure
  */
  long reset_read_only();

  /*
    Method to kill the session identified by the given sesion id in those
    cases where the server hangs while executing the sql query.

    @param session_id  id of the session to be killed.
    @param session  the session to be killed

    @retval the error value returned
       0 - success
       >0 - Failure
  */
  long kill_session(uint32_t session_id, MYSQL_SESSION session);

  /*
    Method to return the server gtid_executed by executing the corresponding
    sql query.

    @param gtid_executed[out]  The string where the result will be appended

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error
  */
  int get_server_gtid_executed(std::string& gtid_executed);

  /*
    Method to wait for the server gtid_executed to match the given GTID string

    @param gtid_executed[in]  The GTID string to check
    @param timeout[in]        The timeout after which the method should break

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error when executed or timeout.
  */
  long wait_for_server_gtid_executed(std::string& gtid_executed, int timeout= 0);

  /*
    Method to get the value of the super_read_only variable on the server.

    @retval
      -1  Error reading the value
       0  Not in super read mode
       1  In read super mode
  */
  long get_server_super_read_only();

  /*
    Method to get the value of the read_only variable on the server.

    @retval
      -1  Error reading the value
       0  Not in super read mode
       1  In read super mode
  */
  long get_server_read_only();
private:
  //The internal SQL session service interface to the server
  Sql_service_interface *server_interface;
};

#endif //SQL_SERVICE_COMMAND_INCLUDE
