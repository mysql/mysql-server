/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_SERVICES_SERVICE_REGISTRY_REGISTRATION_H_
#define PLUGIN_X_SRC_SERVICES_SERVICE_REGISTRY_REGISTRATION_H_

#include "plugin/x/src/interface/service_register_service.h"
#include "plugin/x/src/interface/service_registry.h"

namespace xpl {

class Service_registry_registration
    : public iface::Service_registry_registration {
 public:
  explicit Service_registry_registration(iface::Service_registry *registry);
  ~Service_registry_registration() override;

  bool register_service(const char *service_implementation_name,
                        my_h_service ptr) override;
  bool unregister(const char *service_implementation_name) override;

  bool is_valid() override;

 private:
  iface::Service_registry *m_registry;
  SERVICE_TYPE_NO_CONST(registry_registration) * m_registry_registration;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVICES_SERVICE_REGISTRY_REGISTRATION_H_
