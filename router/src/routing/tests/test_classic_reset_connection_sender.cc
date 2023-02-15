/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "classic_reset_connection_sender.h"

#include <initializer_list>

#include <gmock/gmock.h>

#include "classic_connection_base.h"
#include "mysql/harness/net_ts/buffer.h"
#include "protocol/base_protocol.h"
#include "stdx_expected_no_error.h"

using namespace std::chrono_literals;

TEST(ResetConnectionSenderTest, sender) {
  net::io_context io_ctx;
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  conf.net_buffer_length = 16 * 1024;
  conf.connect_timeout = 10;
  conf.client_connect_timeout = 10;
  conf.bind_address = mysql_harness::TCPAddress{"", 3306};

  MySQLRoutingContext mock_ctx{conf, "name", {}, {}};

  auto conn = MysqlRoutingClassicConnectionBase::create(
      mock_ctx,  // ctx
      nullptr,   // RouteDestination
      std::make_unique<TcpConnection>(net::ip::tcp::socket(io_ctx),
                                      net::ip::tcp::endpoint{}),
      nullptr,  // client-routing-connection
      [](auto *) {});

  // taint the seq-id
  conn->server_protocol()->seq_id(42);

  ResetConnectionSender sender(conn.get());

  // first
  EXPECT_EQ(sender.stage(), ResetConnectionSender::Stage::Command);

  auto *channel = conn->socket_splicer()->server_channel();

  // send packet to server
  {
    auto process_res = sender.process();
    ASSERT_NO_ERROR(process_res);
    EXPECT_EQ(*process_res, Processor::Result::SendToServer);
    EXPECT_EQ(sender.stage(), ResetConnectionSender::Stage::Response);

    // send-buffer should contain a ResetConnection message.
    EXPECT_THAT(channel->send_buffer(),
                ::testing::ElementsAreArray({0x01, 0x00, 0x00, 0x00, 0x1f}));

    net::dynamic_buffer(channel->send_buffer()).consume(5);
  }

  {
    // ::response: Ok
    auto &recv_buf = channel->recv_buffer();

    recv_buf.insert(recv_buf.end(), {0x07, 0x00, 0x00, 0x01,  //
                                     0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00});
    channel->view_sync_raw();

    auto process_res = sender.process();
    ASSERT_NO_ERROR(process_res);
    EXPECT_EQ(*process_res, Processor::Result::Again);
    EXPECT_EQ(sender.stage(), ResetConnectionSender::Stage::Ok);
  }

  {
    // ::ok
    auto process_res = sender.process();
    ASSERT_NO_ERROR(process_res);
    EXPECT_EQ(*process_res, Processor::Result::Again);
    EXPECT_EQ(sender.stage(), ResetConnectionSender::Stage::Done);

    // all consumed.
    EXPECT_EQ(channel->recv_view().size(), 0);
  }

  {
    // finished.
    auto process_res = sender.process();
    ASSERT_NO_ERROR(process_res);
    EXPECT_EQ(*process_res, Processor::Result::Done);
  }
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
