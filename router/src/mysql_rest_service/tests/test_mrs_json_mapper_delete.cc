/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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
#include "mrs/database/json_mapper/delete.h"
#include "mrs/database/query_rest_table_updater.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;
using namespace mrs::database::dv;

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;

#define EXPECT_UUID(value) EXPECT_EQ(16, unescape(value).size() - 2) << value

class JsonMappingDelete : public DatabaseRestTableTest {
 public:
  void delete_e(std::shared_ptr<JsonMapping> view,
                const PrimaryKeyColumnValues &pks,
                const ObjectRowOwnership &row_owner = {}) {
    try {
      return delete_(view, pks, row_owner);
    } catch (const JSONInputError &e) {
      ADD_FAILURE() << "DELETE threw JSONInputError: " << e.what();
      throw;
    } catch (const DataMappingViewError &e) {
      ADD_FAILURE() << "DELETE threw DataMappingViewError: " << e.what();
      throw;
    } catch (const MySQLError &e) {
      ADD_FAILURE() << "DELETE threw MySQLError: " << e.what();
      throw;
    } catch (const std::runtime_error &e) {
      ADD_FAILURE() << "DELETE threw runtime_error: " << e.what();
      throw;
    }
  }

  void delete_(std::shared_ptr<JsonMapping> view,
               const PrimaryKeyColumnValues &pks,
               const ObjectRowOwnership &row_owner = {}) {
    JsonMappingUpdater dvu(view, row_owner);

    dvu.delete_(m_.get(), pks);
  }

  void test_delete(std::shared_ptr<JsonMapping> view,
                   const PrimaryKeyColumnValues &pks,
                   const ObjectRowOwnership &row_owner = {}) {
    delete_(view, pks, row_owner);
  }

  void expect_delete(std::shared_ptr<JsonMapping> view,
                     const PrimaryKeyColumnValues &pks) {
    EXPECT_NO_THROW(delete_(view, pks));

    auto response = select_one(view, pks);
    EXPECT_EQ(response, "");
  }

  void test_delete(std::shared_ptr<JsonMapping> view,
                   const std::string &filter) {
    JsonMappingUpdater dvu(view);
    FilterObjectGenerator fog(view, true);

    fog.parse(filter);

    dvu.delete_(m_.get(), fog);
  }

  void expect_delete(std::shared_ptr<JsonMapping> view,
                     const std::string &filter) {
    EXPECT_NO_THROW(test_delete(view, filter));
  }

  void insert_rows() {
    std::vector<const char *> k_rows_autoinc = {
        R"*(INSERT INTO mrstestdb.child_11 VALUES
        (200, 'test1', null),
        (201, 'test2', null)
        )*",
        R"*(INSERT INTO mrstestdb.root VALUES
        (100, null, 200, 'data1', 12345),
        (101, null, 200, 'data1', 23456),
        (102, null, 201, 'data1', 34567),
        (103, null, null, 'data1', 45678),
        (104, null, null, 'data1', 8910)
        )*",
        R"*(INSERT INTO mrstestdb.child_1n VALUES
        (300, 'data', 100),
        (301, 'data', 100),
        (302, 'data', 101),
        (303, 'data', 101)
        )*",
        R"*(INSERT INTO mrstestdb.child_1n_1n VALUES
        (400, 'data', 300)
        )*",
        R"*(INSERT INTO mrstestdb.child_nm VALUES
        (500, 'data'),
        (501, 'data'),
        (502, 'data')
        )*",
        R"*(INSERT INTO mrstestdb.child_nm_join VALUES
        (100, 500),
        (100, 501),
        (101, 500),
        (103, 502)
        )*"};

    for (const char *row : k_rows_autoinc) {
      m_->execute(row);
    }
  }
};

#define EXPECT_DELETE(f, pks) \
  do {                        \
    SCOPED_TRACE("");         \
    expect_delete(f, pks);    \
  } while (0)

#define EXPECT_DELETE_F(f, filter) \
  do {                             \
    SCOPED_TRACE("");              \
    expect_delete(f, filter);      \
  } while (0)

TEST_F(JsonMappingDelete, key_nodelete) {
  auto reset = [&]() {
    drop_schema();
    prepare(TestSchema::AUTO_INC);
    snapshot();
  };

  reset();
  auto root1 =
      JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_UPDATE)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_one(
              "child11",
              ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
          .field_to_many(
              "child1n",
              ViewBuilder("child_1n", 0)
                  .field("id", FieldFlag::AUTO_INC)
                  .field_to_many(
                      "child1n1n",
                      ViewBuilder("child_1n_1n", 0).field("id").field("data")))
          .field_to_many(
              "childnm",
              ViewBuilder("child_nm_join", 0)
                  .field("root_id", 0)
                  .field("child_id", 0)
                  .field_to_one("child", ViewBuilder("child_nm", 0)
                                             .field("id", FieldFlag::AUTO_INC)))
          .resolve(m_.get(), true);
  SCOPED_TRACE(root1->as_graphql());

  EXPECT_DUALITY_ERROR(
      test_delete(root1, parse_pk(R"*({"id": 100})*")),
      "Data Mapping View does not allow DELETE for table `root`");
  expect_rows_added({{"root", 0},
                     {"child_1n", 0},
                     {"child_1n_1n", 0},
                     {"child_nm_join", 0},
                     {"child_nm", 0}});

  reset();
  auto root2 =
      JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_UPDATE)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_one("child11",
                        ViewBuilder("child_11", TableFlag::WITH_DELETE)
                            .field("id", FieldFlag::AUTO_INC))
          .field_to_many("child1n",
                         ViewBuilder("child_1n", TableFlag::WITH_DELETE)
                             .field("id", FieldFlag::AUTO_INC)
                             .field_to_many("child1n1n",
                                            ViewBuilder("child_1n_1n",
                                                        TableFlag::WITH_DELETE)
                                                .field("id")
                                                .field("data")))
          .field_to_many(
              "childnm",
              ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                  .field("root_id", 0)
                  .field("child_id", 0)
                  .field_to_one("child",
                                ViewBuilder("child_nm", TableFlag::WITH_DELETE)
                                    .field("id", FieldFlag::AUTO_INC)))
          .resolve(m_.get(), true);
  SCOPED_TRACE(root2->as_graphql());

  EXPECT_DUALITY_ERROR(
      test_delete(root2, parse_pk(R"*({"id": 100})*")),
      "Data Mapping View does not allow DELETE for table `root`");
  expect_rows_added({{"root", 0},
                     {"child_1n", 0},
                     {"child_1n_1n", 0},
                     {"child_nm_join", 0},
                     {"child_nm", 0}});
}

TEST_F(JsonMappingDelete, key_delete) {
  auto reset = [&]() {
    drop_schema();
    prepare(TestSchema::AUTO_INC);
    insert_rows();
    snapshot();
  };

  {
    reset();
    auto root_all =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", TableFlag::WITH_DELETE)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many(
                        "child1n1n",
                        ViewBuilder("child_1n_1n", TableFlag::WITH_DELETE)
                            .field("id", FieldFlag::AUTO_INC)
                            .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(), true);
    SCOPED_TRACE(root_all->as_graphql());

    EXPECT_DELETE(root_all, parse_pk(R"*({"id": 100})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", -2},
                       {"child_1n_1n", -1},
                       {"child_nm_join", -2},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_all, parse_pk(R"*({"id": 101})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", -2},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_all, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_all, parse_pk(R"*({"id": 103})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});
  }
  {
    reset();
    auto root_none =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", 0)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many("child1n1n",
                                   ViewBuilder("child_1n_1n", 0)
                                       .field("id", FieldFlag::AUTO_INC)
                                       .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", 0)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(), true);
    SCOPED_TRACE(root_none->as_graphql());

    EXPECT_DUALITY_ERROR(
        test_delete(root_none, parse_pk(R"*({"id": 100})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_none, parse_pk(R"*({"id": 101})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    // should succeed because there are no child refs
    reset();
    EXPECT_DELETE(root_none, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_none, parse_pk(R"*({"id": 103})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
  }
  {
    reset();
    auto root_1n =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", TableFlag::WITH_DELETE)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many("child1n1n",
                                   ViewBuilder("child_1n_1n", 0)
                                       .field("id", FieldFlag::AUTO_INC)
                                       .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", 0)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(), true);
    SCOPED_TRACE(root_1n->as_graphql());

    EXPECT_DUALITY_ERROR(
        test_delete(root_1n, parse_pk(R"*({"id": 100})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_1n, parse_pk(R"*({"id": 101})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_1n, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_1n, parse_pk(R"*({"id": 103})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
  }
  {
    reset();
    auto root_1n_1n =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", 0)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many(
                        "child1n1n",
                        ViewBuilder("child_1n_1n", TableFlag::WITH_DELETE)
                            .field("id", FieldFlag::AUTO_INC)
                            .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", 0)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(), true);
    SCOPED_TRACE(root_1n_1n->as_graphql());

    EXPECT_DUALITY_ERROR(
        test_delete(root_1n_1n, parse_pk(R"*({"id": 100})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_1n_1n, parse_pk(R"*({"id": 101})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_1n_1n, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_1n_1n, parse_pk(R"*({"id": 103})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
  }
  {
    reset();
    auto root_nm =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", 0)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many("child1n1n",
                                   ViewBuilder("child_1n_1n", 0)
                                       .field("id", FieldFlag::AUTO_INC)
                                       .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(), true);
    SCOPED_TRACE(root_nm->as_graphql());

    EXPECT_DUALITY_ERROR(
        test_delete(root_nm, parse_pk(R"*({"id": 100})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_nm, parse_pk(R"*({"id": 101})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_nm, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_nm, parse_pk(R"*({"id": 103})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});
  }
}

TEST_F(JsonMappingDelete, undeletable_child_pkfk) {
  // a reference that's also the PK (like in a n:m table) can't be UPDATE only
}

TEST_F(JsonMappingDelete, undeletable_child) {
  // update reference to NULL instead of deleting the row
  auto reset = [&]() {
    drop_schema();
    prepare(TestSchema::AUTO_INC);
    insert_rows();
    snapshot();
  };
#if 0
  {
    reset();
    auto root_all =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", TableFlag::WITH_UPDATE)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many(
                        "child1n1n",
                        ViewBuilder("child_1n_1n", TableFlag::WITH_UPDATE)
                            .field("id", FieldFlag::AUTO_INC)
                            .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(),true);
    SCOPED_TRACE(root_all->as_graphql());

    EXPECT_DELETE(root_all, parse_pk(R"*({"id": 100})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -2},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_all, parse_pk(R"*({"id": 101})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_all, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_all, parse_pk(R"*({"id": 103})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});
  }
  {
    reset();
    auto root_none =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", 0)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many("child1n1n",
                                   ViewBuilder("child_1n_1n", 0)
                                       .field("id", FieldFlag::AUTO_INC)
                                       .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(),true);
    SCOPED_TRACE(root_none->as_graphql());

    EXPECT_DUALITY_ERROR(
        test_delete(root_none, parse_pk(R"*({"id": 100})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_none, parse_pk(R"*({"id": 101})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    // should succeed because there are no child refs
    reset();
    EXPECT_DELETE(root_none, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_none, parse_pk(R"*({"id": 103})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});
  }
#endif
  {
    reset();
    auto root_1n =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", TableFlag::WITH_UPDATE)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many("child1n1n",
                                   ViewBuilder("child_1n_1n", 0)
                                       .field("id", FieldFlag::AUTO_INC)
                                       .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(), true);
    SCOPED_TRACE(root_1n->as_graphql());

    // child_1n succeeds, no cascade into child_1n_1n
    EXPECT_DELETE(root_1n, parse_pk(R"*({"id": 100})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -2},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_1n, parse_pk(R"*({"id": 101})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_1n, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_1n, parse_pk(R"*({"id": 103})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});
  }
  {
    reset();
    auto root_1n_1n =
        JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
            .field("id", FieldFlag::AUTO_INC)
            .field("data", "data1")
            .field_to_one(
                "child11",
                ViewBuilder("child_11", 0).field("id", FieldFlag::AUTO_INC))
            .field_to_many(
                "child1n",
                ViewBuilder("child_1n", 0)
                    .field("id", FieldFlag::AUTO_INC)
                    .field_to_many(
                        "child1n1n",
                        ViewBuilder("child_1n_1n", TableFlag::WITH_UPDATE)
                            .field("id", FieldFlag::AUTO_INC)
                            .field("data")))
            .field_to_many(
                "childnm",
                ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                    .field("root_id", 0)
                    .field("child_id", 0)
                    .field_to_one("child",
                                  ViewBuilder("child_nm", 0)
                                      .field("id", FieldFlag::AUTO_INC)))
            .resolve(m_.get(), true);
    SCOPED_TRACE(root_1n_1n->as_graphql());

    EXPECT_DUALITY_ERROR(
        test_delete(root_1n_1n, parse_pk(R"*({"id": 100})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DUALITY_ERROR(
        test_delete(root_1n_1n, parse_pk(R"*({"id": 101})*")),
        "Data Mapping View does not allow DELETE for a referenced table");
    expect_rows_added({{"root", 0},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_1n_1n, parse_pk(R"*({"id": 102})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    reset();
    EXPECT_DELETE(root_1n_1n, parse_pk(R"*({"id": 103})*"));
    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -1},
                       {"child_nm", 0}});
  }
}

TEST_F(JsonMappingDelete, filter_nodelete) {
  prepare(TestSchema::PLAIN);

  auto root = JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_NODELETE)
                  .field("id")
                  .field("data", "data1")
                  .resolve(m_.get(), true);

  SCOPED_TRACE(root->as_graphql());

  auto filter = R"*({
    "id": 103
  })*";
  EXPECT_DUALITY_ERROR(
      test_delete(root, filter),
      "Data Mapping View does not allow DELETE for table `root`");
}

TEST_F(JsonMappingDelete, filter_delete) {
  auto reset = [&]() {
    drop_schema();
    prepare(TestSchema::PLAIN);
    snapshot();
  };
  reset();

  auto root =
      JsonMappingBuilder("mrstestdb", "root", TableFlag::WITH_DELETE)
          .field("id", FieldFlag::AUTO_INC)
          .field("data", "data1")
          .field_to_one("child11", ViewBuilder("child_11", 0).field("id"))
          .field_to_many(
              "child1n",
              ViewBuilder("child_1n", TableFlag::WITH_UPDATE).field("id"))
          .field_to_many(
              "childnm",
              ViewBuilder("child_nm_join", TableFlag::WITH_DELETE)
                  .field("root_id", 0)
                  .field("child_id", 0)
                  .field_to_one("child",
                                ViewBuilder("child_nm", 0).field("id")))
          .resolve(m_.get(), true);
  SCOPED_TRACE(root->as_graphql());
  {
    // child_1n rows should have ref updated to NULL instead of deleted
    EXPECT_EQ(2,
              run_select_int(
                  "select count(*) from mrstestdb.child_1n where root_id=1"));

    auto filter = R"*({
    "id": 1
  })*";
    EXPECT_DELETE_F(root, filter);

    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", 0},
                       {"child_nm", 0}});

    EXPECT_EQ(0,
              run_select_int(
                  "select count(*) from mrstestdb.child_1n where root_id=1"));
  }
  {
    snapshot();
    auto filter = R"*({
    "id": 3
  })*";
    EXPECT_DELETE_F(root, filter);

    expect_rows_added({{"root", -1},
                       {"child_11", 0},
                       {"child_1n", 0},
                       {"child_1n_1n", 0},
                       {"child_nm_join", -2},
                       {"child_nm", 0}});
  }
}

TEST_F(JsonMappingDelete, cycle) {
  prepare(TestSchema::CYCLE);
  snapshot();

  auto root =
      JsonMappingBuilder("mrstestdb", "person", TableFlag::WITH_DELETE)
          .field("id")
          .field("name")
          .field_to_one("parent", ViewBuilder("person", TableFlag::WITH_UPDATE)
                                      .field("id")
                                      .field("name"))
          .field_to_many("children",
                         ViewBuilder("person", TableFlag::WITH_DELETE)
                             .field("id")
                             .field("name"))
          .resolve(m_.get(), true);

  // not referenced by any other rows
  {
    EXPECT_DELETE(root, parse_pk(R"*({"id": 4})*"));

    expect_rows_added({{"person", -1}});
  }
  // referenced by grandchild and references root
  {
    drop_schema();
    prepare(TestSchema::CYCLE);
    snapshot();

    EXPECT_DELETE(root, parse_pk(R"*({"id": 2})*"));
    expect_rows_added({{"person", -2}});
  }
  // referenced by children
  // this will fail because child of child is not marked with WITH DELETE
  // (or included in the view structure at all), but the FK constraint will
  // still block the delete of the child referenced by row 4
  {
    drop_schema();
    prepare(TestSchema::CYCLE);
    snapshot();

    EXPECT_MYSQL_ERROR(
        test_delete(root, parse_pk(R"*({"id": 1})*")),
        "Cannot delete or update a parent row: a foreign key constraint fails");
  }
}

TEST_F(JsonMappingDelete, cycle_undeletable) {
  prepare(TestSchema::CYCLE);
  snapshot();

  auto root =
      JsonMappingBuilder("mrstestdb", "person", TableFlag::WITH_DELETE)
          .field("id")
          .field("name")
          .field_to_one("parent", ViewBuilder("person", TableFlag::WITH_UPDATE)
                                      .field("id")
                                      .field("name"))
          .field_to_many("children",
                         ViewBuilder("person", TableFlag::WITH_UPDATE)
                             .field("id")
                             .field("name"))
          .resolve(m_.get(), true);

  // not referenced by any other rows
  {
    EXPECT_DELETE(root, parse_pk(R"*({"id": 4})*"));

    expect_rows_added({{"person", -1}});
  }
  // referenced by grandchild and references root
  {
    drop_schema();
    prepare(TestSchema::CYCLE);
    snapshot();

    EXPECT_DELETE(root, parse_pk(R"*({"id": 2})*"));
    expect_rows_added({{"person", -1}});
  }
  // referenced by childs
  {
    drop_schema();
    prepare(TestSchema::CYCLE);
    snapshot();

    EXPECT_DELETE(root, parse_pk(R"*({"id": 1})*"));

    expect_rows_added({{"person", -1}});
  }
}
