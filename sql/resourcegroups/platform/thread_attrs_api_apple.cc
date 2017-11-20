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
#include "thread_attrs_api.h"

#include "sql/log.h"
#include "my_dbug.h"

namespace resourcegroups
{
namespace platform
{


/*
  Mac OS doesn't have an explicit API to bind a set of processors with thread.
  Hence platform APIs are just stubs on this platform.
*/

bool is_platform_supported()
{
  return false;
}

bool bind_to_cpu(cpu_id_t cpu_id)
{
  DBUG_ASSERT(0);
  return true;
}

bool bind_to_cpu(cpu_id_t cpu_id, my_thread_os_id_t thread_id)
{
  DBUG_ASSERT(0);
  return true;
}

bool bind_to_cpus(const std::vector<cpu_id_t> &cpu_ids)
{
  DBUG_ASSERT(0);
  return true;
}

bool bind_to_cpus(const std::vector<cpu_id_t> &cpu_ids,
                  my_thread_os_id_t thread_id)
{
  DBUG_ASSERT(0);
  return true;
}

bool unbind_thread()
{
  DBUG_ASSERT(0);
  return true;
}

bool unbind_thread(my_thread_os_id_t thread_id)
{
  DBUG_ASSERT(0);
  return true;
}

int thread_priority(my_thread_os_id_t thread_id)
{
  DBUG_ASSERT(0);
  return 0;
}

bool set_thread_priority(int priority)
{
  DBUG_ASSERT(0);
  return true;
}

bool set_thread_priority(int priority, my_thread_os_id_t thread_id)
{
  DBUG_ASSERT(0);
  return true;
}

uint32_t num_vcpus()
{
  DBUG_ASSERT(0);
  return 0;
}

bool can_thread_priority_be_set()
{
  DBUG_ASSERT(0);
  return false;
}
}  // platform
}  // resourcegroups
