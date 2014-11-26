/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef I_OPTION_INCLUDED
#define I_OPTION_INCLUDED

#include "my_getopt.h"

namespace Mysql{
namespace Tools{
namespace Base{

class Abstract_program;

namespace Options{

class I_option_changed_listener;

/**
  Common interface for all program option objects.
 */
class I_option
{
public:
  virtual ~I_option();

protected:
  /**
    Calls all option value callbacks.
    To be used only from Abstract_program.
   */
  virtual void call_callbacks(char* argument)= 0;
  /**
    Internal method to get my_getopt internal option data structure.
   */
  virtual my_option get_my_option()= 0;

  /**
    Method to set listener on optid changed event.
    For use from Abstract_options_provider class only.
   */
  virtual void set_option_changed_listener(
    I_option_changed_listener* listener)= 0;

  static uint32 last_optid;

  friend class Abstract_options_provider;
  friend class Mysql::Tools::Base::Abstract_program;
};

}
}
}
}

#endif
