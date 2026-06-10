/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_REST_RESPONSE_CACHE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_REST_RESPONSE_CACHE_H_

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include "helper/media_type.h"
#include "http/base/uri.h"
#include "mrs/database/entry/universal_id.h"

namespace mrs {

class EndpointResponseCache;

struct CacheEntry {
  using TimeType = std::chrono::time_point<std::chrono::system_clock>;

  std::string data;
  int64_t items = 0;
  std::optional<helper::MediaType> media_type;
  std::optional<std::string> media_type_str;

  std::string key;
  TimeType expiration_time;

  EndpointResponseCache *owner;

  std::shared_ptr<CacheEntry> next_ptr;
  std::shared_ptr<CacheEntry> prev_ptr;

  bool is_expired() const { return expiration_time < TimeType::clock::now(); }
};

constexpr const size_t k_default_object_cache_size = 1000000;

class ResponseCache {
 public:
  friend class EndpointResponseCache;
  friend class ItemEndpointResponseCache;
  friend class FileEndpointResponseCache;

  explicit ResponseCache(const std::string &config_key)
      : config_key_(config_key) {}

  void configure(const std::string &options);

  size_t max_cache_size() const { return max_size_; }

  void remove(std::shared_ptr<CacheEntry> entry);

 private:
  void push(std::shared_ptr<CacheEntry> entry);
  void remove_nolock(std::shared_ptr<CacheEntry> entry);

  int remove_all(EndpointResponseCache *cache);

  void shrink_object_cache(size_t extra_size = 0);

  std::string config_key_;

  std::shared_ptr<CacheEntry> newest_entry_;
  std::shared_ptr<CacheEntry> oldest_entry_;
  std::mutex entries_mutex_;
  std::atomic<size_t> cache_size_ = 0;

  std::atomic<size_t> max_size_ = k_default_object_cache_size;
};

class EndpointResponseCache {
 public:
  using Uri = ::http::base::Uri;
  using UniversalId = ::mrs::database::entry::UniversalId;

 protected:
  EndpointResponseCache(ResponseCache *owner, uint64_t ttl_ms);
  virtual ~EndpointResponseCache() = default;

  std::shared_ptr<CacheEntry> create_entry(
      const std::string &key, const std::string &data, int64_t items = 0,
      std::optional<helper::MediaType> media_type = {},
      std::optional<std::string> media_type_str = {});

  void remove_entry(std::shared_ptr<CacheEntry> entry, bool ejected);
  virtual void remove_entry_nolock(std::shared_ptr<CacheEntry> entry,
                                   bool ejected);

  std::shared_ptr<CacheEntry> lookup(const std::string &key) const;

  friend class ResponseCache;

  ResponseCache *owner_;

  std::chrono::milliseconds ttl_;

  std::unordered_map<std::string, std::shared_ptr<CacheEntry>> cache_;
  mutable std::shared_mutex cache_mutex_;
};

class ItemEndpointResponseCache : public EndpointResponseCache {
 public:
  ItemEndpointResponseCache(ResponseCache *owner, uint64_t ttl_ms);
  ~ItemEndpointResponseCache() override;

  std::shared_ptr<CacheEntry> create_table_entry(const Uri &uri,
                                                 const std::string &user_id,
                                                 const std::string &data,
                                                 int64_t items);

  std::shared_ptr<CacheEntry> create_routine_entry(
      const Uri &uri, std::string_view req_body, const std::string &data,
      std::optional<helper::MediaType> media_type = {});

  std::shared_ptr<CacheEntry> create_routine_entry(
      const Uri &uri, std::string_view req_body, const std::string &data,
      const std::string &media_type_str);

  std::shared_ptr<CacheEntry> lookup_table(const Uri &uri,
                                           const std::string &user_id);

  std::shared_ptr<CacheEntry> lookup_routine(const Uri &uri,
                                             std::string_view req_body);

 private:
  void remove_entry_nolock(std::shared_ptr<CacheEntry> entry,
                           bool ejected) override;
};

class FileEndpointResponseCache : public EndpointResponseCache {
 public:
  explicit FileEndpointResponseCache(ResponseCache *owner);
  ~FileEndpointResponseCache() override;

  std::shared_ptr<CacheEntry> create_file_entry(const UniversalId &id,
                                                const std::string &data,
                                                helper::MediaType media_type);

  std::shared_ptr<CacheEntry> lookup_file(const UniversalId &id);

 private:
  void remove_entry_nolock(std::shared_ptr<CacheEntry> entry,
                           bool ejected) override;
};

}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_REST_RESPONSE_CACHE_H_
