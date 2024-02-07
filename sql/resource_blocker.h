#ifndef RPL_RESOURCE_BLOCKER_H_INCLUDED
#define RPL_RESOURCE_BLOCKER_H_INCLUDED

/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <string.h>       // std::string
#include <set>            // std::set
#include "map_helpers.h"  // std::atomic
#include "mutex_lock.h"   // Mutex_lock

namespace resource_blocker {

/// Represents a "Resource" which can be either "used" by a number of
/// threads, or "blocked" by a number of threads. When one or more
/// threads use the resource, no thread can block it. When a one or
/// more threads is blocking the resource, no thread can use it. Each
/// blocker provides a message that explains why the resource is
/// blocked.
///
/// The resource is blocked or used by creating instances of the
/// Blocker and User classes.
///
/// @code
/// Resource museum;
///
/// // If the museum is open, visit it. This may be called by multiple threads.
/// void visit_museum() {
///   User user(museum);
///   if (!user) return;
///   // walk around the museum
/// }
///
/// // If there are no guests in the museum, close it temporarily for
/// // maintenance.
/// void close_museum_for_maintenance() {
///   Blocker blocker(museum, "Museum is currently closed for maintenance.");
///   if (!blocker) return;
///   // repair exhibitions
/// }
///
/// // If there are no guests in the museum, close it temporarily for
/// // cleaning.
/// void close_museum_for_cleaning() {
///   Blocker blocker(museum, "Museum is currently closed for cleaning.");
///   if (!blocker) return;
///   // clean the floors
/// }
/// @endcode
class Resource {
 public:
  using String_set = std::set<std::string>;

  Resource() : m_user_count(0), m_initialized(0) {
    mysql_mutex_init(PSI_INSTRUMENT_ME, &m_lock, MY_MUTEX_INIT_FAST);
  }

  ~Resource() {
    assert(m_user_count == 0);
    assert(m_block_reasons.empty());
    mysql_mutex_destroy(&get_lock());
  }

  Resource(Resource &other) = delete;
  Resource(Resource &&other) = delete;
  Resource &operator=(Resource const &other) = delete;
  Resource &operator=(Resource &&other) = delete;

 private:
  /// Try to block the resource from being used, for the given reason.
  [[nodiscard]] int try_block(const std::string &reason) {
    MUTEX_LOCK(guard, &get_lock());
    if (m_user_count == 0) {
      m_block_reasons.insert(reason);
    }
    return m_user_count;
  }

  /// Unblock the resource if try_block was previously called successfully.
  void end_block(const std::string &reason) {
    MUTEX_LOCK(guard, &get_lock());
    [[maybe_unused]] auto removed_count = m_block_reasons.erase(reason);
    assert(removed_count == 1);
  }

  /// Try to start using the resource.
  [[nodiscard]] String_set try_use() {
    MUTEX_LOCK(guard, &get_lock());
    if (m_block_reasons.empty()) {
      ++m_user_count;
    }
    //++m_user_count;
    return m_block_reasons;
  }

  /// Stop using the resource if try_use was previously called successfully.
  void end_use() {
    MUTEX_LOCK(guard, &get_lock());
    assert(m_user_count > 0);
    --m_user_count;
  }

  mysql_mutex_t &get_lock() { return m_lock; }

  String_set m_block_reasons;
  int m_user_count;
  std::atomic<int> m_initialized;
  mutable mysql_mutex_t m_lock;

  friend class User;
  friend class Blocker;
};  // class Resource

// RAII class that attempts to use a Resource. When an instance is
// constructed, it checks if the Resource is blocked; if not, it holds
// the Resource in 'used' state during the instance life time.
class User {
 public:
  /// By default, does not use any Resource.
  User() : m_resource(nullptr) {}

  /// Attempt to start using the given Resource. This may fail, so the
  /// caller must check if it succeeded or not, using `operator bool`
  /// or `operator!`.
  explicit User(Resource &resource) {
    m_block_reasons = resource.try_use();
    if (m_block_reasons.empty()) {
      m_resource = &resource;
    } else {
      m_resource = nullptr;
    }
  }

  /// Use the same Resource that `other` uses, if any.
  User(User &other) : m_resource(nullptr) { *this = other; }

  /// Take over the Resource that `other` uses, if any.
  User(User &&other) noexcept : m_resource(nullptr) {
    *this = std::move(other);
  }

  /// Release our own Resource, if any, and then start using the
  /// Resource that `other` uses, if any.
  User &operator=(User const &other) {
    if (this == &other) {
      return *this;
    }
    end_use();
    m_resource = other.m_resource;
    m_block_reasons = other.m_block_reasons;
    if (m_resource != nullptr) {
      [[maybe_unused]] auto ret = m_resource->try_use();
    }
    return *this;
  }

  /// Release our own resource, if any, and then steal the Resource
  /// that `other` uses, if any, so that `other` will not use it any
  /// more.
  User &operator=(User &&other) noexcept {
    end_use();
    m_resource = other.m_resource;
    m_block_reasons = other.m_block_reasons;
    other.m_resource = nullptr;
    return *this;
  }

  /// Return true if we hold the Resource in 'used' state.
  explicit operator bool() { return m_resource != nullptr; }

  /// Return false if we hold the Resource in 'used' state.
  bool operator!() { return m_resource == nullptr; }

  /// Return the set of strings that explain the reasons why the
  /// resource was blocked.
  Resource::String_set block_reasons() { return m_block_reasons; }

  /// Stop holding the Resource in 'used' state, if we do.
  void end_use() {
    if (m_resource != nullptr) {
      m_resource->end_use();
      m_resource = nullptr;
    } else {
      m_block_reasons.clear();
    }
  }

  /// Stop holding the Resource in 'used' state, if we do.
  ~User() { end_use(); }

 private:
  Resource *m_resource{nullptr};
  Resource::String_set m_block_reasons;
};  // class User

// RAII class that attempts to block a Resource. When an instance is
// constructed, it checks if the Resource is used; if not, it blocks
// the Resource during the instance life time.
class Blocker {
 public:
  /// By default, does not block any Resource.
  Blocker() : m_resource(nullptr), m_user_count(0) {}

  /// Attempt to start using the given Resource. This may fail, so the
  /// caller must check if it succeeded or not, using `operator bool`
  /// or `operator!`.
  Blocker(Resource &resource, const std::string &reason)
      : m_resource(nullptr), m_user_count(resource.try_block(reason)) {
    if (m_user_count == 0) {
      m_resource = &resource;
      m_reason = reason;
    }
  }

  /// Block the same Resource that `other` blocks, if any.
  Blocker(Blocker &other) : m_resource(nullptr), m_user_count(0) {
    *this = other;
  }

  /// Take over the Resource that `other` blocks, if any.
  Blocker(Blocker &&other) noexcept : m_resource(nullptr), m_user_count(0) {
    *this = std::move(other);
  }

  /// Release our own Resource, if any, and then block the same Resource
  /// that `other` blocks, if any.
  Blocker &operator=(Blocker const &other) {
    if (this == &other) {
      return *this;
    }
    end_block();
    m_resource = other.m_resource;
    m_reason = other.m_reason;
    m_user_count = other.m_user_count;
    if (m_resource != nullptr) {
      [[maybe_unused]] auto ret = m_resource->try_block(m_reason);
    }
    return *this;
  }

  /// Release our own resource, if any, and then steal the Resource
  /// that `other` blocks, if any, so that `other` does not block
  /// it any more.
  Blocker &operator=(Blocker &&other) noexcept {
    end_block();
    m_resource = other.m_resource;
    m_reason = other.m_reason;
    m_user_count = other.m_user_count;
    other.m_resource = nullptr;
    other.m_reason = "";
    other.m_user_count = 0;
    return *this;
  }

  /// Return true if we block the Resource.
  explicit operator bool() { return m_resource != nullptr; }

  /// Return false if we block the Resource.
  bool operator!() { return m_resource == nullptr; }

  /// Return the number of users of the Resource at the time we tried
  /// to block it.
  int user_count() const { return m_user_count; }

  /// Stop blocking the Resource, if we do.
  void end_block() {
    if (m_resource != nullptr) {
      m_resource->end_block(m_reason);
      m_resource = nullptr;
      m_reason = "";
      m_user_count = 0;
    }
  }

  /// Stop holding the Resource in 'used' state, if we do.
  ~Blocker() { end_block(); }

 private:
  Resource *m_resource;
  std::string m_reason;
  int m_user_count;
};  // class Blocker

}  // namespace resource_blocker

#endif /* RPL_RESOURCE_BLOCKER_H_INCLUDED */
