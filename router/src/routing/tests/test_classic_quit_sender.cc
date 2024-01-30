/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "classic_quit_sender.h"

#include <array>

#include <gmock/gmock.h>

#include "classic_connection_base.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysqlrouter/base_protocol.h"
#include "stdx_expected_no_error.h"

using namespace std::chrono_literals;

using Msg = classic_protocol::message::client::Quit;
using Frm = classic_protocol::frame::Frame<Msg>;

// low-level encode
TEST(QuitMessage, codec_encode) {
  static_assert(classic_protocol::Codec<Msg>({}, {}).size() == 1);
  static_assert(classic_protocol::Codec<Frm>({0, {}}, {}).size() == 5);

  using encode_buf_type =
      std::array<uint8_t, classic_protocol::Codec<Frm>({0, {}}, {}).size()>;

  encode_buf_type encode_buf{};

  EXPECT_TRUE((classic_protocol::Codec<Frm>({0, {}}, {})
                   .encode(net::buffer(encode_buf)) ==
               stdx::expected<size_t, std::error_code>(5)));
}

// high-level encode
TEST(QuitMessage, encode) {
  constexpr Msg msg{};
  constexpr Frm frm{0, msg};

  std::vector<uint8_t> frame_buf;

  auto encode_res =
      classic_protocol::encode(frm, {}, net::dynamic_buffer(frame_buf));
  ASSERT_NO_ERROR(encode_res);

  EXPECT_EQ(*encode_res, 5);
  // buffer should contain a Quit message.
  EXPECT_THAT(frame_buf,
              ::testing::ElementsAreArray({0x01, 0x00, 0x00, 0x00, 0x01}));
}

TEST(QuitSenderTest, sender) {
  net::io_context io_ctx;
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  conf.net_buffer_length = 16 * 1024;
  conf.connect_timeout = 10;
  conf.client_connect_timeout = 10;
  conf.bind_address = mysql_harness::TCPAddress{"", 3306};

  MySQLRoutingContext ctx{conf, "name", {}, {}};

  auto conn = MysqlRoutingClassicConnectionBase::create(
      ctx,      // ctx
      nullptr,  // RouteDestination
      std::make_unique<TcpConnection>(net::ip::tcp::socket(io_ctx),
                                      net::ip::tcp::endpoint{}),
      nullptr,  // client-routing-connection
      [](auto *) {});

  // taint the seq-id
  conn->server_protocol().seq_id(42);

  QuitSender sender(conn.get());

  // first
  EXPECT_EQ(sender.stage(), QuitSender::Stage::Command);

  // send packet to server
  {
    auto process_res = sender.process();
    ASSERT_NO_ERROR(process_res);
    EXPECT_EQ(*process_res, Processor::Result::SendToServer);
    EXPECT_EQ(sender.stage(), QuitSender::Stage::CloseSocket);

    // send-buffer should contain a Quit message.
    EXPECT_THAT(conn->server_conn().channel().send_buffer(),
                ::testing::ElementsAreArray({0x01, 0x00, 0x00, 0x00, 0x01}));
  }

  // close socket
  {
    auto process_res = sender.process();
    ASSERT_NO_ERROR(process_res);
    EXPECT_EQ(*process_res, Processor::Result::Again);
    EXPECT_EQ(sender.stage(), QuitSender::Stage::Done);
  }

  // done
  {
    auto process_res = sender.process();
    ASSERT_NO_ERROR(process_res);
    EXPECT_EQ(*process_res, Processor::Result::Done);
  }
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
