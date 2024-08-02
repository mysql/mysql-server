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

#include "helper/make_shared_ptr.h"
#include "mrs/database/entry/entry.h"
#include "mrs/database/query_rest_sp.h"
#include "test_mrs_object_utils.h"

#include "mock/mock_json_template_factory.h"
#include "mock/mock_session.h"

using mrs::database::QueryRestSP;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

using namespace mrs::database::entry;

class QueryRestSpUnderTest : public QueryRestSP {
 public:
  using QueryRestSP::QueryRestSP;
  using ResultRow = mysqlrouter::MySQLSession::ResultRow;

  // Make those methods public, to be able to call the directly.
  void on_row(const ResultRow &r) override { QueryRestSP::on_row(r); }
  void on_metadata(unsigned int number, MYSQL_FIELD *fields) override {
    QueryRestSP::on_metadata(number, fields);
  }
};

class QueryRestSpTests : public Test {
 public:
  void SetUp() override {
    EXPECT_CALL(mock_session_, prepare("CALL `sch`.`obj`()"))
        .WillOnce(Return(1));
    EXPECT_CALL(mock_session_, prepare_remove(1));
  }

  static MYSQL_FIELD create_field(const char *name, enum_field_types t) {
    MYSQL_FIELD f;
    memset(&f, 0, sizeof(f));
    f.name = const_cast<char *>(name);
    f.name_length = strlen(name);
    f.type = t;
    return f;
  }

  const std::string kSchema{"sch"};
  const std::string kObject{"obj"};
  const std::string kUrl{"host/srv/sch/obj"};
  StrictMock<MockMySQLSession> mock_session_;
  InjectMockJsonTemplateFactory json_template_;
  helper::MakeSharedPtr<QueryRestSpUnderTest> sut_{&json_template_};
};

MATCHER_P2(MatchFields, fields, s, "") {
  *result_listener << "where size of " << arg.size() << " is equal to " << s;
  if (arg.size() != s) return false;

  for (std::size_t i = 0; i < s; ++i) {
    *result_listener << "\nwhere element " << i << " name:\"" << arg[i].name
                     << "\" is equal to \"" << fields[i].name << "\"";
    if (arg[i].name != fields[i].name) return false;
  }
  return true;
}

TEST_F(QueryRestSpTests, procedure_returns_nothing) {
  mrs::database::entry::ResultSets rs;

  EXPECT_CALL(json_template_.mock_nested_, begin());
  EXPECT_CALL(json_template_.mock_nested_, finish());
  EXPECT_CALL(json_template_.mock_nested_, get_result()).WillOnce(Return(""));

  EXPECT_CALL(mock_session_, prepare_execute(1, _, _, _)).WillOnce(Invoke([]() {
  }));
  sut_->query_entries(&mock_session_, kSchema, kObject, kUrl, {}, {}, {}, rs);
}

TEST_F(QueryRestSpTests, procedure_has_one_empty_resultset_unknow_fields) {
  mrs::database::entry::ResultSets rs;
  const std::string kUnknowResultset0 = "items0";
  MYSQL_FIELD fields[2] = {create_field("f1", MYSQL_TYPE_LONG),
                           create_field("f2", MYSQL_TYPE_VARCHAR)};

  EXPECT_CALL(json_template_.mock_nested_, begin());
  EXPECT_CALL(json_template_.mock_nested_,
              begin_resultset(kUrl, kUnknowResultset0,
                              MatchFields(fields, std::size(fields))));
  EXPECT_CALL(json_template_.mock_nested_, finish());
  EXPECT_CALL(json_template_.mock_nested_, get_result()).WillOnce(Return(""));

  EXPECT_CALL(mock_session_, prepare_execute(1, _, _, _))
      .WillOnce(Invoke(
          [this, &fields]() { sut_->on_metadata(std::size(fields), fields); }));
  sut_->query_entries(&mock_session_, kSchema, kObject, kUrl, {}, {}, {}, rs);
}

TEST_F(QueryRestSpTests,
       procedure_has_one_empty_resultset_fields_in_the_same_order) {
  const char *k_resultset_name = "firstRS";
  mrs::database::entry::ResultSets rs{
      {},
      {{{{{}, "a1", {}, "f1", {}, {}}, {{}, "a2", {}, "f2", {}, {}}},
        k_resultset_name,
        {}}}};
  const std::string kUnknowResultset0 = "items0";
  MYSQL_FIELD fields[2] = {create_field("f1", MYSQL_TYPE_LONG),
                           create_field("f2", MYSQL_TYPE_VARCHAR)};
  MYSQL_FIELD fields_reported_to_serializer[2] = {
      create_field("a1", MYSQL_TYPE_LONG),
      create_field("a2", MYSQL_TYPE_VARCHAR)};

  EXPECT_CALL(json_template_.mock_nested_, begin());
  EXPECT_CALL(
      json_template_.mock_nested_,
      begin_resultset(kUrl, k_resultset_name,
                      MatchFields(fields_reported_to_serializer,
                                  std::size(fields_reported_to_serializer))));
  EXPECT_CALL(json_template_.mock_nested_, finish());
  EXPECT_CALL(json_template_.mock_nested_, get_result()).WillOnce(Return(""));

  EXPECT_CALL(mock_session_, prepare_execute(1, _, _, _))
      .WillOnce(Invoke(
          [this, &fields]() { sut_->on_metadata(std::size(fields), fields); }));
  sut_->query_entries(&mock_session_, kSchema, kObject, kUrl, {}, {}, {}, rs);
}

TEST_F(QueryRestSpTests,
       procedure_has_one_empty_resultset_fields_in_the_mixed_order) {
  const char *k_resultset_name = "firstRS";
  mrs::database::entry::ResultSets rs{
      {},
      {{{{{}, "a2", {}, "f2", {}, {}}, {{}, "a1", {}, "f1", {}, {}}},
        k_resultset_name,
        {}}}};
  const std::string kUnknowResultset0 = "items0";
  MYSQL_FIELD fields[2] = {create_field("f1", MYSQL_TYPE_LONG),
                           create_field("f2", MYSQL_TYPE_VARCHAR)};
  MYSQL_FIELD fields_reported_to_serializer[2] = {
      create_field("a1", MYSQL_TYPE_LONG),
      create_field("a2", MYSQL_TYPE_VARCHAR)};

  EXPECT_CALL(json_template_.mock_nested_, begin());
  EXPECT_CALL(
      json_template_.mock_nested_,
      begin_resultset(kUrl, k_resultset_name,
                      MatchFields(fields_reported_to_serializer,
                                  std::size(fields_reported_to_serializer))));
  EXPECT_CALL(json_template_.mock_nested_, finish());
  EXPECT_CALL(json_template_.mock_nested_, get_result()).WillOnce(Return(""));

  EXPECT_CALL(mock_session_, prepare_execute(1, _, _, _))
      .WillOnce(Invoke(
          [this, &fields]() { sut_->on_metadata(std::size(fields), fields); }));
  sut_->query_entries(&mock_session_, kSchema, kObject, kUrl, {}, {}, {}, rs);
}
