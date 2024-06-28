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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_QUERY_FACTORY_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_QUERY_FACTORY_H_

#include "mrs/interface/query_factory.h"

class MockQueryFactory : public mrs::interface::QueryFactory {
 public:
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryAuditLogEntries>,
              create_query_audit_log, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryEntriesAuthPrivileges>,
              create_query_auth_privileges, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryRestSPMedia>,
              create_query_sp_media, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryEntryGroupRowSecurity>,
              create_query_group_row_security, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryEntryContentFile>,
              create_query_content_file, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryEntryAuthUser>,
              create_query_auth_user, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryEntryObject>,
              create_query_object, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryUserGroups>,
              create_query_user_groups, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryRestTable>,
              create_query_table, (), (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryRestTableSingleRow>,
              create_query_table_single_row, (bool encode_bigints_as_string),
              (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryRestSP>, create_query_sp, (),
              (override));
  MOCK_METHOD(std::shared_ptr<mrs::database::QueryEntryFields>,
              create_query_fields, (), (override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_QUERY_FACTORY_H_
