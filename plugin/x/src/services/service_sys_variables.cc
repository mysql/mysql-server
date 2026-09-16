/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/x/src/services/service_sys_variables.h"

#include <cstring>

#include "plugin/x/src/interface/service_registry.h"
#include "plugin/x/src/interface/service_sys_variables.h"

namespace xpl {

Service_sys_variables::Service_sys_variables(iface::Service_registry *registry)
    : m_registry(registry) {
  m_sys_variable =
      reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_system_variable_reader) *>(
          m_registry->acquire("mysql_system_variable_reader"));
  m_sys_variable_source =
      reinterpret_cast<SERVICE_TYPE_NO_CONST(system_variable_source) *>(
          m_registry->acquire("system_variable_source"));
}

Service_sys_variables::~Service_sys_variables() {
  if (m_sys_variable)
    m_registry->release(reinterpret_cast<my_h_service>(m_sys_variable));
  if (m_sys_variable_source)
    m_registry->release(reinterpret_cast<my_h_service>(m_sys_variable_source));
}

bool Service_sys_variables::get_variable(const char *component_name,
                                         const char *name, void **val,
                                         size_t *out_length_of_val) {
  if (!m_sys_variable) return false;

  return !m_sys_variable->get(nullptr, "GLOBAL", component_name, name, val,
                              out_length_of_val);
}

bool Service_sys_variables::get_variable_source(
    const char *name, enum enum_variable_source *source) {
  return m_sys_variable_source &&
         !m_sys_variable_source->get(name, std::strlen(name), source);
}

bool Service_sys_variables::is_valid() const {
  return nullptr != m_sys_variable && nullptr != m_sys_variable_source;
}

}  // namespace xpl
