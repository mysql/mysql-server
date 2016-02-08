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

#include "abstract_dump_task.h"

using namespace Mysql::Tools::Dump;

Abstract_dump_task::Abstract_dump_task(Abstract_data_object* related_object)
  : m_related_object(related_object)
{}

Abstract_dump_task::~Abstract_dump_task()
{}

I_data_object* Abstract_dump_task::get_related_db_object() const
{
  return m_related_object;
}

void Abstract_dump_task::check_execution_availability()
{
  if (m_availability_callbacks.size() > 0 && this->can_be_executed())
  {
    my_boost::mutex::scoped_lock lock(m_task_mutex);

    for (std::vector<Mysql::I_callable<void, const Abstract_dump_task*>*>
      ::const_iterator it = m_availability_callbacks.begin();
      it != m_availability_callbacks.end(); ++it)
    {
      (**it)(this);
    }
    m_availability_callbacks.clear();
  }
}

void Abstract_dump_task::register_execution_availability_callback(
  Mysql::I_callable<void, const Abstract_dump_task*>* availability_callback)
{
  my_boost::mutex::scoped_lock lock(m_task_mutex);
  m_availability_callbacks.push_back(availability_callback);
}

void Abstract_dump_task::set_completed()
{
  Abstract_simple_dump_task::set_completed();
  my_boost::mutex::scoped_lock lock(m_task_mutex);
  for (std::vector<Abstract_dump_task*>::iterator
    it= m_dependents.begin(); it != m_dependents.end(); ++it)
  {
    (*it)->check_execution_availability();
  }
}

bool Abstract_dump_task::can_be_executed() const
{
  for (std::vector<const Abstract_dump_task*>::const_iterator it=
    m_dependencies.begin(); it != m_dependencies.end(); ++it)
  {
    if ((*it)->is_completed() == false)
      return false;
  }

  return true;
}

void Abstract_dump_task::add_dependency(Abstract_dump_task* dependency)
{
  m_dependencies.push_back(dependency);
  my_boost::mutex::scoped_lock lock(dependency->m_task_mutex);
  dependency->m_dependents.push_back(this);
}

std::vector<Abstract_dump_task*> Abstract_dump_task::get_dependents()
  const
{
  return m_dependents;
}

std::vector<const Abstract_dump_task*> Abstract_dump_task::get_dependencies()
  const
{
  return m_dependencies;
}
