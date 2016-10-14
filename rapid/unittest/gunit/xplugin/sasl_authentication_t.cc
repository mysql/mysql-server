
/* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */



#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ngs/memory.h"
#include "auth_plain.h"
#include "mock/session.h"
#include "mock/ngs_general.h"

#include <stdexcept>


namespace xpl
{

namespace test
{

using namespace ::testing;


template<typename Auth_type>
class AuthenticationTestSuite : public Test
{
public:
  static void dont_delete_ptr(ngs::IOptions_session *) {

  }

  void SetUp()
  {
    mock_session.reset(new StrictMock<Mock_session>(&mock_client));
    mock_options_session.reset(new StrictMock<ngs::test::Mock_options_session>());
    sut = Auth_type::create(mock_session.get());

    ON_CALL(mock_data_context, authenticate(_, _, _, _, _, _, _, _)).WillByDefault(Return(default_error));
    EXPECT_CALL(mock_connection,
                options()).WillRepeatedly(Return(ngs::IOptions_session_ptr(mock_options_session.get(),
                                                                           &dont_delete_ptr)));
    EXPECT_CALL(mock_connection, connection_type()).WillRepeatedly(Return(ngs::Connection_tls));
    EXPECT_CALL(mock_client, connection()).WillRepeatedly(ReturnRef(mock_connection));
    EXPECT_CALL(*mock_session, data_context()).WillRepeatedly(ReturnRef(mock_data_context));
  }

  void assert_responce(const ngs::Authentication_handler::Response &result,
                       const std::string &data = "",
                       const ngs::Authentication_handler::Status status = ngs::Authentication_handler::Error,
                       const int error_code = ER_NET_PACKETS_OUT_OF_ORDER) const
  {
    ASSERT_EQ(data,       result.data);
    ASSERT_EQ(status,     result.status);
    ASSERT_EQ(error_code, result.error_code);
  }

  ngs::Error_code                        default_error;

  StrictMock<Mock_sql_data_context>      mock_data_context;
  StrictMock<xpl::test::Mock_client>     mock_client;
  StrictMock<ngs::test::Mock_connection> mock_connection;
  ngs::unique_ptr<Mock_session>        mock_session;
  ngs::shared_ptr<ngs::test::Mock_options_session> mock_options_session;
  ngs::Authentication_handler_ptr        sut;
};


typedef AuthenticationTestSuite<Sasl_plain_auth> SaslAuthenticationTestSuite;


TEST_F(SaslAuthenticationTestSuite, handleContinue_fails_always)
{
  ngs::Authentication_handler::Response result = sut->handle_continue("");

  assert_responce(result, "", ngs::Authentication_handler::Error, ER_NET_PACKETS_OUT_OF_ORDER);
}

template<typename Auth_type>
class ExpectedValuesAuthenticationTestSuite : public AuthenticationTestSuite<Auth_type>
{
public:
  ExpectedValuesAuthenticationTestSuite()
  : expected_database("test_database"),
    expected_login("test_login"),
    expected_password("test_password"),
    expected_password_hash("*4414E26EDED6D661B5386813EBBA95065DBC4728"),
    expected_host("test_host"),
    expected_hostname("test_host"),
    sasl_separator("\0",1),
    ec_failur(1, ""),
    ec_success(0, "")
  {
  }

  std::string get_sasl_message(const std::string &login, const std::string &password, const std::string &autorization = "")
  {
    return std::string() + autorization + sasl_separator + login + sasl_separator + password;
  }

  const char *expected_database;
  const char *expected_login;
  const char *expected_password;
  const char *expected_password_hash;
  const char *expected_host;
  const std::string expected_hostname;

  const std::string sasl_separator;

  const ngs::Error_code ec_failur;
  const ngs::Error_code ec_success;
};


typedef ExpectedValuesAuthenticationTestSuite<Sasl_plain_auth> ExpectedValuesSaslAuthenticationTestSuite;

TEST_F(ExpectedValuesSaslAuthenticationTestSuite, handleStart_autenticateAndReturnsError_whenIllformedStringNoSeparator)
{
  std::string sasl_login_string = expected_login;

  EXPECT_CALL(mock_client, client_address()).WillOnce(Return(expected_host));
  EXPECT_CALL(mock_client, client_hostname()).WillOnce(Return(expected_hostname.c_str()));
  ngs::Authentication_handler::Response result = sut->handle_start("", sasl_login_string, "");

  assert_responce(result, "Invalid user or password", ngs::Authentication_handler::Failed, ER_NO_SUCH_USER);
}


TEST_F(ExpectedValuesSaslAuthenticationTestSuite, handleStart_autenticateAndReturnsError_whenIllformedStringOneSeparator)
{
  std::string sasl_login_string = "some data" + sasl_separator + "some data";

  EXPECT_CALL(mock_client, client_address()).WillOnce(Return(expected_host));
  EXPECT_CALL(mock_client, client_hostname()).WillOnce(Return(expected_hostname.c_str()));
  ngs::Authentication_handler::Response result = sut->handle_start("", sasl_login_string, "");

  assert_responce(result, "Invalid user or password", ngs::Authentication_handler::Failed, ER_NO_SUCH_USER);
}


TEST_F(ExpectedValuesSaslAuthenticationTestSuite, handleStart_autenticateAndReturnsError_whenIllformedStringThusUserNameEmpty)
{
  const std::string  empty_user = "";
  std::string sasl_login_string = get_sasl_message(empty_user, expected_password, "autorize_as");

  EXPECT_CALL(mock_client, client_address()).WillOnce(Return(expected_host));
  EXPECT_CALL(mock_client, client_hostname()).WillOnce(Return(expected_hostname.c_str()));
  ngs::Authentication_handler::Response result = sut->handle_start("", sasl_login_string, "");

  assert_responce(result, "Invalid user or password", ngs::Authentication_handler::Failed, ER_NO_SUCH_USER);
}


TEST_F(ExpectedValuesSaslAuthenticationTestSuite, handleStart_autenticateAndReturnsSuccess_whenPasswordEmptyButValid)
{
  const std::string empty_password = "";
  std::string sasl_login_string = get_sasl_message(expected_login, empty_password);

  EXPECT_CALL(mock_client, client_address()).WillOnce(Return(expected_host));
  EXPECT_CALL(mock_client, supports_expired_passwords()).WillOnce(Return(false));
  EXPECT_CALL(mock_client, client_hostname()).WillOnce(Return(expected_hostname.c_str()));
  EXPECT_CALL(mock_data_context, authenticate(StrEq(expected_login), StrEq(expected_hostname.c_str()), StrEq(expected_host), StrEq(""), _, false, _, ngs::Connection_tls))
      .WillOnce(Return(ec_success));

  ngs::Authentication_handler::Response result = sut->handle_start("", sasl_login_string, "");

  assert_responce(result, "", ngs::Authentication_handler::Succeeded, 0);
}


TEST_F(ExpectedValuesSaslAuthenticationTestSuite, handleStart_autenticateAndReturnsSuccess_whenAuthSucceeded)
{
  std::string sasl_login_string = get_sasl_message(expected_login, expected_password, expected_database);

  EXPECT_CALL(mock_client, client_address()).WillOnce(Return(expected_host));
  EXPECT_CALL(mock_client, supports_expired_passwords()).WillOnce(Return(false));
  EXPECT_CALL(mock_client, client_hostname()).WillOnce(Return(expected_hostname.c_str()));
  EXPECT_CALL(mock_data_context, authenticate(StrEq(expected_login), StrEq(expected_hostname), StrEq(expected_host),StrEq(expected_database), _, false, _, ngs::Connection_tls))
    .WillOnce(Return(ec_success));

  ngs::Authentication_handler::Response result = sut->handle_start("", sasl_login_string, "");

  assert_responce(result, "", ngs::Authentication_handler::Succeeded, 0);
}


TEST_F(ExpectedValuesSaslAuthenticationTestSuite, handleStart_autenticateAndReturnsFailure_whenAuthFailures)
{
  std::string sasl_login_string = get_sasl_message(expected_login, expected_password, expected_database);

  EXPECT_CALL(mock_client, client_address()).WillOnce(Return(expected_host));
  EXPECT_CALL(mock_client, client_hostname()).WillOnce(Return(expected_hostname.c_str()));
  EXPECT_CALL(mock_client, supports_expired_passwords()).WillOnce(Return(false));
  EXPECT_CALL(mock_data_context, authenticate(StrEq(expected_login), StrEq(expected_host), StrEq(expected_host),StrEq(expected_database), _, false, _, ngs::Connection_tls))
    .WillOnce(Return(ec_failur));

  ngs::Authentication_handler::Response result = sut->handle_start("", sasl_login_string, "");

  assert_responce(result, "", ngs::Authentication_handler::Failed, 1);
}


class Partialmock_Sasl_auth : public Sasl_plain_auth
{
public:
  Partialmock_Sasl_auth(xpl::Session *session) : Sasl_plain_auth(session) {}

  static ngs::Authentication_handler_ptr create(ngs::Session_interface *session)
  {
    return Authentication_handler::wrap_ptr(new Partialmock_Sasl_auth((xpl::Session*)session));
  }

  bool DoDone()
  {
    Sasl_plain_auth::done();

    return true;
  }

  // Workaround for GMOCK undefined behaviour with ResultHolder
  MOCK_METHOD0(done_void, bool ());
  void done()
  {
    done_void();
  }
};


typedef AuthenticationTestSuite<Partialmock_Sasl_auth> PartialMockSaslAuthenticationTestSuite;


TEST_F(PartialMockSaslAuthenticationTestSuite, smartPtrDestructor_callsDoneMethod_always)
{
  Partialmock_Sasl_auth *mock_sut = dynamic_cast<Partialmock_Sasl_auth*>(sut.get());

  // Check call to object and ensure that its delete by calling base method
  EXPECT_CALL(*mock_sut, done_void()).WillOnce(InvokeWithoutArgs(mock_sut, &Partialmock_Sasl_auth::DoDone));
}


} // namespace test

} // namespace spl
