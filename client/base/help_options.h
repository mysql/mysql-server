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

#ifndef HELP_OPTIONS_INCLUDED
#define HELP_OPTIONS_INCLUDED

#include "abstract_options_provider.h"

namespace Mysql{
namespace Tools{
namespace Base{

class Abtract_program;

namespace Options{

/**
  Options provider providing --help option and handling usage printing.
 */
class Help_options : public Abstract_options_provider
{
public:
  /**
    Constructs new help options provider.
    @param program Pointer to main program class, used to collect list of all
      options available in program.
   */
  Help_options(Base::Abstract_program *program);
  /**
    Creates all options that will be provided.
    Implementation of Abstract_options_provider virtual method.
   */
  virtual void create_options();
  /**
    Prints program usage message.
  */
  virtual void print_usage();

private:
  void help_callback(char* argument MY_ATTRIBUTE((unused)));
  void version_callback(char* argument MY_ATTRIBUTE((unused)));

  void print_version_line();

  Abstract_program *m_program;
};

}
}
}
}

#endif
