/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/x/src/services/service_registrator.h"

#include <mysql/service_plugin_registry.h>
#include <stdexcept>
#include <string>

namespace xpl {

Service_registrator::Service_registrator()
    : m_registry{mysql_plugin_registry_acquire()},
      m_registrator{"registry_registration", m_registry} {}

Service_registrator::~Service_registrator() {
  mysql_plugin_registry_release(m_registry);
}

void Service_registrator::register_service(const Service &s) {
  if (!m_registrator.is_valid() ||
      m_registrator->register_service(s.m_name, s.m_service))
    throw std::runtime_error(std::string("Can't register '") + s.m_name +
                             "' service");
}

void Service_registrator::unregister_service(const char *name) {
  if (!m_registrator.is_valid() || m_registrator->unregister(name))
    throw std::runtime_error(std::string("Can't unregister '") + name +
                             "' service");
}

}  // namespace xpl
