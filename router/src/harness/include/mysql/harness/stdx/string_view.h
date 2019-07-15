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

#ifndef MYSQL_HARNESS_STDX_STRING_VIEW_INCLUDED
#define MYSQL_HARNESS_STDX_STRING_VIEW_INCLUDED

#include <algorithm>   // copy_n
#include <cstddef>     // ptrdiff_t
#include <functional>  // hash
#include <ostream>
#include <stdexcept>  // out_of_range
#include <string>

namespace stdx {

// implementation of C++17's std::string_view on C++11:
//
// see http://wg21.link/N3762
//
// missing features:
//
// - most of the find_xxx() methods
// - padding support for ostream

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif

#if __has_cpp_attribute(gnu::nonnull)
#define STDX_NONNULL [[gnu::nonnull]]
#else
#define STDX_NONNULL
#endif

namespace impl {
// constexpr variant of std::char_traits<charT>::length()
template <class charT>
constexpr size_t char_traits_length(const charT *s) noexcept;

template <class charT>
constexpr size_t char_traits_length(const charT *s) noexcept {
  size_t len{};
  for (; *s; ++s, ++len)
    ;
  return len;
}

#if __has_builtin(__builtin_strlen) || defined(__GNUC__)
// strlen() isn't constexpr, but __builtin_strlen() is with GCC and clang.
// MSVC has __builtin_strlen() too ... but how to detect that?
template <>
constexpr size_t char_traits_length(const char *s) noexcept {
  return __builtin_strlen(s);
}
#endif

#if __has_builtin(__builtin_wcslen)
// wcslen() isn't constexpr, but __builtin_wcslen() is with clang.
template <>
constexpr size_t char_traits_length(const wchar_t *s) noexcept {
  return __builtin_wcslen(s);
}
#endif

// constexpr variant of c::std::char_traits<charT>::compare()
template <class charT, class traits = std::char_traits<charT>>
constexpr int char_traits_compare(const charT *a, const charT *b,
                                  size_t len) noexcept;

template <class charT, class traits>
STDX_NONNULL constexpr int char_traits_compare(const charT *a, const charT *b,
                                               size_t len) noexcept {
  for (size_t ndx{}; ndx < len; ++ndx) {
    if (traits::lt(a[ndx], b[ndx])) return -1;
    if (traits::lt(b[ndx], a[ndx])) return 1;
  }

  return 0;
}

#if __has_builtin(__builtin_memcmp) || defined(__GNUC__)
// in case of charT == char and and traits std::char_traits<char> we can
// optimize by falling back to __builtin_memcmp() if it exists
//
// compare() isn't constexpr, but __builtin_memcmp() is with GCC and clang.
// MSVC has __builtin_memcmp() too ... but how to detect that?
template <>
STDX_NONNULL constexpr int char_traits_compare<char, std::char_traits<char>>(
    const char *a, const char *b, size_t len) noexcept {
  if (len == 0) return 0;

  return __builtin_memcmp(a, b, len);
}
#endif

#if __has_builtin(__builtin_wmemcmp)
// in case of charT == wchar_t and and traits std::char_traits<wchar_t> we can
// optimize by falling back to __builtin_wmemcmp() if it exists
//
// compare() isn't constexpr, but __builtin_wmemcmp() is with clang.
template <>
STDX_NONNULL constexpr int
char_traits_compare<wchar_t, std::char_traits<wchar_t>>(const wchar_t *a,
                                                        const wchar_t *b,
                                                        size_t len) noexcept {
  if (len == 0) return 0;

  return __builtin_wmemcmp(a, b, len);
}
#endif

/**
 * find first occurence of needle in a haystack.
 *
 * @param haystack string of character to search needle in
 * @param haystack_len length of haystack in characters
 * @param needle string of characters for find in haystack
 * @param needle_len length of needle in characters
 * @returns pointer to the position where needle is found in the haystack
 * @retval nullptr if needle wasn't found
 */
template <class charT, class traits = std::char_traits<charT>>
STDX_NONNULL const charT *memmatch(const charT *haystack, size_t haystack_len,
                                   const charT *needle, size_t needle_len) {
  // a empty needle
  if (needle_len == 0) return haystack;

  const charT *haystack_end = haystack + haystack_len;
  const charT *needle_start = needle;
  const charT *needle_end = needle + needle_len;

  for (; haystack < haystack_end; ++haystack) {
    if (traits::eq(*haystack, *needle)) {
      if (++needle == needle_end) {
        // end of needle reached, all characters matched.
        return haystack + 1 - needle_len;
      }
    } else if (needle != needle_start) {
      // rewind haystack to before the needle which started the match
      haystack -= needle - needle_start;

      // reset the needle to its initial value
      needle = needle_start;
    }
  }

  return nullptr;
}

template <class T>
using identity = std::decay_t<T>;

}  // namespace impl

template <class charT, class traits = std::char_traits<charT>>
class basic_string_view {
 public:
  using traits_type = traits;
  using value_type = charT;
  using pointer = const value_type *;
  using const_pointer = const value_type *;
  using reference = const value_type &;
  using const_reference = const value_type &;
  using const_iterator = const value_type *;
  using iterator = const_iterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = const_reverse_iterator;

  using size_type = size_t;
  using difference_type = ptrdiff_t;
  // in C++17 we can do:
  //
  // static constexpr size_t npos = size_type(-1);
  //
  // which needs a definition in a .cc file. Using a enum should do the trick
  // too in C++11 without a .cc file.
  enum : size_type { npos = size_type(-1) };

  constexpr basic_string_view() noexcept : ptr_(nullptr), length_(0) {}
  constexpr basic_string_view(const basic_string_view &rhs) noexcept = default;
  constexpr basic_string_view &operator=(const basic_string_view &) noexcept =
      default;

  template <typename Allocator>
  constexpr basic_string_view(
      const std::basic_string<value_type, traits_type, Allocator> &str) noexcept
      : ptr_{str.data()}, length_{str.size()} {}

  STDX_NONNULL constexpr basic_string_view(const_pointer data)
      : ptr_{data}, length_{impl::char_traits_length(data)} {}

  STDX_NONNULL constexpr basic_string_view(const_pointer data, size_type len)
      : ptr_{data}, length_{len} {}

  // [string.view.iterators]
  constexpr const_iterator begin() const noexcept { return ptr_; }
  constexpr const_iterator end() const noexcept { return ptr_ + length_; }
  constexpr const_iterator cbegin() const noexcept { return begin(); }
  constexpr const_iterator cend() const noexcept { return end(); }

  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(begin());
  }

  // [string.view.capacity]
  constexpr size_type size() const noexcept { return length_; }
  constexpr size_type length() const noexcept { return length_; }
  constexpr size_type max_size() const noexcept {
    return (npos - sizeof(size_type)) / sizeof(value_type);
  }
  constexpr bool empty() const noexcept { return length_ == 0; }

  // [string.view.access]
  constexpr const_reference operator[](size_type pos) const noexcept {
    return ptr_[pos];
  }
  constexpr const_reference at(size_type pos) const {
    // we may throw if pos > length
    return ptr_[pos];
  }

  /**
   * first element.
   *
   * - Requires: !empty()
   *
   * @returns ref to first element
   */
  constexpr const_reference front() const noexcept { return ptr_[0]; }

  /**
   * last element.
   *
   * - Requires: !empty()
   *
   * @returns ref to last element
   */
  constexpr const_reference back() const noexcept { return ptr_[length_ - 1]; }

  /**
   * pointer to underlaying data.
   *
   * may not be null-terminated.
   */
  constexpr const_pointer data() const noexcept { return ptr_; }

  // [string.view.modifiers]
  void clear() noexcept { *this = basic_string_view(); }

  // requires: n <= size()
  void remove_prefix(size_type n) { *this = substr(n, npos); }

  // requires: n <= size()
  void remove_suffix(size_type n) { *this = substr(0, size() - n); }

  void swap(basic_string_view &s) noexcept {
    auto tmp = *this;
    *this = s;
    s = tmp;
  }

  // [string.view.ops]
  template <class Allocator>
  explicit operator std::basic_string<charT, traits, Allocator>() const {
    return {begin(), end()};
  }

  /**
   * copy into external buffer.
   *
   * if n > characters available: rest of string
   *
   * @param s   pointer to start of destination
   * @param n   character-length of s
   * @param pos start position
   * @throws std::out_of_range if pos > size()
   */
  size_type copy(charT *s, size_type n, size_type pos = 0) const {
    if (pos > size()) throw std::out_of_range("...");

    size_t rlen = std::min(n, size() - pos);

    std::copy_n(begin() + pos, rlen, s);

    return rlen;
  }

  /**
   * get substring of a string_view.
   *
   * if n > characters available: rest of string
   *
   * @param pos start position
   * @param n   length of substring from start position
   * @throws std::out_of_range if pos > size()
   */
  constexpr basic_string_view substr(size_type pos = 0,
                                     size_type n = npos) const {
    if (pos > size()) throw std::out_of_range("...");

    size_t rlen = std::min(n, size() - pos);

    return {data() + pos, rlen};
  }

  constexpr int compare(basic_string_view s) const noexcept {
    size_t rlen = std::min(size(), s.size());

    if (rlen > 0) {
      int res =
          impl::char_traits_compare<charT, traits_type>(data(), s.data(), rlen);
      if (res != 0) return res;
    }

    return size() - s.size();
  }

  constexpr int compare(size_t pos1, size_type n1, basic_string_view s) const {
    return substr(pos1, n1).compare(s);
  }

  constexpr int compare(size_t pos1, size_type n1, basic_string_view s,
                        size_type pos2, size_type n2) const {
    return substr(pos1, n1).compare(s.substr(pos2, n2));
  }

  constexpr int compare(const value_type *s) const {
    return compare(basic_string_view(s));
  }
  constexpr int compare(size_t pos1, size_type n1, const value_type *s) const {
    return compare(pos1, n1, basic_string_view(s));
  }
  constexpr int compare(size_t pos1, size_type n1, const value_type *s,
                        size_type pos2, size_type n2) const {
    return compare(pos1, n1, basic_string_view(s), pos2, n2);
  }

  // [string.view.find]

  size_type find(basic_string_view str, size_type pos = 0) const noexcept {
    if (empty() || pos > length()) {
      if (empty() && pos == 0 && str.empty()) return 0;

      return npos;
    }

    if (const charT *result = impl::memmatch<charT, traits_type>(
            ptr_ + pos, length_ - pos, str.data(), str.size())) {
      return result - ptr_;
    }

    return npos;
  }

 private:
  const value_type *ptr_;
  size_type length_;
};

// ==
template <class charT, class traits>
constexpr bool operator==(basic_string_view<charT, traits> a,
                          basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) == 0;
}

#if defined(_MSC_VER)
// workaround a limitation in the MSVC name mangling which results in:
//
// definition with same mangled name
// '??$?8_WU?$char_traits@_W@std@@@stdx@@YA_NV?$basic_string_view@_WU?$char_traits@_W@std@@@0@0@Z'
// as another definition
//
// msvc CRT marks the operators with: TRANSITION, VSO#409326
#define MSVC_ORDER(x) , int = x
#else
#define MSVC_ORDER(x) /*, int = x */
#endif

template <class charT, class traits MSVC_ORDER(1)>
constexpr bool operator==(
    basic_string_view<charT, traits> a,
    impl::identity<basic_string_view<charT, traits>> b) noexcept {
  return a.compare(b) == 0;
}

template <class charT, class traits MSVC_ORDER(2)>
constexpr bool operator==(impl::identity<basic_string_view<charT, traits>> a,
                          basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) == 0;
}

// !=
template <class charT, class traits>
constexpr bool operator!=(basic_string_view<charT, traits> a,
                          basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) != 0;
}

template <class charT, class traits MSVC_ORDER(1)>
constexpr bool operator!=(
    basic_string_view<charT, traits> a,
    impl::identity<basic_string_view<charT, traits>> b) noexcept {
  return a.compare(b) != 0;
}

template <class charT, class traits MSVC_ORDER(2)>
constexpr bool operator!=(impl::identity<basic_string_view<charT, traits>> a,
                          basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) != 0;
}

// >
template <class charT, class traits>
constexpr bool operator>(basic_string_view<charT, traits> a,
                         basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) > 0;
}

template <class charT, class traits MSVC_ORDER(1)>
constexpr bool operator>(
    basic_string_view<charT, traits> a,
    impl::identity<basic_string_view<charT, traits>> b) noexcept {
  return a.compare(b) > 0;
}

template <class charT, class traits MSVC_ORDER(2)>
constexpr bool operator>(impl::identity<basic_string_view<charT, traits>> a,
                         basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) > 0;
}

// <
template <class charT, class traits>
constexpr bool operator<(basic_string_view<charT, traits> a,
                         basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) < 0;
}

template <class charT, class traits MSVC_ORDER(1)>
constexpr bool operator<(
    basic_string_view<charT, traits> a,
    impl::identity<basic_string_view<charT, traits>> b) noexcept {
  return a.compare(b) < 0;
}

template <class charT, class traits MSVC_ORDER(2)>
constexpr bool operator<(impl::identity<basic_string_view<charT, traits>> a,
                         basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) < 0;
}

// <=
template <class charT, class traits>
constexpr bool operator<=(basic_string_view<charT, traits> a,
                          basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) <= 0;
}

template <class charT, class traits MSVC_ORDER(1)>
constexpr bool operator<=(
    basic_string_view<charT, traits> a,
    impl::identity<basic_string_view<charT, traits>> b) noexcept {
  return a.compare(b) <= 0;
}

template <class charT, class traits MSVC_ORDER(2)>
constexpr bool operator<=(impl::identity<basic_string_view<charT, traits>> a,
                          basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) <= 0;
}

// =>
template <class charT, class traits>
constexpr bool operator>=(basic_string_view<charT, traits> a,
                          basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) >= 0;
}

template <class charT, class traits MSVC_ORDER(1)>
constexpr bool operator>=(
    basic_string_view<charT, traits> a,
    impl::identity<basic_string_view<charT, traits>> b) noexcept {
  return a.compare(b) >= 0;
}

template <class charT, class traits MSVC_ORDER(2)>
constexpr bool operator>=(impl::identity<basic_string_view<charT, traits>> a,
                          basic_string_view<charT, traits> b) noexcept {
  return a.compare(b) >= 0;
}

#undef MSVC_ORDER

template <class charT, class traits = std::char_traits<charT>,
          class Allocator = std::allocator<charT>>
std::basic_string<charT, traits, Allocator> to_string(
    basic_string_view<charT, traits> str, const Allocator &a = Allocator()) {
  return {str.begin(), str.end(), a};
}

template <class charT, class traits>
std::basic_ostream<charT, traits> &operator<<(
    std::basic_ostream<charT, traits> &o, basic_string_view<charT, traits> sv) {
  typename std::basic_ostream<charT, traits>::sentry sentry(o);

  if (sentry) {
    o.write(sv.data(), sv.size());
    o.width(0);
  }

  return o;
}

inline namespace literals {
inline namespace string_view_literals {
inline constexpr basic_string_view<char> operator""_sv(const char *str,
                                                       size_t len) noexcept {
  return basic_string_view<char>{str, len};
}
inline constexpr basic_string_view<wchar_t> operator""_sv(const wchar_t *str,
                                                          size_t len) noexcept {
  return basic_string_view<wchar_t>{str, len};
}
inline constexpr basic_string_view<char16_t> operator""_sv(
    const char16_t *str, size_t len) noexcept {
  return basic_string_view<char16_t>{str, len};
}
inline constexpr basic_string_view<char32_t> operator""_sv(
    const char32_t *str, size_t len) noexcept {
  return basic_string_view<char32_t>{str, len};
}
}  // namespace string_view_literals
}  // namespace literals

using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;
using u16string_view = basic_string_view<char16_t>;
using u32string_view = basic_string_view<char32_t>;

}  // namespace stdx

namespace std {
// [string.view.hash]
template <>
struct hash<stdx::string_view> {
  size_t operator()(stdx::string_view s) const noexcept {
    return std::hash<std::string>()(std::string{s.data(), s.size()});
  }
};
template <>
struct hash<stdx::wstring_view> {
  size_t operator()(stdx::wstring_view s) const noexcept {
    return std::hash<std::wstring>()(std::wstring{s.data(), s.size()});
  }
};

template <>
struct hash<stdx::u16string_view> {
  size_t operator()(stdx::u16string_view s) const noexcept {
    return std::hash<std::u16string>()(std::u16string{s.data(), s.size()});
  }
};
template <>
struct hash<stdx::u32string_view> {
  size_t operator()(stdx::u32string_view s) const noexcept {
    return std::hash<std::u32string>()(std::u32string{s.data(), s.size()});
  }
};
}  // namespace std

#endif
