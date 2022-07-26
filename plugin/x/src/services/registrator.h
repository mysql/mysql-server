/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_SERVICES_REGISTRATOR_H_
#define PLUGIN_X_SRC_SERVICES_REGISTRATOR_H_

#include <memory>

#include "plugin/x/src/interface/service_register_service.h"
#include "plugin/x/src/services/service_registry.h"

#define SERVICE_ID(component, service) #service "." #component

#define SERVICE(component, service)                                  \
  {                                                                  \
    SERVICE_ID(component, service),                                  \
        reinterpret_cast<my_h_service>(&imp_##component##_##service) \
  }

namespace xpl {

class Service_registrator {
 public:
  struct Service {
    const char *m_name;
    my_h_service m_service;
  };

  Service_registrator();

  void register_service(const Service &service);
  void unregister_service(const char *name);

 private:
  Service_registry m_registry;
  std::unique_ptr<iface::Service_registry_registration> m_registration;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVICES_REGISTRATOR_H_
