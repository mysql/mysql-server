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

#ifndef I_OUTPUT_WRITER_WRAPPER_INCLUDED
#define I_OUTPUT_WRITER_WRAPPER_INCLUDED

#include "i_output_writer.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Represents class that directs execution of dump tasks to Output Writers.
 */
class I_output_writer_wrapper
{
public:
  virtual ~I_output_writer_wrapper()
  {}
  /**
    Add new Output Writer to supply formatted strings to.
   */
  virtual void register_output_writer(I_output_writer* new_output_writer)= 0;
};

}
}
}

#endif
