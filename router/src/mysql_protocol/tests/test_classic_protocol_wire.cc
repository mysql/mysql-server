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

#include "mysqlrouter/classic_protocol_codec_wire.h"

#include <gtest/gtest.h>

#include "test_classic_protocol_codec.h"

// wire::FixedInt<1>

using CodecFixedInt1Test = CodecTest<classic_protocol::wire::FixedInt<1>>;

TEST_P(CodecFixedInt1Test, encode) { test_encode(GetParam()); }
TEST_P(CodecFixedInt1Test, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::wire::FixedInt<1>> codec_wire_fixedint_1_param[] =
    {
        {"1", {1}, {}, {0x01}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFixedInt1Test,
                         ::testing::ValuesIn(codec_wire_fixedint_1_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });
// wire::FixedInt<2>

using CodecFixedInt2Test = CodecTest<classic_protocol::wire::FixedInt<2>>;

TEST_P(CodecFixedInt2Test, encode) { test_encode(GetParam()); }
TEST_P(CodecFixedInt2Test, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::wire::FixedInt<2>> codec_wire_fixedint_2_param[] =
    {
        {"1", {1}, {}, {0x01, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFixedInt2Test,
                         ::testing::ValuesIn(codec_wire_fixedint_2_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// wire::FixedInt<3>

using CodecFixedInt3Test = CodecTest<classic_protocol::wire::FixedInt<3>>;

TEST_P(CodecFixedInt3Test, encode) { test_encode(GetParam()); }
TEST_P(CodecFixedInt3Test, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::wire::FixedInt<3>> codec_wire_fixedint_3_param[] =
    {
        {"1", {1}, {}, {0x01, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFixedInt3Test,
                         ::testing::ValuesIn(codec_wire_fixedint_3_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// wire::FixedInt<4>

using CodecFixedInt4Test = CodecTest<classic_protocol::wire::FixedInt<4>>;

TEST_P(CodecFixedInt4Test, encode) { test_encode(GetParam()); }
TEST_P(CodecFixedInt4Test, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::wire::FixedInt<4>> codec_wire_fixedint_4_param[] =
    {
        {"1", {1}, {}, {0x01, 0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFixedInt4Test,
                         ::testing::ValuesIn(codec_wire_fixedint_4_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// wire::FixedInt<8>

using CodecFixedInt8Test = CodecTest<classic_protocol::wire::FixedInt<8>>;

TEST_P(CodecFixedInt8Test, encode) { test_encode(GetParam()); }
TEST_P(CodecFixedInt8Test, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::wire::FixedInt<8>> codec_wire_fixedint_8_param[] =
    {
        {"1", {1}, {}, {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecFixedInt8Test,
                         ::testing::ValuesIn(codec_wire_fixedint_8_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// wire::String

using CodecStringTest = CodecTest<classic_protocol::wire::String>;

TEST_P(CodecStringTest, encode) { test_encode(GetParam()); }
TEST_P(CodecStringTest, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::wire::String> codec_wire_string_param[] = {
    {"1", {"1"}, {}, {'1'}},
    {"with_nul", {std::string("\0\0\0", 3)}, {}, {0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecStringTest,
                         ::testing::ValuesIn(codec_wire_string_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// wire::NulTermString

using CodecWireNulTermStringTest =
    CodecTest<classic_protocol::wire::NulTermString>;

TEST_P(CodecWireNulTermStringTest, encode) { test_encode(GetParam()); }
TEST_P(CodecWireNulTermStringTest, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::wire::NulTermString>
    codec_wire_nulltermstring_param[] = {
        {"1", {"1"}, {}, {'1', '\0'}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecWireNulTermStringTest,
                         ::testing::ValuesIn(codec_wire_nulltermstring_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

using CodecWireNulTermStringFailTest =
    CodecFailTest<classic_protocol::wire::NulTermString>;

TEST_P(CodecWireNulTermStringFailTest, decode) { test_decode(GetParam()); }

CodecFailParam codec_wire_nulltermstring_fail_param[] = {
    {"empty", {}, {}, classic_protocol::codec_errc::missing_nul_term},
    {"no_nul_term", {'1'}, {}, classic_protocol::codec_errc::missing_nul_term},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecWireNulTermStringFailTest,
    ::testing::ValuesIn(codec_wire_nulltermstring_fail_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// wire::VarString

using CodecWireVarStringTest = CodecTest<classic_protocol::wire::VarString>;

TEST_P(CodecWireVarStringTest, encode) { test_encode(GetParam()); }
TEST_P(CodecWireVarStringTest, decode) { test_decode(GetParam()); }

CodecParam<classic_protocol::wire::VarString> codec_wire_varstring_param[] = {
    {"1", {"1"}, {}, {0x01, '1'}},
    {"with_nul", {std::string("\0\0\0", 3)}, {}, {0x03, 0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecWireVarStringTest,
                         ::testing::ValuesIn(codec_wire_varstring_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
