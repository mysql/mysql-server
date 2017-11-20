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

#include "errmsg.h"
#include "session_t.h"
#include "message_helpers.h"
#include "my_inttypes.h"
#include "mysqlx_error.h"
#include "mysqlx_version.h"


namespace xcl {
namespace test {
namespace {

const char *expected_user = "user";
const char *expected_pass = "user_pass";
const char *expected_host = "host";
const char *expected_socket_file = "socket_file";
const char *expected_schema = "schema";
const uint16 expected_port = 1290;
const int   expected_error_code = 10;
const int   expected_error_code_success = 0;

}  // namespace

using ::testing::An;
using ::testing::InvokeWithoutArgs;

class Xcl_session_impl_tests_connect : public Xcl_session_impl_tests {
 public:
  void expect_nothing() {
  }

  XError assert_reauthenticate(const int error_code) {
    EXPECT_CALL(*m_mock_protocol, send(An<const ::Mysqlx::Session::Reset&>())).
        WillOnce(Return(XError{}));
    EXPECT_CALL(*m_mock_protocol, recv_ok()).
        WillOnce(Return(XError{error_code, ""}));

    auto result = m_sut->reauthenticate(
          expected_user,
          expected_pass,
          expected_schema);

    return result;
  }

  XError assert_connect_to_localhost(const int error_code) {
    EXPECT_CALL(m_mock_connection, connect_to_localhost(
        expected_socket_file))
      .WillOnce(Return(XError{error_code, ""}));

    return m_sut->connect(
        expected_socket_file,
        expected_user,
        expected_pass,
        expected_schema);
  }

  XError assert_connect(const int error_code) {
    EXPECT_CALL(m_mock_connection, connect(
        expected_host, expected_port, Internet_protocol::Any))
      .WillOnce(Return(XError{error_code, ""}));

    return m_sut->connect(
        expected_host,
        expected_port,
        expected_user,
        expected_pass,
        expected_schema);
  }

  void expect_protocol_any_connection() {
    m_connection_number = 0;
    auto count_connections = [this]() -> XError {
      ++m_connection_number;

      return {};
    };

    EXPECT_CALL(m_mock_connection, connect(
        expected_host, expected_port, Internet_protocol::Any))
      .WillRepeatedly(InvokeWithoutArgs(count_connections));

    EXPECT_CALL(m_mock_connection, connect_to_localhost(
        expected_socket_file))
      .WillRepeatedly(InvokeWithoutArgs(count_connections));
  }

  void expect_connection_number_to_be_one() {
    EXPECT_EQ(1, m_connection_number);
  }

  int m_connection_number;
};

TEST_F(Xcl_session_impl_tests_connect, reauthenticate_not_connected) {
  auto error = m_sut->reauthenticate(
      expected_user,
      expected_pass,
      expected_schema);

  ASSERT_EQ(CR_CONNECTION_ERROR, error.error());
}

TEST_F(Xcl_session_impl_tests_connect, reauthenticate_send_reset_failed) {
  const int expected_error_code = 10;

  // Old and new object (sut_) share mocks. Object in sut_ must
  // be released first to not interfere with new mocks expectations
  m_sut.reset();
  m_sut.reset(make_sut(true).release());

  EXPECT_CALL(*m_mock_protocol, send(An<const ::Mysqlx::Session::Reset&>())).
      WillOnce(Return(XError{expected_error_code, ""}));
  auto error = m_sut->reauthenticate(
      expected_user,
      expected_pass,
      expected_schema);

  ASSERT_EQ(expected_error_code, error.error());

  expect_connection_close();
}

TEST_F(Xcl_session_impl_tests_connect, connection_tcp_already_connected) {
  // Old and new object (sut_) share mocks. Object in sut_ must
  // be released first to not interfere with new mocks expectations
  m_sut.reset();
  m_sut.reset(make_sut(true).release());

  auto error = m_sut->connect(
      expected_host,
      expected_port,
      expected_user,
      expected_pass,
      expected_schema);

  ASSERT_EQ(CR_ALREADY_CONNECTED, error.error());

  error = m_sut->connect(
        expected_socket_file,
        expected_user,
        expected_pass,
        expected_schema);

  ASSERT_EQ(CR_ALREADY_CONNECTED, error.error());

  expect_connection_close();
}

TEST_F(Xcl_session_impl_tests_connect, connect_nullptrs) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(
      "",
      "",
      "",
      "MYSQL41")).WillOnce(Return(XError{}));

  EXPECT_CALL(m_mock_connection, connect(
      "", MYSQLX_TCP_PORT, Internet_protocol::Any))
    .WillOnce(Return(XError{0, ""}));

  const auto error = m_sut->connect(
      nullptr,
      0,
      nullptr,
      nullptr,
      nullptr);

  ASSERT_FALSE(error);
}

TEST_F(Xcl_session_impl_tests_connect, connect_localhost_nullptrs) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(
      "",
      "",
      "",
      "MYSQL41")).WillOnce(Return(XError{}));

  EXPECT_CALL(m_mock_connection, connect_to_localhost(
      MYSQLX_UNIX_ADDR))
    .WillOnce(Return(XError{0, ""}));

  const auto error = m_sut->connect(
      nullptr,
      nullptr,
      nullptr,
      nullptr);

  ASSERT_FALSE(error);
}

class Xcl_session_impl_tests_connected :
    public Xcl_session_impl_tests_connect {
 public:
  void SetUp() override {
    m_sut = make_sut(true);
  }
};

using Connection_method = XError (Xcl_session_impl_tests_connect::*)
    (const int);

using Closure_method = void (Xcl_session_impl_tests_connect::*)
    ();

struct Open_close_mothods {
  Connection_method m_open;
  Closure_method m_close;
  bool m_is_connected;
  static const bool start_connected = true;
  static const bool start_disconnected = false;
};

void PrintTo(const Open_close_mothods &m, ::std::ostream* os) {
  *os << "Open_close_mothods { m_is_connected:"<< m.m_is_connected << " }";
}

class Xcl_session_impl_tests_connect_param :
    public Xcl_session_impl_tests_connect,
    public WithParamInterface<Open_close_mothods> {
 public:
  using CapabilitiesSet = ::Mysqlx::Connection::CapabilitiesSet;

 public:
  void SetUp() override {
    m_sut = make_sut(GetParam().m_is_connected);
  }

  const Message_from_str<CapabilitiesSet> m_cap_set_tls {
    "capabilities { capabilities { "
    "        name: \"tls\""
    "        value {type: SCALAR scalar { type: V_BOOL v_bool: 1 } }"
    "} }"
  };

  const Message_from_str<CapabilitiesSet> m_cap_expired {
    "capabilities { capabilities {"
    "    name: \"client.pwd_expire_ok\" "
    "    value { type: SCALAR scalar { type: V_BOOL v_bool: 1 } }"
    "} }"
  };
};

TEST_P(Xcl_session_impl_tests_connect_param, connect_mysql41_nocaps) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(
      expected_user,
      expected_pass,
      expected_schema,
      "MYSQL41")).WillOnce(Return(XError{}));

  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_FALSE(error);

  (this->*GetParam().m_close)();
}

TEST_P(Xcl_session_impl_tests_connect_param, connect_mysql41_caps) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*m_mock_protocol,
              execute_set_capability(Cmp_msg(m_cap_expired)))
      .WillOnce(Return(XError{}));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(
      expected_user,
      expected_pass,
      expected_schema,
      "MYSQL41")).WillOnce(Return(XError{}));

  m_sut->set_capability(XSession::Capability_can_handle_expired_password, true);
  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_FALSE(error);

  (this->*GetParam().m_close)();
}

TEST_P(Xcl_session_impl_tests_connect_param, connect_mysql41_caps_fails) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*m_mock_protocol,
              execute_set_capability(Cmp_msg(m_cap_expired)))
      .WillOnce(Return(XError{expected_error_code, ""}));

  m_sut->set_capability(XSession::Capability_can_handle_expired_password, true);
  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_EQ(expected_error_code, error.error());

  (this->*GetParam().m_close)();
}

TEST_P(Xcl_session_impl_tests_connect_param, connect_mysql41_nocaps_auth_fail) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(
      expected_user,
      expected_pass,
      expected_schema,
      "MYSQL41")).WillOnce(Return(XError{expected_error_code, ""}));

  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_EQ(expected_error_code, error.error());

  (this->*GetParam().m_close)();
}

TEST_P(Xcl_session_impl_tests_connect_param,
       connect_plain_nocaps_when_tls_already_works) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(
      expected_user,
      expected_pass,
      expected_schema,
      "PLAIN")).WillOnce(Return(XError{}));

  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_FALSE(error);

  (this->*GetParam().m_close)();
}

TEST_P(Xcl_session_impl_tests_connect_param,
       connect_plain_tls_cap_fails) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*m_mock_protocol,
              execute_set_capability(Cmp_msg(m_cap_set_tls)))
      .WillOnce(Return(XError{expected_error_code, ""}));

  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_EQ(expected_error_code, error.error());

  (this->*GetParam().m_close)();
}

TEST_P(Xcl_session_impl_tests_connect_param,
       connect_plain_tls_activate_fails) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*m_mock_protocol,
              execute_set_capability(Cmp_msg(m_cap_set_tls)))
      .WillOnce(Return(XError{}));
  EXPECT_CALL(m_mock_connection, activate_tls())
      .WillOnce(Return(XError{expected_error_code, ""}));

  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_EQ(expected_error_code, error.error());

  (this->*GetParam().m_close)();
}

TEST_P(Xcl_session_impl_tests_connect_param,
       connect_plain_tls) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillOnce(Return(false)).WillOnce(Return(true));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*m_mock_protocol,
              execute_set_capability(Cmp_msg(m_cap_set_tls)))
      .WillOnce(Return(XError{}));
  EXPECT_CALL(m_mock_connection, activate_tls())
      .WillOnce(Return(XError{}));
  EXPECT_CALL(*m_mock_protocol, execute_authenticate(
      expected_user,
      expected_pass,
      expected_schema,
      "PLAIN")).WillOnce(Return(XError{}));

  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_FALSE(error);

  (this->*GetParam().m_close)();
}

TEST_P(Xcl_session_impl_tests_connect_param,
       connect_plain_tls_caps_fail) {
  EXPECT_CALL(m_mock_connection_state, is_ssl_activated())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(m_mock_connection_state, is_ssl_configured())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*m_mock_protocol,
              execute_set_capability(Cmp_msg(m_cap_expired)))
      .WillOnce(Return(XError{expected_error_code}));

  m_sut->set_capability(XSession::Capability_can_handle_expired_password, true);
  auto error = (this->*GetParam().m_open)(expected_error_code_success);

  ASSERT_EQ(expected_error_code, error.error());

  (this->*GetParam().m_close)();
}

INSTANTIATE_TEST_CASE_P(InstantiationConnectionMethod,
    Xcl_session_impl_tests_connect_param,
    Values(
        Open_close_mothods {
            &Xcl_session_impl_tests_connect_param::assert_connect,
            &Xcl_session_impl_tests_connect_param::expect_nothing,
            Open_close_mothods::start_disconnected },

        Open_close_mothods {
            &Xcl_session_impl_tests_connect_param::assert_connect_to_localhost,
            &Xcl_session_impl_tests_connect_param::expect_nothing,
            Open_close_mothods::start_disconnected },

        Open_close_mothods {
            &Xcl_session_impl_tests_connect_param::assert_reauthenticate,
            &Xcl_session_impl_tests_connect_param::expect_connection_close,
            Open_close_mothods::start_connected }));

class Xcl_session_impl_tests_connect_fails_param:
    public Xcl_session_impl_tests_connect_param {
 public:
};

TEST_P(Xcl_session_impl_tests_connect_fails_param, connect_fails) {
  auto error = (this->*GetParam().m_open)(ER_X_SESSION);

  ASSERT_EQ(ER_X_SESSION, error.error());

  (this->*GetParam().m_close)();
}

INSTANTIATE_TEST_CASE_P(InstantiationConnectionMethod,
                        Xcl_session_impl_tests_connect_fails_param,
    Values(
        Open_close_mothods {
            &Xcl_session_impl_tests_connect_param::assert_connect,
            &Xcl_session_impl_tests_connect_param::expect_nothing,
            Open_close_mothods::start_disconnected },

        Open_close_mothods {
            &Xcl_session_impl_tests_connect_param::assert_connect_to_localhost,
            &Xcl_session_impl_tests_connect_param::expect_nothing,
            Open_close_mothods::start_disconnected },

        Open_close_mothods {
            &Xcl_session_impl_tests_connect_param::assert_reauthenticate,
            &Xcl_session_impl_tests_connect_param::expect_connection_close,
            Open_close_mothods::start_connected }));

}  // namespace test
}  // namespace xcl
