/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql_query_runner.h"

using namespace Mysql::Tools::Upgrade;

Mysql_query_runner::Mysql_query_runner(MYSQL* connection)
  : m_connection(connection)
{}

Mysql::Tools::Upgrade::Mysql_query_runner::Mysql_query_runner(Mysql_query_runner& source)
  : m_result_callbacks(source.m_result_callbacks),
  m_message_callbacks(source.m_message_callbacks),
  m_connection(source.m_connection)
{
}

void Mysql_query_runner::add_result_callback(
  I_callable<int, vector<string> >* result_callback)
{
  this->m_result_callbacks.push_back(result_callback);
}

void Mysql_query_runner::add_message_callback(
  I_callable<int, Mysql_message>* message_callback)
{
  this->m_message_callbacks.push_back(message_callback);
}

int Mysql_query_runner::run_query(string query)
{
  int ret= mysql_query(this->m_connection, query.c_str());

  if (ret != 0)
  {
    return this->report_mysql_error();
  }
  MYSQL_RES* results= mysql_use_result(this->m_connection);

  if (results != NULL)
  {
    for (;;)
    {
      // Feed result callbacks with results.
      MYSQL_ROW row= mysql_fetch_row(results);

      if (row == NULL)
      {
        // NULL row indicates end of rows or error
        if (mysql_errno(this->m_connection) == 0)
        {
          break;
        }
        else
        {
          mysql_free_result(results);
          return this->report_mysql_error();
        }
      }

      vector<string> processed_row= process_row_data(
        results, mysql_field_count(this->m_connection), row);

      vector<I_callable<int, vector<string> >*>::reverse_iterator it;
      for (it= this->m_result_callbacks.rbegin();
        it != this->m_result_callbacks.rend();
        it++)
      {
        int callback_result= (**it)(processed_row);
        if (callback_result != 0)
        {
          mysql_free_result(results);
          return callback_result;
        }
      }
    }
    mysql_free_result(results);
  }
  else
  {
    if (mysql_errno(this->m_connection) != 0)
    {
      return this->report_mysql_error();
    }
  }

  // Get all notes, warnings and errors of last query.
  ret= mysql_query(this->m_connection, "SHOW WARNINGS;");

  // Connection error occurred.
  if (ret != 0)
  {
    return this->report_mysql_error();
  }

  results= mysql_use_result(this->m_connection);

  if (results == NULL)
  {
    return this->report_mysql_error();
  }

  // Process all errors and warnings.
  for (;;)
  {
    // Feed message callbacks with results.
    MYSQL_ROW row= mysql_fetch_row(results);

    if (row == NULL)
    {
      // NULL row indicates end of rows or error
      if (mysql_errno(this->m_connection) == 0)
      {
        break;
      }
      else
      {
        mysql_free_result(results);
        return this->report_mysql_error();
      }
    }

    vector<string> processed_row= process_row_data(
      results, mysql_field_count(this->m_connection), row);

    Mysql_message processed_message;
    processed_message.severity= processed_row[0];
    processed_message.code= atoi(processed_row[1].c_str());
    processed_message.message= processed_row[2];

    vector<I_callable<int, Mysql_message>*>::reverse_iterator it;
    for (it= this->m_message_callbacks.rbegin();
      it != this->m_message_callbacks.rend();
      it++)
    {
      int callback_result= (**it)(processed_message);
      if (callback_result < 0)
      {
        mysql_free_result(results);
        return 0;
      }
      else if (callback_result != 0)
      {
        mysql_free_result(results);
        return callback_result;
      }
    }
  }
  mysql_free_result(results);

  return 0;
}

int Mysql_query_runner::report_mysql_error()
{
  Mysql_message message;
  message.severity= "ERROR";
  message.code= mysql_errno(this->m_connection);
  message.message= mysql_error(this->m_connection);

  vector<I_callable<int, Mysql_message>*>::reverse_iterator it;
  for (it= this->m_message_callbacks.rbegin();
    it != this->m_message_callbacks.rend();
    it++)
  {
    int callback_result= (**it)(message);
    if (callback_result < 0)
    {
      return 0;
    }
    else if (callback_result != 0)
    {
      return callback_result;
    }
  }

  return message.code;
}

vector<string> Mysql_query_runner::process_row_data(
  MYSQL_RES* results, int columns, MYSQL_ROW row)
{
  /*
   Data returned from server can contain NULL characters, data length is
   needed to properly acquire all data.
   */
  unsigned long* column_lenghts= mysql_fetch_lengths(results);

  vector<string> processed_row;
  for (int column=0; column < columns; column++)
  {
    processed_row.push_back(string(row[column], column_lenghts[column]));
  }

  return processed_row;
}
