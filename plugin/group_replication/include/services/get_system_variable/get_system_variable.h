/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#ifndef GR_GET_SYSTEM_VARIABLE
#define GR_GET_SYSTEM_VARIABLE

#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/registry.h>
#include <string>

class THD;

class Get_system_variable {
 public:
  Get_system_variable(bool create_session = false);

  ~Get_system_variable();

  /**
    Method to return the server gtid_executed by executing the get_variables
    component service.

    @param [out] gtid_executed The string where the result will be appended

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int get_server_gtid_executed(std::string &gtid_executed);

  /**
    Method to return the server gtid_purged by executing the get_variables
    component service.

    @param [out] gtid_purged The string where the result will be appended

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int get_server_gtid_purged(std::string &gtid_purged);

 private:
  THD *m_thd{nullptr};
  THD *m_previous_current_thd{nullptr};

  my_h_service component_sys_variable_register_service_handler{nullptr};
};

#endif  // GR_GET_SYSTEM_VARIABLE
