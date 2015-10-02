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

#ifndef I_OBJECT_READER_INCLUDED
#define I_OBJECT_READER_INCLUDED

#include "i_chain_element.h"
#include "item_processing_data.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class I_object_reader : public virtual I_chain_element
{
public:
  /**
    Reads information on DB object related to task.
   */
  virtual void read_object(Item_processing_data* item_to_process)= 0;
};

}
}
}

#endif
