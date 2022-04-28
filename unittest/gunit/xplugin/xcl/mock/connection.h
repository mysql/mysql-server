/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_CONNECTION_H_
#define UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_CONNECTION_H_

#include <gmock/gmock.h>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "plugin/x/client/mysqlxclient/xconnection.h"

namespace xcl {
namespace test {
namespace mock {

class XConnection : public xcl::XConnection {
 public:
  XConnection();
  virtual ~XConnection() override;

  MOCK_METHOD(XError, connect_to_localhost, (const std::string &unix_socket),
              (override));
  MOCK_METHOD(XError, connect,
              (const std::string &host, const uint16_t port,
               const Internet_protocol ip_mode),
              (override));
  MOCK_METHOD(my_socket, get_socket_fd, (), (override));
  MOCK_METHOD(XError, activate_tls, (), (override));
  MOCK_METHOD(XError, shutdown, (const Shutdown_type how_to_shutdown),
              (override));
  MOCK_METHOD(XError, write,
              (const uint8_t *data, const std::size_t data_length), (override));
  MOCK_METHOD(XError, read, (uint8_t * data, const std::size_t data_length),
              (override));
  MOCK_METHOD(XError, set_read_timeout, (const int deadline_milliseconds),
              (override));
  MOCK_METHOD(XError, set_write_timeout, (const int deadline_milliseconds),
              (override));
  MOCK_METHOD(void, close, (), (override));
  MOCK_METHOD(const State &, state, (), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xcl

#endif  // UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_CONNECTION_H_
