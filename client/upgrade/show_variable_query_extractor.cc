/*
   Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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
#include "base/instance_callback.h"

using namespace Mysql::Tools::Upgrade;

Show_variable_query_extractor::Show_variable_query_extractor()
{}

int Show_variable_query_extractor::extract_variable(vector<string> result_row)
{
  this->m_extracted_variable= result_row[0];

  return 0;
}

int Show_variable_query_extractor::get_variable_value(
  Mysql_query_runner* query_runner_to_copy, string variable, string* value)
{
  Show_variable_query_extractor extractor;
  Mysql_query_runner query_runner_to_use(*query_runner_to_copy);

  query_runner_to_use.add_result_callback(
    new Instance_callback<int, vector<string>, Show_variable_query_extractor>(
      &extractor, &Show_variable_query_extractor::extract_variable));

  /*
   * If show_compatibility_56 is ON, variables' values could be read from
   * information_schema. But if it is OFF, variables' values to be read
   * from performance_schema.
   */
  query_runner_to_use.run_query("SELECT @@show_compatibility_56");
  if (extractor.m_extracted_variable.compare("1") == 0)
  {
    if (query_runner_to_use.run_query(
      "SELECT VARIABLE_VALUE FROM information_schema.global_variables WHERE "
      "variable_name='" + variable + "'"))
    {
      return 1;
    }
  }
  else
  {
    if (query_runner_to_use.run_query(
      "SELECT VARIABLE_VALUE FROM performance_schema.global_variables WHERE "
      "variable_name='" + variable + "'"))
    {
      return 1;
    }
  }

  *value= extractor.m_extracted_variable;
  return 0;
}
