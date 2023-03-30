/*
   Copyright (c) 2008, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SQL_CLIENT_HPP
#define SQL_CLIENT_HPP

#include <Vector.hpp>
#include <BaseString.hpp>
#include <Properties.hpp>
#include <mysql.h>

class SqlResultSet : public Properties {
public:

  // Get row with number
  bool get_row(int row_num);
  // Load next row
  bool next(void);
  // Reset iterator
  void reset(void);
  // Remove current row from resultset
  void remove();

  SqlResultSet();
  ~SqlResultSet();

  const char* column(const char* col_name);
  uint columnAsInt(const char* col_name);

  uint insertId();
  uint affectedRows();
  uint numRows(void);
  uint mysqlErrno();
  const char* mysqlError();
  const char* mysqlSqlstate();

private:
  uint get_int(const char* name);
  const char* get_string(const char* name);

  const Properties* m_curr_row;
  uint m_curr_row_num;
};


class SqlClient {
public:
  SqlClient(MYSQL* mysql);
  SqlClient(const char* _user= "root",
             const char* _password= "",
             const char* _suffix= 0);
  ~SqlClient();

  bool doQuery(const char* query);
  bool doQuery(const char* query, SqlResultSet& result);
  bool doQuery(const char* query, const Properties& args, SqlResultSet& result);

  bool doQuery(BaseString& str);
  bool doQuery(BaseString& str, SqlResultSet& result);
  bool doQuery(BaseString& str, const Properties& args, SqlResultSet& result);

  bool waitConnected(int timeout);

protected:

  bool runQuery(const char* query,
               const Properties& args,
               SqlResultSet& rows);

  bool isConnected();

  int connect();
  void disconnect();

protected:
  bool connected;
  MYSQL* mysql;
  bool free_mysql; /* Don't free mysql* if allocated elsewhere */
  BaseString default_file;
  BaseString default_group;
  BaseString user;
  BaseString password;
};

#endif
