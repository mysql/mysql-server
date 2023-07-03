/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include "plugin/x/src/services/service_registry_registration.h"

#include "plugin/x/src/interface/service_register_service.h"
#include "plugin/x/src/interface/service_registry.h"

namespace xpl {

Service_registry_registration::Service_registry_registration(
    iface::Service_registry *registry)
    : m_registry(registry) {
  m_registry_registration =
      reinterpret_cast<SERVICE_TYPE_NO_CONST(registry_registration) *>(
          m_registry->acquire("registry_registration"));
}

Service_registry_registration::~Service_registry_registration() {
  m_registry->release(reinterpret_cast<my_h_service>(m_registry_registration));
}

bool Service_registry_registration::register_service(
    const char *service_implementation_name, my_h_service ptr) {
  return !m_registry_registration->register_service(service_implementation_name,
                                                    ptr);
}

bool Service_registry_registration::unregister(
    const char *service_implementation_name) {
  return !m_registry_registration->unregister(service_implementation_name);
}

bool Service_registry_registration::is_valid() {
  return nullptr != m_registry_registration;
}

}  // namespace xpl
