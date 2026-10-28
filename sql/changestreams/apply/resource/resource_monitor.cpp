// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/changestreams/apply/resource/resource_monitor.h"
#include <cassert>
#include <thread>
#include "mysql/concurrency/stage.h"
#include "mysql/scheduler/constants.h"
#include "sql/changestreams/apply/context/tune.h"

namespace mysql::csa {

Resource_monitor::Instances_map Resource_monitor::m_instances{};

Resource_instance_monitor &Resource_monitor::get(std::size_t instance_id) {
  assert(instance_id < scheduler::Constants::max_instances);
  return m_instances[instance_id];
}

void Resource_instance_monitor::register_resource(const std::string &name,
                                                  std::size_t limit,
                                                  PSI_stage_info *stage) {
  if (!tune::enabled_resource_tracking) {
    return;
  }
  auto entry = std::make_unique<Resource_entry>();
  entry->m_available.store(limit);
  entry->m_limit.store(limit);
  entry->m_stage = stage;
  m_resources[name] = std::move(entry);
}

Resource_entry *Resource_instance_monitor::get_resource_entry(
    const std::string &name) {
  if (!tune::enabled_resource_tracking) {
    return nullptr;
  }
  auto it = m_resources.find(name);
  return it == m_resources.end() ? nullptr : it->second.get();
}

bool Resource_instance_monitor::try_lock_resource(Resource_entry &resource,
                                                  std::size_t amount) {
  return try_lock_resource_internal(resource, amount);
}

bool Resource_instance_monitor::try_lock_resource_soft(Resource_entry &resource,
                                                       std::size_t amount) {
  const auto requested = static_cast<long long>(amount);
  long long prev = resource.m_available.load();
  // Retry only while still under the limit; a failed exchange reloads prev.
  // Keep this rule in step with would_lock_soft() below.
  while (prev > 0) {
    if (resource.m_available.compare_exchange_weak(prev, prev - requested))
      return true;
  }
  return false;
}

bool Resource_instance_monitor::would_lock_soft(const Resource_entry &resource,
                                                std::size_t /*amount*/) {
  // The rule of try_lock_resource_soft() above, without the reservation: the
  // size of the request never matters, only whether the resource is still
  // under its limit.
  return resource.m_available.load() > 0;
}

void Resource_instance_monitor::release_resource(Resource_entry &resource,
                                                 std::size_t amount) {
  resource.m_available.fetch_add(static_cast<long long>(amount));
}

bool Resource_instance_monitor::lock_resource(Resource_entry &entry,
                                              std::size_t amount) {
  // this check avoids setting THD stage - performance boost
  if (try_lock_resource_internal(entry, amount)) {
    return true;
  }
  // enter waiting, set THD stage for observability
  if (entry.m_stage != nullptr) {
    mysql::concurrency::set_thd_stage(current_thd, *(entry.m_stage));
  }
  // checks allow killing this THD
  while (current_thd == nullptr || !current_thd->is_killed()) {
    if (try_lock_resource_internal(entry, amount)) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

Locked_resource Resource_instance_monitor::acquire_resource(
    const std::string &name, std::size_t amount) {
  if (!tune::enabled_resource_tracking) {
    return {};
  }
  auto it = m_resources.find(name);
  assert(it != m_resources.end());
  auto &entry = it->second;
  return Locked_resource(entry.get(), amount, lock_resource(*entry, amount));
}

bool Resource_instance_monitor::lock_resource(const std::string &name,
                                              std::size_t amount) {
  if (!tune::enabled_resource_tracking) {
    return true;
  }
  auto it = m_resources.find(name);
  assert(it != m_resources.end());
  auto &entry = it->second;
  return lock_resource(*entry, amount);
}

bool Resource_instance_monitor::try_lock_resource_internal(
    Resource_entry &resource_entry, std::size_t amount) {
  long long requested = static_cast<long long>(amount);
  long long limit = resource_entry.m_limit.load();
  long long prev = resource_entry.m_available.fetch_sub(requested);
  // Keep this rule in step with would_lock() below.
  if (prev >= requested || prev == limit) {
    return true;
  }
  resource_entry.m_available.fetch_add(requested);
  return false;
}

bool Resource_instance_monitor::would_lock(const Resource_entry &resource,
                                           std::size_t amount) {
  // The rule of try_lock_resource_internal() above, without the reservation.
  // The limit is read only when the request does not fit, so a request that
  // fits costs one atomic load.
  const auto requested = static_cast<long long>(amount);
  const long long available = resource.m_available.load();
  return available >= requested || available == resource.m_limit.load();
}

void Resource_instance_monitor::release_resource(const std::string &name,
                                                 std::size_t amount) {
  if (!tune::enabled_resource_tracking) {
    return;
  }
  auto it = m_resources.find(name);
  assert(it != m_resources.end());
  auto &entry = it->second;
  release_resource(*entry, amount);
}

Locked_resource::Locked_resource(Resource_entry *resource,
                                 std::size_t requested, bool locked)
    : m_resource(resource),
      m_requested_amount(requested),
      m_is_locked(locked) {}

bool Locked_resource::is_locked() const { return m_is_locked; }

Locked_resource::~Locked_resource() {
  if (m_is_locked && m_requested_amount > 0 && m_resource != nullptr) {
    Resource_instance_monitor::release_resource(*m_resource,
                                                m_requested_amount);
  }
}

}  // namespace mysql::csa
