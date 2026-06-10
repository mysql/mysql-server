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

#ifndef ROUTER_SRC_HARNESS_INCLUDE_SECURE_STRING_H_
#define ROUTER_SRC_HARNESS_INCLUDE_SECURE_STRING_H_

#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "harness_export.h"  // NOLINT(build/include_subdir)

#ifndef __sun
#include "secure_allocator.h"  // NOLINT(build/include_subdir)
#endif

namespace mysql_harness {
namespace detail {

template <typename value_type, typename traits_type, typename allocator_type>
concept SameValueType =
    requires {
      std::is_same_v<value_type, typename traits_type::char_type>;
      std::is_same_v<value_type, typename allocator_type::value_type>;
    };

void HARNESS_EXPORT secure_wipe(void *ptr, std::size_t count);

/**
 * Null-terminated string which is securely wiped on destruction.
 */
template <typename CharT, typename Traits = std::char_traits<CharT>,
          typename Allocator = std::allocator<CharT>>
  requires SameValueType<CharT, Traits, Allocator>
class SecureString final {
 private:
  using AllocatorTraits = std::allocator_traits<Allocator>;

 public:
  using traits_type = Traits;
  using value_type = Traits::char_type;
  using allocator_type = Allocator;

  using size_type = AllocatorTraits::size_type;
  using difference_type = AllocatorTraits::difference_type;

  using reference = value_type &;
  using const_reference = const value_type &;

  using pointer = AllocatorTraits::pointer;
  using const_pointer = AllocatorTraits::const_pointer;

  /**
   * Default constructor.
   */
  SecureString() noexcept : data_() { set_empty(); }

  /**
   * Constructor with an allocator.
   */
  explicit SecureString(const allocator_type &alloc) noexcept : data_(alloc) {
    set_empty();
  }

  /**
   * Copies the provided string, wiping the memory pointed by `ptr`.
   *
   * @param ptr Pointer to an array of characters.
   * @param length Number of characters in the array.
   * @param alloc Allocator to use.
   */
  SecureString(pointer ptr, size_type length,
               const allocator_type &alloc = allocator_type())
      : data_(alloc) {
    if (length) [[likely]] {
      allocate(length);

      assign(ptr, length);
      traits_type::assign(data()[length], kNull);

      wipe(ptr, length);
    } else {
      set_empty();
    }
  }

  /**
   * Copies the provided string, wiping the memory between `first` and `last`.
   *
   * @param first Beginning of a range.
   * @param last End of a range.
   * @param alloc Allocator to use.
   */
  template <typename InputIt>
  SecureString(InputIt first, InputIt last,
               const allocator_type &alloc = allocator_type())
      : data_(alloc) {
    if (const auto length = std::distance(first, last)) [[likely]] {
      allocate(length);

      const auto ptr = reinterpret_cast<pointer>(&*first);

      for (auto p = data(); first != last; ++first, ++p) {
        traits_type::assign(*p, *first);
      }

      traits_type::assign(data()[length], kNull);

      wipe(ptr, length);
    } else {
      set_empty();
    }
  }

  /**
   * Copies the provided string, wiping its memory.
   *
   * @param s String to be copied.
   * @param alloc Allocator to use.
   */
  template <class StringTraits = std::char_traits<value_type>,
            class StringAllocator = std::allocator<value_type>>
    requires SameValueType<value_type, StringTraits, StringAllocator>
  SecureString(std::basic_string<value_type, StringTraits, StringAllocator> &&s,
               const allocator_type &alloc = allocator_type())
      : data_(alloc) {
    if (!s.empty()) [[likely]] {
      allocate(s.length());

      assign(s.data(), s.length() + 1);

      wipe(s.data(), s.length());
      s.clear();
    } else {
      set_empty();
    }
  }

  /**
   * Copy constructor.
   */
  SecureString(const SecureString &ss)
      : data_(AllocatorTraits::select_on_container_copy_construction(
            ss.allocator())) {
    if (!ss.empty()) [[likely]] {
      allocate(ss.length());

      assign(ss.data(), ss.length() + 1);
    } else {
      set_empty();
    }
  }

  /**
   * Move constructor.
   */
  SecureString(SecureString &&ss) noexcept : data_(std::move(ss.allocator())) {
    if (!ss.empty()) [[likely]] {
      set_data(ss.data());
      set_length(ss.length());

      ss.set_empty();
    } else {
      set_empty();
    }
  }

  /**
   * Copies the provided string, wiping its memory.
   *
   * @param s String to be copied.
   */
  template <class StringTraits = std::char_traits<value_type>,
            class StringAllocator = std::allocator<value_type>>
    requires SameValueType<value_type, StringTraits, StringAllocator>
  SecureString &operator=(
      std::basic_string<value_type, StringTraits, StringAllocator> &&s) {
    SecureString ss{std::move(s)};
    swap(ss);
    return *this;
  }

  /**
   * Copy assignment operator.
   */
  SecureString &operator=(const SecureString &ss) {
    if (this == std::addressof(ss)) [[unlikely]] {
      return *this;
    }

    // need to release the memory before replacing the allocator
    clear();

    if constexpr (propagate_on_copy()) {
      allocator() = ss.allocator();
    }

    if (!ss.empty()) [[likely]] {
      allocate(ss.length());

      assign(ss.data(), ss.length() + 1);
    }

    return *this;
  }

  /**
   * Move assignment operator.
   */
  SecureString &operator=(SecureString &&ss) noexcept {
    if (this == std::addressof(ss)) [[unlikely]] {
      return *this;
    }

    // need to release the memory before replacing the allocator
    clear();

    if constexpr (propagate_on_move()) {
      allocator() = std::move(ss.allocator());
    }

    if (!ss.empty()) [[likely]] {
      set_data(ss.data());
      set_length(ss.length());

      ss.set_empty();
    }

    return *this;
  }

  /**
   * Releases the string, securely wiping the data.
   */
  ~SecureString() { clear(); }

  /**
   * Comparison operators.
   */
  friend inline bool operator==(const SecureString &l,
                                const SecureString &r) noexcept {
    return l.length() == r.length() &&
           !traits_type::compare(l.data(), r.data(), l.length());
  }

  friend inline bool operator!=(const SecureString &l,
                                const SecureString &r) noexcept {
    return !(l == r);
  }

  /**
   * Provides pointer to the stored string.
   */
  [[nodiscard]] inline const_pointer c_str() const noexcept { return data(); }

  /**
   * Provides length to the stored string.
   */
  [[nodiscard]] inline size_type length() const noexcept { return length_; }
  [[nodiscard]] inline size_type size() const noexcept { return length_; }

  /**
   * Checks if the stored string is empty.
   */
  [[nodiscard]] inline bool empty() const noexcept { return 0 == length(); }

  /**
   * Swaps the contents of this string with the provided one.
   *
   * @param ss String to swap the contents with.
   */
  void swap(SecureString &ss) noexcept {
    if (this == std::addressof(ss)) [[unlikely]] {
      return;
    }

    if constexpr (propagate_on_swap()) {
      using std::swap;
      swap(allocator(), ss.allocator());
    }

    {
      auto ptr = data();
      set_data(ss.data());
      ss.set_data(ptr);
    }

    {
      auto l = length();
      set_length(ss.length());
      ss.set_length(l);
    }
  }

  /**
   * Clears the string, securely wiping the data.
   */
  void clear() noexcept {
    if (empty()) [[unlikely]] {
      return;
    }

    wipe(data(), length());

    AllocatorTraits::deallocate(allocator(), data(), length() + 1);

    set_empty();
  }

 private:
  // empty-base optimization: http://www.cantrip.org/emptyopt.html
  struct Alloc : allocator_type {
    explicit constexpr Alloc(const allocator_type &a) : allocator_type(a) {}

    explicit constexpr Alloc(allocator_type &&a = allocator_type())
        : allocator_type(std::move(a)) {}

    pointer ptr_ = nullptr;
  };

  static constexpr inline bool propagate_on_copy() noexcept {
    return AllocatorTraits::propagate_on_container_copy_assignment::value;
  }

  static constexpr inline bool propagate_on_move() noexcept {
    return AllocatorTraits::propagate_on_container_move_assignment::value;
  }

  static constexpr inline bool propagate_on_swap() noexcept {
    return AllocatorTraits::propagate_on_container_swap::value;
  }

  static inline void wipe(pointer ptr, size_type length) noexcept {
    secure_wipe(ptr, sizeof(value_type) * length);
  }

  inline const allocator_type &allocator() const noexcept { return data_; }
  inline allocator_type &allocator() noexcept { return data_; }

  inline const_pointer data() const noexcept { return data_.ptr_; }
  inline pointer data() noexcept { return data_.ptr_; }

  inline void set_data(pointer ptr) noexcept { data_.ptr_ = ptr; }

  inline void set_length(size_type length) noexcept { length_ = length; }

  void allocate(size_type length) {
    set_data(AllocatorTraits::allocate(allocator(), length + 1));
    set_length(length);
  }

  void assign(const_pointer ptr, size_type length) {
    if (1 == length) [[unlikely]] {
      traits_type::assign(*data(), *ptr);
    } else {
      traits_type::copy(data(), ptr, length);
    }
  }

  void set_empty() noexcept {
    set_data(const_cast<pointer>(&kNull));
    set_length(0);
  }

  static constexpr value_type kNull{};

  Alloc data_;
  size_type length_;
};

}  // namespace detail

#ifdef __sun
// mlock() is a privileged call on Solaris, don't use the SecureAllocator there
using SecureString = detail::SecureString<char>;
#else   // !__sun
using SecureString =
    detail::SecureString<char, std::char_traits<char>, SecureAllocator<char>>;
#endif  // !__sun

}  // namespace mysql_harness

#endif  // ROUTER_SRC_HARNESS_INCLUDE_SECURE_STRING_H_
