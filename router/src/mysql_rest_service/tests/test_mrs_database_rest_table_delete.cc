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
#include <vector>

#include "helper/expect_throw_msg.h"
#include "mock/mock_session.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/query_rest_table_updater.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;

// XXX Corner cases
// - self-referencing FKs
//  - re.: You cannot delete from a table and select from
// the same table in a subquery.

class DatabaseQueryDelete : public DatabaseRestTableTest {
 public:
  void test_delete(std::shared_ptr<DualityView> root,
                   const PrimaryKeyColumnValues &pk,
                   const ObjectRowOwnership &row_owner = {}) {
    mrs::database::dv::DualityViewUpdater rest(root, row_owner);

    rest.delete_(m_.get(), pk);
  }

  void test_delete_f(std::shared_ptr<DualityView> root,
                     const std::string &filter,
                     const ObjectRowOwnership &row_owner = {}) {
    FilterObjectGenerator fog(root, true, 0);
    fog.parse(helper::json::text_to_document(filter));

    mrs::database::dv::DualityViewUpdater rest(root, row_owner);

    rest.delete_(m_.get(), fog);
  }
};

TEST_F(DatabaseQueryDelete, no_pk) {
  auto root = DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_DELETE)
                  .field("country_id", FieldFlag::PRIMARY)
                  .field("country")
                  .resolve(m_.get(), true);

  {
    EXPECT_REST_ERROR(test_delete(root, {}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(test_delete(root, {{"country", "Testland"}}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(test_delete(root, {{"bogus_id", "111"}}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(
        test_delete(root, {{"country_id", "1"}, {"bogus_id", "111"}}),
        "Invalid primary key column");
  }

  EXPECT_NO_CHANGES();
}

TEST_F(DatabaseQueryDelete, no_pk_multi) {
  auto root = DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_DELETE)
                  .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("continent_id", FieldFlag::PRIMARY)
                  .field("country")
                  .resolve();

  {
    EXPECT_REST_ERROR(test_delete(root, {}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(test_delete(root, {{"country_id", "111"}}),
                      "Missing primary key column value");
  }

  EXPECT_NO_CHANGES();
}

TEST_F(DatabaseQueryDelete, plain_multi) {
  auto root =
      DualityViewBuilder("mrstestdb", "tc2_base", TableFlag::WITH_DELETE)
          .field("ID", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("SUBID", "sub_id", "char(2)", FieldFlag::PRIMARY)
          .field("firstName", "data1", "text")
          .field("count", "data2", "int")
          .resolve(m_.get(), true);

  test_delete(root, {{"id", "4"}, {"sub_id", "'AA'"}});
  EXPECT_ROWS_ADDED("tc2_base", -1);
  EXPECT_ROWS_ADDED("tc2_ref_11", 0);
  EXPECT_ROWS_ADDED("tc2_ref_1n", 0);
  EXPECT_ROWS_ADDED("tc2_ref_nm_join", 0);

  // partial - TODO(spec) - should this succeed or fail?
  EXPECT_REST_ERROR(test_delete(root, {{"sub_id", "'AA'"}}),
                    "Missing primary key column value");

  EXPECT_REST_ERROR(test_delete(root, {{"id", "3"}}),
                    "Missing primary key column value");

  EXPECT_ROWS_ADDED("tc2_base", -1);  // still -1
}

TEST_F(DatabaseQueryDelete, plain_nodelete) {
  auto root = DualityViewBuilder("mrstestdb", "actor", TableFlag::WITH_INSERT)
                  .field("actorId", "actor_id", "int",
                         FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("firstName", "first_name", "text")
                  .field("lastName", "last_name", "text")
                  .resolve(m_.get(), true);

  EXPECT_DUALITY_ERROR(test_delete(root, {{"actor_id", "111"}}),
                       "Duality View does not allow DELETE for table `actor`");

  EXPECT_ROWS_ADDED("actor", 0);
}

TEST_F(DatabaseQueryDelete, plain_owner_notpk) {
  prepare(TestSchema::PLAIN);

  {
    auto root =
        DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("Id", "id", "int", FieldFlag::PRIMARY)
            .field("owner_id", FieldFlag::OWNER)
            .field("data1")
            .field("data2")
            .field_to_many(
                "1n",
                ViewBuilder("child_1n", TableFlag::WITH_DELETE).field("id"))
            .resolve(m_.get(), true);

    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("0x33330000000000000000000000000000"));

    // owned
    snapshot();
    test_delete(root, {{"id", "4"}}, owner);
    EXPECT_ROWS_ADDED("root", -1);
    EXPECT_ROWS_ADDED("child_1n", -1);
    EXPECT_ROWS_ADDED("child_11", 0);

    snapshot();
    // owned by someone else
    test_delete(root, {{"id", "1"}}, owner);
    EXPECT_ROWS_ADDED("root", 0);
    EXPECT_ROWS_ADDED("child_1n", 0);
    EXPECT_ROWS_ADDED("child_11", 0);
  }
}

TEST_F(DatabaseQueryDelete, plain_owner_pk) {
  prepare(TestSchema::PLAIN);

  // pk = owner
  m_->execute(R"*(INSERT INTO mrstestdb.root_owner (id, data1) VALUES
   (0x10000000000000000000000000000000, 'one'),
   (0x20000000000000000000000000000000, 'two'),
   (0x30000000000000000000000000000000, 'three'))*");

  {
    auto root =
        DualityViewBuilder("mrstestdb", "root_owner", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::PRIMARY | FieldFlag::OWNER)
            .field("data1", "data1")
            .field_to_one("11",
                          ViewBuilder("child_11").field("id").field("data"))
            .resolve(m_.get(), true);
    {
      auto owner = ObjectRowOwnership(
          root, "id",
          mysqlrouter::sqlstring("0x10000000000000000000000000000000"));

      snapshot();
      // doc with wrong id
      test_delete(root, {{"id", "0x20000000000000000000000000000000"}}, owner);
      EXPECT_ROWS_ADDED("root_owner", -1);
      auto r = select_one(root, {{"id", "0x20000000000000000000000000000000"}});
      EXPECT_TRUE(!r.empty());
    }

    {
      auto owner = ObjectRowOwnership(
          root, "id",
          mysqlrouter::sqlstring("0x20000000000000000000000000000000"));

      snapshot();
      // correct
      test_delete(root, {{"id", "0x20000000000000000000000000000000"}}, owner);
      EXPECT_ROWS_ADDED("root_owner", -1);
    }

    {
      auto owner = ObjectRowOwnership(
          root, "id",
          mysqlrouter::sqlstring("0x30000000000000000000000000000000"));

      snapshot();
      // omitted
      test_delete(root, {}, owner);
      EXPECT_ROWS_ADDED("root_owner", -1);
    }
  }
}

TEST_F(DatabaseQueryDelete, nested_1n) {
  auto root = DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_DELETE)
                  .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("country")
                  .field_to_many("cities",
                                 ViewBuilder("city")
                                     .field("city_id", FieldFlag::PRIMARY |
                                                           FieldFlag::AUTO_INC)
                                     .field("country_id")
                                     .field("city"))
                  .resolve(m_.get(), true);

  { test_delete(root, {{"country_id", "222"}}); }
}

TEST_F(DatabaseQueryDelete, nested_1n_nodelete) {
  auto root = DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_DELETE)
                  .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("country")
                  .field_to_many("cities",
                                 ViewBuilder("city", 0)
                                     .field("city_id", FieldFlag::PRIMARY |
                                                           FieldFlag::AUTO_INC)
                                     .field("country_id")
                                     .field("city"))
                  .resolve(m_.get(), true);

  // nested list is empty
  { test_delete(root, {{"country_id", "333"}}); }
}

TEST_F(DatabaseQueryDelete, filter_plain) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor", TableFlag::WITH_DELETE)
          .field("actorId", "actor_id", "int",
                 FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("firstName", "first_name", "text", FieldFlag::WITH_FILTERING)
          .field("lastName", "last_name", "text")
          .field_to_many("films",
                         ViewBuilder("film_actor", TableFlag::WITH_DELETE)
                             .field("actor_id")
                             .field("film_id"))
          .resolve(m_.get(), true);

  EXPECT_NO_THROW(test_delete_f(root, R"*({"firstName": "Joe"})*"));

  EXPECT_REST_ERROR(test_delete_f(root, R"*({"lastName": "Joe"})*"),
                    "Cannot filter on field lastName");
}

TEST_F(DatabaseQueryDelete, filter_plain_row_owner_notpk) {
  prepare(TestSchema::PLAIN);

  auto root =
      DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
          .field("ID", "id")
          .field("owner_id", FieldFlag::WITH_FILTERING | FieldFlag::OWNER)
          .field("data1", FieldFlag::WITH_FILTERING)
          .field("data2", FieldFlag::WITH_FILTERING)
          .field_to_many(
              "1n", ViewBuilder("child_1n", TableFlag::WITH_DELETE).field("id"))
          .resolve(m_.get(), true);

  {
    snapshot();
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    test_delete_f(root, R"*({"data1": "data2", "data2": 2})*", owner);

    EXPECT_ROWS_ADDED("root", -1);
    EXPECT_ROWS_ADDED("child_1n", 0);
  }
  // try to delete someone else's row
  {
    snapshot();
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    test_delete_f(root, R"*({"owner_id": "IiIAAAAAAAAAAAAAAAAAAA=="})*", owner);
    EXPECT_ROWS_ADDED("root", 0);
  }
  // allow deleting own row
  {
    snapshot();
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('MzMAAAAAAAAAAAAAAAAAAA==')"));

    test_delete_f(
        root, R"*({"owner_id": "MzMAAAAAAAAAAAAAAAAAAA==", "data1": "data4"})*",
        owner);
    EXPECT_ROWS_ADDED("root", -1);
  }
  // allow deleting all of own rows
  {
    snapshot();
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('IiIAAAAAAAAAAAAAAAAAAA==')"));

    test_delete_f(root, R"*({"owner_id": "IiIAAAAAAAAAAAAAAAAAAA=="})*", owner);
    EXPECT_ROWS_ADDED("root", -2);
  }
}

TEST_F(DatabaseQueryDelete, filter_plain_row_owner_pk) {
  prepare(TestSchema::PLAIN);

  auto root_pkowner =
      DualityViewBuilder("mrstestdb", "root_owner", TableFlag::WITH_DELETE)
          .field("ID", "id", "int", FieldFlag::OWNER)
          .field("data1", FieldFlag::WITH_FILTERING)
          .resolve(m_.get(), true);

  m_->execute(R"*(INSERT INTO mrstestdb.root_owner (id, data1) VALUES
   (0x11110000000000000000000000000000, 'one'),
   (0x22220000000000000000000000000000, 'two'),
   (0x33330000000000000000000000000000, 'three'))*");

  // owner_id = PK
  {
    snapshot();
    auto owner = ObjectRowOwnership(
        root_pkowner, "id",
        mysqlrouter::sqlstring("FROM_BASE64('IiIAAAAAAAAAAAAAAAAAAA==')"));

    test_delete_f(root_pkowner, R"*({"data1": "two"})*", owner);
    EXPECT_ROWS_ADDED("root_owner", -1);
  }
  // can't delete someone else's row
  {
    snapshot();
    auto owner = ObjectRowOwnership(
        root_pkowner, "id",
        mysqlrouter::sqlstring("FROM_BASE64('MzMAAAAAAAAAAAAAAAAAAA==')"));

    test_delete_f(root_pkowner, R"*({"data1":"one"})*", owner);
    EXPECT_ROWS_ADDED("root_owner", 0);
  }
  // allow deleting own row
  {
    snapshot();
    auto owner = ObjectRowOwnership(
        root_pkowner, "id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    test_delete_f(root_pkowner,
                  R"*({"ID": "EREAAAAAAAAAAAAAAAAAAA==", "data1": "one"})*",
                  owner);
    EXPECT_ROWS_ADDED("root_owner", -1);
  }
}

TEST_F(DatabaseQueryDelete, filter_nested_nm) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor", TableFlag::WITH_DELETE)
          .field("actor_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("first_name", FieldFlag::WITH_FILTERING)
          .field("last_name", FieldFlag::WITH_FILTERING)
          .field_to_many(
              "film_actor",
              ViewBuilder("film_actor", TableFlag::WITH_DELETE)
                  .field("actor_id", FieldFlag::PRIMARY)
                  .field("film_id", FieldFlag::PRIMARY)
                  .field_to_one("film",
                                ViewBuilder("film", 0)
                                    .field("film_id", FieldFlag::PRIMARY |
                                                          FieldFlag::AUTO_INC)
                                    .field("title")))
          .resolve(m_.get(), true);

  test_delete_f(root, R"*({"first_name": "JOE", "last_name": "SWANK"})*");

  EXPECT_ROWS_ADDED("actor", -1);
  EXPECT_ROWS_ADDED("film_actor", -2);
  EXPECT_ROWS_ADDED("film", 0);
}

TEST_F(DatabaseQueryDelete, filter_nested_nm_row_owner_notpk) {
  prepare(TestSchema::PLAIN);

  m_->execute(
      R"*(INSERT INTO mrstestdb.child_nm_join VALUES (1, 1), (2, 2), (1, 3), (5,1), (5,2))*");

  auto root =
      DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
          .field("ID", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("owner_id", FieldFlag::OWNER)
          .field("data1", FieldFlag::WITH_FILTERING)
          .field("data2", FieldFlag::WITH_FILTERING)
          .field_to_many(
              "1n", ViewBuilder("child_1n", TableFlag::WITH_DELETE).field("id"))
          .field_to_many(
              "nm",
              ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                  .field("root_id", FieldFlag::PRIMARY)
                  .field("child_id", FieldFlag::PRIMARY)
                  .field_to_one("child", ViewBuilder("child_nm", 0)
                                             .field("id", FieldFlag::PRIMARY)
                                             .field("data")))
          .resolve(m_.get(), true);

  // owned
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    snapshot();

    test_delete_f(root, R"*({"data1":"data5"})*", owner);

    EXPECT_ROWS_ADDED("root", -1);
    EXPECT_ROWS_ADDED("child_1n", 0);
    EXPECT_ROWS_ADDED("child_nm_join", -2);
    EXPECT_ROWS_ADDED("child_nm", 0);
  }

  // not owned
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    snapshot();

    test_delete_f(root, R"*({"data1":"data2"})*", owner);

    EXPECT_ROWS_ADDED("root", -1);
    EXPECT_ROWS_ADDED("child_1n", 0);
    EXPECT_ROWS_ADDED("child_nm_join", -1);
    EXPECT_ROWS_ADDED("child_nm", 0);
  }
}

TEST_F(DatabaseQueryDelete, filter_nested_nm_row_owner_pk) {
  prepare(TestSchema::PLAIN);

  m_->execute(R"*(INSERT INTO mrstestdb.root_owner (id, data1) VALUES
   (0x11110000000000000000000000000000, 'one'),
   (0x22220000000000000000000000000000, 'two'),
   (0x33330000000000000000000000000000, 'three'))*");

  m_->execute(
      R"*(INSERT INTO mrstestdb.child_nm_join2 VALUES 
      (0x11110000000000000000000000000000, 1),
      (0x22220000000000000000000000000000, 2),
      (0x11110000000000000000000000000000, 3),
      (0x33330000000000000000000000000000, 1),
      (0x33330000000000000000000000000000, 2))*");

  auto root =
      DualityViewBuilder("mrstestdb", "root_owner", TableFlag::WITH_DELETE)
          .field("ID", "id", FieldFlag::OWNER)
          .field("data1", FieldFlag::WITH_FILTERING)
          .field("data2", FieldFlag::WITH_FILTERING)
          .field_to_many(
              "nm",
              ViewBuilder("child_nm_join2", TableFlag::WITH_DELETE)
                  .field("root_id", FieldFlag::PRIMARY)
                  .field("child_id", FieldFlag::PRIMARY)
                  .field_to_one("child", ViewBuilder("child_nm", 0)
                                             .field("id", FieldFlag::PRIMARY)
                                             .field("data")))
          .resolve(m_.get(), true);

  // row_owner = PK
  {
    snapshot();

    auto owner = ObjectRowOwnership(
        root, "ID",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    test_delete_f(root, R"*({"data1": "two"})*", owner);
    EXPECT_ROWS_ADDED("root_owner", 0);
    EXPECT_ROWS_ADDED("child_nm_join", 0);
    EXPECT_ROWS_ADDED("child_nm", 0);

    test_delete_f(root, R"*({"data1": "one"})*", owner);
    EXPECT_ROWS_ADDED("root_owner", -1);
    EXPECT_ROWS_ADDED("child_nm_join", 0);
    EXPECT_ROWS_ADDED("child_nm", 0);
  }
}
