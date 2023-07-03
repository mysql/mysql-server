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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef _MYSQLX_SESSION_H_
#define _MYSQLX_SESSION_H_

#include <vector>
#include <string>
#include <map>

#include "ngs_common/smart_ptr.h"

namespace Mysqlx
{
  namespace Resultset
  {
    class Row;
  }
}

namespace mysqlx
{
  class Result;
  class XProtocol;
  class ArgumentValue;
  struct Ssl_config;

  class Session : public ngs::enable_shared_from_this < Session >
  {
  public:
    Session(const mysqlx::Ssl_config &ssl_config, const std::size_t timeout);
    ~Session();
    ngs::shared_ptr<Result> executeSql(const std::string &sql);

    ngs::shared_ptr<Result> executeStmt(const std::string &ns, const std::string &stmt,
                                          const std::vector<ArgumentValue> &args);

    ngs::shared_ptr<XProtocol> protocol() { return m_connection; }

    void close();
  private:
    ngs::shared_ptr<XProtocol> m_connection;
  };
  typedef ngs::shared_ptr<Session> SessionRef;

  SessionRef openSession(const std::string &uri, const std::string &pass, const mysqlx::Ssl_config &ssl_config,
                         const bool cap_expired_password, const std::size_t timeout, const bool get_caps = false);
  SessionRef openSession(const std::string &host, int port, const std::string &schema,
                         const std::string &user, const std::string &pass,
                         const mysqlx::Ssl_config &ssl_config, const std::size_t timeout,
                         const std::string &auth_method = "", const bool get_caps = false);

}

#endif
