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
#include "mrs/database/query_rest_table_updater.h"
#include "rapidjson/pointer.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;
using namespace mrs::database::dv;

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;
using namespace helper::json::sql;

// TODO - allow true/false for int (and convert to 1/0)
// TODO - allow omitting columns, which should insert as DEFAULT

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

class DualityViewUpdate : public DatabaseRestTableTest {
 public:
  void update_e(std::shared_ptr<DualityView> view,
                const PrimaryKeyColumnValues &pk, const std::string &input,
                const ObjectRowOwnership &row_owner = {}) {
    try {
      update(view, pk, input, row_owner);
    } catch (const JSONInputError &e) {
      ADD_FAILURE() << "UPDATE threw JSONInputError: " << e.what();
      throw;
    } catch (const DualityViewError &e) {
      ADD_FAILURE() << "UPDATE threw DualityViewError: " << e.what();
      throw;
    } catch (const MySQLError &e) {
      ADD_FAILURE() << "UPDATE threw MySQLError: " << e.what();
      throw;
    } catch (const std::runtime_error &e) {
      ADD_FAILURE() << "UPDATE threw runtime_error: " << e.what();
      throw;
    }
  }

  void update(std::shared_ptr<DualityView> view,
              const PrimaryKeyColumnValues &pk, const std::string &input,
              const ObjectRowOwnership &row_owner = {}) {
    DualityViewUpdater dvu(view, row_owner);
    dvu.update(m_.get(), pk, make_json(input));
  }

  std::string to_base64(const std::string &s) {
    auto row = m_->query_one("SELECT TO_BASE64(" + s + ")");
    return (*row)[0];
  }

  void test_update(std::shared_ptr<DualityView> view, const std::string &templ,
                   const PrimaryKeyColumnValues &pk, std::vector<int> &ids,
                   const ObjectRowOwnership &row_owner = {}) {
    std::string input, expected_output;
    process_template(templ, ids, &input, &expected_output);
    update(view, pk, input, row_owner);
  }

  void expect_update(std::shared_ptr<DualityView> view,
                     const std::string &templ, const PrimaryKeyColumnValues &pk,
                     std::vector<int> &ids) {
    std::string input, expected_output;
    process_template(templ, ids, &input, &expected_output);
    SCOPED_TRACE(input);

    update_e(view, pk, input, {});

    auto output = select_one(view, pk, {}, {}, false);
    EXPECT_EQ(pprint_json(expected_output), pprint_json(output));
  }
};

#define EXPECT_UPDATE(f, input, pk, ids) \
  do {                                   \
    SCOPED_TRACE("");                    \
    expect_update(f, input, pk, ids);    \
  } while (0)

TEST_F(DualityViewUpdate, invalid_json) {
  auto root = DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_UPDATE)
                  .field("film_id", FieldFlag::AUTO_INC)
                  .field("title")
                  .field("release_year")
                  .resolve(m_.get());
  std::vector<int> ids;

  EXPECT_JSON_ERROR(test_update(root, "123", parse_pk("{\"film_id\": 8}"), ids),
                    "Invalid document in JSON input for table `film`");
  EXPECT_JSON_ERROR(
      test_update(root, "null", parse_pk("{\"film_id\": 8}"), ids),
      "Invalid document in JSON input for table `film`");
  EXPECT_JSON_ERROR(test_update(root, "[]", parse_pk("{\"film_id\": 8}"), ids),
                    "Invalid document in JSON input for table `film`");
}

TEST_F(DualityViewUpdate, root_update) {
  auto root = DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_UPDATE)
                  .field("id", "country_id", FieldFlag::AUTO_INC)
                  .field("country")
                  .resolve(m_.get());

  SCOPED_TRACE(root->as_graphql());

  std::vector<int> ids;

  // no changes
  EXPECT_UPDATE(root, R"*({
    "id": 8,
    "country": "Australia"
  })*",
                parse_pk("{\"country_id\": 8}"), ids);

  // pk changed
  EXPECT_JSON_ERROR(test_update(root, R"*({
    "id": 10,
    "country": "Australia"
  })*",
                                parse_pk("{\"country_id\":8}"), ids),
                    "ID for table `country` cannot be changed");

  // pk omitted
  EXPECT_JSON_ERROR(test_update(root, R"*({
    "country": "Australia"
})*",
                                parse_pk("{\"country_id\":8}"), ids),
                    "ID for table `country` missing in JSON input");

  // value changed
  EXPECT_UPDATE(root, R"*({
    "id": 8,
    "country": "AUSTRALIA"
})*",
                parse_pk("{\"country_id\":8}"), ids);
}

TEST_F(DualityViewUpdate, root_noupdate) {
  auto root = DualityViewBuilder("mrstestdb", "country", TableFlag::WITH_INSERT)
                  .field("id", "country_id", FieldFlag::AUTO_INC)
                  .field("country")
                  .resolve(m_.get());

  auto root_field =
      DualityViewBuilder("mrstestdb", "country",
                         TableFlag::WITH_UPDATE | TableFlag::WITH_NOCHECK)
          .field("id", "country_id", FieldFlag::AUTO_INC)
          .field("country", FieldFlag::WITH_NOUPDATE)
          .resolve(m_.get());

  SCOPED_TRACE(root->as_graphql());

  std::vector<int> ids;

  // no changes
  test_update(root, R"*({
    "id": 8,
    "country": "Australia"
  })*",
              parse_pk("{\"country_id\": 8}"), ids);

  test_update(root_field, R"*({
    "id": 8,
    "country": "Australia"
  })*",
              parse_pk("{\"country_id\": 8}"), ids);

  // pk changed
  EXPECT_JSON_ERROR(test_update(root, R"*({
    "id": 10,
    "country": "Australia"
  })*",
                                parse_pk("{\"country_id\":8}"), ids),
                    "ID for table `country` cannot be changed");
  EXPECT_JSON_ERROR(test_update(root_field, R"*({
    "id": 10,
    "country": "Australia"
  })*",
                                parse_pk("{\"country_id\":8}"), ids),
                    "ID for table `country` cannot be changed");

  // pk omitted
  EXPECT_JSON_ERROR(test_update(root, R"*({
    "country": "Australia"
})*",
                                parse_pk("{\"country_id\":8}"), ids),
                    "ID for table `country` cannot be changed");

  // value changed
  EXPECT_DUALITY_ERROR(
      test_update(root, R"*({
    "id": 8,
    "country": "AUSTRALIA"
})*",
                  parse_pk("{\"country_id\":8}"), ids),
      "Duality View does not allow UPDATE for table `country`");

  EXPECT_DUALITY_ERROR(test_update(root_field, R"*({
    "id": 8,
    "country": "AUSTRALIA"
})*",
                                   parse_pk("{\"country_id\":8}"), ids),
                       "Duality View does not allow UPDATE for field "
                       "\"country\" of table `country`");

  // noupdate field omitted (omit = no changes)
  test_update(root_field, R"*({
    "id": 8
})*",
              parse_pk("{\"country_id\":8}"), ids);
}

TEST_F(DualityViewUpdate, child11_parent_noupdate) {
  auto root = DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_NOUPDATE)
                  .field("film_id", FieldFlag::AUTO_INC)
                  .field("title")
                  .field("release_year")
                  .field_to_one("language",
                                ViewBuilder("language", TableFlag::WITH_UPDATE)
                                    .field("language_id", FieldFlag::AUTO_INC)
                                    .field("name"),
                                false, {{"language_id", "language_id"}})
                  .resolve(m_.get());

  auto test_empty = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {},
  "release_year": 2006
})*";
  auto test_duppk_nochanges = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "French",
    "language_id": 5
  },
  "release_year": 2006
})*";
  auto test_duppk_changes = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "FRENCH",
    "language_id": 5
  },
  "release_year": 2006
})*";
  auto test_newpk = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "Swahili",
    "language_id": 100
  },
  "release_year": 2006
})*";
  auto test_nochanges = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "English",
    "language_id": 1
  },
  "release_year": 2006
})*";
  auto test_changes_in_nested = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "ENGLISH",
    "language_id": 1
  },
  "release_year": 2006
})*";

  // original:
  // {"title":"AGENT
  // TRUMAN","film_id":6,"language":{"name":"English","language_id":1},"release_year":2006}

  std::vector<int> ids;
  // language_id is a FK in the root, which is not updatable, despite language
  // being updatable
  EXPECT_DUALITY_ERROR(
      test_update(root, test_empty, parse_pk("{\"film_id\":6}"), ids),
      "Duality View does not allow UPDATE for table `film`");
  EXPECT_DUALITY_ERROR(
      test_update(root, test_duppk_nochanges, parse_pk("{\"film_id\":6}"), ids),
      "Duality View does not allow UPDATE for table `film`");
  EXPECT_DUALITY_ERROR(
      test_update(root, test_duppk_changes, parse_pk("{\"film_id\":6}"), ids),
      "Duality View does not allow UPDATE for table `film`");
  EXPECT_DUALITY_ERROR(
      test_update(root, test_newpk, parse_pk("{\"film_id\":6}"), ids),
      "Duality View does not allow UPDATE for table `film`");
  EXPECT_UPDATE(root, test_nochanges, parse_pk("{\"film_id\":6}"), ids);
  EXPECT_UPDATE(root, test_changes_in_nested, parse_pk("{\"film_id\":6}"), ids);
}

TEST_F(DualityViewUpdate, child11) {
  auto root_noup =
      DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_UPDATE)
          .field("film_id", FieldFlag::AUTO_INC)
          .field("title")
          .field("release_year")
          .field_to_one("language",
                        ViewBuilder("language", TableFlag::WITH_NOUPDATE)
                            .field("language_id", FieldFlag::AUTO_INC)
                            .field("name"),
                        false, {{"language_id", "language_id"}})
          .resolve(m_.get());

  auto root_up =
      DualityViewBuilder("mrstestdb", "film", TableFlag::WITH_UPDATE)
          .field("film_id", FieldFlag::AUTO_INC)
          .field("title")
          .field("release_year")
          .field_to_one("language",
                        ViewBuilder("language", TableFlag::WITH_UPDATE)
                            .field("language_id", FieldFlag::AUTO_INC)
                            .field("name"),
                        false, {{"language_id", "language_id"}})
          .resolve(m_.get());

  [[maybe_unused]] auto test_empty = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {},
  "release_year": 2006
})*";
  [[maybe_unused]] auto test_duppk_nochanges = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "French",
    "language_id": 5
  },
  "release_year": 2006
})*";
  [[maybe_unused]] auto test_duppk_changes = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "FRENCH",
    "language_id": 5
  },
  "release_year": 2006
})*";
  [[maybe_unused]] auto test_newpk = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "Swahili",
    "language_id": 100
  },
  "release_year": 2006
})*";
  [[maybe_unused]] auto test_nochanges = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "English",
    "language_id": 1
  },
  "release_year": 2006
})*";
  [[maybe_unused]] auto test_changes = R"*({
  "title": "AGENT TRUMAN",
  "film_id": 6,
  "language": {
    "name": "ENGLISH",
    "language_id": 1
  },
  "release_year": 2006
})*";
}

std::shared_ptr<DualityView> make_root_1n(mysqlrouter::MySQLSession *session,
                                          int flags,
                                          bool child_autoinc = false) {
  return DualityViewBuilder("mrstestdb", "root", TableFlag::WITH_UPDATE)
      .field("id", FieldFlag::AUTO_INC)
      .field("data", "data1")
      .field_to_many("children",
                     ViewBuilder("child_1n", flags)
                         .field("id", child_autoinc ? FieldFlag::AUTO_INC : 0)
                         .field("data"))
      .resolve(session);
}

constexpr const auto test_1n_add_nopk = R"*({
    "id": 10,
    "data" : "data1",
    "children": [
      {
        "id": 10,
        "data": "test child1"
      },
      {
        "id": 11,
        "data": "test child2"
      },
      {
        "data": "New Test"
      }
    ]
})*";

constexpr const auto test_1n_add_nopk_autoinc = R"*({
    "id": 10,
    "data" : "data1",
    "children": [
      {
        "id": 10,
        "data": "test child1"
      },
      {
        "id": 11,
        "data": "test child2"
      },
      {
        <<o:"id":12,>>
        "data": "New Test"
      }
    ]
})*";

constexpr const auto test_1n_add_duppk = R"*({
    "id": 10,
    "data" : "data1",
    "children": [
      {
        "id": 1
        <<o:,"data": "ref1n-1">>
      },
      {
        "id": 10,
        "data": "test child1"
      },
      {
        "id": 11,
        "data": "test child2"
      }
    ]
})*";

constexpr const auto test_1n_add_newpk = R"*({
    "id": 10,
    "data" : "data1",
    "children": [
      {
        "id": 10,
        "data": "test child1"
      },
      {
        "id": 11,
        "data": "test child2"
      },
      {
        "id": 100,
        "data": "data1"
      }
    ]
})*";

constexpr const auto test_1n_del = R"*({
    "id": 10,
    "data" : "data1",
    "children": [
      {
        "id": 10,
        "data": "test child1"
      }
    ]
})*";

constexpr const auto test_1n_upd = R"*({
    "id": 10,
    "data" : "data1",
    "children": [
      {
        "id": 10,
        "data": "TEST 1"
      },
      {
        "id": 11,
        "data": "TEST 2"
      }
    ]
})*";

constexpr const auto test_1n_upd_nop = R"*({
    "id": 10,
    "data" : "data1",
    "children": [
      {
        "id": 10,
        "data": "test child1"
      },
      {
        "id": 11,
        "data": "test child2"
      }
    ]
})*";

[[maybe_unused]] constexpr const auto test_1n_mix1 = "";

[[maybe_unused]] constexpr const auto test_1n_mix2 = "";

// TODO test for WITH UPDATE on indicidual columns

TEST_F(DualityViewUpdate, child1n_none) {
  auto reset = [this]() {
    drop_schema();
    prepare(TestSchema::PLAIN);
  };

  reset();
  auto root = make_root_1n(m_.get(), TableFlag::WITH_NOCHECK);

  std::vector<int> ids{};

  EXPECT_JSON_ERROR(
      test_update(root, test_1n_add_nopk, parse_pk("{\"id\":10}"), ids),
      "ID for table `child_1n` missing in JSON input");
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow INSERT for table `child_1n`");
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow INSERT for table `child_1n`");
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root, test_1n_del, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow DELETE for table `child_1n`");
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root, test_1n_upd, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow UPDATE for table `child_1n`");
  reset();
  EXPECT_UPDATE(root, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
}

TEST_F(DualityViewUpdate, child1n_all) {
  auto reset = [this]() {
    drop_schema();
    prepare(TestSchema::PLAIN);
  };

  reset();
  auto root = make_root_1n(
      m_.get(), TableFlag::WITH_INSERT | TableFlag::WITH_UPDATE |
                    TableFlag::WITH_DELETE | TableFlag::WITH_NOCHECK);

  std::vector<int> ids;

  EXPECT_MYSQL_ERROR(
      test_update(root, test_1n_add_nopk, parse_pk("{\"id\":10}"), ids),
      "Field 'id' doesn't have a default value");
  reset();
  EXPECT_UPDATE(root, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root, test_1n_del, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root, test_1n_upd, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
}

TEST_F(DualityViewUpdate, child1n) {
  auto reset = [this]() {
    drop_schema();
    prepare(TestSchema::PLAIN);
  };

  reset();

  auto root_i =
      make_root_1n(m_.get(), TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK);
  auto root_d =
      make_root_1n(m_.get(), TableFlag::WITH_DELETE | TableFlag::WITH_NOCHECK);
  auto root_u =
      make_root_1n(m_.get(), TableFlag::WITH_UPDATE | TableFlag::WITH_NOCHECK);

  std::vector<int> ids;
  EXPECT_MYSQL_ERROR(
      test_update(root_i, test_1n_add_nopk, parse_pk("{\"id\":10}"), ids),
      "Field 'id' doesn't have a default value");
  reset();
  EXPECT_MYSQL_ERROR(
      test_update(root_i, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids),
      "Duplicate entry '1' for key");
  reset();
  EXPECT_UPDATE(root_i, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_i, test_1n_del, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow DELETE for table `child_1n`");
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_i, test_1n_upd, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow UPDATE for table `child_1n`");
  reset();
  EXPECT_UPDATE(root_i, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
  reset();

  EXPECT_JSON_ERROR(
      test_update(root_d, test_1n_add_nopk, parse_pk("{\"id\":10}"), ids),
      "ID for table `child_1n` missing in JSON input");
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_d, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow INSERT for table `child_1n`");
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_d, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow INSERT for table `child_1n`");
  reset();
  EXPECT_UPDATE(root_d, test_1n_del, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_d, test_1n_upd, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow UPDATE for table `child_1n`");
  reset();
  EXPECT_UPDATE(root_d, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
  reset();

  EXPECT_JSON_ERROR(
      test_update(root_u, test_1n_add_nopk, parse_pk("{\"id\":10}"), ids),
      "ID for table `child_1n` missing in JSON input");
  reset();
  EXPECT_UPDATE(root_u, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids);
  // XXX check that 1 was stolen
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_u, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow INSERT for table `child_1n`");
  reset();
  EXPECT_UPDATE(root_u, test_1n_del, parse_pk("{\"id\":10}"), ids);
  // XXX check abandoned
  reset();
  EXPECT_UPDATE(root_u, test_1n_upd, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root_u, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
  reset();
}

TEST_F(DualityViewUpdate, child1n_noupdate) {
  auto reset = [this]() {
    drop_schema();
    prepare(TestSchema::PLAIN);
  };

  reset();

  auto root_di =
      make_root_1n(m_.get(), TableFlag::WITH_INSERT | TableFlag::WITH_DELETE |
                                 TableFlag::WITH_NOCHECK);

  std::vector<int> ids;

  EXPECT_MYSQL_ERROR(
      test_update(root_di, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids),
      "Duplicate entry '1' for key");
  reset();
  EXPECT_UPDATE(root_di, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root_di, test_1n_del, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_di, test_1n_upd, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow UPDATE for table `child_1n`");
  reset();
  EXPECT_UPDATE(root_di, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
}

TEST_F(DualityViewUpdate, child1n_noinsert) {
  auto reset = [this]() {
    drop_schema();
    prepare(TestSchema::PLAIN);
  };

  reset();

  auto root_du =
      make_root_1n(m_.get(), TableFlag::WITH_UPDATE | TableFlag::WITH_DELETE |
                                 TableFlag::WITH_NOCHECK);

  std::vector<int> ids;
  // steals
  EXPECT_UPDATE(root_du, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_du, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow INSERT for table `child_1n`");
  reset();
  EXPECT_UPDATE(root_du, test_1n_del, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root_du, test_1n_upd, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root_du, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
}

TEST_F(DualityViewUpdate, child1n_nodelete) {
  auto reset = [this]() {
    drop_schema();
    prepare(TestSchema::PLAIN);
  };

  reset();

  auto root_ui =
      make_root_1n(m_.get(), TableFlag::WITH_INSERT | TableFlag::WITH_UPDATE |
                                 TableFlag::WITH_NOCHECK);

  std::vector<int> ids;
  EXPECT_UPDATE(root_ui, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root_ui, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids);
  reset();
  // abandons
  EXPECT_UPDATE(root_ui, test_1n_del, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root_ui, test_1n_upd, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_UPDATE(root_ui, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
}

TEST_F(DualityViewUpdate, child1n_autoinc) {
  auto reset = [this]() {
    drop_schema();
    prepare(TestSchema::AUTO_INC);
  };

  reset();

  auto root_i = make_root_1n(
      m_.get(), TableFlag::WITH_INSERT | TableFlag::WITH_NOCHECK, true);

  std::vector<int> ids;
  EXPECT_UPDATE(root_i, test_1n_add_nopk_autoinc, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_MYSQL_ERROR(
      test_update(root_i, test_1n_add_duppk, parse_pk("{\"id\":10}"), ids),
      "Duplicate entry '1' for key");
  reset();
  EXPECT_UPDATE(root_i, test_1n_add_newpk, parse_pk("{\"id\":10}"), ids);
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_i, test_1n_del, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow DELETE for table `child_1n`");
  reset();
  EXPECT_DUALITY_ERROR(
      test_update(root_i, test_1n_upd, parse_pk("{\"id\":10}"), ids),
      "Duality View does not allow UPDATE for table `child_1n`");
  reset();
  EXPECT_UPDATE(root_i, test_1n_upd_nop, parse_pk("{\"id\":10}"), ids);
  reset();
}

TEST_F(DualityViewUpdate, deep_nested) {}

TEST_F(DualityViewUpdate, deep_nested_delete) {}

TEST_F(DualityViewUpdate, cycle) {}
