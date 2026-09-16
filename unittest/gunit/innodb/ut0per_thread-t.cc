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

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include "my_config.h"
#include "storage/innobase/include/univ.i"
#include "ut0per_thread.h"

namespace {

struct Value {
  explicit Value(int id_arg) : id(id_arg) {}

  int id;
};

struct Lifetime_value {
  explicit Lifetime_value(std::atomic<int> &destroyed_arg)
      : destroyed(&destroyed_arg) {}

  Lifetime_value(Lifetime_value &&other) noexcept : destroyed(other.destroyed) {
    other.destroyed = nullptr;
  }

  Lifetime_value &operator=(Lifetime_value &&other) noexcept {
    if (this != &other) {
      destroyed = other.destroyed;
      other.destroyed = nullptr;
    }
    return *this;
  }

  Lifetime_value(const Lifetime_value &) = delete;
  Lifetime_value &operator=(const Lifetime_value &) = delete;

  ~Lifetime_value() {
    if (destroyed != nullptr) {
      ++(*destroyed);
    }
  }

  std::atomic<int> *destroyed;
};

}  // namespace

TEST(ut0per_thread, reuses_same_value_within_thread) {
  std::atomic<int> next_id{0};
  ut::Per_thread<Value> per_thread([&next_id] { return Value(++next_id); });

  Value *first = per_thread.operator->();
  Value *second = per_thread.operator->();

  ASSERT_EQ(1, first->id);
  ASSERT_EQ(first, second);
}

TEST(ut0per_thread, creates_distinct_values_for_distinct_threads) {
  std::atomic<int> next_id{0};
  ut::Per_thread<Value> per_thread([&next_id] { return Value(++next_id); });

  int ids[2]{};

  std::thread first([&] { ids[0] = per_thread->id; });
  std::thread second([&] { ids[1] = per_thread->id; });

  first.join();
  second.join();

  ASSERT_EQ(next_id, 2);
  ASSERT_NE(ids[0], 0);
  ASSERT_NE(ids[1], 0);
  ASSERT_NE(ids[0], ids[1]);
}

TEST(ut0per_thread, destroys_thread_values_with_registry) {
  std::atomic<int> destroyed{0};

  {
    ut::Per_thread<Lifetime_value> per_thread(
        [&destroyed] { return Lifetime_value(destroyed); });

    ASSERT_NE(nullptr, per_thread.operator->());

    std::thread worker([&] { ASSERT_NE(nullptr, per_thread.operator->()); });
    worker.join();

    ASSERT_EQ(0, destroyed.load());
  }

  ASSERT_EQ(2, destroyed.load());
}