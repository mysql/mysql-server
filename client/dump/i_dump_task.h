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

#ifndef I_DUMP_TASK_INCLUDED
#define I_DUMP_TASK_INCLUDED

#include "i_data_object.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Interface for all individual dump process tasks.
 */
class I_dump_task
{
public:
  virtual ~I_dump_task();

  virtual I_data_object* get_related_db_object() const= 0;
  /**
    Returns true if task was fully completed by all elements of chain.
   */
  virtual bool is_completed() const= 0;
  /**
    Sets task completed flag. Need to be called once main chain element
    receives completion report.
   */
  virtual void set_completed()= 0;
  /**
    Returns true if task can start processing, for example when all
    dependencies are met.
   */
  virtual bool can_be_executed() const= 0;
};

}
}
}

#endif
