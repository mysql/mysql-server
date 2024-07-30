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

#include "helper/make_shared_ptr.h"

#include "mrs/database/entry/db_object.h"
#include "mrs/database/query_entries_db_object.h"

#include "mock/mock_query_entry_fields.h"
#include "mock/mock_query_entry_group_row_security.h"
#include "mock/mock_query_entry_object.h"
#include "mock/mock_query_factory.h"
#include "mock/mock_session.h"

using mrs::database::QueryEntriesDbObject;
using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::StartsWith;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

class QueryEntriesDbObjectTests : public Test {
 public:
  void verifyAndClearMocks(const std::vector<void *> &mocks) {
    for (auto p : mocks) Mock::VerifyAndClearExpectations(p);
  }

  void expectFetch(const char *audit_id_numeric) {
    EXPECT_CALL(mock_query_factory, create_query_group_row_security())
        .WillRepeatedly(Return(mock_query_group_sec.copy_base()));
    EXPECT_CALL(mock_query_factory, create_query_fields())
        .WillRepeatedly(Return(mock_query_entry_fields.copy_base()));
    EXPECT_CALL(mock_query_factory, create_query_object())
        .WillRepeatedly(Return(mock_query_entry_object.copy_base()));

    InSequence seq;
    EXPECT_CALL(mock_session, execute(StrEq("START TRANSACTION")))
        .RetiresOnSaturation();
    EXPECT_CALL(
        mock_session,
        query(
            StrEq("SELECT max(id) FROM mysql_rest_service_metadata.audit_log"),
            _, _))
        .WillOnce(Invoke([audit_id_numeric](auto, auto &row, auto &) {
          mysqlrouter::MySQLSession::Row fields{audit_id_numeric};
          row(fields);
        }))
        .RetiresOnSaturation();
    EXPECT_CALL(mock_session, query(StartsWith("SELECT * FROM (SELECT "
                                               "  o.id as id, s.id"),
                                    _, _))
        .RetiresOnSaturation();
    EXPECT_CALL(mock_session, execute(StrEq("COMMIT"))).RetiresOnSaturation();
  }

  helper::MakeSharedPtr<StrictMock<MockQueryEntryObject>>
      mock_query_entry_object;
  helper::MakeSharedPtr<StrictMock<MockQueryEntryFields>>
      mock_query_entry_fields;
  helper::MakeSharedPtr<StrictMock<MockQueryEntryGroupRowSecurity>>
      mock_query_group_sec;
  helper::MakeSharedPtr<StrictMock<MockQueryEntryFields>> mock_query_fields;
  StrictMock<MockQueryFactory> mock_query_factory;
  StrictMock<MockMySQLSession> mock_session;
  QueryEntriesDbObject sut_{mrs::interface::kSupportedMrsMetadataVersion_2,
                            &mock_query_factory};
};

TEST_F(QueryEntriesDbObjectTests, returns_audit_id_one_without_entires) {
  expectFetch("1");
  sut_.query_entries(&mock_session);
  ASSERT_EQ(1, sut_.get_last_update());
  ASSERT_EQ(0, sut_.get_entries().size());
}

TEST_F(QueryEntriesDbObjectTests, returns_audit_id_two_without_entires) {
  expectFetch("2");
  sut_.query_entries(&mock_session);
  ASSERT_EQ(2, sut_.get_last_update());
  ASSERT_EQ(0, sut_.get_entries().size());
}

TEST_F(QueryEntriesDbObjectTests, returns_audit_id_two_with_one_entiry) {
  expectFetch("2");
  sut_.query_entries(&mock_session);
  ASSERT_EQ(2, sut_.get_last_update());
  ASSERT_EQ(0, sut_.get_entries().size());
}
