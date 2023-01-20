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

#include "mysqlrouter/classic_protocol_codec_session_track.h"

#include <gtest/gtest.h>

#include "mysqlrouter/classic_protocol_session_track.h"
#include "test_classic_protocol_codec.h"

// string_literals are supposed to solve the same problem, but they are broken
// on dev-studio 12.6

#define S(x) std::string((x), sizeof(x) - 1)

namespace cl = classic_protocol;

static_assert(cl::Codec<cl::borrowed::session_track::Field>({1, "abc"}, {})
                  .size() == 1 + 4);

static_assert(
    cl::Codec<cl::borrowed::session_track::SystemVariable>({"key", "var"}, {})
        .size() == 1 + 3 + 1 + 3);

static_assert(cl::Codec<cl::borrowed::session_track::Schema>({"var"}, {})
                  .size() == 1 + 3);

static_assert(cl::Codec<cl::session_track::State>({1}, {}).size() == 1);

static_assert(cl::Codec<cl::borrowed::session_track::Gtid>({1, "gtid"}, {})
                  .size() == 1 + 1 + 4);

static_assert(
    cl::Codec<cl::session_track::TransactionState>({1, 1, 1, 1, 1, 1, 1, 1}, {})
        .size() == 1 + 8);

static_assert(
    cl::Codec<cl::borrowed::session_track::TransactionCharacteristics>(
        {"SET foo"}, {})
        .size() == 1 + 7);

// session_track::Schema

using CodecSessiontrackSchemaTest =
    CodecTest<classic_protocol::session_track::Schema>;

TEST_P(CodecSessiontrackSchemaTest, encode) { test_encode(GetParam()); }
TEST_P(CodecSessiontrackSchemaTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::session_track::Schema>
    codec_sessiontrack_schema_param[] = {
        {"foo", {"foo"}, {}, {0x03, 'f', 'o', 'o'}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecSessiontrackSchemaTest,
                         ::testing::ValuesIn(codec_sessiontrack_schema_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// session_track::TransactionState

using CodecSessiontrackTransactionStateTest =
    CodecTest<classic_protocol::session_track::TransactionState>;

TEST_P(CodecSessiontrackTransactionStateTest, encode) {
  test_encode(GetParam());
}
TEST_P(CodecSessiontrackTransactionStateTest, decode) {
  test_decode(GetParam());
}

const CodecParam<classic_protocol::session_track::TransactionState>
    codec_sessiontrack_transactionstate_param[] = {
        {"all_flags_explicit_trx",
         {'T', 'r', 'R', 'w', 'W', 's', 'S', 'L'},
         {},
         {0x08, 'T', 'r', 'R', 'w', 'W', 's', 'S', 'L'}},
        {"all_flags_implicit_trx",
         {'I', 'r', 'R', 'w', 'W', 's', 'S', 'L'},
         {},
         {0x08, 'I', 'r', 'R', 'w', 'W', 's', 'S', 'L'}},
        {"no_flags",
         {'_', '_', '_', '_', '_', '_', '_', '_'},
         {},
         {0x08, '_', '_', '_', '_', '_', '_', '_', '_'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecSessiontrackTransactionStateTest,
    ::testing::ValuesIn(codec_sessiontrack_transactionstate_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// session_track::State

using CodecSessiontrackStateTest =
    CodecTest<classic_protocol::session_track::State>;

TEST_P(CodecSessiontrackStateTest, encode) { test_encode(GetParam()); }
TEST_P(CodecSessiontrackStateTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::session_track::State>
    codec_sessiontrack_state_param[] = {
        {
            "1",    // testname
            {'1'},  // decoded
            {},     // caps
            {'1'}   // encoded
        },
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecSessiontrackStateTest,
                         ::testing::ValuesIn(codec_sessiontrack_state_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// session_track::SystemVariable

using CodecSessiontrackSystemVariableTest =
    CodecTest<classic_protocol::session_track::SystemVariable>;

TEST_P(CodecSessiontrackSystemVariableTest, encode) { test_encode(GetParam()); }
TEST_P(CodecSessiontrackSystemVariableTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::session_track::SystemVariable>
    codec_sessiontrack_systemvariable_param[] = {
        {"autocommit_on",
         {"autocommit", "ON"},
         {},
         {0x0a, 'a', 'u', 't', 'o', 'c', 'o', 'm', 'm', 'i', 't', 0x02, 'O',
          'N'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecSessiontrackSystemVariableTest,
    ::testing::ValuesIn(codec_sessiontrack_systemvariable_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// session_track::Field
//
// the session-track info contains of zero-or-more session_track::Fields

using CodecSessiontrackFieldTest =
    CodecTest<classic_protocol::session_track::Field>;

TEST_P(CodecSessiontrackFieldTest, encode) { test_encode(GetParam()); }
TEST_P(CodecSessiontrackFieldTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::session_track::Field>
    codec_sessiontrack_field_param[] = {
        {"with_session_state_info",
         {0,                       // SessionState
          S("\nautocommit\2ON")},  // decoded
         {},                       // caps
         {0x00, 0x0e, 0x0a, 'a', 'u', 't', 'o', 'c', 'o', 'm', 'm', 'i', 't',
          0x02, 'O', 'N'}},  // encoded
        {"with_gtid",
         // decoded
         {3,  // Gtid
          S("\x00\x2a\x34\x64\x64\x30\x66\x39\x64\x35\x2d\x33\x62\x30\x30\x2d"
            "\x31\x31\x65\x62\x2d\x61\x64\x37\x30\x2d\x30\x30\x33\x30\x39\x33"
            "\x31\x34\x30\x65\x34\x65\x3a\x32\x33\x39\x32\x39")},
         {},  // caps
              // encoded
         {0x03, 0x2c, 0x00, 0x2a, 0x34, 0x64, 0x64, 0x30, 0x66, 0x39,
          0x64, 0x35, 0x2d, 0x33, 0x62, 0x30, 0x30, 0x2d, 0x31, 0x31,
          0x65, 0x62, 0x2d, 0x61, 0x64, 0x37, 0x30, 0x2d, 0x30, 0x30,
          0x33, 0x30, 0x39, 0x33, 0x31, 0x34, 0x30, 0x65, 0x34, 0x65,
          0x3a, 0x32, 0x33, 0x39, 0x32, 0x39}},
        {"with_characteristics",
         // decoded
         {5,  // Characteristics
          S("\x08\x5f\x5f\x5f\x5f\x5f\x5f\x5f\x5f")},
         {},  // caps
              // encoded
         {0x05, 0x09, 0x08, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecSessiontrackFieldTest,
                         ::testing::ValuesIn(codec_sessiontrack_field_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// session_track::Gtid

using CodecSessiontrackGtidTest =
    CodecTest<classic_protocol::session_track::Gtid>;

TEST_P(CodecSessiontrackGtidTest, encode) { test_encode(GetParam()); }
TEST_P(CodecSessiontrackGtidTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::session_track::Gtid>
    codec_sessiontrack_gtid_param[] = {
        {"a_gtid",
         {0, S("4dd0f9d5-3b00-11eb-ad70-003093140e4e:23929")},
         {},
         {0x00, 0x2a, 0x34, 0x64, 0x64, 0x30, 0x66, 0x39, 0x64, 0x35, 0x2d,
          0x33, 0x62, 0x30, 0x30, 0x2d, 0x31, 0x31, 0x65, 0x62, 0x2d, 0x61,
          0x64, 0x37, 0x30, 0x2d, 0x30, 0x30, 0x33, 0x30, 0x39, 0x33, 0x31,
          0x34, 0x30, 0x65, 0x34, 0x65, 0x3a, 0x32, 0x33, 0x39, 0x32, 0x39}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecSessiontrackGtidTest,
                         ::testing::ValuesIn(codec_sessiontrack_gtid_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
