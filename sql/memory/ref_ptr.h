/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef MEMORY_REF_PTR_INCLUDED
#define MEMORY_REF_PTR_INCLUDED

#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>

namespace memory {
/**
  Class that holds the pointer to a variable in a static and
  non-destructible way. The purpose is both to clearly state the ownership
  of the memory being pointed to and to avoid unwanted pointer operations
  (a `delete` on a pointer pointing to a stack memory block, for instance).

  It's a convenience class for clearly stating the ownership of the underlying
  pointer and is used for interface and code clarity.
 */
template <typename T>
class Ref_ptr {
 public:
  /**
    Default class constructor.
   */
  Ref_ptr() = default;
  /**
    Class constructor that receives the reference to be managed.

    @param target The reference to be managed.
   */
  Ref_ptr(T &target);
  /**
    Copy constructor.

    @param rhs The object to copy from.
   */
  Ref_ptr(Ref_ptr<T> const &rhs);
  /**
    Move constructor.

    @param rhs The object to move from.
   */
  Ref_ptr(Ref_ptr<T> &&rhs);
  /**
    Default destructor.
   */
  virtual ~Ref_ptr() = default;

  /**
    Assignment operator to instantiate the reference to be managed.

    @param rhs The reference to be managed.

    @return A reference to `this` object.
   */
  Ref_ptr<T> &operator=(T &rhs);
  /**
    Copy operator.

    @param rhs The object to copy from.

    @return A reference to `this` object.
   */
  Ref_ptr<T> &operator=(Ref_ptr<T> const &rhs);
  /**
    Move operator.

    @param rhs The object to move from.

    @return A reference to `this` object.
   */
  Ref_ptr<T> &operator=(Ref_ptr<T> &&rhs);
  /**
    Negation operator.

    @return `true` if there is no managed reference, `false` otherwise.
   */
  bool operator!() const;
  /**
    Arrow operator to access the underlying object of type `T`.

    @return A pointer to the underlying object of type `T`.
   */
  T *operator->() const;
  /**
    Star operator to access the underlying object of type `T`.

    @return A reference to the underlying object of type `T`.
   */
  T &operator*() const;
  /**
    Resets the managed reference and stops managing any pointer.

    @return A reference to `this` object, for chaining purposes.
   */
  Ref_ptr<T> &reset();
  /**
    Equality to `nullptr` operator.

    @param rhs nullptr value

    @return `true` if the managed reference is not instantiated.
   */
  bool operator==(std::nullptr_t rhs) const;
  /**
    Inequality to `nullptr` operator.

    @param rhs nullptr value

    @return `false` if the managed reference is not instantiated.
   */
  bool operator!=(std::nullptr_t rhs) const;
  /**
    Equality  operator.

    @param rhs The object to compare to.

    @return `true` if the managed reference is the same as one managed by the
            `rhs` object.
   */
  template <typename R>
  bool operator==(memory::Ref_ptr<R> const &rhs) const;
  /**
    Inequality  operator.

    @param rhs The object to compare to.

    @return `true` if the managed reference is not the same as one managed by
            the `rhs` object.
   */
  template <typename R>
  bool operator!=(memory::Ref_ptr<R> const &rhs) const;

 private:
  /** The reference to be managed. */
  T *m_underlying{nullptr};
};
}  // namespace memory

template <typename T>
memory::Ref_ptr<T>::Ref_ptr(T &target) : m_underlying{&target} {}

template <typename T>
memory::Ref_ptr<T>::Ref_ptr(memory::Ref_ptr<T> const &rhs)
    : m_underlying{rhs.m_underlying} {}

template <typename T>
memory::Ref_ptr<T>::Ref_ptr(memory::Ref_ptr<T> &&rhs)
    : m_underlying{rhs.m_underlying} {
  rhs.reset();
}

template <typename T>
memory::Ref_ptr<T> &memory::Ref_ptr<T>::operator=(T &rhs) {
  this->m_underlying = &rhs;
  return (*this);
}

template <typename T>
memory::Ref_ptr<T> &memory::Ref_ptr<T>::operator=(
    memory::Ref_ptr<T> const &rhs) {
  this->m_underlying = rhs.m_underlying;
  return (*this);
}

template <typename T>
memory::Ref_ptr<T> &memory::Ref_ptr<T>::operator=(memory::Ref_ptr<T> &&rhs) {
  this->m_underlying = rhs.m_underlying;
  rhs.reset();
  return (*this);
}

template <typename T>
bool memory::Ref_ptr<T>::operator!() const {
  return this->m_underlying == nullptr;
}

template <typename T>
T &memory::Ref_ptr<T>::operator*() const {
  return *this->m_underlying;
}

template <typename T>
T *memory::Ref_ptr<T>::operator->() const {
  return this->m_underlying;
}

template <typename T>
bool memory::Ref_ptr<T>::operator==(std::nullptr_t) const {
  return this->m_underlying == nullptr;
}

template <typename T>
bool memory::Ref_ptr<T>::operator!=(std::nullptr_t) const {
  return this->m_underlying != nullptr;
}

template <typename T>
template <typename R>
bool memory::Ref_ptr<T>::operator==(memory::Ref_ptr<R> const &rhs) const {
  return this->m_underlying == rhs.m_underlying;
}

template <typename T>
template <typename R>
bool memory::Ref_ptr<T>::operator!=(memory::Ref_ptr<R> const &rhs) const {
  return this->m_underlying != rhs.m_underlying;
}

template <typename T>
memory::Ref_ptr<T> &memory::Ref_ptr<T>::reset() {
  this->m_underlying = nullptr;
  return (*this);
}

#endif  // MEMORY_REF_PTR_INCLUDED
