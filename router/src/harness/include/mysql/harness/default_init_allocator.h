/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_DEFAULT_INIT_ALLOCATOR_H_
#define MYSQL_HARNESS_DEFAULT_INIT_ALLOCATOR_H_

#include <memory>

/**
 * allocator which leaves newly constructed fields "default initialized".
 *
 * in case of std::vector<int> after a resize() the newly added fields would
 * be uninitialized.
 *
 * useful in case of network-buffers it will be read() into right afterwards.
 */
template <class T, class A = std::allocator<T>>
class default_init_allocator : public A {
  using a_t = std::allocator_traits<A>;

 public:
  template <class U>
  struct rebind {
    using other =
        default_init_allocator<U, typename a_t::template rebind_alloc<U>>;
  };

  // inherit the constructors of the allocator
  using A::A;

  template <class U>
  void construct(U *ptr) noexcept(
      std::is_nothrow_default_constructible<U>::value) {
    ::new (static_cast<void *>(ptr)) U;
  }
  template <class U, class... Args>
  void construct(U *ptr, Args &&...args) {
    a_t::construct(static_cast<A &>(*this), ptr, std::forward<Args>(args)...);
  }
};

#endif
