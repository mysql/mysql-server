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

#include "mrs/rest/response_cache.h"

#include <cassert>

#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "mrs/router_observation_entities.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {

namespace {
std::string make_table_key(const http::base::Uri &uri,
                           const std::string &user_id) {
  return uri.join_path() + (user_id.empty() ? "" : "\nuser_id=" + user_id);
}

std::string make_routine_key(const http::base::Uri &uri,
                             std::string_view req_body) {
  return std::string(uri.join_path()).append("\n").append(req_body);
}

std::string make_file_key(const mrs::database::entry::UniversalId &id) {
  return std::string(id.to_raw(), id.k_size);
}

class ResponseCacheOptions {
 public:
  std::optional<uint64_t> max_cache_size{};
};

class ParseResponseCacheOptions
    : public helper::json::RapidReaderHandlerStringValuesToStruct<
          ResponseCacheOptions> {
 public:
  explicit ParseResponseCacheOptions(const std::string &group_key)
      : group_key_(group_key) {}

  uint64_t to_uint(const std::string &value) {
    return std::stoull(value.c_str());
  }

  void handle_object_value(const std::string &key, std::string &&vt) override {
    if (key == group_key_ + ".maxCacheSize") {
      try {
        result_.max_cache_size = to_uint(vt);
      } catch (...) {
        log_warning(
            "Option %s has an invalid value and will fallback to the default",
            key.c_str());
        result_.max_cache_size.reset();
      }
    }
  }

  std::string group_key_;
};

auto parse_json_options(const std::string &config_key,
                        const std::string &options) {
  auto result = helper::json::text_to_handler<ParseResponseCacheOptions>(
      options, config_key);

  if (!result) {
    log_error(
        "Failed to parse 'ResponseCache/%s' options from global JSON "
        "configuration",
        config_key.c_str());
  }

  return result.value_or(ParseResponseCacheOptions::Result{});
}

}  // namespace

void ResponseCache::configure(const std::string &options) {
  auto cache_options = parse_json_options(config_key_, options);

  max_size_ =
      cache_options.max_cache_size.value_or(k_default_object_cache_size);

  if (cache_size_ > max_size_) {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    shrink_object_cache();
  }
}

void ResponseCache::shrink_object_cache(size_t extra_size) {
  auto now = CacheEntry::TimeType::clock::now();

  while (oldest_entry_ && cache_size_ + extra_size > max_size_.load()) {
    oldest_entry_->owner->remove_entry(oldest_entry_,
                                       now < oldest_entry_->expiration_time);
    remove_nolock(oldest_entry_);
  }
}

void ResponseCache::push(std::shared_ptr<CacheEntry> entry) {
  size_t size = entry->data.size();

  std::lock_guard<std::mutex> lock(entries_mutex_);
  if (cache_size_ + size > max_size_) {
    shrink_object_cache(size);
  }

  cache_size_ += size;

  entry->next_ptr = newest_entry_;
  if (newest_entry_) newest_entry_->prev_ptr = entry;
  newest_entry_ = entry;
  if (!oldest_entry_) oldest_entry_ = newest_entry_;
}

void ResponseCache::remove(std::shared_ptr<CacheEntry> entry) {
  entry->owner->remove_entry(entry, false);

  {
    std::lock_guard<std::mutex> lock(entries_mutex_);

    remove_nolock(entry);
  }
}

void ResponseCache::remove_nolock(std::shared_ptr<CacheEntry> entry) {
  cache_size_ -= entry->data.size();

  if (entry->prev_ptr)
    entry->prev_ptr->next_ptr = entry->next_ptr;
  else
    newest_entry_ = entry->next_ptr;

  if (entry->next_ptr)
    entry->next_ptr->prev_ptr = entry->prev_ptr;
  else
    oldest_entry_ = entry->prev_ptr;
}

int ResponseCache::remove_all(EndpointResponseCache *cache) {
  int count = 0;
  std::lock_guard<std::mutex> lock(entries_mutex_);

  if (!newest_entry_) return count;

  std::shared_ptr<CacheEntry> new_start = nullptr;
  std::shared_ptr<CacheEntry> new_end = nullptr;

  std::shared_ptr<CacheEntry> next;
  for (auto ptr = newest_entry_; ptr; ptr = next) {
    next = ptr->next_ptr;

    if (ptr->owner == cache) {
      ++count;

      cache_size_ -= ptr->data.size();
      ptr->next_ptr = nullptr;
      ptr->prev_ptr = nullptr;
    } else {
      if (!new_start) {
        new_start = ptr;
        new_end = ptr;
        ptr->next_ptr = nullptr;
        ptr->prev_ptr = nullptr;
      } else {
        new_end->next_ptr = ptr;
        ptr->next_ptr = nullptr;
        ptr->prev_ptr = new_end;
        new_end = ptr;
      }
    }
  }

  newest_entry_ = new_start;
  oldest_entry_ = new_end;

  return count;
}

EndpointResponseCache::EndpointResponseCache(ResponseCache *owner,
                                             uint64_t ttl_ms)
    : owner_(owner), ttl_(std::chrono::milliseconds(ttl_ms)) {
  Counter<kEntityCounterRestCachedEndpoints>::increment();
}

std::shared_ptr<CacheEntry> EndpointResponseCache::create_entry(
    const std::string &key, const std::string &data, int64_t items,
    std::optional<helper::MediaType> media_type,
    std::optional<std::string> media_type_str) {
  if (owner_->max_cache_size() < data.size()) {
    return nullptr;
  }

  auto entry = std::make_shared<CacheEntry>();

  entry->data = data;
  entry->items = items;
  entry->media_type = media_type;
  entry->media_type_str = media_type_str;

  entry->owner = this;
  entry->key = key;
  if (ttl_.count() == 0)  // ttl=0 means no-expiration
    entry->expiration_time = CacheEntry::TimeType::max();
  else
    entry->expiration_time = CacheEntry::TimeType::clock::now() + ttl_;

  owner_->push(entry);
  {
    std::unique_lock lock(cache_mutex_);

    cache_.emplace(entry->key, entry);
  }

  return entry;
}

void EndpointResponseCache::remove_entry(std::shared_ptr<CacheEntry> entry,
                                         bool ejected) {
  {
    std::unique_lock lock(cache_mutex_);
    remove_entry_nolock(entry, ejected);
  }
}

void EndpointResponseCache::remove_entry_nolock(
    std::shared_ptr<CacheEntry> entry, [[maybe_unused]] bool ejected) {
  cache_.erase(entry->key);
}

std::shared_ptr<CacheEntry> EndpointResponseCache::lookup(
    const std::string &key) const {
  std::shared_lock lock(cache_mutex_);

  auto it = cache_.find(key);
  if (it != cache_.end()) {
    return it->second;
  }

  return {};
}

ItemEndpointResponseCache::ItemEndpointResponseCache(ResponseCache *owner,
                                                     uint64_t ttl_ms)
    : EndpointResponseCache(owner, ttl_ms) {}

ItemEndpointResponseCache::~ItemEndpointResponseCache() {
  int count;
  {
    std::unique_lock lock(cache_mutex_);
    count = owner_->remove_all(this);
  }
  Counter<kEntityCounterRestCachedEndpoints>::increment(-1);
  Counter<kEntityCounterRestCachedItems>::increment(-count);
}

std::shared_ptr<CacheEntry> ItemEndpointResponseCache::create_table_entry(
    const Uri &uri, const std::string &user_id, const std::string &data,
    int64_t items) {
  auto r = create_entry(make_table_key(uri, user_id), data, items);
  if (r) {
    Counter<kEntityCounterRestCacheItemLoads>::increment();
    Counter<kEntityCounterRestCachedItems>::increment();
  }
  return r;
}

std::shared_ptr<CacheEntry> ItemEndpointResponseCache::create_routine_entry(
    const Uri &uri, std::string_view req_body, const std::string &data,
    std::optional<helper::MediaType> media_type) {
  auto r = create_entry(make_routine_key(uri, req_body), data, 0, media_type);
  if (r) {
    Counter<kEntityCounterRestCacheItemLoads>::increment();
    Counter<kEntityCounterRestCachedItems>::increment();
  }
  return r;
}

std::shared_ptr<CacheEntry> ItemEndpointResponseCache::create_routine_entry(
    const Uri &uri, std::string_view req_body, const std::string &data,
    const std::string &media_type_str) {
  auto r = create_entry(make_routine_key(uri, req_body), data, 0, {},
                        media_type_str);
  if (r) {
    Counter<kEntityCounterRestCacheItemLoads>::increment();
    Counter<kEntityCounterRestCachedItems>::increment();
  }
  return r;
}

std::shared_ptr<CacheEntry> ItemEndpointResponseCache::lookup_table(
    const Uri &uri, const std::string &user_id) {
  auto r = lookup(make_table_key(uri, user_id));
  if (r) {
    if (r->is_expired()) {
      owner_->remove(r);
      r.reset();
    }
    Counter<kEntityCounterRestCacheItemHits>::increment();
  } else {
    Counter<kEntityCounterRestCacheItemMisses>::increment();
  }
  return r;
}

std::shared_ptr<CacheEntry> ItemEndpointResponseCache::lookup_routine(
    const Uri &uri, std::string_view req_body) {
  auto r = lookup(make_routine_key(uri, req_body));
  if (r) {
    if (r->is_expired()) {
      owner_->remove(r);
      r.reset();
    }
    Counter<kEntityCounterRestCacheItemHits>::increment();
  } else {
    Counter<kEntityCounterRestCacheItemMisses>::increment();
  }
  return r;
}

void ItemEndpointResponseCache::remove_entry_nolock(
    [[maybe_unused]] std::shared_ptr<CacheEntry> entry, bool ejected) {
  Counter<kEntityCounterRestCachedItems>::increment(-1);
  if (ejected) Counter<kEntityCounterRestCacheItemEjects>::increment(1);

  EndpointResponseCache::remove_entry_nolock(entry, ejected);
}

FileEndpointResponseCache::FileEndpointResponseCache(ResponseCache *owner)
    : EndpointResponseCache(owner, 0) {}

std::shared_ptr<CacheEntry> FileEndpointResponseCache::lookup_file(
    const UniversalId &id) {
  auto r = lookup(make_file_key(id));
  if (r) {
    if (r->is_expired()) {
      owner_->remove(r);
      r.reset();
    }
    Counter<kEntityCounterRestCacheFileHits>::increment();
  } else {
    Counter<kEntityCounterRestCacheFileMisses>::increment();
  }

  return r;
}

FileEndpointResponseCache::~FileEndpointResponseCache() {
  int count;
  {
    std::unique_lock lock(cache_mutex_);
    count = owner_->remove_all(this);
  }
  Counter<kEntityCounterRestCachedEndpoints>::increment(-1);
  Counter<kEntityCounterRestCachedFiles>::increment(-count);
}

std::shared_ptr<CacheEntry> FileEndpointResponseCache::create_file_entry(
    const UniversalId &id, const std::string &data,
    helper::MediaType media_type) {
  auto r = create_entry(make_file_key(id), data, 0, media_type);
  if (r) {
    Counter<kEntityCounterRestCacheFileLoads>::increment();
    Counter<kEntityCounterRestCachedFiles>::increment();
  }
  return r;
}

void FileEndpointResponseCache::remove_entry_nolock(
    [[maybe_unused]] std::shared_ptr<CacheEntry> entry, bool ejected) {
  Counter<kEntityCounterRestCachedFiles>::increment(-1);
  if (ejected) Counter<kEntityCounterRestCacheFileEjects>::increment(1);

  EndpointResponseCache::remove_entry_nolock(entry, ejected);
}

}  // namespace mrs
