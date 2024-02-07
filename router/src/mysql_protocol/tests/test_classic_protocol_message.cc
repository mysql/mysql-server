/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "mysqlrouter/classic_protocol_codec_message.h"

#include <list>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <mysql.h>  // MYSQL_TYPE_TINY

#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
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
  os << v.auth_method_data();
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
        {"caching_sha2_password_public_key", {"\x02"}, {}, {0x01, 0x02}},
        {"caching_sha2_password_fast_ack", {"\x03"}, {}, {0x01, 0x03}},
        {"caching_sha2_password_full_handshake", {"\x04"}, {}, {0x01, 0x04}},
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
        {"with_gtid",
         /*
          * {
          *   "status_flags": ["autocommit", "session_state_changed"],
          *   "session_tracker": [{
          *       "gtid": {
          *         "gtid": "4dd0f9d5-3b00-11eb-ad70-003093140e4e:23929",
          *         "spec": 0
          *       }
          *     }, {
          *       "trx_characteristics": {
          *         "trx_state": {
          *           "trx_type": "_",
          *           "read_unsafe": "_",
          *           "read_trx": "_",
          *           "write_unsafe": "_",
          *           "write_trx": "_",
          *           "stmt_unsafe": "_",
          *           "resultset": "_",
          *           "locked_tables": "_"
          *         }
          *       }
          *     }]
          * }
          */
         {0,  // last-insert-id
          0,  // affected-rows
          classic_protocol::status::autocommit |
              classic_protocol::status::session_state_changed,
          0,   // warning-count
          "",  // message
          {S("\x03\x2c\x00\x2a\x34\x64\x64\x30\x66\x39\x64\x35\x2d\x33\x62\x30"
             "\x30\x2d\x31\x31\x65\x62\x2d\x61\x64\x37\x30\x2d\x30\x30\x33\x30"
             "\x39\x33\x31\x34\x30\x65\x34\x65\x3a\x32\x33\x39\x32\x39\x05\x09"
             "\x08\x5f\x5f\x5f\x5f\x5f\x5f\x5f\x5f")}},  // session-track
         classic_protocol::capabilities::protocol_41 |
             classic_protocol::capabilities::session_track,
         {0x00, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x00, 0x39, 0x03, 0x2c,
          0x00, 0x2a, 0x34, 0x64, 0x64, 0x30, 0x66, 0x39, 0x64, 0x35, 0x2d,
          0x33, 0x62, 0x30, 0x30, 0x2d, 0x31, 0x31, 0x65, 0x62, 0x2d, 0x61,
          0x64, 0x37, 0x30, 0x2d, 0x30, 0x30, 0x33, 0x30, 0x39, 0x33, 0x31,
          0x34, 0x30, 0x65, 0x34, 0x65, 0x3a, 0x32, 0x33, 0x39, 0x32, 0x39,
          0x05, 0x09, 0x08, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageServerOkTest,
                         ::testing::ValuesIn(codec_message_server_ok_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

TEST(MessageServerOk, warning_count) {
  classic_protocol::message::server::Ok msg;

  EXPECT_EQ(msg.warning_count(), 0);
  msg.warning_count(1);
  EXPECT_EQ(msg.warning_count(), 1);
}

TEST(MessageServerOk, last_insert_id) {
  classic_protocol::message::server::Ok msg;

  EXPECT_EQ(msg.last_insert_id(), 0);
  msg.last_insert_id(1);
  EXPECT_EQ(msg.last_insert_id(), 1);
}

TEST(MessageServerOk, affected_rows) {
  classic_protocol::message::server::Ok msg;

  EXPECT_EQ(msg.affected_rows(), 0);
  msg.affected_rows(1);
  EXPECT_EQ(msg.affected_rows(), 1);
}

TEST(MessageServerOk, message) {
  classic_protocol::message::server::Ok msg;

  EXPECT_EQ(msg.message(), "");
  msg.message("hi");
  EXPECT_EQ(msg.message(), "hi");
}

TEST(MessageServerOk, session_changes) {
  classic_protocol::message::server::Ok msg;

  EXPECT_EQ(msg.session_changes(), "");
  msg.session_changes("hi");
  EXPECT_EQ(msg.session_changes(), "hi");
}

// server::Eof

using CodecMessageServerEofTest =
    CodecTest<classic_protocol::message::server::Eof>;

TEST_P(CodecMessageServerEofTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerEofTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::Eof> codec_eof_param[] = {
    {"3_23", {}, {}, {0xfe}},
    {"4_1",
     {
         classic_protocol::status::more_results_exist |
             classic_protocol::status::autocommit,  // flags
         1                                          // warning_count
     },
     classic_protocol::capabilities::protocol_41,
     {0xfe, 0x01, 0x00, 0x0a, 0x00}},
    {"5_7",
     {
         classic_protocol::status::autocommit,  // flags
         1                                      // warning_count
     },
     classic_protocol::capabilities::text_result_with_session_tracking |
         classic_protocol::capabilities::protocol_41,
     {0xfe, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00}},
    {"session_tracking",
     {classic_protocol::status::autocommit |
          classic_protocol::status::more_results_exist |
          classic_protocol::status::ps_out_params |
          classic_protocol::status::session_state_changed,  // flags
      0,                                                    // warning_count
      "",                                                   // message
      {S("\1\1\0")}},                                       // session-changes
     classic_protocol::capabilities::text_result_with_session_tracking |
         classic_protocol::capabilities::transactions |
         classic_protocol::capabilities::session_track |
         classic_protocol::capabilities::protocol_41,
     {0xfe,        // EOF
      0x00, 0x00,  // affected-rows, last-insert-id
      0x0a, 0x50,  // status-flags
      0x00, 0x00,  // warning-count
      0x00,        // message
      0x03, 0x01, 0x01, 0x00}},
    {"session_tracking_empty_message_and_session_track",
     {classic_protocol::status::autocommit |
          classic_protocol::status::more_results_exist |
          classic_protocol::status::ps_out_params |
          classic_protocol::status::session_state_changed,  // flags
      0,                                                    // warning_count
      {},                                                   // message
      {}},                                                  // session-changes
     classic_protocol::capabilities::text_result_with_session_tracking |
         classic_protocol::capabilities::transactions |
         classic_protocol::capabilities::session_track |
         classic_protocol::capabilities::protocol_41,
     {
         0xfe,        // EOF
         0x00, 0x00,  // affected-rows, last-insert-id
         0x0a, 0x50,  // status-flags
         0x00, 0x00,  // warning-count
         0x00,        // message
         0x00,        // session-track
     }},
    {"session_tracking_supported_but_no_session_track_used",
     {classic_protocol::status::autocommit |
          classic_protocol::status::more_results_exist |
          classic_protocol::status::ps_out_params,  // flags
      0,                                            // warning_count
      {},                                           // message
      {}},                                          // session-changes
     classic_protocol::capabilities::text_result_with_session_tracking |
         classic_protocol::capabilities::transactions |
         classic_protocol::capabilities::session_track |
         classic_protocol::capabilities::protocol_41,
     {
         0xfe,        // EOF
         0x00, 0x00,  // affected-rows, last-insert-id
         0x0a, 0x10,  // status-flags
         0x00, 0x00,  // warning-count
         // as 'message' is empty and it is the last byte, it is not sent.
     }},
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

TEST(MessageServerError, default_constructed) {
  classic_protocol::message::server::Error msg;

  EXPECT_EQ(msg.error_code(), 0);
  EXPECT_EQ(msg.message(), "");
  EXPECT_EQ(msg.sql_state(), "");
}

TEST(MessageServerError, default_args_constructed) {
  classic_protocol::message::server::Error msg(1234, "foo");

  EXPECT_EQ(msg.error_code(), 1234);
  EXPECT_EQ(msg.message(), "foo");
  EXPECT_EQ(msg.sql_state(), "HY000");
}

TEST(MessageServerError, warning_count) {
  classic_protocol::message::server::Error msg;

  msg.error_code(123);
  EXPECT_EQ(msg.error_code(), 123);
}

TEST(MessageServerError, message) {
  classic_protocol::message::server::Error msg;

  msg.message("foo");
  EXPECT_EQ(msg.message(), "foo");
}

TEST(MessageServerError, sql_state) {
  classic_protocol::message::server::Error msg;

  msg.sql_state("HY000");
  EXPECT_EQ(msg.sql_state(), "HY000");
}

TEST(MessageServerError, short_sql_state) {
  std::array<uint8_t, 6> packet{
      0xff, 0x12, 0x34, '#', 'F', 'O',
  };
  auto decode_res =
      classic_protocol::Codec<classic_protocol::message::server::Error>::decode(
          net::buffer(packet), {classic_protocol::capabilities::protocol_41});
  ASSERT_FALSE(decode_res);
}

// server::Greeting

TEST(MessageServerGreeting, construct) {
  classic_protocol::message::server::Greeting msg{
      0x0a, "8.0.12", 1,    "012345678901234567",
      0,    0xff,     0x10, "mysql_native_password"};

  EXPECT_EQ(msg.protocol_version(), 10);
  EXPECT_EQ(msg.version(), "8.0.12");
  EXPECT_EQ(msg.connection_id(), 1);
  EXPECT_EQ(msg.auth_method_data(), "012345678901234567");
  EXPECT_EQ(msg.capabilities(), 0);
  EXPECT_EQ(msg.collation(), 0xff);
  EXPECT_EQ(msg.status_flags(), 0x10);
  EXPECT_EQ(msg.auth_method_name(), "mysql_native_password");
}

TEST(MessageServerGreeting, setter) {
  classic_protocol::message::server::Greeting msg{
      0x0a, "8.0.12", 1,    "012345678901234567",
      0,    0xff,     0x10, "mysql_native_password"};

  msg.protocol_version(0x09);
  msg.version("8.0.13");
  msg.connection_id(2);
  msg.auth_method_data("012345678901234568");
  msg.capabilities(1);
  msg.collation(0x0);
  msg.status_flags(0x11);
  msg.auth_method_name("mysql_old_password");

  EXPECT_EQ(msg.protocol_version(), 9);
  EXPECT_EQ(msg.version(), "8.0.13");
  EXPECT_EQ(msg.connection_id(), 2);
  EXPECT_EQ(msg.auth_method_data(), "012345678901234568");
  EXPECT_EQ(msg.capabilities(), 1);
  EXPECT_EQ(msg.collation(), 0x0);
  EXPECT_EQ(msg.status_flags(), 0x11);
  EXPECT_EQ(msg.auth_method_name(), "mysql_old_password");
}

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

// server::ColumnCount

using CodecMessageServerColumnCountTest =
    CodecTest<classic_protocol::message::server::ColumnCount>;

TEST_P(CodecMessageServerColumnCountTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerColumnCountTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::ColumnCount>
    codec_message_server_column_count_param[] = {
        {
            "single_byte_1",
            {1},  //
            {},
            {1},
        },
        {
            "double_byte_255",
            {255},  //
            {},
            {0xfc, 0xff, 0x00},  // varint encoding
        },
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerColumnCountTest,
    ::testing::ValuesIn(codec_message_server_column_count_param),
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

// server::SendFileRequest

using CodecMessageServerSendFileRequestTest =
    CodecTest<classic_protocol::message::server::SendFileRequest>;

TEST_P(CodecMessageServerSendFileRequestTest, encode) {
  test_encode(GetParam());
}
TEST_P(CodecMessageServerSendFileRequestTest, decode) {
  test_decode(GetParam());
}

using namespace std::string_literals;

const CodecParam<classic_protocol::message::server::SendFileRequest>
    codec_message_server_send_file_request_param[] = {
        {"somefile",
         {{"somefile"s}},  // decoded
         {},               // caps
         {0xfb, 's', 'o', 'm', 'e', 'f', 'i', 'l', 'e'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerSendFileRequestTest,
    ::testing::ValuesIn(codec_message_server_send_file_request_param),
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
        {"null_null", {{std::nullopt, std::nullopt}}, {}, {0xfb, 0xfb}},
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

// server::StmtPrepareOk

TEST(MessageServerStmtPrepareOk, constructed) {
  classic_protocol::message::server::StmtPrepareOk msg(0, 1, 2, 3, 4);

  EXPECT_EQ(msg.statement_id(), 0);
  EXPECT_EQ(msg.column_count(), 1);
  EXPECT_EQ(msg.param_count(), 2);
  EXPECT_EQ(msg.warning_count(), 3);
  EXPECT_EQ(msg.with_metadata(), 4);
}

TEST(MessageServerStmtPrepareOk, setters) {
  classic_protocol::message::server::StmtPrepareOk msg(0, 1, 2, 3, 4);

  // check the setters overwrite the initial values.

  msg.statement_id(5);
  EXPECT_EQ(msg.statement_id(), 5);

  msg.warning_count(6);
  EXPECT_EQ(msg.warning_count(), 6);

  msg.param_count(7);
  EXPECT_EQ(msg.param_count(), 7);

  msg.column_count(8);
  EXPECT_EQ(msg.column_count(), 8);

  msg.with_metadata(9);
  EXPECT_EQ(msg.with_metadata(), 9);
}

using CodecMessageServerStmtPrepareOkTest =
    CodecTest<classic_protocol::message::server::StmtPrepareOk>;

TEST_P(CodecMessageServerStmtPrepareOkTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerStmtPrepareOkTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::StmtPrepareOk>
    codec_message_server_prepstmtok_param[] = {
        {"do_1",  // like DO 1
         {
             1,     // stmt-id
             0,     // column-count
             0,     // param-count
             0,     // warning-count
             true,  // with-metadata
         },
         {},  // caps: no optional_resultset_metadata
         {
             0x00,                    // ok
             0x01, 0x00, 0x00, 0x00,  // stmt-id
             0x00, 0x00,              // column-count
             0x00, 0x00,              // param-count
             0x00,                    // filler
             0x00, 0x00               // warning-count
         }},
        {"select_1",  // like SELECT 1;
         {
             2,     // stmt-id
             1,     // column-count
             0,     // param-count
             0,     // warning-count
             true,  // with-metadata
         },
         {},  // caps: no optional_resultset_metadata
         {
             0x00,                    // ok
             0x02, 0x00, 0x00, 0x00,  // stmt-id
             0x01, 0x00,              // column-count
             0x00, 0x00,              // param-count
             0x00,                    // filler
             0x00, 0x00               // warning-count
         }},
        {"select_placeholder",  // like SELECT ?
         {
             2,     // stmt-id
             1,     // column-count
             1,     // param-count
             3,     // warning-count
             true,  // with-metadata
         },
         {},  // caps: no optional_resultset_metadata
         {
             0x00,                    // ok
             0x02, 0x00, 0x00, 0x00,  // stmt-id
             0x01, 0x00,              // column-count
             0x01, 0x00,              // param-count
             0x00,                    // filler
             0x03, 0x00               // warning-count
         }},
        {"do_1_with_metadata",  // like DO 1
         {
             1,     // stmt-id
             0,     // column-count
             0,     // param-count
             0,     // warning-count
             true,  // with-metadata
         },
         classic_protocol::capabilities::optional_resultset_metadata,
         {
             0x00,                    // ok
             0x01, 0x00, 0x00, 0x00,  // stmt-id
             0x00, 0x00,              // column-count
             0x00, 0x00,              // param-count
             0x00,                    // filler
             0x00, 0x00,              // warning-count
             0x01                     // with-metadata
         }},
        {"select_1_with_metadata",  // like SELECT 1;
         {
             2,     // stmt-id
             0,     // param-count
             1,     // column-count
             0,     // warning-count
             true,  // with-metadata
         },
         classic_protocol::capabilities::optional_resultset_metadata,
         {
             0x00,                    // ok
             0x02, 0x00, 0x00, 0x00,  // stmt-id
             0x00, 0x00,              // column-count
             0x01, 0x00,              // param-count
             0x00,                    // filler
             0x00, 0x00,              // warning-count
             0x01                     // with-metadata
         }},
        {"select_placeholder_with_metadata",  // like SELECT ?
         {
             2,     // stmt-id
             1,     // param-count
             1,     // column-count
             3,     // warning-count
             true,  // with-metadata
         },
         classic_protocol::capabilities::optional_resultset_metadata,
         {
             0x00,                    // ok
             0x02, 0x00, 0x00, 0x00,  // stmt-id
             0x01, 0x00,              // column-count
             0x01, 0x00,              // param-count
             0x00,                    // filler
             0x03, 0x00,              // warning-count
             0x01                     // with-metadata
         }},
        {"select_placeholder_without_metadata",  // like SELECT ?
         {
             2,      // stmt-id
             1,      // param-count
             1,      // column-count
             3,      // warning-count
             false,  // with-metadata
         },
         classic_protocol::capabilities::optional_resultset_metadata,
         {
             0x00,                    // ok
             0x02, 0x00, 0x00, 0x00,  // stmt-id
             0x01, 0x00,              // column-count
             0x01, 0x00,              // param-count
             0x00,                    // filler
             0x03, 0x00,              // warning-count
             0x00                     // with-metadata
         }},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerStmtPrepareOkTest,
    ::testing::ValuesIn(codec_message_server_prepstmtok_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

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
        {"do_2_query_attributes_no_params",
         {"DO 2"},
         {classic_protocol::capabilities::query_attributes},
         {0x03, 0x00, 0x01, 'D', 'O', ' ', '2'}},
        {"do_3_query_attributes_one_int_param",
         {"DO 3", {{MYSQL_TYPE_TINY, "name", std::string{0x00}}}},
         {classic_protocol::capabilities::query_attributes},
         {0x03,                      // cmd
          0x01,                      // param-count
          0x01,                      // param-set-count
          0x00,                      // null-bit-map: no NULL
          0x01,                      // new-params-bound = 0x01
                                     // param[0]:
          0x01, 0x00,                // .param_type_and_flag: TINY
          0x04, 'n', 'a', 'm', 'e',  // .name
          0x00,                      // .value TINY{0}
          'D', 'O', ' ', '3'}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientQueryTest,
                         ::testing::ValuesIn(codec_message_client_query_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::SendFile

using CodecMessageClientSendFileTest =
    CodecTest<classic_protocol::message::client::SendFile>;

TEST_P(CodecMessageClientSendFileTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientSendFileTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::SendFile>
    codec_message_client_send_file_param[] = {
        {"somefile",    // testname
         {"somefile"},  // decoded
         {},            // caps
         {'s', 'o', 'm', 'e', 'f', 'i', 'l', 'e'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientSendFileTest,
    ::testing::ValuesIn(codec_message_client_send_file_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::AuthMethodData

using CodecMessageClientAuthMethodDataTest =
    CodecTest<classic_protocol::message::client::AuthMethodData>;

TEST_P(CodecMessageClientAuthMethodDataTest, encode) {
  test_encode(GetParam());
}
TEST_P(CodecMessageClientAuthMethodDataTest, decode) {
  test_decode(GetParam());
}

const CodecParam<classic_protocol::message::client::AuthMethodData>
    codec_message_client_auth_method_data_param[] = {
        {"somedata",    // testname
         {"somedata"},  // decoded
         {},            // caps
         {'s', 'o', 'm', 'e', 'd', 'a', 't', 'a'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientAuthMethodDataTest,
    ::testing::ValuesIn(codec_message_client_auth_method_data_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::ListFields

using CodecMessageClientListFieldsTest =
    CodecTest<classic_protocol::message::client::ListFields>;

TEST_P(CodecMessageClientListFieldsTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientListFieldsTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::ListFields>
    codec_message_client_list_fields_param[] = {
        {"some_table_no_wildcard",
         {"some_table", ""},  // decoded
         {},                  // caps
         {0x04, 's', 'o', 'm', 'e', '_', 't', 'a', 'b', 'l', 'e', '\0'}},
        {"some_table_some_wildcard",
         {"some_table", "foo"},  // decoded
         {},                     // caps
         {0x04, 's', 'o', 'm', 'e', '_', 't', 'a', 'b', 'l', 'e', '\0', 'f',
          'o', 'o'}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientListFieldsTest,
    ::testing::ValuesIn(codec_message_client_list_fields_param),
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

// client::Reload

using CodecMessageClientReloadTest =
    CodecTest<classic_protocol::message::client::Reload>;

TEST_P(CodecMessageClientReloadTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientReloadTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::Reload>
    codec_message_client_reload_param[] = {
        {"flush_privileges", {0}, {}, {0x07, 0x00}},
        {"flush_logs", {1}, {}, {0x07, 0x01}},
        {"flush_tables", {2}, {}, {0x07, 0x02}},
        {"flush_hosts", {3}, {}, {0x07, 0x03}},
        {"flush_status", {4}, {}, {0x07, 0x04}},
        {"flush_threads", {5}, {}, {0x07, 0x05}},
        {"reset_slave", {6}, {}, {0x07, 0x06}},
        {"reset_master", {7}, {}, {0x07, 0x07}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientReloadTest,
                         ::testing::ValuesIn(codec_message_client_reload_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// client::Kill

using CodecMessageClientKillTest =
    CodecTest<classic_protocol::message::client::Kill>;

TEST_P(CodecMessageClientKillTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientKillTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::Kill>
    codec_message_client_kill_param[] = {
        {"kill_low", {0}, {}, {0x0c, 0x00, 0x00, 0x00, 0x00}},
        {"kill_1", {1}, {}, {0x0c, 0x01, 0x00, 0x00, 0x00}},
        {"kill_high", {0xffffffff}, {}, {0x0c, 0xff, 0xff, 0xff, 0xff}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientKillTest,
                         ::testing::ValuesIn(codec_message_client_kill_param),
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
    os << "    - " << t.type_and_flags << "\n";
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
  test_decode(GetParam(), [](uint32_t /* statement_id */) {
    // one param
    return std::vector<
        classic_protocol::message::client::StmtExecute::ParamDef>{{}};
  });
}

const CodecParam<classic_protocol::message::client::StmtExecute>
    codec_stmt_execute_param[] = {
        {"one_param",
         {1,                                        // stmt-id
          0x00,                                     // flags
          1,                                        // iteration-count
          true,                                     // new-params bound
          {classic_protocol::field_type::Varchar},  // types
          {{"foo"}}},                               // values
         {},                                        // caps
         {
             // serialized
             0x17,                    // cmd
             0x01, 0x00, 0x00, 0x00,  // stmt-id
             0x00,                    // flags
             0x01, 0x00, 0x00, 0x00,  // iteration-count
             0x00,                    // null-bitmap
             0x01,                    // new-params bound
             0x0f, 0x00,              // parameter-type[0]: Varchar
             0x03, 0x66, 0x6f, 0x6f   // data[0]: len=3, "foo"
         }},
        {"one_null_param",
         {1,                                        // stmt-id
          0x00,                                     // flags
          1,                                        // iteration-count
          true,                                     // new-params bound
          {classic_protocol::field_type::Varchar},  // types
          {{std::nullopt}}},                        // values
         {},                                        // caps
         {
             // serialized
             0x17,                    // cmd
             0x01, 0x00, 0x00, 0x00,  // stmt-id
             0x00,                    // flags
             0x01, 0x00, 0x00, 0x00,  // iteration-count
             0x01,                    // null-bitmap: data[0]: null
             0x01,                    // new-params bound
             0x0f, 0x00,              // parameter-type[0]: Varchar
         }},
        {"cap_query_attributes_one_param_no_param_count_avail",
         {
             1,     // stmt-id
             0x00,  // flags: 0
             1,     // iteration_count: 1
             true,  // new-params bound
             {{classic_protocol::field_type::String, "abc"}},  // types
             {{"val"}},                                        // values
         },                                                    // msg
         {classic_protocol::capabilities::query_attributes},   // caps
         {
             // serialized
             0x17,                    // cmd
             0x01, 0x00, 0x00, 0x00,  // stmt-id
             0x00,                    // flags
             0x01, 0x00, 0x00, 0x00,  // iteration-count
             0x00,                    // null-bitmap
             0x01,                    // new-params bound
             0xfe, 0x00,              // parameter-type[0]: String
             0x03, 0x61, 0x62, 0x63,  // name[0]: len=3, "abc"
             0x03, 0x76, 0x61, 0x6c   // data[0]: len=3, "val"
         }},
        {"cap_query_attributes_one_param",
         {
             1,     // stmt-id
             0x08,  // flags
             1,     // iteration_count
             true,  // new-params bound
             {{classic_protocol::field_type::String, "abc"}},  // types
             {{"val"}},                                        // values
         },                                                    // msg
         {classic_protocol::capabilities::query_attributes},   // caps
         {
             // serialized
             0x17,                    // cmd
             0x01, 0x00, 0x00, 0x00,  // stmt-id
             0x08,                    // flags: param-count-available
             0x01, 0x00, 0x00, 0x00,  // iteration-count
             0x01,                    // param-count
             0x00,                    // null-bitmap
             0x01,                    // new-params bound
             0xfe, 0x00,              // parameter-type[0]: String
             0x03, 0x61, 0x62, 0x63,  // name[0]: len=3, "abc"
             0x03, 0x76, 0x61, 0x6c   // data[0]: len=3, "val"
         }},
        {"cap_query_attributes_null_param",
         {
             1,     // stmt-id
             0x08,  // flags
             1,     // iteration_count
             true,  // new-params bound
             {{classic_protocol::field_type::String, "abc"}},  // types
             {{std::nullopt}},                                 // values
         },                                                    // msg
         {classic_protocol::capabilities::query_attributes},   // caps
         {
             // serialized
             0x17,                    // cmd
             0x01, 0x00, 0x00, 0x00,  // stmt-id
             0x08,                    // flags: param-count-available
             0x01, 0x00, 0x00, 0x00,  // iteration-count
             0x01,                    // param-count
             0x01,                    // null-bitmap: data[0]: NULL
             0x01,                    // new-params bound
             0xfe, 0x00,              // parameter-type[0]: String
             0x03, 0x61, 0x62, 0x63,  // name[0]: len=3, "abc"
         }},
        {"cap_query_attributes_8_params",
         {
             1,     // stmt-id
             0x08,  // flags
             1,     // iteration_count
             true,  // new-params bound
             {
                 {classic_protocol::field_type::Bit, ""},
                 {classic_protocol::field_type::Blob, "1"},
                 {classic_protocol::field_type::Varchar, "2"},
                 {classic_protocol::field_type::VarString, "3"},
                 {classic_protocol::field_type::Set, "4"},
                 {classic_protocol::field_type::String, "5"},
                 {classic_protocol::field_type::Enum, "6"},
                 {classic_protocol::field_type::TinyBlob, "7"},
             },  // types
             {{"a"},
              {"bc"},
              {"def"},
              {"ghij"},
              {"klm"},
              {"no"},
              {"p"},
              {"qrstuvwxyz"}},                                // values
         },                                                   // msg
         {classic_protocol::capabilities::query_attributes},  // caps
         {
             // serialized
             0x17,                          // cmd
             0x01, 0x00, 0x00, 0x00,        // stmt-id
             0x08,                          // flags: param-count-available
             0x01, 0x00, 0x00, 0x00,        // iteration-count
             0x08,                          // param-count
             0x00,                          // null-bitmap
             0x01,                          // new-params bound
             0x10, 0x00,                    // parameter-type[0]: Bit
             0x00,                          // name[0]: len=0, ""
             0xfc, 0x00,                    // parameter-type[1]: Blob
             0x01, 0x31,                    // name[1]: len=1, "1"
             0x0f, 0x00,                    // parameter-type[2]: Varchar
             0x01, 0x32,                    // name[2]: len=1, "2"
             0xfd, 0x00,                    // parameter-type[3]: VarString
             0x01, 0x33,                    // name[3]: len=1, "3"
             0xf8, 0x00,                    // parameter-type[4]: Set
             0x01, 0x34,                    // name[4]: len=1, "4"
             0xfe, 0x00,                    // parameter-type[5]: String
             0x01, 0x35,                    // name[5]: len=1, "5"
             0xf7, 0x00,                    // parameter-type[6]: Enum
             0x01, 0x36,                    // name[6]: len=1, "6"
             0xf9, 0x00,                    // parameter-type[7]: TinyBlob
             0x01, 0x37,                    // name[7]: len=1, "7"
             0x01, 0x61,                    // data[0]: len=1, "a"
             0x02, 0x62, 0x63,              // data[1]: len=2, "bc"
             0x03, 0x64, 0x65, 0x66,        // data[2]: len=3, "def"
             0x04, 0x67, 0x68, 0x69, 0x6a,  // data[3]: len=4, "ghij"
             0x03, 0x6b, 0x6c, 0x6d,        // data[4]: len=5, "klm"
             0x02, 0x6e, 0x6f,              // data[5]: len=4, "no"
             0x01, 0x70,                    // data[6]: len=3, "p"
             0x0a, 0x71, 0x72, 0x73, 0x74,  // data[7]: len=10, "qrstuvwxyz"
             0x75, 0x76, 0x77, 0x78, 0x79,  //
             0x7a,                          //
         }},
        {"cap_query_attributes_9_params",
         {
             1,     // stmt-id
             0x08,  // flags
             1,     // iteration_count
             true,  // new-params bound
             {
                 {classic_protocol::field_type::Decimal, ""},
                 {classic_protocol::field_type::Tiny, "1"},
                 {classic_protocol::field_type::Short, "2"},
                 {classic_protocol::field_type::Long, "3"},
                 {classic_protocol::field_type::Float, "4"},
                 {classic_protocol::field_type::Double, "5"},
                 {classic_protocol::field_type::Null, "6"},
                 {classic_protocol::field_type::Timestamp, "7"},
                 {classic_protocol::field_type::LongLong, "8"},
             },  // types
             {{"\x1"},
              {"\x1"},
              {{"\x2\x0", 2}},
              {{"\x4\x0\x0\x0", 4}},
              {"\x7f\x7f\x7f\x7f"},
              {"\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"},
              {std::nullopt},
              {""},
              {{"\x08\x00\x00\x00\x00\x00\x00\x00", 8}}},     // values
         },                                                   // msg
         {classic_protocol::capabilities::query_attributes},  // caps
         {
             // serialized
             0x17,                    // cmd
             0x01, 0x00, 0x00, 0x00,  // stmt-id
             0x08,                    // flags: param-count-available
             0x01, 0x00, 0x00, 0x00,  // iteration-count
             0x09,                    // param-count
             0x40, 0x00,              // null-bitmap: data[6]: NULL
             0x01,                    // new-params bound
             0x00, 0x00,              // parameter-type[0]: Decimal
             0x00,                    // name[0]: len=0, ""
             0x01, 0x00,              // parameter-type[1]: Tiny
             0x01, 0x31,              // name[1]: len=1, "1"
             0x02, 0x00,              // parameter-type[2]: Short
             0x01, 0x32,              // name[2]: len=1, "2"
             0x03, 0x00,              // parameter-type[3]: Long
             0x01, 0x33,              // name[3]: len=1, "3"
             0x04, 0x00,              // parameter-type[4]: Float
             0x01, 0x34,              // name[4]: len=1, "4"
             0x05, 0x00,              // parameter-type[5]: Double
             0x01, 0x35,              // name[5]: len=1, "5"
             0x06, 0x00,              // parameter-type[6]: Null
             0x01, 0x36,              // name[6]: len=1, "6"
             0x07, 0x00,              // parameter-type[7]: Timestamp
             0x01, 0x37,              // name[7]: len=1, "7"
             0x08, 0x00,              // parameter-type[8]: LongLong
             0x01, 0x38,              // name[8]: len=1, "8"
             0x01, 0x01,              // data[0]: len=1, "a"
             0x01,                    // data[1]: <tiny>1
             0x02, 0x00,              // data[2]: <short>2
             0x04, 0x00, 0x00, 0x00,  // data[3]: <long>4
             0x7f, 0x7f, 0x7f, 0x7f,  // data[4]: <float>
             0x7f, 0x7f, 0x7f, 0x7f,  // data[5]: <double>
             0x7f, 0x7f, 0x7f, 0x7f,  //
                                      // data[6]: NULL
             0x00,                    // data[7]: len=0, ""
             0x08, 0x00, 0x00, 0x00,  // data[8]: len=10, "qrstuvwxyz"
             0x00, 0x00, 0x00, 0x00,  //
         }},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientStmtExecuteTest,
                         ::testing::ValuesIn(codec_stmt_execute_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

TEST(CodecMessageClientStmtExecuteFail, param_count_less_than_num_params) {
  using msg_type = classic_protocol::message::client::StmtExecute;

  auto caps = classic_protocol::capabilities::query_attributes;

  std::array<uint8_t, 11> encoded{
      0x17,                    // cmd
      0x01, 0x00, 0x00, 0x00,  // stmt-id
      0x10,                    // flags: param-count-available
      0x01, 0x00, 0x00, 0x00,  // iteration-count
      0x00,                    // param-count
  };

  auto decode_res = classic_protocol::Codec<msg_type>::decode(
      net::buffer(encoded), caps,
      [](auto /* stmt_id */) { return std::vector<msg_type::ParamDef>{0xff}; });
  EXPECT_FALSE(decode_res);
}

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

using CodecMessageClientSetOptionTest =
    CodecTest<classic_protocol::message::client::SetOption>;

TEST_P(CodecMessageClientSetOptionTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageClientSetOptionTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::client::SetOption>
    codec_stmt_set_option_param[] = {
        {"set_option_0", {0}, {}, {0x1b, 0x00, 0x00}},  // multi-stmts-off
        {"set_option_1", {1}, {}, {0x1b, 0x01, 0x00}},  // multi-stmts-on
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageClientSetOptionTest,
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

namespace classic_protocol::message::client {
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
}  // namespace classic_protocol::message::client

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
            {"3_23_58_empty_schema_server_no_schema",
             {0x240d, 0, 0, "root", "H]^CSVY[", "", "", ""},
             {},           // server doesn't support "connect_with_schema"
             {0x0d, 0x24,  // caps (connect_with_schema set)
              0, 0, 0,     // max-packet-size
              'r', 'o', 'o', 't', 0,  // username
              'H', ']', '^', 'C', 'S', 'V', 'Y', '['}},
            {"3_23_58_no_schema",
             {0x2405, 0, 0, "root", "H]^CSVY[", "", "", ""},
             classic_protocol::capabilities::connect_with_schema,
             {0x05, 0x24,             // caps (no connect_with_schema)
              0, 0, 0,                // max-packet-size
              'r', 'o', 'o', 't', 0,  // username
              'H', ']', '^', 'C', 'S', 'V', 'Y', '['}},
            {"3_23_58_empty_schema",
             {0x240d, 0, 0, "root", "H]^CSVY[", "", "", ""},
             classic_protocol::capabilities::connect_with_schema,
             {0x0d, 0x24,             // caps (connect-with-schema set)
              0, 0, 0,                // max-packet-size
              'r', 'o', 'o', 't', 0,  // username
              'H', ']', '^', 'C', 'S', 'V', 'Y', '[', '\0'}},
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
            {
                "choma",
                {
                    0b1011'1010'0010'0000'1111,  // caps:
                                                 // long-pass
                                                 // found-rows
                                                 // long-flag
                                                 // connect-with-schema
                                                 // protocol_41
                                                 // transactions
                                                 // secure_connections
                                                 // plugin_auth (set, but then
                                                 // not used)
                    (1 << 24) - 1,               // max-packet-size
                    0xff,                        // collation
                    "myroot",                    // user
                    "\x14\xa5\xed\xe0\xdf\x96\x9d\x5e"
                    "\xca\xa3\x45\xc3\x93\x55\xfe\x22"
                    "\x99\x62\xc9\xed",  // authdata
                    "mysql",             // schema
                    "",                  // authmethod
                    {}                   // attributes
                },                       // client::Greeting
                0xffffffff,              // server-caps
                {0x0f, 0xa2, 0x0b, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x6d, 0x79, 0x72, 0x6f, 0x6f, 0x74, 0x00, 0x14,
                 0x14, 0xa5, 0xed, 0xe0, 0xdf, 0x96, 0x9d, 0x5e, 0xca, 0xa3,
                 0x45, 0xc3, 0x93, 0x55, 0xfe, 0x22, 0x99, 0x62, 0xc9, 0xed,
                 0x6d, 0x79, 0x73, 0x71, 0x6c, 0x00}  // bytes
            },
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageClientGreetingTest,
    ::testing::ValuesIn(codec_message_client_greeting_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

// client::ChangeUser

namespace classic_protocol::message::client {
std::ostream &operator<<(std::ostream &os, const ChangeUser &v) {
  os << "ChangeUser: "
     << "\n";
  os << "  schema: " << v.schema() << "\n";
  os << "  username: " << v.username() << "\n";
  os << "  auth_method_name: " << v.auth_method_name() << "\n";

  return os;
}
}  // namespace classic_protocol::message::client

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
         {{classic_protocol::field_type::VarString}, {std::nullopt}},
         {},
         {0x00, 0x04}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CodecMessageServerStmtRowTest,
    ::testing::ValuesIn(codec_message_server_stmtrow_param),
    [](auto const &test_param_info) {
      return test_param_info.param.test_name;
    });

using CodecMessageServerStatisticsTest =
    CodecTest<classic_protocol::message::server::Statistics>;

TEST_P(CodecMessageServerStatisticsTest, encode) { test_encode(GetParam()); }
TEST_P(CodecMessageServerStatisticsTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::message::server::Statistics>
    codec_server_statistics_param[] = {
        {"statistics",
         {"Uptime: 38605  Threads: 6  Questions: 137  Slow queries: 0  Opens: "
          "186  Flush tables: 3  Open tables: 101  Queries per second avg: "
          "0.003"},
         {},  // caps
         {0x55, 0x70, 0x74, 0x69, 0x6d, 0x65, 0x3a, 0x20, 0x33, 0x38, 0x36,
          0x30, 0x35, 0x20, 0x20, 0x54, 0x68, 0x72, 0x65, 0x61, 0x64, 0x73,
          0x3a, 0x20, 0x36, 0x20, 0x20, 0x51, 0x75, 0x65, 0x73, 0x74, 0x69,
          0x6f, 0x6e, 0x73, 0x3a, 0x20, 0x31, 0x33, 0x37, 0x20, 0x20, 0x53,
          0x6c, 0x6f, 0x77, 0x20, 0x71, 0x75, 0x65, 0x72, 0x69, 0x65, 0x73,
          0x3a, 0x20, 0x30, 0x20, 0x20, 0x4f, 0x70, 0x65, 0x6e, 0x73, 0x3a,
          0x20, 0x31, 0x38, 0x36, 0x20, 0x20, 0x46, 0x6c, 0x75, 0x73, 0x68,
          0x20, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x73, 0x3a, 0x20, 0x33, 0x20,
          0x20, 0x4f, 0x70, 0x65, 0x6e, 0x20, 0x74, 0x61, 0x62, 0x6c, 0x65,
          0x73, 0x3a, 0x20, 0x31, 0x30, 0x31, 0x20, 0x20, 0x51, 0x75, 0x65,
          0x72, 0x69, 0x65, 0x73, 0x20, 0x70, 0x65, 0x72, 0x20, 0x73, 0x65,
          0x63, 0x6f, 0x6e, 0x64, 0x20, 0x61, 0x76, 0x67, 0x3a, 0x20, 0x30,
          0x2e, 0x30, 0x30, 0x33}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecMessageServerStatisticsTest,
                         ::testing::ValuesIn(codec_server_statistics_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
