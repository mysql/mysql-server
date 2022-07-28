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
#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_mutex.h>
#include <mysql/components/services/psi_memory.h>
#include <mysql/components/services/psi_mutex.h>
#include <mysql/components/services/reference_caching.h>
#include "cache.h"
#include "channel.h"

namespace reference_caching {

namespace channel {

static DEFINE_BOOL_METHOD(create, (const char *service_names[],
                                   reference_caching_channel *out_channel)) {
  try {
    service_names_set<> refs;
    for (unsigned idx = 0; service_names[idx]; idx++)
      refs.insert(service_names[idx]);

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
    reinterpret_cast<channel_imp *>(channel)->set_valid(false);
    return false;
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(validate, (reference_caching_channel channel)) {
  try {
    reinterpret_cast<channel_imp *>(channel)->set_valid(true);
    return false;
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(fetch, (const char *service_name,
                                  reference_caching_channel *out_channel)) {
  try {
    *out_channel = reinterpret_cast<reference_caching_channel>(
        channel_imp::channel_by_name(service_name));
    return *out_channel ? false : true;
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
    return reinterpret_cast<channel_imp *>(channel)->ignore_list_add(
        implementation_name);
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(remove, (reference_caching_channel channel,
                                   const char *implementation_name)) {
  try {
    return reinterpret_cast<channel_imp *>(channel)->ignore_list_remove(
        implementation_name);
  } catch (...) {
    return true;
  }
}

static DEFINE_BOOL_METHOD(clear, (reference_caching_channel channel)) {
  try {
    return reinterpret_cast<channel_imp *>(channel)->ignore_list_clear();
  } catch (...) {
    return true;
  }
}

}  // namespace channel_ignore_list

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

  int count = static_cast<int>(sizeof(all_memory) / sizeof(PSI_memory_info));
  PSI_MEMORY_CALL(register_memory)(PSI_category, all_memory, count);
}

static mysql_service_status_t init() {
  register_instruments();
  try {
    return channel_imp::factory_init() ? 1 : 0;
  } catch (...) {
    return 1;
  }
}

static mysql_service_status_t deinit() {
  try {
    return channel_imp::factory_deinit() ? 1 : 0;
  } catch (...) {
    return 1;
  }
}

}  // namespace reference_caching

// component definition

BEGIN_SERVICE_IMPLEMENTATION(reference_caching, reference_caching_channel)
reference_caching::channel::create, reference_caching::channel::destroy,
    reference_caching::channel::invalidate,
    reference_caching::channel::validate,
    reference_caching::channel::fetch END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(reference_caching, reference_caching_cache)
reference_caching::cache::create, reference_caching::cache::destroy,
    reference_caching::cache::get,
    reference_caching::cache::flush END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(reference_caching,
                             reference_caching_channel_ignore_list)
reference_caching::channel_ignore_list::add,
    reference_caching::channel_ignore_list::remove,
    reference_caching::channel_ignore_list::clear END_SERVICE_IMPLEMENTATION();

BEGIN_COMPONENT_PROVIDES(reference_caching)
PROVIDES_SERVICE(reference_caching, reference_caching_channel),
    PROVIDES_SERVICE(reference_caching, reference_caching_cache),
    PROVIDES_SERVICE(reference_caching, reference_caching_channel_ignore_list),
    END_COMPONENT_PROVIDES();

REQUIRES_MYSQL_MUTEX_SERVICE_PLACEHOLDER;
REQUIRES_PSI_MUTEX_SERVICE_PLACEHOLDER;
REQUIRES_PSI_MEMORY_SERVICE_PLACEHOLDER;

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(reference_caching)
REQUIRES_MYSQL_MUTEX_SERVICE, REQUIRES_PSI_MUTEX_SERVICE,
    REQUIRES_PSI_MEMORY_SERVICE, END_COMPONENT_REQUIRES();

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
