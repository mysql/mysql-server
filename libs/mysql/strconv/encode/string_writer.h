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

#ifndef MYSQL_STRCONV_ENCODE_STRING_WRITER_H
#define MYSQL_STRCONV_ENCODE_STRING_WRITER_H

/// @file
/// Experimental API header

#include <cassert>                               // assert
#include <cstring>                               // memcpy
#include <string_view>                           // string_view
#include "mysql/strconv/encode/out_str.h"        // Is_out_str
#include "mysql/strconv/encode/string_target.h"  // String_target_interface

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Class that serves as the target for encode(..., Is_string_target), which
/// writes to a `char *` buffer without bounds checking.
class String_writer : public detail::String_target_interface<String_writer> {
 protected:
  /// Construct a new object backed by the given buffer. The caller must ensure
  /// that the buffer has space for everything that will be written to it.
  ///
  /// This is hidden from user code. These objects are only meant to be
  /// created internally by the framework.
  ///
  /// @param first Pointer to the first character of the output buffer.
  ///
  /// @param last Pointer to the one-past-last character of the output buffer.
  String_writer(char *first, char *last) : m_pos(first), m_end(last) {}

 public:
  static constexpr Target_type target_type = Target_type::writer;

  // Allow move, but not copy.
  //
  // Deleting copy semantics protects against the mistake of making a
  // `encode_impl` function take its second parameter by value. And it does
  // not make sense to have two `String_writers` pointing to the same back-end
  // buffer.
  String_writer(const String_writer &) = delete;
  String_writer(String_writer &&) noexcept = default;
  String_writer &operator=(const String_writer &) = delete;
  String_writer &operator=(String_writer &&) noexcept = default;
  ~String_writer() = default;

  /// Append a string_view to the buffer, unformatted.
  ///
  /// @param sv String to write.
  void write_raw(const std::string_view &sv) {
    assert(remaining_size() >= sv.size());
    std::memcpy(m_pos, sv.data(), sv.size());
    advance(sv.size());
  }

  /// Append a single character to the buffer.
  ///
  /// @param ch Character to write.
  void write_char(int ch) {
    assert(m_end > m_pos);
    *m_pos = ch;
    advance(1);
  }

  /// Write the given object to this String_writer.
  void write(const Is_format auto &format, const auto &object) {
    assert(remaining_size() >= compute_encoded_length(format, object));
    resolve_format_and_write(format, object);
  }

  /// Write the given string_view to this String_writer.
  void write(const Is_format auto &format, const std::string_view &sv) {
    assert(remaining_size() >= compute_encoded_length(format, sv));
    resolve_format_and_write(format, sv);
  }

  /// Move the position `size` bytes forward without writing anything.
  ///
  /// @note The characters are left uninitialized. The caller must initialize
  /// them.
  void advance(std::size_t size) {
    assert(remaining_size() >= size);
    m_pos += size;
  }

  /// Return the current write position as a char *.
  [[nodiscard]] char *pos() { return m_pos; }

  /// Return the current write position as a const char *.
  [[nodiscard]] const char *pos() const { return m_pos; }

  /// Return the current write position as an unsigned char *.
  [[nodiscard]] unsigned char *upos() {
    return reinterpret_cast<unsigned char *>(m_pos);
  }

  /// Return the current write position as a const unsigned char *.
  [[nodiscard]] const unsigned char *upos() const {
    return reinterpret_cast<unsigned char *>(m_pos);
  }

  /// Return the current write position as a std::byte *.
  [[nodiscard]] std::byte *bpos() {
    return reinterpret_cast<std::byte *>(m_pos);
  }

  /// Return the current write position as a const std::byte *.
  [[nodiscard]] const std::byte *bpos() const {
    return reinterpret_cast<std::byte *>(m_pos);
  }

  /// Return the buffer end as a char *.
  [[nodiscard]] char *end() { return m_end; }

  /// Return the buffer end as a const char *.
  [[nodiscard]] const char *end() const { return m_end; }

  /// Return the buffer end as an unsigned char *.
  [[nodiscard]] unsigned char *uend() {
    return reinterpret_cast<unsigned char *>(m_end);
  }

  /// Return the buffer end as a const unsigned char *.
  [[nodiscard]] const unsigned char *uend() const {
    return reinterpret_cast<unsigned char *>(m_end);
  }

  /// Return the buffer end as a std::byte *.
  [[nodiscard]] std::byte *bend() {
    return reinterpret_cast<std::byte *>(m_end);
  }

  /// Return the buffer end as a const std::byte *.
  [[nodiscard]] const std::byte *bend() const {
    return reinterpret_cast<std::byte *>(m_end);
  }

  // Returning the distance from the current position to the end of the buffer.
  [[nodiscard]] std::size_t remaining_size() const {
    return std::size_t(m_end - m_pos);
  }

 private:
  /// The current write position.
  char *m_pos;
  /// End of buffer.
  char *m_end;
};  // class String_writer

}  // namespace mysql::strconv

namespace mysql::strconv::detail {

/// String_writer subclass that can be instantiated.
///
/// We hide this in the detail namespace because the class is not supposed to be
/// instantiated in user code, only in this framework.
class Constructible_string_writer : public String_writer {
 public:
  explicit Constructible_string_writer(const Is_out_str auto &out_str)
      : String_writer(out_str.data(), out_str.end()) {}
};

static_assert(!std::copy_constructible<Constructible_string_writer>);
static_assert(!std::copy_constructible<String_writer>);
static_assert(std::movable<Constructible_string_writer>);
static_assert(std::movable<String_writer>);

}  // namespace mysql::strconv::detail

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_STRING_WRITER_H
