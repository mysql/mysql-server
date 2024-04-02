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
#include "mrs/http/error.h"
#include "mrs/rest/handler_file.h"
#include "mrs/rest/request_context.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_http_request.h"
#include "mock/mock_http_server_component.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_object.h"
#include "mock/mock_query_entry_content_file.h"
#include "mock/mock_query_factory.h"
#include "mock/mock_route_schema.h"
#include "mock/mock_session.h"

using helper::MakeSharedPtr;
using helper::SetHttpComponent;
using mrs::rest::HandlerFile;
using testing::_;
using testing::ByMove;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

using MediaType = helper::MediaType;

const std::string k_url{"https://mysql.com/mrs/schema/table"};
const std::string k_path{"^/mrs/schema/table/?"};

class RestHandlerFileTests : public Test {
 public:
  void SetUp() override { request_context_.request = &mock_request_; }

  void make_sut(const mrs::UniversalId id, const std::string &path,
                const std::string &version) {
    std::string options;
    EXPECT_CALL(mock_route_, get_redirection()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock_route_, get_default_content())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock_route_, get_cache())
        .WillRepeatedly(Return(&mock_cache_manager_));
    EXPECT_CALL(mock_route_, get_options()).WillRepeatedly(ReturnRef(options));
    EXPECT_CALL(mock_route_, get_id()).WillRepeatedly(Return(id));
    EXPECT_CALL(mock_route_, get_version()).WillRepeatedly(ReturnRef(version));
    EXPECT_CALL(mock_route_, get_object_path()).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(mock_route_, get_rest_path())
        .WillRepeatedly(Return(std::vector<std::string>({path})));
    EXPECT_CALL(mock_route_, get_rest_url()).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(mock_request_, get_input_headers())
        .WillRepeatedly(ReturnRef(mock_input_headers));
    EXPECT_CALL(*mock_query_factory_, create_query_content_file())
        .WillRepeatedly(Return(mock_query_file_.copy_base()));
    EXPECT_CALL(mock_http_component_, add_route(path, _))
        .WillOnce(Invoke(
            [this](
                const ::std::string &,
                std::unique_ptr<http::base::RequestHandler> handler) -> void * {
              request_handler_ = std::move(handler);
              return request_handler_.get();
            }));
    sut_ = std::make_shared<HandlerFile>(&mock_route_, &mock_auth_manager_,
                                         mock_query_factory_);
  }

  void delete_sut() {
    EXPECT_CALL(mock_http_component_, remove_route(request_handler_.get()));
    sut_.reset();
  }

  std::unique_ptr<http::base::RequestHandler> request_handler_;
  StrictMock<MockMysqlCacheManager> mock_cache_manager_;
  MakeSharedPtr<StrictMock<MockQueryFactory>> mock_query_factory_;
  MakeSharedPtr<StrictMock<MockQueryEntryContentFile>> mock_query_file_;
  StrictMock<MockHttpServerComponent> mock_http_component_;
  SetHttpComponent raii_setter_{&mock_http_component_};
  StrictMock<MockRoute> mock_route_;
  StrictMock<MockAuthManager> mock_auth_manager_;
  StrictMock<MockHttpRequest> mock_request_;
  StrictMock<MockHttpHeaders> mock_input_headers;
  StrictMock<MockMySQLSession> mock_session;
  mrs::rest::RequestContext request_context_;
  std::shared_ptr<HandlerFile> sut_;
};

TEST_F(RestHandlerFileTests, etag_matches_do_not_send_the_file) {
  const std::string k_path = "/schema/file1";
  const std::string k_tag = "tag1";
  const mrs::UniversalId k_file_id{110};

  make_sut(k_file_id, k_path, k_tag);
  EXPECT_CALL(mock_input_headers, find_cstr(StrEq("If-None-Match")))
      .WillOnce(Return(k_tag.c_str()));

  try {
    sut_->handle_get(&request_context_);
    FAIL() << "handle_get must throw http::Error.";
  } catch (const mrs::http::Error &e) {
    ASSERT_EQ(HttpStatusCode::NotModified, e.status);
  } catch (...) {
    throw;
  }

  delete_sut();
}

TEST_F(RestHandlerFileTests, handle_delete_not_supported) {
  const std::string k_path = "/schema/file1";
  const std::string k_tag = "tag1";
  const mrs::UniversalId k_file_id{110};

  make_sut(k_file_id, k_path, k_tag);
  ASSERT_THROW(sut_->handle_delete(&request_context_), mrs::http::Error);
  delete_sut();
}

TEST_F(RestHandlerFileTests, handle_put_not_supported) {
  const std::string k_path = "/schema/file1";
  const std::string k_tag = "tag1";
  const mrs::UniversalId k_file_id{110};

  make_sut(k_file_id, k_path, k_tag);
  ASSERT_THROW(sut_->handle_put(&request_context_), mrs::http::Error);
  delete_sut();
}

TEST_F(RestHandlerFileTests, handle_post_not_supported) {
  const std::string k_path = "/schema/file1";
  const std::string k_tag = "tag1";
  const mrs::UniversalId k_file_id{110};

  make_sut(k_file_id, k_path, k_tag);
  ASSERT_THROW(sut_->handle_post(&request_context_, {}), mrs::http::Error);
  delete_sut();
}

struct Request {
  mrs::UniversalId file_id;
  std::string path;
  const char *tag;
  helper::MediaType expected_media_type;
};

void PrintTo(const Request &v, ::std::ostream *os) {
  (*os) << v.file_id.to_string() << ",path:" << v.path;
}

class RestHandlerDifferentFilesTests
    : public RestHandlerFileTests,
      public testing::WithParamInterface<Request> {
 public:
};

TEST_P(RestHandlerDifferentFilesTests, fetch_file) {
  auto param = GetParam();
  const std::string k_tag = "tag1";

  make_sut(param.file_id, param.path, k_tag);
  EXPECT_CALL(mock_cache_manager_,
              get_instance(collector::kMySQLConnectionMetadataRO, false))
      .WillOnce(Return(ByMove(collector::MysqlCacheManager::CachedObject(
          nullptr, false, &mock_session))));
  EXPECT_CALL(mock_input_headers, find_cstr(StrEq("If-None-Match")))
      .WillOnce(Return(param.tag));
  EXPECT_CALL(*mock_query_file_, query_file(&mock_session, param.file_id));

  mock_query_file_->result = "some content";
  auto result = sut_->handle_get(&request_context_);

  EXPECT_EQ(param.expected_media_type, result.type);
  EXPECT_EQ(mock_query_file_->result, result.response);
  EXPECT_EQ(k_tag, result.etag);

  delete_sut();
}

const Request k_file_to_fetch_param[] = {
    {mrs::UniversalId{1}, "/schema/file.jpg", nullptr, MediaType::typeJpg},
    {mrs::UniversalId{1}, "/schema/file.js", nullptr, MediaType::typeJs},
    {mrs::UniversalId{2}, "/schema/file.mjs", nullptr, MediaType::typeJs},
    {mrs::UniversalId{2}, "/schema/file.html", nullptr, MediaType::typeHtml},
    {mrs::UniversalId{2}, "/schema/file.htm", nullptr, MediaType::typeHtml},
    {mrs::UniversalId{2}, "/schema/file.css", nullptr, MediaType::typeCss},
    {mrs::UniversalId{2}, "/schema/file.map", nullptr, MediaType::typePlain},
    {mrs::UniversalId{3}, "/schema/file.gif", nullptr, MediaType::typeGif}};

INSTANTIATE_TEST_SUITE_P(files_to_fetch, RestHandlerDifferentFilesTests,
                         ::testing::ValuesIn(k_file_to_fetch_param));
