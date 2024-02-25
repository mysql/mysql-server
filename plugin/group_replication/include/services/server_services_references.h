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

#ifndef GR_SERVER_SERVICES_REFERENCES
#define GR_SERVER_SERVICES_REFERENCES

#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/mysql_system_variable.h>
#include <mysql/components/services/registry.h>
#include <mysql/service_plugin_registry.h>
#include <string>

/**
  @class Server_services_references

  This class holds the references to server services that
  Group Replication acquires on plugin install.
  server services are the ones implemented on
  `sql/server_component` folder.
*/
class Server_services_references {
 public:
  Server_services_references() = default;

  virtual ~Server_services_references();

  /**
    Deleted copy ctor.
  */
  Server_services_references(const Server_services_references &) = delete;

  /**
    Deleted move ctor.
  */
  Server_services_references(const Server_services_references &&) = delete;

  /**
    Deleted assignment operator.
  */
  Server_services_references &operator=(const Server_services_references &) =
      delete;

  /**
    Deleted move operator.
  */
  Server_services_references &operator=(const Server_services_references &&) =
      delete;

  /**
    Acquire the server services.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool initialize();

  /**
    Release the server services.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool finalize();

  /* clang-format off */
  SERVICE_TYPE(registry) *registry_service{nullptr};
  SERVICE_TYPE(mysql_charset) *mysql_charset_service{nullptr};
  SERVICE_TYPE(mysql_string_factory) *mysql_string_factory_service{nullptr};
  SERVICE_TYPE(mysql_string_charset_converter) *mysql_string_charset_converter_service{nullptr};
  SERVICE_TYPE(mysql_system_variable_update_string) *mysql_system_variable_update_string_service{nullptr};
  SERVICE_TYPE(mysql_system_variable_update_integer) *mysql_system_variable_update_integer_service{nullptr};
  SERVICE_TYPE(component_sys_variable_register) *component_sys_variable_register_service{nullptr};
  /* clang-format on */

 private:
  /* clang-format off */
  my_h_service m_mysql_charset_handle{nullptr};
  my_h_service m_mysql_string_factory_handle{nullptr};
  my_h_service m_mysql_string_charset_converter_handle{nullptr};
  my_h_service m_mysql_system_variable_update_string_handle{nullptr};
  my_h_service m_mysql_system_variable_update_integer_handle{nullptr};
  my_h_service m_component_sys_variable_register_handle{nullptr};
  /* clang-format on */
};

#endif  // GR_SERVER_SERVICES_REFERENCES
