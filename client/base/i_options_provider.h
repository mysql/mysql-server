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

#ifndef I_OPTIONS_PROVIDER_INCLUDED
#define I_OPTIONS_PROVIDER_INCLUDED

#include <vector>
#include <my_getopt.h>
#include "i_option_changed_listener.h"

namespace Mysql{
namespace Tools{
namespace Base{
namespace Options{

/**
  Interface for basic options providers functionality.
 */
class I_options_provider : public I_option_changed_listener
{
public:
  /**
    Creates list of options provided by this provider.
    @returns list of my_getopt internal option data structures.
   */
  virtual std::vector<my_option> generate_options()= 0;
  /**
    Callback to be called when command-line options parsing have finished.
   */
  virtual void options_parsed()= 0;
  /**
    Sets optional option changes listener to which all changes in all options
    contained in this provider should be reported. This is used when this
    provider is attached to another.
   */
  virtual void set_option_changed_listener(
    I_option_changed_listener* listener)= 0;
};

}
}
}
}

#endif
