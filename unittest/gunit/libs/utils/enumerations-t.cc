// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>
#include "mysql/utils/enumeration_utils.h"

namespace mysql::utils {

enum class Enum1 : uint32_t { const_a = 0, const_b = 1, const_c = 2 };

template <>
constexpr inline Enum1 enum_max() {
  return Enum1::const_c;
}

TEST(Enumerations, Functions) {
  ASSERT_EQ(to_underlying(Enum1::const_a), 0);
  ASSERT_EQ(to_underlying(Enum1::const_b), 1);
  ASSERT_EQ(to_underlying(Enum1::const_c), 2);
  auto r1 = to_enumeration<Enum1>(0UL);
  auto r2 = to_enumeration<Enum1>(1UL);
  auto r3 = to_enumeration<Enum1>(2UL);
  auto r4 = to_enumeration<Enum1>(3UL);
  ASSERT_EQ(r1.first, Enum1::const_a);
  ASSERT_EQ(r2.first, Enum1::const_b);
  ASSERT_EQ(r3.first, Enum1::const_c);
  ASSERT_EQ(r4.first, Enum1::const_c);
  ASSERT_EQ(r1.second, Return_status::ok);
  ASSERT_EQ(r2.second, Return_status::ok);
  ASSERT_EQ(r3.second, Return_status::ok);
  ASSERT_EQ(r4.second, Return_status::error);
}

}  // namespace mysql::utils
