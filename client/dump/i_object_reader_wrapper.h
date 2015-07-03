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

#ifndef I_OBJECT_READER_WRAPPER_INCLUDED
#define I_OBJECT_READER_WRAPPER_INCLUDED

#include "i_object_reader.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Represents class that directs execution of dump tasks to Object Readers.
 */
class I_object_reader_wrapper
{
public:
  virtual ~I_object_reader_wrapper()
  {}
  /**
    Add new Object Reader to supply direct execution of dump tasks to.
   */
  virtual void register_object_reader(I_object_reader* new_object_reader)= 0;
};

}
}
}

#endif
