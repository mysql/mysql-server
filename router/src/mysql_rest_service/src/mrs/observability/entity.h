/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTITY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTITY_H_

#include <atomic>
#include <cassert>
#include <shared_mutex>

namespace mrs {
namespace observability {

class Common {
 public:
  static inline std::shared_mutex mutex;
};

class Entity : public Common {
 public:
  virtual ~Entity() = default;

  virtual uint64_t get_id() const = 0;
  virtual std::string get_name() const = 0;
  virtual uint64_t get_value_and_reset() = 0;
};

template <uint64_t counter_id>
class EntityWithId : public Entity {
 public:
  EntityWithId() { assert(!registred_.test_and_set()); }

 protected:
#ifndef NDEBUG
  static inline std::atomic_flag registred_;
#endif
};

template <uint64_t counter_id>
class EntityCounter : public EntityWithId<counter_id> {
 public:
  static void increment(int32_t inc = 1) {
    auto lock = std::shared_lock<std::shared_mutex>(Common::mutex);
    value_ += inc;
  }

  EntityCounter(const std::string &name) : name_{name} {}

  uint64_t get_id() const override { return counter_id; }
  std::string get_name() const override { return name_; }
  uint64_t get_value_and_reset() override { return value_.exchange(0); }

 protected:
  std::string name_;
  static inline std::atomic<uint64_t> value_;
};

template <uint64_t counter_id>
class EntityCounterNotResetable : public EntityCounter<counter_id> {
 public:
  using Parent = EntityCounter<counter_id>;
  using Parent::EntityCounter;
  uint64_t get_value_and_reset() override { return Parent::value_.load(); }
};

template <uint64_t counter_id>
class EntityAverageInt : public EntityCounter<counter_id> {
 public:
  using Parent = EntityCounter<counter_id>;
  using Parent::EntityCounter;

  static void increment(int32_t inc = 1) {
    auto lock = std::shared_lock<std::shared_mutex>(Common::mutex);
    Parent::value_ += inc;
    ++count_;
  }

  uint64_t get_value_and_reset() override {
    return Parent::value_.exchange(0) / count_.exchange(0);
  }

 protected:
  static inline std::atomic<uint64_t> count_;
};

}  // namespace observability

template <uint64_t counter_id>
using Counter = observability::EntityCounter<counter_id>;

template <uint64_t counter_id>
using Average = observability::EntityAverageInt<counter_id>;

}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTITY_H_
