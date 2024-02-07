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

#include "test_reference_cache.h"
#include <assert.h>
#include <limits.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/reference_caching.h>
#include <stdio.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

REQUIRES_SERVICE_PLACEHOLDER(reference_caching_channel);
REQUIRES_SERVICE_PLACEHOLDER(reference_caching_cache);
REQUIRES_SERVICE_PLACEHOLDER(reference_caching_channel_ignore_list);

BEGIN_COMPONENT_REQUIRES(test_reference_cache)
REQUIRES_SERVICE(reference_caching_channel)
, REQUIRES_SERVICE(reference_caching_cache),
    REQUIRES_SERVICE(reference_caching_channel_ignore_list),
    END_COMPONENT_REQUIRES();

reference_caching_channel channel = nullptr;

/**
  A helper class to implement the storage into the call cache into a thread
  local variable.
  Make sure it's present for all threads calling the serice via the cache

  foo_cache can be:
   - nonexistent (the thread local variable is null)
   - invalid (the thread local variable is set, but the cache creation failed)
   - valid (the thread local variable is set and the cache creation succeeded)

*/
class foo_cache {
  reference_caching_cache cache_;

  foo_cache() : cache_(nullptr) {
    assert(channel != nullptr);
    if (mysql_service_reference_caching_cache->create(
            channel, mysql_service_registry, &cache_))
      cache_ = nullptr;
  }

  /** the thread local variable keeper */
  static foo_cache *current_cache() {
    thread_local foo_cache *tl_cache_ptr = nullptr;
    if (tl_cache_ptr == nullptr) tl_cache_ptr = new foo_cache();
    return tl_cache_ptr;
  }

 public:
  bool is_valid() { return cache_ != nullptr; }

  /**
    Call the service.
    Fills in the cache in the process if empty
    @return: number of consumers called
  */
  unsigned call(int arg) {
    const my_h_service *refs = nullptr;
    unsigned called = 0;
    if (is_valid() && !mysql_service_reference_caching_cache->get(
                          cache_, arg /* service_name_index */, &refs)) {
      for (const my_h_service *svc = refs; *svc; svc++) {
        SERVICE_TYPE(mysql_test_foo) *f =
            reinterpret_cast<SERVICE_TYPE(mysql_test_foo) *>(*svc);
        if (f->emit(arg))
          break;
        else
          called++;
      }
    }
    return called;  // the reference wasn't valid or the service call failed
  }

  /**
    flush the cache.
    @retval false success
    @retval true failure
  */
  bool flush() {
    return (!is_valid() ||
            mysql_service_reference_caching_cache->flush(cache_));
  }

  /** helper method to get or create the thread local cache (if absent) */
  static foo_cache *get_foo_cache() {
    foo_cache *ptr = current_cache();
    return ptr;
  }

  /** helper method to delete the thread local cache if present */
  static void release_foo_cache() { delete current_cache(); }

  ~foo_cache() {
    if (is_valid()) mysql_service_reference_caching_cache->destroy(cache_);
  }
};

static DEFINE_BOOL_METHOD(mysql_test_ref_cache_release_cache, ()) {
  foo_cache::release_foo_cache();
  return false;
}

static DEFINE_BOOL_METHOD(mysql_test_ref_cache_produce_event, (int arg)) {
  int result = 0;
  foo_cache *c = foo_cache::get_foo_cache();
  if (c) {
    return c->call(arg);
  }
  return result;
}

static DEFINE_BOOL_METHOD(mysql_test_ref_cache_flush, ()) {
  int result = 0;
  foo_cache *c = foo_cache::get_foo_cache();
  if (c) {
    if (c->flush()) result = 1;
  }
  return result;
}

// the kill switch for the benchamrk UDFs
std::atomic<bool> kill_switch;

/**
  A benchmark UDF: spawns a number of threads and runs a number a test in them
  Each benchmark does the following for each of its iterations:
  1. takes the cache (this is to measure the effect of taking the cache)
  2. if kill switch is on it exits the loop
  3. if the cache is a valid reference it calls the listeners
  4. if the cache is a valid reference flushes it on every 10th repetition
  5. it sleeps for n_sleep if the cache is not a valid reference
*/
static DEFINE_BOOL_METHOD(mysql_test_ref_cache_benchmark_run,
                          (int threads, int reps, int sleep, int flush)) {
  long long n_threads = 100;
  long long n_reps = 100000;
  long long n_sleep = 500;
  long long n_flush = 10;
  kill_switch = false;

  if (threads) n_threads = threads;

  if (reps) n_reps = reps;

  if (sleep) n_sleep = sleep;

  if (flush) n_flush = flush;

  std::vector<std::thread> thds;
  for (long long i = 0; i < n_threads; i++)
    thds.push_back(std::thread([n_reps, n_sleep, n_flush]() {
      foo_cache *c = foo_cache::get_foo_cache();
      for (long long arg = 0; n_reps == 0 || arg < n_reps; arg++) {
        /* take again to measure the effect of fetching a populated cache */
        c = foo_cache::get_foo_cache();

        if (kill_switch.load(std::memory_order_relaxed)) break;

        if (c) {
          c->call(0 /* service_name_index */);
          if (n_flush && arg % n_flush == 0) c->flush();
        } else
          std::this_thread::sleep_for(std::chrono::milliseconds(n_sleep));
      }

      foo_cache::release_foo_cache();
    }));

  std::for_each(thds.begin(), thds.end(), [](std::thread &t) { t.join(); });
  return 0;
}

static DEFINE_BOOL_METHOD(mysql_test_ref_cache_benchmark_kill, ()) {
  kill_switch.store(true, std::memory_order_relaxed);
  return 0;
}

static mysql_service_status_t init() {
  const char *service_names[] = {"mysql_test_foo", nullptr};
  if (mysql_service_reference_caching_channel->create(service_names, &channel))
    channel = nullptr;
  return 0;
}

static mysql_service_status_t deinit() {
  if (channel != nullptr) {
    if (!mysql_service_reference_caching_channel->destroy(channel)) {
      channel = nullptr;
    }
  }
  return 0;
}

std::atomic<size_t> ctr;

static DEFINE_BOOL_METHOD(mysql_test_foo_emit, (int /*arg*/)) {
  ctr++;
  return false;
}

static DEFINE_BOOL_METHOD(mysql_test_ref_cache_consumer_counter_reset, ()) {
  ctr = 0;
  return false;
}

static DEFINE_BOOL_METHOD(mysql_test_ref_cache_consumer_counter_get, ()) {
  return ctr.load();
}

BEGIN_SERVICE_IMPLEMENTATION(test_reference_cache, mysql_test_foo)
mysql_test_foo_emit END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(test_reference_cache, test_ref_cache_producer)
mysql_test_ref_cache_produce_event, mysql_test_ref_cache_flush,
    mysql_test_ref_cache_release_cache, mysql_test_ref_cache_benchmark_run,
    mysql_test_ref_cache_benchmark_kill END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(test_reference_cache, test_ref_cache_consumer)
mysql_test_ref_cache_consumer_counter_reset,
    mysql_test_ref_cache_consumer_counter_get END_SERVICE_IMPLEMENTATION();

BEGIN_COMPONENT_PROVIDES(test_reference_cache)
PROVIDES_SERVICE(test_reference_cache, mysql_test_foo)
, PROVIDES_SERVICE(test_reference_cache, test_ref_cache_producer),
    PROVIDES_SERVICE(test_reference_cache, test_ref_cache_consumer),
    END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_METADATA(test_reference_cache)
METADATA("mysql.author", "Oracle Corporation")
, METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_reference_cache, "mysql:test_reference_cache")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_reference_cache)
    END_DECLARE_LIBRARY_COMPONENTS
