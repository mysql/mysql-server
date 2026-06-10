// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_RANGES_BUFFER_INTERFACE_H
#define MYSQL_RANGES_BUFFER_INTERFACE_H

/// @file
/// Experimental API header

#include <cassert>                   // assert
#include <cstddef>                   // ptrdiff_t
#include <cstring>                   // memcmp
#include <string_view>               // string_view
#include "mysql/meta/is_charlike.h"  // Is_charlike
#include "mysql/utils/char_cast.h"   // uchar_cast

/// @addtogroup GroupLibsMysqlRanges
/// @{

namespace mysql::ranges::detail {
/// Top of the hierarchy
class Buffer_base {};
}  // namespace mysql::ranges::detail

namespace mysql::ranges {

// NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint
enum class Equality_algorithm { lexicographic, fast, none };

enum class Enable_hash { no, yes };
// NOLINTEND(performance-enum-size)

/// CRTP base class that provides a rich API for classes that behave like byte
/// buffers. This turns a subclass that implements `size()` and `data()` members
/// into a `std::ranges::range`, defines the same members that
/// `std::view_interface` defines, enables comparisons and hashes, and for any
/// member that uses `char *`, provides alternative members that use `unsigned
/// char *` and `std::byte`.
///
/// The subclass should implement the following member functions:
/// @code
/// size_t size() const;
/// char *data();
/// const char *data() const;
/// @endcode
///
/// This class provides the members:
/// - `string_view`: Return an `std::string_view`.
/// - `[u|b]data`: Return data as `unsigned char *` or `std::byte *`.
/// - `[c][u|b]begin`/`[c][u|b]end`: Return (const) begin/end pointers
///   as `char *`, `unsigned char *`, or `std::byte *`.
/// - `operator[]`: Return the n'th element.
/// - `ssize`: Return the size as a signed integer (`std::ptrdiff_t`).
/// - `empty`: Return `size() == 0`.
/// - `operator bool`: return `size() != 0`.
/// Optionally, the following free functions are provided:
/// - `operator==`, `operator!=`, and `operator<=>`.
/// - `std::hash`.
///
/// The following alternative prototypes are allowed:
/// @code
/// char *data() const;
/// @endcode
/// I.e., the data may be non-const even if the buffer object is const. This is
/// useful when the buffer object does not own the data.
///
/// @tparam Self_tp Subclass.
///
/// @tparam equality_algorithm_tp Determines if and how operators ==, !=, <, >,
/// <=, >=, and <=> are implemented: `lexicographic` compares strings
/// lexicographically, for example, "a" < "aa" < "b"; `fast` compares the length
/// first, and compares lexicographically only when the lengths are equal, for
/// example, "a" < "b" < "aa"; `none` does not implement comparison at all.
/// Default is `lexicographic`.
///
/// @tparam enable_hash_tp If `yes`, enable `std::hash<Self_tp>`. Default is
/// `yes`.
template <class Self_tp,
          Equality_algorithm equality_algorithm_tp =
              Equality_algorithm::lexicographic,
          Enable_hash enable_hash_tp = Enable_hash::yes>
class Buffer_interface : public detail::Buffer_base {
 public:
  static constexpr auto equality_algorithm = equality_algorithm_tp;
  static constexpr bool equality_enabled =
      (equality_algorithm != Equality_algorithm::none);
  static constexpr bool hash_enabled = (enable_hash_tp == Enable_hash::yes);

  // ==== Size members ====

  /// Return true if `size() != 0`.
  [[nodiscard]] explicit operator bool() const { return !self().empty(); }

  /// Return true if `size() == 0`.
  [[nodiscard]] bool operator!() const { return self().empty(); }

  /// Return true if `size() == 0`.
  [[nodiscard]] bool empty() const { return self().size() == 0; }

  /// Return the size as `std::ptrdiff_t`.
  [[nodiscard]] std::ptrdiff_t ssize() const {
    return std::ptrdiff_t(self().size());
  }

  // ==== data members ====

  /// Return the data buffer as `unsigned char *`.
  [[nodiscard]] auto *udata() {
    return mysql::utils::uchar_cast(self().data());
  }

  /// Return the data buffer as `const unsigned char *` or `unsigned char *`,
  /// const-ness inherited from `Self_t::data() const`.
  [[nodiscard]] auto *udata() const {
    return mysql::utils::uchar_cast(self().data());
  }

  /// Return the data buffer as `std::byte *`.
  [[nodiscard]] auto *bdata() { return mysql::utils::byte_cast(self().data()); }

  /// Return the data buffer as `const std::byte *` or `std::byte *`, const-ness
  /// inherited from `Self_t::data() const`.
  [[nodiscard]] auto *bdata() const {
    return mysql::utils::byte_cast(self().data());
  }

  // ==== string_view member ====

  [[nodiscard]] std::string_view string_view() const {
    return {self().data(), self().size()};
  }

  // ==== begin members ====

  /// Return the begin as `char *`.
  [[nodiscard]] auto *begin() { return self().data(); }

  /// Return the begin as `const char *` or `char *`, const-ness inherited from
  /// `Self_t::data() const`.
  [[nodiscard]] auto *begin() const { return self().data(); }

  /// Return the begin as `const char *` or `char *`, const-ness inherited from
  /// `Self_t::data() const`.
  [[nodiscard]] auto *cbegin() const { return self().data(); }

  /// Return the begin as `unsigned char *`.
  [[nodiscard]] auto *ubegin() { return udata(); }

  /// Return the begin as `const unsigned char *` or `unsigned char *`,
  /// const-ness inherited from `Self_t::data() const`.
  [[nodiscard]] auto *ubegin() const { return udata(); }

  /// Return the begin as `const unsigned char *` or `unsigned char *`,
  /// const-ness inherited from `Self_t::data() const`.
  [[nodiscard]] auto *cubegin() const { return udata(); }

  /// Return the begin as `std::byte *`.
  [[nodiscard]] auto *bbegin() { return udata(); }

  /// Return the begin as `const std::byte *` or `std::byte *`, const-ness
  /// inherited from `Self_t::data() const`.
  [[nodiscard]] auto *bbegin() const { return udata(); }

  /// Return the begin as `const std::byte *` or `std::byte *`, const-ness
  /// inherited from `Self_t::data() const`.
  [[nodiscard]] auto *cbbegin() const { return udata(); }

  // ==== end members ====

  /// Return the end as `char *`.
  [[nodiscard]] auto *end() { return self().data() + self().size(); }

  /// Return the end as `const char *` or `char *`, const-ness inherited from
  /// `Self_t::data() const`.
  [[nodiscard]] auto *end() const { return self().data() + self().size(); }

  /// Return the end as `const char *` or `char *`, const-ness inherited from
  /// `Self_t::data() const`.
  [[nodiscard]] auto *cend() const { return self().data() + self().size(); }

  /// Return the end as `unsigned char *`.
  [[nodiscard]] auto *uend() { return udata() + self().size(); }

  /// Return the end as `const unsigned char *` or `unsigned char *`, const-ness
  /// inherited from `Self_t::data() const`.
  [[nodiscard]] auto *uend() const { return udata() + self().size(); }

  /// Return the end as `const unsigned char *` or `unsigned char *`, const-ness
  /// inherited from `Self_t::data() const`.
  [[nodiscard]] auto *cuend() const { return udata() + self().size(); }

  /// Return the end as `std::byte *`.
  [[nodiscard]] auto *bend() { return udata() + self().size(); }

  /// Return the end as `const std::byte *` or `std::byte *`, const-ness
  /// inherited from `Self_t::data() const`.
  [[nodiscard]] auto *bend() const { return udata() + self().size(); }

  /// Return the end as `const std::byte *` or  or `std::byte *`, const-ness
  /// inherited from `Self_t::data() const`.
  [[nodiscard]] auto *cbend() const { return udata() + self().size(); }

  // ==== operator[] ====

  /// Return reference the n'th element.
  [[nodiscard]] char &operator[](std::ptrdiff_t n) {
    assert(n >= 0);
    assert(n < self().size());
    return self().data()[n];
  }

  /// Return the n'th element, const-ness inherited from `Self_t::data() const`.
  [[nodiscard]] char operator[](std::ptrdiff_t n) const {
    assert(n >= 0);
    assert(n < self().size());
    return self().data()[n];
  }

 private:
  [[nodiscard]] Self_tp &self() { return static_cast<Self_tp &>(*this); }
  [[nodiscard]] const Self_tp &self() const {
    return static_cast<const Self_tp &>(*this);
  }
};  // class Buffer_interface

/// Enable fast comparison operators for `Buffer_interface` subclasses.
template <std::derived_from<detail::Buffer_base> Buffer_t>
  requires(Buffer_t::equality_algorithm == Equality_algorithm::fast)
inline auto operator<=>(const Buffer_t &left, const Buffer_t &right) {
  auto size_cmp = left.size() <=> right.size();
  if (size_cmp != 0) return size_cmp;
  return std::memcmp(left.data(), right.data(), left.size()) <=> 0;
}

/// Enable lexicographic comparison operators for `Buffer_interface` subclasses.
template <std::derived_from<detail::Buffer_base> Buffer_t>
  requires(Buffer_t::equality_algorithm == Equality_algorithm::lexicographic)
inline auto operator<=>(const Buffer_t &left, const Buffer_t &right) {
  return left.string_view() <=> right.string_view();
}

/// Enable operator== for `Buffer_interface` subclasses.
template <std::derived_from<detail::Buffer_base> Buffer_t>
  requires Buffer_t::equality_enabled
inline bool operator==(const Buffer_t &left, const Buffer_t &right) {
  if (left.size() != right.size()) return false;
  return std::memcmp(left.data(), right.data(), left.size()) == 0;
}

/// Enable operator!= for `Buffer_interface` subclasses.
template <std::derived_from<detail::Buffer_base> Buffer_t>
  requires Buffer_t::equality_enabled
inline bool operator!=(const Buffer_t &left, const Buffer_t &right) {
  return !(left == right);
}

}  // namespace mysql::ranges

/// Define std::hash<T> where T is a subclass of `Buffer_interface`.
//
// The recommended way to do this is to use a syntax that places the namespace
// as a name qualifier, like `struct std::hash<Gtid_t>`, rather than enclose the
// entire struct in a namespace block.
//
// However, gcc 11.4.0 on ARM has a bug that makes it produce "error:
// redefinition of 'struct std::hash<_Tp>'" when using that syntax. See
// https://godbolt.org/z/xo1v8rf6n vs https://godbolt.org/z/GzvrMese1 .
//
// Todo: Switch to the recommended syntax once we drop support for compilers
// having this bug.
//
// clang-tidy warns when not using the recommended syntax
// NOLINTBEGIN(cert-dcl58-cpp)
namespace std {
template <std::derived_from<mysql::ranges::detail::Buffer_base> Buffer_t>
  requires Buffer_t::hash_enabled
struct hash<Buffer_t> {
  std::size_t operator()(const Buffer_t &object) const {
    return std::hash<std::string_view>{}(object.string_view());
  }
};
}  // namespace std
// NOLINTEND(cert-dcl58-cpp)

// addtogroup GroupLibsMysqlRanges
/// @}

#endif  // ifndef MYSQL_RANGES_BUFFER_INTERFACE_H
