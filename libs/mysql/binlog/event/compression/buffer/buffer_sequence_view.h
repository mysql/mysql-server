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

/// @file buffer_sequence_view.h
///
/// Container class that provides a sequence of buffers to the caller.
/// This is intended for capturing the output from compressors.

#ifndef MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_BUFFER_SEQUENCE_VIEW_H
#define MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_BUFFER_SEQUENCE_VIEW_H

#include <algorithm>                                // std::min
#include <cassert>                                  // assert
#include <cstring>                                  // std::memcpy
#include <limits>                                   // std::numeric_limits
#include <memory>                                   // std::allocator
#include <type_traits>                              // std::conditional
#include <vector>                                   // std::vector
#include "mysql/binlog/event/resource/allocator.h"  // mysql::binlog::event::resource::Allocator

#include "mysql/binlog/event/compression/buffer/buffer_view.h"  // buffer::Buffer_view
#include "mysql/binlog/event/compression/buffer/grow_calculator.h"  // buffer::Grow_calculator
#include "mysql/binlog/event/compression/buffer/grow_status.h"  // buffer::Grow_status
#include "mysql/binlog/event/nodiscard.h"                       // NODISCARD

#include "mysql/binlog/event/wrapper_functions.h"  // BAPI_TRACE

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{

namespace mysql::binlog::event::compression::buffer {

/// Sequence of memory buffers.
///
/// This is a minimal class with just a sequence of buffers.  It does
/// not have a read/write position (@see Rw_buffer_sequence).  It does
/// not have methods to grow the buffer sequence (@see
/// Managed_buffer_sequence).
///
/// @tparam Char_tp The type of elements stored in the buffer:
/// typically unsigned char.
///
/// @tparam Container_tp The type of container to hold the buffers.
/// This defaults to std::vector, but std::list is also possible.
///
/// @tparam const_tp If true, use const iterators instead of non-const
/// iterators to represent the beginning and end of the container.
template <class Char_tp = unsigned char,
          template <class Element_tp, class Allocator_tp> class Container_tp =
              std::vector,
          bool const_tp = false>
class Buffer_sequence_view {
 public:
  using Char_t = Char_tp;
  using Size_t = std::size_t;
  using Buffer_view_t = Buffer_view<Char_t>;
  using Buffer_allocator_t =
      mysql::binlog::event::resource::Allocator<Buffer_view_t>;
  using Container_t = Container_tp<Buffer_view_t, Buffer_allocator_t>;
  using Const_iterator_t = typename Container_t::const_iterator;
  using Iterator_t =
      typename std::conditional<const_tp, Const_iterator_t,
                                typename Container_t::iterator>::type;

 private:
  /// Indicates that @c m_size has not yet been computed.
  static constexpr Size_t uninitialized_size =
      std::numeric_limits<Size_t>::max();

 public:
  /// Construct a Buffer_sequence_view with buffers in the range given by
  /// the iterators.
  ///
  /// This copies only the iterators; the underlying container and the
  /// buffers contained in the container are not copied.
  ///
  /// @param begin_arg Iterator to the first buffer.
  ///
  /// @param end_arg Iterator to one-past-the-last buffer.
  ///
  /// @param size_arg The total size of all buffers from begin_arg to
  /// end_arg.  This is an optimization only: if the parameter is
  /// omitted, it will be computed the next time it is needed.
  Buffer_sequence_view(Iterator_t begin_arg, Iterator_t end_arg,
                       Size_t size_arg = uninitialized_size)
      : m_begin(begin_arg), m_end(end_arg), m_size(size_arg) {}

  // Disallow copy, implement move.
  Buffer_sequence_view(Buffer_sequence_view &) = delete;
  Buffer_sequence_view(Buffer_sequence_view &&other) noexcept = default;
  Buffer_sequence_view &operator=(Buffer_sequence_view &) = delete;
  Buffer_sequence_view &operator=(Buffer_sequence_view &&) noexcept = default;

  virtual ~Buffer_sequence_view() = default;

  /// Iterator to the first buffer.
  Iterator_t begin() { return m_begin; }

  /// Iterator to the last buffer.
  Iterator_t end() { return m_end; }

  /// Iterator to the first buffer.
  Const_iterator_t begin() const { return m_begin; }

  /// Iterator to the last buffer.
  Const_iterator_t end() const { return m_end; }

  /// Const iterator pointing to the first buffer.
  Const_iterator_t cbegin() const { return m_begin; }

  /// Const iterator pointing to the last buffer.
  Const_iterator_t cend() const { return m_end; }

  /// Copy all data to the given, contiguous output buffer.
  ///
  /// The caller is responsible for providing a buffer of at
  /// least @c size() bytes.
  ///
  /// @param destination The target buffer.
  template <class Destination_char_t>
  void copy(Destination_char_t *destination) const {
    BAPI_TRACE;
    Size_t position = 0;
    for (const auto &buffer : *this) {
      std::memcpy(destination + position, buffer.data(), buffer.size());
      position += buffer.size();
    }
  }

  /// Return a copy of all the data in this object, as a `std::string`
  /// object.
  template <class Str_char_t = char,
            class Str_traits_t = std::char_traits<Str_char_t>,
            class Str_allocator_t = std::allocator<Str_char_t>>
  std::basic_string<Str_char_t, Str_traits_t, Str_allocator_t> str(
      const Str_allocator_t &allocator = Str_allocator_t()) {
    std::basic_string<Str_char_t, Str_traits_t, Str_allocator_t> ret(
        this->size(), '\0', allocator);
    copy(ret.data());
    return ret;
  }

  /// Return the total size of all buffers.
  Size_t size() const {
    if (m_size == uninitialized_size) {
      Size_t size = 0;
      for (const auto &buffer : *this) size += buffer.size();
      m_size = size;
    }
    return m_size;
  }

  /// In debug mode, return a string that describes the internal
  /// structure of this object, to use for debugging.
  ///
  /// @param show_contents If true, includes the buffer contents.
  /// Otherwise, just pointers and sizes.
  ///
  /// @param indent If 0, put all info on one line. Otherwise, put
  /// each field on its own line and indent the given number of
  /// two-space levels.
  ///
  /// @return String that describes the internal structure of this
  /// Buffer_sequence_view.
  std::string debug_string([[maybe_unused]] bool show_contents = false,
                           [[maybe_unused]] int indent = 0) const {
#ifdef NDEBUG
    return "";
#else
    std::ostringstream ss;
    // whitespace following the comma: newline + indentation, or just space
    std::string ws;
    if (indent != 0)
      ws = std::string("\n") +
           std::string(static_cast<std::string::size_type>(indent * 2), ' ');
    else
      ws = " ";
    // separator = comma + whitespace
    std::string sep = "," + ws;
    // whitespace / separator with one level deeper indentation
    std::string ws2 = (indent != 0) ? (ws + "  ") : ws;
    std::string sep2 = (indent != 0) ? (sep + "  ") : sep;
    // clang-format off
    ss << "Buffer_sequence_view(ptr=" << (const void *)this
       << sep << "size=" << size()
       << sep << "buffers.ptr=" << (const void *)&*this->begin()
       << sep << "buffers=[";
    // clang-format on
    bool first = true;
    for (auto &buffer : *this) {
      if (first) {
        if (indent != 0) ss << ws2;
        first = false;
      } else {
        ss << sep2;
      }
      ss << buffer.debug_string(show_contents);
    }
    ss << "])";
    return ss.str();
#endif
  }

 private:
  /// Iterator to beginning of buffer.
  Iterator_t m_begin;

  /// Iterator to end of buffer.
  Iterator_t m_end;

  /// Total size of all buffers, cached.
  mutable Size_t m_size;
};

}  // namespace mysql::binlog::event::compression::buffer

/// @}

#endif  // MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_BUFFER_SEQUENCE_VIEW_H
