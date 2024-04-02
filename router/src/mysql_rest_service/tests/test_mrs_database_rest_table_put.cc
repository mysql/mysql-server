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
#include "helper/json/to_string.h"
#include "mock/mock_session.h"
#include "mrs/database/query_rest_table_updater.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;

// - owner_id
// - unnest
// - reduce-to-field
// - allow crud flags

class DatabaseQueryPut : public DatabaseRestTableTest {
 public:
  void test_put(std::shared_ptr<entry::Object> root,
                const rapidjson::Document &doc,
                const PrimaryKeyColumnValues &pk,
                const ObjectRowOwnership &row_owner = {}) {
    mrs::database::TableUpdater rest(root, row_owner);

    rest.handle_put(m_.get(), doc, pk);
  }
};

TEST_F(DatabaseQueryPut, missing_fields) {
  // XXX check that missing field is not disabled too
}

TEST_F(DatabaseQueryPut, unknown_fields) {
  // XXX check "links" field allowed
}

TEST_F(DatabaseQueryPut, type_check_nested) {
  {
    auto root =
        ObjectBuilder("mrstestdb", "country")
            .field("country_id", FieldFlag::PRIMARY)
            .nest("nest", ObjectBuilder("city", {{"country_id", "country_id"}})
                              .column("country_id")
                              .field("city"));
    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
    "country_id": 123,
    "nest": "AAA"
  })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error, "/nest expected to be an Object");
    }
    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
    "country_id": 123,
    "nest": 1234
  })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error, "/nest expected to be an Object");
    }
    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
    "country_id": 123,
    "nest": []
  })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error,
                       "/nest is an Array but wasn't expected to be");
    }
  }
  {
    auto root =
        ObjectBuilder("mrstestdb", "country")
            .field("country_id", FieldFlag::PRIMARY)
            .nest_list("nest",
                       ObjectBuilder("city", {{"country_id", "country_id"}})
                           .column("country_id")
                           .field("city", "city", "VARCHAR(40)"));
    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
      "country_id": 123,
      "nest": "AAA"
    })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error, "/nest expected to be an Array");
    }
    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
      "country_id": 123,
      "nest": 1234
    })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error, "/nest expected to be an Array");
    }
    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
      "country_id": 123,
      "nest": {}
    })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error, "/nest expected to be an Array");
    }
    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
      "country_id": 123,
      "nest": null
    })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error, "/nest expected to be an Array");
    }
    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
    "country_id": 123,
    "nest": [1234]
  })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error, "/nest/0 expected to be an Object");
    }

    {
      EXPECT_THROW_MSG(test_put(root, make_json(R"*({
    "country_id": 123,
    "nest": [{"city":1234}]
  })*"),
                                {{"country_id", "5"}}),
                       std::runtime_error,
                       "/nest/0/city has invalid value type");
    }
  }
}

TEST_F(DatabaseQueryPut, type_check) {
  std::vector<std::pair<const char *, entry::ColumnType>> known_types{
      {"BIT(1)", entry::ColumnType::BOOLEAN},
      {"BIT", entry::ColumnType::BINARY},
      {"TINYINT", entry::ColumnType::INTEGER},
      {"SMALLINT", entry::ColumnType::INTEGER},
      {"MEDIUMINT", entry::ColumnType::INTEGER},
      {"INT", entry::ColumnType::INTEGER},
      {"BIGINT", entry::ColumnType::INTEGER},
      {"FLOAT", entry::ColumnType::DOUBLE},
      {"REAL", entry::ColumnType::DOUBLE},
      {"DOUBLE", entry::ColumnType::DOUBLE},
      {"DECIMAL(10,2)", entry::ColumnType::DOUBLE},
      {"CHAR(42)", entry::ColumnType::STRING},
      {"NCHAR", entry::ColumnType::STRING},
      {"VARCHAR", entry::ColumnType::STRING},
      {"NVARCHAR", entry::ColumnType::STRING},
      {"BINARY", entry::ColumnType::BINARY},
      {"VARBINARY", entry::ColumnType::BINARY},
      {"TINYTEXT", entry::ColumnType::STRING},
      {"TEXT", entry::ColumnType::STRING},
      {"MEDIUMTEXT", entry::ColumnType::STRING},
      {"LONGTEXT", entry::ColumnType::STRING},
      {"TINYBLOB", entry::ColumnType::BINARY},
      {"BLOB", entry::ColumnType::BINARY},
      {"MEDIUMBLOB", entry::ColumnType::BINARY},
      {"LONGBLOB", entry::ColumnType::BINARY},
      {"JSON", entry::ColumnType::JSON},
      {"DATETIME", entry::ColumnType::STRING},
      {"DATE", entry::ColumnType::STRING},
      {"TIME(6)", entry::ColumnType::STRING},
      {"YEAR", entry::ColumnType::INTEGER},
      {"TIMESTAMP", entry::ColumnType::STRING},
      {"GEOMETRY", entry::ColumnType::GEOMETRY},
      {"POINT", entry::ColumnType::GEOMETRY},
      {"LINESTRING", entry::ColumnType::GEOMETRY},
      {"POLYGON", entry::ColumnType::GEOMETRY},
      {"GEOMETRYCOLLECTION", entry::ColumnType::GEOMETRY},
      {"MULTIPOINT", entry::ColumnType::GEOMETRY},
      {"MULTILINESTRING", entry::ColumnType::GEOMETRY},
      {"MULTIPOLYGON", entry::ColumnType::GEOMETRY},
      {"BOOLEAN", entry::ColumnType::BOOLEAN},
      {"ENUM", entry::ColumnType::STRING},
      {"SET", entry::ColumnType::STRING}};

  std::map<entry::ColumnType, std::vector<const char *>> bad_values{
      {entry::ColumnType::INTEGER, {"32.20", "\"\"", "\"x\""}},
      {entry::ColumnType::DOUBLE, {"\"\"", "\"x\"", "true"}},
      {entry::ColumnType::BOOLEAN, {"32.34", "\"x\"", "\"\""}},
      {entry::ColumnType::STRING, {"42", "32.34", "true"}},
      {entry::ColumnType::BINARY, {"42", "32.34", "true"}},
      {entry::ColumnType::GEOMETRY, {"42", "32.34", "true"}},
      {entry::ColumnType::JSON, {}}};

  for (const auto &type : known_types) {
    auto root = ObjectBuilder("mrstestdb", "country")
                    .column("country_id", FieldFlag::PRIMARY)
                    .field("value", "value", type.first);

    for (const auto &test : bad_values[type.second]) {
      SCOPED_TRACE(std::string(type.first) + " value=" + test);
      EXPECT_THROW_MSG(
          test_put(root, make_json(std::string("{\"value\": ") + test + "}"),
                   {{"country_id", "1"}}),
          std::runtime_error, "/value has invalid value type");
    }
  }
}

TEST_F(DatabaseQueryPut, etag_check) {}

TEST_F(DatabaseQueryPut, special_types) {
  auto root = ObjectBuilder("mrstestdb", "typetest")
                  .field("id", FieldFlag::PRIMARY)
                  .field("Geom", "geom", "GEOMETRY")
                  .field("Bool", "bool", "BIT(1)")
                  .field("Binary", "bin", "BLOB")
                  .field("Json", "js", "JSON");

  test_put(root, make_json(R"*({
  "id": 1,
  "Bool": false,
  "Geom": {
      "type": "Point",
      "coordinates": [
          12.123,
          34.123
      ]
  },
  "Binary": "SGVsbG8gV29ybGQK",
  "Json": [1,2,3]
})*"),
           {{"id", "1"}});

  auto row = m_->query_one(
      "SELECT id, hex(geom), hex(bool), hex(bin), js FROM mrstestdb.typetest "
      "WHERE id=1");
  EXPECT_STREQ("1", (*row)[0]);
  EXPECT_STREQ("000000000101000000E5D022DBF93E284039B4C876BE0F4140", (*row)[1]);
  EXPECT_STREQ("0", (*row)[2]);
  EXPECT_STREQ("48656C6C6F20576F726C640A", (*row)[3]);
  EXPECT_STREQ("[1, 2, 3]", (*row)[4]);
}

TEST_F(DatabaseQueryPut, plain_fields) {
  auto root = ObjectBuilder("mrstestdb", "actor")
                  .field("actorId", "actor_id", "int",
                         FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("firstName", "first_name", "text")
                  .field("lastName", "last_name", "text")
                  .root();

  // row already exists
  {
    auto doc = make_json(R"*({
    "firstName": "Arnold",
    "lastName": "Smith"
  })*");

    test_put(root, doc, {{"actor_id", "5"}});
  }

  // try to override PK
  {
    auto doc = make_json(R"*({
    "actorId": 123,
    "firstName": "Arnold",
    "lastName": "Smith II"
  })*");

    test_put(root, doc, {{"actor_id", "5"}});
  }
}

TEST_F(DatabaseQueryPut, base_row_no_exist) {
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

  {
    auto doc = make_json(R"*({
    "country": "Testland",
    "cities": []
  })*");

    test_put(root, doc, {{"country_id", "40"}});
  }
  {
    auto doc = make_json(R"*({
    "country": "Testland",
    "cities": [{"city": "Test City"}]
  })*");

    test_put(root, doc, {{"country_id", "41"}});
  }
}

TEST_F(DatabaseQueryPut, no_pk) {
  auto root = ObjectBuilder("mrstestdb", "country")
                  .field("country_id", FieldFlag::PRIMARY)
                  .field("country")
                  .root();

  auto doc = make_json(R"*({
    "country": "Testland"
  })*");

  {
    EXPECT_REST_ERROR(test_put(root, doc, {}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(test_put(root, doc, {{"country", "Testland"}}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(test_put(root, doc, {{"bogus_id", "111"}}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(
        test_put(root, doc, {{"country_id", "1"}, {"bogus_id", "111"}}),
        "Invalid primary key column");
  }

  auto root2 =
      ObjectBuilder("mrstestdb", "country")
          .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("country")
          .root();
  {
    EXPECT_REST_ERROR(test_put(root2, doc, {}),
                      "Missing primary key column value");
  }
}

TEST_F(DatabaseQueryPut, no_pk_multi) {
  auto root = ObjectBuilder("mrstestdb", "country")
                  .field("country_id", FieldFlag::PRIMARY)
                  .field("continent_id", FieldFlag::PRIMARY)
                  .field("country")
                  .root();

  auto doc = make_json(R"*({
    "country": "Testland"
  })*");

  {
    EXPECT_REST_ERROR(test_put(root, doc, {}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(test_put(root, doc, {{"country_id", "111"}}),
                      "Missing primary key column value");
  }
  {
    EXPECT_REST_ERROR(test_put(root, doc, {{"continent_id", "111"}}),
                      "Missing primary key column value");
  }
}

TEST_F(DatabaseQueryPut, plain_autoinc_row_owner) {
  auto root =
      ObjectBuilder("mrstestdb", "t2_base")
          .field("id", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("owner_id")
          .field("data1", "data1", "text")
          .field("data2", "data2", "int")
          .root();

  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("111"));

    test_put(root, make_json(R"*({
    "data1": "Arnold",
    "data2": 42
  })*"),
             {{"id", "20"}}, owner);
  }
  // try to put as someone else's row
  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("222"));

    test_put(root, make_json(R"*({"owner_id": "ROOT",
    "data1": "Bla",
    "data2": 12
  })*"),
             {{"id", "21"}}, owner);
  }
  // allow put own row
  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "owner_id",
                                    mysqlrouter::sqlstring("333"));

    test_put(
        root,
        make_json(R"*({"owner_id": "USER3", "data1": "Joe", "data2": 1})*"),
        {{"id", "22"}}, owner);
  }

  root = ObjectBuilder("mrstestdb", "t2_base")
             .field("Id", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
             .field("firstName", "data1", "text")
             .field("age", "data2", "int")
             .root();
  // owner_id = PK
  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "id",
                                    mysqlrouter::sqlstring("111"));

    test_put(root, make_json(R"*({"firstName": "Joe", "age": 20})*"),
             {{"id", "25"}}, owner);
  }
  // implicit
  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "id",
                                    mysqlrouter::sqlstring("222"));

    test_put(root, make_json(R"*({"firstName": "Joe", "age": 20})*"), {},
             owner);
  }
  // can't insert someone else's row
  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "id",
                                    mysqlrouter::sqlstring("333"));

    test_put(root, make_json(R"*({"Id": 0, "firstName": "Joe", "age": 20})*"),
             {{"id", "26"}}, owner);
  }
  // allow inserting own row
  {
    auto owner = ObjectRowOwnership(root->get_base_table(), "id",
                                    mysqlrouter::sqlstring("125"));

    test_put(root, make_json(R"*({"Id": 125, "firstName": "Joe", "age": 20})*"),
             {{"id", "27"}}, owner);
  }
}

TEST_F(DatabaseQueryPut, nested_11_owned_child_autoinc) {
  auto root = ObjectBuilder("mrstestdb", "city")
                  .field("city_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("city")
                  .column("country_id")
                  .nest("country",
                        ObjectBuilder("country", {{"country_id", "country_id"}})
                            .field("country_id",
                                   FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                            .field("country"))
                  .root();

  // create from scratch
  {
    test_put(root, make_json(R"*({
    "city": "Test City",
    "country": {
        "country": "Test"
    }
  })*"),
             {{"city_id", "40"}});

    EXPECT_ROWS_ADDED("city", 1);
    EXPECT_ROWS_ADDED("country", 1);
  }

  // create a new nested object (deleting the old one)
  {
    test_put(root, make_json(R"*({
    "city": "Test City",
    "country": {
        "country": "Testland"
    }
  })*"),
             {{"city_id", "40"}});

    // no changes from previous case
    EXPECT_ROWS_ADDED("city", 1);
    EXPECT_ROWS_ADDED("country", 1);
  }

  auto city = get_one(root, {{"city_id", "40"}});
  city.RemoveMember("links");

  // update existing nested object (requires id)
  {
    city["city"] = "New Test City";
    city["country"]["country"] = "New Testland";

    test_put(root, city, {{"city_id", "40"}});

    EXPECT_ROWS_ADDED("city", 1);
    EXPECT_ROWS_ADDED("country", 1);

    city = get_one(root, {{"city_id", "40"}});
    EXPECT_EQ("New Test City", city["city"]);
    EXPECT_EQ("New Testland", city["country"]["country"]);
  }
}

TEST_F(DatabaseQueryPut, nested_11_owned_child_uuid) {
  auto root = ObjectBuilder("mrstestdb", "t1_base")
                  .field("id", FieldFlag::PRIMARY | FieldFlag::REV_UUID)
                  .column("ref_11_id")
                  .field("data")
                  .nest("ref", ObjectBuilder("t1_ref_11", {{"ref_11_id", "id"}})
                                   .field("id", FieldFlag::PRIMARY |
                                                    FieldFlag::REV_UUID)
                                   .field("data"))
                  .root();

  // create a new nested object (deleting the old one)
  {
    auto doc = make_json(R"*({
    "data": "Testland",
    "ref": {
        "data": "Capital"
    }
  })*");

    test_put(root, doc, {{"id", "'UUID1'"}});

    EXPECT_ROWS_ADDED("t1_base", 1);
    EXPECT_ROWS_ADDED("t1_ref_11", 1);
  }

  // XXX try to specify capital_id directly (should error out)

  // update existing nested object (requires id)
  {
    auto doc = make_json(R"*({
      "data" : "Testland",
      "ref" : {
        "id" : "UUID2", "data" : "Capital"
      }
  })*");

    test_put(root, doc, {{"id", "'UUID2'"}});
  }

  // assign to null (delete only)
  {
    auto doc = make_json(R"*({
    "data": "Testland",
    "ref": null
  })*");

    test_put(root, doc, {{"id", "'UUID3'"}});
  }
}

TEST_F(DatabaseQueryPut, unnested_11_owned_child_autoinc) {
  auto root = ObjectBuilder("mrstestdb", "t2_base")
                  .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .column("ref_11_id")
                  .field("data1")
                  .field("data2")
                  .unnest(ObjectBuilder("t2_ref_11", {{"ref_11_id", "id"}})
                              .field("nestedId", "id", "int",
                                     FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                              .field("nestedData", "data"))
                  .root();
  // create a new nested object
  {
    test_put(root, make_json(R"*({
    "data1": "Testland",
    "data2": 12,
    "nestedData": "Capital"
  })*"),
             {{"id", "50"}});

    EXPECT_ROWS_ADDED("t2_base", 1);
    EXPECT_ROWS_ADDED("t2_ref_11", 1);
  }

  // update existing nested object, with wrong id (requires id)
  {
    test_put(root, make_json(R"*({
    "data1": "Testland",
    "data2": 123,
    "id": 100,
    "nestedData": "Capital"
  })*"),
             {{"id", "50"}});

    EXPECT_ROWS_ADDED("t2_base", 1);
    EXPECT_ROWS_ADDED("t2_ref_11", 1);
  }

  // assign to null
  {
    test_put(root, make_json(R"*({
    "data1": "Testland",
    "data2": 1234,
    "nestedData": null
  })*"),
             {{"id", "50"}});

    EXPECT_ROWS_ADDED("t2_base", 1);
    EXPECT_ROWS_ADDED("t2_ref_11", 0);
  }

  // change back from null to an object
  {
    test_put(root, make_json(R"*({
    "data1": "Testland",
    "data2": 1234,
    "nestedData": "New Data"
  })*"),
             {{"id", "50"}});

    EXPECT_ROWS_ADDED("t2_base", 1);
    EXPECT_ROWS_ADDED("t2_ref_11", 1);
  }
}

TEST_F(DatabaseQueryPut, nested_11_multi) {
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
    test_put(root, make_json(R"*({
      "id": 222,
      "sub_id": "AB",
      "data1": "AAA",
      "data2": 1,
      "ref": {
        "sub_id": 888,
        "data": "REF11"
      }
    })*"),
             {{"id", "222"}, {"sub_id", "'AB'"}});

    EXPECT_ROWS_ADDED("tc2_base", 1);
    EXPECT_ROWS_ADDED("tc2_ref_11", 1);
  }

  {
    test_put(root, make_json(R"*({
      "id": 222,
      "sub_id": "AB",
      "data1": "CHANGED",
      "data2": 3,
      "ref": {
        "sub_id": 888,
        "data": "REF11"
      }
    })*"),
             {{"id", "222"}, {"sub_id", "'AB'"}});

    EXPECT_ROWS_ADDED("tc2_base", 1);
    EXPECT_ROWS_ADDED("tc2_ref_11", 1);
  }
}

TEST_F(DatabaseQueryPut, nested_n1_ref_child_autoinc) {
  auto root = ObjectBuilder("mrstestdb", "city")
                  .field("city_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("city")
                  .column("country_id")
                  .nest("country",
                        ObjectBuilder("country", {{"country_id", "country_id"}},
                                      Operation::Values::valueRead)
                            .field("country_id",
                                   FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                            .field("country"))
                  .root();
}

// 1:n test combinations:
// root doesnt exist
// root exists
// - delete all
// - all new
// - delete 2, add one, update 2

TEST_F(DatabaseQueryPut, nested_1n_owned_child_autoinc) {
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

  {
    auto doc = make_json(R"*({
    "country": "Testland",
    "cities": []
  })*");

    test_put(root, doc, {{"country_id", "50"}});
  }
  {
    auto doc = make_json(R"*({
    "country": "Testland",
    "cities": [{"city": "Test City"}, {"city": "Another City"}]
  })*");

    test_put(root, doc, {{"country_id", "51"}});
  }
  {
    /* original:
    {
        "country": "Testland",
        "cities": [
            {"city_id": 123, "city": "Test City"},
            {"city_id": 124, "city": "Deleted City 1"},
            {"city_id": 125, "city": "Deleted City 2"},
            {"city_id": 126, "city": "Unchanged City"}
        ]
    }
    */
    auto doc = make_json(R"*({
    "country_id": 52,
    "country": "Testland",
    "cities": [
        {"city_id": 123, "city": "Renamed City"},
        {"city_id": 126, "city": "Unchanged City"},
        {"city": "New City"}
    ]
  })*");

    test_put(root, doc, {{"country_id", "52"}});
  }
  // insert nested with pre-defined PKs
  {
    auto doc = make_json(R"*({
    "country": "Testland",
    "cities": [{"city_id": 111, "city": "Test City"}]
  })*");

    test_put(root, doc, {{"country_id", "60"}});
  }
}

TEST_F(DatabaseQueryPut, nested_1n_autoinc_autoinc) {
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

  // nested list is empty
  {
    auto doc = make_json(R"*({
      "country": "MyCountry",
      "cities": []
  })*");

    test_put(root, doc, {{"country_id", "20"}});
  }

  // nested list has items, overwrite
  {
    auto doc = make_json(R"*({
    "country": "MyCountry",
    "cities": [
      {"city": "MyCity"},
      {"city": "New MyCity"},
      {"city": "West MyCity"}
    ]
  })*");

    test_put(root, doc, {{"country_id", "20"}});
  }

  // nested list has items again, but country row doesn't exist
  {
    auto doc = make_json(R"*({
    "country": "MyCountry",
    "cities": [
      {"city": "MyCity"},
      {"city": "New MyCity"},
      {"city": "West MyCity"}
    ]
  })*");

    test_put(root, doc, {{"country_id", "22"}});
  }

  // bogus country_id in nested row
  {
    auto doc = make_json(R"*({
    "country": "MyCountry",
    "cities": [
      {"city": "MyCity", "country_id": 99999}
    ]
  })*");

    test_put(root, doc, {{"country_id", "23"}});
  }
}

TEST_F(DatabaseQueryPut, nested_1n_autoinc_uuid) {}

TEST_F(DatabaseQueryPut, nested_1n_uuid_autoinc) {}

TEST_F(DatabaseQueryPut, nested_1n_uuid_uuid) {
  auto root =
      ObjectBuilder("mrstestdb", "t1_base")
          .field("id", "id", "binary(16)",
                 FieldFlag::PRIMARY | FieldFlag::REV_UUID)
          .field("data")
          .nest_list("refs",
                     ObjectBuilder("t1_ref_1n", {{"base_id", "id"}})
                         .field("id", "id", "binary(16)",
                                FieldFlag::PRIMARY | FieldFlag::REV_UUID)
                         .field("data")
                         .column("base_id", "binary(16)"))
          .root();

  // nested list is empty
  {
    auto doc = make_json(R"*({
      "data": "data1",
      "refs": []
  })*");

    test_put(root, doc, {{"id", "FROM_BASE64('VVVJRDEAAAAAAAAAAAAAAA==')"}});
  }

  // nested list is empty, row already exists
  {
    auto doc = make_json(R"*({
      "data": "data1.1",
      "refs": []
  })*");

    test_put(root, doc, {{"id", "FROM_BASE64('VVVJRDEAAAAAAAAAAAAAAA==')"}});
  }

  auto tmp = get_one(root, {{"id", "FROM_BASE64('VVVJRDEAAAAAAAAAAAAAAA==')"}});
  std::cout << helper::json::to_string(&tmp) << "\n";

  // bogus id in nested row
  {
    auto doc = make_json(R"*({
    "data": "data2",
    "refs": [
      {"data": "refdata", "id": "VVVJRDEAAAAAAAAAAAAAAB=="}
    ]
  })*");

    test_put(root, doc, {{"id", "FROM_BASE64('VVVJRDEAAAAAAAAAAAAAAQ==')"}});
  }
}

TEST_F(DatabaseQueryPut, nested_nm_autoinc_ref) {
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

  {
    auto doc = make_json(R"*({
    "first_name": "Angelica",
    "last_name": "Joline",
    "film_actor": [
        {"film": {"film_id": 10, "title": "Frozen"}},
        {"film": {"film_id": 15, "title": "Melted"}}
    ]
  })*");

    test_put(root, doc, {{"actor_id", "50"}});
  }
  // empty list
  {
    auto doc = make_json(R"*({
    "first_name": "Angelica",
    "last_name": "Joline",
    "film_actor": []
  })*");

    test_put(root, doc, {{"actor_id", "51"}});
  }
  // add to list
  {
    // film_id 10 and 15 already exist

    auto doc = make_json(R"*({
    "first_name": "Angelica",
    "last_name": "Joline",
    "film_actor": [
        {"film": {"film_id": 10, "title": "Frozen"}},
        {"film": {"film_id": 15, "title": "Melted"}}
    ]
  })*");

    test_put(root, doc, {{"actor_id", "52"}});
  }
}

TEST_F(DatabaseQueryPut, nested_nm_autoinc_ref_extras) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .field("actor_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("first_name")
          .field("last_name")
          .nest_list(
              "film_actor",
              ObjectBuilder("film_actor2", {{"actor_id", "actor_id"}})
                  .column("actor_id", FieldFlag::PRIMARY)
                  .column("film_id", FieldFlag::PRIMARY)
                  .field("character")
                  .nest("film", ObjectBuilder("film", {{"film_id", "film_id"}},
                                              Operation::valueRead)
                                    .field("film_id", FieldFlag::PRIMARY |
                                                          FieldFlag::AUTO_INC)
                                    .field("title")))
          .root();

  {
    // 10 and 15 exists

    auto doc = make_json(R"*({
    "first_name": "Angelica",
    "last_name": "Joline",
    "film_actor": [
        {"character": "Helga", "film": {"film_id": 10, "title": "Frozen"}},
        {"character": "Alsa", "film": {"film_id": 15, "title": "Melted"}}
    ]
  })*");

    test_put(root, doc, {{"actor_id", "50"}});
  }
  // empty list
  {
    auto doc = make_json(R"*({
    "first_name": "Angelica",
    "last_name": "Joline",
    "film_actor": []
  })*");

    test_put(root, doc, {{"actor_id", "51"}});
  }
}

TEST_F(DatabaseQueryPut, nested_nm_ref_multi) {
  // also tests differently named FK columns
  auto root =
      ObjectBuilder("mrstestdb", "tc2_base")
          .field("id", FieldFlag::PRIMARY)
          .field("sub_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("data1")
          .field("data2")
          .nest_list(
              "refs",
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
                                   .field("id", FieldFlag::PRIMARY)
                                   .field("sub_id", FieldFlag::PRIMARY)
                                   .field("data")))
          .root();

  {
    test_put(root, make_json(R"*({
      "id": 222,
      "sub_id": "AB",
      "data1": "AAA",
      "data2": 1,
      "refs": [
        {
          "ref": {
              "id": 111,
              "sub_id": 888,
              "data": "REF1"
          }
        },
        {
          "ref": {
              "id": 222,
              "sub_id": 999,
              "data": "REF2"
          }
        }
      ]
    })*"),
             {{"id", "222"}, {"sub_id", "'AB'"}});
  }
}

TEST_F(DatabaseQueryPut, nested_nm_ref2_multi) {
  auto root =
      ObjectBuilder("mrstestdb", "tc2_base")
          .field("id", FieldFlag::PRIMARY)
          .field("sub_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("data1")
          .field("data2")
          .nest_list(
              "refs",
              ObjectBuilder("tc2_ref_nm_join",
                            {{"base_id", "id"}, {"base_sub_id", "sub_id"}},
                            Operation::valueRead)
                  .column("base_id", FieldFlag::PRIMARY)
                  .column("base_sub_id", FieldFlag::PRIMARY)
                  .column("ref_id", FieldFlag::PRIMARY)
                  .column("ref_sub_id", FieldFlag::PRIMARY)
                  .nest("ref", ObjectBuilder(
                                   "tc2_ref_nm",
                                   {{"ref_id", "id"}, {"ref_sub_id", "sub_id"}},
                                   Operation::valueRead)
                                   .field("id", FieldFlag::PRIMARY)
                                   .field("sub_id", FieldFlag::PRIMARY)
                                   .field("data")))
          .root();

  {
    test_put(root, make_json(R"*({
      "id": 222,
      "sub_id": "AB",
      "data1": "AAA",
      "data2": 1,
      "refs": [
        {
          "ref": {
              "id": 111,
              "sub_id": 888,
              "data": "REF1"
          }
        },
        {
          "ref": {
              "id": 222,
              "sub_id": 999,
              "data": "REF2"
          }
        }
      ]
    })*"),
             {{"id", "222"}, {"sub_id", "'AB'"}});
  }
}

TEST_F(DatabaseQueryPut, nested_nm_autoinc) {
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
    // XXX something broken with this case

    auto doc = make_json(R"*({
    "first_name": "Angelica",
    "last_name": "Joline",
    "film_actor": [
        {"film": {"film_id": 19, "title": "Melted"}},
        {"film": {"title": "Frozen"}}
    ]
  })*");

    test_put(root, doc, {{"actor_id", "50"}});
  }
  // empty list
  if (0) {
    auto doc = make_json(R"*({
    "first_name": "Angelica",
    "last_name": "Joline",
    "film_actor": []
  })*");

    test_put(root, doc, {{"actor_id", "51"}});
  }
}