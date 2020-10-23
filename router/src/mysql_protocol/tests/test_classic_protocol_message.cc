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

#include "mysqlrouter/classic_protocol_codec_message.h"

#include <list>
#include <vector>

#include <gtest/gtest.h>

#include "test_classic_protocol_codec.h"

// string_literals are supposed to solve the same problem, but they are broken
// on dev-studio 12.6

#define S(x) std::string((x), sizeof(x) - 1)

// server::AuthMethodSwitch
namespace classic_protocol {
namespace message {
namespace server {
std::ostream &operator<<(std::ostream &os, const AuthMethodSwitch &v) {
  os << v.auth_method() << ", " << v.auth_method_data();
  return os;
}
}  // namespace server
}  // namespace message
}  // namespace classic_protocol

using CodecMessageServerAuthMethodSwitchTest =
    CodecTest<classic_protocol::message::server::AuthMethodSwitch>;

TEST_P(CodecMessageServerAuthMethodSwitchTest, encode) {
  test_encode(GetParam());
}
TEST_P(CodecMessageServerAuthMethodSwitchTest, decode) {
  test_decode(GetParam());
}

const CodecParam<classic_protocol::message::server::AuthMethodSwitch>
    codec_message_server_authmethodswitch_param[] = {
        {"4_0", {}, {}, {0xfe}},
        {"5_6",
         {"mysql_native_password", S("zQg4i6oNy6=rHN/>-b)A\0")},
         classic_protocol::capabilities::plugin_auth,
         {0xfe, 0x6d, 0x79, 0x73, 0x71, 0x6c, 0x5f, 0x6e, 0x61, 0x74, 0x69,
          0x76, 0x65, 0x5f, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64,
          0x00, 0x7a, 0x51, 0x67, 0x34, 0x69, 0x36, 0x6f, 0x4e, 0x79, 0x36,
          0x3d, 0x72, 0x48, 0x4e, 0x2f, 0x3e, 0x2d, 0x62, 0x29, 0x41, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerAuthMethodSwitchTest,
    ::testing::ValuesIn(codec_message_server_authmethodswitch_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// server::AuthMethodData
namespace classic_protocol {
namespace message {
namespace server {
std::ostream &operator<<(std::ostream &os, const AuthMethodData &v) {
  os << static_cast<uint16_t>(v.packet_type()) << ", " << v.auth_method_data();
  return os;
}
}  // namespace server
}  // namespace message
}  // namespace classic_protocol

using CodecMessageServerAuthMethodDataTest =
    CodecTest<classic_protocol::message::server::AuthMethodData>;

TEST_P(CodecMessageServerAuthMethodDataTest, encode) {
  test_encode(GetParam());
}
TEST_P(CodecMessageServerAuthMethodDataTest, decode) {
  test_decode(GetParam());
}

const CodecParam<classic_protocol::message::server::AuthMethodData>
    codec_message_server_authmethoddata_param[] = {
        {"auth_fast_ack", {0x03, ""}, {}, {0x03}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerAuthMethodDataTest,
    ::testing::ValuesIn(codec_message_server_authmethoddata_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// server::Ok

using CodecMessageServerOkTest =
    CodecTest<classic_protocol::message::server::Ok>;

TEST_P(CodecMessageServerOkTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerOkTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::Ok>
    codec_message_server_ok_param[] = {
        {"3_23", {1, 3, 0, 0}, {}, {0x00, 0x01, 0x03}},
        {"4_0",
         {1, 3, classic_protocol::status::autocommit, 0},
         classic_protocol::capabilities::transactions,
         {0x00, 0x01, 0x03, 0x02, 0x00}},
        {"4_1",
         {1, 3, classic_protocol::status::autocommit, 4},
         classic_protocol::capabilities::protocol_41,
         {0x00, 0x01, 0x03, 0x02, 0x00, 0x04, 0x00}},
        {"with_session_state_info",
         {1,
          3,
          classic_protocol::status::autocommit |
              classic_protocol::status::session_state_changed,
          4,
          {},  // no message
          {S("\0\16\nautocommit\2ON")}},
         classic_protocol::capabilities::protocol_41 |
             classic_protocol::capabilities::session_track,
         {0x00, 0x01, 0x03, 0x02, 0x40, 0x04, 0x00, 0x00, 0x10,
          0x00, 0x0e, 0x0a, 'a',  'u',  't',  'o',  'c',  'o',
          'm',  'm',  'i',  't',  0x02, 'O',  'N'}},
        {"with_session_state_info_and_message",
         {1,
          3,
          classic_protocol::status::in_transaction |
              classic_protocol::status::no_index_used |
              classic_protocol::status::session_state_changed,
          4,
          "Rows matched: 0  Changed: 0  Warnings: 0",
          {S("\5\t\10I___Ws__")}},
         classic_protocol::capabilities::protocol_41 |
             classic_protocol::capabilities::session_track,
         {0x00, 0x01, 0x03, '!',  0x40, 0x04, 0x00, '(', 'R', 'o', 'w', 's',
          ' ',  'm',  'a',  't',  'c',  'h',  'e',  'd', ':', ' ', '0', ' ',
          ' ',  'C',  'h',  'a',  'n',  'g',  'e',  'd', ':', ' ', '0', ' ',
          ' ',  'W',  'a',  'r',  'n',  'i',  'n',  'g', 's', ':', ' ', '0',
          '\v', 0x05, '\t', 0x08, 'I',  '_',  '_',  '_', 'W', 's', '_', '_'}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageServerOkTest,
                         ::testing::ValuesIn(codec_message_server_ok_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// server::Eof

using CodecMessageServerEofTest =
    CodecTest<classic_protocol::message::server::Eof>;

TEST_P(CodecMessageServerEofTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerEofTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::Eof> codec_eof_param[] = {
    {"3_23", {}, {}, {0xfe}},
    {"4_1",
     {classic_protocol::status::more_results_exist |
          classic_protocol::status::autocommit,
      1},
     classic_protocol::capabilities::protocol_41,
     {0xfe, 0x01, 0x00, 0x0a, 0x00}},
    {"5_7",
     {classic_protocol::status::autocommit, 1},
     classic_protocol::capabilities::text_result_with_session_tracking |
         classic_protocol::capabilities::protocol_41,
     {0xfe, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageServerEofTest,
                         ::testing::ValuesIn(codec_eof_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// server::Error
namespace classic_protocol {
namespace message {
namespace server {
std::ostream &operator<<(std::ostream &os, Error const &v) {
  os << v.error_code() << ", " << v.sql_state() << ", " << v.message();
  return os;
}
}  // namespace server
}  // namespace message
}  // namespace classic_protocol

using CodecMessageServerErrorTest =
    CodecTest<classic_protocol::message::server::Error>;

TEST_P(CodecMessageServerErrorTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerErrorTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::Error>
    codec_message_server_error_param[] = {
        {"3_23",
         {1096, "No tables used", ""},
         {},
         {0xff, 0x48, 0x04, 'N', 'o', ' ', 't', 'a', 'b', 'l', 'e', 's', ' ',
          'u', 's', 'e', 'd'}},
        {"4_1",
         {1096, "No tables used", "HY000"},
         classic_protocol::capabilities::protocol_41,
         {0xff, 0x48, 0x04, 0x23, 'H', 'Y', '0', '0', '0', 'N', 'o', ' ',
          't',  'a',  'b',  'l',  'e', 's', ' ', 'u', 's', 'e', 'd'}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageServerErrorTest,
                         ::testing::ValuesIn(codec_message_server_error_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// server::Greeting

using CodecMessageServerGreetingTest =
    CodecTest<classic_protocol::message::server::Greeting>;

TEST_P(CodecMessageServerGreetingTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerGreetingTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::Greeting>
    codec_message_server_greeting_param[] = {
        {"3_20_protocol_9",
         {0x09, "5.6.4-m7-log", 2646, "RB3vz&Gr", 0x0, 0x0, 0x0, ""},
         {},
         {0x09, 0x35, 0x2e, 0x36, 0x2e, 0x34, 0x2d, 0x6d, 0x37,
          0x2d, 0x6c, 0x6f, 0x67, 0x00, 0x56, 0x0a, 0x00, 0x00,
          0x52, 0x42, 0x33, 0x76, 0x7a, 0x26, 0x47, 0x72, 0x00}},

        {"3_21_31",
         {0x0a, "3.21.31", 1, "-8pMne/X", 0b0000'0000'0000'1100, 0x0, 0x0, ""},
         {},
         {'\n', '3', '.', '2', '1', '.', '3', '1', '\0', '\1', '\0',  '\0',
          '\0', '-', '8', 'p', 'M', 'n', 'e', '/', 'X',  '\0', '\xc', '\0'}},
        {"3_23_49",
         {0x0a, "3.23.49a", 1, "-8pMne/X", 0b0000'0000'0010'1100,
          classic_protocol::collation::Latin1SwedishCi,
          classic_protocol::status::autocommit, ""},
         {},
         {'\n', '3',  '.',  '2',   '3',  '.',  '4',  '9',  'a',  '\0', '\1',
          '\0', '\0', '\0', '-',   '8',  'p',  'M',  'n',  'e',  '/',  'X',
          '\0', ',',  '\0', '\10', '\2', '\0', '\0', '\0', '\0', '\0', '\0',
          '\0', '\0', '\0', '\0',  '\0', '\0', '\0', '\0'}},
        {"4_0_24",
         {0x0a, "4.0.24", 1, "v;`PR,\"d", 0b0010'0000'0010'1100,
          classic_protocol::collation::Latin1SwedishCi,
          classic_protocol::status::autocommit, ""},
         {},
         {'\n', '4',  '.',  '0',   '.',  '2',  '4',  '\0', '\1', '\0',
          '\0', '\0', 'v',  ';',   '`',  'P',  'R',  ',',  '"',  'd',
          '\0', ',',  ' ',  '\10', '\2', '\0', '\0', '\0', '\0', '\0',
          '\0', '\0', '\0', '\0',  '\0', '\0', '\0', '\0', '\0'}},
        {"5_6_4",
         {0x0a, "5.6.4-m7-log", 2646, S("RB3vz&Gr+yD&/ZZ305ZG\0"), 0xc00fffff,
          0x8, 0x02, "mysql_native_password"},
         {},
         {0x0a, 0x35, 0x2e, 0x36, 0x2e, 0x34, 0x2d, 0x6d, 0x37, 0x2d,
          0x6c, 0x6f, 0x67, 0x00, 0x56, 0x0a, 0x00, 0x00, 0x52, 0x42,
          0x33, 0x76, 0x7a, 0x26, 0x47, 0x72, 0x00, 0xff, 0xff, 0x08,
          0x02, 0x00, 0x0f, 0xc0, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x2b, 0x79, 0x44, 0x26, 0x2f,
          0x5a, 0x5a, 0x33, 0x30, 0x35, 0x5a, 0x47, 0x00, 0x6d, 0x79,
          0x73, 0x71, 0x6c, 0x5f, 0x6e, 0x61, 0x74, 0x69, 0x76, 0x65,
          0x5f, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerGreetingTest,
    ::testing::ValuesIn(codec_message_server_greeting_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

using CodecMessageServerGreetingFailTest =
    CodecFailTest<classic_protocol::message::server::Greeting>;

TEST_P(CodecMessageServerGreetingFailTest, decode) { test_decode(GetParam()); }

CodecFailParam codec_message_server_greeting_fail_param[] = {
    {"too_short",
     {
         '\n',                                        // protocol
         '3',  '.', '2', '1', '.', '3', '1', 0,       // version
         1,    0,   0,   0,                           // packet-size
         '-',  '8', 'p', 'M', 'n', 'e', '/', 'X', 0,  // auth-method-data
         0xc                                          // fail: missing 2nd byte
     },
     {},
     classic_protocol::codec_errc::not_enough_input},
    {"empty", {}, {}, classic_protocol::codec_errc::not_enough_input},
    {"unknown_protocol_8",
     {8},
     {},
     classic_protocol::codec_errc::invalid_input},
    {"unknown_protocol_11",
     {11},
     {},
     classic_protocol::codec_errc::invalid_input},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerGreetingFailTest,
    ::testing::ValuesIn(codec_message_server_greeting_fail_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// server::ColumMeta

using CodecMessageServerColumnMetaTest =
    CodecTest<classic_protocol::message::server::ColumnMeta>;

TEST_P(CodecMessageServerColumnMetaTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerColumnMetaTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::ColumnMeta>
    codec_message_server_columnmeta_param[] = {
        {"3_21",
         {{},
          {},
          {},
          {},
          "1",
          {},
          0x0,
          1,
          classic_protocol::field_type::LongLong,
          0x0001,
          0x1f},
         {},
         {
             0,                 // table
             1, '1',            // column
             3, 0x1, 0x0, 0x0,  // column-length
             1, 0x8,            // type
             2, 0x1, 0x1f       // flags_and_decimal
         }},
        {"3_23",
         {{},
          {},
          {},
          {},
          "1",
          {},
          0x0,
          1,
          classic_protocol::field_type::LongLong,
          0x0001,
          0x1f},
         classic_protocol::capabilities::long_flag,
         {
             0,                 // table
             1, '1',            // column
             3, 0x1, 0x0, 0x0,  // column-length
             1, 0x8,            // type
             3, 0x1, 0x0, 0x1f  // flags_and_decimal
         }},
        {"4_1",
         {"def", "", "", "", "@@version_comment", "", 0xff, 112,
          classic_protocol::field_type::VarString, 0x0000, 0x1f},
         classic_protocol::capabilities::protocol_41,
         {3,    'd', 'e', 'f',  // catalog
          0,                    // schema
          0,                    // table
          0,                    // orig_table
          17,   '@', '@', 'v', 'e', 'r', 's', 'i', 'o',
          'n',  '_', 'c', 'o', 'm', 'm', 'e', 'n', 't',  // name
          0,                                             // orig_name
          12,                                            // other.length
          0xff, 0,                                       // other.collation
          'p',  0,   0,   0,                             // other.column_length
          0xfd,                                          // other.type
          0,    0,                                       // other.flags
          0x1f,                                          // other.decimals
          0,    0}}};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerColumnMetaTest,
    ::testing::ValuesIn(codec_message_server_columnmeta_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// server::Row

using CodecMessageServerRowTest =
    CodecTest<classic_protocol::message::server::Row>;

TEST_P(CodecMessageServerRowTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerRowTest, decode) { test_decode(GetParam()); }

using namespace std::string_literals;

const CodecParam<classic_protocol::message::server::Row>
    codec_message_server_row_param[] = {
        {"abc_def",
         {{"abc"s, "def"s}},
         {},
         {0x03, 'a', 'b', 'c', 0x03, 'd', 'e', 'f'}},
        {"null_null",
         {{stdx::make_unexpected(), stdx::make_unexpected()}},
         {},
         {0xfb, 0xfb}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageServerRowTest,
                         ::testing::ValuesIn(codec_message_server_row_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// server::StmtRow

using CodecMessageServerStmtRowTest =
    CodecTest<classic_protocol::message::server::StmtRow>;

TEST_P(CodecMessageServerStmtRowTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerStmtRowTest, decode) {
  test_decode(GetParam(), std::vector<classic_protocol::field_type::value_type>{
                              classic_protocol::field_type::VarString});
}

// client::Quit

using CodecMessageClientQuitTest =
    CodecTest<classic_protocol::message::client::Quit>;

TEST_P(CodecMessageClientQuitTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientQuitTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::Quit>
    codec_message_client_quit_param[] = {
        {"1", {}, {}, {0x01}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientQuitTest,
                         ::testing::ValuesIn(codec_message_client_quit_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::InitSchema

using CodecMessageClientInitSchemaTest =
    CodecTest<classic_protocol::message::client::InitSchema>;

TEST_P(CodecMessageClientInitSchemaTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientInitSchemaTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::InitSchema>
    codec_message_client_initschema_param[] = {
        {"schema", {"schema"}, {}, {0x02, 's', 'c', 'h', 'e', 'm', 'a'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientInitSchemaTest,
    ::testing::ValuesIn(codec_message_client_initschema_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::Query

using CodecMessageClientQueryTest =
    CodecTest<classic_protocol::message::client::Query>;

TEST_P(CodecMessageClientQueryTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientQueryTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::Query>
    codec_message_client_query_param[] = {
        {"do_1", {"DO 1"}, {}, {0x03, 'D', 'O', ' ', '1'}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientQueryTest,
                         ::testing::ValuesIn(codec_message_client_query_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::Ping

using CodecMessageClientPingTest =
    CodecTest<classic_protocol::message::client::Ping>;

TEST_P(CodecMessageClientPingTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientPingTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::Ping> codec_ping_param[] = {
    {"ping", {}, {}, {0x0e}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientPingTest,
                         ::testing::ValuesIn(codec_ping_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::Statistics

using CodecMessageClientStatisticsTest =
    CodecTest<classic_protocol::message::client::Statistics>;

TEST_P(CodecMessageClientStatisticsTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientStatisticsTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::Statistics>
    codec_message_client_statistics_param[] = {
        {"1", {}, {}, {0x09}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientStatisticsTest,
    ::testing::ValuesIn(codec_message_client_statistics_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::ResetConnection

using CodecMessageClientResetConnectionTest =
    CodecTest<classic_protocol::message::client::ResetConnection>;

TEST_P(CodecMessageClientResetConnectionTest, encode) {
  test_encode(GetParam());
}
TEST_P(CodecMessageClientResetConnectionTest, decode) {
  test_decode(GetParam());
}

const CodecParam<classic_protocol::message::client::ResetConnection>
    codec_message_client_resetconnection_param[] = {
        {"1", {}, {}, {0x1f}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientResetConnectionTest,
    ::testing::ValuesIn(codec_message_client_resetconnection_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::StmtPrepare

using CodecMessageClientStmtPrepareTest =
    CodecTest<classic_protocol::message::client::StmtPrepare>;

TEST_P(CodecMessageClientStmtPrepareTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientStmtPrepareTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::StmtPrepare>
    codec_message_client_prepstmt_param[] = {
        {"do_1", {"DO 1"}, {}, {0x16, 'D', 'O', ' ', '1'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientStmtPrepareTest,
    ::testing::ValuesIn(codec_message_client_prepstmt_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::StmtParamAppendData

using CodecMessageClientStmtParamAppendDataTest =
    CodecTest<classic_protocol::message::client::StmtParamAppendData>;

TEST_P(CodecMessageClientStmtParamAppendDataTest, encode) {
  test_encode(GetParam());
}
TEST_P(CodecMessageClientStmtParamAppendDataTest, decode) {
  test_decode(GetParam());
}

const CodecParam<classic_protocol::message::client::StmtParamAppendData>
    codec_message_client_stmtparamappenddata_param[] = {
        {"append_stmt_1_param_1_abc",
         {1, 1, "abc"},
         {},
         {0x18, 1, 0, 0, 0, 1, 0, 'a', 'b', 'c'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientStmtParamAppendDataTest,
    ::testing::ValuesIn(codec_message_client_stmtparamappenddata_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::StmtExecute
//
namespace classic_protocol {
namespace message {
namespace client {
std::ostream &operator<<(std::ostream &os, const StmtExecute &v) {
  os << "StmtExecute: "
     << "\n";
  os << "  stmt_id: " << v.statement_id() << "\n";
  os << "  flags: " << v.flags().to_string() << "\n";
  os << "  iteration_count: " << v.iteration_count() << "\n";
  os << "  new_params_bound: " << v.new_params_bound() << "\n";
  os << "  types: "
     << "\n";
  for (auto const &t : v.types()) {
    os << "    - " << static_cast<uint16_t>(t) << "\n";
  }
  os << "  values: "
     << "\n";
  for (auto const &field : v.values()) {
    if (field) {
      os << "    - string{" << field.value().size() << "}\n";
    } else {
      os << "    - NULL\n";
    }
  }
  return os;
}
}  // namespace client
}  // namespace message
}  // namespace classic_protocol

using CodecMessageClientStmtExecuteTest =
    CodecTest<classic_protocol::message::client::StmtExecute>;

TEST_P(CodecMessageClientStmtExecuteTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientStmtExecuteTest, decode) {
  test_decode(GetParam(), [](uint32_t /* statement_id */) { return 1; });
}

const CodecParam<classic_protocol::message::client::StmtExecute>
    codec_stmt_execute_param[] = {
        {"exec_stmt",
         {1, 0x00, 1, true, {classic_protocol::field_type::Varchar}, {{"foo"}}},
         {},
         {0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
          0x01, 0x0f, 0x00, 0x03, 0x66, 0x6f, 0x6f}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientStmtExecuteTest,
                         ::testing::ValuesIn(codec_stmt_execute_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::StmtClose

using CodecMessageClientStmtCloseTest =
    CodecTest<classic_protocol::message::client::StmtClose>;

TEST_P(CodecMessageClientStmtCloseTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientStmtCloseTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::StmtClose>
    codec_stmt_close_param[] = {
        {"close_stmt_1", {1}, {}, {0x19, 0x01, 0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientStmtCloseTest,
                         ::testing::ValuesIn(codec_stmt_close_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::StmtReset

using CodecMessageClientStmtResetTest =
    CodecTest<classic_protocol::message::client::StmtReset>;

TEST_P(CodecMessageClientStmtResetTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientStmtResetTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::StmtReset>
    codec_stmt_reset_param[] = {
        {"reset_stmt_1", {1}, {}, {0x1a, 0x01, 0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientStmtResetTest,
                         ::testing::ValuesIn(codec_stmt_reset_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::StmtSetOption

using CodecMessageClientStmtSetOptionTest =
    CodecTest<classic_protocol::message::client::StmtSetOption>;

TEST_P(CodecMessageClientStmtSetOptionTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientStmtSetOptionTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::StmtSetOption>
    codec_stmt_set_option_param[] = {
        {"set_option_stmt_1", {1}, {}, {0x1b, 0x01, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientStmtSetOptionTest,
                         ::testing::ValuesIn(codec_stmt_set_option_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::StmtFetch

using CodecMessageClientStmtFetchTest =
    CodecTest<classic_protocol::message::client::StmtFetch>;

TEST_P(CodecMessageClientStmtFetchTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientStmtFetchTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::StmtFetch>
    codec_stmt_fetch_param[] = {
        {"fetch_stmt_1",
         {1, 2},
         {},
         {0x1c, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientStmtFetchTest,
                         ::testing::ValuesIn(codec_stmt_fetch_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::Greeting

namespace classic_protocol {
namespace message {
namespace client {
std::ostream &operator<<(std::ostream &os, const Greeting &v) {
  os << "Greeting: "
     << "\n";
  os << "  schema: " << v.schema() << "\n";
  os << "  username: " << v.username() << "\n";
  os << "  auth_method_name: " << v.auth_method_name() << "\n";
  os << "  max_packet_size: " << v.max_packet_size() << "\n";
  os << "  capabilities: " << v.capabilities().to_string() << "\n";

  return os;
}
}  // namespace client
}  // namespace message
}  // namespace classic_protocol

using CodecMessageClientGreetingTest =
    CodecTest<classic_protocol::message::client::Greeting>;

TEST_P(CodecMessageClientGreetingTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientGreetingTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::Greeting>
    codec_message_client_greeting_param[] =
        {
            {"5_6_6",
             {0x001ea285, 1 << 30, 0x8, "root",
              S("\"Py\xA2\x12\xD4\xE8\x82\xE5\xB3\xF4\x1A\x97uk\xC8\xBE\xDB"
                "\x9F\x80"),
              "", "mysql_native_password",
              "\x3"
              "_os"
              "\t"
              "debian6.0"
              "\f"
              "_client_name"
              "\b"
              "libmysql"
              "\x4"
              "_pid"
              "\x5"
              "22344"
              "\xF"
              "_client_version"
              "\b"
              "5.6.6-m9"
              "\t"
              "_platform"
              "\x6"
              "x86_64"
              "\x3"
              "foo"
              "\x3"
              "bar"},
             classic_protocol::capabilities::protocol_41 |
                 classic_protocol::capabilities::secure_connection |
                 classic_protocol::capabilities::plugin_auth |
                 classic_protocol::capabilities::connect_attributes,
             {
                 0x85, 0xa2, 0x1e, 0x00,  // caps
                 0x00, 0x00, 0x00, 0x40,  // max-packet-size
                 0x08,                    // collation
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00,
                 0x00,                          // filler
                 0x72, 0x6f, 0x6f, 0x74, 0x00,  // username
                 0x14, 0x22, 0x50, 0x79, 0xa2, 0x12, 0xd4, 0xe8, 0x82, 0xe5,
                 0xb3, 0xf4, 0x1a, 0x97, 0x75, 0x6b, 0xc8, 0xbe, 0xdb, 0x9f,
                 0x80,  // auth-method-data
                 0x6d, 0x79, 0x73, 0x71, 0x6c, 0x5f, 0x6e, 0x61, 0x74, 0x69,
                 0x76, 0x65, 0x5f, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72,
                 0x64, 0x00,  // auth-method
                 0x61, 0x03, '_',  'o',  's',  0x09, 'd',  'e',  'b',  'i',
                 'a',  'n',  '6',  '.',  '0',  0x0c, 0x5f, 0x63, 0x6c, 0x69,
                 0x65, 0x6e, 0x74, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0x08, 0x6c,
                 0x69, 0x62, 0x6d, 0x79, 0x73, 0x71, 0x6c, 0x04, 0x5f, 0x70,
                 0x69, 0x64, 0x05, 0x32, 0x32, 0x33, 0x34, 0x34, 0x0f, 0x5f,
                 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x5f, 0x76, 0x65, 0x72,
                 0x73, 0x69, 0x6f, 0x6e, 0x08, 0x35, 0x2e, 0x36, 0x2e, 0x36,
                 0x2d, 0x6d, 0x39, 0x09, 0x5f, 0x70, 0x6c, 0x61, 0x74, 0x66,
                 0x6f, 0x72, 0x6d, 0x06, 0x78, 0x38, 0x36, 0x5f, 0x36, 0x34,
                 0x03, 0x66, 0x6f, 0x6f, 0x03, 0x62, 0x61, 0x72  // connect-attributes
             }},
            {"5_5_8",
             {0x000fa68d, 1 << 24, 0x8, "pam",
              "\xAB\t\xEE\xF6\xBC\xB1"
              "2>a\x14"
              "8e\xC0\x99\x1D\x95}u\xD4G",
              "test", "mysql_native_password", ""},
             classic_protocol::capabilities::protocol_41 |
                 classic_protocol::capabilities::secure_connection |
                 classic_protocol::capabilities::connect_with_schema |
                 classic_protocol::capabilities::plugin_auth |
                 classic_protocol::capabilities::connect_attributes,
             {
                 0x8d, 0xa6, 0x0f, 0x00,  // caps
                 0x00, 0x00, 0x00, 0x01,  // max-packet-size
                 0x08,                    // collation
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00,                    // 23 fillers
                 'p',  'a',  'm',  '\0',  // username
                 0x14, 0xab, 0x09, 0xee, 0xf6, 0xbc, 0xb1, 0x32,
                 0x3e, 0x61, 0x14, 0x38, 0x65, 0xc0, 0x99, 0x1d,
                 0x95, 0x7d, 0x75, 0xd4, 0x47,  // auth-method-data
                 0x74, 0x65, 0x73, 0x74, '\0',  // schema
                 0x6d, 0x79, 0x73, 0x71, 0x6c, 0x5f, 0x6e, 0x61,
                 0x74, 0x69, 0x76, 0x65, 0x5f, 0x70, 0x61, 0x73,
                 0x73, 0x77, 0x6f, 0x72, 0x64, '\0'  // auth-method
             }},
            {"4_1_22",
             {0x3a685, 1 << 24, 0x8, "root",
              "U3\xEFk!S\xED\x1\xDB\xBA\x87\xDD\xC6\xD0"
              "8pq\x18('",
              "", "", ""},
             classic_protocol::capabilities::protocol_41 |
                 classic_protocol::capabilities::secure_connection,
             {
                 0x85, 0xa6, 0x03, 0x00,  // caps
                 0x00, 0x00, 0x00, 0x01,  // max-packet-size
                 0x08,                    // collation
                 '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
                 '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
                 '\0', '\0', '\0', '\0', '\0', '\0',
                 '\0',                          // filler
                 'r',  'o',  'o',  't',  '\0',  // username
                 0x14, 'U',  '3',  0xef, 'k',  '!',  'S',  0xed,
                 0x01, 0xdb, 0xba, 0x87, 0xdd, 0xc6, 0xd0, '8',
                 'p',  'q',  0x18, '(',  '\''  // auth-method-data
             }},
            {"3_23_58",
             {0x2405, 0, 0, "root", "H]^CSVY[", "", "", ""},
             {},
             {'\5', '$', 0, 0, 0, 'r', 'o', 'o', 't', 0, 'H', ']', '^', 'C',
              'S', 'V', 'Y', '['}},
            {"3_23_58_with_schema",
             {0x240d, 0, 0, "root", "H]^CSVY[", "foobar", "", ""},
             classic_protocol::capabilities::connect_with_schema,
             {
                 0x0d, 0x24,                   // caps
                 0x00, 0x00, 0x00,             // max-packet-size
                 'r',  'o',  'o',  't', '\0',  // username
                 'H',  ']',  '^',  'C', 'S',  'V',
                 'Y',  '[',  '\0',                 // auth-method-data
                 'f',  'o',  'o',  'b', 'a',  'r'  // schema
             }},
            {"8_0_17_ssl",
             {0b0000'0001'1111'1111'1010'1110'0000'0101,
              1 << 24,
              0xff,
              "",
              "",
              "",
              {},
              {}},
             classic_protocol::capabilities::protocol_41 |
                 classic_protocol::capabilities::ssl,
             {
                 0x05, 0xae, 0xff, 0x01,  // caps
                 0x00, 0x00, 0x00, 0x01,  // max-packet-size
                 0xff,                    // collation
                 0x00, 0x00, 0x00, 0x00,  // 23 fillers
                 0x00, 0x00, 0x00, 0x00,  //
                 0x00, 0x00, 0x00, 0x00,  //
                 0x00, 0x00, 0x00, 0x00,  //
                 0x00, 0x00, 0x00, 0x00,  //
                 0x00, 0x00, 0x00         //
             }},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientGreetingTest,
    ::testing::ValuesIn(codec_message_client_greeting_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::ChangeUser

namespace classic_protocol {
namespace message {
namespace client {
std::ostream &operator<<(std::ostream &os, const ChangeUser &v) {
  os << "ChangeUser: "
     << "\n";
  os << "  schema: " << v.schema() << "\n";
  os << "  username: " << v.username() << "\n";
  os << "  auth_method_name: " << v.auth_method_name() << "\n";

  return os;
}
}  // namespace client
}  // namespace message
}  // namespace classic_protocol

using CodecMessageClientChangeUserTest =
    CodecTest<classic_protocol::message::client::ChangeUser>;

TEST_P(CodecMessageClientChangeUserTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientChangeUserTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::ChangeUser>
    codec_message_client_changeuser_param
        [] =
            {
                {"5_6_6",
                 {"root",
                  S("\"Py\xA2\x12\xD4\xE8\x82\xE5\xB3\xF4\x1A\x97uk\xC8\xBE\xDB"
                    "\x9F\x80"),
                  "", 0x08, "mysql_native_password",
                  "\x3"
                  "_os"
                  "\t"
                  "debian6.0"
                  "\f"
                  "_client_name"
                  "\b"
                  "libmysql"
                  "\x4"
                  "_pid"
                  "\x5"
                  "22344"
                  "\xF"
                  "_client_version"
                  "\b"
                  "5.6.6-m9"
                  "\t"
                  "_platform"
                  "\x6"
                  "x86_64"
                  "\x3"
                  "foo"
                  "\x3"
                  "bar"},
                 classic_protocol::capabilities::protocol_41 |
                     classic_protocol::capabilities::secure_connection |
                     classic_protocol::capabilities::plugin_auth |
                     classic_protocol::capabilities::connect_attributes,
                 {
                     0x11,                          // cmd-byte
                     0x72, 0x6f, 0x6f, 0x74, 0x00,  // username
                     0x14, 0x22, 0x50, 0x79, 0xa2, 0x12, 0xd4, 0xe8, 0x82, 0xe5,
                     0xb3, 0xf4, 0x1a, 0x97, 0x75, 0x6b, 0xc8, 0xbe, 0xdb, 0x9f,
                     0x80,        // auth-method-data
                     0x00,        // schema
                     0x08, 0x00,  // collation
                     0x6d, 0x79, 0x73, 0x71, 0x6c, 0x5f, 0x6e, 0x61, 0x74, 0x69,
                     0x76, 0x65, 0x5f, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72,
                     0x64, 0x00,  // auth-method-name
                     0x61, 0x03, '_',  'o',  's',  0x09, 'd',  'e',  'b',  'i',
                     'a',  'n',  '6',  '.',  '0',  0x0c, 0x5f, 0x63, 0x6c, 0x69,
                     0x65, 0x6e, 0x74, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0x08, 0x6c,
                     0x69, 0x62, 0x6d, 0x79, 0x73, 0x71, 0x6c, 0x04, 0x5f, 0x70,
                     0x69, 0x64, 0x05, 0x32, 0x32, 0x33, 0x34, 0x34, 0x0f, 0x5f,
                     0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x5f, 0x76, 0x65, 0x72,
                     0x73, 0x69, 0x6f, 0x6e, 0x08, 0x35, 0x2e, 0x36, 0x2e, 0x36,
                     0x2d, 0x6d, 0x39, 0x09, 0x5f, 0x70, 0x6c, 0x61, 0x74, 0x66,
                     0x6f, 0x72, 0x6d, 0x06, 0x78, 0x38, 0x36, 0x5f, 0x36, 0x34,
                     0x03, 0x66, 0x6f, 0x6f, 0x03, 0x62, 0x61, 0x72  // connect-attributes
                 }},
                {"5_5_8",
                 {"pam",
                  "\xAB\t\xEE\xF6\xBC\xB1"
                  "2>a\x14"
                  "8e\xC0\x99\x1D\x95}u\xD4G",
                  "test", 0x08, "mysql_native_password", ""},
                 classic_protocol::capabilities::protocol_41 |
                     classic_protocol::capabilities::secure_connection |
                     classic_protocol::capabilities::connect_with_schema |
                     classic_protocol::capabilities::plugin_auth |
                     classic_protocol::capabilities::connect_attributes,
                 {
                     0x11,                    // cmd-byte
                     'p',  'a',  'm',  '\0',  // username
                     0x14, 0xab, 0x09, 0xee, 0xf6, 0xbc, 0xb1, 0x32,
                     0x3e, 0x61, 0x14, 0x38, 0x65, 0xc0, 0x99, 0x1d,
                     0x95, 0x7d, 0x75, 0xd4, 0x47,  // auth-method-data
                     0x74, 0x65, 0x73, 0x74, '\0',  // schema
                     0x08, 0x00,                    // collation
                     0x6d, 0x79, 0x73, 0x71, 0x6c, 0x5f, 0x6e, 0x61,
                     0x74, 0x69, 0x76, 0x65, 0x5f, 0x70, 0x61, 0x73,
                     0x73, 0x77, 0x6f, 0x72, 0x64, '\0',  // auth-method-name
                     0x00,                                // attributes
                 }},
                {"4_1_22",
                 {"root",
                  "U3\xEFk!S\xED\x1\xDB\xBA\x87\xDD\xC6\xD0"
                  "8pq\x18('",
                  "", 0x08, "", ""},
                 classic_protocol::capabilities::protocol_41 |
                     classic_protocol::capabilities::secure_connection,
                 {
                     0x11,                          // cmd-byte
                     'r',  'o',  'o',  't',  '\0',  // username
                     0x14, 'U',  '3',  0xef, 'k',  '!',  'S',  0xed,
                     0x01, 0xdb, 0xba, 0x87, 0xdd, 0xc6, 0xd0, '8',
                     'p',  'q',  0x18, '(',  '\'',  // auth-method-data
                     '\0',                          // schema
                     0x08, 0x00,                    // collation
                 }},
                {"3_23_58",
                 {"root", "H]^CSVY[", "", 0, "", ""},
                 {},
                 {
                     0x11,                      // cmd-byte
                     'r', 'o', 'o', 't', '\0',  // username
                     'H', ']', '^', 'C', 'S', 'V', 'Y', '[',
                     '\0',  // auth-method-data
                     '\0',  // schema
                 }},
                {"3_23_58_with_schema",
                 {"root", "H]^CSVY[", "foobar", 0, "", ""},
                 {},  // caps don't matter here
                 {
                     0x11,                        // cmd-byte
                     'r',  'o',  'o', 't', '\0',  // username
                     'H',  ']',  '^', 'C', 'S',  'V', 'Y',
                     '[',  '\0',                             // auth-method-data
                     'f',  'o',  'o', 'b', 'a',  'r', '\0',  // schema
                 }},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientChangeUserTest,
    ::testing::ValuesIn(codec_message_client_changeuser_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

using namespace std::string_literals;

const CodecParam<classic_protocol::message::server::StmtRow>
    codec_message_server_stmtrow_param[] = {
        {"abc_def",
         {{classic_protocol::field_type::VarString}, {"foobar"s}},
         {},
         {0x00, 0x00, 0x06, 'f', 'o', 'o', 'b', 'a', 'r'}},
        {"null",
         {{classic_protocol::field_type::VarString}, {stdx::make_unexpected()}},
         {},
         {0x00, 0x04}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerStmtRowTest,
    ::testing::ValuesIn(codec_message_server_stmtrow_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

TEST(ClassicProto, Decode_NulTermString_multiple_chunks) {
  std::list<std::vector<uint8_t>> read_storage{{'8', '0'}, {'1', 0x00, 'f'}};
  std::list<net::const_buffer> read_bufs;
  for (auto const &b : read_storage) {
    read_bufs.push_back(net::buffer(b));
  }

  const auto res =
      classic_protocol::Codec<classic_protocol::wire::NulTermString>::decode(
          read_bufs, {});

  ASSERT_TRUE(res);

  // the \0 is consumed too, but not part of the output
  EXPECT_EQ(res->first, 3 + 1);
  EXPECT_EQ(res->second.value(), "801");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
