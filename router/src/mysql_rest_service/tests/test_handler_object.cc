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
#define OPENSSL_NO_DEPRECATED_3_0
#define OPENSSL_NO_DEPRECATED_1_1_0
#define OSSL_DEPRECATEDIN_3_0 extern
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mrs/rest/handler_table.h"
#include "mrs/rest/request_context.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_http_request.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_object.h"
#include "mock/mock_session.h"
#include "test_mrs_object_utils.h"

using testing::_;
using testing::ByMove;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::StartsWith;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

using namespace mrs::rest;

class HandleObjectTests : public Test {
 public:
  void SetUp() override {
    EXPECT_CALL(mock_request_, get_uri()).WillRepeatedly(ReturnRef(uri_));
  }

  class GeneralExpectations {
   public:
    GeneralExpectations(
        HandleObjectTests &parent,
        const mrs::interface::Object::RowUserOwnership &user_row_ownership,
        const mrs::interface::Object::VectorOfRowGroupOwnership
            &group_row_ownership,
        const std::string &cached_primary, const std::string &schema,
        const std::string &object, const std::string &rest_path,
        const std::string &rest_url,
        const std::vector<std::string> &cached_columns,
        collector::MySQLConnection conn = collector::kMySQLConnectionUserdataRO)
        : parent_{parent},
          user_row_ownership_{user_row_ownership},
          group_row_ownership_{group_row_ownership},
          cached_primary_{cached_primary, "INT"},
          schema_{schema},
          object_{object},
          rest_path_{rest_path},
          rest_url_{rest_url} {
      cached_columns_.emplace_back(cached_primary, "text");
      for (auto &a : cached_columns) {
        cached_columns_.emplace_back(a, "text");
      }

      auto builder = DualityViewBuilder(schema, object);
      builder.field(cached_primary, cached_primary, "text");
      for (auto &a : cached_columns) {
        builder.field(a, a, "text");
      }
      cached_object_ = builder.root();

      expectSetup(conn);
    }

    void expectSetup(collector::MySQLConnection conn =
                         collector::kMySQLConnectionUserdataRO) {
      static std::string k_empty_string;
      EXPECT_CALL(parent_.mock_route, get_options())
          .WillRepeatedly(ReturnRef(k_empty_string));

      EXPECT_CALL(parent_.mock_input_headers, find_cstr(StrEq("Cookie")))
          .WillRepeatedly(Return(k_empty_string.c_str()));
      EXPECT_CALL(parent_.mock_input_headers, find_cstr(StrEq("Accept")))
          .WillRepeatedly(Return(nullptr));
      EXPECT_CALL(parent_.mock_request_, get_input_headers())
          .WillRepeatedly(ReturnRef(parent_.mock_input_headers));
      EXPECT_CALL(parent_.mock_request_, get_input_headers())
          .WillRepeatedly(ReturnRef(parent_.mock_input_headers));
      EXPECT_CALL(parent_.mock_route, get_rest_path())
          .WillRepeatedly(Return(std::vector<std::string>({rest_path_})));
      EXPECT_CALL(parent_.mock_route, get_rest_url())
          .WillRepeatedly(ReturnRef(rest_url_));
      EXPECT_CALL(parent_.mock_route, get_rest_path_raw())
          .WillRepeatedly(ReturnRef(rest_path_));
      EXPECT_CALL(parent_.mock_route, get_cache())
          .WillRepeatedly(Return(&parent_.mysql_cache));
      EXPECT_CALL(parent_.mysql_cache, get_instance(conn, false))
          .WillOnce(Return(ByMove(collector::MysqlCacheManager::CachedObject(
              nullptr, false, &parent_.mock_session))));

      using ConnParam = collector::CountedMySQLSession::ConnectionParameters;
      EXPECT_CALL(parent_.mock_session, get_connection_parameters())
          .WillRepeatedly(Return(ConnParam{}));

      //      EXPECT_CALL(parent_.mock_route, get_cached_columnes())
      //          .WillRepeatedly(ReturnRef(cached_columns_));

      EXPECT_CALL(parent_.mock_route, get_on_page()).WillRepeatedly(Return(25));
      EXPECT_CALL(parent_.mock_route, get_user_row_ownership())
          .WillRepeatedly(ReturnRef(user_row_ownership_));
      EXPECT_CALL(parent_.mock_route, get_group_row_ownership())
          .WillRepeatedly(ReturnRef(group_row_ownership_));
      EXPECT_CALL(parent_.mock_route, get_object_name())
          .WillRepeatedly(ReturnRef(object_));
      EXPECT_CALL(parent_.mock_route, get_schema_name())
          .WillRepeatedly(ReturnRef(schema_));
      EXPECT_CALL(parent_.mock_route, get_object())
          .WillRepeatedly(Return(cached_object_));
    }

    HandleObjectTests &parent_;
    mrs::interface::Object::RowUserOwnership user_row_ownership_;
    mrs::interface::Object::VectorOfRowGroupOwnership group_row_ownership_;
    helper::Column cached_primary_;
    std::string schema_;
    std::string object_;
    std::string rest_path_;
    std::string rest_url_;
    std::vector<helper::Column> cached_columns_;
    std::shared_ptr<mrs::database::entry::Object> cached_object_;
  };

  http::base::Uri uri_{""};
  StrictMock<MockHttpHeaders> mock_input_headers;
  StrictMock<MockMysqlCacheManager> mysql_cache;
  StrictMock<MockHttpRequest> mock_request_;
  StrictMock<MockRoute> mock_route;
  StrictMock<MockAuthManager> mock_auth_manager;
  StrictMock<MockMySQLSession> mock_session;
};

TEST_F(HandleObjectTests, fetch_object_feed) {
  const mrs::interface::Object::RowUserOwnership k_user_row_ownership {
    false, ""
  };
  const mrs::interface::Object::VectorOfRowGroupOwnership
      k_group_row_ownership {};
  const std::string k_cached_primary{"column1"};
  GeneralExpectations expectations{*this,
                                   k_user_row_ownership,
                                   k_group_row_ownership,
                                   k_cached_primary,
                                   "schema",
                                   "object",
                                   "/schema/object",
                                   "https://test.pl/schema/object",
                                   {"column2", "column3"}};

  RequestContext ctxt{&mock_request_};
  HandlerTable object{&mock_route, &mock_auth_manager};

  EXPECT_CALL(
      mock_session,
      query(StartsWith("SELECT "
                       "JSON_OBJECT('column1', `t`.`column1`, 'column2', `t`.`"
                       "column2`, 'column3', `t`.`column3`,'links'"),
            _, _));

  object.handle_get(&ctxt);
}

TEST_F(HandleObjectTests, fetch_object_single) {
  const mrs::interface::Object::RowUserOwnership k_user_row_ownership {
    false, ""
  };
  const mrs::interface::Object::VectorOfRowGroupOwnership
      k_group_row_ownership {};
  const std::string k_cached_primary{"column1"};
  GeneralExpectations expectations{*this,
                                   k_user_row_ownership,
                                   k_group_row_ownership,
                                   k_cached_primary,
                                   "schema",
                                   "object",
                                   "/schema/object/1",
                                   "https://test.pl/schema/object",
                                   {"column2", "column3"}};

  RequestContext ctxt{&mock_request_};
  HandlerTable object{&mock_route, &mock_auth_manager};

  EXPECT_CALL(
      mock_session,
      query(StartsWith("SELECT "
                       "JSON_OBJECT('column1', `t`.`column1`, 'column2', `t`.`"
                       "column2`, 'column3', `t`.`column3`,'links'"),
            _, _));

  object.handle_get(&ctxt);
}

TEST_F(HandleObjectTests, delete_single_object_throws_without_filter) {
  const mrs::interface::Object::RowUserOwnership k_user_row_ownership {
    false, ""
  };
  const mrs::interface::Object::VectorOfRowGroupOwnership
      k_group_row_ownership {};
  const std::string k_cached_primary{"column1"};
  GeneralExpectations expectations{*this,
                                   k_user_row_ownership,
                                   k_group_row_ownership,
                                   k_cached_primary,
                                   "schema",
                                   "object",
                                   "/schema/object/1",
                                   "https://test.pl/schema/object",
                                   {"column2", "column3"},
                                   collector::kMySQLConnectionUserdataRW};

  RequestContext ctxt{&mock_request_};
  HandlerTable object{&mock_route, &mock_auth_manager};

  //  EXPECT_CALL(mock_session,
  //              query(StartsWith("SELECT "
  //                               "JSON_OBJECT('column1',`column1`,'column2',`"
  //                               "column2`,'column3',`column3`, 'links'"),
  //                    _, _));

  ASSERT_THROW(object.handle_delete(&ctxt), std::exception);
}

TEST_F(HandleObjectTests, delete_single_object) {
  const mrs::interface::Object::RowUserOwnership k_user_row_ownership {
    false, ""
  };
  const mrs::interface::Object::VectorOfRowGroupOwnership
      k_group_row_ownership {};
  const std::string k_cached_primary{"column1"};
  GeneralExpectations expectations{*this,
                                   k_user_row_ownership,
                                   k_group_row_ownership,
                                   k_cached_primary,
                                   "schema",
                                   "object",
                                   "/schema/object/1",
                                   "https://test.pl/schema/object?q={}",
                                   {"column2", "column3"},
                                   collector::kMySQLConnectionUserdataRW};

  RequestContext ctxt{&mock_request_};
  HandlerTable object{&mock_route, &mock_auth_manager};

  //  EXPECT_CALL(mock_session,
  //              query(StartsWith("SELECT "
  //                               "JSON_OBJECT('column1',`column1`,'column2',`"
  //                               "column2`,'column3',`column3`, 'links'"),
  //                    _, _));

  ASSERT_THROW(object.handle_delete(&ctxt), std::exception);
}
