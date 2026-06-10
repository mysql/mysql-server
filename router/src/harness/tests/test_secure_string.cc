/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gtest/gtest.h>

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "secure_string.h"  // NOLINT(build/include_subdir)

namespace mysql_harness {
namespace {

template <typename T>
std::string to_hex(T t) {
  std::stringstream ss;
  ss << std::hex << std::showbase << t;
  return ss.str();
}

template <typename T>
void EXPECT_ZEROS(const T *p, std::size_t n) {
  static constexpr T kNull{};

  for (std::size_t i = 0; i < n; ++i) {
    SCOPED_TRACE("Pointer: " + to_hex<const void *>(p) + ", index: " +
                 std::to_string(i) + ", value: " + to_hex<int>(p[i]));
    EXPECT_EQ(kNull, p[i]);
  }
}

template <typename T>
using Rebind =
    std::allocator_traits<SecureString::allocator_type>::rebind_alloc<T>;

template <typename CharT>
class TestSecureStringWithType;

template <typename T>
class TestAllocator : public Rebind<T> {
 private:
  using BaseAllocator = Rebind<T>;

 public:
  using Rebind<T>::Rebind;

  template <class U>
  struct rebind {
    using other = TestAllocator<U>;
  };

  BaseAllocator::value_type *allocate(std::size_t n) {
    TestSecureStringWithType<T>::allocated_ += n;
    return BaseAllocator::allocate(n);
  }

  void deallocate(BaseAllocator::value_type *p, std::size_t n) {
    EXPECT_ZEROS(p, n);

    BaseAllocator::deallocate(p, n);

    TestSecureStringWithType<T>::allocated_ -= n;
  }
};

template <typename CharT>
class TestSecureStringWithType : public testing::Test {
 protected:
  using value_type = CharT;
  using traits_type = std::char_traits<value_type>;
  using allocator_type = TestAllocator<value_type>;

  using SecureString_t =
      detail::SecureString<value_type, traits_type, allocator_type>;

  using string = std::basic_string<value_type>;

  using string_view = std::basic_string_view<value_type, traits_type>;

  static constexpr string_view sv(const SecureString_t &ss) {
    return {ss.c_str(), ss.length()};
  }

  static void EXPECT_VALUE(const SecureString_t &ss, const string &expected) {
    static constexpr value_type kNull{};

    SCOPED_TRACE("Expected: " + expected + ", actual: " + string(sv(ss)));

    EXPECT_EQ(expected.empty(), ss.empty());
    EXPECT_EQ(expected.length(), ss.length());
    EXPECT_EQ(expected.size(), ss.size());
    EXPECT_EQ(expected, sv(ss));
    EXPECT_EQ(kNull, ss.c_str()[ss.length()]);
  }

  static void EXPECT_EMPTY(const SecureString_t &ss) { EXPECT_VALUE(ss, {}); }

 private:
  friend class TestAllocator<value_type>;

  static constexpr value_type test_pattern() {
    value_type v{};

    // we set 4 bits per byte
    for (std::size_t i = 0; i < 4 * sizeof(value_type); ++i) {
      v <<= 2;
      v |= 1;
    }

    return v;
  }

  void SetUp() override { allocated_ = 0; }

  void TearDown() override { EXPECT_EQ(0, allocated_); }

  static std::size_t allocated_;
};

template <typename CharT>
std::size_t TestSecureStringWithType<CharT>::allocated_;

using TestSecureString = TestSecureStringWithType<SecureString::value_type>;

TEST_F(TestSecureString, default_constructor) {
  SecureString_t ss;

  EXPECT_EMPTY(ss);
}

TEST_F(TestSecureString, allocator_constructor) {
  SecureString_t ss{allocator_type{}};

  EXPECT_EMPTY(ss);
}

class TestSecureStringP : public TestSecureString,
                          public testing::WithParamInterface<std::size_t> {
 protected:
  using TestSecureString::EXPECT_VALUE;

  void EXPECT_VALUE(const SecureString_t &ss) { EXPECT_VALUE(ss, expected_); }

  string expected_ = string(GetParam(), test_pattern());
  string_view expected_v_ = expected_;

  string unexpected_ =
      string(GetParam(), static_cast<value_type>(test_pattern() << 1));
  string_view unexpected_v_ = unexpected_;

 private:
  static constexpr value_type test_pattern() {
    value_type v{};

    // we set 4 bits per byte
    for (std::size_t i = 0; i < 4 * sizeof(value_type); ++i) {
      v <<= 2;
      v |= 1;
    }

    return v;
  }
};

TEST_P(TestSecureStringP, pointer_constructor) {
  string s{expected_v_};
  SecureString_t ss{s.data(), s.length()};

  EXPECT_ZEROS(s.c_str(), s.length());
  EXPECT_VALUE(ss);
}

TEST_P(TestSecureStringP, range_constructor) {
  string s{expected_v_};
  std::vector<value_type> v{s.begin(), s.end()};
  SecureString_t ss{v.begin(), v.end()};

  EXPECT_ZEROS(v.data(), s.size());
  EXPECT_VALUE(ss);
}

TEST_P(TestSecureStringP, rvalue_string_constructor) {
  string s{expected_v_};
  SecureString_t ss{std::move(s)};

  EXPECT_TRUE(s.empty());
  EXPECT_VALUE(ss);
}

TEST_P(TestSecureStringP, copy_constructor) {
  string s{expected_v_};
  SecureString_t ss_1{s.data(), s.length()};
  SecureString_t ss_2{ss_1};

  EXPECT_VALUE(ss_1);
  EXPECT_VALUE(ss_2);
}

TEST_P(TestSecureStringP, move_constructor) {
  string s{expected_v_};
  SecureString_t ss_1{s.data(), s.length()};
  SecureString_t ss_2{std::move(ss_1)};

  EXPECT_EMPTY(ss_1);
  EXPECT_VALUE(ss_2);
}

TEST_P(TestSecureStringP, rvalue_string_assignment_operator) {
  string s_1{unexpected_v_};
  SecureString_t ss{s_1.data(), s_1.length()};

  string s_2{expected_v_};
  ss = std::move(s_2);

  EXPECT_TRUE(s_2.empty());
  EXPECT_VALUE(ss);
}

TEST_P(TestSecureStringP, copy_assignment_operator) {
  string s_1{unexpected_v_};
  SecureString_t ss_1{s_1.data(), s_1.length()};

  string s_2{expected_v_};
  SecureString_t ss_2{s_2.data(), s_2.length()};

  ss_1 = ss_2;

  EXPECT_VALUE(ss_1);
  EXPECT_VALUE(ss_2);
}

TEST_P(TestSecureStringP, move_assignment_operator) {
  string s_1{unexpected_v_};
  SecureString_t ss_1{s_1.data(), s_1.length()};

  string s_2{expected_v_};
  SecureString_t ss_2{s_2.data(), s_2.length()};

  ss_1 = std::move(ss_2);

  EXPECT_VALUE(ss_1);
  EXPECT_EMPTY(ss_2);
}

TEST_P(TestSecureStringP, swap) {
  string s_1{unexpected_v_};
  SecureString_t ss_1{s_1.data(), s_1.length()};

  string s_2{expected_v_};
  SecureString_t ss_2{s_2.data(), s_2.length()};

  ss_1.swap(ss_2);

  EXPECT_VALUE(ss_1);
  EXPECT_VALUE(ss_2, unexpected_);
}

TEST_P(TestSecureStringP, clear) {
  string s{expected_v_};
  SecureString_t ss{std::move(s)};

  ss.clear();

  EXPECT_EMPTY(ss);
}

TEST_P(TestSecureStringP, equals) {
  string s_1{unexpected_v_};
  SecureString_t ss_1{s_1.data(), s_1.length()};

  string s_2{expected_v_};
  SecureString_t ss_2{s_2.data(), s_2.length()};

  EXPECT_TRUE(ss_1 == ss_1);
  EXPECT_EQ(0 == GetParam(), ss_1 == ss_2);
  EXPECT_EQ(0 == GetParam(), ss_2 == ss_1);
  EXPECT_TRUE(ss_2 == ss_2);

  EXPECT_EQ(0 == GetParam(), ss_1 == SecureString_t{});
}

TEST_P(TestSecureStringP, not_equals) {
  string s_1{unexpected_v_};
  SecureString_t ss_1{s_1.data(), s_1.length()};

  string s_2{expected_v_};
  SecureString_t ss_2{s_2.data(), s_2.length()};

  EXPECT_FALSE(ss_1 != ss_1);
  EXPECT_EQ(0 != GetParam(), ss_1 != ss_2);
  EXPECT_EQ(0 != GetParam(), ss_2 != ss_1);
  EXPECT_FALSE(ss_2 != ss_2);

  EXPECT_EQ(0 != GetParam(), ss_1 != SecureString_t{});
}

INSTANTIATE_TEST_SUITE_P(StringLength, TestSecureStringP,
                         testing::Values(0, 1, 2, 32, 33, 64, 65),
                         testing::PrintToStringParamName());

}  // namespace
}  // namespace mysql_harness

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
