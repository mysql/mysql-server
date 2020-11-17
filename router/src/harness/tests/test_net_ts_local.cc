/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/local.h"

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/socket.h"  // net::impl::socket::init

#include "mysql/harness/stdx/expected_ostream.h"
#include "test/helpers.h"  // TmpDir

#ifndef _WIN32

template <class T>
class LocalProtocolTest : public ::testing::Test {
 public:
};

using LocalProtocolTypes =
    ::testing::Types<local::stream_protocol, local::datagram_protocol,
                     local::seqpacket_protocol>;

TYPED_TEST_SUITE(LocalProtocolTest, LocalProtocolTypes);

template <class T>
class LocalTwoWayProtocolTest : public ::testing::Test {
 public:
};

using LocalTwoWayProtocolTypes =
    ::testing::Types<local::stream_protocol, local::seqpacket_protocol>;

TYPED_TEST_SUITE(LocalTwoWayProtocolTest, LocalTwoWayProtocolTypes);

TYPED_TEST(LocalProtocolTest, socket_default_construct) {
  net::io_context io_ctx;

  typename TypeParam::socket sock(io_ctx);
}

TYPED_TEST(LocalProtocolTest, endpoint_construct_default) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint;

  // sizeof(sa_family_t) on Linux [2], larger on others
  EXPECT_GT(endpoint.size(), 0);
  EXPECT_EQ(endpoint.path().size(), 0);
  EXPECT_EQ(endpoint.path(), std::string());
  EXPECT_EQ(endpoint.capacity(), sizeof(sockaddr_un));
}

TYPED_TEST(LocalProtocolTest, endpoint_construct_pathname) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint("/foo/bar");

  // at least the sizeof(family)
  EXPECT_GT(endpoint.size(), 8);
  EXPECT_EQ(endpoint.path().size(), 8);
  EXPECT_EQ(endpoint.path(), "/foo/bar");
}

TYPED_TEST(LocalProtocolTest, endpoint_construct_pathname_truncated) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint(
      "/foo/bar/some/very/long/path/name/that/is/longer/than/108/or/so/chars/"
      "to/test/that/truncation/works/and/nothing/gets/overridden");

  // at least the sizeof(family)
  EXPECT_EQ(endpoint.size(), endpoint.capacity());
  EXPECT_LT(endpoint.path().size(), endpoint.capacity());
  EXPECT_THAT(endpoint.path(), ::testing::StartsWith("/foo/bar"));
}

TYPED_TEST(LocalProtocolTest, endpoint_resize_zero) {
  using endpoint_type = typename TypeParam::endpoint;

  endpoint_type endpoint("/foo/bar");

  EXPECT_GT(endpoint.size(), 8);

  endpoint.resize(0);

  // at least the sizeof(family)
  EXPECT_GT(endpoint.size(), 0);
  EXPECT_EQ(endpoint.path().size(), 0);
  EXPECT_EQ(endpoint.path(), std::string{});
}

TYPED_TEST(LocalProtocolTest, endpoint_resize_non_zero) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint("/foo/bar");

  EXPECT_GT(endpoint.size(), 8);

  // cut away last char
  endpoint.resize(endpoint.size() - 1);

  // at least the sizeof(family)
  EXPECT_GT(endpoint.size(), 7);
  EXPECT_EQ(endpoint.path().size(), 7);
  EXPECT_EQ(endpoint.path(), "/foo/ba");
}

TYPED_TEST(LocalProtocolTest, endpoint_construct_abstract) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  // workaround dev-studio's broken ""s for string-literals
#define S(x) std::string((x), sizeof(x) - 1)
  endpoint_type endpoint(S("\0/foo/bar"));

  // at least the size of the path + sa_family_t
  EXPECT_GT(endpoint.size(), 8);
  EXPECT_EQ(endpoint.path(), S("\0/foo/bar"));
#undef S
}

namespace net {
template <class Protocol>
std::ostream &operator<<(std::ostream &os,
                         const net::basic_socket<Protocol> &sock) {
  os << sock.native_handle();

  return os;
}
}  // namespace net

TEST(NetTS_local, stream_socket_bind_accept_connect) {
  TmpDir tmpdir;

  std::string socket_path = tmpdir.file("stream-protocol.test.socket");

  net::io_context io_ctx;

  local::stream_protocol::endpoint endp(socket_path);

  local::stream_protocol::acceptor acceptor(io_ctx);
  EXPECT_THAT(acceptor.open(endp.protocol()),
              ::testing::Truly([](const auto &t) { return bool(t); }));
  ASSERT_THAT(acceptor.bind(endp),
              ::testing::Truly([](const auto &t) { return bool(t); }));
  EXPECT_THAT(acceptor.listen(128),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  //
  EXPECT_THAT(acceptor.native_non_blocking(true),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  // should fail with EWOULDBLOCK
  EXPECT_EQ(
      acceptor.accept(),
      stdx::make_unexpected(make_error_code(std::errc::operation_would_block)));
  auto local_endp_res = acceptor.local_endpoint();

  ASSERT_TRUE(local_endp_res);

  auto local_endp = std::move(*local_endp_res);

  local::stream_protocol::socket client_sock(io_ctx);
  EXPECT_TRUE(client_sock.open(local_endp.protocol()));

  // ensure the connect() doesn't block
  EXPECT_THAT(client_sock.native_non_blocking(true),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  // even though non-blocking, this is unix-domain-sockets
  ASSERT_THAT(client_sock.connect(local_endp),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  auto server_sock_res = acceptor.accept();
  ASSERT_TRUE(server_sock_res);
  auto server_sock = std::move(*server_sock_res);

  ASSERT_TRUE(server_sock.is_open());

  // finish the non-blocking connect
  net::socket_base::error so_error;
  ASSERT_TRUE(client_sock.get_option(so_error));
  ASSERT_EQ(so_error.value(), 0);

  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  std::array<char, 16> sink;
  EXPECT_EQ(
      net::read(client_sock, net::buffer(sink)),
      stdx::make_unexpected(make_error_code(std::errc::operation_would_block)));

  auto write_res = net::write(server_sock, net::buffer(source));
  ASSERT_TRUE(write_res) << write_res.error();
  EXPECT_EQ(*write_res, source.size());

  auto read_res = net::read(client_sock, net::buffer(sink),
                            net::transfer_at_least(source.size()));
  ASSERT_TRUE(read_res);
  EXPECT_EQ(*read_res, source.size());

  EXPECT_TRUE(server_sock.shutdown(net::socket_base::shutdown_send));
  EXPECT_TRUE(client_sock.shutdown(net::socket_base::shutdown_send));
}

TEST(NetTS_local, datagram_socket_bind_sendmsg_recvmsg) {
  TmpDir tmpdir;

  net::io_context io_ctx;

  std::string server_socket_path = tmpdir.file("datagram-test.socket");
  std::string client_socket_path = tmpdir.file("datagram-test.client.socket");

  local::datagram_protocol::socket server_sock(io_ctx);
  EXPECT_THAT(server_sock.open(),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  local::datagram_protocol::endpoint server_endp(server_socket_path);
  ASSERT_THAT(server_sock.bind(server_endp),
              ::testing::Truly([](const auto &t) { return bool(t); }));
  EXPECT_THAT(server_sock.native_non_blocking(true),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  local::datagram_protocol::socket client_sock(io_ctx);
  EXPECT_TRUE(client_sock.open());

  // ensure the connect() doesn't block
  EXPECT_THAT(client_sock.native_non_blocking(true),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  // UDP over AF_UNIX requires explicit paths as with the abstract namespace
  // we get ENOTCONN on sendmsg()
  local::datagram_protocol::endpoint client_any_endp(client_socket_path);
  ASSERT_THAT(client_sock.bind(client_any_endp),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  auto client_endp_res = client_sock.local_endpoint();
  ASSERT_THAT(client_endp_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));

  auto client_endp = std::move(*client_endp_res);

  SCOPED_TRACE("// up to now, there is no data");
  std::array<char, 16> sink;
  local::datagram_protocol::endpoint recvfrom_endp;
  EXPECT_EQ(
      client_sock.receive_from(net::buffer(sink), recvfrom_endp),
      stdx::make_unexpected(make_error_code(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something");
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  auto write_res = server_sock.send_to(net::buffer(source), client_endp);
  ASSERT_THAT(write_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// and we should receive something");
  auto read_res = client_sock.receive_from(net::buffer(sink), recvfrom_endp);
  ASSERT_THAT(read_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));
  EXPECT_EQ(*read_res, source.size());

  SCOPED_TRACE("// check the sender address matches");
  EXPECT_EQ(recvfrom_endp, server_endp) << recvfrom_endp.size();
}

// check endpoint after recvfrom a socketpair()
TEST(NetTS_local, datagram_socketpair_recvfrom) {
  net::io_context io_ctx;

  using protocol_type = local::datagram_protocol;
  using socket_type = protocol_type::socket;
  using endpoint_type = protocol_type::endpoint;

  socket_type server_sock(io_ctx);
  socket_type client_sock(io_ctx);

  EXPECT_THAT(
      local::connect_pair<protocol_type>(&io_ctx, server_sock, client_sock),
      ::testing::Truly([](const auto &t) { return bool(t); }));

  EXPECT_THAT(server_sock.native_non_blocking(true),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  EXPECT_THAT(client_sock.native_non_blocking(true),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  SCOPED_TRACE("// up to now, there is no data");
  std::array<char, 16> sink;
  endpoint_type recvfrom_endp;
  EXPECT_EQ(
      client_sock.receive_from(net::buffer(sink), recvfrom_endp),
      stdx::make_unexpected(make_error_code(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something");
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  auto write_res = server_sock.send(net::buffer(source));
  ASSERT_THAT(write_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// and we should receive something");
  auto read_res = client_sock.receive_from(net::buffer(sink), recvfrom_endp);
  ASSERT_THAT(read_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));
  EXPECT_EQ(*read_res, source.size());

  // linux: unnamed socket, .size() == 2 (just the AF_UNIX)
  //   see: man 7 unix
  // freebsd: 16
  // macosx: 16

  EXPECT_GT(recvfrom_endp.size(), 0);
}

TYPED_TEST(LocalProtocolTest, socketpair) {
  net::io_context io_ctx;

  using protocol_type = TypeParam;
  using socket_type = typename protocol_type::socket;

  socket_type server_sock(io_ctx);
  socket_type client_sock(io_ctx);

  auto connect_res =
      local::connect_pair<protocol_type>(&io_ctx, server_sock, client_sock);

  // macosx may not support socketpair() with SEQPACKET
  if (!connect_res &&
      connect_res.error() ==
          make_error_condition(std::errc::protocol_not_supported))
    return;

  ASSERT_THAT(connect_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));

  EXPECT_THAT(server_sock.native_non_blocking(true),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  EXPECT_THAT(client_sock.native_non_blocking(true),
              ::testing::Truly([](const auto &t) { return bool(t); }));

  SCOPED_TRACE("// up to now, there is no data");
  std::array<char, 16> sink;
  EXPECT_EQ(
      client_sock.receive(net::buffer(sink)),
      stdx::make_unexpected(make_error_code(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something");
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  auto write_res = server_sock.send(net::buffer(source));
  ASSERT_THAT(write_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// and we should receive something");
  auto read_res = client_sock.receive(net::buffer(sink));
  ASSERT_THAT(read_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));
  EXPECT_EQ(*read_res, source.size());
}

#if defined(__linux__) || defined(__OpenBSD__) || defined(__FreeBSD__) || \
    defined(__APPLE__) || defined(__NetBSD__)
/**
 * test if peer_creds socket opt work.
 */
TYPED_TEST(LocalTwoWayProtocolTest, twoway_peercreds) {
  net::io_context io_ctx;

  using protocol_type = TypeParam;
  using socket_type = typename protocol_type::socket;

  socket_type server_sock(io_ctx);
  socket_type client_sock(io_ctx);

  auto connect_res =
      local::connect_pair<protocol_type>(&io_ctx, server_sock, client_sock);

  // macosx may not support socketpair() with SEQPACKET
  if (!connect_res &&
      connect_res.error() ==
          make_error_condition(std::errc::protocol_not_supported))
    return;

  ASSERT_THAT(connect_res,
              ::testing::Truly([](const auto &t) { return bool(t); }));

  SCOPED_TRACE("// get creds of the remote side of the socket");
  typename protocol_type::peer_creds peer_creds;

  try {
    ASSERT_THAT(client_sock.get_option(peer_creds),
                ::testing::Truly([](const auto &t) { return bool(t); }));
  } catch (const std::exception &e) {
    // resize() currently throws on macsox and freebsd
    FAIL() << e.what();
  }

  SCOPED_TRACE("// expected creds to match ours as it is the same process");
#if defined(__linux__) || defined(__OpenBSD__)
  EXPECT_EQ(peer_creds.value().uid, getuid());
  EXPECT_EQ(peer_creds.value().gid, getgid());
  EXPECT_EQ(peer_creds.value().pid, getpid());
#elif defined(__FreeBSD__) || defined(__APPLE__)
  ASSERT_GE(peer_creds.size(protocol_type()), sizeof(u_int));
  ASSERT_EQ(peer_creds.value().cr_version, XUCRED_VERSION);

  // after get_option() the value may be too small to have those values.
  if (peer_creds.size(protocol_type()) ==
      sizeof(typename protocol_type::peer_creds::value_type)) {
    EXPECT_EQ(peer_creds.value().cr_uid, getuid());

    // no cr.gid, but .cr_ngroups and .cr_groups instead
    // EXPECT_EQ(peer_creds.value().cr_gid, getgid());

    // PID added in r348847 (freebsd13) ...
    // EXPECT_EQ(peer_creds.value().cr_pid, getpid());
  }
#elif defined(__NetBSD__)
  EXPECT_EQ(peer_creds.value().unp_euid, geteuid());
  EXPECT_EQ(peer_creds.value().unp_egid, getegid());
  EXPECT_EQ(peer_creds.value().unp_pid, getpid());
#endif
}

TEST(NetTS_local, socketpair_unsupported_protocol) {
  class UnsupportedProtocol {
   public:
    using socket = net::basic_datagram_socket<UnsupportedProtocol>;
    class endpoint {
     public:
      using protocol_type = UnsupportedProtocol;

      constexpr protocol_type protocol() const noexcept { return {}; }
    };

    constexpr int family() const noexcept { return PF_UNSPEC; }
    constexpr int type() const noexcept { return SOCK_DGRAM; }
    constexpr int protocol() const noexcept { return 0; }
  };
  net::io_context io_ctx;

  using protocol_type = UnsupportedProtocol;
  using socket_type = protocol_type::socket;

  socket_type server_sock(io_ctx);
  socket_type client_sock(io_ctx);

  // other OSes may return other error-codes
  EXPECT_EQ(
      local::connect_pair<protocol_type>(&io_ctx, server_sock, client_sock),
      stdx::make_unexpected(
          make_error_code(std::errc::address_family_not_supported)));
}

#endif

// instances of basic_socket are
// - destructible
// - move-constructible
// - move-assignable

static_assert(std::is_destructible<local::stream_protocol::socket>::value, "");
static_assert(
    !std::is_copy_constructible<local::stream_protocol::socket>::value, "");
static_assert(std::is_move_constructible<local::stream_protocol::socket>::value,
              "");
static_assert(!std::is_copy_assignable<local::stream_protocol::socket>::value,
              "");
static_assert(std::is_move_assignable<local::stream_protocol::socket>::value,
              "");

// check constexpr
static_assert(local::stream_protocol().family() != AF_UNSPEC, "");
static_assert(local::datagram_protocol().family() != AF_UNSPEC, "");
static_assert(local::seqpacket_protocol().family() != AF_UNSPEC, "");

static_assert(local::stream_protocol::endpoint().size() > 0, "");
static_assert(local::stream_protocol::endpoint().capacity() > 0, "");
static_assert(local::datagram_protocol::endpoint().size() > 0, "");
static_assert(local::datagram_protocol::endpoint().capacity() > 0, "");
static_assert(local::seqpacket_protocol::endpoint().size() > 0, "");
static_assert(local::seqpacket_protocol::endpoint().capacity() > 0, "");

// in C++20, this could succeed
// static_assert(local::stream_protocol::endpoint("foo").size() > 0, "");
#endif

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
