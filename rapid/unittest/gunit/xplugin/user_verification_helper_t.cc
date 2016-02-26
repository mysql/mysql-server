
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


#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <iostream>

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>

#include "user_verification_helper.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mock/connection.h"

namespace xpl
{

  const char     *USER_IP = "100.20.20.10";
  const char     *EXPECTED_HASH         = "AABBCCDD";
  const char     *ACCOUNT_NOT_LOCKET    = "N";
  const longlong  PASSWORD_NOT_EXPIRED  = 0;
  const longlong  DISCONECT_ON_EXPIRED  = 0;
  const longlong  IS_NOT_OFFLINE        = 0;

  namespace tests {

    class Mock_hash_verification
    {
    public:
      MOCK_METHOD1(check_hash, bool (const std::string &));
    };

    class User_verification_test : public ::testing::Test
    {
    public:
      User_verification_test()
      {
        boost::function<bool (const std::string &)> hash_check = boost::bind(&Mock_hash_verification::check_hash, &m_hash, _1);

        m_mock_options.reset(new testing::StrictMock<ngs::test::Mock_options_session>());
        m_options = boost::static_pointer_cast<ngs::IOptions_session>(m_mock_options);
        m_sut.reset(new User_verification_helper(hash_check, m_field_types, USER_IP, m_options));
      }

      void setup_field_types(const char *value)
      {
        Field_value *filed_value = new Field_value(value, strlen(value));
        Command_delegate::Field_type field_type = {MYSQL_TYPE_STRING};

        m_row_data.fields.push_back(filed_value);
        m_field_types.push_back(field_type);
      }

      void setup_field_types(const longlong value)
      {
        Field_value *filed_value = new Field_value(value);
        Command_delegate::Field_type field_type = {MYSQL_TYPE_LONGLONG};

        m_row_data.fields.push_back(filed_value);
        m_field_types.push_back(field_type);
      }

      void setup_db_user(const std::string &host)
      {
        setup_field_types(EXPECTED_HASH);
        setup_field_types(ACCOUNT_NOT_LOCKET);
        setup_field_types(PASSWORD_NOT_EXPIRED);
        setup_field_types(DISCONECT_ON_EXPIRED);
        setup_field_types(IS_NOT_OFFLINE);
        setup_field_types(host.c_str());
      }

      void setup_no_ssl()
      {
        setup_field_types("");
        setup_field_types("");
        setup_field_types("");
        setup_field_types("");
      }

      ::testing::StrictMock<Mock_hash_verification> m_hash;

      boost::shared_ptr<ngs::IOptions_session> m_options;
      boost::shared_ptr<testing::StrictMock<ngs::test::Mock_options_session> > m_mock_options;

      Command_delegate::Field_types m_field_types;
      Row_data m_row_data;

      boost::scoped_ptr<User_verification_helper> m_sut;
    };

    class User_verification_dbuser_param_valid_test : public User_verification_test, public testing::WithParamInterface<std::string>
    {
    };

    TEST_P(User_verification_dbuser_param_valid_test, match_ip_mask_when_significant_part_of_address_is_matches_and_hash_matches)
    {
      setup_db_user(GetParam());
      setup_no_ssl();

      EXPECT_CALL(m_hash, check_hash(EXPECTED_HASH)).WillOnce(testing::Return(true));
      ASSERT_TRUE((*m_sut)(m_row_data));
    }

    INSTANTIATE_TEST_CASE_P(Valid_ip_mask_addresses,
        User_verification_dbuser_param_valid_test,
        ::testing::Values("1.1.1.1/0.0.0.0",
                          "100.20.20.10/255.255.255.255",
                          "100.20.20.1/255.255.255.0",
                          "100.20.40.20/255.255.0.0"));


    class User_verification_dbuser_param_notvalid_test: public User_verification_dbuser_param_valid_test
    {
    };

    TEST_P(User_verification_dbuser_param_notvalid_test, dont_match_ip_mask_when_significant_part_of_address_is_different)
    {
      setup_db_user(GetParam());
      setup_no_ssl();

      ASSERT_FALSE((*m_sut)(m_row_data));
    }

    INSTANTIATE_TEST_CASE_P(Invalid_ip_mask_addresses,
        User_verification_dbuser_param_notvalid_test,
        ::testing::Values("NOT VALID / STRING",
                          "100.20.20.1/24",
                          "1.1.1.1/255.255.255.0",
                          "100.20.20.1/255.255.255.255",
                          "100.20.40.1/255.255.255.0",
                          "100.20.40.20/255.255.255.0"));

    // SQL query already matched most important parts of the IP address
    TEST_F(User_verification_test, match_any_ip_without_mask_when_hash_is_right)
    {
      setup_db_user("ANY IP OR HOST");
      setup_no_ssl();

      EXPECT_CALL(m_hash, check_hash(EXPECTED_HASH)).WillOnce(testing::Return(true));
      ASSERT_TRUE((*m_sut)(m_row_data));
    }

    TEST_F(User_verification_test, match_ip_without_mask_when_hash_is_right)
    {
      setup_db_user(USER_IP);
      setup_no_ssl();

      EXPECT_CALL(m_hash, check_hash(EXPECTED_HASH)).WillOnce(testing::Return(true));
      ASSERT_TRUE((*m_sut)(m_row_data));
    }

    TEST_F(User_verification_test, dont_match_ip_without_mask_when_hash_isnt_right)
    {
      setup_db_user(USER_IP);
      setup_no_ssl();

      EXPECT_CALL(m_hash, check_hash(EXPECTED_HASH)).WillOnce(testing::Return(false));
      ASSERT_FALSE((*m_sut)(m_row_data));
    }

    class User_verification_param_test: public User_verification_test, public testing::WithParamInterface<int>
    {
    };

    TEST_P(User_verification_param_test, if_data_isnt_there_reject)
    {
      setup_db_user(USER_IP);
      setup_no_ssl();

      delete m_row_data.fields[GetParam()];
      m_row_data.fields[GetParam()] = NULL;

      ASSERT_FALSE((*m_sut)(m_row_data));
    }

    TEST_P(User_verification_param_test, if_had_wrong_type_reject)
    {
      setup_db_user(USER_IP);
      setup_no_ssl();

      m_field_types[GetParam()].type = MYSQL_TYPE_FLOAT;

      ASSERT_FALSE((*m_sut)(m_row_data));
    }

    INSTANTIATE_TEST_CASE_P(Range_from_0_to_9,
        User_verification_param_test,
        ::testing::Range(0, 9, 1));

  }
}

