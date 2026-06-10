// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_STRCONV_DECODE_PARSE_POSITION_H
#define MYSQL_STRCONV_DECODE_PARSE_POSITION_H

/// @file
/// Experimental API header

#include <cassert>                               // assert
#include <cstring>                               // strlen
#include <iterator>                              // contiguous_iterator_tag
#include <string_view>                           // string_view
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface
#include "mysql/utils/char_cast.h"               // uchar_cast

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv::detail {

/// Base class for the current position of a string parser, holding both the
/// parsed string and the position within the parsed string. Subclasses are
/// contiguous iterators over the characters in the parsed string.
///
/// @tparam Self_tp The subclass inheriting from this class.
template <class Self_tp>
class Parse_position : public mysql::iterators::Iterator_interface<Self_tp> {
 public:
  /// Construct a new Parse_position from the given range.
  ///
  /// @param source Source string.
  ///
  /// @param int_pos Current position. Defaults to 0, i.e., the beginning.
  explicit Parse_position(const std::string_view &source,
                          std::size_t int_pos = 0)
      : m_source(source), m_int_pos(int_pos) {
    assert(int_pos <= source.size());
  }

  // ==== Iterator implementation ====
  //
  // Functions required by Iterator_interface to implement a contiguous iterator
  // with a sentinel type.

  using Iterator_category_t = std::contiguous_iterator_tag;

  /// Construct a new object. The object cannot be used for anything besides
  /// assigning another object to it.
  Parse_position() = default;

  /// Dereference the iterator and return the value.
  [[nodiscard]] const char *get_pointer() const {
    assert(int_pos() < m_source.size());
    return pos();
  }

  /// Move the iterator delta steps.
  void advance(std::ptrdiff_t delta) {
    m_int_pos += delta;
    assert(int_pos() <= m_source.size());
  }

  /// Return the distance from iterator other to this.
  [[nodiscard]] std::ptrdiff_t distance_from(
      const Parse_position &other) const {
    assert(other.m_source == other.m_source);
    return m_int_pos - other.m_int_pos;
  }

  /// Return true if this iterator is at the end.
  [[nodiscard]] bool is_sentinel() const {
    return m_int_pos == m_source.size();
  }

  // ==== Absolute position ====
  //
  // Access to the iterator, relative to the beginning of the string.

  /// Set the position to the given one.
  void set_int_pos(std::size_t int_pos_arg) {
    assert(int_pos_arg <= str_size());
    m_int_pos = int_pos_arg;
  }

  /// Return the current position as an integer.
  [[nodiscard]] std::size_t int_pos() const { return m_int_pos; }

  /// Return the current position as a char pointer.
  [[nodiscard]] const char *pos() const { return begin() + int_pos(); }

  /// Return the current position as an unsigned char pointer.
  [[nodiscard]] const unsigned char *upos() const {
    return mysql::utils::uchar_cast(pos());
  }

  /// Return the current position as an std::byte pointer.
  [[nodiscard]] const std::byte *bpos() const {
    return mysql::utils::byte_cast(pos());
  }

  // ==== View over parsed string ====

  /// Return pointer to the beginning of the underlying string.
  [[nodiscard]] const char *begin() const { return m_source.data(); }

  /// Return pointer to the beginning of the underlying string.
  [[nodiscard]] const unsigned char *ubegin() const {
    return mysql::utils::uchar_cast(m_source.data());
  }

  /// Return pointer to the beginning of the underlying string.
  [[nodiscard]] const std::byte *bbegin() const {
    return mysql::utils::byte_cast(m_source.data());
  }

  /// Return pointer to the end of the underlying string.
  [[nodiscard]] const char *end() const {
    return m_source.data() + m_source.size();
  }

  /// Return pointer to the end of the underlying string.
  [[nodiscard]] const unsigned char *uend() const {
    return ubegin() + m_source.size();
  }

  /// Return pointer to the end of the underlying string.
  [[nodiscard]] const std::byte *bend() const {
    return bbegin() + m_source.size();
  }

  /// Return the remaining size.
  [[nodiscard]] std::size_t remaining_size() const { return end() - pos(); }

  /// Return the length of the underlying string.
  [[nodiscard]] std::size_t str_size() const { return m_source.size(); }

  /// Return a string_view over the left part of the string, up to the position.
  [[nodiscard]] std::string_view parsed_str() const { return {begin(), pos()}; }

  /// Return a string_view over the remaining string.
  [[nodiscard]] std::string_view remaining_str() const {
    return {pos(), end()};
  }

  /// Return a string_view over the underlying string.
  [[nodiscard]] std::string_view str() const { return m_source; }

 private:
  /// The beginning of the range.
  std::string_view m_source{};

  /// The current position.
  std::size_t m_int_pos{};
};  // class Parse_position

}  // namespace mysql::strconv::detail

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_PARSE_POSITION_H
