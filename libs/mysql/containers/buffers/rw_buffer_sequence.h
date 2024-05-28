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
/// Container class that provides a sequence of buffers to the
/// caller.  This is intended for capturing the output from
/// compressors.

#ifndef MYSQL_CONTAINERS_BUFFERS_RW_BUFFER_SEQUENCE_H
#define MYSQL_CONTAINERS_BUFFERS_RW_BUFFER_SEQUENCE_H

#include <algorithm>  // std::min
#include <cassert>    // assert
#include <cstring>    // std::memcpy
#include <limits>     // std::numeric_limits
// Use when all platforms support it:
// #include <memory_resource>  // std::pmr::polymorphic_allocator
#include <list>    // std::list
#include <vector>  // std::vector
#include "mysql/allocators/allocator.h"

#include "mysql/containers/buffers/buffer_sequence_view.h"  // Buffer_sequence_view
#include "mysql/containers/buffers/buffer_view.h"           // Buffer_view
#include "mysql/containers/buffers/grow_calculator.h"       // Grow_calculator
#include "mysql/containers/buffers/grow_status.h"           // Grow_status
#include "mysql/utils/nodiscard.h"                          // NODISCARD

#include "mysql/binlog/event/wrapper_functions.h"  // BAPI_TRACE

/// @addtogroup GroupLibsMysqlContainers
/// @{

namespace mysql::containers::buffers {

/// Non-owning manager for a fixed sequence of memory buffers, which
/// is split into a read part and a write part, with a movable split
/// position.
///
/// This has a read/write position (which @c Buffer_sequence does not
/// have).  It does not have functionailty to grow the buffer sequence
/// (as @c Managed_buffer_sequence has).
///
/// Objects have one internal contiguous container (vector or list),
/// which is split into two parts, each of which is a
/// Buffer_sequence_view: the first part is the read part and the
/// second part is the write part.  API clients typically write to the
/// write part and then move the position forwards: this will increase
/// the read part so that API clients can read what was just written,
/// and decrease the write part so that next write will happen after
/// the position that was just written.
///
/// The position that defines the end of the read part and the
/// beginning of the write part has byte granularity.  Therefore, it
/// does not have to be at a buffer boundary.  When it is not on a
/// buffer boundary, a buffer has to be split.  When a buffer is
/// split, the sequence needs one more element than the actual number
/// of (contiguous) buffers.  When the position is moved from a buffer
/// boundary to a non-boundary, the number of used elements increases
/// by one.  To avoid having to shift all the buffers for the write
/// part one step right when the container is a vector, there is
/// *always* one more element than the actual number of contiguous
/// buffers.  So when the position is at a buffer boundary, an unused
/// "null buffer" is stored between the read part and the write part.
/// In other words, the buffer sequence has one of the following
/// forms:
///
/// ```
/// Position at buffer boundary:
/// Here, there are N buffers where the first R ones are read buffers.
///   [b_1, ..., b_R, null, b_{R+1}, ..., b_N]
///
/// Position not at buffer boundary:
/// Here there are N buffers where the first R-1 ones are read buffers,
/// and the R'th one is split into a read part and a write part.
///   [b_1, ..., b_R[0..x], b_R[x..], b_{R+1}, ..., b_N]
///```
///
/// @tparam Char_tp the type of elements stored in the buffer:
/// typically unsigned char.
///
/// @tparam Container_tp the type of container: either `std::vector`
/// or `std::list`.
template <class Char_tp = unsigned char,
          template <class Element_tp, class Allocator_tp> class Container_tp =
              std::vector>
class Rw_buffer_sequence {
 public:
  using Buffer_sequence_view_t = Buffer_sequence_view<Char_tp, Container_tp>;
  using Char_t = typename Buffer_sequence_view_t::Char_t;
  using Size_t = typename Buffer_sequence_view_t::Size_t;
  using Difference_t = std::ptrdiff_t;
  using Buffer_view_t = typename Buffer_sequence_view_t::Buffer_view_t;
  using Buffer_allocator_t =
      typename Buffer_sequence_view_t::Buffer_allocator_t;
  using Container_t = typename Buffer_sequence_view_t::Container_t;
  using Iterator_t = typename Buffer_sequence_view_t::Iterator_t;
  using Const_iterator_t = typename Buffer_sequence_view_t::Const_iterator_t;
  using Const_buffer_sequence_view_t =
      Buffer_sequence_view<Char_tp, Container_tp, true>;

  /// Construct a new Rw_buffer_sequence from given endpoint
  /// iterators, with position 0.
  ///
  /// The provided sequence of buffers must start with one null buffer,
  /// followed by zero or more non-null buffers.
  ///
  /// @param begin_arg Iterator to the beginning of the sequence.
  ///
  /// @param end_arg Iterator to one-past-the-end of the sequence.
  Rw_buffer_sequence(Iterator_t begin_arg, Iterator_t end_arg)
      : m_read_part(begin_arg, begin_arg),
        m_write_part(std::next(begin_arg), end_arg) {
#ifndef NDEBUG
    assert(begin_arg != end_arg);
    assert(begin_arg->data() == nullptr);
    assert(begin_arg->size() == 0);
    for (auto it = std::next(begin_arg); it != end_arg; ++it) {
      assert(it->data() != nullptr);
      assert(it->size() != 0);
    }
#endif
  }

  // Disallow copy/move.  We can implement these in the future if we
  // need them.
  Rw_buffer_sequence(Rw_buffer_sequence &) = delete;
  Rw_buffer_sequence(Rw_buffer_sequence &&) = delete;
  Rw_buffer_sequence &operator=(Rw_buffer_sequence &) = delete;
  Rw_buffer_sequence &operator=(Rw_buffer_sequence &&) = delete;

  virtual ~Rw_buffer_sequence() = default;

  /// Set the specified absolute position.
  ///
  /// "Position" is a synonym for "size of the read part".
  ///
  /// @note This may alter the end iterator of the read part, the
  /// begin iterator of the write part, as well as the begin/end
  /// iterators of buffers between the current position and the new
  /// position.  The beginning of the read part, the end of the write
  /// part, and buffers outside the range between the new and old
  /// position remain unchanged.
  ///
  /// @param new_position The new size of the read part.  This must be
  /// within bounds; otherwise an assertion is raised in debug build,
  /// or the position forced to the end in non-debug build.
  void set_position(Size_t new_position) {
    set_position(new_position, this->read_part(), this->write_part());
  }

  /// Move the position right, relative to the current position.
  ///
  /// "Position" is a synonym for "size of the read part".
  ///
  /// @note This may alter the end iterator of the read part, the
  /// begin iterator of the write part, as well as the begin/end
  /// iterators of buffers between the current position and the new
  /// position.  The beginning of the read part, the end of the write
  /// part, and buffers outside the range between the new and old
  /// position remain unchanged.
  ///
  /// @param delta The number of bytes to add to the position.  The
  /// resulting size must be within bounds; otherwise an assertion is
  /// raised in debug build, or the position forced to the
  /// beginning/end in non-debug build.
  void increase_position(Size_t delta) {
    set_position(read_part().size() + delta);
  }

  /// Move the position left or right, relative to the current
  /// position.
  ///
  /// "Position" is a synonym for "size of the read part".
  ///
  /// @note This may alter the end iterator of the read part, the
  /// begin iterator of the write part, as well as the begin/end
  /// iterators of buffers between the current position and the new
  /// position.  The beginning of the read part, the end of the write
  /// part, and buffers outside the range between the new and old
  /// position remain unchanged.
  ///
  /// @param delta The number of bytes to add to the position.  The
  /// resulting size must be within bounds; otherwise an assertion is
  /// raised in debug build, or the position forced to the
  /// beginning/end in non-debug build.
  void move_position(Difference_t delta) {
    BAPI_TRACE;
    Difference_t new_position = Difference_t(read_part().size()) + delta;
    assert(new_position >= Difference_t(0));
    new_position = std::max(new_position, Difference_t(0));
    set_position(Size_t(new_position));
  }

  /// Return the current size, i.e., total size of all buffers.
  Size_t capacity() const { return read_part().size() + write_part().size(); }

  /// Return a const reference to the read part.
  const Buffer_sequence_view_t &read_part() const { return m_read_part; }

  /// Return a non-const reference to the read part.
  Buffer_sequence_view_t &read_part() { return m_read_part; }

  /// Return a const reference to the write part.
  const Buffer_sequence_view_t &write_part() const { return m_write_part; }

  /// Return a non-const reference to the write part.
  Buffer_sequence_view_t &write_part() { return m_write_part; }

  /// In debug mode, return a string that describes the internal
  /// structure of this object, to use for debugging.
  ///
  /// @param show_contents If true, includes the buffer contents.
  /// Otherwise, just pointers and sizes.
  ///
  /// @param indent If 0, put all info on one line. Otherwise, put
  /// each field on its own line and indent the given number of
  /// two-space levels.
  virtual std::string debug_string([[maybe_unused]] bool show_contents,
                                   [[maybe_unused]] int indent) const {
#ifdef NDEBUG
    return "";
#else
    std::ostringstream ss;
    std::string sep;
    if (indent != 0)
      sep = std::string(",\n") +
            std::string(static_cast<std::string::size_type>(indent * 2), ' ');
    else
      sep = ", ";
    int next_indent = (indent != 0) ? indent + 1 : 0;
    // clang-format off
    ss << "Rw_buffer_sequence(ptr=" << (const void *)this
       << sep << "capacity=" << capacity()
       << sep << "read_part="
              << read_part().debug_string(show_contents, next_indent)
       << sep << "between_r_and_w="
              << Const_buffer_sequence_view_t(
                   read_part().end(), write_part().begin())
                 .debug_string(show_contents)
       << sep << "write_part="
       << write_part().debug_string(show_contents, next_indent)
       << ")";
    // clang-format on
    return ss.str();
#endif
  }

  /// In debug mode, return a string that describes the internal
  /// structure of this object, to use for debugging.
  ///
  /// @param show_contents If true, includes the buffer contents.
  /// Otherwise, just pointers and sizes.
  std::string debug_string([[maybe_unused]] bool show_contents = false) const {
    return debug_string(show_contents, 0);
  }

 protected:
  /// Move the position to the given, absolute position.
  ///
  /// @note This may alter the end iterator of the left part, the
  /// begin iterator of the right part, as well as the begin/end
  /// iterators of buffers between the current position and the new
  /// position.  The beginning of the left part, the end of the right
  /// part, and buffers outside the range between the new and old
  /// position remain unchanged.
  ///
  /// @param new_position The new position.  This must be within
  /// bounds.  Otherwise an assertion is raised in debug build; in
  /// non-debug build, the position is forced to the end.
  ///
  /// @param left The left buffer sequence.
  ///
  /// @param right The right buffer sequence.
  ///
  /// @note One of the following must hold: (1) the container has one
  /// element between the left part and the right part, and that
  /// element is a null buffer; (2) left.end()==right.begin() &&
  /// left.end().end()==right.begin().begin().
  static void set_position(Size_t new_position, Buffer_sequence_view_t &left,
                           Buffer_sequence_view_t &right) {
    BAPI_TRACE;
    Size_t position = left.size();
    Size_t capacity = position + right.size();

    assert(new_position <= capacity);
    new_position = std::min(new_position, capacity);

    // If a buffer is split between the read and write parts, merge it.
    position += merge_if_split(left, right);
    assert(position >= 0);
    // Move position left, one buffer at a time, until it becomes less
    // than or equal to new_position.
    while (position > new_position)
      position -= move_position_one_buffer_left(left, right);

    // Move position right, one buffer at a time, until it becomes
    // equal to new_position.  If new_position is not at a buffer
    // boundary, the function will split the buffer.  Therefore, this
    // never makes position greater than new_position, so finally the
    // position is equal to new_position.
    while (position < new_position) {
      position += move_position_at_most_one_buffer_right(
          left, right, new_position - position);
    }
    assert(position == new_position);
  }

  /// If a buffer is split between the read and write parts, glue the
  /// pieces together again and include them in the read part.
  ///
  /// If no buffer is split (the position is at a buffer boundary),
  /// does nothing.
  ///
  /// Graphically, the operation is as follows:
  ///
  /// ```
  /// +-----------------+-----------------+
  /// | b1[0..split]    | b1[split..]     |
  /// +-----------------+-----------------+
  ///                     ^ read_end
  ///                     ^ write_begin
  /// -->
  /// +-----------------+-----------------+
  /// | b1              | null            |
  /// +-----------------+-----------------+
  ///                     ^ read_end        ^ write_begin
  /// ```
  ///
  /// @param left The left buffer sequence.
  ///
  /// @param right The right buffer sequence.
  ///
  /// @return The number of bytes that the position was moved left.
  static Size_t merge_if_split(Buffer_sequence_view_t &left,
                               Buffer_sequence_view_t &right) {
    auto [read_begin, read_end, read_size] = get_boundaries(left);
    auto [write_begin, write_end, write_size] = get_boundaries(right);
    if (read_end == write_begin) {
      auto delta = write_begin->size();
      auto before_read_end = read_end;
      --before_read_end;
      *before_read_end = Buffer_view_t(before_read_end->data(),
                                       before_read_end->size() + delta);
      *read_end = Buffer_view_t();
      ++write_begin;
      left = Buffer_sequence_view_t(read_begin, read_end, read_size + delta);
      right =
          Buffer_sequence_view_t(write_begin, write_end, write_size - delta);
      return delta;
    }
    return 0;
  }

  /// Move the position exactly one buffer left, assuming no buffer is
  /// split.
  ///
  /// Graphically, the operation is as follows:
  ///
  /// ```
  /// +-----------------+-----------------+
  /// | b1              | null            |
  /// +-----------------+-----------------+
  ///                     ^ read_end        ^ write_begin
  /// -->
  /// +-----------------+-----------------+
  /// | null            | b1              |
  /// +-----------------+-----------------+
  ///   ^ read_end        ^ write_begin
  /// ```
  ///
  /// @param left The left buffer sequence.
  ///
  /// @param right The right buffer sequence.
  ///
  /// @return The number of bytes that the position was moved left.
  static Size_t move_position_one_buffer_left(Buffer_sequence_view_t &left,
                                              Buffer_sequence_view_t &right) {
    auto [read_begin, read_end, read_size] = get_boundaries(left);
    auto [write_begin, write_end, write_size] = get_boundaries(right);
    assert(read_end != write_begin);
    assert(read_end->data() == nullptr);
    assert(read_end != read_begin);
    --read_end;
    --write_begin;
    *write_begin = *read_end;
    *read_end = Buffer_view_t();
    auto delta = write_begin->size();
    left = Buffer_sequence_view_t(read_begin, read_end, read_size - delta);
    right = Buffer_sequence_view_t(write_begin, write_end, write_size + delta);
    return delta;
  }

  /// Move the position right by whatever is smaller: the given
  /// number, or one buffer; splits the buffer if the number is
  /// smaller than the buffer.
  ///
  /// @param left The left buffer sequence.
  ///
  /// @param right The right buffer sequence.
  ///
  /// @param limit Move the position at most this number of bytes right.
  ///
  /// @return The number of bytes that the position was moved right.
  static Size_t move_position_at_most_one_buffer_right(
      Buffer_sequence_view_t &left, Buffer_sequence_view_t &right,
      Size_t limit) {
    auto [read_begin, read_end, read_size] = get_boundaries(left);
    auto [write_begin, write_end, write_size] = get_boundaries(right);
    assert(read_end != write_begin);
    assert(read_end->data() == nullptr);
    if (write_begin->size() <= limit) {
      // +-----------------+-----------------+
      // | null            | b1              |
      // +-----------------+-----------------+
      //   ^ read_end        ^ write_begin
      // -->
      // +-----------------+-----------------+
      // | b1              | null            |
      // +-----------------+-----------------+
      //                     ^ read_end        ^ write_begin
      auto delta = write_begin->size();
      *read_end = *write_begin;
      *write_begin = Buffer_view_t();
      ++read_end;
      ++write_begin;
      left = Buffer_sequence_view_t(read_begin, read_end, read_size + delta);
      right =
          Buffer_sequence_view_t(write_begin, write_end, write_size - delta);
      return delta;
    }
    // +-----------------+-----------------+
    // | null            | b1              |
    // +-----------------+-----------------+
    //   ^ read_end        ^ write_begin
    // -->
    // +-----------------+-----------------+
    // | b1[0..limit]    | b1[limit..]     |
    // +-----------------+-----------------+
    //                     ^ read_end
    //                     ^ write_begin
    *read_end = Buffer_view_t(write_begin->data(), limit);
    *write_begin =
        Buffer_view_t(write_begin->data() + limit, write_begin->size() - limit);
    ++read_end;
    left = Buffer_sequence_view_t(read_begin, read_end, read_size + limit);
    right = Buffer_sequence_view_t(write_begin, write_end, write_size - limit);
    return limit;
  }

  /// Return the beginning, end, and size of the read and write parts.
  ///
  /// @param buffer_sequence_view The buffer sequence view.
  ///
  /// @return 3-tuple containing the beginning, size, and end of the
  /// given buffer_sequence_view.
  std::tuple<Iterator_t, Iterator_t, Size_t> static get_boundaries(
      Buffer_sequence_view_t &buffer_sequence_view) {
    return std::make_tuple(buffer_sequence_view.begin(),
                           buffer_sequence_view.end(),
                           buffer_sequence_view.size());
  }

 private:
  /// buffer_sequence_view for the (leading) read segment.
  Buffer_sequence_view_t m_read_part;
  /// buffer_sequence_view for the (trailing) write segment.
  Buffer_sequence_view_t m_write_part;
};

}  // namespace mysql::containers::buffers

/// @}

#endif  // MYSQL_CONTAINERS_BUFFERS_RW_BUFFER_SEQUENCE_H
