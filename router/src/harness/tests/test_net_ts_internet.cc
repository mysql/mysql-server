/*
  Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/net_ts/internet.h"

#include <gmock/gmock.h>

#include <csignal>  // signal

#include "mysql/harness/stdx/expected_ostream.h"

// helper to be used with ::testing::Truly to check if a std::expected<> has a
// value and triggering the proper printer used in case of failure
static auto res_has_value = [](const auto &t) { return bool(t); };

namespace std {

std::ostream &operator<<(std::ostream &os, const std::error_condition &cond) {
  // std::error_condition has no operator<< for ostream, only std::error_code
  // has but here we need it for EXPECT-printer-on-failure

  os << cond.message();

  return os;
}
}  // namespace std

// default constructed address is ipv4-any
TEST(NetTS_internet, address_construct_default) {
  net::ip::address addr;

  EXPECT_FALSE(addr.is_loopback());
  EXPECT_TRUE(addr.is_unspecified());
  EXPECT_FALSE(addr.is_multicast());
  EXPECT_TRUE(addr.is_v4());
  EXPECT_FALSE(addr.is_v6());
}

TEST(NetTS_internet, address_v4_construct_default) {
  net::ip::address_v4 addr;

  EXPECT_FALSE(addr.is_loopback());
  EXPECT_TRUE(addr.is_unspecified());
  EXPECT_FALSE(addr.is_multicast());
}

TEST(NetTS_internet, address_v6_construct_default) {
  net::ip::address_v6 addr;

  EXPECT_FALSE(addr.is_loopback());
  EXPECT_TRUE(addr.is_unspecified());
  EXPECT_FALSE(addr.is_multicast());
}

TEST(NetTS_internet, address_v4_to_string) {
  net::ip::address_v4 addr;

  EXPECT_EQ(addr.to_string(), "0.0.0.0");

  addr = net::ip::address_v4::loopback();

  EXPECT_EQ(addr.to_string(), "127.0.0.1");
}

TEST(NetTS_internet, address_comp_v4_lt_v6) {
  constexpr net::ip::address a4(net::ip::address_v4{});
  constexpr net::ip::address a6(net::ip::address_v6{});

  static_assert(a4 < a6, "");

  EXPECT_LT(a4, a6);
}

TEST(NetTS_internet, address_comp_v4_eq) {
  constexpr net::ip::address a_1(net::ip::address_v4{});
  constexpr net::ip::address a_2(net::ip::address_v4{});

  static_assert(a_1 == a_2, "");

  EXPECT_EQ(a_1, a_2);
}

TEST(NetTS_internet, address_comp_v4_ne) {
  constexpr net::ip::address a_1(net::ip::address_v4{});
  constexpr net::ip::address a_2(net::ip::address_v4{}.loopback());

  static_assert(a_1 != a_2, "");
  static_assert(a_1 < a_2, "");

  EXPECT_NE(a_1, a_2);
  EXPECT_LT(a_1, a_2);
}

TEST(NetTS_internet, address_comp_v6_eq) {
  constexpr net::ip::address a_1(net::ip::address_v6{});
  constexpr net::ip::address a_2(net::ip::address_v6{});

  static_assert(a_1 == a_2, "");

  EXPECT_EQ(a_1, a_2);
}

TEST(NetTS_internet, address_comp_v6_ne) {
  constexpr net::ip::address a_1(net::ip::address_v6{});
  constexpr net::ip::address a_2(net::ip::address_v6{}.loopback());

  static_assert(a_1 != a_2, "");
  static_assert(a_1 < a_2, "");

  EXPECT_NE(a_1, a_2);
  EXPECT_LT(a_1, a_2);
}

TEST(NetTS_internet, tcp_socket_default_construct) {
  net::io_context io_ctx;
  net::ip::tcp::socket sock(io_ctx);
}

TEST(NetTS_internet, tcp_endpoint_default_construct) {
  net::ip::tcp::endpoint endpoint;

  EXPECT_EQ(endpoint.address(), net::ip::address());
  EXPECT_EQ(endpoint.port(), 0);
}

TEST(NetTS_internet, tcp_endpoint_from_address_port) {
  net::ip::tcp::endpoint endpoint(net::ip::address_v4::loopback(), 12345);

  EXPECT_EQ(endpoint.address(), net::ip::address_v4::loopback());
  EXPECT_EQ(endpoint.port(), 12345);
}

TEST(NetTS_internet, network_v4_default_construct) {
  net::ip::network_v4 net;

  EXPECT_FALSE(net.is_host());
  EXPECT_EQ(net.to_string(), "0.0.0.0/0");
}

TEST(NetTS_internet, network_v4_with_prefix) {
  auto addr_res = net::ip::make_address("127.0.0.1");
  ASSERT_THAT(addr_res, ::testing::Truly(res_has_value));

  auto addr = std::move(*addr_res);
  ASSERT_TRUE(addr.is_v4());
  net::ip::network_v4 net(addr.to_v4(), 32);

  EXPECT_TRUE(net.is_host());
  EXPECT_EQ(net.to_string(), "127.0.0.1/32");
}

TEST(NetTS_internet, network_v4_invalid) {
  ASSERT_EQ(
      net::ip::make_address("127.0.0."),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("127.0.0.1."),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("127.0.0,1"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("256.0.0.1"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
}

TEST(NetTS_internet, network_v6_default_construct) {
  net::ip::network_v6 net;

  EXPECT_FALSE(net.is_host());
  EXPECT_EQ(net.to_string(), "::/0");
}

TEST(NetTS_internet, network_v6_with_prefix) {
  auto addr_res = net::ip::make_address("::1");
  ASSERT_THAT(addr_res, ::testing::Truly(res_has_value));

  auto addr = std::move(*addr_res);
  ASSERT_TRUE(addr.is_v6());
  net::ip::network_v6 net(addr.to_v6(), 128);

  EXPECT_TRUE(net.is_host());
  EXPECT_EQ(net.to_string(), "::1/128");
}

TEST(NetTS_internet, network_v6_with_prefix_and_scope_id) {
  auto addr_res = net::ip::make_address("::1%1");
  ASSERT_THAT(addr_res, ::testing::Truly(res_has_value));

  auto addr = std::move(*addr_res);
  ASSERT_TRUE(addr.is_v6());
  ASSERT_EQ(addr.to_v6().scope_id(), 1);
  net::ip::network_v6 net(addr.to_v6(), 128);

  EXPECT_TRUE(net.is_host());
  EXPECT_EQ(net.to_string(), "::1%1/128");
}

TEST(NetTS_internet, make_address_v6_invalid) {
  ASSERT_EQ(
      net::ip::make_address("zzz"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1::2"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1%-1"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1%+1"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1%abc"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1%"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
}

TEST(NetTS_internet, tcp_resolver) {
  net::io_context io_ctx;
  net::ip::tcp::resolver resolver(io_ctx);

  auto resolve_res = resolver.resolve("localhost", "3306");

  // we can't use the ASSERT_TRUE() on resolve_res here as there is no printer
  // for resolve_res
  ASSERT_TRUE(resolve_res.has_value()) << resolve_res.error();

  auto resolved = std::move(*resolve_res);
  ASSERT_GT(resolved.size(), 0);

#if 0
  for (const auto &e : resolved) {
    std::cerr << e.endpoint() << std::endl;
  }
#endif
}

TEST(NetTS_internet, tcp_resolver_reverse) {
  auto addr_res = net::ip::make_address("127.0.0.1");
  ASSERT_THAT(addr_res, ::testing::Truly(res_has_value));
  auto addr = std::move(*addr_res);

  net::io_context io_ctx;
  net::ip::tcp::resolver resolver(io_ctx);

  auto resolve_res = resolver.resolve(net::ip::tcp::endpoint(addr, 3306));

  // we can't use the ASSERT_TRUE() on resolve_res here as there is no printer
  // for resolve_res
  ASSERT_TRUE(resolve_res.has_value()) << resolve_res.error();

  auto resolved = std::move(*resolve_res);
  ASSERT_GT(resolved.size(), 0);
#if 0
  for (const auto &e : resolved) {
    std::cerr << e.host_name() << std::endl;
  }
#endif
}

TEST(NetTS_internet, udp_resolver) {
  net::io_context io_ctx;
  net::ip::udp::resolver resolver(io_ctx);

  auto resolve_res = resolver.resolve("localhost", "22");
  ASSERT_TRUE(resolve_res.has_value()) << resolve_res.error();

  auto resolved = std::move(*resolve_res);
  ASSERT_GT(resolved.size(), 0);

#if 0
  for (const auto &e : resolved) {
    std::cerr << e.endpoint() << std::endl;
  }
#endif
}

namespace net {
template <class Protocol>
std::ostream &operator<<(std::ostream &os,
                         const net::basic_socket<Protocol> &sock) {
  os << sock.native_handle();

  return os;
}
}  // namespace net

// to_string() for classes that only can write to a ostream
template <class T>
static std::string ss_to_string(const T &t) {
  std::stringstream ss;

  ss << t;

  return ss.str();
}

TEST(NetTS_internet, tcp_ipv4_socket_bind_accept_connect) {
  net::io_context io_ctx;

  // localhost, any port
  net::ip::tcp::endpoint endp(net::ip::address_v4().loopback(), 0);

  net::ip::tcp::acceptor acceptor(io_ctx);
  EXPECT_THAT(acceptor.open(endp.protocol()), ::testing::Truly(res_has_value));
  EXPECT_THAT(acceptor.bind(endp), ::testing::Truly(res_has_value));
  EXPECT_THAT(acceptor.listen(128), ::testing::Truly(res_has_value));

  //
  EXPECT_THAT(acceptor.native_non_blocking(true),
              ::testing::Truly(res_has_value));

  // should fail with EWOULDBLOCK as nothing connect()ed yet
  EXPECT_EQ(acceptor.accept(), stdx::make_unexpected(make_error_condition(
                                   std::errc::operation_would_block)));
  auto local_endp_res = acceptor.local_endpoint();

  ASSERT_TRUE(local_endp_res) << local_endp_res.error();

  auto local_endp = std::move(*local_endp_res);

  net::ip::tcp::socket client_sock(io_ctx);
  EXPECT_TRUE(client_sock.open(local_endp.protocol()));

  // ensure the connect() doesn't block
  EXPECT_THAT(client_sock.native_non_blocking(true),
              ::testing::Truly(res_has_value));

  // it may succeed directly, or fail with in_progress due to non-blocking io
  SCOPED_TRACE("// connecting to " + ss_to_string(local_endp));
  auto connect_res = client_sock.connect(local_endp);
  if (!connect_res) {
    ASSERT_THAT(
        connect_res.error(),
        ::testing::AnyOf(make_error_condition(std::errc::operation_would_block),
                         make_error_code(std::errc::operation_in_progress)));
  }

  acceptor.wait(net::socket_base::wait_read);

  auto server_sock_res = acceptor.accept();
  ASSERT_THAT(server_sock_res, ::testing::Truly(res_has_value));
  auto server_sock = std::move(*server_sock_res);

  if (!connect_res) {
    client_sock.wait(net::socket_base::wait_write);

    // finish the non-blocking connect
    net::socket_base::error so_error;
    ASSERT_TRUE(client_sock.get_option(so_error));
    ASSERT_EQ(so_error.value(), 0);
  }

  SCOPED_TRACE("// nothing written, read failed with with would block");
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  std::array<char, 16> sink;
  EXPECT_EQ(net::read(client_sock, net::buffer(sink)),
            stdx::make_unexpected(
                make_error_condition(std::errc::operation_would_block)));

  SCOPED_TRACE("// writing");
  auto write_res = net::write(server_sock, net::buffer(source));
  ASSERT_THAT(write_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// wait for socket to become readable");
  client_sock.wait(net::socket_base::wait_read);

  SCOPED_TRACE("// reading");
  auto read_res = net::read(client_sock, net::buffer(sink),
                            net::transfer_at_least(source.size()));
  ASSERT_THAT(read_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(*read_res, source.size());

  SCOPED_TRACE("// shutting down");
  EXPECT_TRUE(server_sock.shutdown(net::socket_base::shutdown_send));
  EXPECT_TRUE(server_sock.shutdown(net::socket_base::shutdown_receive));
  EXPECT_TRUE(client_sock.shutdown(net::socket_base::shutdown_send));

  SCOPED_TRACE("// read from shutdown socket");
  // even though the socket is shutdown, the following read() may lead
  // to errc::operator_would_block. Better wait.
  client_sock.wait(net::socket_base::wait_read);

  read_res = net::read(client_sock, net::buffer(sink),
                       net::transfer_at_least(source.size()));
  ASSERT_THAT(read_res, ::testing::Not(::testing::Truly(res_has_value)));
  EXPECT_EQ(read_res.error(), make_error_code(net::stream_errc::eof));

  SCOPED_TRACE("// send to shutdown socket");
  read_res = net::write(client_sock, net::buffer(sink),
                        net::transfer_at_least(source.size()));
  ASSERT_THAT(read_res, ::testing::Not(::testing::Truly(res_has_value)));
  EXPECT_THAT(
      read_res.error(),
      ::testing::AnyOf(
          make_error_code(net::stream_errc::eof),
          make_error_condition(std::errc::broken_pipe),       // linux
          make_error_condition(std::errc::connection_reset),  // wine
          net::impl::socket::make_error_code(10058)  // windows: WSAESHUTDOWN
          ));
}

TEST(NetTS_internet, udp_ipv4_socket_bind_sendmsg_recvmsg) {
  net::io_context io_ctx;

  // any ip, any port
  net::ip::udp::endpoint endp;

  net::ip::udp::socket server_sock(io_ctx);
  EXPECT_THAT(server_sock.open(endp.protocol()),
              ::testing::Truly(res_has_value));
  EXPECT_THAT(server_sock.bind(endp), ::testing::Truly(res_has_value));
  EXPECT_THAT(server_sock.native_non_blocking(true),
              ::testing::Truly(res_has_value));

  auto local_endp_res = server_sock.local_endpoint();
  ASSERT_TRUE(local_endp_res);

  // the .local_endpoint() returns the any() + real-port.
  // we need to overwrite the address port with loopback-address
  net::ip::udp::endpoint server_endp(net::ip::address_v4().loopback(),
                                     std::move(*local_endp_res).port());
  net::ip::udp::socket client_sock(io_ctx);
  EXPECT_TRUE(client_sock.open(server_endp.protocol()));

  // ensure the connect() doesn't block
  EXPECT_THAT(client_sock.native_non_blocking(true),
              ::testing::Truly(res_has_value));

  // bind to localhost, any-port
  net::ip::udp::endpoint client_any_endp(net::ip::address_v4::loopback(), 0);
  EXPECT_THAT(client_sock.bind(client_any_endp),
              ::testing::Truly(res_has_value));

  auto client_endp_res = client_sock.local_endpoint();
  ASSERT_TRUE(client_endp_res);

  auto client_endp = std::move(*client_endp_res);

  SCOPED_TRACE("// up to now, there is no data");
  std::array<char, 16> sink;
  net::ip::udp::endpoint recvfrom_endp;
  EXPECT_EQ(client_sock.receive_from(net::buffer(sink), recvfrom_endp),
            stdx::make_unexpected(
                make_error_condition(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something to " + ss_to_string(client_endp));
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  auto write_res = server_sock.send_to(net::buffer(source), client_endp);

  ASSERT_THAT(write_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(*write_res, source.size());

  client_sock.wait(net::socket_base::wait_read);

  SCOPED_TRACE("// and we should receive something");
  auto read_res = client_sock.receive_from(net::buffer(sink), recvfrom_endp);
  ASSERT_THAT(read_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(*read_res, source.size());

  SCOPED_TRACE("// check the sender address matches");
  EXPECT_EQ(recvfrom_endp, server_endp);
}

TEST(NetTS_internet, tcp_ipv6_socket_bind_accept_connect) {
  net::io_context io_ctx;

  // the test relies on bind(addr, port=0) assigns are random port.
  //
  // addr must be either "::" or "::1" depending on OS:
  //
  // - "::1" fails to bind() randomly on FreeBSD
  // - "::" fails to connect() on Windows()

  net::ip::tcp::endpoint endp(
#if defined(__linux__) || defined(__FreeBSD__)
      net::ip::address_v6().any()
#else
      net::ip::address_v6().loopback()
#endif
          ,
      0);

  net::ip::tcp::acceptor acceptor(io_ctx);
  EXPECT_THAT(acceptor.open(endp.protocol()), ::testing::Truly(res_has_value));

  auto bind_res = acceptor.bind(endp);
  if (!bind_res) {
    // if we can't bind the IP-address, because the OS doesn't support IPv6,
    // skip the test

    // check we get the right error.
    ASSERT_EQ(bind_res.error(),
              make_error_condition(std::errc::address_not_available))
        << ss_to_string(endp);

    // leave, if we couldn't bind.
    return;
  }
  EXPECT_THAT(acceptor.listen(128), ::testing::Truly(res_has_value));

  //
  EXPECT_THAT(acceptor.native_non_blocking(true),
              ::testing::Truly(res_has_value));

  // should fail with EWOULDBLOCK
  EXPECT_EQ(acceptor.accept(), stdx::make_unexpected(make_error_condition(
                                   std::errc::operation_would_block)));
  auto local_endp_res = acceptor.local_endpoint();

  ASSERT_TRUE(local_endp_res);

  auto local_endp = std::move(*local_endp_res);

  net::ip::tcp::socket client_sock(io_ctx);
  EXPECT_TRUE(client_sock.open(local_endp.protocol()));

  // ensure the connect() doesn't block
  EXPECT_THAT(client_sock.native_non_blocking(true),
              ::testing::Truly(res_has_value));

  // should fail with EINPROGRESS or succeed
  auto connect_res = client_sock.connect(local_endp);
  if (!connect_res) {
    ASSERT_THAT(connect_res.error(),
                ::testing::AnyOf(
                    make_error_condition(std::errc::operation_in_progress),
                    make_error_condition(std::errc::operation_would_block)));
  }

  acceptor.wait(net::socket_base::wait_read);

  auto server_sock_res = acceptor.accept();
  ASSERT_THAT(server_sock_res, ::testing::Truly(res_has_value));
  auto server_sock = std::move(*server_sock_res);

  // finish the non-blocking connect
  net::socket_base::error so_error;
  ASSERT_TRUE(client_sock.get_option(so_error));
  ASSERT_EQ(so_error.value(), 0);

  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  std::array<char, 16> sink;
  EXPECT_EQ(net::read(client_sock, net::buffer(sink)),
            stdx::make_unexpected(
                make_error_condition(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something");
  auto write_res = net::write(server_sock, net::buffer(source));
  ASSERT_THAT(write_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// and we should receive something");
  client_sock.wait(net::socket_base::wait_read);
  auto read_res = net::read(client_sock, net::buffer(sink),
                            net::transfer_at_least(source.size()));

  ASSERT_THAT(read_res, ::testing::Truly(res_has_value));

  EXPECT_EQ(*read_res, source.size());

  EXPECT_TRUE(server_sock.shutdown(net::socket_base::shutdown_send));
  EXPECT_TRUE(client_sock.shutdown(net::socket_base::shutdown_send));
}

TEST(NetTS_internet, udp_ipv6_socket_bind_accept_connect) {
  net::io_context io_ctx;

  // any ip, any port
  net::ip::udp::endpoint endp(net::ip::address_v6::any(), 0);

  net::ip::udp::socket server_sock(io_ctx);
  EXPECT_THAT(server_sock.open(endp.protocol()),
              ::testing::Truly(res_has_value));
  EXPECT_THAT(server_sock.bind(endp), ::testing::Truly(res_has_value));
  EXPECT_THAT(server_sock.native_non_blocking(true),
              ::testing::Truly(res_has_value));

  auto local_endp_res = server_sock.local_endpoint();
  ASSERT_THAT(local_endp_res, ::testing::Truly(res_has_value));

  // the .local_endpoint() returns the any() + real-port.
  // we need to overwrite the address port with loopback-address
  net::ip::udp::endpoint server_endp(net::ip::address_v6::loopback(),
                                     std::move(*local_endp_res).port());

  // bind to loopback, any-port
  net::ip::udp::endpoint client_any_endp(net::ip::address_v6::loopback(), 0);
  net::ip::udp::socket client_sock(io_ctx);
  EXPECT_TRUE(client_sock.open(client_any_endp.protocol()));

  // ensure the connect() doesn't block
  EXPECT_THAT(client_sock.native_non_blocking(true),
              ::testing::Truly(res_has_value));

  // freebsd: generic:49
  SCOPED_TRACE("// bind() to " + ss_to_string(client_any_endp));
  ASSERT_THAT(client_sock.bind(client_any_endp),
              ::testing::Truly(res_has_value));
  auto client_endp_res = client_sock.local_endpoint();
  ASSERT_TRUE(client_endp_res);

  auto client_endp = std::move(*client_endp_res);

  SCOPED_TRACE("// up to now, there is no data");
  std::array<char, 16> sink;
  net::ip::udp::endpoint recvfrom_endp;
  EXPECT_EQ(client_sock.receive_from(net::buffer(sink), recvfrom_endp),
            stdx::make_unexpected(
                make_error_condition(std::errc::operation_would_block)));

  SCOPED_TRACE("// send something to " + ss_to_string(client_endp));
  std::array<char, 5> source{{0x01, 0x02, 0x03, 0x04, 0x05}};
  auto write_res = server_sock.send_to(net::buffer(source), client_endp);
  ASSERT_THAT(write_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(*write_res, source.size());

  SCOPED_TRACE("// and we should receive something");
  client_sock.wait(net::socket_base::wait_read);
  auto read_res = client_sock.receive_from(net::buffer(sink), recvfrom_endp);
  ASSERT_THAT(read_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(*read_res, source.size());

  SCOPED_TRACE("// check the sender address matches");
  EXPECT_EQ(recvfrom_endp, server_endp);
}

// send of a zero-length-buffer is a no-op, for stream-protocols
//
// therefore, send() of a zero-length-buffer should pass if the socket isn't
// connected
TEST(NetTS_internet, tcp_ipv4_socket_send_0) {
  net::io_context io_ctx;

  net::ip::tcp::socket client_sock(io_ctx);

  auto send_res =
      client_sock.send(net::buffer(static_cast<const void *>(nullptr), 0));

  ASSERT_THAT(send_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(send_res.value(), 0);
}

// recv into a zero-length-buffer is a no-op for stream-protocols
//
// therefore, recv() into a zero-length-buffer should pass if the socket isn't
// connected
TEST(NetTS_internet, tcp_ipv4_socket_recv_0) {
  net::io_context io_ctx;

  net::ip::tcp::socket client_sock(io_ctx);

  auto recv_res =
      client_sock.receive(net::buffer(static_cast<void *>(nullptr), 0));

  ASSERT_THAT(recv_res, ::testing::Truly(res_has_value));
  EXPECT_EQ(recv_res.value(), 0);
}

// send of a zero-length-buffer wants to send something, for datagram-protocols
//
// therefore, a send() of a zero-length buffer should fail if the socket isn't
// connected
TEST(NetTS_internet, udp_ipv4_socket_send_0) {
  net::io_context io_ctx;

  net::ip::udp::socket client_sock(io_ctx);

  auto send_res =
      client_sock.send(net::buffer(static_cast<const void *>(nullptr), 0));

  // expected the send() to fail
  ASSERT_THAT(send_res, ::testing::Not(::testing::Truly(res_has_value)));

  // native_handle() is still invalid, EBADF is expected
  EXPECT_THAT(
      send_res.error(),
      ::testing::AnyOf(
          make_error_condition(std::errc::bad_file_descriptor),  // linux
          make_error_condition(std::errc::not_a_socket)          // windows
          ));
}

// recv into a zero-length-buffer wants to recv something, for
// datagram-protocols
//
// therefore, a recv() into a zero-length buffer should fail if the socket isn't
// connected
TEST(NetTS_internet, udp_ipv4_socket_recv_0) {
  net::io_context io_ctx;

  net::ip::udp::socket client_sock(io_ctx);

  auto recv_res =
      client_sock.receive(net::buffer(static_cast<void *>(nullptr), 0));

  // expected the recv() to fail
  ASSERT_THAT(recv_res, ::testing::Not(::testing::Truly(res_has_value)));
  // native_handle() is still invalid, EBADF is expected
  EXPECT_THAT(
      recv_res.error(),
      ::testing::AnyOf(
          make_error_condition(std::errc::bad_file_descriptor),  // linux
          make_error_condition(std::errc::not_a_socket)          // windows
          ));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  net::impl::socket::init();

  return RUN_ALL_TESTS();
}
