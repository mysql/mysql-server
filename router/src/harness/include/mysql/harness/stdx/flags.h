/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_STDX_FLAGS_INCLUDED
#define MYSQL_HARNESS_STDX_FLAGS_INCLUDED

#include <type_traits>

#include "mysql/harness/stdx/bit.h"          // popcount
#include "mysql/harness/stdx/type_traits.h"  // is_scoped_enum_v

namespace stdx {

// tags a class as flags-class
template <class E, class Enabler = void>
struct is_flags : public std::false_type {};

template <class E>
inline constexpr bool is_flags_v = is_flags<E>::value;

/**
 * a type-safe flags type.
 *
 * # abstract
 *
 * Using flags in the in C++ isn't very ergonimic:
 *
 * 1. using plain C-enum's isn't typesafe
 * 2. using std::bitset requires bit-positions to set/reset/test them
 * 3. scoped-enums (enum class) doesn't have operaters
 *
 * For ease of use it should be possible to have a flags-type which
 * allows the type-safe operations:
 *
 * - flags = flags & flag
 * - flags &= flag
 * - flags = flags | flag
 * - flags |= flag
 * - flags = flags ^ flag
 * - flags ^= flag
 * - flags = ~flag
 *
 * - flag | flag -> flags
 * - flag & flag -> flags
 * - flag ^ flag -> flags
 *
 * # example
 *
 * @code{.cpp}
 * // underlying scoped-enum
 * enum class somebits {
 *   bit0 = 1 << 0,
 *   bit1 = 1 << 1,
 * };
 *
 * // activate stdx::flags support for the scoped-enum
 * namespace stdx {
 * template<>
 * struct is_flags<somebits> : std::true_type {}
 * }
 *
 * { // default-construct + assignment
 *   stdx::flags<somebits> someflags;
 *
 *   someflags = somebits::bit0 | somebits::bit1;
 *
 *   // get the underlying bitvalue.
 *   std::cerr << someflags.underlying_value() << "\n";
 * }
 *
 * { // direct initialization from enum
 *   stdx::flags<somebits> someflags{somebits::bit0 | somebits::bit1};
 * }
 *
 * { // testing if bit set
 *   stdx::flags<somebits> someflags{somebits::bit0 | somebits::bit1};
 *
 *   if (someflags & somebits::bit1) {} // true
 * }
 *
 * { // setting raw value
 *   stdx::flags<somebits> someflags;
 *   someflags.underlying_value(3);
 *
 *   if (someflags & somebits::bit0) {} // true
 *   if (someflags & somebits::bit1) {} // true
 * }
 * @endcode
 *
 * @tparam E a scoped enum that is tagged with stdx::is_flags.
 */
template <class E>
class flags {
 public:
  static_assert(stdx::is_scoped_enum_v<E>,
                "flags<E>, E must be a scoped enum type");
  static_assert(is_flags_v<E>,
                "flags<E>, E must be declared as flags-type via:"
                "namespace stdx { template<> struct is_flags<...> : "
                "std::true_type {};} ");

  //!< type of the wrapped enum.
  using enum_type = std::decay_t<E>;
  //!< underlying integer type of the enum_type.
  using underlying_type = std::underlying_type_t<enum_type>;
  //!< implementation type of the underlying type.
  using impl_type = std::make_unsigned_t<underlying_type>;
  using size_type = std::size_t;

  /**
   * default constructor.
   *
   * initializes underlying_value to 0 (no-bits-set).
   */
  constexpr flags() = default;

  constexpr flags(const flags &other) = default;  //!< copy constructor.
  constexpr flags(flags &&other) = default;       //!< move constructor
  constexpr flags &operator=(const flags &other) =
      default;                                          //!< copy assignment
  constexpr flags &operator=(flags &&other) = default;  //!< move assignment

  /**
   * converting constructor from enum_type.
   */
  constexpr flags(enum_type v) : v_{static_cast<impl_type>(v)} {}

  /**
   * check if any bit is set.
   *
   * @retval true at least one bit is set.
   * @retval false no bit is set.
   */
  constexpr operator bool() const noexcept { return v_ != 0; }

  /**
   * check if no bit is set.
   *
   * @retval true no bit is set.
   * @retval false at least one bit is set.
   */
  constexpr bool operator!() const noexcept { return !v_; }

  /**
   * negation of all bits.
   *
   * ~flags -> flags.
   */
  constexpr flags operator~() const noexcept {
    return flags{static_cast<impl_type>(~v_)};
  }

  /**
   * bit-or.
   *
   * flags |= flags.
   */
  constexpr flags &operator|=(const flags &other) noexcept {
    v_ |= other.v_;
    return *this;
  }

  /**
   * bit-and.
   *
   * flags &= flags.
   */
  constexpr flags &operator&=(const flags &other) noexcept {
    v_ &= other.v_;
    return *this;
  }

  /**
   * bit-xor.
   *
   * flag ^= flag
   */
  constexpr flags &operator^=(const flags &other) noexcept {
    v_ ^= other.v_;
    return *this;
  }

  /**
   * bit-and.
   *
   * flags & flags -> flags
   */
  friend constexpr flags operator&(flags a, flags b) noexcept {
    return flags{static_cast<impl_type>(a.v_ & b.v_)};
  }

  /**
   * bit-or.
   *
   * flags | flags -> flags
   */
  friend constexpr flags operator|(flags a, flags b) noexcept {
    return flags{static_cast<impl_type>(a.v_ | b.v_)};
  }

  /**
   * bit-xor.
   *
   * flags ^ flags -> flags
   */
  friend constexpr flags operator^(flags a, flags b) noexcept {
    return flags{static_cast<impl_type>(a.v_ ^ b.v_)};
  }

  /**
   * set underlying value.
   *
   * @param v underlying value to set.
   */
  constexpr void underlying_value(underlying_type v) noexcept { v_ = v; }

  /**
   * get underlying value.
   *
   * @return underlying value.
   */
  constexpr underlying_type underlying_value() const noexcept { return v_; }

  /**
   * count bits set to true.
   *
   * @sa size()
   *
   * @return bits set to true.
   */
  [[nodiscard]] constexpr size_type count() const noexcept {
    return stdx::popcount(v_);
  }

  /**
   * returns number of bits the flag-type can hold.
   *
   * @sa count()
   *
   * @returns number of bits the flag-type can hold.
   */
  [[nodiscard]] constexpr size_type size() const noexcept {
    return 8 * sizeof(underlying_type);
  }

  /**
   * reset all flags to 0.
   */
  constexpr void reset() { v_ = 0; }

 private:
  constexpr explicit flags(impl_type v) noexcept : v_{v} {}

  impl_type v_{};
};

}  // namespace stdx

/**
 * bit-or.
 *
 * E | E -> flags<E>;
 *
 * @return a flags<E> of bit-or(e1, e2).
 */
template <class E>
constexpr auto operator|(E e1, E e2) noexcept
    -> std::enable_if_t<stdx::is_flags<E>::value, stdx::flags<E>> {
  return stdx::flags<E>(e1) | e2;
}

/**
 * bit-and.
 *
 * E & E -> flags<E>;
 *
 * @return a flags<E> of bit-and(e1, e2).
 */
template <class E>
constexpr auto operator&(E e1, E e2) noexcept
    -> std::enable_if_t<stdx::is_flags<E>::value, stdx::flags<E>> {
  return stdx::flags<E>(e1) & e2;
}

/**
 * bit-xor.
 *
 * E ^ E -> flags<E>;
 *
 * @return a flags<E> of bit-xor(e1, e2).
 */
template <class E>
constexpr auto operator^(E e1, E e2) noexcept
    -> std::enable_if_t<stdx::is_flags<E>::value, stdx::flags<E>> {
  return stdx::flags<E>(e1) ^ e2;
}

#endif
