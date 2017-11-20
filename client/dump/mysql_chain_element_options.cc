/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "client/dump/mysql_chain_element_options.h"

#include <boost/algorithm/string.hpp>

using namespace Mysql::Tools::Dump;

Mysql_chain_element_options::Mysql_chain_element_options(
  Mysql::Tools::Base::Abstract_connection_program* program)
  : m_program(program)
{}

void Mysql_chain_element_options::create_options()
{
}

Mysql::Tools::Base::Abstract_connection_program*
  Mysql_chain_element_options::get_program() const
{
  return m_program;
}
