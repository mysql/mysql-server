/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_MYSQL_CLIENT_THREAD_TOKEN_INCLUDED
#define ROUTER_MYSQL_CLIENT_THREAD_TOKEN_INCLUDED

#include <stdexcept>

#include <mysql.h>

namespace mysqlrouter {

/**
 * Thread Token for libmysqlclient API users.
 *
 * libmysqlclient requires that all threads that used the API deinit
 * via mysql_thread_end()
 *
 * @note Not calling mysql_thread_end() for a thread which had
 * mysql_thread_init() called leads to 5 seconds wait on mysql_library_end()
 * in debug-builds.
 *
 * While the first call to to mysql_init() in a thread calls,
 * mysql_thread_init() automatically, there is no equivalent for the shutdown.
 *
 * Placing the thread token on the stack ensures right after the
 * thread is started ensures, the thread is properly accounted for
 * by libmysqlclient.
 *
 * @code
 * void *some_thread(void *) {
 *   MySQLClientThreadToken api_token;
 *
 *   if (true) throw std::runtime_error();
 *
 *   return nullptr;
 * }
 * @endcode
 *
 * @see mysql_library_end()
 * @see mysql_thread_end()
 * @see my_thread_end()
 */
class MySQLClientThreadToken final {
 public:
  MySQLClientThreadToken() { mysql_thread_init(); }

  ~MySQLClientThreadToken() { mysql_thread_end(); }

 private:
  // disable copy constructor
  MySQLClientThreadToken(const MySQLClientThreadToken &) = delete;
  // disable copy assignment
  MySQLClientThreadToken &operator=(const MySQLClientThreadToken &) = delete;
  // disable move constructor
  MySQLClientThreadToken(MySQLClientThreadToken &&) = delete;
  // disable move assignment
  MySQLClientThreadToken &operator=(MySQLClientThreadToken &&) = delete;
};

}  // namespace mysqlrouter
#endif
