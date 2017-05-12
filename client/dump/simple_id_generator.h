/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SIMPLE_ID_GENERATOR_INCLUDED
#define SIMPLE_ID_GENERATOR_INCLUDED

#include "base/atomic.h"
#include "my_inttypes.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class Simple_id_generator
{
public:
  Simple_id_generator();
  uint64 create_id();

private:
  my_boost::atomic_int64_t m_next_id;
};

}
}
}

#endif
