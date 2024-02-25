/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/services/system_variable/set_system_variable.h"
#include "plugin/group_replication/include/plugin.h"

constexpr unsigned long long WAIT_LOCK_TIMEOUT{5};
constexpr unsigned long long READ_ONLY_WAIT_LOCK_TIMEOUT{120};

int Set_system_variable_parameters::get_error() { return m_error; }

void Set_system_variable_parameters::set_error(int error) { m_error = error; }

Set_system_variable_parameters::System_variable
Set_system_variable_parameters::get_variable() {
  return m_variable;
}

int Set_system_variable::set_global_read_only(bool value) {
  int error = 1;

  if (nullptr == mysql_thread_handler_read_only_mode) {
    return 1;
  }

  std::string parameter_value{"ON"};
  if (!value) {
    parameter_value.assign("OFF");
  }

  Set_system_variable_parameters *parameter =
      new Set_system_variable_parameters(
          Set_system_variable_parameters::VAR_READ_ONLY, parameter_value,
          "GLOBAL");
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  /*
    Since enable `read_only` and `super_read_only` are blocking
    operations, we use the dedicated `mysql_thread_handler_read_only_mode`
    to this operation.
    If we did use `mysql_thread_handler` that would block all other
    other operations until read modes operations complete.
  */
  error = mysql_thread_handler_read_only_mode->trigger(task);
  error |= parameter->get_error();

  delete task;
  return error;
}

int Set_system_variable::set_global_super_read_only(bool value) {
  int error = 1;

  if (nullptr == mysql_thread_handler_read_only_mode) {
    return 1;
  }

  std::string parameter_value{"ON"};
  if (!value) {
    parameter_value.assign("OFF");
  }

  Set_system_variable_parameters *parameter =
      new Set_system_variable_parameters(
          Set_system_variable_parameters::VAR_SUPER_READ_ONLY, parameter_value,
          "GLOBAL");
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  /*
    Since enable `read_only` and `super_read_only` are blocking
    operations, we use the dedicated `mysql_thread_handler_read_only_mode`
    to this operation.
    If we did use `mysql_thread_handler` that would block all other
    other operations until read modes operations complete.
  */
  error = mysql_thread_handler_read_only_mode->trigger(task);
  error |= parameter->get_error();

  delete task;
  return error;
}

int Set_system_variable::set_global_offline_mode(bool value) {
  int error = 1;

  if (nullptr == mysql_thread_handler) {
    return 1;
  }

  std::string parameter_value{"ON"};
  if (!value) {
    parameter_value.assign("OFF");
  }

  Set_system_variable_parameters *parameter =
      new Set_system_variable_parameters(
          Set_system_variable_parameters::VAR_OFFLINE_MODE, parameter_value,
          "GLOBAL");
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  error = mysql_thread_handler->trigger(task);
  error |= parameter->get_error();

  delete task;
  return error;
}

int Set_system_variable::set_persist_only_group_replication_single_primary_mode(
    bool value) {
  int error = 1;

  if (nullptr == mysql_thread_handler) {
    return 1;
  }

  std::string parameter_value{"ON"};
  if (!value) {
    parameter_value.assign("OFF");
  }

  Set_system_variable_parameters *parameter =
      new Set_system_variable_parameters(
          Set_system_variable_parameters::
              VAR_GROUP_REPLICATION_SINGLE_PRIMARY_MODE,
          parameter_value, "PERSIST_ONLY");
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  error = mysql_thread_handler->trigger(task);
  error |= parameter->get_error();

  delete task;
  return error;
}

int Set_system_variable::
    set_persist_only_group_replication_enforce_update_everywhere_checks(
        bool value) {
  int error = 1;

  if (nullptr == mysql_thread_handler) {
    return 1;
  }

  std::string parameter_value{"ON"};
  if (!value) {
    parameter_value.assign("OFF");
  }

  Set_system_variable_parameters *parameter =
      new Set_system_variable_parameters(
          Set_system_variable_parameters::
              VAR_GROUP_REPLICATION_ENFORCE_UPDATE_EVERYWHERE_CHECKS,
          parameter_value, "PERSIST_ONLY");
  Mysql_thread_task *task = new Mysql_thread_task(this, parameter);
  error = mysql_thread_handler->trigger(task);
  error |= parameter->get_error();

  delete task;
  return error;
}

void Set_system_variable::run(Mysql_thread_body_parameters *parameters) {
  Set_system_variable_parameters *param =
      (Set_system_variable_parameters *)parameters;
  switch (param->get_variable()) {
    case Set_system_variable_parameters::VAR_READ_ONLY:
      param->set_error(internal_set_system_variable(
          std::string("read_only"), param->m_value, param->m_type,
          READ_ONLY_WAIT_LOCK_TIMEOUT));
      break;
    case Set_system_variable_parameters::VAR_SUPER_READ_ONLY:
#ifndef NDEBUG
      DBUG_EXECUTE_IF("group_replication_skip_read_mode", {
        if (param->m_value == "ON") {
          param->set_error(0);
          return;
        }
      });
      DBUG_EXECUTE_IF("group_replication_read_mode_error", {
        if (param->m_value == "ON") {
          param->set_error(1);
          return;
        }
      });
#endif
      param->set_error(internal_set_system_variable(
          std::string("super_read_only"), param->m_value, param->m_type,
          READ_ONLY_WAIT_LOCK_TIMEOUT));
      break;
    case Set_system_variable_parameters::VAR_OFFLINE_MODE:
      param->set_error(internal_set_system_variable(
          std::string("offline_mode"), param->m_value, param->m_type,
          WAIT_LOCK_TIMEOUT));
      break;
    case Set_system_variable_parameters::
        VAR_GROUP_REPLICATION_SINGLE_PRIMARY_MODE:
      param->set_error(internal_set_system_variable(
          std::string("group_replication_single_primary_mode"), param->m_value,
          param->m_type, WAIT_LOCK_TIMEOUT));
      break;
    case Set_system_variable_parameters::
        VAR_GROUP_REPLICATION_ENFORCE_UPDATE_EVERYWHERE_CHECKS:
      param->set_error(internal_set_system_variable(
          std::string("group_replication_enforce_update_everywhere_checks"),
          param->m_value, param->m_type, WAIT_LOCK_TIMEOUT));
      break;
    default:
      param->set_error(1);
  }
}

int Set_system_variable::internal_set_system_variable(
    const std::string &variable, const std::string &value,
    const std::string &type, unsigned long long lock_wait_timeout) {
  int error = 0;
  CHARSET_INFO_h charset_utf8{nullptr};
  my_h_string variable_name{nullptr};
  my_h_string variable_value{nullptr};
  const std::string lock_wait_timeout_name{"lock_wait_timeout"};
  my_h_string lock_wait_timeout_variable_name{nullptr};

#ifndef NDEBUG
  DBUG_EXECUTE_IF("group_replication_var_persist_error", {
    if (type == "PERSIST_ONLY") {
      return 1;
    }
  });
#endif

  if (nullptr == server_services_references_module->mysql_charset_service ||
      nullptr ==
          server_services_references_module->mysql_string_factory_service ||
      nullptr == server_services_references_module
                     ->mysql_string_charset_converter_service ||
      nullptr == server_services_references_module
                     ->mysql_system_variable_update_integer_service ||
      nullptr == server_services_references_module
                     ->mysql_system_variable_update_string_service) {
    error = 1;
    goto end;
  }

  if (server_services_references_module->mysql_string_factory_service->create(
          &lock_wait_timeout_variable_name)) {
    error = 1;
    goto end;
  }
  if (server_services_references_module->mysql_string_factory_service->create(
          &variable_name)) {
    error = 1;
    goto end;
  }
  if (server_services_references_module->mysql_string_factory_service->create(
          &variable_value)) {
    error = 1;
    goto end;
  }

  charset_utf8 =
      server_services_references_module->mysql_charset_service->get_utf8mb4();
  if (server_services_references_module->mysql_string_charset_converter_service
          ->convert_from_buffer(
              lock_wait_timeout_variable_name, lock_wait_timeout_name.c_str(),
              lock_wait_timeout_name.length(), charset_utf8)) {
    error = 1;
    goto end;
  }
  if (server_services_references_module->mysql_string_charset_converter_service
          ->convert_from_buffer(variable_name, variable.c_str(),
                                variable.length(), charset_utf8)) {
    error = 1;
    goto end;
  }
  if (server_services_references_module->mysql_string_charset_converter_service
          ->convert_from_buffer(variable_value, value.c_str(), value.length(),
                                charset_utf8)) {
    error = 1;
    goto end;
  }

  if (server_services_references_module
          ->mysql_system_variable_update_integer_service->set_unsigned(
              current_thd, "SESSION", nullptr, lock_wait_timeout_variable_name,
              lock_wait_timeout)) {
    error = 1;
    goto end;
  }

  if (server_services_references_module
          ->mysql_system_variable_update_string_service->set(
              current_thd, type.c_str(), nullptr, variable_name,
              variable_value)) {
    error = 1;
    goto end;
  }

end:
  if (nullptr != lock_wait_timeout_variable_name) {
    server_services_references_module->mysql_string_factory_service->destroy(
        lock_wait_timeout_variable_name);
  }
  if (nullptr != variable_name) {
    server_services_references_module->mysql_string_factory_service->destroy(
        variable_name);
  }
  if (nullptr != variable_value) {
    server_services_references_module->mysql_string_factory_service->destroy(
        variable_value);
  }

  return error;
}
