/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/net_ts/impl/socket.h"

#include <csignal>  // SIG_IGN
#include <system_error>

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/impl/file.h"
#include "mysql/harness/stdx/expected_ostream.h"

/**
 * @test ensure close() a socket with invalid handle fails.
 */
TEST(NetTS_impl_socket, close_invalid_handle) {
  auto expected_ec =
#ifdef _WIN32
      net::impl::socket::make_error_code(WSAENOTSOCK)
#else
      make_error_code(std::errc::bad_file_descriptor)
#endif
      ;

  EXPECT_THAT(net::impl::socket::close(net::impl::socket::kInvalidSocket),
              ::testing::Eq(stdx::make_unexpected(expected_ec)));
}

/**
 * @test ensure close() on open socket works.
 */
TEST(NetTS_impl_socket, socket) {
  auto sock_ec = net::impl::socket::socket(AF_INET, SOCK_STREAM, 0);

  ASSERT_TRUE(sock_ec);

  auto sock_fd = std::move(sock_ec.value());

  EXPECT_TRUE(net::impl::socket::close(sock_fd));
}

/**
 * @test shutdown() fails for not-connected socket.
 */
TEST(NetTS_impl_socket, shutdown_not_connected_socket) {
  auto sock_ec = net::impl::socket::socket(AF_INET, SOCK_STREAM, 0);

  ASSERT_TRUE(sock_ec);

  auto sock_fd = std::move(sock_ec.value());

#ifdef _WIN32
  int shut_how = SD_RECEIVE;
#else
  int shut_how = SHUT_RD;
#endif

  // wine: WSAESHUTDOWN
  // windows: WSAENOTCONN
  // posix: WSAENOTCONN
  auto shutdown_res = net::impl::socket::shutdown(sock_fd, shut_how);
  ASSERT_FALSE(shutdown_res);

  EXPECT_THAT(shutdown_res.error(),
              ::testing::AnyOf(std::error_code(10058, std::system_category()),
                               make_error_condition(std::errc::not_connected)));

  EXPECT_TRUE(net::impl::socket::close(sock_fd));
}

/**
 * @test recvmsg() fails for not-connected socket.
 */
TEST(NetTS_impl_socket, recv_not_connected_socket_into_empty_buffer) {
  auto sock_ec = net::impl::socket::socket(AF_INET, SOCK_STREAM, 0);

  ASSERT_TRUE(sock_ec);

  auto sock_fd = std::move(sock_ec.value());

  // the EMSGSIZE/WSAEINVAL is triggered by the empty buffer
  net::impl::socket::msghdr_base msghdr{};

  auto recv_res = net::impl::socket::recvmsg(sock_fd, msghdr, 0);
  ASSERT_FALSE(recv_res);
  // macosx:  EMSGSIZE
  // freebsd: ENOTCONN
  // linux:   ENOTCONN
  // wine:    WSAECONNRESET
  // windows: WSAEINVAL
  EXPECT_THAT(
      recv_res.error(),
      ::testing::AnyOf(make_error_code(std::errc::message_size),
                       make_error_condition(std::errc::not_connected),
                       make_error_condition(std::errc::connection_reset),
                       make_error_condition(std::errc::invalid_argument)));

  EXPECT_TRUE(net::impl::socket::close(sock_fd));
}

/**
 * @test sendmsg() fails for not-connected socket.
 */
TEST(NetTS_impl_socket, send_not_connected_socket) {
  auto sock_ec = net::impl::socket::socket(AF_INET, SOCK_STREAM, 0);

  ASSERT_TRUE(sock_ec);

  auto sock_fd = std::move(sock_ec.value());

  net::impl::socket::msghdr_base msghdr{};

  // EMSGSIZE is triggered by the empty buffer
  auto send_res = net::impl::socket::sendmsg(sock_fd, msghdr, 0);

  ASSERT_FALSE(send_res);
  // wine:    WSAECONNRESET
  // windows: WSAENOTCONN
  // macosx:  EMSGSIZE
  // freebsd: ENOTCONN
  // linux:   EPIPE
  EXPECT_THAT(
      send_res.error(),
      ::testing::AnyOf(make_error_code(std::errc::message_size),
                       make_error_code(std::errc::broken_pipe),
                       make_error_condition(std::errc::not_connected),
                       make_error_condition(std::errc::connection_reset)));

  EXPECT_TRUE(net::impl::socket::close(sock_fd));
}

/**
 * @test ensure, native_non_blocking() works.
 */
TEST(NetTS_impl_socket, native_non_blocking) {
  auto sock_ec = net::impl::socket::socket(AF_INET, SOCK_STREAM, 0);

  ASSERT_TRUE(sock_ec);

  auto sock_fd = std::move(sock_ec.value());

  {
    auto nb_ec = net::impl::socket::native_non_blocking(sock_fd);
    if (nb_ec) {
      EXPECT_EQ(*nb_ec, false);
    } else {
      EXPECT_EQ(nb_ec, stdx::make_unexpected(
                           make_error_code(std::errc::function_not_supported)));
    }
  }
  {
    auto nb_ec = net::impl::socket::native_non_blocking(sock_fd, true);
    EXPECT_TRUE(nb_ec) << nb_ec.error();
  }
  {
    auto nb_ec = net::impl::socket::native_non_blocking(sock_fd);
    if (nb_ec) {
      EXPECT_EQ(*nb_ec, true);
    } else {
      EXPECT_EQ(nb_ec, stdx::make_unexpected(
                           make_error_code(std::errc::function_not_supported)));
    }
  }
  {
    auto nb_ec = net::impl::socket::native_non_blocking(sock_fd, false);
    EXPECT_TRUE(nb_ec) << nb_ec.error();
  }
  {
    auto nb_ec = net::impl::socket::native_non_blocking(sock_fd);
    if (nb_ec) {
      EXPECT_EQ(*nb_ec, false);
    } else {
      EXPECT_EQ(nb_ec, stdx::make_unexpected(
                           make_error_code(std::errc::function_not_supported)));
    }
  }

  EXPECT_TRUE(net::impl::socket::close(sock_fd));
}

int main(int argc, char *argv[]) {
#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
