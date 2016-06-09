/*
  Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef I_CHAIN_MAKER_INCLUDED
#define I_CHAIN_MAKER_INCLUDED

#include "i_chain_element.h"
#include "i_object_reader.h"
#include "i_dump_task.h"
#include "chain_data.h"
#include "my_global.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class I_chain_maker : public virtual I_chain_element
{
public:
  /**
    Creates new chain for specified dump task. May return null if do not want
    to process specified object.
   */
  virtual I_object_reader* create_chain(
    Chain_data* chain_data, I_dump_task* dump_task)= 0;
  /**
    Frees resources used by specified chain.
   */
  virtual void delete_chain(
    uint64 chain_id, I_object_reader* chain)= 0;

  virtual void stop_queues()= 0;
};

}
}
}

#endif
