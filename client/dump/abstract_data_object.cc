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

#include "abstract_data_object.h"

using namespace Mysql::Tools::Dump;

Abstract_data_object::Abstract_data_object(uint64 id, const std::string& name,
  const std::string& schema)
  : m_id(id),
  m_schema(schema),
  m_name(name)
{}

std::string Abstract_data_object::get_name() const
{
  return m_name;
}

std::string Abstract_data_object::get_schema() const
{
  return m_schema;
}

uint64 Abstract_data_object::get_id() const
{
  return m_id;
}

Abstract_data_object::~Abstract_data_object()
{}
