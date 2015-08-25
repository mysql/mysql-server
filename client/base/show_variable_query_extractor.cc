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

#include "show_variable_query_extractor.h"
#include "instance_callback.h"

using namespace Mysql::Tools::Base;
using std::string;
using std::vector;

Show_variable_query_extractor::Show_variable_query_extractor()
  : m_exists(false)
{}

int64 Show_variable_query_extractor::extract_variable(
  const Mysql_query_runner::Row& result_row)
{
  this->m_extracted_variable= result_row[0];
  this->m_exists= true;
  Mysql_query_runner::cleanup_result(result_row);
  return 0;
}

int64 Show_variable_query_extractor::get_variable_value(
  Mysql_query_runner* query_runner_to_copy,
  string variable, string& value, bool& exists)
{
  Show_variable_query_extractor extractor;
  Mysql_query_runner query_runner_to_use(*query_runner_to_copy);

  Instance_callback<int64, const Mysql_query_runner::Row&,
                    Show_variable_query_extractor>
    result_cb(&extractor, &Show_variable_query_extractor::extract_variable);

  query_runner_to_use.add_result_callback(&result_cb);

  /*
    Note: Because MySQL uses the C escape syntax in strings (for example, '\n'
    to represent newline), you must double any '\' that you use in your LIKE
    strings. For example, to search for '\n', specify it as '\\n'.To search for
    '\', specify it as '\\\\' (the backslashes are stripped once by the parser
    and another time when the pattern match is done, leaving a single backslash
    to be matched).

    Example: "t\1" = > "t\\\\1"
  */
  string quoted_variable;
  for (size_t i= 0; i < variable.size(); i++)
  {
    if (variable[i] == '\\')
    {
      quoted_variable.append(3, '\\');
    }
    else if (variable[i] == '\'' || variable[i] == '_' || variable[i] == '%')
    {
      quoted_variable+= '\\';
    }
    quoted_variable+= variable[i];
  }

  query_runner_to_use.run_query("SELECT @@global." + quoted_variable);

  value= extractor.m_extracted_variable;
  exists= extractor.m_exists;
  return 0;
}
