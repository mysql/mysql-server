/*
   Copyright (c) 2013, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#ifndef NDB_HW_H
#define NDB_HW_H

#include <ndb_global.h>

struct ndb_cpuinfo_data
{
  Uint32 cpu_no;
  Uint32 online;
  Uint32 core_id;
  Uint32 socket_id;
  Uint32 package_id;
  Uint32 l3_cache_id;
  Uint32 next_l3_cpu_map;
  Uint32 next_virt_l3_cpu_map;
  Uint32 prev_virt_l3_cpu_map;
  Uint32 virt_l3_used;
  Uint32 in_l3_cache_list;
  Uint32 next_cpu_map;
  Uint32 prev_cpu_map;
#ifdef _WIN32
  Uint32 group_number;
  Uint32 group_index;
#endif
};

struct ndb_cpudata
{
  Uint32 cpu_no;
  Uint32 online;
  Uint64 cs_user_us;
  Uint64 cs_nice_us;
  Uint64 cs_idle_us;
  Uint64 cs_sys_us;
  Uint64 cs_iowait_us;
  Uint64 cs_irq_us;
  Uint64 cs_sirq_us;
  Uint64 cs_steal_us;
  Uint64 cs_guest_us;
  Uint64 cs_guest_nice_us;
  Uint64 cs_unknown1_us;
  Uint64 cs_unknown2_us;
};

struct ndb_hwinfo
{
  /**
   * Number of Processors, Cores and Sockets
   * MHz of CPU and Model name of CPU.
   */
  Uint32 cpu_cnt_max;
  Uint32 cpu_cnt;
  Uint32 num_cpu_cores;
  Uint32 num_cpu_sockets;
  Uint32 num_cpu_per_core;
  Uint32 num_shared_l3_caches;
  Uint32 num_virt_l3_caches;
  Uint32 num_cpus_per_group;

  char cpu_model_name[128];

  /**
   * How many OS ticks do we have per second in this OS.
   */
  Uint32 os_HZ_per_second;

  /* Amount of memory in HW */
  Uint64 hw_memory_size;

  /**
   * CPU information available on Linux
   * One struct per CPU (cpu_cnt above).
   */
  Uint32 is_cpuinfo_available;
  Uint32 is_cpudata_available;
  Uint32 is_memory_info_available;
  Uint32 first_cpu_map;
  struct ndb_cpuinfo_data *cpu_info;
  struct ndb_cpudata *cpu_data;
};

extern "C"
{

  /**
   * @note ndb_init must be called prior to using these functions
   */

  /**
   * Get HW information
   * This provides information about number of CPUs, number of
   * CPU cores, number of CPU sockets, amount of memory and
   * other tidbits of our underlying HW. This information is
   * gathered at startup of the process.
   *
   * On some platforms it also provides CPU statistics. This
   * information isn't easily accessible on all platforms, so
   * we focus this on the most important ones that contain the
   * information. This information is gathered at the time of
   * this call.
   *
   * @note this call is not thread safe!
   */
  struct ndb_hwinfo * Ndb_GetHWInfo(bool get_data);

  /**
   * Prepare for creating the virtual L3 cache groups used to create
   * Round Robin groups.
   *
   * The specific CPU id is used in conjunction with old configs using
   * ThreadConfig and LockExecuteThreadToCPU. The set online variant is
   * used by automatic thread configuration.
   */
  void Ndb_SetVirtL3CPU(Uint32 cpu_id);
  void Ndb_SetOnlineAsVirtL3CPU();

  /**
   * Create simple CPU map that organises the locked CPU in an order
   * suitable for simple assignment that leads to Round Robin groups
   * formed in a suitable manner.
   *
   * The number returned is the number of Round Robin groups that is
   * decided based on the L3 cache groups.
   *
   * After calling this function one can use Ndb_GetFirstCPUInMap
   * and Ndb_GetNextCPUInMap to get the list of CPUs to assign to
   * threads.
   *
   * The list will be organised such that it returns the CPUs from one
   * CPU core at the time, the next CPU core is fetched from the next
   * virtual L3 cache group. Thus for LDM and Query thread instances
   * we will ensure that instances are close to each other. We create
   * Round Robin groups of LDM groups that are contained in the same
   * virtual L3 cache groups.
   */
  Uint32 Ndb_CreateCPUMap(Uint32 num_ldm_instances,
                          Uint32 num_query_threads_per_ldm);
  Uint32 Ndb_GetFirstCPUInMap();
  Uint32 Ndb_GetNextCPUInMap(Uint32 cpu_id);

  Uint32 Ndb_GetRRGroups(Uint32 ldm_threads);

  /**
   * Get the CPU id of all the CPUs in the CPU core of the
   * CPU issued.
   */
  void Ndb_GetCoreCPUIds(Uint32 cpu_id, Uint32 *cpu_ids, Uint32 &num_cpus);
}
#endif
