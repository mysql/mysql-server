/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
#include "cache.h"
#include <include/mysql/components/services/mysql_mutex.h>
#include <cassert>
#include <cstring>
#include "channel.h"

namespace reference_caching {
cache_imp *cache_imp::create(channel_imp *channel,
                             SERVICE_TYPE(registry) * registry) {
  assert(channel != nullptr);
  return new cache_imp(channel, registry);
}

bool cache_imp::destroy(cache_imp *cache) {
  delete cache;
  return false;
}

bool cache_imp::get(unsigned service_name_index, const my_h_service **out_ref) {
  bool channel_is_valid = m_channel->is_valid();

  if (service_name_index >= m_service_names.size()) {
    *out_ref = nullptr;
    return true;
  }

  if (m_cache && channel_is_valid) {
    // cache hit
    *out_ref = m_cache[service_name_index];
    return *out_ref ? false : true;
  }

  // cache miss
  flush();

  m_cache = (my_h_service **)my_malloc(
      KEY_mem_reference_cache, m_service_names.size() * sizeof(my_h_service *),
      MY_ZEROFILL);

  my_service<SERVICE_TYPE(registry_query)> query("registry_query", m_registry);

  unsigned offset = 0;
  for (std::string service_name : m_service_names) {
    std::set<my_h_service> cache_set;

    my_h_service_iterator iter;
    if (!query->create(service_name.c_str(), &iter)) {
      while (!query->is_valid(iter)) {
        const char *implementation_name;
        my_h_service svc;

        // can't get the name
        if (query->get(iter, &implementation_name)) break;

        // not the same service
        if (strncmp(implementation_name, service_name.c_str(),
                    service_name.length()))
          break;

        // not in the ignore list
        if (m_ignore_list.find(implementation_name) != m_ignore_list.end())
          continue;

        // add the reference to the list
        if (!m_registry->acquire(implementation_name, &svc)) {
          auto res = cache_set.insert(svc);

          /*
            release the unused reference if it's a duplicate of a reference
            already added
          */
          if (!res.second) m_registry->release(svc);
        }

        if (query->next(iter)) break;
      }
      query->release(iter);
    } else {
      // The service is not present in the registry.
      continue;
    }

    my_h_service *cache_row = (my_h_service *)my_malloc(
        KEY_mem_reference_cache, (cache_set.size() + 1) * sizeof(my_h_service),
        MY_ZEROFILL);

    my_h_service *cache_ptr = cache_row;
    for (my_h_service ref : cache_set) *cache_ptr++ = ref;

    if (offset == service_name_index) *out_ref = cache_row;

    m_cache[offset++] = cache_row;
  }
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
  return false;
}

cache_imp::cache_imp(channel_imp *channel, SERVICE_TYPE(registry) * registry)
    : m_channel{channel->ref()}, m_cache{nullptr}, m_registry{registry} {
  m_service_names = channel->get_service_names();
}

cache_imp::~cache_imp() {
  flush();
  m_channel->unref();
}
}  // namespace reference_caching
