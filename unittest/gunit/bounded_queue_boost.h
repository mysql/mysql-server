/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BOUNDED_HEAP_BOOST_INCLUDED
#define BOUNDED_HEAP_BOOST_INCLUDED

#include "my_base.h"

/**
  @file
  A test of how to implement Bounded_queue using boost heap.
  We use the 'pimpl' idiom, to illustrate how to avoid dependencies from
  our own header files to boost header files.
 */

template <typename Element_type, typename Key_type, typename Key_generator,
          typename Key_compare>
class Bounded_queue_impl;

template <typename Element_type, typename Key_type, typename Key_generator,
          typename Key_compare = std::less<Key_type>>
class Bounded_queue_boost {
 public:
  explicit Bounded_queue_boost();

  ~Bounded_queue_boost();

  int init(ha_rows max_elements, Key_generator *sort_param,
           Key_type *sort_keys);

  void push(Element_type element);

  size_t num_elements() const { return m_num_elements; }

 private:
  Key_compare m_cmp;
  Key_type *m_sort_keys;
  size_t m_compare_length;
  Key_generator *m_sort_param;
  size_t m_max_elements;
  size_t m_num_elements;
  Bounded_queue_impl<Element_type, Key_type, Key_generator, Key_compare> *pimpl;
};

#endif  // BOUNDED_HEAP_BOOST_INCLUDED
