/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#ifndef GR_RESOURCE_MANAGER_MEMORY_STATS
#define GR_RESOURCE_MANAGER_MEMORY_STATS

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include "resource_manager_procfs.h"

namespace gr_resource_manager {
struct System_Memory_Info {
  System_Memory_Info() = default;

  explicit System_Memory_Info(Proc_Meminfo &meminfo)
      : m_total_bytes{meminfo.m_mem_total_bytes},
        m_free_bytes{meminfo.m_mem_free_bytes},
        m_avail_bytes{meminfo.m_mem_available_bytes} {}

  time_t m_timestamp{0};
  uint64_t m_total_bytes{0};
  uint64_t m_free_bytes{0};
  uint64_t m_avail_bytes{0};
};

struct Memory_Info {
  Memory_Info() = default;
  System_Memory_Info sys;
};
int get_memory_info(Memory_Info &memory_info);
}  // namespace gr_resource_manager
#endif  // GR_RESOURCE_MANAGER_MEMORY_STATS
