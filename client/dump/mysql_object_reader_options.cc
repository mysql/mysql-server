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

#include "mysql_object_reader_options.h"

using namespace Mysql::Tools::Dump;

void Mysql_object_reader_options::create_options()
{
  this->create_new_option(&m_row_group_size, "extended-insert",
    "Allow usage of multiple-row INSERT syntax that include several VALUES "
    "lists. Specifies number of rows to include in single INSERT statement. "
    "Must be greater than 0")
    ->set_minimum_value(1)
    ->set_maximum_value(MAX_EXTENDED_INSERT)
    ->set_value(250);
}

Mysql_object_reader_options::Mysql_object_reader_options(
  const Mysql_chain_element_options* mysql_chain_element_options)
  : m_mysql_chain_element_options(mysql_chain_element_options)
{}
