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

#ifndef MYSQLROUTER_HOST_CACHE_COMPONENT_INCLUDED
#define MYSQLROUTER_HOST_CACHE_COMPONENT_INCLUDED

#include <memory>
#include <shared_mutex>
#include <vector>

#include "mysqlrouter/host_cache_export.h"

#include "mysqlrouter/host_cache_config.h"
#include "mysqlrouter/host_cache_entry.h"
#include "mysqlrouter/host_cache_temporary_entry.h"

class HostCacheStatisticsComponent {
 public:
  using TemporaryEntryPtr = std::shared_ptr<host_cache::TemporaryEntry>;

 public:
  virtual ~HostCacheStatisticsComponent() = default;

  virtual std::vector<host_cache::Entry> get_entries() = 0;
  virtual std::vector<TemporaryEntryPtr> get_temporary_entries() = 0;
  virtual uint64_t get_temporary_size() = 0;
  virtual uint64_t get_cache_hits() = 0;
  virtual uint64_t get_cache_size() = 0;
  virtual uint64_t get_cache_inserts() = 0;
  virtual uint64_t get_cache_drops() = 0;
  virtual uint64_t get_cache_used() = 0;
  virtual uint64_t get_cache_expired() = 0;
};

class HOST_CACHE_EXPORT HostCacheComponent {
 public:
  using TemporaryEntryPtr = std::shared_ptr<host_cache::TemporaryEntry>;

 public:
  static HostCacheComponent &get_instance();
  ~HostCacheComponent();

  HostCacheConfig get_configuration() const;
  void set_configuration(const HostCacheConfig &config);

  void set_statistics(
      const std::shared_ptr<HostCacheStatisticsComponent> &stats);
  std::vector<host_cache::Entry> get_entries();
  std::vector<TemporaryEntryPtr> get_temporary_entries();
  uint64_t get_temporary_size();
  uint64_t get_cache_hits();
  uint64_t get_cache_size();
  uint64_t get_cache_inserts();
  uint64_t get_cache_drops();
  uint64_t get_cache_used();
  uint64_t get_cache_expired();

 private:
  HostCacheComponent();
  HostCacheConfig config_;
  mutable std::shared_mutex mutex_stat_;
  std::shared_ptr<HostCacheStatisticsComponent> stats_;
};

#endif  // MYSQLROUTER_HOST_CACHE_COMPONENT_INCLUDED
