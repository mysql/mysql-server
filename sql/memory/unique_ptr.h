/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef MEMORY_UNIQUE_PTR_INCLUDED
#define MEMORY_UNIQUE_PTR_INCLUDED

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>

#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"  // my_malloc
#include "sql/memory/aligned_atomic.h"  // memory::cache_line_size
#include "sql/memory/ref_ptr.h"         // memory::Ref_ptr

namespace memory {
namespace traits {
/**
 Tests for the existence of `allocate(size_t)` in order to disambiguate if `T`
 is an allocator class.
 */
template <class T>
auto test_for_allocate(int T::*)
    -> decltype(std::declval<T>().allocate(std::declval<size_t>()),
                std::true_type{});
template <class>
std::false_type test_for_allocate(...);

}  // namespace traits
/**
 Struct that allows for checking if `T` fulfills the Allocator named
 requirements.
 */
template <class T>
struct is_allocator : decltype(traits::test_for_allocate<T>(nullptr)) {};

/**
 Allocator class for instrumenting allocated memory with Performance Schema
 keys.
 */
template <typename T>
class PFS_allocator {
  using value_type = T;
  using size_type = size_t;

  /**
    Constructor for the class that takes the PFS key to be used.

    @param key The PFS key to be used.
   */
  PFS_allocator(PSI_memory_key key);
  /**
    Copy constructor.

    @param rhs The object to copy from.
   */
  template <typename U>
  PFS_allocator(PFS_allocator<U> const &rhs) noexcept;
  /**
    Move constructor.

    @param rhs The object to move from.
   */
  template <typename U>
  PFS_allocator(PFS_allocator<U> &&rhs) noexcept;
  /**
    Retrieves the PFS for `this` allocator object.

    @return The PFS key.
   */
  PSI_memory_key key() const;
  /**
    Allocate `n` bytes and return a pointer to the beginning of the allocated
    memory.

    @param n The size of the memory to allocate.

    @return A pointer to the beginning of the allocated memory.
   */
  T *allocate(std::size_t n);
  /**
    Deallocates the `n` bytes stored in the memory pointer `p` is pointing to.

    @param p The pointer to the beginning of the memory to deallocate.
    @param n The size of the memory to deallocate.
   */
  void deallocate(T *p, std::size_t n) noexcept;
  /**
    In-place constructs an object of class `U` in the memory pointed by `p`.

    @param p The pointer to the beginning of the memory to construct the object
             in.
    @param args The parameters to be used with the `U` constructor.
   */
  template <class U, class... Args>
  void construct(U *p, Args &&... args);
  /**
    In-place invokes the destructor for class `T` on object pointed by `p`.

    @param p The object pointer to invoke the destructor on.
   */
  void destroy(T *p);
  /**
    The maximum size available to allocate.

    @return The maximum size available to allocate.
   */
  size_type max_size() const;

 private:
  /** The PFS key to be used to allocate memory */
  PSI_memory_key m_key;
};

/**
  Smart pointer to hold a unique pointer to a heap allocated memory of type `T`,
  constructed using a specific allocator.

  Template parameters are as follows:
  - `T` is the type of the pointer to allocate. It may be an array type.
  - `A` the allocator to use. If none is passed, `std::nullptr_t` is passed and
    regular `new` and `delete` are used to construct the memory.
 */
template <typename T, typename A = std::nullptr_t>
class Unique_ptr {
 public:
  using type = typename std::remove_extent<T>::type;
  using pointer = type *;
  using reference = type &;

  /**
    Default class constructor, only to be used with no specific allocator.
   */
  template <
      typename D = T, typename B = A,
      std::enable_if_t<std::is_same<B, std::nullptr_t>::value> * = nullptr>
  Unique_ptr();
  /**
    Class constructor, to be used with specific allocators, passing the
    allocator object to be used.

    @param alloc The allocator instance to be used.
   */
  template <
      typename D = T, typename B = A,
      std::enable_if_t<!std::is_same<B, std::nullptr_t>::value> * = nullptr>
  Unique_ptr(A &alloc);
  /**
    Class constructor, to be used with specific allocators and when `T` is an
    array type, passing the allocator object to be used and the size of the
    array.

    @param alloc The allocator instance to be used.
    @param size The size of the array to allocate.
   */
  template <typename D = T, typename B = A,
            std::enable_if_t<!std::is_same<B, std::nullptr_t>::value &&
                             std::is_array<D>::value> * = nullptr>
  Unique_ptr(A &alloc, size_t size);
  /**
    Class constructor, to be used with no specific allocators and when `T` is an
    array type, passing the allocator object to be used and the size of the
    array.

    @param size The size of the array to allocate.
   */
  template <typename D = T, typename B = A,
            std::enable_if_t<std::is_same<B, std::nullptr_t>::value &&
                             std::is_array<D>::value> * = nullptr>
  Unique_ptr(size_t size);
  /**
    Class constructor, to be used with specific allocators and when `T` is not
    an array type, passing the allocator object to be used and the parameters to
    be used with `T` object constructor.

    @param alloc The allocator instance to be used.
    @param args The parameters to be used with `T` object constructor.
   */
  template <typename... Args, typename D = T, typename B = A,
            std::enable_if_t<!std::is_same<B, std::nullptr_t>::value &&
                             !std::is_array<D>::value> * = nullptr>
  Unique_ptr(A &alloc, Args &&... args);
  /**
    Class constructor, to be used with no specific allocators and when `T` is
    not an array type, passing the parameters to be used with `T` object
    constructor.

    @param args The parameters to be used with `T` object constructor.
   */
  template <typename... Args, typename D = T, typename B = A,
            std::enable_if_t<std::is_same<B, std::nullptr_t>::value &&
                             !std::is_array<D>::value> * = nullptr>
  Unique_ptr(Args &&... args);
  // Deleted copy constructor
  Unique_ptr(Unique_ptr<T, A> const &rhs) = delete;
  /**
    Move constructor.

    @param rhs The object to move data from.
   */
  Unique_ptr(Unique_ptr<T, A> &&rhs);
  /**
    Destructor for the class.
   */
  virtual ~Unique_ptr();
  // Deleted copy operator
  Unique_ptr<T, A> &operator=(Unique_ptr<T, A> const &rhs) = delete;
  /**
    Move operator.

    @param rhs The object to move data from.
   */
  Unique_ptr<T, A> &operator=(Unique_ptr<T, A> &&rhs);
  /**
    Arrow operator to access the underlying object of type `T`.

    @return A pointer to the underlying object of type `T`.
   */
  template <typename D = T,
            std::enable_if_t<!std::is_array<D>::value> * = nullptr>
  pointer operator->() const;
  /**
    Star operator to access the underlying object of type `T`.

    @return A reference to the underlying object of type `T`.
   */
  reference operator*() const;
  /**
    Subscript operator, to access an array element when `T` is of array type.

    @param index The index of the element to retrieve the value for.

    @return A reference to the value stored at index.
   */
  template <typename D = T,
            std::enable_if_t<std::is_array<D>::value> * = nullptr>
  reference operator[](size_t index) const;
  /**
    Casting operator to bool.

    @return `true` if the underlying pointer is instantiated, `false` otherwise.
   */
  operator bool() const;
  /**
    Releases the ownership of the underlying allocated memory and returns a
    pointer to the beginning of that memory. This smart pointer will no longer
    manage the underlying memory.

    @return the pointer to the allocated and no longer managed memory.
   */
  template <
      typename B = A,
      std::enable_if_t<std::is_same<B, std::nullptr_t>::value> * = nullptr>
  pointer release();
  /**
    Releases the ownership of the underlying allocated memory and returns a
    pointer to the beginning of that memory. This smart pointer will no longer
    manage the underlying memory.

    @return the pointer to the allocated and no longer managed memory.
   */
  template <
      typename B = A,
      std::enable_if_t<!std::is_same<B, std::nullptr_t>::value> * = nullptr>
  pointer release();
  /**
    Returns a pointer to the underlying allocated memory.

    @return A pointer to the underlying allocated memory
   */
  pointer get() const;
  /**
    The size of the memory allocated, in bytes.

    @return The size of the memory allocated, in bytes
   */
  size_t size() const;
  /**
    Will resize the allocated memory to `new_size`. If the configure allocator
    supports this operation, the allocator is used. If not, a new memory chunk
    is allocated and the memory is copied.

    @param new_size The new desired size for the memory.

    @return The reference to `this` object, for chaining purposed.
   */
  template <
      typename D = T, typename B = A,
      std::enable_if_t<std::is_array<D>::value &&
                       std::is_same<B, std::nullptr_t>::value> * = nullptr>
  Unique_ptr<T, A> &reserve(size_t new_size);
  /**
    Will resize the allocated memory to `new_size`. If the configure allocator
    supports this operation, the allocator is used. If not, a new memory chunk
    is allocated and the memory is copied.

    @param new_size The new desired size for the memory.

    @return The reference to `this` object, for chaining purposed.
   */
  template <
      typename D = T, typename B = A,
      std::enable_if_t<std::is_array<D>::value &&
                       !std::is_same<B, std::nullptr_t>::value> * = nullptr>
  Unique_ptr<T, A> &reserve(size_t new_size);
  /**
    Returns the used allocator instance, if any.

    @return The reference to the allocator object.
   */
  A &allocator() const;

 private:
  /** The pointer to the underlying allocated memory  */
  alignas(std::max_align_t) pointer m_underlying{nullptr};
  /** The allocator to be used to allocate memory */
  memory::Ref_ptr<A> m_allocator;
  /** The size of the allocated memory */
  size_t m_size{0};

  /**
    Clears the underlying pointer and size.
   */
  void reset();
  /**
    Deallocates the underlying allocated memory.
   */
  template <typename D = T, typename B = A,
            std::enable_if_t<std::is_same<B, std::nullptr_t>::value &&
                             std::is_array<D>::value> * = nullptr>
  void destroy();
  /**
    Deallocates the underlying allocated memory.
   */
  template <typename D = T, typename B = A,
            std::enable_if_t<std::is_same<B, std::nullptr_t>::value &&
                             !std::is_array<D>::value> * = nullptr>
  void destroy();
  /**
    Deallocates the underlying allocated memory.
   */
  template <typename D = T, typename B = A,
            std::enable_if_t<!std::is_same<B, std::nullptr_t>::value &&
                             std::is_array<D>::value> * = nullptr>
  void destroy();
  /**
    Deallocates the underlying allocated memory.
   */
  template <typename D = T, typename B = A,
            std::enable_if_t<!std::is_same<B, std::nullptr_t>::value &&
                             !std::is_array<D>::value> * = nullptr>
  void destroy();
  /**
    Clones the underlying memory and returns a pointer to the clone memory.

    @return A pointer to the cloned underlying memory.
   */
  template <typename D = T,
            std::enable_if_t<std::is_array<D>::value> * = nullptr>
  pointer clone() const;
  /**
    Clones the underlying memory and returns a pointer to the clone memory.

    @return A pointer to the cloned underlying memory.
   */
  template <typename D = T,
            std::enable_if_t<!std::is_array<D>::value> * = nullptr>
  pointer clone() const;
};

/**
  In-place constructs a new unique pointer with no specific allocator and with
  array type `T`.

  @param size The size of the array to allocate.

  @return A new instance of unique pointer.
 */
template <typename T, std::enable_if_t<std::is_array<T>::value> * = nullptr>
Unique_ptr<T, std::nullptr_t> make_unique(size_t size);
/**
  In-place constructs a new unique pointer with a specific allocator and with
  array type `T`.

  @param alloc A reference to the allocator object to use.
  @param size The size of the array to allocate.

  @return A new instance of unique pointer.
 */
template <typename T, typename A,
          std::enable_if_t<std::is_array<T>::value> * = nullptr>
Unique_ptr<T, A> make_unique(A &alloc, size_t size);
/**
  In-place constructs a new unique pointer with a specific allocator and with
  non-array type `T`.

  @param alloc A reference to the allocator object to use.
  @param args The parameters to be used in constructing the instance of `T`.

  @return A new instance of unique pointer.
 */
template <typename T, typename A, typename... Args,
          std::enable_if_t<!std::is_array<T>::value &&
                           memory::is_allocator<A>::value> * = nullptr>
Unique_ptr<T, A> make_unique(A &alloc, Args &&... args);
/**
  In-place constructs a new unique pointer with no specific allocator and with
  non-array type `T`.

  @param args The parameters to be used in constructing the instance of `T`.

  @return A new instance of unique pointer.
 */
template <typename T, typename... Args,
          std::enable_if_t<!std::is_array<T>::value> * = nullptr>
Unique_ptr<T, std::nullptr_t> make_unique(Args &&... args);
}  // namespace memory

// global scope
template <typename T, typename U>
bool operator==(const memory::PFS_allocator<T> &lhs,
                const memory::PFS_allocator<U> &rhs) {
  return lhs.key() == rhs.key();
}

template <typename T, typename U>
bool operator!=(const memory::PFS_allocator<T> &lhs,
                const memory::PFS_allocator<U> &rhs) {
  return lhs.key() != rhs.key();
}

template <typename T1, typename A1, typename T2, typename A2>
bool operator==(memory::Unique_ptr<T1, A1> const &lhs,
                memory::Unique_ptr<T2, A2> const &rhs) {
  return static_cast<const void *>(lhs.get()) ==
         static_cast<const void *>(rhs.get());
}

template <typename T1, typename A1, typename T2, typename A2>
bool operator!=(memory::Unique_ptr<T1, A1> const &lhs,
                memory::Unique_ptr<T2, A2> const &rhs) {
  return !(lhs == rhs);
}

template <typename T1, typename A1>
bool operator==(memory::Unique_ptr<T1, A1> const &lhs, std::nullptr_t) {
  return lhs.get() == nullptr;
}

template <typename T1, typename A1>
bool operator!=(memory::Unique_ptr<T1, A1> const &lhs, std::nullptr_t) {
  return !(lhs == nullptr);
}
// global scope

template <typename T>
memory::PFS_allocator<T>::PFS_allocator(PSI_memory_key key) : m_key{key} {}

template <typename T>
template <typename U>
memory::PFS_allocator<T>::PFS_allocator(PFS_allocator<U> const &rhs) noexcept
    : m_key{rhs.m_key} {}

template <typename T>
template <typename U>
memory::PFS_allocator<T>::PFS_allocator(PFS_allocator<U> &&rhs) noexcept
    : m_key{rhs.m_key} {
  rhs.m_key = 0;
}

template <typename T>
PSI_memory_key memory::PFS_allocator<T>::key() const {
  return this->m_key;
}

template <typename T>
T *memory::PFS_allocator<T>::allocate(std::size_t n) {
  if (n <= std::numeric_limits<std::size_t>::max() / sizeof(T)) {
    if (auto p = static_cast<T *>(my_malloc(this->m_key, (n * sizeof(T)),
                                            MYF(MY_WME | ME_FATALERROR))))
      return p;
  }
  throw std::bad_alloc();
}

template <typename T>
void memory::PFS_allocator<T>::deallocate(T *p, std::size_t) noexcept {
  my_free(p);
}

template <typename T>
template <class U, class... Args>
void memory::PFS_allocator<T>::construct(U *p, Args &&... args) {
  assert(p != nullptr);
  try {
    ::new ((void *)p) U(std::forward<Args>(args)...);
  } catch (...) {
    assert(false);  // Constructor should not throw an exception.
  }
}

template <typename T>
void memory::PFS_allocator<T>::destroy(T *p) {
  assert(p != nullptr);
  try {
    p->~T();
  } catch (...) {
    assert(false);  // Destructor should not throw an exception
  }
}

template <typename T>
size_t memory::PFS_allocator<T>::max_size() const {
  return std::numeric_limits<size_t>::max() / sizeof(T);
}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<std::is_same<B, std::nullptr_t>::value> *>
memory::Unique_ptr<T, A>::Unique_ptr() : m_underlying{nullptr}, m_size{0} {}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<!std::is_same<B, std::nullptr_t>::value> *>
memory::Unique_ptr<T, A>::Unique_ptr(A &alloc)
    : m_underlying{nullptr}, m_allocator{alloc}, m_size{0} {}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<!std::is_same<B, std::nullptr_t>::value &&
                           std::is_array<D>::value> *>
memory::Unique_ptr<T, A>::Unique_ptr(A &alloc, size_t size)
    : m_underlying{nullptr}, m_allocator{alloc}, m_size{size} {
  this->m_underlying = this->m_allocator->allocate(this->m_size);
}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<std::is_same<B, std::nullptr_t>::value &&
                           std::is_array<D>::value> *>
memory::Unique_ptr<T, A>::Unique_ptr(size_t size)
    : m_underlying{new type[size]}, m_size{size} {}

template <typename T, typename A>
template <typename... Args, typename D, typename B,
          std::enable_if_t<!std::is_same<B, std::nullptr_t>::value &&
                           !std::is_array<D>::value> *>
memory::Unique_ptr<T, A>::Unique_ptr(A &alloc, Args &&... args)
    : m_underlying{nullptr}, m_allocator{alloc}, m_size{sizeof(T)} {
  this->m_underlying = this->m_allocator->allocate(this->m_size);
  this->m_allocator->construct(this->m_underlying, std::forward<Args>(args)...);
}

template <typename T, typename A>
template <typename... Args, typename D, typename B,
          std::enable_if_t<std::is_same<B, std::nullptr_t>::value &&
                           !std::is_array<D>::value> *>
memory::Unique_ptr<T, A>::Unique_ptr(Args &&... args)
    : m_underlying{new T{std::forward<Args>(args)...}}, m_size{sizeof(T)} {}

template <typename T, typename A>
memory::Unique_ptr<T, A>::Unique_ptr(memory::Unique_ptr<T, A> &&rhs)
    : m_underlying{rhs.m_underlying},
      m_allocator{rhs.m_allocator},
      m_size{rhs.m_size} {
  rhs.reset();
}

template <typename T, typename A>
memory::Unique_ptr<T, A>::~Unique_ptr() {
  this->destroy();
}

template <typename T, typename A>
typename memory::Unique_ptr<T, A> &memory::Unique_ptr<T, A>::operator=(
    memory::Unique_ptr<T, A> &&rhs) {
  this->m_underlying = rhs.m_underlying;
  this->m_allocator = rhs.m_allocator;
  this->m_size = rhs.m_size;
  rhs.reset();
  return (*this);
}

template <typename T, typename A>
template <typename D, std::enable_if_t<!std::is_array<D>::value> *>
typename memory::Unique_ptr<T, A>::pointer
memory::Unique_ptr<T, A>::operator->() const {
  return this->m_underlying;
}

template <typename T, typename A>
typename memory::Unique_ptr<T, A>::reference
memory::Unique_ptr<T, A>::operator*() const {
  return (*this->m_underlying);
}

template <typename T, typename A>
template <typename D, std::enable_if_t<std::is_array<D>::value> *>
typename memory::Unique_ptr<T, A>::reference
memory::Unique_ptr<T, A>::operator[](size_t index) const {
  return this->m_underlying[index];
}

template <typename T, typename A>
memory::Unique_ptr<T, A>::operator bool() const {
  return this->m_underlying != nullptr;
}

template <typename T, typename A>
template <typename B,
          std::enable_if_t<std::is_same<B, std::nullptr_t>::value> *>
typename memory::Unique_ptr<T, A>::pointer memory::Unique_ptr<T, A>::release() {
  pointer to_return = this->m_underlying;
  this->reset();
  return to_return;
}

template <typename T, typename A>
template <typename B,
          std::enable_if_t<!std::is_same<B, std::nullptr_t>::value> *>
typename memory::Unique_ptr<T, A>::pointer memory::Unique_ptr<T, A>::release() {
  pointer to_return = this->m_allocator->release(this->m_underlying);
  if (to_return != this->m_underlying) {
    to_return = this->clone();
    this->destroy();
  } else {
    this->reset();
  }
  return to_return;
}

template <typename T, typename A>
typename memory::Unique_ptr<T, A>::pointer memory::Unique_ptr<T, A>::get()
    const {
  return this->m_underlying;
}

template <typename T, typename A>
size_t memory::Unique_ptr<T, A>::size() const {
  return this->m_size;
}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<std::is_array<D>::value &&
                           std::is_same<B, std::nullptr_t>::value> *>
typename memory::Unique_ptr<T, A> &memory::Unique_ptr<T, A>::reserve(
    size_t new_size) {
  pointer old_ptr = this->m_underlying;
  this->m_underlying = new type[new_size];
  if (this->m_size != 0) {
    std::copy(old_ptr, old_ptr + std::min(this->m_size, new_size),
              this->m_underlying);
  }
  this->m_size = new_size;
  delete[] old_ptr;
  return (*this);
}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<std::is_array<D>::value &&
                           !std::is_same<B, std::nullptr_t>::value> *>
typename memory::Unique_ptr<T, A> &memory::Unique_ptr<T, A>::reserve(
    size_t new_size) {
  if (this->m_allocator->can_resize()) {
    this->m_underlying =
        this->m_allocator->resize(this->m_underlying, this->m_size, new_size);
  } else {
    pointer old_ptr = this->m_underlying;
    this->m_underlying = this->m_allocator->allocate(new_size);
    if (this->m_size != 0) {
      std::copy(old_ptr, old_ptr + std::min(this->m_size, new_size),
                this->m_underlying);
      this->m_allocator->deallocate(old_ptr, this->m_size);
    }
  }
  this->m_size = new_size;
  return (*this);
}

template <typename T, typename A>
A &memory::Unique_ptr<T, A>::allocator() const {
  return *this->m_allocator;
}

template <typename T, typename A>
void memory::Unique_ptr<T, A>::reset() {
  this->m_underlying = nullptr;
  this->m_size = 0;
}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<std::is_same<B, std::nullptr_t>::value &&
                           std::is_array<D>::value> *>
void memory::Unique_ptr<T, A>::destroy() {
  if (this->m_underlying != nullptr) {
    delete[] this->m_underlying;
    this->reset();
  }
}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<std::is_same<B, std::nullptr_t>::value &&
                           !std::is_array<D>::value> *>
void memory::Unique_ptr<T, A>::destroy() {
  if (this->m_underlying != nullptr) {
    delete this->m_underlying;
    this->reset();
  }
}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<!std::is_same<B, std::nullptr_t>::value &&
                           std::is_array<D>::value> *>
void memory::Unique_ptr<T, A>::destroy() {
  if (this->m_underlying != nullptr) {
    this->m_allocator->deallocate(this->m_underlying, this->m_size);
    this->reset();
  }
}

template <typename T, typename A>
template <typename D, typename B,
          std::enable_if_t<!std::is_same<B, std::nullptr_t>::value &&
                           !std::is_array<D>::value> *>
void memory::Unique_ptr<T, A>::destroy() {
  if (this->m_underlying != nullptr) {
    this->m_allocator->destroy(this->m_underlying);
    this->m_allocator->deallocate(this->m_underlying, this->m_size);
    this->reset();
  }
}

template <typename T, typename A>
template <typename D, std::enable_if_t<std::is_array<D>::value> *>
typename memory::Unique_ptr<T, A>::pointer memory::Unique_ptr<T, A>::clone()
    const {
  pointer to_return = new type[this->m_size];
  std::copy(this->m_underlying, this->m_underlying + this->m_size, to_return);
  return to_return;
}

template <typename T, typename A>
template <typename D, std::enable_if_t<!std::is_array<D>::value> *>
typename memory::Unique_ptr<T, A>::pointer memory::Unique_ptr<T, A>::clone()
    const {
  pointer to_return = new type(*this->m_underlying);
  return to_return;
}

#ifndef IN_DOXYGEN  // Doxygen doesn't understand this construction.
template <typename T, std::enable_if_t<std::is_array<T>::value> *>
memory::Unique_ptr<T, std::nullptr_t> memory::make_unique(size_t size) {
  return memory::Unique_ptr<T, std::nullptr_t>{size};
}

template <typename T, typename A, std::enable_if_t<std::is_array<T>::value> *>
memory::Unique_ptr<T, A> memory::make_unique(A &alloc, size_t size) {
  return std::move(memory::Unique_ptr<T, A>{alloc, size});
}

template <typename T, typename A, typename... Args,
          std::enable_if_t<!std::is_array<T>::value &&
                           memory::is_allocator<A>::value> *>
memory::Unique_ptr<T, A> memory::make_unique(A &alloc, Args &&... args) {
  return std::move(
      memory::Unique_ptr<T, A>{alloc, std::forward<Args>(args)...});
}

template <typename T, typename... Args,
          std::enable_if_t<!std::is_array<T>::value> *>
memory::Unique_ptr<T, std::nullptr_t> memory::make_unique(Args &&... args) {
  return memory::Unique_ptr<T, std::nullptr_t>{std::forward<Args>(args)...};
}
#endif

#endif  // MEMORY_UNIQUE_PTR_INCLUDED
