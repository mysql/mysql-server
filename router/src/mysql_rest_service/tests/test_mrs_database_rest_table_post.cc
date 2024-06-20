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

inline std::string unescape(const std::string &s) {
  static std::vector<std::pair<std::string_view, std::string_view>> k_repl = {
      {"\\n", "\n"}, {"\\'", "'"},  {"\\\"", "\""}, {"\\Z", "Z"},
      {"\\r", "\r"}, {"\\b", "\b"}, {"\\\\", "\\"}};

  std::string out = s;
  for (const auto &r : k_repl) {
    out = str_replace(out, r.first, r.second);
  }
  return out;
}

#define EXPECT_UUID(value) EXPECT_EQ(16, unescape(value).size() - 2) << value

class DatabaseQueryPost : public DatabaseRestTableTest {
 public:
  PrimaryKeyColumnValues test_post(std::shared_ptr<DualityView> root,
                                   const rapidjson::Document &doc,
                                   const ObjectRowOwnership &row_owner = {}) {
    mrs::database::dv::DualityViewUpdater rest(root, row_owner);

    return rest.insert(m_.get(), doc);
  }

  void test_post_ai(std::shared_ptr<DualityView> root,
                    const rapidjson::Document &doc,
                    const ObjectRowOwnership &row_owner = {}) {
    auto id = next_auto_inc(root->table);

    auto pk = test_post(root, doc, row_owner);
    EXPECT_EQ(1, pk.size());
    EXPECT_EQ(id, pk[root->primary_key()[0]->name].str());
  }

  void test_post_uuid(std::shared_ptr<DualityView> root,
                      const rapidjson::Document &doc,
                      const ObjectRowOwnership &row_owner = {}) {
    auto pk = test_post(root, doc, row_owner);
    EXPECT_EQ(1, pk.size());
    EXPECT_UUID(pk[root->primary_key()[0]->name].str());
  }
};

TEST_F(DatabaseQueryPost, no_root_fields) {
  prepare(TestSchema::PLAIN);

  // no fields in the root object (auto_inc)
  auto root =
      DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_INSERT)
          .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field_to_many(
              "cities",
              ViewBuilder("city", TableFlag::WITH_INSERT)
                  .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("city")
                  .field("city_id", FieldFlag::PRIMARY),
              false)
          .resolve(m_.get(), true);

  {
    auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);

    // omitting all fields in the row is OK (because PK is auto-inc),
    // but the country column is NOT NULL, so we get a db error
    EXPECT_MYSQL_ERROR(rest->insert(m_.get(), make_json(R"*({
    "cities": [
      {"city": "MyCity"},
      {"city": "New MyCity"},
      {"city": "West MyCity"}
    ]
  })*")),
                       "Field 'country' doesn't have a default value");
  }
}

TEST_F(DatabaseQueryPost, no_pk) {
  prepare(TestSchema::PLAIN);

  auto root = DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
                  .field("id", FieldFlag::PRIMARY)
                  .field("data")
                  .resolve(m_.get(), true);

  {
    auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);

    EXPECT_JSON_ERROR(rest->insert(m_.get(), make_json(R"*({
    "data": "MyCountry"
  })*")),
                      "Invalid input JSON document: ID for table `root` "
                      "missing in JSON input");
  }
}

TEST_F(DatabaseQueryPost, no_pk_multi) {
  prepare(TestSchema::PLAIN);

  auto root =
      DualityViewBuilder("mrstestdb", "tc2_base", TableFlag::WITH_INSERT)
          .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("sub_id", FieldFlag::PRIMARY)
          .field("data1")
          .resolve(m_.get(), true);

  auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
  EXPECT_JSON_ERROR(rest->insert(m_.get(), make_json(R"*({
    "data1": "data"
  })*")),
                    "ID for table `tc2_base` missing in JSON input");

  test_post(root, make_json(R"*({
    "sub_id": "AA",
    "data1": "data"
  })*"));
}

TEST_F(DatabaseQueryPost, no_pk_in_1n_child) {
  prepare(TestSchema::PLAIN);

  auto root = DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
                  .field("id", FieldFlag::PRIMARY)
                  .field("data1")
                  .field_to_many("1n",
                                 ViewBuilder("child_1n", TableFlag::WITH_INSERT)
                                     .field("id", FieldFlag::PRIMARY)
                                     .field("data"),
                                 false)
                  .resolve(m_.get(), true);

  // no child given, so no problem
  {
    auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);

    rest->insert(m_.get(), make_json(R"*({
    "id": 123,
    "data1": "MyCountry"
  })*"));
  }

  {
    auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);

    EXPECT_JSON_ERROR(rest->insert(m_.get(), make_json(R"*({
    "id": 124,
    "data1": "MyCountry",
    "1n": [
      {"data": "MyCity"}
    ]
  })*")),
                      "ID for table `child_1n` missing in JSON input");
  }
}

TEST_F(DatabaseQueryPost, unknown_fields) {
  auto root = DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_INSERT)
                  .field("country_id", FieldFlag::PRIMARY)
                  .field("country")
                  .resolve(m_.get(), true);

  // no PK value given but it's autogenerated
  {
    auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);

    EXPECT_JSON_ERROR(
        rest->insert(m_.get(), make_json(R"*({
    "country_id": 123,
    "country": "AAA",
    "population": 1234
  })*")),
        "Invalid field \"population\" in table `country` in JSON input");
  }
}

TEST_F(DatabaseQueryPost, disabled_fields) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor", TableFlag::WITH_INSERT)
          .field("actor_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("first_name")
          .field("last_name")
          .column("last_update")
          .field_to_many(
              "film_actor",
              ViewBuilder("film_actor", TableFlag::WITH_INSERT)
                  .column("actor_id", FieldFlag::PRIMARY)
                  .column("film_id", FieldFlag::PRIMARY)
                  .field_to_one("film",
                                ViewBuilder("film", TableFlag::WITH_INSERT)
                                    .field("film_id", FieldFlag::PRIMARY |
                                                          FieldFlag::AUTO_INC)
                                    .field("title"),
                                false),
              false)
          .resolve(m_.get(), true);

  // don't require disabled fields
  {
    test_post_ai(root, make_json(R"*({
    "first_name": "Angelina",
    "last_name": "Joline",
    "film_actor": [
        {"film": {"film_id": 1, "title": "Frozen"}}
    ]
  })*"));
  }
  // error if disabled fields given
  {
    auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);

    EXPECT_JSON_ERROR(
        rest->insert(m_.get(), make_json(R"*({
    "first_name": "Angelina",
    "last_name": "Joline",
    "last_update": "1999-01-01 11:11:00",
    "film_actor": [
        {"film": {"film_id": 1, "title": "Frozen"}}
    ]
  })*")),
        "Invalid field \"last_update\" in table `actor` in JSON input");
  }
}

TEST_F(DatabaseQueryPost, type_check_nested) {
  {
    auto root =
        DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_INSERT)
            .field("country_id", FieldFlag::PRIMARY)
            .field_to_one("nest", ViewBuilder("city")
                                      .field("country_id")
                                      .field("city_id", FieldFlag::PRIMARY)
                                      .field("city"))
            .resolve(m_.get(), true);
    {
      auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
      EXPECT_JSON_ERROR(
          rest->insert(m_.get(), make_json(R"*({
    "country_id": 123,
    "nest": "AAA"
  })*")),
          "Invalid value for \"nest\" for table `country` in JSON input");
    }
    {
      auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
      EXPECT_JSON_ERROR(
          rest->insert(m_.get(), make_json(R"*({
    "country_id": 123,
    "nest": 1234
  })*")),
          "Invalid value for \"nest\" for table `country` in JSON input");
    }
    {
      auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
      EXPECT_JSON_ERROR(
          rest->insert(m_.get(), make_json(R"*({
    "country_id": 123,
    "nest": []
  })*")),

          "Invalid value for \"nest\" for table `country` in JSON input");
    }
  }
  {
    auto root =
        DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_INSERT)
            .field("country_id", FieldFlag::PRIMARY)
            .field_to_many("nest",
                           ViewBuilder("city", TableFlag::WITH_INSERT)
                               .column("country_id")
                               .field("city_id", FieldFlag::PRIMARY)
                               .field("city", "city", "VARCHAR(40)"),
                           false)
            .resolve(m_.get(), true);
    {
      auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
      EXPECT_JSON_ERROR(
          rest->insert(m_.get(), make_json(R"*({
      "country_id": 123,
      "nest": "AAA"
    })*")),
          "Invalid value for \"nest\" for table `country` in JSON input");
    }
    {
      auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
      EXPECT_JSON_ERROR(
          rest->insert(m_.get(), make_json(R"*({
      "country_id": 123,
      "nest": 1234
    })*")),
          "Invalid value for \"nest\" for table `country` in JSON input");
    }
    {
      auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
      EXPECT_JSON_ERROR(
          rest->insert(m_.get(), make_json(R"*({
      "country_id": 123,
      "nest": {}
    })*")),
          "Invalid value for \"nest\" for table `country` in JSON input");
    }
    {
      auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
      EXPECT_JSON_ERROR(
          rest->insert(m_.get(), make_json(R"*({
      "country_id": 123,
      "nest": null
    })*")),
          "Invalid value for \"nest\" for table `country` in JSON input");
    }
    {
      auto rest = std::make_unique<mrs::database::dv::DualityViewUpdater>(root);
      EXPECT_JSON_ERROR(rest->insert(m_.get(), make_json(R"*({
    "country_id": 123,
    "nest": [1234]
  })*")),
                        "Invalid document in JSON input for table `city`");
    }
  }
}

TEST_F(DatabaseQueryPost, special_types) {
  auto root =
      DualityViewBuilder("mrstestdb", "typetest", TableFlag::WITH_INSERT)
          .field("id", FieldFlag::PRIMARY)
          .field("Geom", "geom", "GEOMETRY")
          .field("Bool", "bool", "BIT(1)")
          .field("Binary", "bin", "BLOB")
          .field("Json", "js", "JSON")
          .resolve(m_.get(), true);

  test_post(root, make_json(R"*({
  "id": 42,
  "Bool": true,
  "Geom": {
      "type": "Point",
      "coordinates": [
          12.123,
          34.123
      ]
  },
  "Binary": "SGVsbG8gV29ybGQK",
  "Json": [1, {"a": true, "b": null}]
})*"));

  EXPECT_ROWS_ADDED("typetest", 1);

  auto row = m_->query_one(
      "SELECT id, hex(geom), hex(bool), hex(bin), js FROM mrstestdb.typetest "
      "WHERE id=42");
  EXPECT_STREQ("42", (*row)[0]);
  EXPECT_STREQ("000000000101000000E5D022DBF93E284039B4C876BE0F4140", (*row)[1]);
  EXPECT_STREQ("1", (*row)[2]);
  EXPECT_STREQ("48656C6C6F20576F726C640A", (*row)[3]);
  EXPECT_STREQ("[1, {\"a\": true, \"b\": null}]", (*row)[4]);

  root->with_check_ = false;

  // test other json values
  test_post(root, make_json(R"*({
  "id": 43,
  "Json": 1
})*"));
  row = m_->query_one("SELECT js FROM mrstestdb.typetest WHERE id=43");
  EXPECT_STREQ("1", (*row)[0]);

  test_post(root, make_json(R"*({
  "id": 44,
  "Json": "hello"
})*"));
  row = m_->query_one("SELECT js FROM mrstestdb.typetest WHERE id=44");
  EXPECT_STREQ("\"hello\"", (*row)[0]);

  test_post(root, make_json(R"*({
  "id": 45,
  "Json": null
})*"));
  row = m_->query_one("SELECT js FROM mrstestdb.typetest WHERE id=45");
  EXPECT_EQ(NULL, (*row)[0]);

  test_post(root, make_json(R"*({
  "id": 46,
  "Json": {}
})*"));
  row = m_->query_one("SELECT js FROM mrstestdb.typetest WHERE id=46");
  EXPECT_STREQ("{}", (*row)[0]);
}

TEST_F(DatabaseQueryPost, store_bool_in_int) {
  // sending bool values to be stored in any int column should be converted to 1
  // or 0
  auto root = DualityViewBuilder("mrstestdb", "t2_base", TableFlag::WITH_INSERT)
                  .column("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("data2", "data2", "INT")
                  .resolve(m_.get(), true);

  auto pk = test_post(root, make_json(R"*({"data2": true})*"));
  auto row = m_->query_one(
      ("SELECT data2 FROM mrstestdb.t2_base WHERE id=" + pk["id"].str()));
  EXPECT_STREQ("1", (*row)[0]);

  pk = test_post(root, make_json(R"*({"data2": false})*"));
  row = m_->query_one(
      ("SELECT data2 FROM mrstestdb.t2_base WHERE id=" + pk["id"].str()));
  EXPECT_STREQ("0", (*row)[0]);
}

TEST_F(DatabaseQueryPost, field_defaults) {
  // for POST, missing field values should be filled with the DEFAULT (but
  // NOCHECK)
  m_->execute(R"*(CREATE TABLE mrstestdb.defaults_test (a int primary key,
    b int not null,
    c int default null,
    d timestamp default current_timestamp,
    e varchar(4) default 'ABC',
    f json default ('{}')
  ))*");

  auto root =
      DualityViewBuilder("mrstestdb", "defaults_test",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("a", "a", "int", FieldFlag::PRIMARY)
          .field("b")
          .field("c")
          .field("d")
          .field("e")
          .field("f")
          .resolve(m_.get(), true);

  // no value given for field with no default
  EXPECT_MYSQL_ERROR(test_post(root, make_json(R"*({"a": 42})*")),
                     R"*(Field 'b' doesn't have a default value (1364))*");

  // no value given for fields with default
  test_post(root, make_json(R"*({"a": 42, "b": 123})*"));

  auto row = m_->query_one("select * from mrstestdb.defaults_test");
  EXPECT_STREQ("42", (*row)[0]);
  EXPECT_STREQ("123", (*row)[1]);
  EXPECT_EQ(nullptr, (*row)[2]);
  EXPECT_TRUE((strncmp((*row)[3], "20", 2) == 0));
  EXPECT_STREQ("ABC", (*row)[4]);
  EXPECT_STREQ("{}", (*row)[5]);
}

TEST_F(DatabaseQueryPost, root_rowowner_notpk) {
  prepare(TestSchema::PLAIN);
  prepare_user_metadata();

  auto root = DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
                  .field("id", FieldFlag::PRIMARY)
                  .field("owner_id", FieldFlag::OWNER)
                  .field("data2", "data2", "INT")
                  .resolve(m_.get());

  // omit ownership column value in input
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    auto pk = test_post(root, make_json(R"*({
      "id": 9000,
      "data2": 41
    })*"),
                        owner);
    EXPECT_EQ("9000", pk["id"].str());

    auto res = select_one(root, pk, {}, owner);
    EXPECT_EQ(
        R"*({"id":9000,"data2":41,"owner_id":"EREAAAAAAAAAAAAAAAAAAA==","_metadata":{"etag":"431D37275722169D47F3976AC5E8AF7F9B02144715058FEC183E13C77E9708B0"}})*",
        res);
  }

  // ensure ownership column can't be overriden in request
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    auto pk = test_post(root, make_json(R"*({
      "id": 10000,
      "owner_id": "IiIAAAAAAAAAAAAAAAAAAA==",
      "data2": 42
    })*"),
                        owner);
    EXPECT_EQ("10000", pk["id"].str());

    auto res = select_one(root, pk, {}, owner);
    EXPECT_EQ(
        R"*({"id":10000,"data2":42,"owner_id":"EREAAAAAAAAAAAAAAAAAAA==","_metadata":{"etag":"58C8B1D1AAF32B7C7460F15C4ECC133AB22B2603AFD5A4A18E78BCD8D77BFC15"}})*",
        res);
  }
}

TEST_F(DatabaseQueryPost, root_rowowner_pk) {
  prepare(TestSchema::PLAIN);
  prepare_user_metadata();

  auto root =
      DualityViewBuilder("mrstestdb", "root_owner", TableFlag::WITH_INSERT)
          .field("id", FieldFlag::PRIMARY | FieldFlag::OWNER)
          .field("data2", "data2", "INT")
          .resolve(m_.get());

  // omit ownership column value in input
  {
    auto owner = ObjectRowOwnership(
        root, "id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    auto pk = test_post(root, make_json(R"*({
      "data2": 41
    })*"),
                        owner);
    EXPECT_EQ("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')", pk["id"].str());

    auto res = select_one(root, pk, {}, owner);
    EXPECT_EQ(
        R"*({"id":"EREAAAAAAAAAAAAAAAAAAA==","data2":41,"_metadata":{"etag":"03F185C8F6087AC41EA61320D2796D65E09C08D5ABE6446D0FEC3A006FBB2D0B"}})*",
        res);
  }

  // ensure ownership column can't be overriden in request
  {
    auto owner = ObjectRowOwnership(
        root, "id",
        mysqlrouter::sqlstring("FROM_BASE64('IiIAAAAAAAAAAAAAAAAAAA==')"));

    auto pk = test_post(root, make_json(R"*({
      "id": "EREAAAAAAAAAAAAAAAAAAA==",
      "data2": 42
    })*"),
                        owner);
    EXPECT_EQ("FROM_BASE64('IiIAAAAAAAAAAAAAAAAAAA==')", pk["id"].str());

    auto res = select_one(root, pk, {}, owner);
    EXPECT_EQ(
        R"*({"id":"IiIAAAAAAAAAAAAAAAAAAA==","data2":42,"_metadata":{"etag":"030EA55EC5DA0792C318DC30ACE7956238E5C27A6382056D023AC80F65122637"}})*",
        res);
  }
}

TEST_F(DatabaseQueryPost, nested_11_multi) {
  // nested 1:1 children can't be inserted
  auto root =
      DualityViewBuilder("mrstestdb", "tc2_base", TableFlag::WITH_INSERT)
          .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("sub_id", FieldFlag::PRIMARY)
          .field("data1")
          .field("data2")
          .column("ref_11_id")
          .column("ref_11_sub_id")
          .field_to_one(
              "ref", ViewBuilder("tc2_ref_11")
                         .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                         .field("sub_id", FieldFlag::PRIMARY)
                         .field("data"))
          .resolve(m_.get(), true);

  {
    EXPECT_JSON_ERROR(test_post(root, make_json(R"*({
      "id": 222,
      "sub_id": "AB",
      "data1": "AAA",
      "data2": 1,
      "ref": {
        "sub_id": 888,
        "data": "REF11"
      }
    })*")),
                      "ID for table `tc2_ref_11` missing in JSON input");
  }
}

TEST_F(DatabaseQueryPost, nested_nm_autoinc_ref) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor", TableFlag::WITH_INSERT)
          .field("actor_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("first_name")
          .field("last_name")
          .field_to_many(
              "film_actor",
              ViewBuilder("film_actor", TableFlag::WITH_INSERT)
                  .column("actor_id", FieldFlag::PRIMARY)
                  .column("film_id", FieldFlag::PRIMARY)
                  .field_to_one("film",
                                ViewBuilder("film")
                                    .field("film_id", FieldFlag::PRIMARY |
                                                          FieldFlag::AUTO_INC)
                                    .field("title")
                                    .field("language_id")
                                    .field("original_language_id")))
          .resolve(m_.get(), true);

  {
    auto doc = make_json(R"*({
    "first_name": "Angelina",
    "last_name": "Joline",
    "film_actor": [
        {"film": {"film_id": 11, "title": "THE TEST I", "language_id":1, "original_language_id":1}},
        {"film": {"film_id": 12, "title": "THE TEST II", "language_id":1, "original_language_id":1}},
        {"film": {"film_id": 13, "title": "THE TEST III", "language_id":1, "original_language_id":1}}
    ]
  })*");
    test_post_ai(root, doc);
  }
  // empty list
  {
    auto doc = make_json(R"*({
    "first_name": "Angelina",
    "last_name": "Joline",
    "film_actor": []
  })*");
    test_post_ai(root, doc);
  }
}

TEST_F(DatabaseQueryPost, nested_nm_autoinc_ref_extras) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor", TableFlag::WITH_INSERT)
          .field("actor_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("first_name")
          .field("last_name")
          .field_to_many(
              "film_actor",
              ViewBuilder("film_actor", TableFlag::WITH_INSERT)
                  .column("actor_id", FieldFlag::PRIMARY)
                  .column("film_id", FieldFlag::PRIMARY)
                  .field("last_update")
                  .field_to_one("film",
                                ViewBuilder("film")
                                    .field("film_id", FieldFlag::PRIMARY |
                                                          FieldFlag::AUTO_INC)
                                    .field("title")
                                    .field("language_id")
                                    .field("original_language_id")))
          .resolve(m_.get(), true);

  {
    auto doc = make_json(R"*({
    "first_name": "Angelina",
    "last_name": "Joline",
    "film_actor": [
        {"last_update": "2000-01-01 1:10:10", 
          "film": {"film_id": 11, "title": "THE TEST I", "language_id":1, "original_language_id":1}},
        {"last_update": "2000-01-02 1:10:10",
          "film": {"film_id": 12, "title": "THE TEST II", "language_id":1, "original_language_id":1}},
        {"last_update": "2000-01-03 1:10:10",
          "film": {"film_id": 13, "title": "THE TEST III", "language_id":1, "original_language_id":1}}
    ]
  })*");
    test_post_ai(root, doc);
  }
  // empty list
  {
    auto doc = make_json(R"*({
    "first_name": "Angelina",
    "last_name": "Joline",
    "film_actor": []
  })*");
    test_post_ai(root, doc);
  }
}

TEST_F(DatabaseQueryPost, DISABLED_nested_nm_row_owner) {
  // TODO: look into making PK that's also FK optional
  auto root =
      DualityViewBuilder("mrstestdb", "t2_base", TableFlag::WITH_INSERT)
          .field("id",
                 FieldFlag::PRIMARY | FieldFlag::AUTO_INC | FieldFlag::OWNER)
          .field("data1")
          .field("data2")
          .field_to_many(
              "refs",
              ViewBuilder("t2_ref_nm_join", TableFlag::WITH_INSERT)
                  .field("base_id", FieldFlag::PRIMARY)
                  .field("ref_id", FieldFlag::PRIMARY)
                  .field_to_one("ref", ViewBuilder("t2_ref_nm")
                                           .field("id", FieldFlag::PRIMARY |
                                                            FieldFlag::AUTO_INC)
                                           .field("data")))
          .resolve(m_.get());
  // PK = owner
  {
    auto doc = make_json(R"*({
    "data1": "AAA",
    "data2": 5,
    "refs": [
        {"ref": {"id": 1, "data": "DATA1"}},
        {"ref": {"id": 2, "data": "DATA2"}}
    ]
  })*");

    auto owner = ObjectRowOwnership(root, "id", mysqlrouter::sqlstring("444"));
    auto pk = test_post(root, doc, owner);
    EXPECT_EQ(1, pk.size());
    EXPECT_EQ("444", pk["id"].str());
  }
}

TEST_F(DatabaseQueryPost, nested_nm_multi_row_owner) {
  prepare(TestSchema::MULTI);

  auto root = DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
                  .field("id1", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("id2", FieldFlag::PRIMARY | FieldFlag::OWNER)
                  .field("data1")
                  .field("data2")
                  .resolve(m_.get());

  // id1 is provided
  {
    auto owner = ObjectRowOwnership(
        root, "id2",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    auto pk = test_post(root, make_json(R"*({
      "id1": 3000,
      "data1": "ABC",
      "data2": 5
    })*"),
                        owner);
    EXPECT_EQ(2, pk.size());
    EXPECT_EQ("3000", pk["id1"].str());
    EXPECT_EQ("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')", pk["id2"].str());

    auto res = select_one(root, pk, {}, owner);
    EXPECT_EQ(
        R"*({"id1":3000,"id2":"EREAAAAAAAAAAAAAAAAAAA==","data1":"ABC","data2":5,"_metadata":{"etag":"3B08113C73D9F8B244BABCDBB553F86633E31AB96A69767C306D47C4EBB806F1"}})*",
        res);
  }

  // id1 is generated
  {
    auto owner = ObjectRowOwnership(
        root, "id2",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    auto pk = test_post(root, make_json(R"*({
      "data1": "ABCD",
      "data2": 6
    })*"),
                        owner);
    EXPECT_EQ(2, pk.size());
    EXPECT_EQ("3001", pk["id1"].str());
    EXPECT_EQ("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')", pk["id2"].str());

    auto res = select_one(root, pk, {}, owner);
    EXPECT_EQ(
        R"*({"id1":3001,"id2":"EREAAAAAAAAAAAAAAAAAAA==","data1":"ABCD","data2":6,"_metadata":{"etag":"5C5588A0D74FA9899250FE9ECD6F4FF43CC93375B59DF21EEA1B9FE36330C527"}})*",
        res);
  }
}
