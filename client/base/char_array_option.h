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

#ifndef CHAR_ARRAY_OPTION_INCLUDED
#define CHAR_ARRAY_OPTION_INCLUDED

#include "abstract_value_option.h"

namespace Mysql{
namespace Tools{
namespace Base{
namespace Options{

/**
  Option class to get string parameter value and set to char* type object.
 */
class Char_array_option : public Abstract_value_option<char*>
{
public:
  /**
    Constructs new string option.
    @param value Pointer to char* object to receive option value.
    @param allocated Specifies if value set should be some static string or
      dynamically allocated string with my_strdup.
    @param name Name of option. It is used in command line option name as
      --name.
    @param desription Description of option to be printed in --help.
   */
  Char_array_option(
    char** value, bool allocated, std::string name, std::string description);

  /**
    Sets value for this option. If it is specified before handling commandline
    options then supplied value is used as default value of this option.
   */
  Char_array_option* set_value(char* value);
};

}
}
}
}

#endif
