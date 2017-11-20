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

#ifndef ABSTRACT_SIMPLE_DUMP_TASK_INCLUDED
#define ABSTRACT_SIMPLE_DUMP_TASK_INCLUDED

#include <atomic>

#include "client/dump/i_dump_task.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Base class for all individual dump process tasks.
 */
class Abstract_simple_dump_task : public I_dump_task
{
public:
  Abstract_simple_dump_task();

  virtual ~Abstract_simple_dump_task();

  bool is_completed() const;

  virtual void set_completed();

private:
  std::atomic<bool> m_is_completed;
};

}
}
}

#endif
