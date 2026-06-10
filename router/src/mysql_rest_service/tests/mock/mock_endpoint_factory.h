/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_ENDPOINT_FACTORY_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_ENDPOINT_FACTORY_H_

#include "mrs/endpoint/content_file_endpoint.h"
#include "mrs/endpoint/content_set_endpoint.h"
#include "mrs/endpoint/db_object_endpoint.h"
#include "mrs/endpoint/db_schema_endpoint.h"
#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/endpoint/endpoint_factory.h"
#include "mrs/endpoint/url_host_endpoint.h"
#include "mrs/endpoint_manager.h"
#include "mrs/interface/endpoint_base.h"

class MockProxy {
 public:
  MOCK_METHOD(void, activate_private, (), ());
  MOCK_METHOD(void, activate_public, (), ());
  MOCK_METHOD(void, deactivate, (), ());

  MOCK_METHOD(void, created, (), ());
  MOCK_METHOD(void, destroyed, (), ());
};

template <typename Base = mrs::interface::EndpointBase>
class ProxyEndpoint : public Base {
 public:
  using DataType = typename Base::DataType;
  using Uri = typename Base::Uri;

 public:
  ProxyEndpoint(const DataType &data, MockProxy *proxy)
      : Base(data, {}, {}), proxy_(proxy) {
    proxy_->created();
  }

  ~ProxyEndpoint() override { proxy_->destroyed(); }

  void activate_private() override { proxy_->activate_private(); }
  void activate_public() override { proxy_->activate_public(); }
  void deactivate() override { proxy_->deactivate(); }
  Uri get_url() const override { return {}; }

 private:
  MockProxy *proxy_;
};

template <typename Base = mrs::interface::EndpointBase>
class MockEndpoint : public Base {
 public:
  using EnabledType = typename Base::EnabledType;

 public:
  template <typename... An>
  MockEndpoint(An &&...args) : Base(std::forward<An>(args)...) {}

  using UniversalId = mrs::interface::EndpointBase::UniversalId;
  using Uri = mrs::interface::EndpointBase::Uri;

  MOCK_METHOD(UniversalId, get_id, (), (const, override));
  MOCK_METHOD(UniversalId, get_parent_id, (), (const, override));

  MOCK_METHOD(EnabledType, get_this_node_enabled_level, (), (const, override));
  MOCK_METHOD(std::string, get_my_url_path_part, (), (const, override));
  MOCK_METHOD(std::string, get_my_url_part, (), (const, override));

  MOCK_METHOD(std::optional<std::string>, get_options, (), (const, override));

  MOCK_METHOD(bool, does_this_node_require_authentication, (),
              (const, override));
  MOCK_METHOD(bool, required_authentication, (), (const, override));
  MOCK_METHOD(std::string, get_url_path, (), (const, override));
  MOCK_METHOD(Uri, get_url, (), (const, override));
  MOCK_METHOD(void, activate_public, (), (override));
  MOCK_METHOD(void, activate_private, (), (override));
  MOCK_METHOD(void, deactivate, (), (override));
  MOCK_METHOD(EnabledType, get_enabled_level, (), (const, override));
  MOCK_METHOD(void, update, (), (override));
};

class MockEndpointFactory : public mrs::endpoint::EndpointFactory {
 public:
  using UniversalId = mrs::interface::EndpointBase::UniversalId;
  using DbObjectEndpoint = mrs::endpoint::DbObjectEndpoint;
  using DbSchemaEndpoint = mrs::endpoint::DbSchemaEndpoint;
  using ContentFileEndpoint = mrs::endpoint::ContentFileEndpoint;
  using ContentSetEndpoint = mrs::endpoint::ContentSetEndpoint;
  using DbServiceEndpoint = mrs::endpoint::DbServiceEndpoint;
  using UrlHostEndpoint = mrs::endpoint::UrlHostEndpoint;
  using EndpointBasePtr = mrs::EndpointManager::EndpointBasePtr;
  using ContentFile = mrs::EndpointManager::ContentFile;
  using ContentSet = mrs::EndpointManager::ContentSet;
  using DbObject = mrs::EndpointManager::DbObject;
  using DbSchema = mrs::EndpointManager::DbSchema;
  using DbService = mrs::EndpointManager::DbService;
  using UrlHost = mrs::EndpointManager::UrlHost;

  MockEndpointFactory() : EndpointFactory({}, {}) {}

  struct MockIndexer {
    using Mocks = std::array<testing::StrictMock<MockProxy>, 10>;
    int last_unused_idx{0};
    Mocks mock_object;

    auto &get_next_not_used() { return mock_object[last_unused_idx++]; }
    auto &operator[](int idx) { return mock_object[idx]; }

    void verifyAndClearMocks() {
      for (auto &m : mock_object) {
        testing::Mock::VerifyAndClearExpectations(&m);
      }
    }
  };

  MockIndexer mock_db_object;
  MockIndexer mock_db_schema;
  MockIndexer mock_content_file;
  MockIndexer mock_content_set;
  MockIndexer mock_db_service;
  MockIndexer mock_url_host;

  void verifyAndClearMocks() {
    mock_db_object.verifyAndClearMocks();
    mock_db_schema.verifyAndClearMocks();
    mock_content_file.verifyAndClearMocks();
    mock_content_set.verifyAndClearMocks();
    mock_db_service.verifyAndClearMocks();
    mock_url_host.verifyAndClearMocks();
  }

  template <typename Endpoint>
  EndpointBasePtr make_shared_proxy_object(
      const typename Endpoint::DataType &data, EndpointBasePtr parent,
      MockProxy &mock) {
    auto result = std::make_shared<ProxyEndpoint<Endpoint>>(data, &mock);
    result->set_parent(parent);
    return result;
  }

  EndpointBasePtr create_object(const ContentSet &d,
                                EndpointBasePtr parent) override {
    return make_shared_proxy_object<ContentSetEndpoint>(
        d, parent, mock_content_set.get_next_not_used());
  }

  EndpointBasePtr create_object(const ContentFile &d,
                                EndpointBasePtr parent) override {
    return make_shared_proxy_object<ContentFileEndpoint>(
        d, parent, mock_content_file.get_next_not_used());
  }

  EndpointBasePtr create_object(const DbSchema &d,
                                EndpointBasePtr parent) override {
    return make_shared_proxy_object<DbSchemaEndpoint>(
        d, parent, mock_db_schema.get_next_not_used());
  }

  EndpointBasePtr create_object(const DbObject &d,
                                EndpointBasePtr parent) override {
    return make_shared_proxy_object<DbObjectEndpoint>(
        d, parent, mock_db_object.get_next_not_used());
  }

  EndpointBasePtr create_object(const DbService &d,
                                EndpointBasePtr parent) override {
    return make_shared_proxy_object<DbServiceEndpoint>(
        d, parent, mock_db_service.get_next_not_used());
  }

  EndpointBasePtr create_object(const UrlHost &d, EndpointBasePtr) override {
    return make_shared_proxy_object<UrlHostEndpoint>(
        d, {}, mock_url_host.get_next_not_used());
  }
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_ENDPOINT_FACTORY_H_
