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

// TODO
// - composite keys
// - nested join
// - s/base/nested/
// - 1:1
// - 1:n
// - n:m
// - reduce with value
// - 2 subqueries
// - 2 joins
// - allowed crud operation check

// inserts
// - PK - auto-inc / single / composite

FilterObjectGenerator filter(std::shared_ptr<Object> obj,
                             const char *filter_query) {
  FilterObjectGenerator result{obj, true, 0};
  result.parse(filter_query);

  return result;
}

class DatabaseQueryGet : public DatabaseRestTableTest {
 public:
  std::unique_ptr<mrs::database::QueryRestTable> rest;

  void SetUp() override {
    DatabaseRestTableTest::SetUp();

    rest = std::make_unique<mrs::database::QueryRestTable>();
  }

  void reset() { rest = std::make_unique<mrs::database::QueryRestTable>(); }
};

TEST_F(DatabaseQueryGet, plain) {
  auto root = DualityViewBuilder("mrstestdb", "actor")
                  .field("actor_id", FieldFlag::AUTO_INC)
                  .field("first_name")
                  .field("last_name")
                  .field("last_update")
                  .resolve(m_.get());

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
  // std::cout << pprint_json(rest->response) << "\n";
  EXPECT_EQ(
      R"*({
    "items": [
        {
            "links": [
                {
                    "rel": "self",
                    "href": "url/1"
                }
            ],
            "actor_id": 1,
            "last_name": "GUINESS",
            "first_name": "PENELOPE",
            "last_update": "2006-02-15 04:34:33.000000"
        },
        {
            "links": [
                {
                    "rel": "self",
                    "href": "url/2"
                }
            ],
            "actor_id": 2,
            "last_name": "WAHLBERG",
            "first_name": "NICK",
            "last_update": "2006-02-15 04:34:33.000000"
        },
        {
            "links": [
                {
                    "rel": "self",
                    "href": "url/3"
                }
            ],
            "actor_id": 3,
            "last_name": "CHASE",
            "first_name": "ED",
            "last_update": "2006-02-15 04:34:33.000000"
        }
    ],
    "limit": 3,
    "offset": 0,
    "hasMore": true,
    "count": 3,
    "links": [
        {
            "rel": "self",
            "href": "url/"
        },
        {
            "rel": "next",
            "href": "url/?offset=3"
        }
    ]
})*",
      pprint_json(rest->response));
}

TEST_F(DatabaseQueryGet, special_types) {
  auto root = DualityViewBuilder("mrstestdb", "typetest")
                  .field("id", FieldFlag::PRIMARY)
                  .field("Geom", "geom", "GEOMETRY")
                  .field("Bool", "bool", "BIT(1)")
                  .field("Binary", "bin", "BLOB")
                  .field("Json", "js", "JSON")
                  .resolve(m_.get());

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});

  EXPECT_EQ(R"*({
    "items": [
        {
            "id": 1,
            "Bool": true,
            "Geom": {
                "type": "Point",
                "coordinates": [
                    95.3884368,
                    21.4600272
                ]
            },
            "Json": {
                "a": 1
            },
            "links": [
                {
                    "rel": "self",
                    "href": "url/1"
                }
            ],
            "Binary": "aGVsbG8="
        }
    ],
    "limit": 3,
    "offset": 0,
    "hasMore": false,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url/"
        }
    ]
})*",
            pprint_json(rest->response));
}

TEST_F(DatabaseQueryGet, exclude_field_filter) {
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

  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root,
        {"!last_name", "!films.title", "!films.language", "!films.categories"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url2", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies",
                    "original_language": "Italian"
                },
                {
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory"
                },
                {
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico"
                },
                {
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China"
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url2/1"
                }
            ],
            "first_name": "PENELOPE"
        }
    ],
    "limit": 1,
    "offset": 0,
    "hasMore": true,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url2/"
        },
        {
            "rel": "next",
            "href": "url2/?offset=1"
        }
    ]
})*",
        pprint_json(rest->response));
  }
}

TEST_F(DatabaseQueryGet, include_field_filter) {
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
                      "x",
                      ViewBuilder("film")
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .field_to_one("x1",
                                        ViewBuilder("language")
                                            .column("language_id")
                                            .field("language", "name"),
                                        true, {{"language_id", "language_id"}})
                          .field_to_one(
                              "x2",
                              ViewBuilder("language")
                                  .column("language_id")
                                  .field("original_language", "name"),
                              true, {{"original_language_id", "language_id"}})
                          .field_to_many(
                              "categories",
                              ViewBuilder("film_category")
                                  .column("film_id")
                                  .column("category_id")
                                  .field_to_one(
                                      "x",
                                      ViewBuilder("category")
                                          .column("category_id")
                                          .field("name"),
                                      true, {{"category_id", "category_id"}}),
                              true, {{"film_id", "film_id"}}),
                      true, {{"film_id", "film_id"}}),
              false, {{"actor_id", "actor_id"}})
          .resolve(m_.get(), true);

  SCOPED_TRACE(root->as_graphql(true));

  {
    rest->query_entries(m_.get(), root, {}, 0, 1, "url1", true, {}, {});

    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "language": "English",
                    "original_language": "Italian",
                    "title": "ACADEMY DINOSAUR",
                    "categories": [
                        "Documentary"
                    ],
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies"
                },
                {
                    "language": "English",
                    "title": "ADAPTATION HOLES",
                    "categories": [
                        "Documentary",
                        "Drama"
                    ],
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory"
                },
                {
                    "language": "English",
                    "title": "AFRICAN EGG",
                    "categories": [
                        "Family"
                    ],
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico"
                },
                {
                    "language": "English",
                    "title": "ALADDIN CALENDAR",
                    "categories": [
                        "Sports"
                    ],
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China"
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url1/1"
                }
            ],
            "first_name": "PENELOPE"
        }
    ],
    "limit": 1,
    "offset": 0,
    "hasMore": true,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url1/"
        },
        {
            "rel": "next",
            "href": "url1/?offset=1"
        }
    ]
})*",
        pprint_json(rest->response));
  }

  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root,
        {"first_name", "films.title", "films.language", "films.categories"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url2", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "language": "English",
                    "title": "ACADEMY DINOSAUR",
                    "categories": [
                        "Documentary"
                    ]
                },
                {
                    "language": "English",
                    "title": "ADAPTATION HOLES",
                    "categories": [
                        "Documentary",
                        "Drama"
                    ]
                },
                {
                    "language": "English",
                    "title": "AFRICAN EGG",
                    "categories": [
                        "Family"
                    ]
                },
                {
                    "language": "English",
                    "title": "ALADDIN CALENDAR",
                    "categories": [
                        "Sports"
                    ]
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url2/1"
                }
            ],
            "first_name": "PENELOPE"
        }
    ],
    "limit": 1,
    "offset": 0,
    "hasMore": true,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url2/"
        },
        {
            "rel": "next",
            "href": "url2/?offset=1"
        }
    ]
})*",
        pprint_json(rest->response));
  }

  {
    auto filter =
        mrs::database::dv::ObjectFieldFilter::from_url_filter(*root, {"films"});

    reset();

    // auto query = build_select_json_object(root, filter);
    rest->query_entries(m_.get(), root, filter, 0, 1, "url3", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "language": "English",
                    "original_language": "Italian",
                    "title": "ACADEMY DINOSAUR",
                    "categories": [
                        "Documentary"
                    ],
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies"
                },
                {
                    "language": "English",
                    "title": "ADAPTATION HOLES",
                    "categories": [
                        "Documentary",
                        "Drama"
                    ],
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory"
                },
                {
                    "language": "English",
                    "title": "AFRICAN EGG",
                    "categories": [
                        "Family"
                    ],
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico"
                },
                {
                    "language": "English",
                    "title": "ALADDIN CALENDAR",
                    "categories": [
                        "Sports"
                    ],
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China"
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url3/1"
                }
            ]
        }
    ],
    "limit": 1,
    "offset": 0,
    "hasMore": true,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url3/"
        },
        {
            "rel": "next",
            "href": "url3/?offset=1"
        }
    ]
})*",
        pprint_json(rest->response));
  }

  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"films.title"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url4", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "title": "ACADEMY DINOSAUR"
                },
                {
                    "title": "ADAPTATION HOLES"
                },
                {
                    "title": "AFRICAN EGG"
                },
                {
                    "title": "ALADDIN CALENDAR"
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url4/1"
                }
            ]
        }
    ],
    "limit": 1,
    "offset": 0,
    "hasMore": true,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url4/"
        },
        {
            "rel": "next",
            "href": "url4/?offset=1"
        }
    ]
})*",
        pprint_json(rest->response));
  }

  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"films.categories"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url5", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "categories": [
                        "Documentary"
                    ]
                },
                {
                    "categories": [
                        "Documentary",
                        "Drama"
                    ]
                },
                {
                    "categories": [
                        "Family"
                    ]
                },
                {
                    "categories": [
                        "Sports"
                    ]
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url5/1"
                }
            ]
        }
    ],
    "limit": 1,
    "offset": 0,
    "hasMore": true,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url5/"
        },
        {
            "rel": "next",
            "href": "url5/?offset=1"
        }
    ]
})*",
        pprint_json(rest->response));
  }

  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"films.original_language", "films.title"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url6", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "original_language": "Italian",
                    "title": "ACADEMY DINOSAUR"
                },
                {
                    "title": "ADAPTATION HOLES"
                },
                {
                    "title": "AFRICAN EGG"
                },
                {
                    "title": "ALADDIN CALENDAR"
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url6/1"
                }
            ]
        }
    ],
    "limit": 1,
    "offset": 0,
    "hasMore": true,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url6/"
        },
        {
            "rel": "next",
            "href": "url6/?offset=1"
        }
    ]
})*",
        pprint_json(rest->response));
  }
  {
    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"first_name", "films.film_id"});
    // ignore unknown fields
    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url7", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {},
                {},
                {},
                {}
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url7/1"
                }
            ],
            "first_name": "PENELOPE"
        }
    ],
    "limit": 1,
    "offset": 0,
    "hasMore": true,
    "count": 1,
    "links": [
        {
            "rel": "self",
            "href": "url7/"
        },
        {
            "rel": "next",
            "href": "url7/?offset=1"
        }
    ]
})*",
        pprint_json(rest->response));
  }
}
#if 0

TEST_F(DatabaseQueryGet, row_filter) {
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
    rest->query_entries(m_.get(), root, {}, 0, 5, "url", true, {},
                        filter(root, R"*({"firstName": "PENELOPE"})*"));

    auto json = make_json(rest->response);
    EXPECT_EQ(1, json["items"].GetArray().Size());
  }
  {
    reset();

    EXPECT_REST_ERROR(
        rest->query_entries(
            m_.get(), root, {}, 0, 5, "url", true, {},
            filter(root,
                   R"*({"firstName": "PENELOPE", "lastName": "SMITH"})*")),
        "Cannot filter on field lastName");
  }
  {
    reset();

    EXPECT_REST_ERROR(
        rest->query_entries(m_.get(), root, {}, 0, 5, "url", true, {},
                            filter(root, R"*({"invalid_field": "HOORAY"})*")),
        "Cannot filter on field invalid_field");
  }
}

TEST_F(DatabaseQueryGet, row_filter_order) {
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
    rest->query_entries(m_.get(), root, {}, 0, 5, "url", true, {},
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

    rest->query_entries(m_.get(), root, {}, 0, 5, "url", true, {},
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
        rest->query_entries(m_.get(), root, {}, 0, 5, "url", true, {},
                            filter(root, R"*({"$orderby": {"lastName": 1}})*")),
        "Cannot sort on field lastName");
  }
  {
    reset();

    EXPECT_REST_ERROR(
        rest->query_entries(
            m_.get(), root, {}, 0, 5, "url", true, {},
            filter(root, R"*({"$orderby": {"invalid_field": 1}})*")),
        "Cannot sort on field invalid_field");
  }
}
#endif

TEST_F(DatabaseQueryGet, etag) {
  {
    auto root = DualityViewBuilder("mrstestdb", "actor")
                    .field("actor_id", FieldFlag::PRIMARY)
                    .field("first_name")
                    .field("last_name", FieldFlag::WITH_NOCHECK)
                    .field_to_many(
                        "film_actor",
                        ViewBuilder("film_actor")
                            .field("actor_id", FieldFlag::PRIMARY)
                            .field("film_id", FieldFlag::PRIMARY)
                            .field_to_one("film", ViewBuilder("film")
                                                      .field("film_id",
                                                             FieldFlag::PRIMARY)
                                                      .field("title")
                                                      .field("description")))
                    .resolve(m_.get(), true);

    rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {}, true);

    auto json = make_json(rest->response);
    EXPECT_EQ(3, json["items"].GetArray().Size()) << rest->response;

    EXPECT_EQ(1, json["items"][0]["actor_id"].GetInt());
    EXPECT_STREQ(
        "31F155BCEC8184E8879158E1315EA9CD9D957F0AA03685A7A8B34332605F5EE8",
        json["items"][0]["_metadata"]["etag"].GetString());

    EXPECT_EQ(2, json["items"][1]["actor_id"].GetInt());
    EXPECT_STREQ(
        "BC9F16918BFEB2D41FE43FE423EBC6F6288259873E786A03DA9207DE7346F619",
        json["items"][1]["_metadata"]["etag"].GetString());

    EXPECT_EQ(3, json["items"][2]["actor_id"].GetInt());
    EXPECT_STREQ(
        "29D4C6C251FAE21B6D4E20B43C2FDBFF8276B81E60CDA2C0C407852FD61F86AD",
        json["items"][2]["_metadata"]["etag"].GetString());

    reset();

    auto filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"actor_id", "first_name"});

    rest->query_entries(m_.get(), root, filter, 0, 1, "url", true, {}, {},
                        true);
    json = make_json(rest->response);
    EXPECT_EQ(1, json["items"].GetArray().Size()) << rest->response;
    EXPECT_EQ(1, json["items"][0]["actor_id"].GetInt());
    // should be unchanged despite the different field filter
    EXPECT_STREQ(
        "31F155BCEC8184E8879158E1315EA9CD9D957F0AA03685A7A8B34332605F5EE8",
        json["items"][0]["_metadata"]["etag"].GetString());
  }
  {
    reset();

    auto root = DualityViewBuilder("mrstestdb", "actor")
                    .field("actor_id", FieldFlag::PRIMARY)
                    .field("first_name")
                    .field("last_name")
                    .field_to_many(
                        "film_actor",
                        ViewBuilder("film_actor")
                            .field("actor_id", FieldFlag::PRIMARY)
                            .field("film_id", FieldFlag::PRIMARY)
                            .field_to_one("film", ViewBuilder("film")
                                                      .field("film_id",
                                                             FieldFlag::PRIMARY)
                                                      .field("title")
                                                      .field("description")))
                    .resolve(m_.get(), true);

    rest->query_entries(m_.get(), root, {}, 0, 1, "url", true, {}, {}, true);

    auto json = make_json(rest->response);
    EXPECT_EQ(1, json["items"].GetArray().Size()) << rest->response;

    EXPECT_EQ(1, json["items"][0]["actor_id"].GetInt());
    EXPECT_STREQ(
        "C6EDA4EE7C15BAFB6921847822A9F8926DB6E4115B2893CEBD238B628B0D21B3",
        json["items"][0]["_metadata"]["etag"].GetString());
  }

  {
    reset();

    auto root = DualityViewBuilder("mrstestdb", "typetest")
                    .field("id", FieldFlag::PRIMARY)
                    .field("Geom", "geom", "GEOMETRY")
                    .field("Bool", "bool", "BIT(1)")
                    .field("Binary", "bin", "BLOB")
                    .field("Json", "js", "JSON")
                    .resolve(m_.get(), true);

    rest->query_entries(m_.get(), root, {}, 0, 1, "url", true, {}, {}, true);

    auto json = make_json(rest->response);
    EXPECT_EQ(1, json["items"].GetArray().Size()) << rest->response;

    EXPECT_EQ(1, json["items"][0]["id"].GetInt());
    EXPECT_STREQ(
        "FCA79725A9EEE5CD52808D83E74402102BA32004E5D07817010C412E66380A93",
        json["items"][0]["_metadata"]["etag"].GetString());
  }
}

TEST_F(DatabaseQueryGet, row_owner_root) {
  // only root object has owner_id

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

    rest->query_entries(m_.get(), root, {}, 0, 10, "url", true, owner, {},
                        true);

    auto json = make_json(rest->response);
    EXPECT_EQ(5, json["items"].GetArray().Size())
        << pprint_json(rest->response);
    for (const auto &item : json["items"].GetArray()) {
      EXPECT_TRUE(
          strcmp(item["owner_id"].GetString(), "EREAAAAAAAAAAAAAAAAAAA==") == 0)
          << item["owner_id"].GetString();
    }
  }
  {
    auto owner = ObjectRowOwnership(
        root, "owner_id",
        mysqlrouter::sqlstring("0x00000000000000000000000000000000"));

    rest->query_entries(m_.get(), root, {}, 0, 10, "url", true, owner, {},
                        true);

    auto json = make_json(rest->response);
    EXPECT_EQ(0, json["items"].GetArray().Size())
        << pprint_json(rest->response);
  }
}