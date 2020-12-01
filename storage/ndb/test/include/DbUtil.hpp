/* Copyright (c) 2007, 2020, Oracle and/or its affiliates.

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

// dbutil.h: interface for the database utilities class.
// Supplies a database to the test application

#ifndef DBUTIL_HPP
#define DBUTIL_HPP

#include <NDBT.hpp>
#include <BaseString.hpp>
#include <Properties.hpp>

struct MYSQL;

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
  // Clear result
  void clear();

  SqlResultSet();
  ~SqlResultSet();

  const char* column(const char* col_name);
  uint columnAsInt(const char* col_name);
  unsigned long long columnAsLong(const char* col_name);

  unsigned long long insertId();
  unsigned long long affectedRows();
  uint numRows(void);
  uint mysqlErrno();
  const char* mysqlError();
  const char* mysqlSqlstate();

private:
  uint get_int(const char* name);
  unsigned long long get_long(const char* name);
  const char* get_string(const char* name);

  const Properties* m_curr_row;
  uint m_curr_row_num;
};


class DbUtil
{
public:

  DbUtil(MYSQL* mysql);
  DbUtil(const char* dbname = "mysql",
         const char* suffix = NULL);
  ~DbUtil();

  bool doQuery(const char* query);
  bool doQuery(const char* query, SqlResultSet& result);
  bool doQuery(const char* query, const Properties& args, SqlResultSet& result);
  bool doQuery(const char* query, const Properties& args);

  bool doQuery(BaseString& str);
  bool doQuery(BaseString& str, SqlResultSet& result);
  bool doQuery(BaseString& str, const Properties& args, SqlResultSet& result);
  bool doQuery(BaseString& str, const Properties& args);

  bool waitConnected(int timeout = 120);

  unsigned long long selectCountTable(const char * table);

protected:

  bool runQuery(const char* query,
               const Properties& args,
               SqlResultSet& rows);

  bool isConnected();

private:
  MYSQL * m_mysql;
  bool m_free_mysql; /* Don't free mysql* if allocated elsewhere */

  bool m_connected;

  BaseString m_user;       // MySQL User
  BaseString m_pass;       // MySQL User Password
  BaseString m_dbname;     // Database to use
  BaseString m_default_file;
  BaseString m_default_group;

  bool connect();
  void disconnect();

  void report_error(const char* message) const;
  void printError(const char *msg) const;
};
#endif

