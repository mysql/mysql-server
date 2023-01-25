/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include "mrs/rest/handler_authorize.h"

#include "mock/mock_auth_handler.h"
#include "mock/mock_auth_manager.h"
#include "mock/mock_http_request.h"
#include "mock/mock_http_server_component.h"
#include "mock/mock_mysqlcachemanager.h"

using helper::MakeSharedPtr;
using helper::SetHttpComponent;
using mrs::interface::AuthorizeManager;
using mrs::rest::HandlerAuthorize;
using testing::_;
using testing::AllOf;
using testing::HasSubstr;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

const mrs::UniversalId k_service_id{101};
const std::string k_url{"some_url"};
const std::string k_rest_path{"some_rest_path"};

class HandlerAuthorizeTests : public Test {
 public:
  void SetUp() override { make_sut(k_service_id, k_url, k_rest_path); }

  void TearDown() override {
    EXPECT_CALL(mock_http_component_, remove_route(request_handler_.get()));
  }

  void make_sut(const mrs::UniversalId service_id, const std::string &rest_url,
                const std::string &rest_path) {
    EXPECT_CALL(mock_http_component_, add_route(rest_path, _))
        .WillOnce(Invoke(
            [this](const ::std::string &,
                   std::unique_ptr<BaseRequestHandler> handler) -> void * {
              request_handler_ = std::move(handler);
              return request_handler_.get();
            }));
    sut_ = std::make_unique<HandlerAuthorize>(service_id, rest_url, rest_path,
                                              "", "", &mock_auth_);
    ASSERT_NE(nullptr, request_handler_.get());
  }

  void expectGeneric(HttpMethod::type type, const char *cookie = nullptr) {
    EXPECT_CALL(mock_auth_, get_cache())
        .WillRepeatedly(Return(&mock_cache_manager_));
    EXPECT_CALL(mock_cache_manager_,
                get_empty(collector::kMySQLConnectionMetadata));
    EXPECT_CALL(mock_request_, get_output_headers())
        .WillRepeatedly(ReturnRef(mock_output_headers_));
    EXPECT_CALL(mock_request_, get_input_headers())
        .WillRepeatedly(ReturnRef(mock_input_headers_));
    EXPECT_CALL(mock_request_, get_output_buffer())
        .WillRepeatedly(ReturnRef(mock_output_buffer_));
    EXPECT_CALL(mock_request_, get_input_buffer())
        .WillRepeatedly(ReturnRef(mock_input_buffer_));
    EXPECT_CALL(mock_request_, get_method()).WillOnce(Return(type));
    EXPECT_CALL(mock_input_headers_, get(StrEq("Cookie")))
        .WillRepeatedly(Return(cookie));
    EXPECT_CALL(mock_request_, get_uri()).WillRepeatedly(ReturnRef(mock_uri_));
    EXPECT_CALL(mock_uri_, get_query()).WillRepeatedly(Return(""));
  }

  StrictMock<MockHttpUri> mock_uri_;
  StrictMock<MockHttpHeaders> mock_output_headers_;
  StrictMock<MockHttpHeaders> mock_input_headers_;
  StrictMock<MockHttpBuffer> mock_output_buffer_;
  StrictMock<MockHttpBuffer> mock_input_buffer_;
  StrictMock<MockHttpRequest> mock_request_;
  StrictMock<MockMysqlCacheManager> mock_cache_manager_;
  std::unique_ptr<BaseRequestHandler> request_handler_;
  StrictMock<MockHttpServerComponent> mock_http_component_;
  SetHttpComponent raii_setter_{&mock_http_component_};
  MakeSharedPtr<StrictMock<MockAuthHandler>> mock_auth_handler_;
  StrictMock<MockAuthManager> mock_auth_;
  std::unique_ptr<HandlerAuthorize> sut_;
};

TEST_F(HandlerAuthorizeTests, unauthorized_access_when_method_post) {
  expectGeneric(HttpMethod::Post, "localhost");

  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"),
                  StrEq("localhost?status=error&message=Forbidden")));
  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect));

  request_handler_->handle_request(mock_request_);
}

TEST_F(HandlerAuthorizeTests, unauthorized_access_when_method_delete) {
  expectGeneric(HttpMethod::Delete, "localhost");

  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"),
                  StrEq("localhost?status=error&message=Forbidden")));
  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect));

  request_handler_->handle_request(mock_request_);
}

TEST_F(HandlerAuthorizeTests, unauthorized_access_when_method_put) {
  expectGeneric(HttpMethod::Put, "localhost");

  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"),
                  StrEq("localhost?status=error&message=Forbidden")));
  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect));

  request_handler_->handle_request(mock_request_);
}

TEST_F(HandlerAuthorizeTests, do_the_authentication) {
  expectGeneric(HttpMethod::Get);

  // TODO(lkotula): Fixme/remove (Shouldn't be in review)
  //  EXPECT_CALL(mock_auth_, get_handlers_by_id(mrs::UniversalId{
  //                              mrs::IdType::k_id_type_auth_id,
  //                              k_service_id}))
  //      .WillOnce(Return(AuthorizeManager::AuthHandlers{mock_auth_handler_}));

  EXPECT_CALL(mock_uri_, get_query())
      .WillOnce(Return("mrs_redirect=localhost"));
  EXPECT_CALL(*mock_auth_handler_, authorize(_, _, _, _, _))
      .WillOnce(Return(true));

  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Set-Cookie"), StrEq("localhost; Max-Age=900")));
  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"), StrEq("localhost?status=authorized")));

  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect, _));

  request_handler_->handle_request(mock_request_);
}

TEST_F(HandlerAuthorizeTests, do_the_authentication_fails) {
  expectGeneric(HttpMethod::Get);

  // TODO(lkotula): Fixme/remove (Shouldn't be in review)
  //  EXPECT_CALL(mock_auth_, get_handlers_by_id(mrs::UniversalId{
  //                              mrs::IdType::k_id_type_auth_id,
  //                              k_service_id}))
  //      .WillOnce(Return(AuthorizeManager::AuthHandlers{mock_auth_handler_}));

  EXPECT_CALL(mock_uri_, get_query())
      .WillOnce(Return("mrs_redirect=localhost"));
  EXPECT_CALL(*mock_auth_handler_, authorize(_, _, _, _, _))
      .WillOnce(Return(false));

  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Set-Cookie"), StrEq("localhost; Max-Age=900")));
  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"), StrEq("localhost?status=unauthorized")));

  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect));

  request_handler_->handle_request(mock_request_);
}
