/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#include <map>

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

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
  class Schema;
  class XProtocol;
  class ArgumentValue;
  struct Ssl_config;

  class Session : public boost::enable_shared_from_this < Session >
  {
  public:
    Session(const mysqlx::Ssl_config &ssl_config, const std::size_t timeout);
    ~Session();
    boost::shared_ptr<Result> executeSql(const std::string &sql);

    boost::shared_ptr<Result> executeStmt(const std::string &ns, const std::string &stmt,
                                          const std::vector<ArgumentValue> &args);

    boost::shared_ptr<Schema> getSchema(const std::string &name);

    boost::shared_ptr<XProtocol> protocol() { return m_connection; }

    void close();
  private:
    boost::shared_ptr<XProtocol> m_connection;
    std::map<std::string, boost::shared_ptr<Schema> > m_schemas;
  };
  typedef boost::shared_ptr<Session> SessionRef;

  SessionRef openSession(const std::string &uri, const std::string &pass, const mysqlx::Ssl_config &ssl_config,
                         const bool cap_expired_password, const std::size_t timeout, const bool get_caps = false);
  SessionRef openSession(const std::string &host, int port, const std::string &schema,
                         const std::string &user, const std::string &pass,
                         const mysqlx::Ssl_config &ssl_config, const std::size_t timeout,
                         const std::string &auth_method = "", const bool get_caps = false);

}

#endif
