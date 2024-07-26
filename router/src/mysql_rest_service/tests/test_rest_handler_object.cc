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
#include <memory>

#include "helper/make_shared_ptr.h"
#include "helper/set_http_component.h"
#include "mrs/rest/handler_table.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_http_server_component.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_object.h"
#include "mock/mock_route_schema.h"

using helper::MakeSharedPtr;
using helper::SetHttpComponent;
using mrs::rest::HandlerTable;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;
using testing::Test;

const std::string k_url{"https://mysql.com/mrs/schema/table"};
const std::string k_path{"^/mrs/schema/table/?"};
const std::string k_empty;

using Strings = std::vector<std::string>;

class RestHandlerObjectTests : public Test {
 public:
  void make_sut(const std::string &rest_url, const std::string &rest_path) {
    EXPECT_CALL(mock_route_, get_schema())
        .WillRepeatedly(Return(mock_route_schema_.copy_base()));
    EXPECT_CALL(mock_route_, get_options()).WillOnce(ReturnRef(k_empty));
    EXPECT_CALL(mock_route_, get_rest_url()).WillOnce(ReturnRef(rest_url));
    EXPECT_CALL(mock_route_, get_rest_path())
        .WillOnce(Return(Strings{rest_path}));
    EXPECT_CALL(mock_http_component_, add_route(rest_path, _))
        .WillOnce(Invoke(
            [this](
                const ::std::string &,
                std::unique_ptr<http::base::RequestHandler> handler) -> void * {
              request_handler_ = std::move(handler);
              return request_handler_.get();
            }));
    sut_ = std::make_shared<HandlerTable>(&mock_route_, &mock_auth_manager_);
  }

  void delete_sut() {
    EXPECT_CALL(mock_http_component_, remove_route(request_handler_.get()));
    sut_.reset();
  }

  std::unique_ptr<http::base::RequestHandler> request_handler_;
  StrictMock<MockMysqlCacheManager> mock_cache_manager_;
  StrictMock<MockHttpServerComponent> mock_http_component_;
  SetHttpComponent raii_setter_{&mock_http_component_};
  StrictMock<MockRoute> mock_route_;
  MakeSharedPtr<StrictMock<MockRouteSchema>> mock_route_schema_;
  StrictMock<MockAuthManager> mock_auth_manager_;
  std::shared_ptr<HandlerTable> sut_;
};

TEST_F(RestHandlerObjectTests, forwards_get_service_id) {
  const mrs::UniversalId k_service_id{{10, 101}};

  make_sut(k_url, k_path);
  EXPECT_CALL(mock_route_, get_service_id()).WillOnce(Return(k_service_id));
  ASSERT_EQ(k_service_id, sut_->get_service_id());
  delete_sut();
}

TEST_F(RestHandlerObjectTests, forwards_get_schema_id) {
  const auto k_schema_id = mrs::UniversalId{{10, 101}};

  make_sut(k_url, k_path);
  EXPECT_CALL(*mock_route_schema_, get_id()).WillOnce(Return(k_schema_id));
  ASSERT_EQ(k_schema_id, sut_->get_schema_id());
  delete_sut();
}

TEST_F(RestHandlerObjectTests, forwards_get_object_id) {
  const auto k_object_id = mrs::UniversalId{{10, 101}};

  make_sut(k_url, k_path);
  EXPECT_CALL(mock_route_, get_id()).WillOnce(Return(k_object_id));
  ASSERT_EQ(k_object_id, sut_->get_db_object_id());
  delete_sut();
}

TEST_F(RestHandlerObjectTests, forwards_requires_authentication_must_be_check) {
  const auto k_req_auth = mrs::interface::RestHandler::Authorization::kCheck;
  make_sut(k_url, k_path);
  EXPECT_CALL(mock_route_, requires_authentication()).WillOnce(Return(true));
  ASSERT_EQ(k_req_auth, sut_->requires_authentication());
  delete_sut();
}

TEST_F(RestHandlerObjectTests, forwards_access_right) {
  const auto k_access_rights = 5;
  make_sut(k_url, k_path);
  EXPECT_CALL(mock_route_, get_access()).WillOnce(Return(k_access_rights));
  ASSERT_EQ(k_access_rights, sut_->get_access_rights());
  delete_sut();
}
