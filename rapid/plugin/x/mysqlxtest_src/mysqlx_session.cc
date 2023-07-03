/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

// Avoid warnings from includes of other project and protobuf
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4018 4996)
#endif

#include "mysqlx_session.h"
#include "mysqlx_protocol.h"
#include "mysqlx_error.h"

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning (pop)
#endif

#ifdef WIN32
#pragma warning(push, 0)
#endif
#ifdef WIN32
#pragma warning(pop)
#endif
#include <string>

#ifdef WIN32
#  pragma push_macro("ERROR")
#  undef ERROR
#endif


using namespace mysqlx;

Session::Session(const mysqlx::Ssl_config &ssl_config, const std::size_t timeout)
{
  m_connection.reset(new XProtocol(ssl_config, timeout));
}

Session::~Session()
{
  m_connection.reset();
}

ngs::shared_ptr<Session> mysqlx::openSession(const std::string &uri, const std::string &pass, const mysqlx::Ssl_config &ssl_config,
                                               const bool cap_expired_password, const std::size_t timeout, const bool get_caps)
{
  ngs::shared_ptr<Session> session(new Session(ssl_config, timeout));
  session->protocol()->connect(uri, pass, cap_expired_password);
  if (get_caps)
    session->protocol()->fetch_capabilities();
  return session;
}

ngs::shared_ptr<Session> mysqlx::openSession(const std::string &host, int port, const std::string &schema,
                                               const std::string &user, const std::string &pass,
                                               const mysqlx::Ssl_config &ssl_config, const std::size_t timeout,
                                               const std::string &auth_method,
                                               const bool get_caps)
{
  ngs::shared_ptr<Session> session(new Session(ssl_config, timeout));
  session->protocol()->connect(host, port);
  if (get_caps)
    session->protocol()->fetch_capabilities();
  if (auth_method.empty())
    session->protocol()->authenticate(user, pass, schema);
  else
  {
    if (auth_method == "PLAIN")
      session->protocol()->authenticate_plain(user, pass, schema);
    else if (auth_method == "MYSQL41")
      session->protocol()->authenticate_mysql41(user, pass, schema);
    else
      throw Error(CR_INVALID_AUTH_METHOD, "Invalid authentication method " + auth_method);
  }
  return session;
}

ngs::shared_ptr<Result> Session::executeSql(const std::string &sql)
{
  return m_connection->execute_sql(sql);
}

ngs::shared_ptr<Result> Session::executeStmt(const std::string &ns, const std::string &stmt,
                             const std::vector<ArgumentValue> &args)
{
  return m_connection->execute_stmt(ns, stmt, args);
}

void Session::close()
{
  if (m_connection)
  {
    m_connection->close();

    m_connection.reset();
  }
}

#ifdef WIN32
#  pragma pop_macro("ERROR")
#endif
