/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#ifdef _WIN32
#include <Windows.h>
#include <psapi.h>
#include <string>
#else
#include <libgen.h>  // basename()
#include <unistd.h>  // sysconf()
#endif
#include <sys/stat.h>  // stat()
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>

#include "resource_manager_procfs.h"

namespace gr_resource_manager {

/**
  Line processor for values indexed by name.
*/
template <typename T>
struct value_by_name {
  value_by_name(const char *tag, std::function<T(const T &)> convert_fn)
      : name{tag}, found{false}, convert{convert_fn} {}
  std::string name;
  bool found;
  std::function<T(const T &)> convert;

  // Read the value from a named field, for example: "MemTotal: 790780796 kB"
  bool get_value(const std::string &line, T &value) {
    // Ignore if field name does not match or was previously matched
    if (found || line.compare(0, name.size(), name) != 0) {
      return false;
    }
    std::istringstream line_ss(line);
    std::string field_name;
    line_ss >> field_name;
    line_ss >> value;
    if (convert) {
      value = convert(value);
    }
    found = true;
    return true;
  }
};

using value_name_u64 = value_by_name<uint64_t>;

/**
  Reader base class.
*/
template <typename T>
struct Proc_Reader {
  Proc_Reader() = default;
  virtual ~Proc_Reader() = default;

  virtual bool get_field(std::string &line, T &stat) = 0;

  static uint64_t kb_to_bytes(const uint64_t &val) { return val * 1024; }
};

/**
  Reader for /proc/meminfo.
  See Proc_Meminfo for file description and format.
*/
struct Proc_Meminfo_Reader : Proc_Reader<Proc_Meminfo> {
  Proc_Meminfo_Reader() = default;
  /* Find a named value within a string, get the value.
     @param[in] line  String with name and value
     @param[out] info  Struct of values
     @retval true if all fields found, false to get next line
  */
  bool get_field(std::string &line, Proc_Meminfo &info) override {
    if (mem_total.get_value(line, info.m_mem_total_bytes) ||
        mem_free.get_value(line, info.m_mem_free_bytes) ||
        mem_available.get_value(line, info.m_mem_available_bytes) ||
        buffers.get_value(line, info.m_buffers_bytes) ||
        cached.get_value(line, info.m_cached_bytes) ||
        swap_total.get_value(line, info.m_swap_total_bytes) ||
        swap_free.get_value(line, info.m_swap_free_bytes) ||
        slab.get_value(line, info.m_slab_bytes)) {
      num_found++;
    }
    if (num_found == num_fields) return true;  // all fields found
    return false;                              // get next line
  }

 private:
  value_name_u64 mem_total{"MemTotal:", kb_to_bytes};
  value_name_u64 mem_free{"MemFree:", kb_to_bytes};
  value_name_u64 mem_available{"MemAvailable:", kb_to_bytes};
  value_name_u64 buffers{"Buffers:", kb_to_bytes};
  value_name_u64 cached{"Cached:", kb_to_bytes};
  value_name_u64 swap_total{"SwapTotal:", kb_to_bytes};
  value_name_u64 swap_free{"SwapFree:", kb_to_bytes};
  value_name_u64 slab{"Slab:", kb_to_bytes};
  const uint8_t num_fields{8};
  uint8_t num_found{0};
};

constexpr std::string_view k_path_proc_meminfo{"/proc/meminfo"};
std::string ProcFS::path_proc_meminfo{k_path_proc_meminfo};

/**
  Get system memory stats.
  @param[out] mem_info  System memory stats
  @retval 0 Success, 1 file error
*/
int ProcFS::proc_meminfo(Proc_Meminfo &mem_info) {
  std::ifstream file_proc_meminfo(path_proc_meminfo);
  if (!file_proc_meminfo.is_open()) return 1;

  std::string line{};
  Proc_Meminfo_Reader reader;

  while (getline(file_proc_meminfo, line)) {
    if (reader.get_field(line, mem_info)) break;
  }
  file_proc_meminfo.close();
  return 0;
}
}  // namespace gr_resource_manager
