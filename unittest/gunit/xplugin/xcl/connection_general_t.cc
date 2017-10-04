/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <gtest/gtest.h>

#include "plugin/x/client/xconnection_impl.h"


namespace xcl {
namespace test {

TEST(Xcl_connection_impl_tests,
     get_socket_fd_return_invalid_socket_id_when_not_connected) {
  std::shared_ptr<Context> context { new Context() };
  Connection_impl          sut { context };

  ASSERT_EQ(INVALID_SOCKET, sut.get_socket_fd());
}

}  // namespace test
}  // namespace xcl
