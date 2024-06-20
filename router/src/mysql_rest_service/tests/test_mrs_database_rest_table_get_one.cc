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

#include <vector>

#include "helper/expect_throw_msg.h"
#include "mock/mock_session.h"
#include "mrs/database/query_rest_table.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;

class DatabaseQueryGetOne : public DatabaseRestTableTest {
 public:
  std::unique_ptr<mrs::database::QueryRestTableSingleRow> rest;

  void SetUp() override {
    DatabaseRestTableTest::SetUp();

    reset();
  }

  void reset() {
    rest = std::make_unique<mrs::database::QueryRestTableSingleRow>(
        nullptr, false, true);
  }
};

TEST_F(DatabaseQueryGetOne, plain) {
  auto root = DualityViewBuilder("mrstestdb", "actor")
                  .field("actor_id", FieldFlag::AUTO_INC)
                  .field("first_name")
                  .field("last_name")
                  .field("last_update")
                  .resolve(m_.get());

  rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id":3})*"), {}, "url",
                    {}, true);
  EXPECT_EQ(
      R"*({
    "links": [
        {
            "rel": "self",
            "href": "url/3"
        }
    ],
    "actor_id": 3,
    "last_name": "CHASE",
    "first_name": "ED",
    "last_update": "2006-02-15 04:34:33.000000",
    "_metadata": {
        "etag": "09028C2BCDEEC5809F7AF68398EF681BE73608124235927EDD283BF9EFA92D5F"
    }
})*",
      pprint_json(rest->response));
}

TEST_F(DatabaseQueryGetOne, nesting) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .field_to_many(
              "films",
              ViewBuilder("film_actor")
                  .column("actor_id")
                  .column("film_id")
                  .field_to_one(
                      "",
                      ViewBuilder("film")
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .field_to_one("language",
                                        ViewBuilder("language")
                                            .column("language_id")
                                            .field("language", "name"),
                                        true, {{"language_id", "language_id"}})
                          .field_to_one(
                              "original_language",
                              ViewBuilder("language")
                                  .column("language_id")
                                  .field("original_language", "name"),
                              true, {{"original_language_id", "language_id"}})
                          .field_to_many(
                              "categories",
                              ViewBuilder("film_category")
                                  .column("film_id")
                                  .field_to_one(
                                      "category",
                                      ViewBuilder("category")
                                          .column("category_id")
                                          .field("name"),
                                      true, {{"category_id", "category_id"}}),
                              true, {{"film_id", "film_id"}}),
                      true),
              false)
          .resolve(m_.get(), true);

  SCOPED_TRACE(root->as_graphql(true));

  {
    auto filter =
        mrs::database::dv::ObjectFieldFilter::from_url_filter(*root, {});

    reset();

    rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id": 3})*"), filter,
                      "url2", {}, true, {});
    EXPECT_EQ(pprint_json(R"*({
  "films": [
    {
      "title": "ACADEMY DINOSAUR",
      "language": "English",
      "categories": [
        "Documentary"
      ],
      "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies",
      "original_language": "Italian"
    },
    {
      "title": "AFFAIR PREJUDICE",
      "language": "English",
      "categories": [
        "Horror"
      ],
      "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank",
      "original_language": "Japanese"
    }
  ],
  "links": [
    {
      "rel": "self",
      "href": "url2/3"
    }
  ],
  "first_name": "ED",
  "_metadata": {
    "etag": "B7A2A6E1A04D722D361349FAA81CD751782473E7652F24F6E55E6EAB3E5AC3A0"
  }
})*"),
              pprint_json(rest->response));
  }
}

TEST_F(DatabaseQueryGetOne, exclude_field_filter) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .field("last_name")
          .field_to_many(
              "films",
              ViewBuilder("film_actor")
                  .column("actor_id")
                  .column("film_id")
                  .field_to_one(
                      "",
                      ViewBuilder("film")
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .field_to_one("language",
                                        ViewBuilder("language")
                                            .column("language_id")
                                            .field("language", "name"),
                                        true, {{"language_id", "language_id"}})
                          .field_to_one(
                              "original_language",
                              ViewBuilder("language")
                                  .column("language_id")
                                  .field("original_language", "name"),
                              true, {{"original_language_id", "language_id"}})
                          .field_to_many(
                              "categories",
                              ViewBuilder("film_category")
                                  .column("film_id")
                                  .field_to_one(
                                      "category",
                                      ViewBuilder("category")
                                          .column("category_id")
                                          .field("name"),
                                      true, {{"category_id", "category_id"}}),
                              true, {{"film_id", "film_id"}}),
                      true),
              false)
          .resolve(m_.get(), true);

  SCOPED_TRACE(root->as_graphql(true));

  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"!first_name", "!films.title", "!films.language",
                "!films.categories"});

    reset();

    rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id": 3})*"), filter,
                      "url2", {}, true, {});
    EXPECT_EQ(pprint_json(R"*({
  "films": [
    {
      "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies",
      "original_language": "Italian"
    },
    {
      "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank",
      "original_language": "Japanese"
    }
  ],
  "links": [
    {
      "rel": "self",
      "href": "url2/3"
    }
  ],
  "last_name": "CHASE",
  "_metadata": {
    "etag": "1CF9834269C42A1C555390FAD397A4788F25C271327AB421A7C21DCD4FA56A6C"
  }
})*"),
              pprint_json(rest->response));
  }
}

TEST_F(DatabaseQueryGetOne, include_field_filter) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .field("last_name")
          .field_to_many(
              "films",
              ViewBuilder("film_actor")
                  .column("actor_id")
                  .column("film_id")
                  .field_to_one(
                      "",
                      ViewBuilder("film")
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .field_to_one("language",
                                        ViewBuilder("language")
                                            .column("language_id")
                                            .field("language", "name"),
                                        true, {{"language_id", "language_id"}})
                          .field_to_one(
                              "original_language",
                              ViewBuilder("language")
                                  .column("language_id")
                                  .field("original_language", "name"),
                              true, {{"original_language_id", "language_id"}})
                          .field_to_many(
                              "categories",
                              ViewBuilder("film_category")
                                  .column("film_id")
                                  .field_to_one(
                                      "category",
                                      ViewBuilder("category")
                                          .column("category_id")
                                          .field("name"),
                                      true, {{"category_id", "category_id"}}),
                              true, {{"film_id", "film_id"}}),
                      true),
              false)
          .resolve(m_.get(), true);

  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root,
        {"first_name", "films.title", "films.language", "films.categories"});

    reset();

    rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id": 3})*"), filter,
                      "url2", {}, true);
    EXPECT_EQ(
        R"*({
    "films": [
        {
            "title": "ACADEMY DINOSAUR",
            "language": "English",
            "categories": [
                "Documentary"
            ]
        },
        {
            "title": "AFFAIR PREJUDICE",
            "language": "English",
            "categories": [
                "Horror"
            ]
        }
    ],
    "links": [
        {
            "rel": "self",
            "href": "url2/3"
        }
    ],
    "first_name": "ED",
    "_metadata": {
        "etag": "1CF9834269C42A1C555390FAD397A4788F25C271327AB421A7C21DCD4FA56A6C"
    }
})*",
        pprint_json(rest->response));
  }
  {
    auto filter =
        mrs::database::dv::ObjectFieldFilter::from_url_filter(*root, {"films"});

    reset();

    rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id": 3})*"), filter,
                      "url3", {}, true);
    EXPECT_EQ(
        R"*({
    "films": [
        {
            "title": "ACADEMY DINOSAUR",
            "language": "English",
            "categories": [
                "Documentary"
            ],
            "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies",
            "original_language": "Italian"
        },
        {
            "title": "AFFAIR PREJUDICE",
            "language": "English",
            "categories": [
                "Horror"
            ],
            "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank",
            "original_language": "Japanese"
        }
    ],
    "links": [
        {
            "rel": "self",
            "href": "url3/3"
        }
    ],
    "_metadata": {
        "etag": "1CF9834269C42A1C555390FAD397A4788F25C271327AB421A7C21DCD4FA56A6C"
    }
})*",
        pprint_json(rest->response));
  }
  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"films.title"});

    reset();

    rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id": 3})*"), filter,
                      "url4", {}, true);
    EXPECT_EQ(
        R"*({
    "films": [
        {
            "title": "ACADEMY DINOSAUR"
        },
        {
            "title": "AFFAIR PREJUDICE"
        }
    ],
    "links": [
        {
            "rel": "self",
            "href": "url4/3"
        }
    ],
    "_metadata": {
        "etag": "1CF9834269C42A1C555390FAD397A4788F25C271327AB421A7C21DCD4FA56A6C"
    }
})*",
        pprint_json(rest->response));
  }
  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"films.categories"});

    reset();

    rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id": 3})*"), filter,
                      "url5", {}, true);
    EXPECT_EQ(
        R"*({
    "films": [
        {
            "categories": [
                "Documentary"
            ]
        },
        {
            "categories": [
                "Horror"
            ]
        }
    ],
    "links": [
        {
            "rel": "self",
            "href": "url5/3"
        }
    ],
    "_metadata": {
        "etag": "1CF9834269C42A1C555390FAD397A4788F25C271327AB421A7C21DCD4FA56A6C"
    }
})*",
        pprint_json(rest->response));
  }
  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"films.original_language", "films.title"});

    reset();

    rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id": 3})*"), filter,
                      "url6", {}, true);
    EXPECT_EQ(
        R"*({
    "films": [
        {
            "title": "ACADEMY DINOSAUR",
            "original_language": "Italian"
        },
        {
            "title": "AFFAIR PREJUDICE",
            "original_language": "Japanese"
        }
    ],
    "links": [
        {
            "rel": "self",
            "href": "url6/3"
        }
    ],
    "_metadata": {
        "etag": "1CF9834269C42A1C555390FAD397A4788F25C271327AB421A7C21DCD4FA56A6C"
    }
})*",
        pprint_json(rest->response));
  }
  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"first_name", "films.film_id"});
    // ignore unknown fields
    reset();

    rest->query_entry(m_.get(), root, parse_pk(R"*({"actor_id": 3})*"), filter,
                      "url7", {}, true);
    EXPECT_EQ(
        R"*({
    "films": [
        {},
        {}
    ],
    "links": [
        {
            "rel": "self",
            "href": "url7/3"
        }
    ],
    "first_name": "ED",
    "_metadata": {
        "etag": "1CF9834269C42A1C555390FAD397A4788F25C271327AB421A7C21DCD4FA56A6C"
    }
})*",
        pprint_json(rest->response));
  }
}

#if 0
TEST_F(DatabaseQueryGetOne, row_filter) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("firstName", "first_name", "text")
          .field("lastName", "last_name", "text", FieldFlag::NOFILTER)
          .field_to_many(
              "films",
              DualityViewBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(
                      DualityViewBuilder("film", {{"film_id", "film_id"}})
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .column("language_id")
                          .unnest(
                              DualityViewBuilder(
                                  "language", {{"language_id", "language_id"}})
                                  .column("language_id")
                                  .field("language", "name"))
                          .field_to_many(
                              "categories",
                              DualityViewBuilder("film_category",
                                                 {{"film_id", "film_id"}})
                                  .column("film_id")
                                  .column("category_id")
                                  .unnest(DualityViewBuilder(
                                              "category",
                                              {{"category_id", "category_id"}})
                                              .column("category_id")
                                              .field("category", "name")))));

  {
    rest->query_entry(m_.get(), root, {}, 0, 5, "url", true, {},
                        filter(root, R"*({"firstName": "PENELOPE"})*"));

    auto json = make_json(rest->response);
    EXPECT_EQ(1, json["items"].GetArray().Size());
  }
  {
    reset();

    EXPECT_REST_ERROR(
        rest->query_entry(
            m_.get(), root, {}, 0, 5, "url", true, {},
            filter(root,
                   R"*({"firstName": "PENELOPE", "lastName": "SMITH"})*")),
        "Cannot filter on field lastName");
  }
  {
    reset();

    EXPECT_REST_ERROR(
        rest->query_entry(m_.get(), root, {}, 0, 5, "url", true, {},
                            filter(root, R"*({"invalid_field": "HOORAY"})*")),
        "Cannot filter on field invalid_field");
  }
}

TEST_F(DatabaseQueryGetOne, row_filter_order) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor")
          .field("id", "actor_id", "int",
                 FieldFlag::PRIMARY | FieldFlag::SORTABLE)
          .field("firstName", "first_name", "text",
                 FieldFlag::UNIQUE | FieldFlag::SORTABLE)
          .field("lastName", "last_name", "text", FieldFlag::NOFILTER)
          .field_to_many(
              "films",
              DualityViewBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(
                      DualityViewBuilder("film", {{"film_id", "film_id"}})
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .column("language_id")
                          .unnest(
                              DualityViewBuilder(
                                  "language", {{"language_id", "language_id"}})
                                  .column("language_id")
                                  .field("language", "name"))
                          .field_to_many(
                              "categories",
                              DualityViewBuilder("film_category",
                                                 {{"film_id", "film_id"}})
                                  .column("film_id")
                                  .column("category_id")
                                  .unnest(DualityViewBuilder(
                                              "category",
                                              {{"category_id", "category_id"}})
                                              .column("category_id")
                                              .field("category", "name")))));

  {
    rest->query_entry(m_.get(), root, {}, 0, 5, "url", true, {},
                        filter(root, R"*({"$orderby": {"id": 1}})*"));

    auto json = make_json(rest->response);
    EXPECT_EQ(5, json["items"].GetArray().Size());
    EXPECT_EQ(1, json["items"][0]["id"].GetInt());
    EXPECT_EQ(2, json["items"][1]["id"].GetInt());
    EXPECT_EQ(3, json["items"][2]["id"].GetInt());
    EXPECT_EQ(4, json["items"][3]["id"].GetInt());
    EXPECT_EQ(5, json["items"][4]["id"].GetInt());
  }
  {
    reset();

    rest->query_entry(m_.get(), root, {}, 0, 5, "url", true, {},
                        filter(root, R"*({"$orderby": {"firstName": -1}})*"));

    auto json = make_json(rest->response);
    EXPECT_EQ(5, json["items"].GetArray().Size()) << rest->response;
    EXPECT_EQ(11, json["items"][0]["id"].GetInt());
    EXPECT_EQ(1, json["items"][1]["id"].GetInt());
    EXPECT_EQ(2, json["items"][2]["id"].GetInt());
    EXPECT_EQ(8, json["items"][3]["id"].GetInt());
    EXPECT_EQ(5, json["items"][4]["id"].GetInt());
  }
  {
    reset();

    EXPECT_REST_ERROR(
        rest->query_entry(m_.get(), root, {}, 0, 5, "url", true, {},
                            filter(root, R"*({"$orderby": {"lastName": 1}})*")),
        "Cannot sort on field lastName");
  }
  {
    reset();

    EXPECT_REST_ERROR(
        rest->query_entry(
            m_.get(), root, {}, 0, 5, "url", true, {},
            filter(root, R"*({"$orderby": {"invalid_field": 1}})*")),
        "Cannot sort on field invalid_field");
  }
}
#endif

TEST_F(DatabaseQueryGetOne, row_owner_root) {
  prepare(TestSchema::AUTO_INC);
  prepare_user_metadata();

  auto root =
      DualityViewBuilder("mrstestdb", "root", 0)
          .field("_id", "id", FieldFlag::AUTO_INC)
          .field("owner_id", FieldFlag::OWNER)
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", 0)
                            .field("child11Id", "id", FieldFlag::AUTO_INC)
                            .field("child11Data", "data"))
          .field_to_many("child1n",
                         ViewBuilder("child_1n", 0)
                             .field("chld1nId", "id", FieldFlag::AUTO_INC)
                             .field("child1nData", "data"))
          .field_to_many(
              "childnm",
              ViewBuilder("child_nm_join", 0)
                  .field("nmRootId", "root_id")
                  .field("nmChildId", "child_id")
                  .field_to_one("child", ViewBuilder("child_nm", 0)
                                             .field("childnmId", "id",
                                                    FieldFlag::AUTO_INC)))
          .resolve(m_.get());
  SCOPED_TRACE(root->as_graphql());
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("0x11110000000000000000000000000000"));

    // belongs to owner
    rest->query_entry(m_.get(), root, {{"id", "1"}}, {}, "url", owner);
    EXPECT_EQ(1, make_json(rest->response)["_id"].GetInt());
    EXPECT_STREQ("data1", make_json(rest->response)["data"].GetString());
  }
}