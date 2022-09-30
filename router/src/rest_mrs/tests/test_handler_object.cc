/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mrs/rest/handler_object.h"
#include "mrs/rest/handler_request_context.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_http_request.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_route.h"
#include "mock/mock_session.h"

using mrs::interface::Route;
using testing::_;
using testing::ByMove;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::StartsWith;
using testing::StrictMock;
using testing::Test;

using namespace mrs::rest;

class HandleObjectTests : public Test {
 public:
  void SetUp() override {
    EXPECT_CALL(mock_request_, get_uri()).WillRepeatedly(ReturnRef(uri_));
  }

  class GeneralExceptations {
   public:
    GeneralExceptations(
        HandleObjectTests &parent,
        const Route::RowUserOwnership &user_row_ownership,
        const Route::VectorOfRowGroupOwnership &group_row_ownership,
        const std::string &cached_primary, const std::string &schema,
        const std::string &object, const std::string &rest_path,
        const std::string &rest_url,
        const std::vector<std::string> &cached_columns)
        : parent_{parent},
          user_row_ownership_{user_row_ownership},
          group_row_ownership_{group_row_ownership},
          cached_primary_{cached_primary},
          schema_{schema},
          object_{object},
          rest_path_{rest_path},
          rest_url_{rest_url} {
      cached_columns_.emplace_back(cached_primary, MYSQL_TYPE_STRING);
      for (auto &a : cached_columns) {
        cached_columns_.emplace_back(a, MYSQL_TYPE_STRING);
      }

      expectSetup();
    }

    void expectSetup() {
      EXPECT_CALL(parent_.mock_route, get_rest_path())
          .WillRepeatedly(ReturnRef(rest_path_));
      EXPECT_CALL(parent_.mock_route, get_rest_url())
          .WillRepeatedly(ReturnRef(rest_url_));
      EXPECT_CALL(parent_.mock_route, get_rest_path_raw())
          .WillOnce(ReturnRef(rest_path_));
      EXPECT_CALL(parent_.mock_route, get_cache())
          .WillOnce(Return(&parent_.mysql_cache));
      EXPECT_CALL(parent_.mysql_cache,
                  get_instance(collector::kMySQLConnectionMetadata))
          .WillOnce(Return(ByMove(collector::MysqlCacheManager::CachedObject(
              nullptr, &parent_.mock_session))));

      EXPECT_CALL(parent_.mock_route, get_cached_columnes())
          .WillRepeatedly(ReturnRef(cached_columns_));
      EXPECT_CALL(parent_.mock_route, get_cached_primary())
          .WillRepeatedly(ReturnRef(cached_primary_));

      EXPECT_CALL(parent_.mock_route, get_on_page()).WillRepeatedly(Return(25));
      EXPECT_CALL(parent_.mock_route, get_user_row_ownership())
          .WillRepeatedly(ReturnRef(user_row_ownership_));
      EXPECT_CALL(parent_.mock_route, get_group_row_ownership())
          .WillRepeatedly(ReturnRef(group_row_ownership_));
      EXPECT_CALL(parent_.mock_route, get_object_name())
          .WillRepeatedly(ReturnRef(object_));
      EXPECT_CALL(parent_.mock_route, get_schema_name())
          .WillRepeatedly(ReturnRef(schema_));
    }

    HandleObjectTests &parent_;
    Route::RowUserOwnership user_row_ownership_;
    Route::VectorOfRowGroupOwnership group_row_ownership_;
    std::string cached_primary_;
    std::string schema_;
    std::string object_;
    std::string rest_path_;
    std::string rest_url_;
    std::vector<helper::Column> cached_columns_;
  };

  HttpUri uri_{};
  StrictMock<MockMysqlCacheManager> mysql_cache;
  StrictMock<MockHttpRequest> mock_request_;
  StrictMock<MockRoute> mock_route;
  StrictMock<MockAuthManager> mock_auth_manager;
  StrictMock<MockMySQLSession> mock_session;
};

TEST_F(HandleObjectTests, fetch_object_feed) {
  const Route::RowUserOwnership k_user_row_ownership{false, ""};
  const Route::VectorOfRowGroupOwnership k_group_row_ownership{};
  const std::string k_cached_primary{"column1"};
  GeneralExceptations expectations{*this,
                                   k_user_row_ownership,
                                   k_group_row_ownership,
                                   k_cached_primary,
                                   "schema",
                                   "object",
                                   "/schema/object",
                                   "https://test.pl/schema/object",
                                   {"column2", "column3"}};

  RequestContext ctxt{&mock_request_};
  HandlerObject object{&mock_route, &mock_auth_manager};

  EXPECT_CALL(mock_session,
              query(StartsWith("SELECT "
                               "JSON_OBJECT('column1',`column1`,'column2',`"
                               "column2`,'column3',`column3`, 'links'"),
                    _, _));

  object.handle_get(&ctxt);
}

TEST_F(HandleObjectTests, fetch_object_single) {
  const Route::RowUserOwnership k_user_row_ownership{false, ""};
  const Route::VectorOfRowGroupOwnership k_group_row_ownership{};
  const std::string k_cached_primary{"column1"};
  GeneralExceptations expectations{*this,
                                   k_user_row_ownership,
                                   k_group_row_ownership,
                                   k_cached_primary,
                                   "schema",
                                   "object",
                                   "/schema/object/1",
                                   "https://test.pl/schema/object",
                                   {"column2", "column3"}};

  RequestContext ctxt{&mock_request_};
  HandlerObject object{&mock_route, &mock_auth_manager};

  EXPECT_CALL(mock_session,
              query(StartsWith("SELECT "
                               "JSON_OBJECT('column1',`column1`,'column2',`"
                               "column2`,'column3',`column3`, 'links'"),
                    _, _));

  object.handle_get(&ctxt);
}
