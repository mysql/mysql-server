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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SYSTEM_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SYSTEM_H_

#include <gmock/gmock.h>
#include <string>

#include "plugin/x/src/interface/system.h"

namespace xpl {
namespace test {
namespace mock {

class System : public iface::System {
 public:
  System();
  virtual ~System() override;

  MOCK_METHOD(int32_t, unlink, (const char *), (override));
  MOCK_METHOD(int32_t, kill, (int32_t, int32_t), (override));

  MOCK_METHOD(int32_t, get_ppid, (), (override));
  MOCK_METHOD(int32_t, get_pid, (), (override));
  MOCK_METHOD(int32_t, get_errno, (), (override));

  MOCK_METHOD(int32_t, get_socket_errno, (), (override));
  MOCK_METHOD(void, set_socket_errno, (const int32_t), (override));
  MOCK_METHOD(void, get_socket_error_and_message, (int32_t *, std::string *),
              (override));

  MOCK_METHOD(void, freeaddrinfo, (addrinfo *), (override));
  MOCK_METHOD(int32_t, getaddrinfo,
              (const char *, const char *, const addrinfo *, addrinfo **),
              (override));

  MOCK_METHOD(void, sleep, (uint32_t), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  // UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SYSTEM_H_
