/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/services/get_system_variable/get_system_variable.h"
#include "plugin/group_replication/include/plugin.h"

#include "sql/sql_class.h"

// safeguard due unknown gtid_executed or gtid_purged length
#define GTID_VALUES_FETCH_BUFFER_SIZE 500000

int Get_system_variable_parameters::get_error() { return m_error; }

void Get_system_variable_parameters::set_error(int error) { m_error = error; }

Get_system_variable_parameters::System_variable_service
Get_system_variable_parameters::get_service() {
  return m_service;
}

Get_system_variable::Get_system_variable() {
  get_plugin_registry()->acquire(
      "component_sys_variable_register",
      &component_sys_variable_register_service_handler);
}

Get_system_variable::~Get_system_variable() {
  if (component_sys_variable_register_service_handler != nullptr) {
    get_plugin_registry()->release(
        component_sys_variable_register_service_handler);
  }
}

int Get_system_variable::get_server_gtid_executed(std::string &gtid_executed) {
  int error = 1;

  if (nullptr == mysql_thread_handler) {
    return 1;
  }

  Get_system_variable_parameters *parameter =
      new Get_system_variable_parameters(
          Get_system_variable_parameters::VAR_GTID_EXECUTED);
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  error = mysql_thread_handler->trigger(task);
  error |= parameter->get_error();

  if (!error) {
    gtid_executed.assign(parameter->m_result);
  }

  delete task;
  return error;
}

int Get_system_variable::get_server_gtid_purged(std::string &gtid_purged) {
  int error = 1;

  if (nullptr == mysql_thread_handler) {
    return 1;
  }

  Get_system_variable_parameters *parameter =
      new Get_system_variable_parameters(
          Get_system_variable_parameters::VAR_GTID_PURGED);
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  error = mysql_thread_handler->trigger(task);
  error |= parameter->get_error();

  if (!error) {
    gtid_purged.assign(parameter->m_result);
  }

  delete task;
  return error;
}

void Get_system_variable::run(Mysql_thread_body_parameters *parameters) {
  Get_system_variable_parameters *param =
      (Get_system_variable_parameters *)parameters;
  switch (param->get_service()) {
    case Get_system_variable_parameters::VAR_GTID_EXECUTED:
      param->set_error(internal_get_system_variable(
          std::string("gtid_executed"), param->m_result));
      break;
    case Get_system_variable_parameters::VAR_GTID_PURGED:
      param->set_error(internal_get_system_variable(std::string("gtid_purged"),
                                                    param->m_result));
      break;
    default:
      param->set_error(1);
  }
}

int Get_system_variable::internal_get_system_variable(std::string variable,
                                                      std::string &value) {
  SERVICE_TYPE(component_sys_variable_register)
  *component_sys_variable_register_service{nullptr};
  char *var_value = nullptr;
  size_t var_len = GTID_VALUES_FETCH_BUFFER_SIZE;
  bool error = false;

  if (nullptr == component_sys_variable_register_service_handler) {
    error = true; /* purecov: inspected */
    goto end;     /* purecov: inspected */
  }

  component_sys_variable_register_service =
      reinterpret_cast<SERVICE_TYPE(component_sys_variable_register) *>(
          component_sys_variable_register_service_handler);

  if ((var_value = new (std::nothrow) char[var_len + 1]) == nullptr) {
    error = true; /* purecov: inspected */
    goto end;     /* purecov: inspected */
  }

  if (component_sys_variable_register_service->get_variable(
          "mysql_server", variable.c_str(),
          reinterpret_cast<void **>(&var_value), &var_len)) {
    error = true; /* purecov: inspected */
    goto end;     /* purecov: inspected */
  }

  value.assign(var_value, var_len);

end:
  delete[] var_value;
  return error;
}
