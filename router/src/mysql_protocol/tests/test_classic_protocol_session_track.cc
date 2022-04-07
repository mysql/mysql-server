/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#include "test_classic_protocol_codec.h"

// string_literals are supposed to solve the same problem, but they are broken
// on dev-studio 12.6

#define S(x) std::string((x), sizeof(x) - 1)

// session_track::Schema

using CodecSessiontrackSchemaTest =
    CodecTest<classic_protocol::session_track::Schema>;

TEST_P(CodecSessiontrackSchemaTest, encode) { test_encode(GetParam()); }
TEST_P(CodecSessiontrackSchemaTest, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::session_track::Schema>
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

CodecParam<classic_protocol::session_track::TransactionState>
    codec_sessiontrack_transactionstate_param[] = {
        {"all_flags_explicit_trx",
         {'T', 'r', 'R', 'w', 'W', 's', 'S', 'L'},
         {},
         {'T', 'r', 'R', 'w', 'W', 's', 'S', 'L'}},
        {"all_flags_implicit_trx",
         {'I', 'r', 'R', 'w', 'W', 's', 'S', 'L'},
         {},
         {'I', 'r', 'R', 'w', 'W', 's', 'S', 'L'}},
        {"no_flags",
         {'_', '_', '_', '_', '_', '_', '_', '_'},
         {},
         {'_', '_', '_', '_', '_', '_', '_', '_'}},
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

CodecParam<classic_protocol::session_track::State>
    codec_sessiontrack_state_param[] = {
        {"1", {"1"}, {}, {0x01, '1'}},
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

CodecParam<classic_protocol::session_track::SystemVariable>
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

using CodecSessiontrackFieldTest =
    CodecTest<classic_protocol::session_track::Field>;

TEST_P(CodecSessiontrackFieldTest, encode) { test_encode(GetParam()); }
TEST_P(CodecSessiontrackFieldTest, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::session_track::Field>
    codec_sessiontrack_field_param[] = {
        {"with_session_state_info",
         {0, S("\nautocommit\2ON")},
         {},
         {0x00, 0x0e, 0x0a, 'a', 'u', 't', 'o', 'c', 'o', 'm', 'm', 'i', 't',
          0x02, 'O', 'N'}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecSessiontrackFieldTest,
                         ::testing::ValuesIn(codec_sessiontrack_field_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
