/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_MYSQL_CONNECT
#define RPL_MYSQL_CONNECT

#include <string>
#include <vector>
#include "include/mysql.h"
#include "sql/rpl_mi.h"

/**
    result of executed query in rows<cols<value>> format where rows and cols
    both are std::vector and value is std::string.
*/
using MYSQL_RES_VAL = std::vector<std::vector<std::string>>;

/**
  std::tuple<error number, result>
  where
    first element of tuple is function return value and determines:
    0   Successful
    !0  Error

    second element of tuple is result of executed query in rows<cols<value>>
    format where rows and cols both are std::vector and value is std::string.
*/
using MYSQL_RES_TUPLE = std::tuple<uint, std::vector<std::vector<std::string>>>;

/**
  @class Mysql_connection

  Mysql client connection wrapper class to connect MySQL, execute SQL query and
  fetch query results.
*/
class Mysql_connection {
 public:
  /**
    Mysql_connection class constructor.

    @param[in] thd  The thread object.
    @param[in] mi   the pointer to the Master_info object.
    @param[in] host the host or ip address for mysql client connection.
    @param[in] port the port for mysql client connection.
    @param[in] network_namespace  the network_namespace for mysql client
                                  connection.
    @param[in] is_io_thread  to determine its IO or Monitor IO thread.
  */

  Mysql_connection(THD *thd, Master_info *mi, std::string host, uint port,
                   std::string network_namespace, bool is_io_thread = false);

  /**
    Mysql_connection class destructor.
  */
  ~Mysql_connection();

  /**
    Determine if its connected to mysql server.

    @return true if connected, false otherwise.
  */
  bool is_connected();

  /**
    Re-connect to mysql server.

    @return true if successfully reconnected, false otherwise.
  */
  bool reconnect();

  /**
    Get Mysql client connection object.

    @return Mysql client connection object.
  */
  MYSQL *get_mysql() { return m_conn; }

  /**
    Execute given sql query on connected mysql server.

    @param[in] query  sql query to execute.

    @return result of executed query in rows<cols<result>> format
            where rows and cols both are std::vector and result
            is std::string. So other then character strings need to
            be converted.
  */
  MYSQL_RES_TUPLE execute_query(std::string query) const;

 private:
  /* MySQL client connection object */
  MYSQL *m_conn{nullptr};

  /* The flag which stores if its connected to mysql server. */
  bool m_connected{false};

  /* The flag which stores if its initialized. */
  bool m_init{false};

  /* The THD object. */
  THD *m_thd{nullptr};

  /* The Master_info object. */
  Master_info *m_mi{nullptr};

  /* The host or ip address for mysql client connection. */
  std::string m_host;

  /* The port for mysql client connection. */
  uint m_port{0};

  /* The network_namespace for mysql client connection. */
  std::string m_network_namespace;

  /* The flag to determine its IO or Monitor IO thread. */
  bool m_is_io_thread{false};

  /**
    To connect to mysql server.

    @param[in] thd  The thread object.
    @param[in] mi   the pointer to the Master_info object.
    @param[in] host the host or ip address for mysql client connection.
    @param[in] port the port for mysql client connection.
    @param[in] network_namespace  the network_namespace for mysql client
                                  connection.
  */
  bool safe_connect(THD *thd, Master_info *mi, std::string host, uint port,
                    std::string network_namespace);

  /**
    To re-connect to mysql server.

    @param[in] thd  The thread object.
    @param[in] mi   the pointer to the Master_info object.
    @param[in] suppress_warnings  Suppress reconnect warning
    @param[in] host the host or ip address for mysql client connection.
    @param[in] port the port for mysql client connection.
  */
  bool safe_reconnect(THD *thd, Master_info *mi, bool suppress_warnings,
                      std::string host, uint port);
};
#endif  // RPL_MYSQL_CONNECT
