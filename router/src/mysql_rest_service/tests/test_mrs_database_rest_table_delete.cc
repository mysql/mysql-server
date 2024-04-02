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
  void test_delete(std::shared_ptr<entry::Object> root,
                   const PrimaryKeyColumnValues &pk,
                   const ObjectRowOwnership &row_owner = {}) {
    mrs::database::TableUpdater rest(root, row_owner);

    rest.handle_delete(m_.get(), pk);
  }

  void test_delete_f(std::shared_ptr<Object> root, const std::string &filter,
                     const ObjectRowOwnership &row_owner = {}) {
    FilterObjectGenerator fog(root, true, 0);
    fog.parse(helper::json::text_to_document(filter));

    mrs::database::TableUpdater rest(root, row_owner);

    rest.handle_delete(m_.get(), fog);
  }
};

TEST_F(DatabaseQueryDelete, no_pk) {
  auto root = ObjectBuilder("mrstestdb", "country")
                  .field("country_id", FieldFlag::PRIMARY)
                  .field("country")
                  .root();

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
  auto root = ObjectBuilder("mrstestdb", "country")
                  .field("country_id", FieldFlag::PRIMARY)
                  .field("continent_id", FieldFlag::PRIMARY)
                  .field("country")
                  .root();

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

TEST_F(DatabaseQueryDelete, plain) {
  auto root = ObjectBuilder("mrstestdb", "city")
                  .field("CityID", "city_id", "int",
                         FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("Name", "city", "text")
                  .root();

  test_delete(root, {{"city_id", "128"}});

  EXPECT_ROWS_ADDED("city", -1);
}

TEST_F(DatabaseQueryDelete, plain_multi) {
  auto root =
      ObjectBuilder("mrstestdb", "tc2_base")
          .field("ID", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("SUBID", "sub_id", "char(2)", FieldFlag::PRIMARY)
          .field("firstName", "data1", "text")
          .field("count", "data2", "int")
          .root();

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
  auto root = ObjectBuilder("mrstestdb", "actor", kNoDelete)
                  .field("actorId", "actor_id", "int",
                         FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("firstName", "first_name", "text")
                  .field("lastName", "last_name", "text")
                  .root();

  test_delete(root, {{"actor_id", "111"}});

  EXPECT_ROWS_ADDED("actor", 0);
}

TEST_F(DatabaseQueryDelete, plain_autoinc_row_owner) {
  {
    auto root =
        ObjectBuilder("mrstestdb", "t2_base")
            .field("Id", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
            .field("owner_id")
            .field("data1")
            .field("data2")
            .root();

    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("222"));

    test_delete(root, {{"id", "3"}}, owner);

    EXPECT_ROWS_ADDED("t2_base", -1);
  }
  // pk = owner
  {
    auto root =
        ObjectBuilder("mrstestdb", "t1_owner")
            .field("id", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
            .field("user", "data", "text")
            .root();
    {
      auto owner = ObjectRowOwnership(
          root->get_base_table(), "id",
          mysqlrouter::sqlstring("0x75756964310000000000000000000000"));

      // doc with wrong id
      test_delete(root, {{"id", "0x75756964320000000000000000000000"}}, owner);

      EXPECT_ROWS_ADDED("t1_owner", 0);
      auto row = m_->query_one(
          "SELECT * FROM mrstestdb.t1_owner WHERE "
          "id=0x75756964310000000000000000000000");
      EXPECT_TRUE(row);
      EXPECT_STREQ("one", (*row)[1]);
    }
    // implicit
    {
      auto owner = ObjectRowOwnership(
          root->get_base_table(), "id",
          mysqlrouter::sqlstring("0x75756964310000000000000000000000"));

      test_delete(root, {}, owner);

      EXPECT_ROWS_ADDED("t1_owner", -1);

      auto row = m_->query_one(
          "SELECT * FROM mrstestdb.t1_owner WHERE "
          "id=0x75756964310000000000000000000000");
      EXPECT_FALSE(row);
    }
  }
}

TEST_F(DatabaseQueryDelete, nested_1n) {
  auto root =
      ObjectBuilder("mrstestdb", "country")
          .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("country")
          .nest_list(
              "cities",
              ObjectBuilder("city", {{"country_id", "country_id"}})
                  .field("city_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("country_id")
                  .field("city"))
          .root();

  { test_delete(root, {{"country_id", "222"}}); }
}

TEST_F(DatabaseQueryDelete, nested_1n_nodelete) {
  auto root =
      ObjectBuilder("mrstestdb", "country")
          .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("country")
          .nest_list(
              "cities",
              ObjectBuilder("city", {{"country_id", "country_id"}}, kNoDelete)
                  .field("city_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("country_id")
                  .field("city"))
          .root();

  // nested list is empty
  { test_delete(root, {{"country_id", "333"}}); }
}

TEST_F(DatabaseQueryDelete, nested_1n_1n) {
  auto root =
      ObjectBuilder("mrstestdb", "t2_base")
          .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("data")
          .nest_list(
              "refs",
              ObjectBuilder("t2_ref_1n", {{"base_id", "id"}})
                  .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("base_id")
                  .field("data")
                  .nest_list("subrefs", ObjectBuilder("t2_ref_1n_1n",
                                                      {{"ref_1n_id", "id"}})
                                            .column("ref_1n_id")
                                            .field("id", FieldFlag::PRIMARY)
                                            .field("data")))
          .root();

  m_->execute(
      "INSERT INTO t2_base VALUES (111, NULL, NULL, '', 1), (222, NULL, NULL, "
      "'', 2)");
  m_->execute(
      "INSERT INTO t2_ref_1n VALUES (1, 'data1', 111), (2, 'data2', 111), "
      "(3, 'data3', 222)");
  m_->execute(
      "INSERT INTO t2_ref_1n_1n VALUES (1, 'subdata1', 1), (2, 'subdata2', 1), "
      "(3, 'subdata3', 2), (4, 'subdata4', 3)");

  snapshot();

  test_delete(root, {{"id", "111"}});

  EXPECT_ROWS_ADDED("t2_base", -1);
  EXPECT_ROWS_ADDED("t2_ref_1n", -2);
  EXPECT_ROWS_ADDED("t2_ref_1n_1n", -3);
}

TEST_F(DatabaseQueryDelete, nested_11) {
  auto root = ObjectBuilder("mrstestdb", "city")
                  .field("city_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("city")
                  .column("country_id")
                  .nest("country",
                        ObjectBuilder("country", {{"country_id", "country_id"}},
                                      Operation::valueRead)
                            .field("country_id",
                                   FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                            .field("country"))
                  .root();

  test_delete(root, {{"city_id", "20"}});

  EXPECT_ROWS_ADDED("city", -1);
}

TEST_F(DatabaseQueryDelete, nested_11_11) {
  auto root =
      ObjectBuilder("mrstestdb", "t2_base")
          .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("data1")
          .column("ref_11_id")
          .nest(
              "ref",
              ObjectBuilder("t2_ref_11", {{"ref_11_id", "id"}})
                  .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("data")
                  .column("ref_id")
                  .nest("ref", ObjectBuilder("t2_ref_11_11", {{"ref_id", "id"}})
                                   .field("id", FieldFlag::PRIMARY)
                                   .field("data")))
          .root();

  test_delete(root, {{"id", "7"}});
  EXPECT_ROWS_ADDED("t2_base", -1);
  EXPECT_ROWS_ADDED("t2_ref_11", 0);
  EXPECT_ROWS_ADDED("t2_ref_11_11", 0);

  snapshot();
  test_delete(root, {{"id", "9"}});

  EXPECT_ROWS_ADDED("t2_base", -1);
  EXPECT_ROWS_ADDED("t2_ref_11", -1);
  EXPECT_ROWS_ADDED("t2_ref_11_11", -1);
}

// same as n:1
TEST_F(DatabaseQueryDelete, nested_11_nodelete) {
  auto root = ObjectBuilder("mrstestdb", "film")
                  .field("film_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("title")
                  .column("original_language_id")
                  .nest("original_language",
                        ObjectBuilder("language",
                                      {{"original_language_id", "language_id"}},
                                      kNoDelete)
                            .field("language_id",
                                   FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                            .field("name"))
                  .root();

  {
    test_delete(root, {{"film_id", "14"}});

    EXPECT_ROWS_ADDED("film", -1);
    EXPECT_ROWS_ADDED("language", 0);
  }
}

TEST_F(DatabaseQueryDelete, unnested_11_owned_child_autoinc) {
  auto root =
      ObjectBuilder("mrstestdb", "city")
          .field("city_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("city")
          .column("country_id")
          .unnest(
              ObjectBuilder("country", {{"country_id", "country_id"}})
                  .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("country"))
          .root();
  {
    test_delete(root, {{"city_id", "251"}});
    EXPECT_ROWS_ADDED("city", -1);
    EXPECT_ROWS_ADDED("country", -1);
  }
}

TEST_F(DatabaseQueryDelete, unnested_11_ref_child_autoinc) {
  // 2 refs to the same table

  auto root =
      ObjectBuilder("mrstestdb", "city")
          .field("city_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("city")
          .column("country_id")
          .unnest(
              ObjectBuilder("country", {{"country_id", "country_id"}})
                  .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("country"))
          .root();
  {
    test_delete(root, {{"city_id", "251"}});
    EXPECT_ROWS_ADDED("city", -1);
    EXPECT_ROWS_ADDED("country", -1);
  }
}

TEST_F(DatabaseQueryDelete, nested_11_multi) {
  auto root =
      ObjectBuilder("mrstestdb", "tc2_base")
          .field("id", FieldFlag::PRIMARY)
          .field("sub_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("data1")
          .field("data2")
          .column("ref_11_id")
          .column("ref_11_sub_id")
          .nest("ref",
                ObjectBuilder("tc2_ref_11", {{"ref_11_id", "id"},
                                             {"ref_11_sub_id", "sub_id"}})
                    .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                    .field("sub_id", FieldFlag::PRIMARY)
                    .field("data"))
          .root();

  {
    test_delete(root, {{"id", "6"}, {"sub_id", "'AA'"}});

    EXPECT_ROWS_ADDED("tc2_base", -1);
    EXPECT_ROWS_ADDED("tc2_ref_11", -1);
  }
}

TEST_F(DatabaseQueryDelete, nested_nm) {
  // rows in film are assumed to be owned by actor, hence films aren't shared.
  // otherwise, film would be marked no-delete

  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .field("actor_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("first_name")
          .field("last_name")
          .nest_list(
              "film_actor",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id", FieldFlag::PRIMARY)
                  .column("film_id", FieldFlag::PRIMARY)
                  .nest("film", ObjectBuilder("film", {{"film_id", "film_id"}})
                                    .field("film_id", FieldFlag::PRIMARY |
                                                          FieldFlag::AUTO_INC)
                                    .field("title")))
          .root();

  {
    // actor 11 only appears in movies alone
    test_delete(root, {{"actor_id", "11"}});
    EXPECT_ROWS_ADDED("actor", -1);
    EXPECT_ROWS_ADDED("film_actor", -3);
    EXPECT_ROWS_ADDED("film", -3);
  }
}

// nm and 1n with composite key
TEST_F(DatabaseQueryDelete, nested_nm_ref_1n_multi) {
  // also tests differently named FK columns
  auto root =
      ObjectBuilder("mrstestdb", "tc2_base")
          .field("ID", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("sub_id", FieldFlag::PRIMARY)
          .field("data1")
          .field("data2")
          .nest_list("refs1",
                     ObjectBuilder("tc2_ref_1n", {{"base_id", "id"},
                                                  {"base_sub_id", "sub_id"}})
                         .column("id", FieldFlag::PRIMARY)
                         .column("sub_id", FieldFlag::PRIMARY)
                         .column("base_id", FieldFlag::PRIMARY)
                         .column("base_sub_id", FieldFlag::PRIMARY)
                         .field("data"))
          .nest_list(
              "refs2",
              ObjectBuilder("tc2_ref_nm_join",
                            {{"base_id", "id"}, {"base_sub_id", "sub_id"}})
                  .column("base_id", FieldFlag::PRIMARY)
                  .column("base_sub_id", FieldFlag::PRIMARY)
                  .column("ref_id", FieldFlag::PRIMARY)
                  .column("ref_sub_id", FieldFlag::PRIMARY)
                  .nest("ref", ObjectBuilder(
                                   "tc2_ref_nm",
                                   {{"ref_id", "id"}, {"ref_sub_id", "sub_id"}},
                                   Operation::valueRead)
                                   .column("id", FieldFlag::PRIMARY)
                                   .column("sub_id", FieldFlag::PRIMARY)
                                   .field("data")))
          .root();

  { test_delete(root, {{"id", "1"}, {"sub_id", "'AA'"}}); }
}

TEST_F(DatabaseQueryDelete, nested_nm_ref_multi_sh) {
  // composite keys where part of the key is common to all (like in sharding)
  auto root =
      ObjectBuilder("mrstestdb", "ts2_base")
          .field("ID", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("shard_id", FieldFlag::PRIMARY)
          .field("data1")
          .field("data2")
          .nest_list("refs1",
                     ObjectBuilder("ts2_ref_1n", {{"base_id", "id"},
                                                  {"shard_id", "shard_id"}})
                         .column("id", FieldFlag::PRIMARY)
                         .column("shard_id", FieldFlag::PRIMARY)
                         .column("base_id", FieldFlag::PRIMARY)
                         .field("data"))
          .nest_list(
              "refs2",
              ObjectBuilder("ts2_ref_nm_join",
                            {{"base_id", "id"}, {"shard_id", "shard_id"}})
                  .column("shard_id", FieldFlag::PRIMARY)
                  .column("base_id", FieldFlag::PRIMARY)
                  .column("ref_id", FieldFlag::PRIMARY)
                  .nest("ref", ObjectBuilder(
                                   "ts2_ref_nm",
                                   {{"ref_id", "id"}, {"shard_id", "shard_id"}},
                                   Operation::valueRead)
                                   .column("id", FieldFlag::PRIMARY)
                                   .column("shard_id", FieldFlag::PRIMARY)
                                   .field("data")))
          .root();

  { test_delete(root, {{"id", "1"}, {"shard_id", "91"}}); }
}

// XXX TODO check PK that has boolean or blob

TEST_F(DatabaseQueryDelete, nested_nm_nodelete) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .field("actor_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("first_name")
          .field("last_name")
          .nest_list(
              "film_actor",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id", FieldFlag::PRIMARY)
                  .column("film_id", FieldFlag::PRIMARY)
                  .nest("film", ObjectBuilder("film", {{"film_id", "film_id"}},
                                              kNoDelete)
                                    .field("film_id", FieldFlag::PRIMARY |
                                                          FieldFlag::AUTO_INC)
                                    .field("title")))
          .root();

  { test_delete(root, {{"actor_id", "50"}}); }
}

TEST_F(DatabaseQueryDelete, filter_plain) {
  auto root = ObjectBuilder("mrstestdb", "actor")
                  .field("actorId", "actor_id", "int",
                         FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("firstName", "first_name", "text")
                  .field("lastName", "last_name", "text")
                  .root();

  test_delete_f(root, R"*({"firstName": "Joe", "lastName": "Smith"})*");
}

TEST_F(DatabaseQueryDelete, filter_plain_row_owner) {
  // delete REfs that don't matter for this test
  m_->execute("truncate mrstestdb.t2_ref_nm_join");

  auto root =
      ObjectBuilder("mrstestdb", "t2_base")
          .field("ID", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("owner_id")
          .field("data1")
          .field("data2")
          .root();

  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("111"));

    test_delete_f(root, R"*({"data1": "data2", "data2": 2})*", owner);

    EXPECT_ROWS_ADDED("t2_base", -1);
  }
  // try to delete someone else's row
  {
    snapshot();
    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("111"));

    test_delete_f(root, R"*({"owner_id": 222, "data1": "data1"})*", owner);
    EXPECT_ROWS_ADDED("t2_base", 0);
  }
  // allow deleting own row
  {
    snapshot();
    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("111"));

    test_delete_f(root, R"*({"owner_id": 111, "data1": "data5"})*", owner);
    EXPECT_ROWS_ADDED("t2_base", -1);
  }
  // allow deleting all of own rows
  {
    snapshot();
    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("222"));

    test_delete_f(root, R"*({"owner_id": 222})*", owner);
    EXPECT_ROWS_ADDED("t2_base", -2);
  }

  reset_test();
  m_->execute("truncate mrstestdb.t2_ref_nm_join");

  // owner_id = PK
  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "id",
                                    mysqlrouter::sqlstring("6"));

    test_delete_f(root, R"*({"data1": "data6"})*", owner);
    EXPECT_ROWS_ADDED("t2_base", -1);
  }
  // can't delete someone else's row
  {
    snapshot();
    auto owner = ObjectRowOwnership(root->get_base_table(), "id",
                                    mysqlrouter::sqlstring("6"));

    test_delete_f(root, R"*({"data1":"data5"})*", owner);
    EXPECT_ROWS_ADDED("t2_base", 0);
  }
  // allow deleting own row
  {
    snapshot();
    auto owner = ObjectRowOwnership(root->get_base_table(), "id",
                                    mysqlrouter::sqlstring("1"));

    test_delete_f(root, R"*({"ID": 1, "data1": "data1"})*", owner);
    EXPECT_ROWS_ADDED("t2_base", -1);
  }
  // allow deleting all of own rows
  {
    snapshot();
    auto owner = ObjectRowOwnership(root->get_base_table(), "id",
                                    mysqlrouter::sqlstring("4"));

    test_delete_f(root, R"*({"ID": 4})*", owner);
    EXPECT_ROWS_ADDED("t2_base", -1);
  }
}

TEST_F(DatabaseQueryDelete, filter_nested_nm) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .field("actor_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("first_name")
          .field("last_name")
          .nest_list(
              "film_actor",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id", FieldFlag::PRIMARY)
                  .column("film_id", FieldFlag::PRIMARY)
                  .nest("film", ObjectBuilder("film", {{"film_id", "film_id"}},
                                              Operation::valueRead)
                                    .field("film_id", FieldFlag::PRIMARY |
                                                          FieldFlag::AUTO_INC)
                                    .field("title")))
          .root();

  test_delete_f(root, R"*({"first_name": "JOE", "last_name": "SWANK"})*");

  EXPECT_ROWS_ADDED("actor", -1);
  EXPECT_ROWS_ADDED("film_actor", -2);
  EXPECT_ROWS_ADDED("film", 0);
}

TEST_F(DatabaseQueryDelete, filter_nested_nm_row_owner) {
  auto root =
      ObjectBuilder("mrstestdb", "t2_base")
          .field("ID", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("owner_id")
          .field("data1")
          .field("data2")
          .nest_list(
              "refs",
              ObjectBuilder("t2_ref_nm_join", {{"base_id", "id"}})
                  .column("base_id", FieldFlag::PRIMARY)
                  .column("ref_id", FieldFlag::PRIMARY)
                  .nest("ref", ObjectBuilder("t2_ref_nm", {{"ref_id", "id"}})
                                   .column("id", FieldFlag::PRIMARY)
                                   .field("data")))
          .root();

  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("111"));

    test_delete_f(root, R"*({"data1":"data5"})*", owner);

    EXPECT_ROWS_ADDED("t2_base", -1);
    EXPECT_ROWS_ADDED("t2_ref_nm_join", -2);
    EXPECT_ROWS_ADDED("t2_ref_nm", -2);
  }
  // TODO(alfredo) - filter by nested field

  // row_owner = PK
  {
    snapshot();

    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("222"));

    test_delete_f(root, R"*({"data1": "data2", "data2": 2})*", owner);
    EXPECT_ROWS_ADDED("t2_base", 0);
    EXPECT_ROWS_ADDED("t2_ref_nm_join", 0);
    EXPECT_ROWS_ADDED("t2_ref_nm", 0);

    test_delete_f(root, R"*({"data1": "data6", "data2": 6})*", owner);
    EXPECT_ROWS_ADDED("t2_base", -1);
    EXPECT_ROWS_ADDED("t2_ref_nm_join", 0);
    EXPECT_ROWS_ADDED("t2_ref_nm", 0);

    snapshot();

    auto owner2 = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                     mysqlrouter::sqlstring("111"));

    test_delete_f(root, R"*({"data1": "data1"})*", owner2);
    EXPECT_ROWS_ADDED("t2_base", -2);
    EXPECT_ROWS_ADDED("t2_ref_nm_join", -1);
    EXPECT_ROWS_ADDED("t2_ref_nm", -1);
  }
}

TEST_F(DatabaseQueryDelete, disabled_refs) {
  // delete row from table with references that are not enabled

  auto root =
      ObjectBuilder("mrstestdb", "t2_base")
          .field("id", FieldFlag::PRIMARY)
          .field("data1")
          .field("data2")
          .column("ref_11")
          .nest("t2Ref11",
                ObjectBuilder("t2_ref_11", {{"ref_11", "id"}}).column("id"),
                FieldFlag::DISABLED)
          .nest("t2RefNmJoin",
                ObjectBuilder("t2_ref_nm_join", {{"id", "base_id"}})
                    .column("base_id", "int", FieldFlag::PRIMARY)
                    .column("ref_id", "int", FieldFlag::PRIMARY)
                    .nest("t2RefNm",
                          ObjectBuilder("t2_ref_nm", {{"ref_id", "id"}})
                              .column("id", FieldFlag::PRIMARY)
                              .column("data")),
                FieldFlag::DISABLED);

  test_delete(root, {{"id", "12"}});
}

// XXX ensure all RowDelete classes are covered
