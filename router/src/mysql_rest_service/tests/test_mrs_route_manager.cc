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

#include "mrs/endpoint_configuration.h"
#include "mrs/endpoint_manager.h"
#include "mysql/harness/make_shared_ptr.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_endpoint_factory.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mrs/configuration.h"

using mrs::database::entry::DbObject;
using mysql_harness::MakeSharedPtr;
using EnabledType = mrs::database::entry::EnabledType;
using testing::_;
using testing::ByMove;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;
using testing::Test;

/*
 * Decorator for parent-id
 */
uint32_t operator""_pid(unsigned long long v) { return v; }
/*
 * Decorator for service-id
 */
uint32_t operator""_sid(unsigned long long v) { return v; }

/*
 * Decorator for schema-id
 */
uint32_t operator""_did(unsigned long long v) { return v; }

/*
 * Decorator for object-db-id
 */
uint32_t operator""_oid(unsigned long long v) { return v; }

MATCHER_P(EqSmartPtr, raw_ptr, "") { return raw_ptr == arg.get(); }
MATCHER_P(ById, id, "") { return id == arg.id; }

class BaseEndpointManagerTests : public Test {
 public:
  void SetUp() override {
    configuration_.is_https_ = true;

    sut_.reset(new mrs::EndpointManager(
        std::make_shared<mrs::EndpointConfiguration>(configuration_),
        &mock_mysqlcache_, &mock_auth_manager_, nullptr,
        mock_endpoint_factory_.copy_base()));
  }

  struct EntryId {
    mrs::UniversalId schema_id_;
    mrs::UniversalId obj_id_;
  };

  void verifyAndClearMocks(const std::vector<void *> &mocks) {
    Mock::VerifyAndClearExpectations(&mock_auth_manager_);
    Mock::VerifyAndClearExpectations(&mock_mysqlcache_);

    for (auto p : mocks) Mock::VerifyAndClearExpectations(p);
  }

  using UniversalId = mrs::database::entry::UniversalId;
  using DbService = mrs::database::entry::DbService;
  using DbSchema = mrs::database::entry::DbSchema;
  using DbObject = mrs::database::entry::DbObject;
  using UrlHost = mrs::rest::entry::AppUrlHost;

  struct Entry {
    Entry(uint32_t id, uint32_t parent_id = {})
        : id_{{static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8),
               static_cast<uint8_t>(id >> 16), static_cast<uint8_t>(id >> 24)}},
          parent_id_{{static_cast<uint8_t>(parent_id),
                      static_cast<uint8_t>(parent_id >> 8),
                      static_cast<uint8_t>(parent_id >> 16),
                      static_cast<uint8_t>(parent_id >> 24)}} {}

    UniversalId id_;
    UniversalId parent_id_;
  };

  std::vector<UrlHost> create_host(std::initializer_list<Entry> il) {
    using namespace std::string_literals;
    std::vector<UrlHost> result;

    for (auto i : il) {
      UrlHost s;
      s.id = i.id_;
      s.name = "localhost"s;
      result.push_back(s);
    }

    return result;
  }

  std::vector<DbService> create_services(
      std::initializer_list<Entry> il,
      EnabledType enabled = EnabledType::EnabledType_public) {
    std::vector<DbService> result;

    for (auto i : il) {
      DbService s;
      s.id = i.id_;
      s.url_host_id = i.parent_id_;
      s.enabled = enabled;
      result.push_back(s);
    }

    return result;
  }

  std::vector<DbSchema> create_schemas(
      std::initializer_list<Entry> il,
      EnabledType enabled = EnabledType::EnabledType_public) {
    std::vector<DbSchema> result;

    for (auto i : il) {
      DbSchema s;
      s.id = i.id_;
      s.service_id = i.parent_id_;
      s.enabled = enabled;
      result.push_back(s);
    }

    return result;
  }

  std::vector<DbObject> create_objects(
      std::initializer_list<Entry> il,
      EnabledType enabled = EnabledType::EnabledType_public) {
    std::vector<DbObject> result;

    for (auto i : il) {
      DbObject s;
      s.id = i.id_;
      s.schema_id = i.parent_id_;
      s.enabled = enabled;
      result.push_back(s);
    }

    return result;
  }

  const uint32_t k_host_id{0x1000001};
  const std::vector<UrlHost> k_hosts{create_host({k_host_id})};
  MakeSharedPtr<StrictMock<MockEndpointFactory>> mock_endpoint_factory_;
  StrictMock<MockAuthManager> mock_auth_manager_;
  StrictMock<MockMysqlCacheManager> mock_mysqlcache_;
  std::unique_ptr<mrs::EndpointManager> sut_;
  ::mrs::Configuration configuration_;
};

TEST_F(BaseEndpointManagerTests, sut_does_nothing) {}

TEST_F(BaseEndpointManagerTests, sut_manages_service) {
  std::vector<DbService> services{
      create_services({{0x01, k_host_id}, {0x02, k_host_id}})};

  {
    SCOPED_TRACE("Setup");
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Cleanup");
    for (auto &s : services) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());

    sut_->update(services);
  }

  EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());
}

TEST_F(BaseEndpointManagerTests, sut_manages_service_update_service2) {
  std::vector<DbService> services{
      create_services({{0x01, k_host_id}, {0x02, k_host_id}})};

  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Update just one
  {
    SCOPED_TRACE("Update just one service");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());

    sut_->update(create_services({{0x02, k_host_id}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Cleanup");
    for (auto &s : services) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());

    sut_->update(services);
  }
  EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());
}

TEST_F(BaseEndpointManagerTests, sut_manages_service_activates_endpoints) {
  std::vector<DbService> services{
      create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}})};

  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");

    // service-id:   0      1
    //               |      |
    // schema-id:    0      1
    //              / \     |
    // obj-id:     0   1    2

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);
    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}}));
    sut_->update(create_objects({{0x1101_oid, 0x101_did},
                                 {0x2101_oid, 0x101_did},
                                 {0x3102_oid, 0x102_did}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Change service 2, update one db-object
  {
    SCOPED_TRACE("Update service 2, observe update on db_object");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(create_services({{0x02_sid, k_host_id}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Change service 1, update two db-objects
  {
    SCOPED_TRACE("Update service 1, observe update on two db_objects");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());

    sut_->update(create_services({{0x01_sid, k_host_id}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Change schema 1, update two db-objects
  {
    SCOPED_TRACE("Update schema 1");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());

    sut_->update(create_schemas({{0x101_did, 0x1_sid}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Change schema 1, update two db-objects
  {
    SCOPED_TRACE("Update object 3");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(create_objects({{0x3102_oid, 0x102_did}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Delete all objects");
    for (auto &s : services) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], destroyed());

    sut_->update(services);
    mock_endpoint_factory_->verifyAndClearMocks();
  }

  EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());
}

TEST_F(BaseEndpointManagerTests, sut_manages_service_deactivate_endpoints) {
  std::vector<DbService> services{
      create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}})};
  const EnabledType k_is_disabled = EnabledType::EnabledType_none;

  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");

    // service-id:   0      1
    //               |      |
    // schema-id:    0      1
    //              / \     |
    // obj-id:     0   1    2

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);
    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}}));
    sut_->update(create_objects({{0x1101_oid, 0x101_did},
                                 {0x2101_oid, 0x101_did},
                                 {0x3102_oid, 0x102_did}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Update service 2, observe update on db_object");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], deactivate());

    sut_->update(create_services({{0x02_sid, k_host_id}}, k_is_disabled));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Update service 1, observe update on two db_objects");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], deactivate());

    sut_->update(create_services({{0x01_sid, k_host_id}}, k_is_disabled));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Update schema 1, observer update on two db-objects");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], deactivate());

    sut_->update(create_schemas({{0x101_did, 0x1_sid}}, k_is_disabled));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Change schema 1, update two db-objects
  {
    SCOPED_TRACE("Update object 3");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], deactivate());

    sut_->update(create_objects({{0x3102_oid, 0x102_did}}, k_is_disabled));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Delete all objects");
    for (auto &s : services) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], destroyed());

    sut_->update(services);
    mock_endpoint_factory_->verifyAndClearMocks();
  }

  EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());
}

TEST_F(BaseEndpointManagerTests, sut_manages_service_schemas) {
  std::vector<DbService> services{create_services({{0x01_sid, k_host_id}})};
  std::vector<DbSchema> schemas{
      create_schemas({{0x101_did, 0x01_sid}, {0x102_did, 0x01_sid}})};

  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);
    sut_->update(schemas);

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Delete the service, which triggers the deletion of all schemas
  {
    SCOPED_TRACE("Cleanup");
    for (auto &s : services) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], destroyed());

    sut_->update(services);

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());
}

TEST_F(BaseEndpointManagerTests,
       sut_manages_host_update_and_deletes_dependent_endpoints) {
  std::vector<DbService> services{
      create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}})};

  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");

    // service-id:   0      1
    //               |      |
    // schema-id:    0      1
    //              / \     |
    // obj-id:     0   1    2

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);
    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}}));
    sut_->update(create_objects({{0x1101_oid, 0x101_did},
                                 {0x2101_oid, 0x101_did},
                                 {0x3102_oid, 0x102_did}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Change host 1, update on all sub-objects
  {
    SCOPED_TRACE("Update host, observe update on db_object");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());

    sut_->update(k_hosts);

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Delete all objects");
    auto hosts = k_hosts;
    for (auto &s : hosts) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());

    sut_->update(hosts);
    mock_endpoint_factory_->verifyAndClearMocks();
  }
}

TEST_F(BaseEndpointManagerTests,
       sut_manages_service_disable_and_deactivate_dependent_endpoints) {
  std::vector<DbService> services{
      create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}})};

  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");

    // service-id:   0      1
    //               |      |
    // schema-id:    0      1
    //              / \     |
    // obj-id:     0   1    2

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);
    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}}));
    sut_->update(create_objects({{0x1101_oid, 0x101_did},
                                 {0x2101_oid, 0x101_did},
                                 {0x3102_oid, 0x102_did}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Disable service, disable on all sub-objects
  {
    SCOPED_TRACE("Update service, observe update on db_object");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], deactivate());

    sut_->update(create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}},
                                 EnabledType::EnabledType_none));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Delete all objects");
    auto hosts = k_hosts;
    for (auto &s : hosts) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());

    sut_->update(hosts);
    mock_endpoint_factory_->verifyAndClearMocks();
  }
}

TEST_F(BaseEndpointManagerTests,
       sut_manages_schema_disable_and_deactivate_dependent_endpoints) {
  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");

    // service-id:   0      1
    //               |      |
    // schema-id:    0      1
    //              / \     |
    // obj-id:     0   1    2

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(k_hosts);
    sut_->update(
        create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}}));
    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}}));
    sut_->update(create_objects({{0x1101_oid, 0x101_did},
                                 {0x2101_oid, 0x101_did},
                                 {0x3102_oid, 0x102_did}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // Disable schemas, disable on all sub-objects
  {
    SCOPED_TRACE("Disable schemas, observe deactivation on db_object");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], deactivate());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], deactivate());

    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}},
                                EnabledType::EnabledType_none));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Delete all objects");
    auto hosts = k_hosts;
    for (auto &s : hosts) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());

    sut_->update(hosts);
    mock_endpoint_factory_->verifyAndClearMocks();
  }
}

TEST_F(BaseEndpointManagerTests,
       sut_manages_service_private_and_expect_dependent_endpoints_private) {
  std::vector<DbService> services{
      create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}})};

  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");

    // service-id:   0      1
    //               |      |
    // schema-id:    0      1
    //              / \     |
    // obj-id:     0   1    2

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);
    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}}));
    sut_->update(create_objects({{0x1101_oid, 0x101_did},
                                 {0x2101_oid, 0x101_did},
                                 {0x3102_oid, 0x102_did}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // make service private, all sub-objects will be also private
  {
    SCOPED_TRACE("Update service, observe update on db_object");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_private());

    sut_->update(create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}},
                                 EnabledType::EnabledType_private));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Delete all objects");
    auto hosts = k_hosts;
    for (auto &s : hosts) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());

    sut_->update(hosts);
    mock_endpoint_factory_->verifyAndClearMocks();
  }
}

TEST_F(BaseEndpointManagerTests,
       sut_manages_schema_private_and_expect_dependent_endpoints_private) {
  std::vector<DbService> services{
      create_services({{0x01_sid, k_host_id}, {0x02_sid, k_host_id}})};

  // Put two services into the manager
  {
    SCOPED_TRACE("Setup");

    // service-id:   0      1
    //               |      |
    // schema-id:    0      1
    //              / \     |
    // obj-id:     0   1    2

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], created());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], created());

    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_public());

    sut_->update(k_hosts);
    sut_->update(services);
    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}}));
    sut_->update(create_objects({{0x1101_oid, 0x101_did},
                                 {0x2101_oid, 0x101_did},
                                 {0x3102_oid, 0x102_did}}));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // make schema private, all sub-objects will be also private
  {
    SCOPED_TRACE("Update service, observe update on db_object");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_private());

    sut_->update(create_schemas({{0x101_did, 0x1_sid}, {0x102_did, 0x2_sid}},
                                EnabledType::EnabledType_private));

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  // update service, services public and all sub-objects will be also private
  {
    SCOPED_TRACE("Update service, observe update on db_object");
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], activate_public());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], activate_private());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], activate_private());

    sut_->update(services);

    mock_endpoint_factory_->verifyAndClearMocks();
  }

  {
    SCOPED_TRACE("Delete all objects");
    auto hosts = k_hosts;
    for (auto &s : hosts) {
      s.deleted = true;
    }
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_service[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_schema[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[0], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[1], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_db_object[2], destroyed());
    EXPECT_CALL(mock_endpoint_factory_->mock_url_host[0], destroyed());

    sut_->update(hosts);
    mock_endpoint_factory_->verifyAndClearMocks();
  }
}
