/*
   Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef ENUM_OPTION_INCLUDED
#define ENUM_OPTION_INCLUDED

#include <string>
#include "abstract_enum_option.h"

namespace Mysql{
namespace Tools{
namespace Base{
namespace Options{

/**
  Enum value option.
 */
template<typename T_type, typename T_typelib>
class Enum_option : public Abstract_enum_option<T_type, T_typelib>
{
public:
  /**
    Constructs new enum option.
    @param value Pointer to enum object to receive option value.
    @param value Pointer to enum tylelib.
    @param name Name of option. It is used in command line option name as
      --name.
    @param desription Description of option to be printed in --help.
   */
  Enum_option(T_type* value, const T_typelib* type, std::string name, std::string description)
    : Abstract_enum_option<T_type, T_typelib>(value, type, GET_ENUM, name, description, 1),
      m_value(value)
  {}

  void set_value(T_type value)
  {
    *m_value= value;
  }
private:
  T_type* m_value;
};

}
}
}
}

#endif
