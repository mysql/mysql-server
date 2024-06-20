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
  PrimaryKeyColumnValues test_put(std::shared_ptr<DualityView> root,
                                  const std::string &doc,
                                  const PrimaryKeyColumnValues &pk,
                                  const ObjectRowOwnership &row_owner = {}) {
    mrs::database::dv::DualityViewUpdater rest(root, row_owner);

    return rest.update(m_.get(), pk, make_json(doc), true);
  }

  void expect_put(std::shared_ptr<DualityView> root, const std::string &templ,
                  const PrimaryKeyColumnValues &pk,
                  const ObjectRowOwnership &row_owner = {}) {
    std::string input, expected_output;
    std::vector<int> ids;
    process_template(templ, ids, &input, &expected_output);

    auto out_pk = test_put(root, input, pk, row_owner);

    auto res = select_one(root, out_pk, {}, row_owner);
    EXPECT_EQ(pprint_json(expected_output),
              res.empty() ? res : pprint_json(res))
        << "RESULT:" << res;
  }
};

#define EXPECT_PUT(f, input, pk) \
  do {                           \
    SCOPED_TRACE("");            \
    expect_put(f, input, pk);    \
  } while (0)

#define EXPECT_PUT2(f, input, pk, owner) \
  do {                                   \
    SCOPED_TRACE("");                    \
    expect_put(f, input, pk, owner);     \
  } while (0)

TEST_F(DatabaseQueryPut, etag_check) {}

TEST_F(DatabaseQueryPut, special_types) {
  auto root =
      DualityViewBuilder("mrstestdb", "typetest", TableFlag::WITH_UPDATE)
          .field("id", FieldFlag::PRIMARY)
          .field("Geom", "geom", "GEOMETRY")
          .field("Bool", "bool", "BIT(1)")
          .field("Binary", "bin", "BLOB")
          .field("Json", "js", "JSON")
          .resolve(m_.get(), true);

  test_put(root, (R"*({
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

  auto root_json =
      DualityViewBuilder("mrstestdb", "typetest", TableFlag::WITH_UPDATE)
          .field("id", FieldFlag::PRIMARY)
          .field("Json", "js", "JSON")
          .resolve(m_.get(), true);

  PrimaryKeyColumnValues pk_1({{"id", "1"}});

  EXPECT_PUT(root_json, (R"*({
  "id": 1,
  "Json": []
  <<o:,"_metadata": {"etag": "D0AC8868B4F9A79D86F0F30B3EED8F2043552877F9D01F50B5742CE3898DFBE2"}>>
})*"),
             pk_1);

  EXPECT_PUT(root_json, (R"*({
  "id": 1,
  "Json": null
  <<o:,"_metadata": {"etag": "9F7E9381B9B92091F31BFD7C7DA754D1D9C01A4FD3575F4FC2DCE9C84139FB88"}>>
})*"),
             pk_1);

  EXPECT_PUT(root_json, (R"*({
  "id": 1,
  "Json": ""
  <<o:,"_metadata": {"etag": "43B6CB1CD7F9CB9A11F48C109A6582D935048ED509231A0A38D1060AA606FFC4"}>>
})*"),
             pk_1);
}

TEST_F(DatabaseQueryPut, update_plain_fields) {
  auto root = DualityViewBuilder("mrstestdb", "actor", TableFlag::WITH_UPDATE)
                  .field("actorId", "actor_id", "int",
                         FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("firstName", "first_name", "text")
                  .field("lastName", "last_name", "text")
                  .resolve(m_.get(), true);

  // row already exists
  {
    auto doc = (R"*({
    "actorId": 5,
    "lastName": "Smith",
    "firstName": "Arnold",
    "_metadata": {
      "etag": "2C6A57F4528178F85FA4EE33E2F15E5F20A4CED718F403A732D4A9CA26BEE14B"
    }
  })*");

    expect_put(root, doc, {{"actor_id", "5"}});
  }

  // try to override PK
  {
    auto doc = (R"*({
    "actorId": 123,
    "lastName": "Smith II",
    "firstName": "Arnold"
  })*");

    EXPECT_JSON_ERROR(test_put(root, doc, {{"actor_id", "5"}}),
                      "ID for table `actor` cannot be changed");
  }
}

TEST_F(DatabaseQueryPut, no_pk) {
  auto root =
      DualityViewBuilder("mrstestdb", "country",
                         TableFlag::WITH_UPDATE | TableFlag::WITH_NOCHECK)
          .field("country_id", FieldFlag::PRIMARY)
          .field("country")
          .resolve(m_.get(), true);

  auto doc = (R"*({
    "country": "Testland"
  })*");

  {
    EXPECT_REST_ERROR(test_put(root, doc, {}),
                      "Missing primary key column value for country_id");
  }
  {
    EXPECT_REST_ERROR(test_put(root, doc, {{"country", "Testland"}}),
                      "Missing primary key column value for country_id");
  }
  {
    EXPECT_REST_ERROR(test_put(root, doc, {{"bogus_id", "111"}}),
                      "Missing primary key column value for country_id");
  }
  {
    EXPECT_REST_ERROR(
        test_put(root, doc, {{"country_id", "1"}, {"bogus_id", "111"}}),
        "Invalid primary key column");
  }

  auto root2 =
      DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_UPDATE)
          .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("country")
          .resolve(m_.get(), true);
  {
    EXPECT_REST_ERROR(test_put(root2, doc, {}),
                      "Missing primary key column value for country_id");
  }
}

TEST_F(DatabaseQueryPut, no_pk_multi) {
  auto root = DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_UPDATE)
                  .field("country_id", FieldFlag::PRIMARY)
                  .field("continent_id", FieldFlag::PRIMARY)
                  .field("country")
                  .resolve(m_.get(), true);

  auto doc = (R"*({
    "country": "Testland"
  })*");

  {
    EXPECT_REST_ERROR(test_put(root, doc, {}),
                      "Missing primary key column value for country_id");
  }
  {
    EXPECT_REST_ERROR(test_put(root, doc, {{"country_id", "111"}}),
                      "Missing primary key column value for continent_id");
  }
  {
    EXPECT_REST_ERROR(test_put(root, doc, {{"continent_id", "111"}}),
                      "Missing primary key column value for country_id");
  }
}

TEST_F(DatabaseQueryPut, plain_owner_notpk) {
  prepare_user_metadata();

  auto root =
      DualityViewBuilder("mrstestdb", "t2_base",
                         TableFlag::WITH_UPDATE | TableFlag::WITH_INSERT)
          .field("id", "id", "int", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("owner_id", FieldFlag::OWNER)
          .field("data1", "data1", "text")
          .field("data2", "data2", "int")
          .resolve(m_.get(), true);

  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    expect_put(root, (R"*({
    "id": 2,
    "data1": "Arnold",
    "data2": 42
    <<o:,
    "owner_id": "EREAAAAAAAAAAAAAAAAAAA==",
    "_metadata": {
        "etag": "82B454F07CC4CAFEF073EDD2443E52F86F534985FEDA017B37A671DDC823DBCB"
    }>>
  })*"),
               {{"id", "2"}}, owner);
  }
  // try to put as someone else's row
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    EXPECT_HTTP_ERROR(test_put(root, (R"*({"id":3,
    "owner_id": "IiIAAAAAAAAAAAAAAAAAAA==",
    "data1": "Bla",
    "data2": 12
  })*"),
                               {{"id", "3"}}, owner),
                      403, "Forbidden");
  }
  // allow put own row
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('MzMAAAAAAAAAAAAAAAAAAA==')"));

    expect_put(root,
               R"*({
               "id":4,
               "data1": "Joe",
               "data2": 1,
               "owner_id": "MzMAAAAAAAAAAAAAAAAAAA=="
            <<o:,"_metadata": {
                "etag": "119BDC8DC691079010C9CEA48BA881DF140530B5484F1EBFD6447D74DD5B26A6"
               }>>
            })*",
               {{"id", "4"}}, owner);
  }

  // allow insert new own row
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("FROM_BASE64('MzMAAAAAAAAAAAAAAAAAAA==')"));

    expect_put(root,
               R"*({
               "id":44444,
               "data1": "Joe",
               "data2": 1,
               "owner_id": "MzMAAAAAAAAAAAAAAAAAAA=="
            <<o:,"_metadata": {
                "etag": "DCB5B06E98D5358096B542DA10C5645DC0B0B10E1D91562D40835FDB7803841A"
               }>>
            })*",
               {{"id", "44444"}}, owner);
  }
}

TEST_F(DatabaseQueryPut, plain_owner_pk) {
  prepare(TestSchema::PLAIN);
  prepare_user_metadata();

  // pk = owner
  m_->execute(R"*(INSERT INTO mrstestdb.root_owner (id, data1) VALUES
   (0x11110000000000000000000000000000, 'one'),
   (0x22220000000000000000000000000000, 'two'),
   (0x33330000000000000000000000000000, 'three'))*");

  auto root =
      DualityViewBuilder("mrstestdb", "root_owner",
                         TableFlag::WITH_UPDATE | TableFlag::WITH_INSERT)
          .field("id", FieldFlag::PRIMARY | FieldFlag::OWNER)
          .field("data1", "data1")
          .field_to_one("11", ViewBuilder("child_11").field("id").field("data"))
          .resolve(m_.get(), true);

  // owner_id = PK
  {
    auto owner = ObjectRowOwnership(
        root, "id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    PrimaryKeyColumnValues pk = {
        {"id", "FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"}};

    EXPECT_PUT2(root, R"*({
      "11": {},
      "id":"EREAAAAAAAAAAAAAAAAAAA==", 
      "data1": "AAA",
      "_metadata": {
        "etag": "4097C48083B100F77EC95EAEE6A565CB873F1B2DFD118928F87D2A00565A7D91"
      }
  })*",
                pk, owner);
  }
  // implicit
  {
    auto owner = ObjectRowOwnership(
        root, "id",
        mysqlrouter::sqlstring("FROM_BASE64('IiIAAAAAAAAAAAAAAAAAAA==')"));

    EXPECT_PUT2(root, (R"*({
                <<o:"11": {},>>
                "id":"IiIAAAAAAAAAAAAAAAAAAA==",
                "data1": "BBB"
                <<o:, "_metadata": {
        "etag": "C76EE9F6AF8AAECEFFE9663609DA5BFF043C7A3C785DAC750752258DBA071F3F"
    }>>
          })*"),
                {}, owner);
  }
  // implicit in json too
  {
    auto owner = ObjectRowOwnership(
        root, "id",
        mysqlrouter::sqlstring("FROM_BASE64('IiIAAAAAAAAAAAAAAAAAAA==')"));

    EXPECT_PUT2(root, (R"*({
                <<o:"11": {},
                "id":"IiIAAAAAAAAAAAAAAAAAAA==",>>
                "data1": "BBB"
                <<o:, "_metadata": {
        "etag": "C76EE9F6AF8AAECEFFE9663609DA5BFF043C7A3C785DAC750752258DBA071F3F"
    }>>
          })*"),
                {}, owner);
  }
  // can't insert/update someone else's row
  {
    auto owner = ObjectRowOwnership(
        root, "id",
        mysqlrouter::sqlstring("FROM_BASE64('IiIAAAAAAAAAAAAAAAAAAA==')"));
    EXPECT_HTTP_ERROR(
        test_put(root, (R"*({"data1": "Joe"})*"),
                 {{"id", "FROM_BASE64('MzMAAAAAAAAAAAAAAAAAAA==')"}}, owner),
        403, "Forbidden");
  }
  {
    auto owner = ObjectRowOwnership(
        root, "id",
        mysqlrouter::sqlstring("FROM_BASE64('IiIAAAAAAAAAAAAAAAAAAA==')"));

    PrimaryKeyColumnValues pk = {
        {"id", "FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"}};

    EXPECT_HTTP_ERROR(
        test_put(root, R"*({"id":"EREAAAAAAAAAAAAAAAAAAA==", "data1": "XXX"})*",
                 pk, owner),
        403, "Forbidden");
  }

  m_->execute("delete from mrstestdb.root_owner");
  // insert new
  {
    auto owner = ObjectRowOwnership(
        root, "id",
        mysqlrouter::sqlstring("FROM_BASE64('EREAAAAAAAAAAAAAAAAAAA==')"));

    EXPECT_PUT2(root, R"*({
    "11": {},
    "id": "EREAAAAAAAAAAAAAAAAAAA==",
    "data1": "XXX",
    "_metadata": {
        "etag": "847DC45B6C148BC58A14A5FB4AFAF2494098697B6BABFC9113F0DB3CBF61F812"
    }
})*",
                {}, owner);
  }
}

TEST_F(DatabaseQueryPut, nested_11_multi) {
  auto root =
      DualityViewBuilder("mrstestdb", "tc2_base",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id", FieldFlag::PRIMARY)
          .field("sub_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("data1")
          .field("data2")
          .column("ref_11_id")
          .column("ref_11_sub_id")
          .field_to_one(
              "ref", ViewBuilder("tc2_ref_11", TableFlag::WITH_UPDATE |
                                                   TableFlag::WITH_NOCHECK)
                         .field("id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                         .field("sub_id", FieldFlag::PRIMARY)
                         .field("data"))
          .resolve(m_.get(), true);

  {
    test_put(root, (R"*({
      "id": 222,
      "sub_id": "AB",
      "data1": "AAA",
      "data2": 1,
      "ref": {
        "id": 1,
        "sub_id": "AA"
      }
    })*"),
             {{"id", "222"}, {"sub_id", "'AB'"}});

    EXPECT_ROWS_ADDED("tc2_base", 1);
    EXPECT_ROWS_ADDED("tc2_ref_11", 0);
  }
}

TEST_F(DatabaseQueryPut, nested_n1_ref_child_autoinc) {
  auto root =
      DualityViewBuilder("mrstestdb", "city")
          .field("city_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
          .field("city")
          .column("country_id")
          .field_to_one(
              "country",
              ViewBuilder("country")
                  .field("country_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                  .field("country"))
          .resolve(m_.get(), true);
}
