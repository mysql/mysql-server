/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "collector/mysql_cache_manager.h"

#include "mock/mock_mysql_cache_callbacks.h"
#include "mock/mock_session.h"

using namespace collector;
using mysqlrouter::MySQLSession;
using testing::_;
using testing::Mock;
using testing::Pointee;
using testing::Return;
using testing::StrictMock;
using testing::Test;

class MysqlCacheManagerTest : public Test {
 public:
  StrictMock<MockMySQLSession> mock_session_;
  StrictMock<MockMySqlCacheCallbacks> mock_callbacks_;
  MysqlCacheManager sut_{&mock_callbacks_, &mock_callbacks_};
};

TEST_F(MysqlCacheManagerTest, sut_constructor_does_nothing) {}

TEST_F(MysqlCacheManagerTest, multiple_objects_deallocate_themself) {
  MockMySQLSession session[4];
  EXPECT_CALL(mock_callbacks_, object_allocate(false))
      .Times(4)
      .WillOnce(Return(&session[0]))
      .WillOnce(Return(&session[1]))
      .WillOnce(Return(&session[2]))
      .WillOnce(Return(&session[3]));
  {
    auto obj1 = sut_.get_instance(collector::kMySQLConnectionMetadataRO, false);
    auto obj2 = sut_.get_instance(collector::kMySQLConnectionMetadataRO, false);
    auto obj3 = sut_.get_instance(collector::kMySQLConnectionMetadataRO, false);
    auto obj4 = sut_.get_instance(collector::kMySQLConnectionMetadataRO, false);

    EXPECT_CALL(mock_callbacks_, object_before_cache(_, _)).Times(4);
    EXPECT_CALL(mock_callbacks_, object_remove(_)).Times(4);
  }
  Mock::VerifyAndClearExpectations(&mock_callbacks_);
}

TEST_F(MysqlCacheManagerTest, object_deallocates_itself) {
  EXPECT_CALL(mock_callbacks_, object_allocate(false))
      .WillOnce(Return(&mock_session_));
  {
    auto obj1 = sut_.get_instance(collector::kMySQLConnectionMetadataRO, false);
    Mock::VerifyAndClearExpectations(&mock_callbacks_);
    EXPECT_CALL(mock_callbacks_, object_before_cache(&mock_session_, _))
        .WillOnce(Return(false));
    EXPECT_CALL(mock_callbacks_, object_remove(&mock_session_)).Times(1);
  }
  Mock::VerifyAndClearExpectations(&mock_callbacks_);
}

TEST_F(MysqlCacheManagerTest,
       not_empty_object_deallocates_at_sut_destructor_when_its_cached) {
  EXPECT_CALL(mock_callbacks_, object_allocate(false))
      .WillOnce(Return(&mock_session_));
  {
    auto obj1 = sut_.get_instance(collector::kMySQLConnectionMetadataRO, false);
    Mock::VerifyAndClearExpectations(&mock_callbacks_);
    EXPECT_CALL(mock_callbacks_, object_before_cache(&mock_session_, _))
        .WillOnce(Return(true));
  }
  Mock::VerifyAndClearExpectations(&mock_callbacks_);
  EXPECT_CALL(mock_callbacks_, object_remove(&mock_session_)).Times(1);
}

TEST_F(MysqlCacheManagerTest, cache_may_only_keep_three_objects) {
  constexpr uint32_t k_number_of_allocated_objects_at_once = 10;
  sut_.change_cache_object_limit(3);

  EXPECT_CALL(mock_callbacks_, object_allocate(false))
      .Times(k_number_of_allocated_objects_at_once)
      .WillRepeatedly(Return(&mock_session_));
  {
    MysqlCacheManager::CachedObject obj[k_number_of_allocated_objects_at_once];
    for (uint32_t i = 0; i < k_number_of_allocated_objects_at_once; ++i)
      obj[i] = sut_.get_instance(collector::kMySQLConnectionMetadataRO, false);
    Mock::VerifyAndClearExpectations(&mock_callbacks_);
    EXPECT_CALL(mock_callbacks_, object_before_cache(&mock_session_, _))
        .Times(3)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_callbacks_, object_remove(&mock_session_))
        .Times(k_number_of_allocated_objects_at_once - 3);
  }
  Mock::VerifyAndClearExpectations(&mock_callbacks_);
  EXPECT_CALL(mock_callbacks_, object_remove(&mock_session_)).Times(3);
}

TEST_F(MysqlCacheManagerTest, cache_may_only_keep_one_object_and_reuseit) {
  constexpr uint32_t k_number_of_allocated_objects_at_once = 10;
  sut_.change_cache_object_limit(1);

  EXPECT_CALL(mock_callbacks_, object_allocate(false))
      .Times(k_number_of_allocated_objects_at_once)
      .WillRepeatedly(Return(&mock_session_));
  {
    MysqlCacheManager::CachedObject obj[k_number_of_allocated_objects_at_once];
    for (uint32_t i = 0; i < k_number_of_allocated_objects_at_once; ++i)
      obj[i] = sut_.get_instance(collector::kMySQLConnectionMetadataRO, false);
    Mock::VerifyAndClearExpectations(&mock_callbacks_);
    EXPECT_CALL(mock_callbacks_, object_before_cache(&mock_session_, _))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_callbacks_, object_remove(&mock_session_))
        .Times(k_number_of_allocated_objects_at_once - 1);
  }
  Mock::VerifyAndClearExpectations(&mock_callbacks_);
  EXPECT_CALL(mock_callbacks_, object_remove(&mock_session_)).Times(1);
}
