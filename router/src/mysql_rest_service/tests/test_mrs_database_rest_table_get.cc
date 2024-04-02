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

TEST_F(DatabaseQueryGet, bad_metadata) {
  // no columns
  auto root = ObjectBuilder("mrstestdb", "actor").column("first_name");

  EXPECT_THROW_MSG(
      rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {}),
      std::invalid_argument, "Invalid object metadata");
}

TEST_F(DatabaseQueryGet, plain) {
  auto root = ObjectBuilder("mrstestdb", "actor")
                  .field("first_name")
                  .field("last_name")
                  .field("last_update");

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
  // std::cout << pprint_json(rest->response) << "\n";
  EXPECT_EQ(
      R"*({
    "items": [
        {
            "links": [],
            "last_name": "GUINESS",
            "first_name": "PENELOPE",
            "last_update": "2006-02-15 04:34:33.000000"
        },
        {
            "links": [],
            "last_name": "WAHLBERG",
            "first_name": "NICK",
            "last_update": "2006-02-15 04:34:33.000000"
        },
        {
            "links": [],
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
  auto root = ObjectBuilder("mrstestdb", "typetest")
                  .field("id", FieldFlag::PRIMARY)
                  .field("Geom", "geom", "GEOMETRY")
                  .field("Bool", "bool", "BIT(1)")
                  .field("Binary", "bin", "BLOB")
                  .field("Json", "js", "JSON");

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

// nested n:1 reference in base object
TEST_F(DatabaseQueryGet, nested_n1_base) {
  auto root = ObjectBuilder("mrstestdb", "city")
                  .field("city_id", FieldFlag::PRIMARY)
                  .field("city")
                  .field("country_id")
                  .nest("country",
                        ObjectBuilder("country", {{"country_id", "country_id"}})
                            .field("country_id", FieldFlag::PRIMARY)
                            .field("country"));

  {
    rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
    EXPECT_EQ(R"*({
    "items": [
        {
            "city": "Kabul",
            "links": [
                {
                    "rel": "self",
                    "href": "url/251"
                }
            ],
            "city_id": 251,
            "country": {
                "country": "Afghanistan",
                "country_id": 1
            },
            "country_id": 1
        },
        {
            "city": "Tafuna",
            "links": [
                {
                    "rel": "self",
                    "href": "url/516"
                }
            ],
            "city_id": 516,
            "country": {
                "country": "American Samoa",
                "country_id": 3
            },
            "country_id": 3
        },
        {
            "city": "Benguela",
            "links": [
                {
                    "rel": "self",
                    "href": "url/67"
                }
            ],
            "city_id": 67,
            "country": {
                "country": "Angola",
                "country_id": 4
            },
            "country_id": 4
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
}

// nested 1:1 reference in base object (composite key)
TEST_F(DatabaseQueryGet, nested_n1c_base) {
  auto root = ObjectBuilder("mrstestdb", "store")
                  .field("store_id", FieldFlag::PRIMARY)
                  .field("city_id", FieldFlag::PRIMARY)
                  .field("city_country_id")
                  .nest("city", ObjectBuilder(
                                    "city", {{"city_country_id", "country_id"},
                                             {"city_id", "city_id"}})
                                    .field("country_id")
                                    .field("city_id", FieldFlag::PRIMARY)
                                    .field("city"));

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
  EXPECT_EQ(R"*({
    "items": [
        {
            "city": {
                "city": "Tafuna",
                "city_id": 516,
                "country_id": 3
            },
            "links": [
                {
                    "rel": "self",
                    "href": "url/1,516"
                }
            ],
            "city_id": 516,
            "store_id": 1,
            "city_country_id": 3
        },
        {
            "city": {
                "city": "Tafuna",
                "city_id": 516,
                "country_id": 3
            },
            "links": [
                {
                    "rel": "self",
                    "href": "url/5,516"
                }
            ],
            "city_id": 516,
            "store_id": 5,
            "city_country_id": 3
        },
        {
            "city": {
                "city": "South Hill",
                "city_id": 493,
                "country_id": 5
            },
            "links": [
                {
                    "rel": "self",
                    "href": "url/4,493"
                }
            ],
            "city_id": 493,
            "store_id": 4,
            "city_country_id": 5
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

// unnested n:1 reference in base object
TEST_F(DatabaseQueryGet, unnested_n1_base) {
  auto root =
      ObjectBuilder("mrstestdb", "city")
          .field("city_id", FieldFlag::PRIMARY)
          .field("country_id", FieldFlag::PRIMARY | FieldFlag::DISABLED)
          .field("city")
          .unnest(ObjectBuilder("country", {{"country_id", "country_id"}})
                      .field("country_id", FieldFlag::PRIMARY)
                      .field("country"));

  {
    rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
    EXPECT_EQ(R"*({
    "items": [
        {
            "city": "Kabul",
            "links": [
                {
                    "rel": "self",
                    "href": "url/251,1"
                }
            ],
            "city_id": 251,
            "country": "Afghanistan",
            "country_id": 1
        },
        {
            "city": "Tafuna",
            "links": [
                {
                    "rel": "self",
                    "href": "url/516,3"
                }
            ],
            "city_id": 516,
            "country": "American Samoa",
            "country_id": 3
        },
        {
            "city": "Benguela",
            "links": [
                {
                    "rel": "self",
                    "href": "url/67,4"
                }
            ],
            "city_id": 67,
            "country": "Angola",
            "country_id": 4
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
}

// unnested n:1 reference in base object (composite key)
TEST_F(DatabaseQueryGet, unnested_n1c_base) {
  auto root =
      ObjectBuilder("mrstestdb", "store")
          .field("store_id")
          .column("city_id")
          .column("city_country_id")
          .unnest(ObjectBuilder("city", {{"city_country_id", "country_id"},
                                         {"city_id", "city_id"}})
                      .field("city")
                      .column("city_id")
                      .column("country_id"));

  // SELECT
  {
    rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
    EXPECT_EQ(R"*({
    "items": [
        {
            "city": "Tafuna",
            "links": [],
            "store_id": 1
        },
        {
            "city": "Tafuna",
            "links": [],
            "store_id": 5
        },
        {
            "city": "South Hill",
            "links": [],
            "store_id": 4
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
}

// unnested 1:n reference in base object - invalid
TEST_F(DatabaseQueryGet, unnested_1n_base) {
  // skip - validation done when querying metadata
}

// nested 1:n reference in base object
TEST_F(DatabaseQueryGet, nested_1n_base) {
  auto root =
      ObjectBuilder("mrstestdb", "country")
          .field("country")
          .field("country_id", FieldFlag::DISABLED)
          .nest_list("cities",
                     ObjectBuilder("city", {{"country_id", "country_id"}})
                         .field("city_id")
                         .field("country_id", FieldFlag::DISABLED)
                         .field("city"));
  {
    rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
    EXPECT_EQ(R"*({
    "items": [
        {
            "links": [],
            "cities": [
                {
                    "city": "Kabul",
                    "city_id": 251
                }
            ],
            "country": "Afghanistan"
        },
        {
            "links": [],
            "cities": null,
            "country": "Algeria"
        },
        {
            "links": [],
            "cities": [
                {
                    "city": "Tafuna",
                    "city_id": 516
                }
            ],
            "country": "American Samoa"
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
}

// nested 1:n reference in base object (composite key)
TEST_F(DatabaseQueryGet, nested_1nc_base) {
  auto root =
      ObjectBuilder("mrstestdb", "city")
          .column("country_id")
          .column("city_id")
          .field("city")
          .nest_list("stores",
                     ObjectBuilder("store", {{"city_country_id", "country_id"},
                                             {"city_id", "city_id"}})
                         .field("store_id")
                         .column("city_id")
                         .column("city_country_id"));

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
  EXPECT_EQ(
      R"*({
    "items": [
        {
            "city": "Kabul",
            "links": [],
            "stores": null
        },
        {
            "city": "Tafuna",
            "links": [],
            "stores": [
                {
                    "store_id": 1
                },
                {
                    "store_id": 5
                }
            ]
        },
        {
            "city": "Benguela",
            "links": [],
            "stores": null
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

// pure unnested n:m reference in base object - invalid
TEST_F(DatabaseQueryGet, unnested_unnested_nm_base) {
  // skip - validation done when querying metadata
}

// nested+unnested n:m reference in base object
TEST_F(DatabaseQueryGet, nested_unnested_nm_base) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .field("actor_id", FieldFlag::DISABLED | FieldFlag::PRIMARY)
          .field("first_name")
          .nest_list(
              "films",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .field("actor_id", FieldFlag::DISABLED)
                  .field("film_id", FieldFlag::DISABLED)
                  .unnest_list(ObjectBuilder("film", {{"film_id", "film_id"}})
                                   .field("film_id", FieldFlag::PRIMARY |
                                                         FieldFlag::DISABLED)
                                   .field("title")
                                   .field("description")));

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
  EXPECT_EQ(
      R"*({
    "items": [
        {
            "films": [
                {
                    "title": "ACADEMY DINOSAUR",
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies"
                },
                {
                    "title": "ADAPTATION HOLES",
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory"
                },
                {
                    "title": "AFRICAN EGG",
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico"
                },
                {
                    "title": "ALADDIN CALENDAR",
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China"
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url/1"
                }
            ],
            "first_name": "PENELOPE"
        },
        {
            "films": [
                {
                    "title": "ADAPTATION HOLES",
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory"
                },
                {
                    "title": "AFFAIR PREJUDICE",
                    "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank"
                },
                {
                    "title": "AIRPORT POLLOCK",
                    "description": "A Epic Tale of a Moose And a Girl who must Confront a Monkey in Ancient India"
                },
                {
                    "title": "ALABAMA DEVIL",
                    "description": "A Thoughtful Panorama of a Database Administrator And a Mad Scientist who must Outgun a Mad Scientist in A Jet Boat"
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url/2"
                }
            ],
            "first_name": "NICK"
        },
        {
            "films": [
                {
                    "title": "ACADEMY DINOSAUR",
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies"
                },
                {
                    "title": "AFFAIR PREJUDICE",
                    "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank"
                }
            ],
            "links": [
                {
                    "rel": "self",
                    "href": "url/3"
                }
            ],
            "first_name": "ED"
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

// nested+unnested n:m reference in base object + extra lookups, nested
// category

TEST_F(DatabaseQueryGet, nested_unnested_nm_base_11) {
  using _ = ObjectBuilder;

  auto root =
      _("mrstestdb", "actor")
          .field("actor_id", FieldFlag::DISABLED)
          .field("first_name")
          .nest_list(
              "films",
              _("film_actor", {{"actor_id", "actor_id"}})
                  .field("actor_id", FieldFlag::DISABLED)
                  .field("film_id", FieldFlag::DISABLED)
                  .unnest(
                      _("film", {{"film_id", "film_id"}})
                          .field("film_id", FieldFlag::DISABLED)
                          .field("title")
                          .field("description")
                          .field("language_id", FieldFlag::DISABLED)
                          .field("original_language_id", FieldFlag::DISABLED)
                          .nest("language",
                                _("language", {{"language_id", "language_id"}})
                                    .field("language_id"))
                          .nest("original_language",
                                _("language",
                                  {{"original_language_id", "language_id"}})
                                    .field("language_id")))
                  .nest_list("categories",
                             _("film_category", {{"film_id", "film_id"}})
                                 .field("film_id")
                                 .field("category_id")
                                 .unnest(_("category",
                                           {{"category_id", "category_id"}})
                                             .column("category_id")
                                             .field("category", "name"))));

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
  EXPECT_EQ(
      R"*({
    "items": [
        {
            "films": [
                {
                    "title": "ACADEMY DINOSAUR",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 1,
                            "category": "Documentary",
                            "category_id": 6
                        }
                    ],
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies",
                    "original_language": {
                        "language_id": 2
                    }
                },
                {
                    "title": "ADAPTATION HOLES",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 3,
                            "category": "Documentary",
                            "category_id": 6
                        },
                        {
                            "film_id": 3,
                            "category": "Drama",
                            "category_id": 7
                        }
                    ],
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory",
                    "original_language": null
                },
                {
                    "title": "AFRICAN EGG",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 5,
                            "category": "Family",
                            "category_id": 8
                        }
                    ],
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico",
                    "original_language": null
                },
                {
                    "title": "ALADDIN CALENDAR",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 10,
                            "category": "Sports",
                            "category_id": 15
                        }
                    ],
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China",
                    "original_language": null
                }
            ],
            "links": [],
            "first_name": "PENELOPE"
        },
        {
            "films": [
                {
                    "title": "ADAPTATION HOLES",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 3,
                            "category": "Documentary",
                            "category_id": 6
                        },
                        {
                            "film_id": 3,
                            "category": "Drama",
                            "category_id": 7
                        }
                    ],
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory",
                    "original_language": null
                },
                {
                    "title": "AFFAIR PREJUDICE",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 4,
                            "category": "Horror",
                            "category_id": 11
                        }
                    ],
                    "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank",
                    "original_language": {
                        "language_id": 3
                    }
                },
                {
                    "title": "AIRPORT POLLOCK",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 8,
                            "category": "Horror",
                            "category_id": 11
                        }
                    ],
                    "description": "A Epic Tale of a Moose And a Girl who must Confront a Monkey in Ancient India",
                    "original_language": null
                },
                {
                    "title": "ALABAMA DEVIL",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 9,
                            "category": "Horror",
                            "category_id": 11
                        }
                    ],
                    "description": "A Thoughtful Panorama of a Database Administrator And a Mad Scientist who must Outgun a Mad Scientist in A Jet Boat",
                    "original_language": null
                }
            ],
            "links": [],
            "first_name": "NICK"
        },
        {
            "films": [
                {
                    "title": "ACADEMY DINOSAUR",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 1,
                            "category": "Documentary",
                            "category_id": 6
                        }
                    ],
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies",
                    "original_language": {
                        "language_id": 2
                    }
                },
                {
                    "title": "AFFAIR PREJUDICE",
                    "language": {
                        "language_id": 1
                    },
                    "categories": [
                        {
                            "film_id": 4,
                            "category": "Horror",
                            "category_id": 11
                        }
                    ],
                    "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank",
                    "original_language": {
                        "language_id": 3
                    }
                }
            ],
            "links": [],
            "first_name": "ED"
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

TEST_F(DatabaseQueryGet, nested_unnested_nm_base_11_renamed) {
  using _ = ObjectBuilder;

  auto root =
      _("mrstestdb", "actor")
          .column("actor_id")
          .field("firstName", "first_name")
          .nest_list(
              "films",
              _("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(
                      _("film", {{"film_id", "film_id"}})
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .column("language_id")
                          .column("original_language_id")
                          .nest("language",
                                _("language", {{"language_id", "language_id"}})
                                    .field("languageId", "language_id"))
                          .nest("originalLanguage",
                                _("language",
                                  {{"original_language_id", "language_id"}})
                                    .field("languageId", "language_id")))
                  .nest_list("categories",
                             _("film_category", {{"film_id", "film_id"}})
                                 .field("filmId", "film_id")
                                 .field("categoryId", "category_id")
                                 .unnest(_("category",
                                           {{"category_id", "category_id"}})
                                             .field("categoryId", "category_id")
                                             .field("category", "name"))));

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
  EXPECT_EQ(R"*({
    "items": [
        {
            "films": [
                {
                    "title": "ACADEMY DINOSAUR",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 1,
                            "category": "Documentary",
                            "categoryId": 6
                        }
                    ],
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies",
                    "originalLanguage": {
                        "languageId": 2
                    }
                },
                {
                    "title": "ADAPTATION HOLES",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 3,
                            "category": "Documentary",
                            "categoryId": 6
                        },
                        {
                            "filmId": 3,
                            "category": "Drama",
                            "categoryId": 7
                        }
                    ],
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory",
                    "originalLanguage": null
                },
                {
                    "title": "AFRICAN EGG",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 5,
                            "category": "Family",
                            "categoryId": 8
                        }
                    ],
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico",
                    "originalLanguage": null
                },
                {
                    "title": "ALADDIN CALENDAR",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 10,
                            "category": "Sports",
                            "categoryId": 15
                        }
                    ],
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China",
                    "originalLanguage": null
                }
            ],
            "links": [],
            "firstName": "PENELOPE"
        },
        {
            "films": [
                {
                    "title": "ADAPTATION HOLES",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 3,
                            "category": "Documentary",
                            "categoryId": 6
                        },
                        {
                            "filmId": 3,
                            "category": "Drama",
                            "categoryId": 7
                        }
                    ],
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory",
                    "originalLanguage": null
                },
                {
                    "title": "AFFAIR PREJUDICE",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 4,
                            "category": "Horror",
                            "categoryId": 11
                        }
                    ],
                    "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank",
                    "originalLanguage": {
                        "languageId": 3
                    }
                },
                {
                    "title": "AIRPORT POLLOCK",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 8,
                            "category": "Horror",
                            "categoryId": 11
                        }
                    ],
                    "description": "A Epic Tale of a Moose And a Girl who must Confront a Monkey in Ancient India",
                    "originalLanguage": null
                },
                {
                    "title": "ALABAMA DEVIL",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 9,
                            "category": "Horror",
                            "categoryId": 11
                        }
                    ],
                    "description": "A Thoughtful Panorama of a Database Administrator And a Mad Scientist who must Outgun a Mad Scientist in A Jet Boat",
                    "originalLanguage": null
                }
            ],
            "links": [],
            "firstName": "NICK"
        },
        {
            "films": [
                {
                    "title": "ACADEMY DINOSAUR",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 1,
                            "category": "Documentary",
                            "categoryId": 6
                        }
                    ],
                    "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies",
                    "originalLanguage": {
                        "languageId": 2
                    }
                },
                {
                    "title": "AFFAIR PREJUDICE",
                    "language": {
                        "languageId": 1
                    },
                    "categories": [
                        {
                            "filmId": 4,
                            "category": "Horror",
                            "categoryId": 11
                        }
                    ],
                    "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank",
                    "originalLanguage": {
                        "languageId": 3
                    }
                }
            ],
            "links": [],
            "firstName": "ED"
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

// nested+unnested n:m reference in base object + extra lookup, reduce
// category object to single value
TEST_F(DatabaseQueryGet, nested_unnested_nm_base_11_embedded) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .nest_list(
              "films",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(
                      ObjectBuilder("film", {{"film_id", "film_id"}})
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .column("language_id")
                          .unnest(ObjectBuilder("language", {{"language_id",
                                                              "language_id"}})
                                      .column("language_id")
                                      .field("language", "name"))
                          .column("original_language_id")
                          .unnest(ObjectBuilder(
                                      "language",
                                      {{"original_language_id", "language_id"}})
                                      .column("language_id")
                                      .field("original_language", "name"))
                          .nest_unnested_list(
                              "categories",
                              ObjectBuilder("film_category",
                                            {{"film_id", "film_id"}})
                                  .column("film_id")
                                  .column("category_id")
                                  .unnest(ObjectBuilder(
                                              "category",
                                              {{"category_id", "category_id"}})
                                              .column("category_id")
                                              .field("category", "name")))));

  rest->query_entries(m_.get(), root, {}, 0, 1, "url", true, {}, {});
  EXPECT_EQ(
      R"*({
    "items": [
        {
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
                    "title": "ADAPTATION HOLES",
                    "language": "English",
                    "categories": [
                        "Documentary",
                        "Drama"
                    ],
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory",
                    "original_language": null
                },
                {
                    "title": "AFRICAN EGG",
                    "language": "English",
                    "categories": [
                        "Family"
                    ],
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico",
                    "original_language": null
                },
                {
                    "title": "ALADDIN CALENDAR",
                    "language": "English",
                    "categories": [
                        "Sports"
                    ],
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China",
                    "original_language": null
                }
            ],
            "links": [],
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
            "href": "url/"
        },
        {
            "rel": "next",
            "href": "url/?offset=1"
        }
    ]
})*",
      pprint_json(rest->response));
}

// pure nested n:m reference in base object
TEST_F(DatabaseQueryGet, nested_nm_base) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .nest_list(
              "film_actor",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .nest("film", ObjectBuilder("film", {{"film_id", "film_id"}})
                                    .column("film_id")
                                    .field("title")
                                    .field("description")));

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});
  EXPECT_EQ(
      R"*({
    "items": [
        {
            "links": [],
            "film_actor": [
                {
                    "film": {
                        "title": "ACADEMY DINOSAUR",
                        "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies"
                    }
                },
                {
                    "film": {
                        "title": "ADAPTATION HOLES",
                        "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory"
                    }
                },
                {
                    "film": {
                        "title": "AFRICAN EGG",
                        "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico"
                    }
                },
                {
                    "film": {
                        "title": "ALADDIN CALENDAR",
                        "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China"
                    }
                }
            ],
            "first_name": "PENELOPE"
        },
        {
            "links": [],
            "film_actor": [
                {
                    "film": {
                        "title": "ADAPTATION HOLES",
                        "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory"
                    }
                },
                {
                    "film": {
                        "title": "AFFAIR PREJUDICE",
                        "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank"
                    }
                },
                {
                    "film": {
                        "title": "AIRPORT POLLOCK",
                        "description": "A Epic Tale of a Moose And a Girl who must Confront a Monkey in Ancient India"
                    }
                },
                {
                    "film": {
                        "title": "ALABAMA DEVIL",
                        "description": "A Thoughtful Panorama of a Database Administrator And a Mad Scientist who must Outgun a Mad Scientist in A Jet Boat"
                    }
                }
            ],
            "first_name": "NICK"
        },
        {
            "links": [],
            "film_actor": [
                {
                    "film": {
                        "title": "ACADEMY DINOSAUR",
                        "description": "A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies"
                    }
                },
                {
                    "film": {
                        "title": "AFFAIR PREJUDICE",
                        "description": "A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank"
                    }
                }
            ],
            "first_name": "ED"
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

// actors with plain list of films
TEST_F(DatabaseQueryGet, nested_nm_unnested) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .nest_list("filmActor",
                     ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                         .column("actor_id")
                         .column("film_id")
                         .unnest(ObjectBuilder("film", {{"film_id", "film_id"}})
                                     .column("film_id")
                                     .field("title")));

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});

  EXPECT_EQ(R"*({
    "items": [
        {
            "links": [],
            "filmActor": [
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
            "first_name": "PENELOPE"
        },
        {
            "links": [],
            "filmActor": [
                {
                    "title": "ADAPTATION HOLES"
                },
                {
                    "title": "AFFAIR PREJUDICE"
                },
                {
                    "title": "AIRPORT POLLOCK"
                },
                {
                    "title": "ALABAMA DEVIL"
                }
            ],
            "first_name": "NICK"
        },
        {
            "links": [],
            "filmActor": [
                {
                    "title": "ACADEMY DINOSAUR"
                },
                {
                    "title": "AFFAIR PREJUDICE"
                }
            ],
            "first_name": "ED"
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

TEST_F(DatabaseQueryGet, nested_nm_unnested_of_unnest) {
  auto root =
      ObjectBuilder("mrstestdb", "store")
          .field("store_id")
          .column("city_country_id")
          .column("city_id")
          .unnest(ObjectBuilder("city", {{"city_country_id", "country_id"},
                                         {"city_id", "city_id"}})
                      .column("country_id")
                      .column("city_id")
                      .field("city")
                      .unnest(ObjectBuilder("country",
                                            {{"country_id", "country_id"}})
                                  .column("country_id")
                                  .field("country")));
  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});

  EXPECT_EQ(R"*({
    "items": [
        {
            "city": "Tafuna",
            "links": [],
            "country": "American Samoa",
            "store_id": 1
        },
        {
            "city": "Tafuna",
            "links": [],
            "country": "American Samoa",
            "store_id": 5
        },
        {
            "city": "South Hill",
            "links": [],
            "country": "Anguilla",
            "store_id": 4
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

// actors with plain list of unnested film titles

TEST_F(DatabaseQueryGet, nested_nm_reduce_to_list_of_unnested) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .nest_unnested_list(
              "films",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(ObjectBuilder("film", {{"film_id", "film_id"}})
                              .column("film_id")
                              .field("title")));

  rest->query_entries(m_.get(), root, {}, 0, 3, "url", true, {}, {});

  EXPECT_EQ(R"*({
    "items": [
        {
            "films": [
                "ACADEMY DINOSAUR",
                "ADAPTATION HOLES",
                "AFRICAN EGG",
                "ALADDIN CALENDAR"
            ],
            "links": [],
            "first_name": "PENELOPE"
        },
        {
            "films": [
                "ADAPTATION HOLES",
                "AFFAIR PREJUDICE",
                "AIRPORT POLLOCK",
                "ALABAMA DEVIL"
            ],
            "links": [],
            "first_name": "NICK"
        },
        {
            "films": [
                "ACADEMY DINOSAUR",
                "AFFAIR PREJUDICE"
            ],
            "links": [],
            "first_name": "ED"
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

TEST_F(DatabaseQueryGet, nested_nm_reduce_to_unnested) {}

TEST_F(DatabaseQueryGet, exclude_field_filter) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .nest_list(
              "films",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(
                      ObjectBuilder("film", {{"film_id", "film_id"}})
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .column("language_id")
                          .unnest(ObjectBuilder("language", {{"language_id",
                                                              "language_id"}})
                                      .column("language_id")
                                      .field("language", "name"))
                          .column("original_language_id")
                          .unnest(ObjectBuilder(
                                      "language",
                                      {{"original_language_id", "language_id"}})
                                      .column("language_id")
                                      .field("original_language", "name"))
                          .nest_unnested_list(
                              "categories",
                              ObjectBuilder("film_category",
                                            {{"film_id", "film_id"}})
                                  .column("film_id")
                                  .column("category_id")
                                  .unnest(ObjectBuilder(
                                              "category",
                                              {{"category_id", "category_id"}})
                                              .column("category_id")
                                              .field("name")))));

  {
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root.root(),
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
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory",
                    "original_language": null
                },
                {
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico",
                    "original_language": null
                },
                {
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China",
                    "original_language": null
                }
            ],
            "links": [],
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
      ObjectBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("first_name")
          .nest_list(
              "films",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(
                      ObjectBuilder("film", {{"film_id", "film_id"}})
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .column("language_id")
                          .unnest(ObjectBuilder("language", {{"language_id",
                                                              "language_id"}})
                                      .column("language_id")
                                      .field("language", "name"))
                          .column("original_language_id")
                          .unnest(ObjectBuilder(
                                      "language",
                                      {{"original_language_id", "language_id"}})
                                      .column("language_id")
                                      .field("original_language", "name"))
                          .nest_unnested_list(
                              "categories",
                              ObjectBuilder("film_category",
                                            {{"film_id", "film_id"}})
                                  .column("film_id")
                                  .column("category_id")
                                  .unnest(ObjectBuilder(
                                              "category",
                                              {{"category_id", "category_id"}})
                                              .column("category_id")
                                              .field("name")))));
  {
    rest->query_entries(m_.get(), root, {}, 0, 1, "url1", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
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
                    "title": "ADAPTATION HOLES",
                    "language": "English",
                    "categories": [
                        "Documentary",
                        "Drama"
                    ],
                    "description": "A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory",
                    "original_language": null
                },
                {
                    "title": "AFRICAN EGG",
                    "language": "English",
                    "categories": [
                        "Family"
                    ],
                    "description": "A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico",
                    "original_language": null
                },
                {
                    "title": "ALADDIN CALENDAR",
                    "language": "English",
                    "categories": [
                        "Sports"
                    ],
                    "description": "A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China",
                    "original_language": null
                }
            ],
            "links": [],
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
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root.root(),
        {"first_name", "films.title", "films.language", "films.categories"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url2", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "title": "ACADEMY DINOSAUR",
                    "language": "English",
                    "categories": [
                        "Documentary"
                    ]
                },
                {
                    "title": "ADAPTATION HOLES",
                    "language": "English",
                    "categories": [
                        "Documentary",
                        "Drama"
                    ]
                },
                {
                    "title": "AFRICAN EGG",
                    "language": "English",
                    "categories": [
                        "Family"
                    ]
                },
                {
                    "title": "ALADDIN CALENDAR",
                    "language": "English",
                    "categories": [
                        "Sports"
                    ]
                }
            ],
            "links": [],
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
  return;

  {
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root.root(), {"films"});

    reset();

    // auto query = build_select_json_object(root, filter);
    rest->query_entries(m_.get(), root, filter, 0, 1, "url3", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "title": "THE TEST I",
                    "language": "English",
                    "categories": null,
                    "description": "Nothing happens",
                    "original_language": null
                },
                {
                    "title": "THE TEST II",
                    "language": "English",
                    "categories": null,
                    "description": "Nothing happens again",
                    "original_language": null
                },
                {
                    "title": "THE TEST III",
                    "language": "English",
                    "categories": null,
                    "description": "Nothing happens as usual",
                    "original_language": null
                }
            ],
            "links": []
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
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root.root(), {"films.title"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url4", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "title": "THE TEST I"
                },
                {
                    "title": "THE TEST II"
                },
                {
                    "title": "THE TEST III"
                }
            ],
            "links": []
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
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root.root(), {"films.categories"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url5", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "categories": null
                },
                {
                    "categories": null
                },
                {
                    "categories": null
                }
            ],
            "links": []
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
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root.root(), {"films.original_language", "films.title"});

    reset();

    rest->query_entries(m_.get(), root, filter, 0, 1, "url6", true, {}, {});
    EXPECT_EQ(
        R"*({
    "items": [
        {
            "films": [
                {
                    "title": "THE TEST I",
                    "original_language": null
                },
                {
                    "title": "THE TEST II",
                    "original_language": null
                },
                {
                    "title": "THE TEST III",
                    "original_language": null
                }
            ],
            "links": []
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
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root.root(), {"first_name", "films.film_id"});
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
            "links": [],
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

TEST_F(DatabaseQueryGet, row_filter) {
  auto root =
      ObjectBuilder("mrstestdb", "actor")
          .column("actor_id")
          .field("firstName", "first_name", "text")
          .field("lastName", "last_name", "text", FieldFlag::NOFILTER)
          .nest_list(
              "films",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(
                      ObjectBuilder("film", {{"film_id", "film_id"}})
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .column("language_id")
                          .unnest(ObjectBuilder("language", {{"language_id",
                                                              "language_id"}})
                                      .column("language_id")
                                      .field("language", "name"))
                          .nest_list(
                              "categories",
                              ObjectBuilder("film_category",
                                            {{"film_id", "film_id"}})
                                  .column("film_id")
                                  .column("category_id")
                                  .unnest(ObjectBuilder(
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
      ObjectBuilder("mrstestdb", "actor")
          .field("id", "actor_id", "int",
                 FieldFlag::PRIMARY | FieldFlag::SORTABLE)
          .field("firstName", "first_name", "text",
                 FieldFlag::UNIQUE | FieldFlag::SORTABLE)
          .field("lastName", "last_name", "text", FieldFlag::NOFILTER)
          .nest_list(
              "films",
              ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                  .column("actor_id")
                  .column("film_id")
                  .unnest(
                      ObjectBuilder("film", {{"film_id", "film_id"}})
                          .column("film_id")
                          .field("title")
                          .field("description")
                          .column("language_id")
                          .unnest(ObjectBuilder("language", {{"language_id",
                                                              "language_id"}})
                                      .column("language_id")
                                      .field("language", "name"))
                          .nest_list(
                              "categories",
                              ObjectBuilder("film_category",
                                            {{"film_id", "film_id"}})
                                  .column("film_id")
                                  .column("category_id")
                                  .unnest(ObjectBuilder(
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

TEST_F(DatabaseQueryGet, etag) {
  {
    auto root =
        ObjectBuilder("mrstestdb", "actor")
            .field("actor_id", FieldFlag::PRIMARY)
            .field("first_name")
            .field("last_name", FieldFlag::NOCHECK)
            .nest_list("film_actor",
                       ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                           .field("actor_id", FieldFlag::PRIMARY)
                           .field("film_id", FieldFlag::PRIMARY)
                           .nest("film",
                                 ObjectBuilder("film", {{"film_id", "film_id"}})
                                     .field("film_id", FieldFlag::PRIMARY)
                                     .field("title")
                                     .field("description")));

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

    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root.root(), {"actor_id", "first_name"});

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

    auto root =
        ObjectBuilder("mrstestdb", "actor")
            .field("actor_id", FieldFlag::PRIMARY)
            .field("first_name")
            .field("last_name")
            .nest_list("film_actor",
                       ObjectBuilder("film_actor", {{"actor_id", "actor_id"}})
                           .field("actor_id", FieldFlag::PRIMARY)
                           .field("film_id", FieldFlag::PRIMARY)
                           .nest("film",
                                 ObjectBuilder("film", {{"film_id", "film_id"}})
                                     .field("film_id", FieldFlag::PRIMARY)
                                     .field("title")
                                     .field("description")));

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

    auto root = ObjectBuilder("mrstestdb", "typetest")
                    .field("id", FieldFlag::PRIMARY)
                    .field("Geom", "geom", "GEOMETRY")
                    .field("Bool", "bool", "BIT(1)")
                    .field("Binary", "bin", "BLOB")
                    .field("Json", "js", "JSON");

    rest->query_entries(m_.get(), root, {}, 0, 1, "url", true, {}, {}, true);

    auto json = make_json(rest->response);
    EXPECT_EQ(1, json["items"].GetArray().Size()) << rest->response;

    EXPECT_EQ(1, json["items"][0]["id"].GetInt());
    EXPECT_STREQ(
        "FCA79725A9EEE5CD52808D83E74402102BA32004E5D07817010C412E66380A93",
        json["items"][0]["_metadata"]["etag"].GetString());
  }
}
