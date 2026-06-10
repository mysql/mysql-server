/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#ifndef GR_RESOURCE_MANAGER_PROCFS_INCLUDED
#define GR_RESOURCE_MANAGER_PROCFS_INCLUDED

#include <cstdint>
#include <string>
#include <vector>

namespace gr_resource_manager {
struct Proc_Meminfo {
  uint64_t m_mem_total_bytes{0};
  uint64_t m_mem_free_bytes{0};
  uint64_t m_mem_available_bytes{0};
  uint64_t m_buffers_bytes{0};
  uint64_t m_cached_bytes{0};
  uint64_t m_slab_bytes{0};
  uint64_t m_swap_total_bytes{0};
  uint64_t m_swap_free_bytes{0};
};

/* /proc filesystem processor */
class ProcFS {
 public:
  static int proc_meminfo(Proc_Meminfo &mem_info);

  static std::string path_proc_meminfo;
};

}  // namespace gr_resource_manager

#endif /* GR_RESOURCE_MANAGER_PROCFS_INCLUDED */
