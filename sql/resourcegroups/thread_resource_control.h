/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#ifndef RESOURCEGROUPS_THREAD_RESOURCE_CONTROL_H_
#define RESOURCEGROUPS_THREAD_RESOURCE_CONTROL_H_

#include <memory>
#include <vector>

#include "sql/resourcegroups/resource_group_basic_types.h" // Range, Type

namespace resourcegroups
{


/**
  Class that abstracts the resource control that can be applied
  to threads. The resource controls include set of CPU IDS which
  determine the CPUS the thread is allowed to run and the thread
  priority.
*/

class Thread_resource_control
{
public:
  /**
    Default constructor
  */

  Thread_resource_control()
    :m_priority(0)
  {}


  /**
    Get priority associated with Thread resource control object.

    @return an int value indicating the thread priority.
  */

  int  priority() const { return m_priority; }


  /**
    Set priority associated with Thread resource control object.
  */

  void set_priority(int priority)
  {
    m_priority= priority;
  }

  /**
    Get const pointer of vector of CPU ID range.

    @return pointer to vector of CPU ID range.
  */

  const std::vector<Range> &vcpu_vector() const
  {
    return m_vcpu_vector;
  }


  /**
    Set the CPU ID range vector.
  */

  void set_vcpu_vector(const std::vector<Range> &vcpu_vector)
  {
    m_vcpu_vector.clear();
    for (const auto &cpu_range: vcpu_vector)
      m_vcpu_vector.emplace_back(cpu_range);
  }


  /**
    Apply the thread resource controls to the thread on
    which this function is called.

    @return false if control application is successful else true
  */

  bool apply_control();


  /**
    Apply the thread resource controls to thread identified by
    the thread os id.

    @param  thread_os_id Thread OS ID.

    @return false if control application is successful else true
  */

  bool apply_control(my_thread_os_id_t thread_os_id);


  /**
    Validate the CPU ID Ranges and thread priority value associate with
    the thread resource control object.

    @return false if validation is successful else true.
  */

  bool validate(const Type &resource_group_type) const;
private:

  /**
    Vector of CPU ID range.
  */
  std::vector<Range> m_vcpu_vector;

  /**
    Thread priority value
  */
  int m_priority;
};
}
#endif // RESOURCEGROUPS_THREAD_RESOURCE_CONTROL_H_
