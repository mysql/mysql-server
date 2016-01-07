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

#ifndef ABSTRACT_DUMP_TASK_INCLUDED
#define ABSTRACT_DUMP_TASK_INCLUDED

#include "abstract_simple_dump_task.h"
#include "abstract_data_object.h"
#include "i_callable.h"
#include "base/mutex.h"
#include <vector>

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Base class for most individual dump process tasks, not suitable for
  lightweight dump tasks (e.g. Row).
*/
class Abstract_dump_task : public Abstract_simple_dump_task
{
public:
  Abstract_dump_task(Abstract_data_object* related_object);

  virtual ~Abstract_dump_task();

  I_data_object* get_related_db_object() const;

  std::vector<const Abstract_dump_task*> get_dependencies() const;

  std::vector<Abstract_dump_task*> get_dependents() const;

  void add_dependency(Abstract_dump_task* dependency);

  bool can_be_executed() const;

  void set_completed();

  /**
    Registers callback to be called once this task is able to be executed.
   */
  void register_execution_availability_callback(
    Mysql::I_callable<void, const Abstract_dump_task*>* availability_callback);

private:
  void check_execution_availability();

  Abstract_data_object* m_related_object;
  std::vector<const Abstract_dump_task*> m_dependencies;
  std::vector<Abstract_dump_task*> m_dependents;
  std::vector<Mysql::I_callable<void, const Abstract_dump_task*>*>
    m_availability_callbacks;
  my_boost::mutex m_task_mutex;
};

}
}
}

#endif
