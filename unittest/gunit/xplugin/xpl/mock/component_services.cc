/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "unittest/gunit/xplugin/xpl/mock/component_services.h"

namespace xpl {
namespace test {

mock::Service_registry *mock::Service_registry::m_this = nullptr;
mock::Service_admin_session *mock::Service_admin_session::m_this = nullptr;
mock::Mysql_plugin_registry
    *mock::Mysql_plugin_registry::m_mysql_plugin_registry = nullptr;

namespace mock {

Mysql_plugin_registry::Mysql_plugin_registry() {
  assert(nullptr == m_mysql_plugin_registry);
  m_mysql_plugin_registry = this;
}

Mysql_plugin_registry::~Mysql_plugin_registry() {
  m_mysql_plugin_registry = nullptr;
}

Service_registry::Service_registry() {
  assert(m_this == nullptr);
  m_this = this;
}

Service_registry::~Service_registry() { m_this = nullptr; }

Service_admin_session::Service_admin_session() {
  assert(m_this == nullptr);
  m_this = this;
}

Service_admin_session::~Service_admin_session() { m_this = nullptr; }

}  // namespace mock
}  // namespace test
}  // namespace xpl
