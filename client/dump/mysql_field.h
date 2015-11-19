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

#ifndef MYSQL_FIELD_INCLUDED
#define MYSQL_FIELD_INCLUDED

#include "my_global.h"
#include "mysql.h"
#include <string>

namespace Mysql{
namespace Tools{
namespace Dump{

class Mysql_field
{
public:
  Mysql_field(MYSQL_FIELD* field);

  std::string get_name() const;

  uint get_character_set_nr() const;

  uint get_additional_flags() const;

  enum enum_field_types get_type() const;

private:
  std::string m_name;
  unsigned int m_charsetnr;
  unsigned int m_flags;
  enum enum_field_types m_type;
};

}
}
}

#endif
