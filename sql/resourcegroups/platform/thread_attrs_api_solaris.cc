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

#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include "sys/pset.h"

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
  if (processor_bind(P_LWPID, P_MYID, static_cast<processorid_t>(cpu_id),
                     nullptr) == -1)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("bind_to_cpu failed: processor_bind for cpuid %u failed."
                    "(error code %d - %-.192s)", cpu_id, my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    return true;
  }
  return false;
}


bool bind_to_cpu(cpu_id_t cpu_id, my_thread_os_id_t thread_id)
{
  if (processor_bind(P_LWPID, thread_id, static_cast<processorid_t>(cpu_id),
                     nullptr) == -1)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("bind_to_cpu failed: processor_bind for thread %%llx with "
                    "cpu id %u (error code %d - %-.192s)",
                    thread_id, cpu_id, my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    return true;
  }
  return false;
}


bool bind_to_cpus(const std::vector<cpu_id_t> &cpu_ids)
{
  if (cpu_ids.empty())
    return false;

  procset_t ps;
  uint_t nids= cpu_ids.size();
  id_t *ids= reinterpret_cast<id_t *>(const_cast<unsigned *>(cpu_ids.data()));
  uint32_t flags= PA_TYPE_CPU | PA_AFF_STRONG;

  setprocset(&ps, POP_AND, P_PID, P_MYID, P_LWPID,
             my_thread_os_id());
  if (processor_affinity(&ps, &nids, ids, &flags) != 0)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("bind_to_cpus failed: processor_affinity failed with"
                    " error code %d - %-.192s", my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    return true;
  }
  return false;
}


bool bind_to_cpus(const std::vector<cpu_id_t> &cpu_ids,
                  my_thread_os_id_t thread_id)
{
  procset_t ps;
  uint_t nids= cpu_ids.size();
  id_t *ids= reinterpret_cast<id_t *>(const_cast<unsigned *>(cpu_ids.data()));
  uint32_t flags= PA_TYPE_CPU | PA_AFF_STRONG;

  setprocset(&ps, POP_AND, P_PID, P_MYID, P_LWPID,
             static_cast<id_t>(thread_id));

  if (processor_affinity(&ps, &nids, ids, &flags) != 0)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("bind_to_cpus failed: processor_affinity failed with"
                    " errno %d - %-.192s", my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    return true;
  }
  return false;
}


bool unbind_thread()
{
  procset_t ps;
  uint32_t flags= PA_CLEAR;

  setprocset(&ps, POP_AND, P_PID, P_MYID, P_LWPID, my_thread_os_id());

  if (processor_affinity(&ps, nullptr, nullptr, &flags) != 0)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("unbind_thread failed: processor_affinity failed with"
                    " (error code %d - %-.192s", my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    return true;
  }
  return false;
}


bool unbind_thread(my_thread_os_id_t thread_id)
{
  procset_t ps;
  uint32_t flags= PA_CLEAR;

  setprocset(&ps, POP_AND, P_PID, P_MYID, P_LWPID, thread_id);

  if (processor_affinity(&ps, nullptr, nullptr, &flags) != 0)
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    sql_print_error("unbind_thread failed: processor_affinity failed with"
                    " error code %d - %-.192s", my_errno(),
                    my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
    return true;
  }
  return false;
}

int thread_priority()
{
  return getpriority(PRIO_PROCESS, my_thread_os_id());
}

int thread_priority(my_thread_os_id_t thread_id)
{
  DBUG_ENTER("thread_priority");
  DBUG_RETURN(getpriority(PRIO_PROCESS, thread_id));
}

bool set_thread_priority(int priority)
{
  return set_thread_priority(priority, my_thread_os_id());
}


bool set_thread_priority(int priority, my_thread_os_id_t thread_id)
{
  DBUG_ENTER("set_thread_priority");
  // Setting thread priority on solaris is not supported.
  DBUG_RETURN(false);
}


uint32_t num_vcpus()
{
  uint32_t num_vcpus= 0;

  pset_info(P_MYID, nullptr, &num_vcpus, nullptr);
  return num_vcpus;
}

bool can_thread_priority_be_set()
{
  return false;
}
}  // platform
}  // resourcegroups
