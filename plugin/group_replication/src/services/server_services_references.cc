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

#include "plugin/group_replication/include/services/server_services_references.h"

Server_services_references::~Server_services_references() { finalize(); }

bool Server_services_references::initialize() {
  bool error = false;

  registry_service = mysql_plugin_registry_acquire();
  if (nullptr == registry_service) {
    error = true;
    goto end;
  }

  if (registry_service->acquire("mysql_charset", &m_mysql_charset_handle)) {
    error = true;
    goto end;
  }
  mysql_charset_service =
      reinterpret_cast<SERVICE_TYPE(mysql_charset) *>(m_mysql_charset_handle);

  if (registry_service->acquire("mysql_string_factory",
                                &m_mysql_string_factory_handle)) {
    error = true;
    goto end;
  }
  mysql_string_factory_service =
      reinterpret_cast<SERVICE_TYPE(mysql_string_factory) *>(
          m_mysql_string_factory_handle);

  if (registry_service->acquire("mysql_string_charset_converter",
                                &m_mysql_string_charset_converter_handle)) {
    error = true;
    goto end;
  }
  mysql_string_charset_converter_service =
      reinterpret_cast<SERVICE_TYPE(mysql_string_charset_converter) *>(
          m_mysql_string_charset_converter_handle);

  if (registry_service->acquire(
          "mysql_system_variable_update_string",
          &m_mysql_system_variable_update_string_handle)) {
    error = true;
    goto end;
  }
  mysql_system_variable_update_string_service =
      reinterpret_cast<SERVICE_TYPE(mysql_system_variable_update_string) *>(
          m_mysql_system_variable_update_string_handle);

  if (registry_service->acquire(
          "mysql_system_variable_update_integer",
          &m_mysql_system_variable_update_integer_handle)) {
    error = true;
    goto end;
  }
  mysql_system_variable_update_integer_service =
      reinterpret_cast<SERVICE_TYPE(mysql_system_variable_update_integer) *>(
          m_mysql_system_variable_update_integer_handle);

  if (registry_service->acquire("component_sys_variable_register",
                                &m_component_sys_variable_register_handle)) {
    error = true;
    goto end;
  }
  component_sys_variable_register_service =
      reinterpret_cast<SERVICE_TYPE(component_sys_variable_register) *>(
          m_component_sys_variable_register_handle);

end:
  if (error) {
    finalize();
  }

  return error;
}

bool Server_services_references::finalize() {
  int error = 0;

  component_sys_variable_register_service = nullptr;
  if (nullptr != m_component_sys_variable_register_handle) {
    error |=
        registry_service->release(m_component_sys_variable_register_handle);
    m_component_sys_variable_register_handle = nullptr;
  }

  mysql_system_variable_update_integer_service = nullptr;
  if (nullptr != m_mysql_system_variable_update_integer_handle) {
    error |= registry_service->release(
        m_mysql_system_variable_update_integer_handle);
    m_mysql_system_variable_update_integer_handle = nullptr;
  }

  mysql_system_variable_update_string_service = nullptr;
  if (nullptr != m_mysql_system_variable_update_string_handle) {
    error |=
        registry_service->release(m_mysql_system_variable_update_string_handle);
    m_mysql_system_variable_update_string_handle = nullptr;
  }

  mysql_string_charset_converter_service = nullptr;
  if (nullptr != m_mysql_string_charset_converter_handle) {
    error |= registry_service->release(m_mysql_string_charset_converter_handle);
    m_mysql_string_charset_converter_handle = nullptr;
  }

  mysql_string_factory_service = nullptr;
  if (nullptr != m_mysql_string_factory_handle) {
    error |= registry_service->release(m_mysql_string_factory_handle);
    m_mysql_string_factory_handle = nullptr;
  }

  mysql_charset_service = nullptr;
  if (nullptr != m_mysql_charset_handle) {
    error |= registry_service->release(m_mysql_charset_handle);
    m_mysql_charset_handle = nullptr;
  }

  if (nullptr != registry_service) {
    error |= mysql_plugin_registry_release(registry_service);
    registry_service = nullptr;
  }

  return (0 != error);
}
