/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_STDX_SPAN_INCLUDED
#define MYSQL_HARNESS_STDX_SPAN_INCLUDED

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <type_traits>

#include "mysql/harness/stdx/type_traits.h"

namespace stdx {

// C++20's std::span.
//
// - uses slightly different constructor for std::vector<> as there are no
//   <ranges> in C++17.

inline constexpr std::size_t dynamic_extent =
    std::numeric_limits<std::size_t>::max();

template <class T, std::size_t Extent = dynamic_extent>
class span;

namespace detail {
template <class C, class = void>
struct has_data : std::false_type {};

template <class C>
struct has_data<C, std::void_t<decltype(std::data(std::declval<C>()))>>
    : std::true_type {};

template <class C, class = void>
struct has_size : std::false_type {};

template <class C>
struct has_size<C, std::void_t<decltype(std::size(std::declval<C>()))>>
    : std::true_type {};

// is-std-array

template <class T, class = void>
struct is_std_array_impl : std::false_type {};

template <class T, size_t Extend>
struct is_std_array_impl<std::array<T, Extend>> : std::true_type {};

template <class T>
struct is_std_array : is_std_array_impl<stdx::remove_cvref_t<T>> {};

// is-span

template <class T, class = void>
struct is_span_impl : std::false_type {};

template <class T, std::size_t Extend>
struct is_span_impl<span<T, Extend>> : std::true_type {};

template <class T>
struct is_span : is_span_impl<stdx::remove_cvref_t<T>> {};

// minimalistic "concept std::ranged::contiguous_range"
template <class C>
struct is_contiguous_range : std::bool_constant<has_data<C>::value> {};

// minimalistic "concept std::ranged::sized_range"
template <class C>
struct is_sized_range : std::bool_constant<has_size<C>::value> {};

template <class C, class E, class = void>
struct is_compatible_element : std::false_type {};

template <class C, class E>
struct is_compatible_element<
    C, E, std::void_t<decltype(std::data(std::declval<C>()))>>
    : std::is_convertible<
          std::remove_pointer_t<decltype(std::data(std::declval<C &>()))> (*)[],
          E (*)[]> {};

/*
 * - R satisfies contiguous_range or sized_range
 * - (either R satisfies borrowed_range or is_const_v<element_type> is true)
 * - std::remove_cvref_t<R> is not a specialization of std::span
 * - std::remove_cvref_t<R> is not a specialization of std::array
 * - std::is_array_v<std::remove_cvref_t<R>> is false, and
 * - the conversion from range_ref_t<R> to element_type is at most a
 *   qualification conversion.
 */
template <class R, class E>
struct is_compatible_range
    : std::bool_constant<is_contiguous_range<R>::value &&              //
                         is_sized_range<R>::value &&                   //
                         !is_span<R>::value &&                         //
                         !is_std_array<R>::value &&                    //
                         !std::is_array_v<stdx::remove_cvref_t<R>> &&  //
                         is_compatible_element<R, E>::value> {};

template <class From, class To>
struct is_array_convertible : std::is_convertible<From (*)[], To (*)[]> {};

// iterator over a continous memory range.
//
// random-access.
template <class T>
class iterator {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using reference = typename T::reference;
  using pointer = typename T::pointer;
  using value_type = typename T::value_type;
  using difference_type = typename T::difference_type;

  constexpr iterator(pointer data) : data_(data) {}

  // +=
  constexpr iterator &operator+=(difference_type n) {
    data_ += n;

    return *this;
  }

  // binary plus.
  constexpr iterator operator+(difference_type n) {
    auto tmp = *this;

    tmp.data_ += n;

    return tmp;
  }

  // -=
  constexpr iterator &operator-=(difference_type n) {
    data_ -= n;

    return *this;
  }

  // binary minus.
  constexpr iterator operator-(difference_type n) {
    auto tmp = *this;

    tmp.data_ -= n;

    return tmp;
  }

  // pre-decrement
  constexpr iterator &operator++() {
    ++data_;

    return *this;
  }

  // pre-decrement
  constexpr iterator &operator--() {
    --data_;

    return *this;
  }

  // post-decrement
  constexpr iterator operator--(int) {
    auto tmp = *this;

    --tmp.data_;

    return *this;
  }

  // a - b
  constexpr difference_type operator-(const iterator &other) {
    return data_ - other.data_;
  }

  // *it;
  constexpr reference operator*() { return *data_; }

  // it[n]
  constexpr reference operator[](difference_type n) { return *(data_ + n); }

  // a == b
  constexpr bool operator==(const iterator &other) const {
    return data_ == other.data_;
  }

  // a != b
  constexpr bool operator!=(const iterator &other) const {
    return !(*this == other);
  }

  // a < b
  constexpr bool operator<(const iterator &other) const {
    return data_ < other.data_;
  }

  // a > b
  constexpr bool operator>(const iterator &other) const {
    return (other < *this);
  }

  // a >= b
  constexpr bool operator>=(const iterator &other) const {
    return !(*this < other);
  }

  // a <= b
  constexpr bool operator<=(const iterator &other) const {
    return !(*other < *this);
  }

 private:
  pointer data_;
};

template <class T, size_t Extent>
struct span_size {
  static constexpr size_t size = sizeof(T) * Extent;
};

template <class T>
struct span_size<T, dynamic_extent> {
  static constexpr size_t size = dynamic_extent;
};

template <size_t Extent, size_t Offset, size_t Count>
struct extent_size {
  static constexpr size_t size = Count;
};

template <size_t Extent, size_t Offset>
struct extent_size<Extent, Offset, dynamic_extent> {
  static constexpr size_t size =
      Extent == dynamic_extent ? dynamic_extent : Extent - Offset;
};

}  // namespace detail

template <class T, std::size_t Extent>
class span {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;

  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  using pointer = T *;
  using const_pointer = const T *;

  using reference = T &;
  using const_reference = const T &;

  using iterator = detail::iterator<span>;
  using reverse_iterator = std::reverse_iterator<iterator>;

  static constexpr const size_t extent = Extent;

  /**
   * default construct.
   *
   * only enabled if Extent is 0 or dynamic.
   */
  template <bool B = (Extent == 0) || (Extent == dynamic_extent),
            typename std::enable_if<B, int>::type = 0>
  constexpr span() noexcept : span(nullptr, 0) {}

  /**
   * construct a span from pointer and size
   */
  constexpr span(pointer ptr, size_type count) : data_(ptr), size_(count) {}

  /**
   * construct a span from a continous range like a vector.
   */
  template <
      class Range,
      std::enable_if_t<detail::is_compatible_range<Range, element_type>::value,
                       int> = 0>
  constexpr span(Range &cont) noexcept
      : data_(std::data(cont)), size_(std::size(cont)) {}

  /**
   * construct a span from a const continous range like a vector.
   */
  template <class Range, std::enable_if_t<std::is_const_v<element_type> &&
                                              detail::is_compatible_range<
                                                  Range, element_type>::value,
                                          int> = 0>
  constexpr span(const Range &range) noexcept
      : data_(std::data(range)), size_(std::size(range)) {}

  /**
   * construct a span from C-array.
   */
  template <std::size_t N,
            std::enable_if_t<extent == dynamic_extent || extent == N, int> = 0>
  constexpr span(stdx::type_identity_t<element_type> (&arr)[N]) noexcept
      : data_(std::data(arr)), size_(N) {}

  /**
   * construct a span from a std::array.
   */
  template <class U, std::size_t N,
            std::enable_if_t<(Extent == dynamic_extent || Extent == N) &&
                                 detail::is_array_convertible<U, T>::value,
                             int> = 0>
  constexpr span(std::array<U, N> &arr) noexcept
      : data_(std::data(arr)), size_(std::size(arr)) {}

  template <class U, std::size_t N,
            std::enable_if_t<(Extent == dynamic_extent || Extent == N) &&
                                 detail::is_array_convertible<U, T>::value,
                             int> = 0>
  constexpr span(const std::array<U, N> &arr) noexcept
      : data_(std::data(arr)), size_(std::size(arr)) {}

  // copy constructor
  constexpr span(const span &other) noexcept = default;
  constexpr span &operator=(const span &other) noexcept = default;

  ~span() noexcept = default;

  // span.sub

  /**
   * create a span of the first Count elements.
   *
   * The behaviour is undefined if Count > size()
   */
  template <std::size_t Count>
  constexpr span<element_type, Count> first() const {
    static_assert(Count <= Extent);

    assert(Count <= size());

    return {data(), Count};
  }

  /**
   * create a span of the first Count elements.
   *
   * The behaviour is undefined if count > size().
   */
  constexpr span<element_type, dynamic_extent> first(size_type count) const {
    assert(count <= size());

    return {data(), count};
  }

  /**
   * create a span of the last Count elements.
   *
   * The behaviour is undefined if Count > size().
   */
  template <std::size_t Count>
  constexpr span<element_type, Count> last() const {
    if constexpr (Extent != dynamic_extent) {
      static_assert(Count <= Extent);
    }

    assert(Count <= size());

    return {data() + (size() - Count), Count};
  }

  /**
   * create a span of the last Count elements.
   *
   * The behaviour is undefined if count > size().
   */
  constexpr span<element_type, dynamic_extent> last(size_type count) const {
    assert(count <= size());

    return {data() + (size() - count), count};
  }

  /**
   * create a span of the Count elements, starting at offset.
   *
   * if 'Count' is 'dynamic_extent', then up to the end of the span.
   *
   * The behaviour is undefined if Offset or Count are out of range.
   */
  template <std::size_t Offset, std::size_t Count = dynamic_extent>
  constexpr span<element_type, detail::extent_size<Extent, Offset, Count>::size>
  subspan() const {
    if constexpr (Extent != dynamic_extent) {
      static_assert(Offset <= Extent);
    }

    assert(Offset <= size());

    if constexpr (Count == dynamic_extent) {
      return {data() + Offset, size() - Offset};
    } else {
      assert(Count <= size() - Offset);

      if constexpr (Extent != dynamic_extent) {
        static_assert(Count <= Extent);
        static_assert(Count <= Extent - Offset);
      }
    }

    return {data() + Offset, Count};
  }

  /**
   * create a span of the Count elements, starting at offset.
   *
   * if 'count' is 'dynamic_extent', then up to the end of the span.
   *
   * The behaviour is undefined if `Count > size()`, and if `Offset > size()`.
   */
  constexpr span<element_type, dynamic_extent> subspan(
      size_type offset, size_type count = dynamic_extent) const {
    assert(offset <= size());

    if (count == dynamic_extent) {
      return {data() + offset, size() - offset};
    }

    assert(count <= size());

    return {data() + offset, count};
  }

  /**
   * returns a ref to the idx-tn element in the sequence.
   *
   * The behaviour is undefined if idx >= size()
   */
  constexpr reference operator[](size_type idx) const {
    assert(idx < size());

    return *(data() + idx);
  }

  /**
   * check if this span is empty.
   *
   * @retval true if span is empty.
   * @retval false if span is not empty.
   */
  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  /**
   * get the pointer the first element.
   */
  constexpr pointer data() const noexcept { return data_; }

  /**
   * get size in elements.
   */
  constexpr size_type size() const noexcept { return size_; }

  /**
   * get size in bytes.
   */
  constexpr size_type size_bytes() const noexcept {
    return size_ * sizeof(element_type);
  }

  /**
   * return a reference to the first element.
   *
   * The behaviour is undefined if the span is empty.
   */
  constexpr reference front() const {
    assert(!empty());

    return *begin();
  }

  /**
   * return a reference to the last element.
   *
   * The behaviour is undefined if the span is empty.
   */
  constexpr reference back() const {
    assert(!empty());

    return *(end() - 1);
  }

  /**
   * iterator to the first element.
   */
  constexpr iterator begin() const noexcept { return data_; }

  /**
   * iterator past the last element.
   */
  constexpr iterator end() const noexcept { return data_ + size_; }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(this->end());
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(this->begin());
  }

 private:
  pointer data_;
  size_type size_;
};

// deduction guides

template <class Container>
span(Container &) -> span<typename Container::value_type>;

template <class Container>
span(Container const &) -> span<const typename Container::value_type>;

// as_bytes

/**
 * get a view to underlying bytes of a span 'spn'.
 */
template <class T, std::size_t N>
span<const std::byte, detail::span_size<T, N>::size> as_bytes(
    span<T, N> spn) noexcept {
  return {reinterpret_cast<const std::byte *>(spn.data()), spn.size_bytes()};
}

/**
 * get a writable view to underlying bytes of a span 'spn'.
 */
template <class T, std::size_t N,
          std::enable_if_t<!std::is_const_v<T>, int> = 0>
span<std::byte, detail::span_size<T, N>::size> as_writable_bytes(
    span<T, N> spn) noexcept {
  return {reinterpret_cast<std::byte *>(spn.data()), spn.size_bytes()};
}

}  // namespace stdx

#endif
