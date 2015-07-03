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

#ifndef I_CRAWLER_INCLUDED
#define I_CRAWLER_INCLUDED

#include "i_chain_element.h"
#include "i_chain_maker.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class I_crawler : public virtual I_chain_element
{
public:
  /**
    Enumerates all objects it can access, gets chains from all registered
    chain_maker for each object and then execute each chain.
   */
  virtual void enumerate_objects()= 0;
  /**
    Adds new Chain Maker to ask for chains for found objects.
   */
  virtual void register_chain_maker(I_chain_maker* new_chain_maker)= 0;
};

}
}
}

#endif
