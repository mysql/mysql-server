/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
#include <memory>

#include "unittest/gunit/xplugin/xpl/mock/component_services.h"
#include "unittest/gunit/xplugin/xpl/mock/srv_session_services.h"

#include "plugin/x/src/sql_data_context.h"

namespace xpl {
namespace test {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

class Sql_data_context_test_suite : public ::testing::Test {
 public:
  StrictMock<mock::Mysql_plugin_registry> m_mock_plugin_registry;
  StrictMock<mock::Service_registry> m_mock_registry;
  StrictMock<mock::Service_admin_session> m_mock_admin_session;
  StrictMock<mock::Srv_session> m_mock_srv_session;
  StrictMock<mock::Srv_session_info> m_mock_srv_session_info;
  std::unique_ptr<xpl::Sql_data_context> m_sut{new xpl::Sql_data_context()};
};

TEST_F(Sql_data_context_test_suite, create_object_which_does_nothing) {}

TEST_F(Sql_data_context_test_suite, initialize_admin_session_and_fail) {
  const bool k_request_admin_session = true;
  const mysql_service_status_t k_failure = 1;
  const mysql_service_status_t k_ok = 0;

  InSequence forceSequenceOfFollowingExpects;
  EXPECT_CALL(m_mock_plugin_registry, mysql_plugin_registry_acquire())
      .WillOnce(Return(m_mock_registry.get()));
  EXPECT_CALL(m_mock_registry, acquire(StrEq("mysql_admin_session"), _))
      .WillOnce(Return(k_failure));
  EXPECT_CALL(m_mock_plugin_registry,
              mysql_plugin_registry_release(m_mock_registry.get()))
      .WillOnce(Return(k_ok));

  // Call the Object-under-test
  ASSERT_TRUE(m_sut->init(k_request_admin_session));
}

TEST_F(Sql_data_context_test_suite, initialize_admin_session_and_release) {
  const bool k_request_admin_session = true;
  const mysql_service_status_t k_ok = 0;
  const MYSQL_SESSION k_session = reinterpret_cast<MYSQL_SESSION>(10);

  const auto admin_service =
      reinterpret_cast<my_h_service>(m_mock_admin_session.get());

  EXPECT_CALL(m_mock_srv_session_info, get_session_id(k_session))
      .WillRepeatedly(Return(0));

  InSequence forceSequenceOfFollowingExpects;
  EXPECT_CALL(m_mock_plugin_registry, mysql_plugin_registry_acquire())
      .WillOnce(Return(m_mock_registry.get()));
  EXPECT_CALL(m_mock_registry, acquire(StrEq("mysql_admin_session"), _))
      .WillOnce(DoAll(SetArgPointee<1>(admin_service), Return(k_ok)));

  EXPECT_CALL(m_mock_admin_session, open(_, _)).WillOnce(Return(k_session));

  EXPECT_CALL(m_mock_registry, release(admin_service));
  EXPECT_CALL(m_mock_plugin_registry,
              mysql_plugin_registry_release(m_mock_registry.get()))
      .WillOnce(Return(k_ok));

  EXPECT_CALL(m_mock_srv_session, close_session(k_session)).WillOnce(Return(0));
  // Call the Object-under-test
  ASSERT_FALSE(m_sut->init(k_request_admin_session));
}

}  // namespace test
}  // namespace xpl
