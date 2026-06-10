/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

#include "helper/mysql_column.h"
#include "mrs/endpoint/handler/handler_db_object_table.h"
#include "mrs/rest/request_context.h"
#include "mysql/harness/make_shared_ptr.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_endpoint_configuration.h"
#include "mock/mock_endpoint_factory.h"
#include "mock/mock_handler_factory.h"
#include "mock/mock_http_request.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_session.h"
#include "test_mrs_object_utils.h"

using testing::_;
using testing::AtLeast;
using testing::ByMove;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::StartsWith;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

using namespace mrs::rest;
using namespace mrs::endpoint::handler;
using namespace mrs::endpoint;

template <typename T>
using MakeMockPtr = mysql_harness::MakeSharedPtr<StrictMock<T>>;

using RowUserOwnership = mrs::database::entry::RowUserOwnership;
using RowGroupOwnership = mrs::database::entry::RowGroupOwnership;
using VectorOfRowGroupOwnership = std::vector<RowGroupOwnership>;
using HandlerConfiguration = mrs::interface::RestHandler::Configuration;

class HandleObjectTests : public Test {
 public:
  void SetUp() override {
    EXPECT_CALL(mock_request_, get_uri()).WillRepeatedly(ReturnRef(uri_));
    EXPECT_CALL(*mock_configuratation, does_server_support_https())
        .WillRepeatedly(Return(true));

    mock_db_object_endpoint = std::make_shared<Mock<DbObjectEndpoint>>(
        db_object, mock_configuratation.copy_base(),
        mock_handler_factory.copy_base());
    mock_db_schema_endpoint = std::make_shared<Mock<DbSchemaEndpoint>>(
        db_schema, mock_configuratation.copy_base(),
        mock_handler_factory.copy_base());
    mock_db_service_endpoint = std::make_shared<Mock<DbServiceEndpoint>>(
        db_service, mock_configuratation.copy_base(),
        mock_handler_factory.copy_base());
  }

  class GeneralExpectations {
   public:
    GeneralExpectations(
        HandleObjectTests &parent, const RowUserOwnership &user_row_ownership,
        const VectorOfRowGroupOwnership &group_row_ownership,
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

      auto builder = JsonMappingBuilder(schema, object);
      builder.field(cached_primary, cached_primary, "text");
      for (auto &a : cached_columns) {
        builder.field(a, a, "text");
      }
      parent_.db_schema.id = {2, 1};
      parent_.db_object.id = {1, 1};
      parent_.db_object.object_description = builder.root();

      expectSetup(conn);
    }

    void expectSetup(collector::MySQLConnection conn =
                         collector::kMySQLConnectionUserdataRO) {
      static std::string k_empty_string;
      EXPECT_CALL(parent_.mock_input_headers, find_cstr(StrEq("Cookie")))
          .WillRepeatedly(Return(k_empty_string.c_str()));
      EXPECT_CALL(parent_.mock_input_headers, find_cstr(StrEq("Accept")))
          .WillRepeatedly(Return(nullptr));
      EXPECT_CALL(parent_.mock_request_, get_input_headers())
          .WillRepeatedly(ReturnRef(parent_.mock_input_headers));
      EXPECT_CALL(parent_.mock_request_, get_input_headers())
          .WillRepeatedly(ReturnRef(parent_.mock_input_headers));
      EXPECT_CALL(parent_.mysql_cache, get_instance(conn, false))
          .WillOnce(Return(ByMove(collector::MysqlCacheManager::CachedObject(
              nullptr, false, &parent_.mock_session))));

      EXPECT_CALL(*parent_.mock_db_object_endpoint, get_enabled_level())
          .WillRepeatedly(
              Return(mrs::database::entry::EnabledType::EnabledType_public));
      EXPECT_CALL(*parent_.mock_db_object_endpoint, get_url_path())
          .WillRepeatedly(Return(rest_path_));
      EXPECT_CALL(*parent_.mock_db_object_endpoint, get_url())
          .WillRepeatedly(Return(rest_url_));
      EXPECT_CALL(*parent_.mock_db_object_endpoint, get_id())
          .WillRepeatedly(Return(parent_.db_object.id));
      EXPECT_CALL(*parent_.mock_db_object_endpoint, update()).Times(AtLeast(1));

      EXPECT_CALL(*parent_.mock_db_schema_endpoint, get_url_path())
          .WillRepeatedly(Return("/db_service/db_schema"));
      EXPECT_CALL(*parent_.mock_db_schema_endpoint, get_enabled_level())
          .WillRepeatedly(
              Return(mrs::database::entry::EnabledType::EnabledType_public));
      EXPECT_CALL(*parent_.mock_db_schema_endpoint, get_id())
          .WillRepeatedly(Return(parent_.db_schema.id));
      EXPECT_CALL(*parent_.mock_db_schema_endpoint, update()).Times(AtLeast(1));

      parent_.mock_db_object_endpoint->set(parent_.db_object,
                                           parent_.mock_db_schema_endpoint);

      parent_.mock_db_schema_endpoint->set(parent_.db_schema,
                                           parent_.mock_db_service_endpoint);

      using ConnParam = collector::CountedMySQLSession::ConnectionParameters;
      EXPECT_CALL(parent_.mock_session, get_connection_parameters())
          .WillRepeatedly(Return(ConnParam{}));
    }

    HandleObjectTests &parent_;
    RowUserOwnership user_row_ownership_;
    VectorOfRowGroupOwnership group_row_ownership_;
    helper::Column cached_primary_;
    std::string schema_;
    std::string object_;
    std::string rest_path_;
    std::string rest_url_;
    std::vector<helper::Column> cached_columns_;
    std::shared_ptr<mrs::database::entry::Object> cached_object_;
  };
  template <typename T>
  using Mock = StrictMock<MockEndpoint<T>>;
  template <typename T>
  using SharedPtrMock = std::shared_ptr<StrictMock<MockEndpoint<T>>>;

  http::base::Uri uri_{""};
  mrs::database::entry::DbService db_service;
  mrs::database::entry::DbSchema db_schema;
  mrs::database::entry::DbObject db_object;
  StrictMock<MockHttpHeaders> mock_input_headers;
  StrictMock<MockMysqlCacheManager> mysql_cache;
  StrictMock<MockHttpRequest> mock_request_;
  StrictMock<MockAuthManager> mock_auth_manager;
  StrictMock<MockMySQLSession> mock_session;
  MakeMockPtr<MockHandlerFactory> mock_handler_factory;
  MakeMockPtr<MockEndpointConfiguration> mock_configuratation;
  SharedPtrMock<DbObjectEndpoint> mock_db_object_endpoint;
  SharedPtrMock<DbSchemaEndpoint> mock_db_schema_endpoint;
  SharedPtrMock<DbServiceEndpoint> mock_db_service_endpoint;
};

TEST_F(HandleObjectTests, fetch_object_feed) {
  const RowUserOwnership k_user_row_ownership{false, ""};
  const VectorOfRowGroupOwnership k_group_row_ownership{};
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
  HandlerDbObjectTable object{
      std::dynamic_pointer_cast<DbObjectEndpoint>(mock_db_object_endpoint),
      &mock_auth_manager,
      {},
      &mysql_cache};
  object.initialize(HandlerConfiguration());

  EXPECT_CALL(
      mock_session,
      query(
          StartsWith("SELECT /*+ MAX_EXECUTION_TIME(2000) */ "
                     "JSON_OBJECT('column1', `t0`.`column1`, 'column2', `t0`.`"
                     "column2`, 'column3', `t0`.`column3`,'links'"),
          _, _));

  object.handle_get(&ctxt);
}

TEST_F(HandleObjectTests, fetch_object_single) {
  const RowUserOwnership k_user_row_ownership{false, ""};
  const VectorOfRowGroupOwnership k_group_row_ownership{};
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
  HandlerDbObjectTable object{
      std::dynamic_pointer_cast<DbObjectEndpoint>(mock_db_object_endpoint),
      &mock_auth_manager,
      {},
      &mysql_cache};
  object.initialize(HandlerConfiguration());

  EXPECT_CALL(
      mock_session,
      query(
          StartsWith("SELECT /*+ MAX_EXECUTION_TIME(2000) */ "
                     "JSON_OBJECT('column1', `t0`.`column1`, 'column2', `t0`.`"
                     "column2`, 'column3', `t0`.`column3`,'links'"),
          _, _));

  object.handle_get(&ctxt);
}

TEST_F(HandleObjectTests, delete_single_object_throws_without_filter) {
  const RowUserOwnership k_user_row_ownership{false, ""};
  const VectorOfRowGroupOwnership k_group_row_ownership{};
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
  HandlerDbObjectTable object{
      std::dynamic_pointer_cast<DbObjectEndpoint>(mock_db_object_endpoint),
      &mock_auth_manager,
      {},
      &mysql_cache};
  object.initialize(HandlerConfiguration());

  //  EXPECT_CALL(mock_session,
  //              query(StartsWith("SELECT "
  // "JSON_OBJECT('column1',`column1`,'column2',`"
  //                               "column2`,'column3',`column3`, 'links'"),
  //                    _, _));

  ASSERT_THROW(object.handle_delete(&ctxt), std::exception);
}

TEST_F(HandleObjectTests, delete_single_object) {
  const RowUserOwnership k_user_row_ownership{false, ""};
  const VectorOfRowGroupOwnership k_group_row_ownership{};
  const std::string k_cached_primary{"column1"};
  GeneralExpectations expectations{*this,
                                   k_user_row_ownership,
                                   k_group_row_ownership,
                                   k_cached_primary,
                                   "schema",
                                   "object",
                                   "/schema/object/1",
                                   // %7B == {
                                   // %7D == }
                                   "https://test.pl/schema/object?q=%7B%7D",
                                   {"column2", "column3"},
                                   collector::kMySQLConnectionUserdataRW};

  RequestContext ctxt{&mock_request_};
  HandlerDbObjectTable object{
      std::dynamic_pointer_cast<DbObjectEndpoint>(mock_db_object_endpoint),
      &mock_auth_manager,
      {},
      &mysql_cache};
  object.initialize(HandlerConfiguration());

  //  EXPECT_CALL(mock_session,
  //              query(StartsWith("SELECT "
  // "JSON_OBJECT('column1',`column1`,'column2',`"
  //                               "column2`,'column3',`column3`, 'links'"),
  //                    _, _));

  ASSERT_THROW(object.handle_delete(&ctxt), std::exception);
}
