/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.
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

#ifndef NDB_UTIL_SPAN_H
#define NDB_UTIL_SPAN_H

#include <array>
#include <cstddef>
#include <exception>
#include <limits>

/*
 * ndb::span - a view into an existing buffer
 *
 * ndb::span is a partial implementation of C++20 std::span.
 * If further extended, keep it compatible with std::span.
 *
 * A typical use case is a function that takes a buffer that can not change
 * size.
 *
 * Examples,
 *
 * The function declared:
 *
 * template <std::size_t Extent> int f(ndb::span<char,Extent>);
 *
 * Can be called:
 *
 * char buf[100];
 * ndb::span sp(buf);
 * f(sp);
 *
 * std::array<char, 20> arr;
 * f(ndb::span(arr));
 *
 * char* p = some_buffer;
 * size_t len = 8;
 * f({p, len}); // Note extra braces {} around buffer argument.
 *
 * char* begin = &buf[0];
 * char* end = &buf[100];
 * f({begin, end}); // Note extra braces {} around buffer argument.
 *
 * In the first two cases the size of buffer is in type and only a pointer is
 * part of span object.
 *
 * In the other two cases the span object also contains a extent member.
 *
 * Other uses:
 *
 * ndb::span vec(buf);
 * memset(vec.data(), 0, vec.size());
 * for(auto e : vec) assert(e == 0);
 * for(int i = 0; i < vec.size(); i++) vec[i] = i;
 */

namespace ndb {
static inline constexpr std::size_t dynamic_extent =
    std::numeric_limits<std::size_t>::max();

namespace detail {
template <class T>
class span_base;

template <std::size_t Extent>
class span_extent;
}  // namespace detail

template <class T, std::size_t Extent = ndb::dynamic_extent>
class span : private detail::span_base<T>, detail::span_extent<Extent> {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using iterator = T *;
  using pointer = T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = std::size_t;

  static constexpr size_type extent = Extent;

  constexpr span() noexcept : detail::span_base<T>(nullptr) {}
  constexpr span(const span &other) noexcept = default;
  /*
   * To avoid warning: size ... of array exceeds maximum object size ... when
   * Extent is dynamic_extent use zero sized array argument.
   */
  constexpr span(
      element_type (&arr)[Extent != dynamic_extent ? Extent : 0]) noexcept
      : detail::span_base<T>(arr) {
    static_assert(Extent != dynamic_extent);
  }
  template <class U>
  constexpr span(std::array<U, Extent> &arr) noexcept
      : detail::span_base<T>(arr.data()) {}
  template <class U>
  constexpr span(const std::array<U, Extent> &arr) noexcept
      : detail::span_base<T>(arr.data()) {}
  template <class U>
  constexpr span(const ndb::span<U, Extent> &source) noexcept
      : detail::span_base<T>(source.data()) {}
  template <class U>
  constexpr span(U &source) noexcept
      : detail::span_base<typename U::value_type>(source.data()),
        detail::span_extent<Extent>(source.size()) {}
  constexpr span(pointer base, size_type len) noexcept
      : detail::span_base<T>(base), detail::span_extent<Extent>(len) {}
  constexpr span(pointer base, pointer end) noexcept
      : detail::span_base<T>(base), detail::span_extent<Extent>(end - base) {}

  constexpr reference operator[](size_type pos) {
    return this->get_base()[pos];
  }
  constexpr const_reference operator[](size_type pos) const {
    return this->get_base()[pos];
  }

  constexpr iterator begin() const noexcept { return this->get_base(); }
  constexpr iterator end() const noexcept {
    return this->get_base() + this->get_extent();
  }
  constexpr iterator rbegin() const noexcept {
    return this->get_base() + this->get_extent() - 1;
  }
  constexpr iterator rend() const noexcept { return this->get_base() - 1; }
  [[nodiscard]] constexpr bool empty() const noexcept {
    return (this->get_extent() == 0);
  }
  constexpr pointer data() const noexcept { return this->get_base(); }
  constexpr size_type size() const noexcept { return this->get_extent(); }
};

// deduction guides

template <class T, std::size_t N>
span(T (&)[N]) -> span<T, N>;

template <class T, std::size_t N>
span(std::array<T, N> &) -> span<T, N>;

template <class T, std::size_t N>
span(const std::array<T, N> &) -> span<const T, N>;

template <class T, class EndOrSize>
span(T *, EndOrSize) -> span<T>;

template <class Container>
span(Container &) -> span<typename Container::value_type>;

// implementation details

namespace detail {
template <class T>
class span_base {
 public:
  constexpr span_base(T *p) noexcept : m_base(p) {}
  constexpr T *get_base() const noexcept { return m_base; }

 private:
  T *m_base;
};

template <std::size_t Extent>
class span_extent {
 public:
  constexpr span_extent() noexcept { static_assert(Extent != dynamic_extent); }
  constexpr span_extent(std::size_t e) noexcept {
    if (e != Extent) std::terminate();
  }
  constexpr std::size_t get_extent() const noexcept { return Extent; }
};

template <>
class span_extent<dynamic_extent> {
  const std::size_t m_extent;

 public:
  constexpr span_extent(std::size_t e) noexcept : m_extent(e) {}
  constexpr std::size_t get_extent() const noexcept { return m_extent; }
};
}  // namespace detail

}  // namespace ndb

#endif
