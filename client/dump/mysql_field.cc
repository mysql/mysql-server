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

#include "mysql_field.h"

using namespace Mysql::Tools::Dump;
using namespace std;

enum enum_field_types Mysql_field::get_type() const
{
  return m_type;
}

uint Mysql_field::get_additional_flags() const
{
  return m_flags;
}

uint Mysql_field::get_character_set_nr() const
{
  return m_charsetnr;
}

std::string Mysql_field::get_name() const
{
  return m_name;
}

Mysql_field::Mysql_field(MYSQL_FIELD* field)
  : m_name(field->name),
    m_charsetnr(field->charsetnr),
    m_flags(field->flags),
    m_type(field->type)
{
}
