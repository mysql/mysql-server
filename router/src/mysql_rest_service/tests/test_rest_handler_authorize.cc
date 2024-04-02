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
#include "mrs/rest/handler_authorize.h"
#include "mrs/rest/request_context.h"

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
using testing::DoAll;
using testing::HasSubstr;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

using ServiceId = mrs::authentication::AuthorizeManager::ServiceId;
using AuthUser = mrs::database::entry::AuthUser;

const mrs::UniversalId k_service_id{101};

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
            [this](
                const ::std::string &,
                std::unique_ptr<http::base::RequestHandler> handler) -> void * {
              request_handler_ = std::move(handler);
              return request_handler_.get();
            }));
    sut_ = std::make_unique<HandlerAuthorize>(service_id, rest_url, rest_path,
                                              "", "", &mock_auth_);
    ASSERT_NE(nullptr, request_handler_.get());
  }

  void expectGeneric(HttpMethod::key_type type, const char *cookie = nullptr) {
    EXPECT_CALL(mock_auth_, get_cache())
        .WillRepeatedly(Return(&mock_cache_manager_));
    EXPECT_CALL(mock_cache_manager_,
                get_empty(collector::kMySQLConnectionMetadataRO, false));
    EXPECT_CALL(mock_request_, get_output_headers())
        .WillRepeatedly(ReturnRef(mock_output_headers_));
    EXPECT_CALL(mock_request_, get_input_headers())
        .WillRepeatedly(ReturnRef(mock_input_headers_));
    EXPECT_CALL(mock_request_, get_output_buffer())
        .WillRepeatedly(ReturnRef(mock_output_buffer_));
    EXPECT_CALL(mock_request_, get_input_buffer())
        .WillRepeatedly(ReturnRef(mock_input_buffer_));
    EXPECT_CALL(mock_request_, get_method()).WillRepeatedly(Return(type));
    EXPECT_CALL(mock_input_headers_, find_cstr(StrEq("Cookie")))
        .WillRepeatedly(Return(cookie));
    EXPECT_CALL(mock_input_headers_, find_cstr(StrEq("Origin")))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock_input_headers_, find_cstr(StrEq("Accept")))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock_request_, get_uri()).WillRepeatedly(ReturnRef(mock_uri_));
    EXPECT_CALL(mock_uri_, get_query()).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_uri_, get_path()).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_uri_, join()).WillRepeatedly(Return(""));
  }

  const std::string k_url{"some_url"};
  const std::string k_rest_path{"some_rest_path"};
  const std::string k_empty{};

  StrictMock<MockHttpUri> mock_uri_;
  StrictMock<MockHttpHeaders> mock_output_headers_;
  StrictMock<MockHttpHeaders> mock_input_headers_;
  StrictMock<MockHttpBuffer> mock_output_buffer_;
  StrictMock<MockHttpBuffer> mock_input_buffer_;
  StrictMock<MockHttpRequest> mock_request_;
  StrictMock<MockMysqlCacheManager> mock_cache_manager_;
  std::unique_ptr<http::base::RequestHandler> request_handler_;
  StrictMock<MockHttpServerComponent> mock_http_component_;
  SetHttpComponent raii_setter_{&mock_http_component_};
  MakeSharedPtr<StrictMock<MockAuthHandler>> mock_auth_handler_;
  StrictMock<MockAuthManager> mock_auth_;
  std::unique_ptr<HandlerAuthorize> sut_;
};

TEST_F(HandlerAuthorizeTests, unauthorized_access_when_method_delete) {
  expectGeneric(HttpMethod::Delete);

  EXPECT_CALL(mock_input_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_output_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_auth_, get_current_session(_, _, _))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"), StrEq("?login=fail")));
  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect));

  request_handler_->handle_request(mock_request_);
}

TEST_F(HandlerAuthorizeTests, unauthorized_access_when_method_put) {
  expectGeneric(HttpMethod::Put, "localhost");

  EXPECT_CALL(mock_input_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_output_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_auth_, get_current_session(_, _, _))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"), StrEq("?login=fail")));
  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect));

  request_handler_->handle_request(mock_request_);
}

TEST_F(HandlerAuthorizeTests, do_the_authentication_get) {
  expectGeneric(HttpMethod::Get);

  EXPECT_CALL(mock_auth_, get_current_session(_, _, _))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_auth_, authorize(_, _, _))
      .WillOnce(Invoke([this](ServiceId, mrs::rest::RequestContext &ctxt,
                              AuthUser *) -> bool {
        ctxt.selected_handler = mock_auth_handler_;
        return true;
      }));
  EXPECT_CALL(*mock_auth_handler_, redirects()).WillOnce(Return(true));

  EXPECT_CALL(mock_input_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_output_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"), StrEq("?login=success")));

  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect, _));

  request_handler_->handle_request(mock_request_);
}

TEST_F(HandlerAuthorizeTests, do_the_authentication_post) {
  expectGeneric(HttpMethod::Post);

  EXPECT_CALL(mock_auth_, get_current_session(_, _, _))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_auth_, authorize(_, _, _))
      .WillOnce(Invoke([this](ServiceId, mrs::rest::RequestContext &ctxt,
                              AuthUser *) -> bool {
        ctxt.selected_handler = mock_auth_handler_;
        ctxt.post_authentication = true;
        return true;
      }));
  EXPECT_CALL(*mock_auth_handler_, redirects()).WillOnce(Return(true));

  EXPECT_CALL(mock_input_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_input_buffer_, pop_front(0))
      .WillOnce(Return(std::vector<uint8_t>()));
  EXPECT_CALL(mock_output_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"), StrEq("?login=success")));

  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect, _));

  request_handler_->handle_request(mock_request_);
}

TEST_F(HandlerAuthorizeTests, do_the_authentication_fails) {
  expectGeneric(HttpMethod::Get);

  EXPECT_CALL(mock_auth_, get_current_session(_, _, _))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_auth_, authorize(_, _, _))
      .WillOnce(Invoke([this](ServiceId, mrs::rest::RequestContext &ctxt,
                              AuthUser *) -> bool {
        ctxt.selected_handler = mock_auth_handler_;
        return false;
      }));

  EXPECT_CALL(mock_input_buffer_, length()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_output_headers_,
              add(StrEq("Location"), StrEq("?login=fail")));

  EXPECT_CALL(mock_request_, send_reply(HttpStatusCode::TemporaryRedirect));

  request_handler_->handle_request(mock_request_);
}
