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
#include "mrs/database/duality_view/select.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;

class DualityViewSelect : public DatabaseRestTableTest {
 public:
  void SetUp() override {
    select_include_links_ = true;
    DatabaseRestTableTest::SetUp();
  }

  void expect_select_one(std::shared_ptr<DualityView> view,
                         const PrimaryKeyColumnValues &pk,
                         const std::string &expected) {
    auto output = select_one(view, pk);
    EXPECT_EQ(pprint_json(expected), pprint_json(output));
  }
};

#define EXPECT_SELECT_ONE(f, output, pk) \
  do {                                   \
    SCOPED_TRACE("");                    \
    expect_select_one(f, output, pk);    \
  } while (0)

TEST_F(DualityViewSelect, select_one) {
  prepare(TestSchema::AUTO_INC);

  auto root =
      DualityViewBuilder("mrstestdb", "root", 0)
          .field("_id", "id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", 0)
                            .field("child11Id", "id", FieldFlag::AUTO_INC)
                            .field("child11Data", "data")
                            .field_to_one("child1111",
                                          ViewBuilder("child_11_11", 0)
                                              .field("child1111Id", "id",
                                                     FieldFlag::AUTO_INC)
                                              .field("child1111Data", "data")))
          .field_to_many(
              "child1n",
              ViewBuilder("child_1n", 0)
                  .field("chld1nId", "id", FieldFlag::AUTO_INC)
                  .field("child1nData", "data")
                  .field_to_many(
                      "child1n1n",
                      ViewBuilder("child_1n_1n", 0)
                          .field("child1n1nId", "id", FieldFlag::AUTO_INC)
                          .field("child1n1nData", "data")))
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

  // 9 as string is intentional to test passing int as string for key
  EXPECT_SELECT_ONE(root, parse_pk(R"*({"id": "9"})*"),
                    R"*({
  "_id": 9,
  "data": "hello",
  "links": [
    {
      "rel": "self",
      "href": "localhost/9"
    }
  ],
  "child11": {
    "child1111": {
      "child1111Id": 10,
      "child1111Data": "abc-1"
    },
    "child11Id": 21,
    "child11Data": "ref11-2"
  },
  "child1n": [
    {
      "chld1nId": 4,
      "child1n1n": [
        {
          "child1n1nId": 30,
          "child1n1nData": "1n1n-1"
        },
        {
          "child1n1nId": 31,
          "child1n1nData": "1n1n-2"
        }
      ],
      "child1nData": "ref1n-4"
    },
    {
      "chld1nId": 5,
      "child1n1n": [],
      "child1nData": "ref1n-5"
    },
    {
      "chld1nId": 6,
      "child1n1n": [
        {
          "child1n1nId": 32,
          "child1n1nData": "1n1n-3"
        }
      ],
      "child1nData": "ref1n-6"
    }
  ],
  "childnm": [
    {
      "child": {
        "childnmId": 2
      },
      "nmRootId": 9,
      "nmChildId": 2
    },
    {
      "child": {
        "childnmId": 3
      },
      "nmRootId": 9,
      "nmChildId": 3
    }
  ],
  "_metadata": {
    "etag": "A08F96579315F2846F86E85B15E9D6962AA3C33579DE165ACE06B5CF99E8B88B"
  }
})*");
}

TEST_F(DualityViewSelect, unnest_11) {
  prepare(TestSchema::AUTO_INC);
  {
    auto root =
        DualityViewBuilder("mrstestdb", "root", 0)
            .field("_id", "id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0)
                    .field("child11Id", "id", FieldFlag::AUTO_INC)
                    .field("child11Data", "data")
                    .field_to_one(
                        "child1111",
                        ViewBuilder("child_11_11", 0)
                            .field("child1111Id", "id", FieldFlag::AUTO_INC)
                            .field("child1111Data", "data")),
                true)
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", 0)
                    .field("chld1nId", "id", FieldFlag::AUTO_INC)
                    .field_to_many(
                        "child1n1n",
                        ViewBuilder("child_1n_1n", 0)
                            .field("child1n1nId", "id", FieldFlag::AUTO_INC)
                            .field("child1n1nData", "data")))
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

    EXPECT_SELECT_ONE(root, parse_pk(R"*({"id": "9"})*"),
                      R"*({
  "_id": 9,
  "data": "hello",
  "links": [
    {
      "rel": "self",
      "href": "localhost/9"
    }
  ],
  "child1111": {
    "child1111Id": 10,
    "child1111Data": "abc-1"
  },
  "child11Id": 21,
  "child11Data": "ref11-2",
  "child1n": [
    {
      "chld1nId": 4,
      "child1n1n": [
        {
          "child1n1nId": 30,
          "child1n1nData": "1n1n-1"
        },
        {
          "child1n1nId": 31,
          "child1n1nData": "1n1n-2"
        }
      ]
    },
    {
      "chld1nId": 5,
      "child1n1n": []
    },
    {
      "chld1nId": 6,
      "child1n1n": [
        {
          "child1n1nId": 32,
          "child1n1nData": "1n1n-3"
        }
      ]
    }
  ],
  "childnm": [
    {
      "child": {
        "childnmId": 2
      },
      "nmRootId": 9,
      "nmChildId": 2
    },
    {
      "child": {
        "childnmId": 3
      },
      "nmRootId": 9,
      "nmChildId": 3
    }
  ],
  "_metadata": {
    "etag": "7020BC3FFF6EFBA315C2A812FD8AD3AE94ECA6E7526D70845DDE65D4FB562C5A"
  }
})*");
  }
  {
    auto root =
        DualityViewBuilder("mrstestdb", "root", 0)
            .field("_id", "id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one("child11",
                          ViewBuilder("child_11", 0)
                              .field("child11Id", "id", FieldFlag::AUTO_INC)
                              .field("child11Data", "data")
                              .field_to_one("child1111",
                                            ViewBuilder("child_11_11", 0)
                                                .field("child1111Id", "id",
                                                       FieldFlag::AUTO_INC)
                                                .field("child1111Data", "data"),
                                            true),
                          true)
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", 0)
                    .field("chld1nId", "id", FieldFlag::AUTO_INC)
                    .field_to_many(
                        "child1n1n",
                        ViewBuilder("child_1n_1n", 0)
                            .field("child1n1nId", "id", FieldFlag::AUTO_INC)
                            .field("child1n1nData", "data")))
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

    EXPECT_SELECT_ONE(root, parse_pk(R"*({"id": "9"})*"),
                      R"*({
  "_id": 9,
  "data": "hello",
  "links": [
    {
      "rel": "self",
      "href": "localhost/9"
    }
  ],
  "child1111Id": 10,
  "child1111Data": "abc-1",
  "child11Id": 21,
  "child11Data": "ref11-2",
  "child1n": [
    {
      "chld1nId": 4,
      "child1n1n": [
        {
          "child1n1nId": 30,
          "child1n1nData": "1n1n-1"
        },
        {
          "child1n1nId": 31,
          "child1n1nData": "1n1n-2"
        }
      ]
    },
    {
      "chld1nId": 5,
      "child1n1n": []
    },
    {
      "chld1nId": 6,
      "child1n1n": [
        {
          "child1n1nId": 32,
          "child1n1nData": "1n1n-3"
        }
      ]
    }
  ],
  "childnm": [
    {
      "child": {
        "childnmId": 2
      },
      "nmRootId": 9,
      "nmChildId": 2
    },
    {
      "child": {
        "childnmId": 3
      },
      "nmRootId": 9,
      "nmChildId": 3
    }
  ],
  "_metadata": {
    "etag": "7020BC3FFF6EFBA315C2A812FD8AD3AE94ECA6E7526D70845DDE65D4FB562C5A"
  }
})*");
  }
}

TEST_F(DualityViewSelect, unnest_1n) {
  prepare(TestSchema::AUTO_INC);
  {
    auto root =
        DualityViewBuilder("mrstestdb", "root", 0)
            .field("_id", "id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0)
                    .field("child11Id", "id", FieldFlag::AUTO_INC)
                    .field("child11Data", "data")
                    .field_to_one(
                        "child1111",
                        ViewBuilder("child_11_11", 0)
                            .field("child1111Id", "id", FieldFlag::AUTO_INC)
                            .field("child1111Data", "data")))
            .field_to_many("child1n",
                           ViewBuilder("child_1n", 0)
                               .field("chld1nId", "id",
                                      FieldFlag::AUTO_INC | FieldFlag::DISABLED)
                               .field("child1nData", "data"),
                           true)
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

    EXPECT_SELECT_ONE(root, parse_pk(R"*({"id": 9})*"),
                      R"*({
      "_id": 9,
      "data": "hello",
      "links": [
        {
          "rel": "self",
          "href": "localhost/9"
        }
      ],
      "child11": {
        "child1111": {
          "child1111Id": 10,
          "child1111Data": "abc-1"
        },
        "child11Id": 21,
        "child11Data": "ref11-2"
      },
      "child1n": [
        "ref1n-4",
        "ref1n-5",
        "ref1n-6"
      ],
      "childnm": [
        {
          "child": {
            "childnmId": 2
          },
          "nmRootId": 9,
          "nmChildId": 2
        },
        {
          "child": {
            "childnmId": 3
          },
          "nmRootId": 9,
          "nmChildId": 3
        }
      ],
      "_metadata": {
        "etag":
        "F2BE797265680E11C77D5CD76B2462B21200E6B3E4AD3EC321318D725397F153"
      }
    })*");
  }
  {
    auto root =
        DualityViewBuilder("mrstestdb", "root", 0)
            .field("_id", "id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0)
                    .field("child11Id", "id", FieldFlag::AUTO_INC)
                    .field("child11Data", "data")
                    .field_to_one(
                        "child1111",
                        ViewBuilder("child_11_11", 0)
                            .field("child1111Id", "id", FieldFlag::AUTO_INC)
                            .field("child1111Data", "data")))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", 0)
                    .field("chld1nId", "id", FieldFlag::AUTO_INC)
                    .field("child1nData", "data")
                    .field_to_many(
                        "child1n1n",
                        ViewBuilder("child_1n_1n", 0)
                            .field("child1n1nId", "id",
                                   FieldFlag::AUTO_INC | FieldFlag::DISABLED)
                            .field("child1n1nData", "data"),
                        true))
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

    EXPECT_SELECT_ONE(root, parse_pk(R"*({"id": "9"})*"),
                      R"*({
  "_id": 9,
  "data": "hello",
  "links": [
    {
      "rel": "self",
      "href": "localhost/9"
    }
  ],
  "child11": {
    "child1111": {
      "child1111Id": 10,
      "child1111Data": "abc-1"
    },
    "child11Id": 21,
    "child11Data": "ref11-2"
  },
  "child1n": [
    {
      "chld1nId": 4,
      "child1n1n": [
        "1n1n-1",
        "1n1n-2"
      ],
      "child1nData": "ref1n-4"
    },
    {
      "chld1nId": 5,
      "child1n1n": [],
      "child1nData": "ref1n-5"
    },
    {
      "chld1nId": 6,
      "child1n1n": [
        "1n1n-3"
      ],
      "child1nData": "ref1n-6"
    }
  ],
  "childnm": [
    {
      "child": {
        "childnmId": 2
      },
      "nmRootId": 9,
      "nmChildId": 2
    },
    {
      "child": {
        "childnmId": 3
      },
      "nmRootId": 9,
      "nmChildId": 3
    }
  ],
  "_metadata": {
    "etag": "BED162D3F5FABEF85C1280862B7BECF1CCA989731E857DDD04C1A0C42958D951"
  }
})*");
  }
}

TEST_F(DualityViewSelect, unnest_nm) {
  prepare(TestSchema::AUTO_INC);
  {
    auto root =
        DualityViewBuilder("mrstestdb", "root", 0)
            .field("_id", "id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0)
                    .field("child11Id", "id", FieldFlag::AUTO_INC)
                    .field("child11Data", "data")
                    .field_to_one(
                        "child1111",
                        ViewBuilder("child_11_11", 0)
                            .field("child1111Id", "id", FieldFlag::AUTO_INC)
                            .field("child1111Data", "data")))
            .field_to_many("child1n",
                           ViewBuilder("child_1n", 0)
                               .field("chld1nId", "id", FieldFlag::AUTO_INC)
                               .field("child1nData", "data"))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", 0)
                    .field("nmRootId", "root_id", FieldFlag::DISABLED)
                    .field("nmChildId", "child_id", FieldFlag::DISABLED)
                    .field_to_one(
                        "child",
                        ViewBuilder("child_nm", 0)
                            .field("childnmId", "id",
                                   FieldFlag::AUTO_INC | FieldFlag::DISABLED)
                            .field("childnmData", "data"),
                        true),
                true)
            .resolve(m_.get());

    SCOPED_TRACE(root->as_graphql());

    EXPECT_SELECT_ONE(root, parse_pk(R"*({"id": "9"})*"),
                      R"*({
  "_id": 9,
  "data": "hello",
  "links": [
    {
      "rel": "self",
      "href": "localhost/9"
    }
  ],
  "child11": {
    "child1111": {
      "child1111Id": 10,
      "child1111Data": "abc-1"
    },
    "child11Id": 21,
    "child11Data": "ref11-2"
  },
  "child1n": [
    {
      "chld1nId": 4,
      "child1nData": "ref1n-4"
    },
    {
      "chld1nId": 5,
      "child1nData": "ref1n-5"
    },
    {
      "chld1nId": 6,
      "child1nData": "ref1n-6"
    }
  ],
  "childnm": [
      "DATA2",
      "DATA3"
  ],
  "_metadata": {
    "etag": "B43410D8CB1B9D6925D0EAAEB28A4A0E28754D7CA52851210E69AFBA048B3AD4"
  }
})*");
  }
}
