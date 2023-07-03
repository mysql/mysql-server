/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SQL_SESSION_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SQL_SESSION_H_

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/interface/sql_session.h"

namespace xpl {
namespace test {
namespace mock {

class Sql_session : public iface::Sql_session {
 public:
  Sql_session();
  virtual ~Sql_session() override;

  MOCK_METHOD(ngs::Error_code, set_connection_type, (const Connection_type),
              (override));
  MOCK_METHOD(ngs::Error_code, execute_kill_sql_session, (uint64_t),
              (override));
  MOCK_METHOD(bool, is_killed, (), (const, override));
  MOCK_METHOD(bool, password_expired, (), (const, override));
  MOCK_METHOD(std::string, get_authenticated_user_name, (), (const, override));
  MOCK_METHOD(std::string, get_authenticated_user_host, (), (const, override));
  MOCK_METHOD(bool, has_authenticated_user_a_super_priv, (), (const, override));
  MOCK_METHOD(uint64_t, mysql_session_id, (), (const, override));
  MOCK_METHOD(ngs::Error_code, authenticate,
              (const char *, const char *, const char *, const char *,
               const std::string &, const iface::Authentication &, bool),
              (override));
  MOCK_METHOD(ngs::Error_code, execute,
              (const char *, std::size_t, iface::Resultset *), (override));
  MOCK_METHOD(ngs::Error_code, execute_sql,
              (const char *, std::size_t, iface::Resultset *), (override));
  MOCK_METHOD3(fetch_cursor,
               ngs::Error_code(const std::uint32_t, const std::uint32_t,
                               iface::Resultset *));
  MOCK_METHOD(ngs::Error_code, prepare_prep_stmt,
              (const char *, std::size_t, iface::Resultset *), (override));
  MOCK_METHOD(ngs::Error_code, deallocate_prep_stmt,
              (const uint32_t, iface::Resultset *), (override));
  MOCK_METHOD(ngs::Error_code, execute_prep_stmt,
              (const uint32_t, const bool, const PS_PARAM *, std::size_t,
               iface::Resultset *),
              (override));
  MOCK_METHOD(ngs::Error_code, attach, (), (override));
  MOCK_METHOD(ngs::Error_code, detach, (), (override));
  MOCK_METHOD(ngs::Error_code, reset, (), (override));
  MOCK_METHOD(bool, is_sql_mode_set, (const std::string &), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SQL_SESSION_H_
