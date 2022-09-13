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

#include "mysqlrouter/classic_protocol.h"

#include <array>

#include <gtest/gtest.h>

// constexpr checks

static_assert(classic_protocol::wire::FixedInt<1>(1).value() == 1);
static_assert(classic_protocol::wire::FixedInt<2>(1).value() == 1);
static_assert(classic_protocol::wire::FixedInt<3>(1).value() == 1);
static_assert(classic_protocol::wire::FixedInt<4>(1).value() == 1);

static_assert(classic_protocol::Codec<classic_protocol::wire::FixedInt<1>>(1,
                                                                           {})
                  .size() == 1);
static_assert(classic_protocol::Codec<classic_protocol::wire::FixedInt<2>>(1,
                                                                           {})
                  .size() == 2);
static_assert(classic_protocol::Codec<classic_protocol::wire::FixedInt<3>>(1,
                                                                           {})
                  .size() == 3);
static_assert(classic_protocol::Codec<classic_protocol::wire::FixedInt<4>>(1,
                                                                           {})
                  .size() == 4);
static_assert(classic_protocol::Codec<classic_protocol::wire::FixedInt<8>>(1,
                                                                           {})
                  .size() == 8);

static_assert(
    classic_protocol::Codec<classic_protocol::wire::VarInt>(1, {}).size() == 1);

static_assert(classic_protocol::Codec<classic_protocol::wire::VarInt>(251, {})
                  .size() == 1 + 2);
static_assert(classic_protocol::Codec<classic_protocol::wire::VarInt>(1 << 16,
                                                                      {})
                  .size() == 1 + 3);
static_assert(classic_protocol::Codec<classic_protocol::wire::VarInt>(1 << 24,
                                                                      {})
                  .size() == 1 + 8);

TEST(ConstExpr, VarInt) {
  constexpr uint64_t val = 1 << 24;
  constexpr classic_protocol::Codec<classic_protocol::wire::VarInt> codec(val,
                                                                          {});
  std::array<uint8_t, codec.size()> storage;

  EXPECT_EQ(codec.encode(net::buffer(storage)),
            (stdx::expected<size_t, std::error_code>(8 + 1)));
}

static_assert(
    classic_protocol::Codec<classic_protocol::message::client::Quit>({}, {})
        .size() == 1);

static_assert(
    classic_protocol::Codec<classic_protocol::message::client::ResetConnection>(
        {}, {})
        .size() == 1);

static_assert(
    classic_protocol::Codec<classic_protocol::message::client::Ping>({}, {})
        .size() == 1);

static_assert(
    classic_protocol::Codec<classic_protocol::message::client::Statistics>({},
                                                                           {})
        .size() == 1);

// check 'constexpr' frame::Header.payload_size()
static_assert(classic_protocol::frame::Header(0, 0).payload_size() == 0);
// check 'constexpr' frame::Header.seq_id()
static_assert(classic_protocol::frame::Header(0, 0).seq_id() == 0);

// check 'constexpr' encoded frame::Header .size()
static_assert(classic_protocol::Codec<classic_protocol::frame::Header>(
                  classic_protocol::frame::Header{0, 0}, {})
                  .size() == 4);

// check 'constexpr' .seq_id() of ping-frame works
static_assert(
    classic_protocol::frame::Frame<classic_protocol::message::client::Ping>(
        0, classic_protocol::message::client::Ping{})
        .seq_id() == 0);

// check 'constexpr' .size() for a encoded ping-frame works
static_assert(
    classic_protocol::Codec<classic_protocol::frame::Frame<
        classic_protocol::message::client::Ping>>(
        classic_protocol::frame::Frame<classic_protocol::message::client::Ping>(
            0, classic_protocol::message::client::Ping{}),
        {})
        .size() == 5);

static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::Quit>::cmd_byte() == 0x01);
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::InitSchema>::cmd_byte() ==
              0x02);
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::Query>::cmd_byte() ==
              0x03);
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::ListFields>::cmd_byte() ==
              0x04);
// 0x05 - CreateDb
// 0x06 - DropDb
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::Reload>::cmd_byte() ==
              0x07);
// 0x08 - shutdown
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::Statistics>::cmd_byte() ==
              0x09);
// 0x0a - ProcessInfo
// 0x0b - Connect
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::Kill>::cmd_byte() == 0x0c);
// 0x0d - Debug
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::Ping>::cmd_byte() == 0x0e);
// 0x0f - Time
// 0x10 - DelayedInsert
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::ChangeUser>::cmd_byte() ==
              0x11);
// 0x12 - BinlogDump
// 0x13 - TableDump
// 0x14 - ConnectOut
// 0x15 - RegisterSlave
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::StmtPrepare>::cmd_byte() ==
              0x16);
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::StmtExecute>::cmd_byte() ==
              0x17);
static_assert(
    classic_protocol::Codec<
        classic_protocol::message::client::StmtParamAppendData>::cmd_byte() ==
    0x18);
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::StmtClose>::cmd_byte() ==
              0x19);
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::StmtReset>::cmd_byte() ==
              0x1a);
static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::SetOption>::cmd_byte() ==
              0x1b);

static_assert(classic_protocol::Codec<
                  classic_protocol::message::client::StmtFetch>::cmd_byte() ==
              0x1c);

// 0x1d - Daemon
// 0x1e - BinlogDumpGtid

static_assert(
    classic_protocol::Codec<
        classic_protocol::message::client::ResetConnection>::cmd_byte() ==
    0x1f);

// 0x20 - Clone

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
