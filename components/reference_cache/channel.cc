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
#include "channel.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <mysql/components/my_service.h>
#include <mysql/components/services/dynamic_loader_service_notification.h>
#include <scope_guard.h>
#include <template_utils.h>

#include "component.h"

namespace reference_caching {

typedef std::unordered_set<channel_imp *, std::hash<channel_imp *>,
                           std::equal_to<channel_imp *>,
                           Component_malloc_allocator<channel_imp *>>
    channels_t;

static channels_t *channels;
channel_by_name_hash_t *channel_by_name_hash;
mysql_rwlock_t LOCK_channels;

channel_imp::channel_imp()
    : m_has_ignore_list(false), m_reference_count{0}, m_version{0} {
  mysql_rwlock_init(0, &m_lock);
}

channel_imp::channel_imp(service_names_set<> &service_names) : channel_imp() {
  m_service_names = service_names;
}

channel_imp::~channel_imp() { mysql_rwlock_destroy(&m_lock); }

channel_imp *channel_imp::create(service_names_set<> &service_names) {
  channel_imp *result = new channel_imp(service_names);
  mysql_rwlock_wrlock(&LOCK_channels);
  auto release_guard =
      create_scope_guard([&] { mysql_rwlock_unlock(&LOCK_channels); });

  result->initialize_service_counts();

  auto new_element = channels->insert(result);
  if (!new_element.second) {
    delete result;
    return nullptr;
  }

  for (auto service_name : service_names) {
    channel_by_name_hash->insert(
        channel_by_name_hash_t::value_type(service_name.name_, result));
  }
  return result->ref();
}

bool channel_imp::destroy(channel_imp *channel) {
  bool res = true;
  int ref_count;
  mysql_rwlock_wrlock(&LOCK_channels);
  ref_count = channel->m_reference_count;
  if (1 == ref_count) {
    channel->unref();
    auto it = channels->find(channel);
    if (it != channels->end()) {
      channels->erase(it);

      for (auto service_name : channel->get_service_names()) {
        auto range = channel_by_name_hash->equal_range(service_name.name_);
        for (auto it_local = range.first; it_local != range.second;
             ++it_local) {
          if (it_local->second == channel) {
            channel_by_name_hash->erase(it_local);
            break;
          }
        }
      }
      delete channel;
      res = false;
    }
  }
  mysql_rwlock_unlock(&LOCK_channels);
  return res;
}

void channel_imp::increment_version(channel_imp *channel) {
  mysql_rwlock_rdlock(&LOCK_channels);
  channel->increment_version_no_lock();
  mysql_rwlock_unlock(&LOCK_channels);
}

bool channel_imp::factory_init() {
  assert(!channels);

  channels = new channels_t(
      Component_malloc_allocator<channel_imp *>(KEY_mem_reference_cache));
  channel_by_name_hash = new channel_by_name_hash_t(
      Component_malloc_allocator<channel_imp *>(KEY_mem_reference_cache));

  static PSI_rwlock_key key_LOCK_channels = 0;
  static PSI_rwlock_info all_locks[] = {
      {&key_LOCK_channels, "refcache_channel_rw_lock", 0, 0,
       "A RW lock to guard access to the channels list"}};

  mysql_rwlock_register(PSI_category, all_locks, 1);
  mysql_rwlock_init(key_LOCK_channels, &LOCK_channels);
  return false;
}

bool channel_imp::factory_deinit() {
  assert(channels);
  mysql_rwlock_wrlock(&LOCK_channels);
  auto release_guard =
      create_scope_guard([&] { mysql_rwlock_unlock(&LOCK_channels); });

  if (channel_by_name_hash->size() || channels->size()) {
    return true;
  }
  delete channel_by_name_hash;
  delete channels;
  channels = nullptr;
  release_guard.rollback();
  mysql_rwlock_destroy(&LOCK_channels);
  return false;
}

void channel_imp::initialize_service_counts() {
  auto last = m_service_names.end();
  for (auto service_name = m_service_names.begin(); service_name != last;
       ++service_name) {
    my_h_service_iterator iter = nullptr;
    service_name->count_ = 0;
    if (current_registry_query->create(service_name->name_.c_str(), &iter)) {
      continue;
    }
    while (!current_registry_query->is_valid(iter)) {
      const char *implementation_name;
      const char *dot = nullptr;
      if (!current_registry_query->get(iter, &implementation_name)) {
        dot = strchr(implementation_name, '.');
        size_t service_name_length = (dot - implementation_name);
        if ((service_name_length != service_name->name_.length()) ||
            strncmp(implementation_name, service_name->name_.c_str(),
                    service_name->name_.length()))
          break;
      }
      if (dot && m_ignore_list.find(dot) == m_ignore_list.end())
        ++service_name->count_;
      if (current_registry_query->next(iter)) break;
    }
    current_registry_query->release(iter);
  }
}

void channel_imp::ignore_list_copy(
    service_names_set<std::string, std::less<std::string>> &dest_set) {
  mysql_rwlock_rdlock(&m_lock);
  dest_set = m_ignore_list;
  mysql_rwlock_unlock(&m_lock);
}

service_names_set<> &channel_imp::get_service_names() {
  mysql_rwlock_wrlock(&m_lock);
  auto cleanup = create_scope_guard([&] { mysql_rwlock_unlock(&m_lock); });
  return m_service_names;
}

bool channel_imp::ignore_list_add(std::string &service_implementation) {
  mysql_rwlock_wrlock(&m_lock);
  auto ret = m_ignore_list.insert(service_implementation);
  initialize_service_counts();
  m_has_ignore_list = true;
  mysql_rwlock_unlock(&m_lock);
  return !ret.second;
}

bool channel_imp::ignore_list_add(channel_imp *channel,
                                  std::string service_implementation) {
  if (!channel) return true;
  mysql_rwlock_rdlock(&LOCK_channels);
  bool ret = channel->ignore_list_add(service_implementation);
  mysql_rwlock_unlock(&LOCK_channels);
  return ret;
}

bool channel_imp::ignore_list_remove(std::string &service_implementation) {
  mysql_rwlock_wrlock(&m_lock);
  auto release_guard =
      create_scope_guard([&] { mysql_rwlock_unlock(&m_lock); });
  if (m_has_ignore_list) {
    const bool ret = m_ignore_list.erase(service_implementation) == 0;
    if (!ret) initialize_service_counts();
    m_has_ignore_list = m_ignore_list.size() > 0;
    return ret;
  }
  return true;
}

bool channel_imp::ignore_list_remove(channel_imp *channel,
                                     std::string service_implementation) {
  if (!channel) return true;
  mysql_rwlock_rdlock(&LOCK_channels);
  bool ret = channel->ignore_list_remove(service_implementation);
  mysql_rwlock_unlock(&LOCK_channels);
  return ret;
}

bool channel_imp::ignore_list_clear() {
  mysql_rwlock_wrlock(&m_lock);
  auto release_guard =
      create_scope_guard([&] { mysql_rwlock_unlock(&m_lock); });
  if (m_has_ignore_list) {
    m_ignore_list.clear();
    m_has_ignore_list = m_ignore_list.size();
    return false;
  }
  return true;
}

bool channel_imp::ignore_list_clear(channel_imp *channel) {
  if (!channel) return true;
  mysql_rwlock_rdlock(&LOCK_channels);
  bool ret = channel->ignore_list_clear();
  mysql_rwlock_unlock(&LOCK_channels);
  return ret;
}

/**
  Take actions to reference caching caches to refresh
  their cached service references.

  @param [in] services Services being loaded or unloaded
  @param [in] count    Number of services
  @param [in] unload   Flag to denote whether the function
                       is called as a part of dynamic_loader::load
                       or dynamic_loader::unload operation
*/
bool channel_imp::service_notification(const char **services,
                                       unsigned int count, bool unload) {
  std::unordered_map<std::string, std::vector<std::string>>
      service_to_implementation_map;

  /*
    Create a map with service name as key and vector of implementation as value.

    service_1 -> {implementation_1, implementation_2, ...., implementation_n }
    ...
    service_m -> {implementation_1, implementation_2, ...., implementation_n }
  */
  for (unsigned index = 0; index < count; ++index) {
    const char *dot_location = strchr(services[index], '.');
    if (!dot_location) continue;

    /* Format: <service_name>.<implementation_name> */
    std::string service_name{
        services[index], static_cast<size_t>(dot_location - services[index])};
    std::string implementation{dot_location + 1};

    auto it = service_to_implementation_map.find(service_name);
    if (it != service_to_implementation_map.end()) {
      it->second.push_back(implementation);
    } else {
      std::vector<std::string> implementation_vector;
      implementation_vector.push_back(implementation);
      service_to_implementation_map[service_name] = implementation_vector;
    }
  }

  mysql_rwlock_rdlock(&LOCK_channels);
  auto release_guard =
      create_scope_guard([&] { mysql_rwlock_unlock(&LOCK_channels); });

  /*
    For each service in the map created above do following:
    1. Find all channels created for a given service.
    2. For each such channel do following
        2.1 For each implementation mapped against the service do following
            2.1.1 If services are being unloaded, unconditionally add
                  implementation name to channel's ignore list

                  This will prevent reference caches from reacquiring
                  references of services provided by components being
                  unloaded.
            2.1.2 If services have been loaded, unconditionally remove
                  implementation from channel's ignore list

                  This will force reference caches to add new references
                  of services provided by recently loaded components.
        2.2 Invalidate the channel - This will force reference caches to
            refresh cached service references
  */
  for (auto &service_iterator : service_to_implementation_map) {
    auto channel_range =
        channel_by_name_hash->equal_range(service_iterator.first);

    for (auto channel_iterator = channel_range.first;
         channel_iterator != channel_range.second; ++channel_iterator) {
      for (auto &one_implementation : service_iterator.second) {
        auto key = channel_iterator->second->m_service_names.find(
            Service_name_entry{service_iterator.first.c_str(), 0});
        if (unload) {
          /*
            This will force reference caches to ignore services
            provided by components being unloaded
          */
          (void)channel_iterator->second->ignore_list_add(one_implementation);
          if (key != channel_iterator->second->m_service_names.end())
            if (key->count_.load() > 0) --key->count_;
        } else {
          /*
            This will allow reference caches to cache previous
            ignored service implementation.

            This is useful in scenarios when a component is
            uninstalled and then installed again.
          */
          (void)channel_iterator->second->ignore_list_remove(
              one_implementation);
          if (key != channel_iterator->second->m_service_names.end())
            ++key->count_;
        }
        /* Force reference caches to refresh service references */
        channel_iterator->second->increment_version_no_lock();
      }
    }
  }

  release_guard.rollback();
  /*
    At this point, a service load/unload operation is in progress.
    Further,
    - There is no lock on reference caching channels.
      This will allow reference caching caches to reload
      new ignore list.
    - There is no lock on service registry. This will
      allow reference caching caches to release existing
      reference and acquire them again.

    If there are reference caches created from such channel(s),
    we should provide them an opportunity to refresh their
    service references. If this happens *before* we continue
    with rest of the unload operation, there is good chance of
    it succeeding.

    If unload fails, services provided by these component remain
    in the ignore lists of corresponding channels. Thus, trying
    to unload them again at a later point is likely to succeed.

    Note that this will not impact unload operations of services
    which are not served by reference caching component.
  */
  my_service<SERVICE_TYPE(registry_query)> query("registry_query",
                                                 mysql_service_registry);
  if (query.is_valid()) {
    my_h_service_iterator iter;
    std::string service_name =
        unload ? "dynamic_loader_services_unload_notification"
               : "dynamic_loader_services_loaded_notification";
    if (!query->create(service_name.c_str(), &iter)) {
      while (!query->is_valid(iter)) {
        const char *implementation_name;

        // can't get the name
        if (query->get(iter, &implementation_name)) break;

        if (strncmp(implementation_name, service_name.c_str(),
                    service_name.length())) {
          break;
        }

        // not self
        const char *dot = strchr(implementation_name, '.');
        if (dot != nullptr && ++dot != nullptr) {
          if (!strncmp(dot, "reference_caching",
                       (size_t)(sizeof("reference_caching") - 1))) {
            if (query->next(iter)) break;
            continue;
          }
        }

        if (unload) {
          my_service<SERVICE_TYPE(dynamic_loader_services_unload_notification)>
              unload_notification(implementation_name, mysql_service_registry);
          if (unload_notification.is_valid())
            (void)unload_notification->notify(services, count);
        } else {
          my_service<SERVICE_TYPE(dynamic_loader_services_loaded_notification)>
              load_notification(implementation_name, mysql_service_registry);
          if (load_notification.is_valid())
            (void)load_notification->notify(services, count);
        }

        if (query->next(iter)) break;
      }
      query->release(iter);
    }
  }
  return false;
}
}  // namespace reference_caching
