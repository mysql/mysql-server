/* Copyright (c) 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/// @addtogroup Replication
/// @{
///
/// @file managed_buffer.h
///
/// Container class that provides a contiguous memory buffer to
/// the caller, which the caller can request to grow.
///
/// The growth rate is determined by a Grow_calculator.

#ifndef MYSQL_BUFFER_MANAGED_BUFFER_H_
#define MYSQL_BUFFER_MANAGED_BUFFER_H_

#include <limits>

#include "grow_calculator.h"                    // buffer::Grow_calculator
#include "grow_status.h"                        // buffer::Grow_status
#include "libbinlogevents/include/nodiscard.h"  // NODISCARD
#include "libbinlogevents/include/resource/allocator.h"        // Allocator
#include "libbinlogevents/include/resource/memory_resource.h"  // Memory_resource
#include "rw_buffer.h"  // buffer::Rw_buffer

#include "libbinlogevents/include/wrapper_functions.h"  // BAPI_TRACE

namespace mysqlns::buffer {

/// Owned, growable, contiguous memory buffer.
///
/// This class provides a single contiguous buffer.  Therefore, it may
/// have to move data when it grows.  It is implemented as a Buffer
/// that is resized using realloc.
///
/// The caller can provide a user-defined, pre-allocated buffer, which
/// will then be used as long as it suffices; a new buffer will be
/// allocated if not.  This can be used to remove the need for
/// allocation in use cases where the object is small.
///
/// Objects have a growable total capacity, which is split into two
/// parts; the read part and the write part, each represented as a
/// `Buffer_view`.  The intended use case is where the user interacts
/// with an API that produces data into a user-provided buffer.  The
/// user can then: (1) grow the buffer before invoking the API; (2)
/// invoke the API to write data to the write part; (3) tell the
/// Managed_buffer to move the written bytes to the read part.
///
/// Generally, std::stringstream or std::vector<char> are safer and
/// simpler interfaces for buffers and should be preferred when
/// possible.  However they do not fit all use cases:
///
/// - std::stringstream is preferrable and more convenient when
///   appending existing data to the stream.  But it is not suitable
///   for interaction with C-like APIs that produce data in a char*
///   given by the caller.  The user would need to allocate a buffer
///   outside the stringsteam and then append the buffer to the
///   stringstream, which would imply unnecessary memory and cpu
///   overheads.
///
/// - When using a C-like API that produces data in a char* given by
///   the caller, std::vector is often good.  The user can reserve as
///   much memory as needed and then pass the underlying data array to
///   the API.  However, the following properties are sometimes
///   advantageous for Managed_buffer:
///
///   - Vector has no practical way to put an exact bound on the
///     memory usage.  Managed_buffer uses a Grow_calculator which
///     allows exact control on memory usage, including a maximum
///     capacity.
///
///   - Even for small buffer sizes, vector needs at least one heap
///     allocation.  Managed_buffer allows the use of a default buffer
///     of fixed size, for example allocated on the stack.  This can
///     reduce the need for heap allocation in use patterns where the
///     required buffer capacity is *usually* small.
///
/// The main drawback of Managed_buffer is that it is non-standard and
/// has a minimal feature set.
///
/// The main difference between Buffer_sequence and Managed_buffer, is
/// that Managed_buffer keeps data in a contiguous buffer, whereas
/// Buffer_sequence never copies data.
///
/// This class never throws any exception.
///
/// @tparam Char_t The char type; usually char or unsigned char.
///
/// @tparam builtin_capacity Size of pre-allocated initial buffer.
template <class Char_tp = unsigned char>
class Managed_buffer : public buffer::Rw_buffer<Char_tp> {
 public:
  using Char_t = Char_tp;
  using Buffer_view_t = Buffer_view<Char_t>;
  using Rw_buffer_t = Rw_buffer<Char_t>;
  using typename Rw_buffer_t::Const_iterator_t;
  using typename Rw_buffer_t::Iterator_t;
  using typename Rw_buffer_t::Size_t;
  // As soon as all platforms support it, change to:
  // using Allocator_t = std::pmr::polymorphic_allocator<Char_t>;
  using Memory_resource_t = mysqlns::resource::Memory_resource;
  using Char_allocator_t = mysqlns::resource::Allocator<Char_t>;
  using Grow_calculator_t = Grow_calculator;

  /// Construct a new object without a default buffer.
  // Nolint: clang-tidy does not recognize that m_owns_default_buffer
  // is initialized, despite it is initialized in the targed
  // constructor.
  // NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
  explicit Managed_buffer(
      const Memory_resource_t &memory_resource = Memory_resource_t())
      : Managed_buffer(Size_t(0), memory_resource) {}
  // NOLINTEND(cppcoreguidelines-pro-type-member-init)

  /// Construct a new object that owns a default buffer.
  ///
  /// The default buffer is created when needed.  Once
  /// created, it survives calls to @c reset and will only be deleted
  /// when the Managed_buffer is deleted.
  ///
  /// @param default_capacity The capacity of the default buffer.
  ///
  /// @param memory_resource Memory_resource used to allocate memory.
  explicit Managed_buffer(
      Size_t default_capacity,
      const Memory_resource_t &memory_resource = Memory_resource_t())
      : Rw_buffer<Char_t>(),
        m_char_allocator(memory_resource),
        m_default_buffer(nullptr),
        m_default_capacity(default_capacity),
        m_owns_default_buffer(true) {}

  /// Construct a new object that uses the given default buffer.
  ///
  /// The default buffer is owned by the caller, so the caller must
  /// ensure that it outlives the Managed_buffer.
  ///
  /// @param default_buffer The default buffer.
  ///
  /// @param memory_resource Memory_resource used to allocate memory.
  explicit Managed_buffer(
      Buffer_view_t default_buffer,
      const Memory_resource_t &memory_resource = Memory_resource_t())
      : Rw_buffer<Char_t>(default_buffer),
        m_char_allocator(memory_resource),
        m_default_buffer(default_buffer.begin()),
        m_default_capacity(default_buffer.size()),
        m_owns_default_buffer(false) {}

  Managed_buffer(Managed_buffer &other) = delete;
  Managed_buffer(Managed_buffer &&other) noexcept = default;
  Managed_buffer &operator=(Managed_buffer &other) = delete;
  Managed_buffer &operator=(Managed_buffer &&other) noexcept = default;

  ~Managed_buffer() override {
    auto *ptr = this->read_part().begin();
    bool delete_default_buffer =
        m_default_buffer != nullptr && m_owns_default_buffer;
    bool delete_buffer = ptr != nullptr && ptr != m_default_buffer;
    if (delete_default_buffer)
      m_char_allocator.deallocate(m_default_buffer, m_default_capacity);
    if (delete_buffer) m_char_allocator.deallocate(ptr, this->capacity());
  }

  /// Reserve space so that the total buffer size is at least the
  /// given number.
  ///
  /// The buffer will be resized if necessary.  So, on successful
  /// return, the caller should call begin() to get the new buffer
  /// pointer.
  ///
  /// @note This may move existing data to a new address; consider any
  /// existing pointers into the buffer as invalid after this call.
  ///
  /// @param requested_size The requested total size of the read part
  /// and the write part.
  ///
  /// @retval Grow_status::success The object now has at least the
  /// given size; either it was successfully re-allocated, or it
  /// already had the requested size.
  ///
  /// @retval Grow_status::exceeds_max_size if requested_size
  /// exceeds the configured maximum size.
  ///
  /// @retval Grow_status::out_of_memory Memory allocation failed.
  [[NODISCARD]] Grow_status reserve_total_size(Size_t requested_size) {
    BAPI_TRACE;
    auto capacity = this->capacity();
    auto [error, new_capacity] =
        m_grow_calculator.compute_new_size(capacity, requested_size);
    if (error) return Grow_status::exceeds_max_size;
    if (new_capacity > capacity) {
      if (new_capacity <= m_default_capacity) {
        // We have capacity < new_capacity <= m_default_capacity.
        // Since we never allocate capacity less than the default
        // capacity, this situation only occurs when the capacity is
        // 0.  And since we make use of the default buffer as soon as
        // we allocate it, it also means that the default buffer is
        // nullptr.
        assert(capacity == 0);
        assert(m_default_buffer == nullptr);
        m_default_buffer = allocate_buffer(m_default_capacity);
        if (m_default_buffer == nullptr) return Grow_status::out_of_memory;
        replace_buffer(m_default_buffer, m_default_capacity);
      } else {
        // Use dynamic buffer.
        Char_t *new_buffer = allocate_buffer(new_capacity);
        if (new_buffer == nullptr) return Grow_status::out_of_memory;
        replace_buffer(new_buffer, new_capacity);
      }
    }
    return Grow_status::success;
  }

  /// Reserve space so that the write size is at least the given
  /// number.
  ///
  /// @param requested_write_size The requested size of the write
  /// part.
  ///
  /// @retval Grow_status::success The write part now has at least the
  /// given size; either it was successfully re-allocated, or it
  /// already had the requested size.
  ///
  /// @retval Grow_status::exceeds_max_size if the existing read size
  /// plus requested_write_size exceeds the max size configured in the
  /// Grow_calculator.
  ///
  /// @retval Grow_status::out_of_memory Memory allocation failed.
  [[NODISCARD]] Grow_status reserve_write_size(Size_t requested_write_size) {
    auto read_size = this->read_part().size();
    if (requested_write_size > std::numeric_limits<Size_t>::max() - read_size)
      return Grow_status::exceeds_max_size;
    return reserve_total_size(read_size + requested_write_size);
  }

  /// Reset the buffer.
  ///
  /// This makes the read part empty.  The write part will point to
  /// the default buffer if there is one; otherwise the write part
  /// will be empty.
  void reset() {
    BAPI_TRACE;
    auto *rb = this->read_part().begin();
    if (rb != nullptr && rb != m_default_buffer)
      m_char_allocator.deallocate(this->read_part().begin(), this->capacity());
    this->read_part() = Buffer_view_t(m_default_buffer, 0);
    if (m_default_buffer == nullptr)
      this->write_part() = Buffer_view_t();
    else
      this->write_part() = Buffer_view_t(m_default_buffer, m_default_capacity);
  }

  /// Set the grow calculator.
  ///
  /// Details:
  ///
  /// - If the new Grow_calculator's maximum size is less than the
  /// current buffer size, it does not change the existing buffer, but
  /// subsequent calls to reserve will fail.
  ///
  /// - In case the new Grow_calculator's maximum size is less than
  /// the default capacity, this object will provide capacity equal to
  /// the default_size, exceeding the Grow_calculator's maximum size.
  void set_grow_calculator(const Grow_calculator_t &grow_calculator) {
    m_grow_calculator = grow_calculator;
  }

  /// Return a const reference to the grow calculator.
  const Grow_calculator_t &get_grow_calculator() const {
    return m_grow_calculator;
  }

  /// Return the size of the default buffer.
  Size_t get_default_capacity() { return m_default_capacity; }

 private:
  /// Allocate a new buffer and return it.
  ///
  /// This never throws; it returns nullptr on out of memory.
  ///
  /// @param new_size The size of the buffer.
  ///
  /// @returns the new buffer on success, nullptr on out of memory.
  [[NODISCARD]] Char_t *allocate_buffer(Size_t new_size) {
    try {
      return m_char_allocator.allocate(new_size);
    } catch (std::bad_alloc &) {
      return nullptr;
    }
  }

  /// Replace the underlying data buffer by the given one.
  ///
  /// @param new_buffer The new buffer.  This must be different from
  /// the old buffer.
  ///
  /// @param new_size The size of the new buffer.
  void replace_buffer(Char_t *new_buffer, Size_t new_size) {
    assert(new_buffer != this->read_part().data());
    auto &r = this->read_part();
    auto &w = this->write_part();
    auto read_size = r.size();
    if (read_size) memcpy(new_buffer, r.begin(), read_size);
    if (r.begin() != m_default_buffer && r.begin() != nullptr)
      m_char_allocator.deallocate(r.begin(), this->capacity());
    r = Buffer_view_t(new_buffer, read_size);
    w = Buffer_view_t(new_buffer + read_size, new_size - read_size);
  }

  /// Calculator for growing the buffer.
  Grow_calculator_t m_grow_calculator;

  /// Allocator to grow the buffer.
  Char_allocator_t m_char_allocator;

  /// User-provided, user-owned buffer.
  Char_t *m_default_buffer;

  /// Size of user-provided, user-owned buffer.
  Size_t m_default_capacity;

  /// If true, the default buffer will be deallocated by the destructor.
  bool m_owns_default_buffer;
};

constexpr std::size_t default_preallocated_managed_buffer_size =
    std::size_t(8 * 1024);

/// @see Managed_buffer
///
/// This class pre-allocates a fixed-size initial buffer,
/// which is beneficial in use patterns where managed buffers are
/// allocated on the stack and are usually only given small amount of
/// data.
template <class Char_t = unsigned char,
          std::size_t preallocated_size =
              default_preallocated_managed_buffer_size>
class Preallocated_managed_buffer : public Managed_buffer<Char_t> {
 public:
  using typename Managed_buffer<Char_t>::Buffer_view_t;
  using typename Managed_buffer<Char_t>::Grow_calculator_t;
  using typename Managed_buffer<Char_t>::Memory_resource_t;

  explicit Preallocated_managed_buffer(
      const Memory_resource_t &memory_resource = Memory_resource_t())
      : Managed_buffer<Char_t>(
            Buffer_view_t(m_preallocated_buffer, preallocated_size),
            memory_resource) {}

 private:
  /// Preallocated buffer.
  Char_t m_preallocated_buffer[preallocated_size == 0 ? 1 : preallocated_size];
};

}  // namespace mysqlns::buffer

/// @} (end of group Replication)
#endif  // MYSQL_BUFFER_MANAGED_BUFFER_H_
