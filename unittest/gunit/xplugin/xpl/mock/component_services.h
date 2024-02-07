/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_COMPONENT_SERVICES_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_COMPONENT_SERVICES_H_

#include <assert.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// NOLINT(build/include_subdir)
#include "mysql/components/services/mysql_admin_session.h"
#include "mysql/service_plugin_registry.h"

namespace xpl {
namespace test {
namespace mock {

class Mysql_plugin_registry {
 public:
  Mysql_plugin_registry();
  ~Mysql_plugin_registry();

  MOCK_METHOD(int, mysql_plugin_registry_release, (SERVICE_TYPE(registry) *));
  MOCK_METHOD(SERVICE_TYPE(registry) *, mysql_plugin_registry_acquire, ());

  static xpl::test::mock::Mysql_plugin_registry *m_mysql_plugin_registry;
};

class Service_registry {
 public:
  Service_registry();
  ~Service_registry();

  MOCK_METHOD2(acquire, mysql_service_status_t(const char *service_name,
                                               my_h_service *out_service));
  MOCK_METHOD3(acquire_related,
               mysql_service_status_t(const char *service_name,
                                      my_h_service service,
                                      my_h_service *out_service));
  MOCK_METHOD(mysql_service_status_t, release, (my_h_service service));

  SERVICE_TYPE_NO_CONST(registry) * get() { return &m_registry; }

 private:
  static xpl::test::mock::Service_registry *m_this;

  SERVICE_TYPE_NO_CONST(registry)
  m_registry = {[](const char *service_name, my_h_service *out_service) {
                  return m_this->acquire(service_name, out_service);
                },
                [](const char *service_name, my_h_service service,
                   my_h_service *out_service) {
                  return m_this->acquire_related(service_name, service,
                                                 out_service);
                },
                [](my_h_service service) { return m_this->release(service); }};
};

class Service_admin_session {
 public:
  Service_admin_session();
  ~Service_admin_session();

  MOCK_METHOD(MYSQL_SESSION, open, (srv_session_error_cb, void *));
  SERVICE_TYPE_NO_CONST(mysql_admin_session) * get() {
    return &m_admin_session;
  }

 private:
  static xpl::test::mock::Service_admin_session *m_this;

  SERVICE_TYPE_NO_CONST(mysql_admin_session)
  m_admin_session = {[](srv_session_error_cb error_cb, void *ctxt) {
    return m_this->open(error_cb, ctxt);
  }};
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_COMPONENT_SERVICES_H_
