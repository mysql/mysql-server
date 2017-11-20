/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef XPLUGIN_XCL_MOCK_CONNECTION_H_
#define XPLUGIN_XCL_MOCK_CONNECTION_H_

#include <gmock/gmock.h>
#include <cstdint>
#include <memory>
#include <utility>
#include <string>

#include "mysqlxclient/xconnection.h"


namespace xcl {
namespace test {

class Mock_connection : public XConnection {
 public:
  MOCK_METHOD1(connect_to_localhost,
      XError(const std::string &unix_socket));
  MOCK_METHOD3(connect,
      XError(const std::string &host,
             const uint16_t port,
             const Internet_protocol ip_mode));
  MOCK_METHOD0(get_socket_fd,
      my_socket());
  MOCK_METHOD0(activate_tls,
      XError());
  MOCK_METHOD1(shutdown,
      XError(const Shutdown_type how_to_shutdown));
  MOCK_METHOD2(write,
      XError(const uint8_t *data, const std::size_t data_length));
  MOCK_METHOD2(read,
      XError(uint8_t *data, const std::size_t data_length));
  MOCK_METHOD1(set_read_timeout,
      XError(const int deadline_milliseconds));
  MOCK_METHOD1(set_write_timeout,
      XError(const int deadline_milliseconds));
  MOCK_METHOD0(close,
      void());
  MOCK_METHOD0(state,
      const State&());
};

}  // namespace test
}  // namespace xcl

#endif  // XPLUGIN_XCL_MOCK_CONNECTION_H_
