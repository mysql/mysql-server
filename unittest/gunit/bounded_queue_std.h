/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BOUNDED_HEAP_STD_INCLUDED
#define BOUNDED_HEAP_STD_INCLUDED

#include <queue>
#include <vector>

#include "my_base.h"

template <typename Element_type, typename Key_type, typename Key_generator,
          typename Key_compare = std::less<Key_type>>
class Bounded_queue_std {
 public:
  Bounded_queue_std()
      : m_cmp(),
        m_sort_keys(NULL),
        m_compare_length(0),
        m_sort_param(NULL),
        m_max_elements(0),
        m_num_elements(0) {}

  int init(ha_rows max_elements, Key_generator *sort_param,
           Key_type *sort_keys) {
    m_sort_keys = sort_keys;
    m_compare_length = sort_param->max_compare_length();
    m_sort_param = sort_param;
    m_max_elements = max_elements + 1;
    return 0;
  }

  void push(Element_type element) {
    if (m_num_elements == m_max_elements) {
      m_queue.pop();
      --m_num_elements;
    }
    {
      m_sort_param->make_sortkey(m_sort_keys[m_queue.size()], sizeof(Key_type),
                                 element);
      m_queue.push(m_sort_keys[m_queue.size()]);
      ++m_num_elements;
    }
  }

  size_t num_elements() const { return m_num_elements; }

  typedef std::priority_queue<Key_type, std::vector<Key_type>, Key_compare>
      Queue_type;

 private:
  Queue_type m_queue;
  Key_compare m_cmp;
  Key_type *m_sort_keys;
  size_t m_compare_length;
  Key_generator *m_sort_param;
  size_t m_max_elements;
  size_t m_num_elements;
};

#endif  // BOUNDED_HEAP_STD_INCLUDED
