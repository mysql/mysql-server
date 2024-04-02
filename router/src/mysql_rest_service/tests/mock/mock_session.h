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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_SESSION_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_SESSION_H_

#include <gmock/gmock.h>

#include "collector/counted_mysql_session.h"

class MockMySQLSession : public collector::CountedMySQLSession {
 public:
  MOCK_METHOD(void, connect,
              (const std::string &, unsigned int, const std::string &,
               const std::string &, const std::string &, const std::string &,
               int, int, unsigned long),
              (override));
  MOCK_METHOD(void, connect,
              (const MySQLSession &, const std::string &, const std::string &),
              (override));
  MOCK_METHOD(void, disconnect, (), (override));
  MOCK_METHOD(void, connect_and_set_opts,
              (const ConnectionParameters &, const Sqls &), (override));
  MOCK_METHOD(void, change_user,
              (const std::string &, const std::string &, const std::string &),
              (override));
  MOCK_METHOD(void, reset, (), (override));

  MOCK_METHOD(ConnectionParameters, get_connection_parameters, (),
              (const, override));

  MOCK_METHOD(uint64_t, prepare, (const std::string &), (override));
  MOCK_METHOD(void, prepare_execute,
              (uint64_t, std::vector<enum_field_types>,
               const ResultRowProcessor &, const FieldValidator &),
              (override));
  MOCK_METHOD(void, prepare_remove, (uint64_t), (override));

  MOCK_METHOD(void, execute, (const std::string &), (override));
  MOCK_METHOD(void, query,
              (const std::string &, const ResultRowProcessor &,
               const FieldValidator &),
              (override));
  MOCK_METHOD(std::unique_ptr<MySQLSession::ResultRow>, query_one,
              (const std::string &), (override));
  MOCK_METHOD(std::unique_ptr<MySQLSession::ResultRow>, query_one,
              (const std::string &, const FieldValidator &), (override));

  MOCK_METHOD(uint64_t, last_insert_id, (), (override, noexcept));
  MOCK_METHOD(uint64_t, affected_rows, (), (override, noexcept));
  MOCK_METHOD(unsigned, warning_count, (), (override, noexcept));
  MOCK_METHOD(std::string, quote, (const std::string &, char),
              (const, override));
  MOCK_METHOD(bool, is_connected, (), (override, noexcept));

  MOCK_METHOD(const char *, last_error, (), (override));
  MOCK_METHOD(unsigned int, last_errno, (), (override));
  MOCK_METHOD(const char *, ssl_cipher, (), (override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_SESSION_H_
