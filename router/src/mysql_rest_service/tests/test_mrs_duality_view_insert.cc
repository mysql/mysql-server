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
#include "mrs/database/duality_view/insert.h"
#include "mrs/database/query_rest_table_updater.h"
#include "mysqlrouter/base64.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;
using namespace mrs::database::dv;

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;

class DualityViewInsert : public DatabaseRestTableTest {
 public:
  PrimaryKeyColumnValues insert_e(std::shared_ptr<DualityView> view,
                                  const std::string &input,
                                  const ObjectRowOwnership &row_owner = {}) {
    try {
      return insert(view, input, row_owner);
    } catch (const JSONInputError &e) {
      ADD_FAILURE() << "INSERT threw JSONInputError: " << e.what();
      throw;
    } catch (const DualityViewError &e) {
      ADD_FAILURE() << "INSERT threw DualityViewError: " << e.what();
      throw;
    } catch (const MySQLError &e) {
      ADD_FAILURE() << "INSERT threw MySQLError: " << e.what();
      throw;
    } catch (const std::runtime_error &e) {
      ADD_FAILURE() << "INSERT threw runtime_error: " << e.what();
      throw;
    }
  }

  PrimaryKeyColumnValues insert(std::shared_ptr<DualityView> view,
                                const std::string &input,
                                const ObjectRowOwnership &row_owner = {}) {
    DualityViewUpdater dvu(view, row_owner);

    auto json = make_json(input);
    if (!json.IsObject()) {
      std::cout << input << "\n";
    }
    assert(json.IsObject());

    return dvu.insert(m_.get(), json);
  }

  void test_insert(std::shared_ptr<DualityView> view, const std::string &templ,
                   std::vector<int> &ids,
                   const ObjectRowOwnership &row_owner = {}) {
    SCOPED_TRACE(view->as_graphql(true));

    std::string input, expected_output;
    process_template(templ, ids, &input, &expected_output);
    insert(view, input, row_owner);
  }

  void expect_insert(std::shared_ptr<DualityView> view,
                     const std::string &templ, std::vector<int> &ids) {
    std::string input, expected_output;
    process_template(templ, ids, &input, &expected_output);
    SCOPED_TRACE(input);

    PrimaryKeyColumnValues pk;
    EXPECT_NO_THROW(pk = insert_e(view, input));
    auto output = select_one(view, pk, {}, {}, false);
    EXPECT_EQ(pprint_json(expected_output), pprint_json(output));
  }
};

#define EXPECT_INSERT(f, input, ids) \
  do {                               \
    SCOPED_TRACE("");                \
    expect_insert(f, input, ids);    \
  } while (0)

TEST_F(DualityViewInsert, root_noinsert) {
  prepare(TestSchema::PLAIN);

  auto root =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_UPDATE | TableFlag::WITH_NOINSERT)
          .field("id")
          .field("data", "data1")
          .resolve(m_.get(), true);

  SCOPED_TRACE(root->as_graphql());

  // new pk
  EXPECT_DUALITY_ERROR(insert(root, R"*({
    "id": 123,
    "data": "Test"
  })*"),
                       "Duality View does not allow INSERT for table `root`");

  // omitted pk
  EXPECT_JSON_ERROR(insert(root, R"*({
    "data": "Test"
  })*"),
                    "ID for table `root` missing in JSON input");

  // null pk
  EXPECT_DUALITY_ERROR(insert(root, R"*({
    "id": null,
    "data": "Test"
  })*"),
                       "Duality View does not allow INSERT for table `root`");

  // existing pk
  EXPECT_DUALITY_ERROR(insert(root, R"*({
    "id": 1,
    "data": "Test"
  })*"),
                       "Duality View does not allow INSERT for table `root`");
}

TEST_F(DualityViewInsert, root_insert) {
  prepare(TestSchema::PLAIN);

  auto root = DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
                  .field("id")
                  .field("data", "data1")
                  .resolve(m_.get(), true);

  SCOPED_TRACE(root->as_graphql());

  std::vector<int> ids = {100};
  // new pk
  EXPECT_INSERT(root, R"*({
    "id": <id0>,
    "data": "Test"
  })*",
                ids);

  // omitted pk
  EXPECT_JSON_ERROR(insert(root, R"*({
    "data": "Test"
  })*"),
                    "ID for table `root` missing in JSON input");

  // null pk
  EXPECT_MYSQL_ERROR(insert(root, R"*({
    "id": null,
    "data": "Test"
  })*"),
                     "Column 'id' cannot be null (1048)");

  // existing pk
  EXPECT_MYSQL_ERROR(insert(root, R"*({
    "id": 1,
    "data": "Test"
  })*"),
                     "Duplicate entry '1' for key 'root.PRIMARY' (1062)");
}

TEST_F(DualityViewInsert, root_autoinc) {
  prepare(TestSchema::AUTO_INC);

  auto root =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", TableFlag::WITH_UPDATE |
                                                    TableFlag::WITH_NOCHECK)
                            .field("id", FieldFlag::AUTO_INC)
                            .field("data"))
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_INSERT |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id", FieldFlag::AUTO_INC)
                             .field("data"))
          .resolve(m_.get(), true);

  auto root_1n_update =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", TableFlag::WITH_UPDATE |
                                                    TableFlag::WITH_NOCHECK)
                            .field("id", FieldFlag::AUTO_INC)
                            .field("data"))
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_UPDATE |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id", FieldFlag::AUTO_INC)
                             .field("data"))
          .resolve(m_.get(), true);

  SCOPED_TRACE(root->as_graphql());

  const auto test_nopk = R"*({
    <<o:"id": <id0>,>>
    "data": "Test"<<o:,
    "child11": {},
    "child1n": []>>
  })*";

  const auto test_nullpk = R"*({
    <<i:"id": null,>>
    <<o:"id": <id0>,>>
    "data": "Test"<<o:,
    "child11": {},
    "child1n": []>>
  })*";

  const auto test_newpk = R"*({
    "id": <id0>,
    "data": "Test"<<o:,
    "child11": {},
    "child1n": []>>
  })*";

  const auto test_duppk = R"*({
    "id": 1,
    "data": "Test"<<o:,
    "child11": {},
    "child1n": []>>
  })*";

  // ids[0] = next auto_inc value
  std::vector<int> ids{0, 100, 200};
  ids[0] = std::stoi(next_auto_inc("root"));
  EXPECT_INSERT(root, test_nopk, ids);
  EXPECT_INSERT(root, test_nullpk, ids);
  EXPECT_INSERT(root, test_newpk, ids);
  EXPECT_MYSQL_ERROR(test_insert(root, test_duppk, ids),
                     "Duplicate entry '1' for key 'root.PRIMARY' (1062)");

  // with children
  const auto test_nested_nopk = R"*({
    <<o:"id": <id0>,>>
    "data": "Test<id0>",
    "child11": {
      "id": 20<<o:,
      "data": "ref11-1">>
    },
    "child1n": [
      {
        "id": <id1>,
        "data": "new<id1>"
      }
    ]
  })*";

  const auto test_nested_newpk1 = R"*({
    "id": <id0>,
    "data": "Test<id0>",
    "child11": {
      "id": 20<<o:,
      "data": "ref11-1">>
    },
    "child1n": [
      {
        "id": 3<<o:,
        "data": "ref1n-3">>
      }
    ]
  })*";

  const auto test_nested_newpk2 = R"*({
    "id": <id0>,
    "data": "Test<id0>",
    "child11": {
      "id": 20<<o:,
      "data": "ref11-1">>
    },
    "child1n": [
      {
        "id": <id1>,
        "data": "new<id1>"
      }
    ]
  })*";

  // (should steal child1n 3 and insert new 100)
  EXPECT_MYSQL_ERROR(test_insert(root, test_nested_newpk1, ids),
                     "Duplicate entry '3' for key 'child_1n.PRIMARY' (1062)");
  EXPECT_INSERT(root, test_nested_newpk2, ids);
  EXPECT_INSERT(root, test_nested_nopk, ids);

  EXPECT_INSERT(root_1n_update, test_nested_newpk1, ids);
  EXPECT_DUALITY_ERROR(
      test_insert(root_1n_update, test_nested_newpk2, ids),
      "Duality View does not allow INSERT for table `child_1n`");
  EXPECT_DUALITY_ERROR(
      test_insert(root_1n_update, test_nested_nopk, ids),
      "Duality View does not allow INSERT for table `child_1n`");
}

TEST_F(DualityViewInsert, root_uuid) {
  prepare(TestSchema::UUID);

  auto root =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id", FieldFlag::REV_UUID)
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", TableFlag::WITH_UPDATE |
                                                    TableFlag::WITH_NOCHECK)
                            .field("id", FieldFlag::REV_UUID)
                            .field("data"))
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_INSERT |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id", FieldFlag::REV_UUID)
                             .field("data"))
          .resolve(m_.get(), true);

  size_t next_uuid = 0;
  std::vector<std::string> uuids = {
      "MTIzAAAAAAAAAAAAAAAAAA==", "MTI0AAAAAAAAAAAAAAAAAA==",
      "MTI1AAAAAAAAAAAAAAAAAA==", "MTI2AAAAAAAAAAAAAAAAAA==",
      "MTI3AAAAAAAAAAAAAAAAAA==", "MTI4AAAAAAAAAAAAAAAAAA==",
      "MTI5AAAAAAAAAAAAAAAAAA=="};

  auto get_uuid = [&next_uuid, uuids](mysqlrouter::MySQLSession *) {
    assert(next_uuid < uuids.size());
    return mysqlrouter::sqlstring("?") << Base64::decode(uuids[next_uuid++]);
  };

  mrs::database::dv::ReverseUuidRowInsert::set_generate_uuid(get_uuid);

  std::vector<int> ids;

  // new pk
  EXPECT_INSERT(root, R"*({
    "id": "ZDIzAAAAAAAAAAAAAAAAAA==",
    "data": "Test"<<o:,
    "child11": {},
    "child1n": []>>
  })*",
                ids);

  // omitted pk
  EXPECT_INSERT(root, R"*({
    <<o:"id": "MTIzAAAAAAAAAAAAAAAAAA==",>>
    "data": "Test2"<<o:,
    "child11": {},
    "child1n": []>>
  })*",
                ids);

  // null pk
  EXPECT_INSERT(root, R"*({
    <<i:"id": null,>>
    <<o:"id": "MTI0AAAAAAAAAAAAAAAAAA==",>>
    "data": "Test3"<<o:,
    "child11": {},
    "child1n": []>>
  })*",
                ids);

  // existing pk
  EXPECT_MYSQL_ERROR(test_insert(root, R"*({
    "id": "ZDIzAAAAAAAAAAAAAAAAAA==",
    "data": "Test"<<o:,
    "child11": {},
    "child1n": []>>
  })*",
                                 ids),
                     "Duplicate entry 'd23' for key 'root.PRIMARY' (1062)");

  // with children
  const auto test_nested_nopk = R"*({
    <<o:"id": "MTI1AAAAAAAAAAAAAAAAAA==",>>
    "data": "TestC1",
    "child11": {
      "id": "IAAAAAAAAAAAAAAAAAAAAA=="<<o:,
      "data": "ref11-1">>
    },
    "child1n": [
      {
        "id": "MTI1AAAAAAAAAAAAAAAAAA==",
        "data": "new"
      }
    ]
  })*";

  const auto test_nested_newpk1 = R"*({
    "id": "XTI2AAAAAAAAAAAAAAAAAA==",
    "data": "TestC2",
    "child11": {
      "id": "IAAAAAAAAAAAAAAAAAAAAA=="<<o:,
      "data": "ref11-1">>
    },
    "child1n": [
      {
        "id": "MXAAAAAAAAAAAAAAAAAAAA=="<<o:,
        "data": null>>
      }
    ]
  })*";

  const auto test_nested_newpk2 = R"*({
    "id": "XTI3AAAAAAAAAAAAAAAAAA==",
    "data": "TestC3",
    "child11": {
      "id": "IAAAAAAAAAAAAAAAAAAAAA=="<<o:,
      "data": "ref11-1">>
    },
    "child1n": [
      {
        <<o:"id": "MTI2AAAAAAAAAAAAAAAAAA==",>>
        "data": "new!!!"
      }
    ]
  })*";

  EXPECT_INSERT(root, test_nested_nopk, ids);
  EXPECT_INSERT(root, test_nested_newpk1, ids);
  EXPECT_INSERT(root, test_nested_newpk2, ids);
}

TEST_F(DualityViewInsert, child11) {
  prepare(TestSchema::PLAIN);

  auto root =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id")
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", TableFlag::WITH_NOCHECK)
                            .field("id")
                            .field("data"))
          .resolve(m_.get(), true);

  auto root_update =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id")
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", TableFlag::WITH_UPDATE |
                                                    TableFlag::WITH_NOCHECK)
                            .field("id")
                            .field("data"))
          .resolve(m_.get(), true);

  const auto test_empty = R"*({
    "id": <id0>,
    "data": "Test",
    "child11": {}
  })*";

  const auto test_noval = R"*({
    "id": <id0>,
    "data": "Test"<<o:,
    "child11": {}>>
  })*";

  const auto test_nopk = R"*({
    "id": <id0>,
    "data": "Test",
    "child11": {
      "data": "child1"
    }
  })*";

  const auto test_newpk = R"*({
    "id": <id0>,
    "data": "Test",
    "child11": {
      "id": <id1>,
      "data": "child1"
    }
  })*";

  const auto test_duppk1 = R"*({
    "id": <id0>,
    "data": "Test",
    "child11": {
      "id": 20,
      "data": "ref11-1"
    }
  })*";

  const auto test_duppk2 = R"*({
    "id": <id0>,
    "data": "Test",
    "child11": {
      "id": 21,
      "data": "stolen<id1>"
    }
  })*";

  const auto test_duppk2_noup = R"*({
    "id": <id0>,
    "data": "Test",
    "child11": {
      "id": 21,
      <<i:"data": "stolen<id2>">>
      <<o:"data": "stolen<id1>">>
    }
  })*";

  const auto test_duppk3 = R"*({
    "id": <id0>,
    "data": "Test",
    "child11": {
      "id": 22<<o:,
      "data": "ref11-3">>
    }
  })*";

  std::vector<int> ids{100, 200};

  EXPECT_INSERT(root, test_noval, ids);
  EXPECT_INSERT(root, test_empty, ids);
  EXPECT_JSON_ERROR(test_insert(root, test_nopk, ids),
                    "ID for table `child_11` missing in JSON input");
  // new pk
  // in oracle this throws a NOINSERT error, but this might be ok too
  EXPECT_MYSQL_ERROR(test_insert(root, test_newpk, ids),
                     "a foreign key constraint fails");

  // existing pk - UPDATE
  EXPECT_INSERT(root_update, test_duppk1, ids);
  auto saved_duppk2_ids = ids;
  EXPECT_INSERT(root_update, test_duppk2, ids);
  EXPECT_INSERT(root_update, test_duppk3, ids);

  // existing pk - NOUPDATE
  // all should succeed, but the child update should silently fail
  EXPECT_INSERT(root, test_duppk1, ids);
  saved_duppk2_ids.push_back(300);
  saved_duppk2_ids[0] = ids[0];
  // attempt to change "data" should be ignored
  EXPECT_INSERT(root, test_duppk2_noup, saved_duppk2_ids);
  ids[0] = saved_duppk2_ids[0];
  EXPECT_INSERT(root, test_duppk3, ids);
}

TEST_F(DualityViewInsert, child1n) {
  prepare(TestSchema::PLAIN);

  auto root =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id")
          .field("data", "data1")
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_NOCHECK)
                             .field("id")
                             .field("data"))
          .resolve(m_.get(), true);

  auto root_update =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id")
          .field("data", "data1")
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_UPDATE |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id")
                             .field("data"))
          .resolve(m_.get(), true);

  auto root_upsert =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id")
          .field("data", "data1")
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_UPDATE |
                                                     TableFlag::WITH_INSERT |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id")
                             .field("data"))
          .resolve(m_.get(), true);

  auto root_insert =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id")
          .field("data", "data1")
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_INSERT |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id")
                             .field("data"))
          .resolve(m_.get(), true);

  const auto test_nopk = R"*({
    "id": <id0>,
    "data": "Test",
    "child1n": [{
      "data": "child1"
    },
    {
      "id": null,
      "data": "child2"
    }]
  })*";

  const auto test_newpk = R"*({
    "id": <id0>,
    "data": "Test",
    "child1n": [{
      "id": <id1>,
      "data": "child1"
    },
    {
      "id": <id2>,
      "data": "child2"
    }]
  })*";

  const auto test_duppk = R"*({
    "id": <id0>,
    "data": "Test",
    "child1n": [{
      "id": 1,
      "data": "ref1n-1"
    },
    {
      "id": 2,
      "data": "stolen"
    },
    {
      "id": 3<<o:,
      "data": "ref1n-3">>
    }
    ]
  })*";

  std::vector<int> ids = {100, 200, 300};

  EXPECT_JSON_ERROR(test_insert(root, test_nopk, ids),
                    "ID for table `child_1n` missing in JSON input");
  EXPECT_DUALITY_ERROR(test_insert(root, test_newpk, ids),
                       "Duality View does not allow INSERT");
  EXPECT_DUALITY_ERROR(test_insert(root, test_duppk, ids),
                       "Duality View does not allow INSERT");

  EXPECT_JSON_ERROR(test_insert(root_insert, test_nopk, ids),
                    "ID for table `child_1n` missing in JSON input");
  EXPECT_INSERT(root_insert, test_newpk, ids);
  EXPECT_MYSQL_ERROR(test_insert(root_insert, test_duppk, ids),
                     "Duplicate entry '1' for key 'child_1n.PRIMARY' (1062)");

  EXPECT_JSON_ERROR(test_insert(root_update, test_nopk, ids),
                    "ID for table `child_1n` missing in JSON input");
  EXPECT_DUALITY_ERROR(
      test_insert(root_update, test_newpk, ids),
      "Duality View does not allow INSERT for table `child_1n`");
  EXPECT_INSERT(root_update, test_duppk, ids);

  EXPECT_JSON_ERROR(test_insert(root_upsert, test_nopk, ids),
                    "ID for table `child_1n` missing in JSON input");
  EXPECT_INSERT(root_upsert, test_newpk, ids);  // child inserted
  EXPECT_INSERT(root_upsert, test_duppk, ids);  // child updated
}

TEST_F(DualityViewInsert, child1n_autoinc) {
  prepare(TestSchema::AUTO_INC);

  auto root_insert_insert =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_INSERT |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id", FieldFlag::AUTO_INC)
                             .field("data"))
          .resolve(m_.get(), true);

  auto root_insert_update =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_UPDATE |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id", FieldFlag::AUTO_INC)
                             .field("data"))
          .resolve(m_.get(), true);

  auto root_insert_upsert =
      DualityViewBuilder("mrstestdb", "root",
                         TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_INSERT |
                                                     TableFlag::WITH_UPDATE |
                                                     TableFlag::WITH_NOCHECK)
                             .field("id", FieldFlag::AUTO_INC)
                             .field("data"))
          .resolve(m_.get(), true);

  const auto test_nopk = R"*({
    "id": <id0>,
    "data": "Test",
    "child1n": [{
      <<o:"id": <id1++>,>>
      "data": "child1"
    },
    {
      <<i:"id": null,>>
      <<o:"id": <id1++>,>>
      "data": "child2"
    }]
  })*";

  const auto test_newpk = R"*({
    "id": <id0>,
    "data": "Test",
    "child1n": [{
      "id": <id2++>,
      "data": "child1"
    },
    {
      "id": <id2++>,
      "data": "child2"
    }]
  })*";

  const auto test_duppk = R"*({
    "id": <id0>,
    "data": "Test",
    "child1n": [{
      "id": 1,
      "data": "ref1n-1"
    },
    {
      "id": 2,
      "data": "stolen"
    },
    {
      "id": 3<<o:,
      "data": "ref1n-3">>
    }
    ]
  })*";

  std::vector<int> ids = {100, std::stoi(next_auto_inc("child_1n")), 100};
  EXPECT_INSERT(root_insert_insert, test_nopk, ids);
  EXPECT_JSON_ERROR(test_insert(root_insert_update, test_nopk, ids),
                    "ID for table `child_1n` missing in JSON input");
  ids[1] = std::stoi(next_auto_inc("child_1n"));
  EXPECT_INSERT(root_insert_upsert, test_nopk, ids);

  EXPECT_INSERT(root_insert_insert, test_newpk, ids);
  EXPECT_MYSQL_ERROR(test_insert(root_insert_insert, test_duppk, ids),
                     "Duplicate entry '1' for key 'child_1n.PRIMARY' (1062)");

  EXPECT_DUALITY_ERROR(
      test_insert(root_insert_update, test_newpk, ids),
      "Duality View does not allow INSERT for table `child_1n`");
  EXPECT_INSERT(root_insert_update, test_duppk, ids);

  EXPECT_INSERT(root_insert_upsert, test_newpk, ids);  // child inserted
  EXPECT_INSERT(root_insert_upsert, test_duppk, ids);  // child updated
}

TEST_F(DualityViewInsert, deep_nested_autoinc) {
  prepare(TestSchema::AUTO_INC);

  auto root =
      DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_many(
              "child",
              ViewBuilder("child_1n", TableFlag::WITH_INSERT)
                  .field("id", FieldFlag::AUTO_INC)
                  .field("value", "data")
                  .field_to_many("gchild", ViewBuilder("child_1n_1n",
                                                       TableFlag::WITH_INSERT)
                                               .field("id", FieldFlag::AUTO_INC)
                                               .field("data")))
          .resolve(m_.get(), true);

  std::vector<int> ids{std::stoi(next_auto_inc("root")),
                       std::stoi(next_auto_inc("child_1n")),
                       std::stoi(next_auto_inc("child_1n_1n"))};
  EXPECT_INSERT(root, R"*({
    <<o:"id": <id0>,>>
    "data": "The Root",
    "child": [
      {
        <<o:"id": <id1++>,>>
        "value": "Child1",
        "gchild": [
          {
            <<o:"id": <id2++>,>>
            "data": "GrandChild1"
          },
          {
            <<o:"id": <id2++>,>>
            "data": "GrandChild2"
          }
        ]
      },
      {
        <<o:"id": <id1++>,>>
        "value": "Child2",
        "gchild": [
          {
            <<o:"id": <id2++>,>>
            "data": "GrandChild3"
          },
          {
            <<o:"id": <id2++>,>>
            "data": "GrandChild4"
          }
        ]
      }
    ]
})*",
                ids);
}

TEST_F(DualityViewInsert, unnest_11) {
  prepare(TestSchema::AUTO_INC);

  auto root =
      DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
          .field("_id", "id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", TableFlag::WITH_UPDATE)
                            .field("child11Id", "id", FieldFlag::AUTO_INC)
                            .field("child11Data", "data"),
                        true)
          .field_to_many(
              "childnm",
              ViewBuilder("child_nm_join", TableFlag::WITH_INSERT)
                  .field("root_id")
                  .field("child_id")
                  .field_to_one("", ViewBuilder("child_nm", 0).field("data"),
                                true))
          .resolve(m_.get(), true);
  SCOPED_TRACE(root->as_graphql());

  std::vector<int> ids;

  EXPECT_INSERT(root, R"*({
  "_id": 1001,
  "data": "Hello",
  "child11Id": 20,
  "child11Data": "World",
  "childnm": []
})*",
                ids);

  // the insert should be executed, but it will fail because of a NOT NULL
  // constraint error on omitted child_11
  EXPECT_MYSQL_ERROR(test_insert(root, R"*({
  "_id": 1002,
  "data": "Hello",
  "childnm": [
    { 
      "root_id": 1002,
      "child_id": 1,
      "data": "DATA1"
    }
  ]
})*",
                                 ids),
                     "Column 'child_id' cannot be null");
}

TEST_F(DualityViewInsert, unnest_1n) {
  prepare(TestSchema::AUTO_INC);

  auto root =
      DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
          .field("_id", "id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_INSERT)
                             .field("child1nId", "id",
                                    FieldFlag::AUTO_INC | FieldFlag::DISABLED)
                             .field("data"),
                         true)
          .resolve(m_.get());
  SCOPED_TRACE(root->as_graphql());

  std::vector<int> ids;

  EXPECT_DUALITY_ERROR(test_insert(root, R"*({
  "_id": 1001,
  "data": "Hello",
  "child1n": ["Test"]
})*",
                                   ids),
                       "Duality View is read-only");
}

TEST_F(DualityViewInsert, inconsistent_input) {
  // FKs are usually omitted, but they can be required if they're also the PK
  prepare(TestSchema::AUTO_INC);
  {
    auto root =
        DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_INSERT)
            .field("_id", "id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", TableFlag::WITH_INSERT)
                    .field("root_id")
                    .field("child_id")
                    .field_to_one(
                        "",
                        ViewBuilder("child_nm", 0).field("id").field("data"),
                        true))
            .resolve(m_.get(), true);
    SCOPED_TRACE(root->as_graphql());

    std::vector<int> ids;

    EXPECT_JSON_ERROR(test_insert(root, R"*({
  "_id": 1001,
  "data": "Hello",
  "childnm": [
    { 
      "root_id": 1002,
      "child_id": 1,
      "id":1,
      "data": "DATA1"
    }
  ]
})*",
                                  ids),
                      "Value for column `root_id` of table `child_nm_join` "
                      "does not match referenced ID");

    EXPECT_JSON_ERROR(test_insert(root, R"*({
  "_id": 1001,
  "data": "Hello",
  "childnm": [
    { 
      "root_id": 1001,
      "child_id": 1,
      "id":2,
      "data": "DATA2"
    }
  ]
})*",
                                  ids),
                      "Value for column `id` of table `child_nm` does not "
                      "match referenced ID");

    EXPECT_JSON_ERROR(test_insert(root, R"*({
  "_id": 1001,
  "data": "Hello",
  "childnm": [
    { 
      "id":2,
      "root_id": 1001,
      "child_id": 1,
      "data": "DATA2"
    }
  ]
})*",
                                  ids),
                      "Value for column `id` of table `child_nm` does not "
                      "match referenced ID");
  }
}