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

#ifndef DISABLED_OPTION_INCLUDED
#define DISABLED_OPTION_INCLUDED

#include <my_getopt.h>
#include "abstract_option.h"

namespace Mysql{
namespace Tools{
namespace Base{
namespace Options{

/**
  Disabled option. This option is used to mark existance of options
  inavailable due to distribution configuration.
 */
class Disabled_option : public Abstract_option<Disabled_option>
{
public:
  /**
    Constructs new disabled option.
    @param name Name of option. It is used in command-line option name as
      --name.
    @param desription Description of option to be printed in --help.
   */
  Disabled_option(std::string name, std::string description);
};

}
}
}
}

#endif
