/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/net_ts/local.h"

#include <system_error>

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/socket.h"  // net::impl::socket::init
#include "mysql/harness/stdx/expected_ostream.h"
#include "router/tests/helpers/stdx_expected_no_error.h"
#include "test/temp_directory.h"

#ifdef NET_TS_HAS_UNIX_SOCKET

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

TEST(NetTS_local, stream_socket_bind_accept_connect_named) {
  TempDirectory tmpdir;

  std::string socket_path = tmpdir.file("stream-protocol.test.socket");

  net::io_context io_ctx;

  local::stream_protocol::endpoint endp(socket_path);

  local::stream_protocol::acceptor acceptor(io_ctx);
  auto open_res = acceptor.open(endp.protocol());
  if (!open_res) {
    auto ec = open_res.error();
    // macos may not support socketpair() with SEQPACKET
    // windows may not support socketpair() at all
    ASSERT_THAT(
        ec, ::testing::AnyOf(
                make_error_condition(std::errc::protocol_not_supported),
                make_error_condition(std::errc::address_family_not_supported),
                std::error_code(10044,
                                std::system_category())  // WSAESOCKTNOSUPPORT
                ));
    GTEST_SKIP() << ec;
  }

  ASSERT_NO_ERROR(acceptor.bind(endp));
  EXPECT_NO_ERROR(acceptor.listen(128));

  //
  EXPECT_NO_ERROR(acceptor.native_non_blocking(true));

  // should fail with EWOULDBLOCK
  EXPECT_EQ(
      acceptor.accept(),
      stdx::unexpected(make_error_condition(std::errc::operation_would_block)));
  auto local_endp_res = acceptor.local_endpoint();

  ASSERT_NO_ERROR(local_endp_res);

  auto local_endp = std::move(*local_endp_res);

  local::stream_protocol::socket client_sock(io_ctx);
  EXPECT_NO_ERROR(client_sock.open(local_endp.protocol()));

  EXPECT_NO_ERROR(client_sock.native_non_blocking(true));

  auto connect_res = client_sock.connect(local_endp);
  if (!connect_res) {
    ASSERT_EQ(connect_res.error(),
              make_error_condition(std::errc::operation_would_block));
  }

  auto server_sock_res = acceptor.accept();
  ASSERT_NO_ERROR(server_sock_res);
  auto server_sock = std::move(*server_sock_res);

  ASSERT_TRUE(server_sock.is_open());

  if (!connect_res) {
    // finish the non-blocking connect
    net::socket_base::error so_error;
    ASSERT_NO_ERROR(client_sock.get_option(so_error));
    ASSERT_EQ(so_error.value(), 0);
  }

  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  std::array<char, 16> sink;
  EXPECT_EQ(
      net::read(client_sock, net::buffer(sink)),
      stdx::unexpected(make_error_condition(std::errc::operation_would_block)));

  auto write_res = net::write(server_sock, net::buffer(source));
  ASSERT_NO_ERROR(write_res) << write_res.error();
  EXPECT_EQ(*write_res, source.size());

  auto read_res = net::read(client_sock, net::buffer(sink),
                            net::transfer_at_least(source.size()));
  ASSERT_NO_ERROR(read_res);
  EXPECT_EQ(*read_res, source.size());

  EXPECT_TRUE(server_sock.shutdown(net::socket_base::shutdown_send));
  EXPECT_TRUE(client_sock.shutdown(net::socket_base::shutdown_send));
}

TEST(NetTS_local, stream_socket_bind_accept_connect_abstract) {
  net::io_context io_ctx;

#define S(x) std::string((x), sizeof(x) - 1)
  local::stream_protocol::endpoint endp(S("\0foo"));
#undef S

  local::stream_protocol::acceptor acceptor(io_ctx);
  auto open_res = acceptor.open(endp.protocol());
  if (!open_res) {
    auto ec = open_res.error();
    // macos may not support socketpair() with SEQPACKET
    // windows may not support socketpair() at all
    ASSERT_THAT(
        ec, ::testing::AnyOf(
                make_error_condition(std::errc::protocol_not_supported),
                make_error_condition(std::errc::address_family_not_supported),
                std::error_code(10044,
                                std::system_category())  // WSAESOCKTNOSUPPORT
                ));
    GTEST_SKIP() << ec;
  }

  auto bind_res = acceptor.bind(endp);
  if (!bind_res) {
    auto ec = bind_res.error();
    // macos doesn't support abstract paths and will return ENOENT.
    ASSERT_THAT(ec, ::testing::AnyOf(make_error_condition(
                        std::errc::no_such_file_or_directory)));
    GTEST_SKIP() << ec;
  }
  EXPECT_NO_ERROR(acceptor.listen(128));

  //
  EXPECT_NO_ERROR(acceptor.native_non_blocking(true));

  // should fail with EWOULDBLOCK
  EXPECT_EQ(
      acceptor.accept(),
      stdx::unexpected(make_error_condition(std::errc::operation_would_block)));

  const auto local_endp_res = acceptor.local_endpoint();
  ASSERT_NO_ERROR(local_endp_res);

  const auto local_endp = *local_endp_res;

  local::stream_protocol::socket client_sock(io_ctx);
  EXPECT_NO_ERROR(client_sock.open(local_endp.protocol()));

  // ensure the connect() doesn't block
  EXPECT_NO_ERROR(client_sock.native_non_blocking(true));

  const auto connect_res = client_sock.connect(local_endp);
  if (!connect_res) {
    const auto ec = connect_res.error();

    if (ec == make_error_condition(std::errc::invalid_argument)) {
      // windows doesn't support autobind and returns WSAEINVAL
      GTEST_SKIP() << ec;
    }

    ASSERT_EQ(ec, make_error_condition(std::errc::operation_would_block));
  }

  auto server_sock_res = acceptor.accept();
  ASSERT_NO_ERROR(server_sock_res);
  auto server_sock = std::move(*server_sock_res);

  ASSERT_TRUE(server_sock.is_open());

  if (!connect_res) {
    // finish the non-blocking connect
    net::socket_base::error so_error;
    ASSERT_NO_ERROR(client_sock.get_option(so_error));
    ASSERT_EQ(so_error.value(), 0);
  }

  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  std::array<char, 16> sink;
  EXPECT_EQ(
      net::read(client_sock, net::buffer(sink)),
      stdx::unexpected(make_error_condition(std::errc::operation_would_block)));

  auto write_res = net::write(server_sock, net::buffer(source));
  ASSERT_NO_ERROR(write_res) << write_res.error();
  EXPECT_EQ(*write_res, source.size());

  auto read_res = net::read(client_sock, net::buffer(sink),
                            net::transfer_at_least(source.size()));
  ASSERT_NO_ERROR(read_res);
  EXPECT_EQ(*read_res, source.size());

  EXPECT_TRUE(server_sock.shutdown(net::socket_base::shutdown_send));
  EXPECT_TRUE(client_sock.shutdown(net::socket_base::shutdown_send));
}

TEST(NetTS_local, stream_socket_bind_accept_connect_autobind) {
  net::io_context io_ctx;

  local::stream_protocol::endpoint endp;

  local::stream_protocol::acceptor acceptor(io_ctx);
  auto open_res = acceptor.open(endp.protocol());
  if (!open_res) {
    auto ec = open_res.error();
    // macos may not support socketpair() with SEQPACKET
    // windows may not support socketpair() at all
    ASSERT_THAT(
        ec, ::testing::AnyOf(
                make_error_condition(std::errc::protocol_not_supported),
                make_error_condition(std::errc::address_family_not_supported),
                std::error_code(10044,
                                std::system_category())  // WSAESOCKTNOSUPPORT
                ));
    GTEST_SKIP() << ec;
  }

  auto bind_res = acceptor.bind(endp);
  if (!bind_res) {
    auto ec = bind_res.error();
    // macos doesn't support autobind and will return EINVAL.
    // solaris returns EISDIR
    ASSERT_THAT(
        ec, ::testing::AnyOf(make_error_condition(std::errc::invalid_argument),
                             make_error_condition(std::errc::is_a_directory)));
    GTEST_SKIP() << ec;
  }
  EXPECT_NO_ERROR(acceptor.listen(128));

  //
  EXPECT_NO_ERROR(acceptor.native_non_blocking(true));

  // should fail with EWOULDBLOCK
  EXPECT_EQ(
      acceptor.accept(),
      stdx::unexpected(make_error_condition(std::errc::operation_would_block)));
  auto local_endp_res = acceptor.local_endpoint();
  ASSERT_NO_ERROR(local_endp_res);

  auto local_endp = std::move(*local_endp_res);

  // Linux does \0 + 5 bytes.
  // Windows does 108x \0
  EXPECT_GT(local_endp.path().size(), 1);

  local::stream_protocol::socket client_sock(io_ctx);
  EXPECT_NO_ERROR(client_sock.open(local_endp.protocol()));

  EXPECT_NO_ERROR(client_sock.native_non_blocking(true));

  auto connect_res = client_sock.connect(local_endp);
  if (!connect_res) {
    auto ec = connect_res.error();

    if (ec == make_error_condition(std::errc::invalid_argument)) {
      // windows doesn't support autobind and returns WSAEINVAL
      GTEST_SKIP() << ec;
    }

    ASSERT_EQ(ec, make_error_condition(std::errc::operation_would_block));
  }

  auto server_sock_res = acceptor.accept();
  ASSERT_NO_ERROR(server_sock_res);
  auto server_sock = std::move(*server_sock_res);

  ASSERT_TRUE(server_sock.is_open());

  if (!connect_res) {
    // finish the non-blocking connect
    net::socket_base::error so_error;
    ASSERT_NO_ERROR(client_sock.get_option(so_error));
    ASSERT_EQ(so_error.value(), 0);
  }

  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  std::array<char, 16> sink;
  EXPECT_EQ(
      net::read(client_sock, net::buffer(sink)),
      stdx::unexpected(make_error_condition(std::errc::operation_would_block)));

  auto write_res = net::write(server_sock, net::buffer(source));
  ASSERT_NO_ERROR(write_res) << write_res.error();
  EXPECT_EQ(*write_res, source.size());

  auto read_res = net::read(client_sock, net::buffer(sink),
                            net::transfer_at_least(source.size()));
  ASSERT_NO_ERROR(read_res);
  EXPECT_EQ(*read_res, source.size());

  EXPECT_TRUE(server_sock.shutdown(net::socket_base::shutdown_send));
  EXPECT_TRUE(client_sock.shutdown(net::socket_base::shutdown_send));
}

TEST(NetTS_local, datagram_socket_bind_sendmsg_recvmsg) {
  TempDirectory tmpdir;

  net::io_context io_ctx;

  std::string server_socket_path = tmpdir.file("datagram-test.socket");
  std::string client_socket_path = tmpdir.file("datagram-test.client.socket");

  local::datagram_protocol::socket server_sock(io_ctx);
  auto open_res = server_sock.open();
  if (!open_res) {
    auto ec = open_res.error();
    // macos may not support socketpair() with SEQPACKET
    // windows may not support socketpair() at all
    ASSERT_THAT(
        ec, ::testing::AnyOf(
                make_error_condition(std::errc::protocol_not_supported),
                make_error_condition(std::errc::address_family_not_supported)));
    GTEST_SKIP() << ec;
  }

  local::datagram_protocol::endpoint server_endp(server_socket_path);
  ASSERT_NO_ERROR(server_sock.bind(server_endp));
  EXPECT_NO_ERROR(server_sock.native_non_blocking(true));

  local::datagram_protocol::socket client_sock(io_ctx);
  EXPECT_TRUE(client_sock.open());

  // ensure the connect() doesn't block
  EXPECT_NO_ERROR(client_sock.native_non_blocking(true));

  // UDP over AF_UNIX requires explicit paths as with the abstract namespace
  // we get ENOTCONN on sendmsg()
  local::datagram_protocol::endpoint client_any_endp(client_socket_path);
  ASSERT_NO_ERROR(client_sock.bind(client_any_endp));

  auto client_endp_res = client_sock.local_endpoint();
  ASSERT_NO_ERROR(client_endp_res);

  auto client_endp = std::move(*client_endp_res);

  SCOPED_TRACE("// up to now, there is no data");
  std::array<char, 16> sink;
  local::datagram_protocol::endpoint recvfrom_endp;
  EXPECT_EQ(
      client_sock.receive_from(net::buffer(sink), recvfrom_endp),
      stdx::unexpected(make_error_code(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something");
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  auto write_res = server_sock.send_to(net::buffer(source), client_endp);
  ASSERT_NO_ERROR(write_res);
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// and we should receive something");
  auto read_res = client_sock.receive_from(net::buffer(sink), recvfrom_endp);
  ASSERT_NO_ERROR(read_res);
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

  auto open_res =
      local::connect_pair<protocol_type>(&io_ctx, server_sock, client_sock);
  if (!open_res) {
    auto ec = open_res.error();
    // macos may not support socketpair() with SEQPACKET
    // windows may not support socketpair() at all
    ASSERT_THAT(
        ec, ::testing::AnyOf(
                make_error_condition(std::errc::protocol_not_supported),
                make_error_condition(std::errc::address_family_not_supported)));
    GTEST_SKIP() << ec;
  }

  EXPECT_NO_ERROR(server_sock.native_non_blocking(true));

  EXPECT_NO_ERROR(client_sock.native_non_blocking(true));

  SCOPED_TRACE("// up to now, there is no data");
  std::array<char, 16> sink;
  endpoint_type recvfrom_endp;
  EXPECT_EQ(
      client_sock.receive_from(net::buffer(sink), recvfrom_endp),
      stdx::unexpected(make_error_code(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something");
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  auto write_res = server_sock.send(net::buffer(source));
  ASSERT_NO_ERROR(write_res);
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// and we should receive something");
  auto read_res = client_sock.receive_from(net::buffer(sink), recvfrom_endp);
  ASSERT_NO_ERROR(read_res);
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

  if (!connect_res) {
    auto ec = connect_res.error();
    // macos may not support socketpair() with SEQPACKET
    // windows may not support socketpair() at all
    ASSERT_THAT(
        ec, ::testing::AnyOf(
                make_error_condition(std::errc::protocol_not_supported),
                make_error_condition(std::errc::address_family_not_supported),
                std::error_code(10044,
                                std::system_category())  // WSAESOCKTNOSUPPORT
                ));
    GTEST_SKIP() << ec;
  }

  ASSERT_NO_ERROR(connect_res);

  EXPECT_NO_ERROR(server_sock.native_non_blocking(true));

  EXPECT_NO_ERROR(client_sock.native_non_blocking(true));

  SCOPED_TRACE("// up to now, there is no data");
  std::array<char, 16> sink;
  EXPECT_EQ(
      client_sock.receive(net::buffer(sink)),
      stdx::unexpected(make_error_condition(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something");
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  auto write_res = server_sock.send(net::buffer(source));
  ASSERT_NO_ERROR(write_res);
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// and we should receive something");
  auto read_res = client_sock.receive(net::buffer(sink));
  ASSERT_NO_ERROR(read_res);
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

  ASSERT_NO_ERROR(connect_res);

  SCOPED_TRACE("// get creds of the remote side of the socket");
  typename protocol_type::peer_creds peer_creds;

  try {
    ASSERT_NO_ERROR(client_sock.get_option(peer_creds));
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
      stdx::unexpected(
          make_error_code(std::errc::address_family_not_supported)));
}

#endif

// instances of basic_socket are
// - destructible
// - move-constructible
// - move-assignable

static_assert(std::is_destructible<local::stream_protocol::socket>::value);
static_assert(
    !std::is_copy_constructible<local::stream_protocol::socket>::value);
static_assert(
    std::is_move_constructible<local::stream_protocol::socket>::value);
static_assert(!std::is_copy_assignable<local::stream_protocol::socket>::value);
static_assert(std::is_move_assignable<local::stream_protocol::socket>::value);

// check constexpr
static_assert(local::stream_protocol().family() != AF_UNSPEC);
static_assert(local::datagram_protocol().family() != AF_UNSPEC);
static_assert(local::seqpacket_protocol().family() != AF_UNSPEC);

static_assert(local::stream_protocol::endpoint().size() > 0);
static_assert(local::stream_protocol::endpoint().capacity() > 0);
static_assert(local::datagram_protocol::endpoint().size() > 0);
static_assert(local::datagram_protocol::endpoint().capacity() > 0);
static_assert(local::seqpacket_protocol::endpoint().size() > 0);
static_assert(local::seqpacket_protocol::endpoint().capacity() > 0);

// in C++20, this could succeed
// static_assert(local::stream_protocol::endpoint("foo").size() > 0);
#endif

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
