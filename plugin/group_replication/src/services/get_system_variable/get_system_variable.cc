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

#include "plugin/group_replication/include/services/get_system_variable/get_system_variable.h"
#include "plugin/group_replication/include/plugin.h"

#include "sql/sql_class.h"

// safeguard due unknown gtid_executed or gtid_purged length
#define GTID_VALUES_FETCH_BUFFER_SIZE 500000

Get_system_variable::Get_system_variable(bool create_session) {
  if (create_session) {
    my_thread_init();

    m_previous_current_thd = current_thd;
    THD *thd = new THD;
    thd->thread_stack = (char *)&thd;
    thd->store_globals();
    thd->security_context()->skip_grants();
    thd->system_thread = SYSTEM_THREAD_BACKGROUND;
    thd->set_new_thread_id();
    m_thd = thd;
  }

  get_plugin_registry()->acquire(
      "component_sys_variable_register",
      &component_sys_variable_register_service_handler);
}

Get_system_variable::~Get_system_variable() {
  if (component_sys_variable_register_service_handler != nullptr) {
    get_plugin_registry()->release(
        component_sys_variable_register_service_handler);
  }

  if (nullptr != m_thd) {
    m_thd->release_resources();
    delete m_thd;
    m_thd = nullptr;

    if (nullptr != m_previous_current_thd) {
      m_previous_current_thd->store_globals();
      m_previous_current_thd = nullptr;
    }

    my_thread_end();
  }
}

int Get_system_variable::get_server_gtid_executed(std::string &gtid_executed) {
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
          "mysql_server", "gtid_executed",
          reinterpret_cast<void **>(&var_value), &var_len)) {
    error = true; /* purecov: inspected */
    goto end;     /* purecov: inspected */
  }

  gtid_executed.assign(var_value, var_len);

end:
  delete[] var_value;
  return error;
}

int Get_system_variable::get_server_gtid_purged(std::string &gtid_purged) {
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
          "mysql_server", "gtid_purged", reinterpret_cast<void **>(&var_value),
          &var_len)) {
    error = true; /* purecov: inspected */
    goto end;     /* purecov: inspected */
  }

  gtid_purged.assign(var_value, var_len);

end:
  delete[] var_value;
  return error;
}
