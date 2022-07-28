/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/stdx/flags.h"

#include <type_traits>

#include <gmock/gmock.h>
#include <gtest/gtest-typed-test.h>

enum class DefaultTypeEnum {
  Flag1 = 1 << 0,
  Flag2 = 1 << 7,
};

enum class IntTypeEnum : int {
  Flag1 = 1 << 0,
  Flag2 = 1 << 7,
};

enum class Uint8TypeEnum : uint8_t {
  Flag1 = 1 << 0,
  Flag2 = 1 << 7,
};

enum class Int8TypeEnum : int8_t {
  Flag1 = 1 << 0,
  Flag2 = -128,
};

enum class NotAFlagEnum {
  Flag1 = 1 << 0,
  Flag2 = 1 << 7,
};

static_assert(!stdx::is_flags<NotAFlagEnum>());

// mark all (except NotAFlagEnum) enum-types as 'flags'
namespace stdx {
template <>
struct is_flags<DefaultTypeEnum> : std::true_type {};

template <>
struct is_flags<IntTypeEnum> : std::true_type {};

template <>
struct is_flags<Uint8TypeEnum> : std::true_type {};

template <>
struct is_flags<Int8TypeEnum> : std::true_type {};
}  // namespace stdx

static_assert(stdx::is_flags<DefaultTypeEnum>());
static_assert(stdx::is_flags<IntTypeEnum>());
static_assert(stdx::is_flags<Uint8TypeEnum>());

template <class T>
class FlagsTest : public ::testing::Test {
 public:
  using enum_type = T;
  using flag_type = stdx::flags<enum_type>;

  static const auto flag1 = enum_type::Flag1;
  static const auto flag2 = enum_type::Flag2;

  static constexpr const flag_type flag_flag1{flag1};
  static constexpr const flag_type flag_flag2{flag2};

  static constexpr const auto underlying_1 =
      static_cast<std::underlying_type_t<enum_type>>(flag1);
  static constexpr const auto underlying_2 =
      static_cast<std::underlying_type_t<enum_type>>(flag2);
};

using FlagTypes =
    ::testing::Types<Uint8TypeEnum, DefaultTypeEnum, IntTypeEnum, Int8TypeEnum>;
TYPED_TEST_SUITE(FlagsTest, FlagTypes);

// check operator | works
TYPED_TEST(FlagsTest, Or) {
  const auto flag_flag1 = TestFixture::flag_flag1;
  const auto flag_flag2 = TestFixture::flag_flag2;
  const auto underlying_1 = TestFixture::underlying_1;
  const auto underlying_2 = TestFixture::underlying_2;
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  // different flags
  // flag-type <op> flag-type
  EXPECT_EQ((flag_flag1 | flag_flag2).underlying_value(),
            underlying_1 | underlying_2);

  // flag-type <op> flag
  EXPECT_EQ((flag_flag1 | flag2).underlying_value(),
            underlying_1 | underlying_2);

  // flag <op> flag
  EXPECT_EQ((flag1 | flag2).underlying_value(), underlying_1 | underlying_2);

  // explicit check
  EXPECT_EQ((flag_flag1 | flag2).underlying_value(),
            std::underlying_type_t<TypeParam>(0x81));

  // same flags
  // flag-type <op> flag-type
  EXPECT_EQ((flag_flag1 | flag_flag1).underlying_value(), underlying_1);

  // flag-type <op> flag
  EXPECT_EQ((flag_flag1 | flag1).underlying_value(), underlying_1);

  // explicit check
  EXPECT_EQ((flag_flag1 | flag1).underlying_value(), 1);
}

// check operator & works
TYPED_TEST(FlagsTest, And) {
  const auto flag_flag1 = TestFixture::flag_flag1;
  const auto flag_flag2 = TestFixture::flag_flag2;
  const auto underlying_1 = TestFixture::underlying_1;
  const auto underlying_2 = TestFixture::underlying_2;
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  // different flags
  // flag-type <op> flag-type
  EXPECT_EQ((flag_flag1 & flag_flag2).underlying_value(),
            underlying_1 & underlying_2);

  // flag-type <op> flag
  EXPECT_EQ((flag_flag1 & flag2).underlying_value(),
            underlying_1 & underlying_2);

  // flag <op> flag
  EXPECT_EQ((flag1 & flag2).underlying_value(), underlying_1 & underlying_2);

  // explicit check
  EXPECT_EQ((flag_flag1 & flag2).underlying_value(), 0);

  // same flags
  // flag-type <op> flag-type
  EXPECT_EQ((flag_flag1 & flag_flag1).underlying_value(), underlying_1);

  // flag-type <op> flag
  EXPECT_EQ((flag_flag1 & flag1).underlying_value(), underlying_1);

  // explicit check
  EXPECT_EQ((flag_flag1 & flag1).underlying_value(), 1);
}

// check operator ^ works
TYPED_TEST(FlagsTest, Xor) {
  const auto flag_flag1 = TestFixture::flag_flag1;
  const auto flag_flag2 = TestFixture::flag_flag2;
  const auto underlying_1 = TestFixture::underlying_1;
  const auto underlying_2 = TestFixture::underlying_2;
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  // different flags
  // flag-type <op> flag-type
  EXPECT_EQ((flag_flag1 ^ flag_flag2).underlying_value(),
            underlying_1 ^ underlying_2);

  // flag-type <op> flag
  EXPECT_EQ((flag_flag1 ^ flag2).underlying_value(),
            underlying_1 ^ underlying_2);

  // flag <op> flag
  EXPECT_EQ((flag1 ^ flag2).underlying_value(), underlying_1 ^ underlying_2);

  // explicit check
  EXPECT_EQ((flag_flag1 ^ flag2).underlying_value(),
            std::underlying_type_t<TypeParam>(0x81));

  // same flags
  // flag-type <op> flag-type
  EXPECT_EQ((flag_flag1 ^ flag_flag1).underlying_value(), 0);

  // flag-type <op> flag
  EXPECT_EQ((flag_flag1 ^ flag1).underlying_value(), 0);

  // explicit check
  EXPECT_EQ((flag_flag1 ^ flag1).underlying_value(), 0);
}

TYPED_TEST(FlagsTest, Assign) {
  const auto flag_flag1 = TestFixture::flag_flag1;
  const auto flag_flag2 = TestFixture::flag_flag2;
  const auto underlying_1 = TestFixture::underlying_1;
  const auto underlying_2 = TestFixture::underlying_2;
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  using flag_type = typename TestFixture::flag_type;

  // with flag-types
  {
    flag_type flag = flag_flag1;

    EXPECT_EQ(flag.underlying_value(), underlying_1);

    flag = flag_flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_2);
  }

  // with enum-types

  {
    flag_type flag = flag1;

    EXPECT_EQ(flag.underlying_value(), underlying_1);

    flag = flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_2);
  }
}

TYPED_TEST(FlagsTest, OrAssign) {
  const auto flag_flag1 = TestFixture::flag_flag1;
  const auto flag_flag2 = TestFixture::flag_flag2;
  const auto underlying_1 = TestFixture::underlying_1;
  const auto underlying_2 = TestFixture::underlying_2;
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  using flag_type = typename TestFixture::flag_type;

  // with flag-types
  {
    flag_type flag{flag1};

    flag |= flag_flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_1 | underlying_2);
  }

  {
    flag_type flag{flag1 | flag2};

    flag |= flag_flag1;

    EXPECT_EQ(flag.underlying_value(), underlying_1 | underlying_2);
  }

  {
    flag_type flag;

    flag |= flag_flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_2);
  }

  // with enum-types

  {
    flag_type flag{flag1};

    flag |= flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_1 | underlying_2);
  }

  {
    flag_type flag{flag1 | flag2};

    flag |= flag1;

    EXPECT_EQ(flag.underlying_value(), underlying_1 | underlying_2);
  }

  {
    flag_type flag;

    flag |= flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_2);
  }
}

TYPED_TEST(FlagsTest, AndAssign) {
  const auto flag_flag1 = TestFixture::flag_flag1;
  const auto flag_flag2 = TestFixture::flag_flag2;
  const auto underlying_1 = TestFixture::underlying_1;
  const auto underlying_2 = TestFixture::underlying_2;
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  using flag_type = typename TestFixture::flag_type;

  // with flag-types
  {
    flag_type flag{flag1 | flag2};

    flag &= flag_flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_2);
  }

  {
    flag_type flag{flag1 | flag2};

    flag &= flag_flag1;

    EXPECT_EQ(flag.underlying_value(), underlying_1);
  }

  {
    flag_type flag;

    flag &= flag_flag2;

    EXPECT_EQ(flag.underlying_value(), 0);
  }

  // with enum-types
  {
    flag_type flag{flag1 | flag2};

    flag &= flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_2);
  }

  {
    flag_type flag{flag1 | flag2};

    flag &= flag1;

    EXPECT_EQ(flag.underlying_value(), underlying_1);
  }

  {
    flag_type flag;

    flag &= flag2;

    EXPECT_EQ(flag.underlying_value(), 0);
  }
}

TYPED_TEST(FlagsTest, XorAssign) {
  const auto flag_flag2 = TestFixture::flag_flag2;
  const auto underlying_1 = TestFixture::underlying_1;
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  using flag_type = typename TestFixture::flag_type;

  {
    flag_type flag{flag1 | flag2};

    flag ^= flag_flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_1);
  }

  {
    flag_type flag{flag1 | flag2};

    flag ^= flag2;

    EXPECT_EQ(flag.underlying_value(), underlying_1);
  }
}

// check operator ~ works
TYPED_TEST(FlagsTest, Neg) {
  const auto flag_flag1 = TestFixture::flag_flag1;
  const auto underlying_1 = TestFixture::underlying_1;

  using flag_type = typename TestFixture::flag_type;

  EXPECT_EQ((~flag_type{flag_flag1}).underlying_value(),
            std::underlying_type_t<TypeParam>(~underlying_1));
}

// check operator ~ works
TYPED_TEST(FlagsTest, Not) {
  const auto flag_flag1 = TestFixture::flag_flag1;
  const auto underlying_1 = TestFixture::underlying_1;

  using flag_type = typename TestFixture::flag_type;

  EXPECT_EQ((!flag_type{flag_flag1}), !underlying_1);
}

// check operator bool() works
TYPED_TEST(FlagsTest, Bool) {
  using flag_type = typename TestFixture::flag_type;

  EXPECT_TRUE(flag_type{TypeParam::Flag1});
  EXPECT_FALSE(flag_type{});
}

TYPED_TEST(FlagsTest, InitializerList) {
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  using flag_type = typename TestFixture::flag_type;

  flag_type one_flags{flag1};
  EXPECT_EQ(one_flags.underlying_value(), 1);

  flag_type two_flags{flag1 | flag2};
  EXPECT_EQ(two_flags.underlying_value(),
            std::underlying_type_t<TypeParam>(0x81));
}

// check iterators works
TYPED_TEST(FlagsTest, count) {
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  using flag_type = typename TestFixture::flag_type;

  flag_type two_flags{flag1 | flag2};

  EXPECT_EQ(two_flags.count(), 2);
}

TYPED_TEST(FlagsTest, count_constexpr) {
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  using flag_type = typename TestFixture::flag_type;

  static_assert(flag_type{flag1 | flag2}.count() == 2);
}

TYPED_TEST(FlagsTest, reset) {
  const auto flag1 = TestFixture::flag1;
  const auto flag2 = TestFixture::flag2;

  using flag_type = typename TestFixture::flag_type;

  flag_type two_flags{flag1 | flag2};

  two_flags.reset();

  EXPECT_EQ(two_flags.count(), 0);
}

TYPED_TEST(FlagsTest, count_empty) {
  using flag_type = typename TestFixture::flag_type;

  flag_type no_flags;

  EXPECT_EQ(no_flags.count(), 0);

  // after reset, still nothing set
  no_flags.reset();

  EXPECT_EQ(no_flags.count(), 0);
}

TYPED_TEST(FlagsTest, size) {
  using flag_type = typename TestFixture::flag_type;

  static_assert(flag_type{}.size() ==
                8 * sizeof(std::underlying_type_t<TypeParam>));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
