/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MYSQL_QUERY_RUNNER_INCLUDED
#define MYSQL_QUERY_RUNNER_INCLUDED

#include<string>
#include<vector>
#include "mysql.h"
#include "base/i_callable.h"

namespace Mysql{
namespace Tools{
namespace Upgrade{

using std::string;
using std::vector;
using namespace Mysql::Tools::Base;

/**
  Structure to represent message from server sent after executing query.
 */
struct Mysql_message
{
  string severity;
  int code;
  string message;
};

/**
  Helper class to run SQL query on existing MySQL database server connection,
  receive all data and all errors, warnings and notes returned during query
  execution. All acquired information is passed to set of callbacks to make
  data flows more customizable.
 */
class Mysql_query_runner
{
public:
  /**
    Standard constructor based on MySQL connection.
   */
  Mysql_query_runner(MYSQL* connection);
  /**
    Copy constructor.
   */
  Mysql_query_runner(Mysql_query_runner& source);
  /**
    Adds new callback to be called on every result row of query.
    If callback return value other than 0 then query execution, passing
    current row to other callbacks and error messages processing, and
    Mysql_query_runner::run_query() will return value returned from this
    callback.
    Callbacks are called in reverse order of addition, i.e. newest are first.
   */
  void add_result_callback(I_callable<int, vector<string> >* result_callback);
  /**
    Adds new callback to be called on every message after query execution,
    this includes errors, warnings and other notes. Return value from callback
    of 0 will lead to next handler being called, positive number return value
    will cause Mysql_query_runner::run_query() will return immediately this
    value and negative number will continue query execution and other messages
    processing, but will not pass current message to rest of callbacks.
    Callbacks are called in reverse order of addition, i.e. newest are first.
   */
  void add_message_callback(I_callable<int, Mysql_message>* message_callback);

  /**
    Runs specified query and process result rows and messages to callbacks.
   */
  int run_query(string query);

private:
  /**
    Creates error message from mysql_errno and mysql_error and passes it to
    callbacks.
   */
  int report_mysql_error();
  /**
    Transform data in specified row to form of vector of strings.
   */
  vector<string> process_row_data(
    MYSQL_RES* results, int columns, MYSQL_ROW row);

  vector<I_callable<int, vector<string> >*> m_result_callbacks;
  vector<I_callable<int, Mysql_message>*> m_message_callbacks;

  MYSQL* m_connection;
};

}
}
}

#endif
