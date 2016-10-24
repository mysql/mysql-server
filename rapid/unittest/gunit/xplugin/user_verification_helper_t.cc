
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


#include <string>

#include "ngs_common/smart_ptr.h"

#include "user_verification_helper.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mock/ngs_general.h"
#include "mock/session.h"

namespace xpl
{
  const char     *USER_NAME = "TEST";
  const char     *USER_IP = "100.20.20.10";
  const longlong  REQUIRE_SECURE_TRANSPORT = 0;
  const char     *EXPECTED_HASH         = "AABBCCDD";
  const char     *ACCOUNT_NOT_LOCKET    = "N";
  const longlong  PASSWORD_NOT_EXPIRED  = 0;
  const longlong  DISCONECT_ON_EXPIRED  = 0;
  const longlong  IS_NOT_OFFLINE        = 0;

  namespace test {

    using namespace ::testing;
    class Mock_hash_verification
    {
    public:
      MOCK_METHOD1(check_hash, bool (const std::string &));
    };

    class User_verification_test : public Test
    {
    public:
      User_verification_test()
      {
        m_hash_check = ngs::bind(&Mock_hash_verification::check_hash, &m_hash, ngs::placeholders::_1);

        m_mock_options.reset(new StrictMock<ngs::test::Mock_options_session>());
        m_options = ngs::static_pointer_cast<ngs::IOptions_session>(m_mock_options);
        m_sut.reset(new User_verification_helper(m_hash_check, m_options, ngs::Connection_tls));
      }

      void setup_field_types(const char *value)
      {
        Field_value *filed_value = ngs::allocate_object<Field_value>(value, strlen(value));
        Command_delegate::Field_type field_type = {MYSQL_TYPE_STRING};

        m_row_data.fields.push_back(filed_value);
        m_field_types.push_back(field_type);
      }

      void setup_field_types(const longlong value)
      {
        Field_value *filed_value = ngs::allocate_object<Field_value>(value);
        Command_delegate::Field_type field_type = {MYSQL_TYPE_LONGLONG};

        m_row_data.fields.push_back(filed_value);
        m_field_types.push_back(field_type);
      }

      void setup_db_user(const longlong secure_transport = REQUIRE_SECURE_TRANSPORT)
      {
        setup_field_types(secure_transport);
        setup_field_types(EXPECTED_HASH);
        setup_field_types(ACCOUNT_NOT_LOCKET);
        setup_field_types(PASSWORD_NOT_EXPIRED);
        setup_field_types(DISCONECT_ON_EXPIRED);
        setup_field_types(IS_NOT_OFFLINE);
      }

      void setup_no_ssl()
      {
        setup_field_types("");
        setup_field_types("");
        setup_field_types("");
        setup_field_types("");
      }

      void expect_execute_sql(ngs::Error_code error_code = ngs::Error_code())
      {
        Buffering_command_delegate::Resultset result_set;

        result_set.push_back(m_row_data);

        EXPECT_CALL(m_sql_data_context, execute_sql_and_collect_results(_, _, _, _, _))
          .WillOnce(DoAll(
              SetArgReferee<2>(m_field_types),
              SetArgReferee<3>(result_set),
              Return(error_code)));
      }

      StrictMock<Mock_hash_verification> m_hash;
      StrictMock<Mock_sql_data_context>  m_sql_data_context;
      ngs::function<bool (const std::string &)> m_hash_check;

      ngs::shared_ptr<ngs::IOptions_session> m_options;
      ngs::shared_ptr<StrictMock<ngs::test::Mock_options_session> > m_mock_options;

      Command_delegate::Field_types m_field_types;
      Row_data m_row_data;

      ngs::unique_ptr<User_verification_helper> m_sut;
    };

    TEST_F(User_verification_test, everything_matches_and_hash_is_right)
    {
      setup_db_user();
      setup_no_ssl();

      EXPECT_CALL(m_hash, check_hash(EXPECTED_HASH)).WillOnce(Return(true));
      expect_execute_sql();

      ngs::Error_code result = m_sut->verify_mysql_account(
          m_sql_data_context,
          USER_NAME,
          USER_IP);

      ASSERT_FALSE(result);
    }

    TEST_F(User_verification_test, forwards_error_from_query_execution)
    {
      const ngs::Error_code expected_error(ER_MUST_CHANGE_PASSWORD_LOGIN, "");
      setup_db_user();
      setup_no_ssl();

      expect_execute_sql(expected_error);

      ngs::Error_code result = m_sut->verify_mysql_account(
          m_sql_data_context,
          USER_NAME,
          USER_IP);

      ASSERT_TRUE(result);
      ASSERT_EQ(expected_error, result);
    }

    TEST_F(User_verification_test, dont_match_anything_when_hash_isnt_right)
    {
      setup_db_user();
      setup_no_ssl();

      EXPECT_CALL(m_hash, check_hash(EXPECTED_HASH)).WillOnce(Return(false));
      expect_execute_sql();

      ngs::Error_code result = m_sut->verify_mysql_account(
          m_sql_data_context,
          USER_NAME,
          USER_IP);

      ASSERT_TRUE(result);
      ASSERT_EQ(ER_NO_SUCH_USER, result.error);
    }

    class User_verification_param_test: public User_verification_test, public WithParamInterface<int>
    {
    };

    TEST_P(User_verification_param_test, if_data_isnt_there_reject)
    {
      setup_db_user();
      setup_no_ssl();

      ngs::free_object(m_row_data.fields[GetParam()]);
      m_row_data.fields[GetParam()] = NULL;
      expect_execute_sql();

      ngs::Error_code result = m_sut->verify_mysql_account(
          m_sql_data_context,
          USER_NAME,
          USER_IP);

      ASSERT_TRUE(result);
      ASSERT_EQ(ER_NO_SUCH_USER, result.error);
    }

    TEST_P(User_verification_param_test, if_had_wrong_type_reject)
    {
      setup_db_user();
      setup_no_ssl();

      m_field_types[GetParam()].type = MYSQL_TYPE_FLOAT;
      expect_execute_sql();

      ngs::Error_code result = m_sut->verify_mysql_account(
          m_sql_data_context,
          USER_NAME,
          USER_IP);

      ASSERT_TRUE(result);
      ASSERT_EQ(ER_NO_SUCH_USER, result.error);
    }

    INSTANTIATE_TEST_CASE_P(Range_from_0_to_9,
        User_verification_param_test,
        Range(0, 9, 1));

    struct Test_param_connection_type
    {
      Test_param_connection_type(const bool requires_secure, const ngs::Connection_type type)
      : m_requires_secure(requires_secure),
        m_type(type)
      {
      }

      bool m_requires_secure;
      ngs::Connection_type m_type;
    };

    class User_verification_param_test_with_supported_combinations: public User_verification_test, public WithParamInterface<Test_param_connection_type>
    {
    };

    TEST_P(User_verification_param_test_with_supported_combinations, expect_result_on_given_connection_type)
    {
      const Test_param_connection_type &param = GetParam();

      m_sut.reset(new User_verification_helper(m_hash_check, m_options, param.m_type));
      EXPECT_CALL(m_hash, check_hash(EXPECTED_HASH)).WillRepeatedly(Return(true));

      setup_db_user(param.m_requires_secure);
      setup_no_ssl();
      expect_execute_sql();

      ngs::Error_code result = m_sut->verify_mysql_account(
          m_sql_data_context,
          USER_NAME,
          USER_IP);

      ASSERT_FALSE(result);
    }

    INSTANTIATE_TEST_CASE_P(Supported_connection_type_require_transport_combinations,
        User_verification_param_test_with_supported_combinations,
        Values(
            Test_param_connection_type(false, ngs::Connection_tcpip),
            Test_param_connection_type(false, ngs::Connection_namedpipe),
            Test_param_connection_type(false, ngs::Connection_tls),
            Test_param_connection_type(false, ngs::Connection_unixsocket),
            Test_param_connection_type(true,  ngs::Connection_unixsocket),
            Test_param_connection_type(true,  ngs::Connection_tls)));

    class User_verification_param_test_with_unsupported_combinations : public User_verification_param_test_with_supported_combinations { };
    TEST_P(User_verification_param_test_with_unsupported_combinations, expect_result_on_given_connection_type)
    {
      const Test_param_connection_type &param = GetParam();

      m_sut.reset(new User_verification_helper(m_hash_check, m_options, param.m_type));
      EXPECT_CALL(m_hash, check_hash(EXPECTED_HASH)).WillRepeatedly(Return(true));

      setup_db_user(param.m_requires_secure);
      setup_no_ssl();
      expect_execute_sql();

      ngs::Error_code result = m_sut->verify_mysql_account(
          m_sql_data_context,
          USER_NAME,
          USER_IP);

      ASSERT_TRUE(result);
      ASSERT_EQ(ER_SECURE_TRANSPORT_REQUIRED, result.error);
    }

    INSTANTIATE_TEST_CASE_P(Unsupported_connection_type_require_transport_combinations,
        User_verification_param_test_with_unsupported_combinations,
        Values(
            Test_param_connection_type(true, ngs::Connection_tcpip),
            Test_param_connection_type(true, ngs::Connection_namedpipe)));

  }
}

