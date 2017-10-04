/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "plugin/x/src/service_registrator.h"

#include <stdexcept>
#include <string>
#include <mysql/service_plugin_registry.h>

namespace xpl {

Service_registrator::Service_registrator()
    : m_registry{mysql_plugin_registry_acquire()},
      m_registrator{"registry_registration", m_registry} {}

Service_registrator::~Service_registrator() {
  mysql_plugin_registry_release(m_registry);
}

void Service_registrator::register_service(const Service& s) {
  if (!m_registrator.is_valid() ||
      m_registrator->register_service(s.m_name, s.m_service))
    throw std::runtime_error(std::string("Can't register '") + s.m_name +
                             "' service");
}

void Service_registrator::unregister_service(const char* name) {
  if (!m_registrator.is_valid() || m_registrator->unregister(name))
    throw std::runtime_error(std::string("Can't unregister '") + name +
                             "' service");
}

}  // namespace xpl
