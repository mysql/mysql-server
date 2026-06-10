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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "helper/set_http_component.h"
#include "http/base/uri_path_matcher.h"
#include "mrs/endpoint/db_object_endpoint.h"
#include "mrs/endpoint/db_schema_endpoint.h"
#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/endpoint/handler/handler_db_object_table.h"
#include "mrs/endpoint/url_host_endpoint.h"
#include "mysql/harness/make_shared_ptr.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_endpoint_configuration.h"
#include "mock/mock_http_server_component.h"
#include "mock/mock_mysqlcachemanager.h"

using helper::SetHttpComponent;
using mysql_harness::MakeSharedPtr;
using HandlerDbObjectTable = mrs::endpoint::handler::HandlerDbObjectTable;
using DbObjectEndpoint = mrs::endpoint::DbObjectEndpoint;
using DbSchemaEndpoint = mrs::endpoint::DbSchemaEndpoint;
using DbServiceEndpoint = mrs::endpoint::DbServiceEndpoint;
using UrlHostEndpoint = mrs::endpoint::UrlHostEndpoint;
using DbService = mrs::database::entry::DbService;
using DbSchema = mrs::database::entry::DbSchema;
using DbObject = mrs::database::entry::DbObject;
using DbHost = mrs::rest::entry::AppUrlHost;
using HandlerConfiguration = mrs::interface::RestHandler::Configuration;
using ::http::base::UriPathMatcher;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;
using testing::Test;

const std::string k_url{"https://mysql.com/mrs/schema/table"};
const UriPathMatcher k_path{"/mrs/schema/table", true, false};
const int k_access_rights = 5;

struct Endpoints {
  bool is_https{true};
  std::string host{"mysql.com"};
  std::string service{"/mrs"};
  std::string schema{"/schema"};
  std::string object{"/table"};
  std::string url{k_url};
  UriPathMatcher path{k_path};
  mrs::UniversalId host_id{{10, 0}};
  mrs::UniversalId service_id{{10, 100}};
  mrs::UniversalId schema_id{{10, 101}};
  mrs::UniversalId object_id{{10, 102}};
  int access_rights = k_access_rights;
  bool requires_auth{true};
};

const auto k_auth_check = mrs::interface::RestHandler::Authorization::kCheck;
const auto k_auth_none = mrs::interface::RestHandler::Authorization::kNotNeeded;

using Strings = std::vector<std::string>;

class RestHandlerObjectTests : public Test {
 public:
  void make_sut(const Endpoints &config) {
    EXPECT_CALL(*mock_endpoint_configuration_, does_server_support_https())
        .WillRepeatedly(Return(config.is_https));
    EXPECT_CALL(mock_http_component_, add_direct_match_route(_, config.path, _))
        .WillOnce(Invoke(
            [this](
                const ::std::string &, const UriPathMatcher &,
                std::unique_ptr<http::base::RequestHandler> handler) -> void * {
              request_handler_ = std::move(handler);
              return request_handler_.get();
            }));

    DbService db_srv;
    DbSchema db_sch;
    DbObject db_obj;
    DbHost db_host;

    db_host.id = config.host_id;

    db_srv.id = config.service_id;
    db_srv.url_host_id = config.host_id;

    db_sch.id = config.schema_id;
    db_sch.service_id = config.service_id;

    db_obj.id = config.object_id;
    db_obj.schema_id = config.schema_id;

    db_host.name = config.host;
    db_srv.url_context_root = config.service;
    db_sch.request_path = config.schema;
    db_sch.requires_auth = config.requires_auth;

    db_obj.request_path = config.object;
    db_obj.crud_operation = config.access_rights;
    db_obj.requires_authentication = config.requires_auth;

    endpoint_host_ = std::make_shared<UrlHostEndpoint>(
        db_host, mock_endpoint_configuration_, nullptr);
    endpoint_db_srv_ = std::make_shared<DbServiceEndpoint>(
        db_srv, mock_endpoint_configuration_, nullptr);
    endpoint_db_sch_ = std::make_shared<DbSchemaEndpoint>(
        db_sch, mock_endpoint_configuration_, nullptr);
    endpoint_db_obj_ = std::make_shared<DbObjectEndpoint>(
        db_obj, mock_endpoint_configuration_, nullptr);
    endpoint_db_sch_->change_parent(endpoint_db_srv_);
    endpoint_db_obj_->change_parent(endpoint_db_sch_);
    sut_ = std::make_shared<HandlerDbObjectTable>(endpoint_db_obj_,
                                                  &mock_auth_manager_);
    sut_->initialize(HandlerConfiguration());
  }

  void delete_sut() {
    EXPECT_CALL(mock_http_component_, remove_route(request_handler_.get()));
    sut_.reset();
  }

  std::unique_ptr<http::base::RequestHandler> request_handler_;
  StrictMock<MockMysqlCacheManager> mock_cache_manager_;
  StrictMock<MockHttpServerComponent> mock_http_component_;
  SetHttpComponent raii_setter_{&mock_http_component_};
  MakeSharedPtr<MockEndpointConfiguration> mock_endpoint_configuration_;
  StrictMock<MockAuthManager> mock_auth_manager_;
  std::shared_ptr<UrlHostEndpoint> endpoint_host_;
  std::shared_ptr<DbServiceEndpoint> endpoint_db_srv_;
  std::shared_ptr<DbSchemaEndpoint> endpoint_db_sch_;
  std::shared_ptr<DbObjectEndpoint> endpoint_db_obj_;
  std::shared_ptr<HandlerDbObjectTable> sut_;
};

TEST_F(RestHandlerObjectTests, forwards_data_from_endpoints_set1) {
  const Endpoints k_default{};

  make_sut(k_default);
  ASSERT_EQ(k_default.service_id, sut_->get_service_id());
  ASSERT_EQ(k_default.service, sut_->get_service_path());
  ASSERT_EQ(k_default.schema, sut_->get_schema_path());
  ASSERT_EQ(k_default.object, sut_->get_db_object_path());
  ASSERT_EQ(k_auth_check, sut_->requires_authentication());
  ASSERT_EQ(k_default.access_rights, sut_->get_access_rights());
  delete_sut();
}

TEST_F(RestHandlerObjectTests, forwards_data_from_endpoints_set2) {
  const Endpoints k_other_data{false,
                               "oracle.com",
                               "/svc",
                               "/sakila",
                               "/actor",
                               "http://oracle.com/svc/sakila/actor",
                               UriPathMatcher{"/svc/sakila/actor", true, false},
                               mrs::UniversalId{{100, 100}},
                               mrs::UniversalId{{200, 100}},
                               mrs::UniversalId{{222, 100}},
                               mrs::UniversalId{{233, 100}},
                               1,
                               false};
  make_sut(k_other_data);
  ASSERT_EQ(k_other_data.service_id, sut_->get_service_id());
  ASSERT_EQ(k_other_data.service, sut_->get_service_path());
  ASSERT_EQ(k_other_data.schema, sut_->get_schema_path());
  ASSERT_EQ(k_other_data.object, sut_->get_db_object_path());
  ASSERT_EQ(k_other_data.access_rights, sut_->get_access_rights());
  ASSERT_EQ(k_auth_none, sut_->requires_authentication());
  delete_sut();
}
