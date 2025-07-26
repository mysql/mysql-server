/*****************************************************************************
Copyright (c) 2022, Huawei Technologies Co., Ltd. All Rights Reserved.
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.
This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.
*****************************************************************************/

#include "sql/sched_affinity_manager.h"

#include <cmath>

#include <sys/syscall.h>

#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "sql/mysqld.h"

namespace sched_affinity {
const std::vector<Thread_type> thread_types = {
    Thread_type::FOREGROUND,         Thread_type::LOG_WRITER,
    Thread_type::LOG_FLUSHER,        Thread_type::LOG_WRITE_NOTIFIER,
    Thread_type::LOG_FLUSH_NOTIFIER, Thread_type::LOG_CHECKPOINTER,
    Thread_type::PURGE_COORDINATOR};

const std::map<Thread_type, std::string> thread_type_names = {
    {Thread_type::FOREGROUND, "foreground"},
    {Thread_type::LOG_WRITER, "log_writer"},
    {Thread_type::LOG_FLUSHER, "log_flusher"},
    {Thread_type::LOG_WRITE_NOTIFIER, "log_write_notifier"},
    {Thread_type::LOG_FLUSH_NOTIFIER, "log_flush_notifier"},
    {Thread_type::LOG_CHECKPOINTER, "log_checkpointer"},
    {Thread_type::PURGE_COORDINATOR, "purge_coordinator"},
    {Thread_type::UNDEFINED, "undefined"}};
}  // namespace sched_affinity


#ifdef HAVE_LIBNUMA
namespace sched_affinity {
class Lock_guard {
 public:
  explicit Lock_guard(mysql_mutex_t &mutex) {
    m_mutex = &mutex;
    mysql_mutex_lock(m_mutex);
  }
  Lock_guard(const Lock_guard &) = delete;
  Lock_guard &operator=(const Lock_guard &) = delete;
  ~Lock_guard() { mysql_mutex_unlock(m_mutex); }

 private:
  mysql_mutex_t *m_mutex;
};


Sched_affinity_manager_numa::Sched_affinity_manager_numa()
    : Sched_affinity_manager(),
      m_total_cpu_num(0),
      m_total_node_num(0),
      m_cpu_num_per_node(0),
      m_numa_aware(false),
      m_root_pid(0),
      m_is_fallback(false) {
  mysql_mutex_init(key_sched_affinity_mutex, &m_mutex, nullptr);
}

Sched_affinity_manager_numa::~Sched_affinity_manager_numa() {
  mysql_mutex_destroy(&m_mutex);
}


bool Sched_affinity_manager_numa::init(
    const std::map<Thread_type, const char *> &sched_affinity_parameter,
    bool numa_aware) {
  m_total_cpu_num = numa_num_configured_cpus();
  m_total_node_num = numa_num_configured_nodes();
  m_cpu_num_per_node = m_total_cpu_num / m_total_node_num;
  m_numa_aware = numa_aware;
  m_root_pid = gettid();

  m_thread_bitmask.clear();
  m_sched_affinity_groups.clear();
  m_thread_pid.clear();
  for (const auto &thread_type : thread_types) {
    if (sched_affinity_parameter.find(thread_type) ==
        sched_affinity_parameter.end()) {
      continue;
    }
    m_thread_pid[thread_type] = std::set<pid_t>();
    auto cpu_string = sched_affinity_parameter.at(thread_type);
    if (!init_sched_affinity_info(
            cpu_string == nullptr ? std::string("") : std::string(cpu_string),
            m_thread_bitmask[thread_type])) {
      return false;
    }
    if (is_thread_sched_enabled(thread_type) &&
        !init_sched_affinity_group(
            m_thread_bitmask[thread_type],
            m_numa_aware && thread_type == Thread_type::FOREGROUND,
            m_sched_affinity_groups[thread_type])) {
      return false;
    }
  }

  return true;
}

void Sched_affinity_manager_numa::fallback() {
  if (!m_is_fallback) {
    m_is_fallback = true;
    m_fallback_delegate.reset(new Sched_affinity_manager_dummy());
    LogErr(ERROR_LEVEL, ER_SET_FALLBACK_MODE);
  }
}

bool Sched_affinity_manager_numa::init_sched_affinity_info(
    const std::string &cpu_string, Bitmask_ptr &group_bitmask) {
  group_bitmask.reset();
  if (cpu_string.empty()) {
    return true;
  }
  std::pair<std::string, bool> normalized_result =
      normalize_cpu_string(cpu_string);
  if (normalized_result.second == false) {
    LogErr(ERROR_LEVEL, ER_CANT_PARSE_CPU_STRING, cpu_string.c_str());
    return false;
  }
  group_bitmask.reset(numa_parse_cpustring(normalized_result.first.c_str()));
  if (!group_bitmask) {
    LogErr(ERROR_LEVEL, ER_CANT_PARSE_CPU_STRING, cpu_string.c_str());
    return false;
  }
  return true;
}

bool Sched_affinity_manager_numa::init_sched_affinity_group(
    const Bitmask_ptr &group_bitmask, const bool numa_aware,
    std::vector<Sched_affinity_group> &sched_affinity_group) {
  if (numa_aware) {
    sched_affinity_group.resize(m_total_node_num);
    for (auto node_id = 0; node_id < m_total_node_num; ++node_id) {
      sched_affinity_group[node_id].avail_cpu_num = 0;
      sched_affinity_group[node_id].avail_cpu_mask =
          Bitmask_ptr(numa_allocate_cpumask());
      sched_affinity_group[node_id].assigned_thread_num = 0;
      for (auto cpu_id = m_cpu_num_per_node * node_id;
           cpu_id < m_cpu_num_per_node * (node_id + 1); ++cpu_id) {
        if (numa_bitmask_isbitset(group_bitmask.get(), cpu_id)) {
          numa_bitmask_setbit(
              sched_affinity_group[node_id].avail_cpu_mask.get(), cpu_id);
          ++sched_affinity_group[node_id].avail_cpu_num;
        }
      }
    }
  } else {
    sched_affinity_group.resize(1);
    sched_affinity_group[0].avail_cpu_num = 0;
    sched_affinity_group[0].avail_cpu_mask =
        Bitmask_ptr(numa_allocate_cpumask());
    copy_bitmask_to_bitmask(group_bitmask.get(),
                            sched_affinity_group[0].avail_cpu_mask.get());
    sched_affinity_group[0].assigned_thread_num = 0;
    for (auto cpu_id = 0; cpu_id < m_total_cpu_num; ++cpu_id) {
      if (numa_bitmask_isbitset(group_bitmask.get(), cpu_id)) {
        ++sched_affinity_group[0].avail_cpu_num;
      }
    }
  }
  return true;
}


bool Sched_affinity_manager_numa::rebalance_group(
    const char *cpu_string, const Thread_type thread_type) {
  const Lock_guard lock(m_mutex);
  if (m_is_fallback) {
    LogErr(ERROR_LEVEL, ER_FALLBACK_DELEGATE_SCHED_AFFINITY_MANAGER_IS_CALLED);
    return m_fallback_delegate->rebalance_group(cpu_string, thread_type);
  }
  const bool is_previous_sched_enabled = is_thread_sched_enabled(thread_type);
  std::vector<std::set<pid_t>> group_thread;
  if (!reset_sched_affinity_info(cpu_string, thread_type, group_thread)) {
    fallback();
    return false;
  }
  if (!is_thread_sched_enabled(thread_type) && !is_previous_sched_enabled) {
    return true;
  }
  if (!is_thread_sched_enabled(thread_type) && is_previous_sched_enabled) {
    Bitmask_ptr root_process_bitmask(numa_allocate_cpumask());
    if (numa_sched_getaffinity(m_root_pid, root_process_bitmask.get()) < 0) {
      fallback();
      return false;
    }
    for (const auto tid : m_thread_pid[thread_type]) {
      m_pid_group_id.erase(tid);
      if (numa_sched_setaffinity(tid, root_process_bitmask.get()) < 0) {
        fallback();
        return false;
      }
    }
    return true;
  }
  if (is_thread_sched_enabled(thread_type) && !is_previous_sched_enabled) {
    for (const auto tid : m_thread_pid[thread_type]) {
      if (!bind_to_group(tid)) {
        fallback();
        return false;
      }
    }
    return true;
  }
  auto &sched_affinity_group = m_sched_affinity_groups[thread_type];
  std::vector<int> migrate_thread_num;
  migrate_thread_num.resize(sched_affinity_group.size());
  count_migrate_thread_num(group_thread, sched_affinity_group,
                           migrate_thread_num);
  if (!migrate_thread_and_setaffinity(group_thread, sched_affinity_group,
                                      migrate_thread_num)) {
    fallback();
    return false;
  }
  return true;
}

bool Sched_affinity_manager_numa::reset_sched_affinity_info(
    const char *cpu_string, const Thread_type &thread_type,
    std::vector<std::set<pid_t>> &group_thread) {
  bool numa_aware = m_numa_aware && thread_type == Thread_type::FOREGROUND;
  group_thread.resize(numa_aware ? m_total_node_num : 1, std::set<pid_t>());
  for (const auto tid : m_thread_pid[thread_type]) {
    const auto group_index = m_pid_group_id[tid];
    group_thread[group_index].insert(tid);
  }
  if (!init_sched_affinity_info(
          cpu_string == nullptr ? std::string("") : std::string(cpu_string),
          m_thread_bitmask[thread_type])) {
    return false;
  }
  if (is_thread_sched_enabled(thread_type) &&
      !init_sched_affinity_group(m_thread_bitmask[thread_type], numa_aware,
                                 m_sched_affinity_groups[thread_type])) {
    return false;
  }
  return true;
}

void Sched_affinity_manager_numa::count_migrate_thread_num(
    const std::vector<std::set<pid_t>> &group_thread,
    std::vector<Sched_affinity_group> &sched_affinity_group,
    std::vector<int> &migrate_thread_num) {
  int total_thread_num = 0;
  int total_avail_cpu_num = 0;
  for (auto i = 0u; i < sched_affinity_group.size(); ++i) {
    total_thread_num += group_thread[i].size();
    total_avail_cpu_num += sched_affinity_group[i].avail_cpu_num;
  }
  if (total_avail_cpu_num == 0) {
    for (auto i = 0u; i < sched_affinity_group.size(); ++i) {
      sched_affinity_group[i].assigned_thread_num = 0;
      migrate_thread_num[i] = 0;
    }
    return;
  }
  for (auto i = 0u; i < sched_affinity_group.size(); ++i) {
    sched_affinity_group[i].assigned_thread_num =
        std::ceil(static_cast<double>(total_thread_num *
                                      sched_affinity_group[i].avail_cpu_num) /
                  total_avail_cpu_num);
    migrate_thread_num[i] =
        sched_affinity_group[i].assigned_thread_num - group_thread[i].size();
  }
}

bool Sched_affinity_manager_numa::migrate_thread_and_setaffinity(
    const std::vector<std::set<pid_t>> &group_thread,
    const std::vector<Sched_affinity_group> &sched_affinity_group,
    std::vector<int> &migrate_thread_num) {
  for (auto i = 0u; i < group_thread.size(); ++i) {
    for (auto tid : group_thread[i]) {
      if (sched_affinity_group[i].avail_cpu_num != 0 &&
          numa_sched_setaffinity(
              tid, sched_affinity_group[i].avail_cpu_mask.get()) < 0) {
        return false;
      }
    }
  }
  for (auto i = 0u; i < group_thread.size(); ++i) {
    if (migrate_thread_num[i] >= 0) {
      continue;
    }
    std::set<pid_t>::iterator it = group_thread[i].begin();
    for (auto j = 0u; j < group_thread.size(); ++j) {
      while (migrate_thread_num[j] > 0 && migrate_thread_num[i] < 0 &&
             it != group_thread[i].end()) {
        m_pid_group_id[*it] = j;
        if (numa_sched_setaffinity(
                *it, sched_affinity_group[j].avail_cpu_mask.get()) < 0) {
          return false;
        }
        --migrate_thread_num[j];
        ++migrate_thread_num[i];
        ++it;
      }
    }
  }
  return true;
}

bool Sched_affinity_manager_numa::is_thread_sched_enabled(
    const Thread_type thread_type) {
  auto it = m_thread_bitmask.find(thread_type);
  return (it != m_thread_bitmask.end() && it->second) ? true : false;
}

bool Sched_affinity_manager_numa::register_thread(const Thread_type thread_type,
                                                  const pid_t pid) {
  const Lock_guard lock(m_mutex);

  if (m_is_fallback) {
    LogErr(ERROR_LEVEL, ER_FALLBACK_DELEGATE_SCHED_AFFINITY_MANAGER_IS_CALLED);
    return m_fallback_delegate->register_thread(thread_type, pid);
  }

  m_thread_pid[thread_type].insert(pid);
  if (!bind_to_group(pid)) {
    LogErr(ERROR_LEVEL, ER_CANNOT_SET_THREAD_SCHED_AFFINIFY,
           thread_type_names.at(thread_type).c_str());
    fallback();
    return false;
  }
  return true;
}

bool Sched_affinity_manager_numa::unregister_thread(const pid_t pid) {
  const Lock_guard lock(m_mutex);

  if (m_is_fallback) {
    LogErr(ERROR_LEVEL, ER_FALLBACK_DELEGATE_SCHED_AFFINITY_MANAGER_IS_CALLED);
    return m_fallback_delegate->unregister_thread(pid);
  }

  auto thread_type = get_thread_type_by_pid(pid);
  if (thread_type == Thread_type::UNDEFINED) {
    return false;
  }

  if (!unbind_from_group(pid)) {
    LogErr(ERROR_LEVEL, ER_CANNOT_UNSET_THREAD_SCHED_AFFINIFY,
           thread_type_names.at(thread_type).c_str());
    fallback();
    return false;
  }
  m_thread_pid[thread_type].erase(pid);
  return true;
}

Thread_type Sched_affinity_manager_numa::get_thread_type_by_pid(
    const pid_t pid) {
  for (const auto &thread_pid : m_thread_pid) {
    if (thread_pid.second.find(pid) != thread_pid.second.end()) {
      return thread_pid.first;
    }
  }
  return Thread_type::UNDEFINED;
}

bool Sched_affinity_manager_numa::bind_to_group(const pid_t pid) {
  auto thread_type = get_thread_type_by_pid(pid);
  if (thread_type == Thread_type::UNDEFINED) {
    return false;
  }
  if (!is_thread_sched_enabled(thread_type)) {
    return true;
  }
  auto &sched_affinity_group = m_sched_affinity_groups[thread_type];
  const int INVALID_INDEX = -1;
  auto best_index = INVALID_INDEX;
  for (auto i = 0u; i < sched_affinity_group.size(); ++i) {
    if (sched_affinity_group[i].avail_cpu_num == 0) {
      continue;
    }
    if (best_index == INVALID_INDEX ||
        sched_affinity_group[i].assigned_thread_num *
                sched_affinity_group[best_index].avail_cpu_num <
            sched_affinity_group[best_index].assigned_thread_num *
                sched_affinity_group[i].avail_cpu_num) {
      best_index = i;
    }
  }

  if (best_index == INVALID_INDEX) {
    return false;
  }
  auto ret = numa_sched_setaffinity(
      pid, sched_affinity_group[best_index].avail_cpu_mask.get());
  if (ret == 0) {
    ++sched_affinity_group[best_index].assigned_thread_num;
    m_pid_group_id[pid] = best_index;
    return true;
  }
  return false;
}


bool Sched_affinity_manager_numa::unbind_from_group(const pid_t pid) {
  auto thread_type = get_thread_type_by_pid(pid);
  if (thread_type == Thread_type::UNDEFINED) {
    return false;
  }
  if (!is_thread_sched_enabled(thread_type)) {
    return true;
  }
  auto &sched_affinity_group = m_sched_affinity_groups[thread_type];
  auto index = m_pid_group_id.find(pid);
  if (index == m_pid_group_id.end() ||
      index->second >= static_cast<int>(sched_affinity_group.size())) {
    return false;
  }
  --sched_affinity_group[index->second].assigned_thread_num;
  m_pid_group_id.erase(index);

  return copy_affinity(pid, m_root_pid);
}

bool Sched_affinity_manager_numa::copy_affinity(pid_t from, pid_t to) {
  Bitmask_ptr to_bitmask(numa_allocate_cpumask());
  if (numa_sched_getaffinity(to, to_bitmask.get()) < 0) {
    return false;
  }
  if (numa_sched_setaffinity(from, to_bitmask.get()) < 0) {
    return false;
  }
  return true;
}

std::string Sched_affinity_manager_numa::take_group_snapshot() {
  const Lock_guard lock(m_mutex);

  if (m_is_fallback) {
    LogErr(ERROR_LEVEL, ER_FALLBACK_DELEGATE_SCHED_AFFINITY_MANAGER_IS_CALLED);
    return m_fallback_delegate->take_group_snapshot();
  }

  std::string group_snapshot = "";
  for (const auto &thread_type : thread_types) {
    if (!is_thread_sched_enabled(thread_type)) {
      continue;
    }
    group_snapshot += thread_type_names.at(thread_type) + ": ";
    for (const auto &sched_affinity_group :
         m_sched_affinity_groups[thread_type]) {
      group_snapshot +=
          (std::to_string(sched_affinity_group.assigned_thread_num) +
           std::string("/") +
           std::to_string(sched_affinity_group.avail_cpu_num) +
           std::string("; "));
    }
  }
  return group_snapshot;
}

int Sched_affinity_manager_numa::get_total_node_number() {
  return m_total_node_num;
}

int Sched_affinity_manager_numa::get_cpu_number_per_node() {
  return m_cpu_num_per_node;
}

bool Sched_affinity_manager_numa::check_cpu_string(
    const std::string &cpu_string) {
  auto ret = normalize_cpu_string(cpu_string);
  if (!ret.second) {
    return false;
  }
  Bitmask_ptr bitmask(numa_parse_cpustring(ret.first.c_str()));
  return bitmask.get() != nullptr;
}

std::pair<std::string, bool> Sched_affinity_manager_numa::normalize_cpu_string(
    const std::string &cpu_string) {
  std::string normalized_cpu_string = "";
  bool invalid_cpu_string = false;
  const int INVALID_CORE_ID = -1;
  int core_id = INVALID_CORE_ID;
  for (auto c : cpu_string) {
    switch (c) {
      case ' ':
        break;
      case '-':
      case ',':
        if (core_id == INVALID_CORE_ID) {
          invalid_cpu_string = true;
        } else {
          normalized_cpu_string += std::to_string(core_id);
          normalized_cpu_string += c;
          core_id = INVALID_CORE_ID;
        }
        break;
      case '0' ... '9':
        if (core_id == INVALID_CORE_ID) {
          core_id = (c - '0');
        } else {
          core_id = core_id * 10 + (c - '0');
        }
        break;
      default:
        invalid_cpu_string = true;
        break;
    }
    if (invalid_cpu_string) {
      break;
    }
  }
  if (core_id != INVALID_CORE_ID) {
    normalized_cpu_string += std::to_string(core_id);
  }
  if (!normalized_cpu_string.empty() &&
      (*normalized_cpu_string.rbegin() == '-' ||
       *normalized_cpu_string.rbegin() == ',')) {
    invalid_cpu_string = true;
  }
  if (invalid_cpu_string) {
    return std::make_pair(std::string(), false);
  }
  return std::make_pair(normalized_cpu_string, true);
}

bool Sched_affinity_manager_numa::update_numa_aware(bool numa_aware) {
  const Lock_guard lock(m_mutex);
  if (m_is_fallback) {
    LogErr(ERROR_LEVEL, ER_FALLBACK_DELEGATE_SCHED_AFFINITY_MANAGER_IS_CALLED);
    return m_fallback_delegate->update_numa_aware(numa_aware);
  }
  if (m_numa_aware == numa_aware) {
    return true;
  }
  std::vector<pid_t> pending_pids;
  pending_pids.resize(m_pid_group_id.size());
  std::transform(m_pid_group_id.begin(), m_pid_group_id.end(),
                 pending_pids.begin(),
                 [](auto &pid_group_id) { return pid_group_id.first; });
  for (const auto &pending_pid : pending_pids) {
    if (!unbind_from_group(pending_pid)) {
      LogErr(ERROR_LEVEL, ER_CANNOT_UNSET_THREAD_SCHED_AFFINIFY,
             thread_type_names.at(get_thread_type_by_pid(pending_pid)).c_str());
      fallback();
      return false;
    }
  }
  m_numa_aware = numa_aware;
  for (const auto &thread_type : thread_types) {
    if (is_thread_sched_enabled(thread_type) &&
        !init_sched_affinity_group(
            m_thread_bitmask[thread_type],
            m_numa_aware && thread_type == Thread_type::FOREGROUND,
            m_sched_affinity_groups[thread_type])) {
      fallback();
      return false;
    }
  }
  for (const auto &pending_pid : pending_pids) {
    if (!bind_to_group(pending_pid)) {
      LogErr(ERROR_LEVEL, ER_CANNOT_SET_THREAD_SCHED_AFFINIFY,
             thread_type_names.at(get_thread_type_by_pid(pending_pid)).c_str());
      fallback();
      return false;
    }
  }
  return true;
}
}  // namespace sched_affinity
#endif /* HAVE_LIBNUMA */

namespace sched_affinity {
static Sched_affinity_manager *sched_affinity_manager = nullptr;
Sched_affinity_manager *Sched_affinity_manager::create_instance(
    const std::map<Thread_type, const char *> &sched_affinity_parameter,
    bool numa_aware) {
  Sched_affinity_manager::free_instance();
#ifdef HAVE_LIBNUMA
  if (numa_available() == -1) {
    LogErr(WARNING_LEVEL, ER_NUMA_AVAILABLE_TEST_FAIL);
    LogErr(INFORMATION_LEVEL, ER_USE_DUMMY_SCHED_AFFINITY_MANAGER);
    sched_affinity_manager = new Sched_affinity_manager_dummy();
  } else {
    sched_affinity_manager = new Sched_affinity_manager_numa();
  }
#else
  LogErr(WARNING_LEVEL, ER_LIBNUMA_TEST_FAIL);
  LogErr(INFORMATION_LEVEL, ER_USE_DUMMY_SCHED_AFFINITY_MANAGER);
  sched_affinity_manager = new Sched_affinity_manager_dummy();
#endif /* HAVE_LIBNUMA */
  if (!sched_affinity_manager->init(sched_affinity_parameter, numa_aware)) {
    return nullptr;
  }
  return sched_affinity_manager;
}

Sched_affinity_manager *Sched_affinity_manager::get_instance() {
  return sched_affinity_manager;
}

void Sched_affinity_manager::free_instance() {
  if (sched_affinity_manager != nullptr) {
    delete sched_affinity_manager;
    sched_affinity_manager = nullptr;
  }
}

pid_t gettid() { return static_cast<pid_t>(syscall(SYS_gettid)); }
}  // namespace sched_affinity
