/*
   Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef STORAGE_NDB_PLUGIN_NDB_MYSQL_SERVICES_H
#define STORAGE_NDB_PLUGIN_NDB_MYSQL_SERVICES_H

#include <mysql/components/services/registry.h>

/**
   RAII style wrapper for working with services provided in the MySQL Server

   There are three different ways to work with the services:
    1. For long lived usage. Use @ref acquire_service() and @ref
       release_service(), then keep references to the service for an
       extended time.
    2. For short lived usage.
      a) Implement functions in this class, see @ref
         request_mysql_server_shutdown as an example. This is particularly
         useful for functionality which is used from several different places.
      b) Use the @ref my_service class, which provides a RAII interface for
         acquiring and calling the service functions. The acquired service will
         automatically be released.
*/
class Ndb_mysql_services {
  /**
     The MySQL Server creates the minimal chassis (which amongst other
     implements the registry service) and registers its registry implementation
     as a service in there. This means that the registry service always exist
     and a reference can be acquired directly in the constructor of this class.
     This also means it's not necessary to check the pointer when using it in
     functions.
  */
  const mysql_service_registry_t *const m_registry;

 public:
  Ndb_mysql_services();
  ~Ndb_mysql_services();

  operator const mysql_service_registry_t *() const { return m_registry; }

  /**
     Use "registry" service to acquire a service by name

    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  template <typename T>
  bool acquire_service(T &service, const char *name) const {
    my_h_service mysql_service;
    if (m_registry->acquire(name, &mysql_service)) {
      return true;
    }
    service = reinterpret_cast<T>(mysql_service);
    return false;
  }

  /**
    Use "registry" service to release a service which was previously
    acquired
  */
  template <typename T>
  void release_service(T &service) const {
    if (!m_registry) {
      return;
    }
    if (service != nullptr) {
      m_registry->release(reinterpret_cast<my_h_service>(service));
      service = nullptr;
    }
  }

  /**
    Use "host_application_signal" service to request server shutdown

    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  bool request_mysql_server_shutdown() const;
};

#endif
