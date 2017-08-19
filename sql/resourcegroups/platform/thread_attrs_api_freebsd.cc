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

#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/cpuset.h>
#include <errno.h>

#include "sql/log.h"
#include "my_dbug.h"

namespace resourcegroups
{
namespace platform
{

bool is_platform_supported()
{
  return true;
}

bool bind_to_cpu(cpu_id_t cpu_id)
{
  return bind_to_cpu(cpu_id, my_thread_os_id());
}

bool bind_to_cpu(cpu_id_t cpu_id, my_thread_os_id_t thread_id)
{
  DBUG_ENTER("bind_to_cpu");

  cpuset_t cpu_set;

  CPU_ZERO(&cpu_set);
  CPU_SET(cpu_id, &cpu_set);
  if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, thread_id,
                         sizeof(cpu_set), &cpu_set) == -1)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("Unable to bind thread id %llu to "
                    "cpu id %u (error code %d - %-.192s)",
                    thread_id, cpu_id, my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}


bool bind_to_cpus(const std::vector<cpu_id_t> &cpu_ids)
{
  return bind_to_cpus(cpu_ids, my_thread_os_id());
}

bool bind_to_cpus(const std::vector<cpu_id_t> &cpu_ids,
                  my_thread_os_id_t thread_id)
{
  DBUG_ENTER("bind_to_cpu");

  if (cpu_ids.empty())
    DBUG_RETURN(false);

  cpuset_t cpu_set;

  CPU_ZERO(&cpu_set);
  for (const auto &cpu_id : cpu_ids)
    CPU_SET(cpu_id, &cpu_set);

  if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, thread_id,
                         sizeof(cpu_set), &cpu_set) == -1)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("Unable to bind thread id %llu to "
                    "cpu ids (error code %d - %-.192s)",
                    thread_id, my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}


bool unbind_thread()
{
  return unbind_thread(my_thread_os_id());
}

bool unbind_thread(my_thread_os_id_t thread_id)
{
  DBUG_ENTER("unbind_thread");

  cpuset_t cpu_set;

  CPU_ZERO(&cpu_set);
  uint32_t num_cpus= num_vcpus();
  if (num_cpus == 0)
  {
    sql_print_error("Unable to unbind thread %llu", thread_id);
    DBUG_RETURN(true);
  }
  for (cpu_id_t cpu_id= 0; cpu_id < num_cpus; ++cpu_id)
    CPU_SET(cpu_id, &cpu_set);
  if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, thread_id,
                         sizeof(cpu_set), &cpu_set) == -1)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("Unbind thread id %llu failed. (error code %d - %-.192s)",
                    thread_id, my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


int thread_priority()
{
  return getpriority(PRIO_PROCESS, my_thread_os_id());
}


int thread_priority(my_thread_os_id_t thread_id)
{
  DBUG_ASSERT(0);
  sql_print_warning("Retrieval of thread priority unsupported on FreeBSD");
  return 0;
}


bool set_thread_priority(int priority)
{
  return set_thread_priority(priority, my_thread_os_id());
}


bool set_thread_priority(int priority, my_thread_os_id_t thread_id)
{
  DBUG_ENTER("set_thread_priority");
  // Thread priority setting unsupported in FreeBSD.
  DBUG_RETURN(false);
}


uint32_t num_vcpus()
{
  cpu_id_t num_vcpus= 0;
  size_t num_vcpus_size= sizeof(cpu_id_t);
  if (sysctlbyname("hw.ncpu", &num_vcpus, &num_vcpus_size, nullptr, 0) != 0)
  {
    sql_print_error("Unable to determine the number of cpus");
    num_vcpus= 0;
  }
  return num_vcpus;
}

bool can_thread_priority_be_set()
{
   return false;
}
}  // platform
}  // resourcegroups
