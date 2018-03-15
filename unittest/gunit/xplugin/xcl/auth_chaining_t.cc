/* Copyright (c) 2017, 2018 Oracle and/or its affiliates. All rights reserved.

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
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "unittest/gunit/xplugin/xcl/session_t.h"

namespace xcl {
namespace test {

class Auth_chaining_test_suite : public Xcl_session_impl_tests {
 public:
  void SetUp() {
    m_sut = prepare_session();
    EXPECT_CALL(m_mock_connection_state, is_connected())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(m_mock_connection, connect(_, _, _))
        .WillRepeatedly(Return(XError{0, ""}));
  }

  void set_ssl_state(bool is_enabled) {
    EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
        .WillRepeatedly(Return(is_enabled));
    EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
        .WillRepeatedly(Return(is_enabled));
  }

  void fill(Mysqlx::Datatypes::Array *cap,
            const std::vector<std::string> &values) {
    for (const auto &value : values) {
      auto any = cap->add_value();
      any->set_type(Mysqlx::Datatypes::Any_Type_SCALAR);

      auto scalar = any->mutable_scalar();

      scalar->set_type(Mysqlx::Datatypes::Scalar_Type_V_STRING);

      scalar->mutable_v_string()->set_value(value);
    }
  }

  template <typename... Types>
  Mysqlx::Connection::Capabilities *make_capability(const Types &... values) {
    auto result = new Mysqlx::Connection::Capabilities();
    auto cap = result->add_capabilities();

    cap->set_name("authentication.mechanisms");
    auto value = cap->mutable_value();

    value->set_type(Mysqlx::Datatypes::Any_Type_ARRAY);

    fill(value->mutable_array(), {values...});

    return result;
  }

  XError m_ok_auth{ngs::Authentication_interface::Status::Succeeded, ""};
  XError m_failed_auth{ER_NO_SUCH_USER, "Invalid user or password"};
};

TEST_F(Auth_chaining_test_suite, cap_auth_method_server_supports_nothing) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "FROM_CAPABILITIES");

  EXPECT_CALL(*m_mock_protocol, execute_fetch_capabilities_raw(_))
      .WillOnce(Return(make_capability()));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  ASSERT_EQ(CR_X_INVALID_AUTH_METHOD,
            m_sut->connect("host", 1290, "user", "pass", "schema").error());
}

TEST_F(Auth_chaining_test_suite, cap_auth_method_server_supports_plain_no_ssl) {
  set_ssl_state(false);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "FROM_CAPABILITIES");

  EXPECT_CALL(*m_mock_protocol, execute_fetch_capabilities_raw(_))
      .WillOnce(Return(make_capability("PLAIN")));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  ASSERT_EQ(CR_X_INVALID_AUTH_METHOD,
            m_sut->connect("host", 1290, "user", "pass", "schema").error());
}

TEST_F(Auth_chaining_test_suite, cap_auth_method_server_supports_plain) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "FROM_CAPABILITIES");

  EXPECT_CALL(*m_mock_protocol, execute_fetch_capabilities_raw(_))
      .WillOnce(Return(make_capability("PLAIN")));

  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, cap_auth_method_server_supports_mysql41) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "FROM_CAPABILITIES");

  EXPECT_CALL(*m_mock_protocol, execute_fetch_capabilities_raw(_))
      .WillOnce(Return(make_capability("MYSQL41")));

  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, cap_auth_method_server_supports_memory) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "FROM_CAPABILITIES");

  EXPECT_CALL(*m_mock_protocol, execute_fetch_capabilities_raw(_))
      .WillOnce(Return(make_capability("SHA256_MEMORY")));

  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite,
       cap_auth_method_server_supports_memory_and_unk) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "FROM_CAPABILITIES");

  EXPECT_CALL(*m_mock_protocol, execute_fetch_capabilities_raw(_))
      .WillOnce(Return(make_capability("SHA256_MEMORY", "UNK")));

  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, cap_auth_method_server_supports_all_ssl) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "FROM_CAPABILITIES");

  EXPECT_CALL(*m_mock_protocol, execute_fetch_capabilities_raw(_))
      .WillOnce(Return(make_capability("SHA256_MEMORY", "PLAIN", "MYSQL41")));

  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, cap_auth_method_server_supports_all_non_ssl) {
  set_ssl_state(false);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "FROM_CAPABILITIES");

  EXPECT_CALL(*m_mock_protocol, execute_fetch_capabilities_raw(_))
      .WillOnce(Return(make_capability("SHA256_MEMORY", "MYSQL41")));

  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, auto_auth_method) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "AUTO");
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, auto_auth_method_ssl_disabled) {
  set_ssl_state(false);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "AUTO");
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, auto_auth_method_unix_socket_connection) {
  set_ssl_state(false);
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Unix_socket));
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          "AUTO");
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, ambigous_auth_method) {
  set_ssl_state(false);
  ASSERT_EQ(
      CR_X_UNSUPPORTED_OPTION_VALUE,
      m_sut
          ->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                             std::vector<std::string>{"AUTO", "PLAIN"})
          .error());

  // Default value is not changed
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, ambigous_auth_method_multiple_auto) {
  set_ssl_state(false);
  ASSERT_EQ(
      CR_X_UNSUPPORTED_OPTION_VALUE,
      m_sut
          ->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                             std::vector<std::string>{"AUTO", "AUTO"})
          .error());

  // Default value is not changed
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite,
       auto_auth_method_in_compatibility_mode_with_ssl_disabled) {
  set_ssl_state(false);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"FALLBACK"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite,
       auto_auth_method_in_compatibility_mode_with_ssl_enabled) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"FALLBACK"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite,
       auto_auth_method_in_compatibility_mode_using_unix_socket) {
  set_ssl_state(false);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"FALLBACK"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Unix_socket));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, only_wrong_auth_method) {
  set_ssl_state(true);
  // Wrong method given will mean that we do not use user provided auth and
  // resort to Auth::AUTO
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"FOOBAR_AUTH"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, wrong_and_good_auth_method) {
  set_ssl_state(true);
  // Wrong method given will mean that we do not use user provided auth and
  // resort to Auth::AUTO
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"FOOBAR_AUTH", "MYSQL41"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, only_sha256_memory_auth_method_ssl_disabled) {
  set_ssl_state(false);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"SHA256_MEMORY"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, only_sha256_memory_auth_method_ssl_enabled) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"SHA256_MEMORY"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, only_mysql41_auth_method_ssl_disabled) {
  set_ssl_state(false);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"MYSQL41"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, only_mysql41_auth_method_ssl_enabled) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"MYSQL41"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, only_plain_method_ssl_disabled) {
  set_ssl_state(false);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"PLAIN"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .Times(0);
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, only_plain_method_ssl_enabled) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"PLAIN"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, custom_sequence_of_two_auths) {
  set_ssl_state(true);
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"PLAIN", "MYSQL41"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, custom_sequence_of_three_auths) {
  set_ssl_state(true);
  m_sut->set_mysql_option(
      XSession::Mysqlx_option::Authentication_method,
      std::vector<std::string>{"PLAIN", "SHA256_MEMORY", "MYSQL41"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, duplicate_auth_methods) {
  set_ssl_state(true);
  // Duplicate auth methods are not an error
  m_sut->set_mysql_option(XSession::Mysqlx_option::Authentication_method,
                          std::vector<std::string>{"MYSQL41", "MYSQL41"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .Times(2)
      .WillRepeatedly(Return(m_failed_auth));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

TEST_F(Auth_chaining_test_suite, sequence_with_plain_and_no_ssl) {
  set_ssl_state(false);

  m_sut->set_mysql_option(
      XSession::Mysqlx_option::Authentication_method,
      std::vector<std::string>{"MYSQL41", "PLAIN", "SHA256_MEMORY"});

  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .Times(0);
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "SHA256_MEMORY"))
      .WillOnce(Return(m_failed_auth));

  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));

  ASSERT_EQ(ER_NO_SUCH_USER,
            m_sut->connect("host", 1290, "user", "pass", "schema").error());
}

TEST_F(Auth_chaining_test_suite, sequence_successfull_auth_attempt) {
  set_ssl_state(true);
  m_sut->set_mysql_option(
      XSession::Mysqlx_option::Authentication_method,
      std::vector<std::string>{"MYSQL41", "PLAIN", "SHA256_MEMORY"});
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "MYSQL41"))
      .WillOnce(Return(m_failed_auth));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(_, _, _, "PLAIN"))
      .WillOnce(Return(XError{}));
  EXPECT_CALL(m_mock_connection_state, get_connection_type())
      .WillOnce(Return(Connection_type::Tcp));
  m_sut->connect("host", 1290, "user", "pass", "schema");
}

}  // namespace test
}  // namespace xcl
