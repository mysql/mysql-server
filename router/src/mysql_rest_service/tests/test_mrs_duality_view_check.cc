/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

#include "helper/expect_throw_msg.h"
#include "mock/mock_session.h"
#include "mrs/database/duality_view/check.h"
#include "mrs/database/helper/object_checksum.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;
using namespace mrs::database::dv;

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;

#define EXPECT_UUID(value) EXPECT_EQ(16, unescape(value).size() - 2) << value

class DualityViewCheck : public DatabaseRestTableTest {
 public:
  void check_e(std::shared_ptr<DualityView> view, const std::string &input,
               bool for_update = false,
               const ObjectRowOwnership &row_owner = {}) {
    SCOPED_TRACE(input);
    try {
      check(view, input, for_update, row_owner);
    } catch (const JSONInputError &e) {
      ADD_FAILURE() << "check() threw JSONInputError: " << e.what();
      throw;
    } catch (const DualityViewError &e) {
      ADD_FAILURE() << "check() threw DualityViewError: " << e.what();
      throw;
    } catch (const MySQLError &e) {
      ADD_FAILURE() << "check() threw MySQLError: " << e.what();
      throw;
    } catch (const std::runtime_error &e) {
      ADD_FAILURE() << "check() threw runtime_error: " << e.what();
      throw;
    }
  }

  void check(std::shared_ptr<DualityView> view, const std::string &input,
             bool for_update = false,
             const ObjectRowOwnership &row_owner = {}) {
    DualityViewUpdater dvu(view, row_owner);

    auto json = make_json(input);
    assert(json.IsObject());

    dvu.check(json, for_update);
  }

  void insert_check(std::shared_ptr<DualityView> view, const std::string &input,
                    const ObjectRowOwnership &row_owner = {}) {
    check(view, input, false, row_owner);
  }

  void update_check(std::shared_ptr<DualityView> view, const std::string &input,
                    const ObjectRowOwnership &row_owner = {}) {
    check(view, input, true, row_owner);
  }
};

TEST_F(DualityViewCheck, is_read_only) {
  int flags[] = {
      0,
      TableFlag::WITH_INSERT,
      TableFlag::WITH_UPDATE,
      TableFlag::WITH_DELETE,
      TableFlag::WITH_INSERT | TableFlag::WITH_UPDATE,
      TableFlag::WITH_INSERT | TableFlag::WITH_DELETE,
      TableFlag::WITH_UPDATE | TableFlag::WITH_DELETE,
      TableFlag::WITH_INSERT | TableFlag::WITH_UPDATE | TableFlag::WITH_DELETE};

  const char *fstr[] = {"-", "I", "U", "D", "IU", "ID", "UD", "IUD"};
  assert(sizeof(flags) / sizeof(int) == sizeof(fstr) / sizeof(const char *));

  for (int i : flags) {
    for (int j : flags) {
      for (int k : flags) {
        for (int l : flags) {
          if ((j != 0 && j != TableFlag::WITH_UPDATE) ||
              (l != 0 && l != TableFlag::WITH_UPDATE))
            continue;

          auto root =
              DualityViewBuilder("mrstestdb", "film", i)
                  .field("id", "film_id", FieldFlag::AUTO_INC)
                  .field("title")
                  .field("description")
                  .field_to_one("language",
                                ViewBuilder("language", j)
                                    .field("language_id", FieldFlag::AUTO_INC)
                                    .field("name"),
                                false, {{"language_id", "language_id"}})
                  .field_to_many(
                      "actors",
                      ViewBuilder("film_actor", k)
                          .field("film_id")
                          .field("actor_id")
                          .field_to_one(
                              "actor",
                              ViewBuilder("actor", l)
                                  .field("actor_id", FieldFlag::AUTO_INC)
                                  .field("firstName", "first_name")
                                  .field("last_name")))
                  .resolve();

          if (i == 0 && j == 0 && k == 0 && l == 0)
            EXPECT_TRUE(root->is_read_only())
                << " i=" << fstr[i] << " j=" << fstr[j] << " k=" << fstr[k]
                << " l=" << fstr[l];
          else
            EXPECT_FALSE(root->is_read_only())
                << " i=" << fstr[i] << " j=" << fstr[j] << " k=" << fstr[k]
                << " l=" << fstr[l];
        }
      }
    }
  }
}

TEST_F(DualityViewCheck, insert_common) {
  // WITH INSERT/NOINSERT doesn't affect checks here
  // CHECK/NOCHECK shouldn't either
  int film_flags[] = {TableFlag::WITH_CHECK, TableFlag::WITH_NOCHECK};
  for (int i = 0; i < 1; i++) {
    SCOPED_TRACE(std::to_string(i));

    auto root =
        DualityViewBuilder("mrstestdb", "film", film_flags[i])
            .field("id", "film_id", FieldFlag::AUTO_INC)
            .field("title")
            .field("description")
            .field_to_one("language",
                          ViewBuilder("language", 0)
                              .field("language_id", FieldFlag::AUTO_INC)
                              .field("name"),
                          false, {{"language_id", "language_id"}})
            .field_to_many(
                "actors",
                ViewBuilder("film_actor", 0)
                    .field("film_id")
                    .field("actor_id")
                    .field_to_one("actor",
                                  ViewBuilder("actor", 0)
                                      .field("actor_id", FieldFlag::AUTO_INC)
                                      .field("firstName", "first_name")
                                      .field("last_name")))
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    // all fields filled
    check_e(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA" 
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ],
    "_metadata": {
      "ignoreme": 1
    }
  })*",
            true);

    // invalid field
    EXPECT_JSON_ERROR(
        check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "badfield": 1,
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*"),
        "Invalid field \"badfield\" in table `film` in JSON input");
    //@ 1:1
    EXPECT_JSON_ERROR(
        check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English",
      "badfield": 1
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*"),
        "Invalid field \"badfield\" in table `language` in JSON input");
    //@ 1:n
    EXPECT_JSON_ERROR(
        check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "badfield": 1,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*"),
        "Invalid field \"badfield\" in table `film_actor` in JSON input");
    EXPECT_JSON_ERROR(
        check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA",
          "badfield": 1
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*"),
        "Invalid field \"badfield\" in table `actor` in JSON input");

    // null for reference
    // @1:1
    EXPECT_JSON_ERROR(
        check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": null,
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA" 
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*",
              true),
        "Invalid value for \"language\" for table `film` in JSON input");

    // @1:n
    EXPECT_JSON_ERROR(check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": [
      null,
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*",
                            true),
                      "Invalid document in JSON input for table `film_actor`");

    EXPECT_JSON_ERROR(
        check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": null
  })*",
              true),
        "Invalid value for \"actors\" for table `film` in JSON input");

    // omitted nested object (allowed even with check)
    check_e(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Lorem ipsum"
  })*");

    // empty nested object
    check_e(root, R"*({
    "id": 123,
    "description": "Lorem ipsum",
    "title": "The Movie",
    "language": {},
    "actors": []
  })*");
  }
}

TEST_F(DualityViewCheck, missing_fields) {
  auto root =
      DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_NOCHECK)
          .field("id", "film_id", FieldFlag::AUTO_INC)
          .field("title")
          .field("description")
          .field_to_one("language",
                        ViewBuilder("language", TableFlag::WITH_NOCHECK)
                            .field("language_id", FieldFlag::AUTO_INC)
                            .field("name"),
                        false, {{"language_id", "language_id"}})
          .field_to_many(
              "actors",
              ViewBuilder("film_actor", TableFlag::WITH_NOCHECK)
                  .field("film_id")
                  .field("actor_id")
                  .field_to_one("actor",
                                ViewBuilder("actor", TableFlag::WITH_NOCHECK)
                                    .field("actor_id", FieldFlag::AUTO_INC)
                                    .field("firstName", "first_name")
                                    .field("last_name")))
          .resolve(m_.get());

  auto root_check =
      DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_CHECK)
          .field("id", "film_id", FieldFlag::AUTO_INC)
          .field("title")
          .field("description")
          .field_to_one("language",
                        ViewBuilder("language")
                            .field("language_id", FieldFlag::AUTO_INC)
                            .field("name"),
                        false, {{"language_id", "language_id"}})
          .field_to_many(
              "actors",
              ViewBuilder("film_actor", 0)
                  .field("film_id")
                  .field("actor_id")
                  .field_to_one("actor",
                                ViewBuilder("actor", 0)
                                    .field("actor_id", FieldFlag::AUTO_INC)
                                    .field("firstName", "first_name")
                                    .field("last_name")))
          .resolve(m_.get());

  auto root_check_nocheck =
      DualityViewBuilder("mrstestdb", "film")
          .field("id", "film_id", FieldFlag::AUTO_INC)
          .field("title")
          .field("description", FieldFlag::WITH_NOCHECK)
          .field_to_one("language",
                        ViewBuilder("language", 0)
                            .field("language_id", FieldFlag::AUTO_INC)
                            .field("name", FieldFlag::WITH_NOCHECK),
                        false, {{"language_id", "language_id"}})
          .field_to_many(
              "actors",
              ViewBuilder("film_actor", 0)
                  .field("film_id")
                  .field("actor_id")
                  .field_to_one(
                      "actor",
                      ViewBuilder("actor", 0)
                          .field("actor_id", FieldFlag::AUTO_INC)
                          .field("firstName", "first_name")
                          .field("last_name", FieldFlag::WITH_NOCHECK)))
          .resolve(m_.get());

  // missing regular column
  check_e(root, R"*({
    "id": 123,
    "title": "The Movie",
    "language": {
      "language_id": 1
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10
        }
      }
    ]
  })*",
          true);

  EXPECT_JSON_ERROR(
      check(root_check, R"*({
    "id": 123,
    "title": "The Movie",
    "language": {
      "language_id": 1
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10
        }
      }
    ]
  })*",
            true),
      "Field \"description\" for table `film` missing in JSON input");

  check_e(root_check_nocheck, R"*({
    "id": 123,
    "title": "The Movie",
    "language": {
      "language_id": 1
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "JOHNNY2"
        }
      }
    ]
  })*",
          true);

  // inside nested
  EXPECT_JSON_ERROR(
      check(root_check, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Ipsum lorem",
    "language": {
      "language_id": 1
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10
        }
      }
    ]
  })*",
            true),
      "Field \"name\" for table `language` missing in JSON input");

  check_e(root_check_nocheck, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Ipsum lorem",
    "language": {
      "language_id": 1
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "JOHNNY2"
        }
      }
    ]
  })*",
          true);

  EXPECT_JSON_ERROR(update_check(root_check_nocheck, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Ipsum lorem",
    "language": {
      "language_id": 1
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "firstName": "JOHNNY2"
        }
      }
    ]
  })*"),
                    "ID for table `actor` missing in JSON input");

  update_check(root_check_nocheck, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Ipsum lorem",
    "language": {
      "language_id": 1
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "JOHNNY2"
        }
      }
    ]
  })*");
}

TEST_F(DualityViewCheck, duplicate_id_in_array) {
  // not affected by flags
  auto root =
      DualityViewBuilder("mrstestdb", "film", 0)
          .field("id", "film_id", FieldFlag::AUTO_INC)
          .field("title")
          .field("description")
          .field_to_one("language",
                        ViewBuilder("language", 0)
                            .field("language_id", FieldFlag::AUTO_INC)
                            .field("name"),
                        false, {{"language_id", "language_id"}})
          .field_to_many(
              "actors",
              ViewBuilder("film_actor", 0)
                  .field("film_id")
                  .field("actor_id")
                  .field_to_one("actor",
                                ViewBuilder("actor", TableFlag::WITH_NOCHECK)
                                    .field("actor_id", FieldFlag::AUTO_INC)
                                    .field("last_name")))
          .resolve(m_.get());
  SCOPED_TRACE(root->as_graphql());

  check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {},
    "actors": [
      {
        "film_id": 123,
        "actor_id": 10
      },
      {
        "film_id": 123,
        "actor_id": 5
      },
      {
        "film_id": 123,
        "actor_id": 6,
        "actor": {
          "actor_id": 6
        }
      }
    ]
  })*",
        true);

  EXPECT_JSON_ERROR(
      check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {},
    "actors": [
      {
        "film_id": 123,
        "actor_id": 4
      },
      {
        "film_id": 123,
        "actor_id": 5
      },
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5
        }
      }
    ]
  })*",
            true),
      "Duplicate keys in \"actors\" for table `film` in JSON input");
}

TEST_F(DualityViewCheck, insert_missing_pk) {
  auto root =
      DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_CHECK)
          .field("id", "film_id", FieldFlag::AUTO_INC)
          .field("title")
          .field("description")
          .field_to_one("language",
                        ViewBuilder("language", 0)
                            .field("language_id", FieldFlag::AUTO_INC)
                            .field("name"),
                        false, {{"language_id", "language_id"}})
          .field_to_many(
              "actors",
              ViewBuilder("film_actor", 0)
                  .field("film_id")
                  .field("actor_id")
                  .field_to_one("actor",
                                ViewBuilder("actor", 0)
                                    .field("actor_id", FieldFlag::AUTO_INC)
                                    .field("firstName", "first_name")
                                    .field("last_name")))
          .resolve(m_.get());

  SCOPED_TRACE(root->as_graphql(true));

  // missing required PK
  EXPECT_JSON_ERROR(check(root, R"*({
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE"
        }
      }
    ]
  })*",
                          true),
                    "ID for table `film` missing in JSON input");
  // @1:1
  EXPECT_JSON_ERROR(check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 5,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA" 
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*",
                          true),
                    "ID for table `language` missing in JSON input");
  //@ n:m
  EXPECT_JSON_ERROR(check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA"
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*",
                          true),
                    "ID for table `film_actor` missing in JSON input");

  // @1:n
  EXPECT_JSON_ERROR(check(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language": {
      "language_id": 1,
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor": {
          "actor_id": 5,
          "firstName": "JOHNNY",
          "last_name": "LOLLOBRIGIDA" 
        }
      },
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "CHRISTIAN",
          "last_name": "GABLE" 
        }
      }
    ]
  })*",
                          true),
                    "ID for table `film_actor` missing in JSON input");
}

TEST_F(DualityViewCheck, unnest_11) {
  // WITH INSERT/NOINSERT doesn't affect checks here
  // CHECK/NOCHECK shouldn't either
  int film_flags[] = {TableFlag::WITH_CHECK, TableFlag::WITH_NOCHECK};
  for (int i = 0; i < 1; i++) {
    SCOPED_TRACE(std::to_string(i));

    auto root =
        DualityViewBuilder("mrstestdb", "film",
                           film_flags[i] | TableFlag::WITH_UPDATE)
            .field("id", "film_id", FieldFlag::AUTO_INC)
            .field("title")
            .field("description")
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_UPDATE)
                              .field("language_id", FieldFlag::AUTO_INC)
                              .field("language", "name"),
                          true, {{"language_id", "language_id"}})
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    // is updatable
    EXPECT_FALSE(root->is_read_only());

    // all fields filled
    check_e(root, R"*({
    "id": 123,
    "title": "The Movie",
    "description": "Some test movie",
    "language_id": 1,
    "language": "English",
    "_metadata": {
      "ignoreme": 1
    }
  })*",
            true);
  }
}

TEST_F(DualityViewCheck, unnest_1n) {
  int flags[] = {TableFlag::WITH_CHECK, TableFlag::WITH_NOCHECK};
  for (int i = 0; i < 1; i++) {
    SCOPED_TRACE(std::to_string(i));

    auto root =
        DualityViewBuilder("mrstestdb", "country",
                           flags[i] | TableFlag::WITH_UPDATE)
            .field("id", "country_id", FieldFlag::AUTO_INC)
            .field("country")
            .field_to_many(
                "cities",
                ViewBuilder("city", TableFlag::WITH_UPDATE)
                    .field("city_id", FieldFlag::AUTO_INC | FieldFlag::DISABLED)
                    .field("city"),
                true)
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    // unnested 1:n is not updatable
    EXPECT_TRUE(root->is_read_only());

    EXPECT_THROW(check(root, R"*({
    "id": 123,
    "country": "Country",
    "cities": [
      "City",
      "New City",
      "North City"
    ],
    "_metadata": {
      "ignoreme": 1
    }
  })*",
                       true),
                 std::logic_error);
  }
}

TEST_F(DualityViewCheck, non_pk_fields_are_optional) {
  // - all PKs are WITH CHECK (for etag ) by default, regardless of the table
  // level CHECK

  auto root =
      DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_INSERT)
          .field("id", "film_id",
                 FieldFlag::PRIMARY | FieldFlag::AUTO_INC |
                     FieldFlag::WITH_NOCHECK)
          .field("title", FieldFlag::WITH_CHECK)
          .field("description", 0)
          .field_to_one("language",
                        ViewBuilder("language", TableFlag::WITH_NOCHECK)
                            .field("language_id",
                                   FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                            .field("name", 0),
                        false, {{"language_id", "language_id"}})
          .field_to_many(
              "actors",
              ViewBuilder("film_actor")
                  .field("film_id",
                         FieldFlag::PRIMARY | FieldFlag::WITH_NOCHECK)
                  .field("actor_id", FieldFlag::PRIMARY)
                  .field_to_one(
                      "actor",
                      ViewBuilder("actor", TableFlag::WITH_CHECK)
                          .field("actor_id", FieldFlag::PRIMARY |
                                                 FieldFlag::AUTO_INC |
                                                 FieldFlag::WITH_NOCHECK)
                          .field("first_name", FieldFlag::WITH_CHECK)
                          .field("last_name")))
          .resolve(m_.get());

  SCOPED_TRACE(root->as_graphql());

  EXPECT_NO_THROW(check(root, R"*({
    "id": 1,
    "language": {
      "language_id": 1
    },
    "actors": [{
      "film_id": 1,
      "actor_id": 1,
      "actor": {
        "actor_id": 1
      }
    }]
  })*",
                        false));

  // NOCHECK on a PK should affect the etag but not the validation
  EXPECT_JSON_ERROR(check(root, R"*({
    "id": 1,
    "language": {
      "name": "English"
    }
  })*",
                          false),
                    "ID for table `language` missing in JSON input");

  // NOCHECK on a PK should affect the etag but not the validation
  EXPECT_JSON_ERROR(check(root, R"*({
    "id": 1,
    "actors": [{
      "actor": {
        "first_name": "hello"
      }
    }]
  })*",
                          false),
                    "ID for table `film_actor` missing in JSON input");
}

static std::string get_etag(const std::string &json) {
  auto j = make_json(json);
  if (!j.GetObject().HasMember("_metadata") ||
      !j.GetObject()["_metadata"].HasMember("etag"))
    return "";
  return j["_metadata"]["etag"].GetString();
}

TEST_F(DualityViewCheck, checksum) {
  auto data = R"*({
    "id": 123,
    "title": "Title",
    "description": "Description",
    "language": {
      "language_id": 32,
      "name": "English"
    },
    "actors": [
      {
        "film_id": 123,
        "actor_id": 10,
        "actor": {
          "actor_id": 10,
          "firstName": "John",
          "last_name": "Johnson"
        }
      },
      {
        "film_id": 123,
        "actor_id": 11,
        "actor": {
          "actor_id": 11,
          "firstName": "Marie",
          "last_name": "Mary"
        }
      }
    ]
  })*";
#if 0
  // defaults
  {
    auto root =
        DualityViewBuilder("mrstestdb", "film", 0)
            .field("id", "film_id", FieldFlag::AUTO_INC)
            .field("title")
            .field("description")
            .field_to_one("language",
                          ViewBuilder("language", 0)
                              .field("language_id", FieldFlag::AUTO_INC)
                              .field("name"),
                          false, {{"language_id", "language_id"}})
            .field_to_many(
                "actors",
                ViewBuilder("film_actor", 0)
                    .field("film_id")
                    .field("actor_id")
                    .field_to_one("actor",
                                  ViewBuilder("actor", 0)
                                      .field("actor_id", FieldFlag::AUTO_INC)
                                      .field("firstName", "first_name")
                                      .field("last_name")))
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    std::string out = post_process_json(root, {}, {}, data, true);
    EXPECT_EQ(
        get_etag(out),
        "36094E0EB5203E664F680DC4CD32CD62E875504CC32AF822308ADEC3E6D971A5");
  }
#endif
  // explicit WITH CHECK
  {
    auto root =
        DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_CHECK)
            .field("id", "film_id", FieldFlag::AUTO_INC)
            .field("title")
            .field("description", FieldFlag::WITH_CHECK)
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_CHECK)
                              .field("language_id", FieldFlag::AUTO_INC)
                              .field("name"),
                          false, {{"language_id", "language_id"}})
            .field_to_many(
                "actors",
                ViewBuilder("film_actor", TableFlag::WITH_CHECK)
                    .field("film_id")
                    .field("actor_id")
                    .field_to_one("actor",
                                  ViewBuilder("actor", TableFlag::WITH_CHECK)
                                      .field("actor_id", FieldFlag::AUTO_INC)
                                      .field("firstName", "first_name")
                                      .field("last_name")))
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    std::string out = post_process_json(root, {}, {}, data, true);
    EXPECT_EQ(
        get_etag(out),
        "D01B8369638740D738D9ACD9D7D46A78B505E6630311AAC7C5F7F86804CFE518");
  }

  // explicit WITH CHECK, disable check in some columns
  {
    auto root =
        DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_CHECK)
            .field("id", "film_id", FieldFlag::AUTO_INC)
            .field("title")
            .field("description", FieldFlag::WITH_NOCHECK)
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_CHECK)
                              .field("language_id", FieldFlag::AUTO_INC)
                              .field("name", FieldFlag::WITH_NOCHECK),
                          false, {{"language_id", "language_id"}})
            .field_to_many(
                "actors",
                ViewBuilder("film_actor", TableFlag::WITH_CHECK)
                    .field("film_id")
                    .field("actor_id", FieldFlag::WITH_NOCHECK)
                    .field_to_one(
                        "actor",
                        ViewBuilder("actor", TableFlag::WITH_CHECK)
                            .field("actor_id", FieldFlag::AUTO_INC)
                            .field("firstName", "first_name")
                            .field("last_name", FieldFlag::WITH_NOCHECK)))
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());
    std::string out = post_process_json(root, {}, {}, data, true);
    EXPECT_EQ(
        get_etag(out),
        "9EE8E6454D92F02BA9C0B5A390DA162CC3AF557B5D08CB1A898081EA03EBC8C5");
  }
  // small variation
  {
    auto root =
        DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_CHECK)
            .field("id", "film_id", FieldFlag::AUTO_INC)
            .field("title")
            .field("description", FieldFlag::WITH_NOCHECK)
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_CHECK)
                              .field("language_id", FieldFlag::AUTO_INC)
                              .field("name", FieldFlag::WITH_NOCHECK),
                          false, {{"language_id", "language_id"}})
            .field_to_many(
                "actors",
                ViewBuilder("film_actor", TableFlag::WITH_CHECK)
                    .field("film_id")
                    .field("actor_id", FieldFlag::WITH_NOCHECK)
                    .field_to_one(
                        "actor",
                        ViewBuilder("actor", TableFlag::WITH_CHECK)
                            .field("actor_id", FieldFlag::AUTO_INC)
                            .field("firstName", "first_name",
                                   FieldFlag::WITH_NOCHECK)  // <--
                            .field("last_name", FieldFlag::WITH_NOCHECK)))
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    std::string out = post_process_json(root, {}, {}, data, true);
    EXPECT_EQ(
        get_etag(out),
        "C68960AE8DE1E422AB8E087944B3D56912C7C9D881295A0D17099BCBF02D9626");
  }
  // same but disable field
  {
    auto root =
        DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_CHECK)
            .field("id", "film_id", FieldFlag::AUTO_INC)
            .field("title")
            .field("description", FieldFlag::WITH_NOCHECK)
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_CHECK)
                              .field("language_id", FieldFlag::AUTO_INC)
                              .field("name", FieldFlag::WITH_NOCHECK),
                          false, {{"language_id", "language_id"}})
            .field_to_many(
                "actors",
                ViewBuilder("film_actor", TableFlag::WITH_CHECK)
                    .field("film_id")
                    .field("actor_id", FieldFlag::WITH_NOCHECK)
                    .field_to_one(
                        "actor",
                        ViewBuilder("actor", TableFlag::WITH_CHECK)
                            .field("actor_id", FieldFlag::AUTO_INC)
                            .field("firstName", "first_name",
                                   FieldFlag::DISABLED)  // <--
                            .field("last_name", FieldFlag::WITH_NOCHECK)))
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    std::string out = post_process_json(root, {}, {}, data, true);
    EXPECT_EQ(
        get_etag(out),
        "9EE8E6454D92F02BA9C0B5A390DA162CC3AF557B5D08CB1A898081EA03EBC8C5");
  }
  // invert the flags, but etag should match
  {
    auto root =
        DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_NOCHECK)
            .field("id", "film_id", FieldFlag::AUTO_INC | FieldFlag::WITH_CHECK)
            .field("title", FieldFlag::WITH_CHECK)
            .field("description", 0)
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_NOCHECK)
                              .field("language_id", FieldFlag::AUTO_INC |
                                                        FieldFlag::WITH_CHECK)
                              .field("name", 0),
                          false, {{"language_id", "language_id"}})
            .field_to_many(
                "actors",
                ViewBuilder("film_actor", TableFlag::WITH_CHECK)
                    .field("film_id", FieldFlag::WITH_CHECK)
                    .field("actor_id", 0)
                    .field_to_one(
                        "actor",
                        ViewBuilder("actor", TableFlag::WITH_CHECK)
                            .field("actor_id",
                                   FieldFlag::AUTO_INC | FieldFlag::WITH_CHECK)
                            .field("firstName", "first_name",
                                   FieldFlag::WITH_CHECK)
                            .field("last_name", FieldFlag::WITH_NOCHECK)))
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    std::string out = post_process_json(root, {}, {}, data, true);
    EXPECT_EQ(
        get_etag(out),
        "8B5CCFA86FDD4C17DCE49BCA229B0D26D821738E9B576C5DB2B9AAFC1197D8FF");
  }
  // PK is always checksummed, unless explicitly NOCHECK on the field
  {
    auto root1 =
        DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_NOCHECK)
            .field("id", "film_id", FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
            .field("title", FieldFlag::WITH_CHECK)
            .field("description", 0)
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_NOCHECK)
                              .field("language_id",
                                     FieldFlag::PRIMARY | FieldFlag::AUTO_INC)
                              .field("name", 0),
                          false, {{"language_id", "language_id"}})
            .field_to_many("actors",
                           ViewBuilder("film_actor", TableFlag::WITH_NOCHECK)
                               .field("film_id", FieldFlag::PRIMARY)
                               .field("actor_id", FieldFlag::PRIMARY))
            .resolve(m_.get());

    auto root2 =
        DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_NOCHECK)
            .field("id", "film_id",
                   FieldFlag::PRIMARY | FieldFlag::AUTO_INC |
                       FieldFlag::WITH_NOCHECK)
            .field("title", FieldFlag::WITH_CHECK)
            .field("description", 0)
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_NOCHECK)
                              .field("language_id", FieldFlag::PRIMARY |
                                                        FieldFlag::AUTO_INC |
                                                        FieldFlag::WITH_NOCHECK)
                              .field("name", 0),
                          false, {{"language_id", "language_id"}})
            .field_to_many("actors",
                           ViewBuilder("film_actor", TableFlag::WITH_NOCHECK)
                               .field("film_id", FieldFlag::PRIMARY |
                                                     FieldFlag::WITH_NOCHECK)
                               .field("actor_id", FieldFlag::PRIMARY |
                                                      FieldFlag::WITH_NOCHECK))
            .resolve(m_.get());

    SCOPED_TRACE(root1->as_graphql());

    std::string out = post_process_json(root1, {}, {}, R"*({
      "id": 1,
      "language": {
        "language_id": 1
      },
      "actors": [
        {
          "film_id": 1,
          "actor_id": 1
        }
      ]
    })*",
                                        true);
    std::string with_check_pk;
    EXPECT_EQ(
        with_check_pk = get_etag(out),
        "B9B0920E2489A09F203820EEF91F5D0739B618DE7877931E78A92708A780F5C9");

    SCOPED_TRACE(root2->as_graphql());

    out = post_process_json(root2, {}, {}, R"*({
      "id": 1,
      "language": {
        "language_id": 1
      },
      "actors": [
        {
          "film_id": 1,
          "actor_id": 1
        }
      ]
    })*",
                            true);
    EXPECT_NE(get_etag(out), with_check_pk);
  }
  // completely NOCHECK
  {
    auto root =
        DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_NOCHECK)
            .field("id", "film_id",
                   FieldFlag::PRIMARY | FieldFlag::AUTO_INC |
                       FieldFlag::WITH_NOCHECK)
            .field("title")
            .field("description", 0)
            .field_to_one("language",
                          ViewBuilder("language", TableFlag::WITH_NOCHECK)
                              .field("language_id", FieldFlag::PRIMARY |
                                                        FieldFlag::AUTO_INC |
                                                        FieldFlag::WITH_NOCHECK)
                              .field("name", 0),
                          false, {{"language_id", "language_id"}})
            .field_to_many("actors",
                           ViewBuilder("film_actor", TableFlag::WITH_NOCHECK)
                               .field("film_id", FieldFlag::PRIMARY |
                                                     FieldFlag::WITH_NOCHECK)
                               .field("actor_id", FieldFlag::PRIMARY |
                                                      FieldFlag::WITH_NOCHECK))
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    std::string out = post_process_json(root, {}, {}, R"*({
      "id": 1,
      "language": {
        "language_id": 1
      },
      "actors": [
        {
          "film_id": 1,
          "actor_id": 1
        }
      ]
    })*",
                                        true);
    EXPECT_EQ(get_etag(out), "");
  }
}
