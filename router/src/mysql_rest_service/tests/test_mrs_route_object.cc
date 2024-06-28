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
#include "mrs/object.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_handler_factory.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_query_factory.h"
#include "mock/mock_query_table_columns.h"
#include "mock/mock_rest_handler.h"
#include "mock/mock_route_schema.h"
#include "mock/mock_session.h"

using namespace helper::json;

using CachedObject = collector::MysqlCacheManager::CachedObject;

using helper::MakeSharedPtr;
using mrs::interface::Object;
using EntryDbObject = mrs::database::entry::DbObject;
using mrs::database::entry::Operation;
using mrs::database::entry::RowGroupOwnership;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

const int kFirstColumnId = 123;
const uint64_t kDefaultInPage = 24;
const auto kDefaultForamt = EntryDbObject::formatFeed;
const auto kDefaultOperation = Operation::valueRead;

template <typename T>
auto make_shared_for_mock(T *t) {
  return std::shared_ptr<T>(t, [](T *) {});
}

template <typename T>
auto make_shared_for_mock_track(T *t) {
  return std::shared_ptr<T>(t, [](T *r) { r->destroy(); });
}

class RouteObjectTests : public Test {
 public:
  auto make_test_data(const mrs::UniversalId service_id,
                      const mrs::UniversalId schema_id,
                      const std::string &service, const std::string &schema,
                      const std::string &object) {
    using namespace std::string_literals;
    EntryDbObject obj;

    ++last_id_;
    obj.id = mrs::UniversalId({static_cast<uint8_t>(last_id_ / 256),
                               static_cast<uint8_t>(last_id_ % 256)});
    obj.active_object = obj.active_schema = obj.active_service = true;

    obj.service_id = service_id;
    obj.schema_id = schema_id;

    obj.service_path = service;
    obj.schema_path = schema;
    obj.object_path = object;

    obj.db_schema = schema.substr(1);
    obj.db_table = object.substr(1);
    obj.on_page = kDefaultInPage;
    obj.requires_authentication = false;
    obj.schema_requires_authentication = false;
    obj.deleted = false;
    obj.type = EntryDbObject::typeTable;
    obj.operation = kDefaultOperation;
    obj.autodetect_media_type = false;
    obj.host = "mysql.com";
    obj.format = kDefaultForamt;
    obj.object_description = std::make_shared<mrs::database::entry::Object>();

    first_field_ = std::make_shared<mrs::database::entry::ObjectField>();
    first_field_->id = mrs::UniversalId{kFirstColumnId};
    first_field_->name = "name";
    obj.object_description->fields.push_back(first_field_);

    return obj;
  }

  void delete_sut() {
    EXPECT_CALL(*mock_route_schema_, route_unregister(sut_.get()));
    sut_.reset();
  }

  void make_sut(const EntryDbObject &obj, const bool is_https = false) {
    Object *register_argument = nullptr;
    EXPECT_CALL(*mock_route_schema_, route_register(_))
        .WillOnce(Invoke([&register_argument](mrs::interface::Object *r) {
          register_argument = r;
        }));

    sut_ = std::make_shared<mrs::Object>(
        obj, mock_route_schema_, &mock_mysqlcache_, is_https,
        &mock_auth_manager_, nullptr, &mock_handler_factory_,
        &mock_query_factory_);
    ASSERT_EQ(sut_.get(), register_argument);
  }

  void verifyAndClearMocks(const std::vector<void *> &mocks) {
    for (auto p : mocks) Mock::VerifyAndClearExpectations(p);
  }

  StrictMock<MockQueryFactory> mock_query_factory_;
  StrictMock<MockHandlerFactory> mock_handler_factory_;
  StrictMock<MockAuthManager> mock_auth_manager_;
  StrictMock<MockMysqlCacheManager> mock_mysqlcache_;
  StrictMock<MockMySQLSession> mock_session;
  MakeSharedPtr<StrictMock<MockRouteSchema>> mock_route_schema_;
  std::shared_ptr<Object> sut_;
  std::shared_ptr<mrs::database::entry::ObjectField> first_field_;

 private:
  uint64_t last_id_{0};
};

TEST_F(RouteObjectTests, validate_route_generic_parameters) {
  const mrs::UniversalId kServiceId{33};
  const mrs::UniversalId kSchemaId{44};
  auto pe = make_test_data(kServiceId, kSchemaId, "/ser", "/sch", "/obj");
  make_sut(pe);

  EXPECT_EQ("http://mysql.com/ser/sch/metadata-catalog/obj",
            sut_->get_rest_canonical_url());
  EXPECT_EQ("http://mysql.com/ser/sch/obj", sut_->get_rest_url());
  ASSERT_EQ(1, sut_->get_rest_path().size());
  EXPECT_EQ("^/ser/sch/obj(/([0-9]|[a-z]|[A-Z]|[-._~!$&'()*+,;=:@%]| )*/?)?$",
            sut_->get_rest_path()[0]);
  EXPECT_EQ("/ser/sch/obj", sut_->get_rest_path_raw());
  EXPECT_EQ("^/ser/sch/metadata-catalog/obj/?$",
            sut_->get_rest_canonical_path());
  EXPECT_EQ("/obj", sut_->get_object_path());
  EXPECT_EQ("obj", sut_->get_object_name());
  EXPECT_EQ("sch", sut_->get_schema_name());
  EXPECT_EQ(kDefaultInPage, sut_->get_on_page());
  EXPECT_EQ(kDefaultForamt, sut_->get_format());
  EXPECT_EQ(kDefaultOperation, sut_->get_access());
  EXPECT_EQ("", sut_->get_user_row_ownership().user_ownership_column);
  EXPECT_EQ(false, sut_->get_user_row_ownership().user_ownership_enforced);
  EXPECT_EQ(0, sut_->get_group_row_ownership().size());
  EXPECT_EQ(kServiceId, sut_->get_service_id());
  EXPECT_EQ(pe.id, sut_->get_id());
  EXPECT_EQ(false, sut_->requires_authentication());
  EXPECT_EQ(mock_route_schema_.get(), sut_->get_schema());

  std::string error_description;
  std::string schema{R"({
  "id": "http://json-schema.org/draft-04/schema#",
  "$schema": "http://json-schema.org/draft-04/schema#",
  "type": "object",
  "required" : ["name","links"],
  "properties": {
    "name": { "type": "string", "enum": "/obj" },
    "links": {
       "type": "array",
       "items": [ {
             "type": "object",
             "required" : ["rel","href"],
             "properties": {
                "rel": { "type":"string", "enum": ["describes"]},
                "href": { "type":"string", "enum": ["http://mysql.com/ser/sch/obj"]}
             }
       }, {
             "type": "object",
             "required" : ["rel","href"],
             "properties": {
                "rel": { "type":"string", "enum": ["canonical"]},
                "href": { "type":"string", "enum": ["http://mysql.com/ser/sch/metadata-catalog/obj"]}
             }
       }]
    }
   }})"};
  EXPECT_TRUE(validate_json_with_schema(sut_->get_json_description(), schema,
                                        &error_description))
      << error_description;

  delete_sut();
}

TEST_F(RouteObjectTests, validate_route_parameters_after_update) {
  const mrs::UniversalId kServiceId{33};
  const mrs::UniversalId kSchemaId{44};
  const uint64_t kNewInPage = 1232;
  const auto kNewFormat = EntryDbObject::formatItem;
  const auto kNewOperation = Operation::valueUpdate;
  const std::string kNewHost = "abc.de";
  const std::string kNewServicePath = "/mrs";
  const std::string kNewSchemaPath = "/sakila";
  const std::string kNewObjectPath = "/city";
  const std::string kNewSchema = "sakila";
  const std::string kNewObject = "city";

  auto pe = make_test_data(kServiceId, kSchemaId, "/ser", "/sch", "/obj");
  make_sut(pe);

  pe.db_schema = kNewSchema;
  pe.db_table = kNewObject;
  pe.service_path = kNewServicePath;
  pe.schema_path = kNewSchemaPath;
  pe.object_path = kNewObjectPath;
  pe.host = kNewHost;
  pe.format = kNewFormat;
  pe.operation = kNewOperation;
  pe.on_page = kNewInPage;
  pe.schema_requires_authentication = true;
  pe.requires_authentication = true;
  pe.object_description->user_ownership_field.emplace();
  pe.object_description->user_ownership_field->uid = first_field_->id;
  pe.object_description->user_ownership_field->field = first_field_;
  pe.row_group_security.push_back(RowGroupOwnership{
      mrs::UniversalId{101}, "group_name", 0, RowGroupOwnership::kHigher});

  sut_->update(&pe, mock_route_schema_);
  EXPECT_EQ("http://abc.de/mrs/sakila/metadata-catalog/city",
            sut_->get_rest_canonical_url());
  EXPECT_EQ("http://abc.de/mrs/sakila/city", sut_->get_rest_url());
  ASSERT_EQ(1, sut_->get_rest_path().size());
  EXPECT_EQ(
      "^/mrs/sakila/city(/([0-9]|[a-z]|[A-Z]|[-._~!$&'()*+,;=:@%]| )*/?)?$",
      sut_->get_rest_path()[0]);
  EXPECT_EQ("/mrs/sakila/city", sut_->get_rest_path_raw());
  EXPECT_EQ("^/mrs/sakila/metadata-catalog/city/?$",
            sut_->get_rest_canonical_path());
  EXPECT_EQ(kNewObjectPath, sut_->get_object_path());
  EXPECT_EQ(kNewObject, sut_->get_object_name());
  EXPECT_EQ(kNewSchema, sut_->get_schema_name());
  EXPECT_EQ(kNewInPage, sut_->get_on_page());
  EXPECT_EQ(kNewFormat, sut_->get_format());
  EXPECT_EQ(kNewOperation, sut_->get_access());
  EXPECT_EQ("name", sut_->get_user_row_ownership().user_ownership_column);
  EXPECT_EQ(true, sut_->get_user_row_ownership().user_ownership_enforced);
  EXPECT_EQ(1, sut_->get_group_row_ownership().size());
  EXPECT_EQ(kServiceId, sut_->get_service_id());
  EXPECT_EQ(pe.id, sut_->get_id());
  EXPECT_EQ(true, sut_->requires_authentication());
  EXPECT_EQ(mock_route_schema_.get(), sut_->get_schema());

  std::string error_description;
  std::string schema{R"({
  "id": "http://json-schema.org/draft-04/schema#",
  "$schema": "http://json-schema.org/draft-04/schema#",
  "type": "object",
  "required" : ["name","links"],
  "properties": {
    "name": { "type": "string", "enum": "/city" },
    "links": {
       "type": "array",
       "items": [ {
             "type": "object",
             "required" : ["rel","href"],
             "properties": {
                "rel": { "type":"string", "enum": ["describes"]},
                "href": { "type":"string", "enum": ["http://abc.de/mrs/sakila/city"]}
             }
       }, {
             "type": "object",
             "required" : ["rel","href"],
             "properties": {
                "rel": { "type":"string", "enum": ["canonical"]},
                "href": { "type":"string", "enum": ["http://abc.de/mrs/sakila/metadata-catalog/city"]}
             }
       }]
    }
   }})"};
  EXPECT_TRUE(validate_json_with_schema(sut_->get_json_description(), schema,
                                        &error_description))
      << error_description;

  delete_sut();
}

TEST_F(RouteObjectTests, route_turnon_on_deactivated_route_does_nothing) {
  const mrs::UniversalId kServiceId{22};
  const mrs::UniversalId kSchemaId{11};
  auto pe = make_test_data(kServiceId, kSchemaId, "/a", "/b", "/c");
  pe.active_object = pe.active_schema = pe.active_service = false;
  make_sut(pe);
  sut_->turn(mrs::stateOn);
  delete_sut();
}

TEST_F(RouteObjectTests,
       route_turnon_on_activated_table_route_registers_the_request_handler) {
  const mrs::UniversalId kServiceId{22};
  const mrs::UniversalId kSchemaId{11};
  auto pe = make_test_data(kServiceId, kSchemaId, "/a", "/b", "/c");
  make_sut(pe);
  EXPECT_CALL(mock_handler_factory_,
              create_object_handler(sut_.get(), &mock_auth_manager_, _));
  EXPECT_CALL(mock_handler_factory_,
              create_object_metadata_handler(sut_.get(), &mock_auth_manager_));
  sut_->turn(mrs::stateOn);
  verifyAndClearMocks({&mock_handler_factory_});
  delete_sut();
}

TEST_F(RouteObjectTests, second_activation_recreates_handler) {
  const mrs::UniversalId kServiceId{22};
  const mrs::UniversalId kSchemaId{11};
  auto pe = make_test_data(kServiceId, kSchemaId, "/a", "/b", "/c");
  make_sut(pe);

  EXPECT_CALL(mock_handler_factory_,
              create_object_handler(sut_.get(), &mock_auth_manager_, _));
  EXPECT_CALL(mock_handler_factory_,
              create_object_metadata_handler(sut_.get(), &mock_auth_manager_));
  sut_->turn(mrs::stateOn);
  verifyAndClearMocks({&mock_handler_factory_});

  EXPECT_CALL(mock_handler_factory_,
              create_object_handler(sut_.get(), &mock_auth_manager_, _));
  EXPECT_CALL(mock_handler_factory_,
              create_object_metadata_handler(sut_.get(), &mock_auth_manager_));
  sut_->turn(mrs::stateOn);
  verifyAndClearMocks({&mock_handler_factory_});

  delete_sut();
}
