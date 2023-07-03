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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_CAPABILITIES_CONFIGURATOR_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_CAPABILITIES_CONFIGURATOR_H_

#include <gmock/gmock.h>

#include "plugin/x/src/interface/capabilities_configurator.h"

namespace xpl {
namespace test {
namespace mock {

class Capabilities_configurator : public iface::Capabilities_configurator {
 public:
  Capabilities_configurator();
  virtual ~Capabilities_configurator() override;

  MOCK_METHOD(::Mysqlx::Connection::Capabilities *, get, (), (override));

  MOCK_METHOD(ngs::Error_code, prepare_set,
              (const ::Mysqlx::Connection::Capabilities &capabilities),
              (override));
  MOCK_METHOD(void, commit, (), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  // UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_CAPABILITIES_CONFIGURATOR_H_
