/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <string>
#include "plugin/group_replication/include/thread/mysql_thread.h"

class Get_system_variable_parameters : public Mysql_thread_body_parameters {
 public:
  enum System_variable_service {
    VAR_GTID_EXECUTED,
    VAR_GTID_PURGED,
    VAR_READ_ONLY,
    VAR_SUPER_READ_ONLY
  };

  Get_system_variable_parameters(System_variable_service service)
      : m_result(""), m_service(service), m_error(1){};
  virtual ~Get_system_variable_parameters(){};

  /**
    Get value for class private member error.

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int get_error();

  /**
    Set value for class private member error.

    @param [in] error Set value of error
    */
  void set_error(int error);

  /**
    Set value for class private member service.

    @return System defined to run
    */
  Get_system_variable_parameters::System_variable_service get_service();

  // to avoid multiple copies on get and set methods we define it as public
  std::string m_result{""};

 private:
  System_variable_service m_service{System_variable_service::VAR_GTID_EXECUTED};
  // error returned by service executed
  int m_error{1};
};

class Get_system_variable : Mysql_thread_body {
 public:
  Get_system_variable() = default;

  virtual ~Get_system_variable() = default;

  /**
    Method to return the server gtid_executed by executing the get_variables
    component service.

    @param [out] gtid_executed The string where the result will be appended

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int get_global_gtid_executed(std::string &gtid_executed);

  /**
    Method to return the server gtid_purged by executing the get_variables
    component service.

    @param [out] gtid_purged The string where the result will be appended

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int get_global_gtid_purged(std::string &gtid_purged);

  /**
    Method to return the global value of read_only by executing
    the get_variables component service.

    @param [out] value The variable where the value will be set

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int get_global_read_only(bool &value);

  /**
    Method to return the global value of super_read_only by executing
    the get_variables component service.

    @param [out] value The variable where the value will be set

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int get_global_super_read_only(bool &value);

  /**
    Method that will be run on mysql_thread.

    @param [in, out] parameters Values used by method to get service variable.

    */
  void run(Mysql_thread_body_parameters *parameters);

 private:
  /**
    Method to convert a string into a boolean.

    @param [in] value The value as string.

    @return the value as boolean
    @retval true   string value is "ON"
    @retval false  otherwise
    */
  bool string_to_bool(const std::string &value);

  /**
    Method to return the server system variable specified on variable.

    @param [in]  variable The system variable name to be retrieved
    @param [out] value    The string where the result will be set
    @param [in]  value_max_length The maximum string value length

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int internal_get_system_variable(std::string variable, std::string &value,
                                   size_t value_max_length);
};

#endif  // GR_GET_SYSTEM_VARIABLE
