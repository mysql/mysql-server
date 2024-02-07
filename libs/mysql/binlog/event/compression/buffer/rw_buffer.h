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

/// @file rw_buffer.h
///
/// Non-owning manager for a fixed memory buffer, which is split into
/// a read part and a write part, with a movable split position.

#ifndef MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_RW_BUFFER_H
#define MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_RW_BUFFER_H

#include <cassert>

#include "buffer_view.h"

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{

namespace mysql::binlog::event::compression::buffer {

/// Non-owning read/write memory buffer manager with a fixed size.
///
/// This has a read/write position (which @c Buffer_view does not
/// have).  It does not have functionailty to grow the buffer (as @c
/// Managed_buffer has).
///
/// Objects have one internal contiguous buffer which is split into
/// two parts, each of which is a Buffer_view: the first part is the
/// read part and the second part is the write part. API clients may
/// write to the write part and then move the position forwards: this
/// will increase the read part so that API clients can read what was
/// just written, and decrease the write part so that next write will
/// happen after the position that was just written.
///
/// Generally, std::stringstream is a safer interface for buffers and
/// should be preferred when possible.  This class is intended for
/// interaction with C-like APIs that request a (possibly
/// uninitialized) memory buffer which they write to.
template <class Char_tp = unsigned char>
class Rw_buffer {
 public:
  using Char_t = Char_tp;
  using Size_t = std::size_t;
  using Difference_t = std::ptrdiff_t;
  using Iterator_t = Char_t *;
  using Const_iterator_t = const Char_t *;
  using Buffer_view_t =
      mysql::binlog::event::compression::buffer::Buffer_view<Char_t>;

  Rw_buffer() = default;

  /// Create a new Rw_buffer from the specified size and buffer.
  ///
  /// The read part will be 0 bytes at the beginning of the buffer,
  /// and the write part will be the full buffer.
  explicit Rw_buffer(Buffer_view_t buffer)
      : m_read_part(buffer.begin(), 0),
        m_write_part(buffer.begin(), buffer.size()) {}

  /// Deleted copy constructor.
  Rw_buffer(Rw_buffer &) = delete;

  /// Default move constructor.
  Rw_buffer(Rw_buffer &&) noexcept = default;

  /// Deleted copy assignment operator.
  Rw_buffer &operator=(Rw_buffer &) = delete;

  /// Default move assignment operator.
  Rw_buffer &operator=(Rw_buffer &&) noexcept = default;

  /// Default delete operator.
  virtual ~Rw_buffer() = default;

  /// Return the read part.
  const Buffer_view_t &read_part() const { return m_read_part; }

  /// Return the read part.
  Buffer_view_t &read_part() { return m_read_part; }

  /// Return the write part.
  const Buffer_view_t &write_part() const { return m_write_part; }

  /// Return the write part.
  Buffer_view_t &write_part() { return m_write_part; }

  /// Return the total size of the read part and the write part.
  Size_t capacity() const { return read_part().size() + write_part().size(); }

  /// Set the position to a fixed number.
  ///
  /// The position is the same as the size of the read part.
  ///
  /// This adjusts the read part and the write part, so that the read
  /// part size becomes equal to position, and the write part begins
  /// where the read part ends.
  ///
  /// The specified position must be between 0 and capacity(),
  /// inclusive.
  ///
  /// @note This alters the end iterator for the read part and the
  /// begin iterator for the write part.
  void set_position(Size_t new_position) {
    auto capacity = this->capacity();
    assert(new_position <= capacity);
    new_position = std::min(new_position, capacity);

    read_part() = Buffer_view_t(read_part().begin(), new_position);
    write_part() = Buffer_view_t(read_part().end(), capacity - new_position);
  }

  /// Increase the position right, relative to the currrent position.
  ///
  /// The position is the same as the size of the read part.
  ///
  /// This adjusts the read part and the write part, so that the read
  /// part size becomes equal to position, and the write part begins
  /// where the read part ends.
  ///
  /// The resulting new position must be less than or equal to
  /// capacity().
  ///
  /// @note This alters the end iterator for the read part and the
  /// begin iterator for the write part.
  void increase_position(Size_t increment) {
    auto read_size = read_part().size();
    assert(increment <= this->capacity() - read_size);
    auto new_position = read_size + increment;
    set_position(new_position);
  }

  /// Move the position left or right, relative to the current position.
  ///
  /// The position is the same as the size of the read part.
  ///
  /// This increments the right end of the read part by delta, and
  /// increments the left end of the write part by delta.
  ///
  /// The resulting new position must be between 0 and capacity(),
  /// inclusive.
  ///
  /// @note This alters the end iterator for the read part and the
  /// begin iterator for the write part.
  void move_position(Difference_t delta) {
    auto new_position = Difference_t(read_part().size()) + delta;
    assert(new_position >= 0);
    new_position = std::max(new_position, Difference_t(0));
    set_position(Size_t(new_position));
  }

 protected:
  Buffer_view_t m_read_part;
  Buffer_view_t m_write_part;
};

}  // namespace mysql::binlog::event::compression::buffer

/// @}

#endif  // MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_RW_BUFFER_H
