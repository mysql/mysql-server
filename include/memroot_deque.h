/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MEM_ROOT_DEQUE_H
#define MEM_ROOT_DEQUE_H

#include <deque>
#include <initializer_list>
#include <utility>

#include "sql/memroot_allocator.h"

/// A utility for having an std::deque which stores its elements on a MEM_ROOT.
template <class T>
class memroot_deque : public std::deque<T, Memroot_allocator<T>> {
 private:
  using super = std::deque<T, Memroot_allocator<T>>;

 public:
  explicit memroot_deque(MEM_ROOT *mem_root)
      : super(Memroot_allocator<T>(mem_root)) {}

  memroot_deque(typename super::size_type count, const T &value,
                MEM_ROOT *mem_root)
      : super(count, value, Memroot_allocator<T>(mem_root)) {}

  memroot_deque(typename super::size_type count, MEM_ROOT *mem_root)
      : super(count, Memroot_allocator<T>(mem_root)) {}

  template <class InputIt>
  memroot_deque(InputIt first, InputIt last, MEM_ROOT *mem_root)
      : super(first, last, Memroot_allocator<T>(mem_root)) {}

  memroot_deque(const memroot_deque &other) : super(other) {}
  memroot_deque(const memroot_deque &other, MEM_ROOT *mem_root)
      : super(other, Memroot_allocator<T>(mem_root)) {}

  memroot_deque(memroot_deque &&other) : super(std::move(other)) {}
  memroot_deque(memroot_deque &&other, MEM_ROOT *mem_root)
      : super(std::move(other), Memroot_allocator<T>(mem_root)) {}

  memroot_deque(std::initializer_list<T> init, MEM_ROOT *mem_root)
      : super(std::move(init), Memroot_allocator<T>(mem_root)) {}
};

#endif  // MEM_ROOT_DEQUE_H
