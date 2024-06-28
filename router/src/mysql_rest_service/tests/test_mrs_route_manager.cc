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

#include "mrs/object_manager.h"

#include "mock/mock_auth_manager.h"
#include "mock/mock_mysqlcachemanager.h"
#include "mock/mock_object.h"
#include "mock/mock_route_factory.h"
#include "mock/mock_route_schema.h"

using mrs::database::entry::ContentFile;
using mrs::database::entry::DbObject;
using mrs::interface::Object;
using mrs::interface::ObjectSchema;
using testing::_;
using testing::ByMove;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;
using testing::Test;

MATCHER_P(EqSmartPtr, raw_ptr, "") { return raw_ptr == arg.get(); }
MATCHER_P(ById, id, "") { return id == arg.id; }

MATCHER_P(DbObjectById, id, "") {
  return mrs::UniversalId({id}) == reinterpret_cast<const DbObject *>(arg)->id;
}

template <typename T>
class RouteManagerTests : public Test {
 public:
  void SetUp() override {
    const bool k_is_ssl = true;
    sut_.reset(new mrs::ObjectManager(&mock_mysqlcache_, k_is_ssl,
                                      &mock_auth_manager_, nullptr,
                                      &mock_route_factory_));
  }

  struct EntryId {
    mrs::UniversalId schema_id_;
    mrs::UniversalId obj_id_;
  };

  void verifyAndClearMocks(const std::vector<void *> &mocks) {
    Mock::VerifyAndClearExpectations(&mock_route_factory_);
    Mock::VerifyAndClearExpectations(&mock_auth_manager_);
    Mock::VerifyAndClearExpectations(&mock_mysqlcache_);

    for (auto p : mocks) Mock::VerifyAndClearExpectations(p);
  }

  static mrs::UniversalId get_schema_id(const ContentFile &cfile) {
    return cfile.content_set_id;
  }

  static mrs::UniversalId get_schema_id(const DbObject &obj) {
    return obj.schema_id;
  }

  template <typename Obj>
  void expect_create_schema(MockRouteSchema &return_mock, const Obj &obj,
                            bool track_destruction = false) {
    EXPECT_CALL(mock_route_factory_,
                create_router_schema(_, _, _, _, _, _, _, obj.service_id,
                                     get_schema_id(obj), _, _))
        .WillOnce(Return(ByMove(std::shared_ptr<ObjectSchema>(
            &return_mock, [track_destruction](ObjectSchema *r) {
              if (track_destruction)
                reinterpret_cast<MockRouteSchema *>(r)->destroy();
            }))));
  }

  void expect_create(MockRoute &return_mock, const DbObject &obj,
                     bool track_destruction = false) {
    EXPECT_CALL(mock_route_factory_,
                create_router_object(ById(obj.id), _, _, _, _, _))
        .WillOnce(Return(ByMove(std::shared_ptr<Object>(
            &return_mock, [track_destruction](Object *r) {
              if (track_destruction)
                reinterpret_cast<MockRoute *>(r)->destroy();
            }))));
  }

  void expect_create(MockRoute &return_mock, const ContentFile &obj,
                     bool track_destruction = false) {
    EXPECT_CALL(mock_route_factory_,
                create_router_static_object(ById(obj.id), _, _, _, _))
        .WillOnce(Return(ByMove(std::shared_ptr<Object>(
            &return_mock, [track_destruction](Object *r) {
              if (track_destruction)
                reinterpret_cast<MockRoute *>(r)->destroy();
            }))));
  }

  static void create_testing_objects(std::vector<DbObject> &result,
                                     mrs::UniversalId service_id,
                                     const std::vector<EntryId> &ids) {
    using namespace std::string_literals;
    for (auto &entry : ids) {
      DbObject item;
      item.active_object = item.active_schema = item.active_service = true;
      item.deleted = false;
      item.autodetect_media_type = false;
      item.service_id = service_id;
      item.db_schema = "obj"s + service_id.to_string() + "schema"s +
                       entry.schema_id_.to_string();
      item.schema_path = item.db_schema;
      item.schema_id = entry.schema_id_;
      item.db_table = "object"s + entry.obj_id_.to_string();
      item.object_path = item.db_table;
      item.id = entry.obj_id_;
      result.push_back(item);
    }
  }

  static void create_testing_objects(std::vector<ContentFile> &result,
                                     mrs::UniversalId service_id,
                                     const std::vector<EntryId> &ids) {
    using namespace std::string_literals;
    for (auto &entry : ids) {
      ContentFile item;
      item.active_service = item.active_set = item.active_file = true;
      item.deleted = false;
      item.service_id = service_id;

      item.schema_path = "file"s + service_id.to_string() + "schema"s +
                         entry.schema_id_.to_string();
      item.content_set_id = entry.schema_id_;
      item.file_path = "object"s + entry.obj_id_.to_string();
      item.id = entry.obj_id_;
      result.push_back(item);
    }
  }

  static auto create_testing_data(mrs::UniversalId service_id,
                                  const std::vector<EntryId> &ids) {
    std::vector<T> result;

    create_testing_objects(result, service_id, ids);
    return result;
  }

  StrictMock<MockRouteFactory> mock_route_factory_;
  StrictMock<MockAuthManager> mock_auth_manager_;
  StrictMock<MockMysqlCacheManager> mock_mysqlcache_;
  std::unique_ptr<mrs::ObjectManager> sut_;
};

using MyTypes = ::testing::Types<DbObject, ContentFile>;
TYPED_TEST_SUITE(RouteManagerTests, MyTypes);

TYPED_TEST(RouteManagerTests, turnon_on_empty_does_nothing) {
  this->sut_->turn(mrs::stateOn, {});
}

TYPED_TEST(RouteManagerTests, notexisting_schema_does_noting) {
  StrictMock<MockRouteSchema> schema;
  EXPECT_CALL(schema, get_full_path()).WillOnce(Return("Schema1"));
  this->sut_->schema_not_used(&schema);
}

TYPED_TEST(RouteManagerTests, db_object_two_routes_with_the_same_schema) {
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  auto objs = this->create_testing_data(
      k_service_id, {{k_schema_id, {1}}, {k_schema_id, {2}}});
  StrictMock<MockRouteSchema> schema;
  EXPECT_CALL(schema, get_name())
      .WillRepeatedly(ReturnRef(objs[0].schema_path));
  StrictMock<MockRoute> route1;
  StrictMock<MockRoute> route2;

  this->expect_create_schema(schema, objs[0]);
  this->expect_create(route1, objs[0]);
  this->expect_create(route2, objs[1]);

  EXPECT_CALL(schema, turn(mrs::stateOff));
  EXPECT_CALL(route1, turn(mrs::stateOff));
  EXPECT_CALL(route2, turn(mrs::stateOff));

  this->sut_->update(objs);
}

TYPED_TEST(RouteManagerTests, db_object_two_routes_with_different_schemas) {
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema1_id{2};
  const mrs::UniversalId k_schema2_id{3};
  const mrs::UniversalId k_object1_id{1};
  const mrs::UniversalId k_object2_id{2};
  auto objs = this->create_testing_data(
      k_service_id,
      {{k_schema1_id, k_object1_id}, {k_schema2_id, k_object2_id}});
  StrictMock<MockRouteSchema> schema1;
  StrictMock<MockRouteSchema> schema2;
  EXPECT_CALL(schema1, get_name())
      .WillRepeatedly(ReturnRef(objs[0].schema_path));
  EXPECT_CALL(schema2, get_name())
      .WillRepeatedly(ReturnRef(objs[1].schema_path));
  StrictMock<MockRoute> route1;
  StrictMock<MockRoute> route2;

  this->expect_create_schema(schema1, objs[0]);
  this->expect_create_schema(schema2, objs[1]);
  this->expect_create(route1, objs[0]);
  this->expect_create(route2, objs[1]);

  EXPECT_CALL(schema1, turn(mrs::stateOff));
  EXPECT_CALL(schema2, turn(mrs::stateOff));
  EXPECT_CALL(route1, turn(mrs::stateOff));
  EXPECT_CALL(route2, turn(mrs::stateOff));

  this->sut_->update(objs);
}

TYPED_TEST(RouteManagerTests, db_object_verify_destruction) {
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  const mrs::UniversalId k_object_id{1};
  auto objs =
      this->create_testing_data(k_service_id, {{k_schema_id, k_object_id}});
  StrictMock<MockRouteSchema> schema;
  EXPECT_CALL(schema, get_name())
      .WillRepeatedly(ReturnRef(objs[0].schema_path));
  StrictMock<MockRoute> route1;

  this->expect_create_schema(schema, objs[0], true);
  this->expect_create(route1, objs[0], true);

  EXPECT_CALL(schema, turn(_));
  EXPECT_CALL(route1, turn(_));

  this->sut_->update(objs);
  this->verifyAndClearMocks({&route1, &schema});

  EXPECT_CALL(route1, destroy());
  EXPECT_CALL(schema, destroy());
  this->sut_.reset();
}

TYPED_TEST(RouteManagerTests, db_object_by_default_disabled) {
  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  const mrs::UniversalId k_object_id{1};
  auto objs =
      this->create_testing_data(k_service_id, {{k_schema_id, k_object_id}});
  StrictMock<MockRouteSchema> schema;
  EXPECT_CALL(schema, get_name())
      .WillRepeatedly(ReturnRef(objs[0].schema_path));
  StrictMock<MockRoute> route1;

  this->expect_create_schema(schema, objs[0]);
  this->expect_create(route1, objs[0]);

  EXPECT_CALL(schema, turn(mrs::stateOff));
  EXPECT_CALL(route1, turn(mrs::stateOff));

  this->sut_->update(objs);
  this->verifyAndClearMocks({&route1, &schema});
}

TYPED_TEST(RouteManagerTests, db_object_enabled_before_start) {
  this->sut_->turn(mrs::stateOn, {});

  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  const mrs::UniversalId k_object_id{1};
  auto objs =
      this->create_testing_data(k_service_id, {{k_schema_id, k_object_id}});
  StrictMock<MockRouteSchema> schema;
  EXPECT_CALL(schema, get_name())
      .WillRepeatedly(ReturnRef(objs[0].schema_path));
  StrictMock<MockRoute> route1;

  this->expect_create_schema(schema, objs[0]);
  this->expect_create(route1, objs[0]);

  EXPECT_CALL(schema, turn(mrs::stateOn));
  EXPECT_CALL(route1, turn(mrs::stateOn));

  this->sut_->update(objs);
}

TYPED_TEST(RouteManagerTests, db_object_update_two_times_same_object) {
  this->sut_->turn(mrs::stateOn, {});

  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  const mrs::UniversalId k_object_id{1};
  auto objs =
      this->create_testing_data(k_service_id, {{k_schema_id, k_object_id}});
  StrictMock<MockRouteSchema> schema;
  EXPECT_CALL(schema, get_name())
      .WillRepeatedly(ReturnRef(objs[0].schema_path));
  StrictMock<MockRoute> route1;

  this->expect_create_schema(schema, objs[0]);
  this->expect_create(route1, objs[0]);

  EXPECT_CALL(schema, turn(mrs::stateOn));
  EXPECT_CALL(route1, turn(mrs::stateOn));

  this->sut_->update(objs);

  this->verifyAndClearMocks({&route1, &schema});
  objs[0].requires_authentication = !objs[0].requires_authentication;

  EXPECT_CALL(route1,
              update(DbObjectById(mrs::UniversalId{1}), EqSmartPtr(&schema)));
  EXPECT_CALL(route1, turn(mrs::stateOn));
  this->sut_->update(objs);
}

TYPED_TEST(RouteManagerTests, db_object_update_two_times_schema_changes_name) {
  this->sut_->turn(mrs::stateOn, {});

  const mrs::UniversalId k_service_id{1};
  const mrs::UniversalId k_schema_id{2};
  const mrs::UniversalId k_object_id{1};
  auto objs =
      this->create_testing_data(k_service_id, {{k_schema_id, k_object_id}});
  const auto k_old_schema_name = objs[0].schema_path;
  StrictMock<MockRouteSchema> schema_old;
  StrictMock<MockRouteSchema> schema_new;
  EXPECT_CALL(schema_old, get_name())
      .WillRepeatedly(ReturnRef(k_old_schema_name));
  EXPECT_CALL(schema_old, get_full_path())
      .WillRepeatedly(Return(k_old_schema_name));
  StrictMock<MockRoute> route1;

  this->expect_create_schema(schema_old, objs[0], true);
  this->expect_create(route1, objs[0]);

  EXPECT_CALL(schema_old, turn(mrs::stateOn));
  EXPECT_CALL(route1, turn(mrs::stateOn));

  this->sut_->update(objs);
  this->verifyAndClearMocks({&route1, &schema_old});

  objs[0].schema_path = "new_path";
  objs[0].schema_path = "new_schema";

  this->expect_create_schema(schema_new, objs[0]);
  EXPECT_CALL(schema_old, get_name())
      .WillRepeatedly(ReturnRef(k_old_schema_name));
  EXPECT_CALL(schema_old, get_full_path())
      .WillRepeatedly(Return(k_old_schema_name));
  // In case when "update" method receives new schema, it must remove inform
  // manager that it removed old schema
  EXPECT_CALL(route1, update(DbObjectById(mrs::UniversalId{1}),
                             EqSmartPtr(&schema_new)))
      .WillOnce(InvokeWithoutArgs([this, &schema_old]() {
        this->sut_->schema_not_used(&schema_old);
        return true;
      }));
  EXPECT_CALL(route1, turn(mrs::stateOn));
  EXPECT_CALL(schema_new, turn(mrs::stateOn));
  EXPECT_CALL(schema_new, get_name())
      .WillRepeatedly(ReturnRef(objs[0].schema_path));
  EXPECT_CALL(schema_old, destroy());

  this->sut_->update(objs);
  this->verifyAndClearMocks({&route1, &schema_old});

  this->sut_.reset();
}
