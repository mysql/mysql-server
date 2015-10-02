/*
   Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#ifndef ABSTRACT_VALUE_OPTION_INCLUDED
#define ABSTRACT_VALUE_OPTION_INCLUDED

#include <string>
#include <my_getopt.h>
#include "abstract_option.h"

namespace Mysql{
namespace Tools{
namespace Base{
namespace Options{

/**
  Abstract option to handle options accepting value argument.
 */
template<typename T_type> class Abstract_value_option
  : public Abstract_option<T_type>
{
public:
  /**
    Sets arg_type to OPT_ARG - doesn't require argument to be specified when
    using this option.
   */
  T_type* value_optional();

protected:
  /**
    Constructs new option with value argument.
    @param value Pointer to object to receive option value.
    @param var_type my_getopt internal option type.
    @param name Name of option. It is used in command line option name as
      --name.
    @param desription Description of option to be printed in --help.
    @param default_value default value to be supplied to internal option
      data structure.
   */
  Abstract_value_option(void* value, ulong var_type, std::string name,
    std::string description, uint64 default_value);
};


template<typename T_type> Abstract_value_option<T_type>::Abstract_value_option(
    void* value, ulong var_type, std::string name, std::string description,
    uint64 default_value)
  : Abstract_option<T_type>(value, var_type, name, description, default_value)
{}

template<typename T_type> T_type* Abstract_value_option<T_type>::value_optional()
{
  this->m_option_structure.arg_type= OPT_ARG;
  return (T_type*)this;
}

}
}
}
}
#endif
