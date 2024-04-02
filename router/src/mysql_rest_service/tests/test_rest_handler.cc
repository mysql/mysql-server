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

#include "helper/set_http_component.h"
#include "mrs/interface/object.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_http_request.h"
#include "mock/mock_http_server_component.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/partialmock_rest_handler.h"

using helper::SetHttpComponent;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

class RestHandlerTests : public Test {
 public:
  void make_sut(const std::string &rest_url, const std::string &rest_path) {
    EXPECT_CALL(mock_http_component_, add_route(rest_path, _))
        .WillOnce(Invoke(
            [this](
                const ::std::string &,
                std::unique_ptr<http::base::RequestHandler> handler) -> void * {
              request_handler_ = std::move(handler);
              return request_handler_.get();
            }));
    sut_ = std::make_shared<StrictMock<PartialMockRestHandler>>(
        rest_url, rest_path, &mock_auth_manager_);
    ASSERT_NE(nullptr, request_handler_.get());
  }

  void delete_sut() {
    EXPECT_CALL(mock_http_component_, remove_route(request_handler_.get()));
    sut_.reset();
  }

  const std::string k_url{"https://mysql.com/mrs/schema/table"};
  const std::string k_path{"^/mrs/schema/table/?"};
  const std::string k_empty{};

  StrictMock<MockMysqlCacheManager> mock_cache_manager_;
  std::unique_ptr<http::base::RequestHandler> request_handler_;
  StrictMock<MockHttpServerComponent> mock_http_component_;
  SetHttpComponent raii_setter_{&mock_http_component_};
  StrictMock<MockAuthManager> mock_auth_manager_;
  std::shared_ptr<PartialMockRestHandler> sut_;
};

TEST_F(RestHandlerTests, handle_request_calls_handle_get) {
  make_sut(k_url, k_path);
  StrictMock<MockHttpRequest> mock_request;
  StrictMock<MockHttpHeaders> mock_oheaders;
  StrictMock<MockHttpHeaders> mock_iheaders;
  StrictMock<MockHttpBuffer> mock_obuffer;
  StrictMock<MockHttpBuffer> mock_ibuffer;
  StrictMock<MockHttpUri> mock_uri;

  EXPECT_CALL(*sut_, get_access_rights())
      .WillRepeatedly(Return(mrs::interface::Object::kRead));

  EXPECT_CALL(mock_request, get_method())
      .WillRepeatedly(Return(HttpMethod::Get));
  EXPECT_CALL(mock_request, get_uri()).WillRepeatedly(ReturnRef(mock_uri));
  EXPECT_CALL(mock_request, get_output_buffer())
      .WillRepeatedly(ReturnRef(mock_obuffer));
  EXPECT_CALL(mock_request, get_input_buffer())
      .WillRepeatedly(ReturnRef(mock_ibuffer));
  EXPECT_CALL(mock_request, get_output_headers())
      .WillRepeatedly(ReturnRef(mock_oheaders));
  EXPECT_CALL(mock_request, get_input_headers())
      .WillRepeatedly(ReturnRef(mock_iheaders));

  EXPECT_CALL(mock_iheaders, find_cstr(StrEq("Cookie")))
      .WillRepeatedly(Return(""));
  EXPECT_CALL(mock_iheaders, find_cstr(StrEq("Accept")))
      .WillRepeatedly(Return(""));
  EXPECT_CALL(mock_iheaders, find_cstr(StrEq("Origin")))
      .WillRepeatedly(Return(""));

  EXPECT_CALL(mock_uri, join()).WillRepeatedly(Return(""));
  EXPECT_CALL(mock_uri, get_path()).WillRepeatedly(Return("/"));

  EXPECT_CALL(mock_ibuffer, length()).WillRepeatedly(Return(0));

  EXPECT_CALL(mock_obuffer, length()).WillRepeatedly(Return(0));

  EXPECT_CALL(mock_auth_manager_, get_cache())
      .WillRepeatedly(Return(&mock_cache_manager_));
  EXPECT_CALL(mock_cache_manager_,
              get_empty(collector::kMySQLConnectionMetadataRO, false));
  EXPECT_CALL(*sut_, get_service_id())
      .WillRepeatedly(Return(mrs::UniversalId{1}));
  EXPECT_CALL(*sut_, requires_authentication())
      .WillRepeatedly(
          Return(mrs::interface::RestHandler::Authorization::kNotNeeded));
  EXPECT_CALL(mock_oheaders,
              add(StrEq("Content-Type"), StrEq("application/json")));
  EXPECT_CALL(mock_obuffer, add(_, _));
  EXPECT_CALL(mock_request, send_reply(HttpStatusCode::Ok, _, _));

  EXPECT_CALL(*sut_, handle_get(_));

  request_handler_->handle_request(mock_request);
  delete_sut();
}
