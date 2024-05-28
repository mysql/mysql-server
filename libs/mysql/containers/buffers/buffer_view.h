/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

/// @file
///
/// Class that groups a pointer+size as one object, without managing
/// the memory for it.

#ifndef MYSQL_CONTAINERS_BUFFERS_BUFFER_VIEW_H
#define MYSQL_CONTAINERS_BUFFERS_BUFFER_VIEW_H

#include <cassert>  // assert
#include <string>   // std::string
#ifndef NDEBUG
#include <sstream>  // std::ostringstream
#endif

/// @addtogroup GroupLibsMysqlContainers
/// @{

namespace mysql::containers::buffers {

/// Non-owning view of a memory buffer with a fixed size.
///
/// This is a minimal class holding just a pointer and a size.  It
/// does not have a read/write position (@see Rw_buffer_sequence).  It
/// does not have methods to grow the buffer (@see
/// Growable_buffer_sequence).
template <class Char_tp = unsigned char>
class Buffer_view {
 public:
  using Char_t = Char_tp;
  /// The 'size' type.  Keep this equal to
  /// Grow_calculator::Size_t
  using Size_t = std::size_t;
  using Iterator_t = Char_t *;
  using Const_iterator_t = const Char_t *;

  /// Create a new Buffer_view with the specified size and data.
  Buffer_view(Char_t *data, Size_t size) : m_data(data), m_size(size) {
    if (data == nullptr) assert(size == 0);
  }

  /// Create a new "null Buffer_view": pointer is nullptr and size is
  /// 0.
  Buffer_view() = default;

  /// Shallow copy constructor.
  ///
  /// @note The data pointer is copied but not the contents.
  Buffer_view(const Buffer_view &) = default;

  /// Default move constructor.
  Buffer_view(Buffer_view &&) noexcept = default;

  /// Shallow copy assignment operator.
  ///
  /// @note The data pointer is copied but not the contents.
  Buffer_view &operator=(const Buffer_view &) = default;

  /// Default move assignment operator.
  Buffer_view &operator=(Buffer_view &&) noexcept = default;

  /// Default delete operator.
  virtual ~Buffer_view() = default;

  /// Return const pointer to the data.
  const Char_t *data() const { return m_data; }

  /// Return non-const pointer to the data.
  Char_t *data() { return m_data; }

  /// Return pointer to the first character of the data.
  Iterator_t begin() { return m_data; }

  /// Return pointer to one-past-the-last character of the data.
  Iterator_t end() { return m_data + m_size; }

  /// Return pointer to the first character of the data.
  Const_iterator_t begin() const { return m_data; }

  /// Return pointer to one-past-the-last character of the data.
  Const_iterator_t end() const { return m_data + m_size; }

  /// Return const pointer to the first character of the data.
  Const_iterator_t cbegin() const { return m_data; }

  /// Return const pointer to one-past-the-last character of the data.
  Const_iterator_t cend() const { return m_data + m_size; }

  /// Return the number of bytes.
  Size_t size() const { return m_size; }

  /// Return a copy of this object as a `std::string`.
  template <class Str_char_t = char,
            class Str_traits_t = std::char_traits<Str_char_t>,
            class Str_allocator_t = std::allocator<Str_char_t>>
  std::basic_string<Str_char_t, Str_traits_t, Str_allocator_t> str(
      const Str_allocator_t &allocator = Str_allocator_t()) const {
    return std::basic_string<Str_char_t, Str_traits_t, Str_allocator_t>(
        reinterpret_cast<const Str_char_t *>(m_data), m_size, allocator);
  }

  /// In debug mode, return a string with debug info.
  ///
  /// @param show_contents If true, includes the buffer contents.
  /// Otherwise, just pointers and sizes.
  std::string debug_string([[maybe_unused]] bool show_contents = false) const {
#ifdef NDEBUG
    return "";
#else
    std::ostringstream ss;
    // clang-format off
    ss << "Buffer_view(ptr=" << (const void *)this
       << ", data=" << (const void *)data()
       << ", size=" << size();
    // clang-format on
    if (show_contents && begin() != nullptr)
      ss << ", contents=\""
         << std::string(reinterpret_cast<const char *>(begin()), size())
         << "\"";
    ss << ")";
    return ss.str();
#endif
  }

 private:
  /// Pointer to the data.
  Char_t *m_data{nullptr};

  /// The number of bytes.
  Size_t m_size{0};
};

}  // namespace mysql::containers::buffers

/// @}

#endif  // MYSQL_CONTAINERS_BUFFERS_BUFFER_VIEW_H
