/*
 * Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_AUTHENTICATION_CONTAINER_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_AUTHENTICATION_CONTAINER_H_

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/interface/authentication_container.h"

namespace xpl {
namespace test {
namespace mock {

class Authentication_container : public iface::Authentication_container {
 public:
  Authentication_container();
  virtual ~Authentication_container() override;

  std::unique_ptr<iface::Authentication> get_auth_handler(
      const std::string &name, iface::Session *session) override {
    return std::unique_ptr<iface::Authentication>{
        get_auth_handler_raw(name, session)};
  }

  MOCK_METHOD(iface::Authentication *, get_auth_handler_raw,
              (const std::string &name, iface::Session *session));
  MOCK_METHOD(std::vector<std::string>, get_authentication_mechanisms,
              (iface::Client * client), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_AUTHENTICATION_CONTAINER_H_
