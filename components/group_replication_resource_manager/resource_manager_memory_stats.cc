/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#include "resource_manager_memory_stats.h"
#include "resource_manager.h"
#include "resource_manager_procfs.h"

#include <functional>

#ifdef _WIN32
#include <windows.h>  // GlobalMemoryStatusEx()
#else
#include <sys/resource.h>
#endif

#if defined(__APPLE__) || defined(__MACH__)
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

#if defined(__sun)
#include <kstat.h>
#include <unistd.h>
#endif

namespace gr_resource_manager {

bool m_sysinfo_error{false};

#ifdef _WIN32
/**
  Get current system memory info (Windows).
  @param[out] sys_memory_info  System memory info
  @return 0 success, 1 failure
*/
int get_system_memory_info(System_Memory_Info &sys_memory_info) {
  MEMORYSTATUSEX win_mem_info{};
  win_mem_info.dwLength = sizeof(MEMORYSTATUSEX);
  const bool result = GlobalMemoryStatusEx(&win_mem_info);
  if (!result) {
    if (!m_sysinfo_error) {
      m_sysinfo_error = true;
    }
    return 1;
  }
  sys_memory_info.m_total_bytes = win_mem_info.ullTotalPhys;
  sys_memory_info.m_avail_bytes = win_mem_info.ullAvailPhys;
  sys_memory_info.m_free_bytes = win_mem_info.ullAvailPhys;
  m_sysinfo_error = false;
  return 0;
}
#elif defined(__APPLE__) || defined(__MACH__)

uint64_t get_total_memory() {
  uint64_t total_memory;
  int mib[2] = {CTL_HW, HW_MEMSIZE};
  size_t length = sizeof(total_memory);
  if (sysctl(mib, 2, &total_memory, &length, NULL, 0) != 0) {
    m_sysinfo_error = true;
    return 0;
  }
  return total_memory;
}

uint64_t get_free_memory() {
  mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
  vm_statistics64_data_t vmstat;
  if (host_statistics64(mach_host_self(), HOST_VM_INFO,
                        reinterpret_cast<host_info64_t>(&vmstat),
                        &count) != KERN_SUCCESS) {
    m_sysinfo_error = true;
    return 0;
  }
  return vmstat.free_count * sysconf(_SC_PAGESIZE);
}

uint64_t get_available_memory() {
  mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
  vm_statistics64_data_t vmstat;
  if (host_statistics64(mach_host_self(), HOST_VM_INFO,
                        reinterpret_cast<host_info64_t>(&vmstat),
                        &count) != KERN_SUCCESS) {
    m_sysinfo_error = true;
    return 0;
  }
  return (vmstat.free_count + vmstat.inactive_count) * sysconf(_SC_PAGESIZE);
}

/**
  Get memory stats for the system and current process (Mac).
  @param[out] sys_memory_info  System memory info
  @return 0 success, 1 failure
*/
int get_system_memory_info(System_Memory_Info &sys_memory_info) {
  m_sysinfo_error = false;
  sys_memory_info.m_total_bytes = get_total_memory();
  sys_memory_info.m_avail_bytes = get_available_memory();
  sys_memory_info.m_free_bytes = get_free_memory();
  return m_sysinfo_error;
}

#elif defined(__sun)

/**
  Get memory stats for the system and current process (Solaris).
  @param[out] sys_memory_info  System memory info
  @return 0 success, 1 failure
*/
int get_system_memory_info(System_Memory_Info &sys_memory_info) {
  m_sysinfo_error = true;

  int pagesize = sysconf(_SC_PAGESIZE);

  /* get memory usage */
  kstat_ctl_t *g_kstat_ctl;
  kstat_t *kstat_syspage;
  kstat_named_t *kname_freemem;
  kstat_named_t *kname_physmem;
  kstat_named_t *kname_avail;
  int result_syspage;

  g_kstat_ctl = kstat_open();
  if (g_kstat_ctl == NULL) return m_sysinfo_error;

  char unix_str[] = "unix";
  char system_pages_str[] = "system_pages";
  kstat_syspage = kstat_lookup(g_kstat_ctl, unix_str, 0, system_pages_str);
  if (kstat_syspage == NULL) return m_sysinfo_error;

  result_syspage = kstat_read(g_kstat_ctl, kstat_syspage, 0);
  if (result_syspage == -1) return m_sysinfo_error;

  m_sysinfo_error = false;

  char freemem_str[] = "freemem";
  kname_freemem =
      (kstat_named_t *)kstat_data_lookup(kstat_syspage, freemem_str);
  if (kname_freemem != NULL)
    sys_memory_info.m_free_bytes = kname_freemem->value.ul;

  char physmem_str[] = "physmem";
  kname_physmem =
      (kstat_named_t *)kstat_data_lookup(kstat_syspage, physmem_str);
  if (kname_physmem != NULL)
    sys_memory_info.m_total_bytes = kname_physmem->value.ul;

  char availrmem_str[] = "availrmem";
  kname_avail =
      (kstat_named_t *)kstat_data_lookup(kstat_syspage, availrmem_str);
  if (kname_avail != NULL)
    sys_memory_info.m_avail_bytes = kname_avail->value.ul;

  sys_memory_info.m_free_bytes *= pagesize;
  sys_memory_info.m_total_bytes *= pagesize;
  sys_memory_info.m_avail_bytes *= pagesize;

  kstat_close(g_kstat_ctl);

  return m_sysinfo_error;
}

#else

/**
  Get memory stats for the system and current process (Linux).
  @param[out] sys_memory_info  System memory info
  @return 0 success, 1 failure
*/
int get_system_memory_info(System_Memory_Info &sys_memory_info) {
  Proc_Meminfo meminfo;
  if (ProcFS::proc_meminfo(meminfo) != 0) {
    return 1;
  }
  sys_memory_info = System_Memory_Info(meminfo);
  return 0;
}

#endif

int get_memory_info(Memory_Info &memory_info) {
  return get_system_memory_info(memory_info.sys) ? 1 : 0;
}
}  // namespace gr_resource_manager
