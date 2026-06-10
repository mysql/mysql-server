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

#include "host_cache.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <mutex>
#include <random>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/resolver/error_code.h"
#include "mysql/harness/resolver/resolver.h"
#include "mysql/harness/utility/cache.h"

#include "mysqlrouter/host_cache_component.h"
#include "mysqlrouter/host_cache_entry.h"
#include "mysqlrouter/host_cache_temporary_entry.h"

using ResolvedAddresses = mysql_harness::resolver::ResolvedAddresses;
using ResolveHostResult = mysql_harness::resolver::ResolveHostResult;
using Hostname = std::string;
using LruCache =
    mysql_harness::utility::cache::DynamicLruCache<Hostname, host_cache::Entry>;

class HostCache::HostCacheImpl : public HostCacheStatisticsComponent {
 public:
  using VectorOfAddresses = std::vector<net::ip::address>;

  HostCacheImpl(std::shared_ptr<HostCacheConfig> config)
      : config_{std::move(config)},
        cache_{config_->max_entries_},
        rand_engine_{std::random_device{}()} {}
  ~HostCacheImpl() override = default;

  ResolveHostResult resolve_host(const std::string &hostname,
                                 CachePolicy cache_policy) {
    if (!config_->enabled_) {
      return mysql_harness::resolver::resolve_host(hostname,
                                                   CachePolicy::Bypass);
    }
    ++cache_uses_tot_;
    // Ensure that the HostCache instance is used with the expected cache
    // policies.
    //
    // It should be registered with ResolverRegistry using either:
    // * CachePolicy::fill_on_success,
    // * CachePolicy::use_if_present.
    assert(cache_policy == CachePolicy::FillOnSuccess ||
           cache_policy == CachePolicy::UseIfPresent);
    ResolvedAddresses result;
    TemporaryEntryPtr temporary_entry_ptr;
    if (LookupResult::CachedEntry ==
        lookup_cache(hostname, cache_policy, &result, &temporary_entry_ptr)) {
      if (result.addresses.empty()) {
        return stdx::unexpected(mysql_harness::resolver::make_error_code(
            mysql_harness::resolver::ErrcResolveResult::NotFound));
      }
      return result;
    }

    //  case LookupResult::k_temporary_entry
    temporary_resolving(temporary_entry_ptr);

    if (temporary_entry_ptr->host_entry_.addresses_.empty()) {
      return stdx::unexpected(mysql_harness::resolver::make_error_code(
          mysql_harness::resolver::ErrcResolveResult::NotFound));
    }

    // Copy the address list.
    result.addresses = temporary_entry_ptr->host_entry_.addresses_;
    result.ttl = temporary_entry_ptr->host_entry_.ttl_.count();
    return result;
  }

 private:
  enum class LookupResult { CachedEntry, TemporaryEntry };
  using State = host_cache::TemporaryEntry::State;
  using TemporaryEntry = host_cache::TemporaryEntry;
  using TemporaryEntryPtr = std::shared_ptr<host_cache::TemporaryEntry>;
  using Entry = host_cache::Entry;
  using second = std::chrono::seconds;

  static bool is_entry_valid(const Entry::time_point &tp, const Entry *entry) {
    return (tp - entry->creation_time_) < entry->ttl_;
  }
  static bool is_entry_valid(const Entry *entry) {
    return is_entry_valid(Entry::steady_clock::now(), entry);
  }

  uint32_t get_minimum_ttl_impl(const ResolveHostResult &result) {
    // This function is called for !result only in case not_found.
    if (!result) return config_->ttl_negative_;

    return std::min(result->ttl.value_or(config_->ttl_success_),
                    config_->ttl_success_);
  }

  std::chrono::seconds get_minimum_ttl(const ResolveHostResult &result) {
    const bool has_dedicated_ttl =
        result ||
        result.error() == mysql_harness::resolver::ErrcResolveResult::NotFound;
    if (!has_dedicated_ttl) return second{0};

    const auto ttl = get_minimum_ttl_impl(result);
    const auto jitter = config_->ttl_jitter_ratio_;
    const auto random_factor = distribution_(rand_engine_);

    const auto entry_ttl = std::chrono::seconds(
        static_cast<int>(std::round((1.0 + random_factor * jitter) * ttl)));

    return entry_ttl;
  }

  void try_purge_expired_entries() {
    if (!cache_.is_full()) return;

    std::vector<std::string> expired_entries;
    auto now = Entry::steady_clock::now();

    for (auto &[k, v] : cache_.get_container()) {
      if (!is_entry_valid(now, &v)) expired_entries.push_back(k);
    }

    for (auto &hostname : expired_entries) {
      cache_.remove(hostname);
      cache_drops_expired_tot_++;
    }

    // If cache is still full, then it will drop the item
    if (cache_.is_full()) ++cache_drops_tot_;
  }

  LookupResult lookup_cache(const Hostname &hostname,
                            const CachePolicy cache_policy,
                            ResolvedAddresses *out_cached,
                            TemporaryEntryPtr *out_temporary) {
    std::unique_lock<std::mutex> lock(context_mutex_);

    if (cache_policy == CachePolicy::UseIfPresent) {
      auto entry = cache_.get_cached_value(hostname);

      if (entry && is_entry_valid(entry)) {
        out_cached->addresses = entry->addresses_;
        out_cached->ttl = entry->ttl_.count();
        ++(entry->cache_hits_);
        ++cache_hits_tot_;
        return LookupResult::CachedEntry;
      }
    }

    if (const auto it = temporary_entries_.find(hostname);
        temporary_entries_.end() != it) {
      *out_temporary = it->second;
      ++(*out_temporary)->resolve_waiters_peak_;
      return LookupResult::TemporaryEntry;
    }

    auto temporary_entry = std::make_shared<TemporaryEntry>();
    temporary_entry->host_entry_.hostname_ = hostname;
    temporary_entries_[hostname] = temporary_entry;

    *out_temporary = std::move(temporary_entry);

    return LookupResult::TemporaryEntry;
  }

  void temporary_finished(TemporaryEntryPtr &temporary_entry,
                          const bool store_result) {
    if (store_result) {
      try_purge_expired_entries();
      // If the temporary object exists we know that
      // the cache doesn't contain the element.
      // All parallel requests to the hosts are also
      // aggregated under the temporary.
      // We can safely set it in cache and increment
      // the insertion cache-inserts.
      cache_.set(temporary_entry->host_entry_.hostname_,
                 temporary_entry->host_entry_);
      ++cache_inserts_tot_;
    }

    temporary_entries_.erase(temporary_entry->host_entry_.hostname_);
    temporary_entry->state_.set(State::Finished);
  }

  void temporary_process_result(TemporaryEntryPtr &temporary_entry,
                                const ResolveHostResult &result) {
    const bool store_to_cache =
        result ||
        result.error() == mysql_harness::resolver::ErrcResolveResult::NotFound;
    if (result) {
      temporary_entry->host_entry_.addresses_ = std::move(result->addresses);
    }

    // Lock is needed to:
    //
    // * sync access to random number generator,
    // * finalize the temporary (remove from temporary list, insert into cache).
    std::unique_lock<std::mutex> lock(context_mutex_);

    temporary_entry->host_entry_.ttl_ = get_minimum_ttl(result);
    temporary_entry->host_entry_.resolve_waiters_peak_ =
        temporary_entry->resolve_waiters_peak_.load();
    temporary_finished(temporary_entry, store_to_cache);
  }

  void temporary_resolving(TemporaryEntryPtr &temporary_entry) {
    auto this_thread_is_resolving =
        temporary_entry->state_.exchange(State::Initializing, State::Resolving);

    if (this_thread_is_resolving) {
      ResolveHostResult result;

      try {
        result = mysql_harness::resolver::resolve_host(
            temporary_entry->host_entry_.hostname_, CachePolicy::Bypass);
      } catch (const std::bad_alloc &) {
        result = stdx::unexpected(
            std::make_error_code(std::errc::not_enough_memory));
      } catch (...) {
        result = stdx::unexpected(
            std::make_error_code(std::errc::state_not_recoverable));
      }

      temporary_process_result(temporary_entry, result);
      return;
    }

    temporary_entry->state_.wait(State::Finished);
    return;
  }

 private:  // HostCacheStatisticsComponent
  std::atomic<uint64_t> cache_hits_tot_{0};
  std::atomic<uint64_t> cache_inserts_tot_{0};
  std::atomic<uint64_t> cache_uses_tot_{0};
  std::atomic<uint64_t> cache_drops_expired_tot_{0};
  std::atomic<uint64_t> cache_drops_tot_{0};

  std::vector<host_cache::Entry> get_entries() override {
    std::unique_lock<std::mutex> lock(context_mutex_);
    const auto &entries = cache_.get_container();
    std::vector<host_cache::Entry> result;

    result.reserve(entries.size());

    for (const auto &[k, v] : entries) {
      result.push_back(v);
    }
    return result;
  }

  std::vector<TemporaryEntryPtr> get_temporary_entries() override {
    std::unique_lock<std::mutex> lock(context_mutex_);
    std::vector<TemporaryEntryPtr> result;

    result.reserve(temporary_entries_.size());

    for (const auto &[k, v] : temporary_entries_) {
      result.push_back(v);
    }
    return result;
  }

  uint64_t get_temporary_size() override {
    std::unique_lock<std::mutex> lock(context_mutex_);
    return temporary_entries_.size();
  }

  uint64_t get_cache_hits() override { return cache_hits_tot_.load(); }

  uint64_t get_cache_size() override {
    std::unique_lock<std::mutex> lock(context_mutex_);
    return cache_.get_container().size();
  }

  uint64_t get_cache_inserts() override { return cache_inserts_tot_.load(); }

  uint64_t get_cache_drops() override { return cache_drops_tot_.load(); }

  uint64_t get_cache_used() override { return cache_uses_tot_.load(); }

  uint64_t get_cache_expired() override {
    return cache_drops_expired_tot_.load();
  }

 private:
  const std::shared_ptr<HostCacheConfig> config_;
  std::map<Hostname, TemporaryEntryPtr> temporary_entries_;
  LruCache cache_;
  std::mt19937 rand_engine_;
  std::uniform_real_distribution<double> distribution_{-1.0, 1.0};

  std::mutex context_mutex_;
};

HostCache::HostCache(std::shared_ptr<HostCacheConfig> config)
    : impl_(std::make_shared<HostCacheImpl>(std::move(config))) {}

HostCache::~HostCache() = default;

ResolveHostResult HostCache::resolve_host(const std::string &hostname,
                                          CachePolicy cache_policy) {
  return impl_->resolve_host(hostname, cache_policy);
}

std::shared_ptr<HostCacheStatisticsComponent> HostCache::get_statistics()
    const {
  return impl_;
}
