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

#include "mysqlrouter/classic_protocol.h"

#include <array>

#include <gtest/gtest.h>

// constexpr checks
namespace cl = classic_protocol;

static_assert(cl::wire::FixedInt<1>(1).value() == 1);
static_assert(cl::wire::FixedInt<2>(1).value() == 1);
static_assert(cl::wire::FixedInt<3>(1).value() == 1);
static_assert(cl::wire::FixedInt<4>(1).value() == 1);

static_assert(cl::Codec<cl::wire::FixedInt<1>>::size() == 1);
static_assert(cl::Codec<cl::wire::FixedInt<2>>::size() == 2);
static_assert(cl::Codec<cl::wire::FixedInt<3>>::size() == 3);
static_assert(cl::Codec<cl::wire::FixedInt<4>>::size() == 4);
static_assert(cl::Codec<cl::wire::FixedInt<8>>::size() == 8);

static_assert(cl::Codec<cl::wire::VarInt>(1, {}).size() == 1);

static_assert(cl::Codec<cl::wire::VarInt>(251, {}).size() == 1 + 2);
static_assert(cl::Codec<cl::wire::VarInt>(1 << 16, {}).size() == 1 + 3);
static_assert(cl::Codec<cl::wire::VarInt>(1 << 24, {}).size() == 1 + 8);

TEST(ConstExpr, VarInt) {
  constexpr uint64_t val = 1 << 24;
  constexpr cl::Codec<cl::wire::VarInt> codec(val, {});
  std::array<uint8_t, codec.size()> storage;

  EXPECT_EQ(codec.encode(net::buffer(storage)),
            (stdx::expected<size_t, std::error_code>(8 + 1)));
}

// message::client

static_assert(cl::Codec<cl::message::client::Quit>({}, {}).size() == 1);

static_assert(cl::Codec<cl::borrowed::message::client::InitSchema>({"foo"}, {})
                  .size() == 1 + 3);

#if 0
// std::vector<> isn't constexpr.
static_assert(cl::Codec<cl::borrowed::message::client::Query>({"foo"}, {})
                  .size() == 1 + 3);
#endif

static_assert(cl::Codec<cl::borrowed::message::client::ListFields>({"foo", ""},
                                                                   {})
                  .size() == 1            // cmd-byte
                                 + 3 + 1  // foo\0
                                 + 0      //
);

#if 0
// std::bitset<>.to_ulong() isn't constexpr
static_assert(cl::Codec<cl::message::client::Reload>(
                  {classic_protocol::reload_cmds::value_type{}}, {})
                  .size() == 1        // cmd-byte
                                 + 1  // flags
);
#endif

static_assert(cl::Codec<cl::message::client::Statistics>({}, {}).size() == 1);

static_assert(cl::Codec<cl::message::client::Kill>({1}, {}).size() == 1 + 4);

static_assert(cl::Codec<cl::message::client::Ping>({}, {}).size() == 1);

#if 0
// std::bitset<>.test() isn't constexpr
static_assert(cl::Codec<cl::borrowed::message::client::ChangeUser>(
                  {"user", "", "", 0xff, "", ""}, {})
                  .size() == 1 + 4 + 1 + 1 + 1 + 1);
#endif

static_assert(cl::Codec<cl::borrowed::message::client::StmtPrepare>({"stmt"},
                                                                    {})
                  .size() == 1 + 4);

#if 0
// std::vector<> isn't constexpr
static_assert(cl::Codec<cl::message::client::StmtExecute>({}, {}).size() == 1);
#endif

static_assert(cl::Codec<cl::borrowed::message::client::StmtParamAppendData>(
                  {1, 0, "foo"}, {})
                  .size() == 1 + 4 + 2 + 3);
static_assert(cl::Codec<cl::message::client::StmtClose>({1}, {}).size() ==
              1 + 4);

static_assert(cl::Codec<cl::message::client::StmtReset>({1}, {}).size() ==
              1 + 4);

static_assert(cl::Codec<cl::message::client::SetOption>({1}, {}).size() ==
              1 + 2);

static_assert(cl::Codec<cl::message::client::StmtFetch>({1, 1}, {}).size() ==
              1 + 4 + 4);

static_assert(
    cl::Codec<cl::borrowed::message::client::BinlogDump>({{}, 1, "foo", 4}, {})
        .size() == 1 + 4 + 2 + 4 + 3);

static_assert(cl::Codec<cl::borrowed::message::client::BinlogDumpGtid>(
                  {{}, 1, "foo", 0, ""}, {})
                  .size() == 1        // cmd-byte
                                 + 2  // flags
                                 + 4  // server-id
                                 + 4  // file-name size
                                 + 3  // file-name
                                 + 8  // position
                                 + 0  // sids
);

static_assert(cl::Codec<cl::borrowed::message::client::RegisterReplica>(
                  {1, "host", "user", "pass", 3306, 1, 1}, {})
                  .size() == 1            // cmd-byte
                                 + 4      // server-id
                                 + 1 + 4  // len + hostname
                                 + 1 + 4  // len + username
                                 + 1 + 4  // len + password
                                 + 2      // port
                                 + 4      // replication-rank
                                 + 4      // master-id
);

static_assert(cl::Codec<cl::message::client::ResetConnection>({}, {}).size() ==
              1);

static_assert(cl::Codec<cl::message::client::Clone>({}, {}).size() == 1);

static_assert(cl::Codec<cl::borrowed::message::client::SendFile>({"filedata"},
                                                                 {})
                  .size() == 8);
#if 0
// std::bitset<>.operator& isn't constexpr.
static_assert(
    cl::Codec<cl::borrowed::message::client::Greeting>(
        {{}, 0, 0, "username", "authdata", "schema", "authname", "attrs"}, {})
        .size() == 1 + 8);
#endif

static_assert(cl::Codec<cl::borrowed::message::client::AuthMethodData>({"data"},
                                                                       {})
                  .size() == 4);

//
// message::server
//

// Ok
#if 0
// std::bitset<>.test() isn't constexpr
static_assert(
    cl::Codec<cl::borrowed::message::server::Ok>({0, 0, {}, 0, "", ""}, {})
        .size() == 1 + 1 + 1 + 2 + 2);
#endif

// Error
#if 0
// std::bitset<>.test() isn't constexpr
static_assert(
    cl::Codec<cl::borrowed::message::server::Error>({1, "data", "HY000"}, {})
        .size() == 1 + 2 + 4 + 5);
#endif

// Eof
#if 0
// std::bitset<>.test() isn't constexpr
static_assert(
    cl::Codec<cl::borrowed::message::server::Eof>({0, 0, {}, 0, "", ""}, {})
        .size() == 1 + 1 + 1 + 2 + 2);
#endif

#if 0
// std::bitset<>.test() isn't constexpr
static_assert(cl::Codec<cl::borrowed::message::server::Greeting>(
                  {10, "8.0.32", 1, "", {}, 0xff, {}, ""}, {})
                  .size() == 1);
#endif

static_assert(cl::Codec<cl::message::server::ColumnCount>({1}, {}).size() == 1);

// ColumnMeta
// std::vector<> isn't constexpr

#if 0
// std::bitset<>.test() isn't constexpr
static_assert(cl::Codec<cl::borrowed::message::server::AuthMethodSwitch>(
                  {"meth", "data"}, {})
                  .size() == 1 + 4 + 4);
#endif

static_assert(cl::Codec<cl::borrowed::message::server::AuthMethodData>({"abc"},
                                                                       {})
                  .size() == 1 + 3);

static_assert(cl::Codec<cl::borrowed::message::server::SendFileRequest>({"abc"},
                                                                        {})
                  .size() == 1 + 3);

// Row
// std::vector<> isn't constexpr

// StmtRow
// std::vector<> isn't constexpr

#if 0
// std::bitset<>.test() isn't constexpr
static_assert(cl::Codec<cl::message::server::StmtPrepareOk>({1, 1, 1, 1, 1}, {})
                  .size() == 1 + 4 + 2 + 2 + 1 + 2 + 0);
#endif

static_assert(cl::Codec<cl::borrowed::message::server::Statistics>({"abc"}, {})
                  .size() == 3);

//
// header
//

// check 'constexpr' frame::Header.payload_size()
static_assert(cl::frame::Header(0, 0).payload_size() == 0);
// check 'constexpr' frame::Header.seq_id()
static_assert(cl::frame::Header(0, 0).seq_id() == 0);

// check 'constexpr' encoded frame::Header .size()
static_assert(
    cl::Codec<cl::frame::Header>(cl::frame::Header{0, 0}, {}).size() == 4);

// check 'constexpr' .seq_id() of ping-frame works
static_assert(
    cl::frame::Frame<cl::message::client::Ping>(0, cl::message::client::Ping{})
        .seq_id() == 0);

// check 'constexpr' .size() for a encoded ping-frame works
static_assert(cl::Codec<cl::frame::Frame<cl::message::client::Ping>>(
                  cl::frame::Frame<cl::message::client::Ping>(
                      0, cl::message::client::Ping{}),
                  {})
                  .size() == 5);

static_assert(cl::Codec<cl::message::client::Quit>::cmd_byte() == 0x01);
static_assert(cl::Codec<cl::message::client::InitSchema>::cmd_byte() == 0x02);
static_assert(cl::Codec<cl::message::client::Query>::cmd_byte() == 0x03);
static_assert(cl::Codec<cl::message::client::ListFields>::cmd_byte() == 0x04);
// 0x05 - CreateDb
// 0x06 - DropDb
static_assert(cl::Codec<cl::message::client::Reload>::cmd_byte() == 0x07);
// 0x08 - shutdown
static_assert(cl::Codec<cl::message::client::Statistics>::cmd_byte() == 0x09);
// 0x0a - ProcessInfo
// 0x0b - Connect
static_assert(cl::Codec<cl::message::client::Kill>::cmd_byte() == 0x0c);
// 0x0d - Debug
static_assert(cl::Codec<cl::message::client::Ping>::cmd_byte() == 0x0e);
// 0x0f - Time
// 0x10 - DelayedInsert
static_assert(cl::Codec<cl::message::client::ChangeUser>::cmd_byte() == 0x11);
// 0x12 - BinlogDump
// 0x13 - TableDump
// 0x14 - ConnectOut
// 0x15 - RegisterSlave
static_assert(cl::Codec<cl::message::client::StmtPrepare>::cmd_byte() == 0x16);
static_assert(cl::Codec<cl::message::client::StmtExecute>::cmd_byte() == 0x17);
static_assert(cl::Codec<cl::message::client::StmtParamAppendData>::cmd_byte() ==
              0x18);
static_assert(cl::Codec<cl::message::client::StmtClose>::cmd_byte() == 0x19);
static_assert(cl::Codec<cl::message::client::StmtReset>::cmd_byte() == 0x1a);
static_assert(cl::Codec<cl::message::client::SetOption>::cmd_byte() == 0x1b);

static_assert(cl::Codec<cl::message::client::StmtFetch>::cmd_byte() == 0x1c);

// 0x1d - Daemon
// 0x1e - BinlogDumpGtid

static_assert(cl::Codec<cl::message::client::ResetConnection>::cmd_byte() ==
              0x1f);

// 0x20 - Clone

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
