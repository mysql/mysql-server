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

#ifndef SHOW_VARIABLE_QUERY_EXTRACTOR_INCLUDED
#define SHOW_VARIABLE_QUERY_EXTRACTOR_INCLUDED

#include<string>
#include<vector>
#include "mysql.h"
#include "base/i_callable.h"
#include "mysql_query_runner.h"

namespace Mysql{
namespace Tools{
namespace Upgrade{

using std::string;
using std::vector;
using namespace ::Mysql::Tools::Base;

/**
  Extracts the value of server variable.
 */
class Show_variable_query_extractor
{
public:
  /**
    Extract the value of server variable.

    @param[in] query_runner MySQL query runner to use.
    @param[in] variable Name of variable to get value of.
    @param[out] value Pointer to String to store variable value to.
    @return nonzero if error was encountered.
   */
  static int get_variable_value(
    Mysql_query_runner* query_runner, string variable, string* value);

private:
  Show_variable_query_extractor();
  /**
    Result row callback to be used in query runner.
   */
  int extract_variable(vector<string> result_row);

  /**
    Temporary placeholder for extracted value.
   */
  string m_extracted_variable;
};

}
}
}

#endif
