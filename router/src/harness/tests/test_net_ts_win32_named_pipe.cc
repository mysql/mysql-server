/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/win32_named_pipe.h"

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/impl/socket.h"  // net::impl::socket::init

#include "mysql/harness/stdx/expected_ostream.h"

#if defined(_WIN32)

#define EXPECT_NO_ERROR(x) \
  EXPECT_THAT((x), ::testing::Truly([](const auto &t) { return bool(t); }))

#define ASSERT_NO_ERROR(x) \
  ASSERT_THAT((x), ::testing::Truly([](const auto &t) { return bool(t); }))

using namespace std::string_literals;

template <class T>
class NamedPipeProtocolTest : public ::testing::Test {
 public:
};

using NamedPipeProtocolTypes =
    ::testing::Types<local::byte_protocol, local::message_protocol>;

TYPED_TEST_SUITE(NamedPipeProtocolTest, NamedPipeProtocolTypes);

TYPED_TEST(NamedPipeProtocolTest, socket_default_construct) {
  net::io_context io_ctx;

  typename TypeParam::socket sock(io_ctx);
}

TYPED_TEST(NamedPipeProtocolTest, endpoint_construct_default) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint;

  EXPECT_EQ(endpoint.size(), 0);
  EXPECT_EQ(endpoint.path().size(), 0);
  EXPECT_EQ(endpoint.path(), std::string());
  EXPECT_GT(endpoint.capacity(), 0);
}

TYPED_TEST(NamedPipeProtocolTest, endpoint_construct_pathname) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  std::string endpoint_name{R"(\\.\pipe\)"s};

  endpoint_type endpoint(endpoint_name);

  // at least the sizeof(family)
  EXPECT_EQ(endpoint.size(), endpoint_name.size());
  EXPECT_EQ(endpoint.path().size(), endpoint_name.size());
  EXPECT_EQ(endpoint.path(), endpoint_name);
}

TYPED_TEST(NamedPipeProtocolTest, endpoint_construct_pathname_truncated) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  // append
  endpoint_type endpoint(R"(\\.\pipe\)"s + std::string(256, 'a'));

  EXPECT_EQ(endpoint.size(), endpoint.capacity());
  EXPECT_EQ(endpoint.path().size(), endpoint.capacity());
  EXPECT_THAT(endpoint.path(), ::testing::StartsWith(R"(\\.\pipe\)"s));
}

TYPED_TEST(NamedPipeProtocolTest, endpoint_resize_zero) {
  using endpoint_type = typename TypeParam::endpoint;

  endpoint_type endpoint(R"(\\.\pipe\foo)"s);

  EXPECT_GT(endpoint.size(), 0);

  endpoint.resize(0);

  EXPECT_EQ(endpoint.size(), 0);
  EXPECT_EQ(endpoint.path().size(), 0);
  EXPECT_EQ(endpoint.path(), std::string{});
}

TYPED_TEST(NamedPipeProtocolTest, endpoint_resize_non_zero) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint(R"(\\.\pipe\foo)"s);

  EXPECT_EQ(endpoint.size(), 12);

  // cut away last char
  endpoint.resize(endpoint.size() - 1);

  // at least the sizeof(family)
  EXPECT_EQ(endpoint.size(), 11);
  EXPECT_EQ(endpoint.path().size(), 11);
  EXPECT_EQ(endpoint.path(), R"(\\.\pipe\fo)");
}

namespace local {
template <class Protocol>
std::ostream &operator<<(std::ostream &os,
                         const local::basic_named_pipe_impl<Protocol> &sock) {
  os << sock.native_handle();

  return os;
}
}  // namespace local

TEST(NetTS_named_pipe, stream_socket_bind_invalid_pipe_name) {
  std::string socket_path = R"(invalid-pipe-name)";

  net::io_context io_ctx;

  using protocol = local::byte_protocol;

  protocol::endpoint endp(socket_path);

  protocol::acceptor acceptor(io_ctx);
  EXPECT_NO_ERROR(acceptor.open());

  EXPECT_EQ(acceptor.bind(endp),
            stdx::make_unexpected(
                std::error_code{ERROR_INVALID_NAME, std::system_category()}));
  auto local_endp_res = acceptor.local_endpoint();
  ASSERT_NO_ERROR(local_endp_res);
}

TEST(NetTS_named_pipe, stream_socket_bind_accept_connect) {
  std::string socket_path = R"(\\.\pipe\abc)";

  net::io_context io_ctx;

  using protocol = local::byte_protocol;

  protocol::endpoint endp(socket_path);

  protocol::acceptor acceptor(io_ctx);
  EXPECT_NO_ERROR(acceptor.open());
  ASSERT_NO_ERROR(acceptor.bind(endp));
  EXPECT_NO_ERROR(acceptor.listen(128));

  // non-blocking is needed to accept()
  EXPECT_NO_ERROR(acceptor.native_non_blocking(true));

  // should fail with ERROR_PIPE_LISTENING
  EXPECT_EQ(acceptor.accept(),
            stdx::make_unexpected(
                std::error_code{ERROR_PIPE_LISTENING, std::system_category()}));
  auto local_endp_res = acceptor.local_endpoint();
  ASSERT_NO_ERROR(local_endp_res);

  auto local_endp = std::move(*local_endp_res);

  protocol::socket client_sock(io_ctx);
  EXPECT_NO_ERROR(client_sock.open());

  // ensure the connect() doesn't block
  EXPECT_NO_ERROR(client_sock.native_non_blocking(true));

  // even though non-blocking, this is a local named pipe and will quite likely
  // just succeed.
  ASSERT_NO_ERROR(client_sock.connect(local_endp));

  // accept() again which should finish accept now.
  auto server_sock_res = acceptor.accept();
  ASSERT_NO_ERROR(server_sock_res);
  auto server_sock = std::move(*server_sock_res);

  ASSERT_TRUE(server_sock.is_open());

  // named pipe is non-blocking, read() should non block if there is no data.
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  std::array<char, 16> sink;
  EXPECT_EQ(net::read(client_sock, net::buffer(sink)),
            stdx::make_unexpected(
                std::error_code{ERROR_NO_DATA, std::system_category()}));

  // write something
  auto write_res = net::write(server_sock, net::buffer(source));
  ASSERT_NO_ERROR(write_res);
  EXPECT_EQ(*write_res, source.size());

  // read should succeed now.
  auto read_res = net::read(client_sock, net::buffer(sink),
                            net::transfer_at_least(source.size()));
  ASSERT_NO_ERROR(read_res);
  EXPECT_EQ(*read_res, source.size());
}
#endif

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
