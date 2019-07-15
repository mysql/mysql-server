/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/stdx/string_view.h"

#include <cctype>  // toupper

#include <gmock/gmock.h>

template <class RetT>
RetT get_string_abc();

template <>
const char *get_string_abc() {
  return "abc";
}

template <>
const wchar_t *get_string_abc() {
  return L"abc";
}

template <>
const char16_t *get_string_abc() {
  return u"abc";
}

template <>
const char32_t *get_string_abc() {
  return U"abc";
}

template <>
stdx::wstring_view get_string_abc() {
  using namespace stdx::string_view_literals;
  return L"abc"_sv;
}

template <>
stdx::string_view get_string_abc() {
  using namespace stdx::string_view_literals;
  return "abc"_sv;
}
template <>
stdx::u16string_view get_string_abc() {
  using namespace stdx::string_view_literals;
  return u"abc"_sv;
}
template <>
stdx::u32string_view get_string_abc() {
  using namespace stdx::string_view_literals;
  return U"abc"_sv;
}

template <class RetT>
RetT get_string_empty();

template <>
const char *get_string_empty() {
  return "";
}

template <>
const wchar_t *get_string_empty() {
  return L"";
}

template <>
const char16_t *get_string_empty() {
  return u"";
}

template <>
const char32_t *get_string_empty() {
  return U"";
}

template <>
stdx::wstring_view get_string_empty() {
  using namespace stdx::string_view_literals;
  return L""_sv;
}

template <>
stdx::string_view get_string_empty() {
  using namespace stdx::string_view_literals;
  return ""_sv;
}

template <>
stdx::u16string_view get_string_empty() {
  using namespace stdx::string_view_literals;
  return u""_sv;
}

template <>
stdx::u32string_view get_string_empty() {
  using namespace stdx::string_view_literals;
  return U""_sv;
}

template <class RetT>
RetT get_string_aab();

template <>
const char *get_string_aab() {
  return "aab";
}

template <>
const wchar_t *get_string_aab() {
  return L"aab";
}

template <>
const char16_t *get_string_aab() {
  return u"aab";
}

template <>
const char32_t *get_string_aab() {
  return U"aab";
}

template <class RetT>
RetT get_string_aaab();

template <>
const char *get_string_aaab() {
  return "aaab";
}

template <>
const wchar_t *get_string_aaab() {
  return L"aaab";
}

template <>
const char16_t *get_string_aaab() {
  return u"aaab";
}

template <>
const char32_t *get_string_aaab() {
  return U"aaab";
}

template <class RetT>
RetT get_string_ba();

template <>
const char *get_string_ba() {
  return "ba";
}

template <>
const wchar_t *get_string_ba() {
  return L"ba";
}

template <>
const char16_t *get_string_ba() {
  return u"ba";
}

template <>
const char32_t *get_string_ba() {
  return U"ba";
}

template <class T>
class StringViewTest : public ::testing::Test {
 public:
  using value_type = typename T::value_type;
  using pointer = typename T::pointer;
  using string_type = std::basic_string<value_type>;
  using string_view_type = stdx::basic_string_view<value_type>;

  string_type empty_std_string() { return get_string_empty<pointer>(); }
  pointer empty_cstring() { return get_string_empty<pointer>(); }
  string_view_type empty_string_view() {
    return get_string_empty<string_view_type>();
  }

  string_type abc_std_string() { return get_string_abc<pointer>(); }
  pointer abc_cstring() { return get_string_abc<pointer>(); }
  string_view_type abc_string_view() {
    return get_string_abc<string_view_type>();
  }

  pointer aab() { return get_string_aab<pointer>(); }
  pointer aaab() { return get_string_aaab<pointer>(); }
  pointer ba() { return get_string_ba<pointer>(); }
};

using StringViewTestTypes =
    ::testing::Types<stdx::string_view, stdx::wstring_view,
                     stdx::u16string_view, stdx::u32string_view>;

TYPED_TEST_CASE(StringViewTest, StringViewTestTypes);

TYPED_TEST(StringViewTest, construct_default) {
  TypeParam sv;

  EXPECT_TRUE(sv.empty());
  EXPECT_EQ(sv.size(), 0);
  EXPECT_EQ(sv.data(), nullptr);
}

TYPED_TEST(StringViewTest, construct_from_empty_std_string) {
  typename TestFixture::string_type s;

  TypeParam sv(s);

  EXPECT_TRUE(sv.empty());
  EXPECT_EQ(sv.size(), 0);
  EXPECT_NE(sv.data(), nullptr);
}

TYPED_TEST(StringViewTest, construct_from_std_string) {
  using string_type = typename TestFixture::string_type;

  auto abc = this->abc_std_string();
  string_type s(abc);

  // .size()
  TypeParam sv(s);

  EXPECT_FALSE(sv.empty());
  EXPECT_EQ(sv.size(), 3);
  EXPECT_EQ(sv.data(), abc);
}

TYPED_TEST(StringViewTest, construct_cstring) {
  auto abc = this->abc_cstring();
  // strlen
  TypeParam sv(abc);

  EXPECT_FALSE(sv.empty());
  EXPECT_EQ(sv.size(), 3);
  EXPECT_THAT(sv, ::testing::ElementsAre(abc[0], abc[1], abc[2]));
}

TYPED_TEST(StringViewTest, construct_from_string_view_literal) {
  auto abc = this->abc_cstring();
  // knows the size, no strlen()
  TypeParam sv(this->abc_string_view());

  EXPECT_FALSE(sv.empty());
  EXPECT_EQ(sv.size(), 3);
  EXPECT_THAT(sv, ::testing::ElementsAre(abc[0], abc[1], abc[2]));
}

TYPED_TEST(StringViewTest, op_ndx) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  EXPECT_EQ(sv[0], abc[0]);
  EXPECT_EQ(sv[1], abc[1]);
  EXPECT_EQ(sv[2], abc[2]);
}

TYPED_TEST(StringViewTest, at) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  EXPECT_EQ(sv.at(0), abc[0]);
  EXPECT_EQ(sv.at(1), abc[1]);
  EXPECT_EQ(sv.at(2), abc[2]);
}

TYPED_TEST(StringViewTest, front) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  EXPECT_EQ(sv.front(), abc[0]);
}

TYPED_TEST(StringViewTest, back) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  EXPECT_EQ(sv.back(), abc[2]);
}

TYPED_TEST(StringViewTest, clear) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  EXPECT_FALSE(sv.empty());

  sv.clear();
  EXPECT_TRUE(sv.empty());
}

TYPED_TEST(StringViewTest, length) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  EXPECT_FALSE(sv.empty());
  EXPECT_EQ(sv.size(), 3);
  EXPECT_EQ(sv.length(), 3);

  sv.clear();
  EXPECT_TRUE(sv.empty());
  EXPECT_EQ(sv.size(), 0);
  EXPECT_EQ(sv.length(), 0);
}

TYPED_TEST(StringViewTest, remove_prefix) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  sv.remove_prefix(1);

  EXPECT_EQ(sv.size(), 2);
  EXPECT_THAT(sv, ::testing::ElementsAre(abc[1], abc[2]));
}

TYPED_TEST(StringViewTest, remove_suffix) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  sv.remove_suffix(1);

  EXPECT_EQ(sv.size(), 2);
  EXPECT_THAT(sv, ::testing::ElementsAre(abc[0], abc[1]));
}

TYPED_TEST(StringViewTest, substr_all) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  auto sub = sv.substr();

  // input is unchanged
  EXPECT_EQ(sv.size(), 3);
  EXPECT_THAT(sv, ::testing::ElementsAre(abc[0], abc[1], abc[2]));

  // substr
  EXPECT_EQ(sub.size(), 3);
  EXPECT_THAT(sub, ::testing::ElementsAre(abc[0], abc[1], abc[2]));
}

TYPED_TEST(StringViewTest, substr_pos_all) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  auto sub = sv.substr(1);

  // input is unchanged
  EXPECT_EQ(sv.size(), 3);
  EXPECT_THAT(sv, ::testing::ElementsAre(abc[0], abc[1], abc[2]));

  // substr
  EXPECT_EQ(sub.size(), 2);
  EXPECT_THAT(sub, ::testing::ElementsAre(abc[1], abc[2]));
}

TYPED_TEST(StringViewTest, substr) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  auto sub = sv.substr(1, 1);

  // input is unchanged
  EXPECT_EQ(sv.size(), 3);
  EXPECT_THAT(sv, ::testing::ElementsAre(abc[0], abc[1], abc[2]));

  // substr
  EXPECT_EQ(sub.size(), 1);
  EXPECT_THAT(sub, ::testing::ElementsAre(abc[1]));
}

TYPED_TEST(StringViewTest, iter) {
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  size_t ndx{};
  for (auto c : sv) {
    EXPECT_EQ(c, abc[ndx++]);
  }
}

// reverse iterator adaptor for range-for-loops
template <class T>
struct reverser {
  T &iterable;
};

template <class T>
constexpr auto begin(reverser<T> w) {
  return std::rbegin(w.iterable);
}

template <class T>
constexpr auto end(reverser<T> w) {
  return std::rend(w.iterable);
}

template <class T>
constexpr reverser<T> reverse(T &&iterable) noexcept {
  return {iterable};
}

TYPED_TEST(StringViewTest, reverse_iter) {
  auto abc = this->abc_cstring();

  TypeParam sv(abc);

  size_t ndx{TypeParam::traits_type::length(abc) - 1};
  for (auto c : reverse(sv)) {
    EXPECT_EQ(c, abc[ndx--]);
  }
}

TYPED_TEST(StringViewTest, find_found_overlap) {
  TypeParam sv(this->aaab());

  EXPECT_EQ(sv.find(this->aab()), 1);
}

TYPED_TEST(StringViewTest, find_found_full_match) {
  TypeParam sv(this->aaab());

  EXPECT_EQ(sv.find(this->aaab()), 0);
}

TYPED_TEST(StringViewTest, find_no_match) {
  TypeParam sv(this->aaab());

  EXPECT_EQ(sv.find(this->ba()), stdx::string_view::npos);
}

TYPED_TEST(StringViewTest, find_empty) {
  TypeParam sv(this->aaab());

  EXPECT_EQ(sv.find(this->empty_string_view()), 0);
}

TYPED_TEST(StringViewTest, find_empty_in_empty) {
  TypeParam sv(this->empty_string_view());

  EXPECT_EQ(sv.find(get_string_empty<TypeParam>()), 0);
}

TYPED_TEST(StringViewTest, find_empty_in_empty_out_of_range) {
  TypeParam sv(this->empty_string_view());

  EXPECT_EQ(sv.find(this->empty_string_view(), 25), TypeParam::npos);
}

TYPED_TEST(StringViewTest, find_pos_out_of_range) {
  TypeParam sv(this->abc_string_view());

  EXPECT_EQ(sv.find(this->empty_string_view(), 25), TypeParam::npos);
}

TYPED_TEST(StringViewTest, to_string) {
  auto abc = this->abc_cstring();

  TypeParam sv(abc);

  EXPECT_EQ(typename TestFixture::string_type(sv), abc);
}

TYPED_TEST(StringViewTest, to_ostream) {
  std::basic_ostringstream<typename TypeParam::value_type> os;
  auto abc = this->abc_std_string();

  TypeParam sv(abc);

  os << sv;

  EXPECT_EQ(os.str(), abc);
}

template <class A, class B>
void compare_string_view(A a, B b, int comp_res) {
  EXPECT_EQ(a == b, comp_res == 0);
  EXPECT_EQ(a != b, comp_res != 0);
  EXPECT_EQ(a > b, comp_res == 1);
  EXPECT_EQ(a <= b, comp_res != 1);
  EXPECT_EQ(a < b, comp_res == -1);
  EXPECT_EQ(a >= b, comp_res != -1);
}

TYPED_TEST(StringViewTest, comp_sv_sv) {
  using string_view_type = typename TestFixture::string_view_type;
  using other_type = string_view_type;

  compare_string_view<string_view_type, other_type>(this->aab(), this->aab(),
                                                    0);

  compare_string_view<string_view_type, other_type>(this->abc_cstring(),
                                                    this->aab(), 1);

  compare_string_view<other_type, string_view_type>(this->aab(),
                                                    this->abc_cstring(), -1);
}

TYPED_TEST(StringViewTest, comp_s_sv) {
  using string_view_type = typename TestFixture::string_view_type;
  using other_type = typename TestFixture::pointer;

  compare_string_view<string_view_type, other_type>(this->aab(), this->aab(),
                                                    0);

  compare_string_view<string_view_type, other_type>(this->abc_cstring(),
                                                    this->aab(), 1);

  compare_string_view<other_type, string_view_type>(this->aab(),
                                                    this->abc_cstring(), -1);
}

TYPED_TEST(StringViewTest, comp_cstring_sv) {
  using string_view_type = typename TestFixture::string_view_type;
  using other_type = typename TestFixture::pointer;

  compare_string_view<string_view_type, other_type>(this->aab(), this->aab(),
                                                    0);

  compare_string_view<string_view_type, other_type>(this->abc_cstring(),
                                                    this->aab(), 1);

  compare_string_view<other_type, string_view_type>(this->aab(),
                                                    this->abc_cstring(), -1);
}

TYPED_TEST(StringViewTest, impl_length) {
  EXPECT_EQ(stdx::impl::char_traits_length(this->aab()), 3);
  EXPECT_EQ(stdx::impl::char_traits_length(this->aaab()), 4);
  EXPECT_EQ(stdx::impl::char_traits_length(this->ba()), 2);
}

TYPED_TEST(StringViewTest, impl_compare_aab_aaab_0) {
  EXPECT_EQ(stdx::impl::char_traits_compare(this->aab(), this->aaab(), 0), 0);
}

TYPED_TEST(StringViewTest, impl_compare_aab_aaab_1) {
  EXPECT_EQ(stdx::impl::char_traits_compare(this->aab(), this->aaab(), 1), 0);
}

TYPED_TEST(StringViewTest, impl_compare_aab_aaab_2) {
  EXPECT_EQ(stdx::impl::char_traits_compare(this->aab(), this->aaab(), 2), 0);
}

TYPED_TEST(StringViewTest, impl_compare_aab_aaab_3) {
  EXPECT_EQ(stdx::impl::char_traits_compare(this->aab(), this->aaab(), 3), 1);
}

TYPED_TEST(StringViewTest, impl_compare_aaab_aab_3) {
  EXPECT_EQ(stdx::impl::char_traits_compare(this->aaab(), this->aab(), 3), -1);
}

TYPED_TEST(StringViewTest, impl_memmatch) {
  auto aab = this->aab();
  auto aaab = this->aaab();

  EXPECT_EQ(stdx::impl::memmatch(aaab, 4, aaab, 4), aaab);
  EXPECT_EQ(stdx::impl::memmatch(aaab, 4, aab, 3), aaab + 1);
  EXPECT_EQ(stdx::impl::memmatch(aab, 3, aaab, 4), nullptr);
}

TYPED_TEST(StringViewTest, hash) {
  std::map<TypeParam, int> a_map{{this->abc_string_view(), 1}};
}

struct ci_char_traits : public std::char_traits<char> {
  static char to_upper(char ch) { return std::toupper((unsigned char)ch); }

  // is used by impl::find
  static bool eq(char c1, char c2) { return to_upper(c1) == to_upper(c2); }

  // is used by impl::compare
  static bool lt(char c1, char c2) { return to_upper(c1) < to_upper(c2); }
};

TEST(CaseInsensitiveStringViewTest, find) {
  using ci_string_view = stdx::basic_string_view<char, ci_char_traits>;

  EXPECT_EQ(ci_string_view("abc", 3).find("BC"), 1);
}

TEST(CaseInsensitiveStringViewTest, compare) {
  using ci_string_view = stdx::basic_string_view<char, ci_char_traits>;

  EXPECT_EQ(ci_string_view("abc", 3).compare("ABC"), 0);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
