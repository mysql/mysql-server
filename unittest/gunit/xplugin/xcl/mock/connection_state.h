/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_CONNECTION_STATE_H_
#define UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_CONNECTION_STATE_H_

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>

#include "plugin/x/client/mysqlxclient/xconnection.h"

namespace xcl {
namespace test {
namespace mock {

class XConnection_state : public xcl::XConnection::State {
 public:
  XConnection_state();
  virtual ~XConnection_state() override;

  MOCK_METHOD(bool, is_ssl_configured, (), (const, override));
  MOCK_METHOD(bool, is_ssl_activated, (), (const, override));
  MOCK_METHOD(bool, is_connected, (), (const, override));
  MOCK_METHOD(std::string, get_ssl_version, (), (const, override));
  MOCK_METHOD(std::string, get_ssl_cipher, (), (const, override));
  MOCK_METHOD(Connection_type, get_connection_type, (), (const, override));
  MOCK_METHOD(bool, has_data, (), (const, override));
};

}  // namespace mock
}  // namespace test
}  // namespace xcl

#endif  // UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_CONNECTION_STATE_H_
