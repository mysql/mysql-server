/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include "mysqlrouter/classic_protocol_codec_frame.h"

#include <gtest/gtest.h>

#include "mysqlrouter/classic_protocol_codec_message.h"  // Ping
#include "test_classic_protocol_codec.h"

using namespace classic_protocol;

// check constexpr handling

static_assert(Codec<borrowable::wire::FixedInt<1>>::size() == 1);
static_assert(Codec<borrowable::wire::FixedInt<2>>::size() == 2);
static_assert(Codec<borrowable::wire::FixedInt<3>>::size() == 3);
static_assert(Codec<borrowable::wire::FixedInt<4>>::size() == 4);
static_assert(Codec<borrowable::wire::FixedInt<8>>::size() == 8);

static_assert(Codec<borrowable::wire::VarInt>({1}, {}).size() == 1);

static_assert(borrowable::message::client::StmtClose(1).statement_id() == 1);

static_assert(Codec<borrowable::message::client::StmtClose>({1}, {}).size() ==
              1 + 4);

// static_assert(Codec<message::client::StmtClose>({1}, {}).size() == 1 + 4);

static_assert(Codec<message::client::Ping>({}, {}).size() == 1);

// Frame is fixed size
static_assert(Codec<frame::Header>({0, 0}, {}).size() == 4);

static_assert(Codec<frame::Frame<message::client::Quit>>({0, {}}, {}).size() ==
              4 + 1);

static_assert(Codec<frame::Frame<message::client::ResetConnection>>({0, {}}, {})
                  .size() == 4 + 1);

static_assert(Codec<frame::Frame<message::client::Statistics>>({0, {}}, {})
                  .size() == 4 + 1);

// Frame<Ping> is fixed size
static_assert(Codec<frame::Frame<message::client::Ping>>({0, {}}, {}).size() ==
              4 + 1);

static_assert(Codec<frame::Frame<message::client::StmtClose>>({0, {1}}, {})
                  .size() == 4 + 1 + 4);

static_assert(Codec<frame::Frame<message::client::StmtReset>>({0, {1}}, {})
                  .size() == 4 + 1 + 4);

static_assert(Codec<frame::Frame<message::client::StmtFetch>>({0, {1, 2}}, {})
                  .size() == 4 + 1 + 4 + 4);

static_assert(Codec<frame::Frame<message::client::SetOption>>({0, {1}}, {})
                  .size() == 4 + 1 + 2);
// Frame::Quit

using CodecFrameQuitTest = CodecTest<frame::Frame<message::client::Quit>>;

TEST_P(CodecFrameQuitTest, encode) { test_encode(GetParam()); }
TEST_P(CodecFrameQuitTest, decode) { test_decode(GetParam()); }

CodecParam<frame::Frame<message::client::Quit>> codec_frame_quit_param[] = {
    {"quit", {0, {}}, {}, {0x01, 0x00, 0x00, 0x00, 0x01}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFrameQuitTest,
                         ::testing::ValuesIn(codec_frame_quit_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// Frame::ResetConnection

using CodecFrameResetConnectionTest =
    CodecTest<frame::Frame<message::client::ResetConnection>>;

TEST_P(CodecFrameResetConnectionTest, encode) { test_encode(GetParam()); }
TEST_P(CodecFrameResetConnectionTest, decode) { test_decode(GetParam()); }

CodecParam<frame::Frame<message::client::ResetConnection>>
    codec_frame_resetconnection_param[] = {
        {"quit", {0, {}}, {}, {0x01, 0x00, 0x00, 0x00, 0x1f}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFrameResetConnectionTest,
                         ::testing::ValuesIn(codec_frame_resetconnection_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// Frame::Ping

using CodecFramePingTest = CodecTest<frame::Frame<message::client::Ping>>;

TEST_P(CodecFramePingTest, encode) { test_encode(GetParam()); }
TEST_P(CodecFramePingTest, decode) { test_decode(GetParam()); }

CodecParam<frame::Frame<message::client::Ping>> codec_frame_ping_param[] = {
    {"ping", {0, {}}, {}, {0x01, 0x00, 0x00, 0x00, 0x0e}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFramePingTest,
                         ::testing::ValuesIn(codec_frame_ping_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });
// Frame::StmtClose

using CodecFrameStmtCloseTest =
    CodecTest<frame::Frame<message::client::StmtClose>>;

TEST_P(CodecFrameStmtCloseTest, encode) { test_encode(GetParam()); }
TEST_P(CodecFrameStmtCloseTest, decode) { test_decode(GetParam()); }

CodecParam<frame::Frame<message::client::StmtClose>>
    codec_frame_stmtclose_param[] = {
        {"stmt_close",
         {0, {1}},
         {},
         {0x05, 0x00, 0x00, 0x00, 0x19, 0x01, 0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFrameStmtCloseTest,
                         ::testing::ValuesIn(codec_frame_stmtclose_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// Frame::StmtReset

using CodecFrameStmtResetTest =
    CodecTest<frame::Frame<message::client::StmtReset>>;

TEST_P(CodecFrameStmtResetTest, encode) { test_encode(GetParam()); }
TEST_P(CodecFrameStmtResetTest, decode) { test_decode(GetParam()); }

CodecParam<frame::Frame<message::client::StmtReset>>
    codec_frame_stmtreset_param[] = {
        {"stmt_reset",
         {0, {1}},
         {},
         {0x05, 0x00, 0x00, 0x00, 0x1a, 0x01, 0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFrameStmtResetTest,
                         ::testing::ValuesIn(codec_frame_stmtreset_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
