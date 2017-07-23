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

#ifndef DUMP_START_DUMP_TASK_INCLUDED
#define DUMP_START_DUMP_TASK_INCLUDED

#include "abstract_dump_task.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class Dump_start_dump_task : public Abstract_dump_task
{
public:
  Dump_start_dump_task(std::string, std::string);
  std::string m_gtid_mode;
  std::string m_gtid_executed;
};

}
}
}

#endif
