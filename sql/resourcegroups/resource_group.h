/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
#ifndef RESOURCEGROUPS_RESOURCE_GROUP_H_
#define RESOURCEGROUPS_RESOURCE_GROUP_H_

#include "resource_group_sql_cmd.h"

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "sql/log.h"  // sql_print_warning
#include "sql/resourcegroups/thread_resource_control.h"

namespace resourcegroups {

class Resource_group_switch_handler;

/**
  Class that represents an abstraction of the Resource Group.
  It has generic attributes of Resource group name, type,
  active or inactive, a pointer to a Resource control object.
*/

class Resource_group {
 public:
  /**
    Construct a Resource_group object.

    @param name name of the Resource_group.
    @param type type of the Resource_group.
    @param enabled Check if Resource_group is enabled or not.
  */

  Resource_group(const std::string &name, const Type type, bool enabled)
      : m_name(name), m_type(type), m_enabled(enabled) {}

  const std::string &name() const { return m_name; }

  Type type() const { return m_type; }

  bool enabled() const { return m_enabled; }

  void set_type(Type type) { m_type = type; }

  void set_enabled(bool enabled) { m_enabled = enabled; }

  Thread_resource_control *controller() { return &m_thread_resource_control; }

  const Thread_resource_control *controller() const {
    return &m_thread_resource_control;
  }

  /**
    Check if resource group is associated with threads.

    @return true if some threads are mapped with this resource group
            else false.
  */

  bool is_bound_to_threads() {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex);
    return !m_pfs_thread_id_map.empty();
  }

  /**
    Is pfs thread id already exists in the m_pfs_thread_id_map map.

    @param pfs_thread_id  PFS thread id.

    @return true if thread id exists in the set else false.
  */

  bool is_pfs_thread_id_exists(const ulonglong pfs_thread_id) {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex);
    return m_pfs_thread_id_map.find(pfs_thread_id) != m_pfs_thread_id_map.end();
  }

  /**
    Add pfs_thread_id to the thread id map associated with this resource group.

    @param pfs_thread_id      PFS thread id.
    @param rg_switch_handler  Resource_group_switch_handler instance to
                              apply new resource group to a thread on
                              resource group switch from current to some
                              other resource group.
  */

  void add_pfs_thread_id(const ulonglong pfs_thread_id,
                         Resource_group_switch_handler *rg_switch_handler) {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex);
    (void)m_pfs_thread_id_map.insert({pfs_thread_id, rg_switch_handler});
  }

  /**
    Add pfs_thread_id if pfs_thread_id does not exists in the map of threads
    associated with this thread group. Otherwise, update resource group switch
    handler for the thread_id.

    @param pfs_thread_id      PFS thread id.
    @param rg_switch_handler  Resource_group_switch_handler instance to
                              apply new resource group to a thread on
                              resource group switch from current to some
                              other resource group.
  */

  void add_or_update_pfs_thread_id(
      const ulonglong pfs_thread_id,
      Resource_group_switch_handler *rg_switch_handler) {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex);
    (void)m_pfs_thread_id_map.insert_or_assign(pfs_thread_id,
                                               rg_switch_handler);
  }

  /**
    Add pfs_thread_id of a thread which is temporarily switched this resource
    group.

    @param pfs_thread_id      PFS thread id.
    @param rg_switch_handler  Resource_group_switch_handler instance to
                              apply new resource group to a thread on
                              resource group switch from current to some
                              other resource group.
  */

  void add_temporarily_switched_pfs_thread_id(
      const ulonglong pfs_thread_id,
      Resource_group_switch_handler *rg_switch_handler) {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex);
    (void)m_temporarily_switched_pfs_thread_id_map.insert(
        {pfs_thread_id, rg_switch_handler});
  }

  /**
    Get Resource_group_switch_handler instance for a thread identified by
    pfs_thread_id from the thread id maps.

    @param pfs_thread_id      PFS thread id.
  */

  Resource_group_switch_handler *resource_group_switch_handler(
      ulonglong pfs_thread_id) {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex);
    auto res = m_pfs_thread_id_map.find(pfs_thread_id);
    if (res == m_pfs_thread_id_map.end()) {
      res = m_temporarily_switched_pfs_thread_id_map.find(pfs_thread_id);
      if (res == m_temporarily_switched_pfs_thread_id_map.end()) return nullptr;
    }
    return res->second;
  }

  /**
    Remove the PFS thread id from the thread id maps.

    @param pfs_thread_id    Remove pfs thread id.
    @param skip_mutex_lock  Skip locking m_thread_id_maps_mutex.
  */

  void remove_pfs_thread_id(const ulonglong pfs_thread_id,
                            bool skip_mutex_lock = false) {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex, std::defer_lock);
    if (!skip_mutex_lock) lock.lock();
    (void)m_pfs_thread_id_map.erase(pfs_thread_id);
    (void)m_temporarily_switched_pfs_thread_id_map.erase(pfs_thread_id);
  }

  /**
    Clear the thread id maps associated with this resource group.
  */

  void clear() {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex);
    (void)m_pfs_thread_id_map.clear();
    (void)m_temporarily_switched_pfs_thread_id_map.clear();
  }

  /**
    Apply a control function on threads *associated* with this resource group.

    @param control_func pointer to Control function.
  */

  void apply_control_func(
      std::function<void(ulonglong, Resource_group_switch_handler *)>
          control_func) {
    std::unique_lock<std::mutex> lock(m_thread_id_maps_mutex);
    for (auto &[pfs_thread_id, resource_group_switch_handler] :
         m_pfs_thread_id_map)
      control_func(pfs_thread_id, resource_group_switch_handler);
  }

  ~Resource_group() = default;

 private:
  /**
    Name of the resource group.
  */
  std::string m_name;

  /**
    Type whether it is user or system resource group.
  */
  Type m_type;

  /**
    bool flag whether resource is enabled or disabled.
  */
  bool m_enabled;

  /**
    Thread resource controller object.
  */
  Thread_resource_control m_thread_resource_control;

  /*
    Contains threads associated with this resource group and resource group
    switch handler instance pair.

    Resource group switch handler instance is used to apply resource group to
    a thread when resource group fo a thread is switched from this to some other
    resource group.
  */
  std::map<ulonglong, Resource_group_switch_handler *> m_pfs_thread_id_map;

  /*
    Contains threads temporarily switched to this resource group and resource
    group switch handler instance pair.
  */
  std::map<ulonglong, Resource_group_switch_handler *>
      m_temporarily_switched_pfs_thread_id_map;

  /**
    Mutex protecting the pfs thread id maps.
  */
  std::mutex m_thread_id_maps_mutex;

  /**
    Disable copy construction and assignment.
  */
  Resource_group(const Resource_group &) = delete;
  void operator=(const Resource_group &) = delete;
};

/**
  Class used to apply new resource group to a thread on resource group switch.
*/
class Resource_group_switch_handler {
 public:
  virtual ~Resource_group_switch_handler() {}
  /**
    Apply thread resource control to thread identified by thread os id,

    @param      new_rg                      New resource group to apply to the
                                            thread.
    @param      thread_os_id                OS thread id.
    @param[out] is_rg_applied_to_thread     Set to "true" if resource group is
                                            applied to a thread.

    @retval false   Success.
    @retval true    Failure.
 */
  virtual bool apply(Resource_group *new_rg, my_thread_os_id_t thread_os_id,
                     bool *is_rg_applied_to_thread);
};

/**
  Default resource group switch handler instance.
*/
extern Resource_group_switch_handler default_rg_switch_handler;
}  // namespace resourcegroups
#endif  // RESOURCEGROUPS_RESOURCE_GROUP_H_
