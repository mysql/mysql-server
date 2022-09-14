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

#include "plugin/group_replication/include/services/system_variable/get_system_variable.h"
#include "plugin/group_replication/include/plugin.h"

// safeguard due unknown gtid_executed or gtid_purged length
constexpr size_t GTID_VALUES_FETCH_BUFFER_SIZE{500000};
constexpr size_t BOOL_VALUES_FETCH_BUFFER_SIZE{4};

int Get_system_variable_parameters::get_error() { return m_error; }

void Get_system_variable_parameters::set_error(int error) { m_error = error; }

Get_system_variable_parameters::System_variable_service
Get_system_variable_parameters::get_service() {
  return m_service;
}

int Get_system_variable::get_global_gtid_executed(std::string &gtid_executed) {
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

int Get_system_variable::get_global_gtid_purged(std::string &gtid_purged) {
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

int Get_system_variable::get_global_read_only(bool &value) {
  int error = 1;

  if (nullptr == mysql_thread_handler_read_only_mode) {
    return 1;
  }

  Get_system_variable_parameters *parameter =
      new Get_system_variable_parameters(
          Get_system_variable_parameters::VAR_READ_ONLY);
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  error = mysql_thread_handler_read_only_mode->trigger(task);
  error |= parameter->get_error();

  if (!error) {
    value = string_to_bool(parameter->m_result);
  }

  delete task;
  return error;
}

int Get_system_variable::get_global_super_read_only(bool &value) {
  int error = 1;

  if (nullptr == mysql_thread_handler_read_only_mode) {
    return 1;
  }

  Get_system_variable_parameters *parameter =
      new Get_system_variable_parameters(
          Get_system_variable_parameters::VAR_SUPER_READ_ONLY);
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  error = mysql_thread_handler_read_only_mode->trigger(task);
  error |= parameter->get_error();

  if (!error) {
    value = string_to_bool(parameter->m_result);
  }

  delete task;
  return error;
}

bool Get_system_variable::string_to_bool(const std::string &value) {
  if (value == "ON") {
    return true;
  }
  assert(value == "OFF");
  return false;
}

void Get_system_variable::run(Mysql_thread_body_parameters *parameters) {
  Get_system_variable_parameters *param =
      (Get_system_variable_parameters *)parameters;
  switch (param->get_service()) {
    case Get_system_variable_parameters::VAR_GTID_EXECUTED:
      param->set_error(internal_get_system_variable(
          std::string("gtid_executed"), param->m_result,
          GTID_VALUES_FETCH_BUFFER_SIZE));
      break;
    case Get_system_variable_parameters::VAR_GTID_PURGED:
      param->set_error(internal_get_system_variable(
          std::string("gtid_purged"), param->m_result,
          GTID_VALUES_FETCH_BUFFER_SIZE));
      break;
    case Get_system_variable_parameters::VAR_READ_ONLY:
      param->set_error(internal_get_system_variable(
          std::string("read_only"), param->m_result,
          BOOL_VALUES_FETCH_BUFFER_SIZE));
      break;
    case Get_system_variable_parameters::VAR_SUPER_READ_ONLY:
      param->set_error(internal_get_system_variable(
          std::string("super_read_only"), param->m_result,
          BOOL_VALUES_FETCH_BUFFER_SIZE));
      break;
    default:
      param->set_error(1);
  }
}

int Get_system_variable::internal_get_system_variable(std::string variable,
                                                      std::string &value,
                                                      size_t value_max_length) {
  char *var_value = nullptr;
  size_t var_len = value_max_length;
  int error = false;

  if (nullptr == server_services_references_module
                     ->component_sys_variable_register_service) {
    error = 1; /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

  if ((var_value = new (std::nothrow) char[var_len + 1]) == nullptr) {
    error = 1; /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

  if (server_services_references_module->component_sys_variable_register_service
          ->get_variable("mysql_server", variable.c_str(),
                         reinterpret_cast<void **>(&var_value), &var_len)) {
    error = 1; /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

  value.assign(var_value, var_len);

end:
  delete[] var_value;
  return error;
}
