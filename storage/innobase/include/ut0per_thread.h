/* Copyright (c) 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#pragma once

#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "univ.i"

namespace ut {

/** Registry of per-thread values owned by one enclosing object.

This solves the case in which code needs one Value per participating thread,
but the values must follow the lifetime of a registry.
Plain thread_local is not enough, because thread-local objects outlive the
registry and are destroyed only when their threads exit. */
template <typename Value>
class Per_thread {
 public:
  /** Create an empty registry.

  @param[in] factory callable used to create the current thread's Value on the
  first access from that thread; must be non-empty; must not attempt calling the
  Per_thread::operator->() as it could lead to endless recursion. */
  explicit Per_thread(std::function<Value()> factory)
      : m_factory(std::move(factory)) {
    ut_a(static_cast<bool>(m_factory));
  }

  Per_thread(const Per_thread &) = delete;
  Per_thread &operator=(const Per_thread &) = delete;

  /** Return this thread's Value, creating and storing it on first access.

  Repeated calls from the same thread return the same object until the
  registry is destroyed.

  As thread ids might be reused, it is possible that a new thread "inherits" the
  Value which was used by an old thread, instead of starting with a fresh one.

  @return pointer to the Value owned by this registry for the calling thread */
  Value *operator->() {
    const auto tid = std::this_thread::get_id();
    {
      std::lock_guard<std::mutex> guard(m_mutex);

      auto it = m_map.find(tid);
      if (it != m_map.end()) {
        return &it->second;
      }
    }
    /* We know no other thread can add an entry to the map for our tid, so
    there's no need to do everything in one critical section. As m_factory()
    might be heavy or acquire some latches, it is also better for performance
    and deadlock-avoidance. */
    auto instance = m_factory();

    std::lock_guard<std::mutex> guard(m_mutex);
    auto [inserted_it, inserted] = m_map.emplace(tid, std::move(instance));
    ut_a(inserted);
    return &inserted_it->second;
  }

 private:
  std::function<Value()> m_factory;
  std::mutex m_mutex;
  std::unordered_map<std::thread::id, Value> m_map;
};

}  // namespace ut