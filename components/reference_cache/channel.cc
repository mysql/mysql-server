/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
#include "channel.h"
#include <include/mysql/components/services/mysql_mutex.h>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

namespace reference_caching {

typedef std::unordered_set<channel_imp *, std::hash<channel_imp *>,
                           std::equal_to<channel_imp *>,
                           Component_malloc_allocator<channel_imp *>>
    channels_t;
static channels_t *channels;
typedef std::unordered_map<
    std::string, channel_imp *, std::hash<std::string>,
    std::equal_to<std::string>,
    Component_malloc_allocator<std::pair<const std::string, channel_imp *>>>
    channel_by_name_hash_t;
static channel_by_name_hash_t *channel_by_name_hash;
static mysql_mutex_t LOCK_channels;

channel_imp *channel_imp::create(service_names_set<> &service_names) {
  channel_imp *result = new channel_imp(service_names);
  mysql_mutex_lock(&LOCK_channels);

  auto new_element = channels->insert(result);
  if (!new_element.second) {
    delete result;
    return nullptr;
  }

  for (auto service_name : service_names) {
    auto new_cbyn = channel_by_name_hash->insert(
        channel_by_name_hash_t::value_type(service_name, result));
    if (!new_cbyn.second) {
      for (auto service_name_del : service_names)
        channel_by_name_hash->erase(service_name_del);
      channels->erase(new_element.first);
      delete result;
      return nullptr;
    }
  }

  mysql_mutex_unlock(&LOCK_channels);
  return result->ref();
}

bool channel_imp::destroy(channel_imp *channel) {
  bool res = true;
  int ref_count;
  mysql_mutex_lock(&LOCK_channels);
  channel->unref();
  ref_count = channel->m_reference_count;
  if (0 == ref_count) {
    auto it = channels->find(channel);
    if (it != channels->end()) {
      channels->erase(it);

      for (auto service_name : channel->get_service_names())
        channel_by_name_hash->erase(service_name);
      delete channel;
      res = false;
    }
  }
  mysql_mutex_unlock(&LOCK_channels);
  return res;
}

bool channel_imp::factory_init() {
  assert(!channels);

  channels = new channels_t(
      Component_malloc_allocator<channel_imp *>(KEY_mem_reference_cache));
  channel_by_name_hash = new channel_by_name_hash_t(
      Component_malloc_allocator<channel_imp *>(KEY_mem_reference_cache));

  static PSI_mutex_key key_LOCK_channels = 0;
  static PSI_mutex_info all_mutex[] = {
      {&key_LOCK_channels, "refcache_channel_mutex", 0, 0,
       "A mutex to guard access to the channels list"}};

  mysql_mutex_register(PSI_category, all_mutex, 1);
  mysql_mutex_init(key_LOCK_channels, &LOCK_channels, nullptr);
  return false;
}

bool channel_imp::factory_deinit() {
  assert(channels);
  mysql_mutex_lock(&LOCK_channels);

  if (channel_by_name_hash->size() || channels->size()) {
    mysql_mutex_unlock(&LOCK_channels);
    return true;
  }
  delete channel_by_name_hash;
  delete channels;
  channels = nullptr;
  mysql_mutex_unlock(&LOCK_channels);
  mysql_mutex_destroy(&LOCK_channels);
  return false;
}

channel_imp *channel_imp::channel_by_name(std::string service_name) {
  channel_imp *res = nullptr;
  mysql_mutex_lock(&LOCK_channels);

  auto it = channel_by_name_hash->find(service_name);
  if (it != channel_by_name_hash->end()) res = it->second->ref();

  mysql_mutex_unlock(&LOCK_channels);
  return res;
}

void channel_imp::ignore_list_copy(service_names_set<> &dest_set) {
  if (m_has_ignore_list) {
    mysql_mutex_lock(&LOCK_channels);
    dest_set = m_ignore_list;
    mysql_mutex_unlock(&LOCK_channels);
  }
}

bool channel_imp::ignore_list_add(std::string service_implementation) {
  mysql_mutex_lock(&LOCK_channels);
  auto ret = m_ignore_list.insert(service_implementation);
  m_has_ignore_list = true;
  mysql_mutex_unlock(&LOCK_channels);
  return !ret.second;
}

bool channel_imp::ignore_list_remove(std::string service_implementation) {
  if (m_has_ignore_list) {
    mysql_mutex_lock(&LOCK_channels);
    bool ret = m_ignore_list.erase(service_implementation) != 0;
    m_has_ignore_list = m_ignore_list.size() > 0;
    mysql_mutex_unlock(&LOCK_channels);
    return ret;
  }
  return true;
}

bool channel_imp::ignore_list_clear() {
  if (m_has_ignore_list) {
    mysql_mutex_lock(&LOCK_channels);
    m_ignore_list.clear();
    m_has_ignore_list = m_ignore_list.size();
    mysql_mutex_unlock(&LOCK_channels);
    return false;
  }
  return true;
}
}  // namespace reference_caching
