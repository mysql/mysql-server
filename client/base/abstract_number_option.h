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

#ifndef ABSTRACT_NUMBER_OPTION_INCLUDED
#define ABSTRACT_NUMBER_OPTION_INCLUDED

#include <string>
#include "abstract_value_option.h"

namespace Mysql{
namespace Tools{
namespace Base{
namespace Options{

/**
  Abstract option to handle numeric option values.
 */
template<typename T_type, typename T_value> class Abstract_number_option
  : public Abstract_value_option<T_type>
{
public:
  /**
    Sets minimum value boundary for option value. Smaller values passed as
    option value will be changed to this minimum value.
   */
  virtual T_type* set_minimum_value(T_value minimum)= 0;
  /**
    Sets maximum value boundary for option value. Greater values passed as
    option value will be changed to this maximum value.
   */
  virtual T_type* set_maximum_value(T_value maximum)= 0;

protected:
  /**
    Constructs new number option.
    @param value Pointer to object to receive option value.
    @param var_type my_getopt internal option type.
    @param name Name of option. It is used in command line option name as
      --name.
    @param desription Description of option to be printed in --help.
    @param default_value default value to be supplied to internal option
      data structure.
   */
  Abstract_number_option(T_value* value, ulong var_type, std::string name,
    std::string description, uint64 default_value);
};


template<typename T_type, typename T_value>Abstract_number_option<T_type, T_value>
::Abstract_number_option(
  T_value* value, ulong var_type, std::string name, std::string description,
  uint64 default_value)
  : Abstract_value_option<T_type>(
      value, var_type, name, description, default_value)
{}

}
}
}
}

#endif
