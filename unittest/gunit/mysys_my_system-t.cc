/* Copyright (c) 2026 Oracle and/or its affiliates.

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

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <sstream>

#include "components/library_mysys/my_system_api/my_system_api.h"

#ifndef WIN32
namespace {
constexpr uint64_t kUnlimited = 0;
constexpr uint64_t kLimit4GiB = 4ULL * 1024 * 1024 * 1024;
constexpr uint64_t kLimit8GiB = 8ULL * 1024 * 1024 * 1024;

struct CgroupMemoryCase {
  std::optional<uint64_t> self;
  std::optional<uint64_t> root;
  std::optional<uint64_t> expected;
};
}  // namespace

namespace my_system_cgroup {
std::optional<uint64_t> get_cgroup_memory(
    const std::optional<uint64_t> &self_memory,
    const std::optional<uint64_t> &root_memory);
} /* namespace my_system_cgroup */

TEST(MysysMySystem, GetCgroupMemory_ChoosesExpectedLimit) {
  /*
    Spec:
      - (nullopt, nullopt) => nullopt
      - (x, nullopt) or (nullopt, x) => x
      - (a, b) =>
          if (a == 0 || b == 0) choose max(a, b)   // 0 means unlimited
          else choose min(a, b)
  */

  const CgroupMemoryCase cases[] = {
      /* (nullopt, nullopt) => nullopt */
      {std::nullopt, std::nullopt, std::nullopt},

      /* (nullopt, x) => x */
      {std::nullopt, kUnlimited, kUnlimited},
      {std::nullopt, kLimit4GiB, kLimit4GiB},
      {std::nullopt, kLimit8GiB, kLimit8GiB},

      /* (x, nullopt) => x */
      {kUnlimited, std::nullopt, kUnlimited},
      {kLimit4GiB, std::nullopt, kLimit4GiB},
      {kLimit8GiB, std::nullopt, kLimit8GiB},

      /* (a, b); if (a == 0 || b == 0) max(a,b) */
      {kUnlimited, kUnlimited, kUnlimited},
      {kUnlimited, kLimit4GiB, kLimit4GiB},
      {kLimit8GiB, kUnlimited, kLimit8GiB},

      /* (a, b); if (a != 0 && b != 0) min(a,b) */
      {kLimit4GiB, kLimit8GiB, kLimit4GiB},
      {kLimit8GiB, kLimit4GiB, kLimit4GiB},

      /* (a, a) => a */
      {kLimit8GiB, kLimit8GiB, kLimit8GiB},

      /* Boundary */
      {uint64_t{1}, uint64_t{2}, uint64_t{1}},
      {uint64_t{0}, uint64_t{1}, uint64_t{1}},
  };

  for (const auto &tc : cases) {
    const auto result = my_system_cgroup::get_cgroup_memory(tc.self, tc.root);

    if (!tc.expected.has_value()) {
      EXPECT_FALSE(result.has_value());
    } else {
      ASSERT_TRUE(result.has_value());
      EXPECT_EQ(tc.expected.value(), result.value());
    }
  }
}
#endif
