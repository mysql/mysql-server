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
#include <string>

#include "helper/json/rapid_json_to_map.h"
#include "helper/json/schema_validator.h"
#include "helper/json/text_to.h"
#include "helper/make_shared_ptr.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/object_schema.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_handler_factory.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_object.h"
#include "mock/mock_rest_handler.h"
#include "mock/mock_route_manager.h"

using namespace helper::json;

using helper::MakeSharedPtr;
using mrs::ObjectSchema;
using mrs::interface::Object;
using testing::ByMove;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

class RouteSchemaRestTests : public Test {
 public:
  void verifyAndClearMocks(const std::vector<void *> &mocks) {
    for (auto p : mocks) Mock::VerifyAndClearExpectations(p);
  }

  void make_sut(const mrs::UniversalId service_id,
                const mrs::UniversalId schema_id,
                const std::string &service_name, const std::string &schema_name,
                const bool is_ssl = false, const bool require_auth = false,
                const std::string &host = "127.0.0.1") {
    sut_ = std::make_shared<ObjectSchema>(
        &mock_route_manager_, &mock_mysqlcache_, service_name, schema_name,
        is_ssl, host, require_auth, service_id, schema_id, "",
        &mock_auth_manager_, mock_handler_factory_);
  }

  StrictMock<MockRouteManager> mock_route_manager_;
  StrictMock<MockMysqlCacheManager> mock_mysqlcache_;
  StrictMock<MockAuthManager> mock_auth_manager_;
  MakeSharedPtr<StrictMock<MockHandlerFactory>> mock_handler_factory_;

  std::shared_ptr<ObjectSchema> sut_;

 private:
  uint64_t last_id_{0};
};

TEST_F(RouteSchemaRestTests, expect_generic_data1) {
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  const auto k_service_name = "/ser";
  const auto k_schema_name = "/sch";

  make_sut(k_service_id, k_schema_id, k_service_name, k_schema_name);

  EXPECT_EQ(k_service_id, sut_->get_service_id());
  EXPECT_EQ(k_schema_id, sut_->get_id());
  EXPECT_EQ(k_schema_name, sut_->get_name());
  EXPECT_EQ("http://127.0.0.1/ser/sch/metadata-catalog", sut_->get_url());
  EXPECT_EQ("^/ser/sch/metadata-catalog/?$", sut_->get_path());
  EXPECT_EQ(false, sut_->requires_authentication());
  EXPECT_EQ(0, sut_->get_routes().size());
}

TEST_F(RouteSchemaRestTests, expect_generic_data2) {
  const mrs::UniversalId k_service_id{101};
  const mrs::UniversalId k_schema_id{202};
  const auto k_service_name = "/service";
  const auto k_schema_name = "/schema";

  make_sut(k_service_id, k_schema_id, k_service_name, k_schema_name, true, true,
           "localhost");

  EXPECT_EQ(k_service_id, sut_->get_service_id());
  EXPECT_EQ(k_schema_id, sut_->get_id());
  EXPECT_EQ(k_schema_name, sut_->get_name());
  EXPECT_EQ("https://localhost/service/schema/metadata-catalog",
            sut_->get_url());
  EXPECT_EQ("^/service/schema/metadata-catalog/?$", sut_->get_path());
  EXPECT_EQ(true, sut_->requires_authentication());
  EXPECT_EQ(0, sut_->get_routes().size());
}

TEST_F(RouteSchemaRestTests, register_unregister_route) {
  StrictMock<MockRoute> route;
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  make_sut(k_service_id, k_schema_id, "/ser", "/sch");

  sut_->route_register(&route);
  auto routes = sut_->get_routes();
  ASSERT_EQ(1, routes.size());
  ASSERT_EQ(&route, routes[0]);

  // When last object is removed from the schema, then we are notifing the
  // manager about that.
  EXPECT_CALL(mock_route_manager_, schema_not_used(sut_.get()));
  sut_->route_unregister(&route);

  routes = sut_->get_routes();
  ASSERT_EQ(0, routes.size());
}

TEST_F(RouteSchemaRestTests, register_unregister_routes) {
  StrictMock<MockRoute> routes[4];
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  make_sut(k_service_id, k_schema_id, "/ser", "/sch");

  sut_->route_register(&routes[0]);

  auto r = sut_->get_routes();
  ASSERT_EQ(1, r.size());
  ASSERT_EQ(&routes[0], r[0]);

  sut_->route_register(&routes[1]);
  sut_->route_register(&routes[2]);
  sut_->route_unregister(&routes[0]);

  r = sut_->get_routes();
  ASSERT_EQ(2, r.size());
  ASSERT_EQ(&routes[1], r[0]);
  ASSERT_EQ(&routes[2], r[1]);

  sut_->route_unregister(&routes[2]);

  // When last object is removed from the schema, then we are notifing the
  // manager about that.
  EXPECT_CALL(mock_route_manager_, schema_not_used(sut_.get()));
  sut_->route_unregister(&routes[1]);

  r = sut_->get_routes();
  ASSERT_EQ(0, r.size());
}

TEST_F(RouteSchemaRestTests, turn_off_does_nothing_when_already_off) {
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  make_sut(k_service_id, k_schema_id, "/ser", "/sch");

  sut_->turn(mrs::stateOff);
}

TEST_F(RouteSchemaRestTests, turn_on) {
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  make_sut(k_service_id, k_schema_id, "/ser", "/sch");

  EXPECT_CALL(*mock_handler_factory_,
              create_schema_metadata_handler(sut_.get(), &mock_auth_manager_));
  sut_->turn(mrs::stateOn);

  // Second turnOn, does nothing.
  sut_->turn(mrs::stateOn);
}

TEST_F(RouteSchemaRestTests, turn_off_releases_the_object) {
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  make_sut(k_service_id, k_schema_id, "/ser", "/sch");

  class TrackDestructionRestHandler : public MockRestHandler {
   public:
    TrackDestructionRestHandler(bool &release) : release_{release} {}
    ~TrackDestructionRestHandler() { release_ = true; }
    bool &release_;
  };

  bool released = false;

  EXPECT_CALL(*mock_handler_factory_,
              create_schema_metadata_handler(sut_.get(), &mock_auth_manager_))
      .WillOnce(Return(
          ByMove(std::make_unique<TrackDestructionRestHandler>(released))));
  sut_->turn(mrs::stateOn);
  verifyAndClearMocks({&mock_handler_factory_});

  sut_->turn(mrs::stateOff);
}
