/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#ifndef MEMORY_ALIGNED_ATOMIC_H
#define MEMORY_ALIGNED_ATOMIC_H

#include <assert.h>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <stdlib.h>
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace memory {

/**
 Calculates and returns the size of the CPU cache line.

 @return the cache line size
 */
#if defined(__APPLE__)
static inline size_t _cache_line_size() {
  size_t line_size{0};
  size_t sizeof_line_size = sizeof(line_size);
  sysctlbyname("hw.cachelinesize", &line_size, &sizeof_line_size, 0, 0);
  return line_size;
}

#elif defined(_WIN32)
static inline size_t _cache_line_size() {
  size_t line_size{0};
  DWORD buffer_size = 0;
  DWORD i = 0;
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buffer = 0;

  GetLogicalProcessorInformation(0, &buffer_size);
  buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(buffer_size);
  GetLogicalProcessorInformation(&buffer[0], &buffer_size);

  for (i = 0; i != buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
       ++i) {
    if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1) {
      line_size = buffer[i].Cache.LineSize;
      break;
    }
  }

  free(buffer);
  return line_size;
}

#elif defined(__linux__)
static inline size_t _cache_line_size() {
  long size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
  if (size == -1) return 64;
#if defined(__s390x__)
  // returns 0 on s390x RHEL 7.x
  if (size == 0) {
    FILE *p = fopen(
        "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    if (p) {
      fscanf(p, "%ld", &size);
      fclose(p);
    }
  }
#endif
  return static_cast<size_t>(size);
}

#else
static inline size_t _cache_line_size() { return 64; }
#endif
static inline size_t cache_line_size() {
  static const size_t size{memory::_cache_line_size()};
  return size;
}

/**
 Retrieves the amount of bytes, multiple of the current cacheline size, needed
 to store an element of type `T`. This is a non-caching non-thread safe helper
 function and `memory::minimum_cacheline_for` should be used instead.

 @return the amount of bytes, multiple of the current cacheline size, needed to
 store an element of type `T`.
 */
template <typename T>
static inline size_t _cacheline_for() {
  size_t csize = memory::cache_line_size();
  size_t size{static_cast<size_t>(std::ceil(static_cast<double>(sizeof(T)) /
                                            static_cast<double>(csize))) *
              csize};
  return size;
}

/**
 Retrieves the amount of bytes, multiple of the current cacheline size, needed
 to store an element of type `T`. This function caches the computed value in a
 static storage variable and does it in a thread-safe manner.

 @return the amount of bytes, multiple of the current cacheline size, needed to
 store an element of type `T`.
 */
template <typename T>
static inline size_t minimum_cacheline_for() {
  static const size_t size{memory::_cacheline_for<T>()};
  return size;
}

/**
  @class memory::Aligned_atomic

  Templated class that encapsulates an `std::atomic` within a byte buffer that
  is padded to the processor cache-line size.

  This class purpose is to help prevent false sharing between atomically
  accessed variables that are contiguous in memory. This is the normal case for
  arrays or class members declared next to each other.

  If the intended usage is none of the above, `std::atomic` class should be used
  since the below implementation allocates more memory than needed for storing
  the intended value (in order to implement the padding to the cache-line).
 */
template <typename T>
class Aligned_atomic {
 public:
  /*
    Default class constructor, will allocate a byte buffer with enough space to
    store an instance of `std::atomic<T>` and paded to a multiple of the
    processor cache-line size.

    Will invoke `std::atomic<T>` inplace constructor using the allocated byte
    array.
   */
  Aligned_atomic();
  /*
    Constructor that will assign the parameter value to the newly allocated
    `std::atomic<T>`.

    @param value The value to store in the underlying `std::atomic<T>` object.
   */
  Aligned_atomic(T value);
  /*
    Deleted copy constructor, no copies allowed.
   */
  Aligned_atomic(Aligned_atomic<T> const &rhs) = delete;
  /*
    Move semantics constructor.

    @param rhs The object to collect the underlying `std::atomic<T>` and byte
               buffer from.
   */
  Aligned_atomic(Aligned_atomic<T> &&rhs);
  /*
    Destructor that will invoke `std::atomi<T>` inplace destructor and release
    the allocated byte buffer.
   */
  virtual ~Aligned_atomic();

  /*
    Deleted copy operator.
   */
  Aligned_atomic<T> &operator=(Aligned_atomic<T> const &rhs) = delete;
  /*
    Move semantics operator.

    @param rhs The object to collect the underlying `std::atomic<T>` and byte
               buffer from.

    @return This object reference, instantiated with the collected values.
   */
  Aligned_atomic<T> &operator=(Aligned_atomic<T> &&rhs);
  /*
    Assignment operator for parameter of templated type.

    @param rhs The templated type value to be stored in the underlying
               `std::atomic`.

    @return This object reference, instantiated with the collected value.
   */
  Aligned_atomic<T> &operator=(T rhs);
  /**
    Casting operator for directly accessing the value of the underlying
    `std::atomic<T>`.

    @return The value of type `T` stored in the underlying `std::atomic`..
   */
  operator T() const;
  /**
    Equality operator to determine if the underlying storage memory is
    initialized.

    @param rhs nullptr value

    @return true if the underlying storage memory is not initialized,
            false otherwise.
   */
  bool operator==(std::nullptr_t rhs) const;
  /**
    Inequality operator to determine if the underlying storage memory is
    initialized.

    @param rhs nullptr value

    @return true if the underlying storage memory is initialized, false
            otherwise.
   */
  bool operator!=(std::nullptr_t rhs) const;
  /**
    Equality operator for determining if the value stored in the underlying
    `std::atomic` equals the passed parameter.

    @param rhs The value to compare with.

    @return true if the parameter value equals the value stored in the
            underlying `std::atomic`, false otherwise.
   */
  bool operator==(T rhs) const;
  /**
    Inequality operator for determining if the value stored in the underlying
    `std::atomic` differs from the passed parameter.

    @param rhs The value to compare with.

    @return true if the parameter value differs from the value stored in the
            underlying `std::atomic`, false otherwise.
   */
  bool operator!=(T rhs) const;
  /*
    Pointer operator that allows the access to the underlying `std::atomic<T>`
    object.

    @return The pointer to the underlying `std::atomic<T>` object.
   */
  std::atomic<T> *operator->() const;
  /*
    Dereference operator that allows the access to the underlying
    `std::atomic<T>` object.

    @return The reference to the underlying `std::atomic<T>` object.
   */
  std::atomic<T> &operator*() const;
  /*
    The size of `std::atomic<T>`, as returned by `sizeof std::atomic<T>`.

    @return The in-memory size of an `std::atomic<T>` instance.
   */
  size_t size() const;
  /*
    The size of the allocated byte buffer.

    @return The in-memory size of the allocated byte buffer.
   */
  size_t allocated_size() const;

 private:
  /** The size of the byte buffer. */
  size_t m_storage_size{0};
  /** The byte buffer to use as underlying storage. */
  alignas(std::max_align_t) unsigned char *m_storage{nullptr};
  /** The pointer to the underlying `std::atomic<T>` object. */
  std::atomic<T> *m_underlying{nullptr};
};
}  // namespace memory

template <typename T>
memory::Aligned_atomic<T>::Aligned_atomic()
    : m_storage_size{memory::minimum_cacheline_for<std::atomic<T>>()},
      m_storage{new unsigned char[m_storage_size]},
      m_underlying{new (this->m_storage) std::atomic<T>()} {}

template <typename T>
memory::Aligned_atomic<T>::Aligned_atomic(T value)
    : memory::Aligned_atomic<T>() {
  this->m_underlying->store(value);
}

template <typename T>
memory::Aligned_atomic<T>::Aligned_atomic(Aligned_atomic<T> &&rhs)
    : m_storage_size{rhs.m_storage_size}, m_underlying{rhs.m_underlying} {
  delete[] this->m_storage;
  this->m_storage = rhs.m_storage;
  rhs.m_storage_size = 0;
  rhs.m_storage = nullptr;
  rhs.m_underlying = nullptr;
}

template <typename T>
memory::Aligned_atomic<T>::~Aligned_atomic() {
  if (this->m_underlying != nullptr) {
    this->m_underlying->~atomic();
    this->m_underlying = nullptr;
  }
  delete[] this->m_storage;
  this->m_storage = nullptr;
  this->m_storage_size = 0;
}

template <typename T>
memory::Aligned_atomic<T> &memory::Aligned_atomic<T>::operator=(
    Aligned_atomic<T> &&rhs) {
  delete[] this->m_storage;
  this->m_storage_size = rhs.m_storage_size;
  this->m_storage = rhs.m_storage;
  this->m_underlying = rhs.m_underlying;
  rhs.m_storage_size = 0;
  rhs.m_storage = nullptr;
  rhs.m_underlying = nullptr;
  return (*this);
}

template <typename T>
memory::Aligned_atomic<T> &memory::Aligned_atomic<T>::operator=(T rhs) {
  assert(this->m_underlying != nullptr);
  this->m_underlying->store(rhs, std::memory_order_seq_cst);
  return (*this);
}

template <typename T>
memory::Aligned_atomic<T>::operator T() const {
  assert(this->m_underlying != nullptr);
  return this->m_underlying->load(std::memory_order_relaxed);
}

template <typename T>
bool memory::Aligned_atomic<T>::operator==(std::nullptr_t) const {
  return this->m_underlying == nullptr;
}

template <typename T>
bool memory::Aligned_atomic<T>::operator!=(std::nullptr_t) const {
  return this->m_underlying != nullptr;
}

template <typename T>
bool memory::Aligned_atomic<T>::operator==(T rhs) const {
  if (this->m_underlying == nullptr) return false;
  return this->m_underlying->load(std::memory_order_relaxed) == rhs;
}

template <typename T>
bool memory::Aligned_atomic<T>::operator!=(T rhs) const {
  return !((*this) == rhs);
}

template <typename T>
std::atomic<T> *memory::Aligned_atomic<T>::operator->() const {
  assert(this->m_underlying != nullptr);
  return this->m_underlying;
}

template <typename T>
std::atomic<T> &memory::Aligned_atomic<T>::operator*() const {
  assert(this->m_underlying != nullptr);
  return *this->m_underlying;
}

template <typename T>
size_t memory::Aligned_atomic<T>::size() const {
  return sizeof(std::atomic<T>);
}

template <typename T>
size_t memory::Aligned_atomic<T>::allocated_size() const {
  return this->m_storage_size;
}

#endif  // MEMORY_ALIGNED_ATOMIC_H
