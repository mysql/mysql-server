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
/// @brief Container class that provides a sequence of buffers to
/// the caller.

#ifndef MYSQL_CONTAINERS_BUFFERS_MANAGED_BUFFER_SEQUENCE_H
#define MYSQL_CONTAINERS_BUFFERS_MANAGED_BUFFER_SEQUENCE_H

#include <algorithm>  // std::min
#include <cassert>    // assert
#include <cstring>    // std::memcpy
#include <limits>     // std::numeric_limits
#include <vector>     // std::vector

#include "mysql/allocators/allocator.h"                   // Allocator
#include "mysql/allocators/memory_resource.h"             // Memory_resource
#include "mysql/containers/buffers/buffer_view.h"         // Buffer_view
#include "mysql/containers/buffers/grow_calculator.h"     // Grow_calculator
#include "mysql/containers/buffers/grow_status.h"         // Grow_status
#include "mysql/containers/buffers/rw_buffer_sequence.h"  // Rw_buffer_sequence
#include "mysql/utils/nodiscard.h"                        // NODISCARD

#include "mysql/binlog/event/wrapper_functions.h"  // BAPI_TRACE

/// @addtogroup GroupLibsMysqlContainers
/// @{

namespace mysql::containers::buffers {

// Forward declare Accessor so we can make it a friend
namespace managed_buffer_sequence::unittest {
template <class T>
class Accessor;
}  // namespace managed_buffer_sequence::unittest

/// Owned, non-contiguous, growable memory buffer.
///
/// This class never moves buffer data, but is non-contiguous.  It is
/// implemented as a container of Buffer objects.
///
/// Objects have a growable size, a movable position, and two
/// buffer_sequence_view objects called the read part and the write
/// part, which are accessible through the member functions @c
/// read_part and @c write_part.  The read part is everything
/// preceding the position, and the write part is everything following
/// the position.  API clients acting as producers should write to the
/// write part and then move the position forward as many bytes as it
/// wrote.  API clients acting as consumers should read the read part.
///
/// Generally, std::ostringstream or std::vector<char> are safer and
/// simpler interfaces for buffers and should be preferred when
/// possible.  However they do not fit all use cases:
///
/// - std::ostringstream is preferrable and more convenient when
///   appending existing data to the stream.  But it is not suitable
///   for interaction with C-like APIs that produce data in a char*
///   given by the caller.  The user would need to allocate a buffer
///   outside the ostringsteam and then append the buffer to the
///   ostringstream, which would imply unnecessary memory and cpu
///   overheads.
///
/// - When using a C-like API that produces data in a char* given by
///   the caller, std::vector is often good.  The user can reserve as
///   much memory as needed and then pass the underlying data array to
///   the API.  However, the following properties are sometimes
///   advantageous for Managed_buffer_sequence:
///
///   - Vector has no practical way to put an exact bound on the
///     memory usage.  Managed_buffer_sequence uses a Grow_calculator
///     which allows exact control over memory usage, including a
///     maximum size.
///
///   - Vector has to copy all existing data when it grows.
///     Managed_buffer_sequence never needs to copy data.  Since it
///     allows data to be non-contigous, it retains existing buffers
///     while allocating new ones.
///
/// The main drawbacks of Managed_buffer_sequence are that it is
/// non-standard, has a minimal feature set, and is non-contiguous.
///
/// This class never throws any exception.
///
/// @tparam Char_tp the type of elements stored in the buffer:
/// typically unsigned char.
///
/// @tparam Container_tp The type of container to hold the buffers.
/// This defaults to std::vector, but std::list is also possible.
template <class Char_tp = unsigned char,
          template <class Element_tp, class Allocator_tp> class Container_tp =
              std::vector>
class Managed_buffer_sequence
    : public Rw_buffer_sequence<Char_tp, Container_tp> {
 public:
  using Rw_buffer_sequence_t = Rw_buffer_sequence<Char_tp, Container_tp>;
  // Would prefer to use:
  // using typename Rw_buffer_sequence_t::Buffer_sequence_t;
  // But that doesn't compile on Windows (maybe a compiler bug).
  using Buffer_sequence_view_t = Buffer_sequence_view<Char_tp, Container_tp>;
  using typename Rw_buffer_sequence_t::Buffer_view_t;
  using typename Rw_buffer_sequence_t::Char_t;
  using typename Rw_buffer_sequence_t::Const_iterator_t;
  using typename Rw_buffer_sequence_t::Container_t;
  using typename Rw_buffer_sequence_t::Iterator_t;
  using typename Rw_buffer_sequence_t::Size_t;
  using Grow_calculator_t = Grow_calculator;
  using Buffer_allocator_t =
      typename Buffer_sequence_view_t::Buffer_allocator_t;
  using Char_allocator_t = mysql::allocators::Allocator<Char_t>;
  using Memory_resource_t = mysql::allocators::Memory_resource;

  /// Construct a new, empty object.
  ///
  /// @param grow_calculator the policy to determine how much memory to
  /// allocate, when new memory is needed
  ///
  /// @param memory_resource The memory_resource used to allocate new
  /// memory, both for the container and for the buffers.
  ///
  /// @param default_buffer_count The initial size of the container.
  /// This preallocates the container but not the buffers contained in
  /// it.
  explicit Managed_buffer_sequence(
      const Grow_calculator_t &grow_calculator = Grow_calculator_t(),
      const Memory_resource_t &memory_resource = Memory_resource_t(),
      const Size_t default_buffer_count = 16)
      : Managed_buffer_sequence(
            Container_t(std::max(default_buffer_count, Size_t(1)),
                        Buffer_allocator_t(memory_resource)),
            grow_calculator, memory_resource) {}

  // Disallow copy/move.  We can implement these in the future if we
  // need them.
  Managed_buffer_sequence(Managed_buffer_sequence &) = delete;
  Managed_buffer_sequence(Managed_buffer_sequence &&) = delete;
  Managed_buffer_sequence &operator=(Managed_buffer_sequence &) = delete;
  Managed_buffer_sequence &operator=(Managed_buffer_sequence &&) = delete;

  ~Managed_buffer_sequence() override { this->reset(0); }

  /// Ensure the write part has at least the given size.
  ///
  /// This is only a convenience wrapper around @c
  /// reserve_total_size.
  ///
  /// @param requested_write_size The requested size of the write
  /// part.
  ///
  /// @retval success The write part now has at least the requested
  /// size.  The object may have been resized, following the rules of
  /// the Grow_calculator.
  ///
  /// @retval exceeds_max_size Either size() or read_part.size() +
  /// requested_write_size exceeds the max size configured in the
  /// Grow_calculator.  The object is unchanged.
  ///
  /// @retval out_of_memory The request could only be fulfilled by
  /// allocating more memory, but memory allocation failed.  The
  /// object is unchanged.
  [[NODISCARD]] Grow_status reserve_write_size(Size_t requested_write_size) {
    auto read_size = this->read_part().size();
    if (requested_write_size > std::numeric_limits<Size_t>::max() - read_size)
      return Grow_status::exceeds_max_size;
    return reserve_total_size(read_size + requested_write_size);
  }

  /// Ensure the total capacity - the sum of sizes of read part and
  /// write part - is at least the given number.
  ///
  /// This may add a new buffer if needed.  When the previous size is
  /// less than the default size, this may even add two buffers: the
  /// default buffer and one more.  Therefore, the caller should not
  /// assume that all the added size resides within one buffer.
  ///
  /// Existing buffer data will not move.  The container of buffers
  /// may grow, which may move the Buffer objects which hold pointers
  /// to the data.  Therefore, all iterators are invalidated by this.
  ///
  /// @param requested_total_size The requested total size of all read
  /// and write buffers.
  ///
  /// @retval success The object now has at least the requested total
  /// size.  The object may have been resized.
  ///
  /// @retval exceeds_max_size The existing size or the requested size
  /// exceeds either the maximum size.  The object is unchanged.
  ///
  /// @retval out_of_memory The request could only be fulfilled by
  /// allocating more memory, but memory allocation failed.  The
  /// object is unchanged.
  [[NODISCARD]] Grow_status reserve_total_size(Size_t requested_total_size) {
    auto work = [&] {
      auto capacity = this->capacity();
      auto [error, new_capacity] =
          m_grow_calculator.compute_new_size(capacity, requested_total_size);
      if (error) return Grow_status::exceeds_max_size;
      if (new_capacity > capacity) {
        if (allocate_and_add_buffer(new_capacity - capacity))
          return Grow_status::out_of_memory;
      }
      return Grow_status::success;
    };
    auto ret = work();
    BAPI_LOG("info", BAPI_VAR(ret)
                         << " " << BAPI_VAR(requested_total_size) << " "
                         << BAPI_VAR(this->capacity()) << " "
                         << BAPI_VAR(m_grow_calculator.get_max_size()));
    return ret;
  }

  /// Reset the read part and the write part to size 0.
  ///
  /// This optionally keeps a given number of allocated buffers in the
  /// write part, as well as a given amount of container capacity.
  ///
  /// @param keep_buffer_count The number of existing buffers to keep.
  /// Using a nonzero value for this reduces container allocations
  /// when this object is reused.  If the container has fewer buffers,
  /// the existing buffers will be kept and no more will be allocated.
  /// If the container has more buffers, the excess buffers will be
  /// deallocated.
  ///
  /// @param keep_container_capacity The amount of container capacity
  /// to keep.  Using a small nonzero value for this reduces container
  /// allocations when this object is reused.  This must be at least
  /// the number of kept buffers plus two; otherwise it is modified to
  /// that number.  If the underlying container type is a vector, it
  /// will only shrink if it would reduce the number of elements by
  /// half.
  void reset(Size_t keep_buffer_count = 1,
             // NOLINTNEXTLINE(readability-magic-numbers)
             Size_t keep_container_capacity = 16) {
    BAPI_TRACE;
    // Move all buffers from read part to write part.
    this->set_position(0);

    // Skip over buffers we need to keep, and count them.
    auto it = this->write_part().begin();
    assert(std::distance(this->m_buffers.begin(), it) == 1);
    Size_t kept_buffer_count = 0;
    Size_t kept_size = 0;
    for (; it != this->write_part().end() &&
           kept_buffer_count < keep_buffer_count;
         ++it) {
      ++kept_buffer_count;
      kept_size += it->size();
    }

    // Deallocate buffers we don't need to keep.
    for (; it != this->write_part().end(); ++it) {
      m_char_allocator.deallocate(it->data(), it->size());
      *it = Buffer_view_t();
    }

    // Remove exceess container capacity.
    keep_container_capacity =
        std::max(keep_container_capacity, 2 + kept_buffer_count);
    reset_container(m_buffers, keep_container_capacity);

    // Reset Buffer_sequences
    it = m_buffers.begin();
    this->read_part() = Buffer_sequence_view_t(it, it, 0);
    ++it;
    this->write_part() =
        Buffer_sequence_view_t(it, std::next(it, kept_buffer_count), kept_size);
  }

  /// Return a const reference to the grow calculator.
  const Grow_calculator_t &get_grow_calculator() const {
    return m_grow_calculator;
  }

  /// Set the grow calculator.
  void set_grow_calculator(const Grow_calculator_t &grow_calculator) {
    m_grow_calculator = grow_calculator;
  }

  /// Append the given data.
  ///
  /// This will grow the buffer if needed. Then it writes the data to
  /// the write part, and moves the position so that the written
  /// becomes part of the read part instead of the write part.
  ///
  /// @param data The data to write
  ///
  /// @param size The number of bytes to write.
  ///
  /// @retval success The buffer already had enough capacity, or could
  /// be grown without error. The data has been appended and the
  /// position has been advanced `size` bytes.
  ///
  /// @retval out_of_memory An out of memory condition occurred when
  /// allocating memory for the buffer.  This object is unchanged.
  ///
  /// @retval exceeds_max_size The required size would exceed the
  /// maximum specified by the Grow_calculator.  This object is
  /// unchanged.
  [[NODISCARD]] Grow_status write(const Char_t *data, Size_t size) {
    auto grow_status = this->reserve_write_size(size);
    if (grow_status != Grow_status::success) return grow_status;
    const auto *remaining_data = data;
    auto remaining_size = size;
    auto buffer_it = this->write_part().begin();
    while (remaining_size != 0) {
      auto copy_size = std::min(buffer_it->size(), remaining_size);
      std::memcpy(buffer_it->begin(), remaining_data, copy_size);
      remaining_data += copy_size;
      remaining_size -= copy_size;
      ++buffer_it;
    }
    this->increase_position(size);
    return Grow_status::success;
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
  std::string debug_string([[maybe_unused]] bool show_contents,
                           [[maybe_unused]] int indent) const override {
#ifdef NDEBUG
    return "";
#else
    std::string sep;
    if (indent != 0)
      sep = std::string(",\n") +
            std::string(static_cast<std::string::size_type>(indent * 2), ' ');
    else
      sep = ", ";
    int next_indent = (indent != 0) ? indent + 1 : 0;
    std::ostringstream ss;
    // clang-format off
    ss << "Managed_buffer_sequence(ptr=" << (const void *)this
       << sep << Rw_buffer_sequence_t::debug_string(show_contents, next_indent)
       << sep << m_grow_calculator.debug_string()
       << sep << "buffers.size=" << m_buffers.size()
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
  /// Construct a new object from a given container, where both the
  /// read part and the write part are size zero.
  ///
  /// The container will be moved.  All elements in the container must
  /// be null buffers.
  ///
  /// @param buffers Container of buffers. This must have at least one
  /// element.  All elements must be null buffers.
  ///
  /// @param grow_calculator determines how much memory to allocate
  /// when new memory is needed.
  ///
  /// @param memory_resource The memory_resource used to allocate new
  /// memory, both for the container and for the buffers.
  Managed_buffer_sequence(Container_t buffers,
                          const Grow_calculator_t &grow_calculator,
                          const Memory_resource_t &memory_resource)
      : Rw_buffer_sequence_t(buffers.begin(), std::next(buffers.begin())),
        m_grow_calculator(grow_calculator),
        m_char_allocator(memory_resource),
        m_buffers(std::move(buffers)) {
#ifndef NDEBUG
    assert(m_buffers.size() >= 1);
    for (auto &buffer : m_buffers) {
      assert(buffer.data() == nullptr);
      assert(buffer.size() == 0);
    }
#endif
  }

  /// Allocate and add a new buffer.
  ///
  /// @param size The size of the new buffer that should be allocated.
  ///
  /// @retval true An out of memory condition occurred, and the
  /// function did not produce any side effects.
  ///
  /// @retval false The operation succeeded, and the object now has at
  /// least the requested size.
  [[NODISCARD]] bool allocate_and_add_buffer(Size_t size) {
    // Allocate the data.
    auto data = m_char_allocator.allocate(size);
    if (data == nullptr) {
      BAPI_LOG("info", "error: out of memory allocating " << size << " bytes");
      return true;
    }
    // Add the buffer to the container.
    if (add_buffer(data, size)) {
      BAPI_LOG("info", "error: out of memory growing container of "
                           << m_buffers.size() << " elements");
      m_char_allocator.deallocate(data, size);
      return true;
    }
    return false;
  }

  /// Insert the given buffer in the container, appending it to the
  /// write part.
  ///
  /// @param buffer_data The buffer.
  ///
  /// @param buffer_size The buffer size.
  ///
  /// @retval false Success.
  ///
  /// @retval true An out of memory error occurred when growing the
  /// container.
  [[NODISCARD]] bool add_buffer(Char_t *buffer_data, Size_t buffer_size) {
    BAPI_TRACE;
    auto [write_begin, write_end, write_size] =
        this->get_boundaries(this->write_part());
    if (write_end == m_buffers.end()) {
      // Compute relative positions for all iterators.
      auto [read_begin, read_end, read_size] =
          this->get_boundaries(this->read_part());
      auto read_end_offset = std::distance(read_begin, read_end);
      auto write_begin_offset = std::distance(read_begin, write_begin);
      auto write_end_offset = std::distance(read_begin, write_end);
      // Insert in container, and handle the out-of-memory case.
      try {
        m_buffers.emplace_back(buffer_data, buffer_size);
      } catch (...) {
        BAPI_LOG("info", "error: out of memory growing container");
        return true;
      }
      // Compute new iterators based on relative position from new
      // beginning.
      read_begin = m_buffers.begin();
      read_end = std::next(read_begin, read_end_offset);
      write_begin = std::next(read_begin, write_begin_offset);
      write_end = std::next(read_begin, write_end_offset + 1);
      // Update Buffer_sequence objects with new iterators.
      this->read_part() =
          Buffer_sequence_view_t(read_begin, read_end, read_size);
    } else {
      *write_end = Buffer_view_t(buffer_data, buffer_size);
      ++write_end;
    }
    this->write_part() = Buffer_sequence_view_t(write_begin, write_end,
                                                write_size + buffer_size);
    return false;
  }

  using List_t = typename std::list<Buffer_view_t, Buffer_allocator_t>;
  using List_iterator_t = typename List_t::iterator;
  using Vector_t = typename std::vector<Buffer_view_t, Buffer_allocator_t>;
  using Vector_iterator_t = typename Vector_t::iterator;

  /// `std::vector`-specific function to reset the container.
  ///
  /// This shrinks the vector to keep_container_capacity if it is bigger
  /// than twice keep_container_capacity.
  ///
  /// @param[in,out] container Reference to the vector to be reset.
  ///
  /// @param keep_container_capacity Keep a number of elements in the
  /// vector, to save on future vector resize operations.  If the
  /// number of elements is at least twice this number, the vector
  /// size is reduced to this number.
  ///
  static void reset_container(Vector_t &container,
                              Size_t keep_container_capacity) {
    if (container.capacity() > 2 * keep_container_capacity) {
      container.resize(keep_container_capacity);
      container.shrink_to_fit();
    }
  }

  /// `std::list`-specific function to reset the container.
  ///
  /// This shrinks the list to @c keep_container_capacity if it is
  /// bigger than @c keep_container_capacity.
  ///
  /// @param[in,out] container Reference to the list to be reset.
  ///
  /// @param keep_container_capacity Keep this number of elements in
  /// the list, in order to save future allocations of list nodes.
  ///
  static void reset_container(List_t &container,
                              Size_t keep_container_capacity) {
    if (container.size() > keep_container_capacity)
      container.resize(keep_container_capacity);
  }

 private:
  /// Determines how much memory to allocate when new memory is
  /// needed.
  Grow_calculator_t m_grow_calculator;

  /// Allocator to allocate buffer data (characters).
  Char_allocator_t m_char_allocator;

  /// Container of buffers.
  Container_t m_buffers;

  /// Open the class internals to any class named Accessor<T>, for
  /// some T.
  ///
  /// This may be used by unit tests that need access to internals.
  ///
  /// For example, if unittest SomeTest needs access to member `int m`,
  /// define the helper class:
  /// @code
  /// namespace
  /// mysql::containers::buffers::managed_buffer_sequence::unittest
  /// { template<> class Accessor<SomeTest> {
  ///   static int &m(Managed_buffer_sequence &rbs) { return rbs.m; }
  /// };
  /// }
  /// @endcode
  template <class T>
  friend class managed_buffer_sequence::unittest::Accessor;
};

}  // namespace mysql::containers::buffers

/// @}

#endif  // MYSQL_CONTAINERS_BUFFERS_MANAGED_BUFFER_SEQUENCE_H
