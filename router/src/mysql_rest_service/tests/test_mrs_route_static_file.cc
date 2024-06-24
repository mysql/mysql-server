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

#include "helper/make_shared_ptr.h"
#include "mrs/database/entry/content_file.h"
#include "mrs/object_static_file.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_handler_factory.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_rest_handler.h"
#include "mock/mock_route_schema.h"
#include "mock/mock_session.h"

using namespace helper::json;

using helper::MakeSharedPtr;
using mrs::ObjectStaticFile;
using mrs::interface::Object;
using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using testing::Test;
using ContentFile = mrs::rest::entry::AppContentFile;

class RouteStaticFileTests : public Test {
 public:
  auto make_test_data(const mrs::UniversalId service_id,
                      const mrs::UniversalId set_id, const std::string &service,
                      const std::string &schema, const std::string &object) {
    ContentFile obj;

    obj.active_service = obj.active_set = obj.active_file = true;
    obj.deleted = false;

    obj.id = mrs::UniversalId{{static_cast<uint8_t>(last_id_ % 256),
                               static_cast<uint8_t>(last_id_ / 256)}};
    obj.service_id = service_id;
    obj.content_set_id = set_id;

    obj.service_path = service;
    obj.schema_path = schema;
    obj.file_path = object;

    obj.host = "mysql.com";
    obj.requires_authentication = false;
    obj.schema_requires_authentication = false;
    obj.size = 100;

    return obj;
  }

  void make_sut(const ContentFile &obj, const bool is_https = false) {
    Object *register_argument = nullptr;
    EXPECT_CALL(*mock_route_schema_, route_register(_))
        .WillOnce(
            Invoke([&register_argument](Object *r) { register_argument = r; }));

    sut_ = std::make_shared<ObjectStaticFile>(
        obj, mock_route_schema_, &mock_mysqlcache_, is_https,
        &mock_auth_manager_, mock_handler_factory_);
    ASSERT_EQ(sut_.get(), register_argument);
  }

  void delete_sut() { sut_.reset(); }

  void verifyAndClearMocks(const std::vector<void *> &mocks) {
    for (auto p : mocks) Mock::VerifyAndClearExpectations(p);
  }

  MakeSharedPtr<StrictMock<MockHandlerFactory>> mock_handler_factory_;
  StrictMock<MockAuthManager> mock_auth_manager_;
  StrictMock<MockMysqlCacheManager> mock_mysqlcache_;
  StrictMock<MockMySQLSession> mock_session;
  MakeSharedPtr<StrictMock<MockRouteSchema>> mock_route_schema_;
  std::shared_ptr<ObjectStaticFile> sut_;

 private:
  uint64_t last_id_{0};
};

TEST_F(RouteStaticFileTests, validate_route_generic_parameters) {
  const mrs::UniversalId kServiceId{33};
  const mrs::UniversalId kSchemaId{44};
  auto pe = make_test_data(kServiceId, kSchemaId, "/ser", "/sch", "/obj");
  make_sut(pe);

  EXPECT_EQ("", sut_->get_rest_canonical_url());
  EXPECT_EQ("", sut_->get_rest_canonical_path());
  EXPECT_EQ("http://mysql.com/ser/sch/obj", sut_->get_rest_url());
  ASSERT_EQ(1, sut_->get_rest_path().size());
  EXPECT_EQ("^/ser/sch/obj$", sut_->get_rest_path()[0]);
  EXPECT_EQ("/ser/sch/obj", sut_->get_rest_path_raw());
  EXPECT_EQ("/obj", sut_->get_object_path());
  EXPECT_EQ("", sut_->get_object_name());
  EXPECT_EQ("", sut_->get_schema_name());
  EXPECT_EQ(1, sut_->get_on_page());
  EXPECT_EQ("", sut_->get_user_row_ownership().user_ownership_column);
  EXPECT_EQ(false, sut_->get_user_row_ownership().user_ownership_enforced);
  EXPECT_EQ(0, sut_->get_group_row_ownership().size());
  EXPECT_EQ(kServiceId, sut_->get_service_id());
  EXPECT_EQ(pe.id, sut_->get_id());
  EXPECT_EQ(false, sut_->requires_authentication());
  EXPECT_EQ(mock_route_schema_.get(), sut_->get_schema());
  EXPECT_EQ("", sut_->get_json_description());
  EXPECT_EQ(Object::kMedia, sut_->get_format());
  EXPECT_EQ(Object::kRead, sut_->get_access());
}

TEST_F(RouteStaticFileTests, validate_route_parameters_after_update) {
  const mrs::UniversalId kServiceId{33};
  const mrs::UniversalId kSchemaId{44};
  const std::string kNewHost = "abc.de";
  const std::string kNewServicePath = "/mrs";
  const std::string kNewSchemaPath = "/sakila";
  const std::string kNewObjectPath = "/city";
  const std::string kNewSchema = "sakila";
  const std::string kNewObject = "city";

  auto pe = make_test_data(kServiceId, kSchemaId, "/ser", "/sch", "/obj");
  make_sut(pe);

  pe.service_path = kNewServicePath;
  pe.schema_path = kNewSchemaPath;
  pe.file_path = kNewObjectPath;
  pe.host = kNewHost;
  pe.schema_requires_authentication = true;
  pe.requires_authentication = true;
  pe.size = 200;

  sut_->update(&pe, mock_route_schema_);
  EXPECT_EQ("", sut_->get_rest_canonical_url());
  EXPECT_EQ("", sut_->get_rest_canonical_path());
  EXPECT_EQ("http://abc.de/mrs/sakila/city", sut_->get_rest_url());
  ASSERT_EQ(1, sut_->get_rest_path().size());
  EXPECT_EQ("^/mrs/sakila/city$", sut_->get_rest_path()[0]);
  EXPECT_EQ("/mrs/sakila/city", sut_->get_rest_path_raw());
  EXPECT_EQ(kNewObjectPath, sut_->get_object_path());
  EXPECT_EQ("", sut_->get_object_name());
  EXPECT_EQ("", sut_->get_schema_name());
  EXPECT_EQ(1, sut_->get_on_page());
  EXPECT_EQ(Object::kMedia, sut_->get_format());
  EXPECT_EQ(Object::kRead, sut_->get_access());
  EXPECT_EQ("", sut_->get_user_row_ownership().user_ownership_column);
  EXPECT_EQ(false, sut_->get_user_row_ownership().user_ownership_enforced);
  EXPECT_EQ(0, sut_->get_group_row_ownership().size());
  EXPECT_EQ(kServiceId, sut_->get_service_id());
  EXPECT_EQ(pe.id, sut_->get_id());
  EXPECT_EQ(true, sut_->requires_authentication());
  EXPECT_EQ(mock_route_schema_.get(), sut_->get_schema());

  EXPECT_EQ("", sut_->get_json_description());
}

TEST_F(RouteStaticFileTests, route_turnon_on_deactivated_route_does_nothing) {
  const mrs::UniversalId kServiceId{22};
  const mrs::UniversalId kSchemaId{11};
  auto pe = make_test_data(kServiceId, kSchemaId, "/a", "/b", "/c");
  pe.active_service = pe.active_set = pe.active_file = false;
  make_sut(pe);
  sut_->turn(mrs::stateOn);
  delete_sut();
}

TEST_F(RouteStaticFileTests,
       route_turnon_on_activated_table_route_registers_the_request_handler) {
  const mrs::UniversalId kServiceId{22};
  const mrs::UniversalId kSchemaId{11};
  auto pe = make_test_data(kServiceId, kSchemaId, "/a", "/b", "/c");
  make_sut(pe);
  //  EXPECT_CALL(create_object_metadata_handler(sut_.get(),
  //  &mock_auth_manager_));
  sut_->turn(mrs::stateOn);
  verifyAndClearMocks({mock_handler_factory_.get()});
  delete_sut();
}

TEST_F(RouteStaticFileTests, second_activation_recreates_handler) {
  const mrs::UniversalId kServiceId{22};
  const mrs::UniversalId kSchemaId{11};
  auto pe = make_test_data(kServiceId, kSchemaId, "/a", "/b", "/c");
  make_sut(pe);

  //  EXPECT_CALL(*mock_handler_factory_,
  //              create_object_handler(sut_.get(), &mock_auth_manager_));
  //  EXPECT_CALL(*mock_handler_factory_,
  //              create_object_metadata_handler(sut_.get(),
  //              &mock_auth_manager_));
  sut_->turn(mrs::stateOn);
  verifyAndClearMocks({mock_handler_factory_.get()});

  //  EXPECT_CALL(*mock_handler_factory_,
  //              create_object_handler(sut_.get(), &mock_auth_manager_));
  //  EXPECT_CALL(*mock_handler_factory_,
  //              create_object_metadata_handler(sut_.get(),
  //              &mock_auth_manager_));
  sut_->turn(mrs::stateOn);
  verifyAndClearMocks({mock_handler_factory_.get()});

  delete_sut();
}
