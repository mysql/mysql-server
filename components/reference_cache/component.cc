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

#include "component.h"

#include <mysql/components/services/dynamic_loader_service_notification.h>
#include <mysql/components/services/mysql_rwlock.h>
#include <mysql/components/services/psi_memory.h>
#include <mysql/components/services/psi_rwlock.h>
#include <mysql/components/services/reference_caching.h>
#include <mysql/components/services/registry.h>

#include "cache.h"
#include "channel.h"

REQUIRES_SERVICE_PLACEHOLDER_AS(registry_registration,
                                current_registry_registration);
REQUIRES_SERVICE_PLACEHOLDER_AS(registry_query, current_registry_query);

namespace reference_caching {

namespace channel {

static DEFINE_BOOL_METHOD(create, (const char *service_names[],
                                   reference_caching_channel *out_channel)) {
  try {
    service_names_set<> refs;
    for (unsigned idx = 0; service_names[idx]; idx++) {
      Service_name_entry entry{service_names[idx], 0};
      refs.insert(entry);
    }

    *out_channel =
        reinterpret_cast<reference_caching_channel>(channel_imp::create(refs));
    return *out_channel ? false : true;
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(destroy, (reference_caching_channel channel)) {
  try {
    return channel_imp::destroy(reinterpret_cast<channel_imp *>(channel));
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(invalidate, (reference_caching_channel channel)) {
  try {
    channel_imp::increment_version(reinterpret_cast<channel_imp *>(channel));
    return false;
  } catch (...) {
    return true;
  }
}

}  // namespace channel

namespace cache {

static DEFINE_BOOL_METHOD(create, (reference_caching_channel channel,
                                   SERVICE_TYPE(registry) * registry,
                                   reference_caching_cache *out_cache)) {
  try {
    cache_imp *ptr =
        cache_imp::create(reinterpret_cast<channel_imp *>(channel), registry);
    if (!ptr) return true;
    *out_cache = reinterpret_cast<reference_caching_cache>(ptr);
    return false;
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(destroy, (reference_caching_cache cache)) {
  try {
    return cache_imp::destroy(reinterpret_cast<cache_imp *>(cache));
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(get, (reference_caching_cache cache,
                                unsigned service_name_index,
                                const my_h_service **refs)) {
  try {
    return reinterpret_cast<cache_imp *>(cache)->get(service_name_index, refs);
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(flush, (reference_caching_cache cache)) {
  try {
    return reinterpret_cast<cache_imp *>(cache)->flush();
  } catch (...) {
    return true;
  }
}

}  // namespace cache

namespace channel_ignore_list {

static DEFINE_BOOL_METHOD(add, (reference_caching_channel channel,
                                const char *implementation_name)) {
  try {
    return channel_imp::ignore_list_add(
        reinterpret_cast<channel_imp *>(channel), implementation_name);
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(remove, (reference_caching_channel channel,
                                   const char *implementation_name)) {
  try {
    return channel_imp::ignore_list_remove(
        reinterpret_cast<channel_imp *>(channel), implementation_name);
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(clear, (reference_caching_channel channel)) {
  try {
    return channel_imp::ignore_list_clear(
        reinterpret_cast<channel_imp *>(channel));
  } catch (...) {
    return true;
  }
}

}  // namespace channel_ignore_list

namespace service_notification {
static DEFINE_BOOL_METHOD(notify_before_unload,
                          (const char **services, unsigned int count)) {
  try {
    return channel_imp::service_notification(services, count, true);
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(notify_after_load,
                          (const char **services, unsigned int count)) {
  try {
    return channel_imp::service_notification(services, count, false);
  } catch (...) {
    return true;
  }
}
}  // namespace service_notification

PSI_memory_key KEY_mem_reference_cache;

void *Cache_malloced::operator new(std::size_t sz) {
  return my_malloc(KEY_mem_reference_cache, sz, 0);
}

void Cache_malloced::operator delete(void *ptr, std::size_t) { my_free(ptr); }

static void register_instruments() {
  static PSI_memory_info all_memory[] = {
      {&KEY_mem_reference_cache, "reference_cache_mem", 0, 0,
       "All the memory allocations for the reference cache component"},
  };

  const int count =
      static_cast<int>(sizeof(all_memory) / sizeof(PSI_memory_info));
  PSI_MEMORY_CALL(register_memory)(PSI_category, all_memory, count);
}

static mysql_service_status_t init() {
  register_instruments();
  try {
    if (channel_imp::factory_init()) return 1;
    if (current_registry_registration->set_default(
            "dynamic_loader_services_loaded_notification.reference_caching") ||
        current_registry_registration->set_default(
            "dynamic_loader_services_unload_notification.reference_caching")) {
      channel_imp::factory_deinit();
      return 1;
    }
    return 0;
  } catch (...) {
    return 1;
  }
}

static mysql_service_status_t deinit() {
  try {
    if (channel_imp::factory_deinit()) return 1;
    /*
      No need to change dynamic_loader default.
      Registry will take care of choose new default.
    */
    return 0;
  } catch (...) {
    return 1;
  }
}

}  // namespace reference_caching

// component definition

BEGIN_SERVICE_IMPLEMENTATION(reference_caching, reference_caching_channel)
reference_caching::channel::create, reference_caching::channel::destroy,
    reference_caching::channel::invalidate, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(reference_caching, reference_caching_cache)
reference_caching::cache::create, reference_caching::cache::destroy,
    reference_caching::cache::get,
    reference_caching::cache::flush END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(reference_caching,
                             reference_caching_channel_ignore_list)
reference_caching::channel_ignore_list::add,
    reference_caching::channel_ignore_list::remove,
    reference_caching::channel_ignore_list::clear END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(reference_caching,
                             dynamic_loader_services_loaded_notification)
reference_caching::service_notification::notify_after_load
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(reference_caching,
                             dynamic_loader_services_unload_notification)
reference_caching::service_notification::notify_before_unload
END_SERVICE_IMPLEMENTATION();

BEGIN_COMPONENT_PROVIDES(reference_caching)
PROVIDES_SERVICE(reference_caching, reference_caching_channel),
    PROVIDES_SERVICE(reference_caching, reference_caching_cache),
    PROVIDES_SERVICE(reference_caching, reference_caching_channel_ignore_list),
    PROVIDES_SERVICE(reference_caching,
                     dynamic_loader_services_loaded_notification),
    PROVIDES_SERVICE(reference_caching,
                     dynamic_loader_services_unload_notification),
    END_COMPONENT_PROVIDES();

REQUIRES_MYSQL_RWLOCK_SERVICE_PLACEHOLDER;
REQUIRES_PSI_RWLOCK_SERVICE_PLACEHOLDER;
REQUIRES_PSI_MEMORY_SERVICE_PLACEHOLDER;

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(reference_caching)
REQUIRES_MYSQL_RWLOCK_SERVICE, REQUIRES_PSI_RWLOCK_SERVICE,
    REQUIRES_PSI_MEMORY_SERVICE,
    REQUIRES_SERVICE_AS(registry_registration, current_registry_registration),
    REQUIRES_SERVICE_AS(registry_query, current_registry_query),
    END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(reference_caching)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(reference_caching, "mysql:reference_caching")
reference_caching::init, reference_caching::deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(reference_caching)
    END_DECLARE_LIBRARY_COMPONENTS
