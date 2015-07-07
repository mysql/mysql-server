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

#include "row.h"

using namespace Mysql::Tools::Dump;

Row::~Row()
{
  Mysql::Tools::Base::Mysql_query_runner::cleanup_result(m_row_data);
}

Row::Row(const Mysql::Tools::Base::Mysql_query_runner::Row& row_data)
  : m_row_data(row_data)
{}
