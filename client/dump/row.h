/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROW_INCLUDED
#define ROW_INCLUDED

#include "base/mysql_query_runner.h"
#include "i_data_object.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Represents single data row.
 */
class Row : public I_data_object
{
public:
  Row(const Mysql::Tools::Base::Mysql_query_runner::Row& row_data);

  ~Row();

  /**
    Returns all raw data of fields.
   */
  const Mysql::Tools::Base::Mysql_query_runner::Row& m_row_data;
};

}
}
}

#endif
