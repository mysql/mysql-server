/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "classic_connection.h"

#include <array>
#include <memory>
#include <ostream>
#include <string>
#include <system_error>
#include <vector>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include <openssl/ssl.h>  // SSL_CTX

#include "channel.h"
#include "connection.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/tls_client_context.h"
#include "mysql/harness/tls_context.h"
#include "mysql/harness/tls_error.h"
#include "mysql/harness/tls_server_context.h"
#include "openssl_version.h"
#include "protocol/base_protocol.h"
#include "ssl_mode.h"
#include "test/helpers.h"  // init_test_logger

#define ASSERT_NO_ERROR(x) \
  ASSERT_THAT((x), ::testing::Truly([](auto const &v) { return (bool)v; }))

#define EXPECT_NO_ERROR(x) \
  EXPECT_THAT((x), ::testing::Truly([](auto const &v) { return (bool)v; }))

using namespace std::chrono_literals;
#if 0
class ClassicProtocolSplicerTest : public ::testing::Test {
 public:
 protected:
  MySQLRoutingContext ctx_{BaseProtocol::Type::kClassicProtocol,  // protocol
                           "someroute",                           // name
                           4 * 1024,               // net_buffer_length
                           1s,                     // dest_connect_timeout
                           1s,                     // client_connect_timeout
                           {},                     // bind_address
                           {"somepath"},           // bind_named_socket
                           100,                    // max_connect_errors
                           1024,                   // thread_stack_size
                           SslMode::kPassthrough,  // client_ssl_mode
                           nullptr,                // client_ssl_ctx
                           SslMode::kAsClient,     // server_ssl_mode
                           nullptr};
};

template <>
inline std::vector<std::pair<std::string, std::string>>
initial_connection_attributes<local::stream_protocol>(
    const local::stream_protocol::endpoint &ep) {
  return {
      {"_client_socket", ep.path()},
  };
}

TEST_F(ClassicProtocolSplicerTest, disabled_to_disabled_initial_state) {
  net::io_context io_ctx;

  MysqlRoutingClassicConnection<net::ip::tcp, net::ip::tcp> splicer(
      ctx_, "somedestid",            //
      net::ip::tcp::socket{io_ctx},  // from_socket
      net::ip::tcp::endpoint{},      // from_endpoint
      net::ip::tcp::socket{io_ctx},  // to_socket
      net::ip::tcp::endpoint{},      // to_endpoint
      [](MySQLRoutingConnectionBase *) {});

  EXPECT_EQ(splicer.source_ssl_mode(), SslMode::kPassthrough);
  EXPECT_EQ(splicer.dest_ssl_mode(), SslMode::kAsClient);
}

TEST_F(ClassicProtocolSplicerTest, server_greeting_broken) {
  net::io_context io_ctx;

  using Connection = MysqlRoutingClassicConnection<local::stream_protocol,
                                                   local::stream_protocol>;

  Connection::client_protocol_type::socket client_sock{io_ctx};
  Connection::client_protocol_type::socket from_sock2{io_ctx};

  Connection::server_protocol_type::socket to_sock1{io_ctx};
  Connection::server_protocol_type::socket server_sock{io_ctx};

  ASSERT_TRUE(local::connect_pair(&io_ctx, client_sock, from_sock2));
  ASSERT_TRUE(local::connect_pair(&io_ctx, to_sock1, server_sock));

  Connection splicer(ctx_, "somedestid",                     //
                     std::move(from_sock2),                  // from_socket
                     client_sock.remote_endpoint().value(),  // from_endpoint
                     std::move(to_sock1),                    // to_socket
                     server_sock.remote_endpoint().value(),  // to_endpoint
                     [](MySQLRoutingConnectionBase *) {});
  splicer.async_run();  // should call server_greeting()

  std::vector<uint8_t> broken_greeting{0x01, 0x00, 0x00, 0x00, 0x00};

  server_sock.send(net::buffer(broken_greeting));

  // recv-buffer is empty, we should stay in the same state.
  io_ctx.run_one();

  std::vector<uint8_t> recv_buf;

  auto read_res = net::read(client_sock, net::dynamic_buffer(recv_buf),
                            net::transfer_at_least(4));
  EXPECT_EQ(read_res, stdx::unexpected(make_error_code(net::stream_errc::eof)));
}

TEST_F(ClassicProtocolSplicerTest, server_greeting_partial) {
  net::io_context io_ctx;

  using Connection = MysqlRoutingClassicConnection<local::stream_protocol,
                                                   local::stream_protocol>;

  Connection::client_protocol_type::socket client_sock{io_ctx};
  Connection::client_protocol_type::socket from_sock2{io_ctx};

  Connection::server_protocol_type::socket to_sock1{io_ctx};
  Connection::server_protocol_type::socket server_sock{io_ctx};

  ASSERT_TRUE(local::connect_pair(&io_ctx, client_sock, from_sock2));
  ASSERT_TRUE(local::connect_pair(&io_ctx, to_sock1, server_sock));

  Connection splicer(ctx_, "somedestid",                     //
                     std::move(from_sock2),                  // from_socket
                     client_sock.remote_endpoint().value(),  // from_endpoint
                     std::move(to_sock1),                    // to_socket
                     server_sock.remote_endpoint().value(),  // to_endpoint
                     [](MySQLRoutingConnectionBase *) {});

  const std::array<uint8_t, 0x4a + 4> packet = {
      {0x4a, 0x00, 0x00, 0x00, 0x0a, 0x38, 0x2e, 0x30, 0x2e, 0x32, 0x30, 0x00,
       0xf1, 0x03, 0x00, 0x00, 0x11, 0x3f, 0x0f, 0x35, 0x70, 0x2f, 0x62, 0x5b,
       0x00, 0xff, 0xff, 0xff, 0x02, 0x00, 0xff, 0xc7, 0x15, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x01, 0x67, 0x23, 0x0b,
       0x44, 0x01, 0x2c, 0x1c, 0x65, 0x58, 0x57, 0x00, 0x63, 0x61, 0x63, 0x68,
       0x69, 0x6e, 0x67, 0x5f, 0x73, 0x68, 0x61, 0x32, 0x5f, 0x70, 0x61, 0x73,
       0x73, 0x77, 0x6f, 0x72, 0x64, 0x00}};

  auto send_buf = net::buffer(packet);

  // split the packet into 2 parts at byte ... 12. Any position would be good.
  constexpr const size_t part_split{12};

  auto send_res = server_sock.send(net::buffer(send_buf, part_split));
  ASSERT_TRUE(send_res) << send_res.error();
  EXPECT_EQ(send_res.value(), part_split);
  send_buf += send_res.value();

  // trigger server greeting.
  splicer.async_run();

  io_ctx.run_one();

  // nothing should be sent yet.
  auto avail_res = client_sock.available();
  ASSERT_TRUE(avail_res) << avail_res.error();
  EXPECT_EQ(avail_res.value(), 0);

  send_res = server_sock.send(net::buffer(send_buf));
  ASSERT_TRUE(send_res) << send_res.error();
  EXPECT_EQ(send_res.value(), packet.size() - part_split);

  io_ctx.run_one();
  io_ctx.run_one();

  avail_res = client_sock.available();
  ASSERT_TRUE(avail_res) << avail_res.error();
  EXPECT_EQ(avail_res.value(), packet.size());
}

TEST_F(ClassicProtocolSplicerTest, server_greeting_full) {
  net::io_context io_ctx;

  using Connection = MysqlRoutingClassicConnection<local::stream_protocol,
                                                   local::stream_protocol>;

  Connection::client_protocol_type::socket client_sock{io_ctx};
  Connection::client_protocol_type::socket from_sock2{io_ctx};

  Connection::server_protocol_type::socket to_sock1{io_ctx};
  Connection::server_protocol_type::socket server_sock{io_ctx};

  ASSERT_TRUE(local::connect_pair(&io_ctx, client_sock, from_sock2));
  ASSERT_TRUE(local::connect_pair(&io_ctx, to_sock1, server_sock));

  Connection splicer(ctx_, "somedestid",                     //
                     std::move(from_sock2),                  // from_socket
                     client_sock.remote_endpoint().value(),  // from_endpoint
                     std::move(to_sock1),                    // to_socket
                     server_sock.remote_endpoint().value(),  // to_endpoint
                     [](MySQLRoutingConnectionBase *) {});

  const std::array<uint8_t, 0x4a + 4> packet = {
      {0x4a, 0x00, 0x00, 0x00, 0x0a, 0x38, 0x2e, 0x30, 0x2e, 0x32, 0x30, 0x00,
       0xf1, 0x03, 0x00, 0x00, 0x11, 0x3f, 0x0f, 0x35, 0x70, 0x2f, 0x62, 0x5b,
       0x00, 0xff, 0xff, 0xff, 0x02, 0x00, 0xff, 0xc7, 0x15, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x01, 0x67, 0x23, 0x0b,
       0x44, 0x01, 0x2c, 0x1c, 0x65, 0x58, 0x57, 0x00, 0x63, 0x61, 0x63, 0x68,
       0x69, 0x6e, 0x67, 0x5f, 0x73, 0x68, 0x61, 0x32, 0x5f, 0x70, 0x61, 0x73,
       0x73, 0x77, 0x6f, 0x72, 0x64, 0x00}};

  auto send_buf = net::buffer(packet);

  auto send_res = server_sock.send(send_buf);
  ASSERT_TRUE(send_res) << send_res.error();
  EXPECT_EQ(send_res.value(), packet.size());
  send_buf += send_res.value();

  // trigger server greeting.
  splicer.async_run();

  io_ctx.run_one();
  io_ctx.run_one();

  // nothing should be sent yet.
  auto avail_res = client_sock.available();
  ASSERT_TRUE(avail_res) << avail_res.error();
  EXPECT_EQ(avail_res.value(), packet.size());
}

TEST_F(ClassicProtocolSplicerTest, client_greeting_partial) {
  net::io_context io_ctx;

  using Connection = MysqlRoutingClassicConnection<local::stream_protocol,
                                                   local::stream_protocol>;

  Connection::client_protocol_type::socket client_sock{io_ctx};
  Connection::client_protocol_type::socket from_sock2{io_ctx};

  Connection::server_protocol_type::socket to_sock1{io_ctx};
  Connection::server_protocol_type::socket server_sock{io_ctx};

  ASSERT_TRUE(local::connect_pair(&io_ctx, client_sock, from_sock2));
  ASSERT_TRUE(local::connect_pair(&io_ctx, to_sock1, server_sock));

  Connection splicer(ctx_, "somedestid",                     //
                     std::move(from_sock2),                  // from_socket
                     client_sock.remote_endpoint().value(),  // from_endpoint
                     std::move(to_sock1),                    // to_socket
                     server_sock.remote_endpoint().value(),  // to_endpoint
                     [](MySQLRoutingConnectionBase *) {});

  const std::array<uint8_t, 0x4a + 4> packet = {
      {0x4a, 0x00, 0x00, 0x00, 0x0a, 0x38, 0x2e, 0x30, 0x2e, 0x32, 0x30, 0x00,
       0xf1, 0x03, 0x00, 0x00, 0x11, 0x3f, 0x0f, 0x35, 0x70, 0x2f, 0x62, 0x5b,
       0x00, 0xff, 0xff, 0xff, 0x02, 0x00, 0xff, 0xc7, 0x15, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x01, 0x67, 0x23, 0x0b,
       0x44, 0x01, 0x2c, 0x1c, 0x65, 0x58, 0x57, 0x00, 0x63, 0x61, 0x63, 0x68,
       0x69, 0x6e, 0x67, 0x5f, 0x73, 0x68, 0x61, 0x32, 0x5f, 0x70, 0x61, 0x73,
       0x73, 0x77, 0x6f, 0x72, 0x64, 0x00}};

  auto send_buf = net::buffer(packet);

  auto send_res = server_sock.send(send_buf);
  ASSERT_TRUE(send_res) << send_res.error();
  EXPECT_EQ(send_res.value(), packet.size());
  send_buf += send_res.value();

  // trigger server greeting.
  splicer.async_run();

  io_ctx.run_one();
  io_ctx.run_one();

  // nothing should be sent yet.
  auto avail_res = client_sock.available();
  ASSERT_TRUE(avail_res) << avail_res.error();
  EXPECT_EQ(avail_res.value(), packet.size());

  const std::array<uint8_t, 4> client_greeting_partial = {
      {0x20, 0x00, 0x00, 0x01}};
  auto client_send_res = client_sock.send(net::buffer(client_greeting_partial));
  ASSERT_TRUE(client_send_res) << client_send_res.error();
  EXPECT_EQ(client_send_res.value(), client_greeting_partial.size());
}


TEST(ClassicProtocolSplicerTest, client_greeting_ssl) {
  TlsClientContext tls_client_ctx;

  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kDisabled,
      [&]() -> SSL_CTX * { return tls_client_ctx.get(); },
      []() { return nullptr; }, {});

  splicer.state(BasicSplicer::State::CLIENT_GREETING);

  // server has SSL enabled.
  splicer.client_protocol()->server_capabilities(
      classic_protocol::capabilities::ssl);

  ASSERT_EQ(splicer.state(), BasicSplicer::State::CLIENT_GREETING);

  // a short client greeting with SSL cap set.
  const std::array<uint8_t, 4 + 0x20> packet = {
      {0x20, 0x00, 0x00, 0x01, 0x05, 0xae, 0xff, 0x01, 0x00, 0x00, 0x00, 0x01,
       0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

  auto &recv_buf = splicer.client_channel()->recv_buffer();

  recv_buf.resize(packet.size());
  net::buffer_copy(net::buffer(recv_buf), net::buffer(packet));

  // recv-buffer is empty, we should stay in the same state.
  EXPECT_EQ(splicer.client_greeting(), BasicSplicer::State::TLS_ACCEPT);
}

TEST(ClassicProtocolSplicerTest, client_greeting_ssl_split) {
  TlsClientContext tls_client_ctx;

  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kDisabled,
      [&]() -> SSL_CTX * { return tls_client_ctx.get(); },
      []() { return nullptr; }, {});

  splicer.state(BasicSplicer::State::CLIENT_GREETING);

  // server has SSL enabled.
  splicer.client_protocol()->server_capabilities(
      classic_protocol::capabilities::ssl);

  ASSERT_EQ(splicer.state(), BasicSplicer::State::CLIENT_GREETING);

  // a short client greeting with SSL cap set, 1st part
  const std::array<uint8_t, 4 + 0x20 - 2> packet_part_1 = {
      {0x20, 0x00, 0x00, 0x01, 0x05, 0xae, 0xff, 0x01, 0x00, 0x00, 0x00, 0x01,
       0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

  // a short client greeting with SSL cap set, 2nd part
  const std::array<uint8_t, 2> packet_part_2 = {{0x00, 0x00}};

  // append part-1 to empty recv-buf
  auto &recv_buf = splicer.client_channel()->recv_buffer();

  recv_buf.resize(packet_part_1.size());
  net::buffer_copy(net::buffer(recv_buf), net::buffer(packet_part_1));

  // check it is treated as to-short
  EXPECT_EQ(splicer.client_greeting(), BasicSplicer::State::CLIENT_GREETING);
  EXPECT_NE(splicer.client_channel()->want_recv(), 0);

  // append part-2
  const auto orig_size = recv_buf.size();
  recv_buf.resize(orig_size + packet_part_2.size());
  net::buffer_copy(net::buffer(net::buffer(recv_buf) + orig_size),
                   net::buffer(packet_part_2));

  // it should make progress now.
  EXPECT_EQ(splicer.client_greeting(), BasicSplicer::State::TLS_ACCEPT);
}

TEST(ClassicProtocolSplicerTest, tls_accept_partial) {
  TlsServerContext tls_server_ctx;

  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kDisabled,
      [&]() -> SSL_CTX * { return tls_server_ctx.get(); },
      []() { return nullptr; }, {});

  ASSERT_NO_ERROR(tls_server_ctx.load_key_and_cert(
      SSL_TEST_DATA_DIR "/server-key-sha512.pem",
      SSL_TEST_DATA_DIR "/server-cert-sha512.pem"));
  ASSERT_NO_ERROR(tls_server_ctx.cipher_list("ALL"));
  ASSERT_NO_ERROR(tls_server_ctx.init_tmp_dh(""));

  // done at the end of the client-greeting state.
  splicer.client_channel()->init_ssl(tls_server_ctx.get());
  splicer.state(BasicSplicer::State::TLS_ACCEPT);

  // server has SSL enabled.
  splicer.client_protocol()->server_capabilities(
      classic_protocol::capabilities::ssl);

  // a TLS handshake.
  const std::array<uint8_t, 5 + 0x012a> packet_tls_1 = {
      {0x16, 0x03, 0x01, 0x01, 0x2a, 0x01, 0x00, 0x01, 0x26, 0x03, 0x03, 0x68,
       0xba, 0xd0, 0x7b, 0xf9, 0xea, 0x92, 0x65, 0x56, 0x06, 0xfe, 0x30, 0xcd,
       0x9f, 0x51, 0x7a, 0x10, 0xdb, 0x8c, 0x6f, 0x6f, 0x05, 0xf4, 0xb5, 0x8a,
       0x8f, 0x12, 0x08, 0xfa, 0x54, 0x3d, 0x88, 0x20, 0xe4, 0xbd, 0x7f, 0x36,
       0x76, 0xac, 0xf9, 0x6b, 0x26, 0x5d, 0x7f, 0x03, 0x11, 0x00, 0x3c, 0xd0,
       0x65, 0xcd, 0xe0, 0x3b, 0xf6, 0x1e, 0x63, 0xa8, 0x37, 0x58, 0x53, 0xd2,
       0xd6, 0x10, 0x3e, 0x47, 0x00, 0x48, 0x13, 0x02, 0x13, 0x03, 0x13, 0x01,
       0xc0, 0x2b, 0xc0, 0x2c, 0xc0, 0x2f, 0xc0, 0x23, 0xc0, 0x27, 0xc0, 0x30,
       0xc0, 0x24, 0xc0, 0x28, 0x00, 0x9e, 0x00, 0xa2, 0x00, 0x67, 0x00, 0x40,
       0x00, 0xa3, 0x00, 0x6b, 0x00, 0x6a, 0x00, 0x9f, 0xc0, 0x13, 0xc0, 0x09,
       0xc0, 0x14, 0xc0, 0x0a, 0x00, 0x32, 0x00, 0x33, 0x00, 0x38, 0x00, 0x39,
       0x00, 0x35, 0x00, 0x84, 0x00, 0x41, 0x00, 0x9c, 0x00, 0x9d, 0x00, 0x3c,
       0x00, 0x3d, 0x00, 0x2f, 0x00, 0xff, 0x01, 0x00, 0x00, 0x95, 0x00, 0x0b,
       0x00, 0x04, 0x03, 0x00, 0x01, 0x02, 0x00, 0x0a, 0x00, 0x0c, 0x00, 0x0a,
       0x00, 0x1d, 0x00, 0x17, 0x00, 0x1e, 0x00, 0x19, 0x00, 0x18, 0x00, 0x23,
       0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x0d,
       0x00, 0x30, 0x00, 0x2e, 0x04, 0x03, 0x05, 0x03, 0x06, 0x03, 0x08, 0x07,
       0x08, 0x08, 0x08, 0x09, 0x08, 0x0a, 0x08, 0x0b, 0x08, 0x04, 0x08, 0x05,
       0x08, 0x06, 0x04, 0x01, 0x05, 0x01, 0x06, 0x01, 0x03, 0x03, 0x02, 0x03,
       0x03, 0x01, 0x02, 0x01, 0x03, 0x02, 0x02, 0x02, 0x04, 0x02, 0x05, 0x02,
       0x06, 0x02, 0x00, 0x2b, 0x00, 0x09, 0x08, 0x03, 0x04, 0x03, 0x03, 0x03,
       0x02, 0x03, 0x01, 0x00, 0x2d, 0x00, 0x02, 0x01, 0x01, 0x00, 0x33, 0x00,
       0x26, 0x00, 0x24, 0x00, 0x1d, 0x00, 0x20, 0x81, 0xd5, 0x98, 0xd6, 0x85,
       0xb5, 0x54, 0x35, 0x2f, 0x99, 0x30, 0x52, 0x05, 0x51, 0xc4, 0x16, 0x5d,
       0xc9, 0x96, 0xb9, 0x00, 0x43, 0xb4, 0x84, 0x90, 0x2c, 0x11, 0x6e, 0xb6,
       0xf6, 0xe9, 0x08}};

  auto &recv_buf = splicer.client_channel()->recv_buffer();

  recv_buf.resize(packet_tls_1.size());
  net::buffer_copy(net::buffer(recv_buf), net::buffer(packet_tls_1));

  // recv-buffer has a TLS handshake, but we isn't finished, we should stay in
  // the same state.
  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_ACCEPT);
  EXPECT_EQ(splicer.tls_accept(), BasicSplicer::State::TLS_ACCEPT);

  EXPECT_GT(splicer.client_channel()->send_buffer().size(), 0);

  // we can't parse more here as it involves randomness
}

TEST(ClassicProtocolSplicerTest, tls_accept_fail) {
  TlsServerContext tls_server_ctx;

  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kDisabled,
      [&]() -> SSL_CTX * { return tls_server_ctx.get(); },
      []() { return nullptr; }, {});

  // don't set the server-cert/key which leads to no ciphers that are shared.
  ASSERT_NO_ERROR(tls_server_ctx.cipher_list("ALL"));
  ASSERT_NO_ERROR(tls_server_ctx.init_tmp_dh(""));

  // done at the end of the client-greeting state.
  splicer.client_channel()->init_ssl(tls_server_ctx.get());
  splicer.state(BasicSplicer::State::TLS_ACCEPT);

  // server has SSL enabled.
  splicer.client_protocol()->server_capabilities(
      classic_protocol::capabilities::ssl);

  // a TLS handshake.
  const std::array<uint8_t, 5 + 0x012a> packet_tls_1 = {
      {0x16, 0x03, 0x01, 0x01, 0x2a, 0x01, 0x00, 0x01, 0x26, 0x03, 0x03, 0x68,
       0xba, 0xd0, 0x7b, 0xf9, 0xea, 0x92, 0x65, 0x56, 0x06, 0xfe, 0x30, 0xcd,
       0x9f, 0x51, 0x7a, 0x10, 0xdb, 0x8c, 0x6f, 0x6f, 0x05, 0xf4, 0xb5, 0x8a,
       0x8f, 0x12, 0x08, 0xfa, 0x54, 0x3d, 0x88, 0x20, 0xe4, 0xbd, 0x7f, 0x36,
       0x76, 0xac, 0xf9, 0x6b, 0x26, 0x5d, 0x7f, 0x03, 0x11, 0x00, 0x3c, 0xd0,
       0x65, 0xcd, 0xe0, 0x3b, 0xf6, 0x1e, 0x63, 0xa8, 0x37, 0x58, 0x53, 0xd2,
       0xd6, 0x10, 0x3e, 0x47, 0x00, 0x48, 0x13, 0x02, 0x13, 0x03, 0x13, 0x01,
       0xc0, 0x2b, 0xc0, 0x2c, 0xc0, 0x2f, 0xc0, 0x23, 0xc0, 0x27, 0xc0, 0x30,
       0xc0, 0x24, 0xc0, 0x28, 0x00, 0x9e, 0x00, 0xa2, 0x00, 0x67, 0x00, 0x40,
       0x00, 0xa3, 0x00, 0x6b, 0x00, 0x6a, 0x00, 0x9f, 0xc0, 0x13, 0xc0, 0x09,
       0xc0, 0x14, 0xc0, 0x0a, 0x00, 0x32, 0x00, 0x33, 0x00, 0x38, 0x00, 0x39,
       0x00, 0x35, 0x00, 0x84, 0x00, 0x41, 0x00, 0x9c, 0x00, 0x9d, 0x00, 0x3c,
       0x00, 0x3d, 0x00, 0x2f, 0x00, 0xff, 0x01, 0x00, 0x00, 0x95, 0x00, 0x0b,
       0x00, 0x04, 0x03, 0x00, 0x01, 0x02, 0x00, 0x0a, 0x00, 0x0c, 0x00, 0x0a,
       0x00, 0x1d, 0x00, 0x17, 0x00, 0x1e, 0x00, 0x19, 0x00, 0x18, 0x00, 0x23,
       0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x0d,
       0x00, 0x30, 0x00, 0x2e, 0x04, 0x03, 0x05, 0x03, 0x06, 0x03, 0x08, 0x07,
       0x08, 0x08, 0x08, 0x09, 0x08, 0x0a, 0x08, 0x0b, 0x08, 0x04, 0x08, 0x05,
       0x08, 0x06, 0x04, 0x01, 0x05, 0x01, 0x06, 0x01, 0x03, 0x03, 0x02, 0x03,
       0x03, 0x01, 0x02, 0x01, 0x03, 0x02, 0x02, 0x02, 0x04, 0x02, 0x05, 0x02,
       0x06, 0x02, 0x00, 0x2b, 0x00, 0x09, 0x08, 0x03, 0x04, 0x03, 0x03, 0x03,
       0x02, 0x03, 0x01, 0x00, 0x2d, 0x00, 0x02, 0x01, 0x01, 0x00, 0x33, 0x00,
       0x26, 0x00, 0x24, 0x00, 0x1d, 0x00, 0x20, 0x81, 0xd5, 0x98, 0xd6, 0x85,
       0xb5, 0x54, 0x35, 0x2f, 0x99, 0x30, 0x52, 0x05, 0x51, 0xc4, 0x16, 0x5d,
       0xc9, 0x96, 0xb9, 0x00, 0x43, 0xb4, 0x84, 0x90, 0x2c, 0x11, 0x6e, 0xb6,
       0xf6, 0xe9, 0x08}};

  auto &recv_buf = splicer.client_channel()->recv_buffer();

  recv_buf.resize(packet_tls_1.size());
  net::buffer_copy(net::buffer(recv_buf), net::buffer(packet_tls_1));

  // recv-buffer has a TLS handshake, but we isn't finished, we should stay in
  // the same state.
  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_ACCEPT);
  EXPECT_EQ(splicer.tls_accept(), BasicSplicer::State::FINISH);

  // an alert for the client
  EXPECT_GT(splicer.client_channel()->send_buffer().size(), 0);
}

TEST(ClassicProtocolSplicerTest, tls_connect_fail) {
  TlsClientContext tls_client_ctx;

  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kPreferred,
      [&]() -> SSL_CTX * { return tls_client_ctx.get(); },
      []() { return nullptr; }, {});

  splicer.state(BasicSplicer::State::TLS_CONNECT);

  // server has SSL enabled.
  splicer.server_channel()->init_ssl(tls_client_ctx.get());
  splicer.server_protocol()->server_capabilities(
      classic_protocol::capabilities::ssl);

  // two TLS packets
  const std::array<uint8_t, 5 + 0x02> packet = {{
      0x15,        // alert
      0x03, 0x03,  // version
      0x00, 0x02,  // length
      0x02,        // level: fatal
      0x28         // Handshake failure
  }};

  auto &recv_buf = splicer.server_channel()->recv_buffer();

  recv_buf.resize(packet.size());
  net::buffer_copy(net::buffer(recv_buf), net::buffer(packet));

  // recv-buffer is empty, we should stay in the same state.
  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_CONNECT);
  EXPECT_EQ(splicer.tls_connect(), BasicSplicer::State::FINISH);

  // the client should get an error-msg
  EXPECT_GT(splicer.client_channel()->send_buffer().size(), 0);
}

struct ProtocolSplicerParam {
  const char *test_name;

  TlsVersion tls_version;
};

/*
 * parametrize tests by tls-version.
 */
class ProtocolSplicerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ProtocolSplicerParam> {
 protected:
  void tls_handshake(ClassicProtocolSplicer &splicer,
                     Channel &connector_channel) {
    auto &recv_buf = splicer.client_channel()->recv_buffer();

    do {
      SCOPED_TRACE("// tls-connect");
      const auto connect_res = connector_channel.tls_connect();

      EXPECT_THAT(connect_res,
                  ::testing::AnyOf(stdx::expected<void, std::error_code>{},
                                   stdx::make_unexpected(
                                       make_error_code(TlsErrc::kWantRead))))
          << connect_res.error().message();

      connector_channel.flush_to_send_buf();

      if (connector_channel.send_buffer().size() > 0) {
        // move client handshake to the splicer
        BasicSplicer::move_buffer(
            net::dynamic_buffer(recv_buf),
            net::dynamic_buffer(connector_channel.send_buffer()));

        EXPECT_GT(recv_buf.size(), 0);
      }

      // connect and accept finished.
      if (connect_res &&
          splicer.state() == BasicSplicer::State::TLS_CLIENT_GREETING) {
        break;
      }

      SCOPED_TRACE("// tls-accept");
      // client handshake is in the recv-buffer.
      ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_ACCEPT);
      splicer.state(splicer.tls_accept());
      EXPECT_THAT(splicer.state(),
                  ::testing::AnyOf(BasicSplicer::State::TLS_ACCEPT,
                                   BasicSplicer::State::TLS_CLIENT_GREETING));

      if (splicer.client_channel()->send_buffer().size() > 0) {
        // move data to the mock-client again.
        // move client handshake to the splicer
        BasicSplicer::move_buffer(
            net::dynamic_buffer(connector_channel.recv_buffer()),
            net::dynamic_buffer(splicer.client_channel()->send_buffer()));

        connector_channel.flush_from_recv_buf();
      }

      // connect and accept finished.
      if (connect_res &&
          splicer.state() == BasicSplicer::State::TLS_CLIENT_GREETING) {
        break;
      }
    } while (true);
  }
};

/*
 * a full TLS accept handshake
 */
TEST_P(ProtocolSplicerTest, tls_accept_tls) {
  TlsClientContext tls_client_ctx(TlsVerify::NONE);
  TlsServerContext tls_server_ctx;

  tls_server_ctx.version_range(GetParam().tls_version, GetParam().tls_version);

  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kDisabled,
      [&]() -> SSL_CTX * { return tls_server_ctx.get(); },
      []() { return nullptr; }, {});

  ASSERT_NO_ERROR(tls_server_ctx.load_key_and_cert(
      SSL_TEST_DATA_DIR "/server-key-sha512.pem",
      SSL_TEST_DATA_DIR "/server-cert-sha512.pem"));
  ASSERT_NO_ERROR(tls_server_ctx.cipher_list("ALL"));
  ASSERT_NO_ERROR(tls_server_ctx.init_tmp_dh(""));

  // done at the end of the client-greeting state.
  splicer.client_channel()->init_ssl(tls_server_ctx.get());
  splicer.state(BasicSplicer::State::TLS_ACCEPT);

  // server has SSL enabled.
  splicer.client_protocol()->server_capabilities(
      classic_protocol::capabilities::ssl);

  // use another channel to issue the connect.
  Channel mock_client_channel;
  mock_client_channel.init_ssl(tls_client_ctx.get());

  tls_handshake(splicer, mock_client_channel);

  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_CLIENT_GREETING);
}

/*
 * a full TLS accept handshake with client greeting.
 */
TEST_P(ProtocolSplicerTest, tls_client_greeting) {
  TlsClientContext tls_client_ctx(TlsVerify::NONE);
  TlsServerContext tls_server_ctx;

  tls_server_ctx.version_range(GetParam().tls_version, GetParam().tls_version);

  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kDisabled,
      [&]() -> SSL_CTX * { return tls_server_ctx.get(); },
      []() { return nullptr; }, {});

  ASSERT_NO_ERROR(tls_server_ctx.load_key_and_cert(
      SSL_TEST_DATA_DIR "/server-key-sha512.pem",
      SSL_TEST_DATA_DIR "/server-cert-sha512.pem"));
  ASSERT_NO_ERROR(tls_server_ctx.cipher_list("ALL"));
  ASSERT_NO_ERROR(tls_server_ctx.init_tmp_dh(""));

  // pretend we are at the end of the client-greeting state.
  splicer.client_channel()->init_ssl(tls_server_ctx.get());
  splicer.state(BasicSplicer::State::TLS_ACCEPT);

  // server has SSL enabled.
  splicer.client_protocol()->server_capabilities(
      classic_protocol::capabilities::ssl);

  auto &recv_buf = splicer.client_channel()->recv_buffer();

  // use another channel to issue the connect.
  Channel mock_client_channel;
  mock_client_channel.init_ssl(tls_client_ctx.get());

  tls_handshake(splicer, mock_client_channel);

  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_CLIENT_GREETING);

  // c->r: tls-client-greeting.
  std::array<uint8_t, 4 + 0xd1> packet{
      {0xd1, 0x00, 0x00, 0x02, 0x05, 0xae, 0xff, 0x01, 0x00, 0x00, 0x00, 0x01,
       0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x66, 0x75, 0x7a, 0x7a, 0x00, 0x20, 0xfc, 0x0d, 0xdd, 0xb3, 0xc2, 0x17,
       0x88, 0x90, 0x42, 0x49, 0xe9, 0x58, 0x87, 0x33, 0xfc, 0xd2, 0x76, 0x86,
       0x9e, 0x22, 0x19, 0x59, 0x44, 0xa4, 0x72, 0xaf, 0xf7, 0x4f, 0x76, 0x43,
       0xf1, 0x0c, 0x63, 0x61, 0x63, 0x68, 0x69, 0x6e, 0x67, 0x5f, 0x73, 0x68,
       0x61, 0x32, 0x5f, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x00,
       0x74, 0x04, 0x5f, 0x70, 0x69, 0x64, 0x07, 0x33, 0x30, 0x36, 0x36, 0x38,
       0x35, 0x32, 0x09, 0x5f, 0x70, 0x6c, 0x61, 0x74, 0x66, 0x6f, 0x72, 0x6d,
       0x06, 0x78, 0x38, 0x36, 0x5f, 0x36, 0x34, 0x03, 0x5f, 0x6f, 0x73, 0x05,
       0x4c, 0x69, 0x6e, 0x75, 0x78, 0x0c, 0x5f, 0x63, 0x6c, 0x69, 0x65, 0x6e,
       0x74, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0x08, 0x6c, 0x69, 0x62, 0x6d, 0x79,
       0x73, 0x71, 0x6c, 0x07, 0x6f, 0x73, 0x5f, 0x75, 0x73, 0x65, 0x72, 0x03,
       0x6a, 0x61, 0x6e, 0x0f, 0x5f, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x5f,
       0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x06, 0x38, 0x2e, 0x30, 0x2e,
       0x32, 0x30, 0x0c, 0x70, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d, 0x5f, 0x6e,
       0x61, 0x6d, 0x65, 0x05, 0x6d, 0x79, 0x73, 0x71, 0x6c}};

  mock_client_channel.write_plain(net::buffer(packet));
  mock_client_channel.flush_to_send_buf();

  EXPECT_GT(mock_client_channel.send_buffer().size(), 0);

  BasicSplicer::move_buffer(
      net::dynamic_buffer(recv_buf),
      net::dynamic_buffer(mock_client_channel.send_buffer()));

  // c<-r: tls-accept = finished.
  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_CLIENT_GREETING);
  splicer.state(splicer.tls_client_greeting());
  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE_INIT);
}

/*
 * a full TLS accept handshake with client greeting.
 *
 * client-greeting is sent in parts.
 */
TEST_P(ProtocolSplicerTest, tls_client_greeting_partial) {
  TlsClientContext tls_client_ctx(TlsVerify::NONE);
  TlsServerContext tls_server_ctx;

  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kDisabled,
      [&]() -> SSL_CTX * { return tls_server_ctx.get(); },
      []() { return nullptr; }, {});

  ASSERT_NO_ERROR(tls_server_ctx.load_key_and_cert(
      SSL_TEST_DATA_DIR "/server-key-sha512.pem",
      SSL_TEST_DATA_DIR "/server-cert-sha512.pem"));
  ASSERT_NO_ERROR(tls_server_ctx.cipher_list("ALL"));
  ASSERT_NO_ERROR(tls_server_ctx.init_tmp_dh(""));

  // done at the end of the client-greeting state.
  splicer.client_channel()->init_ssl(tls_server_ctx.get());
  splicer.state(BasicSplicer::State::TLS_ACCEPT);

  // server has SSL enabled.
  splicer.client_protocol()->server_capabilities(
      classic_protocol::capabilities::ssl);

  auto &recv_buf = splicer.client_channel()->recv_buffer();

  // use another channel to issue the connect.
  Channel mock_client_channel;
  mock_client_channel.init_ssl(tls_client_ctx.get());

  tls_handshake(splicer, mock_client_channel);

  // c->r: tls-client-greeting. - part 1
  std::array<uint8_t, 4 + 0xd1 - 2> packet_part_1{
      {0xd1, 0x00, 0x00, 0x02, 0x05, 0xae, 0xff, 0x01, 0x00, 0x00, 0x00, 0x01,
       0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x66, 0x75, 0x7a, 0x7a, 0x00, 0x20, 0xfc, 0x0d, 0xdd, 0xb3, 0xc2, 0x17,
       0x88, 0x90, 0x42, 0x49, 0xe9, 0x58, 0x87, 0x33, 0xfc, 0xd2, 0x76, 0x86,
       0x9e, 0x22, 0x19, 0x59, 0x44, 0xa4, 0x72, 0xaf, 0xf7, 0x4f, 0x76, 0x43,
       0xf1, 0x0c, 0x63, 0x61, 0x63, 0x68, 0x69, 0x6e, 0x67, 0x5f, 0x73, 0x68,
       0x61, 0x32, 0x5f, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x00,
       0x74, 0x04, 0x5f, 0x70, 0x69, 0x64, 0x07, 0x33, 0x30, 0x36, 0x36, 0x38,
       0x35, 0x32, 0x09, 0x5f, 0x70, 0x6c, 0x61, 0x74, 0x66, 0x6f, 0x72, 0x6d,
       0x06, 0x78, 0x38, 0x36, 0x5f, 0x36, 0x34, 0x03, 0x5f, 0x6f, 0x73, 0x05,
       0x4c, 0x69, 0x6e, 0x75, 0x78, 0x0c, 0x5f, 0x63, 0x6c, 0x69, 0x65, 0x6e,
       0x74, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0x08, 0x6c, 0x69, 0x62, 0x6d, 0x79,
       0x73, 0x71, 0x6c, 0x07, 0x6f, 0x73, 0x5f, 0x75, 0x73, 0x65, 0x72, 0x03,
       0x6a, 0x61, 0x6e, 0x0f, 0x5f, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x5f,
       0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x06, 0x38, 0x2e, 0x30, 0x2e,
       0x32, 0x30, 0x0c, 0x70, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d, 0x5f, 0x6e,
       0x61, 0x6d, 0x65, 0x05, 0x6d, 0x79, 0x73}};

  mock_client_channel.write(net::buffer(packet_part_1));
  mock_client_channel.flush_to_send_buf();

  EXPECT_GT(mock_client_channel.send_buffer().size(), 0);

  BasicSplicer::move_buffer(
      net::dynamic_buffer(recv_buf),
      net::dynamic_buffer(mock_client_channel.send_buffer()));

  // no progress.
  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_CLIENT_GREETING);
  splicer.state(splicer.tls_client_greeting());
  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_CLIENT_GREETING);
  // the recv-plain buffer should be the above, partial packet.
  ASSERT_EQ(splicer.client_channel()->recv_plain_buffer().size(),
            packet_part_1.size());

  // c->r: tls-client-greeting - part 2
  std::array<uint8_t, 2> packet_part_2{{0x71, 0x6c}};

  mock_client_channel.write_plain(net::buffer(packet_part_2));
  mock_client_channel.flush_to_send_buf();

  EXPECT_GT(mock_client_channel.send_buffer().size(), 0);

  BasicSplicer::move_buffer(
      net::dynamic_buffer(recv_buf),
      net::dynamic_buffer(mock_client_channel.send_buffer()));

  // c<-r: tls-accept = finished.
  ASSERT_EQ(splicer.state(), BasicSplicer::State::TLS_CLIENT_GREETING);
  splicer.state(splicer.tls_client_greeting());
  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE_INIT);

  // the plain-buffer should be empty now.
  ASSERT_EQ(splicer.client_channel()->recv_plain_buffer().size(), 0);
}

ProtocolSplicerParam protocol_splicer_param[] = {
#if 0
    {"tls_1_0", TlsVersion::TLS_1_0},
    {"tls_1_1", TlsVersion::TLS_1_1},
#endif
    {"tls_1_2", TlsVersion::TLS_1_2},
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 1)
    {"tls_1_3", TlsVersion::TLS_1_3},
#endif
};

INSTANTIATE_TEST_SUITE_P(Spec, ProtocolSplicerTest,
                         ::testing::ValuesIn(protocol_splicer_param),
                         [](auto const &p) { return p.param.test_name; });

/*
 * splice from one channel to another.
 *
 * not encrypted.
 */
TEST(ClassicProtocolSplicerTest, splice_plain) {
  ClassicProtocolSplicer splicer(
      SslMode::kPreferred, SslMode::kDisabled, []() { return nullptr; },
      []() { return nullptr; }, {});

  splicer.state(BasicSplicer::State::SPLICE);

  auto &recv_buf = splicer.client_channel()->recv_buffer();

  std::vector<uint8_t> send_buf{0x04, 0x00, 0x00};

  BasicSplicer::move_buffer(net::dynamic_buffer(recv_buf),
                            net::dynamic_buffer(send_buf));

  EXPECT_GT(recv_buf.size(), 0);

  // c<-r: tls-accept = want-more
  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE);
  splicer.state(splicer.splice<true>());
  EXPECT_EQ(splicer.state(), BasicSplicer::State::SPLICE);

  // data should still be in the recv-plain-buf
  EXPECT_EQ(splicer.client_channel()->recv_plain_buffer().size(), 3);
  EXPECT_EQ(splicer.server_channel()->send_buffer().size(), 0);

  send_buf = {0x00, 0x01, 't', 'e', 'x'};

  BasicSplicer::move_buffer(net::dynamic_buffer(recv_buf),
                            net::dynamic_buffer(send_buf));

  EXPECT_GT(recv_buf.size(), 0);

  // c<-r: tls-accept = want-more
  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE);
  splicer.state(splicer.splice<true>());
  EXPECT_EQ(splicer.state(), BasicSplicer::State::SPLICE);

  EXPECT_EQ(splicer.client_channel()->recv_plain_buffer().size(), 0);
  EXPECT_EQ(splicer.server_channel()->send_buffer().size(), 8);
}

/*
 * splice from one channel to another.
 *
 * passthrough.
 */
TEST(ClassicProtocolSplicerTest, splice_passthrough_plain) {
  ClassicProtocolSplicer splicer(
      SslMode::kPassthrough, SslMode::kAsClient, []() { return nullptr; },
      []() { return nullptr; }, {});

  splicer.state(BasicSplicer::State::SPLICE);

  auto &recv_buf = splicer.client_channel()->recv_buffer();

  std::vector<uint8_t> send_buf{0x04, 0x00, 0x00};

  BasicSplicer::move_buffer(net::dynamic_buffer(recv_buf),
                            net::dynamic_buffer(send_buf));

  EXPECT_GT(recv_buf.size(), 0);

  // c<-r: tls-accept = want-more
  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE);
  splicer.state(splicer.splice<true>());
  EXPECT_EQ(splicer.state(), BasicSplicer::State::SPLICE);

  // data should still be in the recv-plain-buf
  EXPECT_EQ(splicer.client_channel()->recv_plain_buffer().size(), 3);
  EXPECT_EQ(splicer.server_channel()->send_buffer().size(), 0);

  send_buf = {0x00, 0x01, 't', 'e', 'x'};

  BasicSplicer::move_buffer(net::dynamic_buffer(recv_buf),
                            net::dynamic_buffer(send_buf));

  EXPECT_GT(recv_buf.size(), 0);

  // c<-r: tls-accept = want-more
  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE);
  splicer.state(splicer.splice<true>());
  EXPECT_EQ(splicer.state(), BasicSplicer::State::SPLICE);

  EXPECT_EQ(splicer.client_channel()->recv_plain_buffer().size(), 0);
  EXPECT_EQ(splicer.server_channel()->send_buffer().size(), 8);
}

/*
 * splice from one channel to another.
 *
 * passthrough, with TLS.
 */
TEST(ClassicProtocolSplicerTest, splice_passthrough_tls) {
  ClassicProtocolSplicer splicer(
      SslMode::kPassthrough, SslMode::kAsClient, []() { return nullptr; },
      []() { return nullptr; }, {});

  splicer.state(BasicSplicer::State::SPLICE);
  splicer.client_channel()->is_tls(true);

  auto &recv_buf = splicer.client_channel()->recv_buffer();

  // c->r: alert - part 1
  std::vector<uint8_t> send_buf{
      0x15,        // alert
      0x03, 0x03,  // version
  };

  BasicSplicer::move_buffer(net::dynamic_buffer(recv_buf),
                            net::dynamic_buffer(send_buf));

  EXPECT_GT(recv_buf.size(), 0);

  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE);
  splicer.state(splicer.splice<true>());
  EXPECT_EQ(splicer.state(), BasicSplicer::State::SPLICE);

  // data should still be in the recv-plain-buf
  EXPECT_EQ(splicer.client_channel()->recv_plain_buffer().size(), 3);
  EXPECT_EQ(splicer.server_channel()->send_buffer().size(), 0);

  // c->r: alert - part 2
  send_buf = {
      0x00, 0x02,  // length
      0x02,        // level: fatal
  };

  BasicSplicer::move_buffer(net::dynamic_buffer(recv_buf),
                            net::dynamic_buffer(send_buf));

  EXPECT_GT(recv_buf.size(), 0);

  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE);
  splicer.state(splicer.splice<true>());
  EXPECT_EQ(splicer.state(), BasicSplicer::State::SPLICE);

  // data should still be in the recv-plain-buf
  EXPECT_EQ(splicer.client_channel()->recv_plain_buffer().size(), 6);
  EXPECT_EQ(splicer.server_channel()->send_buffer().size(), 0);

  // c->r: alert - part 3
  send_buf = {
      0x28,  // Handshake failure
  };

  BasicSplicer::move_buffer(net::dynamic_buffer(recv_buf),
                            net::dynamic_buffer(send_buf));

  EXPECT_GT(recv_buf.size(), 0);

  // c<-r: tls-accept = want-more
  ASSERT_EQ(splicer.state(), BasicSplicer::State::SPLICE);
  splicer.state(splicer.splice<true>());
  EXPECT_EQ(splicer.state(), BasicSplicer::State::SPLICE);

  EXPECT_EQ(splicer.client_channel()->recv_plain_buffer().size(), 0);
  EXPECT_EQ(splicer.server_channel()->send_buffer().size(), 7);
}
#endif

int main(int argc, char *argv[]) {
  TlsLibraryContext lib_ctx;
  net::impl::socket::init();

  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
