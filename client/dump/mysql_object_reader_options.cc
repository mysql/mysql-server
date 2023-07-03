/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include "client/dump/mysql_object_reader_options.h"

using namespace Mysql::Tools::Dump;

void Mysql_object_reader_options::create_options() {
  this->create_new_option(&m_row_group_size, "extended-insert",
                          "Allow usage of multiple-row INSERT syntax that "
                          "include several VALUES "
                          "lists. Specifies number of rows to include in "
                          "single INSERT statement. "
                          "Must be greater than 0")
      ->set_minimum_value(1)
      ->set_maximum_value(MAX_EXTENDED_INSERT)
      ->set_value(250);
  this->create_new_option(&m_skip_gipk, "skip-generated-invisible-primary-key",
                          "Controls whether generated primary key and key "
                          "column should be dumped or not.")
      ->set_value(false);
}

Mysql_object_reader_options::Mysql_object_reader_options(
    const Mysql_chain_element_options *mysql_chain_element_options)
    : m_mysql_chain_element_options(mysql_chain_element_options) {}
