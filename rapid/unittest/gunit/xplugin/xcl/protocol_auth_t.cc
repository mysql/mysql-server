/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "protocol_t.h"

#include <string>

#include "message_helpers.h"


namespace xcl {
namespace test {

class Xcl_protocol_impl_tests_auth : public Xcl_protocol_impl_tests {
 public:
  using AuthenticateStart    = ::Mysqlx::Session::AuthenticateStart;
  using AuthenticateOk       = ::Mysqlx::Session::AuthenticateOk;
  using AuthenticateContinue = ::Mysqlx::Session::AuthenticateContinue;

 public:
  void assert_authenticate(const std::string &mech,
                           const int32 error_code = 0) {
    auto error = m_sut->execute_authenticate(
        expected_user, expected_pass, expected_schema, mech);

    ASSERT_EQ(error_code, error.error());
  }

  const std::string expected_user   = "user";
  const std::string expected_pass   = "pass";
  const std::string expected_schema = "schema";
  const Message_from_str<AuthenticateStart> m_msg_auth_start_plain{
    "mech_name: \"PLAIN\" "
    "auth_data: \"schema\\0user\\0pass\" "
  };

  const Message_from_str<AuthenticateStart> m_msg_auth_start_mysql41{
    "mech_name: \"MYSQL41\" "
  };

  const Message_from_str<AuthenticateContinue> m_msg_auth_cont_c{
    "auth_data: \"schema\\0user\\0*ACFC0C3FA7F3C1F39849B44177D8B82C7F75E0D1\""
  };
};

TEST_F(Xcl_protocol_impl_tests_auth, execute_authenticate_invalid_method) {
  assert_authenticate("INVALID", CR_X_INVALID_AUTH_METHOD);
  assert_authenticate("plain",   CR_X_INVALID_AUTH_METHOD);
  assert_authenticate("mysql41", CR_X_INVALID_AUTH_METHOD);
}

TEST_F(Xcl_protocol_impl_tests_auth, execute_authenticate_plain_method) {
  expect_write_message(m_msg_auth_start_plain);
  expect_read_message_without_payload(AuthenticateOk());

  assert_authenticate("PLAIN");
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_plain_method_error_msg) {
  const int32 expected_error_code = 30001;
  auto        msg_error = Server_message<::Mysqlx::Error>::make_required();

  msg_error.set_code(expected_error_code);

  expect_write_message(m_msg_auth_start_plain);
  expect_read_message(msg_error);

  assert_authenticate("PLAIN", expected_error_code);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_plain_method_unexpected_msg) {
  auto msg_stmt_ok =
      Server_message<::Mysqlx::Sql::StmtExecuteOk>::make_required();

  expect_write_message(m_msg_auth_start_plain);
  expect_read_message_without_payload(msg_stmt_ok);

  assert_authenticate("PLAIN", CR_MALFORMED_PACKET);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_plain_method_io_error_at_write) {
  const int32 expected_error_code = 30002;

  expect_write_message_without_payload(
      m_msg_auth_start_plain,
      expected_error_code);

  assert_authenticate("PLAIN", expected_error_code);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_plain_method_io_error_at_read) {
  const int32 expected_error_code = 30003;

  expect_write_message(m_msg_auth_start_plain);
  expect_read_message_without_payload(
      AuthenticateOk(),
      expected_error_code);

  assert_authenticate("PLAIN", expected_error_code);
}

TEST_F(Xcl_protocol_impl_tests_auth, execute_authenticate_mysql41_method) {
  auto msg_auth_cont_s =
      Server_message<::Mysqlx::Session::AuthenticateContinue>::make_required();

  {
    InSequence s;

    expect_write_message(m_msg_auth_start_mysql41);
    expect_read_message(msg_auth_cont_s);
    expect_write_message(m_msg_auth_cont_c);
    expect_read_message_without_payload(AuthenticateOk());
  }

  assert_authenticate("MYSQL41");
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_mysql41_method_recv_error_msg1) {
  const int32 expected_error_code = 30004;
  auto msg_error =
      Server_message<::Mysqlx::Error>::make_required();

  msg_error.set_code(expected_error_code);

  expect_write_message(m_msg_auth_start_mysql41);
  expect_read_message(msg_error);

  assert_authenticate("MYSQL41", expected_error_code);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_mysql41_method_recv_error_msg2) {
  const int32 expected_error_code = 30005;
  auto msg_auth_cont_s =
      Server_message<::Mysqlx::Session::AuthenticateContinue>::make_required();

  auto msg_error =
      Server_message<::Mysqlx::Error>::make_required();

  msg_error.set_code(expected_error_code);

  {
    InSequence s;

    expect_write_message(m_msg_auth_start_mysql41);
    expect_read_message(msg_auth_cont_s);
    expect_write_message(m_msg_auth_cont_c);
    expect_read_message(msg_error);
  }

  assert_authenticate("MYSQL41", expected_error_code);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_mysql41_method_recv_unexpected_msg1) {
  auto msg_unexpected =
      Server_message<::Mysqlx::Ok>::make_required();

  {
    InSequence s;

    expect_write_message(m_msg_auth_start_mysql41);
    expect_read_message_without_payload(msg_unexpected);
  }

  assert_authenticate("MYSQL41", CR_MALFORMED_PACKET);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_mysql41_method_recv_unexpected_msg2) {
  auto msg_auth_cont_s =
      Server_message<::Mysqlx::Session::AuthenticateContinue>::make_required();

  auto msg_unexpected =
      Server_message<::Mysqlx::Ok>::make_required();

  {
    InSequence s;

    expect_write_message(m_msg_auth_start_mysql41);
    expect_read_message(msg_auth_cont_s);
    expect_write_message(m_msg_auth_cont_c);
    expect_read_message_without_payload(msg_unexpected);
  }

  assert_authenticate("MYSQL41", CR_MALFORMED_PACKET);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_mysql41_method_recv_io_error1) {
  const int32 expected_error_code =  30006;
  auto        msg_auth_cont_s =
      Server_message<::Mysqlx::Session::AuthenticateContinue>::make_required();

  expect_write_message(m_msg_auth_start_mysql41);
  expect_read_message_without_payload(msg_auth_cont_s, expected_error_code);

  assert_authenticate("MYSQL41", expected_error_code);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_mysql41_method_recv_io_error2) {
  const int32 expected_error_code =  30007;
  auto msg_auth_cont_s =
      Server_message<::Mysqlx::Session::AuthenticateContinue>::make_required();

  {
    InSequence s;

    expect_write_message(m_msg_auth_start_mysql41);
    expect_read_message(msg_auth_cont_s);
    expect_write_message(m_msg_auth_cont_c);
    expect_read_message_without_payload(AuthenticateOk(), expected_error_code);
  }

  assert_authenticate("MYSQL41", expected_error_code);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_mysql41_method_write_io_error1) {
  const int32 expected_error_code =  30008;

  expect_write_message_without_payload(
      m_msg_auth_start_mysql41,
      expected_error_code);

  assert_authenticate("MYSQL41", expected_error_code);
}

TEST_F(Xcl_protocol_impl_tests_auth,
       execute_authenticate_mysql41_method_write_io_error2) {
  const int32 expected_error_code =  30009;
  auto msg_auth_cont_s =
      Server_message<::Mysqlx::Session::AuthenticateContinue>::make_required();

  {
    InSequence s;

    expect_write_message(m_msg_auth_start_mysql41);
    expect_read_message(msg_auth_cont_s);
    expect_write_message(m_msg_auth_cont_c, expected_error_code);
  }

  assert_authenticate("MYSQL41", expected_error_code);
}


}  // namespace test
}  // namespace xcl
