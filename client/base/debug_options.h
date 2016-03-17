/*
   Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DEBUG_OPTIONS_INCLUDED
#define DEBUG_OPTIONS_INCLUDED

#include "abstract_options_provider.h"
#include "nullable.h"

namespace Mysql{
namespace Tools{
namespace Base{

class Abstract_program;

namespace Options{

/**
  Options provider providing debugging options.
 */
class Debug_options : public Abstract_options_provider
{
public:
  /**
    Constructs new debug options provider.
    @param program Pointer to main program class.
   */
  Debug_options(Abstract_program* program);
  /**
    Creates all options that will be provided.
    Implementation of Abstract_options_provider virtual method.
   */
  virtual void create_options();
  /**
    Callback to be called when command-line options parsing have finished.
  */
  virtual void options_parsed();

private:

  void debug_option_callback(char *argument MY_ATTRIBUTE((unused)));

  Abstract_program* m_program;
  bool m_debug_info_flag;
  bool m_debug_check_flag;
  Nullable<std::string> m_dbug_option;
};

}
}
}
}

#endif
