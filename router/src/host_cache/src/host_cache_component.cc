/*
  Copyright (c) 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/host_cache_component.h"

using TemporaryEntryPtr = HostCacheComponent::TemporaryEntryPtr;

HostCacheComponent &HostCacheComponent::get_instance() {
  static HostCacheComponent obj;
  return obj;
}

HostCacheComponent::~HostCacheComponent() = default;

HostCacheComponent::HostCacheComponent() = default;

HostCacheConfig HostCacheComponent::get_configuration() const {
  std::shared_lock lock(mutex_stat_);
  return config_;
}

void HostCacheComponent::set_configuration(const HostCacheConfig &config) {
  std::unique_lock lock(mutex_stat_);
  config_ = config;
}

void HostCacheComponent::set_statistics(
    const std::shared_ptr<HostCacheStatisticsComponent> &stats) {
  std::unique_lock lock(mutex_stat_);
  stats_ = stats;
}

std::vector<host_cache::Entry> HostCacheComponent::get_entries() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return {};

  return stats_->get_entries();
}

std::vector<TemporaryEntryPtr> HostCacheComponent::get_temporary_entries() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return {};

  return stats_->get_temporary_entries();
}

uint64_t HostCacheComponent::get_cache_hits() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return 0;

  return stats_->get_cache_hits();
}

uint64_t HostCacheComponent::get_cache_size() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return 0;

  return stats_->get_cache_size();
}

uint64_t HostCacheComponent::get_cache_inserts() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return 0;

  return stats_->get_cache_inserts();
}

uint64_t HostCacheComponent::get_cache_drops() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return 0;

  return stats_->get_cache_drops();
}

uint64_t HostCacheComponent::get_cache_used() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return 0;

  return stats_->get_cache_used();
}

uint64_t HostCacheComponent::get_cache_expired() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return 0;

  return stats_->get_cache_expired();
}

uint64_t HostCacheComponent::get_temporary_size() {
  std::shared_lock lock(mutex_stat_);

  if (!stats_) return 0;

  return stats_->get_temporary_size();
}
