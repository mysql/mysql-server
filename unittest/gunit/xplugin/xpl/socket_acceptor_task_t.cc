/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "plugin/x/ngs/include/ngs/interface/listener_factory_interface.h"
#include "plugin/x/ngs/include/ngs/socket_acceptors_task.h"
#include "unittest/gunit/xplugin/xpl/mock/ngs_general.h"

namespace ngs {
namespace test {

using ::testing::Ref;
using ::testing::StrEq;

const uint32 k_backlog = 10;
const std::string k_unix_file = "unix test";
const std::string k_host = "host test";
const uint16 k_port = 11;
const uint32 k_open_timeout = 12;

TEST(Socket_acceptors_task_suite, prepare_without_any_interface) {
  Mock_listener_factory_interface mock_factory;
  std::shared_ptr<Mock_socket_events> mock_event{new Mock_socket_events()};

  EXPECT_CALL(
      mock_factory,
      create_tcp_socket_listener_ptr(
          StrEq(k_host), k_port, k_open_timeout,
          Ref(*(Socket_events_interface *)mock_event.get()), k_backlog));

#if defined(HAVE_SYS_UN_H)
  EXPECT_CALL(
      mock_factory,
      create_unix_socket_listener_ptr(
          StrEq(k_unix_file), Ref(*(Socket_events_interface *)mock_event.get()),
          k_backlog));
#endif

  Socket_acceptors_task sut(mock_factory, k_host, k_port, k_open_timeout,
                            k_unix_file, k_backlog, mock_event);

  Server_task_interface::Task_context context;
  ASSERT_FALSE(sut.prepare(&context));
}

}  // namespace test
}  // namespace ngs
