/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef GR_SET_SYSTEM_VARIABLE
#define GR_SET_SYSTEM_VARIABLE

#include <string>
#include "plugin/group_replication/include/thread/mysql_thread.h"

class Set_system_variable_parameters : public Mysql_thread_body_parameters {
 public:
  enum System_variable {
    VAR_READ_ONLY,
    VAR_SUPER_READ_ONLY,
    VAR_OFFLINE_MODE,
    VAR_GROUP_REPLICATION_SINGLE_PRIMARY_MODE,
    VAR_GROUP_REPLICATION_ENFORCE_UPDATE_EVERYWHERE_CHECKS
  };

  /**
    Set_system_variable_parameters constructor.

    @param [in]  variable The system variable to be set
    @param [in]  value    The value to be set
    @param [in]  type     GLOBAL or PERSIST_ONLY
    */
  Set_system_variable_parameters(System_variable variable,
                                 const std::string &value,
                                 const std::string &type)
      : m_value(value), m_type(type), m_variable(variable), m_error(1){};
  virtual ~Set_system_variable_parameters(){};

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
    Get value for class private member variable.

    @return System defined to run
    */
  Set_system_variable_parameters::System_variable get_variable();

  // to avoid multiple copies on get and set methods we define it as public
  const std::string m_value{""};
  const std::string m_type{""};

 private:
  System_variable m_variable{System_variable::VAR_READ_ONLY};
  int m_error{1};
};

class Set_system_variable : Mysql_thread_body {
 public:
  Set_system_variable() = default;

  virtual ~Set_system_variable() = default;

  /**
    Method to set the global value of read_only.

    @param [in] value The value to be set

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int set_global_read_only(bool value);

  /**
    Method to set the global value of super_read_only.

    @param [in] value The value to be set

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int set_global_super_read_only(bool value);

  /**
    Method to set the global value of offline_mode.

    @param [in] value The value to be set

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int set_global_offline_mode(bool value);

  /**
    Method to only persist the value of
    group_replication_single_primary_mode.

    @param [in] value The value to be persisted

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int set_persist_only_group_replication_single_primary_mode(bool value);

  /**
    Method to only persist the value of
    group_replication_enforce_update_everywhere_checks

    @param [in] value The value to be persisted

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int set_persist_only_group_replication_enforce_update_everywhere_checks(
      bool value);

  /**
    Method that will be run on mysql_thread.

    @param [in, out] parameters Values used by method to get service variable.

    */
  void run(Mysql_thread_body_parameters *parameters);

 private:
  /**
    Method to set the server system variable specified on variable.

    @param [in]  variable The system variable name to be set
    @param [in]  value    The value to be set
    @param [in]  type     GLOBAL or PERSIST_ONLY
    @param [in]  lock_wait_timeout Lock wait timeout in seconds

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int internal_set_system_variable(const std::string &variable,
                                   const std::string &value,
                                   const std::string &type,
                                   unsigned long long lock_wait_timeout);
};

#endif  // GR_SET_SYSTEM_VARIABLE
