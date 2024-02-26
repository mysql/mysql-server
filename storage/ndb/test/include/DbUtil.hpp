/* Copyright (c) 2007, 2023, Oracle and/or its affiliates.

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

// Interface for the database utilities class that supplies an object
// oriented way to work with MySQL in test applications

#ifndef DBUTIL_HPP
#define DBUTIL_HPP

#include <BaseString.hpp>
#include <Properties.hpp>

struct MYSQL;

class SqlResultSet : public Properties {
public:

  // Get row with number
  bool get_row(int row_num);
  // Load next row
  bool next();
  // Reset iterator
  void reset();
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
  uint numRows();
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

  /*
   The DbUtil class can be used in two modes.
    1) The class owns its MySQL object which it will create,
       connect and release.
    2) The class only uses a MYSQL object which is passed in by the
       caller, in this mode it's assumed that the MYSQL object has been
       created and is connected. The class will not release the MySQL
       object (since it's not owned by the class).
   */
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

  /*
     Usage of DbUtil initializes the MySQL library and allocates resources in
     the thread that need to be released.
     */
  static void thread_end();

protected:

  bool runQuery(const char* query,
               const Properties& args,
               SqlResultSet& rows);

  bool isConnected();

private:
  MYSQL * m_mysql;
  const bool m_owns_mysql; // The MYSQL object is owned by this class

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

