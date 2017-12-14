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

#ifndef MYSQLX_SERVICE_H
#define MYSQLX_SERVICE_H

#include <mysql/components/my_service.h>

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
    const char* m_name;
    my_h_service m_service;
  };

  Service_registrator();
  ~Service_registrator();
  void register_service(const Service& service);
  void unregister_service(const char* name);

 private:
  Service_registrator(const Service_registrator&) = delete;
  const Service_registrator& operator==(const Service_registrator&) = delete;

  SERVICE_TYPE(registry) * m_registry;
  my_service<SERVICE_TYPE(registry_registration)> m_registrator;
};

}  // namespace xpl

#endif  // MYSQLX_SERVICE_H
