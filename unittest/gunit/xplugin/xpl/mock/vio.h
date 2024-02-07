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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_VIO_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_VIO_H_

#include <gmock/gmock.h>
#include <string>

#include "plugin/x/src/interface/vio.h"

namespace xpl {
namespace test {
namespace mock {

class Vio : public iface::Vio {
 public:
  Vio();
  virtual ~Vio() override;

  MOCK_METHOD(ssize_t, read, (uchar * buffer, ssize_t bytes_to_send),
              (override));
  MOCK_METHOD(ssize_t, write, (const uchar *buffer, ssize_t bytes_to_send),
              (override));
  MOCK_METHOD(void, set_timeout_in_ms,
              (const iface::Vio::Direction, const uint64_t timeout),
              (override));
  MOCK_METHOD(void, set_state, (PSI_socket_state state), (override));
  MOCK_METHOD(void, set_thread_owner, (), (override));
  MOCK_METHOD(my_socket, get_fd, (), (override));
  MOCK_METHOD(Connection_type, get_type, (), (const, override));
  MOCK_METHOD(sockaddr_storage *, peer_addr,
              (std::string * address, uint16_t *port), (override));
  MOCK_METHOD(int32_t, shutdown, (), (override));
  MOCK_METHOD(::Vio *, get_vio, (), (override));
  MOCK_METHOD(MYSQL_SOCKET &, get_mysql_socket, (), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_VIO_H_
