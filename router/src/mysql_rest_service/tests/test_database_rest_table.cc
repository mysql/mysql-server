/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include "mrs/database/query_rest_table.h"
#include "test_mrs_object_utils.h"

#include "mock/mock_session.h"

using mrs::database::QueryRestTable;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

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

TEST_F(QueryRestTableTests, DISABLED_basic_empty_request_throws) {
  auto object =
      ObjectBuilder("schema", "obj").field("c2", FieldFlag::PRIMARY).root();

  EXPECT_THROW(sut_->query_entries(
                   &mock_session_, object,
                   mrs::database::ObjectFieldFilter::from_object(*object), 0,
                   25, "my.url", true),
               std::invalid_argument);
}

TEST_F(QueryRestTableTests, DISABLED_basic_two_request_without_result) {
  auto object = ObjectBuilder("schema", "obj")
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

  sut_->query_entries(&mock_session_, object,
                      mrs::database::ObjectFieldFilter::from_object(*object), 0,
                      25, "my.url", true);
}

TEST_F(QueryRestTableTests,
       DISABLED_basic_two_request_without_result_and_no_links) {
  auto object = ObjectBuilder("schema", "obj").field("c1").field("c2").root();

  EXPECT_CALL(
      mock_session_,
      query(StrEq("SELECT JSON_SET(doc, '$._metadata', JSON_OBJECT('etag', "
                  "sha2(doc, 256)), '$.links', JSON_ARRAY()) doc FROM (SELECT "
                  "JSON_OBJECT('c1', `t`.`c1`, 'c2', `t`.`c2`) as doc FROM "
                  "`schema`.`obj` as `t`  LIMIT 0,26) tbl"),
            _, _));

  sut_->query_entries(&mock_session_, object,
                      mrs::database::ObjectFieldFilter::from_object(*object), 0,
                      25, "my.url", true);
}

TEST_F(QueryRestTableTests, DISABLED_basic_query) {
  auto root = ObjectBuilder("schema", "obj")
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
                      mrs::database::ObjectFieldFilter::from_object(*root), 0,
                      25, "my.url", true);
}
