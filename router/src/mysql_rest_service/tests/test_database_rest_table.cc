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
// TODO XXX obsolete
#if 0
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

#include "helper/make_shared_ptr.h"
#include "mrs/database/entry/entry.h"
#include "mrs/database/query_rest_table.h"
#include "mrs/database/query_rest_table_single_row.h"
#include "test_mrs_object_utils.h"

#include "mock/mock_json_template_factory.h"
#include "mock/mock_session.h"

using mrs::database::QueryRestTable;
using mrs::database::QueryRestTableSingleRow;
using mysqlrouter::MySQLSession;

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StartsWith;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;
using testing::WithParamInterface;

using namespace mrs::database::entry;

class QueryRestTableTests : public Test {
 public:
  StrictMock<MockMySQLSession> mock_session_;
  helper::MakeSharedPtr<QueryRestTable> sut_;
};

TEST(DbEntry, less) {
  std::map<EntryKey, uint64_t> m{
      {EntryKey{key_static, {1}}, 1}, {EntryKey{key_static, {2}}, 2},
      {EntryKey{key_static, {3}}, 3}, {EntryKey{key_static, {4}}, 4},
      {EntryKey{key_static, {5}}, 5}, {EntryKey{key_static, {6}}, 6},
      {EntryKey{key_rest, {1}}, 7},   {EntryKey{key_rest, {2}}, 8},
      {EntryKey{key_rest, {3}}, 9},   {EntryKey{key_rest, {4}}, 10},
      {EntryKey{key_rest, {5}}, 11},  {EntryKey{key_rest, {6}}, 12}};

  ASSERT_TRUE(m.count({key_static, {1}}));
  ASSERT_TRUE(m.count({key_static, {2}}));
  ASSERT_TRUE(m.count({key_static, {3}}));
  ASSERT_TRUE(m.count({key_static, {4}}));
  ASSERT_TRUE(m.count({key_static, {5}}));
  ASSERT_TRUE(m.count({key_static, {6}}));
  ASSERT_TRUE(m.count({key_rest, {1}}));
  ASSERT_TRUE(m.count({key_rest, {2}}));
  ASSERT_TRUE(m.count({key_rest, {3}}));
  ASSERT_TRUE(m.count({key_rest, {4}}));
  ASSERT_TRUE(m.count({key_rest, {5}}));
  ASSERT_TRUE(m.count({key_rest, {6}}));

  ASSERT_TRUE((m[{key_static, {1}}] == 1));
  ASSERT_TRUE((m[{key_static, {2}}] == 2));
  ASSERT_TRUE((m[{key_static, {3}}] == 3));
  ASSERT_TRUE((m[{key_static, {4}}] == 4));
  ASSERT_TRUE((m[{key_static, {5}}] == 5));
  ASSERT_TRUE((m[{key_static, {6}}] == 6));
  ASSERT_TRUE((m[{key_rest, {1}}] == 7));
  ASSERT_TRUE((m[{key_rest, {2}}] == 8));
  ASSERT_TRUE((m[{key_rest, {3}}] == 9));
  ASSERT_TRUE((m[{key_rest, {4}}] == 10));
  ASSERT_TRUE((m[{key_rest, {5}}] == 11));
  ASSERT_TRUE((m[{key_rest, {6}}] == 12));
}

class QueryVerification {
 public:
  QueryVerification() = default;

  QueryVerification(const QueryVerification &qv) {
    object = qv.object;
    query_starts_with = qv.query_starts_with;
    encode_bigints_as_string = qv.encode_bigints_as_string;
  }

  QueryVerification(const std::string &expected_query,
                    std::shared_ptr<Object> ob,
                    bool encode_bigint_as_st = false)
      : object{ob},
        query_starts_with{expected_query},
        encode_bigints_as_string{encode_bigint_as_st} {}

  std::shared_ptr<Object> object;
  std::string query_starts_with;
  bool encode_bigints_as_string{false};
};
class QueryRestTableParameterTests
    : public QueryRestTableTests,
      public WithParamInterface<QueryVerification> {
 public:
  InjectMockJsonTemplateFactory mock_factory_;
};

TEST_P(
    QueryRestTableParameterTests,
    single_row_verify_generated_sql_and_basic_flow_between_internal_objects) {
  const std::string k_url{"my.url"};
  const char *k_query_generated_value{"forward-this-value"};
  char k_some_string[] = "Some String";

  QueryRestTableSingleRow sut(GetParam().encode_bigints_as_string);
  auto object = GetParam().object;

  EXPECT_CALL(mock_session_,
              query(StartsWith(GetParam().query_starts_with), _, _))
      .WillOnce(Invoke([&k_query_generated_value, &k_some_string](
                           auto &, auto &on_row_processor, auto &on_metadata) {
        MYSQL_FIELD field;
        memset(&field, 0, sizeof(field));
        field.name = k_some_string;
        field.type = MYSQL_TYPE_JSON;
        on_metadata(1, &field);
        on_row_processor(MySQLSession::ResultRow({k_query_generated_value}));
      }));
  sut.query_entries(&mock_session_, object,
                    mrs::database::dv::ObjectFieldFilter::from_object(*object),
                    {}, k_url);

  // Verify that the resulting JSON is forwarded from JsonTemplate to
  // sut_->response.
  EXPECT_EQ(k_query_generated_value, sut.response);
}

TEST_P(QueryRestTableParameterTests,
       verify_generated_sql_and_basic_flow_between_internal_objects) {
  const std::string k_result{"some-result"};
  const std::string k_url{"my.url"};
  const char *k_query_generated_value{"forward-this-value"};
  const int k_offset = 100;
  const int k_page_size = 22;
  char k_some_string[] = "Some String";

  QueryRestTable sut(&mock_factory_, GetParam().encode_bigints_as_string);
  auto object = GetParam().object;

  {
    testing::InSequence seq;

    EXPECT_CALL(mock_factory_.mock_, begin()).RetiresOnSaturation();
    EXPECT_CALL(mock_factory_.mock_,
                begin_resultset(k_offset, k_page_size, true, k_url, _))
        .RetiresOnSaturation();
    EXPECT_CALL(mock_factory_.mock_,
                push_json_document(StrEq(k_query_generated_value)))
        .RetiresOnSaturation();
    EXPECT_CALL(mock_factory_.mock_, finish()).RetiresOnSaturation();
    EXPECT_CALL(mock_factory_.mock_, get_result())
        .WillOnce(Return(k_result))
        .RetiresOnSaturation();
  }

  EXPECT_CALL(mock_session_,
              query(StartsWith(GetParam().query_starts_with), _, _))
      .WillOnce(Invoke([&k_query_generated_value, &k_some_string](
                           auto &, auto &on_row_processor, auto &on_metadata) {
        MYSQL_FIELD field;
        memset(&field, 0, sizeof(field));
        field.name = k_some_string;
        field.type = MYSQL_TYPE_JSON;
        on_metadata(1, &field);
        on_row_processor(MySQLSession::ResultRow({k_query_generated_value}));
      }));
  sut.query_entries(&mock_session_, object,
                    mrs::database::dv::ObjectFieldFilter::from_object(*object),
                    k_offset, k_page_size, k_url, true);

  // Verify that the resulting JSON is forwarded from JsonTemplate to
  // sut_->response.
  EXPECT_EQ(k_result, sut.response);
}

using Ob = DualityViewBuilder;

INSTANTIATE_TEST_SUITE_P(
    InstantiateQueryExpectations, QueryRestTableParameterTests,
    testing::Values(
        QueryVerification(
            "SELECT JSON_OBJECT('c1', `t`.`db_column_name_c1`, 'c2', "
            "`t`.`db_column_name_c2`,'links',",
            Ob("schema", "obj")
                .field("c1", "db_column_name_c1", "TEXT")
                .field("c2", "db_column_name_c2", "INT")
                .resolve()),

        QueryVerification(
            "SELECT JSON_OBJECT('c2', `t`.`db_column_name_c2`,'links',",
            Ob("schema", "obj")
                .field("c2", "db_column_name_c2", "INT", FieldFlag::PRIMARY)),

        QueryVerification(
            "SELECT JSON_OBJECT('c2', `t`.`db_column_name_c2`,'links',",
            Ob("schema", "obj")
                .field("c2", "db_column_name_c2", "INT", FieldFlag::PRIMARY),
            true),

        QueryVerification(
            "SELECT JSON_OBJECT('c2', `t`.`db_column_name_c2`,'links',",
            Ob("schema", "obj")
                .field("c2", "db_column_name_c2", "BIGINT",
                       FieldFlag::PRIMARY)),

        // Only case when we wrap the c2 column inside `CONVERT` call(convert to
        // string).
        QueryVerification("SELECT JSON_OBJECT('c2', "
                          "CONVERT(`t`.`db_column_name_c2`, CHAR),'links',",
                          Ob("schema", "obj")
                              .field("c2", "db_column_name_c2", "BIGINT",
                                     FieldFlag::PRIMARY),
                          true)));

TEST_F(QueryRestTableTests, DISABLED_basic_empty_request_throws) {
  auto object =
      DualityViewBuilder("schema", "obj").field("c2", FieldFlag::PRIMARY).root();

  EXPECT_THROW(sut_->query_entries(
                   &mock_session_, object,
                   mrs::database::dv::ObjectFieldFilter::from_object(*object),
                   0, 25, "my.url", true),
               std::invalid_argument);
}

TEST_F(QueryRestTableTests, DISABLED_basic_two_request_without_result) {
  auto object = DualityViewBuilder("schema", "obj")
                    .field("c1")
                    .field("c2", FieldFlag::PRIMARY)
                    .root();

  EXPECT_CALL(
      mock_session_,
      query(
          StrEq("SELECT JSON_SET(doc, '$._metadata', JSON_OBJECT('etag', "
                "sha2(doc, 256)), '$.links', "
                "JSON_ARRAY(JSON_OBJECT('rel','self','href',CONCAT('my.url','/"
                "',`c2`)))) doc FROM (SELECT JSON_OBJECT('c1', `t`.`c1`, 'c2', "
                "`t`.`c2`) as doc FROM `schema`.`obj` as `t`  LIMIT 0,26) tbl"),
          _, _));

  sut_->query_entries(
      &mock_session_, object,
      mrs::database::dv::ObjectFieldFilter::from_object(*object), 0, 25,
      "my.url", true);
}

TEST_F(QueryRestTableTests,
       DISABLED_basic_two_request_without_result_and_no_links) {
  auto object = DualityViewBuilder("schema", "obj").field("c1").field("c2").root();

  EXPECT_CALL(
      mock_session_,
      query(StrEq("SELECT JSON_SET(doc, '$._metadata', JSON_OBJECT('etag', "
                  "sha2(doc, 256)), '$.links', JSON_ARRAY()) doc FROM (SELECT "
                  "JSON_OBJECT('c1', `t`.`c1`, 'c2', `t`.`c2`) as doc FROM "
                  "`schema`.`obj` as `t`  LIMIT 0,26) tbl"),
            _, _));

  sut_->query_entries(
      &mock_session_, object,
      mrs::database::dv::ObjectFieldFilter::from_object(*object), 0, 25,
      "my.url", true);
}

TEST_F(QueryRestTableTests, DISABLED_basic_query) {
  auto root = DualityViewBuilder("schema", "obj")
                  .field("c1", FieldFlag::PRIMARY)
                  .field("c2")
                  .root();

  EXPECT_CALL(
      mock_session_,
      query(
          StrEq("SELECT JSON_SET(doc, '$._metadata', JSON_OBJECT('etag', "
                "sha2(doc, 256)), '$.links', "
                "JSON_ARRAY(JSON_OBJECT('rel','self','href',CONCAT('my.url','/"
                "',`c1`)))) doc FROM (SELECT JSON_OBJECT('c1', `t`.`c1`, 'c2', "
                "`t`.`c2`) as doc FROM `schema`.`obj` as `t`  LIMIT 0,26) tbl"),
          _, _));

  sut_->query_entries(&mock_session_, root,
                      mrs::database::dv::ObjectFieldFilter::from_object(*root),
                      0, 25, "my.url", true);
}
#endif