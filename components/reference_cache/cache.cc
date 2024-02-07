/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <include/mysql/components/services/mysql_mutex.h>
#include <cassert>
#include <cstring>

#include "cache.h"
#include "channel.h"
#include "component.h"

namespace reference_caching {
cache_imp *cache_imp::create(channel_imp *channel,
                             SERVICE_TYPE(registry) * registry) {
  assert(channel != nullptr);
  mysql_rwlock_rdlock(&LOCK_channels);
  cache_imp *retval = new cache_imp(channel, registry);
  mysql_rwlock_unlock(&LOCK_channels);
  return retval;
}

bool cache_imp::destroy(cache_imp *cache) {
  delete cache;
  return false;
}

bool cache_imp::get(unsigned service_name_index, const my_h_service **out_ref) {
  const bool channel_is_valid = (m_cache_version == m_channel->version());

  if (unlikely(service_name_index >= m_service_names.size())) {
    *out_ref = nullptr;
    return true;
  }

  *out_ref = nullptr;
  if (m_populated && channel_is_valid) {
    if (m_cache) {
      // cache hit
      *out_ref = m_cache[service_name_index];
    }
    return *out_ref ? false : true;
  }

  // cache miss
  flush();

  /*
    Channel's ignore list may have been updated.
    So we have to update the local copy.

    Note that it is always safe to refer to m_channel
    without taking LOCK_channels because reference count
    for the channel was increased at the time of creating
    the cache. A channel cannot be destroyed when its
    reference count is > 1.

    ignore_list_copy() will take read lock on the channel.
  */
  m_channel->ignore_list_copy(m_ignore_list);

  m_service_names = m_channel->get_service_names();

  m_cache_version = m_channel->version();

  bool no_op = true;
  for (auto service_name : m_service_names) {
    no_op &= (service_name.count_.load() == 0);
  }

  if (!no_op) {
    m_cache = (my_h_service **)my_malloc(
        KEY_mem_reference_cache,
        m_service_names.size() * sizeof(my_h_service *), MY_ZEROFILL);

    unsigned offset = 0;
    for (auto service_name : m_service_names) {
      if (service_name.count_.load() == 0) continue;
      std::set<my_h_service> cache_set;

      my_h_service_iterator iter;
      if (!current_registry_query->create(service_name.name_.c_str(), &iter)) {
        while (!current_registry_query->is_valid(iter)) {
          const char *implementation_name;
          my_h_service svc;

          // can't get the name
          if (current_registry_query->get(iter, &implementation_name)) break;

          const char *dot = strchr(implementation_name, '.');
          size_t service_name_length = (dot - implementation_name);

          // not the same service
          if ((service_name_length != service_name.name_.length()) ||
              strncmp(implementation_name, service_name.name_.c_str(),
                      service_name.name_.length()))
            break;

          // not in the ignore list
          if (dot != nullptr && ++dot != nullptr) {
            if (m_ignore_list.find(dot) != m_ignore_list.end()) {
              if (current_registry_query->next(iter)) break;
              continue;
            }
          }

          // add the reference to the list
          if (!m_registry->acquire(implementation_name, &svc)) {
            auto res = cache_set.insert(svc);

            /*
              release the unused reference if it's a duplicate of a reference
              already added
            */
            if (!res.second) m_registry->release(svc);
          }

          if (current_registry_query->next(iter)) break;
        }
        current_registry_query->release(iter);
      } else {
        // The service is not present in the registry.
        continue;
      }

      my_h_service *cache_row = (my_h_service *)my_malloc(
          KEY_mem_reference_cache,
          (cache_set.size() + 1) * sizeof(my_h_service), MY_ZEROFILL);

      my_h_service *cache_ptr = cache_row;
      for (my_h_service ref : cache_set) *cache_ptr++ = ref;

      if (offset == service_name_index) *out_ref = cache_row;

      m_cache[offset++] = cache_row;
    }
  }
  m_populated = true;
  return *out_ref ? false : true;
}

bool cache_imp::flush() {
  if (m_cache) {
    unsigned offset = 0;
    for (auto service_name : m_service_names) {
      my_h_service *cache_row = m_cache[offset];
      if (cache_row) {
        for (my_h_service *iter = cache_row; *iter; iter++)
          m_registry->release(*iter);
        my_free(cache_row);
        m_cache[offset] = nullptr;
      }
      offset++;
    }
    my_free(m_cache);
    m_cache = nullptr;
  }
  m_populated = false;
  return false;
}

cache_imp::cache_imp(channel_imp *channel, SERVICE_TYPE(registry) * registry)
    : m_channel{channel->ref()},
      m_cache{nullptr},
      m_registry{registry},
      m_cache_version{channel->version()},
      m_populated{false} {
  m_service_names = channel->get_service_names();
}

cache_imp::~cache_imp() {
  flush();
  m_channel->unref();
}
}  // namespace reference_caching
